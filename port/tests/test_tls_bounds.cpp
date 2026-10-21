// The TLS version bounds core's SetupSsl puts on a connection (FTPS, WebDAV,
// S3, core's HTTP requests).
//
// Built against the system's OpenSSL (TLS_SYSTEM_POLICY,
// port/patches/core-tls-system-policy-bounds.patch), a bound the session
// leaves at its default is the system crypto policy's: whatever the SSL_CTX
// got from openssl.cnf, stood in for here by a context whose range is set
// first. A bound the session changes is applied either way. With the bundled
// OpenSSL (and on Windows) both defaults are set, as upstream does.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>

#include <openssl/ssl.h>

#include <SessionData.h>
#include <NeonIntf.h>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

#ifdef TLS_SYSTEM_POLICY
const bool SystemPolicy = true;
#else
const bool SystemPolicy = false;
#endif

// The range a crypto policy might give every new context: TLS 1.3 only,
// stricter than core's default minimum.
const int PolicyMin = TLS1_3_VERSION;
const int PolicyMax = 0;

struct TBounds
{
  long Min;
  long Max;
  int SecurityLevel;
};

TBounds Setup(SSL_CTX * Ctx, TTlsVersion MinTls, TTlsVersion MaxTls)
{
  SSL * Ssl = SSL_new(Ctx);
  SetupSsl(Ssl, MinTls, MaxTls);
  TBounds Result = { SSL_get_min_proto_version(Ssl), SSL_get_max_proto_version(Ssl), SSL_get_security_level(Ssl) };
  std::printf("min %d, max %d -> min 0x%lx, max 0x%lx, security level %d\n",
              static_cast<int>(MinTls), static_cast<int>(MaxTls), Result.Min, Result.Max, Result.SecurityLevel);
  SSL_free(Ssl);
  return Result;
}

}  // namespace

int main()
{
  std::printf("system policy: %s\n", SystemPolicy ? "yes" : "no");
  SSL_CTX * Ctx = SSL_CTX_new(TLS_client_method());
  CHECK(Ctx != NULL);
  if (Ctx == NULL)
  {
    return 1;
  }
  CHECK(SSL_CTX_set_min_proto_version(Ctx, PolicyMin) == 1);
  CHECK(SSL_CTX_set_max_proto_version(Ctx, PolicyMax) == 1);
  const int PolicyLevel = SSL_CTX_get_security_level(Ctx);

  // Both bounds at their defaults: the policy's range, or TLS 1.2 to 1.3.
  TBounds B = Setup(Ctx, tlsDefaultMin, tlsMax);
  CHECK(B.Min == (SystemPolicy ? PolicyMin : TLS1_2_VERSION));
  CHECK(B.Max == (SystemPolicy ? PolicyMax : TLS1_3_VERSION));
  CHECK(B.SecurityLevel == PolicyLevel);

  // A minimum the session raises applies; the default maximum stays the policy's.
  B = Setup(Ctx, tls13, tlsMax);
  CHECK(B.Min == TLS1_3_VERSION);
  CHECK(B.Max == (SystemPolicy ? PolicyMax : TLS1_3_VERSION));

  // A maximum the session lowers applies; the default minimum stays the policy's.
  B = Setup(Ctx, tlsDefaultMin, tls12);
  CHECK(B.Min == (SystemPolicy ? PolicyMin : TLS1_2_VERSION));
  CHECK(B.Max == TLS1_2_VERSION);

  // An old version the user asks for explicitly is allowed, at security
  // level 0, which OpenSSL 3 needs for TLS 1.0 and 1.1: a deliberate,
  // per-session choice, as before.
  B = Setup(Ctx, tls10, tls12);
  CHECK(B.Min == TLS1_VERSION);
  CHECK(B.Max == TLS1_2_VERSION);
  CHECK(B.SecurityLevel == 0);

  SSL_CTX_free(Ctx);
  if (Failures == 0)
  {
    std::printf("ok\n");
  }
  return Failures == 0 ? 0 : 1;
}
