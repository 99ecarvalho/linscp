//---------------------------------------------------------------------------
// UnicodeString — the VCL string type, reimplemented for clang.
//
// The single most-used type in WinSCP's core: 6,248 references. Two design
// points dominate, both chosen to let upstream source compile unmodified.
//
// ONE-BASED INDEXING
//   Delphi strings are 1-based: S[1] is the first character. Core relies on this
//   everywhere, so SubString, Pos, Delete, Insert and LastDelimiter are all
//   1-based too, and Pos returns 0 for "not found". Getting this wrong would
//   produce off-by-one corruption rather than compile errors, so every boundary
//   is covered by tests.
//
// CHARACTER WIDTH  (see doc/qt/adr/0001-wchar-t-width.md)
//   Delphi's UnicodeString is UTF-16. We build with -fshort-wchar so wchar_t is
//   2 bytes on every target and upstream's L"..." literals keep working.
//
//   Storage is std::u16string, NOT std::wstring: libstdc++'s
//   char_traits<wchar_t> calls wmemcmp/wcslen/wmemchr from libc, which are
//   compiled for 4-byte wchar_t and would be an ABI mismatch under
//   -fshort-wchar. char_traits<char16_t> is a separate, safe specialisation.
//   wchar_t and char16_t are both 2-byte and identically represented here, so
//   the two reinterpret_casts at the wchar_t boundary are confined to this file.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_UNICODESTRING_H
#define LINSCP_VCL_UNICODESTRING_H

#include <cstddef>
#include <string>
#include <type_traits>

static_assert(sizeof(wchar_t) == 2,
              "linscp requires -fshort-wchar: UnicodeString is UTF-16 and "
              "upstream assumes a 2-byte wchar_t (see ADR 0001)");

class UnicodeString
{
public:
  // C++Builder's static form of StringOfChar.
  static UnicodeString StringOfChar(wchar_t C, int Count)
  {
    UnicodeString Result;
    for (int I = 0; I < Count; ++I)
    {
      Result += C;
    }
    return Result;
  }

  using size_type = int;

  UnicodeString() = default;
  UnicodeString(const UnicodeString &) = default;
  UnicodeString(UnicodeString &&) noexcept = default;
  UnicodeString & operator=(const UnicodeString &) = default;
  UnicodeString & operator=(UnicodeString &&) noexcept = default;
  ~UnicodeString() = default;

  UnicodeString(const wchar_t * S);
  UnicodeString(const wchar_t * S, int Len);

  // Narrow literals. C++Builder accepts them and core relies on it in 161
  // places, e.g. SameText(Value, "on"). Decoded as UTF-8, which is a superset
  // of the ASCII those literals actually are.
  UnicodeString(const char * S);

  // A single character, implicitly, as in C++Builder: core passes a bare
  // wchar_t constant to functions taking a string. Constrained to exactly
  // wchar_t so an integer cannot quietly become a one-character string.
  template <typename C,
            typename = std::enable_if_t<std::is_same_v<C, wchar_t>>>
  UnicodeString(C Ch) : FData(1, static_cast<char16_t>(Ch)) {}

  // Numbers, implicitly, producing their decimal representation. C++Builder
  // has these and core depends on one: EXT_EXCEPTION_METHODS calls
  // ExtException(E, Ident) with an int, and the only viable overload takes a
  // UnicodeString, so the id becomes its own message text.
  //
  // The hazard is real -- `UnicodeString s = 0;` yields "0" rather than an
  // empty string -- but C++Builder behaves the same way, and diverging would
  // mean editing core.
  UnicodeString(int Value);
  UnicodeString(unsigned int Value);
  UnicodeString(__int64 Value);
  UnicodeString(double Value);

  // Repeated character; explicit, since the count makes intent clear anyway.
  explicit UnicodeString(wchar_t C, int Count);

  // --- length ---------------------------------------------------------------
  int Length() const { return static_cast<int>(FData.size()); }
  bool IsEmpty() const { return FData.empty(); }
  void SetLength(int NewLength);

  // --- raw access -----------------------------------------------------------
  // Null-terminated. Never returns nullptr: an empty string yields L"".
  //
  // The non-const overload hands back a *writable* buffer, as C++Builder's
  // does. Core depends on that: DoLoadStrFrom calls SetLength and then passes
  // c_str() to LoadString as the destination to fill.
  const wchar_t * c_str() const;
  wchar_t * c_str();
  const wchar_t * data() const { return c_str(); }
  wchar_t * MutableData();

  // --- element access (1-based) --------------------------------------------
  // Index must be in [1, Length()]. Out-of-range is a programming error and is
  // checked by assert in debug builds.
  wchar_t operator[](int Index) const;
  wchar_t & operator[](int Index);

  // --- substrings (1-based, clamped like Delphi's Copy) --------------------
  UnicodeString SubString(int Index, int Count) const;
  UnicodeString SubString(int Index) const;

  // --- searching (1-based; 0 means not found) ------------------------------
  int Pos(const UnicodeString & Substr) const;
  int Pos(wchar_t C) const;
  int LastDelimiter(const UnicodeString & Delimiters) const;
  int FirstDelimiter(const UnicodeString & Delimiters) const;
  bool IsDelimiter(const UnicodeString & Delimiters, int Index) const;

  // --- mutation (1-based) --------------------------------------------------
  void Delete(int Index, int Count);
  void Insert(const UnicodeString & S, int Index);
  void Unique() {}   // no copy-on-write here; present for source compatibility

  // --- case and whitespace -------------------------------------------------
  // ASCII-only case folding. Delphi's versions are locale-aware; the paths core
  // uses these on are protocol tokens and switch names, which are ASCII.
  // Anything user-facing must not rely on this. Tracked as a known limitation.
  UnicodeString LowerCase() const;
  UnicodeString UpperCase() const;
  UnicodeString Trim() const;
  UnicodeString TrimLeft() const;
  UnicodeString TrimRight() const;

  // --- comparison ----------------------------------------------------------
  int Compare(const UnicodeString & S) const;
  int CompareIC(const UnicodeString & S) const;   // case-insensitive, ASCII

  // --- operators -----------------------------------------------------------
  UnicodeString & operator+=(const UnicodeString & S);
  UnicodeString & operator+=(wchar_t C);

  friend bool operator==(const UnicodeString & A, const UnicodeString & B)
    { return A.FData == B.FData; }
  friend bool operator!=(const UnicodeString & A, const UnicodeString & B)
    { return A.FData != B.FData; }
  friend bool operator<(const UnicodeString & A, const UnicodeString & B)
    { return A.FData < B.FData; }
  friend bool operator>(const UnicodeString & A, const UnicodeString & B)
    { return A.FData > B.FData; }
  friend bool operator<=(const UnicodeString & A, const UnicodeString & B)
    { return A.FData <= B.FData; }
  friend bool operator>=(const UnicodeString & A, const UnicodeString & B)
    { return A.FData >= B.FData; }

  friend UnicodeString operator+(const UnicodeString & A, const UnicodeString & B);

  // Concatenating a single character. A wchar_t does not convert to
  // UnicodeString implicitly -- the single-character constructor is explicit so
  // that an accidental integer cannot become a string -- so these are needed on
  // their own. Core writes `Path + L'/'` in a number of places.
  friend UnicodeString operator+(const UnicodeString & A, wchar_t C);
  friend UnicodeString operator+(wchar_t C, const UnicodeString & B);

private:
  std::u16string FData;
};

#endif // LINSCP_VCL_UNICODESTRING_H
