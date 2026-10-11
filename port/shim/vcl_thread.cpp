#include "vcl_thread.h"

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include "win32_on_posix.h"
#endif

namespace {

struct TBeginThreadParam
{
  TThreadFunc ThreadFunc;
  void * Parameter;
};

// Adapts Delphi's int(void *) thread function to Win32's DWORD WINAPI(LPVOID).
DWORD WINAPI BeginThreadEntry(LPVOID Arg)
{
  // Freed before the call, so EndThread leaves nothing behind on either target:
  // on Windows ExitThread skips destructors entirely.
  const TBeginThreadParam Param = *static_cast<TBeginThreadParam *>(Arg);
  delete static_cast<TBeginThreadParam *>(Arg);
  return static_cast<DWORD>(Param.ThreadFunc(Param.Parameter));
}

}  // namespace

NativeUInt BeginThread(void * SecurityAttributes, unsigned StackSize,
  TThreadFunc ThreadFunc, void * Parameter, unsigned CreationFlags,
  TThreadID & ThreadId)
{
  auto * Param = new TBeginThreadParam{ThreadFunc, Parameter};
  DWORD Id = 0;
  const HANDLE Handle = CreateThread(
    static_cast<LPSECURITY_ATTRIBUTES>(SecurityAttributes), StackSize,
    BeginThreadEntry, Param, CreationFlags, &Id);
  if (Handle == nullptr)
  {
    delete Param;
    return 0;
  }
  ThreadId = static_cast<TThreadID>(Id);
  return reinterpret_cast<NativeUInt>(Handle);
}

void EndThread(int ExitCode)
{
  ExitThread(static_cast<DWORD>(ExitCode));
}
