// The PuTTY slice's stand-ins (PuttySlice.h).
#include <vcl.h>
#pragma hdrstop

#include "PuttySlice.h"

namespace
{

CoreBridge::TCustomCommandPrompts GPuttyPrompts;

}  // namespace

const CoreBridge::TCustomCommandPrompts & PuttyCommandPrompts()
{
  return GPuttyPrompts;
}

void SetPuttyCommandPrompts(const CoreBridge::TCustomCommandPrompts & Prompts)
{
  GPuttyPrompts = Prompts;
}

void __fastcall ExecuteShellChecked(const UnicodeString Path, const UnicodeString Params, bool ChangeWorkingDirectory)
{
  CoreBridge::IFrontend * Frontend = CoreBridge::Frontend();
  const UnicodeString Directory = ChangeWorkingDirectory ? ExtractFilePath(Path) : UnicodeString();
  if ((Frontend == NULL) ||
      !Frontend->StartProgram(CoreBridge::Utf8(Path), CoreBridge::Utf8(Params), CoreBridge::Utf8(Directory)))
  {
    throw EOSExtException(FMTLOAD(EXECUTE_APP_ERROR, (Path)));
  }
}
