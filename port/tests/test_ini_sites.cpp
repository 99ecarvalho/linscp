// Sites stored in WinSCP's INI file, read back through core's storage the
// way CoreLoad reads them: TIniFileStorage, the Sessions subkey, its subkey
// names, TStoredSessionList::Load. On POSIX the INI file is where all
// configuration lives (there is no registry), so this is the path every
// stored site takes there.
//
// Last argument: a directory to write the INI file in.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>
#include <memory>

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <HierarchicalStorage.h>
#include <SessionData.h>
#include "HeadlessMain.h"

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

}  // namespace

int main()
{
  try
  {
    HeadlessInitialize();
    const UnicodeString Path = IncludeTrailingBackslash(ParamStr(ParamCount())) + L"sites.ini";
    {
      std::unique_ptr<TStrings> Lines(new TStringList());
      Lines->Add(L"[Sessions\\my%20site]");
      Lines->Add(L"HostName=example.com");
      Lines->Add(L"PortNumber=2222");
      Lines->Add(L"UserName=bob");
      Lines->Add(L"[Sessions\\folder/other]");
      Lines->Add(L"HostName=other.example.com");
      Lines->SaveToFile(Path);
    }

    std::unique_ptr<THierarchicalStorage> Storage(TIniFileStorage::CreateFromPath(Path));
    Storage->AccessMode = smRead;
    CHECK(Storage->KeyExists(Configuration->StoredSessionsSubKey));
    CHECK(Storage->OpenSubKey(Configuration->StoredSessionsSubKey, false));
    std::unique_ptr<TStrings> Names(new TStringList());
    Storage->GetSubKeyNames(Names.get());
    CHECK(Names->Count == 2);
    CHECK(Names->IndexOf(L"my site") >= 0);
    CHECK(Names->IndexOf(L"folder/other") >= 0);

    std::unique_ptr<TStoredSessionList> Sites(new TStoredSessionList());
    Sites->Load(Storage.get());
    TSessionData * Site = dynamic_cast<TSessionData *>(Sites->FindByName(L"my site"));
    CHECK(Site != NULL);
    if (Site != NULL)
    {
      CHECK(Site->HostName == UnicodeString(L"example.com"));
      CHECK(Site->PortNumber == 2222);
      CHECK(Site->UserName == UnicodeString(L"bob"));
    }
    CHECK(Sites->FindByName(L"folder/other") != NULL);
    Storage.reset();
    Sites.reset();
    HeadlessFinalize();
  }
  catch (Exception & E)
  {
    std::printf("FAIL exception: %s\n", UTF8String(E.Message).c_str());
    ++Failures;
  }
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
