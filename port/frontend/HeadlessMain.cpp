#include <vcl.h>
#pragma hdrstop

#include <Common.h>
#include <CoreMain.h>
#include <SessionInfo.h>
#include "ProgParams.h"
#include "HeadlessMain.h"
//---------------------------------------------------------------------------
// wWinMain's order: the log exists before core, because core logs to it from
// CoreInitialize on, and startup information is added once the
// configuration it reports on exists. Upstream also routes its Delphi
// components' logging here (OnAppLog, packages/my/PasTools.pas); there are
// no such components.
void HeadlessInitialize()
{
  TProgramParams * Params = TProgramParams::Instance();
  ApplicationLog = new TApplicationLog();
  UnicodeString AppLogPath;
  if (Params->FindSwitch(L"applog", AppLogPath))
  {
    ApplicationLog->Enable(AppLogPath);
  }
  AppLog(L"Starting...");
  AppLogFmt(L"Process: %d", (GetCurrentProcessId()));
  CoreInitialize();
  ApplicationLog->AddStartupInfo(); // Needs Configuration
}
//---------------------------------------------------------------------------
void HeadlessFinalize()
{
  AppLog(L"Finalizing");
  CoreFinalize();
  AppLog(L"Finalizing done");
  SAFE_DESTROY_EX(TApplicationLog, ApplicationLog);
}
//---------------------------------------------------------------------------
// windows/ProgParams.cpp's TProgramParams, verbatim but for its first line:
// upstream's file now opens with the GUI's precompiled header (WinPCH.h,
// which is FormsPCH.h), so it cannot be compiled without the VCL forms.
static std::unique_ptr<TProgramParams> ProgramParamsOwner;
//---------------------------------------------------------------------------
TProgramParams * __fastcall TProgramParams::Instance()
{
  if (ProgramParamsOwner.get() == NULL)
  {
    ProgramParamsOwner.reset(new TProgramParams());
  }
  return ProgramParamsOwner.get();
}
//---------------------------------------------------------------------------
void SetProgramCommandLine(const wchar_t * CommandLine)
{
  ProgramParamsOwner.reset(new TProgramParams(UnicodeString(CommandLine)));
}
//---------------------------------------------------------------------------
__fastcall TProgramParams::TProgramParams()
{
  Init(CmdLine);
}
//---------------------------------------------------------------------------
__fastcall TProgramParams::TProgramParams(const UnicodeString & CmdLine)
{
  Init(CmdLine);
}
//---------------------------------------------------------------------------
void __fastcall TProgramParams::Init(const UnicodeString & CmdLine)
{
  UnicodeString CommandLine = CmdLine;

  UnicodeString Param;
  CutToken(CommandLine, Param);
  Parse(CommandLine);
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TProgramParams::FormatSwitch(const UnicodeString & Switch)
{
  return FORMAT(L"/%s", (Switch));
}
