// The shell's main window, in WinSCP's Commander layout: the local panel
// and, beside it, the remote panel of the session opened from the site
// manager or a URL; transfers between them by keys, menu, or drag and drop;
// the transfer queue below.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QDateTime>
#include <QMainWindow>

#include <QPointer>

#include <functional>
#include <memory>
#include <vector>

#include "CoreBridge.h"
#include "PanelBands.h"

class EditorWindow;
class QAction;
class QLineEdit;
class QMimeData;
class LocalPanel;
class QueuePanel;
class QTemporaryDir;
class QTimer;
class QUrl;
class QMenu;
class FileView;
class QToolBar;
class RemotePanel;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);
  ~MainWindow() override;

  // Connects to Url, as File > Open URL does. False when it did not.
  bool OpenUrl(const QString & Url);
  // Connects to a site, saved or as edited in the site manager.
  bool OpenSite(const CoreBridge::TSite & Site);
  // File > Site Manager: false when it was closed without logging in.
  bool SiteManagerDialog();
  // As WinSCP starts: the session the command line names (a site or a URL,
  // CoreBridge::TSession::FromCommandLine), else, or when it says too
  // little or does not connect, the site manager.
  void StartUp();
  // Go To > Explore Directory (Ctrl+Alt+E): the local directory in the
  // system's file manager. False when it could not be opened.
  bool ExploreLocalDirectory();
  // What opens URLs instead of the desktop's (tests).
  void SetUrlOpener(std::function<bool(const QUrl &)> Opener) { FUrlOpener = std::move(Opener); }
  // Transfers between the panels, as WinSCP's Commander's F5 (copy) and F6
  // (move) from the active panel to the other's directory, after asking for
  // the target unless Confirm is false. False when nothing was transferred
  // in full.
  // Settings: this transfer's; null for the current ones.
  bool Upload(const QStringList & Names, const QString & Target, bool Move,
    const CoreBridge::TTransferSettings * Settings = nullptr);
  bool Download(const QStringList & Names, const QString & Target, bool Move,
    const CoreBridge::TTransferSettings * Settings = nullptr);
  // Background: through the transfer queue, as the Copy dialog's "transfer
  // in background" (with Confirm, the dialog's checkbox decides).
  bool TransferSelected(bool Move, bool Confirm = true, bool Background = false);
  // Names (or full paths, from the local side) to Target, as the above.
  bool Transfer(bool FromLocal, const QStringList & Names, QString Target, bool Move, bool Confirm, bool Background);
  // What is dropped on a panel (OnLocal: the local one), onto one of its
  // directories or (OnDirectory empty) the panel itself, as WinSCP's drag
  // and drop: from the other panel, transferred there after the Copy
  // dialog; within the panel, onto a directory, moved into it; files from
  // another program, onto the remote panel, uploaded. Move: dropped with
  // Shift. False when nothing was done in full.
  bool Drop(bool OnLocal, const QMimeData * Data, const QString & OnDirectory, bool Move, bool Confirm = true);
  // Commands > Synchronize, as WinSCP's Commander: the two panels'
  // directories, the direction from the active panel, after the Synchronize
  // dialog unless Confirm is false.
  bool Synchronize(bool Confirm = true);
  // Commands > Keep Remote Directory up to Date (Ctrl+U), as WinSCP's
  // Commander: the panels' directories in its dialog, which watches and
  // synchronizes until closed.
  // Given: its settings and directories (the command line's); AutoStart:
  // started at once.
  bool KeepUpToDate(const CoreBridge::TKeepUpToDateParams * Given = nullptr, bool AutoStart = false);
  // Commands > Open in PuTTY (Ctrl+P), as WinSCP's: the session in the
  // program Preferences name, PuTTY by default (on POSIX, if none, ssh in
  // the desktop's terminal). False when it did not start (the error shown).
  bool OpenInPutty();
  // The dialog, while open: for tests.
  class KeepUpToDateDialog * KeepUpToDateWindow() const { return FKeepUpToDate; }
  // Compares and, after the checklist when Params asks to preview, applies
  // (or queues). False when cancelled or nothing was applied.
  bool SynchronizeDirectories(const QString & LocalDirectory, const QString & RemoteDirectory,
    const CoreBridge::TSyncParams & Params, const CoreBridge::TTransferSettings * Settings = nullptr);
  // Files > Edit (F4), as WinSCP's internal editor: the active panel's
  // focused file in an editor window of its own. A remote one is
  // downloaded to a temporary directory first, uploaded back each time it
  // is saved, and its directory removed once the editor is closed and its
  // uploads are done (ADR 0015). Null when no editor opened; an editor
  // already open on the file is brought up instead.
  //
  // The editor is the first of Preferences > Editors whose mask matches the
  // file (TWinConfiguration::DefaultEditorForFile), else the internal one.
  // An external one, or the associated application, gets a remote file's
  // temporary copy, uploaded back whenever its time changes, until the
  // session closes (TEditorManager's files with no process); a local file
  // as it is. Null then too.
  EditorWindow * EditCurrent();
  // Files > Edit With: the given editor (its index in Preferences >
  // Editors), or (-1) one asked for (Edit With..., TEditorPreferencesDialog
  // ad hoc, which may remember it). Files > Open: the associated
  // application (efShell). False when nothing was opened.
  bool EditCurrentWith(int Editor);
  bool OpenCurrent();
  // A panel's right click, as WinSCP's popups (NonVisual.dfm's
  // RemoteFilePopup, LocalFilePopup, RemoteDirViewPopup, LocalDirViewPopup):
  // over a file, what can be done to the selection; elsewhere, the
  // directory's. The menu is returned unshown when Show is false (tests).
  QMenu * ContextMenu(bool Local, bool OnItem, const QPoint & Global, bool Show = true);
  // File Names > Copy to Clipboard: the selected names, one per line, with
  // their full paths if asked (FileListToClipboard, FullFileListToClipboard).
  void CopyFileNames(bool Local, bool Paths);
  // As CustomExecuteFile's efExternalEditor and efShell, on a file of the
  // active panel.
  bool EditExternally(const CoreBridge::TEditorDefinition & Editor, bool Local, const QString & Name);
  // The remote files open externally: checked, and changed ones uploaded
  // (the timer's job; for tests).
  void CheckExternalEdited();
  int ExternalEditedCount() const { return static_cast<int>(FExternalEdited.size()); }
  // New: a file not on the server, made empty (NewFile).
  EditorWindow * EditRemote(const QString & Name, bool New = false);
  EditorWindow * EditLocal(const QString & FileName);
  // The editors open: the remote ones only, or all.
  std::vector<EditorWindow *> Editors(bool RemoteOnly = false) const;
  // Closes them, each asking to save a modified file, the remote ones'
  // uploads done here. False when one stayed open.
  // Session: only that session's.
  bool CloseEditors(bool RemoteOnly, const CoreBridge::TSession * Session = nullptr);
  // Files > Properties (F9), as WinSCP's: the remote panel's selection (or
  // its focused entry) in the Properties dialog, its changes applied. False
  // when nothing was changed. Local files' properties are the system's.
  bool Properties();
  // Files > Edit Link (Alt+F6), as WinSCP's AddEditLink: a link in the
  // active panel's directory, to its focused entry, or that entry edited
  // when it is a link. Locally, the system's (a symbolic link, a shortcut
  // on Windows). False when none was made.
  bool AddEditLink();
  // Files > Duplicate and Move To (Shift+F5, Shift+F6), as WinSCP's: the
  // remote panel's selection within the server, after its dialog. False
  // when nothing was done in full.
  bool RemoteTransfer(bool Move);
  // Open Directory/Bookmark (Ctrl+O) and Add Path to Bookmarks (Ctrl+B),
  // as WinSCP's DoOpenDirectoryDialog for the active panel: the directory
  // chosen opened, the dialog again when it cannot be. False when none was.
  bool OpenDirectory(bool AddBookmark = false);
  // A shared bookmark's keyboard shortcut pressed: its directory opened on
  // the active panel's side. False when none has it, or no directory there.
  bool OpenBookmarkShortCut(unsigned ShortCut);
  // Find Files (Alt+F7, F3 with Explorer's keys): WinSCP's dialog, beside
  // the window. Its search and actions: FindFiles searches with the
  // window busy (false when it failed, its error shown); FocusRemote opens
  // the file's directory in the remote panel and focuses it; the found
  // files downloaded (after the Copy dialog) and deleted (after core's
  // confirmation).
  void FindFilesDialog();
  bool FindFiles(const QString & Directory, const QString & Mask,
    const std::function<bool(const std::string & Directory, const CoreBridge::TRemoteEntry & Entry)> & Found,
    const std::function<bool(const std::string & Directory)> & Searching);
  bool FocusRemote(const QString & Directory, const QString & Name);
  bool DownloadFound(const std::vector<int> & Indexes, const QStringList & Names);
  bool DeleteFound(const std::vector<int> & Indexes, const QStringList & Names);
  // View > Filter (Ctrl+Alt+F), as WinSCP's: the active panel shows only
  // what matches the mask asked for (normalized: "*.*" is none). False
  // when cancelled.
  bool Filter();
  // Commands > Server/Protocol Information, as WinSCP's.
  void ServerInfo();
  // Commands > Generate Session URL/Code, Files > Generate File URL (the
  // remote panel's selection), as WinSCP's.
  void GenerateUrl(bool Files);
  // Commands > Open Terminal (Shift+Ctrl+T), as WinSCP's OpenConsole:
  // where the protocol runs no commands, a separate shell session opened
  // first, after asking. False when the console did not open.
  bool OpenConsole();
  // A custom command run on the remote panel's selection (or its focused
  // entry), as TCustomScpExplorerForm::CustomCommand: its parameters asked,
  // its output shown in the console, copied, or in a message as it says.
  // False when it did not run.
  bool RunCustomCommand(const CoreBridge::TCustomCommandDefinition & Command);
  // Enter...: an ad hoc command, from the Custom command dialog (Files: a
  // file command's), then run; kept as Last.
  bool EnterCustomCommand(bool Files);
  // The custom commands' menus and keys, from the list as it is now.
  void UpdateCustomCommands();
  void UpdateBookmarkShortCuts();
  CoreBridge::TCustomCommandPrompts CommandPrompts();
  class QMenu * FileCommandsMenu() const { return FFileCommandsMenu; }
  QMenu * StaticCommandsMenu() const { return FStaticCommandsMenu; }
  // Options > Preferences: false when cancelled.
  // Page: the one to open on (its title), else the first.
  bool PreferencesDialog(const QString & Page = QString());
  // The preferences applied to the window and its panels.
  void ApplyPreferences();
  // The layout kept between runs (WinSCP's): restored as the window is
  // made, saved as it closes.
  void RestoreLayout();
  void SaveLayout();
  // The title, as WinSCP's: the path (Preferences, Window), the session,
  // the program.
  void UpdateTitle();
  // Polls the session's queue: QueueTimer's, and tests'.
  void QueueIdle();
  QueuePanel * Queue() const { return FQueue; }
  // Remote drags downloaded for other programs: for tests.
  int DragOuts() const { return FDragOuts; }
  // The panel transfers start from: the one the focus last went to.
  void ActivatePanel(bool Local);
  bool LocalActive() const { return FLocalActive; }
  CoreBridge::TSession * Session() const { return FSession.get(); }
  // What runs local custom commands' programs.
  static CoreBridge::TSession::TLocalRunner LocalRunner();
  // As TCustomScpExplorerForm::CommandLineFromAnotherInstance: the session a
  // command line given to another instance names, opened here, unless the
  // window is busy (false: that instance opens it itself).
  bool CommandLineFromAnotherInstance(const QStringList & Arguments);
  // As ApplicationMinimize and ApplicationRestore with MinimizeToTray: the
  // window hidden, the tray's icon (its tip the window's title) bringing
  // it back. Where the desktop has no tray, minimized as ever.
  void MinimizeToTray();
  // As ApplicationMinimize (Keep Up To Date's Minimize): to the tray when
  // asked, or Preferences say so.
  void MinimizeApplication(bool ToTray);
  void RestoreFromTray();
  class QSystemTrayIcon * TrayIcon() const { return FTrayIcon; }
  QAction * ProcessQueueAction() const { return FProcessQueueAction; }
  // Session tabs, as WinSCP's (TTerminalManager's sessions, its
  // SessionsPageControl): opening a session adds one, Disconnect closes the
  // active one. The active tab's session is Session().
  int TabCount() const { return static_cast<int>(FTabs.size()); }
  int ActiveTab() const { return FActiveTab; }
  CoreBridge::TSession * TabSession(int Index) const;
  // False when busy, or no such tab.
  bool SwitchTab(int Index);
  // The tab's remote editors closed (each asking to save), its session
  // disconnected, the next tab then active. False when an editor stayed.
  bool CloseTab(int Index);
  // Ctrl+Tab and Ctrl+Shift+Tab.
  void CycleTabs(bool Forward);
  // Tabs > Duplicate Tab: the active session's site, as it is now, in a new
  // tab (TCustomScpExplorerForm::DuplicateTab).
  bool DuplicateTab();
  // Tabs > New Tab > Local Tab, as WinSCP's local browser (NewLocalTab): a
  // tab with no session and a local directory in each panel, both on the
  // local panel's to start with. Copy and Move (F5, F6) and drags between
  // its panels copy and move local files (LocalLocalCopy).
  // Local and Other: its panels' directories (a workspace's), else the
  // local panel's in both.
  void NewLocalTab(const QString & Local = QString(), const QString & Other = QString());
  // The Tabs and Commands menus' session commands, as WinSCP's (P3-14):
  // Reconnect Session (a session that lost its connection), Save Session
  // as Site (Name given: no dialog), Rename Tab (Name given: no prompt),
  // Change Password, Install Public Key into Server (FileName given: no
  // file dialog), Clear Caches. False when nothing was done.
  bool ReconnectSession();
  bool SaveSessionAsSite(const QString * Name = nullptr, bool SavePassword = false);
  bool RenameTab(const QString * Name = nullptr);
  bool ChangePassword();
  bool InstallPublicKey(const QString * FileName = nullptr);
  bool ClearCaches();
  // A file double-clicked (or Enter) in a panel, as DoDirViewExecFile:
  // opened, copied to the other panel, or edited, as Preferences say.
  bool FileActivated(bool Local);
  // Commands > Compare Directories (Shift+F2), as TScpCommanderForm's: in
  // each panel, the files newer than the other panel's of the name, or
  // missing there (and, by Preferences, of another size), selected; WinSCP's
  // message when there are none. False then.
  bool CompareDirectories();
  // Commands > Synchronize Browsing (Ctrl+Alt+B): a panel's change of
  // directory followed by the other, relative to where both were
  // (SynchronizeBrowsingLocal and ...Remote); a directory that is not there
  // created if the user wants, else browsing synchronization turned off.
  void SetSynchronizeBrowsing(bool On);
  bool SynchronizeBrowsing() const;
  // Files > Calculate Directory Sizes (Shift+Alt+Enter): the selected
  // directories' sizes, shown in the Size column.
  bool CalculateDirectorySizes();
  // Go To > Path from Other Panel (Ctrl+.): in a local tab, the panel to the
  // other's directory.
  bool PathFromOtherPanel(bool Local);
  // Files > New > File, as EditNew: a file of the name asked for (the
  // focused file's, else "New file"; given: no prompt), edited -- an
  // existing one as it is, a new one empty, uploaded when saved.
  EditorWindow * NewFile(const QString * Name = nullptr);
  // Files > Copy to Clipboard (Ctrl+C), Paste from Clipboard (Ctrl+V), as
  // WinSCP's: the active panel's selection, as its drag has it (remote
  // files by their full paths, downloaded as another program pastes them);
  // pasted, files are uploaded to the remote panel, copied to a local one,
  // or downloaded from the session they were copied from; a line of text is
  // a session URL to open, else a path to go to. Pasting remote files into
  // the remote panel (WinSCP's duplicate) is not done.
  bool CopyToClipboard();
  bool PasteFromClipboard();
  // Options > Status Bar and Options > Queue (show, hide when empty, hide),
  // kept as WinSCP keeps them.
  void SetStatusBarShown(bool Shown);
  void SetQueueShow(CoreBridge::TLayout::TQueueShow Show);
  bool LocalBrowser() const;
  // The panel on a side as a local one: the left (Local) panel, or the
  // right one in a local tab; null for the remote panel.
  LocalPanel * SideLocal(bool Local) const;
  LocalPanel * OtherLocal() const { return FOtherLocal; }
  // The active local panel's directory (the left one's, when the remote is active).
  QString ActiveLocalPath() const;
  LocalPanel * ActiveLocal() const;
  // As TScpCommanderForm::LocalLocalCopy: the active panel's selection (or
  // the given full paths) to the other panel's directory, after the Copy
  // or Move dialog unless Confirm is false (then Target, else the other
  // panel's, with Mask). False when cancelled or something failed.
  bool LocalLocalCopy(bool Move, bool Confirm = true, const QStringList * Sources = nullptr,
    const QString * Target = nullptr, const QString & Mask = QStringLiteral("*.*"));
  // Tabs > Save Workspace (TCustomScpExplorerForm::SaveWorkspace): the
  // tabs' sessions saved together, after its dialog, unless Name is given
  // (tests). Opening one (the site manager's, or at start): each of its
  // sessions in a tab. False when nothing was saved or opened.
  bool SaveWorkspace(const QString * Name = nullptr, bool SavePasswords = false, bool AutoSave = false);
  bool OpenWorkspace(const QString & Name);
  // As WorkspaceName: the last saved, else the automatic one, else "My
  // Workspace".
  QString WorkspaceName() const;
  class QTabBar * TabBar() const { return FTabBar; }
  RemotePanel * Panel() const { return FPanel; }
  LocalPanel * Local() const { return FLocal; }

private slots:
  void OpenUrlDialog();
  void Disconnect();
  void About();

protected:
  // Closing ends the session as Disconnect does, its directories kept.
  void closeEvent(QCloseEvent * Event) override;
  // Minimized: to the tray, as Preferences > Window says.
  void changeEvent(QEvent * Event) override;

private:
  bool Open(std::unique_ptr<CoreBridge::TSession> Session);
  // Loads it and shows it; null (and gone) when it could not be read.
  EditorWindow * ShowEditor(EditorWindow * Editor);
  bool EditUpload(CoreBridge::TSession * Session, CoreBridge::TEditedFile & File);
  // OpenDirectory's two dialogs; Switched: the other one asked for.
  bool OpenBookmark(bool AddBookmark, bool & Switched);
  bool OpenLocationProfile(bool AddBookmark, bool & Switched);
  bool GoLocal(const QString & Directory);
  bool EnsureShellSession(bool Shell);
  bool EnsureCommandSession(bool Capable);
  bool CopyDialog(bool FromLocal, const QStringList & Names, QString & Target, bool Move, bool * Background,
    CoreBridge::TTransferSettings & Settings);
  bool GoRemote(const QString & Directory);
  // Removes the closed editors' temporary directories, once the queue has
  // no uploads left for them (or there is no session).
  void CleanUpEdited();
  void ShowError(const CoreBridge::TCoreError & E);
  void UpdateActions();
  void TransferPresetAutoSelect();
  bool RunLocalCustomCommand(const CoreBridge::TCustomCommandDefinition & Command);
  void StandaloneUpload(const std::vector<std::string> & Files, bool UseDefaults);
  void StandaloneKeepUpToDate(const CoreBridge::TKeepUpToDateParams & Params, bool UseDefaults);
  CoreBridge::TTransferPresetRule TransferPresetAutoSelectData() const;

  CoreBridge::TInfo FInfo;
  std::unique_ptr<CoreBridge::TSession> FSession;
  bool FBusy = false;
  QAction * FOpenAction = nullptr;
  QAction * FSitesAction = nullptr;
  bool FLocalActive = false;
  QAction * FCopyAction = nullptr;
  QAction * FMoveAction = nullptr;
  QAction * FSynchronizeAction = nullptr;
  QAction * FKeepUpToDateAction = nullptr;
  QAction * FPuttyAction = nullptr;
  QAction * FDuplicateTabAction = nullptr;
  QAction * FSaveWorkspaceAction = nullptr;
  QAction * FQuitAction = nullptr;
  QAction * FRenameTabAction = nullptr;
  QAction * FReconnectAction = nullptr;
  QAction * FSaveSiteAction = nullptr;
  QAction * FChangePasswordAction = nullptr;
  QAction * FPublicKeyAction = nullptr;
  QAction * FClearCachesAction = nullptr;
  QAction * FCompareAction = nullptr;
  QAction * FSyncBrowseAction = nullptr;
  QAction * FDirectorySizesAction = nullptr;
  QAction * FNewFileAction = nullptr;
  QAction * FProcessQueueAction = nullptr;
  QSystemTrayIcon * FTrayIcon = nullptr;
  QList<QPointer<QWidget>> FTrayHidden;
  // TransferPresetAutoSelect's: the preset chosen by hand, the one last
  // autoselected.
  QString FCopyParamDefault;
  QString FCopyParamAutoSelected;
  // /upload's run (StandaloneUpload).
  bool FStandalone = false;
  std::function<bool(const QStringList &)> FNewInstanceStarter;
public:
  // What starts another instance (Start in New Window's), instead of the
  // program itself (tests).
  void SetNewInstanceStarter(std::function<bool(const QStringList &)> Starter) { FNewInstanceStarter = std::move(Starter); }
private:
  QAction * FCopyClipboardAction = nullptr;
  QAction * FPasteAction = nullptr;
  QAction * FStatusBarAction = nullptr;
  QAction * FQueueShowActions[3] = {};
  // The session remote files were copied to the clipboard from.
  CoreBridge::TSession * FClipboardSession = nullptr;
  // What this window put on the clipboard for remote files.
  const QMimeData * FClipboardData = nullptr;
  void UpdateQueueView();
  // Synchronized browsing: each side's directory as last loaded (local:
  // [1]), and while following the other.
  QString FPrevPath[2];
  bool FSynchronisingBrowse = false;
  void PanelLoaded(bool Local);
  bool SyncBrowse(bool Local, const QString & PrevPath, QString & NewPath, bool Create);
  QAction * FAboutAction = nullptr;
  QAction * FOpenCurrentAction = nullptr;
  QAction * FExploreAction = nullptr;
  QAction * FPreferencesAction = nullptr;
  LocalPanel * FOtherLocal = nullptr;
  QMenu * FLocalMenu = nullptr;
  QMenu * FRemoteMenu = nullptr;
  // The menu bar, as WinSCP's Commander's (forms/ScpCommander.dfm).
  void BuildMenus();
  // The actions' icons by role (IconSet), applied again as the set changes.
  std::vector<std::pair<QAction *, QString>> FIconRoles;
  QAction * Iconize(QAction * Action, const char * Role);
  void ApplyIcons();
  // WinSCP's toolbars (P3-16): the main window's bands (Commands, Sessions
  // and Tabs, Preferences, Sort, Transfer Settings, Custom Commands) and the
  // panels' (PanelBands.h), each shown or hidden as Options > Toolbars and
  // Left/Right Panel say, kept as WinSCP's ToolbarsLayout; locked,
  // selective text labels, the icons' size as its Options > Toolbars.
  struct TBand
  {
    QString Name;
    QToolBar * Bar = nullptr;
    QAction * Toggle = nullptr;
    // A panel's: both local panels' bands follow the left one's toggle.
    QToolBar * Twin = nullptr;
  };
  std::vector<TBand> FBands;
  std::vector<std::pair<QToolBar *, QAction *>> FTextButtons;
  QAction * FLockToolbarsAction = nullptr;
  QAction * FSelectiveTextAction = nullptr;
  QAction * FIconSizeActions[3] = {};
  class QComboBox * FPresetCombo = nullptr;
  QToolBar * FCustomCommandsBar = nullptr;
  QAction * FLeftCopyAction = nullptr;
  QAction * FLeftMoveAction = nullptr;
  QAction * FSortActions[7] = {};
  void BuildToolbars(QMenu * Options);
  QToolBar * MainBand(const QString & Name, const QString & Title);
  QAction * SideAction(bool Local, QAction * Source, const QString & Text, const char * Role);
  void BandAction(QToolBar * Bar, QAction * Action, bool Text = false);
  void ApplyToolbarSettings();
  void SaveToolbarSettings();
  void UpdatePresetCombo();
  void UpdateSortActions();
  // The Open Directory button's menu (CreateOpenDirMenu): the side's
  // bookmarks, the site's and the shared ones.
  void FillOpenDirMenu(QMenu * Menu, bool Left, QAction * OpenDirectory);
  QMenu * PanelMenu(bool Local);
  FileView * ActiveView() const;
  void SelectSameExtension(bool Select);
  QAction * FCloseTabAction = nullptr;
  QAction * FNextTabAction = nullptr;
  QAction * FPreviousTabAction = nullptr;
  std::function<bool(const QUrl &)> FUrlOpener;
  std::vector<QAction *> FBookmarkActions;
  class KeepUpToDateDialog * FKeepUpToDate = nullptr;
  QAction * FEditAction = nullptr;
  QAction * FPropertiesAction = nullptr;
  QAction * FLinkAction = nullptr;
  QAction * FDuplicateAction = nullptr;
  QAction * FOpenDirectoryAction = nullptr;
  QAction * FFindFilesAction = nullptr;
  QAction * FFilterAction = nullptr;
  QAction * FServerInfoAction = nullptr;
  QAction * FConsoleAction = nullptr;
  class QMenu * FFileCommandsMenu = nullptr;
  QMenu * FStaticCommandsMenu = nullptr;
  // Enter...'s last command, for Last.
  bool FHasLastCustomCommand = false;
  CoreBridge::TCustomCommandDefinition FLastCustomCommand;
  QAction * FGenerateSessionUrlAction = nullptr;
  QAction * FGenerateFileUrlAction = nullptr;
  QAction * FAddBookmarkAction = nullptr;
  QAction * FMoveToAction = nullptr;
  // The editors, and for a remote file what it was downloaded as.
  struct TEdited
  {
    QPointer<EditorWindow> Editor;
    std::shared_ptr<CoreBridge::TEditedFile> File;
    // The session a remote file is of.
    CoreBridge::TSession * Session = nullptr;
  };
  std::vector<TEdited> FEdited;
  // Remote files open in an external editor or their application, and their
  // time as last uploaded.
  struct TExternalEdited
  {
    std::shared_ptr<CoreBridge::TEditedFile> File;
    QDateTime Modified;
    CoreBridge::TSession * Session = nullptr;
  };
  // The tabs: the active one's session is FSession, the others' their own,
  // with each one's local directory and finished queue count.
  struct TTab
  {
    std::unique_ptr<CoreBridge::TSession> Session;
    QString LocalPath;
    int QueueDone = 0;
    // WinSCP's local browser: no session, the right panel local too.
    bool LocalBrowser = false;
    QString OtherLocalPath;
    // A local tab's name, as renamed (a session's is its own).
    QString Name;
    bool SynchronizeBrowsing = false;
    // The panels' state, kept with "remember panels' state" (P3-16).
    TPanelState LocalState;
    TPanelState RemoteState;
    TPanelState OtherLocalState;
  };
  // The right panel's place: the remote panel, or in a local tab the other
  // local one.
  void ShowLocalBrowser(bool Local);
  std::vector<TTab> FTabs;
  int FActiveTab = -1;
  QTabBar * FTabBar = nullptr;
  // WinConfiguration's LastWorkspace: this run's.
  QString FLastWorkspace;
  std::vector<CoreBridge::TSession::TWorkspaceTab> WorkspaceTabs() const;
  bool FUpdatingTabs = false;
  // The session parked into its tab, and the tab's back again.
  void ParkActiveTab();
  void UnparkTab(int Index);
  void UpdateTabs();
  // Whether Session is one of the tabs'.
  bool SessionOpen(const CoreBridge::TSession * Session) const;
  std::vector<TExternalEdited> FExternalEdited;
  QTimer * FExternalTimer = nullptr;
  QMenu * FEditWithMenu = nullptr;
  bool CurrentFile(bool & Local, QString & Name, std::int64_t & Size, double & ModifiedTime) const;
  std::vector<std::string> FEditedCleanUp;
  // Closing the editors: their uploads here, not queued.
  bool FClosingEditors = false;
  QAction * FHiddenAction = nullptr;
  class QSplitter * FPanels = nullptr;
  QSplitter * FVertical = nullptr;
  QAction * FDisconnectAction = nullptr;
  RemotePanel * FPanel = nullptr;
  LocalPanel * FLocal = nullptr;
  QueuePanel * FQueue = nullptr;
  QTimer * FQueueTimer = nullptr;
  std::vector<std::unique_ptr<QTemporaryDir>> FDragDirectories;
  int FDragOuts = 0;
  int FQueueDone = 0;
};

#endif
