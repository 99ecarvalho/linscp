// The Qt shell's stored sites (port/qt/Sites.cpp) over core's own storage,
// in an INI file, in two processes so that saving is seen to persist:
//
//   test_qt_sites /ini=<file> save <port>
//     a new site from the defaults, saved with its password into a folder,
//     and a second one; names core refuses are refused
//   test_qt_sites /ini=<file> check <port>
//     both read back as saved; the first opens with its stored password (no
//     prompt) against run_qt_sites.py's SFTP server; a rename keeps the
//     site's other settings and drops the old name; a delete removes it
#include "CoreBridge.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
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

class TFrontend : public CoreBridge::IFrontend
{
public:
  int Prompts = 0;
  CoreBridge::TQueryResult Query(const CoreBridge::TQuery &) override
  {
    // The unknown host key: accept.
    CoreBridge::TQueryResult Result;
    Result.Answer = CoreBridge::AnswerYes;
    return Result;
  }
  bool Prompt(CoreBridge::TPrompt &) override { Prompts++; return false; }
  void ShowError(const CoreBridge::TError & Error) override { std::printf("  error: %s\n", Error.Message.c_str()); }
  void Banner(const std::string &, const std::string &, bool, bool &) override {}
  void Information(const std::string &) override {}
  void BusyStart() override {}
  void BusyEnd() override {}
  void ProcessEvents() override {}
  void SetClipboard(const std::string &) override {}
  bool GetClipboard(std::string &) override { return false; }
};

bool Listed(const std::string & Name)
{
  const std::vector<std::string> Names = CoreBridge::SiteNames();
  return std::find(Names.begin(), Names.end(), Name) != Names.end();
}

void Save(int Port)
{
  CoreBridge::TSite Site = CoreBridge::NewSite();
  Check(Site.Protocol == CoreBridge::TSite::TProtocol::Sftp, "a new site is SFTP, as core's default");
  Check(Site.Port == 0, "on the protocol's default port");
  Site.Name = "Tests/rclone";
  Site.HostName = "127.0.0.1";
  Site.Port = Port;
  Site.UserName = "linscp";
  Site.Password = "secret";
  Site.RemoteDirectory = "/sub";
  CoreBridge::SaveSite(Site);
  Check(Listed("Tests/rclone"), "a saved site is listed under its folder");

  CoreBridge::TSite Ftp = CoreBridge::NewSite();
  Ftp.Name = "Other ftp";
  Ftp.Protocol = CoreBridge::TSite::TProtocol::Ftp;
  Ftp.Encryption = CoreBridge::TSite::TEncryption::Explicit;
  Ftp.HostName = "ftp.example.com";
  CoreBridge::SaveSite(Ftp);

  bool Refused = false;
  try
  {
    CoreBridge::TSite Bad = Ftp;
    Bad.Name = " ";
    CoreBridge::SaveSite(Bad);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Refused = !E.Error.Message.empty();
  }
  Check(Refused, "a site without a name is refused, with a message");
  Check(CoreBridge::SiteExists("Other ftp") && !CoreBridge::SiteExists("Nothing"), "a saved site exists, others not");
  Check(CoreBridge::DefaultPort(CoreBridge::TSite::TProtocol::Ftp, CoreBridge::TSite::TEncryption::Implicit) == 990,
    "implicit FTPS defaults to port 990");
}

void Check(int Port, TFrontend & Frontend)
{
  Check(Listed("Tests/rclone") && Listed("Other ftp"), "both sites persist");
  const CoreBridge::TSite Site = CoreBridge::LoadSite("Tests/rclone");
  Check((Site.HostName == "127.0.0.1") && (Site.Port == Port) && (Site.UserName == "linscp"), "the server and user persist");
  Check(Site.Password == "secret", "the saved password persists");
  Check(Site.RemoteDirectory == "/sub", "the remote directory persists");
  const CoreBridge::TSite Ftp = CoreBridge::LoadSite("Other ftp");
  Check((Ftp.Protocol == CoreBridge::TSite::TProtocol::Ftp) && (Ftp.Encryption == CoreBridge::TSite::TEncryption::Explicit),
    "FTP with explicit TLS persists");
  Check(Ftp.Port == 0, "on its default port");

  {
    CoreBridge::TSession Session("Tests/rclone");
    try
    {
      Session.Open();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      std::printf("  open failed: %s\n", E.what());
    }
    Check(Session.Active(), "the stored site opens by its name");
    Check(Frontend.Prompts == 0, "with its stored password, no prompt");
    Check(Session.CurrentDirectory() == "/sub", "in its remote directory");
  }

  // Saved as: a copy, the original kept.
  CoreBridge::TSite Copy = Site;
  Copy.Name = "Tests/copy";
  CoreBridge::SaveSite(Copy, "Tests/rclone");
  Check(Listed("Tests/copy") && Listed("Tests/rclone"), "saving under another name copies");
  CoreBridge::DeleteSite("Tests/copy");
  CoreBridge::RenameSite("Tests/rclone", "Tests/renamed");
  Check(Listed("Tests/renamed") && !Listed("Tests/rclone"), "a rename drops the old name");
  Check(CoreBridge::LoadSite("Tests/renamed").Password == "secret", "and keeps the site");

  CoreBridge::DeleteSite("Tests/renamed");
  Check(!Listed("Tests/renamed"), "a deleted site is gone");
  bool Missing = false;
  try
  {
    CoreBridge::LoadSite("Tests/renamed");
  }
  catch (const CoreBridge::TCoreError &)
  {
    Missing = true;
  }
  Check(Missing, "and does not load");
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 4)
  {
    std::fprintf(stderr, "usage: test_qt_sites /ini=<file> save|check <port>\n");
    return 2;
  }
  const int Port = std::atoi(argv[3]);
  TFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  if (std::strcmp(argv[2], "save") == 0)
  {
    Save(Port);
  }
  else
  {
    Check(Port, Frontend);
  }
  CoreBridge::Finalize();
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures ? 1 : 0;
}
