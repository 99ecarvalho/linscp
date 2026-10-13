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
  FDefaultLocale = AppliedLocale;
}

void TShellConfiguration::SetPreferredLocale(LCID Locale)
{
  FOverridden = false;
  this->Locale = Locale;
}

void TShellConfiguration::SetSettings(const TShellSettings & Settings)
{
  FSettings = Settings;
  Changed();
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

UnicodeString __fastcall TShellConfiguration::ModuleFileName()
{
  return !FModuleFileName.IsEmpty() ? FModuleFileName : TGUIConfiguration::ModuleFileName();
}

void TShellConfiguration::SetTranslationsDirectory(const UnicodeString & Translations)
{
  const UnicodeString Own = TGUIConfiguration::ModuleFileName();
  FModuleFileName =
    IncludeTrailingBackslash(ExtractFilePath(ExcludeTrailingBackslash(Translations))) + L"linscp" + ExtractFileExt(Own);
}

UnicodeString TShellConfiguration::TemporaryDir(bool Mask)
{
  return DefaultTemporaryDir(Mask);
}

// TWinConfiguration's keys, in its own Interface and Interface\Commander
// blocks (REGCONFIG), with its names (PropertyToKey: the property's).
void __fastcall TShellConfiguration::SaveData(THierarchicalStorage * Storage, bool All)
{
  TGUIConfiguration::SaveData(Storage, All);
  if (Storage->OpenSubKeyPath(L"Interface", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(PropertyToKey(L"LocaleSafe"), FOverridden ? FSavedLocale : LocaleSafe);
    Storage->WriteBool(L"ShowHiddenFiles", FSettings.ShowHiddenFiles);
    Storage->WriteInteger(L"FormatSizeBytes", FSettings.FormatSizeBytes);
    Storage->WriteBool(L"NaturalOrderNumericalSorting", FSettings.NaturalOrderNumericalSorting);
    Storage->WriteBool(L"AlwaysSortDirectoriesByName", FSettings.AlwaysSortDirectoriesByName);
    Storage->WriteBool(L"RenameWholeName", FSettings.RenameWholeName);
    Storage->WriteBool(L"DefaultDirIsHome", FSettings.DefaultDirIsHome);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteBool(L"SwappedPanels", FSettings.SwappedPanels);
    Storage->WriteBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
  }
}

void __fastcall TShellConfiguration::LoadData(THierarchicalStorage * Storage)
{
  TGUIConfiguration::LoadData(Storage);
  if (Storage->OpenSubKeyPath(L"Interface", false))
  {
    TCloseSubKeyPath Close(Storage);
    LocaleSafe = Storage->ReadInteger(PropertyToKey(L"LocaleSafe"), LocaleSafe);
    FSettings.ShowHiddenFiles = Storage->ReadBool(L"ShowHiddenFiles", FSettings.ShowHiddenFiles);
    FSettings.FormatSizeBytes = Storage->ReadInteger(L"FormatSizeBytes", FSettings.FormatSizeBytes);
    FSettings.NaturalOrderNumericalSorting =
      Storage->ReadBool(L"NaturalOrderNumericalSorting", FSettings.NaturalOrderNumericalSorting);
    FSettings.AlwaysSortDirectoriesByName =
      Storage->ReadBool(L"AlwaysSortDirectoriesByName", FSettings.AlwaysSortDirectoriesByName);
    FSettings.RenameWholeName = Storage->ReadBool(L"RenameWholeName", FSettings.RenameWholeName);
    FSettings.DefaultDirIsHome = Storage->ReadBool(L"DefaultDirIsHome", FSettings.DefaultDirIsHome);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.SwappedPanels = Storage->ReadBool(L"SwappedPanels", FSettings.SwappedPanels);
    FSettings.ExplorerKeyboardShortcuts =
      Storage->ReadBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
  }
}
