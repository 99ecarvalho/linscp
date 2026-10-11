// Masks, TPath, TTimeSpan, TEncoding, RTTI and the environment helpers.
//
// Wildcard matching gets the most attention: it is recursive, and the cases
// that break such matchers are adjacent stars, a trailing star over an empty
// remainder, and a pattern longer than the name. Core uses this for the file
// masks users type, so a wrong answer silently transfers the wrong files.
#include "vcl_misc.h"
#include "vcl_strutils.h"

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

bool M(const wchar_t * Name, const wchar_t * Mask)
{
  return MatchesMask(U(Name), U(Mask));
}

void TestWildcardMatching()
{
  // Exact and trivial patterns.
  CHECK(M(L"file.txt", L"file.txt"));
  CHECK(!M(L"file.txt", L"other.txt"));
  CHECK(M(L"file.txt", L"*"));
  CHECK(M(L"", L"*"));
  CHECK(M(L"", L""));
  CHECK(!M(L"a", L""));
  CHECK(!M(L"", L"a"));

  // '*' spans any run, including an empty one.
  CHECK(M(L"file.txt", L"*.txt"));
  CHECK(M(L".txt", L"*.txt"));
  CHECK(M(L"file.txt", L"file.*"));
  CHECK(M(L"file.", L"file.*"));
  CHECK(M(L"file.txt", L"f*t"));
  CHECK(M(L"ft", L"f*t"));
  CHECK(!M(L"file.txtx", L"*.txt"));

  // '?' is exactly one character, never zero.
  CHECK(M(L"a.txt", L"?.txt"));
  CHECK(!M(L".txt", L"?.txt"));
  CHECK(!M(L"ab.txt", L"?.txt"));
  CHECK(M(L"ab", L"??"));

  // Adjacent stars must not multiply the work or change the answer.
  CHECK(M(L"abc", L"**"));
  CHECK(M(L"abc", L"*a*b*c*"));
  CHECK(M(L"abc", L"a**c"));
  CHECK(!M(L"abc", L"a**d"));

  // A pattern longer than the name must fail rather than read past the end.
  CHECK(!M(L"ab", L"abcdef"));
  CHECK(!M(L"ab", L"ab?"));

  // Case-insensitive, which is what Delphi's TMask does and what users expect
  // of a typed file mask.
  CHECK(M(L"FILE.TXT", L"*.txt"));
  CHECK(M(L"file.txt", L"*.TXT"));

  // And through the class form.
  Masks::TMask Mask(U(L"*.log"));
  CHECK(Mask.Mask == U(L"*.log"));
  CHECK(Mask.Matches(U(L"debug.log")));
  CHECK(!Mask.Matches(U(L"debug.txt")));
}

void TestPath()
{
  CHECK(TPath::GetFileName(U(L"/a/b/c.txt")) == U(L"c.txt"));
  CHECK(TPath::GetDirectoryName(U(L"/a/b/c.txt")) == U(L"/a/b"));
  CHECK(TPath::GetExtension(U(L"/a/b.tar.gz")) == U(L".gz"));
  CHECK(TPath::ChangeExtension(U(L"a/b.txt"), U(L".bak")) == U(L"a/b.bak"));

  const UnicodeString Joined = TPath::Combine(U(L"/a"), U(L"b.txt"));
  CHECK(Joined.Pos(U(L"b.txt")) > 0);
  CHECK(IsPathDelimiter(Joined, Joined.Pos(U(L"b.txt")) - 1));

  CHECK(TPath::IsPathRooted(U(L"/a/b")));
  CHECK(TPath::IsPathRooted(U(L"C:\\a")));
  CHECK(TPath::IsPathRooted(U(L"\\\\srv\\s")));
  CHECK(!TPath::IsPathRooted(U(L"a/b")));
  CHECK(!TPath::IsPathRooted(U(L"")));

  const UnicodeString Temp = TPath::GetTempPath();
  CHECK(!Temp.IsEmpty());
  CHECK(IsPathDelimiter(Temp, Temp.Length()));   // always ends in a separator
}

void TestTimeSpan()
{
  const TTimeSpan Second = TTimeSpan::FromSeconds(1.0);
  CHECK(Second.Ticks == TTimeSpan::TicksPerSecond);
  CHECK(Second.TotalSeconds == 1.0);
  CHECK(Second.TotalMilliseconds == 1000.0);

  const TTimeSpan Ms = TTimeSpan::FromMilliseconds(250.0);
  CHECK(Ms.TotalMilliseconds == 250.0);
  CHECK(Ms.TotalSeconds == 0.25);

  CHECK(TTimeSpan().Ticks == 0);
  CHECK(TTimeSpan::TicksPerDay ==
        TTimeSpan::TicksPerHour * 24);
}

void TestEncoding()
{
  const UnicodeString S = U(L"caf\u00e9");

  const RawByteString Utf8 = TEncoding::UTF8()->GetBytes(S);
  CHECK(Utf8.Length() == 5);                    // the e-acute takes two bytes
  CHECK(TEncoding::UTF8()->GetString(Utf8) == S);

  const RawByteString Ansi = TEncoding::ANSI()->GetBytes(S);
  CHECK(Ansi.Length() == 4);                    // one byte per character
  CHECK(TEncoding::ANSI()->GetString(Ansi) == S);

  // Default must round-trip as well, whichever it is.
  CHECK(TEncoding::Default()->GetString(
          TEncoding::Default()->GetBytes(S)) == S);

  CHECK(TEncoding::UTF8()->GetBytes(UnicodeString()).Length() == 0);
}

void TestInheritsFrom()
{
  struct TBase : public TObject {};
  struct TDerived : public TBase {};

  TDerived D;
  TObject * O = &D;
  CHECK(O->InheritsFrom<TDerived>());
  CHECK(O->InheritsFrom<TBase>());
  CHECK(O->InheritsFrom<TObject>());

  TBase B;
  TObject * P = &B;
  CHECK(P->InheritsFrom<TBase>());
  CHECK(!P->InheritsFrom<TDerived>());          // a base is not its derived
}

void TestEnvironment()
{
  // Round-trip a variable of our own rather than depending on the host's.
  CHECK(SetEnvironmentVariable(U(L"LINSCP_TEST_VAR"), U(L"hello")));
  CHECK(GetEnvironmentVariable(U(L"LINSCP_TEST_VAR")) == U(L"hello"));
  CHECK(GetEnvironmentVariable(U(L"LINSCP_NOT_SET_XYZ")).IsEmpty());

#if !defined(_WIN32)
  // POSIX-style expansion, which is the equivalent of %VAR% on Windows.
  CHECK(ExpandEnvironmentVariables(U(L"[$LINSCP_TEST_VAR]")) ==
        U(L"[hello]"));
  CHECK(ExpandEnvironmentVariables(U(L"[${LINSCP_TEST_VAR}]")) ==
        U(L"[hello]"));
  CHECK(ExpandEnvironmentVariables(U(L"plain")) == U(L"plain"));
  CHECK(ExpandEnvironmentVariables(U(L"$LINSCP_NOT_SET_XYZ")).IsEmpty());
#endif

  CHECK(AnsiSameText(U(L"AbC"), U(L"aBc")));
  CHECK(!AnsiSameStr(U(L"AbC"), U(L"aBc")));
}

void TestHostVersion()
{
  // Cannot assert exact values, but they must have been populated rather than
  // left at zero, which is what a failed query would leave behind.
  CHECK(Win32MajorVersion > 0);
  CHECK(MSecsPerDay == 86400000);
}

}  // namespace

void TestStripHotkey()
{
  CHECK(StripHotkey(L"&File") == UnicodeString(L"File"));
  CHECK(StripHotkey(L"Save &As") == UnicodeString(L"Save As"));
  CHECK(StripHotkey(L"No hotkey") == UnicodeString(L"No hotkey"));
  CHECK(StripHotkey(L"") == UnicodeString(L""));
  // An escaped ampersand survives as one literal '&' -- Windows' convention.
  CHECK(StripHotkey(L"Save && Exit") == UnicodeString(L"Save & Exit"));
  CHECK(StripHotkey(L"&&&x") == UnicodeString(L"&x"));
  CHECK(StripHotkey(L"trailing&") == UnicodeString(L"trailing"));
}

int main()
{
  TestWildcardMatching();
  TestPath();
  TestTimeSpan();
  TestEncoding();
  TestInheritsFrom();
  TestEnvironment();
  TestHostVersion();
  TestStripHotkey();

  if (Failures == 0)
  {
    std::printf("all misc tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
