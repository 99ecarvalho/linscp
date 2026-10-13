# Plan

## 1. Answers to the four framing questions

### Q1 — Can we compile the Windows version here on Linux and test it?

Two different questions, with two different answers.

**Today's VCL WinSCP: no.** It requires Embarcadero C++Builder 11 (`bcc64`) plus
MSBuild, the VCL runtime, and `.dfm` form compilation. The toolchain is
Windows-only, proprietary, and has no cross-compiler. There is no path to
building the current `WinSCP.groupproj` on Linux, and this will never change.

**The Qt branch: yes.** Verified on this machine:

```
clang++ --target=x86_64-w64-windows-gnu  -fms-extensions  -> x86-64 COFF  OK
clang++ --target=x86_64-pc-windows-msvc  -fms-extensions  -> x86-64 COFF  OK
clang++ --target=x86_64-pc-linux-gnu     -fms-extensions  -> ELF 64-bit   OK
```

All three with `__declspec(property)` emulation working. So one compiler
(clang) covers all targets, which matters because GCC cannot compile the
property shim at all.

**Toolchain decision: clang + mingw-w64 (`*-windows-gnu`), not clang-cl.**
Rationale: no MSVC licence or `xwin` SDK download needed, a single libc++/libstdc++
story, and Debian ships the full `mingw-w64` sysroot in apt. Keep the
`windows-msvc` target as a later option if MSVC ABI interop is ever required.
*Amended 2026-10-13 (ADR 0011):* the target stands, but the toolchain is
llvm-mingw (clang, lld, libc++, UCRT) instead of Debian's GCC sysroot, and Qt
is MSYS2's prebuilt `clang64` build of Debian's Qt version, pinned by a lock
file -- no Qt build of our own.

**Two real gaps, both solvable:**

- `g++-mingw-w64` is in apt but is **useless to us** — GCC rejects
  `__declspec(property)`. We need the mingw-w64 *headers, libs and binutils*
  and then drive them with `clang++ --target=x86_64-w64-mingw32`.
- **Qt6-for-mingw is not packaged in Debian.** Cross-building the Qt GUI for
  Windows therefore needs Qt built once for mingw (a few hours, scriptable and
  cacheable in CI), or MXE. Phases 0–1 are unaffected — they have no Qt
  dependency, so cross-compilation works immediately for core and CLI.

**Testing:** Wine 10.0 is in apt. Good enough for CLI smoke tests and most Qt
GUI behaviour. It is **not** sufficient to certify a Windows release — a real
Windows CI runner is required before shipping binaries.

### Q2 — A Qt branch that generates Windows builds and tracks upstream cheaply

See [§3 Upstream tracking model](#3-upstream-tracking-model). Short version: the
port never contains a modified copy of WinSCP source. A committed script
transforms the pinned submodule into a generated tree at build time.

### Q3 — One codebase, Qt, both platforms

Endorsed as the end state, with one consequence worth stating plainly: Windows
users lose the native-feeling VCL UI in exchange for a Qt one. That is the real
cost of this project and the reason upstream would never undertake it. It also
means C++Builder — a paid, proprietary, Windows-only compiler, and today the
single largest barrier to outside contributors — leaves the toolchain entirely.

### Q4 — "Shims get removed once the maintainer adopts it, and by then the UI must be 100% ported"

This is the one place the plan diverges from the brief. Three corrections.

**(a) Most shims are not scaffolding — they are the permanent portability layer.**
The `UnicodeString` / `TStrings` / `TDateTime` shim is not a temporary adapter;
it *is* the cross-platform RTL. Deleting it means replacing 5,178 `UnicodeString`
and 497 `TStrings` uses across the core with `std::wstring`/`QString` and STL
containers — a second project comparable in size to the first, with no user-visible
benefit and a large regression risk. Recommendation: **keep it permanently.** A
stable, well-tested 16k-line shim nobody has to look at is a feature, not debt.

What *should* eventually be deleted is different and much cheaper:
the VCL frontend, the Windows-only Pascal packages, and the dual-target
`#ifdef __BORLANDC__` branches. That is Phase 4, and it is weeks of work.

**(b) Do not couple cleanup to 100% UI parity.** These are independent axes, and
coupling them creates a trap: you would carry the dual-build burden for an extra
year waiting on the last few preference dialogs. The correct trigger for retiring
the VCL build is *parity on the features users actually depend on*, which lands
well short of all 48 dialogs. Decoupling them also means a stall in UI work never
blocks toolchain cleanup.

**(c) Do not make an architectural decision contingent on a social outcome.**
The maintainer has publicly called a Qt rewrite an "inconceivable task". Plan for
this fork to stand alone and be sustainable indefinitely; treat upstream adoption
as upside. The "keep the VCL build green" discipline in §3 is what maximises the
chance of adoption — but the architecture must not depend on it.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  qt/            Qt 6 frontend            (ours, new)        │
│  vcl-frontend/  existing VCL UI          (upstream, frozen) │
├─────────────────────────────────────────────────────────────┤
│  core/Interface.h — the frontend contract (upstream, as-is)  │
├─────────────────────────────────────────────────────────────┤
│  WinSCP core: SFTP SCP FTP WebDAV S3     (upstream, as-is)  │
│  PuTTY 0.85 · FileZilla FTP engine       (upstream, patched)│
├─────────────────────────────────────────────────────────────┤
│  port/shim/      VCL RTL replacement     (ours, permanent)  │
│  port/platform/  POSIX + Win32 helpers   (ours, permanent)  │
├─────────────────────────────────────────────────────────────┤
│  OpenSSL · expat · neon · libs3          (system packages)  │
└─────────────────────────────────────────────────────────────┘
```

`core/Interface.h` already is a clean abstract frontend contract — free functions
and virtuals (`ShowExtendedException`, `CopyToClipboard`, `ProcessGUI`,
`CreateConfiguration`, `StartThread`). The core never calls the GUI directly.
`ProcessGUI` is virtual on `TSessionUI`, so a headless frontend no-ops it.
Config storage is already abstracted behind `THierarchicalStorage` with both
registry and INI backends; Linux uses the INI path. **We are filling in an
existing seam, not cutting a new one.**

### The dual-target macro — the keystone of the whole plan

The `__property` rewrite expands to *native VCL syntax on C++Builder* and to
`__declspec(property)` everywhere else:

```cpp
#ifdef __BORLANDC__
  #define PROP_RW_GS(T, Name, G, S) __property T Name = { read = G, write = S }
#else
  #define PROP_RW_GS(T, Name, G, S) __declspec(property(get=G, put=S)) T Name
#endif
```

This makes the transformation **non-destructive**: the same transformed header
compiles under C++Builder and under clang. That single property is what lets the
VCL build stay green, what makes the diff theoretically acceptable upstream, and
what keeps the port honest.

Property forms in core, all mechanically rewritable:

| Form | Count |
|---|---|
| `read = FField` | 576 |
| `write = SetX` | 362 |
| `read = GetX` | 246 |
| `write = FField` | 182 |
| `index = …` (4 hand-written wrappers) | 4 |
| `default` / `nodefault` / `stored` | 0 |

591 declarations across core headers; four exceptions. Everything else is a
one-line-in, one-line-out substitution.

Other Embarcadero extensions: `__fastcall` (5,277 sites) becomes an empty
`#define` — zero diff. `__closure` (45 sites) becomes a macro over a
`std::function`-alike. `ARRAYOFCONST` (29) needs a variadic shim. Delphi `Set<>`
appears once in core.

### Win32 call sites in core

Roughly 200 direct Win32 calls across ~40 distinct functions (`CreateFile`,
`ReadFile`, `GetFileAttributes`, `FindFirstFile`, `WSAEventSelect`,
`CreateEvent`, one `RegQueryValueEx`, one `SHGetFolderPath`). Rather than edit
core, implement that subset in `port/platform/win32-on-posix/`. This keeps the
core diff at zero at the cost of a contained, well-understood compatibility
module. `WSAEventSelect` semantics are the awkward part; everything else is
routine.

Stored-password crypto is **not** DPAPI — core uses its own `Cryptography.cpp`,
so it ports cleanly.

---

## 3. Upstream tracking model

**Generated-tree model.** This repo contains zero modified WinSCP source.

```
linscp/
├── 3rd-party/winscp/      submodule, pristine upstream, pinned to a tag
├── port/
│   ├── tools/transform.py committed, re-runnable, line-preserving
│   ├── patches/*.patch    small series for what cannot be mechanised
│   ├── shim/              VCL RTL replacement        (first-class source)
│   └── platform/          POSIX + Win32-on-POSIX     (first-class source)
├── qt/                    Qt 6 frontend              (first-class source)
├── cmake/
└── build/gen/             generated transformed tree (gitignored)
```

Build = `transform.py` reads the submodule → writes `build/gen/` → compile
`build/gen/` + `port/` + `qt/`.

**Updating to a new WinSCP release:**

1. `git -C 3rd-party/winscp fetch && checkout <new tag>`
2. Re-run `transform.py`
3. Re-apply the patch series; fix rejects
4. CI runs the golden-output suite on Linux + Windows

Your diff against upstream is *structurally* empty, so there is nothing to
conflict. The maintenance burden is the script and the patch series, not 77k
lines of forked code. Anything the script cannot express mechanically is a
deliberate decision recorded as a patch — which keeps the patch series small by
construction, because a growing series is an immediate, visible warning sign.

This is exactly what the submodule you asked for is good for: a pristine
upstream reference, not a vendored copy to edit.

**Branches**

| Branch | Role |
|---|---|
| `main` | the port: `port/`, `qt/`, `doc/`, submodule pin |
| `upstream-pin/*` | tags recording which WinSCP commit each release was built against |

No long-lived divergent branch is needed, because there is no forked source to
diverge. This is the structural reason to prefer this model over forking
`winscp/winscp` outright.

---

## 4. Incremental UI strategy

**Do not mix VCL and Qt in one process.** It is technically possible on Windows
and a maintenance disaster, and it is impossible on Linux where VCL does not
exist. "Incremental" must mean something else.

The workable model: **two independent frontends over one ported core.**

- `vcl-frontend/` — the existing UI, untouched, builds only on Windows with
  C++Builder. Stays shippable throughout.
- `qt/` — starts minimal and grows feature by feature on both platforms.

The Qt app becomes the default when it is good enough; the VCL build is retired
in Phase 4. Users are never handed a half-ported UI, and UI work can stop and
restart at any point without breaking anything.

The `.dfm` designer files are an asset here, not an obstacle: each is a
declarative spec of a dialog's layout, widgets, captions and tab order, which
makes `.dfm` → `.ui` conversion unusually tractable and reviewable.

The 121k lines of Pascal component packages are **deleted, not ported** — all
seven map onto functionality Qt already ships:

| Deleted | Replaced by |
|---|---|
| Toolbar2000 + TBX (32k) | `QToolBar` / `QMenuBar` |
| `filemng` shell tree (14k) | `QFileSystemModel` |
| JCL (59k) | utility code; drop |
| PngComponents (4.6k) | Qt native image support |
| `dragndrop` (3.6k) | Qt native drag & drop |

Translations in `translations/` should be reused rather than rewritten; mapping
WinSCP's string tables onto Qt's `tr()`/`.ts` pipeline is a task in its own right
(P3-07) and is worth doing early enough to avoid hardcoded strings accumulating.

---

## 5. Verification strategy

The thing that makes this port tractable to *verify*, not just write:

**Golden-output differential testing.** WinSCP's scripting frontend emits
deterministic XML session logs — the same protocol the `netstandard2.0` .NET
assembly already consumes. So:

1. Build a corpus of scripts covering SFTP/SCP/FTP/FTPS/WebDAV/S3 operations.
2. Run each against **real WinSCP on Windows** → golden XML.
3. Run each against **the port** → candidate XML.
4. Normalise (timestamps, paths, session IDs) and diff.

This gives a regression suite over the core port for a fraction of the cost of
writing unit tests for 77k lines, and it is the single highest-leverage item in
Phase 0. Protocol servers come from containers (OpenSSH, vsftpd, ProFTPD,
Apache/WebDAV, MinIO) so the matrix runs in CI.

Corollary benefit: the existing .NET assembly targets `netstandard2.0` already,
so it will drive the Linux port unchanged once the CLI speaks the same protocol.

---

## 6. Risks

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| R1 | C++Builder rejects `__property` produced by macro expansion, killing the dual-target keystone | **High** | Cheap to test — one header, one person with a licence, Phase 0. Macros expand before parsing and `bcc64` is clang-based, so confidence is high but it is **unverified here**. If it fails, the VCL build cannot stay green and §3's upstream story weakens considerably. Test this first. |
| R2 | FileZilla `WSAAsyncSelect` → epoll rewrite introduces subtle async bugs | **High** | ~1,700 lines of hidden-window message-pump state machine. Golden-output suite + a real FTP server zoo. Budget generously; this is the least mechanical task in Phase 1. |
| R3 | No C++Builder licence and no Windows machine available locally | Medium | Windows CI runner; R1 needs a collaborator or a trial licence |
| R4 | Qt6-for-mingw not packaged; must be built once | Medium | Scripted, cached CI artifact; blocks only Phase 2+ |
| R5 | Qt UI parity is the dominant cost and may stall | Medium | Decouple from Phase 4 per Q4(b); ship a useful subset early |
| R6 | `WSAEventSelect` emulation on POSIX proves awkward | Medium | Core's socket use is mostly mediated by PuTTY/FileZilla; scope is small |
| R7 | Upstream churn outpaces the transform script | Low | Script is declarative and line-preserving; patch-series growth is the early-warning signal |
| R8 | Protocol regressions reach users | Medium | Golden suite is a merge gate, not an afterthought |

**R1 is the project's pivot point.** Everything in §3 — the zero-diff upstream
story, the green VCL build, the plausible path to maintainer adoption — rests on
it. It is also among the cheapest things to test. Do it first, before any shim
code is written.

---

## 7. Phases

| Phase | Outcome | Gate to exit |
|---|---|---|
| **P0** Foundation | Transform script, CMake skeleton, CI on both platforms, golden harness | `core/Common.cpp` compiles on Linux clang; VCL build still green (R1 resolved) |
| **P1** Headless core | Core + CLI on Linux *and* Windows, one codebase | Real SFTP/FTP/S3/WebDAV sessions pass the golden suite on both platforms |
| **P2** Qt shell | Minimal usable GUI client: connect, browse, transfer, queue | A user can do a day's work in it |
| **P3** Qt parity | Commander, sync, editor, dialogs, i18n | Parity on features users depend on |
| **P4** Cleanup | VCL frontend retired, dual-target `#ifdef`s collapsed | Single-toolchain build; C++Builder gone |

P0 targets **Windows first**, because Far-NetBox already proves the de-VCL'd core
compiles for Windows. That separates "does the shim work" from "does POSIX work"
instead of hitting both risks simultaneously.
