#include "vcl_sysutils.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined(_WIN32)
  #include <unistd.h>
#endif

#include "vcl_bytestring.h"
#include "vcl_strutils.h"

const UnicodeString EmptyStr;

#if defined(_WIN32)

#include <windows.h>
#include "vcl_win32_undef.h"

namespace {

// Translate a Win32 find result into Delphi's record shape.
void FillFromFindData(const WIN32_FIND_DATAW & Data, TSearchRec & F)
{
  F.FindData = Data;
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

// The platform layer's mapping, so FileGetAttr and GetFileAttributes agree;
// Delphi's fa* constants are Win32's attribute bits.
int AttrFromStat(const struct stat & St, const std::string & Name)
{
  static_assert((faReadOnly == FILE_ATTRIBUTE_READONLY) && (faHidden == FILE_ATTRIBUTE_HIDDEN) &&
                (faDirectory == FILE_ATTRIBUTE_DIRECTORY) && (faSymLink == FILE_ATTRIBUTE_REPARSE_POINT) &&
                (faNormal == FILE_ATTRIBUTE_NORMAL), "fa* must equal FILE_ATTRIBUTE_*");
  return static_cast<int>(linscp::AttributesFromStat(St, Name.c_str()));
}

FILETIME FileTimeFromTimespec(const struct timespec & T)
{
  // 100 ns ticks since 1601-01-01, the FILETIME epoch, 11644473600 s before
  // the Unix one.
  const unsigned long long Ticks =
    (static_cast<unsigned long long>(T.tv_sec) + 11644473600ULL) * 10000000ULL +
    static_cast<unsigned long long>(T.tv_nsec) / 100ULL;
  FILETIME Result;
  Result.dwLowDateTime = static_cast<DWORD>(Ticks & 0xFFFFFFFFULL);
  Result.dwHighDateTime = static_cast<DWORD>(Ticks >> 32);
  return Result;
}

void FillFindData(const struct stat & St, int Attr, const UnicodeString & Name,
                  WIN32_FIND_DATAW & Data)
{
  Data = WIN32_FIND_DATAW();
  Data.dwFileAttributes = static_cast<DWORD>(Attr);   // fa* are Win32's bits
  Data.ftLastWriteTime = FileTimeFromTimespec(St.st_mtim);
  Data.ftLastAccessTime = FileTimeFromTimespec(St.st_atim);
  Data.ftCreationTime = Data.ftLastWriteTime;
  const unsigned long long Size = static_cast<unsigned long long>(St.st_size);
  Data.nFileSizeLow = static_cast<DWORD>(Size & 0xFFFFFFFFULL);
  Data.nFileSizeHigh = static_cast<DWORD>(Size >> 32);
  const int N = (Name.Length() < MAX_PATH - 1) ? Name.Length() : MAX_PATH - 1;
  for (int I = 0; I < N; ++I)
  {
    Data.cFileName[I] = Name[I + 1];
  }
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
    // Case-sensitive, matching the host filesystem rather than Windows. But
    // "*.*" is every name, as Windows has it: core lists directories with it
    // (synchronization scans <dir>\*.*), and read as fnmatch reads it, a
    // name without a dot -- a typical directory -- went unseen, so
    // "synchronize remote -delete" deleted it on the server.
    if ((State->Pattern != "*") && (State->Pattern != "*.*") &&
        (fnmatch(State->Pattern.c_str(), E->d_name, 0) != 0))
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
    FillFindData(St, Attr, F.Name, F.FindData);
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
  if (Offset < 0 || Count <= 0 || Offset + Count > Buffer.get_length())
  {
    return 0;
  }
  return FileRead(Handle, Buffer.data() + Offset, Count);
}

int FileWrite(THandle Handle, const TBytes Buffer, int Offset, int Count)
{
  if (Offset < 0 || Count <= 0 || Offset + Count > Buffer.get_length())
  {
    return 0;
  }
  return FileWrite(Handle, Buffer.data() + Offset, Count);
}

namespace linscp {

std::vector<UnicodeString> SplitDelphiCommandLine(const UnicodeString & CommandLine)
{
  std::vector<UnicodeString> Result;
  const int Len = CommandLine.Length();
  int P = 1;
  // 1-based; past the end reads as the terminator, as Delphi's PChar does.
  auto At = [&](int I) -> wchar_t { return (I <= Len) ? CommandLine[I] : L'\0'; };
  for (;;)
  {
    for (;;)
    {
      while ((At(P) != L'\0') && (At(P) <= L' '))
      {
        ++P;
      }
      if ((At(P) == L'"') && (At(P + 1) == L'"'))
      {
        P += 2;
      }
      else
      {
        break;
      }
    }
    UnicodeString Param;
    while (At(P) > L' ')
    {
      if (At(P) == L'"')
      {
        ++P;
        while ((At(P) != L'\0') && (At(P) != L'"'))
        {
          Param += At(P);
          ++P;
        }
        if (At(P) != L'\0')
        {
          ++P;
        }
      }
      else
      {
        Param += At(P);
        ++P;
      }
    }
    if (Param.IsEmpty())
    {
      break;
    }
    Result.push_back(Param);
  }
  return Result;
}

}  // namespace linscp

namespace {

// The program token at 0, then the arguments.
std::vector<UnicodeString> CommandLineParams()
{
#if defined(_WIN32)
  return linscp::SplitDelphiCommandLine(UnicodeString(GetCommandLineW()));
#else
  std::vector<UnicodeString> Result;
  FILE * F = std::fopen("/proc/self/cmdline", "rb");
  if (F == nullptr)
  {
    return Result;
  }
  std::string All;
  char Buf[4096];
  std::size_t N;
  while ((N = std::fread(Buf, 1, sizeof(Buf), F)) > 0)
  {
    All.append(Buf, N);
  }
  std::fclose(F);
  // NUL-separated argv, UTF-8 by convention.
  std::size_t Start = 0;
  while (Start < All.size())
  {
    const std::size_t End = All.find('\0', Start);
    const std::size_t Stop = (End == std::string::npos) ? All.size() : End;
    Result.push_back(linscp::Utf8ToUtf16(All.data() + Start, Stop - Start));
    Start = Stop + 1;
  }
  return Result;
#endif
}

UnicodeString ExecutablePath()
{
#if defined(_WIN32)
  std::vector<wchar_t> Buf(32768);
  const DWORD N = GetModuleFileNameW(nullptr, Buf.data(), static_cast<DWORD>(Buf.size()));
  return UnicodeString(Buf.data(), static_cast<int>(N));
#else
  char Buf[4096];
  const ssize_t N = readlink("/proc/self/exe", Buf, sizeof(Buf));
  if (N <= 0)
  {
    return UnicodeString();
  }
  return linscp::Utf8ToUtf16(Buf, static_cast<std::size_t>(N));
#endif
}

}  // namespace

int ParamCount()
{
  const std::size_t N = CommandLineParams().size();
  return (N > 0) ? static_cast<int>(N - 1) : 0;
}

UnicodeString ParamStr(int Index)
{
  if (Index == 0)
  {
    return ExecutablePath();
  }
  const std::vector<UnicodeString> Params = CommandLineParams();
  if ((Index < 0) || (static_cast<std::size_t>(Index) >= Params.size()))
  {
    return UnicodeString();
  }
  return Params[static_cast<std::size_t>(Index)];
}

int FileSetAttr(const UnicodeString & FileName, int Attr, bool FollowLink)
{
  (void)FollowLink;
#if defined(_WIN32)
  return SetFileAttributesW(FileName.c_str(), static_cast<DWORD>(Attr))
    ? 0 : static_cast<int>(GetLastError());
#else
  const std::string Path = ToBytes(FileName);
  struct stat St{};
  if (stat(Path.c_str(), &St) != 0)
  {
    const DWORD Code = Win32ErrorFromErrno(errno);
    SetLastError(Code);
    return static_cast<int>(Code);
  }
  mode_t Mode = St.st_mode & 07777;
  if ((Attr & faReadOnly) != 0)
  {
    Mode &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
  }
  else
  {
    Mode |= S_IWUSR;
  }
  if (chmod(Path.c_str(), Mode) != 0)
  {
    const DWORD Code = Win32ErrorFromErrno(errno);
    SetLastError(Code);
    return static_cast<int>(Code);
  }
  return 0;
#endif
}

bool ForceDirectories(UnicodeString Dir)
{
  if (Dir.IsEmpty())
  {
    throw EInOutError(UnicodeString(L"Unable to create directory"));
  }
  Dir = ExcludeTrailingPathDelimiter(Dir);
  // Delphi's own guard, which also stops the recursion at a root or drive.
  if ((Dir.Length() < 3) || DirectoryExists(Dir) || (ExtractFilePath(Dir) == Dir))
  {
    return true;
  }
  return ForceDirectories(ExtractFilePath(Dir)) && CreateDir(Dir);
}

int FileGetAttr(const UnicodeString & FileName, bool FollowLink)
{
#if defined(_WIN32)
  (void)FollowLink;
  return static_cast<int>(GetFileAttributesW(FileName.c_str()));
#else
  const std::string Path = ToBytes(FileName);
  struct stat St{};
  if ((FollowLink ? stat(Path.c_str(), &St) : lstat(Path.c_str(), &St)) != 0)
  {
    SetLastError(Win32ErrorFromErrno(errno));
    return -1;
  }
  const std::size_t Slash = Path.rfind('/');
  return AttrFromStat(St, (Slash == std::string::npos) ? Path : Path.substr(Slash + 1));
#endif
}

TDateTime SystemTimeToDateTime(const SYSTEMTIME & SystemTime)
{
  return ComposeDateTime(
    EncodeDate(SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay),
    EncodeTime(SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
               SystemTime.wMilliseconds));
}

bool FileGetSymLinkTarget(const UnicodeString & FileName, UnicodeString & TargetName)
{
#if defined(_WIN32)
  const DWORD Attr = GetFileAttributesW(FileName.c_str());
  if ((Attr == INVALID_FILE_ATTRIBUTES) || ((Attr & FILE_ATTRIBUTE_REPARSE_POINT) == 0))
  {
    return false;
  }
  const HANDLE H = CreateFileW(FileName.c_str(), 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (H == INVALID_HANDLE_VALUE)
  {
    return false;   // dangling
  }
  wchar_t Buf[32768];
  const DWORD N = GetFinalPathNameByHandleW(H, Buf, 32768, FILE_NAME_NORMALIZED);
  CloseHandle(H);
  if ((N == 0) || (N >= 32768))
  {
    return false;
  }
  UnicodeString Path(Buf, static_cast<int>(N));
  // Drop the \\?\ namespace prefix, keeping UNC paths as \\server\share.
  if (Path.SubString(1, 8) == UnicodeString(L"\\\\?\\UNC\\"))
  {
    Path = UnicodeString(L"\\\\") + Path.SubString(9);
  }
  else if (Path.SubString(1, 4) == UnicodeString(L"\\\\?\\"))
  {
    Path = Path.SubString(5);
  }
  TargetName = Path;
  return true;
#else
  const std::string Path = ToBytes(FileName);
  struct stat St{};
  if ((lstat(Path.c_str(), &St) != 0) || !S_ISLNK(St.st_mode))
  {
    return false;
  }
  char * Resolved = realpath(Path.c_str(), nullptr);
  if (Resolved == nullptr)
  {
    return false;   // dangling
  }
  TargetName = linscp::Utf8ToUtf16(Resolved, std::strlen(Resolved));
  std::free(Resolved);
  return true;
#endif
}

UnicodeString ExtractShortPathName(const UnicodeString & FileName)
{
#if defined(_WIN32)
  wchar_t Buf[32768];
  const DWORD N = GetShortPathNameW(FileName.c_str(), Buf, 32768);
  return ((N == 0) || (N >= 32768)) ? UnicodeString() : UnicodeString(Buf, static_cast<int>(N));
#else
  return FileName;
#endif
}

namespace {

// 1601-01-01, the FILETIME epoch, as a TDateTime serial.
constexpr double FileTimeEpochSerial = -109205.0;

FILETIME DateTimeToFileTimeUtc(const TDateTime & DT)
{
  const long long Ms = std::llround((DT.Val() - FileTimeEpochSerial) * 86400000.0);
  const unsigned long long Ticks = static_cast<unsigned long long>(Ms) * 10000ULL;
  FILETIME F;
  F.dwLowDateTime = static_cast<DWORD>(Ticks & 0xFFFFFFFFULL);
  F.dwHighDateTime = static_cast<DWORD>(Ticks >> 32);
  return F;
}

TDateTime FileTimeToDateTimeValue(const FILETIME & F)
{
  SYSTEMTIME S;
  if (FileTimeToSystemTime(&F, &S) == 0)
  {
    return TDateTime();
  }
  return SystemTimeToDateTime(S);
}

TDateTime UtcToLocal(const TDateTime & Utc)
{
  const FILETIME U = DateTimeToFileTimeUtc(Utc);
  FILETIME L;
  return (FileTimeToLocalFileTime(&U, &L) != 0) ? FileTimeToDateTimeValue(L) : Utc;
}

int Digits(const UnicodeString & S, int & I, int Count)
{
  int V = 0;
  for (int K = 0; K < Count; ++K, ++I)
  {
    if ((I > S.Length()) || (S[I] < L'0') || (S[I] > L'9'))
    {
      throw EConvertError(UnicodeString(L"'") + S + L"' is not a valid ISO 8601 date");
    }
    V = V * 10 + (S[I] - L'0');
  }
  return V;
}

}  // namespace

bool FileAge(const UnicodeString & FileName, TDateTime & FileDateTime, bool FollowLink)
{
  FILETIME Write;
#if defined(_WIN32)
  (void)FollowLink;
  WIN32_FILE_ATTRIBUTE_DATA Data;
  if (!GetFileAttributesExW(FileName.c_str(), GetFileExInfoStandard, &Data))
  {
    return false;
  }
  Write = Data.ftLastWriteTime;
#else
  struct stat St{};
  const std::string Path = ToBytes(FileName);
  if ((FollowLink ? stat(Path.c_str(), &St) : lstat(Path.c_str(), &St)) != 0)
  {
    return false;
  }
  Write = FileTimeFromTimespec(St.st_mtim);
#endif
  FILETIME Local;
  if (FileTimeToLocalFileTime(&Write, &Local) == 0)
  {
    return false;
  }
  FileDateTime = FileTimeToDateTimeValue(Local);
  return true;
}

TDateTime ISO8601ToDate(const UnicodeString & AISODate, bool AReturnUTC)
{
  const UnicodeString S = AISODate.Trim();
  int I = 1;
  const int Y = Digits(S, I, 4);
  if ((I > S.Length()) || (S[I++] != L'-')) { Digits(S, I = S.Length() + 1, 1); }
  const int Mo = Digits(S, I, 2);
  if ((I > S.Length()) || (S[I++] != L'-')) { Digits(S, I = S.Length() + 1, 1); }
  const int D = Digits(S, I, 2);
  int H = 0, Mi = 0, Sec = 0, Ms = 0, OffsetMinutes = 0;
  if ((I <= S.Length()) && ((S[I] == L'T') || (S[I] == L't') || (S[I] == L' ')))
  {
    ++I;
    H = Digits(S, I, 2);
    if ((I <= S.Length()) && (S[I] == L':')) { ++I; }
    Mi = Digits(S, I, 2);
    if ((I <= S.Length()) && (S[I] == L':'))
    {
      ++I;
      Sec = Digits(S, I, 2);
      if ((I <= S.Length()) && ((S[I] == L'.') || (S[I] == L',')))
      {
        ++I;
        int Scale = 100;
        while ((I <= S.Length()) && (S[I] >= L'0') && (S[I] <= L'9'))
        {
          Ms += (S[I++] - L'0') * Scale;   // digits past milliseconds are dropped
          Scale /= 10;
        }
      }
    }
    if ((I <= S.Length()) && ((S[I] == L'Z') || (S[I] == L'z')))
    {
      ++I;
    }
    else if ((I <= S.Length()) && ((S[I] == L'+') || (S[I] == L'-')))
    {
      const int Sign = (S[I++] == L'-') ? -1 : 1;
      const int OH = Digits(S, I, 2);
      if ((I <= S.Length()) && (S[I] == L':')) { ++I; }
      const int OM = (I <= S.Length()) ? Digits(S, I, 2) : 0;
      OffsetMinutes = Sign * (OH * 60 + OM);
    }
  }
  TDateTime Date, Time;
  if ((I <= S.Length()) ||
      !TryEncodeDate(static_cast<Word>(Y), static_cast<Word>(Mo), static_cast<Word>(D), Date) ||
      !TryEncodeTime(static_cast<Word>(H), static_cast<Word>(Mi), static_cast<Word>(Sec),
                     static_cast<Word>(Ms), Time))
  {
    throw EConvertError(UnicodeString(L"'") + AISODate + L"' is not a valid ISO 8601 date");
  }
  // Local wall time minus its offset is UTC.
  const TDateTime Utc = IncMinute(ComposeDateTime(Date, Time), -OffsetMinutes);
  return AReturnUTC ? Utc : UtcToLocal(Utc);
}

namespace {

wchar_t * InitCmdLine()
{
#if defined(_WIN32)
  return GetCommandLineW();
#else
  static UnicodeString Line;
  for (const UnicodeString & Arg : CommandLineParams())
  {
    if (!Line.IsEmpty())
    {
      Line += L" ";
    }
    // Quoted as core's CutToken reads it back: within quotes, "" is a
    // quote. An argument with a space, tab or quote needs quoting.
    if ((Arg.Pos(UnicodeString(L" ")) > 0) || (Arg.Pos(UnicodeString(L"\t")) > 0) ||
        (Arg.Pos(UnicodeString(L"\"")) > 0))
    {
      Line += UnicodeString(L"\"") + ReplaceStr(Arg, L"\"", L"\"\"") + L"\"";
    }
    else
    {
      Line += Arg;
    }
  }
  return Line.c_str();
#endif
}

}  // namespace

wchar_t * CmdLine = InitCmdLine();
