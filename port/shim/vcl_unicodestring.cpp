#include "vcl_unicodestring.h"
#include "vcl_bytestring.h"

#include <algorithm>
#include <cassert>

namespace {

// wchar_t and char16_t are both 2 bytes and identically represented under
// -fshort-wchar. These two casts are the only place the port relies on that,
// which is why they live here and nowhere else.
inline const char16_t * AsU16(const wchar_t * S)
{
  return reinterpret_cast<const char16_t *>(S);
}

inline const wchar_t * AsWide(const char16_t * S)
{
  return reinterpret_cast<const wchar_t *>(S);
}

// Length of a 2-byte-wide C string. Hand-rolled because libc's wcslen is
// compiled for 4-byte wchar_t (ADR 0001).
std::size_t WideLen(const wchar_t * S)
{
  const char16_t * P = AsU16(S);
  std::size_t N = 0;
  while (P[N] != u'\0')
  {
    ++N;
  }
  return N;
}

inline char16_t FoldAscii(char16_t C)
{
  return (C >= u'A' && C <= u'Z') ? static_cast<char16_t>(C - u'A' + u'a') : C;
}

inline bool IsSpace(char16_t C)
{
  // Delphi's Trim removes everything up to and including the space character.
  return C != u'\0' && C <= u' ';
}

}  // namespace

UnicodeString::UnicodeString(const wchar_t * S)
{
  if (S != nullptr)
  {
    FData.assign(AsU16(S), WideLen(S));
  }
}

UnicodeString::UnicodeString(const wchar_t * S, int Len)
{
  if (S != nullptr && Len > 0)
  {
    FData.assign(AsU16(S), static_cast<std::size_t>(Len));
  }
}

UnicodeString::UnicodeString(const char * S)
{
  if (S != nullptr)
  {
    *this = linscp::Utf8ToUtf16(S, std::char_traits<char>::length(S));
  }
}

UnicodeString::UnicodeString(wchar_t C, int Count)
{
  if (Count > 0)
  {
    FData.assign(static_cast<std::size_t>(Count), static_cast<char16_t>(C));
  }
}

void UnicodeString::SetLength(int NewLength)
{
  FData.resize(NewLength > 0 ? static_cast<std::size_t>(NewLength) : 0);
}

const wchar_t * UnicodeString::c_str() const
{
  return AsWide(FData.c_str());
}

wchar_t * UnicodeString::c_str()
{
  // Writable, and still null-terminated: std::u16string guarantees a
  // terminator at data()[size()].
  return const_cast<wchar_t *>(AsWide(FData.c_str()));
}

wchar_t * UnicodeString::MutableData()
{
  return const_cast<wchar_t *>(AsWide(FData.data()));
}

wchar_t UnicodeString::operator[](int Index) const
{
  assert(Index >= 1 && Index <= Length());
  return static_cast<wchar_t>(FData[static_cast<std::size_t>(Index - 1)]);
}

wchar_t & UnicodeString::operator[](int Index)
{
  assert(Index >= 1 && Index <= Length());
  return *reinterpret_cast<wchar_t *>(&FData[static_cast<std::size_t>(Index - 1)]);
}

UnicodeString UnicodeString::SubString(int Index, int Count) const
{
  // Delphi's Copy clamps rather than throwing.
  if (Count <= 0 || Index > Length())
  {
    return UnicodeString();
  }
  if (Index < 1)
  {
    Count += Index - 1;      // characters before the start do not count
    Index = 1;
    if (Count <= 0)
    {
      return UnicodeString();
    }
  }
  const std::size_t Start = static_cast<std::size_t>(Index - 1);
  const std::size_t Avail = FData.size() - Start;
  const std::size_t Take = std::min(static_cast<std::size_t>(Count), Avail);
  UnicodeString Result;
  Result.FData = FData.substr(Start, Take);
  return Result;
}

UnicodeString UnicodeString::SubString(int Index) const
{
  return SubString(Index, Length() - Index + 1);
}

int UnicodeString::Pos(const UnicodeString & Substr) const
{
  const std::size_t P = FData.find(Substr.FData);
  return P == std::u16string::npos ? 0 : static_cast<int>(P) + 1;
}

int UnicodeString::Pos(wchar_t C) const
{
  const std::size_t P = FData.find(static_cast<char16_t>(C));
  return P == std::u16string::npos ? 0 : static_cast<int>(P) + 1;
}

int UnicodeString::LastDelimiter(const UnicodeString & Delimiters) const
{
  const std::size_t P = FData.find_last_of(Delimiters.FData);
  return P == std::u16string::npos ? 0 : static_cast<int>(P) + 1;
}

int UnicodeString::FirstDelimiter(const UnicodeString & Delimiters) const
{
  const std::size_t P = FData.find_first_of(Delimiters.FData);
  return P == std::u16string::npos ? 0 : static_cast<int>(P) + 1;
}

bool UnicodeString::IsDelimiter(const UnicodeString & Delimiters, int Index) const
{
  if (Index < 1 || Index > Length())
  {
    return false;
  }
  return Delimiters.FData.find(FData[static_cast<std::size_t>(Index - 1)]) !=
         std::u16string::npos;
}

void UnicodeString::Delete(int Index, int Count)
{
  if (Count <= 0 || Index > Length())
  {
    return;
  }
  if (Index < 1)
  {
    Count += Index - 1;
    Index = 1;
    if (Count <= 0)
    {
      return;
    }
  }
  const std::size_t Start = static_cast<std::size_t>(Index - 1);
  FData.erase(Start, std::min(static_cast<std::size_t>(Count),
                              FData.size() - Start));
}

void UnicodeString::Insert(const UnicodeString & S, int Index)
{
  // Delphi clamps an out-of-range index to the nearest end.
  const int Clamped = std::min(std::max(Index, 1), Length() + 1);
  FData.insert(static_cast<std::size_t>(Clamped - 1), S.FData);
}

UnicodeString UnicodeString::LowerCase() const
{
  UnicodeString Result(*this);
  for (char16_t & C : Result.FData)
  {
    C = FoldAscii(C);
  }
  return Result;
}

UnicodeString UnicodeString::UpperCase() const
{
  UnicodeString Result(*this);
  for (char16_t & C : Result.FData)
  {
    if (C >= u'a' && C <= u'z')
    {
      C = static_cast<char16_t>(C - u'a' + u'A');
    }
  }
  return Result;
}

UnicodeString UnicodeString::TrimLeft() const
{
  std::size_t B = 0;
  while (B < FData.size() && IsSpace(FData[B]))
  {
    ++B;
  }
  return SubString(static_cast<int>(B) + 1);
}

UnicodeString UnicodeString::TrimRight() const
{
  std::size_t E = FData.size();
  while (E > 0 && IsSpace(FData[E - 1]))
  {
    --E;
  }
  return SubString(1, static_cast<int>(E));
}

UnicodeString UnicodeString::Trim() const
{
  return TrimLeft().TrimRight();
}

int UnicodeString::Compare(const UnicodeString & S) const
{
  const int R = FData.compare(S.FData);
  return R < 0 ? -1 : (R > 0 ? 1 : 0);
}

int UnicodeString::CompareIC(const UnicodeString & S) const
{
  const std::size_t N = std::min(FData.size(), S.FData.size());
  for (std::size_t I = 0; I < N; ++I)
  {
    const char16_t A = FoldAscii(FData[I]);
    const char16_t B = FoldAscii(S.FData[I]);
    if (A != B)
    {
      return A < B ? -1 : 1;
    }
  }
  if (FData.size() == S.FData.size())
  {
    return 0;
  }
  return FData.size() < S.FData.size() ? -1 : 1;
}

UnicodeString & UnicodeString::operator+=(const UnicodeString & S)
{
  FData += S.FData;
  return *this;
}

UnicodeString & UnicodeString::operator+=(wchar_t C)
{
  FData += static_cast<char16_t>(C);
  return *this;
}

UnicodeString operator+(const UnicodeString & A, const UnicodeString & B)
{
  UnicodeString Result(A);
  Result += B;
  return Result;
}

UnicodeString operator+(const UnicodeString & A, wchar_t C)
{
  UnicodeString Result(A);
  Result += C;
  return Result;
}

UnicodeString operator+(wchar_t C, const UnicodeString & B)
{
  UnicodeString Result;
  Result += C;
  Result += B;
  return Result;
}
