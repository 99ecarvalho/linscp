# Bundled libraries

What linscp compiles from source instead of linking the system's, what the
bundled copies change relative to their upstream releases, and which can be
replaced by a distribution's package. Reviewed 2026-10-20; the diffs were
taken against the release tarballs or repositories named below, ignoring line
endings.

| Component | Bundled copy | Upstream base | System library? |
|---|---|---|---|
| OpenSSL | `3rd-party/openssl` (built), `3rd-party/winscp/libs/openssl` (not built) | 3.5.9 | **Yes**: `LINSCP_SYSTEM_OPENSSL=ON`, 3.0 or later, 4.0 included |
| expat | `3rd-party/winscp/libs/expat` | 2.9.0 | **Yes**: `LINSCP_SYSTEM_EXPAT=ON` |
| neon | `3rd-party/winscp/libs/neon` | 0.37.1 | No: the core uses API only the modified copy has |
| libs3 | `3rd-party/winscp/libs/libs3` | libs3 4.1 (git master) | No: rewritten to run over neon, with extra API |
| PuTTY | `3rd-party/winscp/source/putty` | 0.85 | No: there is no PuTTY library; see [putty-deltas.md](putty-deltas.md) |
| FileZilla engine | `3rd-party/winscp/source/filezilla` | an old FileZilla 2 engine | No: there is no upstream library |
| MFC subset | `3rd-party/winscp/libs/mfc` | | Not built; `port/mfc-compat` replaces it |

Already from the system on Linux: Qt 6, zlib (through Qt), the C++ runtime,
and GSSAPI/Kerberos (`libgssapi_krb5.so.2`, loaded at run time by
`port/putty/linux/gss.c`).

## OpenSSL

The upstream tree carries a trimmed copy of 3.5.9 configured for Windows. The
build uses the matching release instead (`3rd-party/openssl`), and
`port/tools/openssl_tree.py` checks that every file the trimmed copy changes
is listed in `port/openssl/winscp-changes.txt`. There are five, all
C++Builder compiler fixes, and the build skips all five:

- `crypto/threads_win.c`, `crypto/thread/arch/thread_win.c`: atomics and
  memory barriers declared for bcc64x;
- `crypto/bio/bio_addr.c`: a Win64 Debug stand-in for `gai_strerrorA`;
- `crypto/bio/bio_sock2.c`: a cast on `INVALID_SOCKET`;
- `crypto/x509/x509_def.c`: `const` on a local.

None of them is about behaviour, and none applies to Linux, so the core does
not depend on a modified OpenSSL. The core, neon, libs3 and the FileZilla TLS
layer are compiled with `OPENSSL_API_COMPAT=11000 OPENSSL_NO_DEPRECATED`.
They use only public API: no engines, no internal headers, no struct access
beyond what is listed below for 4.0, and no `OPENSSL_NO_*` assumptions beyond
the disabled algorithms in `cmake/openssl.cmake` (camellia, RC5, MDC2,
Whirlpool, SEED), which none of them use.

With `LINSCP_SYSTEM_OPENSSL=ON` (Linux only), `find_package(OpenSSL 3.0)`
provides the library and its headers. The bundled tree is neither assembled
nor built, and `--exclude-libs` is dropped because there is no static copy to
hide. Tested:

- **Debian trixie 3.5.7**: full ctest passes, including the golden corpus
  and the `real-servers` tests.
- **4.0.3**, built from the release tarball (sha256 `325b5c80...16d9`,
  checked against openssl.org and GitHub): full ctest passes, including
  golden and `real-servers` (including explicit and implicit FTPS and
  WebDAV over TLS).

4.0 makes `ASN1_STRING` opaque and returns `const` from several X509
getters. Three changes were needed, and none of them changes behaviour with
3.x:

- `port/patches/filezilla-openssl4-const-opaque.patch`: `AsnTimeToValidTime`
  reads the time through `ASN1_STRING_length`, `ASN1_STRING_get0_data` and
  `ASN1_STRING_type`, and the name, entry and extension pointers are `const`;
- `port/neon/config-posix.h`: `HAVE_ASN1_TIME_TO_TM` is defined with OpenSSL
  4.0 or later, so neon uses `ASN1_TIME_to_tm` where configure would find it.
  The fallback that reads the structure no longer compiles. With 3.x the
  choice is unchanged, the same as on Windows;
- `port/tests/test_win32_posix.cpp`: the test certificate's subject name is
  built separately and then set.

Notes for packagers:

- A system OpenSSL reads the system's `openssl.cnf`; the bundled build has the
  default `OPENSSLDIR` (`/usr/local/ssl`) and normally finds none. Under a
  distribution crypto policy (Fedora's, for example), old FTPS/WebDAV servers
  that need TLS 1.0/1.1 or SHA-1 signatures may be refused where the bundled
  build would connect. This is the distribution's policy working as intended.
- Built against the system OpenSSL, linscp follows that policy for TLS
  (FTPS, WebDAV, S3): no code sets a cipher list, cipher suites, groups or
  signature algorithms, and the configuration is loaded as OpenSSL's
  initialisation does by default. A session's TLS version range (site
  settings, Advanced, TLS/SSL) is applied only where it differs from the
  default (minimum TLS 1.2, maximum TLS 1.3); a bound left at the default is
  the policy's (`TLS_SYSTEM_POLICY`,
  `port/patches/core-tls-system-policy-bounds.patch`). A minimum of TLS 1.0
  or 1.1, chosen per session, also lowers that connection's security level
  to 0, which OpenSSL 3 needs for those versions, as with the bundled build.
  With the bundled OpenSSL and on Windows, both bounds are set as before.
  SSH (SFTP, SCP) is PuTTY's own code and does not use OpenSSL for its
  protocol, so the policy does not reach it; its ciphers, key exchange and
  host-key algorithms are the session's own preference lists.
- The version shown in About and `linscp-cli /info` is the one the headers
  had at build time (`OPENSSL_VERSION_TEXT`), not the one loaded at run time.
- On a system whose other libraries still use OpenSSL 3 (Qt's TLS plugin,
  libldap), building against 4.0 loads both `libssl.so.3` and `libssl.so.4`.
  Their symbols are versioned (`OPENSSL_3.0.0`, `OPENSSL_4.0.0`), so neither
  resolves to the other, and the test suite passes that way. A distribution
  that moved to 4.0 has only the one.

## expat

The bundled copy is expat 2.9.0 (`lib/expat.h`) plus:

- `bcb5/`: C++Builder make files;
- `expat_config.h`: arc4random and getrandom turned off and getentropy kept;
- `lib/random_rand_s.c`, `lib/loadlibrary.c`: a Windows entropy source through
  `RtlGenRandom`, because C++Builder has no `rand_s`. Not built on Linux;
- `lib/xmltok_impl.c`: a bounds check in `updatePosition` from Chromium's
  copy, which only hardens line and column tracking.

The core (`WebDAVFileSystem.cpp`), neon, libs3 and the shim's XML reader use
only expat's public `char` API. With `LINSCP_SYSTEM_EXPAT=ON` (Linux only),
`find_package(EXPAT 2.4)` provides it, and the core's and FileZilla's direct
include of the bundled `lib/` directory is dropped. Upstream's neon calls
`XML_GetCurrentLineNumber64`, which is new in 2.9.0;
`port/patches/neon-xml-system-expat-line-number.patch` maps it to
`XML_GetCurrentLineNumber` (the same value) for older versions. Tested with
Debian trixie's 2.8.3: full ctest passes. Debian's security update to 2.9.0
needs no patch.

## neon

The bundled copy is neon 0.37.1 with changes in 30 files (about 490 lines
added, 35 removed, 121 `WINSCP` markers), plus a Win32 `config.h`. The core
depends on them directly:

- new API used by `NeonIntf.cpp`, `WebDAVFileSystem.cpp`, `S3FileSystem.cpp`
  and libs3: `ne_ssl_get_version`, `ne_ssl_get_cipher`, `ne_init_ssl_session`,
  `ne_ssl_set_certificates_storage`, `ne_set_realhost`,
  `ne_ssl_clicert_create`, `ne_lock_register_discovery`/`_free`,
  `ne_set_request_body_provider_pre`, `ne_get_request_body_buffer`,
  `ne_remove_server_auth`, `ne_capability_name`, `ne_sock_sspi_init`;
- ABI changes: two session flags (`NE_SESSFLAG_LIBERAL_ESCAPING`,
  `SE_SESSFLAG_SNDBUF`) added before the `NE_SESSFLAG_LAST` sentinel;
  `NE_SSL_DIGESTLEN` grows for SHA-256 fingerprints; `NE_BUFSIZ` is 64 KiB
  instead of 8 KiB; a new error code `NE_SOCKET`; and new fields in private
  structures (`realhost`);
- behaviour: identity checks against a "real host" (for S3 virtual-host
  endpoints), liberal URL escaping, and logging levels.

Debian trixie ships `libneon27-dev` (and `-gnutls-dev`) 0.34.2: older than
the base, and without any of the above. A system neon would need these
changes upstream first, so it stays bundled. It is compiled from
`3rd-party/winscp/libs/neon` with the patch series (`port/neon/`).

## libs3

The bundled copy is libs3 from its git master (the copy reports 4.1;
compared with `98f667b`, 2024-04-24): 37 files changed, about 660 lines added
and 440 removed. Its HTTP layer, `src/request.c`, is rewritten to run over
neon instead of libcurl. Its API adds session, TLS-certificate and
response-data callbacks, requester-pays, `maxkeys` and per-request contexts
for multipart operations, new status codes, and larger limits. It is compiled
as C++, because the additions rely on it and the core declares its functions
with C++ linkage.

Debian trixie's `libs3-dev` is 2.0-5, built on libcurl and libxml2. Neither
it nor a current stock libs3 offers the API the core calls, so it stays
bundled (`port/libs3/`).
