//---------------------------------------------------------------------------
// CString for linscp's MFC-compatible layer (see include/afx.h).
//
// Original code, written for linscp (GPL-3.0-or-later). Works on the
// UnicodeString's UTF-16 buffer directly with 0-based indexes; no C runtime
// wide function is called (ADR 0001).
//---------------------------------------------------------------------------
#include "afx.h"

#include <vector>

#include "vcl.h"

namespace {

int LengthOf(const wchar_t * S)
{
  int N = 0;
  if (S != nullptr)
  {
    while (S[N] != L'\0')
    {
      ++N;
    }
  }
  return N;
}

wchar_t FoldLower(wchar_t C)
{
  return ((C >= L'A') && (C <= L'Z')) ? static_cast<wchar_t>(C - L'A' + L'a') : C;
}

wchar_t FoldUpper(wchar_t C)
{
  return ((C >= L'a') && (C <= L'z')) ? static_cast<wchar_t>(C - L'a' + L'A') : C;
}

bool IsCWhiteSpace(wchar_t C)
{
  return (C == L' ') || (C == L'\t') || (C == L'\n') || (C == L'\v') || (C == L'\f') || (C == L'\r');
}

bool InSet(const wchar_t * Set, wchar_t C)
{
  for (const wchar_t * P = Set; *P != L'\0'; ++P)
  {
    if (*P == C)
    {
      return true;
    }
  }
  return false;
}

// Both strings end at their first null; null pointers are empty. Fold: ASCII
// A-Z compare as a-z.
int CompareCodeUnits(const wchar_t * A, const wchar_t * B, bool Fold)
{
  static const wchar_t EmptyText[] = L"";
  const wchar_t * X = (A != nullptr) ? A : EmptyText;
  const wchar_t * Y = (B != nullptr) ? B : EmptyText;
  for (;; ++X, ++Y)
  {
    const wchar_t CX = Fold ? FoldLower(*X) : *X;
    const wchar_t CY = Fold ? FoldLower(*Y) : *Y;
    if (CX != CY)
    {
      const int Difference = static_cast<int>(static_cast<unsigned>(CX)) - static_cast<int>(static_cast<unsigned>(CY));
      // Compare: the sign alone; CompareNoCase: the difference itself.
      return Fold ? Difference : ((Difference < 0) ? -1 : 1);
    }
    if (CX == L'\0')
    {
      return 0;
    }
  }
}

// Each byte becomes the code point of the same value.
UnicodeString FromLatin1(const char * Src, int Length)
{
  if ((Src == nullptr) || (Length <= 0))
  {
    return UnicodeString();
  }
  UnicodeString Result;
  Result.SetLength(Length);
  wchar_t * D = Result.MutableData();
  for (int I = 0; I < Length; ++I)
  {
    D[I] = static_cast<wchar_t>(static_cast<unsigned char>(Src[I]));
  }
  return Result;
}

// As UnicodeString decodes a const char *; null is empty.
UnicodeString FromNarrow(const char * Src)
{
  return (Src == nullptr) ? UnicodeString() : UnicodeString(Src);
}

// The length of S as a null-terminated string, from Start.
int TextEnd(const wchar_t * S, int Start, int Length)
{
  int I = Start;
  while ((I < Length) && (S[I] != L'\0'))
  {
    ++I;
  }
  return I;
}

[[noreturn]] void RaiseIndex(int Index, int Length)
{
  throw EListError(UnicodeString(L"CString index ") + UnicodeString(Index) +
                   UnicodeString(L" out of range [0, ") + UnicodeString(Length) + UnicodeString(L"]"));
}

}  // namespace

//---------------------------------------------------------------------------
void CString::Assign(const wchar_t * Src, int Length)
{
  FData = UnicodeString(Src, Length);
}

CString::CString(const wchar_t * Src) : FData(Src, LengthOf(Src))
{
}

CString::CString(const wchar_t * Src, int Length) : FData(Src, Length)
{
}

CString::CString(const char * Src) : FData(FromNarrow(Src))
{
}

CString::CString(const char * Src, int Length) : FData(FromLatin1(Src, Length))
{
}

CString::CString(const UnicodeString & Src) : FData(Src)
{
}

CString & CString::operator=(const wchar_t * Src)
{
  // Src may point into this string's own buffer.
  UnicodeString Copy(Src, LengthOf(Src));
  FData = Copy;
  return *this;
}

CString & CString::operator=(const char * Src)
{
  FData = FromNarrow(Src);
  return *this;
}

CString & CString::operator=(wchar_t Ch)
{
  Assign(&Ch, 1);
  return *this;
}

//---------------------------------------------------------------------------
wchar_t CString::GetAt(int Index) const
{
  if ((Index < 0) || (Index > GetLength()))
  {
    RaiseIndex(Index, GetLength());
  }
  return FData.c_str()[Index];
}

void CString::SetAt(int Index, wchar_t Ch)
{
  if ((Index < 0) || (Index >= GetLength()))
  {
    RaiseIndex(Index, GetLength() - 1);
  }
  FData.MutableData()[Index] = Ch;
}

//---------------------------------------------------------------------------
CString & CString::operator+=(const CString & Src)
{
  FData += Src.FData;
  return *this;
}

CString & CString::operator+=(const wchar_t * Src)
{
  FData += UnicodeString(Src, LengthOf(Src));
  return *this;
}

CString & CString::operator+=(wchar_t Ch)
{
  FData += UnicodeString(&Ch, 1);
  return *this;
}

CString operator+(const CString & A, const CString & B)
{
  CString R(A);
  R += B;
  return R;
}

CString operator+(const CString & A, const wchar_t * B)
{
  CString R(A);
  R += B;
  return R;
}

CString operator+(const wchar_t * A, const CString & B)
{
  CString R(A);
  R += B;
  return R;
}

CString operator+(const CString & A, wchar_t B)
{
  CString R(A);
  R += B;
  return R;
}

CString operator+(wchar_t A, const CString & B)
{
  CString R(&A, 1);
  R += B;
  return R;
}

//---------------------------------------------------------------------------
int CString::Compare(const wchar_t * Other) const
{
  return CompareCodeUnits(GetString(), Other, false);
}

int CString::CompareNoCase(const wchar_t * Other) const
{
  return CompareCodeUnits(GetString(), Other, true);
}

bool operator==(const CString & A, const CString & B) { return A.Compare(B) == 0; }
bool operator==(const CString & A, const wchar_t * B) { return A.Compare(B) == 0; }
bool operator==(const wchar_t * A, const CString & B) { return B.Compare(A) == 0; }
bool operator!=(const CString & A, const CString & B) { return A.Compare(B) != 0; }
bool operator!=(const CString & A, const wchar_t * B) { return A.Compare(B) != 0; }
bool operator!=(const wchar_t * A, const CString & B) { return B.Compare(A) != 0; }
bool operator<(const CString & A, const CString & B) { return A.Compare(B) < 0; }
bool operator<(const CString & A, const wchar_t * B) { return A.Compare(B) < 0; }
bool operator<(const wchar_t * A, const CString & B) { return B.Compare(A) > 0; }
bool operator>(const CString & A, const CString & B) { return A.Compare(B) > 0; }
bool operator>(const CString & A, const wchar_t * B) { return A.Compare(B) > 0; }
bool operator>(const wchar_t * A, const CString & B) { return B.Compare(A) < 0; }
bool operator<=(const CString & A, const CString & B) { return A.Compare(B) <= 0; }
bool operator<=(const CString & A, const wchar_t * B) { return A.Compare(B) <= 0; }
bool operator<=(const wchar_t * A, const CString & B) { return B.Compare(A) >= 0; }
bool operator>=(const CString & A, const CString & B) { return A.Compare(B) >= 0; }
bool operator>=(const CString & A, const wchar_t * B) { return A.Compare(B) >= 0; }
bool operator>=(const wchar_t * A, const CString & B) { return B.Compare(A) <= 0; }

//---------------------------------------------------------------------------
CString CString::Mid(int First) const
{
  return Mid(First, GetLength());
}

CString CString::Mid(int First, int Count) const
{
  const int Length = GetLength();
  if (First < 0)
  {
    First = 0;
  }
  if ((Count <= 0) || (First >= Length))
  {
    return CString();
  }
  if (Count > Length - First)
  {
    Count = Length - First;
  }
  return CString(GetString() + First, Count);
}

CString CString::Left(int Count) const
{
  return Mid(0, Count);
}

CString CString::Right(int Count) const
{
  const int Length = GetLength();
  if (Count <= 0)
  {
    return CString();
  }
  if (Count > Length)
  {
    Count = Length;
  }
  return CString(GetString() + (Length - Count), Count);
}

//---------------------------------------------------------------------------
int CString::Find(wchar_t Ch, int Start) const
{
  const int Length = GetLength();
  if (Start < 0)
  {
    Start = 0;
  }
  if (Start >= Length)
  {
    return -1;
  }
  const wchar_t * S = GetString();
  const int End = TextEnd(S, Start, Length);
  if (Ch == L'\0')
  {
    return End;
  }
  for (int I = Start; I < End; ++I)
  {
    if (S[I] == Ch)
    {
      return I;
    }
  }
  return -1;
}

int CString::Find(const wchar_t * Sub, int Start) const
{
  if (Sub == nullptr)
  {
    return -1;
  }
  const int Length = GetLength();
  if (Start < 0)
  {
    Start = 0;
  }
  if (Start > Length)
  {
    return -1;
  }
  const int SubLength = LengthOf(Sub);
  const wchar_t * S = GetString();
  const int End = TextEnd(S, Start, Length);
  for (int I = Start; I + SubLength <= End; ++I)
  {
    int J = 0;
    while ((J < SubLength) && (S[I + J] == Sub[J]))
    {
      ++J;
    }
    if (J == SubLength)
    {
      return I;
    }
  }
  return -1;
}

int CString::ReverseFind(wchar_t Ch) const
{
  const wchar_t * S = GetString();
  const int End = TextEnd(S, 0, GetLength());
  if (Ch == L'\0')
  {
    return End;
  }
  for (int I = End - 1; I >= 0; --I)
  {
    if (S[I] == Ch)
    {
      return I;
    }
  }
  return -1;
}

int CString::FindOneOf(const wchar_t * CharSet) const
{
  if (CharSet == nullptr)
  {
    return -1;
  }
  const wchar_t * S = GetString();
  const int End = TextEnd(S, 0, GetLength());
  for (int I = 0; I < End; ++I)
  {
    if (InSet(CharSet, S[I]))
    {
      return I;
    }
  }
  return -1;
}

//---------------------------------------------------------------------------
int CString::Replace(const wchar_t * Old, const wchar_t * New)
{
  const int OldLength = LengthOf(Old);
  if (OldLength == 0)
  {
    return 0;
  }
  // Old and New may point into this string's own buffer: copy them first.
  const UnicodeString OldText(Old, OldLength);
  const UnicodeString NewText(New, LengthOf(New));
  const wchar_t * S = GetString();
  const wchar_t * O = OldText.c_str();
  const int Length = GetLength();
  UnicodeString Result;
  int Count = 0;
  int Copied = 0;
  int I = 0;
  while (I + OldLength <= Length)
  {
    int J = 0;
    while ((J < OldLength) && (S[I + J] == O[J]))
    {
      ++J;
    }
    if (J == OldLength)
    {
      Result += UnicodeString(S + Copied, I - Copied);
      Result += NewText;
      I += OldLength;
      Copied = I;
      ++Count;
    }
    else
    {
      ++I;
    }
  }
  if (Count == 0)
  {
    return 0;
  }
  Result += UnicodeString(S + Copied, Length - Copied);
  const bool Changed = (Result != FData);
  FData = Result;
  return Changed ? 1 : 0;
}

int CString::Replace(wchar_t Old, wchar_t New)
{
  int Count = 0;
  const int Length = GetLength();
  if ((Length == 0) || (Old == New))
  {
    return 0;
  }
  wchar_t * S = FData.MutableData();
  for (int I = 0; I < Length; ++I)
  {
    if (S[I] == Old)
    {
      S[I] = New;
      ++Count;
    }
  }
  return Count;
}

int CString::Delete(int Index, int Count)
{
  const int Length = GetLength();
  if (Index < 0)
  {
    Index = 0;
  }
  if ((Count > 0) && (Index < Length))
  {
    if (Count > Length - Index)
    {
      Count = Length - Index;
    }
    // UnicodeString's Delete is 1-based.
    FData.Delete(Index + 1, Count);
  }
  return Length;
}

int CString::Insert(int Index, wchar_t Ch)
{
  return Insert(Index, UnicodeString(&Ch, 1).c_str());
}

int CString::Insert(int Index, const wchar_t * Src)
{
  const int Length = GetLength();
  if (Index < 0)
  {
    Index = 0;
  }
  if (Index > Length)
  {
    Index = Length;
  }
  const UnicodeString Text(Src, LengthOf(Src));
  UnicodeString Result(GetString(), Index);
  Result += Text;
  Result += UnicodeString(GetString() + Index, Length - Index);
  FData = Result;
  return GetLength();
}

CString & CString::MakeLower()
{
  const int Length = GetLength();
  if (Length > 0)
  {
    wchar_t * S = FData.MutableData();
    for (int I = 0; I < Length; ++I)
    {
      S[I] = FoldLower(S[I]);
    }
  }
  return *this;
}

CString & CString::MakeUpper()
{
  const int Length = GetLength();
  if (Length > 0)
  {
    wchar_t * S = FData.MutableData();
    for (int I = 0; I < Length; ++I)
    {
      S[I] = FoldUpper(S[I]);
    }
  }
  return *this;
}

//---------------------------------------------------------------------------
// Set == nullptr: trim Single, or white space when Single is 0.
void CString::TrimLeftWhere(const wchar_t * Set, wchar_t Single)
{
  const int Length = GetLength();
  const wchar_t * S = GetString();
  int I = 0;
  while ((I < Length) &&
         ((Set != nullptr) ? InSet(Set, S[I]) : ((Single != L'\0') ? (S[I] == Single) : IsCWhiteSpace(S[I]))))
  {
    ++I;
  }
  if (I > 0)
  {
    FData = UnicodeString(S + I, Length - I);
  }
}

void CString::TrimRightWhere(const wchar_t * Set, wchar_t Single)
{
  const int Length = GetLength();
  const wchar_t * S = GetString();
  int N = Length;
  while ((N > 0) &&
         ((Set != nullptr) ? InSet(Set, S[N - 1]) : ((Single != L'\0') ? (S[N - 1] == Single) : IsCWhiteSpace(S[N - 1]))))
  {
    --N;
  }
  if (N < Length)
  {
    FData = UnicodeString(S, N);
  }
}

CString & CString::TrimLeft()
{
  TrimLeftWhere(nullptr, L'\0');
  return *this;
}

CString & CString::TrimLeft(wchar_t Target)
{
  if (Target != L'\0')
  {
    TrimLeftWhere(nullptr, Target);
  }
  return *this;
}

CString & CString::TrimLeft(const wchar_t * Targets)
{
  if (Targets != nullptr)
  {
    const UnicodeString Set(Targets, LengthOf(Targets));
    TrimLeftWhere(Set.c_str(), L'\0');
  }
  return *this;
}

CString & CString::TrimRight()
{
  TrimRightWhere(nullptr, L'\0');
  return *this;
}

CString & CString::TrimRight(wchar_t Target)
{
  if (Target != L'\0')
  {
    TrimRightWhere(nullptr, Target);
  }
  return *this;
}

CString & CString::TrimRight(const wchar_t * Targets)
{
  if (Targets != nullptr)
  {
    const UnicodeString Set(Targets, LengthOf(Targets));
    TrimRightWhere(Set.c_str(), L'\0');
  }
  return *this;
}

CString & CString::Trim()
{
  return TrimRight().TrimLeft();
}

CString & CString::Trim(wchar_t Target)
{
  return TrimRight(Target).TrimLeft(Target);
}

CString & CString::Trim(const wchar_t * Targets)
{
  return TrimRight(Targets).TrimLeft(Targets);
}

//---------------------------------------------------------------------------
void CString::FormatV(const wchar_t * Format, va_list Args)
{
  UnicodeString Result;
  if (Format != nullptr)
  {
    va_list Copy;
    va_copy(Copy, Args);
    Result.vprintf(Format, Copy);
    va_end(Copy);
  }
  // Formatted apart and then assigned, so an argument may be this string.
  FData = Result;
}

void CString::Format(const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  FormatV(Format, Args);
  va_end(Args);
}

void CString::Format(unsigned int FormatId, ...)
{
  const UnicodeString Text = LoadStr(FormatId);
  va_list Args;
  va_start(Args, FormatId);
  FormatV(Text.c_str(), Args);
  va_end(Args);
}
