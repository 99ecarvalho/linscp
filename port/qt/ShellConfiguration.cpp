#include <vcl.h>
#pragma hdrstop

#include "ShellConfiguration.h"

#include <algorithm>
#include <memory>

#include <Common.h>
#include <HierarchicalStorage.h>
#include <Exceptions.h>

#include "FrontendCommon.h"
#include "CustomCommandType.h"
#include "EditorPreferences.h"
#include <TextsWin.h>
#include <TextsCore.h>
#include <Cryptography.h>
#include <Security.h>
#include <SessionData.h>
#include <CoreMain.h>

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

TShellConfiguration * WinConfiguration = NULL;

namespace
{

#if !defined(_WIN32)
// Upstream's default, %ProgramFiles%\PuTTY\putty.exe, has no POSIX
// counterpart: PuTTY where it is installed; else OpenSSH's ssh in the
// desktop's terminal, in the current remote directory. That is a site
// command (WinSCP's patterns: !U user, !@ host, !# port, !/ directory), to
// which OpenSessionInPutty adds none of PuTTY's options.
UnicodeString PosixPuttyPath()
{
  const UnicodeString Path = GetEnvironmentVariable(L"PATH");
  if (!FileSearch(L"putty", Path).IsEmpty())
  {
    return FormatCommand(L"putty", L"");
  }
  static const wchar_t * const Terminals[][2] = {
    { L"x-terminal-emulator", L"-e" }, { L"gnome-terminal", L"--" }, { L"konsole", L"-e" },
    { L"xfce4-terminal", L"-x" }, { L"xterm", L"-e" } };
  for (const auto & Terminal : Terminals)
  {
    if (!FileSearch(Terminal[0], Path).IsEmpty())
    {
      return FORMAT(L"%s %s ssh -t -p !# !U@!@ \"cd '!/'; exec $SHELL -l\"", (Terminal[0], Terminal[1]));
    }
  }
  return FormatCommand(L"putty", L"");
}
#endif

}  // namespace

TShellConfiguration::TShellConfiguration() :
  FBookmarks(new TBookmarks()),
  FCustomCommandList(new TCustomCommandList()),
  FEditorList(new TEditorList()),
  FCustomCommandOptions(new TStringList())
{
  WinConfiguration = this;
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

std::vector<UnicodeString> TShellConfiguration::History(const UnicodeString & Name) const
{
  const auto Found = FHistory.find(Name);
  return (Found != FHistory.end()) ? Found->second : std::vector<UnicodeString>();
}

void TShellConfiguration::AddToHistory(const UnicodeString & Name, const UnicodeString & Value)
{
  std::vector<UnicodeString> & Strings = FHistory[Name];
  if (!Value.IsEmpty())
  {
    Strings.erase(std::remove(Strings.begin(), Strings.end(), Value), Strings.end());
    Strings.insert(Strings.begin(), Value);
  }
  if (Strings.size() > 30)
  {
    Strings.resize(30);
  }
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

TStrings * TShellConfiguration::FindTemporaryFolders(bool OnlyFirst)
{
  std::unique_ptr<TStrings> Result(new TStringList());
  TSearchRecOwned SRec;
  const UnicodeString Mask = TemporaryDir(true);
  if (FindFirstUnchecked(Mask, faDirectory | faHidden, SRec) == 0)
  {
    do
    {
      if (SRec.IsDirectory())
      {
        Result->Add(SRec.GetFilePath());
      }
    }
    while ((FindNextChecked(SRec) == 0) && (!OnlyFirst || (Result->Count == 0)));
  }
  if (Result->Count == 0)
  {
    Result.reset(NULL);
  }
  return Result.release();
}

bool TShellConfiguration::AnyTemporaryFolders()
{
  std::unique_ptr<TStrings> Folders(FindTemporaryFolders(true));
  return (Folders.get() != NULL);
}

void TShellConfiguration::CleanupTemporaryFolders()
{
  std::unique_ptr<TStrings> Folders(FindTemporaryFolders(false));
  if (Folders.get() == NULL)
  {
    return;
  }
  Usage->Inc(L"TemporaryDirectoryCleanups");
  UnicodeString ErrorList;
  for (int Index = 0; Index < Folders->Count; Index++)
  {
    if (!RecursiveDeleteFile(Folders->Strings[Index]))
    {
      if (!ErrorList.IsEmpty())
      {
        ErrorList += L"\n";
      }
      ErrorList += Folders->Strings[Index];
    }
  }
  if (!ErrorList.IsEmpty())
  {
    throw ExtException(LoadStr(CLEANUP_TEMP_ERROR), ErrorList);
  }
}

// TWinConfiguration's keys, in its own Interface and Interface\Commander
// blocks (REGCONFIG), with its names (PropertyToKey: the property's).
void __fastcall TShellConfiguration::SaveData(THierarchicalStorage * Storage, bool All)
{
  TGUIConfiguration::SaveData(Storage, All);
  if (!FHistory.empty() && Storage->OpenSubKey(L"History", true))
  {
    for (const auto & Each : FHistory)
    {
      if (Storage->OpenSubKey(Each.first, true))
      {
        std::unique_ptr<TStringList> Strings(new TStringList());
        for (const UnicodeString & Value : Each.second)
        {
          Strings->Add(Value);
        }
        Storage->WriteValues(Strings.get());
        Storage->CloseSubKey();
      }
    }
    Storage->CloseSubKey();
  }
  if (Storage->OpenSubKeyPath(L"Interface", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(PropertyToKey(L"LocaleSafe"), FOverridden ? FSavedLocale : LocaleSafe);
    Storage->WriteBool(L"ShowHiddenFiles", FSettings.ShowHiddenFiles);
    Storage->WriteBool(L"ConfirmExitOnCompletion", FSettings.ConfirmExitOnCompletion);
    Storage->WriteBool(L"DefaultToNewRemoteTab", FSettings.DefaultToNewRemoteTab);
    Storage->WriteInteger(L"FormatSizeBytes", FSettings.FormatSizeBytes);
    Storage->WriteBool(L"NaturalOrderNumericalSorting", FSettings.NaturalOrderNumericalSorting);
    Storage->WriteBool(L"AlwaysSortDirectoriesByName", FSettings.AlwaysSortDirectoriesByName);
    Storage->WriteBool(L"RenameWholeName", FSettings.RenameWholeName);
    Storage->WriteBool(L"DefaultDirIsHome", FSettings.DefaultDirIsHome);
    Storage->WriteInteger(L"PathInCaption", FSettings.PathInCaption);
    Storage->WriteInteger(L"PanelSearch", FSettings.PanelSearch);
    Storage->WriteBool(L"PreservePanelState", FSettings.PreservePanelState);
    Storage->WriteBool(L"EnableQueueByDefault", FSettings.EnableQueueByDefault);
    Storage->WriteBool(L"CopyParamAutoSelectNotice", FSettings.CopyParamAutoSelectNotice);
    Storage->WriteBool(L"MinimizeToTray", FSettings.MinimizeToTray);
    Storage->WriteBool(L"ExternalSessionInExistingInstance", FSettings.ExternalSessionInExistingInstance);
    Storage->WriteBool(L"FullRowSelect", FSettings.FullRowSelect);
    Storage->WriteInteger(L"CopyOnDoubleClick", FSettings.DoubleClickAction);
    Storage->WriteBool(L"CopyOnDoubleClickConfirmation", FSettings.CopyOnDoubleClickConfirmation);
    Storage->WriteString(L"PanelFontName", FSettings.PanelFontName);
    Storage->WriteInteger(L"PanelFontSize", FSettings.PanelFontSize);
    Storage->WriteInteger(L"PanelFontStyle", FSettings.PanelFontStyle);
    Storage->WriteInteger(L"PanelFontCharset", FSettings.PanelFontCharset);
    Storage->WriteString(L"IconSet", FSettings.IconSet);
    Storage->WriteBool(L"LockToolbars", FSettings.LockToolbars);
    Storage->WriteBool(L"SelectiveToolbarText", FSettings.SelectiveToolbarText);
    Storage->WriteInteger(L"LargerToolbar", FSettings.LargerToolbar);
    Storage->WriteBool(L"KeepOpenWhenNoSession", FSettings.KeepOpenWhenNoSession);
    Storage->WriteBool(L"AutoOpenInPutty", FSettings.AutoOpenInPutty);
    Storage->WriteBool(L"AutoSaveWorkspace", FSettings.AutoSaveWorkspace);
    Storage->WriteBool(L"AutoSaveWorkspacePasswords", FSettings.AutoSaveWorkspacePasswords);
    Storage->WriteString(L"AutoWorkspace", FSettings.AutoWorkspace);
    Storage->WriteString(L"LastStoredSession", FSettings.LastStoredSession);
    Storage->WriteString(L"FileColors", FSettings.FileColors);
    Storage->WriteString(L"DDTemporaryDirectory", FSettings.DDTemporaryDirectory);
    Storage->WriteBool(L"TemporaryDirectoryAppendSession", FSettings.TemporaryDirectoryAppendSession);
    Storage->WriteBool(L"TemporaryDirectoryDeterministic", FSettings.TemporaryDirectoryDeterministic);
    Storage->WriteBool(L"TemporaryDirectoryAppendPath", FSettings.TemporaryDirectoryAppendPath);
    Storage->WriteBool(L"TemporaryDirectoryCleanup", FSettings.TemporaryDirectoryCleanup);
    Storage->WriteBool(L"ConfirmTemporaryDirectoryCleanup", FSettings.ConfirmTemporaryDirectoryCleanup);
    Storage->WriteBool(L"EditorCheckNotModified", FSettings.EditorCheckNotModified);
    Storage->WriteBool(L"UseSharedBookmarks", FSettings.UseSharedBookmarks);
    Storage->WriteBool(L"UseLocationProfiles", FSettings.UseLocationProfiles);
    Storage->WriteInteger(L"GenerateUrlComponents", FSettings.GenerateUrlComponents);
    Storage->WriteInteger(L"GenerateUrlCodeTarget", FSettings.GenerateUrlCodeTarget);
    Storage->WriteInteger(L"GenerateUrlScriptFormat", FSettings.GenerateUrlScriptFormat);
    Storage->WriteInteger(L"GenerateUrlAssemblyLanguage", FSettings.GenerateUrlAssemblyLanguage);
  }
  if (Storage->OpenSubKey(L"Bookmarks", true))
  {
    FBookmarks->Save(Storage, All);
    Storage->CloseSubKey();
  }
  // As TWinConfiguration::SaveData.
  if ((All && !FCustomCommandsDefaults) || FCustomCommandList->Modified)
  {
    FCustomCommandList->Save(Storage);

    if (FCustomCommandList->Count == 0)
    {
      Storage->WriteBool(L"CustomCommandsNone", true);
    }
    else
    {
      Storage->DeleteValue(L"CustomCommandsNone");
    }
  }
  if ((All || FCustomCommandOptionsModified) &&
      Storage->OpenSubKey(L"CustomCommandOptions", true))
  {
    Storage->ClearValues();
    Storage->WriteValues(FCustomCommandOptions.get(), true);
    Storage->CloseSubKey();
    FCustomCommandOptionsModified = false;
  }
  if (Storage->OpenSubKeyPath(L"Security", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteBool(L"UseMasterPassword", FUseMasterPassword);
    Storage->WriteString(L"MasterPasswordVerifier", FMasterPasswordVerifier);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteBool(L"SwappedPanels", FSettings.SwappedPanels);
    Storage->WriteBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
    Storage->WriteBool(L"CompareByTime", FSettings.CompareByTime);
    Storage->WriteInteger(L"ExplorerStyleSelection", FSettings.NortonLikeMode);
    Storage->WriteBool(L"CompareBySize", FSettings.CompareBySize);
    Storage->WriteBool(L"StatusBar", FSettings.CommanderStatusBar);
    Storage->WriteBool(L"SessionsTabs", FSettings.CommanderSessionsTabs);
    Storage->WriteString(L"ToolbarsLayout2", FSettings.CommanderToolbarsLayout);
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
    Storage->WriteBool(L"DriveView", FSettings.LocalDriveView);
    Storage->WriteBool(L"StatusBar", FSettings.LocalPanelStatusBar);
    Storage->WriteInteger(L"DriveViewHeight", FSettings.LocalDriveViewHeight);
    Storage->WriteInteger(L"DriveViewHeightPixelsPerInch", FSettings.LocalDriveViewHeightPixelsPerInch);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\RemotePanel", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteString(L"DirViewParams", FSettings.RemoteDirViewParams);
    Storage->WriteBool(L"DriveView", FSettings.RemoteDriveView);
    Storage->WriteBool(L"StatusBar", FSettings.RemotePanelStatusBar);
    Storage->WriteInteger(L"DriveViewHeight", FSettings.RemoteDriveViewHeight);
    Storage->WriteInteger(L"DriveViewHeightPixelsPerInch", FSettings.RemoteDriveViewHeightPixelsPerInch);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Editor", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteString(L"WindowParams", FSettings.EditorWindowParams);
    Storage->WriteBool(L"WordWrap", FSettings.EditorWordWrap);
    Storage->WriteInteger(L"TabSize", FSettings.EditorTabSize);
    Storage->WriteInteger(L"Encoding", FSettings.EditorEncoding);
    Storage->WriteString(L"FindText", FSettings.EditorFindText);
    Storage->WriteString(L"ReplaceText", FSettings.EditorReplaceText);
    Storage->WriteBool(L"FindMatchCase", FSettings.EditorFindMatchCase);
    Storage->WriteBool(L"FindWholeWord", FSettings.EditorFindWholeWord);
    // As TWinConfiguration::SaveData: the editors, its subkeys.
    if (All || FEditorList->Modified)
    {
      FEditorList->Save(Storage);
    }
    Storage->WriteString(L"FontName2", FSettings.EditorFontName);
    Storage->WriteInteger(L"FontSize", FSettings.EditorFontSize);
    Storage->WriteInteger(L"FontStyle", FSettings.EditorFontStyle);
    Storage->WriteInteger(L"FontCharset", FSettings.EditorFontCharset);
    Storage->WriteInteger(L"FontColor", FSettings.EditorFontColor);
    Storage->WriteInteger(L"BackgroundColor", FSettings.EditorBackgroundColor);
    Storage->WriteBool(L"WarnOrLargeFileSize", FSettings.EditorWarnOrLargeFileSize);
    Storage->WriteInteger(L"LargeFileSize", FSettings.EditorLargeFileSize);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\ConsoleWin", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteString(L"WindowSize", FSettings.ConsoleWindowSize);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(L"Height", FSettings.QueueViewHeight);
    Storage->WriteInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
    Storage->WriteInteger(L"Show", FSettings.QueueViewShow);
    Storage->WriteBool(L"ToolBar", FSettings.QueueViewToolBar);
  }
}

// As TWinConfiguration::Saved.
void __fastcall TShellConfiguration::Saved()
{
  TGUIConfiguration::Saved();
  FBookmarks->ModifyAll(false);
  FCustomCommandList->Reset();
  FEditorList->Saved();
}

// As TWinConfiguration::DefaultLocalized: WinSCP's own commands, until the
// user's are loaded.
// As TWinConfiguration::LoadFrom's, for no editors kept: the internal
// editor, then Notepad (Windows), or the associated application.
void TShellConfiguration::DefaultEditorList()
{
  FEditorList->Clear();
  FEditorList->Add(new TEditorPreferences());
  TEditorData Data;
#if defined(_WIN32)
  Data.Editor = edExternal;
  Data.ExternalEditor = TEditorPreferences::GetDefaultExternalEditor();
  Data.ExternalEditorOptionsAutodetect();
#else
  Data.Editor = edOpen;
#endif
  FEditorList->Add(new TEditorPreferences(Data));
  FEditorList->Saved();
}

void TShellConfiguration::SetEditorList(const TEditorList * List)
{
  if (!(*FEditorList == *List))
  {
    *FEditorList = *List;
    Changed();
  }
}

void __fastcall TShellConfiguration::Default()
{
  TGUIConfiguration::Default();
  if (FEditorList != NULL)
  {
    DefaultEditorList();
  }
#if !defined(_WIN32)
  PuttyPath = PosixPuttyPath();
#endif
}

void __fastcall TShellConfiguration::DefaultLocalized()
{
  TGUIConfiguration::DefaultLocalized();

  if (FCustomCommandsDefaults)
  {
    FCustomCommandList->Clear();
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_EXECUTE), L"\"./!\"", 0);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_TOUCH), L"touch \"!\"", ccApplyToDirectories | ccRecursive);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_TAR),
      FORMAT(L"tar -cz  -f \"!?%s?archive.tgz!\" !&",
        (LoadStr(CUSTOM_COMMAND_TAR_ARCHIVE))), ccApplyToDirectories);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_UNTAR),
      FORMAT(L"tar -xz --directory=\"!?%s?.!\" -f \"!\"",
        (LoadStr(CUSTOM_COMMAND_UNTAR_DIRECTORY))), 0);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_GREP),
      FORMAT(L"grep \"!?%s?!\" !&", (LoadStr(CUSTOM_COMMAND_GREP_PATTERN))),
      ccShowResults);
    FCustomCommandList->Add(LoadStr(CUSTOM_COMMAND_PRINT), L"notepad.exe /p \"!\"", ccLocal);
    FCustomCommandList->Reset();
    FCustomCommandsDefaults = true;
  }
}

void TShellConfiguration::SetCustomCommandList(const TCustomCommandList * List)
{
  if (!FCustomCommandList->Equals(List))
  {
    FCustomCommandList->Assign(List);
    FCustomCommandsDefaults = false;
    Changed();
  }
}

void TShellConfiguration::SetCustomCommandOptions(TStrings * Options)
{
  if (!FCustomCommandOptions->Equals(Options))
  {
    FCustomCommandOptions->Assign(Options);
    FCustomCommandOptionsModified = true;
    Changed();
  }
}

UnicodeString __fastcall TShellConfiguration::ExtensionStringTranslation(const UnicodeString & /*ExtensionId*/,
  const UnicodeString & S)
{
  return S;
}

void __fastcall TShellConfiguration::LoadData(THierarchicalStorage * Storage)
{
  TGUIConfiguration::LoadData(Storage);
  if (Storage->OpenSubKey(L"History", false))
  {
    std::unique_ptr<TStringList> Names(new TStringList());
    Storage->GetSubKeyNames(Names.get());
    for (int Index = 0; Index < Names->Count; Index++)
    {
      if (Storage->OpenSubKey(Names->Strings[Index], false))
      {
        std::unique_ptr<TStringList> Strings(new TStringList());
        Storage->ReadValues(Strings.get());
        std::vector<UnicodeString> & Values = FHistory[Names->Strings[Index]];
        Values.clear();
        for (int Value = 0; Value < Strings->Count; Value++)
        {
          Values.push_back(Strings->Strings[Value]);
        }
        Storage->CloseSubKey();
      }
    }
    Storage->CloseSubKey();
  }
  if (Storage->OpenSubKeyPath(L"Interface", false))
  {
    TCloseSubKeyPath Close(Storage);
    LocaleSafe = Storage->ReadInteger(PropertyToKey(L"LocaleSafe"), LocaleSafe);
    FSettings.ShowHiddenFiles = Storage->ReadBool(L"ShowHiddenFiles", FSettings.ShowHiddenFiles);
    FSettings.ConfirmExitOnCompletion = Storage->ReadBool(L"ConfirmExitOnCompletion", FSettings.ConfirmExitOnCompletion);
    FSettings.DefaultToNewRemoteTab = Storage->ReadBool(L"DefaultToNewRemoteTab", FSettings.DefaultToNewRemoteTab);
    FSettings.FormatSizeBytes = Storage->ReadInteger(L"FormatSizeBytes", FSettings.FormatSizeBytes);
    FSettings.NaturalOrderNumericalSorting =
      Storage->ReadBool(L"NaturalOrderNumericalSorting", FSettings.NaturalOrderNumericalSorting);
    FSettings.AlwaysSortDirectoriesByName =
      Storage->ReadBool(L"AlwaysSortDirectoriesByName", FSettings.AlwaysSortDirectoriesByName);
    FSettings.RenameWholeName = Storage->ReadBool(L"RenameWholeName", FSettings.RenameWholeName);
    FSettings.DefaultDirIsHome = Storage->ReadBool(L"DefaultDirIsHome", FSettings.DefaultDirIsHome);
    FSettings.PathInCaption = Storage->ReadInteger(L"PathInCaption", FSettings.PathInCaption);
    FSettings.PanelSearch = Storage->ReadInteger(L"PanelSearch", FSettings.PanelSearch);
    FSettings.PreservePanelState = Storage->ReadBool(L"PreservePanelState", FSettings.PreservePanelState);
    FSettings.EnableQueueByDefault = Storage->ReadBool(L"EnableQueueByDefault", FSettings.EnableQueueByDefault);
    FSettings.CopyParamAutoSelectNotice = Storage->ReadBool(L"CopyParamAutoSelectNotice", FSettings.CopyParamAutoSelectNotice);
    FSettings.MinimizeToTray = Storage->ReadBool(L"MinimizeToTray", FSettings.MinimizeToTray);
    FSettings.ExternalSessionInExistingInstance =
      Storage->ReadBool(L"ExternalSessionInExistingInstance", FSettings.ExternalSessionInExistingInstance);
    FSettings.FullRowSelect = Storage->ReadBool(L"FullRowSelect", FSettings.FullRowSelect);
    FSettings.DoubleClickAction = Storage->ReadInteger(L"CopyOnDoubleClick", FSettings.DoubleClickAction);
    FSettings.CopyOnDoubleClickConfirmation =
      Storage->ReadBool(L"CopyOnDoubleClickConfirmation", FSettings.CopyOnDoubleClickConfirmation);
    FSettings.PanelFontName = Storage->ReadString(L"PanelFontName", FSettings.PanelFontName);
    FSettings.PanelFontSize = Storage->ReadInteger(L"PanelFontSize", FSettings.PanelFontSize);
    FSettings.PanelFontStyle = Storage->ReadInteger(L"PanelFontStyle", FSettings.PanelFontStyle);
    FSettings.PanelFontCharset = Storage->ReadInteger(L"PanelFontCharset", FSettings.PanelFontCharset);
    FSettings.IconSet = Storage->ReadString(L"IconSet", FSettings.IconSet);
    FSettings.LockToolbars = Storage->ReadBool(L"LockToolbars", FSettings.LockToolbars);
    FSettings.SelectiveToolbarText = Storage->ReadBool(L"SelectiveToolbarText", FSettings.SelectiveToolbarText);
    FSettings.LargerToolbar = Storage->ReadInteger(L"LargerToolbar", FSettings.LargerToolbar);
    FSettings.KeepOpenWhenNoSession = Storage->ReadBool(L"KeepOpenWhenNoSession", FSettings.KeepOpenWhenNoSession);
    FSettings.AutoOpenInPutty = Storage->ReadBool(L"AutoOpenInPutty", FSettings.AutoOpenInPutty);
    FSettings.AutoSaveWorkspace = Storage->ReadBool(L"AutoSaveWorkspace", FSettings.AutoSaveWorkspace);
    FSettings.AutoSaveWorkspacePasswords = Storage->ReadBool(L"AutoSaveWorkspacePasswords", FSettings.AutoSaveWorkspacePasswords);
    FSettings.AutoWorkspace = Storage->ReadString(L"AutoWorkspace", FSettings.AutoWorkspace);
    FSettings.LastStoredSession = Storage->ReadString(L"LastStoredSession", FSettings.LastStoredSession);
    FSettings.FileColors = Storage->ReadString(L"FileColors", FSettings.FileColors);
    FSettings.DDTemporaryDirectory = Storage->ReadString(L"DDTemporaryDirectory", FSettings.DDTemporaryDirectory);
    FSettings.TemporaryDirectoryAppendSession =
      Storage->ReadBool(L"TemporaryDirectoryAppendSession", FSettings.TemporaryDirectoryAppendSession);
    FSettings.TemporaryDirectoryDeterministic =
      Storage->ReadBool(L"TemporaryDirectoryDeterministic", FSettings.TemporaryDirectoryDeterministic);
    FSettings.TemporaryDirectoryAppendPath = Storage->ReadBool(L"TemporaryDirectoryAppendPath", FSettings.TemporaryDirectoryAppendPath);
    FSettings.TemporaryDirectoryCleanup = Storage->ReadBool(L"TemporaryDirectoryCleanup", FSettings.TemporaryDirectoryCleanup);
    FSettings.ConfirmTemporaryDirectoryCleanup =
      Storage->ReadBool(L"ConfirmTemporaryDirectoryCleanup", FSettings.ConfirmTemporaryDirectoryCleanup);
    FSettings.EditorCheckNotModified = Storage->ReadBool(L"EditorCheckNotModified", FSettings.EditorCheckNotModified);
    FSettings.UseSharedBookmarks = Storage->ReadBool(L"UseSharedBookmarks", FSettings.UseSharedBookmarks);
    FSettings.UseLocationProfiles = Storage->ReadBool(L"UseLocationProfiles", FSettings.UseLocationProfiles);
    FSettings.GenerateUrlComponents = Storage->ReadInteger(L"GenerateUrlComponents", FSettings.GenerateUrlComponents);
    FSettings.GenerateUrlCodeTarget = Storage->ReadInteger(L"GenerateUrlCodeTarget", FSettings.GenerateUrlCodeTarget);
    FSettings.GenerateUrlScriptFormat = Storage->ReadInteger(L"GenerateUrlScriptFormat", FSettings.GenerateUrlScriptFormat);
    FSettings.GenerateUrlAssemblyLanguage =
      Storage->ReadInteger(L"GenerateUrlAssemblyLanguage", FSettings.GenerateUrlAssemblyLanguage);
  }
  if (Storage->OpenSubKey(L"Bookmarks", false))
  {
    FBookmarks->Load(Storage);
    Storage->CloseSubKey();
  }
  // As TWinConfiguration::LoadData.
  if (Storage->KeyExists(L"CustomCommands") ||
      Storage->ReadBool(L"CustomCommandsNone", false))
  {
    FCustomCommandList->Load(Storage);
    FCustomCommandsDefaults = false;
  }
  if (Storage->OpenSubKey(L"CustomCommandOptions", false))
  {
    Storage->ReadValues(FCustomCommandOptions.get(), true);
    Storage->CloseSubKey();
    FCustomCommandOptionsModified = false;
  }
  if (Storage->OpenSubKeyPath(L"Security", false))
  {
    TCloseSubKeyPath Close(Storage);
    FUseMasterPassword = Storage->ReadBool(L"UseMasterPassword", FUseMasterPassword);
    FMasterPasswordVerifier = Storage->ReadString(L"MasterPasswordVerifier", FMasterPasswordVerifier);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.SwappedPanels = Storage->ReadBool(L"SwappedPanels", FSettings.SwappedPanels);
    FSettings.ExplorerKeyboardShortcuts =
      Storage->ReadBool(L"ExplorerKeyboardShortcuts", FSettings.ExplorerKeyboardShortcuts);
    FSettings.CompareByTime = Storage->ReadBool(L"CompareByTime", FSettings.CompareByTime);
    FSettings.NortonLikeMode = Storage->ReadInteger(L"ExplorerStyleSelection", FSettings.NortonLikeMode);
    FSettings.CompareBySize = Storage->ReadBool(L"CompareBySize", FSettings.CompareBySize);
    FSettings.CommanderStatusBar = Storage->ReadBool(L"StatusBar", FSettings.CommanderStatusBar);
    FSettings.CommanderSessionsTabs = Storage->ReadBool(L"SessionsTabs", FSettings.CommanderSessionsTabs);
    FSettings.CommanderToolbarsLayout = Storage->ReadString(L"ToolbarsLayout2", FSettings.CommanderToolbarsLayout);
    FSettings.CurrentPanel = Storage->ReadInteger(L"CurrentPanel", FSettings.CurrentPanel);
    FSettings.LocalPanelWidth = Storage->ReadFloat(L"LocalPanelWidth", FSettings.LocalPanelWidth);
    FSettings.WindowParams = Storage->ReadString(L"WindowParams", FSettings.WindowParams);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\LocalPanel", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.LocalDirViewParams = Storage->ReadString(L"DirViewParams", FSettings.LocalDirViewParams);
    FSettings.LocalDriveView = Storage->ReadBool(L"DriveView", FSettings.LocalDriveView);
    FSettings.LocalPanelStatusBar = Storage->ReadBool(L"StatusBar", FSettings.LocalPanelStatusBar);
    FSettings.LocalDriveViewHeight = Storage->ReadInteger(L"DriveViewHeight", FSettings.LocalDriveViewHeight);
    FSettings.LocalDriveViewHeightPixelsPerInch =
      Storage->ReadInteger(L"DriveViewHeightPixelsPerInch", FSettings.LocalDriveViewHeightPixelsPerInch);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Commander\\RemotePanel", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.RemoteDirViewParams = Storage->ReadString(L"DirViewParams", FSettings.RemoteDirViewParams);
    FSettings.RemoteDriveView = Storage->ReadBool(L"DriveView", FSettings.RemoteDriveView);
    FSettings.RemotePanelStatusBar = Storage->ReadBool(L"StatusBar", FSettings.RemotePanelStatusBar);
    FSettings.RemoteDriveViewHeight = Storage->ReadInteger(L"DriveViewHeight", FSettings.RemoteDriveViewHeight);
    FSettings.RemoteDriveViewHeightPixelsPerInch =
      Storage->ReadInteger(L"DriveViewHeightPixelsPerInch", FSettings.RemoteDriveViewHeightPixelsPerInch);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\Editor", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.EditorWindowParams = Storage->ReadString(L"WindowParams", FSettings.EditorWindowParams);
    FSettings.EditorWordWrap = Storage->ReadBool(L"WordWrap", FSettings.EditorWordWrap);
    FSettings.EditorTabSize = Storage->ReadInteger(L"TabSize", FSettings.EditorTabSize);
    FSettings.EditorEncoding = Storage->ReadInteger(L"Encoding", FSettings.EditorEncoding);
    FSettings.EditorFindText = Storage->ReadString(L"FindText", FSettings.EditorFindText);
    FSettings.EditorReplaceText = Storage->ReadString(L"ReplaceText", FSettings.EditorReplaceText);
    FSettings.EditorFindMatchCase = Storage->ReadBool(L"FindMatchCase", FSettings.EditorFindMatchCase);
    FSettings.EditorFindWholeWord = Storage->ReadBool(L"FindWholeWord", FSettings.EditorFindWholeWord);
    // As TWinConfiguration::LoadData and LoadFrom: the editors, else (none
    // kept) the defaults. WinSCP 4's single editor keys are not read.
    std::unique_ptr<TEditorList> Editors(new TEditorList());
    Editors->Load(Storage);
    if (Editors->Count > 0)
    {
      *FEditorList = *Editors;
      FEditorList->Saved();
    }
    FSettings.EditorFontName = Storage->ReadString(L"FontName2", FSettings.EditorFontName);
    FSettings.EditorFontSize = Storage->ReadInteger(L"FontSize", FSettings.EditorFontSize);
    FSettings.EditorFontStyle = Storage->ReadInteger(L"FontStyle", FSettings.EditorFontStyle);
    FSettings.EditorFontCharset = Storage->ReadInteger(L"FontCharset", FSettings.EditorFontCharset);
    FSettings.EditorFontColor = Storage->ReadInteger(L"FontColor", FSettings.EditorFontColor);
    FSettings.EditorBackgroundColor = Storage->ReadInteger(L"BackgroundColor", FSettings.EditorBackgroundColor);
    FSettings.EditorWarnOrLargeFileSize = Storage->ReadBool(L"WarnOrLargeFileSize", FSettings.EditorWarnOrLargeFileSize);
    FSettings.EditorLargeFileSize = Storage->ReadInteger(L"LargeFileSize", FSettings.EditorLargeFileSize);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\ConsoleWin", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.ConsoleWindowSize = Storage->ReadString(L"WindowSize", FSettings.ConsoleWindowSize);
  }
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.QueueViewHeight = Storage->ReadInteger(L"Height", FSettings.QueueViewHeight);
    FSettings.QueueViewHeightPixelsPerInch =
      Storage->ReadInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
    FSettings.QueueViewShow = Storage->ReadInteger(L"Show", FSettings.QueueViewShow);
    FSettings.QueueViewToolBar = Storage->ReadBool(L"ToolBar", FSettings.QueueViewToolBar);
  }
}

// TWinConfiguration's master password, as it has it.
RawByteString __fastcall TShellConfiguration::StronglyRecryptPassword(RawByteString Password, UnicodeString Key)
{
  RawByteString Dummy;
  RawByteString Result;
  if (GetExternalEncryptedPassword(Password, Dummy) || !FUseMasterPassword)
  {
    // Already strongly encrypted, or no master password to do it with.
    Result = Password;
  }
  else
  {
    UnicodeString PasswordText = TGUIConfiguration::DecryptPassword(Password, Key);
    if (!PasswordText.IsEmpty())
    {
      AskForMasterPasswordIfNotSet();
      Password = ScramblePassword(PasswordText);
      AES256EncyptWithMAC(Password, FPlainMasterPasswordEncrypt, Result);
      Result = SetExternalEncryptedPassword(Result);
    }
  }
  return Result;
}

UnicodeString __fastcall TShellConfiguration::DecryptPassword(RawByteString Password, UnicodeString Key)
{
  UnicodeString Result;
  RawByteString Buf;
  if (GetExternalEncryptedPassword(Password, Buf))
  {
    // The decrypting password, as it is the one kept while recrypting.
    if (FPlainMasterPasswordDecrypt.IsEmpty())
    {
      AskForMasterPassword();
    }
    if (!AES256DecryptWithMAC(Buf, FPlainMasterPasswordDecrypt, Buf) || !UnscramblePassword(Buf, Result))
    {
      throw Exception(LoadStr(DECRYPT_PASSWORD_ERROR));
    }
  }
  else
  {
    Result = TGUIConfiguration::DecryptPassword(Password, Key);
  }
  return Result;
}

void TShellConfiguration::SetMasterPassword(const UnicodeString & Value)
{
  // Off, a stray encrypted password (a site imported) is all it decrypts:
  // new passwords are not encrypted with it.
  if (!FUseMasterPassword)
  {
    FPlainMasterPasswordDecrypt = Value;
  }
  else if (DebugAlwaysTrue(ValidateMasterPassword(Value)))
  {
    FPlainMasterPasswordEncrypt = Value;
    FPlainMasterPasswordDecrypt = Value;
  }
}

void TShellConfiguration::RecryptPasswords(TStrings * RecryptPasswordErrors)
{
  StoredSessions->RecryptPasswords(RecryptPasswordErrors);
  if (OnMasterPasswordRecrypt)
  {
    try
    {
      OnMasterPasswordRecrypt();
    }
    catch (Exception & E)
    {
      UnicodeString Message;
      if (ExceptionMessage(&E, Message))
      {
        RecryptPasswordErrors->Add(Message);
      }
    }
  }
}

void TShellConfiguration::ChangeMasterPassword(const UnicodeString & Value, TStrings * RecryptPasswordErrors)
{
  RawByteString Verifier;
  AES256CreateVerifier(Value, Verifier);
  FMasterPasswordVerifier = BytesToHex(Verifier);
  FPlainMasterPasswordEncrypt = Value;
  FUseMasterPassword = true;
  // The old one decrypts while recrypting; the new one after, whatever
  // happens.
  try
  {
    RecryptPasswords(RecryptPasswordErrors);
  }
  catch (...)
  {
    FPlainMasterPasswordDecrypt = Value;
    throw;
  }
  FPlainMasterPasswordDecrypt = Value;
}

bool TShellConfiguration::ValidateMasterPassword(const UnicodeString & Value)
{
  DebugAssert(FUseMasterPassword && !FMasterPasswordVerifier.IsEmpty());
  return AES256Verify(Value, HexToBytes(FMasterPasswordVerifier));
}

void TShellConfiguration::ClearMasterPassword(TStrings * RecryptPasswordErrors)
{
  FMasterPasswordVerifier = L"";
  FUseMasterPassword = false;
  Shred(FPlainMasterPasswordEncrypt);
  try
  {
    RecryptPasswords(RecryptPasswordErrors);
  }
  catch (...)
  {
    Shred(FPlainMasterPasswordDecrypt);
    throw;
  }
  Shred(FPlainMasterPasswordDecrypt);
}

void TShellConfiguration::AskForMasterPassword()
{
  if (FMasterPasswordSession > 0)
  {
    if (FMasterPasswordSessionAsked)
    {
      FMasterPasswordRefusals++;
      Abort();
    }
    // Before the prompt, as it may abort.
    FMasterPasswordSessionAsked = true;
  }
  if (!OnMasterPasswordPrompt)
  {
    throw Exception(L"Master password handler not set");
  }
  try
  {
    OnMasterPasswordPrompt();
  }
  catch (EAbort &)
  {
    FMasterPasswordRefusals++;
    throw;
  }
  DebugAssert(!FPlainMasterPasswordDecrypt.IsEmpty());
}

void TShellConfiguration::AskForMasterPasswordIfNotSet()
{
  if (FPlainMasterPasswordEncrypt.IsEmpty())
  {
    AskForMasterPassword();
  }
}

void TShellConfiguration::BeginMasterPasswordSession()
{
  DebugAssert(FMasterPasswordSession == 0);
  FMasterPasswordSession++;
}

void TShellConfiguration::EndMasterPasswordSession()
{
  if (DebugAlwaysTrue(FMasterPasswordSession > 0))
  {
    FMasterPasswordSession--;
  }
  FMasterPasswordSessionAsked = false;
}
