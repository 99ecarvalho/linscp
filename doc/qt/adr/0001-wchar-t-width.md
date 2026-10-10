# ADR 0001 — Character width for `UnicodeString`

**Status:** open — blocks P1-01
**Date:** 2026-10-10

## Context

`UnicodeString` is the single most-used type in the core: **6,248 references**.
Getting its character representation wrong is not a detail, it is a rewrite.

Delphi's `UnicodeString` is **UTF-16**, and `wchar_t` on Windows is 2 bytes, so
upstream's `wchar_t`, `L"..."` literals and Win32 `W` APIs all agree today.

On Linux `wchar_t` is **4 bytes**. That breaks the agreement, and the core
contains thousands of `L"..."` literals plus code that assumes 2-byte characters
when talking to Win32.

Far-NetBox does not face this: it is Windows-only and simply uses `wchar_t`.

## Options

**A. `char16_t` / `std::u16string` throughout.**
Correct and portable, but `L"..."` literals are `const wchar_t*` and will not
convert. Every one of the thousands of literals in core would need rewriting to
`u"..."` — a large, invasive transform, and one that would conflict with upstream
on far more than the 613 property lines. Destroys the near-zero-diff property.

**B. `wchar_t` with `-fshort-wchar` on Linux.**
Makes `wchar_t` 2 bytes and `L"..."` literals UTF-16 on both platforms, so
upstream code compiles unmodified. The catch: glibc's wide functions
(`wcslen`, `wcscmp`, `swprintf`) and `libstdc++`'s `char_traits<wchar_t>`
are compiled for 4-byte `wchar_t`, so calling them is an ABI mismatch. The shim
must therefore implement its own string primitives and must not use
`std::wstring` or wide iostreams. That cost is largely unavoidable anyway, since
the shim needs Delphi semantics rather than standard-library ones.

**C. `wchar_t` as-is, 4 bytes on Linux.**
Doubles memory for every string, silently diverges from upstream's UTF-16
assumptions, and breaks any code that round-trips strings through byte counts or
Win32 structures. Rejected.

## Decision

**Option B**, with the shim owning its string primitives and `std::wstring`
banned from the port. It is the only option that keeps upstream source
unmodified, which is the constraint the whole project rests on.

The ABI hazard is real but containable, and it is containable *because* the shim
already has to exist. Enforce it mechanically: a test that asserts
`sizeof(wchar_t) == 2` on every target, and a CI grep rejecting `std::wstring`
and `<cwchar>` in `port/`.

## Consequences

- `-fshort-wchar` must be set for every Linux target, uniformly. A single
  translation unit compiled without it is an ABI landmine.
- The shim implements length, compare, copy, search and formatting itself.
- Third-party libraries taking wide strings must be checked individually.
  OpenSSL, expat, neon and libs3 are byte-oriented, so the exposure is small.
- Qt uses UTF-16 internally (`QString`), so conversion to and from the shim is
  cheap — a point in this option's favour for Phase 2.

## Open question for review

This is the highest-leverage irreversible decision in Phase 1. It is recorded as
**Q-8** and should be confirmed before `UnicodeString` is written, because
reversing it later means touching all 6,248 references.
