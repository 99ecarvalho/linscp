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
  #include <wctype.h>
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

  // ...and those MFC and FileZilla call. DeleteFile(LPCWSTR) overloads the
  // VCL DeleteFile(UnicodeString), as both exist side by side in C++Builder.
  inline BOOL CreateDirectory(LPCWSTR PathName, LPSECURITY_ATTRIBUTES Security)
    { return CreateDirectoryW(PathName, Security); }
  inline BOOL DeleteFile(LPCWSTR FileName)
    { return DeleteFileW(FileName); }
  inline BOOL MoveFile(LPCWSTR Existing, LPCWSTR New)
    { return MoveFileW(Existing, New); }
  inline HANDLE FindFirstFile(LPCWSTR FileName, LPWIN32_FIND_DATAW Data)
    { return FindFirstFileW(FileName, Data); }
  inline BOOL FindNextFile(HANDLE Find, LPWIN32_FIND_DATAW Data)
    { return FindNextFileW(Find, Data); }
  inline DWORD GetFullPathName(LPCWSTR FileName, DWORD Length, LPWSTR Buffer, LPWSTR * FilePart)
    { return GetFullPathNameW(FileName, Length, Buffer, FilePart); }
  inline DWORD GetModuleFileName(HMODULE Module, LPWSTR FileName, DWORD Size)
    { return GetModuleFileNameW(Module, FileName, Size); }
  inline DWORD FormatMessage(DWORD Flags, LPCVOID Source, DWORD MessageId, DWORD LanguageId,
                             LPWSTR Buffer, DWORD Size, va_list * Arguments)
    { return FormatMessageW(Flags, Source, MessageId, LanguageId, Buffer, Size, Arguments); }

  // C++Builder's towupper and towlower return wchar_t; mingw-w64's return
  // wint_t (unsigned short). Core turns the result into a string --
  // SessionInfo.cpp logs a file's type as XmlAttributeEscape(towupper(Type))
  // -- and from a wint_t that is the number, "68" for 'D'. The names are
  // redirected to single functions returning wchar_t, as the POSIX layer
  // does, rather than overloaded, since FileZilla passes towupper to
  // std::transform. <wctype.h> is included above, so its own declarations
  // keep their names.
  inline wchar_t linscp_TowUpper(wchar_t C) { return static_cast<wchar_t>(::towupper(static_cast<wint_t>(C))); } // ADR 0001: Windows CRT
  inline wchar_t linscp_TowLower(wchar_t C) { return static_cast<wchar_t>(::towlower(static_cast<wint_t>(C))); } // ADR 0001: Windows CRT
  #define towupper linscp_TowUpper
  #define towlower linscp_TowLower

  // Deliberately not a forwarder: the shim's LoadString, which serves the
  // string table generated from upstream's .rc files. MFC's CString::LoadString
  // calls ::LoadString, and this build compiles no .res, so FileZilla's
  // strings come from that table on both targets. Declared here as well as in
  // vcl_resources.h so MFC units, which see only this header, reach it.
  int LoadString(void * Module, unsigned Ident, wchar_t * Buffer, int BufferMax);
#endif

#endif // LINSCP_VCL_WIN32_UNDEF_H
