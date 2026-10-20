// Forced first into every FileZilla unit and the MFC-compatible layer
// (port/mfc-compat) it uses (-include), so they see the same Win32
// environment as core and the shim: <windows.h> with its A/W name macros
// removed.
//
// FileZilla includes <afx.h> before any core header. Without this, the MFC
// classes would be declared under the macros -- a member named like a Win32
// function becomes its ...W form -- and the shim then removes them, so
// FileZilla's own calls no longer match. Under C++Builder the macros simply
// stay throughout; here they are gone throughout, which is consistent too.
// Win32 functions called under the bare name reach the API through
// vcl_win32_undef.h's forwarders.
#ifndef LINSCP_VCL_WIN32_PRELUDE_H
#define LINSCP_VCL_WIN32_PRELUDE_H
#if defined(_WIN32)
  // No min/max macros from <windows.h>: libstdc++, unlike MSVC's library,
  // does not survive them.
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <windows.h>
  #include "vcl_win32_undef.h"
  // A header that tests <windows.h>'s include guard to refuse being
  // included after it still compiles: every
  // sub-header windows.h pulls in has its own guard, so dropping the outer
  // one lets windows.h be included again without bringing the macros back.
  // The check protects MSVC builds from include-order mistakes, which a
  // forced prelude rules out.
  #undef _WINDOWS_

  // <algorithm> first, so that a later `#define max std::max` (or min) in
  // upstream code cannot rewrite libstdc++'s own declarations when
  // <algorithm> is included after it: its guard makes that include a no-op.
  #ifdef __cplusplus
    #include <algorithm>
  #endif
#else
  // POSIX: the port's Win32 layer, which has no name macros to remove. The
  // <algorithm> reasoning above holds here as well.
  #include "windows.h"
  #include "winsock2.h"
  #ifdef __cplusplus
    #include <algorithm>
  #endif
#endif
#endif
