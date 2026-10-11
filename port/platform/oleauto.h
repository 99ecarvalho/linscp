// Stand-in for Win32's <oleauto.h>. FileZilla's stdafx.h includes it, but
// neither FileZilla nor WinSCP's MFC subset calls OLE Automation; only the
// name has to resolve.
#ifndef LINSCP_OLEAUTO_H_COMPAT
#define LINSCP_OLEAUTO_H_COMPAT

#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real <oleauto.h> must be used."
#endif

#include "win32_on_posix.h"

#endif
