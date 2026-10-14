// Stand-ins for what the custom commands' slices call in parts of
// upstream's GUI linscp does not have (CustomCommandsSlice.h).
#include <vcl.h>
#pragma hdrstop

#include "CustomCommandsSlice.h"

// windows/Setup.cpp detects these on Windows from the registry; none known
// here, so an extension requiring one is not offered.
UnicodeString GetNetVersionStr()
{
  return UnicodeString();
}

UnicodeString GetNetCoreVersionStr()
{
  return UnicodeString();
}

UnicodeString GetPowerShellVersionStr()
{
  return UnicodeString();
}

UnicodeString GetPowerShellCoreVersionStr()
{
  return UnicodeString();
}
