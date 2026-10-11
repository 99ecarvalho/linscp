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

  // read = <field>
  #define PROP_R_F(T, Name, Field)                                            \
    T __gp_##Name() const { return Field; }                                   \
    __declspec(property(get = __gp_##Name)) T Name

  // read = <method>
  #define PROP_R_G(T, Name, Getter)                                           \
    __declspec(property(get = Getter)) T Name

  // read = <field>, write = <same field>
  #define PROP_RW_FF(T, Name, Field)                                          \
    T __gp_##Name() const { return Field; }                                   \
    void __sp_##Name(T __v) { Field = __v; }                                  \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <field>, write = <a different field>
  #define PROP_RW_F2(T, Name, RField, WField)                                 \
    T __gp_##Name() const { return RField; }                                  \
    void __sp_##Name(T __v) { WField = __v; }                                 \
    __declspec(property(get = __gp_##Name, put = __sp_##Name)) T Name

  // read = <field>, write = <method>
  #define PROP_RW_FS(T, Name, Field, Setter)                                  \
    T __gp_##Name() const { return Field; }                                   \
    __declspec(property(get = __gp_##Name, put = Setter)) T Name

  // read = <method>, write = <field>
  #define PROP_RW_GF(T, Name, Getter, Field)                                  \
    void __sp_##Name(T __v) { Field = __v; }                                  \
    __declspec(property(get = Getter, put = __sp_##Name)) T Name

  // read = <method>, write = <method>
  #define PROP_RW_GS(T, Name, Getter, Setter)                                 \
    __declspec(property(get = Getter, put = Setter)) T Name

  // read = <method>, index = <constant>  (Borland passes Idx to the getter)
  #define PROP_R_GI(T, Name, Getter, Idx)                                     \
    auto __gp_##Name() const -> T { return Getter(Idx); }                     \
    __declspec(property(get = __gp_##Name)) T Name

  // write-only
  #define PROP_W_F(T, Name, Field)                                            \
    void __sp_##Name(T __v) { Field = __v; }                                  \
    __declspec(property(put = __sp_##Name)) T Name

  #define PROP_W_S(T, Name, Setter)                                           \
    __declspec(property(put = Setter)) T Name

  // Indexed properties.
  //
  // LIMITATION: clang reads a chained subscript on one of these -- Obj.Prop[i][j]
  // -- as a single two-argument getter call, not as subscripting the result.
  // Upstream core never does this (verified: 0 occurrences), and
  // check_properties.py fails if that ever changes. Where you need it, take the
  // result into a local first.
  #define PROP_IDX_R_G(T, Name, IT, Getter)                                   \
    __declspec(property(get = Getter)) T Name[]

  #define PROP_IDX_RW_GS(T, Name, IT, G, S)                                   \
    __declspec(property(get = G, put = S)) T Name[]

#else
  #error "linscp requires clang or C++Builder: GCC has no __declspec(property)."
#endif

#endif // LINSCP_VCL_PROPERTY_H
