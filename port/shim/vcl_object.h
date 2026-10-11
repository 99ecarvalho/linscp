//---------------------------------------------------------------------------
// TObject, the Delphi exception hierarchy, and assorted small RTL types.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_OBJECT_H
#define LINSCP_VCL_OBJECT_H

#include "vcl_property.h"
#include "vcl_types.h"
#include "vcl_unicodestring.h"

// Declared later; the exception constructors only take them by pointer.
class TVarRec;

// Embarcadero package/export decorations: meaningless outside C++Builder.
#ifndef __BORLANDC__
  #define DELPHI_PACKAGE
  #define PACKAGE
  #define __declspec_delphirtti
#endif

class TObject;

// Borland's metaclass, as far as core uses it: E->InheritsFrom(__classid(T)).
// One instance per type; TClass is a pointer to it, as in Delphi.
class TMetaClass
{
public:
  virtual ~TMetaClass() = default;
  virtual bool IsInstance(const TObject * Object) const = 0;
};
using TClass = const TMetaClass *;

// Root of the Delphi object hierarchy. Core deletes through TObject* (see
// SAFE_DESTROY in Common.h), so the destructor must be virtual.
class TObject
{
public:
  TObject() = default;
  virtual ~TObject() = default;
  TObject(const TObject &) = delete;
  TObject & operator=(const TObject &) = delete;

  // Delphi's RTTI check. Implemented with dynamic_cast rather than a class
  // registry, which covers what core uses it for: asking whether an object is
  // of a given type before downcasting.
  template <typename T>
  bool InheritsFrom() const { return dynamic_cast<const T *>(this) != nullptr; }

  // Delphi's class-level check: this object's class is Class or derives from
  // it. A null class matches nothing.
  bool InheritsFrom(TClass Class) const
    { return (Class != nullptr) && Class->IsInstance(this); }

  // The dynamic type's name, from RTTI: unqualified, as Delphi's ClassName
  // returns it; QualifiedClassName keeps the namespace path, written with '.'
  // the way Delphi writes unit scopes.
  UnicodeString ClassName() const;
  UnicodeString QualifiedClassName() const;
};

namespace linscp {

template <typename T>
class TMetaClassOf final : public TMetaClass
{
public:
  bool IsInstance(const TObject * Object) const override
    { return dynamic_cast<const T *>(Object) != nullptr; }
  static const TMetaClassOf Instance;
};

template <typename T>
const TMetaClassOf<T> TMetaClassOf<T>::Instance{};

}  // namespace linscp

// Borland's __classid keyword.
#ifndef __BORLANDC__
  #define __classid(T) (static_cast<TClass>(&::linscp::TMetaClassOf<T>::Instance))
#endif

// Core's own declarations, and the closure typedefs the transform produces,
// spell some of these with their Delphi unit scope.
namespace System {
using TObject = ::TObject;
using Boolean = bool;
}  // namespace System

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
class Exception : public TObject
{
public:
  // C++Builder's full constructor set. Core's own Exceptions.h declares
  // descendants that delegate to every one of these, so all must exist even
  // though most call sites use only the first: 77 of them are
  // Exception(FMTLOAD(...)), where FMTLOAD has already produced a string.
  //
  // The Ident and ResStringRec forms resolve a resource string, which is why
  // these live in a .cpp rather than inline -- the resource table and TVarRec
  // are declared after this header.
  explicit Exception(const UnicodeString & Msg);
  Exception(const UnicodeString & Msg, int AHelpContext);
  Exception(const UnicodeString & Msg, const TVarRec * Args, int Args_Size);
  Exception(const UnicodeString & Msg, const TVarRec * Args, int Args_Size,
            int AHelpContext);
  explicit Exception(NativeUInt Ident);
  Exception(NativeUInt Ident, int AHelpContext);
  Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size);
  Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size,
            int AHelpContext);
  explicit Exception(PResStringRec ResStringRec);
  Exception(PResStringRec ResStringRec, int AHelpContext);
  Exception(PResStringRec ResStringRec, const TVarRec * Args, int Args_Size);
  Exception(PResStringRec ResStringRec, const TVarRec * Args, int Args_Size,
            int AHelpContext);

  virtual ~Exception() = default;

  // TObject is deliberately non-copyable, but C++ requires a thrown type to be
  // copy-constructible even when the copy is elided, and core throws by value.
  // So Exception -- and only Exception -- re-enables copying, starting a fresh
  // TObject base rather than copying one. Copies every member below; a test
  // pins that, so a member added later cannot be silently dropped on rethrow.
  Exception(const Exception & Other)
    : TObject(), FMessage(Other.FMessage), FHelpContext(Other.FHelpContext)
  {
  }
  Exception & operator=(const Exception & Other)
  {
    FMessage = Other.FMessage;
    FHelpContext = Other.FHelpContext;
    return *this;
  }

  PROP_RW_FF(UnicodeString, Message, FMessage);
  PROP_RW_FF(int, HelpContext, FHelpContext);

  // Core also reads the message directly.
  const UnicodeString & GetMessage() const { return FMessage; }

protected:
  UnicodeString FMessage;
  int FHelpContext = 0;
};

// Inherits every constructor, so a descendant can be thrown with any of the
// forms the base accepts.
#define LINSCP_DERIVED_EXCEPTION(NAME, BASE)                                  \
  class NAME : public BASE                                                    \
  {                                                                           \
  public:                                                                     \
    using BASE::BASE;                                                         \
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
// Delphi's stream exception hierarchy, which core catches at EStreamError in
// places: EFilerError, EFOpenError and EFCreateError are all siblings under it,
// and EReadError/EWriteError sit under EFilerError. Flattening any of this would
// stop existing handlers matching, with no compile error to say so.
LINSCP_DERIVED_EXCEPTION(EStreamError, Exception);
LINSCP_DERIVED_EXCEPTION(EFCreateError, EStreamError);
LINSCP_DERIVED_EXCEPTION(EFOpenError, EStreamError);
LINSCP_DERIVED_EXCEPTION(EFilerError, EStreamError);
LINSCP_DERIVED_EXCEPTION(EReadError, EFilerError);
LINSCP_DERIVED_EXCEPTION(EWriteError, EFilerError);
LINSCP_DERIVED_EXCEPTION(EClassNotFound, EFilerError);
LINSCP_DERIVED_EXCEPTION(EVariantError, Exception);
LINSCP_DERIVED_EXCEPTION(EEncodingError, Exception);
LINSCP_DERIVED_EXCEPTION(EDirectoryNotFoundException, Exception);
LINSCP_DERIVED_EXCEPTION(EOSError, Exception);
LINSCP_DERIVED_EXCEPTION(EInvalidOperation, Exception);
LINSCP_DERIVED_EXCEPTION(EVariantBoundsError, EVariantError);
LINSCP_DERIVED_EXCEPTION(EFileNotFoundException, Exception);

#endif // LINSCP_VCL_OBJECT_H
