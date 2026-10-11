// Stand-in for Win32's <commctrl.h> (common controls). PuTTY's
// windows/utils/defaults.c includes it for nothing it uses on POSIX.
#ifndef LINSCP_COMMCTRL_H
#define LINSCP_COMMCTRL_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
#endif
