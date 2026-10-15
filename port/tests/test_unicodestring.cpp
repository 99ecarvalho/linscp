// UnicodeString tests, weighted toward the 1-based indexing boundaries.
//
// Off-by-one errors here would corrupt data silently rather than fail to
// compile, so every 1-based entry point is tested at Index 1, at Length(), and
// out of range on both sides.
#include "vcl_strutils.h"
#include "vcl_bytestring.h"
#include "vcl_unicodestring.h"

#include <cassert>
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

bool Eq(const UnicodeString & S, const wchar_t * Expected)
{
  return S == UnicodeString(Expected);
}

void TestConstruction()
{
  CHECK(UnicodeString().IsEmpty());
  CHECK(UnicodeString().Length() == 0);
  CHECK(UnicodeString(static_cast<const wchar_t *>(nullptr)).IsEmpty());

  UnicodeString S(L"hello");
  CHECK(S.Length() == 5);
  CHECK(!S.IsEmpty());
  CHECK(Eq(S, L"hello"));

  CHECK(Eq(UnicodeString(L"hello", 3), L"hel"));
  CHECK(Eq(UnicodeString(L'x', 4), L"xxxx"));
  CHECK(UnicodeString(L'x', 0).IsEmpty());

  // c_str is always null-terminated and never null
  CHECK(UnicodeString().c_str() != nullptr);
  CHECK(UnicodeString().c_str()[0] == L'\0');

  // Numbers convert implicitly to their decimal text, as in C++Builder.
  CHECK(Eq(UnicodeString(42), L"42"));
  CHECK(Eq(UnicodeString(-7), L"-7"));
  CHECK(Eq(UnicodeString(0), L"0"));
  CHECK(Eq(UnicodeString(static_cast<__int64>(9000000000LL)), L"9000000000"));
  CHECK(S.c_str()[5] == L'\0');
}

void TestIndexingIsOneBased()
{
  UnicodeString S(L"abc");
  CHECK(S[1] == L'a');          // first character is index 1, not 0
  CHECK(S[2] == L'b');
  CHECK(S[3] == L'c');          // last character is index Length()

  S[1] = L'A';
  CHECK(Eq(S, L"Abc"));
  S[3] = L'C';
  CHECK(Eq(S, L"AbC"));
}

void TestSubString()
{
  UnicodeString S(L"abcdef");
  CHECK(Eq(S.SubString(1, 3), L"abc"));     // from the very start
  CHECK(Eq(S.SubString(4, 3), L"def"));     // to the very end
  CHECK(Eq(S.SubString(6, 1), L"f"));       // last character only
  CHECK(Eq(S.SubString(4), L"def"));        // open-ended overload
  CHECK(Eq(S.SubString(1), L"abcdef"));

  // clamping rather than throwing
  CHECK(Eq(S.SubString(4, 100), L"def"));   // count past the end
  CHECK(S.SubString(7, 1).IsEmpty());       // index past the end
  CHECK(S.SubString(1, 0).IsEmpty());       // zero count
  CHECK(S.SubString(1, -5).IsEmpty());      // negative count
  // An index below 1 is taken as 1, the count kept: C++Builder's
  // SubString, which TS3FileSystem::ParsePath relies on (SubString(0, P - 1)).
  CHECK(Eq(S.SubString(0, 3), L"abc"));
  CHECK(Eq(S.SubString(-1, 2), L"ab"));
  CHECK(UnicodeString().SubString(1, 5).IsEmpty());
}

void TestSearchingIsOneBasedAndZeroForMissing()
{
  UnicodeString S(L"a/b/c");
  CHECK(S.Pos(UnicodeString(L"a")) == 1);   // found at the first character
  CHECK(S.Pos(UnicodeString(L"b")) == 3);
  CHECK(S.Pos(UnicodeString(L"c")) == 5);   // found at the last character
  CHECK(S.Pos(UnicodeString(L"zz")) == 0);  // absent yields 0, not -1
  CHECK(S.Pos(L'/') == 2);
  CHECK(S.Pos(L'z') == 0);
  CHECK(UnicodeString().Pos(UnicodeString(L"x")) == 0);

  CHECK(S.LastDelimiter(UnicodeString(L"/")) == 4);
  CHECK(S.FirstDelimiter(UnicodeString(L"/")) == 2);
  CHECK(S.LastDelimiter(UnicodeString(L"#")) == 0);
  CHECK(S.FirstDelimiter(UnicodeString(L"#")) == 0);

  CHECK(S.IsDelimiter(UnicodeString(L"/"), 2));
  CHECK(!S.IsDelimiter(UnicodeString(L"/"), 1));
  CHECK(!S.IsDelimiter(UnicodeString(L"/"), 0));     // out of range, not a crash
  CHECK(!S.IsDelimiter(UnicodeString(L"/"), 99));
}

void TestMutation()
{
  UnicodeString S(L"abcdef");
  S.Delete(1, 2);
  CHECK(Eq(S, L"cdef"));                    // delete from the start
  S.Delete(3, 99);                          // count past the end clamps
  CHECK(Eq(S, L"cd"));
  S.Delete(5, 1);                           // index past the end is a no-op
  CHECK(Eq(S, L"cd"));
  S.Delete(1, 0);                           // zero count is a no-op
  CHECK(Eq(S, L"cd"));

  UnicodeString T(L"bcd");
  T.Insert(UnicodeString(L"a"), 1);         // insert at the very start
  CHECK(Eq(T, L"abcd"));
  T.Insert(UnicodeString(L"e"), T.Length() + 1);   // append position
  CHECK(Eq(T, L"abcde"));
  T.Insert(UnicodeString(L"!"), 999);       // clamped to the end
  CHECK(Eq(T, L"abcde!"));
  T.Insert(UnicodeString(L"^"), -3);        // clamped to the start
  CHECK(Eq(T, L"^abcde!"));

  UnicodeString U(L"abcdef");
  U.SetLength(3);
  CHECK(Eq(U, L"abc"));
  U.SetLength(0);
  CHECK(U.IsEmpty());
  U.SetLength(-1);                          // negative treated as zero
  CHECK(U.IsEmpty());
}

void TestCaseAndTrim()
{
  CHECK(Eq(UnicodeString(L"AbC").LowerCase(), L"abc"));
  CHECK(Eq(UnicodeString(L"AbC").UpperCase(), L"ABC"));
  CHECK(Eq(UnicodeString(L"a1-B").LowerCase(), L"a1-b"));   // non-letters intact

  CHECK(Eq(UnicodeString(L"  ab  ").Trim(), L"ab"));
  CHECK(Eq(UnicodeString(L"  ab  ").TrimLeft(), L"ab  "));
  CHECK(Eq(UnicodeString(L"  ab  ").TrimRight(), L"  ab"));
  CHECK(Eq(UnicodeString(L"\t\r\nab").Trim(), L"ab"));      // control chars too
  CHECK(UnicodeString(L"    ").Trim().IsEmpty());
  CHECK(UnicodeString().Trim().IsEmpty());
  CHECK(Eq(UnicodeString(L"ab").Trim(), L"ab"));            // nothing to trim
}

void TestComparison()
{
  CHECK(UnicodeString(L"abc") == UnicodeString(L"abc"));
  CHECK(UnicodeString(L"abc") != UnicodeString(L"abd"));
  CHECK(UnicodeString(L"abc") < UnicodeString(L"abd"));
  CHECK(UnicodeString(L"abd") > UnicodeString(L"abc"));
  CHECK(UnicodeString(L"ab") < UnicodeString(L"abc"));      // prefix sorts first

  CHECK(UnicodeString(L"abc").Compare(UnicodeString(L"abc")) == 0);
  CHECK(UnicodeString(L"abc").Compare(UnicodeString(L"abd")) == -1);
  CHECK(UnicodeString(L"abd").Compare(UnicodeString(L"abc")) == 1);

  CHECK(UnicodeString(L"AbC").CompareIC(UnicodeString(L"aBc")) == 0);
  CHECK(UnicodeString(L"abc").CompareIC(UnicodeString(L"abd")) == -1);
  CHECK(UnicodeString(L"ab").CompareIC(UnicodeString(L"abc")) == -1);
  CHECK(UnicodeString(L"abc").CompareIC(UnicodeString(L"ab")) == 1);
  CHECK(UnicodeString().CompareIC(UnicodeString()) == 0);
}

void TestConcatenation()
{
  UnicodeString S(L"ab");
  S += UnicodeString(L"cd");
  CHECK(Eq(S, L"abcd"));
  S += L'e';
  CHECK(Eq(S, L"abcde"));
  CHECK(Eq(UnicodeString(L"ab") + UnicodeString(L"cd"), L"abcd"));
  CHECK(Eq(UnicodeString(L"ab") + L'c', L"abc"));          // char on the right
  CHECK(Eq(L'a' + UnicodeString(L"bc"), L"abc"));          // char on the left
  CHECK(Eq(UnicodeString(L"a") + L"bc", L"abc"));          // literal via ctor
  CHECK(Eq(UnicodeString() + L'x', L"x"));
  CHECK(Eq(UnicodeString() + UnicodeString(L"x"), L"x"));
  CHECK((UnicodeString() + UnicodeString()).IsEmpty());
}

void TestUtf16Representation()
{
  // Characters outside the BMP occupy two UTF-16 code units, and Delphi's
  // Length() counts code units, not code points. Core relies on that.
  const wchar_t Emoji[] = { 0xD83D, 0xDE00, 0x0000 };   // U+1F600
  UnicodeString S(Emoji);
  CHECK(S.Length() == 2);
  CHECK(static_cast<unsigned>(S[1]) == 0xD83Du);
  CHECK(static_cast<unsigned>(S[2]) == 0xDE00u);

  // A 2-byte wchar_t is what makes the above true.
  CHECK(sizeof(wchar_t) == 2);
}

void TestCrossTypeStrings()
{
  // HierarchicalStorage.cpp: UnicodeString(Dest.c_str(), Dest.Length()).
  const RawByteString Dest("key%20x", 7);
  CHECK(UnicodeString(Dest.c_str(), Dest.Length()) == UnicodeString(L"key%20x"));
  CHECK(UnicodeString("abcdef", 3) == UnicodeString(L"abc"));
  // WebDAVFileSystem.cpp: U"\U0001F512" + Owner, a lock and a name.
  const UnicodeString Lock = U"\U0001F512" + UnicodeString(L"bob");
  CHECK(Lock.Length() == 5);
  CHECK((Lock[1] == static_cast<wchar_t>(0xD83D)) && (Lock[2] == static_cast<wchar_t>(0xDD12)));
  CHECK(Lock.SubString(3) == UnicodeString(L"bob"));
  // PuttyIntf.cpp: UTF8ToString on a char *.
  const char * Name = "caf\xC3\xA9";
  CHECK(UTF8ToString(Name) == UnicodeString(L"caf\u00E9"));
  // Byte strings compare with UnicodeString as text (S3, SessionData).
  const AnsiString Host("s3.amazonaws.com");
  CHECK(Host == UnicodeString(L"s3.amazonaws.com"));
  CHECK(UnicodeString(L"other") != Host);
  const RawByteString Pass("p", 1);
  CHECK(UnicodeString(L"p") == Pass);
  // SessionInfo.cpp: UtfLine.Insert('\r', Index).
  UTF8String Line("ab\n", 3);
  Line.Insert('\r', 3);
  CHECK(Line == UTF8String("ab\r\n"));
  // c_str() is char * even on a const string, for C APIs (neon).
  const UTF8String Path("/dir", 4);
  char * Raw = Path.c_str();
  CHECK(Raw[0] == '/');
}

}  // namespace

// data() NULL when empty, as C++Builder's.
void TestDataOfEmpty()
{
  const UnicodeString Empty;
  CHECK(Empty.data() == nullptr);
  CHECK(UnicodeString(L"x").data() != nullptr);
}

int main()
{
  TestDataOfEmpty();
  TestConstruction();
  TestIndexingIsOneBased();
  TestSubString();
  TestSearchingIsOneBasedAndZeroForMissing();
  TestMutation();
  TestCaseAndTrim();
  TestComparison();
  TestConcatenation();
  TestUtf16Representation();

  TestCrossTypeStrings();

  if (Failures == 0)
  {
    std::printf("all UnicodeString tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
