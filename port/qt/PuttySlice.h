// What GUITools.cpp's PuTTY part (port/tools/slices.json: FindFile, the
// PuTTY session export and cleanup, OpenSessionInPutty) has from upstream's
// precompiled header and GUITools.h, and linscp's stand-ins for the GUI it
// calls (ADR 0014).
#ifndef LINSCP_QT_PUTTY_SLICE_H
#define LINSCP_QT_PUTTY_SLICE_H

#include <vcl.h>
#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <HierarchicalStorage.h>
#include <PuttyTools.h>
#include <Queue.h>
#include <SessionData.h>
#include <TextsCore.h>
#include <TextsWin.h>
#include <GUIConfiguration.h>

#include <memory>

#include "BridgeCustomCommand.h"
#include "CustomCommandsSlice.h"
#include "LocalCustomCommand.h"

// PasTools.pas's: whether the file exists, its last error cleared first.
inline bool FileExistsFix(const UnicodeString & Path)
{
  SetLastError(ERROR_SUCCESS);
  return FileExists(ApiPath(Path));
}

// GUITools.cpp's ExecuteShellChecked, over the frontend's StartProgram:
// EOSExtException (EXECUTE_APP_ERROR) when the program does not start.
// ChangeWorkingDirectory: in the program's own directory.
void __fastcall ExecuteShellChecked(const UnicodeString Path, const UnicodeString Params,
  bool ChangeWorkingDirectory = false);

// The prompts the PuTTY command's patterns are asked through: those given
// TSession::OpenInPutty while it runs.
const CoreBridge::TCustomCommandPrompts & PuttyCommandPrompts();
void SetPuttyCommandPrompts(const CoreBridge::TCustomCommandPrompts & Prompts);

// WinInterface.cpp's, the frontend's: the bridge's.
class TWinInteractiveCustomCommand : public CoreBridge::TBridgeInteractiveCustomCommand
{
public:
  TWinInteractiveCustomCommand(TCustomCommand * ChildCustomCommand, const UnicodeString CustomCommandName,
      const UnicodeString /*HelpKeyword*/) :
    TBridgeInteractiveCustomCommand(ChildCustomCommand, CustomCommandName, PuttyCommandPrompts())
  {
  }
};

// The slice's own prologue (slices.json) has IsUWP() true on POSIX: PuTTY
// there keeps its sessions in files (~/.putty/sessions), which the registry
// export cannot reach, so, as upstream does for the Store's PuTTY, the
// session goes on PuTTY's command line instead.

// GUITools.h's, which the slice defines.
bool __fastcall FindFile(UnicodeString & Path);
bool DoesSessionExistInPutty(const UnicodeString & StorageKey);
bool __fastcall ExportSessionToPutty(TSessionData * SessionData, bool ReuseExisting, const UnicodeString & SessionName);
void SplitPuttyCommand(UnicodeString & Program, UnicodeString & Params);
UnicodeString FindPuttyPath();
void OpenSessionInPutty(TSessionData * SessionData);
// The slice's epilogue: upstream's GUIFinalize's TPuttyCleanupThread::Finalize,
// waiting for the temporary PuTTY sessions' cleanup to end.
void PuttySessionsFinalize();

#endif
