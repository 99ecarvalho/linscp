// The C runtime's wide-character functions, for code built with
// -fshort-wchar on POSIX. Included by win32_on_posix.h, and forced into every
// PuTTY unit on POSIX: some of PuTTY's platform-independent utilities
// (dupwcs.c, burnwcs.c) never include a platform header, and reached glibc's
// 4-byte functions with 2-byte strings.
#ifndef LINSCP_WIN32_ON_POSIX_WIDE_H
#define LINSCP_WIN32_ON_POSIX_WIDE_H

#if defined(_WIN32)
  #error "POSIX only: Windows' runtime has a 2-byte wchar_t of its own."
#endif

// --- wide character functions ----------------------------------------------
//
// Core calls these directly: towupper 9, wcslen 10, wcschr 10, towlower 4,
// wcscmp 1. On Windows they come from the CRT with a 2-byte wchar_t. glibc's
// take a 4-byte wint_t, so under -fshort-wchar calling them is the ABI mismatch
// ADR 0001 describes: it links cleanly and then misbehaves.
//
// They are given distinct names and the CRT names redirected with macros,
// rather than redeclared. A redeclaration is not possible -- <string> pulls in
// <cwchar>, whose wcschr differs from ours only in return type -- and a macro
// also guarantees core's and PuTTY's calls reach these rather than glibc's.
//
// Case folding is ASCII only, matching what the Windows CRT does in the default
// locale -- verified by running the same test against the real CRT under Wine.
// Being wider on one platform would make the two disagree, which is worse than
// being narrow. Real Unicode folding is a separate job; see the divergence
// table in doc/qt/TRACKER.md.
//
// The runtime's own headers come first, so that libstdc++'s <cwchar>, which
// #undefs these names, has done so before they are defined here; a later
// include of it is then a guarded no-op.
#include <stddef.h>
#ifdef __cplusplus
  #include <cwchar>
  #include <cwctype>
extern "C" {
#else
  #include <wchar.h>
  #include <wctype.h>
#endif

wchar_t linscp_ToLower(wchar_t C);
wchar_t linscp_ToUpper(wchar_t C);
size_t linscp_WideLength(const wchar_t * S);
int linscp_WideCompare(const wchar_t * A, const wchar_t * B);
int linscp_WideCompareN(const wchar_t * A, const wchar_t * B, size_t N);
wchar_t * linscp_WideFindChar(const wchar_t * S, wchar_t C);
wchar_t * linscp_WideFindCharLast(const wchar_t * S, wchar_t C);
wchar_t * linscp_WideFindString(const wchar_t * Haystack, const wchar_t * Needle);
wchar_t * linscp_WideCopy(wchar_t * Dest, const wchar_t * Src);
wchar_t * linscp_WideCopyN(wchar_t * Dest, const wchar_t * Src, size_t N);
wchar_t * linscp_WideFindAny(const wchar_t * S, const wchar_t * Chars);
long linscp_WideToLong(const wchar_t * S, wchar_t ** End, int Base);
int linscp_WideToInt(const wchar_t * S);
int linscp_WideCompareIC(const wchar_t * A, const wchar_t * B);
long long linscp_WideToInt64(const wchar_t * S);
int linscp_WideIsAlpha(wchar_t C);
char * linscp_LowerAscii(char * S);
// swscanf for numeric input, which is all upstream parses with it (SCP's
// time line, FileZilla's MLSD dates): the string and format narrowed to
// ASCII and handed to vsscanf. As on Windows, %ld and friends store a 32-bit
// long. %s, %c and %[ would write narrow text into wide buffers, so a format
// with them fails (EOF) instead.
int linscp_WideScan(const wchar_t * S, const wchar_t * Format, ...);

#define towlower  linscp_ToLower
#define towupper  linscp_ToUpper
#define wcslen    linscp_WideLength
#define wcscmp    linscp_WideCompare
#define wcsncmp   linscp_WideCompareN
#define wcschr    linscp_WideFindChar
#define wcsrchr   linscp_WideFindCharLast
#define wcsstr    linscp_WideFindString
#define wcscpy    linscp_WideCopy
#define wcsncpy   linscp_WideCopyN
#define wcspbrk   linscp_WideFindAny
#define wcstol    linscp_WideToLong
#define _wtoi     linscp_WideToInt
#define _wcsicmp  linscp_WideCompareIC
#define _wtoi64   linscp_WideToInt64
#define swscanf   linscp_WideScan
// tchar.h's, as UNICODE maps it: iswalpha.
#define _istalpha linscp_WideIsAlpha
// The CRT's old name; in the C locale it folds ASCII only.
#define strlwr    linscp_LowerAscii
#define _strlwr   linscp_LowerAscii
// tchar.h's: the next character; UTF-16 is not stepped by code point.
#define _wcsinc(P) ((P) + 1)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // LINSCP_WIN32_ON_POSIX_WIDE_H
