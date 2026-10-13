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
//   test_qt_sites /ini=nul keys <sshd port> <OpenSSH key> <user> <directory>
//     an OpenSSH key is found convertible, converted to a .ppk in the
//     directory, which is usable and logs in to run_qt_sites.py's sshd
//   test_qt_sites /ini=nul proxy <sftp port> <socks5 port>
//     a site through a SOCKS5 proxy that requires a password
#include "CoreBridge.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
  void TransferStarted(bool, int) override {}
  bool TransferProgress(const CoreBridge::TTransferProgress &) override { return true; }
  void TransferFinished() override {}
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
  Site.LocalDirectory = "/tmp";
  Site.Timeout = 31;
  Site.TryAgent = false;
  Site.AgentForwarding = true;
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
  Check((Site.LocalDirectory == "/tmp") && (Site.Timeout == 31) && !Site.TryAgent && Site.AgentForwarding,
    "the advanced settings persist");
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

void Keys(int Port, const std::string & Key, const std::string & User, const std::string & Directory, TFrontend & Frontend)
{
  const CoreBridge::TKeyCheck Check1 = CoreBridge::CheckKey(Key);
  Check(Check1.Status == CoreBridge::TKeyCheck::TStatus::Convertible, "an OpenSSH key is convertible");
  Check(Check1.Message.find("OpenSSH") != std::string::npos, "and core's question names its format");
  Check(Check1.ConvertedFileName.size() > 4 && Check1.ConvertedFileName.substr(Check1.ConvertedFileName.size() - 4) == ".ppk",
    "with a .ppk beside it suggested");
  const std::string Ppk = Directory + "/client.ppk";
  std::string Message;
  try
  {
    Message = CoreBridge::ConvertKey(Key, Ppk);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    std::printf("  conversion failed: %s\n", E.what());
  }
  Check(Message.find("client.ppk") != std::string::npos, "the conversion says where it saved it");
  Check(CoreBridge::CheckKey(Ppk).Status == CoreBridge::TKeyCheck::TStatus::Usable, "and the .ppk is usable");
  Check(CoreBridge::CheckKey(Directory + "/missing").Status == CoreBridge::TKeyCheck::TStatus::Unusable,
    "a missing file is not");

  CoreBridge::TSite Site = CoreBridge::NewSite();
  Site.HostName = "127.0.0.1";
  Site.Port = Port;
  Site.UserName = User;
  Site.PrivateKeyFile = Ppk;
  Site.TryAgent = false;
  CoreBridge::TSession Session(Site);
  try
  {
    Session.Open();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    std::printf("  open failed: %s\n", E.what());
  }
  Check(Session.Active(), "the converted key logs in");
  Check(Frontend.Prompts == 0, "with no prompt");
}

void Proxy(int Port, int ProxyPort, TFrontend & Frontend)
{
  CoreBridge::TSite Site = CoreBridge::NewSite();
  Site.HostName = "127.0.0.1";
  Site.Port = Port;
  Site.UserName = "linscp";
  Site.Password = "secret";
  Site.Proxy = CoreBridge::TSite::TProxy::Socks5;
  Site.ProxyHost = "127.0.0.1";
  Site.ProxyPort = ProxyPort;
  Site.ProxyUserName = "proxyuser";
  Site.ProxyPassword = "proxypass";
  // The server is local, which a proxy is otherwise not consulted for.
  Site.ProxyLocalhost = true;
  CoreBridge::TSession Session(Site);
  try
  {
    Session.Open();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    std::printf("  open failed: %s\n", E.what());
  }
  Check(Session.Active(), "a site connects through a SOCKS5 proxy with a password");
  Check(Frontend.Prompts == 0, "with no prompt");
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
  else if ((std::strcmp(argv[2], "keys") == 0) && (argc >= 7))
  {
    Keys(Port, argv[4], argv[5], argv[6], Frontend);
  }
  else if ((std::strcmp(argv[2], "proxy") == 0) && (argc >= 5))
  {
    Proxy(Port, std::atoi(argv[4]), Frontend);
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
