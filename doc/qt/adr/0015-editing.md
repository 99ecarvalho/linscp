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
  - the large-file warning (done: see the amendment below);
  - WinSCP's "never ask again" on the encoding fallback;
  - opening a remote file with its associated application, and Edit
    with… (done: see the amendment below);
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

## Amendment 2026-10-14: external editors and the associated application (P3-04e)

Preferences > Editors lists the editors files open in, as WinSCP's. F4
uses the first editor whose mask matches the file, else the internal one.
Files > Edit With offers each editor, or one asked for, which may be
remembered. Files > Open uses the file's associated application.

- **The list is upstream's.** `TEditorData`, `TEditorPreferences` and
  `TEditorList` are compiled as they are, from slices of
  `WinConfiguration.h` and `.cpp` (ADR 0014). The shell's configuration
  keeps the list where `TWinConfiguration` keeps it: the subkeys of
  `Interface\Editor`.
  - The default list is upstream's when nothing is kept: the internal
    editor, then Notepad. On POSIX the second is the associated
    application.
  - WinSCP 4's single-editor keys are not read.
- **Starting an editor is upstream's** (`CustomExecuteFile`): the command
  is split, its `!.!` becomes the file or the file is appended, and it
  runs through the frontend's `StartProgram`. The associated application
  is `QDesktopServices::openUrl`.
- **A remote file's copy is watched as TEditorManager watches files with
  no process.** It is downloaded to a temporary directory, with text mode
  where the editor asks for it. Each change of its time is a save,
  uploaded, as the internal editor's are, with "check not modified". The
  copy is kept until the session closes. Then its last change is uploaded
  in the foreground, and its directory is removed with the internal
  editors'.
  - The shell does not wait on the editor's process, which upstream does
    for an SDI editor. So the copy outlives the editor's window until the
    session ends, as it does in WinSCP for an MDI editor or the
    associated application.
- **The internal editor's own page** sets the font, the text and
  background colours, the tab size, the encoding and word wrap, as WinSCP's
  Internal editor page does. The large-file question is also asked, with
  "never ask again".
- **Not yet:** detecting an MDI editor and the "editor closed too early"
  question; "Use default system editor".

Verified by `qt_gui_edit` (an external editor chosen by mask, given
`!.!`; its copy's change uploaded; Open on a local file; the copy gone
with the session) and `qt_gui_preferences` (the pages; the editor in the
font and colours; the large-file question). Both run on Linux, under Wine
and on native Windows.

