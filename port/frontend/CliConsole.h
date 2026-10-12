// The console the scripting client runs on: upstream's TConsole
// (windows/WinInterface.h, a GUI header) as linscp-cli needs it.
//
// Two implementations: TStdioConsole, the process's own standard input and
// output with winscp.com's semantics, and on Windows TExternalConsole,
// winscp.com's protocol itself -- shared memory and events, as winscp.com
// and WinSCP's .NET assembly drive WinSCP.exe /consoleinstance.
#ifndef LINSCP_CLI_CONSOLE_H
#define LINSCP_CLI_CONSOLE_H

#include <Classes.hpp>
#include <cstddef>

class TScriptProgress;

class TCliConsole
{
public:
  // /stdout and /stdin: off, raw, or (out only) chunked.
  enum TStdInOut { StdOff, StdBinary, StdChunked };

  virtual ~TCliConsole() = default;

  virtual void Print(UnicodeString Str, bool FromBeginning = false, bool Error = false) = 0;
  void PrintLine(const UnicodeString & Str = UnicodeString(), bool Error = false)
  {
    Print(Str + L"\n", false, Error);
  }
  // A line, without its end. Timer is in milliseconds, 0 for none.
  virtual bool Input(UnicodeString & Str, bool Echo, unsigned int Timer) = 0;
  // The 1-based index of the accelerator chosen in Options, or Cancel, Break
  // or Timeouted; Message is the query, for a console that shows it apart.
  virtual int Choice(const UnicodeString & Options, int Cancel, int Break, int Continue, int Timeouted,
    bool Timeouting, unsigned int Timer, const UnicodeString & Message) = 0;
  // Whether the user asked to abort since the last call.
  virtual bool PendingAbort() = 0;
  virtual void SetTitle(const UnicodeString & Title) = 0;
  virtual void TransferOut(const unsigned char * Data, size_t Len) = 0;
  virtual size_t TransferIn(unsigned char * Data, size_t Len) = 0;
  virtual void Progress(TScriptProgress & /*Progress*/) {}
  // A line for the session log at exit.
  virtual UnicodeString FinalLogMessage() { return UnicodeString(); }

  // TConsole's flags.
  virtual bool LimitedOutput() const = 0;
  virtual bool LiveOutput() const = 0;
  virtual bool Interactive() const = 0;
  virtual bool NoInteractiveInput() const = 0;
  virtual bool StdOut() const = 0;
  virtual bool StdIn() const = 0;
  virtual bool WantsProgress() const { return false; }
};

#endif
