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
#include <TextsCore.h>
#include <CopyParam.h>
#include <FileOperationProgress.h>
#include <Queue.h>
#include <GUIConfiguration.h>

#include "ShellConfiguration.h"
#include "TransferSettings.h"

#include <TextsWin.h>

#include <atomic>
#include <mutex>
#include <memory>

namespace CoreBridge
{

struct TSession::TImpl
{
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
  TTerminalQueue * EnsureQueue()
  {
    if (Queue == NULL)
    {
      Queue.reset(new TTerminalQueue(Terminal.get(), Configuration));
      // As TTerminalManager::NewQueue.
      Queue->TransfersLimit = Configuration->QueueTransfersLimit;
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

std::string TSession::LocalDirectory() const
{
  return Utf8(F->Data->LocalDirectoryExpanded);
}

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
      TRemoteEntry Entry;
      Entry.Name = Utf8(File->FileName);
      Entry.Directory = File->IsDirectory;
      Entry.Size = File->Size;
      Entry.Modified = Utf8(File->UserModificationStr);
      // TDateTime counts days from 1899-12-30; 1970 is day 25569.
      Entry.ModifiedTime = (static_cast<double>(File->Modification) - 25569.0) * 86400.0;
      Entry.Rights = Utf8(File->RightsStr);
      Entry.Owner = Utf8(File->Owner.DisplayText);
      Entry.Group = Utf8(File->Group.DisplayText);
      Entry.Link = File->IsSymLink;
      Entry.LinkTarget = Utf8(File->LinkTo);
      Entry.Hidden = (Entry.Name.size() > 1) && (Entry.Name[0] == '.');
      Result.push_back(Entry);
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
    FLAGMASK(Params.CaseSensitive, TTerminal::spCaseSensitive);
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
    F->Items.push_back(Item);
    FItems.push_back(Entry);
  }
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

std::string TSession::SessionKey() const
{
  return Active() ? Utf8(F->Terminal->SessionData->SessionKey) : std::string();
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
bool TSession::RemoteCopy(const std::vector<std::string> & Names, const std::string & Target, const std::string & FileMask,
  bool Direct)
{
  try
  {
    CheckActive(*this);
    TTerminal * Terminal = F->Terminal.get();
    std::unique_ptr<TStringList> Files = FileListOf(Terminal, Names);
    if (Direct)
    {
      return Terminal->CopyFiles(Files.get(), FromUtf8(Target), FromUtf8(FileMask), false);
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
          Terminal->CopyToLocal(Files.get(), Root, &DownParam, cpTemporary, NULL);
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
TEditedFile TSession::EditDownload(const std::string & Name, bool ForceText)
{
  try
  {
    CheckActive(*this);
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    const TShellSettings & Settings = Shell->Settings();
    TCopyParamType CopyParam = TemporaryFileCopyParam(ForceText);
    const UnicodeString RemoteDirectory = F->Terminal->CurrentDirectory;
    UnicodeString Root;
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
      Directory = IncludeTrailingBackslash(Directory + MakeValidFileName(F->Terminal->SessionData->SessionName));
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

}
