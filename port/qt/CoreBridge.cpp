#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"
#include "ShellConfiguration.h"
#include "GuiShim.h"

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <GUIConfiguration.h>
#include <FileMasks.h>
#include <HeadlessMain.h>
#include <TextsCore.h>
#include <TextsWin.h>
#include <PuttyTools.h>

#include <functional>
#include <FtpFileSystem.h>
#include <S3FileSystem.h>

#include "PuttySlice.h"
#include "EditorPreferences.h"

namespace
{

CoreBridge::IFrontend * GFrontend = NULL;
DWORD GGuiThread = 0;

}  // namespace

namespace CoreBridge
{

std::string Utf8(const UnicodeString & Str)
{
  const UTF8String Result(Str);
  return std::string(Result.c_str(), Result.Length());
}

UnicodeString FromUtf8(const std::string & Str)
{
  return UTF8ToString(Str.c_str());
}

IFrontend * Frontend()
{
  return GFrontend;
}

bool IsGuiThread()
{
  return (GGuiThread != 0) && (GetCurrentThreadId() == GGuiThread);
}

bool ErrorOf(Exception * E, TError & Error)
{
  UnicodeString Message;
  if (!ExceptionMessageFormatted(E, Message))
  {
    return false;
  }
  Error.Message = Utf8(UnformatMessage(RemoveInteractiveMsgTag(Message)));
  ExtException * EE = dynamic_cast<ExtException *>(E);
  if (EE != NULL)
  {
    if ((EE->MoreMessages != NULL) && (EE->MoreMessages->Count > 0))
    {
      Error.MoreMessages = Utf8(UnformatMessage(EE->MoreMessages->Text));
    }
    Error.HelpKeyword = Utf8(EE->HelpKeyword);
  }
  return true;
}

TCoreError CoreErrorOf(Exception & E)
{
  TError Error;
  const bool Shown = ErrorOf(&E, Error);
  if (!Shown)
  {
    Error.Message = Utf8(E.Message);
  }
  return TCoreError(Error, !Shown);
}

namespace
{

TQueryType BridgeType(::TQueryType Type)
{
  switch (Type)
  {
    case qtWarning: return TQueryType::Warning;
    case qtError: return TQueryType::Error;
    case qtInformation: return TQueryType::Information;
    default: return TQueryType::Confirmation;
  }
}

bool IsPositiveAnswer(unsigned int Answer)
{
  return (Answer == qaYes) || (Answer == qaOK) || (Answer == qaYesToAll);
}

}  // namespace

unsigned int AskFrontend(const UnicodeString & Message, TStrings * MoreMessages, ::TQueryType Type,
  unsigned int Answers, const TQueryParams * Params)
{
  DebugAssert(GFrontend != NULL);
  TQuery Query;
  Query.Type = BridgeType(Type);
  Query.Message = Utf8(UnformatMessage(RemoveInteractiveMsgTag(Message)));
  if ((MoreMessages != NULL) && (MoreMessages->Count > 0))
  {
    Query.MoreMessages = Utf8(UnformatMessage(MoreMessages->Text));
  }
  for (unsigned int A = qaFirst; A <= qaLast; A = A << 1)
  {
    if (FLAGSET(Answers, A))
    {
      UnicodeString Name;
      UnicodeString Caption;
      AnswerNameAndCaption(A, Name, Caption);
      TQueryButton Button;
      Button.Answer = A;
      Button.Caption = Utf8(Caption);
      if ((Params != NULL) && (Params->Aliases != NULL))
      {
        for (unsigned int I = 0; I < Params->AliasesCount; I++)
        {
          const TQueryButtonAlias & Alias = Params->Aliases[I];
          if (Alias.Button == A)
          {
            if (!Alias.Alias.IsEmpty())
            {
              Button.Caption = Utf8(Alias.Alias);
            }
            if (Alias.OnSubmit != NULL)
            {
              // As TMessageForm::ButtonSubmit: the handler sets the answer to
              // close with, or leaves it 0.
              const TButtonSubmitEvent OnSubmit = Alias.OnSubmit;
              Button.OnSubmit = [OnSubmit]()
              {
                unsigned int Answer = 0;
                OnSubmit(NULL, Answer);
                return Answer;
              };
            }
            break;
          }
        }
      }
      Query.Buttons.push_back(Button);
    }
  }
  Query.CancelAnswer = CancelAnswer(Answers);
  if (Params != NULL)
  {
    Query.NeverAskAgainCheck = FLAGSET(Params->Params, qpNeverAskAgainCheck);
    if (Params->Timeout > 0)
    {
      Query.Timeout = Params->Timeout;
      Query.TimeoutAnswer = Params->TimeoutAnswer;
    }
  }
  const TQueryResult Result = GFrontend->Query(Query);
  // WinInterface.cpp's MessageDialog: a positive answer with the box ticked
  // is qaNeverAskAgain.
  if (Query.NeverAskAgainCheck && Result.NeverAskAgain && IsPositiveAnswer(Result.Answer))
  {
    return qaNeverAskAgain;
  }
  return Result.Answer;
}

// As upstream: the GUI's configuration chooses and loads the translation
// (TGUIConfiguration::LoadNewResourceModule, ADR 0014); here only for this
// run.
std::string SetLanguage(const std::string & Locale, const std::string & Directory)
{
  TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
  if (Shell == NULL)
  {
    return std::string();
  }
  if (!Directory.empty())
  {
    Shell->SetTranslationsDirectory(FromUtf8(Directory));
  }
  LCID Lcid = Shell->AppliedLocale;
  if (!Locale.empty())
  {
    const UnicodeString Name = FromUtf8(Locale);
    const bool English = SameText(CopyToChar(ReplaceStr(Name, L"_", L"-"), L'-', false), L"en") ||
      SameText(Name, L"C") || SameText(Name, L"POSIX");
    Lcid = English ? MAKELANGID(0x09, SUBLANG_DEFAULT) : LinscpLocaleOfName(Name);
  }
  if (Lcid != 0)
  {
    Shell->OverrideLocale(Lcid);
  }
  return LanguageCode();
}

std::string LanguageCode()
{
  if ((GUIConfiguration == NULL) || GUIConfiguration->UsingInternalTranslation())
  {
    return std::string();
  }
  return Utf8(UpperCase(ExtractFileExt(LinscpLastTranslationFile()).SubString(2, 3)));
}

std::string LanguageLocale()
{
  return (GUIConfiguration != NULL) ? Utf8(LinscpLocaleName(GUIConfiguration->AppliedLocale)) : std::string();
}

namespace
{

TShellConfiguration * Shell()
{
  TShellConfiguration * Result = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
  if (Result == NULL)
  {
    throw std::logic_error("the shell's configuration is not there");
  }
  return Result;
}

}  // namespace

TPreferences Preferences()
{
  const TShellSettings & Settings = Shell()->Settings();
  TPreferences Result;
  Result.ShowHiddenFiles = Settings.ShowHiddenFiles;
  Result.SizeFormat = static_cast<TPreferences::TSizeFormat>(Settings.FormatSizeBytes);
  Result.NaturalOrderNumericalSorting = Settings.NaturalOrderNumericalSorting;
  Result.AlwaysSortDirectoriesByName = Settings.AlwaysSortDirectoriesByName;
  Result.RenameWholeName = Settings.RenameWholeName;
  Result.PanelSearch = static_cast<TPreferences::TPanelSearch>(std::clamp(Settings.PanelSearch, 0, 2));
  Result.PreservePanelState = Settings.PreservePanelState;
  Result.FullRowSelect = Settings.FullRowSelect;
  Result.DoubleClickAction = static_cast<TPreferences::TDoubleClick>(std::clamp(Settings.DoubleClickAction, 0, 2));
  Result.CopyOnDoubleClickConfirmation = Settings.CopyOnDoubleClickConfirmation;
  Result.PanelFontName = Utf8(Settings.PanelFontName);
  Result.PanelFontSize = Settings.PanelFontSize;
  Result.PanelFontBold = FLAGSET(Settings.PanelFontStyle, 1);
  Result.PanelFontItalic = FLAGSET(Settings.PanelFontStyle, 2);
  // TNortonLikeMode: nlOn, nlOff, nlKeyboard.
  Result.ExplorerSelection = (Settings.NortonLikeMode == 0) ? TPreferences::TExplorerSelection::Never :
    ((Settings.NortonLikeMode == 1) ? TPreferences::TExplorerSelection::MouseAndKeyboard : TPreferences::TExplorerSelection::MouseOnly);
  Result.DefaultDirIsHome = Settings.DefaultDirIsHome;
  Result.SwappedPanels = Settings.SwappedPanels;
  Result.ExplorerKeyboardShortcuts = Settings.ExplorerKeyboardShortcuts;
  Result.CompareByTime = Settings.CompareByTime;
  Result.CompareBySize = Settings.CompareBySize;
  Result.UseLocationProfiles = Settings.UseLocationProfiles;
  Result.PathInCaption = static_cast<TPreferences::TPathInCaption>(Settings.PathInCaption);
  Result.IconSet = Utf8(Settings.IconSet);
  Result.KeepOpenWhenNoSession = Settings.KeepOpenWhenNoSession;
  // As TFileColorData::LoadList: comma text of "BBGGRR:mask" (TColor).
  std::unique_ptr<TStringList> Colors(CommaTextToStringList(Settings.FileColors));
  for (int Index = 0; Index < Colors->Count; Index++)
  {
    UnicodeString Item = Colors->Strings[Index];
    const unsigned Bgr = static_cast<unsigned>(StrToIntDef(UnicodeString(L"$") + CutToChar(Item, L':', true), 0));
    TPreferences::TFileColor Color;
    Color.Mask = Utf8(Item);
    Color.Color = ((Bgr & 0xff) << 16) | (Bgr & 0xff00) | ((Bgr >> 16) & 0xff);
    Result.FileColors.push_back(Color);
  }
  // As TPreferencesDialog::LoadConfiguration.
  Result.QueueTransfersLimit = Configuration->QueueTransfersLimit;
  Result.QueueKeepDoneItemsFor = GUIConfiguration->QueueKeepDoneItems ? GUIConfiguration->QueueKeepDoneItemsFor : 0;
  Result.QueueAutoPopup = GUIConfiguration->QueueAutoPopup;
  Result.SessionReopenAuto = Configuration->SessionReopenAuto / MSecsPerSec;
  Result.SessionReopenAutoIdleOn = GUIConfiguration->SessionReopenAutoIdleOn && (GUIConfiguration->SessionReopenAutoIdle > 0);
  Result.SessionReopenAutoIdle = GUIConfiguration->SessionReopenAutoIdle / MSecsPerSec;
  Result.SessionReopenAutoInactive = GUIConfiguration->SessionReopenAutoInactive;
  Result.SessionReopenAutoStall = Configuration->SessionReopenAutoStall / MSecsPerSec;
  Result.SessionReopenTimeout = Configuration->SessionReopenTimeout / MSecsPerSec;
  // Logging, Network, Security, Storage: as TPreferencesDialog::LoadConfiguration.
  Result.Logging = Configuration->Logging;
  Result.LogLevel = Configuration->LogProtocol + 1;
  Result.LogFileName = Utf8(Configuration->LogFileName);
  Result.DefaultLogFileName = Utf8(Configuration->DefaultLogFileName);
  Result.LogFileAppend = Configuration->LogFileAppend;
  Result.LogSensitive = Configuration->LogSensitive;
  Result.LogMaxSize = Configuration->LogMaxSize;
  Result.LogMaxCount = Configuration->LogMaxCount;
  Result.LogActions = Configuration->LogActions;
  Result.ActionsLogFileName = Utf8(Configuration->ActionsLogFileName);
  Result.ExternalIpAddress = Utf8(Configuration->ExternalIpAddress);
  Result.LocalPortNumberMin = Configuration->LocalPortNumberMin;
  Result.LocalPortNumberMax = Configuration->LocalPortNumberMax;
  Result.TryFtpWhenSshFails = Configuration->TryFtpWhenSshFails;
  Result.SessionRememberPassword = GUIConfiguration->SessionRememberPassword;
  Result.PuttyPath = Utf8(GUIConfiguration->PuttyPath);
  Result.PuttyPassword = GUIConfiguration->PuttyPassword;
  Result.TelnetForFtpInPutty = GUIConfiguration->TelnetForFtpInPutty;
  Result.AutoOpenInPutty = Settings.AutoOpenInPutty;
  Result.PuttyRegistryStorageKey = Utf8(Configuration->PuttyRegistryStorageKey);
  Result.AuthAgent = Configuration->AuthAgent;
  if (Configuration->Storage == stNul)
  {
    Result.Storage = TPreferences::TStorage::None;
  }
  else if (!Configuration->ExplicitIniFileStorageName.IsEmpty())
  {
    Result.Storage = TPreferences::TStorage::Explicit;
  }
  else if (Configuration->Storage == stRegistry)
  {
    Result.Storage = TPreferences::TStorage::Registry;
  }
  else
  {
    Result.Storage = Configuration->CustomIniFileStorageName.IsEmpty() ?
      TPreferences::TStorage::AutomaticIni : TPreferences::TStorage::CustomIni;
  }
  Result.StorageFile = Utf8((Result.Storage == TPreferences::TStorage::Explicit) ? Configuration->ExplicitIniFileStorageName :
    ExpandEnvironmentVariables(Configuration->GetAutomaticIniFileStorageName(false)));
  Result.CustomIniFile = Utf8(!Configuration->CustomIniFileStorageName.IsEmpty() ? Configuration->CustomIniFileStorageName :
    Configuration->GetDefaultIniFileExportPath());
  Result.RandomSeedFile = Utf8(Configuration->RandomSeedFile);
  Result.TemporaryDirectory = Utf8(Settings.DDTemporaryDirectory);
  return Result;
}

TEditorSettings EditorSettings()
{
  const TShellSettings & Settings = Shell()->Settings();
  TEditorSettings Result;
  Result.WindowParams = Utf8(Settings.EditorWindowParams);
  Result.WordWrap = Settings.EditorWordWrap;
  Result.TabSize = Settings.EditorTabSize;
  Result.Utf8 = (Settings.EditorEncoding == CP_UTF8);
  Result.FindText = Utf8(Settings.EditorFindText);
  Result.ReplaceText = Utf8(Settings.EditorReplaceText);
  Result.FindMatchCase = Settings.EditorFindMatchCase;
  Result.FindWholeWord = Settings.EditorFindWholeWord;
  Result.CheckNotModified = Settings.EditorCheckNotModified;
  Result.FontName = Utf8(Settings.EditorFontName);
  Result.FontSize = Settings.EditorFontSize;
  Result.FontBold = FLAGSET(Settings.EditorFontStyle, 1);
  Result.FontItalic = FLAGSET(Settings.EditorFontStyle, 2);
  Result.FontUnderline = FLAGSET(Settings.EditorFontStyle, 4);
  Result.FontStrikeOut = FLAGSET(Settings.EditorFontStyle, 8);
  // TColor is 0x00BBGGRR; 0 the default (GetWindowTextColor, GetWindowColor).
  const auto Rgb = [](int Color) { return (Color == 0) ? -1 : (((Color & 0xff) << 16) | (Color & 0xff00) | ((Color >> 16) & 0xff)); };
  Result.FontColor = Rgb(Settings.EditorFontColor);
  Result.BackgroundColor = Rgb(Settings.EditorBackgroundColor);
  Result.WarnOnLargeFile = Settings.EditorWarnOrLargeFileSize;
  Result.LargeFileSize = Settings.EditorLargeFileSize;
  return Result;
}

std::vector<TBookmarkEntry> Bookmarks(const std::string & SessionKey)
{
  TBookmarks * All = Shell()->Bookmarks();
  TBookmarkList * List = SessionKey.empty() ? All->SharedBookmarks : All->Bookmarks[FromUtf8(SessionKey)];
  std::vector<TBookmarkEntry> Result;
  for (int Index = 0; (List != NULL) && (Index < List->Count); Index++)
  {
    TBookmark * Bookmark = List->Bookmarks[Index];
    TBookmarkEntry Entry;
    Entry.Name = Utf8(Bookmark->Name);
    Entry.Local = Utf8(Bookmark->Local);
    Entry.Remote = Utf8(Bookmark->Remote);
    Entry.Node = Utf8(Bookmark->Node);
    Entry.ShortCut = static_cast<unsigned>(Bookmark->ShortCut);
    Result.push_back(Entry);
  }
  return Result;
}

std::vector<std::string> BookmarkOpenedNodes(const std::string & SessionKey)
{
  TBookmarks * All = Shell()->Bookmarks();
  TBookmarkList * List = SessionKey.empty() ? All->SharedBookmarks : All->Bookmarks[FromUtf8(SessionKey)];
  std::vector<std::string> Result;
  for (int Index = 0; (List != NULL) && (Index < List->Count); Index++)
  {
    const UnicodeString Node = List->Bookmarks[Index]->Node;
    if (!Node.IsEmpty() && List->NodeOpened[Node] &&
        (std::find(Result.begin(), Result.end(), Utf8(Node)) == Result.end()))
    {
      Result.push_back(Utf8(Node));
    }
  }
  return Result;
}

void SetBookmarks(const std::string & SessionKey, const std::vector<TBookmarkEntry> & Bookmarks,
  const std::vector<std::string> * OpenedNodes)
{
  try
  {
    const std::vector<std::string> Opened = (OpenedNodes != nullptr) ? *OpenedNodes : BookmarkOpenedNodes(SessionKey);
    std::unique_ptr<TBookmarkList> List(new TBookmarkList());
    for (const TBookmarkEntry & Entry : Bookmarks)
    {
      TBookmark * Bookmark = new TBookmark();
      Bookmark->Name = FromUtf8(Entry.Name);
      Bookmark->Local = FromUtf8(Entry.Local);
      Bookmark->Remote = FromUtf8(Entry.Remote);
      Bookmark->Node = FromUtf8(Entry.Node);
      Bookmark->ShortCut = static_cast<TShortCut>(Entry.ShortCut);
      List->Add(Bookmark);
    }
    for (const std::string & Node : Opened)
    {
      List->NodeOpened[FromUtf8(Node)] = true;
    }
    if (Bookmarks.empty())
    {
      // Modified even when emptied, so the list is written, its removal
      // with it (the flag is the list's own).
      TBookmark * Placeholder = new TBookmark();
      List->Add(Placeholder);
      List->Delete(Placeholder);
    }
    TBookmarks * All = Shell()->Bookmarks();
    if (SessionKey.empty())
    {
      All->SharedBookmarks = List.get();
    }
    else
    {
      All->Bookmarks[FromUtf8(SessionKey)] = List.get();
    }
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TGenerateCodeOptions GenerateCodeSettings()
{
  const TShellSettings & Settings = Shell()->Settings();
  TGenerateCodeOptions Result;
  // As TGenerateUrlDialog::Execute: never chosen, the user name and the
  // directory.
  Result.Components = (Settings.GenerateUrlComponents < 0) ? (1 | 8) : Settings.GenerateUrlComponents;
  Result.Target = static_cast<TGenerateCodeOptions::TTarget>(std::min(std::max(Settings.GenerateUrlCodeTarget, 0), 2));
  Result.ScriptFormat = Settings.GenerateUrlScriptFormat;
  Result.AssemblyLanguage = Settings.GenerateUrlAssemblyLanguage;
  return Result;
}

void SetGenerateCodeSettings(const TGenerateCodeOptions & Options)
{
  TShellSettings Settings = Shell()->Settings();
  Settings.GenerateUrlComponents = Options.Components;
  Settings.GenerateUrlCodeTarget = static_cast<int>(Options.Target);
  Settings.GenerateUrlScriptFormat = Options.ScriptFormat;
  Settings.GenerateUrlAssemblyLanguage = Options.AssemblyLanguage;
  try
  {
    Shell()->SetSettings(Settings);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool ConfirmCommandSession()
{
  return GUIConfiguration->ConfirmCommandSession;
}

void SetConfirmCommandSession(bool Confirm)
{
  try
  {
    GUIConfiguration->ConfirmCommandSession = Confirm;
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool UseSharedBookmarks()
{
  return Shell()->Settings().UseSharedBookmarks;
}

void SetUseSharedBookmarks(bool Use)
{
  TShellSettings Settings = Shell()->Settings();
  if (Settings.UseSharedBookmarks != Use)
  {
    Settings.UseSharedBookmarks = Use;
    try
    {
      Shell()->SetSettings(Settings);
      Configuration->SaveExplicit();
    }
    catch (Exception & E)
    {
      throw CoreErrorOf(E);
    }
  }
}

std::string DefaultEncodingName()
{
  return Utf8(::DefaultEncodingName());
}

void SetEditorSettings(const TEditorSettings & Editor)
{
  TShellSettings Settings = Shell()->Settings();
  Settings.EditorWindowParams = FromUtf8(Editor.WindowParams);
  Settings.EditorWordWrap = Editor.WordWrap;
  Settings.EditorTabSize = Editor.TabSize;
  Settings.EditorEncoding = Editor.Utf8 ? CP_UTF8 : CP_ACP;
  Settings.EditorFindText = FromUtf8(Editor.FindText);
  Settings.EditorReplaceText = FromUtf8(Editor.ReplaceText);
  Settings.EditorFindMatchCase = Editor.FindMatchCase;
  Settings.EditorFindWholeWord = Editor.FindWholeWord;
  Settings.EditorCheckNotModified = Editor.CheckNotModified;
  Settings.EditorFontName = FromUtf8(Editor.FontName);
  Settings.EditorFontSize = Editor.FontSize;
  Settings.EditorFontStyle = FLAGMASK(Editor.FontBold, 1) | FLAGMASK(Editor.FontItalic, 2) |
    FLAGMASK(Editor.FontUnderline, 4) | FLAGMASK(Editor.FontStrikeOut, 8);
  const auto Bgr = [](int Color) { return (Color < 0) ? 0 : (((Color & 0xff) << 16) | (Color & 0xff00) | ((Color >> 16) & 0xff)); };
  Settings.EditorFontColor = Bgr(Editor.FontColor);
  Settings.EditorBackgroundColor = Bgr(Editor.BackgroundColor);
  Settings.EditorWarnOrLargeFileSize = Editor.WarnOnLargeFile;
  Settings.EditorLargeFileSize = Editor.LargeFileSize;
  try
  {
    Shell()->SetSettings(Settings);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::int64_t SizeOfText(const std::string & Text)
{
  __int64 Size = 0;
  return TryStrToSize(FromUtf8(Text), Size) ? Size : 0;
}

std::string TextOfSize(std::int64_t Size)
{
  return Utf8(SizeToStr(Size));
}

std::string TemporaryDirectoryBase()
{
  return Utf8(Shell()->ExpandedTemporaryDirectory());
}

void SetPreferences(const TPreferences & Preferences)
{
  // What the preferences do not set stays (the layout, the internal
  // editor's settings).
  TShellSettings Settings = Shell()->Settings();
  Settings.ShowHiddenFiles = Preferences.ShowHiddenFiles;
  Settings.FormatSizeBytes = static_cast<int>(Preferences.SizeFormat);
  Settings.NaturalOrderNumericalSorting = Preferences.NaturalOrderNumericalSorting;
  Settings.AlwaysSortDirectoriesByName = Preferences.AlwaysSortDirectoriesByName;
  Settings.RenameWholeName = Preferences.RenameWholeName;
  Settings.PanelSearch = static_cast<int>(Preferences.PanelSearch);
  Settings.PreservePanelState = Preferences.PreservePanelState;
  Settings.FullRowSelect = Preferences.FullRowSelect;
  Settings.DoubleClickAction = static_cast<int>(Preferences.DoubleClickAction);
  Settings.CopyOnDoubleClickConfirmation = Preferences.CopyOnDoubleClickConfirmation;
  Settings.PanelFontName = FromUtf8(Preferences.PanelFontName);
  Settings.PanelFontSize = Preferences.PanelFontSize;
  Settings.PanelFontStyle = FLAGMASK(Preferences.PanelFontBold, 1) | FLAGMASK(Preferences.PanelFontItalic, 2);
  Settings.NortonLikeMode = (Preferences.ExplorerSelection == TPreferences::TExplorerSelection::Never) ? 0 :
    ((Preferences.ExplorerSelection == TPreferences::TExplorerSelection::MouseAndKeyboard) ? 1 : 2);
  Settings.DefaultDirIsHome = Preferences.DefaultDirIsHome;
  Settings.SwappedPanels = Preferences.SwappedPanels;
  Settings.ExplorerKeyboardShortcuts = Preferences.ExplorerKeyboardShortcuts;
  Settings.CompareByTime = Preferences.CompareByTime;
  Settings.CompareBySize = Preferences.CompareBySize;
  Settings.UseLocationProfiles = Preferences.UseLocationProfiles;
  Settings.AutoOpenInPutty = Preferences.AutoOpenInPutty;
  Settings.PathInCaption = static_cast<int>(Preferences.PathInCaption);
  Settings.IconSet = FromUtf8(Preferences.IconSet);
  Settings.KeepOpenWhenNoSession = Preferences.KeepOpenWhenNoSession;
  // As TFileColorData::SaveList.
  std::unique_ptr<TStringList> Colors(new TStringList());
  for (const TPreferences::TFileColor & Color : Preferences.FileColors)
  {
    const unsigned Bgr = ((Color.Color & 0xff) << 16) | (Color.Color & 0xff00) | ((Color.Color >> 16) & 0xff);
    Colors->Add(IntToHex(static_cast<int>(Bgr), 6) + L":" + FromUtf8(Color.Mask));
  }
  Settings.FileColors = Colors->CommaText;
  Settings.DDTemporaryDirectory = FromUtf8(Preferences.TemporaryDirectory);
  try
  {
    Shell()->SetSettings(Settings);
    // As TPreferencesDialog::SaveConfiguration.
    Configuration->QueueTransfersLimit = Preferences.QueueTransfersLimit;
    GUIConfiguration->QueueKeepDoneItems = (Preferences.QueueKeepDoneItemsFor != 0);
    GUIConfiguration->QueueKeepDoneItemsFor = Preferences.QueueKeepDoneItemsFor;
    GUIConfiguration->QueueAutoPopup = Preferences.QueueAutoPopup;
    Configuration->SessionReopenAuto = Preferences.SessionReopenAuto * MSecsPerSec;
    GUIConfiguration->SessionReopenAutoIdleOn = Preferences.SessionReopenAutoIdleOn;
    GUIConfiguration->SessionReopenAutoIdle = Preferences.SessionReopenAutoIdle * MSecsPerSec;
    GUIConfiguration->SessionReopenAutoInactive = Preferences.SessionReopenAutoInactive;
    Configuration->SessionReopenAutoStall = Preferences.SessionReopenAutoStall * MSecsPerSec;
    Configuration->SessionReopenTimeout = Preferences.SessionReopenTimeout * MSecsPerSec;
    // As TPreferencesDialog::SaveConfiguration: logging only with a file.
    Configuration->Logging = Preferences.Logging && !Preferences.LogFileName.empty();
    Configuration->LogProtocol = Preferences.LogLevel - 1;
    Configuration->LogFileName = FromUtf8(Preferences.LogFileName);
    Configuration->LogFileAppend = Preferences.LogFileAppend;
    Configuration->LogSensitive = Preferences.LogSensitive;
    Configuration->LogMaxSize = Preferences.LogMaxSize;
    Configuration->LogMaxCount = Preferences.LogMaxCount;
    Configuration->LogActions = Preferences.LogActions;
    Configuration->ActionsLogFileName = FromUtf8(Preferences.ActionsLogFileName);
    Configuration->ExternalIpAddress = FromUtf8(Preferences.ExternalIpAddress);
    Configuration->LocalPortNumberMin = Preferences.LocalPortNumberMin;
    Configuration->LocalPortNumberMax = Preferences.LocalPortNumberMax;
    Configuration->TryFtpWhenSshFails = Preferences.TryFtpWhenSshFails;
    GUIConfiguration->SessionRememberPassword = Preferences.SessionRememberPassword;
    GUIConfiguration->PuttyPath = FromUtf8(Preferences.PuttyPath);
    GUIConfiguration->PuttyPassword = Preferences.PuttyPassword;
    GUIConfiguration->TelnetForFtpInPutty = Preferences.TelnetForFtpInPutty;
    if (!Preferences.PuttyRegistryStorageKey.empty())
    {
      Configuration->PuttyRegistryStorageKey = FromUtf8(Preferences.PuttyRegistryStorageKey);
    }
    Configuration->AuthAgent = Preferences.AuthAgent;
    Configuration->RandomSeedFile = FromUtf8(Preferences.RandomSeedFile);
    // The storage moves as the dialog's OK moves it, when it is one of the
    // three that can be chosen and another than it was.
    const TPreferences Before = CoreBridge::Preferences();
    const bool Movable = (Before.Storage == TPreferences::TStorage::Registry) ||
      (Before.Storage == TPreferences::TStorage::AutomaticIni) || (Before.Storage == TPreferences::TStorage::CustomIni);
    if (Movable &&
        ((Preferences.Storage != Before.Storage) ||
         ((Preferences.Storage == TPreferences::TStorage::CustomIni) && (Preferences.CustomIniFile != Before.CustomIniFile))))
    {
      if (Preferences.Storage == TPreferences::TStorage::Registry)
      {
        Configuration->MoveStorage(stRegistry, UnicodeString());
      }
      else if (Preferences.Storage == TPreferences::TStorage::AutomaticIni)
      {
        Configuration->MoveStorage(stIniFile, UnicodeString());
      }
      else if (Preferences.Storage == TPreferences::TStorage::CustomIni)
      {
        Configuration->MoveStorage(stIniFile, FromUtf8(Preferences.CustomIniFile));
      }
    }
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TLayout Layout()
{
  const TShellSettings & Settings = Shell()->Settings();
  TLayout Result;
  Result.WindowParams = Utf8(Settings.WindowParams);
  Result.LocalPanelWidth = Settings.LocalPanelWidth;
  Result.CurrentPanel = Settings.CurrentPanel;
  Result.LocalDirViewParams = Utf8(Settings.LocalDirViewParams);
  Result.RemoteDirViewParams = Utf8(Settings.RemoteDirViewParams);
  Result.QueueViewHeight = Settings.QueueViewHeight;
  Result.QueueViewHeightPixelsPerInch = Settings.QueueViewHeightPixelsPerInch;
  Result.QueueShow = static_cast<TLayout::TQueueShow>(std::clamp(Settings.QueueViewShow, 0, 2));
  Result.StatusBar = Settings.CommanderStatusBar;
  Result.LockToolbars = Settings.LockToolbars;
  Result.SelectiveToolbarText = Settings.SelectiveToolbarText;
  Result.LargerToolbar = Settings.LargerToolbar;
  Result.ToolbarsLayout = Utf8(Settings.CommanderToolbarsLayout);
  Result.ConsoleWindowSize = Utf8(Settings.ConsoleWindowSize);
  return Result;
}

void SetLayout(const TLayout & Layout)
{
  TShellSettings Settings = Shell()->Settings();
  Settings.WindowParams = FromUtf8(Layout.WindowParams);
  Settings.LocalPanelWidth = Layout.LocalPanelWidth;
  Settings.CurrentPanel = Layout.CurrentPanel;
  Settings.LocalDirViewParams = FromUtf8(Layout.LocalDirViewParams);
  Settings.RemoteDirViewParams = FromUtf8(Layout.RemoteDirViewParams);
  Settings.QueueViewHeight = Layout.QueueViewHeight;
  Settings.QueueViewHeightPixelsPerInch = Layout.QueueViewHeightPixelsPerInch;
  Settings.QueueViewShow = static_cast<int>(Layout.QueueShow);
  Settings.CommanderStatusBar = Layout.StatusBar;
  Settings.LockToolbars = Layout.LockToolbars;
  Settings.SelectiveToolbarText = Layout.SelectiveToolbarText;
  Settings.LargerToolbar = Layout.LargerToolbar;
  Settings.CommanderToolbarsLayout = FromUtf8(Layout.ToolbarsLayout);
  Settings.ConsoleWindowSize = FromUtf8(Layout.ConsoleWindowSize);
  try
  {
    Shell()->SetSettings(Settings);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::vector<TLocaleOption> Locales()
{
  std::vector<TLocaleOption> Result;
  TObjectList * List = GUIConfiguration->Locales;
  for (int Index = 0; Index < List->Count; Index++)
  {
    const TLocaleInfo * Info = DebugNotNull(dynamic_cast<TLocaleInfo *>(List->Items[Index]));
    TLocaleOption Option;
    Option.Locale = Info->Locale;
    Option.Name = Utf8(Info->Name);
    Option.Completeness = Info->Completeness;
    Result.push_back(Option);
  }
  return Result;
}

// As TPreferencesDialog::LoadLanguages.
unsigned PreferredLocale()
{
  const LCID Locale = GUIConfiguration->Locale;
  return (Locale != 0) ? Locale : Shell()->DefaultLocale();
}

// As TPreferencesDialog::SaveConfiguration's languages.
void SetPreferredLocale(unsigned Locale)
{
  try
  {
    Shell()->SetPreferredLocale((Locale == Shell()->DefaultLocale()) ? 0 : Locale);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

unsigned AppliedLocale()
{
  return GUIConfiguration->AppliedLocale;
}

void GuiStarted()
{
  Screen->FormCount = 1;
}

void SetArguments(const std::vector<std::string> & Arguments)
{
  // ParamStr(0) first, as TProgramParams skips it.
  UnicodeString CommandLine = ShellQuoteStr(ParamStr(0));
  for (const std::string & Argument : Arguments)
  {
    CommandLine += L" " + ShellQuoteStr(FromUtf8(Argument));
  }
  SetProgramCommandLine(CommandLine.c_str());
}

void Initialize(IFrontend * AFrontend)
{
  GFrontend = AFrontend;
  GGuiThread = GetCurrentThreadId();
  try
  {
    HeadlessInitialize();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void Finalize()
{
  // As upstream's WinSCP.cpp: GUIFinalize, before core's.
  PuttySessionsFinalize();
  HeadlessFinalize();
  GFrontend = NULL;
  GGuiThread = 0;
}

bool MaskMatches(const std::string & Masks, const std::string & FileName, bool Directory,
  std::int64_t Size, double ModifiedTime, bool AllowImplicit)
{
  try
  {
    TFileMasks Mask(FromUtf8(Masks));
    TFileMasks::TParams Params;
    Params.Size = Size;
    // TDateTime counts days from 1899-12-30; 1970 is day 25569.
    Params.Modification = TDateTime(ModifiedTime / 86400.0 + 25569.0);
    bool ImplicitMatch = false;
    return Mask.Matches(FromUtf8(FileName), false, Directory, &Params, true, ImplicitMatch) && (AllowImplicit || !ImplicitMatch);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

std::vector<std::string> LinesOf(TStrings * Strings)
{
  std::vector<std::string> Result;
  for (int Index = 0; Index < Strings->Count; Index++)
  {
    Result.push_back(Utf8(Strings->Strings[Index]));
  }
  return Result;
}

TStringList * StringsOf(const std::vector<std::string> & Lines)
{
  TStringList * Result = new TStringList();
  for (const std::string & Line : Lines)
  {
    if (!Line.empty())
    {
      Result->Add(FromUtf8(Line));
    }
  }
  return Result;
}

}  // namespace

TMaskParts SplitMask(const std::string & Mask)
{
  try
  {
    TFileMasks Masks(FromUtf8(Mask));
    TMaskParts Result;
    Result.IncludeFiles = LinesOf(Masks.IncludeFileMasksStr);
    Result.ExcludeFiles = LinesOf(Masks.ExcludeFileMasksStr);
    Result.IncludeDirectories = LinesOf(Masks.IncludeDirectoryMasksStr);
    Result.ExcludeDirectories = LinesOf(Masks.ExcludeDirectoryMasksStr);
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string ComposeMask(const TMaskParts & Parts)
{
  try
  {
    std::unique_ptr<TStrings> IncludeFiles(StringsOf(Parts.IncludeFiles));
    std::unique_ptr<TStrings> ExcludeFiles(StringsOf(Parts.ExcludeFiles));
    std::unique_ptr<TStrings> IncludeDirectories(StringsOf(Parts.IncludeDirectories));
    std::unique_ptr<TStrings> ExcludeDirectories(StringsOf(Parts.ExcludeDirectories));
    const UnicodeString Result = TFileMasks::ComposeMaskStr(IncludeFiles.get(), ExcludeFiles.get(),
      IncludeDirectories.get(), ExcludeDirectories.get());
    // As the dialog's preview: what does not parse is core's error.
    TFileMasks Check(Result);
    return Utf8(Result);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string NormalizeMask(const std::string & Mask)
{
  return Utf8(TFileMasks::NormalizeMask(FromUtf8(Mask)));
}

std::string MaskFileName(const std::string & FileName, const std::string & Mask)
{
  return Utf8(::MaskFileName(FromUtf8(FileName), FromUtf8(Mask)));
}

bool MaskMatchesPath(const std::string & Masks, const std::string & Path, bool Local, bool Directory,
  std::int64_t Size, double ModifiedTime)
{
  try
  {
    TFileMasks Mask(FromUtf8(Masks));
    TFileMasks::TParams Params;
    Params.Size = Size;
    Params.Modification = TDateTime(ModifiedTime / 86400.0 + 25569.0);
    bool ImplicitMatch = false;
    return Mask.Matches(FromUtf8(Path), Local, Directory, &Params, false, ImplicitMatch) && !ImplicitMatch;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string Text(TText Text)
{
  int Ident;
  switch (Text)
  {
    case TText::NewSiteNode: Ident = LOGIN_NEW_SITE_NODE; break;
    case TText::SaveSiteCaption: Ident = SAVE_SESSION_CAPTION; break;
    case TText::SaveSitePrompt: Ident = SAVE_SESSION_PROMPT; break;
    case TText::SaveSiteFolder: Ident = SAVE_SESSION_FOLDER; break;
    case TText::SaveSitePassword: Ident = SAVE_SESSION_PASSWORD; break;
    case TText::ConfirmOverwriteSite: Ident = CONFIRM_OVERWRITE_SESSION; break;
    case TText::ConfirmDeleteSite: Ident = CONFIRM_DELETE_SESSION; break;
    case TText::FtpsImplicit: Ident = FTPS_IMPLICIT; break;
    case TText::FtpsExplicit: Ident = FTPS_EXPLICIT; break;
    case TText::NewFolderCaption: Ident = LOGIN_NEW_SESSION_FOLDER_CAPTION; break;
    case TText::NewFolderPrompt: Ident = LOGIN_NEW_SESSION_FOLDER_PROMPT; break;
    case TText::ConfirmDeleteFolder: Ident = LOGIN_DELETE_SESSION_FOLDER; break;
    case TText::ConfirmDeleteWorkspace: Ident = LOGIN_DELETE_WORKSPACE; break;
    case TText::ConfirmDeleteFile: Ident = CONFIRM_DELETE_FILE; break;
    case TText::ConfirmDeleteFiles: Ident = CONFIRM_DELETE_FILES; break;
    case TText::NewFolderName: Ident = NEW_FOLDER; break;
    case TText::ConfirmRecycleFile: Ident = CONFIRM_RECYCLE_FILE; break;
    case TText::ConfirmRecycleFiles: Ident = CONFIRM_RECYCLE_FILES; break;
    case TText::ChangeDirError: Ident = CHANGE_DIR_ERROR; break;
    case TText::CreateDirError: Ident = CREATE_DIR_ERROR; break;
    case TText::RenameFileError: Ident = RENAME_FILE_ERROR; break;
    case TText::DeleteLocalFileError: Ident = DELETE_LOCAL_FILE_ERROR; break;
    case TText::FileInfoFormat: Ident = FILE_INFO_FORMAT; break;
    case TText::FileInfoHidden: Ident = FILE_INFO_HIDDEN2; break;
    case TText::CopyParamDefault: Ident = COPY_PARAM_DEFAULT; break;
    case TText::CopyParamDuplicate: Ident = COPY_PARAM_DUPLICATE; break;
    case TText::WinScpCopyright: Ident = WINSCP_COPYRIGHT; break;
    case TText::PropertiesFile: Ident = PROPERTIES_FILE; break;
    case TText::PropertiesFiles: Ident = PROPERTIES_FILES; break;
    case TText::PropertiesDirectory: Ident = PROPERTIES_DIRECTORY; break;
    case TText::PropertiesDirectories: Ident = PROPERTIES_DIRECTORIES; break;
    case TText::PropertiesSymlink: Ident = PROPERTIES_SYMLINK; break;
    case TText::PropertiesSymlinks: Ident = PROPERTIES_SYMLINKS; break;
    case TText::PropertiesFileCaption: Ident = PROPERTIES_FILE_CAPTION; break;
    case TText::PropertiesFilesCaption: Ident = PROPERTIES_FILES_CAPTION; break;
    case TText::PropertiesUnknownSize: Ident = PROPERTIES_UNKNOWN_SIZE; break;
    case TText::PropertiesChecksumUnknown: Ident = PROPERTIES_CHECKSUM_UNKNOWN; break;
    case TText::LinkAddCaption: Ident = LINK_ADD_CAPTION; break;
    case TText::LinkEditCaption: Ident = LINK_EDIT_CAPTION; break;
    case TText::CreateLinkError: Ident = CREATE_LINK_ERROR; break;
    case TText::RemoteCopyTitle: Ident = REMOTE_COPY_TITLE; break;
    case TText::RemoteMoveTitle: Ident = REMOTE_MOVE_TITLE; break;
    case TText::RemoteTransferPrompt: Ident = REMOTE_TRANSFER_PROMPT2; break;
    case TText::OpenDirectoryCaption: Ident = OPEN_DIRECTORY_BROWSE_CAPTION; break;
    case TText::ManageBookmarksCaption: Ident = OPEN_DIRECTORY_ADD_BOOMARK_ACTION; break;
    case TText::AddBookmarkConfirm: Ident = ADD_BOOKMARK_CONFIRM; break;
    case TText::AddBookmarkShared: Ident = ADD_BOOKMARK_SHARED; break;
    case TText::BookmarkInvalidName: Ident = BOOKMARK_INVALID_NAME; break;
    case TText::BookmarkFolderInvalidName: Ident = BOOKMARK_FOLDER_INVALID_NAME; break;
    case TText::DuplicateBookmarkFolder: Ident = DUPLICATE_BOOKMARK_FOLDER; break;
    case TText::DeleteBookmarkFolder: Ident = DELETE_BOOKMARK_FOLDER; break;
    case TText::AddBookmarkCaption: Ident = ADD_BOOKMARK_CAPTION; break;
    case TText::AddBookmarkPrompt: Ident = ADD_BOOKMARK_PROMPT; break;
    case TText::MoveBookmarkCaption: Ident = MOVE_BOOKMARK_CAPTION; break;
    case TText::MoveBookmarkPrompt: Ident = MOVE_BOOKMARK_PROMPT; break;
    case TText::BookmarkInfoTip: Ident = BOOKMARK_INFO_TIP; break;
    case TText::SiteRawCaption: Ident = SITE_RAW_CAPTION; break;
    case TText::SiteRawAdd: Ident = SITE_RAW_ADD; break;
    case TText::SiteRawAddCaption: Ident = SITE_RAW_ADD_CAPTION; break;
    case TText::SiteRawAddLabel: Ident = SITE_RAW_ADD_LABEL; break;
    case TText::ImportCaption: Ident = IMPORT_CAPTION; break;
    case TText::ImportIniTitle: Ident = IMPORT_INI_TITLE; break;
    case TText::CustomCommandAdHoc: Ident = CUSTOM_COMMAND_AD_HOC; break;
    case TText::CustomCommandAdd: Ident = CUSTOM_COMMAND_ADD; break;
    case TText::CustomCommandEdit: Ident = CUSTOM_COMMAND_EDIT; break;
    case TText::CustomCommandInvalid: Ident = CUSTOM_COMMAND_INVALID; break;
    case TText::CustomCommandDuplicate: Ident = CUSTOM_COMMAND_DUPLICATE; break;
    case TText::CustomCommandAdHocName: Ident = CUSTOM_COMMAND_AD_HOC_NAME; break;
    case TText::CustomCommandLast: Ident = CUSTOM_COMMAND_LAST; break;
    case TText::CustomCommandPatternsHint: Ident = CUSTOM_COMMAND_PATTERNS_HINT5; break;
    case TText::CustomCommandPatternsHint6: Ident = CUSTOM_COMMAND_PATTERNS_HINT6; break;
    case TText::PatternsHintK: Ident = PATTERNS_HINT_K; break;
    case TText::PatternsHintBackslash: Ident = PATTERNS_HINT_BACKSLASH; break;
    case TText::ShortCutNone: Ident = SHORTCUT_NONE; break;
    case TText::ShortCutCaption: Ident = SHORTCUT_CAPTION; break;
    case TText::ShortCutLabel: Ident = SHORTCUT_LABEL; break;
    case TText::CustomCommandsParamsTitle: Ident = CUSTOM_COMMANDS_PARAMS_TITLE; break;
    case TText::CustomCommandsParamPrompt: Ident = CUSTOM_COMMANDS_PARAM_PROMPT2; break;
    case TText::FindFileTitle: Ident = FIND_FILE_TITLE; break;
    case TText::FindFileFinding: Ident = FIND_FILE_FINDING; break;
    case TText::FindFileInDirectory: Ident = FIND_FILE_IN_DIRECTORY; break;
    case TText::FindFileDone: Ident = FIND_FILE_DONE; break;
    case TText::FindFileAborted: Ident = FIND_FILE_ABORTED; break;
    case TText::FindFileStart: Ident = FIND_FILE_START; break;
    case TText::FindFileStop: Ident = FIND_FILE_STOP; break;
    case TText::SelectMaskSelectCaption: Ident = SELECT_MASK_SELECT_CAPTION; break;
    case TText::SelectMaskDeselectCaption: Ident = SELECT_MASK_DESELECT_CAPTION; break;
    case TText::FilterMaskCaption: Ident = FILTER_MASK_CAPTION; break;
    case TText::CopyLocalCopyCaption: Ident = COPY_LOCAL_COPY_CAPTION; break;
    case TText::CopyLocalMoveCaption: Ident = COPY_LOCAL_MOVE_CAPTION; break;
    case TText::CreateLocalDirectory: Ident = CREATE_LOCAL_DIRECTORY; break;
    case TText::CreateLocalDirectoryError: Ident = CREATE_LOCAL_DIR_ERROR; break;
    case TText::SelectLocalDirectory: Ident = SELECT_LOCAL_DIRECTORY; break;
    case TText::FileOverwrite: Ident = FILE_OVERWRITE; break;
    case TText::SaveSyncMode: Ident = SAVE_SYNCHRONIZE_MODE2; break;
    case TText::SaveSessionCaption: Ident = SAVE_SESSION_CAPTION; break;
    case TText::SaveSessionPrompt: Ident = SAVE_SESSION_PROMPT; break;
    case TText::SaveSessionFolder: Ident = SAVE_SESSION_FOLDER; break;
    case TText::SaveSessionRootFolder: Ident = SAVE_SESSION_ROOT_FOLDER2; break;
    case TText::SaveSessionPassword: Ident = SAVE_SESSION_PASSWORD; break;
    case TText::SaveSessionPasswordRecommended: Ident = SAVE_SESSION_PASSWORD_RECOMMENDED; break;
    case TText::RenameTabTitle: Ident = RENAME_TAB_TITLE; break;
    case TText::RenameTabPrompt: Ident = RENAME_TAB_PROMPT; break;
    case TText::PasswordChanged: Ident = PASSWORD_CHANGED; break;
    case TText::PublicKeyTitle: Ident = LOGIN_PUBLIC_KEY_TITLE; break;
    case TText::NotOpenSsh: Ident = LOGIN_NOT_OPENSSH; break;
    case TText::CompareNoDifferences: Ident = COMPARE_NO_DIFFERENCES; break;
    case TText::SyncBrowseCreate: Ident = SYNC_DIR_BROWSE_CREATE2; break;
    case TText::SyncBrowseError: Ident = SYNC_DIR_BROWSE_ERROR; break;
    case TText::SyncBrowseToggle: Ident = SYNC_DIR_BROWSE_TOGGLE; break;
    case TText::NewFileName: Ident = NEW_FILE; break;
    case TText::EditFileCaption: Ident = EDIT_FILE_CAPTION; break;
    case TText::EditFilePrompt: Ident = EDIT_FILE_PROMPT; break;
    case TText::CreateFileError: Ident = CREATE_FILE_ERROR; break;
    case TText::UnsafeSession: Ident = UNSAFE_SESSION; break;
    case TText::IncrementalSearch: Ident = INC_SEARCH; break;
    case TText::MaskHint: Ident = MASK_HINT2; break;
    case TText::FileMaskExHint: Ident = FILE_MASK_EX_HINT; break;
    case TText::CombiningMasksHint: Ident = COMBINING_MASKS_HINT; break;
    case TText::PathMaskHint: Ident = PATH_MASK_HINT2; break;
    case TText::DirectoryMaskHint: Ident = DIRECTORY_MASK_HINT; break;
    case TText::NotSupported: Ident = NOTSUPPORTED; break;
    case TText::LicenseCaption: Ident = LICENSE_CAPTION; break;
    case TText::TagAdd: Ident = TAG_ADD; break;
    case TText::TagEdit: Ident = TAG_EDIT; break;
    case TText::TagKey: Ident = TAG_KEY; break;
    case TText::TagValue: Ident = TAG_VALUE; break;
    case TText::TagNotUnique: Ident = TAG_NOT_UNIQUE; break;
    case TText::TagInvalidChar: Ident = TAG_INVALID_CHAR; break;
    case TText::EditorLargeFile: Ident = INTERNAL_EDITOR_LARGE_FILE2; break;
    case TText::EditorAdd: Ident = EDITOR_ADD; break;
    case TText::EditorEdit: Ident = EDITOR_EDIT; break;
    case TText::EditorAdHoc: Ident = EDITOR_AD_HOC; break;
    case TText::SelectExternalEditor: Ident = PREFERENCES_SELECT_EXTERNAL_EDITOR; break;
    case TText::ConfirmRegisterUrl: Ident = CONFIRM_REGISTER_URL2; break;
    case TText::ConfirmUnregisterUrl: Ident = CONFIRM_UNREGISTER_URL; break;
    case TText::ConfirmAddSearchPath: Ident = CONFIRM_ADD_SEARCH_PATH; break;
    case TText::CreateDesktopIconAllUsers: Ident = CREATE_DESKTOP_ICON2; break;
    case TText::RegisterUrlError: Ident = REGISTER_URL_ERROR2; break;
    case TText::CreateShortcutError: Ident = CREATE_SHORTCUT_ERROR; break;
    case TText::SaveWorkspaceCaption: Ident = SAVE_WORKSPACE_CAPTION; break;
    case TText::SaveWorkspacePrompt: Ident = SAVE_WORKSPACE_PROMPT; break;
    case TText::SaveWorkspacePasswords: Ident = SAVE_WORKSPACE_PASSWORDS; break;
    case TText::SaveWorkspacePasswordsRecommended: Ident = SAVE_WORKSPACE_PASSWORDS_RECOMMENDED; break;
    case TText::SaveWorkspaceAuto: Ident = SAVE_WORKSPACE_AUTO; break;
    case TText::NewWorkspace: Ident = NEW_WORKSPACE; break;
    default: return std::string();
  }
  return Utf8(LoadStr(Ident));
}

const char ExpatLicenseUrl[] = "license:expat";

void PuttyCommandPatterns(const std::string & Command, bool & SiteCommand, bool & PasswordCommand)
{
  SiteCommand = false;
  PasswordCommand = false;
  try
  {
    TRemoteCustomCommand RemoteCustomCommand;
    TInteractiveCustomCommand InteractiveCustomCommand(&RemoteCustomCommand);
    const UnicodeString PuttyPath = InteractiveCustomCommand.Complete(FromUtf8(Command), false);
    SiteCommand = RemoteCustomCommand.IsSiteCommand(PuttyPath);
    PasswordCommand = RemoteCustomCommand.IsPasswordCommand(PuttyPath);
  }
  catch (...)
  {
    // As upstream: neither.
  }
}

std::string PuttyPatternsHint()
{
  std::unique_ptr<TStrings> Hint(TextToStringList(LoadStr(PUTTY_PATTERNS_HINT2)));
  Hint->Insert(7, LoadStr(PATTERNS_HINT_K));
  return Utf8(TrimRight(StringsToText(Hint.get())));
}

std::vector<std::string> PuttyRegistryStorageKeys()
{
  return { Utf8(OriginalPuttyRegistryStorageKey), Utf8(KittyRegistryStorageKey) };
}

std::string CustomCommandSide(bool Local)
{
  return Utf8(LoadStr(Local ? CUSTOM_COMMAND_LOCAL : CUSTOM_COMMAND_REMOTE));
}

struct TCleanupData::TImpl
{
  std::vector<std::function<void()>> Events;
};

TCleanupData::TCleanupData() :
  F(new TImpl())
{
  try
  {
    const auto Add = [this](int CaptionId, const UnicodeString & Location, std::function<void()> Event)
    {
      FItems.push_back(TItem{ Utf8(LoadStr(CaptionId)), Utf8(Location) });
      F->Events.push_back(std::move(Event));
      FAnyData = true;
    };
    const auto Registry = [](const UnicodeString & Location)
    {
      return Configuration->RootKeyStr + L'\\' + Configuration->RegistryStorageKey + L'\\' + Location;
    };
    // Listed regardless, for its side effect (the configuration is not
    // saved), but data only when it exists.
    Add(CLEANUP_CONFIG, Registry(Configuration->ConfigurationSubKey), []() { Configuration->CleanupConfiguration(); });
    FAnyData = Configuration->RegistryPathExists(Configuration->ConfigurationSubKey);
    if (Configuration->RegistryPathExists(Configuration->StoredSessionsSubKey))
    {
      Add(CLEANUP_SESSIONS, Registry(Configuration->StoredSessionsSubKey), []() { StoredSessions->Cleanup(); });
    }
    if (Configuration->HasAnyCache())
    {
      Add(CLEANUP_CACHES, Registry(L"..."), []() { Configuration->CleanupCaches(); });
    }
    const UnicodeString IniFilePath = ExpandEnvironmentVariables(Configuration->IniFileStorageNameForReading);
    if (FileExists(IniFilePath))
    {
      Add(CLEANUP_INIFILE, IniFilePath, []() { Configuration->CleanupIniFile(); });
    }
    if (RandomSeedExists())
    {
      Add(CLEANUP_SEEDFILE, Configuration->RandomSeedFileName, []() { Configuration->CleanupRandomSeedFile(); });
    }
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    if ((Shell != NULL) && Shell->AnyTemporaryFolders())
    {
      Add(CLEANUP_TEMP_FOLDERS, Shell->TemporaryDir(true), [Shell]() { Shell->CleanupTemporaryFolders(); });
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TCleanupData::~TCleanupData() = default;

void TCleanupData::Cleanup(std::size_t Index)
{
  try
  {
    Configuration->Usage->Inc(L"Cleanups");
    F->Events.at(Index)();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

// TAboutDialog's AddPara, CreateLink and CreateLicenseLinkLine, HTML only.
void AddPara(UnicodeString & Text, const UnicodeString & S)
{
  Text += L"<p>" + S + L"</p>\n";
}

UnicodeString HtmlLink(const UnicodeString & Url, const UnicodeString & Title = UnicodeString())
{
  return FORMAT(L"<a href=\"%s\">%s</a>", (Url, Title.IsEmpty() ? Url : Title));
}

UnicodeString LicenseLinkLine(const UnicodeString & Url)
{
  return HtmlLink(Url, LoadStr(ABOUT_THIRDPARTY_LICENSE)) + L"<br>";
}

}  // namespace

// The VCL components WinSCP lists after these (Toolbar2000, TBX, the file
// manager's, JCL, PNG components) are not linscp's.
std::string ThirdPartyHtml(const std::string & LanguageName)
{
  UnicodeString ThirdParty;
  const UnicodeString Br = L"<br>";
  if (!GUIConfiguration->UsingInternalTranslation())
  {
    const UnicodeString TranslatorUrl = LoadStr(TRANSLATOR_URL);
    const UnicodeString TranslatorInfo = LoadStr(TRANSLATOR_INFO2);
    const UnicodeString Header = ReplaceStr(LoadStr(ABOUT_TRANSLATIONS_HEADER), L"{language}", FromUtf8(LanguageName));
    AddPara(ThirdParty,
      Header + Br +
      FMTLOAD(ABOUT_TRANSLATIONS_COPYRIGHT, (GUIConfiguration->AppliedLocaleCopyright())) + Br +
      (!TranslatorInfo.IsEmpty() ? TranslatorInfo + Br : EmptyStr) +
      (!TranslatorUrl.IsEmpty() ? HtmlLink(TranslatorUrl) : EmptyStr));
  }

  AddPara(ThirdParty, LoadStr(ABOUT_THIRDPARTY_HEADER));

  AddPara(ThirdParty,
    FMTLOAD(PUTTY_BASED_ON, (GetPuTTYVersion())) + Br +
    LoadStr(PUTTY_COPYRIGHT) + Br +
    LicenseLinkLine(LoadStr(PUTTY_LICENSE_URL)) +
    HtmlLink(LoadStr(PUTTY_URL)));

  UnicodeString OpenSSLVersionText = GetOpenSSLVersionText();
  CutToChar(OpenSSLVersionText, L' ', true); // "OpenSSL"
  const UnicodeString OpenSSLVersion = CutToChar(OpenSSLVersionText, L' ', true);
  CutToChar(OpenSSLVersionText, L' ', true); // day
  CutToChar(OpenSSLVersionText, L' ', true); // month
  const UnicodeString OpenSSLYear = CutToChar(OpenSSLVersionText, L' ', true);
  AddPara(ThirdParty,
    FMTLOAD(OPENSSL_BASED_ON, (OpenSSLVersion)) + Br +
    FMTLOAD(OPENSSL_COPYRIGHT2, (OpenSSLYear)) + Br +
    HtmlLink(LoadStr(OPENSSL_URL)));

  AddPara(ThirdParty,
    LoadStr(FILEZILLA_BASED_ON2) + Br +
    LoadStr(FILEZILLA_COPYRIGHT2) + Br +
    HtmlLink(LoadStr(FILEZILLA_URL)));

  AddPara(ThirdParty,
    FMTLOAD(NEON_BASED_ON2, (NeonVersion())) + Br +
    LoadStr(NEON_COPYRIGHT) + Br +
    HtmlLink(LoadStr(NEON_URL)));

  AddPara(ThirdParty,
    FMTLOAD(S3_BASED_ON, (S3LibVersion())) + Br +
    LoadStr(S3_COPYRIGHT) + Br +
    LicenseLinkLine(LoadStr(S3_LICENSE_URL)) +
    HtmlLink(LoadStr(S3_URL)));

  AddPara(ThirdParty,
    FMTLOAD(EXPAT_BASED_ON, (ExpatVersion())) + Br +
    LicenseLinkLine(ExpatLicenseUrl) +
    HtmlLink(LoadStr(EXPAT_URL)));

  return Utf8(ThirdParty);
}

TInfo Info()
{
  TInfo Result;
  Result.AppName = Utf8(AppNameString());
  Result.Version = LINSCP_VERSION;
  Result.WinScpVersion = Utf8(Configuration->Version);
  Result.ConfigurationStorage = Utf8((Configuration->Storage == stRegistry) ?
    (L"HKCU\\" + Configuration->RegistryStorageKey) : Configuration->IniFileStorageName);
  return Result;
}

}

namespace CoreBridge
{

namespace
{

TEditorData EditorDataOf(const TEditorDefinition & Editor)
{
  TEditorData Data;
  Data.Editor = (Editor.Kind == TEditorDefinition::TKind::External) ? edExternal :
    ((Editor.Kind == TEditorDefinition::TKind::Open) ? edOpen : edInternal);
  Data.ExternalEditor = FromUtf8(Editor.Command);
  Data.FileMask = FromUtf8(Editor.Mask);
  Data.ExternalEditorText = Editor.Text;
  Data.SDIExternalEditor = Editor.Sdi;
  Data.DetectMDIExternalEditor = Editor.DetectMdi;
  return Data;
}

TEditorDefinition DefinitionOf(const TEditorPreferences & Preferences)
{
  const TEditorData * Data = Preferences.Data;
  TEditorDefinition Result;
  Result.Kind = (Data->Editor == edExternal) ? TEditorDefinition::TKind::External :
    ((Data->Editor == edOpen) ? TEditorDefinition::TKind::Open : TEditorDefinition::TKind::Internal);
  Result.Command = Utf8(Data->ExternalEditor);
  Result.Mask = Utf8(Data->FileMask.Masks);
  Result.Text = Data->ExternalEditorText;
  Result.Sdi = Data->SDIExternalEditor;
  Result.DetectMdi = Data->DetectMDIExternalEditor;
  Result.Name = Utf8(Preferences.Name);
  return Result;
}

}  // namespace

std::vector<TEditorDefinition> Editors()
{
  const TEditorList * List = Shell()->EditorList();
  std::vector<TEditorDefinition> Result;
  for (int Index = 0; Index < List->Count; Index++)
  {
    Result.push_back(DefinitionOf(*List->Editors[Index]));
  }
  return Result;
}

void SetEditors(const std::vector<TEditorDefinition> & Editors)
{
  try
  {
    std::unique_ptr<TEditorList> List(new TEditorList());
    for (const TEditorDefinition & Editor : Editors)
    {
      List->Add(new TEditorPreferences(EditorDataOf(Editor)));
    }
    Shell()->SetEditorList(List.get());
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

int FindEditor(const std::vector<TEditorDefinition> & Editors, const std::string & FileName, bool Local,
  std::int64_t Size, double ModifiedTime)
{
  try
  {
    TFileMasks::TParams Params;
    Params.Size = Size;
    // TDateTime counts days from 1899-12-30; 1970 is day 25569.
    Params.Modification = TDateTime(ModifiedTime / 86400.0 + 25569.0);
    const UnicodeString Name = FromUtf8(FileName);
    for (std::size_t Index = 0; Index < Editors.size(); Index++)
    {
      const TEditorPreferences Preferences(EditorDataOf(Editors[Index]));
      if (Preferences.Matches(Name, Local, Params))
      {
        return static_cast<int>(Index);
      }
    }
    return -1;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void StartExternalEditor(const std::string & Command, const std::string & FileName)
{
  try
  {
    UnicodeString ACommand = FromUtf8(Command);
    ReformatFileNameCommand(ACommand);
    UnicodeString Program, Params, Dir;
    SplitCommand(ACommand, Program, Params, Dir);
    Params = ExpandFileNameCommand(Params, FromUtf8(FileName));
    Program = ExpandEnvironmentVariables(Program);
    if ((Frontend() == NULL) || !Frontend()->StartProgram(Utf8(Program), Utf8(Params), std::string()))
    {
      throw EOSExtException(FMTLOAD(EDITOR_ERROR, (Program)));
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string ExecuteFileError(const std::string & FileName)
{
  return Utf8(FMTLOAD(EXECUTE_FILE_ERROR, (FromUtf8(FileName))));
}

std::string EditorName(const TEditorDefinition & Editor)
{
  return Utf8(TEditorPreferences(EditorDataOf(Editor)).Name);
}

void AutodetectEditorOptions(TEditorDefinition & Editor)
{
  TEditorData Data = EditorDataOf(Editor);
  Data.ExternalEditorOptionsAutodetect();
  Editor.Text = Data.ExternalEditorText;
  Editor.Sdi = Data.SDIExternalEditor;
}

}  // namespace CoreBridge

namespace CoreBridge
{

TWorkspaceSettings WorkspaceSettings()
{
  const TShellSettings & Settings = Shell()->Settings();
  TWorkspaceSettings Result;
  Result.AutoSave = Settings.AutoSaveWorkspace;
  Result.AutoSavePasswords = Settings.AutoSaveWorkspacePasswords;
  Result.AutoWorkspace = Utf8(Settings.AutoWorkspace);
  Result.LastStoredSession = Utf8(Settings.LastStoredSession);
  return Result;
}

void SetWorkspaceSettings(const TWorkspaceSettings & Workspace)
{
  TShellSettings Settings = Shell()->Settings();
  Settings.AutoSaveWorkspace = Workspace.AutoSave;
  Settings.AutoSaveWorkspacePasswords = Workspace.AutoSavePasswords;
  Settings.AutoWorkspace = FromUtf8(Workspace.AutoWorkspace);
  Settings.LastStoredSession = FromUtf8(Workspace.LastStoredSession);
  try
  {
    Shell()->SetSettings(Settings);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}  // namespace CoreBridge
