// The program-specific half of core's Interface.h for the Qt shell
// (port/frontend/FrontendCommon.cpp has the shared half): errors as dialogs,
// the busy cursor, the event loop and the clipboard, all through the
// frontend the shell gave CoreBridge::Initialize.
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"
#include "FrontendCommon.h"
#include "ShellConfiguration.h"

#include <Common.h>
#include <Exceptions.h>

#include <cstdio>

using namespace CoreBridge;

// As upstream's (WinInterface.cpp): the message dialog for an error no
// terminal reports itself. Without a frontend (before startup, after
// shutdown), stderr, as the headless set has it.
void __fastcall ShowExtendedException(Exception * E)
{
  TError Error;
  if (!ErrorOf(E, Error))
  {
    return;
  }
  if ((Frontend() != NULL) && IsGuiThread())
  {
    Frontend()->ShowError(Error);
  }
  else
  {
    std::fputs((Error.Message + "\n" + Error.MoreMessages).c_str(), stderr);
  }
}

void * __fastcall BusyStart()
{
  if ((Frontend() != NULL) && IsGuiThread())
  {
    Frontend()->BusyStart();
    return reinterpret_cast<void *>(1);
  }
  return NULL;
}

void __fastcall BusyEnd(void * Token)
{
  if ((Token != NULL) && (Frontend() != NULL))
  {
    Frontend()->BusyEnd();
  }
}

// As upstream's (WinInterface.cpp): on the GUI thread only, at most every
// GUIUpdateInterval unless forced, and says whether it did.
bool __fastcall ProcessGUI(bool Force)
{
  static DWORD LastUpdate = 0;
  bool Result = false;
  if ((Frontend() != NULL) && IsGuiThread())
  {
    const DWORD Now = GetTickCount();
    if (Force || (static_cast<DWORD>(Now - LastUpdate) >= GUIUpdateInterval))
    {
      Frontend()->ProcessEvents();
      LastUpdate = GetTickCount();
      Result = true;
    }
  }
  return Result;
}

void __fastcall CopyToClipboard(UnicodeString Text)
{
  if ((Frontend() == NULL) || !IsGuiThread())
  {
    throw Exception(L"No clipboard is available.");
  }
  Frontend()->SetClipboard(Utf8(Text));
}

// As upstream's (GUITools.cpp's TextFromClipboard): optionally trimmed, and
// false for an empty clipboard.
bool __fastcall TextFromClipboard(UnicodeString & Text, bool Trim)
{
  std::string Value;
  if ((Frontend() == NULL) || !IsGuiThread() || !Frontend()->GetClipboard(Value))
  {
    return false;
  }
  Text = FromUtf8(Value);
  if (Trim)
  {
    Text = Text.Trim();
  }
  return !Text.IsEmpty();
}
//---------------------------------------------------------------------------
// As upstream's (windows/UserInterface.cpp): the GUI's configuration, which
// GUIConfiguration names.
TConfiguration * __fastcall CreateConfiguration()
{
  TShellConfiguration * Result = new TShellConfiguration();
  GUIConfiguration = Result;
  return SetUpConfigurationStorage(Result);
}
