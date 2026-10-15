// The Setup slice's stand-ins (SetupSlice.h). Windows only.
#include <vcl.h>
#pragma hdrstop

#include "SetupSlice.h"
#include "BridgeVcl.h"

namespace
{

TSetupApplication GApplication{ ParamStr(0) };

}  // namespace

TSetupApplication * Application = &GApplication;

void ShellOpen(const UnicodeString & Param)
{
  CoreBridge::IFrontend * Frontend = CoreBridge::Frontend();
  if (Frontend != NULL)
  {
    Frontend->StartProgram(CoreBridge::Utf8(Param), std::string(), std::string());
  }
}

void __fastcall SimpleErrorDialog(const UnicodeString Msg, const UnicodeString MoreMessages)
{
  CoreBridge::TError Error;
  Error.Message = CoreBridge::Utf8(Msg);
  Error.MoreMessages = CoreBridge::Utf8(MoreMessages);
  if (CoreBridge::Frontend() != NULL)
  {
    CoreBridge::Frontend()->ShowError(Error);
  }
}

namespace CoreBridge
{

void RegisterUrlHandler()
{
  try
  {
    ::RegisterForDefaultProtocols();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void UnregisterUrlHandler()
{
  try
  {
    ::UnregisterForProtocols();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void MakeDefaultUrlHandler()
{
  try
  {
    ::LaunchAdvancedAssociationUI();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void AddToSearchPath(const std::string & Directory)
{
  try
  {
    ::AddSearchPath(FromUtf8(Directory));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void RemoveFromSearchPath(const std::string & Directory)
{
  try
  {
    ::RemoveSearchPath(FromUtf8(Directory));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}  // namespace CoreBridge
