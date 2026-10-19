// forms/GenerateUrl.cpp's generation, as TGenerateUrlDialog's GenerateUrl,
// GenerateScript, GenerateAssemblyCode and UpdateControls write it, for a
// session, for files, and for the Copy dialog's transfer. The scripts run
// linscp's command-line client, where upstream's run WinSCP.com.
#include <vcl.h>
#pragma hdrstop

#include "GenerateCode.h"

#include <Common.h>
#include <Interface.h>
#include <TextsWin.h>
#include <RemoteFiles.h>

#include <algorithm>
#include <vector>

namespace
{

// windows/WinInterface.h's and ProgParams' (the GUI's headers).
const wchar_t * const LogSwitch = L"Log";
const wchar_t * const IniSwitch = L"Ini";

const UnicodeString AllFilesMask(L"*");
const UnicodeString NoOpOperationMask(L"*");

// Upstream's: a drive's root keeps its backslash (and POSIX's root, its
// slash).
UnicodeString ExcludeTrailingBackslashUnlessRoot(const UnicodeString & Path)
{
  UnicodeString Result = ExcludeTrailingBackslash(Path);
  if (Result.IsEmpty() || SameText(Result, ExtractFileDrive(Result)))
  {
    Result = IncludeTrailingBackslash(Path);
  }
  return Result;
}

UnicodeString QuoteStringParam(UnicodeString S)
{
  return AddQuotes(RtfEscapeParam(S, false));
}

// Keep in sync with .NET Session.EscapeFileMask (as upstream's).
UnicodeString EscapeFileMask(UnicodeString S)
{
  return
    ReplaceStr(ReplaceStr(ReplaceStr(ReplaceStr(ReplaceStr(
      S, L"[", L"[[]"), L"*", L"[*]"), L"?", L"[?]"), L">", L">>"), L"<", L"<<");
}

void AddSampleDescription(const TGenerateTransfer * Transfer, UnicodeString & Description)
{
  if ((Transfer != nullptr) && Transfer->PathsSample)
  {
    Description += LoadStr(GENERATE_URL_FILE_SAMPLE) + L"\n";
  }
}

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

// TGenerateUrlDialog::GenerateScript.
UnicodeString GenerateScript(TSessionData * Data, int ScriptFormat, const UnicodeString & ExeName,
  const TGenerateTransfer * Transfer, UnicodeString & ScriptDescription)
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

  if (Transfer != nullptr)
  {
    UnicodeString TransferCommand;

    if (Transfer->ToRemote)
    {
      Commands.push_back(RtfScriptCommand(L"lcd") + L" " + RtfText(QuoteStringParam(Transfer->SourcePath)));
      Commands.push_back(RtfScriptCommand(L"cd") + L" " + RtfText(QuoteStringParam(UnixExcludeTrailingBackslash(Transfer->Path))));
      TransferCommand = L"put";
    }
    else
    {
      Commands.push_back(RtfScriptCommand(L"cd") + L" " + RtfText(QuoteStringParam(Transfer->SourcePath)));
      Commands.push_back(RtfScriptCommand(L"lcd") + L" " + RtfText(QuoteStringParam(ExcludeTrailingBackslashUnlessRoot(Transfer->Path))));
      TransferCommand = L"get";
    }

    Commands.push_back(UnicodeString());

    UnicodeString TransferCommandLink = ScriptCommandLink(TransferCommand);
    UnicodeString TransferCommandArgs;
    if (Transfer->Move)
    {
      TransferCommandArgs += RtfSwitch(DELETE_SWITCH, TransferCommandLink);
    }
    TransferCommandArgs += Transfer->CopyParam.GenerateTransferCommandArgs(Transfer->CopyParamAttrs, TransferCommandLink);

    AddSampleDescription(Transfer, ScriptDescription);

    TransferCommand = RtfScriptCommand(TransferCommand) + TransferCommandArgs;

    if (!Transfer->AllFiles)
    {
      for (const UnicodeString & Name : Transfer->Paths)
      {
        UnicodeString Path = ExtractFileName(Name, !Transfer->ToRemote);
        if (!Transfer->ToRemote)
        {
          Path = EscapeFileMask(Path);
        }
        Commands.push_back(TransferCommand + L" " + RtfText(QuoteStringParam(Path)));
      }
    }
    else
    {
      Commands.push_back(TransferCommand + L" " + RtfText(AllFilesMask));
    }
  }
  else
  {
    Commands.push_back(L"# " + CommandPlaceholder1);
    Commands.push_back(L"# " + CommandPlaceholder2);
  }

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
  else if (ScriptFormat == 4)
  {
    // linscp's: the batch file's counterpart for a POSIX shell, the
    // commands single-quoted (a quote in one as '\''), so the shell expands
    // nothing in them.
    Result =
      RtfScriptPlaceholder(L"#!/bin/sh") + RtfPara +
      RtfPara +
      RtfText(L"\"" + ComExeName + "\" \\") + RtfPara +
      RtfText(L"  ") + LogParameter + L" " + IniParameter + RtfText(L" \\") + RtfPara +
      RtfText(L"  ") + CommandParameter;

    for (TCommands::const_iterator I = Commands.begin(); I != Commands.end(); I++)
    {
      UnicodeString Command = *I;
      if (!Command.IsEmpty())
      {
        Result += RtfText(L" \\") + RtfPara + RtfText(L"    '");
        if (Command[1] == L'#')
        {
          Command.Delete(1, 1);
          Result += RtfScriptPlaceholder(Command.TrimLeft());
        }
        else
        {
          Result += ReplaceStr(Command, L"'", L"'\\\\''");
        }
        Result += L"'";
      }
    }

    Result +=
      RtfPara +
      RtfPara +
      RtfText(L"winscp_result=$?") + RtfPara +
      RtfKeyword(L"if") + RtfText(L" [ $winscp_result -eq 0 ]") + RtfPara +
      RtfKeyword(L"then") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"echo") + RtfText(L" Success") + RtfPara +
      RtfKeyword(L"else") + RtfPara +
      RtfText(L"  ") + RtfKeyword(L"echo") + RtfText(L" Error") + RtfPara +
      RtfKeyword(L"fi") + RtfPara +
      RtfPara +
      RtfKeyword(L"exit") + RtfText(L" $winscp_result") + RtfPara;
  }

  return Result;
}

// TGenerateUrlDialog::GenerateAssemblyCode.
UnicodeString GenerateAssemblyCode(TSessionData * Data, int AssemblyLanguage, const TGenerateTransfer * Transfer,
  UnicodeString & AssemblyDescription)
{
  TAssemblyLanguage Language = static_cast<TAssemblyLanguage>(AssemblyLanguage);

  UnicodeString Head;
  UnicodeString Tail;
  int Indent;

  Data->GenerateAssemblyCode(Language, Head, Tail, Indent);

  UnicodeString Result = Head;

  UnicodeString Code;
  if (Transfer != nullptr)
  {
    UnicodeString CopyParamProperties = Transfer->CopyParam.GenerateAssemblyCode(Language, Transfer->CopyParamAttrs);

    bool HasTransferOptions = !CopyParamProperties.IsEmpty();
    if (HasTransferOptions)
    {
      Code +=
        AssemblyCommentLine(Language, LoadStr(GENERATE_URL_COPY_PARAM)) +
        CopyParamProperties +
        RtfPara;
    }

    Code += AssemblyCommentLine(Language, LoadStr(GENERATE_URL_TRANSFER_FILES));

    AddSampleDescription(Transfer, AssemblyDescription);

    UnicodeString DestPath = Transfer->Path;
    UnicodeString TransferMethodName;
    if (Transfer->ToRemote)
    {
      TransferMethodName = L"PutFiles";
      DestPath = UnixIncludeTrailingBackslash(DestPath);
    }
    else
    {
      TransferMethodName = L"GetFiles";
      DestPath = IncludeTrailingBackslash(DestPath);
    }
    DestPath += NoOpOperationMask;

    UnicodeString DestPathVariableName = AssemblyVariableName(Language, L"remotePath");
    UnicodeString StatementSeparator = AssemblyStatementSeparator(Language);
    UnicodeString DestPathString = AssemblyString(Language, DestPath);
    UnicodeString DestPathCode;
    if (Transfer->AllFiles || (Transfer->Paths.size() == 1))
    {
      DestPathCode = DestPathString;
    }
    else
    {
      switch (Language)
      {
        case alCSharp:
          Code += RtfKeyword(L"const") + L" " + RtfKeyword(L"string") + L" " + DestPathVariableName;
          break;

        case alVBNET:
          Code += RtfKeyword(L"Const") + L" " + DestPathVariableName;
          break;

        case alPowerShell:
          Code += DestPathVariableName;
          break;
      }
      Code += L" = " + DestPathString + StatementSeparator + RtfPara;
      DestPathCode = DestPathVariableName;
    }

    UnicodeString TransferMethodCallStart =
      AssemblyVariableName(Language, SessionClassName) + L"." +
      RtfLibraryMethod(SessionClassName, TransferMethodName, false) + L"(";
    const UnicodeString ParameterSeparator = L", ";
    UnicodeString TransferMethodCallEnd = ParameterSeparator + DestPathCode;
    if (Transfer->Move || HasTransferOptions)
    {
      TransferMethodCallEnd += ParameterSeparator + AssemblyBoolean(Language, Transfer->Move);
    }
    if (HasTransferOptions)
    {
      TransferMethodCallEnd += ParameterSeparator + AssemblyVariableName(Language, TransferOptionsClassName);
    }

    TransferMethodCallEnd +=
      L")." + RtfLibraryMethod(L"OperationResultBase", L"Check", true) + L"()" +
      StatementSeparator + RtfPara;

    UnicodeString SourcePath = UniversalIncludeTrailingBackslash(!Transfer->ToRemote, Transfer->SourcePath);
    if (!Transfer->AllFiles)
    {
      for (const UnicodeString & FileName : Transfer->Paths)
      {
        UnicodeString Path = SourcePath + FileName;
        UnicodeString PathCode = AssemblyString(Language, Path);
        if (!Transfer->ToRemote && (FileName != EscapeFileMask(FileName)))
        {
          PathCode =
            AssemblyVariableName(Language, SessionClassName) + L"." +
            RtfLibraryMethod(SessionClassName, L"EscapeFileMask", false) + L"(" + PathCode + L")";
        }
        Code += TransferMethodCallStart + PathCode + TransferMethodCallEnd;
      }
    }
    else
    {
      SourcePath += AllFilesMask;
      Code += TransferMethodCallStart + AssemblyString(Language, SourcePath) + TransferMethodCallEnd;
    }
  }
  else
  {
    Code = AssemblyCommentLine(Language, LoadStr(GENERATE_URL_YOUR_CODE));
  }

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
  const UnicodeString & ExeName, const TGenerateTransfer * Transfer)
{
  using TTarget = CoreBridge::TGenerateCodeOptions::TTarget;
  CoreBridge::TGeneratedCode Result;
  const bool FileUrl = !Options.Paths.empty() && (Transfer == nullptr);
  TTarget Target = FileUrl ? TTarget::Url : Options.Target;
  // A transfer has no URL page (UrlSheet hidden).
  if ((Transfer != nullptr) && (Target == TTarget::Url))
  {
    Target = TTarget::Script;
  }
  Result.Title = Narrow(LoadStr((Transfer != nullptr) ? GENERATE_URL_TRANSFER_TITLE :
    (FileUrl ? GENERATE_URL_FILE_TITLE : GENERATE_URL_SESSION_TITLE)));

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
    const int ScriptFormat = std::min(std::max(Options.ScriptFormat, 0), 4);
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
    Rtf = GenerateScript(Data, ScriptFormat, ExeName, Transfer, Description);
  }
  else
  {
    Result.ResultCaption = Narrow(LoadStr(GENERATE_URL_CODE));
    if (HostKeyUnknown)
    {
      Description += LoadStr(GENERATE_URL_HOSTKEY_UNKNOWN) + L"\n";
    }
    Rtf = GenerateAssemblyCode(Data, std::min(std::max(Options.AssemblyLanguage, 0), 2), Transfer, Description);
    Result.WordWrap = false;
    Result.FixedWidth = true;
  }
  Result.Rtf = Narrow(Rtf);
  Result.Description = Narrow(Description);
  return Result;
}
