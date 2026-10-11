#include "vcl_strutils.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "vcl_object.h"
#include "vcl_sysutils.h"

#if defined(_WIN32)
  #include <windows.h>

  #include "vcl_win32_undef.h"
  const wchar_t PathDelim = L'\\';
  const UnicodeString sLineBreak(L"\r\n");
#else
  #include <sys/stat.h>
  #include <unistd.h>
  const wchar_t PathDelim = L'/';
  const UnicodeString sLineBreak(L"\n");
#endif

namespace {

// Both separators are recognised when parsing, whatever the host, so a path
// that arrived from a remote server is split the same way on either platform.
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

double StrToFloatDef(const UnicodeString & S, double Default)
{
  const std::string A = Narrow(Trim(S));
  if (A.empty())
  {
    return Default;
  }
  errno = 0;
  char * End = nullptr;
  const double V = std::strtod(A.c_str(), &End);
  if (errno == ERANGE || End == nullptr || *End != '\0')
  {
    return Default;
  }
  return V;
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
  const unsigned long Code = GetLastError();
#else
  const unsigned long Code = static_cast<unsigned long>(errno);
#endif
  throw EOSError(UnicodeString(L"System error, code ") +
                 IntToStr(static_cast<__int64>(Code)));
}
