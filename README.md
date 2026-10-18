# linscp

A Qt port of [WinSCP](https://winscp.net/), targeting **Linux and Windows from
one codebase**.

> **Status: pre-release (0.6.1).** WinSCP's core runs unmodified under a Qt 6
> shell on Linux and Windows: sites, the Commander layout, transfers and the
> queue, synchronization, the editor, translations. See
> [doc/qt/TRACKER.md](doc/qt/TRACKER.md) for what is done and what is left.
> Not affiliated with or endorsed by the WinSCP project.

## Screenshots

The Commander interface: local files on the left, the remote server on the right.

![The Commander interface](doc/screenshots/commander.png)

The Explorer interface: only the remote panel, with the folder tree beside it.

![The Explorer interface](doc/screenshots/explorer.png)

| The Site Manager | Preferences |
|---|---|
| ![The Site Manager](doc/screenshots/site-manager.png) | ![Preferences, the Panels page](doc/screenshots/preferences.png) |

These were taken on Linux with the light theme. To retake them, run
`port/tests/run_qt_session.py build/linux/port/tests/test_qt_gui screenshots /ini=nul {port} {tmp}/local $PWD/doc/screenshots -platform offscreen`.

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
3rd-party/openssl  pinned OpenSSL release (the version WinSCP vendors)
3rd-party/putty    pinned PuTTY release (the version WinSCP vendors), for
                   the Unix platform code WinSCP's copy does not carry
port/patches       reviewed patch series applied to the generated tree
port/shim          VCL runtime replacement
port/platform      POSIX and Win32 compatibility
port/frontend      headless frontend interface and linscp-cli, the scripting client
port/qt            Qt 6 frontend (the linscp program)
port/packaging     install rules and the .deb, AppImage, Flatpak and
                   Windows installer definitions
port/tests         unit, protocol, golden and GUI tests (ctest)
port/tools         transformation, verification, fetch and packaging scripts
doc/qt/            plan, tasks, tracker, estimates
```

## Documentation

Start with [doc/qt/README.md](doc/qt/README.md), or read the rendered
[doc/qt/plan.html](doc/qt/plan.html). [doc/qt/TRACKER.md](doc/qt/TRACKER.md) is
the live status.

## Building

Requires **clang** (GCC is not supported: it has no `__declspec(property)`
equivalent, which the VCL property shim depends on), CMake, Ninja, Python 3,
Perl and make (for OpenSSL's `Configure`), and Qt 6 (`qt6-base-dev`,
`qt6-svg-dev`, `qt6-l10n-tools` on Debian).

```sh
git clone https://github.com/99ecarvalho/linscp
cd linscp
# Not --recurse-submodules: OpenSSL's own submodules are test suites.
git submodule update --init --depth 1
```

### Linux

```sh
cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/linux --target linscp linscp-cli
build/linux/port/qt/linscp
ctest --test-dir build/linux        # the test suite
```

The suite starts its own throwaway servers on loopback (rclone, sshd, and
FTP and S3 servers written for it). With docker or podman, the tests labelled
`real-servers` also run against real ones, built from
[port/tests/servers/](port/tests/servers/) into one image the first time:
OpenSSH, vsftpd (FTP, explicit and implicit FTPS), Apache's mod_dav (WebDAV,
with locking) and Versity's S3 gateway. Without either they are skipped.

```sh
ctest --test-dir build/linux -L real-servers
```

### Windows (cross-compiled from Linux)

The toolchain and Qt are fetched pinned
([doc/qt/windows-toolchain.md](doc/qt/windows-toolchain.md)); the tests run
under Wine.

```sh
python3 port/tools/fetch_llvm_mingw.py --dest build/tools
python3 port/tools/fetch_msys2_qt.py --dest build/tools/msys2-qt
cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
cmake --build build/win --target deploy   # a folder that runs on Windows: build/win/deploy
```

### Packages

Configure a release build (`LINSCP_RELEASE=ON` drops the "Dev Build"
marking), then build the package's target; the packages land in
`<build>/packages`. CI builds all four on every run, as artifacts.

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINSCP_RELEASE=ON \
      -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/release --target package-deb        # linscp_<version>_amd64.deb
cmake --build build/release --target package-appimage   # linscp-<version>-x86_64.AppImage
```

- **.deb**: built with CPack; its dependencies are read from the binaries, so
  it is for the distribution it is built on. Needs `dpkg-dev`.
- **AppImage**: linuxdeploy and appimagetool are fetched pinned
  ([port/packaging/appimage/tools.lock](port/packaging/appimage/tools.lock)).
  Needs `file`, `qt6-svg-plugins` and `qt6-wayland`. It runs on distributions
  whose glibc is at least the build machine's.
- **Flatpak**: built from the checkout with `flatpak-builder` on KDE's
  runtime; the runtime, SDK and LLVM extension are installed for the user
  from Flathub when missing.

  ```sh
  python3 port/tools/build_flatpak.py      # build/flatpak/packages/linscp-<version>.flatpak
  flatpak install --user build/flatpak/packages/linscp-<version>.flatpak
  ```

- **Windows installer**: Inno Setup 6, fetched pinned and run under Wine
  (`wine`, with `wine32` for its 32-bit compiler), over the deploy folder:

  ```sh
  cmake -S . -B build/win-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINSCP_RELEASE=ON \
        -DBUILD_TESTING=OFF -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
  cmake --build build/win-release --target installer      # linscp-<version>-setup.exe
  ```

  It installs for all users or only the current one, and offers a desktop
  icon, the Send To upload shortcut, the URL handler and the search path, as
  WinSCP's installer does.

  With the .NET SDK at `build/win-release/tools/dotnet`
  (`python3 port/tools/fetch_dotnet.py --dest build/win-release/tools/dotnet`;
  or `-DLINSCP_HOST_DOTNET=<dotnet>`), the Windows packages also carry
  WinSCP's .NET assembly in a `dotnet` folder, with `linscp-cli` as the
  `WinSCP.exe` it starts: WinSCP's own PowerShell extensions (Preferences >
  Commands > Add Extension) then run with linscp.

- **Portable**, as WinSCP's portable mode: the configuration is kept in an
  INI file beside the program instead of the user's.
  - Windows: `cmake --build build/win-release --target package-portable`
    gives `linscp-<version>-portable.zip`, the program and an empty
    `linscp.ini` (which `linscp-cli` shares).
  - Linux: the AppImage, with a `WinSCP.ini` (empty to start with) beside
    the `.AppImage` file.

## Licence

GPL v3 — see [LICENSE](LICENSE). WinSCP is GPL v3; PuTTY and the FileZilla FTP
engine carry their own compatible licences. See
[3rd-party/winscp/license.txt](3rd-party/winscp/license.txt).
