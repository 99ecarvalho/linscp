// A Debug build's assertion report (port/shim/vcl_assert.cpp), for core's
// DebugAssert (DoAssert, wide strings) or PuTTY's assert (DoAssertC, narrow).
// The abort is caught and announced as "aborted", so ctest can tell it from a
// crash; "returned" after the call is reached only with
// LINSCP_ASSERT=continue.
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void DoAssert(const wchar_t * Message, const wchar_t * Filename, int LineNumber);
extern "C" void DoAssertC(char * Message, char * Filename, int LineNumber);

namespace {

extern "C" void Aborted(int)
{
  std::fputs("aborted\n", stderr);
  std::fflush(stderr);
  std::_Exit(0);
}

}

int main(int argc, char ** argv)
{
  std::signal(SIGABRT, Aborted);
  if ((argc > 1) && (std::strcmp(argv[1], "c") == 0))
  {
    char Message[] = "entry != NULL";
    char Filename[] = "conf.c";
    DoAssertC(Message, Filename, 7);
  }
  else
  {
    DoAssert(L"Count == 2 \x00e9", L"Global.cpp", 42);
  }
  std::fputs("returned\n", stderr);
  return 0;
}
