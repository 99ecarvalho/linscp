# ADR 0005 — Where upstream relies on C++Builder, the shim is C++Builder

**Status:** accepted 2026-10-12
**Date:** 2026-10-12

## Context

Core is written for C++Builder and the VCL, and silently relies on their
semantics where standard C++ differs. Each found so far was a real bug on
Linux, found only by running real sessions:

- Delphi zeroes an object before its constructor runs; `TSessionData`'s
  constructor wrote through an unset `FUnsafeSettings`, `ExtException`'s read
  an unset `FMoreMessages`.
- A property read from a field *is* the field; `Rights.Number = rfDefault`
  changed a copy and SCP uploaded files with mode 0000.
- `UnicodeString::SubString(0, n)` takes the index as 1 and keeps the count;
  read as Delphi's `Copy`, S3 bucket names lost a character.
- `towupper` returns `wchar_t`; mingw's returns `wint_t`, and file types were
  logged as numbers.
- `FindFirst("*.*")` matches every name; read literally, synchronization did
  not see local directories without a dot and `-delete` removed them remotely.

## Decision

The shim and the platform layer reproduce C++Builder's and Windows' observable
behaviour wherever upstream relies on it, rather than standard C++'s or
Delphi's documentation. Evidence decides: what WinSCP itself does (upstream
code that works only one way, real WinSCP's output), then the real API under
Wine or natively. Specifically: every allocation and every thrown object
starts zeroed (a global `operator new`, `--wrap=__cxa_allocate_exception`),
field-backed class properties are references, and the rest as listed.

## Consequences

- Correct code cannot tell a zeroed allocation apart; the cost is a `calloc`
  per allocation.
- The optimizer must not know better: clang otherwise takes memory from the
  global `operator new` as uninitialized and, at -O2 and up, deletes code that
  reads a field no constructor set. Everything built against the shim gets
  `-fno-assume-sane-operator-new` (port/shim/CMakeLists.txt), and
  `object_exceptions` builds at -O2 in every build type to keep it so.
- Each such semantic gets a test on both targets (`object_exceptions`,
  `unicodestring`, `sysutils`, ...).
- The golden comparison (ADR 0007) is what finds the next one.
