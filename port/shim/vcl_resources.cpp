#include "vcl_resources.h"

#include <algorithm>

namespace linscp {

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

}  // namespace

void * HInstance = reinterpret_cast<void *>(1);
void * LibModuleList = &MainModule;

int LoadString(void * Module, unsigned Ident, wchar_t * Buffer, int BufferMax)
{
  (void)Module;   // one module; the handle is not used to select a table
  if (Buffer == nullptr || BufferMax <= 0)
  {
    return 0;
  }
  const wchar_t * Text = linscp::FindResourceString(static_cast<int>(Ident));
  if (Text == nullptr)
  {
    Buffer[0] = L'\0';
    return 0;   // Win32 also returns 0 for a missing id
  }
  int Length = 0;
  // Leave room for the terminator, as Win32 does: it truncates rather than
  // overrunning, and reports the truncated length.
  while (Text[Length] != L'\0' && Length < BufferMax - 1)
  {
    Buffer[Length] = Text[Length];
    ++Length;
  }
  Buffer[Length] = L'\0';
  return Length;
}

UnicodeString LoadStr(NativeUInt Ident)
{
  wchar_t Buffer[1024];
  const int Length = LoadString(HInstance, static_cast<unsigned>(Ident), Buffer,
                                static_cast<int>(sizeof(Buffer) / sizeof(Buffer[0])));
  return UnicodeString(Buffer, Length);
}
