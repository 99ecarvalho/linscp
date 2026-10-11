//---------------------------------------------------------------------------
// Borland's try/__finally.
//
// Core has 182 of these and no __try or __except. clang maps __finally onto
// SEH, which the mingw target supports and Linux does not, so the construct has
// to be expressed in standard C++ for the port.
//
// The transform rewrites three lines per block, which keeps the line count and
// therefore the diff against upstream intact:
//
//     try                  ->  LINSCP_TRY
//     {                        {
//       BODY                     BODY
//     }                        }
//     __finally            ->  LINSCP_FINALLY
//     {                        {
//       CLEANUP                  CLEANUP
//     }                    ->  LINSCP_END_FINALLY
//
// Under C++Builder the macros expand back to the original keywords, so the
// generated source still builds there. Elsewhere they expand to a call taking
// two lambdas, and the cleanup runs from a guard destructor so it happens on
// both the normal and the exceptional path.
//
// The same expansion is used for the mingw target even though SEH would work
// there. One code path on both targets is worth more than relying on SEH, and
// it means the two platforms cannot diverge in behaviour here.
//
// LIMITATION: a `return` inside the guarded body would return from the lambda
// rather than the enclosing function. Only 2 of the 182 blocks do that, and
// transform.py refuses to rewrite them so they stay visible.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_TRYFINALLY_H
#define LINSCP_VCL_TRYFINALLY_H

#include <utility>

namespace linscp {

template <typename TBody, typename TCleanup>
void TryFinally(TBody && Body, TCleanup && Cleanup)
{
  // A guard rather than a catch-and-rethrow, so the cleanup runs exactly once
  // on every path out of the body, including an exception.
  class TGuard
  {
  public:
    explicit TGuard(TCleanup & ACleanup) : FCleanup(ACleanup) {}
    ~TGuard() { FCleanup(); }
  private:
    TCleanup & FCleanup;
  };

  TGuard Guard(Cleanup);
  Body();
}

}  // namespace linscp

#if defined(__BORLANDC__)
  #define LINSCP_TRY try
  #define LINSCP_FINALLY __finally
  #define LINSCP_END_FINALLY }
#else
  #define LINSCP_TRY ::linscp::TryFinally([&]()
  #define LINSCP_FINALLY , [&]()
  #define LINSCP_END_FINALLY });
#endif

#endif // LINSCP_VCL_TRYFINALLY_H
