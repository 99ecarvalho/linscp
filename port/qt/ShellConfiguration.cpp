#include <vcl.h>
#pragma hdrstop

#include "ShellConfiguration.h"

#include <Common.h>
#include <HierarchicalStorage.h>
#include <Exceptions.h>

#include "FrontendCommon.h"
#include "CustomCommandType.h"
#include "EditorPreferences.h"
#include <TextsWin.h>

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
  if (Storage->OpenSubKeyPath(L"Interface\\Editor", true))
  {
    TCloseSubKeyPath Close(Storage);
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
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", true))
  {
    TCloseSubKeyPath Close(Storage);
    Storage->WriteInteger(L"Height", FSettings.QueueViewHeight);
    Storage->WriteInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
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
  if (Storage->OpenSubKeyPath(L"Interface\\Editor", false))
  {
    TCloseSubKeyPath Close(Storage);
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
  if (Storage->OpenSubKeyPath(L"Interface\\QueueView", false))
  {
    TCloseSubKeyPath Close(Storage);
    FSettings.QueueViewHeight = Storage->ReadInteger(L"Height", FSettings.QueueViewHeight);
    FSettings.QueueViewHeightPixelsPerInch =
      Storage->ReadInteger(L"HeightPixelsPerInch", FSettings.QueueViewHeightPixelsPerInch);
  }
}
