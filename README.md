# linscp

A Qt port of [WinSCP](https://winscp.net/), targeting **Linux and Windows from
one codebase**.

> **Status: early. Phase 0 of 4.** There is no usable client yet — the work so
> far decouples WinSCP's protocol core from the Embarcadero C++Builder toolchain
> it currently requires. Not affiliated with or endorsed by the WinSCP project.

## Why

WinSCP's protocol core — SFTP, SCP, FTP/FTPS, WebDAV and S3 — is mature, widely
trusted, and GPL. Its UI is Delphi VCL, and it builds only with Embarcadero
C++Builder on Windows. This project reuses the core, replaces the UI with Qt 6,
and drops C++Builder from the toolchain.

## How upstream stays trackable

**This repository contains no copy of WinSCP's source.** Upstream is a pristine,
pinned submodule at `3rd-party/winscp`. A committed transformation script rewrites
it into a buildable tree at build time, so the diff against upstream is
structurally empty and new WinSCP releases are absorbed by re-running the script
rather than by resolving conflicts.

The transformation is deliberately **dual-target**: its output compiles both under
clang (Linux and Windows) and under C++Builder with the original VCL, so the
existing Windows build is never broken by the port.

## Layout

```
3rd-party/winscp   pinned upstream submodule, never modified
port/shim          VCL runtime replacement
port/platform      POSIX and Win32 compatibility
port/tools         transformation and verification scripts
qt/                Qt 6 frontend
doc/qt/            plan, tasks, tracker, estimates
```

## Documentation

Start with [doc/qt/README.md](doc/qt/README.md), or read the rendered
[doc/qt/plan.html](doc/qt/plan.html). [doc/qt/TRACKER.md](doc/qt/TRACKER.md) is
the live status.

## Building

Not yet buildable end to end. To run what exists:

```sh
git clone --recurse-submodules https://github.com/99ecarvalho/linscp
cd linscp
python3 port/tools/transform.py --out build/gen   # rewrite upstream core
python3 port/tools/check_properties.py            # verify the rewrite compiles
```

Requires **clang** and Python 3. GCC is not supported: it has no
`__declspec(property)` equivalent, which the VCL property shim depends on.

## Licence

GPL v3 — see [LICENSE](LICENSE). WinSCP is GPL v3; PuTTY and the FileZilla FTP
engine carry their own compatible licences. See
[3rd-party/winscp/license.txt](3rd-party/winscp/license.txt).
