# WinSCP functionality linscp did not have (now closed)

> **Status: all 47 gaps below are implemented** as of 2026-10-18
> ([gaps_tracker.md](gaps_tracker.md): 47/47 done). This file is kept as the
> record of what each gap was and how it was closed; the descriptions are
> written in the present tense of when they were open.

Found 2026-10-16/17 at WinSCP 6.7.1 (`e19a9a3e`). Status per item is in
[gaps_tracker.md](gaps_tracker.md); this file says what each gap is and how
to close it.

**How it was found.** Every caption of WinSCP's 47 forms
(`3rd-party/winscp/source/forms/*.dfm`) was looked up in linscp's sources:
linscp reuses WinSCP's captions word for word, so a missing caption marks a
missing feature. Then each candidate was confirmed by reading both codes,
including the command-line switches (`windows/WinMain.cpp`,
`windows/ConsoleRunner.cpp`). Every WinSCP form and code-built dialog has a
linscp counterpart; the gaps are controls and actions inside them. Behaviour
without a caption or switch was not audited.

**Out of scope** (decided earlier, not gaps): WinSCP's tips, usage
statistics and donor automatic updates; the Windows installer's switches
(`/UninstallCleanup`, `/MaintenanceTask`, `/ImportSitesIfAny`) and COM
(`/comregistration`). Known and recorded with P3-27: Site Advanced's "Edit
in PuTTY" and the private key's Tools menu (G-0 below keeps them listed).

## Summary

| Area | Gaps | Weight |
|---|---|---|
| A. Site Manager | 17 | High: used at every login |
| B. Password prompt and banner | 3 | Medium |
| C. Progress and synchronization dialogs | 4 | High: transfers |
| D. Main window | 9 | Medium |
| E. Preferences | 10 | Low to medium: most settings are already stored |
| F. Command line | 3 | Medium |

Suggested order: C1 (transfer progress), A, then D1 and C3, E, F.

## Conventions for implementing them

- Upstream paths are under `3rd-party/winscp/source/` ("F/" = `forms/`,
  "W/" = `windows/`); linscp paths are under `port/qt/` unless stated.
- Follow upstream: its captions verbatim (WinSCP's translations then fill
  the 53 languages through `port/tools/winscp_ts_suggest.py`), its
  configuration keys (`port/qt/ShellConfiguration.cpp` for the GUI's,
  core's `TSessionData` and `TConfiguration` for the rest), its enabling
  logic. Core and Qt types never meet: core-side code goes in the bridge
  units (`CoreBridge.cpp`, `Sites.cpp`, `Session.cpp`), declared in
  `CoreBridge.h`.
- Each change: a ctest (`port/tests/test_qt_gui.cpp`, run through
  `run_qt_session.py`), `qt_gui_accessibility` passing (mnemonic clashes
  WinSCP itself has go in `WinScpClash`, `port/tests/AccessibilityAudit.cpp`),
  `update_translations` run, the new texts filled
  (`winscp_ts_suggest.py`, then `port/tools/ts_language.py export/import`
  for the rest), `translations_current` passing. Test on Linux, Wine and the
  VM where it touches Windows.
- Update [gaps_tracker.md](gaps_tracker.md) in the same commit, and
  [TRACKER.md](TRACKER.md) when a gap becomes a task.

## A. Site Manager (F/Login.dfm, F/Login.cpp; linscp SiteManager.cpp)

linscp's dialog has the tree (New Folder, Rename, Delete), the Advanced
split button (Advanced..., Edit Raw Settings...), Login, Save, Close and
Tools (Import Sites, Clean Up, Desktop Icon, Send To on Windows), and
workspaces in the tree. No Manage button, tree context menus, drag & drop or
key handlers.

- **A1. S3 "Credentials from AWS environment"** (`S3CredentialsEnvCheck3`,
  F/Login.dfm:174, handler F/Login.cpp:3365): takes the access key from the
  AWS environment/profile; the user and password fields are disabled. Add
  `TSite::S3CredentialsEnv` (core `TSessionData::S3CredentialsEnv`) through
  `Sites.cpp`'s `SiteOf`/`ApplyAdvanced`, and the checkbox to the S3 part
  of the form.
- **A2. FTP "Anonymous login"** (`AnonymousLoginCheck`, F/Login.dfm:308,
  F/Login.cpp:2615): sets user `anonymous` and an empty password, disabling
  both fields; on load, checked when the user is `anonymous`.
- **A3. Save drop-down** (F/Login.dfm:1224, F/Login.cpp:2625): Save, Save
  As... (for an existing site too: SaveSiteDialog with the site's name,
  F/Login.cpp:1047), Set Defaults (store the form as the default settings,
  `StoredSessions->DefaultSettings`, F/Login.cpp:1770, with its
  confirmation).
- **A4. Manage button and menu** (F/Login.dfm:494, menus 1241-1500): the
  site's, folder's, new site's and workspace's actions in one place: Edit,
  Clone to New Site (A5), Paste Session URL (A6), Transfer Settings Rule
  (A7), Reset (new site, F/Login.cpp:1653), Generate Session URL/Code
  (F/Login.cpp:3272; the main window's dialog exists), Rename, Delete, New
  Folder, Desktop Icon / Send To (move from Tools, keep them there too if
  wanted). The same menus as the tree's context menus (A11).
- **A5. Clone to New Site** (F/Login.cpp:3113): the selected site's settings
  in the New Site form, unsaved.
- **A6. Paste Session URL** (F/Login.cpp:3225): parse a URL from the
  clipboard into the form (core `ParseUrl` through the bridge, as
  `Session.cpp:326` does for the command line).
- **A7. Transfer Settings Rule...** (F/Login.cpp:3285, also in the Advanced
  drop-down, F/Login.dfm:720/1520): opens a preset's rule
  (`TCopyParamRuleData`) prefilled with the site's host, user and
  directory; linscp's preset rule editing is in `TransferSettingsDialog`.
- **A8. Tools menu additions** (F/Login.dfm:776-795): Run Pageant, Run
  PuTTYgen (F/Login.cpp:2637/2642: start them where present; on POSIX
  `pageant`/`puttygen` from PATH, disabled when absent); Import/Restore
  Configuration... and Export/Backup Configuration... (F/Login.cpp:2736/2727:
  core's `Configuration->Import`/`Export` of the INI); Preferences, Check
  for Updates, About (the main window's, reachable before logging in).
- **A9. Login drop-down** (F/Login.dfm:2020): Login, Open in PuTTY
  (F/Login.cpp:3149/3182; the main window's Open in PuTTY exists).
- **A10. Site search** (F/Login.cpp:2759-2859, 3336-3346, 3388; dfm
  514-559): type-to-search in the tree with three modes (Beginning of Site
  Name Only, Any Part of Site Name, All Major Site Fields), Find Site
  (Ctrl+F) opening the search box. The mode is a GUI setting
  (`SiteSearch`, W/WinConfiguration).
- **A11. Tree context menus** (F/Login.dfm:1241-1500): per item type, as A4.
- **A12. Drag & drop in the tree** (F/Login.cpp:2408-2609): move sites and
  folders into folders (a rename of the stored site's path).
- **A13. Rename in place** (F/Login.cpp:1932-1988, F2 at 879): edit the
  tree label instead of the Rename dialog; workspaces renamable too
  (linscp disables rename for them, SiteManager.cpp:872).
- **A14. Site color icons** (F/Login.cpp:375): each site's icon tinted with
  its color (`TSite::Color`, P3-27); `ColorMenu.h`'s `SessionColorIcon`.
- **A15. Note on the form** (F/Login.dfm:370-395, F/Login.cpp:550): the
  selected site's note shown read-only below the form (`TSite::Note`).
- **A16. Host name URL parsing** (F/Login.cpp:3187-3267, `HostNameEditExit`):
  a URL typed or pasted into the host name is split into protocol, host,
  port, user and directory on leaving the field.
- **A17. Advanced drop-down: Global Preferences > Logging...**
  (F/Login.dfm:1523-1530, F/Login.cpp:1627): Preferences opened on its
  Logging page (`PreferencesDialog::ShowPage`).

## B. Password prompt and banner (F/Authenticate.dfm; linscp PromptDialog.cpp, QtFrontend.cpp)

- **B1. "Change stored password to this one"** (`SavePasswordCheck`,
  F/Authenticate.dfm:155) and **"Remember password for this session"**
  (`SessionRememberPasswordCheck`, :216): core asks for them through the
  prompt's options (`pupSave`, `pupRemember` in core's `TPromptUserParams`
  / `PromptUser`'s `Results` objects). Carry them in `CoreBridge::TPrompt`
  (CoreBridge.h:88) and back, show the checkboxes in `PromptDialog`.
- **B2. Banner "Use Monospaced Font"** (F/Authenticate.dfm:299): a toggle;
  linscp's banner (QtFrontend.cpp:145-167) always uses a fixed font. The
  choice is a setting (upstream `WinConfiguration->BannerMonospacedFont`).
- **B3. Banner links** (F/Authenticate.dfm:309, 331-340): URLs in the banner
  clickable, with an "Open Link" context action (a QTextBrowser with
  `setOpenExternalLinks`, or linkify the text).

## C. Progress and synchronization dialogs

- **C1. Transfer progress** (F/Progress.dfm, F/Progress.cpp; linscp
  TransferDialog.cpp, which has File, File progress, Overall progress,
  Transferred, Speed, Cancel):
  - Time left, Time elapsed, Start time (Progress.dfm:111-178; core's
    `TFileOperationProgressType` has `TimeElapsed`, `TotalTimeLeft`,
    `StartTime`; extend the bridge's progress data).
  - Skip this file (dfm:240: `SkipToAll`/`Cancel = csCancelFile`).
  - Minimize (dfm:245), Proceed in Background (dfm:251: move the operation
    to the queue, `TFileOperationProgressType::Suspend`/WinSCP's
    `MoveToQueue`).
  - Once Finished drop-down (dfm:257-284: Stay Idle, Disconnect, Suspend,
    Shut Down; linscp has these for the queue: reuse
    `MainWindow`'s once-empty code).
  - Speed limit combo (dfm:290: `CPSLimit`).
- **C2. Synchronization progress** (F/SynchronizeProgress.dfm:26-151; linscp
  SynchronizeDialog.cpp:263-277 "Comparing" shows Local, Remote, Cancel):
  Time left, Start time, Time elapsed, Minimize.
- **C3. Synchronization checklist menu** (F/SynchronizeChecklist.dfm:
  2040-2220; linscp SynchronizeDialog.cpp:346-378 has buttons only):
  a context menu with Check, Uncheck (the selection), Check/Uncheck All
  Actions in This Directory (dfm:2189/2197), the Left Directory / Right
  Directory submenus (Explore; Copy Path to Clipboard, Ctrl+[ / Ctrl+],
  dfm:2115/2124, 2201-2220), and the existing tools (Reverse, Calculate,
  Move, Find Move Candidate, Custom Commands).
- **C4. Console** (F/Console.dfm; linscp ConsoleDialog.cpp): Adjust Window
  (dfm:1712, F/Console.cpp:265: fit the window to the output) and the
  output's context menu (Copy, Select All, dfm:1702/1707).

## D. Main window (F/NonVisual.dfm/.cpp, F/ScpCommander.dfm/.cpp)

- **D1. Column header menu** (NonVisual.dfm:2979-2993 local, 3050-3061
  remote; linscp FileList.cpp:423-429 shows only the column checkboxes):
  Sort Ascending / Sort Descending by the clicked column
  (`SortColumnAscendingAction` dfm:1121/1129), Hide Column (dfm:330,
  NonVisual.cpp:770), Sort by Extension, the file size format submenu
  (Bytes / Kilobytes / Short, `FormatSizeBytes*`, dfm:1531/1764/1771;
  linscp has it in Preferences only, PreferencesDialog.cpp:226),
  Calculate Directory Sizes, Columns.
- **D2. Customize Toolbar** (NonVisual.dfm:2316, NonVisual.cpp:712/1986
  `CreateToolbarButtonsList`): a submenu in the toolbar popups listing each
  button of the toolbar to show or hide, kept in the toolbar layout
  setting. linscp's popup (MainWindow.cpp:7199-7256) has the toolbars,
  Lock, Selective Text Labels and Icon Size.
- **D3. Back/Forward history drop-downs** (ScpCommander.dfm:1321-1328,
  ScpCommander.cpp:73 `HistoryMenu`): the buttons' drop-down lists of
  visited directories (the panels keep the history already).
- **D4. Panel navigation toolbars' Tree button and "Change directory view
  style" submenu** (ScpCommander.dfm:1355-1372 remote, 1741 local).
- **D5. Go to Tree** (NonVisual.dfm:213, NonVisual.cpp:693): Ctrl+Alt+T
  focuses the panel's tree in WinSCP; in linscp Ctrl+Alt+T toggles the tree
  (MainWindow.cpp:1728). Check WinSCP's keys (`check_menus.py`,
  WinSCP's shortcut tables) and follow them: focus, showing it if hidden.
- **D6. Go to Command Line** (NonVisual.dfm:271, NonVisual.cpp:692):
  Ctrl+Shift+M focuses the command line; linscp's toggles it
  (MainWindow.cpp:7078-7088), hiding it when shown.
- **D7. Go to Address** (NonVisual.dfm:2248, Ctrl+L) and **Incremental
  search start** (dfm:2489, Ctrl+F): focus the path/address box; start the
  file list's incremental search (FileList.cpp:760).
- **D8. Transfer Files in Clipboard** (NonVisual.dfm:1720,
  F/CustomScpExplorer.cpp:9338 `FileListFromClipboard`): the files named in
  the clipboard's text, one per line, transferred to the other panel.
- **D9. Editor Preferences' "Use default system editor"** (F/EditorPreferences
  .dfm:114, the `DefaultButton`): fills the external editor with the
  system's default (on Windows `DetectSystemExternalEditor`, W/Tools.cpp;
  on POSIX `xdg-mime query default text/plain`).

## E. Preferences (F/Preferences.dfm/.cpp; linscp PreferencesDialog.cpp)

Settings marked "stored" already have their keys in
`ShellConfiguration.cpp`; only the controls are missing.

- **E1. Commander: "Show directory tree left of file list"** (dfm:954,
  cpp:349; key `TreeOnLeft` under the Commander's panel settings, not
  stored yet): the tree beside the file list instead of above it
  (`RemotePanel::SetTreeBeside` exists for Explorer).
- **E2. Explorer page** (dfm:1041-1084): its own page under Environment >
  Interface, "Preferences on this page apply to Explorer interface only."
  and "Show full path on address bar" (stored:
  ShellConfiguration.cpp:352/937, `ExplorerShowFullAddress`).
- **E3. Storage: temporary directory options** (dfm:1865-1905, cpp:293-297;
  stored: ShellConfiguration.cpp:273-277/863-869): Cleanup obsolete
  temporary directories on startup, Ask before cleanup, Append session name,
  Append remote path, Keep temporary copies in deterministic paths. Check
  that linscp's editing of remote files honours them.
- **E4. Window: "Show Login dialog on startup and when the last session is
  closed"** (dfm:2643, cpp:519/927, `ShowLoginWhenNoSession`; not stored):
  linscp's start-up and last-tab behaviour should follow it.
- **E5. Window: "Truncate tab titles when they do not fit to window"**
  (dfm:2655, cpp:522) and **"Do not popup error message when idle session
  is disconnected"** (dfm:2665, cpp:521): not stored.
- **E6. Window: Workspaces group** (dfm:2695-2725, cpp:496-504): Default
  workspace name, Automatically save workspace on exit, Save passwords (not
  recommended) (stored: ShellConfiguration.cpp:267-269/857-859), Connect
  all sessions of opened workspace or site folder (not stored).
- **E7. Internal editor: "Disable smooth scrolling"** (dfm:3468, cpp:401):
  upstream's workaround for the Windows rich edit control; offer only if
  Qt's editor has an equivalent, else record as not applicable.
- **E8. Languages: "Get more..."** (dfm:3314, cpp:2650): opens WinSCP's
  translations page; linscp's equivalent is its own translations doc or
  repository page.
- **E9. Drag & Drop: "Allow dropping files to these network drives:"**
  (dfm:1484/1522, cpp:320, enabled with the fake-file drop, cpp:1416):
  Windows only, the drive letters for DragExt's drop.
- **E10. Search box above the page tree** (dfm:3608/3709, cpp:1182,
  3539-3779): filters the pages and settings by text, with a results page.

## F. Command line (W/WinMain.cpp, W/ConsoleRunner.cpp; linscp port/qt/main.cpp, CoreBridge.cpp, port/frontend/CliMain.cpp)

Session URLs and their switches (`/privatekey`, `/hostkey`, `/passive`,
`/timeout`, `/rawsettings`...), `/ini`, `/rawconfig`, `/unsafe`,
`/newinstance`, `/upload`, `/uploadifany`, `/defaults`, `/keepuptodate` and
the URL-handler registrations work.

- **F1. GUI logging switches**: `/log`, `/xmllog` (WinMain.cpp:1272/1276),
  `/loglevel`, `/logsize` (WinMain.cpp:859/885). linscp-cli has them
  (CliMain.cpp:116-164); the GUI does not.
- **F2. GUI operation switches**: `/synchronize` (full synchronization,
  WinMain.cpp:1139), `/edit` (1147), `/refresh` (1152), `/explore` and
  `/browse` (1292), `/workspace` (18: linscp writes it, CoreBridge.cpp:1424,
  but Session.cpp:284 ignores it), global `/rawtransfersettings` (910;
  linscp applies it only with `/keepuptodate`, Session.cpp:1995),
  `/privatekey` as the global default key (840), `/help` (917),
  `/Console=`/`/DotNet=` wrapper versions (847). Today `/synchronize`,
  `/edit` and `/refresh` only stop the forwarding to a running instance
  (CoreBridge.cpp:1001).
- **F3. linscp-cli switches**: `/info`, `/batchsettings` (ConsoleRunner.cpp
  :2468), `/copyid` with `/identity` (2974), `/fingerprintscan`,
  `/dumpcallstack` (2772), `/h`.

## G. Known before this audit

- **G1. Site Advanced: "Edit in PuTTY"** (PuTTY's terminal settings,
  F/SiteAdvanced.cpp:1842) and **the private key's Tools menu** (Generate
  New Key Pair with PuTTYgen, Install Public Key into Server,
  F/SiteAdvanced.cpp:1654-1686).
