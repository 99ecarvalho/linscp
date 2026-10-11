// A first real session: core connects to an SSH server over SFTP, lists a
// directory and downloads a file.
//
// Driven by run_sftp_session.py, which starts a throwaway sshd on loopback
// with its own host key and a fresh client key, and prepares the remote
// directory. Arguments:
//   host port user openssh-key host-key-fingerprint remote-dir local-dir
// The key arrives in OpenSSH format and core itself converts it to PuTTY's
// (LoadKey/SaveKey), as WinSCP's key import does.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>
#include <memory>

#include <Common.h>
#include <Configuration.h>
#include <CopyParam.h>
#include <CoreMain.h>
#include <PuttyTools.h>
#include <RemoteFiles.h>
#include <SessionData.h>
#include <Terminal.h>
#include "HeadlessMain.h"

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

UnicodeString Arg(int Index)
{
  return ParamStr(Index);
}

}  // namespace

int main()
{
  if (ParamCount() < 7)
  {
    std::printf("usage: test_sftp_session host port user key hostkey remotedir localdir\n");
    return 2;
  }
  const UnicodeString Host = Arg(1);
  const int Port = StrToInt(Arg(2));
  const UnicodeString User = Arg(3);
  const UnicodeString OpenSshKey = Arg(4);
  const UnicodeString HostKey = Arg(5);
  const UnicodeString RemoteDir = Arg(6);
  const UnicodeString LocalDir = Arg(7);

  try
  {
    HeadlessInitialize();
    {
      const UnicodeString PuttyKey = IncludeTrailingBackslash(LocalDir) + L"client.ppk";
      TPrivateKey * Key = LoadKey(KeyType(OpenSshKey), OpenSshKey, UnicodeString());
      SaveKey(ktSSH2, PuttyKey, UnicodeString(), Key);
      FreeKey(Key);
      CHECK(KeyType(PuttyKey) == ktSSH2);

      std::unique_ptr<TSessionData> Data(new TSessionData(L""));
      Data->HostName = Host;
      Data->PortNumber = Port;
      Data->UserName = User;
      Data->PublicKeyFile = PuttyKey;
      Data->HostKey = HostKey;
      Data->FSProtocol = fsSFTPonly;

      std::unique_ptr<TTerminal> Terminal(new TTerminal(Data.get(), Configuration));
      Terminal->Open();
      CHECK(Terminal->Active);
      std::printf("connected: %s\n", UTF8String(Terminal->GetSessionInfo().ProtocolName).c_str());

      std::unique_ptr<TRemoteFileList> List(
        Terminal->ReadDirectoryListing(RemoteDir, TFileMasks()));
      CHECK(List.get() != NULL);
      bool SawSub = false;
      TRemoteFile * Hello = NULL;
      for (int Index = 0; (List.get() != NULL) && (Index < List->Count); Index++)
      {
        TRemoteFile * File = List->Files[Index];
        std::printf("  %s%s %lld\n", UTF8String(File->FileName).c_str(),
          File->IsDirectory ? "/" : "",
          static_cast<long long>(File->Size));
        if (File->FileName == L"hello.txt")
        {
          Hello = File;
        }
        if ((File->FileName == L"sub") && File->IsDirectory)
        {
          SawSub = true;
        }
      }
      CHECK(SawSub);
      CHECK((Hello != NULL) && (Hello->Size == 14));

      // As WinSCP's callers do: each name carries its TRemoteFile.
      std::unique_ptr<TStrings> Files(new TStringList());
      if (Hello != NULL)
      {
        Files->AddObject(UnixIncludeTrailingBackslash(RemoteDir) + L"hello.txt", Hello);
      }
      TCopyParamType CopyParam;
      CHECK(Terminal->CopyToLocal(Files.get(), IncludeTrailingBackslash(LocalDir),
        &CopyParam, 0, NULL));

      std::unique_ptr<TFileStream> Local(new TFileStream(
        IncludeTrailingBackslash(LocalDir) + L"hello.txt", fmOpenRead));
      char Buffer[64] = {};
      const int Read = Local->Read(Buffer, sizeof(Buffer));
      CHECK(Read == 14);
      CHECK(std::string(Buffer, Read > 0 ? Read : 0) == "Hello, linscp\n");

      Terminal->Close();
      CHECK(!Terminal->Active);
    }
    HeadlessFinalize();
  }
  catch (Exception & E)
  {
    std::printf("FAIL exception: %s\n", UTF8String(E.Message).c_str());
    ExtException * EE = dynamic_cast<ExtException *>(&E);
    if ((EE != NULL) && (EE->MoreMessages != NULL))
    {
      std::printf("%s\n", UTF8String(EE->MoreMessages->Text).c_str());
    }
    ++Failures;
  }
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
