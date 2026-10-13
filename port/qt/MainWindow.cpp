#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QFormLayout>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QSplitter>
#include <QScreen>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "FileList.h"
#include "QtFrontend.h"
#include "LocalPanel.h"
#include "QueuePanel.h"
#include "RemotePanel.h"
#include "SiteManager.h"
#include "PreferencesDialog.h"
#include "SynchronizeDialog.h"
#include "TransferSettingsDialog.h"

namespace
{


}  // namespace

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));

  QMenu * FileMenu = menuBar()->addMenu(tr("&File"));
  FSitesAction = FileMenu->addAction(tr("&Site Manager..."), this, &MainWindow::SiteManagerDialog);
  FSitesAction->setShortcut(QKeySequence::New);
  FOpenAction = FileMenu->addAction(tr("&Open URL..."), this, &MainWindow::OpenUrlDialog);
  FOpenAction->setShortcut(QKeySequence::Open);
  // As WinSCP: the last session closed, the window too unless kept open.
  FDisconnectAction = FileMenu->addAction(tr("&Disconnect"), this, [this]()
  {
    Disconnect();
    if (!CoreBridge::Preferences().KeepOpenWhenNoSession)
    {
      close();
    }
  });
  FileMenu->addSeparator();
  QAction * Quit = FileMenu->addAction(tr("E&xit"), this, &QWidget::close);
  // F10 too, as WinSCP's Commander.
  Quit->setShortcuts({ QKeySequence(QKeySequence::Quit), QKeySequence(Qt::Key_F10) });
  QMenu * HelpMenu = menuBar()->addMenu(tr("&Help"));
  HelpMenu->addAction(tr("&About..."), this, &MainWindow::About);

  // The panels side by side, the queue below them, as WinSCP's Commander.
  QSplitter * Vertical = new QSplitter(Qt::Vertical, this);
  FVertical = Vertical;
  QSplitter * Panels = new QSplitter(Vertical);
  FPanels = Panels;
  FLocal = new LocalPanel(Panels);
  FPanel = new RemotePanel(Panels);
  Panels->setChildrenCollapsible(false);
  FQueue = new QueuePanel(Vertical);
  // Dropped: handled once the drag is over (its loop is the source's, and on
  // Windows the system's), with a copy of what was dropped.
  for (FileView * View : { FLocal->View(), FPanel->View() })
  {
    const bool OnLocal = (View == FLocal->View());
    connect(View, &FileView::Dropped, this, [this, OnLocal](const QMimeData * Data, const QString & OnDirectory, Qt::DropAction Action)
    {
      // A panel's drag is taken by what it says: its URLs, a remote one's
      // download for other programs, are not read.
      std::shared_ptr<QMimeData> Copy(new QMimeData());
      const QString PanelFormat = QString::fromLatin1(FileDragMimeType);
      if (Data->hasFormat(PanelFormat))
      {
        Copy->setData(PanelFormat, Data->data(PanelFormat));
      }
      else
      {
        for (const QString & Format : Data->formats())
        {
          Copy->setData(Format, Data->data(Format));
        }
        if (Data->hasUrls())
        {
          Copy->setUrls(Data->urls());
        }
      }
      QTimer::singleShot(0, this, [this, OnLocal, Copy, OnDirectory, Action]()
      {
        Drop(OnLocal, Copy.get(), OnDirectory, Action == Qt::MoveAction);
      });
    });
  }
  // Remote files dragged to other programs: downloaded, at the drop, to a
  // temporary directory of their own, kept while the window is (WinSCP
  // deletes its after a delay; the program dropped on may still be copying).
  FPanel->MakeDrag = [this](const TFileDrag & Drag) -> QMimeData *
  {
    // Under Storage's temporary directory (else the system's), as WinSCP's
    // (TWinConfiguration::TemporaryDir: "scp" and a unique part).
    FDragDirectories.push_back(std::make_unique<QTemporaryDir>(
      QDir(QString::fromStdString(CoreBridge::TemporaryDirectoryBase())).filePath(QStringLiteral("scpXXXXXX"))));
    const QString Directory = FDragDirectories.back()->path();
    const QStringList Names = Drag.Names;
    return new RemoteFileDrag(Drag, Directory, [this, Names, Directory]()
    {
      FDragOuts++;
      return Download(Names, Directory, false);
    });
  };
  // Tab: to the other panel.
  connect(FLocal->View(), &FileView::SwitchPanel, this, [this]() { ActivatePanel(false); });
  connect(FPanel->View(), &FileView::SwitchPanel, this, [this]() { ActivatePanel(true); });
  // Ctrl+[ and Ctrl+]: the local and remote paths to the clipboard.
  QAction * LocalPathAction = new QAction(this);
  LocalPathAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
  connect(LocalPathAction, &QAction::triggered, this, [this]()
  {
    QApplication::clipboard()->setText(QDir::toNativeSeparators(FLocal->Path()));
  });
  addAction(LocalPathAction);
  QAction * RemotePathAction = new QAction(this);
  RemotePathAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
  connect(RemotePathAction, &QAction::triggered, this, [this]() { QApplication::clipboard()->setText(FPanel->Path()); });
  addAction(RemotePathAction);
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

  QMenu * CommandsMenu = menuBar()->addMenu(tr("&Commands"));
  FSynchronizeAction = CommandsMenu->addAction(tr("&Synchronize..."), this, [this]() { Synchronize(true); });
  FSynchronizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));

  QMenu * ViewMenu = menuBar()->addMenu(tr("&View"));
  FHiddenAction = ViewMenu->addAction(tr("Show &Hidden Files"));
  FHiddenAction->setCheckable(true);
  FHiddenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_H));
  // As WinSCP's: a setting, kept (WinConfiguration's ShowHiddenFiles).
  connect(FHiddenAction, &QAction::triggered, this, [this](bool Show)
  {
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.ShowHiddenFiles = Show;
    try
    {
      CoreBridge::SetPreferences(Preferences);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
    ApplyPreferences();
  });

  QMenu * OptionsMenu = menuBar()->addMenu(tr("&Options"));
  QAction * PreferencesAction = OptionsMenu->addAction(tr("&Preferences..."), this, [this]() { PreferencesDialog(); });
  PreferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));

  statusBar()->showMessage(tr("%1 %2").arg(QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version)));
  // From now on a language chosen applies on the next start, as WinSCP's.
  CoreBridge::GuiStarted();
  ApplyPreferences();
  RestoreLayout();
  connect(FPanel, &RemotePanel::Changed, this, &MainWindow::UpdateTitle);
  UpdateActions();
}

// As TTerminalManager::UpdateAppTitle and PathForCaption: the remote
// directory (short: its last part, full, or none), the session, the
// program.
void MainWindow::UpdateTitle()
{
  QString Title = QString::fromStdString(FInfo.AppName);
  if (FSession && FSession->Active())
  {
    Title = tr("%1 - %2").arg(QString::fromStdString(FSession->Name()), Title);
    QString Path = FPanel->Path();
    switch (CoreBridge::Preferences().PathInCaption)
    {
      case CoreBridge::TPreferences::TPathInCaption::Short:
      {
        // ExtractShortName: the last part, the root as is.
        QString Trimmed = Path;
        while ((Trimmed.size() > 1) && Trimmed.endsWith(QLatin1Char('/')))
        {
          Trimmed.chop(1);
        }
        const QString Last = Trimmed.mid(Trimmed.lastIndexOf(QLatin1Char('/')) + 1);
        Path = Last.isEmpty() ? Trimmed : Last;
        break;
      }
      case CoreBridge::TPreferences::TPathInCaption::None:
        Path.clear();
        break;
      default:
        break;
    }
    if (!Path.isEmpty())
    {
      Title = tr("%1 - %2").arg(Path, Title);
    }
  }
  setWindowTitle(Title);
}

// As TScpCommanderForm's RestoreParams: the window's placement, the panels'
// split and columns, the queue's height, the active panel; WinSCP's
// formats (TWinConfiguration's ScpCommander, QueueView).
void MainWindow::RestoreLayout()
{
  const CoreBridge::TLayout Layout = CoreBridge::Layout();
  const int Dpi = logicalDpiX();
  const QStringList Window = QString::fromStdString(Layout.WindowParams).split(QLatin1Char(';'));
  QScreen * Screen = this->screen();
  const QRect Area = (Screen != nullptr) ? Screen->availableGeometry() : QRect(0, 0, 1280, 800);
  if (Window.size() >= 6)
  {
    // left;top;right;bottom;state;ppi, on the window's monitor, at the
    // pixels per inch saved; -1;-1;width;height when never placed.
    const int Saved = (Window[5].toInt() > 0) ? Window[5].toInt() : Dpi;
    const auto Scale = [Dpi, Saved](int Value) { return Value * Dpi / Saved; };
    if (Window[0].toInt() == -1)
    {
      resize(Scale(Window[2].toInt()), Scale(Window[3].toInt()));
    }
    else
    {
      setGeometry(QRect(QPoint(Area.left() + Scale(Window[0].toInt()), Area.top() + Scale(Window[1].toInt())),
        QPoint(Area.left() + Scale(Window[2].toInt()) - 1, Area.top() + Scale(Window[3].toInt()) - 1)));
    }
    if (Window[4].toInt() == 2)
    {
      setWindowState(windowState() | Qt::WindowMaximized);
    }
  }
  else
  {
    // TWinConfiguration::Default's: the work area, less a margin, at most
    // 1090 by 700.
    resize(std::min(Area.width() - 40, 1090 * Dpi / 96), std::min(Area.height() - 30, 700 * Dpi / 96));
  }
  FLocal->SetColumnsParams(QString::fromStdString(Layout.LocalDirViewParams));
  FPanel->SetColumnsParams(QString::fromStdString(Layout.RemoteDirViewParams));
  const int Total = std::max(FPanels->width(), width());
  const int LocalWidth = static_cast<int>(Total * std::clamp(Layout.LocalPanelWidth, 0.1, 0.9));
  FPanels->setSizes(FPanels->widget(0) == FLocal ? QList<int>{ LocalWidth, Total - LocalWidth } :
    QList<int>{ Total - LocalWidth, LocalWidth });
  const int QueueHeight = Layout.QueueViewHeight * Dpi / std::max(1, Layout.QueueViewHeightPixelsPerInch);
  FVertical->setSizes({ std::max(100, height() - QueueHeight), QueueHeight });
  ActivatePanel(Layout.CurrentPanel == 0);
}

void MainWindow::SaveLayout()
{
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  const int Dpi = logicalDpiX();
  QScreen * Screen = this->screen();
  const QRect Area = (Screen != nullptr) ? Screen->availableGeometry() : QRect();
  // StoreForm: relative to the monitor; normal bounds even when maximized.
  const QRect Bounds = isMaximized() ? normalGeometry() : geometry();
  Layout.WindowParams = QStringLiteral("%1;%2;%3;%4;%5;%6")
    .arg(Bounds.left() - Area.left()).arg(Bounds.top() - Area.top())
    .arg(Bounds.right() + 1 - Area.left()).arg(Bounds.bottom() + 1 - Area.top())
    .arg(isMaximized() ? 2 : 0).arg(Dpi).toStdString();
  const QList<int> Sizes = FPanels->sizes();
  const int Total = std::max(1, Sizes.value(0) + Sizes.value(1));
  const int LocalWidth = (FPanels->widget(0) == FLocal) ? Sizes.value(0) : Sizes.value(1);
  Layout.LocalPanelWidth = static_cast<double>(LocalWidth) / Total;
  Layout.CurrentPanel = FLocalActive ? 0 : 1;
  Layout.LocalDirViewParams = FLocal->ColumnsParams().toStdString();
  Layout.RemoteDirViewParams = FPanel->ColumnsParams().toStdString();
  Layout.QueueViewHeight = FVertical->sizes().value(1);
  Layout.QueueViewHeightPixelsPerInch = Dpi;
  try
  {
    CoreBridge::SetLayout(Layout);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
}

bool MainWindow::PreferencesDialog()
{
  ::PreferencesDialog Dialog(this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  try
  {
    Dialog.Save();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  ApplyPreferences();
  return true;
}

// What the preferences say of the window: the panels', their order, the
// keys, Home of the local panel.
void MainWindow::ApplyPreferences()
{
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  FLocal->ApplyPreferences(Preferences);
  FPanel->ApplyPreferences(Preferences);
  FHiddenAction->setChecked(Preferences.ShowHiddenFiles);
  // Commander: local left, remote right, unless swapped.
  FPanels->insertWidget(0, Preferences.SwappedPanels ? static_cast<QWidget *>(FPanel) : static_cast<QWidget *>(FLocal));
  // NonVisual.cpp's: Explorer's keys copy with Ctrl+K, as F5 refreshes.
  FCopyAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::CTRL | Qt::Key_K) : QKeySequence(Qt::Key_F5));
  // As TScpCommanderForm::ConfigurationChanged: the site's local directory is
  // Home, with "default directory is home", unless it remembers the last.
  FLocal->HomeDirectory =
    (Preferences.DefaultDirIsHome && FSession && !FSession->UpdateDirectories() && !FSession->LocalDirectory().empty()) ?
    QString::fromStdString(FSession->LocalDirectory()) : QString();
  UpdateTitle();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent * Event)
{
  SaveLayout();
  Disconnect();
  QMainWindow::closeEvent(Event);
}

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
  UpdateTitle();
  UpdateActions();
  bool Result = false;
  try
  {
    statusBar()->showMessage(tr("Connecting to %1...").arg(QString::fromStdString(Session->Name())));
    Session->Open();
    FSession = std::move(Session);
    ApplyPreferences();
    // As WinSCP (TScpCommanderForm::RestoreSessionLocalDirView): the site's
    // local directory, when it has one; when remembered, one that is gone
    // falls back to the nearest that is not, quietly.
    if (!FSession->LocalDirectory().empty())
    {
      QString Directory = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(FSession->LocalDirectory())));
      if (FSession->UpdateDirectories())
      {
        while (!QFileInfo(Directory).isDir() && !QDir(Directory).isRoot() && !Directory.isEmpty())
        {
          const QString Parent = QFileInfo(Directory).path();
          if (Parent == Directory)
          {
            break;
          }
          Directory = Parent;
        }
      }
      if (QFileInfo(Directory).isDir() || !FSession->UpdateDirectories())
      {
        FLocal->ChangeDirectory(Directory);
      }
    }
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
    // As WinSCP: the directories kept for next time, when the site does.
    try
    {
      FSession->SaveDirectories(QDir::toNativeSeparators(FLocal->Path()).toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
    FSession->Close();
    FSession.reset();
    FLocal->HomeDirectory.clear();
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
  UpdateTitle();
  UpdateActions();
}

bool MainWindow::Upload(const QStringList & Names, const QString & Target, bool Move,
  const CoreBridge::TTransferSettings * Settings)
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
    Result = FSession->Upload(Paths, Target.toStdString(), Move, Settings);
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

bool MainWindow::Download(const QStringList & Names, const QString & Target, bool Move,
  const CoreBridge::TTransferSettings * Settings)
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
    Result = FSession->Download(List, QDir::toNativeSeparators(Target).toStdString(), Move, Settings);
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
  const QString Target = FromLocal ? FPanel->Path() : QDir::toNativeSeparators(FLocal->Path());
  return Transfer(FromLocal, Names, Target, Move, Confirm, Background);
}

// Into a directory of the panel or, with OnDirectory empty, the panel's own.
static QString DropTarget(const QString & Path, const QString & OnDirectory, bool Remote)
{
  if (OnDirectory.isEmpty())
  {
    return Path;
  }
  if (Remote)
  {
    return Path.endsWith(QLatin1Char('/')) ? (Path + OnDirectory) : (Path + QLatin1Char('/') + OnDirectory);
  }
  return QDir::cleanPath(QDir(Path).filePath(OnDirectory));
}

bool MainWindow::Drop(bool OnLocal, const QMimeData * Data, const QString & OnDirectory, bool Move, bool Confirm)
{
  if (FBusy)
  {
    return false;
  }
  TFileDrag Drag;
  if (ReadFileDrag(Data, Drag))
  {
    if (Drag.Local == OnLocal)
    {
      // Within the panel: onto one of its directories, moved there, as
      // WinSCP's panels do; onto the panel itself, nothing.
      if (OnDirectory.isEmpty())
      {
        return false;
      }
      if (Confirm)
      {
        const QString What = (Drag.Names.size() == 1) ? QStringLiteral("'%1'").arg(Drag.Names.first()) :
          tr("%n file(s)", nullptr, Drag.Names.size());
        if (QMessageBox::question(this, tr("Move"), tr("Move %1 to '%2'?").arg(What, OnDirectory)) != QMessageBox::Yes)
        {
          return false;
        }
      }
      return OnLocal ? FLocal->MoveInto(Drag.Names, OnDirectory) : FPanel->MoveInto(Drag.Names, OnDirectory);
    }
    // From the other panel: its entries, from the directory they were
    // dragged from.
    QStringList Sources;
    for (const QString & Name : Drag.Names)
    {
      Sources.append(Drag.Local ? QDir(Drag.Directory).filePath(Name) : Name);
    }
    if (!Drag.Local && (Drag.Directory != FPanel->Path()))
    {
      return false;
    }
    const QString Target = OnLocal ?
      QDir::toNativeSeparators(DropTarget(FLocal->Path(), OnDirectory, false)) : DropTarget(FPanel->Path(), OnDirectory, true);
    return Transfer(Drag.Local, Sources, Target, Move, Confirm, false);
  }
  // Files from another program: uploaded when dropped on the remote panel.
  if (OnLocal || (Data == nullptr) || !Data->hasUrls())
  {
    return false;
  }
  QStringList Sources;
  for (const QUrl & Url : Data->urls())
  {
    if (Url.isLocalFile())
    {
      Sources.append(QDir::cleanPath(Url.toLocalFile()));
    }
  }
  return Transfer(true, Sources, DropTarget(FPanel->Path(), OnDirectory, true), Move, Confirm, false);
}

bool MainWindow::Transfer(bool FromLocal, const QStringList & Names, QString Target, bool Move, bool Confirm, bool Background)
{
  if (Names.isEmpty() || !FSession || !FSession->Active() || FBusy)
  {
    return false;
  }
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (Confirm)
  {
    // As WinSCP's Copy dialog: where to, editable.
    const QString What = (Names.size() == 1) ? QStringLiteral("'%1'").arg(QFileInfo(Names.first()).fileName()) : tr("%n file(s)", nullptr, Names.size());
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
    // The transfer settings, as WinSCP's Copy dialog shows them; the
    // background default is theirs.
    TransferSettingsBox * SettingsBox = new TransferSettingsBox(&Dialog);
    Layout->addRow(SettingsBox);
    QCheckBox * InBackground = new QCheckBox(tr("Transfer in &background (add to transfer queue)"), &Dialog);
    InBackground->setChecked(Background || SettingsBox->Settings().Queue);
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
    Settings = SettingsBox->Settings();
    try
    {
      SettingsBox->Commit();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
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
        FSession->QueueUpload(List, Target.toStdString(), Move, &Settings);
      }
      else
      {
        FSession->QueueDownload(List, QDir::toNativeSeparators(Target).toStdString(), Move, &Settings);
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
  return FromLocal ? Upload(Names, Target, Move, &Settings) : Download(Names, Target, Move, &Settings);
}

// As TScpCommanderForm::FullSynchronizeDirectories: from the local panel, the
// remote directory is updated; from the remote, the local one.
bool MainWindow::Synchronize(bool Confirm)
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  QString LocalDirectory = QDir::toNativeSeparators(FLocal->Path());
  QString RemoteDirectory = FPanel->Path();
  CoreBridge::TSyncParams Params = FSyncParams;
  Params.Direction = FLocalActive ? CoreBridge::TSyncParams::TDirection::Remote : CoreBridge::TSyncParams::TDirection::Local;
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (Confirm)
  {
    SynchronizeDialog Dialog(LocalDirectory, RemoteDirectory, Params, FSession->CanSyncTimestamps(), this);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    LocalDirectory = Dialog.LocalDirectory();
    RemoteDirectory = Dialog.RemoteDirectory();
    Params = Dialog.Params();
    FSyncParams = Params;
    Settings = Dialog.TransferSettings()->Settings();
    try
    {
      Dialog.TransferSettings()->Commit();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  return SynchronizeDirectories(LocalDirectory, RemoteDirectory, Params, &Settings);
}

// As TCustomScpExplorerForm::DoFullSynchronizeDirectories.
bool MainWindow::SynchronizeDirectories(const QString & LocalDirectory, const QString & RemoteDirectory,
  const CoreBridge::TSyncParams & Params, const CoreBridge::TTransferSettings * Settings)
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  std::unique_ptr<CoreBridge::TSyncChecklist> Checklist;
  {
    SynchronizeProgress Progress(this);
    Progress.show();
    try
    {
      Checklist = FSession->SyncCollect(LocalDirectory.toStdString(), RemoteDirectory.toStdString(), Params,
        [&Progress](const std::string & Local, const std::string & Remote)
        {
          return Progress.Directory(QString::fromStdString(Local), QString::fromStdString(Remote));
        }, Settings);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  FBusy = false;
  UpdateActions();
  if (!Checklist)
  {
    return false;
  }
  if (Checklist->Items().empty())
  {
    QMessageBox::information(this, tr("Synchronize"), tr("No differences found."));
    return false;
  }
  bool Queue = false;
  if (Params.PreviewChanges)
  {
    SyncChecklistDialog Dialog(*Checklist, LocalDirectory, RemoteDirectory, this);
    // As WinSCP: synchronizing timestamps is not queued.
    Dialog.QueueButton()->setVisible(Params.Mode != CoreBridge::TSyncParams::TMode::Timestamps);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Queue = (Dialog.Result() == SyncChecklistDialog::Queued);
  }
  bool Result = false;
  FBusy = true;
  UpdateActions();
  try
  {
    if (Queue)
    {
      FSession->SyncQueue(*Checklist);
      Result = true;
    }
    else
    {
      Result = FSession->SyncApply(*Checklist);
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  FLocal->Refresh();
  UpdateActions();
  if (Queue)
  {
    QueueIdle();
  }
  return Result;
}

void MainWindow::UpdateActions()
{
  FOpenAction->setEnabled(!FBusy);
  FSitesAction->setEnabled(!FBusy);
  FCopyAction->setEnabled(!FBusy && FSession);
  FMoveAction->setEnabled(!FBusy && FSession);
  FSynchronizeAction->setEnabled(!FBusy && FSession);
  FDisconnectAction->setEnabled(!FBusy && FSession);
}

void MainWindow::About()
{
  QMessageBox::about(this, tr("About %1").arg(QString::fromStdString(FInfo.AppName)),
    tr("%1 %2\nQt %3\nConfiguration: %4").arg(
      QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version),
      QString::fromLatin1(qVersion()), QString::fromStdString(FInfo.ConfigurationStorage)));
}
