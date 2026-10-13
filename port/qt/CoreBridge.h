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

// One connection, as WinSCP's "open" command takes it: a URL
// (sftp://user@host:port/path, ftp://, ftps://, ftpes://, dav://, davs://,
// s3://, scp://). Prompts and errors reach the shell through IFrontend while
// Open runs.
class TSession
{
public:
  // Throws TCoreError when the URL is not one.
  explicit TSession(const std::string & Url);
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
