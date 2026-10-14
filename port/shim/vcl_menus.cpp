#include "vcl.h"

#include "vcl_menus.h"

namespace
{

// Vcl.Menus' key names (its MenuKeyCaps, in English).
struct TKeyName
{
  Word Key;
  const wchar_t * Name;
};

const TKeyName KeyNames[] = {
  { 0x08, L"BkSp" }, { 0x09, L"Tab" }, { 0x0D, L"Enter" }, { 0x1B, L"Esc" }, { 0x20, L"Space" },
  { 0x21, L"PgUp" }, { 0x22, L"PgDn" }, { 0x23, L"End" }, { 0x24, L"Home" },
  { 0x25, L"Left" }, { 0x26, L"Up" }, { 0x27, L"Right" }, { 0x28, L"Down" },
  { 0x2D, L"Ins" }, { 0x2E, L"Del" },
};

UnicodeString KeyName(Word Key)
{
  if (((Key >= L'0') && (Key <= L'9')) || ((Key >= L'A') && (Key <= L'Z')))
  {
    return UnicodeString(static_cast<wchar_t>(Key));
  }
  if ((Key >= 0x60) && (Key <= 0x69))
  {
    return UnicodeString(L"Num ") + IntToStr(Key - 0x60);
  }
  if ((Key >= 0x70) && (Key <= 0x87))
  {
    return UnicodeString(L"F") + IntToStr(Key - 0x70 + 1);
  }
  for (const TKeyName & Name : KeyNames)
  {
    if (Name.Key == Key)
    {
      return Name.Name;
    }
  }
  return UnicodeString();
}

}  // namespace

TShortCut __fastcall ShortCut(Word Key, TShiftState Shift)
{
  if ((Key & 0xFF00) != 0)
  {
    return 0;
  }
  TShortCut Result = Key;
  if (Shift.Contains(ssShift)) Result |= scShift;
  if (Shift.Contains(ssCtrl)) Result |= scCtrl;
  if (Shift.Contains(ssAlt)) Result |= scAlt;
  return Result;
}

UnicodeString __fastcall ShortCutToText(TShortCut ShortCut)
{
  const UnicodeString Name = KeyName(static_cast<Word>(ShortCut & 0xFF));
  if (Name.IsEmpty())
  {
    return UnicodeString();
  }
  UnicodeString Result;
  if ((ShortCut & scShift) != 0) Result += L"Shift+";
  if ((ShortCut & scCtrl) != 0) Result += L"Ctrl+";
  if ((ShortCut & scAlt) != 0) Result += L"Alt+";
  return Result + Name;
}

TShortCut __fastcall TextToShortCut(UnicodeString Text)
{
  TShortCut Shift = 0;
  bool Found = true;
  while (Found)
  {
    Found = false;
    for (const std::pair<const wchar_t *, TShortCut> & Modifier :
           { std::pair<const wchar_t *, TShortCut>(L"Shift+", scShift), { L"Ctrl+", scCtrl }, { L"Alt+", scAlt } })
    {
      if (StartsText(Modifier.first, Text))
      {
        Shift |= Modifier.second;
        Text.Delete(1, UnicodeString(Modifier.first).Length());
        Found = true;
      }
    }
  }
  if (Text.IsEmpty())
  {
    return 0;
  }
  for (unsigned Key = 1; Key < 0x100; Key++)
  {
    const UnicodeString Name = KeyName(static_cast<Word>(Key));
    if (!Name.IsEmpty() && SameText(Name, Text))
    {
      return static_cast<TShortCut>(Key | Shift);
    }
  }
  return 0;
}
