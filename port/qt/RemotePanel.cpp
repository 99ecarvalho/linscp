#include "RemotePanel.h"
#include <QScrollBar>
#include "PanelBands.h"
#include "IconSet.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
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
  // After the editor has closed: renaming lists the directory again.
  connect(FModel, &FileListModel::RenameRequested, this, [this](const QString & Name, const QString & NewName)
  {
    QTimer::singleShot(0, this, [this, Name, NewName]() { Rename(Name, NewName); });
  });
  connect(FView, &QTreeView::activated, this, &RemotePanel::Activated);
  connect(FView, &QWidget::customContextMenuRequested, this, &RemotePanel::ContextMenu);
  connect(FView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RemotePanel::UpdateActions);
  Layout->addWidget(FView, 1);
  FStatus = new QLabel(this);
  Layout->addWidget(FStatus);
  UpdateActions();
}

void RemotePanel::SetSession(CoreBridge::TSession * Session)
{
  FSession = Session;
  FBack.clear();
  FForward.clear();
  Reload();
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
  FCurrent = Current;
  FPath->setText(Current);
  FPath->setEnabled(Active);
  Select(Keep);
  UpdateActions();
  emit Changed();
}

// Core reports a directory it cannot enter itself, and stays: so it is the
// directory shown afterwards that says whether it changed.
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
  return Run([this]() { FSession->Refresh(); }, false);
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
  if (Confirm)
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
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
  }
  std::vector<std::string> List;
  for (const QString & Name : Names)
  {
    List.push_back(Name.toStdString());
  }
  bool Complete = true;
  const bool Result = Run([this, &List, &Complete]() { Complete = FSession->Delete(List); }, false);
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

void RemotePanel::SetShowHidden(bool Show)
{
  FSort->SetShowHidden(Show);
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
  if (!FView->SearchText().isEmpty())
  {
    FStatus->setText(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::IncrementalSearch))
      .replace(QStringLiteral("%s"), FView->SearchText()));
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
}

// WinSCP's columns: name, size, changed, rights, owner, group, extension, link target, type (TUnixDirView).
static const std::vector<int> WinScpColumns = { 0, 1, 2, 3, 4, 5, 7, 6 };

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
