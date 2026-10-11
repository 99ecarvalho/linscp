// Aggregate of the VCL runtime shim.
#ifndef LINSCP_VCL_RTL_H
#define LINSCP_VCL_RTL_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// Embarcadero's <vcl.h> transitively exposes the Win32 API, and upstream's
// CorePCH.h relies on that: it includes <vcl.h> and core's own headers before
// <Winapi.Windows.hpp>, so DWORD, HANDLE and friends must already be visible.
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

#include "vcl_property.h"
#include "vcl_object.h"
#include "vcl_syncobjs.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"
#include "vcl_bytestring.h"
#include "vcl_datetime.h"

// Embarcadero calling-convention and closure keywords. __fastcall is a pure
// calling convention and means nothing to us on x86-64; __closure becomes a
// bound callable (see vcl_closure.h once TStrings lands).
#ifndef __BORLANDC__
  #define __fastcall
#endif

#endif // LINSCP_VCL_RTL_H
