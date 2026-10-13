// CoreBridge::TSession: a TTerminal, and its events turned into the
// frontend's dialogs, as upstream's TTerminalManager wires them for the GUI
// (windows/TerminalManager.cpp).
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <Common.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <RemoteFiles.h>
#include <SessionData.h>
#include <Terminal.h>
#include <TextsCore.h>

#include <memory>

namespace CoreBridge
{

struct TSession::TImpl
{
  std::unique_ptr<TSessionData> Data;
  std::unique_ptr<TTerminal> Terminal;

  void __fastcall QueryUser(TObject * /*Sender*/, const UnicodeString Query, TStrings * MoreMessages,
    unsigned int Answers, const TQueryParams * Params, unsigned int & Answer, ::TQueryType QueryType,
    void * /*Arg*/)
  {
    Answer = AskFrontend(Query, MoreMessages, QueryType, Answers, Params);
  }

  void __fastcall PromptUser(TTerminal * /*Terminal*/, ::TPromptKind Kind, UnicodeString Name,
    UnicodeString Instructions, TStrings * Prompts, TStrings * Results, bool & Result, void * /*Arg*/)
  {
    TPrompt Prompt;
    Prompt.Kind = static_cast<TPromptKind>(Kind);
    Prompt.Name = Utf8(Name);
    Prompt.Instructions = Utf8(UnformatMessage(Instructions));
    for (int Index = 0; Index < Prompts->Count; Index++)
    {
      TPromptField Field;
      Field.Label = Utf8(Prompts->Strings[Index]);
      Field.Echo = FLAGSET(reinterpret_cast<intptr_t>(Prompts->Objects[Index]), pupEcho);
      if (Index < Results->Count)
      {
        Field.Value = Utf8(Results->Strings[Index]);
      }
      Prompt.Fields.push_back(Field);
    }
    Result = Frontend()->Prompt(Prompt);
    if (Result)
    {
      for (int Index = 0; Index < Prompts->Count; Index++)
      {
        const UnicodeString Value = FromUtf8(Prompt.Fields[Index].Value);
        if (Index < Results->Count)
        {
          Results->Strings[Index] = Value;
        }
        else
        {
          Results->Add(Value);
        }
      }
    }
  }

  void __fastcall DisplayBanner(TTerminal * /*Terminal*/, UnicodeString SessionName,
    const UnicodeString & Banner, bool & NeverShowAgain, int Options, unsigned int & /*Params*/)
  {
    Frontend()->Banner(Utf8(SessionName), Utf8(Banner), !FLAGSET(Options, boDisableNeverShowAgain),
      NeverShowAgain);
  }

  void __fastcall ShowExtendedException(TTerminal * /*Terminal*/, Exception * E, void * /*Arg*/)
  {
    TError Error;
    if (ErrorOf(E, Error))
    {
      Frontend()->ShowError(Error);
    }
  }

  // As the script's TerminalInformation: the progress lines while connecting.
  void __fastcall Information(TTerminal * ATerminal, const UnicodeString & Str, int Phase,
    const UnicodeString & /*Additional*/)
  {
    if ((Phase < 0) && (ATerminal->Status == ssOpening))
    {
      Frontend()->Information(Utf8(Str));
    }
  }
};

TSession::TSession(const std::string & Url) :
  F(new TImpl())
{
  try
  {
    int ParsedInfo = 0;
    F->Data.reset(StoredSessions->ParseUrl(FromUtf8(Url), NULL, ParsedInfo));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSession::~TSession()
{
  Close();
}

std::string TSession::Name() const
{
  return Utf8(F->Data->SessionName);
}

void TSession::Open()
{
  try
  {
    // As the script's open: a URL with no host asks for one.
    if (!F->Data->CanLogin)
    {
      TPrompt Prompt;
      Prompt.Kind = TPromptKind::Prompt;
      Prompt.Name = Utf8(AppNameString());
      TPromptField Field;
      Field.Label = Utf8(LoadStr(SCRIPT_HOST_PROMPT));
      Prompt.Fields.push_back(Field);
      if (!Frontend()->Prompt(Prompt) || Prompt.Fields[0].Value.empty())
      {
        Abort();
      }
      F->Data->HostName = FromUtf8(Prompt.Fields[0].Value);
    }
    F->Terminal.reset(new TTerminal(F->Data.get(), Configuration));
    TTerminal * Terminal = F->Terminal.get();
    Terminal->AutoReadDirectory = true;
    Terminal->OnQueryUser = CLOSURE_OF(F.get(), QueryUser);
    Terminal->OnPromptUser = CLOSURE_OF(F.get(), PromptUser);
    Terminal->OnDisplayBanner = CLOSURE_OF(F.get(), DisplayBanner);
    Terminal->OnShowExtendedException = CLOSURE_OF(F.get(), ShowExtendedException);
    Terminal->OnInformation = CLOSURE_OF(F.get(), Information);
    Terminal->Open();
  }
  catch (Exception & E)
  {
    F->Terminal.reset();
    throw CoreErrorOf(E);
  }
}

bool TSession::Active() const
{
  return (F->Terminal != NULL) && F->Terminal->Active;
}

std::string TSession::CurrentDirectory() const
{
  return Active() ? Utf8(F->Terminal->CurrentDirectory) : std::string();
}

std::vector<TRemoteEntry> TSession::List() const
{
  std::vector<TRemoteEntry> Result;
  if (Active() && (F->Terminal->Files != NULL))
  {
    TRemoteDirectory * Files = F->Terminal->Files;
    for (int Index = 0; Index < Files->Count; Index++)
    {
      TRemoteFile * File = Files->Files[Index];
      if (File->IsThisDirectory || File->IsParentDirectory)
      {
        continue;
      }
      TRemoteEntry Entry;
      Entry.Name = Utf8(File->FileName);
      Entry.Directory = File->IsDirectory;
      Entry.Size = File->Size;
      Entry.Modified = Utf8(File->UserModificationStr);
      Entry.Rights = Utf8(File->RightsStr);
      Result.push_back(Entry);
    }
  }
  return Result;
}

void TSession::ChangeDirectory(const std::string & Directory)
{
  try
  {
    if (!Active())
    {
      throw Exception(LoadStr(NOT_CONNECTED));
    }
    F->Terminal->ChangeDirectory(FromUtf8(Directory));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Close()
{
  if (F->Terminal != NULL)
  {
    try
    {
      if (F->Terminal->Active)
      {
        F->Terminal->Close();
      }
    }
    catch (Exception & E)
    {
      // Closing a broken connection fails as it closes: nothing to tell.
    }
    F->Terminal.reset();
  }
}

}
