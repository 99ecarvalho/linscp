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
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_CLOSURE_H
#define LINSCP_VCL_CLOSURE_H

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

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
  TClosure(F && Callable) : FFn(std::forward<F>(Callable)) {}

  TClosure & operator=(std::nullptr_t)
  {
    FFn = nullptr;
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

private:
  std::function<R(A...)> FFn;
};

// Bind an instance to one of its member functions.
template <typename C, typename R, typename... A>
auto MakeClosure(C * Instance, R (C::*Method)(A...))
{
  return [Instance, Method](A... Args) -> R
  {
    return (Instance->*Method)(std::forward<A>(Args)...);
  };
}

template <typename C, typename R, typename... A>
auto MakeClosure(const C * Instance, R (C::*Method)(A...) const)
{
  return [Instance, Method](A... Args) -> R
  {
    return (Instance->*Method)(std::forward<A>(Args)...);
  };
}

// The classic VCL event signature, used across core and declared by the VCL
// rather than by WinSCP, so it has to come from here.
class TObject;
using TNotifyEvent = TClosure<void(TObject *)>;

// Used at the call sites the transform rewrites. The enclosing class comes from
// decltype(*this), so the macro needs only the method name -- which is all the
// original Borland source gives us.
#define CLOSURE(METHOD) \
  MakeClosure(this, &std::remove_reference_t<decltype(*this)>::METHOD)

// For binding a method of some other object.
#define CLOSURE_OF(OBJ, METHOD) \
  MakeClosure(OBJ, &std::remove_pointer_t<std::remove_reference_t<decltype(OBJ)>>::METHOD)

#endif // LINSCP_VCL_CLOSURE_H
