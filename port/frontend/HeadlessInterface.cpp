// The program-specific half of core's Interface.h, for a process with no GUI:
// what a command-line client or a test harness links (FrontendCommon.cpp has
// the shared half). Where upstream's definition needs a GUI, the comment
// says what stands in for it.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>

#include <Common.h>
#include <Exceptions.h>
#include <Interface.h>
#include <Configuration.h>
#include <Cryptography.h>
#include <HierarchicalStorage.h>
#include <Security.h>
#include <TextsWin.h>
#include "HeadlessMain.h"

#include "FrontendCommon.h"
//---------------------------------------------------------------------------
// The message and its details go to stderr, where a console client reports.
void __fastcall ShowExtendedException(Exception * E)
{
  UnicodeString Message;
  if (ExceptionMessageFormatted(E, Message))
  {
    ExtException * EE = dynamic_cast<ExtException *>(E);
    if ((EE != NULL) && (EE->MoreMessages != NULL) && !EE->MoreMessages->Text.IsEmpty())
    {
      Message += L"\n" + EE->MoreMessages->Text;
    }
    std::fputs(UTF8String(Message + L"\n").c_str(), stderr);
    std::fflush(stderr);
  }
}
//---------------------------------------------------------------------------
// Upstream swaps in the hourglass cursor; there is no cursor.
void * __fastcall BusyStart()
{
  return NULL;
}
//---------------------------------------------------------------------------
void __fastcall BusyEnd(void * /*Token*/)
{
}
//---------------------------------------------------------------------------
// Upstream pumps the VCL message queue at most every GUIUpdateInterval and
// reports whether it did. There is no queue to pump.
bool __fastcall ProcessGUI(bool /*Force*/)
{
  return false;
}
//---------------------------------------------------------------------------
// Only reachable from the Copy button of a GUI prompt.
void __fastcall CopyToClipboard(UnicodeString /*Text*/)
{
  throw Exception(L"No clipboard is available.");
}
//---------------------------------------------------------------------------
// Core asks before offering to paste a password; there is nothing to paste.
bool __fastcall TextFromClipboard(UnicodeString & /*Text*/, bool /*Trim*/)
{
  return false;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
namespace
{
// TConfiguration leaves only TemporaryDir abstract; upstream's
// TWinConfiguration adds the GUI's settings on top, which a program without
// a GUI has none of.
THeadlessMasterPasswordPrompt GMasterPasswordPrompt = nullptr;

class THeadlessConfiguration : public TConfiguration
{
public:
  // TConfiguration's constructor leaves the settings to Default(), which
  // TWinConfiguration's constructor calls; so does this one. Without it
  // every setting is zero.
  THeadlessConfiguration()
  {
    Default();
  }

  virtual __fastcall ~THeadlessConfiguration()
  {
    Shred(FMasterPassword);
  }

  virtual UnicodeString TemporaryDir(bool Mask)
  {
    return DefaultTemporaryDir(Mask);
  }

protected:
  // Storage detection is the application configuration's, as
  // TWinConfiguration's GetStorage: core's own fallback asserts that it is
  // never reached. The same rule as that fallback (an existing INI file,
  // else the registry); only Windows without /ini gets here.
  virtual TStorage __fastcall GetStorage()
  {
    {
      TGuard Guard(FCriticalSection);
      if (FStorage == stDetect)
      {
        FStorage = FileExists(ApiPath(IniFileStorageNameForReading)) ? stIniFile : stRegistry;
      }
    }
    return TConfiguration::GetStorage();
  }

  // TWinConfiguration's master password, read only: what the GUI set
  // (Security\UseMasterPassword and its verifier), so the console client
  // can open sites whose passwords it encrypted, as WinSCP.com can.
  virtual void __fastcall LoadData(THierarchicalStorage * Storage)
  {
    TConfiguration::LoadData(Storage);
    if (Storage->OpenSubKeyPath(L"Security", false))
    {
      FUseMasterPassword = Storage->ReadBool(L"UseMasterPassword", FUseMasterPassword);
      FMasterPasswordVerifier = Storage->ReadString(L"MasterPasswordVerifier", FMasterPasswordVerifier);
      Storage->CloseSubKeyPath();
    }
  }

  // As TWinConfiguration::DecryptPassword: a password encrypted with the
  // master password asks for it once (TConsoleRunner::MasterPasswordPrompt),
  // checked against the verifier and asked again until it matches.
  virtual UnicodeString __fastcall DecryptPassword(RawByteString Password, UnicodeString Key)
  {
    RawByteString Buf;
    if (!GetExternalEncryptedPassword(Password, Buf))
    {
      return TConfiguration::DecryptPassword(Password, Key);
    }
    if (FMasterPassword.IsEmpty())
    {
      AskForMasterPassword();
    }
    UnicodeString Result;
    if (!AES256DecryptWithMAC(Buf, FMasterPassword, Buf) || !UnscramblePassword(Buf, Result))
    {
      throw Exception(LoadStr(DECRYPT_PASSWORD_ERROR));
    }
    return Result;
  }

private:
  void AskForMasterPassword()
  {
    if (GMasterPasswordPrompt == nullptr)
    {
      throw Exception(LoadStr(DECRYPT_PASSWORD_ERROR));
    }
    bool Incorrect = false;
    for (;;)
    {
      UnicodeString Value;
      if (!GMasterPasswordPrompt(Value, Incorrect))
      {
        Abort();
      }
      // Off (a stray encrypted password, as an imported site's): whatever is
      // given is tried, as the GUI does.
      if (!FUseMasterPassword || FMasterPasswordVerifier.IsEmpty() ||
          AES256Verify(Value, HexToBytes(FMasterPasswordVerifier)))
      {
        FMasterPassword = Value;
        Shred(Value);
        return;
      }
      Shred(Value);
      Incorrect = true;
    }
  }

  bool FUseMasterPassword = false;
  UnicodeString FMasterPasswordVerifier;
  UnicodeString FMasterPassword;
};
}
//---------------------------------------------------------------------------
void SetHeadlessMasterPasswordPrompt(THeadlessMasterPasswordPrompt Prompt)
{
  GMasterPasswordPrompt = Prompt;
}
//---------------------------------------------------------------------------
TConfiguration * __fastcall CreateConfiguration()
{
  return SetUpConfigurationStorage(new THeadlessConfiguration());
}
