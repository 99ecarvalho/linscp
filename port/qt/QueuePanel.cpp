#include "QueuePanel.h"
#include "IconSet.h"
#include "QtFrontend.h"

#include <QAction>
#include <QHeaderView>
#include <QLocale>
#include <QMenu>
#include <QStyle>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{

enum { OperationColumn, SourceColumn, DestinationColumn, TransferredColumn, SpeedColumn, ProgressColumn, ColumnCount };
enum { IdRole = Qt::UserRole };

QString StateText(CoreBridge::TQueueEntry::TState State)
{
  using TState = CoreBridge::TQueueEntry::TState;
  switch (State)
  {
    case TState::Pending: return QueuePanel::tr("Waiting");
    case TState::Connecting: return QueuePanel::tr("Connecting");
    case TState::Processing: return QueuePanel::tr("In progress");
    case TState::Prompt:
    case TState::Query: return QueuePanel::tr("Waiting for an answer");
    case TState::Error: return QueuePanel::tr("Error");
    case TState::Paused: return QueuePanel::tr("Paused");
    case TState::Done: return QueuePanel::tr("Done");
  }
  return QString();
}

}  // namespace

QueuePanel::QueuePanel(QWidget * Parent) :
  QWidget(Parent)
{
  QVBoxLayout * Layout = new QVBoxLayout(this);
  Layout->setContentsMargins(0, 0, 0, 0);
  // NonVisual.dfm's Queue* actions' captions and hints (the mnemonics made
  // distinct within each menu, which WinSCP's are not).
  const struct
  {
    TOperation Operation;
    QString Text;
    QString Hint;
  } Actions[] = {
    { TOperation::UserAction, QString(), QString() },
    { TOperation::Query, tr("Show &Query"), tr("Show pending query of selected queue item") },
    { TOperation::Error, tr("Sho&w Error"), tr("Show pending error message of selected queue item") },
    { TOperation::Prompt, tr("Show Pr&ompt"), tr("Show pending prompt of selected queue item") },
    { TOperation::Delete, tr("&Cancel"), tr("Remove selected queue item") },
    { TOperation::Execute, tr("&Execute Now"), tr("Execute selected queue item now") },
    { TOperation::Up, tr("Move &Up"), tr("Move selected queue item up to be processed earlier") },
    { TOperation::Down, tr("Move &Down"), tr("Move selected queue item down to be processed later") },
    { TOperation::Pause, tr("&Suspend"), tr("Suspend selected queue item") },
    { TOperation::Resume, tr("&Resume"), tr("Resume selected suspended queue item") },
    { TOperation::PauseAll, tr("&Suspend All"), tr("Suspend all running queue items") },
    { TOperation::ResumeAll, tr("&Resume All"), tr("Resume all suspended queue items") },
    { TOperation::DeleteAllDone, tr("&Delete All Completed"), tr("Remove all completed queue items") },
    { TOperation::DeleteAll, tr("&Cancel All"), tr("Remove all queue items") },
  };
  for (const auto & Each : Actions)
  {
    QAction * Action = new QAction(Each.Text, this);
    Action->setStatusTip(Each.Hint);
    Action->setToolTip(QString(Each.Text).remove(QLatin1Char('&')));
    const TOperation Operation = Each.Operation;
    connect(Action, &QAction::triggered, this, [this, Operation]() { Execute(Operation); });
    FActions[static_cast<int>(Operation)] = Action;
  }
  FCancelAction = Action(TOperation::Delete);
  FPauseAction = Action(TOperation::Pause);
  FResumeAction = Action(TOperation::Resume);
  FCancelAction->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
  FPauseAction->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
  FResumeAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  QToolBar * Bar = new QToolBar(this);
  Bar->setIconSize(QSize(16, 16));
  Bar->addAction(FCancelAction);
  Bar->addAction(FPauseAction);
  Bar->addAction(FResumeAction);
  Layout->addWidget(Bar);
  FList = new QTreeWidget(this);
  FList->setAccessibleName(tr("Queue"));
  FList->setRootIsDecorated(false);
  FList->setUniformRowHeights(true);
  FList->setHeaderLabels({ tr("Operation"), tr("Source"), tr("Destination"), tr("Transferred"), tr("Speed"), tr("Progress") });
  FList->header()->setSectionResizeMode(SourceColumn, QHeaderView::Stretch);
  FList->header()->setSectionResizeMode(DestinationColumn, QHeaderView::Stretch);
  connect(FList, &QTreeWidget::itemSelectionChanged, this, &QueuePanel::UpdateActions);
  // As the queue list's default operation: the entry's question, if any.
  connect(FList, &QTreeWidget::itemDoubleClicked, this, [this]()
  {
    if (FSession != nullptr && FSession->QueueAllows(TOperation::UserAction, SelectedId()))
    {
      Execute(TOperation::UserAction);
    }
  });
  FList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(FList, &QWidget::customContextMenuRequested, this, [this](const QPoint & Position)
  {
    UpdateActions();
    QMenu Menu(this);
    AddCommands(&Menu);
    Menu.exec(FList->viewport()->mapToGlobal(Position));
  });
  Layout->addWidget(FList, 1);
  UpdateActions();
}

void QueuePanel::SetSession(CoreBridge::TSession * Session)
{
  FSession = Session;
  Update();
}

void QueuePanel::Update()
{
  const std::uintptr_t Selected = SelectedId();
  const std::vector<CoreBridge::TQueueEntry> Entries =
    (FSession != nullptr) ? FSession->QueueEntries() : std::vector<CoreBridge::TQueueEntry>();
  FList->clear();
  const QLocale Locale;
  for (const CoreBridge::TQueueEntry & Entry : Entries)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FList);
    Item->setData(0, IdRole, QVariant::fromValue<qulonglong>(Entry.Id));
    Item->setText(OperationColumn, Entry.Upload ? (Entry.Move ? tr("Move up") : tr("Upload")) :
      (Entry.Move ? tr("Move down") : tr("Download")));
    Item->setIcon(OperationColumn, style()->standardIcon(Entry.Upload ? QStyle::SP_ArrowUp : QStyle::SP_ArrowDown));
    Item->setText(SourceColumn, QString::fromStdString(Entry.FileName.empty() ? Entry.Source : Entry.FileName));
    Item->setText(DestinationColumn, QString::fromStdString(Entry.Destination));
    if (Entry.State == CoreBridge::TQueueEntry::TState::Processing)
    {
      Item->setText(TransferredColumn, Locale.formattedDataSize(Entry.Transferred));
      Item->setText(SpeedColumn, tr("%1/s").arg(Locale.formattedDataSize(Entry.Speed)));
      Item->setText(ProgressColumn, QStringLiteral("%1%").arg(Entry.Percent));
    }
    else
    {
      Item->setText(ProgressColumn, StateText(Entry.State));
    }
    if (Entry.Id == Selected)
    {
      Item->setSelected(true);
    }
  }
  UpdateActions();
}

std::uintptr_t QueuePanel::SelectedId() const
{
  const QList<QTreeWidgetItem *> Items = FList->selectedItems();
  return Items.isEmpty() ? 0 : static_cast<std::uintptr_t>(Items.first()->data(0, IdRole).toULongLong());
}

QAction * QueuePanel::Action(TOperation Operation) const
{
  return FActions[static_cast<int>(Operation)];
}

void QueuePanel::AddCommands(QMenu * Menu) const
{
  Menu->addAction(Action(TOperation::Query));
  Menu->addAction(Action(TOperation::Error));
  Menu->addAction(Action(TOperation::Prompt));
  Menu->addSeparator();
  Menu->addAction(Action(TOperation::Execute));
  Menu->addAction(Action(TOperation::Pause));
  Menu->addAction(Action(TOperation::Resume));
  Menu->addAction(Action(TOperation::Delete));
  Menu->addSeparator();
  Menu->addAction(Action(TOperation::Up));
  Menu->addAction(Action(TOperation::Down));
  Menu->addSeparator();
  QMenu * All = Menu->addMenu(tr("&All"));
  All->addAction(Action(TOperation::PauseAll));
  All->addAction(Action(TOperation::ResumeAll));
  All->addAction(Action(TOperation::DeleteAll));
  All->addSeparator();
  All->addAction(Action(TOperation::DeleteAllDone));
}

void QueuePanel::Execute(TOperation Operation)
{
  if (FSession == nullptr)
  {
    return;
  }
  try
  {
    FSession->QueueExecute(Operation, SelectedId());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(window());
    Frontend.ShowError(E.Error);
  }
  // The list follows on the window's next queue refresh.
  Update();
}

bool QueuePanel::CancelSelected()
{
  const bool Result = (FSession != nullptr) && FSession->QueueCancel(SelectedId());
  Update();
  return Result;
}

bool QueuePanel::PauseSelected()
{
  const bool Result = (FSession != nullptr) && FSession->QueuePause(SelectedId());
  Update();
  return Result;
}

bool QueuePanel::ResumeSelected()
{
  const bool Result = (FSession != nullptr) && FSession->QueueResume(SelectedId());
  Update();
  return Result;
}

void QueuePanel::UpdateActions()
{
  const std::uintptr_t Id = SelectedId();
  for (int Index = 0; Index <= static_cast<int>(TOperation::DeleteAll); Index++)
  {
    FActions[Index]->setEnabled((FSession != nullptr) && FSession->QueueAllows(static_cast<TOperation>(Index), Id));
  }
}

void QueuePanel::ApplyIcons()
{
  const std::pair<QAction *, std::pair<const char *, QStyle::StandardPixmap>> Icons[] = {
    { FCancelAction, { "cancel", QStyle::SP_DialogCancelButton } },
    { FPauseAction, { "pause", QStyle::SP_MediaPause } },
    { FResumeAction, { "resume", QStyle::SP_MediaPlay } },
  };
  for (const auto & Icon : Icons)
  {
    const QIcon Set = IconSet::Icon(QString::fromLatin1(Icon.second.first));
    Icon.first->setIcon(Set.isNull() ? style()->standardIcon(Icon.second.second) : Set);
  }
}
