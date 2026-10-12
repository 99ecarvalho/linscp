#include <vcl.h>
#pragma hdrstop

#include "DirectoryWatcher.h"

#include <Common.h>
#include <Exceptions.h>
#include <SysUtils.hpp>
#include <TextsWin.h>

#include <map>
#include <string>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <cerrno>
  #include <cstring>
  #include <dirent.h>
  #include <poll.h>
  #include <sys/inotify.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

namespace {

UnicodeString WithoutSlash(UnicodeString Path)
{
  while ((Path.Length() > 1) && ((Path[Path.Length()] == L'/') || (Path[Path.Length()] == L'\\')))
  {
    Path.SetLength(Path.Length() - 1);
  }
  return Path;
}

}  // namespace

struct TDirectoryWatcher::TImpl
{
  UnicodeString Root;
  bool Recursive = true;
  TFilter Filter;
  UnicodeString Error;
  UnicodeString ErrorDirectory;
  // Changed directories: when each last changed, and the order they did.
  std::map<UnicodeString, DWORD> LastChange;
  std::vector<UnicodeString> Order;

  void Changed(const UnicodeString & Directory)
  {
    if (LastChange.find(Directory) == LastChange.end())
    {
      Order.push_back(Directory);
    }
    LastChange[Directory] = GetTickCount();
  }

  bool Wanted(const UnicodeString & Directory) const
  {
    return !Filter || Filter(Directory);
  }

#if defined(_WIN32)
  HANDLE Handle = INVALID_HANDLE_VALUE;
  HANDLE Event = NULL;
  OVERLAPPED Overlapped{};
  alignas(DWORD) unsigned char Buffer[64 * 1024];
  int Count = 0;

  void Issue()
  {
    ResetEvent(Event);
    Overlapped = OVERLAPPED{};
    Overlapped.hEvent = Event;
    const DWORD Filters = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
      (Recursive ? FILE_NOTIFY_CHANGE_DIR_NAME : 0);
    if (!ReadDirectoryChangesW(Handle, Buffer, sizeof(Buffer), Recursive ? TRUE : FALSE, Filters, NULL,
          &Overlapped, NULL))
    {
      Error = LastSysErrorMessage();
      ErrorDirectory = Root;
    }
  }

  void CountDirectories(const UnicodeString & Directory)
  {
    Count++;
    if (!Recursive)
    {
      return;
    }
    TSearchRec Rec;
    if (FindFirst(IncludeTrailingBackslash(Directory) + L"*", faAnyFile, Rec) == 0)
    {
      do
      {
        if (((Rec.Attr & faDirectory) != 0) && (Rec.Name != L".") && (Rec.Name != L".."))
        {
          const UnicodeString Sub = IncludeTrailingBackslash(Directory) + Rec.Name;
          if (Wanted(Sub))
          {
            CountDirectories(Sub);
          }
        }
      }
      while (FindNext(Rec) == 0);
      FindClose(Rec);
    }
  }

  // Whether every directory from the root down to Directory is wanted.
  bool WantedPath(const UnicodeString & Directory) const
  {
    UnicodeString Path = Root;
    UnicodeString Rest = Directory.SubString(Root.Length() + 2, Directory.Length());
    while (!Rest.IsEmpty())
    {
      const int P = Rest.Pos(L"\\");
      const UnicodeString Part = (P > 0) ? Rest.SubString(1, P - 1) : Rest;
      Rest = (P > 0) ? Rest.SubString(P + 1, Rest.Length()) : UnicodeString();
      Path = IncludeTrailingBackslash(Path) + Part;
      if (!Wanted(Path))
      {
        return false;
      }
    }
    return true;
  }
#else
  int Fd = -1;
  std::map<int, UnicodeString> Watches;
  std::map<UnicodeString, int> ByPath;

  void Add(const UnicodeString & Directory)
  {
    if (ByPath.find(Directory) != ByPath.end())
    {
      return;
    }
    const UTF8String Native(Directory);
    const int Wd = inotify_add_watch(Fd, Native.c_str(),
      IN_CREATE | IN_DELETE | IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO |
      IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR);
    if (Wd < 0)
    {
      // A directory gone again before we got to it is no error.
      if (errno != ENOENT)
      {
        Error = UnicodeString(std::strerror(errno));
        ErrorDirectory = Directory;
      }
      return;
    }
    Watches[Wd] = Directory;
    ByPath[Directory] = Wd;
    if (!Recursive)
    {
      return;
    }
    DIR * D = opendir(Native.c_str());
    if (D == NULL)
    {
      return;
    }
    while (const dirent * E = readdir(D))
    {
      if ((std::strcmp(E->d_name, ".") == 0) || (std::strcmp(E->d_name, "..") == 0))
      {
        continue;
      }
      const UnicodeString Sub = Directory + L"/" + UTF8ToString(E->d_name);
      struct stat St;
      if ((stat(UTF8String(Sub).c_str(), &St) == 0) && S_ISDIR(St.st_mode) && Wanted(Sub))
      {
        Add(Sub);
      }
    }
    closedir(D);
  }

  void Remove(int Wd)
  {
    const auto It = Watches.find(Wd);
    if (It != Watches.end())
    {
      ByPath.erase(It->second);
      Watches.erase(It);
    }
  }
#endif
};

TDirectoryWatcher::TDirectoryWatcher(const UnicodeString & Root, bool Recursive, TFilter Filter) :
  F(new TImpl())
{
  F->Root = WithoutSlash(Root);
  F->Recursive = Recursive;
  F->Filter = std::move(Filter);
#if defined(_WIN32)
  F->Handle = CreateFileW(F->Root.c_str(), FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
  if (F->Handle == INVALID_HANDLE_VALUE)
  {
    throw Exception(FMTLOAD(WATCH_ERROR_DIRECTORY, (F->Root)) + L"\n" + LastSysErrorMessage());
  }
  F->Event = CreateEventW(NULL, TRUE, FALSE, NULL);
  F->CountDirectories(F->Root);
  F->Issue();
#else
  F->Fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (F->Fd < 0)
  {
    throw Exception(LoadStr(WATCH_ERROR_GENERAL) + L"\n" + UnicodeString(std::strerror(errno)));
  }
  F->Add(F->Root);
  if (F->Watches.empty())
  {
    const UnicodeString Reason = F->Error;
    close(F->Fd);
    F->Fd = -1;
    throw Exception(FMTLOAD(WATCH_ERROR_DIRECTORY, (F->Root)) + L"\n" + Reason);
  }
#endif
  if (!F->Error.IsEmpty())
  {
    throw Exception(FMTLOAD(WATCH_ERROR_DIRECTORY, (F->ErrorDirectory)) + L"\n" + F->Error);
  }
}

TDirectoryWatcher::~TDirectoryWatcher()
{
#if defined(_WIN32)
  if (F->Handle != INVALID_HANDLE_VALUE)
  {
    CancelIo(F->Handle);
    DWORD Ignored;
    GetOverlappedResult(F->Handle, &F->Overlapped, &Ignored, TRUE);
    CloseHandle(F->Handle);
  }
  if (F->Event != NULL)
  {
    CloseHandle(F->Event);
  }
#else
  if (F->Fd >= 0)
  {
    close(F->Fd);
  }
#endif
}

int TDirectoryWatcher::Directories() const
{
#if defined(_WIN32)
  return F->Count;
#else
  return static_cast<int>(F->Watches.size());
#endif
}

void TDirectoryWatcher::Wait(unsigned int Timeout)
{
  if (!F->Error.IsEmpty())
  {
    Sleep(Timeout);
    return;
  }
#if defined(_WIN32)
  if (WaitForSingleObject(F->Event, Timeout) != WAIT_OBJECT_0)
  {
    return;
  }
  DWORD Bytes = 0;
  if (!GetOverlappedResult(F->Handle, &F->Overlapped, &Bytes, FALSE))
  {
    F->Error = LastSysErrorMessage();
    F->ErrorDirectory = F->Root;
    return;
  }
  if (Bytes == 0)
  {
    // The buffer overflowed: something changed, but not said what.
    F->Changed(F->Root);
  }
  else
  {
    const unsigned char * P = F->Buffer;
    for (;;)
    {
      const FILE_NOTIFY_INFORMATION * Info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(P);
      const UnicodeString Name(Info->FileName, static_cast<int>(Info->FileNameLength / sizeof(wchar_t)));
      const int Slash = Name.LastDelimiter(L"\\");
      const UnicodeString Directory =
        (Slash > 0) ? IncludeTrailingBackslash(F->Root) + Name.SubString(1, Slash - 1) : F->Root;
      if (F->WantedPath(Directory))
      {
        F->Changed(Directory);
      }
      if (Info->NextEntryOffset == 0)
      {
        break;
      }
      P += Info->NextEntryOffset;
    }
    // Directories may have come or gone.
    if (F->Recursive)
    {
      F->Count = 0;
      F->CountDirectories(F->Root);
    }
  }
  F->Issue();
#else
  pollfd Poll = { F->Fd, POLLIN, 0 };
  if (poll(&Poll, 1, static_cast<int>(Timeout)) <= 0)
  {
    return;
  }
  alignas(inotify_event) char Buffer[64 * 1024];
  for (;;)
  {
    const ssize_t N = read(F->Fd, Buffer, sizeof(Buffer));
    if (N <= 0)
    {
      break;
    }
    for (ssize_t Offset = 0; Offset < N;)
    {
      const inotify_event * E = reinterpret_cast<const inotify_event *>(Buffer + Offset);
      Offset += static_cast<ssize_t>(sizeof(inotify_event) + E->len);
      if (E->mask & IN_Q_OVERFLOW)
      {
        F->Changed(F->Root);
        continue;
      }
      const auto It = F->Watches.find(E->wd);
      if (It == F->Watches.end())
      {
        continue;
      }
      const UnicodeString Directory = It->second;
      if (E->mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF))
      {
        if (Directory == F->Root)
        {
          F->Error = LoadStr(WATCH_ERROR_GENERAL);
          F->ErrorDirectory = F->Root;
        }
        F->Remove(E->wd);
        continue;
      }
      if ((E->mask & IN_ISDIR) && (E->len > 0))
      {
        const UnicodeString Sub = Directory + L"/" + UTF8ToString(E->name);
        if (!F->Wanted(Sub))
        {
          continue;  // a change to a directory not watched
        }
        if (F->Recursive && (E->mask & (IN_CREATE | IN_MOVED_TO)))
        {
          F->Add(Sub);
        }
      }
      F->Changed(Directory);
    }
  }
#endif
}

std::vector<UnicodeString> TDirectoryWatcher::TakeSettled(unsigned int Delay)
{
  std::vector<UnicodeString> Result;
  const DWORD Now = GetTickCount();
  for (auto It = F->Order.begin(); It != F->Order.end();)
  {
    const auto Last = F->LastChange.find(*It);
    if (static_cast<DWORD>(Now - Last->second) >= Delay)
    {
      Result.push_back(*It);
      F->LastChange.erase(Last);
      It = F->Order.erase(It);
    }
    else
    {
      ++It;
    }
  }
  return Result;
}

UnicodeString TDirectoryWatcher::Error() const
{
  return F->Error;
}

UnicodeString TDirectoryWatcher::ErrorDirectory() const
{
  return F->ErrorDirectory;
}
