# ADR 0016 — Full fork: converge the generated tree, Qt-only, drop upstream tracking

**Status:** proposed 2026-10-18 (not scheduled; supersedes [ADR 0006](0006-upstream-tracking.md) when accepted, and relaxes [ADR 0005](0005-cppbuilder-semantics.md))
**Date:** 2026-10-18

## Context

linscp is built by a generator pipeline, not a source tree. `3rd-party/winscp`
is a pristine pinned submodule; `port/tools/transform.py` plus a reviewed
`port/patches/` series and `port/tools/slices.json` produce ~136k lines of C++
at configure time, and `verify_transform.py` enforces that the generated tree
is a faithful mirror of upstream. Several invariants exist only to keep that
mirror and the option of merging upstream:

- no modified copies of WinSCP source in the repo (ADR 0006);
- the transform output must compile under **both** C++Builder (`__property`,
  `__fastcall`) and clang (the `__declspec(property)` shim) (ADR 0005);
- carried changes must stay mergeable, and bugs are reported upstream.

This machinery has real cost — the transform, the patch/slice format with its
per-hunk line-count rule, closure-site discovery, the dual-target property
model, and a set of tests that validate the generator rather than the product.

The question is whether to stop tracking WinSCP entirely: converge everything
done so far into one self-contained tree, keep only the Qt interface, and build
Linux and Windows from ordinary source.

This is driven by the user's position that linscp has its own user base to
protect and cannot always wait on upstream (e.g. the 2026-10-18 security
fixes were carried and released as 0.6.1 ahead of any upstream merge).

## Decision

**Proposed, not scheduled.** If taken, linscp becomes a hard fork:

- Materialize the generated tree (patches + slices + the pinned upstream folded
  in) as real committed source; delete the `3rd-party/winscp` submodule, the
  transform, the patch series, the slice map, and `verify_transform`.
- Vendor the third-party libraries explicitly (OpenSSL, PuTTY, neon, expat,
  libs3, the FileZilla engine) instead of sourcing them from the submodule.
- Keep only the Qt GUI and the CLI; delete the unused VCL GUI (`forms/`, the
  VCL app, unused `windows/` units, C++Builder project files).
- Keep the compatibility layer (`port/shim` + `port/platform`): the core still
  calls VCL/Win32 APIs, and the shim is what lets it compile clang-only.
- Drop the C++Builder dual-target: clang is the only target. Keep the clang
  property shim initially (behaviour-preserving); converting `__property` to
  real accessors is optional later work.
- Build Linux (native clang) and Windows (llvm-mingw cross), clang-only, with a
  plain `cmake --build` and the Qt app in `ALL`.
- Remain GPLv3 with prominent attribution that linscp is a fork of WinSCP
  (© Martin Přikryl), recording the exact upstream commit it was forked from.

The work is sequenced to be behaviour-preserving first (materialize, verified
by the full test suite passing unchanged), then cosmetic (layout), then the
dual-target removal and the Qt-only prune. The full phased plan, gates, risks,
effort and the open decisions live in [doc/fork/PLAN.md](../../fork/PLAN.md).

## Consequences

- No more free upstream bugfixes or security patches: WinSCP advisories must be
  watched and relevant fixes ported by hand. The carried security patches
  (GSSCustom, PuttySettings, SCP dot-names) become permanent source rather than
  drop-on-pin-move carries.
- ADR 0006 (track `master`; libraries as WinSCP vendors them; report upstream)
  no longer applies; ADR 0005's dual-target requirement is relaxed to
  clang-only.
- The generator-validation tests (`patchset`, `closure_shapes`,
  `upstream_units`, `tryfinally_inline`, `lp64_long`, `openssl_tree`) are
  retired; their runtime equivalents stay.
- CLAUDE.md's architecture invariants ("no modified copies", "transform stays
  dual-target", "pristine submodule") are rewritten.
- One-time convergence cost ~6–11 working days (Phases 0–6), the risk
  concentrated in the materialize step's behaviour-preserving gate.

## Status note

Accepted as a direction, **not scheduled**. No code has changed for it. Revisit
when the user decides to stop tracking WinSCP; until then linscp continues to
carry changes and coordinate with upstream per ADR 0006.
