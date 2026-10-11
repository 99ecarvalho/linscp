// Stand-in for Win32's <io.h> (low-level I/O).
//
// Declares only what WinSCP's core references.
#ifndef LINSCP_IO_H
#define LINSCP_IO_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"
#include <fcntl.h>
#include <unistd.h>

// No text mode on POSIX.
#ifndef O_BINARY
  #define O_BINARY 0
#endif

#ifdef __cplusplus
extern "C" {
#endif
// A file HANDLE is already its descriptor (see win32_on_posix.h), so the C
// runtime descriptor is the same number; _close then closes the HANDLE, as
// on Windows, where WebDAVFileSystem.cpp relies on that.
int _open_osfhandle(intptr_t Handle, int Flags);
int _close(int Fd);
#ifdef __cplusplus
}
#endif
#endif
