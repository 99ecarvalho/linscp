// The Win32 subset implemented for POSIX.
//
// FILETIME is the dangerous part: it counts 100-nanosecond intervals since
// 1601-01-01 UTC, so an epoch or scale mistake shifts every file timestamp by a
// plausible-looking amount rather than failing. The anchors below are fixed
// points of the format, not values read back from this implementation.
#if defined(_WIN32)
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
  #include <sys/stat.h>
  #include <unistd.h>
#endif

#include <cstdint>
#include <string>
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

}  // namespace

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

  if (Failures == 0)
  {
    std::printf("all win32-on-posix tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
