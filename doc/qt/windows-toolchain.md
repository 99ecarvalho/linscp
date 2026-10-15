# The Windows toolchain and Qt

How linscp is built for Windows: the compiler, where Qt comes from, how
every piece is pinned, the one constraint that ties them together, and what
was verified. The decision itself, with the options weighed, is
[ADR 0011](adr/0011-windows-toolchain-llvm-mingw.md); this page is the
working detail behind it (P2-01, 2026-10-13).

## At a glance

Everything is built on Linux. Windows binaries are cross-compiled, tested
under Wine, and spot-checked natively on Windows 11.

| Piece | What | Pinned by | Fetched by |
|---|---|---|---|
| Compiler, linker, headers | [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) **20250114** (clang/lld **19.1.7**, mingw-w64 headers, UCRT) | release name + SHA-256 in `port/tools/fetch_llvm_mingw.py` | `fetch_llvm_mingw.py --dest build/tools` |
| Qt for Windows | MSYS2 `clang64` **qt6-base 6.8.2-1**, **qt6-svg** (the icon sets) | `port/qt/msys2-qt.lock` | `fetch_msys2_qt.py --dest build/tools/msys2-qt` |
| Qt's dependencies and the C++ runtime | 26 more MSYS2 `clang64` packages (libc++/libunwind **19.1.7**, ICU 76, harfbuzz, ...) | the same lock | the same command |
| Qt's code generators (`moc`, `uic`, `rcc`) | Debian trixie's **Qt 6.8.2** (`qt6-base-dev`) | Debian's version | `apt` |
| `objcopy` for COFF | binutils (`binutils-mingw-w64-x86-64`) | Debian's version | `apt` |
| Test runner | Wine 10 | Debian's version | `apt` |

The toolchain file is `cmake/toolchain-llvm-mingw.cmake`; with the two
fetches done, it finds both under `build/tools` on its own.

## Why this combination

- **One compiler family.** linscp needs clang (GCC cannot compile
  `__declspec(property)`). llvm-mingw is clang, lld, libc++ and compiler-rt
  for the `x86_64-w64-mingw32` target, with no GCC parts: no libstdc++, no
  libgcc, no msvcrt. Programs use the UCRT, part of Windows 10 and later.
- **Prebuilt Qt of that ABI.** Debian packages no Qt for Windows. Qt's own
  binaries are for MSVC, or for MinGW GCC with its own libstdc++, which
  would not mix safely with our code. MSYS2's `clang64` environment is built
  with clang, libc++ and the UCRT: llvm-mingw's ABI, prebuilt.
- **Cross-builds and Wine tests stay.** Everything runs on one Linux machine
  and in CI's Linux containers. An MSVC-ABI build (clang-cl) would have
  needed Microsoft's SDK from Linux and moved testing onto Windows runners.

## Qt: one version on both sides

A Qt cross-build runs Qt's code generators -- `moc`, `uic`, `rcc` -- on the
build machine. Qt's CMake packages take them from a **host Qt**
(`QT_HOST_PATH`), which must be the **same version** as the target Qt.

Debian trixie ships Qt **6.8.2**. MSYS2's current Qt is newer, but MSYS2
keeps old package files, and `qt6-base-6.8.2-1` is still there. So the
target Qt is MSYS2's 6.8.2 and the host Qt is Debian's 6.8.2, with no Qt
built by us.

The top-level `CMakeLists.txt` finds the host Qt by its `Qt6HostInfo`
package (`/usr/lib/<multiarch>/cmake/Qt6HostInfo`, from `qt6-base-dev`) and
sets `QT_HOST_PATH=/usr` and `QT_HOST_PATH_CMAKE_DIR`. Without a host Qt the
shell is not configured. Building with the target's tools instead would
fail: they are Windows `.exe`s, and running one on Linux is what
`moc.exe: Syntax error: word unexpected` means.

## Pinning MSYS2: rebuilding the repository as it was

MSYS2 keeps old *package files* but not old *repository databases*. A
package from February 2025 lists its dependencies by name only (for
example `icu`, not `icu-76.1`), and today's `icu` is a different major
version with differently named DLLs (`libicuuc77.dll`, not
`libicuuc76.dll`). Installing Qt 6.8.2 with today's dependencies would give
DLLs Qt cannot load.

`port/tools/fetch_msys2_qt.py --resolve` reconstructs the repository as it
stood when that Qt was built:

1. It reads the repository's file index
   (`https://repo.msys2.org/mingw/clang64/`), which lists every kept file
   with its upload time. Names there are URL-encoded (`libc%2B%2B`), which
   the tool decodes.
2. The **snapshot** is qt6-base 6.8.2-1's upload time plus one day:
   **2025-02-02**.
3. Starting from the roots -- `qt6-base`, `libc++`, `libunwind`,
   `libwinpthread` -- it takes, for each package, the newest version
   uploaded by the snapshot (`libwinpthread` was then named
   `libwinpthread-git`, so both names are looked up). Then it follows the
   `depend =` lines of each downloaded package's `.PKGINFO`, version
   constraints stripped, until nothing new appears.
4. It skips what Qt does not link at run time: `vulkan-headers`; Python and
   what it brings (`python`, `python-packaging`, `mpdecimal`, `tcl`, `tk`,
   `tzdata`, `ncurses`, `readline`, `termcap`, `wineditline`), which glib2
   needs only for its build-time code generators; and `gcc-libs`, a virtual
   package for the C++ runtime, which is fetched by name instead.
5. It writes `port/qt/msys2-qt.lock`: one line per file, `<sha256>
   <file name>`.

The resulting 27 packages, as locked:

> brotli 1.1.0-4, bzip2 1.0.8-3, dbus 1.14.10-1, double-conversion 3.3.0-1,
> expat 2.6.4-1, freetype 2.13.3-1, gettext-runtime 0.23.1-1, glib2 2.82.4-1,
> graphite2 1.3.14-3, harfbuzz 10.2.0-2, icu 76.1-1, libb2 0.98.1-2,
> **libc++ 19.1.7-1**, libffi 3.4.6-1, libiconv 1.18-1, libjpeg-turbo 3.1.0-1,
> libpng 1.6.46-1, **libunwind 19.1.7-1**,
> libwinpthread-git 12.0.0.r480.gea22a99cb-1, md4c 0.5.2-1, openssl 3.4.0-1,
> pcre2 10.44-2, **qt6-base 6.8.2-1**, sqlite3 3.47.2-1,
> vulkan-loader 1.4.304.0-1, zlib 1.3.1-1, zstd 1.5.6-2

`fetch_msys2_qt.py --dest DIR` (no `--resolve`) is what builds and CI run.
It downloads exactly the locked files, fails on any SHA-256 mismatch,
unpacks each with `tar --zstd` into `DIR/clang64`, and records the lock it
used (`DIR/msys2-qt.lock`), so a second run with the same lock does
nothing. The result is about 363 MB unpacked.

Resolving again is needed only to move to another Qt version. Nothing at
build time depends on the repository's current state.

## The catch: the C++ runtime, and why LLVM 19.1.7

The first attempt paired the newest llvm-mingw (LLVM 23.1.3) with the locked
Qt, shipping llvm-mingw's own `libc++.dll` and `libunwind.dll`. Every
ctest passed under Wine. On Windows 11 the program did not start at all:
exit code `0xC0000139`, `STATUS_ENTRYPOINT_NOT_FOUND`.

The cause: in MSYS2's build of libc++, `libc++.dll` **re-exports the
unwinder**. So Qt's DLLs, and everything else MSYS2 builds, import
`_Unwind_Resume` from `libc++.dll`. llvm-mingw's `libc++.dll` keeps the
unwinder in `libunwind.dll` and does not export `_Unwind_Resume`. The same
DLL name, a different export table.

Wine missed it because Wine resolves imports lazily: a missing entry point
becomes a stub that fails only if it is called, and these tests never threw
through Qt. Windows checks every import at load time and refuses to start
the program. (`port/tools/check_dll_imports.py` now checks what Windows
checks; see below.)

The fix has two halves:

1. **Ship the runtime Qt was built against.** The lock's roots include
   MSYS2's `libc++`, `libunwind` and `libwinpthread` from the same snapshot,
   19.1.7. With Qt present, the toolchain points every Windows program --
   tests, `linscp-cli`, `linscp` -- at that runtime (`LINSCP_QT_PREFIX/bin`),
   so one runtime is shipped and tested.
2. **Compile with the same LLVM release.** Code compiled against libc++ 23's
   headers may call functions that libc++ 19's DLL does not export. So the
   compiler is pinned to the llvm-mingw release built on LLVM **19.1.7**:
   **20250114**. Our objects then need nothing that runtime lacks. (It is
   also the version of Debian trixie's own clang, so both platforms build
   with clang 19.1.7.)

**Consequence: Qt, the runtime and llvm-mingw move together.** To move to
another Qt:

1. Check that Debian's `qt6-base-dev` is that version, or find host tools of
   that version elsewhere.
2. `fetch_msys2_qt.py --resolve --qt <version>-<release>`.
3. Read the LLVM version of `libc++` in the new lock, and pin the
   llvm-mingw release built on that LLVM in `fetch_llvm_mingw.py` (release
   name, asset name -- older releases are `ubuntu-20.04`, newer
   `ubuntu-22.04` -- and SHA-256).
4. Rebuild from a clean build directory and run ctest, `windows_imports`
   included, then run the deploy folder on Windows.

## What else the newer headers needed

llvm-mingw's mingw-w64 headers are newer and stricter than Debian's. Three
changes came with the switch, all still valid under C++Builder:

- `port/patches/mfc-cstring-const-search.patch`: ISO C++ declares `wcschr`,
  `wcsrchr`, `wcsstr` and `wcspbrk` on a const string as returning
  `const wchar_t *`, and these headers (like MSVC's) do. WinSCP's MFC
  `CString` kept the results in `wchar_t *`. They are now `const`; they are
  only compared and subtracted. Line-preserving, like every patch in the
  series.
- neon (`port/neon/CMakeLists.txt`): with `NE_LFS`, neon's `config.h`
  spells `stat64` as `__stat64` (that is, `_stat64`), and the newer headers
  declare a `struct stat64` of their own, which the macro turned into a
  second `_stat64`. `<sys/stat.h>` is now included first, so its include
  guard keeps the header's declaration. neon also gets
  `-Wno-error=incompatible-pointer-types`: newer clang makes that an error
  even under `-w`, and neon passes `int *` for Winsock's `char *` and
  `u_long *`, which are the same size there.
- `port/shim/vcl_datetime.cpp` includes `<cstdlib>` for `std::llabs`, which
  libstdc++ had pulled in implicitly and libc++ does not.

Two pieces of the old setup stay:

- binutils' `objcopy`: `port/putty` weakens the PuTTY symbols core
  overrides (`--weaken-symbols`), and `llvm-objcopy` cannot do that for
  COFF.
- The census (`compile_census.py`) compiles core for Windows with
  llvm-mingw's `clang++` when it is present, so it sees the build's own
  headers, and falls back to the host clang with a mingw-w64 sysroot without
  it.

## Building, testing, running

```sh
# once per machine (or when a pin changes)
python3 port/tools/fetch_llvm_mingw.py --dest build/tools
python3 port/tools/fetch_msys2_qt.py --dest build/tools/msys2-qt

cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
python3 port/tools/fetch_rclone.py --corpus port/tests/golden/expected --dest build/win/tools
cmake --build build/win
ctest --test-dir build/win            # under Wine

cmake --build build/win --target deploy   # build/win/deploy: runs on Windows as is
```

- `LINSCP_LLVM_MINGW` and `LINSCP_QT_PREFIX` override where the toolchain
  file looks (cache variables or environment).
- **`deploy`** (`port/tools/deploy_windows.py`) copies `linscp.exe`,
  `linscp-cli.exe`, three Qt plugins (`platforms/qwindows.dll`,
  `styles/qmodernwindowsstyle.dll`, `imageformats/qico.dll`), and every DLL
  they load from the Qt prefix, found by following their imports: 25 DLLs,
  about 103 MB, mostly ICU and Qt. Nothing has to be installed on the target.
- Under Wine, Qt's platform is chosen with Qt's `-platform` argument
  (`-platform offscreen`), because Wine does not pass the `QT_QPA_PLATFORM`
  environment variable through.

The tests that hold this together:

| ctest | Checks |
|---|---|
| the whole suite (59 tests under Wine) | core, PuTTY, FileZilla, neon, libs3 and the CLI on the llvm-mingw build, golden comparison with real WinSCP included |
| `gui_build`, `gui_smoke` | the Qt shell builds, starts core, shows its window, quits from the event loop |
| `windows_imports` | every import of `linscp.exe` and `linscp-cli.exe` from a shipped DLL resolves, recursively through Qt and its dependencies: what Windows checks at load time and Wine does not |

CI's `windows-cross` job fetches both pins, caching each by its pin's hash
(`llvm-mingw-<hash of fetch_llvm_mingw.py>`,
`msys2-qt-<hash of msys2-qt.lock>`), and runs the suite. The `linux` job
fetches llvm-mingw too, for the census.

## Results (2026-10-13)

| Where | What | Result |
|---|---|---|
| Linux, Wine 10 | full ctest on the llvm-mingw 23.1.3 build (first attempt) | 56/56 |
| Windows 11 VM | `linscp-cli` from that build, driven by WinSCP's .NET assembly over SFTP, FTP, FTPES, WebDAV and S3 | all five pass |
| Windows 11 VM | a Qt Widgets program from that build | does not start: `0xC0000139`, the runtime mismatch above |
| Linux | `check_dll_imports.py` over that program | names the missing `_Unwind_Resume` in 12 DLLs |
| Linux, Wine 10 | full ctest on the llvm-mingw 20250114 (19.1.7) build | 56/56; with the shell, 59/59 |
| Windows 11 VM | the Qt Widgets program, rebuilt with 19.1.7 against MSYS2's runtime | runs, `windows` platform plugin, every import resolved |
| Windows 11 VM | `linscp.exe --smoke` from the `deploy` folder | `linscp 6.6.5, Qt 6.8.2, windows: ok` |
| GitHub Actions | `linux` and `windows-cross` jobs | 63/63 and 59/59 |

## When something goes wrong

| Symptom | Cause |
|---|---|
| `No llvm-mingw at ...` at configure | run `fetch_llvm_mingw.py`, or set `LINSCP_LLVM_MINGW` |
| `no Qt 6.8 Widgets found` | run `fetch_msys2_qt.py` (Windows) or install `qt6-base-dev` (Linux); for the cross-build both are needed |
| `moc.exe: Syntax error: word unexpected` | the host Qt was not found, so Qt fell back to the target's Windows tools; install `qt6-base-dev` |
| a program does not start on Windows, `0xC0000139` | an import does not resolve: run ctest `windows_imports`; usually a runtime or LLVM mismatch after a pin moved |
| `0xC0000135` (DLL not found) on Windows | run from the `deploy` folder, not from the build tree |
| Qt ignores `QT_QPA_PLATFORM` under Wine | pass `-platform <name>` instead |
