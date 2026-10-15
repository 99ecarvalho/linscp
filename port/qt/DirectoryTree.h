// The panels' directory trees (WinSCP's TUnixDriveView and TDriveView, shown
// above a Commander panel's file list): the remote one loads a folder's
// subfolders as it is expanded; the local one is the file system's, its
// drives at the top on Windows. Each follows the panel's directory, and
// takes it where a folder is chosen.
#ifndef LINSCP_QT_DIRECTORY_TREE_H
#define LINSCP_QT_DIRECTORY_TREE_H

#include <QTreeView>

#include "CoreBridge.h"

class QFileSystemModel;
class QStandardItem;
class QStandardItemModel;

class RemoteDirectoryTree : public QTreeView
{
  Q_OBJECT

public:
  explicit RemoteDirectoryTree(QWidget * Parent = nullptr);
  // A new session's: its root alone, folded.
  void SetSession(CoreBridge::TSession * Session);
  // The path's folders loaded and unfolded, it selected.
  void ShowPath(const QString & Path);
  // Everything read again (Refresh).
  void Reload();
  QString CurrentPath() const;

signals:
  void DirectoryChosen(const QString & Path);

private:
  void Load(QStandardItem * Item);
  QStandardItem * Child(QStandardItem * Parent, const QString & Name);
  QStandardItem * MakeItem(const QString & Name, const QString & Path);

  CoreBridge::TSession * FSession = nullptr;
  QStandardItemModel * FModel = nullptr;
  bool FShowing = false;
};

class LocalDirectoryTree : public QTreeView
{
  Q_OBJECT

public:
  explicit LocalDirectoryTree(QWidget * Parent = nullptr);
  void ShowPath(const QString & Path);
  void SetShowHidden(bool Show);
  QString CurrentPath() const;

signals:
  void DirectoryChosen(const QString & Path);

private:
  QFileSystemModel * FModel = nullptr;
  bool FShowing = false;
};

#endif
