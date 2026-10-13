// CoreBridge's stored sites: core's TStoredSessionList, used as upstream's
// Login dialog uses it (forms/Login.cpp; saving as forms/Custom.cpp's
// DoSaveSession).
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <CoreMain.h>
#include <SessionData.h>
#include <TextsWin.h>

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
  Data->FSProtocol = FSProtocolOf(Site.Protocol);
  Data->Ftps = FtpsOf(Site.Protocol, Site.Encryption);
  Data->HostName = FromUtf8(Site.HostName);
  Data->PortNumber = (Site.Port > 0) ? Site.Port : DefaultPort(Data->FSProtocol, Data->Ftps);
  Data->UserName = FromUtf8(Site.UserName);
  Data->Password = FromUtf8(Site.Password);
  Data->RemoteDirectory = FromUtf8(Site.RemoteDirectory);
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
