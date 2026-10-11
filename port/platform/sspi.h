// Stand-in for Win32's <sspi.h> (Security Support Provider Interface).
//
// SessionInfo.cpp includes it for <secext.h>'s GetUserNameEx; nothing else
// from SSPI is used.
#ifndef LINSCP_SSPI_H
#define LINSCP_SSPI_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
#endif
