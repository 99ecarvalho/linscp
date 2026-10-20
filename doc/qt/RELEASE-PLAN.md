# Release & distribution plan

How linscp gets from GitHub releases to "install it from your distro's store".

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

This document covers step 1: the distribution rollout, in phases, plus the code
changes each phase needs.

Everything is anchored on signed, changelogged GitHub releases (0.7.x and on),
which already exist.

## Phase 1 — Flathub (≈ weeks)

One Flatpak runs on almost every distribution, and it is what the app stores —
GNOME Software, KDE Discover, Fedora, Linux Mint — surface. This is the single
highest-leverage channel, so it goes first.

We already have a Flatpak recipe (`port/packaging/flatpak/`). What is still
needed:

- A **build recipe that downloads nothing at build time** (Flathub builds
  offline). Sources and submodules are declared, not fetched.
- A **validated `metainfo.xml`** with screenshots and a `<release>` per version
  (we have the releases list; screenshots still to add).
- A **pull request to `flathub/flathub`**.
- Likely an **app-ID change.** Today it is `io.github._99ecarvalho.linscp`.
  Now that we own the domain, **`org.linscp.linscp`** is cleaner, and Flathub can
  domain-verify it. Changing the ID touches the metainfo, the `.desktop` file,
  the Flatpak manifest, and the D-Bus/desktop integration — do it once, before
  the Flathub submission.

## Phase 2 — Self-maintained repositories (≈ days)

These give immediate reach and demonstrate demand, which helps the Phase 3
sponsorships. All of them are **under our control** — no distro sponsor needed.

- **Arch — AUR:** a `PKGBUILD` (covers Arch and Manjaro).
- **Fedora — Copr:** a personal repo with a `.spec`.
- **openSUSE — OBS** (Open Build Service): also builds `.deb` and `.rpm` for a
  range of other distributions from one place.
- **Ubuntu — PPA** on Launchpad.

## Phase 3 — Official distributions (≈ months, sponsor-gated)

- **Debian:** file an **ITP** (Intent To Package) via `reportbug wnpp`, prepare
  the `debian/` folder, and find a sponsor on mentors.debian.net. Once in Debian,
  **Ubuntu inherits the package automatically.**
- **Fedora:** request a package review on Bugzilla; an experienced packager must
  sponsor it.
- **Arch official (extra):** a distro maintainer must adopt it — heavy AUR usage
  makes this far more likely.
- **NixOS:** a pull request to `nixpkgs` — the easiest official path.

## Code changes required before distro review

These are what typically **fails review** in Debian and Fedora. They are the
real blockers for Phase 3, and some unblock Phase 1/2 too:

1. **Unbundle OpenSSL.** linscp builds its own copy (`3rd-party/openssl`).
   Debian and Fedora forbid bundled crypto libraries — security fixes must come
   from the system package. Add a CMake option to **build against the system
   OpenSSL** (there is none today).
2. **Bundled core and PuTTY.** Negotiable: this code is modified and does not
   exist as a standalone library. Document the reason in the package.
3. **Full source tarball per release.** GitHub's auto-generated zip omits
   submodules. Publish a `linscp-<version>.tar.xz` with everything vendored in,
   **GPG-signed**, with a `SHA256SUMS` alongside it.
4. **Offline build.** Distros build without network access, so no CMake step may
   download anything. The `fetch_*` scripts are for developer/CI use only, never
   the official build.
5. **Man pages:** `man linscp` and `man linscp-cli`, generated from the same
   `cli.yaml` the website uses. Expected (or strongly preferred).
6. **Stable releases with changelogs.** 0.7.x already helps. Keep **signed tags**
   and release notes on every release.
7. **Reproducible builds.** Output must not depend on build date or path. The
   "Dev Build" label already disappears under `LINSCP_RELEASE`.

`clang`-only and Qt 6 are both accepted by the distributions and are not a
problem.

## Windows channels

Fast, manifest-only PRs with good visibility:

- **winget** (manifest PR)
- **Scoop**
- **Chocolatey**

## Recommended immediate next steps

Start with what is fully in our control and unblocks the most downstream:

1. Add the **system-OpenSSL CMake option** and the **full signed source tarball +
   SHA256SUMS in the release CI** — these two unblock Debian *and* Fedora.
2. Decide and apply the **`org.linscp.linscp` app ID**.
3. Submit to **Flathub**; publish the **AUR `PKGBUILD`** and the **Copr `.spec`**.
4. Then open the sponsor-gated Phase 3 tracks (Debian ITP, Fedora review, nixpkgs
   PR) once install numbers back the case.
