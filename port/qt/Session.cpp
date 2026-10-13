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
#include <TextsCore.h>
#include <CopyParam.h>
#include <FileOperationProgress.h>
#include <Queue.h>

#include <atomic>
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

}  // namespace

void TSession::HomeDirectory()
{
  try
  {
    CheckActive(*this);
    F->Terminal->HomeDirectory();
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

void TSession::CreateDirectory(const std::string & Name)
{
  try
  {
    CheckActive(*this);
    // Core reads the properties whatever is set in them: none here, the
    // server's defaults (as the GUI's Create folder with nothing ticked).
    TRemoteProperties Properties;
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

bool TSession::Upload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move)
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
    TCopyParamType CopyParam;
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
bool TSession::Download(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move)
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
    TCopyParamType CopyParam;
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

void TSession::QueueUpload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move)
{
  try
  {
    CheckActive(*this);
    std::unique_ptr<TStrings> Files(UploadList(LocalPaths));
    const UnicodeString Target = RemoteDirectory.empty() ? F->Terminal->CurrentDirectory : FromUtf8(RemoteDirectory);
    TCopyParamType CopyParam;
    F->EnsureQueue()->AddItem(new TUploadQueueItem(F->Terminal.get(), Files.get(),
      UnixIncludeTrailingBackslash(Target), &CopyParam, FLAGMASK(Move, cpDelete), false));
    F->QueueAdded++;
    F->QueueChanged = true;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void TSession::QueueDownload(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move)
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
    TCopyParamType CopyParam;
    F->EnsureQueue()->AddItem(new TDownloadQueueItem(F->Terminal.get(), Files.get(),
      IncludeTrailingBackslash(FromUtf8(LocalDirectory)), &CopyParam, FLAGMASK(Move, cpDelete), false));
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
    // As WinSCP's queue auto-popup: the first question waiting is asked.
    for (int Index = 0; Index < F->QueueStatus->Count; Index++)
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

}
