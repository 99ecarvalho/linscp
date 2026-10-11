//---------------------------------------------------------------------------
// Delphi set types.
//
// Delphi has first-class sets over a contiguous enum range, and C++Builder
// exposes them as Set<T, Low, High>. Core uses TShiftState this way, so the
// operators it relies on -- << to include, >> to exclude, Contains -- have to
// behave the same.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_SET_H
#define LINSCP_VCL_SET_H

#include <bitset>
#include <cstddef>

template <typename T, int Low, int High>
class Set
{
public:
  static constexpr std::size_t Range = static_cast<std::size_t>(High - Low + 1);

  Set() = default;

  // Delphi's include and exclude operators, which mutate and return *this so
  // they chain: S << ssShift << ssCtrl.
  Set & operator<<(T Element)
  {
    const int I = static_cast<int>(Element) - Low;
    if (I >= 0 && static_cast<std::size_t>(I) < Range)
    {
      FBits.set(static_cast<std::size_t>(I));
    }
    return *this;
  }

  Set & operator>>(T Element)
  {
    const int I = static_cast<int>(Element) - Low;
    if (I >= 0 && static_cast<std::size_t>(I) < Range)
    {
      FBits.reset(static_cast<std::size_t>(I));
    }
    return *this;
  }

  bool Contains(T Element) const
  {
    const int I = static_cast<int>(Element) - Low;
    return I >= 0 && static_cast<std::size_t>(I) < Range &&
           FBits.test(static_cast<std::size_t>(I));
  }

  bool Empty() const { return FBits.none(); }
  void Clear() { FBits.reset(); }

  // Union, difference and intersection, as Delphi's +, - and *.
  Set operator+(const Set & Other) const
  {
    Set R;
    R.FBits = FBits | Other.FBits;
    return R;
  }
  Set operator-(const Set & Other) const
  {
    Set R;
    R.FBits = FBits & ~Other.FBits;
    return R;
  }
  Set operator*(const Set & Other) const
  {
    Set R;
    R.FBits = FBits & Other.FBits;
    return R;
  }

  friend bool operator==(const Set & A, const Set & B)
    { return A.FBits == B.FBits; }
  friend bool operator!=(const Set & A, const Set & B)
    { return A.FBits != B.FBits; }

private:
  std::bitset<Range> FBits;
};

// Shift-key state, as in VCL's Classes unit.
enum TShiftStateEnum
{
  ssShift, ssAlt, ssCtrl, ssLeft, ssRight, ssMiddle, ssDouble, ssTouch,
  ssPen, ssCommand, ssHorizontal
};

using TShiftState = Set<TShiftStateEnum, ssShift, ssHorizontal>;

#endif // LINSCP_VCL_SET_H
