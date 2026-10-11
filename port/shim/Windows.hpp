// Stand-in for Embarcadero's Windows.hpp.
//
// C++Builder exposes RTL units under both their unqualified legacy name and
// their System.* name, and core uses both spellings. Everything resolves to the
// single shim aggregate.
#ifndef LINSCP_WINDOWS_HPP
#define LINSCP_WINDOWS_HPP
#include "vcl_rtl.h"
#endif
