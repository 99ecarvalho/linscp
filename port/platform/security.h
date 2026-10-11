// Stand-in for Win32's <security.h> (SSPI). Only the user-name function
// from <secext.h> is used outside GSSAPI, whose Windows unit Linux replaces.
#ifndef LINSCP_SECURITY_H
#define LINSCP_SECURITY_H
#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "sspi.h"
#include "secext.h"
#endif
