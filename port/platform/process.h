// Stand-in for Win32's <process.h> (process control).
//
// Declares only what WinSCP's core references.
#ifndef LINSCP_PROCESS_H
#define LINSCP_PROCESS_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
// getpid, which Cryptography.cpp seeds rand() with.
#include <unistd.h>
#endif
