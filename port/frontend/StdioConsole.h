// The command-line client's console: standard input and output, as
// winscp.com presents them to upstream's TConsoleRunner (its
// TExternalConsole side, console/Main.cpp), without the shared memory
// between two processes.
//
// What winscp.com does, kept here:
//  - Output is UTF-8. To a terminal, a "from beginning" print rewrites the
//    line with a carriage return; to a file or pipe it is held until the next
//    print, so progress lines do not pile up.
//  - Input from a terminal is a line, with echo off for secrets; it fails on
//    Ctrl+C, at end of input, or when a timer runs out. Input from a file or
//    pipe is a UTF-8 line, echoed to the output, failing only at its end.
//  - A choice from a terminal is one key: an accelerator, Esc (cancel) or
//    Ctrl+C (break), with an optional timer. From a file or pipe there is
//    nothing to read: a timed choice waits it out, any other breaks.
//  - With /stdout, messages move to standard error and standard output
//    carries the transferred data, raw or chunked; /stdin reads data alike.
#ifndef LINSCP_STDIO_CONSOLE_H
#define LINSCP_STDIO_CONSOLE_H

#include <Classes.hpp>
#include <cstddef>

class TStdioConsole
{
public:
  enum TStdInOut { StdOff, StdBinary, StdChunked };

  TStdioConsole(TStdInOut StdOut, TStdInOut StdIn, bool NoInteractiveInput);
  ~TStdioConsole();

  void Print(UnicodeString Str, bool FromBeginning = false, bool Error = false);
  void PrintLine(const UnicodeString & Str = UnicodeString(), bool Error = false);
  // A line, without its end. Timer is in milliseconds, 0 for none.
  bool Input(UnicodeString & Str, bool Echo, unsigned int Timer);
  // The 1-based index of the accelerator pressed in Options, or Cancel,
  // Break or Timeouted.
  int Choice(const UnicodeString & Options, int Cancel, int Break, int Timeouted, bool Timeouting,
    unsigned int Timer);
  // Whether Ctrl+C was pressed since the last call.
  bool PendingAbort();
  void SetTitle(const UnicodeString & Title);
  void TransferOut(const unsigned char * Data, size_t Len);
  size_t TransferIn(unsigned char * Data, size_t Len);

  // TConsole's flags, as TExternalConsole derives them.
  bool LimitedOutput() const { return FOutputIsTerminal; }
  bool LiveOutput() const { return FOutputIsTerminal; }
  bool Interactive() const { return FInputIsTerminal; }
  bool NoInteractiveInput() const { return FNoInteractiveInput; }
  bool StdOut() const { return FStdOut != StdOff; }
  bool StdIn() const { return FStdIn != StdOff; }

private:
  TStdInOut FStdOut;
  TStdInOut FStdIn;
  bool FNoInteractiveInput;
  bool FInputIsTerminal;
  bool FOutputIsTerminal;
  // A from-beginning print to a file or pipe, written once superseded.
  UnicodeString FLastFromBeginning;

  void Write(const UnicodeString & Str);
  void Flush();
  bool ReadPipedLine(UnicodeString & Str);
  bool ReadTerminalLine(UnicodeString & Str, bool Echo, unsigned int Timer);
  int ReadTerminalKey(const UnicodeString & Options, int Cancel, int Break, int Timeouted, unsigned int Timer);
};

#endif
