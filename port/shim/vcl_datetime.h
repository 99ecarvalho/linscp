//---------------------------------------------------------------------------
// TDateTime and the date/time RTL surface core uses.
//
// A TDateTime is a double, exactly as in Delphi:
//   integral part   days since 1899-12-30 (so 0 is 1899-12-30, 2 is 1900-01-01)
//   fractional part fraction of a 24-hour day
//
// The trap is negative values. Delphi treats the fractional part as a positive
// time of day even when the date is negative, so -1.25 is 1899-12-29 06:00, not
// 18:00. Core stores file timestamps in these, so getting it wrong would shift
// times by the wrong sign rather than fail loudly. Tested explicitly.
//
// Everything here is UTC-naive: it does no timezone or DST work. Core does its
// own DST handling (ConvertTimestampToUTC, AdjustDateTimeFromUnix and friends),
// so the shim must not second-guess it.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_DATETIME_H
#define LINSCP_VCL_DATETIME_H

#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Day number of 1899-12-30 relative to the Unix epoch, i.e. the offset between
// a Delphi date serial and days-since-1970.
constexpr int DelphiEpochToUnixDays = 25569;

class TDateTime
{
public:
  TDateTime() = default;
  TDateTime(double Value) : FValue(Value) {}          // implicit, as in C++Builder
  TDateTime(Word Year, Word Month, Word Day);
  TDateTime(Word Hour, Word Min, Word Sec, Word MSec);

  operator double() const { return FValue; }
  double Val() const { return FValue; }

  void DecodeDate(Word * Year, Word * Month, Word * Day) const;
  void DecodeTime(Word * Hour, Word * Min, Word * Sec, Word * MSec) const;

  TDateTime & operator+=(double D) { FValue += D; return *this; }
  TDateTime & operator-=(double D) { FValue -= D; return *this; }

  UnicodeString FormatString(const UnicodeString & Format) const;
  UnicodeString TimeString() const;
  UnicodeString DateString() const;

private:
  double FValue = 0.0;
};

// Locale-ish settings. Defaults are ISO-8601 rather than system locale so
// behaviour is deterministic across machines; anything user-facing should set
// these explicitly rather than rely on the defaults.
using TLocaleID = unsigned int;   // an LCID, as on Windows

struct TFormatSettings
{
  TFormatSettings();

  UnicodeString ShortDateFormat = L"yyyy-mm-dd";
  UnicodeString LongDateFormat  = L"dddd, d mmmm yyyy";
  UnicodeString ShortTimeFormat = L"hh:nn";
  UnicodeString LongTimeFormat  = L"hh:nn:ss";
  UnicodeString DateSeparator   = L"-";
  UnicodeString TimeSeparator   = L":";
  UnicodeString DecimalSeparator = L".";
  UnicodeString ThousandSeparator = L",";

  // English, whatever the locale: core asks for these by LCID 1033 to parse
  // protocol dates, and the shim does not localise. The slack slot past the
  // end (see StaticArray) holds U+FFFF, which no input contains.
  StaticArray<UnicodeString, 12> ShortMonthNames;
  StaticArray<UnicodeString, 12> LongMonthNames;
  StaticArray<UnicodeString, 7> ShortDayNames;
  StaticArray<UnicodeString, 7> LongDayNames;

  static TFormatSettings & Default();
  // Delphi's constructors. Locale-independent here: every locale yields the
  // shim's deterministic defaults; see the divergence table.
  static TFormatSettings Create();
  static TFormatSettings Create(TLocaleID Locale);
  static TFormatSettings Create(const UnicodeString & LocaleName);
};

// SysUtils' global settings variable.
extern TFormatSettings FormatSettings;

// --- constants -------------------------------------------------------------
// The rest of the family (MSecsPerSec ... MSecsPerDay) is in vcl_strutils.h.
constexpr int MinsPerDay = 24 * 60;
constexpr int SecsPerHour = 60 * 60;
constexpr int DateDelta = 693594;       // days from 0001-01-01 to 1899-12-31
constexpr int UnixDateDelta = 25569;    // days from 1899-12-30 to 1970-01-01
extern const TDateTime MinDateTime;     // 0100-01-01 00:00:00.000
extern const TDateTime MaxDateTime;     // 9999-12-31 23:59:59.999

// Delphi's TTimeStamp: milliseconds into the day, and days since 0001-01-01
// plus one -- so Date - DateDelta is the serial's whole-day part.
struct TTimeStamp
{
  int Time = 0;
  int Date = 0;
};
TTimeStamp DateTimeToTimeStamp(const TDateTime & DT);
TDateTime TimeStampToDateTime(const TTimeStamp & TS);

// --- civil date <-> serial -------------------------------------------------
bool TryEncodeDate(Word Year, Word Month, Word Day, TDateTime & Date);
bool TryEncodeTime(Word Hour, Word Min, Word Sec, Word MSec, TDateTime & Time);
TDateTime EncodeDate(Word Year, Word Month, Word Day);
TDateTime EncodeDateTime(Word Year, Word Month, Word Day,
                         Word Hour, Word Min, Word Sec, Word MSec);

// Combine a date serial with a time-of-day fraction.
//
// Use this rather than Date + Time. Because Delphi keeps the time of day
// positive for negative dates, the time must be *subtracted* when the date is
// before 1899-12-30; plain addition lands a day late at the mirrored time.
// Harmless for file timestamps, which are all positive, but wrong in general
// and wrong silently.
TDateTime ComposeDateTime(const TDateTime & Date, const TDateTime & Time);
TDateTime EncodeTime(Word Hour, Word Min, Word Sec, Word MSec);
void DecodeDate(const TDateTime & DT, Word & Year, Word & Month, Word & Day);
void DecodeTime(const TDateTime & DT, Word & Hour, Word & Min, Word & Sec, Word & MSec);

// --- clock -----------------------------------------------------------------
TDateTime Now();    // local time, as Delphi's Now
TDateTime Date();   // today at midnight
TDateTime Time();   // time of day only

// --- arithmetic ------------------------------------------------------------
TDateTime IncYear(const TDateTime & DT, int Years = 1);
TDateTime IncMonth(const TDateTime & DT, int Months = 1);
TDateTime IncDay(const TDateTime & DT, int Days = 1);
// Through a linear millisecond count, as Delphi's TTimeStamp route does, rather
// than by adding a fraction to the serial: before 1899-12-30 the serial runs
// backwards within a day, so float addition would move time the wrong way.
TDateTime IncMilliSecond(const TDateTime & DT, __int64 MilliSeconds = 1);
TDateTime IncSecond(const TDateTime & DT, __int64 Seconds = 1);
TDateTime IncMinute(const TDateTime & DT, __int64 Minutes = 1);
TDateTime IncHour(const TDateTime & DT, __int64 Hours = 1);

// Delphi's return types, exactly. Core negates these to truncate a timestamp:
// it casts the unsigned ones to int first, but negates MilliSecondOfTheYear
// directly -- correct only because Delphi returns that one as signed Int64.
Word MilliSecondOfTheSecond(const TDateTime & DT);
Cardinal MilliSecondOfTheMinute(const TDateTime & DT);
Cardinal MilliSecondOfTheHour(const TDateTime & DT);
Cardinal MilliSecondOfTheDay(const TDateTime & DT);
__int64 MilliSecondOfTheYear(const TDateTime & DT);
int DayOfWeek(const TDateTime & DT);     // 1 = Sunday, as in Delphi
bool IsLeapYear(Word Year);
Word DaysInMonth(Word Year, Word Month);

__int64 MilliSecondsBetween(const TDateTime & A, const TDateTime & B);
__int64 SecondsBetween(const TDateTime & A, const TDateTime & B);
__int64 DaysBetween(const TDateTime & A, const TDateTime & B);
__int64 MinutesBetween(const TDateTime & A, const TDateTime & B);
__int64 HoursBetween(const TDateTime & A, const TDateTime & B);
// Delphi's documented approximations: 30.4375 days a month, 365.25 a year.
int MonthsBetween(const TDateTime & A, const TDateTime & B);
int YearsBetween(const TDateTime & A, const TDateTime & B);
bool IsSameDay(const TDateTime & Value, const TDateTime & Basis);

// --- formatting ------------------------------------------------------------
UnicodeString FormatDateTime(const UnicodeString & Format, const TDateTime & DT);
UnicodeString FormatDateTime(const UnicodeString & Format, const TDateTime & DT,
                             const TFormatSettings & Settings);
UnicodeString DateTimeToStr(const TDateTime & DT);
bool TryStrToDateTime(const UnicodeString & S, TDateTime & DT);
// Delphi's scan: field order from ShortDateFormat (in which '/' stands for
// DateSeparator), the time after whitespace with TimeSeparator, milliseconds
// after DecimalSeparator. Two-digit years pivot on 50, as Delphi's default
// TwoDigitYearCenturyWindow does.
bool TryStrToDateTime(const UnicodeString & S, TDateTime & DT,
                      const TFormatSettings & Settings);
TDateTime StrToDateTime(const UnicodeString & S);

#endif // LINSCP_VCL_DATETIME_H
