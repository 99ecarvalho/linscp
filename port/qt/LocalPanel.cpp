#include "LocalPanel.h"
#include <cstdlib>
#include <QToolButton>
#include <QImageReader>
#include <QMenu>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <memory>
#include "DirectoryTree.h"
#include <QScrollBar>
#include <QApplication>
#include "PanelBands.h"
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
#include <QSplitter>
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
  // WinSCP's bands (PanelBands.h): the history and navigation ones are the
  // panel's; its file commands go in the window's Files band.
  FBands.Make(this);
  QToolBar * Bar = FBands[TPanelBand::History];
  auto Add = [this, &Bar](QStyle::StandardPixmap Icon, const QString & Text, const QKeySequence & Key,
    std::function<void()> Handler) -> QAction *
  {
    QAction * Action = (Bar != nullptr) ? Bar->addAction(style()->standardIcon(Icon), Text) :
      new QAction(style()->standardIcon(Icon), Text, this);
    Action->setShortcut(Key);
    Action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(Action, &QAction::triggered, this, Handler);
    addAction(Action);
    return Action;
  };
  FBackAction = Add(QStyle::SP_ArrowBack, tr("&Back"), QKeySequence::Back, [this]() { Back(); });
  FForwardAction = Add(QStyle::SP_ArrowForward, tr("&Forward"), QKeySequence::Forward, [this]() { Forward(); });
  // As HistoryMenu: the buttons' drop-downs.
  for (QAction * Action : { FBackAction, FForwardAction })
  {
    QToolButton * Button = (Bar != nullptr) ? qobject_cast<QToolButton *>(Bar->widgetForAction(Action)) : nullptr;
    if (Button != nullptr)
    {
      // (The panel's: the button goes when hidden, Customize Toolbar.)
      QMenu * Menu = new QMenu(this);
      const bool Back = (Action == FBackAction);
      connect(Menu, &QMenu::aboutToShow, this, [this, Menu, Back]() { FillHistoryMenu(Menu, Back); });
      Button->setMenu(Menu);
      Button->setPopupMode(QToolButton::MenuButtonPopup);
    }
  }
  Bar = FBands[TPanelBand::Navigation];
  FParentAction = Add(QStyle::SP_FileDialogToParent, tr("&Parent Directory"), QKeySequence(Qt::Key_Backspace),
    [this]() { LocalPanel::Parent(); });
  // As TCustomDirView.KeyDown: Alt+Up and Ctrl+PgUp too.
  FParentAction->setShortcuts({ QKeySequence(Qt::Key_Backspace), QKeySequence(Qt::ALT | Qt::Key_Up),
    QKeySequence(Qt::CTRL | Qt::Key_PageUp) });
  FRootAction = Add(QStyle::SP_DriveHDIcon, tr("&Root Directory"), QKeySequence(Qt::CTRL | Qt::Key_Backslash), [this]() { Root(); });
  FHomeAction = Add(QStyle::SP_DirHomeIcon, tr("&Home Directory"), QKeySequence(Qt::CTRL | Qt::Key_H), [this]() { Home(); });
  FRefreshAction = Add(QStyle::SP_BrowserReload, tr("&Refresh"), QKeySequence(Qt::CTRL | Qt::Key_R), [this]() { Refresh(); });
  Bar = nullptr;
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

  FPath = new QLineEdit(this);
  FPath->setAccessibleName(tr("Local path"));
  connect(FPath, &QLineEdit::returnPressed, this, [this]() { ChangeDirectory(FPath->text().trimmed()); });
  FBands.LayOut(Layout, FPath);

  FModel = new FileListModel(this);
#if defined(Q_OS_WIN)
  FModel->SetAttributes(true);
#endif
  FSort = new DirectoriesFirst(this);
  FSort->setSourceModel(FModel);
  FSort->setSortRole(FileListModel::SortRole);
  FView = new FileView(this);
  connect(FView, &FileView::SearchChanged, this, [this]() { UpdateActions(); });
  FView->setAccessibleName(tr("Local files"));
  FView->setModel(FSort);
  FView->SetupColumns(true);
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
  // As the Commander's DriveView: the tree above the files (Options >
  // Panel > Tree), hidden by default.
  FSplitter = new QSplitter(Qt::Vertical, this);
  FSplitter->setChildrenCollapsible(false);
  FTree = new LocalDirectoryTree(FSplitter);
  FTree->hide();
  FSplitter->addWidget(FTree);
  FSplitter->addWidget(FView);
  FThumbs = new ThumbnailView(FView, FSplitter);
  FThumbs->hide();
  FSplitter->addWidget(FThumbs);
  FSplitter->setStretchFactor(2, 1);
  connect(FThumbs, &QListView::activated, this, &LocalPanel::Activated);
  // Previews of the files Qt reads as images.
  FThumbs->ImagePath = [this](const CoreBridge::TRemoteEntry & Entry)
  {
    static const QList<QByteArray> Formats = QImageReader::supportedImageFormats();
    const QString Name = QString::fromStdString(Entry.Name);
    return Formats.contains(QFileInfo(Name).suffix().toLower().toLatin1()) ? QDir(FCurrent).filePath(Name) : QString();
  };
  connect(FThumbs, &QWidget::customContextMenuRequested, this, &LocalPanel::ContextMenu);
  FSplitter->setStretchFactor(1, 1);
  connect(FTree, &LocalDirectoryTree::DirectoryChosen, this, [this](const QString & Path)
  {
    if (QDir::cleanPath(Path) != QDir::cleanPath(this->Path()))
    {
      ChangeDirectory(Path);
    }
  });
  Layout->addWidget(FSplitter, 1);
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
  if (FCurrent != Target)
  {
    FView->ForgetSelectedNames();
    FThumbs->Forget();
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
  ShowTreePath();
  emit Changed();
}

void LocalPanel::ShowTreePath()
{
  if (TreeVisible())
  {
    FTree->ShowPath(FCurrent);
  }
}

QMenu * LocalPanel::PathMenu()
{
  QMenu * Menu = new QMenu(this);
  std::vector<std::pair<QString, QString>> Places = {
    { QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SpecialFolderDocuments)),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) },
    { QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SpecialFolderDesktop)),
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) } };
#if defined(Q_OS_WIN)
  for (const QFileInfo & Drive : QDir::drives())
  {
    const QString Root = Drive.absoluteFilePath();
    const QStorageInfo Info(Root);
    const QString Letter = QDir::toNativeSeparators(Root).left(2);
    // As GetPrettyName: "C: Label", the letter its accelerator.
    Places.push_back({ QStringLiteral("&") + Letter + (Info.displayName().isEmpty() || (Info.displayName() == Root) ?
      QString() : (QLatin1Char(' ') + Info.displayName())), Root });
  }
#else
  Places.push_back({ QStringLiteral("/"), QStringLiteral("/") });
  Places.push_back({ QDir::homePath(), QDir::homePath() });
  for (const QStorageInfo & Volume : QStorageInfo::mountedVolumes())
  {
    const QString Root = Volume.rootPath();
    if (Volume.isValid() && Volume.isReady() &&
        (Root.startsWith(QStringLiteral("/media/")) || Root.startsWith(QStringLiteral("/mnt/")) ||
         Root.startsWith(QStringLiteral("/run/media/"))))
    {
      Places.push_back({ Volume.displayName().isEmpty() ? Root : QStringLiteral("%1 (%2)").arg(Volume.displayName(), Root), Root });
    }
  }
#endif
  // The current one: the longest that the path starts with.
  int Current = -1;
  int Longest = -1;
  const QString Here = QDir::cleanPath(FCurrent);
  for (int Index = 0; Index < static_cast<int>(Places.size()); Index++)
  {
    const QString Path = QDir::cleanPath(Places[Index].second);
    const bool Under = !Path.isEmpty() && ((Here.compare(Path, Qt::CaseInsensitive) == 0) ||
      Here.startsWith(Path.endsWith(QLatin1Char('/')) ? Path : (Path + QLatin1Char('/')), Qt::CaseInsensitive));
    if (Under && (Path.length() > Longest))
    {
      Longest = Path.length();
      Current = Index;
    }
  }
  for (int Index = 0; Index < static_cast<int>(Places.size()); Index++)
  {
    if (Places[Index].second.isEmpty())
    {
      continue;
    }
    QAction * Action = Menu->addAction(style()->standardIcon((Index < 2) ? QStyle::SP_DirHomeIcon : QStyle::SP_DriveHDIcon),
      Places[Index].first);
    Action->setCheckable(true);
    Action->setChecked(Index == Current);
    const QString Path = Places[Index].second;
    connect(Action, &QAction::triggered, this, [this, Path]() { ChangeDirectory(Path); });
  }
  return Menu;
}

void LocalPanel::ShowPathMenu()
{
  std::unique_ptr<QMenu> Menu(PathMenu());
  Menu->exec(FPath->mapToGlobal(QPoint(0, FPath->height())));
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

bool LocalPanel::HistoryGo(int Index)
{
  QStringList & From = (Index < 0) ? FBack : FForward;
  QStringList & To = (Index < 0) ? FForward : FBack;
  const int Steps = std::abs(Index);
  if ((Steps == 0) || (Steps > From.size()))
  {
    return false;
  }
  const QString Target = From[From.size() - Steps];
  const QString Here = FCurrent;
  if (!Go(Target, false))
  {
    return false;
  }
  To.append(Here);
  for (int Step = 1; Step < Steps; Step++)
  {
    To.append(From.takeLast());
  }
  From.removeLast();
  UpdateActions();
  return true;
}

void LocalPanel::FillHistoryMenu(QMenu * Menu, bool Back)
{
  Menu->clear();
  Menu->setToolTipsVisible(true);
  const QStringList & List = Back ? FBack : FForward;
  for (int Step = 1; Step <= std::min<int>(List.size(), 10); Step++)
  {
    const QString Path = List[List.size() - Step];
    QAction * Action = Menu->addAction(QString::fromStdString(CoreBridge::MinimizeName(Path.toStdString(), 50, false)));
    Action->setToolTip(Path);
    const int Index = Back ? -Step : Step;
    connect(Action, &QAction::triggered, this, [this, Index]() { HistoryGo(Index); });
  }
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
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  const bool ToTrash = ((DeleteToTrash && Preferences.DeleteToRecycleBin) != Alternative);
  const QStringList Names = Selected();
  if (Names.isEmpty())
  {
    return false;
  }
  // As DeleteFiles: Environment's confirmation for that, with "Never ask
  // me again" turning it off.
  if (Confirm && (ToTrash ? Preferences.ConfirmRecycling : Preferences.ConfirmDeleting))
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
    Query.NeverAskAgainCheck = true;
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
    if (Answer.Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
    if (Answer.NeverAskAgain)
    {
      CoreBridge::ChangePreferences([ToTrash](CoreBridge::TPreferences & Changed)
      {
        (ToTrash ? Changed.ConfirmRecycling : Changed.ConfirmDeleting) = false;
      });
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
    const QModelIndex Index = FocusView()->indexAt(Pos);
    const QModelIndex Source = Index.isValid() ? FSort->mapToSource(Index) : QModelIndex();
    const bool OnItem = Source.isValid() && !FModel->IsParent(Source.row()) && (FModel->Entry(Source.row()) != nullptr);
    OnContextMenu(FocusView()->viewport()->mapToGlobal(Pos), OnItem);
    return;
  }
  QMenu Menu(this);
  Menu.addAction(FNewFolderAction);
  Menu.addAction(FRenameAction);
  Menu.addAction(FDeleteAction);
  Menu.addSeparator();
  Menu.addAction(FRefreshAction);
  Menu.exec(FocusView()->viewport()->mapToGlobal(Pos));
}

void LocalPanel::SetShowHidden(bool Show)
{
  FSort->SetShowHidden(Show);
  FTree->SetShowHidden(Show);
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
  // As CreateFileList: none selected, the focused one.
  if (Result.isEmpty() && (FView->CurrentEntry() != nullptr))
  {
    Result.append(QString::fromStdString(FView->CurrentEntry()->Name));
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
  // While searching, what is typed (FormatIncrementalSearchStatus).
  if (FView->Searching())
  {
    // "(start typing)" when nothing is yet.
    FStatus->setText(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::IncrementalSearch))
      .replace(QStringLiteral("%s"), !FView->SearchText().isEmpty() ? FView->SearchText() :
        QString::fromStdString(CoreBridge::Text(CoreBridge::TText::IncrementalSearchType))));
  }
  else
  {
    FStatus->setText(FView->StatusText());
  }
  const int Count = Selected().size();
  FBackAction->setEnabled(!FBack.isEmpty());
  FForwardAction->setEnabled(!FForward.isEmpty());
  FParentAction->setEnabled(!QDir(FCurrent).isRoot());
  FRenameAction->setEnabled(Count == 1);
  FDeleteAction->setEnabled(Count > 0);
}

void LocalPanel::ApplyPreferences(const CoreBridge::TPreferences & Preferences)
{
  FView->SetExplorerSelection(Preferences.ExplorerSelection);
  FView->SetFullRowSelect(Preferences.FullRowSelect);
  FView->SetPanelSearch(Preferences.PanelSearch);
  // The panel font (none: the system's).
  QFont Font = QApplication::font();
  if (!Preferences.PanelFontName.empty())
  {
    Font.setFamily(QString::fromStdString(Preferences.PanelFontName));
    if (Preferences.PanelFontSize > 0)
    {
      Font.setPointSize(Preferences.PanelFontSize);
    }
    Font.setBold(Preferences.PanelFontBold);
    Font.setItalic(Preferences.PanelFontItalic);
  }
  FView->setFont(Font);
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
static const std::vector<int> WinScpColumns = { 0, 1, 3, 4, -1, -1, -1, 5, 2 };

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

TPanelState LocalPanel::State() const
{
  TPanelState Result;
  if (const CoreBridge::TRemoteEntry * Focused = FView->CurrentEntry())
  {
    Result.Focused = QString::fromStdString(Focused->Name);
  }
  for (const QModelIndex & Index : FView->selectionModel()->selectedRows(FileListModel::NameColumn))
  {
    Result.Selected.append(Index.data(Qt::DisplayRole).toString());
  }
  Result.Scroll = FView->verticalScrollBar()->value();
  return Result;
}

void LocalPanel::RestoreState(const TPanelState & State)
{
  FView->selectionModel()->clearSelection();
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    const QModelIndex Index = FSort->index(Row, FileListModel::NameColumn);
    const QString Name = Index.data(Qt::DisplayRole).toString();
    if (Index.data(FileListModel::ParentRole).toBool())
    {
      continue;
    }
    if (State.Selected.contains(Name))
    {
      FView->selectionModel()->select(Index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    if (Name == State.Focused)
    {
      FView->selectionModel()->setCurrentIndex(Index, QItemSelectionModel::NoUpdate);
    }
  }
  FView->verticalScrollBar()->setValue(State.Scroll);
}

QStringList LocalPanel::Selection() const
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

void LocalPanel::SetTreeVisible(bool Visible)
{
  FTree->setVisible(Visible);
  if (Visible)
  {
    ShowTreePath();
  }
}

bool LocalPanel::TreeVisible() const
{
  return !FTree->isHidden();
}

int LocalPanel::TreeHeight() const
{
  return FSplitter->sizes().value(0);
}

// As the Commander's TreeOnLeft: the tree beside the files.
void LocalPanel::SetTreeBeside(bool Beside)
{
  FSplitter->setOrientation(Beside ? Qt::Horizontal : Qt::Vertical);
  FSplitter->setStretchFactor(0, Beside ? 0 : 1);
}

bool LocalPanel::TreeBeside() const
{
  return FSplitter->orientation() == Qt::Horizontal;
}

void LocalPanel::SetTreeHeight(int Height)
{
  const int Total = std::max(TreeBeside() ? FSplitter->width() : FSplitter->height(), Height + 100);
  FSplitter->setSizes({ Height, Total - Height });
}

void LocalPanel::SetThumbnails(bool Thumbnails)
{
  const bool Focused = FocusView()->hasFocus();
  FView->setVisible(!Thumbnails);
  FThumbs->setVisible(Thumbnails);
  if (Focused)
  {
    FocusView()->setFocus();
  }
}

bool LocalPanel::Thumbnails() const
{
  return !FThumbs->isHidden();
}

QAbstractItemView * LocalPanel::FocusView() const
{
  return Thumbnails() ? static_cast<QAbstractItemView *>(FThumbs) : static_cast<QAbstractItemView *>(FView);
}
