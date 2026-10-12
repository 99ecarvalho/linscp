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
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <strings.h>
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
#include <ctype.h>

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
typedef ULONG * PULONG;
typedef USHORT * PUSHORT;
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
// The C runtime's, under 2-byte wchar_t: see win32_on_posix_wide.h.
#ifdef __cplusplus
}  // extern "C"
#endif
#include "win32_on_posix_wide.h"
#ifdef __cplusplus
extern "C" {
#endif

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
#if defined(__cplusplus) || defined(UNICODE)
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
#endif

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
// GetFileType's answers, which winscp.com's console protocol reports for the
// caller's input and output; there is no GetFileType here.
#define FILE_TYPE_UNKNOWN            0x0000
#define FILE_TYPE_DISK               0x0001
#define FILE_TYPE_CHAR               0x0002
#define FILE_TYPE_PIPE               0x0003

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
// Security attributes are ignored; the directory gets the umask's mode.
// Fails with ERROR_ALREADY_EXISTS or, for a missing parent,
// ERROR_PATH_NOT_FOUND, as Win32.
BOOL CreateDirectory(LPCWSTR PathName, LPSECURITY_ATTRIBUTES SecurityAttributes);
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

// --- PuTTY's Windows platform layer --------------------------------------------
// What WinSCP's PuTTY, compiled with its Windows platform layer on POSIX (see
// the decisions table), calls beyond core's needs. PuTTY is C and built
// without UNICODE, so it calls the narrow (A) forms; see the end of this file.

#define FAR
#define NEAR
#define CONST const
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))

// The CRT's case-insensitive comparisons, under their POSIX names.
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define strnicmp strncasecmp
// MSVC's: -1 when the output does not fit, where C99's returns the length
// needed. Code written for Windows tests for the former.
int _vsnprintf(char * Buffer, size_t Count, const char * Format, va_list Args);
int _snprintf(char * Buffer, size_t Count, const char * Format, ...);

typedef union _LARGE_INTEGER
{
  struct { DWORD LowPart; LONG HighPart; };
  struct { DWORD LowPart; LONG HighPart; } u;
  LONGLONG QuadPart;
} LARGE_INTEGER;
// A monotonic clock in nanoseconds.
BOOL QueryPerformanceCounter(LARGE_INTEGER * Count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER * Frequency);

typedef struct _MEMORYSTATUS
{
  DWORD dwLength;
  DWORD dwMemoryLoad;
  SIZE_T dwTotalPhys;
  SIZE_T dwAvailPhys;
  SIZE_T dwTotalPageFile;
  SIZE_T dwAvailPageFile;
  SIZE_T dwTotalVirtual;
  SIZE_T dwAvailVirtual;
} MEMORYSTATUS;
// From sysinfo(2).
void GlobalMemoryStatus(MEMORYSTATUS * Status);
// The thread's pseudo-handle, as on Windows: (HANDLE)-2.
HANDLE GetCurrentThread(void);
// From getrusage(2): this process, or with GetCurrentThread() this thread.
BOOL GetProcessTimes(HANDLE Process, FILETIME * Creation, FILETIME * Exit, FILETIME * Kernel, FILETIME * User);
BOOL GetThreadTimes(HANDLE Thread, FILETIME * Creation, FILETIME * Exit, FILETIME * Kernel, FILETIME * User);

// Window and input state, which PuTTY's noise.c mixes into its entropy pool
// as a light extra source. There are no windows: nothing to report.
#define QS_ALLEVENTS 0x04BF
// Font character sets, for the default terminal font PuTTY's settings
// carry (WinSCP shows no terminal).
#define ANSI_CHARSET     0
#define DEFAULT_CHARSET  1
#define OEM_CHARSET    255
HWND GetForegroundWindow(void);
HWND GetCapture(void);
HWND GetClipboardOwner(void);
DWORD GetQueueStatus(UINT Flags);
BOOL GetCursorPos(POINT * Point);

// Code pages. The ANSI and OEM code pages are Latin-1, as the shim takes the
// ANSI one to be; conversion handles CP_UTF8 and Latin-1 (28591, and CP_ACP /
// CP_OEMCP as such), and fails with ERROR_INVALID_PARAMETER for any other.
#define MB_PRECOMPOSED        0x00000001
#define MB_USEGLYPHCHARS      0x00000004
#define MB_ERR_INVALID_CHARS  0x00000008
#define WC_NO_BEST_FIT_CHARS  0x00000400
UINT GetACP(void);
UINT GetOEMCP(void);
BOOL IsDBCSLeadByteEx(UINT CodePage, BYTE Byte);
int MultiByteToWideChar(UINT CodePage, DWORD Flags, LPCSTR Src, int SrcLength, LPWSTR Dst, int DstLength);
int WideCharToMultiByte(UINT CodePage, DWORD Flags, LPCWSTR Src, int SrcLength, LPSTR Dst, int DstLength,
  LPCSTR DefaultChar, BOOL * UsedDefaultChar);

// System messages for Win32 and Winsock error codes, in English, as Windows'
// own en-US text. Codes without a message fail, as Windows does.
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_STRING     0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define FORMAT_MESSAGE_MAX_WIDTH_MASK  0x000000FF
DWORD FormatMessageA(DWORD Flags, LPCVOID Source, DWORD MessageId, DWORD LanguageId, LPSTR Buffer, DWORD Size,
  va_list * Arguments);
DWORD FormatMessageW(DWORD Flags, LPCVOID Source, DWORD MessageId, DWORD LanguageId, LPWSTR Buffer, DWORD Size,
  va_list * Arguments);
// The text alone, for the shim's SysErrorMessage; null for an unknown code.
const char * linscp_SystemMessage(DWORD Code);

DWORD GetEnvironmentVariableA(LPCSTR Name, LPSTR Buffer, DWORD Size);
// Where load_system32_dll looks; LoadLibrary only goes by the file's name.
UINT GetSystemDirectoryA(LPSTR Buffer, UINT Size);
UINT GetWindowsDirectoryA(LPSTR Buffer, UINT Size);
HMODULE LoadLibraryA(LPCSTR FileName);
HMODULE GetModuleHandleA(LPCSTR ModuleName);
HRESULT SHGetFolderPathA(HWND Owner, int Folder, HANDLE Token, DWORD Flags, LPSTR Path);

// Files, the narrow forms and the rest of PuTTY's use.
#define ERROR_BROKEN_PIPE        109
#define ERROR_IO_PENDING         997
#define ERROR_HANDLE_EOF          38
struct _OVERLAPPED
{
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union { struct { DWORD Offset; DWORD OffsetHigh; } s; PVOID Pointer; } u;
  HANDLE hEvent;
};
// Blocking reads and writes on a descriptor; an OVERLAPPED is accepted and
// completed synchronously, so GetOverlappedResult reports what was done.
BOOL ReadFile(HANDLE File, LPVOID Buffer, DWORD Count, LPDWORD Read, LPOVERLAPPED Overlapped);
BOOL WriteFile(HANDLE File, LPCVOID Buffer, DWORD Count, LPDWORD Written, LPOVERLAPPED Overlapped);
BOOL GetOverlappedResult(HANDLE File, LPOVERLAPPED Overlapped, LPDWORD Transferred, BOOL Wait);
HANDLE CreateFileA(LPCSTR FileName, DWORD DesiredAccess, DWORD ShareMode,
  LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDisposition, DWORD FlagsAndAttributes,
  HANDLE TemplateFile);
DWORD GetFileAttributesA(LPCSTR FileName);
BOOL DeleteFileA(LPCSTR FileName);
typedef enum _GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard, GetFileExMaxInfoLevel } GET_FILEEX_INFO_LEVELS;
typedef struct _WIN32_FILE_ATTRIBUTE_DATA
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;
BOOL GetFileAttributesExW(LPCWSTR FileName, GET_FILEEX_INFO_LEVELS Level, LPVOID Info);
BOOL GetFileAttributesExA(LPCSTR FileName, GET_FILEEX_INFO_LEVELS Level, LPVOID Info);
typedef struct _WIN32_FIND_DATAA
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  CHAR cFileName[MAX_PATH];
  CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA;
// Directory enumeration: the final component of the name may hold * and ?
// wildcards, matched as Windows matches them ("*.*" is everything).
HANDLE FindFirstFileA(LPCSTR FileName, WIN32_FIND_DATAA * Data);
BOOL FindNextFileA(HANDLE Find, WIN32_FIND_DATAA * Data);
HANDLE FindFirstFileW(LPCWSTR FileName, WIN32_FIND_DATAW * Data);
BOOL FindNextFileW(HANDLE Find, WIN32_FIND_DATAW * Data);
BOOL FindClose(HANDLE Find);

// Processes and pipes, for PuTTY's local proxy commands. A command line runs
// under /bin/sh -c, the POSIX counterpart of handing a command line to
// Windows; handles marked inheritable and named in STARTF_USESTDHANDLES
// become the child's standard descriptors, and no others are inherited.
#define HANDLE_FLAG_INHERIT            0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE 0x00000002
#define STARTF_USESHOWWINDOW   0x00000001
#define STARTF_USESTDHANDLES   0x00000100
#define CREATE_NO_WINDOW       0x08000000
#define NORMAL_PRIORITY_CLASS  0x00000020
#define DETACHED_PROCESS       0x00000008
struct _SECURITY_ATTRIBUTES
{
  DWORD nLength;
  LPVOID lpSecurityDescriptor;
  BOOL bInheritHandle;
};
typedef struct _STARTUPINFOA
{
  DWORD cb;
  LPSTR lpReserved;
  LPSTR lpDesktop;
  LPSTR lpTitle;
  DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute;
  DWORD dwFlags;
  WORD wShowWindow;
  WORD cbReserved2;
  LPBYTE lpReserved2;
  HANDLE hStdInput;
  HANDLE hStdOutput;
  HANDLE hStdError;
} STARTUPINFOA;
typedef struct _PROCESS_INFORMATION
{
  HANDLE hProcess;
  HANDLE hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
} PROCESS_INFORMATION;
BOOL CreatePipe(HANDLE * Read, HANDLE * Write, LPSECURITY_ATTRIBUTES Attributes, DWORD Size);
BOOL SetHandleInformation(HANDLE Object, DWORD Mask, DWORD Flags);
// hProcess is signalled when the child exits (waitable, and its exit code
// readable with GetExitCodeProcess); hThread is a handle to the same.
BOOL CreateProcessA(LPCSTR Application, LPSTR CommandLine, LPSECURITY_ATTRIBUTES ProcessAttributes,
  LPSECURITY_ATTRIBUTES ThreadAttributes, BOOL InheritHandles, DWORD CreationFlags, LPVOID Environment,
  LPCSTR CurrentDirectory, STARTUPINFOA * StartupInfo, PROCESS_INFORMATION * ProcessInformation);
BOOL GetExitCodeProcess(HANDLE Process, LPDWORD ExitCode);
// No named pipes exist, so no handle is one's server end: fails.
BOOL GetNamedPipeClientProcessId(HANDLE Pipe, PULONG ClientProcessId);
// The plain user name, from the password database.
BOOL GetUserNameA(LPSTR Buffer, LPDWORD Size);
BOOL TerminateProcess(HANDLE Process, UINT ExitCode);

// The registry's narrow forms: as with the wide ones, no key ever opens.
#define REG_OPTION_NON_VOLATILE 0x00000000
#define REG_CREATED_NEW_KEY     0x00000001
LONG RegOpenKeyExA(HKEY Key, LPCSTR SubKey, DWORD Options, REGSAM Desired, HKEY * Result);
LONG RegCreateKeyExA(HKEY Key, LPCSTR SubKey, DWORD Reserved, LPSTR Class, DWORD Options, REGSAM Desired,
  LPSECURITY_ATTRIBUTES Security, HKEY * Result, LPDWORD Disposition);
LONG RegCloseKey(HKEY Key);
LONG RegEnumKeyA(HKEY Key, DWORD Index, LPSTR Name, DWORD NameSize);
LONG RegQueryValueExA(HKEY Key, LPCSTR ValueName, LPDWORD Reserved, LPDWORD Type, LPBYTE Data, LPDWORD DataSize);
LONG RegSetValueExA(HKEY Key, LPCSTR ValueName, DWORD Reserved, DWORD Type, const BYTE * Data, DWORD DataSize);
LONG RegDeleteKeyA(HKEY Key, LPCSTR SubKey);
LONG RegDeleteValueA(HKEY Key, LPCSTR ValueName);

// CryptoAPI's random numbers, which PuTTY seeds its generator from: the
// kernel's (getrandom), through a provider that needs no key container.
typedef ULONG_PTR HCRYPTPROV;
#define PROV_RSA_FULL        1
#define CRYPT_VERIFYCONTEXT  0xF0000000
BOOL CryptAcquireContextA(HCRYPTPROV * Provider, LPCSTR Container, LPCSTR ProviderName, DWORD Type, DWORD Flags);
BOOL CryptGenRandom(HCRYPTPROV Provider, DWORD Length, BYTE * Buffer);
BOOL CryptReleaseContext(HCRYPTPROV Provider, DWORD Flags);

// Whether a block can be written. Windows probes the pages; POSIX has no
// probe that is not itself a hazard, so only a null block is reported bad.
BOOL IsBadWritePtr(LPVOID Block, UINT_PTR Size);

// A message box, in a process with no desktop: the caption and text go to
// standard error, and the default button (IDOK) is the answer.
#define MB_OK              0x00000000L
#define MB_ICONHAND        0x00000010L
#define MB_ICONERROR       MB_ICONHAND
#define MB_ICONEXCLAMATION 0x00000030L
#define MB_ICONWARNING     MB_ICONEXCLAMATION
#define IDOK 1
int MessageBoxW(HWND Window, LPCWSTR Text, LPCWSTR Caption, UINT Type);

// --- windows and message queues --------------------------------------------
// No desktop: windows are message targets only, as FileZilla's helper window
// is -- a window procedure, an owning thread and a user-data slot. Every
// thread gets a message queue on its first call to a message function, as
// Win32 creates one; it goes when the thread exits. Retrieval is Win32's:
// posted messages first, in order, then WM_TIMER, synthesized for a due
// timer; filters by window (NULL: any; (HWND)-1: thread messages only) and by
// message range; WM_QUIT makes GetMessage return 0.
typedef WORD ATOM;
typedef void * HBRUSH;
typedef void * HMENU;
typedef LRESULT (CALLBACK * WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (CALLBACK * TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

typedef struct tagMSG
{
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
} MSG, * PMSG, * LPMSG;

typedef struct tagWNDCLASSEXW
{
  UINT cbSize;
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  LPCWSTR lpszMenuName;
  LPCWSTR lpszClassName;
  HICON hIconSm;
} WNDCLASSEXW, WNDCLASSEX;

typedef struct tagCREATESTRUCTW
{
  LPVOID lpCreateParams;
  HINSTANCE hInstance;
  HMENU hMenu;
  HWND hwndParent;
  int cy;
  int cx;
  int y;
  int x;
  LONG style;
  LPCWSTR lpszName;
  LPCWSTR lpszClass;
  DWORD dwExStyle;
} CREATESTRUCTW, CREATESTRUCT;

#define WM_NULL       0x0000
#define WM_CREATE     0x0001
#define WM_DESTROY    0x0002
#define WM_QUIT       0x0012
#define WM_NCCREATE   0x0081
#define WM_NCDESTROY  0x0082
#define WM_TIMER      0x0113
#define WM_USER       0x0400
#define WM_APP        0x8000
#define PM_NOREMOVE   0x0000
#define PM_REMOVE     0x0001
#define PM_NOYIELD    0x0002
#define GWLP_WNDPROC  (-4)
#define GWLP_USERDATA (-21)
#define HWND_MESSAGE  ((HWND)(LONG_PTR)-3)
#define USER_TIMER_MINIMUM 0x0000000A
#define USER_TIMER_MAXIMUM 0x7FFFFFFF
#define ERROR_INVALID_WINDOW_HANDLE  1400
#define ERROR_CANNOT_FIND_WND_CLASS  1407
#define ERROR_CLASS_ALREADY_EXISTS   1410
#define ERROR_INVALID_INDEX          1413
#define ERROR_INVALID_THREAD_ID      1444
#define ERROR_NOT_ENOUGH_QUOTA       1816

ATOM RegisterClassEx(const WNDCLASSEXW * Class);
HWND CreateWindowEx(DWORD ExStyle, LPCWSTR ClassName, LPCWSTR WindowName, DWORD Style,
  int X, int Y, int Width, int Height, HWND Parent, HMENU Menu, HINSTANCE Instance, LPVOID Param);
BOOL DestroyWindow(HWND Window);
BOOL IsWindow(HWND Window);
LRESULT DefWindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
LONG_PTR GetWindowLongPtr(HWND Window, int Index);
LONG_PTR SetWindowLongPtr(HWND Window, int Index, LONG_PTR Value);
UINT RegisterWindowMessage(LPCWSTR Name);
BOOL PostMessage(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);
BOOL PostThreadMessage(DWORD ThreadId, UINT Message, WPARAM WParam, LPARAM LParam);
BOOL GetMessage(LPMSG Msg, HWND Window, UINT FilterMin, UINT FilterMax);
BOOL PeekMessage(LPMSG Msg, HWND Window, UINT FilterMin, UINT FilterMax, UINT RemoveMsg);
BOOL TranslateMessage(const MSG * Msg);
LRESULT DispatchMessage(const MSG * Msg);
UINT_PTR SetTimer(HWND Window, UINT_PTR IdEvent, UINT Elapse, TIMERPROC TimerFunc);
BOOL KillTimer(HWND Window, UINT_PTR IdEvent);

// In C built without UNICODE -- PuTTY -- the bare names are the narrow
// forms, as <windows.h> maps them. C++ (core, built with UNICODE) keeps the
// wide functions under the bare names.
#if !defined(__cplusplus) && !defined(UNICODE)
  #define LoadLibrary LoadLibraryA
  #define GetModuleHandle GetModuleHandleA
  #define CreateFile CreateFileA
  #define GetFileAttributes GetFileAttributesA
  #define GetFileAttributesEx GetFileAttributesExA
  #define DeleteFile DeleteFileA
  #define FindFirstFile FindFirstFileA
  #define FindNextFile FindNextFileA
  #define WIN32_FIND_DATA WIN32_FIND_DATAA
  #define CreateProcess CreateProcessA
  #define STARTUPINFO STARTUPINFOA
  #define GetEnvironmentVariable GetEnvironmentVariableA
  #define GetUserName GetUserNameA
  #define FormatMessage FormatMessageA
  #define GetSystemDirectory GetSystemDirectoryA
  #define GetWindowsDirectory GetWindowsDirectoryA
  #define SHGetFolderPath SHGetFolderPathA
  #define RegOpenKeyEx RegOpenKeyExA
  #define RegCreateKeyEx RegCreateKeyExA
  #define RegEnumKey RegEnumKeyA
  #define RegQueryValueEx RegQueryValueExA
  #define RegSetValueEx RegSetValueExA
  #define RegDeleteKey RegDeleteKeyA
  #define RegDeleteValue RegDeleteValueA
  #define CryptAcquireContext CryptAcquireContextA
#else
  #define STARTUPINFO STARTUPINFOA
#endif

#ifdef __cplusplus
}  // extern "C"

// C++ linkage even where this header is first included inside extern "C".
extern "C++" {

// C++ is built with UNICODE: the bare names are the wide functions.
inline HANDLE FindFirstFile(LPCWSTR FileName, WIN32_FIND_DATAW * Data)
{
  return FindFirstFileW(FileName, Data);
}
inline BOOL FindNextFile(HANDLE Find, WIN32_FIND_DATAW * Data)
{
  return FindNextFileW(Find, Data);
}
inline int MessageBox(HWND Window, LPCWSTR Text, LPCWSTR Caption, UINT Type)
{
  return MessageBoxW(Window, Text, Caption, Type);
}
inline HWND CreateWindow(LPCWSTR ClassName, LPCWSTR WindowName, DWORD Style, int X, int Y,
  int Width, int Height, HWND Parent, HMENU Menu, HINSTANCE Instance, LPVOID Param)
{
  return CreateWindowEx(0, ClassName, WindowName, Style, X, Y, Width, Height, Parent, Menu, Instance, Param);
}

// The Windows CRT's original swprintf, with no buffer size, which mingw-w64
// and C++Builder still declare (AsyncSslSocketLayer.cpp uses it): formatted
// as UnicodeString::sprintf formats, with Windows' conventions (%s is wide),
// by the shim, which links with this layer. ISO's swprintf(Buffer, Count,
// Format, ...) is a different overload, and libc's (4-byte wchar_t).
int linscp_FormatWide(wchar_t * Buffer, const wchar_t * Format, va_list Args);
inline int swprintf(wchar_t * Buffer, const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  const int Result = linscp_FormatWide(Buffer, Format, Args);
  va_end(Args);
  return Result;
}
// snwprintf as mingw-w64 and C++Builder define it: C99's semantics (always
// terminated if Count > 0, returning the length the whole output needs).
int linscp_FormatWideN(wchar_t * Buffer, size_t Count, const wchar_t * Format, va_list Args);
inline int snwprintf(wchar_t * Buffer, size_t Count, const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  const int Result = linscp_FormatWideN(Buffer, Count, Format, Args);
  va_end(Args);
  return Result;
}

}  // extern "C++"
#endif

#endif // LINSCP_WIN32_ON_POSIX_H
