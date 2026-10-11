//---------------------------------------------------------------------------
// System::DynamicArray, C++Builder's mapping of a Delphi dynamic array.
//
// Core passes these by value -- TSafeHandleStream::Read takes a
// DynamicArray<Byte> -- so, like Delphi's, the type is reference-counted:
// copying shares the buffer rather than duplicating it, and a caller that
// writes into the copy is seen by the original. Deep-copying instead would
// make those reads silently return nothing.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_DYNAMICARRAY_H
#define LINSCP_VCL_DYNAMICARRAY_H

#include <cstddef>
#include <memory>
#include <vector>

#include "vcl_types.h"

namespace System {

using Byte = ::Byte;
using Word = ::Word;
using Integer = ::Integer;

template <typename T>
class DynamicArray
{
public:
  DynamicArray() = default;
  explicit DynamicArray(int Len) { set_length(Len); }

  // Shares the buffer, as Delphi's dynamic arrays do.
  DynamicArray(const DynamicArray &) = default;
  DynamicArray & operator=(const DynamicArray &) = default;

  int Length() const
    { return FData ? static_cast<int>(FData->size()) : 0; }
  int get_length() const { return Length(); }

  void set_length(int Len)
  {
    if (Len <= 0)
    {
      FData.reset();
      return;
    }
    if (!FData)
    {
      FData = std::make_shared<std::vector<T>>();
    }
    FData->resize(static_cast<std::size_t>(Len));
  }

  // 0-based, like Delphi's dynamic arrays and unlike its strings.
  T & operator[](int Index) { return (*FData)[static_cast<std::size_t>(Index)]; }
  const T & operator[](int Index) const
    { return (*FData)[static_cast<std::size_t>(Index)]; }

  T * data() { return FData ? FData->data() : nullptr; }
  const T * data() const { return FData ? FData->data() : nullptr; }

  bool IsEmpty() const { return Length() == 0; }

private:
  std::shared_ptr<std::vector<T>> FData;
};

}  // namespace System

using TBytes = System::DynamicArray<System::Byte>;

#endif // LINSCP_VCL_DYNAMICARRAY_H
