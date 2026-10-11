// TStream and its descendants.
//
// Two things get tested hardest because getting them wrong corrupts data
// silently rather than failing:
//
//   Read/Write report how much actually moved; ReadBuffer/WriteBuffer raise on
//   a short transfer. Confusing the two turns truncation into success.
//
//   Seek has two overloads with different origin types but identical values.
//   Both are exercised against the same expectations.
#include "vcl_streams.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

UnicodeString Widen(const std::string & S)
{
  UnicodeString R;
  for (char C : S)
  {
    R += static_cast<wchar_t>(static_cast<unsigned char>(C));
  }
  return R;
}

void TestMemoryStreamBasics()
{
  TMemoryStream M;
  CHECK(M.Size == 0);
  CHECK(M.Position == 0);

  const char Data[] = "hello world";
  CHECK(M.Write(Data, 11) == 11);
  CHECK(M.Size == 11);
  CHECK(M.Position == 11);

  M.Position = 0;
  char Buf[16] = { 0 };
  CHECK(M.Read(Buf, 5) == 5);
  CHECK(std::memcmp(Buf, "hello", 5) == 0);
  CHECK(M.Position == 5);

  // A read past the end returns what is available, not an error.
  std::memset(Buf, 0, sizeof(Buf));
  CHECK(M.Read(Buf, 100) == 6);
  CHECK(std::memcmp(Buf, " world", 6) == 0);
  CHECK(M.Read(Buf, 1) == 0);            // nothing left

  M.Clear();
  CHECK(M.Size == 0 && M.Position == 0);
}

void TestShortTransferVersusAllOrNothing()
{
  TMemoryStream M;
  const char Data[] = "abc";
  M.Write(Data, 3);

  // Read tolerates a short result and reports it.
  M.Position = 0;
  char Buf[8] = { 0 };
  CHECK(M.Read(Buf, 8) == 3);

  // ReadBuffer must raise instead, because core treats it as a guarantee.
  M.Position = 0;
  bool Raised = false;
  try { M.ReadBuffer(Buf, 8); } catch (const EReadError &) { Raised = true; }
  CHECK(Raised);

  // An exact-length ReadBuffer succeeds.
  M.Position = 0;
  M.ReadBuffer(Buf, 3);
  CHECK(std::memcmp(Buf, "abc", 3) == 0);

  // Zero-length transfers are not errors.
  M.Position = 0;
  M.ReadBuffer(Buf, 0);
  M.WriteBuffer(Buf, 0);
}

void TestSeekBothOverloads()
{
  TMemoryStream M;
  const char Data[] = "0123456789";
  M.Write(Data, 10);

  // Modern overload, TSeekOrigin.
  CHECK(M.Seek(static_cast<__int64>(0), soBeginning) == 0);
  CHECK(M.Seek(static_cast<__int64>(3), soBeginning) == 3);
  CHECK(M.Seek(static_cast<__int64>(2), soCurrent) == 5);
  CHECK(M.Seek(static_cast<__int64>(0), soEnd) == 10);
  CHECK(M.Seek(static_cast<__int64>(-4), soEnd) == 6);

  // Legacy overload, Word origin, same values.
  CHECK(M.Seek(0, soFromBeginning) == 0);
  CHECK(M.Seek(3, soFromBeginning) == 3);
  CHECK(M.Seek(2, soFromCurrent) == 5);
  CHECK(M.Seek(0, soFromEnd) == 10);

  // Seeking before the start clamps; seeking past the end is allowed and a
  // later write extends the buffer, as in Delphi.
  CHECK(M.Seek(static_cast<__int64>(-100), soBeginning) == 0);
  CHECK(M.Seek(static_cast<__int64>(20), soBeginning) == 20);
  const char X = 'X';
  CHECK(M.Write(&X, 1) == 1);
  CHECK(M.Size == 21);
}

void TestSetSizeAndPositionInteraction()
{
  TMemoryStream M;
  const char Data[] = "0123456789";
  M.Write(Data, 10);
  CHECK(M.Position == 10);

  // Truncating below Position must pull Position back, or the next read would
  // run off the end of the buffer.
  M.SetSize(4);
  CHECK(M.Size == 4);
  CHECK(M.Position == 4);

  M.SetSize(8);                       // growing leaves Position alone
  CHECK(M.Size == 8);
  CHECK(M.Position == 4);

  M.SetSize(-1);                      // negative means empty
  CHECK(M.Size == 0);
  CHECK(M.Position == 0);
}

void TestCopyFrom()
{
  TMemoryStream Src;
  const char Data[] = "abcdefghij";
  Src.Write(Data, 10);

  // An explicit count copies from the source's current position.
  TMemoryStream Dst;
  Src.Position = 2;
  CHECK(Dst.CopyFrom(&Src, 3) == 3);
  CHECK(Dst.Size == 3);
  Dst.Position = 0;
  char Buf[16] = { 0 };
  Dst.ReadBuffer(Buf, 3);
  CHECK(std::memcmp(Buf, "cde", 3) == 0);

  // Count 0 means the whole source from the start, which is Delphi's rule and
  // not at all the obvious reading.
  TMemoryStream All;
  Src.Position = 5;
  CHECK(All.CopyFrom(&Src, 0) == 10);
  CHECK(All.Size == 10);

  // Asking for more than exists copies what there is rather than raising.
  TMemoryStream Partial;
  Src.Position = 7;
  CHECK(Partial.CopyFrom(&Src, 100) == 3);

  CHECK(Dst.CopyFrom(nullptr, 5) == 0);
}

void TestSaveAndLoad()
{
  TMemoryStream A;
  const char Data[] = "payload";
  A.Write(Data, 7);

  TMemoryStream B;
  A.SaveToStream(&B);
  CHECK(B.Size == 7);

  TMemoryStream C;
  C.LoadFromStream(&B);
  CHECK(C.Size == 7);
  C.Position = 0;
  char Buf[16] = { 0 };
  C.ReadBuffer(Buf, 7);
  CHECK(std::memcmp(Buf, "payload", 7) == 0);

  // LoadFromStream replaces existing content rather than appending.
  C.LoadFromStream(&B);
  CHECK(C.Size == 7);
}

void TestFileStream()
{
  const fs::path Dir = fs::temp_directory_path() / "linscp_stream_test";
  fs::remove_all(Dir);
  fs::create_directories(Dir);
  const std::string Path = (Dir / "f.bin").string();

  {
    TFileStream F(Widen(Path), fmCreate);
    const char Data[] = "file contents";
    F.WriteBuffer(Data, 13);
    CHECK(F.Size == 13);
    CHECK(F.Position == 13);
    CHECK(F.Handle != InvalidHandleValue);
  }

  {
    TFileStream F(Widen(Path), fmOpenRead);
    CHECK(F.Size == 13);
    char Buf[32] = { 0 };
    F.ReadBuffer(Buf, 13);
    CHECK(std::memcmp(Buf, "file contents", 13) == 0);

    F.Position = 5;
    std::memset(Buf, 0, sizeof(Buf));
    CHECK(F.Read(Buf, 3) == 3);
    CHECK(std::memcmp(Buf, "con", 3) == 0);
  }

  // Truncation through the base-class interface.
  {
    TFileStream F(Widen(Path), fmOpenReadWrite);
    F.SetSize(4);
    CHECK(F.Size == 4);
  }
  CHECK(fs::file_size(Path) == 4);

  // Opening a missing file must raise, not yield an unusable stream.
  bool Raised = false;
  try
  {
    TFileStream F(Widen((Dir / "missing.bin").string()), fmOpenRead);
  }
  catch (const EFOpenError &)
  {
    Raised = true;
  }
  CHECK(Raised);

  // And it must be catchable at the level core uses.
  Raised = false;
  try
  {
    TFileStream F(Widen((Dir / "missing.bin").string()), fmOpenRead);
  }
  catch (const EStreamError &)
  {
    Raised = true;
  }
  CHECK(Raised);

  // Round-trip a file through CopyFrom, which is how core moves payloads.
  {
    TFileStream Src(Widen(Path), fmOpenRead);
    TMemoryStream Dst;
    CHECK(Dst.CopyFrom(&Src, 0) == 4);
  }

  std::error_code Ec;
  fs::remove_all(Dir, Ec);
}

void TestHandleStreamOverAFileStreamHandle()
{
  // Core derives TSafeHandleStream from THandleStream, so a bare
  // THandleStream over a live handle must work the same way.
  const fs::path Dir = fs::temp_directory_path() / "linscp_stream_test2";
  fs::remove_all(Dir);
  fs::create_directories(Dir);
  const std::string Path = (Dir / "h.bin").string();

  TFileStream Owner(Widen(Path), fmCreate);
  const char Data[] = "handle";
  Owner.WriteBuffer(Data, 6);

  THandleStream Borrowed(Owner.Handle);
  CHECK(Borrowed.Seek(static_cast<__int64>(0), soBeginning) == 0);
  char Buf[16] = { 0 };
  CHECK(Borrowed.Read(Buf, 6) == 6);
  CHECK(std::memcmp(Buf, "handle", 6) == 0);
  CHECK(Borrowed.Size == 6);

  // An invalid handle must report nothing moved rather than crash.
  THandleStream Bad(InvalidHandleValue);
  CHECK(Bad.Read(Buf, 4) == 0);
  CHECK(Bad.Write(Buf, 4) == 0);
  CHECK(Bad.Seek(static_cast<__int64>(0), soBeginning) == 0);

  std::error_code Ec;
  fs::remove_all(Dir, Ec);
}

void TestTBytesOverloads()
{
  TMemoryStream M;
  TBytes Out;
  Out.Length = 5;
  for (int I = 0; I < 5; ++I) { Out[I] = static_cast<unsigned char>('a' + I); }
  CHECK(M.Write(Out, 1, 3) == 3);                 // "bcd"
  M.Position = 0;
  TBytes In;
  In.Length = 6;
  CHECK(M.Read(In, 2, 3) == 3);
  CHECK((In[2] == 'b') && (In[3] == 'c') && (In[4] == 'd'));
  TStream * Base = &M;                            // through the base, as core calls it
  Base->Position = 0;
  CHECK(Base->Read(In, 0, 1) == 1);
  CHECK(In[0] == 'b');
}

}  // namespace

int main()
{
  TestMemoryStreamBasics();
  TestShortTransferVersusAllOrNothing();
  TestSeekBothOverloads();
  TestSetSizeAndPositionInteraction();
  TestCopyFrom();
  TestSaveAndLoad();
  TestFileStream();
  TestHandleStreamOverAFileStreamHandle();

  TestTBytesOverloads();

  if (Failures == 0)
  {
    std::printf("all stream tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
