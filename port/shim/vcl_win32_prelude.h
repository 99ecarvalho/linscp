// Forced first into every MFC and FileZilla unit (-include), so they see the
// same Win32 environment as core and the shim: <windows.h> with its A/W name
// macros removed.
//
// FileZilla includes MFC's afx.h before any core header. Without this, MFC's
// classes are declared under the macros -- CString::LoadString becomes
// LoadStringW -- and the shim then removes them, so FileZilla's own calls no
// longer match. Under C++Builder the macros simply stay throughout; here they
// are gone throughout, which is consistent too. Win32 functions called under
// the bare name reach the API through vcl_win32_undef.h's forwarders.
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
  // MFC's afx.h refuses to compile after <windows.h> by testing its include
  // guard. Every sub-header windows.h pulls in has its own guard, so dropping
  // the outer one lets afx.h include windows.h again without bringing the
  // macros back. The check protects MSVC builds from include-order mistakes,
  // which a forced prelude rules out.
  #undef _WINDOWS_

  // WinSCP's MFC defines `max` as std::max (and min likewise) in its
  // stdafx.h, and strex.cpp includes <algorithm> after that. Included here
  // first, its guard makes that later include a no-op -- as it evidently was
  // under C++Builder -- instead of the macro rewriting libstdc++'s own
  // declarations.
  #ifdef __cplusplus
    #include <algorithm>
  #endif
#endif
#endif
