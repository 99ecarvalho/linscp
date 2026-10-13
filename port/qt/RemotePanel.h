// The remote panel: a session's current directory as WinSCP's remote pane
// shows it (components/UnixDirView.cpp, the explorer's address bar and
// toolbars) -- sortable columns with directories first, the path to type
// into, back and forward, parent, root, home, refresh -- and the remote
// operations that are not transfers: new folder, rename, delete, with
// WinSCP's keys (F2, F5, F7, Del, Backspace, Enter).
#ifndef LINSCP_QT_REMOTE_PANEL_H
#define LINSCP_QT_REMOTE_PANEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <vector>

#include "CoreBridge.h"

class QAction;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

// The current directory's entries, with ".." first below the root.
class RemoteModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum { NameColumn, SizeColumn, ModifiedColumn, RightsColumn, OwnerColumn, ColumnCount };
  // What the sort proxy compares; directories first is the proxy's.
  enum { SortRole = Qt::UserRole, DirectoryRole, ParentRole };

  using QAbstractTableModel::QAbstractTableModel;
  void SetEntries(std::vector<CoreBridge::TRemoteEntry> Entries, bool WithParent);
  const CoreBridge::TRemoteEntry * Entry(int Row) const;
  bool IsParent(int Row) const { return FWithParent && (Row == 0); }

  int rowCount(const QModelIndex & Parent = QModelIndex()) const override;
  int columnCount(const QModelIndex & Parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & Index, int Role = Qt::DisplayRole) const override;
  QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;

private:
  std::vector<CoreBridge::TRemoteEntry> FEntries;
  bool FWithParent = false;
};

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
  // Opens the entry: a directory is entered.
  void Open(const QString & Name);

  QStringList Selected() const;
  void Select(const QStringList & Names);
  QStringList Names() const;
  QString Path() const;
  QTreeView * View() const { return FView; }
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
  RemoteModel * FModel = nullptr;
  QSortFilterProxyModel * FSort = nullptr;
  QTreeView * FView = nullptr;
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
