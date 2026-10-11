//---------------------------------------------------------------------------
// Assorted VCL types core references in small numbers.
//
// Each is scoped to what core actually uses. Anything missing surfaces as a
// compile error naming the exact member, which is a better outcome than a stub
// that quietly does nothing.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_MISC_H
#define LINSCP_VCL_MISC_H

#include "vcl_datetime.h"
#include "vcl_object.h"
#include "vcl_strings.h"
#include "vcl_strutils.h"
#include "vcl_unicodestring.h"

#if defined(_WIN32)
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
#endif

// Delphi's name for the Win32 version record. Core reads the version of its
// own binary through this.
using TVSFixedFileInfo = VS_FIXEDFILEINFO;
using PVSFixedFileInfo = VS_FIXEDFILEINFO *;

//---------------------------------------------------------------------------
// System.Masks: shell-style wildcard matching. Core wraps this in TFileMasks
// and only ever asks whether a name matches.
//---------------------------------------------------------------------------
namespace Masks {

class TMask : public TObject
{
public:
  explicit TMask(const UnicodeString & MaskValue) : FMask(MaskValue) {}

  bool Matches(const UnicodeString & Filename) const;

  PROP_R_F(UnicodeString, Mask, FMask);

private:
  UnicodeString FMask;
};

}  // namespace Masks

// Free form, as System.Masks also offers.
bool MatchesMask(const UnicodeString & Filename, const UnicodeString & Mask);

// Vcl.Menus.StripHotkey: removes the '&' marking a menu accelerator. The
// character after each '&' is kept verbatim, so an escaped "&&" yields a
// literal '&'. Delphi additionally strips a whole "(&X)" suffix when the
// system locale is East Asian; that branch is not reproduced.
UnicodeString StripHotkey(const UnicodeString & Text);

//---------------------------------------------------------------------------
// System.IOUtils.TPath, used for a handful of path queries.
//---------------------------------------------------------------------------
class TPath
{
public:
  static UnicodeString GetFileName(const UnicodeString & Path)
    { return ExtractFileName(Path); }
  static UnicodeString GetDirectoryName(const UnicodeString & Path)
    { return ExtractFileDir(Path); }
  static UnicodeString GetExtension(const UnicodeString & Path)
    { return ExtractFileExt(Path); }
  static UnicodeString ChangeExtension(const UnicodeString & Path,
                                       const UnicodeString & Extension)
    { return ChangeFileExt(Path, Extension); }
  static UnicodeString Combine(const UnicodeString & A, const UnicodeString & B)
    { return IncludeTrailingBackslash(A) + B; }
  static UnicodeString GetTempPath();
  static UnicodeString GetFullPath(const UnicodeString & Path)
    { return ExpandUNCFileName(Path); }
  static bool IsPathRooted(const UnicodeString & Path);
  // "X:\" or "X:/": absolute and on a drive. Always false on POSIX, as in
  // Delphi's own POSIX RTL -- core uses it only to reach Windows' UAC
  // VirtualStore, which has no counterpart there.
  static bool IsDriveRooted(const UnicodeString & Path);
};

//---------------------------------------------------------------------------
// System.IOUtils.TFile, the two members core uses.
//---------------------------------------------------------------------------
class TFile
{
public:
  // Decodes by byte-order mark (UTF-8, UTF-16 LE or BE). Without one, UTF-8 --
  // Delphi uses TEncoding.Default there, which is ANSI on Windows; see the
  // divergence table in doc/qt/TRACKER.md. Raises EFileNotFoundException for
  // a missing file, as Delphi does.
  static UnicodeString ReadAllText(const UnicodeString & Path);
  // UTF-8 without a byte-order mark, as Delphi's one-argument overload writes.
  // A BOM would break the one file core writes this way: authorized_keys.
  static void WriteAllText(const UnicodeString & Path, const UnicodeString & Contents);
};

//---------------------------------------------------------------------------
// System.TimeSpan, a duration. Core uses it to report elapsed times.
//---------------------------------------------------------------------------
class TTimeSpan
{
public:
  TTimeSpan() = default;
  explicit TTimeSpan(__int64 Ticks) : FTicks(Ticks) {}

  // 100-nanosecond ticks, as in Delphi and .NET.
  static constexpr __int64 TicksPerMillisecond = 10000;
  static constexpr __int64 TicksPerSecond = TicksPerMillisecond * 1000;
  static constexpr __int64 TicksPerMinute = TicksPerSecond * 60;
  static constexpr __int64 TicksPerHour = TicksPerMinute * 60;
  static constexpr __int64 TicksPerDay = TicksPerHour * 24;

  static TTimeSpan FromMilliseconds(double Value)
    { return TTimeSpan(static_cast<__int64>(Value * TicksPerMillisecond)); }
  static TTimeSpan FromSeconds(double Value)
    { return TTimeSpan(static_cast<__int64>(Value * TicksPerSecond)); }

  __int64 GetTicks() const { return FTicks; }
  double GetTotalMilliseconds() const
    { return static_cast<double>(FTicks) / TicksPerMillisecond; }
  double GetTotalSeconds() const
    { return static_cast<double>(FTicks) / TicksPerSecond; }

  PROP_R_G(__int64, Ticks, GetTicks);
  PROP_R_G(double, TotalMilliseconds, GetTotalMilliseconds);
  PROP_R_G(double, TotalSeconds, GetTotalSeconds);

private:
  __int64 FTicks = 0;
};

//---------------------------------------------------------------------------
// System.SysUtils.TEncoding, reduced to the conversions core performs.
//---------------------------------------------------------------------------
class TEncoding
{
public:
  static TEncoding * UTF8();
  static TEncoding * ANSI();
  static TEncoding * Default();

  virtual ~TEncoding() = default;
  virtual RawByteString GetBytes(const UnicodeString & S) const = 0;
  virtual UnicodeString GetString(const RawByteString & Bytes) const = 0;
};

#endif // LINSCP_VCL_MISC_H
