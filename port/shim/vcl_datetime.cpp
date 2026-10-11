#include "vcl_datetime.h"

#include <chrono>
#include <cmath>
#include <cstdio>

namespace {

constexpr double MSecsPerDay = 86400000.0;

// Howard Hinnant's civil-date algorithms: exact for the whole proleptic
// Gregorian range, which matters because Delphi dates predate 1900.
long long DaysFromCivil(int Y, unsigned M, unsigned D)
{
  Y -= M <= 2;
  const int Era = (Y >= 0 ? Y : Y - 399) / 400;
  const unsigned YoE = static_cast<unsigned>(Y - Era * 400);
  const unsigned DoY = (153u * (M + (M > 2 ? -3 : 9)) + 2u) / 5u + D - 1u;
  const unsigned DoE = YoE * 365u + YoE / 4u - YoE / 100u + DoY;
  return static_cast<long long>(Era) * 146097LL +
         static_cast<long long>(DoE) - 719468LL;
}

void CivilFromDays(long long Z, int & Y, unsigned & M, unsigned & D)
{
  Z += 719468;
  const long long Era = (Z >= 0 ? Z : Z - 146096) / 146097;
  const unsigned DoE = static_cast<unsigned>(Z - Era * 146097);
  const unsigned YoE = (DoE - DoE / 1460u + DoE / 36524u - DoE / 146096u) / 365u;
  const long long Yr = static_cast<long long>(YoE) + Era * 400;
  const unsigned DoY = DoE - (365u * YoE + YoE / 4u - YoE / 100u);
  const unsigned Mp = (5u * DoY + 2u) / 153u;
  D = DoY - (153u * Mp + 2u) / 5u + 1u;
  M = Mp + (Mp < 10u ? 3u : -9u);
  Y = static_cast<int>(Yr + (M <= 2u));
}

// Split a serial into its Delphi parts: whole days, and a *positive* time of
// day regardless of sign. See the header note.
void SplitSerial(double V, long long & Days, double & DayFraction)
{
  const double T = std::trunc(V);
  Days = static_cast<long long>(T);
  DayFraction = std::fabs(V - T);
}

constexpr long long MSecsPerDayInt = 86400000LL;

// A serial as signed milliseconds from 1899-12-30 00:00. Linear and monotonic
// in real time, unlike the serial itself; -0.25 and 0.25 both denote 06:00 on
// day zero, and both map to the same count.
long long SerialToMSecs(double V)
{
  long long Days = 0;
  double Frac = 0.0;
  SplitSerial(V, Days, Frac);
  return Days * MSecsPerDayInt + std::llround(Frac * MSecsPerDay);
}

double MSecsToSerial(long long MSecs)
{
  // Floor division, so the time of day comes out non-negative for any sign.
  long long Days = MSecs / MSecsPerDayInt;
  long long Rest = MSecs % MSecsPerDayInt;
  if (Rest < 0)
  {
    Rest += MSecsPerDayInt;
    --Days;
  }
  const double Time = static_cast<double>(Rest) / MSecsPerDay;
  const double Date = static_cast<double>(Days);
  return (Date < 0) ? (Date - Time) : (Date + Time);
}

long long MSecsOfDay(const TDateTime & DT)
{
  long long Rest = SerialToMSecs(DT.Val()) % MSecsPerDayInt;
  return (Rest < 0) ? (Rest + MSecsPerDayInt) : Rest;
}

UnicodeString Pad(int Value, int Width)
{
  wchar_t Buf[16];
  // Formatted through char to avoid libc's wide printf under -fshort-wchar.
  char Tmp[16];
  std::snprintf(Tmp, sizeof(Tmp), "%0*d", Width, Value);
  int I = 0;
  for (; Tmp[I] != '\0' && I < 15; ++I)
  {
    Buf[I] = static_cast<wchar_t>(Tmp[I]);
  }
  Buf[I] = L'\0';
  return UnicodeString(Buf);
}

const wchar_t * const ShortDayNames[] =
  { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
const wchar_t * const LongDayNames[] =
  { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday",
    L"Saturday" };
const wchar_t * const ShortMonthNames[] =
  { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
    L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
const wchar_t * const LongMonthNames[] =
  { L"January", L"February", L"March", L"April", L"May", L"June",
    L"July", L"August", L"September", L"October", L"November", L"December" };

}  // namespace

TFormatSettings & TFormatSettings::Default()
{
  static TFormatSettings Settings;
  return Settings;
}

bool IsLeapYear(Word Year)
{
  return (Year % 4 == 0) && ((Year % 100 != 0) || (Year % 400 == 0));
}

Word DaysInMonth(Word Year, Word Month)
{
  static const Word Days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (Month < 1 || Month > 12)
  {
    return 0;
  }
  if (Month == 2 && IsLeapYear(Year))
  {
    return 29;
  }
  return Days[Month - 1];
}

bool TryEncodeDate(Word Year, Word Month, Word Day, TDateTime & Date)
{
  if (Month < 1 || Month > 12 || Day < 1 || Day > DaysInMonth(Year, Month))
  {
    return false;
  }
  const long long Days = DaysFromCivil(Year, Month, Day) + DelphiEpochToUnixDays;
  Date = TDateTime(static_cast<double>(Days));
  return true;
}

bool TryEncodeTime(Word Hour, Word Min, Word Sec, Word MSec, TDateTime & Time)
{
  if (Hour > 23 || Min > 59 || Sec > 59 || MSec > 999)
  {
    return false;
  }
  const double MS = ((static_cast<double>(Hour) * 60.0 + Min) * 60.0 + Sec) *
                    1000.0 + MSec;
  Time = TDateTime(MS / MSecsPerDay);
  return true;
}

TDateTime EncodeDate(Word Year, Word Month, Word Day)
{
  TDateTime R;
  TryEncodeDate(Year, Month, Day, R);   // invalid input yields 0, as core expects
  return R;
}

TDateTime EncodeTime(Word Hour, Word Min, Word Sec, Word MSec)
{
  TDateTime R;
  TryEncodeTime(Hour, Min, Sec, MSec, R);
  return R;
}

TDateTime ComposeDateTime(const TDateTime & Date, const TDateTime & Time)
{
  const double D = Date.Val();
  const double T = std::fabs(Time.Val());
  return TDateTime(D < 0.0 ? D - T : D + T);
}

TDateTime EncodeDateTime(Word Year, Word Month, Word Day,
                         Word Hour, Word Min, Word Sec, Word MSec)
{
  return ComposeDateTime(EncodeDate(Year, Month, Day),
                         EncodeTime(Hour, Min, Sec, MSec));
}

void DecodeDate(const TDateTime & DT, Word & Year, Word & Month, Word & Day)
{
  long long Days = 0;
  double Frac = 0.0;
  SplitSerial(DT.Val(), Days, Frac);
  int Y = 0;
  unsigned M = 0, D = 0;
  CivilFromDays(Days - DelphiEpochToUnixDays, Y, M, D);
  Year = static_cast<Word>(Y);
  Month = static_cast<Word>(M);
  Day = static_cast<Word>(D);
}

void DecodeTime(const TDateTime & DT, Word & Hour, Word & Min, Word & Sec,
                Word & MSec)
{
  long long Days = 0;
  double Frac = 0.0;
  SplitSerial(DT.Val(), Days, Frac);
  // Round to the nearest millisecond: the fraction cannot represent every
  // millisecond exactly, and truncating would drift timestamps down by 1ms.
  long long MS = static_cast<long long>(std::llround(Frac * MSecsPerDay));
  if (MS >= 86400000LL)
  {
    MS = 86399999LL;   // guard against a fraction that rounds up to a full day
  }
  MSec = static_cast<Word>(MS % 1000);
  MS /= 1000;
  Sec = static_cast<Word>(MS % 60);
  MS /= 60;
  Min = static_cast<Word>(MS % 60);
  MS /= 60;
  Hour = static_cast<Word>(MS);
}

TDateTime::TDateTime(Word Year, Word Month, Word Day)
  : FValue(EncodeDate(Year, Month, Day).Val()) {}

TDateTime::TDateTime(Word Hour, Word Min, Word Sec, Word MSec)
  : FValue(EncodeTime(Hour, Min, Sec, MSec).Val()) {}

void TDateTime::DecodeDate(Word * Year, Word * Month, Word * Day) const
{
  Word Y = 0, M = 0, D = 0;
  ::DecodeDate(*this, Y, M, D);
  if (Year != nullptr)  { *Year = Y; }
  if (Month != nullptr) { *Month = M; }
  if (Day != nullptr)   { *Day = D; }
}

void TDateTime::DecodeTime(Word * Hour, Word * Min, Word * Sec, Word * MSec) const
{
  Word H = 0, N = 0, S = 0, Z = 0;
  ::DecodeTime(*this, H, N, S, Z);
  if (Hour != nullptr) { *Hour = H; }
  if (Min != nullptr)  { *Min = N; }
  if (Sec != nullptr)  { *Sec = S; }
  if (MSec != nullptr) { *MSec = Z; }
}

TDateTime Now()
{
  // Delphi's Now is local time. std::chrono gives UTC, so apply the zone offset.
  const auto TP = std::chrono::system_clock::now();
  const auto MS = std::chrono::duration_cast<std::chrono::milliseconds>(
                    TP.time_since_epoch()).count();
  const std::time_t T = static_cast<std::time_t>(MS / 1000);
  std::tm Local{};
#if defined(_WIN32)
  localtime_s(&Local, &T);
#else
  localtime_r(&T, &Local);
#endif
  TDateTime D = EncodeDate(static_cast<Word>(Local.tm_year + 1900),
                           static_cast<Word>(Local.tm_mon + 1),
                           static_cast<Word>(Local.tm_mday));
  TDateTime Tm = EncodeTime(static_cast<Word>(Local.tm_hour),
                            static_cast<Word>(Local.tm_min),
                            static_cast<Word>(Local.tm_sec),
                            static_cast<Word>(MS % 1000));
  return ComposeDateTime(D, Tm);
}

TDateTime Date()
{
  return TDateTime(std::trunc(Now().Val()));
}

TDateTime Time()
{
  const double V = Now().Val();
  return TDateTime(V - std::trunc(V));
}

TDateTime IncDay(const TDateTime & DT, int Days)
{
  return TDateTime(DT.Val() + Days);
}

TDateTime IncMilliSecond(const TDateTime & DT, __int64 MilliSeconds)
{
  return TDateTime(MSecsToSerial(SerialToMSecs(DT.Val()) + MilliSeconds));
}

TDateTime IncSecond(const TDateTime & DT, __int64 Seconds)
{
  return IncMilliSecond(DT, Seconds * 1000);
}

TDateTime IncMinute(const TDateTime & DT, __int64 Minutes)
{
  return IncMilliSecond(DT, Minutes * 60 * 1000);
}

TDateTime IncHour(const TDateTime & DT, __int64 Hours)
{
  return IncMilliSecond(DT, Hours * 60 * 60 * 1000);
}

Word MilliSecondOfTheSecond(const TDateTime & DT)
{
  return static_cast<Word>(MSecsOfDay(DT) % 1000);
}

Cardinal MilliSecondOfTheMinute(const TDateTime & DT)
{
  return static_cast<Cardinal>(MSecsOfDay(DT) % (60 * 1000));
}

Cardinal MilliSecondOfTheHour(const TDateTime & DT)
{
  return static_cast<Cardinal>(MSecsOfDay(DT) % (60 * 60 * 1000));
}

Cardinal MilliSecondOfTheDay(const TDateTime & DT)
{
  return static_cast<Cardinal>(MSecsOfDay(DT));
}

__int64 MilliSecondOfTheYear(const TDateTime & DT)
{
  Word Y = 0, M = 0, D = 0;
  ::DecodeDate(DT, Y, M, D);
  return SerialToMSecs(DT.Val()) - SerialToMSecs(EncodeDate(Y, 1, 1).Val());
}

TDateTime IncMonth(const TDateTime & DT, int Months)
{
  Word Y = 0, M = 0, D = 0;
  ::DecodeDate(DT, Y, M, D);
  long long Total = static_cast<long long>(Y) * 12 + (M - 1) + Months;
  int NY = static_cast<int>(Total / 12);
  int NM = static_cast<int>(Total % 12) + 1;
  if (NM < 1)
  {
    NM += 12;
    --NY;
  }
  // Clamp the day, so 31 January plus one month is 28/29 February rather than
  // an invalid date. This is Delphi's behaviour.
  const Word Max = DaysInMonth(static_cast<Word>(NY), static_cast<Word>(NM));
  const Word ND = D > Max ? Max : D;
  const double Frac = DT.Val() - std::trunc(DT.Val());
  return ComposeDateTime(EncodeDate(static_cast<Word>(NY), static_cast<Word>(NM), ND),
                         TDateTime(Frac));
}

TDateTime IncYear(const TDateTime & DT, int Years)
{
  return IncMonth(DT, Years * 12);
}

int DayOfWeek(const TDateTime & DT)
{
  long long Days = 0;
  double Frac = 0.0;
  SplitSerial(DT.Val(), Days, Frac);
  // Serial 0 is 1899-12-30, a Saturday, which Delphi numbers 7.
  long long W = (Days + 6) % 7;
  if (W < 0)
  {
    W += 7;
  }
  return static_cast<int>(W) + 1;
}

__int64 MilliSecondsBetween(const TDateTime & A, const TDateTime & B)
{
  return static_cast<__int64>(std::llround(std::fabs(A.Val() - B.Val()) *
                                           MSecsPerDay));
}

__int64 SecondsBetween(const TDateTime & A, const TDateTime & B)
{
  return MilliSecondsBetween(A, B) / 1000;
}

__int64 DaysBetween(const TDateTime & A, const TDateTime & B)
{
  return static_cast<__int64>(std::fabs(std::trunc(A.Val()) -
                                        std::trunc(B.Val())));
}

UnicodeString FormatDateTime(const UnicodeString & Format, const TDateTime & DT)
{
  Word Y = 0, Mo = 0, D = 0, H = 0, Mi = 0, S = 0, Z = 0;
  ::DecodeDate(DT, Y, Mo, D);
  ::DecodeTime(DT, H, Mi, S, Z);
  const TFormatSettings & FS = TFormatSettings::Default();

  UnicodeString Out;
  const int N = Format.Length();
  int I = 1;
  while (I <= N)
  {
    const wchar_t C = Format[I];

    // Quoted literals pass through verbatim.
    if (C == L'\'' || C == L'"')
    {
      const wchar_t Quote = C;
      ++I;
      while (I <= N && Format[I] != Quote)
      {
        Out += Format[I];
        ++I;
      }
      ++I;
      continue;
    }

    // Count the run of identical specifier characters.
    int Run = 0;
    while (I + Run <= N && Format[I + Run] == C)
    {
      ++Run;
    }
    const wchar_t Lower = (C >= L'A' && C <= L'Z')
                          ? static_cast<wchar_t>(C - L'A' + L'a') : C;

    switch (Lower)
    {
      case L'y':
        Out += (Run >= 4) ? Pad(Y, 4) : Pad(Y % 100, 2);
        break;
      case L'm':
        if (Run >= 4)      { Out += UnicodeString(LongMonthNames[Mo - 1]); }
        else if (Run == 3) { Out += UnicodeString(ShortMonthNames[Mo - 1]); }
        else               { Out += Pad(Mo, Run); }
        break;
      case L'd':
        if (Run >= 6)      { Out += FormatDateTime(FS.LongDateFormat, DT); }
        else if (Run == 5) { Out += FormatDateTime(FS.ShortDateFormat, DT); }
        else if (Run == 4) { Out += UnicodeString(LongDayNames[DayOfWeek(DT) - 1]); }
        else if (Run == 3) { Out += UnicodeString(ShortDayNames[DayOfWeek(DT) - 1]); }
        else               { Out += Pad(D, Run); }
        break;
      case L'h':
        Out += Pad(H, Run);
        break;
      case L'n':
        Out += Pad(Mi, Run);
        break;
      case L's':
        Out += Pad(S, Run);
        break;
      case L'z':
        Out += (Run >= 3) ? Pad(Z, 3) : Pad(Z, Run);
        break;
      case L't':
        Out += FormatDateTime(Run >= 2 ? FS.LongTimeFormat : FS.ShortTimeFormat, DT);
        break;
      default:
        for (int K = 0; K < Run; ++K)
        {
          Out += C;
        }
        break;
    }
    I += Run;
  }
  return Out;
}

UnicodeString TDateTime::FormatString(const UnicodeString & Format) const
{
  return FormatDateTime(Format, *this);
}

UnicodeString TDateTime::DateString() const
{
  return FormatDateTime(TFormatSettings::Default().ShortDateFormat, *this);
}

UnicodeString TDateTime::TimeString() const
{
  return FormatDateTime(TFormatSettings::Default().LongTimeFormat, *this);
}

UnicodeString DateTimeToStr(const TDateTime & DT)
{
  const TFormatSettings & FS = TFormatSettings::Default();
  return FormatDateTime(FS.ShortDateFormat + UnicodeString(L" ") +
                        FS.LongTimeFormat, DT);
}

bool TryStrToDateTime(const UnicodeString & S, TDateTime & DT)
{
  // Accepts the ISO-ish shape the shim emits: YYYY-MM-DD[ HH:MM[:SS[.mmm]]].
  int Y = 0, Mo = 0, D = 0, H = 0, Mi = 0, Sec = 0, Ms = 0;
  std::string A;
  for (int I = 1; I <= S.Length(); ++I)
  {
    A += static_cast<char>(static_cast<unsigned char>(S[I] & 0xFF));
  }
  const int Got = std::sscanf(A.c_str(), "%d-%d-%d %d:%d:%d.%d",
                              &Y, &Mo, &D, &H, &Mi, &Sec, &Ms);
  if (Got < 3)
  {
    return false;
  }
  TDateTime DatePart;
  if (!TryEncodeDate(static_cast<Word>(Y), static_cast<Word>(Mo),
                     static_cast<Word>(D), DatePart))
  {
    return false;
  }
  TDateTime TimePart;
  if (Got >= 5 && !TryEncodeTime(static_cast<Word>(H), static_cast<Word>(Mi),
                                 static_cast<Word>(Got >= 6 ? Sec : 0),
                                 static_cast<Word>(Got >= 7 ? Ms : 0), TimePart))
  {
    return false;
  }
  DT = ComposeDateTime(DatePart, TimePart);
  return true;
}

TDateTime StrToDateTime(const UnicodeString & S)
{
  TDateTime R;
  TryStrToDateTime(S, R);
  return R;
}
