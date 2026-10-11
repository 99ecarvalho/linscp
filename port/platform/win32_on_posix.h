//---------------------------------------------------------------------------
// The subset of the Win32 API that WinSCP's core uses, implemented for POSIX.
//
// Core calls Win32 directly in a few hundred places. Rather than edit those
// call sites -- which would put a large diff between this port and upstream and
// defeat the whole tracking model -- the subset they need is provided here. The
// core sources stay byte-identical outside the property and closure rewrites.
//
// Scope is driven by the compile census, not by the Win32 documentation: types
// and constants first, because those are what actually block compilation, with
// functions added as the census asks for them.
//
// On Windows this file is never used; the real <windows.h> is.
//---------------------------------------------------------------------------
#ifndef LINSCP_WIN32_ON_POSIX_H
#define LINSCP_WIN32_ON_POSIX_H

#if defined(_WIN32)
  #error "win32_on_posix.h is for non-Windows targets; include <windows.h> instead"
#endif

#include <cstddef>
#include <cstdint>

// <windows.h> transitively provides these, and core relies on it: it calls
// strlen, strcmp and memmove unqualified. Using the C headers rather than the
// C++ ones so the names land in the global namespace, as they do on Windows.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// --- integral types --------------------------------------------------------
// Fixed-width deliberately: core serialises several of these into protocol and
// on-disk structures, so their widths are part of the contract, not an
// implementation detail.
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using DWORD32 = std::uint32_t;
using DWORD64 = std::uint64_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using LONGLONG = std::int64_t;
using ULONGLONG = std::uint64_t;
using SHORT = std::int16_t;
using USHORT = std::uint16_t;
using UINT = std::uint32_t;
using INT = std::int32_t;
using BOOL = int;                 // Win32 BOOL is int, not bool
using CHAR = char;
using WCHAR = wchar_t;
using UCHAR = unsigned char;
using LCID = DWORD;
using LANGID = WORD;
using HRESULT = std::int32_t;
using NTSTATUS = std::int32_t;
using REGSAM = DWORD;
using SIZE_T = std::size_t;
using SSIZE_T = std::ptrdiff_t;
using INT_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using ULONG_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;

using PVOID = void *;
using LPVOID = void *;
using LPCVOID = const void *;
using LPSTR = char *;
using LPCSTR = const char *;
using LPWSTR = wchar_t *;
using LPCWSTR = const wchar_t *;
using LPTSTR = wchar_t *;
using LPCTSTR = const wchar_t *;
using LPBYTE = BYTE *;
using LPDWORD = DWORD *;
using LPBOOL = BOOL *;
using PDWORD = DWORD *;

// --- handles ---------------------------------------------------------------
// Opaque, as on Windows. A descriptor is stored in the pointer value, so the
// POSIX backing is an implementation detail callers never see.
using HANDLE = void *;
using HMODULE = void *;
using HINSTANCE = void *;
using HWND = void *;
using HKEY = void *;
using HLOCAL = void *;
using HGLOBAL = void *;
using HRSRC = void *;
using HICON = void *;
using HCURSOR = void *;

#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(-1))
#define INVALID_FILE_ATTRIBUTES (static_cast<DWORD>(-1))
#define INVALID_SET_FILE_POINTER (static_cast<DWORD>(-1))

#ifndef MAX_PATH
  #define MAX_PATH 260
#endif

// Win32's literal macros. TEXT() is used in 40 places in core, mostly by
// DebugAssert as TEXT(__FILE__). Two levels, as <winnt.h> has it: an argument
// that is itself a macro only expands when it is not an operand of ##, so a
// one-level L##quote pastes the token __FILE__ and yields L__FILE__.
#define __TEXT(quote) L##quote
#define TEXT(quote) __TEXT(quote)
#define _T(quote) __TEXT(quote)
#ifndef TRUE
  #define TRUE 1
  #define FALSE 0
#endif

// --- wide character functions ----------------------------------------------
//
// Core calls these directly: towupper 9, wcslen 10, wcschr 10, towlower 4,
// wcscmp 1. On Windows they come from the CRT with a 2-byte wchar_t. glibc's
// take a 4-byte wint_t, so under -fshort-wchar calling them is the ABI mismatch
// ADR 0001 describes: it links cleanly and then misbehaves.
//
// They are given distinct names and the CRT names redirected with macros,
// rather than redeclared. A redeclaration is not possible -- <string> pulls in
// <cwchar>, whose wcschr differs from ours only in return type -- and a macro
// also guarantees core's calls reach these rather than glibc's.
//
// Case folding is ASCII only, matching what the Windows CRT does in the default
// locale -- verified by running the same test against the real CRT under Wine.
// Being wider on one platform would make the two disagree, which is worse than
// being narrow. Real Unicode folding is a separate job; see the divergence
// table in doc/qt/TRACKER.md.
#include <cwchar>
#include <cwctype>

namespace linscp {

wchar_t ToLower(wchar_t C);
wchar_t ToUpper(wchar_t C);
std::size_t WideLength(const wchar_t * S);
int WideCompare(const wchar_t * A, const wchar_t * B);
int WideCompareN(const wchar_t * A, const wchar_t * B, std::size_t N);
wchar_t * WideFindChar(const wchar_t * S, wchar_t C);
wchar_t * WideFindCharLast(const wchar_t * S, wchar_t C);
wchar_t * WideFindString(const wchar_t * Haystack, const wchar_t * Needle);

}  // namespace linscp

#define towlower  ::linscp::ToLower
#define towupper  ::linscp::ToUpper
#define wcslen    ::linscp::WideLength
#define wcscmp    ::linscp::WideCompare
#define wcsncmp   ::linscp::WideCompareN
#define wcschr    ::linscp::WideFindChar
#define wcsrchr   ::linscp::WideFindCharLast
#define wcsstr    ::linscp::WideFindString

// --- time ------------------------------------------------------------------
// FILETIME counts 100-nanosecond intervals since 1601-01-01 UTC. Core stores
// file timestamps in these and compares them against remote times, so the
// epoch and the scale both matter.
struct FILETIME
{
  DWORD dwLowDateTime = 0;
  DWORD dwHighDateTime = 0;
};

// Win32's directory-entry record, with its real field set. Core reads it
// through Delphi's TSearchRec.FindData.
struct WIN32_FIND_DATAW
{
  DWORD dwFileAttributes = 0;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh = 0;
  DWORD nFileSizeLow = 0;
  DWORD dwReserved0 = 0;
  DWORD dwReserved1 = 0;
  wchar_t cFileName[MAX_PATH] = {};
  wchar_t cAlternateFileName[14] = {};
};
using WIN32_FIND_DATA = WIN32_FIND_DATAW;

struct SYSTEMTIME
{
  WORD wYear = 0;
  WORD wMonth = 0;
  WORD wDayOfWeek = 0;
  WORD wDay = 0;
  WORD wHour = 0;
  WORD wMinute = 0;
  WORD wSecond = 0;
  WORD wMilliseconds = 0;
};

struct TIME_ZONE_INFORMATION
{
  LONG Bias = 0;
  WCHAR StandardName[32] = { 0 };
  SYSTEMTIME StandardDate;
  LONG StandardBias = 0;
  WCHAR DaylightName[32] = { 0 };
  SYSTEMTIME DaylightDate;
  LONG DaylightBias = 0;
};

// Intervals between 1601-01-01 and 1970-01-01: 11644473600 seconds.
constexpr std::uint64_t FileTimeUnixEpochDelta = 116444736000000000ULL;
constexpr std::uint64_t FileTimeTicksPerSecond = 10000000ULL;

BOOL FileTimeToSystemTime(const FILETIME * FileTime, SYSTEMTIME * SystemTime);
BOOL SystemTimeToFileTime(const SYSTEMTIME * SystemTime, FILETIME * FileTime);
BOOL FileTimeToLocalFileTime(const FILETIME * FileTime, FILETIME * LocalFileTime);
BOOL LocalFileTimeToFileTime(const FILETIME * LocalFileTime, FILETIME * FileTime);
void GetSystemTime(SYSTEMTIME * SystemTime);
void GetLocalTime(SYSTEMTIME * SystemTime);
void GetSystemTimeAsFileTime(FILETIME * FileTime);
LONG CompareFileTime(const FILETIME * A, const FILETIME * B);
DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION * Info);

// Helpers used by the shim itself; not part of the Win32 API.
std::uint64_t FileTimeToTicks(const FILETIME & FileTime);
FILETIME TicksToFileTime(std::uint64_t Ticks);

// --- file attributes -------------------------------------------------------
#define FILE_ATTRIBUTE_READONLY            0x00000001
#define FILE_ATTRIBUTE_HIDDEN              0x00000002
#define FILE_ATTRIBUTE_SYSTEM              0x00000004
#define FILE_ATTRIBUTE_DIRECTORY           0x00000010
#define FILE_ATTRIBUTE_ARCHIVE             0x00000020
#define FILE_ATTRIBUTE_DEVICE              0x00000040
#define FILE_ATTRIBUTE_NORMAL              0x00000080
#define FILE_ATTRIBUTE_TEMPORARY           0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE         0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT       0x00000400
#define FILE_ATTRIBUTE_COMPRESSED          0x00000800
#define FILE_ATTRIBUTE_OFFLINE             0x00001000
#define FILE_ATTRIBUTE_ENCRYPTED           0x00004000

// --- error codes -----------------------------------------------------------
// Only the codes core actually tests for.
#define NO_ERROR                      0
#define ERROR_SUCCESS                 0
#define ERROR_FILE_NOT_FOUND          2
#define ERROR_PATH_NOT_FOUND          3
#define ERROR_ACCESS_DENIED           5
#define ERROR_INVALID_HANDLE          6
#define ERROR_NOT_ENOUGH_MEMORY       8
#define ERROR_NO_MORE_FILES          18
#define ERROR_SHARING_VIOLATION      32
#define ERROR_HANDLE_DISK_FULL       39
#define ERROR_NOT_SUPPORTED          50
#define ERROR_FILE_EXISTS            80
#define ERROR_INVALID_PARAMETER      87
#define ERROR_DISK_FULL             112
#define ERROR_ALREADY_EXISTS        183
#define ERROR_MORE_DATA             234
#define ERROR_NO_MORE_ITEMS         259
#define ERROR_CANCELLED            1223

DWORD GetLastError();
void SetLastError(DWORD Code);

// Translate the current errno into the nearest Win32 code, so core's existing
// comparisons against ERROR_* keep working.
DWORD Win32ErrorFromErrno(int Err);

// --- threads, events and waits --------------------------------------------
// Kernel objects live in one process-wide handle table. Handle values are never
// reused, so a use-after-CloseHandle fails with ERROR_INVALID_HANDLE instead of
// silently reaching whatever object took the slot.
//
// Only what core uses is provided. SuspendThread is deliberately absent: a
// running pthread cannot be suspended, and a missing declaration fails loudly
// at compile time where a stub would fail silently at run time.
#define WINAPI

struct SECURITY_ATTRIBUTES;
using LPSECURITY_ATTRIBUTES = SECURITY_ATTRIBUTES *;
using LPTHREAD_START_ROUTINE = DWORD (WINAPI *)(LPVOID Parameter);

#define INFINITE              0xFFFFFFFF
#define WAIT_OBJECT_0         0x00000000
#define WAIT_ABANDONED_0      0x00000080
#define WAIT_TIMEOUT          0x00000102
#define WAIT_FAILED           0xFFFFFFFF
#define MAXIMUM_WAIT_OBJECTS  64
#define STILL_ACTIVE          259
#define CREATE_SUSPENDED      0x00000004

#define THREAD_PRIORITY_IDLE          (-15)
#define THREAD_PRIORITY_LOWEST        (-2)
#define THREAD_PRIORITY_BELOW_NORMAL  (-1)
#define THREAD_PRIORITY_NORMAL        0
#define THREAD_PRIORITY_ABOVE_NORMAL  1
#define THREAD_PRIORITY_HIGHEST       2
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_PRIORITY_ERROR_RETURN  0x7FFFFFFF

// Named events would need to be shared across processes; core never names one,
// so a non-null name fails with ERROR_NOT_SUPPORTED rather than quietly
// creating a private event.
HANDLE CreateEvent(LPSECURITY_ATTRIBUTES Attributes, BOOL ManualReset,
  BOOL InitialState, LPCWSTR Name);
BOOL SetEvent(HANDLE Event);
BOOL ResetEvent(HANDLE Event);

HANDLE CreateThread(LPSECURITY_ATTRIBUTES Attributes, SIZE_T StackSize,
  LPTHREAD_START_ROUTINE StartAddress, LPVOID Parameter, DWORD CreationFlags,
  LPDWORD ThreadId);
// Returns the suspend count *before* the call, as Win32 does; core relies on
// that, treating 1 as "this call started the thread".
DWORD ResumeThread(HANDLE Thread);
// Unwinds to the thread's entry point by exception, so unlike Win32 the
// destructors on the way out do run. A catch (...) between the call and the
// entry point would intercept it; core never calls this inside such a block.
[[noreturn]] void ExitThread(DWORD ExitCode);
BOOL GetExitCodeThread(HANDLE Thread, LPDWORD ExitCode);
// Recorded and reported back, but has no scheduling effect. See the divergence
// table in doc/qt/TRACKER.md.
BOOL SetThreadPriority(HANDLE Thread, int Priority);
int GetThreadPriority(HANDLE Thread);
// Unique and non-zero per thread for the life of the process, and for threads
// started by CreateThread equal to the id CreateThread reported -- core asserts
// exactly that in TTerminalThread.
DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();

DWORD WaitForSingleObject(HANDLE Handle, DWORD Milliseconds);
DWORD WaitForMultipleObjects(DWORD Count, const HANDLE * Handles, BOOL WaitAll,
  DWORD Milliseconds);
BOOL CloseHandle(HANDLE Object);
void Sleep(DWORD Milliseconds);

// Milliseconds on a monotonic clock. Wraps at 2^32 (about 49.7 days), as the
// Win32 one does; core only ever subtracts two readings, which survives that.
DWORD GetTickCount();
ULONGLONG GetTickCount64();

// --- COM -------------------------------------------------------------------
// Core initialises COM per thread for MSXML, which on POSIX is expat with no
// COM underneath. Counted per thread so the return values match: S_OK on a
// thread's first call, S_FALSE after.
#define S_OK     (static_cast<HRESULT>(0))
#define S_FALSE  (static_cast<HRESULT>(1))
HRESULT CoInitialize(LPVOID Reserved);
void CoUninitialize();

// --- registry access modes -------------------------------------------------
// Present so declarations compile; the storage backend core actually uses on
// Linux is the INI path, which already exists upstream.
#define KEY_QUERY_VALUE       0x0001
#define KEY_SET_VALUE         0x0002
#define KEY_CREATE_SUB_KEY    0x0004
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_READ              0x20019
#define KEY_WRITE             0x20006
#define KEY_ALL_ACCESS        0xF003F

#define HKEY_CLASSES_ROOT     (reinterpret_cast<HKEY>(0x80000000ULL))
#define HKEY_CURRENT_USER     (reinterpret_cast<HKEY>(0x80000001ULL))
#define HKEY_LOCAL_MACHINE    (reinterpret_cast<HKEY>(0x80000002ULL))
#define HKEY_USERS            (reinterpret_cast<HKEY>(0x80000003ULL))

// --- version info ----------------------------------------------------------
struct VS_FIXEDFILEINFO
{
  DWORD dwSignature = 0;
  DWORD dwStrucVersion = 0;
  DWORD dwFileVersionMS = 0;
  DWORD dwFileVersionLS = 0;
  DWORD dwProductVersionMS = 0;
  DWORD dwProductVersionLS = 0;
  DWORD dwFileFlagsMask = 0;
  DWORD dwFileFlags = 0;
  DWORD dwFileOS = 0;
  DWORD dwFileType = 0;
  DWORD dwFileSubtype = 0;
  DWORD dwFileDateMS = 0;
  DWORD dwFileDateLS = 0;
};

#endif // LINSCP_WIN32_ON_POSIX_H
