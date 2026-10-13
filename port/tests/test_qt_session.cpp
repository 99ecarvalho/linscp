// The Qt shell's bridge to core (port/qt: CoreBridge, Session, QtInterface)
// against a real SFTP server, with a scripted frontend in Qt's place: each
// question core asks while a session opens reaches the frontend as the shell
// would show it, and the frontend's answers reach core.
//
//   test_qt_session /ini=nul <port> [sftp|ftp [<locale> <translations directory>]]
//
// With a locale (pt_BR), core runs in WinSCP's translation for it, and its
// error comes back in that language.
//
// The server is run_qt_session.py's, serving a directory holding hello.txt
// and sub/ to user linscp, password secret: rclone over SFTP, its host key
// unknown, or the suite's FTP server -- FileZilla's engine, whose sockets run
// on window messages, under the shell's event loop.
#include "CoreBridge.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>

namespace
{

int Failures = 0;

void Check(bool Condition, const char * What)
{
  if (!Condition)
  {
    std::printf("FAIL %s\n", What);
    Failures++;
  }
}

// "Copy key" is the host key query's qaRetry, "Paste key" its qaIgnore
// (SecureShell.cpp's aliases).
const unsigned int CopyKeyAnswer = CoreBridge::AnswerRetry;

// Answers from a script, and records what it was asked.
class TScriptedFrontend : public CoreBridge::IFrontend
{
public:
  std::deque<std::string> Passwords;
  // Whether to press "Copy key" before accepting the host key, and whether to
  // accept it at all.
  bool CopyHostKey = true;
  bool AcceptHostKey = true;

  int Queries = 0;
  int HostKeyQueries = 0;
  int Prompts = 0;
  int Errors = 0;
  bool StayedOpenAfterCopy = false;
  std::string Clipboard;
  std::string LastPromptName;
  std::string InformationLines;

  CoreBridge::TQueryResult Query(const CoreBridge::TQuery & Query) override
  {
    Queries++;
    CoreBridge::TQueryResult Result;
    Result.Answer = Query.CancelAnswer;
    // The host key query, in any language: the one with "Copy key" and
    // "Paste key", buttons that act before they answer.
    bool HostKey = false;
    for (const CoreBridge::TQueryButton & Button : Query.Buttons)
    {
      HostKey = HostKey || static_cast<bool>(Button.OnSubmit);
    }
    if (HostKey)
    {
      HostKeyQueries++;
      for (const CoreBridge::TQueryButton & Button : Query.Buttons)
      {
        // "Copy key" acts and keeps the dialog open. (Its sibling, "Paste
        // key", compares the clipboard with the key, and asks about that.)
        if (CopyHostKey && Button.OnSubmit && (Button.Answer == CopyKeyAnswer))
        {
          StayedOpenAfterCopy = (Button.OnSubmit() == 0);
        }
      }
      // Accept: upstream's first button, Yes; or Cancel.
      Result.Answer = AcceptHostKey ? CoreBridge::AnswerYes : CoreBridge::AnswerCancel;
    }
    return Result;
  }

  bool Prompt(CoreBridge::TPrompt & Prompt) override
  {
    Prompts++;
    LastPromptName = Prompt.Name;
    if ((Prompt.Kind != CoreBridge::TPromptKind::Password) || Prompt.Fields.empty() || Passwords.empty())
    {
      return false;
    }
    Check(!Prompt.Fields[0].Echo, "a password is asked for without echo");
    Prompt.Fields[0].Value = Passwords.front();
    Passwords.pop_front();
    return true;
  }

  void ShowError(const CoreBridge::TError & Error) override
  {
    Errors++;
    std::printf("  error shown: %s\n", Error.Message.c_str());
  }

  void Banner(const std::string &, const std::string &, bool, bool &) override {}
  void Information(const std::string & Text) override { InformationLines += Text + "\n"; }
  void BusyStart() override {}
  void BusyEnd() override {}
  void ProcessEvents() override {}
  void SetClipboard(const std::string & Text) override { Clipboard = Text; }
  bool GetClipboard(std::string & Text) override { Text = Clipboard; return !Text.empty(); }
};

bool HasEntry(const std::vector<CoreBridge::TRemoteEntry> & Entries, const std::string & Name, bool Directory)
{
  for (const CoreBridge::TRemoteEntry & Entry : Entries)
  {
    if ((Entry.Name == Name) && (Entry.Directory == Directory))
    {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 3)
  {
    std::fprintf(stderr, "usage: test_qt_session /ini=nul <port> [sftp|ftp]\n");
    return 2;
  }
  const std::string Protocol = (argc > 3) ? argv[3] : "sftp";
  const bool Sftp = (Protocol == "sftp");
  const std::string Url = Protocol + "://linscp@127.0.0.1:" + argv[2] + "/";
  const bool Portuguese = (argc > 5);
  if (Portuguese)
  {
    const std::string Code = CoreBridge::SetLanguage(argv[4], argv[5]);
    Check(Code == "PT", "pt_BR is WinSCP's PT translation");
  }

  TScriptedFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    // A wrong password first: core says so and asks again.
    Frontend.Passwords = { "wrong", "secret" };
    CoreBridge::TSession Session(Url);
    try
    {
      Session.Open();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      std::printf("  open failed: %s\n", E.what());
    }
    Check(Session.Active(), "the session opens");
    if (Sftp)
    {
      Check(Frontend.HostKeyQueries == 1, "the unknown host key is asked about once");
      Check(Frontend.StayedOpenAfterCopy, "\"Copy key\" acts without answering");
      Check(!Frontend.Clipboard.empty(), "and puts the key on the clipboard");
      Check(Frontend.InformationLines.find(Portuguese ? "Autentica" : "Authenticat") != std::string::npos,
        "progress lines arrive while connecting, in core's language");
    }
    Check(Frontend.Prompts == 2, "the password is asked for again after a wrong one");
    Check(Frontend.Passwords.empty(), "both passwords were used");

    const std::vector<CoreBridge::TRemoteEntry> Root = Session.List();
    Check(HasEntry(Root, "hello.txt", false), "the listing has hello.txt");
    Check(HasEntry(Root, "sub", true), "and the sub directory");
    Session.ChangeDirectory("sub");
    Check(Session.CurrentDirectory() == "/sub", "the session changes directory");
    Session.ChangeDirectory("..");
    Check(Session.CurrentDirectory() == "/", "and back up");

    // The remote panel's operations, on the server for real.
    try
    {
    Session.CreateDirectory("made");
    Check(HasEntry(Session.List(), "made", true), "a directory is made");
    Session.Rename("made", "renamed");
    Check(HasEntry(Session.List(), "renamed", true) && !HasEntry(Session.List(), "made", true), "and renamed");
    bool Bad = false;
    for (const CoreBridge::TRemoteEntry & Entry : Session.List())
    {
      Bad = Bad || ((Entry.Name == "hello.txt") && ((Entry.Size != 14) || (Entry.ModifiedTime < 1e9)));
    }
    Check(!Bad, "entries carry their size and time");
    Check(Session.Delete({ "renamed", "hello.txt" }), "a directory and a file delete together");
    Check(!HasEntry(Session.List(), "renamed", true) && !HasEntry(Session.List(), "hello.txt", false), "and are gone");
    Check(HasEntry(Session.List(), "sub", true), "leaving the rest");
    bool Refused = false;
    try
    {
      Session.Rename("nothing", "else");
    }
    catch (const CoreBridge::TCoreError &)
    {
      Refused = true;
    }
    Check(Refused, "renaming an entry that is not there fails");
    Session.Refresh();
    Check(HasEntry(Session.List(), "sub", true), "a refresh reads the directory again");
    Session.ChangeDirectory("sub");
    Session.HomeDirectory();
    Check(Session.CurrentDirectory() == "/", "home is the server's home");
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Check(false, "the remote operations succeed");
      std::printf("  %s\n  %s\n", E.what(), E.Error.MoreMessages.c_str());
    }
    Session.Close();
    Check(!Session.Active(), "and closes");
  }
  {
    // Cancelling the password prompt: core gives up with its own error, which
    // the shell shows (SecureShell.cpp's CREDENTIALS_NOT_SPECIFIED).
    Frontend.Passwords.clear();
    Frontend.CopyHostKey = false;
    CoreBridge::TSession Session(Url);
    bool Shown = false;
    bool Failed = false;
    std::string Message;
    try
    {
      Session.Open();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Failed = true;
      Shown = !E.Aborted;
      Message = E.what();
    }
    Check(Failed && !Session.Active(), "a cancelled password prompt does not connect");
    Check(Shown && (Message.find(Portuguese ? "Não foram apresentadas credenciais" : "Credentials were not specified") !=
      std::string::npos), "and core's error says so, to be shown, in core's language");
  }
  if (Sftp)
  {
    // Declining the unknown host key does not connect, and core says why.
    Frontend.AcceptHostKey = false;
    Frontend.Passwords = { "secret" };
    CoreBridge::TSession Session(Url);
    bool Shown = false;
    try
    {
      Session.Open();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Shown = !E.Aborted && !E.Error.Message.empty();
    }
    Check(!Session.Active() && Shown, "a declined host key does not connect, with core's error");
    Check(Frontend.Passwords.size() == 1, "and no password is asked for");
    bool Refused = false;
    try
    {
      Session.ChangeDirectory("sub");
    }
    catch (const CoreBridge::TCoreError &)
    {
      Refused = true;
    }
    Check(Refused, "a session that did not open refuses to change directory");
    Frontend.AcceptHostKey = true;
  }
  {
    // A URL with no host asks for one; giving none gives up.
    CoreBridge::TSession Session("sftp://");
    bool Aborted = false;
    try
    {
      Session.Open();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Aborted = E.Aborted;
    }
    Check(Aborted, "a URL without a host asks for it, and giving none aborts");
  }
  CoreBridge::Finalize();

  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures ? 1 : 0;
}
