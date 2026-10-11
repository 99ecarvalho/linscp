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
// the explicit W variants internally, so nothing here loses functionality.
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
#endif

#endif // LINSCP_VCL_WIN32_UNDEF_H
