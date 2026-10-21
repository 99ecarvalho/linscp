# linscp news

What changed in each release, newest first. Full notes and downloads for every
version are on [GitHub releases](https://github.com/99ecarvalho/linscp/releases).

The release workflow publishes the section whose heading matches the tag, so
each release needs a `## X.Y.Z — YYYY-MM-DD` heading here before it is tagged.

## 0.7.2 — 2026-10-21
linscp gets its permanent name on the Linux desktop, signed releases, and
packages for more systems.

- **A new application ID, `org.linscp.linscp`.** The desktop entry, icons and
  Flatpak use it; on Wayland the window now matches its launcher and icon.
  What an older version registered (menu entry, URL handlers, shortcuts) is
  renamed or cleaned up on first start.
- **Signed releases.** The source tarball and `SHA256SUMS` carry OpenPGP
  signatures from linscp's release key; [SECURITY.md](https://github.com/99ecarvalho/linscp/blob/main/SECURITY.md#verifying-a-download) shows how
  to check a download. Each release also publishes the source code of the
  bundled open-source libraries that ask for it.
- **FTP downloads on Linux** name the file, not its whole path, when asking
  whether to overwrite it, and create the target folders first.
- **Secure connections follow the system's crypto policy** when linscp is
  built against the system OpenSSL (Fedora and others): the TLS versions
  offered are the system's unless a site sets its own.
- **A smaller AppImage,** with only the Qt plugins linscp uses; it and the
  Windows packages carry the licence texts of every bundled library.
- **Flatpak:** updates come from Flathub only (no update check of its own),
  and "Sleep" and "Shut Down Computer" once the queue is empty are hidden,
  as the sandbox cannot do them.
- **Manual pages:** `man linscp` and `man linscp-cli`.
- The interface, the command-line client and their messages name linscp
  throughout.

For packagers: options to build against the system OpenSSL and expat
(`-DLINSCP_SYSTEM_OPENSSL=ON -DLINSCP_SYSTEM_EXPAT=ON`) and to turn off the
update check (`-DLINSCP_UPDATE_CHECK=OFF`); recipes for Arch (AUR), Fedora
and openSUSE, Debian and Ubuntu, and Nix are in `port/packaging/`, and every
release has a complete, reproducible, signed source tarball.

## 0.7.1 — 2026-10-20

A maintenance release: CI clean-up.

## 0.7.0 — 2026-10-19

The first official release, out of pre-release: a fast, friendly app for
moving files between your computer and remote servers, with the same
experience on Linux and Windows. The About dialog and the in-app links (manual,
homepage, donations, security reports) now go to <https://linscp.org>, and the
Help menu has a Report a Security Issue item.

## 0.6.1 — 2026-10-18

A maintenance release with internal fixes. The interface is unchanged.

## 0.6.0 — 2026-10-17

Fills in the remaining parts of the interface. The Site Manager gains its
Manage and context menus (clone, copy URL or code, open in PuTTY, back up and
import the configuration), Save As and Set Defaults, site search, in-place
rename, drag and drop into folders, site colours and notes, URL splitting in
the host name box, and every Site Advanced page. Transfers and synchronization
show elapsed and remaining time and can skip, move to the background, limit the
speed and act once finished. Preferences can be searched, and the main window
gets the remaining navigation keys, history drop-downs and toolbar customizing.

## 0.5.0 — 2026-10-16

linscp's own menus and dialogs are translated into 53 languages and follow the
language chosen in Preferences > Languages (or `--lang=<code>`). Screenshots in
the README.

## 0.4.0 — 2026-10-15

The Explorer interface (single panel, folder tree and address bar), the
remaining Preferences pages (Environment, Interface with a Light/Dark/Automatic
theme, the Remote and Local panel pages, Drag & Drop, Updates), custom command
extensions, and on Windows the taskbar jump list, the shell context menu and the
drag-and-drop shell extension.

## 0.3.0 — 2026-10-15

The rest of the Commander menus: thumbnails, the command line, the queue
commands, upload/download, links, locking and tab colours. Portable mode (a
Windows zip, and an INI beside the AppImage). Fixes the update check crashing on
Linux and HTTPS failing in the Windows packages. Every protocol is now tested
against real servers.

## 0.2.0 — 2026-10-15

The first published pre-release: SFTP, SCP, FTP/FTPS, WebDAV and S3; sites,
workspaces and session tabs; the two-panel Commander interface; foreground and
queued transfers; synchronization and keeping a remote directory up to date;
the internal editor; Preferences; and `linscp-cli`, the scripting client.

## 0.1.0 — 2026-10-13

First tagged version (not published as a release).
