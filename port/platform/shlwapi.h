// Stand-in for Win32's <shlwapi.h> (Shell path helpers).
//
// Declares only what WinSCP's core references. Deliberately thin: anything
// missing shows up as a compile error naming the exact symbol, which is a
// better outcome than a stub that silently does nothing at runtime.
#ifndef LINSCP_SHLWAPI_H
#define LINSCP_SHLWAPI_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
#endif
