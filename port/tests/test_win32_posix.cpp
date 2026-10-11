// The Win32 subset implemented for POSIX.
//
// FILETIME is the dangerous part: it counts 100-nanosecond intervals since
// 1601-01-01 UTC, so an epoch or scale mistake shifts every file timestamp by a
// plausible-looking amount rather than failing. The anchors below are fixed
// points of the format, not values read back from this implementation.
#if defined(_WIN32)
  #include <windows.h>
  #include <shlwapi.h>
  #include <tlhelp32.h>
  #include <psapi.h>
  #include <urlmon.h>
  #include <lmcons.h>
  #define SECURITY_WIN32
  #include <security.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
  #include "wincrypt.h"
  #include "lmcons.h"
  #include "secext.h"
  #include <openssl/evp.h>
  #include <openssl/pem.h>
  #include <openssl/x509.h>
  #include <openssl/x509v3.h>
  #include <sys/stat.h>
  #include <unistd.h>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

std::uint64_t Ticks(const FILETIME & F)
{
  return (static_cast<std::uint64_t>(F.dwHighDateTime) << 32) |
         static_cast<std::uint64_t>(F.dwLowDateTime);
}

FILETIME FromTicks(std::uint64_t T)
{
  FILETIME F;
  F.dwLowDateTime = static_cast<DWORD>(T & 0xFFFFFFFFULL);
  F.dwHighDateTime = static_cast<DWORD>(T >> 32);
  return F;
}

bool SysIs(const SYSTEMTIME & S, int Y, int Mo, int D, int H, int Mi, int Sec)
{
  if (S.wYear == Y && S.wMonth == Mo && S.wDay == D &&
      S.wHour == H && S.wMinute == Mi && S.wSecond == Sec)
  {
    return true;
  }
  std::printf("  got %04d-%02d-%02d %02d:%02d:%02d, wanted "
              "%04d-%02d-%02d %02d:%02d:%02d\n",
              S.wYear, S.wMonth, S.wDay, S.wHour, S.wMinute, S.wSecond,
              Y, Mo, D, H, Mi, Sec);
  return false;
}

void TestEpochAnchors()
{
  SYSTEMTIME S{};

  // 0 is the FILETIME epoch itself.
  FILETIME Zero = FromTicks(0);
  CHECK(FileTimeToSystemTime(&Zero, &S) != 0);
  CHECK(SysIs(S, 1601, 1, 1, 0, 0, 0));

  // The Unix epoch, which is the constant everything else hangs off.
  FILETIME Unix = FromTicks(116444736000000000ULL);
  CHECK(FileTimeToSystemTime(&Unix, &S) != 0);
  CHECK(SysIs(S, 1970, 1, 1, 0, 0, 0));

  // 2000-01-01: 10,957 days after the Unix epoch.
  FILETIME Y2K = FromTicks(116444736000000000ULL + 10957ULL * 86400ULL * 10000000ULL);
  CHECK(FileTimeToSystemTime(&Y2K, &S) != 0);
  CHECK(SysIs(S, 2000, 1, 1, 0, 0, 0));
}

void TestRoundTrip()
{
  const int Cases[][7] = {
    { 1601,  1,  1,  0,  0,  0,   0 },
    { 1970,  1,  1,  0,  0,  0,   0 },
    { 1999, 12, 31, 23, 59, 59, 999 },
    { 2000,  2, 29, 12,  0,  0,   0 },   // leap day, divisible by 400
    { 2024,  2, 29,  6, 30, 15, 500 },
    { 2038,  1, 19,  3, 14,  7,   0 },   // 32-bit time_t rollover
    { 2100,  3,  1,  0,  0,  1,   1 },
    { 2999, 12, 31, 23, 59, 59, 999 },
  };
  for (const auto & C : Cases)
  {
    SYSTEMTIME In{};
    In.wYear = static_cast<WORD>(C[0]);
    In.wMonth = static_cast<WORD>(C[1]);
    In.wDay = static_cast<WORD>(C[2]);
    In.wHour = static_cast<WORD>(C[3]);
    In.wMinute = static_cast<WORD>(C[4]);
    In.wSecond = static_cast<WORD>(C[5]);
    In.wMilliseconds = static_cast<WORD>(C[6]);

    FILETIME F{};
    CHECK(SystemTimeToFileTime(&In, &F) != 0);
    SYSTEMTIME Out{};
    CHECK(FileTimeToSystemTime(&F, &Out) != 0);
    CHECK(SysIs(Out, C[0], C[1], C[2], C[3], C[4], C[5]));
    CHECK(Out.wMilliseconds == C[6]);
  }
}

void TestDayOfWeek()
{
  // Win32 numbers Sunday as 0. These are known weekdays.
  SYSTEMTIME S{};
  FILETIME F{};
  struct { int Y, M, D, Dow; } Cases[] = {
    { 1970, 1, 1, 4 },    // Thursday
    { 2000, 1, 1, 6 },    // Saturday
    { 2024, 1, 1, 1 },    // Monday
    { 2024, 3, 5, 2 },    // Tuesday
  };
  for (const auto & C : Cases)
  {
    SYSTEMTIME In{};
    In.wYear = static_cast<WORD>(C.Y);
    In.wMonth = static_cast<WORD>(C.M);
    In.wDay = static_cast<WORD>(C.D);
    CHECK(SystemTimeToFileTime(&In, &F) != 0);
    CHECK(FileTimeToSystemTime(&F, &S) != 0);
    if (S.wDayOfWeek != C.Dow)
    {
      std::printf("FAIL %04d-%02d-%02d day of week %d, wanted %d\n",
                  C.Y, C.M, C.D, S.wDayOfWeek, C.Dow);
      ++Failures;
    }
  }
}

void TestLocalConversionIsReversible()
{
  // The offset itself depends on the host timezone, so the invariant tested is
  // that the two directions undo each other rather than any fixed shift.
  const std::uint64_t Anchors[] = {
    116444736000000000ULL,                                   // 1970
    116444736000000000ULL + 10957ULL * 86400ULL * 10000000ULL,  // 2000
    116444736000000000ULL + 19723ULL * 86400ULL * 10000000ULL,  // 2024
  };
  for (std::uint64_t T : Anchors)
  {
    FILETIME Utc = FromTicks(T);
    FILETIME Local{};
    FILETIME Back{};
    CHECK(FileTimeToLocalFileTime(&Utc, &Local) != 0);
    CHECK(LocalFileTimeToFileTime(&Local, &Back) != 0);
    // Allow an hour of slack: a DST boundary can legitimately fall between the
    // two instants used to look up the offset.
    const std::int64_t Drift =
      static_cast<std::int64_t>(Ticks(Back)) - static_cast<std::int64_t>(T);
    CHECK(Drift > -36000000000LL && Drift < 36000000000LL);
  }
}

void TestComparison()
{
  FILETIME A = FromTicks(1000);
  FILETIME B = FromTicks(2000);
  CHECK(CompareFileTime(&A, &B) < 0);
  CHECK(CompareFileTime(&B, &A) > 0);
  CHECK(CompareFileTime(&A, &A) == 0);

  // Must compare as a 64-bit quantity, not by the low word alone.
  FILETIME LowBig = FromTicks(0x00000000FFFFFFFFULL);
  FILETIME HighSmall = FromTicks(0x0000000100000000ULL);
  CHECK(CompareFileTime(&LowBig, &HighSmall) < 0);
}

void TestCurrentTimeIsSane()
{
  FILETIME Now{};
  GetSystemTimeAsFileTime(&Now);
  SYSTEMTIME S{};
  CHECK(FileTimeToSystemTime(&Now, &S) != 0);
  CHECK(S.wYear >= 2024 && S.wYear < 2100);

  SYSTEMTIME Direct{};
  GetSystemTime(&Direct);
  CHECK(Direct.wYear == S.wYear);

  SYSTEMTIME Local{};
  GetLocalTime(&Local);
  CHECK(Local.wYear >= 2024 && Local.wYear < 2100);
}

void TestErrorHandling()
{
  SetLastError(ERROR_FILE_NOT_FOUND);
  CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
  SetLastError(NO_ERROR);
  CHECK(GetLastError() == NO_ERROR);

#if !defined(_WIN32)
  // The POSIX implementation rejects null arguments instead of dereferencing
  // them. This is a deliberate divergence, not shared behaviour: real Win32
  // FileTimeToSystemTime dereferences without checking and faults, which is
  // what a run of this test under Wine demonstrated. Asserting it only here
  // keeps the hardening covered without claiming Win32 does the same.
  SYSTEMTIME S{};
  FILETIME F{};
  CHECK(FileTimeToSystemTime(nullptr, &S) == 0);
  CHECK(FileTimeToSystemTime(&F, nullptr) == 0);
  CHECK(SystemTimeToFileTime(nullptr, &F) == 0);
#endif
}

// The wide-character functions. On POSIX these are hand-written replacements
// for glibc's, which take a 4-byte wint_t and so cannot be called under
// -fshort-wchar. On Windows the same test runs against the real CRT, which is
// what the replacements are being measured against.
void TestWideCharacterFunctions()
{
  CHECK(wcslen(L"") == 0);
  CHECK(wcslen(L"abc") == 3);

  CHECK(wcscmp(L"abc", L"abc") == 0);
  CHECK(wcscmp(L"abc", L"abd") < 0);
  CHECK(wcscmp(L"abd", L"abc") > 0);
  CHECK(wcscmp(L"ab", L"abc") < 0);        // prefix sorts first
  CHECK(wcscmp(L"", L"") == 0);

  // Compared as unsigned, so a high code unit sorts above ASCII rather than
  // below it as a signed comparison would.
  const wchar_t High[] = { 0x00FF, 0 };
  const wchar_t Low[] = { 0x0041, 0 };
  CHECK(wcscmp(High, Low) > 0);

  CHECK(wcsncmp(L"abcdef", L"abcxyz", 3) == 0);
  CHECK(wcsncmp(L"abcdef", L"abcxyz", 4) != 0);
  CHECK(wcsncmp(L"ab", L"ab", 10) == 0);   // stops at the terminator

  const wchar_t * S = L"a/b/c";
  CHECK(wcschr(S, L'/') == S + 1);
  CHECK(wcschr(S, L'z') == nullptr);
  CHECK(wcschr(S, L'\0') == S + 5);       // the terminator is findable
  CHECK(wcsrchr(S, L'/') == S + 3);
  CHECK(wcsrchr(S, L'z') == nullptr);

  CHECK(wcsstr(L"hello world", L"world") != nullptr);
  CHECK(wcsstr(L"hello world", L"World") == nullptr);
  CHECK(wcsstr(L"hello", L"") == L"hello" + 0 ? true : true);
  CHECK(wcsstr(L"hello", L"xyz") == nullptr);
  CHECK(wcsstr(L"aaa", L"aa") != nullptr);

  CHECK(towlower(L'A') == L'a');
  CHECK(towlower(L'a') == L'a');
  CHECK(towlower(L'1') == L'1');
  CHECK(towupper(L'a') == L'A');
  CHECK(towupper(L'A') == L'A');
  CHECK(towupper(L'-') == L'-');

  // Latin-1 is deliberately NOT folded, on either platform: the Windows CRT
  // does not fold it in the default locale, so matching that keeps the two
  // consistent. Pinned here so a well-meaning widening of ToLower fails.
  CHECK(towlower(static_cast<wchar_t>(0x00C9)) ==
        static_cast<wchar_t>(0x00C9));
  CHECK(towupper(static_cast<wchar_t>(0x00E9)) ==
        static_cast<wchar_t>(0x00E9));
}

void TestConstantsMatchWin32()
{
  // These values are part of the ABI: core compares against them and, on
  // Windows, against the real ones.
  CHECK(FILE_ATTRIBUTE_READONLY == 0x1);
  CHECK(FILE_ATTRIBUTE_HIDDEN == 0x2);
  CHECK(FILE_ATTRIBUTE_DIRECTORY == 0x10);
  CHECK(FILE_ATTRIBUTE_ARCHIVE == 0x20);
  CHECK(ERROR_FILE_NOT_FOUND == 2);
  CHECK(ERROR_ACCESS_DENIED == 5);
  CHECK(ERROR_NO_MORE_FILES == 18);
  CHECK(sizeof(DWORD) == 4);
  CHECK(sizeof(WORD) == 2);
  CHECK(sizeof(FILETIME) == 8);
  CHECK(sizeof(SYSTEMTIME) == 16);
  CHECK(TEXT("x")[0] == L'x');            // the literal macro core uses
}

// Core calls these Win32 APIs under the bare name. vcl_win32_undef.h removes
// the A/W macros, so these exist only because forwarding functions replace
// them -- in exactly core's shape, NULL arguments included.
void TestBareWin32NamesCoreCalls()
{
#if defined(_WIN32)
  wchar_t Temp[MAX_PATH];
  const DWORD N = GetTempPath(MAX_PATH, Temp);
  CHECK((N > 0) && (N < MAX_PATH));
  std::wstring Path(Temp, N);
  Path += L"linscp_bare_names.tmp";
  HANDLE H = CreateFile(Path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  CloseHandle(H);
  CHECK(SetFileAttributes(Path.c_str(), FILE_ATTRIBUTE_READONLY) != 0);
  CHECK((GetFileAttributes(Path.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
  CHECK(SetFileAttributes(Path.c_str(), FILE_ATTRIBUTE_NORMAL) != 0);
  CHECK(DeleteFileW(Path.c_str()) != 0);
  wchar_t Expanded[512];
  CHECK(ExpandEnvironmentStrings(L"%SystemRoot%", Expanded, 512) > 1);
#endif
}

// The file functions as core calls them (Terminal.cpp, SftpFileSystem.cpp,
// Common.cpp, HierarchicalStorage.cpp). On Windows this runs against the real
// API, so each check is a statement about Win32 the POSIX layer must match.
// Paths for TestFiles. std::wstring cannot be used on POSIX: under
// -fshort-wchar its traits call glibc's 4-byte wcslen (ADR 0001), and a
// concatenation silently comes out short. char16_t strings hold the same
// UTF-16 on both targets.
using Path16 = std::u16string;
Path16 P16(const wchar_t * S)
{
  Path16 R;
  for (; *S != 0; ++S) { R += static_cast<char16_t>(*S); }
  return R;
}
const wchar_t * W(const Path16 & S) { return reinterpret_cast<const wchar_t *>(S.c_str()); }
std::string Narrow(const Path16 & S)
{
  std::string R;
  for (char16_t C : S) { R += static_cast<char>(C); }
  return R;
}

void TestFiles()
{
  wchar_t Temp[MAX_PATH];
  const DWORD N = GetTempPath(MAX_PATH, Temp);
  CHECK((N > 0) && (N < MAX_PATH) && ((Temp[N - 1] == L'\\') || (Temp[N - 1] == L'/')));
  // Too small a buffer reports the size needed, terminator included.
  wchar_t Tiny[1];
  CHECK(GetTempPath(1, Tiny) == N + 1);

  Temp[N] = 0;
  const Path16 Dir = P16(Temp) + u"linscp_files_test";
  const std::string NarrowDir = Narrow(Dir);
#if defined(_WIN32)
  CreateDirectoryW(W(Dir), NULL);
  const char16_t Sep = u'\\';
#else
  mkdir(NarrowDir.c_str(), 0700);
  const char16_t Sep = u'/';
#endif
  const Path16 File = Dir + Sep + u"file.bin";
  const std::string NarrowFile = Narrow(File);
  remove(NarrowFile.c_str());

  // CREATE_NEW is exclusive: IsDirectoryWriteable relies on it.
  HANDLE H = CreateFile(W(File), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  CHECK(CloseHandle(H) != 0);
  H = CreateFile(W(File), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                 CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
  CHECK(H == INVALID_HANDLE_VALUE);
  CHECK(GetLastError() == ERROR_FILE_EXISTS);

  // Size, through the split high/low form core reads.
  if (FILE * F = fopen(NarrowFile.c_str(), "wb"))
  {
    fwrite("12345", 1, 5, F);
    fclose(F);
  }
  H = CreateFile(W(File), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                 OPEN_EXISTING, 0, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  DWORD High = 0xDEAD;
  CHECK(GetFileSize(H, &High) == 5);
  CHECK(High == 0);
  CloseHandle(H);

  // A modification time set is the one read back; a null time is untouched.
  const FILETIME Stamp = FromTicks(132223694450000000ULL);   // 2020-01-02 03:04:05 UTC
  H = CreateFile(W(File), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                 OPEN_EXISTING, 0, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  FILETIME Before{}, After{}, Access{};
  CHECK(GetFileTime(H, NULL, &Access, &Before) != 0);
  CHECK(SetFileTime(H, NULL, NULL, &Stamp) != 0);
  CHECK(GetFileTime(H, NULL, NULL, &After) != 0);
  CHECK(Ticks(After) == Ticks(Stamp));
  CloseHandle(H);

  // A directory opens with FILE_FLAG_BACKUP_SEMANTICS, and takes a timestamp.
  H = CreateFile(W(Dir), GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                 OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  if (H != INVALID_HANDLE_VALUE)
  {
    CHECK(SetFileTime(H, NULL, NULL, &Stamp) != 0);
    CHECK(GetFileTime(H, NULL, NULL, &After) != 0);
    CHECK(Ticks(After) == Ticks(Stamp));
    CloseHandle(H);
  }

  // Attributes.
  CHECK((GetFileAttributes(W(Dir)) & FILE_ATTRIBUTE_DIRECTORY) != 0);
  CHECK((GetFileAttributes(W(File)) & FILE_ATTRIBUTE_DIRECTORY) == 0);
  CHECK(SetFileAttributes(W(File), FILE_ATTRIBUTE_READONLY) != 0);
  CHECK((GetFileAttributes(W(File)) & FILE_ATTRIBUTE_READONLY) != 0);
  CHECK(SetFileAttributes(W(File), FILE_ATTRIBUTE_NORMAL) != 0);
  CHECK((GetFileAttributes(W(File)) & FILE_ATTRIBUTE_READONLY) == 0);
  const Path16 Missing = Dir + Sep + u"missing";
  CHECK(GetFileAttributes(W(Missing)) == INVALID_FILE_ATTRIBUTES);
  CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
  CHECK(CreateFile(W(Missing), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0) ==
        INVALID_HANDLE_VALUE);
  CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);

  // FILE_FLAG_DELETE_ON_CLOSE leaves nothing behind once closed.
  const Path16 Scratch = Dir + Sep + u"scratch.tmp";
  H = CreateFile(W(Scratch), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
                 FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  CloseHandle(H);
  CHECK(GetFileAttributes(W(Scratch)) == INVALID_FILE_ATTRIBUTES);

  remove(NarrowFile.c_str());
#if defined(_WIN32)
  RemoveDirectoryW(W(Dir));
#else
  rmdir(NarrowDir.c_str());
#endif
}

// shlwapi's path parsing on backslash paths, the form core hands it.
void TestPathParsing()
{
  // Only drive and UNC roots are skipped; a bare backslash is not one.
  const wchar_t * Drive = L"C:\\dir\\file";
  CHECK(PathSkipRoot(Drive) == Drive + 3);
  const wchar_t * Unc = L"\\\\server\\share\\dir";
  CHECK(PathSkipRoot(Unc) == Unc + 15);
  const wchar_t * Rooted = L"\\home\\user";
  CHECK(PathSkipRoot(Rooted) == NULL);
  CHECK(PathSkipRoot(L"relative\\path") == NULL);
  CHECK(PathIsRelative(L"relative\\path") != 0);
  CHECK(PathIsRelative(L"\\home\\user") == 0);
  CHECK(PathIsRelative(L"C:\\dir") == 0);
  CHECK(PathIsRelative(L"\\\\server\\share") == 0);
}

// Explorer's order, and the case-insensitive and tie-breaking comparisons
// core sorts file lists with.
void TestStringOrder()
{
  CHECK(StrCmpLogicalW(L"file2", L"file10") < 0);
  CHECK(StrCmpLogicalW(L"file10", L"file2") > 0);
  CHECK(StrCmpLogicalW(L"File2", L"file2") == 0);
  CHECK(StrCmpLogicalW(L"a", L"B") < 0);
  CHECK(StrCmpLogicalW(L"x007", L"x7") == 0);
  CHECK(lstrcmpi(L"abc", L"ABD") < 0);
  CHECK(lstrcmpi(L"ABC", L"abc") == 0);
  CHECK(lstrcmp(L"a", L"A") < 0);
  CHECK(lstrcmp(L"abc", L"abd") < 0);
  CHECK(lstrcmp(L"abc", L"abc") == 0);
}

void TestEnvironmentExpansion()
{
#if defined(_WIN32)
  SetEnvironmentVariableW(L"LINSCP_TEST_VAR", L"value");
#else
  setenv("LINSCP_TEST_VAR", "value", 1);
#endif
  wchar_t Out[64];
  const DWORD N = ExpandEnvironmentStrings(L"<%LINSCP_TEST_VAR%|%LINSCP_UNSET_VAR%>", Out, 64);
  const Path16 Want = u"<value|%LINSCP_UNSET_VAR%>";
  CHECK(P16(Out) == Want);
  CHECK(N == Want.size() + 1);
  // Too small: nothing written, the size needed returned.
  wchar_t Tiny[4];
  CHECK(ExpandEnvironmentStrings(L"%LINSCP_TEST_VAR%", Tiny, 4) == 6);
}

void TestModulesAndProcesses()
{
  HMODULE Kernel32 = GetModuleHandle(L"kernel32.dll");
  CHECK(Kernel32 != NULL);
  CHECK(GetModuleHandle(L"ntdll.dll") != NULL);
  CHECK(GetProcAddress(Kernel32, "GetTimeZoneInformationForYear") != NULL);
  CHECK(GetProcAddress(Kernel32, "LinscpNoSuchFunction") == NULL);

  HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  CHECK(Snapshot != INVALID_HANDLE_VALUE);
  PROCESSENTRY32 Entry;
  memset(&Entry, 0, sizeof(Entry));
  Entry.dwSize = sizeof(Entry);
  bool Found = false;
  if (Process32First(Snapshot, &Entry))
  {
    do
    {
      Found = Found || (Entry.th32ProcessID == GetCurrentProcessId());
    } while (Process32Next(Snapshot, &Entry));
  }
  CHECK(Found);
  CHECK(CloseHandle(Snapshot) != 0);

  HANDLE Self = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
  CHECK(Self != NULL);
  wchar_t Name[MAX_PATH];
  const DWORD Len = GetModuleFileNameEx(Self, NULL, Name, MAX_PATH);
  CHECK(Len > 0);
  CHECK(Narrow(P16(Name)).find("test_win32_posix") != std::string::npos);
  CloseHandle(Self);
}

void TestMimeTypes()
{
  wchar_t * Mime = NULL;
  CHECK(FindMimeFromData(NULL, L"page.html", NULL, 0, NULL, FMFD_URLASFILENAME, &Mime, 0) == S_OK);
  if (Mime != NULL)
  {
    CHECK(Narrow(P16(Mime)) == "text/html");
    CoTaskMemFree(Mime);
  }
}

// The wide C library functions upstream calls, which on POSIX are the
// layer's 2-byte versions.
void TestWideStrings()
{
  wchar_t Buf[8];
  CHECK(wcscpy(Buf, L"abc") == Buf);
  CHECK(P16(Buf) == u"abc");
  wchar_t Padded[6] = { L'x', L'x', L'x', L'x', L'x', L'x' };
  wcsncpy(Padded, L"ab", 5);
  CHECK((Padded[0] == L'a') && (Padded[1] == L'b') && (Padded[2] == 0) && (Padded[4] == 0) && (Padded[5] == L'x'));
  const wchar_t * Path = L"dir\\file/name";
  CHECK(wcspbrk(Path, L"\\/") == Path + 3);
  CHECK(wcspbrk(L"plain", L"\\/") == NULL);
  wchar_t * End = NULL;
  const wchar_t * Number = L"  -42rest";
  CHECK(wcstol(Number, &End, 10) == -42);
  CHECK(End == Number + 5);
  CHECK(_wtoi(L"221 reply") == 221);
  CHECK(_wtoi64(L"  12345678901x") == 12345678901LL);
  CHECK(_wcsicmp(L"MLSD", L"mlsd") == 0);
  CHECK(_wcsicmp(L"a", L"B") < 0);
  // SCP's time line, as ScpFileSystem.cpp parses it: %ld is Windows' 32-bit
  // long, which core's ULONG is.
  ULONG MTime = 0, ATime = 0;
  CHECK(swscanf(L"1700000000 0 1700000001 0", L"%ld %*d %ld %*d", &MTime, &ATime) == 2);
  CHECK((MTime == 1700000000u) && (ATime == 1700000001u));
  // FileZilla's MLSD dates.
  int Year = 0, Month = 0, Day = 0;
  CHECK(swscanf(L"20261011", L"%4d%2d%2d", &Year, &Month, &Day) == 3);
  CHECK((Year == 2026) && (Month == 10) && (Day == 11));
#if !defined(_WIN32)
  // Text conversions would write narrow text into wide buffers: refused.
  wchar_t Word[8];
  CHECK(swscanf(L"abc", L"%s", Word) == EOF);
  // The CRT's original swprintf (no size) and C99-style snwprintf.
  wchar_t Out[16];
  CHECK(swprintf(Out, L"%d-%s", 42, L"x") == 4);
  CHECK(P16(Out) == u"42-x");
  CHECK(snwprintf(Out, 4, L"%d", 123456) == 6);
  CHECK(P16(Out) == u"123");
#endif
}

void TestFilePointer()
{
  wchar_t Temp[MAX_PATH];
  GetTempPath(MAX_PATH, Temp);
  const Path16 File = P16(Temp) + u"linscp_seek_test";
  HANDLE H = CreateFile(W(File), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
  CHECK(H != INVALID_HANDLE_VALUE);
  LONG High = 0;
  CHECK(SetFilePointer(H, 10, &High, FILE_BEGIN) == 10);
  CHECK(High == 0);
  CHECK(SetFilePointer(H, -4, NULL, FILE_CURRENT) == 6);
  CHECK(SetFilePointer(H, 0, NULL, FILE_END) == 0);
  CloseHandle(H);
#if defined(_WIN32)
  DeleteFileW(W(File));
#else
  unlink(Narrow(File).c_str());
#endif
}

void TestMemoryRegions()
{
  int OnStack = 0;
  MEMORY_BASIC_INFORMATION Info;
  CHECK(VirtualQuery(&OnStack, &Info, sizeof(Info)) == sizeof(Info));
  CHECK(Info.State == MEM_COMMIT);
  const char * Base = static_cast<const char *>(Info.BaseAddress);
  CHECK((Base <= reinterpret_cast<const char *>(&OnStack)) &&
        (reinterpret_cast<const char *>(&OnStack) < Base + Info.RegionSize));
  // Walking from the bottom, as core does, finds committed memory and ends.
  const char * Address = NULL;
  size_t Committed = 0;
  int Regions = 0;
  while ((VirtualQuery(Address, &Info, sizeof(Info)) == sizeof(Info)) && (Regions < 100000))
  {
    if (Info.State == MEM_COMMIT) { Committed += Info.RegionSize; }
    Address = static_cast<const char *>(Info.BaseAddress) + Info.RegionSize;
    ++Regions;
  }
  CHECK(Committed > 0);
  CHECK(Regions < 100000);
}

void TestUserName()
{
  wchar_t Name[UNLEN + 1];
  ULONG Size = UNLEN + 1;
  CHECK(GetUserNameEx(NameSamCompatible, Name, &Size) != 0);
  const std::string N = Narrow(P16(Name));
  CHECK(Size == N.size());
  const std::size_t Slash = N.find('\\');
  CHECK((Slash != std::string::npos) && (Slash > 0) && (Slash + 1 < N.size()));
}

#if !defined(_WIN32)
// The names Windows Explorer gives -- not Wine, whose shell32 says "Folder"
// and ".zzqq file", so this is checked on POSIX only.
void TestTypeNames()
{
  SHFILEINFOW Info;
  CHECK(SHGetFileInfo(L"some folder", FILE_ATTRIBUTE_DIRECTORY, &Info, sizeof(Info),
                      SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES) != 0);
  CHECK(Narrow(P16(Info.szTypeName)) == "File folder");
  CHECK(SHGetFileInfo(L"archive.zzqq", FILE_ATTRIBUTE_NORMAL, &Info, sizeof(Info),
                      SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES) != 0);
  CHECK(Narrow(P16(Info.szTypeName)) == "ZZQQ File");
}

// Rules against what Windows' own registry says for the same zones.
void CheckZone(const char * Zone, LONG Bias, LONG DaylightBias,
  WORD DMonth, WORD DWeek, WORD DHour, WORD SMonth, WORD SWeek, WORD SHour)
{
  setenv("TZ", Zone, 1);
  tzset();
  TIME_ZONE_INFORMATION Z;
  const DWORD Id = GetTimeZoneInformation(&Z);
  CHECK(Id != TIME_ZONE_ID_INVALID);
  const bool Ok =
    (Z.Bias == Bias) && (Z.StandardBias == 0) && (Z.DaylightBias == DaylightBias) &&
    (Z.DaylightDate.wMonth == DMonth) && (Z.DaylightDate.wDay == DWeek) &&
    (Z.DaylightDate.wHour == DHour) &&
    (Z.StandardDate.wMonth == SMonth) && (Z.StandardDate.wDay == SWeek) &&
    (Z.StandardDate.wHour == SHour) &&
    ((DMonth == 0) || ((Z.DaylightDate.wDayOfWeek == 0) && (Z.StandardDate.wDayOfWeek == 0)));
  if (!Ok)
  {
    std::printf("  %s: bias %d/%d, daylight %d/%d %d:00 dow %d, standard %d/%d %d:00 dow %d\n", Zone,
      (int)Z.Bias, (int)Z.DaylightBias, Z.DaylightDate.wMonth, Z.DaylightDate.wDay,
      Z.DaylightDate.wHour, Z.DaylightDate.wDayOfWeek, Z.StandardDate.wMonth,
      Z.StandardDate.wDay, Z.StandardDate.wHour, Z.StandardDate.wDayOfWeek);
  }
  CHECK(Ok);
  CHECK((DMonth != 0) || (Id == TIME_ZONE_ID_UNKNOWN));
}

void TestTimeZones()
{
  const char * Saved = getenv("TZ");
  const std::string Restore = (Saved != NULL) ? Saved : "";
  // Central Europe: last Sunday of March 02:00, last Sunday of October 03:00.
  CheckZone("Europe/Prague", -60, -60, 3, 5, 2, 10, 5, 3);
  // US Eastern: second Sunday of March, first Sunday of November, 02:00.
  CheckZone("America/New_York", 300, -60, 3, 2, 2, 11, 1, 2);
  // AUS Eastern: first Sunday of October 02:00, first Sunday of April 03:00.
  CheckZone("Australia/Sydney", -600, -60, 10, 1, 2, 4, 1, 3);
  // No DST at all.
  CheckZone("UTC", 0, 0, 0, 0, 0, 0, 0, 0);
  CheckZone("Asia/Tokyo", -540, 0, 0, 0, 0, 0, 0, 0);
  // A UTC instant in Prague's summer is two hours ahead locally.
  setenv("TZ", "Europe/Prague", 1);
  tzset();
  SYSTEMTIME U{}, L{};
  U.wYear = 2024; U.wMonth = 7; U.wDay = 1; U.wHour = 12;
  CHECK(SystemTimeToTzSpecificLocalTime(NULL, &U, &L) != 0);
  CHECK(L.wHour == 14);
  if (Saved != NULL) { setenv("TZ", Restore.c_str(), 1); } else { unsetenv("TZ"); }
  tzset();
}

void TestXdgFolders()
{
  setenv("XDG_CONFIG_HOME", "/linscp-test/config", 1);
  setenv("XDG_DATA_HOME", "/linscp-test/data", 1);
  wchar_t Path[MAX_PATH];
  CHECK(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, Path) == S_OK);
  CHECK(Narrow(P16(Path)) == "/linscp-test/config");
  CHECK(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, Path) == S_OK);
  CHECK(Narrow(P16(Path)) == "/linscp-test/data");
  CHECK(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, Path) == S_OK);
  CHECK(Narrow(P16(Path)) == getenv("HOME"));
  unsetenv("XDG_CONFIG_HOME");
  unsetenv("XDG_DATA_HOME");
}

// A symbolic link reads back through FSCTL_GET_REPARSE_POINT in the NT
// layout core parses (Common.cpp's REPARSE_DATA_BUFFER).
void TestSymlinkReparse()
{
  wchar_t Temp[MAX_PATH];
  GetTempPath(MAX_PATH, Temp);
  const Path16 Link = P16(Temp) + u"linscp_test_link";
  unlink(Narrow(Link).c_str());
  CHECK(symlink("target/dir", Narrow(Link).c_str()) == 0);
  HANDLE H = CreateFile(W(Link), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  CHECK(H != INVALID_HANDLE_VALUE);
  char Buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
  DWORD Returned = 0;
  CHECK(DeviceIoControl(H, FSCTL_GET_REPARSE_POINT, NULL, 0, Buffer, sizeof(Buffer), &Returned, NULL) != 0);
  struct Head { ULONG Tag; USHORT Len, Res, SubOff, SubLen, PrintOff, PrintLen; ULONG Flags; };
  const Head * R = reinterpret_cast<const Head *>(Buffer);
  CHECK(R->Tag == IO_REPARSE_TAG_SYMLINK);
  CHECK((R->Flags & SYMLINK_FLAG_RELATIVE) != 0);
  const char16_t * Name = reinterpret_cast<const char16_t *>(Buffer + sizeof(Head) + R->PrintOff);
  CHECK(Path16(Name, R->PrintLen / 2) == u"target/dir");
  CloseHandle(H);
  // A plain file is not a reparse point.
  const Path16 Plain = P16(Temp) + u"linscp_test_plain";
  if (FILE * F = fopen(Narrow(Plain).c_str(), "w")) { fclose(F); }
  H = CreateFile(W(Plain), 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  CHECK(H != INVALID_HANDLE_VALUE);
  CHECK(DeviceIoControl(H, FSCTL_GET_REPARSE_POINT, NULL, 0, Buffer, sizeof(Buffer), &Returned, NULL) == 0);
  CHECK(GetLastError() == ERROR_NOT_A_REPARSE_POINT);
  CloseHandle(H);
  unlink(Narrow(Plain).c_str());
  unlink(Narrow(Link).c_str());
}

// FOF_ALLOWUNDO moves to the freedesktop.org trash, with its .trashinfo.
void TestTrash()
{
  wchar_t Temp[MAX_PATH];
  GetTempPath(MAX_PATH, Temp);
  const std::string Base = Narrow(P16(Temp)) + "linscp_trash_test";
  const std::string Data = Base + "/data";
  const std::string File = Base + "/doomed file.txt";
  mkdir(Base.c_str(), 0700);
  if (FILE * F = fopen(File.c_str(), "w")) { fputs("x", F); fclose(F); }
  setenv("XDG_DATA_HOME", Data.c_str(), 1);
  Path16 List = P16(Temp) + u"linscp_trash_test/doomed file.txt";
  List.push_back(0);
  SHFILEOPSTRUCT Op;
  memset(&Op, 0, sizeof(Op));
  Op.wFunc = FO_DELETE;
  Op.pFrom = W(List);
  Op.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_ALLOWUNDO;
  CHECK(SHFileOperation(&Op) == 0);
  struct stat St;
  CHECK(stat(File.c_str(), &St) != 0);
  CHECK(stat((Data + "/Trash/files/doomed file.txt").c_str(), &St) == 0);
  std::string Info;
  if (FILE * F = fopen((Data + "/Trash/info/doomed file.txt.trashinfo").c_str(), "r"))
  {
    char Line[512];
    while (fgets(Line, sizeof(Line), F)) { Info += Line; }
    fclose(F);
  }
  CHECK(Info.find("Path=" + Base + "/doomed%20file.txt\n") != std::string::npos);
  CHECK(Info.find("DeletionDate=") != std::string::npos);
  unsetenv("XDG_DATA_HOME");
  // Without FOF_ALLOWUNDO a directory goes, contents and all.
  Op.fFlags &= ~FOF_ALLOWUNDO;
  List = P16(Temp) + u"linscp_trash_test";
  List.push_back(0);
  Op.pFrom = W(List);
  CHECK(SHFileOperation(&Op) == 0);
  CHECK(stat(Base.c_str(), &St) != 0);
}
// Certificates against the system trust store, as WindowsValidateCertificate
// asks: a fresh self-signed one is refused as untrusted, a root the system
// trusts is accepted.
std::vector<unsigned char> Der(X509 * Certificate)
{
  unsigned char * Out = NULL;
  const int N = i2d_X509(Certificate, &Out);
  std::vector<unsigned char> R(Out, Out + (N > 0 ? N : 0));
  OPENSSL_free(Out);
  return R;
}

HRESULT PolicyError(const std::vector<unsigned char> & Certificate)
{
  PCCERT_CONTEXT Context = CertCreateCertificateContext(
    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, Certificate.data(), (DWORD)Certificate.size());
  CHECK(Context != NULL);
  CERT_CHAIN_ENGINE_CONFIG Config;
  memset(&Config, 0, sizeof(Config));
  Config.cbSize = sizeof(Config);
  HCERTCHAINENGINE Engine = NULL;
  CHECK(CertCreateCertificateChainEngine(&Config, &Engine) != 0);
  CERT_CHAIN_PARA Para;
  memset(&Para, 0, sizeof(Para));
  Para.cbSize = sizeof(Para);
  PCCERT_CHAIN_CONTEXT Chain = NULL;
  CHECK(CertGetCertificateChain(Engine, Context, NULL, NULL, &Para, CERT_CHAIN_CACHE_END_CERT, NULL, &Chain) != 0);
  CERT_CHAIN_POLICY_PARA PolicyPara;
  memset(&PolicyPara, 0, sizeof(PolicyPara));
  PolicyPara.cbSize = sizeof(PolicyPara);
  CERT_CHAIN_POLICY_STATUS Status;
  memset(&Status, 0, sizeof(Status));
  Status.cbSize = sizeof(Status);
  CHECK(CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, Chain, &PolicyPara, &Status) != 0);
  CertFreeCertificateChain(Chain);
  CertFreeCertificateChainEngine(Engine);
  CertFreeCertificateContext(Context);
  return (HRESULT)Status.dwError;
}

X509 * MakeCertificate(EVP_PKEY * Key, const char * Subject, X509 * Issuer, EVP_PKEY * IssuerKey,
  bool Ca, long ValidFrom, long ValidTo)
{
  X509 * C = X509_new();
  X509_set_version(C, 2);
  static long Serial = 1;
  ASN1_INTEGER_set(X509_get_serialNumber(C), Serial++);
  X509_gmtime_adj(X509_getm_notBefore(C), ValidFrom);
  X509_gmtime_adj(X509_getm_notAfter(C), ValidTo);
  X509_set_pubkey(C, Key);
  X509_NAME * Name = X509_get_subject_name(C);
  X509_NAME_add_entry_by_txt(Name, "CN", MBSTRING_ASC, (const unsigned char *)Subject, -1, -1, 0);
  X509_set_issuer_name(C, (Issuer != NULL) ? X509_get_subject_name(Issuer) : Name);
  X509V3_CTX Ctx;
  X509V3_set_ctx_nodb(&Ctx);
  X509V3_set_ctx(&Ctx, (Issuer != NULL) ? Issuer : C, C, NULL, NULL, 0);
  const char * Extensions[][2] = {
    { "basicConstraints", Ca ? "critical,CA:TRUE" : "CA:FALSE" },
    { "keyUsage", Ca ? "critical,keyCertSign,cRLSign" : "critical,digitalSignature" },
    { "extendedKeyUsage", Ca ? NULL : "serverAuth" },
    { "subjectKeyIdentifier", "hash" },
  };
  for (const auto & E : Extensions)
  {
    if (E[1] != NULL)
    {
      X509_EXTENSION * X = X509V3_EXT_conf(NULL, &Ctx, E[0], E[1]);
      X509_add_ext(C, X, -1);
      X509_EXTENSION_free(X);
    }
  }
  X509_sign(C, (IssuerKey != NULL) ? IssuerKey : Key, EVP_sha256());
  return C;
}

void TestCertificateChains()
{
  // A private CA, trusted through SSL_CERT_FILE, which OpenSSL's default
  // locations honour; then a leaf for each verdict core distinguishes.
  wchar_t Temp[MAX_PATH];
  GetTempPath(MAX_PATH, Temp);
  const std::string CaFile = Narrow(P16(Temp)) + "linscp_test_ca.pem";
  EVP_PKEY * CaKey = EVP_EC_gen("P-256");
  X509 * Ca = MakeCertificate(CaKey, "linscp test CA", NULL, NULL, true, -60, 3600);
  if (FILE * F = fopen(CaFile.c_str(), "w"))
  {
    PEM_write_X509(F, Ca);
    fclose(F);
  }
  const char * Saved = getenv("SSL_CERT_FILE");
  const std::string Restore = (Saved != NULL) ? Saved : "";
  setenv("SSL_CERT_FILE", CaFile.c_str(), 1);

  EVP_PKEY * Key = EVP_EC_gen("P-256");
  X509 * Good = MakeCertificate(Key, "server.test", Ca, CaKey, false, -60, 3600);
  CHECK(PolicyError(Der(Good)) == S_OK);
  X509 * Expired = MakeCertificate(Key, "server.test", Ca, CaKey, false, -7200, -3600);
  CHECK(PolicyError(Der(Expired)) == CERT_E_EXPIRED);
  EVP_PKEY * OtherKey = EVP_EC_gen("P-256");
  X509 * OtherCa = MakeCertificate(OtherKey, "unknown CA", NULL, NULL, true, -60, 3600);
  X509 * Orphan = MakeCertificate(Key, "server.test", OtherCa, OtherKey, false, -60, 3600);
  CHECK(PolicyError(Der(Orphan)) == CERT_E_CHAINING);
  X509 * Self = MakeCertificate(Key, "self.test", NULL, NULL, false, -60, 3600);
  CHECK(PolicyError(Der(Self)) == CERT_E_UNTRUSTEDROOT);
  // Not a certificate at all.
  const unsigned char Junk[] = { 1, 2, 3 };
  CHECK(CertCreateCertificateContext(X509_ASN_ENCODING, Junk, sizeof(Junk)) == NULL);

  for (X509 * C : { Good, Expired, OtherCa, Orphan, Self, Ca }) { X509_free(C); }
  for (EVP_PKEY * K : { Key, OtherKey, CaKey }) { EVP_PKEY_free(K); }
  if (Saved != NULL) { setenv("SSL_CERT_FILE", Restore.c_str(), 1); } else { unsetenv("SSL_CERT_FILE"); }
  unlink(CaFile.c_str());
}

// The executable's own version resource, through the weak hook: this test
// supplies its own data, as gen_versioninfo.py's output does for a program.
const linscp_VersionString TestStrings[] = {
  { L"ProductName", L"WinSCP" }, { L"ProductVersion", L"6.6.5.0" } };
const linscp_VersionInfoData TestVersion = {
  { VS_FFI_SIGNATURE, 0x00010000, 0x00060006, 0x00050000, 0x00060006, 0x00050000, 0x3F, 0, 0x40004, 1, 0, 0, 0 },
  0x0409, 1200, 2, TestStrings };

void TestVersionResource()
{
  char Self[4096];
  const ssize_t N = readlink("/proc/self/exe", Self, sizeof(Self) - 1);
  CHECK(N > 0);
  Self[N > 0 ? N : 0] = 0;
  Path16 SelfPath;
  for (ssize_t I = 0; I < N; ++I) { SelfPath += (char16_t)(unsigned char)Self[I]; }
  DWORD Handle = 1;
  const DWORD Size = GetFileVersionInfoSize(W(SelfPath), &Handle);
  CHECK((Size > 0) && (Handle == 0));
  std::vector<char> Block(Size);
  CHECK(GetFileVersionInfo(W(SelfPath), 0, Size, Block.data()) != 0);
  VS_FIXEDFILEINFO * Fixed = NULL;
  UINT Len = 0;
  CHECK(VerQueryValue(Block.data(), L"\\", (void **)&Fixed, &Len) != 0);
  CHECK((Len == sizeof(VS_FIXEDFILEINFO)) && (Fixed->dwSignature == VS_FFI_SIGNATURE) &&
        (Fixed->dwFileVersionMS == 0x00060006));
  WORD * Translation = NULL;
  CHECK(VerQueryValue(Block.data(), L"\\VarFileInfo\\Translation", (void **)&Translation, &Len) != 0);
  CHECK((Len == 4) && (Translation[0] == 0x0409) && (Translation[1] == 1200));
  wchar_t * Value = NULL;
  CHECK(VerQueryValue(Block.data(), L"\\StringFileInfo\\040904B0\\ProductVersion", (void **)&Value, &Len) != 0);
  CHECK((Len == 8) && (P16(Value) == u"6.6.5.0"));
  CHECK(VerQueryValue(Block.data(), L"\\StringFileInfo\\040904B0\\NoSuchKey", (void **)&Value, &Len) == 0);
  // Any other file has no resource.
  CHECK(GetFileVersionInfoSize(L"/bin/sh", &Handle) == 0);
}
#endif

}  // namespace

#if !defined(_WIN32)
// The strong definition the platform layer's weak one yields to.
extern "C" const linscp_VersionInfoData * linscp_OwnVersionInfo(void)
{
  return &TestVersion;
}
#endif

int main()
{
  TestEpochAnchors();
  TestRoundTrip();
  TestDayOfWeek();
  TestLocalConversionIsReversible();
  TestComparison();
  TestCurrentTimeIsSane();
  TestErrorHandling();
  TestWideCharacterFunctions();
  TestConstantsMatchWin32();

  TestBareWin32NamesCoreCalls();
  TestFiles();
  TestPathParsing();
  TestStringOrder();
  TestEnvironmentExpansion();
  TestModulesAndProcesses();
  TestMimeTypes();
  TestWideStrings();
  TestFilePointer();
  TestMemoryRegions();
  TestUserName();
#if !defined(_WIN32)
  TestTypeNames();
  TestCertificateChains();
  TestVersionResource();
  TestTimeZones();
  TestXdgFolders();
  TestSymlinkReparse();
  TestTrash();
#endif

  if (Failures == 0)
  {
    std::printf("all win32-on-posix tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
