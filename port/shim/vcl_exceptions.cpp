#include <cstring>
#include <new>
#include "vcl_object.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>

#include <cxxabi.h>

#include "vcl_format.h"
#include "vcl_resources.h"

namespace {

// Resolve a resource id to its string, as Delphi's CreateRes forms do.
UnicodeString FromIdent(NativeUInt Ident)
{
  const wchar_t * Text = linscp::FindResourceString(static_cast<int>(Ident));
  return Text != nullptr ? UnicodeString(Text) : UnicodeString();
}

UnicodeString FromResStringRec(PResStringRec Rec)
{
  return Rec == nullptr ? UnicodeString() : FromIdent(Rec->Identifier);
}

// Delphi's CreateFmt forms format the message with the variant arguments.
UnicodeString Formatted(const UnicodeString & Fmt, const TVarRec * Args,
                        int ArgsSize)
{
  if (Args == nullptr || ArgsSize < 0)
  {
    return Fmt;
  }
  // C++Builder passes the *index of the last element*, not the count, which is
  // what ARRAYOFCONST computes. Off by one here would silently drop the final
  // argument from every formatted exception message.
  return Format(Fmt, Args, ArgsSize);
}

}  // namespace

Exception::Exception(const UnicodeString & Msg)
  : FMessage(Msg) {}

Exception::Exception(const UnicodeString & Msg, int AHelpContext)
  : FMessage(Msg), FHelpContext(AHelpContext) {}

Exception::Exception(const UnicodeString & Msg, const TVarRec * Args,
                     int Args_Size)
  : FMessage(Formatted(Msg, Args, Args_Size)) {}

Exception::Exception(const UnicodeString & Msg, const TVarRec * Args,
                     int Args_Size, int AHelpContext)
  : FMessage(Formatted(Msg, Args, Args_Size)), FHelpContext(AHelpContext) {}

Exception::Exception(NativeUInt Ident)
  : FMessage(FromIdent(Ident)) {}

Exception::Exception(NativeUInt Ident, int AHelpContext)
  : FMessage(FromIdent(Ident)), FHelpContext(AHelpContext) {}

Exception::Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size)
  : FMessage(Formatted(FromIdent(Ident), Args, Args_Size)) {}

Exception::Exception(NativeUInt Ident, const TVarRec * Args, int Args_Size,
                     int AHelpContext)
  : FMessage(Formatted(FromIdent(Ident), Args, Args_Size)),
    FHelpContext(AHelpContext) {}

Exception::Exception(PResStringRec ResStringRec)
  : FMessage(FromResStringRec(ResStringRec)) {}

Exception::Exception(PResStringRec ResStringRec, int AHelpContext)
  : FMessage(FromResStringRec(ResStringRec)), FHelpContext(AHelpContext) {}

Exception::Exception(PResStringRec ResStringRec, const TVarRec * Args,
                     int Args_Size)
  : FMessage(Formatted(FromResStringRec(ResStringRec), Args, Args_Size)) {}

Exception::Exception(PResStringRec ResStringRec, const TVarRec * Args,
                     int Args_Size, int AHelpContext)
  : FMessage(Formatted(FromResStringRec(ResStringRec), Args, Args_Size)),
    FHelpContext(AHelpContext) {}

namespace {

std::string DemangledName(const std::type_info & Type)
{
  int Status = 0;
  std::unique_ptr<char, void (*)(void *)> Name(
    abi::__cxa_demangle(Type.name(), nullptr, nullptr, &Status), std::free);
  return (Status == 0) && Name ? std::string(Name.get()) : std::string(Type.name());
}

}  // namespace

UnicodeString TObject::ClassName() const
{
  const std::string Full = DemangledName(typeid(*this));
  const std::size_t Colon = Full.rfind("::");
  return UnicodeString((Colon == std::string::npos) ? Full.c_str()
                                                    : Full.c_str() + Colon + 2);
}

UnicodeString TObject::QualifiedClassName() const
{
  std::string Full = DemangledName(typeid(*this));
  for (std::size_t I = Full.find("::"); I != std::string::npos; I = Full.find("::", I))
  {
    Full.replace(I, 2, ".");
  }
  return UnicodeString(Full.c_str());
}

//---------------------------------------------------------------------------
// Delphi zeroes an object's memory before any constructor runs
// (TObject.InitInstance), so a field no constructor sets is 0, false or
// null, and upstream relies on it: TSessionData's constructor reaches
// Modify(), which writes through FUnsafeSettings, before anything sets it.
// C++Builder gives every Delphi-style class that guarantee. A class-scoped
// operator new on TObject cannot provide it -- core derives privately from
// TStringList (TRemoteDirectoryCache), which makes the base's operator
// inaccessible -- so every allocation is zeroed. That is a superset no
// correct code can tell apart; the cost is a calloc per allocation.
//
// In this unit because it is always linked: it defines Exception.
//---------------------------------------------------------------------------
namespace {

void * ZeroedAllocate(std::size_t Size)
{
  if (Size == 0)
  {
    Size = 1;
  }
  for (;;)
  {
    void * Result = std::calloc(1, Size);
    if (Result != nullptr)
    {
      return Result;
    }
    std::new_handler Handler = std::get_new_handler();
    if (Handler == nullptr)
    {
      throw std::bad_alloc();
    }
    Handler();
  }
}

void * ZeroedAllocateNoThrow(std::size_t Size) noexcept
{
  try
  {
    return ZeroedAllocate(Size);
  }
  catch (...)
  {
    return nullptr;
  }
}

}  // namespace

void * operator new(std::size_t Size) { return ZeroedAllocate(Size); }
void * operator new[](std::size_t Size) { return ZeroedAllocate(Size); }
void * operator new(std::size_t Size, const std::nothrow_t &) noexcept { return ZeroedAllocateNoThrow(Size); }
void * operator new[](std::size_t Size, const std::nothrow_t &) noexcept { return ZeroedAllocateNoThrow(Size); }
void operator delete(void * Pointer) noexcept { std::free(Pointer); }
void operator delete[](void * Pointer) noexcept { std::free(Pointer); }
void operator delete(void * Pointer, std::size_t) noexcept { std::free(Pointer); }
void operator delete[](void * Pointer, std::size_t) noexcept { std::free(Pointer); }
void operator delete(void * Pointer, const std::nothrow_t &) noexcept { std::free(Pointer); }
void operator delete[](void * Pointer, const std::nothrow_t &) noexcept { std::free(Pointer); }

// A thrown object is built in the runtime's exception storage, not by
// operator new, and libstdc++ zeroes only its own header there. Core throws
// Delphi-style exceptions by value (`throw ESshFatal(...)`), constructed in
// place in that storage, and ExtException's constructor reads FMoreMessages
// before setting it. The executables link with
// --wrap=__cxa_allocate_exception (linscp-shim's link options), which routes
// every allocation here to be zeroed as Delphi's would be.
extern "C" void * __real___cxa_allocate_exception(std::size_t Size) noexcept;
extern "C" void * __wrap___cxa_allocate_exception(std::size_t Size) noexcept
{
  void * Result = __real___cxa_allocate_exception(Size);
  std::memset(Result, 0, Size);
  return Result;
}
