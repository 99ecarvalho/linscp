#include "vcl_sysutils.h"

#include <cstring>

#if !defined(_WIN32)
  #include <unistd.h>
#endif

#include "vcl_bytestring.h"

const UnicodeString EmptyStr;

#if defined(_WIN32)

#include <windows.h>
#include "vcl_win32_undef.h"

namespace {

// Translate a Win32 find result into Delphi's record shape.
void FillFromFindData(const WIN32_FIND_DATAW & Data, TSearchRec & F)
{
  F.Name = UnicodeString(Data.cFileName);
  F.Attr = static_cast<int>(Data.dwFileAttributes);
  F.Size = (static_cast<__int64>(Data.nFileSizeHigh) << 32) |
           static_cast<__int64>(Data.nFileSizeLow);

  SYSTEMTIME Sys{};
  FILETIME Local{};
  if (FileTimeToLocalFileTime(&Data.ftLastWriteTime, &Local) != 0 &&
      FileTimeToSystemTime(&Local, &Sys) != 0)
  {
    F.TimeStamp = EncodeDateTime(
      static_cast<Word>(Sys.wYear), static_cast<Word>(Sys.wMonth),
      static_cast<Word>(Sys.wDay), static_cast<Word>(Sys.wHour),
      static_cast<Word>(Sys.wMinute), static_cast<Word>(Sys.wSecond),
      static_cast<Word>(Sys.wMilliseconds));
  }
}

// Delphi skips entries carrying an attribute the caller did not ask for.
bool Matches(const TSearchRec & F, int Wanted)
{
  const int Special = faHidden | faSysFile | faDirectory;
  return (F.Attr & Special & ~Wanted) == 0;
}

}  // namespace

int FindFirst(const UnicodeString & Path, int Attr, TSearchRec & F)
{
  WIN32_FIND_DATAW Data{};
  HANDLE H = FindFirstFileW(Path.c_str(), &Data);
  if (H == INVALID_HANDLE_VALUE)
  {
    F.FindHandle = nullptr;
    return static_cast<int>(GetLastError());
  }
  F.FindHandle = H;
  F.ExcludeAttr = ~Attr & (faHidden | faSysFile | faDirectory);
  FillFromFindData(Data, F);
  if (Matches(F, Attr))
  {
    return 0;
  }
  return FindNext(F);
}

int FindNext(TSearchRec & F)
{
  if (F.FindHandle == nullptr)
  {
    return ERROR_INVALID_HANDLE;
  }
  const int Wanted = ~F.ExcludeAttr & (faHidden | faSysFile | faDirectory);
  WIN32_FIND_DATAW Data{};
  while (FindNextFileW(static_cast<HANDLE>(F.FindHandle), &Data) != 0)
  {
    FillFromFindData(Data, F);
    if (Matches(F, Wanted))
    {
      return 0;
    }
  }
  return static_cast<int>(GetLastError());
}

void FindClose(TSearchRec & F)
{
  if (F.FindHandle != nullptr)
  {
    ::FindClose(static_cast<HANDLE>(F.FindHandle));
    F.FindHandle = nullptr;
  }
}

bool FileExists(const UnicodeString & FileName)
{
  const DWORD A = GetFileAttributesW(FileName.c_str());
  return (A != INVALID_FILE_ATTRIBUTES) &&
         ((A & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool DirectoryExists(const UnicodeString & Name)
{
  const DWORD A = GetFileAttributesW(Name.c_str());
  return (A != INVALID_FILE_ATTRIBUTES) &&
         ((A & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

#else   // POSIX

#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <ctime>
#include <string>

namespace {

// Iteration state for one FindFirst/FindNext sequence.
struct TPosixFindState
{
  DIR * Dir = nullptr;
  std::string Directory;
  std::string Pattern;
  int Wanted = 0;
};

std::string ToBytes(const UnicodeString & S)
{
  return linscp::Utf16ToUtf8(S);
}

int AttrFromStat(const struct stat & St, const std::string & Name)
{
  int Attr = 0;
  if (S_ISDIR(St.st_mode))  { Attr |= faDirectory; }
  if (S_ISLNK(St.st_mode))  { Attr |= faSymLink; }
  if ((St.st_mode & S_IWUSR) == 0) { Attr |= faReadOnly; }
  // POSIX has no hidden bit; a leading dot is the established convention and
  // is what users expect "show hidden files" to act on.
  if (!Name.empty() && Name[0] == '.') { Attr |= faHidden; }
  if (Attr == 0) { Attr = faNormal; }
  return Attr;
}

TDateTime TimeStampFromStat(const struct stat & St)
{
  std::tm Local{};
  const std::time_t T = St.st_mtime;
  localtime_r(&T, &Local);
  return EncodeDateTime(
    static_cast<Word>(Local.tm_year + 1900), static_cast<Word>(Local.tm_mon + 1),
    static_cast<Word>(Local.tm_mday), static_cast<Word>(Local.tm_hour),
    static_cast<Word>(Local.tm_min), static_cast<Word>(Local.tm_sec), 0);
}

bool Matches(int Attr, int Wanted)
{
  const int Special = faHidden | faSysFile | faDirectory;
  return (Attr & Special & ~Wanted) == 0;
}

// Advance to the next entry matching the pattern and the requested attributes.
int Advance(TSearchRec & F)
{
  auto * State = static_cast<TPosixFindState *>(F.FindState);
  if (State == nullptr || State->Dir == nullptr)
  {
    return EBADF;
  }
  for (;;)
  {
    errno = 0;
    const struct dirent * E = readdir(State->Dir);
    if (E == nullptr)
    {
      return errno != 0 ? errno : ENOENT;   // ENOENT means "no more entries"
    }
    // Case-sensitive, matching the host filesystem rather than Windows.
    if (fnmatch(State->Pattern.c_str(), E->d_name, 0) != 0)
    {
      continue;
    }
    const std::string Full = State->Directory + "/" + E->d_name;
    struct stat St{};
    if (lstat(Full.c_str(), &St) != 0)
    {
      continue;                              // vanished mid-scan; skip it
    }
    const int Attr = AttrFromStat(St, E->d_name);
    if (!Matches(Attr, State->Wanted))
    {
      continue;
    }
    F.Name = linscp::Utf8ToUtf16(E->d_name, std::strlen(E->d_name));
    F.Attr = Attr;
    F.Size = static_cast<__int64>(St.st_size);
    F.TimeStamp = TimeStampFromStat(St);
    return 0;
  }
}

}  // namespace

int FindFirst(const UnicodeString & Path, int Attr, TSearchRec & F)
{
  FindClose(F);

  const std::string P = ToBytes(Path);
  const std::size_t Slash = P.find_last_of('/');
  auto * State = new TPosixFindState();
  State->Directory = (Slash == std::string::npos) ? "." : P.substr(0, Slash);
  State->Pattern = (Slash == std::string::npos) ? P : P.substr(Slash + 1);
  if (State->Directory.empty())
  {
    State->Directory = "/";
  }
  State->Wanted = Attr;
  State->Dir = opendir(State->Directory.c_str());
  if (State->Dir == nullptr)
  {
    const int Err = errno;
    delete State;
    return Err != 0 ? Err : ENOENT;
  }

  F.FindState = State;
  F.FindHandle = State;          // core only ever tests this for non-null
  F.ExcludeAttr = ~Attr & (faHidden | faSysFile | faDirectory);
  const int R = Advance(F);
  if (R != 0)
  {
    FindClose(F);
  }
  return R;
}

int FindNext(TSearchRec & F)
{
  return Advance(F);
}

void FindClose(TSearchRec & F)
{
  auto * State = static_cast<TPosixFindState *>(F.FindState);
  if (State != nullptr)
  {
    if (State->Dir != nullptr)
    {
      closedir(State->Dir);
    }
    delete State;
  }
  F.FindState = nullptr;
  F.FindHandle = nullptr;
}

bool FileExists(const UnicodeString & FileName)
{
  struct stat St{};
  return stat(ToBytes(FileName).c_str(), &St) == 0 && !S_ISDIR(St.st_mode);
}

bool DirectoryExists(const UnicodeString & Name)
{
  struct stat St{};
  return stat(ToBytes(Name).c_str(), &St) == 0 && S_ISDIR(St.st_mode);
}

#endif

// --- Delphi's handle-based file API ----------------------------------------
//
// Implemented over THandleStream so the platform differences live in one place
// rather than being repeated here.

THandle FileOpen(const UnicodeString & FileName, unsigned Mode)
{
  try
  {
    // Deliberately leaks the stream object's handle to the caller, which is
    // what a handle-returning API means: FileClose releases it.
    auto * Stream = new TFileStream(FileName, static_cast<Word>(Mode));
    const THandle Handle = Stream->Handle;
    // Detach: the handle now belongs to the caller, so the stream must not
    // close it on destruction.
    Stream->DetachHandle();
    delete Stream;
    return Handle;
  }
  catch (const EStreamError &)
  {
    return InvalidHandleValue;
  }
}

THandle FileCreate(const UnicodeString & FileName)
{
  return FileOpen(FileName, fmCreate);
}

void FileClose(THandle Handle)
{
  if (Handle != InvalidHandleValue)
  {
#if defined(_WIN32)
    CloseHandle(reinterpret_cast<HANDLE>(Handle));
#else
    ::close(static_cast<int>(static_cast<std::intptr_t>(Handle)));
#endif
  }
}

__int64 FileSeek(THandle Handle, __int64 Offset, int Origin)
{
  THandleStream Stream(Handle);
  const __int64 Result = Stream.Seek(Offset, static_cast<TSeekOrigin>(Origin));
  Stream.DetachHandle();
  return Result;
}

int FileRead(THandle Handle, void * Buffer, int Count)
{
  THandleStream Stream(Handle);
  const int Result = Stream.Read(Buffer, Count);
  Stream.DetachHandle();
  return Result;
}

int FileWrite(THandle Handle, const void * Buffer, int Count)
{
  THandleStream Stream(Handle);
  const int Result = Stream.Write(Buffer, Count);
  Stream.DetachHandle();
  return Result;
}

int FileRead(THandle Handle, TBytes Buffer, int Offset, int Count)
{
  if (Offset < 0 || Count <= 0 || Offset + Count > Buffer.Length())
  {
    return 0;
  }
  return FileRead(Handle, Buffer.data() + Offset, Count);
}

int FileWrite(THandle Handle, const TBytes Buffer, int Offset, int Count)
{
  if (Offset < 0 || Count <= 0 || Offset + Count > Buffer.Length())
  {
    return 0;
  }
  return FileWrite(Handle, Buffer.data() + Offset, Count);
}
