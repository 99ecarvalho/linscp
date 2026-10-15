#include "LocalPanel.h"
#include "IconSet.h"

#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QStorageInfo>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include "CreateDirectoryDialog.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

// Core's text with its "%s"s filled in, in order.
QString CoreText(CoreBridge::TText Text, const QStringList & Args)
{
  QString Result = CoreText(Text);
  for (const QString & Arg : Args)
  {
    const int At = Result.indexOf(QStringLiteral("%s"));
    if (At >= 0)
    {
      Result.replace(At, 2, Arg);
    }
  }
  return Result;
}

// The entry the shared model shows for a file. Rights: rwx on POSIX; on
// Windows the attributes WinSCP's local pane shows (r, h, s, a).
CoreBridge::TRemoteEntry EntryOf(const QFileInfo & Info)
{
  CoreBridge::TRemoteEntry Entry;
  Entry.Name = Info.fileName().toStdString();
  Entry.Directory = Info.isDir();
  Entry.Size = Info.isDir() ? 0 : Info.size();
  const QDateTime Modified = Info.lastModified();
  Entry.Modified = QLocale().toString(Modified, QLocale::ShortFormat).toStdString();
  Entry.ModifiedTime = static_cast<double>(Modified.toSecsSinceEpoch());
#if defined(Q_OS_WIN)
  QString Attributes;
  if (!Info.isWritable()) Attributes += QLatin1Char('r');
  if (Info.isHidden()) Attributes += QLatin1Char('h');
  Entry.Rights = Attributes.toStdString();
#else
  const QFile::Permissions P = Info.permissions();
  const auto Bit = [&P](QFile::Permission Flag, char Letter) { return (P & Flag) ? Letter : '-'; };
  Entry.Rights = std::string({ Bit(QFile::ReadOwner, 'r'), Bit(QFile::WriteOwner, 'w'), Bit(QFile::ExeOwner, 'x'),
    Bit(QFile::ReadGroup, 'r'), Bit(QFile::WriteGroup, 'w'), Bit(QFile::ExeGroup, 'x'),
    Bit(QFile::ReadOther, 'r'), Bit(QFile::WriteOther, 'w'), Bit(QFile::ExeOther, 'x') });
  Entry.Owner = Info.owner().toStdString();
  Entry.Group = Info.group().toStdString();
#endif
  Entry.Link = Info.isSymLink();
  Entry.LinkTarget = Info.symLinkTarget().toStdString();
  Entry.Hidden = Info.isHidden();
  return Entry;
}

}  // namespace

LocalPanel::LocalPanel(QWidget * Parent) :
  QWidget(Parent)
{
  OpenFile = [](const QString & FileName) { QDesktopServices::openUrl(QUrl::fromLocalFile(FileName)); };

  QVBoxLayout * Layout = new QVBoxLayout(this);
  Layout->setContentsMargins(0, 0, 0, 0);
  QToolBar * Bar = new QToolBar(this);
  Bar->setIconSize(QSize(16, 16));
  auto Add = [this, Bar](QStyle::StandardPixmap Icon, const QString & Text, const QKeySequence & Key,
    std::function<void()> Handler) -> QAction *
  {
    QAction * Action = Bar->addAction(style()->standardIcon(Icon), Text);
    Action->setShortcut(Key);
    Action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(Action, &QAction::triggered, this, Handler);
    addAction(Action);
    return Action;
  };
  FBackAction = Add(QStyle::SP_ArrowBack, tr("&Back"), QKeySequence::Back, [this]() { Back(); });
  FForwardAction = Add(QStyle::SP_ArrowForward, tr("&Forward"), QKeySequence::Forward, [this]() { Forward(); });
  FParentAction = Add(QStyle::SP_FileDialogToParent, tr("&Parent Directory"), QKeySequence(Qt::Key_Backspace),
    [this]() { LocalPanel::Parent(); });
  // As TCustomDirView.KeyDown: Alt+Up and Ctrl+PgUp too.
  FParentAction->setShortcuts({ QKeySequence(Qt::Key_Backspace), QKeySequence(Qt::ALT | Qt::Key_Up),
    QKeySequence(Qt::CTRL | Qt::Key_PageUp) });
  FRootAction = Add(QStyle::SP_DriveHDIcon, tr("&Root Directory"), QKeySequence(Qt::CTRL | Qt::Key_Backslash), [this]() { Root(); });
  FHomeAction = Add(QStyle::SP_DirHomeIcon, tr("&Home Directory"), QKeySequence(Qt::CTRL | Qt::Key_H), [this]() { Home(); });
  FRefreshAction = Add(QStyle::SP_BrowserReload, tr("&Refresh"), QKeySequence(Qt::CTRL | Qt::Key_R), [this]() { Refresh(); });
  Bar->addSeparator();
  FNewFolderAction = Add(QStyle::SP_FileDialogNewFolder, tr("Create Directory"), QKeySequence(Qt::Key_F7), [this]()
  {
    NewFolderDialog();
  });
  FRenameAction = Add(QStyle::SP_FileDialogDetailedView, tr("Rename"), QKeySequence(Qt::Key_F2), [this]()
  {
    FView->EditCurrentName();
  });
  FDeleteAction = Add(QStyle::SP_TrashIcon, tr("Delete"), QKeySequence::Delete, [this]() { DeleteSelected(true); });
  FDeleteAction->setShortcuts({ QKeySequence::Delete, QKeySequence(Qt::Key_F8) });
  QAction * DeleteAlternative = new QAction(this);
  DeleteAlternative->setShortcuts({ QKeySequence(Qt::SHIFT | Qt::Key_Delete), QKeySequence(Qt::SHIFT | Qt::Key_F8) });
  DeleteAlternative->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  connect(DeleteAlternative, &QAction::triggered, this, [this]() { DeleteSelected(true, true); });
  addAction(DeleteAlternative);
  Layout->addWidget(Bar);

  FPath = new QLineEdit(this);
  FPath->setAccessibleName(tr("Local path"));
  connect(FPath, &QLineEdit::returnPressed, this, [this]() { ChangeDirectory(FPath->text().trimmed()); });
  Layout->addWidget(FPath);

  FModel = new FileListModel(this);
#if defined(Q_OS_WIN)
  FModel->SetAttributes(true);
#endif
  FSort = new DirectoriesFirst(this);
  FSort->setSourceModel(FModel);
  FSort->setSortRole(FileListModel::SortRole);
  FView = new FileView(this);
  FView->setAccessibleName(tr("Local files"));
  FView->setModel(FSort);
  FView->SetupColumns();
  FView->setRootIsDecorated(false);
  FView->setItemsExpandable(false);
  FView->setUniformRowHeights(true);
  FView->setAllColumnsShowFocus(true);
  FView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  FView->setSortingEnabled(true);
  FView->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
  FView->header()->setSectionResizeMode(FileListModel::NameColumn, QHeaderView::Stretch);
  FView->setContextMenuPolicy(Qt::CustomContextMenu);
  FView->MakeDrag = [this]() -> QMimeData *
  {
    const QStringList Names = Selected();
    return Names.isEmpty() ? nullptr : MakeFileDrag({ true, FCurrent, Names });
  };
  // After the editor has closed: renaming lists the directory again.
  connect(FModel, &FileListModel::RenameRequested, this, [this](const QString & Name, const QString & NewName)
  {
    QTimer::singleShot(0, this, [this, Name, NewName]() { Rename(Name, NewName); });
  });
  connect(FView, &QTreeView::activated, this, &LocalPanel::Activated);
  connect(FView, &QWidget::customContextMenuRequested, this, &LocalPanel::ContextMenu);
  connect(FView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &LocalPanel::UpdateActions);
  Layout->addWidget(FView, 1);
  FStatus = new QLabel(this);
  Layout->addWidget(FStatus);

  // Kept up to date as the directory changes, as WinSCP's local pane is:
  // changes coalesced, so a burst of them reads the directory once.
  FWatcher = new QFileSystemWatcher(this);
  FReloadTimer = new QTimer(this);
  FReloadTimer->setSingleShot(true);
  FReloadTimer->setInterval(200);
  connect(FWatcher, &QFileSystemWatcher::directoryChanged, FReloadTimer, qOverload<>(&QTimer::start));
  connect(FReloadTimer, &QTimer::timeout, this, &LocalPanel::Reload);

  Go(QDir::homePath(), false);
}

void LocalPanel::Fail(const QString & Message)
{
  CoreBridge::TError Error;
  Error.Message = Message.toStdString();
  QtFrontend Frontend;
  Frontend.SetWindow(window());
  Frontend.ShowError(Error);
}

bool LocalPanel::Go(const QString & Directory, bool Record)
{
  const QString Target = QDir::cleanPath(QDir(FCurrent).absoluteFilePath(QDir::fromNativeSeparators(Directory)));
  const QFileInfo Info(Target);
  if (!Info.isDir() || !QDir(Target).isReadable())
  {
    Fail(CoreText(CoreBridge::TText::ChangeDirError, { QDir::toNativeSeparators(Target) }));
    FPath->setText(QDir::toNativeSeparators(FCurrent));
    return false;
  }
  if (Target == FCurrent)
  {
    return false;
  }
  if (Record && !FCurrent.isEmpty())
  {
    FBack.append(FCurrent);
    FForward.clear();
  }
  if (!FCurrent.isEmpty())
  {
    FWatcher->removePath(FCurrent);
  }
  FCurrent = Target;
  FWatcher->addPath(FCurrent);
  FView->selectionModel()->clearSelection();
  Reload();
  return true;
}

void LocalPanel::Reload()
{
  const QStringList Keep = Selected();
  std::vector<CoreBridge::TRemoteEntry> Entries;
  const QFileInfoList Infos = QDir(FCurrent).entryInfoList(
    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::NoSort);
  for (const QFileInfo & Info : Infos)
  {
    Entries.push_back(EntryOf(Info));
  }
  FModel->SetEntries(std::move(Entries), !QDir(FCurrent).isRoot());
  FPath->setText(QDir::toNativeSeparators(FCurrent));
  Select(Keep);
  UpdateActions();
  emit Changed();
}

bool LocalPanel::ChangeDirectory(const QString & Directory)
{
  return Go(Directory, true);
}

bool LocalPanel::Back()
{
  if (FBack.isEmpty())
  {
    return false;
  }
  const QString Here = FCurrent;
  const QString Target = FBack.takeLast();
  if (!Go(Target, false))
  {
    FBack.append(Target);
    return false;
  }
  FForward.append(Here);
  UpdateActions();
  return true;
}

bool LocalPanel::Forward()
{
  if (FForward.isEmpty())
  {
    return false;
  }
  const QString Here = FCurrent;
  const QString Target = FForward.takeLast();
  if (!Go(Target, false))
  {
    FForward.append(Target);
    return false;
  }
  FBack.append(Here);
  UpdateActions();
  return true;
}

bool LocalPanel::Parent()
{
  return !QDir(FCurrent).isRoot() && ChangeDirectory(QString::fromLatin1(ParentEntryName));
}

// The root of the current volume: its drive on Windows, "/" elsewhere.
bool LocalPanel::Root()
{
  const QString Root = QStorageInfo(FCurrent).rootPath();
  return ChangeDirectory(Root.isEmpty() ? QStringLiteral("/") : Root);
}

bool LocalPanel::Home()
{
  return ChangeDirectory(HomeDirectory.isEmpty() ? QDir::homePath() : HomeDirectory);
}

bool LocalPanel::Refresh()
{
  Reload();
  return true;
}

bool LocalPanel::NewFolderDialog()
{
  CreateDirectoryDialog Dialog(CoreText(CoreBridge::TText::NewFolderName), false, false, CoreBridge::TNewDirectory(), this);
  return (Dialog.exec() == QDialog::Accepted) && NewFolder(Dialog.Name().trimmed());
}

bool LocalPanel::NewFolder(const QString & Name)
{
  if (!QDir(FCurrent).mkdir(Name))
  {
    Fail(CoreText(CoreBridge::TText::CreateDirError, { Name }));
    return false;
  }
  Reload();
  Select({ Name });
  return true;
}

bool LocalPanel::Rename(const QString & Name, const QString & NewName)
{
  if (QFileInfo::exists(QDir(FCurrent).filePath(NewName)) || !QDir(FCurrent).rename(Name, NewName))
  {
    Fail(CoreText(CoreBridge::TText::RenameFileError, { Name, NewName }));
    return false;
  }
  Reload();
  Select({ NewName });
  return true;
}

bool LocalPanel::MoveInto(const QStringList & Names, const QString & Directory)
{
  if (Names.isEmpty() || Directory.isEmpty() || Names.contains(Directory))
  {
    return false;
  }
  const QDir Current(FCurrent);
  const QDir Target(Current.filePath(Directory));
  bool Result = true;
  for (const QString & Name : Names)
  {
    const QString NewName = Target.filePath(Name);
    if (QFileInfo::exists(NewName) || !QDir().rename(Current.filePath(Name), NewName))
    {
      Fail(CoreText(CoreBridge::TText::RenameFileError, { Name, QDir::toNativeSeparators(NewName) }));
      Result = false;
      break;
    }
  }
  Reload();
  return Result;
}

// As WinSCP's local delete by default: to the recycle bin (the trash), with
// core's confirmation for that; where the system has no trash, for good,
// with the plain one.
bool LocalPanel::DeleteSelected(bool Confirm, bool Alternative)
{
  // WinSCP's alternative delete (Shift+Del): the other of the two.
  const bool ToTrash = (DeleteToTrash != Alternative);
  const QStringList Names = Selected();
  if (Names.isEmpty())
  {
    return false;
  }
  if (Confirm)
  {
    const bool One = (Names.size() == 1);
    QString Message = CoreText(ToTrash ?
      (One ? CoreBridge::TText::ConfirmRecycleFile : CoreBridge::TText::ConfirmRecycleFiles) :
      (One ? CoreBridge::TText::ConfirmDeleteFile : CoreBridge::TText::ConfirmDeleteFiles));
    Message = One ? Message.replace(QStringLiteral("%s"), Names.first()) :
      Message.replace(QStringLiteral("%d"), QString::number(Names.size()));
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = Message.toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
  }
  bool Result = true;
  for (const QString & Name : Names)
  {
    const QString Path = QDir(FCurrent).filePath(Name);
    const bool Directory = QFileInfo(Path).isDir() && !QFileInfo(Path).isSymLink();
    if (!(ToTrash && QFile::moveToTrash(Path)) &&
        !(Directory ? QDir(Path).removeRecursively() : QFile::remove(Path)))
    {
      Fail(CoreText(CoreBridge::TText::DeleteLocalFileError, { Name }));
      Result = false;
      break;
    }
  }
  Reload();
  return Result;
}

void LocalPanel::Open(const QString & Name)
{
  if (Name == QString::fromLatin1(ParentEntryName))
  {
    LocalPanel::Parent();
    return;
  }
  const QString Path = QDir(FCurrent).filePath(Name);
  if (QFileInfo(Path).isDir())
  {
    ChangeDirectory(Name);
  }
  else if (OpenFile)
  {
    OpenFile(Path);
  }
}

void LocalPanel::Activated(const QModelIndex & Index)
{
  const QModelIndex Source = FSort->mapToSource(Index);
  Open(FModel->IsParent(Source.row()) ? QString::fromLatin1(ParentEntryName) :
    QString::fromStdString(FModel->Entry(Source.row())->Name));
}

void LocalPanel::ContextMenu(const QPoint & Pos)
{
  if (OnContextMenu)
  {
    const QModelIndex Index = FView->indexAt(Pos);
    const QModelIndex Source = Index.isValid() ? FSort->mapToSource(Index) : QModelIndex();
    const bool OnItem = Source.isValid() && !FModel->IsParent(Source.row()) && (FModel->Entry(Source.row()) != nullptr);
    OnContextMenu(FView->viewport()->mapToGlobal(Pos), OnItem);
    return;
  }
  QMenu Menu(this);
  Menu.addAction(FNewFolderAction);
  Menu.addAction(FRenameAction);
  Menu.addAction(FDeleteAction);
  Menu.addSeparator();
  Menu.addAction(FRefreshAction);
  Menu.exec(FView->viewport()->mapToGlobal(Pos));
}

void LocalPanel::SetShowHidden(bool Show)
{
  FSort->SetShowHidden(Show);
  UpdateActions();
}

QStringList LocalPanel::Selected() const
{
  QStringList Result;
  for (const QModelIndex & Index : FView->selectionModel()->selectedRows(FileListModel::NameColumn))
  {
    const QModelIndex Source = FSort->mapToSource(Index);
    if (!FModel->IsParent(Source.row()) && (FModel->Entry(Source.row()) != nullptr))
    {
      Result.append(QString::fromStdString(FModel->Entry(Source.row())->Name));
    }
  }
  return Result;
}

void LocalPanel::Select(const QStringList & Names)
{
  FView->selectionModel()->clearSelection();
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    const QModelIndex Index = FSort->index(Row, FileListModel::NameColumn);
    if (Names.contains(Index.data(Qt::DisplayRole).toString()) && !Index.data(FileListModel::ParentRole).toBool())
    {
      FView->selectionModel()->select(Index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      FView->selectionModel()->setCurrentIndex(Index, QItemSelectionModel::NoUpdate);
    }
  }
}

QStringList LocalPanel::Names() const
{
  QStringList Result;
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    Result.append(FSort->index(Row, FileListModel::NameColumn).data(Qt::DisplayRole).toString());
  }
  return Result;
}

void LocalPanel::UpdateActions()
{
  FStatus->setText(FView->StatusText());
  const int Count = Selected().size();
  FBackAction->setEnabled(!FBack.isEmpty());
  FForwardAction->setEnabled(!FForward.isEmpty());
  FParentAction->setEnabled(!QDir(FCurrent).isRoot());
  FRenameAction->setEnabled(Count == 1);
  FDeleteAction->setEnabled(Count > 0);
}

void LocalPanel::ApplyPreferences(const CoreBridge::TPreferences & Preferences)
{
  FModel->SetSizeFormat(Preferences.SizeFormat);
  FSort->SetSorting(Preferences.NaturalOrderNumericalSorting, Preferences.AlwaysSortDirectoriesByName);
  FView->RenameWholeName = Preferences.RenameWholeName;
  FRefreshAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::Key_F5) : QKeySequence(Qt::CTRL | Qt::Key_R));
  FModel->SetFileColors(Preferences.FileColors, true,
    [this](const CoreBridge::TRemoteEntry & Entry)
    {
      return QDir::toNativeSeparators(QDir(FCurrent).filePath(QString::fromStdString(Entry.Name))).toStdString();
    });
  SetShowHidden(Preferences.ShowHiddenFiles);
}

// WinSCP's columns: name, size, type, changed, attributes, extension (TDirView); owner, group and link target are not among them.
static const std::vector<int> WinScpColumns = { 0, 1, 3, 4, -1, -1, -1 };

void LocalPanel::SetColumnsParams(const QString & Params)
{
  FView->SetColumnsParams(Params, WinScpColumns, 6);
}

QString LocalPanel::ColumnsParams() const
{
  return FView->ColumnsParams(WinScpColumns, 6);
}

QStringList LocalPanel::History() const
{
  QStringList Result;
  for (const QString & Directory : FBack + QStringList{ FCurrent } + QStringList(FForward.rbegin(), FForward.rend()))
  {
    Result.append(QDir::toNativeSeparators(Directory));
  }
  Result.removeDuplicates();
  Result.removeAll(QString());
  return Result;
}

void LocalPanel::SetFilter(const QString & Mask)
{
  FSort->SetMask(Mask);
  UpdateActions();
}

QString LocalPanel::Filter() const
{
  return FSort->Mask();
}

std::vector<CoreBridge::TRemoteEntry> LocalPanel::Entries() const
{
  std::vector<CoreBridge::TRemoteEntry> Result;
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    const CoreBridge::TRemoteEntry * Entry = FModel->Entry(FSort->mapToSource(FSort->index(Row, 0)).row());
    if (Entry != nullptr)
    {
      Result.push_back(*Entry);
    }
  }
  return Result;
}

void LocalPanel::SetDirectorySize(const QString & Name, std::int64_t Size)
{
  FModel->SetDirectorySize(Name, Size);
}

// The set's icons (IconSet), else -- with none -- Qt's own, the panel's
// toolbar being of icons.
void LocalPanel::ApplyIcons()
{
  const std::pair<QAction *, std::pair<const char *, QStyle::StandardPixmap>> Icons[] = {
    { FBackAction, { "back", QStyle::SP_ArrowBack } },
    { FForwardAction, { "forward", QStyle::SP_ArrowForward } },
    { FParentAction, { "parent", QStyle::SP_FileDialogToParent } },
    { FRootAction, { "root", QStyle::SP_DriveHDIcon } },
    { FHomeAction, { "home", QStyle::SP_DirHomeIcon } },
    { FRefreshAction, { "refresh", QStyle::SP_BrowserReload } },
    { FNewFolderAction, { "new-folder", QStyle::SP_FileDialogNewFolder } },
    { FRenameAction, { "rename", QStyle::SP_FileDialogDetailedView } },
    { FDeleteAction, { "delete", QStyle::SP_TrashIcon } },
  };
  for (const auto & Icon : Icons)
  {
    const QIcon Set = IconSet::Icon(QString::fromLatin1(Icon.second.first));
    Icon.first->setIcon(Set.isNull() ? style()->standardIcon(Icon.second.second) : Set);
  }
}
