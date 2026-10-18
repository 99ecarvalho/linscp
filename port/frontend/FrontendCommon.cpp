// The frontend half of core's Interface.h that every linscp program shares.
//
// Core declares these and leaves their definition to the application; WinSCP
// defines them across windows/ and forms/, against the VCL. These need
// nothing of a GUI: configuration, registry keys, names, threads. The rest --
// errors, the busy cursor, the event loop, the clipboard -- is the
// program's: HeadlessInterface.cpp for the command line and tests,
// port/qt/QtInterface.cpp for the Qt shell. Where upstream's definition needs
// nothing beyond core, it is followed line for line.
#include <vcl.h>
#pragma hdrstop

#include <Common.h>
#include <memory>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <Interface.h>
#include <TextsWin.h>
#include "FrontendCommon.h"
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
}
//---------------------------------------------------------------------------
// Upstream's (UniqTempDir, "scp" prefix) under the system's temporary
// directory, as TWinConfiguration's is when no custom one is configured.
UnicodeString DefaultTemporaryDir(bool Mask, const UnicodeString & Base)
{
  return UniqTempDir(Base.IsEmpty() ? SystemTemporaryDirectory() : Base, L"scp", Mask);
}
#if !defined(_WIN32)
//---------------------------------------------------------------------------
// Where the configuration lives on POSIX, which has no registry. The file is
// WinSCP's INI, under WinSCP's name, as on Windows linscp keeps WinSCP's
// registry key: a WinSCP.ini exported from Windows can be dropped in. One
// beside the executable wins, as upstream's portable mode has it (there
// named after WinSCP.exe); otherwise the user's XDG configuration
// directory, created if need be.
UnicodeString PosixIniFileName()
{
  const UnicodeString FileName = L"WinSCP.ini";
  // An AppImage's executable is in its read-only image: the one beside the
  // .AppImage file itself makes it portable ($APPIMAGE, its path).
  const UnicodeString AppImage = GetEnvironmentVariable(L"APPIMAGE");
  if (!AppImage.IsEmpty())
  {
    const UnicodeString Beside = IncludeTrailingBackslash(ExtractFilePath(AppImage)) + FileName;
    if (FileExists(ApiPath(Beside)))
    {
      return Beside;
    }
  }
  const UnicodeString Portable = IncludeTrailingBackslash(ExtractFilePath(ParamStr(0))) + FileName;
  if (FileExists(ApiPath(Portable)))
  {
    return Portable;
  }
  const UnicodeString Directory =
    IncludeTrailingBackslash(GetShellFolderPath(CSIDL_APPDATA)) + AppNameString();
  ForceDirectories(ApiPath(Directory));
  return IncludeTrailingBackslash(Directory) + FileName;
}
#endif
//---------------------------------------------------------------------------
// The /ini and /rawconfig switches, as UserInterface.cpp handles them; the
// raw configuration only without /unsafe, as its CheckSafe has it. Without
// /ini, Windows detects the storage as upstream does (an INI file if one
// exists, else the registry); POSIX uses its INI file. Each program's
// CreateConfiguration makes its own configuration and hands it here.
TConfiguration * SetUpConfigurationStorage(TConfiguration * Result)
{
  TProgramParams * Params = TProgramParams::Instance();
  UnicodeString IniFileName = Params->SwitchValue(IniSwitch);
  const bool Nul = !IniFileName.IsEmpty() && SameText(IniFileName, INI_NUL);
  // As WinInterface.cpp's CheckSafe handling of /ini: an explicit INI file is
  // honoured only for a safe (non-/Unsafe) invocation, so a session URL opened
  // through the handler cannot point the whole configuration at an
  // attacker-supplied file. The null store is always allowed; an /ini refused
  // here falls through to the default storage below.
  const bool ExplicitIni = !IniFileName.IsEmpty() && !Nul && CheckSafe(Params);
  if (Nul)
  {
    Result->SetNulStorage();
  }
  else if (ExplicitIni)
  {
    IniFileName = ExpandFileName(ExpandEnvironmentVariables(IniFileName));
    Result->SetExplicitIniFileStorageName(IniFileName);
  }
  else
  {
#if !defined(_WIN32)
    Result->SetExplicitIniFileStorageName(PosixIniFileName());
#else
    // The portable package's (linscp.ini beside linscp.exe, as WinSCP's
    // automatic one): the other programs beside it share it, as
    // WinSCP.com shares WinSCP.exe's.
    const UnicodeString Shared = IncludeTrailingBackslash(ExtractFilePath(ParamStr(0))) + L"linscp.ini";
    if (!SameText(Shared, ChangeFileExt(ParamStr(0), L".ini")) && FileExists(ApiPath(Shared)))
    {
      Result->SetExplicitIniFileStorageName(Shared);
    }
#endif
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
//---------------------------------------------------------------------------
namespace
{
const wchar_t * const UnsafeSwitch = L"Unsafe";
const wchar_t * const LogSwitch = L"Log";
const wchar_t * const LogSizeSwitch = L"LogSize";
const wchar_t LogSizeSeparator = L'*';
}
//---------------------------------------------------------------------------
bool CheckSafe(TProgramParams * Params)
{
  return !Params->FindSwitch(UnsafeSwitch);
}
//---------------------------------------------------------------------------
// WinMain.cpp's handling of the logging switches, and UserInterface.cpp's
// CheckLogParam/CheckXmlLogParam.
void ApplyLogSwitches(TProgramParams * Params, bool Console)
{
  UnicodeString Value;
  if (Params->FindSwitch(L"loglevel", Value))
  {
    const int Star = Value.Pos(L"*");
    if (Star > 0)
    {
      bool Sensitive = true;
      Value.Delete(Star, 1);
      if ((Star <= Value.Length()) && (Value[Star] == L'-'))
      {
        Sensitive = false;
        Value.Delete(Star, 1);
      }
      Value = Value.Trim();
      Configuration->TemporaryLogSensitive(Sensitive);
    }
    int Level = 0;
    if (!Value.IsEmpty() && TryStrToInt(Value, Level) && (Level >= -1))
    {
      Configuration->TemporaryLogProtocol(Level);
    }
  }
  if (Params->FindSwitch(LogSizeSwitch, Value))
  {
    const int Star = Value.Pos(LogSizeSeparator);
    int MaxCount = 0;
    if (Star > 1)
    {
      if (!TryStrToInt(Value.SubString(1, Star - 1), MaxCount))
      {
        MaxCount = -1;
      }
      Value.Delete(1, Star);
      Value = Value.Trim();
    }
    __int64 MaxSize = 0;
    if ((MaxCount >= 0) && !Value.IsEmpty() && TryStrToSize(Value, MaxSize))
    {
      Configuration->TemporaryLogMaxCount(MaxCount);
      Configuration->TemporaryLogMaxSize(MaxSize);
    }
  }
  if (Params->FindSwitch(LogSwitch, Value) && CheckSafe(Params))
  {
    Configuration->TemporaryLogging(Value);
  }
  if (Params->FindSwitch(L"XmlLog", Value) && CheckSafe(Params))
  {
    Configuration->TemporaryActionsLogging(Value);
    // (The console's: WinSCP.com's.)
    if (Console && Params->FindSwitch(L"XmlLogRequired"))
    {
      Configuration->LogActionsRequired = true;
    }
  }
}

