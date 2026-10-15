# Tracker

Last updated: **2026-10-15**

Legend: `todo` · `wip` · `blocked` · `done` · `dropped`

## Progress

| Phase | Done | Total | Gate |
|---|---|---|---|
| P0 Foundation | 32 | 35 | `core/Common.cpp` compiles on Linux clang; VCL build green (R1) |
| P1 Headless core | 37 | 38 | Golden suite green, six protocols, both platforms |
| P2 Qt shell | 11 | 11 | A user can do a day's work in it |
| P3 Qt parity | 33 | 35 | Parity on features users depend on |
| P4 Cleanup | 0 | 4 | Dropped for now (2026-10-15); may be reopened |
| **Total** | **110** | **122** | |

## Next three actions

Driven by `compile_census.py` rather than by grep counts — the compiler names
what is actually blocking each unit, in dependency order.

**The whole core library compiles for Windows** -- all 33 units in upstream's
`ScpCore.cbproj`, 0 errors -- and now not only in the syntax-only census but
as a real library: `linscp-core`, built by CMake with the cross toolchain.
Linux: **558 errors, 14/33 clean, 4 aborting.**

**Core compiles on Linux.** All 33 units of `ScpCore.cbproj` build with
clang into `liblinscp-core.a` (ctest `core_build`), from WinSCP's `master`
(6.6.4-RC + 3) with no core changes beyond four small patches. Linux had 588
census errors at the pin move. The Win32 functions core calls are now
provided by the POSIX layer, tested against the real API under Wine wherever
the behaviour is observable there.

On Windows, core links and runs real SFTP sessions under Wine, as negotiated
and pinned to AES-CTR, AES-GCM and ChaCha20-Poly1305.

**WinSCP's PuTTY builds on Linux** too: all 176 units of `Putty.cbproj`,
its Windows platform layer included, over the POSIX layer, with
`port/putty/linux/units.txt` replacing the agent (`SSH_AUTH_SOCK`) and
GSSAPI (the system's Kerberos library) and dropping the four units only
Pageant's named pipe used. The crypto known-answer tests pass on Linux.

**Winsock runs on Linux**: a `ws2_32` module whose `GetProcAddress` gives
`network.c` Winsock-signature functions, and `WSAEventSelect` /
`WSAEnumNetworkEvents` with Winsock's re-enabling rules, verified by one
test against real Winsock under Wine and against the emulation (ctest
`winsock`).

**neon and libs3 build on Linux** (P1-13 done). neon takes a POSIX
configuration from its own `configure` (`port/neon/config-posix.h`); WinSCP's
Windows-only additions to it are guarded by one patch, and libs3's use the
POSIX layer.

**Core links on Linux** -- every core object, PuTTY, neon, libs3, FileZilla
and WinSCP's MFC subset, nothing undefined (ctest `core_link_build`) -- and
initialises and finalises there (`core_init`). FileZilla is compiled as is
over an emulation of the user32 it uses: windows as message targets, thread
message queues, timers, `WSAAsyncSelect` and `WSAAsyncGetHostByName` (P1-11,
P1-12 done). ctest `messages` checks it against user32 and ws2_32 under Wine
and against the emulation, with the same assertions.

**SFTP sessions run on Linux** (P1-17 done): the same four loopback-`sshd`
tests as on Windows -- connect, list, download -- as negotiated and pinned to
AES-CTR, AES-GCM and ChaCha20-Poly1305. Getting there took two fixes of
general reach: core's `long` is respelled `LONG`/`ULONG` by the transform, so
it keeps the 32 bits upstream relies on (TSFTPPacket read every uint32 as 8
bytes), and nothing linked may call the C library's 4-byte wide functions,
which ctest `wide_imports` now checks at link level.

**The scripting client runs on both targets** (P1-15, P1-18 done):
`linscp-cli`, winscp.com's counterpart, runs core's script on standard input
and output -- `/command`, `/script`, `/parameter`, piped commands, a terminal
prompt, `/stdout`, `/keygen`, exit codes -- tested against the loopback `sshd`
on Linux and under Wine (ctest `cli_session`). On the way: Delphi's
zero-initialised instances, which upstream relies on, are now given by the
shim (every allocation and every thrown object starts zeroed).

**Configuration persists on Linux** (P1-06d done): WinSCP's INI file at
`$XDG_CONFIG_HOME/linscp/WinSCP.ini` (or a `WinSCP.ini` beside the
executable), stored sites included (ctest `ini_sites`, `cli_session`). Two
fixes came with it: storage keys are joined with backslashes on every platform
(`core-storage-key-backslashes.patch`), and the headless configuration now
starts from upstream's defaults -- it had never called `Default()`, so every
setting had been zero.

**Every protocol passes the same everyday script on both targets** -- SFTP,
SCP, WebDAV, WebDAVS, FTP, FTPES, FTPS and S3 -- list, download, upload, mkdir, move, delete, rmdir, each checked
on the server's side (ctest `protocols`). OpenSSH serves SSH and rclone
WebDAV; this machine has no FTP or S3 server, so the suite has small ones of
its own (`ftp_server.py`, with explicit and implicit TLS; `s3_server.py`). It
found four bugs: class-typed properties read from a field were copies, so
`Rights.Number = ...` was lost and SCP uploaded files with mode 000; neon's
TLS-init closure travelled as two raw pointers
(`core-neon-tls-init-closure.patch`); and a script `rmdir` over WebDAV
dereferenced a null directory list, upstream too
(`core-script-rmdir-full-path.patch`); and `SubString(0, n)` returned n-1
characters, which cut S3 bucket names short.

**The golden-output harness runs** (P0-13 done): `port/tests/golden`, five
cases over seven protocols, servers and data chosen to be identical on Linux
and Windows. The two linscp targets already agree on all 35 logs -- getting
there found two bugs: on Windows mingw's `towupper` returns `wint_t`, so
file types were logged as `68`/`45`; and on POSIX `FindFirst` read `*.*`
literally, so synchronization did not see local directories without a dot
in their name and `synchronize remote -delete` deleted them on the server.

**linscp matches real WinSCP** (P0-15, P1-19 done): the golden corpus was
captured on a native Windows 11 VM with WinSCP 6.5.7 against rclone v1.75.2
and the suite's servers (`golden/expected`), and ctest `golden` passes on
Linux and on the Windows build: all 35 logs identical but the two WebDAV
`rmdir` cases, where WinSCP 6.5.7 fails with "Invalid access to memory" and
linscp's patch removes the directory (`known-differences.txt`).

Script `rmdir` over WebDAV is reported upstream with its fix:
winscp/winscp#69 (`core-script-rmdir-full-path.patch` until it lands).

**linscp-cli speaks winscp.com's protocol** (P1-20 done, on Windows):
started with `/consoleinstance`, it talks to its caller through the shared
`TConsoleRunner` structure and events, as WinSCP.exe does. On the Windows
VM, real winscp.com drives it (copied beside it as WinSCP.exe) and the
golden corpus matches through that path too, and WinSCP's .NET assembly,
built from the same source, runs full sessions through it over SFTP, FTP,
FTPES, WebDAV and S3 (`port/tests/dotnet/run_dotnet.py`). Natively on
Windows linscp matches the corpus as under Wine.

**keepuptodate works** (P1-22 done): `TDirectoryWatcher` (inotify on Linux,
`ReadDirectoryChangesW` on Windows) in place of upstream's `TDiscMonitor`,
driven as `TSynchronizeController` drives it -- per changed directory, after
the 500 ms change delay, excluded directories not watched, upstream's
messages. `run_keepuptodate.py` passes on Linux (ctest `keepuptodate`) and
natively on the Windows VM.

**CI runs on pull requests** (P0-11, P0-12 done): GitHub Actions builds
Linux and the Windows cross-build in Debian trixie and runs the whole ctest
suite, natively and under Wine (`.github/workflows/ci.yml`). The smaller
upstream bugs are reported: winscp/winscp#70 (FileGetSymLinkTargetSafe's
handle leak), #71 (GetParentProcessId's memset), #72 (aes-ni.c's pragma pop).

**The .NET assembly runs on Linux** (P1-23 done, ADR 0010): off Windows
the assembly hands winscp.com's `TConsoleCommStruct` to the executable over
two inherited pipes (`/consolepipes`) in place of named events and a file
mapping. `linscp-cli` speaks both transports. linscp carries the assembly
change (`port/patches/dotnet/`) and builds the assembly from the pinned
source with it; it drives `linscp-cli` on Linux over SFTP, FTP, FTPES,
WebDAV and S3 (ctest `dotnet`, in CI), and natively on Windows through
`/consoleinstance`. Offered upstream as winscp/winscp#73; work does not wait
on the answer.

**P1 is closed** (P1-10 done, P1-21 dropped). WinSCP's PuTTY is reconciled
against stock 0.85 (`doc/qt/putty-deltas.md`): 65 files modified, 7 added,
each delta classified, held in place by ctest `putty_deltas`. Its findings:
four upstream bugs worth reporting (`random_add_noise` leaves the PuTTY
lock held; `retrieve_host_key` compares against its output buffer; the
connect timeout bounds no `connect()`; `win_misc_cleanup` leaves a dangling
pointer), and nothing the Linux build gets wrong. P1-21's subsystems are
not compiled at all.

**Qt for Windows is settled** (P2-01 done, ADR 0011): the Windows build is
llvm-mingw (clang, lld, libc++, UCRT; no GCC) against MSYS2's prebuilt Qt
6.8.2 -- Debian's version, so Debian's moc/uic/rcc are the host tools --
with its C++ runtime, all pinned (`port/qt/msys2-qt.lock`, llvm-mingw
20250114 = LLVM 19.1.7, the runtime's). All 56 ctests pass on it under Wine;
`linscp-cli` and a Qt Widgets program run natively on Windows 11. The PuTTY
delta bugs are reported (winscp/winscp#74-#77).

**The Qt shell exists** (P2-02 done, ADR 0012): `linscp` (`port/qt`) starts
core through `CoreBridge` -- the only file where the two meet -- and shows
its main window, on Linux and, cross-built, on Windows. ctest `gui_smoke`
runs it on both (offscreen; under Wine), `windows_imports` checks what Wine
cannot: every import from a shipped DLL resolves. CI builds it on both jobs.

**Core's questions reach Qt** (P2-03 done, ADR 0012 amended): host keys,
passwords, banners, errors and connection progress arrive as Qt dialogs
built the way upstream's are -- answers, aliases, OnSubmit buttons,
never-ask-again, timeouts -- with core on the GUI thread as in WinSCP.
File > Open URL connects and lists the remote directory. Tested on Linux,
under Wine and natively on Windows 11, over SFTP and FTP.

**linscp speaks WinSCP's 55 languages** (P2-09 done, ADR 0013): core's
texts come from WinSCP's own translations, loaded where upstream loads its
translation DLL (the main module's `ResInstance`), so core is unchanged;
the shell's texts are Qt Linguist files (Brazilian Portuguese first), Qt's
its own; one locale for all three (`--lang`, else the system's).

**The site manager works** (P2-04 done, ADR 0012 amended): the shell starts
with it, as WinSCP starts with its Login dialog -- core's stored sites in a
tree with their folders, a site's protocol, encryption, server, credentials
and remote directory in a form, new / save / save as / delete / login, the
port following the protocol. Sites are core's, in core's storage (the INI
file on Linux; WinSCP's registry key on Windows, so WinSCP's own sites
appear).

**Sites have their advanced settings** (P2-05 done, ADR 0012 amended):
directories, timeout, FTP passive mode, proxies, private keys with
upstream's OpenSSH-key conversion, agent options; folders made, renamed and
deleted; sites renamed. A site through an authenticating SOCKS5 proxy and
an OpenSSH key converted and logging in are tested.

**The remote panel works** (P2-06 done): sortable columns with directories
first, the path box, back/forward/parent/root/home/refresh, new folder,
rename and delete with core's confirmations, WinSCP's keys.

**Both panels are there** (P2-08 done): WinSCP's Commander layout, the
local panel beside the remote one, sharing their model (`FileList`): the
same columns and order, hidden files off by default (Ctrl+Alt+H, WinSCP's
default), kept up to date as the local directory changes, deleting to the
trash as WinSCP's default, files opened with their applications, core's
texts for confirmations and errors.

**Files transfer, in the foreground and through the queue** (P2-07 done,
ADR 0012 amended): F5 copies and F6 moves from the active panel, after
WinSCP's Copy confirmation with its "transfer in background" choice; a
progress window with cancel for foreground transfers; the queue panel below
the panels for background ones (core's `TTerminalQueue`, polled on the GUI
thread, its questions popped up as WinSCP does), with cancel, suspend and
resume; the panels catch up when transfers finish.

Drag and drop (P3-07, done): between the panels, a transfer after the Copy dialog
(Shift: move); within a panel, onto a directory, a move into it (remotely
through core's `MoveFiles`, as WinSCP); files from other programs, onto the
remote panel, an upload; remote files dragged out to other programs,
downloaded at the drop to a temporary directory, as WinSCP's drag without its
shell extension. Preferences (P3-04a…d, done): upstream's GUI configuration compiled as
it is, the language chosen as upstream does, Options > Preferences with
Window, Commander, Languages, Panels, File colors, Transfer, Background,
Endurance, Network, Security, Logging and Storage; transfers use the GUI's
transfer settings and presets, and the layout is kept between runs, as
WinSCP's. Last used directories (P3-11, done): kept in the site when a session ends,
as WinSCP. The Commander (P3-02, done): WinSCP's keys, rename in place, status lines,
columns. Synchronization (P3-03, done): Commands > Synchronize, WinSCP's
Synchronize dialog and checklist over core's own synchronization, applied
here or in the queue. Editing (P3-05, done): Files > Edit (F4) in WinSCP's
internal editor, remote files downloaded to a temporary directory and
uploaded back on save, as WinSCP (ADR 0015). Keeping the remote directory
up to date (P3-06e, done): Commands > Keep Remote Directory up to Date, over
upstream's own controller. The rest of the dialogs (P3-06f, done): License and
third-party credits, Clean Up, S3 tags, bookmark shortcuts. Shell integration
(P3-08, done): Open in PuTTY (or a terminal), a session named on the command
line, Explore Directory. The rest of Preferences (P3-04e, done): Editors with
external editors and the associated application, Internal editor,
Integration, Applications, Commands. Session tabs and workspaces (P3-13,
done): several sessions at once, saved together and opened again. The
local browser (P3-12, done): local tabs, copying and moving between two
local panels. Accessibility (P3-09, done): every window the GUI tests show
is audited for screen-reader names, label mnemonics and clashes. The
Commander commands users reach daily (P3-14, done): session commands,
Compare Directories, Synchronize Browsing, New File, the clipboard,
directory sizes, the status bar and queue. **The P3 exit gate is passed**
(P3-10, 2026-10-14); the rest of WinSCP's menus are backlog (P3-15). The
UI gaps raised before packaging (P3-16, done 2026-10-15) are closed: the
toolbars and icon sets, the directory trees, the tray and single instance,
the master password, host CAs, local custom commands, `/upload` with Send
To and shortcuts, Keep Up To Date in a new window, S3 ACLs and more (the
audit below); only custom command extensions are deferred.
P2-10 and P2-11 waited, at the user's request, for P3.

1. **P3-21**: the Explorer interface.
2. Release 0.4.0 (the user's call): the Preferences pages, extensions, the
   Windows shell's parts and, once done, the Explorer interface.
3. The upstream PRs for the carried patches (ADR 0006), waiting on WinSCP's
   maintainer: winscp/winscp#79 (portability), #80 (`LogEvent`), #81 (log
   file names), #82 (local EOL), #83 (OpenSSH import paths), #84, #85 (S3);
   each patch dropped when a pin move brings it in.

## P3 UI gap audit (2026-10-14)

The earlier P3 rows' "Not yet" notes, checked against the code (P3-16 closes
what is still missing). Win-only: WinSCP does it through the Windows shell,
registry, tray or window messages.

| Area | Gap | Status | Note |
|---|---|---|---|
| Panels | extension column | done | `uvExt`, `dvExt`: hidden by default, the header's menu, kept in the layout |
| Panels | incremental search with its status | done | name start, any part, all columns (`PanelSearch`); "Search: …" in the panel's status |
| Main window | WinSCP's toolbars (Commands, Sort, Preferences, Transfer, Custom commands) | done | 2026-10-14, with P3-17's icons |
| Synchronize | selected files only; by checksum; "use same options next time" saved | done | `spSelectedOnly` with the panels' selection as the filter, `spByChecksum` where the server can, `GUIConfiguration`'s SynchronizeParams and SynchronizeModeAuto (the direction asked about) |
| Synchronize checklist | calculate sizes; find move candidates; custom commands | done | Tools > Calculate, Calculate All, Move, Find Move Candidate; Custom Commands, the "diff"-style local ones (`!^!`) on each selected pair on both sides (BothCustomCommand) |
| Preferences > Panels | incremental search; preserve panel state; full row; double-click action; panel font; Explorer-style selection; tree | done | WinSCP's keys (Explorer-style selection's default, Mouse only, keeps the arrows off the selection and commands take the focused file); the directory tree as Options > Left/Right Panel > Tree (DriveView, its height kept; Ctrl+Alt+T): the remote one loading a folder's subfolders as it unfolds, the local one the file system's (the drives first on Windows) |
| Preferences > Transfer | presets' autoselection rules (edited, matched) | done | the preset dialog's rule group with Current; matched on each panel load and tab switch (`TransferPresetAutoSelect`), back to the preset chosen by hand; announced as `CopyParamAutoSelectNotice` says, else noted in the status bar |
| Preferences > Background | parallel threshold; enable queue by default; queue list visibility on the page; Process Queue | done | core's `ParallelTransferThreshold` (KB, -1 off, 100 MB offered); `EnableQueueByDefault` sets each session's `TTerminalQueue::Enabled`; Commands > Queue > Process Queue toggles it |
| Preferences > Window | minimize to tray; single instance | done | `MinimizeToTray` through QSystemTrayIcon, offered only where the desktop has a tray (GNOME needs an extension); `ExternalSessionInExistingInstance` over a per-user QLocalServer in place of WM_COPYDATA, Shift reversing it, never with /NewInstance, /RawConfig, a command or a download URL |
| Preferences > Security | master password | done | TWinConfiguration's, carried in `TShellConfiguration` (AES-encrypted stored passwords, the verifier under Security); asked once per site read, a refusal leaving the site's passwords as stored; saving one with a password asks first, as WinSCP's dialogs do; other instances found by process, as `AnyOtherInstanceOfSelf` |
| Preferences > Security | trusted host CAs | done | on the Security page, as WinSCP has them: core's `SshHostCAList` edited (TSshHostCADialog: a key typed or loaded, fingerprinted, the hosts checked); "Load authorities from PuTTY" and "Edit in PuTTY" on Windows only (PuTTY keeps them in the registry) |
| Preferences > Integration | "Send to" | done | the Send To shortcut (Preferences, and the Site Manager's for a site) on Windows only, as Explorer's; it runs `/upload`, now handled as WinSCP's standalone upload (the session, the Copy dialog unless `/defaults`, then the program ends); the Site Manager's Desktop Icon too |
| Editor | reload from the server; save all; window position kept | done | Reload downloads again (`ExecutedFileReload`); Save All (Shift+Ctrl+S); the first editor's place as `Editor.WindowParams` |
| Properties | local files' | done | the system's own: the Windows shell's dialog (SHMultiFileProperties), elsewhere the desktop's file manager's (FileManager1's ShowItemProperties over D-Bus: Nautilus, Dolphin, Nemo, Caja) |
| Properties | S3 ACLs | done | as cpAcl: a file's ACL in the rights frame (DisplayAsAcl: Owner, Users, Everyone; R, R ACL, W ACL), set and read back; not for folders |
| Properties | checksums through the secondary shell | done | CanCalculateChecksum's: offered where only the secondary shell calculates them, the shell opened first (EnsureCommandSessionFallback), for Synchronize by checksum too |
| Navigation | Go To's bookmark list; Location Profiles' opened folders kept; mask hints | done | the Open Directory button's menu (`CreateOpenDirMenu`); `NodeOpened`; the mask hint buttons (Select, Filter, Edit mask, Find files, Transfer settings) |
| Session tools | Copy dialog's transfer code; OpenSSH key conversion on import | done | the Copy dialog's Generate Code (DoGenerateTransferCodeDialog: script and .NET code, no URL page); an OpenSSH import's keys offered for conversion first (TImportSessionsDialog::Execute), its paths kept POSIX ones on POSIX (`core-openssh-import-posix-paths.patch`); the console's size is kept (`ConsoleWin.WindowSize`) |
| Custom commands | local commands; extensions and their options | done | local commands run as LocalCustomCommand (on local files, or on remote ones downloaded first and what changed uploaded back; output copied or shown, a failure's exit code an error), through the shell on POSIX, as a command line on Windows; extensions deferred (decisions, 2026-10-15) |
| Keep up to date | start in a new window; minimize to the tray | done | Minimize a split button with Minimize to System Tray (where the desktop has a tray), the dialog going with the window; Start in New Window as SynchronizeInNewWindow (a hidden copy of the site, `/rawtransfersettings`, `/keepuptodate // local remote params options`), `/keepuptodate` run standalone; presets are a combo, not WinSCP's split button |
| Workspaces | local tabs saved | done | as WinSCP's (TSessionData::IsLocalBrowser: both local directories, no host), opened again as local tabs |
| Clipboard | remote files pasted into the remote panel (duplicate) | done | into the same session's panel, through the Duplicate dialog (foRemoteCopy), from wherever they were copied; between sessions only through a local panel |
| Save Session as Site | create desktop shortcut | done | `.lnk` through IShellLink on Windows (its arguments, as CreateDesktopSessionShortCut), a desktop entry on POSIX |
| Done since their notes | Shift+F5/F6, kept layout, Synchronize's transfer settings, keep up to date, location profiles, compare criteria, editor font, external editors, S3 tags, bookmark shortcuts, commands list, transfer presets, open with the associated application, URL handler, folders of sites on the command line | done | |

## Decisions taken

| Date | Decision | Rationale |
|---|---|---|
| 2026-10-15 | linscp's installer registers WinSCP's drag & drop shell extension (`DragExt64.dll`, from the pinned source) only when no other copy is registered | It is WinSCP's own extension, its class, mapping and protocol unchanged, so one registered copy serves both programs; registering over WinSCP's would leave WinSCP without it once linscp is uninstalled |
| 2026-10-15 | linscp offers the Explorer interface (P3-21) and the Windows shell's jump list, system context menu and drag & drop shell extension (P3-22…24); WinSCP's own services (tips, usage statistics, the donors' automatic updates) stay out for now | The user's call (Q-13) |
| 2026-10-15 | P2 exit gate (P2-11) passed: the user's daily work in linscp | The user's sign-off: it does the daily work |
| 2026-10-15 | P3-20: extensions as WinSCP has them, its own code (`TCustomCommandType`, sliced) loading them; the lists, folders and keys of `TWinConfiguration` carried in `TShellConfiguration` (`common`: the program's folder; `commonext`: its Extensions, on POSIX `share/linscp/Extensions`; `userext`: the configuration's folder) | Extensions written for WinSCP install and run unchanged; the configuration keys (`ExtensionsOrder`, `ExtensionsDeleted`, `ExtensionsShortCuts`, `CustomCommandOptions`) carry over both ways |
| 2026-10-15 | The Windows packages carry WinSCP's .NET assembly (net40, built from the pinned source with linscp's patches) in a `dotnet` folder, with `linscp-cli` as the `WinSCP.exe` it starts; the shell points `WINSCP_PATH` there | WinSCP's own extensions are PowerShell scripts that load the assembly from `%WINSCP_PATH%`; the assembly looks for `winscp.exe` beside itself and checks its product version, which `linscp-cli` shares. In a folder of its own so that no `WinSCP.exe` lands on the search path. Verified on Windows 11: Windows PowerShell 5.1, the assembly and `linscp-cli` listing a server |
| 2026-10-15 | `@require` checked as WinSCP checks it, the versions found by linscp's own code: on Windows the registry keys and folders upstream reads (not PowerShell 7 from the Store); on POSIX .NET's shared runtimes and `pwsh --version` | Upstream's are in Setup.cpp among GUI code linscp does not compile. On POSIX there is no Windows PowerShell or .NET Framework, so WinSCP's own extensions (which require them) are not offered there |
| 2026-10-15 | P3-19: the Drag & Drop and Updates pages offered after all (P3-04d and P3-04e had left them out), with what applies to linscp: drags out through the temporary folder and its free-space warning; linscp's GitHub releases, checked as WinSCP's period has it (Never by default), through its proxy settings, pre-releases as "beta versions" | The user's call. The fake file stays the shell extension's (ADR 0012); the authorized e-mail and usage statistics are WinSCP's own service and are not offered |
| 2026-10-15 | The interface theme asks Qt for the colour scheme; where the platform cannot switch (X11's generic theme, KDE's), Fusion with linscp's own light or dark palette | Qt 6.8's `setColorScheme` switches on Windows and the portal's desktops only; Fusion's standard palette follows the system's scheme, so the light one is spelled out |
| 2026-10-15 | After uploads, downloads and synchronizing, the remote panel shows what core has; core reads the directory again as `AutoReadDirectoryAfterOp` has it | Panels > Remote's "Automatically refresh directory after operation" (Ctrl+Alt+R) would mean nothing if the shell always read it again; one listing instead of two |
| 2026-10-15 | Preferences' pages as WinSCP's tree (each section's page with its subpages under it), not a flat list | 23 pages: WinSCP's tree, and its names readable |
| 2026-10-15 | Version 0.3.0 tagged and published as a GitHub pre-release, with the CI packages and now the Windows portable zip | The user's call: it ships the Check for Updates crash fix (0.2.0 crashes there), the rest of the Commander menus (P3-15), thumbnails, the real-server tests, the Site Manager folders on save, the preset captions and Donate |
| 2026-10-15 | Help > Donate offered, linscp's GitHub Sponsors page for now (one constant, `DonateUrl`) | The user's call: linscp's own donation pages come later |
| 2026-10-15 | A portable package: on Windows a zip of the deploy folder with an empty `linscp.ini` (WinSCP's portable mode: an INI beside the program; `linscp-cli` shares it), on Linux the AppImage with a `WinSCP.ini` beside the `.AppImage` file | The user's call; WinSCP offers a portable package too |
| 2026-10-15 | WinSCP's OpenSSL kept out of the programs' dynamic symbols on POSIX (`--exclude-libs,libssl.a:libcrypto.a`) | The system's libssl is loaded as well (Kerberos' libldap, Qt's TLS backend); with our OpenSSL exported, the two mixed and Help > Check for Updates crashed in SSL_CTX_new |
| 2026-10-15 | The Windows suite runs natively on the VM through `port/tools/vm_tests.py` (ctest's own list, made native), beside Linux and Wine | The user's call: every change tested on Linux, Wine and real Windows; the first run found the deploy folder without Qt's TLS backend (no HTTPS) and image plugins |
| 2026-10-15 | Real test servers (P3-18) in one image, one container per test on the host's network (free loopback ports, FTP's passive ports needing no mapping), the test's directory mounted at its own path, the servers as user linscp with the host user's uid; versitygw for S3, not MinIO | Each test keeps owning its server's files, as with the loopback fixtures. MinIO refuses secret keys under 8 characters (the suite's "secret") and keeps objects in its own format; versitygw serves a directory as buckets. OpenSSH stays out of the GUI tests' SFTP: its chroot cannot give a writable "/", which they expect (rclone stays) |
| 2026-10-15 | The shim's AnsiString and UnicodeString data() return NULL when empty, as C++Builder's | Core tells "none" by it: S3FileSystem's empty session token went out as an empty, unsigned x-amz-security-token header, which a real S3 server rejects |
| 2026-10-15 | `core-s3-acl-not-implemented.patch` and `core-s3-folder-tags.patch` carried, to offer upstream | Against versitygw WinSCP's Properties fail: GetObjectAcl answers NotImplemented (many S3-compatible servers keep no object ACLs), and a folder that is only a key prefix has no tags to read; NotImplemented now reads as no grants, and tags load for files only, as the dialog shows them |
| 2026-10-15 | Help's pages are linscp's (GitHub: homepage, issues as the support forum, releases as the history) but for Contents, WinSCP's documentation, which linscp's interface follows; Check for Updates asks GitHub's releases API; Show Tips and Donate are left out | WinSCP's pages, tips and update service are about WinSCP; linscp takes no donations |
| 2026-10-15 | P3-18 added: a devcontainer with real test servers for every protocol (FTP/FTPS and S3 included), the suites run against them; done after P3-15 | The user's call. The suite's FTP and S3 servers are pyftpdlib and moto, written for tests; real servers catch what they do not |
| 2026-10-15 | P4 (cleanup) dropped for now, all four tasks; it may be reopened | The user's call: retiring the VCL frontend and collapsing the dual-target code would make linscp unmergeable upstream, which stays the goal (ADR 0006); the C++Builder build keeps working |
| 2026-10-15 | Version 0.2.0 tagged and published as a GitHub pre-release, with the four packages CI builds | The user's call; P2-11 (the exit gate) is still to be driven by the user |
| 2026-10-15 | Every carried core patch offered upstream: the twelve portability ones as one PR of independent commits (winscp/winscp#79), the `LogEvent` link fix and the three POSIX behaviour fixes as their own (#80 to #83), written in upstream's style rather than our line-count-preserving form | The user's call; one portability PR with a short justification, bugs and behaviour separately so each can be judged alone |
| 2026-10-15 | Packages: a .deb, an AppImage and a Flatpak on Linux, an Inno Setup installer on Windows laid out as WinSCP's (all-users or current-user, translations a component, desktop icon, Send To, URL handler and search path as tasks the program does: `/RegisterForDefaultProtocols`, `/AddSearchPath` and their reverses on uninstall); app id `io.github._99ecarvalho.linscp` | The user's choice of formats; D-Bus names cannot have a segment starting with a digit, hence the underscore; the installer is linscp's own script, not a copy of WinSCP's |
| 2026-10-15 | Inno Setup pinned at 6.7.3 (`port/tools/fetch_innosetup.py`, SHA-256), installed portable and run under Wine, locally and in CI | WinSCP's installer is Inno Setup 6; a pinned compiler keeps the installer reproducible without a Windows build machine |
| 2026-10-15 | `-fno-builtin-wcslen` on POSIX | clang 21 (the Flatpak SDK's) turns the shim's 2-byte length loops into calls to glibc's 4-byte `wcslen`; static initializers then looped over overlong strings. Windows' `wcslen` is 2-byte |
| 2026-10-15 | `core-terminal-logevent-not-inline.patch` carried, to offer upstream | `TTerminal::LogEvent` is defined `inline` in Terminal.cpp but called from other units; optimized builds (the packages' Release) do not emit it and fail to link |
| 2026-10-15 | linscp's application icon is its own (`assets/app/linscp.svg`: an original tile around Lucide's ISC `folder-sync`), not WinSCP's with an "L" | WinSCP's license.txt part B forbids reusing its images or modifications of them in other software, which packaging would have done |
| 2026-10-15 | WinSCP pin moved to 6.7.1 (`e19a9a3e`); `core-script-rmdir-full-path.patch` and `putty-random-add-noise-section.patch` dropped, `putty-aes-ni-non-win64-clang.patch` cut to its `MM_SETR_EPI8` half, `get_remote_username` no longer an override | Upstream merged linscp's #69, #70, #71, #72, #74, #76 and #77 and removed core's own `get_remote_username`; 16 closure sites moved by lines only |
| 2026-10-15 | Custom command extensions (.WinSCPextension) deferred; P3-16 closed without them | The user's call: WinSCP's extensions are PowerShell scripts over WinSCP's .NET assembly, which drives winscp.com, not linscp, so most would not run; packaging (P2-10) goes first |
| 2026-10-14 | P3 exit gate: every WinSCP dialog has its counterpart (the Explorer interface aside); the gate also waits for the Commander commands users reach daily (P3-14); the rest of WinSCP's menus are backlog (P3-15). Answers Q-1 | The user's call at the gate review: the gap was ~50 menu items, about a dozen of daily use |
| 2026-10-14 | linscp's icons come from bundled, freely licensed sets (Lucide, Tabler, Fluent UI), the user choosing; not WinSCP's | WinSCP's images are licensed stock icons that may not be reused in other software (its license.txt, part B) |
| 2026-10-14 | P3-01 (`.dfm` → `.ui` converter) dropped | Every dialog was written by hand from the `.dfm`s without it; nothing is left for it to convert |
| 2026-10-10 | clang everywhere; GCC unsupported | GCC cannot compile `__declspec(property)`; verified locally |
| 2026-10-10 | mingw-w64 (`*-windows-gnu`) over clang-cl/MSVC | No MSVC licence or SDK download; sysroot is in Debian apt |
| 2026-10-13 | The panels follow WinSCP's defaults: hidden files not shown (dot files remotely, hidden files locally), local deletes to the trash; shell texts that core already has (confirmations, errors) come from core | WinSCP's `ShowHiddenFiles = false` and `DeleteToRecycleBin = true`; core's texts reach every language WinSCP's translators cover |
| 2026-10-13 | Core's texts are translated with WinSCP's own translations, loaded by the shim where upstream loads its translation DLL; the shell's with Qt Linguist; Qt's with Qt's (ADR 0013) | 55 languages by WinSCP's translators at no cost, and core unchanged. The shell's texts are new and get .ts files; `--lang` chooses until preferences exist (P3-04) |
| 2026-10-13 | The Windows toolchain is llvm-mingw against MSYS2's Qt 6.8.2 and its C++ runtime, all pinned; the GCC sysroot is retired (ADR 0011) | Debian has no Qt for mingw. MSYS2's clang64 Qt is that ABI and prebuilt; Debian's own Qt version, so its host tools serve the cross-build. Wine testing from Linux is kept, which clang-cl/MSVC would have weakened |
| 2026-10-13 | P2-10 (packaging) and P2-11 (the P2 exit gate) wait; P3 goes first | The user's call: parity features before packaging. They stay `todo` |
| 2026-10-13 | Remote files dragged out to other programs are downloaded on demand to a temporary directory (WinSCP's drag without its shell extension); no XDS | XDS exists only on X11 and GNOME Files dropped it; on-demand URLs work on X11, Wayland and Windows (ADR 0012) |
| 2026-10-14 | External editors' remote copies are watched as WinSCP's files with no process: uploaded on each change, kept until the session closes (ADR 0015) | The same on every platform and for every editor, the associated application included; waiting on an SDI editor's process is a refinement for later |
| 2026-10-14 | Open in PuTTY is upstream's, sliced; on POSIX PuTTY gets the session on its command line, and without PuTTY the default is `ssh` in the desktop's terminal (ADR 0014) | PuTTY's POSIX sessions are files the registry export cannot reach, and upstream already builds the command line for the Store's PuTTY; WinSCP's session patterns make the terminal command a plain setting the user can change |
| 2026-10-14 | The menu bar is WinSCP's Commander's: Local (Left), Mark, Files, Commands, Tabs, Options, Remote (Right), Help, each in WinSCP's order; items linscp lacks are left out, never reordered (ctest `qt_gui_menus` reads WinSCP's forms) | Users know WinSCP's menus; the shell's own File, Go To and View menus had grown in their own order and hid commands only keys reached |
| 2026-10-14 | Save Workspace waits for session tabs (new task P3-13) | A workspace is WinSCP's set of open tabs; the shell holds one session, so there is nothing yet to save or reopen together |
| 2026-10-14 | Keep-up-to-date is upstream's `TSynchronizeController`, sliced, over a linscp `TDiscMonitor` with no thread of its own: the GUI's timer polls it (ADR 0014) | Upstream's controller unchanged, closure sites discovered as core's; one directory watcher for the shell and the command-line client |
| 2026-10-14 | Upstream code bound up with the VCL GUI in one file is compiled from slices the transform cuts by anchored line ranges, outside the mirror: first WinSCP's custom commands (ADR 0014) | The user's choice over mirroring ~930 lines by hand: upstream's own logic, extensions' parsing included, with no drift; anchors that must match exactly once fail a pin bump loudly |
| 2026-10-13 | Files are edited as WinSCP edits them: its internal editor now, external editors with Preferences > Editors; core's local end of line is the platform's (ADR 0015, Q-3) | WinSCP has both and defaults to the internal one; its temporary directories, transfer settings and queue upload are followed, so editing behaves the same. Core assumed CRLF locally, wrong on POSIX |
| 2026-10-13 | The shell's settings are upstream's `TGUIConfiguration`, compiled as it is from the transformed tree, with `TWinConfiguration`'s keys mirrored by the shell's `TShellConfiguration` (ADR 0014) | Upstream's code keeps presets, synchronize and queue options and the language exactly as WinSCP does, and `WinSCP.ini` carries over both ways; a spike showed 23/11 errors (Linux/Windows), all shim-sized. `TWinConfiguration` is bound to the VCL GUI, so it is mirrored instead |
| 2026-10-10 | Generated-tree model, not a source fork | Keeps the upstream diff structurally empty |
| 2026-10-10 | Keep the VCL RTL shim permanently | De-shimming is a second project of similar size with no user benefit |
| 2026-10-10 | Decouple shim cleanup from 100% UI parity | Coupling them would add ~a year of dual-build burden |
| 2026-10-10 | Two frontends over one core; never mix VCL and Qt in-process | Impossible on Linux, unmaintainable on Windows |
| 2026-10-10 | Windows target first in P0 | Far-NetBox already proves the core compiles for Windows; separates the two risks |
| 2026-10-10 | Line-preserving transform kept, but demoted from "constraint" to asserted property | It costs nothing — all 591 core declarations are already single-line — and its real payoff is a reviewable zero-offset `diff` against upstream, not debugger ergonomics. Asserted by a test; abandon if it ever conflicts with something substantive |
| 2026-10-10 | `index=` properties handled by a `PROP_R_GI` macro rather than a patch | Removes the last 4 manual cases, so the transform covers 613/613 with an empty patch series |
| 2026-10-10 | Far-NetBox used as reference; verbatim reuse deferred pending licence clarification | Its README says GPLv3 but it ships the GPLv2 text (Q-6) |
| 2026-10-10 | Published as [99ecarvalho/linscp](https://github.com/99ecarvalho/linscp), public, GPL-3.0 | `main` is the released state; active work happens on `qt` |
| 2026-10-11 | Compute cost recorded as measured token usage, not an estimate | The branch so far cost $297 of Opus 5 inference (99% of tokens are cache reads). Confirms compute is not the constraint, and shows Opus 5.5 would halve it — see [ESTIMATES.md](ESTIMATES.md) |
| 2026-10-11 | VCL's `TThread` class not shimmed | Its only user, `KeyGen.cpp`, is in none of upstream's `.cbproj` files and `TKeyGenerator` is referenced nowhere else — dead code upstream. Core's own `TSimpleThread` uses `StartThread` and Win32 directly |
| 2026-10-11 | P1-05b scoped as Win32 kernel objects on POSIX, plus Delphi `BeginThread`/`EndThread` on both targets | That is what core actually calls: 28 `CloseHandle`, 6 waits, 5 `CreateEvent`, 10 `GetCurrentThreadId`. `BeginThread` is written once over `CreateThread`, so upstream's frontend `StartThread` ports unchanged |
| 2026-10-11 | POSIX kernel objects share one handle table, one lock and one condition variable; handle values are never reused | Makes `WaitForMultipleObjects` over mixed threads and events — including atomic `WaitAll` — correct by construction. Core has a handful of threads, so the coarse lock costs nothing measurable. Never reusing values turns use-after-close into `ERROR_INVALID_HANDLE` |
| 2026-10-11 | Shim XML interfaces live in `Xml::Xmlintf`; only the `_di_` aliases are global | Embarcadero's own layout. At global scope ours collided on Windows with the COM `IXMLDocument` from `<msxml.h>`. Core names them only through `_di_` aliases and `Xmlintf::` |
| 2026-10-11 | Closure-site discovery compiles a separately transformed, *unbound* tree | Compiling the ordinary tree saw the known sites already wrapped and "rediscovered" only new ones — it cut 62 sites to 2. Now idempotent: a second run is byte-identical |
| 2026-10-11 | VCL's implicit `SysUtils::LoadStr` and `Vcl.Menus::StripHotkey` supplied by the shim, not by patching core | Core receives both through `<vcl.h>` and never declares them. `LoadStr` is built on the existing `LoadString` table, so there is still one resource path |
| 2026-10-11 | Core is compiled with upstream's own preprocessor configuration, read from `ScpCore.cbproj` by `port/tools/upstream_defines.py` | Copying the list would drift. Applied by the census, closure discovery and CMake alike, shim included, because `UNICODE` decides which Win32 A/W function every macro names. Windows-only names are dropped on POSIX; `_CRTIMP=` is dropped everywhere, as C++Builder's CRT linkage |
| 2026-10-11 | Census and discovery build the Debug configuration | Its `DebugAssert` expressions are code that must compile too; Release compiles them out, which hid ten closure comparisons |
| 2026-10-11 | `port/compat` withdrawn; supersedes the 2026-10-10 decision creating it | `help.h` was compensating for the missing `WINSCP` define, not a gap in upstream: with `WINSCP` set, PuTTY's `platform.h` never includes it |
| 2026-10-11 | Closure binding covers Borland's four shapes: `Name`, `&Name`, `obj.Name` / `&obj.Name`, `p->Name` / `&p->Name` | Recorded as the method name's column; the transform recovers the shape from the text and refuses longer chains or a call as the object. The verifier recomputes the rewrite with an independent regex, so a bug in either is caught by the other. The old binder had been emitting `obj.CLOSURE(Name)`, invalid C++, in two units nothing yet compiled |
| 2026-10-11 | Win32 APIs core calls under the bare name get forwarding functions, not their A/W macros | `vcl_win32_undef.h` removes the macros so they cannot rename the shim's VCL functions; it had also removed `CreateFile` and four others core calls as the API, leaving those calls nothing to bind to. Exact signatures, so core's `NULL` arguments still convert |
| 2026-10-11 | `__int64` is clang's MS-extension keyword, never a macro | A `long long` macro -- ours, and mingw's own -- breaks core's `__int64(x)` functional casts |
| 2026-10-11 | `Random` is Delphi's own generator | It reproduces the published Turbo Pascal / Delphi sequence from a zero seed exactly, which the test pins |
| 2026-10-11 | C++Builder's static class properties become static `const` pointers (`TEncoding::UTF8`) | clang accepts `static __declspec(property)` but compiles it as a plain static member, failing only at link (see verified facts). Pointers serve both of core's spellings, as a value and through `->`; `[[clang::require_constant_initialization]]` guarantees no static-init-order exposure |
| 2026-10-11 | `Win32MajorVersion()` and friends are functions, read from `RtlGetVersion` | Core calls them as functions. `GetVersionEx` reports 6.2 to an unmanifested executable, which would turn core's `IsWin10()` false on 10 and 11. Computed on first use, which also removes a dynamic-initialiser hazard |
| 2026-10-11 | `StaticArray<T, N>` keeps one slack element, filled by its owner with a value no input matches | Core passes `Size()` as an open array's *high index* (`ParseShortEngMonthName`) and reads one past the end. In C++Builder that is whatever follows; here it is defined and means "not found" |
| 2026-10-11 | The census measures every core unit in `ScpCore.cbproj`, read from the project, in parallel | Ten units were a sample. Reading the list from upstream means a unit it adds fails the ratchet until baselined, rather than going unmeasured. 66 compiles take about 5 s |
| 2026-10-11 | Case-mismatched upstream includes get checked-in forwarding headers in `port/casefold` | They resolve only on case-insensitive file systems, so a Linux host fails even when cross-compiling for Windows. Explicit, not generated: a basename match would have mapped `putty/windows/gss.c`'s `security.h` -- the Windows SDK header -- onto core's `Security.h`. `check_include_case.py` applies a same-directory rule and fails on drift either way |
| 2026-10-11 | `ARRAYOFCONST` expands to C++Builder's two arguments, a `TVarRec` pointer and the high index | Core's own functions take open arrays that way (`ExecCommand`, `ExtException`), so a single container argument cannot reach them. The count comes from `sizeof` over an unevaluated call, so each value is evaluated once -- C++Builder's macro evaluates them twice |
| 2026-10-11 | The XML DOM follows Delphi's model: text and comment nodes as children, Null attribute variants, raising loaders | FileZilla keeps a site's name as a text node among element children, found by `NodeType`; core tests absent attributes with `IsNull()`; and `ImportFromFilezilla` has no handler around `LoadFromFile`, so a malformed file must raise rather than import nothing. The earlier `bool` loaders were a divergence justified by a wrong reading of core |
| 2026-10-11 | `TCustomIniFile`'s virtuals take parameters by value, and its abstract set is Delphi's eight | C++Builder declares Delphi virtuals that way and core's `TOptionsIniFile` overrides with exactly those signatures; by-reference parameters made its methods new functions and left the class abstract |
| 2026-10-11 | `TStrings::Text` uses CRLF on both platforms | Delphi's POSIX RTL uses LF. Core builds configuration and server-facing text through `Text`, and identical output everywhere matters more |
| 2026-10-11 | try/__finally brace depth is measured over code only | A `}` inside a comment in a `__finally` body put `LINSCP_END_FINALLY` one brace early in ScpFileSystem.cpp. Comments and string/character literals are now blanked before counting; still 179 blocks |
| 2026-10-11 | The transform finds its patch series relative to itself, and a missing one is an error | CMake runs it from the build tree, where the relative path found nothing: every CMake-built tree had **no closure bindings at all**, and the verifier, loading the same way, agreed. The census ran from the repo root and never saw it. `bind_closures.py --require-bound` now checks the tree the build compiles |
| 2026-10-11 | Header function definitions at namespace scope get `inline` (transform rule, verified) | `Common.h` defines `IsWideChar` without it: one definition per including unit, 31 in all, which C++Builder's linker tolerates and the standard forbids. A rule rather than a patch, so a new one upstream is handled too; a global `--allow-multiple-definition` would have hidden real duplicates |
| 2026-10-11 | The shim's `ExpandEnvironmentVariables` removed | Core defines it itself, with Windows `%VAR%` semantics through `ExpandEnvironmentStrings`. The shim's was an invented POSIX `$VAR` variant, a duplicate the syntax-only census could not see. Linux gets `ExpandEnvironmentStrings` with Windows semantics instead (P1-07b) |
| 2026-10-11 | Include paths with backslashes are normalised to `/` (transform rule, verified) | Only Windows resolves `#include "ssh\gss.h"`; clang accepts it when targeting Windows and not otherwise. Six such includes, five in core: part of why units abort on Linux. A forwarding file named with a backslash would break Windows checkouts |
| 2026-10-11 | The patch series exists, applied before the rules, strictly: exact context, line counts preserved | First patch: `putty-64bit-conf-defaults.patch`. WinSCP stores string option defaults as `(int)"..."` in an int-first union, which truncates pointers on x86-64; the first union member becomes `intptr_t`. Upstream is built 32-bit only, so this is new territory for its code |
| 2026-10-11 | Defines are per project, not global | PuTTY is built without `UNICODE`, core and FileZilla with it, and `UNICODE` decides which Win32 A/W function each macro names. Core's are public on the shim, so everything built against it sees them |
| 2026-10-11 | Core's overrides of PuTTY symbols are expressed by weakening PuTTY's copies in the archive | `PuttyIntf.cpp` redefines `appname` and `get_remote_username`, relying on C++Builder's linker keeping the first definition. GNU objcopy marks PuTTY's weak (llvm-objcopy cannot for COFF); `check_overrides.py` keeps `overrides.txt` exact. Sources untouched, and dual-target, which a weak attribute in a patch would not be |
| 2026-10-11 | `aes-ni.c` is compiled with `-msse4.1 -maes`, its CPU check verified free of those instructions | WinSCP dropped PuTTY's per-function target attributes, which MSVC does not need; restoring them would break the MSVC build of the same file. `check_isa_free.py` disassembles `aes_ni_available` and fails on any SSE4.1 or AES-NI instruction |
| 2026-10-11 | Case-mismatched includes are respelled in the generated tree (transform rule); `port/casefold` withdrawn | Supersedes the forwarding headers: once FileZilla compiles from the generated tree, a forwarder to the pristine header would mix untransformed code into a transformed unit. Same same-directory rule, now a verified transform rule; core, PuTTY and FileZilla all include from the generated tree |
| 2026-10-11 | FileZilla and WinSCP's MFC subset join the transform's scope; two more patches | `mfc-x86-64.patch`: the subset keys off `_X86_` where it means "not 32-bit", so it wants an unvendored `afxv_cpu.h`, withholds `GetLength64` and asserts on `I64`. `filezilla-ws2tcpip-case.patch`: `<Ws2tcpip.h>`, the only system include whose case only Windows forgives |
| 2026-10-11 | MFC and FileZilla units are compiled with a forced prelude: `<windows.h>` without its A/W macros | FileZilla includes `afx.h` before any core header, so MFC's members were declared under the macros (`CString::LoadString` became `LoadStringW`) and then called without them. The prelude drops windows.h's outer guard after removing the macros, so `afx.h`'s include-order check passes and its re-include brings nothing back. MFC's `CString::LoadString` then reaches the shim's resource table, which already holds FileZilla's strings |
| 2026-10-10 | A PR-able branch against `winscp/winscp` will be **generated**, not hand-maintained | linscp shares no history with upstream, so its `qt` branch is not directly mergeable. Since the port is mechanical, the fork branch can be produced on demand (Q-7). Rehearsed end to end — see facts below |
| 2026-10-10 | Transform must be byte-exact outside property lines | A full-file rewrite (CRLF loss) turned a 613-line change into 76,551. Now asserted by `verify_transform.py` |
| 2026-10-10 | Field/accessor classification is convention-first, parens only as fallback | The parens heuristic false-positives on constructor initialiser lists; WinSCP's `F<Upper>` / `Get*`/`Set*` convention is airtight. Truly unclassifiable names now fail the run instead of producing a wrong-arity macro |
| 2026-10-10 | Windows releases link statically via lld | `-fuse-ld=lld -static -L/usr/x86_64-w64-mingw32/lib`; leaves only KERNEL32 and msvcrt as deps |
| 2026-10-10 | CMake hard-fails on non-clang compilers | A clear configure-time error beats a confusing failure in the first transformed header |
| 2026-10-10 | The transform stamp lives outside the generated tree | `verify_transform.py` treats any extra file as a violation, which is the behaviour we want; the stamp was the first thing it caught |
| 2026-10-10 | **Accepted (Q-8):** `-fshort-wchar` with `std::u16string` storage; `std::wstring` banned in `port/` | Only option leaving upstream's thousands of `L"..."` literals untouched. Enforced by `check_wide_abi.py` and a `static_assert`. ADR 0001 |
| 2026-10-10 | `-fshort-wchar` set globally, not per-target | One translation unit without it is an ABI landmine that links cleanly |
| 2026-10-10 | `UnicodeString` case folding is ASCII-only for now | Delphi's is locale-aware; core uses it on protocol tokens and switch names. Recorded as a known limitation, not silently equivalent |
| 2026-10-10 | `vcl.h` transitively exposes the Win32 API | Matches C++Builder, which upstream's `CorePCH.h` depends on: it includes `<vcl.h>` and core's headers before `<Winapi.Windows.hpp>`. Fixing this alone removed 9 of 24 missing types |
| 2026-10-10 | UTF-8 decoding substitutes one U+FFFD per maximal subpart | Unicode's recommended practice. The first implementation emitted one per byte; a test on truncated input caught it |
| 2026-10-10 | Byte strings keep C++Builder's code-page template shape | Core relies on `RawByteString` and `UTF8String` being distinct types; collapsing them would let byte reinterpretation happen silently |
| 2026-10-10 | Date and time are combined only through `ComposeDateTime` | Delphi keeps the time of day positive for negative dates, so `Date + Time` lands a day late at the mirrored time. Correct for file timestamps, wrong in general and wrong silently |
| 2026-10-10 | `TCriticalSection` wraps `std::recursive_mutex` | Delphi's is re-entrant; a plain `std::mutex` would deadlock on core's re-entrant paths rather than fail a test |
| 2026-10-10 | Compile-census numbers are ratcheted in CI | No core TU compiles cleanly yet, so a reintroduced missing type would otherwise present as a passing build |
| 2026-10-10 | `TStrings` is 0-based while `UnicodeString` is 1-based | Both match their Delphi originals. Two bases in one codebase is a hazard, so each is pinned by a boundary test rather than left to memory |
| 2026-10-10 | `TStrings` references objects, never owns them | Delphi does not free them and core owns them elsewhere; freeing here would be a double-free. Asserted by a test that outlives the list |
| 2026-10-10 | Chained subscripts on indexed properties are unsupported | clang reads `Prop[i][j]` as a two-argument getter call. Core has 0 occurrences; `check_properties.py` fails if an upstream change adds one |
| 2026-10-10 | Win32 A/W name macros are undone in `vcl_win32_undef.h` | `<windows.h>` renamed the *definition* of `ReplaceText` to `ReplaceTextW` while the header still declared `ReplaceText`. Both compiled; only the link failed, and only because a test called it. A link-time guard now references every colliding name |
| 2026-10-10 | `ARRAYOFCONST` pastes its parenthesised list onto a variadic call | C++Builder builds an open array of `TVarRec`, which needs compound literals. `ARRAYOFCONST((a, b))` becoming `MakeFormatArgs(a, b)` is ordinary C++ |
| 2026-10-11 | XML is parsed for real, using the expat upstream vendors, rather than stubbed | A stub would silently import no FileZilla sessions. The mingw sysroot has no expat, so the vendored source is compiled for both targets, which also keeps them on one version |
| 2026-10-11 | Wide-character functions are renamed and the CRT names redirected by macro | They cannot be redeclared -- `<string>` pulls in `<cwchar>`, whose `wcschr` differs from ours only in return type -- and a macro also guarantees core's calls reach ours rather than glibc's |
| 2026-10-11 | `towlower`/`towupper` fold ASCII only, on both platforms | The Windows CRT folds only ASCII in the default locale, confirmed by running the test against it. Folding Latin-1 on POSIX alone would make the platforms disagree, which is worse than being narrow |
| 2026-10-11 | `try`/`__finally` uses the same lambda expansion on *both* targets | SEH would work on mingw, but one code path on both platforms is worth more than relying on it, and it removes a way for the two to diverge in behaviour |
| 2026-10-11 | The `try`/`__finally` transform measures brace depth on the *pristine* lines and applies every edit afterwards | `LINSCP_END_FINALLY` replaces a `}` with text containing no brace, so editing in place miscounted the depth of any enclosing block -- and core nests these |
| 2026-10-11 | One bare `return` inside a guarded body is permitted, by checked condition | Returning from the lambda runs the cleanup through the guard and then falls off the end of the function, which is what Borland does -- but only when the block ends the function and the return carries no value. Both are verified rather than assumed |
| 2026-10-11 | Every property form routes through a generated public wrapper | Borland's `__property` grants access to a private accessor from outside the class and ignores constness; `__declspec(property)` does neither, because it expands to an ordinary member call at the use site. The wrappers restore both behaviours, and the getter casts its result to the declared type because C++Builder is lax where the two differ in constness |
| 2026-10-11 | `UnicodeString` accepts numbers implicitly, yielding their decimal text | C++Builder does, and core depends on it. The hazard is real -- `UnicodeString s = 0;` gives `"0"` -- but diverging would mean editing core |
| 2026-10-11 | Globals that need dynamic initialisation are declared *before* the object that initialises them | Within a translation unit, dynamic initialisation runs in declaration order. Putting the initialiser first had its constructor assign to a `UnicodeString` that was not yet constructed, which segfaulted rather than failing to compile |
| 2026-10-11 | `TMask` matching is case-insensitive | Delphi's is, and core uses it for file masks users type without regard to case |
| 2026-10-11 | Closure call sites are **compiler-discovered, committed, then replayed** | The sites cannot be inferred by regex: whether an argument is a closure depends on the callee's signature. clang names each one exactly, so discovery records them to `port/patches/closure_sites.json` and `transform.py` replays that list, failing loudly if a recorded site no longer holds the expected identifier |
| 2026-10-11 | A diagnostic inside a macro expansion is resolved to the macro *definition* | clang reports it at the expansion point, so `SessionInfo.cpp`'s `ADSTR` macro alone produced 61 duplicate reports. Binding the definition once fixes every call site, and cut the work from 118 sites to 62 |
| 2026-10-11 | `TRegistry` on POSIX reports every key absent rather than empty | There is no registry, and returning defaults for settings a user had actually set would look to them like their configuration had vanished. The INI backend, which core already supports, is the correct one there (P1-06d) |
| 2026-10-11 | INI section and key names match case-insensitively | Delphi's do, and files written by WinSCP on Windows have inconsistent casing |
| 2026-10-11 | INI sections and keys keep their file order | Sorting them would churn the user's configuration on every save and make diffs useless |
| 2026-10-11 | `DynamicArray` shares its buffer on copy, as Delphi's does | Core declares `TSafeHandleStream::Read` as taking one *by value* and fills it for the caller. A deep-copying version would compile and silently return no data |
| 2026-10-11 | `win32_on_posix.h` exposes `<string.h>`, `<stdlib.h>` and `<stdio.h>` | `<windows.h>` does transitively, and core calls `strlen`, `strcmp` and `memmove` unqualified |
| 2026-10-10 | **Resource strings: supply Win32 `LoadString`, not a `.res` loader, and do not patch core** | core's `LoadStr` is already implemented, in terms of `HInstance`, `LibModuleList` and Win32 `LoadString`. Supplying those three means no `.res` binary format to parse, no core edits, and a purely additive change — the kind upstream could take rather than a fork of its own code. The table is generated from upstream's `.rc` files, so the strings track the submodule automatically |
| 2026-10-10 | `UnicodeString::c_str()` has a non-const overload returning a writable buffer | C++Builder's does, and core depends on it: `DoLoadStrFrom` calls `SetLength` and then passes `c_str()` to `LoadString` as the destination to fill |
| 2026-10-10 | `TList::Notify` is virtual and fires before a removal, after an addition | Core overrides it. `TObjectList` frees owned objects from inside it, so a wrong `Action` means a leak or a double free, neither of which announces itself |
| 2026-10-10 | `Extract` reports `lnExtracted`, never `lnDeleted` | That is how Delphi lets a caller take ownership back without the list freeing the object |
| 2026-10-10 | `TList::Sort` is stable, where Delphi's quicksort is not | Relying on an unstable order would be a latent caller bug either way, and stable is the safer guarantee to provide |
| 2026-10-10 | Derived streams re-expose the base's legacy `Seek` with a using-declaration | Overriding one overload hides the other. Core calls both on derived streams, so without this the legacy origins would stop resolving |
| 2026-10-10 | Stream exceptions follow Delphi's shape exactly | `EFilerError`, `EFOpenError` and `EFCreateError` are siblings under `EStreamError`, with `EReadError`/`EWriteError` beneath `EFilerError`. Core catches at both levels |
| 2026-10-10 | `port/platform` headers refuse to compile on Windows, and are POSIX-only on the include path | Putting them on a Windows target shadowed the genuine mingw headers, which presented as a 1,220 -> 783 "improvement". The `#error` makes that mistake loud |
| 2026-10-10 | `port/compat` is separate from `port/platform` | `help.h` is missing from upstream itself rather than being a platform difference, so it is needed on every target |
| 2026-10-10 | The ratchet records whether a unit aborted | An aborted parse reports a floor. Treating that as a distinct state stops a new abort from reading as sudden progress |
| 2026-10-11 | On Linux, WinSCP's PuTTY is compiled with its Windows platform layer, over the Win32-on-POSIX layer; Linux-specific services replace individual units (refines P1-09) | Core is written against PuTTY's Windows platform API (`select_result`, `do_select`, `get_handle_wait_list`, `HandleWaitList`), and WinSCP's changes to that layer carry real behaviour (per-session `do_select`, connect timeouts, socket buffers, source address). Taking PuTTY's `unix/` layer instead would mean reimplementing both against core's model. Where Linux genuinely differs -- the agent (`SSH_AUTH_SOCK`), GSSAPI, entropy, proxy commands -- a unit is replaced by one implementing the same API, drawing on the pinned PuTTY 0.85 release's `unix/` code |
| 2026-10-11 | On Linux PuTTY's narrow (`A`) Win32 names are distinct functions, and the bare names map to them in C without `UNICODE`; C++ keeps the wide bare names | PuTTY is C built without `UNICODE`, core C++ with it. In C a `char *` passed to a wide parameter is only a warning, so PuTTY on Linux is compiled with incompatible-pointer, int-conversion and implicit-declaration diagnostics as errors -- they found four such calls |
| 2026-10-11 | Winsock on POSIX is a `ws2_32` module of Winsock-signature functions, with `WSAEventSelect` emulated by a monitor thread that applies Winsock's re-enabling rules | `network.c` takes every socket function through `GetProcAddress`, so the module is the exact boundary where Winsock semantics live; the libc names keep their POSIX meaning for anything else. Registering re-evaluates the socket; FD_READ waits for `recv`, FD_WRITE for a send that would block, FD_ACCEPT for `accept`; FD_CLOSE is detected with `POLLRDHUP` while read is disarmed, so a close with unread data is not missed and nothing spins |
| 2026-10-11 | A Linux unit list for PuTTY (`port/putty/linux/units.txt`): `replace` or `drop` per unit, everything else as `Putty.cbproj` lists it | The agent client and GSSAPI are Windows services with Linux counterparts; the named-pipe client, its ACLs and Pageant's pipe name serve only Pageant. A rule naming a unit upstream no longer lists is an error |
| 2026-10-11 | The Win32-on-POSIX headers are C as well as C++, with C linkage | PuTTY is C and includes `<windows.h>` as core does |
| 2026-10-11 | The WinSCP pin tracks `master`, not a release tag (user's decision; supersedes P0-02's "release tag, not `master`") | Upstream is porting itself to 64-bit on `master` and fixing the same bugs this port was finding -- five of the six reported were already fixed there. Following it inherits those fixes instead of duplicating them. A bump re-runs the transform, the patch series and closure-site discovery, all of which fail loudly where upstream moved |
| 2026-10-11 | Static initialisation follows upstream's project order: executables link core's objects directly, in `ScpCore.cbproj` order, reversed for Windows | Core's globals depend on each other across units (`EscapedBom` in HierarchicalStorage.cpp is built from `Bom` in Common.cpp). C++Builder runs constructors in link order; mingw-w64 runs them in reverse link order and Linux in link order, and archive extraction gives no order at all |
| 2026-10-11 | Library units and defines are read from upstream's make files too (neon, libs3, expat, the MFC subset) | `upstream_units.py` reads `$(OBJ_D)\x.$(O)` objects and `DEFINES`/`DEFS` plus `Makefile.inc`'s unconditional `CFLAG_OPENSSL`. Two defines meant for C++Builder's headers, `_ASSERTE` and `NOUSER`, are undefined again for mingw-w64's |
| 2026-10-11 | Third-party libraries stay on the version WinSCP vendors while that is straightforward (user's policy) | Today that holds for all of them (OpenSSL is the same 3.3.7 release plus WinSCP's reviewed changes). If keeping one in step gets complicated, the port moves to a newer upstream release instead, carries the patches it needs on a locally maintained branch, and offers that version to WinSCP when the two merge |
| 2026-10-11 | Bugs found in upstream code are reported to the project that owns them, against its current `master` | Six were found. Five are already fixed on WinSCP's `master`, which is being ported to 64-bit itself (`WinSCP.cbproj` gained a `Win64` platform after 6.5.8): libs3's `ssize_t`, FileZilla's `GWLP_USERDATA`, the AES-NI constants, PuTTY's option defaults (back to PuTTY's own `conf.h`) and neon's `ne_init_ssl_session` declaration. The sixth, `get_remote_username` reading `CONF_username` with `conf_get_str`, is winscp/winscp#68. None is in PuTTY, OpenSSL, neon or libs3 proper |
| 2026-10-11 | Every non-Debug build defines `NDEBUG`, including one with no build type | Upstream has only Debug (`_DEBUG`) and Release (`NDEBUG`). With neither, PuTTY's standard `assert` stays live, which upstream never builds -- and core depends on that: `get_remote_username` reads `CONF_username` with `conf_get_str`, which asserts on its type |
| 2026-10-11 | Session tests run against a throwaway unprivileged `sshd` on loopback | Real protocol code, no network and no credentials: fresh host and client keys per run, the client key the only one authorized, and sshd can only log in the user running it. Skips where no sshd is installed |
| 2026-10-11 | linscp names itself to servers but keeps WinSCP's stored settings (user's decision) | `AppNameString` is "linscp" and the SSH banner `linscp-release-<version>` -- the HTTP User-Agent, S3 role session, FTP CSID and PuTTY appname follow. The registry keys stay `Software\Martin Prikryl\WinSCP 2`, so on Windows existing sites carry over |
| 2026-10-11 | The VERSIONINFO resource is generated from `WinSCP.cbproj`'s `VerInfo_*` settings (`gen_versioninfo.py`) | Core reads its own version from the running module's resource, which C++Builder writes from those settings; generated, it follows the pin. POSIX has no resources: P1-07b's |
| 2026-10-11 | The headless frontend compiles upstream's `windows/ProgParams.cpp` as is and copies `UniqTempDir` from `GUITools.cpp` | `TProgramParams` needs nothing beyond core; `GUITools.cpp` is otherwise GUI. Startup follows `wWinMain`'s order: the application log exists before `CoreInitialize` |
| 2026-10-11 | OpenSSL is built from the pinned release WinSCP vendors (3.3.7 then, 3.5.9 since the move to `master`; `3rd-party/openssl`) with OpenSSL's own `Configure`, not from `libs/openssl` | WinSCP's copy is trimmed for 32-bit Windows: x86 assembly without its C fallbacks (`bn_asm.c`, `des_enc.c`, ...), no Unix sources, a 32-bit `configuration.h`. It cannot build for x86-64 or Linux. Disabled features mirror WinSCP's `OPENSSL_NO_*` set |
| 2026-10-11 | WinSCP's changes to OpenSSL are adopted or skipped by a reviewed list, `port/openssl/winscp-changes.txt` | 17 files differ. One is a security backport (CVE-2026-45447, from 3.4.6), adopted; the rest are C++Builder workarounds, skipped. `openssl_tree.py --check` (ctest `openssl_tree`) fails on a difference the list does not cover, so a WinSCP backport cannot be missed on an upgrade |
| 2026-10-11 | neon and libs3 are compiled from the generated tree, from the unit lists in their make files | The transform's rules leave them byte-identical; the tree gives them the patch series, which libs3 already needs. `upstream_units.py` reads Borland make files as well as projects |
| 2026-10-11 | libs3 is compiled as C++ | Its Makefile passes bcc32 `-P`. WinSCP's additions depend on it (struct names without `struct`) and core declares libs3's functions with C++ linkage |
| 2026-10-11 | neon drops `NOUSER` and tolerates one implicit declaration | mingw-w64's `iphlpapi.h` reaches `ole2.h`, which needs `MSG`. `ne_socket.c` calls WinSCP's `ne_init_ssl_session` undeclared, as bcc32's C89 allows; pointer arguments, result unused, so the implicit signature is ABI-correct |
| 2026-10-11 | WinSCP's MFC subset is built as is for Windows, as `libuafxcw.a` | `FileZillaIntf.cpp` links it by name with `#pragma comment(lib, "uafxcw.lib")`, meaning the subset `borland.mak` builds under that name; same name, on the link path, resolves the request to exactly that library. Replacing it is a Linux concern (P1-11) |
| 2026-10-11 | MFC and FileZilla get a forced prelude (`vcl_win32_prelude.h`) with `NOMINMAX` and `<algorithm>` pre-included | WinSCP's MFC `stdafx.h` defines `min`/`max` as macros before a later `<algorithm>`; libstdc++, unlike MSVC's library, does not survive that. Included first, the later include is a no-op |
| 2026-10-11 | `_AFX_NO_DAO_SUPPORT` for the MFC subset | DAO is 32-bit only; Microsoft's MFC turns it off for every non-x86 target, in the `afxv_cpu.h` WinSCP does not vendor |
| 2026-10-11 | Dead MFC functions join upstream's `#ifndef WINSCP` exclusions (`mfc-winscp-dead-references.patch`) | `CObject::IsKindOf` and timecore's `CArchive` operators call code upstream never builds; nothing uses them. C++Builder's linker tolerated it, lld does not |
| 2026-10-11 | Generated files are declared byproducts of the transform, which writes only what changed | Ninja otherwise stats them before the transform runs: a new patch reached `build/gen` but not the objects built from it. Unchanged files keep their mtimes, so a patch rebuilds only what it touches |
| 2026-10-11 | neon on POSIX: platform facts from neon's own `configure`, feature choices from WinSCP's Win32 `config.h` (`port/neon/config-posix.h`, force-included) | WinSCP's `src/config.h` is empty off Win32. Taking configure's feature answers too would make Linux differ from Windows in what it offers servers and shows users: SHA-512 digest auth (`HAVE_OPENSSL11`), IANA cipher names (`HAVE_SSL_CIPHER_STANDARD_NAME`). The sizes it records are checked by `_Static_assert` |
| 2026-10-11 | WinSCP's Windows-only additions to neon are compiled on Windows only (`neon-winscp-additions-windows-only.patch`) | An unused `<windows.h>`, `ioctlsocket(FIONBIO)` where upstream's `fcntl` code sits beside it, and `ne_sock_sspi_init` calling SSPI unguarded. On POSIX neon uses BSD sockets directly, not the Winsock emulation |
| 2026-10-11 | libs3 on POSIX is compiled over the POSIX layer (`-include windows.h`) | WinSCP's additions call `GetSystemTime` and `strnicmp`, which on Windows arrive through OpenSSL's headers including `<windows.h>`. Emulating them keeps libs3 unpatched |
| 2026-10-11 | FileZilla keeps its Win32 window-message model on POSIX, over an emulated user32 subset (`win32_window_posix.cpp`), instead of a rewrite onto epoll (changes P1-12's approach) | FileZilla's socket layer is built around one hidden helper window per thread: `WSAAsyncSelect`, layer notifications and a timer all post to it, and its worker runs a `GetMessage` loop. About twenty functions with Win32's rules -- verified against user32 under Wine by one test -- leave FileZilla unmodified, where a rewrite would be the least mechanical change in P1 and would diverge from upstream with every merge |
| 2026-10-11 | WinSCP's MFC subset is built as is on Linux (closes P1-11 without a replacement) | It is `CString`/`CFile`/`CTime` over the shim's `UnicodeString` already; given the POSIX layer it compiles unchanged |
| 2026-10-11 | Direct Winsock calls from C++ reach the `ws2_32` module's functions through overloads on `SOCKET` | FileZilla calls `connect`, `send`, `recv`, `accept`, ... directly with `SOCKET`s and `int` lengths. Overloads on the `SOCKET` argument win over libc's, so those calls get Winsock's semantics; a `void *` buffer, which would match libc's better, is a deleted overload, so no call quietly takes BSD semantics. Functions with libc's exact signature (`socket`, `getaddrinfo`, ...) stay libc's: FileZilla tests only their results |
| 2026-10-11 | `WSAGetLastError` is `GetLastError` on POSIX | As on Windows. FileZilla sets errors with `WSASetLastError` and reads them with `GetLastError`. The previous separate Winsock error, falling back to `errno`, predated the `ws2_32` module |
| 2026-10-11 | The `*_build` tests share a ctest resource lock | Each builds this tree; two at once contend for Ninja's lock and fail spuriously under `ctest -j` (seen as flakes of `putty_overrides_build`, `core_init_build`, `sftp_session_build`) |
| 2026-10-11 | Core's `long` is respelled `LONG` and `unsigned long` `ULONG` by a transform rule (`lp64_long`) | Upstream is written for Windows, where long is 32 bits (Win64 included, so WinSCP's own 64-bit port does not meet this), and depends on it: TSFTPPacket consumed `sizeof(unsigned long)` bytes per SFTP uint32 -- 8 on LP64 -- and sign-extends times through `signed long`. LONG and ULONG are `<windows.h>`'s typedefs of exactly those types, so C++Builder and mingw see no change; the POSIX layer makes them 32 bits, which is upstream's meaning everywhere at once instead of a long tail of per-site patches. Comments, literals, `long long` and `long double` are untouched; core files only. Lines where core implements another library's interface keep `long` (`port/tools/lp64_long_keep.txt`: PuTTY's `schedule_timer`); a missing entry is a conflicting-declaration error, not a silent one. No override is hidden (checked with `-Woverloaded-virtual`) |
| 2026-10-11 | `%l` in the shim's formatter and in `swscanf` is 32 bits on POSIX | Windows' meaning, and what core's `LONG`/`ULONG` arguments now are. On x86-64 an `int` and a `long` take one argument slot each, so reading 32 bits is also safe for a caller passing a real `long` |
| 2026-10-11 | Nothing linked may call the C library's wide functions (ctest `wide_imports`, at link level) | ADR 0001's source check covers `port/`; upstream reached glibc's 4-byte functions three ways: PuTTY utilities that include no platform header (`dupwcs.c`), `std::wstring` in FileZilla's tool interface (`char_traits<wchar_t>` calls `wcslen`), and `swscanf` (as glibc's `__isoc23_swscanf`). PuTTY now has the wide redirects forced in (`win32_on_posix_wide.h`), the interface takes `UnicodeString` (`filezilla-tools-unicodestring.patch`), and `swscanf` has a POSIX implementation |
| 2026-10-11 | The scripting client (`linscp-cli`) is a runner of its own over core's `TManagementScript`, following `TConsoleRunner` and winscp.com | Upstream's runner (windows/ConsoleRunner.cpp) cannot be compiled without the VCL forms: its header is the GUI's `WinInterface.h` (117 VCL types, `WinConfiguration.h`, Delphi components). The script and its output are core's either way; the runner carries how queries (accelerators, batch answers, timeouts), prompts and input meet the console, and the console has winscp.com's semantics (UTF-8, from-beginning prints held on pipes, piped input echoed, a choice from a pipe breaks). It names itself linscp (`linscp> `), per the naming decision |
| 2026-10-11 | Every allocation, and every thrown object, starts zeroed (global `operator new`; `--wrap=__cxa_allocate_exception`) | Delphi's InitInstance zeroes an instance before its constructor, and upstream relies on it: `TSessionData`'s constructor reaches `Modify()`, which writes through an unset `FUnsafeSettings`; `ExtException`'s reads `FMoreMessages`. A class-scoped `operator new` on `TObject` cannot be reached through core's private inheritance from `TStringList`, and thrown objects are built in the runtime's exception storage, so both paths are zeroed instead. A superset no correct code can tell apart; one `calloc` per allocation |
| 2026-10-11 | On POSIX, `CmdLine` quotes an argument with a space, tab or quote, doubling its quotes | Core's `CutToken` reads `""` within quotes as a quote, the convention winscp.com's users write by hand. Embedded quotes were passed through, which cut `-hostkey="ssh-ed25519 255 ..."` at its first space |
| 2026-10-11 | On POSIX the configuration is WinSCP's INI, named `WinSCP.ini`, in `$XDG_CONFIG_HOME/linscp/` -- unless `/ini` says otherwise or a `WinSCP.ini` sits beside the executable (closes P1-06d) | The counterpart of keeping WinSCP's registry key on Windows: the same format under the same name, so a `WinSCP.ini` exported from Windows can be dropped in. One file for every linscp program, as WinSCP.exe and winscp.com share one configuration; upstream's automatic name (`<executable>.ini`) would give each program its own. The executable's directory wins, as upstream's portable mode has it |
| 2026-10-11 | Storage key paths use backslashes on every platform (`core-storage-key-backslashes.patch`) | `THierarchicalStorage` joined keys through the RTL's `IncludeTrailingBackslash`, which on POSIX adds the platform's path delimiter. Keys are not file paths; the INI file's sections are backslash-separated. Unchanged on Windows |
| 2026-10-11 | A field-backed property of class type reads as the field itself (a reference) | Borland makes such a property the field: core changes members through it (`Rights.Number = TRights::rfDefault` in `TCopyParamType::Default`). Returning a copy lost every such change silently -- SCP sent mode 0000. Scalars stay by value; a class field of another type than the property's fails to compile rather than copy |
| 2026-10-11 | Protocol tests use loopback servers: OpenSSH's sshd, rclone for WebDAV, and the suite's own where none is installed | Real servers where the machine has them, so the client is tested against implementations it did not write; a protocol whose server is missing is skipped, not failed |
| 2026-10-11 | `SubString` takes an index below 1 as 1 and keeps the count (C++Builder's), not Delphi `Copy`'s dropping of the characters before 1 | Core's one use of it, `TS3FileSystem::ParsePath`'s `SubString(0, P - 1)`, works in WinSCP's S3 transfers only that way; the shim's earlier reading cut every bucket name by one character |
| 2026-10-11 | Golden runs use servers that exist on both platforms -- rclone (SFTP, WebDAV), the suite's FTP and S3 servers -- with fixed data, fixed permissions, password logins and `*` for host keys and certificates | Real WinSCP captures on Windows and linscp replays on Linux; any difference has to be the client's. SCP is left out: Windows has no SCP server without installing OpenSSH. Normalising removes only the start time and the local working directory |
| 2026-10-11 | Deliberate departures from WinSCP's output are listed per case in `golden/known-differences.txt` | Printed, not failed; cases are kept small (rmdir has its own) so an entry hides nothing else |
| 2026-10-12 | The golden corpus is captured on a native Windows VM (192.168.0.33, reached over SSH; tools kept in `C:\temp`) and pins its servers' versions (`expected/SERVERS.txt`, `CLIENT.txt`); `ctest golden` skips, saying why, unless the same rclone is at `LINSCP_RCLONE` (`port/tools/fetch_rclone.py`) | A newer rclone answers WebDAV synchronization differently (seen: 1.60 against 1.75.2), which would read as a client difference |
| 2026-10-12 | P1-19 closes with SCP outside the golden comparison | No SCP server runs on both platforms (Windows' OpenSSH has no POSIX shell, which SCP needs); SCP is tested against OpenSSH in ctest `protocols` |
| 2026-10-12 | ctest `protocols` also runs against servers the suite did not write -- pyftpdlib (FTP, FTPES), moto (S3, checked through boto3) -- and through proxies that must see the traffic and require credentials: microsocks (SOCKS5: SFTP, FTP with its data connections) and tinyproxy (HTTP CONNECT: SFTP, WebDAV) | The suite's own FTP and S3 servers test against its own reading of the protocols; independent implementations do not. Proxy support (PuTTY's, FileZilla's, neon's) was untested. The workstation's packages: python3-pyftpdlib, python3-moto, python3-flask-cors, microsocks, tinyproxy |
| 2026-10-12 | `linscp-cli` implements winscp.com's console protocol (`/consoleinstance`, `ExternalConsole.cpp`) on Windows, over upstream's own `console/Console.h` structure | WinSCP's .NET assembly and winscp.com both drive WinSCP.exe that way; with it, the stock assembly and winscp.com drive linscp unchanged. The CLI's console is now an interface (`CliConsole.h`) with the stdio and external implementations |
| 2026-10-12 | Waiting on upstream never blocks the port: changes linscp needs are carried in its own patches and offered upstream in parallel (ADR 0006, amended). Merging upstream remains the intention, so carried changes stay mergeable | The user's direction. Waiting on review would stall P1 and P2; but the port is still meant to go upstream one day |
| 2026-10-12 | Work is committed to `main` directly; pull requests only to put a change through CI, branches deleted once merged (`qt` merged into `main` and retired) | The user's direction; CI runs on pull requests only |
| 2026-10-12 | Off Windows, winscp.com's console protocol runs over two inherited anonymous pipes (`/consolepipes`), the structure unchanged; proposed upstream for the assembly (ADR 0010) | Over a Unix socket, which needs `netstandard2.1` and a file system name another local user could connect to first: pipes have no name, exist in `netstandard2.0` and `net40`, and their end tells each side the other has gone, as the job object does |
| 2026-10-12 | P1-20 is met on Windows only; Linux becomes P1-23 | The assembly's transport is Windows-only: named events and named file mappings, which .NET does not provide elsewhere, so "unchanged on Linux" cannot hold |
| 2026-10-12 | CI runs on pull requests (and by hand), not on every push; in `debian:trixie`, with the tests as an unprivileged user | The repository is public, so Actions are free, but a run takes ~12 minutes (Wine) and pushes to a branch under review need not each run. trixie is the development toolchain (clang 19, mingw-w64, Wine 10); the fixtures start sshd and the other servers unprivileged, as locally. Wine uses its `null` graphics driver, since FileZilla's sockets and timers need windows and CI has no display |
| 2026-10-10 | `Format` keeps malformed input visible | Delphi raises on a missing argument. Core routes user- and server-supplied text through these into logs, so an unknown specifier is echoed and a missing argument renders as `<missing>` rather than throwing from a log path |

## Correction: census figures before 2026-10-11 used the wrong configuration

Until `upstream_defines.py`, every compile of core here used **none** of the
defines upstream builds it with -- not `WINSCP`, not `UNICODE`, not `_DEBUG`.
So the census measured a configuration upstream never builds. Two effects:

- Code upstream compiles was hidden. `_DEBUG` alone exposed ten closure
  comparisons in `Queue.cpp`, which had been reported clean; they needed
  Borland closure *equality*, which did not exist.
- Workarounds were built for problems that were not real: `port/compat/help.h`
  stood in for a header that `WINSCP` compiles out.

It also surfaced a shim bug that only `_DEBUG` reaches: the POSIX `TEXT()` was
one-level, so `TEXT(__FILE__)` pasted the token into `L__FILE__`.

The corrected census is **159 / 303**. Earlier figures are not comparable with
it; the trend they showed was real, the absolute numbers were not.

## Correction: the earlier "5 errors left" figure was wrong

Recorded because the mistake is instructive, not just to log it.

`compile_census.py` filtered clang diagnostics on `': error:'`, which does not
match `': fatal error:'`. A missing header emits only a *fatal* error and aborts
the parse, so a translation unit that never got past its first include was
reported as having **zero** problems. The progression logged earlier for
`Common.cpp` (109 -> 83 -> 70 -> 30 -> 13 -> 5) was real for the portion that
parsed, but it was a floor, not a total: the parse was stopping at
`#include <Classes.hpp>`.

Two fixes. The census now counts fatal errors and prints `ABORTED EARLY` with the
cause, so an aborted unit can never again look like a clean one. And
`port/shim` now stubs the unqualified legacy RTL header names (`Classes.hpp`,
`SysUtils.hpp`, `Contnrs.hpp`, ...) alongside the `System.*` spellings, since
core uses both.

The honest figure for `Common.cpp` is **417 errors across 29 missing types**, and
the ratchet now covers nine translation units rather than one, precisely so a
single unit cannot give a falsely rosy picture again.

## Known divergences from Delphi

Recorded deliberately, so they are findable when something behaves oddly.

| Area | Divergence | Why |
|---|---|---|
| Connect timeout (PuTTY's `windows/network.c`) | `-timeout` does not bound connecting: an unreachable host fails after the kernel's SYN retries, 136 s on Linux, where WinSCP 6.5.7 on Windows 11 takes 24 s | Upstream's bug on both platforms: the timeout is set with `SO_RCVTIMEO`, which no `connect()` honours. To be fixed upstream (a non-blocking connect bounded by the timeout), not papered over in the emulation |
| `UnicodeString::LowerCase` / `CompareIC`, `AnsiLowerCase` / `AnsiUpperCase` | ASCII-only case folding | Delphi's is locale-aware. Core uses these on protocol tokens and switch names. Must not be relied on for user-facing text |
| `FindFirst` pattern matching | Case-sensitive on Linux, case-insensitive on Windows | Each matches its host filesystem. A pattern that matches on Windows may not on Linux |
| `faHidden` on Linux | Set from a leading dot in the name | POSIX has no hidden attribute; the dot convention is what users expect |
| `TFormatSettings` defaults | ISO-8601 rather than system locale | Deterministic across machines. Anything user-facing should set them explicitly |
| Null arguments to the Win32 time functions | Rejected on POSIX; real Win32 dereferences and faults | The hardening is deliberate, so the null-argument assertions run only on POSIX |
| Winsock error codes on POSIX | Keep their Winsock numeric values rather than mapping to `errno` | Core and the FileZilla layer compare them by name and store them in their own state |
| `TRegistry` on Linux | Every key reports absent; nothing can be read or written | No registry exists. The INI backend is the correct one there, and failing visibly beats returning defaults for settings the user had set |
| INI share modes | Windows share modes are ignored on POSIX rather than approximated | Advisory locking would change behaviour rather than match it |
| `SetThreadPriority` on POSIX | Recorded and reported back by `GetThreadPriority`, but no scheduling effect | Core only lowers background queue threads; running them at normal priority is harmless |
| `ExitThread` / `EndThread` on POSIX | Unwinds to the entry point by exception, so destructors run; Win32 skips them | A `catch (...)` between the call and the entry point would intercept it. Core calls it only as the last statement of a thread entry point |
| Named events on POSIX | `CreateEvent` with a name fails with `ERROR_NOT_SUPPORTED` | Would need to be cross-process. Core never names one; refusing beats quietly creating a private event |
| `StripHotkey` in East Asian locales | The whole-`(&X)` suffix removal Delphi does under `SysLocale.FarEast` is not reproduced | Only the `&` is removed. Affects only CJK system locales |
| `ParamStr(n)` splitting on Windows | Follows Delphi's `GetParamStr` rules as documented in the shim -- quotes toggle and are dropped, no backslash escapes, empty `""` skipped -- but has not been compared against a Delphi binary | Wine cannot check it: `CommandLineToArgvW` implements the MSVC rules, which differ deliberately. Core itself only uses `ParamStr(0)` |
| `TPath::IsDriveRooted` on POSIX | Always false | As in Delphi's POSIX RTL. Core uses it only to locate Windows' UAC VirtualStore |
| Calling a closure through a type it was cast to | Stops the program with a message | Borland reinterprets the (code, data) pair, so the call is undefined behaviour there. Core only casts to launder a closure and back, which round-trips exactly |
| `TFile::ReadAllText` without a BOM | Decodes UTF-8 on both platforms | Delphi uses `TEncoding.Default`, ANSI on Windows. Core reads only `authorized_keys` this way and writes it back as UTF-8, so ANSI would double-encode non-ASCII comments. ASCII files are identical either way |
| `FileSetAttr` on POSIX | Only `faReadOnly` acts, on the write permission bits; archive, hidden and system are ignored | They have no POSIX counterpart. Read-only mirrors how `FileGetAttr` reports it |
| `FindData.ftCreationTime` on POSIX | The modification time | POSIX has no creation time |
| `Variant` | Only one-dimensional integer variant arrays | Core's only use. Anything wider raises `EVariantError` so a new use is noticed |
| `ParseShortEngMonthName("January")` | Returns 0, "not found" | Upstream reads past `ShortMonthNames` into `LongMonthNames[0]` and returns 13, an invalid month. Every other input agrees |
| `TFormatSettings::Create(locale)` | Every locale yields the shim's deterministic defaults, with English month and day names | Core asks by LCID 1033 for protocol parsing, which this matches; user-facing formatting should set fields explicitly |
| `TEncoding::Default` | UTF-8 on both platforms | Delphi's is ANSI on Windows |
| `FileGetSymLinkTarget` | Resolves the whole chain to an absolute path | Delphi returns the immediate target. Core only reads the target's attributes |
| `ExtractShortPathName` on POSIX | The name unchanged | No 8.3 names exist |
| `ExceptAddr` / `ExceptionErrorMessage` | No addresses are recorded: `ExceptAddr` is null and the module is the executable | The class name and message, the useful part, are exact |
| `FormatFloat` | Digits, grouping, decimals, literals and sections only; scientific notation raises `EConvertError` | Core's one use is `"#,##0"` |
| Element `Text` with mixed content | Concatenates the element's text children | Delphi raises when an element holds more than a single text node. Leaf elements, which is what core reads, agree |
| `RawByteString` to another code page | Bytes pass through | C++Builder's `RawByteString` carries a run-time code page and would transcode out of it; this one has none |
| `ISO8601ToDate` without a zone designator | Taken as UTC | Delphi's handling of a zoneless time is not established here; every value core parses (S3) carries `Z` |
| `TStrings::Text` on POSIX | CRLF, not LF | See the decisions table |
| Headless frontend: stack traces | None are recorded, so none are appended to error reports | Upstream records them through JCL's exception hooks, a Windows-only C++Builder facility |
| Headless frontend: sleep | `SystemRequired` holds off sleep on Windows (`SetThreadExecutionState`) and does nothing on POSIX | POSIX has no equivalent without a session bus (logind inhibitors); upstream also runs it from a helper thread on Windows 11 |
| Headless frontend: clipboard | Pasting a password finds nothing; copying raises | No clipboard without a GUI; the copy is reachable only from a GUI prompt's button |
| String order on POSIX (`lstrcmp`, `lstrcmpi`, `StrCmpLogicalW`) | Ordinal comparison of ASCII-folded UTF-16, digits by value for the logical form, lower case first on ties | Windows uses linguistic collation for the user's locale. Sort order of file lists can differ for non-ASCII names and punctuation |
| Time zone names on POSIX | `StandardName`/`DaylightName` are the zone database's abbreviations (`CET`, `CEST`) | Windows gives long names ("Central Europe Standard Time"). Core only logs them |
| ANSI code page and locale on POSIX | Reported as 28591 (Latin-1) and en-US (0x0409) | The shim takes ACP to be Latin-1; there is no Windows locale to report |
| `FindMimeFromData` on POSIX | By file name only, from shared-mime-info's globs; never by content | Core's one call passes `FMFD_URLASFILENAME` |
| `SHFileOperation` on POSIX | `FO_DELETE` only; `FOF_ALLOWUNDO` uses the home trash of the freedesktop.org specification, so a file on another file system fails rather than being copied | Core only deletes, to the recycle bin or not |
| `EnumProcessModules` on POSIX | Fails | Only used to log the module list |
| `WindowsValidateCertificate` on POSIX | The chain is verified with OpenSSL against the system trust store (OpenSSL's default locations, `SSL_CERT_FILE`/`SSL_CERT_DIR`, the distributions' bundles); missing intermediates are not fetched and revocation is not checked | Windows' chain engine does both online. A server that omits its intermediate is refused here where Windows might accept it |
| `VirtualQuery` on POSIX | Every mapping counts as committed | Linux overcommits; core only logs the totals, which overstate memory in use |
| `SHGetFileInfo` on POSIX | Type names only; no icons (`iIcon` 0) | There is no system image list without a GUI |
| `TSynchronizeChecklistFileOperation` | Windows only (`core-shell-file-operation-windows-only.patch`) | It drives Explorer's `IFileOperation`, and only the Windows GUI uses it |
| File share modes on POSIX | Accepted and ignored; a second opener is never refused with `ERROR_SHARING_VIOLATION` | POSIX has no mandatory locking. Core retries on that error (HierarchicalStorage.cpp), so never seeing it only removes a wait |
| `FILE_FLAG_DELETE_ON_CLOSE` on POSIX | The name is unlinked at open, not at the last close | Either way nothing is left behind; core uses it only to probe that a directory is writable |
| File creation time on POSIX | The birth time where the file system records one (`statx`), else the modification time; it cannot be set | POSIX has no settable creation time |
| SSH agent on Linux | `$SSH_AUTH_SOCK`; Pageant's window-message protocol has no counterpart | Every Linux agent (ssh-agent, gpg-agent, KeePassXC, ...) serves that socket |
| GSSAPI on Linux | Library slot 0 is the system's Kerberos library (MIT `libgssapi_krb5.so.2`, else Heimdal); slot 1, SSPI, is never available | Core addresses three fixed slots and asserts there are three; their settings keywords are unchanged |
| Narrow (`A`) Win32 file names on POSIX | Taken as UTF-8, the file system's convention | Windows takes them in the ANSI code page. PuTTY passes paths through unchanged, so UTF-8 in, UTF-8 out |
| Proxy commands on POSIX | Run with `/bin/sh -c` | Windows hands the command line to `CreateProcess` |
| WebDAV authentication on Linux | No SSPI, so no Windows-integrated NTLM/Negotiate; `ne_sock_sspi_init` reports it unavailable, which core already handles. neon's GSSAPI is not enabled | SSPI is Windows'. GSSAPI for WebDAV would be a feature beyond upstream's |
| Agent choice (`AuthAgent`) on Linux | Stored and restored, but Pageant and OpenSSH both mean `$SSH_AUTH_SOCK` | PuTTY's Unix Pageant and OpenSSH's agent listen on the same socket |
| Windows on POSIX | Message targets only: a procedure, an owning thread, user data. No desktop, painting or `SendMessage`; creation and destruction send `WM_NCCREATE`/`WM_CREATE` and `WM_DESTROY`/`WM_NCDESTROY` only | Nothing headless needs more. `SendMessage` is undeclared, so a use fails to compile |
| `IsBadWritePtr` on POSIX | Reports only a null block as bad | Windows probes the pages; POSIX has no probe that is not itself a hazard. FileZilla uses it as a sanity check on its own requests |
| `MessageBox` on POSIX | Caption and text go to standard error; the answer is `IDOK` | No desktop. FileZilla raises one only for "cannot happen" TLS session errors |
| `swscanf` on POSIX | Numeric conversions only; a format with `%s`, `%c` or `%[` fails (EOF) | Implemented by narrowing to `vsscanf`. Upstream parses only numbers with it (SCP's time line, FileZilla's MLSD dates); text conversions would write narrow text into wide buffers |
| CLI input timer on a terminal | POSIX: counts from the start of the line; Windows: runs until the first key | winscp.com restarts it on every key press. In canonical mode POSIX sees nothing until Enter. Affects only batch-mode prompts, which time out after 10 s |
| CLI terminal title | Not set on POSIX | Escape sequences would land in logs of a redirected terminal; Windows sets the console title as winscp.com does |
| `SuspendThread` on POSIX | Not declared | A running pthread cannot be suspended. A missing declaration fails at compile time; a stub would fail silently. `CREATE_SUSPENDED` + `ResumeThread` is supported |

## Open questions

| # | Question | Blocks | Owner |
|---|---|---|---|
| ~~Q-1~~ | ~~P3 scope: faithful parity, or a deliberately narrower client?~~ — **resolved 2026-10-14**: every dialog, and the Commander commands users reach daily (P3-14) before the gate; the rest is backlog (P3-15) | — | — |
| Q-2 | Access to a C++Builder 11 licence? (A native Windows host is available: the user runs things there on request, 2026-10-11) | P0-04, R1, R3 | — |
| ~~Q-3~~ | ~~Internal editor, or delegate to the system editor?~~ — **resolved 2026-10-13**: both, as WinSCP; the internal editor first (ADR 0015) | — | — |
| Q-4 | Ship 32-bit Windows, or x86-64 and arm64 only? | P0-10, packaging | — |
| Q-5 | Approach the maintainer early for feedback, or after P1 lands? | social strategy, not architecture | — |
| Q-6 | Far-NetBox licence: README says GPLv3, `LICENSE.txt` is the GPLv2 text | verbatim reuse of its shim (reference use is unaffected) | — |
| Q-7 | Build the fork-branch generator now, or when there is something worth proposing upstream? | nothing today; recommend deferring until P1 lands | — |
| ~~Q-8~~ | ~~Confirm ADR 0001~~ — **resolved 2026-10-10**: accepted | — | — |
| ~~Q-9~~ | ~~Stay on WinSCP's OpenSSL 3.3, or move to a supported LTS?~~ — **resolved 2026-10-11** by the third-party library policy (decisions table): stay while it is straightforward | — | — |
| ~~Q-10~~ | ~~Bump the WinSCP pin to upstream's first 64-bit release?~~ — **resolved 2026-10-11**: pinned to `master` instead (decisions table) | — | — |
| Q-11 | How should the assembly check the executable's version off Windows, where an ELF file has no version resource? The test sets `DisableVersionCheck`; the patch could skip the check off Windows or carry the version in INIT | nothing (linscp's assembly can decide alone) | — |
| Q-12 | Rare crashes and failures under the full suite's parallel load (`ctest -j8`), not reproduced alone or in 24 loaded runs of the session tests: `qt_session_pt_BR` aborted in glibc (`pthread_mutex_lock ... e != ESRCH \|\| !robust`: the critical section's recursive mutex locked after it was destroyed or freed, likely a queue thread at teardown); `qt_session` once; `qt_gui_keep` twice at "it starts". Cause unknown | test reliability; possibly a teardown use-after-free | — |
| ~~Q-13~~ | ~~The Explorer interface: offer it, or not?~~ — **resolved 2026-10-15**: offered (P3-21) | — | — |

## Verified facts (re-check if the environment changes)

| Fact | Evidence | Date |
|---|---|---|
| A thread timer's `WM_TIMER` is not retrieved with the `(HWND)-1` filter | Under Wine, `PeekMessage`/`GetMessage` with `(HWND)-1` never return it once due, while a `NULL` filter does (ctest `messages`). Microsoft documents the filter as selecting posted thread messages; the emulation matches Wine | 2026-10-11 |
| Linux polls a TCP socket that was never connected as writable and hung up | `POLLOUT|POLLHUP` before `connect`: the Winsock emulation reported `FD_WRITE` and a spurious `FD_CLOSE` for a socket selected before connecting -- the order FileZilla uses -- until it tracked that state (ctest `messages`, repeated runs) | 2026-10-11 |
| PuTTY's AES-NI SDCTR is wrong when built by clang or GCC | WinSCP's `_MM_SETR_EPI8` brace-initialises `__m128i`, which is a byte union under MSVC but a two-lane vector elsewhere: the byte-reversal mask loses all but two values. NIST SP 800-38A F.5.1 fails, software AES passes; sessions fail with "Incoming packet was garbled on decryption" | 2026-10-11 |
| `CopyToLocal` needs each name's `TRemoteFile` in `Objects[]` | A bare name list crashes in `TRemoteFile::GetLinkedFile`; WinSCP's callers always pass the listed files | 2026-10-11 |
| WinSCP's OpenSSL differs from the 3.3.7 release in 17 files, one of them functionally (a CVE-2026-45447 backport in `pk7_smime.c`) | `openssl_tree.py --check`, CRLF ignored; WinSCP commit bb46bb15d | 2026-10-11 |
| WinSCP's OpenSSL lacks the C fallbacks for its x86 assembly and every Unix source | `bn_asm.c`, `des_enc.c`, `bf_enc.c`, `c_enc.c`, `threads_pthread.c`, `rand_unix.c` absent from `libs/openssl` | 2026-10-11 |
| Upstream libs3 returns `int` where neon's body provider returns `ssize_t` | `neon_read_func` in `libs3/src/request.c`; fixed by `libs3-64bit-body-provider.patch` | 2026-10-11 |
| WinSCP `master` is being ported to 64-bit | `WinSCP.cbproj` has a `Win64` platform; libs are built with bcc64 (`libs/Makefile.inc`); `StartThread` returns `THandle`; AES-NI uses `_mm_setr_epi8` under `_WIN64` | 2026-10-11 |
| The POSIX path parsing, string order, environment expansion, module, process and MIME functions match Win32 on everything the tests check | The same tests pass against the real API under Wine; that is how `PathSkipRoot` was found not to skip a bare leading backslash | 2026-10-11 |
| Core passes `unsigned long *` where Win32 declares `DWORD *`/`PULONG` (Terminal.cpp, FileInfo.cpp, HierarchicalStorage.cpp, SessionInfo.cpp) | 32 bits on Windows (LLP64), 64 on Linux (LP64); the POSIX layer adds C++ overloads for those calls, and one read through `unsigned long *` that over-read became `core-lp64-knock-address.patch` | 2026-10-11 |
| A one-line `try { } __finally { }` in a macro (Configuration.cpp's `BLOCK`) escaped the transform, and compiled on Windows as clang's native SEH | The Linux census reported "SEH '__try' is not supported"; the transform now rewrites the one-line form too | 2026-10-11 |
| The POSIX time zone rules match Windows' for Central Europe, US Eastern, AUS Eastern, UTC and Japan | `TestTimeZones`, against the rules Windows' registry holds for those zones | 2026-10-11 |
| The POSIX file functions match Win32 on everything `TestFiles` checks | The same test passes against the real API under Wine and against the POSIX layer | 2026-10-11 |
| `std::wstring` is unusable on POSIX under `-fshort-wchar` | Its traits call glibc's 4-byte `wcslen`: `wstring(L"/tmp/") + L"x"` came out as "/tmp/" | 2026-10-11 |
| mingw-w64 runs global constructors in reverse link order, Linux in link order | Three one-line units linked a, b, c print "c b a" under Wine and "a b c" natively | 2026-10-11 |
| WinSCP `master`'s OpenSSL 3.5.9 differs from the release only by C++Builder workarounds (5 files) | `openssl_tree.py --check` | 2026-10-11 |
| WinSCP `master`'s `aes-ni.c` pops its clang attribute pragma under `_WIN64` but pushes it under `__clang__`, and picks `_mm_setr_epi8` only under `_WIN64` | Fails to compile with clang for any other target, and would bring back the AES-NI SDCTR bug; fixed by `putty-aes-ni-non-win64-clang.patch` | 2026-10-11 |
| Upstream FileZilla truncates a pointer on x86-64 | `CAsyncSocketEx` stores `this` with `SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG)this)`; fixed by `filezilla-64bit-window-userdata.patch` | 2026-10-11 |
| lld reports undefined references in every object it pulls in, even from functions `--gc-sections` would drop | Local test: an unreferenced function calling an undefined symbol fails the link with and without `-ffunction-sections` | 2026-10-11 |
| Upstream WinSCP is built for 32-bit Windows only | `PuTTYVS.vcxproj` has only `Win32` configurations, and PuTTY's option defaults cast string addresses to `int`, which only fits in 32 bits | 2026-10-11 |
| clang accepts `static __declspec(property(...))` syntactically but treats it as an ordinary static data member | `-fsyntax-only` passes; linking fails with an undefined reference to the "property" on both targets | 2026-10-11 |
| clang 19 supports `__declspec(property)` on Linux under `-fms-extensions` | Local test: field r/o, field r/w, method+validating setter, read-modify-write, indexed — all pass | 2026-10-10 |
| GCC 14 rejects it outright | `error: 'property' has not been declared` | 2026-10-10 |
| clang cross-emits x86-64 COFF for `windows-gnu` and `windows-msvc` | Local test, property emulation intact | 2026-10-10 |
| Qt6 present on this host; Qt6-for-mingw absent from Debian | `apt-cache search` returns nothing for mingw+qt | 2026-10-10 |
| Wine 10.0 installable from apt | `apt-cache policy wine` | 2026-10-10 |
| Core has 591 `__property` decls; 0 use `default`/`nodefault`/`stored` | grep over `source/core/*.h` | 2026-10-10 |
| Core has no `__try`/`__except` | grep | 2026-10-10 |
| `ConsoleRunner.cpp` has zero `forms/` dependencies | include analysis | 2026-10-10 |
| Stored-password crypto is not DPAPI | no DPAPI symbols in core | 2026-10-10 |
| WinSCP's own compiler is already clang-based | `#ifdef __clang__` in `source/core/Global.h:37` | 2026-10-10 |
| Far-NetBox tracks WinSCP 6.5.6, last commit 2026-07-24, ~16.3k-line VCL shim | repo inspection | 2026-10-10 |
| Core has 613 `__property` declarations across 69 files (`.h` and `.cpp`) | `transform.py --check` | 2026-10-10 |
| 0 of 613 span multiple lines; 0 have comma-bearing or template types | AST-free scan of `core/*.h` | 2026-10-10 |
| All 613 transform to 9 macro forms with **0 unhandled** | `transform.py` | 2026-10-10 |
| All 613 transformed declarations compile under clang | `check_properties.py`, exit 0 | 2026-10-10 |
| Transform is idempotent and line-count-preserving across all 69 files | `transform.py`, second pass is a no-op | 2026-10-10 |
| All 8 property shapes behave correctly at runtime (incl. validating setters, read-modify-write, indexed) | `vcl_property.h` self-test, assertions pass | 2026-10-10 |
| Cross-built Windows PE32+ binary passes the full property suite under Wine | `clang --target=x86_64-w64-mingw32`, exit 0 | 2026-10-10 |
| Static Windows link needs lld plus explicit `-L` for `libwinpthread` | GNU ld fails; lld leaves only KERNEL32 + msvcrt | 2026-10-10 |
| Dynamic Wine runs need **two** DLL dirs on `WINEPATH` | `libwinpthread-1.dll` lives apart from libstdc++/libgcc | 2026-10-10 |
| Importing linscp history into a winscp fork merges with **zero conflicts** | `git merge --allow-unrelated-histories`; 10,602 commits, both roots, all port commits preserved | 2026-10-10 |
| Applied in-tree, the core transform is a **613-line diff across 23 files** | measured on the rehearsal fork | 2026-10-10 |
| After conversion, an upstream release touching a property line conflicts in **1 file**, resolvable mechanically | simulated release; `checkout --theirs` + re-run transform; new upstream property transformed automatically | 2026-10-10 |
| Transform is byte-exact outside property lines; CRLF fully preserved | 76,551 CRLF, 0 bare LF; `Terminal.h` 1003 = 1003 | 2026-10-10 |
| linscp tracks **0** WinSCP source files; only the submodule gitlink | `git ls-files` | 2026-10-10 |
| Both targets build and pass 4/4 tests; Windows runs under Wine via CTest | `ctest` on `build/linux` and `build/win` | 2026-10-10 |
| VCL surface the core needs: ~45 real types, dominated by `UnicodeString` (6,248), `TStrings` (521), `Exception` (438), `TDateTime` (250) | `census.py`, resolved against real mingw/OpenSSL/stdlib headers | 2026-10-10 |
| Core's Win32 headers (`shlobj`, `psapi`, `winioctl`…) are satisfied free on the mingw target | only VCL needs shimming for Windows | 2026-10-10 |
| `-fshort-wchar` yields `sizeof(wchar_t) == 2` on both linux-gnu and mingw32 | `static_assert` compiled for both targets | 2026-10-10 |
| `char_traits<wchar_t>` really does call `wmemcmp`/`wcslen`/`wmemchr`; `char_traits<char16_t>` does not | `/usr/include/c++/14/bits/char_traits.h:512,522,534` | 2026-10-10 |
| `UnicodeString` API scoped from measured use: `IsEmpty` 594, `Length` 444, `c_str` 247, `SubString` 184, `Pos` 86, `Delete` 79 | grep over the generated core | 2026-10-10 |
| 6/6 tests pass on Linux and on Windows under Wine, including `UnicodeString` | `ctest` both build trees | 2026-10-10 |
| **A real core TU compiles against the shim with 70 errors from 12 distinct missing VCL types** (was 109 / 24) | `compile_census.py` on `core/Common.cpp`, mingw target | 2026-10-10 |
| On the mingw target every remaining diagnostic is a VCL gap, not a platform one | Win32 types all resolve via the real mingw headers | 2026-10-10 |
| 7/7 tests pass on both targets after the byte-string family | `ctest` | 2026-10-10 |
| `Common.cpp` down to **30 errors / 5 distinct missing types** (from 109 / 24) | `compile_census.py` | 2026-10-10 |
| 10/10 tests pass on Linux and on Windows under Wine | `ctest` both trees | 2026-10-10 |
| The ratchet detects a regression: baseline 25 against actual 30 fails | simulated baseline, exit 1 | 2026-10-10 |
| `TDateTime` matches Delphi's four documented anchors, including -1.25 = 1899-12-29 06:00 | `test_datetime` | 2026-10-10 |
| Delphi's `DayOfWeek` offset verified independently against five known dates | standalone cross-check, k=6 | 2026-10-10 |
| ~~`Common.cpp`: 109 -> 5 errors~~ — **superseded**: the parse was aborting at a missing header | see the correction above | 2026-10-10 |
| Upstream pinned to **6.5.8**, the latest stable release | `git tag`; 6.6.x is entirely RC/beta | 2026-10-10 |
| The transform handled the version change with **no script edits**: 624 declarations across 70 files, 0 unhandled | `transform.py` on 6.5.8, which differs from the old pin by ~2,500 lines across 58 core files | 2026-10-10 |
| All 624 transformed declarations compile on 6.5.8 | `check_properties.py`, exit 0 | 2026-10-10 |
| Honest aggregate census: **2,409 errors across 9 core TUs**, ratcheted | `census_ratchet.py` | 2026-10-10 |
| 12/12 tests pass on both targets after the version change | `ctest` both trees | 2026-10-10 |
| All 44 `__closure` typedefs rewrite to `TClosure<Sig>`; 16 single-line, 28 spanning lines, 0 unhandled | `transform.py` | 2026-10-10 |
| Changed lines now fully accounted for: 624 property + 44 closure + 28 template-end = 696 | `verify_transform.py` | 2026-10-10 |
| `CLOSURE()` binds the enclosing instance correctly on both targets, including non-void returns and reference arguments | `test_closure` on Linux and under Wine | 2026-10-10 |
| Aggregate census **2,409 -> 1,924** across 9 TUs; `Terminal.cpp` missing types 51 -> 20 | `census_ratchet.py` | 2026-10-10 |
| 116 call sites pass a bare member-function name where a closure is expected | clang diagnostic count across all core TUs | 2026-10-10 |
| 13/13 tests pass on both targets | `ctest` | 2026-10-10 |
| **`core/Option.cpp` compiles clean on mingw: exit 0, 0 errors, 0 fatal** | direct clang run, verified rather than inferred from the census | 2026-10-10 |
| The same TU still fails on Linux with 10 Win32-type errors | the deferred P1-07 gap | 2026-10-10 |
| Aggregate census **1,924 -> 1,220** across 9 TUs | `census_ratchet.py` | 2026-10-10 |
| `Format` handles width, alignment, precision and explicit argument indices; unknown specifiers and missing arguments stay visible instead of dropping text | `test_strutils` | 2026-10-10 |
| 161 narrow string literals in core require `UnicodeString(const char *)` | grep over generated core | 2026-10-10 |
| 14/14 tests pass on both targets | `ctest` | 2026-10-10 |
| **`core/Option.cpp` compiles clean on Linux *and* Windows: 0 errors each** | direct clang runs on both targets | 2026-10-10 |
| FILETIME conversions validated against the real Win32 API under Wine, using epoch anchors fixed by the format | `test_win32_posix` on both targets | 2026-10-10 |
| Real Win32 `FileTimeToSystemTime` faults on a null argument; the POSIX version rejects it | Wine crash dump, `cmpq $0, (%rcx)` | 2026-10-10 |
| Honest totals with 0 units aborting: mingw 1,334, Linux 1,575 | `census_ratchet.py`, both targets | 2026-10-10 |
| 15/15 tests pass on both targets | `ctest` | 2026-10-10 |
| Census after streams: mingw 1,242, Linux 1,483, 0 aborting | `census_ratchet.py` | 2026-10-10 |
| 16/16 tests pass on both targets | `ctest` | 2026-10-10 |
| **`Option.cpp` and `NamedObjs.cpp` both compile clean on both targets** | `compile_census.py`, 0 errors each | 2026-10-10 |
| Census after containers: mingw 1,070, Linux 1,299, 2/9 units clean | `census_ratchet.py` | 2026-10-10 |
| 17/17 tests pass on both targets | `ctest` | 2026-10-10 |
| Resource strings resolved with **no core changes**: 608 strings generated from upstream's `.rc`, all ids matched, 0 unknown, 0 duplicates | `gen_resources.py` | 2026-10-10 |
| 18/18 tests pass on both targets | `ctest` | 2026-10-10 |
| **`FileBuffer.cpp` joins the clean set: 3/9 units at 0 errors on both targets** | `compile_census.py` | 2026-10-11 |
| Census after storage and arrays: mingw 963, Linux 1,182, 0 aborting | `census_ratchet.py` | 2026-10-11 |
| `TRegistry` round-trips against the real Windows registry under Wine | `test_storage` on the mingw target | 2026-10-11 |
| 20/20 tests pass on both targets | `ctest` | 2026-10-11 |
| Closure call sites: **62**, not 118 — resolving macro-expansion diagnostics to their definitions collapsed the duplicates | `bind_closures.py --discover` | 2026-10-11 |
| `DynamicArray::Length` is a read/write property, not a method; 56 of the original 118 reports were this, not closures | `Buffer.Length = 10*1024` in core | 2026-10-11 |
| All 758 changed lines accounted for: 624 property + 44 closure typedef + 28 template end + 62 binding | `verify_transform.py` | 2026-10-11 |
| Census after closure binding: mingw 933, Linux 1,152 | `census_ratchet.py` | 2026-10-11 |
| 21/21 tests pass on both targets | `ctest` | 2026-10-11 |
| Census after the long tail: mingw **619**, Linux **863** (from 933 / 1,152) | `census_ratchet.py` | 2026-10-11 |
| Every `__closure` in core converted: 44 typedefs, 2 inline parameters, 62 bound sites | `transform.py`, 0 remaining | 2026-10-11 |
| 22/22 tests pass on both targets | `ctest` | 2026-10-11 |
| Census after the property-access fix: mingw **342**, Linux **586**; 4/9 clean on mingw | `census_ratchet.py` | 2026-10-11 |
| Borland's `__property` reaches *private* accessors; `__declspec(property)` does not | `TBookmark` declares public properties over private setters | 2026-10-11 |
| `__finally` in core: **182 blocks**, 0 `__try`, 0 `__except`; only 2 bodies contain a `return` | grep plus brace analysis | 2026-10-11 |
| `UnicodeString` has implicit numeric constructors in C++Builder | `EXT_EXCEPTION_METHODS` calls `ExtException(E, Ident)` with an `int`, and the only viable overload takes a string | 2026-10-11 |
| All **179** `try`/`__finally` blocks transformed, 0 unhandled; the patch series is still empty | `transform.py` | 2026-10-11 |
| Changed lines: 1,297, every one accounted for — 624 property, 46 closure, 28 template end, 62 binding, 537 try/finally (179 x 3) | `verify_transform.py` | 2026-10-11 |
| Census after try/finally: mingw **314**, Linux **496**; 4/9 clean on *both* targets | `census_ratchet.py` | 2026-10-11 |
| 23/23 tests pass on both targets | `ctest` | 2026-10-11 |
| `FileMasks.cpp` clean on both targets; **5/9 units clean on both** | `compile_census.py` | 2026-10-11 |
| Census: mingw **266**, Linux **441** | `census_ratchet.py` | 2026-10-11 |
| Vendored expat compiles for both targets; the mingw sysroot has no expat | `cmake/expat.cmake` | 2026-10-11 |
| The Windows CRT's `towlower` folds only ASCII in the default locale | the same test run against the real CRT under Wine | 2026-10-11 |
| 24/24 tests pass on both targets | `ctest` | 2026-10-11 |
| CI green: 57/57 tests on Linux, 55/55 on Windows under Wine, none skipped | GitHub Actions, PR #1 | 2026-10-12 |
| .NET on Linux: an anonymous pipe made `HandleInheritability.Inheritable` reaches a child `Process.Start` runs, at the descriptor `GetClientHandleAsString` names | ctest `dotnet` with the fork's assembly, .NET 8.0.425 | 2026-10-12 |
| 12/12 tests pass on Linux and on Windows under Wine | `ctest` both trees | 2026-10-10 |
| Directory search validated against both backends: real `FindFirstFileW` under Wine, and `dirent` + `fnmatch` on Linux | `test_sysutils` on both targets | 2026-10-10 |
| `__closure` scope: 44 distinct closure types, 16 typedefs; call sites pass bare member-function names | grep over generated core | 2026-10-10 |

## Task status

### P0 — Foundation
| ID | Task | Status | Note |
|---|---|---|---|
| P0-01 | Repo layout | done | `port/{shim,platform,tools,patches}`, `qt/`, `cmake/` |
| P0-02 | Pin submodule to a release tag | done | was 6.5.8; since 2026-10-11 tracks `master` by decision (decisions table) |
| P0-03 | Dual-target property macro header | done | `port/shim/vcl_property.h`; 9 forms, self-tested |
| P0-04 | Verify C++Builder accepts the macro | blocked | Q-2: needs licence + Windows |
| P0-05 | `transform.py` property rewriter | done | 613/613, 0 unhandled |
| P0-06 | `transform.py` fastcall/ARRAYOFCONST | done | `__closure` handled by P0-06b/c/d |
| P0-06b | `__closure` type + typedef transform | done | 44 typedefs; `CLOSURE()` macro verified on both targets |
| P0-06c | `__closure` call-site binding | done | 62 sites; verified as a test |
| P0-06d | `__closure` inline parameter form | done | 2 sites in `SessionInfo` |
| P0-07 | Transform idempotence tests | done | idempotent + line-count parity asserted |
| P0-08 | CMake skeleton | done | transform+verify wired as a build step; CTest on both targets |
| P0-09 | Minimal shim for `core/Common.cpp` | done | Overtaken: all 33 core units compile and link on both targets |
| P0-10 | mingw-w64 sysroot + clang toolchain file | done | `cmake/toolchain-mingw-w64.cmake`; clang + lld + Wine emulator. Replaced 2026-10-13 by `cmake/toolchain-llvm-mingw.cmake` (ADR 0011) |
| P0-11 | CI matrix Linux + Windows cross | done | `.github/workflows/ci.yml`, on pull requests; both jobs run the full ctest suite |
| P0-12 | Wine smoke tests in CI | done | the windows-cross job runs ctest under Wine (`CMAKE_CROSSCOMPILING_EMULATOR`), with the null display driver |
| P0-13 | Golden-output harness | done | `port/tests/golden/run_golden.py` (capture, normalise, compare, check), cases in `golden/cases`; ctest `golden` against `golden/expected`. Linux and Windows linscp agree on all 35 logs |
| P0-14 | Protocol server containers | done | Not containers: loopback servers per test (`fixtures.py`: OpenSSH, rclone, the suite's FTP and S3 servers), on Linux and Windows alike |
| P0-15 | Capture golden corpus from real WinSCP | done | WinSCP 6.5.7 on a Windows 11 VM, rclone v1.75.2: 35 logs in `port/tests/golden/expected` |
| P0-16 | ADR log | done | `doc/qt/adr/`: ADRs 0001-0009 and an index; kept up as decisions are made |
| P0-17 | Property compile-check harness | done | `check_properties.py`, 613 declarations, clang exit 0 |
| P0-18 | HTML render of plan docs | done | `doc/qt/plan.html` via `build-html.sh` |
| P0-19 | Publish repository, licence, root README | done | public at `99ecarvalho/linscp`; work on `qt` |
| P0-20 | Fork-branch generator | todo | deferred to P1-19 (Q-7); rehearsed manually in P0-22 |
| P0-21 | `verify_transform.py` upstream-diff audit | done | asserts line parity, byte-exactness and 1 macro per changed line |
| P0-22 | Rehearse history import into a winscp fork | done | zero-conflict merge; 613-line in-tree diff; mechanical conflict resolution |
| P0-23 | `check_docs.py` TASKS/TRACKER consistency | done | found 3 drifts on first run |
| P0-24 | VCL surface census | done | ~45 types; `build/shim_spec.txt` |
| P0-25 | ADR 0001 character width | done | accepted: `-fshort-wchar` + `std::u16string` |
| P0-26 | `check_wide_abi.py` ADR enforcement | done | `shim_no_wide_abi` test |
| P0-27 | `compile_census.py` | done | classifies clang diagnostics into shim gaps |
| P0-29 | Legacy unqualified RTL header stubs | done | `Classes.hpp` etc.; absence was aborting most TUs |
| P0-30 | Win32 A/W macro collisions undone | done | link-time guard test |
| P0-31 | Ratchet both targets, track aborts | done | an abort is a distinct state, not a low number |
| P0-32 | `port/compat` for upstream gaps | dropped | `help.h` was compensating for the missing `WINSCP` define; withdrawn 2026-10-11 |
| P0-28 | `census_ratchet.py` | done | both targets; aborts tracked separately |

### P1 — Headless core
| ID | Task | Status | Note |
|---|---|---|---|
| P1-01 | Shim: `UnicodeString` class | done | 1-based API, UTF-16, 9 test groups incl. surrogate pairs |
| P1-01b | Shim: free string routines | done | `Option.cpp` reaches 0 errors |
| P1-01d | Shim: `TVarRec`/`Format`/`ARRAYOFCONST` | done | `FORMAT` 438 + `FMTLOAD` 252 call sites |
| P1-01e | Resource strings | done | 608 strings generated from upstream `.rc`; no core changes |
| P1-01c | Shim: byte-string family + UTF-8 codec | done | malformed-input behaviour tested explicitly |
| P1-02 | Shim: `TObject`/`TStrings`/`TStringList` | done | 0-based indexing, case-insensitive default, CommaText round trip |
| P1-02b | Shim: `TList`/`TObjectList`/sets | done | ownership and `Notify` semantics tested |
| P1-03 | Shim: `TDateTime` + date/time RTL | done | documented anchors, negative composition, `FormatDateTime` |
| P1-03b | Shim: `SysUtils`/`IOUtils`/`StrUtils` paths | done | everything core calls from those units compiles for Windows; POSIX file calls are P1-07b |
| P1-04 | Shim: exceptions | done | 17 Delphi classes; base and sibling matching tested |
| P1-05 | Shim: `TCriticalSection`/`SyncObjs` | done | recursive; thread exclusion tested |
| P1-05b | Shim: `TThread`/`StartThread` | done | Win32 threads, events and waits on POSIX; `BeginThread`/`EndThread` on both. Tested against the real kernel under Wine. `TThread` itself dropped — dead upstream |
| P1-08b | Shim: masks, paths, timespan, encoding, env | done | the census long tail |
| P1-08c | Shim: XML DOM over expat | done | real parsing; FileZilla-shaped fixtures |
| P1-07c | `port/platform` wide-char functions | done | ASCII folding, matching the Windows CRT |
| P1-09b | `try`/`__finally` for POSIX | done | 179 blocks, 0 unhandled; same expansion on both targets |
| P1-06 | Shim: streams | done | `TStream`/`THandleStream`/`TFileStream`/`TMemoryStream` |
| P1-06b | Shim: INI/registry storage | done | INI parses real-world files; registry real on Windows |
| P1-06c | Shim: `DynamicArray` + file API | done | sharing semantics tested |
| P1-06d | Select INI backend on Linux | done | `$XDG_CONFIG_HOME/linscp/WinSCP.ini`, or `WinSCP.ini` beside the executable, or `/ini`; sites load and save (ctest `ini_sites`, `cli_session`) |
| P1-07 | `port/platform` Win32 subset | done | types, FILETIME/SYSTEMTIME, errors, winsock names |
| P1-07b | `port/platform` Win32 file/handle functions | done | every Win32 call core makes: files, time zones (real transition rules), modules, processes (/proc), paths, string order, environment, XDG folders, MIME and type names, reparse points, trash, registry entry points, memory regions, version resources, user name, certificate chains (OpenSSL, system trust store); tested against real Win32 under Wine where observable. Core compiles on Linux |
| P1-08 | Socket-event emulation | done | `ws2_32` module and `WSAEventSelect`/`WSAEnumNetworkEvents`; one test passes against real Winsock (Wine) and the emulation. `WSAAsyncSelect` (FileZilla) is P1-12's |
| P1-09 | PuTTY unix backend re-import | done | As decided 2026-10-11: WinSCP's PuTTY with its Windows layer over the emulation, Linux services replaced per unit (`port/putty/linux`); SFTP/SCP sessions pass on Linux, proxied ones included |
| P1-10 | PuTTY delta reconciliation | done | `doc/qt/putty-deltas.md`: every delta of WinSCP's PuTTY against stock 0.85 classified, Linux-relevant flags checked against the build; ctest `putty_deltas` fails when a bump changes any |
| P1-11 | FileZilla MFC subset replacement | done | Not replaced: WinSCP's subset (`CString`/`CFile`/`CTime` over the shim) builds as is on both targets |
| P1-12 | FileZilla epoll rewrite | done | Not rewritten: FileZilla builds unmodified on Linux over an emulated user32 subset (windows, message queues, timers, `WSAAsyncSelect`, `WSAAsyncGetHostByName`); ctest `messages` passes against user32/ws2_32 (Wine) and the emulation. One patch (`in_addr`'s `s_addr`) |
| P1-13 | System libs | done | PuTTY, puttyvs, neon, libs3 from the WinSCP submodule, OpenSSL 3.5.9 from its own, expat from its make file, on both targets; FileZilla + MFC on Windows (Linux is P1-12). neon on POSIX: `config-posix.h` from its `configure` |
| P1-14 | Drop `puttyvs` | done | the MSVC build is replaced by clang building the same files with the same defines, from `PuTTYVS.vcxproj` |
| P1-15 | Console IPC → stdio | done | `linscp-cli` (port/frontend/CliMain.cpp, StdioConsole.cpp): core's script on stdin/stdout with TConsoleRunner's query, prompt and input handling and winscp.com's console semantics; `/command`, `/script`, `/parameter`, `/stdout`, `/stdin`, `/log`, `/xmllog`, `/loglevel`, `/logsize`, `/ini`, `/rawconfig`, `/keygen`, `/help`. ctest `cli_session` on both targets |
| P1-16 | Headless `Interface.h` frontend | done | `port/frontend`: all 17 functions plus startup/shutdown; core initialises and finalises under Wine. Linux follows core (P1-07b) |
| P1-17 | First SFTP session on Linux | done | Core links on Linux, and the four loopback-`sshd` session tests pass there as on Windows (negotiated, AES-CTR, AES-GCM, ChaCha20-Poly1305). Needed the `lp64_long` transform rule and the link-level wide-function check |
| P1-18 | Windows cross-build + Wine | done | Core and the CLI, cross-built, run real SFTP sessions and scripts under Wine (`sftp_session*`, `cli_session`) |
| P1-19 | Golden suite green, both platforms | done | ctest `golden`: Linux and Windows linscp match real WinSCP on SFTP, WebDAV(S), FTP, FTPES, FTPS and S3 (two documented differences); SCP in ctest `protocols` only. **P1 gate** |
| P1-20 | .NET assembly drives Linux CLI | done | On Windows: the assembly built from the pinned source runs sessions through linscp-cli over SFTP, FTP, FTPES, WebDAV and S3 (`port/tests/dotnet`); Linux needs another transport (P1-23) |
| P1-21 | `#ifdef` out Windows-only subsystems | dropped | nothing to `#ifdef`: `dragext`, `Setup.cpp`, jump lists and file associations live in `windows/`, `forms/` and `dragext/`, none compiled by linscp, and core references none of them (`core_link_build` links with nothing undefined). Their user-facing parts are P3-08's |
| P1-22 | CLI `keepuptodate` (directory watching) | done | `port/frontend/DirectoryWatcher.cpp` (inotify; ReadDirectoryChangesW), the controller's logic in CliMain.cpp; tested on Linux (ctest) and natively on Windows |
| P1-23 | .NET assembly on Linux (new transport) | done | ADR 0010: `/consolepipes` in `linscp-cli` (ctest `console_pipes`); the assembly built with `port/patches/dotnet/console-pipes.patch` drives linscp on Linux (ctests `winscpnet_build`, `dotnet`, in CI); offered upstream as winscp/winscp#73 (draft), not waited on |

### P2 — Qt shell
| ID | Task | Status | Note |
|---|---|---|---|
| P2-01 | Qt6 for mingw | done | R4 retired without building Qt: MSYS2's clang64 Qt 6.8.2 and its runtime, locked (`port/tools/fetch_msys2_qt.py`), with llvm-mingw 19.1.7 (`cmake/toolchain-llvm-mingw.cmake`); ADR 0011 |
| P2-02 | Qt app skeleton | done | `port/qt`: `linscp` on Qt 6.8.2 both platforms, core through `CoreBridge` (ADR 0012), Qt units with Qt's `wchar_t` (ADR 0001 amended); ctests `gui_build`, `gui_smoke`, `windows_imports` |
| P2-03 | Qt `Interface.h` implementation | done | `port/qt`: `IFrontend` (`QtFrontend`, `QueryDialog`, `PromptDialog`), `QtInterface.cpp`, `Session.cpp`; `Interface.h` split into `FrontendCommon.cpp` and per-program halves. ctests `qt_session`, `qt_session_ftp`, `qt_dialogs`, `qt_gui_session`; native on Windows 11 too. Not yet: query timer events, transfer progress (P2-07) |
| P2-04 | Site manager | done | `port/qt/SiteManager.cpp` over `Sites.cpp` (core's `StoredSessions`, as upstream's Login dialog uses it); shown at startup; ctests `qt_sites`, `qt_gui_sites`, native on Windows 11 too |
| P2-05 | Login/auth dialogs | done | `SiteAdvanced` (directories, connection, proxy, SSH authentication), keys checked and converted as upstream (`Keys.cpp`), folders and rename in `SiteManager`; ctests `qt_sites` (proxy, key phases), `qt_gui_sites`; native on Windows 11 too |
| P2-06 | Remote file panel | done | `port/qt/RemotePanel.cpp` over `TSession`'s new operations; ctests `qt_gui_panel`, `qt_session`/`qt_session_ftp` (operations over SFTP and FTP); native on Windows 11 too |
| P2-07 | Transfer + queue UI | done | foreground: `TSession::Upload`/`Download`, `TransferDialog`, F5/F6; background: `TSession::Queue*` over `TTerminalQueue`, `QueuePanel`; ctests `qt_gui_transfer`, `qt_session`/`qt_session_ftp`; native on Windows 11 too |
| P2-08 | Local file panel | done | `port/qt/LocalPanel.cpp` over the shared `FileList` model; Commander layout; ctest `qt_gui_local`; native on Windows 11 too |
| P2-09 | i18n pipeline | done | ADR 0013, `doc/qt/translations.md`: core via `gen_translations.py` and shim translation modules (55 languages), the shell via Qt Linguist (`linscp_pt_BR.ts`), Qt's own; ctests `qt_session_pt_BR`, `qt_language`, `translations_current`; native on Windows 11 too |
| P2-10 | Packaging | done | `port/packaging`: install rules and a .deb (CPack, `package-deb`), an AppImage (`build_appimage.py`, `package-appimage`), a Flatpak (`build_flatpak.py`, KDE 6.10 runtime) and the Windows installer (`windows/linscp.iss`, Inno Setup 6.7.3 under Wine, `installer`), each tested installed (the installer on Windows 11, tasks and uninstall); CI builds all four as artifacts (`packages-linux`, `packages-windows`, `packages-flatpak`) |
| P2-11 | P2 exit gate | done | passed 2026-10-15: signed off by the user after daily use (decision) |

### P3 — Qt parity
| ID | Task | Status | Note |
|---|---|---|---|
| P3-01 | `.dfm` → `.ui` converter | dropped | every dialog written by hand from the `.dfm`s without it (decision 2026-10-14) |
| P3-02 | Commander dual-pane | done | WinSCP's Commander keys (panels, `FileView`, main window), select by core's masks (`CoreBridge::MaskMatches`), rename in place, status lines, group and link target columns with the header's menu; ctests `qt_gui_keys`, `qt_gui_panel`; native on Windows 11 too. Not yet (ADR 0012): Shift+F5/F6, the extension column, incremental search status, toolbars, kept layout (P3-04) |
| P3-03 | Synchronization UI | done | `port/qt/SynchronizeDialog.cpp` (Synchronize dialog, comparing progress, checklist) over `TSession::Sync*` (core's `SynchronizeCollect`/`Apply`/`ToQueue`); Commands > Synchronize (Ctrl+S); ctests `qt_gui_sync`, `qt_session`/`qt_session_ftp`; native on Windows 11 too. Not yet (ADR 0012): selected files only, checksum, transfer settings, saved options (P3-04); the checklist's size calculation, move finding, custom commands; keep remote directory up to date |
| P3-04 | Preferences | done | split into P3-04a…e (below), each counted as its own task; done when they are |
| P3-04a | Preferences foundation, Languages, Panels, Commander | done | upstream's `GUIConfiguration` compiled as it is (`port/gui`, ADR 0014); `TShellConfiguration` mirroring `TWinConfiguration`'s keys; the language as upstream chooses it; Options > Preferences (Ctrl+Alt+P) with Panels, Commander and Languages, applied to the panels; ctests `qt_gui_preferences`, `qt_gui_sites`, `qt_language`; native on Windows 11 too. Not yet on these pages (ADR 0014): incremental search, panel state, full row, double-click, font, Explorer-style selection, tree, location profiles, compare criteria |
| P3-04b | Preferences: Transfer, Background, Endurance | done | transfers use the GUI's settings (`TTransferSettings`, `port/qt/TransferSettings.cpp`); the Copy and Synchronize dialogs' Transfer settings group and dialog (`TransferSettingsDialog.cpp`); Preferences' Transfer (presets), Background, Endurance; ctests `qt_session`/`qt_session_ftp`, `qt_gui_preferences`; native on Windows 11 too. Not yet (ADR 0014): autoselection rules, parallel threshold, queue enabling and list visibility |
| P3-04c | Preferences: Window, Explorer, File colors | done | Window (path in title, keep open), File colors (`MaskMatchesPath`), the layout kept in WinSCP's formats (`DirViewParams`, `WindowParams`, `LocalPanelWidth`, `CurrentPanel`, the queue's height); pages in WinSCP's tree order; ctests `qt_gui_preferences`, `qt_gui_panel`; native on Windows 11 too. The Explorer page is not offered (no Explorer interface); not yet: minimize to tray, single instance |
| P3-04d | Preferences: Storage, Logging, Network, Security, Updates | done | Network, Security, Logging, Storage over core's configuration (ADR 0014); the temporary directory used; upstream's `%TEMP%`/`%APPDATA%` defaults working on POSIX (Win32 layer, `core-log-file-name-delimiter.patch`); ctests `qt_session`/`qt_session_ftp`, `win32_on_posix`, `qt_gui_preferences`; native on Windows 11 too. Updates not offered (WinSCP's own); master password, host CAs not yet |
| P3-04e | Preferences: Editors, Integration, Applications, Commands, Drag & Drop | done | Applications (the PuTTY command, its options as upstream enables them, "automatically open new sessions in PuTTY"; the registry key on Windows) and Commands (the custom commands edited) done. Editors and Internal editor done: upstream's editor preference list (sliced), external editors and the associated application with remote copies uploaded on change, Files > Edit With and Open, the internal editor's font, colours and large-file question (ADR 0015 amendment). Drag & Drop is dropped: its settings are the Windows shell extension's (fake file or temporary folder), which linscp does not have (ADR 0012). Integration done: on Windows upstream's protocol registration and search path (a Windows-only slice of `Setup.cpp`) and a desktop shortcut; on POSIX their XDG counterparts (a desktop entry for the protocols' URLs and its `mimeapps.list` defaults, a desktop icon, `~/.local/bin` links). Not ported: "Send to" (needs the command line's `/upload`) |
| P3-05 | Internal editor | done | `port/qt/EditorWindow.cpp` (WinSCP's editor: find and replace, go to line, UTF-8 or the system's code page with fallback, BOM and line endings kept, status bar); Files > Edit (F4), local files in place, remote ones through `TSession::EditDownload`/`EditUpload` (temporary directory, queue upload, "check not modified"), editors closed with the session; `core-local-eol-platform.patch`; ctests `qt_gui_edit`, `qt_session`/`qt_session_ftp`; native on Windows 11 too. Not yet (ADR 0015): reload from the server, save all, window position and font, external editors (P3-04e) |
| P3-06 | Remaining 48 dialogs, batched | done | split into P3-06a…f (below), each counted as its own task; done when they are. Written by hand from the `.dfm`s, as P3-02…05 were, without waiting on P3-01 |
| P3-06a | Dialogs: file operations | done | Properties (F9: `PropertiesDialog`, `RightsFrame` with mixed bits, owner and group, recursion, size, checksums) over core's `TRemoteProperties`; Create folder (F7) with permissions kept in `NewDirectoryProperties`; links (Alt+F6, local ones the system's); Duplicate and Move To (Shift+F5/F6) within the server, through a temporary copy where it cannot copy; ctest `qt_gui_properties` (FTP); native on Windows 11 too. WinSCP's CopyLocal belongs to its local-to-local browsing, which the shell does not have. Not yet: local files' properties, S3 ACLs and tags, the secondary shell for checksums and copies |
| P3-06b | Dialogs: navigation | done | Open directory and bookmarks (Ctrl+O, Ctrl+B) over core's `TBookmarks`, kept by `TShellConfiguration` under `TWinConfiguration`'s keys; Location Profiles (the Commander's preference), with folders; Find Files (Alt+F7) over core's `FilesFind`; Edit file mask; the panels' filter (Ctrl+Alt+F) and the Select dialogs' Edit...; ctests `qt_gui_bookmarks`, `qt_gui_keys`; native on Windows 11 too. Not yet: bookmarks' shortcuts (shown, not editable or bound), the Go To menu's bookmark list, opened folders kept, mask hints |
| P3-06c | Dialogs: session tools | done | Server/Protocol Information (`TSession::ServerInfo`, space available); Generate Session URL/Code and File URL (TGenerateUrlDialog's generation ported to `GenerateCode.cpp`, its RTF shown by `RtfText.cpp`, scripts for linscp's command-line client); Open Terminal (core's `AnyCommand`, a separate shell session after WinSCP's question); Edit Raw Settings (the site manager's Advanced dropdown); Tools > Import Sites (PuTTY, KiTTY, FileZilla, OpenSSH, INI, known_hosts); ctests `qt_gui_tools`, `qt_gui_properties`, `qt_gui_sites`; native on Windows 11 too. Not yet: the transfer code from the Copy dialog, the console's window size kept, OpenSSH key conversion on import |
| P3-06d | Dialogs: custom commands | done | WinSCP's `TCustomCommandType`/`TCustomCommandList` and `TLocalCustomCommand` compiled as they are from transform slices (`port/tools/slices.json`, ADR 0014 amendment); the list kept as `TWinConfiguration` keeps it, WinSCP's defaults; Files > File Custom Commands and Commands > Static Custom Commands with their keys, Enter... and Last; the Custom command dialog and the parameters' prompts; remote commands run over core (`CustomCommandOnFiles`, `AnyCommand`), in a shell session where needed; ctest `qt_gui_commands` against OpenSSH's sshd (Linux; skipped on Windows). Not yet: local commands (on downloaded files), extensions and their options dialog, the commands' list edited (Preferences > Commands, P3-04e) |
| P3-06e | Dialogs: keep remote directory up to date | done | Upstream's `TSynchronizeController` compiled as it is from a slice, over linscp's `TDiscMonitor` (the directory watcher `keepuptodate` uses, polled from the GUI's timer; ADR 0014 amendment); Commands > Keep Remote Directory up to Date (Ctrl+U), WinSCP's dialog and log, its options kept as `GUIConfiguration` keeps them, the synchronize-on-start question, "continue on error"; ctest `qt_gui_keep` (Linux, Wine, native Windows). Not yet: start in a new window, minimize to the tray, transfer presets' menu |
| P3-06f | Dialogs: the rest | done | About's License button and WinSCP's third-party list (core's libraries, Expat's license shown, Qt after them; the License dialog); Clean Up (the site manager's Tools; `TCleanupDialog`'s data over core's cleanups, temporary folders as `TWinConfiguration`); S3 tags on Properties' Tags page with the Add/Edit tag dialog (the suite's S3 server gains `?acl` and `?tagging`; ctest `qt_gui_tags`); Select keyboard shortcut for shared bookmarks and location profiles, which then open them from the main window. Save Workspace moved to P3-13 (tabs); usage statistics not ported |
| P3-07 | Drag & drop | done | between the panels, within one (onto a directory), and from other programs onto the remote panel: `FileView` in `port/qt/FileList.cpp`, `MainWindow::Drop`, `TSession::Move`; `RemoteFileDrag` (remote files out to other programs, downloaded at the drop); ctest `qt_gui_drop`; native on Windows 11 too; dragging out checked by hand with Explorer (Windows 11) and Nautilus, foreground and background |
| P3-08 | Shell integration | done | Commands > Open in PuTTY (Ctrl+P): upstream's `OpenSessionInPutty` compiled from a slice of `GUITools.cpp` (ADR 0014 amendment), the program started through the frontend; on POSIX the session on PuTTY's command line, and by default PuTTY if installed, else `ssh` in the desktop's terminal ("open in terminal"). `linscp <site or URL>` opens it at start, as WinSCP.exe (`TSession::FromCommandLine`; ctest `qt_gui_startup`). Go To > Explore Directory (Ctrl+Alt+E). Local files open with their associated application, as before. Not yet: a remote file opened with its associated application and registering as the URL handler (with Preferences > Integration, P3-04e); folders of sites on the command line (P3-13) |
| P3-09 | Accessibility | done | An audit (`port/tests/AccessibilityAudit.cpp`) of every window the GUI tests show, the first time each appears, and of each page of the dialogs with pages (ctest `qt_gui_accessibility` shows those the other tests only drive): every control that takes the focus has a name for screen readers (its text, its label's, or one given); every label's mnemonic reaches a control (a buddy); no two visible controls of a window, nor items of a menu, share a mnemonic, except where WinSCP's own forms do (listed). Fixed: labels with no buddy (their `&` shown as is), unnamed lists, views and paths (the panels' named Local/Remote files and path), WinSCP's `&patterns` hints reachable by keyboard, linscp's own clashing captions (New Folder, keyboard-interactive, Copy). Platform standard buttons (OK, Cancel) keep the platform's mnemonics |
| P3-10 | P3 exit gate | done | passed 2026-10-14 (decision): every WinSCP dialog has its counterpart (the Explorer interface aside), menus in WinSCP's order (ctest `qt_gui_menus`), the daily Commander commands (P3-14); the rest of the menus are P3-15's backlog |
| P3-11 | Remember last used directories | done | `TSite::UpdateDirectories` (advanced settings' Directories page), `TSession::SaveDirectories` as `TTerminalManager::SaveTerminal` on disconnect, another session, or closing; the local directory's quiet fallback; ctest `qt_gui_sites`; native on Windows 11 too. "Preserve local directory" is P3-04's |
| P3-12 | Local-to-local browsing | done | Tabs > New Tab > Local Tab, as WinSCP's local browser: a tab with no session and a second local panel in the remote one's place (the right side, swapped with it), titled by its two directories; Copy and Move (F5, F6) and drags between its panels copy and move local files (`LocalLocalCopy`), after the Copy dialog (`TCopyLocalDialog`: target path and file mask, Browse, the directory created after asking, several files to one name confirmed), an existing file overwritten after asking, a directory merged; the popups, Mark, Files and the Right menu act on the panel focused. Not yet: local tabs in saved workspaces. ctest `qt_gui_localtab` |
| P3-13 | Session tabs and workspaces | done | Tabs: a session opened goes to a tab of its own, each tab with its own remote and local directories, its editors uploading to it whichever tab is active, every tab's session kept alive (`TTerminal::Idle`, which the shell had not called); the Tabs menu (New, Duplicate, Close Ctrl+Shift+D, Next and Previous Ctrl+Tab). Workspaces: Tabs > Save Workspace (`TSaveWorkspaceDialog`'s name, passwords and automatic saving) stores the tabs as core's workspace sessions; the site manager lists workspaces, logs in to one (a tab per session) and deletes it; saved on closing when automatic and opened at the next start; a workspace or folder of sites named on the command line opens in tabs. ctest `qt_gui_tabs` |
| P3-14 | Daily Commander commands | done | the gate waited for it (decision 2026-10-14). Tabs > Rename Tab, Reconnect Session (a lost connection reopened where it was, `TTerminal::Reopen`), Save Session as Site (`TSaveSessionDialog`: name, folder, password), Change Password (a secondary session, as WinSCP), Install Public Key into Server (the key checked and converted as on login, non-OpenSSH servers asked about); Commands > Clear Caches; ctest `qt_gui_sessioncommands`. Commands > Compare Directories (Shift+F2: newer or missing files selected in each panel, times at the coarser listing's precision, by time and size as Preferences > Commander's new criteria say) and Synchronize Browsing (Ctrl+Alt+B: the other panel follows, a missing directory created after asking, per tab); Files > New > File (local made, remote edited empty and uploaded once saved), Calculate Directory Sizes (Shift+Alt+Enter); Go To > Path from Other Panel (Ctrl+., local tabs); ctest `qt_gui_panelcommands`. Files > Copy to Clipboard and Paste from Clipboard (Ctrl+C, Ctrl+V: the panel's drag data, remote files by their full paths and downloaded as another program pastes them; pasted, uploaded, copied locally or downloaded; a line of text opened as a session URL or gone to as a path); Options > Status Bar and Queue (show, hide when empty -- WinSCP's default, now linscp's -- or hide), kept under WinSCP's keys; ctest `qt_gui_clipboard`. Not done: pasting remote files into the remote panel (WinSCP's duplicate) |
| P3-15 | The rest of the Commander's commands | done | done 2026-10-15, every item of WinSCP's Commander menus but Help > Show Tips and Donate (not offered) and, on POSIX, the local Columns' Attributes (Permissions there). Done: Commands > Queue's commands (TQueueController's operations: show query/error/prompt, execute now, suspend, resume, cancel, move up/down, all; Go To), also the queue list's context menu; the panels' Type column (the shell's type names on Windows, MIME descriptions elsewhere), Sort submenu, Columns' Name, Size Automatically and Reset Layout; Mark > Restore Selection; Help's pages (the documentation WinSCP's, the homepage, support and history linscp's GitHub ones) and Check for Updates (linscp's GitHub releases, pre-releases included; ctest `qt_gui_options`); Options > Tabs, Left/Right Panel > Status Bar, Queue > Toolbar, Reset Columns Layout and Customize, kept under WinSCP's keys; Queue > Once Empty (Stay Idle, Disconnect Session, Sleep Computer, Shut Down Computer: the session closed, the computer asleep or shut down through logind or Windows' own calls, then the application closed as ConfirmExitOnCompletion asks); Files > Upload/Download (the active side's, in the background, and deleting; Copy and Move in a local tab), New > Link, Locking (Lock, Unlock where the protocol locks: WebDAV; only its disabled state is tested, the suite's WebDAV server does not lock, see P3-18); Tabs > Color (WinSCP's standard session colors, Default and More Colors: the tab's swatch and the remote panel's background, kept when the session is saved as a site) and New Tab > Default to Remote Tab (the New Tab button's choice, Ctrl reversing it); Local > Change Drive (Alt+F1: documents, desktop, the drives or, on POSIX, the root, home and mounted volumes) and Remote > Change Directory (Alt+F2: the path's directories), as the path boxes' drop-downs; the right side's Go To > Explore Directory (a local tab's); the command line (Options > Command Line, Shift+Ctrl+M: the remote side's commands in the console, a local side's started in its directory, each side's history kept as WinSCP's History\\Commands and LocalCommands; Files > File Names > Insert to Command Line, Ctrl+Enter). Histories are now kept as WinSCP keeps them (History\\<name>), the command line's the first; Queue > File List (a parallel transfer's files beside the queue, those done grayed); Toolbars > Update Button (Check for Updates, its menu the project's pages) and Hot Keys Toolbar (F2 to F10, at the bottom). Left/Right > View > Details and Thumbnails (a grid over the same entries and selection: local images previewed in the background, remote ones downloaded first by a background queue item of their own, as TThumbnailDownloadQueueItem, WinSCP's image masks up to 50 MB; kept as each panel's ViewStyle). Not offered: Help > Show Tips (WinSCP's tips service) and Donate |
| P3-16 | Verified UI gaps | done | the audit below, every gap closed but extensions (deferred, 2026-10-15) |
| P3-17 | Icon sets | done | Lucide (ISC), Tabler (MIT), Fluent UI (MIT) imported to `assets/icons/` (`port/tools/import_icons.py`, 62 roles after WinSCP's glyphs), embedded and drawn by `port/qt/IconSet.cpp` in the palette's text colour (light and dark themes); Preferences > Window > Icon set (Fluent by default on Windows, Lucide elsewhere; the desktop's theme on POSIX only; none), on the menus and the panels' toolbars; About credits each set with its license; Qt Svg added on both platforms (`qt6-svg-dev`, MSYS2 `qt6-svg` in `msys2-qt.lock`); ctest `qt_gui_icons`; native on Windows 11 too. WinSCP's toolbars: the main bands (Commands, Preferences with the queue's, Transfer Settings with the presets, and hidden by default Sessions and Tabs, Sort, Custom Commands) and each panel's five (Path, History, Navigation, Files, Selection; PanelBands.h), Options > Toolbars (each band, lock, selective text labels, icon size) and Left/Right Panel, kept as WinSCP's `ToolbarsLayout2`, `LockToolbars`, `SelectiveToolbarText`, `LargerToolbar`; ctest `qt_gui_toolbars` |
| P3-18 | Test devcontainer with real servers | done | `port/tests/servers/` (Containerfile, entrypoint.sh): OpenSSH, vsftpd (FTP, FTPES, implicit FTPS), Apache's mod_dav (WebDAV, HTTPS, locking), versitygw (S3), one container per test on the host's network serving the test's directory (fixtures.servers); ctest label `real-servers`: protocols_real (every protocol through linscp-cli), qt_session_ftp_real, qt_gui_properties_real, qt_gui_tags_real, qt_gui_commands_real, qt_gui_locking_real, native and under Wine. They found: S3 requests carrying an empty, unsigned x-amz-security-token (the shim's data() of an empty string, fixed), Properties failing on servers without object ACLs and on prefix-only folders (core patches). CI skips them: its Debian container has no container engine |
| P3-19 | The remaining Preferences pages | done | done 2026-10-15. Environment: the confirmations (transfers, with the Copy dialog's "Do not show this dialog box again"; drags', Auto until that is used; overwriting, resuming; deleting and moving to the trash, with "Never ask me again"; closing sessions, with WinSCP's workspace question; exiting on completion; the shell session; the synchronization summary; continue on error, errors collected into WinSCP's list) and the notifications (the beep after so many seconds; tray balloons, else the status bar; the taskbar flashed while elsewhere). Interface: the Commander (Explorer shown, not available) and the theme. Remote: inaccessible directories hidden in the panel and its tree, the refresh after operations left to core (Ctrl+Alt+R), the periodic refresh. Local: the local directory kept across tabs, deleting to the trash. Drag & Drop: the temporary folder and its free-space warning. Updates: the period checked at start, the result on startup or noted, beta versions, the proxy. ctests `qt_gui_preferences`, `qt_gui_environment`, `qt_gui_options`; Wine and the VM too. Not offered: the system context menu (the Windows shell's), the updates' e-mail and usage statistics |
| P3-20 | Custom command extensions | done | done 2026-10-15. WinSCP's TCustomCommandType (sliced) loads them from the three folders, with their order, deletions and shortcuts kept; Preferences > Commands lists them after the custom commands: Add's split button (a custom command or an extension, from a file or a URL, winscp.net asked as WinSCP asks it, untrusted ones confirmed), Configure (the extension's options dialog, `ExtensionOptionsDialog`, and its keyboard shortcut), Remove, Up, Down; the menus and the Custom Commands toolbar; run with their options expanded, those asked on each run first and kept, per site where they say so; `@require` checked. The Windows packages' `dotnet` folder (WinSCP's .NET assembly, `linscp-cli` as `WinSCP.exe`), `WINSCP_PATH` set. ctest `qt_gui_extensions` (a local extension; on Windows a PowerShell one listing the server over the assembly); Linux, Wine and the VM |
| P3-21 | Explorer interface | todo | |
| P3-22 | Jump list | done | done 2026-10-15: the recent sites (on connecting) and workspaces (on opening), newest first, at most 30, under WinSCP's keys (`JumpList`, `JumpListWorkspaces`), dropped when deleted or removed from the taskbar's list; shown as the taskbar's Recent Sites and Recent Workspaces (ICustomDestinationList), each a link with `/JumpList` (a site's with `/UploadIfAny`), shown again at start. ctest `qt_gui_preferences` (on Windows 11, the link in the program's `.customDestinations-ms`) |
| P3-23 | System context menu | done | done 2026-10-15: the Windows shell's context menu of the selected local files (as WinSCP's ShellDisplayContextMenu: IContextMenu from the folder's IShellFolder, IContextMenu2 drawing Open With and Send To through a callback window, Shift for extended verbs, a rename done in place by the panel), on a right click with Panels > Local's "Use system file context menu" (shown on Windows only) and from the local file menu's System Menu (with Properties, now there for local files too). ctest `qt_gui_environment` (Windows 11's own items) |
| P3-24 | Drag & drop shell extension | done | done 2026-10-15: WinSCP's DragExt built from the pristine source (`port/dragext`, a wrapper for mingw's GUID operators) as `DragExt64.dll`, in the deploy folder, registered by the installer unless WinSCP's own copy is; remote files dragged as WinSCP's fake file (a hidden `scp…` directory, the shared WinSCPDragExtMapping), downloaded where the copy hook says the drop was, under drag & drop's confirmation; the temporary folder's way when the extension is not there; Drag & Drop's fake-file option and status (installed, loaded, broken builds) on Windows. ctest `qt_gui_environment` (on Windows 11, the shell's copy stopped by the real hook and the file downloaded there) |

### P4 — Cleanup
| ID | Task | Status | Note |
|---|---|---|---|
| P4-01 | Retire VCL frontend | dropped | for now (decision 2026-10-15); may be reopened |
| P4-02 | Collapse dual-target `#ifdef`s | dropped | for now (decision 2026-10-15); may be reopened |
| P4-03 | Fold generated tree into source | dropped | for now (decision 2026-10-15); optional, one-way door |
| P4-04 | De-shimming evaluation | dropped | for now (decision 2026-10-15); recommendation was: do not |
