// Win32 threads, events and waits over pthreads.
//
// Every kernel object lives in one handle table guarded by one mutex, with one
// condition variable announcing any change of signal state. That is coarse, but
// it makes WaitForMultipleObjects over a mix of threads and events correct by
// construction -- including WaitAll, which must acquire every object at once --
// and core has a handful of threads, not hundreds.
#include "win32_on_posix.h"

#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct TKernelObject
{
  virtual ~TKernelObject() = default;
  // Both are called with GKernelMutex held.
  virtual bool IsSignaled() const = 0;
  // A satisfied wait consumes the object where Win32 says it does.
  virtual void Acquire() {}
};

struct TEvent final : TKernelObject
{
  bool ManualReset = false;
  bool Signaled = false;

  bool IsSignaled() const override { return Signaled; }
  void Acquire() override
  {
    if (!ManualReset)
    {
      Signaled = false;
    }
  }
};

struct TThread final : TKernelObject
{
  LPTHREAD_START_ROUTINE Start = nullptr;
  LPVOID Parameter = nullptr;
  DWORD Id = 0;
  DWORD SuspendCount = 0;
  bool Finished = false;
  DWORD ExitCode = STILL_ACTIVE;
  int Priority = THREAD_PRIORITY_NORMAL;

  // A thread handle is signalled once, permanently, when the thread ends.
  bool IsSignaled() const override { return Finished; }
};

// Thrown by ExitThread and caught at the thread's entry point.
struct TThreadExit
{
  DWORD ExitCode;
};

std::mutex GKernelMutex;
std::condition_variable GKernelChanged;
std::unordered_map<std::uintptr_t, std::shared_ptr<TKernelObject>> GHandles;
// Multiples of four, like Win32, and never reused.
std::uintptr_t GNextHandle = 0x100;

// One counter for both CreateThread and lazily-named foreign threads (the main
// thread included), so ids are unique across the two.
std::atomic<DWORD> GNextThreadId{1};
thread_local DWORD GThreadId = 0;

HANDLE Register(std::shared_ptr<TKernelObject> Object)
{
  std::lock_guard<std::mutex> Lock(GKernelMutex);
  const std::uintptr_t Value = GNextHandle;
  GNextHandle += 4;
  GHandles.emplace(Value, std::move(Object));
  return reinterpret_cast<HANDLE>(Value);
}

// Requires GKernelMutex. Null when the handle is unknown or of another type.
template<typename T>
std::shared_ptr<T> Find(HANDLE Handle)
{
  const auto It = GHandles.find(reinterpret_cast<std::uintptr_t>(Handle));
  if (It == GHandles.end())
  {
    return nullptr;
  }
  return std::dynamic_pointer_cast<T>(It->second);
}

void * ThreadEntry(void * Arg) noexcept
{
  std::unique_ptr<std::shared_ptr<TThread>> Owner(
    static_cast<std::shared_ptr<TThread> *>(Arg));
  const std::shared_ptr<TThread> Self = *Owner;
  Owner.reset();

  GThreadId = Self->Id;

  // CREATE_SUSPENDED: pthreads cannot be created stopped, so the thread runs
  // up to here and parks until ResumeThread brings the count to zero.
  {
    std::unique_lock<std::mutex> Lock(GKernelMutex);
    GKernelChanged.wait(Lock, [&] { return Self->SuspendCount == 0; });
  }

  DWORD ExitCode = 0;
  try
  {
    ExitCode = Self->Start(Self->Parameter);
  }
  catch (const TThreadExit & E)
  {
    ExitCode = E.ExitCode;
  }
  catch (...)
  {
    // An exception escaping a thread ends the process on Windows too.
    std::terminate();
  }

  {
    std::lock_guard<std::mutex> Lock(GKernelMutex);
    Self->ExitCode = ExitCode;
    Self->Finished = true;
  }
  GKernelChanged.notify_all();
  return nullptr;
}

bool IsValidPriority(int Priority)
{
  switch (Priority)
  {
    case THREAD_PRIORITY_IDLE:
    case THREAD_PRIORITY_LOWEST:
    case THREAD_PRIORITY_BELOW_NORMAL:
    case THREAD_PRIORITY_NORMAL:
    case THREAD_PRIORITY_ABOVE_NORMAL:
    case THREAD_PRIORITY_HIGHEST:
    case THREAD_PRIORITY_TIME_CRITICAL:
      return true;
    default:
      return false;
  }
}

}  // namespace

HANDLE CreateEvent(LPSECURITY_ATTRIBUTES, BOOL ManualReset, BOOL InitialState,
  LPCWSTR Name)
{
  if ((Name != nullptr) && (Name[0] != L'\0'))
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return nullptr;
  }
  auto Event = std::make_shared<TEvent>();
  Event->ManualReset = (ManualReset != FALSE);
  Event->Signaled = (InitialState != FALSE);
  return Register(std::move(Event));
}

BOOL SetEvent(HANDLE Handle)
{
  {
    std::lock_guard<std::mutex> Lock(GKernelMutex);
    const auto Event = Find<TEvent>(Handle);
    if (!Event)
    {
      SetLastError(ERROR_INVALID_HANDLE);
      return FALSE;
    }
    Event->Signaled = true;
  }
  GKernelChanged.notify_all();
  return TRUE;
}

BOOL ResetEvent(HANDLE Handle)
{
  std::lock_guard<std::mutex> Lock(GKernelMutex);
  const auto Event = Find<TEvent>(Handle);
  if (!Event)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  Event->Signaled = false;
  return TRUE;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T StackSize,
  LPTHREAD_START_ROUTINE StartAddress, LPVOID Parameter, DWORD CreationFlags,
  LPDWORD ThreadId)
{
  if (StartAddress == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return nullptr;
  }

  auto Thread = std::make_shared<TThread>();
  Thread->Start = StartAddress;
  Thread->Parameter = Parameter;
  Thread->Id = GNextThreadId++;
  Thread->SuspendCount = ((CreationFlags & CREATE_SUSPENDED) != 0) ? 1 : 0;
  const HANDLE Handle = Register(Thread);

  pthread_attr_t Attributes;
  pthread_attr_init(&Attributes);
  pthread_attr_setdetachstate(&Attributes, PTHREAD_CREATE_DETACHED);
  if (StackSize != 0)
  {
    // pthreads rejects sizes below its minimum or off a page boundary, where
    // Win32 silently rounds up; round up the same way.
    constexpr SIZE_T Granularity = 64 * 1024;
    SIZE_T Size = std::max<SIZE_T>(StackSize, PTHREAD_STACK_MIN);
    Size = (Size + Granularity - 1) / Granularity * Granularity;
    pthread_attr_setstacksize(&Attributes, Size);
  }

  auto * Arg = new std::shared_ptr<TThread>(Thread);
  pthread_t Native;
  const int Error = pthread_create(&Native, &Attributes, ThreadEntry, Arg);
  pthread_attr_destroy(&Attributes);
  if (Error != 0)
  {
    delete Arg;
    {
      std::lock_guard<std::mutex> Lock(GKernelMutex);
      GHandles.erase(reinterpret_cast<std::uintptr_t>(Handle));
    }
    SetLastError(Win32ErrorFromErrno(Error));
    return nullptr;
  }

  if (ThreadId != nullptr)
  {
    *ThreadId = Thread->Id;
  }
  return Handle;
}

DWORD ResumeThread(HANDLE Handle)
{
  DWORD Previous;
  {
    std::lock_guard<std::mutex> Lock(GKernelMutex);
    const auto Thread = Find<TThread>(Handle);
    if (!Thread)
    {
      SetLastError(ERROR_INVALID_HANDLE);
      return static_cast<DWORD>(-1);
    }
    Previous = Thread->SuspendCount;
    if (Previous > 0)
    {
      --Thread->SuspendCount;
    }
  }
  if (Previous == 1)
  {
    GKernelChanged.notify_all();
  }
  return Previous;
}

void ExitThread(DWORD ExitCode)
{
  throw TThreadExit{ExitCode};
}

BOOL GetExitCodeThread(HANDLE Handle, LPDWORD ExitCode)
{
  if (ExitCode == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  std::lock_guard<std::mutex> Lock(GKernelMutex);
  const auto Thread = Find<TThread>(Handle);
  if (!Thread)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  *ExitCode = Thread->ExitCode;
  return TRUE;
}

BOOL SetThreadPriority(HANDLE Handle, int Priority)
{
  if (!IsValidPriority(Priority))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  std::lock_guard<std::mutex> Lock(GKernelMutex);
  const auto Thread = Find<TThread>(Handle);
  if (!Thread)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  Thread->Priority = Priority;
  return TRUE;
}

int GetThreadPriority(HANDLE Handle)
{
  std::lock_guard<std::mutex> Lock(GKernelMutex);
  const auto Thread = Find<TThread>(Handle);
  if (!Thread)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return THREAD_PRIORITY_ERROR_RETURN;
  }
  return Thread->Priority;
}

DWORD GetCurrentThreadId()
{
  if (GThreadId == 0)
  {
    GThreadId = GNextThreadId++;
  }
  return GThreadId;
}

DWORD GetCurrentProcessId()
{
  return static_cast<DWORD>(getpid());
}

DWORD WaitForMultipleObjects(DWORD Count, const HANDLE * Handles, BOOL WaitAll,
  DWORD Milliseconds)
{
  if ((Count == 0) || (Count > MAXIMUM_WAIT_OBJECTS) || (Handles == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return WAIT_FAILED;
  }
  if (WaitAll)
  {
    // Win32 rejects duplicates here: one object cannot be acquired twice.
    std::vector<HANDLE> Sorted(Handles, Handles + Count);
    std::sort(Sorted.begin(), Sorted.end());
    if (std::adjacent_find(Sorted.begin(), Sorted.end()) != Sorted.end())
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return WAIT_FAILED;
    }
  }

  const auto Deadline =
    std::chrono::steady_clock::now() + std::chrono::milliseconds(Milliseconds);

  std::unique_lock<std::mutex> Lock(GKernelMutex);
  // Hold references, so a CloseHandle on another thread mid-wait cannot free
  // an object we are still examining.
  std::vector<std::shared_ptr<TKernelObject>> Objects(Count);
  for (DWORD Index = 0; Index < Count; ++Index)
  {
    Objects[Index] = Find<TKernelObject>(Handles[Index]);
    if (!Objects[Index])
    {
      SetLastError(ERROR_INVALID_HANDLE);
      return WAIT_FAILED;
    }
  }

  for (;;)
  {
    if (WaitAll)
    {
      const bool All = std::all_of(Objects.begin(), Objects.end(),
        [](const std::shared_ptr<TKernelObject> & O) { return O->IsSignaled(); });
      if (All)
      {
        for (const auto & Object : Objects)
        {
          Object->Acquire();
        }
        return WAIT_OBJECT_0;
      }
    }
    else
    {
      // The lowest signalled index wins, as on Win32.
      for (DWORD Index = 0; Index < Count; ++Index)
      {
        if (Objects[Index]->IsSignaled())
        {
          Objects[Index]->Acquire();
          return WAIT_OBJECT_0 + Index;
        }
      }
    }

    if (Milliseconds == INFINITE)
    {
      GKernelChanged.wait(Lock);
    }
    else if ((Milliseconds == 0) || (std::chrono::steady_clock::now() >= Deadline))
    {
      return WAIT_TIMEOUT;
    }
    else
    {
      GKernelChanged.wait_until(Lock, Deadline);
    }
  }
}

DWORD WaitForSingleObject(HANDLE Handle, DWORD Milliseconds)
{
  return WaitForMultipleObjects(1, &Handle, FALSE, Milliseconds);
}

BOOL CloseHandle(HANDLE Object)
{
  std::shared_ptr<TKernelObject> Released;
  {
    std::lock_guard<std::mutex> Lock(GKernelMutex);
    const auto It = GHandles.find(reinterpret_cast<std::uintptr_t>(Object));
    if (It == GHandles.end())
    {
      SetLastError(ERROR_INVALID_HANDLE);
      return FALSE;
    }
    // Closing a thread's handle does not stop the thread: it keeps its own
    // reference until it ends.
    Released = std::move(It->second);
    GHandles.erase(It);
  }
  return TRUE;
}

DWORD GetTickCount()
{
  return static_cast<DWORD>(GetTickCount64());
}

ULONGLONG GetTickCount64()
{
  return static_cast<ULONGLONG>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

namespace {
thread_local int GComInitCount = 0;
}

HRESULT CoInitialize(LPVOID)
{
  return (GComInitCount++ == 0) ? S_OK : S_FALSE;
}

void CoUninitialize()
{
  if (GComInitCount > 0)
  {
    --GComInitCount;
  }
}

void Sleep(DWORD Milliseconds)
{
  if (Milliseconds == INFINITE)
  {
    for (;;)
    {
      std::this_thread::sleep_for(std::chrono::hours(24));
    }
  }
  if (Milliseconds == 0)
  {
    std::this_thread::yield();
    return;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));
}
