# Release & distribution plan

How linscp gets from GitHub releases to "install it from your distro's store".
Requirements below were checked against each channel's own documentation on
2026-10-20; the tracker at the end lists every deliverable, one line each.

## Strategy

linscp is an independent cross-platform file-transfer client for Linux and
Windows.

The ideal order would be to finish the code cleanup and rewrite, then release.
We are **not** waiting for that — it takes too long to sit on. So the order is
deliberately inverted:

1. **Publish broadly now** — get reach, and let real install numbers show demand
   (which in turn helps the later, sponsor-gated steps).
2. **Clean up and rewrite the codebase in parallel and afterward** — tracked
   separately from this document.

One exception to "publish first": **licence clean-up (Phase 0) comes before any
new channel.** Every repository below reviews licences, and a single non-free
file blocks all of them.

### Constraints that shape every phase

- **Release cadence.** Channels expect stable, spaced releases. Several releases
  a week reads as development snapshots (Flathub rejects those outright). From
  0.7.x on: tagged releases only when there is something to ship, with real
  release notes.
- **Project age.** Flathub, nixpkgs and the official distributions weigh a
  sustained history and real-world use. Self-maintained channels (Phase 2) do
  not, so they come first in practice; Flathub is prepared now and submitted
  once the project has a few months of steady history.
- **Minimum platforms.** linscp needs Qt ≥ 6.8 and CMake ≥ 3.25. That excludes
  Ubuntu 24.04 (Qt 6.4) and openSUSE Leap 15.x. Supported targets today: Debian
  13, Ubuntu 26.04 / 26.10, Fedora 43 / 44 / rawhide, openSUSE Leap 16.0 /
  Tumbleweed, Arch.
- **OpenSSL 4.0.** Fedora rawhide and Ubuntu 26.10 already ship OpenSSL 4.0. The
  system-OpenSSL option must be tested against 4.0 before it is enabled there.
- **LTO.** Arch, Fedora, openSUSE and Ubuntu enable LTO by default. Start with
  LTO off in each recipe and turn it on per target once a clang LTO build passes
  the test suite.

## Phase 0 — Licence clean-up (blocks everything)

### 0.1 Remove the MFC subset (non-free)

**What and why.** The FTP/FTPS engine (`source/filezilla`, GPL-2.0-or-later)
was originally written against Microsoft's MFC class library. The vendored
upstream tree carries a trimmed copy of MFC's own source in
`3rd-party/winscp/libs/mfc/` (8 files, ~1,100 lines) so that engine compiles
without MFC. Those files are Microsoft-copyrighted ("All rights reserved"), not
under a free licence. `port/filezilla/CMakeLists.txt` builds them as
`linscp-mfc` (`OUTPUT_NAME uafxcw`), and the object code ends up in every
shipped `linscp` and `linscp-cli`.

**What is actually used** — three classes, ~55 functions:

- `CString` (≈250 uses): construction/assignment, `+`/`+=`, comparisons,
  `GetLength`, `IsEmpty`, `GetAt`/`[]`, `SetAt`, `Find`, `ReverseFind`,
  `FindOneOf`, `Mid`/`Left`/`Right`, `Replace`, `Delete`, `MakeLower`,
  `TrimLeft`/`TrimRight`, `Format`/`FormatV`/`Format(UINT id)`,
  `CompareNoCase`.
- `CTime` (18 uses): `CreateForCurrentTime`, construction from `FILETIME` and
  `time_t`, `GetTime`, `GetYear`…`GetMinute`.
- `CFile` (16 uses): `Open` with the documented `modeCreate`/`modeWrite`/
  `modeRead`/`modeNoTruncate`/`shareDeny*` flags, `Read`, `Write`, `Close`, a
  public `HANDLE m_hFile`.

Nothing else (`CObject`, exceptions, collections, `CArchive`, `CAsyncSocket`)
is used; the core outside the FTP engine does not use MFC at all.

**Fix: a clean-room `port/mfc-compat/`** — original GPL code implementing that
API surface over the existing `UnicodeString` and Win32/POSIX layers
(~300–450 lines, 2–3 days). The ~250 call sites stay untouched; rewriting them
instead would take 1–2 weeks and risk index-base bugs in FTP listing parsing.

Clean-room rules:

- The implementer works only from the API list above and Microsoft's public
  reference documentation, and never opens `libs/mfc`.
- Different internal structure, file layout and naming; no copied comments.
- Provenance recorded in an ADR: author, date, sources consulted, and a
  statement that the original source was not viewed.

Behaviour to match (all from the public documentation): 0-based indexes; `Find`
family returns −1 when not found; `Mid`/`Left`/`Right` clamp out-of-range
arguments; `Format` follows `UnicodeString::vprintf`; `CTime` from `FILETIME`
converts UTC to local time; `CFile` errors raise through `RaiseLastOSError` with
the file name appended (that text reaches logs compared by the golden corpus); a
short write raises `ERROR_DISK_FULL`. No libc `wcs*` calls on POSIX (ADR 0001,
ctest `wide_imports`).

Wiring: replace the `linscp-mfc` target with one built from `port/mfc-compat/`
(keep `OUTPUT_NAME uafxcw`, or drop the `#pragma comment(lib, …)` through the
transform); point the FTP engine's include path at `port/mfc-compat/include`;
remove `libs/mfc` from `transform.py`'s default subdirs; drop
`mfc-cstring-const-search.patch` from the series.

Verification: a new unit test (`mfc_compat`) for every behaviour above, plus
`wide_imports`, `core_link_*`, the FTP/FTPES/FTPS cases of `protocols` and
`protocols_real`, `golden`, `cli_session`, `qt_session_ftp*`,
`qt_gui_properties*`, on Linux and Windows-cross CI and the Windows VM.

Guardrails: a configure-time CMake check and a ctest (`no_mfc`) that fail if
any target compiles or includes anything under `libs/mfc`; the source-tarball
script (0.4) prunes `libs/mfc` and fails if an MFC banner survives.

### 0.2 Third-party images in the vendored tree

The upstream tree's images (application/site icons, toolbar glyphs and
animations embedded in `source/forms/*.dfm`, the installer icon) are licensed
for that application only and may not be redistributed with other software.
linscp compiles none of them: its app icon is original (gradient tile + Lucide
glyph, ISC), and its toolbar icons are Lucide (ISC), Tabler (MIT) and Fluent
(MIT). The exposure is **redistribution in source form**: any source tarball,
Flatpak source, or distro orig tarball must strip
`source/resource/*.ico`, `deployment/` and the image-bearing `source/forms/*.dfm`
(linscp builds none of them). A CI check fails if CMake or a Qt resource file
ever references an image under `3rd-party/winscp`.

### 0.3 Notices for bundled binaries (LGPL and others)

The Windows package bundles Qt 6 and other MSYS2 DLLs — LGPL (Qt, glib,
gettext, libiconv, graphite2) and notice-requiring (ICU, FreeType, HarfBuzz,
libpng, zlib, zstd, brotli, bzip2, PCRE2, double-conversion, md4c, libb2) —
without their licence texts or a source offer. Required:

- Copy each deployed package's `share/licenses/<pkg>/` into `licenses\`.
- Publish the matching MSYS2 source archives (the versions pinned in
  `port/qt/msys2-qt.lock`) with each release, or ship a written offer.
- State in About and the README that Qt is used under the LGPL-3.0, with a link
  to its source.
- Audit the AppImage the same way (it bundles Qt and its dependencies).
- Small fixes: ship neon's `COPYING.LIB` (it exists; a comment says otherwise)
  and libs3's `COPYING-GPLv2`; ship the Inno Setup licence alongside the
  installer's other notices.

The Inno Setup script is linscp's own (structure modelled on a GPL-3 installer
script; a handful of short task strings are shared). Inno Setup's licence is
zlib-style and permits this use; no change of installer technology is needed.

### 0.4 Release engineering every channel depends on

- **Full source tarball** `linscp-X.Y.Z.tar.xz` built in CI: the tree plus the
  `winscp`, `putty` and `openssl` submodules (not `linscp-website`), `.git`
  removed, Phase 0 exclusions applied. GitHub's automatic archives omit
  submodules and are not byte-stable, so every recipe uses this tarball.
- **Signing.** An OpenPGP release key (published in the repo and on
  keys.openpgp.org); annotated, signed tags (`git tag -s vX.Y.Z` — the 0.7.x
  tags are currently lightweight); `linscp-X.Y.Z.tar.xz.asc`; a signed
  `SHA256SUMS` covering every release asset.
- **Release workflow.** A tag-triggered GitHub Actions workflow builds the
  tarball, the packages, the checksums and signatures, drafts the release, and
  notifies the website (`repository_dispatch: linscp-release`, already
  expected by the website's deploy workflow).
- **Changelog.** `NEWS.md` in the tree, one entry per release, 2–3 meaningful
  sentences each (Flathub and Debian both read release notes).

## Phase 1 — Flathub (prepare now, submit after history builds)

One Flatpak runs on almost every distribution, and it is what GNOME Software,
KDE Discover, Fedora and Linux Mint surface.

### Policies that decide acceptance

- **Hand-written submission.** Flathub forbids AI-generated or AI-assisted
  content in the manifest, and forbids AI tools opening the submission pull
  request or writing its description or review replies. The submission manifest
  and PR are written and handled by the maintainer.
- **Disclosure.** Submitters must disclose AI-generated material included in
  the application (which parts, roughly how much); reviewers judge it case by
  case.
- **History.** Apps that have existed only a short time are generally not
  accepted; evidence is a sustained commit history and tagged releases.
- **Category.** Broad-scope file managers are generally not accepted unless
  submitted by upstream with an explicit commitment to support the Flatpak.
  linscp is presented as a file-transfer client, and is submitted by upstream.
- **Stable software only**; complete English localisation; no machine-generated
  translations.
- **PR checklist** (closed automatically if incomplete): description, a
  **video of the app running as the Flatpak**, ID compliance, development
  history, AI disclosure, upstream relationship.

### App ID

Choose the final ID before submitting — renaming afterwards means submitting a
new app and end-of-lifing the old one. **Decided: `org.linscp.linscp`**
(lowercase, domain-verified, independent of a GitHub account). Verification
after acceptance: a token at `https://linscp.org/.well-known/org.flathub.VerifiedApps.txt`
(website `public/.well-known/`) or a `_flathub.linscp.org` TXT record.

The rename touches: the metainfo, `.desktop` and manifest file names and IDs;
`LINSCP_APP_ID` in `port/packaging/CMakeLists.txt`; `build_flatpak.py`;
`build_appimage.py`; the icon names; Qt's `setDesktopFileName`; the developer
id; and the website pages that mention the ID.

### Manifest requirements

- **Sources:** `type: git` with both `tag` and `commit` (a local `type: dir`
  source is rejected), or the Phase 0 release tarball with `sha256`. No network
  during the build.
- **Runtime:** the latest KDE runtime at submission time — currently
  `org.kde.Platform`/`org.kde.Sdk` **6.11** (freedesktop 25.08 base, Qt 6.11,
  OpenSSL 3.5) — with `org.freedesktop.Sdk.Extension.llvm22` (25.08 branch) for
  clang.
- **Bundled OpenSSL** is allowed (built from source), but be ready to explain
  why the runtime's copy is not used (static linking keeps it apart from the
  libssl Qt loads); switching to the runtime's OpenSSL once 0.4's option exists
  avoids the question.
- **cleanup:** headers, static archives, pkg-config and CMake files, man pages
  of bundled modules. Licence files of every module installed under
  `share/licenses/$FLATPAK_ID`.
- **Permissions:** `--share=ipc`, `--socket=wayland`, `--socket=fallback-x11`,
  `--device=dri`, `--share=network`; `--filesystem=home` needs a written
  justification (a two-pane local/remote browser); drop
  `--system-talk-name=org.freedesktop.login1` (disable "shut down when the queue
  finishes" under Flatpak) or justify it.
- **x-checker-data** on the main source (`is-main-source: true`, tag pattern
  `^v([\d.]+)$`) and on OpenSSL and PuTTY; never track commits.
- **Architectures:** Flathub builds x86_64 and aarch64; test aarch64 or add
  `flathub.json` with `only-arches`.

### MetaInfo, desktop file, icon (the linter treats warnings as errors)

- Developer: `<developer id="br.com.apliant"><name>Apliant</name></developer>`
  (AppStream allows exactly one developer element).
- URLs: `homepage` `https://linscp.org`; `vcs-browser`
  `https://github.com/99ecarvalho/linscp`; `contribute`; `bugtracker`;
  `donation`; `help`.
- The package descriptions (deb, AppStream) describe linscp on its own terms,
  with no comparison to other software.
- **Screenshots:** 3–6 PNG, at most 1000×700, captioned (one sentence, no
  full stop), one `type="default"`, URLs pinned to a tag or commit.
- **Branding:** a light and a dark primary colour.
- **Releases:** every entry with a date and 2–3 sentences; latest not a
  prerelease; no future dates.
- **Summary:** ≤ 35 characters, no trailing period, no app name, no generic
  "client"/"tool". **Description:** 3–6 lines, not a file manager.
- **Desktop file:** remove `Qt` from `Categories`; `Icon=<appid>`; pass
  `desktop-file-validate`.
- **Icon:** scalable SVG (or ≥ 256 px PNG), square, no baked shadow, readable on
  light and dark.

### Desktop integration conventions

From the Flatpak conventions and the freedesktop Icon Theme and Desktop Entry
specifications:

- Every exported file is named after the app ID (or `<appid>.foo` /
  `<appid>-foo`): `share/applications/<appid>.desktop`,
  `share/metainfo/<appid>.metainfo.xml`,
  `share/icons/hicolor/<N>x<N>/apps/<appid>.png` (square, at most 512×512, at
  least 48×48) and `share/icons/hicolor/scalable/apps/<appid>.svg`; MIME
  definitions, if ever added, in `share/mime/packages/<appid>.xml`; a D-Bus
  service, if ever added, named after the app ID.
- `Icon=` is the app ID without an extension; the desktop file has `Name`,
  `Exec`, `Type`, `Icon`, `Categories` and passes `desktop-file-validate`.
- The application calls `QGuiApplication::setDesktopFileName("<appid>")`, so
  the Wayland app_id matches the desktop file and docks show the right
  launcher and icon.
- Runtime self-registration (`port/qt/Integration.cpp`: the user-level
  `.desktop`, icon, shortcuts and `mimeapps.list` entries) uses the app ID for
  every name, and is skipped when linscp runs from a distribution package or a
  Flatpak, where the packaged desktop file already declares the URL schemes. It
  stays for the AppImage and plain archives.
- Under Flatpak, configuration lives in `~/.var/app/<appid>/config`; document
  that a Flatpak install does not share settings with a package or AppImage
  install, and check that portable (`/ini`) mode and the configuration paths
  behave inside the sandbox.

### App behaviour under Flatpak

- Disable Help › Check for Updates when running as a Flatpak (updates come from
  Flathub).
- Disable or adapt anything that cannot work in the sandbox (shutdown-on-finish,
  default-protocol registration, search-path changes).

### Process

Build and run locally with `org.flatpak.Builder`; run `flatpak-builder-lint` on
the manifest and the built repo; fork `flathub/flathub` (all branches), branch
from `new-pr`, add the manifest, open a PR titled `Add org.linscp.linscp`
against `new-pr`. Reviews typically take 2–14 days. After merge: enable GitHub
2FA and accept the collaborator invite within a week; updates are PRs to
`flathub/org.linscp.linscp`; verify the domain in the Developer Portal.

## Phase 2 — Self-maintained repositories (days each)

All four use the Phase 0 release tarball, build offline, and can be automated
from the release workflow. Bundled OpenSSL is tolerated in all of them;
switch to system OpenSSL as soon as the option exists.

### Arch — AUR

- Account with a dedicated SSH key; packaging repo cloned from
  `ssh://aur@aur.archlinux.org/linscp.git`; only `master` accepted.
- `PKGBUILD` + `.SRCINFO` (regenerated every push) + a 0BSD `LICENSE` for the
  packaging files (without 0BSD a package is not eligible for promotion to the
  official repos).
- `linscp` from the release tarball (sha256, `validpgpkeys`); optional
  `linscp-git` (submodules as separate `source=()` entries rewired in
  `prepare()`, `provides`/`conflicts`).
- `makedepends=(cmake ninja clang lld python perl qt6-tools)`;
  `depends=(qt6-base qt6-svg …)`; `CC=clang CXX=clang++`;
  `-DCMAKE_BUILD_TYPE=None`; LTO off (`options=(!lto)`) or `-fuse-ld=lld`.
- Checks: `namcap` on the PKGBUILD and the package; build in a clean chroot
  (`pkgctl build`).
- Automation: `KSXGitHub/github-actions-deploy-aur` on release.

### Fedora — Copr

- FAS account; Copr API token as a CI secret (tokens expire); project with
  `fedora-43`, `fedora-44`, `fedora-rawhide` chroots (add 45 after it ships),
  build timeout raised above the 5-hour default if needed.
- `linscp.spec`: `%global toolchain clang` (with a comment: upstream does not
  build with GCC), SPDX `License:`, `Provides: bundled(...)` for every bundled
  component, `%cmake -G Ninja`, `%{gpgverify}` of the tarball,
  `desktop-file-validate` and `appstream-util validate-relax --nonet` in
  `%check`.
- Checks: `mock` build, `rpmlint`, `fedora-review`.
- Automation: `copr-cli build` from the release workflow (or Packit
  `copr_build` on release).

### openSUSE — OBS

- `home:<user>` project with Tumbleweed, Leap 16.0, Fedora, Debian 13 and Ubuntu
  26.04/26.10 targets.
- `_service` fetching the release tarball (`download_files` + `verify_file`) or
  `obs_scm` at the tag (submodules on by default); a shared `.spec` with
  `%if 0%{?suse_version}` branches; `debian.*` files with `DEBTRANSFORM-TAR`
  for the Debian/Ubuntu targets.
- Automation: an OBS workflow token + GitHub webhook + `.obs/workflows.yml`.

### Ubuntu — Launchpad PPA

- Launchpad account, OpenPGP key imported; PPA activated (accept the Terms of
  Use; sign the Code of Conduct if the form asks).
- Debian source package (`3.0 (quilt)`, DEP-5 `debian/copyright`), built with
  `debuild -S`, uploaded with `dput`; source uploads only.
- Series: **26.04 (resolute)** and **26.10 (stonking)** only; versions like
  `X.Y.Z-1~ppa1~ubuntu26.04.1`; the same byte-identical
  `linscp_X.Y.Z.orig.tar.xz` for every series.
- `Build-Depends: debhelper-compat (= 13), cmake, ninja-build, clang, lld,
  qt6-base-dev, qt6-svg-dev, …`; `DEB_BUILD_MAINT_OPTIONS = hardening=+all
  optimize=-lto` to start.
- Checks: `lintian -EvIL +pedantic`, an offline `sbuild`.
- Automation: a CI job with a signing subkey running `debuild -S` and `dput` per
  series. 8 GiB PPA quota — prune old versions.

## Phase 3 — Official distributions (months, sponsor-gated)

All of them require Phase 0 complete, the system-OpenSSL option, man pages, and
the signed source tarball.

### Debian (Ubuntu follows automatically)

- **ITP** via `reportbug wnpp` (package, version, author, URL, licence,
  description, why it is useful).
- `debian/` (debhelper compat 13+, `Build-Depends: clang` with the reason
  documented, hardening flags kept), DEP-5 `debian/copyright` with a stanza for
  every bundled tree and `Files-Excluded` for Phase 0 exclusions (`+dfsg`
  repack), watch file version 5 with signature checking and
  `debian/upstream/signing-key.asc`, `debian/upstream/metadata`, an autopkgtest
  smoke test.
- Embedded copies (Policy §4.13): system OpenSSL and expat are expected;
  modified PuTTY, neon and libs3 copies are justified in `debian/copyright` and
  registered in the security tracker's embedded-code list.
- Man pages are required (Policy §12.1); not generated from `--help` at build
  time.
- Lintian clean, including `embedded-library`, `no-manual-page`,
  `source-is-missing`, `debian-watch-does-not-check-openpgp-signature`.
- Upload to mentors.debian.net, file an RFS against `sponsorship-requests`, find
  a sponsor (the Qt/KDE team is a natural fit), pass the NEW queue (ftpmasters
  review the copyright file).
- Timeline: 3–9 months. Reaching unstable before **2027-02-25** (Ubuntu 27.04
  Debian Import Freeze) puts linscp in Ubuntu 27.04.

### Fedora (then EPEL)

- Package built in Copr first (Phase 2); review request in Bugzilla stating
  "first package, needs a sponsor, I am upstream"; sponsorship requested at
  packager-sponsors; `fedpkg request-repo` after approval.
- Bundling: where upstream supports a system library the package must use it
  (so OpenSSL comes from the system once the option exists); otherwise
  `Provides: bundled(...)` plus a public request to upstream.
- Crypto policies: OpenSSL users must follow the system policy (no hard-coded
  cipher lists, or `PROFILE=SYSTEM`).
- Prohibited content removed by a documented repack script.
- Timeline: 1–4 months. EPEL 10 once Qt ≥ 6.8 is confirmed there.

### Arch [extra]

No application process: a Package Maintainer must adopt the AUR package. Votes
and pkgstats usage matter (≈ 10 votes is the informal threshold, not enforced);
the AUR packaging must be 0BSD-licensed and build against system libraries.

### NixOS — nixpkgs

- `pkgs/by-name/li/linscp/package.nix`: `clangStdenv`, `fetchFromGitHub` with
  `fetchSubmodules` (or the release tarball), `qt6.wrapQtAppsHook`, system
  `openssl` (security-critical components must be unvendored), `meta`
  (description, license, `mainProgram`, maintainers, homepage).
- A separate `maintainers: add <handle>` commit; PR titled `linscp: init at
  X.Y.Z` with a `nixpkgs-review` report.
- nixpkgs prefers packages proposed by users rather than their authors; wait for
  real usage.

## Windows channels

- **winget:** multi-file manifest (schema 1.12.0; singleton manifests are
  refused), `InstallerType: inno` (silent switches supplied by the client),
  `ProductCode: <AppId>_is1` with the installer's fixed AppId GUID, Publisher
  and PackageName matching Add/Remove Programs, the portable zip as a second
  `zip`/`portable` installer. Created with `wingetcreate new`, updated from the
  release workflow with `wingetcreate update --submit`; Microsoft CLA signed
  once. The validation pipeline scans URLs (SmartScreen reputation) and
  binaries — unsigned installers are more likely to be flagged.
- **Scoop:** the Extras bucket requires ≈ 100 stars or 50 forks. Until then, an
  own bucket (`linscp.json`: portable zip, `checkver: github`, `autoupdate`
  against `SHA256SUMS`, `shortcuts`, `bin` for `linscp-cli.exe`).
- **Chocolatey:** `linscp.nuspec` + `chocolateyInstall.ps1` (`/VERYSILENT`,
  SHA256, `packageSourceUrl`); automated validation, verification and human
  moderation; VirusTotal scan shown on the package page; updates via
  `chocolatey-au`.
- **Code signing** — not required by any of the three, strongly recommended
  (unsigned public software gets SmartScreen blocks). Options:
  - SignPath Foundation (free for open source; requires every component under
    an OSI licence — possible after 0.1 — MFA for the team, CI-built releases
    with manual approval, a "Code signing policy" section on the website).
  - Azure Artifact Signing (≈ USD 10/month; individual eligibility limited to
    a few countries).
  - An OV certificate (≈ USD 150–300/year, hardware or cloud HSM).

## Tracker

Status: `todo`, `wip`, `blocked`, `done`, `dropped`. Owner `M` = maintainer only
(cannot be delegated: hand-written submissions, accounts, keys, sponsorship).

| ID | Phase | Deliverable | Owner | Status |
|---|---|---|---|---|
| R0-01 | 0 | Clean-room spec of the used `CString`/`CTime`/`CFile` API (from public docs) | | done |
| R0-02 | 0 | `port/mfc-compat/` implemented by an author who has not seen the MFC source | | done |
| R0-03 | 0 | ctest `mfc_compat` unit test | | done |
| R0-04 | 0 | Swap `linscp-mfc` for `mfc-compat`; drop `libs/mfc` from transform and the patch series | | done |
| R0-05 | 0 | FTP/FTPES/FTPS suites green on Linux, Windows-cross and the Windows VM | | done |
| R0-06 | 0 | Guard: CMake check + ctest `no_mfc` | | done |
| R0-07 | 0 | ADR recording the clean-room provenance | | done |
| R0-08 | 0 | CI check: no image under `3rd-party/winscp` referenced by CMake or Qt resources | | dropped |
| R0-09 | 0 | Windows package: licence texts of every bundled DLL in `licenses\` (`deploy_windows.py`: `third-party/<pkg>/`, `THIRD-PARTY-NOTICES.txt`) | | done |
| R0-10 | 0 | Written source offer (`licenses\SOURCE-OFFER.txt`, three years) shipped in the installer and portable zip | | done |
| R0-24 | 0 | Keep a durable copy of every offered source archive (`fetch_msys2_qt.py --sources`), per release — MSYS2 prunes old archives. Local copy in `source-archive/msys2/` (gitignored); an off-site backup is still needed | M | wip |
| R0-25 | 0 | Re-resolve the MSYS2 lock: two pinned packages are no longer published, so a cold CI cache cannot fetch the Windows toolchain | | done |
| R0-26 | 0 | Release-only crash in ctest `object_exceptions` (double free); run the unit tests in a Release configuration in CI | | done |
| R0-27 | 0 | `CMAKE_BUILD_TYPE=Debug` fails to link `linscp-cli` (undefined `DoAssert`/`DoAssertC`) | | todo |
| R0-11 | 0 | About + README: Qt used under LGPL-3.0, with source link | | done |
| R0-12 | 0 | AppImage: same licence-text and source audit | | todo |
| R0-13 | 0 | Ship neon `COPYING.LIB`, libs3 `COPYING-GPLv2`, Inno Setup licence; fix the neon comment | | done |
| R0-14 | 0 | `make_source_tarball.py`: submodules, no `.git`, no `linscp-website`, Phase 0 exclusions, fails on MFC banner or excluded images; reproducible (byte-identical reruns), builds offline. Keeps the five `libs/openssl` files the OpenSSL-tree check compares and the three image-free `.dfm` menu layouts the menu tests read | | done |
| R0-15 | 0 | OpenPGP release key, published in repo and on keys.openpgp.org | M | todo |
| R0-16 | 0 | Annotated signed tags from the next release on | M | todo |
| R0-17 | 0 | Tag-triggered release workflow: tarball, packages, `.asc`, signed `SHA256SUMS`, draft release, website dispatch | | done |
| R0-18 | 0 | `NEWS.md` with an entry per release | | done |
| R0-19 | 0 | CMake option for system OpenSSL, tested against 3.5 and 4.0 | | todo |
| R0-20 | 0 | Options for system expat (and neon/libs3 where feasible) | | todo |
| R0-21 | 0 | Man pages `linscp.1`, `linscp-cli.1` (from `cli.yaml`), installed to `CMAKE_INSTALL_MANDIR` | | done |
| R0-22 | 0 | Reproducible builds: `SOURCE_DATE_EPOCH`, no `__DATE__`/`__TIME__`, path-independent output. `cmake/reproducible.cmake`; Linux programs and `.deb`, Windows programs and portable zip byte-identical from two checkout paths; ctest `no_build_paths`. Installer and AppImage not checked | | done |
| R0-23 | 0 | Update check off by default in distro/Flatpak builds; build-time switch | | done |
| R1-01 | 1 | Decide the final app ID: `org.linscp.linscp` | M | done |
| R1-02 | 1 | Rename the app ID everywhere (packaging, tools, icons, `setDesktopFileName`, website) | | todo |
| R1-03 | 1 | MetaInfo: developer `br.com.apliant` (Apliant); homepage linscp.org; vcs-browser, contribute, bugtracker, donation, help URLs | | todo |
| R1-18 | 1 | Package descriptions (deb, AppStream) rewritten to describe linscp only | | todo |
| R1-19 | 1 | `QGuiApplication::setDesktopFileName("org.linscp.linscp")`; `StartupWMClass` consistent | | todo |
| R1-20 | 1 | `Integration.cpp`: app-ID names for the desktop file, icon and shortcuts; skip self-registration in package and Flatpak builds | | todo |
| R1-21 | 1 | Flatpak config location (`~/.var/app/…`) documented; `/ini` mode and config paths checked in the sandbox | | todo |
| R1-04 | 1 | MetaInfo: 3–6 captioned screenshots ≤ 1000×700, pinned URLs, one default | | todo |
| R1-05 | 1 | MetaInfo: light/dark branding colours | | todo |
| R1-06 | 1 | MetaInfo: release notes for every entry; summary ≤ 35 chars; description not a file manager | | todo |
| R1-07 | 1 | Desktop file: drop `Qt` category, `Icon=<appid>`, `desktop-file-validate` clean | | todo |
| R1-08 | 1 | Icon check: scalable SVG, square, no shadow, light and dark | | todo |
| R1-09 | 1 | Flatpak build: disable update check and sandbox-incompatible features | | todo |
| R1-10 | 1 | `appstreamcli validate`, `desktop-file-validate`, `flatpak-builder-lint` in CI | | todo |
| R1-11 | 1 | Website: `public/.well-known/org.flathub.VerifiedApps.txt` path served (token added after acceptance) | | todo |
| R1-12 | 1 | aarch64 build tested, or `flathub.json` `only-arches` | | todo |
| R1-13 | 1 | Submission manifest (KDE 6.11, llvm22, git tag+commit, cleanup, permissions, x-checker-data), hand-written | M | todo |
| R1-14 | 1 | Written justification for `--filesystem=home` | M | todo |
| R1-15 | 1 | Screen recording of the Flatpak running | M | todo |
| R1-16 | 1 | Disclosure statement and PR against `new-pr`, written and opened by the maintainer | M | todo |
| R1-17 | 1 | After merge: 2FA, accept invite, domain verification in the Developer Portal | M | todo |
| R2-01 | 2 | AUR account + SSH key | M | todo |
| R2-02 | 2 | AUR `linscp` PKGBUILD + `.SRCINFO` + 0BSD LICENSE; namcap and clean-chroot build | | todo |
| R2-03 | 2 | AUR `linscp-git` (optional) | | todo |
| R2-04 | 2 | AUR deploy job in the release workflow | | todo |
| R2-05 | 2 | Fedora account + Copr project + API token secret | M | todo |
| R2-06 | 2 | `linscp.spec`; mock, rpmlint and fedora-review clean | | todo |
| R2-07 | 2 | Copr build job in the release workflow | | todo |
| R2-08 | 2 | OBS account + `home:` project with targets | M | todo |
| R2-09 | 2 | OBS `_service`, shared spec, `debian.*` files; `osc build` per target | | todo |
| R2-10 | 2 | OBS workflow token + webhook + `.obs/workflows.yml` | | todo |
| R2-11 | 2 | Launchpad account, key, PPA activated | M | todo |
| R2-12 | 2 | `debian/` packaging (shared with Phase 3); lintian and offline sbuild clean | | todo |
| R2-13 | 2 | PPA upload job (26.04, 26.10) with a signing subkey | | todo |
| R3-01 | 3 | Debian ITP filed | M | todo |
| R3-02 | 3 | `debian/copyright` (DEP-5) for every bundled tree, `Files-Excluded`, `+dfsg` repack | | todo |
| R3-03 | 3 | Watch file v5 + upstream signing key + `debian/upstream/metadata` + autopkgtest | | todo |
| R3-04 | 3 | Security-tracker embedded-copy entries (PuTTY, neon, libs3) | M | todo |
| R3-05 | 3 | mentors.debian.net upload + RFS + sponsor + NEW queue | M | todo |
| R3-06 | 3 | Fedora review request + sponsorship + `fedpkg request-repo` | M | todo |
| R3-07 | 3 | Fedora repack script for prohibited content; crypto-policy compliance | | todo |
| R3-08 | 3 | EPEL 10 branch (once Qt ≥ 6.8 confirmed) | M | todo |
| R3-09 | 3 | Arch [extra]: approach a Package Maintainer once there is AUR usage | M | todo |
| R3-10 | 3 | nixpkgs `package.nix` + maintainer entry + PR | | todo |
| RW-01 | W | winget manifest (inno + portable) via `wingetcreate`; CLA signed | | todo |
| RW-02 | W | `wingetcreate update --submit` in the release workflow | | todo |
| RW-03 | W | Scoop: own bucket now; Extras request after ≈ 100 stars | | todo |
| RW-04 | W | Chocolatey package + `chocolatey-au` | | todo |
| RW-05 | W | Code signing: SignPath application after R0-02, or another option | M | todo |
| RW-06 | W | Sign `setup.exe` and the executables in the portable zip | | todo |
| RC-01 | — | Colour render path in `IconSet.cpp` (paint SVGs as-is, with a disabled-state treatment) | | todo |
| RC-02 | — | Add the Tango, icons8 Flat Color and Papirus sets (licences shipped, roles mapped in `import_icons.py`) | | todo |

### Icon sets (RC-01, RC-02)

Decided: add all three colour sets below (Tango, icons8 Flat Color, Papirus)
alongside the existing monochrome ones.

The bundled sets (Lucide, Tabler, Fluent regular) are monochrome and drawn as a
mask in the palette's text colour. A colourful set needs a second render path
that paints the SVG as-is. Candidates with licences compatible with every
channel above:

| Set | Licence | Style | Fit |
|---|---|---|---|
| Tango Icon Library | Public domain | Classic multi-colour, SVG + hand-tuned 16/22 px | Best coverage of the needed roles; no attribution; already in Debian and Fedora; frozen since 2009 |
| icons8 Flat Color Icons | MIT | Modern flat multi-colour, SVG | Good coverage; copy/move/rename/new-folder must be composed; exclude its brand logos |
| Papirus | GPL-3.0 | Flat colour, maintained, freedesktop names | Widest coverage; on Linux the user's theme can be used via `QIcon::fromTheme` |
| Phosphor (duotone) | MIT | One accent hue + tint | Works with the existing mask renderer; "colourful" only as accent |

Not suitable: Fluent colour (≈ 200 icons, no folder/copy/transfer/server), Silk
and Fugue (raster only, CC-BY attribution, no HiDPI), Visual Studio Image
Library (proprietary terms).
