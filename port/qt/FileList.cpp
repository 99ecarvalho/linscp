#include "FileList.h"

#include <QApplication>
#include <QLocale>
#include <QStyle>

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

