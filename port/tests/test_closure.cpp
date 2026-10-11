// TClosure: Embarcadero's __closure.
//
// The contract tested here was measured from core rather than assumed: core
// compares these against NULL (62 sites), tests equality with NULL (31),
// assigns NULL (51), truth-tests them, and calls them. It uses NULL throughout
// rather than nullptr, so the null comparisons must accept a null pointer
// constant and not only std::nullptr_t.
#include "vcl_closure.h"
#include "vcl_unicodestring.h"

#include <cstdio>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

// Shapes taken from real core typedefs.
using TNotifyEvent = TClosure<void(int)>;
using TTransferInEvent = TClosure<std::size_t(int, unsigned char *, std::size_t)>;
using TCancelEvent = TClosure<void(const UnicodeString &, bool &)>;

int FreeSeen = 0;
void FreeHandler(int V) { FreeSeen = V; }

struct TOwner
{
  int Got = 0;
  UnicodeString LastName;

  void Handler(int V) { Got = V; }
  void Other(int V) { Got = V * 2; }
  std::size_t Transfer(int, unsigned char *, std::size_t Len) { return Len + 1; }
  void MaybeCancel(const UnicodeString & Name, bool & Cancel)
  {
    LastName = Name;
    Cancel = true;
  }

  // The Borland idiom the transform must support: a bare method name, with no
  // object and no &, assigned where a closure is expected.
  void WireUp(TNotifyEvent & E) { E = CLOSURE(Handler); }
  TTransferInEvent MakeTransfer() { return CLOSURE(Transfer); }
  TCancelEvent MakeCancel() { return CLOSURE(MaybeCancel); }
};

void TestNullIdioms()
{
  TNotifyEvent E;
  CHECK(E == NULL);                      // NULL, not nullptr: core's spelling
  CHECK(NULL == E);                      // and reversed
  CHECK(!(E != NULL));
  CHECK(!E);
  CHECK(E == nullptr);

  E = FreeHandler;
  CHECK(E != NULL);
  CHECK(NULL != E);
  CHECK(static_cast<bool>(E));

  E = NULL;                              // assignment of NULL clears it
  CHECK(E == NULL);
  CHECK(!E);
}

void TestFreeFunction()
{
  TNotifyEvent E = FreeHandler;
  FreeSeen = 0;
  E(11);
  CHECK(FreeSeen == 11);
}

void TestBoundMemberCarriesTheInstance()
{
  TOwner A;
  TOwner B;
  TNotifyEvent E;

  A.WireUp(E);
  E(5);
  CHECK(A.Got == 5);
  CHECK(B.Got == 0);                     // the right instance, not just the type

  B.WireUp(E);
  E(7);
  CHECK(B.Got == 7);
  CHECK(A.Got == 5);                     // rebinding does not disturb the first
}

void TestNonVoidReturnAndReferenceArgs()
{
  TOwner O;
  TTransferInEvent T = O.MakeTransfer();
  unsigned char Buf[4] = { 0 };
  CHECK(T(1, Buf, 9) == 10);             // return value propagates

  // A reference argument must reach the handler as a reference, since core uses
  // these to let a handler cancel an operation.
  TCancelEvent C = O.MakeCancel();
  bool Cancel = false;
  C(UnicodeString(L"file.txt"), Cancel);
  CHECK(Cancel);
  CHECK(O.LastName == UnicodeString(L"file.txt"));
}

void TestCopyAndReassign()
{
  TOwner O;
  TNotifyEvent E;
  O.WireUp(E);

  TNotifyEvent Copy = E;                 // copies stay bound to the same object
  Copy(3);
  CHECK(O.Got == 3);

  TNotifyEvent Assigned;
  Assigned = E;
  Assigned(4);
  CHECK(O.Got == 4);

  // Closure-to-closure assignment, which core does when forwarding handlers.
  TNotifyEvent Forward = NULL;
  CHECK(Forward == NULL);
  Forward = Assigned;
  CHECK(Forward != NULL);
  Forward(6);
  CHECK(O.Got == 6);
}

void TestLambdaAndOtherObject()
{
  int Seen = 0;
  TNotifyEvent E = [&Seen](int V) { Seen = V; };
  E(8);
  CHECK(Seen == 8);

  // Binding a method of a different object.
  TOwner Other;
  TOwner * P = &Other;
  TNotifyEvent F = CLOSURE_OF(P, Other);
  F(5);
  CHECK(Other.Got == 10);                // Other() doubles its argument
}

}  // namespace

int main()
{
  TestNullIdioms();
  TestFreeFunction();
  TestBoundMemberCarriesTheInstance();
  TestNonVoidReturnAndReferenceArgs();
  TestCopyAndReassign();
  TestLambdaAndOtherObject();

  if (Failures == 0)
  {
    std::printf("all closure tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
