// wincrypt.h's certificate-chain API over OpenSSL and the system trust store.
#include "wincrypt.h"

#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

namespace {

struct TCertificate
{
  CERT_CONTEXT Context;     // first, so a PCCERT_CONTEXT is a TCertificate *
  X509 * Certificate;
};

}  // namespace

struct _CERT_CHAIN_CONTEXT
{
  int VerifyError;
  int ErrorDepth;
};

PCCERT_CONTEXT CertCreateCertificateContext(DWORD, const BYTE * Encoded, DWORD Size)
{
  const unsigned char * P = Encoded;
  X509 * Certificate = d2i_X509(nullptr, &P, static_cast<long>(Size));
  if (Certificate == nullptr)
  {
    ERR_clear_error();
    SetLastError(ERROR_INVALID_PARAMETER);
    return nullptr;
  }
  auto * C = new TCertificate();
  C->Context.dwCertEncodingType = X509_ASN_ENCODING;
  C->Context.pbCertEncoded = static_cast<BYTE *>(std::malloc(Size));
  std::memcpy(C->Context.pbCertEncoded, Encoded, Size);
  C->Context.cbCertEncoded = Size;
  C->Certificate = Certificate;
  return &C->Context;
}

BOOL CertFreeCertificateContext(PCCERT_CONTEXT Context)
{
  if (Context != nullptr)
  {
    auto * C = reinterpret_cast<TCertificate *>(const_cast<CERT_CONTEXT *>(Context));
    X509_free(C->Certificate);
    std::free(C->Context.pbCertEncoded);
    delete C;
  }
  return TRUE;
}

BOOL CertCreateCertificateChainEngine(CERT_CHAIN_ENGINE_CONFIG * Config, HCERTCHAINENGINE * Engine)
{
  if ((Engine == nullptr) || ((Config != nullptr) && (Config->cAdditionalStore != 0)))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  X509_STORE * Store = X509_STORE_new();
  if (Store == nullptr)
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }
  // OpenSSL's own defaults (which honour SSL_CERT_FILE and SSL_CERT_DIR),
  // then the bundles distributions install, wherever this OpenSSL was
  // configured to look.
  X509_STORE_set_default_paths(Store);
  static const char * const Bundles[] = {
    "/etc/ssl/certs/ca-certificates.crt",   // Debian, Ubuntu, Arch
    "/etc/pki/tls/certs/ca-bundle.crt",     // Fedora, RHEL
    "/etc/ssl/ca-bundle.pem",               // openSUSE
    "/etc/ssl/cert.pem",                    // Alpine, macOS
  };
  for (const char * Bundle : Bundles)
  {
    struct stat St{};
    if (stat(Bundle, &St) == 0)
    {
      X509_STORE_load_file(Store, Bundle);
    }
  }
  struct stat St{};
  if (stat("/etc/ssl/certs", &St) == 0)
  {
    X509_STORE_load_path(Store, "/etc/ssl/certs");
  }
  ERR_clear_error();
  *Engine = Store;
  return TRUE;
}

void CertFreeCertificateChainEngine(HCERTCHAINENGINE Engine)
{
  X509_STORE_free(static_cast<X509_STORE *>(Engine));
}

BOOL CertGetCertificateChain(HCERTCHAINENGINE Engine, PCCERT_CONTEXT Context, FILETIME *,
  HCERTSTORE, CERT_CHAIN_PARA *, DWORD, LPVOID, PCCERT_CHAIN_CONTEXT * Chain)
{
  if ((Engine == nullptr) || (Context == nullptr) || (Chain == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  auto * C = reinterpret_cast<const TCertificate *>(Context);
  X509_STORE_CTX * Ctx = X509_STORE_CTX_new();
  if ((Ctx == nullptr) ||
      (X509_STORE_CTX_init(Ctx, static_cast<X509_STORE *>(Engine), C->Certificate, nullptr) != 1))
  {
    X509_STORE_CTX_free(Ctx);
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }
  X509_STORE_CTX_set_purpose(Ctx, X509_PURPOSE_SSL_SERVER);
  auto * Result = new _CERT_CHAIN_CONTEXT();
  Result->VerifyError = (X509_verify_cert(Ctx) == 1) ? X509_V_OK : X509_STORE_CTX_get_error(Ctx);
  Result->ErrorDepth = X509_STORE_CTX_get_error_depth(Ctx);
  X509_STORE_CTX_free(Ctx);
  ERR_clear_error();
  *Chain = Result;
  return TRUE;
}

void CertFreeCertificateChain(PCCERT_CHAIN_CONTEXT Chain)
{
  delete Chain;
}

BOOL CertVerifyCertificateChainPolicy(LPCSTR Policy, PCCERT_CHAIN_CONTEXT Chain,
  CERT_CHAIN_POLICY_PARA *, CERT_CHAIN_POLICY_STATUS * Status)
{
  if ((Policy != CERT_CHAIN_POLICY_SSL) || (Chain == nullptr) || (Status == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  HRESULT Error;
  switch (Chain->VerifyError)
  {
    case X509_V_OK:
      Error = S_OK;
      break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_CERT_NOT_YET_VALID:
      Error = CERT_E_EXPIRED;
      break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
    case X509_V_ERR_CERT_UNTRUSTED:
      Error = CERT_E_UNTRUSTEDROOT;
      break;
    case X509_V_ERR_CERT_REVOKED:
      Error = CRYPT_E_REVOKED;
      break;
    case X509_V_ERR_INVALID_PURPOSE:
      Error = CERT_E_WRONG_USAGE;
      break;
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
      Error = TRUST_E_CERT_SIGNATURE;
      break;
    default:
      // Notably "unable to get local issuer certificate": no chain to a
      // trusted root could be built.
      Error = CERT_E_CHAINING;
      break;
  }
  Status->dwError = static_cast<DWORD>(Error);
  Status->lChainIndex = (Error == S_OK) ? -1 : 0;
  Status->lElementIndex = (Error == S_OK) ? -1 : Chain->ErrorDepth;
  return TRUE;
}
