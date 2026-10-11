// Delphi's System.BeginThread and EndThread.
//
// Upstream's frontend implements core's StartThread (Interface.h) over these,
// so providing them lets that code port unchanged. One implementation serves
// both targets, written over CreateThread: on Windows that is the real API, on
// POSIX the kernel-object layer in port/platform.
//
// VCL's TThread class is deliberately not provided. Its only user, KeyGen.cpp,
// is listed in none of upstream's .cbproj files and TKeyGenerator is referenced
// nowhere else -- it is dead code upstream.
#ifndef LINSCP_VCL_THREAD_H
#define LINSCP_VCL_THREAD_H

#include "vcl_types.h"

// Returns the thread handle, or 0 on failure with GetLastError set. ThreadId is
// written only on success, and equals GetCurrentThreadId() inside the thread.
NativeUInt BeginThread(void * SecurityAttributes, unsigned StackSize,
  TThreadFunc ThreadFunc, void * Parameter, unsigned CreationFlags,
  TThreadID & ThreadId);

[[noreturn]] void EndThread(int ExitCode);

#endif // LINSCP_VCL_THREAD_H
