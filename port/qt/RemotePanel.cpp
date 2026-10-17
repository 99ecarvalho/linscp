#include "RemotePanel.h"
#include <QListView>
#include "DirectoryTree.h"
#include <QScrollBar>
#include "PanelBands.h"
#include "IconSet.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QSplitter>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <functional>
#include <memory>

#include "CreateDirectoryDialog.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

RemotePanel::RemotePanel(QWidget * Parent) :
  QWidget(Parent)
{
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
  Bar = FBands[TPanelBand::Navigation];
  FParentAction = Add(QStyle::SP_FileDialogToParent, tr("&Parent Directory"), QKeySequence(Qt::Key_Backspace), [this]() { RemotePanel::Parent(); });
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

  FPath = new QLineEdit(this);
  FPath->setAccessibleName(tr("Remote path"));
  connect(FPath, &QLineEdit::returnPressed, this, [this]() { ChangeDirectory(FPath->text().trimmed()); });
  FBands.LayOut(Layout, FPath);

  FModel = new FileListModel(this);
  FSort = new DirectoriesFirst(this);
  FSort->setSourceModel(FModel);
  FSort->setSortRole(FileListModel::SortRole);
  FView = new FileView(this);
  connect(FView, &FileView::SearchChanged, this, [this]() { UpdateActions(); });
  FView->setAccessibleName(tr("Remote files"));
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
    if (Names.isEmpty())
    {
      return nullptr;
    }
    const TFileDrag Drag{ false, FCurrent, Names };
    return MakeDrag ? MakeDrag(Drag) : MakeFileDrag(Drag);
  };
  FView->OnDragFinished = [this](Qt::DropAction Action)
  {
    if (OnDragFinished)
    {
      OnDragFinished(Action);
    }
  };
  // After the editor has closed: renaming lists the directory again.
  connect(FModel, &FileListModel::RenameRequested, this, [this](const QString & Name, const QString & NewName)
  {
    QTimer::singleShot(0, this, [this, Name, NewName]() { Rename(Name, NewName); });
  });
  connect(FView, &QTreeView::activated, this, &RemotePanel::Activated);
  connect(FView, &QWidget::customContextMenuRequested, this, &RemotePanel::ContextMenu);
  connect(FView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RemotePanel::UpdateActions);
  // As the Commander's DriveView: the tree above the files (Options >
  // Panel > Tree), hidden by default.
  FSplitter = new QSplitter(Qt::Vertical, this);
  FSplitter->setChildrenCollapsible(false);
  FTree = new RemoteDirectoryTree(FSplitter);
  FTree->hide();
  FSplitter->addWidget(FTree);
  FSplitter->addWidget(FView);
  FThumbs = new ThumbnailView(FView, FSplitter);
  FThumbs->hide();
  FSplitter->addWidget(FThumbs);
  FSplitter->setStretchFactor(2, 1);
  connect(FThumbs, &QListView::activated, this, &RemotePanel::Activated);
  FThumbs->Local = false;
  // As GetThumbnail's candidates (InitializeRemoteThumbnailMask and
  // RemoteThumbnailSizeLimit): WinSCP's image masks, up to 50 MB; asked
  // for as shown, downloaded by the session in the background.
  FThumbs->ImagePath = [this](const CoreBridge::TRemoteEntry & Entry)
  {
    static const QStringList Masks = { QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("gif"),
      QStringLiteral("png"), QStringLiteral("svg"), QStringLiteral("bmp"), QStringLiteral("raw"), QStringLiteral("ico"),
      QStringLiteral("heic"), QStringLiteral("webp") };
    const QString Name = QString::fromStdString(Entry.Name);
    if ((FSession == nullptr) || !FSession->Active() || (Entry.Size > 50LL * 1024 * 1024) ||
        !Masks.contains(QFileInfo(Name).suffix().toLower()))
    {
      return QString();
    }
    const std::string Local = FSession->ThumbnailFile(Entry.Name);
    if (Local.empty())
    {
      FSession->RequestThumbnail(Entry.Name);
    }
    return QString::fromStdString(Local);
  };
  connect(FThumbs, &QWidget::customContextMenuRequested, this, &RemotePanel::ContextMenu);
  // The other view styles (TUnixDirView's vsIcon, vsSmallIcon, vsList): the
  // names with their type icons, over the same entries and selection.
  FIcons = new QListView(FSplitter);
  FIcons->hide();
  FIcons->setModel(FView->model());
  FIcons->setModelColumn(FileListModel::NameColumn);
  FIcons->setSelectionModel(FView->selectionModel());
  FIcons->setSelectionMode(QAbstractItemView::ExtendedSelection);
  FIcons->setResizeMode(QListView::Adjust);
  FIcons->setMovement(QListView::Static);
  FIcons->setUniformItemSizes(true);
  FIcons->setContextMenuPolicy(Qt::CustomContextMenu);
  FIcons->setAccessibleName(FView->accessibleName());
  FSplitter->addWidget(FIcons);
  connect(FIcons, &QListView::activated, this, &RemotePanel::Activated);
  connect(FIcons, &QWidget::customContextMenuRequested, this, &RemotePanel::ContextMenu);
  FSplitter->setStretchFactor(1, 1);
  connect(FTree, &RemoteDirectoryTree::DirectoryChosen, this, [this](const QString & Path)
  {
    if (QDir::cleanPath(Path) != QDir::cleanPath(this->Path()))
    {
      ChangeDirectory(Path);
    }
  });
  Layout->addWidget(FSplitter, 1);
  FStatus = new QLabel(this);
  Layout->addWidget(FStatus);
  UpdateActions();
}

void RemotePanel::SetSession(CoreBridge::TSession * Session)
{
  FSession = Session;
  FBack.clear();
  FForward.clear();
  FTree->SetSession(Session);
  Reload();
}

void RemotePanel::ShowTreePath()
{
  if (TreeVisible() && (FSession != nullptr) && FSession->Active())
  {
    if (FTree->model()->rowCount() == 0)
    {
      FTree->SetSession(FSession);
    }
    FTree->ShowPath(Path());
  }
}

void RemotePanel::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    Frontend.ShowError(E.Error);
  }
}

// One operation on the session: refused while another runs, its error
// shown, the directory read back after; with Record, the directory left is
// kept for Back.
bool RemotePanel::Run(const std::function<void()> & Operation, bool Record)
{
  if (FBusy || (FSession == nullptr) || !FSession->Active())
  {
    return false;
  }
  const QString Before = FCurrent;
  FBusy = true;
  bool Result = true;
  try
  {
    Operation();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    Result = false;
  }
  FBusy = false;
  Reload();
  if (Result && Record && (FCurrent != Before))
  {
    FBack.append(Before);
    FForward.clear();
    UpdateActions();
  }
  return Result;
}

void RemotePanel::Reload()
{
  const QStringList Keep = Selected();
  const bool Active = (FSession != nullptr) && FSession->Active();
  const QString Current = Active ? QString::fromStdString(FSession->CurrentDirectory()) : QString();
  FModel->SetEntries(Active ? FSession->List() : std::vector<CoreBridge::TRemoteEntry>(),
    Active && (Current != QStringLiteral("/")));
  if (FCurrent != Current)
  {
    FView->ForgetSelectedNames();
    FThumbs->Forget();
    if (FSession != nullptr)
    {
      FSession->ForgetThumbnails();
    }
  }
  FCurrent = Current;
  FPath->setText(Current);
  FPath->setEnabled(Active);
  Select(Keep);
  UpdateActions();
  ShowTreePath();
  emit Changed();
}

// Core reports a directory it cannot enter itself, and stays: so it is the
// directory shown afterwards that says whether it changed.
QMenu * RemotePanel::PathMenu()
{
  QMenu * Menu = new QMenu(this);
  if ((FSession == nullptr) || !FSession->Active() || FCurrent.isEmpty())
  {
    return Menu;
  }
  QStringList Paths = { QStringLiteral("/") };
  QString Path;
  for (const QString & Part : FCurrent.split(QLatin1Char('/'), Qt::SkipEmptyParts))
  {
    Path += QLatin1Char('/') + Part;
    Paths.append(Path);
  }
  for (int Index = 0; Index < Paths.size(); Index++)
  {
    const QString Name = (Index == 0) ? QStringLiteral("/") : Paths[Index].section(QLatin1Char('/'), -1);
    // Indented by depth, as the drop-down's tree.
    QAction * Action = Menu->addAction(style()->standardIcon((Index == Paths.size() - 1) ? QStyle::SP_DirOpenIcon : QStyle::SP_DirIcon),
      QString(Index * 2, QLatin1Char(' ')) + Name);
    Action->setCheckable(true);
    Action->setChecked(Index == Paths.size() - 1);
    const QString Target = Paths[Index];
    connect(Action, &QAction::triggered, this, [this, Target]() { ChangeDirectory(Target); });
  }
  return Menu;
}

void RemotePanel::ShowPathMenu()
{
  std::unique_ptr<QMenu> Menu(PathMenu());
  Menu->exec(FPath->mapToGlobal(QPoint(0, FPath->height())));
}

bool RemotePanel::ChangeDirectory(const QString & Directory)
{
  const QString Before = FCurrent;
  return Run([this, &Directory]() { FSession->ChangeDirectory(Directory.toStdString()); }, true) &&
    (FCurrent != Before);
}

bool RemotePanel::Back()
{
  if (FBack.isEmpty())
  {
    return false;
  }
  const QString Target = FBack.last();
  const QString Here = FCurrent;
  if (!Run([this, &Target]() { FSession->ChangeDirectory(Target.toStdString()); }, false))
  {
    return false;
  }
  FBack.removeLast();
  FForward.append(Here);
  UpdateActions();
  return true;
}

bool RemotePanel::Forward()
{
  if (FForward.isEmpty())
  {
    return false;
  }
  const QString Target = FForward.last();
  const QString Here = FCurrent;
  if (!Run([this, &Target]() { FSession->ChangeDirectory(Target.toStdString()); }, false))
  {
    return false;
  }
  FForward.removeLast();
  FBack.append(Here);
  UpdateActions();
  return true;
}

bool RemotePanel::Parent()
{
  return (Path() != QStringLiteral("/")) && ChangeDirectory(QString::fromLatin1(ParentEntryName));
}

bool RemotePanel::Root()
{
  return ChangeDirectory(QStringLiteral("/"));
}

bool RemotePanel::Home()
{
  return Run([this]() { FSession->HomeDirectory(FDefaultDirIsHome); }, true);
}

bool RemotePanel::Refresh()
{
  const bool Result = Run([this]() { FSession->Refresh(); }, false);
  // Its folders read again too.
  if (TreeVisible())
  {
    FTree->Reload();
  }
  return Result;
}

bool RemotePanel::NewFolderDialog()
{
  if (FSession == nullptr)
  {
    return false;
  }
  CreateDirectoryDialog Dialog(CoreText(CoreBridge::TText::NewFolderName), true, FSession->CanChangeRights(),
    CoreBridge::NewDirectorySettings(), this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  const CoreBridge::TNewDirectory Settings = Dialog.Settings();
  if (Dialog.SaveSettings())
  {
    try
    {
      CoreBridge::SetNewDirectorySettings(Settings);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  return NewFolder(Dialog.Name().trimmed(), &Settings);
}

bool RemotePanel::NewFolder(const QString & Name, const CoreBridge::TNewDirectory * Settings)
{
  const bool Result = Run([this, &Name, Settings]() { FSession->CreateDirectory(Name.toStdString(), Settings); }, false);
  if (Result)
  {
    Select({ Name });
  }
  return Result;
}

bool RemotePanel::Rename(const QString & Name, const QString & NewName)
{
  const bool Result = Run([this, &Name, &NewName]() { FSession->Rename(Name.toStdString(), NewName.toStdString()); }, false);
  if (Result)
  {
    Select({ NewName });
  }
  return Result;
}

// As WinSCP's remote panel moves what is dropped on one of its directories.
bool RemotePanel::MoveInto(const QStringList & Names, const QString & Directory)
{
  if (Names.isEmpty() || Directory.isEmpty() || Names.contains(Directory) || (FSession == nullptr))
  {
    return false;
  }
  QString Target;
  if (Directory == QLatin1String(ParentEntryName))
  {
    const int Slash = FCurrent.lastIndexOf(QLatin1Char('/'));
    Target = (Slash <= 0) ? QStringLiteral("/") : FCurrent.left(Slash);
  }
  else
  {
    Target = FCurrent.endsWith(QLatin1Char('/')) ? (FCurrent + Directory) : (FCurrent + QLatin1Char('/') + Directory);
  }
  std::vector<std::string> List;
  for (const QString & Name : Names)
  {
    List.push_back(Name.toStdString());
  }
  bool Moved = false;
  const bool Result = Run([this, &List, &Target, &Moved]() { Moved = FSession->Move(List, Target.toStdString()); }, false);
  return Result && Moved;
}

// As the explorer deletes: core's confirmation naming the file, or how many.
bool RemotePanel::DeleteSelected(bool Confirm)
{
  const QStringList Names = Selected();
  if (Names.isEmpty() || FBusy)
  {
    return false;
  }
  // As DeleteFiles: Environment's confirmation, with "Never ask me again"
  // turning it off.
  if (Confirm && CoreBridge::Preferences().ConfirmDeleting)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = ((Names.size() == 1) ?
      CoreText(CoreBridge::TText::ConfirmDeleteFile).replace(QStringLiteral("%s"), Names.first()) :
      CoreText(CoreBridge::TText::ConfirmDeleteFiles).replace(QStringLiteral("%d"), QString::number(Names.size()))).toStdString();
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
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed) { Changed.ConfirmDeleting = false; });
    }
  }
  std::vector<std::string> List;
  for (const QString & Name : Names)
  {
    List.push_back(Name.toStdString());
  }
  bool Complete = true;
  // As ExecuteFileOperation's TAutoBatch: "Continue on error" collecting.
  FSession->BeginErrorList();
  const bool Result = Run([this, &List, &Complete]() { Complete = FSession->Delete(List); }, false);
  if (FSession != nullptr)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    Frontend.ShowErrorList(FSession->EndErrorList());
  }
  return Result && Complete;
}

void RemotePanel::Open(const QString & Name)
{
  if (Name == QString::fromLatin1(ParentEntryName))
  {
    Parent();
    return;
  }
  for (int Row = 0; Row < FModel->rowCount(); Row++)
  {
    const CoreBridge::TRemoteEntry * Entry = FModel->Entry(Row);
    if ((Entry != nullptr) && (QString::fromStdString(Entry->Name) == Name) && Entry->Directory)
    {
      ChangeDirectory(Name);
      return;
    }
  }
  // A file: the window's (its double-click action).
  if (OpenFile)
  {
    OpenFile(Name);
  }
}

void RemotePanel::Activated(const QModelIndex & Index)
{
  const QModelIndex Source = FSort->mapToSource(Index);
  Open(FModel->IsParent(Source.row()) ? QString::fromLatin1(ParentEntryName) :
    QString::fromStdString(FModel->Entry(Source.row())->Name));
}

void RemotePanel::ContextMenu(const QPoint & Pos)
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

void RemotePanel::SetShowHidden(bool Show)
{
  FSort->SetShowHidden(Show);
  if (TreeVisible())
  {
    FTree->Reload();
  }
  UpdateActions();
}

QStringList RemotePanel::Selected() const
{
  QStringList Result;
  if (FView->selectionModel() == nullptr)
  {
    return Result;
  }
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

void RemotePanel::Select(const QStringList & Names)
{
  FView->selectionModel()->clearSelection();
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    const QModelIndex Index = FSort->index(Row, FileListModel::NameColumn);
    if (Names.contains(Index.data(Qt::DisplayRole).toString()) && !Index.data(FileListModel::ParentRole).toBool())
    {
      FView->selectionModel()->select(Index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      // The view's own setCurrentIndex would clear the selection made so far.
      FView->selectionModel()->setCurrentIndex(Index, QItemSelectionModel::NoUpdate);
    }
  }
}

// In the panel's order: directories first, then as sorted.
QStringList RemotePanel::Names() const
{
  QStringList Result;
  for (int Row = 0; Row < FSort->rowCount(); Row++)
  {
    Result.append(FSort->index(Row, FileListModel::NameColumn).data(Qt::DisplayRole).toString());
  }
  return Result;
}

QString RemotePanel::Path() const
{
  return FCurrent;
}

void RemotePanel::UpdateActions()
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
    FStatus->setText(((FSession != nullptr) && FSession->Active()) ? FView->StatusText() : QString());
  }
  const bool Active = (FSession != nullptr) && FSession->Active();
  const int Count = Selected().size();
  FBackAction->setEnabled(Active && !FBack.isEmpty());
  FForwardAction->setEnabled(Active && !FForward.isEmpty());
  FParentAction->setEnabled(Active && (Path() != QStringLiteral("/")));
  for (QAction * Action : { FRootAction, FHomeAction, FRefreshAction, FNewFolderAction })
  {
    Action->setEnabled(Active);
  }
  FRenameAction->setEnabled(Active && (Count == 1));
  FDeleteAction->setEnabled(Active && (Count > 0));
}

void RemotePanel::ApplyPreferences(const CoreBridge::TPreferences & Preferences)
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
  FDefaultDirIsHome = Preferences.DefaultDirIsHome;
  FModel->SetFileColors(Preferences.FileColors, false,
    [this](const CoreBridge::TRemoteEntry & Entry)
    {
      return (FCurrent.endsWith(QLatin1Char('/')) ? FCurrent : (FCurrent + QLatin1Char('/'))).toStdString() + Entry.Name;
    });
  SetShowHidden(Preferences.ShowHiddenFiles);
  FSort->SetShowInaccessible(Preferences.ShowInaccesibleDirectories);
}

// WinSCP's columns: name, size, changed, rights, owner, group, extension, link target, type (TUnixDirView).
static const std::vector<int> WinScpColumns = { 0, 1, 2, 3, 4, 5, 7, 6, 8 };

void RemotePanel::SetColumnsParams(const QString & Params)
{
  FView->SetColumnsParams(Params, WinScpColumns, 9);
}

QString RemotePanel::ColumnsParams() const
{
  return FView->ColumnsParams(WinScpColumns, 9);
}

QStringList RemotePanel::History() const
{
  QStringList Result = FBack;
  Result.append(FCurrent);
  for (auto It = FForward.rbegin(); It != FForward.rend(); ++It)
  {
    Result.append(*It);
  }
  Result.removeDuplicates();
  Result.removeAll(QString());
  return Result;
}

void RemotePanel::SetFilter(const QString & Mask)
{
  FSort->SetMask(Mask);
  UpdateActions();
}

QString RemotePanel::Filter() const
{
  return FSort->Mask();
}

std::vector<CoreBridge::TRemoteEntry> RemotePanel::Entries() const
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

void RemotePanel::SetDirectorySize(const QString & Name, std::int64_t Size)
{
  FModel->SetDirectorySize(Name, Size);
}

// The set's icons (IconSet), else -- with none -- Qt's own, the panel's
// toolbar being of icons.
void RemotePanel::ApplyIcons()
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

TPanelState RemotePanel::State() const
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

void RemotePanel::RestoreState(const TPanelState & State)
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

QStringList RemotePanel::Selection() const
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

void RemotePanel::SetTreeVisible(bool Visible)
{
  FTree->setVisible(Visible);
  if (Visible)
  {
    ShowTreePath();
  }
}

bool RemotePanel::TreeVisible() const
{
  return !FTree->isHidden();
}

int RemotePanel::TreeHeight() const
{
  return FSplitter->sizes().value(0);
}

void RemotePanel::SetTreeHeight(int Height)
{
  const int Total = std::max(FSplitter->height(), Height + 100);
  FSplitter->setSizes({ Height, Total - Height });
}

void RemotePanel::SetThumbnails(bool Thumbnails)
{
  SetViewStyle(Thumbnails ? TViewStyle::Thumbnail : TViewStyle::Report);
}

bool RemotePanel::Thumbnails() const
{
  return (FViewStyle == TViewStyle::Thumbnail);
}

// As TCustomDirView's ViewStyle: details, thumbnails, or the names with
// large icons (wrapping across), small ones (across) or a list (down).
void RemotePanel::SetViewStyle(TViewStyle Style)
{
  const bool Focused = FocusView()->hasFocus();
  FViewStyle = Style;
  const bool Icons = (Style == TViewStyle::Icon) || (Style == TViewStyle::SmallIcon) || (Style == TViewStyle::List);
  if (Icons)
  {
    const int Size = ((Style == TViewStyle::Icon) ? 32 : 16) * logicalDpiX() / 96;
    FIcons->setIconSize(QSize(Size, Size));
    FIcons->setWrapping(true);
    if (Style == TViewStyle::Icon)
    {
      FIcons->setViewMode(QListView::IconMode);
      FIcons->setFlow(QListView::LeftToRight);
      FIcons->setWordWrap(true);
      FIcons->setGridSize(QSize(Size * 3, Size + FIcons->fontMetrics().height() * 2 + 8));
    }
    else
    {
      FIcons->setViewMode(QListView::ListMode);
      FIcons->setFlow((Style == TViewStyle::List) ? QListView::TopToBottom : QListView::LeftToRight);
      FIcons->setWordWrap(false);
      FIcons->setGridSize(QSize());
    }
  }
  FView->setVisible(Style == TViewStyle::Report);
  FThumbs->setVisible(Style == TViewStyle::Thumbnail);
  FIcons->setVisible(Icons);
  if (Focused)
  {
    FocusView()->setFocus();
  }
}

QAbstractItemView * RemotePanel::FocusView() const
{
  switch (FViewStyle)
  {
    case TViewStyle::Thumbnail: return FThumbs;
    case TViewStyle::Report: return FView;
    default: return FIcons;
  }
}

void RemotePanel::SetPathBarVisible(bool Visible)
{
  FPath->setVisible(Visible);
}

// As the Explorer has its DriveView: beside the files, not above them.
void RemotePanel::SetTreeBeside(bool Beside)
{
  FSplitter->setOrientation(Beside ? Qt::Horizontal : Qt::Vertical);
  // Its width kept as the window's changes (the files take the rest).
  FSplitter->setStretchFactor(0, Beside ? 0 : 1);
}
