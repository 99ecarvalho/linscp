// Resource strings.
//
// The table is generated from upstream's .rc files, so these tests check the
// pipeline end to end: that known ids resolve, that the .rc escapes and
// multi-line concatenation survived the generator, and that LoadString behaves
// as core's DoLoadStrFrom expects -- including truncation, which core relies on
// to size the result.
#include "vcl_resources.h"

#include <cstdio>
#include <cstring>

// Ids come from upstream's own header, so a renumbering upstream fails here
// rather than silently shifting every string.
#include "TextsCore.h"

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

bool Same(const wchar_t * A, const wchar_t * B)
{
  if (A == nullptr || B == nullptr)
  {
    return A == B;
  }
  while (*A != L'\0' && *A == *B)
  {
    ++A;
    ++B;
  }
  return *A == *B;
}

void TestTableIsPopulatedAndSorted()
{
  CHECK(linscp::ResourceStringCount > 500);
  for (std::size_t I = 1; I < linscp::ResourceStringCount; ++I)
  {
    // Sorted and without duplicates, which the binary search depends on.
    CHECK(linscp::ResourceStrings[I - 1].Ident <
          linscp::ResourceStrings[I].Ident);
  }
}

void TestKnownStrings()
{
  // A plain string.
  const wchar_t * Failed = linscp::FindResourceString(CONNECTION_FAILED);
  CHECK(Failed != nullptr);
  CHECK(Same(Failed, L"Connection failed."));

  // One carrying format specifiers and an escaped newline, which is where a
  // generator mistake would show up.
  const wchar_t * Cmd = linscp::FindResourceString(COMMAND_FAILED2);
  CHECK(Cmd != nullptr);
  CHECK(Same(Cmd, L"Command '%s'\nfailed with return code %d and the "
                  L"following error message."));

  // One assembled from adjacent literals across several .rc lines.
  const wchar_t * Help = linscp::FindResourceString(SCRIPT_EXIT_HELP);
  CHECK(Help != nullptr);
  CHECK(Same(Help, L"exit\n  Closes all sessions and terminates the "
                   L"program.\nalias:\n  bye\n"));

  CHECK(linscp::FindResourceString(999999) == nullptr);
  CHECK(linscp::FindResourceString(-1) == nullptr);
}

void TestLoadStringContract()
{
  wchar_t Buf[256];

  // The return value is the length copied, excluding the terminator: core uses
  // it to trim the string it pre-sized.
  std::memset(Buf, 0xAB, sizeof(Buf));
  const int N = LoadString(HInstance, CONNECTION_FAILED, Buf, 256);
  CHECK(N == 18);
  CHECK(Buf[N] == L'\0');
  CHECK(Same(Buf, L"Connection failed."));

  // Truncation must fit the buffer including the terminator, not overrun it.
  wchar_t Small[6];
  std::memset(Small, 0xAB, sizeof(Small));
  const int M = LoadString(HInstance, CONNECTION_FAILED, Small, 6);
  CHECK(M == 5);
  CHECK(Small[5] == L'\0');
  CHECK(Same(Small, L"Conne"));

  // An unknown id yields an empty string and 0, as Win32 does.
  CHECK(LoadString(HInstance, 999999, Buf, 256) == 0);
  CHECK(Buf[0] == L'\0');

  // Degenerate arguments must not write anywhere.
  CHECK(LoadString(HInstance, CONNECTION_FAILED, nullptr, 256) == 0);
  CHECK(LoadString(HInstance, CONNECTION_FAILED, Buf, 0) == 0);
  CHECK(LoadString(HInstance, CONNECTION_FAILED, Buf, -1) == 0);

  // A buffer of exactly 1 leaves room only for the terminator.
  wchar_t One[1];
  CHECK(LoadString(HInstance, CONNECTION_FAILED, One, 1) == 0);
  CHECK(One[0] == L'\0');
}

void TestModuleListMatchesFindModule()
{
  // core's FindModule walks LibModuleList comparing
  // `CurModule->Instance == (unsigned)Instance`, so the list must be non-empty
  // and the first entry must match HInstance under that truncating comparison.
  CHECK(LibModuleList != nullptr);
  auto * Module = static_cast<TLibModule *>(LibModuleList);
  CHECK(Module->Instance ==
        static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(HInstance)));
  CHECK(Module->ResInstance != 0);
  CHECK(Module->Next == nullptr);
}

}  // namespace

int main()
{
  TestTableIsPopulatedAndSorted();
  TestKnownStrings();
  TestLoadStringContract();
  TestModuleListMatchesFindModule();

  if (Failures == 0)
  {
    std::printf("all resource tests passed (%zu strings)\n",
                linscp::ResourceStringCount);
  }
  return Failures == 0 ? 0 : 1;
}
