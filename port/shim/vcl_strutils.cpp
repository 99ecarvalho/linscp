#include "vcl_strutils.h"

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>

#include "vcl_object.h"
#include "vcl_sysutils.h"

#if defined(_WIN32)
  #include <windows.h>

  #include "vcl_win32_undef.h"
  const wchar_t PathDelim = L'\\';
  const UnicodeString sLineBreak(L"\r\n");
#else
  #include "win32_on_posix.h"
  #include <sys/stat.h>
  #include <sys/utsname.h>
  #include <unistd.h>
  const wchar_t PathDelim = L'/';
  const UnicodeString sLineBreak(L"\n");
#endif

namespace {

// Both separators are recognised when parsing, whatever the host, so a path
// that arrived from a remote server is split the same way on either platform.
// ASCII alphanumeric test, written out rather than calling iswalnum: that is
// one of the libc wide functions compiled against a 4-byte wchar_t (ADR 0001).
bool IsAsciiAlnum(wchar_t C)
{
  return (C >= L'0' && C <= L'9') || (C >= L'A' && C <= L'Z') ||
         (C >= L'a' && C <= L'z');
}

bool IsSep(wchar_t C)
{
  return C == L'\\' || C == L'/';
}

std::string Narrow(const UnicodeString & S)
{
  return linscp::Utf16ToUtf8(S);
}

UnicodeString FromAscii(const char * S)
{
  UnicodeString R;
  for (const char * P = S; *P != '\0'; ++P)
  {
    R += static_cast<wchar_t>(static_cast<unsigned char>(*P));
  }
  return R;
}

// Delphi accepts leading and trailing whitespace and an optional sign, and
// treats a leading '$' or '0x' as hexadecimal.
bool ParseInt64(const UnicodeString & S, __int64 & Value)
{
  const UnicodeString T = Trim(S);
  if (T.IsEmpty())
  {
    return false;
  }
  std::string A = Narrow(T);
  int Base = 10;
  std::size_t Start = 0;
  bool Negative = false;
  if (A[Start] == '+' || A[Start] == '-')
  {
    Negative = A[Start] == '-';
    ++Start;
  }
  if (Start < A.size() && A[Start] == '$')
  {
    Base = 16;
    ++Start;
  }
  else if (Start + 1 < A.size() && A[Start] == '0' &&
           (A[Start + 1] == 'x' || A[Start + 1] == 'X'))
  {
    Base = 16;
    Start += 2;
  }
  if (Start >= A.size())
  {
    return false;
  }
  errno = 0;
  char * End = nullptr;
  const unsigned long long Raw =
    std::strtoull(A.c_str() + Start, &End, Base);
  if (errno == ERANGE || End == nullptr || *End != '\0')
  {
    return false;      // trailing garbage is a failure, as in Delphi
  }
  Value = Negative ? -static_cast<__int64>(Raw) : static_cast<__int64>(Raw);
  return true;
}

}  // namespace

// --- numbers ---------------------------------------------------------------

UnicodeString IntToStr(__int64 Value)
{
  char Buf[32];
  std::snprintf(Buf, sizeof(Buf), "%lld", static_cast<long long>(Value));
  return FromAscii(Buf);
}

UnicodeString UIntToStr(unsigned __int64 Value)
{
  char Buf[32];
  std::snprintf(Buf, sizeof(Buf), "%llu",
                static_cast<unsigned long long>(Value));
  return FromAscii(Buf);
}

UnicodeString IntToHex(__int64 Value, int Digits)
{
  char Buf[40];
  if (Digits < 0) { Digits = 0; }
  std::snprintf(Buf, sizeof(Buf), "%0*llX", Digits,
                static_cast<unsigned long long>(Value));
  return FromAscii(Buf);
}

bool TryStrToInt64(const UnicodeString & S, __int64 & Value)
{
  return ParseInt64(S, Value);
}

bool TryStrToInt(const UnicodeString & S, int & Value)
{
  __int64 V = 0;
  if (!ParseInt64(S, V))
  {
    return false;
  }
  Value = static_cast<int>(V);
  return true;
}

int StrToInt(const UnicodeString & S)
{
  int V = 0;
  if (!TryStrToInt(S, V))
  {
    throw EConvertError(UnicodeString(L"'") + S +
                        UnicodeString(L"' is not a valid integer value"));
  }
  return V;
}

__int64 StrToInt64(const UnicodeString & S)
{
  __int64 V = 0;
  if (!TryStrToInt64(S, V))
  {
    throw EConvertError(UnicodeString(L"'") + S +
                        UnicodeString(L"' is not a valid integer value"));
  }
  return V;
}

int StrToIntDef(const UnicodeString & S, int Default)
{
  int V = 0;
  return TryStrToInt(S, V) ? V : Default;
}

__int64 StrToInt64Def(const UnicodeString & S, __int64 Default)
{
  __int64 V = 0;
  return TryStrToInt64(S, V) ? V : Default;
}

UnicodeString FloatToStr(double Value)
{
  char Buf[64];
  // %g, matching Delphi's general shortest representation closely enough for
  // the diagnostic and log output core uses it for.
  std::snprintf(Buf, sizeof(Buf), "%g", Value);
  return FromAscii(Buf);
}

bool TryStrToFloat(const UnicodeString & S, double & Value)
{
  const std::string A = Narrow(Trim(S));
  if (A.empty())
  {
    return false;
  }
  errno = 0;
  char * End = nullptr;
  const double V = std::strtod(A.c_str(), &End);
  if (errno == ERANGE || End == nullptr || *End != '\0')
  {
    return false;
  }
  Value = V;
  return true;
}

double StrToFloatDef(const UnicodeString & S, double Default)
{
  double V;
  return TryStrToFloat(S, V) ? V : Default;
}

// --- comparison ------------------------------------------------------------

bool SameText(const UnicodeString & A, const UnicodeString & B)
{
  return A.CompareIC(B) == 0;
}

bool SameStr(const UnicodeString & A, const UnicodeString & B)
{
  return A == B;
}

int CompareText(const UnicodeString & A, const UnicodeString & B)
{
  return A.CompareIC(B);
}

int CompareStr(const UnicodeString & A, const UnicodeString & B)
{
  return A.Compare(B);
}

// --- searching -------------------------------------------------------------

int Pos(const UnicodeString & Substr, const UnicodeString & S)
{
  return S.Pos(Substr);
}

int PosEx(const UnicodeString & Substr, const UnicodeString & S, int Offset)
{
  if (Offset < 1)
  {
    Offset = 1;
  }
  if (Offset > S.Length())
  {
    return 0;
  }
  const int Found = S.SubString(Offset).Pos(Substr);
  return Found == 0 ? 0 : Found + Offset - 1;
}

int LastDelimiter(const UnicodeString & Delimiters, const UnicodeString & S)
{
  return S.LastDelimiter(Delimiters);
}

bool StartsStr(const UnicodeString & Prefix, const UnicodeString & S)
{
  return Prefix.Length() <= S.Length() &&
         S.SubString(1, Prefix.Length()) == Prefix;
}

bool StartsText(const UnicodeString & Prefix, const UnicodeString & S)
{
  return Prefix.Length() <= S.Length() &&
         S.SubString(1, Prefix.Length()).CompareIC(Prefix) == 0;
}

bool EndsStr(const UnicodeString & Suffix, const UnicodeString & S)
{
  return Suffix.Length() <= S.Length() &&
         S.SubString(S.Length() - Suffix.Length() + 1) == Suffix;
}

bool EndsText(const UnicodeString & Suffix, const UnicodeString & S)
{
  return Suffix.Length() <= S.Length() &&
         S.SubString(S.Length() - Suffix.Length() + 1).CompareIC(Suffix) == 0;
}

bool ContainsStr(const UnicodeString & S, const UnicodeString & Substr)
{
  return S.Pos(Substr) > 0;
}

bool ContainsText(const UnicodeString & S, const UnicodeString & Substr)
{
  return S.LowerCase().Pos(Substr.LowerCase()) > 0;
}

// --- transformation --------------------------------------------------------

UnicodeString Trim(const UnicodeString & S)      { return S.Trim(); }
UnicodeString TrimLeft(const UnicodeString & S)  { return S.TrimLeft(); }
UnicodeString TrimRight(const UnicodeString & S) { return S.TrimRight(); }
UnicodeString LowerCase(const UnicodeString & S) { return S.LowerCase(); }
UnicodeString UpperCase(const UnicodeString & S) { return S.UpperCase(); }

UnicodeString ReplaceStr(const UnicodeString & S, const UnicodeString & From,
                         const UnicodeString & To)
{
  if (From.IsEmpty())
  {
    return S;
  }
  UnicodeString Out;
  int I = 1;
  while (I <= S.Length())
  {
    const int Found = PosEx(From, S, I);
    if (Found == 0)
    {
      Out += S.SubString(I);
      break;
    }
    Out += S.SubString(I, Found - I);
    Out += To;
    I = Found + From.Length();
  }
  return Out;
}

UnicodeString ReplaceText(const UnicodeString & S, const UnicodeString & From,
                          const UnicodeString & To)
{
  if (From.IsEmpty())
  {
    return S;
  }
  const UnicodeString Lower = S.LowerCase();
  const UnicodeString FromLower = From.LowerCase();
  UnicodeString Out;
  int I = 1;
  while (I <= S.Length())
  {
    const int Found = PosEx(FromLower, Lower, I);
    if (Found == 0)
    {
      Out += S.SubString(I);
      break;
    }
    Out += S.SubString(I, Found - I);
    Out += To;
    I = Found + From.Length();
  }
  return Out;
}

UnicodeString StringOfChar(wchar_t C, int Count)
{
  return Count > 0 ? UnicodeString(C, Count) : UnicodeString();
}

UnicodeString DupeString(const UnicodeString & S, int Count)
{
  UnicodeString Out;
  for (int I = 0; I < Count; ++I)
  {
    Out += S;
  }
  return Out;
}

// --- encoding --------------------------------------------------------------

UnicodeString UTF8ToString(const UTF8String & S)
{
  return ToUnicodeString(S);
}

UnicodeString UTF8ToString(const RawByteString & S)
{
  return linscp::Utf8ToUtf16(S.c_str(), static_cast<std::size_t>(S.Length()));
}

UTF8String StringToUTF8(const UnicodeString & S)
{
  return UTF8String(S);
}

// --- paths -----------------------------------------------------------------

bool IsPathDelimiter(const UnicodeString & S, int Index)
{
  return Index >= 1 && Index <= S.Length() && IsSep(S[Index]);
}

UnicodeString IncludeTrailingBackslash(const UnicodeString & S)
{
  if (!S.IsEmpty() && IsSep(S[S.Length()]))
  {
    return S;
  }
  return S + PathDelim;
}

UnicodeString ExcludeTrailingBackslash(const UnicodeString & S)
{
  if (!S.IsEmpty() && IsSep(S[S.Length()]))
  {
    return S.SubString(1, S.Length() - 1);
  }
  return S;
}

UnicodeString IncludeTrailingPathDelimiter(const UnicodeString & S)
{
  return IncludeTrailingBackslash(S);
}

UnicodeString ExcludeTrailingPathDelimiter(const UnicodeString & S)
{
  return ExcludeTrailingBackslash(S);
}

UnicodeString ExtractFileName(const UnicodeString & FileName)
{
  const int P = FileName.LastDelimiter(UnicodeString(L"\\/:"));
  return P == 0 ? FileName : FileName.SubString(P + 1);
}

UnicodeString ExtractFilePath(const UnicodeString & FileName)
{
  const int P = FileName.LastDelimiter(UnicodeString(L"\\/:"));
  return P == 0 ? UnicodeString() : FileName.SubString(1, P);
}

UnicodeString ExtractFileDir(const UnicodeString & FileName)
{
  const int P = FileName.LastDelimiter(UnicodeString(L"\\/:"));
  if (P == 0)
  {
    return UnicodeString();
  }
  // Delphi keeps the trailing separator only when what remains is a root, and
  // drops it otherwise. Three cases count as a root:
  //   "/c.txt"     -> "/"      the separator is the whole path
  //   "C:b.txt"    -> "C:"     drive-relative; the colon is kept
  //   "C:\\c.txt"   -> "C:\\"    a drive root
  // Anything else, such as "a\\b", yields "a".
  const bool RootSlash = (P == 1);
  const bool DriveRelative = (P == 2 && FileName[2] == L':');
  const bool DriveRoot = (P == 3 && FileName[2] == L':' && IsSep(FileName[3]));
  if (RootSlash || DriveRelative || DriveRoot)
  {
    return FileName.SubString(1, P);
  }
  return FileName.SubString(1, P - 1);
}

UnicodeString ExtractFileExt(const UnicodeString & FileName)
{
  const UnicodeString Name = ExtractFileName(FileName);
  const int P = Name.LastDelimiter(UnicodeString(L"."));
  return P == 0 ? UnicodeString() : Name.SubString(P);
}

UnicodeString ExtractFileDrive(const UnicodeString & FileName)
{
  if (FileName.Length() >= 2 && FileName[2] == L':')
  {
    return FileName.SubString(1, 2);
  }
  // A UNC path: keep \\server\share.
  if (FileName.Length() >= 2 && IsSep(FileName[1]) && IsSep(FileName[2]))
  {
    int Slashes = 0;
    for (int I = 3; I <= FileName.Length(); ++I)
    {
      if (IsSep(FileName[I]) && ++Slashes == 2)
      {
        return FileName.SubString(1, I - 1);
      }
    }
    return FileName;
  }
  return UnicodeString();
}

UnicodeString ChangeFileExt(const UnicodeString & FileName,
                            const UnicodeString & Extension)
{
  const UnicodeString Ext = ExtractFileExt(FileName);
  if (Ext.IsEmpty())
  {
    return FileName + Extension;
  }
  return FileName.SubString(1, FileName.Length() - Ext.Length()) + Extension;
}

// --- filesystem actions ----------------------------------------------------

UnicodeString GetCurrentDir()
{
#if defined(_WIN32)
  wchar_t Buf[4096];
  const DWORD N = GetCurrentDirectoryW(
    static_cast<DWORD>(sizeof(Buf) / sizeof(Buf[0])), Buf);
  return N == 0 ? UnicodeString() : UnicodeString(Buf, static_cast<int>(N));
#else
  char Buf[4096];
  if (getcwd(Buf, sizeof(Buf)) == nullptr)
  {
    return UnicodeString();
  }
  return linscp::Utf8ToUtf16(Buf, std::string(Buf).size());
#endif
}

bool SetCurrentDir(const UnicodeString & Dir)
{
#if defined(_WIN32)
  return SetCurrentDirectoryW(Dir.c_str()) != 0;
#else
  return chdir(Narrow(Dir).c_str()) == 0;
#endif
}

bool CreateDir(const UnicodeString & Dir)
{
#if defined(_WIN32)
  return CreateDirectoryW(Dir.c_str(), nullptr) != 0;
#else
  return mkdir(Narrow(Dir).c_str(), 0777) == 0;
#endif
}

bool RemoveDir(const UnicodeString & Dir)
{
#if defined(_WIN32)
  return RemoveDirectoryW(Dir.c_str()) != 0;
#else
  return rmdir(Narrow(Dir).c_str()) == 0;
#endif
}

bool DeleteFile(const UnicodeString & FileName)
{
#if defined(_WIN32)
  return DeleteFileW(FileName.c_str()) != 0;
#else
  return unlink(Narrow(FileName).c_str()) == 0;
#endif
}

bool RenameFile(const UnicodeString & OldName, const UnicodeString & NewName)
{
#if defined(_WIN32)
  return MoveFileW(OldName.c_str(), NewName.c_str()) != 0;
#else
  return std::rename(Narrow(OldName).c_str(), Narrow(NewName).c_str()) == 0;
#endif
}

UnicodeString ExpandUNCFileName(const UnicodeString & FileName)
{
#if defined(_WIN32)
  wchar_t Buf[4096];
  const DWORD N = GetFullPathNameW(
    FileName.c_str(), static_cast<DWORD>(sizeof(Buf) / sizeof(Buf[0])),
    Buf, nullptr);
  return N == 0 ? FileName : UnicodeString(Buf, static_cast<int>(N));
#else
  char Buf[4096];
  if (realpath(Narrow(FileName).c_str(), Buf) == nullptr)
  {
    return FileName;     // Delphi leaves an unresolvable name untouched
  }
  return linscp::Utf8ToUtf16(Buf, std::string(Buf).size());
#endif
}

// --- control flow ----------------------------------------------------------

void Abort()
{
  // EAbort is Delphi's silent abort: handlers swallow it without reporting.
  throw EAbort(UnicodeString());
}

void RaiseLastOSError()
{
#if defined(_WIN32)
  const int Code = static_cast<int>(GetLastError());
#else
  int Code = static_cast<int>(GetLastError());
  if ((Code == 0) && (errno != 0))
  {
    Code = static_cast<int>(Win32ErrorFromErrno(errno));
  }
#endif
  RaiseLastOSError(Code);
}

void RaiseLastOSError(int LastError)
{
  RaiseLastOSError(LastError, UnicodeString());
}

void RaiseLastOSError(int LastError, const UnicodeString & AdditionalInfo)
{
  if (LastError == 0)
  {
    throw EOSError(UnicodeString(L"A call to an OS function failed"));
  }
  throw EOSError(UnicodeString(L"System Error.  Code: ") +
                 IntToStr(static_cast<__int64>(LastError)) + UnicodeString(L".") +
                 sLineBreak + SysErrorMessage(LastError) + AdditionalInfo);
}

namespace {

UnicodeString TrimSystemMessage(UnicodeString S)
{
  while ((S.Length() > 0) && ((S[S.Length()] <= L' ') || (S[S.Length()] == L'.')))
  {
    S.SetLength(S.Length() - 1);
  }
  return S;
}

}  // namespace

UnicodeString SysErrorMessage(int ErrorCode)
{
#if defined(_WIN32)
  wchar_t Buffer[1024];
  const DWORD Len = FormatMessageW(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
    static_cast<DWORD>(ErrorCode), 0, Buffer, sizeof(Buffer) / sizeof(Buffer[0]),
    nullptr);
  return TrimSystemMessage(UnicodeString(Buffer, static_cast<int>(Len)));
#else
  // Windows' own English wording, for every code port/platform can produce.
  struct TMessage { int Code; const wchar_t * Text; };
  static const TMessage Messages[] = {
    {ERROR_SUCCESS, L"The operation completed successfully."},
    {ERROR_FILE_NOT_FOUND, L"The system cannot find the file specified."},
    {ERROR_PATH_NOT_FOUND, L"The system cannot find the path specified."},
    {ERROR_ACCESS_DENIED, L"Access is denied."},
    {ERROR_INVALID_HANDLE, L"The handle is invalid."},
    {ERROR_NOT_ENOUGH_MEMORY, L"Not enough memory resources are available to process this command."},
    {ERROR_NO_MORE_FILES, L"There are no more files."},
    {ERROR_SHARING_VIOLATION, L"The process cannot access the file because it is being used by another process."},
    {ERROR_HANDLE_DISK_FULL, L"The disk is full."},
    {ERROR_NOT_SUPPORTED, L"The request is not supported."},
    {ERROR_FILE_EXISTS, L"The file exists."},
    {ERROR_INVALID_PARAMETER, L"The parameter is incorrect."},
    {ERROR_DISK_FULL, L"There is not enough space on the disk."},
    {ERROR_ALREADY_EXISTS, L"Cannot create a file when that file already exists."},
    {ERROR_MORE_DATA, L"More data is available."},
    {ERROR_NO_MORE_ITEMS, L"No more data is available."},
    {ERROR_CANCELLED, L"The operation was canceled by the user."},
  };
  for (const TMessage & M : Messages)
  {
    if (M.Code == ErrorCode)
    {
      return TrimSystemMessage(UnicodeString(M.Text));
    }
  }
  return UnicodeString(L"Unknown error ") + IntToStr(static_cast<__int64>(ErrorCode));
#endif
}

// --- System.Random ----------------------------------------------------------

int RandSeed = 0;

void Randomize()
{
#if defined(_WIN32)
  LARGE_INTEGER Counter;
  RandSeed = QueryPerformanceCounter(&Counter)
    ? static_cast<int>(Counter.LowPart) : static_cast<int>(GetTickCount());
#else
  RandSeed = static_cast<int>(GetTickCount64() * 2654435761u);
#endif
}

namespace {

std::uint32_t NextRandSeed()
{
  const std::uint32_t Next = static_cast<std::uint32_t>(RandSeed) * 0x08088405u + 1u;
  RandSeed = static_cast<int>(Next);
  return Next;
}

}  // namespace

int Random(int Range)
{
  const std::uint64_t Next = NextRandSeed();
  return static_cast<int>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(Range)) * Next) >> 32);
}

double Random()
{
  return static_cast<double>(NextRandSeed()) * (1.0 / 4294967296.0);
}

// --- environment -----------------------------------------------------------

UnicodeString GetEnvironmentVariable(const UnicodeString & Name)
{
#if defined(_WIN32)
  wchar_t Buf[32767];
  const DWORD N = GetEnvironmentVariableW(
    Name.c_str(), Buf, static_cast<DWORD>(sizeof(Buf) / sizeof(Buf[0])));
  return N == 0 ? UnicodeString() : UnicodeString(Buf, static_cast<int>(N));
#else
  const char * V = ::getenv(Narrow(Name).c_str());
  return V == nullptr ? UnicodeString()
                      : linscp::Utf8ToUtf16(V, std::char_traits<char>::length(V));
#endif
}

bool SetEnvironmentVariable(const UnicodeString & Name,
                            const UnicodeString & Value)
{
#if defined(_WIN32)
  return SetEnvironmentVariableW(Name.c_str(), Value.c_str()) != 0;
#else
  return ::setenv(Narrow(Name).c_str(), Narrow(Value).c_str(), 1) == 0;
#endif
}

UnicodeString ExpandFileName(const UnicodeString & FileName)
{
  return ExpandUNCFileName(FileName);
}

UnicodeString ExpandEnvironmentVariables(const UnicodeString & S)
{
#if defined(_WIN32)
  wchar_t Buf[32767];
  const DWORD N = ExpandEnvironmentStringsW(
    S.c_str(), Buf, static_cast<DWORD>(sizeof(Buf) / sizeof(Buf[0])));
  return N == 0 ? S : UnicodeString(Buf);
#else
  // $VAR and ${VAR}, the POSIX equivalent of Windows' %VAR%.
  UnicodeString Out;
  int I = 1;
  while (I <= S.Length())
  {
    if (S[I] != L'$')
    {
      Out += S[I];
      ++I;
      continue;
    }
    ++I;
    const bool Braced = (I <= S.Length() && S[I] == L'{');
    if (Braced)
    {
      ++I;
    }
    UnicodeString Name;
    while (I <= S.Length() &&
           (Braced ? S[I] != L'}' : (IsAsciiAlnum(S[I]) || S[I] == L'_')))
    {
      Name += S[I];
      ++I;
    }
    if (Braced && I <= S.Length())
    {
      ++I;
    }
    Out += GetEnvironmentVariable(Name);
  }
  return Out;
#endif
}

bool AnsiSameText(const UnicodeString & A, const UnicodeString & B)
{
  return A.CompareIC(B) == 0;
}

bool AnsiSameStr(const UnicodeString & A, const UnicodeString & B)
{
  return A == B;
}

// --- host version ----------------------------------------------------------
//
// On Windows these are the OS version; on POSIX they describe the kernel, which
// is the nearest honest answer to what core asks for when reporting the
// environment.

namespace {

struct THostVersion
{
  unsigned Major = 0;
  unsigned Minor = 0;
  unsigned Build = 0;
  UnicodeString CSD;
};

THostVersion ReadHostVersion()
{
  THostVersion V;
#if defined(_WIN32)
  // RtlGetVersion takes the same record as GetVersionExW.
  using TRtlGetVersion = LONG (WINAPI *)(OSVERSIONINFOW *);
  OSVERSIONINFOW Info{};
  Info.dwOSVersionInfoSize = sizeof(Info);
  const HMODULE Ntdll = GetModuleHandleW(L"ntdll.dll");
  const auto RtlGetVersion = (Ntdll != nullptr)
    ? reinterpret_cast<TRtlGetVersion>(reinterpret_cast<void *>(GetProcAddress(Ntdll, "RtlGetVersion")))
    : nullptr;
  bool Got = (RtlGetVersion != nullptr) && (RtlGetVersion(&Info) == 0);
  if (!Got)
  {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    Got = (GetVersionExW(&Info) != 0);
    #pragma clang diagnostic pop
  }
  if (Got)
  {
    V.Major = Info.dwMajorVersion;
    V.Minor = Info.dwMinorVersion;
    V.Build = Info.dwBuildNumber;
    V.CSD = UnicodeString(Info.szCSDVersion);
  }
#else
  // The kernel, the nearest honest answer to what core asks when it reports
  // the environment.
  struct utsname Name{};
  if (uname(&Name) == 0)
  {
    int Major = 0, Minor = 0, Patch = 0;
    std::sscanf(Name.release, "%d.%d.%d", &Major, &Minor, &Patch);
    V.Major = static_cast<unsigned>(Major);
    V.Minor = static_cast<unsigned>(Minor);
    V.Build = static_cast<unsigned>(Patch);
    V.CSD = linscp::Utf8ToUtf16(Name.sysname, std::char_traits<char>::length(Name.sysname));
  }
#endif
  return V;
}

const THostVersion & HostVersion()
{
  static const THostVersion Version = ReadHostVersion();
  return Version;
}

}  // namespace

unsigned Win32MajorVersion() { return HostVersion().Major; }
unsigned Win32MinorVersion() { return HostVersion().Minor; }
unsigned Win32BuildNumber() { return HostVersion().Build; }
UnicodeString Win32CSDVersion() { return HostVersion().CSD; }

bool CheckWin32Version(int AMajor, int AMinor)
{
  const int Major = static_cast<int>(Win32MajorVersion());
  const int Minor = static_cast<int>(Win32MinorVersion());
  return (Major > AMajor) || ((Major == AMajor) && (Minor >= AMinor));
}

UnicodeString LeftStr(const UnicodeString & AText, int ACount)
{
  return AText.SubString(1, ACount);
}

UnicodeString RightStr(const UnicodeString & AText, int ACount)
{
  if (ACount >= AText.Length())
  {
    return AText;
  }
  return AText.SubString(AText.Length() + 1 - ACount, ACount);
}

UnicodeString MidStr(const UnicodeString & AText, int AStart, int ACount)
{
  return AText.SubString(AStart, ACount);
}

UnicodeString AnsiLowerCase(const UnicodeString & S) { return LowerCase(S); }
UnicodeString AnsiUpperCase(const UnicodeString & S) { return UpperCase(S); }

UnicodeString AnsiReplaceStr(const UnicodeString & AText, const UnicodeString & AFromText,
                             const UnicodeString & AToText)
{
  return ReplaceStr(AText, AFromText, AToText);
}

bool IsDelimiter(const UnicodeString & Delimiters, const UnicodeString & S, int Index)
{
  if ((Index <= 0) || (Index > S.Length()))
  {
    return false;
  }
  return Delimiters.Pos(UnicodeString(S[Index])) > 0;
}

namespace {

template <typename T>
UnicodeString HexOfWidth(T Value)
{
  using U = std::make_unsigned_t<T>;
  return IntToHex(static_cast<__int64>(static_cast<unsigned __int64>(static_cast<U>(Value))),
                  static_cast<int>(sizeof(T) * 2));
}

}  // namespace

UnicodeString IntToHex(signed char Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(unsigned char Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(short Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(unsigned short Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(int Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(unsigned int Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(__int64 Value) { return HexOfWidth(Value); }
UnicodeString IntToHex(unsigned __int64 Value) { return HexOfWidth(Value); }

int IndexStr(const UnicodeString & AText, const UnicodeString * AValues, int AValues_High)
{
  for (int I = 0; I <= AValues_High; ++I)
  {
    if (AValues[I] == AText)
    {
      return I;
    }
  }
  return -1;
}

int IndexText(const UnicodeString & AText, const UnicodeString * AValues, int AValues_High)
{
  for (int I = 0; I <= AValues_High; ++I)
  {
    if (SameText(AValues[I], AText))
    {
      return I;
    }
  }
  return -1;
}

int FindDelimiter(const UnicodeString & Delimiters, const UnicodeString & S, int StartIdx)
{
  for (int I = (StartIdx < 1) ? 1 : StartIdx; I <= S.Length(); ++I)
  {
    if (Delimiters.Pos(UnicodeString(S[I])) > 0)
    {
      return I;
    }
  }
  return 0;
}

int AnsiCompareText(const UnicodeString & S1, const UnicodeString & S2)
{
  return CompareText(S1, S2);
}

int StrLIComp(const wchar_t * Str1, const wchar_t * Str2, unsigned MaxLen)
{
  for (unsigned I = 0; I < MaxLen; ++I)
  {
    wchar_t A = Str1[I], B = Str2[I];
    if ((A >= L'A') && (A <= L'Z')) { A = static_cast<wchar_t>(A + 32); }
    if ((B >= L'A') && (B <= L'Z')) { B = static_cast<wchar_t>(B + 32); }
    if (A != B)
    {
      return (A < B) ? -1 : 1;
    }
    if (A == L'\0')
    {
      return 0;
    }
  }
  return 0;
}

double StrToFloat(const UnicodeString & S)
{
  double V;
  if (!TryStrToFloat(S, V))
  {
    throw EConvertError(UnicodeString(L"'") + S + L"' is not a valid floating point value");
  }
  return V;
}

int random(int num)
{
  return static_cast<int>((static_cast<long long>(std::rand()) * num) /
                          (static_cast<long long>(RAND_MAX) + 1));
}

// --- UnicodeString members that need the RTL ---------------------------------

int UnicodeString::ToInt() const
{
  return StrToInt(*this);
}

int UnicodeString::ToIntDef(int Default) const
{
  return StrToIntDef(*this, Default);
}

namespace {

// C++Builder's wide printf conventions, without libc's wide functions (whose
// wchar_t is four bytes here): numbers through narrow snprintf, strings
// padded and truncated on UTF-16 directly.
UnicodeString WideVFormat(const wchar_t * Format, va_list Args)
{
  UnicodeString Out;
  const wchar_t * P = Format;
  while ((P != nullptr) && (*P != L'\0'))
  {
    if (*P != L'%')
    {
      Out += *P++;
      continue;
    }
    ++P;
    if (*P == L'%')
    {
      Out += L'%';
      ++P;
      continue;
    }
    std::string Spec = "%";
    bool Left = false;
    while ((*P == L'-') || (*P == L'+') || (*P == L' ') || (*P == L'#') || (*P == L'0'))
    {
      Left = Left || (*P == L'-');
      Spec += static_cast<char>(*P++);
    }
    int Width = -1;
    if (*P == L'*') { Width = va_arg(Args, int); ++P; if (Width < 0) { Left = true; Width = -Width; } }
    else { while ((*P >= L'0') && (*P <= L'9')) { Width = ((Width < 0) ? 0 : Width) * 10 + (*P++ - L'0'); } }
    int Precision = -1;
    if (*P == L'.')
    {
      ++P;
      Precision = 0;
      if (*P == L'*') { Precision = va_arg(Args, int); ++P; }
      else { while ((*P >= L'0') && (*P <= L'9')) { Precision = Precision * 10 + (*P++ - L'0'); } }
    }
    // Length modifiers.
    enum { lNone, lH, lHH, lL, lLL, lBigL, lZ } Len = lNone;
    if (*P == L'h') { ++P; Len = lH; if (*P == L'h') { ++P; Len = lHH; } }
    else if (*P == L'l') { ++P; Len = lL; if (*P == L'l') { ++P; Len = lLL; } }
    else if (*P == L'L') { ++P; Len = lBigL; }
    else if ((*P == L'z') || (*P == L'j') || (*P == L't')) { ++P; Len = lZ; }
    else if ((P[0] == L'I') && (P[1] == L'6') && (P[2] == L'4')) { P += 3; Len = lLL; }
    const wchar_t Conv = *P;
    if (Conv == L'\0')
    {
      break;
    }
    ++P;

    auto Pad = [&](UnicodeString S) {
      if ((Precision >= 0) && (S.Length() > Precision)) { S.SetLength(Precision); }
      while (S.Length() < Width) { S = Left ? S + L" " : UnicodeString(L" ") + S; }
      return S;
    };
    auto Narrow = [&](auto Value, const char * LengthText, char C) {
      std::string F = Spec;
      if (Width >= 0) { F += std::to_string(Width); }
      if (Precision >= 0) { F += "." + std::to_string(Precision); }
      F += LengthText;
      F += C;
      char Buf[512];
      std::snprintf(Buf, sizeof(Buf), F.c_str(), Value);
      Out += UnicodeString(Buf);
    };

    switch (Conv)
    {
      case L'd': case L'i':
        if ((Len == lLL) || (Len == lZ)) { Narrow(va_arg(Args, long long), "ll", 'd'); }
        else if (Len == lL) { Narrow(va_arg(Args, long), "l", 'd'); }
        else { Narrow(va_arg(Args, int), "", 'd'); }
        break;
      case L'u': case L'x': case L'X': case L'o':
      {
        const char C = static_cast<char>(Conv);
        if ((Len == lLL) || (Len == lZ)) { Narrow(va_arg(Args, unsigned long long), "ll", C); }
        else if (Len == lL) { Narrow(va_arg(Args, unsigned long), "l", C); }
        else { Narrow(va_arg(Args, unsigned int), "", C); }
        break;
      }
      case L'f': case L'F': case L'e': case L'E': case L'g': case L'G':
        if (Len == lBigL) { Narrow(va_arg(Args, long double), "L", static_cast<char>(Conv)); }
        else { Narrow(va_arg(Args, double), "", static_cast<char>(Conv)); }
        break;
      case L'p':
        Narrow(va_arg(Args, void *), "", 'p');
        break;
      case L'c':
        if (Len == lH) { Out += Pad(UnicodeString(static_cast<wchar_t>(static_cast<unsigned char>(va_arg(Args, int))))); }
        else { Out += Pad(UnicodeString(static_cast<wchar_t>(va_arg(Args, int)))); }
        break;
      case L'C':
        Out += Pad(UnicodeString(static_cast<wchar_t>(static_cast<unsigned char>(va_arg(Args, int)))));
        break;
      case L's':
        if (Len == lH)
        {
          const char * S = va_arg(Args, const char *);
          Out += Pad(UnicodeString((S != nullptr) ? S : "(null)"));
        }
        else
        {
          const wchar_t * S = va_arg(Args, const wchar_t *);
          Out += Pad(UnicodeString((S != nullptr) ? S : L"(null)"));
        }
        break;
      case L'S':
        if (Len == lL)
        {
          const wchar_t * S = va_arg(Args, const wchar_t *);
          Out += Pad(UnicodeString((S != nullptr) ? S : L"(null)"));
        }
        else
        {
          const char * S = va_arg(Args, const char *);
          Out += Pad(UnicodeString((S != nullptr) ? S : "(null)"));
        }
        break;
      default:
        throw EConvertError(UnicodeString(L"UnicodeString::sprintf: unsupported conversion"));
    }
  }
  return Out;
}

}  // namespace

UnicodeString & UnicodeString::sprintf(const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  *this = WideVFormat(Format, Args);
  va_end(Args);
  return *this;
}

UnicodeString & UnicodeString::cat_sprintf(const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  *this += WideVFormat(Format, Args);
  va_end(Args);
  return *this;
}
