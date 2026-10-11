//---------------------------------------------------------------------------
// Delphi's byte-string family: AnsiString, UTF8String, RawByteString.
//
// C++Builder models these as one template parameterised by code page, and core
// relies on them being distinct types -- assigning a RawByteString to a
// UTF8String must not silently reinterpret bytes. The same shape is kept here.
//
// Indexing is 1-based, as for UnicodeString.
//
// Conversion to and from UnicodeString is where encoding actually happens:
//   RawByteString  bytes are preserved one-for-one (no transcoding)
//   UTF8String     real UTF-8 <-> UTF-16, since protocol correctness depends on it
//   AnsiString     Latin-1; see the note on CP_ACP below
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_BYTESTRING_H
#define LINSCP_VCL_BYTESTRING_H

#include <cstdarg>
#include <cstdio>
#include <string>
#include <type_traits>

#include "vcl_unicodestring.h"

enum : unsigned short
{
  CP_ACP_SHIM = 0,        // system ANSI code page; approximated as Latin-1
  CP_UTF8_SHIM = 65001,
  CP_RAW_SHIM = 65535,    // no conversion: bytes pass through unchanged
};

class TVarRec;

template <unsigned short CodePage>
class AnsiStringT
{
public:
  AnsiStringT() = default;
  AnsiStringT(const AnsiStringT &) = default;
  AnsiStringT(AnsiStringT &&) noexcept = default;
  AnsiStringT & operator=(const AnsiStringT &) = default;
  AnsiStringT & operator=(AnsiStringT &&) noexcept = default;

  AnsiStringT(const char * S) : FData(S != nullptr ? S : "") {}
  AnsiStringT(const char * S, int Len)
    : FData(S != nullptr && Len > 0 ? std::string(S, static_cast<std::size_t>(Len))
                                    : std::string()) {}
  explicit AnsiStringT(char C, int Count = 1)
    : FData(Count > 0 ? std::string(static_cast<std::size_t>(Count), C)
                      : std::string()) {}

  // Transcoding constructor and conversion. Deliberately explicit in neither
  // direction only where C++Builder is also implicit, so upstream compiles.
  AnsiStringT(const UnicodeString & S) { Assign(S); }
  // A wide literal directly: through UnicodeString would be two user-defined
  // conversions. Core passes its L"..." SFTP extension names this way.
  AnsiStringT(const wchar_t * S) { Assign(UnicodeString(S)); }
  // And assigned, or `Str = L""` would be ambiguous between this constructor
  // and the UnicodeString one.
  AnsiStringT & operator=(const wchar_t * S) { Assign(UnicodeString(S)); return *this; }

  // Between code pages, implicitly, as C++Builder allows. Into or out of
  // RawByteString the bytes pass through untouched; between two real code
  // pages they are transcoded via UTF-16. (C++Builder's RawByteString carries
  // a run-time code page and would transcode out of it; this one has none --
  // see the divergence table.)
  template <unsigned short Other,
            typename = std::enable_if_t<Other != CodePage>>
  AnsiStringT(const AnsiStringT<Other> & S)
  {
    if constexpr ((CodePage == CP_RAW_SHIM) || (Other == CP_RAW_SHIM))
    {
      FData = S.Bytes();
    }
    else
    {
      Assign(ToUnicodeString(S));
    }
  }
  AnsiStringT & operator=(const UnicodeString & S) { Assign(S); return *this; }
  // Bytes as they are. Without this, `Str = Str.c_str()` is ambiguous between
  // the byte constructor and the UnicodeString conversion.
  AnsiStringT & operator=(const char * S) { FData = (S != nullptr) ? S : ""; return *this; }
  // And back, implicitly, as C++Builder's AnsiStringT has it: core passes a
  // RawByteString where a UnicodeString is expected. Uses the per-code-page
  // conversion below, so it adds no new encoding rule.
  operator UnicodeString() const;

  int Length() const { return static_cast<int>(FData.size()); }
  bool IsEmpty() const { return FData.empty(); }
  void SetLength(int NewLength)
    { FData.resize(NewLength > 0 ? static_cast<std::size_t>(NewLength) : 0); }

  // char * even on a const string, as C++Builder's c_str() is: core shreds
  // secrets in place with memset(Str.c_str(), ...) and hands c_str() to C APIs
  // taking char * (neon's ne_uri.path).
  char * c_str() const { return const_cast<char *>(FData.c_str()); }
  const char * data() const { return FData.c_str(); }
  char * MutableData() { return const_cast<char *>(FData.data()); }

  // 1-based, as in Delphi.
  char operator[](int Index) const;
  char & operator[](int Index);

  AnsiStringT SubString(int Index, int Count) const;
  AnsiStringT SubString(int Index) const { return SubString(Index, Length() - Index + 1); }
  // Delphi's: strip every character up to and including ' ' from the ends.
  AnsiStringT TrimLeft() const
  {
    std::size_t B = 0;
    while ((B < FData.size()) && (static_cast<unsigned char>(FData[B]) <= ' ')) { ++B; }
    AnsiStringT R; R.FData = FData.substr(B); return R;
  }
  AnsiStringT TrimRight() const
  {
    std::size_t E = FData.size();
    while ((E > 0) && (static_cast<unsigned char>(FData[E - 1]) <= ' ')) { --E; }
    AnsiStringT R; R.FData = FData.substr(0, E); return R;
  }
  AnsiStringT Trim() const { return TrimLeft().TrimRight(); }
  // ASCII folding, as the shim's UnicodeString::UpperCase (see the divergence
  // table); core applies these to protocol tokens.
  AnsiStringT UpperCase() const
  {
    AnsiStringT R(*this);
    for (char & C : R.FData) { if ((C >= 'a') && (C <= 'z')) { C = static_cast<char>(C - 'a' + 'A'); } }
    return R;
  }
  AnsiStringT LowerCase() const
  {
    AnsiStringT R(*this);
    for (char & C : R.FData) { if ((C >= 'A') && (C <= 'Z')) { C = static_cast<char>(C - 'A' + 'a'); } }
    return R;
  }
  int Pos(const AnsiStringT & Substr) const;
  int Pos(char C) const;
  void Delete(int Index, int Count);
  void Insert(const AnsiStringT & S, int Index);
  void Insert(char C, int Index) { Insert(AnsiStringT(&C, 1), Index); }
  void Unique() {}

  AnsiStringT & operator+=(const AnsiStringT & S) { FData += S.FData; return *this; }

  // C++Builder's narrow printf family; replaces the content, returns its length.
  int vprintf(const char * Format, va_list Args);
  int sprintf(const char * Format, ...);
  // C++Builder's static Format over an open array, via Delphi's Format.
  static AnsiStringT Format(const AnsiStringT & Fmt, const TVarRec * Args, int Args_High);
  AnsiStringT & operator+=(char C) { FData += C; return *this; }

  friend bool operator==(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData == B.FData; }
  friend bool operator!=(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData != B.FData; }
  friend bool operator<(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData < B.FData; }
  friend AnsiStringT operator+(const AnsiStringT & A, const AnsiStringT & B)
    { AnsiStringT R(A); R += B; return R; }
  friend AnsiStringT operator+(const AnsiStringT & A, char C)
    { AnsiStringT R(A); R += C; return R; }
  friend AnsiStringT operator+(char C, const AnsiStringT & A)
    { AnsiStringT R(&C, 1); R += A; return R; }

  // Exposed for the conversion helpers and for callers that need the raw bytes.
  const std::string & Bytes() const { return FData; }
  std::string & Bytes() { return FData; }

private:
  void Assign(const UnicodeString & S);
  std::string FData;
};

using AnsiString    = AnsiStringT<CP_ACP_SHIM>;
using UTF8String    = AnsiStringT<CP_UTF8_SHIM>;
using RawByteString = AnsiStringT<CP_RAW_SHIM>;

// Encoding primitives. Separate from the string types so they can be tested
// directly -- protocol correctness depends on these being exactly right.
namespace linscp {

std::string Utf16ToUtf8(const UnicodeString & S);
UnicodeString Utf8ToUtf16(const char * Bytes, std::size_t Count);
std::string Utf16ToLatin1(const UnicodeString & S);
UnicodeString Latin1ToUtf16(const char * Bytes, std::size_t Count);
std::string Utf16ToRaw(const UnicodeString & S);
UnicodeString RawToUtf16(const char * Bytes, std::size_t Count);

}  // namespace linscp

// Conversions back to UnicodeString, one per code page.
UnicodeString ToUnicodeString(const AnsiString & S);
UnicodeString ToUnicodeString(const UTF8String & S);
UnicodeString ToUnicodeString(const RawByteString & S);

template <unsigned short CodePage>
inline AnsiStringT<CodePage>::operator UnicodeString() const
{
  return ToUnicodeString(*this);
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::vprintf(const char * Format, va_list Args)
{
  va_list Copy;
  va_copy(Copy, Args);
  const int N = std::vsnprintf(nullptr, 0, Format, Copy);
  va_end(Copy);
  if (N < 0)
  {
    FData.clear();
    return 0;
  }
  FData.assign(static_cast<std::size_t>(N) + 1, '\0');
  std::vsnprintf(&FData[0], FData.size(), Format, Args);
  FData.resize(static_cast<std::size_t>(N));
  return N;
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::sprintf(const char * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  const int N = vprintf(Format, Args);
  va_end(Args);
  return N;
}

// Defined in vcl_format.cpp, which knows TVarRec.
UnicodeString FormatOpenArray(const UnicodeString & Fmt, const TVarRec * Args, int Args_High);

template <unsigned short CodePage>
AnsiStringT<CodePage> AnsiStringT<CodePage>::Format(const AnsiStringT & Fmt,
                                                    const TVarRec * Args, int Args_High)
{
  return AnsiStringT(FormatOpenArray(ToUnicodeString(Fmt), Args, Args_High));
}

// Comparing a byte string with a UnicodeString compares as text, as Delphi
// does. Exact templates, so neither side's implicit conversion is in play.
template <unsigned short CP>
bool operator==(const AnsiStringT<CP> & A, const UnicodeString & B) { return ToUnicodeString(A) == B; }
template <unsigned short CP>
bool operator==(const UnicodeString & A, const AnsiStringT<CP> & B) { return A == ToUnicodeString(B); }
template <unsigned short CP>
bool operator!=(const AnsiStringT<CP> & A, const UnicodeString & B) { return !(A == B); }
template <unsigned short CP>
bool operator!=(const UnicodeString & A, const AnsiStringT<CP> & B) { return !(A == B); }

// WideStrUtils' classification of a byte string.
enum TEncodeType { etUSASCII, etUTF8, etANSI };
TEncodeType DetectUTF8Encoding(const RawByteString & S);
// System's UTF8ArrayToString over an open array: A[0..A_High], stopping at the
// first NUL as Delphi does.
UnicodeString UTF8ArrayToString(const char * A, int A_High);

#endif // LINSCP_VCL_BYTESTRING_H
