#include "MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));
  resize(800, 500);

  QMenu * FileMenu = menuBar()->addMenu(tr("&File"));
  QAction * Quit = FileMenu->addAction(tr("E&xit"), this, &QWidget::close);
  Quit->setShortcut(QKeySequence::Quit);
  QMenu * HelpMenu = menuBar()->addMenu(tr("&Help"));
  HelpMenu->addAction(tr("&About..."), this, &MainWindow::About);

  QLabel * Placeholder = new QLabel(
    tr("%1 %2").arg(QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version)), this);
  Placeholder->setAlignment(Qt::AlignCenter);
  setCentralWidget(Placeholder);
}

void MainWindow::About()
{
  QMessageBox::about(this, tr("About %1").arg(QString::fromStdString(FInfo.AppName)),
    tr("%1 %2\nQt %3\nConfiguration: %4").arg(
      QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version),
      QString::fromLatin1(qVersion()), QString::fromStdString(FInfo.ConfigurationStorage)));
}
