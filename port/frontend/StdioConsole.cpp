#include <vcl.h>
#pragma hdrstop

#include "StdioConsole.h"

#include <Common.h>
#include <csignal>
#include <cstdio>
#include <string>

#if defined(_WIN32)
  #include <fcntl.h>
  #include <io.h>
#else
  #include <cerrno>
  #include <poll.h>
  #include <termios.h>
  #include <unistd.h>
#endif
//---------------------------------------------------------------------------
namespace {

// Set by Ctrl+C, consumed by PendingAbort (or by the read it interrupted).
volatile std::sig_atomic_t GAbort = 0;

bool TakeAbort()
{
  const bool Result = (GAbort != 0);
  GAbort = 0;
  return Result;
}

const unsigned int ChoiceSlice = 50;

void TrimNewLine(UnicodeString & Str)
{
  while (!Str.IsEmpty() && ((Str[Str.Length()] == L'\n') || (Str[Str.Length()] == L'\r')))
  {
    Str.SetLength(Str.Length() - 1);
  }
}

#if defined(_WIN32)

BOOL WINAPI CtrlHandler(DWORD CtrlType)
{
  if ((CtrlType == CTRL_C_EVENT) || (CtrlType == CTRL_BREAK_EVENT))
  {
    GAbort = 1;
    return TRUE;
  }
  return FALSE;
}

bool IsConsole(HANDLE Handle)
{
  DWORD Mode;
  return (GetFileType(Handle) == FILE_TYPE_CHAR) && (GetConsoleMode(Handle, &Mode) != 0);
}

#else

void SigIntHandler(int)
{
  GAbort = 1;
}

// The terminal's settings for the duration of one read.
class TTerminalMode
{
public:
  explicit TTerminalMode(tcflag_t Clear)
  {
    FValid = (tcgetattr(STDIN_FILENO, &FSaved) == 0);
    if (FValid)
    {
      termios Mode = FSaved;
      Mode.c_lflag &= ~Clear;
      Mode.c_cc[VMIN] = 1;
      Mode.c_cc[VTIME] = 0;
      tcsetattr(STDIN_FILENO, TCSANOW, &Mode);
    }
  }
  ~TTerminalMode()
  {
    if (FValid)
    {
      tcsetattr(STDIN_FILENO, TCSANOW, &FSaved);
    }
  }

private:
  termios FSaved;
  bool FValid;
};

// Waits up to Timeout ms (negative: forever) for input. 1 ready, 0 timed
// out, -1 interrupted by Ctrl+C.
int WaitForInput(int Timeout)
{
  pollfd P = { STDIN_FILENO, POLLIN, 0 };
  for (;;)
  {
    const int N = poll(&P, 1, Timeout);
    if (N > 0)
    {
      return 1;
    }
    if (N == 0)
    {
      return 0;
    }
    if (errno != EINTR)
    {
      return 1; // let the read report it
    }
    if (GAbort != 0)
    {
      return -1;
    }
  }
}

#endif

}  // namespace
//---------------------------------------------------------------------------
TStdioConsole::TStdioConsole(TStdInOut StdOut, TStdInOut StdIn, bool NoInteractiveInput) :
  FStdOut(StdOut),
  FStdIn(StdIn),
  FNoInteractiveInput(NoInteractiveInput || (StdIn != StdOff))
{
#if defined(_WIN32)
  SetConsoleCtrlHandler(CtrlHandler, TRUE);
  FInputIsTerminal = IsConsole(GetStdHandle(STD_INPUT_HANDLE));
  FOutputIsTerminal = IsConsole(GetStdHandle(StdOut != StdOff ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE));
  SetConsoleOutputCP(CP_UTF8);
  if (StdOut != StdOff)
  {
    _setmode(_fileno(stdout), _O_BINARY);
  }
  if (StdIn != StdOff)
  {
    _setmode(_fileno(stdin), _O_BINARY);
  }
#else
  // Without SA_RESTART, so a read or wait in progress returns to look.
  struct sigaction Action = {};
  Action.sa_handler = SigIntHandler;
  sigemptyset(&Action.sa_mask);
  sigaction(SIGINT, &Action, nullptr);
  FInputIsTerminal = (isatty(STDIN_FILENO) != 0);
  FOutputIsTerminal = (isatty(StdOut != StdOff ? STDERR_FILENO : STDOUT_FILENO) != 0);
#endif
}
//---------------------------------------------------------------------------
TStdioConsole::~TStdioConsole()
{
  if (!FLastFromBeginning.IsEmpty())
  {
    Write(FLastFromBeginning);
  }
  Flush();
}
//---------------------------------------------------------------------------
void TStdioConsole::Write(const UnicodeString & Str)
{
  std::FILE * Out = (FStdOut != StdOff) ? stderr : stdout;
#if defined(_WIN32)
  if (FOutputIsTerminal)
  {
    DWORD Written;
    WriteConsoleW(GetStdHandle(FStdOut != StdOff ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE),
      Str.c_str(), static_cast<DWORD>(Str.Length()), &Written, nullptr);
    return;
  }
  // As winscp.com writes to a file or pipe: CRLF line ends.
  const UTF8String Text(ReplaceStr(Str, L"\n", L"\r\n"));
#else
  const UTF8String Text(Str);
#endif
  std::fwrite(Text.c_str(), 1, Text.Length(), Out);
}
//---------------------------------------------------------------------------
void TStdioConsole::Flush()
{
  std::fflush((FStdOut != StdOff) ? stderr : stdout);
}
//---------------------------------------------------------------------------
void TStdioConsole::Print(UnicodeString Str, bool FromBeginning, bool /*Error*/)
{
  if (FOutputIsTerminal)
  {
    if (FromBeginning)
    {
      Write(L"\r");
    }
    Write(Str);
  }
  else if (FromBeginning && !Str.IsEmpty() && (Str[1] != L'\n'))
  {
    FLastFromBeginning = Str;
    return;
  }
  else
  {
    if (!FLastFromBeginning.IsEmpty())
    {
      Write(FLastFromBeginning);
      FLastFromBeginning = UnicodeString();
    }
    if (FromBeginning && !Str.IsEmpty() && (Str[1] == L'\n'))
    {
      Write(L"\n");
      FLastFromBeginning = Str.SubString(2, Str.Length() - 1);
    }
    else
    {
      Write(Str);
    }
  }
  Flush();
}
//---------------------------------------------------------------------------
void TStdioConsole::PrintLine(const UnicodeString & Str, bool Error)
{
  Print(Str + L"\n", false, Error);
}
//---------------------------------------------------------------------------
bool TStdioConsole::PendingAbort()
{
  return TakeAbort();
}
//---------------------------------------------------------------------------
void TStdioConsole::SetTitle(const UnicodeString & Title)
{
#if defined(_WIN32)
  SetConsoleTitleW(Title.c_str());
#else
  // A terminal's title is the shell's; writing escape sequences into what
  // may be a log is worse than leaving it.
  (void)Title;
#endif
}
//---------------------------------------------------------------------------
bool TStdioConsole::ReadPipedLine(UnicodeString & Str)
{
  std::string Line;
  int C;
  bool Any = false;
  while ((C = std::fgetc(stdin)) != EOF)
  {
    Any = true;
    if (C == '\n')
    {
      break;
    }
    if (C != '\r')
    {
      Line += static_cast<char>(C);
    }
  }
  Str = UTF8ToString(Line.c_str());
  // winscp.com echoes what it read, so a log of the session shows it.
  Print(Str);
  Print(L"\n");
  return Any;
}
//---------------------------------------------------------------------------
bool TStdioConsole::ReadTerminalLine(UnicodeString & Str, bool Echo, unsigned int Timer)
{
  bool Cancelled = false;
  bool Result = false;
#if defined(_WIN32)
  HANDLE In = GetStdHandle(STD_INPUT_HANDLE);
  DWORD PrevMode;
  GetConsoleMode(In, &PrevMode);
  DWORD Mode = PrevMode | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT;
  Mode = Echo ? (Mode | ENABLE_ECHO_INPUT) : (Mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
  SetConsoleMode(In, Mode);
  // The timer runs until the first key; once typing has begun, the line is
  // waited for.
  bool Timeout = false;
  if (Timer > 0)
  {
    unsigned int Remaining = Timer;
    for (;;)
    {
      INPUT_RECORD Record;
      DWORD Read = 0;
      if (PeekConsoleInputW(In, &Record, 1, &Read) && (Read == 1))
      {
        if ((Record.EventType == KEY_EVENT) && Record.Event.KeyEvent.bKeyDown)
        {
          break;
        }
        ReadConsoleInputW(In, &Record, 1, &Read);
        continue;
      }
      if ((GAbort != 0) || (Remaining == 0))
      {
        Timeout = (GAbort == 0);
        break;
      }
      const unsigned int Slice = (Remaining < ChoiceSlice) ? Remaining : ChoiceSlice;
      Sleep(Slice);
      Remaining -= Slice;
    }
  }
  if (!Timeout && (GAbort == 0))
  {
    wchar_t Buffer[10240];
    DWORD Read = 0;
    Result = (ReadConsoleW(In, Buffer, static_cast<DWORD>(std::size(Buffer) - 1), &Read, nullptr) != 0) &&
      (Read > 0);
    Buffer[Read] = L'\0';
    Str = Buffer;
  }
  Cancelled = TakeAbort();
  SetConsoleMode(In, PrevMode);
#else
  TTerminalMode Mode(Echo ? 0 : static_cast<tcflag_t>(ECHO));
  std::string Line;
  bool Ended = false;
  const auto Start = GetTickCount();
  while (!Ended)
  {
    int Timeout = -1;
    if (Timer > 0)
    {
      const auto Elapsed = GetTickCount() - Start;
      Timeout = (Elapsed >= Timer) ? 0 : static_cast<int>(Timer - Elapsed);
    }
    const int Ready = WaitForInput(Timeout);
    if (Ready < 0)
    {
      Cancelled = true;
      break;
    }
    if (Ready == 0)
    {
      break;
    }
    char Buffer[256];
    const ssize_t N = read(STDIN_FILENO, Buffer, sizeof(Buffer));
    if (N < 0)
    {
      if ((errno == EINTR) && (GAbort != 0))
      {
        Cancelled = true;
        break;
      }
      continue;
    }
    if (N == 0)
    {
      break; // end of input
    }
    for (ssize_t I = 0; I < N; ++I)
    {
      if (Buffer[I] == '\n')
      {
        Ended = true;
        break;
      }
      Line += Buffer[I];
    }
  }
  Result = Ended && !Cancelled;
  Str = UTF8ToString(Line.c_str());
  TakeAbort();
#endif
  TrimNewLine(Str);
  if (Cancelled || !Echo)
  {
    Print(L"\n");
  }
  return Result && !Cancelled;
}
//---------------------------------------------------------------------------
bool TStdioConsole::Input(UnicodeString & Str, bool Echo, unsigned int Timer)
{
  Flush();
  return FInputIsTerminal ? ReadTerminalLine(Str, Echo, Timer) : ReadPipedLine(Str);
}
//---------------------------------------------------------------------------
int TStdioConsole::ReadTerminalKey(
  const UnicodeString & Options, int Cancel, int Break, int Timeouted, unsigned int Timer)
{
  unsigned int Remaining = Timer;
  int Result = 0;
#if defined(_WIN32)
  HANDLE In = GetStdHandle(STD_INPUT_HANDLE);
  DWORD PrevMode;
  GetConsoleMode(In, &PrevMode);
  SetConsoleMode(In, (PrevMode | ENABLE_PROCESSED_INPUT) & ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
  while (Result == 0)
  {
    INPUT_RECORD Record;
    DWORD Read = 0;
    if (PeekConsoleInputW(In, &Record, 1, &Read) && (Read == 1) &&
        ReadConsoleInputW(In, &Record, 1, &Read) && (Read == 1) &&
        (Record.EventType == KEY_EVENT) && Record.Event.KeyEvent.bKeyDown &&
        (Record.Event.KeyEvent.uChar.UnicodeChar != 0))
    {
      const wchar_t C = UpperCase(UnicodeString(Record.Event.KeyEvent.uChar.UnicodeChar))[1];
      const bool Modified = (Record.Event.KeyEvent.dwControlKeyState &
        (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED | LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
      if (C == 27)
      {
        Result = Cancel;
      }
      else if (!Modified && (Options.Pos(C) > 0))
      {
        Result = Options.Pos(C);
      }
    }
    if (TakeAbort())
    {
      Result = Break;
    }
    if (Result == 0)
    {
      Sleep(ChoiceSlice);
      if (Timer > 0)
      {
        if (Remaining > ChoiceSlice)
        {
          Remaining -= ChoiceSlice;
        }
        else
        {
          Result = Timeouted;
        }
      }
    }
  }
  SetConsoleMode(In, PrevMode);
#else
  TTerminalMode Mode(static_cast<tcflag_t>(ICANON | ECHO));
  while (Result == 0)
  {
    const int Ready = WaitForInput(static_cast<int>(ChoiceSlice));
    if ((Ready < 0) || TakeAbort())
    {
      Result = Break;
      break;
    }
    if (Ready > 0)
    {
      unsigned char C;
      if (read(STDIN_FILENO, &C, 1) == 1)
      {
        if (C == 27)
        {
          Result = Cancel;
        }
        else if (C < 0x80)
        {
          const wchar_t Key = UpperCase(UnicodeString(static_cast<wchar_t>(C)))[1];
          if (Options.Pos(Key) > 0)
          {
            Result = Options.Pos(Key);
          }
        }
      }
      continue;
    }
    if (Timer > 0)
    {
      if (Remaining > ChoiceSlice)
      {
        Remaining -= ChoiceSlice;
      }
      else
      {
        Result = Timeouted;
      }
    }
  }
#endif
  return Result;
}
//---------------------------------------------------------------------------
int TStdioConsole::Choice(
  const UnicodeString & Options, int Cancel, int Break, int Timeouted, bool Timeouting, unsigned int Timer)
{
  Flush();
  if (!FInputIsTerminal)
  {
    // Nothing to read an answer from.
    if (Timeouting)
    {
      Sleep(Timer);
      return Timeouted;
    }
    return Break;
  }
  return ReadTerminalKey(Options, Cancel, Break, Timeouted, Timer);
}
//---------------------------------------------------------------------------
void TStdioConsole::TransferOut(const unsigned char * Data, size_t Len)
{
  if (FStdOut == StdChunked)
  {
    std::fprintf(stdout, "%x\r\n", static_cast<unsigned int>(Len));
    std::fwrite(Data, 1, Len, stdout);
    std::fputs("\r\n", stdout);
  }
  else
  {
    std::fwrite(Data, 1, Len, stdout);
  }
  std::fflush(stdout);
}
//---------------------------------------------------------------------------
size_t TStdioConsole::TransferIn(unsigned char * Data, size_t Len)
{
  const size_t Read = std::fread(Data, 1, Len, stdin);
  if ((Read != Len) && std::ferror(stdin))
  {
    throw Exception(L"Error reading standard input");
  }
  return Read;
}
