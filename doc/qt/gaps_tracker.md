# Gaps tracker

Status of the gaps in [gaps.md](gaps.md) (each ID's section there says what
to do). Status values as [TRACKER.md](TRACKER.md)'s: `todo`, `wip`,
`blocked`, `done`, `dropped`. Update this file in the commit that changes a
gap's status; note the commit or ctest in Notes.

Last updated: **2026-10-17**

## Progress

| Area | Done | Total |
|---|---|---|
| A. Site Manager | 9 | 17 |
| B. Password prompt and banner | 1 | 3 |
| C. Progress and synchronization dialogs | 2 | 4 |
| D. Main window | 1 | 9 |
| E. Preferences | 0 | 10 |
| F. Command line | 0 | 3 |
| G. Known before | 0 | 1 |
| **Total** | **13** | **47** |

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
| A8 | Tools: Run Pageant/PuTTYgen, Import/Export Configuration, Preferences, Check for Updates, About | Medium | todo | |
| A9 | Login drop-down: Open in PuTTY | Low | todo | Open in PuTTY is in Manage's menu (A4); the Login button's drop-down is left |
| A10 | Site search (three modes, Find Site) | High | done | 2026-10-17. Typed in the tree, Tab/Shift+Tab, Escape, Backspace; Search in the Manage menus; `Interface\\LoginDialog\\SiteSearch`; the tree's Delete and F2 keys, and the actions' keys (Ctrl+F, Ctrl+P, Ctrl+V, Alt+A); ctest `qt_gui_sites` |
| A11 | Tree context menus | High | done | 2026-10-17. Manage's menu (A4); ctest `qt_gui_sites` |
| A12 | Drag & drop in the tree | Medium | todo | |
| A13 | Rename in place (F2), workspaces renamable | Medium | todo | |
| A14 | Site color icons in the tree | Low | todo | |
| A15 | Note shown on the form | Low | todo | |
| A16 | Host name URL parsing | Medium | done | 2026-10-17. On leaving the field and before Save and Login, `CoreBridge::HostNameIsUrl`; ctest `qt_gui_sites` |
| A17 | Advanced drop-down: Global Preferences > Logging | Low | todo | |
| B1 | Prompt: change stored password, remember for this session | High | done | 2026-10-17. Offered as TAuthenticateForm::PromptUser offers them; ctest `qt_gui_sites` (a refused stored password replaced; remember offered with the global option off) |
| B2 | Banner: Use Monospaced Font | Low | todo | |
| B3 | Banner: clickable links, Open Link | Low | todo | |
| C1 | Transfer progress: times, Skip, Background, Once Finished, Minimize, speed limit | High | done | 2026-10-17. Proceed in Background for the panels' uploads and downloads (`TMoveToQueue`, Session.cpp), as upstream for its ExecuteFileOperation; ctest `qt_gui_transfer` (times, speed limit, skip, background, once finished), `qt_gui_progress` (the queue resumes and completes the part, OpenSSH) |
| C2 | Synchronization progress: times, Minimize | Medium | todo | |
| C3 | Synchronization checklist context menu | Medium | done | 2026-10-17. `SynchronizeDialog::PopupMenu`; the remote directory's Explore waits for `/explore` (F2); ctest `qt_gui_sync` |
| C4 | Console: Adjust Window, Copy/Select All menu | Low | todo | |
| D1 | Column header menu: sort asc/desc, hide, size format, ... | High | done | 2026-10-17. `FileView::HeaderMenu`; ctest `qt_gui_panel` |
| D2 | Customize Toolbar | Medium | todo | |
| D3 | Back/Forward history drop-downs | Medium | todo | |
| D4 | Panel navigation toolbars: Tree button, view style submenu | Low | todo | |
| D5 | Go to Tree (Ctrl+Alt+T focuses) | Low | todo | Check against WinSCP's keys |
| D6 | Go to Command Line (Ctrl+Shift+M focuses) | Low | todo | |
| D7 | Go to Address (Ctrl+L), incremental search (Ctrl+F) | Medium | todo | |
| D8 | Transfer Files in Clipboard | Low | todo | |
| D9 | Editor Preferences: Use default system editor | Low | todo | |
| E1 | Commander: tree left of file list | Medium | todo | Not stored yet |
| E2 | Explorer page: Show full path on address bar | Low | todo | Stored |
| E3 | Storage: temporary directory options (5) | Medium | todo | Stored |
| E4 | Window: Show Login dialog on startup / last session closed | Medium | todo | Not stored |
| E5 | Window: truncate tab titles; silent idle disconnect | Low | todo | Not stored |
| E6 | Window: Workspaces group (4) | Medium | todo | 3 of 4 stored |
| E7 | Internal editor: Disable smooth scrolling | Low | todo | May be not applicable |
| E8 | Languages: Get more... | Low | todo | |
| E9 | Drag & Drop: network drives for the fake-file drop | Low | todo | Windows only |
| E10 | Preferences search box | Low | todo | |
| F1 | GUI: /log, /xmllog, /loglevel, /logsize | Medium | todo | |
| F2 | GUI: /synchronize, /edit, /refresh, /explore, /workspace, /rawtransfersettings, /privatekey, /help | Medium | todo | |
| F3 | CLI: /info, /batchsettings, /copyid, /fingerprintscan, /dumpcallstack, /h | Low | todo | |
| G1 | Site Advanced: Edit in PuTTY; private key Tools menu | Low | todo | From P3-27 |
