/* neon's configuration on POSIX, force-included before every unit.

   WinSCP's src/config.h configures neon for Win32 only; elsewhere it is
   empty and neon expects autoconf's config.h. This is that header, from
   neon's own configure (run on glibc x86-64, LP64, against the bundled
   OpenSSL and expat: --with-ssl=openssl --with-expat --without-gssapi
   --without-libproxy --disable-nls --without-zlib), reduced to what the
   units test.

   Facts about the platform are configure's. Choices about features are
   WinSCP's, as its Win32 config.h makes them, so that both targets behave
   the same: no HAVE_OPENSSL11 (digest auth offers MD5/SHA-256 only), no
   HAVE_SSL_CIPHER_STANDARD_NAME (OpenSSL's cipher names), no
   HAVE_ASN1_TIME_TO_TM (unless OpenSSL 4.0, below), and
   HAVE_CRYPTO_SET_IDPTR_CALLBACK. HAVE_SSPI has no POSIX counterpart;
   GSSAPI is not enabled. Makefile.bcb's defines
   (NE_HAVE_SSL, HAVE_OPENSSL, HAVE_EXPAT, NE_HAVE_DAV, NE_LFS, WINSCP, ...)
   come on the command line, as on Windows. */
#ifndef LINSCP_NEON_CONFIG_POSIX_H
#define LINSCP_NEON_CONFIG_POSIX_H

#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

#define NEON_VERSION "0.37.1"
#define NE_VERSION_MAJOR (0)
#define NE_VERSION_MINOR (37)
#define NE_VERSION_PATCH (1)
#define PACKAGE_NAME "neon"
#define NEON_IS_LIBRARY 1
#define NE_DEBUGGING 1

#define HAVE_ARPA_INET_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_NET_IF_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_POLL_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_UNISTD_H 1

#define HAVE_DECL_STPCPY 1
#define HAVE_DECL_STRERROR_R 1
#define HAVE_EXPLICIT_BZERO 1
#define HAVE_FCNTL 1
#define HAVE_GAI_STRERROR 1
#define HAVE_GETHOSTNAME 1
#define HAVE_GETNAMEINFO 1
#define HAVE_GETRANDOM 1
#define HAVE_GETSOCKOPT 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GMTIME_R 1
#define HAVE_IF_INDEXTONAME 1
#define HAVE_IF_NAMETOINDEX 1
#define HAVE_INET_NTOP 1
#define HAVE_INET_PTON 1
#define HAVE_ISATTY 1
#define HAVE_PIPE 1
#define HAVE_POLL 1
#define HAVE_SENDMSG 1
#define HAVE_SETLOCALE 1
#define HAVE_SETSOCKOPT 1
#define HAVE_SETVBUF 1
#define HAVE_SHUTDOWN 1
#define HAVE_SIGNAL 1
#define HAVE_SNPRINTF 1
#define HAVE_SOCKLEN_T 1
#define HAVE_STPCPY 1
#define HAVE_STRCASECMP 1
#define HAVE_STRERROR_R 1
#define STRERROR_R_CHAR_P 1
#define HAVE_STRTOLL 1
#define HAVE_STRUCT_TM_TM_GMTOFF 1
#define HAVE_TIMEGM 1
#define HAVE_TIMEZONE 1
#define HAVE_USLEEP 1
#define HAVE_VSNPRINTF 1
#define STDC_HEADERS 1

#define NE_HAVE_IPV6 1
#define NE_HAVE_TS_SSL 1
#define NE_USE_POLL 1
#define USE_GAI_ADDRCONFIG 1
#define USE_GETADDRINFO 1

/* WINSCP, as in its Win32 config.h. */
#define HAVE_CRYPTO_SET_IDPTR_CALLBACK

/* Except with a system OpenSSL 4.0 or later (LINSCP_SYSTEM_OPENSSL): its
   ASN1_TIME is opaque, so the fallback that reads the structure no longer
   compiles; ASN1_TIME_to_tm, which configure would find, is used instead. */
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR >= 4
#define HAVE_ASN1_TIME_TO_TM 1
#endif

/* LP64: configure's sizes, checked below. NE_LFS (from the make file)
   makes ne_off_t glibc's off64_t, the same type as off_t here. */
#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_SSIZE_T 8
#define SIZEOF_TIME_T 8
#define SIZEOF_XML_SIZE 8
#define NE_FMT_OFF_T "ld"
#define NE_FMT_OFF64_T "ld"
#define NE_FMT_NE_OFF_T NE_FMT_OFF_T
#define NE_FMT_SIZE_T "lu"
#define NE_FMT_SSIZE_T "ld"
#define NE_FMT_TIME_T "ld"
#define NE_FMT_XML_SIZE "ld"

#include <sys/types.h>
#include <time.h>
_Static_assert(sizeof(int) == SIZEOF_INT, "neon config: int");
_Static_assert(sizeof(long) == SIZEOF_LONG, "neon config: long");
_Static_assert(sizeof(off_t) == SIZEOF_OFF_T, "neon config: off_t");
_Static_assert(sizeof(size_t) == SIZEOF_SIZE_T, "neon config: size_t");
_Static_assert(sizeof(time_t) == SIZEOF_TIME_T, "neon config: time_t");

#endif
