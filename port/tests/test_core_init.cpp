// Core's own startup and shutdown, through the headless frontend.
//
// HeadlessInitialize creates the application log, then CoreInitialize creates the configuration (CreateConfiguration), starts
// PuTTY, FileZilla and neon, and loads the configuration and stored sessions;
// CoreFinalize (from HeadlessFinalize) saves and tears all of it down. This is the first test that
// runs core code end to end rather than linking it. Run with /ini=nul, so
// storage is the null backend and nothing is read from or written to the
// registry of the machine (or Wine prefix) running it.
#include <vcl.h>
#pragma hdrstop

#include <cstdio>

#include <Common.h>
#include <Configuration.h>
#include <CoreMain.h>
#include <Interface.h>
#include <SessionData.h>
#include "HeadlessMain.h"

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

}  // namespace

int main()
{
  try
  {
    HeadlessInitialize();
    CHECK(Configuration != NULL);
    CHECK(StoredSessions != NULL);
    if (Configuration != NULL)
    {
      CHECK(Configuration->Storage == stNul);
      std::printf("version: %s\n", UTF8String(Configuration->Version).c_str());
      std::printf("ssh banner: %s\n", UTF8String(SshVersionString()).c_str());
      CHECK(SshVersionString().Pos(L"linscp-release-") == 1);
    }
    if (StoredSessions != NULL)
    {
      // Nothing stored under the null backend.
      CHECK(StoredSessions->Count == 0);
    }
    HeadlessFinalize();
    CHECK(Configuration == NULL);
    CHECK(StoredSessions == NULL);
  }
  catch (Exception & E)
  {
    std::printf("FAIL exception: %s\n", UTF8String(E.Message).c_str());
    ++Failures;
  }
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
