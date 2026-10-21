// The report behind a Debug build's assertions.
//
// With _DEBUG defined, core's DebugAssert, DebugCheck and DebugFail
// (core/Global.h) call DoAssert, and PuTTY's assert (putty/putty.h) calls
// DoAssertC. Both are declared upstream but defined in no published source
// file, so a Debug build has to bring them. They live in the shim because
// every program links it, including tests that link the PuTTY libraries
// without core.
//
// A failed assertion is reported on standard error, as the C library's assert
// reports one, and the process aborts, so a debugger stops on it, a test fails
// on it and nothing carries on with state the code has just declared
// impossible. LINSCP_ASSERT=continue keeps going after the report instead, to
// get past one known assertion and find the next.
//
// Defined in every configuration, though only a Debug build calls them, so
// the behaviour can be tested in any build.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "vcl_bytestring.h"
#include "vcl_unicodestring.h"

namespace {

void Report(const char * Message, const char * Filename, int LineNumber)
{
  std::fprintf(stderr, "Assertion failed: %s, file %s, line %d\n",
    (Message != nullptr) ? Message : "?", (Filename != nullptr) ? Filename : "?",
    LineNumber);
  std::fflush(stderr);
  const char * Mode = std::getenv("LINSCP_ASSERT");
  if ((Mode == nullptr) || (std::strcmp(Mode, "continue") != 0))
  {
    std::abort();
  }
}

UTF8String Narrow(const wchar_t * S)
{
  return (S != nullptr) ? UTF8String(UnicodeString(S)) : UTF8String("?");
}

}

// core/Global.h's. Its TEXT() strings are wide: the core projects map _TCHAR
// to wchar_t.
void DoAssert(const wchar_t * Message, const wchar_t * Filename, int LineNumber)
{
  Report(Narrow(Message).c_str(), Narrow(Filename).c_str(), LineNumber);
}

// putty/putty.h's, called from C. PuTTY's project leaves _TCHAR as char, so
// its TEXT() strings are narrow.
extern "C" void DoAssertC(char * Message, char * Filename, int LineNumber)
{
  Report(Message, Filename, LineNumber);
}
