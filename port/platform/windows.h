// Stand-in for Win32's <windows.h>.
//
// Core includes this directly in a few places, so the name has to resolve on
// POSIX too. It forwards to the Win32 subset the port implements.
#ifndef LINSCP_WINDOWS_H_COMPAT
#define LINSCP_WINDOWS_H_COMPAT

#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real <windows.h> must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"

#endif
