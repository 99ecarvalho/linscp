#include <vcl.h>
#pragma hdrstop

#include "DiscMon.hpp"

#include <Common.h>
#include <DirectoryWatcher.h>
#include <SysUtils.hpp>

#include <algorithm>
#include <vector>

namespace Discmon
{

namespace
{

// The monitors open, which Pump polls.
std::vector<TDiscMonitor *> & Monitors()
{
  static std::vector<TDiscMonitor *> Result;
  return Result;
}

bool IsOpen(const TDiscMonitor * Monitor)
{
  const std::vector<TDiscMonitor *> & List = Monitors();
  return std::find(List.begin(), List.end(), Monitor) != List.end();
}

}  // namespace

TDiscMonitor::TDiscMonitor(TComponent * AOwner) :
  TComponent(AOwner),
  FDirectories(new TStringList())
{
  Filters << moFilename;
}

TDiscMonitor::~TDiscMonitor()
{
  Close();
}

TStrings * TDiscMonitor::GetDirectories() const
{
  return FDirectories.get();
}

bool TDiscMonitor::Filter(const UnicodeString & Directory)
{
  bool Add = true;
  if (OnFilter != NULL)
  {
    OnFilter(this, Directory, Add);
  }
  return Add;
}

// DiscMon.pas's AddDirectory: the tree below Directory, depth first.
void TDiscMonitor::Scan(const UnicodeString & Directory)
{
  const UnicodeString Path = IncludeTrailingBackslash(Directory);
  TSearchRec SearchRec;
  if (FindFirst(Path + L"*", faReadOnly | faHidden | faSysFile | faDirectory | faArchive, SearchRec) != 0)
  {
    return;
  }
  // FindClose however the filter or the question leaves.
  std::unique_ptr<TSearchRec, void (*)(TSearchRec *)> Closer(&SearchRec, [](TSearchRec * Rec) { FindClose(*Rec); });
  do
  {
    if ((SearchRec.Name != L".") && (SearchRec.Name != L"..") && FLAGSET(SearchRec.Attr, faDirectory))
    {
      const UnicodeString FileName = Path + SearchRec.Name;
      if (Filter(FileName))
      {
        FDirectories->Add(FileName);
        if ((MaxDirectories >= 0) && (FDirectories->Count > MaxDirectories) && (OnTooManyDirectories != NULL))
        {
          OnTooManyDirectories(NULL, MaxDirectories);
        }
        Scan(FileName);
      }
    }
  }
  while (FindNext(SearchRec) == 0);
}

void TDiscMonitor::AddDirectory(UnicodeString Directory, bool SubDirs)
{
  DebugAssert(FWatcher == NULL);
  DebugAssert(FRoot.IsEmpty());
  FDirectories->Clear();
  FRoot = Directory;
  FRecursive = SubDirs;
  if (!Directory.IsEmpty())
  {
    FDirectories->Add(Directory);
    if (SubDirs)
    {
      Scan(Directory);
    }
  }
}

void TDiscMonitor::Open()
{
  if ((FWatcher != NULL) || FRoot.IsEmpty())
  {
    return;
  }
  // What AddDirectory left out the watcher leaves out too: it asks the same
  // filter, for the directories there and those that appear.
  FWatcher.reset(new TDirectoryWatcher(FRoot, FRecursive,
    [this](const UnicodeString & Directory) { return Filter(Directory); }));
  FWatched = FWatcher->Directories();
  Monitors().push_back(this);
}

void TDiscMonitor::Close()
{
  std::vector<TDiscMonitor *> & List = Monitors();
  List.erase(std::remove(List.begin(), List.end(), this), List.end());
  FWatcher.reset();
}

void TDiscMonitor::Pump()
{
  // An event may close or free any monitor, this one included.
  const std::vector<TDiscMonitor *> List = Monitors();
  for (TDiscMonitor * Monitor : List)
  {
    if (IsOpen(Monitor))
    {
      Monitor->Poll();
    }
  }
}

bool TDiscMonitor::DoSynchronize(void (TDiscMonitor::*Method)())
{
  if (OnSynchronize != NULL)
  {
    OnSynchronize(this, MakeClosure(this, Method));
  }
  else
  {
    (this->*Method)();
  }
  return IsOpen(this);
}

bool TDiscMonitor::Poll()
{
  FWatcher->Wait(0);
  if (!FWatcher->Error().IsEmpty())
  {
    // As upstream's thread, which ends after OnInvalid.
    FInvalidDirectory = FWatcher->ErrorDirectory();
    FInvalidError = FWatcher->Error();
    if (DoSynchronize(&TDiscMonitor::InformInvalid))
    {
      Close();
    }
    return false;
  }
  for (const UnicodeString & Directory : FWatcher->TakeSettled(static_cast<unsigned int>(std::max(ChangeDelay, 0))))
  {
    // As upstream's: a directory gone again by now is not reported.
    if (DirectoryExists(Directory))
    {
      FNotifiedDirectory = Directory;
      // New directories are watched as they appear: the watcher needs no
      // rescan when the event says the subdirectories changed.
      FSubdirsChanged = false;
      if (!DoSynchronize(&TDiscMonitor::InformChange))
      {
        return false;
      }
    }
  }
  if (FWatcher->Directories() != FWatched)
  {
    FWatched = FWatcher->Directories();
    if (!DoSynchronize(&TDiscMonitor::InformDirectoriesChange))
    {
      return false;
    }
  }
  return true;
}

void TDiscMonitor::InformChange()
{
  if (OnChange != NULL)
  {
    OnChange(this, FNotifiedDirectory, FSubdirsChanged);
  }
}

void TDiscMonitor::InformInvalid()
{
  if (OnInvalid != NULL)
  {
    OnInvalid(this, FInvalidDirectory, FInvalidError);
  }
}

void TDiscMonitor::InformDirectoriesChange()
{
  if (OnDirectoriesChange != NULL)
  {
    OnDirectoriesChange(this, FWatched);
  }
}

}  // namespace Discmon
