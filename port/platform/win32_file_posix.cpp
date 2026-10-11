// Win32 file functions over POSIX descriptors. See the files section of
// win32_on_posix.h for the handle model.
#include "win32_on_posix.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linscp {

std::string WideToUtf8(const wchar_t * S)
{
  std::string Out;
  if (S == nullptr)
  {
    return Out;
  }
  auto Append = [&Out](char32_t C)
  {
    if (C < 0x80)
    {
      Out += static_cast<char>(C);
    }
    else if (C < 0x800)
    {
      Out += static_cast<char>(0xC0 | (C >> 6));
      Out += static_cast<char>(0x80 | (C & 0x3F));
    }
    else if (C < 0x10000)
    {
      Out += static_cast<char>(0xE0 | (C >> 12));
      Out += static_cast<char>(0x80 | ((C >> 6) & 0x3F));
      Out += static_cast<char>(0x80 | (C & 0x3F));
    }
    else
    {
      Out += static_cast<char>(0xF0 | (C >> 18));
      Out += static_cast<char>(0x80 | ((C >> 12) & 0x3F));
      Out += static_cast<char>(0x80 | ((C >> 6) & 0x3F));
      Out += static_cast<char>(0x80 | (C & 0x3F));
    }
  };
  for (const auto * P = reinterpret_cast<const char16_t *>(S); *P != 0; ++P)
  {
    const char16_t C = *P;
    if ((C >= 0xD800) && (C < 0xDC00) && (P[1] >= 0xDC00) && (P[1] < 0xE000))
    {
      Append(0x10000u + ((static_cast<char32_t>(C) - 0xD800u) << 10) +
             (static_cast<char32_t>(P[1]) - 0xDC00u));
      ++P;
    }
    else if ((C >= 0xD800) && (C < 0xE000))
    {
      Append(0xFFFD);
    }
    else
    {
      Append(C);
    }
  }
  return Out;
}

DWORD AttributesFromStat(const struct stat & St, const char * Name)
{
  DWORD Attr = 0;
  if (S_ISDIR(St.st_mode))
  {
    Attr |= FILE_ATTRIBUTE_DIRECTORY;
  }
  if (S_ISLNK(St.st_mode))
  {
    Attr |= FILE_ATTRIBUTE_REPARSE_POINT;
  }
  if ((St.st_mode & S_IWUSR) == 0)
  {
    Attr |= FILE_ATTRIBUTE_READONLY;
  }
  // POSIX has no hidden bit; a leading dot is the established convention and
  // is what users expect "show hidden files" to act on.
  if ((Name != nullptr) && (Name[0] == '.'))
  {
    Attr |= FILE_ATTRIBUTE_HIDDEN;
  }
  return (Attr == 0) ? FILE_ATTRIBUTE_NORMAL : Attr;
}

}  // namespace linscp

namespace {

int FdOf(HANDLE File)
{
  const auto Value = reinterpret_cast<std::intptr_t>(File);
  return ((File == INVALID_HANDLE_VALUE) || (Value < 0) || (Value > 0x7FFFFFFF))
    ? -1 : static_cast<int>(Value);
}

BOOL Fail()
{
  SetLastError(Win32ErrorFromErrno(errno));
  return FALSE;
}

FILETIME FromTimespec(const struct timespec & T)
{
  return TicksToFileTime(
    static_cast<std::uint64_t>(static_cast<std::int64_t>(T.tv_sec) *
                               static_cast<std::int64_t>(FileTimeTicksPerSecond) +
                               T.tv_nsec / 100) +
    FileTimeUnixEpochDelta);
}

struct timespec ToTimespec(const FILETIME & F)
{
  const std::int64_t Unix100ns =
    static_cast<std::int64_t>(FileTimeToTicks(F)) -
    static_cast<std::int64_t>(FileTimeUnixEpochDelta);
  std::int64_t Secs = Unix100ns / static_cast<std::int64_t>(FileTimeTicksPerSecond);
  std::int64_t Rem = Unix100ns % static_cast<std::int64_t>(FileTimeTicksPerSecond);
  if (Rem < 0)
  {
    --Secs;
    Rem += static_cast<std::int64_t>(FileTimeTicksPerSecond);
  }
  struct timespec T{};
  T.tv_sec = static_cast<time_t>(Secs);
  T.tv_nsec = static_cast<long>(Rem * 100);
  return T;
}

const char * FinalName(const std::string & Path)
{
  const std::size_t Slash = Path.rfind('/');
  return Path.c_str() + ((Slash == std::string::npos) ? 0 : Slash + 1);
}

}  // namespace

HANDLE CreateFile(LPCWSTR FileName, DWORD DesiredAccess, DWORD /*ShareMode*/,
  LPSECURITY_ATTRIBUTES /*SecurityAttributes*/, DWORD CreationDisposition,
  DWORD FlagsAndAttributes, HANDLE /*TemplateFile*/)
{
  const std::string Path = linscp::WideToUtf8(FileName);
  if ((DesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == 0)
  {
    // No data access: opened only to be asked about (timestamps, a link's
    // target), as Win32 allows. With FILE_FLAG_OPEN_REPARSE_POINT a link is
    // opened itself, not followed.
    if (CreationDisposition != OPEN_EXISTING)
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return INVALID_HANDLE_VALUE;
    }
    int PathFlags = O_PATH | O_CLOEXEC;
    if ((FlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) != 0)
    {
      PathFlags |= O_NOFOLLOW;
    }
    const int Fd = open(Path.c_str(), PathFlags);
    if (Fd < 0)
    {
      Fail();
      return INVALID_HANDLE_VALUE;
    }
    return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(Fd));
  }
  int Flags = O_CLOEXEC;
  if ((FlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS) != 0)
  {
    struct stat St{};
    if ((stat(Path.c_str(), &St) == 0) && S_ISDIR(St.st_mode))
    {
      // A directory: read-only is what can be opened, and enough to set its
      // timestamps.
      const int Fd = open(Path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      if (Fd < 0)
      {
        Fail();
        return INVALID_HANDLE_VALUE;
      }
      return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(Fd));
    }
  }
  const bool Read = (DesiredAccess & GENERIC_READ) != 0;
  const bool Write = (DesiredAccess & GENERIC_WRITE) != 0;
  Flags |= (Read && Write) ? O_RDWR : (Write ? O_WRONLY : O_RDONLY);
  switch (CreationDisposition)
  {
    case CREATE_NEW:        Flags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS:     Flags |= O_CREAT | O_TRUNC; break;
    case OPEN_EXISTING:     break;
    case OPEN_ALWAYS:       Flags |= O_CREAT; break;
    case TRUNCATE_EXISTING: Flags |= O_TRUNC; break;
    default:
      SetLastError(ERROR_INVALID_PARAMETER);
      return INVALID_HANDLE_VALUE;
  }
  const mode_t Mode =
    ((FlagsAndAttributes & FILE_ATTRIBUTE_READONLY) != 0) ? 0444 : 0666;
  const int Fd = open(Path.c_str(), Flags, Mode);
  if (Fd < 0)
  {
    // Win32 reports an existing file under CREATE_NEW as ERROR_FILE_EXISTS,
    // which is what errno's EEXIST maps to.
    Fail();
    return INVALID_HANDLE_VALUE;
  }
  if ((FlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) != 0)
  {
    unlink(Path.c_str());
  }
  return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(Fd));
}

BOOL GetFileTime(HANDLE File, FILETIME * CreationTime, FILETIME * LastAccessTime,
  FILETIME * LastWriteTime)
{
  const int Fd = FdOf(File);
  struct stat St{};
  if ((Fd < 0) || (fstat(Fd, &St) != 0))
  {
    if (Fd < 0)
    {
      SetLastError(ERROR_INVALID_HANDLE);
      return FALSE;
    }
    return Fail();
  }
  if (CreationTime != nullptr)
  {
    *CreationTime = FromTimespec(St.st_mtim);
#if defined(STATX_BTIME)
    struct statx Sx{};
    if ((statx(Fd, "", AT_EMPTY_PATH, STATX_BTIME, &Sx) == 0) &&
        ((Sx.stx_mask & STATX_BTIME) != 0))
    {
      struct timespec B{};
      B.tv_sec = static_cast<time_t>(Sx.stx_btime.tv_sec);
      B.tv_nsec = static_cast<long>(Sx.stx_btime.tv_nsec);
      *CreationTime = FromTimespec(B);
    }
#endif
  }
  if (LastAccessTime != nullptr)
  {
    *LastAccessTime = FromTimespec(St.st_atim);
  }
  if (LastWriteTime != nullptr)
  {
    *LastWriteTime = FromTimespec(St.st_mtim);
  }
  return TRUE;
}

BOOL SetFileTime(HANDLE File, const FILETIME * /*CreationTime*/,
  const FILETIME * LastAccessTime, const FILETIME * LastWriteTime)
{
  const int Fd = FdOf(File);
  if (Fd < 0)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  struct timespec Times[2];
  Times[0].tv_sec = 0;
  Times[0].tv_nsec = UTIME_OMIT;
  Times[1] = Times[0];
  if (LastAccessTime != nullptr)
  {
    Times[0] = ToTimespec(*LastAccessTime);
  }
  if (LastWriteTime != nullptr)
  {
    Times[1] = ToTimespec(*LastWriteTime);
  }
  return (futimens(Fd, Times) == 0) ? TRUE : Fail();
}

DWORD GetFileSize(HANDLE File, LPDWORD FileSizeHigh)
{
  const int Fd = FdOf(File);
  struct stat St{};
  if (Fd < 0)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return INVALID_FILE_SIZE;
  }
  if (fstat(Fd, &St) != 0)
  {
    Fail();
    return INVALID_FILE_SIZE;
  }
  const auto Size = static_cast<std::uint64_t>(St.st_size);
  if (FileSizeHigh != nullptr)
  {
    *FileSizeHigh = static_cast<DWORD>(Size >> 32);
  }
  // As Win32: a low half that happens to be 0xFFFFFFFF is told apart from
  // failure by GetLastError, so clear it.
  SetLastError(NO_ERROR);
  return static_cast<DWORD>(Size & 0xFFFFFFFFu);
}

DWORD GetFileAttributes(LPCWSTR FileName)
{
  const std::string Path = linscp::WideToUtf8(FileName);
  struct stat St{};
  if (stat(Path.c_str(), &St) != 0)
  {
    Fail();
    return INVALID_FILE_ATTRIBUTES;
  }
  struct stat Link{};
  DWORD Attr = linscp::AttributesFromStat(St, FinalName(Path));
  if ((lstat(Path.c_str(), &Link) == 0) && S_ISLNK(Link.st_mode))
  {
    Attr = (Attr & ~static_cast<DWORD>(FILE_ATTRIBUTE_NORMAL)) | FILE_ATTRIBUTE_REPARSE_POINT;
  }
  return Attr;
}

BOOL SetFileAttributes(LPCWSTR FileName, DWORD FileAttributes)
{
  const std::string Path = linscp::WideToUtf8(FileName);
  struct stat St{};
  if (stat(Path.c_str(), &St) != 0)
  {
    return Fail();
  }
  mode_t Mode = St.st_mode & 07777;
  if ((FileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
  {
    Mode &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
  }
  else
  {
    Mode |= S_IWUSR;
  }
  return (chmod(Path.c_str(), Mode) == 0) ? TRUE : Fail();
}

DWORD GetTempPath(DWORD BufferLength, LPWSTR Buffer)
{
  const char * Env = getenv("TMPDIR");
  std::string Dir = ((Env != nullptr) && (*Env != '\0')) ? Env : "/tmp";
  if (Dir.back() != '/')
  {
    Dir += '/';
  }
  // UTF-8 to UTF-16; a path from the environment is not otherwise checked.
  std::u16string Wide;
  for (std::size_t I = 0; I < Dir.size();)
  {
    const unsigned char C = static_cast<unsigned char>(Dir[I]);
    char32_t CP;
    int Len;
    if (C < 0x80) { CP = C; Len = 1; }
    else if ((C >> 5) == 0x6) { CP = C & 0x1F; Len = 2; }
    else if ((C >> 4) == 0xE) { CP = C & 0x0F; Len = 3; }
    else { CP = C & 0x07; Len = 4; }
    for (int K = 1; (K < Len) && (I + K < Dir.size()); ++K)
    {
      CP = (CP << 6) | (static_cast<unsigned char>(Dir[I + K]) & 0x3F);
    }
    I += Len;
    if (CP >= 0x10000)
    {
      CP -= 0x10000;
      Wide += static_cast<char16_t>(0xD800 + (CP >> 10));
      Wide += static_cast<char16_t>(0xDC00 + (CP & 0x3FF));
    }
    else
    {
      Wide += static_cast<char16_t>(CP);
    }
  }
  const auto Needed = static_cast<DWORD>(Wide.size());
  if ((Buffer == nullptr) || (BufferLength <= Needed))
  {
    return Needed + 1;
  }
  for (DWORD I = 0; I < Needed; ++I)
  {
    Buffer[I] = static_cast<wchar_t>(Wide[I]);
  }
  Buffer[Needed] = L'\0';
  return Needed;
}
