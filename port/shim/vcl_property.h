//---------------------------------------------------------------------------
// Dual-target emulation of Embarcadero's __property extension.
//
// On C++Builder (__BORLANDC__) every macro expands to the native VCL syntax,
// so transformed headers remain byte-equivalent in meaning to upstream and the
// existing VCL build keeps working. Everywhere else they expand to
// __declspec(property), which clang supports under -fms-extensions on all
// targets (verified: linux-gnu, windows-gnu, windows-msvc).
//
// GCC is not supported: it has no __declspec(property) equivalent.
//
// Naming: generated accessors are prefixed __gp_/__sp_ to avoid colliding with
// WinSCP's own Get*/Set* methods.
//---------------------------------------------------------------------------
#ifndef LINSCP_VCL_PROPERTY_H
#define LINSCP_VCL_PROPERTY_H

#include <type_traits>

#if defined(__BORLANDC__)

  #define PROP_R_F(T, Name, Field)              __property T Name = { read = Field }
  #define PROP_R_G(T, Name, Getter)             __property T Name = { read = Getter }
  #define PROP_RW_FF(T, Name, Field)            __property T Name = { read = Field, write = Field }
  #define PROP_RW_F2(T, Name, RField, WField)   __property T Name = { read = RField, write = WField }
  #define PROP_RW_FS(T, Name, Field, Setter)    __property T Name = { read = Field, write = Setter }
  #define PROP_RW_GF(T, Name, Getter, Field)    __property T Name = { read = Getter, write = Field }
  #define PROP_RW_GS(T, Name, Getter, Setter)   __property T Name = { read = Getter, write = Setter }
  #define PROP_W_F(T, Name, Field)              __property T Name = { write = Field }
  #define PROP_W_S(T, Name, Setter)             __property T Name = { write = Setter }
  #define PROP_R_GI(T, Name, Getter, Idx)       __property T Name = { read = Getter, index = Idx }
  #define PROP_IDX_R_G(T, Name, IT, Getter)     __property T Name[IT Index] = { read = Getter }
  #define PROP_IDX_RW_GS(T, Name, IT, G, S)     __property T Name[IT Index] = { read = G, write = S }

#elif defined(__clang__)

  // Every form routes through a generated wrapper rather than naming the
  // accessor in the __declspec, even when a direct reference would compile.
  //
  // This is not cosmetic. Borland's __property grants access to a *private*
  // accessor from outside the class -- core relies on it, e.g. TBookmark
  // declares public properties over private SetName/SetLocal/SetRemote.
  // __declspec(property) has no such rule: it expands to an ordinary member
  // call at the use site, which fails on a private accessor. The wrappers are
  // declared where the property is (public) and call the accessor from inside
  // the class, which restores Borland's behaviour.

  // read = <field>
  //
  // Borland makes a field-backed property the field itself: core writes
  // `Rights.Number = TRights::rfDefault` (TCopyParamType::Default) and
  // expects the field to change. A class-typed field of the property's own
  // type is therefore returned by reference (TFieldRead); a scalar by value,
  // converted to the property's type, as before.
  #define PROP_R_F(T, Name, Field)                                            \
    linscp::TFieldRead<T> __gp_##Name() { return static_cast<linscp::TFieldRead<T>>(Field); }      \
    linscp::TFieldReadConst<T> __gp_##Name() const { return static_cast<linscp::TFieldReadConst<T>>(Field); } \
    __declspec(property(get = __gp_##Name)) T Name

  // Method-backed reads go through LINSCP_SELF, which drops constness before
  // calling the accessor.
  //
  // That mirrors Borland: __property has no const-correctness, so reading a
  // property on a const object calls the getter whether or not the getter is
  // const -- and core's getters are often non-const because they genuinely
  // mutate, as TStream's Position getter does by seeking. Marking the wrapper
  // const without the cast would reject reads on const objects; leaving the
  // wrapper non-const would reject them too, from the other direction.
  #define LINSCP_SELF                                                         \
    const_cast<std::remove_const_t<                                           \
      std::remove_reference_t<decltype(*this)>> *>(this)

  // The result is cast to the property's declared type as well. Borland is lax
  // about the two differing in constness -- TConfiguration declares
  // `__property TSshHostCAList * SshHostCAList` over a getter returning
  // `const TSshHostCAList *` -- and a C++ wrapper has to reproduce that or the
  // declaration does not compile. A cast rather than a const_cast because the
  // same wrapper serves value-typed properties, where it is a no-op.
  #define LINSCP_AS(T, EXPR) ((T)(EXPR))

namespace linscp {
// A field-backed property's read: the field itself when the property's type
// is a class, so members can be changed through the property, as under
// Borland; otherwise the field converted to the property's type. A class
// field of another type than the property's does not bind, and fails to
// compile rather than copy.
template <typename T>
using TFieldRead = std::conditional_t<std::is_class_v<T>, T &, T>;
template <typename T>
using TFieldReadConst = std::conditional_t<std::is_class_v<T>, const T &, T>;
}  // namespace linscp
  #define PROP_R_G(T, Name, Getter)                                           \
    T __gp_##Name() const { return LINSCP_AS(T, LINSCP_SELF->Getter()); }     \
    __declspec(property(get = __gp_##Name)) T Name

  // read = <field>, write = <same field>
  #define PROP_RW_FF(T, Name, Field)                                          \
    linscp::TFieldRead<T> __gp_##Name() { return static_cast<linscp::TFieldRead<T>>(Field); }      \
    linscp::TFieldReadConst<T> __gp_##Name() const { return static_cast<linscp::TFieldReadConst<T>>(Field); }\
    void __sp_##Name(T __v) { Field = __v; }                                   \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <field>, write = <a different field>
  #define PROP_RW_F2(T, Name, RField, WField)                                 \
    linscp::TFieldRead<T> __gp_##Name() { return static_cast<linscp::TFieldRead<T>>(RField); }     \
    linscp::TFieldReadConst<T> __gp_##Name() const { return static_cast<linscp::TFieldReadConst<T>>(RField); }\
    void __sp_##Name(T __v) { WField = __v; }                                 \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <field>, write = <method>
  #define PROP_RW_FS(T, Name, Field, Setter)                                  \
    linscp::TFieldRead<T> __gp_##Name() { return static_cast<linscp::TFieldRead<T>>(Field); }      \
    linscp::TFieldReadConst<T> __gp_##Name() const { return static_cast<linscp::TFieldReadConst<T>>(Field); }\
    void __sp_##Name(T __v) { Setter(__v); }                                  \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <method>, write = <field>
  #define PROP_RW_GF(T, Name, Getter, Field)                                  \
    T __gp_##Name() const { return LINSCP_AS(T, LINSCP_SELF->Getter()); }     \
    void __sp_##Name(T __v) { Field = __v; }                                  \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <method>, write = <method>
  #define PROP_RW_GS(T, Name, Getter, Setter)                                 \
    T __gp_##Name() const { return LINSCP_AS(T, LINSCP_SELF->Getter()); }     \
    void __sp_##Name(T __v) { Setter(__v); }                                  \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <method>, index = <constant>  (Borland passes Idx to the getter)
  #define PROP_R_GI(T, Name, Getter, Idx)                                     \
    auto __gp_##Name() const -> T                                             \
      { return LINSCP_AS(T, LINSCP_SELF->Getter(Idx)); }                      \
    __declspec(property(get = __gp_##Name)) T Name

  // write-only
  #define PROP_W_F(T, Name, Field)                                            \
    void __sp_##Name(T __v) { Field = __v; }                                  \
    __declspec(property(put = __sp_##Name)) T Name

  #define PROP_W_S(T, Name, Setter)                                           \
    void __sp_##Name(T __v) { Setter(__v); }                                  \
    __declspec(property(put = __sp_##Name)) T Name

  // Indexed properties.
  //
  // LIMITATION: clang reads a chained subscript on one of these -- Obj.Prop[i][j]
  // -- as a single two-argument getter call, not as subscripting the result.
  // Upstream core never does this (verified: 0 occurrences), and
  // check_properties.py fails if that ever changes. Where you need it, take the
  // result into a local first.
  #define PROP_IDX_R_G(T, Name, IT, Getter)                                   \
    T __gp_##Name(IT __i) const                                               \
      { return LINSCP_AS(T, LINSCP_SELF->Getter(__i)); }                      \
    __declspec(property(get = __gp_##Name)) T Name[]

  #define PROP_IDX_RW_GS(T, Name, IT, G, S)                                   \
    T __gp_##Name(IT __i) const                                               \
      { return LINSCP_AS(T, LINSCP_SELF->G(__i)); }                           \
    void __sp_##Name(IT __i, T __v) { S(__i, __v); }                          \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name[]

#else
  #error "linscp requires clang or C++Builder: GCC has no __declspec(property)."
#endif

#endif // LINSCP_VCL_PROPERTY_H
