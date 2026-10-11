// TMemIniFile and TRegistry.
//
// The INI backend is the one that matters on Linux, and it has to read files
// WinSCP wrote on Windows. So the tests cover what those files actually
// contain: a UTF-8 BOM, mixed-case section and key names, comments, blank
// lines, and values with '=' inside them.
#include "vcl_inifiles.h"
#include "vcl_registry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
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

UnicodeString U(const wchar_t * S) { return UnicodeString(S); }

UnicodeString Widen(const std::string & S)
{
  UnicodeString R;
  for (char C : S)
  {
    R += static_cast<wchar_t>(static_cast<unsigned char>(C));
  }
  return R;
}

fs::path Dir()
{
  static const fs::path D = fs::temp_directory_path() / "linscp_storage_test";
  return D;
}

void Write(const std::string & Name, const std::string & Content)
{
  fs::create_directories(Dir());
  std::ofstream F(Dir() / Name, std::ios::binary);
  F << Content;
}

void TestReadWriteRoundTrip()
{
  const std::string Path = (Dir() / "rw.ini").string();
  fs::remove(Path);
  {
    TMemIniFile Ini(Widen(Path));
    CHECK(Ini.FileName == Widen(Path));
    // A missing file reads as empty rather than raising.
    CHECK(Ini.ReadString(U(L"S"), U(L"K"), U(L"fallback")) == U(L"fallback"));
    CHECK(!Ini.SectionExists(U(L"S")));

    Ini.WriteString(U(L"Session"), U(L"HostName"), U(L"example.com"));
    Ini.WriteInteger(U(L"Session"), U(L"PortNumber"), 2222);
    Ini.WriteBool(U(L"Session"), U(L"Compression"), true);
    Ini.WriteBool(U(L"Session"), U(L"Tunnel"), false);
    Ini.WriteInt64(U(L"Session"), U(L"Big"), 9000000000LL);
    Ini.UpdateFile();
  }
  {
    TMemIniFile Ini(Widen(Path));
    CHECK(Ini.ReadString(U(L"Session"), U(L"HostName"), U(L"")) ==
          U(L"example.com"));
    CHECK(Ini.ReadInteger(U(L"Session"), U(L"PortNumber"), 0) == 2222);
    CHECK(Ini.ReadBool(U(L"Session"), U(L"Compression"), false));
    CHECK(!Ini.ReadBool(U(L"Session"), U(L"Tunnel"), true));
    CHECK(Ini.ReadInt64(U(L"Session"), U(L"Big"), 0) == 9000000000LL);

    // Defaults apply only to absent keys.
    CHECK(Ini.ReadInteger(U(L"Session"), U(L"Absent"), 42) == 42);
    CHECK(Ini.ReadBool(U(L"Session"), U(L"Absent"), true));
  }
}

void TestCaseInsensitivity()
{
  // Delphi matches section and key names without regard to case, and settings
  // written on Windows will have inconsistent casing.
  Write("case.ini", "[Session]\nHostName=host\n");
  TMemIniFile Ini(Widen((Dir() / "case.ini").string()));
  CHECK(Ini.ReadString(U(L"session"), U(L"hostname"), U(L"")) == U(L"host"));
  CHECK(Ini.ReadString(U(L"SESSION"), U(L"HOSTNAME"), U(L"")) == U(L"host"));
  CHECK(Ini.SectionExists(U(L"sEsSiOn")));
  CHECK(Ini.ValueExists(U(L"session"), U(L"HostName")));

  // Writing with different casing updates the existing key rather than
  // adding a second one.
  Ini.WriteString(U(L"SESSION"), U(L"hostname"), U(L"other"));
  TStringList Keys;
  Ini.ReadSection(U(L"Session"), &Keys);
  CHECK(Keys.Count == 1);
  CHECK(Ini.ReadString(U(L"Session"), U(L"HostName"), U(L"")) == U(L"other"));
}

void TestRealWorldFileShapes()
{
  // A UTF-8 BOM, comments, blank lines, indentation, a value containing '=',
  // and a duplicate key. All of these turn up in real configuration files.
  Write("shapes.ini",
        "\xEF\xBB\xBF; a comment\n"
        "# another comment\n"
        "\n"
        "[Alpha]\n"
        "  Key1 = value one  \n"
        "Key2=a=b=c\n"
        "Key3=\n"
        "\n"
        "[Beta]\n"
        "Dup=first\n"
        "Dup=second\n"
        "NoSeparator\n");
  TMemIniFile Ini(Widen((Dir() / "shapes.ini").string()));

  // The BOM must not become part of the first section's name.
  CHECK(Ini.SectionExists(U(L"Alpha")));
  CHECK(Ini.SectionExists(U(L"Beta")));

  CHECK(Ini.ReadString(U(L"Alpha"), U(L"Key1"), U(L"")) == U(L"value one"));
  // Only the first '=' separates; the rest belongs to the value.
  CHECK(Ini.ReadString(U(L"Alpha"), U(L"Key2"), U(L"")) == U(L"a=b=c"));
  // An empty value is present, and must not fall back to the default.
  CHECK(Ini.ValueExists(U(L"Alpha"), U(L"Key3")));
  CHECK(Ini.ReadString(U(L"Alpha"), U(L"Key3"), U(L"dflt")).IsEmpty());

  CHECK(Ini.ReadString(U(L"Beta"), U(L"Dup"), U(L"")) == U(L"second"));
  CHECK(!Ini.ValueExists(U(L"Beta"), U(L"NoSeparator")));
  CHECK(!Ini.ValueExists(U(L"Alpha"), U(L"; a comment")));
}

void TestSectionsAndDeletion()
{
  const std::string Path = (Dir() / "sections.ini").string();
  fs::remove(Path);
  TMemIniFile Ini(Widen(Path));
  Ini.WriteString(U(L"A"), U(L"k1"), U(L"v1"));
  Ini.WriteString(U(L"A"), U(L"k2"), U(L"v2"));
  Ini.WriteString(U(L"B"), U(L"k3"), U(L"v3"));

  TStringList Sections;
  Ini.ReadSections(&Sections);
  CHECK(Sections.Count == 2);
  CHECK(Sections.IndexOf(U(L"A")) >= 0 && Sections.IndexOf(U(L"B")) >= 0);

  TStringList Keys;
  Ini.ReadSection(U(L"A"), &Keys);
  CHECK(Keys.Count == 2);
  CHECK(Keys.Strings[0] == U(L"k1"));          // insertion order preserved

  // ReadSectionValues yields "key=value", which makes TStrings::Values work
  // on the result -- that is what core relies on.
  TStringList Values;
  Ini.ReadSectionValues(U(L"A"), &Values);
  CHECK(Values.Count == 2);
  CHECK(Values.Values[U(L"k1")] == U(L"v1"));

  Ini.DeleteKey(U(L"A"), U(L"k1"));
  CHECK(!Ini.ValueExists(U(L"A"), U(L"k1")));
  CHECK(Ini.ValueExists(U(L"A"), U(L"k2")));

  Ini.EraseSection(U(L"A"));
  CHECK(!Ini.SectionExists(U(L"A")));
  CHECK(Ini.SectionExists(U(L"B")));

  // Deleting what is not there must be a no-op, not a crash.
  Ini.DeleteKey(U(L"Nope"), U(L"k"));
  Ini.EraseSection(U(L"Nope"));

  Ini.Clear();
  Ini.ReadSections(&Sections);
  CHECK(Sections.Count == 0);
}

void TestOrderIsPreservedAcrossSaves()
{
  // Saving must not reshuffle a file, or every write would churn the user's
  // configuration and make diffs useless.
  const std::string Path = (Dir() / "order.ini").string();
  fs::remove(Path);
  {
    TMemIniFile Ini(Widen(Path));
    Ini.WriteString(U(L"Z"), U(L"c"), U(L"3"));
    Ini.WriteString(U(L"Z"), U(L"a"), U(L"1"));
    Ini.WriteString(U(L"A"), U(L"b"), U(L"2"));
    Ini.UpdateFile();
  }
  {
    TMemIniFile Ini(Widen(Path));
    TStringList Sections;
    Ini.ReadSections(&Sections);
    CHECK(Sections.Strings[0] == U(L"Z"));     // not sorted
    CHECK(Sections.Strings[1] == U(L"A"));
    TStringList Keys;
    Ini.ReadSection(U(L"Z"), &Keys);
    CHECK(Keys.Strings[0] == U(L"c"));
    CHECK(Keys.Strings[1] == U(L"a"));
  }
}

void TestRename()
{
  const std::string A = (Dir() / "ren_a.ini").string();
  const std::string B = (Dir() / "ren_b.ini").string();
  fs::remove(A);
  fs::remove(B);
  Write("ren_b.ini", "[S]\nK=fromB\n");

  TMemIniFile Ini(Widen(A));
  Ini.WriteString(U(L"S"), U(L"K"), U(L"fromA"));

  // Renaming without reloading keeps the in-memory content.
  Ini.Rename(Widen(B), false);
  CHECK(Ini.FileName == Widen(B));
  CHECK(Ini.ReadString(U(L"S"), U(L"K"), U(L"")) == U(L"fromA"));

  // Renaming with a reload replaces it from the new file.
  Ini.Rename(Widen(B), true);
  CHECK(Ini.ReadString(U(L"S"), U(L"K"), U(L"")) == U(L"fromB"));
}

void TestRegistry()
{
  TRegistry R;
  CHECK(R.RootKey == HKEY_CURRENT_USER);
  R.Access = KEY_READ;
  CHECK(R.Access == KEY_READ);

#if defined(_WIN32)
  // Under Wine a real registry exists, so a round trip is checkable.
  R.RootKey = HKEY_CURRENT_USER;
  R.Access = KEY_ALL_ACCESS;
  CHECK(R.OpenKey(U(L"Software\\linscp_test"), true));
  R.WriteString(U(L"S"), U(L"value"));
  R.WriteInteger(U(L"I"), 7);
  R.WriteBool(U(L"B"), true);
  CHECK(R.ValueExists(U(L"S")));
  CHECK(R.ReadString(U(L"S")) == U(L"value"));
  CHECK(R.ReadInteger(U(L"I")) == 7);
  CHECK(R.ReadBool(U(L"B")));
  CHECK(R.GetDataType(U(L"S")) == rdString);
  CHECK(R.GetDataType(U(L"I")) == rdInteger);
  TStringList Names;
  R.GetValueNames(&Names);
  CHECK(Names.Count >= 3);
  CHECK(R.DeleteValue(U(L"S")));
  CHECK(!R.ValueExists(U(L"S")));
  R.CloseKey();
  CHECK(R.DeleteKey(U(L"Software\\linscp_test")));
#else
  // On POSIX every key must report absent rather than empty. Returning
  // defaults for settings a user had actually set would look to them like
  // their configuration had vanished.
  CHECK(!R.OpenKey(U(L"Software\\Martin Prikryl"), false));
  CHECK(!R.OpenKey(U(L"Software\\Martin Prikryl"), true));
  CHECK(!R.OpenKeyReadOnly(U(L"Software\\Martin Prikryl")));
  CHECK(!R.KeyExists(U(L"Software")));
  CHECK(!R.ValueExists(U(L"Anything")));
  CHECK(R.GetDataSize(U(L"Anything")) == -1);
  CHECK(R.GetDataType(U(L"Anything")) == rdUnknown);
  CHECK(R.ReadString(U(L"Anything")).IsEmpty());
  CHECK(R.ReadInteger(U(L"Anything")) == 0);
  CHECK(!R.ReadBool(U(L"Anything")));

  TStringList Names;
  Names.Add(U(L"stale"));
  R.GetValueNames(&Names);
  CHECK(Names.Count == 0);          // cleared, not left stale
  R.CloseKey();                     // safe with nothing open
#endif
}

}  // namespace

int main()
{
  fs::remove_all(Dir());
  fs::create_directories(Dir());

  TestReadWriteRoundTrip();
  TestCaseInsensitivity();
  TestRealWorldFileShapes();
  TestSectionsAndDeletion();
  TestOrderIsPreservedAcrossSaves();
  TestRename();
  TestRegistry();

  std::error_code Ec;
  fs::remove_all(Dir(), Ec);

  if (Failures == 0)
  {
    std::printf("all storage tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
