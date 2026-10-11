//---------------------------------------------------------------------------
// TVarRec and Format: Delphi's variant-argument formatting.
//
// Core reaches this through two macros in Global.h, used 438 and 252 times:
//     #define FORMAT(S, F)    Format(S, ARRAYOFCONST(F))
//     #define FMTLOAD(I, F)   FmtLoadStr(I, ARRAYOFCONST(F))
// written as FORMAT(L"%s=%s", (Name, Value)) -- note the extra parentheses.
//
// That shape is what makes this tractable. C++Builder builds an open array of
// TVarRec, which needs compound literals; here ARRAYOFCONST simply pastes the
// already-parenthesised list onto a variadic call, so
//     ARRAYOFCONST((Name, Value))  ->  MakeFormatArgs(Name, Value)
// which is ordinary C++ with no macro trickery beyond the paste.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_FORMAT_H
#define LINSCP_VCL_FORMAT_H

#include <vector>

#include "vcl_bytestring.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Delphi's variant record. Core declares these directly in a few places, so
// the tag names match Delphi's.
enum TVarRecType
{
  vtInteger, vtBoolean, vtChar, vtExtended, vtString, vtPointer, vtPChar,
  vtObject, vtClass, vtWideChar, vtPWideChar, vtAnsiString, vtCurrency,
  vtVariant, vtInterface, vtWideString, vtInt64, vtUnicodeString,
};

class TVarRec
{
public:
  TVarRec() = default;
  TVarRec(int V)            : FType(vtInteger), FInt(V) {}
  TVarRec(unsigned int V)   : FType(vtInteger), FInt(static_cast<__int64>(V)) {}
  TVarRec(long V)           : FType(vtInteger), FInt(V) {}
  TVarRec(unsigned long V)  : FType(vtInteger), FInt(static_cast<__int64>(V)) {}
  TVarRec(__int64 V)        : FType(vtInt64), FInt(V) {}
  TVarRec(unsigned __int64 V) : FType(vtInt64), FInt(static_cast<__int64>(V)) {}
  TVarRec(bool V)           : FType(vtBoolean), FInt(V ? 1 : 0) {}
  TVarRec(wchar_t V)        : FType(vtWideChar), FInt(static_cast<__int64>(V)) {}
  TVarRec(double V)         : FType(vtExtended), FFloat(V) {}
  TVarRec(const wchar_t * V) : FType(vtUnicodeString), FString(V) {}
  TVarRec(const char * V)   : FType(vtUnicodeString), FString(V) {}
  TVarRec(const UnicodeString & V) : FType(vtUnicodeString), FString(V) {}
  TVarRec(const AnsiString & V) : FType(vtUnicodeString), FString(ToUnicodeString(V)) {}
  TVarRec(const UTF8String & V) : FType(vtUnicodeString), FString(ToUnicodeString(V)) {}
  TVarRec(void * V)         : FType(vtPointer), FPointer(V) {}

  TVarRecType Type() const { return FType; }
  __int64 AsInt() const { return FInt; }
  double AsFloat() const { return FFloat; }
  const UnicodeString & AsString() const { return FString; }
  const void * AsPointer() const { return FPointer; }

  // Rendered the way Delphi's %s renders each variant type.
  UnicodeString ToDisplayString() const;

private:
  TVarRecType FType = vtInteger;
  __int64 FInt = 0;
  double FFloat = 0.0;
  UnicodeString FString;
  void * FPointer = nullptr;
};

using TFormatArgs = std::vector<TVarRec>;

template <typename... A>
TFormatArgs MakeFormatArgs(A &&... Args)
{
  return TFormatArgs{ TVarRec(std::forward<A>(Args))... };
}

// Pastes the already-parenthesised argument list onto the variadic call.
#define ARRAYOFCONST(VALUES) MakeFormatArgs VALUES

UnicodeString Format(const UnicodeString & Fmt, const TFormatArgs & Args);

// Format a resource string: FmtLoadStr(Ident, Args) is Format(LoadStr(Ident),
// Args). Declared but not defined here on purpose -- LoadStr is core's own
// (Common.h) and reads from the resource module, so the definition belongs with
// the resource layer rather than the RTL shim.
UnicodeString FmtLoadStr(int Ident, const TFormatArgs & Args);

#endif // LINSCP_VCL_FORMAT_H
