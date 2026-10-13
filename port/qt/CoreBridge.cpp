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
  return Result;
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
    default: return std::string();
  }
  return Utf8(LoadStr(Ident));
}

TInfo Info()
{
  TInfo Result;
  Result.AppName = Utf8(AppNameString());
  Result.Version = Utf8(Configuration->Version);
  Result.ConfigurationStorage = Utf8((Configuration->Storage == stRegistry) ?
    (L"HKCU\\" + Configuration->RegistryStorageKey) : Configuration->IniFileStorageName);
  return Result;
}

}
