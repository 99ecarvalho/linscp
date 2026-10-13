#include <vcl.h>
#pragma hdrstop

#include "GuiShim.h"

#include <Common.h>

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <shlobj.h>
#endif

namespace
{

struct TLocaleRow
{
  LCID Locale;
  const wchar_t * Abbreviation;
  const wchar_t * Name;
  const wchar_t * EnglishLanguage;
  const wchar_t * Language;
  const wchar_t * NativeLanguage;
};

const TLocaleRow LocaleRows[] =
{
#include "../locales.inc"
};
const int LocaleRowCount = static_cast<int>(sizeof(LocaleRows) / sizeof(LocaleRows[0]));

const TLocaleRow * FindLocale(LCID Locale)
{
  for (const TLocaleRow & Row : LocaleRows)
  {
    if (Row.Locale == Locale)
    {
      return &Row;
    }
  }
  return nullptr;
}

TScreen GScreen;
TLanguages GLanguages;

}  // namespace

TScreen * Screen = &GScreen;

UnicodeString GLastTranslationFile;

UnicodeString LinscpLastTranslationFile()
{
  return GLastTranslationFile;
}

void * LinscpLoadDataFile(const wchar_t * AFileName)
{
  const UnicodeString FileName(AFileName);
  void * Result = LoadTranslationModule(FileName);
  if (Result != nullptr)
  {
    GLastTranslationFile = FileName;
  }
  return Result;
}

LCID LinscpLocaleOfName(const UnicodeString & AName)
{
  UnicodeString Name = AName;
  Name = CutToChar(Name, L'.', false);
  Name = CutToChar(Name, L'@', false);
  Name = ReplaceStr(Name, L"_", L"-");
  const UnicodeString Language = CopyToChar(Name, L'-', false);
  for (const TLocaleRow & Row : LocaleRows)
  {
    if (SameText(Row.Name, Name))
    {
      return Row.Locale;
    }
  }
  for (const TLocaleRow & Row : LocaleRows)
  {
    if (SameText(CopyToChar(Row.Name, L'-', false), Language))
    {
      return MAKELANGID(PRIMARYLANGID(Row.Locale), SUBLANG_DEFAULT);
    }
  }
  return 0;
}

UnicodeString LinscpLocaleName(LCID Locale)
{
  const TLocaleRow * Row = FindLocale(Locale);
  return (Row != nullptr) ? UnicodeString(Row->Name) : UnicodeString();
}

void LinscpFreeModule(void * Module)
{
  FreeTranslationModule(Module);
}

int TLanguages::GetCount() const
{
  return LocaleRowCount;
}

LCID TLanguages::GetLocaleID(int Index) const
{
  return LocaleRows[Index].Locale;
}

UnicodeString TLanguages::GetExt(int Index) const
{
  return LocaleRows[Index].Abbreviation;
}

UnicodeString TLanguages::GetLocaleName(int Index) const
{
  return LocaleRows[Index].Name;
}

TLanguages * Languages()
{
  return &GLanguages;
}

#ifndef _WIN32

int GetLocaleInfo(LCID Locale, DWORD Type, wchar_t * Data, int Size)
{
  const TLocaleRow * Row = FindLocale(Locale);
  const wchar_t * Value = nullptr;
  if (Row != nullptr)
  {
    switch (Type)
    {
      case LOCALE_SABBREVLANGNAME: Value = Row->Abbreviation; break;
      case LOCALE_SENGLANGUAGE: Value = Row->EnglishLanguage; break;
      case LOCALE_SLANGUAGE: Value = Row->Language; break;
      case LOCALE_SNATIVELANGNAME: Value = Row->NativeLanguage; break;
    }
  }
  if ((Value == nullptr) || (Size <= 0))
  {
    return 0;
  }
  int Length = 0;
  while (Value[Length] != L'\0')
  {
    Length++;
  }
  if (Length + 1 > Size)
  {
    return 0;
  }
  for (int Index = 0; Index <= Length; Index++)
  {
    Data[Index] = Value[Index];
  }
  return Length + 1;
}

// As setlocale() reads it: "pt_BR.UTF-8" is pt-BR; "pt" is the language's
// default (Windows' SUBLANG_DEFAULT one).
LANGID GetUserDefaultUILanguage()
{
  const char * Variable = nullptr;
  for (const char * Name : { "LC_ALL", "LC_MESSAGES", "LANG" })
  {
    Variable = std::getenv(Name);
    if ((Variable != nullptr) && (*Variable != '\0'))
    {
      break;
    }
  }
  const LANGID Default = MAKELANGID(0x09, SUBLANG_DEFAULT);
  const LCID Locale = (Variable != nullptr) ? LinscpLocaleOfName(UnicodeString(Variable)) : 0;
  return (Locale != 0) ? static_cast<LANGID>(Locale) : Default;
}

bool KnownFolderPath(TKnownFolder /*FolderID*/, UnicodeString & Path)
{
  Path = UnicodeString();
  return false;
}

#else

// As WinSCP's (windows/GUITools.cpp).
bool KnownFolderPath(REFKNOWNFOLDERID FolderID, UnicodeString & Path)
{
  PWSTR Buffer = nullptr;
  bool Result = SUCCEEDED(SHGetKnownFolderPath(FolderID, 0, nullptr, &Buffer));
  if (Result)
  {
    Path = Buffer;
    CoTaskMemFree(Buffer);
  }
  return Result;
}

#endif
