#include "vcl_misc.h"
#include <string>
#include "vcl_streams.h"

#include "vcl_bytestring.h"
#include "vcl_sysutils.h"

namespace {

// Shell wildcard matching: '*' spans any run including empty, '?' one
// character. Case-insensitive, because Delphi's TMask is and core relies on it
// for file masks that users type without regard to case.
bool MatchHere(const UnicodeString & Name, int N, const UnicodeString & Mask,
               int M)
{
  const int NameLen = Name.Length();
  const int MaskLen = Mask.Length();
  while (M <= MaskLen)
  {
    const wchar_t C = Mask[M];
    if (C == L'*')
    {
      // Collapse a run of '*' and try every split point.
      while (M <= MaskLen && Mask[M] == L'*')
      {
        ++M;
      }
      if (M > MaskLen)
      {
        return true;      // trailing '*' matches the rest
      }
      for (int K = N; K <= NameLen + 1; ++K)
      {
        if (MatchHere(Name, K, Mask, M))
        {
          return true;
        }
      }
      return false;
    }
    if (N > NameLen)
    {
      return false;
    }
    if (C != L'?')
    {
      const UnicodeString A(Name[N]);
      const UnicodeString B(C);
      if (A.CompareIC(B) != 0)
      {
        return false;
      }
    }
    ++M;
    ++N;
  }
  return N > NameLen;
}

class TUtf8Encoding : public TEncoding
{
public:
  RawByteString GetBytes(const UnicodeString & S) const override
  {
    const UTF8String Utf8(S);
    return RawByteString(Utf8.c_str(), Utf8.Length());
  }
  UnicodeString GetString(const RawByteString & Bytes) const override
  {
    return linscp::Utf8ToUtf16(Bytes.c_str(),
                               static_cast<std::size_t>(Bytes.Length()));
  }
};

class TAnsiEncoding : public TEncoding
{
public:
  RawByteString GetBytes(const UnicodeString & S) const override
  {
    const AnsiString Ansi(S);
    return RawByteString(Ansi.c_str(), Ansi.Length());
  }
  UnicodeString GetString(const RawByteString & Bytes) const override
  {
    return linscp::Latin1ToUtf16(Bytes.c_str(),
                                 static_cast<std::size_t>(Bytes.Length()));
  }
};

}  // namespace

bool Masks::TMask::Matches(const UnicodeString & Filename) const
{
  return MatchHere(Filename, 1, FMask, 1);
}

bool MatchesMask(const UnicodeString & Filename, const UnicodeString & Mask)
{
  return MatchHere(Filename, 1, Mask, 1);
}

UnicodeString TPath::GetTempPath()
{
#if defined(_WIN32)
  wchar_t Buf[MAX_PATH + 1];
  const DWORD N = GetTempPathW(MAX_PATH, Buf);
  return N == 0 ? UnicodeString(L".\\") : UnicodeString(Buf, static_cast<int>(N));
#else
  const UnicodeString Tmp = GetEnvironmentVariable(UnicodeString(L"TMPDIR"));
  return IncludeTrailingBackslash(Tmp.IsEmpty() ? UnicodeString(L"/tmp") : Tmp);
#endif
}

bool TPath::IsPathRooted(const UnicodeString & Path)
{
  if (Path.IsEmpty())
  {
    return false;
  }
  if (Path[1] == L'/' || Path[1] == L'\\')
  {
    return true;
  }
  // A drive-qualified Windows path is rooted too.
  return Path.Length() >= 2 && Path[2] == L':';
}

TEncoding * TEncoding::UTF8()
{
  static TUtf8Encoding Instance;
  return &Instance;
}

TEncoding * TEncoding::ANSI()
{
  static TAnsiEncoding Instance;
  return &Instance;
}

TEncoding * TEncoding::Default()
{
  // UTF-8 on both platforms: it is what WinSCP writes and what Linux uses.
  return UTF8();
}

UnicodeString StripHotkey(const UnicodeString & Text)
{
  UnicodeString Result = Text;
  int I = 1;
  while (I <= Result.Length())
  {
    if (Result[I] == L'&')
    {
      Result.Delete(I, 1);
    }
    ++I;   // whatever followed the '&' is now at I, and is kept
  }
  return Result;
}

bool TPath::IsDriveRooted(const UnicodeString & Path)
{
#if defined(_WIN32)
  if (Path.Length() < 3)
  {
    return false;
  }
  const wchar_t Drive = Path[1];
  const bool Letter = ((Drive >= L'A') && (Drive <= L'Z')) ||
                      ((Drive >= L'a') && (Drive <= L'z'));
  return Letter && (Path[2] == L':') && ((Path[3] == L'\\') || (Path[3] == L'/'));
#else
  (void)Path;
  return false;
#endif
}

UnicodeString TFile::ReadAllText(const UnicodeString & Path)
{
  if (!FileExists(Path))
  {
    throw EFileNotFoundException(UnicodeString(L"File not found: ") + Path);
  }
  TFileStream Stream(Path, fmOpenRead | fmShareDenyNone);
  const __int64 Size = Stream.Size;
  std::string Bytes(static_cast<std::size_t>(Size), '\0');
  if (Size > 0)
  {
    Stream.ReadBuffer(&Bytes[0], static_cast<int>(Size));
  }
  const auto Byte = [&](std::size_t I) { return static_cast<unsigned char>(Bytes[I]); };
  const std::size_t N = Bytes.size();
  if ((N >= 3) && (Byte(0) == 0xEF) && (Byte(1) == 0xBB) && (Byte(2) == 0xBF))
  {
    return linscp::Utf8ToUtf16(Bytes.data() + 3, N - 3);
  }
  const bool Le = (N >= 2) && (Byte(0) == 0xFF) && (Byte(1) == 0xFE);
  const bool Be = (N >= 2) && (Byte(0) == 0xFE) && (Byte(1) == 0xFF);
  if (Le || Be)
  {
    UnicodeString Result;
    for (std::size_t I = 2; I + 1 < N; I += 2)
    {
      const unsigned Unit = Le ? (Byte(I) | (Byte(I + 1) << 8)) : ((Byte(I) << 8) | Byte(I + 1));
      Result += static_cast<wchar_t>(Unit);
    }
    return Result;
  }
  return linscp::Utf8ToUtf16(Bytes.data(), N);
}

void TFile::WriteAllText(const UnicodeString & Path, const UnicodeString & Contents)
{
  const std::string Bytes = linscp::Utf16ToUtf8(Contents);
  TFileStream Stream(Path, fmCreate);
  if (!Bytes.empty())
  {
    Stream.WriteBuffer(Bytes.data(), static_cast<int>(Bytes.size()));
  }
}
