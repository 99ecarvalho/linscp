# Developing linscp

How to build, test and package linscp. For using it, see the manual at
<https://linscp.org/manual/>.

## Requirements

- **clang**. GCC is not supported: it has no `__declspec(property)`
  equivalent, which the VCL property shim depends on.
- CMake, Ninja, Python 3, Perl and make (for OpenSSL's `Configure`).
- Qt 6: `qt6-base-dev`, `qt6-svg-dev` and `qt6-l10n-tools` on Debian.

```sh
git clone https://github.com/99ecarvalho/linscp
cd linscp
# Not --recurse-submodules: OpenSSL's own submodules are test suites.
git submodule update --init --depth 1
```

## Linux

```sh
cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/linux --target linscp linscp-cli
build/linux/port/qt/linscp
ctest --test-dir build/linux        # the test suite
```

The suite starts its own throwaway servers on loopback (rclone, sshd, and FTP
and S3 servers written for it). With docker or podman, the tests labelled
`real-servers` also run against real servers built from
[port/tests/servers/](port/tests/servers/) into one image the first time:
OpenSSH, vsftpd (FTP, explicit and implicit FTPS), Apache's mod_dav (WebDAV,
with locking) and Versity's S3 gateway. Without either, they are skipped.

```sh
ctest --test-dir build/linux -L real-servers
```

### System libraries (distributions)

By default OpenSSL and expat are compiled from the bundled sources and linked
statically. Distribution builds can link the system's instead (Linux only;
Windows always uses the bundled copies):

```sh
cmake -S . -B build/linux -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DLINSCP_SYSTEM_OPENSSL=ON -DLINSCP_SYSTEM_EXPAT=ON
```

- `LINSCP_SYSTEM_OPENSSL`: OpenSSL 3.0 or later, 4.0 included (`libssl-dev`);
  `-DOPENSSL_ROOT_DIR=<prefix>` picks a non-default one.
- `LINSCP_SYSTEM_EXPAT`: expat 2.4 or later (`libexpat1-dev`).

neon and libs3 stay bundled: the core uses API that exists only in the
modified copies. [doc/qt/bundled-libraries.md](doc/qt/bundled-libraries.md)
lists every bundled component, its changes, and why.

## Windows (cross-compiled from Linux)

The toolchain and Qt are fetched pinned
([doc/qt/windows-toolchain.md](doc/qt/windows-toolchain.md)); the tests run
under Wine.

```sh
python3 port/tools/fetch_llvm_mingw.py --dest build/tools
python3 port/tools/fetch_msys2_qt.py --dest build/tools/msys2-qt
cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
cmake --build build/win --target deploy   # a folder that runs on Windows: build/win/deploy
```

## Packages

Configure a release build (`LINSCP_RELEASE=ON` drops the "Dev Build"
marking), then build the package's target. Packages land in
`<build>/packages`. CI builds all of them on every run, as artifacts.

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINSCP_RELEASE=ON \
      -DBUILD_TESTING=OFF -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/release --target package-deb        # linscp_<version>_amd64.deb
cmake --build build/release --target package-appimage   # linscp-<version>-x86_64.AppImage
```

Release builds are reproducible ([cmake/reproducible.cmake](cmake/reproducible.cmake)):
the same commit gives byte-identical programs, `.deb` and portable zip from
any checkout path. They are dated by `SOURCE_DATE_EPOCH`, or else by the last
commit (a source tarball's file dates); no absolute path is compiled in
(ctest `no_build_paths`). `-DLINSCP_REPRODUCIBLE=OFF` turns it off.

- **.deb**: built with CPack. Its dependencies are read from the binaries, so
  it targets the distribution it is built on. Needs `dpkg-dev`.
- **AppImage**: linuxdeploy and appimagetool are fetched pinned
  ([port/packaging/appimage/tools.lock](port/packaging/appimage/tools.lock)).
  Needs `file`, `qt6-svg-plugins` and `qt6-wayland`. It runs on distributions
  whose glibc is at least the build machine's.
- **Flatpak**: built from the checkout with `flatpak-builder` on KDE's
  runtime. The runtime, SDK and LLVM extension are installed for the user
  from Flathub when missing.

  ```sh
  python3 port/tools/build_flatpak.py      # build/flatpak/packages/linscp-<version>.flatpak
  flatpak install --user build/flatpak/packages/linscp-<version>.flatpak
  ```

- **Windows installer**: Inno Setup 6, fetched pinned and run under Wine
  (`wine`, with `wine32` for its 32-bit compiler), over the deploy folder.

  ```sh
  cmake -S . -B build/win-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DLINSCP_RELEASE=ON \
        -DBUILD_TESTING=OFF -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-llvm-mingw.cmake
  cmake --build build/win-release --target installer      # linscp-<version>-setup.exe
  ```

  It installs for all users or only the current one, and offers a desktop
  icon, the Send To upload shortcut, the URL handler and the search path.

- **.NET assembly**: with a .NET SDK at `<build>/tools/dotnet`
  (`python3 port/tools/fetch_dotnet.py --dest <build>/tools/dotnet`, or
  `-DLINSCP_HOST_DOTNET=<dotnet>`), the packages also carry the .NET assembly
  in a `dotnet` folder, with `linscp-cli` as the `WinSCP.exe` it starts.

- **Portable**: the configuration is kept in an INI file beside the program.
  - Windows: `cmake --build build/win-release --target package-portable`
    gives `linscp-<version>-portable.zip`, the program and an empty
    `linscp.ini` (which `linscp-cli` shares).
  - Linux: the AppImage, with a `WinSCP.ini` (empty to start with) beside
    the `.AppImage` file.

## Screenshots

The screenshots in [doc/screenshots/](doc/screenshots/) are taken from a demo
session against a local test server, with the light theme:

```sh
port/tests/run_qt_session.py build/linux/port/tests/test_qt_gui screenshots \
  /ini=nul {port} {tmp}/local $PWD/doc/screenshots -platform offscreen
```

## How the source is organised

The upstream core is a pristine, pinned submodule; this repository holds no
modified copy of it. At build time `port/tools/transform.py` rewrites it into
a buildable tree, and the reviewed patches in `port/patches/` are applied on
top. The transformed code compiles both under clang and under C++Builder, so
changes stay mergeable upstream.

```
3rd-party/winscp   pinned upstream submodule, never modified
3rd-party/openssl  pinned OpenSSL release
3rd-party/putty    pinned PuTTY release, for the Unix platform code
port/patches       reviewed patch series applied to the generated tree
port/shim          VCL runtime replacement
port/platform      POSIX and Win32 compatibility
port/frontend      headless frontend interface and linscp-cli
port/qt            Qt 6 frontend (the linscp program)
port/packaging     install rules and package definitions
port/tests         unit, protocol, golden and GUI tests (ctest)
port/tools         transformation, verification, fetch and packaging scripts
doc/qt/            plan, tasks and design records
```

## Website and manual

The website and manual live in the
[linscp-website](https://github.com/99ecarvalho/linscp-website) submodule at
`linscp-website/`. Its `WRITING.md` explains how to write manual pages.
