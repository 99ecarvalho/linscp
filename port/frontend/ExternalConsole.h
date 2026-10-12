// winscp.com's console protocol (/consoleinstance), for the program WinSCP's
// .NET assembly and winscp.com start: see ExternalConsole.cpp. Windows only.
#ifndef LINSCP_EXTERNAL_CONSOLE_H
#define LINSCP_EXTERNAL_CONSOLE_H

#include "CliConsole.h"

#if defined(_WIN32)

#include <memory>

class TExternalConsole : public TCliConsole
{
public:
  TExternalConsole(const UnicodeString & Instance, bool NoInteractiveInput, TStdInOut StdOut, TStdInOut StdIn);
  ~TExternalConsole() override;

  void Print(UnicodeString Str, bool FromBeginning = false, bool Error = false) override;
  bool Input(UnicodeString & Str, bool Echo, unsigned int Timer) override;
  int Choice(const UnicodeString & Options, int Cancel, int Break, int Continue, int Timeouted,
    bool Timeouting, unsigned int Timer, const UnicodeString & Message) override;
  bool PendingAbort() override;
  void SetTitle(const UnicodeString & Title) override;
  void TransferOut(const unsigned char * Data, size_t Len) override;
  size_t TransferIn(unsigned char * Data, size_t Len) override;
  void Progress(TScriptProgress & Progress) override;
  UnicodeString FinalLogMessage() override;

  bool LimitedOutput() const override { return FLimitedOutput; }
  bool LiveOutput() const override { return FLiveOutput; }
  bool Interactive() const override { return FInteractive; }
  bool NoInteractiveInput() const override { return FNoInteractiveInput; }
  bool StdOut() const override { return FStdOut != StdOff; }
  bool StdIn() const override { return FStdIn != StdOff; }
  bool WantsProgress() const override { return FWantsProgress; }

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
  bool FNoInteractiveInput;
  TStdInOut FStdOut;
  TStdInOut FStdIn;
  bool FLimitedOutput = false;
  bool FLiveOutput = false;
  bool FInteractive = false;
  bool FWantsProgress = false;
};

#endif

#endif
