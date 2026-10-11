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
