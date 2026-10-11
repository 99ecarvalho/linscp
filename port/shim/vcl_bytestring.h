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

#include <string>

#include "vcl_unicodestring.h"

enum : unsigned short
{
  CP_ACP_SHIM = 0,        // system ANSI code page; approximated as Latin-1
  CP_UTF8_SHIM = 65001,
  CP_RAW_SHIM = 65535,    // no conversion: bytes pass through unchanged
};

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

  const char * c_str() const { return FData.c_str(); }
  // Writable, as C++Builder's c_str() is: core shreds secrets in place with
  // memset(Str.c_str(), 0, ...).
  char * c_str() { return FData.data(); }
  const char * data() const { return FData.c_str(); }
  char * MutableData() { return const_cast<char *>(FData.data()); }

  // 1-based, as in Delphi.
  char operator[](int Index) const;
  char & operator[](int Index);

  AnsiStringT SubString(int Index, int Count) const;
  AnsiStringT SubString(int Index) const { return SubString(Index, Length() - Index + 1); }
  int Pos(const AnsiStringT & Substr) const;
  int Pos(char C) const;
  void Delete(int Index, int Count);
  void Insert(const AnsiStringT & S, int Index);
  void Unique() {}

  AnsiStringT & operator+=(const AnsiStringT & S) { FData += S.FData; return *this; }
  AnsiStringT & operator+=(char C) { FData += C; return *this; }

  friend bool operator==(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData == B.FData; }
  friend bool operator!=(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData != B.FData; }
  friend bool operator<(const AnsiStringT & A, const AnsiStringT & B)
    { return A.FData < B.FData; }
  friend AnsiStringT operator+(const AnsiStringT & A, const AnsiStringT & B)
    { AnsiStringT R(A); R += B; return R; }

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

#endif // LINSCP_VCL_BYTESTRING_H
