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
using UInt64   = std::uint64_t;

#ifndef __BORLANDC__
  // Embarcadero spells 64-bit integers this way; mingw's headers do too, so
  // only define it where neither has.
  #ifndef _MSC_VER
    #ifndef __int64
      #define __int64 long long
    #endif
  #endif
#endif

#endif // LINSCP_VCL_TYPES_H
