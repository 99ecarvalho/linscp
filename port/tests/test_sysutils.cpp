// FindFirst/FindNext and friends.
//
// This is the first shim code with two genuinely different implementations
// (Win32 FindFirstFileW, and dirent + fnmatch on POSIX), so the tests assert
// behaviour rather than implementation and run on both targets.
#include "vcl_sysutils.h"
#include "vcl_strutils.h"

#include <vector>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
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

std::string Narrow(const UnicodeString & S)
{
  std::string R;
  for (int I = 1; I <= S.Length(); ++I)
  {
    R += static_cast<char>(static_cast<unsigned char>(S[I] & 0xFF));
  }
  return R;
}

UnicodeString Widen(const std::string & S)
{
  UnicodeString R;
  for (char C : S)
  {
    R += static_cast<wchar_t>(static_cast<unsigned char>(C));
  }
  return R;
}

// Collect every match for a pattern into a set of names.
std::set<std::string> Enumerate(const std::string & Pattern, int Attr)
{
  std::set<std::string> Names;
  TSearchRec F;
  if (FindFirst(Widen(Pattern), Attr, F) == 0)
  {
    do
    {
      Names.insert(Narrow(F.Name));
    }
    while (FindNext(F) == 0);
  }
  FindClose(F);
  return Names;
}

fs::path MakeFixture()
{
  const fs::path Root = fs::temp_directory_path() / "linscp_sysutils_test";
  fs::remove_all(Root);
  fs::create_directories(Root / "subdir");
  for (const char * Name : { "alpha.txt", "beta.txt", "gamma.dat" })
  {
    std::ofstream F(Root / Name, std::ios::binary);
    F << "0123456789";      // 10 bytes, so Size is checkable
  }
  return Root;
}

void TestEnumeration(const fs::path & Root)
{
  const std::string Base = Root.string();

  // Pattern matching, and the extension must actually filter.
  const auto Txt = Enumerate(Base + "/*.txt", faAnyFile);
  CHECK(Txt.size() == 2);
  CHECK(Txt.count("alpha.txt") == 1);
  CHECK(Txt.count("beta.txt") == 1);
  CHECK(Txt.count("gamma.dat") == 0);

  const auto Dat = Enumerate(Base + "/*.dat", faAnyFile);
  CHECK(Dat.size() == 1 && Dat.count("gamma.dat") == 1);

  // An exact name is just a pattern with no wildcard.
  const auto One = Enumerate(Base + "/alpha.txt", faAnyFile);
  CHECK(One.size() == 1 && One.count("alpha.txt") == 1);

  // No match must report failure, not an empty success.
  TSearchRec F;
  CHECK(FindFirst(Widen(Base + "/*.nothing"), faAnyFile, F) != 0);
  FindClose(F);

  // A missing directory likewise.
  TSearchRec G;
  CHECK(FindFirst(Widen(Base + "/no_such_dir/*"), faAnyFile, G) != 0);
  FindClose(G);
}

void TestDirectoryAttribute(const fs::path & Root)
{
  const std::string Base = Root.string();

  // Without faDirectory, directories must be excluded.
  const auto FilesOnly = Enumerate(Base + "/*", faNormal);
  CHECK(FilesOnly.count("subdir") == 0);
  CHECK(FilesOnly.count("alpha.txt") == 1);

  // With faAnyFile, the subdirectory shows up and is flagged.
  bool SawSubdir = false;
  bool SubdirFlagged = false;
  bool FileNotFlagged = true;
  TSearchRec F;
  if (FindFirst(Widen(Base + "/*"), faAnyFile, F) == 0)
  {
    do
    {
      const std::string N = Narrow(F.Name);
      if (N == "subdir")
      {
        SawSubdir = true;
        SubdirFlagged = (F.Attr & faDirectory) != 0;
      }
      else if (N == "alpha.txt")
      {
        FileNotFlagged = (F.Attr & faDirectory) == 0;
      }
    }
    while (FindNext(F) == 0);
  }
  FindClose(F);
  CHECK(SawSubdir);
  CHECK(SubdirFlagged);
  CHECK(FileNotFlagged);
}

void TestSizeAndTimestamp(const fs::path & Root)
{
  TSearchRec F;
  CHECK(FindFirst(Widen((Root / "alpha.txt").string()), faAnyFile, F) == 0);
  CHECK(F.Size == 10);

  // The fixture was written moments ago, so its timestamp must be close to now
  // and definitely not the zero serial (1899-12-30), which is what a failed
  // conversion would leave behind.
  Word Y = 0, M = 0, D = 0;
  DecodeDate(F.TimeStamp, Y, M, D);
  CHECK(Y >= 2024 && Y < 2100);
  CHECK(SecondsBetween(F.TimeStamp, Now()) < 600);
  FindClose(F);
}

void TestFindCloseIsIdempotent(const fs::path & Root)
{
  // Core calls FindClose on records that may never have been opened, and on
  // paths where FindFirst already failed. Neither may crash or double-free.
  TSearchRec F;
  FindClose(F);
  FindClose(F);

  TSearchRec G;
  CHECK(FindFirst(Widen((Root / "*.nothing").string()), faAnyFile, G) != 0);
  FindClose(G);
  FindClose(G);

  TSearchRec H;
  CHECK(FindFirst(Widen((Root / "*.txt").string()), faAnyFile, H) == 0);
  FindClose(H);
  FindClose(H);

  // Reusing a record for a second search must not leak the first.
  TSearchRec R;
  CHECK(FindFirst(Widen((Root / "*.txt").string()), faAnyFile, R) == 0);
  CHECK(FindFirst(Widen((Root / "*.dat").string()), faAnyFile, R) == 0);
  CHECK(Narrow(R.Name) == "gamma.dat");
  FindClose(R);
}

void TestFileAndDirectoryExists(const fs::path & Root)
{
  CHECK(FileExists(Widen((Root / "alpha.txt").string())));
  CHECK(!FileExists(Widen((Root / "nope.txt").string())));
  CHECK(!FileExists(Widen(Root.string())));              // a directory is not a file

  CHECK(DirectoryExists(Widen(Root.string())));
  CHECK(DirectoryExists(Widen((Root / "subdir").string())));
  CHECK(!DirectoryExists(Widen((Root / "alpha.txt").string())));
  CHECK(!DirectoryExists(Widen((Root / "nope").string())));
}

void TestMiscellany()
{
  CHECK(EmptyStr.IsEmpty());
  CHECK(System::Types::dupIgnore == dupIgnore);
  CHECK(static_cast<int>(faDirectory) == 0x10);   // must match Win32's value
  CHECK(static_cast<int>(faReadOnly) == 0x01);
}

bool EndsWithText(const UnicodeString & S, const UnicodeString & Tail)
{
  return (S.Length() >= Tail.Length()) &&
         SameText(S.SubString(S.Length() - Tail.Length() + 1), Tail);
}

void TestParamStr()
{
  // ctest runs us with no arguments.
  const UnicodeString Self = ParamStr(0);
#if defined(_WIN32)
  CHECK(EndsWithText(Self, L"test_sysutils.exe"));
  CHECK((Self.Length() > 3) && (Self[2] == L':'));   // a full path
#else
  CHECK(EndsWithText(Self, L"test_sysutils"));
  CHECK((Self.Length() > 0) && (Self[1] == L'/'));
#endif
  CHECK(ParamCount() == 0);
  CHECK(ParamStr(1).IsEmpty());
  CHECK(ParamStr(-1).IsEmpty());
}

bool Split(const wchar_t * Line, std::vector<UnicodeString> Expected)
{
  const std::vector<UnicodeString> Got = linscp::SplitDelphiCommandLine(Line);
  if (Got.size() != Expected.size())
  {
    return false;
  }
  for (std::size_t I = 0; I < Got.size(); ++I)
  {
    if (Got[I] != Expected[I])
    {
      return false;
    }
  }
  return true;
}

void TestDelphiCommandLineSplitting()
{
  CHECK(Split(L"prog a b", {L"prog", L"a", L"b"}));
  CHECK(Split(L"\"C:\\Program Files\\x.exe\" \"a b\" c",
              {L"C:\\Program Files\\x.exe", L"a b", L"c"}));
  CHECK(Split(L"prog a\"b c\"d", {L"prog", L"ab cd"}));    // quotes join, are dropped
  CHECK(Split(L"prog \"\" x", {L"prog", L"x"}));            // empty "" skipped
  CHECK(Split(L"prog a\\\"b", {L"prog", L"a\\b"}));        // no backslash escape
  CHECK(Split(L"prog\ta   ", {L"prog", L"a"}));             // tab; trailing space
  CHECK(Split(L"", {}));
}

}  // namespace

int main()
{
  const fs::path Root = MakeFixture();

  TestEnumeration(Root);
  TestDirectoryAttribute(Root);
  TestSizeAndTimestamp(Root);
  TestFindCloseIsIdempotent(Root);
  TestFileAndDirectoryExists(Root);
  TestMiscellany();

  std::error_code Ec;
  fs::remove_all(Root, Ec);

  TestParamStr();
  TestDelphiCommandLineSplitting();

  if (Failures == 0)
  {
    std::printf("all sysutils tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
