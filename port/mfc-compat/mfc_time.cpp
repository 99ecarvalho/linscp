//---------------------------------------------------------------------------
// CTime for linscp's MFC-compatible layer (see include/afx.h).
//
// Original code, written for linscp (GPL-3.0-or-later).
//---------------------------------------------------------------------------
#include "afx.h"

#include <cstring>

CTime::CTime(const FILETIME & FileTime, int /*DST*/)
{
  if ((FileTime.dwLowDateTime == 0) && (FileTime.dwHighDateTime == 0))
  {
    return;
  }
  FILETIME Local;
  SYSTEMTIME ST;
  if (!FileTimeToLocalFileTime(&FileTime, &Local) || !FileTimeToSystemTime(&Local, &ST) ||
      (ST.wYear < 1900))
  {
    return;
  }
  struct tm T;
  std::memset(&T, 0, sizeof(T));
  T.tm_year = ST.wYear - 1900;
  T.tm_mon = ST.wMonth - 1;
  T.tm_mday = ST.wDay;
  T.tm_hour = ST.wHour;
  T.tm_min = ST.wMinute;
  T.tm_sec = ST.wSecond;
  T.tm_isdst = -1;
  FTime = static_cast<__int64>(std::mktime(&T));
}

CTime CTime::CreateForCurrentTime()
{
  return CTime(static_cast<__int64>(std::time(nullptr)));
}

struct tm * CTime::GetLocalTm(struct tm * Result) const
{
  if (Result == nullptr)
  {
    return nullptr;
  }
  const time_t T = static_cast<time_t>(FTime);
#if defined(_WIN32)
  if (localtime_s(Result, &T) != 0)
  {
    return nullptr;
  }
#else
  if (localtime_r(&T, Result) == nullptr)
  {
    return nullptr;
  }
#endif
  return Result;
}

struct tm * CTime::GetGmtTm(struct tm * Result) const
{
  if (Result == nullptr)
  {
    return nullptr;
  }
  const time_t T = static_cast<time_t>(FTime);
#if defined(_WIN32)
  if (gmtime_s(Result, &T) != 0)
  {
    return nullptr;
  }
#else
  if (gmtime_r(&T, Result) == nullptr)
  {
    return nullptr;
  }
#endif
  return Result;
}

struct tm CTime::Local() const
{
  struct tm Result;
  if (GetLocalTm(&Result) == nullptr)
  {
    std::memset(&Result, 0, sizeof(Result));
    // So that GetYear/GetMonth/GetDay of a time the runtime cannot convert
    // read 0, not 1900/1/0.
    Result.tm_year = -1900;
    Result.tm_mon = -1;
    Result.tm_wday = -1;
  }
  return Result;
}

int CTime::GetYear() const { return Local().tm_year + 1900; }
int CTime::GetMonth() const { return Local().tm_mon + 1; }
int CTime::GetDay() const { return Local().tm_mday; }
int CTime::GetHour() const { return Local().tm_hour; }
int CTime::GetMinute() const { return Local().tm_min; }
int CTime::GetSecond() const { return Local().tm_sec; }
int CTime::GetDayOfWeek() const { return Local().tm_wday + 1; }
