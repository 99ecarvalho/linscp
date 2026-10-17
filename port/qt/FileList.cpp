#include "FileList.h"
#include <QMouseEvent>
#include <QMimeDatabase>
#include <QStyledItemDelegate>
#include "HintButton.h"
#include "Integration.h"
#include "EditMaskDialog.h"

#include <algorithm>
#include <cmath>

#include <QActionGroup>
#include <QFileIconProvider>
#include <QImageReader>
#include <QPainter>
#include <QPointer>
#include <QThreadPool>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QLabel>
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

// Full row select off (WinSCP's default): only the name shows selected.
class TPanelDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  bool FullRow = false;

protected:
  void initStyleOption(QStyleOptionViewItem * Option, const QModelIndex & Index) const override
  {
    QStyledItemDelegate::initStyleOption(Option, Index);
    if (!FullRow && (Index.column() != FileListModel::NameColumn))
    {
      Option->state &= ~QStyle::State_Selected;
    }
  }
};

FileView::FileView(QWidget * Parent) :
  QTreeView(Parent)
{
  FDelegate = new TPanelDelegate(this);
  setItemDelegate(FDelegate);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::CopyAction);
  // Only F2 edits (EditCurrentName): a double click opens.
  setEditTriggers(QAbstractItemView::NoEditTriggers);
}

const CoreBridge::TRemoteEntry * FileView::CurrentEntry() const
{
  const QModelIndex Current = currentIndex();
  return Current.isValid() ? EntryAt(Current.row()) : nullptr;
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
  // As WinSCP's: the name without its extension selected, unless the whole
  // name is to be (a directory's is always whole).
  QLineEdit * Editor = viewport()->findChild<QLineEdit *>();
  if ((Editor != nullptr) && !RenameWholeName && !Name.data(FileListModel::DirectoryRole).toBool())
  {
    const QString Text = Editor->text();
    const int Dot = Text.lastIndexOf(QLatin1Char('.'));
    if (Dot > 0)
    {
      Editor->setSelection(0, Dot);
    }
  }
  return true;
}

void FileView::startDrag(Qt::DropActions /*Actions*/)
{
  QMimeData * Data = MakeDrag ? MakeDrag() : nullptr;
  if (Data == nullptr)
  {
    return;
  }
  if (OnDragStarted)
  {
    OnDragStarted(Data);
    return;
  }
  QDrag * Drag = new QDrag(this);
  Drag->setMimeData(Data);
  const Qt::DropAction Action = Drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
  if (OnDragFinished)
  {
    OnDragFinished(Action);
  }
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

// As BaseUtils.pas' FormatPanelBytes, a panel's size column: bytes without
// a unit, kilobytes rounded up, or FormatBytes' short form.
QString FormatPanelBytes(std::int64_t Bytes, CoreBridge::TPreferences::TSizeFormat Format)
{
  const QLocale Locale;
  switch (Format)
  {
    case CoreBridge::TPreferences::TSizeFormat::Bytes:
      return Locale.toString(static_cast<qlonglong>(Bytes));
    case CoreBridge::TPreferences::TSizeFormat::Kilobytes:
      return QStringLiteral("%1 KB").arg(Locale.toString(static_cast<qlonglong>((Bytes / 1024) + ((Bytes % 1024) > 0 ? 1 : 0))));
    default:
      return FormatBytes(Bytes);
  }
}

void FileListModel::SetFileColors(const std::vector<CoreBridge::TPreferences::TFileColor> & Colors, bool Local,
  std::function<std::string(const CoreBridge::TRemoteEntry &)> Path)
{
  FFileColorRules = Colors;
  FColorsLocal = Local;
  FColorPath = std::move(Path);
  UpdateColors();
  if (rowCount() > 0)
  {
    emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1), { Qt::ForegroundRole });
  }
}

// As TCustomScpExplorerForm::DirViewGetItemColor: the first rule the full
// path matches, explicitly.
void FileListModel::UpdateColors()
{
  FColors.assign(FEntries.size(), QColor());
  if (FFileColorRules.empty() || !FColorPath)
  {
    return;
  }
  for (std::size_t Index = 0; Index < FEntries.size(); Index++)
  {
    const CoreBridge::TRemoteEntry & Entry = FEntries[Index];
    const std::string Path = FColorPath(Entry);
    for (const CoreBridge::TPreferences::TFileColor & Rule : FFileColorRules)
    {
      bool Matches = false;
      try
      {
        Matches = CoreBridge::MaskMatchesPath(Rule.Mask, Path, FColorsLocal, Entry.Directory, Entry.Size, Entry.ModifiedTime);
      }
      catch (const CoreBridge::TCoreError &)
      {
        // A mask that is not one colors nothing, as upstream's would not
        // have been saved.
      }
      if (Matches)
      {
        FColors[Index] = QColor(static_cast<QRgb>(Rule.Color));
        break;
      }
    }
  }
}

void FileListModel::SetSizeFormat(CoreBridge::TPreferences::TSizeFormat Format)
{
  FSizeFormat = Format;
  if (rowCount() > 0)
  {
    emit dataChanged(index(0, SizeColumn), index(rowCount() - 1, SizeColumn));
  }
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

void FileView::SetupColumns(bool TypeShown)
{
  setColumnHidden(FileListModel::GroupColumn, true);
  setColumnHidden(FileListModel::LinkTargetColumn, true);
  setColumnHidden(FileListModel::ExtColumn, true);
  setColumnHidden(FileListModel::TypeColumn, !TypeShown);
  // As WinSCP's panels have it: after the size (moved, not reordered in the
  // model, so that saved layouts keep their indexes).
  header()->moveSection(header()->visualIndex(FileListModel::TypeColumn), FileListModel::SizeColumn + 1);
  FDefaultLayout = header()->saveState();
  header()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(header(), &QWidget::customContextMenuRequested, this, [this](const QPoint & Pos)
  {
    QMenu * Menu = HeaderMenu(header()->logicalIndexAt(Pos));
    Menu->exec(header()->mapToGlobal(Pos));
    Menu->deleteLater();
  });
}

// TIEListViewColProperties::SetParamsStr: the sort, then each WinSCP
// column's width and visibility at the pixels per inch saved, then their
// order.
void FileView::SetColumnsParams(const QString & Params, const std::vector<int> & Map, int Count)
{
  const QStringList Parts = Params.split(QLatin1Char('|'));
  if (Parts.size() < 3)
  {
    return;
  }
  const QStringList Sort = Parts[0].split(QLatin1Char(';'));
  QStringList Widths = Parts[1].split(QLatin1Char(';'));
  int PixelsPerInch = logicalDpiX();
  QStringList Entries;
  for (const QString & Width : Widths)
  {
    if (Width.startsWith(QLatin1Char('@')))
    {
      PixelsPerInch = Width.mid(1).toInt();
    }
    else if (!Width.isEmpty() && (Entries.size() < Count))
    {
      Entries.append(Width);
    }
  }
  if (PixelsPerInch <= 0)
  {
    PixelsPerInch = logicalDpiX();
  }
  FColumnEntries = Entries;
  FColumnOrder = Parts[2].split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (int Column = 0; Column < static_cast<int>(Map.size()); Column++)
  {
    const int Index = Map[Column];
    if ((Index < 0) || (Index >= Entries.size()))
    {
      continue;
    }
    const QStringList Entry = Entries[Index].split(QLatin1Char(','));
    if (Entry.size() < 2)
    {
      continue;
    }
    // LoadDimension: to this view's pixels per inch.
    const int Width = Entry[0].toInt() * logicalDpiX() / PixelsPerInch;
    const bool Visible = (Entry[1].toInt() != 0) || (Column == FileListModel::NameColumn);
    setColumnHidden(Column, !Visible);
    if (Width > 0)
    {
      header()->resizeSection(Column, Width);
    }
  }
  // The order: WinSCP's columns, of which the view's in that order.
  int Visual = 0;
  for (const QString & Item : FColumnOrder)
  {
    const auto Found = std::find(Map.begin(), Map.end(), Item.toInt());
    if (Found != Map.end())
    {
      header()->moveSection(header()->visualIndex(static_cast<int>(Found - Map.begin())), Visual++);
    }
  }
  if (Sort.size() >= 2)
  {
    const auto Found = std::find(Map.begin(), Map.end(), Sort[0].toInt());
    const int Column = (Found != Map.end()) ? static_cast<int>(Found - Map.begin()) : FileListModel::NameColumn;
    sortByColumn(Column, (Sort[1].toInt() != 0) ? Qt::AscendingOrder : Qt::DescendingOrder);
  }
}

// TIEListViewColProperties::GetParamsStr, keeping what the view does not
// have as it was given.
QString FileView::ColumnsParams(const std::vector<int> & Map, int Count) const
{
  QStringList Entries = FColumnEntries;
  while (Entries.size() < Count)
  {
    Entries.append(QStringLiteral("100,0"));
  }
  for (int Column = 0; Column < static_cast<int>(Map.size()); Column++)
  {
    const int Index = Map[Column];
    if ((Index >= 0) && (Index < Count))
    {
      const int Width = isColumnHidden(Column) ? Entries[Index].split(QLatin1Char(',')).value(0).toInt() : header()->sectionSize(Column);
      Entries[Index] = QStringLiteral("%1,%2").arg(Width).arg(isColumnHidden(Column) ? 0 : 1);
    }
  }
  // The order: as given, the view's columns in its visual order in the
  // places its columns had.
  QStringList Order = FColumnOrder;
  for (int Index = 0; Index < Count; Index++)
  {
    if (!Order.contains(QString::number(Index)))
    {
      Order.append(QString::number(Index));
    }
  }
  std::vector<int> Mine;
  for (int Visual = 0; Visual < header()->count(); Visual++)
  {
    const int Column = header()->logicalIndex(Visual);
    if ((Column >= 0) && (Column < static_cast<int>(Map.size())) && (Map[Column] >= 0))
    {
      Mine.push_back(Map[Column]);
    }
  }
  std::size_t Next = 0;
  for (QString & Item : Order)
  {
    if ((std::find(Map.begin(), Map.end(), Item.toInt()) != Map.end()) && (Next < Mine.size()))
    {
      Item = QString::number(Mine[Next++]);
    }
  }
  const int SortColumn = header()->sortIndicatorSection();
  const int SortIndex = ((SortColumn >= 0) && (SortColumn < static_cast<int>(Map.size())) && (Map[SortColumn] >= 0)) ?
    Map[SortColumn] : 0;
  return QStringLiteral("%1;%2|%3;@%4|%5").arg(SortIndex).arg(header()->sortIndicatorOrder() == Qt::AscendingOrder ? 1 : 0)
    .arg(Entries.join(QLatin1Char(';'))).arg(logicalDpiX()).arg(Order.join(QLatin1Char(';')));
}

QMenu * FileView::ColumnsMenu(bool MenuCaptions)
{
  QMenu * Menu = new QMenu(this);
  // Filled each time it opens, so that the checks follow the header's.
  const auto Fill = [this, Menu, MenuCaptions]()
  {
    Menu->clear();
    const int Order[] = { FileListModel::NameColumn, FileListModel::SizeColumn, FileListModel::TypeColumn,
      FileListModel::ModifiedColumn, FileListModel::RightsColumn, FileListModel::OwnerColumn, FileListModel::GroupColumn,
      FileListModel::LinkTargetColumn, FileListModel::ExtColumn };
    for (const int Column : Order)
    {
      // The main menu's Columns has no extension, as WinSCP's (its header's
      // has); the header's has no name, which stays.
      if ((MenuCaptions && (Column == FileListModel::ExtColumn)) || (!MenuCaptions && (Column == FileListModel::NameColumn)))
      {
        continue;
      }
      const QString Header = model()->headerData(Column, Qt::Horizontal).toString();
      QString Caption = Header;
      // As the main menu's Columns (NonVisual.dfm's ShowHide...ColumnAction).
      if (MenuCaptions)
      {
        switch (Column)
        {
          case FileListModel::NameColumn: Caption = tr("&Name"); break;
          case FileListModel::SizeColumn: Caption = tr("&Size"); break;
          case FileListModel::TypeColumn: Caption = tr("&Type"); break;
          case FileListModel::ModifiedColumn: Caption = tr("&Date modified"); break;
          case FileListModel::RightsColumn:
            Caption = (Header == FileListModel::tr("Attributes")) ? tr("&Attributes") : tr("&Permissions");
            break;
          case FileListModel::OwnerColumn: Caption = tr("&Owner"); break;
          case FileListModel::GroupColumn: Caption = tr("&Group"); break;
          case FileListModel::LinkTargetColumn: Caption = tr("&Link Target"); break;
          default: break;
        }
      }
      QAction * Action = Menu->addAction(Caption);
      Action->setCheckable(true);
      Action->setChecked(!isColumnHidden(Column));
      // The name is always shown.
      Action->setEnabled(Column != FileListModel::NameColumn);
      connect(Action, &QAction::toggled, this, [this, Column](bool Shown) { setColumnHidden(Column, !Shown); });
    }
    if (MenuCaptions)
    {
      Menu->addSeparator();
      Menu->addAction(tr("Size A&utomatically"), this, [this]() { SizeColumnsAutomatically(); })
        ->setStatusTip(tr("Adjust columns width to fit their contents"));
      Menu->addAction(tr("&Reset Layout"), this, [this]() { ResetLayout(); })
        ->setStatusTip(tr("Reset to the default layout of list columns"));
    }
  };
  Fill();
  connect(Menu, &QMenu::aboutToShow, this, Fill);
  return Menu;
}

void FileView::SizeColumnsAutomatically()
{
  for (int Column = 0; Column < model()->columnCount(); Column++)
  {
    if (!isColumnHidden(Column))
    {
      resizeColumnToContents(Column);
    }
  }
}

void FileView::ResetLayout()
{
  if (!FDefaultLayout.isEmpty())
  {
    header()->restoreState(FDefaultLayout);
  }
  // WinSCP's default sort ("0;1": by name, ascending).
  sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
}

QMenu * FileView::HeaderMenu(int Column)
{
  QMenu * Menu = new QMenu(this);
  const bool OnColumn = (Column >= 0);
  const Qt::SortOrder Order = header()->sortIndicatorOrder();
  const int Sorted = header()->sortIndicatorSection();
  QAction * Ascending = Menu->addAction(tr("Sort &Ascending"), this, [this, Column]() { sortByColumn(Column, Qt::AscendingOrder); });
  Ascending->setCheckable(true);
  Ascending->setChecked(OnColumn && (Sorted == Column) && (Order == Qt::AscendingOrder));
  Ascending->setEnabled(OnColumn);
  QAction * Descending = Menu->addAction(tr("Sort &Descending"), this, [this, Column]() { sortByColumn(Column, Qt::DescendingOrder); });
  Descending->setCheckable(true);
  Descending->setChecked(OnColumn && (Sorted == Column) && (Order == Qt::DescendingOrder));
  Descending->setEnabled(OnColumn);
  QAction * ByExtension = Menu->addAction(tr("By &Extension"), this, [this]()
  {
    sortByColumn(FileListModel::ExtColumn, header()->sortIndicatorOrder());
  });
  ByExtension->setCheckable(true);
  ByExtension->setChecked(Sorted == FileListModel::ExtColumn);
  // The name is always shown.
  QAction * Hide = Menu->addAction(tr("&Hide Column"), this, [this, Column]() { setColumnHidden(Column, true); });
  Hide->setEnabled(OnColumn && (Column != FileListModel::NameColumn));
  Menu->addSeparator();
  QMenu * Sizes = Menu->addMenu(tr("Show File Si&zes In"));
  QActionGroup * SizeGroup = new QActionGroup(Sizes);
  const int Current = static_cast<int>(CoreBridge::Preferences().SizeFormat);
  const std::pair<QString, CoreBridge::TPreferences::TSizeFormat> Formats[] = {
    { tr("&Bytes"), CoreBridge::TPreferences::TSizeFormat::Bytes },
    { tr("&Kilobytes"), CoreBridge::TPreferences::TSizeFormat::Kilobytes },
    { tr("&Short format"), CoreBridge::TPreferences::TSizeFormat::Short } };
  for (const auto & Format : Formats)
  {
    const int Value = static_cast<int>(Format.second);
    QAction * Action = Sizes->addAction(Format.first, this, [this, Value]() { emit SizeFormatChosen(Value); });
    Action->setCheckable(true);
    Action->setChecked(Value == Current);
    SizeGroup->addAction(Action);
  }
  Menu->addAction(tr("&Calculate Directory Sizes"), this, [this]() { emit CalculateDirectorySizesRequested(); });
  Menu->addSeparator();
  QMenu * Columns = ColumnsMenu(true);
  Columns->setTitle(tr("&Columns"));
  Columns->setParent(Menu, Columns->windowFlags());
  Menu->addMenu(Columns);
  return Menu;
}

QMenu * FileView::SortMenu(bool Local)
{
  QMenu * Menu = new QMenu(this);
  QAction * Ascending = Menu->addAction(tr("&Ascending"));
  Ascending->setCheckable(true);
  Ascending->setStatusTip(tr("Toggle ascending/descending sort of the panel"));
  connect(Ascending, &QAction::triggered, this, [this](bool On)
  {
    sortByColumn(header()->sortIndicatorSection(), On ? Qt::AscendingOrder : Qt::DescendingOrder);
  });
  Menu->addSeparator();
  std::vector<std::pair<int, QString>> By = {
    { FileListModel::NameColumn, tr("By &Name") },
    { FileListModel::ExtColumn, tr("By &Extension") },
    { FileListModel::SizeColumn, tr("By &Size") },
    { FileListModel::TypeColumn, tr("By &Type") },
    { FileListModel::ModifiedColumn, tr("By &Date modified") } };
  if (Local)
  {
    By.push_back({ FileListModel::RightsColumn, tr("By Att&ributes") });
  }
  else
  {
    By.push_back({ FileListModel::RightsColumn, tr("By &Permissions/Attributes") });
    By.push_back({ FileListModel::OwnerColumn, tr("By &Owner") });
    By.push_back({ FileListModel::GroupColumn, tr("By &Group") });
  }
  QActionGroup * Group = new QActionGroup(Menu);
  std::vector<std::pair<int, QAction *>> Actions;
  for (const auto & Each : By)
  {
    QAction * Action = Menu->addAction(Each.second);
    Action->setCheckable(true);
    Group->addAction(Action);
    const int Column = Each.first;
    connect(Action, &QAction::triggered, this, [this, Column]() { sortByColumn(Column, header()->sortIndicatorOrder()); });
    Actions.push_back({ Column, Action });
  }
  connect(Menu, &QMenu::aboutToShow, this, [this, Ascending, Actions]()
  {
    Ascending->setChecked(header()->sortIndicatorOrder() == Qt::AscendingOrder);
    for (const auto & Each : Actions)
    {
      Each.second->setChecked(header()->sortIndicatorSection() == Each.first);
    }
  });
  return Menu;
}

void FileView::SaveSelectedNames()
{
  FSavedNames.clear();
  const QModelIndexList Rows = selectionModel()->selectedRows(FileListModel::NameColumn);
  for (const QModelIndex & Row : Rows)
  {
    if (EntryAt(Row.row()) != nullptr)
    {
      FSavedNames.append(Row.data(Qt::DisplayRole).toString());
    }
  }
}

void FileView::RestoreSelectedNames()
{
  QItemSelection Selection;
  for (int Row = 0; Row < model()->rowCount(); Row++)
  {
    const QModelIndex Name = model()->index(Row, FileListModel::NameColumn);
    if ((EntryAt(Row) != nullptr) && FSavedNames.contains(Name.data(Qt::DisplayRole).toString()))
    {
      Selection.select(Name, Name);
    }
  }
  selectionModel()->select(Selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  setFocus();
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
  // While searching: Backspace takes the last character back, Esc ends it;
  // any other key but a character typed ends it too.
  if (Searching())
  {
    if ((Event->key() == Qt::Key_Backspace) && (Modifiers == Qt::NoModifier))
    {
      const QString Text = FSearch.isEmpty() ? QString() : FSearch.chopped(1);
      FSearch.clear();
      if (!Text.isEmpty())
      {
        keyboardSearch(Text);
      }
      else
      {
        FSearching = false;
        emit SearchChanged(QString());
      }
      Event->accept();
      return;
    }
    if (Event->key() == Qt::Key_Escape)
    {
      EndSearch();
      Event->accept();
      return;
    }
    if (Event->text().isEmpty() || !Event->text().at(0).isPrint() || (Modifiers & ~Qt::ShiftModifier))
    {
      EndSearch();
    }
  }
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
// colors; its mask hints.
bool MaskDialog(QWidget * Parent, TMaskDialogMode Mode, QString & Mask, bool * Directories)
{
  QDialog Dialog(Parent);
  Dialog.setWindowTitle(QString::fromStdString(CoreBridge::Text(
    (Mode == TMaskDialogMode::Select) ? CoreBridge::TText::SelectMaskSelectCaption :
    ((Mode == TMaskDialogMode::Deselect) ? CoreBridge::TText::SelectMaskDeselectCaption : CoreBridge::TText::FilterMaskCaption))));
  QFormLayout * Layout = new QFormLayout(&Dialog);
  QHBoxLayout * Row = new QHBoxLayout();
  QLineEdit * MaskEdit = new QLineEdit(Mask, &Dialog);
  MaskEdit->setMinimumWidth(300);
  Row->addWidget(MaskEdit, 1);
  QPushButton * EditButton = new QPushButton(FileView::tr("&Edit..."), &Dialog);
  QObject::connect(EditButton, &QPushButton::clicked, &Dialog, [&Dialog, MaskEdit]()
  {
    QString Edited = MaskEdit->text();
    if (EditMaskDialog::Edit(Edited, &Dialog))
    {
      MaskEdit->setText(Edited);
    }
  });
  Row->addWidget(EditButton);
  QLabel * MaskLabel = new QLabel(FileView::tr("File &mask:"), &Dialog);
  MaskLabel->setBuddy(MaskEdit);
  Layout->addRow(MaskLabel, Row);
  Layout->addRow(QString(), MakeMaskHintButton(FileView::tr("mask hi&nts"),
    { CoreBridge::TText::MaskHint, CoreBridge::TText::FileMaskExHint, CoreBridge::TText::CombiningMasksHint }, &Dialog));
  QCheckBox * DirectoriesCheck = nullptr;
  if ((Mode != TMaskDialogMode::Filter) && (Directories != nullptr))
  {
    DirectoriesCheck = new QCheckBox(FileView::tr("Apply to &directories"), &Dialog);
    DirectoriesCheck->setChecked(*Directories);
    Layout->addRow(DirectoriesCheck);
  }
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  if (Mode == TMaskDialogMode::Filter)
  {
    // Clear: no filter.
    QObject::connect(Box->addButton(FileView::tr("&Clear"), QDialogButtonBox::ResetRole), &QPushButton::clicked, &Dialog,
      [&Dialog, MaskEdit]()
      {
        MaskEdit->clear();
        Dialog.accept();
      });
  }
  QObject::connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  QObject::connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addRow(Box);
  MaskEdit->selectAll();
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Mask = MaskEdit->text();
  if (DirectoriesCheck != nullptr)
  {
    *Directories = DirectoriesCheck->isChecked();
  }
  return true;
}

void FileView::SelectByMaskDialog(bool Select)
{
  QString Mask = FLastMask;
  bool Directories = FLastDirectories;
  if (MaskDialog(this, Select ? TMaskDialogMode::Select : TMaskDialogMode::Deselect, Mask, &Directories))
  {
    FLastMask = Mask;
    FLastDirectories = Directories;
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

void DirectoriesFirst::SetShowInaccessible(bool Show)
{
  if (Show != FShowInaccessible)
  {
    FShowInaccessible = Show;
    invalidateFilter();
  }
}

void DirectoriesFirst::SetMask(const QString & Mask)
{
  FMask = Mask;
  invalidateFilter();
}

bool DirectoriesFirst::filterAcceptsRow(int Row, const QModelIndex & Parent) const
{
  const QModelIndex Index = sourceModel()->index(Row, 0, Parent);
  if (!FShowHidden && Index.data(FileListModel::HiddenRole).toBool())
  {
    return false;
  }
  if (!FShowInaccessible && Index.data(FileListModel::InaccessibleRole).toBool())
  {
    return false;
  }
  if (FMask.isEmpty() || Index.data(FileListModel::ParentRole).toBool())
  {
    return true;
  }
  const FileListModel * Model = qobject_cast<const FileListModel *>(sourceModel());
  const CoreBridge::TRemoteEntry * Entry = (Model != nullptr) ? Model->Entry(Row) : nullptr;
  if (Entry == nullptr)
  {
    return true;
  }
  try
  {
    return CoreBridge::MaskMatches(FMask.toStdString(), Entry->Name, Entry->Directory, Entry->Size, Entry->ModifiedTime, true);
  }
  catch (const CoreBridge::TCoreError &)
  {
    return true;
  }
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
  // As TUnixDirView's CompareFile: the column, unless directories always go
  // by name; then, the same, the name; all of it reversed for descending.
  int Result = 0;
  if (!(LeftDirectory && FDirectoriesByName) && (Left.column() != FileListModel::NameColumn))
  {
    const QVariant A = Left.data(FileListModel::SortRole);
    const QVariant B = Right.data(FileListModel::SortRole);
    if (A.typeId() == QMetaType::QString)
    {
      Result = CompareText(A.toString(), B.toString());
    }
    else
    {
      Result = (A.toDouble() < B.toDouble()) ? -1 : ((A.toDouble() > B.toDouble()) ? 1 : 0);
    }
  }
  if (Result == 0)
  {
    Result = CompareText(Left.siblingAtColumn(FileListModel::NameColumn).data(FileListModel::SortRole).toString(),
      Right.siblingAtColumn(FileListModel::NameColumn).data(FileListModel::SortRole).toString());
  }
  // The proxy reverses what this says for descending; the reversal of the
  // fallback is wanted too, as upstream's.
  return Result < 0;
}

// CompareLogicalText: case-insensitive, numbers by value when natural order
// numerical sorting is on (StrCmpLogicalW's "file2" before "file10").
int DirectoriesFirst::CompareText(const QString & A, const QString & B) const
{
  if (FNatural)
  {
    return FCollator.compare(A, B);
  }
  return QString::compare(A, B, Qt::CaseInsensitive);
}

// The system's collation, except C's: Qt's C collator compares plainly,
// numbers by their digits, where StrCmpLogicalW compares them by value in
// any locale.
static QLocale CollationLocale()
{
  const QLocale System = QLocale::system();
  return (System.language() == QLocale::C) ? QLocale(QLocale::English, QLocale::UnitedStates) : System;
}

DirectoriesFirst::DirectoriesFirst(QObject * Parent) :
  QSortFilterProxyModel(Parent),
  FCollator(CollationLocale())
{
  FCollator.setNumericMode(true);
  FCollator.setCaseSensitivity(Qt::CaseInsensitive);
}

void DirectoriesFirst::SetSorting(bool Natural, bool DirectoriesByName)
{
  FNatural = Natural;
  FDirectoriesByName = DirectoriesByName;
  invalidate();
}


void FileListModel::SetEntries(std::vector<CoreBridge::TRemoteEntry> Entries, bool WithParent)
{
  beginResetModel();
  FEntries = std::move(Entries);
  FSized.assign(FEntries.size(), false);
  FWithParent = WithParent;
  UpdateColors();
  endResetModel();
}

bool FileListModel::SetDirectorySize(const QString & Name, std::int64_t Size)
{
  for (size_t Index = 0; Index < FEntries.size(); Index++)
  {
    if (FEntries[Index].Directory && (QString::fromStdString(FEntries[Index].Name) == Name))
    {
      FEntries[Index].Size = Size;
      FSized[Index] = true;
      const int Row = static_cast<int>(Index) + (FWithParent ? 1 : 0);
      emit dataChanged(index(Row, SizeColumn), index(Row, SizeColumn));
      return true;
    }
  }
  return false;
}

// The Ext column's: a file's extension, without its dot (ExtractFileExt's
// shown as WinSCP's panels show it); a directory has none.
static QString Extension(const CoreBridge::TRemoteEntry & Entry)
{
  if (Entry.Directory)
  {
    return QString();
  }
  const QString Name = QString::fromStdString(Entry.Name);
  const int Dot = Name.lastIndexOf(QLatin1Char('.'));
  return (Dot > 0) ? Name.mid(Dot + 1) : QString();
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
  if (Index.isValid() && !IsParent(Index.row()))
  {
    // The view starts a drag only from draggable items (FileView::startDrag).
    Result |= Qt::ItemIsDragEnabled;
    if (Index.column() == NameColumn)
    {
      Result |= Qt::ItemIsEditable;
    }
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
        case SizeColumn:
          return (E->Directory && !FSized[Index.row() - (FWithParent ? 1 : 0)]) ? QVariant() : FormatPanelBytes(E->Size, FSizeFormat);
        case ModifiedColumn: return QString::fromStdString(E->Modified);
        case RightsColumn: return QString::fromStdString(E->Rights);
        case OwnerColumn: return QString::fromStdString(E->Owner);
        case GroupColumn: return QString::fromStdString(E->Group);
        case LinkTargetColumn: return QString::fromStdString(E->LinkTarget);
        case ExtColumn: return Extension(*E);
        case TypeColumn: return Integration::FileTypeName(QString::fromStdString(E->Name), E->Directory);
      }
      break;
    case Qt::ForegroundRole:
    {
      const std::size_t Row = static_cast<std::size_t>(Index.row() - (FWithParent ? 1 : 0));
      return ((Row < FColors.size()) && FColors[Row].isValid()) ? QVariant(FColors[Row]) : QVariant();
    }
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
    case InaccessibleRole: return E->Inaccessible;
    case SortRole:
      switch (Index.column())
      {
        case SizeColumn: return static_cast<double>(E->Size);
        case ModifiedColumn: return E->ModifiedTime;
        case RightsColumn: return QString::fromStdString(E->Rights);
        case OwnerColumn: return QString::fromStdString(E->Owner);
        case GroupColumn: return QString::fromStdString(E->Group);
        case LinkTargetColumn: return QString::fromStdString(E->LinkTarget);
        case ExtColumn: return Extension(*E);
        case TypeColumn: return Integration::FileTypeName(QString::fromStdString(E->Name), E->Directory);
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
    case ExtColumn: return tr("Ext");
    case TypeColumn: return tr("Type");
  }
  return QVariant();
}


// --- Selection and incremental search (P3-16) ------------------------------------

void FileView::SetExplorerSelection(CoreBridge::TPreferences::TExplorerSelection Selection)
{
  FExplorerSelection = Selection;
}

void FileView::SetFullRowSelect(bool FullRow)
{
  FDelegate->FullRow = FullRow;
  // The row painted as the cells say.
  setSelectionBehavior(QAbstractItemView::SelectRows);
  viewport()->update();
}

// As TNortonLikeListView: in its modes the arrows (and, Never, plain clicks)
// move the focus and leave the selection; Shift and Ctrl as usual.
QItemSelectionModel::SelectionFlags FileView::selectionCommand(const QModelIndex & Index, const QEvent * Event) const
{
  using TSelection = CoreBridge::TPreferences::TExplorerSelection;
  if (Event != nullptr)
  {
    const bool KeyEvent = (Event->type() == QEvent::KeyPress);
    const bool MouseEvent = (Event->type() == QEvent::MouseButtonPress) || (Event->type() == QEvent::MouseMove);
    Qt::KeyboardModifiers Modifiers = Qt::NoModifier;
    if (KeyEvent)
    {
      Modifiers = static_cast<const QKeyEvent *>(Event)->modifiers() & ~Qt::KeypadModifier;
    }
    else if (MouseEvent)
    {
      Modifiers = static_cast<const QMouseEvent *>(Event)->modifiers();
    }
    const bool Plain = !(Modifiers & (Qt::ShiftModifier | Qt::ControlModifier));
    if (KeyEvent && Plain && (FExplorerSelection != TSelection::MouseAndKeyboard))
    {
      return QItemSelectionModel::NoUpdate;
    }
    if (MouseEvent && Plain && (FExplorerSelection == TSelection::Never))
    {
      return QItemSelectionModel::NoUpdate;
    }
  }
  return QTreeView::selectionCommand(Index, Event);
}

// As TCustomScpExplorerForm's incremental search: typed on, the next entry
// that matches focused (by the name's start, any part of it, or any
// column, as Preferences say); the panel's status says what is typed.
void FileView::keyboardSearch(const QString & Search)
{
  QString Text = FSearch + Search;
  const int Rows = model()->rowCount();
  if (Rows == 0)
  {
    return;
  }
  const int Start = currentIndex().isValid() ? currentIndex().row() : 0;
  auto Matches = [this, &Text](int Row)
  {
    if (FPanelSearch == CoreBridge::TPreferences::TPanelSearch::AllColumns)
    {
      for (int Column = 0; Column < model()->columnCount(); Column++)
      {
        if (!isColumnHidden(Column) && model()->index(Row, Column).data().toString().contains(Text, Qt::CaseInsensitive))
        {
          return true;
        }
      }
      return false;
    }
    const QString Name = model()->index(Row, FileListModel::NameColumn).data().toString();
    return (FPanelSearch == CoreBridge::TPreferences::TPanelSearch::AnyPart) ? Name.contains(Text, Qt::CaseInsensitive) :
      Name.startsWith(Text, Qt::CaseInsensitive);
  };
  // The current one still matching stays; else the next that does.
  for (int Offset = 0; Offset < Rows; Offset++)
  {
    const int Row = (Start + Offset) % Rows;
    if ((EntryAt(Row) != nullptr) && Matches(Row))
    {
      FSearch = Text;
      setCurrentIndex(model()->index(Row, FileListModel::NameColumn));
      if (FExplorerSelection == CoreBridge::TPreferences::TExplorerSelection::MouseAndKeyboard)
      {
        selectionModel()->select(currentIndex(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      }
      scrollTo(currentIndex());
      emit SearchChanged(FSearch);
      return;
    }
  }
  // Nothing matches what is typed: kept as it was, as WinSCP beeps.
  QApplication::beep();
}

void FileView::EndSearch()
{
  if (Searching())
  {
    FSearch.clear();
    FSearching = false;
    emit SearchChanged(QString());
  }
}

void FileView::StartSearch()
{
  if (!Searching())
  {
    FSearching = true;
    emit SearchChanged(QString());
  }
}

void FileView::focusOutEvent(QFocusEvent * Event)
{
  EndSearch();
  QTreeView::focusOutEvent(Event);
}

// --- Thumbnails (P3-15) ---------------------------------------------------------

namespace
{

// Draws a thumbnail's icon: the preview when there is one, else the type's
// icon, as large; the name below.
class TThumbnailDelegate : public QStyledItemDelegate
{
public:
  explicit TThumbnailDelegate(ThumbnailView * View) : QStyledItemDelegate(View), FView(View) {}

protected:
  void initStyleOption(QStyleOptionViewItem * Option, const QModelIndex & Index) const override
  {
    QStyledItemDelegate::initStyleOption(Option, Index);
    const CoreBridge::TRemoteEntry * Entry = FView->EntryAt(Index);
    QIcon Icon;
    if ((Entry != nullptr) && !Entry->Directory && FView->ImagePath)
    {
      const QString Path = FView->ImagePath(*Entry);
      const QPixmap Preview = Path.isEmpty() ? QPixmap() : FView->Preview(Path);
      if (!Preview.isNull())
      {
        Icon = QIcon(Preview);
      }
    }
    if (Icon.isNull() && (Entry != nullptr))
    {
      Icon = TypeIcon(*Entry);
    }
    if (!Icon.isNull())
    {
      Option->icon = Icon;
      Option->features |= QStyleOptionViewItem::HasDecoration;
    }
    Option->decorationSize = QSize(FView->ThumbnailSize(), FView->ThumbnailSize());
    Option->decorationAlignment = Qt::AlignCenter;
  }

private:
  QIcon TypeIcon(const CoreBridge::TRemoteEntry & Entry) const
  {
    if (Entry.Directory)
    {
      return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    }
    static const QMimeDatabase Database;
    const QMimeType Type = Database.mimeTypeForFile(QString::fromStdString(Entry.Name), QMimeDatabase::MatchExtension);
    const QIcon Themed = QIcon::fromTheme(Type.iconName(), QIcon::fromTheme(Type.genericIconName()));
    return Themed.isNull() ? QApplication::style()->standardIcon(QStyle::SP_FileIcon) : Themed;
  }

  ThumbnailView * FView;
};

}  // namespace

ThumbnailView::ThumbnailView(FileView * Details, QWidget * Parent) :
  QListView(Parent), FDetails(Details), FSize(128 * logicalDpiX() / 96)
{
  setViewMode(QListView::IconMode);
  setResizeMode(QListView::Adjust);
  setMovement(QListView::Static);
  setWrapping(true);
  setUniformItemSizes(true);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setIconSize(QSize(FSize, FSize));
  setGridSize(QSize(FSize + 24, FSize + fontMetrics().height() * 2 + 12));
  setTextElideMode(Qt::ElideMiddle);
  setWordWrap(true);
  setModel(Details->model());
  setModelColumn(FileListModel::NameColumn);
  setSelectionModel(Details->selectionModel());
  setItemDelegate(new TThumbnailDelegate(this));
  setContextMenuPolicy(Qt::CustomContextMenu);
  setAccessibleName(Details->accessibleName());
}

const CoreBridge::TRemoteEntry * ThumbnailView::EntryAt(const QModelIndex & Index) const
{
  const QSortFilterProxyModel * Proxy = qobject_cast<const QSortFilterProxyModel *>(model());
  const FileListModel * Files = (Proxy != nullptr) ? qobject_cast<const FileListModel *>(Proxy->sourceModel()) : nullptr;
  return ((Files != nullptr) && Index.isValid()) ? Files->Entry(Proxy->mapToSource(Index).row()) : nullptr;
}

void ThumbnailView::Forget()
{
  FPreviews.clear();
  FPending.clear();
}

QPixmap ThumbnailView::Preview(const QString & Path)
{
  const auto Found = FPreviews.constFind(Path);
  if (Found != FPreviews.constEnd())
  {
    return Found.value();
  }
  if (!FPending.contains(Path))
  {
    FPending.insert(Path);
    QPointer<ThumbnailView> Self(this);
    const int Size = FSize;
    QThreadPool::globalInstance()->start([Self, Path, Size]()
    {
      QImageReader Reader(Path);
      Reader.setAutoTransform(true);
      const QSize Full = Reader.size();
      if (Full.isValid() && ((Full.width() > Size) || (Full.height() > Size)))
      {
        Reader.setScaledSize(Full.scaled(Size, Size, Qt::KeepAspectRatio));
      }
      QImage Image = Reader.read();
      if (!Image.isNull() && ((Image.width() > Size) || (Image.height() > Size)))
      {
        Image = Image.scaled(Size, Size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      }
      // Back on the GUI thread, if the view is still there.
      QMetaObject::invokeMethod(qApp, [Self, Path, Image]()
      {
        if (Self)
        {
          Self->FPending.remove(Path);
          // A file that is no image: its type's icon stays (a null preview).
          Self->FPreviews.insert(Path, Image.isNull() ? QPixmap() : QPixmap::fromImage(Image));
          Self->viewport()->update();
        }
      }, Qt::QueuedConnection);
    });
  }
  return QPixmap();
}
