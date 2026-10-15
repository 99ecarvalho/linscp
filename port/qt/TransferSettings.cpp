// The bridge's transfer settings: core's TCopyParamType and the GUI's
// presets (TGUIConfiguration), as WinSCP's dialogs edit them. VCL side.
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"
#include "ShellConfiguration.h"
#include "TransferSettings.h"

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <CopyParam.h>
#include <GUIConfiguration.h>

namespace CoreBridge
{

TTransferSettings SettingsOf(const TGUICopyParamType & CopyParam)
{
  TTransferSettings Result;
  Result.Mode = static_cast<TTransferSettings::TMode>(CopyParam.TransferMode);
  Result.TextMask = Utf8(CopyParam.AsciiFileMask.Masks);
  Result.NameCase = static_cast<TTransferSettings::TNameCase>(CopyParam.FileNameCase);
  Result.ReplaceInvalidChars = CopyParam.ReplaceInvalidChars;
  Result.PreserveTime = CopyParam.PreserveTime;
  Result.PreserveTimeDirs = CopyParam.PreserveTimeDirs;
  Result.CalculateSize = CopyParam.CalculateSize;
  Result.SpeedLimit = CopyParam.CPSLimit;
  Result.PreserveReadOnly = CopyParam.PreserveReadOnly;
  Result.PreserveRights = CopyParam.PreserveRights;
  Result.Rights = Utf8(CopyParam.Rights.Octal);
  Result.AddXToDirectories = CopyParam.AddXToDirectories;
  Result.IgnorePermErrors = CopyParam.IgnorePermErrors;
  Result.ClearArchive = CopyParam.ClearArchive;
  Result.RemoveBomAndEof = CopyParam.RemoveCtrlZ && CopyParam.RemoveBOM;
  Result.EncryptNewFiles = CopyParam.EncryptNewFiles;
  Result.IncludeFileMask = Utf8(CopyParam.IncludeFileMask.Masks);
  Result.NewerOnly = CopyParam.NewerOnly;
  Result.ExcludeHiddenFiles = CopyParam.ExcludeHiddenFiles;
  Result.ExcludeEmptyDirectories = CopyParam.ExcludeEmptyDirectories;
  Result.Resume = static_cast<TTransferSettings::TResume>(CopyParam.ResumeSupport);
  Result.ResumeThreshold = CopyParam.ResumeThreshold;
  Result.Queue = CopyParam.Queue;
  Result.QueueNoConfirmation = CopyParam.QueueNoConfirmation;
  Result.QueueParallel = CopyParam.QueueParallel;
  return Result;
}

// Over Base, so what the shell does not edit (the invalid characters'
// replacement, for one) stays as it was.
TGUICopyParamType CopyParamOf(const TTransferSettings & Settings, const TGUICopyParamType & Base)
{
  TGUICopyParamType Result(Base);
  Result.TransferMode = static_cast<TTransferMode>(Settings.Mode);
  Result.AsciiFileMask = FromUtf8(Settings.TextMask);
  Result.FileNameCase = static_cast<TFileNameCase>(Settings.NameCase);
  Result.ReplaceInvalidChars = Settings.ReplaceInvalidChars;
  Result.PreserveTime = Settings.PreserveTime;
  Result.PreserveTimeDirs = Settings.PreserveTimeDirs;
  Result.CalculateSize = Settings.CalculateSize;
  Result.CPSLimit = Settings.SpeedLimit;
  Result.PreserveReadOnly = Settings.PreserveReadOnly;
  Result.PreserveRights = Settings.PreserveRights;
  TRights Rights = Result.Rights;
  Rights.Octal = FromUtf8(Settings.Rights);
  Result.Rights = Rights;
  Result.AddXToDirectories = Settings.AddXToDirectories;
  Result.IgnorePermErrors = Settings.IgnorePermErrors;
  Result.ClearArchive = Settings.ClearArchive;
  Result.RemoveCtrlZ = Settings.RemoveBomAndEof;
  Result.RemoveBOM = Settings.RemoveBomAndEof;
  Result.EncryptNewFiles = Settings.EncryptNewFiles;
  Result.IncludeFileMask = FromUtf8(Settings.IncludeFileMask);
  Result.NewerOnly = Settings.NewerOnly;
  Result.ExcludeHiddenFiles = Settings.ExcludeHiddenFiles;
  Result.ExcludeEmptyDirectories = Settings.ExcludeEmptyDirectories;
  Result.ResumeSupport = static_cast<TResumeSupport>(Settings.Resume);
  Result.ResumeThreshold = Settings.ResumeThreshold;
  Result.Queue = Settings.Queue;
  Result.QueueNoConfirmation = Settings.QueueNoConfirmation;
  Result.QueueParallel = Settings.QueueParallel;
  return Result;
}

TGUICopyParamType TransferCopyParam(const TTransferSettings * Settings)
{
  const TGUICopyParamType Current = GUIConfiguration->CurrentCopyParam;
  return (Settings != NULL) ? CopyParamOf(*Settings, Current) : Current;
}

std::string TransferSettingsInfo(const TTransferSettings & Settings)
{
  const TGUICopyParamType CopyParam = CopyParamOf(Settings, GUIConfiguration->DefaultCopyParam);
  return Utf8(CopyParam.GetInfoStr(L"; ", 0));
}

namespace
{

TTransferPresetRule RuleOf(const TCopyParamRuleData & Data)
{
  TTransferPresetRule Result;
  Result.HostName = Utf8(Data.HostName);
  Result.UserName = Utf8(Data.UserName);
  Result.RemoteDirectory = Utf8(Data.RemoteDirectory);
  Result.LocalDirectory = Utf8(Data.LocalDirectory);
  return Result;
}

TCopyParamRuleData RuleDataOf(const TTransferPresetRule & Rule)
{
  TCopyParamRuleData Result;
  Result.HostName = FromUtf8(Rule.HostName);
  Result.UserName = FromUtf8(Rule.UserName);
  Result.RemoteDirectory = FromUtf8(Rule.RemoteDirectory);
  Result.LocalDirectory = FromUtf8(Rule.LocalDirectory);
  return Result;
}

}  // namespace

std::string FindTransferPreset(const TTransferPresetRule & Data, bool & Found)
{
  const TCopyParamList * List = GUIConfiguration->CopyParamList;
  const int Index = List->Find(RuleDataOf(Data));
  Found = (Index >= 0);
  return Found ? Utf8(List->Names[Index]) : std::string();
}

std::string TransferPresetRuleInfo(const TTransferPresetRule & Rule)
{
  return Utf8(TCopyParamRule(RuleDataOf(Rule)).GetInfoStr(L"\n"));
}

TTransferSettings DefaultTransferSettings()
{
  return SettingsOf(GUIConfiguration->DefaultCopyParam);
}

std::vector<TTransferPreset> TransferPresets()
{
  std::vector<TTransferPreset> Result;
  const TCopyParamList * List = GUIConfiguration->CopyParamList;
  for (int Index = 0; Index < List->Count; Index++)
  {
    TTransferPreset Preset;
    Preset.Name = Utf8(List->Names[Index]);
    TGUICopyParamType CopyParam(GUIConfiguration->DefaultCopyParam);
    CopyParam.Assign(List->CopyParams[Index]);
    Preset.Settings = SettingsOf(CopyParam);
    if (List->Rules[Index] != NULL)
    {
      const TCopyParamRuleData & Data = List->Rules[Index]->Data;
      Preset.HasRule = true;
      Preset.Rule = RuleOf(Data);
    }
    Result.push_back(Preset);
  }
  return Result;
}

std::string CurrentTransferPreset()
{
  return Utf8(GUIConfiguration->CopyParamCurrent);
}

TTransferSettings CurrentTransferSettings()
{
  return SettingsOf(GUIConfiguration->CurrentCopyParam);
}

void SetDefaultTransferSettings(const TTransferSettings & Settings)
{
  try
  {
    GUIConfiguration->DefaultCopyParam = CopyParamOf(Settings, GUIConfiguration->DefaultCopyParam);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As TPreferencesDialog edits its copy of the list: each preset by name,
// over what it was, with its rule.
void SetTransferPresets(const std::vector<TTransferPreset> & Presets)
{
  try
  {
    const TCopyParamList * Old = GUIConfiguration->CopyParamList;
    TCopyParamList List;
    for (const TTransferPreset & Preset : Presets)
    {
      const UnicodeString Name = FromUtf8(Preset.Name);
      TCopyParamList::ValidateName(Name);
      const int OldIndex = Old->IndexOfName(Name);
      TGUICopyParamType Base(GUIConfiguration->DefaultCopyParam);
      if (OldIndex >= 0)
      {
        Base.Assign(Old->CopyParams[OldIndex]);
      }
      TCopyParamType * CopyParam = new TCopyParamType(CopyParamOf(Preset.Settings, Base));
      TCopyParamRule * Rule = Preset.HasRule ? new TCopyParamRule(RuleDataOf(Preset.Rule)) : NULL;
      List.Add(Name, CopyParam, Rule);
    }
    GUIConfiguration->CopyParamList = &List;
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void SetCurrentTransferPreset(const std::string & Name)
{
  try
  {
    GUIConfiguration->CopyParamCurrent = FromUtf8(Name);
    Configuration->SaveExplicit();
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}  // namespace CoreBridge
