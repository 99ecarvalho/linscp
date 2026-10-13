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
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "QtFrontend.h"
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

  FPanel = new RemotePanel(this);
  setCentralWidget(FPanel);

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
  UpdateActions();
}

void MainWindow::UpdateActions()
{
  FOpenAction->setEnabled(!FBusy);
  FSitesAction->setEnabled(!FBusy);
  FDisconnectAction->setEnabled(!FBusy && FSession);
}

void MainWindow::About()
{
  QMessageBox::about(this, tr("About %1").arg(QString::fromStdString(FInfo.AppName)),
    tr("%1 %2\nQt %3\nConfiguration: %4").arg(
      QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version),
      QString::fromLatin1(qVersion()), QString::fromStdString(FInfo.ConfigurationStorage)));
}
