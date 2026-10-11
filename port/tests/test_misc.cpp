// Masks, TPath, TTimeSpan, TEncoding, RTTI and the environment helpers.
//
// Wildcard matching gets the most attention: it is recursive, and the cases
// that break such matchers are adjacent stars, a trailing star over an empty
// remainder, and a pattern longer than the name. Core uses this for the file
// masks users type, so a wrong answer silently transfers the wrong files.
#include <cmath>
#include "vcl_format.h"
#include <initializer_list>
#include <string>
#include "vcl_streams.h"
#include "vcl_variant.h"
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

TBytes MakeBytes(std::initializer_list<unsigned char> Values)
{
  TBytes B;
  B.Length = static_cast<int>(Values.size());
  int I = 0;
  for (unsigned char V : Values) { B[I++] = V; }
  return B;
}

void TestEncoding()
{
  const UnicodeString S = U(L"caf\u00e9");

  const TBytes Utf8 = TEncoding::UTF8->GetBytes(S);
  CHECK(Utf8.Length == 5);                      // the e-acute takes two bytes
  CHECK(TEncoding::UTF8->GetString(Utf8) == S);
  CHECK(TEncoding::UTF8->GetString(Utf8, 1, 2) == UnicodeString(L"af"));

  const TBytes Ansi = TEncoding::ANSI->GetBytes(S);
  CHECK(Ansi.Length == 4);                      // one byte per character
  CHECK(TEncoding::ANSI->GetString(Ansi) == S);
  CHECK(TEncoding::ASCII->GetString(Ansi) == UnicodeString(L"caf?"));

  const TBytes Le = TEncoding::Unicode->GetBytes(L"A");
  CHECK((Le.Length == 2) && (Le[0] == 0x41) && (Le[1] == 0));
  const TBytes Be = TEncoding::BigEndianUnicode->GetBytes(L"A");
  CHECK((Be.Length == 2) && (Be[0] == 0) && (Be[1] == 0x41));

  CHECK(TEncoding::Default->GetString(TEncoding::Default->GetBytes(S)) == S);
  CHECK(TEncoding::UTF8->GetBytes(UnicodeString()).Length == 0);

  bool Raised = false;
  try { TEncoding::UTF8->GetString(Utf8, 3, 9); }
  catch (const EEncodingError &) { Raised = true; }
  CHECK(Raised);
}

// Common.cpp's use: detect a BOM, defaulting to UTF-8, and skip it.
void TestGetBufferEncoding()
{
  TEncoding * E = nullptr;
  CHECK(TEncoding::GetBufferEncoding(MakeBytes({0xEF, 0xBB, 0xBF, 'x'}), E, TEncoding::UTF8) == 3);
  CHECK(E == TEncoding::UTF8);
  E = nullptr;
  CHECK(TEncoding::GetBufferEncoding(MakeBytes({0xFF, 0xFE, 'x', 0}), E, TEncoding::UTF8) == 2);
  CHECK(E == TEncoding::Unicode);
  E = nullptr;
  CHECK(TEncoding::GetBufferEncoding(MakeBytes({0xFE, 0xFF, 0, 'x'}), E, TEncoding::UTF8) == 2);
  CHECK(E == TEncoding::BigEndianUnicode);
  E = nullptr;
  CHECK(TEncoding::GetBufferEncoding(MakeBytes({'p', 'l', 'a', 'i', 'n'}), E, TEncoding::ANSI) == 0);
  CHECK(E == TEncoding::ANSI);                  // no BOM: the default
  // With an encoding given: its preamble's length only if the buffer has it.
  E = TEncoding::UTF8;
  CHECK(TEncoding::GetBufferEncoding(MakeBytes({0xFF, 0xFE}), E) == 0);
  CHECK(E == TEncoding::UTF8);
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
  CHECK(Win32MajorVersion() > 0);
  CHECK(CheckWin32Version(1));
  CHECK(!CheckWin32Version(1000));
  CHECK(MSecsPerDay == 86400000);
}

void TestIsDriveRooted()
{
#if defined(_WIN32)
  CHECK(TPath::IsDriveRooted(L"C:\\Users\\x.ini"));
  CHECK(TPath::IsDriveRooted(L"d:/data"));
  CHECK(!TPath::IsDriveRooted(L"C:relative"));     // drive-relative, not absolute
  CHECK(!TPath::IsDriveRooted(L"C:"));
  CHECK(!TPath::IsDriveRooted(L"\\\\server\\share"));
  CHECK(!TPath::IsDriveRooted(L"1:\\x"));
#else
  // No drives on POSIX, as in Delphi's POSIX RTL.
  CHECK(!TPath::IsDriveRooted(L"/home/user"));
  CHECK(!TPath::IsDriveRooted(L"C:\\Users"));
#endif
  CHECK(!TPath::IsDriveRooted(L""));
}

void TestVariantArray()
{
  // SftpFileSystem's use, line for line.
  Variant Numbers = VarArrayCreate(OPENARRAY(int, (0, 1)), varLongWord);
  CHECK(Numbers.ArrayLowBound() == 0);
  CHECK(Numbers.ArrayHighBound() == 1);
  Numbers.PutElement(4000000000u, 1);              // beyond int, as LongWord allows
  CHECK((unsigned int)Numbers.GetElement(1) == 4000000000u);
  CHECK((unsigned int)Numbers.GetElement(0) == 0u);
  Numbers.ArrayRedim(10);
  CHECK(Numbers.ArrayHighBound() == 10);
  CHECK((unsigned int)Numbers.GetElement(1) == 4000000000u);   // kept
  Numbers.PutElement(Numbers.GetElement(1), 0);                // shifting down
  CHECK((unsigned int)Numbers.GetElement(0) == 4000000000u);

  const Variant Copy = Numbers;                     // deep, as VarCopy is
  Numbers.PutElement(7u, 0);
  CHECK((unsigned int)Copy.GetElement(0) == 4000000000u);

  bool Raised = false;
  try { Numbers.GetElement(11); }
  catch (const EVariantBoundsError &) { Raised = true; }
  CHECK(Raised);
  Raised = false;
  try { VarArrayCreate(OPENARRAY(int, (0, 1, 0, 1)), varLongWord); }
  catch (const EVariantError &) { Raised = true; }
  CHECK(Raised);                                    // two dimensions: refused
}

UnicodeString TempPath(const wchar_t * Name)
{
  return IncludeTrailingBackslash(TPath::GetTempPath()) + Name;
}

void WriteBytes(const UnicodeString & Path, const std::string & Bytes)
{
  TFileStream S(Path, fmCreate);
  S.WriteBuffer(Bytes.data(), static_cast<int>(Bytes.size()));
}

std::string ReadBytes(const UnicodeString & Path)
{
  TFileStream S(Path, fmOpenRead);
  std::string B(static_cast<std::size_t>(S.Size), '\0');
  if (!B.empty()) { S.ReadBuffer(&B[0], static_cast<int>(B.size())); }
  return B;
}

void TestTFile()
{
  const UnicodeString P = TempPath(L"linscp_tfile_test.txt");
  const UnicodeString Text(L"ssh-ed25519 AAAA key caf\u00E9\n");
  TFile::WriteAllText(P, Text);
  const std::string Bytes = ReadBytes(P);
  CHECK(Bytes == std::string("ssh-ed25519 AAAA key caf\xC3\xA9\n"));   // UTF-8, no BOM
  CHECK(TFile::ReadAllText(P) == Text);

  WriteBytes(P, std::string("\xEF\xBB\xBFx\xC3\xA9"));                 // UTF-8 BOM
  CHECK(TFile::ReadAllText(P) == UnicodeString(L"x\u00E9"));
  WriteBytes(P, std::string("\xFF\xFEx\0\xE9\0", 6));                   // UTF-16 LE
  CHECK(TFile::ReadAllText(P) == UnicodeString(L"x\u00E9"));
  WriteBytes(P, std::string("\xFE\xFF\0x\0\xE9", 6));                   // UTF-16 BE
  CHECK(TFile::ReadAllText(P) == UnicodeString(L"x\u00E9"));
  DeleteFile(P);

  bool Raised = false;
  try { TFile::ReadAllText(TempPath(L"linscp_no_such_file.txt")); }
  catch (const EFileNotFoundException &) { Raised = true; }
  CHECK(Raised);
}

void TestInt64FunctionalCast()
{
  // Terminal.cpp's spelling, which a `long long` macro would not compile.
  const unsigned long HSize = 1, LSize = 2;
  const __int64 Size = (__int64(HSize) << 32) + LSize;
  CHECK(Size == 0x100000002LL);
}

void TestTimeSpanComponents()
{
  // FormatTimeZone's input: UTC+05:30 arrives as -19800 seconds.
  const TTimeSpan Span = TTimeSpan::FromSeconds(-19800);
  CHECK(Span.Hours == -5);
  CHECK(Span.Minutes == -30);
  CHECK(Span.Seconds == 0);
  CHECK(Span <= TTimeSpan::Zero);
  CHECK(!(TTimeSpan::FromSeconds(1) <= TTimeSpan::Zero));
  const TTimeSpan Long = TTimeSpan::FromSeconds(90061.5);   // 1d 1h 1m 1.5s
  CHECK((Long.Days == 1) && (Long.Hours == 1) && (Long.Minutes == 1) &&
        (Long.Seconds == 1) && (Long.Milliseconds == 500));
}

void TestBase64()
{
  CHECK(EncodeBase64("Man", 3) == UnicodeString(L"TWFu"));
  CHECK(EncodeBase64("Ma", 2) == UnicodeString(L"TWE="));
  CHECK(EncodeBase64("M", 1) == UnicodeString(L"TQ=="));
  // Line breaks every 76 characters, which core strips (EncodeStrToBase64).
  std::string Long(100, 'x');
  const UnicodeString Encoded = EncodeBase64(Long.data(), static_cast<int>(Long.size()));
  CHECK(Encoded.Pos(sLineBreak) == 77);
  const UnicodeString Stripped = ReplaceStr(Encoded, sLineBreak, L"");
  CHECK(Stripped.Length() == 136);
  const TBytes Back = DecodeBase64(Encoded);           // breaks ignored
  CHECK(Back.Length == 100);
  bool Same = true;
  for (int I = 0; I < Back.Length; ++I) { Same = Same && (Back[I] == 'x'); }
  CHECK(Same);
  CHECK(DecodeBase64(L"TQ==").Length == 1);
}

void TestExceptObject()
{
  CHECK(ExceptObject() == nullptr);                   // outside a handler
  try
  {
    throw EAbort(UnicodeString(L"stopped"));
  }
  catch (...)
  {
    TObject * O = ExceptObject();
    CHECK(O != nullptr);
    CHECK(O->ClassName() == UnicodeString(L"EAbort"));
    wchar_t Buffer[256];
    const int N = ExceptionErrorMessage(O, ExceptAddr(), Buffer, 256);
    const UnicodeString M(Buffer, N);
    CHECK(M.Pos(UnicodeString(L"Exception EAbort in module ")) == 1);
    CHECK(M.Pos(UnicodeString(L"stopped.")) > 0);      // a period appended
  }
  try { throw 42; }
  catch (...) { CHECK(ExceptObject() == nullptr); }   // not a Delphi object
}

void TestFormatFloat()
{
  CHECK(FormatFloat(L"#,##0", 1234567.4) == UnicodeString(L"1,234,567"));
  CHECK(FormatFloat(L"#,##0", 999.5) == UnicodeString(L"1,000"));
  CHECK(FormatFloat(L"#,##0", 0) == UnicodeString(L"0"));
  CHECK(FormatFloat(L"#,##0", -1234.5) == UnicodeString(L"-1,235"));
  CHECK(FormatFloat(L"0.00", 3.14159) == UnicodeString(L"3.14"));
  CHECK(FormatFloat(L"0.##", 2.5) == UnicodeString(L"2.5"));
  CHECK(FormatFloat(L"0;(0)", -5) == UnicodeString(L"(5)"));
  CHECK(FormatFloat(L"0 'KB'", 12) == UnicodeString(L"12 KB"));
  bool Raised = false;
  try { FormatFloat(L"0.00E+00", 1.0); }
  catch (const EConvertError &) { Raised = true; }
  CHECK(Raised);
}

void TestTimeSpanTotals()
{
  // FtpFileSystem.cpp's time-difference check.
  const double Hours = TTimeSpan::FromSeconds(-48 * 3600 - 1800).TotalHours;
  CHECK(std::fabs(Hours + 48.5) < 1e-9);
  CHECK(std::fabs(TTimeSpan::FromSeconds(90).TotalMinutes - 1.5) < 1e-9);
  CHECK(std::fabs(TTimeSpan::FromSeconds(43200).TotalDays - 0.5) < 1e-9);
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
  TestGetBufferEncoding();
  TestInheritsFrom();
  TestEnvironment();
  TestHostVersion();
  TestStripHotkey();

  TestIsDriveRooted();

  TestVariantArray();
  TestTFile();
  TestInt64FunctionalCast();

  TestTimeSpanComponents();
  TestBase64();
  TestExceptObject();
  TestFormatFloat();

  TestTimeSpanTotals();

  if (Failures == 0)
  {
    std::printf("all misc tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
