#include "win32_on_posix.h"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <locale.h>
#include <wctype.h>
#include <string>

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

// --- wide character functions ----------------------------------------------
//
// The macros in the header point the CRT names here. Written out rather than
// delegating to glibc, whose versions take a 4-byte wint_t (ADR 0001).

wchar_t linscp_ToLower(wchar_t C)
{
  // ASCII only, deliberately. The Windows CRT's towlower is locale-dependent
  // and folds only ASCII in the default locale, which a run of this test under
  // Wine confirmed. Folding Latin-1 here would make the two platforms disagree,
  // and a behaviour difference is worse than being narrow. Real Unicode folding
  // is a separate job, recorded in the divergence table.
  if (C >= L'A' && C <= L'Z')
  {
    return static_cast<wchar_t>(C - L'A' + L'a');
  }
  return C;
}

wchar_t linscp_ToUpper(wchar_t C)
{
  // ASCII only; see ToLower.
  if (C >= L'a' && C <= L'z')
  {
    return static_cast<wchar_t>(C - L'a' + L'A');
  }
  return C;
}

size_t linscp_WideLength(const wchar_t * S)
{
  if (S == nullptr)
  {
    return 0;
  }
  std::size_t N = 0;
  while (S[N] != L'\0')
  {
    ++N;
  }
  return N;
}

int linscp_WideCompare(const wchar_t * A, const wchar_t * B)
{
  while (*A != L'\0' && *A == *B)
  {
    ++A;
    ++B;
  }
  // Unsigned, as the CRT compares, so ordering matches Windows.
  const unsigned short X = static_cast<unsigned short>(*A);
  const unsigned short Y = static_cast<unsigned short>(*B);
  return X < Y ? -1 : (X > Y ? 1 : 0);
}

int linscp_WideCompareN(const wchar_t * A, const wchar_t * B, std::size_t N)
{
  for (std::size_t I = 0; I < N; ++I)
  {
    if (A[I] != B[I])
    {
      const unsigned short X = static_cast<unsigned short>(A[I]);
      const unsigned short Y = static_cast<unsigned short>(B[I]);
      return X < Y ? -1 : 1;
    }
    if (A[I] == L'\0')
    {
      break;
    }
  }
  return 0;
}

wchar_t * linscp_WideFindChar(const wchar_t * S, wchar_t C)
{
  if (S == nullptr)
  {
    return nullptr;
  }
  for (; *S != L'\0'; ++S)
  {
    if (*S == C)
    {
      return const_cast<wchar_t *>(S);
    }
  }
  // Searching for the terminator finds it, as the CRT does.
  return C == L'\0' ? const_cast<wchar_t *>(S) : nullptr;
}

wchar_t * linscp_WideFindCharLast(const wchar_t * S, wchar_t C)
{
  if (S == nullptr)
  {
    return nullptr;
  }
  const wchar_t * Last = nullptr;
  for (; *S != L'\0'; ++S)
  {
    if (*S == C)
    {
      Last = S;
    }
  }
  if (C == L'\0')
  {
    return const_cast<wchar_t *>(S);
  }
  return const_cast<wchar_t *>(Last);
}

wchar_t * linscp_WideFindString(const wchar_t * Haystack, const wchar_t * Needle)
{
  if (Haystack == nullptr || Needle == nullptr)
  {
    return nullptr;
  }
  if (*Needle == L'\0')
  {
    return const_cast<wchar_t *>(Haystack);
  }
  for (; *Haystack != L'\0'; ++Haystack)
  {
    const wchar_t * H = Haystack;
    const wchar_t * N = Needle;
    while (*H != L'\0' && *H == *N)
    {
      ++H;
      ++N;
    }
    if (*N == L'\0')
    {
      return const_cast<wchar_t *>(Haystack);
    }
  }
  return nullptr;
}

wchar_t * linscp_WideCopy(wchar_t * Dest, const wchar_t * Src)
{
  wchar_t * D = Dest;
  while ((*D++ = *Src++) != L'\0')
  {
  }
  return Dest;
}

wchar_t * linscp_WideCopyN(wchar_t * Dest, const wchar_t * Src, size_t N)
{
  // As the CRT: stops copying at the terminator but pads to N with nulls,
  // and leaves Dest unterminated when Src is N or longer.
  size_t I = 0;
  for (; (I < N) && (Src[I] != L'\0'); ++I)
  {
    Dest[I] = Src[I];
  }
  for (; I < N; ++I)
  {
    Dest[I] = L'\0';
  }
  return Dest;
}

wchar_t * linscp_WideFindAny(const wchar_t * S, const wchar_t * Chars)
{
  for (; *S != L'\0'; ++S)
  {
    for (const wchar_t * C = Chars; *C != L'\0'; ++C)
    {
      if (*S == *C)
      {
        return const_cast<wchar_t *>(S);
      }
    }
  }
  return nullptr;
}

long linscp_WideToLong(const wchar_t * S, wchar_t ** End, int Base)
{
  // ASCII digits only, which is what the CRT accepts too: narrowed and
  // handed to strtol, then the end mapped back by position.
  std::string Narrow;
  const wchar_t * P = S;
  for (; (*P != L'\0') && (static_cast<unsigned>(*P) < 0x80); ++P)
  {
    Narrow += static_cast<char>(*P);
  }
  char * NarrowEnd = nullptr;
  const long Result = std::strtol(Narrow.c_str(), &NarrowEnd, Base);
  if (End != nullptr)
  {
    *End = const_cast<wchar_t *>(S + (NarrowEnd - Narrow.c_str()));
  }
  return Result;
}

int linscp_WideToInt(const wchar_t * S)
{
  return static_cast<int>(linscp_WideToLong(S, nullptr, 10));
}

long long linscp_WideToInt64(const wchar_t * S)
{
  // As linscp_WideToLong: ASCII digits only, as the CRT accepts.
  std::string Narrow;
  for (const wchar_t * P = S; (*P != L'\0') && (static_cast<unsigned>(*P) < 0x80); ++P)
  {
    Narrow += static_cast<char>(*P);
  }
  return std::strtoll(Narrow.c_str(), nullptr, 10);
}

int linscp_WideIsAlpha(wchar_t C)
{
  // The CRT classifies wide characters by Unicode whatever the locale, so
  // this asks a fixed UTF-8 locale rather than the process's. Surrogates are
  // not letters on their own, there as here.
  static const locale_t Utf8 = newlocale(LC_CTYPE_MASK, "C.UTF-8", static_cast<locale_t>(0));
  const unsigned U = static_cast<unsigned>(C);
  if (Utf8 == static_cast<locale_t>(0))
  {
    return ((U | 0x20) >= 'a') && ((U | 0x20) <= 'z');
  }
  return iswalpha_l(static_cast<wint_t>(U), Utf8) ? 1 : 0;
}

char * linscp_LowerAscii(char * S)
{
  for (char * P = S; *P != '\0'; ++P)
  {
    if ((*P >= 'A') && (*P <= 'Z'))
    {
      *P = static_cast<char>(*P - 'A' + 'a');
    }
  }
  return S;
}

// The CRT's: compares as towlower folds, returning the difference.
int linscp_WideCompareIC(const wchar_t * A, const wchar_t * B)
{
  for (;; ++A, ++B)
  {
    const wchar_t X = linscp_ToLower(*A), Y = linscp_ToLower(*B);
    if ((X != Y) || (X == L'\0'))
    {
      return static_cast<int>(X) - static_cast<int>(Y);
    }
  }
}

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

namespace {

// The offset and DST flag in effect at a UTC instant.
long OffsetAt(std::time_t T, bool & IsDst)
{
  std::tm Local{};
  localtime_r(&T, &Local);
  IsDst = Local.tm_isdst > 0;
  return Local.tm_gmtoff;
}

// A transition as a Win32 recurring rule, from the local time in effect just
// before it: month, weekday, week of the month (5 for the last), time.
SYSTEMTIME RuleFor(std::time_t LocalBefore)
{
  std::tm L{};
  gmtime_r(&LocalBefore, &L);
  SYSTEMTIME R{};
  R.wYear = 0;
  R.wMonth = static_cast<WORD>(L.tm_mon + 1);
  R.wDayOfWeek = static_cast<WORD>(L.tm_wday);
  static const int DaysIn[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  const int Year = L.tm_year + 1900;
  const bool Leap = ((Year % 4 == 0) && (Year % 100 != 0)) || (Year % 400 == 0);
  const int Days = DaysIn[L.tm_mon] + (((L.tm_mon == 1) && Leap) ? 1 : 0);
  R.wDay = static_cast<WORD>((L.tm_mday + 7 > Days) ? 5 : (L.tm_mday - 1) / 7 + 1);
  R.wHour = static_cast<WORD>(L.tm_hour);
  R.wMinute = static_cast<WORD>(L.tm_min);
  return R;
}

void CopyName(WCHAR * Dest, const char * Name)
{
  int I = 0;
  for (; (Name != nullptr) && (Name[I] != '\0') && (I < 31); ++I)
  {
    Dest[I] = static_cast<WCHAR>(static_cast<unsigned char>(Name[I]));
  }
  Dest[I] = 0;
}

// Win32's description of the system zone for a year, read from the zone
// database: each day of the year is sampled, and every change of offset is
// located to the second by bisection.
bool ZoneForYear(int Year, TIME_ZONE_INFORMATION * Info)
{
  tzset();
  *Info = TIME_ZONE_INFORMATION();
  std::tm Jan{};
  Jan.tm_year = Year - 1900;
  Jan.tm_mon = 0;
  Jan.tm_mday = 1;
  const std::time_t Start = timegm(&Jan);
  bool PrevDst = false;
  long Prev = OffsetAt(Start, PrevDst);
  long Standard = PrevDst ? 0 : Prev;
  long Daylight = PrevDst ? Prev : 0;
  bool HaveStandard = !PrevDst, HaveDaylight = PrevDst;
  bool HaveDaylightDate = false, HaveStandardDate = false;
  for (int Day = 1; Day <= 366; ++Day)
  {
    const std::time_t T = Start + static_cast<std::time_t>(Day) * 86400;
    bool Dst = false;
    const long Off = OffsetAt(T, Dst);
    if (Dst && !HaveDaylight) { Daylight = Off; HaveDaylight = true; }
    if (!Dst && !HaveStandard) { Standard = Off; HaveStandard = true; }
    if ((Off != Prev) || (Dst != PrevDst))
    {
      std::time_t Lo = T - 86400, Hi = T;     // Lo before, Hi after
      while (Hi - Lo > 1)
      {
        const std::time_t Mid = Lo + (Hi - Lo) / 2;
        bool MidDst = false;
        const long MidOff = OffsetAt(Mid, MidDst);
        if ((MidOff == Prev) && (MidDst == PrevDst)) { Lo = Mid; } else { Hi = Mid; }
      }
      const SYSTEMTIME Rule = RuleFor(Hi + Prev);
      if (Dst && !PrevDst && !HaveDaylightDate)
      {
        Info->DaylightDate = Rule;
        HaveDaylightDate = true;
      }
      else if (!Dst && PrevDst && !HaveStandardDate)
      {
        Info->StandardDate = Rule;
        HaveStandardDate = true;
      }
    }
    Prev = Off;
    PrevDst = Dst;
  }
  if (!HaveStandard)
  {
    Standard = Daylight;
  }
  Info->Bias = -static_cast<LONG>(Standard / 60);
  Info->StandardBias = 0;
  CopyName(Info->StandardName, tzname[0]);
  const bool HasDst = HaveDaylight && HaveDaylightDate && HaveStandardDate;
  if (HasDst)
  {
    Info->DaylightBias = -static_cast<LONG>((Daylight - Standard) / 60);
    CopyName(Info->DaylightName, tzname[1]);
  }
  else
  {
    Info->StandardDate = SYSTEMTIME();
    Info->DaylightDate = SYSTEMTIME();
  }
  return HasDst;
}

}  // namespace

DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION * Info)
{
  if (Info == nullptr)
  {
    return TIME_ZONE_ID_INVALID;
  }
  const std::time_t Now = std::time(nullptr);
  std::tm Local{};
  localtime_r(&Now, &Local);
  if (!ZoneForYear(Local.tm_year + 1900, Info))
  {
    return TIME_ZONE_ID_UNKNOWN;
  }
  return (Local.tm_isdst > 0) ? TIME_ZONE_ID_DAYLIGHT : TIME_ZONE_ID_STANDARD;
}

BOOL GetTimeZoneInformationForYear(USHORT Year, PDYNAMIC_TIME_ZONE_INFORMATION DynamicInfo,
  LPTIME_ZONE_INFORMATION Info)
{
  if ((DynamicInfo != nullptr) || (Info == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  ZoneForYear(Year, Info);
  return TRUE;
}

BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION * Zone,
  const SYSTEMTIME * Universal, SYSTEMTIME * Local)
{
  if (Zone != nullptr)
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
  }
  FILETIME U, L;
  return SystemTimeToFileTime(Universal, &U) && FileTimeToLocalFileTime(&U, &L) &&
    FileTimeToSystemTime(&L, Local);
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

// One per-thread error, as on Windows, where WSAGetLastError is
// GetLastError: FileZilla sets errors with WSASetLastError and reads them
// with GetLastError.
int WSAGetLastError()
{
  return static_cast<int>(GetLastError());
}

void WSASetLastError(int Error)
{
  SetLastError(static_cast<DWORD>(Error));
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


