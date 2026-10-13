// What the Qt shell and core ask of each other, in types both sides share:
// plain C++ and UTF-8. Qt's translation units never include the VCL shim, and
// core's never include Qt -- the two disagree on too much (wchar_t's width on
// Linux, Win32 names, macros) to meet in one file. CoreBridge.cpp,
// QtInterface.cpp and Session.cpp are the VCL side. ADR 0012.
//
// Core runs on the GUI thread, as in WinSCP: a long operation calls back into
// the event loop (IFrontend::ProcessEvents) and asks its questions as modal
// dialogs from within the operation.
#ifndef LINSCP_QT_CORE_BRIDGE_H
#define LINSCP_QT_CORE_BRIDGE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace CoreBridge
{

// --- What core asks the shell -----------------------------------------------

// core/Interface.h's answers (qaYes, ...), whose values the bridge keeps.
enum : unsigned int
{
  AnswerYes = 0x0001, AnswerNo = 0x0004, AnswerOK = 0x0008, AnswerCancel = 0x0010,
  AnswerYesToAll = 0x0020, AnswerNoToAll = 0x0040, AnswerAbort = 0x0080, AnswerRetry = 0x0100,
  AnswerIgnore = 0x0200, AnswerSkip = 0x0400, AnswerAll = 0x0800, AnswerHelp = 0x1000,
  AnswerReport = 0x2000,
};

enum class TQueryType { Confirmation, Warning, Error, Information };

struct TQueryButton
{
  unsigned int Answer = 0;
  // Upstream's caption, '&' marking the accelerator, as Qt has it too.
  std::string Caption;
  // A button that acts before it answers (upstream's OnSubmit: "Copy key"):
  // returns the answer to close with, or 0 to keep the dialog open.
  std::function<unsigned int()> OnSubmit;
};

struct TQuery
{
  TQueryType Type = TQueryType::Confirmation;
  std::string Message;
  // Details, shown apart (upstream's MoreMessages).
  std::string MoreMessages;
  std::vector<TQueryButton> Buttons;
  // The answer Escape and closing the window give.
  unsigned int CancelAnswer = 0;
  // Offer "Never ask me again" (upstream's qpNeverAskAgainCheck).
  bool NeverAskAgainCheck = false;
  // Answered by itself after Timeout milliseconds, when not 0.
  unsigned int Timeout = 0;
  unsigned int TimeoutAnswer = 0;
};

struct TQueryResult
{
  unsigned int Answer = 0;
  bool NeverAskAgain = false;
};

// core/Interface.h's TPromptKind, in its order.
enum class TPromptKind
{
  Prompt, FileName, UserName, Passphrase, TIS, CryptoCard, KeybInteractive, Password, NewPassword, ProxyAuth
};

struct TPromptField
{
  std::string Label;
  bool Echo = true;
  // In: the value to start from. Out: the answer.
  std::string Value;
};

struct TPrompt
{
  TPromptKind Kind = TPromptKind::Prompt;
  // The dialog's title (upstream's Name) and the text above the fields.
  std::string Name;
  std::string Instructions;
  std::vector<TPromptField> Fields;
};

struct TError
{
  std::string Message;
  std::string MoreMessages;
  // upstream's help keyword, when the error has one
  std::string HelpKeyword;
};

// A transfer's progress, as core reports it (TFileOperationProgressType).
struct TTransferProgress
{
  bool Upload = true;
  // The file being transferred, and how far along it is.
  std::string FileName;
  int FilePercent = 0;
  // The whole operation: percent of the files (or of the bytes, once core
  // knows the total), bytes so far and in all (-1 not known yet).
  int OverallPercent = 0;
  std::int64_t Transferred = 0;
  std::int64_t Total = -1;
  int FilesDone = 0;
  int FileCount = 0;
  // Bytes per second.
  unsigned int Speed = 0;
};

// The shell's side, implemented on Qt (port/qt/QtFrontend.cpp). Every call
// comes on the GUI thread.
class IFrontend
{
public:
  virtual ~IFrontend() = default;

  virtual TQueryResult Query(const TQuery & Query) = 0;
  // Fills in the fields' values; false when the user cancels.
  virtual bool Prompt(TPrompt & Prompt) = 0;
  virtual void ShowError(const TError & Error) = 0;
  // The server's pre-authentication banner.
  virtual void Banner(const std::string & SessionName, const std::string & Banner,
    bool CanNeverShowAgain, bool & NeverShowAgain) = 0;
  // A line of progress while connecting ("Authenticating...").
  virtual void Information(const std::string & Text) = 0;

  // A transfer starting and ending (around its progress), for the shell's
  // progress window.
  virtual void TransferStarted(bool Upload, int FileCount) = 0;
  // false to cancel it.
  virtual bool TransferProgress(const TTransferProgress & Progress) = 0;
  virtual void TransferFinished() = 0;

  virtual void BusyStart() = 0;
  virtual void BusyEnd() = 0;
  // Runs the event loop's pending work, so the window stays alive while a
  // core operation blocks the GUI thread.
  virtual void ProcessEvents() = 0;
  virtual void SetClipboard(const std::string & Text) = 0;
  virtual bool GetClipboard(std::string & Text) = 0;
};

// --- Core's lifetime -----------------------------------------------------------

// What core reported, as an exception the shell can show in full. Aborted:
// the user gave up (cancelled a prompt), which is not to be shown.
class TCoreError : public std::runtime_error
{
public:
  TCoreError(const TError & Error, bool Aborted) :
    std::runtime_error(Error.Message), Error(Error), Aborted(Aborted) {}
  TError Error;
  bool Aborted;
};

// Starts core as upstream's WinSCP.exe does, with this process's command
// line, and the shell as its frontend (kept, not owned). Throws TCoreError.
void Initialize(IFrontend * Frontend);
// Saves the configuration and stops core.
void Finalize();

// Core's texts in a language: WinSCP's own translation (its translators'
// work, translations/<LANG>.ini upstream, generated into
// translations/linscp.<LANG> beside the executable by gen_translations.py).
// Locale is a BCP 47 name ("pt-BR", "de", "zh-TW"; "_" also accepted), mapped
// to WinSCP's language codes as upstream maps a Windows locale: the
// language-and-country code first, then the language's. Returns the code in
// use ("PT"), or "" for English: asked for, or no translation found. Call
// before Initialize, so that core starts in it. Directory: where the
// translations are, when not beside the executable (tests).
std::string SetLanguage(const std::string & Locale, const std::string & Directory = std::string());

// Texts of WinSCP's own string tables that the shell's WinSCP-like dialogs
// show, in core's language: WinSCP's translations serve them, in WinSCP's
// wording. "%s" and "%d" are core's placeholders.
enum class TText
{
  NewSiteNode,          // "New Site"
  SaveSiteCaption,      // "Save session as site"
  SaveSitePrompt,       // "&Site name:"
  SaveSiteFolder,       // "&Folder:"
  SaveSitePassword,     // "Save &password (not recommended)"
  ConfirmOverwriteSite, // "Site with name '%s' already exists. Overwrite?"
  ConfirmDeleteSite,    // "Are you sure you want to delete site '%s'?"
  FtpsImplicit,         // "TLS/SSL Implicit encryption"
  FtpsExplicit,         // "TLS/SSL Explicit encryption"
  NewFolderCaption,     // "Create site folder"
  NewFolderPrompt,      // "New &folder name:"
  ConfirmDeleteFolder,  // "Are you sure you want to delete site folder '%s' with %d site(s)?"
  ConfirmDeleteFile,    // "Are you sure you want to delete file '%s'?"
  ConfirmDeleteFiles,   // "Are you sure you want to delete %d selected files?"
  NewFolderName,        // "New folder"
  ConfirmRecycleFile,   // "Are you sure you want to move file '%s' to recycle bin?"
  ConfirmRecycleFiles,  // "Are you sure you want to move %d selected files to recycle bin?"
  ChangeDirError,       // "Error changing directory to '%s'."
  CreateDirError,       // "Error creating folder '%s'."
  RenameFileError,      // "Error renaming file '%s' to '%s'."
  DeleteLocalFileError, // "Error deleting file '%s'."
  FileInfoFormat,       // "%s of %s in %s of %s"
  FileInfoHidden,       // "%s hidden"
};
std::string Text(TText Text);

// Whether a file matches a file mask (core's TFileMasks syntax: "*.txt;
// *.log | old/", size and time conditions...), as WinSCP's panels match
// one when selecting (TCustomScpExplorerForm::DirViewMatchMask, without
// implicit matches). ModifiedTime is in seconds since 1970. Throws
// TCoreError when the mask is not one.
bool MaskMatches(const std::string & Masks, const std::string & FileName, bool Directory,
  std::int64_t Size, double ModifiedTime);

struct TInfo
{
  std::string AppName;
  std::string Version;
  // Where the configuration is kept: an INI file or the registry.
  std::string ConfigurationStorage;
};
TInfo Info();

// --- A session ---------------------------------------------------------------

struct TRemoteEntry
{
  std::string Name;
  bool Directory = false;
  std::int64_t Size = 0;
  // As core formats it for display, and as seconds since 1970 for sorting.
  std::string Modified;
  double ModifiedTime = 0;
  std::string Rights;
  std::string Owner;
  std::string Group;
  // A symbolic link, and where it points.
  bool Link = false;
  std::string LinkTarget;
  // Hidden as WinSCP's "Show hidden files" means it: a dot file remotely; a
  // hidden file locally.
  bool Hidden = false;
};

// --- Stored sites -------------------------------------------------------------

// A site as WinSCP's Login dialog edits it: its protocol and encryption, the
// server, the credentials, the directory to start in, and the advanced
// settings the shell offers (upstream's Site Advanced dialog: directories,
// connection, proxy, SSH authentication). Everything else a stored site
// holds is kept as it was when the site is saved again.
struct TSite
{
  enum class TProtocol { Sftp, Scp, Ftp, WebDav, S3 };
  // FTP: implicit TLS (ftps://) or explicit (ftpes://); WebDAV and S3: TLS
  // or not (Implicit = TLS).
  enum class TEncryption { None, Implicit, Explicit };
  // core's TProxyMethod, those the dialog offers.
  enum class TProxy { None, Socks4, Socks5, Http };

  // The site's path in the tree: "Folder/Subfolder/Site".
  std::string Name;
  TProtocol Protocol = TProtocol::Sftp;
  TEncryption Encryption = TEncryption::None;
  std::string HostName;
  // 0: the protocol's default port.
  int Port = 0;
  std::string UserName;
  std::string Password;
  std::string RemoteDirectory;

  // Directories.
  std::string LocalDirectory;
  // "Remember last used directory" (UpdateDirectories): on by default, as
  // WinSCP's.
  bool UpdateDirectories = true;
  // Connection: seconds to wait for the server; FTP's passive mode.
  int Timeout = 15;
  bool FtpPassive = true;
  // Proxy.
  TProxy Proxy = TProxy::None;
  std::string ProxyHost;
  int ProxyPort = 0;
  std::string ProxyUserName;
  std::string ProxyPassword;
  // "Consult proxy for local addresses" (core's ProxyLocalhost).
  bool ProxyLocalhost = false;
  // SSH authentication.
  std::string PrivateKeyFile;
  bool TryAgent = true;
  bool KeyboardInteractive = true;
  bool AgentForwarding = false;
};

// --- Private keys -----------------------------------------------------------

// A private key file as upstream's Login dialog checks it (windows/Tools.cpp's
// DoVerifyKey): usable; convertible, an OpenSSH or ssh.com key PuTTY's code
// cannot use as it is; or not usable, why in core's words.
struct TKeyCheck
{
  enum class TStatus { Usable, Convertible, Unusable } Status = TStatus::Usable;
  // For Convertible, core's question (KEY_TYPE_CONVERT4); for Unusable, its
  // message.
  std::string Message;
  // For Convertible: where upstream suggests saving it (the .ppk beside it).
  std::string ConvertedFileName;
};
TKeyCheck CheckKey(const std::string & FileName);
// Converts it to PuTTY's format as upstream's ConvertKey does: its
// passphrase asked for through the frontend when it has one, a matching
// certificate added, saved to Target with the same passphrase. Returns core's
// message saying so. Throws TCoreError (Aborted when the passphrase prompt is
// cancelled).
std::string ConvertKey(const std::string & FileName, const std::string & Target);

// The protocol's usual port, as core's default for it.
int DefaultPort(TSite::TProtocol Protocol, TSite::TEncryption Encryption);
// The stored sites' names, sorted as core keeps them; workspaces left out.
std::vector<std::string> SiteNames();
// A new site, from core's default settings. Throws TCoreError.
TSite NewSite();
// Throws TCoreError when there is no such site.
TSite LoadSite(const std::string & Name);
bool SiteExists(const std::string & Name);
// Saves Site under Site.Name, as upstream's Login dialog does: its other
// settings taken from the site BaseName (empty: a new site, from the default
// settings), which stays as it is -- saving under another name is "save as".
// As upstream's SessionNameValidate, a special site is not overwritten;
// asking before overwriting another is the dialog's (SiteExists). Throws
// TCoreError: no name, a special site, a storage error.
void SaveSite(const TSite & Site, const std::string & BaseName = std::string());
// As the Login dialog renames a site in its tree: saved under the new name,
// the old one removed. Throws TCoreError.
void RenameSite(const std::string & Name, const std::string & NewName);
void DeleteSite(const std::string & Name);

// A background transfer of the session's queue, as WinSCP's queue panel
// lists it.
struct TQueueEntry
{
  // Stable while the entry is listed; what the queue's actions take.
  std::uintptr_t Id = 0;
  bool Upload = true;
  bool Move = false;
  std::string Source;
  std::string Destination;
  enum class TState { Pending, Connecting, Processing, Prompt, Query, Error, Paused, Done } State = TState::Pending;
  // While it runs: the file, how far along the whole item is, its speed.
  std::string FileName;
  int Percent = 0;
  std::int64_t Transferred = 0;
  unsigned int Speed = 0;
};

// One connection, as WinSCP's "open" command takes it: a URL
// (sftp://user@host:port/path, ftp://, ftps://, ftpes://, dav://, davs://,
// s3://, scp://). Prompts and errors reach the shell through IFrontend while
// Open runs.
// --- Synchronization -------------------------------------------------------

// As WinSCP's Synchronize dialog sets it (TFullSynchronizeDialog's params):
// which side is brought up to date, how, and what tells files apart.
struct TSyncParams
{
  // As TTerminal::TSynchronizeMode: the remote directory updated from the
  // local one, the local from the remote, or both from each other.
  enum class TDirection { Remote, Local, Both };
  enum class TMode { Files, Mirror, Timestamps };
  TDirection Direction = TDirection::Remote;
  TMode Mode = TMode::Files;
  bool Delete = false;
  bool ExistingOnly = false;
  bool PreviewChanges = true;
  bool ByTime = true;
  bool BySize = false;
  bool CaseSensitive = false;
};

// An entry of the synchronization checklist (TSynchronizeChecklist::TItem):
// what is to be done, and the file on each side.
struct TSyncItem
{
  enum class TAction { None, UploadNew, DownloadNew, UploadUpdate, DownloadUpdate, DeleteRemote, DeleteLocal };
  TAction Action = TAction::None;
  bool Checked = true;
  bool Directory = false;
  std::string Name;
  // Each side's directory; its size and time only where the file is.
  std::string LocalDirectory;
  std::string RemoteDirectory;
  bool LocalExists = false;
  bool RemoteExists = false;
  std::int64_t LocalSize = 0;
  std::int64_t RemoteSize = 0;
  // As core formats it, and as seconds since 1970.
  std::string LocalModified;
  std::string RemoteModified;
  double LocalModifiedTime = 0;
  double RemoteModifiedTime = 0;
};

// What synchronizing found, before it is applied: the checklist WinSCP shows
// for "preview changes", whose entries can be unchecked or reversed.
class TSyncChecklist
{
public:
  ~TSyncChecklist();
  TSyncChecklist(const TSyncChecklist &) = delete;
  TSyncChecklist & operator=(const TSyncChecklist &) = delete;

  const std::vector<TSyncItem> & Items() const { return FItems; }
  void Check(std::size_t Index, bool Checked);
  // The opposite action (TSynchronizeChecklist::Reverse): an upload becomes
  // a download, a delete a transfer back.
  void Reverse(std::size_t Index);
  int CheckedCount() const;

private:
  friend class TSession;
  struct TImpl;
  explicit TSyncChecklist(TImpl * Impl);
  void Load();
  std::unique_ptr<TImpl> F;
  std::vector<TSyncItem> FItems;
};

class TSession
{
public:
  // Throws TCoreError when the URL is not one. A stored site's name is one,
  // as for the script's open.
  explicit TSession(const std::string & Url);
  // A site as edited, saved or not.
  explicit TSession(const TSite & Site);
  ~TSession();
  TSession(const TSession &) = delete;
  TSession & operator=(const TSession &) = delete;

  std::string Name() const;
  // Connects and authenticates, asking through the frontend. Throws
  // TCoreError when that fails or the user gives up.
  void Open();
  bool Active() const;
  std::string CurrentDirectory() const;
  // The site's local directory, environment variables expanded; empty when
  // it has none.
  std::string LocalDirectory() const;
  // The site's "remember last used directory".
  bool UpdateDirectories() const;
  // As WinSCP saves a session's directories when it ends
  // (TTerminalManager::SaveTerminal): into the stored site it was opened
  // from, when it remembers them, the current remote directory (as last
  // known, without asking the server) and LocalDirectory. False when
  // nothing was saved. Throws TCoreError.
  bool SaveDirectories(const std::string & LocalDirectory);
  // The current directory's entries, without "." and "..".
  std::vector<TRemoteEntry> List() const;
  // These throw TCoreError; what fails along the way is asked about first
  // (retry, skip, abort) through the frontend, as core does.
  void ChangeDirectory(const std::string & Directory);
  // The user's home directory, as the server reports it.
  void HomeDirectory();
  // Reads the current directory again.
  void Refresh();
  // In the current directory, or at a full path.
  void CreateDirectory(const std::string & Name);
  // An entry of the current directory, to a new name (or path).
  void Rename(const std::string & Name, const std::string & NewName);
  // Entries of the current directory into another remote directory (a full
  // path), keeping their names. False when not all moved.
  bool Move(const std::vector<std::string> & Names, const std::string & TargetDirectory);
  // Entries of the current directory. False when core skipped some.
  bool Delete(const std::vector<std::string> & Names);
  // Transfers with core's default transfer settings, as the GUI's Copy
  // without changes; Move deletes the source after. What already exists is
  // asked about (overwrite, skip, ...) through the frontend, as core does.
  // False when the transfer was cancelled or core skipped some. Throws
  // TCoreError.
  // Local files or directories (full paths) to a remote directory (or the
  // current one, when empty).
  bool Upload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move = false);
  // Entries of the current directory to a local directory.
  bool Download(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move = false);

  // The same in the background, through core's transfer queue (its own
  // connections, on its own threads), as WinSCP's "transfer in background".
  void QueueUpload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move = false);
  void QueueDownload(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move = false);

  // Synchronization, as WinSCP's Synchronize command: what differs between
  // a local and a remote directory and its subdirectories, collected into a
  // checklist (OnDirectory is told each pair compared; false from it
  // cancels, and nothing is returned); then applied, here or in the queue.
  // Throws TCoreError.
  std::unique_ptr<TSyncChecklist> SyncCollect(const std::string & LocalDirectory, const std::string & RemoteDirectory,
    const TSyncParams & Params,
    const std::function<bool(const std::string & Local, const std::string & Remote)> & OnDirectory = nullptr);
  // The checked entries. False when it was cancelled or core skipped some.
  bool SyncApply(TSyncChecklist & Checklist);
  // Each checked entry as its own queue item (TTerminal::SynchronizeToQueue).
  void SyncQueue(TSyncChecklist & Checklist);
  // Whether the server can set times, which "synchronize timestamps" needs.
  bool CanSyncTimestamps() const;
  // To be called regularly on the GUI thread (a timer): keeps the queue's
  // connections alive, and asks the questions its transfers wait on -- the
  // first one pending, through the frontend, as WinSCP pops them up. True
  // when the entries changed since the last call.
  bool QueueIdle();
  std::vector<TQueueEntry> QueueEntries() const;
  // How many entries finished (or were removed) since the session opened.
  int QueueDoneCount() const;
  bool QueueEmpty() const;
  // Removes an entry: a running transfer is cancelled. False when gone.
  bool QueueCancel(std::uintptr_t Id);
  bool QueuePause(std::uintptr_t Id);
  bool QueueResume(std::uintptr_t Id);
  void Close();

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

}

#endif
