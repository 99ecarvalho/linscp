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

// C and C++ alike: WinSCP's PuTTY, which is C, includes this through
// <windows.h> as core does. The API has C linkage so both reach the same
// definitions; the few C++-only helpers are fenced off with __cplusplus.
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#ifdef __cplusplus
  #include <cstddef>
  #include <cstdint>
  #include <string>
#endif

// <windows.h> transitively provides these, and core relies on it: it calls
// strlen, strcmp and memmove unqualified. Using the C headers rather than the
// C++ ones so the names land in the global namespace, as they do on Windows.
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
  #define LINSCP_NORETURN [[noreturn]]
#else
  #define LINSCP_NORETURN _Noreturn
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- integral types --------------------------------------------------------
// Fixed-width deliberately: core serialises several of these into protocol and
// on-disk structures, so their widths are part of the contract, not an
// implementation detail.
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint32_t DWORD32;
typedef uint64_t DWORD64;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef uint32_t UINT;
typedef int32_t INT;
typedef int BOOL;                 // Win32 BOOL is int, not bool
typedef char CHAR;
typedef wchar_t WCHAR;
typedef unsigned char UCHAR;
typedef DWORD LCID;
typedef WORD LANGID;
typedef int32_t HRESULT;
typedef int32_t NTSTATUS;
typedef DWORD REGSAM;
typedef size_t SIZE_T;
typedef ptrdiff_t SSIZE_T;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;

typedef void * PVOID;
typedef void * LPVOID;
typedef const void * LPCVOID;
typedef char * LPSTR;
typedef const char * LPCSTR;
typedef wchar_t * LPWSTR;
typedef const wchar_t * LPCWSTR;
typedef wchar_t * LPTSTR;
typedef const wchar_t * LPCTSTR;
typedef BYTE * LPBYTE;
typedef DWORD * LPDWORD;
typedef BOOL * LPBOOL;
typedef DWORD * PDWORD;
typedef uint32_t UINT32;
typedef int64_t INT64;
typedef uint64_t UINT64;

typedef const wchar_t * PCWSTR;
typedef wchar_t * PWSTR;

// Window-message parameter types. No windows exist on POSIX, but PuTTY's
// Windows platform header and core's PuTTY glue pass these through as plain
// integers (select_result takes a socket in a WPARAM).
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;
typedef struct tagPOINT { LONG x; LONG y; } POINT;
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
// Import linkage is a DLL notion; declarations carrying it are ordinary.
#define DECLSPEC_IMPORT

// --- handles ---------------------------------------------------------------
// Opaque, as on Windows. A descriptor is stored in the pointer value, so the
// POSIX backing is an implementation detail callers never see.
typedef void * HANDLE;
typedef void * HMODULE;
typedef void * HINSTANCE;
typedef void * HWND;
typedef void * HKEY;
typedef void * HLOCAL;
typedef void * HGLOBAL;
typedef void * HRSRC;
typedef void * HICON;
typedef void * HCURSOR;

#define INVALID_HANDLE_VALUE (((HANDLE)(-1)))
#define INVALID_FILE_ATTRIBUTES (((DWORD)(-1)))
#define INVALID_SET_FILE_POINTER (((DWORD)(-1)))
#define INVALID_FILE_SIZE (((DWORD)(0xFFFFFFFF)))

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
// also guarantees core's and PuTTY's calls reach these rather than glibc's.
//
// Case folding is ASCII only, matching what the Windows CRT does in the default
// locale -- verified by running the same test against the real CRT under Wine.
// Being wider on one platform would make the two disagree, which is worse than
// being narrow. Real Unicode folding is a separate job; see the divergence
// table in doc/qt/TRACKER.md.
#ifdef __cplusplus
}  // extern "C"
  #include <cwchar>
  #include <cwctype>
extern "C" {
#else
  #include <wchar.h>
  #include <wctype.h>
#endif

wchar_t linscp_ToLower(wchar_t C);
wchar_t linscp_ToUpper(wchar_t C);
size_t linscp_WideLength(const wchar_t * S);
int linscp_WideCompare(const wchar_t * A, const wchar_t * B);
int linscp_WideCompareN(const wchar_t * A, const wchar_t * B, size_t N);
wchar_t * linscp_WideFindChar(const wchar_t * S, wchar_t C);
wchar_t * linscp_WideFindCharLast(const wchar_t * S, wchar_t C);
wchar_t * linscp_WideFindString(const wchar_t * Haystack, const wchar_t * Needle);
wchar_t * linscp_WideCopy(wchar_t * Dest, const wchar_t * Src);
wchar_t * linscp_WideCopyN(wchar_t * Dest, const wchar_t * Src, size_t N);
wchar_t * linscp_WideFindAny(const wchar_t * S, const wchar_t * Chars);
long linscp_WideToLong(const wchar_t * S, wchar_t ** End, int Base);
int linscp_WideToInt(const wchar_t * S);

#define towlower  linscp_ToLower
#define towupper  linscp_ToUpper
#define wcslen    linscp_WideLength
#define wcscmp    linscp_WideCompare
#define wcsncmp   linscp_WideCompareN
#define wcschr    linscp_WideFindChar
#define wcsrchr   linscp_WideFindCharLast
#define wcsstr    linscp_WideFindString
#define wcscpy    linscp_WideCopy
#define wcsncpy   linscp_WideCopyN
#define wcspbrk   linscp_WideFindAny
#define wcstol    linscp_WideToLong
#define _wtoi     linscp_WideToInt

// --- time ------------------------------------------------------------------
// FILETIME counts 100-nanosecond intervals since 1601-01-01 UTC. Core stores
// file timestamps in these and compares them against remote times, so the
// epoch and the scale both matter.
typedef struct _FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME;

// Win32's directory-entry record, with its real field set. Core reads it
// through Delphi's TSearchRec.FindData.
typedef struct _WIN32_FIND_DATAW
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  wchar_t cFileName[MAX_PATH];
  wchar_t cAlternateFileName[14];
} WIN32_FIND_DATAW;
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;

typedef struct _SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME;

typedef struct _TIME_ZONE_INFORMATION
{
  LONG Bias;
  WCHAR StandardName[32];
  SYSTEMTIME StandardDate;
  LONG StandardBias;
  WCHAR DaylightName[32];
  SYSTEMTIME DaylightDate;
  LONG DaylightBias;
} TIME_ZONE_INFORMATION;

// Intervals between 1601-01-01 and 1970-01-01: 11644473600 seconds.
#define FileTimeUnixEpochDelta ((uint64_t)116444736000000000ULL)
#define FileTimeTicksPerSecond ((uint64_t)10000000ULL)

BOOL FileTimeToSystemTime(const FILETIME * FileTime, SYSTEMTIME * SystemTime);
BOOL SystemTimeToFileTime(const SYSTEMTIME * SystemTime, FILETIME * FileTime);
BOOL FileTimeToLocalFileTime(const FILETIME * FileTime, FILETIME * LocalFileTime);
BOOL LocalFileTimeToFileTime(const FILETIME * LocalFileTime, FILETIME * FileTime);
void GetSystemTime(SYSTEMTIME * SystemTime);
void GetLocalTime(SYSTEMTIME * SystemTime);
void GetSystemTimeAsFileTime(FILETIME * FileTime);
LONG CompareFileTime(const FILETIME * A, const FILETIME * B);
// The system time zone as Win32 describes one: Bias is the standard offset
// (minutes to add to local standard time to reach UTC), DaylightBias the
// further shift in DST, and StandardDate/DaylightDate this year's two
// transitions as recurring rules (wYear 0; wDay the week of the month, 5 for
// the last), in the local time in effect just before each -- found in the
// zone database rather than assumed. Without DST both dates have wMonth 0
// and TIME_ZONE_ID_UNKNOWN is returned, as on Windows.
#define TIME_ZONE_ID_INVALID   ((DWORD)0xFFFFFFFF)
#define TIME_ZONE_ID_UNKNOWN   0
#define TIME_ZONE_ID_STANDARD  1
#define TIME_ZONE_ID_DAYLIGHT  2
typedef TIME_ZONE_INFORMATION * LPTIME_ZONE_INFORMATION;
typedef struct _TIME_DYNAMIC_ZONE_INFORMATION
{
  LONG Bias;
  WCHAR StandardName[32];
  SYSTEMTIME StandardDate;
  LONG StandardBias;
  WCHAR DaylightName[32];
  SYSTEMTIME DaylightDate;
  LONG DaylightBias;
  WCHAR TimeZoneKeyName[128];
  BOOL DynamicDaylightTimeDisabled;
} DYNAMIC_TIME_ZONE_INFORMATION;
typedef DYNAMIC_TIME_ZONE_INFORMATION * PDYNAMIC_TIME_ZONE_INFORMATION;
DWORD GetTimeZoneInformation(TIME_ZONE_INFORMATION * Info);
// The same for any year's rules, which core finds through
// GetProcAddress(kernel32, ...); DynamicInfo must be null (the system zone).
BOOL GetTimeZoneInformationForYear(USHORT Year, PDYNAMIC_TIME_ZONE_INFORMATION DynamicInfo,
  LPTIME_ZONE_INFORMATION Info);
// Only for the system zone (Zone null), which the zone database converts
// exactly; core never passes another.
BOOL SystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION * Zone,
  const SYSTEMTIME * Universal, SYSTEMTIME * Local);

#ifdef __cplusplus
}  // extern "C"
// Helpers used by the shim itself; not part of the Win32 API.
uint64_t FileTimeToTicks(const FILETIME & FileTime);
FILETIME TicksToFileTime(uint64_t Ticks);
extern "C" {
#endif


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

typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;
typedef SECURITY_ATTRIBUTES * LPSECURITY_ATTRIBUTES;

// Kernel-object handles start here; every value below is a descriptor.
// Descriptors are bounded by RLIMIT_NOFILE, orders of magnitude lower.
#define LinscpKernelHandleBase ((uintptr_t)0x10000000)

// --- files -----------------------------------------------------------------
// A file HANDLE is the descriptor itself, as the shim's THandleStream takes it,
// so a handle from CreateFile can be wrapped in a stream, read through it and
// closed with CloseHandle. Kernel-object handles (threads, events) are issued
// from a range no descriptor reaches, which is how CloseHandle tells the two
// apart.
//
// Share modes are accepted and ignored: POSIX has no mandatory locking, so a
// second opener is never refused with ERROR_SHARING_VIOLATION. See the
// divergence table in doc/qt/TRACKER.md.
#define GENERIC_READ                 0x80000000u
#define GENERIC_WRITE                0x40000000u
#define FILE_SHARE_READ              0x00000001
#define FILE_SHARE_WRITE             0x00000002
#define FILE_SHARE_DELETE            0x00000004
#define CREATE_NEW                   1
#define CREATE_ALWAYS                2
#define OPEN_EXISTING                3
#define OPEN_ALWAYS                  4
#define TRUNCATE_EXISTING            5
// Opens a directory (read-only, which is all futimens needs); core uses it to
// set a directory's timestamps.
#define FILE_FLAG_BACKUP_SEMANTICS   0x02000000
// The name is unlinked as soon as the file is open, where Windows removes it
// at the last close; either way nothing is left behind.
#define FILE_FLAG_DELETE_ON_CLOSE    0x04000000

// Attributes at creation: READONLY creates the file without write permission;
// the others have no POSIX meaning and are ignored.
HANDLE CreateFile(LPCWSTR FileName, DWORD DesiredAccess, DWORD ShareMode,
  LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDisposition,
  DWORD FlagsAndAttributes, HANDLE TemplateFile);
// Creation time is the file's birth time where the file system records one,
// else its modification time; it cannot be set.
BOOL GetFileTime(HANDLE File, FILETIME * CreationTime, FILETIME * LastAccessTime,
  FILETIME * LastWriteTime);
// A null time is left unchanged, as on Windows.
BOOL SetFileTime(HANDLE File, const FILETIME * CreationTime,
  const FILETIME * LastAccessTime, const FILETIME * LastWriteTime);
DWORD GetFileSize(HANDLE File, LPDWORD FileSizeHigh);
#define FILE_BEGIN    0
#define FILE_CURRENT  1
#define FILE_END      2
// lseek, with Win32's split offset: the high half in and out through
// DistanceToMoveHigh when it is given.
DWORD SetFilePointer(HANDLE File, LONG DistanceToMove, LONG * DistanceToMoveHigh, DWORD MoveMethod);
// The mapping the shim's FileGetAttr uses: DIRECTORY, REPARSE_POINT for a
// symbolic link, READONLY without the owner's write bit, HIDDEN for a leading
// dot, NORMAL when none applies. GetFileAttributes follows links, as Win32
// does for everything but the link's own reparse bit.
DWORD GetFileAttributes(LPCWSTR FileName);
// READONLY clears every write bit, its absence restores the owner's; the
// rest are ignored.
BOOL SetFileAttributes(LPCWSTR FileName, DWORD FileAttributes);
// $TMPDIR, else /tmp, with a trailing slash. As Win32: the length without the
// terminator, or when the buffer is too small the size needed including it.
DWORD GetTempPath(DWORD BufferLength, LPWSTR Buffer);

#ifdef __cplusplus
}  // extern "C"
namespace linscp {

// UTF-16 to the UTF-8 the file system takes. An unpaired surrogate becomes
// U+FFFD, as in the shim's own conversion.
std::string WideToUtf8(const wchar_t * S);
// The attributes of a stat result, for an entry with the given final name.
DWORD AttributesFromStat(const struct stat & St, const char * Name);

}  // namespace linscp
extern "C" {
#endif


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
#define ERROR_INSUFFICIENT_BUFFER   122
#define ERROR_MOD_NOT_FOUND         126
#define ERROR_PROC_NOT_FOUND        127
#define ERROR_ENVVAR_NOT_FOUND      203
#define ERROR_NOT_A_REPARSE_POINT  4390
#define ERROR_NONE_MAPPED          1332
#define ERROR_RESOURCE_DATA_NOT_FOUND 1812
#define ERROR_RESOURCE_TYPE_NOT_FOUND 1813
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
// Calling conventions: one convention on x86-64 POSIX, so all are empty.
#define WINAPI
#define CALLBACK
#define APIENTRY
#define WINAPIV
#define PASCAL

typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID Parameter);

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
LINSCP_NORETURN void ExitThread(DWORD ExitCode);

// Critical sections are re-entrant on Win32 -- WinSCP's putty_section is
// entered recursively -- so this is a recursive pthread mutex.
typedef struct _RTL_CRITICAL_SECTION { pthread_mutex_t Mutex; } CRITICAL_SECTION;
typedef CRITICAL_SECTION * LPCRITICAL_SECTION;
void InitializeCriticalSection(LPCRITICAL_SECTION Section);
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION Section, DWORD SpinCount);
void EnterCriticalSection(LPCRITICAL_SECTION Section);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION Section);
void LeaveCriticalSection(LPCRITICAL_SECTION Section);
void DeleteCriticalSection(LPCRITICAL_SECTION Section);
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
// No asynchronous procedure calls exist to wake an alertable sleep.
DWORD SleepEx(DWORD Milliseconds, BOOL Alertable);

// Milliseconds on a monotonic clock. Wraps at 2^32 (about 49.7 days), as the
// Win32 one does; core only ever subtracts two readings, which survives that.
DWORD GetTickCount();
ULONGLONG GetTickCount64();

// --- COM -------------------------------------------------------------------
// Core initialises COM per thread for MSXML, which on POSIX is expat with no
// COM underneath. Counted per thread so the return values match: S_OK on a
// thread's first call, S_FALSE after.
#define S_OK     (((HRESULT)(0)))
#define S_FALSE  (((HRESULT)(1)))
HRESULT CoInitialize(LPVOID Reserved);
void CoUninitialize();

// Class and interface identifiers, so core's TComPtr compiles. There is no
// COM to instantiate anything from: CoCreateInstance reports every class as
// unregistered, which is what Windows says for a class that is not installed
// -- TComPtr::TryCreate then fails and Create raises, as they would there.
typedef struct _GUID
{
  DWORD Data1;
  WORD Data2;
  WORD Data3;
  BYTE Data4[8];
} GUID;
typedef GUID IID;
typedef GUID CLSID;
// References in C++, pointers in C: the same ABI.
#ifdef __cplusplus
typedef const IID & REFIID;
typedef const CLSID & REFCLSID;
#else
typedef const IID * REFIID;
typedef const CLSID * REFCLSID;
#endif
#define CLSCTX_INPROC_SERVER   0x1
#define CLSCTX_LOCAL_SERVER    0x4
#define CLSCTX_ALL             0x17
#define REGDB_E_CLASSNOTREG    (((HRESULT)(0x80040154)))
#define SUCCEEDED(hr)          (((HRESULT)(hr)) >= 0)
#define FAILED(hr)             (((HRESULT)(hr)) < 0)
HRESULT CoCreateInstance(REFCLSID ClsId, LPVOID Outer, DWORD ClsContext,
  REFIID Iid, LPVOID * Object);
#ifdef __cplusplus
}  // extern "C"
namespace linscp {
// IID_PPV_ARGS's identifier half. No interface has an identifier here; the
// value only ever reaches CoCreateInstance, which refuses every request.
template <typename T> const IID & IidOf(T **) { static const IID None{}; return None; }
}
#define IID_PPV_ARGS(pp) ::linscp::IidOf(pp), ((void **)(pp))
extern "C" {
#endif

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

#define HKEY_CLASSES_ROOT     (((HKEY)(0x80000000ULL)))
#define HKEY_CURRENT_USER     (((HKEY)(0x80000001ULL)))
#define HKEY_LOCAL_MACHINE    (((HKEY)(0x80000002ULL)))
#define HKEY_USERS            (((HKEY)(0x80000003ULL)))
#define HKEY_PERFORMANCE_DATA ((HKEY)(0x80000004ULL))
#define HKEY_CURRENT_CONFIG   ((HKEY)(0x80000005ULL))
#define HKEY_DYN_DATA         ((HKEY)(0x80000006ULL))
// Registry value types.
#define REG_NONE              0
#define REG_SZ                1
#define REG_EXPAND_SZ         2
#define REG_BINARY            3
#define REG_DWORD             4
#define REG_MULTI_SZ          7
#define REG_QWORD             11
// There is no registry on POSIX: the shim's TRegistry opens no key, so the
// handle these receive is never a key, and they say so.
LONG RegQueryValueEx(HKEY Key, LPCWSTR ValueName, LPDWORD Reserved, LPDWORD Type,
  LPBYTE Data, LPDWORD DataSize);
LONG RegSetValueEx(HKEY Key, LPCWSTR ValueName, DWORD Reserved, DWORD Type,
  const BYTE * Data, DWORD DataSize);
#define KEY_WOW64_64KEY       0x0100
#define KEY_WOW64_32KEY       0x0200

// --- modules ---------------------------------------------------------------
// Core probes optional system functions with GetModuleHandle(L"kernel32.dll")
// and GetProcAddress. kernel32.dll and ntdll.dll are "loaded" -- IsWine()
// asserts ntdll is -- and resolve the functions this layer really implements
// (GetTimeZoneInformationForYear); any other name is absent, so core takes
// its fallback, as it would on a Windows without the function. Nothing else
// can be loaded.
typedef void (*FARPROC)(void);
HMODULE GetModuleHandle(LPCWSTR ModuleName);
HMODULE LoadLibrary(LPCWSTR FileName);
FARPROC GetProcAddress(HMODULE Module, LPCSTR ProcName);
BOOL FreeLibrary(HMODULE Module);

// --- processes -------------------------------------------------------------
// The process list comes from /proc, as Toolhelp's does from the kernel;
// core reads it to name its ancestor processes in logs. OpenProcess gives a
// handle GetModuleFileNameEx turns into the executable's path.
#define TH32CS_SNAPPROCESS          0x00000002
#define PROCESS_QUERY_INFORMATION   0x0400
#define PROCESS_VM_READ             0x0010
typedef struct tagPROCESSENTRY32W
{
  DWORD dwSize;
  DWORD cntUsage;
  DWORD th32ProcessID;
  ULONG_PTR th32DefaultHeapID;
  DWORD th32ModuleID;
  DWORD cntThreads;
  DWORD th32ParentProcessID;
  LONG pcPriClassBase;
  DWORD dwFlags;
  WCHAR szExeFile[MAX_PATH];
} PROCESSENTRY32W;
typedef PROCESSENTRY32W PROCESSENTRY32;
HANDLE CreateToolhelp32Snapshot(DWORD Flags, DWORD ProcessId);
BOOL Process32First(HANDLE Snapshot, PROCESSENTRY32W * Entry);
BOOL Process32Next(HANDLE Snapshot, PROCESSENTRY32W * Entry);
HANDLE OpenProcess(DWORD DesiredAccess, BOOL InheritHandle, DWORD ProcessId);
// The pseudo-handle, as on Windows: (HANDLE)-1, which CloseHandle ignores.
HANDLE GetCurrentProcess(void);
// Module null is the executable; that is the only module named.
DWORD GetModuleFileNameEx(HANDLE Process, HMODULE Module, LPWSTR FileName, DWORD Size);
// Fails: the module list is only logged, and an ELF process's has no
// HMODULE equivalent to report.
BOOL EnumProcessModules(HANDLE Process, HMODULE * Modules, DWORD Size, LPDWORD Needed);
// Never WOW64: nothing on POSIX runs a 32-bit Windows binary on a 64-bit one.
BOOL IsWow64Process(HANDLE Process, BOOL * Wow64Process);
BOOL GetProductInfo(DWORD OSMajor, DWORD OSMinor, DWORD SpMajor, DWORD SpMinor,
  PDWORD ReturnedProductType);

// --- environment, paths, strings -------------------------------------------
// %NAME% expanded from the environment; an unset name is left as written, as
// on Windows. Returns the length including the terminator, or when the
// buffer is too small the size needed.
DWORD ExpandEnvironmentStrings(LPCWSTR Src, LPWSTR Dst, DWORD Size);
// shlwapi's, with its rules on backslash paths: core turns every slash into
// a backslash before calling them, so a POSIX path arrives as "\home\x".
// PathIsRelative calls that absolute; PathSkipRoot skips only drive and UNC
// roots, so returns null for it, as shlwapi does.
LPCWSTR PathSkipRoot(LPCWSTR Path);
BOOL PathIsRelative(LPCWSTR Path);
// Case-insensitive with runs of digits compared as numbers, as Explorer
// sorts. No linguistic collation: see the divergence table.
int StrCmpLogicalW(LPCWSTR A, LPCWSTR B);
// Ordinal comparison of the case-folded strings; lstrcmp breaks ties with
// lower case first, as Windows' word sort does.
int lstrcmpi(LPCWSTR A, LPCWSTR B);
int lstrcmp(LPCWSTR A, LPCWSTR B);
FILE * _wfopen(LPCWSTR FileName, LPCWSTR Mode);
wchar_t * _wcsdup(const wchar_t * S);

// --- shell folders ---------------------------------------------------------
// The CSIDL folders core asks for, mapped onto the XDG directories: the
// Documents and Desktop user directories (user-dirs.dirs), the configuration
// and data homes for the two application-data folders, $HOME for the profile.
#define CSIDL_PERSONAL          0x0005
#define CSIDL_DESKTOPDIRECTORY  0x0010
#define CSIDL_APPDATA           0x001a
#define CSIDL_LOCAL_APPDATA     0x001c
#define CSIDL_COMMON_APPDATA    0x0023
#define CSIDL_PROFILE           0x0028
#define SHGFP_TYPE_CURRENT      0
#define SHGFP_TYPE_DEFAULT      1
#define E_FAIL                  ((HRESULT)0x80004005)
#define E_INVALIDARG            ((HRESULT)0x80070057)
HRESULT SHGetFolderPath(HWND Owner, int Folder, HANDLE Token, DWORD Flags, LPWSTR Path);

// File type names, by name only (SHGFI_USEFILEATTRIBUTES, core's one use):
// "File folder" for a directory, shared-mime-info's description for a known
// type ("HTML document"), else "XYZ File" for extension .xyz, as Windows names
// an unregistered type, or "File". There is no system image list; iIcon is 0.
#define SHGFI_ICON              0x000000100
#define SHGFI_DISPLAYNAME       0x000000200
#define SHGFI_TYPENAME          0x000000400
#define SHGFI_SYSICONINDEX      0x000004000
#define SHGFI_USEFILEATTRIBUTES 0x000000010
typedef struct _SHFILEINFOW
{
  HICON hIcon;
  int iIcon;
  DWORD dwAttributes;
  WCHAR szDisplayName[MAX_PATH];
  WCHAR szTypeName[80];
} SHFILEINFOW;
DWORD_PTR SHGetFileInfo(LPCWSTR Path, DWORD FileAttributes, SHFILEINFOW * Info, UINT Size, UINT Flags);

// --- memory ----------------------------------------------------------------
// Both release malloc'd memory: everything this layer hands out that a
// caller frees with them was allocated so.
HGLOBAL GlobalFree(HGLOBAL Memory);
void CoTaskMemFree(LPVOID Memory);

// --- MIME types (urlmon) ----------------------------------------------------
// By file name only (FMFD_URLASFILENAME, core's one use), from the
// shared-mime-info glob database; the content is never sniffed. E_FAIL when
// nothing matches, as for an unknown extension on Windows.
#define FMFD_DEFAULT            0x00000000
#define FMFD_URLASFILENAME      0x00000001
HRESULT FindMimeFromData(LPVOID BindCtx, LPCWSTR Url, LPVOID Buffer, DWORD Size,
  LPCWSTR ProposedMime, DWORD MimeFlags, LPWSTR * MimeOut, DWORD Reserved);

// --- code pages and locale ---------------------------------------------------
// The shim takes the ANSI code page to be Latin-1, so that is what is
// reported for it; the user locale is en-US. See the divergence table.
#define CP_ACP      0
#define CP_OEMCP    1
#define CP_UTF8     65001
#define MAX_DEFAULTCHAR      2
#define MAX_LEADBYTES        12
typedef struct _cpinfoexW
{
  UINT MaxCharSize;
  BYTE DefaultChar[MAX_DEFAULTCHAR];
  BYTE LeadByte[MAX_LEADBYTES];
  WCHAR UnicodeDefaultChar;
  UINT CodePage;
  WCHAR CodePageName[MAX_PATH];
} CPINFOEXW;
typedef CPINFOEXW CPINFOEX;
BOOL GetCPInfoEx(UINT CodePage, DWORD Flags, CPINFOEXW * Info);
LCID GetUserDefaultLCID(void);

// --- reparse points ------------------------------------------------------------
// A symbolic link reads back as a Win32 symlink reparse point: opened with
// FILE_FLAG_OPEN_REPARSE_POINT the link itself is opened (O_PATH, not
// followed), and FSCTL_GET_REPARSE_POINT fills the NT REPARSE_DATA_BUFFER
// layout with its target. Anything else is not a reparse point.
#define FILE_FLAG_OPEN_REPARSE_POINT       0x00200000
#define FSCTL_GET_REPARSE_POINT            0x000900A8
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE   (16 * 1024)
#define IO_REPARSE_TAG_MOUNT_POINT         0xA0000003u
#define IO_REPARSE_TAG_SYMLINK             0xA000000Cu
#define SYMLINK_FLAG_RELATIVE              0x00000001
typedef struct _OVERLAPPED OVERLAPPED;
typedef OVERLAPPED * LPOVERLAPPED;
BOOL DeviceIoControl(HANDLE Device, DWORD IoControlCode, LPVOID InBuffer, DWORD InSize,
  LPVOID OutBuffer, DWORD OutSize, LPDWORD BytesReturned, LPOVERLAPPED Overlapped);

// --- shell file operations ------------------------------------------------------
// FO_DELETE only, which is what core uses: with FOF_ALLOWUNDO the files go to
// the freedesktop.org trash (the user's home trash), as Explorer's go to the
// recycle bin; without it they are deleted, directories recursively.
#define FO_MOVE                  0x0001
#define FO_COPY                  0x0002
#define FO_DELETE                0x0003
#define FO_RENAME                0x0004
#define FOF_SILENT               0x0004
#define FOF_RENAMEONCOLLISION    0x0008
#define FOF_NOCONFIRMATION       0x0010
#define FOF_ALLOWUNDO            0x0040
#define FOF_NOCONFIRMMKDIR       0x0200
#define FOF_NOERRORUI            0x0400
typedef WORD FILEOP_FLAGS;
typedef struct _SHFILEOPSTRUCTW
{
  HWND hwnd;
  UINT wFunc;
  LPCWSTR pFrom;
  LPCWSTR pTo;
  FILEOP_FLAGS fFlags;
  BOOL fAnyOperationsAborted;
  LPVOID hNameMappings;
  LPCWSTR lpszProgressTitle;
} SHFILEOPSTRUCTW;
typedef SHFILEOPSTRUCTW SHFILEOPSTRUCT;
typedef SHFILEOPSTRUCTW * LPSHFILEOPSTRUCTW;
int SHFileOperation(LPSHFILEOPSTRUCTW FileOp);

// --- memory regions --------------------------------------------------------
// The process's address space as Win32 describes it, from /proc/self/maps:
// a mapping is committed (MEM_COMMIT), anonymous ones private (MEM_PRIVATE)
// and file-backed ones mapped (MEM_MAPPED); the gaps are free. Core sums
// these to log memory use. Linux overcommits, so "committed" here is
// address space, not memory in use: see the divergence table.
#define MEM_COMMIT    0x00001000
#define MEM_RESERVE   0x00002000
#define MEM_FREE      0x00010000
#define MEM_PRIVATE   0x00020000
#define MEM_MAPPED    0x00040000
#define MEM_IMAGE     0x01000000
typedef struct _MEMORY_BASIC_INFORMATION
{
  PVOID BaseAddress;
  PVOID AllocationBase;
  DWORD AllocationProtect;
  WORD PartitionId;
  SIZE_T RegionSize;
  DWORD State;
  DWORD Protect;
  DWORD Type;
} MEMORY_BASIC_INFORMATION;
// Returns the bytes written to Info, or 0 past the last mapping.
SIZE_T VirtualQuery(LPCVOID Address, MEMORY_BASIC_INFORMATION * Info, SIZE_T Length);

// --- version info ----------------------------------------------------------
typedef struct tagVS_FIXEDFILEINFO
{
  DWORD dwSignature;
  DWORD dwStrucVersion;
  DWORD dwFileVersionMS;
  DWORD dwFileVersionLS;
  DWORD dwProductVersionMS;
  DWORD dwProductVersionLS;
  DWORD dwFileFlagsMask;
  DWORD dwFileFlags;
  DWORD dwFileOS;
  DWORD dwFileType;
  DWORD dwFileSubtype;
  DWORD dwFileDateMS;
  DWORD dwFileDateLS;
} VS_FIXEDFILEINFO;
#define VS_FFI_SIGNATURE 0xFEEF04BD

// The running executable's version resource. On Windows windres embeds it;
// here gen_versioninfo.py generates the same data from WinSCP.cbproj into
// each executable (port/frontend), which defines linscp_OwnVersionInfo. The
// platform layer's own definition is weak and returns null, for executables
// without one.
typedef struct linscp_VersionString { const WCHAR * Key; const WCHAR * Value; } linscp_VersionString;
typedef struct linscp_VersionInfoData
{
  VS_FIXEDFILEINFO Fixed;
  WORD Language;
  WORD CodePage;
  int StringCount;
  const linscp_VersionString * Strings;
} linscp_VersionInfoData;
const linscp_VersionInfoData * linscp_OwnVersionInfo(void);

// The executable's own resource only (the file core asks about); for any
// other file there is none, ERROR_RESOURCE_DATA_NOT_FOUND. VerQueryValue
// answers "\\", "\\VarFileInfo\\Translation" and
// "\\StringFileInfo\\<lang><codepage>\\<key>", returning pointers into the
// block, string lengths in characters with the terminator, as Win32 does.
DWORD GetFileVersionInfoSize(LPCWSTR FileName, LPDWORD Handle);
BOOL GetFileVersionInfo(LPCWSTR FileName, DWORD Handle, DWORD Size, LPVOID Data);
BOOL VerQueryValue(LPCVOID Block, LPCWSTR SubBlock, LPVOID * Buffer, UINT * Length);

#ifdef __cplusplus
}  // extern "C"

// Win32 code written for LLP64 passes an unsigned long where a DWORD is
// declared: the same 32 bits on Windows, 64 on LP64 POSIX. Overloads take
// the wider type and fill it, so such code needs no change (Terminal.cpp's
// GetFileSize(Handle, &HSize)).
inline LONG RegQueryValueEx(HKEY Key, LPCWSTR ValueName, LPDWORD Reserved, unsigned long * Type,
  LPBYTE Data, unsigned long * DataSize)
{
  DWORD T = (Type != nullptr) ? static_cast<DWORD>(*Type) : 0;
  DWORD S = (DataSize != nullptr) ? static_cast<DWORD>(*DataSize) : 0;
  const LONG R = RegQueryValueEx(Key, ValueName, Reserved, (Type != nullptr) ? &T : nullptr, Data,
    (DataSize != nullptr) ? &S : nullptr);
  if (Type != nullptr) { *Type = T; }
  if (DataSize != nullptr) { *DataSize = S; }
  return R;
}

inline DWORD GetFileVersionInfoSize(LPCWSTR FileName, unsigned long * Handle)
{
  DWORD H = 0;
  const DWORD R = GetFileVersionInfoSize(FileName, &H);
  if (Handle != nullptr)
  {
    *Handle = H;
  }
  return R;
}

inline DWORD GetFileSize(HANDLE File, unsigned long * FileSizeHigh)
{
  DWORD High = 0;
  const DWORD Low = GetFileSize(File, &High);
  if (FileSizeHigh != nullptr)
  {
    *FileSizeHigh = High;
  }
  return Low;
}
#endif

#endif // LINSCP_WIN32_ON_POSIX_H
