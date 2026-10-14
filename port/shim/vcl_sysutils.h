//---------------------------------------------------------------------------
// System.SysUtils: file attributes, directory search, and odds and ends.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_SYSUTILS_H
#define LINSCP_VCL_SYSUTILS_H

#include <vector>

// TSearchRec carries Win32's WIN32_FIND_DATAW, as Delphi's does on Windows.
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
#endif

#include "vcl_datetime.h"
#include "vcl_dynamicarray.h"
#include "vcl_streams.h"
#include "vcl_object.h"
#include "vcl_strings.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Delphi's file-attribute bits. These match the Win32 FILE_ATTRIBUTE_* values
// because Delphi defines them that way and core compares against both.
enum : int
{
  faReadOnly  = 0x00000001,
  faHidden    = 0x00000002,
  faSysFile   = 0x00000004,
  faVolumeID  = 0x00000008,
  faDirectory = 0x00000010,
  faArchive   = 0x00000020,
  faNormal    = 0x00000080,
  faTemporary = 0x00000100,
  faSymLink   = 0x00000400,
  faCompressed = 0x00000800,
  faEncrypted = 0x00004000,
  faAnyFile   = 0x000001FF,
};

using TFileName = UnicodeString;

// Delphi's empty-string constant. Used as a default argument in core, so it
// must be a compile-time-usable value rather than a function call.
extern const UnicodeString EmptyStr;

//---------------------------------------------------------------------------
// TSearchRec. Core derives TSearchRecSmart and TSearchRecChecked from this, so
// it must be an ordinary inheritable struct, and the field names must match
// Delphi's because core reads them directly.
//---------------------------------------------------------------------------
struct TSearchRec
{
  int Time = 0;                 // legacy DOS timestamp; kept for layout parity
  __int64 Size = 0;
  int Attr = 0;
  TFileName Name;
  int ExcludeAttr = 0;
  void * FindHandle = nullptr;
  TDateTime TimeStamp;

  // Opaque platform state for the iteration in progress.
  void * FindState = nullptr;

  // Delphi's Windows TSearchRec field; core reads ftLastWriteTime from it. On
  // POSIX it is filled from lstat: attributes as Win32 bits, times as FILETIME
  // (UTC). POSIX has no creation time, so ftCreationTime is the modification
  // time.
  WIN32_FIND_DATAW FindData{};
};

// Delphi's convention: 0 on success, a non-zero error code when there is
// nothing (more) to find. Callers in core test for == 0.
int FindFirst(const UnicodeString & Path, int Attr, TSearchRec & F);
int FindNext(TSearchRec & F);
void FindClose(TSearchRec & F);

//---------------------------------------------------------------------------
// Delphi's handle-based file API. Core uses these in FileBuffer.cpp, including
// the array overloads that take an offset into a dynamic array.
//---------------------------------------------------------------------------
THandle FileOpen(const UnicodeString & FileName, unsigned Mode);
THandle FileCreate(const UnicodeString & FileName);
void FileClose(THandle Handle);
__int64 FileSeek(THandle Handle, __int64 Offset, int Origin);

int FileRead(THandle Handle, void * Buffer, int Count);
int FileWrite(THandle Handle, const void * Buffer, int Count);
int FileRead(THandle Handle, TBytes Buffer, int Offset, int Count);
int FileWrite(THandle Handle, const TBytes Buffer, int Offset, int Count);

bool FileExists(const UnicodeString & FileName);
bool DirectoryExists(const UnicodeString & Name);
// Delphi's FileSearch: Name as given, else in each directory of DirList,
// separated by the platform's PathSep (';' on Windows, ':' on POSIX); the
// first that exists, or empty.
UnicodeString FileSearch(const UnicodeString & Name, const UnicodeString & DirList);
// Delphi's GetFileVersion: the file's VERSIONINFO file version, its major
// and minor in the high and low word; (unsigned)-1 without one.
unsigned int GetFileVersion(const UnicodeString & FileName);

// Delphi's FileSetAttr: 0 on success, otherwise the error code. On POSIX only
// faReadOnly has a meaning -- it clears the write permission bits, and its
// absence restores the owner's, mirroring how FileGetAttr reports it; archive,
// hidden and system have no POSIX counterpart and are ignored.
int FileSetAttr(const UnicodeString & FileName, int Attr, bool FollowLink = true);
// Delphi's FileGetAttr: the fa* attributes, or -1 (faInvalid) on failure. On
// POSIX the same mapping FindFirst reports, following links when asked.
int FileGetAttr(const UnicodeString & FileName, bool FollowLink = true);

// Delphi's, with its sign rule for dates before 1899-12-30, which every
// SYSTEMTIME from 1601 to 1899 needs.
TDateTime SystemTimeToDateTime(const SYSTEMTIME & SystemTime);

// True for a symbolic link, with TargetName its target. Resolves the whole
// chain to an absolute path (GetFinalPathNameByHandle / realpath), where
// Delphi returns the immediate target; core reads only the target's
// attributes. False for anything else, or a dangling link.
bool FileGetSymLinkTarget(const UnicodeString & FileName, UnicodeString & TargetName);

// The 8.3 form on Windows, empty on failure as Delphi's is. POSIX has no short
// names, so the name comes back unchanged.
UnicodeString ExtractShortPathName(const UnicodeString & FileName);

// Delphi's: the file's last-write time, local. False when it does not exist.
bool FileAge(const UnicodeString & FileName, TDateTime & FileDateTime, bool FollowLink = true);

// DateUtils' ISO8601ToDate: YYYY-MM-DD[THH:MM[:SS[.fff]]][Z|+HH:MM|-HH:MM].
// The result is UTC, or local when AReturnUTC is false. A time without a zone
// designator is taken as UTC; see the divergence table. Raises EConvertError.
TDateTime ISO8601ToDate(const UnicodeString & AISODate, bool AReturnUTC = true);

// System's CmdLine: the whole command line. Windows' own string there; on
// POSIX the arguments joined by spaces, quoted where they contain a space,
// tab or quote, with quotes doubled -- as core's CutToken reads them. A
// variable, as in Delphi -- core also has a parameter of that name, which a
// macro would break.
extern wchar_t * CmdLine;

// Delphi's ForceDirectories: creates every missing component. Raises
// EInOutError on an empty name, as Delphi does.
bool ForceDirectories(UnicodeString Dir);

//---------------------------------------------------------------------------
// Core spells some RTL names with their Delphi unit scope.
//---------------------------------------------------------------------------
namespace System {
namespace Types {

using TDuplicates = ::TDuplicates;
constexpr TDuplicates dupIgnore = ::dupIgnore;
constexpr TDuplicates dupAccept = ::dupAccept;
constexpr TDuplicates dupError  = ::dupError;

}  // namespace Types
}  // namespace System

// Core also writes these unqualified by System.
namespace Types = System::Types;

// System.ParamStr / ParamCount. Index 0 is the full path of the running
// executable, as in Delphi; core uses it to find a portable INI beside the
// program. Arguments come from the OS rather than from main(), so nothing has
// to be initialised: GetCommandLineW on Windows, /proc/self/cmdline on Linux.
// An index past the end yields an empty string.
int ParamCount();
UnicodeString ParamStr(int Index);

namespace linscp {

// Delphi's Windows command-line splitting (System.GetParamStr), which is not
// the MSVC CRT's: any character up to ' ' separates, '"' toggles quoting and
// is dropped, there are no backslash escapes, and an empty "" before a token
// is skipped. Element 0 is the program token. Splitting stops at the first
// empty token, as ParamCount does.
std::vector<UnicodeString> SplitDelphiCommandLine(const UnicodeString & CommandLine);

}  // namespace linscp

#endif // LINSCP_VCL_SYSUTILS_H
