// The frontend half of core's Interface.h, for a process with no GUI.
//
// Core declares these and leaves their definition to the application; WinSCP
// defines them across windows/ and forms/, against the VCL. This is the
// headless set: what a command-line client or a test harness links. Where
// upstream's definition needs nothing beyond core, it is followed line for
// line; where it needs a GUI, the comment says what stands in for it.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>

#include <Common.h>
#include <memory>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <Interface.h>
#include <TextsWin.h>
#include "ProgParams.h"
//---------------------------------------------------------------------------
namespace
{
// windows/WinInterface.h's, which brings the VCL forms with it.
const wchar_t * const IniSwitch = L"Ini";
//---------------------------------------------------------------------------
// windows/GUITools.cpp's, verbatim; the rest of that file is GUI.
UnicodeString UniqTempDir(const UnicodeString BaseDir, const UnicodeString Identity,
  bool Mask)
{
  DebugAssert(!BaseDir.IsEmpty());
  UnicodeString TempDir;
  do
  {
    TempDir = IncludeTrailingBackslash(BaseDir) + Identity;
    if (Mask)
    {
      TempDir += L"?????";
    }
    else
    {
      TempDir += IncludeTrailingBackslash(FormatDateTime(L"nnzzz", Now()));
    }
  }
  while (!Mask && DirectoryExists(ApiPath(TempDir)));

  return TempDir;
}
//---------------------------------------------------------------------------
// TConfiguration leaves only TemporaryDir abstract; upstream's
// TWinConfiguration adds the GUI's settings on top. The directory is
// upstream's (UniqTempDir, "scp" prefix) under the system's temporary
// directory, as TWinConfiguration's is when no custom one is configured.
class THeadlessConfiguration : public TConfiguration
{
public:
  virtual UnicodeString TemporaryDir(bool Mask)
  {
    return UniqTempDir(SystemTemporaryDirectory(), L"scp", Mask);
  }
};
}
//---------------------------------------------------------------------------
// The /ini and /rawconfig switches, as UserInterface.cpp handles them; the
// raw configuration only without /unsafe, as its CheckSafe has it.
TConfiguration * __fastcall CreateConfiguration()
{
  TConfiguration * Result = new THeadlessConfiguration();
  TProgramParams * Params = TProgramParams::Instance();
  UnicodeString IniFileName = Params->SwitchValue(IniSwitch);
  if (!IniFileName.IsEmpty())
  {
    if (SameText(IniFileName, INI_NUL))
    {
      Result->SetNulStorage();
    }
    else
    {
      IniFileName = ExpandFileName(ExpandEnvironmentVariables(IniFileName));
      Result->SetExplicitIniFileStorageName(IniFileName);
    }
  }
  if (!Params->FindSwitch(L"Unsafe"))
  {
    std::unique_ptr<TStrings> RawConfig(new TStringList());
    if (Params->FindSwitch(L"RawConfig", RawConfig.get()))
    {
      Result->OptionsStorage = RawConfig.get();
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
TOptions * __fastcall GetGlobalOptions()
{
  return TProgramParams::Instance();
}
//---------------------------------------------------------------------------
// The message and its details go to stderr, where a console client reports.
void __fastcall ShowExtendedException(Exception * E)
{
  UnicodeString Message;
  if (ExceptionMessageFormatted(E, Message))
  {
    ExtException * EE = dynamic_cast<ExtException *>(E);
    if ((EE != NULL) && (EE->MoreMessages != NULL) && !EE->MoreMessages->Text.IsEmpty())
    {
      Message += L"\n" + EE->MoreMessages->Text;
    }
    std::fputs(UTF8String(Message + L"\n").c_str(), stderr);
    std::fflush(stderr);
  }
}
//---------------------------------------------------------------------------
// As upstream's WinInterface.cpp: only core and resource strings involved.
void AppendStackTrace(TStrings * MoreMessages, TStrings * StackTrace)
{
  if (!MoreMessages->Text.IsEmpty())
  {
    MoreMessages->Text = MoreMessages->Text + "\n";
  }
  MoreMessages->Text = MoreMessages->Text + LoadStr(STACK_TRACE) + "\n";
  MoreMessages->AddStrings(StackTrace);
}
//---------------------------------------------------------------------------
// Upstream walks the stack with JCL, a C++Builder-only library; nothing
// records traces here, so an exception never carries one to append or report.
bool AppendExceptionStackTrace(Exception * /*E*/, TStrings *& /*MoreMessages*/)
{
  return false;
}
//---------------------------------------------------------------------------
// The caller owns the list; it is empty, for the same reason.
TStrings * GetCurrentStackTrace()
{
  return new TStringList();
}
//---------------------------------------------------------------------------
UnicodeString GetExceptionDebugInfo(Exception * /*E*/)
{
  return UnicodeString();
}
//---------------------------------------------------------------------------
// Stored settings are WinSCP's own: on Windows linscp reads and writes the
// same registry key, so existing sites carry over. Only the name it gives
// servers is its own (AppNameString, SshVersionString).
UnicodeString __fastcall GetCompanyRegistryKey()
{
  return L"Software\\Martin Prikryl";
}
//---------------------------------------------------------------------------
UnicodeString __fastcall GetRegistryKey()
{
  return GetCompanyRegistryKey() + L"\\WinSCP 2";
}
//---------------------------------------------------------------------------
// Upstream swaps in the hourglass cursor; there is no cursor.
void * __fastcall BusyStart()
{
  return NULL;
}
//---------------------------------------------------------------------------
void __fastcall BusyEnd(void * /*Token*/)
{
}
//---------------------------------------------------------------------------
// Upstream pumps the VCL message queue at most every GUIUpdateInterval and
// reports whether it did. There is no queue to pump.
bool __fastcall ProcessGUI(bool /*Force*/)
{
  return false;
}
//---------------------------------------------------------------------------
// Keeps the machine from sleeping during a transfer. Upstream does this with
// SetThreadExecutionState, from a helper thread on Windows 11, where the
// state does not outlive the calling thread's next idle; the direct call is
// the one upstream makes elsewhere. POSIX has no equivalent without a session
// bus, so a transfer there does not hold off sleep.
void SystemRequired()
{
#ifdef _WIN32
  SetThreadExecutionState(ES_SYSTEM_REQUIRED);
#endif
}
//---------------------------------------------------------------------------
// The name linscp gives servers: the HTTP User-Agent, the S3 role session,
// FTP's CSID, PuTTY's appname.
UnicodeString __fastcall AppNameString()
{
  return L"linscp";
}
//---------------------------------------------------------------------------
UnicodeString __fastcall SshVersionString()
{
  return FORMAT(L"linscp-release-%s", (Configuration->Version));
}
//---------------------------------------------------------------------------
// Only reachable from the Copy button of a GUI prompt.
void __fastcall CopyToClipboard(UnicodeString /*Text*/)
{
  throw Exception(L"No clipboard is available.");
}
//---------------------------------------------------------------------------
// Core asks before offering to paste a password; there is nothing to paste.
bool __fastcall TextFromClipboard(UnicodeString & /*Text*/, bool /*Trim*/)
{
  return false;
}
//---------------------------------------------------------------------------
namespace
{
struct TThreadParam
{
  TThreadFunc ThreadFunc;
  void * Parameter;
};
//---------------------------------------------------------------------------
int __fastcall ThreadProc(void * AParam)
{
  TThreadParam * Param = reinterpret_cast<TThreadParam *>(AParam);
  unsigned int Result = Param->ThreadFunc(Param->Parameter);
  delete Param;
  EndThread(Result);
  return Result;
}
}
//---------------------------------------------------------------------------
// As upstream.
THandle StartThread(void * SecurityAttributes, unsigned StackSize,
  TThreadFunc ThreadFunc, void * Parameter, unsigned CreationFlags,
  TThreadID & ThreadId)
{
  TThreadParam * Param = new TThreadParam;
  Param->ThreadFunc = ThreadFunc;
  Param->Parameter = Parameter;
  return BeginThread(SecurityAttributes, StackSize, ThreadProc, Param,
    CreationFlags, ThreadId);
}
//---------------------------------------------------------------------------
// Names as upstream's MessageDlg.cpp gives them; core logs these when a
// script answers a prompt. The captions are the English text of the VCL's
// own resource strings (SMsgDlgYes, ...) where upstream uses those, and
// WinSCP's where it has its own.
void __fastcall AnswerNameAndCaption(
  unsigned int Answer, UnicodeString & Name, UnicodeString & Caption)
{
  switch (Answer)
  {
    case qaYes: Caption = L"&Yes"; Name = L"Yes"; break;
    case qaNo: Caption = L"&No"; Name = L"No"; break;
    case qaOK: Caption = L"OK"; Name = L"OK"; break;
    case qaCancel: Caption = L"Cancel"; Name = L"Cancel"; break;
    case qaAbort: Caption = L"&Abort"; Name = L"Abort"; break;
    case qaRetry: Caption = L"&Retry"; Name = L"Retry"; break;
    case qaIgnore: Caption = L"&Ignore"; Name = L"Ignore"; break;
    case qaAll: Caption = LoadStr(ALL_BUTTON); Name = L"All"; break;
    case qaNoToAll: Caption = L"N&o to All"; Name = L"NoToAll"; break;
    case qaYesToAll: Caption = LoadStr(YES_TO_ALL_BUTTON); Name = L"YesToAll"; break;
    case qaHelp: Caption = L"&Help"; Name = L"Help"; break;
    case qaSkip: Caption = LoadStr(SKIP_BUTTON); Name = L"Skip"; break;
    case qaReport: Caption = LoadStr(REPORT_BUTTON); Name = L"Report"; break;
    default:
      DebugFail();
      throw Exception(L"Undefined answer");
  }
}
