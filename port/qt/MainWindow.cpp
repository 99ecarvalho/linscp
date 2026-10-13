#include "MainWindow.h"

#include <QApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QTimer>
#include <QTreeView>
#include <QSplitter>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "QtFrontend.h"
#include "LocalPanel.h"
#include "QueuePanel.h"
#include "RemotePanel.h"
#include "SiteManager.h"

namespace
{


}  // namespace

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));
  resize(900, 560);

  QMenu * FileMenu = menuBar()->addMenu(tr("&File"));
  FSitesAction = FileMenu->addAction(tr("&Site Manager..."), this, &MainWindow::SiteManagerDialog);
  FSitesAction->setShortcut(QKeySequence::New);
  FOpenAction = FileMenu->addAction(tr("&Open URL..."), this, &MainWindow::OpenUrlDialog);
  FOpenAction->setShortcut(QKeySequence::Open);
  FDisconnectAction = FileMenu->addAction(tr("&Disconnect"), this, &MainWindow::Disconnect);
  FileMenu->addSeparator();
  QAction * Quit = FileMenu->addAction(tr("E&xit"), this, &QWidget::close);
  Quit->setShortcut(QKeySequence::Quit);
  QMenu * HelpMenu = menuBar()->addMenu(tr("&Help"));
  HelpMenu->addAction(tr("&About..."), this, &MainWindow::About);

  // The panels side by side, the queue below them, as WinSCP's Commander.
  QSplitter * Vertical = new QSplitter(Qt::Vertical, this);
  QSplitter * Panels = new QSplitter(Vertical);
  FLocal = new LocalPanel(Panels);
  FPanel = new RemotePanel(Panels);
  Panels->setChildrenCollapsible(false);
  FQueue = new QueuePanel(Vertical);
  Vertical->setStretchFactor(0, 4);
  Vertical->setStretchFactor(1, 1);
  setCentralWidget(Vertical);
  FQueueTimer = new QTimer(this);
  FQueueTimer->setInterval(250);
  connect(FQueueTimer, &QTimer::timeout, this, &MainWindow::QueueIdle);

  // The active panel follows the focus, as in WinSCP's Commander.
  connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget * Now)
  {
    if ((Now != nullptr) && FLocal->isAncestorOf(Now))
    {
      FLocalActive = true;
    }
    else if ((Now != nullptr) && FPanel->isAncestorOf(Now))
    {
      FLocalActive = false;
    }
  });

  QMenu * FilesMenu = menuBar()->addMenu(tr("F&iles"));
  FCopyAction = FilesMenu->addAction(tr("&Copy..."), this, [this]() { TransferSelected(false, true); });
  FCopyAction->setShortcut(QKeySequence(Qt::Key_F5));
  FMoveAction = FilesMenu->addAction(tr("&Move..."), this, [this]() { TransferSelected(true, true); });
  FMoveAction->setShortcut(QKeySequence(Qt::Key_F6));

  QMenu * ViewMenu = menuBar()->addMenu(tr("&View"));
  QAction * Hidden = ViewMenu->addAction(tr("Show &Hidden Files"));
  Hidden->setCheckable(true);
  Hidden->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
  connect(Hidden, &QAction::toggled, this, [this](bool Show)
  {
    FLocal->SetShowHidden(Show);
    FPanel->SetShowHidden(Show);
  });

  statusBar()->showMessage(tr("%1 %2").arg(QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version)));
  UpdateActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

bool MainWindow::OpenUrl(const QString & Url)
{
  try
  {
    return Open(std::unique_ptr<CoreBridge::TSession>(new CoreBridge::TSession(Url.toStdString())));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

bool MainWindow::OpenSite(const CoreBridge::TSite & Site)
{
  try
  {
    return Open(std::unique_ptr<CoreBridge::TSession>(new CoreBridge::TSession(Site)));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

bool MainWindow::SiteManagerDialog()
{
  if (FBusy)
  {
    return false;
  }
  SiteManager Dialog(this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  return OpenSite(Dialog.Site());
}

bool MainWindow::Open(std::unique_ptr<CoreBridge::TSession> Session)
{
  if (FBusy)
  {
    return false;
  }
  Disconnect();
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    statusBar()->showMessage(tr("Connecting to %1...").arg(QString::fromStdString(Session->Name())));
    Session->Open();
    FSession = std::move(Session);
    // As WinSCP: the site's local directory, when it has one.
    if (!FSession->LocalDirectory().empty())
    {
      FLocal->ChangeDirectory(QString::fromStdString(FSession->LocalDirectory()));
    }
    setWindowTitle(tr("%1 - %2").arg(QString::fromStdString(FSession->Name()), QString::fromStdString(FInfo.AppName)));
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  statusBar()->showMessage(Result ? tr("Connected") : tr("Not connected"));
  FPanel->SetSession(FSession.get());
  FQueue->SetSession(FSession.get());
  FQueueDone = 0;
  if (FSession)
  {
    FQueueTimer->start();
  }
  else
  {
    FQueueTimer->stop();
  }
  UpdateActions();
  return Result;
}

void MainWindow::OpenUrlDialog()
{
  bool OK = false;
  const QString Url = QInputDialog::getText(this, tr("Open URL"),
    tr("Session URL (sftp://, scp://, ftp://, ftps://, ftpes://, dav://, davs://, s3://):"),
    QLineEdit::Normal, QStringLiteral("sftp://"), &OK);
  if (OK && !Url.trimmed().isEmpty())
  {
    OpenUrl(Url.trimmed());
  }
}

void MainWindow::Disconnect()
{
  if (FSession)
  {
    FSession->Close();
    FSession.reset();
    setWindowTitle(QString::fromStdString(FInfo.AppName));
    statusBar()->showMessage(tr("Disconnected"));
  }
  FPanel->SetSession(FSession.get());
  FQueue->SetSession(FSession.get());
  FQueueDone = 0;
  if (FSession)
  {
    FQueueTimer->start();
  }
  else
  {
    FQueueTimer->stop();
  }
  UpdateActions();
}

bool MainWindow::Upload(const QStringList & Names, const QString & Target, bool Move)
{
  if (FBusy || !FSession || Names.isEmpty())
  {
    return false;
  }
  std::vector<std::string> Paths;
  for (const QString & Name : Names)
  {
    Paths.push_back(QDir::toNativeSeparators(QDir(FLocal->Path()).filePath(Name)).toStdString());
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    Result = FSession->Upload(Paths, Target.toStdString(), Move);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  FLocal->Refresh();
  UpdateActions();
  return Result;
}

bool MainWindow::Download(const QStringList & Names, const QString & Target, bool Move)
{
  if (FBusy || !FSession || Names.isEmpty())
  {
    return false;
  }
  std::vector<std::string> List;
  for (const QString & Name : Names)
  {
    List.push_back(Name.toStdString());
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    Result = FSession->Download(List, QDir::toNativeSeparators(Target).toStdString(), Move);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  FLocal->Refresh();
  UpdateActions();
  return Result;
}

void MainWindow::ActivatePanel(bool Local)
{
  FLocalActive = Local;
  (Local ? static_cast<QWidget *>(FLocal->View()) : static_cast<QWidget *>(FPanel->View()))->setFocus();
}

void MainWindow::QueueIdle()
{
  if (!FSession || !FSession->Active() || FBusy)
  {
    return;
  }
  bool Changed = false;
  try
  {
    Changed = FSession->QueueIdle();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  if (Changed)
  {
    FQueue->Update();
  }
  // Finished transfers change the directories: as WinSCP, the panels read
  // theirs again.
  const int Done = FSession ? FSession->QueueDoneCount() : 0;
  if (Done != FQueueDone)
  {
    FQueueDone = Done;
    FPanel->Refresh();
    FLocal->Refresh();
    FQueue->Update();
  }
}

bool MainWindow::TransferSelected(bool Move, bool Confirm, bool Background)
{
  const bool FromLocal = FLocalActive;
  const QStringList Names = FromLocal ? FLocal->Selected() : FPanel->Selected();
  if (Names.isEmpty() || !FSession || !FSession->Active())
  {
    return false;
  }
  QString Target = FromLocal ? FPanel->Path() : QDir::toNativeSeparators(FLocal->Path());
  if (Confirm)
  {
    // As WinSCP's Copy dialog: where to, editable.
    const QString What = (Names.size() == 1) ? QStringLiteral("'%1'").arg(Names.first()) : tr("%n file(s)", nullptr, Names.size());
    const QString Prompt = FromLocal ?
      (Move ? tr("Move %1 to remote directory:") : tr("Upload %1 to remote directory:")).arg(What) :
      (Move ? tr("Move %1 to local directory:") : tr("Download %1 to local directory:")).arg(What);
    QDialog Dialog(this);
    Dialog.setWindowTitle(Move ? tr("Move") : tr("Copy"));
    QFormLayout * Layout = new QFormLayout(&Dialog);
    QLineEdit * TargetEdit = new QLineEdit(Target, &Dialog);
    TargetEdit->setMinimumWidth(380);
    Layout->addRow(new QLabel(Prompt, &Dialog));
    Layout->addRow(TargetEdit);
    QCheckBox * InBackground = new QCheckBox(tr("Transfer in &background (add to transfer queue)"), &Dialog);
    InBackground->setChecked(Background);
    Layout->addRow(InBackground);
    QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
    connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    Layout->addRow(Box);
    if ((Dialog.exec() != QDialog::Accepted) || TargetEdit->text().trimmed().isEmpty())
    {
      return false;
    }
    Target = TargetEdit->text().trimmed();
    Background = InBackground->isChecked();
  }
  if (Background)
  {
    std::vector<std::string> List;
    for (const QString & Name : Names)
    {
      List.push_back(FromLocal ? QDir::toNativeSeparators(QDir(FLocal->Path()).filePath(Name)).toStdString() : Name.toStdString());
    }
    try
    {
      if (FromLocal)
      {
        FSession->QueueUpload(List, Target.toStdString(), Move);
      }
      else
      {
        FSession->QueueDownload(List, QDir::toNativeSeparators(Target).toStdString(), Move);
      }
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return false;
    }
    QueueIdle();
    return true;
  }
  return FromLocal ? Upload(Names, Target, Move) : Download(Names, Target, Move);
}

void MainWindow::UpdateActions()
{
  FOpenAction->setEnabled(!FBusy);
  FSitesAction->setEnabled(!FBusy);
  FCopyAction->setEnabled(!FBusy && FSession);
  FMoveAction->setEnabled(!FBusy && FSession);
  FDisconnectAction->setEnabled(!FBusy && FSession);
}

void MainWindow::About()
{
  QMessageBox::about(this, tr("About %1").arg(QString::fromStdString(FInfo.AppName)),
    tr("%1 %2\nQt %3\nConfiguration: %4").arg(
      QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version),
      QString::fromLatin1(qVersion()), QString::fromStdString(FInfo.ConfigurationStorage)));
}
