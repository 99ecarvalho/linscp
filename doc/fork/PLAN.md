# Plan: full fork — converge, Qt-only, clean Linux/Windows builds

> **Status: not scheduled.** This is the plan for if/when linscp stops tracking
> WinSCP and becomes a self-contained project. Recorded 2026-10-18. The
> decision is captured in [ADR 0016](../qt/adr/0016-full-fork.md) (status:
> proposed). Nothing here has been executed.

A phased plan to fully fork from WinSCP and converge into one clean,
self-contained Qt-only project that builds for Linux and Windows.

## 1. Where we are today (what the fork has to absorb)

The current build is a **generator pipeline**, not a source tree:

- `3rd-party/winscp` — pristine pinned submodule (WinSCP 6.7.1 source + its
  bundled libs: expat, neon, libs3, mfc, PuTTY under `source/putty`, openssl).
- `port/tools/transform.py` (~970 lines) + `port/patches/` (27 patches) +
  `port/tools/slices.json` (~150) → produce `build/*/gen` and
  `build/*/gen-slices` (~136k lines of C++) at configure time.
  `verify_transform.py` enforces that the generated tree is a faithful mirror
  of upstream.
- `port/shim` (VCL-compat types), `port/platform` (Win32-on-POSIX), `port/qt`
  (Qt GUI + bridge), `port/frontend` (CLI), `port/{core,gui,putty,filezilla,
  neon,openssl,libs3,dragext}`, `port/packaging`.
- Dual-target constraint: transform output must compile under **both**
  C++Builder (`__property`, `__fastcall`) and clang (`__declspec(property)`
  shim). clang-only otherwise.
- CI: `build` (linux native + windows-cross llvm-mingw), `packages`,
  `real-servers`, `flatpak`.

The fork decision removes four constraints: no pristine submodule, no
transform/patch/slice machinery, no C++Builder dual-target, no
upstream-mergeability (ADR 0006). That is what lets the tree be flattened.

## 2. End state (definition of done)

- One self-contained source tree, no submodule for WinSCP, no code generation
  at build time.
- Qt GUI only; CLI kept. All former "patches" and "slices" are ordinary source.
- clang-only, builds **Linux (native clang)** and **Windows (llvm-mingw
  cross)** with a plain `cmake --build` (Qt app in `ALL`).
- Third-party libraries vendored explicitly (OpenSSL, PuTTY, neon, expat,
  libs3, the FileZilla engine).
- GPLv3-compliant, with clear attribution that this is a fork of WinSCP.
- CI builds + tests + packages on both platforms; no transform/verify steps.

## 3. Guiding principle: converge first, modernize later

Do the **mechanical, behaviour-preserving convergence** before any cleanup
refactor. After each phase, CI must be green on both platforms. Keep the shim —
it is linscp's own code and is what lets WinSCP's VCL-based core compile under
clang. "Clean" means one coherent tree with no generator, **not** rewriting
core off VCL (an optional, separate, much larger effort — Phase 7).

---

## Phase 0 — Freeze a baseline (~0.5 day)

1. Land every upstream patch to be folded in (already carried). Confirm CI
   green.
2. Write **ADR 0016 "Full fork"**: records the decision; supersedes ADR 0006
   (upstream tracking); relaxes ADR 0005 (C++Builder semantics) and the "no
   modified copies" invariant.
3. Tag `pre-fork-baseline` for diff/rollback.
4. Record the exact pin (`git -C 3rd-party/winscp rev-parse HEAD`) in the ADR —
   provenance for the GPL fork.

**Gate:** tag exists, CI green.

## Phase 1 — Materialize the generated tree (the core step, ~1–2 days)

Folds patches + slices + the pinned upstream into real files.

1. Add a `transform.py --materialize` mode (or a one-shot script) that emits the
   generated tree **without `#line` directives** (they point at upstream paths
   that will no longer exist) and with the linscp prologue/epilogue baked into
   slices.
2. Run it once; copy `gen/` and `gen-slices/` into the repo as real source
   (provisional location, e.g. `vendor-src/`), committed.
3. Repoint CMake to compile from the committed files instead of
   `${LINSCP_GEN}`; delete the `add_custom_command` transform step,
   `LINSCP_GEN*`, `LINSCP_UPSTREAM`, the stamp, and the `transform`/
   `verify_transform` wiring.
4. Relocate third-party libraries that came from the submodule so they no
   longer depend on it:
   - PuTTY (`winscp/source/putty`) → `3rd-party/putty-winscp` (WinSCP's patched
     PuTTY — vendor as-is).
   - neon, expat, libs3, the FileZilla engine (`winscp/source/filezilla`), the
     MFC shim bits → vendor under `3rd-party/` or `libs/`.
   - OpenSSL already is its own submodule — keep.
   - **Action:** audit every path the build reads from `3rd-party/winscp/**`
     (grep CMake + `port/*/CMakeLists.txt`) and give each a new home.
5. Delete the `3rd-party/winscp` submodule, `port/patches/`,
   `port/tools/{transform.py,patchset.py,slices.json,verify_transform.py}`, and
   the `.gitmodules` entry.

**Gate (critical):** the materialized tree is *behaviour-identical* — it is
literally what the compiler saw before. Full Linux + Windows build and the
**entire** test suite must pass unchanged. If anything differs, the
materialization is wrong — fix before proceeding.

## Phase 2 — Clean directory layout (~1 day)

Reorganize into an intention-revealing tree and drop the "port" framing:

```
src/
  core/        (ex winscp/source/core, materialized)
  putty/       platform/gss etc.
  filezilla/   neon/  libs3/
  compat/      (ex port/shim + port/platform — VCL + Win32-on-POSIX)
  app/         (ex port/qt — Qt GUI + bridge)
  cli/         (ex port/frontend)
  dragext/
3rd-party/     (openssl, putty-winscp, neon, expat, libs3 — vendored)
cmake/  packaging/  tests/  translations/  doc/  assets/
```

- Move files with `git mv`, update `#include` paths and CMake
  `target_include_directories`, in one reviewable commit.
- Fix the "Qt targets not in ALL" quirk so `cmake --build` builds the app +
  tests by default.

**Gate:** clean build both platforms, tests green.

## Phase 3 — Drop the C++Builder dual-target (~1–3 days, incremental)

Now that C++Builder is never a target:

1. Simplify the property model: keep the clang `__declspec(property)` shim
   (lowest risk) **or** mechanically convert `__property` to real
   getters/setters. Recommended: **keep the shim** first (zero behaviour
   change); convert only if pure C++ is wanted later.
2. Neutralize C++Builder-isms that are pure noise under clang: `__fastcall`,
   `__published`, `PACKAGE`, `.cbproj`/`.groupproj`/`.bpr` project files, `Jcl`,
   `tb2k`, `tbxp`, VCL packages — delete the unused ones.
3. Retire transform-validation tests that no longer apply: `test_patchset.py`,
   `test_closure_shapes.py`, `test_upstream_units.py`,
   `test_tryfinally_inline.py`, `test_lp64_long.py`, `test_openssl_tree.py`
   (keep the runtime equivalents `test_closure.cpp`, `test_tryfinally.cpp`).

**Gate:** build + tests green after each sub-step; safely incremental.

## Phase 4 — Prune to Qt-only (~0.5–1 day)

- Delete upstream GUI never compiled: VCL `forms/`, the VCL app entry, unused
  `windows/` units. Keep only the units actually sliced/built:
  `GUIConfiguration`, `WinConfiguration`, `CustomCommandType`,
  `LocalCustomCommand`, `CustomShortCuts`, `SynchronizeController`,
  `PuttyTools`, `EditorPreferences`, and on Windows `SetupIntegration`.
- Keep `compat/` (shim + Win32-on-POSIX): Qt-only does **not** mean removing
  the Win32 layer; core still calls Win32/VCL APIs.
- Remove dead resources (`.dfm`, `.rc` for unused VCL dialogs).

**Gate:** build + tests green; grep for references to deleted units is empty.

## Phase 5 — Build system & dependencies (~1–2 days)

- **Linux:** clang + system Qt 6 (Debian trixie Qt 6.8). Keep AppImage/Flatpak/
  .deb packaging.
- **Windows:** keep llvm-mingw cross + pinned Qt. Consider moving the Qt pin
  from the MSYS2 snapshot to a reproducible source (aqtinstall or an official
  Qt build) and bumping to >= 6.8.4 (the DoS CVEs from the library audit).
  Keep clang-only.
- Vendor + pin third-party libs with checksums (extend the existing
  `port/tools/fetch_*` pin pattern to Qt and any fetched lib).
- Remove all C++Builder/MSVC build inputs.

**Gate:** `cmake --preset linux` and `cmake --preset windows` build everything
incl. packages; CI matrix simplified (no submodule checkout, no transform).

## Phase 6 — CI, tests, docs, licensing (~1 day)

- CI: drop submodule/transform/verify steps; keep build+test+packages on both
  platforms, real-servers, flatpak.
- Docs: new top-level `README`, `ARCHITECTURE.md` (compat layer + Qt app +
  CLI), retire/repurpose `doc/qt/` (TRACKER/TASKS/gaps were for the port
  effort). Rewrite `CLAUDE.md` — delete the "no modified copies," "transform
  stays dual-target," "pristine submodule" invariants; keep Conventional
  Commits, clang-only, test-on-Linux/Wine.
- **Licensing (must-do):** WinSCP is GPLv3. A fork must keep GPLv3, retain
  WinSCP copyright headers, and add a prominent `NOTICE`/README: "linscp is a
  fork of WinSCP (© Martin Přikryl), based on WinSCP 6.7.1 (commit …), licensed
  under GPLv3." Keep PuTTY's MIT notice and the other libs' licenses.
  Non-negotiable for distribution.

## Phase 7 — First post-fork release + optional modernization

- Tag `0.7.0` (or `1.0.0`) as the first release built from the converged tree.
- **Optional, open-ended:** de-VCL-ify core (replace `UnicodeString`/
  `TStringList`/`__property` with std/Qt types), module by module, behind the
  same test suite. Huge; only if pure modern C++ is a goal.

---

## Decisions to make before starting

1. **Property model:** keep the clang property shim (fast, zero-risk) or convert
   to real accessors (cleaner, large mechanical change). Recommended: keep the
   shim now.
2. **Windows Qt source:** stay on the MSYS2 snapshot, or move to a reproducible
   pinned Qt >= 6.8.4.
3. **History:** keep full history, or start the converged tree with a squash +
   a "forked from WinSCP @ <commit>" root commit (cleaner; still GPL-compliant
   if provenance is documented).
4. **Layout naming:** confirm `src/{core,compat,app,cli}` or keep `port/*`.

## The one thing lost (and must be replaced)

No more free upstream bugfixes/security patches. New post-fork process: watch
WinSCP releases/advisories and port relevant fixes by hand into `src/`. The
three carried security patches (GSSCustom, PuttySettings, SCP dot-names) become
permanent source — the "drop on pin-move" note no longer applies.

## Rough effort

~6–11 working days to a clean, releasable converged tree (Phases 0–6), with the
big risk concentrated in Phase 1's behaviour-preserving gate. Phase 7
modernization is separate and unbounded.
