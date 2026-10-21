{
  lib,
  clangStdenv,
  fetchurl,
  cmake,
  ninja,
  perl,
  python3,
  qt6,
  openssl,
  expat,
  krb5,
}:

# GCC cannot compile linscp (its property shim needs clang's
# __declspec(property)), hence clangStdenv.
clangStdenv.mkDerivation (finalAttrs: {
  pname = "linscp";
  version = "0.7.1";

  # The release tarball carries the submodules linscp builds from (its core,
  # PuTTY, OpenSSL) and none of OpenSSL's own test-suite submodules, which
  # fetchFromGitHub's recursive fetchSubmodules would pull in.
  src = fetchurl {
    url = "https://github.com/99ecarvalho/linscp/releases/download/v${finalAttrs.version}/linscp-${finalAttrs.version}.tar.xz";
    hash = "sha256-EpSlJPaLWy/r5eJZPHgAz+iAhJWCWoi4HD2y+7MYROs=";
  };

  patches = [
    # The bin/ symlinks with absolute install directories (Nix's cmake hook
    # passes them so). Fixed after 0.7.1: drop on the next release.
    ./install-absolute-dirs.patch
  ];

  strictDeps = true;

  nativeBuildInputs = [
    cmake
    ninja
    perl
    python3
    qt6.qttools # lupdate, lrelease
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    qt6.qtbase
    qt6.qtsvg
    qt6.qtwayland
    openssl
    expat
  ];

  cmakeFlags = [
    (lib.cmakeBool "LINSCP_RELEASE" true)
    (lib.cmakeBool "LINSCP_SYSTEM_OPENSSL" true)
    (lib.cmakeBool "LINSCP_SYSTEM_EXPAT" true)
    (lib.cmakeBool "LINSCP_UPDATE_CHECK" false)
    (lib.cmakeBool "BUILD_TESTING" false)
  ];

  # The programs are not in the default target; build them explicitly.
  ninjaFlags = [
    "linscp"
    "linscp-cli"
    "core-translations"
  ];

  # Kerberos (GSSAPI) authentication loads libgssapi_krb5.so.2 at run time.
  postFixup = ''
    for program in $out/lib/linscp/linscp $out/lib/linscp/linscp-cli; do
      patchelf --add-rpath ${lib.makeLibraryPath [ krb5 ]} "$program"
    done
  '';

  meta = {
    description = "File transfer client for SFTP, SCP, FTP, WebDAV and S3";
    longDescription = ''
      linscp transfers files between your computer and remote servers over
      SFTP, SCP, FTP (with TLS), WebDAV and S3. Its two-panel interface shows
      local and remote files side by side. It synchronizes and compares
      directories, keeps a remote directory up to date, edits remote files,
      runs commands on the server, and has a command-line client for
      scripting (linscp-cli).
    '';
    homepage = "https://linscp.org";
    changelog = "https://github.com/99ecarvalho/linscp/blob/v${finalAttrs.version}/NEWS.md";
    license = with lib.licenses; [
      gpl3Plus
      # bundled: neon (LGPL-2.0-or-later), libs3 (LGPL-3.0-or-later or
      # GPL-2.0-or-later), PuTTY (MIT), icon sets (ISC, MIT)
      lgpl2Plus
      lgpl3Plus
      mit
      isc
    ];
    mainProgram = "linscp";
    platforms = lib.platforms.linux;
    # The maintainer entry is added in a separate `maintainers: add` commit.
    maintainers = [ ];
  };
})
