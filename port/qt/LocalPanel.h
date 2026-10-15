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

#include "PanelBands.h"

#include <functional>

#include "FileList.h"

class QAction;
class QFileSystemWatcher;
class QAbstractItemView;
class QLabel;
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
  // Local > Change Drive (Alt+F1), as the path box's drop-down
  // (LocalPathComboUpdateDrives): the documents and desktop folders, then
  // the drives (elsewhere the file system's root, the home directory and
  // the mounted volumes), the current one checked; one chosen is changed to.
  // PathMenu builds it (tests), ShowPathMenu shows it under the path box.
  class QMenu * PathMenu();
  void ShowPathMenu();
  bool Back();
  bool Forward();
  bool Parent();
  bool Root();
  bool Home();
  bool Refresh();
  bool NewFolder(const QString & Name);
  // F7: the Create folder dialog, then the folder.
  bool NewFolderDialog();
  bool Rename(const QString & Name, const QString & NewName);
  // The selected entries, to the trash (or for good where there is no
  // trash), after core's confirmation unless not asked.
  // Alternative: the other way (for good when deleting to the trash, and
  // the other way round), as WinSCP's Shift+Del.
  bool DeleteSelected(bool Confirm = true, bool Alternative = false);
  // As RemotePanel's: entries into one of the directory's directories.
  bool MoveInto(const QStringList & Names, const QString & Directory);
  // A directory is entered; a file opened with its application.
  void Open(const QString & Name);
  // Where a file is opened: the system's application by default.
  std::function<void(const QString & FileName)> OpenFile;
  // Whether the trash may be used: deletes go there as Panels > Local's
  // "Delete files to recycle bin" has it (its default), else for good.
  bool DeleteToTrash = true;
  // Where Home goes, when not the user's home: the site's local directory
  // with "default directory is home", as WinSCP's (TDirView.HomeDirectory).
  QString HomeDirectory;

  void SetShowHidden(bool Show);
  // What the Preferences set of a panel: hidden files, the sizes' format,
  // sorting, renaming, and Explorer's refresh key (F5) for the Commander's
  // (Ctrl+R).
  void ApplyPreferences(const CoreBridge::TPreferences & Preferences);
  // The columns' layout in WinSCP's own terms (DirViewParams; this panel's
  // columns mapped onto WinSCP's local panel's six).
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
  // The entries shown (filtered, hidden files as the setting says), in the
  // panel's order; and a directory's size, calculated, shown.
  std::vector<CoreBridge::TRemoteEntry> Entries() const;
  // Its focused entry, selection and scroll (PreservePanelState's), and
  // them back, as far as the entries still are.
  TPanelState State() const;
  void RestoreState(const TPanelState & State);
  void SetDirectorySize(const QString & Name, std::int64_t Size);
  QStringList Names() const;
  // The directory shown, with '/' separators (Qt's), as the panel works in it.
  QString Path() const { return FCurrent; }
  // The directories visited: back, this one, forward (native separators).
  QStringList History() const;
  FileView * View() const { return FView; }
  // View > Details and Thumbnails (ViewStyle: dvsReport, dvsThumbnail):
  // the file list, or a grid of thumbnails over the same entries and
  // selection. FocusView: whichever is shown.
  void SetThumbnails(bool Thumbnails);
  bool Thumbnails() const;
  ThumbnailView * Thumbs() const { return FThumbs; }
  QAbstractItemView * FocusView() const;
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
  class LocalDirectoryTree * Tree() const { return FTree; }

signals:
  void Changed();

private:
  LocalDirectoryTree * FTree = nullptr;
  class QSplitter * FSplitter = nullptr;
  void ShowTreePath();
  bool Go(const QString & Directory, bool Record);
  void Reload();
  void Fail(const QString & Message);
  void Activated(const QModelIndex & Index);
  void ContextMenu(const QPoint & Pos);
  void UpdateActions();

  FileListModel * FModel = nullptr;
  DirectoriesFirst * FSort = nullptr;
  FileView * FView = nullptr;
  ThumbnailView * FThumbs = nullptr;
  QLineEdit * FPath = nullptr;
  QLabel * FStatus = nullptr;
  QFileSystemWatcher * FWatcher = nullptr;
  QTimer * FReloadTimer = nullptr;
  QString FCurrent;
  QStringList FBack;
  QStringList FForward;
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
