#include "vcl_misc.h"
#include <exception>
#include <cstring>
#include <cstdio>
#include <initializer_list>
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
  TBytes GetPreamble() const override { return Bytes({0xEF, 0xBB, 0xBF}); }

protected:
  std::string Encode(const UnicodeString & S) const override
    { return linscp::Utf16ToUtf8(S); }
  UnicodeString Decode(const unsigned char * B, std::size_t N) const override
    { return linscp::Utf8ToUtf16(reinterpret_cast<const char *>(B), N); }

public:
  static TBytes Bytes(std::initializer_list<unsigned char> Values)
  {
    TBytes Result;
    Result.Length = static_cast<int>(Values.size());
    int I = 0;
    for (unsigned char V : Values)
    {
      Result[I++] = V;
    }
    return Result;
  }
};

// The shim's AnsiString approximation: Latin-1.
class TAnsiEncoding : public TEncoding
{
protected:
  std::string Encode(const UnicodeString & S) const override
    { return linscp::Utf16ToLatin1(S); }
  UnicodeString Decode(const unsigned char * B, std::size_t N) const override
    { return linscp::Latin1ToUtf16(reinterpret_cast<const char *>(B), N); }
};

// Seven-bit ASCII; anything outside it becomes '?'.
class TAsciiEncoding : public TEncoding
{
protected:
  std::string Encode(const UnicodeString & S) const override
  {
    std::string Result;
    for (int I = 1; I <= S.Length(); ++I)
    {
      Result += (S[I] < 0x80) ? static_cast<char>(S[I]) : '?';
    }
    return Result;
  }
  UnicodeString Decode(const unsigned char * B, std::size_t N) const override
  {
    UnicodeString Result;
    for (std::size_t I = 0; I < N; ++I)
    {
      Result += (B[I] < 0x80) ? static_cast<wchar_t>(B[I]) : L'?';
    }
    return Result;
  }
};

class TUtf16Encoding : public TEncoding
{
public:
  constexpr explicit TUtf16Encoding(bool BigEndian) : FBigEndian(BigEndian) {}
  TBytes GetPreamble() const override
  {
    return FBigEndian ? TUtf8Encoding::Bytes({0xFE, 0xFF})
                      : TUtf8Encoding::Bytes({0xFF, 0xFE});
  }

protected:
  std::string Encode(const UnicodeString & S) const override
  {
    std::string Result;
    for (int I = 1; I <= S.Length(); ++I)
    {
      const unsigned Unit = static_cast<unsigned>(S[I]);
      const char Hi = static_cast<char>(Unit >> 8), Lo = static_cast<char>(Unit & 0xFF);
      Result += FBigEndian ? Hi : Lo;
      Result += FBigEndian ? Lo : Hi;
    }
    return Result;
  }
  UnicodeString Decode(const unsigned char * B, std::size_t N) const override
  {
    UnicodeString Result;
    for (std::size_t I = 0; I + 1 < N; I += 2)   // a trailing odd byte is dropped
    {
      const unsigned Unit = FBigEndian ? ((B[I] << 8) | B[I + 1]) : (B[I] | (B[I + 1] << 8));
      Result += static_cast<wchar_t>(Unit);
    }
    return Result;
  }

private:
  bool FBigEndian;
};

// Constant-initialised, which the attribute makes a compile-time guarantee: no
// other translation unit's static initialiser can reach one before its vtable
// is in place.
[[clang::require_constant_initialization]] TUtf8Encoding Utf8Instance;
[[clang::require_constant_initialization]] TAnsiEncoding AnsiInstance;
[[clang::require_constant_initialization]] TAsciiEncoding AsciiInstance;
[[clang::require_constant_initialization]] TUtf16Encoding UnicodeInstance(false);
[[clang::require_constant_initialization]] TUtf16Encoding BigEndianInstance(true);

bool StartsWith(const TBytes & Buffer, const TBytes & Prefix)
{
  if ((Prefix.Length == 0) || (Buffer.Length < Prefix.Length))
  {
    return false;
  }
  for (int I = 0; I < Prefix.Length; ++I)
  {
    if (Buffer[I] != Prefix[I])
    {
      return false;
    }
  }
  return true;
}

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

// Addresses of namespace-scope objects: constant-initialised, so usable from
// any other translation unit's static initialisation.
TEncoding * const TEncoding::ASCII = &AsciiInstance;
TEncoding * const TEncoding::ANSI = &AnsiInstance;
TEncoding * const TEncoding::UTF8 = &Utf8Instance;
TEncoding * const TEncoding::Unicode = &UnicodeInstance;
TEncoding * const TEncoding::BigEndianUnicode = &BigEndianInstance;
TEncoding * const TEncoding::Default = &Utf8Instance;

TBytes TEncoding::GetPreamble() const
{
  return TBytes();
}

TBytes TEncoding::GetBytes(const UnicodeString & S) const
{
  const std::string Encoded = Encode(S);
  TBytes Result;
  Result.Length = static_cast<int>(Encoded.size());
  for (int I = 0; I < Result.Length; ++I)
  {
    Result[I] = static_cast<unsigned char>(Encoded[static_cast<std::size_t>(I)]);
  }
  return Result;
}

UnicodeString TEncoding::GetString(const TBytes & Bytes) const
{
  return GetString(Bytes, 0, Bytes.Length);
}

UnicodeString TEncoding::GetString(const TBytes & Bytes, int ByteIndex, int ByteCount) const
{
  if ((ByteIndex < 0) || (ByteCount < 0) || (ByteIndex > Bytes.Length - ByteCount))
  {
    throw EEncodingError(UnicodeString(L"Byte index or count out of bounds"));
  }
  if (ByteCount == 0)
  {
    return UnicodeString();
  }
  return Decode(&Bytes[ByteIndex], static_cast<std::size_t>(ByteCount));
}

int TEncoding::GetBufferEncoding(const TBytes & Buffer, TEncoding *& AEncoding)
{
  return GetBufferEncoding(Buffer, AEncoding, Default);
}

int TEncoding::GetBufferEncoding(const TBytes & Buffer, TEncoding *& AEncoding,
                                 TEncoding * ADefaultEncoding)
{
  if (AEncoding != nullptr)
  {
    const TBytes Preamble = AEncoding->GetPreamble();
    return StartsWith(Buffer, Preamble) ? Preamble.Length : 0;
  }
  for (TEncoding * Candidate : {Unicode, BigEndianUnicode, UTF8})
  {
    const TBytes Preamble = Candidate->GetPreamble();
    if (StartsWith(Buffer, Preamble))
    {
      AEncoding = Candidate;
      return Preamble.Length;
    }
  }
  AEncoding = ADefaultEncoding;
  return 0;
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

TObject * ExceptObject()
{
  const std::exception_ptr Current = std::current_exception();
  if (!Current)
  {
    return nullptr;
  }
  try
  {
    std::rethrow_exception(Current);
  }
  catch (TObject & E)
  {
    return &E;      // the handled object, which the outer handler keeps alive
  }
  catch (...)
  {
    return nullptr;
  }
}

void * ExceptAddr()
{
  return nullptr;
}

int ExceptionErrorMessage(TObject * ExceptObject, void * ExceptAddr,
                          wchar_t * Buffer, int Size)
{
  if ((Buffer == nullptr) || (Size <= 0))
  {
    return 0;
  }
  UnicodeString Msg;
  if (const Exception * E = dynamic_cast<const Exception *>(ExceptObject))
  {
    Msg = E->Message;
  }
  if (!Msg.IsEmpty() && (Msg[Msg.Length()] > L'.'))
  {
    Msg += L".";
  }
  char Address[32];
  std::snprintf(Address, sizeof(Address), "%p", ExceptAddr);
  const UnicodeString Text =
    UnicodeString(L"Exception ") +
    ((ExceptObject != nullptr) ? ExceptObject->ClassName() : UnicodeString(L"(unknown)")) +
    L" in module " + ExtractFileName(ParamStr(0)) + L" at " + UnicodeString(Address) +
    L"." + sLineBreak + Msg;
  const int N = (Text.Length() < Size - 1) ? Text.Length() : Size - 1;
  for (int I = 0; I < N; ++I)
  {
    Buffer[I] = Text[I + 1];
  }
  Buffer[N] = L'\0';
  return N;
}

namespace {
const char Base64Alphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

UnicodeString EncodeBase64(const void * Input, int Size)
{
  const unsigned char * In = static_cast<const unsigned char *>(Input);
  UnicodeString Result;
  int Line = 0;
  for (int I = 0; I < Size; I += 3)
  {
    const unsigned B0 = In[I];
    const unsigned B1 = (I + 1 < Size) ? In[I + 1] : 0;
    const unsigned B2 = (I + 2 < Size) ? In[I + 2] : 0;
    const wchar_t Quad[4] = {
      static_cast<wchar_t>(Base64Alphabet[B0 >> 2]),
      static_cast<wchar_t>(Base64Alphabet[((B0 & 3) << 4) | (B1 >> 4)]),
      static_cast<wchar_t>((I + 1 < Size) ? Base64Alphabet[((B1 & 15) << 2) | (B2 >> 6)] : '='),
      static_cast<wchar_t>((I + 2 < Size) ? Base64Alphabet[B2 & 63] : '='),
    };
    for (wchar_t C : Quad)
    {
      if (Line == 76)
      {
        Result += sLineBreak;
        Line = 0;
      }
      Result += C;
      ++Line;
    }
  }
  return Result;
}

TBytes DecodeBase64(const UnicodeString & Input)
{
  std::string Out;
  unsigned Acc = 0;
  int Bits = 0;
  for (int I = 1; I <= Input.Length(); ++I)
  {
    const wchar_t C = Input[I];
    if (C == L'=')
    {
      break;
    }
    const char * P = (C < 0x80) ? std::strchr(Base64Alphabet, static_cast<char>(C)) : nullptr;
    if ((P == nullptr) || (C == 0))
    {
      continue;
    }
    Acc = (Acc << 6) | static_cast<unsigned>(P - Base64Alphabet);
    Bits += 6;
    if (Bits >= 8)
    {
      Bits -= 8;
      Out += static_cast<char>((Acc >> Bits) & 0xFF);
    }
  }
  TBytes Result;
  Result.Length = static_cast<int>(Out.size());
  for (int I = 0; I < Result.Length; ++I)
  {
    Result[I] = static_cast<unsigned char>(Out[static_cast<std::size_t>(I)]);
  }
  return Result;
}

const TTimeSpan TTimeSpan::Zero(0);
