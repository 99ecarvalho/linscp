# ADR 0006 — Track WinSCP's master; libraries as WinSCP vendors them

**Status:** accepted 2026-10-11 (supersedes P0-02's "release tag, not master")
**Date:** 2026-10-11 (recorded 2026-10-12)

## Context

The pin was a release tag. Upstream is porting itself to 64-bit on `master`,
and five of the first six bugs this port found were already fixed there.
Third-party libraries (OpenSSL, PuTTY, neon, libs3, expat) are vendored by
WinSCP, sometimes trimmed or changed.

## Decision

- `3rd-party/winscp` follows `master`. A bump re-runs the transform, the patch
  series and closure-site discovery, all of which fail loudly where upstream
  moved.
- Libraries stay on the versions WinSCP vendors while that is
  straightforward (user's policy). OpenSSL is built from the same release with
  its own `Configure`, WinSCP's changes adopted or skipped by a reviewed list
  (`port/openssl/winscp-changes.txt`, enforced by `openssl_tree`). If keeping
  a library in step gets complicated, the port moves to a newer upstream
  release with locally maintained patches, offered to WinSCP when the two merge.
- Bugs found in upstream code are reported to the project that owns them,
  against its current `master`, with the fix where there is one:
  winscp/winscp#68, winscp/winscp#69.

## Consequences

- Fixes arrive with each bump instead of being duplicated; the patch series
  shrinks as upstream merges.
- `master` can break the build between releases; the transform's checks make
  that loud.
