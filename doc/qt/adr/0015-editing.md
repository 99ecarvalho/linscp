# ADR 0015 — Files are edited as WinSCP edits them: its internal editor, and external editors later

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

Q-3 asked whether the shell should have an internal editor, as WinSCP
does, or open every file in the system's editor. WinSCP has both:

- an internal editor (`forms/Editor.cpp`), used by default;
- external editors, configured in Preferences > Editors and chosen by file
  mask.

For a remote file, WinSCP:

1. downloads it to a temporary directory of its own
   (`TemporaryDirectoryForRemoteFiles`), with the temporary files'
   transfer settings (`TemporaryFileCopyParam`), as text for a text
   editor;
2. uploads it back each time it is saved (`ExecutedFileChanged`), through
   the queue as a `TEditorUploadQueueItem` where the protocol has
   background transfers, after asking when the server's copy changed in
   the meantime (`EditorCheckNotModified`);
3. removes the temporary directory once the editor is closed and the
   uploads are done.

Core always took the local end of line to be CRLF
(`TConfiguration::GetLocalEOLType`), since it has only run on Windows. On
POSIX, a text-mode download then gave local files CRLF line endings.

## Decision

- **Both, as WinSCP.** The internal editor comes first (P3-05). External
  editors come with Preferences > Editors (P3-04e), where WinSCP keeps
  them.
- **The bridge does WinSCP's part of editing:**
  - `TSession::EditDownload` follows `TemporaryDirectoryForRemoteFiles`,
    with `TemporaryDirectory*` options mirrored from `TWinConfiguration`;
  - `TSession::EditUpload` follows `ExecutedFileChanged`, with the same
    queue item and question.

  `TEditedFile` carries what WinSCP's `TEditedFileData` carries: where the
  file came from, its temporary root and its source timestamp. The queue's
  thread may update the timestamp.
- **The internal editor, `EditorWindow`, is WinSCP's in Qt**
  (`QPlainTextEdit`):
  - WinSCP's actions and keys;
  - its find and replace (as the system's dialogs WinSCP's editor uses),
    go to line and status bar, with WinSCP's texts;
  - its encodings: UTF-8 (the default), else the system's code page as
    core names it. On POSIX, core's code page is Latin-1. A file that is
    not valid in the configured encoding is read in the other one, with
    WinSCP's message. A UTF-8 BOM means UTF-8, and is kept when saving.
  - Esc and F10 close it, asking to save a modified file.
  - The search and the editor's options are kept under
    `TWinConfiguration`'s `Interface\Editor` keys.
- **Line endings are kept as the file has them.** WinSCP's editor writes
  CRLF, its platform's. linscp's writes back what the file's first line
  ends with, so a local file edited in place keeps its line endings on
  either platform.
- **Core's local end of line is the platform's**
  (`core-local-eol-platform.patch`): LF on POSIX, CRLF on Windows, so
  upstream's behaviour is unchanged. Text-mode transfers convert to it.
- **Files > Edit (F4)** edits the active panel's focused file:
  - a local one in place;
  - a remote one through the bridge.

  An editor already open on the file is brought up instead. Closing the
  session, or the window, closes the editors first, each asking to save.
  Uploads from those saves go in the foreground, so that they finish
  before the session closes.

## Consequences

- Editing a remote file behaves as in WinSCP: the same temporary
  directories, transfer settings, queue item and question. A pin bump
  that changes these needs the bridge's copies checked again; they are
  marked with the upstream function they follow.
- The patch changes text-mode transfers on POSIX for the command-line
  client as well. That is the point: there, CRLF was never the local
  convention.
- **Not yet:**
  - reloading a remote file from the server (WinSCP's `OnFileReload`;
    Reload rereads the temporary copy);
  - "Save all", the editor's window position, font and colour;
  - the large-file warning;
  - WinSCP's "never ask again" on the encoding fallback;
  - opening a remote file with its associated application (WinSCP's
    Open);
  - Edit with…;
  - the cleanup confirmation for temporary directories left from a
    previous run (`ConfirmTemporaryDirectoryCleanup`).

## Verification

- `qt_session`/`qt_session_ftp`, the bridge's editing:
  - a download into a temporary directory of its own;
  - an upload back through the queue (SFTP) or in the foreground (FTP);
  - the question when the server's copy changed, declined and accepted.
- `qt_gui_edit`, the editor and F4:
  - a local file edited in place, with its CRLF line endings, BOM and
    Latin-1 encoding kept;
  - find, replace all (undone in one step), go to line, the status bar;
  - a remote file saved back through the queue, its temporary copy
    removed once closed;
  - editors closed with the session, saved first in the foreground.
- Natively on Windows 11, `qt_gui_edit` and `qt_session` too, where text
  mode gives CRLF.
