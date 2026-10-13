// The program-specific half of core's Interface.h, for a process with no GUI:
// what a command-line client or a test harness links (FrontendCommon.cpp has
// the shared half). Where upstream's definition needs a GUI, the comment
// says what stands in for it.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>

#include <Common.h>
#include <Exceptions.h>
#include <Interface.h>
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
