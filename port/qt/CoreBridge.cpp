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
  Result.DefaultDirIsHome = Settings.DefaultDirIsHome;
  Result.SwappedPanels = Settings.SwappedPanels;
  Result.ExplorerKeyboardShortcuts = Settings.ExplorerKeyboardShortcuts;
  Result.PathInCaption = static_cast<TPreferences::TPathInCaption>(Settings.PathInCaption);
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
  Result.WordWrap = Settings.EditorWordWrap;
  Result.TabSize = Settings.EditorTabSize;
  Result.Utf8 = (Settings.EditorEncoding == CP_UTF8);
  Result.FindText = Utf8(Settings.EditorFindText);
  Result.ReplaceText = Utf8(Settings.EditorReplaceText);
  Result.FindMatchCase = Settings.EditorFindMatchCase;
  Result.FindWholeWord = Settings.EditorFindWholeWord;
  Result.CheckNotModified = Settings.EditorCheckNotModified;
  return Result;
}

std::string DefaultEncodingName()
{
  return Utf8(::DefaultEncodingName());
}

void SetEditorSettings(const TEditorSettings & Editor)
{
  TShellSettings Settings = Shell()->Settings();
  Settings.EditorWordWrap = Editor.WordWrap;
  Settings.EditorTabSize = Editor.TabSize;
  Settings.EditorEncoding = Editor.Utf8 ? CP_UTF8 : CP_ACP;
  Settings.EditorFindText = FromUtf8(Editor.FindText);
  Settings.EditorReplaceText = FromUtf8(Editor.ReplaceText);
  Settings.EditorFindMatchCase = Editor.FindMatchCase;
  Settings.EditorFindWholeWord = Editor.FindWholeWord;
  Settings.EditorCheckNotModified = Editor.CheckNotModified;
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
  TShellSettings Settings;
  Settings.ShowHiddenFiles = Preferences.ShowHiddenFiles;
  Settings.FormatSizeBytes = static_cast<int>(Preferences.SizeFormat);
  Settings.NaturalOrderNumericalSorting = Preferences.NaturalOrderNumericalSorting;
  Settings.AlwaysSortDirectoriesByName = Preferences.AlwaysSortDirectoriesByName;
  Settings.RenameWholeName = Preferences.RenameWholeName;
  Settings.DefaultDirIsHome = Preferences.DefaultDirIsHome;
  Settings.SwappedPanels = Preferences.SwappedPanels;
  Settings.ExplorerKeyboardShortcuts = Preferences.ExplorerKeyboardShortcuts;
  Settings.PathInCaption = static_cast<int>(Preferences.PathInCaption);
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
  // The layout is SetLayout's.
  const TShellSettings & Current = Shell()->Settings();
  Settings.WindowParams = Current.WindowParams;
  Settings.LocalPanelWidth = Current.LocalPanelWidth;
  Settings.CurrentPanel = Current.CurrentPanel;
  Settings.LocalDirViewParams = Current.LocalDirViewParams;
  Settings.RemoteDirViewParams = Current.RemoteDirViewParams;
  Settings.QueueViewHeight = Current.QueueViewHeight;
  Settings.QueueViewHeightPixelsPerInch = Current.QueueViewHeightPixelsPerInch;
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
  HeadlessFinalize();
  GFrontend = NULL;
  GGuiThread = 0;
}

bool MaskMatches(const std::string & Masks, const std::string & FileName, bool Directory,
  std::int64_t Size, double ModifiedTime)
{
  try
  {
    TFileMasks Mask(FromUtf8(Masks));
    TFileMasks::TParams Params;
    Params.Size = Size;
    // TDateTime counts days from 1899-12-30; 1970 is day 25569.
    Params.Modification = TDateTime(ModifiedTime / 86400.0 + 25569.0);
    bool ImplicitMatch = false;
    return Mask.Matches(FromUtf8(FileName), false, Directory, &Params, true, ImplicitMatch) && !ImplicitMatch;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
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
    default: return std::string();
  }
  return Utf8(LoadStr(Ident));
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
