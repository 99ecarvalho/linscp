// The remote panel: a session's current directory as WinSCP's remote pane
// shows it (components/UnixDirView.cpp, the explorer's address bar and
// toolbars) -- sortable columns with directories first, the path to type
// into, back and forward, parent, root, home, refresh -- and the remote
// operations that are not transfers: new folder, rename, delete, with
// WinSCP's Commander keys (F2, F7, Del, Backspace, Enter, Ctrl+R; F5 and F6
// are the main window's transfers).
#ifndef LINSCP_QT_REMOTE_PANEL_H
#define LINSCP_QT_REMOTE_PANEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <QWidget>

#include "PanelBands.h"

#include <functional>
#include <vector>

#include "CoreBridge.h"
#include "FileList.h"

class QAction;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

class RemotePanel : public QWidget
{
  Q_OBJECT

public:
  explicit RemotePanel(QWidget * Parent = nullptr);

  // The session to show; null when there is none.
  void SetSession(CoreBridge::TSession * Session);
  CoreBridge::TSession * Session() const { return FSession; }

  // What the toolbar, keys and menu do, for tests and them alike. Each
  // reports core's error itself; false when it did not happen.
  bool ChangeDirectory(const QString & Directory);
  // Remote > Change Directory (Alt+F2), as the path box's drop-down
  // (UpdateRemotePathComboBox): the root and each directory down to the
  // current one, which is checked; one chosen is changed to. PathMenu builds
  // it (tests), ShowPathMenu shows it under the path box.
  class QMenu * PathMenu();
  void ShowPathMenu();
  bool Back();
  bool Forward();
  bool Parent();
  bool Root();
  bool Home();
  bool Refresh();
  // With the permissions the Create folder dialog sets, when given.
  bool NewFolder(const QString & Name, const CoreBridge::TNewDirectory * Settings = nullptr);
  // F7: the Create folder dialog, then the folder. False when cancelled or
  // not made.
  bool NewFolderDialog();
  bool Rename(const QString & Name, const QString & NewName);
  // The selected entries, after asking (core's confirmation) unless not.
  bool DeleteSelected(bool Confirm = true);
  // Entries of the directory into one of its directories ("..": the
  // parent), as a drop within the panel.
  bool MoveInto(const QStringList & Names, const QString & Directory);
  // Opens the entry: a directory is entered.
  void Open(const QString & Name);

  void SetShowHidden(bool Show);
  // What the Preferences set of a panel: hidden files, the sizes' format,
  // sorting, renaming, and Explorer's refresh key (F5) for the Commander's
  // (Ctrl+R).
  void ApplyPreferences(const CoreBridge::TPreferences & Preferences);
  // The columns' layout in WinSCP's own terms (DirViewParams; this panel's
  // columns mapped onto WinSCP's remote panel's nine).
  void SetColumnsParams(const QString & Params);
  QString ColumnsParams() const;
  // The filter (Ctrl+Alt+F), as WinSCP's panels': a mask only what matches
  // shows; "" none.
  void SetFilter(const QString & Mask);
  QString Filter() const;
  QStringList Selected() const;
  // The selection alone (Selected: else the focused entry, as commands take it).
  QStringList Selection() const;
  void Select(const QStringList & Names);
  // A file activated (double-click, Enter): the window's double-click action.
  std::function<void(const QString & Name)> OpenFile;
  // The entries shown (filtered, hidden files as the setting says), in the
  // panel's order; and a directory's size, calculated, shown.
  std::vector<CoreBridge::TRemoteEntry> Entries() const;
  // Its focused entry, selection and scroll (PreservePanelState's), and
  // them back, as far as the entries still are.
  TPanelState State() const;
  void RestoreState(const TPanelState & State);
  void SetDirectorySize(const QString & Name, std::int64_t Size);
  QStringList Names() const;
  QString Path() const;
  // The directories visited: back, this one, forward.
  QStringList History() const;
  FileView * View() const { return FView; }
  // The drag of the selection, for other programs too: the main window's
  // (RemoteFileDrag); without it, a drag for the panels only.
  std::function<QMimeData *(const TFileDrag & Drag)> MakeDrag;
  QLineEdit * PathEdit() const { return FPath; }
  QLabel * Status() const { return FStatus; }

  // Its own actions, which the window's context menus offer too.
  // The icons, of the set chosen (IconSet).
  void ApplyIcons();
  // WinSCP's toolbars above the panel (PanelBands.h).
  QToolBar * Band(TPanelBand Which) const { return FBands[Which]; }
  QAction * BackAction() const { return FBackAction; }
  QAction * ForwardAction() const { return FForwardAction; }
  QAction * ParentAction() const { return FParentAction; }
  QAction * RootAction() const { return FRootAction; }
  QAction * HomeAction() const { return FHomeAction; }
  QAction * RefreshAction() const { return FRefreshAction; }
  QAction * NewFolderAction() const { return FNewFolderAction; }
  QAction * RenameAction() const { return FRenameAction; }
  QAction * DeleteAction() const { return FDeleteAction; }
  // The right click, given to the window when set (WinSCP's file and
  // directory popups): OnItem when over a file, not the parent entry.
  std::function<void(const QPoint & Global, bool OnItem)> OnContextMenu;

  // The directory tree above the files (WinSCP's DriveView), and its
  // height (DriveViewHeight).
  void SetTreeVisible(bool Visible);
  bool TreeVisible() const;
  int TreeHeight() const;
  void SetTreeHeight(int Height);
  class RemoteDirectoryTree * Tree() const { return FTree; }

signals:
  // The directory or its contents changed.
  void Changed();

private:
  RemoteDirectoryTree * FTree = nullptr;
  class QSplitter * FSplitter = nullptr;
  void ShowTreePath();
  bool Run(const std::function<void()> & Operation, bool Record);
  void Reload();
  void ShowError(const CoreBridge::TCoreError & E);
  void Activated(const QModelIndex & Index);
  void ContextMenu(const QPoint & Pos);
  void UpdateActions();

  CoreBridge::TSession * FSession = nullptr;
  FileListModel * FModel = nullptr;
  DirectoriesFirst * FSort = nullptr;
  FileView * FView = nullptr;
  QLineEdit * FPath = nullptr;
  QLabel * FStatus = nullptr;
  // The directory shown: the path box may hold what is being typed.
  QString FCurrent;
  QStringList FBack;
  QStringList FForward;
  bool FBusy = false;
  bool FDefaultDirIsHome = true;
  TPanelBands FBands;
  QAction * FBackAction = nullptr;
  QAction * FForwardAction = nullptr;
  QAction * FParentAction = nullptr;
  QAction * FRootAction = nullptr;
  QAction * FHomeAction = nullptr;
  QAction * FRefreshAction = nullptr;
  QAction * FNewFolderAction = nullptr;
  QAction * FRenameAction = nullptr;
  QAction * FDeleteAction = nullptr;
};

#endif
