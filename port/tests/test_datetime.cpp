// TDateTime tests.
//
// Anchored on the example values in Delphi's own TDateTime documentation, which
// is the authoritative statement of the encoding:
//       0      1899-12-30 00:00
//       2.75   1900-01-01 18:00
//      -1.25   1899-12-29 06:00
//   35065      1996-01-01 00:00
//
// The negative case is the one that matters most: Delphi treats the fractional
// part as a positive time of day even when the date is negative. Core stores
// file timestamps in these, so a sign error would shift times rather than fail.
#include "vcl_datetime.h"

#include <cmath>
#include <cstdio>
#include <type_traits>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

bool Fmt(const TDateTime & DT, const wchar_t * Format, const wchar_t * Expected)
{
  const UnicodeString Got = FormatDateTime(UnicodeString(Format), DT);
  if (Got != UnicodeString(Expected))
  {
    std::printf("  format '%ls' mismatch\n", Format);
    return false;
  }
  return true;
}

void CheckDecode(double Serial, int Y, int Mo, int D, int H, int Mi, int S)
{
  Word yy = 0, mm = 0, dd = 0, hh = 0, nn = 0, ss = 0, zz = 0;
  TDateTime DT(Serial);
  DecodeDate(DT, yy, mm, dd);
  DecodeTime(DT, hh, nn, ss, zz);
  if (yy != Y || mm != Mo || dd != D || hh != H || nn != Mi || ss != S)
  {
    std::printf("FAIL serial %.6f -> %04d-%02d-%02d %02d:%02d:%02d "
                "(expected %04d-%02d-%02d %02d:%02d:%02d)\n",
                Serial, yy, mm, dd, hh, nn, ss, Y, Mo, D, H, Mi, S);
    ++Failures;
  }
}

void TestDocumentedAnchors()
{
  CheckDecode(0.0,      1899, 12, 30,  0, 0, 0);
  CheckDecode(2.75,     1900,  1,  1, 18, 0, 0);
  CheckDecode(-1.25,    1899, 12, 29,  6, 0, 0);   // negative: time stays positive
  CheckDecode(35065.0,  1996,  1,  1,  0, 0, 0);
  CheckDecode(1.0,      1899, 12, 31,  0, 0, 0);
  CheckDecode(2.0,      1900,  1,  1,  0, 0, 0);

  // and the same anchors in the encoding direction
  CHECK(EncodeDate(1899, 12, 30).Val() == 0.0);
  CHECK(EncodeDate(1899, 12, 31).Val() == 1.0);
  CHECK(EncodeDate(1900,  1,  1).Val() == 2.0);
  CHECK(EncodeDate(1996,  1,  1).Val() == 35065.0);
  CHECK(std::fabs(EncodeTime(18, 0, 0, 0).Val() - 0.75) < 1e-12);
  CHECK(std::fabs(EncodeTime(6, 0, 0, 0).Val() - 0.25) < 1e-12);
}

void TestRoundTrip()
{
  // Exercise a spread of dates including pre-epoch, leap days and a far future.
  const int Cases[][6] = {
    { 1600,  2, 29, 23, 59, 59 },   // leap year, divisible by 400
    { 1899, 12, 29,  6,  0,  0 },
    { 1900,  2, 28, 12, 30, 15 },   // 1900 is not a leap year
    { 1970,  1,  1,  0,  0,  0 },
    { 2000,  2, 29,  1,  2,  3 },
    { 2024,  2, 29, 13, 45, 56 },
    { 2038,  1, 19,  3, 14,  7 },   // 32-bit time_t rollover
    { 2100,  3,  1,  0,  0,  1 },
    { 9999, 12, 31, 23, 59, 59 },
  };
  for (const auto & C : Cases)
  {
    const TDateTime DT = EncodeDateTime(
      static_cast<Word>(C[0]), static_cast<Word>(C[1]), static_cast<Word>(C[2]),
      static_cast<Word>(C[3]), static_cast<Word>(C[4]), static_cast<Word>(C[5]), 0);
    CheckDecode(DT.Val(), C[0], C[1], C[2], C[3], C[4], C[5]);
  }
}

void TestMillisecondPrecision()
{
  // The fraction cannot hold every millisecond exactly, so decoding rounds.
  // Truncating instead would drift every timestamp down by a millisecond.
  for (int Ms = 0; Ms < 1000; Ms += 37)
  {
    TDateTime T = EncodeTime(12, 34, 56, static_cast<Word>(Ms));
    Word h = 0, n = 0, s = 0, z = 0;
    DecodeTime(T, h, n, s, z);
    CHECK(h == 12 && n == 34 && s == 56);
    if (z != Ms)
    {
      std::printf("FAIL ms round trip: %d -> %d\n", Ms, z);
      ++Failures;
    }
  }
  // A fraction just under a full day must not decode as hour 24.
  Word h = 0, n = 0, s = 0, z = 0;
  DecodeTime(TDateTime(0.9999999999), h, n, s, z);
  CHECK(h <= 23);
}

void TestValidation()
{
  TDateTime D;
  CHECK(!TryEncodeDate(2023, 2, 29, D));     // not a leap year
  CHECK(TryEncodeDate(2024, 2, 29, D));      // leap year
  CHECK(!TryEncodeDate(2023, 13, 1, D));     // month out of range
  CHECK(!TryEncodeDate(2023, 0, 1, D));
  CHECK(!TryEncodeDate(2023, 1, 32, D));     // day out of range
  CHECK(!TryEncodeDate(2023, 4, 31, D));     // April has 30 days
  CHECK(!TryEncodeTime(24, 0, 0, 0, D));
  CHECK(!TryEncodeTime(0, 60, 0, 0, D));
  CHECK(!TryEncodeTime(0, 0, 0, 1000, D));
  CHECK(TryEncodeTime(23, 59, 59, 999, D));
}

void TestLeapYearsAndMonthLengths()
{
  CHECK(IsLeapYear(2000));        // divisible by 400
  CHECK(!IsLeapYear(1900));       // divisible by 100 but not 400
  CHECK(IsLeapYear(2024));
  CHECK(!IsLeapYear(2023));
  CHECK(DaysInMonth(2024, 2) == 29);
  CHECK(DaysInMonth(2023, 2) == 28);
  CHECK(DaysInMonth(2023, 4) == 30);
  CHECK(DaysInMonth(2023, 12) == 31);
  CHECK(DaysInMonth(2023, 0) == 0);
  CHECK(DaysInMonth(2023, 13) == 0);
}

void TestArithmetic()
{
  TDateTime Jan31 = EncodeDate(2023, 1, 31);
  Word y = 0, m = 0, d = 0;

  // Delphi clamps the day rather than producing an invalid date.
  DecodeDate(IncMonth(Jan31, 1), y, m, d);
  CHECK(y == 2023 && m == 2 && d == 28);
  DecodeDate(IncMonth(EncodeDate(2024, 1, 31), 1), y, m, d);
  CHECK(y == 2024 && m == 2 && d == 29);      // leap year clamps to 29

  DecodeDate(IncMonth(Jan31, -1), y, m, d);
  CHECK(y == 2022 && m == 12 && d == 31);     // crossing backwards over a year
  DecodeDate(IncMonth(EncodeDate(2023, 12, 15), 1), y, m, d);
  CHECK(y == 2024 && m == 1 && d == 15);

  DecodeDate(IncYear(EncodeDate(2024, 2, 29), 1), y, m, d);
  CHECK(y == 2025 && m == 2 && d == 28);      // leap day in a non-leap year

  DecodeDate(IncDay(EncodeDate(2023, 12, 31), 1), y, m, d);
  CHECK(y == 2024 && m == 1 && d == 1);
  DecodeDate(IncDay(EncodeDate(2024, 1, 1), -1), y, m, d);
  CHECK(y == 2023 && m == 12 && d == 31);

  // IncMonth preserves the time of day.
  const TDateTime WithTime = EncodeDateTime(2023, 1, 15, 10, 30, 0, 0);
  Word h = 0, n = 0, s = 0, z = 0;
  DecodeTime(IncMonth(WithTime, 1), h, n, s, z);
  CHECK(h == 10 && n == 30);
}

void TestDayOfWeek()
{
  // 1 = Sunday, as in Delphi. Serial 0 (1899-12-30) was a Saturday.
  CHECK(DayOfWeek(TDateTime(0.0)) == 7);
  CHECK(DayOfWeek(EncodeDate(1899, 12, 31)) == 1);   // Sunday
  CHECK(DayOfWeek(EncodeDate(2024, 1, 1)) == 2);     // Monday
  CHECK(DayOfWeek(EncodeDate(2024, 1, 6)) == 7);     // Saturday
  CHECK(DayOfWeek(EncodeDate(2000, 1, 1)) == 7);     // Saturday
}

void TestNegativeComposition()
{
  // Delphi's documented anchor: 1899-12-29 06:00 is -1.25, not -0.75.
  // Plain addition gives -0.75, which decodes to 1899-12-30 18:00 -- a day late
  // at the mirrored time. ComposeDateTime exists to prevent that.
  const TDateTime Neg = EncodeDateTime(1899, 12, 29, 6, 0, 0, 0);
  CHECK(std::fabs(Neg.Val() - (-1.25)) < 1e-12);
  CheckDecode(Neg.Val(), 1899, 12, 29, 6, 0, 0);

  const TDateTime Naive(EncodeDate(1899, 12, 29).Val() +
                        EncodeTime(6, 0, 0, 0).Val());
  CHECK(std::fabs(Naive.Val() - (-0.75)) < 1e-12);   // the trap, pinned down

  // Positive dates are unaffected, so the two agree there.
  CHECK(ComposeDateTime(EncodeDate(2024, 1, 1), EncodeTime(6, 0, 0, 0)).Val() ==
        EncodeDate(2024, 1, 1).Val() + EncodeTime(6, 0, 0, 0).Val());
}

// Exact to the millisecond, on both sides of day zero.
bool IsMoment(const TDateTime & DT, int Y, int Mo, int D, int H, int Mi, int S,
              int MS)
{
  Word yy = 0, mm = 0, dd = 0, hh = 0, nn = 0, ss = 0, zz = 0;
  DecodeDate(DT, yy, mm, dd);
  DecodeTime(DT, hh, nn, ss, zz);
  return yy == Y && mm == Mo && dd == D && hh == H && nn == Mi && ss == S &&
         zz == MS;
}

void TestIncrementsAreLinear()
{
  const TDateTime NewYear = EncodeDateTime(2024, 1, 1, 0, 0, 0, 0);
  CHECK(IsMoment(IncSecond(NewYear, -1), 2023, 12, 31, 23, 59, 59, 0));
  CHECK(IsMoment(IncMilliSecond(NewYear, -1), 2023, 12, 31, 23, 59, 59, 999));
  CHECK(IsMoment(IncMinute(NewYear, 90), 2024, 1, 1, 1, 30, 0, 0));
  CHECK(IsMoment(IncHour(NewYear, 25), 2024, 1, 2, 1, 0, 0, 0));
  CHECK(IsMoment(IncSecond(NewYear), 2024, 1, 1, 0, 0, 1, 0));  // default 1

  // Before day zero the serial runs backwards within a day. Float addition of
  // one millisecond here moves time *back*; the linear route crosses forward.
  const TDateTime LastMs = EncodeDateTime(1899, 12, 29, 23, 59, 59, 999);
  CHECK(IsMoment(IncMilliSecond(LastMs, 1), 1899, 12, 30, 0, 0, 0, 0));
  const TDateTime Naive(LastMs.Val() + 1.0 / 86400000.0);
  CHECK(!IsMoment(Naive, 1899, 12, 30, 0, 0, 0, 0));  // the trap, pinned down

  const TDateTime SixAm = EncodeDateTime(1899, 12, 29, 6, 0, 0, 0);  // -1.25
  CHECK(IsMoment(IncHour(SixAm, 1), 1899, 12, 29, 7, 0, 0, 0));
  CHECK(IsMoment(IncHour(SixAm, -7), 1899, 12, 28, 23, 0, 0, 0));
  CHECK(IsMoment(IncHour(SixAm, 18), 1899, 12, 30, 0, 0, 0, 0));
}

void TestMilliSecondOf()
{
  const TDateTime DT = EncodeDateTime(2024, 3, 5, 13, 14, 15, 678);
  CHECK(MilliSecondOfTheSecond(DT) == 678);
  CHECK(MilliSecondOfTheMinute(DT) == 15678u);
  CHECK(MilliSecondOfTheHour(DT) == (14 * 60 + 15) * 1000u + 678u);
  CHECK(MilliSecondOfTheDay(DT) == ((13 * 60 + 14) * 60 + 15) * 1000u + 678u);
  // 2024 is a leap year: 5 March is day 65.
  CHECK(MilliSecondOfTheYear(DT) ==
        64LL * 86400000LL + ((13 * 60 + 14) * 60 + 15) * 1000LL + 678LL);

  // Negative serials report a positive time of day, as everywhere else.
  CHECK(MilliSecondOfTheDay(EncodeDateTime(1899, 12, 29, 6, 0, 0, 0)) ==
        6u * 60 * 60 * 1000);
}

// Delphi's types, because core's truncation idiom depends on them.
static_assert(std::is_same<decltype(MilliSecondOfTheSecond(TDateTime())), Word>::value, "");
static_assert(std::is_same<decltype(MilliSecondOfTheDay(TDateTime())), Cardinal>::value, "");
static_assert(std::is_signed<decltype(MilliSecondOfTheYear(TDateTime()))>::value,
              "core negates this without a cast");

void TestCoreTruncationIdiom()
{
  // Exactly the expressions in Common.cpp's relative-date parser.
  const TDateTime DT = EncodeDateTime(2024, 3, 5, 13, 14, 15, 678);
  CHECK(IsMoment(IncMilliSecond(DT, -static_cast<int>(MilliSecondOfTheSecond(DT))),
                 2024, 3, 5, 13, 14, 15, 0));
  CHECK(IsMoment(IncMilliSecond(DT, -static_cast<int>(MilliSecondOfTheMinute(DT))),
                 2024, 3, 5, 13, 14, 0, 0));
  CHECK(IsMoment(IncMilliSecond(DT, -static_cast<int>(MilliSecondOfTheHour(DT))),
                 2024, 3, 5, 13, 0, 0, 0));
  CHECK(IsMoment(IncMilliSecond(DT, -static_cast<int>(MilliSecondOfTheDay(DT))),
                 2024, 3, 5, 0, 0, 0, 0));
  CHECK(IsMoment(IncMilliSecond(DT, -MilliSecondOfTheYear(DT)),
                 2024, 1, 1, 0, 0, 0, 0));
}

void TestBetween()
{
  TDateTime A = EncodeDate(2024, 1, 1);
  const TDateTime B = ComposeDateTime(A, EncodeTime(1, 0, 0, 0));
  CHECK(MilliSecondsBetween(A, B) == 3600000);
  CHECK(SecondsBetween(A, B) == 3600);
  CHECK(MilliSecondsBetween(B, A) == 3600000);       // order-independent
  CHECK(DaysBetween(A, EncodeDate(2024, 1, 11)) == 10);
  CHECK(SecondsBetween(A, A) == 0);
}

void TestFormatDateTime()
{
  const TDateTime DT = EncodeDateTime(2024, 3, 5, 14, 7, 9, 42);

  CHECK(Fmt(DT, L"yyyy-mm-dd", L"2024-03-05"));
  CHECK(Fmt(DT, L"yyyy", L"2024"));
  CHECK(Fmt(DT, L"yy", L"24"));
  CHECK(Fmt(DT, L"m", L"3"));
  CHECK(Fmt(DT, L"mm", L"03"));
  CHECK(Fmt(DT, L"mmm", L"Mar"));
  CHECK(Fmt(DT, L"mmmm", L"March"));
  CHECK(Fmt(DT, L"d", L"5"));
  CHECK(Fmt(DT, L"dd", L"05"));
  CHECK(Fmt(DT, L"ddd", L"Tue"));
  CHECK(Fmt(DT, L"dddd", L"Tuesday"));
  CHECK(Fmt(DT, L"hh:nn:ss", L"14:07:09"));
  CHECK(Fmt(DT, L"h:n:s", L"14:7:9"));
  CHECK(Fmt(DT, L"zzz", L"042"));
  CHECK(Fmt(DT, L"hhnnss", L"140709"));
  CHECK(Fmt(DT, L"yyyymmdd-hhnnss", L"20240305-140709"));

  // Quoted literals, including the forms core actually uses.
  CHECK(Fmt(DT, L"yyyy'-'mm'-'dd'T'hh':'nn':'ss'.'zzz'Z'",
            L"2024-03-05T14:07:09.042Z"));
  CHECK(Fmt(DT, L"ddd, d mmm yyyy hh:nn:ss 'GMT'",
            L"Tue, 5 Mar 2024 14:07:09 GMT"));
  CHECK(Fmt(DT, L"yyyy-mm-dd hh:nn:ss.zzz", L"2024-03-05 14:07:09.042"));

  // Locale-driven specifiers resolve through TFormatSettings.
  CHECK(Fmt(DT, L"ddddd", L"2024-03-05"));           // ShortDateFormat
  CHECK(Fmt(DT, L"t", L"14:07"));                    // ShortTimeFormat
  CHECK(Fmt(DT, L"tt", L"14:07:09"));                // LongTimeFormat
  CHECK(Fmt(DT, L"dddddd", L"Tuesday, 5 March 2024"));

  CHECK(Fmt(TDateTime(0.0), L"yyyy-mm-dd", L"1899-12-30"));
  CHECK(Fmt(DT, L"", L""));
}

void TestStrToDateTime()
{
  TDateTime DT;
  CHECK(TryStrToDateTime(UnicodeString(L"2024-03-05 14:07:09.042"), DT));
  CHECK(FormatDateTime(UnicodeString(L"yyyy-mm-dd hh:nn:ss.zzz"), DT) ==
        UnicodeString(L"2024-03-05 14:07:09.042"));

  CHECK(TryStrToDateTime(UnicodeString(L"2024-03-05"), DT));
  CHECK(FormatDateTime(UnicodeString(L"yyyy-mm-dd"), DT) ==
        UnicodeString(L"2024-03-05"));

  CHECK(!TryStrToDateTime(UnicodeString(L"not a date"), DT));
  CHECK(!TryStrToDateTime(UnicodeString(L"2023-02-29"), DT));   // invalid day
  CHECK(!TryStrToDateTime(UnicodeString(L""), DT));
}

void TestNowIsSane()
{
  // Cannot assert an exact value, but it must land in a plausible range and
  // Date()/Time() must agree with it.
  TDateTime N = Now();
  Word y = 0, m = 0, d = 0;
  DecodeDate(N, y, m, d);
  CHECK(y >= 2024 && y < 2100);
  CHECK(m >= 1 && m <= 12);
  CHECK(d >= 1 && d <= 31);
  CHECK(std::fabs(Date().Val() - std::trunc(N.Val())) < 2.0);
  CHECK(Time().Val() >= 0.0 && Time().Val() < 1.0);
}

}  // namespace

int main()
{
  TestDocumentedAnchors();
  TestRoundTrip();
  TestMillisecondPrecision();
  TestValidation();
  TestLeapYearsAndMonthLengths();
  TestArithmetic();
  TestDayOfWeek();
  TestNegativeComposition();
  TestIncrementsAreLinear();
  TestMilliSecondOf();
  TestCoreTruncationIdiom();
  TestBetween();
  TestFormatDateTime();
  TestStrToDateTime();
  TestNowIsSane();

  if (Failures == 0)
  {
    std::printf("all TDateTime tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
