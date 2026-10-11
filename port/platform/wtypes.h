// Stand-in for Win32's <wtypes.h>, which FileZilla's stdafx.h includes for
// the basic types. It forwards to the Win32 subset the port implements.
#ifndef LINSCP_WTYPES_H_COMPAT
#define LINSCP_WTYPES_H_COMPAT

#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real <wtypes.h> must be used."
#endif

#include "win32_on_posix.h"

#endif
