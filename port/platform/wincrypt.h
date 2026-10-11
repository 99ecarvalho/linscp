// Stand-in for Win32's <wincrypt.h>: the certificate-chain API core's
// WindowsValidateCertificate uses (Security.cpp), implemented over OpenSSL
// with the system's trust store.
//
// That function is core's last resort for a TLS server certificate (FTPS,
// WebDAV) its own checks did not accept: "does the operating system trust
// this?". On POSIX the operating system's trust store is the CA bundle and
// hashed directory OpenSSL is pointed at, so the chain is built and verified
// against those, for TLS server use. What Windows adds -- fetching missing
// intermediates and checking revocation online -- is not done; see the
// divergence table.
#ifndef LINSCP_WINCRYPT_H
#define LINSCP_WINCRYPT_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"

#define X509_ASN_ENCODING                                0x00000001
#define PKCS_7_ASN_ENCODING                              0x00010000
#define CERT_CHAIN_CACHE_END_CERT                        0x00000001
#define CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT   0x40000000
#define CERT_CHAIN_POLICY_SSL                            ((LPCSTR)4)

// The policy errors, as Windows reports them.
#define CERT_E_EXPIRED          ((HRESULT)0x800B0101)
#define CERT_E_UNTRUSTEDROOT    ((HRESULT)0x800B0109)
#define CERT_E_CHAINING         ((HRESULT)0x800B010A)
#define CERT_E_WRONG_USAGE      ((HRESULT)0x800B0110)
#define CRYPT_E_REVOKED         ((HRESULT)0x80092010)
#define TRUST_E_CERT_SIGNATURE  ((HRESULT)0x80096004)

#define FIELD_OFFSET(type, field) ((LONG)offsetof(type, field))
#define RTL_SIZEOF_THROUGH_FIELD(type, field) \
  (FIELD_OFFSET(type, field) + sizeof(((type *)0)->field))

typedef void * HCERTSTORE;
typedef void * HCERTCHAINENGINE;

typedef struct _CERT_CONTEXT
{
  DWORD dwCertEncodingType;
  BYTE * pbCertEncoded;
  DWORD cbCertEncoded;
  void * pCertInfo;
  HCERTSTORE hCertStore;
} CERT_CONTEXT;
typedef const CERT_CONTEXT * PCCERT_CONTEXT;

typedef struct _CERT_USAGE_MATCH
{
  DWORD dwType;
  struct { DWORD cUsageIdentifier; LPSTR * rgpszUsageIdentifier; } Usage;
} CERT_USAGE_MATCH;

typedef struct _CERT_CHAIN_PARA
{
  DWORD cbSize;
  CERT_USAGE_MATCH RequestedUsage;
} CERT_CHAIN_PARA;

typedef struct _CERT_CHAIN_ENGINE_CONFIG
{
  DWORD cbSize;
  HCERTSTORE hRestrictedRoot;
  HCERTSTORE hRestrictedTrust;
  HCERTSTORE hRestrictedOther;
  DWORD cAdditionalStore;
  HCERTSTORE * rghAdditionalStore;
  DWORD dwFlags;
  DWORD dwUrlRetrievalTimeout;
  DWORD MaximumCachedCertificates;
  DWORD CycleDetectionModulus;
  HCERTSTORE hExclusiveRoot;
  HCERTSTORE hExclusiveTrustedPeople;
  DWORD dwExclusiveFlags;
} CERT_CHAIN_ENGINE_CONFIG;

typedef struct _CERT_CHAIN_CONTEXT CERT_CHAIN_CONTEXT;
typedef const CERT_CHAIN_CONTEXT * PCCERT_CHAIN_CONTEXT;

typedef struct _CERT_CHAIN_POLICY_PARA
{
  DWORD cbSize;
  DWORD dwFlags;
  void * pvExtraPolicyPara;
} CERT_CHAIN_POLICY_PARA;

typedef struct _CERT_CHAIN_POLICY_STATUS
{
  DWORD cbSize;
  DWORD dwError;
  LONG lChainIndex;
  LONG lElementIndex;
  void * pvExtraPolicyStatus;
} CERT_CHAIN_POLICY_STATUS;

#ifdef __cplusplus
extern "C" {
#endif
// A DER certificate; null if it does not parse.
PCCERT_CONTEXT CertCreateCertificateContext(DWORD EncodingType, const BYTE * Encoded, DWORD Size);
BOOL CertFreeCertificateContext(PCCERT_CONTEXT Context);
// The engine is the system trust store. Config's stores must be empty, as
// core leaves them.
BOOL CertCreateCertificateChainEngine(CERT_CHAIN_ENGINE_CONFIG * Config, HCERTCHAINENGINE * Engine);
void CertFreeCertificateChainEngine(HCERTCHAINENGINE Engine);
// Succeeds whenever the certificate could be examined, trusted or not, as
// on Windows; the verdict is the policy's.
BOOL CertGetCertificateChain(HCERTCHAINENGINE Engine, PCCERT_CONTEXT Context, FILETIME * Time,
  HCERTSTORE AdditionalStore, CERT_CHAIN_PARA * Para, DWORD Flags, LPVOID Reserved,
  PCCERT_CHAIN_CONTEXT * Chain);
void CertFreeCertificateChain(PCCERT_CHAIN_CONTEXT Chain);
// CERT_CHAIN_POLICY_SSL: dwError 0 when the chain reaches a trusted root and
// every certificate is valid now and fit for a TLS server, else the CERT_E_*
// code Windows would report, with lElementIndex the failing certificate.
BOOL CertVerifyCertificateChainPolicy(LPCSTR Policy, PCCERT_CHAIN_CONTEXT Chain,
  CERT_CHAIN_POLICY_PARA * Para, CERT_CHAIN_POLICY_STATUS * Status);
#ifdef __cplusplus
}
#endif
#endif
