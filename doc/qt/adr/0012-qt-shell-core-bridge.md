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

## Amendment 2026-10-13: the remote panel (P2-06)

- `TSession` gains the remote operations that are not transfers --
  `HomeDirectory`, `Refresh`, `CreateDirectory`, `Rename`, `Delete` -- used
  as upstream's explorer uses `TTerminal`: `DeleteFiles` gets the entries'
  names with their `TRemoteFile`s, as the remote view's `CreateFileList`
  gives them (full paths are the local side's; a directory's full name ends
  in a slash, which FileZilla's `removedir` refuses), and `CreateDirectory`
  an empty `TRemoteProperties` (core reads it unconditionally).
- Core reports some failures itself rather than throwing -- a directory it
  cannot enter is shown and the terminal stays -- so the panel judges a
  directory change by the directory it shows afterwards.
- `TRemoteEntry` carries owner, group, link target and the modification
  time as a number, for the panel's columns and sorting.

Verified by `qt_session`/`qt_session_ftp` (the operations on rclone's SFTP
and the suite's FTP server) and `qt_gui_panel` (the panel: order, sorting,
new folder, rename, a two-entry delete with core's confirmation, the path
box, back, forward, parent, root, home, a directory that cannot be
entered): Linux, Wine, natively on Windows 11.

## Amendment 2026-10-13: transfers and the queue (P2-07)

- **Foreground transfers** run on the GUI thread, as WinSCP's: core's
  `CopyToRemote`/`CopyToLocal` with its default copy parameters, its
  progress reported through `IFrontend::TransferProgress`, whose `false`
  cancels at core's next report (`SetCancel(csCancel)`), and its questions
  (a file that exists) through the frontend as all of core's.
- **The queue** is core's `TTerminalQueue`, with its own connections on its
  own threads. Those threads never reach Qt: the queue's events only flag a
  change (an atomic). The GUI polls it from a timer (`TSession::QueueIdle`),
  which calls the queue's `Idle`, takes a fresh status, and -- as WinSCP's
  queue auto-popup -- calls `ProcessUserAction` on the first item waiting
  for an answer, which asks it on the GUI thread through the session's own
  handlers. Entries are named by their status proxies, which core keeps
  stable while an item is listed. Core drops done items at once unless told
  to keep them, so "finished" is counted as added minus still listed, which
  tells the panels to read their directories again.

Verified by `qt_session`/`qt_session_ftp` (foreground and queued uploads and
downloads, bytes compared, overwrite, cancel, move; the queue's connection
reusing the password) and `qt_gui_transfer` (F5/F6 between the panels, the
progress window's cancel, queued transfers the panels catch up with, a
queued upload cancelled from the queue panel): Linux, Wine, natively on
Windows 11.

## Amendment 2026-10-13: drag and drop (P3-07, first part)

- **What a panel drags** is its own MIME type, `application/x-linscp-files`:
  the side, the directory and the selected names. A local drag also carries
  the files' URLs, so other programs take it as a file drag.
- **A drop is handled after the drag ends.** The view reports the drop and
  the directory row it fell on. The main window copies the data and acts on
  it from the event loop, because the drag loop belongs to the source (on
  Windows, to the system's `DoDragDrop`), and a modal dialog or a transfer
  should not run inside it.
- **What a drop does follows WinSCP.** From the other panel it transfers,
  through the Copy dialog, as F5 does; with Shift it moves. Within a panel,
  a drop onto a directory moves the entries into it after asking. Remotely
  that goes through core's `MoveFiles`, as WinSCP's remote panel does,
  which also invalidates the target in core's directory cache. A rename
  into a subdirectory leaves the target's cached listing stale. Files from
  other programs upload when dropped on the remote panel.
- **Remote files dragged out to other programs** work as WinSCP's drag
  without its shell extension does. The drag offers the files' URLs in a
  temporary directory of its own (`RemoteFileDrag`), and downloads into it
  when the program dropped on asks for them. WinSCP downloads when the
  button is released (`QueryContinueDrag`); Qt has no such hook, but the
  data is produced on demand, and programs ask for it at the drop. Explorer
  reads the paths while the drag hovers, too. With a mouse button still
  down the paths are given without downloading, as WinSCP gives them up
  front. The panels never read a panel drag's URLs, so drops on them
  download nothing extra. The directories are kept while the window is
  open, because the program dropped on may copy from them after the drop.
  WinSCP deletes its after a delay.
- **No XDS** (X Direct Save), which TASKS had planned for Linux. It exists
  only on X11 (Wayland has none), and GNOME Files has dropped it. The
  on-demand URLs work on X11, Wayland and Windows alike.

Verified by `qt_gui_drop` (each kind of drop; drops delivered through the
view as the system sends them, with their questions answered; a remote
drag's URLs downloaded when asked for, once, and not for drops on the
panels): Linux, Wine, natively on Windows 11. Dragging out was checked by
hand with Explorer on Windows 11 and with Nautilus.

## Amendment 2026-10-13: synchronization (P3-03)

- **The bridge carries core's synchronization as it is.** It does not
  re-implement it. `TSession::SyncCollect` is core's `SynchronizeCollect`,
  with the params WinSCP's Synchronize dialog sets (`TSyncParams`, mapped as
  `TFullSynchronizeDialog::GetParams`, plus `spNoConfirmation` as the GUI
  adds) and core's default transfer settings rooted at the two
  directories. It returns the checklist (`TSyncChecklist` over
  `TSynchronizeChecklist`, its entries' actions in core's order).
- **Checking and reversing an entry update core's checklist at once**
  (`TSynchronizeChecklist::Update`, `Reverse`). WinSCP's dialog keeps them
  aside and updates on OK. The outcome is the same, because a cancelled
  checklist is never applied.
- **Applying** is core's `SynchronizeApply` with the progress window, or each
  checked entry queued (`SynchronizeToQueue`), as WinSCP's "Start" and
  "Start in Background". Timestamps are not queued, as in WinSCP.
- **Collecting runs on the GUI thread.** Core reports each directory pair to
  a small progress window, whose Cancel makes the callback return false.
  Core then aborts, which reaches the shell as an aborted `TCoreError`,
  and nothing is shown.
- **What core finds is shown as core finds it.** Remote-only files are
  listed as deletions even without "delete files", unchecked, as WinSCP's
  checklist shows them.
- **Not yet:**
  - in the Synchronize dialog: "selected files only", "checksum", transfer
    settings, and "use same options next time" (preferences, P3-04);
  - in the checklist: calculating sizes, finding moves, custom commands,
    and checking a directory's actions;
  - "Keep remote directory up to date".

Verified by `qt_session`/`qt_session_ftp` (collect, uncheck, apply, compare
again, reverse a delete into a download, queue, cancel) and `qt_gui_sync`
(the dialog's rules, the checklist's checks and reverse and counts,
Commands > Synchronize through both dialogs, "no differences", "start in
background"): Linux, Wine, natively on Windows 11.

## Amendment 2026-10-13: the Commander's keys, rename in place, columns (P3-02)

- **The keys are WinSCP's Commander keys**, from `NonVisual.cpp` and
  `TCustomDirView.KeyDown`. The panels' own actions carry:
  - F2 rename;
  - F7 new folder;
  - F8 and Del delete (Shift+Del and Shift+F8 the other way locally:
    for good instead of the trash, or the other way round);
  - Backspace, Alt+Up and Ctrl+PgUp parent;
  - Ctrl+\ root and Ctrl+H home;
  - Alt+Left and Alt+Right back and forward;
  - Ctrl+R refresh.
- **The view (`FileView`) carries the selection keys and Tab:**
  - Insert toggles the focused entry and moves down;
  - keypad + and - select and unselect by mask, keypad * inverts;
  - Ctrl+A selects all, Ctrl+Shift+L clears;
  - Ctrl+F3, F5, F6, F7, F8 and F9 sort by name, time, size, rights,
    owner and group (the same key again reverses);
  - Tab goes to the other panel.
- **The main window** adds F10 to quit and Ctrl+[ and Ctrl+] to copy the
  local and remote paths.
- **Masks are core's.** `CoreBridge::MaskMatches` is `TFileMasks` as
  `TCustomScpExplorerForm::DirViewMatchMask` uses it, so selecting by mask
  takes WinSCP's whole syntax (exclusions, size and time conditions).
  "Apply to directories" matches a directory by its name alone, as
  `TUnixDirView::ItemMatchesFilter` does.
- **Renaming is in place**, as WinSCP's Commander does it. F2 edits the
  name in the list. The model turns a committed edit into a rename
  request, which the panel carries out after the editor has closed,
  because it lists the directory again. A double click still opens.
- **Each panel has a status line**, as WinSCP's (`FileStatusBarText`):
  - the selected entries' size of all, and how many of how many;
  - how many are hidden;
  - in core's texts (`FILE_INFO_FORMAT`, `FILE_INFO_HIDDEN2`), with sizes
    formatted as WinSCP's `FormatBytes` does in its default style.
- **Columns:** group and link target join the others, hidden until shown
  from the header's menu.
- **Not yet:**
  - Shift+F5 and Shift+F6 (copy or move only the focused entry);
  - the extension column and its sort (Ctrl+F4);
  - incremental search in the status line;
  - toolbars to customize;
  - columns and sorting kept between runs (preferences, P3-04).

Verified by `qt_gui_keys` (each key pressed through the window as a user
presses it, rename in place, the status line and `FormatBytes`, the columns'
menu) and `qt_gui_panel` (rename in place remotely): Linux, Wine, natively
on Windows 11.
