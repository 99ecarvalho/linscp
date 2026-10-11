// Stand-in for Win32's <lmcons.h> (LAN Manager constants).
//
// Declares only what WinSCP's core references.
#ifndef LINSCP_LMCONS_H
#define LINSCP_LMCONS_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
#define UNLEN 256
#define DNLEN 15
#endif
