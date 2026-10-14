// The custom commands' bridge, VCL side: WinSCP's own TCustomCommandType
// and TCustomCommandList (compiled from their slices, ADR 0014) as the
// shell's dialogs see them.
#include <vcl.h>
#pragma hdrstop

#include "CustomCommandsSlice.h"
#include "CustomCommandType.h"
#include "LocalCustomCommand.h"
#include "BridgeVcl.h"

namespace CoreBridge
{

namespace
{

TCustomCommandDefinition DefinitionOf(const TCustomCommandType * Command)
{
  TCustomCommandDefinition Result;
  Result.Name = Utf8(Command->Name);
  Result.Command = Utf8(Command->Command);
  const int Params = Command->Params;
  Result.ApplyToDirectories = FLAGSET(Params, ccApplyToDirectories);
  Result.Recursive = FLAGSET(Params, ccRecursive);
  Result.Local = FLAGSET(Params, ccLocal);
  Result.ShowResults = FLAGSET(Params, ccShowResults);
  Result.CopyResults = FLAGSET(Params, ccCopyResults);
  Result.RemoteFiles = FLAGSET(Params, ccRemoteFiles);
  Result.ShowResultsInMessageBox = FLAGSET(Params, ccShowResultsInMsgBox);
  Result.OtherParams = Params & ~(ccApplyToDirectories | ccRecursive | ccLocal | ccShowResults | ccCopyResults |
    ccRemoteFiles | ccShowResultsInMsgBox);
  Result.ShortCut = Command->ShortCut;
  Result.Id = Utf8(Command->Id);
  return Result;
}

}  // namespace

int ParamsOf(const TCustomCommandDefinition & Command)
{
  return Command.OtherParams |
    FLAGMASK(Command.ApplyToDirectories, ccApplyToDirectories) |
    FLAGMASK(Command.Recursive, ccRecursive) |
    FLAGMASK(Command.Local, ccLocal) |
    FLAGMASK(Command.ShowResults, ccShowResults) |
    FLAGMASK(Command.CopyResults, ccCopyResults) |
    FLAGMASK(Command.RemoteFiles, ccRemoteFiles) |
    FLAGMASK(Command.ShowResultsInMessageBox, ccShowResultsInMsgBox);
}

std::vector<TCustomCommandDefinition> CustomCommands()
{
  std::vector<TCustomCommandDefinition> Result;
  const TCustomCommandList * List = WinConfiguration->CustomCommandList();
  for (int Index = 0; Index < List->Count; Index++)
  {
    Result.push_back(DefinitionOf(List->Commands[Index]));
  }
  return Result;
}

void SetCustomCommands(const std::vector<TCustomCommandDefinition> & Commands)
{
  try
  {
    std::unique_ptr<TCustomCommandList> List(new TCustomCommandList());
    // Each as it was, but what the dialog changed: an extension's keeps its
    // own fields.
    const TCustomCommandList * Current = WinConfiguration->CustomCommandList();
    for (const TCustomCommandDefinition & Definition : Commands)
    {
      const TCustomCommandType * Was = Current->Find(FromUtf8(Definition.Name));
      TCustomCommandType * Command = (Was != NULL) ? new TCustomCommandType(*Was) : new TCustomCommandType();
      Command->Name = FromUtf8(Definition.Name);
      Command->Command = FromUtf8(Definition.Command);
      Command->Params = ParamsOf(Definition);
      Command->ShortCut = static_cast<TShortCut>(Definition.ShortCut);
      List->Add(Command);
    }
    WinConfiguration->SetCustomCommandList(List.get());
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TCustomCommandDialog::UpdateControls and FormCloseQuery's validation.
TCustomCommandCheck CheckCustomCommand(const std::string & ACommand, bool Remote)
{
  TCustomCommandCheck Result;
  const UnicodeString Command = FromUtf8(ACommand);
  TRemoteCustomCommand RemoteCustomCommand;
  TLocalCustomCommand LocalCustomCommand;
  TFileCustomCommand * FileCustomCommand = (Remote ? &RemoteCustomCommand : &LocalCustomCommand);
  try
  {
    TInteractiveCustomCommand InteractiveCustomCommand(FileCustomCommand);
    UnicodeString Cmd = InteractiveCustomCommand.Complete(Command, false);
    Result.FileCommand = FileCustomCommand->IsFileCommand(Cmd);
    Result.AllowRemoteFiles = !Remote && FileCustomCommand->IsRemoteFileCommand(Cmd);
    Result.AllowRecursive = Result.FileCommand && !FileCustomCommand->IsFileListCommand(Cmd);
    if (Result.AllowRecursive && !Remote)
    {
      Result.AllowRecursive = !LocalCustomCommand.HasLocalFileName(Cmd);
    }
    Result.AllowApplyToDirectories = Result.FileCommand;
  }
  catch (...)
  {
  }
  try
  {
    TInteractiveCustomCommand InteractiveCustomCommand(FileCustomCommand);
    InteractiveCustomCommand.Validate(Command);
    UnicodeString Cmd = InteractiveCustomCommand.Complete(Command, false);
    FileCustomCommand->Validate(Cmd);
  }
  catch (Exception & E)
  {
    Result.Error = Utf8(E.Message);
  }
  return Result;
}

}  // namespace CoreBridge
