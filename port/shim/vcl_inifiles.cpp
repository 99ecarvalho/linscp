#include "vcl_inifiles.h"

#include <algorithm>

#include "vcl_bytestring.h"
#include "vcl_streams.h"
#include "vcl_strutils.h"
#include "vcl_sysutils.h"

namespace {

bool SameName(const UnicodeString & A, const UnicodeString & B)
{
  return A.CompareIC(B) == 0;
}

}  // namespace

// --- TCustomIniFile typed accessors ----------------------------------------

int TCustomIniFile::ReadInteger(const UnicodeString & Section,
                                const UnicodeString & Ident, int Default)
{
  return StrToIntDef(ReadString(Section, Ident, UnicodeString()), Default);
}

void TCustomIniFile::WriteInteger(const UnicodeString & Section,
                                  const UnicodeString & Ident, int Value)
{
  WriteString(Section, Ident, IntToStr(Value));
}

__int64 TCustomIniFile::ReadInt64(const UnicodeString & Section,
                                  const UnicodeString & Ident, __int64 Default)
{
  return StrToInt64Def(ReadString(Section, Ident, UnicodeString()), Default);
}

void TCustomIniFile::WriteInt64(const UnicodeString & Section,
                                const UnicodeString & Ident, __int64 Value)
{
  WriteString(Section, Ident, IntToStr(Value));
}

bool TCustomIniFile::ReadBool(const UnicodeString & Section,
                              const UnicodeString & Ident, bool Default)
{
  // Delphi stores booleans as "0" and "1" and reads anything non-zero as true.
  return ReadInteger(Section, Ident, Default ? 1 : 0) != 0;
}

void TCustomIniFile::WriteBool(const UnicodeString & Section,
                               const UnicodeString & Ident, bool Value)
{
  WriteInteger(Section, Ident, Value ? 1 : 0);
}

// --- TMemIniFile -----------------------------------------------------------

TMemIniFile::TMemIniFile(const UnicodeString & AFileName)
  : TCustomIniFile(AFileName)
{
  LoadFromFile();
}

TMemIniFile::TSection * TMemIniFile::FindSection(const UnicodeString & Name)
{
  for (TSection & S : FSections)
  {
    if (SameName(S.Name, Name))
    {
      return &S;
    }
  }
  return nullptr;
}

TMemIniFile::TSection & TMemIniFile::EnsureSection(const UnicodeString & Name)
{
  if (TSection * S = FindSection(Name))
  {
    return *S;
  }
  FSections.push_back(TSection{ Name, {} });
  return FSections.back();
}

TMemIniFile::TPair * TMemIniFile::FindValue(TSection & Section,
                                            const UnicodeString & Ident)
{
  for (TPair & P : Section.Values)
  {
    if (SameName(P.first, Ident))
    {
      return &P;
    }
  }
  return nullptr;
}

void TMemIniFile::LoadFromFile()
{
  FSections.clear();
  if (!FileExists(FFileName))
  {
    return;      // a missing file is an empty one, as in Delphi
  }

  UnicodeString Body;
  try
  {
    TFileStream F(FFileName, fmOpenRead);
    const __int64 Length = F.Size;
    if (Length > 0)
    {
      std::string Bytes(static_cast<std::size_t>(Length), '\0');
      F.ReadBuffer(&Bytes[0], static_cast<int>(Length));
      // Skip a UTF-8 BOM if present: WinSCP writes one on Windows, and a
      // leading U+FEFF would otherwise become part of the first section name.
      std::size_t Start = 0;
      if (Bytes.size() >= 3 &&
          static_cast<unsigned char>(Bytes[0]) == 0xEF &&
          static_cast<unsigned char>(Bytes[1]) == 0xBB &&
          static_cast<unsigned char>(Bytes[2]) == 0xBF)
      {
        Start = 3;
      }
      Body = linscp::Utf8ToUtf16(Bytes.data() + Start, Bytes.size() - Start);
    }
  }
  catch (const EStreamError &)
  {
    return;      // unreadable is treated as empty, not fatal
  }

  TStringList Lines;
  Lines.Text = Body;
  TSection * Current = nullptr;
  for (int I = 0; I < Lines.Count; ++I)
  {
    const UnicodeString Line = Trim(Lines.Strings[I]);
    if (Line.IsEmpty() || Line[1] == L';' || Line[1] == L'#')
    {
      continue;      // blank and comment lines are dropped, as Delphi drops them
    }
    if (Line[1] == L'[' && Line[Line.Length()] == L']')
    {
      Current = &EnsureSection(Line.SubString(2, Line.Length() - 2));
      continue;
    }
    const int Eq = Line.Pos(L'=');
    if (Eq <= 0)
    {
      continue;      // a line with no separator is not a value
    }
    if (Current == nullptr)
    {
      // Values before any section header belong to the unnamed section.
      Current = &EnsureSection(UnicodeString());
    }
    const UnicodeString Key = Trim(Line.SubString(1, Eq - 1));
    const UnicodeString Value = Trim(Line.SubString(Eq + 1));
    if (TPair * P = FindValue(*Current, Key))
    {
      P->second = Value;     // a later duplicate wins
    }
    else
    {
      Current->Values.emplace_back(Key, Value);
    }
  }
}

UnicodeString TMemIniFile::ReadString(const UnicodeString & Section,
                                      const UnicodeString & Ident,
                                      const UnicodeString & Default)
{
  TSection * S = FindSection(Section);
  if (S == nullptr)
  {
    return Default;
  }
  TPair * P = FindValue(*S, Ident);
  return P == nullptr ? Default : P->second;
}

void TMemIniFile::WriteString(const UnicodeString & Section,
                              const UnicodeString & Ident,
                              const UnicodeString & Value)
{
  TSection & S = EnsureSection(Section);
  if (TPair * P = FindValue(S, Ident))
  {
    P->second = Value;
  }
  else
  {
    S.Values.emplace_back(Ident, Value);
  }
}

void TMemIniFile::ReadSection(const UnicodeString & Section, TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  if (TSection * S = FindSection(Section))
  {
    for (const TPair & P : S->Values)
    {
      Strings->Add(P.first);
    }
  }
}

void TMemIniFile::ReadSections(TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  for (const TSection & S : FSections)
  {
    Strings->Add(S.Name);
  }
}

void TMemIniFile::ReadSectionValues(const UnicodeString & Section,
                                    TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  if (TSection * S = FindSection(Section))
  {
    for (const TPair & P : S->Values)
    {
      // "Key=Value" lines, which is what Delphi puts here and what makes
      // TStrings::Values usable on the result.
      Strings->Add(P.first + UnicodeString(L"=") + P.second);
    }
  }
}

void TMemIniFile::EraseSection(const UnicodeString & Section)
{
  FSections.erase(
    std::remove_if(FSections.begin(), FSections.end(),
                   [&Section](const TSection & S)
                   { return SameName(S.Name, Section); }),
    FSections.end());
}

void TMemIniFile::DeleteKey(const UnicodeString & Section,
                            const UnicodeString & Ident)
{
  if (TSection * S = FindSection(Section))
  {
    S->Values.erase(
      std::remove_if(S->Values.begin(), S->Values.end(),
                     [&Ident](const TPair & P)
                     { return SameName(P.first, Ident); }),
      S->Values.end());
  }
}

bool TMemIniFile::SectionExists(const UnicodeString & Section)
{
  return FindSection(Section) != nullptr;
}

bool TMemIniFile::ValueExists(const UnicodeString & Section,
                              const UnicodeString & Ident)
{
  TSection * S = FindSection(Section);
  return S != nullptr && FindValue(*S, Ident) != nullptr;
}

void TMemIniFile::Clear()
{
  FSections.clear();
}

void TMemIniFile::GetStrings(TStrings * Strings)
{
  if (Strings == nullptr)
  {
    return;
  }
  Strings->Clear();
  for (const TSection & S : FSections)
  {
    if (!S.Name.IsEmpty())
    {
      Strings->Add(UnicodeString(L"[") + S.Name + UnicodeString(L"]"));
    }
    for (const TPair & P : S.Values)
    {
      Strings->Add(P.first + UnicodeString(L"=") + P.second);
    }
    Strings->Add(UnicodeString());
  }
}

void TMemIniFile::SetStrings(TStrings * Strings)
{
  Clear();
  if (Strings == nullptr)
  {
    return;
  }
  TSection * Current = nullptr;
  for (int I = 0; I < Strings->Count; ++I)
  {
    const UnicodeString Line = Trim(Strings->Strings[I]);
    if (Line.IsEmpty() || Line[1] == L';')
    {
      continue;
    }
    if (Line[1] == L'[' && Line[Line.Length()] == L']')
    {
      Current = &EnsureSection(Line.SubString(2, Line.Length() - 2));
      continue;
    }
    const int Eq = Line.Pos(L'=');
    if (Eq > 0)
    {
      if (Current == nullptr)
      {
        Current = &EnsureSection(UnicodeString());
      }
      const UnicodeString Key = Trim(Line.SubString(1, Eq - 1));
      if (TPair * P = FindValue(*Current, Key))
      {
        P->second = Trim(Line.SubString(Eq + 1));
      }
      else
      {
        Current->Values.emplace_back(Key, Trim(Line.SubString(Eq + 1)));
      }
    }
  }
}

void TMemIniFile::UpdateFile()
{
  TStringList Lines;
  GetStrings(&Lines);
  const UTF8String Body(Lines.Text);
  TFileStream F(FFileName, fmCreate);
  if (Body.Length() > 0)
  {
    F.WriteBuffer(Body.c_str(), Body.Length());
  }
}

void TMemIniFile::Rename(const UnicodeString & AFileName, bool Reload)
{
  FFileName = AFileName;
  if (Reload)
  {
    LoadFromFile();
  }
}
