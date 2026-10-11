// The kernel-object table behind HANDLE, for the platform layer's own units:
// an object registered here gets a handle CloseHandle releases and the wait
// functions understand. Not part of the Win32 surface.
#ifndef LINSCP_WIN32_KERNEL_INTERNAL_H
#define LINSCP_WIN32_KERNEL_INTERNAL_H

#include "win32_on_posix.h"

#include <functional>
#include <memory>

namespace linscp {

struct TKernelObject
{
  virtual ~TKernelObject() = default;
  // Both are called with the table's lock held.
  virtual bool IsSignaled() const = 0;
  // A satisfied wait consumes the object where Win32 says it does.
  virtual void Acquire() {}
};

HANDLE RegisterKernelObject(std::shared_ptr<TKernelObject> Object);
// Null when the handle is unknown.
std::shared_ptr<TKernelObject> FindKernelObject(HANDLE Handle);
// Runs Change with the table's lock held, then wakes every wait so it
// re-examines its objects: for state changed from outside a Win32 call (a
// child process exiting).
void ChangeKernelObject(const std::function<void()> & Change);

}  // namespace linscp

#endif
