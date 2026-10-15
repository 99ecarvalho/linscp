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
#include <utility>
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
  // Its caption, when not upstream's default, and whether it starts
  // checked (TMessageParams' NeverAskAgainTitle and
  // NeverAskAgainCheckedInitially).
  std::string NeverAskAgainTitle;
  bool NeverAskAgainCheckedInitially = false;
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
  // Starts a program, not waiting for it, as upstream's ExecuteShell: its
  // parameters as one string, as Windows passes them (split into a shell's
  // words on POSIX), in Directory unless empty. False when it did not start.
  virtual bool StartProgram(const std::string & /*Program*/, const std::string & /*Params*/,
    const std::string & /*Directory*/)
  {
    return false;
  }
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
// The arguments core is to see, as upstream's GUI sees its command line
// (switches, and the session to open), instead of the process's: what Qt
// leaves of them. Before Initialize.
void SetArguments(const std::vector<std::string> & Arguments);
// Saves the configuration and stops core.
void Finalize();

// Core's language (WinSCP's translations, as upstream's GUI chooses and
// loads them: the configuration's, else the user interface's; ADR 0014).
// SetLanguage applies Locale for this run only, not saved (linscp's
// --lang): a POSIX or BCP 47 name ("pt_BR", "pt-BR", "de"), as Windows
// numbers it. Returns WinSCP's code of the translation in use ("PT"), or ""
// for English: asked for, or none found. Call after Initialize. Directory:
// the shell's Translations/, when the program is not the shell beside it
// (tests).
std::string SetLanguage(const std::string & Locale, const std::string & Directory = std::string());
// --- Transfer settings (P3-04b, ADR 0014) --------------------------------------

// WinSCP's transfer settings (core's TCopyParamType, with the GUI's own
// TGUICopyParamType additions), as its Transfer settings dialog edits them.
struct TTransferSettings
{
  enum class TMode { Binary, Text, Automatic };  // tmBinary, tmAscii, tmAutomatic
  TMode Mode = TMode::Binary;
  // The files transferred as text in automatic mode, a file mask.
  std::string TextMask;
  // ncNoChange, ncUpperCase, ncLowerCase, ncFirstUpperCase, ncLowerCaseShort.
  enum class TNameCase { NoChange, Upper, Lower, FirstUpper, LowerShort };
  TNameCase NameCase = TNameCase::NoChange;
  bool ReplaceInvalidChars = true;
  bool PreserveTime = true;
  bool PreserveTimeDirs = false;
  bool CalculateSize = true;
  // Bytes a second; 0 unlimited.
  unsigned long SpeedLimit = 0;
  // Downloads.
  bool PreserveReadOnly = false;
  // Uploads: the permissions to set (octal, "644"), with X added to
  // directories.
  bool PreserveRights = false;
  std::string Rights = "644";
  bool AddXToDirectories = true;
  bool IgnorePermErrors = false;
  bool ClearArchive = false;
  // "Remove BOM and EOF marks".
  bool RemoveBomAndEof = false;
  bool EncryptNewFiles = true;
  // Other.
  std::string IncludeFileMask;
  bool NewerOnly = false;
  bool ExcludeHiddenFiles = false;
  bool ExcludeEmptyDirectories = false;
  // Endurance: resume (and transfer to a temporary name) for all files,
  // files above the threshold (bytes), or none.
  enum class TResume { On, Smart, Off };
  TResume Resume = TResume::Smart;
  std::int64_t ResumeThreshold = 100 * 1024;
  // Background: transfer in the background by default, without asking;
  // several connections for one transfer.
  bool Queue = false;
  bool QueueNoConfirmation = true;
  bool QueueParallel = true;
};
// WinSCP's summary of them (TCopyParamType::GetInfoStr), as the Copy
// dialog shows it; what differs from the defaults.
std::string TransferSettingsInfo(const TTransferSettings & Settings);

// A transfer settings preset, as the Transfer preferences page lists them.
struct TTransferPreset
{
  std::string Name;
  TTransferSettings Settings;
};
// The default settings, the presets, and the one in use ("": the default).
TTransferSettings DefaultTransferSettings();
std::vector<TTransferPreset> TransferPresets();
std::string CurrentTransferPreset();
// What transfers use: the preset in use, else the default.
TTransferSettings CurrentTransferSettings();
// These save. A preset kept by name keeps its autoselection rule. Throw
// TCoreError (a preset name that is not one, for instance).
void SetDefaultTransferSettings(const TTransferSettings & Settings);
void SetTransferPresets(const std::vector<TTransferPreset> & Presets);
void SetCurrentTransferPreset(const std::string & Name);

// --- Editing (P3-05, ADR 0015) ---------------------------------------------------

// A remote file being edited: its temporary copy, where it came from, and
// what it was like on the server when opened (seconds since 1970; 0
// unknown), which an upload that went through the queue may update from the
// queue's thread.
class TEditedFile
{
public:
  TEditedFile();
  std::string LocalFile;
  // The temporary directory made for it, to remove once done; empty when it
  // is a shared one (TemporaryDirectoryDeterministic).
  std::string RootDirectory;
  std::string RemoteDirectory;
  std::string OriginalName;
  // Transferred as text (the internal editor's, and a text editor's).
  bool ForceText = true;
  double SourceTimestamp() const;
  void SetSourceTimestamp(double Timestamp);

private:
  struct TShared;
  std::shared_ptr<TShared> FShared;
};

// The internal editor's settings (TWinConfiguration's Editor), and
// EditorCheckNotModified.
struct TEditorSettings
{
  // Where the first editor opens (TEditorConfiguration's WindowParams, as
  // StoreForm writes it); empty: the system's place.
  std::string WindowParams;
  bool WordWrap = false;
  int TabSize = 8;
  // CP_UTF8 (WinSCP's default), else the system's code page.
  bool Utf8 = true;
  std::string FindText;
  std::string ReplaceText;
  bool FindMatchCase = false;
  bool FindWholeWord = false;
  bool CheckNotModified = false;
  // The font: an empty name the system's fixed one; points (0: its size);
  // bold, italic, underline, strike out. Colours as 0xRRGGBB, or -1 for the
  // defaults.
  std::string FontName;
  int FontSize = 0;
  bool FontBold = false;
  bool FontItalic = false;
  bool FontUnderline = false;
  bool FontStrikeOut = false;
  int FontColor = -1;
  int BackgroundColor = -1;
  // Asking before a file larger than LargeFileSize KB is opened.
  bool WarnOnLargeFile = true;
  int LargeFileSize = 10 * 1024;
};
TEditorSettings EditorSettings();

// An editor files open in, as WinSCP's editor preferences
// (TEditorPreferences): the internal editor, an external one (its command,
// with !.! for the file, else the file appended), or the file's associated
// application; for the files its mask matches.
struct TEditorDefinition
{
  enum class TKind { Internal, External, Open };
  TKind Kind = TKind::Internal;
  std::string Command;
  std::string Mask = "*.*";
  // Transfer as text; the editor opens each file in a process of its own
  // (SDI), or this is to be detected (MDI).
  bool Text = false;
  bool Sdi = false;
  bool DetectMdi = false;
  // As upstream names it (TEditorPreferences::Name): "Internal editor", the
  // command's program, or "Associated application". Read only.
  std::string Name;
};
// The editors, in order (WinConfiguration's EditorList); SetEditors saves.
std::vector<TEditorDefinition> Editors();
void SetEditors(const std::vector<TEditorDefinition> & Editors);
// The first editor whose mask matches the file (TEditorList::Find): its
// index, -1 for none. Local: a local file; its size and time (seconds since
// 1970) for the masks' size and time conditions.
int FindEditor(const std::vector<TEditorDefinition> & Editors, const std::string & FileName, bool Local,
  std::int64_t Size, double ModifiedTime);
// TEditorData::ExternalEditorOptionsAutodetect: the options a known editor
// (Notepad) needs.
void AutodetectEditorOptions(TEditorDefinition & Editor);
// Its name, as TEditorPreferences::Name.
std::string EditorName(const TEditorDefinition & Editor);
// An external editor started on FileName, as CustomExecuteFile starts one:
// the command's !.! the file, else the file appended. Throws TCoreError
// (EDITOR_ERROR) when it does not start.
void StartExternalEditor(const std::string & Command, const std::string & FileName);
// EXECUTE_FILE_ERROR: the associated application did not open FileName.
std::string ExecuteFileError(const std::string & FileName);
// Saves them.
void SetEditorSettings(const TEditorSettings & Settings);
// The system's code page as core names it (DefaultEncodingName: the ANSI
// code page on Windows, Latin-1 elsewhere), the internal editor's other
// encoding.
std::string DefaultEncodingName();

// --- Preferences (P3-04, ADR 0014) --------------------------------------------

// What the Preferences dialog sets, as WinSCP's (TWinConfiguration's
// settings, under its keys and with its defaults).
struct TPreferences
{
  // Panels.
  bool ShowHiddenFiles = false;
  // "Show file sizes in": bytes, kilobytes, or short (KB, MB, GB).
  enum class TSizeFormat { Bytes, Kilobytes, Short };
  TSizeFormat SizeFormat = TSizeFormat::Kilobytes;
  bool NaturalOrderNumericalSorting = true;
  bool AlwaysSortDirectoriesByName = false;
  // Rename in place selects the whole name, not the name without its
  // extension.
  bool RenameWholeName = false;
  // Home is the site's own directories, when the site does not remember
  // the last used ones.
  bool DefaultDirIsHome = true;
  // Window: the current directory in the title, short (its last part),
  // full, or none; the window kept when the last session closes.
  enum class TPathInCaption { Short, Full, None };
  TPathInCaption PathInCaption = TPathInCaption::Short;
  // The icon set (IconSet.h's id; empty: the platform's default).
  std::string IconSet;
  bool KeepOpenWhenNoSession = true;
  // File colors: the first mask a file matches colors it.
  struct TFileColor
  {
    std::string Mask;
    // 0xRRGGBB.
    unsigned Color = 0;
  };
  std::vector<TFileColor> FileColors;
  // Commander.
  bool SwappedPanels = false;
  // Explorer's keys (F5 refresh, Ctrl+K copy) rather than the Commander's.
  bool ExplorerKeyboardShortcuts = false;
  // Compare Directories: a file is newer, or of another size.
  bool CompareByTime = true;
  bool CompareBySize = false;
  // Open Directory as Location Profiles (a local and a remote directory
  // each) rather than the side's bookmarks.
  bool UseLocationProfiles = false;
  // Background (core's and the GUI's configuration; the transfer settings'
  // background options are TTransferSettings'): transfers at the same
  // time; how long completed ones stay listed (seconds; 0 never, -1
  // forever); questions of background transfers popped up when idle.
  int QueueTransfersLimit = 2;
  int QueueKeepDoneItemsFor = 15;
  bool QueueAutoPopup = true;
  // Endurance: reconnect a session that breaks during a transfer, while
  // idle or inactive, or stalls, after so many seconds (0: not), and keep
  // trying for so many (0: for ever).
  int SessionReopenAuto = 5;
  bool SessionReopenAutoIdleOn = true;
  int SessionReopenAutoIdle = 9;
  bool SessionReopenAutoInactive = false;
  int SessionReopenAutoStall = 60;
  int SessionReopenTimeout = 0;
  // Logging (core's configuration): the session log, at a level (0
  // reduced, 1 normal, 2 debug 1, 3 debug 2), to a file (patterns as
  // WinSCP's: !S the session...), appended or overwritten, passwords
  // included or not, rotated above a size (0: not) keeping so many (0: all);
  // the XML log.
  bool Logging = false;
  int LogLevel = 1;
  // Empty: none set; the dialog shows DefaultLogFileName, as WinSCP's.
  std::string LogFileName;
  std::string DefaultLogFileName;
  bool LogFileAppend = true;
  bool LogSensitive = false;
  std::int64_t LogMaxSize = 0;
  int LogMaxCount = 0;
  bool LogActions = false;
  std::string ActionsLogFileName;
  // Network: the external IP address for active FTP (empty: the operating
  // system's), the listening ports (0: any), knocking the FTP port when SFTP
  // is rejected.
  std::string ExternalIpAddress;
  int LocalPortNumberMin = 0;
  int LocalPortNumberMax = 0;
  bool TryFtpWhenSshFails = true;
  // Security: passwords remembered for the session; the agent (0 Pageant,
  // 1 OpenSSH's ssh-agent).
  bool SessionRememberPassword = true;
  // Integration > Applications (GUIConfiguration's): the program Open in
  // PuTTY runs, with WinSCP's patterns, and whether it is given the
  // password; FTP sessions opened in PuTTY as Telnet.
  std::string PuttyPath;
  bool PuttyPassword = false;
  bool TelnetForFtpInPutty = true;
  // Each new session opened in PuTTY too (WinConfiguration's).
  bool AutoOpenInPutty = false;
  // Where PuTTY keeps its sessions in the registry: PuTTY's or KiTTY's
  // (Configuration's PuttyRegistryStorageKey; Windows only).
  std::string PuttyRegistryStorageKey;
  int AuthAgent = 0;
  // Storage: where the configuration is (the registry, the automatic INI
  // file, a custom one; or a file given on the command line, or none), the
  // file in use, the custom one; the random seed file; where temporary
  // files go (empty: the system's temporary directory).
  enum class TStorage { Registry, AutomaticIni, CustomIni, Explicit, None };
  TStorage Storage = TStorage::AutomaticIni;
  std::string StorageFile;
  std::string CustomIniFile;
  std::string RandomSeedFile;
  std::string TemporaryDirectory;
};
// What a temporary directory goes under (Storage's, else the system's).
std::string TemporaryDirectoryBase();
// Sizes as WinSCP's dialogs type them ("1M", "100K", "10G"; core's
// TryStrToSize and SizeToStr); 0 for text that is not one.
std::int64_t SizeOfText(const std::string & Text);
std::string TextOfSize(std::int64_t Size);
TPreferences Preferences();

// The window's layout, kept between runs in WinSCP's own formats and keys
// (TWinConfiguration's ScpCommander and QueueView): the window's placement
// ("left;top;right;bottom;state;ppi"; empty, never placed), the local panel's
// share of the width, the active panel (0 local, 1 remote), each panel's
// columns (WinSCP's DirViewParams), the queue's height at its pixels per
// inch.
struct TLayout
{
  std::string WindowParams;
  double LocalPanelWidth = 0.5;
  int CurrentPanel = 0;
  std::string LocalDirViewParams;
  std::string RemoteDirViewParams;
  int QueueViewHeight = 140;
  int QueueViewHeightPixelsPerInch = 96;
  // Options > Queue (QueueView.Show, TQueueViewShow's order) and Options >
  // Status Bar (ScpCommander.StatusBar).
  enum class TQueueShow { Show, HideWhenEmpty, Hide };
  TQueueShow QueueShow = TQueueShow::HideWhenEmpty;
  bool StatusBar = true;
  // Options > Toolbars: locked, selective text labels, the icons' size
  // (LargerToolbar: 0 normal, 1 large, 2 very large); which bands show, as
  // WinSCP's ToolbarsLayout ("Name=<1 shown, 0 not>:<dock>:<row>+<pos>,"
  // per band; empty: WinSCP's default).
  bool LockToolbars = false;
  bool SelectiveToolbarText = true;
  int LargerToolbar = 1;
  std::string ToolbarsLayout;
  // The console's size (ConsoleWin.WindowSize: "width,height,ppi").
  std::string ConsoleWindowSize;
};
TLayout Layout();
// Saves it.
void SetLayout(const TLayout & Layout);
// Saves them, as the dialog's OK does (SaveExplicit).
void SetPreferences(const TPreferences & Preferences);

// A language there is a translation for, as the Languages page lists them
// (TGUIConfiguration::Locales). Completeness is a percentage; negative
// when the translation does not load.
struct TLocaleOption
{
  unsigned Locale = 0;
  std::string Name;
  int Completeness = 100;
};
std::vector<TLocaleOption> Locales();
// The language chosen: the one set, else the default one (the user
// interface's), as the page focuses it.
unsigned PreferredLocale();
// As the page's OK: the default one is saved as "none set" (0), so that a
// translation of the user interface's language found later is taken. Saves.
void SetPreferredLocale(unsigned Locale);
// The language in use now, which a change differs from until the next
// start.
unsigned AppliedLocale();
// The shell's window exists: from now on a language chosen applies on the
// next start, as upstream's (CanApplyLocaleImmediately).
void GuiStarted();

// The code of the translation in use, as SetLanguage returns it.
std::string LanguageCode();
// The locale core's texts are in, as BCP 47 names it ("pt-BR"; "en-US" for
// English): what the shell's and Qt's own texts follow.
std::string LanguageLocale();

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
  ConfirmDeleteWorkspace, // "Are you sure you want to delete workspace '%s' with %d session(s)?"
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
  CopyParamDefault,     // "&Default" (the default transfer settings)
  CopyParamDuplicate,   // "Transfer settings preset with description '%s' already exists."
  WinScpCopyright,      // "Copyright © 2000–2026 Martin Prikryl"
  PropertiesFile,       // "%d file"
  PropertiesFiles,      // "%d files"
  PropertiesDirectory,  // "%d folder"
  PropertiesDirectories, // "%d folders"
  PropertiesSymlink,    // "%d symbolic link"
  PropertiesSymlinks,   // "%d symbolic links"
  PropertiesFileCaption, // "%s Properties"
  PropertiesFilesCaption, // "%s, ... Properties"
  PropertiesUnknownSize, // "Unknown"
  PropertiesChecksumUnknown, // "Unknown"
  LinkAddCaption,       // "Add link/shortcut"
  LinkEditCaption,      // "Edit link/shortcut"
  CreateLinkError,      // "Cannot create link '%s'."
  RemoteCopyTitle,      // "Duplicate"
  RemoteMoveTitle,      // "Move"
  RemoteTransferPrompt, // "Target remote &path:"
  OpenDirectoryCaption, // "Open directory"
  ManageBookmarksCaption, // "Manage bookmarks"
  AddBookmarkConfirm,   // "Add directory '%s' to bookmarks?"
  AddBookmarkShared,    // "Add to &shared bookmarks"
  BookmarkInvalidName, // "'%s' is not valid Location Profile name."
  BookmarkFolderInvalidName, // "'%s' is not valid Location Profile folder name."
  DuplicateBookmarkFolder, // "Location Profile folder with name '%s' already exists."
  DeleteBookmarkFolder, // "Delete selected Location Profile folder?"
  AddBookmarkCaption, // "Add Location Profile"
  AddBookmarkPrompt, // "Location Profile name:"
  MoveBookmarkCaption, // "Move Location Profile"
  MoveBookmarkPrompt, // "New folder name:"
  BookmarkInfoTip, // "Local: %s\nRemote: %s"
  SiteRawCaption, // "Raw Site Settings"
  SiteRawAdd, // "&Add..."
  SiteRawAddCaption, // "Add Raw Site Setting"
  SiteRawAddLabel, // "&Site Setting:"
  ImportCaption, // "Import sites"
  ImportIniTitle, // "Select file to import sites from"
  CustomCommandAdHoc, // "Custom Command"
  CustomCommandAdd, // "Add Custom Command"
  CustomCommandEdit, // "Edit Custom Command"
  CustomCommandInvalid, // "Custom command description cannot contain '%s'."
  CustomCommandDuplicate, // "Custom command with description '%s' already exists."
  CustomCommandAdHocName, // "Ad Hoc"
  CustomCommandLast, // "&Last: %s"
  CustomCommandPatternsHint, // "Patterns: ..."
  CustomCommandPatternsHint6, // "!E ..."
  PatternsHintK, // "!K ..."
  PatternsHintBackslash, // "!\\ ..."
  ShortCutNone, // "None"
  ShortCutCaption, // "Select keyboard shortcut"
  ShortCutLabel, // "&Keyboard shortcut:"
  CustomCommandsParamsTitle, // "'%s' command parameters"
  CustomCommandsParamPrompt, // "&Parameter value:"
  FindFileTitle, // "Find"
  FindFileFinding, // "Finding ..."
  FindFileInDirectory, // "Searching in %s"
  FindFileDone, // "Done."
  FindFileAborted, // "Aborted."
  FindFileStart, // "&Start"
  FindFileStop, // "&Stop"
  SelectMaskSelectCaption, // "Select"
  SelectMaskDeselectCaption, // "Deselect"
  FilterMaskCaption, // "Filter"
  CopyLocalCopyCaption, // "Copy"
  CopyLocalMoveCaption, // "Move"
  CreateLocalDirectory, // "Directory '%s' doesn't exist. Create?"
  CreateLocalDirectoryError, // "Can't create folder '%s'."
  SelectLocalDirectory, // "Select local directory."
  FileOverwrite, // "File '%s' already exists. Overwrite?"
  SaveSyncMode, // "Do you want to use the synchronization direction ... next time too?" (SAVE_SYNCHRONIZE_MODE2)
  SaveSessionCaption, // "Save session as site"
  SaveSessionPrompt, // "&Site name:"
  SaveSessionFolder, // "&Folder:"
  SaveSessionRootFolder, // "<none>"
  SaveSessionPassword, // "Save &password (not recommended)"
  SaveSessionPasswordRecommended, // "Save &password"
  RenameTabTitle, // "Rename tab"
  RenameTabPrompt, // "&New tab name:"
  PasswordChanged, // "Password has been changed."
  PublicKeyTitle, // "Select key to install into server"
  NotOpenSsh, // "**Install public key to non-OpenSSH server?**..." (%s: the server's)
  CompareNoDifferences, // "No differences found."
  SyncBrowseCreate, // "**Do you want to try to create directory '%s'?**..."
  SyncBrowseError, // "Cannot open corresponding directory in the opposite panel. ..."
  SyncBrowseToggle, // "Synchronized browsing was turned %s.|on|off"
  NewFileName, // "New file"
  EditFileCaption, // "Edit file"
  EditFilePrompt, // "&Enter file name:"
  CreateFileError, // "Can't create file '%s'."
  UnsafeSession, // "**Use potentially unsafe session settings?**..."
  MaskHint, // "* matches any number of characters. ..." (MASK_HINT2)
  FileMaskExHint, // ">size matches file larger than size ..."
  CombiningMasksHint, // "Masks are separated by semicolon or comma. ..."
  PathMaskHint, // "Mask can be extended with path mask. ..." (PATH_MASK_HINT2)
  DirectoryMaskHint, // "Mask ending with slash selects directories."
  NotSupported, // "Operation not supported."
  LicenseCaption, // "%s license"
  TagAdd,         // "Add tag"
  TagEdit,        // "Edit tag"
  TagKey,         // "&Key:"
  TagValue,       // "&Value (optional):"
  TagNotUnique,   // "Keys must be unique."
  TagInvalidChar, // "Character \"%s\" is not allowed in tags."
  EditorLargeFile, // "**Try to open large file?**\n\nThe file ... is too large (%s). ..."
  EditorAdd,      // "Add editor"
  EditorEdit,     // "Edit editor"
  EditorAdHoc,    // "Editor"
  SelectExternalEditor, // "Select editor application."
  ConfirmRegisterUrl, // "Do you want to register WinSCP to handle URL addresses?"
  ConfirmUnregisterUrl, // "Do you want to unregister WinSCP from handling all URL addresses?"
  ConfirmAddSearchPath, // "Do you want to add application path '%s' to system search path (%%PATH%%)?"
  CreateDesktopIconAllUsers, // "**Do you want to create desktop icon for all users?**\n \nYou must have administrator privileges for this."
  RegisterUrlError, // "Cannot register application to handle URL addresses."
  CreateShortcutError, // "Cannot create shortcut."
  SaveWorkspaceCaption, // "Save workspace as"
  SaveWorkspacePrompt, // "&Save workspace as:"
  SaveWorkspacePasswords, // "Save &passwords (not recommended)"
  SaveWorkspacePasswordsRecommended, // "Save &passwords"
  SaveWorkspaceAuto, // "Enable &automatic saving of this workspace"
  NewWorkspace, // "My Workspace"
};
std::string Text(TText Text);

// The PuTTY command, as the Applications page checks it
// (TPreferencesDialog::UpdateControls): whether it has the session's
// patterns, so PuTTY's options are not added, and the password's. Its
// patterns' hint (PUTTY_PATTERNS_HINT2 with !K's line).
void PuttyCommandPatterns(const std::string & Command, bool & SiteCommand, bool & PasswordCommand);
std::string PuttyPatternsHint();
// The registry keys PuTTY's sessions may be under: PuTTY's own and KiTTY's.
std::vector<std::string> PuttyRegistryStorageKeys();
// The custom commands list's L/R column (CUSTOM_COMMAND_LOCAL/REMOTE).
std::string CustomCommandSide(bool Local);

#if defined(_WIN32)
// Preferences > Integration on Windows, upstream's (Setup.cpp, sliced):
// the protocols' URLs handled by the program (RegisterForDefaultProtocols)
// or no longer; Windows' default apps settings opened, the program
// registered first (LaunchAdvancedAssociationUI); a directory added to the
// search path (AddSearchPath). Throw TCoreError.
void RegisterUrlHandler();
void UnregisterUrlHandler();
void MakeDefaultUrlHandler();
void AddToSearchPath(const std::string & Directory);
#endif

// Help > About's third-party list, as TAboutDialog::GenerateThirdParty
// writes it, in HTML: the translation in use, if any (LanguageName in place
// of its "{language}"), then the libraries linscp has of WinSCP's. Expat's
// license link is ExpatLicenseUrl, which the shell shows itself, as
// WinSCP's dialog does.
std::string ThirdPartyHtml(const std::string & LanguageName);
extern const char ExpatLicenseUrl[];

// Whether a file matches a file mask (core's TFileMasks syntax: "*.txt;
// *.log | old/", size and time conditions...), as WinSCP's panels match
// one when selecting (TCustomScpExplorerForm::DirViewMatchMask, without
// implicit matches). ModifiedTime is in seconds since 1970. Throws
// TCoreError when the mask is not one.
// AllowImplicit: a directory matches a mask with no directory part, as a
// panel's filter has it (TCustomDirView's Mask).
bool MaskMatches(const std::string & Masks, const std::string & FileName, bool Directory,
  std::int64_t Size, double ModifiedTime, bool AllowImplicit = false);
// A mask's parts, as the Edit mask dialog shows them (TFileMasks'
// IncludeFileMasksStr and the like), one mask each; put back together as
// TFileMasks::ComposeMaskStr. Throw TCoreError when the mask is not one.
struct TMaskParts
{
  std::vector<std::string> IncludeFiles;
  std::vector<std::string> ExcludeFiles;
  std::vector<std::string> IncludeDirectories;
  std::vector<std::string> ExcludeDirectories;
};
TMaskParts SplitMask(const std::string & Mask);
std::string ComposeMask(const TMaskParts & Parts);
// TFileMasks::NormalizeMask: "*.*" and the like as no mask ("").
std::string NormalizeMask(const std::string & Mask);
// A name through a transfer's file mask ("*.bak": the name with that
// extension), as core's MaskFileName.
std::string MaskFileName(const std::string & FileName, const std::string & Mask);
// The same against a full path (local or remote), as the panels' file
// colors match (TCustomScpExplorerForm::DirViewGetItemColor), so masks with
// directories work.
bool MaskMatchesPath(const std::string & Masks, const std::string & Path, bool Local, bool Directory,
  std::int64_t Size, double ModifiedTime);

struct TInfo
{
  std::string AppName;
  // linscp's version, and WinSCP's, which core is.
  std::string Version;
  std::string WinScpVersion;
  // Where the configuration is kept: an INI file or the registry.
  std::string ConfigurationStorage;
};
TInfo Info();

// --- Server and protocol information (P3-06c) ---------------------------------

// What TFileSystemInfoDialog::Feed shows, its labels WinSCP's: the server's
// rows and the capabilities' (label, value), the host key's algorithm and
// fingerprints, the certificate's, and the protocol's additional
// information. Values empty are not shown.
struct TServerInfo
{
  std::vector<std::pair<std::string, std::string>> Server;
  std::vector<std::pair<std::string, std::string>> Capabilities;
  std::string HostKeyAlgorithm;
  std::string HostKeySha256;
  std::string HostKeyMd5;
  std::string CertificateSha256;
  std::string CertificateSha1;
  std::string Certificate;
  std::string AdditionalInfo;
  // The space available can be checked (fcCheckingSpaceAvailable).
  bool CanCheckSpace = false;
};
// Space available on the server (TSpaceAvailable): each with its label; 0
// unknown.
struct TSpaceInfo
{
  std::vector<std::pair<std::string, std::int64_t>> Items;
};

// --- Custom commands (P3-06d) --------------------------------------------------

// A custom command, as core's TCustomCommandType has it (its Params as
// flags; the others it has kept as they are).
struct TCustomCommandDefinition
{
  std::string Name;
  std::string Command;
  bool ApplyToDirectories = false;
  bool Recursive = false;
  bool Local = false;
  bool ShowResults = false;
  bool CopyResults = false;
  bool RemoteFiles = false;
  bool ShowResultsInMessageBox = false;
  int OtherParams = 0;
  // The VCL's TShortCut (0 none).
  unsigned ShortCut = 0;
  // From an extension (its options, if any, are not offered yet).
  std::string Id;
};
// Commands > Custom Commands: the list, as WinConfiguration keeps it (its
// defaults until changed); SetCustomCommands replaces and saves it.
std::vector<TCustomCommandDefinition> CustomCommands();
void SetCustomCommands(const std::vector<TCustomCommandDefinition> & Commands);
// What the Custom command dialog checks of a command, as
// TCustomCommandDialog's UpdateControls and FormCloseQuery: whether it works
// on files, which choices apply, and core's error when it is not a command.
struct TCustomCommandCheck
{
  bool FileCommand = false;
  bool AllowRecursive = true;
  bool AllowApplyToDirectories = true;
  bool AllowRemoteFiles = false;
  std::string Error;
};
TCustomCommandCheck CheckCustomCommand(const std::string & Command, bool Remote);
// The questions a command asks as it runs (TWinInteractiveCustomCommand):
// its !?prompt?default! patterns, all at once, false when cancelled; and
// what a !`command` pattern's command prints, run locally.
struct TCustomCommandPrompts
{
  std::function<bool(const std::string & Name, const std::vector<std::pair<std::string, std::string>> & Prompts,
    std::vector<std::string> & Values)> Prompt;
  std::function<std::string(const std::string & Command)> Execute;
};

// --- Generate URL and code (P3-06c) -------------------------------------------

// What the Generate URL dialog asks for (TGenerateUrlDialog): the session's
// URL (Components: its checkboxes' flags, WinSCP's tags), script or .NET
// code, or with Paths (full remote paths) each file's URL. As WinSCP's
// GenerateUrl* settings, kept between runs.
struct TGenerateCodeOptions
{
  enum class TTarget { Url, Script, Assembly };
  TTarget Target = TTarget::Url;
  // 1 user name, 2 password, 4 host key, 8 initial directory, 16
  // WinSCP-specific, 32 save extension, 64 advanced settings.
  int Components = 1 | 8;
  // sfScriptFile, sfBatchFile, sfCommandLine, sfPowerShell.
  int ScriptFormat = 0;
  // alCSharp, alVBNET, alPowerShell.
  int AssemblyLanguage = 0;
  std::vector<std::string> Paths;
};
TGenerateCodeOptions GenerateCodeSettings();
void SetGenerateCodeSettings(const TGenerateCodeOptions & Options);
// What it generated: RTF, as upstream's rich edit shows it (colours, links
// to WinSCP's documentation), with the dialog's captions and which
// components can be had.
struct TGeneratedCode
{
  std::string Rtf;
  std::string Description;
  std::string Title;
  std::string ResultCaption;
  bool WordWrap = true;
  bool FixedWidth = false;
  bool CanUserName = false;
  bool CanPassword = false;
  bool CanHostKey = false;
  bool CanRemoteDirectory = false;
  bool CanSaveExtension = false;
  bool CanRawSettings = false;
};

// --- Bookmarks (P3-06b) -------------------------------------------------------

// A bookmark, as core's TBookmark: its name, local and remote directories,
// folder (Node), and keyboard shortcut (the VCL's TShortCut: a virtual key
// with scShift 0x2000, scCtrl 0x4000, scAlt 0x8000; 0 none).
struct TBookmarkEntry
{
  std::string Name;
  std::string Local;
  std::string Remote;
  std::string Node;
  unsigned ShortCut = 0;
};
// A site's bookmarks, by its session key, or with an empty key the shared
// ones, as TWinConfiguration keeps them.
std::vector<TBookmarkEntry> Bookmarks(const std::string & SessionKey);
// Replaces them, and saves.
// OpenedNodes: the folders the Location Profiles dialog leaves expanded
// (TBookmarkList::NodeOpened); none given, those kept as they were.
void SetBookmarks(const std::string & SessionKey, const std::vector<TBookmarkEntry> & Bookmarks,
  const std::vector<std::string> * OpenedNodes = nullptr);
std::vector<std::string> BookmarkOpenedNodes(const std::string & SessionKey);
// Ask before a separate shell session opens (GUIConfiguration's
// ConfirmCommandSession).
bool ConfirmCommandSession();
void SetConfirmCommandSession(bool Confirm);
// Open directory's page: the shared bookmarks (WinConfiguration's
// UseSharedBookmarks).
bool UseSharedBookmarks();
void SetUseSharedBookmarks(bool Use);

// --- Properties (P3-06a) ------------------------------------------------------

// Permissions as core's TRights keeps them: the bits set and the bits unset
// (of 07777); a bit in neither is mixed, as several files' can be.
struct TRightsState
{
  unsigned Set = 0;
  unsigned Unset = 07777;
};

// As core's TRights writes them: "rwxr-xr-x", with '$' for a mixed bit.
std::string RightsText(const TRightsState & Rights);
// Core's reading of "rwxr-xr-x" or an octal "0755"/"755"; false when it is
// neither.
bool RightsFromText(const std::string & Text, TRightsState & Rights);

// What the Properties dialog changes, as core's TRemoteProperties. Owner
// and group are as the dialog shows them: a name, or with "by ID" servers
// "name [id]" (TPropertiesDialog::LoadRemoteToken).
struct TFileProperties
{
  bool HasRights = false;
  TRightsState Rights;
  bool AddXToDirectories = false;
  bool HasOwner = false;
  std::string Owner;
  bool HasGroup = false;
  std::string Group;
  bool Recursive = false;
  // S3's tags, key and value, where the protocol has them (fcTags).
  bool HasTags = false;
  std::vector<std::pair<std::string, std::string>> Tags;
};

// Core's file masks: whether a target's name part is a mask ("*.*", "*.bak")
// rather than a file name (IsFileNameMask).
bool IsFileNameMask(const std::string & Mask);
// WinSCP's question before several files go to one file Target
// (FormatMultiFilesToOneConfirmation), as a query's message.
std::string MultiFilesToOneConfirmation(const std::string & Target, bool Unix);

// The Create folder dialog's permissions, and as kept for next time
// (GUIConfiguration's NewDirectoryProperties).
struct TNewDirectory
{
  bool SetRights = false;
  TRightsState Rights;
};
TNewDirectory NewDirectorySettings();
// Saves them ("use same settings next time").
void SetNewDirectorySettings(const TNewDirectory & Settings);

// What the dialog shows of a selection, as TCustomScpExplorerForm's
// SetProperties gathers it.
struct TPropertiesInfo
{
  // What the files have in common.
  TFileProperties Properties;
  bool CanChangeRights = false;
  bool CanChangeOwner = false;
  bool CanChangeGroup = false;
  // The owners and groups to offer.
  std::vector<std::string> Owners;
  std::vector<std::string> Groups;
  int Files = 0;
  int Directories = 0;
  int SymLinks = 0;
  std::int64_t Size = 0;
  // A directory to recurse into: its size is to be calculated.
  bool CanCalculateSize = false;
  // Where a single link points.
  std::string LinkTarget;
  // The server's checksum algorithms; empty when it calculates none.
  std::vector<std::string> ChecksumAlgorithms;
  // The Tags page: a single file, where the protocol has tags (poTags).
  bool ShowTags = false;
};

// --- A session ---------------------------------------------------------------

struct TRemoteEntry
{
  std::string Name;
  bool Directory = false;
  std::int64_t Size = 0;
  // As core formats it for display, and as seconds since 1970 for sorting.
  std::string Modified;
  double ModifiedTime = 0;
  // What the time says (the listing's TModificationFmt): Compare
  // Directories compares to the coarser of two.
  enum class TPrecision { None, Day, Minute, Second };
  TPrecision ModifiedPrecision = TPrecision::Second;
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
  // Raw settings, once edited (Edit Raw Settings): the site's settings in
  // core's options ("Name=Value" lines), replacing the base's under the
  // fields above.
  bool HasRawSettings = false;
  std::string RawSettings;
};

// Cleanup application data, as WinSCP's (forms/Cleanup.cpp,
// TCleanupDialog): what the program keeps -- its configuration, sites,
// caches, INI file, random seed file, temporary folders -- and where.
class TCleanupData
{
public:
  struct TItem
  {
    std::string Caption;
    std::string Location;
  };
  // As FindData. Throws TCoreError.
  TCleanupData();
  ~TCleanupData();
  const std::vector<TItem> & Items() const { return FItems; }
  // Whether there is anything to clean up: the configuration is listed
  // regardless (cleaning it up keeps it from being saved), as WinSCP's.
  bool AnyData() const { return FAnyData; }
  // The item removed. Throws TCoreError.
  void Cleanup(std::size_t Index);

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
  std::vector<TItem> FItems;
  bool FAnyData = false;
};

// Import sites, as WinSCP's (forms/ImportSessions.cpp's
// DoImportSessionsDialog): the sites other programs keep, by source --
// PuTTY, KiTTY, FileZilla, OpenSSH's config, an INI file (WinSCP's), and
// OpenSSH's known_hosts (host keys) -- each with why it has none.
class TImportSites
{
public:
  enum TSource { Putty, Kitty, FileZilla, OpenSsh, Ini, KnownHosts, SourceCount };
  // Gathers them all. Throws TCoreError.
  TImportSites();
  ~TImportSites();
  std::string SourceName(int Source) const;
  std::string Error(int Source) const;
  // Each site's name and description (its info tip), as listed.
  std::vector<std::pair<std::string, std::string>> Sites(int Source) const;
  bool Selected(int Source, int Index) const;
  void Select(int Source, int Index, bool Selected);
  // Browse... (an INI file) and Paste (known_hosts lines): that source
  // read again from them.
  void LoadIni(const std::string & FileName);
  void LoadKnownHosts(const std::string & Text);
  // The source's selected sites imported (their host keys with them), as
  // TImportSessionsDialog::Execute and DoImportSessionsDialog; the names
  // given them. Throws TCoreError.
  std::vector<std::string> Import(int Source);

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
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
// What a text says as a session URL (TStoredSessionList::GetUrlInfo): a
// protocol given (it is one); settings that are potentially unsafe.
struct TUrlInfo
{
  bool ProtocolDefined = false;
  bool UnsafeSettings = false;
};
TUrlInfo UrlInfo(const std::string & Text);
// Saves Site under Site.Name, as upstream's Login dialog does: its other
// settings taken from the site BaseName (empty: a new site, from the default
// settings), which stays as it is -- saving under another name is "save as".
// As upstream's SessionNameValidate, a special site is not overwritten;
// asking before overwriting another is the dialog's (SiteExists). Throws
// TCoreError: no name, a special site, a storage error.
void SaveSite(const TSite & Site, const std::string & BaseName = std::string());
// Edit Raw Settings, as TSiteRawDialog: the site's settings (of BaseName's
// stored site, or the defaults, with Site's fields) as core's options
// other than the protocol, host, port, user and password; and, the options
// edited, the site they make (with HasRawSettings), as Execute applies
// them. Throws TCoreError.
std::string SiteRawSettings(const TSite & Site, const std::string & BaseName);
TSite ApplySiteRawSettings(const TSite & Site, const std::string & BaseName, const std::string & RawSettings);
// The options not yet among RawSettings, each with its default, sorted,
// for the dialog's Add (TSiteRawDialog::AddButtonClick).
std::vector<std::pair<std::string, std::string>> RawSettingsToAdd(const std::string & RawSettings);
// As the Login dialog renames a site in its tree: saved under the new name,
// the old one removed. Throws TCoreError.
void RenameSite(const std::string & Name, const std::string & NewName);
void DeleteSite(const std::string & Name);

// Workspaces, as WinSCP's (TStoredSessionList's): the tabs' sessions kept
// together under a name, each with its directories. Their names
// (GetWorkspaces); one removed with its sessions; a name's check as
// TSaveWorkspaceDialog::DoValidate does it (TCoreError when it is not one,
// or a site folder's).
std::vector<std::string> WorkspaceNames();
void DeleteWorkspace(const std::string & Name);
int WorkspaceSessionCount(const std::string & Name);
void ValidateWorkspaceName(const std::string & Name);
// What WinSCP keeps for them (TWinConfiguration's): the workspace saved
// automatically when the window closes, with passwords or not, and whether
// it was (LastStoredSession), so it opens at the next start.
struct TWorkspaceSettings
{
  bool AutoSave = false;
  bool AutoSavePasswords = false;
  std::string AutoWorkspace;
  std::string LastStoredSession;
};
TWorkspaceSettings WorkspaceSettings();
void SetWorkspaceSettings(const TWorkspaceSettings & Settings);

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
  // Selected files only (spSelectedOnly): the panels' selection, by name
  // (GetSynchronizeOptions' filter); by checksum (spByChecksum).
  bool SelectedOnly = false;
  std::vector<std::string> Selected;
  bool ByChecksum = false;
};
// The Synchronize dialog's options as kept by "Use same options next time"
// (GUIConfiguration's SynchronizeParams; SynchronizeModeAuto: the direction
// too, when SaveMode).
struct TSyncDefaults
{
  TSyncParams Params;
  bool SaveMode = false;
};
TSyncDefaults SyncDefaults();
void SetSyncDefaults(const TSyncParams & Params, bool SaveMode);

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
  // A file's size, or a directory's once calculated (HasSize).
  bool HasSize = true;
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
  // Find Move Candidate (TSynchronizeChecklistDialog's): from Index on, an
  // entry and the one it would be moved from or to -- a new one and the
  // opposite delete: directories of the name, files of the size with the
  // name or the directory. False when there is none.
  bool FindMoveCandidate(std::size_t Index, std::size_t & First, std::size_t & Second) const;
  // Whether Move applies to these: exactly a new entry and its opposite
  // delete of the same kind, or a new directory and deletes to move into it.
  bool CanMove(const std::vector<std::size_t> & Indexes) const;

private:
  friend class TSession;
  struct TImpl;
  explicit TSyncChecklist(TImpl * Impl);
  void Load();
  std::unique_ptr<TImpl> F;
  std::vector<TSyncItem> FItems;
};

// --- Keeping the remote directory up to date -------------------------------

// The Keep Remote Directory up to Date dialog's settings, as WinSCP's
// (TSynchronizeParamType: its spXXX params and soXXX options).
struct TKeepUpToDateParams
{
  std::string LocalDirectory;
  std::string RemoteDirectory;
  bool Delete = false;
  bool ExistingOnly = false;
  bool SelectedOnly = false;
  // "Update subdirectories" (soRecurse).
  bool Recurse = false;
  // "Synchronize on start": a full synchronization first, or (Ask: the box
  // grayed) the question whether to (SYNCHRONISE_BEFORE_KEEPUPTODATE2).
  enum class TSynchronizeFirst { No, Yes, Ask };
  TSynchronizeFirst SynchronizeFirst = TSynchronizeFirst::No;
  bool ContinueOnError = false;
};

// As TCustomScpExplorerForm::DoSynchronizeDirectories: the settings kept
// (GUIConfiguration's SynchronizeParams and SynchronizeOptions), with no
// directories; saved back with "use same options next time", or else only
// an Ask the question's "never ask again" made Yes.
TKeepUpToDateParams KeepUpToDateDefaults();
void SaveKeepUpToDateDefaults(const TKeepUpToDateParams & Params, bool SaveSettings);
// The synchronize-on-start question, as TSynchronizeDialog::Start asks it.
std::string KeepUpToDateSynchronizeQuestion();

// What keeping up to date logs (TSynchronizeLogEntry).
enum class TKeepUpToDateLog { Scan, Start, Change, Upload, Delete, DirChange, ContinuedError };

struct TKeepUpToDateEvents
{
  // A line of the dialog's log (TSynchronizeDialog::DoLogInternal); with
  // "continue on error", errors are ContinuedError lines, with their more
  // messages, instead of dialogs.
  std::function<void(TKeepUpToDateLog Entry, const std::string & Message, const std::string & MoreMessages)> OnLog;
  // It stopped itself, after an error or when the directory could no longer
  // be watched; Close when the session is gone too (the dialog closes). The
  // shell is to call KeepUpToDateStop once back from KeepUpToDateIdle.
  std::function<void(bool Close)> OnAbort;
};

class TSession
{
public:
  // Throws TCoreError when the URL is not one. A stored site's name is one,
  // as for the script's open.
  explicit TSession(const std::string & Url);
  // A site as edited, saved or not.
  explicit TSession(const TSite & Site);
  // The session the command line names, as WinSCP.exe starts with one
  // (WinMain's AutoStartSession, GetLoginData): a site's name or a URL,
  // with the switches that go with it (/privatekey=, /hostkey=,
  // /rawsettings ...; as unsafe with /unsafe). Null without one. CanOpen:
  // whether it says enough to connect; if not, WinSCP shows the Login
  // dialog with it. A folder of sites or a workspace is none: its name is
  // CommandLineFolderOrWorkspace's (else empty), opened with Workspace.
  // Throws TCoreError.
  static std::unique_ptr<TSession> FromCommandLine(bool & CanOpen);
  static std::string CommandLineFolderOrWorkspace();
  // A session on the same site, as it is now (TCustomScpExplorerForm's
  // DuplicateTab: CloneCurrentSessionData), not opened yet. Throws
  // TCoreError.
  std::unique_ptr<TSession> Duplicate() const;
  // Tabs > Reconnect Session, as TTerminalManager::ReconnectActiveTerminal:
  // a session that has lost its connection opened again
  // (TTerminal::Reopen), in the directory it was in. Throws TCoreError.
  void Reconnect();
  // The connection closed, the session kept, as a lost one leaves it (tests).
  void DropConnection();
  // Commands > Clear Caches (TTerminal::ClearCaches); whether there is
  // anything to clear (AreCachesEmpty).
  bool CachesEmpty() const;
  void ClearCaches();
  // Tabs > Change Password, as TCustomScpExplorerForm::ChangePassword: a
  // secondary session that authenticates with the password only and asks
  // for a new one. CanChangePassword: the protocol's fcChangePassword.
  // Throws TCoreError (Aborted when cancelled).
  bool CanChangePassword() const;
  void ChangePassword();
  // Tabs > Install Public Key into Server (SFTP: TTerminal::UploadPublicKey,
  // the key added to ~/.ssh/authorized_keys): its report. IsOpenSsh: the
  // server says it is OpenSSH (else WinSCP asks first); SshImplementation
  // is what it says. Throws TCoreError.
  bool CanUploadPublicKey() const;
  bool IsOpenSsh() const;
  std::string SshImplementation() const;
  std::string UploadPublicKey(const std::string & FileName);
  std::string PublicKeyFile() const;
  // Tabs > Save Session as Site (TCustomScpExplorerForm::SaveCurrentSession,
  // DoSaveSession): the session as it is now saved under Name; its
  // passwords only if SavePassword. Defaults: what the dialog starts with.
  struct TSaveAsSite
  {
    std::string Name;
    // The session has a password to save (and storing them is allowed);
    // saving it is not recommended (no master password, not anonymous);
    // the box's initial state.
    bool CanSavePassword = false;
    bool NotRecommended = true;
    bool SavePassword = false;
    // The sites' folders, sorted.
    std::vector<std::string> Folders;
  };
  TSaveAsSite SaveAsSiteDefaults() const;
  void SaveAsSite(const std::string & Name, bool SavePassword);
  // Tabs > Rename Tab (TCustomScpExplorerForm::RenameTab): the session's
  // name, as its tab shows it. Throws TCoreError for a name that is not one.
  void Rename(const std::string & Name);
  // A workspace's sessions, in its order, not opened yet
  // (GetFolderOrWorkspace). Throws TCoreError.
  static std::vector<std::unique_ptr<TSession>> Workspace(const std::string & Name);
  // The tabs saved as a workspace (DoCollectWorkspace, DoSaveWorkspace):
  // each session as it is now, with its local directory; passwords only if
  // asked. HasPasswords: whether any has one to save (SaveWorkspace's
  // AnyNonStoredSessionWithPassword). Throw TCoreError.
  struct TWorkspaceTab
  {
    const TSession * Session = nullptr;
    std::string LocalDirectory;
  };
  static void SaveWorkspace(const std::string & Name, const std::vector<TWorkspaceTab> & Tabs, bool SavePasswords);
  static bool WorkspaceHasPasswords(const std::vector<TWorkspaceTab> & Tabs);
  // To be called regularly on the GUI thread, for every open session: its
  // keepalives and dropped-connection checks (TTerminal::Idle, as
  // TTerminalManager::Idle calls it).
  void Idle();
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
  // The user's home directory, as the server reports it; with
  // DefaultDirIsHome (Preferences), the site's own directory when it has
  // one and does not remember the last used, as WinSCP's Home.
  void HomeDirectory(bool DefaultDirIsHome = false);
  // Reads the current directory again.
  void Refresh();
  // In the current directory, or at a full path.
  // As TCustomScpExplorerForm::CreateRemoteDirectory: with the Create
  // folder dialog's permissions, when set, and the transfer settings'
  // encryption of new files.
  void CreateDirectory(const std::string & Name, const TNewDirectory * Properties = nullptr);
  // Whether the server can set permissions (fcModeChanging).
  bool CanChangeRights() const;
  // Links, as TCustomScpExplorerForm::AddEditLink: whether links can be
  // made (symbolic links resolved, and the server makes them), hard ones
  // too, and existing links edited (the site resolves them).
  bool CanLink() const;
  bool CanHardLink() const;
  bool CanEditLinks() const;
  // Duplicate and Move to (Shift+F5, Shift+F6), as WinSCP's
  // RemoteTransferFiles: entries of the current directory to Target
  // (a directory) under FileMask ("*.*", or a new name). A direct copy is
  // the server's (fcRemoteCopy); otherwise the files go through a local
  // temporary copy. Throw TCoreError; false when not done in full.
  bool CanRemoteCopy() const;
  bool RemoteMove(const std::vector<std::string> & Names, const std::string & Target, const std::string & FileMask);
  bool RemoteCopy(const std::vector<std::string> & Names, const std::string & Target, const std::string & FileMask,
    bool Direct);
  // Whether Path is a directory on the server.
  bool DirectoryExists(const std::string & Path);
  // Find files, as TFileFindDialog over core's FilesFind: Directory and
  // below searched for what matches Mask (core's file masks); Found is
  // given each file (its directory, and it), Searching each directory
  // entered; either returning false stops the search. The files found are
  // kept by their order, for the calls after, until the next search.
  // Throws TCoreError (a mask that is not one; the listing failing).
  void FindFiles(const std::string & Directory, const std::string & Mask,
    const std::function<bool(const std::string & Directory, const TRemoteEntry & Entry)> & Found,
    const std::function<bool(const std::string & Directory)> & Searching);
  // The files found, by their order: downloaded, deleted (the caller
  // confirms). False when not done in full; throw TCoreError.
  bool DownloadFound(const std::vector<int> & Indexes, const std::string & LocalDirectory,
    const TTransferSettings * Settings = nullptr);
  bool DeleteFound(const std::vector<int> & Indexes);
  // A link FileName to PointTo in the current directory; Replace: the
  // existing link FileName is deleted first (an edit). Throws TCoreError.
  void CreateLink(const std::string & FileName, const std::string & PointTo, bool Symbolic, bool Replace);
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
  // Settings: this transfer's (the Copy dialog's); null for the GUI's
  // current ones, as WinSCP's.
  bool Upload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move = false,
    const TTransferSettings * Settings = nullptr);
  // Entries of the current directory to a local directory.
  // Files by their full paths, wherever the panel is now (the clipboard's,
  // as WinSCP's ClipboardDownload). Throws TCoreError.
  bool DownloadPaths(const std::vector<std::string> & Paths, const std::string & LocalDirectory,
    const TTransferSettings * Settings = nullptr);
  bool Download(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move = false,
    const TTransferSettings * Settings = nullptr);

  // The same in the background, through core's transfer queue (its own
  // connections, on its own threads), as WinSCP's "transfer in background".
  void QueueUpload(const std::vector<std::string> & LocalPaths, const std::string & RemoteDirectory, bool Move = false,
    const TTransferSettings * Settings = nullptr);
  void QueueDownload(const std::vector<std::string> & Names, const std::string & LocalDirectory, bool Move = false,
    const TTransferSettings * Settings = nullptr);

  // Synchronization, as WinSCP's Synchronize command: what differs between
  // a local and a remote directory and its subdirectories, collected into a
  // checklist (OnDirectory is told each pair compared; false from it
  // cancels, and nothing is returned); then applied, here or in the queue.
  // Throws TCoreError.
  std::unique_ptr<TSyncChecklist> SyncCollect(const std::string & LocalDirectory, const std::string & RemoteDirectory,
    const TSyncParams & Params,
    const std::function<bool(const std::string & Local, const std::string & Remote)> & OnDirectory = nullptr,
    const TTransferSettings * Settings = nullptr);
  // The checked entries. False when it was cancelled or core skipped some.
  bool SyncApply(TSyncChecklist & Checklist);
  // Each checked entry as its own queue item (TTerminal::SynchronizeToQueue).
  void SyncQueue(TSyncChecklist & Checklist);
  // The checklist's Calculate: the directories' sizes, each side's
  // (UpdateDirectorySize). Move (DoSynchronizeMove): the deleted ones moved
  // to the new one's place on their side, the entries gone. Throw TCoreError.
  void SyncCalculateSizes(TSyncChecklist & Checklist, const std::vector<std::size_t> & Indexes);
  void SyncMove(TSyncChecklist & Checklist, const std::vector<std::size_t> & Indexes);
  // Whether the server can set times, which "synchronize timestamps" needs.
  bool CanSyncTimestamps() const;
  // Whether it can compare by checksum (CanCalculateChecksum: the protocol's,
  // or a separate shell's).
  bool CanSyncChecksum() const;
  // Commands > Keep Remote Directory up to Date, as WinSCP's (upstream's
  // TSynchronizeController, compiled as it is): the local directory
  // watched, and each directory that changed, once quiet, synchronized to
  // the remote one; a full synchronization first when Params says.
  // Selected: the names "selected files only" limits the directories' first
  // level to. Throws TCoreError (Aborted when the question whether to watch
  // more directories is answered no).
  void KeepUpToDateStart(const TKeepUpToDateParams & Params, const TTransferSettings * Settings,
    const std::vector<std::string> & Selected, const TKeepUpToDateEvents & Events);
  void KeepUpToDateStop();
  bool KeepingUpToDate() const;
  // To be called regularly on the GUI thread (a timer) while it runs: the
  // changes collected and synchronized.
  void KeepUpToDateIdle();
  // Commands > Server/Protocol Information (TCustomScpExplorerForm's
  // FileSystemInfo), and the space available below Path. Throw TCoreError.
  TServerInfo ServerInfo();
  TSpaceInfo SpaceAvailable(const std::string & Path);
  // A remote custom command run on entries of the current directory, as
  // TCustomScpExplorerForm::RemoteCustomCommand: its patterns completed
  // (asking through Prompts), on each file or the list, in the protocol's
  // or the shell session's shell (which OpenConsole's checks open first).
  // Its output, when the command shows or copies it. Throws TCoreError
  // (Aborted when the prompts were cancelled).
  std::vector<std::string> RunCustomCommand(const TCustomCommandDefinition & Command,
    const std::vector<std::string> & Names, const TCustomCommandPrompts & Prompts);
  // Commands > Open in PuTTY, as WinSCP's: the session as it is now, in
  // the program Preferences > Integration > Applications names
  // (GUIConfiguration's PuttyPath), PuTTY's own options added unless its
  // command has the session's patterns. Prompts: its prompt patterns'.
  // Throws TCoreError.
  void OpenInPutty(const TCustomCommandPrompts & Prompts = TCustomCommandPrompts());
  // Whether custom commands can run here: the protocol's shell, or a shell
  // session's already open (fcShellAnyCommand).
  bool CanRunShellCommands() const;
  // The console (Commands > Open Terminal), as TCustomScpExplorerForm's
  // OpenConsole: whether commands can run here (the protocol's, or a
  // shell session's already open), or in a separate shell session
  // (fcSecondaryShell), which opens after WinSCP's question
  // (CommandSessionQuestion; GUIConfiguration's ConfirmCommandSession).
  bool CanRunCommands() const;
  bool CanOpenShellSession() const;
  std::string CommandSessionQuestion() const;
  void OpenShellSession();
  // A command run (AnyCommand), each line of its output and errors given
  // to Output as it comes; the current directory read after. Throws
  // TCoreError.
  void RunCommand(const std::string & Command, const std::function<void(const std::string & Line)> & Output);
  // Commands > Generate Session URL/Code and Files > Generate File URL, as
  // TCustomScpExplorerForm's GenerateUrl over this session's data
  // (SessionDataForCode). Throws TCoreError.
  TGeneratedCode GenerateCode(const TGenerateCodeOptions & Options);
  // The site's key for what is kept per site (SessionData's SessionKey):
  // its bookmarks.
  std::string SessionKey() const;

  // Properties, as WinSCP's (TCustomScpExplorerForm::SetProperties): of
  // entries of the current directory, their properties loaded first where
  // the listing lacks them. Throw TCoreError.
  TPropertiesInfo PropertiesOf(const std::vector<std::string> & Names);
  // Changes what Properties says differently from what the files have
  // (TRemoteProperties::ChangedProperties); an owner or group that is not
  // one is core's error. False when nothing was changed in full.
  bool SetProperties(const std::vector<std::string> & Names, const TFileProperties & Properties);
  // The entries' size, directories recursed, and how many of each kind
  // (CalculateFilesSize).
  void CalculateSize(const std::vector<std::string> & Names, std::int64_t & Size, int & Files, int & Directories,
    int & SymLinks);
  // Each file's checksum by the algorithm, recursing directories, as file
  // name and checksum (CalculateFilesChecksum).
  std::vector<std::pair<std::string, std::string>> Checksums(const std::vector<std::string> & Names,
    const std::string & Algorithm);

  // Editing, as WinSCP's (TCustomScpExplorerForm's ExecuteFile and
  // ExecutedFileChanged): an entry of the current directory downloaded to a
  // temporary directory of its own (TemporaryDirectoryForRemoteFiles), with
  // the temporary files' transfer settings (TemporaryFileCopyParam). Throws
  // TCoreError.
  TEditedFile EditDownload(const std::string & Name, bool ForceText = true);
  // Files > New > File, as TCustomScpExplorerForm::EditNew: an empty file
  // of that name made in such a temporary directory, to be edited and
  // uploaded as one downloaded (nothing on the server until it is saved).
  // Exists: there is such a file in the current directory already (edit
  // that instead). Throws TCoreError.
  TEditedFile EditNew(const std::string & Name);
  // The editor's Reload, as TCustomScpExplorerForm::ExecutedFileReload: the
  // file downloaded again over its copy, its source timestamp the server's
  // now. Throws TCoreError.
  void EditReload(TEditedFile & File);
  bool FileExists(const std::string & Name);
  // Its changed copy uploaded back over the original: through the queue
  // where the protocol has background transfers (as TEditorUploadQueueItem,
  // the file's source timestamp refreshed after), else here. With "check not
  // modified", a file changed on the server since is asked about first;
  // false when the user declined. Not Background: here in any case (the
  // session is about to close). Throws TCoreError.
  bool EditUpload(TEditedFile & File, bool Background = true);
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
  TSession();
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

}

#endif
