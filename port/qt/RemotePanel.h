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

#include <functional>
#include <vector>

#include "CoreBridge.h"
#include "FileList.h"

class QAction;
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
  bool Back();
  bool Forward();
  bool Parent();
  bool Root();
  bool Home();
  bool Refresh();
  bool NewFolder(const QString & Name);
  bool Rename(const QString & Name, const QString & NewName);
  // The selected entries, after asking (core's confirmation) unless not.
  bool DeleteSelected(bool Confirm = true);
  // Entries of the directory into one of its directories ("..": the
  // parent), as a drop within the panel.
  bool MoveInto(const QStringList & Names, const QString & Directory);
  // Opens the entry: a directory is entered.
  void Open(const QString & Name);

  void SetShowHidden(bool Show);
  QStringList Selected() const;
  void Select(const QStringList & Names);
  QStringList Names() const;
  QString Path() const;
  FileView * View() const { return FView; }
  QLineEdit * PathEdit() const { return FPath; }

signals:
  // The directory or its contents changed.
  void Changed();

private:
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
  // The directory shown: the path box may hold what is being typed.
  QString FCurrent;
  QStringList FBack;
  QStringList FForward;
  bool FBusy = false;
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
