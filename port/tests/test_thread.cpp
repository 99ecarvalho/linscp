// Threads, events and waits, as core's TSimpleThread / TSignalThread use them.
//
// Built for both targets. On Windows this runs against the real Win32 kernel
// (under Wine when cross-compiling), so every assertion that is not explicitly
// scoped to POSIX is a statement about Win32 behaviour that the POSIX layer
// must reproduce. Weighted toward semantics that fail silently: an atomic
// WaitAll, an auto-reset event releasing exactly one waiter, a suspended
// thread genuinely not running, and the thread-id identity core asserts.
#if defined(_WIN32)
  #include <windows.h>
  #include <objbase.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
#endif

#include "vcl_thread.h"

#include <atomic>
#include <chrono>
#include <cstdio>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

long long ElapsedMs(std::chrono::steady_clock::time_point Since)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - Since).count();
}

// --- TSimpleThread's protocol, replayed -------------------------------------

std::atomic<int> GRan{0};
std::atomic<DWORD> GSeenId{0};

int RecordingThread(void *)
{
  GSeenId = GetCurrentThreadId();
  ++GRan;
  return 0;
}

void TestSuspendedStartProtocol()
{
  GRan = 0;
  TThreadID Id = 0;
  // Exactly the shape of Queue.cpp: the handle travels through an int.
  const int AsInt = static_cast<int>(
    BeginThread(nullptr, 0, RecordingThread, nullptr, CREATE_SUSPENDED, Id));
  const HANDLE Thread = reinterpret_cast<HANDLE>(AsInt);
  CHECK(Thread != nullptr);
  CHECK(Id != 0);

  Sleep(100);
  CHECK(GRan == 0);  // CREATE_SUSPENDED must really hold it
  DWORD Code = 0;
  CHECK(GetExitCodeThread(Thread, &Code) && (Code == STILL_ACTIVE));
  CHECK(WaitForSingleObject(Thread, 0) == WAIT_TIMEOUT);

  CHECK(ResumeThread(Thread) == 1);  // TSimpleThread::Start keys on this
  CHECK(WaitForSingleObject(Thread, 5000) == WAIT_OBJECT_0);
  CHECK(GRan == 1);
  CHECK(ResumeThread(Thread) == 0);  // already running: count stays 0

  // TTerminalThread asserts the id StartThread reported is the thread's own.
  CHECK(GSeenId == Id);
  CHECK(GSeenId != GetCurrentThreadId());

  // A finished thread's handle stays signalled.
  CHECK(WaitForSingleObject(Thread, 0) == WAIT_OBJECT_0);
  CHECK(CloseHandle(Thread));
}

void TestThreadIdStable()
{
  const DWORD Main = GetCurrentThreadId();
  CHECK(Main != 0);
  CHECK(GetCurrentThreadId() == Main);
}

// --- exit codes ---------------------------------------------------------------

int ReturnsFortyTwo(void *) { return 42; }

std::atomic<bool> GPastEndThread{false};

void Nested()
{
  EndThread(7);
}

int EndsEarly(void *)
{
  Nested();
  GPastEndThread = true;  // must never run
  return 99;
}

void TestExitCodes()
{
  TThreadID Id;
  HANDLE A = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, ReturnsFortyTwo, nullptr, 0, Id));
  CHECK(WaitForSingleObject(A, 5000) == WAIT_OBJECT_0);
  DWORD Code = 0;
  CHECK(GetExitCodeThread(A, &Code) && (Code == 42));
  CloseHandle(A);

  GPastEndThread = false;
  HANDLE B = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, EndsEarly, nullptr, 0, Id));
  CHECK(WaitForSingleObject(B, 5000) == WAIT_OBJECT_0);
  CHECK(GetExitCodeThread(B, &Code) && (Code == 7));
  CHECK(!GPastEndThread);
  CloseHandle(B);
}

void TestSmallStackIsRoundedUp()
{
  TThreadID Id;
  HANDLE T = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 1, ReturnsFortyTwo, nullptr, 0, Id));
  CHECK(T != nullptr);
  CHECK(WaitForSingleObject(T, 5000) == WAIT_OBJECT_0);
  CloseHandle(T);
}

// --- events -------------------------------------------------------------------

void TestAutoResetConsumes()
{
  HANDLE E = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  CHECK(E != nullptr);
  CHECK(WaitForSingleObject(E, 0) == WAIT_TIMEOUT);
  CHECK(SetEvent(E));
  CHECK(WaitForSingleObject(E, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(E, 0) == WAIT_TIMEOUT);  // consumed by the wait
  CloseHandle(E);
}

void TestManualResetPersists()
{
  HANDLE E = CreateEvent(nullptr, TRUE, TRUE, nullptr);  // initially set
  CHECK(WaitForSingleObject(E, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(E, 0) == WAIT_OBJECT_0);  // not consumed
  CHECK(ResetEvent(E));
  CHECK(WaitForSingleObject(E, 0) == WAIT_TIMEOUT);
  CloseHandle(E);
}

void TestTimeoutIsHonoured()
{
  HANDLE E = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  const auto Start = std::chrono::steady_clock::now();
  CHECK(WaitForSingleObject(E, 100) == WAIT_TIMEOUT);
  // Windows timer granularity can return a tick early; allow for it.
  CHECK(ElapsedMs(Start) >= 70);
  CloseHandle(E);
}

HANDLE GTarget = nullptr;

int SetsAfterDelay(void *)
{
  Sleep(50);
  SetEvent(GTarget);
  return 0;
}

void TestCrossThreadWake()
{
  GTarget = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  TThreadID Id;
  HANDLE T = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, SetsAfterDelay, nullptr, 0, Id));
  CHECK(WaitForSingleObject(GTarget, INFINITE) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(T, 5000) == WAIT_OBJECT_0);
  CloseHandle(T);
  CloseHandle(GTarget);
}

// An auto-reset event releases one waiter per SetEvent, not all of them.
HANDLE GGate = nullptr;
std::atomic<int> GReleased{0};

int WaitsOnGate(void *)
{
  if (WaitForSingleObject(GGate, 3000) == WAIT_OBJECT_0)
  {
    ++GReleased;
  }
  return 0;
}

void TestAutoResetReleasesExactlyOne()
{
  GGate = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  GReleased = 0;
  TThreadID Id;
  HANDLE Threads[2];
  for (HANDLE & T : Threads)
  {
    T = reinterpret_cast<HANDLE>(
      BeginThread(nullptr, 0, WaitsOnGate, nullptr, 0, Id));
  }
  Sleep(200);  // let both reach the wait
  SetEvent(GGate);
  Sleep(300);
  CHECK(GReleased == 1);
  SetEvent(GGate);
  CHECK(WaitForMultipleObjects(2, Threads, TRUE, 5000) == WAIT_OBJECT_0);
  CHECK(GReleased == 2);
  for (HANDLE T : Threads)
  {
    CloseHandle(T);
  }
  CloseHandle(GGate);
}

// A manual-reset event releases every waiter.
int WaitsOnManualGate(void *)
{
  return (WaitForSingleObject(GGate, 5000) == WAIT_OBJECT_0) ? 1 : 0;
}

void TestManualResetReleasesAll()
{
  GGate = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  TThreadID Id;
  HANDLE Threads[4];
  for (HANDLE & T : Threads)
  {
    T = reinterpret_cast<HANDLE>(
      BeginThread(nullptr, 0, WaitsOnManualGate, nullptr, 0, Id));
  }
  Sleep(100);
  SetEvent(GGate);
  CHECK(WaitForMultipleObjects(4, Threads, TRUE, 5000) == WAIT_OBJECT_0);
  for (HANDLE T : Threads)
  {
    DWORD Code = 0;
    CHECK(GetExitCodeThread(T, &Code) && (Code == 1));
    CloseHandle(T);
  }
  CloseHandle(GGate);
}

// --- WaitForMultipleObjects -------------------------------------------------

void TestWaitAnyPicksLowestAndConsumesOnlyIt()
{
  HANDLE E[2] = {CreateEvent(nullptr, FALSE, TRUE, nullptr),
                 CreateEvent(nullptr, FALSE, TRUE, nullptr)};
  CHECK(WaitForMultipleObjects(2, E, FALSE, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(E[1], 0) == WAIT_OBJECT_0);  // untouched
  CHECK(WaitForSingleObject(E[0], 0) == WAIT_TIMEOUT);   // consumed
  CloseHandle(E[0]);
  CloseHandle(E[1]);

  HANDLE F[2] = {CreateEvent(nullptr, FALSE, FALSE, nullptr),
                 CreateEvent(nullptr, FALSE, TRUE, nullptr)};
  CHECK(WaitForMultipleObjects(2, F, FALSE, 0) == WAIT_OBJECT_0 + 1);
  CloseHandle(F[0]);
  CloseHandle(F[1]);
}

void TestWaitAllIsAtomic()
{
  HANDLE E[2] = {CreateEvent(nullptr, FALSE, TRUE, nullptr),
                 CreateEvent(nullptr, FALSE, FALSE, nullptr)};
  CHECK(WaitForMultipleObjects(2, E, TRUE, 0) == WAIT_TIMEOUT);
  // The signalled one must not have been consumed by the failed wait.
  CHECK(WaitForSingleObject(E[0], 0) == WAIT_OBJECT_0);

  SetEvent(E[0]);
  SetEvent(E[1]);
  CHECK(WaitForMultipleObjects(2, E, TRUE, 0) == WAIT_OBJECT_0);
  CHECK(WaitForSingleObject(E[0], 0) == WAIT_TIMEOUT);  // both consumed
  CHECK(WaitForSingleObject(E[1], 0) == WAIT_TIMEOUT);
  CloseHandle(E[0]);
  CloseHandle(E[1]);
}

void TestMixedThreadAndEvent()
{
  HANDLE Event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  TThreadID Id;
  HANDLE Thread = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, ReturnsFortyTwo, nullptr, 0, Id));
  HANDLE Both[2] = {Event, Thread};
  CHECK(WaitForMultipleObjects(2, Both, FALSE, 5000) == WAIT_OBJECT_0 + 1);
  CloseHandle(Thread);
  CloseHandle(Event);
}

// --- handle lifetime ----------------------------------------------------------

HANDLE GGo = nullptr;
HANDLE GDone = nullptr;

int OutlivesItsHandle(void *)
{
  WaitForSingleObject(GGo, 5000);
  SetEvent(GDone);
  return 0;
}

void TestClosingThreadHandleDoesNotStopIt()
{
  GGo = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  GDone = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  TThreadID Id;
  HANDLE T = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, OutlivesItsHandle, nullptr, 0, Id));
  CHECK(CloseHandle(T));
  SetEvent(GGo);
  CHECK(WaitForSingleObject(GDone, 5000) == WAIT_OBJECT_0);
  Sleep(50);  // let it leave before its events go
  CloseHandle(GGo);
  CloseHandle(GDone);
}

void TestClosedHandleIsInvalid()
{
  HANDLE E = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  CHECK(CloseHandle(E));
  SetLastError(0);
  CHECK(!CloseHandle(E));
  CHECK(GetLastError() == ERROR_INVALID_HANDLE);
  SetLastError(0);
  CHECK(WaitForSingleObject(E, 0) == WAIT_FAILED);
  CHECK(GetLastError() == ERROR_INVALID_HANDLE);
}

// --- priority -----------------------------------------------------------------

void TestPriority()
{
  TThreadID Id;
  HANDLE T = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, ReturnsFortyTwo, nullptr, CREATE_SUSPENDED, Id));
  // TSignalThread lowers the priority of a suspended thread.
  CHECK(SetThreadPriority(T, THREAD_PRIORITY_BELOW_NORMAL));
  CHECK(GetThreadPriority(T) == THREAD_PRIORITY_BELOW_NORMAL);
  SetLastError(0);
  CHECK(!SetThreadPriority(T, 5));
  CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
  ResumeThread(T);
  CHECK(WaitForSingleObject(T, 5000) == WAIT_OBJECT_0);
  CloseHandle(T);
}

// --- tick count and COM --------------------------------------------------------

void TestTickCountIsMonotonic()
{
  const DWORD A = GetTickCount();
  Sleep(60);
  const DWORD B = GetTickCount();
  // Unsigned subtraction, as core does it, survives the 2^32 wrap.
  CHECK(static_cast<DWORD>(B - A) >= 40);
  CHECK(static_cast<DWORD>(B - A) < 5000);
}

std::atomic<HRESULT> GFirst{-1};
std::atomic<HRESULT> GSecond{-1};

int InitialisesCom(void *)
{
  GFirst = CoInitialize(nullptr);
  GSecond = CoInitialize(nullptr);
  CoUninitialize();
  CoUninitialize();
  return 0;
}

void TestComInitIsPerThread()
{
  // Queue.cpp calls this at the top of each terminal thread.
  const HRESULT Main = CoInitialize(nullptr);
  CHECK((Main == S_OK) || (Main == S_FALSE));

  TThreadID Id;
  HANDLE T = reinterpret_cast<HANDLE>(
    BeginThread(nullptr, 0, InitialisesCom, nullptr, 0, Id));
  CHECK(WaitForSingleObject(T, 5000) == WAIT_OBJECT_0);
  CHECK(GFirst == S_OK);     // a fresh thread, whatever the main thread did
  CHECK(GSecond == S_FALSE);
  CloseHandle(T);
  CoUninitialize();
}

// --- POSIX-only limits ----------------------------------------------------------

void TestPosixLimits()
{
#if !defined(_WIN32)
  // Named events would have to be cross-process; refused rather than faked.
  SetLastError(0);
  CHECK(CreateEvent(nullptr, FALSE, FALSE, L"linscp-test") == nullptr);
  CHECK(GetLastError() == ERROR_NOT_SUPPORTED);
#endif
}

}  // namespace

int main()
{
  TestSuspendedStartProtocol();
  TestThreadIdStable();
  TestExitCodes();
  TestSmallStackIsRoundedUp();
  TestAutoResetConsumes();
  TestManualResetPersists();
  TestTimeoutIsHonoured();
  TestCrossThreadWake();
  TestAutoResetReleasesExactlyOne();
  TestManualResetReleasesAll();
  TestWaitAnyPicksLowestAndConsumesOnlyIt();
  TestWaitAllIsAtomic();
  TestMixedThreadAndEvent();
  TestClosingThreadHandleDoesNotStopIt();
  TestClosedHandleIsInvalid();
  TestPriority();
  TestTickCountIsMonotonic();
  TestComInitIsPerThread();
  TestPosixLimits();

  if (Failures == 0)
  {
    std::printf("all thread tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
