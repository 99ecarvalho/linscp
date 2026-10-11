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

Phase 0 is 21 of 32 done; Phase 1 is 8 of 29. Upstream is pinned to the latest
**stable** release, `6.5.8`.

**`source/core/Option.cpp` compiles cleanly against the shim on the Windows
target** — the first real WinSCP core translation unit to build. Across the nine
tracked units the count is **1,220 errors**, down from 2,409, ratcheted in CI so
it can only fall.

The shim now covers `UnicodeString`, the byte-string family with a UTF-8 codec,
`TDateTime`, the Delphi exception hierarchy, `TCriticalSection`,
`TStrings`/`TStringList`, directory search on both Win32 and POSIX, `TClosure`
for `__closure`, the free string/number/path routines, and `TVarRec`/`Format`.
14 test binaries pass on Linux and on Windows under Wine.

The Windows target currently runs ahead of Linux: the same unit still fails there
on Win32 types, which is what `port/platform/win32-on-posix` (P1-07) is for.

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
