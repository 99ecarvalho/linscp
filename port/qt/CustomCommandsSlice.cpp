// Stand-ins for what the custom commands' slices call in parts of
// upstream's GUI linscp does not have (CustomCommandsSlice.h).
#include <vcl.h>
#pragma hdrstop

#include "CustomCommandsSlice.h"

#include <FileInfo.h>
#include <HierarchicalStorage.h>

#include <memory>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <climits>
#include <cstdio>
#include <cstdlib>
#endif

// What windows/Setup.cpp detects for extensions' "@require" lines: the
// newest .NET Framework, .NET (Core) runtime, Windows PowerShell and
// PowerShell (pwsh) installed, "0" for none, found once. On Windows where
// upstream looks (the registry, the dotnet folders; PowerShell 7 from the
// Store is not looked for); on POSIX .NET's shared runtimes and pwsh
// --version (no .NET Framework nor Windows PowerShell there).
namespace
{

UnicodeString NetVersionStr;
UnicodeString NetCoreVersionStr;
UnicodeString PowerShellVersionStr;
UnicodeString PowerShellCoreVersionStr;

void Newer(UnicodeString & Result, const UnicodeString & Version)
{
  if (!Version.IsEmpty() && (CompareVersion(Version, Result) > 0))
  {
    Result = Version;
  }
}

// The runtimes' folders under DotNet (shared/Microsoft.NETCore.App/<version>).
void NewestRuntime(UnicodeString & Result, const UnicodeString & DotNet)
{
  const UnicodeString Runtimes = IncludeTrailingBackslash(IncludeTrailingBackslash(DotNet) + L"shared") + L"Microsoft.NETCore.App";
  TSearchRecOwned SearchRec;
  if (FindFirstUnchecked(IncludeTrailingBackslash(Runtimes) + L"*", faDirectory, SearchRec) == 0)
  {
    do
    {
      if (SearchRec.IsRealFile() && FLAGSET(SearchRec.Attr, faDirectory))
      {
        Newer(Result, SearchRec.Name);
      }
    }
    while (FindNextChecked(SearchRec) == 0);
  }
}

#if defined(_WIN32)
// The subkeys' values named Name under Key (of HKEY_LOCAL_MACHINE, the
// 64-bit view), and under their SubKey, as upstream reads them.
void NewestInRegistry(UnicodeString & Result, const UnicodeString & Key, const UnicodeString & Name,
  const std::vector<UnicodeString> & SubKeys)
{
  std::unique_ptr<TRegistryStorage> Registry(new TRegistryStorage(Key, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY));
  if (Registry->OpenRootKey(false))
  {
    std::unique_ptr<TStringList> Keys(new TStringList());
    Registry->GetSubKeyNames(Keys.get());
    for (int Index = 0; Index < Keys->Count; Index++)
    {
      for (const UnicodeString & SubKey : SubKeys)
      {
        const UnicodeString Path = SubKey.IsEmpty() ? Keys->Strings[Index] : (Keys->Strings[Index] + L"\\" + SubKey);
        if (Registry->OpenSubKeyPath(Path, false))
        {
          Newer(Result, Registry->ReadString(Name, L""));
          Registry->CloseSubKeyPath();
        }
      }
    }
  }
}
#else
// What a command prints, its first line.
UnicodeString FirstLineOf(const char * Command)
{
  std::string Output;
  if (FILE * Pipe = popen(Command, "r"))
  {
    char Buffer[256];
    if (fgets(Buffer, sizeof(Buffer), Pipe) != nullptr)
    {
      Output = Buffer;
    }
    pclose(Pipe);
  }
  return UnicodeString(UTF8String(Output.c_str())).Trim();
}
#endif

}  // namespace

UnicodeString GetNetVersionStr()
{
  if (NetVersionStr.IsEmpty())
  {
    NetVersionStr = L"0";
#if defined(_WIN32)
    NewestInRegistry(NetVersionStr, L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP", L"Version", { L"", L"Full", L"Client" });
#endif
  }
  return NetVersionStr;
}

UnicodeString GetNetCoreVersionStr()
{
  if (NetCoreVersionStr.IsEmpty())
  {
    NetCoreVersionStr = L"0";
#if defined(_WIN32)
    const UnicodeString ProgramsFolder = DefaultStr(GetEnvironmentVariable(L"ProgramW6432"), GetEnvironmentVariable(L"ProgramFiles"));
    NewestRuntime(NetCoreVersionStr, IncludeTrailingBackslash(ProgramsFolder) + L"dotnet");
    const UnicodeString DotNet = FileSearch(L"dotnet.exe", GetEnvironmentVariable(L"PATH"));
#else
    for (const wchar_t * Folder : { L"/usr/share/dotnet", L"/usr/lib/dotnet", L"/usr/lib64/dotnet", L"/opt/dotnet" })
    {
      NewestRuntime(NetCoreVersionStr, Folder);
    }
    UnicodeString DotNet = FileSearch(L"dotnet", GetEnvironmentVariable(L"PATH"));
    if (!DotNet.IsEmpty())
    {
      // Usually a link into its folder.
      char Resolved[PATH_MAX];
      if (realpath(UTF8String(DotNet).c_str(), Resolved) != nullptr)
      {
        DotNet = UnicodeString(UTF8String(Resolved));
      }
    }
#endif
    if (!DotNet.IsEmpty())
    {
      NewestRuntime(NetCoreVersionStr, ExtractFilePath(DotNet));
    }
  }
  return NetCoreVersionStr;
}

UnicodeString GetPowerShellVersionStr()
{
  if (PowerShellVersionStr.IsEmpty())
  {
    PowerShellVersionStr = L"0";
#if defined(_WIN32)
    NewestInRegistry(PowerShellVersionStr, L"SOFTWARE\\Microsoft\\PowerShell", L"PowerShellVersion", { L"PowerShellEngine" });
#endif
  }
  return PowerShellVersionStr;
}

UnicodeString GetPowerShellCoreVersionStr()
{
  if (PowerShellCoreVersionStr.IsEmpty())
  {
    PowerShellCoreVersionStr = L"0";
#if defined(_WIN32)
    NewestInRegistry(PowerShellCoreVersionStr, L"SOFTWARE\\Microsoft\\PowerShellCore\\InstalledVersions", L"SemanticVersion", { L"" });
#else
    if (!FileSearch(L"pwsh", GetEnvironmentVariable(L"PATH")).IsEmpty())
    {
      // "PowerShell 7.4.1".
      UnicodeString Line = FirstLineOf("pwsh --version 2>/dev/null");
      CutToChar(Line, L' ', true);
      Newer(PowerShellCoreVersionStr, Line.Trim());
    }
#endif
  }
  return PowerShellCoreVersionStr;
}
