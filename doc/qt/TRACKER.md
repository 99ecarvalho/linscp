# Tracker

Last updated: **2026-10-11**

Legend: `todo` · `wip` · `blocked` · `done` · `dropped`

## Progress

| Phase | Done | Total | Gate |
|---|---|---|---|
| P0 Foundation | 25 | 35 | `core/Common.cpp` compiles on Linux clang; VCL build green (R1) |
| P1 Headless core | 19 | 36 | Golden suite green, six protocols, both platforms |
| P2 Qt shell | 0 | 11 | A user can do a day's work in it |
| P3 Qt parity | 0 | 10 | Parity on features users depend on |
| P4 Cleanup | 0 | 4 | Single toolchain; C++Builder gone |
| **Total** | **44** | **95** | |

## Next three actions

Driven by `compile_census.py` rather than by grep counts — the compiler names
what is actually blocking each unit, in dependency order.

**Eight of ten tracked units compile cleanly for Windows, seven for Linux**,
nothing aborting on either, measured in upstream's own preprocessor
configuration. Census: **146 errors on mingw, 290 on Linux.**

| Unit | mingw | Linux |
|---|---|---|
| `Bookmarks`, `CopyParam`, `FileBuffer`, `FileMasks`, `NamedObjs`, `Option`, `Queue` | 0 | 0 |
| `Configuration.cpp` | **0** | 67 |
| `Terminal.cpp` | 46 | 70 |
| `Common.cpp` | 100 | 153 |

`Configuration.cpp`'s 67 Linux errors are not shim work in the usual sense:
**55 come from PuTTY headers** -- `putty/windows/platform.h` and the Windows
GSSAPI declarations -- because WinSCP vendors only PuTTY's Windows platform
layer. The rest are Win32 shell-folder and message constants.

1. **P1-09**: PuTTY's Unix platform layer. It removes the bulk of the Linux
   excess in every unit that includes `PuttyIntf.h`.
2. `Terminal.cpp` (46 / 70).
3. `Common.cpp` (100 / 153).

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
| 2026-10-11 | Compute cost recorded as measured token usage, not an estimate | The branch so far cost $297 of Opus 5 inference (99% of tokens are cache reads). Confirms compute is not the constraint, and shows Opus 5.5 would halve it — see [ESTIMATES.md](ESTIMATES.md) |
| 2026-10-11 | VCL's `TThread` class not shimmed | Its only user, `KeyGen.cpp`, is in none of upstream's `.cbproj` files and `TKeyGenerator` is referenced nowhere else — dead code upstream. Core's own `TSimpleThread` uses `StartThread` and Win32 directly |
| 2026-10-11 | P1-05b scoped as Win32 kernel objects on POSIX, plus Delphi `BeginThread`/`EndThread` on both targets | That is what core actually calls: 28 `CloseHandle`, 6 waits, 5 `CreateEvent`, 10 `GetCurrentThreadId`. `BeginThread` is written once over `CreateThread`, so upstream's frontend `StartThread` ports unchanged |
| 2026-10-11 | POSIX kernel objects share one handle table, one lock and one condition variable; handle values are never reused | Makes `WaitForMultipleObjects` over mixed threads and events — including atomic `WaitAll` — correct by construction. Core has a handful of threads, so the coarse lock costs nothing measurable. Never reusing values turns use-after-close into `ERROR_INVALID_HANDLE` |
| 2026-10-11 | Shim XML interfaces live in `Xml::Xmlintf`; only the `_di_` aliases are global | Embarcadero's own layout. At global scope ours collided on Windows with the COM `IXMLDocument` from `<msxml.h>`. Core names them only through `_di_` aliases and `Xmlintf::` |
| 2026-10-11 | Closure-site discovery compiles a separately transformed, *unbound* tree | Compiling the ordinary tree saw the known sites already wrapped and "rediscovered" only new ones — it cut 62 sites to 2. Now idempotent: a second run is byte-identical |
| 2026-10-11 | VCL's implicit `SysUtils::LoadStr` and `Vcl.Menus::StripHotkey` supplied by the shim, not by patching core | Core receives both through `<vcl.h>` and never declares them. `LoadStr` is built on the existing `LoadString` table, so there is still one resource path |
| 2026-10-11 | Core is compiled with upstream's own preprocessor configuration, read from `ScpCore.cbproj` by `port/tools/upstream_defines.py` | Copying the list would drift. Applied by the census, closure discovery and CMake alike, shim included, because `UNICODE` decides which Win32 A/W function every macro names. Windows-only names are dropped on POSIX; `_CRTIMP=` is dropped everywhere, as C++Builder's CRT linkage |
| 2026-10-11 | Census and discovery build the Debug configuration | Its `DebugAssert` expressions are code that must compile too; Release compiles them out, which hid ten closure comparisons |
| 2026-10-11 | `port/compat` withdrawn; supersedes the 2026-10-10 decision creating it | `help.h` was compensating for the missing `WINSCP` define, not a gap in upstream: with `WINSCP` set, PuTTY's `platform.h` never includes it |
| 2026-10-10 | A PR-able branch against `winscp/winscp` will be **generated**, not hand-maintained | linscp shares no history with upstream, so its `qt` branch is not directly mergeable. Since the port is mechanical, the fork branch can be produced on demand (Q-7). Rehearsed end to end — see facts below |
| 2026-10-10 | Transform must be byte-exact outside property lines | A full-file rewrite (CRLF loss) turned a 613-line change into 76,551. Now asserted by `verify_transform.py` |
| 2026-10-10 | Field/accessor classification is convention-first, parens only as fallback | The parens heuristic false-positives on constructor initialiser lists; WinSCP's `F<Upper>` / `Get*`/`Set*` convention is airtight. Truly unclassifiable names now fail the run instead of producing a wrong-arity macro |
| 2026-10-10 | Windows releases link statically via lld | `-fuse-ld=lld -static -L/usr/x86_64-w64-mingw32/lib`; leaves only KERNEL32 and msvcrt as deps |
| 2026-10-10 | CMake hard-fails on non-clang compilers | A clear configure-time error beats a confusing failure in the first transformed header |
| 2026-10-10 | The transform stamp lives outside the generated tree | `verify_transform.py` treats any extra file as a violation, which is the behaviour we want; the stamp was the first thing it caught |
| 2026-10-10 | **Accepted (Q-8):** `-fshort-wchar` with `std::u16string` storage; `std::wstring` banned in `port/` | Only option leaving upstream's thousands of `L"..."` literals untouched. Enforced by `check_wide_abi.py` and a `static_assert`. ADR 0001 |
| 2026-10-10 | `-fshort-wchar` set globally, not per-target | One translation unit without it is an ABI landmine that links cleanly |
| 2026-10-10 | `UnicodeString` case folding is ASCII-only for now | Delphi's is locale-aware; core uses it on protocol tokens and switch names. Recorded as a known limitation, not silently equivalent |
| 2026-10-10 | `vcl.h` transitively exposes the Win32 API | Matches C++Builder, which upstream's `CorePCH.h` depends on: it includes `<vcl.h>` and core's headers before `<Winapi.Windows.hpp>`. Fixing this alone removed 9 of 24 missing types |
| 2026-10-10 | UTF-8 decoding substitutes one U+FFFD per maximal subpart | Unicode's recommended practice. The first implementation emitted one per byte; a test on truncated input caught it |
| 2026-10-10 | Byte strings keep C++Builder's code-page template shape | Core relies on `RawByteString` and `UTF8String` being distinct types; collapsing them would let byte reinterpretation happen silently |
| 2026-10-10 | Date and time are combined only through `ComposeDateTime` | Delphi keeps the time of day positive for negative dates, so `Date + Time` lands a day late at the mirrored time. Correct for file timestamps, wrong in general and wrong silently |
| 2026-10-10 | `TCriticalSection` wraps `std::recursive_mutex` | Delphi's is re-entrant; a plain `std::mutex` would deadlock on core's re-entrant paths rather than fail a test |
| 2026-10-10 | Compile-census numbers are ratcheted in CI | No core TU compiles cleanly yet, so a reintroduced missing type would otherwise present as a passing build |
| 2026-10-10 | `TStrings` is 0-based while `UnicodeString` is 1-based | Both match their Delphi originals. Two bases in one codebase is a hazard, so each is pinned by a boundary test rather than left to memory |
| 2026-10-10 | `TStrings` references objects, never owns them | Delphi does not free them and core owns them elsewhere; freeing here would be a double-free. Asserted by a test that outlives the list |
| 2026-10-10 | Chained subscripts on indexed properties are unsupported | clang reads `Prop[i][j]` as a two-argument getter call. Core has 0 occurrences; `check_properties.py` fails if an upstream change adds one |
| 2026-10-10 | Win32 A/W name macros are undone in `vcl_win32_undef.h` | `<windows.h>` renamed the *definition* of `ReplaceText` to `ReplaceTextW` while the header still declared `ReplaceText`. Both compiled; only the link failed, and only because a test called it. A link-time guard now references every colliding name |
| 2026-10-10 | `ARRAYOFCONST` pastes its parenthesised list onto a variadic call | C++Builder builds an open array of `TVarRec`, which needs compound literals. `ARRAYOFCONST((a, b))` becoming `MakeFormatArgs(a, b)` is ordinary C++ |
| 2026-10-11 | XML is parsed for real, using the expat upstream vendors, rather than stubbed | A stub would silently import no FileZilla sessions. The mingw sysroot has no expat, so the vendored source is compiled for both targets, which also keeps them on one version |
| 2026-10-11 | Wide-character functions are renamed and the CRT names redirected by macro | They cannot be redeclared -- `<string>` pulls in `<cwchar>`, whose `wcschr` differs from ours only in return type -- and a macro also guarantees core's calls reach ours rather than glibc's |
| 2026-10-11 | `towlower`/`towupper` fold ASCII only, on both platforms | The Windows CRT folds only ASCII in the default locale, confirmed by running the test against it. Folding Latin-1 on POSIX alone would make the platforms disagree, which is worse than being narrow |
| 2026-10-11 | `try`/`__finally` uses the same lambda expansion on *both* targets | SEH would work on mingw, but one code path on both platforms is worth more than relying on it, and it removes a way for the two to diverge in behaviour |
| 2026-10-11 | The `try`/`__finally` transform measures brace depth on the *pristine* lines and applies every edit afterwards | `LINSCP_END_FINALLY` replaces a `}` with text containing no brace, so editing in place miscounted the depth of any enclosing block -- and core nests these |
| 2026-10-11 | One bare `return` inside a guarded body is permitted, by checked condition | Returning from the lambda runs the cleanup through the guard and then falls off the end of the function, which is what Borland does -- but only when the block ends the function and the return carries no value. Both are verified rather than assumed |
| 2026-10-11 | Every property form routes through a generated public wrapper | Borland's `__property` grants access to a private accessor from outside the class and ignores constness; `__declspec(property)` does neither, because it expands to an ordinary member call at the use site. The wrappers restore both behaviours, and the getter casts its result to the declared type because C++Builder is lax where the two differ in constness |
| 2026-10-11 | `UnicodeString` accepts numbers implicitly, yielding their decimal text | C++Builder does, and core depends on it. The hazard is real -- `UnicodeString s = 0;` gives `"0"` -- but diverging would mean editing core |
| 2026-10-11 | Globals that need dynamic initialisation are declared *before* the object that initialises them | Within a translation unit, dynamic initialisation runs in declaration order. Putting the initialiser first had its constructor assign to a `UnicodeString` that was not yet constructed, which segfaulted rather than failing to compile |
| 2026-10-11 | `TMask` matching is case-insensitive | Delphi's is, and core uses it for file masks users type without regard to case |
| 2026-10-11 | Closure call sites are **compiler-discovered, committed, then replayed** | The sites cannot be inferred by regex: whether an argument is a closure depends on the callee's signature. clang names each one exactly, so discovery records them to `port/patches/closure_sites.json` and `transform.py` replays that list, failing loudly if a recorded site no longer holds the expected identifier |
| 2026-10-11 | A diagnostic inside a macro expansion is resolved to the macro *definition* | clang reports it at the expansion point, so `SessionInfo.cpp`'s `ADSTR` macro alone produced 61 duplicate reports. Binding the definition once fixes every call site, and cut the work from 118 sites to 62 |
| 2026-10-11 | `TRegistry` on POSIX reports every key absent rather than empty | There is no registry, and returning defaults for settings a user had actually set would look to them like their configuration had vanished. The INI backend, which core already supports, is the correct one there (P1-06d) |
| 2026-10-11 | INI section and key names match case-insensitively | Delphi's do, and files written by WinSCP on Windows have inconsistent casing |
| 2026-10-11 | INI sections and keys keep their file order | Sorting them would churn the user's configuration on every save and make diffs useless |
| 2026-10-11 | `DynamicArray` shares its buffer on copy, as Delphi's does | Core declares `TSafeHandleStream::Read` as taking one *by value* and fills it for the caller. A deep-copying version would compile and silently return no data |
| 2026-10-11 | `win32_on_posix.h` exposes `<string.h>`, `<stdlib.h>` and `<stdio.h>` | `<windows.h>` does transitively, and core calls `strlen`, `strcmp` and `memmove` unqualified |
| 2026-10-10 | **Resource strings: supply Win32 `LoadString`, not a `.res` loader, and do not patch core** | core's `LoadStr` is already implemented, in terms of `HInstance`, `LibModuleList` and Win32 `LoadString`. Supplying those three means no `.res` binary format to parse, no core edits, and a purely additive change — the kind upstream could take rather than a fork of its own code. The table is generated from upstream's `.rc` files, so the strings track the submodule automatically |
| 2026-10-10 | `UnicodeString::c_str()` has a non-const overload returning a writable buffer | C++Builder's does, and core depends on it: `DoLoadStrFrom` calls `SetLength` and then passes `c_str()` to `LoadString` as the destination to fill |
| 2026-10-10 | `TList::Notify` is virtual and fires before a removal, after an addition | Core overrides it. `TObjectList` frees owned objects from inside it, so a wrong `Action` means a leak or a double free, neither of which announces itself |
| 2026-10-10 | `Extract` reports `lnExtracted`, never `lnDeleted` | That is how Delphi lets a caller take ownership back without the list freeing the object |
| 2026-10-10 | `TList::Sort` is stable, where Delphi's quicksort is not | Relying on an unstable order would be a latent caller bug either way, and stable is the safer guarantee to provide |
| 2026-10-10 | Derived streams re-expose the base's legacy `Seek` with a using-declaration | Overriding one overload hides the other. Core calls both on derived streams, so without this the legacy origins would stop resolving |
| 2026-10-10 | Stream exceptions follow Delphi's shape exactly | `EFilerError`, `EFOpenError` and `EFCreateError` are siblings under `EStreamError`, with `EReadError`/`EWriteError` beneath `EFilerError`. Core catches at both levels |
| 2026-10-10 | `port/platform` headers refuse to compile on Windows, and are POSIX-only on the include path | Putting them on a Windows target shadowed the genuine mingw headers, which presented as a 1,220 -> 783 "improvement". The `#error` makes that mistake loud |
| 2026-10-10 | `port/compat` is separate from `port/platform` | `help.h` is missing from upstream itself rather than being a platform difference, so it is needed on every target |
| 2026-10-10 | The ratchet records whether a unit aborted | An aborted parse reports a floor. Treating that as a distinct state stops a new abort from reading as sudden progress |
| 2026-10-10 | `Format` keeps malformed input visible | Delphi raises on a missing argument. Core routes user- and server-supplied text through these into logs, so an unknown specifier is echoed and a missing argument renders as `<missing>` rather than throwing from a log path |

## Correction: census figures before 2026-10-11 used the wrong configuration

Until `upstream_defines.py`, every compile of core here used **none** of the
defines upstream builds it with -- not `WINSCP`, not `UNICODE`, not `_DEBUG`.
So the census measured a configuration upstream never builds. Two effects:

- Code upstream compiles was hidden. `_DEBUG` alone exposed ten closure
  comparisons in `Queue.cpp`, which had been reported clean; they needed
  Borland closure *equality*, which did not exist.
- Workarounds were built for problems that were not real: `port/compat/help.h`
  stood in for a header that `WINSCP` compiles out.

It also surfaced a shim bug that only `_DEBUG` reaches: the POSIX `TEXT()` was
one-level, so `TEXT(__FILE__)` pasted the token into `L__FILE__`.

The corrected census is **159 / 303**. Earlier figures are not comparable with
it; the trend they showed was real, the absolute numbers were not.

## Correction: the earlier "5 errors left" figure was wrong

Recorded because the mistake is instructive, not just to log it.

`compile_census.py` filtered clang diagnostics on `': error:'`, which does not
match `': fatal error:'`. A missing header emits only a *fatal* error and aborts
the parse, so a translation unit that never got past its first include was
reported as having **zero** problems. The progression logged earlier for
`Common.cpp` (109 -> 83 -> 70 -> 30 -> 13 -> 5) was real for the portion that
parsed, but it was a floor, not a total: the parse was stopping at
`#include <Classes.hpp>`.

Two fixes. The census now counts fatal errors and prints `ABORTED EARLY` with the
cause, so an aborted unit can never again look like a clean one. And
`port/shim` now stubs the unqualified legacy RTL header names (`Classes.hpp`,
`SysUtils.hpp`, `Contnrs.hpp`, ...) alongside the `System.*` spellings, since
core uses both.

The honest figure for `Common.cpp` is **417 errors across 29 missing types**, and
the ratchet now covers nine translation units rather than one, precisely so a
single unit cannot give a falsely rosy picture again.

## Known divergences from Delphi

Recorded deliberately, so they are findable when something behaves oddly.

| Area | Divergence | Why |
|---|---|---|
| `UnicodeString::LowerCase` / `CompareIC` | ASCII-only case folding | Delphi's is locale-aware. Core uses these on protocol tokens and switch names. Must not be relied on for user-facing text |
| `FindFirst` pattern matching | Case-sensitive on Linux, case-insensitive on Windows | Each matches its host filesystem. A pattern that matches on Windows may not on Linux |
| `faHidden` on Linux | Set from a leading dot in the name | POSIX has no hidden attribute; the dot convention is what users expect |
| `TFormatSettings` defaults | ISO-8601 rather than system locale | Deterministic across machines. Anything user-facing should set them explicitly |
| Null arguments to the Win32 time functions | Rejected on POSIX; real Win32 dereferences and faults | The hardening is deliberate, so the null-argument assertions run only on POSIX |
| Winsock error codes on POSIX | Keep their Winsock numeric values rather than mapping to `errno` | Core and the FileZilla layer compare them by name and store them in their own state |
| `TRegistry` on Linux | Every key reports absent; nothing can be read or written | No registry exists. The INI backend is the correct one there, and failing visibly beats returning defaults for settings the user had set |
| INI share modes | Windows share modes are ignored on POSIX rather than approximated | Advisory locking would change behaviour rather than match it |
| `SetThreadPriority` on POSIX | Recorded and reported back by `GetThreadPriority`, but no scheduling effect | Core only lowers background queue threads; running them at normal priority is harmless |
| `ExitThread` / `EndThread` on POSIX | Unwinds to the entry point by exception, so destructors run; Win32 skips them | A `catch (...)` between the call and the entry point would intercept it. Core calls it only as the last statement of a thread entry point |
| Named events on POSIX | `CreateEvent` with a name fails with `ERROR_NOT_SUPPORTED` | Would need to be cross-process. Core never names one; refusing beats quietly creating a private event |
| `StripHotkey` in East Asian locales | The whole-`(&X)` suffix removal Delphi does under `SysLocale.FarEast` is not reproduced | Only the `&` is removed. Affects only CJK system locales |
| `ParamStr(n)` splitting on Windows | Follows Delphi's `GetParamStr` rules as documented in the shim -- quotes toggle and are dropped, no backslash escapes, empty `""` skipped -- but has not been compared against a Delphi binary | Wine cannot check it: `CommandLineToArgvW` implements the MSVC rules, which differ deliberately. Core itself only uses `ParamStr(0)` |
| `TPath::IsDriveRooted` on POSIX | Always false | As in Delphi's POSIX RTL. Core uses it only to locate Windows' UAC VirtualStore |
| `SuspendThread` on POSIX | Not declared | A running pthread cannot be suspended. A missing declaration fails at compile time; a stub would fail silently. `CREATE_SUSPENDED` + `ResumeThread` is supported |

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
| **A real core TU compiles against the shim with 70 errors from 12 distinct missing VCL types** (was 109 / 24) | `compile_census.py` on `core/Common.cpp`, mingw target | 2026-10-10 |
| On the mingw target every remaining diagnostic is a VCL gap, not a platform one | Win32 types all resolve via the real mingw headers | 2026-10-10 |
| 7/7 tests pass on both targets after the byte-string family | `ctest` | 2026-10-10 |
| `Common.cpp` down to **30 errors / 5 distinct missing types** (from 109 / 24) | `compile_census.py` | 2026-10-10 |
| 10/10 tests pass on Linux and on Windows under Wine | `ctest` both trees | 2026-10-10 |
| The ratchet detects a regression: baseline 25 against actual 30 fails | simulated baseline, exit 1 | 2026-10-10 |
| `TDateTime` matches Delphi's four documented anchors, including -1.25 = 1899-12-29 06:00 | `test_datetime` | 2026-10-10 |
| Delphi's `DayOfWeek` offset verified independently against five known dates | standalone cross-check, k=6 | 2026-10-10 |
| ~~`Common.cpp`: 109 -> 5 errors~~ — **superseded**: the parse was aborting at a missing header | see the correction above | 2026-10-10 |
| Upstream pinned to **6.5.8**, the latest stable release | `git tag`; 6.6.x is entirely RC/beta | 2026-10-10 |
| The transform handled the version change with **no script edits**: 624 declarations across 70 files, 0 unhandled | `transform.py` on 6.5.8, which differs from the old pin by ~2,500 lines across 58 core files | 2026-10-10 |
| All 624 transformed declarations compile on 6.5.8 | `check_properties.py`, exit 0 | 2026-10-10 |
| Honest aggregate census: **2,409 errors across 9 core TUs**, ratcheted | `census_ratchet.py` | 2026-10-10 |
| 12/12 tests pass on both targets after the version change | `ctest` both trees | 2026-10-10 |
| All 44 `__closure` typedefs rewrite to `TClosure<Sig>`; 16 single-line, 28 spanning lines, 0 unhandled | `transform.py` | 2026-10-10 |
| Changed lines now fully accounted for: 624 property + 44 closure + 28 template-end = 696 | `verify_transform.py` | 2026-10-10 |
| `CLOSURE()` binds the enclosing instance correctly on both targets, including non-void returns and reference arguments | `test_closure` on Linux and under Wine | 2026-10-10 |
| Aggregate census **2,409 -> 1,924** across 9 TUs; `Terminal.cpp` missing types 51 -> 20 | `census_ratchet.py` | 2026-10-10 |
| 116 call sites pass a bare member-function name where a closure is expected | clang diagnostic count across all core TUs | 2026-10-10 |
| 13/13 tests pass on both targets | `ctest` | 2026-10-10 |
| **`core/Option.cpp` compiles clean on mingw: exit 0, 0 errors, 0 fatal** | direct clang run, verified rather than inferred from the census | 2026-10-10 |
| The same TU still fails on Linux with 10 Win32-type errors | the deferred P1-07 gap | 2026-10-10 |
| Aggregate census **1,924 -> 1,220** across 9 TUs | `census_ratchet.py` | 2026-10-10 |
| `Format` handles width, alignment, precision and explicit argument indices; unknown specifiers and missing arguments stay visible instead of dropping text | `test_strutils` | 2026-10-10 |
| 161 narrow string literals in core require `UnicodeString(const char *)` | grep over generated core | 2026-10-10 |
| 14/14 tests pass on both targets | `ctest` | 2026-10-10 |
| **`core/Option.cpp` compiles clean on Linux *and* Windows: 0 errors each** | direct clang runs on both targets | 2026-10-10 |
| FILETIME conversions validated against the real Win32 API under Wine, using epoch anchors fixed by the format | `test_win32_posix` on both targets | 2026-10-10 |
| Real Win32 `FileTimeToSystemTime` faults on a null argument; the POSIX version rejects it | Wine crash dump, `cmpq $0, (%rcx)` | 2026-10-10 |
| Honest totals with 0 units aborting: mingw 1,334, Linux 1,575 | `census_ratchet.py`, both targets | 2026-10-10 |
| 15/15 tests pass on both targets | `ctest` | 2026-10-10 |
| Census after streams: mingw 1,242, Linux 1,483, 0 aborting | `census_ratchet.py` | 2026-10-10 |
| 16/16 tests pass on both targets | `ctest` | 2026-10-10 |
| **`Option.cpp` and `NamedObjs.cpp` both compile clean on both targets** | `compile_census.py`, 0 errors each | 2026-10-10 |
| Census after containers: mingw 1,070, Linux 1,299, 2/9 units clean | `census_ratchet.py` | 2026-10-10 |
| 17/17 tests pass on both targets | `ctest` | 2026-10-10 |
| Resource strings resolved with **no core changes**: 608 strings generated from upstream's `.rc`, all ids matched, 0 unknown, 0 duplicates | `gen_resources.py` | 2026-10-10 |
| 18/18 tests pass on both targets | `ctest` | 2026-10-10 |
| **`FileBuffer.cpp` joins the clean set: 3/9 units at 0 errors on both targets** | `compile_census.py` | 2026-10-11 |
| Census after storage and arrays: mingw 963, Linux 1,182, 0 aborting | `census_ratchet.py` | 2026-10-11 |
| `TRegistry` round-trips against the real Windows registry under Wine | `test_storage` on the mingw target | 2026-10-11 |
| 20/20 tests pass on both targets | `ctest` | 2026-10-11 |
| Closure call sites: **62**, not 118 — resolving macro-expansion diagnostics to their definitions collapsed the duplicates | `bind_closures.py --discover` | 2026-10-11 |
| `DynamicArray::Length` is a read/write property, not a method; 56 of the original 118 reports were this, not closures | `Buffer.Length = 10*1024` in core | 2026-10-11 |
| All 758 changed lines accounted for: 624 property + 44 closure typedef + 28 template end + 62 binding | `verify_transform.py` | 2026-10-11 |
| Census after closure binding: mingw 933, Linux 1,152 | `census_ratchet.py` | 2026-10-11 |
| 21/21 tests pass on both targets | `ctest` | 2026-10-11 |
| Census after the long tail: mingw **619**, Linux **863** (from 933 / 1,152) | `census_ratchet.py` | 2026-10-11 |
| Every `__closure` in core converted: 44 typedefs, 2 inline parameters, 62 bound sites | `transform.py`, 0 remaining | 2026-10-11 |
| 22/22 tests pass on both targets | `ctest` | 2026-10-11 |
| Census after the property-access fix: mingw **342**, Linux **586**; 4/9 clean on mingw | `census_ratchet.py` | 2026-10-11 |
| Borland's `__property` reaches *private* accessors; `__declspec(property)` does not | `TBookmark` declares public properties over private setters | 2026-10-11 |
| `__finally` in core: **182 blocks**, 0 `__try`, 0 `__except`; only 2 bodies contain a `return` | grep plus brace analysis | 2026-10-11 |
| `UnicodeString` has implicit numeric constructors in C++Builder | `EXT_EXCEPTION_METHODS` calls `ExtException(E, Ident)` with an `int`, and the only viable overload takes a string | 2026-10-11 |
| All **179** `try`/`__finally` blocks transformed, 0 unhandled; the patch series is still empty | `transform.py` | 2026-10-11 |
| Changed lines: 1,297, every one accounted for — 624 property, 46 closure, 28 template end, 62 binding, 537 try/finally (179 x 3) | `verify_transform.py` | 2026-10-11 |
| Census after try/finally: mingw **314**, Linux **496**; 4/9 clean on *both* targets | `census_ratchet.py` | 2026-10-11 |
| 23/23 tests pass on both targets | `ctest` | 2026-10-11 |
| `FileMasks.cpp` clean on both targets; **5/9 units clean on both** | `compile_census.py` | 2026-10-11 |
| Census: mingw **266**, Linux **441** | `census_ratchet.py` | 2026-10-11 |
| Vendored expat compiles for both targets; the mingw sysroot has no expat | `cmake/expat.cmake` | 2026-10-11 |
| The Windows CRT's `towlower` folds only ASCII in the default locale | the same test run against the real CRT under Wine | 2026-10-11 |
| 24/24 tests pass on both targets | `ctest` | 2026-10-11 |
| 12/12 tests pass on Linux and on Windows under Wine | `ctest` both trees | 2026-10-10 |
| Directory search validated against both backends: real `FindFirstFileW` under Wine, and `dirent` + `fnmatch` on Linux | `test_sysutils` on both targets | 2026-10-10 |
| `__closure` scope: 44 distinct closure types, 16 typedefs; call sites pass bare member-function names | grep over generated core | 2026-10-10 |

## Task status

### P0 — Foundation
| ID | Task | Status | Note |
|---|---|---|---|
| P0-01 | Repo layout | done | `port/{shim,platform,tools,patches}`, `qt/`, `cmake/` |
| P0-02 | Pin submodule to a release tag | done | 6.5.8, latest stable; 6.6.x is all RC/beta |
| P0-03 | Dual-target property macro header | done | `port/shim/vcl_property.h`; 9 forms, self-tested |
| P0-04 | Verify C++Builder accepts the macro | blocked | Q-2: needs licence + Windows |
| P0-05 | `transform.py` property rewriter | done | 613/613, 0 unhandled |
| P0-06 | `transform.py` fastcall/ARRAYOFCONST | done | `__closure` handled by P0-06b/c/d |
| P0-06b | `__closure` type + typedef transform | done | 44 typedefs; `CLOSURE()` macro verified on both targets |
| P0-06c | `__closure` call-site binding | done | 62 sites; verified as a test |
| P0-06d | `__closure` inline parameter form | done | 2 sites in `SessionInfo` |
| P0-07 | Transform idempotence tests | done | idempotent + line-count parity asserted |
| P0-08 | CMake skeleton | done | transform+verify wired as a build step; CTest on both targets |
| P0-09 | Minimal shim for `core/Common.cpp` | wip | 224 errors; `Option.cpp` is the first TU at 0 |
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
| P0-27 | `compile_census.py` | done | classifies clang diagnostics into shim gaps |
| P0-29 | Legacy unqualified RTL header stubs | done | `Classes.hpp` etc.; absence was aborting most TUs |
| P0-30 | Win32 A/W macro collisions undone | done | link-time guard test |
| P0-31 | Ratchet both targets, track aborts | done | an abort is a distinct state, not a low number |
| P0-32 | `port/compat` for upstream gaps | dropped | `help.h` was compensating for the missing `WINSCP` define; withdrawn 2026-10-11 |
| P0-28 | `census_ratchet.py` | done | both targets; aborts tracked separately |

### P1 — Headless core
| ID | Task | Status | Note |
|---|---|---|---|
| P1-01 | Shim: `UnicodeString` class | done | 1-based API, UTF-16, 9 test groups incl. surrogate pairs |
| P1-01b | Shim: free string routines | done | `Option.cpp` reaches 0 errors |
| P1-01d | Shim: `TVarRec`/`Format`/`ARRAYOFCONST` | done | `FORMAT` 438 + `FMTLOAD` 252 call sites |
| P1-01e | Resource strings | done | 608 strings generated from upstream `.rc`; no core changes |
| P1-01c | Shim: byte-string family + UTF-8 codec | done | malformed-input behaviour tested explicitly |
| P1-02 | Shim: `TObject`/`TStrings`/`TStringList` | done | 0-based indexing, case-insensitive default, CommaText round trip |
| P1-02b | Shim: `TList`/`TObjectList`/sets | done | ownership and `Notify` semantics tested |
| P1-03 | Shim: `TDateTime` + date/time RTL | done | documented anchors, negative composition, `FormatDateTime` |
| P1-03b | Shim: `SysUtils`/`IOUtils`/`StrUtils` paths | wip | `TSearchRec`, `FindFirst`/`FindNext`, `FileExists` done; path routines next |
| P1-04 | Shim: exceptions | done | 17 Delphi classes; base and sibling matching tested |
| P1-05 | Shim: `TCriticalSection`/`SyncObjs` | done | recursive; thread exclusion tested |
| P1-05b | Shim: `TThread`/`StartThread` | done | Win32 threads, events and waits on POSIX; `BeginThread`/`EndThread` on both. Tested against the real kernel under Wine. `TThread` itself dropped — dead upstream |
| P1-08b | Shim: masks, paths, timespan, encoding, env | done | the census long tail |
| P1-08c | Shim: XML DOM over expat | done | real parsing; FileZilla-shaped fixtures |
| P1-07c | `port/platform` wide-char functions | done | ASCII folding, matching the Windows CRT |
| P1-09b | `try`/`__finally` for POSIX | done | 179 blocks, 0 unhandled; same expansion on both targets |
| P1-06 | Shim: streams | done | `TStream`/`THandleStream`/`TFileStream`/`TMemoryStream` |
| P1-06b | Shim: INI/registry storage | done | INI parses real-world files; registry real on Windows |
| P1-06c | Shim: `DynamicArray` + file API | done | sharing semantics tested |
| P1-06d | Select INI backend on Linux | todo | frontend decision |
| P1-07 | `port/platform` Win32 subset | done | types, FILETIME/SYSTEMTIME, errors, winsock names |
| P1-07b | `port/platform` Win32 file/handle functions | todo | as the census asks |
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
