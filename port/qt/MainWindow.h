// The shell's main window, in WinSCP's Commander layout: the local panel
// and, beside it, the remote panel of the session opened from the site
// manager or a URL; transfers between them by keys, menu, or drag and drop;
// the transfer queue below.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include <QPointer>

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
  QAction * FEditAction = nullptr;
  QAction * FPropertiesAction = nullptr;
  QAction * FLinkAction = nullptr;
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
