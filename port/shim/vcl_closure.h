//---------------------------------------------------------------------------
// TClosure: Embarcadero's __closure, which is a method pointer bound to an
// instance.
//
// Delphi spells these as
//     typedef void __fastcall (__closure * TNotifyEvent)(TObject * Sender);
// and transform.py rewrites that to
//     using TNotifyEvent = TClosure<void(TObject *)>;
//
// The contract is set by how core actually uses them, which was measured rather
// than guessed: comparison against NULL (62 sites), equality against NULL (31),
// assignment of NULL (51), truth-testing, and calling. NULL rather than nullptr
// throughout, so the null comparisons must accept a null pointer constant.
//
// Binding to an instance is the part Borland did implicitly. Where core writes
//     OnCaptureOutput = CaptureOutput;
// naming a member function with no object and no &, which is not valid C++ at
// all, the transform wraps it as
//     OnCaptureOutput = CLOSURE(CaptureOutput);
// and the macro recovers the enclosing class from decltype(*this).
//
// Equality is Borland's: two closures are equal when both are null, or both
// name the same method on the same object. Core relies on it -- TTerminalThread
// asserts its handlers were restored by comparing them. A bare std::function
// has no equality at all, so a closure made by CLOSURE() carries its (instance,
// member pointer) identity alongside the callable. One made from any other
// callable gets a token its copies share: equal to its copies, and to nothing
// else.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_CLOSURE_H
#define LINSCP_VCL_CLOSURE_H

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <typeinfo>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace linscp {

// What Borland's TMethod holds: the code and the instance.
struct TMethodIdentity
{
  const void * Instance = nullptr;
  // A member function pointer, as raw bytes. Itanium's ABI -- clang's on both
  // targets, mingw included -- makes every one two words; checked below.
  alignas(void *) unsigned char Method[2 * sizeof(void *)] = {};

  friend bool operator==(const TMethodIdentity & A, const TMethodIdentity & B)
  {
    return (A.Instance == B.Instance) &&
           (std::memcmp(A.Method, B.Method, sizeof(A.Method)) == 0);
  }
};

// A method bound to an instance: callable, and knows its identity.
template <typename C, typename PMF>
class TBoundMethod
{
public:
  static_assert(sizeof(PMF) <= sizeof(TMethodIdentity::Method),
                "member function pointer larger than two words");

  TBoundMethod(C * Instance, PMF Method) : FInstance(Instance), FMethod(Method) {}

  template <typename... A>
  decltype(auto) operator()(A &&... Args) const
  {
    return (FInstance->*FMethod)(std::forward<A>(Args)...);
  }

  TMethodIdentity Identity() const
  {
    TMethodIdentity Result;
    Result.Instance = FInstance;
    std::memcpy(Result.Method, &FMethod, sizeof(FMethod));
    return Result;
  }

private:
  C * FInstance;
  PMF FMethod;
};

template <typename T>
struct IsBoundMethod : std::false_type {};
template <typename C, typename PMF>
struct IsBoundMethod<TBoundMethod<C, PMF>> : std::true_type {};

}  // namespace linscp

template <typename Signature>
class TClosure;

template <typename R, typename... A>
class TClosure<R(A...)>
{
public:
  using TSignature = R(A...);

  TClosure() = default;
  TClosure(std::nullptr_t) {}
  TClosure(const TClosure &) = default;
  TClosure(TClosure &&) noexcept = default;
  TClosure & operator=(const TClosure &) = default;
  TClosure & operator=(TClosure &&) noexcept = default;

  // Any compatible callable: a free function, a lambda, or the result of
  // MakeClosure below.
  template <typename F,
            typename = std::enable_if_t<
              !std::is_same_v<std::decay_t<F>, TClosure> &&
              std::is_invocable_r_v<R, std::decay_t<F> &, A...>>>
  TClosure(F && Callable)
  {
    if constexpr (linscp::IsBoundMethod<std::decay_t<F>>::value)
    {
      FBound = true;
      FIdentity = Callable.Identity();
    }
    else
    {
      // A null function pointer makes an empty closure, as it would in Borland.
      if constexpr (std::is_pointer_v<std::decay_t<F>>)
      {
        if (Callable == nullptr)
        {
          return;
        }
      }
      FToken = std::make_shared<char>();
    }
    FFn = std::forward<F>(Callable);
  }

  // Borland casts freely between closure types: a closure is a (code, data)
  // pair, so the cast only reinterprets it. Core does this to launder a
  // closure through another type and back -- Terminal.cpp passes a
  // TProcessFileEventEx through ProcessFiles as a TProcessFileEvent and casts
  // it back before calling it. So a cast keeps the original, a cast back
  // recovers it exactly (identity included), and calling a closure through a
  // signature it was not made for stops the program: in Borland that is
  // undefined behaviour, and core never does it.
  template <typename R2, typename... A2,
            typename = std::enable_if_t<!std::is_same_v<R2(A2...), R(A...)>>>
  explicit TClosure(const TClosure<R2(A2...)> & Other)
  {
    if (!Other)
    {
      return;
    }
    if (Other.FOriginalType == &typeid(TClosure))
    {
      *this = *static_cast<const TClosure *>(Other.FOriginal.get());
      return;
    }
    if (Other.FOriginal)
    {
      // Already laundered from a third type: keep the true original.
      FOriginal = Other.FOriginal;
      FOriginalType = Other.FOriginalType;
    }
    else
    {
      FOriginal = std::make_shared<TClosure<R2(A2...)>>(Other);
      FOriginalType = &typeid(TClosure<R2(A2...)>);
    }
    FBound = Other.FBound;
    FIdentity = Other.FIdentity;
    FToken = Other.FToken;
    FFn = [](A...) -> R
    {
      std::fputs("linscp: closure called through a signature it was cast to, "
                 "not made with\n", stderr);
      std::terminate();
    };
  }

  TClosure & operator=(std::nullptr_t)
  {
    FFn = nullptr;
    FBound = false;
    FIdentity = linscp::TMethodIdentity();
    FToken.reset();
    FOriginal.reset();
    FOriginalType = nullptr;
    return *this;
  }

  R operator()(A... Args) const
  {
    return FFn(std::forward<A>(Args)...);
  }

  // Deliberately explicit: core writes `if (OnX)`, which works, while an
  // implicit conversion would also let a closure silently become an int.
  explicit operator bool() const { return static_cast<bool>(FFn); }

  // Core compares against NULL, not nullptr. A null pointer constant converts
  // to std::nullptr_t, so these cover both spellings.
  friend bool operator==(const TClosure & C, std::nullptr_t)
    { return !static_cast<bool>(C.FFn); }
  friend bool operator==(std::nullptr_t, const TClosure & C)
    { return !static_cast<bool>(C.FFn); }
  friend bool operator!=(const TClosure & C, std::nullptr_t)
    { return static_cast<bool>(C.FFn); }
  friend bool operator!=(std::nullptr_t, const TClosure & C)
    { return static_cast<bool>(C.FFn); }

  // Borland identity: same object and same method, or both null.
  friend bool operator==(const TClosure & X, const TClosure & Y)
  {
    const bool XNull = !static_cast<bool>(X.FFn);
    const bool YNull = !static_cast<bool>(Y.FFn);
    if (XNull || YNull)
    {
      return XNull && YNull;
    }
    if (X.FBound || Y.FBound)
    {
      return X.FBound && Y.FBound && (X.FIdentity == Y.FIdentity);
    }
    return X.FToken == Y.FToken;
  }
  friend bool operator!=(const TClosure & X, const TClosure & Y)
    { return !(X == Y); }

private:
  template <typename> friend class TClosure;

  std::function<R(A...)> FFn;
  bool FBound = false;
  linscp::TMethodIdentity FIdentity;
  std::shared_ptr<char> FToken;  // identity of a non-method callable
  // Set only on a closure produced by a cast between closure types.
  std::shared_ptr<const void> FOriginal;
  const std::type_info * FOriginalType = nullptr;
};

// Bind an instance to one of its member functions. Returns a TBoundMethod, so
// the closure it initialises knows which method on which object it holds.
template <typename C, typename R, typename... A>
auto MakeClosure(C * Instance, R (C::*Method)(A...))
{
  return linscp::TBoundMethod<C, R (C::*)(A...)>(Instance, Method);
}

template <typename C, typename R, typename... A>
auto MakeClosure(const C * Instance, R (C::*Method)(A...) const)
{
  return linscp::TBoundMethod<const C, R (C::*)(A...) const>(Instance, Method);
}

// The classic VCL event signature, used across core and declared by the VCL
// rather than by WinSCP, so it has to come from here.
class TObject;
using TNotifyEvent = TClosure<void(TObject *)>;
// TThread::Synchronize's argument, which TDiscMonitor's OnSynchronize passes on.
using TThreadMethod = TClosure<void()>;

// Used at the call sites the transform rewrites. The enclosing class comes from
// decltype(*this), so the macro needs only the method name -- which is all the
// original Borland source gives us.
#define CLOSURE(METHOD) \
  MakeClosure(this, &std::remove_reference_t<decltype(*this)>::METHOD)

// For binding a method of some other object.
#define CLOSURE_OF(OBJ, METHOD) \
  MakeClosure(OBJ, &std::remove_pointer_t<std::remove_reference_t<decltype(OBJ)>>::METHOD)

#endif // LINSCP_VCL_CLOSURE_H
