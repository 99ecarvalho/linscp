// winscp.com's console protocol, as TExternalConsole speaks it in upstream's
// windows/ConsoleRunner.cpp: the caller -- winscp.com, or WinSCP's .NET
// assembly, which starts the program with /console /consoleinstance=<name>
// -- owns a file mapping holding a TConsoleCommStruct (console/Console.h,
// upstream's own header) and three events. Each exchange fills in the
// structure, signals the request event and waits for the response; the
// cancel event says the user asked to abort. The process joins the caller's
// job object, so it ends when the caller does.
//
// Windows only: the .NET assembly relies on named events and file mappings,
// which .NET provides only there.
#include <vcl.h>
#pragma hdrstop

#include "ExternalConsole.h"

#if defined(_WIN32)

#include <Common.h>
#include <Script.h>
#include <TextsCore.h>
#include <TextsWin.h>
#include <FileOperationProgress.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <mutex>

#include "Console.h"

namespace {

void CopyOut(wchar_t * Target, size_t Size, const UnicodeString & Source)
{
  const size_t Length = std::min(static_cast<size_t>(Source.Length()), Size - 1);
  std::memcpy(Target, Source.c_str(), Length * sizeof(wchar_t));
  Target[Length] = L'\0';
}

void TrimNewLine(UnicodeString & Str)
{
  while (!Str.IsEmpty() && ((Str[Str.Length()] == L'\n') || (Str[Str.Length()] == L'\r')))
  {
    Str.SetLength(Str.Length() - 1);
  }
}

}  // namespace

struct TExternalConsole::TImpl
{
  HANDLE RequestEvent = NULL;
  HANDLE ResponseEvent = NULL;
  HANDLE CancelEvent = NULL;
  HANDLE FileMapping = NULL;
  TConsoleCommStruct * Comm = NULL;
  // FTP calls TransferOut and TransferIn from its own thread.
  std::recursive_mutex Section;
  unsigned int MaxSend = 0;

  // Signals the request and waits for the response.
  void Send()
  {
    SetEvent(RequestEvent);
    const DWORD Start = GetTickCount();
    if (WaitForSingleObject(ResponseEvent, INFINITE) != WAIT_OBJECT_0)
    {
      throw Exception(LoadStr(CONSOLE_SEND_TIMEOUT));
    }
    MaxSend = std::max(MaxSend, static_cast<unsigned int>(GetTickCount() - Start));
  }
};

namespace {

HANDLE Checked(HANDLE Handle, const wchar_t * Desc)
{
  if (Handle == NULL)
  {
    throw ExtException(LoadStr(EXTERNAL_CONSOLE_INIT_ERROR), FORMAT(L"%s\n%s", (Desc, LastSysErrorMessage())));
  }
  return Handle;
}

}  // namespace

TExternalConsole::TExternalConsole(const UnicodeString & Instance, bool NoInteractiveInput,
  TStdInOut StdOut, TStdInOut StdIn) :
  F(new TImpl()),
  FNoInteractiveInput(NoInteractiveInput || (StdIn != StdOff)),
  FStdOut(StdOut),
  FStdIn(StdIn)
{
  F->RequestEvent = Checked(OpenEventW(EVENT_ALL_ACCESS, FALSE,
    (UnicodeString(CONSOLE_EVENT_REQUEST) + Instance).c_str()), L"Request event");
  F->ResponseEvent = Checked(OpenEventW(EVENT_ALL_ACCESS, FALSE,
    (UnicodeString(CONSOLE_EVENT_RESPONSE) + Instance).c_str()), L"Response event");
  F->CancelEvent = Checked(OpenEventW(EVENT_ALL_ACCESS, FALSE,
    (UnicodeString(CONSOLE_EVENT_CANCEL) + Instance).c_str()), L"Cancel event");
  F->FileMapping = Checked(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE,
    (UnicodeString(CONSOLE_MAPPING) + Instance).c_str()), L"File mapping");

  // The caller keeps the only reference to the job: if it is killed, so
  // are we.
  HANDLE Job = OpenJobObjectW(JOB_OBJECT_ASSIGN_PROCESS, FALSE, (UnicodeString(CONSOLE_JOB) + Instance).c_str());
  if (Job != NULL)
  {
    AssignProcessToJobObject(Job, GetCurrentProcess());
    CloseHandle(Job);
  }

  F->Comm = static_cast<TConsoleCommStruct *>(MapViewOfFile(F->FileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
  if (F->Comm == NULL)
  {
    throw Exception(LoadStr(CONSOLE_COMM_ERROR));
  }
  if (F->Comm->Version != TConsoleCommStruct::CurrentVersion)
  {
    throw Exception(FMTLOAD(EXTERNAL_CONSOLE_INCOMPATIBLE, (F->Comm->Version)));
  }
  F->Comm->Version = TConsoleCommStruct::CurrentVersionConfirmed;

  // INIT: the caller reports its own input and output.
  {
    std::lock_guard<std::recursive_mutex> Lock(F->Section);
    F->Comm->Event = TConsoleCommStruct::INIT;
    F->Comm->InitEvent.WantsProgress = false;
    F->Comm->InitEvent.UseStdErr = (FStdOut != StdOff);
    F->Comm->InitEvent.OutputFormat = static_cast<TConsoleCommStruct::TInitEvent::STDINOUT>(FStdOut);
    F->Comm->InitEvent.InputFormat = static_cast<TConsoleCommStruct::TInitEvent::STDINOUT>(FStdIn);
    F->Send();
    const unsigned int OutputType = F->Comm->InitEvent.OutputType;
    const unsigned int InputType = F->Comm->InitEvent.InputType;
    FLimitedOutput = (OutputType == FILE_TYPE_CHAR);
    FLiveOutput = (OutputType != FILE_TYPE_DISK) && (OutputType != FILE_TYPE_PIPE);
    FInteractive = (InputType != FILE_TYPE_DISK) && (InputType != FILE_TYPE_PIPE);
    FWantsProgress = F->Comm->InitEvent.WantsProgress;
  }
}

TExternalConsole::~TExternalConsole()
{
  if (F->Comm != NULL)
  {
    UnmapViewOfFile(F->Comm);
  }
  for (HANDLE H : { F->RequestEvent, F->ResponseEvent, F->CancelEvent, F->FileMapping })
  {
    if (H != NULL)
    {
      CloseHandle(H);
    }
  }
}

void TExternalConsole::Print(UnicodeString Str, bool FromBeginning, bool Error)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  // At least one round, even for an empty string (a new line).
  do
  {
    const int MaxLen = static_cast<int>(std::size(F->Comm->PrintEvent.Message) - 1);
    const UnicodeString Piece = Str.SubString(1, MaxLen);
    Str.Delete(1, MaxLen);
    F->Comm->Event = TConsoleCommStruct::PRINT;
    CopyOut(F->Comm->PrintEvent.Message, std::size(F->Comm->PrintEvent.Message), Piece);
    F->Comm->PrintEvent.FromBeginning = FromBeginning;
    F->Comm->PrintEvent.Error = Error;
    // Later pieces append.
    FromBeginning = false;
    F->Send();
  }
  while (!Str.IsEmpty());
}

bool TExternalConsole::Input(UnicodeString & Str, bool Echo, unsigned int Timer)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  F->Comm->Event = TConsoleCommStruct::INPUT;
  F->Comm->InputEvent.Echo = Echo;
  F->Comm->InputEvent.Result = false;
  F->Comm->InputEvent.Str[0] = L'\0';
  F->Comm->InputEvent.Timer = Timer;
  F->Send();
  Str = F->Comm->InputEvent.Str;
  TrimNewLine(Str);
  return F->Comm->InputEvent.Result;
}

int TExternalConsole::Choice(const UnicodeString & Options, int Cancel, int Break, int Continue, int Timeouted,
  bool Timeouting, unsigned int Timer, const UnicodeString & Message)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  F->Comm->Event = TConsoleCommStruct::CHOICE;
  CopyOut(F->Comm->ChoiceEvent.Options, std::size(F->Comm->ChoiceEvent.Options), Options);
  F->Comm->ChoiceEvent.Cancel = Cancel;
  F->Comm->ChoiceEvent.Break = Break;
  F->Comm->ChoiceEvent.Result = Break;
  F->Comm->ChoiceEvent.Continue = Continue;
  F->Comm->ChoiceEvent.Timeouted = Timeouted;
  F->Comm->ChoiceEvent.Timer = Timer;
  F->Comm->ChoiceEvent.Timeouting = Timeouting;
  CopyOut(F->Comm->ChoiceEvent.Message, std::size(F->Comm->ChoiceEvent.Message), Message);
  F->Send();
  return F->Comm->ChoiceEvent.Result;
}

bool TExternalConsole::PendingAbort()
{
  return WaitForSingleObject(F->CancelEvent, 0) == WAIT_OBJECT_0;
}

void TExternalConsole::SetTitle(const UnicodeString & Title)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  F->Comm->Event = TConsoleCommStruct::TITLE;
  CopyOut(F->Comm->TitleEvent.Title, std::size(F->Comm->TitleEvent.Title), Title);
  F->Send();
}

void TExternalConsole::Progress(TScriptProgress & Progress)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  typedef TConsoleCommStruct::TProgressEvent TProgressEvent;
  TProgressEvent & Event = F->Comm->ProgressEvent;
  F->Comm->Event = TConsoleCommStruct::PROGRESS;
  Event.Operation = TProgressEvent::COPY;
  Event.Side = (Progress.Side == osLocal) ? TProgressEvent::LOCAL : TProgressEvent::REMOTE;
  CopyOut(Event.FileName, std::size(Event.FileName), Progress.FileName);
  CopyOut(Event.Directory, std::size(Event.Directory), Progress.Directory);
  Event.OverallProgress = Progress.OverallProgress;
  Event.FileProgress = Progress.FileProgress;
  Event.CPS = Progress.CPS;
  Event.Cancel = Progress.Cancel;
  F->Send();
  Progress.Cancel = Event.Cancel;
}

void TExternalConsole::TransferOut(const unsigned char * Data, size_t Len)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  size_t Offset = 0;
  do
  {
    F->Comm->Event = TConsoleCommStruct::TRANSFEROUT;
    const size_t Block = std::min(Len - Offset, sizeof(F->Comm->TransferEvent.Data));
    if (Block > 0)
    {
      std::memcpy(F->Comm->TransferEvent.Data, Data + Offset, Block);
    }
    F->Comm->TransferEvent.Len = static_cast<unsigned int>(Block);
    Offset += Block;
    F->Send();
  }
  while (Offset < Len);
}

size_t TExternalConsole::TransferIn(unsigned char * Data, size_t Len)
{
  std::lock_guard<std::recursive_mutex> Lock(F->Section);
  size_t Offset = 0;
  size_t Result = 0;
  while ((Result == Offset) && (Offset < Len))
  {
    const size_t Block = std::min(Len - Offset, sizeof(F->Comm->TransferEvent.Data));
    F->Comm->Event = TConsoleCommStruct::TRANSFERIN;
    F->Comm->TransferEvent.Len = static_cast<unsigned int>(Block);
    F->Comm->TransferEvent.Error = false;
    F->Send();
    if (F->Comm->TransferEvent.Error)
    {
      throw Exception(LoadStr(STREAM_READ_ERROR));
    }
    const size_t Got = std::min(static_cast<size_t>(F->Comm->TransferEvent.Len), Block);
    std::memcpy(Data + Offset, F->Comm->TransferEvent.Data, Got);
    Result += Got;
    Offset += Block;
  }
  return Result;
}

UnicodeString TExternalConsole::FinalLogMessage()
{
  return FORMAT(L"Max roundtrip: %d", (static_cast<int>(F->MaxSend)));
}

#endif
