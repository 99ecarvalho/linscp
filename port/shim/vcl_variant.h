//---------------------------------------------------------------------------
// System.Variant, as far as core uses it.
//
// One use, in SftpFileSystem: packet numbers kept in a one-dimensional variant
// array of varLongWord, through VarArrayCreate, GetElement, PutElement,
// ArrayHighBound and ArrayRedim. That is what is provided. Anything wider --
// a second dimension, a non-integer element type -- raises rather than
// half-working, so a new use in upstream shows up as an error.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_VARIANT_H
#define LINSCP_VCL_VARIANT_H

#include <vector>

#include "vcl_object.h"
#include "vcl_types.h"

using TVarType = Word;
constexpr TVarType varEmpty    = 0x0000;
constexpr TVarType varInteger  = 0x0003;
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

  TVarType VarType() const { return FType; }
  bool IsArray() const { return (FType & varArray) != 0; }

  operator int() const;
  operator unsigned int() const;
  operator __int64() const;

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
  int FLow = 0;
  std::vector<__int64> FElements;
};

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
