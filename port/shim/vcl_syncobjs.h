//---------------------------------------------------------------------------
// System.SyncObjs equivalents.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_SYNCOBJS_H
#define LINSCP_VCL_SYNCOBJS_H

#include <mutex>

#include "vcl_property.h"
#include "vcl_object.h"

// Delphi's TCriticalSection is recursive: the same thread may acquire it more
// than once. std::mutex is not, so this must be recursive_mutex or core
// deadlocks on re-entrant paths.
class TCriticalSection : public TObject
{
public:
  void Acquire() { FMutex.lock(); ++FAcquired; }
  void Release() { --FAcquired; FMutex.unlock(); }
  void Enter() { Acquire(); }
  void Leave() { Release(); }
  bool TryEnter()
  {
    if (FMutex.try_lock())
    {
      ++FAcquired;
      return true;
    }
    return false;
  }

  int GetAcquired() const { return FAcquired; }
  PROP_R_G(int, Acquired, GetAcquired);

private:
  std::recursive_mutex FMutex;
  int FAcquired = 0;
};

#endif // LINSCP_VCL_SYNCOBJS_H
