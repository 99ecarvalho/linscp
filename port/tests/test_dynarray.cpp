// System::DynamicArray and Delphi's handle-based file API.
//
// The array's sharing behaviour is the load-bearing part. Core declares
// TSafeHandleStream::Read as taking a DynamicArray<Byte> *by value* and fills
// it for the caller, which only works because Delphi dynamic arrays are
// reference-counted. A deep-copying implementation would compile perfectly and
// then silently return no data.
#include "vcl_dynamicarray.h"
#include "vcl_sysutils.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

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

// Mirrors core's signature: taken by value, filled for the caller.
void FillByValue(TBytes Buffer)
{
  for (int I = 0; I < Buffer.Length; ++I)
  {
    Buffer[I] = static_cast<System::Byte>(I + 1);
  }
}

void TestLengthAndIndexing()
{
  TBytes A;
  CHECK(A.Length == 0);
  CHECK(A.IsEmpty());
  CHECK(A.data() == nullptr);

  A.set_length(4);
  CHECK(A.Length == 4);
  CHECK(!A.IsEmpty());
  CHECK(A.data() != nullptr);

  // 0-based, unlike UnicodeString in the same codebase.
  A[0] = 10;
  A[3] = 40;
  CHECK(A[0] == 10);
  CHECK(A[3] == 40);

  A.set_length(2);
  CHECK(A.Length == 2);
  CHECK(A[0] == 10);              // surviving elements keep their values

  A.set_length(0);
  CHECK(A.IsEmpty());
  A.set_length(-5);               // negative means empty, not a crash
  CHECK(A.IsEmpty());

  TBytes B(3);
  CHECK(B.Length == 3);

  // The write form, which is how core resizes: `Buffer.Length = N`.
  TBytes C;
  C.Length = 5;
  CHECK(C.Length == 5);
  C[4] = 9;
  CHECK(C[4] == 9);
  C.Length = 2;
  CHECK(C.Length == 2);
  C.Length = 0;
  CHECK(C.IsEmpty());
}

void TestSharesRatherThanCopies()
{
  TBytes A(3);
  A[0] = 1;

  // A copy shares the buffer: this is what makes core's by-value Read work.
  TBytes B = A;
  B[0] = 99;
  CHECK(A[0] == 99);

  TBytes C;
  C = A;
  C[1] = 7;
  CHECK(A[1] == 7);

  // And through a by-value parameter, exactly as core uses it.
  TBytes D(4);
  FillByValue(D);
  CHECK(D[0] == 1);
  CHECK(D[3] == 4);
}

void TestFileApi()
{
  const fs::path Dir = fs::temp_directory_path() / "linscp_dynarray_test";
  fs::remove_all(Dir);
  fs::create_directories(Dir);
  const std::string Path = (Dir / "f.bin").string();

  // Create and write through the raw-pointer overload.
  THandle H = FileCreate(Widen(Path));
  CHECK(H != InvalidHandleValue);
  const char Data[] = "0123456789";
  CHECK(FileWrite(H, Data, 10) == 10);
  FileClose(H);
  CHECK(fs::file_size(Path) == 10);

  // Read back, and check the handle stayed usable across calls -- the
  // wrappers borrow it in a temporary stream, so a missing detach would close
  // it after the first use.
  H = FileOpen(Widen(Path), fmOpenRead);
  CHECK(H != InvalidHandleValue);
  char Buf[16] = { 0 };
  CHECK(FileRead(H, Buf, 4) == 4);
  CHECK(std::memcmp(Buf, "0123", 4) == 0);
  CHECK(FileRead(H, Buf, 3) == 3);
  CHECK(std::memcmp(Buf, "456", 3) == 0);
  CHECK(FileSeek(H, 0, 0) == 0);
  CHECK(FileRead(H, Buf, 2) == 2);
  CHECK(std::memcmp(Buf, "01", 2) == 0);
  FileClose(H);

  // The array overloads, with an offset into the buffer.
  H = FileOpen(Widen(Path), fmOpenRead);
  TBytes Array(10);
  for (int I = 0; I < 10; ++I)
  {
    Array[I] = 0xFF;
  }
  CHECK(FileRead(H, Array, 2, 4) == 4);
  CHECK(Array[0] == 0xFF);        // before the offset, untouched
  CHECK(Array[1] == 0xFF);
  CHECK(Array[2] == '0');         // filled from the offset
  CHECK(Array[5] == '3');
  CHECK(Array[6] == 0xFF);        // past the count, untouched
  FileClose(H);

  // Out-of-range offsets and counts must refuse rather than overrun.
  H = FileOpen(Widen(Path), fmOpenRead);
  CHECK(FileRead(H, Array, 8, 5) == 0);    // offset + count past the end
  CHECK(FileRead(H, Array, -1, 2) == 0);
  CHECK(FileRead(H, Array, 0, 0) == 0);
  CHECK(FileWrite(H, Array, 20, 1) == 0);
  FileClose(H);

  // Opening a missing file reports an invalid handle rather than raising,
  // which is what the handle-based API promises.
  CHECK(FileOpen(Widen((Dir / "nope.bin").string()), fmOpenRead) ==
        InvalidHandleValue);

  FileClose(InvalidHandleValue);           // must be safe

  std::error_code Ec;
  fs::remove_all(Dir, Ec);
}

}  // namespace

int main()
{
  TestLengthAndIndexing();
  TestSharesRatherThanCopies();
  TestFileApi();

  if (Failures == 0)
  {
    std::printf("all dynamic-array/file-api tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
