# ADR 0003 — FileZilla keeps its window-message model on POSIX

**Status:** accepted 2026-10-11 (supersedes PLAN.md's R2 approach for P1-12)
**Date:** 2026-10-11 (recorded 2026-10-12)

## Context

FileZilla's socket layer drives every socket through a hidden helper window
per thread: `WSAAsyncSelect` posts socket events to it, the layers post their
own notifications, a timer fires on it, and the worker thread runs a
`GetMessage` loop fed by `PostThreadMessage`. PLAN.md budgeted the riskiest
P1 task (R2) for rewriting that onto epoll.

## Options

**A. Rewrite onto epoll/poll.** About 1,700 lines of state machine changed,
diverging from upstream on every merge, with the subtle async bugs R2 warned
of.

**B. Emulate the user32 subset it uses.** Windows as message targets, thread
message queues, timers, `RegisterWindowMessage`, `WSAAsyncSelect` and
`WSAAsyncGetHostByName`, with Win32's retrieval and filter rules.

## Decision

**Option B**, in `port/platform/win32_window_posix.cpp` and the Winsock
monitor (ADR 0002). About twenty functions; FileZilla compiles unmodified.

## Consequences

- No FileZilla rewrite and no FileZilla diff beyond one field name
  (`filezilla-in-addr-s-addr.patch`).
- The emulation's rules are checked against real user32 under Wine by the
  `messages` test; the one difference found (the `(HWND)-1` filter does not
  return thread timers) now matches Wine.
- `SendMessage`, painting and the desktop are absent: nothing headless needs
  them, and a use fails to compile.

## Verification

FTP, FTPES and FTPS pass the protocol matrix and the golden comparison on
both platforms, against the suite's server and pyftpdlib, and through a
SOCKS5 proxy.
