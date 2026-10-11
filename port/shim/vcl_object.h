//---------------------------------------------------------------------------
// TObject, the Delphi exception hierarchy, and assorted small RTL types.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_OBJECT_H
#define LINSCP_VCL_OBJECT_H

#include "vcl_property.h"
#include "vcl_unicodestring.h"

// Embarcadero package/export decorations: meaningless outside C++Builder.
#ifndef __BORLANDC__
  #define DELPHI_PACKAGE
  #define PACKAGE
  #define __declspec_delphirtti
#endif

// Root of the Delphi object hierarchy. Core deletes through TObject* (see
// SAFE_DESTROY in Common.h), so the destructor must be virtual.
class TObject
{
public:
  TObject() = default;
  virtual ~TObject() = default;
  TObject(const TObject &) = delete;
  TObject & operator=(const TObject &) = delete;
};

class TPersistent : public TObject
{
public:
  virtual void Assign(TPersistent * Source) { (void)Source; }
};

// Borland's method pointer: a code address plus the instance it is bound to.
struct TMethod
{
  void * Code = nullptr;
  void * Data = nullptr;
};

//---------------------------------------------------------------------------
// Delphi's exception hierarchy. Core catches these by type (see Exceptions.h),
// so the shapes and inheritance must match or handlers silently stop matching.
//---------------------------------------------------------------------------
class Exception
{
public:
  explicit Exception(const UnicodeString & Msg) : FMessage(Msg) {}
  virtual ~Exception() = default;

  PROP_RW_FF(UnicodeString, Message, FMessage);

  // Delphi exposes Message as a property; core also reads it directly.
  const UnicodeString & GetMessage() const { return FMessage; }

protected:
  UnicodeString FMessage;
};

#define LINSCP_DERIVED_EXCEPTION(NAME, BASE)                                  \
  class NAME : public BASE                                                    \
  {                                                                           \
  public:                                                                     \
    explicit NAME(const UnicodeString & Msg) : BASE(Msg) {}                   \
  }

LINSCP_DERIVED_EXCEPTION(EAbort, Exception);
LINSCP_DERIVED_EXCEPTION(EOutOfMemory, Exception);
LINSCP_DERIVED_EXCEPTION(EInOutError, Exception);
LINSCP_DERIVED_EXCEPTION(EExternal, Exception);
LINSCP_DERIVED_EXCEPTION(EIntError, Exception);
LINSCP_DERIVED_EXCEPTION(EMathError, Exception);
LINSCP_DERIVED_EXCEPTION(EConvertError, Exception);
LINSCP_DERIVED_EXCEPTION(EListError, Exception);
LINSCP_DERIVED_EXCEPTION(EStringListError, EListError);
LINSCP_DERIVED_EXCEPTION(EFilerError, Exception);
LINSCP_DERIVED_EXCEPTION(EReadError, EFilerError);
LINSCP_DERIVED_EXCEPTION(EWriteError, EFilerError);
LINSCP_DERIVED_EXCEPTION(EClassNotFound, EFilerError);
LINSCP_DERIVED_EXCEPTION(EVariantError, Exception);
LINSCP_DERIVED_EXCEPTION(EEncodingError, Exception);
LINSCP_DERIVED_EXCEPTION(EDirectoryNotFoundException, Exception);
LINSCP_DERIVED_EXCEPTION(EOSError, Exception);

// Module registration record. Core walks this only to resolve resource
// strings; an empty list is the correct behaviour until P1-01b lands LoadStr.
struct TLibModule
{
  void * Instance = nullptr;
  TLibModule * Next = nullptr;
};

#endif // LINSCP_VCL_OBJECT_H
