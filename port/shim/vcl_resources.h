//---------------------------------------------------------------------------
// Resource strings.
//
// core/Common.cpp already implements LoadStr, LoadStrFrom and LoadStrPart; it
// does so in terms of three things the VCL/Win32 runtime supplies:
//
//     HInstance        the module handle
//     LibModuleList    the VCL module list that FindModule walks
//     LoadString()     the Win32 API that copies a string by numeric id
//
// So the cheapest correct route is to supply those three, not to write a .res
// parser and not to patch core. Core stays byte-identical, the resource binary
// format never has to be understood, and what we add is purely additive --
// which also makes it the kind of change upstream could take rather than a
// fork of its own code.
//
// The table itself is generated from WinSCP's own .rc files by
// port/tools/gen_resources.py, so the strings stay in step with upstream
// automatically: re-running the transform regenerates them.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_RESOURCES_H
#define LINSCP_VCL_RESOURCES_H

#include <cstddef>

#include "vcl_types.h"

namespace linscp {

struct TResourceString
{
  int Ident;
  const wchar_t * Text;
};

// Defined by the generated translation unit.
extern const TResourceString ResourceStrings[];
extern const std::size_t ResourceStringCount;

// Returns nullptr when the id is unknown.
const wchar_t * FindResourceString(int Ident);

}  // namespace linscp

//---------------------------------------------------------------------------
// The VCL/Win32 surface core's LoadStr is written against.
//---------------------------------------------------------------------------

// Delphi's module record. Field names and the integral Instance type match
// what core's FindModule compares against.
struct TLibModule
{
  std::uintptr_t Instance = 0;
  std::uintptr_t CodeInstance = 0;
  std::uintptr_t DataInstance = 0;
  std::uintptr_t ResInstance = 0;
  TLibModule * Next = nullptr;
};

extern void * HInstance;
extern void * LibModuleList;

// Win32's LoadString, serving the generated table. Returns the number of
// characters copied, not counting the terminator, which is what core's
// DoLoadStrFrom uses to trim the result.
int LoadString(void * Module, unsigned Ident, wchar_t * Buffer, int BufferMax);

#endif // LINSCP_VCL_RESOURCES_H
