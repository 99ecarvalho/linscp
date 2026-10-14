// The shell's main window, in WinSCP's Commander layout: the local panel
// and, beside it, the remote panel of the session opened from the site
// manager or a URL; transfers between them by keys, menu, or drag and drop;
// the transfer queue below.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include <QPointer>

#include <functional>
#include <memory>
#include <vector>

#include "CoreBridge.h"

class EditorWindow;
class QAction;
class QLineEdit;
class QMimeData;
class LocalPanel;
class QueuePanel;
class QTemporaryDir;
class QTimer;
class QUrl;
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
  bool KeepUpToDate();
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
  EditorWindow * EditCurrent();
  EditorWindow * EditRemote(const QString & Name);
  EditorWindow * EditLocal(const QString & FileName);
  // The editors open: the remote ones only, or all.
  std::vector<EditorWindow *> Editors(bool RemoteOnly = false) const;
  // Closes them, each asking to save a modified file, the remote ones'
  // uploads done here. False when one stayed open.
  bool CloseEditors(bool RemoteOnly);
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
  bool PreferencesDialog();
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
  RemotePanel * Panel() const { return FPanel; }
  LocalPanel * Local() const { return FLocal; }

private slots:
  void OpenUrlDialog();
  void Disconnect();
  void About();

protected:
  // Closing ends the session as Disconnect does, its directories kept.
  void closeEvent(QCloseEvent * Event) override;

private:
  bool Open(std::unique_ptr<CoreBridge::TSession> Session);
  // Loads it and shows it; null (and gone) when it could not be read.
  EditorWindow * ShowEditor(EditorWindow * Editor);
  bool EditUpload(CoreBridge::TEditedFile & File);
  // OpenDirectory's two dialogs; Switched: the other one asked for.
  bool OpenBookmark(bool AddBookmark, bool & Switched);
  bool OpenLocationProfile(bool AddBookmark, bool & Switched);
  bool GoLocal(const QString & Directory);
  bool EnsureShellSession(bool Shell);
  bool CopyDialog(bool FromLocal, const QStringList & Names, QString & Target, bool Move, bool * Background,
    CoreBridge::TTransferSettings & Settings);
  bool GoRemote(const QString & Directory);
  // Removes the closed editors' temporary directories, once the queue has
  // no uploads left for them (or there is no session).
  void CleanUpEdited();
  void ShowError(const CoreBridge::TCoreError & E);
  void UpdateActions();

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
  };
  std::vector<TEdited> FEdited;
  std::vector<std::string> FEditedCleanUp;
  // Closing the editors: their uploads here, not queued.
  bool FClosingEditors = false;
  QAction * FHiddenAction = nullptr;
  class QSplitter * FPanels = nullptr;
  QSplitter * FVertical = nullptr;
  // As GUIConfiguration's SynchronizeParams, for the session's lifetime.
  CoreBridge::TSyncParams FSyncParams;
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
