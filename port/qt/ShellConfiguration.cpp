#include <vcl.h>
#pragma hdrstop

#include "ShellConfiguration.h"

#include <Common.h>
#include <HierarchicalStorage.h>

#include "FrontendCommon.h"

namespace
{

// The block opened, closed however its reading or writing ends.
class TCloseSubKeyPath
{
public:
  explicit TCloseSubKeyPath(THierarchicalStorage * Storage) : FStorage(Storage) {}
  ~TCloseSubKeyPath() { FStorage->CloseSubKeyPath(); }

private:
  THierarchicalStorage * FStorage;
};

}  // namespace

TShellConfiguration::TShellConfiguration()
{
  Default();
  // Load complete locale according to the UI language.
  SetLocaleInternal(NULL, true, true);
}

void TShellConfiguration::OverrideLocale(LCID Locale)
{
  if (!FOverridden)
  {
    FSavedLocale = this->Locale;
    FOverridden = true;
  }
  SetLocaleInternal(Locale, true, false);
}

UnicodeString TShellConfiguration::TemporaryDir(bool Mask)
{
  return DefaultTemporaryDir(Mask);
}

// TWinConfiguration's keys, in its own Interface block (REGCONFIG).
void __fastcall TShellConfiguration::SaveData(THierarchicalStorage * Storage, bool All)
{
  TGUIConfiguration::SaveData(Storage, All);
  if (Storage->OpenSubKeyPath(L"Interface", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(PropertyToKey(L"LocaleSafe"), FOverridden ? FSavedLocale : LocaleSafe);
  }
}

void __fastcall TShellConfiguration::LoadData(THierarchicalStorage * Storage)
{
  TGUIConfiguration::LoadData(Storage);
  if (Storage->OpenSubKeyPath(L"Interface", false))
  {
    TCloseSubKeyPath Close(Storage);
    LocaleSafe = Storage->ReadInteger(PropertyToKey(L"LocaleSafe"), LocaleSafe);
  }
}
