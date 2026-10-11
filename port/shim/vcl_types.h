// Delphi scalar types and aliases used throughout WinSCP's core.
#ifndef LINSCP_VCL_TYPES_H
#define LINSCP_VCL_TYPES_H

#include <cstdint>

// Delphi integer aliases. Fixed-width on purpose: core serialises some of these
// into protocol and registry structures, so their size is part of the contract.
using Word     = std::uint16_t;
using Byte     = std::uint8_t;
using Cardinal = std::uint32_t;
using Smallint = std::int16_t;
using Shortint = std::int8_t;
using Longint  = std::int32_t;
using Longword = std::uint32_t;
using Int64    = std::int64_t;

// Delphi's capitalised aliases for the built-in types. Core uses these
// interchangeably with the C++ spellings.
using Integer  = std::int32_t;
using Boolean  = bool;
using Double   = double;
using Single   = float;
using Extended = long double;
using NativeInt  = std::intptr_t;
using NativeUInt = std::uintptr_t;
using Pointer  = void *;

// Delphi aliases core uses in declarations.
using TShortCut = Word;              // a virtual key plus shift-state bits
using TThreadID = std::uint32_t;
// Delphi: function(Parameter: Pointer): Integer. Core passes an int function.
using TThreadFunc = int (*)(void *);

// Pointer to a resource-string record. Core only ever passes these through to
// the resource layer, so the record itself stays opaque.
struct TResStringRec
{
  void * Module = nullptr;
  unsigned Identifier = 0;
};
using PResStringRec = TResStringRec *;
using UInt64   = std::uint64_t;

// __int64 is clang's Microsoft-extension keyword (-fms-extensions is
// mandatory here), not a macro. A macro expanding to `long long` breaks core's
// functional casts such as __int64(HSize), which is why mingw's own
// #define __int64 is undone in vcl_win32_undef.h.
static_assert(sizeof(__int64) == 8, "__int64 must be the 64-bit keyword");

namespace System {

// C++Builder's fixed-size array, 0-based. One slot of slack is kept past N:
// core passes Size() where an open-array parameter expects the *high index*
// (Common.cpp's ParseShortEngMonthName), so it reads element N. In
// C++Builder that silently reads whatever follows; here it reads the slack,
// which owners fill with a value no input matches. Defined behaviour, and
// "not found" rather than a bogus index.
template <typename T, int N>
class StaticArray
{
public:
  int Size() const { return N; }
  T & operator[](int Index) { return FData[Index]; }
  const T & operator[](int Index) const { return FData[Index]; }
  operator T *() { return FData; }
  operator const T *() const { return FData; }
  T & Slack() { return FData[N]; }

private:
  T FData[N + 1] = {};
};

}  // namespace System
using System::StaticArray;

#endif // LINSCP_VCL_TYPES_H
