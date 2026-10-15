// What the custom commands' slices of upstream's GUI (port/tools/slices.json:
// windows/WinConfiguration.cpp's TCustomCommandType and TCustomCommandList,
// GUITools.cpp's TLocalCustomCommand) expect around them, upstream's
// headers' share: core, and WinConfiguration -- the shell's configuration,
// which plays TWinConfiguration's part (ADR 0014).
#ifndef LINSCP_QT_CUSTOM_COMMANDS_SLICE_H
#define LINSCP_QT_CUSTOM_COMMANDS_SLICE_H

#include <vcl.h>
#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <FileInfo.h>
#include <FileMasks.h>
#include <HierarchicalStorage.h>
#include <Terminal.h>
#include <TextsWin.h>

#include <vector>

#include "ShellConfiguration.h"

extern TShellConfiguration * WinConfiguration;

// windows/UserInterface.cpp's (sliced: CustomShortCuts.cpp).
TShortCut __fastcall NormalizeCustomShortCut(TShortCut ShortCut);
bool __fastcall IsCustomShortCut(TShortCut ShortCut);
// windows/Setup.cpp's, which extensions' "Require" lines check: the .NET
// and PowerShell versions installed, as linscp finds them
// (CustomCommandsSlice.cpp).
UnicodeString GetNetVersionStr();
UnicodeString GetNetCoreVersionStr();
UnicodeString GetPowerShellVersionStr();
UnicodeString GetPowerShellCoreVersionStr();

#endif
