// Borland's try/__finally, as the transform rewrites it.
//
// Written in the macro form the transform emits, so this tests the same
// expansion core gets rather than a hand-written approximation. The cleanup
// must run on every path out of the guarded body, which is the whole point of
// the construct and the thing core relies on for releasing handles and locks.
#include "vcl_tryfinally.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

std::vector<std::string> Order;

void TestNormalPath()
{
  Order.clear();
  LINSCP_TRY
  {
    Order.push_back("body");
  }
  LINSCP_FINALLY
  {
    Order.push_back("cleanup");
  LINSCP_END_FINALLY

  CHECK(Order.size() == 2);
  CHECK(Order[0] == "body");
  CHECK(Order[1] == "cleanup");      // cleanup runs after, not before
}

void TestExceptionPath()
{
  Order.clear();
  bool Caught = false;
  try
  {
    LINSCP_TRY
    {
      Order.push_back("body");
      throw std::runtime_error("boom");
    }
    LINSCP_FINALLY
    {
      Order.push_back("cleanup");
    LINSCP_END_FINALLY
    Order.push_back("unreachable");
  }
  catch (const std::runtime_error & E)
  {
    Caught = (std::string(E.what()) == "boom");
  }

  CHECK(Caught);                     // the exception still propagates
  CHECK(Order.size() == 2);
  CHECK(Order[1] == "cleanup");      // and the cleanup ran first
}

void TestCleanupRunsExactlyOnce()
{
  // A catch-and-rethrow implementation would run it twice on the normal path
  // or miss it entirely on one branch; count rather than assume.
  int Count = 0;
  LINSCP_TRY
  {
  }
  LINSCP_FINALLY
  {
    ++Count;
  LINSCP_END_FINALLY
  CHECK(Count == 1);

  Count = 0;
  try
  {
    LINSCP_TRY
    {
      throw 1;
    }
    LINSCP_FINALLY
    {
      ++Count;
    LINSCP_END_FINALLY
  }
  catch (int) { }
  CHECK(Count == 1);
}

void TestNesting()
{
  // Core nests these, and a nested block is what broke the first version of
  // the transform: rewriting the inner one removed a brace and corrupted the
  // outer one's depth count.
  Order.clear();
  LINSCP_TRY
  {
    LINSCP_TRY
    {
      Order.push_back("inner body");
    }
    LINSCP_FINALLY
    {
      Order.push_back("inner cleanup");
    LINSCP_END_FINALLY
    Order.push_back("outer body");
  }
  LINSCP_FINALLY
  {
    Order.push_back("outer cleanup");
  LINSCP_END_FINALLY

  CHECK(Order.size() == 4);
  CHECK(Order[0] == "inner body");
  CHECK(Order[1] == "inner cleanup");
  CHECK(Order[2] == "outer body");
  CHECK(Order[3] == "outer cleanup");
}

void TestNestedExceptionUnwindsBothCleanups()
{
  Order.clear();
  bool Caught = false;
  try
  {
    LINSCP_TRY
    {
      LINSCP_TRY
      {
        throw 7;
      }
      LINSCP_FINALLY
      {
        Order.push_back("inner cleanup");
      LINSCP_END_FINALLY
    }
    LINSCP_FINALLY
    {
      Order.push_back("outer cleanup");
    LINSCP_END_FINALLY
  }
  catch (int V) { Caught = (V == 7); }

  CHECK(Caught);
  CHECK(Order.size() == 2);
  CHECK(Order[0] == "inner cleanup");   // innermost first, as unwinding does
  CHECK(Order[1] == "outer cleanup");
}

void TestCapturesLocalsByReference()
{
  // The body and cleanup become lambdas, so both must see and mutate the
  // enclosing locals. Capturing by value would make core's cleanup operate on
  // stale copies of the handles it is meant to release.
  int Resource = 0;
  bool Released = false;
  LINSCP_TRY
  {
    Resource = 5;
  }
  LINSCP_FINALLY
  {
    Released = (Resource == 5);
    Resource = 0;
  LINSCP_END_FINALLY
  CHECK(Released);
  CHECK(Resource == 0);
}

// A bare `return` where the block ends the function: the transform permits
// this one shape, because returning from the lambda runs the cleanup through
// the guard and then falls off the end, which is what Borland does.
void ReturnsEarly(int & Cleaned, bool Early)
{
  LINSCP_TRY
  {
    if (Early)
    {
      return;
    }
  }
  LINSCP_FINALLY
  {
    ++Cleaned;
  LINSCP_END_FINALLY
}

void TestBareReturnStillCleansUp()
{
  int Cleaned = 0;
  ReturnsEarly(Cleaned, true);
  CHECK(Cleaned == 1);
  ReturnsEarly(Cleaned, false);
  CHECK(Cleaned == 2);
}

}  // namespace

int main()
{
  TestNormalPath();
  TestExceptionPath();
  TestCleanupRunsExactlyOnce();
  TestNesting();
  TestNestedExceptionUnwindsBothCleanups();
  TestCapturesLocalsByReference();
  TestBareReturnStillCleansUp();

  if (Failures == 0)
  {
    std::printf("all try/__finally tests passed\n");
  }
  return Failures == 0 ? 0 : 1;
}
