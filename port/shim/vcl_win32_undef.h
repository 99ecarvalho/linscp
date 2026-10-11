//---------------------------------------------------------------------------
// Undo Win32's A/W name macros where they collide with Delphi RTL names.
//
// <windows.h> defines e.g. #define DeleteFile DeleteFileW. Several of those
// names are also RTL function names the shim declares, and the result is
// genuinely nasty: a header that has not yet seen <windows.h> declares
// DeleteFile, while a .cpp that has seen it *defines* DeleteFileW. Both compile,
// and the mismatch only appears at link time -- and only once something calls
// the function, so it can stay hidden for a long while.
//
// Include this immediately after <windows.h>, everywhere. The shim always calls
// the explicit W variants internally. Where core calls the API itself under the
// bare name, a forwarding function replaces the macro (see the end).
//
// test_strutils.cpp references every one of these so the linker checks them.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_WIN32_UNDEF_H
#define LINSCP_VCL_WIN32_UNDEF_H

#if defined(_WIN32)
  #undef DeleteFile
  #undef CopyFile
  #undef MoveFile
  #undef CreateDirectory
  #undef RemoveDirectory
  #undef GetCurrentDirectory
  #undef SetCurrentDirectory
  #undef ReplaceText
  #undef FindFirstFile
  #undef FindNextFile
  #undef GetFullPathName
  #undef GetFileAttributes
  #undef SetFileAttributes
  #undef GetTempPath
  #undef GetTempFileName
  #undef LoadString
  #undef FormatMessage
  #undef CompareString
  #undef GetUserName
  #undef GetComputerName
  #undef GetModuleFileName
  #undef GetEnvironmentVariable
  #undef SetEnvironmentVariable
  #undef ExpandEnvironmentStrings
  #undef CreateFile
  #undef GetVersionEx
  #undef OutputDebugString
  // GDI's, reached through the shell headers; collides with TStrings::GetObject.
  #undef GetObject

  // mingw's _mingw.h defines __int64 as `long long`; clang's keyword is the
  // same type, and unlike the macro allows __int64(x). _mingw.h is guarded,
  // so a later <windows.h> does not bring the macro back.
  #undef __int64

  // Core also calls some of these as the Win32 API, under the bare name. For
  // those the macro is replaced by a real function forwarding to the W
  // variant: what the macro gave core, without renaming anything else. Exact
  // signatures rather than a forwarding template, so that core's NULL
  // arguments still convert to pointers. Undefining these without a
  // replacement left core's calls with nothing to bind to.
  inline HANDLE CreateFile(LPCWSTR FileName, DWORD Access, DWORD Share,
                           LPSECURITY_ATTRIBUTES Security, DWORD Disposition,
                           DWORD Flags, HANDLE Template)
    { return CreateFileW(FileName, Access, Share, Security, Disposition, Flags, Template); }
  inline DWORD GetFileAttributes(LPCWSTR FileName)
    { return GetFileAttributesW(FileName); }
  inline BOOL SetFileAttributes(LPCWSTR FileName, DWORD Attributes)
    { return SetFileAttributesW(FileName, Attributes); }
  inline DWORD GetTempPath(DWORD BufferLength, LPWSTR Buffer)
    { return GetTempPathW(BufferLength, Buffer); }
  inline DWORD ExpandEnvironmentStrings(LPCWSTR Source, LPWSTR Destination, DWORD Size)
    { return ExpandEnvironmentStringsW(Source, Destination, Size); }
#endif

#endif // LINSCP_VCL_WIN32_UNDEF_H
