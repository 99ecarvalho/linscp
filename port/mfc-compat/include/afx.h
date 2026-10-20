//---------------------------------------------------------------------------
// linscp's MFC-compatible layer: the part of the MFC API the FTP engine
// (source/filezilla) is written against, and nothing more.
//
// Original code, written for linscp (GPL-3.0-or-later). The interface is the
// one the FTP engine's call sites use; the behaviour follows Microsoft's
// public reference documentation for CStringT/CSimpleStringT, CTime and
// CFile. See doc/qt/adr/0017-mfc-compat.md for the scope and the choices
// made where that documentation is silent.
//
// The FTP engine reaches this header through its stdafx.h (#include
// <afx.h>), compiled with port/shim/vcl_win32_prelude.h forced first, so the
// Win32 API (the real one on Windows, the platform layer on POSIX) is
// already declared.
//
// Storage and conventions:
//   CString  wraps the shim's UnicodeString (UTF-16, a 2-byte wchar_t on
//            every target, ADR 0001). Indexes are 0-based, as in MFC, unlike
//            UnicodeString's 1-based ones. No C runtime wide function is
//            used (ADR 0001); case folding is ASCII only, as the shim's.
//   CTime    a 64-bit count of seconds since 1970-01-01 00:00:00 UTC.
//   CFile    a Win32 file HANDLE; I/O errors raise EOSError through the
//            shim's RaiseLastOSError, which is what the engine catches.
//---------------------------------------------------------------------------
#ifndef LINSCP_MFC_COMPAT_AFX_H
#define LINSCP_MFC_COMPAT_AFX_H

#include <cstdarg>
#include <ctime>

#include "vcl_unicodestring.h"

//---------------------------------------------------------------------------
// CString
//---------------------------------------------------------------------------
class CString
{
public:
  // --- construction --------------------------------------------------------
  CString() = default;
  CString(const CString & Src) = default;
  CString(CString && Src) noexcept = default;
  // A null-terminated string; a null pointer gives an empty string.
  CString(const wchar_t * Src);
  // Exactly Length characters of Src (embedded nulls included); none when
  // Length <= 0 or Src is null.
  CString(const wchar_t * Src, int Length);
  // Null-terminated narrow text, decoded exactly as UnicodeString decodes a
  // const char * (UTF-8; each maximal invalid subsequence becomes one
  // U+FFFD), on every platform. A null pointer gives an empty string.
  CString(const char * Src);
  // Exactly Length bytes, each becoming the code point of the same value
  // (Latin-1), embedded nulls included, on every platform; none when
  // Length <= 0 or Src is null.
  CString(const char * Src, int Length);
  explicit CString(const UnicodeString & Src);
  ~CString() = default;

  // --- assignment ----------------------------------------------------------
  CString & operator=(const CString & Src) = default;
  CString & operator=(CString && Src) noexcept = default;
  CString & operator=(const wchar_t * Src);
  // As CString(const char *).
  CString & operator=(const char * Src);
  CString & operator=(wchar_t Ch);

  // --- length and character access -----------------------------------------
  int GetLength() const { return FData.Length(); }
  bool IsEmpty() const { return FData.IsEmpty(); }
  void Empty() { FData = UnicodeString(); }
  // 0 <= Index <= GetLength(); the terminator reads as 0. Otherwise raises.
  wchar_t GetAt(int Index) const;
  wchar_t operator[](int Index) const { return GetAt(Index); }
  // 0 <= Index < GetLength(); otherwise raises and the string is unchanged.
  void SetAt(int Index, wchar_t Ch);
  // Null-terminated, never null; valid until the string is next changed.
  const wchar_t * GetString() const { return FData.c_str(); }
  // The same, under UnicodeString's name, which the engine's call sites use
  // (typically to pass the text through a variadic argument list).
  const wchar_t * c_str() const { return FData.c_str(); }
  operator const wchar_t *() const { return FData.c_str(); }

  // --- concatenation -------------------------------------------------------
  CString & operator+=(const CString & Src);
  CString & operator+=(const wchar_t * Src);
  CString & operator+=(wchar_t Ch);
  friend CString operator+(const CString & A, const CString & B);
  friend CString operator+(const CString & A, const wchar_t * B);
  friend CString operator+(const wchar_t * A, const CString & B);
  friend CString operator+(const CString & A, wchar_t B);
  friend CString operator+(wchar_t A, const CString & B);

  // --- comparison ----------------------------------------------------------
  // Unsigned code unit order; both sides end at their first null character,
  // and a null Other is empty. Exactly -1, 0 or +1.
  int Compare(const wchar_t * Other) const;
  // The same order after folding ASCII A-Z to a-z on both sides, but the
  // result is the difference of the first differing folded code units (as
  // unsigned values), or 0.
  int CompareNoCase(const wchar_t * Other) const;

  // --- extraction ----------------------------------------------------------
  // Counts and positions are clamped to the string: a negative count gives
  // an empty result, a count past the end stops at the end.
  CString Mid(int First) const;
  CString Mid(int First, int Count) const;
  CString Left(int Count) const;
  CString Right(int Count) const;

  // --- searching -----------------------------------------------------------
  // Searches treat the text as null-terminated: they stop at its first null
  // character (at or after Start, for Find). A negative Start is 0.
  //
  // The first Ch at or after Start, or -1; -1 when Start >= GetLength().
  // Ch 0 finds the terminating null: GetLength() when there is no embedded
  // one.
  int Find(wchar_t Ch, int Start = 0) const;
  // The first occurrence of Sub at or after Start, or -1; -1 when
  // Start > GetLength() or Sub is null. An empty Sub is found at Start.
  int Find(const wchar_t * Sub, int Start = 0) const;
  // The last Ch, or -1. Ch 0 finds the terminating null, as Find does.
  int ReverseFind(wchar_t Ch) const;
  // The first character that is in CharSet, or -1 (also for a null or empty
  // CharSet).
  int FindOneOf(const wchar_t * CharSet) const;

  // --- modification --------------------------------------------------------
  // Every non-overlapping occurrence, left to right. Returns 1 when the
  // content changed, else 0 (replacing text by itself changes nothing). An
  // empty or null Old replaces nothing; a null New is empty.
  int Replace(const wchar_t * Old, const wchar_t * New);
  // Returns how many characters were replaced; 0 when Old == New.
  int Replace(wchar_t Old, wchar_t New);
  // Removes up to Count characters from Index (a negative Index is 0);
  // returns the length before the removal.
  int Delete(int Index, int Count = 1);
  // Inserts at Index (clamped to [0, GetLength()]); returns the new length.
  int Insert(int Index, wchar_t Ch);
  int Insert(int Index, const wchar_t * Src);
  // ASCII only.
  CString & MakeLower();
  CString & MakeUpper();
  // Without an argument: the C locale's white space, " \t\n\v\f\r".
  CString & TrimLeft();
  CString & TrimLeft(wchar_t Target);
  CString & TrimLeft(const wchar_t * Targets);
  CString & TrimRight();
  CString & TrimRight(wchar_t Target);
  CString & TrimRight(const wchar_t * Targets);
  CString & Trim();
  CString & Trim(wchar_t Target);
  CString & Trim(const wchar_t * Targets);

  // --- formatting ----------------------------------------------------------
  // printf-style, with the Windows wide conventions (%s takes a wide string,
  // %hs a narrow one), as the shim's UnicodeString::sprintf formats.
  void Format(const wchar_t * Format, ...);
  // The format is the string resource FormatId (empty if there is none).
  void Format(unsigned int FormatId, ...);
  void FormatV(const wchar_t * Format, va_list Args);

private:
  UnicodeString FData;

  void Assign(const wchar_t * Src, int Length);
  void TrimLeftWhere(const wchar_t * Set, wchar_t Single);
  void TrimRightWhere(const wchar_t * Set, wchar_t Single);
};

bool operator==(const CString & A, const CString & B);
bool operator==(const CString & A, const wchar_t * B);
bool operator==(const wchar_t * A, const CString & B);
bool operator!=(const CString & A, const CString & B);
bool operator!=(const CString & A, const wchar_t * B);
bool operator!=(const wchar_t * A, const CString & B);
bool operator<(const CString & A, const CString & B);
bool operator<(const CString & A, const wchar_t * B);
bool operator<(const wchar_t * A, const CString & B);
bool operator>(const CString & A, const CString & B);
bool operator>(const CString & A, const wchar_t * B);
bool operator>(const wchar_t * A, const CString & B);
bool operator<=(const CString & A, const CString & B);
bool operator<=(const CString & A, const wchar_t * B);
bool operator<=(const wchar_t * A, const CString & B);
bool operator>=(const CString & A, const CString & B);
bool operator>=(const CString & A, const wchar_t * B);
bool operator>=(const wchar_t * A, const CString & B);

//---------------------------------------------------------------------------
// CTime
//---------------------------------------------------------------------------
class CTime
{
public:
  // Zero, 1970-01-01 00:00:00 UTC.
  CTime() = default;
  CTime(__int64 Time) : FTime(Time) {}
  // A UTC FILETIME, through local time: FileTimeToLocalFileTime, then
  // FileTimeToSystemTime (milliseconds dropped), then the C runtime's mktime
  // of that local date and time with tm_isdst = -1, so mktime decides DST
  // for that date. The result is mktime's, -1 included; 0 for an all-zero
  // FILETIME, when either conversion fails, or for a local year before 1900.
  // DST is not used. Core converts this value back on the same assumption.
  CTime(const FILETIME & FileTime, int DST = -1);

  // The current time. (Microsoft's name is GetCurrentTime, which <windows.h>
  // defines as a macro.)
  static CTime CreateForCurrentTime();

  // Seconds since 1970-01-01 00:00:00 UTC.
  __int64 GetTime() const { return FTime; }

  // Broken down in local or UTC time into *Result, which is returned; null
  // when Result is null or the C runtime cannot convert the time.
  struct tm * GetLocalTm(struct tm * Result) const;
  struct tm * GetGmtTm(struct tm * Result) const;
  int GetYear() const;
  int GetMonth() const;       // 1..12
  int GetDay() const;         // 1..31
  int GetHour() const;        // 0..23
  int GetMinute() const;      // 0..59
  int GetSecond() const;      // 0..59 (60 on a leap second)
  int GetDayOfWeek() const;   // 1 = Sunday .. 7 = Saturday

  CTime & operator=(__int64 Time) { FTime = Time; return *this; }

  bool operator==(CTime Other) const { return FTime == Other.FTime; }
  bool operator!=(CTime Other) const { return FTime != Other.FTime; }
  bool operator<(CTime Other) const { return FTime < Other.FTime; }
  bool operator>(CTime Other) const { return FTime > Other.FTime; }
  bool operator<=(CTime Other) const { return FTime <= Other.FTime; }
  bool operator>=(CTime Other) const { return FTime >= Other.FTime; }

private:
  __int64 FTime = 0;

  // Local broken-down time, or all fields zero when it cannot be computed.
  struct tm Local() const;
};

//---------------------------------------------------------------------------
// CFile
//---------------------------------------------------------------------------
class CFile
{
public:
  // Bits of Open's flags. Exactly one access mode and one share mode; the
  // zero values are the documented defaults (read access, exclusive).
  enum OpenFlags : unsigned int
  {
    modeRead =         0x00000000,
    modeWrite =        0x00000001,
    modeReadWrite =    0x00000002,
    // The share field: bit 0x100 lets others read, bit 0x200 lets others
    // write.
    shareExclusive =   0x00000000,
    shareDenyWrite =   0x00000100,
    shareDenyRead =    0x00000200,
    shareDenyNone =    0x00000300,
    modeNoInherit =    0x00000400,
    modeCreate =       0x00001000,
    modeNoTruncate =   0x00002000,
    // Text/binary/Unicode modes concern derived classes; accepted, ignored.
    typeText =         0x00004000,
    typeBinary =       0x00008000,
    typeUnicode =      0x00010000,
    osNoBuffer =       0x00020000,
    osWriteThrough =   0x00040000,
    osRandomAccess =   0x00080000,
    osSequentialScan = 0x00100000,
  };

  enum SeekPosition : unsigned int { begin = 0, current = 1, end = 2 };

  // The attribute byte, as documented (the Win32 FILE_ATTRIBUTE_* bits).
  enum Attribute : unsigned char
  {
    normal =    0x00,
    readOnly =  0x01,
    hidden =    0x02,
    system =    0x04,
    volume =    0x08,
    directory = 0x10,
    archive =   0x20,
  };

  // The value of m_hFile when no file is attached: INVALID_HANDLE_VALUE.
  static const HANDLE hFileNull;

  // Attached to no file.
  CFile();
  // Attached to File, which the object then owns and closes.
  explicit CFile(HANDLE File);
  // Opens FileName as Open does; raises EOSError when it cannot.
  CFile(const wchar_t * FileName, unsigned int OpenFlags);
  CFile(const CFile &) = delete;
  CFile & operator=(const CFile &) = delete;
  // Closes the file if one is attached. Never raises: a failure to close is
  // counted (DestructorCloseFailures) and, on Windows, reported with
  // OutputDebugString.
  virtual ~CFile();

  // Closes any file already attached, then opens FileName. Returns FALSE,
  // with the Win32 last error set by the failed call, when it cannot.
  // Successful or not, remembers "\r\n" + ExpandFileName(FileName) as the
  // text the errors below end with.
  virtual BOOL Open(const wchar_t * FileName, unsigned int OpenFlags);
  // Errors raise EOSError as RaiseLastOSError(Code, Text) words it: Code the
  // Win32 last error right after the failed call (no errno fallback; 0 gives
  // "A call to an OS function failed" alone), Text the remembered one.
  //
  // Up to Count bytes; fewer at the end of the file.
  virtual unsigned int Read(void * Buffer, unsigned int Count);
  // All Count bytes; a short write raises with ERROR_DISK_FULL.
  virtual void Write(const void * Buffer, unsigned int Count);
  // Raises EOSError when the file cannot be flushed.
  virtual void Flush();
  // The new position; raises EOSError on failure.
  virtual ULONGLONG Seek(LONGLONG Offset, unsigned int From);
  void SeekToBegin() { Seek(0, begin); }
  ULONGLONG SeekToEnd() { return Seek(0, end); }
  virtual ULONGLONG GetPosition() const;
  // Raises EOSError on failure.
  virtual ULONGLONG GetLength() const;
  // Detaches the file and forgets the remembered text, then closes it;
  // raises EOSError if closing fails. Nothing to do when none is attached.
  virtual void Close();
  // As Close, but never raises.
  virtual void Abort();

  operator HANDLE() const { return m_hFile; }

  // linscp's: how many times, process-wide, the destructor failed to close
  // its file.
  static unsigned long DestructorCloseFailures();

  HANDLE m_hFile;

private:
  UnicodeString FErrorText;

  [[noreturn]] void RaiseError(DWORD Code) const;
};

#endif // LINSCP_MFC_COMPAT_AFX_H
