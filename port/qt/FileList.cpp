#include "FileList.h"

#include <cmath>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QLineEdit>
#include <QMessageBox>
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
  // Only F2 edits (EditCurrentName): a double click opens.
  setEditTriggers(QAbstractItemView::NoEditTriggers);
}

bool FileView::EditCurrentName()
{
  const QModelIndex Current = currentIndex();
  if (!Current.isValid() || (EntryAt(Current.row()) == nullptr))
  {
    return false;
  }
  const QModelIndex Name = Current.siblingAtColumn(FileListModel::NameColumn);
  setCurrentIndex(Name);
  edit(Name);
  return true;
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

QString FormatBytes(std::int64_t Bytes)
{
  const QLocale Locale;
  if (Bytes < 1024)
  {
    return QStringLiteral("%1 B").arg(Locale.toString(static_cast<qlonglong>(Bytes)));
  }
  double Order;
  QString Unit;
  if (Bytes < 1000LL * 1024)
  {
    Order = 1024.0;
    Unit = QStringLiteral("KB");
  }
  else if (Bytes < 1000LL * 1024 * 1024)
  {
    Order = 1024.0 * 1024;
    Unit = QStringLiteral("MB");
  }
  else
  {
    Order = 1024.0 * 1024 * 1024;
    Unit = QStringLiteral("GB");
  }
  const double Value = static_cast<double>(Bytes) / Order;
  QString Number;
  if (Value >= 100)
  {
    Number = Locale.toString(static_cast<qlonglong>(std::floor(Value)));
  }
  else if (Value >= 10)
  {
    Number = Locale.toString(std::floor(Value * 10) / 10, 'f', 1);
  }
  else
  {
    Number = Locale.toString(std::floor(Value * 100) / 100, 'f', 2);
  }
  return QStringLiteral("%1 %2").arg(Number, Unit);
}

QString FileView::StatusText() const
{
  const QSortFilterProxyModel * Proxy = qobject_cast<const QSortFilterProxyModel *>(model());
  if (Proxy == nullptr)
  {
    return QString();
  }
  std::int64_t FilesSize = 0;
  std::int64_t SelectedSize = 0;
  int FilesCount = 0;
  int SelectedCount = 0;
  for (int Row = 0; Row < Proxy->rowCount(); Row++)
  {
    const CoreBridge::TRemoteEntry * Entry = EntryAt(Row);
    if (Entry == nullptr)
    {
      continue;
    }
    const std::int64_t Size = Entry->Directory ? 0 : Entry->Size;
    FilesCount++;
    FilesSize += Size;
    if (selectionModel()->isRowSelected(Row, QModelIndex()))
    {
      SelectedCount++;
      SelectedSize += Size;
    }
  }
  const QLocale Locale;
  QString Result = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FileInfoFormat));
  for (const QString & Part : { FormatBytes(SelectedSize), FormatBytes(FilesSize), Locale.toString(SelectedCount),
         Locale.toString(FilesCount) })
  {
    Result.replace(Result.indexOf(QStringLiteral("%s")), 2, Part);
  }
  const FileListModel * Source = qobject_cast<const FileListModel *>(Proxy->sourceModel());
  const int Hidden = (Source != nullptr) ? (Source->rowCount() - Proxy->rowCount()) : 0;
  if (Hidden > 0)
  {
    QString HiddenText = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FileInfoHidden));
    Result += QStringLiteral("    ") + HiddenText.replace(QStringLiteral("%s"), Locale.toString(Hidden));
  }
  return Result;
}

void FileView::SetupColumns()
{
  setColumnHidden(FileListModel::GroupColumn, true);
  setColumnHidden(FileListModel::LinkTargetColumn, true);
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), &QWidget::customContextMenuRequested, this, [this](const QPoint & Pos)
  {
    QMenu * Menu = ColumnsMenu();
    Menu->exec(header()->mapToGlobal(Pos));
    Menu->deleteLater();
  });
}

QMenu * FileView::ColumnsMenu()
{
  QMenu * Menu = new QMenu(this);
  for (int Column = FileListModel::NameColumn + 1; Column < model()->columnCount(); Column++)
  {
    QAction * Action = Menu->addAction(model()->headerData(Column, Qt::Horizontal).toString());
    Action->setCheckable(true);
    Action->setChecked(!isColumnHidden(Column));
    connect(Action, &QAction::toggled, this, [this, Column](bool Shown) { setColumnHidden(Column, !Shown); });
  }
  return Menu;
}

QString FileView::FLastMask = QStringLiteral("*.*");
bool FileView::FLastDirectories = false;

bool FileView::event(QEvent * Event)
{
  if (Event->type() == QEvent::KeyPress)
  {
    QKeyEvent * Key = static_cast<QKeyEvent *>(Event);
    if (((Key->key() == Qt::Key_Tab) || (Key->key() == Qt::Key_Backtab)) &&
        ((Key->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier))
    {
      emit SwitchPanel();
      return true;
    }
  }
  return QTreeView::event(Event);
}

// As WinSCP's Commander: Ctrl+F3 name, Ctrl+F5 changed, Ctrl+F6 size,
// Ctrl+F7 rights, Ctrl+F8 owner, Ctrl+F9 group (Ctrl+F4, extension, has no
// column here).
static int SortColumnOfKey(int Key)
{
  switch (Key)
  {
    case Qt::Key_F3: return FileListModel::NameColumn;
    case Qt::Key_F5: return FileListModel::ModifiedColumn;
    case Qt::Key_F6: return FileListModel::SizeColumn;
    case Qt::Key_F7: return FileListModel::RightsColumn;
    case Qt::Key_F8: return FileListModel::OwnerColumn;
    case Qt::Key_F9: return FileListModel::GroupColumn;
    default: return -1;
  }
}

void FileView::keyPressEvent(QKeyEvent * Event)
{
  const Qt::KeyboardModifiers Modifiers = Event->modifiers() & ~Qt::KeypadModifier;
  const bool Keypad = (Event->modifiers() & Qt::KeypadModifier) != 0;
  if ((Event->key() == Qt::Key_Insert) && (Modifiers == Qt::NoModifier))
  {
    ToggleCurrent();
  }
  else if (Keypad && (Modifiers == Qt::NoModifier) && (Event->key() == Qt::Key_Asterisk))
  {
    InvertSelection();
  }
  else if (Keypad && (Modifiers == Qt::NoModifier) && ((Event->key() == Qt::Key_Plus) || (Event->key() == Qt::Key_Minus)))
  {
    SelectByMaskDialog(Event->key() == Qt::Key_Plus);
  }
  else if ((Event->key() == Qt::Key_L) && (Modifiers == (Qt::ControlModifier | Qt::ShiftModifier)))
  {
    clearSelection();
  }
  else if ((Modifiers == Qt::ControlModifier) && (SortColumnOfKey(Event->key()) >= 0))
  {
    // WinSCP's Ctrl+F3... sort keys: the column again, the other way.
    const int Column = SortColumnOfKey(Event->key());
    const bool Again = (header()->sortIndicatorSection() == Column);
    sortByColumn(Column, (Again && (header()->sortIndicatorOrder() == Qt::AscendingOrder)) ? Qt::DescendingOrder : Qt::AscendingOrder);
  }
  else
  {
    QTreeView::keyPressEvent(Event);
    return;
  }
  Event->accept();
}

const CoreBridge::TRemoteEntry * FileView::EntryAt(int Row) const
{
  const QSortFilterProxyModel * Proxy = qobject_cast<const QSortFilterProxyModel *>(model());
  const FileListModel * Source = (Proxy != nullptr) ? qobject_cast<const FileListModel *>(Proxy->sourceModel()) : nullptr;
  if (Source == nullptr)
  {
    return nullptr;
  }
  const int SourceRow = Proxy->mapToSource(Proxy->index(Row, 0)).row();
  return Source->IsParent(SourceRow) ? nullptr : Source->Entry(SourceRow);
}

void FileView::ToggleCurrent()
{
  const QModelIndex Current = currentIndex();
  if (!Current.isValid())
  {
    return;
  }
  if (EntryAt(Current.row()) != nullptr)
  {
    selectionModel()->select(Current, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
  }
  const QModelIndex Next = model()->index(Current.row() + 1, Current.column());
  if (Next.isValid())
  {
    selectionModel()->setCurrentIndex(Next, QItemSelectionModel::NoUpdate);
  }
}

void FileView::InvertSelection()
{
  QItemSelection Selection;
  for (int Row = 0; Row < model()->rowCount(); Row++)
  {
    if (EntryAt(Row) != nullptr)
    {
      Selection.select(model()->index(Row, 0), model()->index(Row, model()->columnCount() - 1));
    }
  }
  selectionModel()->select(Selection, QItemSelectionModel::Toggle);
}

// As TUnixDirView::ItemMatchesFilter: a directory also by its name alone
// when the mask is to apply to directories.
bool FileView::SelectByMask(bool Select, const QString & Masks, bool Directories)
{
  QItemSelection Selection;
  try
  {
    for (int Row = 0; Row < model()->rowCount(); Row++)
    {
      const CoreBridge::TRemoteEntry * Entry = EntryAt(Row);
      if (Entry == nullptr)
      {
        continue;
      }
      const std::string Mask = Masks.toStdString();
      const bool Matches = Masks.trimmed().isEmpty() ||
        CoreBridge::MaskMatches(Mask, Entry->Name, Entry->Directory, Entry->Size, Entry->ModifiedTime) ||
        (Entry->Directory && Directories && CoreBridge::MaskMatches(Mask, Entry->Name, false, Entry->Size, Entry->ModifiedTime));
      if (Matches)
      {
        Selection.select(model()->index(Row, 0), model()->index(Row, model()->columnCount() - 1));
      }
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QMessageBox::warning(this, window()->windowTitle(), QString::fromStdString(E.Error.Message));
    return false;
  }
  selectionModel()->select(Selection, Select ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
  return true;
}

// As WinSCP's Select/Deselect dialog (forms/SelectMask.cpp), without its
// colors and hints.
void FileView::SelectByMaskDialog(bool Select)
{
  QDialog Dialog(this);
  Dialog.setWindowTitle(Select ? tr("Select") : tr("Deselect"));
  QFormLayout * Layout = new QFormLayout(&Dialog);
  QLineEdit * Mask = new QLineEdit(FLastMask, &Dialog);
  Mask->setMinimumWidth(300);
  Layout->addRow(tr("File &mask:"), Mask);
  QCheckBox * Directories = new QCheckBox(tr("Apply to &directories"), &Dialog);
  Directories->setChecked(FLastDirectories);
  Layout->addRow(Directories);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addRow(Box);
  if (Dialog.exec() == QDialog::Accepted)
  {
    FLastMask = Mask->text();
    FLastDirectories = Directories->isChecked();
    SelectByMask(Select, FLastMask, FLastDirectories);
  }
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

Qt::ItemFlags FileListModel::flags(const QModelIndex & Index) const
{
  Qt::ItemFlags Result = QAbstractTableModel::flags(Index);
  if (Index.isValid() && (Index.column() == NameColumn) && !IsParent(Index.row()))
  {
    Result |= Qt::ItemIsEditable;
  }
  return Result;
}

bool FileListModel::setData(const QModelIndex & Index, const QVariant & Value, int Role)
{
  const CoreBridge::TRemoteEntry * E = Index.isValid() ? Entry(Index.row()) : nullptr;
  if ((Role == Qt::EditRole) && (E != nullptr) && (Index.column() == NameColumn))
  {
    const QString Name = QString::fromStdString(E->Name);
    const QString NewName = Value.toString().trimmed();
    if (!NewName.isEmpty() && (NewName != Name))
    {
      emit RenameRequested(Name, NewName);
    }
  }
  return false;
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
    case Qt::EditRole:
      return (Index.column() == NameColumn) ? QVariant(QString::fromStdString(E->Name)) : QVariant();
    case Qt::DisplayRole:
      switch (Index.column())
      {
        case NameColumn: return QString::fromStdString(E->Name);
        case SizeColumn: return E->Directory ? QVariant() : QLocale().toString(static_cast<qlonglong>(E->Size));
        case ModifiedColumn: return QString::fromStdString(E->Modified);
        case RightsColumn: return QString::fromStdString(E->Rights);
        case OwnerColumn: return QString::fromStdString(E->Owner);
        case GroupColumn: return QString::fromStdString(E->Group);
        case LinkTargetColumn: return QString::fromStdString(E->LinkTarget);
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
        case GroupColumn: return QString::fromStdString(E->Group);
        case LinkTargetColumn: return QString::fromStdString(E->LinkTarget);
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
    case GroupColumn: return tr("Group");
    case LinkTargetColumn: return tr("Link target");
  }
  return QVariant();
}

