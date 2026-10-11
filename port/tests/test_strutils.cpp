// Free string, number, path and Format routines.
//
// Delphi's conventions are the thing under test: 1-based positions with 0 for
// absent, the *Text (case-insensitive) versus *Str (case-sensitive) split, and
// Format's own specifier grammar, which is not printf's.
#include "vcl_format.h"
#include "vcl_strutils.h"
#include "vcl_sysutils.h"

#include <cstdio>

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

bool FmtIs(const wchar_t * Fmt, const TFormatArgs & Args, const wchar_t * Want)
{
  const UnicodeString Got = Format(U(Fmt), Args);
  if (Got != U(Want))
  {
    std::printf("  Format(\"%ls\") gave \"%ls\", wanted \"%ls\"\n",
                Fmt, Got.c_str(), Want);
    return false;
  }
  return true;
}

void TestNumbers()
{
  CHECK(IntToStr(0) == U(L"0"));
  CHECK(IntToStr(-42) == U(L"-42"));
  CHECK(IntToStr(9223372036854775807LL) == U(L"9223372036854775807"));
  CHECK(IntToHex(255, 4) == U(L"00FF"));
  CHECK(IntToHex(255, 0) == U(L"FF"));

  int V = 0;
  CHECK(TryStrToInt(U(L"123"), V) && V == 123);
  CHECK(TryStrToInt(U(L"-7"), V) && V == -7);
  CHECK(TryStrToInt(U(L"  42  "), V) && V == 42);     // surrounding space is ok
  CHECK(TryStrToInt(U(L"$FF"), V) && V == 255);        // Delphi's hex prefix
  CHECK(TryStrToInt(U(L"0x10"), V) && V == 16);

  CHECK(!TryStrToInt(U(L""), V));
  CHECK(!TryStrToInt(U(L"abc"), V));
  CHECK(!TryStrToInt(U(L"12abc"), V));                 // trailing garbage fails
  CHECK(!TryStrToInt(U(L"1 2"), V));

  CHECK(StrToIntDef(U(L"5"), 99) == 5);
  CHECK(StrToIntDef(U(L"junk"), 99) == 99);
  CHECK(StrToInt(U(L"8")) == 8);

  bool Raised = false;
  try { StrToInt(U(L"junk")); } catch (const EConvertError &) { Raised = true; }
  CHECK(Raised);
}

void TestTextVersusStr()
{
  // Delphi's naming rule: *Text ignores case, *Str does not. Core depends on it.
  CHECK(SameText(U(L"Abc"), U(L"aBC")));
  CHECK(!SameStr(U(L"Abc"), U(L"aBC")));
  CHECK(SameStr(U(L"Abc"), U(L"Abc")));
  CHECK(CompareText(U(L"a"), U(L"A")) == 0);
  CHECK(CompareStr(U(L"a"), U(L"A")) != 0);

  CHECK(StartsText(U(L"AB"), U(L"abcdef")));
  CHECK(!StartsStr(U(L"AB"), U(L"abcdef")));
  CHECK(StartsStr(U(L"ab"), U(L"abcdef")));
  CHECK(EndsText(U(L"EF"), U(L"abcdef")));
  CHECK(!EndsStr(U(L"EF"), U(L"abcdef")));
  CHECK(EndsStr(U(L"ef"), U(L"abcdef")));
  CHECK(ContainsText(U(L"abcdef"), U(L"CD")));
  CHECK(!ContainsStr(U(L"abcdef"), U(L"CD")));

  // A prefix longer than the string is not a match, and must not read past it.
  CHECK(!StartsStr(U(L"abcdefgh"), U(L"abc")));
  CHECK(!EndsStr(U(L"abcdefgh"), U(L"abc")));
  CHECK(StartsStr(U(L""), U(L"abc")));            // empty prefix always matches
}

void TestSearching()
{
  CHECK(Pos(U(L"cd"), U(L"abcdef")) == 3);        // 1-based
  CHECK(Pos(U(L"zz"), U(L"abcdef")) == 0);        // 0 for absent
  CHECK(PosEx(U(L"a"), U(L"banana"), 1) == 2);
  CHECK(PosEx(U(L"a"), U(L"banana"), 3) == 4);    // offset is also 1-based
  CHECK(PosEx(U(L"a"), U(L"banana"), 5) == 6);
  CHECK(PosEx(U(L"a"), U(L"banana"), 7) == 0);    // offset past the end
  CHECK(PosEx(U(L"a"), U(L"banana"), 0) == 2);    // offset below 1 clamps
}

void TestTransformation()
{
  CHECK(Trim(U(L"  x  ")) == U(L"x"));
  CHECK(LowerCase(U(L"AbC")) == U(L"abc"));
  CHECK(UpperCase(U(L"AbC")) == U(L"ABC"));

  CHECK(ReplaceStr(U(L"a.b.c"), U(L"."), U(L"/")) == U(L"a/b/c"));
  CHECK(ReplaceStr(U(L"aaa"), U(L"aa"), U(L"b")) == U(L"ba"));  // non-overlapping
  CHECK(ReplaceStr(U(L"abc"), U(L"x"), U(L"y")) == U(L"abc"));
  CHECK(ReplaceStr(U(L"abc"), U(L""), U(L"y")) == U(L"abc"));   // empty needle
  CHECK(ReplaceStr(U(L"a.b"), U(L"."), U(L"")) == U(L"ab"));    // delete
  CHECK(ReplaceText(U(L"A.b"), U(L"a"), U(L"z")) == U(L"z.b")); // ignores case

  CHECK(StringOfChar(L'x', 3) == U(L"xxx"));
  CHECK(StringOfChar(L'x', 0).IsEmpty());
  CHECK(DupeString(U(L"ab"), 2) == U(L"abab"));
}

void TestPaths()
{
  CHECK(ExtractFileName(U(L"/a/b/c.txt")) == U(L"c.txt"));
  CHECK(ExtractFileName(U(L"c.txt")) == U(L"c.txt"));
  CHECK(ExtractFileName(U(L"/a/b/")).IsEmpty());
  CHECK(ExtractFileName(U(L"C:\\a\\b.txt")) == U(L"b.txt"));   // both separators

  CHECK(ExtractFilePath(U(L"/a/b/c.txt")) == U(L"/a/b/"));
  CHECK(ExtractFilePath(U(L"c.txt")).IsEmpty());
  CHECK(ExtractFileDir(U(L"/a/b/c.txt")) == U(L"/a/b"));
  CHECK(ExtractFileDir(U(L"/c.txt")) == U(L"/"));             // root keeps its slash
  CHECK(ExtractFileDir(U(L"C:\\c.txt")) == U(L"C:\\"));       // so does a drive
  CHECK(ExtractFileDir(U(L"C:b.txt")) == U(L"C:"));         // drive-relative
  CHECK(ExtractFileDir(U(L"a\\b")) == U(L"a"));             // ordinary: separator dropped

  CHECK(ExtractFileExt(U(L"a/b.tar.gz")) == U(L".gz"));
  CHECK(ExtractFileExt(U(L"a/b")).IsEmpty());
  CHECK(ExtractFileExt(U(L"a.d/b")).IsEmpty());               // dot in a directory
  CHECK(ChangeFileExt(U(L"a/b.txt"), U(L".bak")) == U(L"a/b.bak"));
  CHECK(ChangeFileExt(U(L"a/b"), U(L".bak")) == U(L"a/b.bak"));

  CHECK(ExtractFileDrive(U(L"C:\\x")) == U(L"C:"));
  CHECK(ExtractFileDrive(U(L"\\\\srv\\share\\x")) == U(L"\\\\srv\\share"));
  CHECK(ExtractFileDrive(U(L"/a/b")).IsEmpty());

  // Trailing-separator handling must be idempotent in both directions.
  const UnicodeString With = IncludeTrailingBackslash(U(L"/a/b"));
  CHECK(IsPathDelimiter(With, With.Length()));
  CHECK(IncludeTrailingBackslash(With) == With);
  CHECK(ExcludeTrailingBackslash(U(L"/a/b")) == U(L"/a/b"));
  CHECK(ExcludeTrailingBackslash(U(L"/a/b/")) == U(L"/a/b"));
  CHECK(ExcludeTrailingBackslash(U(L"/a/b\\")) == U(L"/a/b"));
  CHECK(IncludeTrailingBackslash(U(L"")).Length() == 1);
}

void TestFormat()
{
  CHECK(FmtIs(L"%s=%s", MakeFormatArgs(U(L"k"), U(L"v")), L"k=v"));
  CHECK(FmtIs(L"%d", MakeFormatArgs(42), L"42"));
  CHECK(FmtIs(L"%d", MakeFormatArgs(-42), L"-42"));
  CHECK(FmtIs(L"%x", MakeFormatArgs(255), L"FF"));
  CHECK(FmtIs(L"%.2f", MakeFormatArgs(1.5), L"1.50"));
  CHECK(FmtIs(L"no args", MakeFormatArgs(), L"no args"));
  CHECK(FmtIs(L"100%%", MakeFormatArgs(), L"100%"));

  // Width and alignment, which core uses to build aligned listings.
  CHECK(FmtIs(L"[%5s]", MakeFormatArgs(U(L"ab")), L"[   ab]"));
  CHECK(FmtIs(L"[%-5s]", MakeFormatArgs(U(L"ab")), L"[ab   ]"));
  CHECK(FmtIs(L"[%3d]", MakeFormatArgs(7), L"[  7]"));
  CHECK(FmtIs(L"[%-8s|%-8s]", MakeFormatArgs(U(L"a"), U(L"b")),
              L"[a       |b       ]"));
  CHECK(FmtIs(L"[%2s]", MakeFormatArgs(U(L"abcd")), L"[abcd]"));  // no truncation

  // Precision truncates a string but pads a number.
  CHECK(FmtIs(L"%.2s", MakeFormatArgs(U(L"abcd")), L"ab"));
  CHECK(FmtIs(L"%.4d", MakeFormatArgs(7), L"0007"));

  // Explicit argument index.
  CHECK(FmtIs(L"%1:s-%0:s", MakeFormatArgs(U(L"a"), U(L"b")), L"b-a"));

  // Mixed types, including the real format string from core's listing code.
  CHECK(FmtIs(L"%s/%s", MakeFormatArgs(U(L"dir"), U(L"file")), L"dir/file"));
  CHECK(FmtIs(L"%s", MakeFormatArgs(true), L"True"));
  CHECK(FmtIs(L"%s", MakeFormatArgs(L'z'), L"z"));
  CHECK(FmtIs(L"%s", MakeFormatArgs(123), L"123"));
  CHECK(FmtIs(L"%s", MakeFormatArgs("narrow"), L"narrow"));

  // Malformed input must stay visible rather than silently dropping text:
  // these carry server-supplied strings into logs.
  CHECK(FmtIs(L"%s %s", MakeFormatArgs(U(L"only")), L"only <missing>"));
  CHECK(FmtIs(L"%q", MakeFormatArgs(U(L"x")), L"%q"));
  CHECK(FmtIs(L"trailing %", MakeFormatArgs(), L"trailing "));

  // The ARRAYOFCONST macro paste, which is how core actually calls this.
  CHECK(Format(U(L"%s=%s"), ARRAYOFCONST((U(L"a"), U(L"b")))) == U(L"a=b"));
}

// Every shim function whose name collides with a Win32 A/W macro is referenced
// here, so the linker verifies the declaration and the definition agree.
//
// This exists because ReplaceText actually broke this way: <windows.h> renamed
// the definition to ReplaceTextW while the header still declared ReplaceText.
// Both compiled; only the link failed, and only because a test happened to call
// it. Taking the address of each one makes that failure immediate and certain.
void TestNoWin32MacroCollisions()
{
  volatile const void * Refs[] = {
    reinterpret_cast<const void *>(
      static_cast<UnicodeString (*)(const UnicodeString &, const UnicodeString &,
                                    const UnicodeString &)>(&ReplaceText)),
    reinterpret_cast<const void *>(
      static_cast<UnicodeString (*)(const UnicodeString &, const UnicodeString &,
                                    const UnicodeString &)>(&ReplaceStr)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&DeleteFile)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&CreateDir)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&RemoveDir)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&SetCurrentDir)),
    reinterpret_cast<const void *>(
      static_cast<UnicodeString (*)()>(&GetCurrentDir)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &, const UnicodeString &)>(&RenameFile)),
    reinterpret_cast<const void *>(
      static_cast<UnicodeString (*)(const UnicodeString &)>(&ExpandUNCFileName)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&FileExists)),
    reinterpret_cast<const void *>(
      static_cast<bool (*)(const UnicodeString &)>(&DirectoryExists)),
    reinterpret_cast<const void *>(
      static_cast<int (*)(const UnicodeString &, int, TSearchRec &)>(&FindFirst)),
    reinterpret_cast<const void *>(
      static_cast<int (*)(TSearchRec &)>(&FindNext)),
  };
  for (const volatile void * P : Refs)
  {
    CHECK(P != nullptr);
  }

  // And they must actually work, not merely link.
  CHECK(ReplaceText(U(L"A-b"), U(L"a"), U(L"z")) == U(L"z-b"));
  CHECK(!GetCurrentDir().IsEmpty());
  CHECK(!FileExists(U(L"/definitely/not/here/xyz")));
}

void TestConstructorsFromCore()
{
  // Both come straight from core idioms: a narrow literal, and a bare wchar_t
  // passed where a string is expected.
  CHECK(SameText(U(L"ON"), "on"));
  CHECK(EndsStr(L']', U(L"[x]")));
  CHECK(UnicodeString("abc") == U(L"abc"));
  CHECK(UnicodeString(L'x') == U(L"x"));
}

void TestLeftRightMid()
{
  const UnicodeString S(L"abcdef");
  CHECK(LeftStr(S, 2) == UnicodeString(L"ab"));
  CHECK(LeftStr(S, 99) == S);
  CHECK(LeftStr(S, 0).IsEmpty());
  CHECK(LeftStr(S, -1).IsEmpty());
  CHECK(RightStr(S, 2) == UnicodeString(L"ef"));
  CHECK(RightStr(S, 99) == S);
  CHECK(RightStr(S, 0).IsEmpty());
  CHECK(MidStr(S, 2, 3) == UnicodeString(L"bcd"));
  CHECK(MidStr(S, 5, 99) == UnicodeString(L"ef"));
  CHECK(MidStr(S, 7, 1).IsEmpty());
  // Configuration.cpp's idiom: does a key start with a prefix?
  const UnicodeString Prefix(L"abc");
  CHECK(LeftStr(S, Prefix.Length()) == Prefix);
}

}  // namespace

int main()
{
  TestNumbers();
  TestTextVersusStr();
  TestSearching();
  TestTransformation();
  TestPaths();
  TestFormat();
  TestNoWin32MacroCollisions();
  TestConstructorsFromCore();

  TestLeftRightMid();

  if (Failures == 0)
  {
    std::printf("all strutils/format tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
