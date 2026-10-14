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
#include <QTabBar>
#include <QTimer>
#include <QTreeView>
#include <QSplitter>
#include <QScreen>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "AboutDialog.h"
#include "EditorWindow.h"
#include "FileList.h"
#include "QtFrontend.h"
#include "LocalPanel.h"
#include "QueuePanel.h"
#include "RemotePanel.h"
#include "SiteManager.h"
#include "PreferencesDialog.h"
#include "PropertiesDialog.h"
#include "LinkDialog.h"
#include "RemoteTransferDialog.h"
#include "OpenDirectoryDialog.h"
#include "LocationProfilesDialog.h"
#include "FindFilesDialog.h"
#include "ServerInfoDialog.h"
#include "GenerateUrlDialog.h"
#include "ConsoleDialog.h"
#include "CustomCommandDialog.h"
#include <QDesktopServices>
#include <QProcess>
#include "QueryDialog.h"
#include "SynchronizeDialog.h"
#include "KeepUpToDateDialog.h"
#include "EditorPreferencesDialog.h"
#include "SaveWorkspaceDialog.h"
#include "TransferSettingsDialog.h"

namespace
{


}  // namespace

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));

  // The actions, placed in WinSCP's menus by BuildMenus once the panels are
  // made; each the window's too, so its key works wherever it is shown.
  QMenu * FileMenu = new QMenu(this);
  FSitesAction = FileMenu->addAction(tr("&Site Manager..."), this, &MainWindow::SiteManagerDialog);
  // Ctrl+N: WinSCP's New Tab, the Login dialog.
  FSitesAction->setShortcut(QKeySequence::New);
  FOpenAction = FileMenu->addAction(tr("&Open URL..."), this, &MainWindow::OpenUrlDialog);
  // Ctrl+O is Open Directory's, as WinSCP's.
  // As WinSCP: the last session closed, the window too unless kept open.
  FDisconnectAction = FileMenu->addAction(tr("&Disconnect Session"), this, [this]()
  {
    if ((FActiveTab >= 0) && !CloseTab(FActiveTab))
    {
      return;
    }
    if (FTabs.empty() && !CoreBridge::Preferences().KeepOpenWhenNoSession)
    {
      close();
    }
  });
  FQuitAction = FileMenu->addAction(tr("&Quit"), this, &QWidget::close);
  // F10 too, as WinSCP's Commander.
  FQuitAction->setShortcuts({ QKeySequence(QKeySequence::Quit), QKeySequence(Qt::Key_F10) });
  FAboutAction = FileMenu->addAction(tr("&About..."), this, &MainWindow::About);

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
  // Right clicks: WinSCP's popups.
  FLocal->OnContextMenu = [this](const QPoint & Global, bool OnItem) { ContextMenu(true, OnItem, Global); };
  FPanel->OnContextMenu = [this](const QPoint & Global, bool OnItem) { ContextMenu(false, OnItem, Global); };
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
  // The session tabs above the panels, as WinSCP's.
  QWidget * Central = new QWidget(this);
  QVBoxLayout * CentralLayout = new QVBoxLayout(Central);
  CentralLayout->setContentsMargins(0, 0, 0, 0);
  CentralLayout->setSpacing(0);
  FTabBar = new QTabBar(Central);
  FTabBar->setTabsClosable(true);
  FTabBar->setDocumentMode(true);
  FTabBar->setExpanding(false);
  FTabBar->setVisible(false);
  connect(FTabBar, &QTabBar::currentChanged, this, [this](int Index)
  {
    if (!FUpdatingTabs && (Index >= 0) && !SwitchTab(Index))
    {
      // Busy: back to the tab that is.
      UpdateTabs();
    }
  });
  connect(FTabBar, &QTabBar::tabCloseRequested, this, [this](int Index)
  {
    QTimer::singleShot(0, this, [this, Index]()
    {
      if (CloseTab(Index) && FTabs.empty() && !CoreBridge::Preferences().KeepOpenWhenNoSession)
      {
        close();
      }
    });
  });
  CentralLayout->addWidget(FTabBar);
  CentralLayout->addWidget(Vertical, 1);
  setCentralWidget(Central);
  FQueueTimer = new QTimer(this);
  FQueueTimer->setInterval(250);
  connect(FQueueTimer, &QTimer::timeout, this, &MainWindow::QueueIdle);
  // As the explorer's timer calling TEditorManager::Check.
  FExternalTimer = new QTimer(this);
  FExternalTimer->setInterval(1000);
  connect(FExternalTimer, &QTimer::timeout, this, &MainWindow::CheckExternalEdited);

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

  QMenu * FilesMenu = new QMenu(this);
  FEditAction = FilesMenu->addAction(tr("&Edit"), this, [this]() { EditCurrent(); });
  FEditAction->setShortcut(QKeySequence(Qt::Key_F4));
  // As WinSCP's Edit With submenu: the editors, then one asked for.
  FEditWithMenu = new QMenu(tr("Edit &With"), this);
  connect(FEditWithMenu, &QMenu::aboutToShow, this, [this]()
  {
    FEditWithMenu->clear();
    const std::vector<CoreBridge::TEditorDefinition> All = CoreBridge::Editors();
    for (int Index = 0; Index < static_cast<int>(All.size()); Index++)
    {
      FEditWithMenu->addAction(QString::fromStdString(All[Index].Name), this, [this, Index]() { EditCurrentWith(Index); });
    }
    FEditWithMenu->addSeparator();
    FEditWithMenu->addAction(tr("Edit &With..."), this, [this]() { EditCurrentWith(-1); });
  });
  FOpenCurrentAction = FilesMenu->addAction(tr("&Open"), this, [this]() { OpenCurrent(); });
  FLinkAction = FilesMenu->addAction(tr("Edit &Link..."), this, [this]() { AddEditLink(); });
  FPropertiesAction = FilesMenu->addAction(tr("&Properties"), this, [this]() { Properties(); });
  FGenerateFileUrlAction = FilesMenu->addAction(tr("Generate File &URL..."), this, [this]() { GenerateUrl(true); });
  FFileCommandsMenu = new QMenu(tr("File &Custom Commands"), this);
  FCopyAction = FilesMenu->addAction(tr("&Copy..."), this, [this]() { TransferSelected(false, true); });
  FCopyAction->setShortcut(QKeySequence(Qt::Key_F5));
  FMoveAction = FilesMenu->addAction(tr("&Move..."), this, [this]() { TransferSelected(true, true); });
  FMoveAction->setShortcut(QKeySequence(Qt::Key_F6));
  FDuplicateAction = FilesMenu->addAction(tr("Du&plicate..."), this, [this]() { RemoteTransfer(false); });
  FDuplicateAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  FMoveToAction = FilesMenu->addAction(tr("Mo&ve To..."), this, [this]() { RemoteTransfer(true); });

  QMenu * GoMenu = FilesMenu;
  FOpenDirectoryAction = GoMenu->addAction(tr("&Open Directory/Bookmark..."), this, [this]() { OpenDirectory(false); });
  FOpenDirectoryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
  FAddBookmarkAction = GoMenu->addAction(tr("&Add Path to Bookmarks"), this, [this]() { OpenDirectory(true); });
  FAddBookmarkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
  // The local panel's (the Commander's Local > Go To).
  FExploreAction = GoMenu->addAction(tr("&Explore Directory"), this, [this]() { ExploreLocalDirectory(); });
  FExploreAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_E));

  QMenu * CommandsMenu = FilesMenu;
  FFindFilesAction = CommandsMenu->addAction(tr("&Find Files..."), this, [this]() { FindFilesDialog(); });
  FStaticCommandsMenu = new QMenu(tr("Static &Custom Commands"), this);
  FConsoleAction = CommandsMenu->addAction(tr("Open &Terminal"), this, [this]() { OpenConsole(); });
  FConsoleAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::CTRL | Qt::Key_T));
  FPuttyAction = CommandsMenu->addAction(tr("Open in &PuTTY"), this, [this]() { OpenInPutty(); });
  FPuttyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
  FServerInfoAction = CommandsMenu->addAction(tr("Server/Protocol &Information"), this, [this]() { ServerInfo(); });
  FGenerateSessionUrlAction = CommandsMenu->addAction(tr("Generate Session &URL/Code..."), this, [this]() { GenerateUrl(false); });
  FSynchronizeAction = CommandsMenu->addAction(tr("&Synchronize..."), this, [this]() { Synchronize(true); });
  FSynchronizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
  FKeepUpToDateAction = CommandsMenu->addAction(tr("&Keep Remote Directory up to Date..."), this,
    [this]() { KeepUpToDate(); });
  FKeepUpToDateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));

  // As WinSCP's Tabs menu.
  QMenu * TabsMenu = FilesMenu;
  FDuplicateTabAction = TabsMenu->addAction(tr("Du&plicate Tab"), this, [this]() { DuplicateTab(); });
  FCloseTabAction = TabsMenu->addAction(tr("&Close Tab"), this, [this]()
  {
    if ((FActiveTab >= 0) && CloseTab(FActiveTab) && FTabs.empty() && !CoreBridge::Preferences().KeepOpenWhenNoSession)
    {
      close();
    }
  });
  FCloseTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  FSaveWorkspaceAction = TabsMenu->addAction(tr("Save Wor&kspace..."), this, [this]() { SaveWorkspace(); });
  FNextTabAction = TabsMenu->addAction(tr("&Next Tab"), this, [this]() { CycleTabs(true); });
  FNextTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
  FPreviousTabAction = TabsMenu->addAction(tr("&Previous Tab"), this, [this]() { CycleTabs(false); });
  FPreviousTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab));

  QMenu * ViewMenu = FilesMenu;
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

  FFilterAction = ViewMenu->addAction(tr("&Filter..."), this, [this]() { Filter(); });
  FFilterAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));

  QMenu * OptionsMenu = FilesMenu;
  FPreferencesAction = OptionsMenu->addAction(tr("&Preferences..."), this, [this]() { PreferencesDialog(); });
  FPreferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));
  BuildMenus();

  statusBar()->showMessage(tr("%1 %2").arg(QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version)));
  // From now on a language chosen applies on the next start, as WinSCP's.
  CoreBridge::GuiStarted();
  ApplyPreferences();
  RestoreLayout();
  connect(FPanel, &RemotePanel::Changed, this, &MainWindow::UpdateTitle);
  UpdateCustomCommands();
  UpdateBookmarkShortCuts();
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
  FPropertiesAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::ALT | Qt::Key_Return) : QKeySequence(Qt::Key_F9));
  FLinkAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L) : QKeySequence(Qt::ALT | Qt::Key_F6));
  FMoveToAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M) : QKeySequence(Qt::SHIFT | Qt::Key_F6));
  FFindFilesAction->setShortcut(Preferences.ExplorerKeyboardShortcuts ? QKeySequence(Qt::Key_F3) : QKeySequence(Qt::ALT | Qt::Key_F7));
  // As TScpCommanderForm::ConfigurationChanged: the site's local directory is
  // Home, with "default directory is home", unless it remembers the last.
  FLocal->HomeDirectory =
    (Preferences.DefaultDirIsHome && FSession && !FSession->UpdateDirectories() && !FSession->LocalDirectory().empty()) ?
    QString::fromStdString(FSession->LocalDirectory()) : QString();
  UpdateTitle();
}

MainWindow::~MainWindow()
{
  // Editors are windows of their own (not children): they go with it.
  for (EditorWindow * Editor : Editors())
  {
    delete Editor;
  }
  FSession.reset();
  CleanUpEdited();
}

void MainWindow::closeEvent(QCloseEvent * Event)
{
  // As WinSCP: the editors close first, each asking to save.
  if (!CloseEditors(false))
  {
    Event->ignore();
    return;
  }
  SaveLayout();
  // As FormClose: the workspace saved automatically, when that is on (an
  // empty one is not), and noted as the last stored, so it opens next time.
  CoreBridge::TWorkspaceSettings Workspace = CoreBridge::WorkspaceSettings();
  if (Workspace.AutoSave && (TabCount() > 0))
  {
    try
    {
      const QString Name = WorkspaceName();
      CoreBridge::TSession::SaveWorkspace(Name.toStdString(), WorkspaceTabs(), Workspace.AutoSavePasswords);
      Workspace.LastStoredSession = Name.toStdString();
      CoreBridge::SetWorkspaceSettings(Workspace);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  // Every tab's session closed, each keeping its directories.
  while (!FTabs.empty())
  {
    if (FActiveTab < 0)
    {
      UnparkTab(0);
    }
    Disconnect();
  }
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

// As TScpCommanderForm::ExploreLocalDirectory: the system's file manager
// on the local panel's directory.
bool MainWindow::ExploreLocalDirectory()
{
  const QUrl Url = QUrl::fromLocalFile(FLocal->Path());
  return FUrlOpener ? FUrlOpener(Url) : QDesktopServices::openUrl(Url);
}

void MainWindow::StartUp()
{
  bool CanOpen = false;
  std::unique_ptr<CoreBridge::TSession> Session;
  try
  {
    Session = CoreBridge::TSession::FromCommandLine(CanOpen);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  // A folder of sites or a workspace named: each of its sessions in a tab.
  const QString Named = QString::fromStdString(CoreBridge::TSession::CommandLineFolderOrWorkspace());
  if (!Session && !Named.isEmpty() && OpenWorkspace(Named))
  {
    return;
  }
  // As the explorer's start: the workspace saved automatically when the
  // window last closed, when no session is named.
  const CoreBridge::TWorkspaceSettings Workspace = CoreBridge::WorkspaceSettings();
  if (!Session && Named.isEmpty() && Workspace.AutoSave && !Workspace.AutoWorkspace.empty() &&
      (QString::fromStdString(Workspace.LastStoredSession).compare(QString::fromStdString(Workspace.AutoWorkspace),
        Qt::CaseInsensitive) == 0) &&
      OpenWorkspace(QString::fromStdString(Workspace.AutoWorkspace)))
  {
    return;
  }
  if (!Session || !CanOpen || !Open(std::move(Session)))
  {
    SiteManagerDialog();
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
  if (!Dialog.Workspace().isEmpty())
  {
    return OpenWorkspace(Dialog.Workspace());
  }
  return OpenSite(Dialog.Site());
}

bool MainWindow::Open(std::unique_ptr<CoreBridge::TSession> Session)
{
  if (FBusy)
  {
    return false;
  }
  // As WinSCP: a new tab; the one active until now kept.
  const int Previous = FActiveTab;
  ParkActiveTab();
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
  if (Result)
  {
    FTabs.push_back(TTab());
    FActiveTab = static_cast<int>(FTabs.size()) - 1;
  }
  else if (Previous >= 0)
  {
    // Not connected: the tab active before is again.
    UnparkTab(Previous);
    UpdateTabs();
    UpdateTitle();
    UpdateActions();
    return false;
  }
  UpdateTabs();
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
  // As TTerminalManager::ConnectActiveTerminal: "automatically open new
  // sessions in PuTTY" (CanOpenInPutty: a command to run).
  if (Result)
  {
    const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    if (Preferences.AutoOpenInPutty && !QString::fromStdString(Preferences.PuttyPath).trimmed().isEmpty())
    {
      OpenInPutty();
    }
  }
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
  const int Closed = FActiveTab;
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
    CleanUpEdited();
    FLocal->HomeDirectory.clear();
    statusBar()->showMessage(tr("Disconnected"));
  }
  // Its tab gone, the next one (else the previous) active, as WinSCP's.
  if ((Closed >= 0) && (Closed < static_cast<int>(FTabs.size())))
  {
    FTabs.erase(FTabs.begin() + Closed);
    FActiveTab = -1;
    if (!FTabs.empty())
    {
      UnparkTab(std::min(Closed, static_cast<int>(FTabs.size()) - 1));
    }
  }
  UpdateTabs();
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
  if (FBusy)
  {
    return;
  }
  // The other tabs' sessions and queues keep going, as WinSCP's
  // TTerminalManager::Idle has them.
  for (TTab & Tab : FTabs)
  {
    if (Tab.Session && Tab.Session->Active())
    {
      try
      {
        Tab.Session->Idle();
        Tab.Session->QueueIdle();
      }
      catch (const CoreBridge::TCoreError &)
      {
        // Seen once the tab is active again.
      }
    }
  }
  if (!FSession || !FSession->Active())
  {
    return;
  }
  bool Changed = false;
  try
  {
    FSession->Idle();
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
  CleanUpEdited();
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

// As WinSCP's Copy dialog: where to, editable, the transfer settings, and
// (Background given) whether through the queue. False when cancelled.
bool MainWindow::CopyDialog(bool FromLocal, const QStringList & Names, QString & Target, bool Move, bool * Background,
  CoreBridge::TTransferSettings & Settings)
{
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
  QCheckBox * InBackground = nullptr;
  if (Background != nullptr)
  {
    InBackground = new QCheckBox(tr("Transfer in &background (add to transfer queue)"), &Dialog);
    InBackground->setChecked(*Background || SettingsBox->Settings().Queue);
    Layout->addRow(InBackground);
  }
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addRow(Box);
  if ((Dialog.exec() != QDialog::Accepted) || TargetEdit->text().trimmed().isEmpty())
  {
    return false;
  }
  Target = TargetEdit->text().trimmed();
  if (Background != nullptr)
  {
    *Background = InBackground->isChecked();
  }
  Settings = SettingsBox->Settings();
  try
  {
    SettingsBox->Commit();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  return true;
}

bool MainWindow::Transfer(bool FromLocal, const QStringList & Names, QString Target, bool Move, bool Confirm, bool Background)
{
  if (Names.isEmpty() || !FSession || !FSession->Active() || FBusy)
  {
    return false;
  }
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (Confirm && !CopyDialog(FromLocal, Names, Target, Move, &Background, Settings))
  {
    return false;
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

// A custom command's prompt patterns asked in WinSCP's dialog; its !`command`
// patterns run here, their output taken.
CoreBridge::TCustomCommandPrompts MainWindow::CommandPrompts()
{
  CoreBridge::TCustomCommandPrompts Prompts;
  Prompts.Prompt = [this](const std::string & Name, const std::vector<std::pair<std::string, std::string>> & Asked,
    std::vector<std::string> & Values)
  {
    return CustomCommandPrompts(this, Name, Asked, Values);
  };
  Prompts.Execute = [](const std::string & Line)
  {
    QProcess Process;
#if defined(_WIN32)
    Process.start(QStringLiteral("cmd.exe"), { QStringLiteral("/c"), QString::fromStdString(Line) });
#else
    Process.start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), QString::fromStdString(Line) });
#endif
    Process.waitForFinished(-1);
    return QString::fromLocal8Bit(Process.readAllStandardOutput()).toStdString();
  };
  return Prompts;
}

// Commands > Open in PuTTY, as TCustomScpExplorerForm's PuttyAction.
bool MainWindow::OpenInPutty()
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  try
  {
    FSession->OpenInPutty(CommandPrompts());
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

// As TCustomScpExplorerForm::DoSynchronizeDirectories, from the Commander's
// SynchronizeDirectories: the panels' directories, the settings kept, and
// what is selected in either panel for "selected files only".
bool MainWindow::KeepUpToDate()
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  CoreBridge::TKeepUpToDateParams Params = CoreBridge::KeepUpToDateDefaults();
  Params.LocalDirectory = QDir::toNativeSeparators(FLocal->Path()).toStdString();
  Params.RemoteDirectory = FPanel->Path().toStdString();
  std::vector<std::string> Selected;
  for (const QStringList & Names : { FPanel->Selected(), FLocal->Selected() })
  {
    for (const QString & Name : Names)
    {
      Selected.push_back(Name.toStdString());
    }
  }
  FBusy = true;
  UpdateActions();
  bool Result;
  {
    KeepUpToDateDialog Dialog(*FSession, Params, Selected, this);
    FKeepUpToDate = &Dialog;
    Dialog.exec();
    FKeepUpToDate = nullptr;
    Result = true;
    try
    {
      CoreBridge::SaveKeepUpToDateDefaults(Dialog.Params(), Dialog.SaveSettings());
      Dialog.TransferSettings()->Commit();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  FBusy = false;
  UpdateActions();
  // What was uploaded or deleted shows.
  if (FSession && FSession->Active())
  {
    FPanel->Refresh();
  }
  return Result;
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
  // As WinSCP's: Local and Remote with a session, else Left and Right.
  if (FLocalMenu != nullptr)
  {
    FLocalMenu->setTitle(FSession ? tr("&Local") : tr("&Left"));
    FRemoteMenu->setTitle(FSession ? tr("&Remote") : tr("&Right"));
  }
  FOpenAction->setEnabled(!FBusy);
  FSitesAction->setEnabled(!FBusy);
  FEditAction->setEnabled(!FBusy);
  FPropertiesAction->setEnabled(!FBusy && FSession);
  FLinkAction->setEnabled(!FBusy);
  FDuplicateAction->setEnabled(!FBusy && FSession);
  FOpenDirectoryAction->setEnabled(!FBusy);
  FFindFilesAction->setEnabled(!FBusy && FSession);
  FServerInfoAction->setEnabled(!FBusy && FSession);
  FConsoleAction->setEnabled(!FBusy && FSession);
  FPuttyAction->setEnabled(!FBusy && FSession);
  FGenerateSessionUrlAction->setEnabled(!FBusy && FSession);
  FGenerateFileUrlAction->setEnabled(!FBusy && FSession);
  FAddBookmarkAction->setEnabled(!FBusy);
  FMoveToAction->setEnabled(!FBusy && FSession);
  FCopyAction->setEnabled(!FBusy && FSession);
  FMoveAction->setEnabled(!FBusy && FSession);
  FSynchronizeAction->setEnabled(!FBusy && FSession);
  FKeepUpToDateAction->setEnabled(!FBusy && FSession);
  FDisconnectAction->setEnabled(!FBusy && FSession);
  FDuplicateTabAction->setEnabled(!FBusy && FSession);
  FSaveWorkspaceAction->setEnabled(!FBusy && (TabCount() > 0));
  FCloseTabAction->setEnabled(!FBusy && FSession);
  FNextTabAction->setEnabled(!FBusy && (TabCount() > 1));
  FPreviousTabAction->setEnabled(!FBusy && (TabCount() > 1));
}

void MainWindow::About()
{
  AboutDialog Dialog(FInfo, this);
  Dialog.exec();
}

// The active panel's focused file: files only, as WinSCP's.
bool MainWindow::CurrentFile(bool & Local, QString & Name, std::int64_t & Size, double & ModifiedTime) const
{
  const FileView * View = FLocalActive ? FLocal->View() : FPanel->View();
  const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
  if ((Entry == nullptr) || Entry->Directory || (!FLocalActive && !FSession))
  {
    return false;
  }
  Local = FLocalActive;
  Name = QString::fromStdString(Entry->Name);
  Size = Entry->Size;
  ModifiedTime = Entry->ModifiedTime;
  return true;
}

EditorWindow * MainWindow::EditCurrent()
{
  bool Local = false;
  QString Name;
  std::int64_t Size = 0;
  double ModifiedTime = 0;
  if (FBusy || !CurrentFile(Local, Name, Size, ModifiedTime))
  {
    return nullptr;
  }
  CoreBridge::TEditorDefinition Editor;
  try
  {
    const std::vector<CoreBridge::TEditorDefinition> All = CoreBridge::Editors();
    const int Index = CoreBridge::FindEditor(All, Name.toStdString(), Local, Size, ModifiedTime);
    if (Index >= 0)
    {
      Editor = All[Index];
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return nullptr;
  }
  if (Editor.Kind != CoreBridge::TEditorDefinition::TKind::Internal)
  {
    EditExternally(Editor, Local, Name);
    return nullptr;
  }
  return Local ? EditLocal(QDir(FLocal->Path()).filePath(Name)) : EditRemote(Name);
}

bool MainWindow::EditCurrentWith(int Index)
{
  bool Local = false;
  QString Name;
  std::int64_t Size = 0;
  double ModifiedTime = 0;
  if (FBusy || !CurrentFile(Local, Name, Size, ModifiedTime))
  {
    return false;
  }
  CoreBridge::TEditorDefinition Editor;
  if (Index >= 0)
  {
    const std::vector<CoreBridge::TEditorDefinition> All = CoreBridge::Editors();
    if (Index >= static_cast<int>(All.size()))
    {
      return false;
    }
    Editor = All[Index];
  }
  else
  {
    // As ExecuteCurrentFileWith: an external editor, remembered if asked
    // (unless one with that command is).
    Editor.Kind = CoreBridge::TEditorDefinition::TKind::External;
    EditorPreferencesDialog Dialog(EditorPreferencesDialog::TMode::AdHoc, Editor, !Local, this);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Editor = Dialog.Editor();
    if (Dialog.Remember())
    {
      std::vector<CoreBridge::TEditorDefinition> All = CoreBridge::Editors();
      const bool Found = std::any_of(All.begin(), All.end(), [&Editor](const CoreBridge::TEditorDefinition & Other)
      {
        return (Other.Kind == CoreBridge::TEditorDefinition::TKind::External) && (Other.Command == Editor.Command);
      });
      if (!Found)
      {
        All.push_back(Editor);
        try
        {
          CoreBridge::SetEditors(All);
        }
        catch (const CoreBridge::TCoreError & E)
        {
          ShowError(E);
        }
      }
    }
  }
  if (Editor.Kind == CoreBridge::TEditorDefinition::TKind::Internal)
  {
    return (Local ? EditLocal(QDir(FLocal->Path()).filePath(Name)) : EditRemote(Name)) != nullptr;
  }
  return EditExternally(Editor, Local, Name);
}

bool MainWindow::OpenCurrent()
{
  bool Local = false;
  QString Name;
  std::int64_t Size = 0;
  double ModifiedTime = 0;
  if (FBusy || !CurrentFile(Local, Name, Size, ModifiedTime))
  {
    return false;
  }
  CoreBridge::TEditorDefinition Editor;
  Editor.Kind = CoreBridge::TEditorDefinition::TKind::Open;
  return EditExternally(Editor, Local, Name);
}

QMenu * MainWindow::ContextMenu(bool Local, bool OnItem, const QPoint & Global, bool Show)
{
  // The panel clicked is the one acted on.
  ActivatePanel(Local);
  UpdateActions();
  QMenu * Menu = new QMenu(this);
  Menu->setAttribute(Qt::WA_DeleteOnClose, Show);
  if (OnItem)
  {
    const FileView * View = Local ? FLocal->View() : FPanel->View();
    const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
    // CurrentAddEditLinkContextAction: a link's own.
    if (!Local && (Entry != nullptr) && Entry->Link)
    {
      Menu->addAction(FLinkAction);
    }
    Menu->addAction(tr("&Open"), this, [this]() { OpenCurrent(); });
    Menu->addAction(FEditAction);
    Menu->addMenu(FEditWithMenu);
    Menu->addSeparator();
    Menu->addAction(FCopyAction);
    Menu->addAction(FMoveAction);
    if (!Local)
    {
      Menu->addAction(FDuplicateAction);
      Menu->addAction(FMoveToAction);
    }
    Menu->addAction(Local ? FLocal->DeleteAction() : FPanel->DeleteAction());
    Menu->addAction(Local ? FLocal->RenameAction() : FPanel->RenameAction());
    Menu->addSeparator();
    if (!Local)
    {
      Menu->addMenu(FFileCommandsMenu);
    }
    QMenu * Names = Menu->addMenu(tr("&File Names"));
    Names->addAction(tr("&Copy to Clipboard"), this, [this, Local]() { CopyFileNames(Local, false); });
    Names->addAction(tr("Copy to Clipboard (Include &Paths)"), this, [this, Local]() { CopyFileNames(Local, true); });
    if (!Local)
    {
      Names->addAction(FGenerateFileUrlAction);
    }
    if (!Local)
    {
      Menu->addSeparator();
      Menu->addAction(FPropertiesAction);
    }
  }
  else
  {
    QMenu * GoTo = Menu->addMenu(tr("&Go To"));
    GoTo->addAction(FOpenDirectoryAction);
    if (Local)
    {
      GoTo->addAction(tr("&Explore Directory"), this, [this]() { ExploreLocalDirectory(); });
    }
    GoTo->addSeparator();
    for (QAction * Action : Local ?
           std::initializer_list<QAction *>{ FLocal->ParentAction(), FLocal->RootAction(), FLocal->HomeAction() } :
           std::initializer_list<QAction *>{ FPanel->ParentAction(), FPanel->RootAction(), FPanel->HomeAction() })
    {
      GoTo->addAction(Action);
    }
    GoTo->addSeparator();
    GoTo->addAction(Local ? FLocal->BackAction() : FPanel->BackAction());
    GoTo->addAction(Local ? FLocal->ForwardAction() : FPanel->ForwardAction());
    Menu->addAction(Local ? FLocal->RefreshAction() : FPanel->RefreshAction());
    Menu->addAction(FAddBookmarkAction);
    Menu->addAction(FFilterAction);
    Menu->addAction(tr("&Copy Path to Clipboard"), this, [this, Local]()
    {
      QApplication::clipboard()->setText(Local ? QDir::toNativeSeparators(FLocal->Path()) : FPanel->Path());
    });
    Menu->addSeparator();
    QMenu * New = Menu->addMenu(tr("&New"));
    New->addAction(Local ? FLocal->NewFolderAction() : FPanel->NewFolderAction());
    if (!Local)
    {
      Menu->addMenu(FStaticCommandsMenu);
    }
  }
  if (Show)
  {
    Menu->popup(Global);
  }
  return Menu;
}

void MainWindow::CopyFileNames(bool Local, bool Paths)
{
  const QStringList Names = Local ? FLocal->Selected() : FPanel->Selected();
  QStringList Lines;
  for (const QString & Name : Names)
  {
    if (!Paths)
    {
      Lines.append(Name);
    }
    else if (Local)
    {
      Lines.append(QDir::toNativeSeparators(QDir(FLocal->Path()).filePath(Name)));
    }
    else
    {
      const QString Directory = FPanel->Path();
      Lines.append(Directory + (Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name);
    }
  }
  QApplication::clipboard()->setText(Lines.join(QStringLiteral("\n")));
}

bool MainWindow::EditExternally(const CoreBridge::TEditorDefinition & Editor, bool Local, const QString & Name)
{
  const bool External = (Editor.Kind == CoreBridge::TEditorDefinition::TKind::External);
  std::shared_ptr<CoreBridge::TEditedFile> File;
  bool Fresh = false;
  QString LocalFile;
  if (Local)
  {
    LocalFile = QDir(FLocal->Path()).filePath(Name);
  }
  else
  {
    if (FBusy || !FSession)
    {
      return false;
    }
    const QString Directory = FPanel->Path();
    // Open already: its copy, as it is.
    for (const TExternalEdited & Edited : FExternalEdited)
    {
      if ((QString::fromStdString(Edited.File->RemoteDirectory) == Directory) &&
          (QString::fromStdString(Edited.File->OriginalName) == Name))
      {
        File = Edited.File;
      }
    }
    if (!File)
    {
      FBusy = true;
      UpdateActions();
      try
      {
        // As RemoteExecuteForceText: text for an external editor that asks for it.
        File = std::make_shared<CoreBridge::TEditedFile>(FSession->EditDownload(Name.toStdString(), External && Editor.Text));
        Fresh = true;
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
      }
      FBusy = false;
      UpdateActions();
      if (!File)
      {
        return false;
      }
    }
    LocalFile = QString::fromStdString(File->LocalFile);
  }
  bool Started = false;
  if (External)
  {
    try
    {
      CoreBridge::StartExternalEditor(Editor.Command, QDir::toNativeSeparators(LocalFile).toStdString());
      Started = true;
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  else
  {
    const QUrl Url = QUrl::fromLocalFile(LocalFile);
    Started = FUrlOpener ? FUrlOpener(Url) : QDesktopServices::openUrl(Url);
    if (!Started)
    {
      CoreBridge::TError Error;
      Error.Message = CoreBridge::ExecuteFileError(QDir::toNativeSeparators(LocalFile).toStdString());
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(Error);
    }
  }
  if (Fresh)
  {
    if (Started)
    {
      FExternalEdited.push_back({ File, QFileInfo(LocalFile).lastModified(), FSession.get() });
      FExternalTimer->start();
    }
    else if (!File->RootDirectory.empty())
    {
      QDir(QString::fromStdString(File->RootDirectory)).removeRecursively();
    }
  }
  return Started;
}

// As TEditorManager::Check for files with no process: a changed time is a
// save, uploaded.
void MainWindow::CheckExternalEdited()
{
  for (std::size_t Index = 0; Index < FExternalEdited.size(); Index++)
  {
    if (FBusy)
    {
      return;
    }
    TExternalEdited & Edited = FExternalEdited[Index];
    const QDateTime Modified = QFileInfo(QString::fromStdString(Edited.File->LocalFile)).lastModified();
    if (Modified.isValid() && (Modified != Edited.Modified))
    {
      Edited.Modified = Modified;
      std::shared_ptr<CoreBridge::TEditedFile> File = Edited.File;
      EditUpload(Edited.Session, *File);
    }
  }
}

EditorWindow * MainWindow::ShowEditor(EditorWindow * Editor)
{
  Editor->setAttribute(Qt::WA_DeleteOnClose);
  if (!Editor->Load())
  {
    delete Editor;
    return nullptr;
  }
  Editor->show();
  Editor->raise();
  Editor->activateWindow();
  return Editor;
}

EditorWindow * MainWindow::EditLocal(const QString & FileName)
{
  const QString Path = QFileInfo(FileName).absoluteFilePath();
  for (const TEdited & Edited : FEdited)
  {
    if (Edited.Editor && !Edited.File && (QFileInfo(Edited.Editor->FileName()).absoluteFilePath() == Path))
    {
      Edited.Editor->raise();
      Edited.Editor->activateWindow();
      return Edited.Editor;
    }
  }
  // Edited in place, as WinSCP's local files.
  EditorWindow * Editor = new EditorWindow(Path, QDir::toNativeSeparators(Path), QString::fromStdString(FInfo.AppName));
  FEdited.push_back({ Editor, nullptr });
  return ShowEditor(Editor);
}

EditorWindow * MainWindow::EditRemote(const QString & Name)
{
  if (FBusy || !FSession)
  {
    return nullptr;
  }
  const QString Directory = FPanel->Path();
  const QString RemotePath = Directory + (Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name;
  for (const TEdited & Edited : FEdited)
  {
    if (Edited.Editor && Edited.File &&
        (QString::fromStdString(Edited.File->RemoteDirectory) == Directory) && (QString::fromStdString(Edited.File->OriginalName) == Name))
    {
      Edited.Editor->raise();
      Edited.Editor->activateWindow();
      return Edited.Editor;
    }
  }
  FBusy = true;
  UpdateActions();
  std::shared_ptr<CoreBridge::TEditedFile> File;
  try
  {
    File = std::make_shared<CoreBridge::TEditedFile>(FSession->EditDownload(Name.toStdString()));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  if (!File)
  {
    return nullptr;
  }
  EditorWindow * Editor = new EditorWindow(QString::fromStdString(File->LocalFile), RemotePath, QString::fromStdString(FInfo.AppName));
  CoreBridge::TSession * Session = FSession.get();
  Editor->OnFileChanged = [this, File, Session]() { return EditUpload(Session, *File); };
  // Its temporary directory goes once it is closed and uploaded.
  connect(Editor, &QObject::destroyed, this, [this, File]()
  {
    if (!File->RootDirectory.empty())
    {
      FEditedCleanUp.push_back(File->RootDirectory);
    }
    CleanUpEdited();
  });
  FEdited.push_back({ Editor, File, Session });
  return ShowEditor(Editor);
}

// The file goes back to the session it came from, whichever tab is active.
bool MainWindow::EditUpload(CoreBridge::TSession * Session, CoreBridge::TEditedFile & File)
{
  if (FBusy || !SessionOpen(Session) || !Session->Active())
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    Result = Session->EditUpload(File, !FClosingEditors);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  // Uploaded here (not queued): the panel shows it now.
  if (Result && (Session == FSession.get()) && Session->QueueEmpty())
  {
    FPanel->Refresh();
  }
  return Result;
}

std::vector<EditorWindow *> MainWindow::Editors(bool RemoteOnly) const
{
  std::vector<EditorWindow *> Result;
  for (const TEdited & Edited : FEdited)
  {
    if (Edited.Editor && (!RemoteOnly || Edited.File))
    {
      Result.push_back(Edited.Editor);
    }
  }
  return Result;
}

bool MainWindow::CloseEditors(bool RemoteOnly, const CoreBridge::TSession * Session)
{
  FClosingEditors = true;
  // As CloseExternalFilesWithoutProcess: their last changes uploaded, then
  // their directories cleaned up with the rest.
  CheckExternalEdited();
  for (const TExternalEdited & Edited : FExternalEdited)
  {
    if (((Session == nullptr) || (Edited.Session == Session)) && !Edited.File->RootDirectory.empty())
    {
      FEditedCleanUp.push_back(Edited.File->RootDirectory);
    }
  }
  FExternalEdited.erase(std::remove_if(FExternalEdited.begin(), FExternalEdited.end(),
    [Session](const TExternalEdited & Edited) { return (Session == nullptr) || (Edited.Session == Session); }),
    FExternalEdited.end());
  if (FExternalEdited.empty())
  {
    FExternalTimer->stop();
  }
  std::vector<EditorWindow *> Closing;
  for (const TEdited & Edited : FEdited)
  {
    if (Edited.Editor && (!RemoteOnly || Edited.File) && ((Session == nullptr) || (Edited.Session == Session)))
    {
      Closing.push_back(Edited.Editor);
    }
  }
  bool Result = true;
  for (EditorWindow * Editor : Closing)
  {
    if (!Editor->close())
    {
      Result = false;
      break;
    }
    // Gone now, not later: its directory with it.
    delete Editor;
  }
  FClosingEditors = false;
  FEdited.erase(std::remove_if(FEdited.begin(), FEdited.end(), [](const TEdited & Edited) { return Edited.Editor.isNull(); }),
    FEdited.end());
  return Result;
}

void MainWindow::CleanUpEdited()
{
  FEdited.erase(std::remove_if(FEdited.begin(), FEdited.end(), [](const TEdited & Edited) { return Edited.Editor.isNull(); }),
    FEdited.end());
  if (FEditedCleanUp.empty())
  {
    return;
  }
  // Their uploads may still be queued, in any tab.
  for (int Index = 0; Index < TabCount(); Index++)
  {
    if ((TabSession(Index) != nullptr) && !TabSession(Index)->QueueEmpty())
    {
      return;
    }
  }
  for (const std::string & Directory : FEditedCleanUp)
  {
    QDir(QString::fromStdString(Directory)).removeRecursively();
  }
  FEditedCleanUp.clear();
}

bool MainWindow::Properties()
{
  if (FBusy || !FSession || FLocalActive)
  {
    return false;
  }
  QStringList Names = FPanel->Selected();
  if (Names.isEmpty())
  {
    const CoreBridge::TRemoteEntry * Entry = FPanel->View()->CurrentEntry();
    if (Entry != nullptr)
    {
      Names.append(QString::fromStdString(Entry->Name));
    }
  }
  if (Names.isEmpty())
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
    const CoreBridge::TPropertiesInfo Info = FSession->PropertiesOf(List);
    ::PropertiesDialog Dialog(FSession.get(), List, FPanel->Path(), Info, this);
    if (Dialog.exec() == QDialog::Accepted)
    {
      Result = FSession->SetProperties(List, Dialog.Properties());
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  UpdateActions();
  return Result;
}

bool MainWindow::AddEditLink()
{
  if (FBusy || (!FLocalActive && (!FSession || !FSession->CanLink())))
  {
    return false;
  }
  const FileView * View = FLocalActive ? FLocal->View() : FPanel->View();
  const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
  bool Edit = false;
  QString FileName;
  QString PointTo;
  if (Entry != nullptr)
  {
    const QString Name = QString::fromStdString(Entry->Name);
    Edit = Entry->Link && (FLocalActive || FSession->CanEditLinks());
    if (Edit)
    {
      FileName = Name;
      PointTo = FLocalActive ? QFileInfo(QDir(FLocal->Path()).filePath(Name)).symLinkTarget() : QString::fromStdString(Entry->LinkTarget);
    }
    else
    {
      PointTo = Name;
    }
  }
  LinkDialog Dialog(FileName, PointTo, !FLocalActive, Edit, !FLocalActive && FSession->CanHardLink(), this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  bool Result = false;
  if (FLocalActive)
  {
    const QString Path = QDir(FLocal->Path()).filePath(Dialog.FileName());
    if (Edit)
    {
      QFile::remove(Path);
    }
    Result = QFile::link(Dialog.PointTo(), Path);
    if (!Result)
    {
      CoreBridge::TError Error;
      Error.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateLinkError))
        .replace(QStringLiteral("%s"), QDir::toNativeSeparators(Path)).toStdString();
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(Error);
    }
    FLocal->Refresh();
    return Result;
  }
  FBusy = true;
  UpdateActions();
  try
  {
    FSession->CreateLink(Dialog.FileName().toStdString(), Dialog.PointTo().toStdString(), Dialog.SymbolicLink(), Edit);
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  UpdateActions();
  return Result;
}

bool MainWindow::RemoteTransfer(bool Move)
{
  // As WinSCP's Commander: the remote panel's (local files are copied
  // within the disk only in its local-to-local browsing, which the shell
  // does not have).
  if (FBusy || FLocalActive || !FSession)
  {
    return false;
  }
  QStringList Names = FPanel->Selected();
  if (Names.isEmpty())
  {
    const CoreBridge::TRemoteEntry * Entry = FPanel->View()->CurrentEntry();
    if (Entry != nullptr)
    {
      Names.append(QString::fromStdString(Entry->Name));
    }
  }
  if (Names.isEmpty())
  {
    return false;
  }
  // As TCustomScpExplorerForm::RemoteTransferDialog: here, under the one
  // file's name or all.
  const QString FileMask = (Names.size() == 1) ? Names.first() : QStringLiteral("*.*");
  RemoteTransferDialog Dialog(FSession.get(), Move, Names.size() > 1, FPanel->Path(), FileMask, this);
  if (Dialog.exec() != QDialog::Accepted)
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
    Result = Move ?
      FSession->RemoteMove(List, Dialog.Target().toStdString(), Dialog.FileMask().toStdString()) :
      FSession->RemoteCopy(List, Dialog.Target().toStdString(), Dialog.FileMask().toStdString(), Dialog.DirectCopy());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  UpdateActions();
  return Result;
}

// A panel to a directory, already there counting (the panels' own
// ChangeDirectory is false then).
bool MainWindow::GoLocal(const QString & Directory)
{
  return FLocal->ChangeDirectory(Directory) ||
    (QDir::cleanPath(QDir::fromNativeSeparators(Directory)) == QDir::cleanPath(FLocal->Path()));
}

bool MainWindow::GoRemote(const QString & Directory)
{
  const auto Clean = [](QString Path)
  {
    while ((Path.size() > 1) && Path.endsWith(QLatin1Char('/')))
    {
      Path.chop(1);
    }
    return Path;
  };
  return FPanel->ChangeDirectory(Directory) || (Clean(Directory) == Clean(FPanel->Path()));
}

// The shared bookmarks' shortcuts (TCustomScpExplorerForm::KeyDown's
// SharedBookmarks->FindByShortCut), as actions of the window.
void MainWindow::UpdateBookmarkShortCuts()
{
  for (QAction * Action : FBookmarkActions)
  {
    delete Action;
  }
  FBookmarkActions.clear();
  std::vector<CoreBridge::TBookmarkEntry> Shared;
  try
  {
    Shared = CoreBridge::Bookmarks(std::string());
  }
  catch (const CoreBridge::TCoreError &)
  {
    return;
  }
  for (const CoreBridge::TBookmarkEntry & Bookmark : Shared)
  {
    const QString ShortCut = ShortCutText(Bookmark.ShortCut);
    if (!ShortCut.isEmpty())
    {
      QAction * Action = new QAction(this);
      Action->setShortcut(QKeySequence::fromString(ShortCut, QKeySequence::NativeText));
      Action->setShortcutContext(Qt::WindowShortcut);
      const unsigned Key = Bookmark.ShortCut;
      connect(Action, &QAction::triggered, this, [this, Key]() { OpenBookmarkShortCut(Key); });
      addAction(Action);
      FBookmarkActions.push_back(Action);
    }
  }
}

// As OpenBookmark: the bookmark's directory on the active panel's side,
// where it has one.
bool MainWindow::OpenBookmarkShortCut(unsigned ShortCut)
{
  if (FBusy)
  {
    return false;
  }
  for (const CoreBridge::TBookmarkEntry & Bookmark : CoreBridge::Bookmarks(std::string()))
  {
    if (Bookmark.ShortCut == ShortCut)
    {
      const bool Local = FLocalActive || !FSession;
      const QString Path = QString::fromStdString(Local ? Bookmark.Local : Bookmark.Remote);
      return !Path.isEmpty() && (Local ? GoLocal(Path) : GoRemote(Path));
    }
  }
  return false;
}

// As TScpCommanderForm::DoOpenDirectoryDialog: bookmarks or Location
// Profiles, as the preference says, the other when the dialog switches.
bool MainWindow::OpenDirectory(bool AddBookmark)
{
  // The bookmarks' shortcuts, as the dialogs leave them.
  struct TUpdate
  {
    MainWindow * Window;
    ~TUpdate() { Window->UpdateBookmarkShortCuts(); }
  } Update{ this };
  if (FBusy)
  {
    return false;
  }
  while (true)
  {
    // Profiles want a session, as upstream's.
    const bool Profiles = CoreBridge::Preferences().UseLocationProfiles && FSession;
    bool Switched = false;
    const bool Result = Profiles ? OpenLocationProfile(AddBookmark, Switched) : OpenBookmark(AddBookmark, Switched);
    if (!Switched)
    {
      return Result;
    }
    AddBookmark = false;
  }
}

bool MainWindow::OpenBookmark(bool AddBookmark, bool & Switched)
{
  const bool Local = FLocalActive || !FSession;
  bool Shared = CoreBridge::UseSharedBookmarks();
  const QString Directory = Local ? QDir::toNativeSeparators(FLocal->Path()) : FPanel->Path();
  if (AddBookmark)
  {
    // As upstream: asked first, the checkbox choosing the shared bookmarks.
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddBookmarkConfirm))
      .replace(QStringLiteral("%s"), Directory).toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    Query.NeverAskAgainCheck = true;
    Query.NeverAskAgainTitle = CoreBridge::Text(CoreBridge::TText::AddBookmarkShared);
    Query.NeverAskAgainCheckedInitially = Shared;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
    if (Answer.Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
    Shared = Answer.NeverAskAgain;
    try
    {
      CoreBridge::SetUseSharedBookmarks(Shared);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  QString Name = Directory;
  while (true)
  {
    // Site bookmarks are the site's: with a session, on either side.
    OpenDirectoryDialog Dialog(!Local, Name, Local ? FLocal->History() : FPanel->History(),
      FSession ? FSession->SessionKey() : std::string(), AddBookmark, Shared, this, FSession != nullptr);
    if (Dialog.exec() != QDialog::Accepted)
    {
      Switched = Dialog.Switched();
      return false;
    }
    Name = Dialog.Directory();
    if (Local ? GoLocal(Name) : GoRemote(Name))
    {
      return true;
    }
    AddBookmark = false;
    Shared = CoreBridge::UseSharedBookmarks();
  }
}

// The Location Profiles dialog, then DoOpenBookmark: the local directory,
// then the remote one, each reporting its own error.
bool MainWindow::OpenLocationProfile(bool AddBookmark, bool & Switched)
{
  LocationProfilesDialog Dialog(QDir::toNativeSeparators(FLocal->Path()), FPanel->Path(), FLocal->History(), FPanel->History(),
    !FLocalActive, FSession->SessionKey(), AddBookmark, this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    Switched = Dialog.Switched();
    return false;
  }
  bool Result = true;
  if (!Dialog.LocalDirectory().isEmpty())
  {
    Result = GoLocal(Dialog.LocalDirectory()) && Result;
  }
  if (!Dialog.RemoteDirectory().isEmpty())
  {
    Result = GoRemote(Dialog.RemoteDirectory()) && Result;
  }
  return Result;
}

void MainWindow::FindFilesDialog()
{
  if (FBusy || !FSession)
  {
    return;
  }
  ::FindFilesDialog * Dialog = new ::FindFilesDialog(this, FPanel->Path());
  Dialog->show();
  Dialog->raise();
  Dialog->activateWindow();
}

bool MainWindow::FindFiles(const QString & Directory, const QString & Mask,
  const std::function<bool(const std::string & Directory, const CoreBridge::TRemoteEntry & Entry)> & Found,
  const std::function<bool(const std::string & Directory)> & Searching)
{
  if (FBusy || !FSession)
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    FSession->FindFiles(Directory.toStdString(), Mask.toStdString(), Found, Searching);
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  return Result;
}

bool MainWindow::FocusRemote(const QString & Directory, const QString & Name)
{
  if (FBusy || !FSession || !GoRemote(Directory))
  {
    return false;
  }
  FPanel->Select({ Name });
  ActivatePanel(false);
  raise();
  activateWindow();
  return true;
}

bool MainWindow::DownloadFound(const std::vector<int> & Indexes, const QStringList & Names)
{
  if (FBusy || !FSession)
  {
    return false;
  }
  QString Target = QDir::toNativeSeparators(FLocal->Path());
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (!CopyDialog(false, Names, Target, false, nullptr, Settings))
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    Result = FSession->DownloadFound(Indexes, QDir::toNativeSeparators(Target).toStdString(), &Settings);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FLocal->Refresh();
  UpdateActions();
  return Result;
}

bool MainWindow::DeleteFound(const std::vector<int> & Indexes, const QStringList & Names)
{
  if (FBusy || !FSession)
  {
    return false;
  }
  // Core's confirmation, as the panels ask it.
  const bool One = (Names.size() == 1);
  QString Message = QString::fromStdString(CoreBridge::Text(One ? CoreBridge::TText::ConfirmDeleteFile : CoreBridge::TText::ConfirmDeleteFiles));
  Message = One ? Message.replace(QStringLiteral("%s"), Names.first()) : Message.replace(QStringLiteral("%d"), QString::number(Names.size()));
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Confirmation;
  Query.Message = Message.toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  CoreBridge::TQueryButton Cancel;
  Cancel.Answer = CoreBridge::AnswerCancel;
  Cancel.Caption = "Cancel";
  Query.Buttons = { OK, Cancel };
  Query.CancelAnswer = CoreBridge::AnswerCancel;
  QtFrontend Frontend;
  Frontend.SetWindow(QApplication::activeWindow());
  if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    Result = FSession->DeleteFound(Indexes);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  UpdateActions();
  return Result;
}

bool MainWindow::Filter()
{
  QString Mask = FLocalActive ? FLocal->Filter() : FPanel->Filter();
  if (Mask.isEmpty())
  {
    Mask = QStringLiteral("*.*");
  }
  if (!MaskDialog(this, TMaskDialogMode::Filter, Mask))
  {
    return false;
  }
  const QString Normalized = QString::fromStdString(CoreBridge::NormalizeMask(Mask.toStdString()));
  try
  {
    // A mask that is not one: core's error, the filter as it was.
    CoreBridge::SplitMask(Normalized.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  if (FLocalActive)
  {
    FLocal->SetFilter(Normalized);
  }
  else
  {
    FPanel->SetFilter(Normalized);
  }
  return true;
}

void MainWindow::ServerInfo()
{
  if (FBusy || !FSession)
  {
    return;
  }
  try
  {
    const CoreBridge::TServerInfo Info = FSession->ServerInfo();
    ServerInfoDialog Dialog(FSession.get(), Info, FPanel->Path(), this);
    Dialog.exec();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
}

void MainWindow::GenerateUrl(bool Files)
{
  if (FBusy || !FSession)
  {
    return;
  }
  std::vector<std::string> Paths;
  if (Files)
  {
    QStringList Names = FPanel->Selected();
    if (Names.isEmpty() && (FPanel->View()->CurrentEntry() != nullptr))
    {
      Names.append(QString::fromStdString(FPanel->View()->CurrentEntry()->Name));
    }
    if (Names.isEmpty())
    {
      return;
    }
    const QString Directory = FPanel->Path();
    for (const QString & Name : Names)
    {
      Paths.push_back((Directory + (Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name).toStdString());
    }
  }
  GenerateUrlDialog Dialog(FSession.get(), Paths, this);
  Dialog.exec();
}

// As TCustomScpExplorerForm::EnsureCommandSessionFallback: true when the
// session runs commands (Shell: a shell's, as custom commands need), after
// opening a separate shell session, asked first, where it must.
bool MainWindow::EnsureShellSession(bool Shell)
{
  if (Shell ? FSession->CanRunShellCommands() : FSession->CanRunCommands())
  {
    return true;
  }
  if (!FSession->CanOpenShellSession())
  {
    CoreBridge::TError Error;
    Error.Message = CoreBridge::Text(CoreBridge::TText::NotSupported);
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(Error);
    return false;
  }
  if (CoreBridge::ConfirmCommandSession())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = FSession->CommandSessionQuestion();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    Query.NeverAskAgainCheck = true;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
    if (Answer.Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
    if (Answer.NeverAskAgain)
    {
      try
      {
        CoreBridge::SetConfirmCommandSession(false);
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
      }
    }
  }
  FBusy = true;
  UpdateActions();
  bool Opened = false;
  try
  {
    FSession->OpenShellSession();
    Opened = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  return Opened;
}

bool MainWindow::OpenConsole()
{
  if (FBusy || !FSession || !EnsureShellSession(false))
  {
    return false;
  }
  ConsoleDialog Dialog(FSession.get(), this);
  Dialog.exec();
  // A "cd" or a change made there: the panel reads its directory again.
  FPanel->Refresh();
  UpdateTitle();
  return true;
}

// As TNonVisualDataModule::CreateCustomCommandsMenu: the file commands under
// Files, the others under Commands, each with its key; then Enter... and
// Last.
void MainWindow::UpdateCustomCommands()
{
  for (QMenu * Menu : { FFileCommandsMenu, FStaticCommandsMenu })
  {
    Menu->clear();
  }
  for (const CoreBridge::TCustomCommandDefinition & Command : CoreBridge::CustomCommands())
  {
    const bool FileCommand = CoreBridge::CheckCustomCommand(Command.Command, !Command.Local).FileCommand;
    QAction * Action = (FileCommand ? FFileCommandsMenu : FStaticCommandsMenu)->addAction(QString::fromStdString(Command.Name));
    const QString ShortCut = ShortCutText(Command.ShortCut);
    if (!ShortCut.isEmpty())
    {
      Action->setShortcut(QKeySequence::fromString(ShortCut, QKeySequence::NativeText));
    }
    // Local commands (on downloaded files) are not ported yet.
    Action->setEnabled(!Command.Local);
    connect(Action, &QAction::triggered, this, [this, Command]() { RunCustomCommand(Command); });
  }
  for (QMenu * Menu : { FFileCommandsMenu, FStaticCommandsMenu })
  {
    const bool Files = (Menu == FFileCommandsMenu);
    if (!Menu->isEmpty())
    {
      Menu->addSeparator();
    }
    Menu->addAction(tr("&Enter..."), this, [this, Files]() { EnterCustomCommand(Files); });
    if (FHasLastCustomCommand)
    {
      const CoreBridge::TCustomCommandDefinition Last = FLastCustomCommand;
      Menu->addAction(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CustomCommandLast))
        .replace(QStringLiteral("%s"), QString::fromStdString(Last.Command).replace(QLatin1Char('&'), QStringLiteral("&&"))),
        this, [this, Last]() { RunCustomCommand(Last); });
    }
  }
}

bool MainWindow::RunCustomCommand(const CoreBridge::TCustomCommandDefinition & Command)
{
  if (FBusy || !FSession)
  {
    return false;
  }
  QStringList Names = FPanel->Selected();
  if (Names.isEmpty() && (FPanel->View()->CurrentEntry() != nullptr))
  {
    Names.append(QString::fromStdString(FPanel->View()->CurrentEntry()->Name));
  }
  const CoreBridge::TCustomCommandCheck Check = CoreBridge::CheckCustomCommand(Command.Command, !Command.Local);
  if (Check.FileCommand && Names.isEmpty())
  {
    return false;
  }
  if (!Command.Local && !EnsureShellSession(true))
  {
    return false;
  }
  std::vector<std::string> List;
  for (const QString & Name : Names)
  {
    List.push_back(Name.toStdString());
  }
  const CoreBridge::TCustomCommandPrompts Prompts = CommandPrompts();
  FBusy = true;
  UpdateActions();
  bool Result = false;
  std::vector<std::string> Output;
  try
  {
    Output = FSession->RunCustomCommand(Command, List, Prompts);
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  FPanel->Refresh();
  UpdateActions();
  if (Result && !Output.empty())
  {
    QStringList Lines;
    for (const std::string & Line : Output)
    {
      Lines.append(QString::fromStdString(Line));
    }
    if (Command.CopyResults)
    {
      QApplication::clipboard()->setText(Lines.join(QLatin1Char('\n')));
    }
    if (Command.ShowResults)
    {
      ConsoleDialog Dialog(FSession.get(), this);
      Dialog.AddOutput(Lines);
      Dialog.exec();
    }
    else if (Command.ShowResultsInMessageBox)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Information;
      Query.Message = Lines.join(QLatin1Char('\n')).toStdString();
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = "OK";
      Query.Buttons = { OK };
      Query.CancelAnswer = CoreBridge::AnswerOK;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.Query(Query);
    }
  }
  return Result;
}

bool MainWindow::EnterCustomCommand(bool Files)
{
  Q_UNUSED(Files);
  if (FBusy || !FSession)
  {
    return false;
  }
  CoreBridge::TCustomCommandDefinition Command = FHasLastCustomCommand ? FLastCustomCommand : CoreBridge::TCustomCommandDefinition();
  CustomCommandDialog Dialog(CustomCommandDialog::TMode::AdHoc, Command, CoreBridge::CustomCommands(), this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  FLastCustomCommand = Dialog.Command();
  FHasLastCustomCommand = true;
  UpdateCustomCommands();
  return RunCustomCommand(FLastCustomCommand);
}

// --- Session tabs ----------------------------------------------------------------

CoreBridge::TSession * MainWindow::TabSession(int Index) const
{
  if ((Index < 0) || (Index >= static_cast<int>(FTabs.size())))
  {
    return nullptr;
  }
  return (Index == FActiveTab) ? FSession.get() : FTabs[Index].Session.get();
}

bool MainWindow::SessionOpen(const CoreBridge::TSession * Session) const
{
  for (int Index = 0; (Session != nullptr) && (Index < TabCount()); Index++)
  {
    if (TabSession(Index) == Session)
    {
      return true;
    }
  }
  return false;
}

void MainWindow::ParkActiveTab()
{
  if ((FActiveTab < 0) || (FActiveTab >= static_cast<int>(FTabs.size())))
  {
    return;
  }
  TTab & Tab = FTabs[FActiveTab];
  Tab.Session = std::move(FSession);
  Tab.LocalPath = FLocal->Path();
  Tab.QueueDone = FQueueDone;
  FActiveTab = -1;
}

// As TTerminalManager::SetActiveTerminal: the panels on the tab's session,
// the local one on its directory.
void MainWindow::UnparkTab(int Index)
{
  TTab & Tab = FTabs[Index];
  FSession = std::move(Tab.Session);
  FActiveTab = Index;
  FQueueDone = Tab.QueueDone;
  FPanel->SetSession(FSession.get());
  FQueue->SetSession(FSession.get());
  if (!Tab.LocalPath.isEmpty())
  {
    FLocal->ChangeDirectory(Tab.LocalPath);
  }
  if (FSession)
  {
    ApplyPreferences();
    FQueueTimer->start();
  }
}

bool MainWindow::SwitchTab(int Index)
{
  if (FBusy || (Index < 0) || (Index >= TabCount()))
  {
    return false;
  }
  if (Index == FActiveTab)
  {
    return true;
  }
  ParkActiveTab();
  UnparkTab(Index);
  UpdateTabs();
  UpdateTitle();
  UpdateActions();
  return true;
}

bool MainWindow::CloseTab(int Index)
{
  if (FBusy || !SwitchTab(Index))
  {
    return false;
  }
  if (!CloseEditors(true, FSession.get()))
  {
    return false;
  }
  Disconnect();
  return true;
}

void MainWindow::CycleTabs(bool Forward)
{
  if (TabCount() > 1)
  {
    SwitchTab((FActiveTab + (Forward ? 1 : TabCount() - 1)) % TabCount());
  }
}

bool MainWindow::DuplicateTab()
{
  if (FBusy || !FSession)
  {
    return false;
  }
  std::unique_ptr<CoreBridge::TSession> Copy;
  try
  {
    Copy = FSession->Duplicate();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  return Open(std::move(Copy));
}

// The tab bar as the tabs are: each session's name, the active one current.
void MainWindow::UpdateTabs()
{
  FUpdatingTabs = true;
  while (FTabBar->count() > TabCount())
  {
    FTabBar->removeTab(FTabBar->count() - 1);
  }
  for (int Index = 0; Index < TabCount(); Index++)
  {
    CoreBridge::TSession * Session = TabSession(Index);
    const QString Name = (Session != nullptr) ? QString::fromStdString(Session->Name()) : QString();
    if (Index >= FTabBar->count())
    {
      FTabBar->addTab(Name);
    }
    else
    {
      FTabBar->setTabText(Index, Name);
    }
  }
  if (FActiveTab >= 0)
  {
    FTabBar->setCurrentIndex(FActiveTab);
  }
  FTabBar->setVisible(TabCount() > 0);
  FUpdatingTabs = false;
}

// --- Workspaces --------------------------------------------------------------------

std::vector<CoreBridge::TSession::TWorkspaceTab> MainWindow::WorkspaceTabs() const
{
  std::vector<CoreBridge::TSession::TWorkspaceTab> Result;
  for (int Index = 0; Index < TabCount(); Index++)
  {
    CoreBridge::TSession::TWorkspaceTab Tab;
    Tab.Session = TabSession(Index);
    Tab.LocalDirectory = QDir::toNativeSeparators((Index == FActiveTab) ? FLocal->Path() : FTabs[Index].LocalPath).toStdString();
    if (Tab.Session != nullptr)
    {
      Result.push_back(Tab);
    }
  }
  return Result;
}

QString MainWindow::WorkspaceName() const
{
  if (!FLastWorkspace.isEmpty())
  {
    return FLastWorkspace;
  }
  const std::string Auto = CoreBridge::WorkspaceSettings().AutoWorkspace;
  return !Auto.empty() ? QString::fromStdString(Auto) :
    QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NewWorkspace));
}

bool MainWindow::SaveWorkspace(const QString * Name, bool SavePasswords, bool AutoSave)
{
  if (FBusy || (TabCount() == 0))
  {
    return false;
  }
  const std::vector<CoreBridge::TSession::TWorkspaceTab> Tabs = WorkspaceTabs();
  QString AName;
  try
  {
    if (Name != nullptr)
    {
      AName = *Name;
      CoreBridge::ValidateWorkspaceName(AName.toStdString());
    }
    else
    {
      const bool CanSavePasswords = CoreBridge::TSession::WorkspaceHasPasswords(Tabs);
      SaveWorkspaceDialog Dialog(WorkspaceName(), CanSavePasswords, false, false, this);
      if (Dialog.exec() != QDialog::Accepted)
      {
        return false;
      }
      AName = Dialog.Name();
      SavePasswords = Dialog.SavePasswords();
      AutoSave = Dialog.AutoSave();
    }
    // As DoSaveWorkspace.
    FLastWorkspace = AName;
    CoreBridge::TSession::SaveWorkspace(AName.toStdString(), Tabs, SavePasswords);
    if (AutoSave)
    {
      CoreBridge::TWorkspaceSettings Workspace = CoreBridge::WorkspaceSettings();
      Workspace.AutoSave = true;
      Workspace.AutoWorkspace = AName.toStdString();
      Workspace.AutoSavePasswords = SavePasswords;
      CoreBridge::SetWorkspaceSettings(Workspace);
    }
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

// As DoOpenFolderOrWorkspace: each of its sessions connected in a tab of its
// own (WinSCP connects the first only, unless "connect all"), the first
// active.
bool MainWindow::OpenWorkspace(const QString & Name)
{
  if (FBusy)
  {
    return false;
  }
  std::vector<std::unique_ptr<CoreBridge::TSession>> Sessions;
  try
  {
    Sessions = CoreBridge::TSession::Workspace(Name.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  const int First = TabCount();
  bool Any = false;
  for (std::unique_ptr<CoreBridge::TSession> & Session : Sessions)
  {
    Any = Open(std::move(Session)) || Any;
  }
  if (Any)
  {
    FLastWorkspace = Name;
    SwitchTab(First);
  }
  return Any;
}

// --- Menus --------------------------------------------------------------------

FileView * MainWindow::ActiveView() const
{
  return FLocalActive ? FLocal->View() : FPanel->View();
}

// Mark > Select/Unselect Files with Same Extension: the focused file's.
void MainWindow::SelectSameExtension(bool Select)
{
  FileView * View = ActiveView();
  const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
  if ((Entry != nullptr) && !Entry->Directory)
  {
    const QString Name = QString::fromStdString(Entry->Name);
    const int Dot = Name.lastIndexOf(QLatin1Char('.'));
    View->SelectByMask(Select, (Dot > 0) ? (QStringLiteral("*") + Name.mid(Dot)) : QStringLiteral("*."), false);
  }
}

// The Local and Remote menus, as WinSCP's: the panel's own, acted on as the
// active one.
QMenu * MainWindow::PanelMenu(bool Local)
{
  QMenu * Menu = menuBar()->addMenu(Local ? tr("&Local") : tr("&Remote"));
  auto OnPanel = [this, Local](auto Function)
  {
    return [this, Local, Function]()
    {
      ActivatePanel(Local);
      Function();
    };
  };
  QMenu * GoTo = Menu->addMenu(tr("&Go To"));
  GoTo->addAction(tr("&Open Directory/Bookmark..."), this, OnPanel([this]() { OpenDirectory(false); }));
  if (Local)
  {
    GoTo->addAction(FExploreAction);
  }
  GoTo->addSeparator();
  for (QAction * Action : Local ?
         std::initializer_list<QAction *>{ FLocal->ParentAction(), FLocal->RootAction(), FLocal->HomeAction() } :
         std::initializer_list<QAction *>{ FPanel->ParentAction(), FPanel->RootAction(), FPanel->HomeAction() })
  {
    GoTo->addAction(Action);
  }
  GoTo->addSeparator();
  GoTo->addAction(Local ? FLocal->BackAction() : FPanel->BackAction());
  GoTo->addAction(Local ? FLocal->ForwardAction() : FPanel->ForwardAction());
  Menu->addAction(Local ? FLocal->RefreshAction() : FPanel->RefreshAction());
  Menu->addAction(tr("&Add Path to Bookmarks"), this, OnPanel([this]() { OpenDirectory(true); }));
  Menu->addAction(tr("Copy &Path to Clipboard"), this, [this, Local]()
  {
    QApplication::clipboard()->setText(Local ? QDir::toNativeSeparators(FLocal->Path()) : FPanel->Path());
  });
  Menu->addSeparator();
  // The view's columns, as its header's menu.
  QMenu * Columns = Menu->addMenu(tr("&Columns"));
  connect(Columns, &QMenu::aboutToShow, this, [this, Local, Columns]()
  {
    Columns->clear();
    QMenu * Own = (Local ? FLocal->View() : FPanel->View())->ColumnsMenu();
    for (QAction * Action : Own->actions())
    {
      Action->setParent(Columns);
      Columns->addAction(Action);
    }
    delete Own;
  });
  Menu->addAction(tr("&Filter..."), this, OnPanel([this]() { Filter(); }));
  return Menu;
}

// As WinSCP's Commander's menu bar: Local, Mark, Files, Commands, Tabs,
// Options, Remote, Help, each in its order. What linscp does not have yet
// is left out, as are the actions WinSCP keeps to keys (Next and Previous
// Tab, Show Hidden Files, Ctrl+O and Ctrl+B on the active panel).
void MainWindow::BuildMenus()
{
  FLocalMenu = PanelMenu(true);

  QMenu * Mark = menuBar()->addMenu(tr("&Mark"));
  Mark->addAction(tr("&Select/Unselect"), this, [this]() { ActiveView()->ToggleCurrent(); });
  Mark->addAction(tr("Sele&ct Files..."), this, [this]() { ActiveView()->SelectByMaskDialog(true); });
  Mark->addAction(tr("&Unselect Files..."), this, [this]() { ActiveView()->SelectByMaskDialog(false); });
  Mark->addAction(tr("Select &All"), this, [this]() { ActiveView()->selectAll(); });
  Mark->addSeparator();
  Mark->addAction(tr("&Invert Selection"), this, [this]() { ActiveView()->InvertSelection(); });
  Mark->addAction(tr("C&lear Selection"), this, [this]() { ActiveView()->clearSelection(); });
  Mark->addSeparator();
  Mark->addAction(tr("Select Files with Same &Extension"), this, [this]() { SelectSameExtension(true); });
  Mark->addAction(tr("Unselect Files with Same E&xtension"), this, [this]() { SelectSameExtension(false); });

  // The active panel's.
  auto OnActive = [this](QAction * (LocalPanel::*LocalAction)() const, QAction * (RemotePanel::*RemoteAction)() const)
  {
    return [this, LocalAction, RemoteAction]()
    {
      (FLocalActive ? (FLocal->*LocalAction)() : (FPanel->*RemoteAction)())->trigger();
    };
  };
  QMenu * Files = menuBar()->addMenu(tr("&Files"));
  QMenu * New = Files->addMenu(tr("&New"));
  New->addAction(tr("&Directory..."), this, OnActive(&LocalPanel::NewFolderAction, &RemotePanel::NewFolderAction));
  Files->addSeparator();
  Files->addAction(FOpenCurrentAction);
  Files->addAction(FEditAction);
  Files->addMenu(FEditWithMenu);
  Files->addAction(FLinkAction);
  Files->addSeparator();
  Files->addAction(FCopyAction);
  Files->addAction(FMoveAction);
  Files->addAction(FDuplicateAction);
  Files->addAction(FMoveToAction);
  Files->addAction(tr("&Delete"), this, OnActive(&LocalPanel::DeleteAction, &RemotePanel::DeleteAction));
  Files->addAction(tr("&Rename"), this, OnActive(&LocalPanel::RenameAction, &RemotePanel::RenameAction));
  Files->addSeparator();
  Files->addMenu(FFileCommandsMenu);
  QMenu * Names = Files->addMenu(tr("&File Names"));
  Names->addAction(tr("Copy to &Clipboard"), this, [this]() { CopyFileNames(FLocalActive, false); });
  Names->addAction(tr("Copy to Clipboard (Include &Paths)"), this, [this]() { CopyFileNames(FLocalActive, true); });
  Names->addAction(FGenerateFileUrlAction);
  Files->addSeparator();
  Files->addAction(FPropertiesAction);

  QMenu * Commands = menuBar()->addMenu(tr("&Commands"));
  Commands->addAction(FKeepUpToDateAction);
  Commands->addAction(FSynchronizeAction);
  Commands->addAction(FFindFilesAction);
  Commands->addMenu(FStaticCommandsMenu);
  Commands->addSeparator();
  Commands->addAction(FConsoleAction);
  Commands->addAction(FPuttyAction);
  Commands->addSeparator();
  Commands->addAction(FQuitAction);

  QMenu * Tabs = menuBar()->addMenu(tr("&Tabs"));
  QAction * NewTab = Tabs->addAction(tr("&New Tab..."), this, &MainWindow::SiteManagerDialog);
  NewTab->setShortcut(FSitesAction->shortcut());
  FSitesAction->setShortcut(QKeySequence());
  Tabs->addAction(FCloseTabAction);
  Tabs->addAction(FDuplicateTabAction);
  Tabs->addSeparator();
  Tabs->addAction(FDisconnectAction);
  Tabs->addSeparator();
  Tabs->addAction(FServerInfoAction);
  Tabs->addAction(FGenerateSessionUrlAction);
  Tabs->addSeparator();
  // As WinSCP's Opened Tabs and Workspaces: listed as the menu opens.
  QMenu * Opened = Tabs->addMenu(tr("&Opened Tabs"));
  connect(Opened, &QMenu::aboutToShow, this, [this, Opened]()
  {
    Opened->clear();
    for (int Index = 0; Index < TabCount(); Index++)
    {
      QAction * Action = Opened->addAction(FTabBar->tabText(Index), this, [this, Index]() { SwitchTab(Index); });
      Action->setCheckable(true);
      Action->setChecked(Index == FActiveTab);
    }
  });
  QMenu * Workspaces = Tabs->addMenu(tr("&Workspaces"));
  connect(Workspaces, &QMenu::aboutToShow, this, [this, Workspaces]()
  {
    Workspaces->clear();
    for (const std::string & Name : CoreBridge::WorkspaceNames())
    {
      const QString AName = QString::fromStdString(Name);
      Workspaces->addAction(AName, this, [this, AName]() { OpenWorkspace(AName); });
    }
  });
  Tabs->addAction(FSaveWorkspaceAction);
  Tabs->addSeparator();
  // As WinSCP's Sites: the site manager, then the sites to open in a tab.
  QMenu * Sites = Tabs->addMenu(tr("Si&tes"));
  connect(Sites, &QMenu::aboutToShow, this, [this, Sites]()
  {
    Sites->clear();
    Sites->addAction(FSitesAction);
    Sites->addAction(FOpenAction);
    Sites->addSeparator();
    for (const std::string & Name : CoreBridge::SiteNames())
    {
      Sites->addAction(QString::fromStdString(Name), this, [this, Name]()
      {
        try
        {
          OpenSite(CoreBridge::LoadSite(Name));
        }
        catch (const CoreBridge::TCoreError & E)
        {
          ShowError(E);
        }
      });
    }
  });

  QMenu * Options = menuBar()->addMenu(tr("&Options"));
  Options->addAction(FPreferencesAction);

  FRemoteMenu = PanelMenu(false);

  QMenu * Help = menuBar()->addMenu(tr("&Help"));
  Help->addAction(FAboutAction);

  // The keys of what no menu shows.
  addActions({ FSitesAction, FOpenAction, FOpenDirectoryAction, FAddBookmarkAction, FFilterAction, FHiddenAction,
    FNextTabAction, FPreviousTabAction, FExploreAction });
}
