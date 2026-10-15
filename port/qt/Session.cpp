// CoreBridge::TSession: a TTerminal, and its events turned into the
// frontend's dialogs, as upstream's TTerminalManager wires them for the GUI
// (windows/TerminalManager.cpp).
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <Common.h>
#include <CoreMain.h>
#include <Exceptions.h>
#include <RemoteFiles.h>
#include <SessionData.h>
#include <Terminal.h>
#include <FileSystems.h>
#include <FileMasks.h>
#include <TextsCore.h>
#include <CopyParam.h>
#include <FileOperationProgress.h>
#include <Queue.h>
#include <GUIConfiguration.h>
#include <SynchronizeController.h>
#include <HelpWin.h>

#include "ShellConfiguration.h"
#include "TransferSettings.h"
#include "GenerateCode.h"
#include "CustomCommandsSlice.h"
#include "CustomCommandType.h"
#include "DiscMon.hpp"
#include "BridgeCustomCommand.h"
#include "PuttySlice.h"
#include "ProgParams.h"

#include <TextsWin.h>

#include <atomic>
#include <mutex>
#include <set>
#include <memory>

namespace CoreBridge
{

class TKeepUpToDateRun;

struct TSession::TImpl
{
  // Out of line: TKeepUpToDateRun is complete only further on. Each one
  // listed while it lives, for RecryptPasswords.
  TImpl();
  ~TImpl();
  static std::set<TImpl *> & Live();

  std::unique_ptr<TSessionData> Data;
  std::unique_ptr<TTerminal> Terminal;
  // The background queue, made with its first item; its status as last
  // taken, and whether its threads said it changed since.
  std::unique_ptr<TTerminalQueue> Queue;
  TTerminalQueueStatus * QueueStatus = NULL;
  std::atomic<bool> QueueChanged{false};
  // Items added; those no longer listed have finished (done, or removed:
  // core drops done items at once unless told to keep them).
  int QueueAdded = 0;
  int QueueDone = 0;
  // The last search's files (Find files), by full path, as found.
  std::vector<std::pair<UnicodeString, std::unique_ptr<TRemoteFile>>> Found;
  // Keeping the remote directory up to date, and where its errors go with
  // "continue on error" (TSynchronizeDialog::FeedSynchronizeError).
  std::unique_ptr<TKeepUpToDateRun> KeepUpToDate;
  std::function<void(const UnicodeString & Message, TStrings * MoreMessages)> FeedError;

  // The queue's events come on its threads: they only flag the change.
  void __fastcall QueueListUpdate(TTerminalQueue * /*Queue*/)
  {
    QueueChanged = true;
  }

  void __fastcall QueueItemUpdate(TTerminalQueue * /*Queue*/, TQueueItem * /*Item*/)
  {
    QueueChanged = true;
  }

  void __fastcall QueueEvent(TTerminalQueue * /*Queue*/, TQueueEvent /*Event*/)
  {
    QueueChanged = true;
  }

  // Made with its first item, as the GUI's: core's queue over this session,
  // asking its questions through the same handlers as the session's own.
  // Process Queue: as the session's queue was made (EnableQueueByDefault).
  bool QueueEnabled = true;

  TTerminalQueue * EnsureQueue()
  {
    if (Queue == NULL)
    {
      Queue.reset(new TTerminalQueue(Terminal.get(), Configuration));
      // As TTerminalManager::NewQueue.
      Queue->TransfersLimit = Configuration->QueueTransfersLimit;
      Queue->Enabled = QueueEnabled;
      Queue->KeepDoneItemsFor =
        (GUIConfiguration->QueueKeepDoneItems ? GUIConfiguration->QueueKeepDoneItemsFor : 0);
      Queue->OnQueryUser = CLOSURE(QueryUser);
      Queue->OnPromptUser = CLOSURE(PromptUser);
      Queue->OnShowExtendedException = CLOSURE(ShowExtendedException);
      Queue->OnListUpdate = CLOSURE(QueueListUpdate);
      Queue->OnQueueItemUpdate = CLOSURE(QueueItemUpdate);
      Queue->OnEvent = CLOSURE(QueueEvent);
    }
    return Queue.get();
  }

  void __fastcall QueryUser(TObject * /*Sender*/, const UnicodeString Query, TStrings * MoreMessages,
    unsigned int Answers, const TQueryParams * Params, unsigned int & Answer, ::TQueryType QueryType,
    void * /*Arg*/)
  {
    // As TCustomScpExplorerForm::MoreMessageDialog while keeping up to date.
    if (FeedError && (Params != NULL) && FLAGSET(Params->Params, qpAllowContinueOnError))
    {
      FeedError(Query, MoreMessages);
      Answer = ContinueAnswer(Answers);
      return;
    }
    Answer = AskFrontend(Query, MoreMessages, QueryType, Answers, Params);
  }

  void __fastcall PromptUser(TTerminal * /*Terminal*/, ::TPromptKind Kind, UnicodeString Name,
    UnicodeString Instructions, TStrings * Prompts, TStrings * Results, bool & Result, void * /*Arg*/)
  {
    TPrompt Prompt;
    Prompt.Kind = static_cast<TPromptKind>(Kind);
    Prompt.Name = Utf8(Name);
    Prompt.Instructions = Utf8(UnformatMessage(Instructions));
    for (int Index = 0; Index < Prompts->Count; Index++)
    {
      TPromptField Field;
      Field.Label = Utf8(Prompts->Strings[Index]);
      Field.Echo = FLAGSET(reinterpret_cast<intptr_t>(Prompts->Objects[Index]), pupEcho);
      if (Index < Results->Count)
      {
        Field.Value = Utf8(Results->Strings[Index]);
      }
      Prompt.Fields.push_back(Field);
    }
    Result = Frontend()->Prompt(Prompt);
    if (Result)
    {
      for (int Index = 0; Index < Prompts->Count; Index++)
      {
        const UnicodeString Value = FromUtf8(Prompt.Fields[Index].Value);
        if (Index < Results->Count)
        {
          Results->Strings[Index] = Value;
        }
        else
        {
          Results->Add(Value);
        }
      }
    }
  }

  void __fastcall DisplayBanner(TTerminal * /*Terminal*/, UnicodeString SessionName,
    const UnicodeString & Banner, bool & NeverShowAgain, int Options, unsigned int & /*Params*/)
  {
    Frontend()->Banner(Utf8(SessionName), Utf8(Banner), !FLAGSET(Options, boDisableNeverShowAgain),
      NeverShowAgain);
  }

  void __fastcall ShowExtendedException(TTerminal * /*Terminal*/, Exception * E, void * /*Arg*/)
  {
    TError Error;
    if (ErrorOf(E, Error))
    {
      Frontend()->ShowError(Error);
    }
  }

  // A transfer's progress, to the frontend; its answer may cancel it.
  void __fastcall OperationProgress(TFileOperationProgressType & ProgressData)
  {
    if (!ProgressData.IsTransfer() || !ProgressData.InProgress)
    {
      return;
    }
    TTransferProgress Progress;
    Progress.Upload = (ProgressData.Side == osLocal);
    Progress.FileName = Utf8(UniversalExcludeTrailingBackslash(ProgressData.Side != osLocal, ProgressData.FileName));
    Progress.FilePercent = ProgressData.TransferProgress();
    Progress.OverallPercent = ProgressData.OverallProgress();
    Progress.Transferred = ProgressData.TotalTransferred;
    Progress.Total = ProgressData.TotalSizeSet ? ProgressData.TotalSize : -1;
    Progress.FilesDone = ProgressData.FilesFinishedSuccessfully;
    Progress.FileCount = ProgressData.Count;
    Progress.Speed = ProgressData.CPS();
    if (!Frontend()->TransferProgress(Progress))
    {
      ProgressData.SetCancel(csCancel);
    }
  }

  // As the script's TerminalInformation: the progress lines while connecting.
  void __fastcall Information(TTerminal * ATerminal, const UnicodeString & Str, int Phase,
    const UnicodeString & /*Additional*/)
  {
    if ((Phase < 0) && (ATerminal->Status == ssOpening))
    {
      Frontend()->Information(Utf8(Str));
    }
  }
};

TSession::TSession(const std::string & Url) :
  F(new TImpl())
{
  try
  {
    int ParsedInfo = 0;
    F->Data.reset(StoredSessions->ParseUrl(FromUtf8(Url), NULL, ParsedInfo));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSession::TSession() :
  F(new TImpl())
{
}

// As GetFolderOrWorkspaceName.
namespace
{

// The process's parameters, or ones given (as another instance's).
TProgramParams * ProgramParamsOf(const std::vector<std::string> * Arguments, std::unique_ptr<TProgramParams> & Owner)
{
  if (Arguments == nullptr)
  {
    return TProgramParams::Instance();
  }
  UnicodeString CommandLine = ShellQuoteStr(ParamStr(0));
  for (const std::string & Argument : *Arguments)
  {
    CommandLine += L" " + ShellQuoteStr(FromUtf8(Argument));
  }
  Owner.reset(new TProgramParams(CommandLine));
  return Owner.get();
}

}  // namespace

std::string TSession::CommandLineFolderOrWorkspace(const std::vector<std::string> * Arguments)
{
  std::unique_ptr<TProgramParams> Owner;
  TProgramParams * Params = ProgramParamsOf(Arguments, Owner);
  if (Params->ParamCount == 0)
  {
    return std::string();
  }
  const UnicodeString Name = DecodeUrlChars(Params->Param[1]);
  return ((StoredSessions->FindByName(Name) == NULL) && StoredSessions->IsFolderOrWorkspace(Name)) ?
    Utf8(Name) : std::string();
}

std::unique_ptr<TSession> TSession::FromCommandLine(bool & CanOpen, const std::vector<std::string> * Arguments)
{
  CanOpen = false;
  try
  {
    std::unique_ptr<TProgramParams> Owner;
    TProgramParams * Params = ProgramParamsOf(Arguments, Owner);
    if (Arguments != nullptr)
    {
      StoredSessions->Reload();
    }
    if (Params->ParamCount == 0)
    {
      return nullptr;
    }
    const UnicodeString SessionName = Params->Param[1];
    // GetFolderOrWorkspaceName: several sites, which need tabs.
    if ((StoredSessions->FindByName(DecodeUrlChars(SessionName)) == NULL) &&
        StoredSessions->IsFolderOrWorkspace(DecodeUrlChars(SessionName)))
    {
      return nullptr;
    }
    // GetCommandLineParseUrlFlags; UNSAFE_SWITCH (WinInterface.h) is "Unsafe".
    const int Flags = pufAllowStoredSiteWithProtocol | FLAGMASK(Params->FindSwitch(L"Unsafe"), pufUnsafe);
    // A copy, as ParseUrl consumes /rawsettings.
    TOptions Options(*Params);
    int ParsedInfo = 0;
    UnicodeString DownloadFile;
    std::unique_ptr<TSession> Result(new TSession());
    Result->F->Data.reset(StoredSessions->ParseUrl(SessionName, &Options, ParsedInfo, &DownloadFile, NULL, Flags));
    CanOpen = Result->F->Data->CanOpen && FLAGCLEAR(ParsedInfo, piDefaultsOnly);
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSession::TSession(const TSite & Site) :
  F(new TImpl())
{
  try
  {
    // The site as edited, over the settings stored for it.
    F->Data.reset(SessionDataOf(Site, Site.Name));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSession::~TSession()
{
  Close();
}

std::string TSession::Name() const
{
  return Utf8(F->Data->SessionName);
}

std::string TSession::HostNameExpanded() const
{
  return Utf8(F->Data->HostNameExpanded);
}

std::string TSession::UserNameExpanded() const
{
  return Utf8(F->Data->UserNameExpanded);
}

bool TSession::UpdateDirectories() const
{
  return F->Data->UpdateDirectories;
}

bool TSession::SaveDirectories(const std::string & LocalDirectory)
{
  try
  {
    TSessionData * Stored = StoredSessions->FindSame(F->Data.get());
    if ((Stored == NULL) || !F->Data->UpdateDirectories)
    {
      return false;
    }
    // As TCustomScpExplorerForm::UpdateSessionData: only peeked, the
    // connection may be gone.
    if (F->Terminal != NULL)
    {
      const UnicodeString Current = F->Terminal->PeekCurrentDirectory();
      if (!Current.IsEmpty())
      {
        Stored->RemoteDirectory = Current;
      }
    }
    if (!LocalDirectory.empty())
    {
      Stored->LocalDirectory = FromUtf8(LocalDirectory);
    }
    // Modified only, implicit, as SaveTerminal.
    StoredSessions->Save(false, false);
    return true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Open()
{
  try
  {
    // As the script's open: a URL with no host asks for one.
    if (!F->Data->CanLogin)
    {
      TPrompt Prompt;
      Prompt.Kind = TPromptKind::Prompt;
      Prompt.Name = Utf8(AppNameString());
      TPromptField Field;
      Field.Label = Utf8(LoadStr(SCRIPT_HOST_PROMPT));
      Prompt.Fields.push_back(Field);
      if (!Frontend()->Prompt(Prompt) || Prompt.Fields[0].Value.empty())
      {
        Abort();
      }
      F->Data->HostName = FromUtf8(Prompt.Fields[0].Value);
    }
    F->QueueEnabled = dynamic_cast<TShellConfiguration *>(GUIConfiguration)->Settings().EnableQueueByDefault;
    F->Terminal.reset(new TTerminal(F->Data.get(), Configuration));
    TTerminal * Terminal = F->Terminal.get();
    Terminal->AutoReadDirectory = true;
    Terminal->OnQueryUser = CLOSURE_OF(F.get(), QueryUser);
    Terminal->OnPromptUser = CLOSURE_OF(F.get(), PromptUser);
    Terminal->OnDisplayBanner = CLOSURE_OF(F.get(), DisplayBanner);
    Terminal->OnShowExtendedException = CLOSURE_OF(F.get(), ShowExtendedException);
    Terminal->OnInformation = CLOSURE_OF(F.get(), Information);
    Terminal->OnProgress = CLOSURE_OF(F.get(), OperationProgress);
    Terminal->Open();
  }
  catch (Exception & E)
  {
    F->Terminal.reset();
    throw CoreErrorOf(E);
  }
}

bool TSession::Active() const
{
  return (F->Terminal != NULL) && F->Terminal->Active;
}

std::string TSession::CurrentDirectory() const
{
  return Active() ? Utf8(F->Terminal->CurrentDirectory) : std::string();
}

bool TSession::IsLocalBrowser() const
{
  return F->Data->IsLocalBrowser && F->Data->HostName.IsEmpty();
}

std::string TSession::OtherLocalDirectory() const
{
  return Utf8(F->Data->OtherLocalDirectory);
}

std::string TSession::LocalDirectory() const
{
  return Utf8(F->Data->LocalDirectoryExpanded);
}

namespace
{

TRemoteEntry EntryOf(const TRemoteFile * File)
{
  TRemoteEntry Entry;
  Entry.Name = Utf8(File->FileName);
  Entry.Directory = File->IsDirectory;
  Entry.Size = File->Size;
  Entry.Modified = Utf8(File->UserModificationStr);
  // TDateTime counts days from 1899-12-30; 1970 is day 25569.
  Entry.ModifiedTime = (static_cast<double>(File->Modification) - 25569.0) * 86400.0;
  // TUnixDirView::ItemFileTime's.
  switch (File->ModificationFmt)
  {
    case mfNone: Entry.ModifiedPrecision = TRemoteEntry::TPrecision::None; break;
    case mfMDHM:
    case mfYMDHM: Entry.ModifiedPrecision = TRemoteEntry::TPrecision::Minute; break;
    case mfMDY: Entry.ModifiedPrecision = TRemoteEntry::TPrecision::Day; break;
    case mfFull: Entry.ModifiedPrecision = TRemoteEntry::TPrecision::Second; break;
  }
  Entry.Rights = Utf8(File->RightsStr);
  Entry.Owner = Utf8(File->Owner.DisplayText);
  Entry.Group = Utf8(File->Group.DisplayText);
  Entry.Link = File->IsSymLink;
  Entry.LinkTarget = Utf8(File->LinkTo);
  Entry.Hidden = (Entry.Name.size() > 1) && (Entry.Name[0] == '.');
  return Entry;
}

}  // namespace

std::vector<TRemoteEntry> TSession::List() const
{
  std::vector<TRemoteEntry> Result;
  if (Active() && (F->Terminal->Files != NULL))
  {
    TRemoteDirectory * Files = F->Terminal->Files;
    for (int Index = 0; Index < Files->Count; Index++)
    {
      TRemoteFile * File = Files->Files[Index];
      if (File->IsThisDirectory || File->IsParentDirectory)
      {
        continue;
      }
      Result.push_back(EntryOf(File));
    }
  }
  return Result;
}

void TSession::ChangeDirectory(const std::string & Directory)
{
  try
  {
    if (!Active())
    {
      throw Exception(LoadStr(NOT_CONNECTED));
    }
    F->Terminal->ChangeDirectory(FromUtf8(Directory));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

void CheckActive(const TSession & Session)
{
  if (!Session.Active())
  {
    throw Exception(LoadStr(NOT_CONNECTED));
  }
}

// The current directory's entry Name, as core lists it.
TRemoteFile * FindFile(TTerminal * Terminal, const std::string & Name)
{
  TRemoteDirectory * Files = Terminal->Files;
  const UnicodeString FileName = FromUtf8(Name);
  for (int Index = 0; (Files != NULL) && (Index < Files->Count); Index++)
  {
    if (Files->Files[Index]->FileName == FileName)
    {
      return Files->Files[Index];
    }
  }
  throw Exception(FMTLOAD(FILE_NOT_EXISTS, (FileName)));
}

// Core's TRights of the bridge's (Properties, below).
TRights RightsOf(const TRightsState & State);
// The temporary files' transfer settings (Editing, below).
TCopyParamType TemporaryFileCopyParam(bool ForceText);

}  // namespace

// As TUnixDirView::ExecuteHomeDirectory: with "default directory is home",
// the site's own directory, when it has one and does not remember the last
// used; a relative one from the server's home.
void TSession::HomeDirectory(bool DefaultDirIsHome)
{
  try
  {
    CheckActive(*this);
    const UnicodeString Path = F->Terminal->SessionData->RemoteDirectory;
    if (DefaultDirIsHome && !Path.IsEmpty() && !F->Terminal->SessionData->UpdateDirectories)
    {
      if (Path[1] != L'/')
      {
        F->Terminal->BeginTransaction();
        try
        {
          F->Terminal->HomeDirectory();
          F->Terminal->ChangeDirectory(Path);
        }
        catch (...)
        {
          F->Terminal->EndTransaction();
          throw;
        }
        F->Terminal->EndTransaction();
      }
      else
      {
        F->Terminal->ChangeDirectory(Path);
      }
    }
    else
    {
      F->Terminal->HomeDirectory();
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Refresh()
{
  try
  {
    CheckActive(*this);
    F->Terminal->ReloadDirectory();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::CreateDirectory(const std::string & Name, const TNewDirectory * NewDirectory)
{
  try
  {
    CheckActive(*this);
    // Core reads the properties whatever is set in them: with nothing set,
    // the server's defaults (as the GUI's Create folder with nothing ticked).
    TRemoteProperties Properties;
    if ((NewDirectory != NULL) && NewDirectory->SetRights && F->Terminal->IsCapable[fcModeChanging])
    {
      Properties.Valid << vpRights;
      Properties.Rights = RightsOf(NewDirectory->Rights);
    }
    Properties.Valid << vpEncrypt;
    Properties.Encrypt = GUIConfiguration->CurrentCopyParam.EncryptNewFiles;
    F->Terminal->CreateDirectory(FromUtf8(Name), &Properties);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Rename(const std::string & Name, const std::string & NewName)
{
  try
  {
    CheckActive(*this);
    F->Terminal->RenameFile(FindFile(F->Terminal.get(), Name), FromUtf8(NewName));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As WinSCP's remote panel moves (TCustomScpExplorerForm's RemoteTransferFiles):
// the files with their TRemoteFile, through MoveFiles, which also tells the
// directory cache the target changed.
bool TSession::Move(const std::vector<std::string> & Names, const std::string & TargetDirectory)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    for (const std::string & Name : Names)
    {
      TRemoteFile * File = FindFile(F->Terminal.get(), Name);
      Files->AddObject(File->FileName, File);
    }
    return F->Terminal->MoveFiles(Files.get(), FromUtf8(TargetDirectory), L"*.*", false);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As the GUI deletes: the files' names, each with its TRemoteFile. (A
// directory's full name ends in a slash, which FileZilla's removedir refuses.)
bool TSession::Delete(const std::vector<std::string> & Names)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    for (const std::string & Name : Names)
    {
      TRemoteFile * File = FindFile(F->Terminal.get(), Name);
      Files->AddObject(File->FileName, File);
    }
    return F->Terminal->DeleteFiles(Files.get());
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

// Brackets a transfer for the frontend's progress window.
class TTransferBracket
{
public:
  TTransferBracket(bool Upload, int Count) { Frontend()->TransferStarted(Upload, Count); }
  ~TTransferBracket() { Frontend()->TransferFinished(); }
};

}  // namespace

bool TSession::Upload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    for (const std::string & Path : LocalPaths)
    {
      Files->Add(FromUtf8(Path));
    }
    const UnicodeString Target = RemoteDirectory.empty() ? F->Terminal->CurrentDirectory : FromUtf8(RemoteDirectory);
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    TTransferBracket Bracket(true, Files->Count);
    return F->Terminal->CopyToRemote(Files.get(), UnixIncludeTrailingBackslash(Target), &CopyParam,
      FLAGMASK(Move, cpDelete), NULL);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As the GUI downloads: the entries' names with their TRemoteFiles, as for
// deleting.
bool TSession::DownloadPaths(const std::vector<std::string> & Paths, const std::string & LocalDirectory,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    std::vector<std::unique_ptr<TRemoteFile>> Owned;
    for (const std::string & Path : Paths)
    {
      const UnicodeString APath = FromUtf8(Path);
      Owned.emplace_back(F->Terminal->ReadFile(APath));
      // Read alone, it has no directory listing to take its path from.
      Owned.back()->FullFileName = APath;
      Files->AddObject(APath, Owned.back().get());
    }
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    TTransferBracket Bracket(false, Files->Count);
    return F->Terminal->CopyToLocal(Files.get(), IncludeTrailingBackslash(FromUtf8(LocalDirectory)), &CopyParam, 0, NULL);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::Download(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    for (const std::string & Name : Names)
    {
      TRemoteFile * File = FindFile(F->Terminal.get(), Name);
      Files->AddObject(File->FileName, File);
    }
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    TTransferBracket Bracket(false, Files->Count);
    return F->Terminal->CopyToLocal(Files.get(), IncludeTrailingBackslash(FromUtf8(LocalDirectory)), &CopyParam,
      FLAGMASK(Move, cpDelete), NULL);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

TStrings * UploadList(const std::vector<std::string> & LocalPaths)
{
  TStringList * Files = new TStringList();
  for (const std::string & Path : LocalPaths)
  {
    Files->Add(FromUtf8(Path));
  }
  return Files;
}

TQueueEntry::TState StateOf(TQueueItem::TStatus Status)
{
  switch (Status)
  {
    case TQueueItem::qsConnecting: return TQueueEntry::TState::Connecting;
    case TQueueItem::qsProcessing: return TQueueEntry::TState::Processing;
    case TQueueItem::qsPrompt: return TQueueEntry::TState::Prompt;
    case TQueueItem::qsQuery: return TQueueEntry::TState::Query;
    case TQueueItem::qsError: return TQueueEntry::TState::Error;
    case TQueueItem::qsPaused: return TQueueEntry::TState::Paused;
    case TQueueItem::qsDone: return TQueueEntry::TState::Done;
    default: return TQueueEntry::TState::Pending;
  }
}

}  // namespace

// As TCustomScpExplorerForm::AddQueueItem: no confirmations when the
// settings say so, several connections when they allow.
void TSession::QueueUpload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStrings> Files(UploadList(LocalPaths));
    const UnicodeString Target = RemoteDirectory.empty() ? F->Terminal->CurrentDirectory : FromUtf8(RemoteDirectory);
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    F->EnsureQueue()->AddItem(new TUploadQueueItem(F->Terminal.get(), Files.get(),
      UnixIncludeTrailingBackslash(Target), &CopyParam,
      FLAGMASK(Move, cpDelete) | FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation), CopyParam.QueueParallel));
    F->QueueAdded++;
    F->QueueChanged = true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::QueueDownload(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    for (const std::string & Name : Names)
    {
      TRemoteFile * File = FindFile(F->Terminal.get(), Name);
      Files->AddObject(File->FileName, File);
    }
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    F->EnsureQueue()->AddItem(new TDownloadQueueItem(F->Terminal.get(), Files.get(),
      IncludeTrailingBackslash(FromUtf8(LocalDirectory)), &CopyParam,
      FLAGMASK(Move, cpDelete) | FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation), CopyParam.QueueParallel));
    F->QueueAdded++;
    F->QueueChanged = true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::QueueIdle()
{
  if (F->Queue == NULL)
  {
    return false;
  }
  try
  {
    F->Queue->Idle();
    const bool Changed = F->QueueChanged.exchange(false) || (F->QueueStatus == NULL);
    // Progress moves without list events: the running items are read anew
    // on every call.
    F->QueueStatus = F->Queue->CreateStatus(F->QueueStatus);
    F->QueueDone = F->QueueAdded - (F->QueueStatus->Count - F->QueueStatus->DoneCount);
    // As WinSCP's queue auto-popup (when on): the first question waiting is
    // asked.
    for (int Index = 0; GUIConfiguration->QueueAutoPopup && (Index < F->QueueStatus->Count); Index++)
    {
      TQueueItemProxy * Item = F->QueueStatus->Items[Index];
      if (TQueueItem::IsUserActionStatus(Item->Status))
      {
        Item->ProcessUserAction();
        break;
      }
    }
    return Changed || (F->QueueStatus->ActiveCount > 0);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::vector<TQueueEntry> TSession::QueueEntries() const
{
  std::vector<TQueueEntry> Result;
  for (int Index = 0; (F->QueueStatus != NULL) && (Index < F->QueueStatus->Count); Index++)
  {
    TQueueItemProxy * Item = F->QueueStatus->Items[Index];
    TQueueEntry Entry;
    Entry.Id = reinterpret_cast<std::uintptr_t>(Item);
    Entry.Upload = (Item->Info->Side == osLocal);
    Entry.Move = (Item->Info->Operation == foMove);
    Entry.Source = Utf8(Item->Info->Source);
    Entry.Destination = Utf8(Item->Info->Destination);
    Entry.State = StateOf(Item->Status);
    TFileOperationProgressType * Progress = Item->ProgressData;
    if ((Progress != NULL) && Progress->InProgress)
    {
      Entry.FileName = Utf8(Progress->FileName);
      Entry.Percent = Progress->OverallProgress();
      Entry.Transferred = Progress->TotalTransferred;
      Entry.Speed = Progress->CPS();
    }
    else if (Entry.State == TQueueEntry::TState::Done)
    {
      Entry.Percent = 100;
    }
    Result.push_back(Entry);
  }
  return Result;
}

int TSession::QueueDoneCount() const
{
  return F->QueueDone;
}

bool TSession::QueueEmpty() const
{
  return (F->Queue == NULL) || F->Queue->IsEmpty;
}

namespace
{

TQueueItemProxy * FindItem(TTerminalQueueStatus * Status, std::uintptr_t Id)
{
  for (int Index = 0; (Status != NULL) && (Index < Status->Count); Index++)
  {
    if (reinterpret_cast<std::uintptr_t>(Status->Items[Index]) == Id)
    {
      return Status->Items[Index];
    }
  }
  return NULL;
}

}  // namespace

bool TSession::QueueCancel(std::uintptr_t Id)
{
  TQueueItemProxy * Item = FindItem(F->QueueStatus, Id);
  return (Item != NULL) && Item->Delete();
}

bool TSession::QueuePause(std::uintptr_t Id)
{
  TQueueItemProxy * Item = FindItem(F->QueueStatus, Id);
  return (Item != NULL) && Item->Pause();
}

bool TSession::QueueResume(std::uintptr_t Id)
{
  TQueueItemProxy * Item = FindItem(F->QueueStatus, Id);
  return (Item != NULL) && Item->Resume();
}

void TSession::Close()
{
  KeepUpToDateStop();
  // The queue's connections and threads go first: they use the session's.
  delete F->QueueStatus;
  F->QueueStatus = NULL;
  F->Queue.reset();
  if (F->Terminal != NULL)
  {
    try
    {
      if (F->Terminal->Active)
      {
        F->Terminal->Close();
      }
    }
    catch (Exception & E)
    {
      // Closing a broken connection fails as it closes: nothing to tell.
    }
    F->Terminal.reset();
  }
}

// --- Synchronization -----------------------------------------------------------

struct TSyncChecklist::TImpl
{
  std::unique_ptr<TSynchronizeChecklist> Checklist;
  // Each entry's core item, in the order of Items().
  std::vector<const TSynchronizeChecklist::TItem *> Items;
  TTerminal::TSynchronizeMode Mode = TTerminal::smRemote;
  int Params = 0;
  TCopyParamType CopyParam;
};

namespace
{

// As TFullSynchronizeDialog::GetParams, with spNoConfirmation as the GUI
// adds it.
int SyncParamsOf(const TSyncParams & Params, bool CanTimestamps)
{
  const bool Timestamps = (Params.Mode == TSyncParams::TMode::Timestamps) && CanTimestamps;
  return TTerminal::spNoConfirmation |
    FLAGMASK(Params.Delete, TTerminal::spDelete) |
    FLAGMASK(Params.ExistingOnly, TTerminal::spExistingOnly) |
    FLAGMASK(Params.PreviewChanges, TTerminal::spPreviewChanges) |
    FLAGMASK(Timestamps, TTerminal::spTimestamp) |
    FLAGMASK(Params.Mode == TSyncParams::TMode::Mirror, TTerminal::spMirror) |
    FLAGMASK(!Params.ByTime, TTerminal::spNotByTime) |
    FLAGMASK(Params.BySize, TTerminal::spBySize) |
    FLAGMASK(Params.CaseSensitive, TTerminal::spCaseSensitive) |
    FLAGMASK(Params.SelectedOnly, TTerminal::spSelectedOnly) |
    FLAGMASK(Params.ByChecksum, TTerminal::spByChecksum);
}

TTerminal::TSynchronizeMode SyncModeOf(TSyncParams::TDirection Direction)
{
  switch (Direction)
  {
    case TSyncParams::TDirection::Local: return TTerminal::smLocal;
    case TSyncParams::TDirection::Both: return TTerminal::smBoth;
    default: return TTerminal::smRemote;
  }
}

// Core's directory callback, to the bridge's.
class TSyncDirectoryHandler
{
public:
  std::function<bool(const std::string &, const std::string &)> OnDirectory;

  void __fastcall Handle(const UnicodeString & LocalDirectory, const UnicodeString & RemoteDirectory,
    bool & Continue, bool /*Collect*/, const TSynchronizeOptions * /*Options*/)
  {
    if (OnDirectory && !OnDirectory(Utf8(LocalDirectory), Utf8(RemoteDirectory)))
    {
      Continue = false;
    }
  }
};

double UnixTime(TDateTime DateTime)
{
  // TDateTime counts days from 1899-12-30; 1970 is day 25569.
  return (static_cast<double>(DateTime) - 25569.0) * 86400.0;
}

}  // namespace

TSyncChecklist::TSyncChecklist(TImpl * Impl) :
  F(Impl)
{
  Load();
}

TSyncChecklist::~TSyncChecklist() = default;

void TSyncChecklist::Load()
{
  F->Items.clear();
  FItems.clear();
  for (int Index = 0; Index < F->Checklist->Count; Index++)
  {
    const TSynchronizeChecklist::TItem * Item = F->Checklist->Item[Index];
    TSyncItem Entry;
    Entry.Action = static_cast<TSyncItem::TAction>(Item->Action);
    Entry.Checked = Item->Checked;
    Entry.Directory = Item->IsDirectory;
    Entry.Name = Utf8(Item->GetFileName());
    Entry.LocalDirectory = Utf8(Item->Info1.Directory);
    Entry.RemoteDirectory = Utf8(Item->Info2.Directory);
    Entry.LocalExists = !Item->IsRemoteOnly();
    Entry.RemoteExists = !Item->IsLocalOnly();
    if (Entry.LocalExists)
    {
      Entry.LocalSize = Item->Info1.Size;
      Entry.LocalModified = Utf8(UserModificationStr(Item->Info1.Modification, Item->Info1.ModificationFmt));
      Entry.LocalModifiedTime = UnixTime(Item->Info1.Modification);
    }
    if (Entry.RemoteExists)
    {
      Entry.RemoteSize = Item->Info2.Size;
      Entry.RemoteModified = Utf8(UserModificationStr(Item->Info2.Modification, Item->Info2.ModificationFmt));
      Entry.RemoteModifiedTime = UnixTime(Item->Info2.Modification);
    }
    Entry.HasSize = Item->HasSize();
    F->Items.push_back(Item);
    FItems.push_back(Entry);
  }
}

namespace
{

using TChecklistAction = TSynchronizeChecklist::TAction;

bool IsTransferNewAction(TChecklistAction Action)
{
  return (Action == TSynchronizeChecklist::saUploadNew) || (Action == TSynchronizeChecklist::saDownloadNew);
}

TChecklistAction OppositeMoveAction(TChecklistAction Action)
{
  switch (Action)
  {
    case TSynchronizeChecklist::saUploadNew: return TSynchronizeChecklist::saDeleteRemote;
    case TSynchronizeChecklist::saDownloadNew: return TSynchronizeChecklist::saDeleteLocal;
    case TSynchronizeChecklist::saDeleteRemote: return TSynchronizeChecklist::saUploadNew;
    case TSynchronizeChecklist::saDeleteLocal: return TSynchronizeChecklist::saDownloadNew;
    default: return TSynchronizeChecklist::saNone;
  }
}

}  // namespace

bool TSyncChecklist::FindMoveCandidate(std::size_t Index, std::size_t & First, std::size_t & Second) const
{
  const std::size_t Count = F->Items.size();
  for (std::size_t Offset = 0; Offset < Count; Offset++)
  {
    const std::size_t At = (Index + Offset) % Count;
    const TSynchronizeChecklist::TItem * Item = F->Items[At];
    const TChecklistAction Opposite = OppositeMoveAction(Item->Action);
    // The focused one either way; the next ones as transfers only.
    if ((Opposite == TSynchronizeChecklist::saNone) || ((Offset > 0) && !IsTransferNewAction(Item->Action)))
    {
      continue;
    }
    for (std::size_t Other = 0; Other < Count; Other++)
    {
      const TSynchronizeChecklist::TItem * Candidate = F->Items[Other];
      if ((Other == At) || (Candidate->Action != Opposite) || (Candidate->IsDirectory != Item->IsDirectory))
      {
        continue;
      }
      bool Match;
      if (Item->IsDirectory)
      {
        Match = SameText(Item->GetFileName(), Candidate->GetFileName());
      }
      else if (Item->GetBaseSize() != Candidate->GetBaseSize())
      {
        Match = false;
      }
      else if (SameText(Item->GetFileName(), Candidate->GetFileName()))
      {
        Match = true;
      }
      else if ((Item->Action == TSynchronizeChecklist::saDeleteLocal) || (Item->Action == TSynchronizeChecklist::saDownloadNew))
      {
        Match = SamePaths(Item->Info1.Directory, Candidate->Info1.Directory);
      }
      else
      {
        Match = UnixSamePath(Item->Info2.Directory, Candidate->Info2.Directory);
      }
      if (Match)
      {
        First = At;
        Second = Other;
        return true;
      }
    }
  }
  return false;
}

bool TSyncChecklist::CanMove(const std::vector<std::size_t> & Indexes) const
{
  // As MoveAction's update: two opposite kinds of action...
  int Transfers = 0;
  int TransferDirectories = 0;
  TChecklistAction Transfer = TSynchronizeChecklist::saNone;
  TChecklistAction Delete = TSynchronizeChecklist::saNone;
  bool DeletesFiles = true;
  bool DeletesDirectories = true;
  for (std::size_t Index : Indexes)
  {
    const TSynchronizeChecklist::TItem * Item = F->Items.at(Index);
    if (IsTransferNewAction(Item->Action))
    {
      if ((Transfer != TSynchronizeChecklist::saNone) && (Transfer != Item->Action))
      {
        return false;
      }
      Transfer = Item->Action;
      Transfers++;
      TransferDirectories += Item->IsDirectory ? 1 : 0;
    }
    else if (OppositeMoveAction(Item->Action) != TSynchronizeChecklist::saNone)
    {
      if ((Delete != TSynchronizeChecklist::saNone) && (Delete != Item->Action))
      {
        return false;
      }
      Delete = Item->Action;
      DeletesFiles = DeletesFiles && !Item->IsDirectory;
      DeletesDirectories = DeletesDirectories && Item->IsDirectory;
    }
    else
    {
      return false;
    }
  }
  if ((Transfers != 1) || (Delete == TSynchronizeChecklist::saNone) || (OppositeMoveAction(Transfer) != Delete))
  {
    return false;
  }
  // ... exactly two of one kind, or the new one a directory.
  const bool TransferIsDirectory = (TransferDirectories == 1);
  return ((Indexes.size() == 2) && (TransferIsDirectory ? DeletesDirectories : DeletesFiles)) || TransferIsDirectory;
}

void TSyncChecklist::Check(std::size_t Index, bool Checked)
{
  const TSynchronizeChecklist::TItem * Item = F->Items.at(Index);
  F->Checklist->Update(Item, Checked, Item->Action);
  FItems[Index].Checked = Checked;
}

void TSyncChecklist::Reverse(std::size_t Index)
{
  const TSynchronizeChecklist::TItem * Item = F->Items.at(Index);
  const TSynchronizeChecklist::TAction Action = TSynchronizeChecklist::Reverse(Item->Action);
  F->Checklist->Update(Item, Item->Checked, Action);
  FItems[Index].Action = static_cast<TSyncItem::TAction>(Action);
}

int TSyncChecklist::CheckedCount() const
{
  return F->Checklist->CheckedCount;
}

bool TSession::CanSyncTimestamps() const
{
  return Active() && F->Terminal->IsCapable[fcTimestampChanging];
}

void TSession::SyncCalculateSizes(TSyncChecklist & Checklist, const std::vector<std::size_t> & Indexes)
{
  try
  {
    CheckActive(*this);
    TSyncChecklist::TImpl & C = *Checklist.F;
    for (std::size_t Index : Indexes)
    {
      const TSynchronizeChecklist::TItem * Item = C.Items.at(Index);
      if (!Item->IsDirectory)
      {
        continue;
      }
      // As DoSynchronizeChecklistCalculateSize: the side the size is of.
      __int64 Size = 0;
      if (!Item->IsRemoteOnly())
      {
        std::unique_ptr<TStringList> Files(new TStringList());
        Files->Add(Item->ForceGetLocalPath());
        F->Terminal->CalculateLocalFilesSize(Files.get(), Size, NULL, true, NULL, NULL);
      }
      else
      {
        std::unique_ptr<TStringList> Files(new TStringList());
        Files->AddObject(Item->ForceGetRemotePath(), Item->RemoteFile);
        TCalculateSizeParams Params;
        Params.UseCache = true;
        F->Terminal->CalculateFilesSize(Files.get(), Size, Params);
      }
      C.Checklist->UpdateDirectorySize(Item, Size);
    }
    Checklist.Load();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::SyncMove(TSyncChecklist & Checklist, const std::vector<std::size_t> & Indexes)
{
  try
  {
    CheckActive(*this);
    TSyncChecklist::TImpl & C = *Checklist.F;
    if (!Checklist.CanMove(Indexes))
    {
      return;
    }
    const TSynchronizeChecklist::TItem * Transfer = NULL;
    std::vector<const TSynchronizeChecklist::TItem *> Deletes;
    for (std::size_t Index : Indexes)
    {
      const TSynchronizeChecklist::TItem * Item = C.Items.at(Index);
      if (IsTransferNewAction(Item->Action))
      {
        Transfer = Item;
      }
      else
      {
        Deletes.push_back(Item);
      }
    }
    // As MoveActionExecute: where the new one is to be, on the deletes' side.
    const bool Remote = (Deletes.front()->Action == TSynchronizeChecklist::saDeleteRemote);
    const UnicodeString NewFileName = Remote ? UnixCombinePaths(Transfer->Info2.Directory, Transfer->Info1.FileName) :
      CombinePaths(Transfer->Info1.Directory, Transfer->Info2.FileName);
    const bool TargetIsDirectory = Transfer->IsDirectory && !((Deletes.size() == 1) && Deletes.front()->IsDirectory);
    // As DoSynchronizeMove.
    if (Remote)
    {
      std::unique_ptr<TStringList> Files(new TStringList());
      for (const TSynchronizeChecklist::TItem * Item : Deletes)
      {
        Files->AddObject(Item->ForceGetRemotePath(), Item->RemoteFile);
      }
      UnicodeString Target;
      UnicodeString FileMask;
      if (TargetIsDirectory)
      {
        Target = NewFileName;
        F->Terminal->CreateTargetDirectory(Target, faDirectory, &C.CopyParam);
      }
      else
      {
        Target = UnixExtractFileDir(NewFileName);
        FileMask = DelimitFileNameMask(UnixExtractFileName(NewFileName));
      }
      F->Terminal->MoveFiles(Files.get(), Target, FileMask, false);
    }
    else
    {
      if (TargetIsDirectory && !ForceDirectories(ApiPath(NewFileName)))
      {
        throw EOSExtException(FMTLOAD(CREATE_DIR_ERROR, (NewFileName)));
      }
      for (const TSynchronizeChecklist::TItem * Item : Deletes)
      {
        const UnicodeString FileName = Item->ForceGetLocalPath();
        const UnicodeString NewFilePath = TargetIsDirectory ? CombinePaths(NewFileName, ExtractFileName(FileName)) : NewFileName;
        F->Terminal->LogEvent(FORMAT(L"Moving file \"%s\" to \"%s\".", (FileName, NewFilePath)));
        if (!RenameFile(FileName, NewFilePath))
        {
          throw EOSExtException(FMTLOAD(RENAME_FILE_ERROR, (FileName, NewFilePath)));
        }
      }
    }
    // The entries done with: the moved ones, and the new one but a
    // directory (its other contents still to transfer).
    for (const TSynchronizeChecklist::TItem * Item : Deletes)
    {
      C.Checklist->Delete(Item);
    }
    if (!TargetIsDirectory)
    {
      C.Checklist->Delete(Transfer);
    }
    Checklist.Load();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::QueueEnabled() const
{
  return F->QueueEnabled;
}

void TSession::SetQueueEnabled(bool Enabled)
{
  F->QueueEnabled = Enabled;
  if (F->Queue)
  {
    F->Queue->Enabled = Enabled;
  }
}

bool TSession::CanSyncChecksum() const
{
  return Active() && (F->Terminal->IsCapable[fcCalculatingChecksum] ||
    (F->Terminal->IsCapable[fcSecondaryShell] && !F->Terminal->IsEncryptingFiles()));
}

TSyncDefaults SyncDefaults()
{
  const int Params = GUIConfiguration->SynchronizeParams;
  TSyncDefaults Result;
  TSyncParams & P = Result.Params;
  P.Delete = FLAGSET(Params, TTerminal::spDelete);
  P.ExistingOnly = FLAGSET(Params, TTerminal::spExistingOnly);
  P.PreviewChanges = FLAGSET(Params, TTerminal::spPreviewChanges);
  P.Mode = FLAGSET(Params, TTerminal::spTimestamp) ? TSyncParams::TMode::Timestamps :
    (FLAGSET(Params, TTerminal::spMirror) ? TSyncParams::TMode::Mirror : TSyncParams::TMode::Files);
  P.ByTime = FLAGCLEAR(Params, TTerminal::spNotByTime);
  P.BySize = FLAGSET(Params, TTerminal::spBySize);
  P.CaseSensitive = FLAGSET(Params, TTerminal::spCaseSensitive);
  P.SelectedOnly = FLAGSET(Params, TTerminal::spSelectedOnly);
  P.ByChecksum = FLAGSET(Params, TTerminal::spByChecksum);
  // SynchronizeModeAuto: TSynchronizeMode, or -1 (by the active panel).
  const int Mode = GUIConfiguration->SynchronizeModeAuto;
  Result.SaveMode = (Mode >= 0);
  P.Direction = (Mode == TTerminal::smLocal) ? TSyncParams::TDirection::Local :
    ((Mode == TTerminal::smBoth) ? TSyncParams::TDirection::Both : TSyncParams::TDirection::Remote);
  return Result;
}

void SetSyncDefaults(const TSyncParams & Params, bool SaveMode)
{
  try
  {
    // The params that are not the dialog's kept (TFullSynchronizeDialog's FParams).
    const int Dialog = TTerminal::spDelete | TTerminal::spExistingOnly | TTerminal::spPreviewChanges |
      TTerminal::spTimestamp | TTerminal::spNotByTime | TTerminal::spBySize | TTerminal::spSelectedOnly |
      TTerminal::spMirror | TTerminal::spCaseSensitive | TTerminal::spByChecksum;
    GUIConfiguration->SynchronizeParams = (GUIConfiguration->SynchronizeParams & ~Dialog) |
      (SyncParamsOf(Params, true) & Dialog);
    if (SaveMode)
    {
      GUIConfiguration->SynchronizeModeAuto = SyncModeOf(Params.Direction);
    }
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As TCustomScpExplorerForm::DoFullSynchronizeDirectories collects: core's
// default transfer settings, rooted at the two directories.
std::unique_ptr<TSyncChecklist> TSession::SyncCollect(const std::string & LocalDirectory,
  const std::string & RemoteDirectory, const TSyncParams & Params,
  const std::function<bool(const std::string &, const std::string &)> & OnDirectory, const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TSyncChecklist::TImpl> Impl(new TSyncChecklist::TImpl());
    Impl->Mode = SyncModeOf(Params.Direction);
    Impl->Params = SyncParamsOf(Params, CanSyncTimestamps());
    // As DoFullSynchronizeDirectories: the GUI's current transfer settings.
    Impl->CopyParam = TransferCopyParam(Settings);
    const UnicodeString Local = FromUtf8(LocalDirectory);
    const UnicodeString Remote = FromUtf8(RemoteDirectory);
    Impl->CopyParam.IncludeFileMask.SetRoots(Local, Remote);
    TSyncDirectoryHandler Handler;
    Handler.OnDirectory = OnDirectory;
    TSynchronizeOptions Options;
    // As GetSynchronizeOptions: the panels' selection, by name.
    if (Params.SelectedOnly && !Params.Selected.empty())
    {
      Options.Filter = new TStringList();
      Options.Filter->CaseSensitive = false;
      Options.Filter->Duplicates = Types::dupAccept;
      for (const std::string & Name : Params.Selected)
      {
        Options.Filter->Add(FromUtf8(Name));
      }
      Options.Filter->Sort();
    }
    Impl->Checklist.reset(F->Terminal->SynchronizeCollect(Local, Remote, Impl->Mode, &Impl->CopyParam,
      Impl->Params, CLOSURE_OF(&Handler, Handle), &Options));
    return std::unique_ptr<TSyncChecklist>(new TSyncChecklist(Impl.release()));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As TCustomScpExplorerForm::FullSynchronize: the checked entries, with the
// progress window.
bool TSession::SyncApply(TSyncChecklist & Checklist)
{
  try
  {
    CheckActive(*this);
    TSyncChecklist::TImpl & C = *Checklist.F;
    TTransferBracket Bracket(C.Mode != TTerminal::smLocal, C.Checklist->CheckedCount);
    TFileOperationStatistics Statistics;
    F->Terminal->SynchronizeApply(C.Checklist.get(), &C.CopyParam, C.Params | TTerminal::spNoConfirmation,
      TSynchronizeDirectory(), TProcessedSynchronizationChecklistItem(), TUpdatedSynchronizationChecklistItems(),
      NULL, &Statistics);
    return true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As TCustomScpExplorerForm::DoQueueSynchronize.
void TSession::SyncQueue(TSyncChecklist & Checklist)
{
  try
  {
    CheckActive(*this);
    TSyncChecklist::TImpl & C = *Checklist.F;
    const int CopyParams = TTerminal::GetSynchronizeCopyParams(C.Params);
    TCopyParamType SyncCopyParam = TTerminal::GetSynchronizeCopyParam(&C.CopyParam, C.Params);
    int Index = 0;
    const TSynchronizeChecklist::TItem * Item;
    while (C.Checklist->GetNextChecked(Index, Item))
    {
      F->EnsureQueue()->AddItem(F->Terminal->SynchronizeToQueue(Item, &SyncCopyParam, CopyParams, false));
      F->QueueAdded++;
    }
    F->QueueChanged = true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// --- Keeping the remote directory up to date -------------------------------

namespace
{

// The params and options the dialog sets (TSynchronizeDialog's GetParams).
const int KeepUpToDateDialogParams =
  TTerminal::spDelete | TTerminal::spExistingOnly | TTerminal::spSelectedOnly | TTerminal::spTimestamp;
const int KeepUpToDateDialogOptions = soRecurse | soSynchronize | soSynchronizeAsk | soContinueOnError;
// Those of the GUI's synchronize params keep-up-to-date does not use
// (DoSynchronizeDirectories' UnusedParams).
const int KeepUpToDateUnusedParams =
  TTerminal::spPreviewChanges | TTerminal::spTimestamp | TTerminal::spNotByTime | TTerminal::spBySize |
  TTerminal::spCaseSensitive | TTerminal::spMirror | TTerminal::spByChecksum;

TSynchronizeParamType SynchronizeParamsOf(const TKeepUpToDateParams & Params)
{
  TSynchronizeParamType Result;
  Result.LocalDirectory = FromUtf8(Params.LocalDirectory);
  Result.RemoteDirectory = FromUtf8(Params.RemoteDirectory);
  Result.Params =
    (GUIConfiguration->SynchronizeParams & ~(KeepUpToDateUnusedParams | KeepUpToDateDialogParams)) |
    FLAGMASK(Params.Delete, TTerminal::spDelete) |
    FLAGMASK(Params.ExistingOnly, TTerminal::spExistingOnly) |
    FLAGMASK(Params.SelectedOnly, TTerminal::spSelectedOnly);
  using TFirst = TKeepUpToDateParams::TSynchronizeFirst;
  Result.Options =
    (GUIConfiguration->SynchronizeOptions & ~KeepUpToDateDialogOptions) |
    FLAGMASK(Params.Recurse, soRecurse) |
    FLAGMASK(Params.SynchronizeFirst == TFirst::Yes, soSynchronize) |
    FLAGMASK(Params.SynchronizeFirst == TFirst::Ask, soSynchronizeAsk) |
    FLAGMASK(Params.ContinueOnError, soContinueOnError);
  return Result;
}

}  // namespace

TKeepUpToDateParams KeepUpToDateDefaults()
{
  const int Params = GUIConfiguration->SynchronizeParams;
  const int Options = GUIConfiguration->SynchronizeOptions;
  TKeepUpToDateParams Result;
  Result.Delete = FLAGSET(Params, TTerminal::spDelete);
  Result.ExistingOnly = FLAGSET(Params, TTerminal::spExistingOnly);
  Result.SelectedOnly = FLAGSET(Params, TTerminal::spSelectedOnly);
  Result.Recurse = FLAGSET(Options, soRecurse);
  using TFirst = TKeepUpToDateParams::TSynchronizeFirst;
  Result.SynchronizeFirst =
    FLAGSET(Options, soSynchronizeAsk) ? TFirst::Ask : (FLAGSET(Options, soSynchronize) ? TFirst::Yes : TFirst::No);
  Result.ContinueOnError = FLAGSET(Options, soContinueOnError);
  return Result;
}

void SaveKeepUpToDateDefaults(const TKeepUpToDateParams & Params, bool SaveSettings)
{
  const TSynchronizeParamType SParams = SynchronizeParamsOf(Params);
  if (SaveSettings)
  {
    GUIConfiguration->SynchronizeParams =
      SParams.Params | (GUIConfiguration->SynchronizeParams & KeepUpToDateUnusedParams);
    GUIConfiguration->SynchronizeOptions = SParams.Options;
  }
  else if (FLAGSET(GUIConfiguration->SynchronizeOptions, soSynchronizeAsk) &&
           FLAGCLEAR(SParams.Options, soSynchronizeAsk) &&
           FLAGSET(SParams.Options, soSynchronize))
  {
    GUIConfiguration->SynchronizeOptions =
      (GUIConfiguration->SynchronizeOptions & ~soSynchronizeAsk) | soSynchronize;
  }
}

std::string KeepUpToDateSynchronizeQuestion()
{
  return Utf8(LoadStr(SYNCHRONISE_BEFORE_KEEPUPTODATE2));
}

// Keep-up-to-date, as TCustomScpExplorerForm runs TSynchronizeController
// (DoSynchronizeDirectories, DoSynchronize, Synchronize), with
// TSynchronizeDialog's handlers (DoLog, DoAbort, FeedSynchronizeError).
class TKeepUpToDateRun
{
public:
  TKeepUpToDateRun(TTerminal * ATerminal, const TKeepUpToDateEvents & AEvents) :
    FTerminal(ATerminal),
    FEvents(AEvents),
    FController(CLOSURE(DoSynchronize), CLOSURE(DoSynchronizeInvalid), CLOSURE(DoSynchronizeTooManyDirectories))
  {
  }

  ~TKeepUpToDateRun()
  {
    Stop();
  }

  void Start(const TSynchronizeParamType & Params, const TCopyParamType & CopyParam,
    const std::vector<std::string> & Selected)
  {
    FParams = Params;
    // As TCustomScpExplorerForm::GetSynchronizeOptions.
    if (FLAGSET(Params.Params, TTerminal::spSelectedOnly) && !Selected.empty())
    {
      FOptions.Filter = new TStringList();
      FOptions.Filter->CaseSensitive = false;
      FOptions.Filter->Duplicates = Types::dupAccept;
      for (const std::string & Name : Selected)
      {
        FOptions.Filter->Add(FromUtf8(Name));
      }
      FOptions.Filter->Sort();
    }
    // Uploads and deletes are logged as the GUI's FileOperationFinished does.
    FPreviousOnFinished = FTerminal->OnFinished;
    FTerminal->OnFinished = CLOSURE(FileOperationFinished);
    FStarted = true;
    try
    {
      FController.StartStop(NULL, true, FParams, CopyParam, &FOptions, CLOSURE(DoAbort), TSynchronizeThreadsEvent(),
        CLOSURE(DoLog));
    }
    catch (...)
    {
      Stop();
      throw;
    }
  }

  void Stop()
  {
    if (FStarted)
    {
      FStarted = false;
      FController.StartStop(NULL, false, FParams, TCopyParamType(), NULL, TSynchronizeAbortEvent(),
        TSynchronizeThreadsEvent(), TSynchronizeLog());
      FTerminal->OnFinished = FPreviousOnFinished;
    }
  }

  bool ContinueOnError() const
  {
    return FLAGSET(FParams.Options, soContinueOnError);
  }

  void FeedError(const UnicodeString & Message, TStrings * MoreMessages)
  {
    Log(TKeepUpToDateLog::ContinuedError, Message, MoreMessages);
  }

private:
  TTerminal * FTerminal;
  TKeepUpToDateEvents FEvents;
  TSynchronizeController FController;
  TSynchronizeParamType FParams;
  TSynchronizeOptions FOptions;
  TFileOperationFinished FPreviousOnFinished;
  bool FStarted = false;

  void Log(TKeepUpToDateLog Entry, const UnicodeString & Message, TStrings * MoreMessages)
  {
    if (FEvents.OnLog)
    {
      const std::string More =
        ((MoreMessages != NULL) && (MoreMessages->Count > 0)) ? Utf8(UnformatMessage(MoreMessages->Text)) : std::string();
      FEvents.OnLog(Entry, Utf8(UnformatMessage(Message)), More);
    }
    // The dialog's OnSynchronizeSessionLog (SynchronizeSessionLog).
    LogSynchronizeEvent(FTerminal, Message);
  }

  void __fastcall DoLog(TSynchronizeController * /*Controller*/, TSynchronizeLogEntry Entry,
    const UnicodeString Message)
  {
    Log(static_cast<TKeepUpToDateLog>(Entry), Message, NULL);
  }

  void __fastcall DoAbort(TObject * /*Sender*/, bool Close)
  {
    if (FEvents.OnAbort)
    {
      FEvents.OnAbort(Close);
    }
  }

  void __fastcall FileOperationFinished(TFileOperation Operation, TOperationSide Side, bool Temp,
    const UnicodeString & FileName, bool Success, bool NotCancelled, TOnceDoneOperation & OnceDoneOperation)
  {
    if (FPreviousOnFinished != NULL)
    {
      FPreviousOnFinished(Operation, Side, Temp, FileName, Success, NotCancelled, OnceDoneOperation);
    }
    if (Success)
    {
      if ((Operation == foCopy) && (Side == osLocal))
      {
        FController.LogOperation(soUpload, FileName);
      }
      else if ((Operation == foDelete) && (Side == osRemote))
      {
        FController.LogOperation(soDelete, FileName);
      }
    }
  }

  void ShowException(Exception & E)
  {
    TError Error;
    if (ErrorOf(&E, Error))
    {
      Frontend()->ShowError(Error);
    }
  }

  void __fastcall DoSynchronize(TSynchronizeController * /*Sender*/, const UnicodeString LocalDirectory,
    const UnicodeString RemoteDirectory, const TCopyParamType & CopyParam, const TSynchronizeParamType & Params,
    TSynchronizeChecklist ** Checklist, TSynchronizeOptions * Options, bool Full)
  {
    if (FTerminal->Status != ssOpened)
    {
      return;
    }
    try
    {
      int PParams = Params.Params;
      if (!Full)
      {
        PParams |= TTerminal::spNoRecurse | TTerminal::spUseCache | TTerminal::spDelayProgress;
        if (FLAGSET(Params.Options, soRecurse))
        {
          PParams |= TTerminal::spSubDirs;
        }
      }
      else if (FLAGCLEAR(Params.Options, soRecurse))
      {
        // A non-recursive keep-up-to-date's full synchronization is too.
        PParams |= TTerminal::spNoRecurse;
      }
      Synchronize(LocalDirectory, RemoteDirectory, CopyParam, PParams, Checklist, Options);
    }
    catch (Exception & E)
    {
      if (!FTerminal->Active)
      {
        ShowException(E);
        throw;
      }
      else if (!ContinueOnError())
      {
        // Mostly EAbort, which shows nothing; otherwise an error listing
        // directories while looking for differences.
        ShowException(E);
        throw;
      }
      // Else already logged, through FeedError.
    }
  }

  // As TCustomScpExplorerForm::Synchronize (keep-up-to-date mode).
  void Synchronize(const UnicodeString & LocalDirectory, const UnicodeString & RemoteDirectory,
    const TCopyParamType & CopyParam, int Params, TSynchronizeChecklist ** Checklist, TSynchronizeOptions * Options)
  {
    std::unique_ptr<TSynchronizeChecklist> AChecklist(FTerminal->SynchronizeCollect(LocalDirectory, RemoteDirectory,
      TTerminal::smRemote, &CopyParam, Params | TTerminal::spNoConfirmation, TSynchronizeDirectory(), Options));
    if (AChecklist->CheckedCount > 0)
    {
      TTransferBracket Bracket(true, AChecklist->CheckedCount);
      FTerminal->SynchronizeApply(AChecklist.get(), &CopyParam, Params | TTerminal::spNoConfirmation,
        TSynchronizeDirectory(), TProcessedSynchronizationChecklistItem(), TUpdatedSynchronizationChecklistItems(),
        NULL, NULL);
    }
    if (Checklist != NULL)
    {
      *Checklist = AChecklist.release();
    }
  }

  void __fastcall DoSynchronizeInvalid(TSynchronizeController * /*Sender*/, const UnicodeString Directory,
    const UnicodeString ErrorStr)
  {
    TError Error;
    Error.Message = Utf8(Directory.IsEmpty() ? LoadStr(WATCH_ERROR_GENERAL) : FMTLOAD(WATCH_ERROR_DIRECTORY, (Directory)));
    Error.MoreMessages = Utf8(ErrorStr);
    Frontend()->ShowError(Error);
  }

  // As TCustomScpExplorerForm::DoSynchronizeTooManyDirectories.
  void __fastcall DoSynchronizeTooManyDirectories(TSynchronizeController * /*Sender*/, int & MaxDirectories)
  {
    if (MaxDirectories < GUIConfiguration->MaxWatchDirectories)
    {
      MaxDirectories = GUIConfiguration->MaxWatchDirectories;
    }
    else
    {
      TQueryParams Params(qpNeverAskAgainCheck, HELP_TOO_MANY_WATCH_DIRECTORIES);
      const unsigned int Result = AskFrontend(FMTLOAD(TOO_MANY_WATCH_DIRECTORIES, (MaxDirectories, MaxDirectories)),
        NULL, qtConfirmation, qaYes | qaNo, &Params);
      if ((Result == qaYes) || (Result == qaNeverAskAgain))
      {
        MaxDirectories *= 2;
        if (Result == qaNeverAskAgain)
        {
          GUIConfiguration->MaxWatchDirectories = MaxDirectories;
        }
      }
      else
      {
        Abort();
      }
    }
  }
};

std::set<TSession::TImpl *> & TSession::TImpl::Live()
{
  static std::set<TImpl *> Result;
  return Result;
}

TSession::TImpl::TImpl()
{
  Live().insert(this);
}

TSession::TImpl::~TImpl()
{
  Live().erase(this);
}

// As TTerminalManager::RecryptPasswords: what the sessions open keep.
void TSession::RecryptPasswords()
{
  for (TImpl * Impl : TImpl::Live())
  {
    if (Impl->Data)
    {
      Impl->Data->RecryptPasswords();
    }
    if (Impl->Terminal)
    {
      Impl->Terminal->RecryptPasswords();
    }
  }
}

void TSession::KeepUpToDateStart(const TKeepUpToDateParams & Params, const TTransferSettings * Settings,
  const std::vector<std::string> & Selected, const TKeepUpToDateEvents & Events)
{
  try
  {
    CheckActive(*this);
    KeepUpToDateStop();
    // As DoStartStop: the question is asked by now.
    TKeepUpToDateParams Started = Params;
    if (Started.SynchronizeFirst == TKeepUpToDateParams::TSynchronizeFirst::Ask)
    {
      Started.SynchronizeFirst = TKeepUpToDateParams::TSynchronizeFirst::No;
    }
    // As TSynchronizeDialog::GetCopyParams.
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    CopyParam.PreserveTime = true;
    CopyParam.NewerOnly = false;
    std::unique_ptr<TKeepUpToDateRun> Run(new TKeepUpToDateRun(F->Terminal.get(), Events));
    TKeepUpToDateRun * R = Run.get();
    if (Started.ContinueOnError)
    {
      F->FeedError = [R](const UnicodeString & Message, TStrings * MoreMessages) { R->FeedError(Message, MoreMessages); };
    }
    try
    {
      R->Start(SynchronizeParamsOf(Started), CopyParam, Selected);
    }
    catch (...)
    {
      F->FeedError = nullptr;
      throw;
    }
    F->KeepUpToDate = std::move(Run);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::KeepUpToDateStop()
{
  F->FeedError = nullptr;
  F->KeepUpToDate.reset();
}

bool TSession::KeepingUpToDate() const
{
  return (F->KeepUpToDate != NULL);
}

void TSession::KeepUpToDateIdle()
{
  if (F->KeepUpToDate != NULL)
  {
    try
    {
      TDiscMonitor::Pump();
      if (F->Terminal != NULL)
      {
        F->Terminal->Idle();
      }
    }
    catch (Exception & E)
    {
      throw CoreErrorOf(E);
    }
  }
}

// --- Properties --------------------------------------------------------------

namespace
{

std::unique_ptr<TStringList> FileListOf(TTerminal * Terminal, const std::vector<std::string> & Names)
{
  std::unique_ptr<TStringList> Files(new TStringList());
  for (const std::string & Name : Names)
  {
    TRemoteFile * File = FindFile(Terminal, Name);
    Files->AddObject(File->FileName, File);
  }
  return Files;
}

TRightsState RightsStateOf(const TRights & Rights)
{
  TRightsState Result;
  Result.Set = 0;
  Result.Unset = 0;
  for (int Right = TRights::rrFirst; Right <= TRights::rrLast; Right++)
  {
    const unsigned Flag = TRights::RightToFlag(static_cast<TRights::TRight>(Right));
    switch (Rights.RightUndef[static_cast<TRights::TRight>(Right)])
    {
      case TRights::rsYes: Result.Set |= Flag; break;
      case TRights::rsNo: Result.Unset |= Flag; break;
      default: break;
    }
  }
  return Result;
}

TRights RightsOf(const TRightsState & State)
{
  TRights Result;
  Result.AllowUndef = true;
  for (int Right = TRights::rrFirst; Right <= TRights::rrLast; Right++)
  {
    const unsigned Flag = TRights::RightToFlag(static_cast<TRights::TRight>(Right));
    Result.RightUndef[static_cast<TRights::TRight>(Right)] =
      (State.Set & Flag) ? TRights::rsYes : ((State.Unset & Flag) ? TRights::rsNo : TRights::rsUndef);
  }
  if (!Result.IsUndef)
  {
    Result.AllowUndef = false;
  }
  return Result;
}

// TPropertiesDialog::LoadRemoteToken.
UnicodeString TokenText(const TRemoteToken & Token, bool ByID)
{
  if (ByID && Token.IDValid)
  {
    return Token.NameValid ? FORMAT(L"%s [%d]", (Token.Name, int(Token.ID))) : FORMAT(L"[%d]", (int(Token.ID)));
  }
  return Token.Name;
}

// TPropertiesDialog::StoreRemoteToken.
TRemoteToken StoreToken(unsigned int ID, const UnicodeString & Text, const TRemoteTokenList * List)
{
  const TRemoteToken * Token = (List != NULL) ? List->Find(ID) : NULL;
  if (Token != NULL)
  {
    return *Token;
  }
  TRemoteToken Result;
  Result.ID = ID;
  Result.Name = Text;
  return Result;
}

TRemoteToken StoreToken(const TRemoteToken & Orig, UnicodeString Text, int Message, const TRemoteTokenList * List, bool ByID)
{
  TRemoteToken Result;
  Text = Text.Trim();
  if (!Text.IsEmpty())
  {
    if (ByID)
    {
      int IDStart = Text.LastDelimiter(L"[");
      if ((IDStart >= 0) && (Text[Text.Length()] == L']'))
      {
        int ID;
        UnicodeString IDStr = Text.SubString(IDStart + 1, Text.Length() - IDStart - 1);
        if (!TryStrToInt(IDStr, ID))
        {
          throw Exception(LoadStr(Message));
        }
        Result = StoreToken(ID, Text.SubString(1, IDStart - 1).Trim(), List);
      }
      else
      {
        const TRemoteToken * Token = (List != NULL) ? List->Find(Text) : NULL;
        int ID;
        if (Token != NULL)
        {
          Result = *Token;
        }
        else if (TryStrToInt(Text, ID))
        {
          Result = StoreToken(ID, Text, List);
        }
        else
        {
          throw Exception(MainInstructions(FMTLOAD(PROPERTIES_UNKNOWN_TOKEN, (Text))));
        }
      }
    }
    else
    {
      Result.Name = Text;
    }
  }
  if (TokenText(Result, ByID) == TokenText(Orig, ByID))
  {
    Result = Orig;
  }
  return Result;
}

class TChecksumCollector
{
public:
  std::vector<std::pair<std::string, std::string>> Result;

  void __fastcall Calculated(const UnicodeString & FileName, const UnicodeString & /*Alg*/, const UnicodeString & Hash)
  {
    Result.emplace_back(Utf8(FileName), Utf8(Hash));
  }
};

}  // namespace

bool TSession::CanChangeRights() const
{
  return Active() && F->Terminal->IsCapable[fcModeChanging];
}

bool TSession::CanLink() const
{
  return Active() && F->Terminal->ResolvingSymlinks && F->Terminal->IsCapable[fcSymbolicLink];
}

bool TSession::CanHardLink() const
{
  return Active() && F->Terminal->IsCapable[fcHardLink];
}

bool TSession::CanEditLinks() const
{
  return Active() && F->Terminal->SessionData->ResolveSymlinks;
}

void TSession::CreateLink(const std::string & FileName, const std::string & PointTo, bool Symbolic, bool Replace)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    if (Replace)
    {
      TRemoteFile * File = FindFile(Terminal, FileName);
      int Params = dfNoRecursive;
      Terminal->ExceptionOnFail = true;
      try
      {
        Terminal->DeleteFile(L"", File, &Params);
      }
      catch (...)
      {
        Terminal->ExceptionOnFail = false;
        throw;
      }
      Terminal->ExceptionOnFail = false;
    }
    Terminal->CreateLink(FromUtf8(FileName), FromUtf8(PointTo), Symbolic);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool IsFileNameMask(const std::string & Mask)
{
  return ::IsFileNameMask(FromUtf8(Mask));
}

std::string MultiFilesToOneConfirmation(const std::string & Target, bool Unix)
{
  return Utf8(UnformatMessage(FormatMultiFilesToOneConfirmation(FromUtf8(Target), Unix)));
}

namespace
{

std::pair<std::string, std::string> Row(int Label, const UnicodeString & Value)
{
  return { Utf8(LoadStr(Label)), Utf8(Value) };
}

}  // namespace

// TFileSystemInfoDialog::Feed's items.
TServerInfo TSession::ServerInfo()
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    const TSessionInfo & Session = Terminal->GetSessionInfo();
    const TFileSystemInfo & FileSystem = Terminal->GetFileSystemInfo(true);
    TServerInfo Result;
    const auto Capability = [&FileSystem](TFSCapability Capability)
    {
      return BooleanToStr(FileSystem.IsCapable[Capability]);
    };
    const auto Pair = [&Capability](TFSCapability One, TFSCapability Two)
    {
      return FORMAT(L"%s/%s", (Capability(One), Capability(Two)));
    };
    Result.Server.push_back(Row(FSINFO_REMOTE_SYSTEM, FileSystem.RemoteSystem));
    Result.Server.push_back(Row(FSINFO_FS_PROTOCOL, FileSystem.ProtocolName));
    Result.Server.push_back(Row(FSINFO_CRYPTOGRAPHIC_PROTOCOL, Session.SecurityProtocolName));
    Result.Server.push_back(Row(FSINFO_SSH_IMPLEMENTATION, Session.SshImplementation));
    UnicodeString Str = Session.CSCipher;
    if (Session.CSCipher != Session.SCCipher)
    {
      Str += FORMAT(L"/%s", (Session.SCCipher));
    }
    Result.Server.push_back(Row(FSINFO_CIPHER, Str));
    Str = DefaultStr(Session.CSCompression, LoadStr(NO_STR));
    if (Session.CSCompression != Session.SCCompression)
    {
      Str += FORMAT(L"/%s", (DefaultStr(Session.SCCompression, LoadStr(NO_STR))));
    }
    Result.Server.push_back(Row(FSINFO_COMPRESSION, Str));
    // ControlsAddItem: the fingerprint's first two words are the algorithm.
    UnicodeString HostKey = Session.HostKeyFingerprintSHA256;
    if (!HostKey.IsEmpty())
    {
      UnicodeString Alg1 = CutToChar(HostKey, L' ', true);
      UnicodeString Alg2 = CutToChar(HostKey, L' ', true);
      Result.HostKeyAlgorithm = Utf8(FORMAT(L"%s %s", (Alg1, Alg2)));
      Result.HostKeySha256 = Utf8(HostKey);
      UnicodeString Md5 = Session.HostKeyFingerprintMD5;
      CutToChar(Md5, L' ', true);
      CutToChar(Md5, L' ', true);
      Result.HostKeyMd5 = Utf8(Md5);
    }
    Result.CertificateSha256 = Utf8(Session.CertificateFingerprintSHA256);
    Result.CertificateSha1 = Utf8(Session.CertificateFingerprintSHA1);
    Result.Certificate = Utf8(Session.Certificate);
    Result.Capabilities.push_back(Row(FSINFO_MODE_CHANGING, Capability(fcModeChanging)));
    Result.Capabilities.push_back(Row(FSINFO_ACL_CHANGING, Capability(fcAclChangingFiles)));
    Result.Capabilities.push_back(Row(FSINFO_OWNER_GROUP_CHANGING, Capability(fcGroupChanging)));
    Result.Capabilities.push_back(Row(FSINFO_ANY_COMMAND,
      (!FileSystem.IsCapable[fcShellAnyCommand] && FileSystem.IsCapable[fcAnyCommand]) ?
        LoadStr(FSINFO_PROTOCOL_ANY_COMMAND) : Capability(fcAnyCommand)));
    Result.Capabilities.push_back(Row(FSINFO_SYMBOLIC_HARD_LINK, Pair(fcSymbolicLink, fcHardLink)));
    Result.Capabilities.push_back(Row(FSINFO_USER_GROUP_LISTING, Capability(fcUserGroupListing)));
    Result.Capabilities.push_back(Row(FSINFO_REMOTE_COPY, Capability(fcRemoteCopy)));
    Result.Capabilities.push_back(Row(FSINFO_CHECKING_SPACE_AVAILABLE, Capability(fcCheckingSpaceAvailable)));
    Result.Capabilities.push_back(Row(FSINFO_CALCULATING_CHECKSUM, Capability(fcCalculatingChecksum)));
    Result.Capabilities.push_back(Row(FSINFO_NATIVE_TEXT_MODE, Capability(fcNativeTextMode)));
    Result.AdditionalInfo = Utf8(FileSystem.AdditionalInfo);
    Result.CanCheckSpace = Terminal->IsCapable[fcCheckingSpaceAvailable];
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSpaceInfo TSession::SpaceAvailable(const std::string & Path)
{
  try
  {
    CheckActive(*this);
    TSpaceAvailable Space;
    F->Terminal->SpaceAvailable(FromUtf8(Path), Space);
    TSpaceInfo Result;
    Result.Items.emplace_back(Utf8(LoadStr(FSINFO_BYTES_ON_DEVICE)), Space.BytesOnDevice);
    Result.Items.emplace_back(Utf8(LoadStr(FSINFO_UNUSED_BYTES_ON_DEVICE)), Space.UnusedBytesOnDevice);
    Result.Items.emplace_back(Utf8(LoadStr(FSINFO_BYTES_AVAILABLE_TO_USER)), Space.BytesAvailableToUser);
    Result.Items.emplace_back(Utf8(LoadStr(FSINFO_UNUSED_BYTES_AVAILABLE_TO_USER)), Space.UnusedBytesAvailableToUser);
    Result.Items.emplace_back(Utf8(LoadStr(FSINFO_BYTES_PER_ALLOCATION_UNIT)), static_cast<std::int64_t>(Space.BytesPerAllocationUnit));
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::CanRunCommands() const
{
  return Active() && (F->Terminal->IsCapable[fcAnyCommand] || F->Terminal->CommandSessionOpened);
}

bool TSession::CanOpenShellSession() const
{
  return Active() && F->Terminal->IsCapable[fcSecondaryShell];
}

std::string TSession::CommandSessionQuestion() const
{
  const UnicodeString Protocol = F->Terminal->GetFileSystemInfo().ProtocolName;
  return Utf8(FMTLOAD(PERFORM_ON_COMMAND_SESSION2, (Protocol, Protocol)));
}

// TCustomScpExplorerForm::CommandSessionFallback.
void TSession::OpenShellSession()
{
  try
  {
    CheckActive(*this);
    F->Terminal->CommandSession->Open();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

class TOutputCollector
{
public:
  std::function<void(const std::string &)> Output;

  void __fastcall Captured(const UnicodeString & Str, TCaptureOutputType OutputType)
  {
    // TConsoleDialog::AddLine: output and errors.
    if (((OutputType == cotOutput) || (OutputType == cotError)) && Output)
    {
      Output(Utf8(Str));
    }
  }
};

}  // namespace

// TConsoleDialog::DoExecuteCommand.
void TSession::RunCommand(const std::string & Command, const std::function<void(const std::string & Line)> & Output)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    TOutputCollector Collector;
    Collector.Output = Output;
    Terminal->ExceptionOnFail = true;
    try
    {
      Terminal->AnyCommand(FromUtf8(Command), CLOSURE_OF(&Collector, Captured));
    }
    catch (...)
    {
      Terminal->ExceptionOnFail = false;
      if (Terminal->Active)
      {
        Terminal->ReadCurrentDirectory();
      }
      throw;
    }
    Terminal->ExceptionOnFail = false;
    if (Terminal->Active)
    {
      Terminal->ReadCurrentDirectory();
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

// TCustomScpExplorerForm::CloneCurrentSessionData: the session's data as
// it is now, the current directory and the credentials used. The caller
// owns it.
TSessionData * CloneCurrentSessionData(TTerminal * Terminal)
{
  std::unique_ptr<TSessionData> Data(new TSessionData(L""));
  Data->Assign(Terminal->SessionData);
  // UpdateSessionData: only peeked.
  const UnicodeString Current = Terminal->PeekCurrentDirectory();
  if (!Current.IsEmpty())
  {
    Data->RemoteDirectory = Current;
  }
  Terminal->UpdateSessionCredentials(Data.get());
  if (!Terminal->SessionData->HasSessionName())
  {
    Data->Name = UnicodeString();
  }
  Data->IsWorkspace = false;
  return Data.release();
}

// TCustomScpExplorerForm::SessionDataForCode: the same, its host key known.
TSessionData * SessionDataForCode(TTerminal * Terminal)
{
  std::unique_ptr<TSessionData> Data(CloneCurrentSessionData(Terminal));
  Terminal->FillSessionDataForCode(Data.get());
  return Data.release();
}

class TCommandOutput
{
public:
  std::vector<std::string> Lines;

  void __fastcall Captured(const UnicodeString & Str, TCaptureOutputType OutputType)
  {
    if ((OutputType == cotOutput) || (OutputType == cotError))
    {
      Lines.push_back(Utf8(Str));
    }
  }
};

}  // namespace

int ParamsOf(const TCustomCommandDefinition & Command);

bool TSession::CanRunShellCommands() const
{
  return Active() && (F->Terminal->IsCapable[fcShellAnyCommand] || F->Terminal->CommandSessionOpened);
}

// TCustomScpExplorerForm::CustomCommand and RemoteCustomCommand.
std::vector<std::string> TSession::RunCustomCommand(const TCustomCommandDefinition & Definition,
  const std::vector<std::string> & Names, const TCustomCommandPrompts & Prompts)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    if (Definition.Local)
    {
      // Local commands (downloading the files to run them on) are not
      // ported yet.
      NotSupported();
    }
    if (!Terminal->IsCapable[fcShellAnyCommand] && !Terminal->CommandSessionOpened)
    {
      NotSupported();
    }
    std::unique_ptr<TSessionData> SessionData(SessionDataForCode(Terminal));
    TCustomCommandData Data(SessionData.get());
    const UnicodeString Site = Terminal->SessionData->SessionKey;
    // An extension's options in it, as GetCommandWithExpandedOptions.
    UnicodeString CommandCommand = FromUtf8(Definition.Command);
    const TCustomCommandType * Listed = WinConfiguration->CustomCommandList()->Find(FromUtf8(Definition.Name));
    if ((Listed != NULL) && (Listed->Command == CommandCommand))
    {
      CommandCommand = Listed->GetCommandWithExpandedOptions(WinConfiguration->CustomCommandOptions(), Site);
    }
    TRemoteCustomCommand RemoteCustomCommand(Data, Terminal->CurrentDirectory);
    TBridgeInteractiveCustomCommand InteractiveCustomCommand(&RemoteCustomCommand, FromUtf8(Definition.Name), Prompts);
    UnicodeString Command = InteractiveCustomCommand.Complete(CommandCommand, false);
    const bool Capture = Definition.ShowResults || Definition.CopyResults || Definition.ShowResultsInMessageBox;
    TCommandOutput Output;
    TCaptureOutputEvent OutputEvent = NULL;
    if (Capture)
    {
      OutputEvent = CLOSURE_OF(&Output, Captured);
    }
    if (!RemoteCustomCommand.IsFileCommand(Command))
    {
      Terminal->AnyCommand(RemoteCustomCommand.Complete(Command, true), OutputEvent);
    }
    else
    {
      std::unique_ptr<TStringList> Files = FileListOf(Terminal, Names);
      Terminal->CustomCommandOnFiles(Command, ParamsOf(Definition), Files.get(), OutputEvent);
    }
    return Output.Lines;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TCustomScpExplorerForm::SessionDataForCode, then the dialog's
// generation.
TGeneratedCode TSession::GenerateCode(const TGenerateCodeOptions & Options)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    std::unique_ptr<TSessionData> Data(new TSessionData(L""));
    Data->Assign(Terminal->SessionData);
    Data->RemoteDirectory = Terminal->CurrentDirectory;
    Terminal->UpdateSessionCredentials(Data.get());
    if (!Terminal->SessionData->HasSessionName())
    {
      Data->Name = UnicodeString();
    }
    Data->IsWorkspace = false;
    Terminal->FillSessionDataForCode(Data.get());
    // The command-line client beside the shell, upstream's WinSCP.com.
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    UnicodeString Cli = IncludeTrailingBackslash(ExtractFilePath(Shell->ModuleFileName())) +
#if defined(_WIN32)
      L"linscp-cli.exe";
#else
      L"linscp-cli";
#endif
    return GenerateSessionCode(Data.get(), Options, Cli);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string TSession::SessionKey() const
{
  return Active() ? Utf8(F->Terminal->SessionData->SessionKey) : std::string();
}

namespace
{

class TFindCollector
{
public:
  std::function<bool(const std::string &, const TRemoteEntry &)> Found;
  std::function<bool(const std::string &)> Searching;
  std::vector<std::pair<UnicodeString, std::unique_ptr<TRemoteFile>>> * Files;

  void __fastcall FileFound(TTerminal * /*Terminal*/, const UnicodeString FileName, const TRemoteFile * File, bool & Cancel)
  {
    Files->emplace_back(FileName, std::unique_ptr<TRemoteFile>(File->Duplicate(true)));
    if (Found && !Found(Utf8(UnixExtractFilePath(FileName)), EntryOf(File)))
    {
      Cancel = true;
    }
  }

  void __fastcall FindingFile(TTerminal * /*Terminal*/, const UnicodeString Directory, bool & Cancel)
  {
    if (Searching && !Searching(Utf8(Directory)))
    {
      Cancel = true;
    }
  }
};

}  // namespace

void TSession::FindFiles(const std::string & Directory, const std::string & Mask,
  const std::function<bool(const std::string & Directory, const TRemoteEntry & Entry)> & Found,
  const std::function<bool(const std::string & Directory)> & Searching)
{
  try
  {
    CheckActive(*this);
    F->Found.clear();
    TFileMasks Masks(FromUtf8(Mask));
    TFindCollector Collector;
    Collector.Found = Found;
    Collector.Searching = Searching;
    Collector.Files = &F->Found;
    F->Terminal->FilesFind(FromUtf8(Directory), Masks, CLOSURE_OF(&Collector, FileFound), CLOSURE_OF(&Collector, FindingFile));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

std::unique_ptr<TStringList> FoundList(
  const std::vector<std::pair<UnicodeString, std::unique_ptr<TRemoteFile>>> & Found, const std::vector<int> & Indexes)
{
  std::unique_ptr<TStringList> Files(new TStringList());
  for (int Index : Indexes)
  {
    if ((Index >= 0) && (Index < static_cast<int>(Found.size())))
    {
      Files->AddObject(Found[Index].first, Found[Index].second.get());
    }
  }
  return Files;
}

}  // namespace

bool TSession::DownloadFound(const std::vector<int> & Indexes, const std::string & LocalDirectory,
  const TTransferSettings * Settings)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files = FoundList(F->Found, Indexes);
    TGUICopyParamType CopyParam = TransferCopyParam(Settings);
    TTransferBracket Bracket(false, Files->Count);
    return F->Terminal->CopyToLocal(Files.get(), IncludeTrailingBackslash(FromUtf8(LocalDirectory)), &CopyParam, 0, NULL);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::DeleteFound(const std::vector<int> & Indexes)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files = FoundList(F->Found, Indexes);
    return F->Terminal->DeleteFiles(Files.get());
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::CanRemoteCopy() const
{
  return Active() && F->Terminal->IsCapable[fcRemoteCopy];
}

bool TSession::DirectoryExists(const std::string & Path)
{
  try
  {
    return Active() && F->Terminal->DirectoryExists(FromUtf8(Path));
  }
  catch (Exception &)
  {
    return false;
  }
}

bool TSession::RemoteMove(const std::vector<std::string> & Names, const std::string & Target, const std::string & FileMask)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files = FileListOf(F->Terminal.get(), Names);
    return F->Terminal->MoveFiles(Files.get(), FromUtf8(Target), FromUtf8(FileMask), false);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TCustomScpExplorerForm::RemoteTransferFiles' copy: the server's, or down
// to a temporary directory (TemporarilyDownloadFiles) and up again.
namespace
{

// RemoteCopy's: on the server, or through a local temporary copy.
bool RemoteCopyFiles(TTerminal * Terminal, TStrings * Files, const std::string & Target, const std::string & FileMask, bool Direct)
{
  if (Direct)
  {
    return Terminal->CopyFiles(Files, FromUtf8(Target), FromUtf8(FileMask), false);
  }
  const UnicodeString Root = IncludeTrailingBackslash(Configuration->TemporaryDir());
  if (!ForceDirectories(ApiPath(Root)))
  {
    throw EOSExtException(FMTLOAD(CREATE_TEMP_DIR_ERROR, (Root)));
  }
  bool Result = false;
  try
  {
    TCopyParamType DownParam = TemporaryFileCopyParam(false);
    {
      TTransferBracket Bracket(false, Files->Count);
      Terminal->ExceptionOnFail = true;
      try
      {
        Terminal->CopyToLocal(Files, Root, &DownParam, cpTemporary, NULL);
      }
      catch (...)
      {
        Terminal->ExceptionOnFail = false;
        throw;
      }
      Terminal->ExceptionOnFail = false;
    }
    std::unique_ptr<TStringList> Temporary(new TStringList());
    TSearchRecChecked Rec;
    if (FindFirstUnchecked(Root + L"*", faAnyFile, Rec) == 0)
    {
      do
      {
        if ((Rec.Name != L".") && (Rec.Name != L".."))
        {
          Temporary->Add(Root + Rec.Name);
        }
      }
      while (FindNextChecked(Rec) == 0);
      FindClose(Rec);
    }
    if (Temporary->Count > 0)
    {
      TGUICopyParamType UpParam = GUIConfiguration->CurrentCopyParam;
      UpParam.FileMask = FromUtf8(FileMask);
      TTransferBracket Bracket(true, Temporary->Count);
      Terminal->CopyToRemote(Temporary.get(), FromUtf8(Target), &UpParam, cpTemporary, NULL);
    }
    Result = true;
  }
  catch (...)
  {
    RecursiveDeleteFile(ExcludeTrailingBackslash(Root));
    throw;
  }
  RecursiveDeleteFile(ExcludeTrailingBackslash(Root));
  return Result;
}

}  // namespace

bool TSession::RemoteCopy(const std::vector<std::string> & Names, const std::string & Target, const std::string & FileMask,
  bool Direct)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files = FileListOf(F->Terminal.get(), Names);
    return RemoteCopyFiles(F->Terminal.get(), Files.get(), Target, FileMask, Direct);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::RemoteCopyPaths(const std::vector<std::string> & Paths, const std::string & Target, const std::string & FileMask,
  bool Direct)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> Files(new TStringList());
    std::vector<std::unique_ptr<TRemoteFile>> Owned;
    for (const std::string & Path : Paths)
    {
      const UnicodeString APath = FromUtf8(Path);
      Owned.emplace_back(F->Terminal->ReadFile(APath));
      Owned.back()->FullFileName = APath;
      Files->AddObject(APath, Owned.back().get());
    }
    return RemoteCopyFiles(F->Terminal.get(), Files.get(), Target, FileMask, Direct);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TNewDirectory NewDirectorySettings()
{
  const TRemoteProperties Properties = GUIConfiguration->NewDirectoryProperties;
  TNewDirectory Result;
  Result.SetRights = Properties.Valid.Contains(vpRights);
  // A sensible value even when not set, as TCreateDirectoryDialog expects.
  Result.Rights = RightsStateOf(Properties.Rights);
  return Result;
}

void SetNewDirectorySettings(const TNewDirectory & Settings)
{
  try
  {
    TRemoteProperties Properties = GUIConfiguration->NewDirectoryProperties;
    Properties.Rights = RightsOf(Settings.Rights);
    Properties.Valid = Settings.SetRights ? (Properties.Valid << vpRights) : (Properties.Valid >> vpRights);
    GUIConfiguration->NewDirectoryProperties = Properties;
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string RightsText(const TRightsState & Rights)
{
  return Utf8(RightsOf(Rights).Text);
}

bool RightsFromText(const std::string & Text, TRightsState & Rights)
{
  const UnicodeString Value = FromUtf8(Text).Trim();
  try
  {
    TRights Result;
    if ((Value.Length() == 3) || (Value.Length() == 4))
    {
      Result.Octal = Value;
    }
    else if (Value.Length() == TRights::TextLen)
    {
      Result.AllowUndef = true;
      Result.Text = Value;
    }
    else
    {
      return false;
    }
    Rights = RightsStateOf(Result);
    return true;
  }
  catch (Exception &)
  {
    return false;
  }
}

TPropertiesInfo TSession::PropertiesOf(const std::vector<std::string> & Names)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    std::unique_ptr<TStringList> Files = FileListOf(Terminal, Names);
    Terminal->LoadFilesProperties(Files.get());
    // Loading may have read the files again.
    Files = FileListOf(Terminal, Names);
    TPropertiesInfo Result;
    const bool ByID = Terminal->IsCapable[fcGroupOwnerChangingByID];
    Result.CanChangeGroup = Terminal->IsCapable[fcGroupChanging];
    Result.CanChangeOwner = Terminal->IsCapable[fcOwnerChanging];
    Result.CanChangeRights = Terminal->IsCapable[fcModeChanging];
    // As upstream: the server's lists, and what the directory's first
    // hundred files and the selection have.
    std::unique_ptr<TRemoteTokenList> Groups(Result.CanChangeGroup ? Terminal->Groups->Duplicate() : new TRemoteTokenList());
    std::unique_ptr<TRemoteTokenList> Users(Result.CanChangeOwner ? Terminal->Users->Duplicate() : new TRemoteTokenList());
    TRemoteDirectory * Directory = Terminal->Files;
    const int Count = std::min(Directory->Count, 100);
    for (int Index = 0; Index < Count; Index++)
    {
      Groups->AddUnique(Directory->Files[Index]->Group);
      Users->AddUnique(Directory->Files[Index]->Owner);
    }
    for (int Index = 0; Index < Files->Count; Index++)
    {
      TRemoteFile * File = static_cast<TRemoteFile *>(Files->Objects[Index]);
      Groups->AddUnique(File->Group);
      Users->AddUnique(File->Owner);
    }
    if (Result.CanChangeGroup)
    {
      for (int Index = 0; Index < Groups->Count(); Index++)
      {
        Result.Groups.push_back(Utf8(TokenText(*Groups->Token(Index), ByID)));
      }
    }
    if (Result.CanChangeOwner)
    {
      for (int Index = 0; Index < Users->Count(); Index++)
      {
        Result.Owners.push_back(Utf8(TokenText(*Users->Token(Index), ByID)));
      }
    }
    const TRemoteProperties Common = TRemoteProperties::CommonProperties(Files.get());
    Result.Properties.HasRights = Common.Valid.Contains(vpRights);
    if (Result.Properties.HasRights)
    {
      Result.Properties.Rights = RightsStateOf(Common.Rights);
    }
    Result.Properties.HasOwner = Common.Valid.Contains(vpOwner);
    Result.Properties.Owner = Utf8(TokenText(Common.Owner, ByID));
    Result.Properties.HasGroup = Common.Valid.Contains(vpGroup);
    Result.Properties.Group = Utf8(TokenText(Common.Group, ByID));
    if (Common.Valid.Contains(vpTags))
    {
      // Key and value lines, as TPropertiesDialog::SetFileProperties reads them.
      Result.Properties.HasTags = true;
      std::unique_ptr<TStrings> Tags(TextToStringList(Common.Tags));
      for (int Index = 0; Index + 1 < Tags->Count; Index += 2)
      {
        Result.Properties.Tags.emplace_back(Utf8(Tags->Strings[Index]), Utf8(Tags->Strings[Index + 1]));
      }
    }
    // TPropertiesDialog::LoadInfo.
    for (int Index = 0; Index < Files->Count; Index++)
    {
      TRemoteFile * File = static_cast<TRemoteFile *>(Files->Objects[Index]);
      if (File->IsDirectory)
      {
        Result.Directories++;
        Result.CanCalculateSize = Result.CanCalculateSize || !File->IsSymLink;
      }
      else
      {
        Result.Files++;
      }
      if (File->IsSymLink)
      {
        Result.SymLinks++;
      }
      Result.Size += File->Size;
    }
    // TCustomScpExplorerForm's poTags, and TPropertiesDialog's ShowTags.
    Result.ShowTags = Terminal->IsCapable[fcTags] && (Result.Files == 1) && (Result.Directories == 0);
    if ((Files->Count == 1) && static_cast<TRemoteFile *>(Files->Objects[0])->IsSymLink)
    {
      Result.LinkTarget = Utf8(static_cast<TRemoteFile *>(Files->Objects[0])->LinkTo);
    }
    // TCustomScpExplorerForm::CanCalculateChecksum, without the secondary
    // shell.
    if (Terminal->IsCapable[fcCalculatingChecksum])
    {
      std::unique_ptr<TStrings> Algs(new TStringList());
      Terminal->GetSupportedChecksumAlgs(Algs.get());
      for (int Index = 0; Index < Algs->Count; Index++)
      {
        Result.ChecksumAlgorithms.push_back(Utf8(Algs->Strings[Index]));
      }
    }
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TPropertiesDialog::GetFileProperties, then TCustomScpExplorerForm's
// SetProperties.
bool TSession::SetProperties(const std::vector<std::string> & Names, const TFileProperties & Properties)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    std::unique_ptr<TStringList> Files = FileListOf(Terminal, Names);
    const bool ByID = Terminal->IsCapable[fcGroupOwnerChangingByID];
    TRemoteProperties Current = TRemoteProperties::CommonProperties(Files.get());
    // What the dialog could change (TPropertiesDialog::SetFileProperties).
    TValidProperties Valid;
    if (Current.Valid.Contains(vpRights) && Terminal->IsCapable[fcModeChanging])
    {
      Valid << vpRights;
    }
    if (Current.Valid.Contains(vpOwner) && Terminal->IsCapable[fcOwnerChanging])
    {
      Valid << vpOwner;
    }
    if (Current.Valid.Contains(vpGroup) && Terminal->IsCapable[fcGroupChanging])
    {
      Valid << vpGroup;
    }
    if (Current.Valid.Contains(vpTags) && Terminal->IsCapable[fcTags])
    {
      Valid << vpTags;
    }
    Current.Valid = Valid;
    Current.Recursive = false;
    TRemoteProperties New;
    if (Terminal->IsCapable[fcModeChanging] && Properties.HasRights)
    {
      New.Valid << vpRights;
      New.Rights = RightsOf(Properties.Rights);
      New.AddXToDirectories = Properties.AddXToDirectories;
    }
    if (Terminal->IsCapable[fcGroupChanging])
    {
      New.Group = StoreToken(Current.Group, FromUtf8(Properties.Group), PROPERTIES_INVALID_GROUP, Terminal->Groups, ByID);
      if (New.Group.IsSet)
      {
        New.Valid << vpGroup;
      }
    }
    if (Terminal->IsCapable[fcOwnerChanging])
    {
      New.Owner = StoreToken(Current.Owner, FromUtf8(Properties.Owner), PROPERTIES_INVALID_OWNER, Terminal->Users, ByID);
      if (New.Owner.IsSet)
      {
        New.Valid << vpOwner;
      }
    }
    if (Terminal->IsCapable[fcTags] && Properties.HasTags && (Files->Count == 1))
    {
      // As GetFileProperties: key and value lines.
      std::unique_ptr<TStrings> Tags(new TStringList());
      for (const auto & Tag : Properties.Tags)
      {
        Tags->Add(FromUtf8(Tag.first));
        Tags->Add(FromUtf8(Tag.second));
      }
      New.Tags = Tags->Text;
      New.Valid << vpTags;
    }
    New.Recursive = Properties.Recursive;
    New = TRemoteProperties::ChangedProperties(Current, New);
    if (New.Valid.Empty() && !New.Recursive)
    {
      return false;
    }
    Terminal->ExceptionOnFail = true;
    try
    {
      Terminal->ChangeFilesProperties(Files.get(), &New);
    }
    catch (...)
    {
      Terminal->ExceptionOnFail = false;
      throw;
    }
    Terminal->ExceptionOnFail = false;
    return true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::CalculateSize(const std::vector<std::string> & Names, std::int64_t & Size, int & Files, int & Directories,
  int & SymLinks)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> List = FileListOf(F->Terminal.get(), Names);
    TCalculateSizeStats Stats;
    TCalculateSizeParams Params;
    Params.Stats = &Stats;
    __int64 Total = 0;
    F->Terminal->CalculateFilesSize(List.get(), Total, Params);
    Size = Total;
    Files = Stats.Files;
    Directories = Stats.Directories;
    SymLinks = Stats.SymLinks;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::vector<std::pair<std::string, std::string>> TSession::Checksums(const std::vector<std::string> & Names,
  const std::string & Algorithm)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStringList> List = FileListOf(F->Terminal.get(), Names);
    TChecksumCollector Collector;
    F->Terminal->CalculateFilesChecksum(FromUtf8(Algorithm), List.get(), CLOSURE_OF(&Collector, Calculated));
    return Collector.Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// --- Editing -----------------------------------------------------------------

struct TEditedFile::TShared
{
  std::mutex Mutex;
  double SourceTimestamp = 0;
};

TEditedFile::TEditedFile() :
  FShared(std::make_shared<TShared>())
{
}

double TEditedFile::SourceTimestamp() const
{
  std::lock_guard<std::mutex> Lock(FShared->Mutex);
  return FShared->SourceTimestamp;
}

void TEditedFile::SetSourceTimestamp(double Timestamp)
{
  std::lock_guard<std::mutex> Lock(FShared->Mutex);
  FShared->SourceTimestamp = Timestamp;
}

namespace
{

double UnixTimeOf(TDateTime DateTime)
{
  return (static_cast<double>(DateTime) - 25569.0) * 86400.0;
}

// TCustomScpExplorerForm::TemporaryFileCopyParam.
TCopyParamType TemporaryFileCopyParam(bool ForceText)
{
  TCopyParamType CopyParam = GUIConfiguration->CurrentCopyParam;
  CopyParam.FileNameCase = ncNoChange;
  CopyParam.PreserveRights = false;
  CopyParam.PreserveReadOnly = false;
  CopyParam.ReplaceInvalidChars = true;
  CopyParam.IncludeFileMask = TFileMasks();
  CopyParam.NewerOnly = false;
  CopyParam.FileMask = L"";
  if (ForceText)
  {
    CopyParam.TransferMode = tmAscii;
  }
  return CopyParam;
}

// TCustomScpExplorerForm's TEditorUploadQueueItem: the upload, then the
// file's timestamp on the server as the new source timestamp, on the
// queue's own connection.
class TEditorUploadQueueItem : public TUploadQueueItem
{
public:
  TEditorUploadQueueItem(TTerminal * Terminal, TStrings * FilesToCopy, const UnicodeString & TargetDir,
      const TCopyParamType * CopyParam, int Params, const TEditedFile & File, bool CheckNotModified) :
    TUploadQueueItem(Terminal, FilesToCopy, TargetDir, CopyParam, Params, false),
    FFile(File),
    FCheckNotModified(CheckNotModified)
  {
  }

protected:
  virtual void __fastcall DoTransferExecute(TTerminal * Terminal, TParallelOperation * ParallelOperation)
  {
    TUploadQueueItem::DoTransferExecute(Terminal, ParallelOperation);
    if (FCheckNotModified)
    {
      std::unique_ptr<TRemoteFile> Remote(
        Terminal->TryReadFile(UnixCombinePaths(FromUtf8(FFile.RemoteDirectory), FromUtf8(FFile.OriginalName))));
      if (Remote.get() != NULL)
      {
        FFile.SetSourceTimestamp(UnixTimeOf(Remote->Modification));
      }
    }
  }

private:
  TEditedFile FFile;
  bool FCheckNotModified;
};

}  // namespace

// TCustomScpExplorerForm::TemporaryDirectoryForRemoteFiles, then
// TemporarilyDownloadFiles.
namespace
{

// TemporaryDirectoryForRemoteFiles: where a remote file edited goes, made;
// Root: what to delete after, when not deterministic.
UnicodeString TemporaryEditDirectory(TTerminal * Terminal, const TCopyParamType & CopyParam, UnicodeString & Root)
{
  TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
  const TShellSettings & Settings = Shell->Settings();
  const UnicodeString RemoteDirectory = Terminal->CurrentDirectory;
  UnicodeString Directory;
  if (!Settings.TemporaryDirectoryDeterministic)
  {
    Root = IncludeTrailingBackslash(Configuration->TemporaryDir());
    Directory = Root;
  }
  else
  {
    Directory = IncludeTrailingBackslash(Shell->ExpandedTemporaryDirectory());
  }
  if (Settings.TemporaryDirectoryAppendSession)
  {
    Directory = IncludeTrailingBackslash(Directory + MakeValidFileName(Terminal->SessionData->SessionName));
  }
  if (Settings.TemporaryDirectoryAppendPath)
  {
    UnicodeString Path = RemoteDirectory;
    if (!Path.IsEmpty() && (Path[1] == L'/'))
    {
      Path.Delete(1, 1);
    }
    Directory = IncludeTrailingBackslash(Directory + CopyParam.ValidLocalPath(FromUnixPath(Path)));
  }
  if (!ForceDirectories(ApiPath(Directory)))
  {
    throw EOSExtException(FMTLOAD(CREATE_TEMP_DIR_ERROR, (Directory)));
  }
  return Directory;
}

}  // namespace

TEditedFile TSession::EditDownload(const std::string & Name, bool ForceText)
{
  try
  {
    CheckActive(*this);
    TCopyParamType CopyParam = TemporaryFileCopyParam(ForceText);
    const UnicodeString RemoteDirectory = F->Terminal->CurrentDirectory;
    UnicodeString Root;
    const UnicodeString Directory = TemporaryEditDirectory(F->Terminal.get(), CopyParam, Root);
    TRemoteFile * File = FindFile(F->Terminal.get(), Name);
    std::unique_ptr<TStringList> Files(new TStringList());
    Files->AddObject(File->FileName, File);
    const TDateTime Modification = File->Modification;
    {
      TTransferBracket Bracket(false, 1);
      F->Terminal->ExceptionOnFail = true;
      try
      {
        // As upstream: no confirmations, the same file may come over again.
        F->Terminal->CopyToLocal(Files.get(), Directory, &CopyParam, cpNoConfirmation | cpTemporary, NULL);
      }
      catch (...)
      {
        F->Terminal->ExceptionOnFail = false;
        if (!Root.IsEmpty())
        {
          RecursiveDeleteFile(ExcludeTrailingBackslash(Root));
        }
        throw;
      }
      F->Terminal->ExceptionOnFail = false;
    }
    TEditedFile Result;
    Result.LocalFile = Utf8(Directory + F->Terminal->ChangeFileName(&CopyParam, FromUtf8(Name), osRemote, false));
    Result.RootDirectory = Utf8(Root);
    Result.RemoteDirectory = Utf8(RemoteDirectory);
    Result.OriginalName = Name;
    Result.ForceText = ForceText;
    Result.SetSourceTimestamp(UnixTimeOf(Modification));
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::FileExists(const std::string & Name)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TRemoteFile> File(F->Terminal->TryReadFile(UnixCombinePaths(F->Terminal->CurrentDirectory, FromUtf8(Name))));
    return (File.get() != NULL) && !File->IsDirectory;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TEditedFile TSession::EditNew(const std::string & Name)
{
  try
  {
    CheckActive(*this);
    TCopyParamType CopyParam = TemporaryFileCopyParam(true);
    const UnicodeString RemoteDirectory = F->Terminal->CurrentDirectory;
    UnicodeString Root;
    const UnicodeString Directory = TemporaryEditDirectory(F->Terminal.get(), CopyParam, Root);
    const UnicodeString LocalFile = Directory + F->Terminal->ChangeFileName(&CopyParam, FromUtf8(Name), osRemote, false);
    if (!::FileExists(ApiPath(LocalFile)))
    {
      THandle File = FileCreate(ApiPath(LocalFile));
      if (File == reinterpret_cast<THandle>(INVALID_HANDLE_VALUE))
      {
        if (!Root.IsEmpty())
        {
          RecursiveDeleteFile(ExcludeTrailingBackslash(Root));
        }
        throw Exception(FMTLOAD(CREATE_FILE_ERROR, (LocalFile)));
      }
      FileClose(File);
    }
    TEditedFile Result;
    Result.LocalFile = Utf8(LocalFile);
    Result.RootDirectory = Utf8(Root);
    Result.RemoteDirectory = Utf8(RemoteDirectory);
    Result.OriginalName = Name;
    Result.ForceText = true;
    // No source timestamp: nothing to check as not modified.
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::EditReload(TEditedFile & File)
{
  try
  {
    CheckActive(*this);
    const UnicodeString RemoteFileName = UnixCombinePaths(FromUtf8(File.RemoteDirectory), FromUtf8(File.OriginalName));
    std::unique_ptr<TRemoteFile> Remote;
    F->Terminal->ExceptionOnFail = true;
    try
    {
      Remote.reset(F->Terminal->ReadFile(RemoteFileName));
    }
    catch (...)
    {
      F->Terminal->ExceptionOnFail = false;
      throw;
    }
    F->Terminal->ExceptionOnFail = false;
    if (!Remote->HaveFullFileName)
    {
      Remote->FullFileName = RemoteFileName;
    }
    std::unique_ptr<TStringList> Files(new TStringList());
    Files->AddObject(RemoteFileName, Remote.get());
    TCopyParamType CopyParam = TemporaryFileCopyParam(File.ForceText);
    {
      TTransferBracket Bracket(false, 1);
      F->Terminal->CopyToLocal(Files.get(), ExtractFilePath(FromUtf8(File.LocalFile)), &CopyParam,
        cpNoConfirmation | cpTemporary, NULL);
    }
    File.SetSourceTimestamp(UnixTimeOf(Remote->Modification));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TCustomScpExplorerForm::ExecutedFileChanged's upload.
bool TSession::EditUpload(TEditedFile & File, bool Background)
{
  try
  {
    CheckActive(*this);
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    const bool CheckNotModified = Shell->Settings().EditorCheckNotModified && (File.SourceTimestamp() != 0);
    const UnicodeString RemoteDirectory = FromUtf8(File.RemoteDirectory);
    const UnicodeString OriginalName = FromUtf8(File.OriginalName);
    if (CheckNotModified)
    {
      std::unique_ptr<TRemoteFile> Remote(F->Terminal->TryReadFile(UnixCombinePaths(RemoteDirectory, OriginalName)));
      if ((Remote.get() != NULL) && (UnixTimeOf(Remote->Modification) != File.SourceTimestamp()))
      {
        if (AskFrontend(MainInstructions(LoadStr(EDIT_CHANGED_EXTERNALLY)), NULL, qtConfirmation, qaOK | qaCancel, NULL) != qaOK)
        {
          return false;
        }
      }
    }
    std::unique_ptr<TStringList> Files(new TStringList());
    Files->Add(FromUtf8(File.LocalFile));
    TCopyParamType CopyParam = TemporaryFileCopyParam(File.ForceText);
    // "" copies the name as it is; otherwise the original's name.
    CopyParam.FileMask =
      (ExtractFileName(FromUtf8(File.LocalFile)) == OriginalName) ? UnicodeString() : DelimitFileNameMask(OriginalName);
    const int Params = cpNoConfirmation | cpTemporary;
    if (Background && F->Terminal->IsCapable[fcBackgroundTransfers])
    {
      F->EnsureQueue()->AddItem(new TEditorUploadQueueItem(F->Terminal.get(), Files.get(), RemoteDirectory, &CopyParam,
        Params, File, CheckNotModified));
      F->QueueAdded++;
      F->QueueChanged = true;
    }
    else
    {
      TTransferBracket Bracket(true, 1);
      F->Terminal->CopyToRemote(Files.get(), RemoteDirectory, &CopyParam, Params, NULL);
      if (CheckNotModified)
      {
        std::unique_ptr<TRemoteFile> Remote(F->Terminal->TryReadFile(UnixCombinePaths(RemoteDirectory, OriginalName)));
        if (Remote.get() != NULL)
        {
          File.SetSourceTimestamp(UnixTimeOf(Remote->Modification));
        }
      }
    }
    return true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// Commands > Open in PuTTY, as TTerminalManager::OpenInPutty: the session as
// it is now, through its tunnel's local port where it has one, in
// upstream's OpenSessionInPutty (the PuTTY slice, PuttySlice.h), its
// command's prompt patterns asked through Prompts.
void TSession::OpenInPutty(const TCustomCommandPrompts & Prompts)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    Configuration->Usage->Inc(L"OpenInPutty");
    std::unique_ptr<TSessionData> Data(CloneCurrentSessionData(Terminal));
    if (Terminal->TunnelLocalPortNumber != 0)
    {
      Data->ConfigureTunnel(Terminal->TunnelLocalPortNumber);
    }
    struct TPrompts
    {
      explicit TPrompts(const TCustomCommandPrompts & Prompts) { SetPuttyCommandPrompts(Prompts); }
      ~TPrompts() { SetPuttyCommandPrompts(TCustomCommandPrompts()); }
    } PromptsInUse(Prompts);
    OpenSessionInPutty(Data.get());
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::unique_ptr<TSession> TSession::Duplicate() const
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TSession> Result(new TSession());
    Result->F->Data.reset(CloneCurrentSessionData(F->Terminal.get()));
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Idle()
{
  if (F->Terminal == NULL)
  {
    return;
  }
  try
  {
    F->Terminal->Idle();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::vector<std::unique_ptr<TSession>> TSession::Workspace(const std::string & Name)
{
  try
  {
    std::unique_ptr<TObjectList> List(new TObjectList());
    StoredSessions->GetFolderOrWorkspace(FromUtf8(Name), List.get());
    std::vector<std::unique_ptr<TSession>> Result;
    for (int Index = 0; Index < List->Count; Index++)
    {
      std::unique_ptr<TSession> Session(new TSession());
      Session->F->Data.reset(new TSessionData(L""));
      Session->F->Data->Assign(static_cast<TSessionData *>(List->Items[Index]));
      Result.push_back(std::move(Session));
    }
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

namespace
{

// As TTerminalManager::SaveWorkspace: each tab's session data as it is, its
// local directory the tab's, a link to its stored site when it is one.
TObjectList * WorkspaceData(const std::vector<TSession::TWorkspaceTab> & Tabs,
  const std::function<TSessionData *(const TSession *)> & Clone)
{
  std::unique_ptr<TObjectList> Result(new TObjectList());
  for (std::size_t Index = 0; Index < Tabs.size(); Index++)
  {
    std::unique_ptr<TSessionData> Data;
    if (Tabs[Index].Session != nullptr)
    {
      Data.reset(Clone(Tabs[Index].Session));
    }
    else
    {
      // As TTerminalManager::NewLocalBrowser's.
      Data.reset(new TSessionData(UnicodeString()));
      Data->OtherLocalDirectory = FromUtf8(Tabs[Index].OtherLocalDirectory);
    }
    if (!Tabs[Index].LocalDirectory.empty())
    {
      Data->LocalDirectory = FromUtf8(Tabs[Index].LocalDirectory);
    }
    Result->Add(StoredSessions->SaveWorkspaceData(Data.get(), static_cast<int>(Index)));
  }
  return Result.release();
}

}  // namespace

void TSession::SaveWorkspace(const std::string & Name, const std::vector<TWorkspaceTab> & Tabs, bool SavePasswords)
{
  try
  {
    std::unique_ptr<TObjectList> DataList(WorkspaceData(Tabs, [](const TSession * Session)
    {
      return (Session->F->Terminal != NULL) ? CloneCurrentSessionData(Session->F->Terminal.get()) :
        Session->F->Data->Clone();
    }));
    // As DoSaveWorkspace.
    if (!SavePasswords)
    {
      for (int Index = 0; Index < DataList->Count; Index++)
      {
        TSessionData * Data = static_cast<TSessionData *>(DataList->Items[Index]);
        if (Data->Link.IsEmpty())
        {
          Data->ClearSessionPasswords();
        }
      }
    }
    StoredSessions->NewWorkspace(FromUtf8(Name), DataList.get());
    StoredSessions->Save(false, true);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::WorkspaceHasPasswords(const std::vector<TWorkspaceTab> & Tabs)
{
  try
  {
    std::unique_ptr<TObjectList> DataList(WorkspaceData(Tabs, [](const TSession * Session)
    {
      return (Session->F->Terminal != NULL) ? CloneCurrentSessionData(Session->F->Terminal.get()) :
        Session->F->Data->Clone();
    }));
    for (int Index = 0; Index < DataList->Count; Index++)
    {
      TSessionData * Data = static_cast<TSessionData *>(DataList->Items[Index]);
      if (Data->Link.IsEmpty() && Data->HasAnySessionPassword())
      {
        return true;
      }
    }
    return false;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}


// --- The Tabs and Commands menus' session commands (P3-14) -------------------

void TSession::Reconnect()
{
  try
  {
    if (F->Terminal == NULL)
    {
      Open();
      return;
    }
    if (!F->Terminal->Active)
    {
      F->Terminal->Reopen(0);
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::DropConnection()
{
  if (Active())
  {
    try
    {
      F->Terminal->Close();
    }
    catch (Exception &)
    {
    }
  }
}

bool TSession::CachesEmpty() const
{
  return !Active() || F->Terminal->AreCachesEmpty;
}

void TSession::ClearCaches()
{
  if (Active())
  {
    F->Terminal->ClearCaches();
  }
}

bool TSession::CanChangePassword() const
{
  return Active() && F->Terminal->IsCapable[fcChangePassword];
}

void TSession::ChangePassword()
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TSessionData> Data(F->Terminal->SessionData->Clone());
    Data->DisableAuthentationsExceptPassword();
    Data->ChangePassword = true;
    std::unique_ptr<TTerminal> Secondary(F->Terminal->CreateSecondarySession(L"ChangePassword", Data.get()));
    Secondary->AutoReadDirectory = false;
    Secondary->OnQueryUser = CLOSURE_OF(F.get(), QueryUser);
    Secondary->OnPromptUser = CLOSURE_OF(F.get(), PromptUser);
    Secondary->OnDisplayBanner = CLOSURE_OF(F.get(), DisplayBanner);
    Secondary->OnShowExtendedException = CLOSURE_OF(F.get(), ShowExtendedException);
    Secondary->OnInformation = CLOSURE_OF(F.get(), Information);
    Secondary->Open();
    Secondary->Close();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool TSession::CanUploadPublicKey() const
{
  // CanPrivateKeyUpload: "no nice way to assert SSH2".
  return Active() && (F->Terminal->FSProtocol == cfsSFTP);
}

std::string TSession::SshImplementation() const
{
  return Active() ? Utf8(F->Terminal->GetSessionInfo().SshImplementation) : std::string();
}

bool TSession::IsOpenSsh() const
{
  return ::IsOpenSSH(FromUtf8(SshImplementation()));
}

std::string TSession::PublicKeyFile() const
{
  return Utf8(F->Data->PublicKeyFile);
}

std::string TSession::UploadPublicKey(const std::string & FileName)
{
  try
  {
    CheckActive(*this);
    const UnicodeString Result = F->Terminal->UploadPublicKey(FromUtf8(FileName));
    F->Terminal->LogEvent(L"Public key installation done.");
    F->Terminal->Log->AddSeparator();
    return Utf8(Result);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSession::TSaveAsSite TSession::SaveAsSiteDefaults() const
{
  TSaveAsSite Result;
  std::unique_ptr<TSessionData> Data(Active() ? CloneCurrentSessionData(F->Terminal.get()) : F->Data->Clone());
  Result.Name = Utf8(Data->SessionName);
  // As DoSaveSession.
  const bool UseMasterPassword = false;
  Result.NotRecommended = !UseMasterPassword && !SameText(Data->UserName, AnonymousUserName);
  Result.CanSavePassword = !Configuration->DisablePasswordStoring && Data->HasAnySessionPassword();
  if (Result.CanSavePassword)
  {
    TSessionData * Original = StoredSessions->FindSame(Data.get());
    Result.SavePassword = ((Original != NULL) && Original->HasAnySessionPassword()) || !Result.NotRecommended;
  }
  std::unique_ptr<TStringList> Folders(new TStringList());
  for (int Index = 0; Index < StoredSessions->Count; Index++)
  {
    TSessionData * Site = StoredSessions->Sessions[Index];
    if (!Site->Special && !Site->IsWorkspace)
    {
      const UnicodeString Folder = Site->FolderName;
      if (!Folder.IsEmpty() && (Folders->IndexOf(Folder) < 0))
      {
        Folders->Add(Folder);
      }
    }
  }
  Folders->Sort();
  for (int Index = 0; Index < Folders->Count; Index++)
  {
    Result.Folders.push_back(Utf8(Folders->Strings[Index]));
  }
  return Result;
}

void TSession::SaveAsSite(const std::string & Name, bool SavePassword)
{
  try
  {
    const UnicodeString AName = FromUtf8(Name);
    // TSaveSessionDialog::DoValidate, SessionNameValidate.
    TSessionData::ValidateName(TSessionData::ExtractLocalName(AName));
    TSessionData::ValidatePath(AName);
    TSessionData * Existing = static_cast<TSessionData *>(StoredSessions->FindByName(AName));
    if ((Existing != NULL) && Existing->Special)
    {
      throw Exception(FMTLOAD(CANNOT_OVERWRITE_SPECIAL_SESSION, (AName)));
    }
    const UnicodeString Folder = TSessionData::ExtractFolderName(AName);
    if (!Folder.IsEmpty() && StoredSessions->IsWorkspace(Folder))
    {
      throw Exception(FMTLOAD(WORKSPACE_NOT_FOLDER, (Folder)));
    }
    std::unique_ptr<TSessionData> Data(Active() ? CloneCurrentSessionData(F->Terminal.get()) : F->Data->Clone());
    if (!SavePassword)
    {
      Data->ClearSessionPasswords();
    }
    StoredSessions->NewSession(AName, Data.get());
    // Modified only, explicit.
    StoredSessions->Save(false, true);
    if (!Data->HostKey.IsEmpty())
    {
      Data->CacheHostKeyIfNotCached();
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::Rename(const std::string & Name)
{
  try
  {
    const UnicodeString AName = FromUtf8(Name);
    TSessionData::ValidateName(AName);
    F->Data->Name = AName;
    if (F->Terminal != NULL)
    {
      F->Terminal->SessionData->Name = AName;
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}
