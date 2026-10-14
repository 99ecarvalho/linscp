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
| [windows-toolchain.md](windows-toolchain.md) | The Windows build: llvm-mingw, MSYS2's Qt, the pins, running it |
| [windows-vm-hints.md](windows-vm-hints.md) | The Windows test VM: SSH access, what the tests need on it |
| [translations.md](translations.md) | Core's, the shell's and Qt's translations |
| [putty-deltas.md](putty-deltas.md) | WinSCP's PuTTY against stock PuTTY |
| [adr/](adr/README.md) | Architecture decision records |

## Goal

One codebase. Qt UI. Builds and runs on Linux and Windows. No Embarcadero
C++Builder in the toolchain.

## Current state (2026-10-11)

Phase 0 is 25 of 35 done; Phase 1 is 22 of 36. Upstream is pinned to WinSCP's
**`master`** (6.6.4-RC plus three commits), which is being ported to 64-bit
itself, with OpenSSL 3.5.9 and PuTTY 0.85; see the tracker for why `master`
rather than a release.

**Core runs real SFTP sessions on Windows.** Cross-built with clang, core and
every library it uses -- PuTTY, FileZilla and its MFC subset, neon, libs3,
expat, OpenSSL -- link with nothing undefined, and under Wine a test program
connects to an OpenSSH server, authenticates with a key, lists a directory and
downloads a file, once per cipher family. All 33 core units compile for
Windows; on Linux 14 of 33 do, 588 errors remaining, mostly Win32 calls the
POSIX layer does not yet provide and PuTTY's missing Unix platform layer.

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
