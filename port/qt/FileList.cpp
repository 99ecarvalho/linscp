#include "FileList.h"

#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeData>
#include <QStyle>
#include <QUrl>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

const char * const FileDragMimeType = "application/x-linscp-files";

static QByteArray EncodeFileDrag(const TFileDrag & Drag)
{
  QJsonObject Object;
  Object[QStringLiteral("local")] = Drag.Local;
  Object[QStringLiteral("directory")] = Drag.Directory;
  Object[QStringLiteral("names")] = QJsonArray::fromStringList(Drag.Names);
  return QJsonDocument(Object).toJson(QJsonDocument::Compact);
}

QMimeData * MakeFileDrag(const TFileDrag & Drag)
{
  QMimeData * Data = new QMimeData();
  Data->setData(QString::fromLatin1(FileDragMimeType), EncodeFileDrag(Drag));
  if (Drag.Local)
  {
    QList<QUrl> Urls;
    for (const QString & Name : Drag.Names)
    {
      Urls.append(QUrl::fromLocalFile(Drag.Directory + QLatin1Char('/') + Name));
    }
    Data->setUrls(Urls);
  }
  return Data;
}

bool ReadFileDrag(const QMimeData * Data, TFileDrag & Drag)
{
  if ((Data == nullptr) || !Data->hasFormat(QString::fromLatin1(FileDragMimeType)))
  {
    return false;
  }
  const QJsonObject Object = QJsonDocument::fromJson(Data->data(QString::fromLatin1(FileDragMimeType))).object();
  Drag.Local = Object[QStringLiteral("local")].toBool();
  Drag.Directory = Object[QStringLiteral("directory")].toString();
  Drag.Names.clear();
  for (const QJsonValue & Name : Object[QStringLiteral("names")].toArray())
  {
    Drag.Names.append(Name.toString());
  }
  return true;
}

static const QString UriListMimeType = QStringLiteral("text/uri-list");

// Whether the drag is still held (Windows asks for the data while hovering).
static bool MouseButtonDown()
{
#ifdef Q_OS_WIN
  return ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON)) & 0x8000) != 0;
#else
  return false;
#endif
}

RemoteFileDrag::RemoteFileDrag(const TFileDrag & Drag, const QString & Directory, std::function<bool()> Download) :
  FNames(Drag.Names),
  FDirectory(Directory),
  FDownload(std::move(Download))
{
  setData(QString::fromLatin1(FileDragMimeType), EncodeFileDrag(Drag));
}

QStringList RemoteFileDrag::formats() const
{
  QStringList Result = QMimeData::formats();
  Result.append(UriListMimeType);
  return Result;
}

bool RemoteFileDrag::hasFormat(const QString & MimeType) const
{
  return (MimeType == UriListMimeType) || QMimeData::hasFormat(MimeType);
}

QVariant RemoteFileDrag::retrieveData(const QString & MimeType, QMetaType Type) const
{
  if (MimeType != UriListMimeType)
  {
    return QMimeData::retrieveData(MimeType, Type);
  }
  if (!FDownloaded && !FDownloading && !MouseButtonDown())
  {
    FDownloading = true;
    FDownloaded = FDownload && FDownload();
    FDownloading = false;
    if (!FDownloaded)
    {
      return QVariant();
    }
  }
  QVariantList Urls;
  for (const QString & Name : FNames)
  {
    Urls.append(QUrl::fromLocalFile(QDir(FDirectory).filePath(Name)));
  }
  return Urls;
}

FileView::FileView(QWidget * Parent) :
  QTreeView(Parent)
{
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::CopyAction);
}

void FileView::startDrag(Qt::DropActions /*Actions*/)
{
  QMimeData * Data = MakeDrag ? MakeDrag() : nullptr;
  if (Data == nullptr)
  {
    return;
  }
  QDrag * Drag = new QDrag(this);
  Drag->setMimeData(Data);
  Drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

// A panel's drag, or files from elsewhere: copy, or move with Shift, as
// WinSCP's.
void FileView::dragEnterEvent(QDragEnterEvent * Event)
{
  if (Event->mimeData()->hasFormat(QString::fromLatin1(FileDragMimeType)) || Event->mimeData()->hasUrls())
  {
    Event->acceptProposedAction();
  }
}

void FileView::dragMoveEvent(QDragMoveEvent * Event)
{
  Event->setDropAction((Event->modifiers() & Qt::ShiftModifier) ? Qt::MoveAction : Qt::CopyAction);
  Event->accept();
}

void FileView::dropEvent(QDropEvent * Event)
{
  const Qt::DropAction Action = (Event->modifiers() & Qt::ShiftModifier) ? Qt::MoveAction : Qt::CopyAction;
  emit Dropped(Event->mimeData(), DirectoryAt(Event->position().toPoint()), Action);
  Event->setDropAction(Action);
  Event->accept();
}

QString FileView::DirectoryAt(const QPoint & Pos) const
{
  const QModelIndex Index = indexAt(Pos);
  if (!Index.isValid() || !Index.data(FileListModel::DirectoryRole).toBool())
  {
    return QString();
  }
  return Index.siblingAtColumn(FileListModel::NameColumn).data(Qt::DisplayRole).toString();
}

const char * const ParentEntryName = "..";

void DirectoriesFirst::SetShowHidden(bool Show)
{
  if (Show != FShowHidden)
  {
    FShowHidden = Show;
    invalidateFilter();
  }
}

bool DirectoriesFirst::filterAcceptsRow(int Row, const QModelIndex & Parent) const
{
  return FShowHidden || !sourceModel()->index(Row, 0, Parent).data(FileListModel::HiddenRole).toBool();
}

bool DirectoriesFirst::lessThan(const QModelIndex & Left, const QModelIndex & Right) const
{
  const bool Ascending = (sortOrder() == Qt::AscendingOrder);
  // ".." stays on top whichever way the column sorts.
  if (Left.data(FileListModel::ParentRole).toBool())
  {
    return Ascending;
  }
  if (Right.data(FileListModel::ParentRole).toBool())
  {
    return !Ascending;
  }
  const bool LeftDirectory = Left.data(FileListModel::DirectoryRole).toBool();
  const bool RightDirectory = Right.data(FileListModel::DirectoryRole).toBool();
  if (LeftDirectory != RightDirectory)
  {
    return Ascending ? LeftDirectory : RightDirectory;
  }
  const QVariant A = Left.data(FileListModel::SortRole);
  const QVariant B = Right.data(FileListModel::SortRole);
  if (A.typeId() == QMetaType::QString)
  {
    return QString::compare(A.toString(), B.toString(), Qt::CaseInsensitive) < 0;
  }
  return A.toDouble() < B.toDouble();
}


void FileListModel::SetEntries(std::vector<CoreBridge::TRemoteEntry> Entries, bool WithParent)
{
  beginResetModel();
  FEntries = std::move(Entries);
  FWithParent = WithParent;
  endResetModel();
}

const CoreBridge::TRemoteEntry * FileListModel::Entry(int Row) const
{
  const int Index = Row - (FWithParent ? 1 : 0);
  return ((Index >= 0) && (Index < static_cast<int>(FEntries.size()))) ? &FEntries[Index] : nullptr;
}

int FileListModel::rowCount(const QModelIndex & Parent) const
{
  return Parent.isValid() ? 0 : static_cast<int>(FEntries.size()) + (FWithParent ? 1 : 0);
}

int FileListModel::columnCount(const QModelIndex & Parent) const
{
  return Parent.isValid() ? 0 : ColumnCount;
}

QVariant FileListModel::data(const QModelIndex & Index, int Role) const
{
  if (!Index.isValid())
  {
    return QVariant();
  }
  if (IsParent(Index.row()))
  {
    switch (Role)
    {
      case Qt::DisplayRole: return (Index.column() == NameColumn) ? QString::fromLatin1(ParentEntryName) : QVariant();
      case Qt::DecorationRole:
        return (Index.column() == NameColumn) ? QApplication::style()->standardIcon(QStyle::SP_FileDialogToParent) : QVariant();
      case DirectoryRole:
      case ParentRole: return true;
      case SortRole: return QString();
      default: return QVariant();
    }
  }
  const CoreBridge::TRemoteEntry * E = Entry(Index.row());
  if (E == nullptr)
  {
    return QVariant();
  }
  switch (Role)
  {
    case Qt::DisplayRole:
      switch (Index.column())
      {
        case NameColumn: return QString::fromStdString(E->Name);
        case SizeColumn: return E->Directory ? QVariant() : QLocale().toString(static_cast<qlonglong>(E->Size));
        case ModifiedColumn: return QString::fromStdString(E->Modified);
        case RightsColumn: return QString::fromStdString(E->Rights);
        case OwnerColumn: return QString::fromStdString(E->Owner);
      }
      break;
    case Qt::ToolTipRole:
      return E->Link ? QStringLiteral("%1 -> %2").arg(QString::fromStdString(E->Name), QString::fromStdString(E->LinkTarget)) : QVariant();
    case Qt::DecorationRole:
      if (Index.column() == NameColumn)
      {
        return QApplication::style()->standardIcon(
          E->Directory ? (E->Link ? QStyle::SP_DirLinkIcon : QStyle::SP_DirIcon) :
          (E->Link ? QStyle::SP_FileLinkIcon : QStyle::SP_FileIcon));
      }
      break;
    case Qt::TextAlignmentRole:
      if (Index.column() == SizeColumn)
      {
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
      }
      break;
    case DirectoryRole: return E->Directory;
    case ParentRole: return false;
    case HiddenRole: return E->Hidden;
    case SortRole:
      switch (Index.column())
      {
        case SizeColumn: return static_cast<double>(E->Size);
        case ModifiedColumn: return E->ModifiedTime;
        case RightsColumn: return QString::fromStdString(E->Rights);
        case OwnerColumn: return QString::fromStdString(E->Owner);
        default: return QString::fromStdString(E->Name);
      }
  }
  return QVariant();
}

QVariant FileListModel::headerData(int Section, Qt::Orientation Orientation, int Role) const
{
  if ((Orientation != Qt::Horizontal) || (Role != Qt::DisplayRole))
  {
    return QVariant();
  }
  switch (Section)
  {
    case NameColumn: return tr("Name");
    case SizeColumn: return tr("Size");
    case ModifiedColumn: return tr("Changed");
    case RightsColumn: return FAttributes ? tr("Attributes") : tr("Rights");
    case OwnerColumn: return tr("Owner");
  }
  return QVariant();
}

