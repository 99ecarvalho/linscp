// What both file panels list: a directory's entries -- core's for the remote
// panel, the file system's for the local one -- in WinSCP's columns, with
// ".." first below the root, sorted directories first (DirectoriesFirst) as
// WinSCP's panels sort.
#ifndef LINSCP_QT_FILE_LIST_H
#define LINSCP_QT_FILE_LIST_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QTreeView>

#include <functional>
#include <vector>

#include "CoreBridge.h"

// The parent directory's entry.
extern const char * const ParentEntryName;

class FileListModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum { NameColumn, SizeColumn, ModifiedColumn, RightsColumn, OwnerColumn, ColumnCount };
  // What the sort proxy compares; directories first is the proxy's.
  enum { SortRole = Qt::UserRole, DirectoryRole, ParentRole, HiddenRole };

  using QAbstractTableModel::QAbstractTableModel;
  void SetEntries(std::vector<CoreBridge::TRemoteEntry> Entries, bool WithParent);
  const CoreBridge::TRemoteEntry * Entry(int Row) const;
  bool IsParent(int Row) const { return FWithParent && (Row == 0); }
  // The rights column as file attributes (a Windows local panel's).
  void SetAttributes(bool Attributes) { FAttributes = Attributes; }

  int rowCount(const QModelIndex & Parent = QModelIndex()) const override;
  int columnCount(const QModelIndex & Parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & Index, int Role = Qt::DisplayRole) const override;
  QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;

private:
  std::vector<CoreBridge::TRemoteEntry> FEntries;
  bool FWithParent = false;
  bool FAttributes = false;
};

// Directories (and "..") first, as WinSCP's panels sort; then the column.
class DirectoriesFirst : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;
  // WinSCP's "Show hidden files" (Ctrl+Alt+H); off by default, as WinSCP's.
  void SetShowHidden(bool Show);
  bool ShowHidden() const { return FShowHidden; }

protected:
  bool lessThan(const QModelIndex & Left, const QModelIndex & Right) const override;
  bool filterAcceptsRow(int Row, const QModelIndex & Parent) const override;

private:
  bool FShowHidden = false;
};

// What a panel's drag carries (application/x-linscp-files): which side the
// entries are on, their directory and names. A local drag carries their file
// URLs too, for other programs.
struct TFileDrag
{
  bool Local = false;
  QString Directory;
  QStringList Names;
};
extern const char * const FileDragMimeType;
class QMimeData;
QMimeData * MakeFileDrag(const TFileDrag & Drag);
// false when Data is not a panel's drag.
bool ReadFileDrag(const QMimeData * Data, TFileDrag & Drag);

// A panel's view: its selection dragged as the panel says, and drops onto it
// reported with the directory row they fell on ("" for the panel's own
// directory; ".." for its parent).
class FileView : public QTreeView
{
  Q_OBJECT

public:
  explicit FileView(QWidget * Parent = nullptr);
  // The drag of the selection; null for none.
  std::function<QMimeData *()> MakeDrag;

signals:
  void Dropped(const QMimeData * Data, const QString & OnDirectory, Qt::DropAction Action);

protected:
  void startDrag(Qt::DropActions Actions) override;
  void dragEnterEvent(QDragEnterEvent * Event) override;
  void dragMoveEvent(QDragMoveEvent * Event) override;
  void dropEvent(QDropEvent * Event) override;

private:
  QString DirectoryAt(const QPoint & Pos) const;
};

#endif
