// The local panel: a local directory as WinSCP's local pane shows it
// (packages/filemng/DirView.pas) -- the same columns and order as the remote
// panel (FileList), the path to type into, back and forward, parent, root,
// home, refresh -- kept up to date as the directory changes, and the local
// operations: new folder, rename, delete (to the trash, as WinSCP's default
// "delete to recycle bin"), opening a file with its application.
#ifndef LINSCP_QT_LOCAL_PANEL_H
#define LINSCP_QT_LOCAL_PANEL_H

#include <QStringList>
#include <QWidget>

#include <functional>

#include "FileList.h"

class QAction;
class QFileSystemWatcher;
class QLineEdit;
class QTimer;
class QTreeView;

class LocalPanel : public QWidget
{
  Q_OBJECT

public:
  explicit LocalPanel(QWidget * Parent = nullptr);

  // As RemotePanel's: each reports its error itself, false when it did not
  // happen.
  bool ChangeDirectory(const QString & Directory);
  bool Back();
  bool Forward();
  bool Parent();
  bool Root();
  bool Home();
  bool Refresh();
  bool NewFolder(const QString & Name);
  bool Rename(const QString & Name, const QString & NewName);
  // The selected entries, to the trash (or for good where there is no
  // trash), after core's confirmation unless not asked.
  bool DeleteSelected(bool Confirm = true);
  // As RemotePanel's: entries into one of the directory's directories.
  bool MoveInto(const QStringList & Names, const QString & Directory);
  // A directory is entered; a file opened with its application.
  void Open(const QString & Name);
  // Where a file is opened: the system's application by default.
  std::function<void(const QString & FileName)> OpenFile;
  // WinSCP's "delete to recycle bin" (its default): to the trash, else for
  // good.
  bool DeleteToTrash = true;

  void SetShowHidden(bool Show);
  QStringList Selected() const;
  void Select(const QStringList & Names);
  QStringList Names() const;
  // The directory shown, with '/' separators (Qt's), as the panel works in it.
  QString Path() const { return FCurrent; }
  FileView * View() const { return FView; }
  QLineEdit * PathEdit() const { return FPath; }

signals:
  void Changed();

private:
  bool Go(const QString & Directory, bool Record);
  void Reload();
  void Fail(const QString & Message);
  void Activated(const QModelIndex & Index);
  void ContextMenu(const QPoint & Pos);
  void UpdateActions();

  FileListModel * FModel = nullptr;
  DirectoriesFirst * FSort = nullptr;
  FileView * FView = nullptr;
  QLineEdit * FPath = nullptr;
  QFileSystemWatcher * FWatcher = nullptr;
  QTimer * FReloadTimer = nullptr;
  QString FCurrent;
  QStringList FBack;
  QStringList FForward;
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
