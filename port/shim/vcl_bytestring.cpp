#include "vcl_bytestring.h"

#include <algorithm>
#include <cassert>

namespace linscp {

namespace {

constexpr char16_t kReplacement = u'�';

constexpr bool IsHighSurrogate(char16_t C) { return C >= 0xD800 && C <= 0xDBFF; }
constexpr bool IsLowSurrogate(char16_t C)  { return C >= 0xDC00 && C <= 0xDFFF; }

void AppendUtf8(std::string & Out, char32_t CP)
{
  if (CP < 0x80)
  {
    Out += static_cast<char>(CP);
  }
  else if (CP < 0x800)
  {
    Out += static_cast<char>(0xC0 | (CP >> 6));
    Out += static_cast<char>(0x80 | (CP & 0x3F));
  }
  else if (CP < 0x10000)
  {
    Out += static_cast<char>(0xE0 | (CP >> 12));
    Out += static_cast<char>(0x80 | ((CP >> 6) & 0x3F));
    Out += static_cast<char>(0x80 | (CP & 0x3F));
  }
  else
  {
    Out += static_cast<char>(0xF0 | (CP >> 18));
    Out += static_cast<char>(0x80 | ((CP >> 12) & 0x3F));
    Out += static_cast<char>(0x80 | ((CP >> 6) & 0x3F));
    Out += static_cast<char>(0x80 | (CP & 0x3F));
  }
}

}  // namespace

std::string Utf16ToUtf8(const UnicodeString & S)
{
  std::string Out;
  Out.reserve(static_cast<std::size_t>(S.Length()));
  const int N = S.Length();
  for (int I = 1; I <= N; ++I)
  {
    const char16_t C = static_cast<char16_t>(S[I]);
    if (IsHighSurrogate(C) && I < N &&
        IsLowSurrogate(static_cast<char16_t>(S[I + 1])))
    {
      const char16_t Lo = static_cast<char16_t>(S[I + 1]);
      const char32_t CP = 0x10000u +
        ((static_cast<char32_t>(C) - 0xD800u) << 10) +
        (static_cast<char32_t>(Lo) - 0xDC00u);
      AppendUtf8(Out, CP);
      ++I;   // the pair was consumed
    }
    else if (IsHighSurrogate(C) || IsLowSurrogate(C))
    {
      // Unpaired surrogate: not representable in UTF-8. Substitute rather than
      // emit invalid bytes, which would corrupt the protocol stream.
      AppendUtf8(Out, kReplacement);
    }
    else
    {
      AppendUtf8(Out, C);
    }
  }
  return Out;
}

UnicodeString Utf8ToUtf16(const char * Bytes, std::size_t Count)
{
  UnicodeString Out;
  if (Bytes == nullptr)
  {
    return Out;
  }
  const auto * P = reinterpret_cast<const unsigned char *>(Bytes);
  std::size_t I = 0;
  while (I < Count)
  {
    const unsigned char B = P[I];
    char32_t CP = 0;
    std::size_t Advance = 1;
    std::size_t Extra = 0;

    if (B < 0x80)      { CP = B;         Extra = 0; }
    else if (B < 0xC2) { CP = kReplacement; Extra = 0; }  // continuation or overlong lead
    else if (B < 0xE0) { CP = B & 0x1Fu; Extra = 1; }
    else if (B < 0xF0) { CP = B & 0x0Fu; Extra = 2; }
    else if (B < 0xF5) { CP = B & 0x07u; Extra = 3; }
    else               { CP = kReplacement; Extra = 0; }

    if (Extra > 0)
    {
      // Consume the maximal valid subpart. Unicode's recommended practice is
      // one U+FFFD per maximal subpart, not one per byte, so a truncated
      // sequence yields a single replacement character.
      char32_t Acc = CP;
      std::size_t Got = 0;
      while (Got < Extra && I + Got + 1 < Count &&
             (P[I + Got + 1] & 0xC0u) == 0x80u)
      {
        Acc = (Acc << 6) | (P[I + Got + 1] & 0x3Fu);
        ++Got;
      }
      Advance = 1 + Got;
      if (Got < Extra || Acc > 0x10FFFFu || (Acc >= 0xD800u && Acc <= 0xDFFFu))
      {
        CP = kReplacement;   // truncated, out of range, or an encoded surrogate
      }
      else
      {
        CP = Acc;
      }
    }

    if (CP < 0x10000u)
    {
      Out += static_cast<wchar_t>(CP);
    }
    else
    {
      const char32_t V = CP - 0x10000u;
      Out += static_cast<wchar_t>(0xD800u + (V >> 10));
      Out += static_cast<wchar_t>(0xDC00u + (V & 0x3FFu));
    }
    I += Advance;
  }
  return Out;
}

std::string Utf16ToLatin1(const UnicodeString & S)
{
  std::string Out;
  Out.reserve(static_cast<std::size_t>(S.Length()));
  for (int I = 1; I <= S.Length(); ++I)
  {
    const char16_t C = static_cast<char16_t>(S[I]);
    Out += (C <= 0xFF) ? static_cast<char>(C) : '?';
  }
  return Out;
}

UnicodeString Latin1ToUtf16(const char * Bytes, std::size_t Count)
{
  UnicodeString Out;
  if (Bytes == nullptr)
  {
    return Out;
  }
  const auto * P = reinterpret_cast<const unsigned char *>(Bytes);
  for (std::size_t I = 0; I < Count; ++I)
  {
    Out += static_cast<wchar_t>(P[I]);
  }
  return Out;
}

// RawByteString carries opaque bytes. Each byte maps to the code unit of the
// same value so a round trip is lossless; it is never a text conversion.
std::string Utf16ToRaw(const UnicodeString & S)
{
  std::string Out;
  Out.reserve(static_cast<std::size_t>(S.Length()));
  for (int I = 1; I <= S.Length(); ++I)
  {
    Out += static_cast<char>(static_cast<unsigned char>(
             static_cast<char16_t>(S[I]) & 0xFF));
  }
  return Out;
}

UnicodeString RawToUtf16(const char * Bytes, std::size_t Count)
{
  return Latin1ToUtf16(Bytes, Count);
}

}  // namespace linscp

// --- AnsiStringT members ---------------------------------------------------

template <unsigned short CodePage>
char AnsiStringT<CodePage>::operator[](int Index) const
{
  assert(Index >= 1 && Index <= Length());
  return FData[static_cast<std::size_t>(Index - 1)];
}

template <unsigned short CodePage>
char & AnsiStringT<CodePage>::operator[](int Index)
{
  assert(Index >= 1 && Index <= Length());
  return FData[static_cast<std::size_t>(Index - 1)];
}

template <unsigned short CodePage>
AnsiStringT<CodePage> AnsiStringT<CodePage>::SubString(int Index, int Count) const
{
  if (Count <= 0 || Index > Length())
  {
    return AnsiStringT();
  }
  if (Index < 1)
  {
    Count += Index - 1;
    Index = 1;
    if (Count <= 0)
    {
      return AnsiStringT();
    }
  }
  const std::size_t Start = static_cast<std::size_t>(Index - 1);
  AnsiStringT Result;
  Result.FData = FData.substr(Start, std::min(static_cast<std::size_t>(Count),
                                              FData.size() - Start));
  return Result;
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::Pos(const AnsiStringT & Substr) const
{
  const std::size_t P = FData.find(Substr.FData);
  return P == std::string::npos ? 0 : static_cast<int>(P) + 1;
}

template <unsigned short CodePage>
int AnsiStringT<CodePage>::Pos(char C) const
{
  const std::size_t P = FData.find(C);
  return P == std::string::npos ? 0 : static_cast<int>(P) + 1;
}

template <unsigned short CodePage>
void AnsiStringT<CodePage>::Delete(int Index, int Count)
{
  if (Count <= 0 || Index > Length())
  {
    return;
  }
  if (Index < 1)
  {
    Count += Index - 1;
    Index = 1;
    if (Count <= 0)
    {
      return;
    }
  }
  const std::size_t Start = static_cast<std::size_t>(Index - 1);
  FData.erase(Start, std::min(static_cast<std::size_t>(Count),
                              FData.size() - Start));
}

template <unsigned short CodePage>
void AnsiStringT<CodePage>::Insert(const AnsiStringT & S, int Index)
{
  const int Clamped = std::min(std::max(Index, 1), Length() + 1);
  FData.insert(static_cast<std::size_t>(Clamped - 1), S.FData);
}

template <unsigned short CodePage>
void AnsiStringT<CodePage>::Assign(const UnicodeString & S)
{
  if (CodePage == CP_UTF8_SHIM)
  {
    FData = linscp::Utf16ToUtf8(S);
  }
  else if (CodePage == CP_RAW_SHIM)
  {
    FData = linscp::Utf16ToRaw(S);
  }
  else
  {
    FData = linscp::Utf16ToLatin1(S);
  }
}

// The three code pages core uses are the only instantiations.
template class AnsiStringT<CP_ACP_SHIM>;
template class AnsiStringT<CP_UTF8_SHIM>;
template class AnsiStringT<CP_RAW_SHIM>;

UnicodeString ToUnicodeString(const AnsiString & S)
{
  return linscp::Latin1ToUtf16(S.c_str(), static_cast<std::size_t>(S.Length()));
}

UnicodeString ToUnicodeString(const UTF8String & S)
{
  return linscp::Utf8ToUtf16(S.c_str(), static_cast<std::size_t>(S.Length()));
}

UnicodeString ToUnicodeString(const RawByteString & S)
{
  return linscp::RawToUtf16(S.c_str(), static_cast<std::size_t>(S.Length()));
}
