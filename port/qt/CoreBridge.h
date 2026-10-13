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
};
std::string Text(TText Text);

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
  // As core formats it for display.
  std::string Modified;
  std::string Rights;
};

// --- Stored sites -------------------------------------------------------------

// A site as WinSCP's Login dialog edits it: its protocol and encryption, the
// server, the credentials, the directory to start in. Everything else a
// stored site holds is kept as it was when the site is saved again.
struct TSite
{
  enum class TProtocol { Sftp, Scp, Ftp, WebDav, S3 };
  // FTP: implicit TLS (ftps://) or explicit (ftpes://); WebDAV and S3: TLS
  // or not (Implicit = TLS).
  enum class TEncryption { None, Implicit, Explicit };

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
};

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

// One connection, as WinSCP's "open" command takes it: a URL
// (sftp://user@host:port/path, ftp://, ftps://, ftpes://, dav://, davs://,
// s3://, scp://). Prompts and errors reach the shell through IFrontend while
// Open runs.
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
  // The current directory's entries, without "." and "..".
  std::vector<TRemoteEntry> List() const;
  // Throws TCoreError.
  void ChangeDirectory(const std::string & Directory);
  void Close();

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

}

#endif
