# Tasks

Stable IDs. Never renumber — append. Status lives in [TRACKER.md](TRACKER.md).

Size: **S** ≤ 2 days · **M** ≤ 1 week · **L** ≤ 1 month · **XL** > 1 month (split before starting)

---

## P0 — Foundation & proof

Goal: prove the keystone works, with no behaviour change anywhere.

| ID | Task | Size | Depends | Notes |
|---|---|---|---|---|
| P0-01 | Repo layout: `port/`, `qt/`, `cmake/`, `build/gen/` gitignored | S | — | Per PLAN §3 |
| P0-02 | Pin submodule to a release tag, not `master` | S | P0-01 | Currently a `master` commit |
| P0-03 | **Dual-target property macro header** (`port/shim/vcl_property.h`) | S | — | The keystone; PLAN §2 |
| P0-04 | **Verify C++Builder accepts macro-expanded `__property`** | S | P0-03 | **R1. Do this first.** Needs a licence + Windows. Blocks the zero-diff story |
| P0-05 | `transform.py`: `__property` rewriter, line-preserving | M | P0-03 | 591 decls, 4 forms + 4 manual |
| P0-06 | `transform.py`: `__fastcall`, `__closure`, `ARRAYOFCONST` | S | P0-05 | `__fastcall` → empty define |
| P0-07 | Transform idempotence + line-count assertion tests | S | P0-05 | Guards against silent drift |
| P0-08 | CMake skeleton: generate → compile one TU | M | P0-01, P0-05 | |
| P0-09 | Minimal shim to compile `core/Common.cpp` on Linux clang | M | P0-08 | The real measurement of shim cost |
| P0-10 | Install + script mingw-w64 sysroot; clang `--target` toolchain file | S | P0-08 | GCC is unusable — clang only |
| P0-11 | CI matrix: Linux native + Windows cross, both clang | M | P0-10 | From the first commit |
| P0-12 | Wine in CI for Windows smoke tests | S | P0-11 | Not release certification |
| P0-13 | Golden-output harness: runner, XML normaliser, differ | M | — | PLAN §5; highest leverage in P0 |
| P0-14 | Protocol server containers (OpenSSH, vsftpd, ProFTPD, WebDAV, MinIO) | M | P0-13 | |
| P0-15 | Capture golden XML corpus from real WinSCP on Windows | M | P0-13, P0-14 | Needs a Windows host |
| P0-16 | ADR log (`doc/qt/adr/`) | S | — | Record toolchain/ABI choices as made |
| P0-17 | `check_properties.py`: compile-test every transformed declaration | S | P0-05 | Validates macro arity across the real corpus without the full shim |
| P0-18 | HTML render of the plan docs (`build-html.sh`) | S | — | pandoc, self-contained, light/dark |
| P0-19 | Publish repository with licence and root README | S | — | Public, GPL-3.0; `main` released, `qt` active |
| P0-20 | Generator for a PR-able branch against a `winscp/winscp` fork | M | P1-19 | Defer until there is something worth proposing (Q-7) |
| P0-21 | `verify_transform.py`: audit the generated tree against upstream | S | P0-05 | Byte-exactness is the invariant the whole model rests on |
| P0-22 | Rehearse history import into a `winscp/winscp` fork | S | P0-05 | Validates Q-7 and measures the real conflict cost |
| P0-23 | `check_docs.py`: keep TASKS and TRACKER consistent | S | — | Enforces the tracker rule mechanically rather than by discipline |

## P1 — Headless core on both platforms

Goal: one codebase, core + CLI, Linux and Windows. No GUI.

| ID | Task | Size | Depends | Notes |
|---|---|---|---|---|
| P1-01 | Shim: `UnicodeString` + string routines | L | P0-09 | 5,178 uses; crib from Far-NetBox `src/base/UnicodeString.hpp` |
| P1-02 | Shim: `TObject`, `TList`, `TStrings`, `TStringList`, `Contnrs` | L | P1-01 | 497 `TStrings` uses |
| P1-03 | Shim: `TDateTime`, `DateUtils`, `SysUtils`, `IOUtils`, `StrUtils` | L | P1-01 | |
| P1-04 | Shim: `Exception` hierarchy + Delphi exception semantics | M | P1-01 | Core has no `__try`/`__except` — simplifies this |
| P1-05 | Shim: `TCriticalSection`, `TThread`, `SyncObjs` | M | P1-02 | `StartThread` is already abstracted in `Interface.h` |
| P1-06 | Shim: `TStream`, `TMemIniFile`; `TRegistry` → file-backed | M | P1-02 | INI backend already exists in core |
| P1-07 | `port/platform/win32-on-posix`: ~40 functions | L | P1-06 | Keeps core diff at zero |
| P1-08 | `WSAEventSelect` / socket-event emulation | M | P1-07 | R6 |
| P1-09 | PuTTY: re-import upstream `unix/` backend | M | P1-03 | Upstream maintains it; reconcile WinSCP's patches |
| P1-10 | PuTTY: reconcile WinSCP deltas against vanilla 0.85 | M | P1-09 | |
| P1-11 | FileZilla: replace vendored MFC subset (`CString`/`CFile`/`CTime`) | M | P1-01 | Only 4 files in `libs/mfc` |
| P1-12 | **FileZilla: `WSAAsyncSelect` hidden-window pump → epoll/poll** | L | P1-11 | **R2.** ~1,700 lines; least mechanical task in P1 |
| P1-13 | Switch `libs/` to system OpenSSL, expat, neon, libs3 | M | P0-08 | All vanilla upstream already |
| P1-14 | Drop `puttyvs` (MSVC wrapper, obsolete) | S | P1-09 | |
| P1-15 | `winscp.com` shared-memory IPC → plain stdio | M | P1-06 | Both platforms |
| P1-16 | Headless frontend implementing `Interface.h` | L | P1-05, P1-15 | Derive from `ConsoleRunner.cpp` — it has zero `forms/` deps |
| P1-17 | First link + first real SFTP session on Linux | M | P1-16 | Expect a long tail of link/ABI issues |
| P1-18 | Same core + CLI cross-built for Windows, Wine-smoked | M | P1-17, P0-12 | Proves "one codebase" |
| P1-19 | Golden suite green across all six protocols, both platforms | L | P1-18, P0-15 | **P1 exit gate** |
| P1-20 | Verify stock `netstandard2.0` .NET assembly drives the Linux CLI | S | P1-19 | Should work unchanged |
| P1-21 | `#ifdef` out `dragext`, `Setup.cpp`, jump lists, file associations | M | P1-16 | Windows-only or dropped |

## P2 — Qt shell: minimal usable client

| ID | Task | Size | Depends | Notes |
|---|---|---|---|---|
| P2-01 | Build Qt6 for mingw; cache as CI artifact | M | P0-10 | R4; not in Debian repos |
| P2-02 | Qt app skeleton, CMake integration, both platforms | M | P2-01, P1-19 | |
| P2-03 | Qt implementation of the `Interface.h` contract | L | P2-02 | Prompts, errors, progress, busy, `ProcessGUI` |
| P2-04 | Session storage + site manager UI | L | P2-03 | INI-backed |
| P2-05 | Login / authentication dialogs (incl. host key, keyboard-interactive) | M | P2-03 | |
| P2-06 | Remote file panel: model/view, columns, sorting | L | P2-03 | `QAbstractItemModel` over `TRemoteFileList` |
| P2-07 | Transfer engine wiring + queue UI | L | P2-06 | Over `TTerminalQueue` |
| P2-08 | Local file panel | M | P2-06 | `QFileSystemModel` |
| P2-09 | i18n pipeline: WinSCP string tables → Qt `.ts` | M | P2-03 | Do early — avoids hardcoded strings |
| P2-10 | Packaging: AppImage/Flatpak + Windows installer | M | P2-07 | |
| P2-11 | **P2 exit gate:** a user can do a day's work in it | S | P2-07, P2-10 | |

## P3 — Qt parity (incremental, independently shippable)

| ID | Task | Size | Depends | Notes |
|---|---|---|---|---|
| P3-01 | `.dfm` → `.ui` assisted converter | M | P2-02 | `.dfm` is a declarative layout spec — very tractable |
| P3-02 | Commander dual-pane: keyboard semantics, inline rename, custom columns | L | P2-08 | The main draw for most users |
| P3-03 | Synchronization UI + `TSynchronizeChecklist` view | L | P2-07 | |
| P3-04 | Preferences dialogs (largest single cluster) | XL → split | P3-01 | Split per page before starting |
| P3-05 | Internal editor | L | P2-06 | Or delegate to `$EDITOR` / system editor |
| P3-06 | Remaining dialogs, batched in groups of ~8 of the 48 | XL → split | P3-01 | Track as P3-06a…f |
| P3-07 | Drag & drop: Qt native + XDS on Linux | M | P3-02 | |
| P3-08 | Shell integration: "open in terminal", file associations | M | P3-02 | Per-platform |
| P3-09 | Accessibility pass | M | P3-06 | |
| P3-10 | **P3 exit gate:** parity on features users depend on | S | P3-02, P3-03, P3-04 | Explicitly *not* 100% of 48 dialogs |

## P4 — Cleanup

| ID | Task | Size | Depends | Notes |
|---|---|---|---|---|
| P4-01 | Retire the VCL frontend; drop Pascal packages | M | P3-10 | 176k lines deleted |
| P4-02 | Collapse `#ifdef __BORLANDC__` dual-target branches | M | P4-01 | Simplifies the transform script |
| P4-03 | Fold the transform output into first-class source (optional) | L | P4-02 | Only once upstream tracking is no longer a goal — **one-way door** |
| P4-04 | Evaluate full de-shimming | S | P4-02 | **Recommendation: do not.** PLAN §1 Q4(a) — write the evaluation down and close it |
