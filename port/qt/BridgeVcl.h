// What the VCL side of the bridge (CoreBridge.cpp, QtInterface.cpp,
// Session.cpp) shares. Includes the shim: never from a Qt unit.
#ifndef LINSCP_QT_BRIDGE_VCL_H
#define LINSCP_QT_BRIDGE_VCL_H

#include <vcl.h>

// Common.h first, as core's own units have it (CorePCH.h): later headers
// rely on it (RemoteFiles.h's TComPtr on Windows).
#include <Common.h>
#include <Interface.h>

#include "CoreBridge.h"

class TSessionData;

namespace CoreBridge
{

std::string Utf8(const UnicodeString & Str);
UnicodeString FromUtf8(const std::string & Str);

// The frontend Initialize was given; NULL before and after.
IFrontend * Frontend();
// Whether this is the thread core was started on, the GUI's.
bool IsGuiThread();

// The error as upstream's message dialog shows it. False for what is not to
// be shown at all (an abort).
bool ErrorOf(Exception * E, TError & Error);
// ErrorOf as a TCoreError to throw, an abort included (its message as is).
TCoreError CoreErrorOf(Exception & E);

// A query as upstream's MoreMessageDialog asks it: the answers' buttons in
// upstream's order, with their aliases and OnSubmit handlers, and the
// never-ask-again rule of WinInterface.cpp's MessageDialog.
unsigned int AskFrontend(const UnicodeString & Message, TStrings * MoreMessages, ::TQueryType Type,
  unsigned int Answers, const TQueryParams * Params);

// The session data of Site, as Sites.cpp builds it for saving: the settings of
// the stored site BaseName (empty: core's defaults) with Site's applied. The
// caller owns it.
::TSessionData * SessionDataOf(const TSite & Site, const std::string & BaseName);

}

#endif
