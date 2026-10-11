// TObject, the exception hierarchy, and TCriticalSection.
//
// The exception tests check that handlers *match by base type*: core catches
// EFilerError and expects EReadError to be caught by it. If the hierarchy were
// flattened, those handlers would stop matching and errors would escape as
// unhandled rather than fail to compile.
#include "vcl_object.h"
#include "vcl_syncobjs.h"

#include <cstdio>
#include <thread>
#include <type_traits>

// At file scope, so its qualified name is exactly Outer.TInner.
namespace Outer {
class TInner : public TObject {};
}

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

void TestExceptionMessage()
{
  Exception E(UnicodeString(L"boom"));
  CHECK(E.Message == UnicodeString(L"boom"));
  CHECK(E.GetMessage() == UnicodeString(L"boom"));

  E.Message = UnicodeString(L"changed");       // Message is a writable property
  CHECK(E.Message == UnicodeString(L"changed"));
}

// Delphi: Exception = class(TObject). Core passes exceptions where a TObject
// is expected, and downcasts them back.
void TestExceptionIsATObject()
{
  TObject * O = new EAbort(UnicodeString(L"abort"));
  CHECK(dynamic_cast<Exception *>(O) != nullptr);
  CHECK(O->InheritsFrom<Exception>());
  CHECK(!O->InheritsFrom<TPersistent>());
  delete O;  // through the root, as SAFE_DESTROY does
}

// Only Exception is copyable -- C++ requires it of thrown types. Everything
// else keeps TObject's protection against slicing.
static_assert(!std::is_copy_constructible<TObject>::value, "TObject must not copy");
static_assert(!std::is_copy_constructible<TPersistent>::value, "nor its descendants");
static_assert(std::is_copy_constructible<Exception>::value, "Exception must copy");
static_assert(std::is_copy_constructible<EAbort>::value, "and so its descendants");

void TestExceptionCopyKeepsEveryMember()
{
  const Exception A(UnicodeString(L"msg"), 7);
  const Exception B(A);
  CHECK(B.Message == UnicodeString(L"msg"));
  CHECK(B.HelpContext == 7);

  Exception C(UnicodeString(L"other"));
  C = A;
  CHECK(C.Message == UnicodeString(L"msg"));
  CHECK(C.HelpContext == 7);

  // An lvalue throw copies; nothing may be lost on the way.
  try { throw A; }
  catch (const Exception & E)
  {
    CHECK(E.Message == UnicodeString(L"msg"));
    CHECK(E.HelpContext == 7);
  }
}

void TestExceptionHierarchy()
{
  // Each derived type must be caught by its documented base.
  try { throw EReadError(UnicodeString(L"r")); }
  catch (const EFilerError & E) { CHECK(E.GetMessage() == UnicodeString(L"r")); }
  catch (...) { CHECK(false); }

  try { throw EWriteError(UnicodeString(L"w")); }
  catch (const EFilerError &) { }
  catch (...) { CHECK(false); }

  try { throw EStringListError(UnicodeString(L"s")); }
  catch (const EListError &) { }
  catch (...) { CHECK(false); }

  // ... and by Exception at the root.
  try { throw EOSError(UnicodeString(L"os")); }
  catch (const Exception & E) { CHECK(E.GetMessage() == UnicodeString(L"os")); }
  catch (...) { CHECK(false); }

  // The stream family: EReadError and EWriteError are reachable as
  // EFilerError and, one level up, as EStreamError. Core catches at both
  // levels, so both must match.
  try { throw EReadError(UnicodeString(L"r")); }
  catch (const EStreamError &) { }
  catch (...) { CHECK(false); }

  try { throw EFOpenError(UnicodeString(L"o")); }
  catch (const EStreamError &) { }
  catch (...) { CHECK(false); }

  // ... but EFOpenError is a sibling of EFilerError, not a child.
  bool AsFiler = false;
  try { throw EFOpenError(UnicodeString(L"o")); }
  catch (const EFilerError &) { AsFiler = true; }
  catch (const EStreamError &) { }
  CHECK(!AsFiler);

  // Siblings must NOT catch each other.
  bool WrongHandler = false;
  try { throw EListError(UnicodeString(L"l")); }
  catch (const EFilerError &) { WrongHandler = true; }
  catch (const Exception &) { }
  CHECK(!WrongHandler);

  // A base must not be caught as a derived type.
  WrongHandler = false;
  try { throw EFilerError(UnicodeString(L"f")); }
  catch (const EReadError &) { WrongHandler = true; }
  catch (const Exception &) { }
  CHECK(!WrongHandler);
}

void TestObjectIsPolymorphic()
{
  // Core deletes through TObject* via SAFE_DESTROY, so the derived destructor
  // must run.
  static int Destroyed = 0;
  struct TDerived : public TObject
  {
    ~TDerived() override { ++Destroyed; }
  };
  TObject * O = new TDerived();
  delete O;
  CHECK(Destroyed == 1);

  TPersistent P;
  CHECK(dynamic_cast<TObject *>(&P) != nullptr);
}

void TestCriticalSectionIsRecursive()
{
  // Delphi's TCriticalSection is recursive. If this were a plain std::mutex,
  // the second Acquire would deadlock rather than fail a test.
  TCriticalSection CS;
  CS.Acquire();
  CS.Acquire();
  CHECK(CS.Acquired == 2);
  CS.Release();
  CHECK(CS.Acquired == 1);
  CS.Release();
  CHECK(CS.Acquired == 0);

  CS.Enter();
  CS.Leave();
  CHECK(CS.Acquired == 0);

  CHECK(CS.TryEnter());
  CS.Release();
}

void TestCriticalSectionExcludesThreads()
{
  TCriticalSection CS;
  int Counter = 0;
  const int Iterations = 2000;

  auto Bump = [&]() {
    for (int I = 0; I < Iterations; ++I)
    {
      CS.Acquire();
      const int Seen = Counter;
      Counter = Seen + 1;      // split read/write to widen any race window
      CS.Release();
    }
  };

  std::thread A(Bump);
  std::thread B(Bump);
  A.join();
  B.join();
  CHECK(Counter == 2 * Iterations);
}

void TestMethodPointer()
{
  TMethod M;
  CHECK(M.Code == nullptr && M.Data == nullptr);
  int Dummy = 0;
  M.Data = &Dummy;
  CHECK(M.Data == &Dummy);
}

void TestMetaclasses()
{
  EAbort E(UnicodeString(L"x"));
  const TObject * O = &E;
  CHECK(O->InheritsFrom(__classid(EAbort)));      // itself
  CHECK(O->InheritsFrom(__classid(Exception)));   // an ancestor
  CHECK(O->InheritsFrom(__classid(TObject)));
  CHECK(!O->InheritsFrom(__classid(TPersistent)));
  CHECK(!O->InheritsFrom(static_cast<TClass>(nullptr)));
  // One metaclass per type, as Delphi's TClass values compare.
  CHECK(__classid(EAbort) == __classid(EAbort));
  CHECK(__classid(EAbort) != __classid(Exception));
}

void TestClassName()
{
  EAbort E(UnicodeString(L"x"));
  const TObject & AsRoot = E;
  CHECK(AsRoot.ClassName() == UnicodeString(L"EAbort"));   // the dynamic type
  Outer::TInner I;
  CHECK(I.ClassName() == UnicodeString(L"TInner"));
  CHECK(I.QualifiedClassName() == UnicodeString(L"Outer.TInner"));
}

}  // namespace

int main()
{
  TestExceptionMessage();
  TestExceptionHierarchy();
  TestExceptionIsATObject();
  TestExceptionCopyKeepsEveryMember();
  TestObjectIsPolymorphic();
  TestCriticalSectionIsRecursive();
  TestCriticalSectionExcludesThreads();
  TestMethodPointer();

  TestMetaclasses();
  TestClassName();

  if (Failures == 0)
  {
    std::printf("all object/exception/sync tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
