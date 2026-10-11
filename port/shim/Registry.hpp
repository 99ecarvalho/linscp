// Stand-in for Embarcadero's Registry.hpp.
//
// C++Builder exposes RTL units under both their unqualified legacy name and
// their System.* name, and core uses both spellings. Everything resolves to the
// single shim aggregate.
#ifndef LINSCP_REGISTRY_HPP
#define LINSCP_REGISTRY_HPP
#include "vcl_rtl.h"
#endif
