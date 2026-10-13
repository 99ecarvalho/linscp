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

namespace
{

enum { NameColumn, SizeColumn, ModifiedColumn, RightsColumn };
const char * const ParentEntry = "..";

}  // namespace

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));
  resize(900, 560);

  QMenu * FileMenu = menuBar()->addMenu(tr("&File"));
  FOpenAction = FileMenu->addAction(tr("&Open URL..."), this, &MainWindow::OpenUrlDialog);
  FOpenAction->setShortcut(QKeySequence::Open);
  FDisconnectAction = FileMenu->addAction(tr("&Disconnect"), this, &MainWindow::Disconnect);
  FileMenu->addSeparator();
  QAction * Quit = FileMenu->addAction(tr("E&xit"), this, &QWidget::close);
  Quit->setShortcut(QKeySequence::Quit);
  QMenu * HelpMenu = menuBar()->addMenu(tr("&Help"));
  HelpMenu->addAction(tr("&About..."), this, &MainWindow::About);

  QWidget * Central = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Central);
  FPath = new QLineEdit(Central);
  FPath->setReadOnly(true);
  FPath->setPlaceholderText(tr("Not connected: File > Open URL..."));
  Layout->addWidget(FPath);
  FFiles = new QTreeWidget(Central);
  FFiles->setHeaderLabels({ tr("Name"), tr("Size"), tr("Changed"), tr("Rights") });
  FFiles->setRootIsDecorated(false);
  FFiles->setSortingEnabled(false);
  FFiles->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
  connect(FFiles, &QTreeWidget::itemActivated, this, &MainWindow::ItemActivated);
  Layout->addWidget(FFiles);
  setCentralWidget(Central);

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
    std::unique_ptr<CoreBridge::TSession> Session(new CoreBridge::TSession(Url.toStdString()));
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
  Refresh();
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
  Refresh();
  UpdateActions();
}

void MainWindow::Refresh()
{
  FFiles->clear();
  if (!FSession || !FSession->Active())
  {
    FPath->clear();
    return;
  }
  FPath->setText(QString::fromStdString(FSession->CurrentDirectory()));
  const QIcon FolderIcon = style()->standardIcon(QStyle::SP_DirIcon);
  const QIcon FileIcon = style()->standardIcon(QStyle::SP_FileIcon);
  if (FSession->CurrentDirectory() != "/")
  {
    QTreeWidgetItem * Up = new QTreeWidgetItem(FFiles, { QString::fromLatin1(ParentEntry) });
    Up->setIcon(NameColumn, style()->standardIcon(QStyle::SP_FileDialogToParent));
    Up->setData(NameColumn, Qt::UserRole, true);
  }
  // Directories first, then files, each by name.
  std::vector<CoreBridge::TRemoteEntry> Entries = FSession->List();
  std::stable_sort(Entries.begin(), Entries.end(), [](const CoreBridge::TRemoteEntry & A, const CoreBridge::TRemoteEntry & B)
  {
    return (A.Directory != B.Directory) ? A.Directory :
      (QString::fromStdString(A.Name).compare(QString::fromStdString(B.Name), Qt::CaseInsensitive) < 0);
  });
  const QLocale Locale;
  for (const CoreBridge::TRemoteEntry & Entry : Entries)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FFiles);
    Item->setText(NameColumn, QString::fromStdString(Entry.Name));
    Item->setIcon(NameColumn, Entry.Directory ? FolderIcon : FileIcon);
    Item->setData(NameColumn, Qt::UserRole, Entry.Directory);
    if (!Entry.Directory)
    {
      Item->setText(SizeColumn, Locale.toString(static_cast<qlonglong>(Entry.Size)));
      Item->setTextAlignment(SizeColumn, Qt::AlignRight | Qt::AlignVCenter);
    }
    Item->setText(ModifiedColumn, QString::fromStdString(Entry.Modified));
    Item->setText(RightsColumn, QString::fromStdString(Entry.Rights));
  }
}

void MainWindow::ItemActivated(QTreeWidgetItem * Item, int /*Column*/)
{
  if (!FSession || FBusy || !Item->data(NameColumn, Qt::UserRole).toBool())
  {
    return;
  }
  FBusy = true;
  try
  {
    FSession->ChangeDirectory(Item->text(NameColumn).toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  Refresh();
}

void MainWindow::UpdateActions()
{
  FOpenAction->setEnabled(!FBusy);
  FDisconnectAction->setEnabled(!FBusy && FSession);
}

void MainWindow::About()
{
  QMessageBox::about(this, tr("About %1").arg(QString::fromStdString(FInfo.AppName)),
    tr("%1 %2\nQt %3\nConfiguration: %4").arg(
      QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version),
      QString::fromLatin1(qVersion()), QString::fromStdString(FInfo.ConfigurationStorage)));
}
