// Stand-in for Embarcadero's SysInit.hpp.
//
// C++Builder exposes RTL units under both their unqualified legacy name and
// their System.* name, and core uses both spellings. Everything resolves to the
// single shim aggregate.
#ifndef LINSCP_SYSINIT_HPP
#define LINSCP_SYSINIT_HPP
#include "vcl_rtl.h"
#endif
