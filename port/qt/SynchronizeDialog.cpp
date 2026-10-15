#include "SynchronizeDialog.h"
#include <algorithm>
#include <QMenu>
#include "QtFrontend.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>

#include "TransferSettingsDialog.h"

using TAction = CoreBridge::TSyncItem::TAction;
using TSyncParams = CoreBridge::TSyncParams;

SynchronizeDialog::SynchronizeDialog(const QString & LocalDirectory, const QString & RemoteDirectory,
  const TSyncParams & Params, bool CanTimestamps, QWidget * Parent, bool CanChecksum, bool AllowSelectedOnly, bool SaveMode) :
  QDialog(Parent),
  FCanTimestamps(CanTimestamps),
  FCanChecksum(CanChecksum),
  FSaveMode(SaveMode),
  FOrigDirection(Params.Direction)
{
  setWindowTitle(tr("Synchronize"));
  QVBoxLayout * Layout = new QVBoxLayout(this);

  QGroupBox * Directories = new QGroupBox(tr("Directories"), this);
  QGridLayout * DirectoriesLayout = new QGridLayout(Directories);
  FLocalEdit = new QLineEdit(LocalDirectory, Directories);
  FLocalEdit->setMinimumWidth(380);
  QPushButton * Browse = new QPushButton(tr("Bro&wse..."), Directories);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    const QString Directory = QFileDialog::getExistingDirectory(this, tr("Select local directory"), FLocalEdit->text());
    if (!Directory.isEmpty())
    {
      FLocalEdit->setText(QDir::toNativeSeparators(Directory));
    }
  });
  FRemoteEdit = new QLineEdit(RemoteDirectory, Directories);
  QLabel * LocalLabel = new QLabel(tr("Lo&cal directory:"), Directories);
  LocalLabel->setBuddy(FLocalEdit);
  QLabel * RemoteLabel = new QLabel(tr("R&emote directory:"), Directories);
  RemoteLabel->setBuddy(FRemoteEdit);
  DirectoriesLayout->addWidget(LocalLabel, 0, 0, 1, 2);
  DirectoriesLayout->addWidget(FLocalEdit, 1, 0);
  DirectoriesLayout->addWidget(Browse, 1, 1);
  DirectoriesLayout->addWidget(RemoteLabel, 2, 0, 1, 2);
  DirectoriesLayout->addWidget(FRemoteEdit, 3, 0, 1, 2);
  Layout->addWidget(Directories);

  QHBoxLayout * Groups = new QHBoxLayout();
  QGroupBox * Direction = new QGroupBox(tr("Direction/Target directory"), this);
  QVBoxLayout * DirectionLayout = new QVBoxLayout(Direction);
  FBoth = new QRadioButton(tr("&Both"), Direction);
  FRemote = new QRadioButton(tr("&Remote"), Direction);
  FLocal = new QRadioButton(tr("&Local"), Direction);
  DirectionLayout->addWidget(FBoth);
  DirectionLayout->addWidget(FRemote);
  DirectionLayout->addWidget(FLocal);
  Groups->addWidget(Direction);
  QGroupBox * Mode = new QGroupBox(tr("Mode"), this);
  QVBoxLayout * ModeLayout = new QVBoxLayout(Mode);
  FFiles = new QRadioButton(tr("Synchronize &files"), Mode);
  FMirror = new QRadioButton(tr("&Mirror files"), Mode);
  FTimestamps = new QRadioButton(tr("Synchronize &timestamps"), Mode);
  ModeLayout->addWidget(FFiles);
  ModeLayout->addWidget(FMirror);
  ModeLayout->addWidget(FTimestamps);
  Groups->addWidget(Mode);
  Layout->addLayout(Groups);

  QHBoxLayout * Groups2 = new QHBoxLayout();
  QGroupBox * Options = new QGroupBox(tr("Synchronize options"), this);
  QVBoxLayout * OptionsLayout = new QVBoxLayout(Options);
  FDelete = new QCheckBox(tr("&Delete files"), Options);
  // As FullSynchronize.dfm's: selected files only, after delete.
  FSelectedOnly = new QCheckBox(tr("Selected files o&nly"), Options);
  FSelectedOnly->setEnabled(AllowSelectedOnly);
  FExistingOnly = new QCheckBox(tr("E&xisting files only"), Options);
  FPreview = new QCheckBox(tr("Pre&view changes"), Options);
  OptionsLayout->addWidget(FDelete);
  OptionsLayout->addWidget(FSelectedOnly);
  OptionsLayout->addWidget(FExistingOnly);
  OptionsLayout->addWidget(FPreview);
  Groups2->addWidget(Options);
  QGroupBox * Criteria = new QGroupBox(tr("Comparison criteria"), this);
  QVBoxLayout * CriteriaLayout = new QVBoxLayout(Criteria);
  FByTime = new QCheckBox(tr("M&odification time"), Criteria);
  FBySizeCaption = tr("File si&ze");
  FBySize = new QCheckBox(FBySizeCaption, Criteria);
  FCaseSensitive = new QCheckBox(tr("C&ase sensitive"), Criteria);
  CriteriaLayout->addWidget(FByTime);
  CriteriaLayout->addWidget(FBySize);
  CriteriaLayout->addWidget(FCaseSensitive);
  FByChecksum = new QCheckBox(tr("Checks&um"), Criteria);
  CriteriaLayout->addWidget(FByChecksum);
  Groups2->addWidget(Criteria);
  Layout->addLayout(Groups2);
  FSaveSettings = new QCheckBox(tr("Use &same options next time"), this);
  Layout->addWidget(FSaveSettings);
  FTransferSettings = new TransferSettingsBox(this);
  Layout->addWidget(FTransferSettings);

  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Box->button(QDialogButtonBox::Ok);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);

  // As TFullSynchronizeDialog::SetParams.
  switch (Params.Direction)
  {
    case TSyncParams::TDirection::Both: FBoth->setChecked(true); break;
    case TSyncParams::TDirection::Local: FLocal->setChecked(true); break;
    default: FRemote->setChecked(true); break;
  }
  if ((Params.Mode == TSyncParams::TMode::Timestamps) && FCanTimestamps)
  {
    FTimestamps->setChecked(true);
  }
  else if (Params.Mode == TSyncParams::TMode::Mirror)
  {
    FMirror->setChecked(true);
  }
  else
  {
    FFiles->setChecked(true);
  }
  FDelete->setChecked(Params.Delete);
  FExistingOnly->setChecked(Params.ExistingOnly);
  FPreview->setChecked(Params.PreviewChanges);
  FByTime->setChecked(Params.ByTime);
  FBySize->setChecked(Params.BySize);
  FCaseSensitive->setChecked(Params.CaseSensitive);
  FSelectedOnly->setChecked(Params.SelectedOnly && AllowSelectedOnly);
  FByChecksum->setChecked(Params.ByChecksum);

  for (QRadioButton * Button : { FBoth, FRemote, FLocal, FFiles, FMirror, FTimestamps })
  {
    connect(Button, &QRadioButton::toggled, this, &SynchronizeDialog::UpdateControls);
  }
  for (QCheckBox * Check : { FDelete, FExistingOnly, FPreview, FByTime, FBySize, FCaseSensitive })
  {
    connect(Check, &QCheckBox::toggled, this, &SynchronizeDialog::UpdateControls);
  }
  connect(FLocalEdit, &QLineEdit::textChanged, this, &SynchronizeDialog::UpdateControls);
  connect(FRemoteEdit, &QLineEdit::textChanged, this, &SynchronizeDialog::UpdateControls);
  UpdateControls();
}

// As TFullSynchronizeDialog::UpdateControls.
void SynchronizeDialog::UpdateControls()
{
  FTimestamps->setEnabled(FCanTimestamps);
  if (FTimestamps->isChecked())
  {
    FExistingOnly->setChecked(true);
    FDelete->setChecked(false);
    FByTime->setChecked(true);
  }
  if (FBoth->isChecked())
  {
    FByTime->setChecked(true);
    FBySize->setChecked(false);
    if (FMirror->isChecked())
    {
      FFiles->setChecked(true);
    }
  }
  FMirror->setEnabled(!FBoth->isChecked());
  FDelete->setEnabled(!FBoth->isChecked() && !FTimestamps->isChecked());
  FExistingOnly->setEnabled(!FTimestamps->isChecked());
  FByTime->setEnabled(!FBoth->isChecked() && !FTimestamps->isChecked());
  FBySize->setEnabled(!FBoth->isChecked());
  // As UpdateControls: by checksum not with both directions nor timestamps.
  if (FBoth->isChecked() || FTimestamps->isChecked())
  {
    FByChecksum->setChecked(false);
  }
  FByChecksum->setEnabled(FCanChecksum && !FBoth->isChecked() && !FTimestamps->isChecked());
  FOk->setEnabled(!FLocalEdit->text().trimmed().isEmpty() && !FRemoteEdit->text().trimmed().isEmpty());
  FBySize->setText(FTimestamps->isChecked() ? tr("Same si&ze only") : FBySizeCaption);
}

QString SynchronizeDialog::LocalDirectory() const
{
  return FLocalEdit->text().trimmed();
}

QString SynchronizeDialog::RemoteDirectory() const
{
  return FRemoteEdit->text().trimmed();
}

TSyncParams SynchronizeDialog::Params() const
{
  TSyncParams Result;
  Result.Direction = FBoth->isChecked() ? TSyncParams::TDirection::Both :
    (FLocal->isChecked() ? TSyncParams::TDirection::Local : TSyncParams::TDirection::Remote);
  Result.Mode = (FTimestamps->isChecked() && FCanTimestamps) ? TSyncParams::TMode::Timestamps :
    (FMirror->isChecked() ? TSyncParams::TMode::Mirror : TSyncParams::TMode::Files);
  Result.Delete = FDelete->isChecked();
  Result.ExistingOnly = FExistingOnly->isChecked();
  Result.PreviewChanges = FPreview->isChecked();
  Result.ByTime = FByTime->isChecked();
  Result.BySize = FBySize->isChecked();
  Result.CaseSensitive = FCaseSensitive->isChecked();
  Result.SelectedOnly = FSelectedOnly->isEnabled() && FSelectedOnly->isChecked();
  Result.ByChecksum = FByChecksum->isEnabled() && FByChecksum->isChecked();
  return Result;
}

bool SynchronizeDialog::SaveSettings() const
{
  return FSaveSettings->isChecked();
}

// As FormCloseQuery: a direction other than the active panel's, with the
// options kept, offered as the default too (SAVE_SYNCHRONIZE_MODE2).
void SynchronizeDialog::accept()
{
  if (SaveSettings() && !FSaveMode && (Params().Direction != FOrigDirection))
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreBridge::Text(CoreBridge::TText::SaveSyncMode);
    for (unsigned Answer : { CoreBridge::AnswerYes, CoreBridge::AnswerNo, CoreBridge::AnswerCancel })
    {
      CoreBridge::TQueryButton Button;
      Button.Answer = Answer;
      Button.Caption = (Answer == CoreBridge::AnswerYes) ? "&Yes" : ((Answer == CoreBridge::AnswerNo) ? "&No" : "Cancel");
      Query.Buttons.push_back(Button);
    }
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const unsigned Answer = Frontend.Query(Query).Answer;
    if (Answer == CoreBridge::AnswerCancel)
    {
      return;
    }
    FSaveMode = (Answer == CoreBridge::AnswerYes);
  }
  QDialog::accept();
}

SynchronizeProgress::SynchronizeProgress(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Comparing"));
  QGridLayout * Layout = new QGridLayout(this);
  Layout->addWidget(new QLabel(tr("Local:"), this), 0, 0);
  FLocal = new QLabel(this);
  FLocal->setMinimumWidth(380);
  Layout->addWidget(FLocal, 0, 1);
  Layout->addWidget(new QLabel(tr("Remote:"), this), 1, 0);
  FRemote = new QLabel(this);
  Layout->addWidget(FRemote, 1, 1);
  FCancel = new QPushButton(tr("Cancel"), this);
  connect(FCancel, &QPushButton::clicked, this, [this]() { FCancelled = true; });
  Layout->addWidget(FCancel, 2, 1, Qt::AlignRight);
}

bool SynchronizeProgress::Directory(const QString & Local, const QString & Remote)
{
  FLocal->setText(Local);
  FRemote->setText(Remote);
  QApplication::processEvents();
  return !FCancelled;
}

namespace
{

enum { NameColumn, LocalDirectoryColumn, LocalSizeColumn, LocalModifiedColumn, ActionColumn,
  RemoteDirectoryColumn, RemoteSizeColumn, RemoteModifiedColumn };
enum { IndexRole = Qt::UserRole };

QString ActionText(TAction Action)
{
  switch (Action)
  {
    case TAction::UploadNew: return SyncChecklistDialog::tr("Upload new");
    case TAction::DownloadNew: return SyncChecklistDialog::tr("Download new");
    case TAction::UploadUpdate: return SyncChecklistDialog::tr("Upload update");
    case TAction::DownloadUpdate: return SyncChecklistDialog::tr("Download update");
    case TAction::DeleteRemote: return SyncChecklistDialog::tr("Delete remote");
    case TAction::DeleteLocal: return SyncChecklistDialog::tr("Delete local");
    default: return QString();
  }
}

QIcon ActionIcon(TAction Action)
{
  QStyle * Style = QApplication::style();
  switch (Action)
  {
    case TAction::UploadNew:
    case TAction::UploadUpdate: return Style->standardIcon(QStyle::SP_ArrowRight);
    case TAction::DownloadNew:
    case TAction::DownloadUpdate: return Style->standardIcon(QStyle::SP_ArrowLeft);
    case TAction::DeleteRemote:
    case TAction::DeleteLocal: return Style->standardIcon(QStyle::SP_TrashIcon);
    default: return QIcon();
  }
}

}  // namespace

SyncChecklistDialog::SyncChecklistDialog(CoreBridge::TSyncChecklist & Checklist, const QString & LocalDirectory,
  const QString & RemoteDirectory, QWidget * Parent, CoreBridge::TSession * Session) :
  QDialog(Parent),
  FChecklist(Checklist),
  FSession(Session)
{
  setWindowTitle(tr("Synchronization checklist - %1 <=> %2").arg(LocalDirectory, RemoteDirectory));
  resize(960, 480);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Top = new QHBoxLayout();
  FList = new QTreeWidget(this);
  FList->setAccessibleName(tr("Checklist"));
  FList->setRootIsDecorated(false);
  FList->setUniformRowHeights(true);
  FList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  FList->setHeaderLabels({ tr("Name"), tr("Local directory"), tr("Size"), tr("Date modified"), QString(),
    tr("Remote directory"), tr("Size"), tr("Date modified") });
  FList->header()->setSectionResizeMode(ActionColumn, QHeaderView::ResizeToContents);
  Top->addWidget(FList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  FStart = new QPushButton(tr("&Start"), this);
  FStart->setDefault(true);
  connect(FStart, &QPushButton::clicked, this, [this]() { FResult = Started; accept(); });
  FQueue = new QPushButton(tr("Start in &Background"), this);
  connect(FQueue, &QPushButton::clicked, this, [this]() { FResult = Queued; accept(); });
  QPushButton * Cancel = new QPushButton(tr("Cancel"), this);
  connect(Cancel, &QPushButton::clicked, this, &QDialog::reject);
  QPushButton * CheckAllButton = new QPushButton(tr("Check &All"), this);
  connect(CheckAllButton, &QPushButton::clicked, this, [this]() { CheckAll(true); });
  QPushButton * UncheckAllButton = new QPushButton(tr("Uncheck A&ll"), this);
  connect(UncheckAllButton, &QPushButton::clicked, this, [this]() { CheckAll(false); });
  FReverse = new QPushButton(tr("&Reverse"), this);
  connect(FReverse, &QPushButton::clicked, this, &SyncChecklistDialog::ReverseSelected);
  // As ToolsMenuButton's menu.
  QPushButton * Tools = new QPushButton(tr("&Tools"), this);
  QMenu * ToolsMenu = new QMenu(Tools);
  FCalculateAction = ToolsMenu->addAction(tr("C&alculate"), this, [this]() { CalculateSizes(false); });
  FCalculateAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
  FCalculateAllAction = ToolsMenu->addAction(tr("Calc&ulate All"), this, [this]() { CalculateSizes(true); });
  FCalculateAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_Return));
  FMoveAction = ToolsMenu->addAction(tr("&Move"), this, [this]() { MoveSelected(); });
  FMoveAction->setShortcut(QKeySequence(Qt::Key_F6));
  FFindMoveAction = ToolsMenu->addAction(tr("&Find Move Candidate"), this, [this]() { FindMoveCandidate(); });
  FFindMoveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F6));
  for (QAction * Action : ToolsMenu->actions())
  {
    // The keys while the dialog is up, the menu shut.
    addAction(Action);
  }
  Tools->setMenu(ToolsMenu);
  Tools->setVisible(FSession != nullptr);
  for (QPushButton * Button : { FStart, FQueue, Cancel, CheckAllButton, UncheckAllButton, FReverse, Tools })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  Top->addLayout(Buttons);
  Layout->addLayout(Top, 1);
  FStatus = new QLabel(this);
  Layout->addWidget(FStatus);

  FLoading = true;
  for (std::size_t Index = 0; Index < FChecklist.Items().size(); Index++)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FList);
    Item->setData(NameColumn, IndexRole, static_cast<int>(Index));
    Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
    Load(static_cast<int>(Index));
  }
  FLoading = false;
  for (int Column = 0; Column < FList->columnCount(); Column++)
  {
    FList->resizeColumnToContents(Column);
  }
  connect(FList, &QTreeWidget::itemChanged, this, &SyncChecklistDialog::ItemChanged);
  connect(FList, &QTreeWidget::itemSelectionChanged, this, [this]()
  {
    FReverse->setEnabled(!FList->selectedItems().isEmpty());
    UpdateTools();
  });
  FReverse->setEnabled(false);
  UpdateTools();
  UpdateStatus();
}

void SyncChecklistDialog::Reload()
{
  FLoading = true;
  FList->clear();
  for (std::size_t Index = 0; Index < FChecklist.Items().size(); Index++)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FList);
    Item->setData(NameColumn, IndexRole, static_cast<int>(Index));
    Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
    Load(static_cast<int>(Index));
  }
  FLoading = false;
  UpdateTools();
  UpdateStatus();
}

std::vector<std::size_t> SyncChecklistDialog::SelectedIndexes() const
{
  std::vector<std::size_t> Result;
  for (QTreeWidgetItem * Item : FList->selectedItems())
  {
    Result.push_back(static_cast<std::size_t>(Item->data(NameColumn, IndexRole).toInt()));
  }
  std::sort(Result.begin(), Result.end());
  return Result;
}

void SyncChecklistDialog::SelectIndexes(const std::vector<std::size_t> & Indexes)
{
  FList->clearSelection();
  for (std::size_t Index : Indexes)
  {
    if (QTreeWidgetItem * Item = FList->topLevelItem(static_cast<int>(Index)))
    {
      Item->setSelected(true);
      FList->setCurrentItem(Item, NameColumn, QItemSelectionModel::NoUpdate);
      FList->scrollToItem(Item);
    }
  }
}

// As UpdateControls: Calculate for directories, Move for a pair that can.
void SyncChecklistDialog::UpdateTools()
{
  if (FCalculateAction == nullptr)
  {
    return;
  }
  const std::vector<std::size_t> Selected = SelectedIndexes();
  bool AnyDirectory = false;
  bool AllDirectories = false;
  for (std::size_t Index : Selected)
  {
    AnyDirectory = AnyDirectory || FChecklist.Items()[Index].Directory;
  }
  for (const CoreBridge::TSyncItem & Item : FChecklist.Items())
  {
    AllDirectories = AllDirectories || Item.Directory;
  }
  const bool Can = (FSession != nullptr);
  FCalculateAction->setEnabled(Can && AnyDirectory);
  FCalculateAllAction->setEnabled(Can && AllDirectories);
  FMoveAction->setEnabled(Can && FChecklist.CanMove(Selected));
  FFindMoveAction->setEnabled(!FChecklist.Items().empty());
}

void SyncChecklistDialog::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

bool SyncChecklistDialog::CalculateSizes(bool All)
{
  if (FSession == nullptr)
  {
    return false;
  }
  std::vector<std::size_t> Indexes;
  if (All)
  {
    for (std::size_t Index = 0; Index < FChecklist.Items().size(); Index++)
    {
      Indexes.push_back(Index);
    }
  }
  else
  {
    Indexes = SelectedIndexes();
  }
  try
  {
    FSession->SyncCalculateSizes(FChecklist, Indexes);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  for (std::size_t Index : Indexes)
  {
    Load(static_cast<int>(Index));
  }
  UpdateStatus();
  return true;
}

bool SyncChecklistDialog::MoveSelected()
{
  const std::vector<std::size_t> Selected = SelectedIndexes();
  if ((FSession == nullptr) || !FChecklist.CanMove(Selected))
  {
    return false;
  }
  try
  {
    FSession->SyncMove(FChecklist, Selected);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  Reload();
  return true;
}

bool SyncChecklistDialog::FindMoveCandidate()
{
  const QTreeWidgetItem * Current = FList->currentItem();
  const std::size_t From = (Current != nullptr) ? static_cast<std::size_t>(Current->data(NameColumn, IndexRole).toInt()) : 0;
  std::size_t First = 0;
  std::size_t Second = 0;
  if (!FChecklist.FindMoveCandidate(From, First, Second))
  {
    return false;
  }
  SelectIndexes({ Second, First });
  return true;
}

// As TSynchronizeChecklistDialog::LoadItem: each side's directory, and its
// size and time where the file is.
void SyncChecklistDialog::Load(int Index)
{
  const CoreBridge::TSyncItem & Entry = FChecklist.Items()[Index];
  QTreeWidgetItem * Item = FList->topLevelItem(Index);
  const QLocale Locale;
  const bool WasLoading = FLoading;
  FLoading = true;
  Item->setText(NameColumn, QString::fromStdString(Entry.Name));
  Item->setIcon(NameColumn, QApplication::style()->standardIcon(Entry.Directory ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
  Item->setCheckState(NameColumn, Entry.Checked ? Qt::Checked : Qt::Unchecked);
  Item->setText(LocalDirectoryColumn, QString::fromStdString(Entry.LocalDirectory));
  Item->setText(RemoteDirectoryColumn, QString::fromStdString(Entry.RemoteDirectory));
  Item->setText(LocalSizeColumn, (Entry.LocalExists && Entry.HasSize) ? Locale.toString(static_cast<qlonglong>(Entry.LocalSize)) : QString());
  Item->setText(LocalModifiedColumn, Entry.LocalExists ? QString::fromStdString(Entry.LocalModified) : QString());
  Item->setText(RemoteSizeColumn, (Entry.RemoteExists && Entry.HasSize) ? Locale.toString(static_cast<qlonglong>(Entry.RemoteSize)) : QString());
  Item->setText(RemoteModifiedColumn, Entry.RemoteExists ? QString::fromStdString(Entry.RemoteModified) : QString());
  Item->setIcon(ActionColumn, ActionIcon(Entry.Action));
  Item->setToolTip(ActionColumn, ActionText(Entry.Action));
  Item->setData(ActionColumn, Qt::AccessibleTextRole, ActionText(Entry.Action));
  FLoading = WasLoading;
}

void SyncChecklistDialog::ItemChanged(QTreeWidgetItem * Item, int Column)
{
  if (FLoading || (Column != NameColumn))
  {
    return;
  }
  FChecklist.Check(Item->data(NameColumn, IndexRole).toInt(), Item->checkState(NameColumn) == Qt::Checked);
  UpdateStatus();
}

void SyncChecklistDialog::CheckAll(bool Check)
{
  for (std::size_t Index = 0; Index < FChecklist.Items().size(); Index++)
  {
    FChecklist.Check(Index, Check);
    Load(static_cast<int>(Index));
  }
  UpdateStatus();
}

void SyncChecklistDialog::ReverseSelected()
{
  for (QTreeWidgetItem * Item : FList->selectedItems())
  {
    const int Index = Item->data(NameColumn, IndexRole).toInt();
    FChecklist.Reverse(Index);
    Load(Index);
  }
  UpdateStatus();
}

// As the checklist's status bar: each action, checked of all.
void SyncChecklistDialog::UpdateStatus()
{
  std::map<TAction, std::pair<int, int>> Counts;
  for (const CoreBridge::TSyncItem & Entry : FChecklist.Items())
  {
    std::pair<int, int> & Count = Counts[Entry.Action];
    Count.second++;
    Count.first += Entry.Checked ? 1 : 0;
  }
  QStringList Parts;
  for (const auto & Count : Counts)
  {
    Parts.append(tr("%1: %2 of %3").arg(ActionText(Count.first)).arg(Count.second.first).arg(Count.second.second));
  }
  FStatus->setText(Parts.join(QStringLiteral("   ")));
  const bool Any = (FChecklist.CheckedCount() > 0);
  FStart->setEnabled(Any);
  FQueue->setEnabled(Any);
}
