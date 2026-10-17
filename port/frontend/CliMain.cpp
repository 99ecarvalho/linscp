// linscp-cli: WinSCP's scripting client on standard input and output.
//
// Upstream's is two programs: winscp.com owns the console and relays it,
// through shared memory, to WinSCP.exe /console, whose TConsoleRunner
// (windows/ConsoleRunner.cpp) runs core's TManagementScript. That runner
// cannot be compiled without the VCL forms -- its header is the GUI's
// WinInterface.h -- so this is a runner of its own over the same core
// script, with the same behaviour: the script and its output are core's;
// what lives here is how queries, prompts and input meet the console, and
// those follow TConsoleRunner and winscp.com (see StdioConsole.h). Modes
// besides scripting: /keygen (key conversion, upstream's KeyGen),
// /batchsettings, /copyid, /fingerprintscan, /info and /help.
#include <vcl.h>
#pragma hdrstop

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <FileMasks.h>
#include <FtpFileSystem.h>
#include <HierarchicalStorage.h>
#include <Interface.h>
#include <PuttyTools.h>
#include <Script.h>
#include <SessionData.h>
#include <SessionInfo.h>
#include <Terminal.h>
#include <TextsCore.h>
#include <TextsWin.h>

#include <memory>
#include <vector>

#include "FrontendCommon.h"
#include "HeadlessMain.h"
#include "ProgParams.h"
#include "DirectoryWatcher.h"
#include "ExternalConsole.h"
#include "StdioConsole.h"
//---------------------------------------------------------------------------
namespace {

// WinInterface.h's names for these, which is a GUI header.
enum { ResultSuccess = 0, ResultAnyError = 1 };
const wchar_t * const KeyGenSwitch = L"KeyGen";
const wchar_t * const KeyGenOutputSwitch = L"Output";
const wchar_t * const KeyGenCommentSwitch = L"Comment";
const wchar_t * const KeyGenChangePassphraseSwitch = L"ChangePassphrase";
const wchar_t * const KeyGenCertificateSwitch = L"Certificate";
const wchar_t * const NoInteractiveInputSwitch = L"NoInteractiveInput";
const wchar_t * const StdOutSwitch = L"StdOut";
const wchar_t * const StdInSwitch = L"StdIn";
const wchar_t * const BatchSettingsSwitch = L"batchsettings";
const wchar_t * const CopyIdSwitch = L"CopyId";
const wchar_t * const IdentitySwitch = L"Identity";
const wchar_t * const FingerprintScanSwitch = L"FingerprintScan";
const wchar_t * const InfoSwitch = L"Info";
const wchar_t * const ExitCommand = L"exit";
// GUIConfiguration's KeepUpToDateChangeDelay default: how long a directory
// must be quiet before keepuptodate synchronizes it.
const unsigned int KeepUpToDateChangeDelay = 500;
// Upstream waits this long for input in batch mode before giving up.
const unsigned int BatchInputTimeout = 10000;
const UnicodeString TimestampVarName(L"TIMESTAMP");

// GUITools.cpp's ApplyTabs with spaces: "label<TAB>value" lines aligned.
UnicodeString ApplyTabs(const UnicodeString & Text)
{
  std::unique_ptr<TStringList> Lines(TextToStringList(Text));
  std::vector<int> TabPos(Lines->Count, 0);
  int MaxWidth = -1;
  for (int Index = 0; Index < Lines->Count; Index++)
  {
    const UnicodeString Line = Lines->Strings[Index];
    const int P = Line.Pos(L"\t");
    if (P > 0)
    {
      TabPos[Index] = P;
      MaxWidth = Max(MaxWidth, Line.SubString(1, P - 1).TrimRight().Length() + 2);
    }
  }
  if (MaxWidth < 0)
  {
    return Text;
  }
  for (int Index = 0; Index < Lines->Count; Index++)
  {
    if (TabPos[Index] > 0)
    {
      const UnicodeString Line = Lines->Strings[Index];
      UnicodeString Start = Line.SubString(1, TabPos[Index] - 1).TrimRight() + L"  ";
      UnicodeString Remaining = Line.SubString(TabPos[Index] + 1, Line.Length() - TabPos[Index]);
      while (Remaining.SubString(1, 1) == L"\t")
      {
        Remaining.Delete(1, 1);
      }
      while (Start.Length() < MaxWidth)
      {
        Start += L' ';
      }
      Lines->Strings[Index] = Start + Remaining;
    }
  }
  return Lines->Text.TrimRight();
}

UnicodeString ExeBaseName()
{
  return ExtractFileBaseName(ParamStr(0));
}


TCliConsole::TStdInOut ParseStdInOut(TProgramParams * Params, const UnicodeString & Switch, bool AllowChunked)
{
  UnicodeString Value;
  if (!Params->FindSwitch(Switch, Value))
  {
    return TCliConsole::StdOff;
  }
  if (Value.IsEmpty() || SameText(Value, L"binary"))
  {
    return TCliConsole::StdBinary;
  }
  if (AllowChunked && SameText(Value, L"chunked"))
  {
    return TCliConsole::StdChunked;
  }
  throw Exception(FORMAT(LoadStr(SCRIPT_VALUE_UNKNOWN), (Value, Switch)));
}

//---------------------------------------------------------------------------
class TCliRunner
{
public:
  explicit TCliRunner(TCliConsole * Console);
  ~TCliRunner();

  int Run(const UnicodeString & Session, TOptions * Options, TStrings * ScriptCommands,
    TStrings * ScriptParameters, bool UsageWarnings);
  void ShowException(Exception * E);
  void PrintMessage(const UnicodeString & Str, bool Error = false);

  void __fastcall ScriptPrint(TScript * Script, const UnicodeString Str, bool Error);
  void __fastcall ScriptPrintProgress(TScript * Script, bool First, const UnicodeString Str);
  void __fastcall ScriptInput(TScript * Script, const UnicodeString Prompt, UnicodeString & Str);
  void __fastcall ScriptTerminalPromptUser(TTerminal * Terminal, TPromptKind Kind, UnicodeString Name,
    UnicodeString Instructions, TStrings * Prompts, TStrings * Results, bool & Result, void * Arg);
  void __fastcall ScriptShowExtendedException(TTerminal * Terminal, Exception * E, void * Arg);
  void __fastcall ScriptTerminalQueryUser(TObject * Sender, const UnicodeString Query,
    TStrings * MoreMessages, unsigned int Answers, const TQueryParams * Params, unsigned int & Answer,
    TQueryType QueryType, void * Arg);
  void __fastcall ScriptQueryCancel(TScript * Script, bool & Cancel);
  void __fastcall ScriptTransferOut(TObject * Sender, const unsigned char * Data, size_t Len);
  size_t __fastcall ScriptTransferIn(TObject * Sender, unsigned char * Data, size_t Len);
  void __fastcall ConfigurationChange(TObject * Sender);
  void __fastcall ScriptProgress(TScript * Script, TScriptProgress & Progress);
  void __fastcall ScriptSynchronizeStartStop(TScript * Script, const UnicodeString LocalDirectory,
    const UnicodeString RemoteDirectory, const TCopyParamType & CopyParam, int SynchronizeParams);

private:
  TCliConsole * FConsole;
  TManagementScript * FScript;
  int FLastProgressLen;
  bool FCommandError;
  bool FBatchScript;
  bool FAborted;
  bool FExternalTimestampVar;

  void Print(const UnicodeString & Str, bool FromBeginning = false, bool Error = false);
  bool DoInput(UnicodeString & Str, bool Echo, unsigned int Timer, bool Interactive);
  void Input(const UnicodeString & Prompt, UnicodeString & Str, bool Echo, bool Interactive);
  unsigned int InputTimeout() const;
  bool NotifyAbort();
  bool Aborted(bool AllowCompleteAbort = true);
  void DoShowException(TTerminal * Terminal, Exception * E);
  void UpdateTitle();
  UnicodeString ExpandCommand(UnicodeString Command, TStrings * ScriptParameters);
  void Failed(bool & AnyError);
  void SynchronizeLog(const UnicodeString & Message);
  bool SynchronizeChange(const UnicodeString & LocalRoot, const UnicodeString & RemoteRoot,
    const UnicodeString & Directory, const TCopyParamType & CopyParam, int SynchronizeParams);
};
//---------------------------------------------------------------------------
TCliRunner::TCliRunner(TCliConsole * Console) :
  FConsole(Console),
  FScript(NULL),
  FLastProgressLen(0),
  FCommandError(false),
  FBatchScript(false),
  FAborted(false)
{
  FExternalTimestampVar = !GetEnvironmentVariable(TimestampVarName).IsEmpty();
  Configuration->OnChange = CLOSURE(ConfigurationChange);
  Configuration->Scripting = true;
}
//---------------------------------------------------------------------------
TCliRunner::~TCliRunner()
{
  Configuration->OnChange = NULL;
}
//---------------------------------------------------------------------------
unsigned int TCliRunner::InputTimeout() const
{
  return ((FScript != NULL) && (FScript->Batch != TScript::BatchOff)) ? BatchInputTimeout : 0;
}
//---------------------------------------------------------------------------
void TCliRunner::Print(const UnicodeString & Str, bool FromBeginning, bool Error)
{
  if (FLastProgressLen > 0)
  {
    FConsole->Print(L"\n" + Str, FromBeginning, Error);
    FLastProgressLen = 0;
  }
  else
  {
    FConsole->Print(Str, FromBeginning, Error);
  }
}
//---------------------------------------------------------------------------
void TCliRunner::PrintMessage(const UnicodeString & Str, bool Error)
{
  const UnicodeString Line = RemoveEmptyLines(Str);
  if (FScript != NULL)
  {
    // This logs it too.
    FScript->PrintLine(Line, Error);
  }
  else
  {
    FConsole->PrintLine(Line, Error);
  }
}
//---------------------------------------------------------------------------
bool TCliRunner::NotifyAbort()
{
  const bool Result = FBatchScript;
  if (Result)
  {
    FAborted = true;
  }
  return Result;
}
//---------------------------------------------------------------------------
bool TCliRunner::Aborted(bool AllowCompleteAbort)
{
  if (FAborted)
  {
    return true;
  }
  const bool Result = FConsole->PendingAbort();
  if (Result)
  {
    PrintMessage(LoadStr(USER_TERMINATED), true);
    if (AllowCompleteAbort && NotifyAbort() && (FScript->Terminal != NULL))
    {
      std::unique_ptr<TStringList> Failure(TextToStringList(LoadStr(USER_TERMINATED)));
      FScript->Terminal->ActionLog->AddFailure(Failure.get());
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
bool TCliRunner::DoInput(UnicodeString & Str, bool Echo, unsigned int Timer, bool Interactive)
{
  bool Result;
  if (Interactive && FConsole->NoInteractiveInput())
  {
    Result = false;
  }
  else
  {
    Result = FConsole->Input(Str, Echo, Timer);
  }
  if (!Result)
  {
    NotifyAbort();
  }
  return Result;
}
//---------------------------------------------------------------------------
void TCliRunner::Input(const UnicodeString & Prompt, UnicodeString & Str, bool Echo, bool Interactive)
{
  Print(Prompt);
  if (!DoInput(Str, Echo, InputTimeout(), Interactive))
  {
    Abort();
  }
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptPrint(TScript * /*Script*/, const UnicodeString Str, bool Error)
{
  Print(Str, false, Error);
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptPrintProgress(TScript * /*Script*/, bool First, const UnicodeString Str)
{
  UnicodeString S = Str;
  if (First && (FLastProgressLen > 0))
  {
    S = L"\n" + S;
  }
  else if (S.Length() < FLastProgressLen)
  {
    const int Padding = FLastProgressLen - S.Length();
    S += UnicodeString::StringOfChar(L' ', Padding) + UnicodeString::StringOfChar(L'\b', Padding);
  }
  FConsole->Print(S, true);
  FLastProgressLen = Str.Length();
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptInput(TScript * /*Script*/, const UnicodeString Prompt, UnicodeString & Str)
{
  Input(Prompt, Str, true, true);
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptTerminalPromptUser(TTerminal * /*Terminal*/, TPromptKind /*Kind*/,
  UnicodeString /*Name*/, UnicodeString Instructions, TStrings * Prompts, TStrings * Results, bool & Result,
  void * /*Arg*/)
{
  if (!Instructions.IsEmpty())
  {
    PrintMessage(Instructions);
  }
  // With no prompts, success.
  Result = true;
  for (int Index = 0; Index < Prompts->Count; Index++)
  {
    UnicodeString Prompt = Prompts->Strings[Index];
    if (!Prompt.IsEmpty() && (Prompt[Prompt.Length()] != L' '))
    {
      Prompt += L' ';
    }
    const int P = Prompt.Pos(L'&');
    if (P > 0)
    {
      Prompt.Delete(P, 1);
    }
    Print(Prompt);
    UnicodeString Answer = Results->Strings[Index];
    const bool Echo = FLAGSET(reinterpret_cast<intptr_t>(Prompts->Objects[Index]), pupEcho);
    Result = DoInput(Answer, Echo, InputTimeout(), true);
    Results->Strings[Index] = Answer;
  }
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptShowExtendedException(TTerminal * Terminal, Exception * E, void * /*Arg*/)
{
  DoShowException(Terminal, E);
}
//---------------------------------------------------------------------------
// TConsoleRunner's query: the message, then the answers with accelerators
// -- a caption's own, else its first free letter, else its first free
// character, else a number -- answered from the console, by the batch mode,
// or by a timeout.
void __fastcall TCliRunner::ScriptTerminalQueryUser(TObject * /*Sender*/, const UnicodeString Query,
  TStrings * MoreMessages, unsigned int Answers, const TQueryParams * Params, unsigned int & Answer,
  TQueryType /*QueryType*/, void * /*Arg*/)
{
  UnicodeString AQuery = Query;
  unsigned int Timer = 0;
  unsigned int Timeout = 0;
  unsigned int TimeoutA = 0;
  unsigned int TimeoutR = 0;
  unsigned int NoBatchA = 0;

  if (Params != NULL)
  {
    if (Params->Timeout > 0)
    {
      Timeout = Params->Timeout;
      TimeoutA = Params->TimeoutAnswer;
      TimeoutR = Params->TimeoutResponse;
    }
    if (Params->Timer > 0)
    {
      Timer = Params->Timer;
      if (Params->TimerAnswers > 0)
      {
        Answers = Params->TimerAnswers;
      }
      if (!Params->TimerMessage.IsEmpty())
      {
        AQuery = Params->TimerMessage;
      }
    }
    if (FLAGSET(Params->Params, qpFatalAbort))
    {
      AQuery = FMTLOAD(WARN_FATAL_ERROR, (AQuery));
    }
    NoBatchA = Params->NoBatchAnswers;
    if (FLAGSET(Params->Params, qpWaitInBatch))
    {
      Timeout = InputTimeout();
      if (Timeout != 0)
      {
        TimeoutA = AbortAnswer(Answers & ~NoBatchA);
        TimeoutR = TimeoutA;
      }
    }
  }

  AQuery = ApplyTabs(RemoveInteractiveMsgTag(UnformatMessage(AQuery)));
  // The whole query, for a console that shows it apart (the .NET assembly).
  UnicodeString Message = AQuery;
  PrintMessage(AQuery);
  if ((MoreMessages != NULL) && (MoreMessages->Count > 0))
  {
    PrintMessage(MoreMessages->Text);
    Message += L"\n" + MoreMessages->Text;
  }

  std::vector<unsigned int> Buttons;
  std::vector<UnicodeString> Captions;
  std::vector<TButtonSubmitEvent> OnSubmits;
  for (unsigned int A = qaFirst; A <= qaLast; A = A << 1)
  {
    if (FLAGSET(Answers, A))
    {
      UnicodeString Name;
      UnicodeString Caption;
      AnswerNameAndCaption(A, Name, Caption);
      Buttons.push_back(A);
      Captions.push_back(Caption);
      OnSubmits.push_back(NULL);
    }
  }
  if ((Params != NULL) && (Params->Aliases != NULL))
  {
    for (size_t B = 0; B < Buttons.size(); B++)
    {
      for (unsigned int I = 0; I < Params->AliasesCount; I++)
      {
        if (Params->Aliases[I].Button == Buttons[B])
        {
          if (!Params->Aliases[I].Alias.IsEmpty())
          {
            Captions[B] = Params->Aliases[I].Alias;
          }
          OnSubmits[B] = Params->Aliases[I].OnSubmit;
          break;
        }
      }
    }
  }

  // The captions' own accelerators, unless taken.
  UnicodeString Accels;
  for (size_t B = 0; B < Buttons.size(); B++)
  {
    UnicodeString & Caption = Captions[B];
    const int P = Caption.Pos(L'&');
    if ((P > 0) && (P < Caption.Length()))
    {
      const wchar_t Accel = AnsiUpperCase(Caption)[P + 1];
      if (Accels.Pos(Accel) > 0)
      {
        Caption.Delete(P, 1);
        Accels += L' ';
      }
      else
      {
        Accels += Accel;
      }
    }
    else
    {
      Accels += L' ';
    }
  }

  const unsigned int CancelA = CancelAnswer(Answers);
  const unsigned int AbortA = AbortAnswer(Answers & ~NoBatchA);
  const unsigned int ContinueA = ContinueAnswer(Answers & ~NoBatchA);
  int CancelIndex = -1;
  int AbortIndex = -1;
  int ContinueIndex = -1;
  int TimeoutIndex = 0;
  int NumberAccel = 0;
  for (size_t B = 0; B < Buttons.size(); B++)
  {
    UnicodeString & Caption = Captions[B];
    const int Pos = static_cast<int>(B) + 1;
    for (int Pass = 0; (Pass < 2) && (Accels[Pos] == L' '); Pass++)
    {
      const UnicodeString Upper = AnsiUpperCase(Caption);
      for (int I = 1; I <= Caption.Length(); I++)
      {
        const wchar_t C = Upper[I];
        if (((Pass == 0) ? IsLetter(C) : (C != L' ')) && (Accels.Pos(C) == 0))
        {
          Caption.Insert(L"&", I);
          Accels[Pos] = C;
          break;
        }
      }
    }
    if (Accels[Pos] == L' ')
    {
      NumberAccel++;
      Caption = FORMAT(L"&%d%s", (NumberAccel, Caption));
      Accels[Pos] = Caption[2];
    }
    if (Buttons[B] == CancelA) { CancelIndex = Pos; }
    if (Buttons[B] == AbortA) { AbortIndex = Pos; }
    if (Buttons[B] == ContinueA) { ContinueIndex = Pos; }
    if (Buttons[B] == TimeoutA) { TimeoutIndex = Pos; }
  }

  const bool Timeouting = (Timeout > 0);
  bool FirstOutput = true;
  do
  {
    Answer = 0;
    int AnswerIndex = 0;
    bool Retry;
    do
    {
      Retry = false;
      if (FirstOutput || FConsole->LiveOutput())
      {
        UnicodeString Output;
        for (size_t B = 0; B < Buttons.size(); B++)
        {
          if (B > 0)
          {
            Output += L", ";
          }
          UnicodeString Caption = Captions[B];
          const int P = Caption.Pos(L'&');
          if (P > 0)
          {
            Caption[P] = L'(';
            Caption.Insert(L")", P + 2);
          }
          if (static_cast<int>(B) + 1 == TimeoutIndex)
          {
            Caption = FMTLOAD(TIMEOUT_BUTTON, (Caption, int(Timeout / MSecsPerSec)));
          }
          Output += Caption;
        }
        Output += L": ";
        // Rewritten in place only where the output is live.
        Print(Output, !FirstOutput);
        FirstOutput = false;
      }

      if (!Timeouting && (FScript->Batch == TScript::BatchContinue))
      {
        AnswerIndex = ContinueIndex;
      }
      else if (!Timeouting && (FScript->Batch != TScript::BatchOff))
      {
        AnswerIndex = AbortIndex;
      }
      else if (Timeouting && (Timeout < MSecsPerSec))
      {
        AnswerIndex = TimeoutIndex;
        Answer = TimeoutR;
      }
      else
      {
        unsigned int ActualTimer;
        if (Timeouting)
        {
          ActualTimer = (Timer == 0) ?
            (FConsole->NoInteractiveInput() ? Timeout : MSecsPerSec) :
            ((Timer < MSecsPerSec) ? Timer : MSecsPerSec);
        }
        else
        {
          ActualTimer = Timer;
        }
        if (FConsole->NoInteractiveInput() && (Timer > 0))
        {
          Sleep(Timer);
          AnswerIndex = -2;
        }
        else
        {
          AnswerIndex = FConsole->Choice(Accels, CancelIndex, -1, ContinueIndex, -2, Timeouting, ActualTimer,
            Message);
        }
        if (AnswerIndex == -1)
        {
          NotifyAbort();
          AnswerIndex = AbortIndex;
        }
        else if (AnswerIndex == -2)
        {
          if (Timeouting)
          {
            Timeout -= ActualTimer;
            Retry = true;
          }
          if ((Timer > 0) && (Params != NULL) && (Params->TimerEvent != NULL))
          {
            unsigned int AAnswer = 0;
            Params->TimerEvent(AAnswer);
            if (AAnswer != 0)
            {
              Answer = AAnswer;
              Retry = false;
            }
            else
            {
              Retry = true;
            }
          }
        }
      }
    }
    while (Retry);

    if (Answer == 0)
    {
      if ((AnswerIndex < 1) || (AnswerIndex > static_cast<int>(Buttons.size())))
      {
        // No answer to give: only possible with no abort answer in batch.
        Abort();
      }
      FConsole->PrintLine(StripHotkey(Captions[AnswerIndex - 1]));
      FirstOutput = true;
      if (OnSubmits[AnswerIndex - 1] != NULL)
      {
        OnSubmits[AnswerIndex - 1](NULL, Answer);
      }
      else
      {
        Answer = Buttons[AnswerIndex - 1];
      }
    }
    else
    {
      FConsole->PrintLine();
    }
  }
  while (Answer == 0);

  if ((Answer == AbortA) && ((Params == NULL) || FLAGCLEAR(Params->Params, qpIgnoreAbort)))
  {
    if (FScript->Terminal != NULL)
    {
      std::unique_ptr<TStrings> Messages(new TStringList());
      Messages->Add(Query);
      if (MoreMessages != NULL)
      {
        Messages->AddStrings(MoreMessages);
      }
      FScript->Terminal->ActionLog->AddFailure(Messages.get());
    }
    FCommandError = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptQueryCancel(TScript * /*Script*/, bool & Cancel)
{
  if (Aborted())
  {
    Cancel = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptTransferOut(TObject *, const unsigned char * Data, size_t Len)
{
  FConsole->TransferOut(Data, Len);
}
//---------------------------------------------------------------------------
size_t __fastcall TCliRunner::ScriptTransferIn(TObject *, unsigned char * Data, size_t Len)
{
  return FConsole->TransferIn(Data, Len);
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ScriptProgress(TScript * /*Script*/, TScriptProgress & Progress)
{
  FConsole->Progress(Progress);
}
//---------------------------------------------------------------------------
// keepuptodate, as TConsoleRunner runs it over TSynchronizeController: the
// local tree is watched, and each directory that changed, once quiet for
// the change delay, is synchronized on its own (the script's parameters
// include spNoRecurse and spSubDirs); until Ctrl+C or a failure.
void __fastcall TCliRunner::ScriptSynchronizeStartStop(TScript * /*Script*/, const UnicodeString LocalDirectory,
  const UnicodeString RemoteDirectory, const TCopyParamType & CopyParam, int SynchronizeParams)
{
  TCopyParamType Params = CopyParam;
  Params.IncludeFileMask.SetRoots(LocalDirectory, RemoteDirectory);
  // Directories the copy parameters exclude are not watched (SynchronizeFilter).
  auto Filter = [&Params](const UnicodeString & Directory) -> bool
  {
    if (Params.AllowAnyTransfer())
    {
      return true;
    }
    TFileMasks::TParams MaskParams;
    const bool Hidden = FLAGSET(FileGetAttrFix(Directory), faHidden);
    return Params.AllowTransfer(Directory, osLocal, true, MaskParams, Hidden);
  };
  SynchronizeLog(FMTLOAD(SYNCHRONIZE_SCAN, (LocalDirectory)));
  TDirectoryWatcher Watcher(LocalDirectory, true, Filter);
  int Directories = Watcher.Directories();
  SynchronizeLog(FMTLOAD(SYNCHRONIZE_START, (Directories)));

  bool Stop = false;
  while (!Stop && !Aborted(false))
  {
    Watcher.Wait(100);
    if (!Watcher.Error().IsEmpty())
    {
      // SynchronizeControllerSynchronizeInvalid, then the abort.
      PrintMessage(Watcher.ErrorDirectory().IsEmpty() ? LoadStr(WATCH_ERROR_GENERAL) :
        FMTLOAD(WATCH_ERROR_DIRECTORY, (Watcher.ErrorDirectory())), true);
      PrintMessage(Watcher.Error(), true);
      NotifyAbort();
      break;
    }
    for (const UnicodeString & Directory : Watcher.TakeSettled(KeepUpToDateChangeDelay))
    {
      if (!SynchronizeChange(LocalDirectory, RemoteDirectory, Directory, Params, SynchronizeParams))
      {
        Stop = true;
        break;
      }
    }
    if (Watcher.Directories() != Directories)
    {
      Directories = Watcher.Directories();
      SynchronizeLog(FMTLOAD(SYNCHRONIZE_START, (Directories)));
    }
    if (FScript->Terminal != NULL)
    {
      FScript->Terminal->Idle();
    }
  }
}
//---------------------------------------------------------------------------
void TCliRunner::SynchronizeLog(const UnicodeString & Message)
{
  PrintMessage(Message);
  if (FScript->Terminal != NULL)
  {
    FScript->Terminal->LogEvent(FORMAT(L"Keep up to date: %s", (Message)));
  }
}
//---------------------------------------------------------------------------
// TSynchronizeController::SynchronizeChange and TConsoleRunner's
// SynchronizeControllerSynchronize: the changed directory and its remote
// counterpart, synchronized. False when keepuptodate is to stop.
bool TCliRunner::SynchronizeChange(const UnicodeString & LocalRoot, const UnicodeString & RemoteRoot,
  const UnicodeString & Directory, const TCopyParamType & CopyParam, int SynchronizeParams)
{
  const UnicodeString Root = IncludeTrailingBackslash(LocalRoot);
  const UnicodeString Local = IncludeTrailingBackslash(Directory);
  const UnicodeString Remote = UnixIncludeTrailingBackslash(RemoteRoot) +
    ToUnixPath(Local.SubString(Root.Length() + 1, Local.Length() - Root.Length()));
  SynchronizeLog(FMTLOAD(SYNCHRONIZE_CHANGE, (ExcludeTrailingBackslash(Local))));
  try
  {
    TSynchronizeChecklist * Checklist = NULL;
    try
    {
      FScript->Synchronize(Local, Remote, CopyParam, SynchronizeParams, &Checklist);
    }
    catch (Exception &)
    {
      delete Checklist;
      if ((FScript->Batch == TScript::BatchContinue) && FScript->Terminal->Active)
      {
        return true;
      }
      throw;
    }
    delete Checklist;
  }
  catch (Exception &)
  {
    // SynchronizeAbort: the error is the script's to report.
    NotifyAbort();
    return false;
  }
  return true;
}
//---------------------------------------------------------------------------
void __fastcall TCliRunner::ConfigurationChange(TObject * /*Sender*/)
{
  if (FScript != NULL)
  {
    FScript->ReflectSettings();
  }
}
//---------------------------------------------------------------------------
void TCliRunner::ShowException(Exception * E)
{
  DoShowException(NULL, E);
}
//---------------------------------------------------------------------------
void TCliRunner::DoShowException(TTerminal * Terminal, Exception * E)
{
  if ((Terminal == NULL) && (FScript != NULL))
  {
    Terminal = FScript->Terminal;
  }
  UnicodeString Message;
  if (ExceptionFullMessage(E, Message))
  {
    FCommandError = true;
    PrintMessage(Message, true);
  }
  TTerminal * LoggingTerminal = Terminal;
  TSecondaryTerminal * Secondary = dynamic_cast<TSecondaryTerminal *>(LoggingTerminal);
  if (Secondary != NULL)
  {
    LoggingTerminal = Secondary->MainTerminal;
  }
  if (LoggingTerminal != NULL)
  {
    LoggingTerminal->ActionLog->AddFailure(E);
  }
}
//---------------------------------------------------------------------------
void TCliRunner::UpdateTitle()
{
  const UnicodeString Name = (FScript->Terminal != NULL) ? FScript->Terminal->SessionData->SessionName : UnicodeString();
  FConsole->SetTitle(Name.IsEmpty() ? AppNameString() : (Name + L" - " + AppNameString()));
}
//---------------------------------------------------------------------------
// %1%... for /parameter, %TIMESTAMP% and %TIMESTAMP[+-relative]#format%,
// then environment variables, as TConsoleRunner expands commands.
UnicodeString TCliRunner::ExpandCommand(UnicodeString Command, TStrings * ScriptParameters)
{
  for (int Index = 0; Index < ScriptParameters->Count; Index++)
  {
    Command = ReplaceStr(Command, FORMAT(L"%%%d%%", (Index + 1)), ScriptParameters->Strings[Index]);
  }
  const TDateTime N = Now();
  if (!FExternalTimestampVar)
  {
    Command = ReplaceStr(Command, FORMAT(L"%%%s%%", (TimestampVarName)), FormatDateTime(L"yyyymmddhhnnss", N));
  }
  int Offset = 1;
  int P2;
  do
  {
    const int P = Pos(UpperCase(L"%" + TimestampVarName), UpperCase(Command), Offset);
    if (P > 0)
    {
      Offset = P + 1 + TimestampVarName.Length();
      P2 = Pos(L"%", Command, Offset);
      const int P3 = Pos(L"#", Command, Offset);
      if ((P2 > 0) && (P3 > 0) && (P3 < P2) &&
          ((P3 == Offset) || (Command[Offset] == L'+') || (Command[Offset] == L'-')))
      {
        bool Valid = true;
        TDateTime T = N;
        if (P3 > Offset)
        {
          const bool Add = (Command[Offset] == L'+');
          Offset++;
          Valid = TryRelativeStrToDateTime(Command.SubString(Offset, P3 - Offset), T, Add);
        }
        Offset = P3 + 1;
        if (Valid)
        {
          const UnicodeString Value = FormatDateTime(Command.SubString(Offset, P2 - Offset), T);
          Command = Command.SubString(1, P - 1) + Value + Command.SubString(P2 + 1, Command.Length() - P2);
          Offset = P + Value.Length();
        }
      }
    }
    else
    {
      P2 = 0;
    }
  }
  while (P2 > 0);
  return ExpandEnvironmentVariables(Command);
}
//---------------------------------------------------------------------------
void TCliRunner::Failed(bool & AnyError)
{
  if (FScript != NULL)
  {
    FScript->Log(llMessage, L"Failed");
  }
  AnyError = true;
}
//---------------------------------------------------------------------------
int TCliRunner::Run(const UnicodeString & Session, TOptions * Options, TStrings * ScriptCommands,
  TStrings * ScriptParameters, bool UsageWarnings)
{
  int ExitCode;
  bool AnyError = false;
  try
  {
    FScript = new TManagementScript(StoredSessions, FConsole->LimitedOutput());
    FScript->CopyParam = TCopyParamType();
    FScript->SynchronizeParams = static_cast<int>(+TTerminal::spDefault);
    FScript->WantsProgress = FConsole->WantsProgress();
    FScript->OnProgress = CLOSURE(ScriptProgress);
    FScript->OnSynchronizeStartStop = CLOSURE(ScriptSynchronizeStartStop);
    FScript->OnPrint = CLOSURE(ScriptPrint);
    FScript->OnPrintProgress = CLOSURE(ScriptPrintProgress);
    FScript->OnInput = CLOSURE(ScriptInput);
    FScript->OnTerminalPromptUser = CLOSURE(ScriptTerminalPromptUser);
    FScript->OnShowExtendedException = CLOSURE(ScriptShowExtendedException);
    FScript->OnTerminalQueryUser = CLOSURE(ScriptTerminalQueryUser);
    FScript->OnQueryCancel = CLOSURE(ScriptQueryCancel);
    FScript->UsageWarnings = UsageWarnings;
    if (FConsole->StdOut())
    {
      FScript->OnTransferOut = CLOSURE(ScriptTransferOut);
    }
    if (FConsole->StdIn())
    {
      FScript->OnTransferIn = CLOSURE(ScriptTransferIn);
    }

    UpdateTitle();
    // Everything until the first command typed in is batch, the session
    // from the command line and the script included.
    FBatchScript = true;

    if (!Session.IsEmpty())
    {
      if (UsageWarnings)
      {
        PrintMessage(LoadStr(SCRIPT_CMDLINE_SESSION));
      }
      FCommandError = false;
      FScript->Connect(Session, Options, false);
      if (FCommandError)
      {
        Failed(AnyError);
      }
    }

    FScript->Groups = Options->SwitchValue(L"xmlgroups", true, false);

    int ScriptPos = 0;
    bool Result;
    do
    {
      UpdateTitle();
      UnicodeString Command;
      if ((ScriptCommands != NULL) && (ScriptPos < ScriptCommands->Count))
      {
        Result = true;
        Command = ScriptCommands->Strings[ScriptPos];
        ScriptPos++;
      }
      else
      {
        if (FBatchScript)
        {
          FBatchScript = false;
          FScript->StartInteractive();
        }
        Print(LowerCase(AppNameString()) + L"> ");
        Result = DoInput(Command, true, 0, false);
      }
      if (Result)
      {
        FCommandError = false;
        FScript->Command(ExpandCommand(Command, ScriptParameters));
        if (FCommandError)
        {
          Failed(AnyError);
          if (FScript->Batch == TScript::BatchAbort)
          {
            Result = false;
          }
        }
        if (FScript->Terminal != NULL)
        {
          FScript->Terminal->Idle();
        }
      }
    }
    while (Result && FScript->Continue && !Aborted());
  }
  catch (Exception & E)
  {
    Failed(AnyError);
    ShowException(&E);
  }

  if (FLastProgressLen > 0)
  {
    FConsole->Print(L"\n");
    FLastProgressLen = 0;
  }

  ExitCode = (AnyError || FAborted) ? ResultAnyError : ResultSuccess;
  if (FScript != NULL)
  {
    const UnicodeString ExitCodeMessage = FORMAT(L"Exit code: %d", (ExitCode));
    FScript->Log(llMessage, ExitCodeMessage);
    if (Configuration->ActualLogProtocol >= 1)
    {
      FConsole->Print(ExitCodeMessage + L"\n");
      const UnicodeString LogMessage = FConsole->FinalLogMessage();
      if (!LogMessage.IsEmpty())
      {
        FScript->Log(llMessage, LogMessage);
        FConsole->Print(LogMessage + L"\n");
      }
    }
  }
  delete FScript;
  FScript = NULL;
  return ExitCode;
}

//---------------------------------------------------------------------------
int HandleException(TCliConsole * Console, Exception & E)
{
  UnicodeString Message;
  if (ExceptionFullMessage(&E, Message))
  {
    Console->Print(Message);
  }
  return ResultAnyError;
}
//---------------------------------------------------------------------------
// A switch under its Windows name, or puttygen's -x form.
bool FindNixCompatibleSwitch(TProgramParams * Params, const UnicodeString & Name, const UnicodeString & NixName,
  UnicodeString & Value, bool & Set)
{
  bool Result = !Name.IsEmpty() && Params->FindSwitch(Name, Value, Set);
  if (!Result)
  {
    std::unique_ptr<TStrings> Args(new TStringList());
    Result = Params->FindSwitchCaseSensitive(NixName, Args.get(), 1);
    if (Result && (Args->Count >= 1))
    {
      Value = Args->Strings[0];
      Set = true;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
// Upstream's /keygen: converts a key to PuTTY's format, optionally changing
// its comment, passphrase or certificate.
int KeyGen(TCliConsole * Console, TProgramParams * Params)
{
  int Result = ResultSuccess;
  UnicodeString Passphrase;
  UnicodeString NewPassphrase;
  try
  {
    std::unique_ptr<TStrings> Args(new TStringList());
    if (!Params->FindSwitch(KeyGenSwitch, Args.get(), 1) || (Args->Count < 1) || Args->Strings[0].IsEmpty())
    {
      throw Exception(LoadStr(KEYGEN_NO_INPUT));
    }
    const UnicodeString InputFileName = Args->Strings[0];
    bool ValueSet;
    UnicodeString OutputFileName;
    FindNixCompatibleSwitch(Params, KeyGenOutputSwitch, L"o", OutputFileName, ValueSet);
    UnicodeString NewComment;
    FindNixCompatibleSwitch(Params, KeyGenCommentSwitch, L"C", NewComment, ValueSet);
    bool ChangePassphrase;
    bool NewPassphraseSet = false;
    if (Params->FindSwitchCaseSensitive(L"P"))
    {
      ChangePassphrase = true;
      FindNixCompatibleSwitch(Params, EmptyStr, L"-new-passphrase", NewPassphrase, NewPassphraseSet);
    }
    else
    {
      ChangePassphrase = Params->FindSwitch(KeyGenChangePassphraseSwitch, NewPassphrase, NewPassphraseSet);
    }
    UnicodeString Certificate;
    FindNixCompatibleSwitch(Params, KeyGenCertificateSwitch, L"-certificate", Certificate, ValueSet);
    FindNixCompatibleSwitch(Params, PassphraseOption, L"-old-passphrase", Passphrase, ValueSet);
    if (Params->ParamCount > 0)
    {
      throw Exception(LoadStr(TOO_MANY_PARAMS_ERROR));
    }

    const TKeyType Type = KeyType(InputFileName);
    const int Error = errno;
    switch (Type)
    {
      case ktSSH1:
        throw Exception(LoadStr(KEYGEN_SSH1));
      case ktSSH2:
        if (NewComment.IsEmpty() && !ChangePassphrase && Certificate.IsEmpty() &&
            (Configuration->KeyVersion == 0))
        {
          throw Exception(LoadStr(KEYGEN_NO_ACTION));
        }
        break;
      case ktOpenSSHPEM:
      case ktOpenSSHNew:
      case ktSSHCom:
        if (OutputFileName.IsEmpty())
        {
          throw Exception(LoadStr(KEYGEN_NO_OUTPUT));
        }
        break;
      case ktSSH1Public:
      case ktSSH2PublicRFC4716:
      case ktSSH2PublicOpenSSH:
        throw Exception(LoadStr(KEYGEN_PUBLIC));
      case ktUnopenable:
        throw EOSExtException(FMTLOAD(KEY_TYPE_UNOPENABLE, (InputFileName)), Error);
      default:
        throw Exception(FMTLOAD(KEY_TYPE_UNKNOWN2, (InputFileName)));
    }

    UnicodeString Comment;
    if (IsKeyEncrypted(Type, InputFileName, Comment) && Passphrase.IsEmpty())
    {
      Console->Print(StripHotkey(FMTLOAD(PROMPT_KEY_PASSPHRASE, (Comment))) + L" ");
      if (!Console->Input(Passphrase, false, 0) || Passphrase.IsEmpty())
      {
        Abort();
      }
    }

    TPrivateKey * PrivateKey = LoadKey(Type, InputFileName, Passphrase);
    try
    {
      if (!NewComment.IsEmpty())
      {
        ChangeKeyComment(PrivateKey, NewComment);
      }
      if (!Certificate.IsEmpty())
      {
        AddCertificateToKey(PrivateKey, Certificate);
      }
      if (ChangePassphrase)
      {
        if (!NewPassphraseSet)
        {
          Console->Print(LoadStr(KEYGEN_PASSPHRASE) + L" ");
          if (!Console->Input(NewPassphrase, false, 0))
          {
            Abort();
          }
          Console->Print(LoadStr(KEYGEN_PASSPHRASE2) + L" ");
          UnicodeString NewPassphrase2;
          if (!Console->Input(NewPassphrase2, false, 0))
          {
            Abort();
          }
          if (NewPassphrase != NewPassphrase2)
          {
            Shred(NewPassphrase2);
            throw Exception(LoadStr(KEYGEN_PASSPHRASES_MISMATCH));
          }
        }
      }
      else
      {
        NewPassphrase = Passphrase;
      }
      if (OutputFileName.IsEmpty())
      {
        OutputFileName = InputFileName;
      }
      SaveKey(ktSSH2, OutputFileName, NewPassphrase, PrivateKey);
      Console->PrintLine(FMTLOAD(KEYGEN_SAVED, (OutputFileName)));
    }
    catch (...)
    {
      FreeKey(PrivateKey);
      throw;
    }
    FreeKey(PrivateKey);
  }
  catch (Exception & E)
  {
    Result = HandleException(Console, E);
  }
  Shred(Passphrase);
  Shred(NewPassphrase);
  return Result;
}
//---------------------------------------------------------------------------
// Upstream's /batchsettings: raw settings applied to the sites a mask
// matches.
int BatchSettings(TCliConsole * Console, TProgramParams * Params)
{
  int Result = ResultSuccess;
  try
  {
    std::unique_ptr<TStrings> Arguments(new TStringList());
    Params->FindSwitch(BatchSettingsSwitch, Arguments.get());
    if (Arguments->Count < 1)
    {
      throw Exception(LoadStr(BATCH_SET_NO_MASK));
    }
    else if (Arguments->Count < 2)
    {
      throw Exception(LoadStr(BATCH_SET_NO_SETTINGS));
    }
    TFileMasks Mask(Arguments->Strings[0]);
    Arguments->Delete(0);
    std::unique_ptr<TOptionsStorage> OptionsStorage(new TOptionsStorage(Arguments.get(), false));
    int Matches = 0;
    int Changes = 0;
    for (int Index = 0; Index < StoredSessions->Count; Index++)
    {
      TSessionData * Data = StoredSessions->Sessions[Index];
      if (!Data->IsWorkspace && Mask.Matches(Data->Name, false, false))
      {
        Matches++;
        std::unique_ptr<TSessionData> OriginalData(new TSessionData(L""));
        OriginalData->CopyDataNoRecrypt(Data);
        bool UnsafeSettings; // unused
        Data->ApplyRawSettings(OptionsStorage.get(), false, true, UnsafeSettings);
        const bool Changed = !OriginalData->IsSame(Data, false);
        if (Changed)
        {
          Changes++;
        }
        Console->PrintLine(Data->Name + TitleSeparator + LoadStr(Changed ? BATCH_SET_CHANGED : BATCH_SET_NOT_CHANGED));
      }
    }
    StoredSessions->Save(false, true); // explicit
    Console->PrintLine(FMTLOAD(BATCH_SET_SUMMARY, (Matches, Changes)));
  }
  catch (Exception & E)
  {
    Result = HandleException(Console, E);
  }
  return Result;
}
//---------------------------------------------------------------------------
// Upstream's /fingerprintscan: the host key's (or TLS certificate's)
// fingerprints, unverified.
int FingerprintScan(TCliConsole * Console, TProgramParams * Params)
{
  int Result = ResultSuccess;
  try
  {
    ApplyLogSwitches(Params, true);
    std::unique_ptr<TSessionData> SessionData;
    if (Params->ParamCount > 0)
    {
      int ParsedInfo;
      SessionData.reset(StoredSessions->ParseUrl(Params->Param[1], Params, ParsedInfo));
      if (FLAGSET(ParsedInfo, piDefaultsOnly) || !SessionData->CanLogin ||
          (!SessionData->UsesSsh && (SessionData->Ftps == ftpsNone)))
      {
        SessionData.reset();
      }
    }
    if (!SessionData)
    {
      throw Exception(LoadStr(FINGERPRINTSCAN_NEED_SECURE_SESSION));
    }
    std::unique_ptr<TTerminal> Terminal(new TTerminal(SessionData.get(), Configuration));
    UnicodeString SHA256;
    UnicodeString SHA1;
    UnicodeString MD5;
    Terminal->FingerprintScan(SHA256, SHA1, MD5);
    if (!SHA256.IsEmpty())
    {
      Console->PrintLine(FORMAT(L"SHA-256: %s", (SHA256)));
    }
    if (!SHA1.IsEmpty())
    {
      Console->PrintLine(FORMAT(L"SHA-1: %s", (SHA1)));
    }
    if (!MD5.IsEmpty())
    {
      Console->PrintLine(FORMAT(L"MD5:     %s", (MD5)));
    }
  }
  catch (Exception & E)
  {
    Result = HandleException(Console, E);
  }
  return Result;
}
//---------------------------------------------------------------------------
void PrintListAndFree(TCliConsole * Console, const UnicodeString & Caption, TStrings * List)
{
  std::unique_ptr<TStrings> Owner(List);
  Console->PrintLine(Caption);
  for (int Index = 0; Index < List->Count; Index++)
  {
    Console->PrintLine(List->Strings[Index]);
  }
  Console->PrintLine();
}
//---------------------------------------------------------------------------
// Upstream's /info: the SSH and TLS implementations and their algorithms.
int Info(TCliConsole * Console)
{
  int Result = ResultSuccess;
  try
  {
    Console->PrintLine(FORMAT(L"SSH implementation: PuTTY %s", (GetPuTTYVersion())));
    Console->PrintLine();
    PrintListAndFree(Console, L"SSH encryption ciphers:", SshCipherList());
    PrintListAndFree(Console, L"SSH key exchange algorithms:", SshKexList());
    PrintListAndFree(Console, L"SSH host key algorithms:", SshHostKeyList());
    PrintListAndFree(Console, L"SSH MAC algorithms:", SshMacList());
    Console->PrintLine(TSessionLog::GetSeparator());
    Console->PrintLine(FORMAT(L"TLS/SSL implementation: %s", (GetOpenSSLVersionText())));
    Console->PrintLine();
    PrintListAndFree(Console, L"TLS/SSL cipher suites:", TlsCipherList());
  }
  catch (Exception & E)
  {
    Result = HandleException(Console, E);
  }
  return Result;
}
//---------------------------------------------------------------------------
// The switches this client has, with upstream's descriptions.
void Usage(TCliConsole * Console)
{
  Console->PrintLine(FORMAT(L"%s, %s", (AppNameString(), Configuration->VersionStr)));
  Console->PrintLine(ReplaceText(LoadStr(WINSCP_COPYRIGHT), L"©", L"(c)"));
  Console->PrintLine();
  Console->PrintLine(LoadStr(USAGE_SYNTAX_LABEL));
  const UnicodeString Exe = ExeBaseName();
  const wchar_t * const Syntax[] = {
    L"site|workspace|folder",
    L"(sftp|scp|ftp[es]|dav[s]|s3)://[user[:password]@]host[:port][/path/[file]]",
    L"[mysession] [/privatekey=<file> [/passphrase=<passphrase>]]",
    L"[mysession] [/hostkey=<fingerprint>]",
    L"[mysession] [/username=<user> [/password=<password>]]",
    L"[mysession] [/passwordsfromfiles]",
    L"[mysession] [/clientcert=<file> [/passphrase=<passphrase>]]",
    L"[mysession] [/certificate=<fingerprint>]",
    L"[mysession] [/passive[=on|off]] [/implicit|explicit]",
    L"[mysession] [/timeout=<sec>]",
    L"[mysession] [/rawsettings setting1=value1 setting2=value2 ...]",
    L"[/script=<file>] [/command command1 command2 ...] [/parameter // param1 ...]",
    L"[/stdout[=binary|chunked]] [/stdin[=binary]]",
    L"[/log=<logfile> [/loglevel=<level>] [/logsize=[<count>*]<size>]] [/xmllog=<logfile> [/xmlgroups]]",
    L"[/nointeractiveinput] [/ini=<inifile>] [/rawconfig config1=value1 config2=value2 ...]",
    L"/batchsettings <site_mask> setting1=value1 setting2=value2 ...",
    L"/keygen keyfile [/output=<file>] [/changepassphrase] [/comment=<text>] [/certificate=<file>]",
    L"/copyid /identity=<file> mysession",
    L"/info",
    L"/help",
  };
  for (const wchar_t * S : Syntax)
  {
    Console->PrintLine(Exe + L" " + S);
  }
  Console->PrintLine();
  const struct { const wchar_t * Name; int Desc; } Switches[] = {
    { L"session", USAGE_SESSION }, { L"/privatekey=", USAGE_PRIVATEKEY }, { L"/hostkey=", USAGE_HOSTKEY },
    { L"/username", USAGE_USERNAME }, { L"/password", USAGE_PASSWORD },
    { L"/passwordsfromfiles", USAGE_PASSWORDSFROMFILES }, { L"/clientcert=", USAGE_CLIENTCERT },
    { L"/certificate=", USAGE_CERTIFICATE }, { L"/passphrase=", USAGE_PASSPHRASE },
    { L"/passive=", USAGE_PASSIVE }, { L"/implicit", USAGE_IMPLICIT }, { L"/explicit", USAGE_EXPLICIT },
    { L"/timeout=", USAGE_TIMEOUT }, { L"/rawsettings", USAGE_RAWSETTINGS }, { L"/script=", USAGE_SCRIPT },
    { L"/command", USAGE_COMMAND }, { L"/parameter", USAGE_PARAMETER }, { L"/stdout", USAGE_STDOUT },
    { L"/stdin", USAGE_STDIN }, { L"/log=", USAGE_LOG }, { L"/loglevel=", USAGE_LOGLEVEL },
    { L"/logsize=", USAGE_LOGSIZE }, { L"/xmllog=", USAGE_XMLLOG }, { L"/xmlgroups", USAGE_XMLGROUPS },
    { L"/nointeractiveinput", USAGE_INTERACTIVEINPUT }, { L"/ini=", USAGE_INI },
    { L"/rawconfig", USAGE_RAWCONFIG }, { L"/batchsettings", USAGE_BATCHSETTINGS },
    { L"/copyid", USAGE_COPYID }, { L"/info", USAGE_INFO }, { L"/help", USAGE_HELP },
  };
  int Width = 0;
  for (const auto & S : Switches)
  {
    Width = Max(Width, UnicodeString(S.Name).Length());
  }
  for (const auto & S : Switches)
  {
    UnicodeString Name = S.Name;
    while (Name.Length() < Width)
    {
      Name += L' ';
    }
    const UnicodeString Desc = ReplaceText(LoadStr(S.Desc), L"%APP%", Exe);
    Console->PrintLine(L"  " + Name + L"  " + Desc);
  }
}
//---------------------------------------------------------------------------
int RunScripting(TCliConsole * Console, TProgramParams * Params)
{
  std::unique_ptr<TStrings> ScriptCommands(new TStringList());
  std::unique_ptr<TStrings> ScriptParameters(new TStringList());
  TCliRunner Runner(Console);
  int Result;
  try
  {
    const bool CopyId = Params->FindSwitch(CopyIdSwitch);
    if (CheckSafe(Params))
    {
      UnicodeString Value;
      if (CopyId)
      {
        // Upstream's /copyid: the script's copyid, with /identity's (or
        // -i's) key.
        UnicodeString IdentityFileName;
        bool ValueSet;
        FindNixCompatibleSwitch(Params, IdentitySwitch, L"i", IdentityFileName, ValueSet);
        if (IdentityFileName.IsEmpty())
        {
          throw Exception(LoadStr(COPYID_IDETITY_MISSING));
        }
        ScriptCommands->Add(FORMAT(L"%s \"%s\"", (COPYID_COMMAND, IdentityFileName)));
        ScriptCommands->Add(ExitCommand);
      }
      else if (Params->FindSwitch(SCRIPT_SWITCH, Value) && !Value.IsEmpty())
      {
        LoadScriptFromFile(Value, ScriptCommands.get());
      }
      Params->FindSwitch(COMMAND_SWITCH, ScriptCommands.get());
      Params->FindSwitch(L"parameter", ScriptParameters.get());
    }
    UnicodeString Session;
    if (Params->ParamCount >= 1)
    {
      Session = Params->Param[1];
      if (Params->ParamCount > 1)
      {
        // Parameters left over once the URL's own (/rawsettings) are taken.
        TOptions OptionsCopy(*Params);
        int ParsedInfo;
        StoredSessions->ParseUrl(Session, &OptionsCopy, ParsedInfo);
        if (OptionsCopy.ParamCount > 1)
        {
          Runner.PrintMessage(LoadStr(SCRIPT_CMDLINE_PARAMETERS));
        }
      }
    }
    ApplyLogSwitches(Params, true);
    const bool UsageWarnings = !CopyId && ((ScriptCommands->Count > 0) || !Console->Interactive());
    Result = Runner.Run(Session, Params, ScriptCommands.get(), ScriptParameters.get(), UsageWarnings);
  }
  catch (Exception & E)
  {
    Runner.ShowException(&E);
    Result = ResultAnyError;
  }
  return Result;
}

}  // namespace
//---------------------------------------------------------------------------
int main()
{
  int Result = ResultSuccess;
  try
  {
    HeadlessInitialize();
    try
    {
      TProgramParams * Params = TProgramParams::Instance();
      const TCliConsole::TStdInOut StdOut = ParseStdInOut(Params, StdOutSwitch, true);
      const TCliConsole::TStdInOut StdIn = ParseStdInOut(Params, StdInSwitch, false);
      const bool NoInteractiveInput = Params->FindSwitch(NoInteractiveInputSwitch);
      std::unique_ptr<TCliConsole> Console;
      // Started by winscp.com or WinSCP's .NET assembly, as WinSCP.exe is.
#if defined(_WIN32)
      const wchar_t * const ConsoleSwitch = L"consoleinstance";
#else
      const wchar_t * const ConsoleSwitch = L"consolepipes";
#endif
      UnicodeString ConsoleConnection;
      if (Params->FindSwitch(ConsoleSwitch, ConsoleConnection))
      {
        Console.reset(new TExternalConsole(ConsoleConnection, NoInteractiveInput, StdOut, StdIn));
      }
      if (!Console)
      {
        Console.reset(new TStdioConsole(StdOut, StdIn, NoInteractiveInput));
      }
      // In upstream's order (WinMain.cpp).
      if (Params->FindSwitch(L"help") || Params->FindSwitch(L"h") || Params->FindSwitch(L"?"))
      {
        Usage(Console.get());
      }
      else if (Params->FindSwitch(BatchSettingsSwitch))
      {
        Result = CheckSafe(Params) ? BatchSettings(Console.get(), Params) : ResultAnyError;
      }
      else if (Params->FindSwitch(KeyGenSwitch))
      {
        Result = CheckSafe(Params) ? KeyGen(Console.get(), Params) : ResultAnyError;
      }
      else if (Params->FindSwitch(CopyIdSwitch))
      {
        Result = RunScripting(Console.get(), Params);
      }
      else if (Params->FindSwitch(FingerprintScanSwitch))
      {
        Result = CheckSafe(Params) ? FingerprintScan(Console.get(), Params) : ResultAnyError;
      }
      else if (Params->FindSwitch(InfoSwitch))
      {
        Result = Info(Console.get());
      }
      else
      {
        Result = RunScripting(Console.get(), Params);
      }
    }
    catch (Exception & E)
    {
      UnicodeString Message;
      ExceptionFullMessage(&E, Message);
      std::fprintf(stderr, "%s\n", UTF8String(Message).c_str());
      Result = ResultAnyError;
    }
    HeadlessFinalize();
  }
  catch (Exception & E)
  {
    UnicodeString Message;
    ExceptionFullMessage(&E, Message);
    std::fprintf(stderr, "%s\n", UTF8String(Message).c_str());
    Result = ResultAnyError;
  }
  return Result;
}
