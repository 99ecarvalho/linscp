# ADR 0002 — Win32 below the core on POSIX, rather than a POSIX core

**Status:** accepted 2026-10-11
**Date:** 2026-10-11 (recorded 2026-10-12)

## Context

Core, WinSCP's PuTTY and FileZilla call Win32 directly: files and handles,
events and threads, Winsock, time zones, the registry, version resources,
certificate chains, and (FileZilla) window messages. Upstream changes none of
that, and every line changed to make core POSIX-native is a line that
conflicts on the next merge.

## Options

**A. Port the callers.** Rewrite the Win32 calls in core, PuTTY and FileZilla
to POSIX, and take PuTTY's `unix/` platform layer. Native, but a large,
permanent diff: core is written against PuTTY's *Windows* layer
(`select_result`, `do_select`, `HandleWaitList`, WinSCP's per-session
`do_select`), so PuTTY's Unix layer would need reimplementing against core's
model.

**B. Provide Win32 underneath.** A Win32 subset implemented on POSIX
(`port/platform`), with the semantics core depends on -- verified against the
real API -- and upstream code compiled unchanged on top, PuTTY's Windows layer
included. Where Linux genuinely differs, single units are replaced by ones
implementing the same API (`port/putty/linux`: the agent on
`$SSH_AUTH_SOCK`, GSSAPI from the system's Kerberos).

## Decision

**Option B.** The layer is C and C++ (PuTTY is C), with Win32's narrow and
wide names as Windows maps them; Winsock is a `ws2_32` module whose
`GetProcAddress` serves Winsock-signature functions, with `WSAEventSelect`'s
and `WSAAsyncSelect`'s re-enabling rules; FileZilla gets the user32 subset of
ADR 0003. Behaviour is taken from Win32, checked where observable by the same
test run against real Win32 (under Wine and natively) and against the layer.

## Consequences

- Upstream compiles with a handful of reviewed patches, all upstreamable or
  dual-target; merges stay mechanical.
- The layer is the port's largest component and its semantics must be right,
  not merely compile: a missing re-enabling rule hangs or spins, a
  never-connected socket polled as hung up deleted nothing only because a test
  caught it. Each emulated area has a test that runs on both platforms.
- Where Windows behaviour has no POSIX meaning, the layer documents what it
  does instead (the divergence table in TRACKER.md).

## Verification

`win32_on_posix`, `winsock`, `messages`, `sftp_session*`, `protocols`, and the
golden comparison against real WinSCP (ADR 0007) pass on Linux, under Wine and
natively on Windows.
