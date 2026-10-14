# ADR 0014 — The shell's settings are upstream's GUI configuration, compiled as it is

**Status:** accepted 2026-10-13
**Date:** 2026-10-13

## Context

Preferences (P3-04) need somewhere to keep the GUI's settings. WinSCP keeps
them in core's own configuration storage (the registry, or `WinSCP.ini`),
under `Configuration\Interface\...`, through two classes over core's
`TConfiguration`:

- **`TGUIConfiguration`** (`windows/GUIConfiguration.cpp`, about 1,400
  lines): transfer settings presets, synchronization and queue options,
  the language and its translation, PuTTY's path. It depends on core and
  on little of the GUI.
- **`TWinConfiguration`** (`windows/WinConfiguration.cpp`, about 4,000
  lines): the panels, Commander, Explorer, editors, windows, security.
  It is bound to the VCL GUI (the terminal manager, the drag extension,
  drive views, fonts).

A spike compiled `GUIConfiguration.cpp` from the transformed tree: 23
errors on Linux and 11 on Windows. All were the locale and
translation-module calls, the VCL's `Languages()` and `Screen`, and one
property subscript.

## Decision

- **`GUIConfiguration.cpp` is compiled as it is**, from the transformed
  tree. The transform's scope takes single files as well as directories,
  so only this unit and its header come from `windows/`. A line-preserving
  patch (`gui-configuration-property-subscript.patch`) makes one
  string-list subscript compile under clang.
- **`port/gui` builds it**, VCL side and without Qt, through a linscp
  stand-in for the GUI's precompiled header, `FormsPCH.h`. Upstream's
  would bring the whole VCL GUI; linscp's brings core's PCH, `TextsWin.h`
  and `GuiShim.h`. The stand-in alone redirects the unit's
  `LoadLibraryEx`/`FreeLibrary`, which load nothing but translations as
  data files, to the shim's translation modules (ADR 0013).
- **`GuiShim` supplies what the unit asks of the VCL and Windows:**
  - Delphi's integer `HINST`;
  - `TLanguages`/`Languages()`, Windows' locale list on both platforms,
    so translations match locales the same everywhere;
  - `Screen` (no VCL forms);
  - `ResStringCleanupCache`;
  - `KnownFolderPath`;
  - on Linux, `GetLocaleInfo`, `GetUserDefaultUILanguage` (from `LC_ALL`,
    `LC_MESSAGES`, `LANG`) and the `LANGID` macros.
- **The locale data is Windows' own.** `port/tools/windows_locales.py`
  dumps it on Windows (`EnumSystemLocales` and `GetLocaleInfo`) into
  `port/gui/locales.inc`, because upstream matches translations by Windows'
  three-letter abbreviations, so a reconstruction would drift.
- **The shell's configuration is a `TGUIConfiguration`**:
  `TShellConfiguration`, made by the shell's `CreateConfiguration` as
  upstream's `UserInterface.cpp` makes `TWinConfiguration`, which it plays
  the part of. The command-line client keeps core's own `TConfiguration`.
  The storage set-up (`/ini`, `/rawconfig`, POSIX's INI) is shared.
- **What the shell keeps of `TWinConfiguration`'s settings, it keeps
  under `TWinConfiguration`'s keys and defaults** (`Interface\LocaleSafe`
  first). A `WinSCP.ini` therefore carries over in both directions.
- **The language is chosen as upstream chooses it:** the saved one, else
  the user interface's. The translations are in `Translations/` beside the
  executable, upstream's folder, as `linscp.<LANG>`, upstream's naming
  after the executable. `--lang` applies a locale for one run without
  saving it. The shell's and Qt's texts follow the locale core applied.

## Consequences

- **Upstream's code decides the settings that matter most**:
  - the presets and their storage;
  - the synchronize parameters the Synchronize dialog remembers;
  - the queue's options;
  - the language and its fallbacks (Brazilian Portuguese's `PTB` finds
    `PT`; Traditional Chinese's `CHT` finds `CH`).

  A pin bump brings upstream's changes to them.
- **`TWinConfiguration`'s settings are mirrored, not compiled.** Each one
  the shell adopts is a key, a default and a load/save line in
  `TShellConfiguration`, checked against upstream's `REGCONFIG`.
- **WinSCP's "additional languages"** (Esperanto, Kabyle), which upstream
  lists through the translation DLL's version information, are not listed:
  linscp's translation files carry none.

## Verification

- `qt_session_pt_BR` and `qt_language`: Brazilian Portuguese chosen and
  loaded through upstream's `LoadNewResourceModule`.
- By hand, on Linux and natively on Windows 11:
  - the system's language by default;
  - a saved `LocaleSafe` over it;
  - `--lang` for one run, leaving `LocaleSafe` as saved;
  - the INI written with upstream's `Interface` keys.

## Amendment 2026-10-13: the Preferences dialog's first pages (P3-04a)

- **The dialog is WinSCP's.** Options > Preferences (Ctrl+Alt+P) lists its
  pages on the left. The first ones are WinSCP's Panels, Commander and
  Languages, with WinSCP's captions (so its pt_BR translation's wording),
  showing only the settings the shell honours. OK saves, as upstream's,
  with `Configuration->SaveExplicit`.
- **Panels:**
  - show hidden files, which Ctrl+Alt+H also toggles and keeps;
  - "default directory is home": Home is the site's own directories when
    the site does not remember the last used ones, as
    `TUnixDirView::ExecuteHomeDirectory` and `TScpCommanderForm` do;
  - select the whole name when renaming (otherwise the name without its
    extension is selected);
  - natural order numerical sorting;
  - always sort directories by name;
  - the sizes' format: bytes, kilobytes (the default), or short, as
    `FormatPanelBytes`.
- **Commander:** swap panels, and Explorer's keyboard shortcuts (Ctrl+K
  copies and F5 refreshes, as `NonVisual.cpp` assigns them).
- **Languages:** `TGUIConfiguration::Locales`, the one chosen focused. As
  `TPreferencesDialog`, choosing the default (the user interface's) saves
  "none", so a translation of it found later is taken. The language applies
  on the next start: once the shell's window exists, the GUI shim's
  `Screen` has a form, so upstream's `CanApplyLocaleImmediately` is false,
  and the page says so as upstream's does.
- **Settings are mirrored from `TWinConfiguration`'s `REGCONFIG`**:
  `Interface`'s `ShowHiddenFiles`, `FormatSizeBytes`,
  `NaturalOrderNumericalSorting`, `AlwaysSortDirectoriesByName`,
  `RenameWholeName` and `DefaultDirIsHome`; `Interface\Commander`'s
  `SwappedPanels` and `ExplorerKeyboardShortcuts`. Each has upstream's
  default.
- **Sorting is `TUnixDirView`'s `CompareFile`:** the column (directories
  by name when asked), then the name, all of it reversed for descending.
  Natural order compares as `CompareLogicalText` does, through Qt's
  numeric collation rather than a call into core per comparison.
- **A test program uses the shell's translations** by naming the shell's
  executable beside them (`TShellConfiguration::ModuleFileName`), so
  upstream's own lookup lists and loads them.
- **Not yet, on these pages:**
  - incremental search;
  - "remember panels' state when switching sessions";
  - full row select;
  - the double-click action;
  - the panel font;
  - Explorer-style selection;
  - the directory tree;
  - location profiles;
  - compare criteria.

  They depend on features the shell does not have yet, and are not shown
  until it does.

Verified by `qt_gui_preferences` (WinSCP's defaults; the pages; each
setting applied to the panels and saved under WinSCP's keys; the language
waiting for the next start; Ctrl+Alt+H kept) and `qt_gui_sites` (Home as
the site's directory): Linux, Wine, natively on Windows 11.

## Amendment 2026-10-13: transfer settings, presets, background, endurance (P3-04b)

- **Transfers use the GUI's transfer settings**, as WinSCP's do. Before,
  they used core's bare `TCopyParamType`. Uploads, downloads, queued
  transfers and synchronization now take `GUIConfiguration`'s
  `CurrentCopyParam` (the preset in use, else the default), or a transfer's
  own when its dialog changed them. The bridge's `TTransferSettings` is
  `TCopyParamType` with `TGUICopyParamType`'s background options, mapped
  field for field and written back over the parameters it came from, so
  what the shell does not edit stays.
- **Queued transfers** add `cpNoConfirmation` and use several connections
  as the settings say (`AddQueueItem`). The queue takes its transfers
  limit and how long completed items stay listed as
  `TTerminalManager::NewQueue` does. Questions pop up only when "popup
  prompts when idle" is on.
- **The Copy and Synchronize dialogs** show WinSCP's Transfer settings group:
  - the summary (`GetInfoStr`);
  - the preset to use, which becomes the current one, as WinSCP's preset
    menu has it;
  - "Transfer settings..." for this transfer alone, through the frame
    WinSCP's dialogs share (`CopyParams.dfm`).

  "Transfer in background" defaults to the settings' "background by
  default".
- **Preferences gains three pages:**
  - Transfer: the default and the presets, starting with upstream's own
    four (`DefaultLocalized`); add, edit, duplicate, remove, reorder, and
    `COPY_PARAM_DUPLICATE` for a name taken. A preset kept by name keeps
    its autoselection rule.
  - Background: core's `QueueTransfersLimit`; the GUI's
    `QueueKeepDoneItems`/`For` (WinSCP's combo of durations) and
    `QueueAutoPopup`; the default settings' `Queue`,
    `QueueNoConfirmation` and `QueueParallel`.
  - Endurance: the default settings' resume, and core's and the GUI's
    reconnect delays, mapped as `TPreferencesDialog` maps them (seconds;
    a delay that is off shows upstream's default).
- **Not yet:**
  - presets' autoselection rules and their announcement;
  - the parallel transfer threshold;
  - "enable queue processing by default";
  - the queue list's visibility.

Verified by `qt_session`/`qt_session_ftp` (the default and a transfer's own
settings followed, a preset in use followed) and `qt_gui_preferences` (the
pages, presets edited, each setting saved under WinSCP's keys in
`Interface` and `Interface\CopyParam`, the Copy dialog's group): Linux,
Wine, natively on Windows 11.

## Amendment 2026-10-13: Window, File colors, the layout kept (P3-04c)

- **Window:**
  - the path in the title, short (its last part, the root as is), full,
    or none, as `TTerminalManager::UpdateAppTitle` and `PathForCaption`
    put it before the session and the program;
  - "keep main window open when the last session is closed": off, a
    disconnect closes the window.

  WinSCP's two other settings on this page are not here yet: "minimize to
  the tray" and "open externally initiated sessions in the existing
  window", which needs a single instance.
- **File colors:** `FileColors`, kept as `TFileColorData::SaveList` keeps
  it (comma text of `BBGGRR:mask`), with colors given to Qt as RGB. As
  `DirViewGetItemColor`, the first rule whose mask a file's full path
  matches (explicitly, through `CoreBridge::MaskMatchesPath`) colors it.
- **The layout is kept between runs in WinSCP's own formats and keys**,
  restored as the window is made and saved as it closes:
  - each panel's columns: `Interface\Commander\LocalPanel` and
    `RemotePanel`'s `DirViewParams`, `TIEListViewColProperties`' params
    (sort; width and visibility per column at the pixels per inch saved;
    the order), mapped onto WinSCP's columns. Those are nine remote and
    six local; the local panel's owner, group and link target are not
    among them. WinSCP's columns linscp does not have (extension, type)
    are written back as they were read;
  - the window's placement (`WindowParams`, as `StoreForm`: on its
    monitor, maximized kept, `-1;-1;width;height` before it is placed;
    the default as `TWinConfiguration::Default`'s);
  - the local panel's share of the width (`LocalPanelWidth`);
  - the active panel (`CurrentPanel`);
  - the queue's height (`Interface\QueueView`).
- **The Explorer page is not offered.** It configures WinSCP's Explorer
  interface, which the shell does not have.
- **Pages are in the order of WinSCP's tree:** Window, Commander,
  Languages, Panels, File colors, Transfer, Background, Endurance.

Verified by `qt_gui_preferences` (the pages; file colors applied and kept;
the layout written in WinSCP's formats, the column linscp lacks kept, and
restored by a new window; the window closing with its session when not
kept open) and `qt_gui_panel` (the path in the title): Linux, Wine,
natively on Windows 11.

## Amendment 2026-10-14: core's pages, Network, Security, Logging, Storage (P3-04d)

- **Four of core's pages**, after Endurance in the order of WinSCP's tree,
  loaded and saved as `TPreferencesDialog` does:
  - Network: the external IP address for active FTP, the listening ports
    (none set shows the private range), knocking the FTP port when SFTP is
    rejected;
  - Security: passwords remembered for the session, and the
    authentication agent;
  - Logging: the session log (level, file, append or overwrite, sensitive
    information, rotation by size as WinSCP's dialog types sizes, through
    core's `TryStrToSize`/`SizeToStr`, files kept) and the XML log. The
    log path shows core's default when none is set, and logging is on
    only with a file, as upstream's;
  - Storage: where the configuration is, and the temporary directory and
    random seed file. On Windows without `/ini` the configuration can be
    moved between the registry, the automatic INI file and a custom one
    (`MoveStorage`). A file given on the command line, and on POSIX the
    INI file always (ADR 0009), is shown, with nothing to move.
- **The temporary directory** is `TWinConfiguration`'s `DDTemporaryDirectory`,
  mirrored. The shell's `TemporaryDir`, which core uses, and the drag-out
  directories go under it as `TWinConfiguration::TemporaryDir` does.
- **Upstream's defaults now work on POSIX:**
  - the Win32 layer's `ExpandEnvironmentStrings` gives `TEMP`, `TMP`,
    `APPDATA`, `LOCALAPPDATA` and `USERPROFILE`, which Windows always
    defines, their POSIX equivalents when the environment does not set
    them;
  - a patch (`core-log-file-name-delimiter.patch`) builds the default
    log names (`%TEMP%\!S.log`, `!S.xml`) with the platform's delimiter,
    unchanged on Windows, so the log goes into the temporary directory
    rather than beside it. It is a candidate to offer upstream
    (ADR 0006).
- **Not offered:**
  - Updates, which checks winscp.net for WinSCP's releases and sends
    WinSCP anonymous usage statistics, neither of which is for linscp;
  - for now, the master password (`TWinConfiguration`'s encryption of
    stored passwords) and the trusted host certification authorities'
    list, which want their own dialogs;
  - the temporary directory's cleanup, session and path options, which
    belong with editing (P3-05).

Verified by `qt_session`/`qt_session_ftp` (logging on writes the session
and XML logs; the default path in `TMPDIR` where `TEMP` is not set),
`win32_on_posix` (the variables' fallbacks) and `qt_gui_preferences` (the
pages, saved under core's `Logging` and `Interface` keys, the temporary
directory in use): Linux, Wine, natively on Windows 11.

## Amendment 2026-10-14: slices of the VCL-bound units (P3-06d)

WinSCP's custom commands (`TCustomCommandType`, `TCustomCommandList`)
live in `windows/WinConfiguration.cpp`, and `TLocalCustomCommand` in
`GUITools.cpp`, among code bound to the VCL GUI. About 930 lines of them
use core only. Mirroring them by hand, as the settings are, would copy
their logic, including the extensions' parsing, and let it drift. The user
chose to compile them as they are.

- **The transform cuts slices** (`port/tools/slices.json`):
  - each slice is a range of an upstream file, from the one line its
    `from` anchor matches up to its `until` anchor, or the end;
  - each anchor must match exactly once, so a pin bump that moves or
    changes the code fails the transform rather than cutting the wrong
    lines;
  - a slice is taken from the transformed file, which must be in the
    transform's scope, or from the pristine one where the code needs no
    rewriting.
  - Each is written after a linscp prologue, with `#line` naming
    upstream's file and line, so diagnostics point at upstream's code.
- **The slices live outside the mirror** (`build/…/gen-slices`).
  `verify_transform.py`'s check is unchanged: the generated tree stays a
  file-for-file mirror. `WinConfiguration.cpp` and `.h` join the
  transform's scope only to be sliced; they are not compiled.
- **Today's slices:**
  - `TCustomCommandType` and `TCustomCommandList`, with their header
    part and `ExtractExtensionBaseName`;
  - `TLocalCustomCommand`;
  - `UserInterface.cpp`'s `NormalizeCustomShortCut`, `IsCustomShortCut`
    and their constants.
- **What surrounds them is linscp's:**
  - `port/qt/CustomCommandsSlice.h`: the headers they need, and
    `WinConfiguration`, which is the shell's `TShellConfiguration`. It
    gains `ExtensionStringTranslation`, which does not translate yet.
  - Stand-ins for `Setup.cpp`'s .NET and PowerShell version probes. They
    find none, so an extension requiring one is not offered.
- **The shim gains what they ask of the VCL:**
  - `Vcl.Menus`' `ShortCut`, `ShortCutToText` and `TextToShortCut`, in
    Delphi's text form;
  - `TList::SortList` with `TListSortCompareFunc`;
  - `AnsiCompareStr`.
- **The commands are kept as `TWinConfiguration` keeps them.**
  `TShellConfiguration` holds the list, with WinSCP's defaults
  (`DefaultLocalized`) until changed. It stores them in `CustomCommands`
  (or `CustomCommandsNone`) and `CustomCommandOptions`, so a `WinSCP.ini`
  carries over both ways.

Running them follows `TCustomScpExplorerForm::CustomCommand` and
`RemoteCustomCommand`, over core's `TRemoteCustomCommand`,
`TInteractiveCustomCommand` (the prompts, the frontend's) and
`CustomCommandOnFiles`, in the protocol's shell or a separate shell
session. Local commands, extensions and their options are not ported yet.

Verified by `qt_gui_commands`, against OpenSSH's sshd over SCP, which has
a real shell: on Linux, and under Wine against the host's sshd. On native
Windows the test skips, since Windows' sshd lets no one in when run
unprivileged.

## Amendment 2026-10-14: keep-up-to-date's controller (P3-06e)

WinSCP's keep-up-to-date is `windows/SynchronizeController.cpp`'s
`TSynchronizeController`. It uses core only, apart from the Delphi
component it watches with, `TDiscMonitor` (`packages/my/DiscMon.pas`).
It is compiled as it is, as a slice of the whole file after its includes.

- **The slice.** `SynchronizeController.cpp` and `.h` join the
  transform's scope. The `.cpp` is sliced from `#include <RemoteFiles.h>`
  on, after `port/qt/KeepUpToDateSlice.h`, which stands for `WinPCH.h`. The
  header is used from the mirror as it is.
- **`TDiscMonitor` is linscp's** (`port/qt/DiscMon.hpp`), with the
  members the controller uses. It lists the directories with upstream's
  `AddDirectory` logic, filter and "too many directories" question
  included. The watching itself is `TDirectoryWatcher`'s, which the
  command-line client's `keepuptodate` already used; it moves to
  `linscp-frontend-common`.
  - Upstream's monitor runs a thread of its own and fires its events
    through `TThread::Synchronize`. Linscp's has no thread: the dialog's
    timer polls it (`TDiscMonitor::Pump`), on the GUI thread, where the
    events then fire.
  - An event may close or free the monitor, as the controller's abort
    does; polling stops at once when one does.
- **Closure-site discovery covers the slices.** `bind_closures.py` also
  compiles each slice cut from a transformed file. The diagnostics name
  upstream's file through `#line`, so the sites are recorded, and bound,
  in the mirror's file, like core's. There are 5, all in the controller's
  `StartStop`.
- **The shim gains** `TComponent`, only as far as its owner, and
  `TThreadMethod`.
- **What surrounds the controller follows WinSCP:**
  - `TCustomScpExplorerForm`'s `DoSynchronizeDirectories`,
    `DoSynchronize`, `Synchronize` and `GetSynchronizeOptions`, ported in
    the bridge;
  - `TSynchronizeDialog`'s log, abort and "continue on error". With
    "continue on error", errors go to the log, as `MoreMessageDialog` sends
    them there.
  - Uploads and deletes are logged from the terminal's `OnFinished`, as
    the GUI's `FileOperationFinished` logs them.

Verified by `qt_gui_keep` on Linux, under Wine and on native Windows. Under
Wine a watched directory cannot be deleted, so the test's "watched
directory gone" step runs only where it can.

## Amendment 2026-10-14: the editor preferences (P3-04e)

`TEditorData`, `TEditorPreferences` and `TEditorList` are sliced from
`WinConfiguration.h` and `.cpp`, as the custom commands are. The `.cpp`
slice's prologue repeats that file's `NotepadName`, a static above the
slice. The shim gains `UnicodeString::ByteType`. The shell's configuration
keeps the list as `TWinConfiguration` does (ADR 0015 amendment).

## Amendment 2026-10-14: Integration (P3-04e)

Preferences > Integration on Windows is upstream's `windows/Setup.cpp`.
That covers the search path, and registering the program as the protocols'
URL handler, undoing it, and opening the default apps settings. It is a
slice compiled on Windows only, through `port/qt/SetupSlice.h`, whose
stand-ins are:

- `Application->ExeName`;
- `ShellOpen`, through the frontend;
- `SimpleErrorDialog`.

The shim's `TRegistry` gains `HasSubKeys`. The desktop icon on Windows is a
shortcut Qt makes (`QFile::link`), without upstream's COM code. On POSIX the
same actions use XDG instead (`port/qt/Integration.cpp`):

- a desktop entry for the protocols' `x-scheme-handler` types, with
  upstream's `/Unsafe %u`;
- the default for them in the user's `mimeapps.list`, the other entries
  kept;
- a desktop icon;
- links in `~/.local/bin` for the search path.

"Send to" is not ported: it needs the command line's `/upload`.

## Amendment 2026-10-14: Open in PuTTY (P3-08)

Commands > Open in PuTTY is upstream's `OpenSessionInPutty`, compiled as it
is, with its helpers from the same part of `windows/GUITools.cpp`:
`FindFile`, the PuTTY session export, its cleanup thread, and the password
pipe.

- **The slice.** `GUITools.cpp` joins the transform's scope only to be
  sliced. Its slice runs from `FindFile` to the operation status window
  that follows `OpenSessionInPutty`.
  - The transform now notes, rather than fails on, what it cannot handle in
    a file it transforms only for slices, when that is outside every
    slice. Such lines are never compiled. Here that is a `__property`
    republished in a VCL toolbar class.
  - Slices may have an epilogue: linscp's lines after upstream's. This one
    has `PuttySessionsFinalize`, the `TPuttyCleanupThread::Finalize` that
    upstream's `GUIFinalize` calls, which the bridge's `Finalize` calls
    before core's.
- **What surrounds it is linscp's** (`port/qt/PuttySlice.h`):
  - `ExecuteShellChecked`, over the frontend's new `StartProgram`
    (`QProcess::startDetached`; on Windows with the command line as
    given);
  - `TWinInteractiveCustomCommand`, the bridge's prompts (moved to
    `BridgeCustomCommand.h`);
  - `FileExistsFix` (PasTools.pas).
- **On POSIX the session goes on PuTTY's command line.** PuTTY there keeps
  its sessions in files under `~/.putty`, which the registry export cannot
  reach. The slice's prologue has `IsUWP()` true on POSIX, so upstream's own
  branch for the Store's PuTTY, which has the same limit, builds the
  command line.
  - The password pipe has no POSIX counterpart: the platform layer's
    `CreateNamedPipe` fails. With PuTTY's auto setting, a program not named
    `putty.exe` gets `-pw`, as upstream gives it to PuTTY before 0.77.
- **The default program on POSIX** (`TShellConfiguration::Default`) is
  `putty` where it is installed. Otherwise it is OpenSSH's `ssh` in the
  desktop's terminal, in the current remote directory, written with WinSCP's
  session patterns. Upstream adds no PuTTY options to a site command like
  that. On Windows the default is upstream's.
- **The shim gains** `FileSearch` and `GetFileVersion`. The platform layer
  gains the named-pipe API (failing) and `FlushFileBuffers` (`fsync`).

Verified by `qt_gui_tools`: the program started with the session, the
command line on Linux and an exported PuTTY session on Windows (under Wine
and natively); a site command with its patterns completed; and a program
not found reported.
