#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
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

namespace
{

void * GTranslation = NULL;

// WinSCP's code for a locale, where it is not the language's own two
// letters upper-cased: upstream names its translations by Windows'
// LOCALE_SABBREVLANGNAME (PTB, PTG, CHT, ...), cut to two letters where one
// translation serves the whole language.
UnicodeString LanguageCode(const UnicodeString & Language, const UnicodeString & Region)
{
  if (Language == L"pt") return (Region == L"PT") ? L"PTG" : L"PT";
  if (Language == L"zh") return ((Region == L"TW") || (Region == L"HK") || (Region == L"MO") || (Region == L"HANT")) ? L"CH" : L"CHS";
  if (Language == L"sr") return L"SRL";
  if (Language == L"id") return L"IN";
  if (Language == L"ja") return L"JP";
  if ((Language == L"fil") || (Language == L"tl")) return L"FP";
  if (Language == L"my") return L"MYA";
  if ((Language == L"nb") || (Language == L"nn")) return L"NO";
  if (Language == L"eo") return L"XXE";
  if (Language == L"kab") return L"XXK";
  return UpperCase(Language);
}

}  // namespace

std::string SetLanguage(const std::string & Locale, const std::string & ADirectory)
{
  // "pt_BR.UTF-8", "pt-BR", "zh-Hant-TW": the language, and the last part.
  UnicodeString Name = FromUtf8(Locale);
  Name = CutToChar(Name, L'.', false);
  Name = ReplaceStr(Name, L"_", L"-");
  const UnicodeString Language = LowerCase(CutToChar(Name, L'-', false));
  UnicodeString Region;
  while (!Name.IsEmpty())
  {
    Region = UpperCase(CutToChar(Name, L'-', false));
  }

  void * Translation = NULL;
  UnicodeString Code;
  if (!Language.IsEmpty() && (Language != L"en") && (Language != L"c") && (Language != L"posix"))
  {
    Code = LanguageCode(Language, Region);
    const UnicodeString Directory = !ADirectory.empty() ? FromUtf8(ADirectory) :
      (IncludeTrailingBackslash(ExtractFilePath(ParamStr(0))) + L"translations");
    Translation = LoadTranslationModule(IncludeTrailingBackslash(Directory) + L"linscp." + Code);
  }
  ChangeResourceModule(Translation);
  if (GTranslation != NULL)
  {
    FreeTranslationModule(GTranslation);
  }
  GTranslation = Translation;
  return (Translation != NULL) ? Utf8(Code) : std::string();
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
