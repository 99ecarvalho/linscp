#include "vcl_strings.h"

#include <algorithm>
#include <fstream>

#include "vcl_bytestring.h"

namespace {

const wchar_t * const LineBreak = L"\r\n";

bool NeedsQuoting(const UnicodeString & S, wchar_t Delimiter, wchar_t QuoteChar)
{
  if (S.IsEmpty())
  {
    return true;
  }
  for (int I = 1; I <= S.Length(); ++I)
  {
    const wchar_t C = S[I];
    if (C == Delimiter || C == QuoteChar || C <= L' ')
    {
      return true;
    }
  }
  return false;
}

}  // namespace

// --- TStrings --------------------------------------------------------------

void TStrings::CheckIndex(int Index) const
{
  if (Index < 0 || Index >= GetCount())
  {
    throw EStringListError(UnicodeString(L"List index out of bounds"));
  }
}

bool TStrings::SameString(const UnicodeString & A, const UnicodeString & B) const
{
  return FCaseSensitive ? (A == B) : (A.CompareIC(B) == 0);
}

int TStrings::Add(const UnicodeString & S)
{
  const int Index = GetCount();
  Insert(Index, S);
  return Index;
}

int TStrings::AddObject(const UnicodeString & S, TObject * AObject)
{
  const int Index = Add(S);
  PutObject(Index, AObject);
  return Index;
}

void TStrings::InsertObject(int Index, const UnicodeString & S, TObject * AObject)
{
  Insert(Index, S);
  PutObject(Index, AObject);
}

void TStrings::AddStrings(const TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  for (int I = 0; I < Strings->GetCount(); ++I)
  {
    AddObject(Strings->GetString(I), Strings->GetObject(I));
  }
}

void TStrings::AddPair(const UnicodeString & Name, const UnicodeString & Value)
{
  Add(Name + UnicodeString(&FNameValueSeparator, 1) + Value);
}

int TStrings::IndexOf(const UnicodeString & S) const
{
  for (int I = 0; I < GetCount(); ++I)
  {
    if (SameString(GetString(I), S))
    {
      return I;
    }
  }
  return -1;
}

int TStrings::IndexOfName(const UnicodeString & Name) const
{
  for (int I = 0; I < GetCount(); ++I)
  {
    if (SameString(GetName(I), Name))
    {
      return I;
    }
  }
  return -1;
}

int TStrings::IndexOfObject(TObject * AObject) const
{
  for (int I = 0; I < GetCount(); ++I)
  {
    if (GetObject(I) == AObject)
    {
      return I;
    }
  }
  return -1;
}

UnicodeString TStrings::GetName(int Index) const
{
  const UnicodeString S = GetString(Index);
  const int P = S.Pos(FNameValueSeparator);
  return P > 0 ? S.SubString(1, P - 1) : UnicodeString();
}

UnicodeString TStrings::GetValueFromIndex(int Index) const
{
  const UnicodeString S = GetString(Index);
  const int P = S.Pos(FNameValueSeparator);
  // Delphi returns the whole string when there is no separator at all, but an
  // empty value when the separator is present with nothing after it.
  return P > 0 ? S.SubString(P + 1) : UnicodeString();
}

UnicodeString TStrings::GetValue(const UnicodeString & Name) const
{
  const int I = IndexOfName(Name);
  return I >= 0 ? GetValueFromIndex(I) : UnicodeString();
}

void TStrings::SetValue(const UnicodeString & Name, UnicodeString Value)
{
  const int I = IndexOfName(Name);
  if (Value.IsEmpty())
  {
    // Delphi removes the entry when the value is cleared.
    if (I >= 0)
    {
      Delete(I);
    }
    return;
  }
  const UnicodeString Line =
    Name + UnicodeString(&FNameValueSeparator, 1) + Value;
  if (I >= 0)
  {
    PutString(I, Line);
  }
  else
  {
    Add(Line);
  }
}

UnicodeString TStrings::GetText() const
{
  UnicodeString Out;
  for (int I = 0; I < GetCount(); ++I)
  {
    Out += GetString(I);
    Out += UnicodeString(LineBreak);   // Delphi appends a break after every line
  }
  return Out;
}

void TStrings::SetText(UnicodeString Value)
{
  Clear();
  const int N = Value.Length();
  int Start = 1;
  int I = 1;
  while (I <= N)
  {
    const wchar_t C = Value[I];
    if (C == L'\r' || C == L'\n')
    {
      Add(Value.SubString(Start, I - Start));
      // Treat CRLF as one break, not two.
      if (C == L'\r' && I < N && Value[I + 1] == L'\n')
      {
        ++I;
      }
      Start = I + 1;
    }
    ++I;
  }
  if (Start <= N)
  {
    Add(Value.SubString(Start, N - Start + 1));
  }
}

UnicodeString TStrings::GetDelimitedText() const
{
  UnicodeString Out;
  for (int I = 0; I < GetCount(); ++I)
  {
    if (I > 0)
    {
      Out += FDelimiter;
    }
    const UnicodeString S = GetString(I);
    if (NeedsQuoting(S, FDelimiter, FQuoteChar))
    {
      Out += FQuoteChar;
      for (int K = 1; K <= S.Length(); ++K)
      {
        if (S[K] == FQuoteChar)
        {
          Out += FQuoteChar;        // embedded quotes are doubled
        }
        Out += S[K];
      }
      Out += FQuoteChar;
    }
    else
    {
      Out += S;
    }
  }
  return Out;
}

void TStrings::SetDelimitedText(UnicodeString Value)
{
  Clear();
  const int N = Value.Length();
  int I = 1;
  while (I <= N)
  {
    // Delphi skips whitespace before an item.
    while (I <= N && Value[I] == L' ')
    {
      ++I;
    }
    UnicodeString Item;
    if (I <= N && Value[I] == FQuoteChar)
    {
      ++I;
      while (I <= N)
      {
        if (Value[I] == FQuoteChar)
        {
          if (I < N && Value[I + 1] == FQuoteChar)
          {
            Item += FQuoteChar;     // a doubled quote is one literal quote
            I += 2;
            continue;
          }
          ++I;
          break;
        }
        Item += Value[I];
        ++I;
      }
    }
    else
    {
      while (I <= N && Value[I] != FDelimiter)
      {
        Item += Value[I];
        ++I;
      }
    }
    Add(Item);
    while (I <= N && Value[I] != FDelimiter)
    {
      ++I;
    }
    if (I <= N && Value[I] == FDelimiter)
    {
      ++I;
      if (I > N)
      {
        Add(UnicodeString());       // a trailing delimiter means a final empty item
      }
    }
  }
}

UnicodeString TStrings::GetCommaText() const
{
  // CommaText is DelimitedText pinned to ',' and '"' regardless of the
  // Delimiter/QuoteChar currently set.
  TStringList Tmp;
  for (int I = 0; I < GetCount(); ++I)
  {
    Tmp.Add(GetString(I));
  }
  Tmp.Delimiter = L',';
  Tmp.QuoteChar = L'"';
  return Tmp.GetDelimitedText();
}

void TStrings::SetCommaText(UnicodeString Value)
{
  const wchar_t SaveD = FDelimiter;
  const wchar_t SaveQ = FQuoteChar;
  FDelimiter = L',';
  FQuoteChar = L'"';
  SetDelimitedText(Value);
  FDelimiter = SaveD;
  FQuoteChar = SaveQ;
}

void TStrings::Assign(TPersistent * Source)
{
  auto * S = dynamic_cast<TStrings *>(Source);
  if (S == nullptr || S == this)
  {
    return;
  }
  Clear();
  FDelimiter = S->FDelimiter;
  FQuoteChar = S->FQuoteChar;
  FNameValueSeparator = S->FNameValueSeparator;
  FCaseSensitive = S->FCaseSensitive;
  AddStrings(S);
}

void TStrings::Move(int CurIndex, int NewIndex)
{
  if (CurIndex == NewIndex)
  {
    return;
  }
  CheckIndex(CurIndex);
  CheckIndex(NewIndex);
  const UnicodeString S = GetString(CurIndex);
  TObject * O = GetObject(CurIndex);
  Delete(CurIndex);
  Insert(NewIndex, S);
  PutObject(NewIndex, O);
}

void TStrings::Exchange(int Index1, int Index2)
{
  CheckIndex(Index1);
  CheckIndex(Index2);
  const UnicodeString S = GetString(Index1);
  TObject * O = GetObject(Index1);
  PutString(Index1, GetString(Index2));
  PutObject(Index1, GetObject(Index2));
  PutString(Index2, S);
  PutObject(Index2, O);
}

bool TStrings::Equals(const TStrings * Strings) const
{
  if (Strings == nullptr || Strings->GetCount() != GetCount())
  {
    return false;
  }
  for (int I = 0; I < GetCount(); ++I)
  {
    if (GetString(I) != Strings->GetString(I))
    {
      return false;
    }
  }
  return true;
}

void TStrings::SaveToFile(const UnicodeString & FileName) const
{
  const UTF8String Path(FileName);
  std::ofstream F(Path.c_str(), std::ios::binary);
  if (!F)
  {
    throw EFilerError(UnicodeString(L"Cannot create ") + FileName);
  }
  const UTF8String Body(GetText());
  F.write(Body.c_str(), Body.Length());
}

void TStrings::LoadFromFile(const UnicodeString & FileName)
{
  const UTF8String Path(FileName);
  std::ifstream F(Path.c_str(), std::ios::binary);
  if (!F)
  {
    throw EFilerError(UnicodeString(L"Cannot open ") + FileName);
  }
  std::string Body((std::istreambuf_iterator<char>(F)),
                   std::istreambuf_iterator<char>());
  SetText(linscp::Utf8ToUtf16(Body.data(), Body.size()));
}

// --- TStringList -----------------------------------------------------------

UnicodeString TStringList::GetString(int Index) const
{
  CheckIndex(Index);
  return FItems[static_cast<std::size_t>(Index)].Text;
}

void TStringList::PutString(int Index, UnicodeString Value)
{
  CheckIndex(Index);
  FItems[static_cast<std::size_t>(Index)].Text = Value;
}

TObject * TStringList::GetObject(int Index) const
{
  CheckIndex(Index);
  return FItems[static_cast<std::size_t>(Index)].Object;
}

void TStringList::PutObject(int Index, TObject * Value)
{
  CheckIndex(Index);
  FItems[static_cast<std::size_t>(Index)].Object = Value;
}

void TStringList::Delete(int Index)
{
  CheckIndex(Index);
  FItems.erase(FItems.begin() + Index);
}

void TStringList::Insert(int Index, const UnicodeString & S)
{
  if (Index < 0 || Index > GetCount())
  {
    throw EStringListError(UnicodeString(L"List index out of bounds"));
  }
  FItems.insert(FItems.begin() + Index, TItem{ S, nullptr });
}

int TStringList::Add(const UnicodeString & S)
{
  if (!FSorted)
  {
    const int Index = GetCount();
    Insert(Index, S);
    return Index;
  }

  // Sorted: find the insertion point, and apply Duplicates there.
  int Lo = 0;
  int Hi = GetCount() - 1;
  int Found = -1;
  while (Lo <= Hi)
  {
    const int Mid = Lo + (Hi - Lo) / 2;
    const int C = FCaseSensitive ? FItems[Mid].Text.Compare(S)
                                 : FItems[Mid].Text.CompareIC(S);
    if (C < 0)
    {
      Lo = Mid + 1;
    }
    else
    {
      if (C == 0)
      {
        Found = Mid;
      }
      Hi = Mid - 1;
    }
  }
  if (Found >= 0)
  {
    if (FDuplicates == dupIgnore)
    {
      return Found;
    }
    if (FDuplicates == dupError)
    {
      throw EStringListError(UnicodeString(L"String list does not allow duplicates"));
    }
  }
  Insert(Lo, S);
  return Lo;
}

int TStringList::IndexOf(const UnicodeString & S) const
{
  if (!FSorted)
  {
    return TStrings::IndexOf(S);
  }
  int Lo = 0;
  int Hi = GetCount() - 1;
  while (Lo <= Hi)
  {
    const int Mid = Lo + (Hi - Lo) / 2;
    const int C = FCaseSensitive ? FItems[Mid].Text.Compare(S)
                                 : FItems[Mid].Text.CompareIC(S);
    if (C == 0)
    {
      return Mid;
    }
    if (C < 0)
    {
      Lo = Mid + 1;
    }
    else
    {
      Hi = Mid - 1;
    }
  }
  return -1;
}

void TStringList::Sort()
{
  const bool CS = FCaseSensitive;
  std::stable_sort(FItems.begin(), FItems.end(),
    [CS](const TItem & A, const TItem & B)
    {
      return (CS ? A.Text.Compare(B.Text) : A.Text.CompareIC(B.Text)) < 0;
    });
}

void TStringList::SetSorted(bool Value)
{
  if (FSorted != Value)
  {
    if (Value)
    {
      Sort();
    }
    FSorted = Value;
  }
}
