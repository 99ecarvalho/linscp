# ADR 0011 — Windows builds with llvm-mingw against MSYS2's Qt

**Status:** accepted 2026-10-13 (supersedes the 2026-10-10 toolchain choice's
GCC sysroot; the `*-windows-gnu` target stands)
**Date:** 2026-10-13

## Context

The Windows build was clang driving Debian's GCC mingw-w64 sysroot:
mingw-w64 headers, GCC's libstdc++ and libgcc, msvcrt. P2 needs Qt for
Windows, which Debian does not package (risk R4). Qt's own binaries are for
MSVC 2022 and MinGW 13.1 (GCC, its own libstdc++); mixing those DLLs with
Debian's GCC 14 libstdc++ is fragile, and building Qt ourselves costs hours
per toolchain change. The user asked why MinGW at all (2026-10-12).

Options weighed:

- **Keep the GCC sysroot**, build Qt with it once and cache it.
- **llvm-mingw** (clang, lld, libc++, UCRT: no GCC parts), linking MSYS2's
  prebuilt `clang64` Qt, which is that ABI.
- **clang-cl, MSVC ABI**, linking Qt's official MSVC binaries: the most usual
  Windows ABI, but building from Linux needs Microsoft's CRT and SDK
  (`xwin`) and testing moves to Windows runners, weakening the Wine tests.

## Decision

llvm-mingw, against MSYS2's Qt:

- `cmake/toolchain-llvm-mingw.cmake`; the toolchain is a pinned llvm-mingw
  release (`port/tools/fetch_llvm_mingw.py`, SHA-256 checked).
- Qt is MSYS2's `qt6-base` **6.8.2**, Debian trixie's version, so Debian's
  `moc`, `uic` and `rcc` are the cross-build's host tools (`QT_HOST_PATH`).
  MSYS2 keeps old package files but not old repository states, so
  `port/tools/fetch_msys2_qt.py --resolve` rebuilds the repository as it was
  when that Qt was built and locks every package by SHA-256
  (`port/qt/msys2-qt.lock`, 27 packages).
- The C++ runtime is MSYS2's from that snapshot (libc++, libunwind,
  winpthreads 19.1.7): Qt was linked against it, and its `libc++.dll`
  exports the unwinder that llvm-mingw's keeps in `libunwind.dll`, so Qt does
  not load with llvm-mingw's own. Hence llvm-mingw **20250114**, LLVM 19.1.7,
  the same release: linscp's objects need nothing that runtime lacks.
  Moving Qt means moving all three together.
- binutils' `objcopy` stays (`binutils-mingw-w64-x86-64`): llvm-objcopy
  cannot weaken COFF symbols, which `port/putty` needs. The census compiles
  core for Windows with llvm-mingw's clang++ too (falling back to the host
  clang and a mingw-w64 sysroot without it), so it sees the build's headers.

## Consequences

- One compiler family on both platforms, no GCC runtime; the UCRT, part of
  Windows 10 and later, replaces msvcrt.
- Builds and tests stay on Linux: cross-built, ctest under Wine, as before.
- Newer mingw-w64 headers are stricter: two changes came with them, both
  valid under C++Builder -- `mfc-cstring-const-search.patch` (ISO C++'s
  const `wcschr` overloads) and neon's `<sys/stat.h>` declared before its
  `stat64` macro.
- Qt's version is Debian's until the host tools come from elsewhere.

## Verification

- All 56 ctests pass on the llvm-mingw build under Wine, golden included.
- Natively on Windows 11: WinSCP's .NET assembly drives the llvm-mingw
  `linscp-cli` over SFTP, FTP, FTPES, WebDAV and S3; a Qt Widgets program
  cross-built against the locked Qt runs with the `windows` platform plugin,
  with every DLL import resolved within the shipped DLLs.
