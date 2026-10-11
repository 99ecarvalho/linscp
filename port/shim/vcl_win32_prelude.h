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
  #include <winsock2.h>
  #include <windows.h>
  #include "vcl_win32_undef.h"
  // MFC's afx.h refuses to compile after <windows.h> by testing its include
  // guard. Every sub-header windows.h pulls in has its own guard, so dropping
  // the outer one lets afx.h include windows.h again without bringing the
  // macros back. The check protects MSVC builds from include-order mistakes,
  // which a forced prelude rules out.
  #undef _WINDOWS_
#endif
#endif
