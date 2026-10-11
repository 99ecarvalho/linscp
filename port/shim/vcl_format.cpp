#include "vcl_format.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#include "vcl_object.h"
#include "vcl_strutils.h"

namespace {

// Render one value through the C library using a narrow format, then widen.
// Narrow printf is used deliberately: the wide variants are compiled against a
// 4-byte wchar_t and are off limits under -fshort-wchar (ADR 0001).
UnicodeString Rendered(const char * Spec, ...)
{
  char Buf[512];
  va_list Args;
  va_start(Args, Spec);
  std::vsnprintf(Buf, sizeof(Buf), Spec, Args);
  va_end(Args);
  UnicodeString R;
  for (const char * P = Buf; *P != '\0'; ++P)
  {
    R += static_cast<wchar_t>(static_cast<unsigned char>(*P));
  }
  return R;
}

// Pad to a field width. Done here rather than by printf because the value may
// be a UnicodeString, which printf cannot take.
UnicodeString Pad(const UnicodeString & S, int Width, bool LeftAlign)
{
  const int N = S.Length();
  if (Width <= N)
  {
    return S;
  }
  const UnicodeString Fill(L' ', Width - N);
  return LeftAlign ? S + Fill : Fill + S;
}

}  // namespace

UnicodeString TVarRec::ToDisplayString() const
{
  switch (FType)
  {
    case vtUnicodeString: return FString;
    case vtBoolean:       return UnicodeString(FInt != 0 ? L"True" : L"False");
    case vtWideChar:      return UnicodeString(static_cast<wchar_t>(FInt));
    case vtExtended:      return FloatToStr(FFloat);
    case vtPointer:       return IntToHex(reinterpret_cast<__int64>(FPointer), 8);
    default:              return IntToStr(FInt);
  }
}

UnicodeString Format(const UnicodeString & Fmt, const TFormatArgs & Args)
{
  UnicodeString Out;
  std::size_t Next = 0;
  const int N = Fmt.Length();
  int I = 1;

  while (I <= N)
  {
    if (Fmt[I] != L'%')
    {
      Out += Fmt[I];
      ++I;
      continue;
    }
    ++I;
    if (I <= N && Fmt[I] == L'%')
    {
      Out += L'%';
      ++I;
      continue;
    }

    // [index:] [-] [width] [.precision] type
    int Index = -1;
    bool LeftAlign = false;
    int Width = 0;
    int Precision = -1;

    const int SaveI = I;
    int Digits = 0;
    while (I <= N && Fmt[I] >= L'0' && Fmt[I] <= L'9')
    {
      Digits = Digits * 10 + (Fmt[I] - L'0');
      ++I;
    }
    if (I <= N && Fmt[I] == L':')
    {
      Index = Digits;      // an explicit argument index
      ++I;
    }
    else
    {
      I = SaveI;           // those digits were the width, not an index
    }

    if (I <= N && Fmt[I] == L'-')
    {
      LeftAlign = true;
      ++I;
    }
    if (I <= N && Fmt[I] == L'*')
    {
      // Width taken from the argument list, as Delphi allows.
      if (Next < Args.size())
      {
        Width = static_cast<int>(Args[Next++].AsInt());
      }
      ++I;
    }
    else
    {
      while (I <= N && Fmt[I] >= L'0' && Fmt[I] <= L'9')
      {
        Width = Width * 10 + (Fmt[I] - L'0');
        ++I;
      }
    }
    if (I <= N && Fmt[I] == L'.')
    {
      ++I;
      Precision = 0;
      if (I <= N && Fmt[I] == L'*')
      {
        if (Next < Args.size())
        {
          Precision = static_cast<int>(Args[Next++].AsInt());
        }
        ++I;
      }
      else
      {
        while (I <= N && Fmt[I] >= L'0' && Fmt[I] <= L'9')
        {
          Precision = Precision * 10 + (Fmt[I] - L'0');
          ++I;
        }
      }
    }
    if (I > N)
    {
      break;               // trailing '%' with no type: drop it, do not crash
    }

    const wchar_t Type = Fmt[I];
    ++I;

    const std::size_t Which =
      (Index >= 0) ? static_cast<std::size_t>(Index) : Next;
    if (Index < 0)
    {
      ++Next;
    }
    if (Which >= Args.size())
    {
      // Delphi raises here. Core logs user- and server-supplied text through
      // these, so emitting a visible marker beats throwing from a log path.
      Out += UnicodeString(L"<missing>");
      continue;
    }
    const TVarRec & V = Args[Which];

    UnicodeString Piece;
    switch (Type)
    {
      case L's': case L'S':
        Piece = V.ToDisplayString();
        if (Precision >= 0 && Piece.Length() > Precision)
        {
          Piece = Piece.SubString(1, Precision);
        }
        break;
      case L'd': case L'D':
        Piece = Rendered("%lld", static_cast<long long>(V.AsInt()));
        if (Precision > Piece.Length())
        {
          Piece = UnicodeString(L'0', Precision - Piece.Length()) + Piece;
        }
        break;
      case L'u': case L'U':
        Piece = Rendered("%llu",
                         static_cast<unsigned long long>(V.AsInt()));
        break;
      case L'x': case L'X':
        Piece = Rendered("%llX",
                         static_cast<unsigned long long>(V.AsInt()));
        if (Precision > Piece.Length())
        {
          Piece = UnicodeString(L'0', Precision - Piece.Length()) + Piece;
        }
        break;
      case L'f': case L'F':
        Piece = Rendered("%.*f", Precision < 0 ? 2 : Precision, V.AsFloat());
        break;
      case L'e': case L'E':
        Piece = Rendered("%.*e", Precision < 0 ? 6 : Precision, V.AsFloat());
        break;
      case L'g': case L'G':
        Piece = Rendered("%.*g", Precision < 0 ? 6 : Precision, V.AsFloat());
        break;
      case L'n': case L'N':
        Piece = Rendered("%.*f", Precision < 0 ? 2 : Precision, V.AsFloat());
        break;
      case L'p': case L'P':
        Piece = IntToHex(V.AsInt(), 8);
        break;
      default:
        // An unrecognised specifier is echoed rather than swallowed, so a
        // format-string mistake is visible instead of silently dropping text.
        Piece = UnicodeString(L'%') + UnicodeString(Type);
        if (Index < 0)
        {
          --Next;            // nothing was consumed
        }
        break;
    }
    Out += Pad(Piece, Width, LeftAlign);
  }
  return Out;
}
