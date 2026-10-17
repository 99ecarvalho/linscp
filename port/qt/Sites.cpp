// CoreBridge's stored sites: core's TStoredSessionList, used as upstream's
// Login dialog uses it (forms/Login.cpp; saving as forms/Custom.cpp's
// DoSaveSession).
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"
#include "ShellConfiguration.h"

#include <CoreMain.h>
#include <SessionData.h>
#include <RemoteFiles.h>
#include <TextsWin.h>
#include <GUIConfiguration.h>
#include <Configuration.h>
#include <HierarchicalStorage.h>
#include <Common.h>
#include <Cryptography.h>
#include <PuttyTools.h>
#include <S3FileSystem.h>
#include <TextsCore.h>

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
    case pmTelnet: return TSite::TProxy::Telnet;
    case pmCmd: return TSite::TProxy::Command;
    // The SSH tunnel's internal method is not offered; such a site's
    // method is kept (SessionDataOf leaves it alone).
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
    case TSite::TProxy::Telnet: return pmTelnet;
    case TSite::TProxy::Command: return pmCmd;
    default: return pmNone;
  }
}

TSite::TAutoSwitch AutoSwitchOf(TAutoSwitch Value)
{
  return static_cast<TSite::TAutoSwitch>(Value);
}

TAutoSwitch AutoSwitchOf(TSite::TAutoSwitch Value)
{
  return static_cast<TAutoSwitch>(Value);
}

// Site Advanced's SSH bugs, in its page's order.
const TSshBug SshBugs[TSite::SshBugCount] =
  { sbHMAC2, sbDeriveKey2, sbRSAPad2, sbPKSessID2, sbRekey2, sbMaxPkt2, sbIgnore2, sbWinAdj };

// IndexToTlsVersion and TlsVersionToIndex.
TSite::TTls TlsOf(TTlsVersion Version)
{
  switch (Version)
  {
    case tls11: return TSite::TTls::Tls11;
    case tls12: return TSite::TTls::Tls12;
    case tls13: return TSite::TTls::Tls13;
    default: return TSite::TTls::Tls10;
  }
}

TTlsVersion TlsVersionOf(TSite::TTls Tls)
{
  switch (Tls)
  {
    case TSite::TTls::Tls11: return tls11;
    case TSite::TTls::Tls12: return tls12;
    case TSite::TTls::Tls13: return tls13;
    default: return tls10;
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
  // As the Login dialog's LoadSession: the master password asked once at
  // most; not given, the passwords stay unknown (PasswordsLocked).
  {
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    const int Refusals = Shell->MasterPasswordRefusals();
    Shell->BeginMasterPasswordSession();
    try
    {
      Site.Password = Utf8(Data->Password);
      Site.ProxyPassword = Utf8(Data->ProxyPassword);
      Site.TunnelPassword = Utf8(Data->TunnelPassword);
      Site.EncryptKey = Utf8(Data->EncryptKey);
    }
    catch (...)
    {
      Shell->EndMasterPasswordSession();
      throw;
    }
    Shell->EndMasterPasswordSession();
    // Core reads what it could not decrypt as empty.
    if (Shell->MasterPasswordRefusals() != Refusals)
    {
      Site.Password.clear();
      Site.ProxyPassword.clear();
      Site.TunnelPassword.clear();
      Site.EncryptKey.clear();
      Site.PasswordsLocked = true;
    }
  }
  Site.RemoteDirectory = Utf8(Data->RemoteDirectory);
  Site.LocalDirectory = Utf8(Data->LocalDirectory);
  Site.UpdateDirectories = Data->UpdateDirectories;
  Site.Timeout = Data->Timeout;
  Site.FtpPassive = Data->FtpPasvMode;
  Site.Proxy = ProxyOf(Data->ProxyMethod);
  Site.ProxyHost = Utf8(Data->ProxyHost);
  Site.ProxyPort = Data->ProxyPort;
  Site.ProxyUserName = Utf8(Data->ProxyUsername);
  Site.ProxyLocalhost = Data->ProxyLocalhost;
  Site.PrivateKeyFile = Utf8(Data->PublicKeyFile);
  Site.TryAgent = Data->TryAgent;
  Site.KeyboardInteractive = Data->AuthKI;
  Site.AgentForwarding = Data->AgentFwd;
  // The rest of Site Advanced's pages, as its LoadSession.
  Site.SynchronizeBrowsing = Data->SynchronizeBrowsing;
  Site.CacheDirectories = Data->CacheDirectories;
  Site.CacheDirectoryChanges = Data->CacheDirectoryChanges;
  Site.PreserveDirectoryChanges = Data->PreserveDirectoryChanges;
  Site.ResolveSymlinks = Data->ResolveSymlinks;
  Site.FollowDirectorySymlinks = Data->FollowDirectorySymlinks;
  // dstmKeep is deprecated: read as Unix's.
  Site.DstWindows = (Data->DSTMode == dstmWin);
  Site.EolCrLf = (Data->EOLType != eolLF);
  Site.Utf = AutoSwitchOf(Data->NotUtf);
  Site.TimeDifferenceMinutes = TimeToMinutes(Data->TimeDifference);
  Site.TimeDifferenceAuto = Data->TimeDifferenceAuto;
  Site.TrimVMSVersions = Data->TrimVMSVersions;
  Site.VMSAllRevisions = Data->VMSAllRevisions;
  Site.DeleteToRecycleBin = Data->DeleteToRecycleBin;
  Site.OverwrittenToRecycleBin = Data->OverwrittenToRecycleBin;
  Site.RecycleBinPath = Utf8(Data->RecycleBinPath);
  Site.SftpServer = Utf8(Data->SftpServer);
  Site.AllowScpFallback = (Data->FSProtocol != fsSFTPonly);
  Site.UsePosixRename = Data->UsePosixRename;
  Site.SftpMaxVersion = (Data->SFTPMaxVersion < 0) ? -1 : Data->SFTPMaxVersion;
  Site.SftpRealPath = AutoSwitchOf(Data->SFTPRealPath);
  Site.SftpBugSymlink = AutoSwitchOf(Data->SFTPBug[sbSymlink]);
  Site.SftpBugSignedTS = AutoSwitchOf(Data->SFTPBug[sbSignedTS]);
  Site.Shell = Data->DefaultShell ? std::string() : Utf8(Data->Shell);
  Site.ReturnVar = Data->DetectReturnVar ? std::string() : Utf8(Data->ReturnVar);
  Site.ListingCommand = Utf8(Data->ListingCommand);
  Site.LookupUserGroups = AutoSwitchOf(Data->LookupUserGroups);
  Site.ClearAliases = Data->ClearAliases;
  Site.IgnoreLsWarnings = Data->IgnoreLsWarnings;
  Site.Scp1Compatibility = Data->Scp1Compatibility;
  Site.UnsetNationalVars = Data->UnsetNationalVars;
  Site.ScpLsFullTime = (Data->SCPLsFullTime != asOff);
  Site.FtpAccount = Utf8(Data->FtpAccount);
  Site.PostLoginCommands = Utf8(Data->PostLoginCommands);
  Site.FtpListAll = AutoSwitchOf(Data->FtpListAll);
  Site.FtpUseMlsd = AutoSwitchOf(Data->FtpUseMlsd);
  Site.FtpForcePasvIp = AutoSwitchOf(Data->FtpForcePasvIp);
  Site.FtpHost = AutoSwitchOf(Data->FtpHost);
  Site.S3CredentialsEnv = Data->S3CredentialsEnv;
  Site.S3Profile = Utf8(Data->S3Profile);
  Site.S3DefaultRegion = Utf8(Data->S3DefaultRegion);
  Site.S3UrlStylePath = (Data->S3UrlStyle == s3usPath);
  Site.S3RequesterPays = Data->S3RequesterPays;
  Site.S3SessionToken = Utf8(Data->S3SessionToken);
  Site.S3RoleArn = Utf8(Data->S3RoleArn);
  Site.WebDavLiberalEscaping = Data->WebDavLiberalEscaping;
  Site.WebDavCrossDomainRedirects = Data->WebDavCrossDomainRedirects;
  Site.OptimizeBuffers = (Data->SendBuf > 0) && Data->SshSimple;
  Site.Ping = static_cast<TSite::TPing>(Data->PingType);
  Site.PingInterval = Data->PingInterval;
  switch (Data->FtpPingType)
  {
    case fptDummyCommand0:
    case fptDummyCommand: Site.FtpPing = TSite::TFtpPing::DummyCommand; break;
    case fptDirectoryListing: Site.FtpPing = TSite::TFtpPing::DirectoryListing; break;
    default: Site.FtpPing = TSite::TFtpPing::Off; break;
  }
  Site.FtpPingInterval = Data->FtpPingInterval;
  Site.AddressFamily = static_cast<TSite::TAddressFamily>(Data->AddressFamily);
  Site.FtpProxyLogonType = Data->FtpProxyLogonType;
  Site.ProxyTelnetCommand = Utf8(Data->ProxyTelnetCommand);
  Site.ProxyLocalCommand = Utf8(Data->ProxyLocalCommand);
  Site.ProxyDns = AutoSwitchOf(Data->ProxyDNS);
  Site.Tunnel = Data->Tunnel;
  Site.TunnelHostName = Utf8(Data->TunnelHostName);
  Site.TunnelPortNumber = Data->TunnelPortNumber;
  Site.TunnelUserName = Utf8(Data->TunnelUserName);
  Site.TunnelPrivateKeyFile = Utf8(Data->TunnelPublicKeyFile);
  Site.TunnelLocalPortNumber = Data->TunnelAutoassignLocalPortNumber ? 0 : Data->TunnelLocalPortNumber;
  Site.MinTls = TlsOf(Data->MinTlsVersion);
  Site.MaxTls = TlsOf(Data->MaxTlsVersion);
  Site.SslSessionReuse = Data->SslSessionReuse;
  Site.TlsCertificateFile = Utf8(Data->TlsCertificateFile);
  Site.Compression = Data->Compression;
  Site.Ssh2DES = Data->Ssh2DES;
  Site.Ciphers.clear();
  for (int Index = 0; Index < CIPHER_COUNT; Index++)
  {
    Site.Ciphers.push_back(Data->Cipher[Index]);
  }
  Site.Kex.clear();
  for (int Index = 0; Index < KEX_COUNT; Index++)
  {
    Site.Kex.push_back(Data->Kex[Index]);
  }
  Site.AuthGSSAPIKEX = Data->AuthGSSAPIKEX;
  Site.RekeyTime = Data->RekeyTime;
  Site.RekeyData = Utf8(Data->RekeyData);
  Site.SshNoUserAuth = Data->SshNoUserAuth;
  Site.KeyboardInteractivePassword = Data->AuthKIPassword;
  Site.AuthGSSAPI = Data->AuthGSSAPI;
  Site.GSSAPIFwdTGT = Data->GSSAPIFwdTGT;
  Site.DetachedCertificate = Utf8(Data->DetachedCertificate);
  for (int Index = 0; Index < TSite::SshBugCount; Index++)
  {
    Site.SshBugs[Index] = AutoSwitchOf(Data->Bug[SshBugs[Index]]);
  }
  Site.Note = Utf8(Data->Note);
  Site.Color = Data->Color;
  return Site;
}

// Site Advanced's SaveSession, on Data: the settings of its pages.
void ApplyAdvanced(TSessionData * Data, const TSite & Site)
{
  Data->SynchronizeBrowsing = Site.SynchronizeBrowsing;
  Data->CacheDirectories = Site.CacheDirectories;
  Data->CacheDirectoryChanges = Site.CacheDirectoryChanges;
  Data->PreserveDirectoryChanges = Site.PreserveDirectoryChanges;
  Data->ResolveSymlinks = Site.ResolveSymlinks;
  Data->FollowDirectorySymlinks = Site.FollowDirectorySymlinks;
  Data->DSTMode = Site.DstWindows ? dstmWin : dstmUnix;
  Data->EOLType = Site.EolCrLf ? eolCRLF : eolLF;
  Data->NotUtf = AutoSwitchOf(Site.Utf);
  Data->TimeDifference = double(Site.TimeDifferenceMinutes) / MinsPerDay;
  Data->TimeDifferenceAuto = Site.TimeDifferenceAuto;
  Data->TrimVMSVersions = Site.TrimVMSVersions;
  Data->VMSAllRevisions = Site.VMSAllRevisions;
  Data->DeleteToRecycleBin = Site.DeleteToRecycleBin;
  Data->OverwrittenToRecycleBin = Site.OverwrittenToRecycleBin;
  Data->RecycleBinPath = FromUtf8(Site.RecycleBinPath);
  // SFTP: SCP as its fallback, or SFTP only.
  if ((Data->FSProtocol == fsSFTP) || (Data->FSProtocol == fsSFTPonly))
  {
    Data->FSProtocol = Site.AllowScpFallback ? fsSFTP : fsSFTPonly;
  }
  Data->SftpServer = FromUtf8(Site.SftpServer);
  Data->UsePosixRename = Site.UsePosixRename;
  Data->SFTPMaxVersion = (Site.SftpMaxVersion < 0) ? SFTPMaxVersionAuto : Site.SftpMaxVersion;
  Data->SFTPRealPath = AutoSwitchOf(Site.SftpRealPath);
  Data->SFTPBug[sbSymlink] = AutoSwitchOf(Site.SftpBugSymlink);
  Data->SFTPBug[sbSignedTS] = AutoSwitchOf(Site.SftpBugSignedTS);
  Data->DefaultShell = Site.Shell.empty();
  Data->Shell = FromUtf8(Site.Shell);
  Data->DetectReturnVar = Site.ReturnVar.empty();
  Data->ReturnVar = FromUtf8(Site.ReturnVar);
  Data->ListingCommand = FromUtf8(Site.ListingCommand);
  Data->LookupUserGroups = AutoSwitchOf(Site.LookupUserGroups);
  Data->ClearAliases = Site.ClearAliases;
  Data->IgnoreLsWarnings = Site.IgnoreLsWarnings;
  Data->Scp1Compatibility = Site.Scp1Compatibility;
  Data->UnsetNationalVars = Site.UnsetNationalVars;
  Data->SCPLsFullTime = Site.ScpLsFullTime ? asAuto : asOff;
  Data->FtpAccount = FromUtf8(Site.FtpAccount);
  Data->PostLoginCommands = FromUtf8(Site.PostLoginCommands);
  Data->FtpListAll = AutoSwitchOf(Site.FtpListAll);
  Data->FtpUseMlsd = AutoSwitchOf(Site.FtpUseMlsd);
  Data->FtpForcePasvIp = AutoSwitchOf(Site.FtpForcePasvIp);
  Data->FtpHost = AutoSwitchOf(Site.FtpHost);
  Data->S3CredentialsEnv = Site.S3CredentialsEnv;
  Data->S3Profile = FromUtf8(Site.S3Profile);
  Data->S3DefaultRegion = FromUtf8(Site.S3DefaultRegion);
  Data->S3UrlStyle = Site.S3UrlStylePath ? s3usPath : s3usVirtualHost;
  Data->S3RequesterPays = Site.S3RequesterPays;
  // With automatic credentials the token is the environment's, not kept;
  // trimmed not to authenticate with a stray new line.
  Data->S3SessionToken = Data->HasAutoCredentials() ? UnicodeString() : FromUtf8(Site.S3SessionToken).Trim();
  Data->S3RoleArn = FromUtf8(Site.S3RoleArn);
  Data->WebDavLiberalEscaping = Site.WebDavLiberalEscaping;
  Data->WebDavCrossDomainRedirects = Site.WebDavCrossDomainRedirects;
  Data->SendBuf = Site.OptimizeBuffers ? DefaultSendBuf : 0;
  Data->SshSimple = Site.OptimizeBuffers;
  Data->PingType = static_cast<TPingType>(Site.Ping);
  Data->PingInterval = Site.PingInterval;
  switch (Site.FtpPing)
  {
    case TSite::TFtpPing::DummyCommand: Data->FtpPingType = fptDummyCommand; break;
    case TSite::TFtpPing::DirectoryListing: Data->FtpPingType = fptDirectoryListing; break;
    default: Data->FtpPingType = fptOff; break;
  }
  Data->FtpPingInterval = Site.FtpPingInterval;
  Data->AddressFamily = static_cast<TAddressFamily>(Site.AddressFamily);
  Data->FtpProxyLogonType = Site.FtpProxyLogonType;
  Data->ProxyTelnetCommand = FromUtf8(Site.ProxyTelnetCommand);
  Data->ProxyLocalCommand = FromUtf8(Site.ProxyLocalCommand);
  Data->ProxyDNS = AutoSwitchOf(Site.ProxyDns);
  Data->Tunnel = Site.Tunnel;
  Data->TunnelHostName = FromUtf8(Site.TunnelHostName);
  Data->TunnelPortNumber = Site.TunnelPortNumber;
  Data->TunnelUserName = FromUtf8(Site.TunnelUserName);
  if (!Site.PasswordsLocked || !Site.TunnelPassword.empty())
  {
    Data->TunnelPassword = FromUtf8(Site.TunnelPassword);
  }
  Data->TunnelPublicKeyFile = FromUtf8(Site.TunnelPrivateKeyFile);
  Data->TunnelLocalPortNumber = Site.TunnelLocalPortNumber;
  Data->MinTlsVersion = TlsVersionOf(Site.MinTls);
  Data->MaxTlsVersion = TlsVersionOf(Site.MaxTls);
  Data->SslSessionReuse = Site.SslSessionReuse;
  Data->TlsCertificateFile = FromUtf8(Site.TlsCertificateFile);
  Data->Compression = Site.Compression;
  Data->Ssh2DES = Site.Ssh2DES;
  if (Site.Ciphers.size() == CIPHER_COUNT)
  {
    for (int Index = 0; Index < CIPHER_COUNT; Index++)
    {
      Data->Cipher[Index] = static_cast<TCipher>(Site.Ciphers[Index]);
    }
  }
  if (Site.Kex.size() == KEX_COUNT)
  {
    for (int Index = 0; Index < KEX_COUNT; Index++)
    {
      Data->Kex[Index] = static_cast<TKex>(Site.Kex[Index]);
    }
  }
  Data->AuthGSSAPIKEX = Site.AuthGSSAPIKEX;
  Data->RekeyTime = Site.RekeyTime;
  Data->RekeyData = FromUtf8(Site.RekeyData);
  Data->SshNoUserAuth = Site.SshNoUserAuth;
  Data->AuthKIPassword = Site.KeyboardInteractivePassword;
  Data->AuthGSSAPI = Site.AuthGSSAPI;
  Data->GSSAPIFwdTGT = Site.GSSAPIFwdTGT;
  Data->DetachedCertificate = FromUtf8(Site.DetachedCertificate);
  for (int Index = 0; (Index < TSite::SshBugCount) && (Index < static_cast<int>(Site.SshBugs.size())); Index++)
  {
    Data->Bug[SshBugs[Index]] = AutoSwitchOf(Site.SshBugs[Index]);
  }
  Data->Note = FromUtf8(Site.Note);
  Data->Color = Site.Color;
  if (!Site.PasswordsLocked || !Site.EncryptKey.empty())
  {
    Data->EncryptKey = FromUtf8(Site.EncryptKey);
  }
}

}  // namespace

// The session data of Site: the stored site of BaseName's settings (or
// core's defaults), with the Login dialog's fields applied. The caller owns it.
TSessionData * SessionDataOf(const TSite & Site, const std::string & BaseName)
{
  std::unique_ptr<TSessionData> Data(new TSessionData(L""));
  TSessionData * Base = !BaseName.empty() ? StoredSite(BaseName) : NULL;
  // Passwords not known (PasswordsLocked): copied as stored, encrypted,
  // rather than read, which would ask for the master password again and
  // copy what it could not read as empty.
  if (Site.PasswordsLocked && (Base != NULL))
  {
    Data->CopyDataNoRecrypt(Base);
  }
  else
  {
    Data->Assign((Base != NULL) ? Base : StoredSessions->DefaultSettings);
  }
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
  // Unknown passwords (PasswordsLocked) stay as stored, unless typed anew.
  if (!Site.PasswordsLocked || !Site.Password.empty())
  {
    Data->Password = FromUtf8(Site.Password);
  }
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
  if (!Site.PasswordsLocked || !Site.ProxyPassword.empty())
  {
    Data->ProxyPassword = FromUtf8(Site.ProxyPassword);
  }
  Data->ProxyLocalhost = Site.ProxyLocalhost;
  Data->PublicKeyFile = FromUtf8(Site.PrivateKeyFile);
  Data->TryAgent = Site.TryAgent;
  Data->AuthKI = Site.KeyboardInteractive;
  Data->AgentFwd = Site.AgentForwarding;
  ApplyAdvanced(Data.get(), Site);
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
    // As AskForMasterPasswordIfNotSetAndNeededToPersistSessionData: its
    // passwords are stored encrypted with the master password, so not
    // without it (refused: not saved).
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    if (!Configuration->DisablePasswordStoring && Data->HasAnyPassword() && Shell->UseMasterPassword())
    {
      Shell->AskForMasterPasswordIfNotSet();
    }
    StoredSessions->NewSession(Name, Data.get());
    // Modified only, explicit: as DoSaveSession.
    StoredSessions->Save(false, true);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

// As TLoginDialog::DoParseUrl.
static void ParseLoginUrl(TSessionData * Data, const UnicodeString & Url, int & ParsedInfo)
{
  Data->ParseUrl(Url, NULL, NULL, ParsedInfo, NULL, NULL, pufPreferProtocol);
  Data->RequireDirectories = false;
}

TSite ParseSiteUrl(const TSite & Site, const std::string & Url, bool & Unsafe)
{
  try
  {
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, Site.Name));
    const UnicodeString Text = FromUtf8(Url);
    Unsafe = false;
    // Otherwise the colons would be taken for the URL's.
    if (IsIPv6Literal(Text))
    {
      Data->HostName = HasIP6LiteralBrackets(Text) ? StripIP6LiteralBrackets(Text) : Text;
    }
    else
    {
      int ParsedInfo = 0;
      ParseLoginUrl(Data.get(), Text, ParsedInfo);
      Unsafe = FLAGSET(ParsedInfo, piUnsafeSettings);
    }
    TSite Result = SiteOf(Data.get());
    Result.Name = Site.Name;
    return Result;
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

bool HostNameIsUrl(const std::string & HostName)
{
  try
  {
    const UnicodeString Text = FromUtf8(HostName).Trim();
    if (Text.IsEmpty())
    {
      return false;
    }
    std::unique_ptr<TSessionData> Parsed(new TSessionData(EmptyStr));
    int ParsedInfo = 0;
    ParseLoginUrl(Parsed.get(), Text, ParsedInfo);
    std::unique_ptr<TSessionData> Plain(new TSessionData(EmptyStr));
    Plain->HostName = Text;
    // Has the legacy HostName parsing intervened?
    return (Plain->HostName != Text) || !Parsed->IsSameDecrypted(Plain.get());
  }
  catch (Exception &)
  {
    return false;
  }
}

std::string TimeSpanText(double Seconds)
{
  return Utf8(FormatDateTimeSpan(TDateTime(Seconds / SecsPerDay)));
}

std::string MinimizeName(const std::string & Path, int MaxLen, bool Unix)
{
  return Utf8(::MinimizeName(FromUtf8(Path), MaxLen, Unix));
}

std::string NowTimeText()
{
  return Utf8(Now().TimeString());
}

std::string ConfigurationExportPath()
{
  return Utf8(Configuration->GetDefaultIniFileExportPath());
}

void ExportConfiguration(const std::string & FileName)
{
  try
  {
    Configuration->Export(FromUtf8(FileName));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void ImportConfiguration(const std::string & FileName)
{
  try
  {
    Configuration->Import(FromUtf8(FileName));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string UniqueSiteName(const std::string & Name)
{
  std::unique_ptr<TSessionData> Data(new TSessionData(FromUtf8(Name)));
  Data->MakeUniqueIn(StoredSessions);
  return Utf8(Data->Name);
}

void SaveDefaultSite(const TSite & Site)
{
  try
  {
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, std::string()));
    // As SetDefaultSessionActionExecute: the master password first.
    TShellConfiguration * Shell = dynamic_cast<TShellConfiguration *>(GUIConfiguration);
    if (!Configuration->DisablePasswordStoring && Data->HasAnyPassword() && Shell->UseMasterPassword())
    {
      Shell->AskForMasterPasswordIfNotSet();
    }
    // Stored by the setter (only modified, explicit).
    StoredSessions->DefaultSettings = Data.get();
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

void ValidateSiteName(const std::string & Label)
{
  try
  {
    TSessionData::ValidateName(FromUtf8(Label));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

void RenameWorkspace(const std::string & Name, const std::string & NewName)
{
  try
  {
    const UnicodeString OldRoot = FromUtf8(Name);
    const UnicodeString NewRoot = FromUtf8(NewName);
    if (OldRoot == NewRoot)
    {
      return;
    }
    TSessionData::ValidateName(NewRoot);
    // As CheckDuplicateFolder.
    if (!SameText(OldRoot, NewRoot) && (StoredSessions->IsFolderOrWorkspace(NewRoot)))
    {
      throw Exception(MainInstructions(FMTLOAD(LOGIN_DUPLICATE_SESSION_FOLDER_WORKSPACE, (NewRoot))));
    }
    std::vector<TSessionData *> Moved;
    for (int Index = 0; Index < StoredSessions->Count; Index++)
    {
      TSessionData * Data = StoredSessions->Sessions[Index];
      if (Data->IsWorkspace && Data->IsInFolderOrWorkspace(OldRoot))
      {
        Moved.push_back(Data);
      }
    }
    for (TSessionData * Data : Moved)
    {
      const UnicodeString Path = NewRoot + Data->Name.SubString(OldRoot.Length() + 1, Data->Name.Length() - OldRoot.Length());
      Data->Remove();
      TSessionData * NewData = StoredSessions->NewSession(Path, Data);
      // The same one, when only the letter case changed.
      if (NewData != Data)
      {
        StoredSessions->Remove(Data);
      }
    }
    // Modified only, explicit.
    StoredSessions->Save(false, true);
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

int SiteColor(const std::string & Name)
{
  const TSessionData * Data = StoredSite(Name);
  return (Data != NULL) ? Data->Color : 0;
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

std::pair<std::string, std::string> TImportSites::KeyFiles(int Source, int Index) const
{
  TStoredSessionList * List = ((Source >= 0) && (Source < SourceCount)) ? F->Lists[Source].get() : NULL;
  if ((List == NULL) || (Index < 0) || (Index >= List->Count))
  {
    return {};
  }
  const TSessionData * Data = List->Sessions[Index];
  return { Utf8(Data->PublicKeyFile), Utf8(Data->TunnelPublicKeyFile) };
}

void TImportSites::SetKeyFiles(int Source, int Index, const std::pair<std::string, std::string> & Files)
{
  TStoredSessionList * List = ((Source >= 0) && (Source < SourceCount)) ? F->Lists[Source].get() : NULL;
  if ((List != NULL) && (Index >= 0) && (Index < List->Count))
  {
    TSessionData * Data = List->Sessions[Index];
    Data->PublicKeyFile = FromUtf8(Files.first);
    Data->TunnelPublicKeyFile = FromUtf8(Files.second);
  }
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

std::vector<std::string> CipherNames()
{
  std::vector<std::string> Result;
  for (int Index = 0; Index < CIPHER_COUNT; Index++)
  {
    Result.push_back(Utf8(LoadStr(CIPHER_NAME_WARN + Index)));
  }
  return Result;
}

std::vector<std::string> KexNames()
{
  std::vector<std::string> Result;
  for (int Index = 0; Index < KEX_COUNT; Index++)
  {
    Result.push_back(Utf8(LoadStr(KEX_NAME_WARN + Index)));
  }
  return Result;
}

TS3Credentials S3Credentials(const TSite & Site)
{
  TS3Credentials Result;
  try
  {
    std::unique_ptr<TSessionData> Data(SessionDataOf(Site, std::string()));
    Result.Automatic = Data->HasAutoCredentials();
    Result.Amazon = IsAmazonS3SessionData(Data.get());
    Result.SessionToken = Utf8(Data->S3SessionToken);
    Result.RoleArn = Utf8(Data->S3RoleArn);
    if (Result.Automatic)
    {
      // As LoadSession: what the environment gives, if anything.
      try
      {
        Result.SessionToken = Utf8(S3EnvSessionToken(Data->S3Profile));
        Result.RoleArn = Utf8(S3EnvRoleArn(Data->S3Profile));
      }
      catch (...)
      {
      }
    }
  }
  catch (Exception &)
  {
  }
  return Result;
}

bool KeyHasCertificate(const std::string & FileName)
{
  bool Result = false;
  try
  {
    UnicodeString Comment;
    GetPublicKeyLine(FromUtf8(FileName), Comment, Result);
  }
  catch (...)
  {
    Result = false;
  }
  return Result;
}

std::string PublicKeyLine(const std::string & FileName)
{
  try
  {
    UnicodeString Comment;
    bool HasCertificate;
    return Utf8(GetPublicKeyLine(FromUtf8(FileName), Comment, HasCertificate));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

std::string GenerateEncryptKey()
{
  return Utf8(BytesToHex(::GenerateEncryptKey()));
}

void ValidateEncryptKey(const std::string & HexKey)
{
  try
  {
    ::ValidateEncryptKey(HexToBytes(FromUtf8(HexKey)));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

int TunnelLocalPortNumberLow()
{
  return Configuration->TunnelLocalPortNumberLow;
}

int TunnelLocalPortNumberHigh()
{
  return Configuration->TunnelLocalPortNumberHigh;
}

std::vector<std::string> S3Profiles()
{
  std::vector<std::string> Result;
  try
  {
    std::unique_ptr<TStringList> Profiles(new TStringList());
    std::unique_ptr<TStrings> Found(GetS3Profiles());
    Profiles->AddStrings(Found.get());
    Profiles->Sort();
    for (int Index = 0; Index < Profiles->Count; Index++)
    {
      Result.push_back(Utf8(Profiles->Strings[Index]));
    }
  }
  catch (Exception &)
  {
  }
  return Result;
}

std::string S3GeneralName()
{
  return Utf8(LoadStr(LOGIN_S3_GENERAL_CREDENTIALS));
}

TS3EnvCredentials S3EnvCredentials(const std::string & Profile)
{
  TS3EnvCredentials Result;
  try
  {
    const UnicodeString AProfile = FromUtf8(Profile);
    Result.UserName = Utf8(S3EnvUserName(AProfile));
    Result.Password = Utf8(S3EnvPassword(AProfile));
    Result.SessionToken = Utf8(S3EnvSessionToken(AProfile));
    Result.RoleArn = Utf8(S3EnvRoleArn(AProfile));
  }
  catch (Exception &)
  {
  }
  return Result;
}

std::string FtpAnonymousUserName()
{
  return Utf8(::AnonymousUserName);
}

std::string FtpAnonymousPassword()
{
  return Utf8(::AnonymousPassword);
}

}  // namespace CoreBridge
