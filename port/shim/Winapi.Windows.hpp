// Stand-in for Embarcadero's Winapi.Windows.hpp, which re-exports the Win32 API.
//
// On the mingw target the real Win32 headers exist, so this just forwards to
// them. On Linux the equivalent subset will come from port/platform
// (win32-on-posix, task P1-07); until that lands, Linux builds of core will
// fail here, which is the honest signal.
#ifndef LINSCP_WINAPI_WINDOWS_HPP
#define LINSCP_WINAPI_WINDOWS_HPP

#include "vcl_rtl.h"

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>

  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
#endif

#endif
