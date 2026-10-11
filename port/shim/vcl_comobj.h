// Delphi's System.Win.ComObj (OleCheck, EOleError, EOleSysError) and
// System.hpp's TCppInterfacedObject, as core uses them.
#ifndef LINSCP_VCL_COMOBJ_H
#define LINSCP_VCL_COMOBJ_H

#include "vcl_object.h"
#include "vcl_unicodestring.h"

#include <atomic>

LINSCP_DERIVED_EXCEPTION(EOleError, Exception);

// Raised for a failed HRESULT; the message is the system's for that code.
class EOleSysError : public EOleError
{
public:
  EOleSysError(const UnicodeString & Message, HRESULT ErrorCode, int HelpContext);
  HRESULT ErrorCode;
};

namespace System { namespace Win { namespace Comobj {
// Core's Common.h redeclares this itself, to avoid ComObj.hpp; the two
// declarations agree.
extern DELPHI_PACKAGE void __fastcall OleCheck(HRESULT Result);
}}}
using System::Win::Comobj::OleCheck;

#if defined(_WIN32)
// A C++ class implementing a COM interface, with TInterfacedObject's
// reference counting: zero once constructed, deleted by its last Release.
// QueryInterface answers for IUnknown and the implemented interface.
template <typename INTF>
class TCppInterfacedObject : public INTF
{
public:
  virtual ~TCppInterfacedObject() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID IID, void ** Object) override
  {
    if (Object == nullptr)
    {
      return E_POINTER;
    }
    if (IsEqualIID(IID, __uuidof(IUnknown)) || IsEqualIID(IID, __uuidof(INTF)))
    {
      *Object = static_cast<INTF *>(this);
      AddRef();
      return S_OK;
    }
    *Object = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override
  {
    return ++FRefCount;
  }

  ULONG STDMETHODCALLTYPE Release() override
  {
    const ULONG Result = --FRefCount;
    if (Result == 0)
    {
      delete this;
    }
    return Result;
  }

private:
  std::atomic<ULONG> FRefCount{0};
};
#endif

#endif // LINSCP_VCL_COMOBJ_H
