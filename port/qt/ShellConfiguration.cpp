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

UnicodeString TShellConfiguration::ExpandedTemporaryDirectory() const
{
  UnicodeString Result = ExpandFileName(ExpandEnvironmentVariables(FSettings.DDTemporaryDirectory));
  if (FSettings.DDTemporaryDirectory.IsEmpty() || Result.IsEmpty())
  {
    Result = SystemTemporaryDirectory();
  }
  return Result;
}

// As TWinConfiguration::TemporaryDir.
UnicodeString TShellConfiguration::TemporaryDir(bool Mask)
{
  return DefaultTemporaryDir(Mask, ExpandedTemporaryDirectory());
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
    Storage->WriteInteger(L"PathInCaption", FSettings.PathInCaption);
    Storage->WriteBool(L"KeepOpenWhenNoSession", FSettings.KeepOpenWhenNoSession);
    Storage->WriteString(L"FileColors", FSettings.FileColors);
    Storage->WriteString(L"DDTemporaryDirectory", FSettings.DDTemporaryDirectory);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteBool(L"SwappedPanels", FSettings.SwappedPanels);
    Storage->WriteBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
    Storage->WriteInteger(L"CurrentPanel", FSettings.CurrentPanel);
    Storage->WriteFloat(L"LocalPanelWidth", FSettings.LocalPanelWidth);
    if (!FSettings.WindowParams.IsEmpty())
    {
      Storage->WriteString(L"WindowParams", FSettings.WindowParams);
    }
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\LocalPanel", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteString(L"DirViewParams", FSettings.LocalDirViewParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\RemotePanel", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteString(L"DirViewParams", FSettings.RemoteDirViewParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(L"Height", FSettings.QueueViewHeight);
    Storage->WriteInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
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
    FSettings.PathInCaption = Storage->ReadInteger(L"PathInCaption", FSettings.PathInCaption);
    FSettings.KeepOpenWhenNoSession = Storage->ReadBool(L"KeepOpenWhenNoSession", FSettings.KeepOpenWhenNoSession);
    FSettings.FileColors = Storage->ReadString(L"FileColors", FSettings.FileColors);
    FSettings.DDTemporaryDirectory = Storage->ReadString(L"DDTemporaryDirectory", FSettings.DDTemporaryDirectory);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.SwappedPanels = Storage->ReadBool(L"SwappedPanels", FSettings.SwappedPanels);
    FSettings.ExplorerKeyboardShortcuts =
      Storage->ReadBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
    FSettings.CurrentPanel = Storage->ReadInteger(L"CurrentPanel", FSettings.CurrentPanel);
    FSettings.LocalPanelWidth = Storage->ReadFloat(L"LocalPanelWidth", FSettings.LocalPanelWidth);
    FSettings.WindowParams = Storage->ReadString(L"WindowParams", FSettings.WindowParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\LocalPanel", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.LocalDirViewParams = Storage->ReadString(L"DirViewParams", FSettings.LocalDirViewParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\RemotePanel", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.RemoteDirViewParams = Storage->ReadString(L"DirViewParams", FSettings.RemoteDirViewParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.QueueViewHeight = Storage->ReadInteger(L"Height", FSettings.QueueViewHeight);
    FSettings.QueueViewHeightPixelsPerInch =
      Storage->ReadInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
  }
}
