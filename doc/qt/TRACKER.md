# Tracker

Last updated: **2026-10-10**

Legend: `todo` · `wip` · `blocked` · `done` · `dropped`

## Progress

| Phase | Done | Total | Gate |
|---|---|---|---|
| P0 Foundation | 15 | 26 | `core/Common.cpp` compiles on Linux clang; VCL build green (R1) |
| P1 Headless core | 1 | 22 | Golden suite green, six protocols, both platforms |
| P2 Qt shell | 0 | 11 | A user can do a day's work in it |
| P3 Qt parity | 0 | 10 | Parity on features users depend on |
| P4 Cleanup | 0 | 4 | Single toolchain; C++Builder gone |
| **Total** | **16** | **73** | |

## Next three actions

1. **P1-01b** — the free string routines, ranked by measured use: `LoadStr`
   (288), `SameText` (113), `Pos` (89), `CutToChar` (70), `IntToStr` (58),
   `Trim` (47), `IncludeTrailingBackslash` (39), `ContainsText` (37).
2. **P1-02** — `TStrings` / `TStringList` (521 uses), the next type by weight.
3. **P0-13** — the golden-output harness. Still the highest-leverage item for
   everything after P1.

## Decisions taken

| Date | Decision | Rationale |
|---|---|---|
| 2026-10-10 | clang everywhere; GCC unsupported | GCC cannot compile `__declspec(property)`; verified locally |
| 2026-10-10 | mingw-w64 (`*-windows-gnu`) over clang-cl/MSVC | No MSVC licence or SDK download; sysroot is in Debian apt |
| 2026-10-10 | Generated-tree model, not a source fork | Keeps the upstream diff structurally empty |
| 2026-10-10 | Keep the VCL RTL shim permanently | De-shimming is a second project of similar size with no user benefit |
| 2026-10-10 | Decouple shim cleanup from 100% UI parity | Coupling them would add ~a year of dual-build burden |
| 2026-10-10 | Two frontends over one core; never mix VCL and Qt in-process | Impossible on Linux, unmaintainable on Windows |
| 2026-10-10 | Windows target first in P0 | Far-NetBox already proves the core compiles for Windows; separates the two risks |
| 2026-10-10 | Line-preserving transform kept, but demoted from "constraint" to asserted property | It costs nothing — all 591 core declarations are already single-line — and its real payoff is a reviewable zero-offset `diff` against upstream, not debugger ergonomics. Asserted by a test; abandon if it ever conflicts with something substantive |
| 2026-10-10 | `index=` properties handled by a `PROP_R_GI` macro rather than a patch | Removes the last 4 manual cases, so the transform covers 613/613 with an empty patch series |
| 2026-10-10 | Far-NetBox used as reference; verbatim reuse deferred pending licence clarification | Its README says GPLv3 but it ships the GPLv2 text (Q-6) |
| 2026-10-10 | Published as [99ecarvalho/linscp](https://github.com/99ecarvalho/linscp), public, GPL-3.0 | `main` is the released state; active work happens on `qt` |
| 2026-10-10 | A PR-able branch against `winscp/winscp` will be **generated**, not hand-maintained | linscp shares no history with upstream, so its `qt` branch is not directly mergeable. Since the port is mechanical, the fork branch can be produced on demand (Q-7). Rehearsed end to end — see facts below |
| 2026-10-10 | Transform must be byte-exact outside property lines | A full-file rewrite (CRLF loss) turned a 613-line change into 76,551. Now asserted by `verify_transform.py` |
| 2026-10-10 | Field/accessor classification is convention-first, parens only as fallback | The parens heuristic false-positives on constructor initialiser lists; WinSCP's `F<Upper>` / `Get*`/`Set*` convention is airtight. Truly unclassifiable names now fail the run instead of producing a wrong-arity macro |
| 2026-10-10 | Windows releases link statically via lld | `-fuse-ld=lld -static -L/usr/x86_64-w64-mingw32/lib`; leaves only KERNEL32 and msvcrt as deps |
| 2026-10-10 | CMake hard-fails on non-clang compilers | A clear configure-time error beats a confusing failure in the first transformed header |
| 2026-10-10 | The transform stamp lives outside the generated tree | `verify_transform.py` treats any extra file as a violation, which is the behaviour we want; the stamp was the first thing it caught |
| 2026-10-10 | **Accepted (Q-8):** `-fshort-wchar` with `std::u16string` storage; `std::wstring` banned in `port/` | Only option leaving upstream's thousands of `L"..."` literals untouched. Enforced by `check_wide_abi.py` and a `static_assert`. ADR 0001 |
| 2026-10-10 | `-fshort-wchar` set globally, not per-target | One translation unit without it is an ABI landmine that links cleanly |
| 2026-10-10 | `UnicodeString` case folding is ASCII-only for now | Delphi's is locale-aware; core uses it on protocol tokens and switch names. Recorded as a known limitation, not silently equivalent |

## Open questions

| # | Question | Blocks | Owner |
|---|---|---|---|
| Q-1 | P3 scope: faithful parity, or a deliberately narrower client? | P3 estimate (swings ~2 years) | — |
| Q-2 | Access to a C++Builder 11 licence + Windows host? | P0-04, P0-15, R1, R3 | — |
| Q-3 | Internal editor, or delegate to the system editor? | P3-05 | — |
| Q-4 | Ship 32-bit Windows, or x86-64 and arm64 only? | P0-10, packaging | — |
| Q-5 | Approach the maintainer early for feedback, or after P1 lands? | social strategy, not architecture | — |
| Q-6 | Far-NetBox licence: README says GPLv3, `LICENSE.txt` is the GPLv2 text | verbatim reuse of its shim (reference use is unaffected) | — |
| Q-7 | Build the fork-branch generator now, or when there is something worth proposing upstream? | nothing today; recommend deferring until P1 lands | — |
| ~~Q-8~~ | ~~Confirm ADR 0001~~ — **resolved 2026-10-10**: accepted | — | — |

## Verified facts (re-check if the environment changes)

| Fact | Evidence | Date |
|---|---|---|
| clang 19 supports `__declspec(property)` on Linux under `-fms-extensions` | Local test: field r/o, field r/w, method+validating setter, read-modify-write, indexed — all pass | 2026-10-10 |
| GCC 14 rejects it outright | `error: 'property' has not been declared` | 2026-10-10 |
| clang cross-emits x86-64 COFF for `windows-gnu` and `windows-msvc` | Local test, property emulation intact | 2026-10-10 |
| Qt6 present on this host; Qt6-for-mingw absent from Debian | `apt-cache search` returns nothing for mingw+qt | 2026-10-10 |
| Wine 10.0 installable from apt | `apt-cache policy wine` | 2026-10-10 |
| Core has 591 `__property` decls; 0 use `default`/`nodefault`/`stored` | grep over `source/core/*.h` | 2026-10-10 |
| Core has no `__try`/`__except` | grep | 2026-10-10 |
| `ConsoleRunner.cpp` has zero `forms/` dependencies | include analysis | 2026-10-10 |
| Stored-password crypto is not DPAPI | no DPAPI symbols in core | 2026-10-10 |
| WinSCP's own compiler is already clang-based | `#ifdef __clang__` in `source/core/Global.h:37` | 2026-10-10 |
| Far-NetBox tracks WinSCP 6.5.6, last commit 2026-07-24, ~16.3k-line VCL shim | repo inspection | 2026-10-10 |
| Core has 613 `__property` declarations across 69 files (`.h` and `.cpp`) | `transform.py --check` | 2026-10-10 |
| 0 of 613 span multiple lines; 0 have comma-bearing or template types | AST-free scan of `core/*.h` | 2026-10-10 |
| All 613 transform to 9 macro forms with **0 unhandled** | `transform.py` | 2026-10-10 |
| All 613 transformed declarations compile under clang | `check_properties.py`, exit 0 | 2026-10-10 |
| Transform is idempotent and line-count-preserving across all 69 files | `transform.py`, second pass is a no-op | 2026-10-10 |
| All 8 property shapes behave correctly at runtime (incl. validating setters, read-modify-write, indexed) | `vcl_property.h` self-test, assertions pass | 2026-10-10 |
| Cross-built Windows PE32+ binary passes the full property suite under Wine | `clang --target=x86_64-w64-mingw32`, exit 0 | 2026-10-10 |
| Static Windows link needs lld plus explicit `-L` for `libwinpthread` | GNU ld fails; lld leaves only KERNEL32 + msvcrt | 2026-10-10 |
| Dynamic Wine runs need **two** DLL dirs on `WINEPATH` | `libwinpthread-1.dll` lives apart from libstdc++/libgcc | 2026-10-10 |
| Importing linscp history into a winscp fork merges with **zero conflicts** | `git merge --allow-unrelated-histories`; 10,602 commits, both roots, all port commits preserved | 2026-10-10 |
| Applied in-tree, the core transform is a **613-line diff across 23 files** | measured on the rehearsal fork | 2026-10-10 |
| After conversion, an upstream release touching a property line conflicts in **1 file**, resolvable mechanically | simulated release; `checkout --theirs` + re-run transform; new upstream property transformed automatically | 2026-10-10 |
| Transform is byte-exact outside property lines; CRLF fully preserved | 76,551 CRLF, 0 bare LF; `Terminal.h` 1003 = 1003 | 2026-10-10 |
| linscp tracks **0** WinSCP source files; only the submodule gitlink | `git ls-files` | 2026-10-10 |
| Both targets build and pass 4/4 tests; Windows runs under Wine via CTest | `ctest` on `build/linux` and `build/win` | 2026-10-10 |
| VCL surface the core needs: ~45 real types, dominated by `UnicodeString` (6,248), `TStrings` (521), `Exception` (438), `TDateTime` (250) | `census.py`, resolved against real mingw/OpenSSL/stdlib headers | 2026-10-10 |
| Core's Win32 headers (`shlobj`, `psapi`, `winioctl`…) are satisfied free on the mingw target | only VCL needs shimming for Windows | 2026-10-10 |
| `-fshort-wchar` yields `sizeof(wchar_t) == 2` on both linux-gnu and mingw32 | `static_assert` compiled for both targets | 2026-10-10 |
| `char_traits<wchar_t>` really does call `wmemcmp`/`wcslen`/`wmemchr`; `char_traits<char16_t>` does not | `/usr/include/c++/14/bits/char_traits.h:512,522,534` | 2026-10-10 |
| `UnicodeString` API scoped from measured use: `IsEmpty` 594, `Length` 444, `c_str` 247, `SubString` 184, `Pos` 86, `Delete` 79 | grep over the generated core | 2026-10-10 |
| 6/6 tests pass on Linux and on Windows under Wine, including `UnicodeString` | `ctest` both build trees | 2026-10-10 |

## Task status

### P0 — Foundation
| ID | Task | Status | Note |
|---|---|---|---|
| P0-01 | Repo layout | done | `port/{shim,platform,tools,patches}`, `qt/`, `cmake/` |
| P0-02 | Pin submodule to a release tag | todo | currently a `master` commit |
| P0-03 | Dual-target property macro header | done | `port/shim/vcl_property.h`; 9 forms, self-tested |
| P0-04 | Verify C++Builder accepts the macro | blocked | Q-2: needs licence + Windows |
| P0-05 | `transform.py` property rewriter | done | 613/613, 0 unhandled |
| P0-06 | `transform.py` fastcall/closure/ARRAYOFCONST | todo | properties done; these three not started |
| P0-07 | Transform idempotence tests | done | idempotent + line-count parity asserted |
| P0-08 | CMake skeleton | done | transform+verify wired as a build step; CTest on both targets |
| P0-09 | Minimal shim for `core/Common.cpp` | todo | scope now measured by P0-24; blocked on Q-8 |
| P0-10 | mingw-w64 sysroot + clang toolchain file | done | `cmake/toolchain-mingw-w64.cmake`; clang + lld + Wine emulator |
| P0-11 | CI matrix Linux + Windows cross | todo | |
| P0-12 | Wine smoke tests in CI | wip | wired into CTest via `CMAKE_CROSSCOMPILING_EMULATOR`; GitHub Actions pending |
| P0-13 | Golden-output harness | todo | highest leverage in P0 |
| P0-14 | Protocol server containers | todo | |
| P0-15 | Capture golden corpus from real WinSCP | blocked | Q-2: needs Windows host |
| P0-16 | ADR log | wip | `doc/qt/adr/` started with ADR 0001 |
| P0-17 | Property compile-check harness | done | `check_properties.py`, 613 declarations, clang exit 0 |
| P0-18 | HTML render of plan docs | done | `doc/qt/plan.html` via `build-html.sh` |
| P0-19 | Publish repository, licence, root README | done | public at `99ecarvalho/linscp`; work on `qt` |
| P0-20 | Fork-branch generator | todo | deferred to P1-19 (Q-7); rehearsed manually in P0-22 |
| P0-21 | `verify_transform.py` upstream-diff audit | done | asserts line parity, byte-exactness and 1 macro per changed line |
| P0-22 | Rehearse history import into a winscp fork | done | zero-conflict merge; 613-line in-tree diff; mechanical conflict resolution |
| P0-23 | `check_docs.py` TASKS/TRACKER consistency | done | found 3 drifts on first run |
| P0-24 | VCL surface census | done | ~45 types; `build/shim_spec.txt` |
| P0-25 | ADR 0001 character width | done | accepted: `-fshort-wchar` + `std::u16string` |
| P0-26 | `check_wide_abi.py` ADR enforcement | done | `shim_no_wide_abi` test |

### P1 — Headless core
| ID | Task | Status | Note |
|---|---|---|---|
| P1-01 | Shim: `UnicodeString` class | done | 1-based API, UTF-16, 9 test groups incl. surrogate pairs |
| P1-01b | Shim: free string routines | todo | next; ranked by measured use |
| P1-02 | Shim: `TObject`/`TList`/`TStrings` | todo | |
| P1-03 | Shim: `TDateTime`/`SysUtils`/`IOUtils` | todo | |
| P1-04 | Shim: exceptions | todo | |
| P1-05 | Shim: threading/sync | todo | |
| P1-06 | Shim: streams/INI/registry | todo | |
| P1-07 | win32-on-posix subset | todo | |
| P1-08 | Socket-event emulation | todo | R6 |
| P1-09 | PuTTY unix backend re-import | todo | |
| P1-10 | PuTTY delta reconciliation | todo | |
| P1-11 | FileZilla MFC subset replacement | todo | |
| P1-12 | FileZilla epoll rewrite | todo | **R2** — budget generously |
| P1-13 | System libs | todo | |
| P1-14 | Drop `puttyvs` | todo | |
| P1-15 | Console IPC → stdio | todo | |
| P1-16 | Headless `Interface.h` frontend | todo | |
| P1-17 | First SFTP session on Linux | todo | long tail expected |
| P1-18 | Windows cross-build + Wine | todo | |
| P1-19 | Golden suite green, both platforms | todo | **P1 gate** |
| P1-20 | .NET assembly drives Linux CLI | todo | |
| P1-21 | `#ifdef` out Windows-only subsystems | todo | |

### P2 — Qt shell
| ID | Task | Status | Note |
|---|---|---|---|
| P2-01 | Qt6 for mingw | todo | R4 |
| P2-02 | Qt app skeleton | todo | |
| P2-03 | Qt `Interface.h` implementation | todo | |
| P2-04 | Site manager | todo | |
| P2-05 | Login/auth dialogs | todo | |
| P2-06 | Remote file panel | todo | |
| P2-07 | Transfer + queue UI | todo | |
| P2-08 | Local file panel | todo | |
| P2-09 | i18n pipeline | todo | do early |
| P2-10 | Packaging | todo | |
| P2-11 | P2 exit gate | todo | |

### P3 — Qt parity
| ID | Task | Status | Note |
|---|---|---|---|
| P3-01 | `.dfm` → `.ui` converter | todo | |
| P3-02 | Commander dual-pane | todo | the main draw |
| P3-03 | Synchronization UI | todo | |
| P3-04 | Preferences | todo | XL — split first |
| P3-05 | Internal editor | todo | Q-3 |
| P3-06 | Remaining 48 dialogs, batched | todo | XL — split into P3-06a…f |
| P3-07 | Drag & drop | todo | |
| P3-08 | Shell integration | todo | |
| P3-09 | Accessibility | todo | |
| P3-10 | P3 exit gate | todo | |

### P4 — Cleanup
| ID | Task | Status | Note |
|---|---|---|---|
| P4-01 | Retire VCL frontend | todo | |
| P4-02 | Collapse dual-target `#ifdef`s | todo | |
| P4-03 | Fold generated tree into source | todo | optional, one-way door |
| P4-04 | De-shimming evaluation | todo | recommendation: do not |
