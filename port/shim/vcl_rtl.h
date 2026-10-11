// Aggregate of the VCL runtime shim.
#ifndef LINSCP_VCL_RTL_H
#define LINSCP_VCL_RTL_H

#include <cmath>     // <vcl.h> brings in the C maths functions; core relies on it
#include <cstddef>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>

// Embarcadero's <vcl.h> transitively exposes the Win32 API, and upstream's
// CorePCH.h relies on that: it includes <vcl.h> and core's own headers before
// <Winapi.Windows.hpp>, so DWORD, HANDLE and friends must already be visible.
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  // COM, which core initialises per thread (Queue.cpp). WIN32_LEAN_AND_MEAN
  // drops it from <windows.h>, where Embarcadero's <vcl.h> has it.
  #include <objbase.h>
  // The shell API (SHFileOperation), which WIN32_LEAN_AND_MEAN also drops,
  // as it does CryptoAPI (certificate chains) and the shell folders.
  #include <shellapi.h>
  #include <shlobj.h>
  #include <wincrypt.h>
  #include <tchar.h>
  using TSHFileInfoW = SHFILEINFOW;   // Delphi's alias

  // A Windows SDK definition mingw-w64's headers lack: ws2def.h's
  // _IOR('t', 123, ULONG).
  #ifndef SIO_IDEAL_SEND_BACKLOG_QUERY
    #define SIO_IDEAL_SEND_BACKLOG_QUERY 0x4004747B
  #endif

  #include "vcl_win32_undef.h"
#else
  // Same reasoning on POSIX: core's headers reference DWORD, FILETIME, HKEY and
  // friends before anything includes a platform header explicitly, so the
  // Win32 subset must be visible from <vcl.h> onwards.
  #include "win32_on_posix.h"
#endif

#include "vcl_property.h"
#include "vcl_tryfinally.h"
#include "vcl_closure.h"
#include "vcl_object.h"
#include "vcl_syncobjs.h"
#include "vcl_thread.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"
#include "vcl_bytestring.h"
#include "vcl_datetime.h"
#include "vcl_strings.h"
#include "vcl_sysutils.h"
#include "vcl_strutils.h"
#include "vcl_format.h"
#include "vcl_streams.h"
#include "vcl_dynamicarray.h"
#include "vcl_set.h"
#include "vcl_containers.h"
#include "vcl_resources.h"
#include "vcl_inifiles.h"
#include "vcl_registry.h"
#include "vcl_misc.h"
#include "vcl_xml.h"
#include "vcl_variant.h"
#include "vcl_json.h"

// Embarcadero calling-convention and closure keywords. __fastcall is a pure
// calling convention and means nothing to us on x86-64; __closure becomes a
// bound callable (see vcl_closure.h once TStrings lands).
#ifndef __BORLANDC__
  #define __fastcall
#endif

#endif // LINSCP_VCL_RTL_H
