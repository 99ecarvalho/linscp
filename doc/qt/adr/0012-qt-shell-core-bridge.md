# ADR 0012 — The Qt shell reaches core through a bridge, never in one unit

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

The Qt shell (P2) drives core, whose headers need the VCL shim: `vcl.h`,
`__property` as `__declspec(property)`, Win32 names and types from the
platform layer on POSIX, `TObject`, `UnicodeString`, and macros upstream
expects. Qt's headers bring their own world: `emit`/`signals`/`slots`,
`QT_*` macros, and on Linux a 4-byte `wchar_t` (ADR 0001). Either can
redefine or shadow the other's names, and the `wchar_t` widths cannot both
hold in one translation unit.

## Decision

- Qt translation units never include the shim or upstream headers; core's
  never include Qt.
- `port/qt/CoreBridge.h` is the only interface between them: plain C++,
  strings as UTF-8 `std::string`, errors as `std::runtime_error`.
  `CoreBridge.cpp` is its VCL side, compiled like any core unit.
- `port/qt/CMakeLists.txt` enforces the split: `linscp-qt-ui` (Qt, moc
  output included) and `linscp-qt-core` (the bridge) are separate targets
  with their own compile options.

## Consequences

- Each core facility the shell needs is a bridge function first. The bridge
  will grow with P2-03 (`Interface.h`'s prompts, errors and progress have to
  reach Qt dialogs) and the file panels; keep it narrow and value-typed.
- Callbacks from core to the shell (core threads, `Interface.h`) go through
  the bridge too, as plain function objects; the Qt side marshals them onto
  its own thread.

## Verification

ctest `gui_smoke` starts core through the bridge, shows the main window and
quits from the event loop, on Linux (offscreen) and on Windows under Wine;
`windows_imports` checks the shell's imports resolve against the DLLs it
ships with.

## Amendment 2026-10-13: core's questions (P2-03)

How the bridge carries `Interface.h` and the terminal's events:

- **Core runs on the GUI thread, as in WinSCP.** A session opens, lists and
  changes directory synchronously on it. While it blocks, core calls
  `ProcessGUI`, which pumps Qt's event loop (`IFrontend::ProcessEvents`) on
  the GUI thread only, at most every `GUIUpdateInterval` unless forced, as
  upstream's does. A question is a modal dialog shown from within the
  operation. Background transfers (P2-07) will marshal their questions onto
  this thread, as upstream's queue does.
- **`Interface.h` is split by program.** `port/frontend/FrontendCommon.cpp`
  holds what every program shares (configuration, registry keys, names,
  threads); `HeadlessInterface.cpp` (stderr, no event loop, no clipboard)
  is linked by the CLI and tests through `linscp-runtime`;
  `port/qt/QtInterface.cpp` is the shell's, linked through
  `linscp-runtime-base`. Only one of the two is ever in a program.
- **`IFrontend`** is what the shell implements: `Query`, `Prompt`,
  `ShowError`, `Banner`, `Information`, busy, event loop, clipboard. It
  receives plain values built on the VCL side the way upstream's dialogs
  build theirs:
  - queries as `MoreMessageDialog` does: one button per answer in
    upstream's order and captions (`AnswerNameAndCaption`), aliases
    applied, `CancelAnswer` for Escape. An alias's `OnSubmit` becomes a
    callback the dialog runs without closing unless it gives an answer, as
    `TMessageForm::ButtonSubmit` does ("Copy key", "Paste key"). A ticked
    "Never ask again" on a positive answer is `qaNeverAskAgain`
    (`WinInterface.cpp`'s rule), and timeouts count down on their answer;
  - prompts with each field's echo flag and starting value; a URL with no
    host asks for one, as the script's `open` does;
  - errors through `ExceptionMessageFormatted`, aborts not shown. A
    cancelled credentials prompt is not an abort: core fails with its own
    "Credentials were not specified", which is shown, as in WinSCP.
- **Not wired yet:** `TQueryParams`' periodic timer events (the reconnect
  countdown), file-transfer progress (P2-07), and Help buttons (there is no
  help system).

Verified by `qt_session` and `qt_session_ftp` (the VCL side against rclone's
SFTP server and the suite's FTP server, a scripted frontend), `qt_dialogs`
(the dialogs alone), and `qt_gui_session` (the main window opening a
session, every question answered in its dialogs by a responder): on Linux,
under Wine, and natively on Windows 11.

## Amendment 2026-10-13: stored sites (P2-04)

- **`TSite`** is the site as WinSCP's Login dialog edits it: protocol and
  encryption, server, credentials, remote directory. Everything else a
  stored site holds survives a save, because `SessionDataOf` starts from the
  stored site (or core's defaults) and applies only those fields.
- **Storage is core's**, used as upstream uses it: `NewSession` plus
  `Save(false, true)` (modified only, explicit) to save; `Remove` then
  `NewSession` to rename; `TSessionData::Remove` plus the list's `Remove` to
  delete. Saving under another name copies, as "save as" does. A special
  site is not overwritten (`SessionNameValidate`); the dialog asks before
  overwriting another.
- **`TText`** hands the shell a curated set of core's own strings, the
  ones its WinSCP-like dialogs show ("New Site", the save and delete
  confirmations, the TLS choices). They reach the user in WinSCP's wording
  and WinSCP's translations, rather than being retyped as the shell's own.

Verified by `qt_sites` (saved in one process, read back in the next, the
stored site opened with its saved password, save-as, rename, delete over an
INI file) and `qt_gui_sites` (the site manager: filled in, saved, read back,
its port following protocol and encryption, logged in from, deleted): on
Linux, under Wine and natively on Windows 11.

## Amendment 2026-10-13: advanced settings and keys (P2-05)

- `TSite` carries the advanced settings the shell offers -- directories,
  timeout, FTP passive mode, a SOCKS4/SOCKS5/HTTP proxy (with "consult
  proxy for local addresses"), private key file, agent, keyboard-interactive,
  agent forwarding -- in the dialog `SiteAdvanced`, after upstream's Site
  Advanced pages. A proxy method the dialog does not offer (telnet, local
  command, SSH tunnel) is kept as stored. The proxy's port stays as set when
  its type changes, as upstream's.
- **Keys** are checked and converted as upstream's `DoVerifyKey` and
  `ConvertKey` do, over core's `PuttyTools` (`port/qt/Keys.cpp`): on login
  and save, an OpenSSH or ssh.com key is offered for conversion to a `.ppk`
  (its passphrase asked through the frontend, a matching certificate added),
  and one that cannot be used is warned of, with Ignore and Abort. The two
  helpers upstream keeps in GUI code (`GetConvertedKeyFileName`,
  `AddMatchingKeyCertificate`) are carried over.
- **Folders** are the sites' paths, as in core: a new folder lives in the
  dialog until a site is saved into it; renaming one renames each of its
  sites; deleting one deletes them, after core's confirmation naming how
  many.

Verified by `qt_sites` (advanced settings persist; a site through
microsocks requiring a password, which logs the connection; an OpenSSH key
converted and logging in to sshd) and `qt_gui_sites` (folders made, saved
into, renamed, deleted; the advanced dialog's settings persisting; a key
converted through the site manager): on Linux, under Wine and natively on
Windows 11, where the key phase runs against Windows' own OpenSSH server.
