#include "vcl_resources.h"

#include "vcl_strutils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace linscp {

namespace {
TResourceStringFilter GResourceStringFilter = nullptr;
}

void SetResourceStringFilter(TResourceStringFilter Filter)
{
  GResourceStringFilter = Filter;
}

const wchar_t * FilterResourceString(unsigned Ident, const wchar_t * Text)
{
  if ((Text != nullptr) && (GResourceStringFilter != nullptr))
  {
    if (const wchar_t * Filtered = GResourceStringFilter(Ident, Text))
    {
      return Filtered;
    }
  }
  return Text;
}

const wchar_t * FindResourceString(int Ident)
{
  // The generated table is sorted by id, so a binary search is available and
  // the generator refuses to emit duplicates.
  const auto * Begin = ResourceStrings;
  const auto * End = ResourceStrings + ResourceStringCount;
  const auto * It = std::lower_bound(
    Begin, End, Ident,
    [](const TResourceString & Row, int Want) { return Row.Ident < Want; });
  if (It != End && It->Ident == Ident)
  {
    return It->Text;
  }
  return nullptr;
}

}  // namespace linscp

namespace {

// A single module describes this program. The value is deliberately small:
// core's FindModule compares `CurModule->Instance == (unsigned)Instance`, which
// truncates on 64-bit, so anything that survives truncation works and anything
// large might not.
TLibModule MainModule = { 1, 1, 1, 1, nullptr };

// A loaded translation: its strings by id, sorted.
struct TTranslationModule
{
  std::vector<std::pair<int, UnicodeString>> Strings;

  const wchar_t * Find(int Ident) const
  {
    const auto It = std::lower_bound(Strings.begin(), Strings.end(), Ident,
      [](const std::pair<int, UnicodeString> & Row, int Want) { return Row.first < Want; });
    return ((It != Strings.end()) && (It->first == Ident)) ? It->second.c_str() : nullptr;
  }
};

// The handles LoadTranslationModule gave, so LoadString tells them from the
// main module's.
std::mutex GTranslationsSection;
std::set<const TTranslationModule *> GTranslations;

// gen_translations.py's escapes.
std::string Unescape(const std::string & Text)
{
  std::string Result;
  Result.reserve(Text.size());
  for (std::size_t I = 0; I < Text.size(); I++)
  {
    if ((Text[I] == '\\') && (I + 1 < Text.size()))
    {
      const char C = Text[++I];
      Result += (C == 'n') ? '\n' : (C == 'r') ? '\r' : (C == 't') ? '\t' : C;
    }
    else
    {
      Result += Text[I];
    }
  }
  return Result;
}

}  // namespace

void * HInstance = reinterpret_cast<void *>(1);
void * LibModuleList = &MainModule;

void * LoadTranslationModule(const UnicodeString & FileName)
{
#if defined(_WIN32)
  std::FILE * File = _wfopen(FileName.c_str(), L"rb");  // ADR 0001: Windows CRT
#else
  std::FILE * File = std::fopen(UTF8String(FileName).c_str(), "rb");
#endif
  if (File == nullptr)
  {
    return nullptr;
  }
  std::string Content;
  char Chunk[65536];
  std::size_t Read;
  while ((Read = std::fread(Chunk, 1, sizeof(Chunk), File)) > 0)
  {
    Content.append(Chunk, Read);
  }
  std::fclose(File);
  if (Content.compare(0, 19, "linscp translation ") != 0)
  {
    return nullptr;
  }
  auto * Module = new TTranslationModule();
  std::size_t Pos = Content.find('\n');
  while ((Pos != std::string::npos) && (Pos + 1 < Content.size()))
  {
    const std::size_t Start = Pos + 1;
    Pos = Content.find('\n', Start);
    const std::string Line = Content.substr(Start, (Pos == std::string::npos) ? std::string::npos : Pos - Start);
    const std::size_t Tab = Line.find('\t');
    if (Tab != std::string::npos)
    {
      Module->Strings.emplace_back(std::atoi(Line.substr(0, Tab).c_str()),
        UTF8ToString(Unescape(Line.substr(Tab + 1)).c_str()));
    }
  }
  std::sort(Module->Strings.begin(), Module->Strings.end(),
    [](const std::pair<int, UnicodeString> & A, const std::pair<int, UnicodeString> & B) { return A.first < B.first; });
  std::lock_guard<std::mutex> Lock(GTranslationsSection);
  GTranslations.insert(Module);
  return Module;
}

void FreeTranslationModule(void * Module)
{
  auto * Translation = static_cast<TTranslationModule *>(Module);
  {
    std::lock_guard<std::mutex> Lock(GTranslationsSection);
    if (GTranslations.erase(Translation) == 0)
    {
      return;
    }
  }
  delete Translation;
}

void * ChangeResourceModule(void * Module)
{
  void * Result = reinterpret_cast<void *>(MainModule.ResInstance);
  MainModule.ResInstance = reinterpret_cast<std::uintptr_t>((Module != nullptr) ? Module : HInstance);
  return Result;
}

namespace {

// Core's texts name the program "WinSCP" ("the key WinSCP has cached", "quit
// other instances of WinSCP"); the program here is linscp. The word alone is
// replaced, in every translation (a name, they keep it): not a longer name it
// begins (WinSCPnet, WinSCP.ini), nor the .NET assembly, whose name it is.
// Copies TEXT into BUFFER as LoadString does; returns the length copied.
int CopyProgramNamed(const wchar_t * Text, wchar_t * Buffer, int BufferMax)
{
  static const wchar_t Word[] = L"WinSCP";
  static const wchar_t Name[] = L"linscp";
  static const wchar_t Assembly[] = L" .NET";
  // ASCII only: a letter of another script next to the name (a CJK
  // particle) does not make it part of a longer one. (<cwchar> is not for
  // this -fshort-wchar code: hand-written comparisons.)
  const auto Alnum = [](wchar_t C)
  {
    return ((C >= L'a') && (C <= L'z')) || ((C >= L'A') && (C <= L'Z')) || ((C >= L'0') && (C <= L'9')) || (C == L'_');
  };
  const auto StartsWith = [](const wchar_t * At, const wchar_t * Prefix)
  {
    for (; *Prefix != L'\0'; ++At, ++Prefix)
    {
      if (*At != *Prefix)
      {
        return false;
      }
    }
    return true;
  };
  const int WordLength = static_cast<int>(sizeof(Word) / sizeof(Word[0])) - 1;
  int Length = 0;
  // Leave room for the terminator, as Win32 does: it truncates rather than
  // overrunning, and reports the truncated length.
  for (int I = 0; (Text[I] != L'\0') && (Length < BufferMax - 1);)
  {
    const wchar_t * After = Text + I + WordLength;
    const bool Found = StartsWith(Text + I, Word) && ((I == 0) || !Alnum(Text[I - 1]));
    const bool Longer = Found && (Alnum(After[0]) || ((After[0] == L'.') && Alnum(After[1])));
    if (Found && !Longer && !StartsWith(After, Assembly))
    {
      for (const wchar_t * N = Name; (*N != L'\0') && (Length < BufferMax - 1); ++N)
      {
        Buffer[Length++] = *N;
      }
      I += WordLength;
    }
    else
    {
      Buffer[Length++] = Text[I++];
    }
  }
  Buffer[Length] = L'\0';
  return Length;
}

}  // namespace

int LoadString(void * Module, unsigned Ident, wchar_t * Buffer, int BufferMax)
{
  if (Buffer == nullptr || BufferMax <= 0)
  {
    return 0;
  }
  const wchar_t * Text = nullptr;
  {
    std::lock_guard<std::mutex> Lock(GTranslationsSection);
    const auto * Translation = static_cast<const TTranslationModule *>(Module);
    if (GTranslations.count(Translation) > 0)
    {
      Text = Translation->Find(static_cast<int>(Ident));
    }
  }
  if (Text == nullptr)
  {
    Text = linscp::FindResourceString(static_cast<int>(Ident));
  }
  Text = linscp::FilterResourceString(Ident, Text);
  if (Text == nullptr)
  {
    Buffer[0] = L'\0';
    return 0;   // Win32 also returns 0 for a missing id
  }
  return CopyProgramNamed(Text, Buffer, BufferMax);
}

UnicodeString LoadStr(NativeUInt Ident)
{
  wchar_t Buffer[1024];
  // The main module's resources: the translation in use, if any (as
  // Delphi's LoadStr reads FindResourceHInstance(HInstance)).
  const int Length = LoadString(reinterpret_cast<void *>(MainModule.ResInstance), static_cast<unsigned>(Ident), Buffer,
                                static_cast<int>(sizeof(Buffer) / sizeof(Buffer[0])));
  return UnicodeString(Buffer, Length);
}
