// Byte-string and encoding tests.
//
// Weighted toward UTF-8 conversion: WinSCP sends these bytes on the wire, so a
// mistake here is silent protocol corruption rather than a crash. Malformed
// input is tested explicitly, because it arrives from remote servers.
#include <cstring>
#include "vcl_bytestring.h"

#include <cstdio>
#include <string>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

std::string Hex(const std::string & S)
{
  static const char * D = "0123456789abcdef";
  std::string R;
  for (unsigned char C : S)
  {
    R += D[C >> 4];
    R += D[C & 0xF];
  }
  return R;
}

void TestUtf8Ascii()
{
  UnicodeString S(L"hello");
  CHECK(linscp::Utf16ToUtf8(S) == "hello");
  CHECK(linscp::Utf8ToUtf16("hello", 5) == S);
  CHECK(linscp::Utf16ToUtf8(UnicodeString()).empty());
  CHECK(linscp::Utf8ToUtf16("", 0).IsEmpty());
  CHECK(linscp::Utf8ToUtf16(nullptr, 0).IsEmpty());
}

void TestUtf8MultiByte()
{
  // U+00E9 e-acute -> 2 bytes
  const wchar_t E[] = { 0x00E9, 0 };
  CHECK(Hex(linscp::Utf16ToUtf8(UnicodeString(E))) == "c3a9");
  CHECK(linscp::Utf8ToUtf16("\xc3\xa9", 2) == UnicodeString(E));

  // U+20AC euro -> 3 bytes
  const wchar_t Euro[] = { 0x20AC, 0 };
  CHECK(Hex(linscp::Utf16ToUtf8(UnicodeString(Euro))) == "e282ac");
  CHECK(linscp::Utf8ToUtf16("\xe2\x82\xac", 3) == UnicodeString(Euro));

  // U+1F600 -> surrogate pair in UTF-16, 4 bytes in UTF-8
  const wchar_t Emoji[] = { 0xD83D, 0xDE00, 0 };
  UnicodeString U(Emoji);
  CHECK(U.Length() == 2);                       // two UTF-16 code units
  CHECK(Hex(linscp::Utf16ToUtf8(U)) == "f09f9880");
  UnicodeString Back = linscp::Utf8ToUtf16("\xf0\x9f\x98\x80", 4);
  CHECK(Back.Length() == 2);
  CHECK(Back == U);                             // round trip preserves the pair
}

void TestUtf8Malformed()
{
  // Each of these must yield U+FFFD rather than invalid output or a crash.
  const char16_t R = 0xFFFD;

  UnicodeString T = linscp::Utf8ToUtf16("\xe2\x82", 2);      // truncated 3-byte
  CHECK(T.Length() == 1 && static_cast<char16_t>(T[1]) == R);

  UnicodeString B = linscp::Utf8ToUtf16("\xe2\x41\x41", 3);  // bad continuation
  CHECK(static_cast<char16_t>(B[1]) == R);

  UnicodeString C = linscp::Utf8ToUtf16("\x80", 1);          // lone continuation
  CHECK(C.Length() == 1 && static_cast<char16_t>(C[1]) == R);

  UnicodeString O = linscp::Utf8ToUtf16("\xc0\xaf", 2);      // overlong "/"
  CHECK(static_cast<char16_t>(O[1]) == R);

  UnicodeString F = linscp::Utf8ToUtf16("\xf5\x80\x80\x80", 4);  // beyond U+10FFFF
  CHECK(static_cast<char16_t>(F[1]) == R);

  // CESU-8 style encoded surrogate must be rejected, not passed through
  UnicodeString S = linscp::Utf8ToUtf16("\xed\xa0\x80", 3);  // U+D800
  CHECK(static_cast<char16_t>(S[1]) == R);

  // An unpaired surrogate on the way out cannot be encoded
  const wchar_t Lone[] = { 0xD83D, 0 };
  CHECK(Hex(linscp::Utf16ToUtf8(UnicodeString(Lone))) == "efbfbd");  // U+FFFD

  // Valid text after malformed input must still decode
  UnicodeString Mixed = linscp::Utf8ToUtf16("\x80" "ok", 3);
  CHECK(Mixed.Length() == 3);
  CHECK(Mixed[2] == L'o' && Mixed[3] == L'k');
}

void TestRawPreservesBytes()
{
  // RawByteString must survive arbitrary bytes untouched, including 0x80-0xFF.
  std::string Bytes;
  for (int I = 0; I < 256; ++I)
  {
    Bytes += static_cast<char>(I);
  }
  UnicodeString U = linscp::RawToUtf16(Bytes.data(), Bytes.size());
  CHECK(U.Length() == 256);
  CHECK(linscp::Utf16ToRaw(U) == Bytes);        // lossless round trip

  RawByteString R(Bytes.data(), static_cast<int>(Bytes.size()));
  CHECK(R.Length() == 256);
  CHECK(R.Bytes() == Bytes);
}

void TestTypesAreDistinct()
{
  // Assigning between code pages must not compile silently; verify each
  // converts through UnicodeString with its own rules instead.
  UnicodeString U(L"café");
  UTF8String Utf8(U);
  AnsiString Ansi(U);
  RawByteString Raw(U);

  CHECK(Hex(Utf8.Bytes()) == "636166c3a9");     // UTF-8: e-acute is 2 bytes
  CHECK(Hex(Ansi.Bytes()) == "636166e9");       // Latin-1: 1 byte
  CHECK(Hex(Raw.Bytes()) == "636166e9");        // raw: low byte of each unit

  CHECK(ToUnicodeString(Utf8) == U);            // only UTF-8 round-trips exactly
  CHECK(ToUnicodeString(Ansi) == U);            // Latin-1 happens to here too
}

void TestAnsiStringApiIsOneBased()
{
  AnsiString S("abcdef");
  CHECK(S.Length() == 6);
  CHECK(S[1] == 'a');                           // 1-based
  CHECK(S[6] == 'f');
  CHECK(S.SubString(1, 3) == AnsiString("abc"));
  CHECK(S.SubString(4) == AnsiString("def"));
  CHECK(S.SubString(4, 100) == AnsiString("def"));   // clamps
  CHECK(S.SubString(7, 1).IsEmpty());
  CHECK(S.Pos(AnsiString("cd")) == 3);
  CHECK(S.Pos(AnsiString("zz")) == 0);               // 0 for absent
  CHECK(S.Pos('a') == 1);

  AnsiString T("abcdef");
  T.Delete(1, 2);
  CHECK(T == AnsiString("cdef"));
  T.Insert(AnsiString("X"), 1);
  CHECK(T == AnsiString("Xcdef"));
  T.SetLength(2);
  CHECK(T == AnsiString("Xc"));

  AnsiString E;
  CHECK(E.IsEmpty() && E.c_str() != nullptr && E.c_str()[0] == '\0');
  CHECK((AnsiString("ab") + AnsiString("cd")) == AnsiString("abcd"));
  CHECK(AnsiString('x', 3) == AnsiString("xxx"));
}

// C++Builder converts byte strings to UnicodeString implicitly; core relies on
// it (Configuration.cpp passes ReadBinaryData's RawByteString to a
// UnicodeString parameter). Same result as the explicit conversion.
UnicodeString TakesUnicode(const UnicodeString & S) { return S; }

void TestImplicitToUnicode()
{
  const RawByteString Raw("a\xE9z", 3);
  CHECK(TakesUnicode(Raw) == ToUnicodeString(Raw));
  CHECK(TakesUnicode(Raw).Length() == 3);       // byte for byte
  CHECK(TakesUnicode(Raw)[2] == static_cast<wchar_t>(0x00E9));

  const UTF8String Utf8("a\xC3\xA9z", 4);
  CHECK(TakesUnicode(Utf8) == UnicodeString(L"a\u00E9z"));  // real UTF-8 decode

  // Core's cache round-trip: UnicodeString -> binary -> UnicodeString.
  const UnicodeString Original(L"dir\u00E9/\u00FFend");
  const RawByteString Stored = Original;
  const UnicodeString Back = Stored;
  CHECK(Back == Original);
}

void TestWritableCStrAndCharAssignment()
{
  // Common.cpp's Shred: wipe a secret in place.
  AnsiString Secret("password");
  std::memset(Secret.c_str(), 0, static_cast<std::size_t>(Secret.Length()));
  CHECK(Secret.Length() == 8);
  CHECK(Secret[1] == '\0');
  RawByteString Raw;
  Raw = "abc";                                   // not ambiguous
  CHECK(Raw.Length() == 3);
  Raw = Raw.c_str();
  CHECK(Raw == RawByteString("abc"));
}

RawByteString TakesRaw(const RawByteString & R) { return R; }

void TestCrossCodePageConversions()
{
  // SftpFileSystem's AddString(UTF8String) reaching AddString(RawByteString):
  // the bytes pass through.
  const UTF8String Utf8("caf\xC3\xA9", 5);
  CHECK(TakesRaw(Utf8).Bytes() == std::string("caf\xC3\xA9"));
  // Its L"..." extension names arrive as RawByteString directly.
  CHECK(TakesRaw(L"check-file-name").Bytes() == std::string("check-file-name"));
  // Between two real code pages the text is transcoded, not the bytes copied.
  const AnsiString Ansi = Utf8;
  CHECK(Ansi.Bytes() == std::string("caf\xE9"));
  const UTF8String Back = Ansi;
  CHECK(Back.Bytes() == std::string("caf\xC3\xA9"));
  // Out of RawByteString: passed through (no run-time code page to honour).
  const RawByteString Raw("\xC3\xA9", 2);
  const UTF8String FromRaw = Raw;
  CHECK(FromRaw.Bytes() == std::string("\xC3\xA9"));

  // Common.cpp's Shred ends with `Str = L""` for every byte-string type.
  RawByteString R2("x", 1);
  R2 = L"";
  CHECK(R2.IsEmpty());
}

void TestTrim()
{
  // FtpListResult.cpp trims listing lines this way.
  const RawByteString Line(" \t-rw-r--r-- 1 u g 0 file\r\n");
  CHECK(Line.TrimRight() == RawByteString(" \t-rw-r--r-- 1 u g 0 file"));
  CHECK(Line.TrimLeft() == RawByteString("-rw-r--r-- 1 u g 0 file\r\n"));
  CHECK(Line.Trim() == RawByteString("-rw-r--r-- 1 u g 0 file"));
  CHECK(RawByteString("   ").Trim().IsEmpty());
  CHECK(RawByteString("caf\xC3\xA9 ").TrimRight() == RawByteString("caf\xC3\xA9"));  // high bytes kept
}

// C++Builder's data() of an empty string is NULL, c_str() an empty one;
// core tells "none" by it (S3FileSystem's session token).
void TestDataOfEmpty()
{
  const UTF8String Empty;
  CHECK(Empty.data() == nullptr);
  CHECK((Empty.c_str() != nullptr) && (Empty.c_str()[0] == '\0'));
  const UTF8String Token("token");
  CHECK((Token.data() != nullptr) && (std::strcmp(Token.data(), "token") == 0));
}

}  // namespace

int main()
{
  TestDataOfEmpty();
  TestUtf8Ascii();
  TestUtf8MultiByte();
  TestUtf8Malformed();
  TestRawPreservesBytes();
  TestTypesAreDistinct();
  TestAnsiStringApiIsOneBased();

  TestImplicitToUnicode();

  TestWritableCStrAndCharAssignment();

  TestCrossCodePageConversions();

  TestTrim();

  if (Failures == 0)
  {
    std::printf("all byte-string tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
