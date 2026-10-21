#include "TransferDialog.h"
#include "Integration.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

TransferDialog::TransferDialog(bool Upload, int FileCount, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(Upload ? tr("Uploading") : tr("Downloading"));
  setWindowModality(Qt::WindowModal);
  setMinimumWidth(520);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  FFile = new QLabel(this);
  FFile->setTextFormat(Qt::PlainText);
  FFile->setMinimumWidth(320);
  Form->addRow(tr("File:"), FFile);
  FFileBar = new QProgressBar(this);
  FFileBar->setRange(0, 100);
  Form->addRow(tr("File progress:"), FFileBar);
  FOverallBar = new QProgressBar(this);
  FOverallBar->setRange(0, 100);
  Form->addRow(tr("Overall progress:"), FOverallBar);
  Layout->addLayout(Form);

  // As Progress.dfm's TransferPanel: the time left (or, while the total is
  // not known, the start time) and the time elapsed beside the bytes and
  // the speed.
  QGridLayout * Grid = new QGridLayout();
  FTimeLeftLabel = new QLabel(tr("Time left:"), this);
  FTimeLeft = new QLabel(this);
  FStartTimeLabel = new QLabel(tr("Start time:"), this);
  FStartTime = new QLabel(this);
  QLabel * ElapsedLabel = new QLabel(tr("Time elapsed:"), this);
  FTimeElapsed = new QLabel(this);
  QLabel * BytesLabel = new QLabel(tr("Bytes transferred:"), this);
  FBytes = new QLabel(this);
  QLabel * SpeedLabel = new QLabel(tr("Speed:"), this);
  FSpeed = new QLabel(this);
  Grid->addWidget(FTimeLeftLabel, 0, 0);
  Grid->addWidget(FTimeLeft, 0, 1);
  Grid->addWidget(FStartTimeLabel, 0, 0);
  Grid->addWidget(FStartTime, 0, 1);
  Grid->addWidget(ElapsedLabel, 1, 0);
  Grid->addWidget(FTimeElapsed, 1, 1);
  Grid->addWidget(BytesLabel, 0, 2);
  Grid->addWidget(FBytes, 0, 3);
  Grid->addWidget(SpeedLabel, 1, 2);
  Grid->addWidget(FSpeed, 1, 3);
  Grid->setColumnStretch(1, 1);
  Grid->setColumnStretch(3, 1);
  Layout->addLayout(Grid);
  FTimeLeftLabel->hide();
  FTimeLeft->hide();

  // Its toolbar.
  QHBoxLayout * Buttons = new QHBoxLayout();
  FCancel = new QPushButton(tr("Cancel"), this);
  connect(FCancel, &QPushButton::clicked, this, &TransferDialog::reject);
  FSkip = new QPushButton(tr("&Skip this file"), this);
  connect(FSkip, &QPushButton::clicked, this, [this]()
  {
    FSkipAsked = true;
    FSkip->setEnabled(false);
  });
  FMinimize = new QPushButton(tr("&Minimize"), this);
  connect(FMinimize, &QPushButton::clicked, this, [this]()
  {
    // As GlobalMinimize: the application, not only this window.
    QWidget * Window = (parentWidget() != nullptr) ? parentWidget()->window() : this;
    Window->showMinimized();
  });
  FBackground = new QPushButton(tr("Proceed in &Background"), this);
  FBackground->setEnabled(false);
  connect(FBackground, &QPushButton::clicked, this, [this]()
  {
    FBackgroundAsked = true;
    FBackground->setEnabled(false);
  });
  FOnceFinishedButton = new QToolButton(this);
  FOnceFinishedButton->setText(tr("Once Finished"));
  FOnceFinishedButton->setToolTip(tr("Action to perform once the operation finishes"));
  FOnceFinishedButton->setPopupMode(QToolButton::InstantPopup);
  QMenu * OnceMenu = new QMenu(FOnceFinishedButton);
  QActionGroup * OnceGroup = new QActionGroup(OnceMenu);
  const std::pair<QString, TOnceFinished> OnceItems[] = {
    { tr("&Stay Idle"), TOnceFinished::Idle }, { tr("&Disconnect Session"), TOnceFinished::Disconnect },
    { tr("Slee&p Computer"), TOnceFinished::Suspend }, { tr("&Shut Down Computer"), TOnceFinished::ShutDown } };
  for (const auto & Item : OnceItems)
  {
    QAction * Action = OnceMenu->addAction(Item.first);
    Action->setCheckable(true);
    Action->setChecked(Item.second == TOnceFinished::Idle);
    Action->setData(static_cast<int>(Item.second));
    OnceGroup->addAction(Action);
    const TOnceFinished Value = Item.second;
    connect(Action, &QAction::triggered, this, [this, Value]() { SetOnceFinished(Value); });
    if ((Value == TOnceFinished::Suspend) || (Value == TOnceFinished::ShutDown))
    {
      Action->setVisible(Integration::PowerActionsAvailable());
    }
  }
  FOnceFinishedButton->setMenu(OnceMenu);
  FSpeedLimit = new QComboBox(this);
  FSpeedLimit->setEditable(true);
  FSpeedLimit->setAccessibleName(tr("Speed limit (KB/s)"));
  FSpeedLimit->setToolTip(tr("Speed limit (KB/s)"));
  for (const std::string & Limit : CoreBridge::SpeedLimits())
  {
    FSpeedLimit->addItem(QString::fromStdString(Limit));
  }
  connect(FSpeedLimit, &QComboBox::activated, this, &TransferDialog::SpeedLimitEntered);
  connect(FSpeedLimit->lineEdit(), &QLineEdit::editingFinished, this, &TransferDialog::SpeedLimitEntered);
  Buttons->addWidget(FCancel);
  Buttons->addWidget(FSkip);
  Buttons->addWidget(FMinimize);
  Buttons->addWidget(FBackground);
  Buttons->addStretch(1);
  Layout->addLayout(Buttons);
  QHBoxLayout * Options = new QHBoxLayout();
  Options->addWidget(FOnceFinishedButton);
  Options->addStretch(1);
  QLabel * SpeedLimitLabel = new QLabel(tr("Speed limit (KB/s)") + QLatin1Char(':'), this);
  Options->addWidget(SpeedLimitLabel);
  Options->addWidget(FSpeedLimit);
  Layout->addLayout(Options);
  FOverallBar->setFormat(tr("%p% of %1 file(s)").arg(FileCount));
}

void TransferDialog::Update(const CoreBridge::TTransferProgress & Progress)
{
  FFile->setText(QString::fromStdString(Progress.FileName));
  FFileBar->setValue(Progress.FilePercent);
  FOverallBar->setValue(Progress.OverallPercent);
  const QLocale Locale;
  FBytes->setText((Progress.Total >= 0) ?
    tr("%1 of %2").arg(Locale.formattedDataSize(Progress.Transferred), Locale.formattedDataSize(Progress.Total)) :
    Locale.formattedDataSize(Progress.Transferred));
  FSpeed->setText(tr("%1/s").arg(Locale.formattedDataSize(Progress.Speed)));
  // As UpdateControls: the time left once the total is known (estimated
  // after three seconds), the start time before.
  const bool TotalKnown = (Progress.Total >= 0);
  FTimeLeftLabel->setVisible(TotalKnown);
  FTimeLeft->setVisible(TotalKnown);
  FStartTimeLabel->setVisible(!TotalKnown);
  FStartTime->setVisible(!TotalKnown);
  FTimeLeft->setText(QString::fromStdString(Progress.TimeLeft));
  FStartTime->setText(QString::fromStdString(Progress.StartTime));
  FTimeElapsed->setText(QString::fromStdString(Progress.TimeElapsed));
  FSkip->setEnabled(!FCancelled && !FSkipAsked && Progress.CanSkip);
  FBackground->setEnabled(!FCancelled && !FBackgroundAsked && Progress.CanMoveToQueue);
  FBackground->setVisible(Progress.CanMoveToQueue || FBackgroundAsked);
  // The limit in force, until one is chosen here.
  if (!FSpeedLimitShown && !FSpeedLimit->hasFocus())
  {
    FSpeedLimitShown = true;
    FSpeedLimit->setCurrentText(QString::fromStdString(CoreBridge::SpeedLimitText(Progress.SpeedLimit)));
  }
}

void TransferDialog::SpeedLimitEntered()
{
  const std::string Text = FSpeedLimit->currentText().trimmed().toStdString();
  try
  {
    FSpeedLimitValue = CoreBridge::SpeedLimitOf(Text);
    FSpeedLimitSet = true;
    FSpeedLimitShown = true;
    // ItemSpeed: shown normalized, and remembered.
    const QString Normalized = QString::fromStdString(CoreBridge::SpeedLimitText(FSpeedLimitValue));
    FSpeedLimit->setCurrentText(Normalized);
    CoreBridge::AddToHistory("SpeedLimit", Normalized.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QMessageBox::warning(this, windowTitle(), QString::fromStdString(E.Error.Message));
  }
}

CoreBridge::TTransferAnswer TransferDialog::Answer()
{
  CoreBridge::TTransferAnswer Result;
  using TAction = CoreBridge::TTransferAnswer::TAction;
  if (FCancelled)
  {
    Result.Action = TAction::Cancel;
  }
  else if (FSkipAsked)
  {
    // As ClearCancel: asked once, then the next file goes on.
    Result.Action = TAction::SkipFile;
    FSkipAsked = false;
  }
  else if (FBackgroundAsked)
  {
    Result.Action = TAction::MoveToQueue;
  }
  if (FSpeedLimitSet)
  {
    Result.SpeedLimitSet = true;
    Result.SpeedLimit = FSpeedLimitValue;
  }
  return Result;
}

void TransferDialog::SetOnceFinished(TOnceFinished OnceFinished)
{
  FOnceFinished = OnceFinished;
  for (QAction * Action : FOnceFinishedButton->menu()->actions())
  {
    Action->setChecked(Action->data().toInt() == static_cast<int>(OnceFinished));
  }
}

// Cancel, Escape and closing the window cancel the transfer; core stops at
// its next progress report, and the window closes when the transfer ends.
void TransferDialog::reject()
{
  FCancelled = true;
  FCancel->setEnabled(false);
  FSkip->setEnabled(false);
  FBackground->setEnabled(false);
}
