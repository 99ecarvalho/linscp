//---------------------------------------------------------------------------
// Free string, number and path routines from System.SysUtils / System.StrUtils.
//
// Scoped from measured use across core rather than transcribed wholesale:
// LoadStr 288, SameText 113, Pos 89, CutToChar 70, IntToStr 58, Trim 47,
// IncludeTrailingBackslash 39, ContainsText 37, ExtractFileName 29.
//
// Index conventions follow Delphi, which means 1-based positions and 0 for
// "not found" -- the same as UnicodeString's own methods.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_STRUTILS_H
#define LINSCP_VCL_STRUTILS_H

#include "vcl_bytestring.h"
#include "vcl_object.h"
#include "vcl_strings.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Delphi constants core references directly.
extern const UnicodeString sLineBreak;
constexpr bool True = true;
constexpr bool False = false;
constexpr int MSecsPerSec = 1000;
constexpr int SecsPerMin = 60;
constexpr int MinsPerHour = 60;
constexpr int HoursPerDay = 24;
constexpr int SecsPerDay = SecsPerMin * MinsPerHour * HoursPerDay;
constexpr int MSecsPerDay = SecsPerDay * MSecsPerSec;
constexpr int MSecsPerDayConst = MSecsPerDay;

// --- numbers ---------------------------------------------------------------
UnicodeString IntToStr(__int64 Value);
UnicodeString IntToHex(__int64 Value, int Digits);
UnicodeString UIntToStr(unsigned __int64 Value);
int StrToInt(const UnicodeString & S);                 // raises EConvertError
int StrToIntDef(const UnicodeString & S, int Default);
bool TryStrToInt(const UnicodeString & S, int & Value);
__int64 StrToInt64(const UnicodeString & S);
__int64 StrToInt64Def(const UnicodeString & S, __int64 Default);
bool TryStrToInt64(const UnicodeString & S, __int64 & Value);
UnicodeString FloatToStr(double Value);
double StrToFloatDef(const UnicodeString & S, double Default);

// --- comparison ------------------------------------------------------------
// *Text variants are case-insensitive, *Str variants are case-sensitive.
// This is Delphi's convention and core relies on the distinction.
bool SameText(const UnicodeString & A, const UnicodeString & B);
bool SameStr(const UnicodeString & A, const UnicodeString & B);
int CompareText(const UnicodeString & A, const UnicodeString & B);
int CompareStr(const UnicodeString & A, const UnicodeString & B);

// --- searching (1-based; 0 means not found) --------------------------------
int Pos(const UnicodeString & Substr, const UnicodeString & S);
int PosEx(const UnicodeString & Substr, const UnicodeString & S, int Offset = 1);
int LastDelimiter(const UnicodeString & Delimiters, const UnicodeString & S);

bool StartsStr(const UnicodeString & Prefix, const UnicodeString & S);
bool StartsText(const UnicodeString & Prefix, const UnicodeString & S);
bool EndsStr(const UnicodeString & Suffix, const UnicodeString & S);
bool EndsText(const UnicodeString & Suffix, const UnicodeString & S);
bool ContainsStr(const UnicodeString & S, const UnicodeString & Substr);
bool ContainsText(const UnicodeString & S, const UnicodeString & Substr);

// --- transformation --------------------------------------------------------
UnicodeString Trim(const UnicodeString & S);
UnicodeString TrimLeft(const UnicodeString & S);
UnicodeString TrimRight(const UnicodeString & S);
UnicodeString LowerCase(const UnicodeString & S);
UnicodeString UpperCase(const UnicodeString & S);
UnicodeString ReplaceStr(const UnicodeString & S, const UnicodeString & From,
                         const UnicodeString & To);
UnicodeString ReplaceText(const UnicodeString & S, const UnicodeString & From,
                          const UnicodeString & To);
UnicodeString StringOfChar(wchar_t C, int Count);
UnicodeString DupeString(const UnicodeString & S, int Count);

// --- encoding --------------------------------------------------------------
UnicodeString UTF8ToString(const UTF8String & S);
UnicodeString UTF8ToString(const RawByteString & S);
UTF8String StringToUTF8(const UnicodeString & S);

// --- paths -----------------------------------------------------------------
// Separator handling is platform-aware: '\\' on Windows, '/' elsewhere, with
// both accepted as separators when parsing so that paths from a remote server
// are handled the same way on either host.
extern const wchar_t PathDelim;

UnicodeString IncludeTrailingBackslash(const UnicodeString & S);
UnicodeString ExcludeTrailingBackslash(const UnicodeString & S);
UnicodeString IncludeTrailingPathDelimiter(const UnicodeString & S);
UnicodeString ExcludeTrailingPathDelimiter(const UnicodeString & S);
UnicodeString ExtractFileName(const UnicodeString & FileName);
UnicodeString ExtractFilePath(const UnicodeString & FileName);
UnicodeString ExtractFileDir(const UnicodeString & FileName);
UnicodeString ExtractFileExt(const UnicodeString & FileName);
UnicodeString ExtractFileDrive(const UnicodeString & FileName);
UnicodeString ChangeFileExt(const UnicodeString & FileName,
                            const UnicodeString & Extension);
bool IsPathDelimiter(const UnicodeString & S, int Index);

// --- filesystem actions ----------------------------------------------------
UnicodeString GetCurrentDir();
bool SetCurrentDir(const UnicodeString & Dir);
bool CreateDir(const UnicodeString & Dir);
bool RemoveDir(const UnicodeString & Dir);
bool DeleteFile(const UnicodeString & FileName);
bool RenameFile(const UnicodeString & OldName, const UnicodeString & NewName);
UnicodeString ExpandUNCFileName(const UnicodeString & FileName);

// --- environment and version ----------------------------------------------
UnicodeString GetEnvironmentVariable(const UnicodeString & Name);
bool SetEnvironmentVariable(const UnicodeString & Name,
                            const UnicodeString & Value);
UnicodeString ExpandFileName(const UnicodeString & FileName);
UnicodeString ExpandEnvironmentVariables(const UnicodeString & S);

// AnsiSameText is Delphi's byte-wise case-insensitive compare. It is not a
// synonym for SameText: Delphi's folds only the ANSI range, which is what the
// shim's ASCII folding already does, so the two coincide here.
bool AnsiSameText(const UnicodeString & A, const UnicodeString & B);
bool AnsiSameStr(const UnicodeString & A, const UnicodeString & B);

// VCL globals describing the host. Read by core when reporting the
// environment; on POSIX they describe the kernel instead, which is the nearest
// honest answer.
extern unsigned Win32MajorVersion;
extern unsigned Win32MinorVersion;
extern unsigned Win32BuildNumber;
extern UnicodeString Win32CSDVersion;

// --- control flow ----------------------------------------------------------
// Delphi's Abort raises the silent EAbort exception.
[[noreturn]] void Abort();
[[noreturn]] void RaiseLastOSError();
// Delphi's overloads, with its message: "System Error.  Code: N." then the
// system's text. A zero code raises "A call to an OS function failed" instead.
// On POSIX the no-argument form reads the emulated Win32 last error, since
// core calls it after Win32-style calls; errno only when that is unset.
[[noreturn]] void RaiseLastOSError(int LastError);
[[noreturn]] void RaiseLastOSError(int LastError, const UnicodeString & AdditionalInfo);
// The system's text for an error code, with trailing whitespace and periods
// trimmed as Delphi trims them. On POSIX, Windows' English text for the codes
// the POSIX layer can produce, so both platforms report alike.
UnicodeString SysErrorMessage(int ErrorCode);

// --- System.Random --------------------------------------------------------
// Delphi's own linear congruential generator, so a given RandSeed yields
// Delphi's sequence. Not thread-safe, as Delphi's is not. RandSeed starts at 0;
// core calls Randomize at startup.
extern int RandSeed;
void Randomize();
int Random(int Range);       // 0 .. Range-1
double Random();             // [0, 1)

//---------------------------------------------------------------------------
// Core also spells RTL names with their Delphi unit scope.
//---------------------------------------------------------------------------
namespace Sysutils {
using ::Exception;
using ::RenameFile;
using ::DeleteFile;
using ::Trim;
}  // namespace Sysutils

namespace Classes {
using ::TObject;
using ::TPersistent;
using ::TStrings;
using ::TStringList;
}  // namespace Classes

// StrUtils' substring helpers, over SubString so they share Copy's clamping.
UnicodeString LeftStr(const UnicodeString & AText, int ACount);
UnicodeString RightStr(const UnicodeString & AText, int ACount);
UnicodeString MidStr(const UnicodeString & AText, int AStart, int ACount);

#endif // LINSCP_VCL_STRUTILS_H
