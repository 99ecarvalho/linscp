// CoreBridge's stored sites: core's TStoredSessionList, used as upstream's
// Login dialog uses it (forms/Login.cpp; saving as forms/Custom.cpp's
// DoSaveSession).
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <CoreMain.h>
#include <SessionData.h>
#include <TextsWin.h>
#include <GUIConfiguration.h>
#include <Configuration.h>
#include <HierarchicalStorage.h>
#include <Common.h>

#include <memory>

namespace CoreBridge
{

namespace
{

TSite::TProtocol ProtocolOf(const TSessionData * Data)
{
  switch (Data->FSProtocol)
  {
    case fsSCPonly: return TSite::TProtocol::Scp;
    case fsFTP: return TSite::TProtocol::Ftp;
    case fsWebDAV: return TSite::TProtocol::WebDav;
    case fsS3: return TSite::TProtocol::S3;
    default: return TSite::TProtocol::Sftp;
  }
}

TFSProtocol FSProtocolOf(TSite::TProtocol Protocol)
{
  switch (Protocol)
  {
    case TSite::TProtocol::Scp: return fsSCPonly;
    case TSite::TProtocol::Ftp: return fsFTP;
    case TSite::TProtocol::WebDav: return fsWebDAV;
    case TSite::TProtocol::S3: return fsS3;
    // The Login dialog's "SFTP": with SCP as the fallback.
    default: return fsSFTP;
  }
}

TSite::TEncryption EncryptionOf(TFtps Ftps)
{
  switch (Ftps)
  {
    case ftpsImplicit: return TSite::TEncryption::Implicit;
    case ftpsExplicitSsl:
    case ftpsExplicitTls: return TSite::TEncryption::Explicit;
    default: return TSite::TEncryption::None;
  }
}

TFtps FtpsOf(TSite::TProtocol Protocol, TSite::TEncryption Encryption)
{
  if ((Protocol == TSite::TProtocol::Sftp) || (Protocol == TSite::TProtocol::Scp))
  {
    return ftpsNone;
  }
  switch (Encryption)
  {
    case TSite::TEncryption::Implicit: return ftpsImplicit;
    // Only FTP has explicit TLS; WebDAV and S3 take it as TLS.
    case TSite::TEncryption::Explicit:
      return (Protocol == TSite::TProtocol::Ftp) ? ftpsExplicitTls : ftpsImplicit;
    default: return ftpsNone;
  }
}

TSite::TProxy ProxyOf(TProxyMethod Method)
{
  switch (Method)
  {
    case pmSocks4: return TSite::TProxy::Socks4;
    case pmSocks5: return TSite::TProxy::Socks5;
    case pmHTTP: return TSite::TProxy::Http;
    // Telnet, local command and SSH tunnel proxies are not offered; such a
    // site's method is kept (SessionDataOf leaves it alone).
    default: return TSite::TProxy::None;
  }
}

TProxyMethod ProxyMethodOf(TSite::TProxy Proxy)
{
  switch (Proxy)
  {
    case TSite::TProxy::Socks4: return pmSocks4;
    case TSite::TProxy::Socks5: return pmSocks5;
    case TSite::TProxy::Http: return pmHTTP;
    default: return pmNone;
  }
}

TSessionData * StoredSite(const std::string & Name)
{
  return dynamic_cast<TSessionData *>(StoredSessions->FindByName(FromUtf8(Name)));
}

TSite SiteOf(const TSessionData * Data)
{
  TSite Site;
  Site.Name = Utf8(Data->Name);
  Site.Protocol = ProtocolOf(Data);
  Site.Encryption = EncryptionOf(Data->Ftps);
  Site.HostName = Utf8(Data->HostName);
  Site.Port = (Data->PortNumber == DefaultPort(Data->FSProtocol, Data->Ftps)) ? 0 : Data->PortNumber;
  Site.UserName = Utf8(Data->UserName);
  Site.Password = Utf8(Data->Password);
  Site.RemoteDirectory = Utf8(Data->RemoteDirectory);
  Site.LocalDirectory = Utf8(Data->LocalDirectory);
  Site.UpdateDirectories = Data->UpdateDirectories;
  Site.Timeout = Data->Timeout;
  Site.FtpPassive = Data->FtpPasvMode;
  Site.Proxy = ProxyOf(Data->ProxyMethod);
  Site.ProxyHost = Utf8(Data->ProxyHost);
  Site.ProxyPort = Data->ProxyPort;
  Site.ProxyUserName = Utf8(Data->ProxyUsername);
  Site.ProxyPassword = Utf8(Data->ProxyPassword);
  Site.ProxyLocalhost = Data->ProxyLocalhost;
  Site.PrivateKeyFile = Utf8(Data->PublicKeyFile);
  Site.TryAgent = Data->TryAgent;
  Site.KeyboardInteractive = Data->AuthKI;
  Site.AgentForwarding = Data->AgentFwd;
  return Site;
}

}  // namespace

// The session data of Site: the stored site of BaseName's settings (or
// core's defaults), with the Login dialog's fields applied. The caller owns it.
TSessionData * SessionDataOf(const TSite & Site, const std::string & BaseName)
{
  std::unique_ptr<TSessionData> Data(new TSessionData(L""));
  TSessionData * Base = !BaseName.empty() ? StoredSite(BaseName) : NULL;
  Data->Assign((Base != NULL) ? Base : StoredSessions->DefaultSettings);
  // Raw settings edited: they replace the base's, as TSiteRawDialog's
  // Execute (the fields below set the rest).
  if (Site.HasRawSettings)
  {
    Data->DefaultSettings();
    std::unique_ptr<TStrings> Raw(TextToStringList(FromUtf8(Site.RawSettings)));
    Data->ApplyRawSettings(Raw.get(), false);
  }
  Data->FSProtocol = FSProtocolOf(Site.Protocol);
  Data->Ftps = FtpsOf(Site.Protocol, Site.Encryption);
  Data->HostName = FromUtf8(Site.HostName);
  Data->PortNumber = (Site.Port > 0) ? Site.Port : DefaultPort(Data->FSProtocol, Data->Ftps);
  Data->UserName = FromUtf8(Site.UserName);
  Data->Password = FromUtf8(Site.Password);
  Data->RemoteDirectory = FromUtf8(Site.RemoteDirectory);
  Data->LocalDirectory = FromUtf8(Site.LocalDirectory);
  Data->UpdateDirectories = Site.UpdateDirectories;
  Data->Timeout = Site.Timeout;
  Data->FtpPasvMode = Site.FtpPassive;
  // A method the dialog does not offer (telnet, command, tunnel) stays.
  if ((Site.Proxy != TSite::TProxy::None) || (ProxyOf(Data->ProxyMethod) != TSite::TProxy::None))
  {
    Data->ProxyMethod = ProxyMethodOf(Site.Proxy);
  }
  Data->ProxyHost = FromUtf8(Site.ProxyHost);
  Data->ProxyPort = Site.ProxyPort;
  Data->ProxyUsername = FromUtf8(Site.ProxyUserName);
  Data->ProxyPassword = FromUtf8(Site.ProxyPassword);
  Data->ProxyLocalhost = Site.ProxyLocalhost;
  Data->PublicKeyFile = FromUtf8(Site.PrivateKeyFile);
  Data->TryAgent = Site.TryAgent;
  Data->AuthKI = Site.KeyboardInteractive;
  Data->AgentFwd = Site.AgentForwarding;
  if (!Site.Name.empty())
  {
    Data->Name = FromUtf8(Site.Name);
  }
  return Data.release();
}

int DefaultPort(TSite::TProtocol Protocol, TSite::TEncryption Encryption)
{
  return ::DefaultPort(FSProtocolOf(Protocol), FtpsOf(Protocol, Encryption));
}

std::vector<std::string> SiteNames()
{
  std::vector<std::string> Result;
  for (int Index = 0; Index < StoredSessions->Count; Index++)
  {
    const TSessionData * Data = StoredSessions->Sessions[Index];
    if (!Data->IsWorkspace)
    {
      Result.push_back(Utf8(Data->Name));
    }
  }
  return Result;
}

TSite NewSite()
{
  TSite Site = SiteOf(StoredSessions->DefaultSettings);
  Site.Name.clear();
  return Site;
}

TSite LoadSite(const std::string & Name)
{
  const TSessionData * Data = StoredSite(Name);
  if (Data == NULL)
  {
    TError Error;
    // Core has no text for it (FILEZILLA_SITE_NOT_EXIST is FileZilla's).
    Error.Message = "Site \"" + Name + "\" was not found.";
    throw TCoreError(Error, false);
  }
  return SiteOf(Data);
}

namespace
{

TStringList * RawLines(const std::string & RawSettings)
{
  std::unique_ptr<TStringList> Lines(new TStringList());
  Lines->Text = FromUtf8(RawSettings);
  // Empty lines are no settings.
  for (int Index = Lines->Count - 1; Index >= 0; Index--)
  {
    if (Lines->Strings[Index].Trim().IsEmpty())
    {
      Lines->Delete(Index);
    }
  }
  return Lines.release();
}

}  // namespace

std::string SiteRawSettings(const TSite & Site, const std::string & BaseName)
{
  try
  {
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, BaseName));
    std::unique_ptr<TSessionData> FactoryDefaults(new TSessionData(L""));
    std::unique_ptr<TSessionData> RawData(new TSessionData(L""));
    RawData->Assign(Data.get());
    // SFTP-only is not reflected by the protocol prefix, we have to use rawsettings for that
    if (RawData->FSProtocol != fsSFTPonly)
    {
      RawData->FSProtocol = FactoryDefaults->FSProtocol;
    }
    RawData->HostName = FactoryDefaults->HostName;
    RawData->PortNumber = FactoryDefaults->PortNumber;
    RawData->UserName = FactoryDefaults->UserName;
    RawData->Password = FactoryDefaults->Password;
    RawData->Ftps = FactoryDefaults->Ftps;
    std::unique_ptr<TStrings> Options(RawData->SaveToOptions(FactoryDefaults.get(), false, false));
    return Utf8(Options->Text);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TSite ApplySiteRawSettings(const TSite & Site, const std::string & BaseName, const std::string & RawSettings)
{
  try
  {
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, BaseName));
    std::unique_ptr<TSessionData> BackupData(new TSessionData(L""));
    BackupData->Assign(Data.get());
    Data->DefaultSettings();
    Data->FSProtocol = BackupData->FSProtocol;
    Data->HostName = BackupData->HostName;
    Data->PortNumber = BackupData->PortNumber;
    Data->UserName = BackupData->UserName;
    Data->Password = BackupData->Password;
    Data->Ftps = BackupData->Ftps;
    std::unique_ptr<TStrings> Settings(RawLines(RawSettings));
    Data->ApplyRawSettings(Settings.get(), false);
    TSite Result = SiteOf(Data.get());
    Result.Name = Site.Name;
    Result.HasRawSettings = true;
    // As they are now: what applying them made of them.
    std::unique_ptr<TSessionData> FactoryDefaults(new TSessionData(L""));
    Data->FSProtocol = (Data->FSProtocol != fsSFTPonly) ? FactoryDefaults->FSProtocol : Data->FSProtocol;
    Data->HostName = FactoryDefaults->HostName;
    Data->PortNumber = FactoryDefaults->PortNumber;
    Data->UserName = FactoryDefaults->UserName;
    Data->Password = FactoryDefaults->Password;
    Data->Ftps = FactoryDefaults->Ftps;
    std::unique_ptr<TStrings> Options(Data->SaveToOptions(FactoryDefaults.get(), false, false));
    Result.RawSettings = Utf8(Options->Text);
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// TSiteRawDialog::AddButtonClick's list.
std::vector<std::pair<std::string, std::string>> RawSettingsToAdd(const std::string & RawSettings)
{
  std::unique_ptr<TSessionData> FactoryDefaults(new TSessionData(L""));
  std::unique_ptr<TSessionData> BasicData(new TSessionData(L""));
  BasicData->FSProtocol = TFSProtocol(FactoryDefaults->FSProtocol + 1);
  UnicodeString RandomAppendix(L"_");
  BasicData->HostName = FactoryDefaults->HostName + RandomAppendix;
  BasicData->Ftps = TFtps(FactoryDefaults->Ftps + 1);
  BasicData->PortNumber = BasicData->GetDefaultPort() + 1;
  BasicData->UserName = FactoryDefaults->UserName + RandomAppendix;
  BasicData->Password = FactoryDefaults->Password + RandomAppendix;
  std::unique_ptr<TStrings> BasicOptions(BasicData->SaveToOptions(FactoryDefaults.get(), false, false));
  std::unique_ptr<TStrings> AllOptions(TSessionData::GetAllOptionNames(false));
  std::unique_ptr<TStringList> Names(CreateSortedStringList());
  for (int Index = 0; Index < AllOptions->Count; Index++)
  {
    Names->Add(AllOptions->Names[Index]);
  }
  std::unique_ptr<TStrings> Settings(RawLines(RawSettings));
  for (TStrings * Options : { BasicOptions.get(), Settings.get() })
  {
    for (int Index = 0; Index < Options->Count; Index++)
    {
      const UnicodeString Name = Options->Names[Index];
      const int I = Name.IsEmpty() ? -1 : Names->IndexOf(Name);
      if (I >= 0)
      {
        Names->Delete(I);
      }
    }
  }
  std::vector<std::pair<std::string, std::string>> Result;
  for (int Index = 0; Index < Names->Count; Index++)
  {
    Result.emplace_back(Utf8(Names->Strings[Index]), Utf8(AllOptions->Values[Names->Strings[Index]]));
  }
  return Result;
}

void SaveSite(const TSite & Site, const std::string & BaseName)
{
  try
  {
    const UnicodeString Name = FromUtf8(Site.Name);
    if (Name.Trim().IsEmpty())
    {
      throw Exception(L"A site needs a name.");
    }
    TSessionData::ValidatePath(Name);
    const TSessionData * Existing = StoredSite(Site.Name);
    if ((Existing != NULL) && Existing->Special)
    {
      throw Exception(FMTLOAD(CANNOT_OVERWRITE_SPECIAL_SESSION, (Name)));
    }
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, BaseName));
    StoredSessions->NewSession(Name, Data.get());
    // Modified only, explicit: as DoSaveSession.
    StoredSessions->Save(false, true);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void RenameSite(const std::string & Name, const std::string & NewName)
{
  if (Name == NewName)
  {
    return;
  }
  TSite Site = LoadSite(Name);
  Site.Name = NewName;
  SaveSite(Site, Name);
  try
  {
    // As the Login dialog's rename: the old one goes.
    TSessionData * Original = StoredSite(Name);
    Original->Remove();
    StoredSessions->Remove(Original);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool SiteExists(const std::string & Name)
{
  return StoredSite(Name) != NULL;
}

TUrlInfo UrlInfo(const std::string & Text)
{
  const int Info = StoredSessions->GetUrlInfo(FromUtf8(Text));
  TUrlInfo Result;
  Result.ProtocolDefined = FLAGSET(Info, piProtocolDefined);
  Result.UnsafeSettings = FLAGSET(Info, piUnsafeSettings);
  return Result;
}

std::vector<std::string> WorkspaceNames()
{
  std::unique_ptr<TStrings> Names(StoredSessions->GetWorkspaces());
  std::vector<std::string> Result;
  for (int Index = 0; Index < Names->Count; Index++)
  {
    Result.push_back(Utf8(Names->Strings[Index]));
  }
  return Result;
}

int WorkspaceSessionCount(const std::string & Name)
{
  const UnicodeString AName = FromUtf8(Name);
  int Result = 0;
  for (int Index = 0; Index < StoredSessions->Count; Index++)
  {
    TSessionData * Data = StoredSessions->Sessions[Index];
    if (Data->IsWorkspace && Data->IsInFolderOrWorkspace(AName))
    {
      Result++;
    }
  }
  return Result;
}

void DeleteWorkspace(const std::string & Name)
{
  try
  {
    const UnicodeString AName = FromUtf8(Name);
    for (int Index = StoredSessions->Count - 1; Index >= 0; Index--)
    {
      TSessionData * Data = StoredSessions->Sessions[Index];
      if (Data->IsWorkspace && Data->IsInFolderOrWorkspace(AName))
      {
        Data->Remove();
        StoredSessions->Remove(Data);
      }
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void ValidateWorkspaceName(const std::string & Name)
{
  try
  {
    const UnicodeString AName = FromUtf8(Name);
    TSessionData::ValidateName(AName);
    if (StoredSessions->IsFolder(AName))
    {
      throw Exception(FMTLOAD(FOLDER_NOT_WORKSPACE, (AName)));
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void DeleteSite(const std::string & Name)
{
  try
  {
    TSessionData * Data = StoredSite(Name);
    if (Data != NULL)
    {
      Data->Remove();
      StoredSessions->Remove(Data);
    }
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}

namespace CoreBridge
{

// --- Import sites --------------------------------------------------------------

struct TImportSites::TImpl
{
  std::unique_ptr<TStoredSessionList> Lists[SourceCount];
  UnicodeString Errors[SourceCount];
  UnicodeString IniFileName;
};

TImportSites::TImportSites() :
  F(new TImpl())
{
  try
  {
    UnicodeString Error;
    F->Lists[Putty].reset(GUIConfiguration->SelectPuttySessionsForImport(OriginalPuttyRegistryStorageKey, L"PuTTY", StoredSessions, Error));
    F->Errors[Putty] = Error;
    F->Lists[Kitty].reset(GUIConfiguration->SelectPuttySessionsForImport(KittyRegistryStorageKey, L"KiTTY", StoredSessions, Error));
    F->Errors[Kitty] = Error;
    F->Lists[FileZilla].reset(Configuration->SelectFilezillaSessionsForImport(StoredSessions, Error));
    F->Errors[FileZilla] = Error;
    F->Lists[OpenSsh].reset(Configuration->SelectOpensshSessionsForImport(StoredSessions, Error));
    F->Errors[OpenSsh] = Error;
    F->Lists[Ini].reset(Configuration->SelectSessionsForImport(StoredSessions, F->IniFileName, Error));
    F->Errors[Ini] = Error;
    F->Lists[KnownHosts].reset(Configuration->SelectKnownHostsSessionsForImport(StoredSessions, Error));
    F->Errors[KnownHosts] = Error;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

TImportSites::~TImportSites() = default;

std::string TImportSites::SourceName(int Source) const
{
  static const char * const Names[] = { "PuTTY", "KiTTY", "FileZilla", "OpenSSH", "INI file", "known_hosts" };
  return ((Source >= 0) && (Source < SourceCount)) ? Names[Source] : std::string();
}

std::string TImportSites::Error(int Source) const
{
  return ((Source >= 0) && (Source < SourceCount)) ? Utf8(F->Errors[Source]) : std::string();
}

// TImportSessionsDialog::SessionListView2InfoTip.
std::vector<std::pair<std::string, std::string>> TImportSites::Sites(int Source) const
{
  std::vector<std::pair<std::string, std::string>> Result;
  TStoredSessionList * List = ((Source >= 0) && (Source < SourceCount)) ? F->Lists[Source].get() : NULL;
  for (int Index = 0; (List != NULL) && (Index < List->Count); Index++)
  {
    TSessionData * Data = List->Sessions[Index];
    UnicodeString InfoTip;
    if (Source == KnownHosts)
    {
      UnicodeString Algs;
      UnicodeString HostKeys = Data->HostKey;
      while (!HostKeys.IsEmpty())
      {
        UnicodeString HostKey = CutToChar(HostKeys, L';', true);
        UnicodeString Alg = CutToChar(HostKey, L':', true);
        AddToList(Algs, Alg, L", ");
      }
      InfoTip = FMTLOAD(IMPORT_KNOWNHOSTS_INFO_TIP, (Data->HostName, Algs));
    }
    else
    {
      InfoTip = Data->InfoTip;
    }
    Result.emplace_back(Utf8(Data->Name), Utf8(InfoTip));
  }
  return Result;
}

bool TImportSites::Selected(int Source, int Index) const
{
  TStoredSessionList * List = F->Lists[Source].get();
  return (List != NULL) && (Index >= 0) && (Index < List->Count) && List->Sessions[Index]->Selected;
}

void TImportSites::Select(int Source, int Index, bool Selected)
{
  TStoredSessionList * List = F->Lists[Source].get();
  if ((List != NULL) && (Index >= 0) && (Index < List->Count))
  {
    List->Sessions[Index]->Selected = Selected;
  }
}

void TImportSites::LoadIni(const std::string & FileName)
{
  F->IniFileName = FromUtf8(FileName);
  UnicodeString Error;
  F->Lists[Ini].reset(Configuration->SelectSessionsForImport(StoredSessions, F->IniFileName, Error));
  F->Errors[Ini] = Error;
}

void TImportSites::LoadKnownHosts(const std::string & Text)
{
  std::unique_ptr<TStrings> Lines(new TStringList());
  Lines->Text = FromUtf8(Text);
  UnicodeString Error;
  F->Lists[KnownHosts].reset(Configuration->SelectKnownHostsSessionsForImport(Lines.get(), StoredSessions, Error));
  F->Errors[KnownHosts] = Error;
}

std::vector<std::string> TImportSites::Import(int Source)
{
  try
  {
    // As Execute's ClearSelections: only the source shown is imported.
    for (int Index = 0; Index < SourceCount; Index++)
    {
      TStoredSessionList * List = F->Lists[Index].get();
      for (int Site = 0; (Index != Source) && (List != NULL) && (Site < List->Count); Site++)
      {
        List->Sessions[Site]->Selected = false;
      }
    }
    std::unique_ptr<TList> Imported(new TList());
    UnicodeString PuttyHostKeysSourceKey = OriginalPuttyRegistryStorageKey;
    StoredSessions->Import(F->Lists[Putty].get(), true, Imported.get());
    TStoredSessionList::ImportHostKeys(PuttyHostKeysSourceKey, F->Lists[Putty].get(), true);
    StoredSessions->Import(F->Lists[Kitty].get(), true, Imported.get());
    TStoredSessionList::ImportHostKeys(KittyRegistryStorageKey, F->Lists[Kitty].get(), true);
    StoredSessions->Import(F->Lists[FileZilla].get(), true, Imported.get());
    // FileZilla uses PuTTY's host key store
    TStoredSessionList::ImportHostKeys(PuttyHostKeysSourceKey, F->Lists[FileZilla].get(), true);
    StoredSessions->Import(F->Lists[OpenSsh].get(), true, Imported.get());
    if (StoredSessions->Import(F->Lists[Ini].get(), true, Imported.get()))
    {
      std::unique_ptr<THierarchicalStorage> IniStorage(TIniFileStorage::CreateFromPath(F->IniFileName));
      TStoredSessionList::ImportHostKeys(IniStorage.get(), F->Lists[Ini].get(), true);
    }
    // The actual import will be done by ImportSelectedKnownHosts
    TStoredSessionList::SelectKnownHostsForSelectedSessions(F->Lists[KnownHosts].get(), F->Lists[OpenSsh].get());
    TStoredSessionList::ImportSelectedKnownHosts(F->Lists[KnownHosts].get());
    std::vector<std::string> Result;
    for (int Index = 0; Index < Imported->Count; Index++)
    {
      Result.push_back(Utf8(static_cast<TSessionData *>(Imported->Items[Index])->Name));
    }
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}  // namespace CoreBridge
