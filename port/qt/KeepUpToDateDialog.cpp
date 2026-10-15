#include "KeepUpToDateDialog.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "QtFrontend.h"
#include "TransferSettingsDialog.h"

using TFirst = CoreBridge::TKeepUpToDateParams::TSynchronizeFirst;
using TLog = CoreBridge::TKeepUpToDateLog;

namespace
{

// TSynchronizeDialog::MaxLogItems.
const int MaxLogItems = 1000;
// A log entry's kind, and an error's more messages.
const int EntryRole = Qt::UserRole;
const int MoreMessagesRole = Qt::UserRole + 1;

}  // namespace

KeepUpToDateDialog::KeepUpToDateDialog(CoreBridge::TSession & Session, const CoreBridge::TKeepUpToDateParams & Params,
  const std::vector<std::string> & Selected, QWidget * Parent) :
  QDialog(Parent),
  FSession(Session),
  FSelected(Selected)
{
  QVBoxLayout * Layout = new QVBoxLayout(this);

  QGroupBox * Directories = new QGroupBox(tr("Directories"), this);
  FDirectories = Directories;
  QGridLayout * DirectoriesLayout = new QGridLayout(Directories);
  FLocalEdit = new QLineEdit(QString::fromStdString(Params.LocalDirectory), Directories);
  FLocalEdit->setMinimumWidth(380);
  QLabel * LocalLabel = new QLabel(tr("&Watch for changes in the local directory:"), Directories);
  LocalLabel->setBuddy(FLocalEdit);
  QPushButton * Browse = new QPushButton(tr("B&rowse..."), Directories);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    const QString Directory = QFileDialog::getExistingDirectory(this, tr("Select local directory"), FLocalEdit->text());
    if (!Directory.isEmpty())
    {
      FLocalEdit->setText(QDir::toNativeSeparators(Directory));
    }
  });
  FRemoteEdit = new QLineEdit(QString::fromStdString(Params.RemoteDirectory), Directories);
  QLabel * RemoteLabel = new QLabel(tr("... &and automatically reflect them on the remote directory:"), Directories);
  RemoteLabel->setBuddy(FRemoteEdit);
  DirectoriesLayout->addWidget(LocalLabel, 0, 0, 1, 2);
  DirectoriesLayout->addWidget(FLocalEdit, 1, 0);
  DirectoriesLayout->addWidget(Browse, 1, 1);
  DirectoriesLayout->addWidget(RemoteLabel, 2, 0, 1, 2);
  DirectoriesLayout->addWidget(FRemoteEdit, 3, 0, 1, 2);
  Layout->addWidget(Directories);

  QGroupBox * Options = new QGroupBox(tr("Synchronize options"), this);
  FOptions = Options;
  QGridLayout * OptionsLayout = new QGridLayout(Options);
  FDelete = new QCheckBox(tr("&Delete files"), Options);
  FExistingOnly = new QCheckBox(tr("&Existing files only"), Options);
  FRecurse = new QCheckBox(tr("Update s&ubdirectories"), Options);
  FSynchronize = new QCheckBox(tr("Synchronize on s&tart"), Options);
  // Grayed: asked on start.
  FSynchronize->setTristate(true);
  FSelectedOnly = new QCheckBox(tr("Selected files o&nly"), Options);
  FContinueOnError = new QCheckBox(tr("Continue on &error"), Options);
  OptionsLayout->addWidget(FDelete, 0, 0);
  OptionsLayout->addWidget(FExistingOnly, 0, 1);
  OptionsLayout->addWidget(FRecurse, 1, 0);
  OptionsLayout->addWidget(FSelectedOnly, 1, 1);
  OptionsLayout->addWidget(FSynchronize, 2, 0);
  OptionsLayout->addWidget(FContinueOnError, 2, 1);
  Layout->addWidget(Options);
  FSaveSettings = new QCheckBox(tr("Use same &options next time"), this);
  Layout->addWidget(FSaveSettings);

  FTransferSettings = new TransferSettingsBox(this);
  Layout->addWidget(FTransferSettings);

  QHBoxLayout * Buttons = new QHBoxLayout();
  Buttons->addStretch();
  FStart = new QPushButton(tr("&Start"), this);
  FStop = new QPushButton(tr("&Stop"), this);
  // As MinimizeButton: split, Minimize to System Tray its menu's, unless
  // Preferences minimize to the tray anyway (or the desktop has none).
  FMinimize = new QToolButton(this);
  FMinimize->setText(tr("&Minimize"));
  FMinimize->setToolButtonStyle(Qt::ToolButtonTextOnly);
  FMinimize->setSizePolicy(FStart->sizePolicy());
  if (!CoreBridge::Preferences().MinimizeToTray && QSystemTrayIcon::isSystemTrayAvailable())
  {
    QMenu * MinimizeMenu = new QMenu(FMinimize);
    MinimizeMenu->addAction(tr("&Minimize"), this, [this]() { Minimize(false); });
    MinimizeMenu->addAction(tr("Minimize to System &Tray"), this, [this]() { Minimize(true); });
    FMinimize->setMenu(MinimizeMenu);
    FMinimize->setPopupMode(QToolButton::MenuButtonPopup);
  }
  FClose = new QPushButton(tr("Close"), this);
  Buttons->addWidget(FStart);
  // As StartButton's split part (AllowStartInNewWindow).
  FStartMenu = new QToolButton(this);
  FStartMenu->setArrowType(Qt::DownArrow);
  FStartMenu->setPopupMode(QToolButton::InstantPopup);
  FStartMenu->setAccessibleName(tr("Start options"));
  QMenu * StartMenu = new QMenu(FStartMenu);
  StartMenu->addAction(tr("&Start"), this, [this]() { Start(); });
  FStartInNewWindow = StartMenu->addAction(tr("Start in &New Window"), this, [this]() { StartInNewWindow(); });
  FStartMenu->setMenu(StartMenu);
  Buttons->addWidget(FStartMenu);
  Buttons->addWidget(FStop);
  Buttons->addWidget(FMinimize);
  Buttons->addWidget(FClose);
  Layout->addLayout(Buttons);
  connect(FStart, &QPushButton::clicked, this, [this]() { Start(); });
  connect(FStop, &QPushButton::clicked, this, &KeepUpToDateDialog::Stop);
  connect(FMinimize, &QToolButton::clicked, this, [this]() { Minimize(false); });
  connect(FClose, &QPushButton::clicked, this, &QDialog::reject);

  FLog = new QTreeWidget(this);
  FLog->setColumnCount(2);
  FLog->setHeaderHidden(true);
  FLog->setRootIsDecorated(false);
  FLog->setUniformRowHeights(true);
  FLog->setMinimumHeight(160);
  FLog->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  connect(FLog, &QTreeWidget::itemDoubleClicked, this, &KeepUpToDateDialog::LogDoubleClicked);
  Layout->addWidget(FLog);

  FTimer = new QTimer(this);
  FTimer->setInterval(100);
  connect(FTimer, &QTimer::timeout, this, &KeepUpToDateDialog::Idle);

  // As TSynchronizeDialog::SetParams.
  FDelete->setChecked(Params.Delete);
  FExistingOnly->setChecked(Params.ExistingOnly);
  FSelectedOnly->setChecked(Params.SelectedOnly);
  FRecurse->setChecked(Params.Recurse);
  FSynchronize->setCheckState((Params.SynchronizeFirst == TFirst::Ask) ? Qt::PartiallyChecked :
    ((Params.SynchronizeFirst == TFirst::Yes) ? Qt::Checked : Qt::Unchecked));
  FContinueOnError->setChecked(Params.ContinueOnError);

  connect(FLocalEdit, &QLineEdit::textChanged, this, &KeepUpToDateDialog::UpdateControls);
  connect(FRemoteEdit, &QLineEdit::textChanged, this, &KeepUpToDateDialog::UpdateControls);
  UpdateControls();
}

KeepUpToDateDialog::~KeepUpToDateDialog()
{
  // Closed some other way than Close, the application quitting say.
  if (FSynchronizing)
  {
    FSynchronizing = false;
    FSession.KeepUpToDateStop();
  }
}

// As TSynchronizeDialog::UpdateControls.
void KeepUpToDateDialog::UpdateControls()
{
  FStart->setEnabled(!FLocalEdit->text().trimmed().isEmpty() && !FRemoteEdit->text().trimmed().isEmpty());
  FStart->setVisible(!FSynchronizing);
  FStartMenu->setVisible(!FSynchronizing && FOnStartInNewWindow);
  FStartMenu->setEnabled(FStart->isEnabled());
  FStartInNewWindow->setEnabled(FStart->isEnabled() && !(FSelectedOnly->isEnabled() && FSelectedOnly->isChecked()));
  FStop->setVisible(FSynchronizing);
  (FSynchronizing ? FStop : FStart)->setDefault(true);
  setWindowTitle(FSynchronizing ? tr("Keeping remote directory up to date ...") :
    tr("Keep remote directory up to date"));
  FTransferSettings->setEnabled(!FSynchronizing);
  FClose->setVisible(!FSynchronizing);
  FDirectories->setEnabled(!FSynchronizing);
  FOptions->setEnabled(!FSynchronizing);
  FSaveSettings->setEnabled(!FSynchronizing);
  FMinimize->setVisible(FSynchronizing);
  FSelectedOnly->setEnabled(!FSynchronizing && !FSelected.empty());
  FLog->setVisible(FSynchronizing || (FLog->topLevelItemCount() > 0));
}

// As TSynchronizeDialog::GetParams.
CoreBridge::TKeepUpToDateParams KeepUpToDateDialog::Params() const
{
  CoreBridge::TKeepUpToDateParams Result;
  Result.LocalDirectory = FLocalEdit->text().trimmed().toStdString();
  Result.RemoteDirectory = FRemoteEdit->text().trimmed().toStdString();
  Result.Delete = FDelete->isChecked();
  Result.ExistingOnly = FExistingOnly->isChecked();
  Result.SelectedOnly = FSelectedOnly->isChecked();
  Result.Recurse = FRecurse->isChecked();
  Result.SynchronizeFirst = (FSynchronize->checkState() == Qt::PartiallyChecked) ? TFirst::Ask :
    ((FSynchronize->checkState() == Qt::Checked) ? TFirst::Yes : TFirst::No);
  Result.ContinueOnError = FContinueOnError->isChecked();
  return Result;
}

bool KeepUpToDateDialog::SaveSettings() const
{
  return FSaveSettings->isChecked();
}

// As TSynchronizeDialog::Start.
bool KeepUpToDateDialog::Start()
{
  if (FSynchronizing || !FStart->isEnabled())
  {
    return false;
  }
  bool Synchronize = (FSynchronize->checkState() == Qt::Checked);
  if (FSynchronize->checkState() == Qt::PartiallyChecked)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreBridge::KeepUpToDateSynchronizeQuestion();
    for (unsigned int Answer : { CoreBridge::AnswerYes, CoreBridge::AnswerNo, CoreBridge::AnswerCancel })
    {
      CoreBridge::TQueryButton Button;
      Button.Answer = Answer;
      Button.Caption = (Answer == CoreBridge::AnswerYes) ? "Yes" : ((Answer == CoreBridge::AnswerNo) ? "No" : "Cancel");
      Query.Buttons.push_back(Button);
    }
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    Query.NeverAskAgainCheck = true;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
    if (Answer.Answer == CoreBridge::AnswerYes)
    {
      Synchronize = true;
      if (Answer.NeverAskAgain)
      {
        FSynchronize->setCheckState(Qt::Checked);
      }
    }
    else if (Answer.Answer == CoreBridge::AnswerNo)
    {
      Synchronize = false;
    }
    else
    {
      return false;
    }
  }

  CoreBridge::TKeepUpToDateParams Started = Params();
  Started.SynchronizeFirst = Synchronize ? TFirst::Yes : TFirst::No;
  CoreBridge::TKeepUpToDateEvents Events;
  Events.OnLog = [this](TLog Entry, const std::string & Message, const std::string & MoreMessages)
  {
    AddLog(Entry, QString::fromStdString(Message), QString::fromStdString(MoreMessages));
  };
  Events.OnAbort = [this](bool Close)
  {
    // As TSynchronizeDialog::Abort: once back from the event.
    QTimer::singleShot(0, this, [this, Close]() { Abort(Close); });
  };
  FLog->clear();
  FSynchronizing = true;
  UpdateControls();
  try
  {
    const CoreBridge::TTransferSettings Settings = FTransferSettings->Settings();
    FSession.KeepUpToDateStart(Started, &Settings, FSelected, Events);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    FSynchronizing = false;
    UpdateControls();
    if (!E.Aborted)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
    return false;
  }
  FTimer->start();
  return true;
}

// As TSynchronizeDialog::Stop.
void KeepUpToDateDialog::Stop()
{
  if (!FSynchronizing)
  {
    return;
  }
  FTimer->stop();
  FSynchronizing = false;
  FSession.KeepUpToDateStop();
  UpdateControls();
  QWidget * Main = (parentWidget() != nullptr) ? parentWidget()->window() : nullptr;
  if ((Main != nullptr) && Main->isMinimized())
  {
    Main->showNormal();
  }
  if (isMinimized())
  {
    showNormal();
  }
}

void KeepUpToDateDialog::Abort(bool Close)
{
  Stop();
  if (Close)
  {
    QDialog::reject();
  }
}

// As FormKeyDown and FormCloseQuery: Escape stops, closing stops first.
void KeepUpToDateDialog::reject()
{
  if (FSynchronizing)
  {
    Stop();
    return;
  }
  QDialog::reject();
}

void KeepUpToDateDialog::Idle()
{
  // A transfer's progress window runs the event loop: no reentry.
  if (FInIdle || !FSynchronizing)
  {
    return;
  }
  FInIdle = true;
  try
  {
    FSession.KeepUpToDateIdle();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    FInIdle = false;
    Stop();
    if (!E.Aborted)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
    return;
  }
  FInIdle = false;
}

// As TSynchronizeDialog::DoLogInternal.
void KeepUpToDateDialog::AddLog(TLog Entry, const QString & Message, const QString & MoreMessages)
{
  QString Text = Message;
  Text.remove(QLatin1Char('\r'));
  Text.replace(QLatin1Char('\n'), QLatin1Char(' '));
  QTreeWidgetItem * Item = new QTreeWidgetItem(FLog);
  Item->setText(0, QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
  Item->setText(1, Text);
  Item->setData(0, EntryRole, static_cast<int>(Entry));
  Item->setData(0, MoreMessagesRole, Message + (MoreMessages.isEmpty() ? QString() : (QStringLiteral("\n") + MoreMessages)));
  if (Entry == TLog::ContinuedError)
  {
    // As LogViewCustomDrawItem.
    Item->setForeground(0, Qt::red);
    Item->setForeground(1, Qt::red);
  }
  while (FLog->topLevelItemCount() > MaxLogItems)
  {
    delete FLog->takeTopLevelItem(0);
  }
  FLog->scrollToItem(Item);
  FLog->setVisible(true);
}

// As TSynchronizeDialog::LogViewDblClick: an error, in full.
void KeepUpToDateDialog::LogDoubleClicked(QTreeWidgetItem * Item)
{
  if ((Item != nullptr) && (Item->data(0, EntryRole).toInt() == static_cast<int>(TLog::ContinuedError)))
  {
    const QString Full = Item->data(0, MoreMessagesRole).toString();
    const int Break = Full.indexOf(QLatin1Char('\n'));
    QMessageBox Box(QMessageBox::Warning, windowTitle(), (Break < 0) ? Full : Full.left(Break), QMessageBox::Ok, this);
    if (Break >= 0)
    {
      Box.setDetailedText(Full.mid(Break + 1));
    }
    Box.exec();
  }
}

void KeepUpToDateDialog::Minimize(bool ToTray)
{
  if (OnMinimize)
  {
    OnMinimize(ToTray);
    return;
  }
  // As TSynchronizeDialog::GlobalMinimize: the application.
  QWidget * Main = (parentWidget() != nullptr) ? parentWidget()->window() : this;
  Main->showMinimized();
  showMinimized();
}

bool KeepUpToDateDialog::StartInNewWindow()
{
  if (!FOnStartInNewWindow || !FStartInNewWindow->isEnabled())
  {
    return false;
  }
  if (!FOnStartInNewWindow(Params(), FTransferSettings->Settings()))
  {
    return false;
  }
  reject();
  return true;
}

void KeepUpToDateDialog::SetStartInNewWindow(TStartInNewWindow Handler)
{
  FOnStartInNewWindow = std::move(Handler);
  UpdateControls();
}
