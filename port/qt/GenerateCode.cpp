// forms/GenerateUrl.cpp's generation, as TGenerateUrlDialog's GenerateUrl,
// GenerateScript, GenerateAssemblyCode and UpdateControls write it, for a
// session and for files (the transfer code, from the Copy dialog, is not
// ported yet). The scripts run linscp's command-line client, where
// upstream's run WinSCP.com.
#include <vcl.h>
#pragma hdrstop

#include "GenerateCode.h"

#include <Common.h>
#include <Interface.h>
#include <TextsWin.h>

#include <algorithm>
#include <vector>

namespace
{

// windows/WinInterface.h's and ProgParams' (the GUI's headers).
const wchar_t * const LogSwitch = L"Log";
const wchar_t * const IniSwitch = L"Ini";

UnicodeString FormatSwitch(const UnicodeString & Switch)
{
  return FORMAT(L"/%s", (Switch));
}

std::string Narrow(const UnicodeString & Str)
{
  UTF8String Utf(Str);
  return std::string(Utf.c_str(), Utf.Length());
}

UnicodeString RtfScriptComment(const UnicodeString & Text)
{
  return RtfColorItalicText(7, Text);
}

UnicodeString RtfScriptPlaceholder(const UnicodeString & Text)
{
  return RtfColorText(7, Text);
}

UnicodeString RtfScriptCommand(const UnicodeString & Command)
{
  return RtfLink(ScriptCommandLink(Command), RtfKeyword(Command));
}

UnicodeString RtfCommandlineSwitch(const UnicodeString & Switch, const UnicodeString & Anchor)
{
  return RtfLink(L"commandline#" + Anchor, RtfParameter(FormatSwitch(Switch.LowerCase())));
}

bool Flag(const CoreBridge::TGenerateCodeOptions & Options, int Tag)
{
  return (Options.Components & Tag) != 0;
}

// TGenerateUrlDialog::GenerateUrl(Path).
UnicodeString GenerateUrl(TSessionData * Data, const CoreBridge::TGenerateCodeOptions & Options,
  const CoreBridge::TGeneratedCode & Can, UnicodeString Path, bool FileUrl)
{
  UnicodeString Url =
    Data->GenerateSessionUrl(
      FLAGMASK(Flag(Options, 16), sufSpecific) |
      FLAGMASK(Can.CanUserName && Flag(Options, 1), sufUserName) |
      FLAGMASK(Can.CanPassword && Flag(Options, 2), sufPassword) |
      FLAGMASK(Can.CanHostKey && Flag(Options, 4), sufHostKey) |
      FLAGMASK(Can.CanRawSettings && Flag(Options, 64), sufRawSettings));

  if ((Can.CanRemoteDirectory && Flag(Options, 8)) || FileUrl)
  {
    if (StartsStr(L"/", Path))
    {
      Path.Delete(1, 1);
    }

    Url += EncodeUrlPath(Path);
  }

  if (Can.CanSaveExtension && Flag(Options, 32))
  {
    Url += UnicodeString(UrlParamSeparator) + UrlSaveParamName;
  }

  return Url;
}

// TGenerateUrlDialog::GenerateScript, without a transfer.
UnicodeString GenerateScript(TSessionData * Data, int ScriptFormat, const UnicodeString & ExeName)
{
  UnicodeString Result;

  UnicodeString BaseExeName = ExtractFileBaseName(ExeName);
  UnicodeString OpenCommand = Data->GenerateOpenCommandArgs(true);
  UnicodeString CommandPlaceholder1 = FMTLOAD(GENERATE_URL_COMMAND, (1));
  UnicodeString CommandPlaceholder2 = FMTLOAD(GENERATE_URL_COMMAND, (2));
  UnicodeString LogPath = LoadStr(GENERATE_URL_WRITABLE_PATH_TO_LOG) + RtfText(BaseExeName + L".log");
  UnicodeString LogParameter =
    RtfCommandlineSwitch(LogSwitch, L"logging") + RtfText(L"=") +
    RtfScriptPlaceholder(L"\"" + LogPath + L"\"");
  UnicodeString IniParameter =
    RtfCommandlineSwitch(IniSwitch, L"configuration") + RtfText(UnicodeString(L"=") + INI_NUL);
  UnicodeString CommandParameter = RtfCommandlineSwitch(COMMAND_SWITCH, L"scripting");

  typedef std::vector<UnicodeString> TCommands;
  TCommands Commands;

  Commands.push_back(RtfScriptCommand(L"open") + L" " + OpenCommand);
  Commands.push_back(UnicodeString());
  Commands.push_back(L"# " + CommandPlaceholder1);
  Commands.push_back(L"# " + CommandPlaceholder2);
  Commands.push_back(UnicodeString());
  Commands.push_back(RtfScriptCommand(L"exit"));

  // Upstream's WinSCP.com: the command-line client, the same here.
  UnicodeString ComExeName = ExeName;

  if (ScriptFormat == 0)
  {
    for (TCommands::const_iterator I = Commands.begin(); I != Commands.end(); I++)
    {
      UnicodeString Command = *I;
      if (!Command.IsEmpty())
      {
        if (Command[1] == L'#')
        {
          Result += RtfScriptComment(Command);
        }
        else
        {
          Result += Command;
        }
      }
      Result += RtfPara;
    }

    UnicodeString ScriptCommandLine =
      FORMAT("\"%s\" /%s=\"%s\" /%s=%s /%s=\"%s\"",
        (ExeName, LowerCase(LogSwitch), LogPath, LowerCase(IniSwitch), INI_NUL, LowerCase(SCRIPT_SWITCH), LoadStr(GENERATE_URL_PATH_TO_SCRIPT)));
    Result +=
      RtfPara +
      RtfScriptComment(L"# " + LoadStr(GENERATE_URL_SCRIPT_DESC)) + RtfPara +
      RtfScriptComment(L"# " + ScriptCommandLine) + RtfPara;
  }
  else if (ScriptFormat == 1)
  {
    Result =
      RtfScriptPlaceholder(L"@echo off") + RtfPara +
      RtfPara +
      RtfText(L"\"" + ComExeName + "\" ^") + RtfPara +
      RtfText(L"  ") + LogParameter + L" " + IniParameter + RtfText(L" ^") + RtfPara +
      RtfText(L"  ") + CommandParameter;

    for (TCommands::const_iterator I = Commands.begin(); I != Commands.end(); I++)
    {
      UnicodeString Command = *I;
      if (!Command.IsEmpty())
      {
        Result +=
          RtfText(L" ^") + RtfPara +
          RtfText(L"    \"");

        if (Command[1] == L'#')
        {
          Command.Delete(1, 1);
          Result += RtfScriptPlaceholder(Command.TrimLeft());
        }
        else
        {
          Result += RtfEscapeParam(ReplaceStr(Command, L"%", L"%%"), false);
        }
        Result += L"\"";
      }
    }

    Result +=
      RtfPara +
      RtfPara +
      RtfKeyword(L"set") + RtfText(L" WINSCP_RESULT=%ERRORLEVEL%") + RtfPara +
      RtfKeyword(L"if") + RtfText(L" %WINSCP_RESULT% ") + RtfKeyword(L"equ") + RtfText(L" 0 (") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"echo") + RtfText(L" Success") + RtfPara +
      RtfText(L") ") + RtfKeyword(L"else") + RtfText(L" (") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"echo") + RtfText(L" Error") + RtfPara +
      RtfText(L")") + RtfPara +
      RtfPara +
      RtfKeyword(L"exit") + RtfText(L" /b %WINSCP_RESULT%") + RtfPara;
  }
  else if (ScriptFormat == 2)
  {
    Result =
      LogParameter + L" " +
      IniParameter + L" " +
      CommandParameter;

    for (TCommands::const_iterator I = Commands.begin(); I != Commands.end(); I++)
    {
      UnicodeString Command = *I;
      if (!Command.IsEmpty())
      {
        Result += RtfText(L" \"");
        if (Command[1] == L'#')
        {
          Command.Delete(1, 1);
          Result += RtfScriptPlaceholder(Command.TrimLeft());
        }
        else
        {
          Result += RtfEscapeParam(Command, false);
        }
        Result += L"\"";
      }
    }
  }
  else if (ScriptFormat == 3)
  {
    // https://stackoverflow.com/q/74440303/850848
    UnicodeString PsArgPassingLink(L"https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_preference_variables#psnativecommandargumentpassing");

    Result =
      RtfLink(PsArgPassingLink, RtfScriptComment(FORMAT(L"# %s", (LoadStr(GENERATE_URL_PS_ARG_PASSING))))) + RtfPara +
      RtfLink(PsArgPassingLink, L"$PSNativeCommandArgumentPassing") + L" = " + AssemblyString(alPowerShell, L"Legacy") + RtfPara +
      RtfPara +
      RtfText(L"& \"" + ComExeName + "\" `") + RtfPara +
      RtfText(L"  ") + LogParameter + L" " + IniParameter + RtfText(L" `") + RtfPara +
      RtfText(L"  ") + CommandParameter;

    for (TCommands::const_iterator I = Commands.begin(); I != Commands.end(); I++)
    {
      UnicodeString Command = *I;
      if (!Command.IsEmpty())
      {
        Result += RtfText(L" `") + RtfPara + RtfText(L"    \"");
        if (Command[1] == L'#')
        {
          Command.Delete(1, 1);
          Result += RtfScriptPlaceholder(Command.TrimLeft());
        }
        else
        {
          Command = ReplaceStr(Command, L"`", L"``");
          Command = ReplaceStr(Command, L"$", L"`$");
          Result += RtfEscapeParam(Command, true);
        }
        Result += L"\"";
      }
    }

    Result +=
      RtfPara +
      RtfPara +
      RtfText(L"$winscpResult = $LastExitCode") + RtfPara +
      RtfKeyword(L"if") + RtfText(L" ($winscpResult -eq 0)") + RtfPara +
      RtfText(L"{") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"Write-Host") + L" " + RtfString(L"\"Success\"") + RtfPara +
      RtfText(L"}") + RtfPara +
      RtfText(L"else") + RtfPara +
      RtfText(L"{") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"Write-Host") + L" " + RtfString(L"\"Error\"") + RtfPara +
      RtfText(L"}") + RtfPara +
      RtfPara +
      RtfKeyword(L"exit") + RtfText(L" $winscpResult") + RtfPara;
  }

  return Result;
}

// TGenerateUrlDialog::GenerateAssemblyCode, without a transfer.
UnicodeString GenerateAssemblyCode(TSessionData * Data, int AssemblyLanguage)
{
  TAssemblyLanguage Language = static_cast<TAssemblyLanguage>(AssemblyLanguage);

  UnicodeString Head;
  UnicodeString Tail;
  int Indent;

  Data->GenerateAssemblyCode(Language, Head, Tail, Indent);

  UnicodeString Result = Head;

  UnicodeString Code = AssemblyCommentLine(Language, LoadStr(GENERATE_URL_YOUR_CODE));

  UnicodeString Indentation = UnicodeString::StringOfChar(L' ', Indent);
  Code = Indentation + ReplaceStr(Code, RtfPara, RtfPara + Indentation);
  if (Code.SubString(Code.Length() - Indentation.Length() + 1, Indentation.Length()) == Indentation)
  {
    Code.SetLength(Code.Length() - Indentation.Length());
  }

  Result += Code;

  Result += Tail;

  return Result;
}

}  // namespace

// TGenerateUrlDialog::UpdateControls' choices and generation.
CoreBridge::TGeneratedCode GenerateSessionCode(TSessionData * Data, const CoreBridge::TGenerateCodeOptions & Options,
  const UnicodeString & ExeName)
{
  using TTarget = CoreBridge::TGenerateCodeOptions::TTarget;
  CoreBridge::TGeneratedCode Result;
  const bool FileUrl = !Options.Paths.empty();
  const TTarget Target = FileUrl ? TTarget::Url : Options.Target;
  Result.Title = Narrow(LoadStr(FileUrl ? GENERATE_URL_FILE_TITLE : GENERATE_URL_SESSION_TITLE));

  bool HostKeyUnknown = Data->UsesSsh && Data->HostKey.IsEmpty();

  Result.CanUserName = !Data->UserNameExpanded.IsEmpty();
  bool UserNameIncluded = Result.CanUserName && Flag(Options, 1);
  Result.CanPassword = UserNameIncluded && Data->HasPassword();
  Result.CanHostKey = UserNameIncluded && !Data->HostKey.IsEmpty();
  Result.CanRemoteDirectory = !Data->RemoteDirectory.IsEmpty() && !FileUrl;
  Result.CanSaveExtension = !FileUrl;
  Result.CanRawSettings = UserNameIncluded && Data->HasRawSettingsForUrl();

  UnicodeString Rtf;
  UnicodeString Description;
  if (Target == TTarget::Url)
  {
    Result.ResultCaption = Narrow(LoadStr(GENERATE_URL_URL));
    if (!FileUrl)
    {
      UnicodeString Path = Data->RemoteDirectory;
      if (!Path.IsEmpty() && !EndsStr(L"/", Path))
      {
        Path += L"/";
      }
      Rtf = RtfText(GenerateUrl(Data, Options, Result, Path, false));
    }
    else
    {
      for (const std::string & Path : Options.Paths)
      {
        Rtf += RtfText(GenerateUrl(Data, Options, Result, UnicodeString(UTF8String(Path.c_str())), true)) + RtfPara;
      }
    }
    Result.WordWrap = true;
    Result.FixedWidth = false;
  }
  else if (Target == TTarget::Script)
  {
    const int ScriptFormat = std::min(std::max(Options.ScriptFormat, 0), 3);
    Result.ResultCaption = Narrow(LoadStr(GENERATE_URL_SCRIPT));
    if (ScriptFormat == 2)
    {
      Result.ResultCaption = Narrow(LoadStr(GENERATE_URL_COMMANDLINE_LABEL));
      Result.WordWrap = true;
      Result.FixedWidth = false;
      const UnicodeString Quoted = FORMAT("\"%s\"", (ExeName));
      Description = FMTLOAD(GENERATE_URL_COMMANDLINE_DESC, (Quoted)) + L"\n";
    }
    else
    {
      Result.WordWrap = false;
      Result.FixedWidth = true;
    }
    if (HostKeyUnknown)
    {
      Description += LoadStr(GENERATE_URL_HOSTKEY_UNKNOWN) + L"\n";
    }
    Rtf = GenerateScript(Data, ScriptFormat, ExeName);
  }
  else
  {
    Result.ResultCaption = Narrow(LoadStr(GENERATE_URL_CODE));
    if (HostKeyUnknown)
    {
      Description += LoadStr(GENERATE_URL_HOSTKEY_UNKNOWN) + L"\n";
    }
    Rtf = GenerateAssemblyCode(Data, std::min(std::max(Options.AssemblyLanguage, 0), 2));
    Result.WordWrap = false;
    Result.FixedWidth = true;
  }
  Result.Rtf = Narrow(Rtf);
  Result.Description = Narrow(Description);
  return Result;
}
