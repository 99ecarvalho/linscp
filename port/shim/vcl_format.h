//---------------------------------------------------------------------------
// TVarRec and Format: Delphi's variant-argument formatting.
//
// Core reaches this through two macros in Global.h, used 438 and 252 times:
//     #define FORMAT(S, F)    Format(S, ARRAYOFCONST(F))
//     #define FMTLOAD(I, F)   FmtLoadStr(I, ARRAYOFCONST(F))
// written as FORMAT(L"%s=%s", (Name, Value)) -- note the extra parentheses.
//
// ARRAYOFCONST expands, as in C++Builder, to the two arguments of a Delphi open
// array: a TVarRec pointer and the array's *high index* (count - 1). Core's
// own functions take them that way -- ExecCommand(Cmd, const TVarRec * Args,
// int Size), ExtException(Msg, Args, Args_Size) -- so a single container
// argument cannot work. The pointer is a temporary array's, alive to the end
// of the full expression. The count comes from sizeof over an unevaluated
// call, so unlike C++Builder's macro each value is evaluated exactly once.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_FORMAT_H
#define LINSCP_VCL_FORMAT_H

#include <vector>

#include "vcl_bytestring.h"
#include "vcl_types.h"
#include "vcl_datetime.h"
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
  TVarRec(const RawByteString & V) : FType(vtUnicodeString), FString(ToUnicodeString(V)) {}
  TVarRec(long double V)    : FType(vtExtended), FFloat(static_cast<double>(V)) {}
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

namespace linscp {

// Never called: only its return type's size is used, to count arguments.
template <typename... A>
char (&FormatArgCount(A &&...))[sizeof...(A) + 1];

}  // namespace linscp

#define ARRAYOFCONST(VALUES) \
  MakeFormatArgs VALUES .data(), \
  (static_cast<int>(sizeof(::linscp::FormatArgCount VALUES)) - 2)

// Delphi's Format. Args_High is the open array's high index, -1 for none.
UnicodeString Format(const UnicodeString & Fmt, const TVarRec * Args, int Args_High);
// The same, over a container; convenient for the shim's own callers.
UnicodeString Format(const UnicodeString & Fmt, const TFormatArgs & Args);

// SysUtils' FmtLoadStr: Format(LoadStr(Ident), Args).
UnicodeString FmtLoadStr(int Ident, const TVarRec * Args, int Args_High);

// Delphi's FormatFloat, for the placeholder subset: '0' and '#' digits, ','
// grouping, '.' decimals, quoted or plain literal text, and up to three ';'
// sections (positive;negative;zero). Separators from FormatSettings. Rounds
// half away from zero. Scientific notation is not supported and raises
// EConvertError, so a new use in upstream is noticed.
UnicodeString FormatFloat(const UnicodeString & Format, double Value);
UnicodeString FormatFloat(const UnicodeString & Format, double Value,
                          const TFormatSettings & Settings);

#endif // LINSCP_VCL_FORMAT_H
