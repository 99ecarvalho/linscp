#include "QueuePanel.h"

#include <QAction>
#include <QHeaderView>
#include <QLocale>
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
  QToolBar * Bar = new QToolBar(this);
  Bar->setIconSize(QSize(16, 16));
  FCancelAction = Bar->addAction(style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Cancel"), this,
    [this]() { CancelSelected(); });
  FPauseAction = Bar->addAction(style()->standardIcon(QStyle::SP_MediaPause), tr("Suspend"), this,
    [this]() { PauseSelected(); });
  FResumeAction = Bar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), tr("Resume"), this,
    [this]() { ResumeSelected(); });
  Layout->addWidget(Bar);
  FList = new QTreeWidget(this);
  FList->setRootIsDecorated(false);
  FList->setUniformRowHeights(true);
  FList->setHeaderLabels({ tr("Operation"), tr("Source"), tr("Destination"), tr("Transferred"), tr("Speed"), tr("Progress") });
  FList->header()->setSectionResizeMode(SourceColumn, QHeaderView::Stretch);
  FList->header()->setSectionResizeMode(DestinationColumn, QHeaderView::Stretch);
  connect(FList, &QTreeWidget::itemSelectionChanged, this, &QueuePanel::UpdateActions);
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
  const bool Any = (SelectedId() != 0);
  FCancelAction->setEnabled(Any);
  FPauseAction->setEnabled(Any);
  FResumeAction->setEnabled(Any);
}
