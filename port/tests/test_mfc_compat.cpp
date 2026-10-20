// linscp's MFC-compatible layer (port/mfc-compat): CString, CTime, CFile.
//
// Every member the layer implements, against the behaviour its header
// specifies (doc/qt/adr/0017-mfc-compat.md): Microsoft's documented
// behaviour, and linscp's choice where that documentation is silent --
// out-of-range counts and positions are clamped, not-found is -1.
//
// Built as the FTP engine is: vcl_win32_prelude.h forced first, then <afx.h>.
// Run with TZ=UTC0 (set by ctest) so CTime's local-time getters can also be
// checked against fixed values where the runtime honours it.
#include <afx.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <type_traits>

#include "vcl.h"

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

// Equal as whole buffers, embedded nulls included.
bool Same(const CString & S, const wchar_t * Expected)
{
  int N = 0;
  while (Expected[N] != L'\0')
  {
    ++N;
  }
  if (S.GetLength() != N)
  {
    return false;
  }
  return std::memcmp(S.GetString(), Expected, sizeof(wchar_t) * N) == 0;
}

template <typename F>
bool Raises(F Fn)
{
  try
  {
    Fn();
  }
  catch (Exception &)
  {
    return true;
  }
  return false;
}

template <typename F>
bool RaisesOS(F Fn)
{
  try
  {
    Fn();
  }
  catch (EOSError &)
  {
    return true;
  }
  return false;
}

// The message of the EOSError Fn raises, or "<none>".
template <typename F>
UnicodeString OSMessage(F Fn)
{
  try
  {
    Fn();
  }
  catch (EOSError & E)
  {
    return E.Message;
  }
  return UnicodeString(L"<none>");
}

bool EndsWith(const UnicodeString & S, const UnicodeString & Tail)
{
  return (S.Length() >= Tail.Length()) && (S.SubString(S.Length() - Tail.Length() + 1) == Tail);
}

// --- CString -----------------------------------------------------------------

void TestConstruction()
{
  CString Empty;
  CHECK(Empty.IsEmpty());
  CHECK(Empty.GetLength() == 0);
  CHECK(Same(Empty, L""));
  CHECK(Empty.GetString() != nullptr);
  CHECK(Empty.c_str() != nullptr);

  CHECK(Same(CString(L"abc"), L"abc"));
  CHECK(Same(CString(static_cast<const wchar_t *>(nullptr)), L""));
  CHECK(Same(CString(L"abcdef", 3), L"abc"));
  CHECK(Same(CString(L"abc", 0), L""));
  CHECK(Same(CString(L"abc", -1), L""));
  CHECK(Same(CString(static_cast<const wchar_t *>(nullptr), 4), L""));
  // Exactly Length characters, an embedded null included.
  const wchar_t WithNull[] = { L'a', L'\0', L'b', L'\0' };
  CString N(WithNull, 3);
  CHECK(N.GetLength() == 3);
  CHECK(N.GetAt(1) == L'\0');
  CHECK(N.GetAt(2) == L'b');

  // Null-terminated narrow text: decoded as UnicodeString decodes it (UTF-8,
  // one U+FFFD per maximal invalid subsequence), on every platform.
  CHECK(Same(CString("plain"), L"plain"));
  CHECK(Same(CString("caf\xC3\xA9"), L"caf\x00E9"));
  CHECK(Same(CString("\xE2\x82\xAC"), L"\x20AC"));
  CHECK(Same(CString("caf\xE9"), L"caf\xFFFD"));
  for (const char * Text : { "caf\xE9", "\xE2\x82", "a\xF0\x9F\x98\x80z", "\xC0\xAF\xED\xA0\x80", "\xFF\xFE" })
  {
    CHECK(UnicodeString(CString(Text).GetString()) == UnicodeString(Text));
  }
  // With a length: each byte is the code point of its value (Latin-1), on
  // every platform, embedded nulls kept.
  CHECK(Same(CString("abcdef", 4), L"abcd"));
  CHECK(Same(CString("\xC5\x91", 2), L"\x00C5\x0091"));
  CHECK(Same(CString("\xC3\xA9", 2), L"\x00C3\x00A9"));
  CHECK(CString("a\0b", 3).GetLength() == 3);
  CHECK(CString("a\0b", 3).GetAt(2) == L'b');
  CHECK(Same(CString(static_cast<const char *>(nullptr), 3), L""));
  CHECK(Same(CString("abc", -1), L""));
  CHECK(Same(CString("abc", 0), L""));
  CHECK(Same(CString(static_cast<const char *>(nullptr)), L""));

  CHECK(Same(CString(UnicodeString(L"from us")), L"from us"));
  // Only explicitly.
  static_assert(!std::is_convertible<UnicodeString, CString>::value, "explicit");
  static_assert(std::is_constructible<CString, UnicodeString>::value, "constructible");
  CHECK(Same(CString(UnicodeString()), L""));

  CString Copy(N);
  CHECK(Copy.GetLength() == 3);
  CString Moved(static_cast<CString &&>(Copy));
  CHECK(Moved.GetLength() == 3);
}

void TestAssignment()
{
  CString S;
  S = L"wide";
  CHECK(Same(S, L"wide"));
  S = "narrow";
  CHECK(Same(S, L"narrow"));
  S = "caf\xC3\xA9";
  CHECK(Same(S, L"caf\x00E9"));
  S = "caf\xE9";
  CHECK(Same(S, L"caf\xFFFD"));
  S = static_cast<const char *>(nullptr);
  CHECK(S.IsEmpty());
  S = L'c';
  CHECK(Same(S, L"c"));
  S = static_cast<const wchar_t *>(nullptr);
  CHECK(S.IsEmpty());
  CString T(L"other");
  S = T;
  CHECK(Same(S, L"other"));
  // From its own buffer.
  S = S.GetString() + 2;
  CHECK(Same(S, L"her"));
  S.Empty();
  CHECK(S.IsEmpty());
  S = CString(UnicodeString(L"us"));
  CHECK(Same(S, L"us"));
}

void TestAccess()
{
  CString S(L"abc");
  CHECK(S.GetAt(0) == L'a');
  CHECK(S.GetAt(2) == L'c');
  CHECK(S[1] == L'b');
  // The terminator is addressable and reads 0.
  CHECK(S.GetAt(3) == L'\0');
  CHECK(Raises([&] { S.GetAt(4); }));
  CHECK(Raises([&] { S.GetAt(-1); }));

  S.SetAt(1, L'X');
  CHECK(Same(S, L"aXc"));
  CHECK(Raises([&] { S.SetAt(3, L'Y'); }));
  CHECK(Raises([&] { S.SetAt(-1, L'Y'); }));
  CHECK(Same(S, L"aXc"));
  // SetAt on a copy leaves the original alone.
  CString C(S);
  C.SetAt(0, L'z');
  CHECK(Same(S, L"aXc"));
  CHECK(Same(C, L"zXc"));

  const wchar_t * P = S;
  CHECK(std::memcmp(P, L"aXc", 4 * sizeof(wchar_t)) == 0);
  CHECK(P == S.GetString());
  CHECK(S.c_str() == S.GetString());
}

void TestConcatenation()
{
  CString S(L"ab");
  S += CString(L"cd");
  S += L"ef";
  S += L'g';
  S += static_cast<const wchar_t *>(nullptr);
  CHECK(Same(S, L"abcdefg"));
  S += S;
  CHECK(Same(S, L"abcdefgabcdefg"));

  CHECK(Same(CString(L"x") + CString(L"y"), L"xy"));
  CHECK(Same(CString(L"x") + L"y", L"xy"));
  CHECK(Same(L"x" + CString(L"y"), L"xy"));
  CHECK(Same(CString(L"x") + L'y', L"xy"));
  CHECK(Same(L'x' + CString(L"y"), L"xy"));
}

void TestComparison()
{
  CString A(L"abc");
  CHECK(A.Compare(L"abc") == 0);
  CHECK(A.Compare(L"abd") < 0);
  // Exactly -1, 0 or +1.
  CHECK(A.Compare(L"abd") == -1);
  CHECK(A.Compare(L"ab") == 1);
  CHECK(A.Compare(L"abcd") == -1);
  CHECK(A.Compare(L"abc") == 0);
  CHECK(A.Compare(nullptr) == 1);
  CHECK(CString().Compare(L"a") == -1);
  CHECK(CString(L"\x00E9").Compare(L"z") == 1);
  CHECK(CString(L"\xFFFF").Compare(L"a") == 1);
  CHECK(CString(L"a").Compare(L"\xFFFF") == -1);
  CHECK(CString(L"A").Compare(L"a") == -1);
  CHECK(A.Compare(L"abb") > 0);
  CHECK(A.Compare(L"ab") > 0);
  CHECK(A.Compare(L"abcd") < 0);
  CHECK(A.Compare(L"ABC") > 0);
  CHECK(A.Compare(nullptr) > 0);
  CHECK(CString().Compare(nullptr) == 0);
  // Code unit order, not signed: U+00E9 sorts after 'z'.
  CHECK(CString(L"\x00E9").Compare(L"z") > 0);
  // Ends at the first null.
  const wchar_t WithNull[] = { L'a', L'b', L'c', L'\0', L'x' };
  CHECK(CString(WithNull, 5).Compare(L"abc") == 0);

  CHECK(A.CompareNoCase(L"ABC") == 0);
  CHECK(A.CompareNoCase(L"aBc") == 0);
  CHECK(A.CompareNoCase(L"ABD") < 0);
  CHECK(A.CompareNoCase(L"AB") > 0);
  // ASCII folding only, to lower case: '_' (0x5F) sorts before 'a'.
  CHECK(CString(L"_").CompareNoCase(L"A") < 0);
  CHECK(CString(L"_").CompareNoCase(L"A") == L'_' - L'a');
  CHECK(A.CompareNoCase(L"ABD") == L'c' - L'd');
  CHECK(CString(L"ABC").CompareNoCase(L"ab") == L'c');
  CHECK(CString(L"\x00C9").CompareNoCase(L"\x00E9") != 0);
  CHECK(!CString(L"OS.unix=symlink:/x").Left(15).CompareNoCase(L"os.UNIX=SYMLINK"));

  CHECK(A == CString(L"abc"));
  CHECK(A == L"abc");
  CHECK(L"abc" == A);
  CHECK(A != CString(L"abd"));
  CHECK(A != L"abd");
  CHECK(L"abd" != A);
  CHECK(A < CString(L"abd"));
  CHECK(A < L"abd");
  CHECK(L"abb" < A);
  CHECK(A > CString(L"abb"));
  CHECK(A > L"abb");
  CHECK(L"abd" > A);
  CHECK(A <= CString(L"abc"));
  CHECK(A <= L"abd");
  CHECK(L"abc" <= A);
  CHECK(A >= CString(L"abc"));
  CHECK(A >= L"abb");
  CHECK(L"abc" >= A);
  CHECK(!(A < A));
}

void TestExtraction()
{
  const CString S(L"abcdef");
  CHECK(Same(S.Mid(2), L"cdef"));
  CHECK(Same(S.Mid(0), L"abcdef"));
  CHECK(Same(S.Mid(6), L""));
  CHECK(Same(S.Mid(7), L""));
  CHECK(Same(S.Mid(-3), L"abcdef"));
  CHECK(Same(S.Mid(2, 3), L"cde"));
  CHECK(Same(S.Mid(2, 100), L"cdef"));
  CHECK(Same(S.Mid(2, 0), L""));
  CHECK(Same(S.Mid(2, -1), L""));
  CHECK(Same(S.Mid(-2, 3), L"abc"));
  CHECK(Same(S.Mid(10, 2), L""));

  CHECK(Same(S.Left(2), L"ab"));
  CHECK(Same(S.Left(0), L""));
  CHECK(Same(S.Left(-1), L""));
  CHECK(Same(S.Left(6), L"abcdef"));
  CHECK(Same(S.Left(99), L"abcdef"));

  CHECK(Same(S.Right(2), L"ef"));
  CHECK(Same(S.Right(0), L""));
  CHECK(Same(S.Right(-1), L""));
  CHECK(Same(S.Right(6), L"abcdef"));
  CHECK(Same(S.Right(99), L"abcdef"));

  CHECK(Same(CString().Mid(0), L""));
  CHECK(Same(CString().Left(3), L""));
  CHECK(Same(CString().Right(3), L""));
}

void TestSearching()
{
  const CString S(L"abcabc");
  CHECK(S.Find(L'b') == 1);
  CHECK(S.Find(L'b', 1) == 1);
  CHECK(S.Find(L'b', 2) == 4);
  CHECK(S.Find(L'b', 5) == -1);
  CHECK(S.Find(L'z') == -1);
  CHECK(S.Find(L'a', -5) == 0);
  CHECK(S.Find(L'a', 6) == -1);
  CHECK(S.Find(L'a', 100) == -1);

  CHECK(S.Find(L"bc") == 1);
  CHECK(S.Find(L"bc", 2) == 4);
  CHECK(S.Find(L"bc", 5) == -1);
  CHECK(S.Find(L"abcabc") == 0);
  CHECK(S.Find(L"abcabcd") == -1);
  CHECK(S.Find(L"zz") == -1);
  CHECK(S.Find(L"ca", -1) == 2);
  CHECK(S.Find(static_cast<const wchar_t *>(nullptr)) == -1);
  CHECK(S.Find(L"") == 0);
  CHECK(S.Find(L"", 3) == 3);
  CHECK(S.Find(L"", 6) == 6);
  CHECK(S.Find(L"", 7) == -1);
  CHECK(S.Find(L"c", 6) == -1);
  CHECK(CString().Find(L'a') == -1);
  CHECK(CString().Find(L"a") == -1);
  CHECK(CString().Find(L"") == 0);
  // Character 0 finds the terminator.
  CHECK(S.Find(L'\0') == 6);
  CHECK(S.Find(L'\0', 5) == 6);
  CHECK(S.Find(L'\0', 6) == -1);
  CHECK(S.ReverseFind(L'\0') == 6);
  CHECK(CString().Find(L'\0') == -1);
  CHECK(CString().ReverseFind(L'\0') == 0);
  // Searches stop at the first embedded null (after Start, for Find).
  const wchar_t Embedded[] = { L'a', L'b', L'\0', L'c', L'b' };
  const CString N(Embedded, 5);
  CHECK(N.GetLength() == 5);
  CHECK(N.Find(L'c') == -1);
  CHECK(N.Find(L'b') == 1);
  CHECK(N.Find(L'b', 3) == 4);
  CHECK(N.Find(L'\0') == 2);
  CHECK(N.Find(L"cb") == -1);
  CHECK(N.Find(L"cb", 3) == 3);
  CHECK(N.Find(L"", 4) == 4);
  CHECK(N.ReverseFind(L'b') == 1);
  CHECK(N.ReverseFind(L'\0') == 2);
  CHECK(N.FindOneOf(L"c") == -1);
  CHECK(N.FindOneOf(L"cb") == 1);

  CHECK(S.ReverseFind(L'a') == 3);
  CHECK(S.ReverseFind(L'c') == 5);
  CHECK(S.ReverseFind(L'z') == -1);
  CHECK(CString().ReverseFind(L'a') == -1);

  CHECK(S.FindOneOf(L"xc") == 2);
  CHECK(S.FindOneOf(L"cb") == 1);
  CHECK(S.FindOneOf(L"xyz") == -1);
  CHECK(S.FindOneOf(L"") == -1);
  CHECK(S.FindOneOf(nullptr) == -1);
}

void TestReplace()
{
  CString S(L"a.b.c");
  CHECK(S.Replace(L'.', L'.') == 0);
  CHECK(Same(S, L"a.b.c"));
  CHECK(S.Replace(L'.', L'/') == 2);
  CHECK(Same(S, L"a/b/c"));
  CHECK(S.Replace(L'x', L'y') == 0);
  CHECK(Same(S, L"a/b/c"));
  CString E;
  CHECK(E.Replace(L'a', L'b') == 0);

  CString T(L"one two one two");
  CHECK(T.Replace(L"one", L"1") == 1);
  CHECK(Same(T, L"1 two 1 two"));
  CHECK(T.Replace(L"two", L"three") == 1);
  CHECK(Same(T, L"1 three 1 three"));
  CHECK(T.Replace(L"zz", L"x") == 0);
  CHECK(Same(T, L"1 three 1 three"));
  CHECK(T.Replace(L" ", nullptr) == 1);
  CHECK(Same(T, L"1three1three"));
  CHECK(T.Replace(L"", L"x") == 0);
  CHECK(T.Replace(nullptr, L"x") == 0);
  CHECK(Same(T, L"1three1three"));

  // Replacing text by itself changes nothing: 0.
  CString Same1(L"xax");
  CHECK(Same1.Replace(L"a", L"a") == 0);
  CHECK(Same(Same1, L"xax"));
  // Non-overlapping, left to right.
  CString O(L"aaaa");
  CHECK(O.Replace(L"aa", L"b") == 1);
  CHECK(Same(O, L"bb"));
  CString P(L"aaa");
  CHECK(P.Replace(L"aa", L"b") == 1);
  CHECK(Same(P, L"ba"));
  // The replacement is not searched again.
  CString Q(L"ab");
  CHECK(Q.Replace(L"a", L"aa") == 1);
  CHECK(Same(Q, L"aab"));
  // Case-sensitive.
  CString R(L"Aa");
  CHECK(R.Replace(L"a", L"x") == 1);
  CHECK(Same(R, L"Ax"));
  // Old taken from the string itself.
  CString Self(L"xyxy");
  CHECK(Self.Replace(Self.GetString() + 2, L"-") == 1);
  CHECK(Same(Self, L"--"));
  // Server paths: \\ -> / as ServerPath does.
  CString W(L"\\dir\\file");
  CHECK(W.Replace(L"\\", L"/") == 1);
  CHECK(Same(W, L"/dir/file"));
}

void TestDeleteInsert()
{
  CString S(L"abcdef");
  CHECK(S.Delete(1, 2) == 6);
  CHECK(Same(S, L"adef"));
  CHECK(S.Delete(3) == 4);
  CHECK(Same(S, L"ade"));
  CHECK(S.Delete(1, 100) == 3);
  CHECK(Same(S, L"a"));
  CString T(L"abc");
  CHECK(T.Delete(5, 1) == 3);
  CHECK(T.Delete(3, 1) == 3);
  CHECK(T.Delete(1, 0) == 3);
  CHECK(T.Delete(1, -2) == 3);
  CHECK(Same(T, L"abc"));
  CHECK(T.Delete(-1, 1) == 3);
  CHECK(Same(T, L"bc"));
  CHECK(T.Delete(T.GetLength() - 1, 1) == 2);
  CHECK(Same(T, L"b"));

  CString I(L"ad");
  CHECK(I.Insert(1, L"bc") == 4);
  CHECK(Same(I, L"abcd"));
  CHECK(I.Insert(0, L'_') == 5);
  CHECK(Same(I, L"_abcd"));
  CHECK(I.Insert(99, L'!') == 6);
  CHECK(Same(I, L"_abcd!"));
  CHECK(I.Insert(-4, L"<") == 7);
  CHECK(Same(I, L"<_abcd!"));
  CHECK(I.Insert(2, nullptr) == 7);
}

void TestCase()
{
  CString S(L"MiXeD 123 \x00C9");
  CHECK(&S.MakeLower() == &S);
  CHECK(Same(S, L"mixed 123 \x00C9"));
  S.MakeUpper();
  CHECK(Same(S, L"MIXED 123 \x00C9"));
  CString E;
  E.MakeLower();
  E.MakeUpper();
  CHECK(E.IsEmpty());
}

void TestTrim()
{
  CString S(L" \t\r\n\v\fx y \t\r\n\v\f");
  S.TrimLeft();
  CHECK(Same(S, L"x y \t\r\n\v\f"));
  S.TrimRight();
  CHECK(Same(S, L"x y"));

  CString C(L"***ab*c***");
  CHECK(&C.TrimLeft(L'*') == &C);
  CHECK(Same(C, L"ab*c***"));
  C.TrimRight(L'*');
  CHECK(Same(C, L"ab*c"));
  C.TrimRight(L'x');
  CHECK(Same(C, L"ab*c"));

  CString Set(L"?!ab!?!");
  Set.TrimRight(L"!?");
  CHECK(Same(Set, L"?!ab"));
  Set.TrimLeft(L"!?");
  CHECK(Same(Set, L"ab"));
  // Characters, not a substring.
  CString Sub(L"abba");
  Sub.TrimRight(L"ab");
  CHECK(Sub.IsEmpty());
  CString None(L"  keep  ");
  None.TrimLeft(static_cast<const wchar_t *>(nullptr));
  None.TrimRight(L"");
  CHECK(Same(None, L"  keep  "));

  CString All(L"   ");
  All.TrimLeft();
  CHECK(All.IsEmpty());
  CString AllR(L"\r\n");
  AllR.TrimRight(L"\r\n");
  CHECK(AllR.IsEmpty());

  CString Both(L"  mid  ");
  Both.Trim();
  CHECK(Same(Both, L"mid"));
  CString BothC(L"--mid--");
  BothC.Trim(L'-');
  CHECK(Same(BothC, L"mid"));
  CString BothS(L"<>mid><");
  BothS.Trim(L"<>");
  CHECK(Same(BothS, L"mid"));
  // Trim leaves inner white space; a non-white character stops it.
  CString Inner(L"\x00A0 a b ");
  Inner.Trim();
  CHECK(Same(Inner, L"\x00A0 a b"));
}

void FormatThrough(CString & S, const wchar_t * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  S.FormatV(Format, Args);
  va_end(Args);
}

void TestFormat()
{
  CString S;
  S.Format(L"%d-%s-%hs-%c", 42, L"wide", "narrow", L'z');
  CHECK(Same(S, L"42-wide-narrow-z"));
  S.Format(L"%02d:%03d", 7, 5);
  CHECK(Same(S, L"07:005"));
  S.Format(L"%I64d", static_cast<__int64>(1234567890123LL));
  CHECK(Same(S, L"1234567890123"));
  S.Format(L"%u %x %%", 7u, 255u);
  CHECK(Same(S, L"7 ff %"));
  S.Format(L"no arguments");
  CHECK(Same(S, L"no arguments"));
  S.Format(L"");
  CHECK(S.IsEmpty());
  // The string's own text as an argument.
  S = L"self";
  S.Format(L"[%s]", S.c_str());
  CHECK(Same(S, L"[self]"));

  FormatThrough(S, L"%s=%d", L"n", 3);
  CHECK(Same(S, L"n=3"));
  FormatThrough(S, nullptr);
  CHECK(S.IsEmpty());

  // A resource format: OPENFILE_ERROR (TextsCore.h 124).
  const unsigned int OpenFileError = 124;
  const UnicodeString Resource = LoadStr(OpenFileError);
  CHECK(!Resource.IsEmpty());
  CString Expected;
  Expected.Format(Resource.c_str(), L"name.txt");
  S.Format(OpenFileError, L"name.txt");
  CHECK(S == Expected);
  CHECK(S.Find(L"name.txt") >= 0);
  // An unknown id has no text: an empty result.
  S = L"not empty";
  S.Format(0x7FFFFFF0u, 1);
  CHECK(S.IsEmpty());
}

// --- CTime -------------------------------------------------------------------

FILETIME FileTimeOf(unsigned __int64 Ticks)
{
  FILETIME F;
  F.dwLowDateTime = static_cast<DWORD>(Ticks & 0xFFFFFFFFu);
  F.dwHighDateTime = static_cast<DWORD>(Ticks >> 32);
  return F;
}

// What CTime(FILETIME) is specified to compute, step by step.
__int64 ThroughLocalTime(unsigned __int64 Ticks)
{
  const FILETIME Utc = FileTimeOf(Ticks);
  FILETIME Local;
  SYSTEMTIME ST;
  if (!FileTimeToLocalFileTime(&Utc, &Local) || !FileTimeToSystemTime(&Local, &ST) || (ST.wYear < 1900))
  {
    return 0;
  }
  struct tm T;
  std::memset(&T, 0, sizeof(T));
  T.tm_year = ST.wYear - 1900;
  T.tm_mon = ST.wMonth - 1;
  T.tm_mday = ST.wDay;
  T.tm_hour = ST.wHour;
  T.tm_min = ST.wMinute;
  T.tm_sec = ST.wSecond;
  T.tm_isdst = -1;
  return static_cast<__int64>(std::mktime(&T));
}

// Local time is UTC (TZ=UTC0 honoured) at two instants six months apart.
bool LocalIsUtc()
{
  for (const time_t Raw : { static_cast<time_t>(951831930), static_cast<time_t>(1768478400) })
  {
    const struct tm * L = std::localtime(&Raw);
    if (L == nullptr)
    {
      return false;
    }
    const int LocalHour = L->tm_hour;
    const struct tm * G = std::gmtime(&Raw);
    if ((G == nullptr) || (G->tm_hour != LocalHour))
    {
      return false;
    }
  }
  return true;
}

void TestTime()
{
  CHECK(CTime().GetTime() == 0);
  CHECK(CTime(12345).GetTime() == 12345);

  // 2000-02-29 13:45:30 UTC, a Tuesday: 951831930 seconds since 1970.
  const __int64 Leap = 951831930;
  const unsigned __int64 Ticks = (static_cast<unsigned __int64>(Leap) * 10000000u) + 116444736000000000u;
  const CTime T(Leap);

  // From a FILETIME: through local time, then mktime with DST left to it.
  const CTime FromFile(FileTimeOf(Ticks));
  CHECK(FromFile.GetTime() == ThroughLocalTime(Ticks));
  // Milliseconds are dropped; the DST argument is not used.
  CHECK(CTime(FileTimeOf(Ticks + 9999999u)).GetTime() == FromFile.GetTime());
  CHECK(CTime(FileTimeOf(Ticks), 1) == FromFile);
  CHECK(CTime(FileTimeOf(Ticks), 0) == FromFile);
  // An all-zero FILETIME is the "no time" value, 0.
  CHECK(CTime(FileTimeOf(0)).GetTime() == 0);
  // A local year before 1900 gives 0: 1601-01-02 and 1899-06-01.
  CHECK(CTime(FileTimeOf(864000000000u)).GetTime() == 0);
  CHECK(CTime(FileTimeOf(94169952000000000u)).GetTime() == 0);
  if (LocalIsUtc())
  {
    CHECK(FromFile.GetTime() == Leap);
    CHECK(CTime(FileTimeOf(116444736000000000u)).GetTime() == 0);
    // One second before 1970: mktime's -1, which is also its failure value.
    CHECK(CTime(FileTimeOf(116444736000000000u - 10000000u)).GetTime() == -1);
    // 2026-01-15 12:00:00 UTC.
    CHECK(CTime(FileTimeOf(134129520000000000u)).GetTime() == 1768478400);
  }
#if !defined(_WIN32)
  // The POSIX layer's local time is the date's own zone offset, so a zone
  // with DST round-trips exactly in both seasons.
  if (FileExists(UnicodeString(L"/usr/share/zoneinfo/Europe/Lisbon")))
  {
    setenv("TZ", "Europe/Lisbon", 1);
    tzset();
    CHECK(CTime(FileTimeOf(134129520000000000u)).GetTime() == 1768478400);
    CHECK(CTime(FileTimeOf(134285904000000000u)).GetTime() == 1784116800);
    setenv("TZ", "UTC0", 1);
    tzset();
  }
#endif

  // As FileTimeToSystemTime sees the same FILETIME.
  SYSTEMTIME ST;
  const FILETIME F = FileTimeOf(Ticks);
  CHECK(FileTimeToSystemTime(&F, &ST));
  CHECK(ST.wYear == 2000 && ST.wMonth == 2 && ST.wDay == 29 && ST.wHour == 13);

  // UTC break-down: exact.
  struct tm G;
  CHECK(T.GetGmtTm(&G) == &G);
  CHECK(G.tm_year == 100 && G.tm_mon == 1 && G.tm_mday == 29);
  CHECK(G.tm_hour == 13 && G.tm_min == 45 && G.tm_sec == 30 && G.tm_wday == 2);
  CHECK(T.GetGmtTm(nullptr) == nullptr);
  CHECK(T.GetLocalTm(nullptr) == nullptr);

  // The getters are local time: as the C runtime's own localtime breaks the
  // same instant down, whatever the zone.
  for (const __int64 Value : { Leap, static_cast<__int64>(0), static_cast<__int64>(1700000000) })
  {
    const CTime V(Value);
    const time_t Raw = static_cast<time_t>(Value);
    const struct tm * RuntimeLocal = std::localtime(&Raw);
    CHECK(RuntimeLocal != nullptr);
    if (RuntimeLocal == nullptr)
    {
      continue;
    }
    const struct tm Expected = *RuntimeLocal;
    struct tm L;
    CHECK(V.GetLocalTm(&L) == &L);
    CHECK(L.tm_year == Expected.tm_year && L.tm_yday == Expected.tm_yday && L.tm_hour == Expected.tm_hour);
    CHECK(V.GetYear() == Expected.tm_year + 1900);
    CHECK(V.GetMonth() == Expected.tm_mon + 1);
    CHECK(V.GetDay() == Expected.tm_mday);
    CHECK(V.GetHour() == Expected.tm_hour);
    CHECK(V.GetMinute() == Expected.tm_min);
    CHECK(V.GetSecond() == Expected.tm_sec);
    CHECK(V.GetDayOfWeek() == Expected.tm_wday + 1);
  }

  // And under TZ=UTC0 (ctest sets it; the Windows runtime under Wine may
  // ignore it) local time is UTC: the documented ranges and numbering.
  struct tm L;
  if ((T.GetLocalTm(&L) != nullptr) && (L.tm_hour == G.tm_hour))
  {
    CHECK(T.GetYear() == 2000);
    CHECK(T.GetMonth() == 2);
    CHECK(T.GetDay() == 29);
    CHECK(T.GetHour() == 13);
    CHECK(T.GetMinute() == 45);
    CHECK(T.GetSecond() == 30);
    CHECK(T.GetDayOfWeek() == 3);
    CHECK(CTime(0).GetYear() == 1970);
    CHECK(CTime(0).GetDayOfWeek() == 5);  // a Thursday
  }
  else
  {
    std::printf("note: local time is not UTC here; exact local values not checked\n");
  }

  // Comparison, including against a plain number as the engine does
  // (status.m_mtime != -1).
  const CTime A(100), B(200);
  CHECK(A == CTime(100));
  CHECK(A != B);
  CHECK(A < B);
  CHECK(B > A);
  CHECK(A <= A && A <= B);
  CHECK(B >= B && B >= A);
  CHECK(A != -1);
  CHECK(CTime(-1) == -1);
  CTime C;
  C = 77;
  CHECK(C.GetTime() == 77);
  C = A;
  CHECK(C == A);

  // The current time, against the C runtime's.
  const __int64 Before = static_cast<__int64>(std::time(nullptr));
  const CTime Now = CTime::CreateForCurrentTime();
  const __int64 After = static_cast<__int64>(std::time(nullptr));
  CHECK(Now.GetTime() >= Before && Now.GetTime() <= After);
}

// --- CFile -------------------------------------------------------------------

const wchar_t * const FileName = L"test_mfc_compat.tmp";
const wchar_t * const MissingName = L"test_mfc_compat-missing.tmp";

CString ReadAll()
{
  CFile F;
  if (!F.Open(FileName, CFile::modeRead | CFile::shareDenyNone))
  {
    return CString(L"<cannot open>");
  }
  char Buffer[256];
  const unsigned int N = F.Read(Buffer, sizeof(Buffer));
  return CString(Buffer, static_cast<int>(N));
}

void TestFile()
{
  DeleteFile(FileName);
  DeleteFile(MissingName);

  CHECK(CFile::hFileNull == INVALID_HANDLE_VALUE);
  {
    CFile F;
    CHECK(F.m_hFile == CFile::hFileNull);
    CHECK(static_cast<HANDLE>(F) == CFile::hFileNull);
    // Nothing attached: Close and Abort do nothing.
    F.Close();
    F.Abort();
    CHECK(F.m_hFile == CFile::hFileNull);
  }

  // Opening a missing file for reading fails, with the system's last error,
  // which the engine reports through LastSysErrorMessage.
  {
    CFile F;
    SetLastError(0);
    CHECK(!F.Open(MissingName, CFile::modeRead | CFile::shareDenyNone));
    const DWORD Error = GetLastError();
    CHECK((Error == ERROR_FILE_NOT_FOUND) || (Error == ERROR_PATH_NOT_FOUND));
    CHECK(F.m_hFile == CFile::hFileNull);
    CHECK(!F.Open(MissingName, CFile::modeWrite | CFile::shareDenyWrite));
    CHECK(!F.Open(nullptr, CFile::modeRead));
  }
  // The opening constructor raises instead.
  CHECK(RaisesOS([] { CFile F(MissingName, CFile::modeRead); }));

  // modeCreate: created, and truncated when it exists.
  {
    CFile F;
    // The name as expanded when Open is called: the file does not exist yet,
    // which POSIX's ExpandFileName (realpath) leaves relative.
    const UnicodeString OpenText = UnicodeString(L"\r\n") + ExpandFileName(FileName);
    CHECK(F.Open(FileName, CFile::modeWrite | CFile::modeCreate | CFile::shareDenyWrite));
    CHECK(F.m_hFile != CFile::hFileNull);
    F.Write("hello world", 11);
    F.Write("", 0);
    CHECK(F.GetPosition() == 11);
    CHECK(F.GetLength() == 11);
    F.Flush();
    // Write-only: reading raises.
    char B[4];
    CHECK(RaisesOS([&] { F.Read(B, sizeof(B)); }));
    // The message is RaiseLastOSError's, ending with the expanded file name.
    const UnicodeString Message = OSMessage([&] { F.Read(B, sizeof(B)); });
    CHECK(Message.Pos(L"System Error.  Code: ") == 1);
    CHECK(EndsWith(Message, OpenText));
    F.Close();
    CHECK(F.m_hFile == CFile::hFileNull);
    F.Close();
  }
  CHECK(Same(ReadAll(), L"hello world"));
  {
    CFile F;
    CHECK(F.Open(FileName, CFile::modeWrite | CFile::modeCreate | CFile::shareDenyWrite));
    CHECK(F.GetLength() == 0);
    F.Write("abc", 3);
  }  // the destructor closes
  CHECK(Same(ReadAll(), L"abc"));

  // modeCreate | modeNoTruncate: the content stays; the engine resumes a
  // download by seeking to the end.
  {
    CFile F;
    CHECK(F.Open(FileName, CFile::modeCreate | CFile::modeWrite | CFile::modeNoTruncate | CFile::shareDenyWrite));
    CHECK(F.GetLength() == 3);
    CHECK(F.GetPosition() == 0);
    CHECK(F.SeekToEnd() == 3);
    F.Write("def", 3);
  }
  CHECK(Same(ReadAll(), L"abcdef"));
  // modeCreate | modeNoTruncate also creates.
  {
    CFile F;
    CHECK(F.Open(MissingName, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite));
    CHECK(F.GetLength() == 0);
  }
  DeleteFile(MissingName);
  // Without modeCreate a missing file is not created, modeNoTruncate or not.
  {
    CFile F;
    CHECK(!F.Open(MissingName, CFile::modeWrite | CFile::modeNoTruncate));
  }

  // Reading: sequential reads, a short read at the end, then 0.
  {
    CFile F(FileName, CFile::modeRead | CFile::shareDenyNone);
    char B[8];
    std::memset(B, 0, sizeof(B));
    CHECK(F.Read(B, 4) == 4);
    CHECK(std::memcmp(B, "abcd", 4) == 0);
    CHECK(F.Read(B, 8) == 2);
    CHECK(std::memcmp(B, "ef", 2) == 0);
    CHECK(F.Read(B, 8) == 0);
    CHECK(F.Read(B, 0) == 0);
    // Seek from each origin.
    CHECK(F.Seek(1, CFile::begin) == 1);
    CHECK(F.Read(B, 1) == 1 && B[0] == 'b');
    CHECK(F.Seek(2, CFile::current) == 4);
    CHECK(F.Read(B, 1) == 1 && B[0] == 'e');
    CHECK(F.Seek(-3, CFile::end) == 3);
    CHECK(F.Read(B, 1) == 1 && B[0] == 'd');
    F.SeekToBegin();
    CHECK(F.GetPosition() == 0);
    // Beyond the end is allowed; reading there gives nothing.
    CHECK(F.Seek(100, CFile::begin) == 100);
    CHECK(F.Read(B, 1) == 0);
    // Before the start is an error.
    CHECK(RaisesOS([&] { F.Seek(-1, CFile::begin); }));
    // Read-only: writing raises.
    CHECK(RaisesOS([&] { F.Write("x", 1); }));
    CHECK(EndsWith(OSMessage([&] { F.Write("x", 1); }), UnicodeString(L"\r\n") + ExpandFileName(FileName)));
    // Open on an open object replaces its file.
    CHECK(F.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
    CHECK(F.GetPosition() == 0);
    F.Abort();
    CHECK(F.m_hFile == CFile::hFileNull);
  }

  // modeReadWrite.
  {
    CFile F(FileName, CFile::modeReadWrite | CFile::shareExclusive);
    char B[2];
    CHECK(F.Read(B, 2) == 2 && B[0] == 'a');
    F.Write("XY", 2);
    F.SeekToBegin();
    char C[6];
    CHECK(F.Read(C, 6) == 6);
    CHECK(std::memcmp(C, "abXYef", 6) == 0);
  }

  // Taking over a handle: the object closes it.
  {
    const HANDLE H = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(H != INVALID_HANDLE_VALUE);
    CFile F(H);
    CHECK(F.m_hFile == H);
    CHECK(F.GetLength() == 6);
  }

  // Operations on no file raise.
  {
    CFile F;
    char B[1];
    CHECK(RaisesOS([&] { F.Read(B, 1); }));
    CHECK(RaisesOS([&] { F.Write("x", 1); }));
    CHECK(RaisesOS([&] { F.GetLength(); }));
    F.Flush();  // nothing to flush
  }

#if defined(_WIN32)
  // Share modes, where the system enforces them.
  {
    CFile Writer;
    CHECK(Writer.Open(FileName, CFile::modeWrite | CFile::shareDenyWrite));
    CFile Other;
    CHECK(Other.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
    Other.Close();
    CHECK(!Other.Open(FileName, CFile::modeWrite | CFile::shareDenyNone));
    CHECK(GetLastError() == ERROR_SHARING_VIOLATION);
  }
  {
    CFile Exclusive;
    CHECK(Exclusive.Open(FileName, CFile::modeRead | CFile::shareExclusive));
    CFile Other;
    CHECK(!Other.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
  }
#endif

  // The remembered text: set by every Open, failed or not; forgotten by Close.
  {
    CFile F;
    CHECK(!F.Open(MissingName, CFile::modeRead));
    char B[1];
    const UnicodeString AfterFailedOpen = OSMessage([&] { F.Read(B, 1); });
    CHECK(EndsWith(AfterFailedOpen, UnicodeString(L"\r\n") + ExpandFileName(MissingName)));
    F.Close();
    const UnicodeString AfterClose = OSMessage([&] { F.Read(B, 1); });
    CHECK(AfterClose.Pos(L"System Error.  Code: ") == 1);
    CHECK(AfterClose.Pos(ExpandFileName(MissingName)) == 0);
    // A handle taken over has no name.
    CFile H(INVALID_HANDLE_VALUE);
    CHECK(OSMessage([&] { H.Read(B, 1); }).Pos(L"\r\n\r\n") == 0);
  }

  // Close: detaches and forgets first, then raises with the remembered name.
  {
    CFile F;
    CHECK(F.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
    CHECK(CloseHandle(F.m_hFile));  // behind its back
    const UnicodeString Message = OSMessage([&] { F.Close(); });
    CHECK(Message.Pos(L"System Error.  Code: ") == 1);
    CHECK(EndsWith(Message, UnicodeString(L"\r\n") + ExpandFileName(FileName)));
    CHECK(F.m_hFile == CFile::hFileNull);
    F.Close();
  }

  // The destructor does not raise; it counts the failure.
  {
    const unsigned long Before = CFile::DestructorCloseFailures();
    {
      CFile F;
      CHECK(F.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
      CHECK(CloseHandle(F.m_hFile));
    }
    CHECK(CFile::DestructorCloseFailures() == Before + 1);
    {
      CFile F;
      CHECK(F.Open(FileName, CFile::modeRead | CFile::shareDenyNone));
    }
    CHECK(CFile::DestructorCloseFailures() == Before + 1);
  }

#if !defined(_WIN32)
  // A full disk: the code and the name in the message.
  if (FileExists(UnicodeString(L"/dev/full")))
  {
    CFile F;
    CHECK(F.Open(L"/dev/full", CFile::modeWrite));
    const UnicodeString Message = OSMessage([&] { F.Write("data", 4); });
    CHECK(Message.Pos(L"System Error.  Code: 112.") == 1);
    CHECK(Message.Pos(SysErrorMessage(ERROR_DISK_FULL)) > 0);
    CHECK(EndsWith(Message, UnicodeString(L"\r\n/dev/full")));
  }
#endif

  CHECK(DeleteFile(FileName));
}

}  // namespace

int main()
{
  TestConstruction();
  TestAssignment();
  TestAccess();
  TestConcatenation();
  TestComparison();
  TestExtraction();
  TestSearching();
  TestReplace();
  TestDeleteInsert();
  TestCase();
  TestTrim();
  TestFormat();
  TestTime();
  TestFile();
  if (Failures != 0)
  {
    std::printf("%d failure(s)\n", Failures);
    return 1;
  }
  std::printf("all mfc_compat tests passed\n");
  return 0;
}
