// Stand-in for Embarcadero's <vcl.h>.
//
// Upstream's CorePCH.h includes this; it is the single entry point to the VCL
// runtime shim. Nothing here is a real VCL header -- each declares only what
// WinSCP's core actually references.
#ifndef LINSCP_VCL_H
#define LINSCP_VCL_H
#include "vcl_rtl.h"
#endif
