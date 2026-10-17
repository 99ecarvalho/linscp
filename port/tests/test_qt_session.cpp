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
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iterator>
#include <thread>
#include <random>
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
  int Transfers = 0;
  bool LastUpload = false;
  int LastCount = 0;
  bool Open = false;
  int ProgressCalls = 0;
  CoreBridge::TTransferProgress LastProgress;
  bool CancelTransfers = false;
  // Overwrite questions: answered with Yes; counted.
  int Overwrites = 0;

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
    else if (QueryAnswer != 0)
    {
      // A scripted answer, and the question kept.
      LastQuery = Query.Message;
      Result.Answer = QueryAnswer;
    }
    else
    {
      // Anything else here is core's "file exists": overwrite.
      Overwrites++;
      Result.Answer = CoreBridge::AnswerYes;
    }
    return Result;
  }

  // When not 0, what other questions are answered (the last one kept).
  unsigned QueryAnswer = 0;
  std::string LastQuery;

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

  void Banner(const std::string &, const std::string &, bool, bool &, bool &) override {}
  void Information(const std::string & Text) override { InformationLines += Text + "\n"; }
  void TransferStarted(bool Upload, int Count) override { Transfers++; LastUpload = Upload; LastCount = Count; Open = true; }
  bool TransferProgress(const CoreBridge::TTransferProgress & Progress) override
  {
    Check(Open, "progress comes between a transfer's start and end");
    ProgressCalls++;
    LastProgress = Progress;
    return !CancelTransfers;
  }
  void TransferFinished() override { Open = false; }
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

  TScriptedFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  if (Portuguese)
  {
    const std::string Code = CoreBridge::SetLanguage(argv[4], argv[5]);
    Check(Code == "PT", "pt_BR is WinSCP's PT translation");
    Check(CoreBridge::LanguageLocale() == "pt-BR", "and core's texts are in pt-BR");
  }
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

    // Transfers, on the server and the disk for real.
    namespace fs = std::filesystem;
    // Its own, as runs of this test (by protocol, language) may run at once.
    const fs::path Local = fs::temp_directory_path() /
      ("linscp-qt-transfer-" + Protocol + "-" + std::to_string(std::random_device()()));
    fs::remove_all(Local);
    fs::create_directories(Local / "down");
    const std::string Content(300000, 'z');
    {
      std::ofstream(Local / "up.bin", std::ios::binary) << Content;
    }
    const int Calls = Frontend.ProgressCalls;
    Check(Session.Upload({ (Local / "up.bin").string() }, ""), "a file uploads");
    Check(Frontend.LastUpload && (Frontend.LastCount == 1) && !Frontend.Open, "as one upload, started and finished");
    Check(Frontend.ProgressCalls > Calls, "reporting progress");
    Check((Frontend.LastProgress.FileName.find("up.bin") != std::string::npos) && (Frontend.LastProgress.Transferred > 0),
      "of that file, in bytes");
    bool Uploaded = false;
    for (const CoreBridge::TRemoteEntry & Entry : Session.List())
    {
      Uploaded = Uploaded || ((Entry.Name == "up.bin") && (Entry.Size == static_cast<std::int64_t>(Content.size())));
    }
    Check(Uploaded, "and is there, whole");
    Check(Session.Download({ "up.bin" }, (Local / "down").string()), "it downloads");
    {
      std::ifstream In(Local / "down" / "up.bin", std::ios::binary);
      const std::string Back((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
      Check(Back == Content, "byte for byte");
    }
    Check(!Frontend.LastUpload, "as a download");
    const int Overwrites = Frontend.Overwrites;
    Check(Session.Download({ "up.bin" }, (Local / "down").string()), "downloading it again");
    Check(Frontend.Overwrites == Overwrites + 1, "asks about overwriting it, through the frontend");

    {
      std::ofstream(Local / "big.bin", std::ios::binary) << std::string(20 * 1024 * 1024, 'b');
    }
    Frontend.CancelTransfers = true;
    bool Completed = true;
    try
    {
      Completed = Session.Upload({ (Local / "big.bin").string() }, "");
    }
    catch (const CoreBridge::TCoreError &)
    {
      Completed = false;
    }
    Frontend.CancelTransfers = false;
    Check(!Completed, "a cancelled upload does not complete");
    Check(!Frontend.Open, "and its progress window closes");

    {
      std::ofstream(Local / "moved.txt", std::ios::binary) << "move me";
    }
    Check(Session.Upload({ (Local / "moved.txt").string() }, "", true), "a file moves up");
    Check(!fs::exists(Local / "moved.txt") && HasEntry(Session.List(), "moved.txt", false), "leaving no local copy");

    // In the background, through core's queue and its own connections.
    {
      std::ofstream(Local / "queued.bin", std::ios::binary) << std::string(200000, 'q');
    }
    const int PromptsBefore = Frontend.Prompts;
    Session.QueueUpload({ (Local / "queued.bin").string() }, "");
    Check(!Session.QueueEmpty(), "an upload is queued");
    const auto Wait = [&Session]()
    {
      for (int Round = 0; (Round < 600) && !Session.QueueEmpty(); Round++)
      {
        Session.QueueIdle();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
      Session.QueueIdle();
    };
    bool SawProcessing = false;
    Session.QueueIdle();
    for (const CoreBridge::TQueueEntry & Entry : Session.QueueEntries())
    {
      SawProcessing = SawProcessing || (Entry.Upload && (Entry.Source.find("queued.bin") != std::string::npos));
    }
    Check(SawProcessing, "and listed, with its source");
    Wait();
    Check(Session.QueueEmpty(), "the queue empties");
    Check(Session.QueueDoneCount() >= 1, "counting it done");
    Check(Frontend.Prompts == PromptsBefore, "its connection reuses the password, no prompt");
    Session.Refresh();
    bool Queued = false;
    for (const CoreBridge::TRemoteEntry & Entry : Session.List())
    {
      Queued = Queued || ((Entry.Name == "queued.bin") && (Entry.Size == 200000));
    }
    Check(Queued, "and the file is on the server, whole");
    fs::remove(Local / "down" / "queued.bin");
    Session.QueueDownload({ "queued.bin" }, (Local / "down").string());
    Wait();
    Check(fs::exists(Local / "down" / "queued.bin") && (fs::file_size(Local / "down" / "queued.bin") == 200000),
      "a queued download arrives whole");

    // Transfer settings, as WinSCP's: the GUI's current ones, or the
    // transfer's own.
    {
      const CoreBridge::TTransferSettings Defaults = CoreBridge::DefaultTransferSettings();
      Check(Defaults.PreserveTime && (Defaults.Mode == CoreBridge::TTransferSettings::TMode::Binary) &&
        (Defaults.Resume == CoreBridge::TTransferSettings::TResume::Smart), "WinSCP's default transfer settings");
      CoreBridge::TTransferSettings Upper = Defaults;
      Upper.NameCase = CoreBridge::TTransferSettings::TNameCase::Upper;
      CoreBridge::SetDefaultTransferSettings(Upper);
      std::ofstream(Local / "lower.txt") << "case";
      Check(Session.Upload({ (Local / "lower.txt").string() }, ""), "an upload with the default settings changed");
      Check(HasEntry(Session.List(), "LOWER.TXT", false), "follows them: the name upper-cased");
      CoreBridge::SetDefaultTransferSettings(Defaults);
      fs::create_directories(Local / "masked");
      std::ofstream(Local / "masked" / "keep.txt") << "keep";
      std::ofstream(Local / "masked" / "skip.log") << "skip";
      CoreBridge::TTransferSettings Masked = Defaults;
      Masked.IncludeFileMask = "|*.log";
      Check(Session.Upload({ (Local / "masked").string() }, "", false, &Masked), "an upload with its own settings");
      Session.ChangeDirectory("/masked");
      Check(HasEntry(Session.List(), "keep.txt", false) && !HasEntry(Session.List(), "skip.log", false),
        "follows them: the mask excludes *.log");
      Session.ChangeDirectory("/");
      CoreBridge::TTransferPreset Preset;
      Preset.Name = "Lower case";
      Preset.Settings = Defaults;
      Preset.Settings.NameCase = CoreBridge::TTransferSettings::TNameCase::Lower;
      CoreBridge::SetTransferPresets({ Preset });
      CoreBridge::SetCurrentTransferPreset("Lower case");
      Check((CoreBridge::TransferPresets().size() == 1) && (CoreBridge::CurrentTransferPreset() == "Lower case") &&
        (CoreBridge::CurrentTransferSettings().NameCase == CoreBridge::TTransferSettings::TNameCase::Lower),
        "a preset in use is what transfers use");
      Check(!CoreBridge::TransferSettingsInfo(Preset.Settings).empty(), "summarised as WinSCP does");
      std::ofstream(Local / "UPPER.TXT") << "case";
      Check(Session.Upload({ (Local / "UPPER.TXT").string() }, "") && HasEntry(Session.List(), "upper.txt", false),
        "uploaded under it, lower-cased");
      CoreBridge::SetCurrentTransferPreset("");
      CoreBridge::SetTransferPresets({});
    }

    // Synchronization, as WinSCP's: collected into a checklist, applied.
    const fs::path SyncLocal = Local / "sync";
    fs::create_directories(SyncLocal / "d");
    std::ofstream(SyncLocal / "a.txt") << "a";
    std::ofstream(SyncLocal / "b.txt") << "b";
    std::ofstream(SyncLocal / "d" / "c.txt") << "c";
    Session.CreateDirectory("/syncr");
    {
      std::ofstream(Local / "remote-only.txt") << "r";
    }
    Session.Upload({ (Local / "remote-only.txt").string() }, "/syncr");
    fs::remove(Local / "remote-only.txt");
    const auto Find = [](const CoreBridge::TSyncChecklist & List, const std::string & Name) -> int
    {
      for (std::size_t Index = 0; Index < List.Items().size(); Index++)
      {
        if (List.Items()[Index].Name == Name)
        {
          return static_cast<int>(Index);
        }
      }
      return -1;
    };
    using TAction = CoreBridge::TSyncItem::TAction;
    CoreBridge::TSyncParams Params;
    std::vector<std::string> Compared;
    std::unique_ptr<CoreBridge::TSyncChecklist> List = Session.SyncCollect(SyncLocal.string(), "/syncr", Params,
      [&Compared](const std::string & L, const std::string & R) { Compared.push_back(L + "|" + R); return true; });
    Check(List && (List->Items().size() == 4), "synchronizing the remote directory finds four differences");
    Check(!Compared.empty(), "telling which directories it compares");
    const int A = List ? Find(*List, "a.txt") : -1;
    const int B = List ? Find(*List, "b.txt") : -1;
    const int D = List ? Find(*List, "d") : -1;
    const int Only = List ? Find(*List, "remote-only.txt") : -1;
    Check((A >= 0) && (B >= 0) && (D >= 0) && (Only >= 0), "a.txt, b.txt, d and remote-only.txt");
    Check((Only >= 0) && (List->Items()[Only].Action == TAction::DeleteRemote) && !List->Items()[Only].Checked,
      "a remote-only file is listed to delete, unchecked without \"delete files\", as WinSCP's");
    Check((A >= 0) && (List->Items()[A].Action == TAction::UploadNew) && List->Items()[A].LocalExists &&
      !List->Items()[A].RemoteExists && (List->Items()[A].LocalSize == 1) && List->Items()[A].Checked,
      "a new file to upload, checked, with its local size");
    Check((D >= 0) && List->Items()[D].Directory && (List->Items()[D].Action == TAction::UploadNew), "a new directory");
    Check((A >= 0) && (List->Items()[A].RemoteDirectory == "/syncr/"), "with the directory it goes to");
    if (B >= 0)
    {
      List->Check(B, false);
    }
    Check(List && (List->CheckedCount() == 2), "an entry unchecked");
    Check(List && Session.SyncApply(*List), "the checked entries apply");
    Session.ChangeDirectory("/syncr");
    Check(HasEntry(Session.List(), "a.txt", false) && HasEntry(Session.List(), "d", true) &&
      !HasEntry(Session.List(), "b.txt", false), "uploaded, but not the unchecked one");
    Session.ChangeDirectory("/syncr/d");
    Check(HasEntry(Session.List(), "c.txt", false), "a new directory with its contents");
    Session.ChangeDirectory("/");

    List = Session.SyncCollect(SyncLocal.string(), "/syncr", Params);
    Check(List && (List->Items().size() == 2) && (Find(*List, "b.txt") >= 0) && (List->CheckedCount() == 1),
      "compared again, only what was left differs (times are kept)");

    Params.Delete = true;
    List = Session.SyncCollect(SyncLocal.string(), "/syncr", Params);
    const int R = List ? Find(*List, "remote-only.txt") : -1;
    Check((R >= 0) && (List->Items()[R].Action == TAction::DeleteRemote) && List->Items()[R].RemoteExists &&
      List->Items()[R].Checked, "with \"delete files\", a remote-only file is to delete, checked");
    if (R >= 0)
    {
      List->Reverse(R);
    }
    Check((R >= 0) && (List->Items()[R].Action == TAction::DownloadNew), "reversed, it is to download");
    Check(List && Session.SyncApply(*List), "and applies");
    Check(fs::exists(SyncLocal / "remote-only.txt"), "downloaded, not deleted");
    Session.ChangeDirectory("/syncr");
    Check(HasEntry(Session.List(), "b.txt", false) && HasEntry(Session.List(), "remote-only.txt", false),
      "and the rest uploaded");
    Session.ChangeDirectory("/");

    Params.Delete = false;
    std::ofstream(SyncLocal / "q.txt") << "queued";
    List = Session.SyncCollect(SyncLocal.string(), "/syncr", Params);
    Check(List && (List->Items().size() == 1), "a new file to synchronize");
    const int DoneBefore = Session.QueueDoneCount();
    if (List)
    {
      Session.SyncQueue(*List);
    }
    // A file this small may be done already, under load: done through the
    // queue then.
    Session.QueueIdle();
    Check(!Session.QueueEmpty() || (Session.QueueDoneCount() > DoneBefore), "goes to the queue");
    Wait();
    Session.ChangeDirectory("/syncr");
    Check(HasEntry(Session.List(), "q.txt", false), "and is uploaded from there");
    Session.ChangeDirectory("/");

    bool Cancelled = false;
    try
    {
      Session.SyncCollect(SyncLocal.string(), "/syncr", Params, [](const std::string &, const std::string &) { return false; });
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Cancelled = E.Aborted;
    }
    Check(Cancelled, "comparing cancelled aborts");
    Check(!Sftp || Session.CanSyncTimestamps(), "timestamps can be synchronized over SFTP");

    // Editing, as WinSCP's: a remote file into a temporary directory of its
    // own, its changed copy back over it.
    {
      Session.ChangeDirectory("/");
      std::ofstream(Local / "edit.txt") << "first\n";
      Session.Upload({ (Local / "edit.txt").string() }, "");
      CoreBridge::TEditedFile Edited = Session.EditDownload("edit.txt");
      Check(fs::exists(Edited.LocalFile) && (fs::path(Edited.LocalFile).filename() == "edit.txt"),
        "a file to edit is downloaded, under its name");
      Check(!Edited.RootDirectory.empty() && (Edited.LocalFile.rfind(Edited.RootDirectory, 0) == 0),
        "into a temporary directory of its own");
      Check((Edited.RemoteDirectory == "/") && (Edited.OriginalName == "edit.txt") && (Edited.SourceTimestamp() > 0),
        "knowing where it came from and how it was");
      {
        std::ifstream In(Edited.LocalFile);
        std::string Line;
        std::getline(In, Line);
        Check(Line == "first", "whole");
      }
      std::ofstream(Edited.LocalFile, std::ios::trunc) << "second\n";
      Check(Session.EditUpload(Edited), "the changed copy goes back");
      Wait();
      fs::remove(Local / "down" / "edit.txt");
      Session.Refresh();
      Session.Download({ "edit.txt" }, (Local / "down").string());
      {
        std::ifstream In(Local / "down" / "edit.txt");
        std::string Line;
        std::getline(In, Line);
        Check(Line == "second", "and is on the server");
      }
      // "Check not modified": a file changed on the server since is asked
      // about; declined, it is not uploaded.
      CoreBridge::TEditorSettings Editor = CoreBridge::EditorSettings();
      Check(!Editor.CheckNotModified && Editor.Utf8 && (Editor.TabSize == 8), "the editor's defaults are WinSCP's");
      Editor.CheckNotModified = true;
      CoreBridge::SetEditorSettings(Editor);
      Edited.SetSourceTimestamp(1);
      Frontend.QueryAnswer = CoreBridge::AnswerCancel;
      Check(!Session.EditUpload(Edited) && !Frontend.LastQuery.empty(), "changed on the server: asked, declined, not uploaded");
      Frontend.QueryAnswer = CoreBridge::AnswerOK;
      Check(Session.EditUpload(Edited), "accepted, uploaded");
      Wait();
      Frontend.QueryAnswer = 0;
      Editor.CheckNotModified = false;
      CoreBridge::SetEditorSettings(Editor);
      fs::remove_all(Edited.RootDirectory);
    }

    // Logging (Preferences, core's configuration): a session opened with it
    // on writes its log and its XML log.
    {
      CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
      Check(!Preferences.Logging && !Preferences.DefaultLogFileName.empty(), "logging is off by default, with a default file");
      Preferences.Logging = true;
      Preferences.LogLevel = 2;
      Preferences.LogFileName = (Local / "session.log").string();
      Preferences.LogActions = true;
      Preferences.ActionsLogFileName = (Local / "session.xml").string();
      CoreBridge::SetPreferences(Preferences);
      Frontend.Passwords = { "secret" };
      CoreBridge::TSession Logged(Url);
      Logged.Open();
      Logged.List();
      Logged.Close();
      std::ifstream Log(Local / "session.log");
      const std::string Text((std::istreambuf_iterator<char>(Log)), std::istreambuf_iterator<char>());
      Check(Text.find("127.0.0.1") != std::string::npos, "the session log is written");
      Check(fs::exists(Local / "session.xml") && (fs::file_size(Local / "session.xml") > 0), "and the XML log");
#if !defined(_WIN32)
      // WinSCP's default path, %TEMP%\!S.log, works where TEMP is not set:
      // the session's log in TMPDIR.
      const fs::path Temporary = Local / "temporary";
      fs::create_directories(Temporary);
      unsetenv("TEMP");
      setenv("TMPDIR", Temporary.c_str(), 1);
      Preferences.LogFileName = Preferences.DefaultLogFileName;
      Preferences.LogActions = false;
      CoreBridge::SetPreferences(Preferences);
      Frontend.Passwords = { "secret" };
      {
        CoreBridge::TSession Default(Url);
        Default.Open();
        Default.Close();
      }
      bool Found = false;
      for (const fs::directory_entry & Entry : fs::directory_iterator(Temporary))
      {
        Found = Found || (Entry.path().extension() == ".log");
      }
      Check(Found, "the default log path is the temporary directory's");
      unsetenv("TMPDIR");
#endif
      Preferences.Logging = false;
      Preferences.LogActions = false;
      Preferences.LogFileName.clear();
      CoreBridge::SetPreferences(Preferences);
      Check(!CoreBridge::Preferences().Logging, "logging turns off again");
      Check(CoreBridge::SizeOfText("10M") == 10 * 1024 * 1024 && (CoreBridge::TextOfSize(1024 * 1024) == "1M"),
        "sizes as WinSCP's dialogs type them");
    }
    fs::remove_all(Local);
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
