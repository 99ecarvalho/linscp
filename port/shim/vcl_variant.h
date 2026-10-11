//---------------------------------------------------------------------------
// System.Variant, as far as core uses it.
//
// Two uses. SftpFileSystem keeps packet numbers in a one-dimensional variant
// array of varLongWord (VarArrayCreate, GetElement, PutElement, ArrayHighBound,
// ArrayRedim). SessionData reads XML attributes as OleVariant, which is Null
// when the attribute is absent and a string otherwise. That is what is
// provided: integers, strings, Null, and integer arrays. Anything wider -- a
// second dimension, another element type -- raises rather than half-working,
// so a new use in upstream shows up as an error.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_VARIANT_H
#define LINSCP_VCL_VARIANT_H

#include <type_traits>
#include <vector>

#include "vcl_object.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

using TVarType = Word;
constexpr TVarType varEmpty    = 0x0000;
constexpr TVarType varNull     = 0x0001;
constexpr TVarType varUString  = 0x0102;
constexpr TVarType varInteger  = 0x0003;
constexpr TVarType varDouble   = 0x0005;
constexpr TVarType varLongWord = 0x0013;
constexpr TVarType varInt64    = 0x0014;
constexpr TVarType varArray    = 0x2000;

class Variant
{
public:
  Variant() = default;
  Variant(int Value) : FType(varInteger), FValue(Value) {}
  Variant(unsigned int Value) : FType(varLongWord), FValue(Value) {}
  Variant(__int64 Value) : FType(varInt64), FValue(Value) {}
  Variant(double Value) : FType(varDouble), FFloat(Value) {}
  Variant(const UnicodeString & Value) : FType(varUString), FString(Value) {}
  Variant(const wchar_t * Value) : FType(varUString), FString(Value) {}

  // Delphi's Null(): distinct from Empty, and what an absent XML attribute is.
  static Variant MakeNull() { Variant V; V.FType = varNull; return V; }

  TVarType VarType() const { return FType; }
  bool IsArray() const { return (FType & varArray) != 0; }
  bool IsNull() const { return FType == varNull; }
  bool IsEmpty() const { return FType == varEmpty; }

  operator int() const;
  operator unsigned int() const;
  operator __int64() const;
  operator double() const;
  // Null raises EVariantTypeCastError, as with Delphi's default
  // NullStrictConvert; Empty gives an empty string; integers their digits.
  operator UnicodeString() const;

  // String comparison, as Delphi compares a string variant with a string.
  // Null equals nothing, Delphi's default ncrLoose rule.
  friend bool operator==(const Variant & V, const UnicodeString & S)
    { return !V.IsNull() && (static_cast<UnicodeString>(V) == S); }
  friend bool operator==(const UnicodeString & S, const Variant & V) { return V == S; }
  friend bool operator!=(const Variant & V, const UnicodeString & S) { return !(V == S); }
  friend bool operator!=(const UnicodeString & S, const Variant & V) { return !(V == S); }

  // Arithmetic as Delphi's variants do it, in double. Templates so a plain
  // number on either side is an exact match: FtpFileSystem.cpp writes
  // SecsPerDay * static_cast<Variant>(Difference).
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  friend Variant operator*(T A, const Variant & B)
    { return Variant(static_cast<double>(A) * static_cast<double>(B)); }
  template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
  friend Variant operator*(const Variant & A, T B)
    { return Variant(static_cast<double>(A) * static_cast<double>(B)); }
  friend Variant operator*(const Variant & A, const Variant & B)
    { return Variant(static_cast<double>(A) * static_cast<double>(B)); }

  // Variant arrays. Copies are deep, as Delphi's VarCopy of an array is.
  Variant GetElement(int Index) const;
  void PutElement(const Variant & Value, int Index);
  int ArrayLowBound(int Dim = 1) const;
  int ArrayHighBound(int Dim = 1) const;
  // Delphi's VarArrayRedim: a new high bound, keeping existing elements.
  void ArrayRedim(int HighBound);

private:
  friend Variant VarArrayCreate(const int * Bounds, int Bounds_High, TVarType VarType);

  void CheckArray() const;
  void CheckIndex(int Index) const;
  __int64 Scalar() const;

  TVarType FType = varEmpty;
  __int64 FValue = 0;
  double FFloat = 0.0;
  UnicodeString FString;
  int FLow = 0;
  std::vector<__int64> FElements;
};

using OleVariant = Variant;
inline Variant Null() { return Variant::MakeNull(); }

// Bounds holds low/high pairs, one per dimension; Bounds_High is the index of
// the last entry, as OPENARRAY produces it.
Variant VarArrayCreate(const int * Bounds, int Bounds_High, TVarType VarType);

namespace linscp {

// What C++Builder's OPENARRAY builds: a temporary array and its high index,
// passed as two arguments. The temporary lives to the end of the call.
template <typename T>
class TOpenArray
{
public:
  template <typename... A>
  explicit TOpenArray(A... Values) : FValues{static_cast<T>(Values)...} {}
  const T * Data() const { return FValues.data(); }
  int High() const { return static_cast<int>(FValues.size()) - 1; }

private:
  std::vector<T> FValues;
};

}  // namespace linscp

#ifndef __BORLANDC__
  #define OPENARRAY(T, VALUES) \
    ::linscp::TOpenArray<T> VALUES .Data(), ::linscp::TOpenArray<T> VALUES .High()
#endif

#endif // LINSCP_VCL_VARIANT_H
