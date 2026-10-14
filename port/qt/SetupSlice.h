// What windows/Setup.cpp's integration part (port/tools/slices.json: the
// search path, and the protocols' registration as their URL handler) has
// from upstream's precompiled header, and linscp's stand-ins for the GUI it
// calls (ADR 0014). Windows only: on POSIX the shell registers through XDG
// instead (port/qt/Integration.cpp).
#ifndef LINSCP_QT_SETUP_SLICE_H
#define LINSCP_QT_SETUP_SLICE_H

#include <vcl.h>
#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <Registry.hpp>
#include <SessionData.h>
#include <TextsCore.h>
#include <TextsWin.h>
#include <GUIConfiguration.h>

#include <shlobj.h>

#include "ProgParams.h"

// WinInterface.h's.
#define UNSAFE_SWITCH L"Unsafe"

// The VCL's Application, as far as the slice asks: the executable.
struct TSetupApplication
{
  UnicodeString ExeName;
};
extern TSetupApplication * Application;

// Tools.cpp's ShellOpen, for the Settings app's default apps page: through
// the frontend.
void ShellOpen(const UnicodeString & Param);
// WinInterface.cpp's, through the frontend.
void __fastcall SimpleErrorDialog(const UnicodeString Msg, const UnicodeString MoreMessages = UnicodeString());

// The slice's, for the bridge.
void __fastcall AddSearchPath(const UnicodeString Path);
void __fastcall RemoveSearchPath(const UnicodeString Path);
void __fastcall RegisterForDefaultProtocols();
void __fastcall UnregisterForProtocols();
void __fastcall LaunchAdvancedAssociationUI();

#endif
