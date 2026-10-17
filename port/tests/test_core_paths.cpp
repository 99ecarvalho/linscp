// Core's local paths built from literals, with the platform's delimiter
// (core-posix-local-path-delimiters.patch), and local paths compared with
// the platform's case rule (core-path-comparison-case.patch).
//
// On POSIX, run in a home of its own (HOME and XDG_CONFIG_HOME set before
// core starts): the default AWS files are read from its ~/.aws, FileZilla's
// sites from its ~/.config/filezilla. On Windows the paths are upstream's.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <Common.h>
#include <Configuration.h>
#include <CopyParam.h>
#include <CoreMain.h>
#include <Interface.h>
#include <S3FileSystem.h>
#include <SessionData.h>
#include "HeadlessMain.h"

#if !defined(_WIN32)
#include <filesystem>
#include <fstream>
#include <unistd.h>
#endif

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

#if !defined(_WIN32)
void WriteFile(const std::filesystem::path & Path, const char * Text)
{
  std::filesystem::create_directories(Path.parent_path());
  std::ofstream(Path) << Text;
}
#endif

}  // namespace

int main()
{
#if !defined(_WIN32)
  char Template[] = "/tmp/linscp-core-paths-XXXXXX";
  const std::filesystem::path Home = mkdtemp(Template);
  setenv("HOME", Home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", (Home / ".config").c_str(), 1);
  unsetenv("AWS_SHARED_CREDENTIALS_FILE");
  unsetenv("AWS_CONFIG_FILE");
  WriteFile(Home / ".aws" / "credentials", "[linscp-profile]\naws_access_key_id = AKIAEXAMPLE\naws_secret_access_key = example\n");
  WriteFile(Home / ".config" / "filezilla" / "sitemanager.xml",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<FileZilla3><Servers><Server><Host>fz.example.com</Host><Port>21</Port><Protocol>0</Protocol>"
    "<Type>0</Type><User>fzuser</User><Logontype>1</Logontype><Name>linscp-fz-site</Name></Server></Servers></FileZilla3>\n");
#endif
  try
  {
    HeadlessInitialize();

    // ValidLocalPath: a remote path's components (as FromUnixPath gives
    // them), one local directory each.
    TCopyParamType CopyParam;
    const UnicodeString Joined = CopyParam.ValidLocalPath(L"home\\user\\dir");
    CHECK(Joined == IncludeTrailingBackslash(IncludeTrailingBackslash(L"home") + L"user") + L"dir");

    // IsPathToSameFile: case matters where the file system's does.
#if defined(_WIN32)
    CHECK(IsPathToSameFile(L"C:\\Temp\\WinSCP.ini", L"c:\\temp\\winscp.ini"));
#else
    CHECK(IsPathToSameFile(L"/tmp/WinSCP.ini", L"/tmp/WinSCP.ini"));
    CHECK(!IsPathToSameFile(L"/tmp/WinSCP.ini", L"/tmp/winscp.ini"));

    // The default AWS credentials, from ~/.aws.
    std::unique_ptr<TStrings> Profiles(GetS3Profiles());
    CHECK(Profiles->IndexOf(L"linscp-profile") >= 0);

    // FileZilla's sites, from ~/.config/filezilla.
    UnicodeString Error;
    std::unique_ptr<TStoredSessionList> FileZilla(Configuration->SelectFilezillaSessionsForImport(StoredSessions, Error));
    CHECK(Error.IsEmpty());
    CHECK((FileZilla->Count == 1) && (FileZilla->Sessions[0]->HostName == L"fz.example.com"));
    if (!Error.IsEmpty())
    {
      std::printf("FileZilla: %s\n", UTF8String(Error).c_str());
    }
#endif
    HeadlessFinalize();
  }
  catch (Exception & E)
  {
    std::printf("FAIL exception: %s\n", UTF8String(E.Message).c_str());
    ++Failures;
  }
#if !defined(_WIN32)
  std::filesystem::remove_all(Home);
#endif
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
