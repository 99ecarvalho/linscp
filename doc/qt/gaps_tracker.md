# Gaps tracker

Status of the gaps in [gaps.md](gaps.md) (each ID's section there says what
to do). Status values as [TRACKER.md](TRACKER.md)'s: `todo`, `wip`,
`blocked`, `done`, `dropped`. Update this file in the commit that changes a
gap's status; note the commit or ctest in Notes.

Last updated: **2026-10-17**

## Progress

| Area | Done | Total |
|---|---|---|
| A. Site Manager | 15 | 17 |
| B. Password prompt and banner | 3 | 3 |
| C. Progress and synchronization dialogs | 4 | 4 |
| D. Main window | 9 | 9 |
| E. Preferences | 9 | 10 |
| F. Command line | 2 | 3 |
| G. Known before | 0 | 1 |
| **Total** | **42** | **47** |

## Items

| ID | Gap | Priority | Status | Notes |
|---|---|---|---|---|
| A1 | S3 "Credentials from AWS environment" | Medium | done | 2026-10-17. With the profile combo, as TLoginDialog's S3CredentialsEnvCheck; ctest `qt_gui_sites` |
| A2 | FTP "Anonymous login" | Medium | done | 2026-10-17. As TLoginDialog's AnonymousLoginCheck; ctest `qt_gui_sites` |
| A3 | Save drop-down: Save As, Set Defaults | High | done | 2026-10-17. `SiteManager::SaveAsDialog`, `SetDefaults` (`CoreBridge::SaveDefaultSite`); ctest `qt_gui_sites` |
| A4 | Manage button and menu | High | done | 2026-10-17. `SiteManager::FillManageMenu`, also the tree's (A11); Generate Session URL/Code and Open in PuTTY for an unconnected site (`CoreBridge::GenerateSiteCode`, `OpenSiteInPutty`); Paste Session URL comes with A6, Transfer Settings Rule with A7, Search with A10; ctest `qt_gui_sites` |
| A5 | Clone to New Site | Medium | done | 2026-10-17. The name made unique for Save (`CoreBridge::UniqueSiteName`); ctest `qt_gui_sites` |
| A6 | Paste Session URL | Medium | done | 2026-10-17. In the new site's Manage menu (Ctrl+V), `CoreBridge::ParseSiteUrl`; ctest `qt_gui_sites` |
| A7 | Transfer Settings Rule... | Low | todo | |
| A8 | Tools: Run Pageant/PuTTYgen, Import/Export Configuration, Preferences, Check for Updates, About | Medium | done | 2026-10-17. `SiteManager::FindTool`, `RunTool`, `ExportConfiguration`, `ImportConfiguration` (restarts with the same /ini); the main window's dialogs through `OnPreferences`, `OnCheckForUpdates`, `OnAbout`; ctest `qt_gui_sites` |
| A9 | Login drop-down: Open in PuTTY | Low | todo | Open in PuTTY is in Manage's menu (A4); the Login button's drop-down is left |
| A10 | Site search (three modes, Find Site) | High | done | 2026-10-17. Typed in the tree, Tab/Shift+Tab, Escape, Backspace; Search in the Manage menus; `Interface\LoginDialog\SiteSearch`; the tree's Delete and F2 keys, and the actions' keys (Ctrl+F, Ctrl+P, Ctrl+V, Alt+A); ctest `qt_gui_sites` |
| A11 | Tree context menus | High | done | 2026-10-17. Manage's menu (A4); ctest `qt_gui_sites` |
| A12 | Drag & drop in the tree | Medium | done | 2026-10-17. Sites only, as upstream; `SiteManager::DropFolder`, `MoveSite`; ctest `qt_gui_sites` |
| A13 | Rename in place (F2), workspaces renamable | Medium | done | 2026-10-17. `SiteManager::RenameInPlace`; a workspace's sessions moved (`CoreBridge::RenameWorkspace`); ctest `qt_gui_sites`, `qt_gui_tabs` |
| A14 | Site color icons in the tree | Low | done | 2026-10-17. A swatch over the site icon's corner; ctest `qt_gui_sites` |
| A15 | Note shown on the form | Low | done | 2026-10-17. Read-only, shown when the site has one; ctest `qt_gui_sites` |
| A16 | Host name URL parsing | Medium | done | 2026-10-17. On leaving the field and before Save and Login, `CoreBridge::HostNameIsUrl`; ctest `qt_gui_sites` |
| A17 | Advanced drop-down: Global Preferences > Logging | Low | done | 2026-10-17. Through `OnPreferences`; ctest `qt_gui_sites` |
| B1 | Prompt: change stored password, remember for this session | High | done | 2026-10-17. Offered as TAuthenticateForm::PromptUser offers them; ctest `qt_gui_sites` (a refused stored password replaced; remember offered with the global option off) |
| B2 | Banner: Use Monospaced Font | Low | done | 2026-10-17. In the banner's menu (Copy, Select All, Use Monospaced Font), kept per banner by core (bpMonospacedFont); the window's font otherwise, as upstream's default; ctest `qt_dialogs` |
| B3 | Banner: clickable links, Open Link | Low | done | 2026-10-17. Upstream's are the prompt's labels (ExternalLabel, LabelPopupMenu): a link opened by a click, Copy and Open Link; ctest `qt_dialogs` |
| C1 | Transfer progress: times, Skip, Background, Once Finished, Minimize, speed limit | High | done | 2026-10-17. Proceed in Background for the panels' uploads and downloads (`TMoveToQueue`, Session.cpp), as upstream for its ExecuteFileOperation; ctest `qt_gui_transfer` (times, speed limit, skip, background, once finished), `qt_gui_progress` (the queue resumes and completes the part, OpenSSH) |
| C2 | Synchronization progress: times, Minimize | Medium | done | 2026-10-17. With the progress bar and the percentage; the time left from upstream's directory statistics cache (`SyncCollect`'s `OnProgress`), kept only with a real storage; ctest `qt_gui_sync` |
| C3 | Synchronization checklist context menu | Medium | done | 2026-10-17. `SynchronizeDialog::PopupMenu`; the remote directory's Explore waits for `/explore` (F2); ctest `qt_gui_sync` |
| C4 | Console: Adjust Window, Copy/Select All menu | Low | done | 2026-10-17. `ConsoleDialog::OutputMenu`, `AdjustWindow` (to the longest line and the lines, within the screen); ctest `qt_gui_properties` |
| D1 | Column header menu: sort asc/desc, hide, size format, ... | High | done | 2026-10-17. `FileView::HeaderMenu`; ctest `qt_gui_panel` |
| D2 | Customize Toolbar | Medium | done | 2026-10-17. In the toolbars' context menus (the main ones' and the panels' bands', as CommanderBarPopup, LocalPanelPopup, RemotePanelPopup); WinSCP's item names, kept in `ToolbarsButtons` (Commander and Explorer); a hidden button leaves the band (its action stays in the menus), back in place with its drop-down; ctest `qt_gui_keys`, `qt_gui_explorer` |
| D3 | Back/Forward history drop-downs | Medium | done | 2026-10-17. Both panels: `HistoryGo`, `FillHistoryMenu` (ten at most, `CoreBridge::MinimizeName`, the full path as tip); ctest `qt_gui_panel`, `qt_gui_local` |
| D4 | Panel navigation toolbars: Tree button, view style submenu | Low | done | 2026-10-17. Each navigation band's Tree (in step with the menus' toggle) and Change directory view style (Details, Thumbnails), named as ScpCommander.dfm's for Customize Toolbar; ctest `qt_gui_trees` |
| D5 | Go to Tree (Ctrl+Alt+T focuses) | Low | done | 2026-10-17. WinSCP's action list runs GoToTreeAction before LocalTreeAction/RemoteTreeAction (NonVisual.dfm:213/222/231; its main window's toolbars do not process shortcuts), so the key shows and focuses; the toggles only show it. `MainWindow::GoToTree`; ctest `qt_gui_trees` |
| D6 | Go to Command Line (Ctrl+Shift+M focuses) | Low | done | 2026-10-17. The same order (dfm:271/451); `MainWindow::GoToCommandLine`; ctest `qt_gui_keys` |
| D7 | Go to Address (Ctrl+L), incremental search (Ctrl+F) | Medium | done | 2026-10-17. Ctrl+L and Alt+D: Open Directory in the Commander (as TScpCommanderForm::GoToAddress), the address bar in the Explorer; Ctrl+F: `FileView::StartSearch`, "(start typing)"; ctest `qt_gui_keys` |
| D8 | Transfer Files in Clipboard | Low | dropped | 2026-10-17. Not in WinSCP: its FileListFromClipboardAction is in no menu or toolbar, and FileListFromClipboard is an empty stub ("// TBD", CustomScpExplorer.cpp:9338) |
| D9 | Editor Preferences: Use default system editor | Low | done | 2026-10-17. `EditorPreferencesDialog::SystemExternalEditor`: FindExecutable of a .txt file on Windows (not "Open with"), else xdg-mime's text/plain desktop file's Exec; ctest `qt_gui_logswitches` (a fake xdg-mime) |
| E1 | Commander: tree left of file list | Medium | done | 2026-10-17. `ScpCommander.TreeOnLeft`; each panel's tree width kept apart (`DriveViewWidth`) from its height; ctest `qt_gui_trees` |
| E2 | Explorer page: Show full path on address bar | Low | done | 2026-10-17. Applied to the address bar at once; ctest `qt_gui_preferences` |
| E3 | Storage: temporary directory options (5) | Medium | done | 2026-10-17. The editing already followed them; the start-up cleanup added (`MainWindow::TemporaryDirectoryCleanup`, as Setup.cpp's, not for a second instance), and core's recursive delete fixed for POSIX (`core-recursive-delete-delimiter.patch`); ctest `qt_gui_preferences` |
| E4 | Window: Show Login dialog on startup / last session closed | Medium | done | 2026-10-17. `MainWindow::NeedSession` (as TCustomScpExplorerForm's) at start-up and once the last tab closes; the Site Manager's own "Show Login dialog..." check when shown by itself, unchecked after asking; ctest `qt_gui_preferences` |
| E5 | Window: truncate tab titles; silent idle disconnect | Low | done | 2026-10-17. Titles elided (`QTabBar::setElideMode`); an idle session's lost connection quiet in the status bar, else its message with "Never popup disconnect messages" (`MainWindow::IdleError`, `TCoreError::Fatal`); ctest `qt_gui_tabs`, `qt_gui_sessioncommands` |
| E6 | Window: Workspaces group (4) | Medium | done | 2026-10-17. With WinSCP's default, a workspace's other sessions connect as their tabs are selected (`TTab::Pending`, `ConnectPendingTab`); Connect all connects them as it opens; ctest `qt_gui_preferences`, `qt_gui_tabs` |
| E7 | Internal editor: Disable smooth scrolling | Low | dropped | 2026-10-17. Not applicable: upstream's works around the Windows rich edit control's smooth scrolling; Qt's editor scrolls without animation |
| E8 | Languages: Get more... | Low | done | 2026-10-17. linscp's translations page (doc/qt/translations.md) |
| E9 | Drag & Drop: network drives for the fake-file drop | Low | done | 2026-10-17. With upstream's fallback it serves: without the extension running (or broken), each drive watched for the fake directory's copy (`DragExt::TCreationMonitor`, `MonitorRoots`: fixed drives, DDDrives' network ones, less "-X"); ctest `qt_gui_dragmonitor` (Wine; the VM was not reachable) |
| E10 | Preferences search box | Low | todo | |
| F1 | GUI: /log, /xmllog, /loglevel, /logsize | Medium | done | 2026-10-17. linscp-cli's `ApplyLogSwitches`, moved to FrontendCommon, at the GUI's initialization; ctest `qt_gui_logswitches` |
| F2 | GUI: /synchronize, /edit, /refresh, /explore, /workspace, /rawtransfersettings, /privatekey, /help | Medium | done | 2026-10-17. /synchronize, /edit (the window hidden), /refresh (to the running instance; a path as core reads it, "/dir/"), download URLs (downloaded, or focused with /explore or /browse), /workspace (before a site of the same name), /rawtransfersettings and /privatekey for the run, /help (linscp-cli's usage). /Console= and /DotNet= not applicable: they record the wrappers' versions for WinSCP's update service; ctest `qt_gui_uploadswitch`, `qt_gui_sessioncommands`, `qt_gui_tabs`, `qt_gui_logswitches` |
| F3 | CLI: /info, /batchsettings, /copyid, /fingerprintscan, /dumpcallstack, /h | Low | todo | |
| G1 | Site Advanced: Edit in PuTTY; private key Tools menu | Low | todo | From P3-27 |
