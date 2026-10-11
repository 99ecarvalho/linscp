# port/compat

Headers that are **missing from the WinSCP repository itself**, as distinct from
`port/platform`, which emulates Win32 on POSIX.

`port/compat` is on the include path for *every* target, because the gap is in
upstream rather than in any platform. `port/platform` is POSIX-only and its
headers refuse to compile on Windows.

| File | Why |
|---|---|
| `help.h` | PuTTY's `windows/platform.h` includes it, but WinSCP does not vendor it — it belongs to PuTTY's Windows GUI, which WinSCP replaces. Core never calls into it. |
