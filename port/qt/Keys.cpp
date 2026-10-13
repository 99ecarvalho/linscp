// CoreBridge's private keys: checked and converted as upstream's Login dialog
// does (windows/Tools.cpp: DoVerifyKey, ConvertKey), over core's PuttyTools.
#include <vcl.h>
#pragma hdrstop

#include "BridgeVcl.h"

#include <Exceptions.h>
#include <PuttyTools.h>
#include <TextsCore.h>
#include <TextsWin.h>

#include <memory>

namespace CoreBridge
{

namespace
{

// windows/Tools.cpp's, which is GUI code upstream.
UnicodeString GetConvertedKeyFileName(const UnicodeString & FileName)
{
  return ChangeFileExt(FileName, FORMAT(L".%s", (PuttyKeyExt)));
}

bool TryAddMatchingKeyCertificate(TPrivateKey * PrivateKey, const UnicodeString & CertificateFileName,
  UnicodeString & Result)
{
  if (FileExists(CertificateFileName))
  {
    try
    {
      AddCertificateToKey(PrivateKey, CertificateFileName);
      Result = CertificateFileName;
    }
    catch (Exception &)
    {
      // As upstream: a certificate that does not fit is not added.
    }
  }
  return !Result.IsEmpty();
}

UnicodeString AddMatchingKeyCertificate(TPrivateKey * PrivateKey, const UnicodeString & FileName)
{
  UnicodeString CertificateFileName = FileName;
  const UnicodeString S = FORMAT(L".%s", (PuttyKeyExt));
  if (EndsText(S, CertificateFileName))
  {
    CertificateFileName.SetLength(CertificateFileName.Length() - S.Length());
  }
  UnicodeString Result;
  TryAddMatchingKeyCertificate(PrivateKey, CertificateFileName + L"-cert.pub", Result) ||
    TryAddMatchingKeyCertificate(PrivateKey, CertificateFileName + L".pub-aadcert.pub", Result);
  return Result;
}

}  // namespace

TKeyCheck CheckKey(const std::string & AFileName)
{
  TKeyCheck Result;
  const UnicodeString FileName = ExpandEnvironmentVariables(FromUtf8(AFileName));
  if (FileName.Trim().IsEmpty())
  {
    return Result;
  }
  try
  {
    const TKeyType Type = KeyType(FileName);
    switch (Type)
    {
      case ktOpenSSHPEM:
      case ktOpenSSHNew:
      case ktSSHCom:
        {
          const UnicodeString TypeName = (Type == ktSSHCom) ? L"ssh.com" : L"OpenSSH";
          const UnicodeString Message = FMTLOAD(KEY_TYPE_UNSUPPORTED2, (FileName, TypeName));
          Result.Status = TKeyCheck::TStatus::Convertible;
          Result.Message = Utf8(UnformatMessage(FMTLOAD(KEY_TYPE_CONVERT4, (TypeName, RemoveMainInstructionsTag(Message)))));
          Result.ConvertedFileName = Utf8(GetConvertedKeyFileName(FileName));
        }
        break;
      case ktSSH1:
        Result.Status = TKeyCheck::TStatus::Unusable;
        Result.Message = Utf8(UnformatMessage(FMTLOAD(KEY_TYPE_SSH1, (FileName))));
        break;
      case ktSSH2:
        Result.Message = Utf8(UnformatMessage(TestKey(Type, FileName)));
        if (!Result.Message.empty())
        {
          Result.Status = TKeyCheck::TStatus::Unusable;
        }
        break;
      case ktSSH1Public:
      case ktSSH2PublicRFC4716:
      case ktSSH2PublicOpenSSH:
        // As upstream: a public key, for an agent's key.
        break;
      case ktUnopenable:
        Result.Status = TKeyCheck::TStatus::Unusable;
        Result.Message = Utf8(UnformatMessage(FMTLOAD(KEY_TYPE_UNOPENABLE, (FileName))));
        break;
      default:
        Result.Status = TKeyCheck::TStatus::Unusable;
        Result.Message = Utf8(UnformatMessage(FMTLOAD(KEY_TYPE_UNKNOWN2, (FileName))));
        break;
    }
  }
  catch (Exception & E)
  {
    Result.Status = TKeyCheck::TStatus::Unusable;
    Result.Message = Utf8(E.Message);
  }
  return Result;
}

std::string ConvertKey(const std::string & AFileName, const std::string & Target)
{
  try
  {
    const UnicodeString FileName = ExpandEnvironmentVariables(FromUtf8(AFileName));
    const TKeyType Type = KeyType(FileName);
    UnicodeString Passphrase;
    UnicodeString Comment;
    if (IsKeyEncrypted(Type, FileName, Comment))
    {
      TPrompt Prompt;
      Prompt.Kind = TPromptKind::Passphrase;
      Prompt.Name = Utf8(LoadStr(PASSPHRASE_TITLE));
      TPromptField Field;
      Field.Label = Utf8(FORMAT(LoadStr(PROMPT_KEY_PASSPHRASE), (Comment)));
      Field.Echo = false;
      Prompt.Fields.push_back(Field);
      if (!Frontend()->Prompt(Prompt))
      {
        Abort();
      }
      Passphrase = FromUtf8(Prompt.Fields[0].Value);
    }

    std::unique_ptr<TPrivateKey, void (*)(TPrivateKey *)> PrivateKey(LoadKey(Type, FileName, Passphrase), FreeKey);
    UnicodeString CertificateMessage;
    const UnicodeString CertificateFileName = AddMatchingKeyCertificate(PrivateKey.get(), FileName);
    if (!CertificateFileName.IsEmpty())
    {
      CertificateMessage = L"\n" + FMTLOAD(CERTIFICATE_ADDED, (CertificateFileName));
    }
    const UnicodeString TargetFileName = FromUtf8(Target);
    SaveKey(ktSSH2, TargetFileName, Passphrase, PrivateKey.get());
    return Utf8(UnformatMessage(MainInstructions(FMTLOAD(CONVERTKEY_SAVED, (TargetFileName))) + CertificateMessage));
  }
  catch (Exception & E)
  {
    throw CoreErrorOf(E);
  }
}

}
