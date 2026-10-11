#include "vcl_json.h"

namespace System {
namespace Json {

namespace {

// Recursive descent over the UTF-16 text, per RFC 8259. Any failure yields
// null all the way up, and partial trees are freed on the way.
class TParser
{
public:
  explicit TParser(const UnicodeString & S) : FS(S), FN(S.Length()) {}

  TJSONValue * ParseDocument()
  {
    std::unique_ptr<TJSONValue> V(ParseValue(0));
    SkipSpace();
    return (V && (FI > FN)) ? V.release() : nullptr;   // nothing may trail
  }

private:
  static constexpr int MaxDepth = 512;

  wchar_t Peek() const { return (FI <= FN) ? FS[FI] : L'\0'; }
  bool AtEnd() const { return FI > FN; }

  void SkipSpace()
  {
    while (!AtEnd() && ((Peek() == L' ') || (Peek() == L'\t') || (Peek() == L'\n') || (Peek() == L'\r')))
    {
      ++FI;
    }
  }

  bool Literal(const wchar_t * Word)
  {
    int I = FI;
    for (; *Word != L'\0'; ++Word, ++I)
    {
      if ((I > FN) || (FS[I] != *Word))
      {
        return false;
      }
    }
    FI = I;
    return true;
  }

  TJSONValue * ParseValue(int Depth)
  {
    if (Depth > MaxDepth)
    {
      return nullptr;
    }
    SkipSpace();
    if (AtEnd())
    {
      return nullptr;
    }
    const wchar_t C = Peek();
    if (C == L'{') { return ParseObject(Depth); }
    if (C == L'[') { return ParseArray(Depth); }
    if (C == L'"')
    {
      UnicodeString S;
      return ParseString(S) ? new TJSONString(S) : nullptr;
    }
    if ((C == L'-') || ((C >= L'0') && (C <= L'9'))) { return ParseNumber(); }
    if (Literal(L"true")) { return new TJSONTrue(); }
    if (Literal(L"false")) { return new TJSONFalse(); }
    if (Literal(L"null")) { return new TJSONNull(); }
    return nullptr;
  }

  TJSONValue * ParseObject(int Depth)
  {
    ++FI;   // {
    std::unique_ptr<TJSONObject> Object(new TJSONObject());
    SkipSpace();
    if (Peek() == L'}')
    {
      ++FI;
      return Object.release();
    }
    for (;;)
    {
      SkipSpace();
      UnicodeString Name;
      if ((Peek() != L'"') || !ParseString(Name))
      {
        return nullptr;
      }
      SkipSpace();
      if (Peek() != L':')
      {
        return nullptr;
      }
      ++FI;
      TJSONValue * Value = ParseValue(Depth + 1);
      if (Value == nullptr)
      {
        return nullptr;
      }
      Object->AddPair(new TJSONPair(new TJSONString(Name), Value));
      SkipSpace();
      if (Peek() == L',') { ++FI; continue; }
      if (Peek() == L'}') { ++FI; return Object.release(); }
      return nullptr;
    }
  }

  TJSONValue * ParseArray(int Depth)
  {
    ++FI;   // [
    std::unique_ptr<TJSONArray> Array(new TJSONArray());
    SkipSpace();
    if (Peek() == L']')
    {
      ++FI;
      return Array.release();
    }
    for (;;)
    {
      TJSONValue * Value = ParseValue(Depth + 1);
      if (Value == nullptr)
      {
        return nullptr;
      }
      Array->Add(Value);
      SkipSpace();
      if (Peek() == L',') { ++FI; continue; }
      if (Peek() == L']') { ++FI; return Array.release(); }
      return nullptr;
    }
  }

  static int HexValue(wchar_t C)
  {
    if ((C >= L'0') && (C <= L'9')) return C - L'0';
    if ((C >= L'a') && (C <= L'f')) return C - L'a' + 10;
    if ((C >= L'A') && (C <= L'F')) return C - L'A' + 10;
    return -1;
  }

  bool ParseString(UnicodeString & Out)
  {
    ++FI;   // opening quote
    while (!AtEnd())
    {
      const wchar_t C = FS[FI++];
      if (C == L'"')
      {
        return true;
      }
      if (C < 0x20)
      {
        return false;          // unescaped control characters are invalid
      }
      if (C != L'\\')
      {
        Out += C;
        continue;
      }
      if (AtEnd())
      {
        return false;
      }
      const wchar_t E = FS[FI++];
      switch (E)
      {
        case L'"':  Out += L'"'; break;
        case L'\\': Out += L'\\'; break;
        case L'/':  Out += L'/'; break;
        case L'b':  Out += L'\b'; break;
        case L'f':  Out += L'\f'; break;
        case L'n':  Out += L'\n'; break;
        case L'r':  Out += L'\r'; break;
        case L't':  Out += L'\t'; break;
        case L'u':
        {
          // A UTF-16 code unit, appended as is: surrogate pairs arrive as two
          // escapes and land as the two units UnicodeString stores anyway.
          unsigned Unit = 0;
          for (int K = 0; K < 4; ++K)
          {
            const int H = AtEnd() ? -1 : HexValue(FS[FI++]);
            if (H < 0)
            {
              return false;
            }
            Unit = (Unit << 4) | static_cast<unsigned>(H);
          }
          Out += static_cast<wchar_t>(Unit);
          break;
        }
        default:
          return false;
      }
    }
    return false;              // unterminated
  }

  TJSONValue * ParseNumber()
  {
    const int Start = FI;
    auto Digits = [&]() {
      const int From = FI;
      while (!AtEnd() && (Peek() >= L'0') && (Peek() <= L'9')) { ++FI; }
      return FI > From;
    };
    if (Peek() == L'-') { ++FI; }
    if (Peek() == L'0') { ++FI; }
    else if (!Digits()) { return nullptr; }
    if (Peek() == L'.')
    {
      ++FI;
      if (!Digits()) { return nullptr; }
    }
    if ((Peek() == L'e') || (Peek() == L'E'))
    {
      ++FI;
      if ((Peek() == L'+') || (Peek() == L'-')) { ++FI; }
      if (!Digits()) { return nullptr; }
    }
    return new TJSONNumber(FS.SubString(Start, FI - Start));
  }

  const UnicodeString & FS;
  const int FN;
  int FI = 1;                  // 1-based, as UnicodeString is
};

}  // namespace

TJSONValue * TJSONObject::ParseJSONValue(const UnicodeString & Data)
{
  return TParser(Data).ParseDocument();
}

TJSONValue * TJSONObject::GetValue(const UnicodeString & Name) const
{
  for (const auto & Pair : FPairs)
  {
    if (Pair->JsonString->Value() == Name)
    {
      return Pair->JsonValue;
    }
  }
  return nullptr;
}

}  // namespace Json
}  // namespace System
