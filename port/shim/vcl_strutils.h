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
// Delphi resourcestrings, as C++Builder spells them. Core formats with the
// first and shows the second on a button.
#define System_Sysconst_SOSError (UnicodeString(L"System Error.  Code: %d.") + sLineBreak + UnicodeString(L"%s%s"))
#define Vcl_Consts_SMsgDlgCancel (UnicodeString(L"Cancel"))

// Winapi.Windows' module-name constant.
inline constexpr wchar_t kernel32[] = L"kernel32.dll";
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
// Delphi's one-argument overloads, one per integer width, each giving twice
// the size in hex digits: IntToHex(-1) is FFFFFFFF. No long overloads, as
// Delphi has none -- core casts DWORD to unsigned int for exactly that reason.
UnicodeString IntToHex(signed char Value);
UnicodeString IntToHex(unsigned char Value);
UnicodeString IntToHex(short Value);
UnicodeString IntToHex(unsigned short Value);
UnicodeString IntToHex(int Value);
UnicodeString IntToHex(unsigned int Value);
UnicodeString IntToHex(__int64 Value);
UnicodeString IntToHex(unsigned __int64 Value);
UnicodeString UIntToStr(unsigned __int64 Value);
int StrToInt(const UnicodeString & S);                 // raises EConvertError
int StrToIntDef(const UnicodeString & S, int Default);
bool TryStrToInt(const UnicodeString & S, int & Value);
__int64 StrToInt64(const UnicodeString & S);
__int64 StrToInt64Def(const UnicodeString & S, __int64 Default);
bool TryStrToInt64(const UnicodeString & S, __int64 & Value);
UnicodeString FloatToStr(double Value);
bool TryStrToFloat(const UnicodeString & S, double & Value);
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
// Delphi's Ansi* forms are locale-aware. These fold ASCII only, like LowerCase
// above, so the two platforms agree; see the case-folding divergence.
UnicodeString AnsiLowerCase(const UnicodeString & S);
UnicodeString AnsiUpperCase(const UnicodeString & S);
// Case-sensitive, as Delphi's AnsiReplaceStr is.
UnicodeString AnsiReplaceStr(const UnicodeString & AText, const UnicodeString & AFromText,
                             const UnicodeString & AToText);
// Delphi's: S[Index] is one of Delimiters; false for an index outside S.
bool IsDelimiter(const UnicodeString & Delimiters, const UnicodeString & S, int Index);
// The index of the first character of S, from StartIdx, that is one of
// Delimiters; 0 when there is none.
int FindDelimiter(const UnicodeString & Delimiters, const UnicodeString & S, int StartIdx = 1);
// Case-insensitive, as Delphi's -- ASCII folding, as everywhere in the shim.
int AnsiCompareText(const UnicodeString & S1, const UnicodeString & S2);
int StrLIComp(const wchar_t * Str1, const wchar_t * Str2, unsigned MaxLen);
// Raises EConvertError, as Delphi's does.
double StrToFloat(const UnicodeString & S);

// --- System.Math --------------------------------------------------------------
// Delphi's overload set, one per numeric type.
inline int Min(int A, int B) { return (A < B) ? A : B; }
inline unsigned Min(unsigned A, unsigned B) { return (A < B) ? A : B; }
inline __int64 Min(__int64 A, __int64 B) { return (A < B) ? A : B; }
inline unsigned __int64 Min(unsigned __int64 A, unsigned __int64 B) { return (A < B) ? A : B; }
inline double Min(double A, double B) { return (A < B) ? A : B; }
inline int Max(int A, int B) { return (A > B) ? A : B; }
inline unsigned Max(unsigned A, unsigned B) { return (A > B) ? A : B; }
inline __int64 Max(__int64 A, __int64 B) { return (A > B) ? A : B; }
inline unsigned __int64 Max(unsigned __int64 A, unsigned __int64 B) { return (A > B) ? A : B; }
inline double Max(double A, double B) { return (A > B) ? A : B; }
// -1, 0 or 1.
inline int CompareValue(int A, int B) { return (A < B) ? -1 : (A > B) ? 1 : 0; }
inline int CompareValue(__int64 A, __int64 B) { return (A < B) ? -1 : (A > B) ? 1 : 0; }
inline int CompareValue(double A, double B) { return (A < B) ? -1 : (A > B) ? 1 : 0; }

// Borland <stdlib.h>'s random(num): 0 .. num-1, over rand(). Not Delphi's
// Random; core's Security.cpp calls this one.
int random(int num);
// StrUtils' open-array lookups: the index of AText among AValues[0..High],
// or -1. IndexStr is case-sensitive, IndexText not.
int IndexStr(const UnicodeString & AText, const UnicodeString * AValues, int AValues_High);
int IndexText(const UnicodeString & AText, const UnicodeString * AValues, int AValues_High);
UnicodeString ReplaceStr(const UnicodeString & S, const UnicodeString & From,
                         const UnicodeString & To);
UnicodeString ReplaceText(const UnicodeString & S, const UnicodeString & From,
                          const UnicodeString & To);
UnicodeString StringOfChar(wchar_t C, int Count);
UnicodeString DupeString(const UnicodeString & S, int Count);

// --- encoding --------------------------------------------------------------
UnicodeString UTF8ToString(const UTF8String & S);
UnicodeString UTF8ToString(const RawByteString & S);
// Exact for the char * PuTTY hands back; otherwise ambiguous between the two.
UnicodeString UTF8ToString(const char * S);
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

// AnsiSameText is Delphi's byte-wise case-insensitive compare. It is not a
// synonym for SameText: Delphi's folds only the ANSI range, which is what the
// shim's ASCII folding already does, so the two coincide here.
bool AnsiSameText(const UnicodeString & A, const UnicodeString & B);
bool AnsiSameStr(const UnicodeString & A, const UnicodeString & B);

// VCL globals describing the host. Read by core when reporting the
// environment; on POSIX they describe the kernel instead, which is the nearest
// honest answer.
//
// Functions, as core calls them -- Win32MajorVersion() -- and computed on
// first use, which also removes the static-initialisation-order hazard a
// global filled by a dynamic initialiser carries. On Windows from
// RtlGetVersion, which reports the true version whether or not the executable
// carries a compatibility manifest; GetVersionEx reports 6.2 without one,
// which would make core's IsWin10() false on Windows 10 and 11.
unsigned Win32MajorVersion();
unsigned Win32MinorVersion();
unsigned Win32BuildNumber();
UnicodeString Win32CSDVersion();
// Delphi's: the running OS is at least AMajor.AMinor.
bool CheckWin32Version(int AMajor, int AMinor = 0);

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
