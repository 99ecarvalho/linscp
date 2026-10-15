// The custom commands' bridge, VCL side: WinSCP's own TCustomCommandType
// and TCustomCommandList (compiled from their slices, ADR 0014) as the
// shell's dialogs see them.
#include <vcl.h>
#pragma hdrstop

#include "CustomCommandsSlice.h"
#include "CustomCommandType.h"
#include "LocalCustomCommand.h"
#include "BridgeVcl.h"
#include "ExtensionsBridge.h"
#include <TextsWin.h>
#include <TextsCore.h>

#include <fstream>

const UnicodeString ExtensionFileExt(L".WinSCPextension");

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
  Result.FileName = Utf8(Command->FileName);
  Result.Description = Utf8(Command->Description);
  Result.HomePage = Utf8(Command->HomePage);
  Result.OptionsPage = Utf8(Command->OptionsPage);
  Result.ConfigOptions = Command->AnyOptionWithFlag(TCustomCommandType::ofConfig);
  Result.RunOptions = Command->AnyOptionWithFlag(TCustomCommandType::ofRun);
  Result.SiteOptions = Command->AnyOptionWithFlag(TCustomCommandType::ofSite);
  return Result;
}

}  // namespace

const TCustomCommandType * FindListedCommand(const TCustomCommandDefinition & Definition)
{
  if (!Definition.Id.empty())
  {
    const TCustomCommandList * Extensions = WinConfiguration->ExtensionList();
    for (int Index = 0; Index < Extensions->Count; Index++)
    {
      if (Extensions->Commands[Index]->Id == FromUtf8(Definition.Id))
      {
        return Extensions->Commands[Index];
      }
    }
    return NULL;
  }
  const TCustomCommandType * Listed = WinConfiguration->CustomCommandList()->Find(FromUtf8(Definition.Name));
  return ((Listed != NULL) && (Listed->Command == FromUtf8(Definition.Command))) ? Listed : NULL;
}

// As TCustomCommandOptionsDialog's constructor: the options with these
// flags, each with its value.
std::vector<TExtensionOption> ExtensionOptionsOf(const TCustomCommandType * Command, TStrings * Values, unsigned Flags,
  const UnicodeString & Site, TCustomCommand * ForOptions)
{
  std::vector<TExtensionOption> Result;
  for (int Index = 0; Index < Command->OptionsCount; Index++)
  {
    const TCustomCommandType::TOption & Option = Command->GetOption(Index);
    if ((Option.Flags & Flags) == 0)
    {
      continue;
    }
    TExtensionOption Item;
    Item.Kind = static_cast<TExtensionOption::TKind>(Option.Kind);
    Item.Caption = Utf8(Option.Caption);
    Item.Default = Utf8(Option.Default);
    Item.FileCaption = Utf8(Option.FileCaption);
    Item.FileFilter = Utf8(Option.FileFilter);
    Item.FileInitial = Utf8(Option.FileInitial);
    Item.FileExt = Utf8(Option.FileExt);
    Item.Control = Option.IsControl;
    for (const UnicodeString & Param : Option.Params)
    {
      // A list's "value=caption" (the caption translated); a check box's values.
      UnicodeString Caption = Param;
      UnicodeString Value = Param;
      const int P = Caption.Pos(L"=");
      if ((P > 0) && (Option.Kind != TCustomCommandType::okCheckBox))
      {
        Value = Caption.SubString(1, P - 1);
        Caption.Delete(1, P);
      }
      Caption = WinConfiguration->ExtensionStringTranslation(Command->Id, Caption);
      Item.Items.emplace_back(Utf8(Value), Utf8(Caption));
    }
    const UnicodeString Key = Command->GetOptionKey(Option, Site);
    // As CreateHistoryComboBox, with GetValidHistoryKey's letters and digits.
    UnicodeString HistoryKey = Key;
    for (int Char = 1; Char <= HistoryKey.Length(); Char++)
    {
      if (!IsLetter(HistoryKey[Char]) && !IsDigit(HistoryKey[Char]))
      {
        HistoryKey[Char] = L'_';
      }
    }
    while (!HistoryKey.IsEmpty() && (HistoryKey[1] == L'_'))
    {
      HistoryKey.Delete(1, 1);
    }
    Item.HistoryKey = Utf8(L"CustomCommandOption_" + HistoryKey);
    if ((ForOptions != NULL) && Option.HasPatterns(ForOptions))
    {
      Item.Value = Utf8(ForOptions->Complete(Option.Default, true));
    }
    else if (Values->IndexOfName(Key) >= 0)
    {
      Item.Value = Utf8(Values->Values[Key]);
    }
    else
    {
      Item.Value = Item.Default;
    }
    Result.push_back(Item);
  }
  return Result;
}

// As TCustomCommandOptionsDialog::Execute: the controls' values kept, an
// empty one too (not to fall back to the default).
void SetExtensionOptionsOf(const TCustomCommandType * Command, TStrings * Values, unsigned Flags, const UnicodeString & Site,
  const std::vector<std::string> & New)
{
  std::size_t At = 0;
  for (int Index = 0; Index < Command->OptionsCount; Index++)
  {
    const TCustomCommandType::TOption & Option = Command->GetOption(Index);
    if ((Option.Flags & Flags) == 0)
    {
      continue;
    }
    if ((Option.Kind != TCustomCommandType::okUnknown) && Option.IsControl && (At < New.size()))
    {
      SetStringValueEvenIfEmpty(Values, Command->GetOptionKey(Option, Site), FromUtf8(New[At]));
    }
    At++;
  }
}

std::vector<TCustomCommandDefinition> Extensions()
{
  std::vector<TCustomCommandDefinition> Result;
  const TCustomCommandList * List = WinConfiguration->ExtensionList();
  for (int Index = 0; Index < List->Count; Index++)
  {
    Result.push_back(DefinitionOf(List->Commands[Index]));
  }
  return Result;
}

struct TExtensionsEdit::TImpl
{
  std::unique_ptr<TCustomCommandList> List{new TCustomCommandList()};
  std::unique_ptr<TStringList> Options{new TStringList()};
  // The files added in this edit, removed unless committed.
  std::unique_ptr<TStringList> Added{CreateSortedStringList()};
  bool Committed = false;

  int Add(TStrings * Lines, const UnicodeString & Source, UnicodeString FileName, const UnicodeString & LinesSourcePath,
    UnicodeString ExtensionPath, const UnicodeString & ProvisionaryId, bool Trusted, int Before,
    const std::function<bool()> & ConfirmUntrusted);
};

TExtensionsEdit::TExtensionsEdit() : F(new TImpl())
{
  F->List->Assign(WinConfiguration->ExtensionList());
  F->Options->Assign(WinConfiguration->CustomCommandOptions());
}

TExtensionsEdit::~TExtensionsEdit()
{
  if (!F->Committed)
  {
    for (int Index = 0; Index < F->Added->Count; Index++)
    {
      DeleteFile(ApiPath(F->Added->Strings[Index]));
    }
  }
}

std::vector<TCustomCommandDefinition> TExtensionsEdit::Items() const
{
  std::vector<TCustomCommandDefinition> Result;
  for (int Index = 0; Index < F->List->Count; Index++)
  {
    Result.push_back(DefinitionOf(F->List->Commands[Index]));
  }
  return Result;
}

// As TPreferencesDialog::AddExtension, from validating the lines on.
int TExtensionsEdit::TImpl::Add(TStrings * Lines, const UnicodeString & Source, UnicodeString FileName,
  const UnicodeString & LinesSourcePath, UnicodeString ExtensionPath, const UnicodeString & ProvisionaryId, bool Trusted,
  int Before, const std::function<bool()> & ConfirmUntrusted)
{
  std::unique_ptr<TCustomCommandType> CustomCommand(new TCustomCommandType());
  try
  {
    // The provisionary id, for ExtensionStringTranslation, so that the
    // duplicate test below works.
    CustomCommand->Id = ProvisionaryId;
    CustomCommand->LoadExtension(Lines, FileName);
  }
  catch (Exception & E)
  {
    throw ExtException(&E, MainInstructions(FMTLOAD(EXTENSION_LOAD_ERROR, (Source))));
  }
  if (!ExtensionPath.IsEmpty() && (List->FindIndexByFileName(ExtensionPath) >= 0))
  {
    throw Exception(MainInstructions(LoadStr(EXTENSION_INSTALLED_ALREADY)));
  }
  if (List->Find(CustomCommand->Name) != NULL)
  {
    throw Exception(MainInstructions(FMTLOAD(EXTENSION_DUPLICATE, (StripHotkey(CustomCommand->Name)))));
  }
  if (ExtensionPath.IsEmpty() && TCustomCommandType::GetExtensionId(FileName).IsEmpty())
  {
    const UnicodeString FileNameOnly = ChangeFileExt(ExtractFileName(FileName), UnicodeString());
    FileName = FileNameOnly.IsEmpty() ?
      (MakeValidFileName(StripHotkey(CustomCommand->Name)) + ExtensionFileExt) :
      (FileNameOnly + ExtensionFileExt + ExtractFileExt(FileName));
  }
  if (!Trusted && !ConfirmUntrusted())
  {
    return -1;
  }
  if (ExtensionPath.IsEmpty())
  {
    const UnicodeString UserExtensionsPath = WinConfiguration->GetUserExtensionsPath();
    if (!DirectoryExists(ApiPath(UserExtensionsPath)) && !ForceDirectories(ApiPath(UserExtensionsPath)))
    {
      throw EOSExtException(MainInstructions(FMTLOAD(CREATE_LOCAL_DIR_ERROR, (UserExtensionsPath))));
    }
    ExtensionPath = IncludeTrailingBackslash(UserExtensionsPath) + FileName;
    int Counter = 1;
    const UnicodeString OriginalExtensionPath = ExtensionPath;
    const int P = Pos(UpperCase(ExtensionFileExt), UpperCase(OriginalExtensionPath));
    while (FileExists(ApiPath(ExtensionPath)))
    {
      Counter++;
      ExtensionPath = WinConfiguration->UniqueExtensionName(LeftStr(OriginalExtensionPath, P - 1), Counter) +
        RightStr(OriginalExtensionPath, OriginalExtensionPath.Length() - P + 1);
    }
    if (!LinesSourcePath.IsEmpty())
    {
      // Copied as it is, its encoding kept.
      std::ifstream In(UTF8String(ApiPath(LinesSourcePath)).c_str(), std::ios::binary);
      std::ofstream Out(UTF8String(ApiPath(ExtensionPath)).c_str(), std::ios::binary);
      Out << In.rdbuf();
      if (!In || !Out)
      {
        throw Exception(MainInstructions(FMTLOAD(EXTENSION_LOAD_ERROR, (Source))));
      }
    }
    else
    {
      // (Without a BOM, as the shim writes it.)
      Lines->SaveToFile(ApiPath(ExtensionPath));
    }
    Added->Add(ExtensionPath);
  }
  std::unique_ptr<TCustomCommandType> Loaded(new TCustomCommandType());
  Loaded->Id = WinConfiguration->GetExtensionId(ExtensionPath);
  Loaded->LoadExtension(ExtensionPath);
  if ((Before >= 0) && (Before < List->Count))
  {
    List->Insert(Before, Loaded.release());
    return Before;
  }
  List->Add(Loaded.release());
  return List->Count - 1;
}

int TExtensionsEdit::AddFromFile(const std::string & APath, int Before, const std::function<bool()> & ConfirmUntrusted)
{
  try
  {
    const UnicodeString Path = FromUtf8(APath);
    if (!FileExists(ApiPath(Path)))
    {
      throw Exception(MainInstructions(FMTLOAD(EXTENSION_LOAD_ERROR, (Path))) + L"\n\n" + FMTLOAD(FILE_NOT_EXISTS, (Path)));
    }
    std::unique_ptr<TStringList> Lines(new TStringList());
    UnicodeString ProvisionaryId;
    UnicodeString ExtensionPath;
    const UnicodeString Id = WinConfiguration->GetExtensionId(Path);
    const UnicodeString FileName = ExtractFileName(Path);
    if (!Id.IsEmpty())
    {
      ExtensionPath = Path;
      ProvisionaryId = Id;
    }
    else
    {
      ProvisionaryId = WinConfiguration->GetProvisionaryExtensionId(FileName);
    }
    try
    {
      LoadScriptFromFile(Path, Lines.get());
    }
    catch (Exception & E)
    {
      throw ExtException(&E, MainInstructions(FMTLOAD(EXTENSION_LOAD_ERROR, (Path))));
    }
    return F->Add(Lines.get(), Path, FileName, Path, ExtensionPath, ProvisionaryId, true, Before, ConfirmUntrusted);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

int TExtensionsEdit::AddFromUrl(const std::string & AUrl, const std::string & Contents, const std::string & IdHeader,
  bool Trusted, int Before, const std::function<bool()> & ConfirmUntrusted)
{
  try
  {
    const UnicodeString Url = FromUtf8(AUrl);
    UnicodeString FileName = MakeValidFileName(FromUtf8(IdHeader));
    if (FileName.IsEmpty())
    {
      FileName = MakeValidFileName(ExtractFileNameFromUrl(Url));
    }
    std::unique_ptr<TStringList> Lines(new TStringList());
    Lines->Text = FromUtf8(Contents);
    return F->Add(Lines.get(), Url, FileName, UnicodeString(), UnicodeString(),
      WinConfiguration->GetProvisionaryExtensionId(FileName), Trusted, Before, ConfirmUntrusted);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As RemoveCommandButtonClick: an extension added in this edit, its file too.
void TExtensionsEdit::Remove(int Index)
{
  if ((Index < 0) || (Index >= F->List->Count))
  {
    return;
  }
  const UnicodeString FileName = F->List->Commands[Index]->FileName;
  const int Added = F->Added->IndexOf(FileName);
  if (Added >= 0)
  {
    F->Added->Delete(Added);
    DeleteFile(ApiPath(FileName));
  }
  F->List->Delete(Index);
}

void TExtensionsEdit::Move(int Index, int To)
{
  if ((Index >= 0) && (Index < F->List->Count) && (To >= 0) && (To < F->List->Count))
  {
    F->List->Move(Index, To);
  }
}

void TExtensionsEdit::SetShortCut(int Index, unsigned ShortCut)
{
  if ((Index >= 0) && (Index < F->List->Count))
  {
    TCustomCommandType * Updated = new TCustomCommandType(*F->List->Commands[Index]);
    Updated->ShortCut = static_cast<TShortCut>(ShortCut);
    F->List->Change(Index, Updated);
  }
}

std::vector<TExtensionOption> TExtensionsEdit::Options(int Index, unsigned Flags, const std::string & Site) const
{
  if ((Index < 0) || (Index >= F->List->Count))
  {
    return std::vector<TExtensionOption>();
  }
  return ExtensionOptionsOf(F->List->Commands[Index], F->Options.get(), Flags, FromUtf8(Site), NULL);
}

void TExtensionsEdit::SetOptions(int Index, unsigned Flags, const std::string & Site, const std::vector<std::string> & Values)
{
  if ((Index >= 0) && (Index < F->List->Count))
  {
    SetExtensionOptionsOf(F->List->Commands[Index], F->Options.get(), Flags, FromUtf8(Site), Values);
  }
}

void TExtensionsEdit::Commit()
{
  try
  {
    WinConfiguration->SetExtensionList(F->List.get());
    WinConfiguration->SetCustomCommandOptions(F->Options.get());
    F->Committed = true;
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As AddExtension's winscp.net request: WinSCP's version and what is
// installed, for the site to give the right extension.
std::string ExtensionDownloadUrl(const std::string & AUrl, bool & WinScpUrl)
{
  UnicodeString Url = FromUtf8(AUrl);
  const UnicodeString HomePage = LoadStr(HOMEPAGE_URL);
  const UnicodeString HttpHomePage = ChangeUrlProtocol(HomePage, HttpProtocol);
  WinScpUrl = StartsText(HomePage, Url) || StartsText(HttpHomePage, Url);
  if (WinScpUrl)
  {
    if (IsHttpUrl(Url))
    {
      Url = ChangeUrlProtocol(Url, HttpsProtocol);
    }
    Url = AppendUrlParams(Url, FORMAT(L"v=%s", (EncodeUrlString(Configuration->Version))));
    Url = AppendUrlParams(Url, FORMAT(L"netframework=%s", (EncodeUrlString(GetNetVersionStr()))));
    Url = AppendUrlParams(Url, FORMAT(L"netcore=%s", (EncodeUrlString(GetNetCoreVersionStr()))));
    Url = AppendUrlParams(Url, FORMAT(L"powershell=%s", (EncodeUrlString(GetPowerShellVersionStr()))));
    Url = AppendUrlParams(Url, FORMAT(L"pwsh=%s", (EncodeUrlString(GetPowerShellCoreVersionStr()))));
    Url = AppendUrlParams(Url, FORMAT(L"windows=%s", (EncodeUrlString(WindowsVersion()))));
  }
  return Utf8(Url);
}

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
    Result.LocalFileCommand = !Remote && LocalCustomCommand.HasLocalFileName(Cmd);
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
