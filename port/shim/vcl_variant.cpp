#include "vcl_variant.h"

#include "vcl_strutils.h"

Variant VarArrayCreate(const int * Bounds, int Bounds_High, TVarType VarType)
{
  if ((Bounds == nullptr) || (Bounds_High != 1))
  {
    throw EVariantError(UnicodeString(L"Only one-dimensional variant arrays are supported"));
  }
  if ((VarType != varLongWord) && (VarType != varInteger) && (VarType != varInt64))
  {
    throw EVariantError(UnicodeString(L"Only integer variant arrays are supported"));
  }
  if (Bounds[1] < Bounds[0] - 1)
  {
    throw EVariantBoundsError(UnicodeString(L"Variant or safe array index out of bounds"));
  }
  Variant Result;
  Result.FType = static_cast<TVarType>(varArray | VarType);
  Result.FLow = Bounds[0];
  Result.FElements.assign(static_cast<std::size_t>(Bounds[1] - Bounds[0] + 1), 0);
  return Result;
}

void Variant::CheckArray() const
{
  if (!IsArray())
  {
    throw EVariantError(UnicodeString(L"Variant is not an array"));
  }
}

void Variant::CheckIndex(int Index) const
{
  CheckArray();
  if ((Index < FLow) || (Index > ArrayHighBound()))
  {
    throw EVariantBoundsError(UnicodeString(L"Variant or safe array index out of bounds"));
  }
}

__int64 Variant::Scalar() const
{
  if (IsArray() || IsNull())
  {
    throw EVariantTypeCastError(UnicodeString(L"Invalid variant type conversion"));
  }
  if (FType == varUString)
  {
    __int64 V = 0;
    if (!TryStrToInt64(FString, V))
    {
      throw EVariantTypeCastError(
        UnicodeString(L"Could not convert variant of type (UnicodeString) into type (Int64)"));
    }
    return V;
  }
  return FValue;
}

Variant::operator UnicodeString() const
{
  switch (FType)
  {
    case varEmpty:   return UnicodeString();
    case varUString: return FString;
    case varNull:
      throw EVariantTypeCastError(
        UnicodeString(L"Could not convert variant of type (Null) into type (UnicodeString)"));
    default:
      if (IsArray())
      {
        throw EVariantTypeCastError(UnicodeString(L"Invalid variant type conversion"));
      }
      return (FType == varLongWord) ? UIntToStr(static_cast<unsigned __int64>(FValue))
                                    : IntToStr(FValue);
  }
}

Variant::operator int() const { return static_cast<int>(Scalar()); }
Variant::operator unsigned int() const { return static_cast<unsigned int>(Scalar()); }
Variant::operator __int64() const { return Scalar(); }

Variant Variant::GetElement(int Index) const
{
  CheckIndex(Index);
  const __int64 V = FElements[static_cast<std::size_t>(Index - FLow)];
  switch (FType & ~varArray)
  {
    case varLongWord: return Variant(static_cast<unsigned int>(V));
    case varInteger:  return Variant(static_cast<int>(V));
    default:          return Variant(V);
  }
}

void Variant::PutElement(const Variant & Value, int Index)
{
  CheckIndex(Index);
  __int64 V = Value.Scalar();
  // Stored as the element type would hold it.
  switch (FType & ~varArray)
  {
    case varLongWord: V = static_cast<unsigned int>(V); break;
    case varInteger:  V = static_cast<int>(V); break;
    default: break;
  }
  FElements[static_cast<std::size_t>(Index - FLow)] = V;
}

int Variant::ArrayLowBound(int Dim) const
{
  CheckArray();
  if (Dim != 1)
  {
    throw EVariantBoundsError(UnicodeString(L"Variant or safe array index out of bounds"));
  }
  return FLow;
}

int Variant::ArrayHighBound(int Dim) const
{
  CheckArray();
  if (Dim != 1)
  {
    throw EVariantBoundsError(UnicodeString(L"Variant or safe array index out of bounds"));
  }
  return FLow + static_cast<int>(FElements.size()) - 1;
}

void Variant::ArrayRedim(int HighBound)
{
  CheckArray();
  if (HighBound < FLow - 1)
  {
    throw EVariantBoundsError(UnicodeString(L"Variant or safe array index out of bounds"));
  }
  FElements.resize(static_cast<std::size_t>(HighBound - FLow + 1), 0);
}
