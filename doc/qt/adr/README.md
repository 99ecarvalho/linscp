# Architecture decision records

The choices that are hard to reverse or cut across the port, with the
context, the options weighed and the evidence. Smaller decisions are rows in
the decisions table in [TRACKER.md](../TRACKER.md).

| ADR | Decision |
|---|---|
| [0001](0001-wchar-t-width.md) | `wchar_t` is 2 bytes everywhere (`-fshort-wchar`); no C library wide functions |
| [0002](0002-win32-layer-on-posix.md) | Win32 is provided below the core on POSIX; upstream compiles unchanged on top |
| [0003](0003-filezilla-emulated-messages.md) | FileZilla keeps its window-message model, over an emulated user32 subset |
| [0004](0004-lp64-long.md) | Core's `long` is respelled `LONG`/`ULONG`, keeping 32 bits on LP64 |
| [0005](0005-cppbuilder-semantics.md) | Where upstream relies on C++Builder's semantics, the shim reproduces them |
| [0006](0006-upstream-tracking.md) | Track WinSCP's `master`; libraries as WinSCP vendors them; bugs reported upstream |
| [0007](0007-verification.md) | Verification against real servers and a golden corpus from real WinSCP |
| [0008](0008-command-line-client.md) | `linscp-cli`: own runner over core's script; winscp.com's protocol on Windows |
| [0009](0009-posix-configuration.md) | Configuration on POSIX is `WinSCP.ini`, in the XDG directory by default |
| [0010](0010-console-pipes.md) | winscp.com's console protocol over inherited pipes on POSIX, for the .NET assembly |
| [0011](0011-windows-toolchain-llvm-mingw.md) | Windows builds with llvm-mingw against MSYS2's Qt, pinned to Debian's Qt version |

Write one when a decision constrains later work or would be costly to undo;
number in order, never renumber, and amend rather than rewrite an accepted
record.
