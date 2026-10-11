#include "win32_on_posix.h"

#include <cerrno>
#include <chrono>
#include <ctime>

namespace {

// Win32's last-error is per-thread, so this must be too.
thread_local DWORD GLastError = NO_ERROR;

// Days from 1970-01-01 to a civil date, and back. Same algorithms as the
// date/time shim, duplicated here so the platform layer has no dependency on
// the VCL layer above it.
long long DaysFromCivil(int Y, unsigned M, unsigned D)
{
  Y -= M <= 2;
  const int Era = (Y >= 0 ? Y : Y - 399) / 400;
  const unsigned YoE = static_cast<unsigned>(Y - Era * 400);
  const unsigned DoY = (153u * (M + (M > 2 ? -3 : 9)) + 2u) / 5u + D - 1u;
  const unsigned DoE = YoE * 365u + YoE / 4u - YoE / 100u + DoY;
  return static_cast<long long>(Era) * 146097LL +
         static_cast<long long>(DoE) - 719468LL;
}

void CivilFromDays(long long Z, int & Y, unsigned & M, unsigned & D)
{
  Z += 719468;
  const long long Era = (Z >= 0 ? Z : Z - 146096) / 146097;
  const unsigned DoE = static_cast<unsigned>(Z - Era * 146097);
  const unsigned YoE = (DoE - DoE / 1460u + DoE / 36524u - DoE / 146096u) / 365u;
  const long long Yr = static_cast<long long>(YoE) + Era * 400;
  const unsigned DoY = DoE - (365u * YoE + YoE / 4u - YoE / 100u);
  const unsigned Mp = (5u * DoY + 2u) / 153u;
  D = DoY - (153u * Mp + 2u) / 5u + 1u;
  M = Mp + (Mp < 10u ? 3u : -9u);
  Y = static_cast<int>(Yr + (M <= 2u));
}

// Seconds of UTC offset currently in effect for a given UTC time. Computed from
// the C library rather than assumed, so DST transitions land correctly.
long LocalOffsetSeconds(std::time_t Utc)
{
  std::tm Local{};
  std::tm Gm{};
  localtime_r(&Utc, &Local);
  gmtime_r(&Utc, &Gm);
  const long long LocalDays = DaysFromCivil(Local.tm_year + 1900,
                                            static_cast<unsigned>(Local.tm_mon + 1),
                                            static_cast<unsigned>(Local.tm_mday));
  const long long GmDays = DaysFromCivil(Gm.tm_year + 1900,
                                         static_cast<unsigned>(Gm.tm_mon + 1),
                                         static_cast<unsigned>(Gm.tm_mday));
  const long long LocalSecs = LocalDays * 86400LL + Local.tm_hour * 3600LL +
                              Local.tm_min * 60LL + Local.tm_sec;
  const long long GmSecs = GmDays * 86400LL + Gm.tm_hour * 3600LL +
                           Gm.tm_min * 60LL + Gm.tm_sec;
  return static_cast<long>(LocalSecs - GmSecs);
}

}  // namespace

std::uint64_t FileTimeToTicks(const FILETIME & FileTime)
{
  return (static_cast<std::uint64_t>(FileTime.dwHighDateTime) << 32) |
         static_cast<std::uint64_t>(FileTime.dwLowDateTime);
}

FILETIME TicksToFileTime(std::uint64_t Ticks)
{
  FILETIME R;
  R.dwLowDateTime = static_cast<DWORD>(Ticks & 0xFFFFFFFFULL);
  R.dwHighDateTime = static_cast<DWORD>(Ticks >> 32);
  return R;
}

BOOL FileTimeToSystemTime(const FILETIME * FileTime, SYSTEMTIME * SystemTime)
{
  if (FileTime == nullptr || SystemTime == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  const std::uint64_t Ticks = FileTimeToTicks(*FileTime);
  // Win32 rejects a FILETIME before its own epoch.
  const std::int64_t Unix100ns =
    static_cast<std::int64_t>(Ticks) -
    static_cast<std::int64_t>(FileTimeUnixEpochDelta);
  std::int64_t Secs = Unix100ns / static_cast<std::int64_t>(FileTimeTicksPerSecond);
  std::int64_t Rem = Unix100ns % static_cast<std::int64_t>(FileTimeTicksPerSecond);
  if (Rem < 0)
  {
    --Secs;
    Rem += static_cast<std::int64_t>(FileTimeTicksPerSecond);
  }
  long long Days = Secs / 86400;
  long long SecOfDay = Secs % 86400;
  if (SecOfDay < 0)
  {
    --Days;
    SecOfDay += 86400;
  }
  int Y = 0;
  unsigned M = 0, D = 0;
  CivilFromDays(Days, Y, M, D);
  SystemTime->wYear = static_cast<WORD>(Y);
  SystemTime->wMonth = static_cast<WORD>(M);
  SystemTime->wDay = static_cast<WORD>(D);
  SystemTime->wHour = static_cast<WORD>(SecOfDay / 3600);
  SystemTime->wMinute = static_cast<WORD>((SecOfDay % 3600) / 60);
  SystemTime->wSecond = static_cast<WORD>(SecOfDay % 60);
  SystemTime->wMilliseconds =
    static_cast<WORD>(Rem / (FileTimeTicksPerSecond / 1000));
  // 1970-01-01 was a Thursday; Win32 numbers Sunday as 0.
  long long Dow = (Days + 4) % 7;
  if (Dow < 0)
  {
    Dow += 7;
  }
  SystemTime->wDayOfWeek = static_cast<WORD>(Dow);
  return TRUE;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME * SystemTime, FILETIME * FileTime)
{
  if (SystemTime == nullptr || FileTime == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  const long long Days = DaysFromCivil(SystemTime->wYear, SystemTime->wMonth,
                                       SystemTime->wDay);
  const std::int64_t Secs = Days * 86400LL + SystemTime->wHour * 3600LL +
                            SystemTime->wMinute * 60LL + SystemTime->wSecond;
  const std::int64_t Ticks =
    Secs * static_cast<std::int64_t>(FileTimeTicksPerSecond) +
    SystemTime->wMilliseconds * static_cast<std::int64_t>(FileTimeTicksPerSecond / 1000) +
    static_cast<std::int64_t>(FileTimeUnixEpochDelta);
  if (Ticks < 0)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  *FileTime = TicksToFileTime(static_cast<std::uint64_t>(Ticks));
  return TRUE;
}

BOOL FileTimeToLocalFileTime(const FILETIME * FileTime, FILETIME * LocalFileTime)
{
  if (FileTime == nullptr || LocalFileTime == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  const std::uint64_t Ticks = FileTimeToTicks(*FileTime);
  const std::time_t Utc = static_cast<std::time_t>(
    (static_cast<std::int64_t>(Ticks) -
     static_cast<std::int64_t>(FileTimeUnixEpochDelta)) /
    static_cast<std::int64_t>(FileTimeTicksPerSecond));
  const long Offset = LocalOffsetSeconds(Utc);
  *LocalFileTime = TicksToFileTime(
    Ticks + static_cast<std::uint64_t>(
      static_cast<std::int64_t>(Offset) *
      static_cast<std::int64_t>(FileTimeTicksPerSecond)));
  return TRUE;
}

BOOL LocalFileTimeToFileTime(const FILETIME * LocalFileTime, FILETIME * FileTime)
{
  if (LocalFileTime == nullptr || FileTime == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  const std::uint64_t Ticks = FileTimeToTicks(*LocalFileTime);
  // The offset depends on the instant, and we only have a local one; using the
  // local value to look it up is what Win32 effectively does too.
  const std::time_t Approx = static_cast<std::time_t>(
    (static_cast<std::int64_t>(Ticks) -
     static_cast<std::int64_t>(FileTimeUnixEpochDelta)) /
    static_cast<std::int64_t>(FileTimeTicksPerSecond));
  const long Offset = LocalOffsetSeconds(Approx);
  *FileTime = TicksToFileTime(
    Ticks - static_cast<std::uint64_t>(
      static_cast<std::int64_t>(Offset) *
      static_cast<std::int64_t>(FileTimeTicksPerSecond)));
  return TRUE;
}

void GetSystemTimeAsFileTime(FILETIME * FileTime)
{
  if (FileTime == nullptr)
  {
    return;
  }
  const auto Now = std::chrono::system_clock::now().time_since_epoch();
  const auto Micros =
    std::chrono::duration_cast<std::chrono::microseconds>(Now).count();
  *FileTime = TicksToFileTime(
    static_cast<std::uint64_t>(Micros) * 10ULL + FileTimeUnixEpochDelta);
}

void GetSystemTime(SYSTEMTIME * SystemTime)
{
  FILETIME Ft;
  GetSystemTimeAsFileTime(&Ft);
  FileTimeToSystemTime(&Ft, SystemTime);
}

void GetLocalTime(SYSTEMTIME * SystemTime)
{
  FILETIME Ft;
  FILETIME Local;
  GetSystemTimeAsFileTime(&Ft);
  FileTimeToLocalFileTime(&Ft, &Local);
  FileTimeToSystemTime(&Local, SystemTime);
}

LONG CompareFileTime(const FILETIME * A, const FILETIME * B)
{
  if (A == nullptr || B == nullptr)
  {
    return 0;
  }
  const std::uint64_t TA = FileTimeToTicks(*A);
  const std::uint64_t TB = FileTimeToTicks(*B);
  return TA < TB ? -1 : (TA > TB ? 1 : 0);
}

DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION * Info)
{
  if (Info == nullptr)
  {
    return 0xFFFFFFFFu;    // TIME_ZONE_ID_INVALID
  }
  *Info = TIME_ZONE_INFORMATION();
  const std::time_t Now = std::time(nullptr);
  // Win32's Bias is minutes to ADD to local to reach UTC, i.e. the negative of
  // the usual offset. Getting the sign wrong would shift every timestamp.
  Info->Bias = -static_cast<LONG>(LocalOffsetSeconds(Now) / 60);
  std::tm Local{};
  localtime_r(&Now, &Local);
  return Local.tm_isdst > 0 ? 2u : 1u;   // DAYLIGHT : STANDARD
}

DWORD GetLastError()
{
  return GLastError;
}

void SetLastError(DWORD Code)
{
  GLastError = Code;
}

DWORD Win32ErrorFromErrno(int Err)
{
  switch (Err)
  {
    case 0:        return NO_ERROR;
    case ENOENT:   return ERROR_FILE_NOT_FOUND;
    case ENOTDIR:  return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:    return ERROR_ACCESS_DENIED;
    case EBADF:    return ERROR_INVALID_HANDLE;
    case ENOMEM:   return ERROR_NOT_ENOUGH_MEMORY;
    case EEXIST:   return ERROR_FILE_EXISTS;
    case EINVAL:   return ERROR_INVALID_PARAMETER;
    case ENOSPC:   return ERROR_DISK_FULL;
    case ENOTSUP:  return ERROR_NOT_SUPPORTED;
    case ECANCELED: return ERROR_CANCELLED;
    default:       return static_cast<DWORD>(Err);
  }
}

// --- Winsock error translation ---------------------------------------------
// Defined here rather than in a socket translation unit of its own: it is the
// only part of the Winsock surface with real behaviour until P1-08 lands.

#include "winsock2.h"

int WinsockErrorFromErrno(int Err)
{
  switch (Err)
  {
    case 0:            return 0;
    case EINTR:        return WSAEINTR;
    case EBADF:        return WSAEBADF;
    case EACCES:       return WSAEACCES;
    case EFAULT:       return WSAEFAULT;
    case EINVAL:       return WSAEINVAL;
    case EMFILE:       return WSAEMFILE;
    case EWOULDBLOCK:  return WSAEWOULDBLOCK;
    case EINPROGRESS:  return WSAEINPROGRESS;
    case EALREADY:     return WSAEALREADY;
    case ENOTSOCK:     return WSAENOTSOCK;
    case EMSGSIZE:     return WSAEMSGSIZE;
    case EADDRINUSE:   return WSAEADDRINUSE;
    case EADDRNOTAVAIL: return WSAEADDRNOTAVAIL;
    case ENETDOWN:     return WSAENETDOWN;
    case ENETUNREACH:  return WSAENETUNREACH;
    case ENETRESET:    return WSAENETRESET;
    case ECONNABORTED: return WSAECONNABORTED;
    case ECONNRESET:   return WSAECONNRESET;
    case ENOBUFS:      return WSAENOBUFS;
    case EISCONN:      return WSAEISCONN;
    case ENOTCONN:     return WSAENOTCONN;
    case ESHUTDOWN:    return WSAESHUTDOWN;
    case ETIMEDOUT:    return WSAETIMEDOUT;
    case ECONNREFUSED: return WSAECONNREFUSED;
    case EHOSTDOWN:    return WSAEHOSTDOWN;
    case EHOSTUNREACH: return WSAEHOSTUNREACH;
    default:           return WSABASEERR + Err;
  }
}

namespace {
thread_local int GWsaLastError = 0;
}

int WSAGetLastError()
{
  return GWsaLastError != 0 ? GWsaLastError : WinsockErrorFromErrno(errno);
}

void WSASetLastError(int Error)
{
  GWsaLastError = Error;
}

int WSAStartup(WORD VersionRequested, WSADATA * Data)
{
  // Nothing to initialise on POSIX. Reporting the requested version back is
  // what callers check.
  if (Data != nullptr)
  {
    *Data = WSADATA();
    Data->wVersion = VersionRequested;
    Data->wHighVersion = VersionRequested;
  }
  return 0;
}

int WSACleanup()
{
  return 0;
}

int closesocket(SOCKET S)
{
  return ::close(static_cast<int>(S));
}
