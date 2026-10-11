# linscp — Qt port of WinSCP

A cross-platform (Linux + Windows) Qt frontend over WinSCP's protocol core,
built so that upstream WinSCP remains trackable with near-zero merge cost.

## Documents

| File | Purpose |
|---|---|
| [PLAN.md](PLAN.md) | Architecture, strategy, repo/branch model, risks |
| [TASKS.md](TASKS.md) | Work breakdown with stable task IDs |
| [TRACKER.md](TRACKER.md) | Live status per task |
| [ESTIMATES.md](ESTIMATES.md) | Effort model, including AI-assisted estimate |

## Goal

One codebase. Qt UI. Builds and runs on Linux and Windows. No Embarcadero
C++Builder in the toolchain.

## Current state (2026-10-10)

Phase 0 is 19 of 30 done; Phase 1 is 6 of 27. Upstream is pinned to the latest
**stable** release, `6.5.8`, as a submodule at `3rd-party/winscp`.

Landed: the dual-target `__property` shim and transform, plus `UnicodeString`,
the byte-string family with a UTF-8 codec, `TDateTime`, the Delphi exception
hierarchy, `TCriticalSection`, `TStrings`/`TStringList`, and directory search on
both Win32 and POSIX. 12 test binaries pass on Linux and on Windows under Wine.

Real core code compiles against the shim: **2,409 errors across 9 translation
units**, ratcheted so the number can only go down. The largest remaining group
is `__closure` support.

## The three findings the plan rests on

1. **`__property` is emulatable.** clang supports `__declspec(property)` under
   `-fms-extensions`, verified on this machine (Debian clang 19) for every form
   WinSCP uses: field-backed r/o, field-backed r/w, method-backed with
   validating setter, read-modify-write, and indexed. GCC does not — **clang is
   mandatory**. WinSCP's own compiler is already clang-based, so this is not a
   compiler-family change.

2. **Windows binaries can be produced on Linux.** clang emits valid x86-64 COFF
   for `x86_64-w64-mingw32`, lld static-links it, and the result runs under Wine
   — the full test suite does exactly that on every build.

3. **The de-VCL'd core already exists and is maintained.** `Far-NetBox`
   (last commit 2026-07-24, tracking WinSCP 6.5.6 — one minor behind our pin)
   builds WinSCP core with MSVC/CMake over a hand-written ~16.3k-line VCL
   replacement in `src/base/`. It is Win32-targeted and its frontend is useless
   to us, but it is a proven reference for the hardest subsystem.

## Non-negotiable constraints

- **Zero modified copies of WinSCP source in this repo.** Core changes are
  produced by a committed, re-runnable transformation script plus a small patch
  series. See [PLAN.md](PLAN.md#3-upstream-tracking-model).
- **Transformations are line-preserving.** Not an imposed constraint — all 613
  core declarations are already single-line, so this costs nothing. The payoff is
  that `diff -u upstream gen` stays reviewable with zero offsets; it is asserted
  by a test rather than defended at any cost.
- **The C++Builder VCL build must stay green** for as long as practical. This
  keeps upstream merges verifiable and keeps the door open with the maintainer.
- **CI builds Linux and Windows from the first commit.** No "add Windows later".
