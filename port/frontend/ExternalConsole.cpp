// winscp.com's console protocol, as TExternalConsole speaks it in upstream's
// windows/ConsoleRunner.cpp: each exchange fills in a TConsoleCommStruct
// (console/Console.h, upstream's own header), hands it to the caller -- winscp.com,
// or WinSCP's .NET assembly -- and waits for it to come back answered.
//
// On Windows, as upstream: the caller starts the program with
// /console /consoleinstance=<name> and owns a file mapping holding the
// structure and three events. The request event hands it over, the response
// event hands it back, and the cancel event says the user asked to abort. The
// process joins the caller's job object, so it ends when the caller does.
//
// Elsewhere, where .NET has neither named events nor file mappings, the
// caller starts it with /console /consolepipes=<read>,<write>: two inherited
// pipe descriptors. The caller first writes the structure as it initialised
// it, header and all; then each request is the whole structure written to
// <write>, and its response the whole structure read back from <read>. With
// the descriptors the only references to the pipes' ends, the end of <read>
// means the caller has gone, and the process aborts as the job would end it.
#include <vcl.h>
#pragma hdrstop

#include "ExternalConsole.h"

#include <Common.h>
#include <Script.h>
#include <TextsCore.h>
#include <TextsWin.h>
#include <FileOperationProgress.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <mutex>

#if !defined(_WIN32)
  #include <cerrno>
  #include <csignal>
  #include <fcntl.h>
  #include <poll.h>
  #include <unistd.h>
#endif

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
#if defined(_WIN32)
  HANDLE RequestEvent = NULL;
  HANDLE ResponseEvent = NULL;
  HANDLE CancelEvent = NULL;
  HANDLE FileMapping = NULL;
#else
  int ReadFd = -1;
  int WriteFd = -1;
  std::unique_ptr<TConsoleCommStruct> Buffer;
#endif
  TConsoleCommStruct * Comm = NULL;
  // FTP calls TransferOut and TransferIn from its own thread.
  std::recursive_mutex Section;
  unsigned int MaxSend = 0;

  // Hands the request over and waits for the response.
  void Send()
  {
    const DWORD Start = GetTickCount();
#if defined(_WIN32)
    SetEvent(RequestEvent);
    if (WaitForSingleObject(ResponseEvent, INFINITE) != WAIT_OBJECT_0)
    {
      throw Exception(LoadStr(CONSOLE_SEND_TIMEOUT));
    }
#else
    if (!WriteAll(Comm, sizeof(*Comm)) || !ReadAll(Comm, sizeof(*Comm)))
    {
      throw Exception(LoadStr(CONSOLE_COMM_ERROR));
    }
#endif
    MaxSend = std::max(MaxSend, static_cast<unsigned int>(GetTickCount() - Start));
  }

#if !defined(_WIN32)
  bool WriteAll(const void * Data, size_t Len)
  {
    const char * P = static_cast<const char *>(Data);
    while (Len > 0)
    {
      const ssize_t N = write(WriteFd, P, Len);
      if ((N < 0) && (errno == EINTR))
      {
        continue;
      }
      if (N <= 0)
      {
        return false;
      }
      P += N;
      Len -= static_cast<size_t>(N);
    }
    return true;
  }

  bool ReadAll(void * Data, size_t Len)
  {
    char * P = static_cast<char *>(Data);
    while (Len > 0)
    {
      const ssize_t N = read(ReadFd, P, Len);
      if ((N < 0) && (errno == EINTR))
      {
        continue;
      }
      if (N <= 0)
      {
        return false;
      }
      P += N;
      Len -= static_cast<size_t>(N);
    }
    return true;
  }
#endif
};

namespace {

void InitError(const UnicodeString & Desc)
{
  throw ExtException(LoadStr(EXTERNAL_CONSOLE_INIT_ERROR), FORMAT(L"%s\n%s", (Desc, LastSysErrorMessage())));
}

#if defined(_WIN32)
HANDLE Checked(HANDLE Handle, const wchar_t * Desc)
{
  if (Handle == NULL)
  {
    InitError(Desc);
  }
  return Handle;
}
#endif

}  // namespace

TExternalConsole::TExternalConsole(const UnicodeString & Connection, bool NoInteractiveInput,
  TStdInOut StdOut, TStdInOut StdIn) :
  F(new TImpl()),
  FNoInteractiveInput(NoInteractiveInput || (StdIn != StdOff)),
  FStdOut(StdOut),
  FStdIn(StdIn)
{
#if defined(_WIN32)
  const UnicodeString & Instance = Connection;
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
#else
  const int Comma = Connection.Pos(L",");
  F->ReadFd = (Comma > 1) ? StrToIntDef(Connection.SubString(1, Comma - 1), -1) : -1;
  F->WriteFd = (Comma > 1) ? StrToIntDef(Connection.SubString(Comma + 1, Connection.Length()), -1) : -1;
  if ((F->ReadFd < 0) || (F->WriteFd < 0) ||
      (fcntl(F->ReadFd, F_SETFD, FD_CLOEXEC) < 0) || (fcntl(F->WriteFd, F_SETFD, FD_CLOEXEC) < 0))
  {
    InitError(FORMAT(L"Console pipes \"%s\"", (Connection)));
  }
  // A caller gone mid-write is an error to report, not a signal to die of.
  signal(SIGPIPE, SIG_IGN);
  F->Buffer.reset(new TConsoleCommStruct());
  F->Comm = F->Buffer.get();
  // The structure as the caller initialised it; its size first, so that a
  // structure of another size is refused rather than misread.
  const size_t HeaderSize = offsetof(TConsoleCommStruct, PrintEvent);
  if (!F->ReadAll(F->Comm, HeaderSize))
  {
    throw Exception(LoadStr(CONSOLE_COMM_ERROR));
  }
  if (F->Comm->Size != sizeof(TConsoleCommStruct))
  {
    throw Exception(FMTLOAD(EXTERNAL_CONSOLE_INCOMPATIBLE, (F->Comm->Version)));
  }
  if (!F->ReadAll(reinterpret_cast<char *>(F->Comm) + HeaderSize, sizeof(TConsoleCommStruct) - HeaderSize))
  {
    throw Exception(LoadStr(CONSOLE_COMM_ERROR));
  }
#endif
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
#if defined(_WIN32)
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
#else
  for (int Fd : { F->ReadFd, F->WriteFd })
  {
    if (Fd >= 0)
    {
      close(Fd);
    }
  }
#endif
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
#if defined(_WIN32)
  return WaitForSingleObject(F->CancelEvent, 0) == WAIT_OBJECT_0;
#else
  // No cancel event: the caller going is the abort. A response pending for
  // another thread is readable data, not a hang-up, so this needs no lock.
  pollfd Poll = { F->ReadFd, POLLIN, 0 };
  return (poll(&Poll, 1, 0) > 0) && ((Poll.revents & (POLLHUP | POLLERR)) != 0);
#endif
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
