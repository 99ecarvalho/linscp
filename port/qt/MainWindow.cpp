#include "MainWindow.h"

#include "ColorMenu.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QStorageInfo>
#include <QComboBox>
#include <QShortcut>
#include <QHeaderView>
#include <QListWidget>
#include <QRegularExpression>
#include <QToolBar>
#include <QToolButton>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QStatusBar>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QActionGroup>
#include <QColorDialog>
#include <QDateTime>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QTemporaryDir>
#include <QTabBar>
#include <QTimer>
#include <QTreeView>
#include <QSplitter>
#include <QScreen>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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
#include "CopyLocalDialog.h"
#include "IconSet.h"
#include "Theme.h"
#include "DragExtClient.h"
#include "ExtensionOptionsDialog.h"
#include "Integration.h"
#include "MasterPasswordDialog.h"
#include "WindowParams.h"
#include "PanelBands.h"
#include "SaveSiteDialog.h"
#include "KeyVerify.h"
#include "TransferSettingsDialog.h"

namespace
{

// Help > Donate's page (DonatePageAction): linscp's GitHub Sponsors page
// until linscp has a page of its own.
const char DonateUrl[] = "https://github.com/sponsors/99ecarvalho";


// ExtractShortName: a path's last part, the root as it is.
QString ShortPath(const QString & Path)
{
  const QString Clean = QDir::cleanPath(Path);
  const QString Last = QFileInfo(Clean).fileName();
  return QDir::toNativeSeparators(Last.isEmpty() ? Clean : Last);
}


}  // namespace

// linscp's icon (port/qt/icons/linscp.ico), loaded once.
const QIcon & MainWindow::AppIcon()
{
  static const QIcon Icon(QStringLiteral(":/icons/linscp.ico"));
  return Icon;
}

MainWindow::MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QMainWindow(Parent),
  FInfo(Info)
{
  setWindowTitle(QString::fromStdString(FInfo.AppName));
  // The interface, as chosen when the window starts (WinSCP's applies on
  // the next start).
  FExplorer = CoreBridge::Preferences().ExplorerInterface;
  // linscp's icon, every window's (the application's).
  QApplication::setWindowIcon(AppIcon());
  // As TTerminalManager's OnMasterPasswordPrompt: over the window in front.
  CoreBridge::SetMasterPasswordPrompt([this]()
  {
    QWidget * Parent = QApplication::activeModalWidget();
    return MasterPasswordDialog::AskCurrent((Parent != nullptr) ? Parent : this);
  });

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
  // The local browser's right panel, in the remote one's place in a local tab.
  FOtherLocal = new LocalPanel(Panels);
  FOtherLocal->setVisible(false);
  Panels->setChildrenCollapsible(false);
  FQueue = new QueuePanel(Vertical);
  // Dropped: handled once the drag is over (its loop is the source's, and on
  // Windows the system's), with a copy of what was dropped.
  // (The other local panel is the right side, as the remote one.)
  for (FileView * View : { FLocal->View(), FPanel->View(), FOtherLocal->View() })
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
    // As DDFakeFileInitDrag (Drag & Drop's "Determine drop target by
    // dragging a fake file", with WinSCP's shell extension there): a hidden
    // "scp" directory dragged, which the extension's copy hook stops in
    // Explorer, saying where; then downloaded straight there.
    FFakeDrag.reset();
    if (CoreBridge::Preferences().DDFakeFile && DragExt::Installed() && !DragExt::Broken())
    {
      auto Fake = std::make_unique<TFakeDrag>();
      Fake->Directory = std::make_unique<QTemporaryDir>(
        QDir(QString::fromStdString(CoreBridge::TemporaryDirectoryBase())).filePath(QStringLiteral("scpXXXXXX")));
      Fake->Names = Drag.Names;
      if (Fake->Directory->isValid() && Fake->Drag.Begin(Fake->Directory->path()))
      {
        QMimeData * Data = MakeFileDrag(Drag);
        Data->setUrls({ QUrl::fromLocalFile(Fake->Directory->path()) });
        FFakeDrag = std::move(Fake);
        return Data;
      }
    }
    // Under Storage's temporary directory (else the system's), as WinSCP's
    // (TWinConfiguration::TemporaryDir: "scp" and a unique part).
    FDragDirectories.push_back(std::make_unique<QTemporaryDir>(
      QDir(QString::fromStdString(CoreBridge::TemporaryDirectoryBase())).filePath(QStringLiteral("scpXXXXXX"))));
    const QString Directory = FDragDirectories.back()->path();
    const QStringList Names = Drag.Names;
    // The files' sizes, for the free space (directories' unknown).
    std::int64_t Size = 0;
    for (const CoreBridge::TRemoteEntry & Entry : FPanel->Entries())
    {
      if (!Entry.Directory && Names.contains(QString::fromStdString(Entry.Name)))
      {
        Size += Entry.Size;
      }
    }
    return new RemoteFileDrag(Drag, Directory, [this, Names, Directory, Size]()
    {
      FDragOuts++;
      return WarnLackOfTempSpace(Directory, Size) && Download(Names, Directory, false);
    });
  };
  // As RemoteFileControlDDEnd: a fake directory's drop, where the extension
  // said (none within linscp's own panels, which drop as usual).
  FPanel->OnDragFinished = [this](Qt::DropAction Action)
  {
    if (FFakeDrag)
    {
      std::unique_ptr<TFakeDrag> Fake = std::move(FFakeDrag);
      const QString Target = Fake->DroppedHere ? QString() : Fake->Drag.Target();
      if (!Target.isEmpty())
      {
        // Prefer copy, for safety, unless it was a move.
        Transfer(false, Fake->Names, Target, (Action == Qt::MoveAction), true, false, true);
      }
    }
  };
  // A file activated: the double-click action (FileActivated).
  FLocal->OpenFile = [this](const QString &) { FileActivated(true); };
  FOtherLocal->OpenFile = [this](const QString &) { FileActivated(false); };
  FPanel->OpenFile = [this](const QString &) { FileActivated(false); };
  // Right clicks: WinSCP's popups.
  FLocal->OnContextMenu = [this](const QPoint & Global, bool OnItem)
  {
    // As DoLocalDirViewContextPopup: on a file, the shell's own menu with
    // Panels > Local's "Use system file context menu".
    if (!(OnItem && CoreBridge::Preferences().SystemContextMenu && SystemContextMenu(true, Global)))
    {
      ContextMenu(true, OnItem, Global);
    }
  };
  FPanel->OnContextMenu = [this](const QPoint & Global, bool OnItem) { ContextMenu(false, OnItem, Global); };
  FOtherLocal->OnContextMenu = [this](const QPoint & Global, bool OnItem) { ContextMenu(false, OnItem, Global); };
  connect(FOtherLocal->View(), &FileView::SwitchPanel, this, [this]() { ActivatePanel(true); });
  // A local tab's title follows its directories.
  for (LocalPanel * Panel : { FLocal, FOtherLocal })
  {
    connect(Panel, &LocalPanel::Changed, this, [this]()
    {
      if (LocalBrowser())
      {
        UpdateTabs();
      }
    });
  }
  // As DoDirViewLoaded: synchronized browsing follows (not the other local
  // panel's).
  connect(FLocal, &LocalPanel::Changed, this, [this]() { PanelLoaded(true); });
  connect(FPanel, &RemotePanel::Changed, this, [this]() { PanelLoaded(false); });
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
    else if ((Now != nullptr) && (FPanel->isAncestorOf(Now) || FOtherLocal->isAncestorOf(Now)))
    {
      FLocalActive = false;
    }
    UpdateTreeShortcut();
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
  FCopyAction = FilesMenu->addAction(tr("Cop&y..."), this, [this]() { TransferSelected(false, true); });
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
  // As LocalChangePathAction2 and RemoteChangePathAction2: each panel's
  // path drop-down (the right side's local one in a local tab).
  FChangeLocalPathAction = GoMenu->addAction(tr("C&hange Drive"), this, [this]()
  {
    ActivatePanel(true);
    FLocal->ShowPathMenu();
  });
  FChangeLocalPathAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F1));
  FChangeLocalPathAction->setStatusTip(tr("Allows selection of different drive for panel"));
  FChangeRemotePathAction = GoMenu->addAction(tr("C&hange Directory"), this, [this]()
  {
    ActivatePanel(false);
    if (LocalBrowser())
    {
      FOtherLocal->ShowPathMenu();
    }
    else
    {
      FPanel->ShowPathMenu();
    }
  });
  FChangeRemotePathAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F2));
  FChangeRemotePathAction->setStatusTip(tr("Allows selection of different directory for panel"));

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

  // Ctrl+Alt+R, as WinSCP's AutoReadDirectoryAfterOpAction (in no menu):
  // Panels > Remote's "Automatically refresh directory after operation"
  // toggled, and noted.
  QAction * AutoReadAction = new QAction(this);
  AutoReadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
  connect(AutoReadAction, &QAction::triggered, this, [this]()
  {
    bool On = false;
    try
    {
      CoreBridge::ChangePreferences([&On](CoreBridge::TPreferences & Changed)
      {
        Changed.AutoReadDirectoryAfterOp = !Changed.AutoReadDirectoryAfterOp;
        On = Changed.AutoReadDirectoryAfterOp;
      });
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return;
    }
    const QStringList Parts = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AutoReadDirectoryToggle)).split(QLatin1Char('|'));
    statusBar()->showMessage(Parts.value(0).replace(QStringLiteral("%s"), Parts.value(On ? 1 : 2)), 30000);
  });
  addAction(AutoReadAction);

  FFilterAction = ViewMenu->addAction(tr("&Filter..."), this, [this]() { Filter(); });
  FFilterAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));

  QMenu * OptionsMenu = FilesMenu;
  FPreferencesAction = OptionsMenu->addAction(tr("&Preferences..."), this, [this]() { PreferencesDialog(); });
  FPreferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P));
  BuildMenus();
  if (FExplorer)
  {
    ApplyExplorerInterface();
  }

  statusBar()->showMessage(tr("%1 %2").arg(QString::fromStdString(FInfo.AppName), QString::fromStdString(FInfo.Version)));
  // From now on a language chosen applies on the next start, as WinSCP's.
  CoreBridge::GuiStarted();
  ApplyPreferences();
  RestoreLayout();
  connect(FPanel, &RemotePanel::Changed, this, &MainWindow::UpdateTitle);
  // When the remote directory was last read (Terminal's DirectoryLoaded).
  FRemoteLoaded.start();
  connect(FPanel, &RemotePanel::Changed, this, [this]() { FRemoteLoaded.restart(); });
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

// Options > Left/Right Panel > Tree.
void MainWindow::SetPanelThumbnails(bool Left, bool Thumbnails)
{
  LocalPanel * Side = SideLocal(Left);
  if (Side != nullptr)
  {
    Side->SetThumbnails(Thumbnails);
  }
  else
  {
    FPanel->SetThumbnails(Thumbnails);
  }
  const bool Other = (Side == FOtherLocal);
  StoreLayoutChoice([Left, Other, Thumbnails](CoreBridge::TLayout & Layout)
  {
    (Left ? Layout.LocalThumbnails : (Other ? Layout.OtherLocalThumbnails : Layout.RemoteThumbnails)) = Thumbnails;
  });
}

void MainWindow::SetPanelStatusBarVisible(bool Left, bool Visible)
{
  if (Left)
  {
    FLocal->Status()->setVisible(Visible);
  }
  else
  {
    FPanel->Status()->setVisible(Visible);
    FOtherLocal->Status()->setVisible(Visible);
  }
  QAction * Action = Left ? FLocalStatusBarAction : FRemoteStatusBarAction;
  if (Action != nullptr)
  {
    Action->setChecked(Visible);
  }
}

void MainWindow::StoreLayoutChoice(const std::function<void(CoreBridge::TLayout &)> & Change)
{
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  Change(Layout);
  try
  {
    CoreBridge::SetLayout(Layout);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
}

void MainWindow::SetTreeVisible(bool Left, bool Visible)
{
  if (Left)
  {
    FLocal->SetTreeVisible(Visible);
  }
  else
  {
    FPanel->SetTreeVisible(Visible);
    FOtherLocal->SetTreeVisible(Visible);
  }
  QAction * Action = Left ? FLocalTreeAction : FRemoteTreeAction;
  if (Action != nullptr)
  {
    Action->setChecked(Visible);
  }
}

// As TScpCommanderForm's RestoreParams: the window's placement, the panels'
// split and columns, the queue's height, the active panel; WinSCP's
// formats (TWinConfiguration's ScpCommander, QueueView).
void MainWindow::RestoreLayout()
{
  const CoreBridge::TLayout Layout = CoreBridge::Layout();
  const int Dpi = logicalDpiX();
  QScreen * Screen = this->screen();
  const QRect Area = (Screen != nullptr) ? Screen->availableGeometry() : QRect(0, 0, 1280, 800);
  // As TScpExplorerForm's RestoreParams: its window (at most 960 by 720),
  // its panel's columns and tree width, the queue.
  if (FExplorer)
  {
    if (!RestoreWindowParams(this, QString::fromStdString(Layout.Explorer.WindowParams)))
    {
      resize(std::min(Area.width() - 40, 960 * Dpi / 96), std::min(Area.height() - 30, 720 * Dpi / 96));
    }
    FPanel->SetColumnsParams(QString::fromStdString(Layout.Explorer.DirViewParams));
    FPanel->SetTreeHeight(Layout.Explorer.TreeWidth * Dpi / std::max(1, Layout.Explorer.TreeWidthPixelsPerInch));
    SetTreeVisible(false, Layout.Explorer.Tree);
    SetPanelStatusBarVisible(false, Layout.RemoteStatusBar);
    const int QueueHeight = Layout.QueueViewHeight * Dpi / std::max(1, Layout.QueueViewHeightPixelsPerInch);
    FVertical->setSizes({ std::max(100, height() - QueueHeight), QueueHeight });
    ActivatePanel(false);
    return;
  }
  if (!RestoreWindowParams(this, QString::fromStdString(Layout.WindowParams)))
  {
    // TWinConfiguration::Default's: the work area, less a margin, at most
    // 1090 by 700.
    resize(std::min(Area.width() - 40, 1090 * Dpi / 96), std::min(Area.height() - 30, 700 * Dpi / 96));
  }
  FLocal->SetColumnsParams(QString::fromStdString(Layout.LocalDirViewParams));
  FPanel->SetColumnsParams(QString::fromStdString(Layout.RemoteDirViewParams));
  const int RemoteTreeHeight = Layout.RemoteTreeHeight * Dpi / std::max(1, Layout.RemoteTreeHeightPixelsPerInch);
  FLocal->SetTreeHeight(Layout.LocalTreeHeight * Dpi / std::max(1, Layout.LocalTreeHeightPixelsPerInch));
  FPanel->SetTreeHeight(RemoteTreeHeight);
  FOtherLocal->SetTreeHeight(RemoteTreeHeight);
  SetTreeVisible(true, Layout.LocalTree);
  SetTreeVisible(false, Layout.RemoteTree);
  SetPanelStatusBarVisible(true, Layout.LocalStatusBar);
  SetPanelStatusBarVisible(false, Layout.RemoteStatusBar);
  FLocal->SetThumbnails(Layout.LocalThumbnails);
  FPanel->SetThumbnails(Layout.RemoteThumbnails);
  FOtherLocal->SetThumbnails(Layout.OtherLocalThumbnails);
  const int Total = std::max(FPanels->width(), width());
  const int LocalWidth = static_cast<int>(Total * std::clamp(Layout.LocalPanelWidth, 0.1, 0.9));
  // (The right side's two panels share its width, one shown.)
  QList<int> Sizes;
  for (int Index = 0; Index < FPanels->count(); Index++)
  {
    Sizes.append((FPanels->widget(Index) == FLocal) ? LocalWidth : (Total - LocalWidth));
  }
  FPanels->setSizes(Sizes);
  const int QueueHeight = Layout.QueueViewHeight * Dpi / std::max(1, Layout.QueueViewHeightPixelsPerInch);
  FVertical->setSizes({ std::max(100, height() - QueueHeight), QueueHeight });
  ActivatePanel(Layout.CurrentPanel == 0);
}

void MainWindow::SaveLayout()
{
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  const int Dpi = logicalDpiX();
  if (FExplorer)
  {
    Layout.Explorer.WindowParams = StoreWindowParams(this).toStdString();
    Layout.Explorer.DirViewParams = FPanel->ColumnsParams().toStdString();
    Layout.Explorer.Tree = FPanel->TreeVisible();
    if (Layout.Explorer.Tree)
    {
      Layout.Explorer.TreeWidth = FPanel->TreeHeight();
      Layout.Explorer.TreeWidthPixelsPerInch = Dpi;
    }
    Layout.Explorer.ViewStyle = FPanel->ViewStyle();
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
    return;
  }
  Layout.WindowParams = StoreWindowParams(this).toStdString();
  const QList<int> Sizes = FPanels->sizes();
  const int LocalWidth = Sizes.value(FPanels->indexOf(FLocal));
  const int Total = std::max(1, LocalWidth + Sizes.value(FPanels->indexOf(FOtherLocal->isVisible() ? static_cast<QWidget *>(FOtherLocal) :
    static_cast<QWidget *>(FPanel))));
  Layout.LocalPanelWidth = static_cast<double>(LocalWidth) / Total;
  Layout.CurrentPanel = FLocalActive ? 0 : 1;
  Layout.LocalDirViewParams = FLocal->ColumnsParams().toStdString();
  Layout.RemoteDirViewParams = FPanel->ColumnsParams().toStdString();
  Layout.LocalTree = FLocal->TreeVisible();
  Layout.RemoteTree = FPanel->TreeVisible();
  if (Layout.LocalTree)
  {
    Layout.LocalTreeHeight = FLocal->TreeHeight();
    Layout.LocalTreeHeightPixelsPerInch = Dpi;
  }
  if (Layout.RemoteTree)
  {
    Layout.RemoteTreeHeight = FPanel->TreeHeight();
    Layout.RemoteTreeHeightPixelsPerInch = Dpi;
  }
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

bool MainWindow::PreferencesDialog(const QString & Page)
{
  ::PreferencesDialog Dialog(this);
  struct TQueueViewChoice
  {
    MainWindow * Window;
    ::PreferencesDialog * Dialog;
    // The page's queue list as Options > Queue, once accepted.
    void Apply() const
    {
      const int Show = Dialog->QueueViewChoice();
      if (Show >= 0)
      {
        Window->SetQueueShow(static_cast<CoreBridge::TLayout::TQueueShow>(Show));
      }
    }
  } QueueView{ this, &Dialog };
  if (FSession && FSession->Active() && !LocalBrowser())
  {
    // A preset's Current button: what autoselection would match now.
    Dialog.SetCurrentRuleData(TransferPresetAutoSelectData());
    // Extensions' options kept per site: this session's.
    Dialog.SetSessionKey(FSession->SessionKey());
  }
  if (!Page.isEmpty())
  {
    Dialog.ShowPage(Page);
  }
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  try
  {
    Dialog.Save();
    QueueView.Apply();
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
  // Interface's theme, at once (the icons take the palette's colours).
  const bool ThemeChanged = (Preferences.DarkTheme != FAppliedTheme);
  if (ThemeChanged)
  {
    FAppliedTheme = Preferences.DarkTheme;
    Theme::Apply(Preferences.DarkTheme);
  }
  if ((QString::fromStdString(Preferences.IconSet) != IconSet::Current()) || ThemeChanged)
  {
    IconSet::SetCurrent(QString::fromStdString(Preferences.IconSet));
    ApplyIcons();
  }
  // Panels > Remote's "Refresh remote panel every": looked at each second
  // while idle, as WinSCP's ApplicationIdle.
  if (Preferences.RefreshRemotePanel && (FRefreshRemoteTimer == nullptr))
  {
    FRefreshRemoteTimer = new QTimer(this);
    connect(FRefreshRemoteTimer, &QTimer::timeout, this, &MainWindow::RefreshRemotePanelIfDue);
  }
  if (FRefreshRemoteTimer != nullptr)
  {
    FRefreshRemoteInterval = Preferences.RefreshRemotePanelInterval;
    if (Preferences.RefreshRemotePanel)
    {
      FRefreshRemoteTimer->start(1000);
    }
    else
    {
      FRefreshRemoteTimer->stop();
    }
  }
  FLocal->ApplyPreferences(Preferences);
  FPanel->ApplyPreferences(Preferences);
  FHiddenAction->setChecked(Preferences.ShowHiddenFiles);
  // Commander: local left, remote right, unless swapped.
  // The other local panel where the remote one is.
  if (Preferences.SwappedPanels)
  {
    FPanels->insertWidget(0, FOtherLocal);
    FPanels->insertWidget(0, FPanel);
  }
  else
  {
    FPanels->insertWidget(0, FLocal);
  }
  FOtherLocal->ApplyPreferences(Preferences);
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
  ApplyExplorerShortcuts();
  UpdateTitle();
}

MainWindow::~MainWindow()
{
  CoreBridge::SetMasterPasswordPrompt(nullptr);
  // As WinSCP's ClipboardClear: remote files on the clipboard download
  // through this window; they go with it.
  if ((FClipboardData != nullptr) && (QApplication::clipboard()->mimeData() == FClipboardData))
  {
    QApplication::clipboard()->clear();
  }
  // Editors are windows of their own (not children): they go with it.
  for (EditorWindow * Editor : Editors())
  {
    delete Editor;
  }
  FSession.reset();
  CleanUpEdited();
}

bool MainWindow::ConfirmClosingSessions()
{
  if (FStandalone || !CoreBridge::Preferences().ConfirmClosingSession)
  {
    return true;
  }
  int Active = (FSession && FSession->Active()) ? 1 : 0;
  for (const TTab & Tab : FTabs)
  {
    if (Tab.Session && Tab.Session->Active())
    {
      Active++;
    }
  }
  // With a session open, or several tabs and no workspace saved
  // automatically.
  const bool AutoSave = CoreBridge::WorkspaceSettings().AutoSave;
  if (!((Active > 0) || ((TabCount() > 1) && !AutoSave)))
  {
    return true;
  }
  using CoreBridge::TText;
  QString Message;
  if ((Active == 1) && FSession && FSession->Active())
  {
    Message = QString::fromStdString(CoreBridge::Text(AutoSave ? TText::CloseSession : TText::CloseSessionWorkspace))
      .replace(QStringLiteral("%s"), QString::fromStdString(FSession->Name()));
  }
  else if (Active > 0)
  {
    Message = QString::fromStdString(CoreBridge::Text(AutoSave ? TText::CloseSessions : TText::CloseSessionsWorkspace));
  }
  else
  {
    Message = QString::fromStdString(CoreBridge::Text(TText::CloseWorkspace));
  }
  const QString Note = AutoSave ?
    QString::fromStdString(CoreBridge::Text(TText::AutoWorkspace)).replace(QStringLiteral("%s"), WorkspaceName()) :
    QString::fromStdString(CoreBridge::Text(TText::AutoWorkspaceEnable));
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Confirmation;
  Query.Message = (Message + QStringLiteral("\n\n") + Note).toStdString();
  auto Button = [](unsigned Answer, const QString & Caption)
  {
    CoreBridge::TQueryButton Result;
    Result.Answer = Answer;
    Result.Caption = Caption.toStdString();
    return Result;
  };
  if (AutoSave)
  {
    Query.Buttons = { Button(CoreBridge::AnswerOK, QMessageBox::tr("OK")), Button(CoreBridge::AnswerCancel, QMessageBox::tr("Cancel")) };
  }
  else
  {
    Query.Buttons = { Button(CoreBridge::AnswerYes, QMessageBox::tr("&Yes")), Button(CoreBridge::AnswerNo, QMessageBox::tr("&No")),
      Button(CoreBridge::AnswerCancel, QMessageBox::tr("Cancel")) };
  }
  Query.CancelAnswer = CoreBridge::AnswerCancel;
  Query.NeverAskAgainCheck = true;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
  if (Answer.Answer == CoreBridge::AnswerCancel)
  {
    return false;
  }
  if (Answer.NeverAskAgain)
  {
    try
    {
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed) { Changed.ConfirmClosingSession = false; });
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  // No: the workspace saved, and from now on automatically.
  return (Answer.Answer != CoreBridge::AnswerNo) || SaveWorkspace(nullptr, false, false, true);
}

void MainWindow::closeEvent(QCloseEvent * Event)
{
  if (!ConfirmClosingSessions())
  {
    Event->ignore();
    return;
  }
  // As WinSCP: the editors close first, each asking to save.
  if (!CloseEditors(false))
  {
    Event->ignore();
    return;
  }
  // A standalone operation's window keeps nothing (WinSCP never shows it).
  if (!FStandalone)
  {
    SaveLayout();
  }
  // As FormClose: the workspace saved automatically, when that is on (an
  // empty one is not), and noted as the last stored, so it opens next time.
  CoreBridge::TWorkspaceSettings Workspace = CoreBridge::WorkspaceSettings();
  if (Workspace.AutoSave && (TabCount() > 0) && !FStandalone)
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

bool MainWindow::CommandLineFromAnotherInstance(const QStringList & Arguments)
{
  // As CanCommandLineFromAnotherInstance (the tray's hidden window counts
  // as shown, as WinSCP's).
  if ((!isVisible() && !((FTrayIcon != nullptr) && FTrayIcon->isVisible())) || FBusy || (QApplication::activeModalWidget() != nullptr))
  {
    return false;
  }
  // After answering: the other instance waits for no session to open.
  QTimer::singleShot(0, this, [this, Arguments]()
  {
    RestoreFromTray();
    if (isMinimized())
    {
      showNormal();
    }
    raise();
    activateWindow();
    std::vector<std::string> Strings;
    for (const QString & Argument : Arguments)
    {
      Strings.push_back(Argument.toStdString());
    }
    bool CanOpen = false;
    std::unique_ptr<CoreBridge::TSession> Session;
    try
    {
      Session = CoreBridge::TSession::FromCommandLine(CanOpen, &Strings);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return;
    }
    const QString Named = QString::fromStdString(CoreBridge::TSession::CommandLineFolderOrWorkspace(&Strings));
    if (!Session && !Named.isEmpty())
    {
      OpenWorkspace(Named);
    }
    else if (Session && CanOpen)
    {
      Open(std::move(Session));
    }
    else
    {
      SiteManagerDialog();
    }
  });
  return true;
}

void MainWindow::changeEvent(QEvent * Event)
{
  QMainWindow::changeEvent(Event);
  if ((Event->type() == QEvent::WindowStateChange) && isMinimized() && CoreBridge::Preferences().MinimizeToTray &&
      QSystemTrayIcon::isSystemTrayAvailable())
  {
    // Once the minimizing is done.
    QTimer::singleShot(0, this, &MainWindow::MinimizeToTray);
  }
}

// As ApplicationMinimize: the window and what is open over it.
void MainWindow::MinimizeApplication(bool ToTray)
{
  if ((ToTray || CoreBridge::Preferences().MinimizeToTray) && QSystemTrayIcon::isSystemTrayAvailable())
  {
    MinimizeToTray();
    return;
  }
  for (QWidget * Widget : QApplication::topLevelWidgets())
  {
    if (Widget->isVisible() && (Widget != this))
    {
      Widget->showMinimized();
    }
  }
  showMinimized();
}

void MainWindow::MinimizeToTray()
{
  if (!QSystemTrayIcon::isSystemTrayAvailable())
  {
    return;
  }
  if (FTrayIcon == nullptr)
  {
    FTrayIcon = new QSystemTrayIcon(this);
    connect(FTrayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason Reason)
    {
      if ((Reason == QSystemTrayIcon::Trigger) || (Reason == QSystemTrayIcon::DoubleClick))
      {
        RestoreFromTray();
      }
    });
  }
  FTrayIcon->setIcon(AppIcon());
  FTrayIcon->setToolTip(windowTitle());
  FTrayIcon->show();
  // The dialogs open over it go with it (Keep Up To Date's, minimized).
  FTrayHidden.clear();
  for (QWidget * Widget : QApplication::topLevelWidgets())
  {
    if (Widget->isVisible() && (Widget != this) && (qobject_cast<QMenu *>(Widget) == nullptr))
    {
      FTrayHidden.append(Widget);
      Widget->hide();
    }
  }
  hide();
}

void MainWindow::RestoreFromTray()
{
  if ((FTrayIcon == nullptr) || !FTrayIcon->isVisible())
  {
    return;
  }
  FTrayIcon->hide();
  setWindowState(windowState() & ~Qt::WindowMinimized);
  show();
  for (const QPointer<QWidget> & Widget : FTrayHidden)
  {
    if (Widget != nullptr)
    {
      Widget->setWindowState(Widget->windowState() & ~Qt::WindowMinimized);
      Widget->show();
    }
  }
  FTrayHidden.clear();
  raise();
  activateWindow();
}

void MainWindow::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    // As ShowExtendedException: the tray's balloon too, while there (and
    // balloons are on).
    if ((FTrayIcon != nullptr) && FTrayIcon->isVisible() && CoreBridge::Preferences().BalloonNotifications)
    {
      FTrayIcon->showMessage(windowTitle(), QString::fromStdString(E.Error.Message), QSystemTrayIcon::Critical);
    }
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

void MainWindow::RefreshRemotePanelIfDue()
{
  if (!FBusy && FSession && FSession->Active() && !LocalBrowser() && (QApplication::activeModalWidget() == nullptr) &&
      FRemoteLoaded.isValid() && (FRemoteLoaded.elapsed() > FRefreshRemoteInterval * 1000LL))
  {
    FRemoteLoaded.restart();
    FPanel->Refresh();
  }
}

bool MainWindow::WarnLackOfTempSpace(const QString & Path, std::int64_t RequiredSpace)
{
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  if (!Preferences.DDWarnLackOfTempSpace)
  {
    return true;
  }
  const QStorageInfo Storage(Path);
  const std::int64_t FreeSpace = Storage.bytesAvailable();
  // DDWarnLackOfTempSpaceRatio's reserve.
  if (!Storage.isValid() || (FreeSpace < 0) ||
      (static_cast<double>(FreeSpace) >= RequiredSpace * Preferences.DDWarnLackOfTempSpaceRatio))
  {
    return true;
  }
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Warning;
  QString Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::DDWarnLackOfTempSpace));
  // Its three %s: the directory, the free space, the size.
  for (const QString & Value : { QDir::toNativeSeparators(Path), FormatBytes(FreeSpace), FormatBytes(RequiredSpace) })
  {
    const int At = Message.indexOf(QStringLiteral("%s"));
    if (At >= 0)
    {
      Message.replace(At, 2, Value);
    }
  }
  Query.Message = Message.toStdString();
  CoreBridge::TQueryButton Yes;
  Yes.Answer = CoreBridge::AnswerYes;
  Yes.Caption = QMessageBox::tr("&Yes").toStdString();
  CoreBridge::TQueryButton No;
  No.Answer = CoreBridge::AnswerNo;
  No.Caption = QMessageBox::tr("&No").toStdString();
  Query.Buttons = { Yes, No };
  Query.CancelAnswer = CoreBridge::AnswerNo;
  Query.NeverAskAgainCheck = true;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
  if (Answer.NeverAskAgain && (Answer.Answer == CoreBridge::AnswerYes))
  {
    try
    {
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed) { Changed.DDWarnLackOfTempSpace = false; });
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  }
  return (Answer.Answer == CoreBridge::AnswerYes);
}

void MainWindow::OperationComplete(double Seconds)
{
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  if (Preferences.BeepOnFinish && (Seconds > Preferences.BeepOnFinishAfter))
  {
    QApplication::beep();
    FBeeps++;
  }
}

void MainWindow::HandleErrorList(CoreBridge::TSession * Session)
{
  if (Session != nullptr)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowErrorList(Session->EndErrorList());
  }
}

void MainWindow::FlashOnBackground()
{
  if (CoreBridge::Preferences().FlashTaskbar && !isActiveWindow())
  {
    QApplication::alert(this);
  }
}

void MainWindow::Notify(const QString & Message, bool Error, const std::function<void()> & OnClick)
{
  if (CoreBridge::Preferences().BalloonNotifications && QSystemTrayIcon::isSystemTrayAvailable())
  {
    // The tray's icon while there, else one for the balloon's time.
    QSystemTrayIcon * Icon = ((FTrayIcon != nullptr) && FTrayIcon->isVisible()) ? FTrayIcon : nullptr;
    if (Icon == nullptr)
    {
      if (FBalloonIcon == nullptr)
      {
        FBalloonIcon = new QSystemTrayIcon(this);
        QTimer * Hide = new QTimer(FBalloonIcon);
        Hide->setObjectName(QStringLiteral("BalloonHide"));
        Hide->setSingleShot(true);
        connect(Hide, &QTimer::timeout, FBalloonIcon, &QSystemTrayIcon::hide);
        connect(FBalloonIcon, &QSystemTrayIcon::activated, this, [this]() { activateWindow(); });
      }
      FBalloonIcon->setIcon(AppIcon());
      FBalloonIcon->setToolTip(windowTitle());
      FBalloonIcon->show();
      FBalloonIcon->findChild<QTimer *>(QStringLiteral("BalloonHide"))->start(30000);
      Icon = FBalloonIcon;
    }
    FBalloonClick = OnClick;
    disconnect(Icon, &QSystemTrayIcon::messageClicked, this, nullptr);
    connect(Icon, &QSystemTrayIcon::messageClicked, this, [this]()
    {
      const std::function<void()> Click = std::move(FBalloonClick);
      FBalloonClick = nullptr;
      if (Click)
      {
        Click();
      }
    });
    Icon->showMessage(windowTitle(), Message, Error ? QSystemTrayIcon::Critical : QSystemTrayIcon::Information);
  }
  else
  {
    FlashOnBackground();
    statusBar()->showMessage(QString(Message).replace(QLatin1Char('\n'), QLatin1Char(' ')), 30000);
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
  const QUrl Url = QUrl::fromLocalFile(ActiveLocalPath());
  return FUrlOpener ? FUrlOpener(Url) : QDesktopServices::openUrl(Url);
}

void MainWindow::StartUp()
{
  // /upload's files first: what is left of the command line names the
  // session.
  bool UseDefaults = false;
  std::vector<std::string> UploadFiles;
  try
  {
    UploadFiles = CoreBridge::CommandLineUpload(UseDefaults);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    close();
    return;
  }
  if (!UploadFiles.empty())
  {
    StandaloneUpload(UploadFiles, UseDefaults);
    return;
  }
  CoreBridge::TKeepUpToDateParams KeepParams;
  bool KeepUseDefaults = false;
  bool Keep = false;
  try
  {
    Keep = CoreBridge::CommandLineKeepUpToDate(KeepParams, KeepUseDefaults);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  if (Keep)
  {
    StandaloneKeepUpToDate(KeepParams, KeepUseDefaults);
    return;
  }
  // Preferences > Updates' periodic check, once the window is up; the jump
  // list shown again, as SetupInitialize has it.
  QTimer::singleShot(0, this, [this]()
  {
    CheckForUpdatesIfDue();
    Integration::UpdateJumpList();
  });
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

// As WinMain's standalone Upload (Explorer's "Send to" starts it): the
// session named, else the site manager's; the files to its directory, through
// the Copy dialog unless /defaults; then the program ends (WinSCP's
// OnceDoneOperation odoDisconnect, its window never shown for it).
void MainWindow::StandaloneUpload(const std::vector<std::string> & Files, bool UseDefaults)
{
  FStandalone = true;
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
  const bool Connected = (Session && CanOpen) ? Open(std::move(Session)) : SiteManagerDialog();
  if (Connected && FSession && FSession->Active())
  {
    QStringList Names;
    for (const std::string & File : Files)
    {
      Names.append(QDir::cleanPath(QDir::current().absoluteFilePath(QString::fromStdString(File))));
    }
    Transfer(true, Names, FPanel->Path(), false, !UseDefaults, false);
  }
  close();
}

// As WinMain's Synchronize (Start in New Window starts it): the session
// named, else the site manager's; Keep Up To Date with the directories
// given, started at once with /defaults; then the program ends.
void MainWindow::StandaloneKeepUpToDate(const CoreBridge::TKeepUpToDateParams & Params, bool UseDefaults)
{
  FStandalone = true;
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
  const bool Connected = (Session && CanOpen) ? Open(std::move(Session)) : SiteManagerDialog();
  if (Connected && FSession && FSession->Active())
  {
    KeepUpToDate(&Params, UseDefaults);
  }
  close();
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
    // As TTerminalManager's: the site first in the jump list (Windows).
    if (!FStandalone)
    {
      Integration::UpdateJumpList(CoreBridge::TJumpListEntry::Site, FSession->Name(), true);
    }
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
    ShowLocalBrowser(false);
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
    else
    {
      ShowLocalBrowser(false);
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
  QElapsedTimer Started;
  Started.start();
  FSession->BeginErrorList();
  try
  {
    Result = FSession->Upload(Paths, Target.toStdString(), Move, Settings);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  HandleErrorList(FSession.get());
  FBusy = false;
  // Read again by core, as Panels > Remote's "Automatically refresh
  // directory after operation" has it (AutoReadDirectoryAfterOp).
  FPanel->Reload();
  FLocal->Refresh();
  UpdateActions();
  OperationComplete(Started.elapsed() / 1000.0);
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
  QElapsedTimer Started;
  Started.start();
  FSession->BeginErrorList();
  try
  {
    Result = FSession->Download(List, QDir::toNativeSeparators(Target).toStdString(), Move, Settings);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  HandleErrorList(FSession.get());
  FBusy = false;
  // Read again by core, as Panels > Remote's "Automatically refresh
  // directory after operation" has it (AutoReadDirectoryAfterOp).
  FPanel->Reload();
  FLocal->Refresh();
  UpdateActions();
  OperationComplete(Started.elapsed() / 1000.0);
  return Result;
}

// Ctrl+Alt+T: the active side's tree (the two actions share it in WinSCP).
void MainWindow::UpdateTreeShortcut()
{
  if ((FLocalTreeAction != nullptr) && (FRemoteTreeAction != nullptr))
  {
    const QKeySequence Key(Qt::CTRL | Qt::ALT | Qt::Key_T);
    FLocalTreeAction->setShortcut(FLocalActive ? Key : QKeySequence());
    FRemoteTreeAction->setShortcut(FLocalActive ? QKeySequence() : Key);
  }
}

void MainWindow::ActivatePanel(bool Local)
{
  // The Explorer's one panel.
  FLocalActive = Local && !FExplorer;
  UpdateTreeShortcut();
  UpdateCommandLine();
  UpdateHotKeys();
  LocalPanel * Panel = SideLocal(Local);
  ((Panel != nullptr) ? Panel->FocusView() : FPanel->FocusView())->setFocus();
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
  // Remote thumbnails that arrived: drawn.
  if (FSession && FSession->ThumbnailsChanged())
  {
    FPanel->Thumbs()->viewport()->update();
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
  UpdateQueueView();
  CleanUpEdited();
  // As QueueListUpdate: once the active session's queue empties.
  if (FSession)
  {
    // Busy as last seen, or anything finished since (a short transfer can
    // come and go between two looks).
    const bool Busy = !FSession->QueueEmpty();
    const int Finished = FSession->QueueDoneCount();
    const bool Same = (FQueueSession == FSession.get());
    const bool Emptied = Same && (FQueueBusy || (Finished > FQueueFinished)) && !Busy;
    // As QueueOperationStart: when it got busy.
    if (Busy && !(Same && FQueueBusy))
    {
      FQueueStarted.start();
    }
    FQueueSession = FSession.get();
    FQueueBusy = Busy;
    FQueueFinished = Finished;
    if (Emptied)
    {
      // As QueueEvent's qeEmpty.
      OperationComplete(FQueueStarted.isValid() ? (FQueueStarted.elapsed() / 1000.0) : 0);
      QueueEmptied();
    }
  }
}

void MainWindow::UpdateCommandLine()
{
  if (FCommandLine == nullptr)
  {
    return;
  }
  const bool Remote = !FLocalActive && !LocalBrowser();
  FCommandPrompt->setText(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CommandLineLabel)) +
    (Remote ? QStringLiteral(" $") : QStringLiteral(" >")));
  FCommandLine->setEnabled(!FBusy && (!Remote || (FSession && FSession->Active())));
  // As CommandLinePopulate: the side's history, when the side changed.
  const int Side = Remote ? 1 : 0;
  if (Side != FCommandLineSide)
  {
    FCommandLineSide = Side;
    const QString Text = FCommandLine->currentText();
    FCommandLine->clear();
    for (const std::string & Command : CoreBridge::History(Remote ? "Commands" : "LocalCommands"))
    {
      FCommandLine->addItem(QString::fromStdString(Command));
    }
    FCommandLine->setEditText(Text);
  }
}

bool MainWindow::ExecuteCommandLine()
{
  const QString Command = FCommandLine->currentText();
  const bool Remote = !FLocalActive && !LocalBrowser();
  if (FBusy || Command.trimmed().isEmpty() || (Remote && (!FSession || !FSession->Active())))
  {
    return false;
  }
  try
  {
    CoreBridge::AddToHistory(Remote ? "Commands" : "LocalCommands", Command.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FCommandLineSide = -1;
  FCommandLine->clearEditText();
  UpdateCommandLine();
  FCommandLine->clearEditText();
  ActivatePanel(FLocalActive);
  if (Remote)
  {
    return OpenConsoleWith(Command);
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  try
  {
    CoreBridge::StartLocalCommand(Command.toStdString(),
      QDir::toNativeSeparators((Panel != nullptr) ? Panel->Path() : FLocal->Path()).toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    // As VK_RETURN's: back to the command line.
    FCommandLine->setEditText(Command);
    FCommandLine->setFocus();
    return false;
  }
  return true;
}

void MainWindow::InsertToCommandLine()
{
  LocalPanel * Panel = SideLocal(FLocalActive);
  QStringList Names = (Panel != nullptr) ? Panel->Selected() : FPanel->Selected();
  const CoreBridge::TRemoteEntry * Focused = ((Panel != nullptr) ? Panel->View() : FPanel->View())->CurrentEntry();
  if (Names.isEmpty() && (Focused != nullptr))
  {
    Names.append(QString::fromStdString(Focused->Name));
  }
  if (Names.isEmpty())
  {
    return;
  }
  // As PanelExport(peFileList) and PanelExportStore: names with spaces
  // quoted, each followed by a space.
  QString Text = FCommandLine->currentText();
  for (const QString & Name : Names)
  {
    Text += (Name.contains(QLatin1Char(' ')) ? (QLatin1Char('"') + Name + QLatin1Char('"')) : Name) + QLatin1Char(' ');
  }
  if (!FCommandLineBar->isVisible())
  {
    FCommandLineBar->setVisible(true);
    FCommandLineAction->setChecked(true);
    SaveToolbarSettings();
  }
  UpdateCommandLine();
  FCommandLine->setEditText(Text);
}

bool MainWindow::OpenConsoleWith(const QString & Command)
{
  if (FBusy || !FSession || !EnsureShellSession(false))
  {
    return false;
  }
  ConsoleDialog Dialog(FSession.get(), this);
  Dialog.show();
  const bool Result = Dialog.Execute(Command);
  Dialog.exec();
  FPanel->Refresh();
  UpdateTitle();
  return Result;
}

void MainWindow::NewDefaultTab(bool Reverse)
{
  if (CoreBridge::Layout().DefaultToNewRemoteTab != Reverse)
  {
    SiteManagerDialog();
  }
  else
  {
    NewLocalTab();
  }
}

// As CreateSessionColorMenu (ColorMenu.h).
void MainWindow::FillColorMenu()
{
  FillSessionColorMenu(FColorMenu, FSession ? FSession->Color() : 0, this, [this](int Color) { SetSessionColor(Color); });
}

void MainWindow::SetSessionColor(int Color)
{
  if (!FSession)
  {
    return;
  }
  FSession->SetColor(Color);
  UpdateTabs();
}

// As PanelColor (GetWindowColor): the remote panel's background the
// session's color, darkened under a dark palette.
void MainWindow::ApplySessionColor()
{
  const int Color = FSession ? FSession->Color() : 0;
  QPalette Palette = FPanel->View()->palette();
  QColor Base = QApplication::palette().color(QPalette::Base);
  if (Color != 0)
  {
    const bool Dark = (Base.lightness() < 128);
    Base = QColor(Color & 0xFF, (Color >> 8) & 0xFF, (Color >> 16) & 0xFF);
    if (Dark)
    {
      Base = Base.darker(300);
    }
  }
  Palette.setColor(QPalette::Base, Base);
  FPanel->View()->setPalette(Palette);
}

void MainWindow::SetOnceEmpty(TOnceEmpty Operation)
{
  FOnceEmpty = Operation;
  FOnceEmptyActions[static_cast<int>(Operation)]->setChecked(true);
}

void MainWindow::QueueEmptied()
{
  const TOnceEmpty Operation = FOnceEmpty;
  SetOnceEmpty(TOnceEmpty::Idle);
  if ((Operation == TOnceEmpty::Idle) || FBusy)
  {
    return;
  }
  // As TTerminal::CloseOnCompletion: the session closed.
  const QString Closed = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ClosedOnQueueEmpty));
  FQueueSession = nullptr;
  Disconnect();
  statusBar()->showMessage(Closed);
  const auto Power = [this](TOnceEmpty What)
  {
    QString Error;
    const bool Done = PowerAction ? PowerAction(What) :
      ((What == TOnceEmpty::Suspend) ? Integration::SuspendComputer(Error) : Integration::ShutDownComputer(Error));
    if (!Done && !Error.isEmpty())
    {
      QMessageBox::warning(this, windowTitle(), Error);
    }
    return Done;
  };
  if (Operation == TOnceEmpty::ShutDown)
  {
    // As TerminateApplication, then ShutDownWindows: no question asked.
    close();
    Power(TOnceEmpty::ShutDown);
    return;
  }
  if (Operation == TOnceEmpty::Suspend)
  {
    // Asleep first, so that the question waits for the user's return.
    Power(TOnceEmpty::Suspend);
  }
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  bool Exit = true;
  if (Layout.ConfirmExitOnCompletion)
  {
    // EXIT_ON_COMPLETION, or DISCONNECT_ON_COMPLETION with other sessions
    // open, after the message ("%s" its place).
    const int Others = static_cast<int>(FTabs.size());
    QString Question = (Others > 0) ?
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::DisconnectOnCompletion)).replace(QStringLiteral("%%s"), QStringLiteral("%s"))
        .replace(QStringLiteral("%d"), QString::number(Others)) :
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExitOnCompletion));
    Question.replace(QStringLiteral("%s"), Closed);
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Information;
    Query.Message = Question.toStdString();
    CoreBridge::TQueryButton Yes;
    Yes.Answer = CoreBridge::AnswerYes;
    Yes.Caption = QMessageBox::tr("&Yes").toStdString();
    CoreBridge::TQueryButton No;
    No.Answer = CoreBridge::AnswerNo;
    No.Caption = QMessageBox::tr("&No").toStdString();
    Query.Buttons = { Yes, No };
    Query.CancelAnswer = CoreBridge::AnswerNo;
    Query.NeverAskAgainCheck = true;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
    if (Answer.NeverAskAgain)
    {
      // As qaNeverAskAgain: yes, and not asked again.
      Layout.ConfirmExitOnCompletion = false;
      StoreLayoutChoice([](CoreBridge::TLayout & Stored) { Stored.ConfirmExitOnCompletion = false; });
    }
    else
    {
      Exit = (Answer.Answer == CoreBridge::AnswerYes);
    }
  }
  if (Exit)
  {
    close();
  }
}

bool MainWindow::TransferSelected(bool Move, bool Confirm, bool Background)
{
  if (LocalBrowser())
  {
    return LocalLocalCopy(Move, Confirm);
  }
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
  // Our own drag dropped here: no fake directory's target to wait for.
  if (FFakeDrag)
  {
    FFakeDrag->DroppedHere = true;
  }
  if (FBusy)
  {
    return false;
  }
  TFileDrag Drag;
  // A local tab: between its panels, local files copied or moved, as
  // WinSCP's local-local drag (LocalLocalCopy with DragDrop).
  if (LocalBrowser())
  {
    LocalPanel * Panel = SideLocal(OnLocal);
    if (!ReadFileDrag(Data, Drag) || !Drag.Local)
    {
      return false;
    }
    const QString Target = DropTarget(Panel->Path(), OnDirectory, false);
    QStringList Sources;
    for (const QString & Name : Drag.Names)
    {
      Sources.append(QDir(Drag.Directory).filePath(Name));
    }
    if (QDir(Drag.Directory) == QDir(Target))
    {
      return false;
    }
    if (QDir(Drag.Directory) == QDir(Panel->Path()))
    {
      // Within the panel, onto one of its directories: moved there.
      return OnDirectory.isEmpty() ? false : Panel->MoveInto(Drag.Names, OnDirectory);
    }
    const QString NativeTarget = QDir::toNativeSeparators(Target);
    return LocalLocalCopy(Move, Confirm, &Sources, &NativeTarget);
  }
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
    return Transfer(Drag.Local, Sources, Target, Move, Confirm, false, true);
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
  return Transfer(true, Sources, DropTarget(FPanel->Path(), OnDirectory, true), Move, Confirm, false, true);
}

// As WinSCP's Copy dialog: where to, editable, the transfer settings, and
// (Background given) whether through the queue. False when cancelled.
bool MainWindow::CopyDialog(bool FromLocal, const QStringList & Names, QString & Target, bool Move, bool * Background,
  CoreBridge::TTransferSettings & Settings, bool DragDrop)
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
  QLabel * PromptLabel = new QLabel(Prompt, &Dialog);
  PromptLabel->setBuddy(TargetEdit);
  Layout->addRow(PromptLabel);
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
  // As CopyParamDialog's coDoNotShowAgain: checked to begin with for a
  // drag while its confirmation is Auto (GetDoNotShowCopyDialogDefault).
  const bool DDAuto = (CoreBridge::Preferences().DDTransferConfirmation == CoreBridge::TPreferences::TAutoSwitch::Auto);
  QCheckBox * NeverShowAgain = new QCheckBox(tr("&Do not show this dialog box again"), &Dialog);
  NeverShowAgain->setObjectName(QStringLiteral("NeverShowAgain"));
  NeverShowAgain->setChecked(DragDrop && DDAuto);
  Layout->addRow(NeverShowAgain);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  // As the transfer settings menu's cplGenerateCode: this transfer, as a
  // script or .NET code (not for a temporary or remote transfer).
  if (FSession && FSession->Active())
  {
    QPushButton * Generate = Box->addButton(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamGenerateCode)),
      QDialogButtonBox::ActionRole);
    connect(Generate, &QPushButton::clicked, &Dialog, [this, &Dialog, FromLocal, &Names, TargetEdit, Move, SettingsBox]()
    {
      CoreBridge::TSession::TTransferCode Transfer;
      Transfer.ToRemote = FromLocal;
      Transfer.Move = Move;
      for (const QString & Name : Names)
      {
        Transfer.Paths.push_back(FromLocal ? QDir::toNativeSeparators(QDir(FLocal->Path()).filePath(Name)).toStdString() : Name.toStdString());
      }
      Transfer.Target = (FromLocal ? TargetEdit->text().trimmed() : QDir::toNativeSeparators(TargetEdit->text().trimmed())).toStdString();
      Transfer.Settings = SettingsBox->Settings();
      GenerateUrlDialog Code(FSession.get(), {}, &Dialog, &Transfer);
      Code.exec();
    });
  }
  Layout->addRow(Box);
  if ((Dialog.exec() != QDialog::Accepted) || TargetEdit->text().trimmed().isEmpty())
  {
    return false;
  }
  Target = TargetEdit->text().trimmed();
  // As the Explorer's CopyParamDialog: a download's target, the next one's.
  if (FExplorer && !FromLocal && QDir(QDir::fromNativeSeparators(Target)).exists())
  {
    const std::string Directory = QDir::toNativeSeparators(Target).toStdString();
    StoreLayoutChoice([Directory](CoreBridge::TLayout & Stored) { Stored.Explorer.LastLocalTargetDirectory = Directory; });
    FLocal->ChangeDirectory(QDir::fromNativeSeparators(Target));
  }
  if (Background != nullptr)
  {
    *Background = InBackground->isChecked();
  }
  // As HandleDoNotShowCopyDialogAgain: a drag's confirmation off (noted,
  // with a click undoing it, the first time), else the transfers'; left
  // unchecked, a drag's asked again from now on.
  try
  {
    if (NeverShowAgain->isChecked())
    {
      if (DragDrop && DDAuto)
      {
        Notify(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::DDTransferConfirmOff)), false, []()
        {
          CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed)
          {
            Changed.DDTransferConfirmation = CoreBridge::TPreferences::TAutoSwitch::On;
          });
        });
      }
      CoreBridge::ChangePreferences([DragDrop](CoreBridge::TPreferences & Changed)
      {
        if (DragDrop)
        {
          Changed.DDTransferConfirmation = CoreBridge::TPreferences::TAutoSwitch::Off;
        }
        else
        {
          Changed.ConfirmTransferring = false;
        }
      });
    }
    else if (DragDrop && DDAuto)
    {
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed)
      {
        Changed.DDTransferConfirmation = CoreBridge::TPreferences::TAutoSwitch::On;
      });
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
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

bool MainWindow::Transfer(bool FromLocal, const QStringList & Names, QString Target, bool Move, bool Confirm, bool Background,
  bool DragDrop)
{
  if (Names.isEmpty() || !FSession || !FSession->Active() || FBusy)
  {
    return false;
  }
  // As CopyParamDialog: Environment's "Transferring of files", and for a
  // drag its own (off: not asked).
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  Confirm = Confirm && Preferences.ConfirmTransferring &&
    !(DragDrop && (Preferences.DDTransferConfirmation == CoreBridge::TPreferences::TAutoSwitch::Off));
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (Confirm && !CopyDialog(FromLocal, Names, Target, Move, &Background, Settings, DragDrop))
  {
    return false;
  }
  // As DoCopyParamDialog's SaveSelectedNames, for Restore Selection.
  (FromLocal ? FLocal->View() : FPanel->View())->SaveSelectedNames();
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
  // As FullSynchronizeDirectories: the options kept (GUIConfiguration's),
  // the direction kept too or the active panel's.
  const CoreBridge::TSyncDefaults Defaults = CoreBridge::SyncDefaults();
  CoreBridge::TSyncParams Params = Defaults.Params;
  if (!Defaults.SaveMode)
  {
    Params.Direction = FLocalActive ? CoreBridge::TSyncParams::TDirection::Remote : CoreBridge::TSyncParams::TDirection::Local;
  }
  // GetSynchronizeOptions: the panels' selection, by name.
  for (const QStringList & Names : { FPanel->Selected(), FLocal->Selected() })
  {
    for (const QString & Name : Names)
    {
      Params.Selected.push_back(Name.toStdString());
    }
  }
  CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
  if (Confirm)
  {
    SynchronizeDialog Dialog(LocalDirectory, RemoteDirectory, Params, FSession->CanSyncTimestamps(), this,
      FSession->CanSyncChecksum(), !Params.Selected.empty(), Defaults.SaveMode);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    LocalDirectory = Dialog.LocalDirectory();
    RemoteDirectory = Dialog.RemoteDirectory();
    const std::vector<std::string> Selected = Params.Selected;
    Params = Dialog.Params();
    Params.Selected = Selected;
    if (Dialog.SaveSettings())
    {
      try
      {
        CoreBridge::SetSyncDefaults(Params, Dialog.SaveMode());
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
      }
    }
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

// What runs a local custom command's programs: on Windows the command line
// as CreateProcess takes it (ExecuteShellCheckedAndWait,
// ExecuteProcessAndReadOutput), elsewhere through the shell. Waited for with
// the window's events going on.
CoreBridge::TSession::TLocalRunner MainWindow::LocalRunner()
{
  CoreBridge::TSession::TLocalRunner Result;
  Result.Run = [](const std::string & CommandLine, bool Wait, std::string * Output, int & ExitCode)
  {
    const QString Line = QString::fromStdString(CommandLine).trimmed();
    QProcess Process;
#if defined(Q_OS_WIN)
    // The program, quoted or up to the first space; the rest as it is.
    QString Program;
    QString Arguments;
    if (Line.startsWith(QLatin1Char('"')))
    {
      const int End = Line.indexOf(QLatin1Char('"'), 1);
      Program = (End > 0) ? Line.mid(1, End - 1) : Line.mid(1);
      Arguments = (End > 0) ? Line.mid(End + 1).trimmed() : QString();
    }
    else
    {
      const int Space = Line.indexOf(QLatin1Char(' '));
      Program = (Space > 0) ? Line.left(Space) : Line;
      Arguments = (Space > 0) ? Line.mid(Space + 1).trimmed() : QString();
    }
    Process.setProgram(Program);
    Process.setNativeArguments(Arguments);
#else
    Process.setProgram(QStringLiteral("/bin/sh"));
    Process.setArguments({ QStringLiteral("-c"), Line });
#endif
    if (!Wait)
    {
      return Process.startDetached();
    }
    if (Output != nullptr)
    {
      Process.setProcessChannelMode(QProcess::MergedChannels);
    }
    Process.start();
    if (!Process.waitForStarted(-1))
    {
      return false;
    }
    QEventLoop Loop;
    QObject::connect(&Process, &QProcess::finished, &Loop, &QEventLoop::quit);
    if (Process.state() != QProcess::NotRunning)
    {
      Loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    if (Output != nullptr)
    {
      *Output = QString::fromLocal8Bit(Process.readAll()).toStdString();
    }
    ExitCode = (Process.exitStatus() == QProcess::NormalExit) ? Process.exitCode() : -1;
    return true;
  };
  return Result;
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
bool MainWindow::KeepUpToDate(const CoreBridge::TKeepUpToDateParams * Given, bool AutoStart)
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  CoreBridge::TKeepUpToDateParams Params = (Given != nullptr) ? *Given : CoreBridge::KeepUpToDateDefaults();
  // As SynchronizeDirectories: what is not given, the panels'.
  if (Params.LocalDirectory.empty())
  {
    Params.LocalDirectory = QDir::toNativeSeparators(FLocal->Path()).toStdString();
  }
  if (Params.RemoteDirectory.empty())
  {
    Params.RemoteDirectory = FPanel->Path().toStdString();
  }
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
    Dialog.OnMinimize = [this](bool ToTray) { MinimizeApplication(ToTray); };
    // Not from a run that is itself standalone (IsMainFormLike).
    if (!FStandalone)
    {
      Dialog.SetStartInNewWindow([this](const CoreBridge::TKeepUpToDateParams & Started, const CoreBridge::TTransferSettings & Settings)
      {
        try
        {
          const std::vector<std::string> Arguments = FSession->KeepUpToDateInNewWindow(Started, Settings);
          QStringList List;
          for (const std::string & Argument : Arguments)
          {
            List.append(QString::fromStdString(Argument));
          }
          return FNewInstanceStarter ? FNewInstanceStarter(List) : QProcess::startDetached(QCoreApplication::applicationFilePath(), List);
        }
        catch (const CoreBridge::TCoreError & E)
        {
          ShowError(E);
          return false;
        }
      });
    }
    FKeepUpToDate = &Dialog;
    // /defaults: started at once (DoSynchronizeDirectories' UseDefaults).
    if (AutoStart)
    {
      QTimer::singleShot(0, &Dialog, [&Dialog]() { Dialog.Start(); });
    }
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
  // By checksum where only the secondary shell calculates them: it first.
  if (Params.ByChecksum && !EnsureCommandSession(FSession->CanCalculateChecksumsNow()))
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  std::unique_ptr<CoreBridge::TSyncChecklist> Checklist;
  QElapsedTimer Collecting;
  Collecting.start();
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
  const double CollectSeconds = Collecting.elapsed() / 1000.0;
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
    SyncChecklistDialog Dialog(*Checklist, LocalDirectory, RemoteDirectory, this, FSession.get());
    // As BothCustomCommand: each pair on its own.
    Dialog.SetCustomCommandRunner([this, &Dialog](const CoreBridge::TCustomCommandDefinition & Command,
      const std::vector<std::pair<std::string, std::string>> & Pairs)
    {
      for (const auto & Pair : Pairs)
      {
        try
        {
          CoreBridge::TSession::RunLocalCustomCommand(FSession.get(), Command, { Pair.second }, { Pair.first }, false,
            QDir::toNativeSeparators(FLocal->Path()).toStdString(), CommandPrompts(), LocalRunner());
        }
        catch (const CoreBridge::TCoreError & E)
        {
          QtFrontend Frontend;
          Frontend.SetWindow(&Dialog);
          Frontend.ShowError(E.Error);
          return;
        }
      }
    });
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
  CoreBridge::TSyncStatistics Statistics;
  QElapsedTimer Applying;
  Applying.start();
  FSession->BeginErrorList();
  try
  {
    if (Queue)
    {
      FSession->SyncQueue(*Checklist);
      Result = true;
    }
    else
    {
      Result = FSession->SyncApply(*Checklist, &Statistics);
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  HandleErrorList(FSession.get());
  FBusy = false;
  FPanel->Reload();
  FLocal->Refresh();
  UpdateActions();
  if (Queue)
  {
    QueueIdle();
  }
  else if (Result)
  {
    OperationComplete(Applying.elapsed() / 1000.0 + CollectSeconds);
    // As Synchronize: Environment's "Synchronization summary".
    if (CoreBridge::Preferences().SynchronizeSummary)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Information;
      Query.Message = CoreBridge::SyncSummary(Statistics, Params.Mode == CoreBridge::TSyncParams::TMode::Timestamps,
        CollectSeconds, Applying.elapsed() / 1000.0,
        [](std::int64_t Bytes) { return FormatBytes(Bytes).toStdString(); });
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = QMessageBox::tr("OK").toStdString();
      Query.Buttons = { OK };
      Query.CancelAnswer = CoreBridge::AnswerOK;
      Query.NeverAskAgainCheck = true;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).NeverAskAgain)
      {
        try
        {
          CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed) { Changed.SynchronizeSummary = false; });
        }
        catch (const CoreBridge::TCoreError & E)
        {
          ShowError(E);
        }
      }
    }
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
  FPropertiesAction->setEnabled(!FBusy && (FSession || (SideLocal(FLocalActive) != nullptr)));
  UpdateTreeShortcut();
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
  FCopyAction->setEnabled(!FBusy && (FSession || LocalBrowser()));
  FMoveAction->setEnabled(!FBusy && (FSession || LocalBrowser()));
  FSynchronizeAction->setEnabled(!FBusy && FSession);
  FKeepUpToDateAction->setEnabled(!FBusy && FSession);
  FDisconnectAction->setEnabled(!FBusy && FSession);
  FDuplicateTabAction->setEnabled(!FBusy && FSession);
  if (FColorMenu != nullptr)
  {
    FColorMenu->menuAction()->setEnabled(!FBusy && FSession);
  }
  UpdateCommandLine();
  UpdateHotKeys();
  if (FRenameTabAction != nullptr)
  {
    const bool Active = FSession && FSession->Active();
    FRenameTabAction->setEnabled(!FBusy && (FActiveTab >= 0));
    FReconnectAction->setEnabled(!FBusy && FSession && !Active);
    FSaveSiteAction->setEnabled(!FBusy && FSession);
    FChangePasswordAction->setEnabled(!FBusy && Active && FSession->CanChangePassword());
    FPublicKeyAction->setEnabled(!FBusy && Active && FSession->CanUploadPublicKey());
    // (As the menu opens, too: the caches fill as the panel is browsed.)
    FClearCachesAction->setEnabled(!FBusy && Active && !FSession->CachesEmpty());
    FCompareAction->setEnabled(!FBusy && (Active || LocalBrowser()));
    FSyncBrowseAction->setEnabled(!FBusy && Active);
    FDirectorySizesAction->setEnabled(!FBusy && (Active || SideLocal(FLocalActive)));
    FNewFileAction->setEnabled(!FBusy && (Active || SideLocal(FLocalActive)));
    FCopyClipboardAction->setEnabled(!FBusy && (Active || SideLocal(FLocalActive)));
    FPasteAction->setEnabled(!FBusy);
    UpdateQueueView();
  }
  UpdateSortActions();
  if (FProcessQueueAction != nullptr)
  {
    FProcessQueueAction->setEnabled(FSession && FSession->Active());
    FProcessQueueAction->setChecked(FSession && FSession->QueueEnabled());
  }
  // The left Files band's: upload, or in a local tab copy.
  if (FLeftCopyAction != nullptr)
  {
    FLeftCopyAction->setText(LocalBrowser() ? tr("&Copy...") : tr("Up&load..."));
    FLeftMoveAction->setText(LocalBrowser() ? tr("&Move...") : tr("Upload and Dele&te..."));
  }
  FSaveWorkspaceAction->setEnabled(!FBusy && (TabCount() > 0));
  FCloseTabAction->setEnabled(!FBusy && FSession);
  FNextTabAction->setEnabled(!FBusy && (TabCount() > 1));
  FPreviousTabAction->setEnabled(!FBusy && (TabCount() > 1));
}

void MainWindow::OpenWebPage(const QString & Url)
{
  const QUrl Address(Url);
  if (!(FUrlOpener ? FUrlOpener(Address) : QDesktopServices::openUrl(Address)))
  {
    QMessageBox::warning(this, windowTitle(), tr("Cannot open %1.").arg(Url));
  }
}

int MainWindow::CompareVersions(const QString & A, const QString & B)
{
  const auto Numbers = [](QString Version)
  {
    if (Version.startsWith(QLatin1Char('v')) || Version.startsWith(QLatin1Char('V')))
    {
      Version = Version.mid(1);
    }
    Version = Version.section(QLatin1Char('-'), 0, 0);
    std::vector<int> Result;
    for (const QString & Part : Version.split(QLatin1Char('.')))
    {
      Result.push_back(Part.toInt());
    }
    return Result;
  };
  const std::vector<int> X = Numbers(A);
  const std::vector<int> Y = Numbers(B);
  for (std::size_t Index = 0; Index < std::max(X.size(), Y.size()); Index++)
  {
    const int Left = (Index < X.size()) ? X[Index] : 0;
    const int Right = (Index < Y.size()) ? Y[Index] : 0;
    if (Left != Right)
    {
      return (Left < Right) ? -1 : 1;
    }
  }
  return 0;
}

double MainWindow::NowAsDateTime()
{
  // A TDateTime counts local days from 1899-12-30; 1970 is day 25569.
  const QDateTime Now = QDateTime::currentDateTime();
  return (Now.toMSecsSinceEpoch() + Now.offsetFromUtc() * 1000.0) / 86400000.0 + 25569.0;
}

bool MainWindow::CheckForUpdatesIfDue()
{
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  if ((Preferences.UpdatesPeriod <= 0) || FStandalone ||
      (NowAsDateTime() - Preferences.UpdatesLastCheck < Preferences.UpdatesPeriod))
  {
    return false;
  }
  CheckForUpdates(true);
  return true;
}

void MainWindow::CheckForUpdates(bool Automatic)
{
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  const QString Current = QString::fromStdString(FInfo.Version);
  // Beta versions: GitHub's pre-releases. "latest" is the newest release
  // that is not one.
  const bool Beta = (Preferences.UpdatesBetaVersions == CoreBridge::TPreferences::TAutoSwitch::On) ||
    ((Preferences.UpdatesBetaVersions == CoreBridge::TPreferences::TAutoSwitch::Auto) &&
     (CompareVersions(Current, QStringLiteral("1.0.0")) < 0));
  const QUrl Url = !UpdatesUrl.isEmpty() ? UpdatesUrl :
    QUrl(Beta ? QStringLiteral("https://api.github.com/repos/99ecarvalho/linscp/releases?per_page=1") :
      QStringLiteral("https://api.github.com/repos/99ecarvalho/linscp/releases/latest"));
  QNetworkAccessManager * Manager = new QNetworkAccessManager(this);
  // Updates' connection (TConnectionType): no proxy, the system's, or the one given.
  if (Preferences.UpdatesConnectionType == 2)
  {
    Manager->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, QString::fromStdString(Preferences.UpdatesProxyHost),
      static_cast<quint16>(Preferences.UpdatesProxyPort)));
  }
  else if (Preferences.UpdatesConnectionType == 1)
  {
    const QList<QNetworkProxy> System = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(Url));
    Manager->setProxy(System.isEmpty() ? QNetworkProxy(QNetworkProxy::NoProxy) : System.first());
  }
  else
  {
    Manager->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
  }
  QNetworkRequest Request(Url);
  Request.setRawHeader("Accept", "application/vnd.github+json");
  Request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("linscp/%1").arg(Current));
  QNetworkReply * Reply = Manager->get(Request);
  if (!Automatic)
  {
    QApplication::setOverrideCursor(Qt::BusyCursor);
  }
  connect(Reply, &QNetworkReply::finished, this, [this, Reply, Manager, Automatic, Current, Preferences]()
  {
    if (!Automatic)
    {
      QApplication::restoreOverrideCursor();
    }
    QString Latest;
    QString Page;
    QString Error;
    if (Reply->error() != QNetworkReply::NoError)
    {
      Error = Reply->errorString();
    }
    else
    {
      // The releases API's list, newest first, or "latest"'s one.
      const QJsonDocument Document = QJsonDocument::fromJson(Reply->readAll());
      const QJsonArray Releases = Document.isArray() ? Document.array() : QJsonArray({ Document.object() });
      const QJsonObject Release = Releases.isEmpty() ? QJsonObject() : Releases.first().toObject();
      Latest = Release.value(QStringLiteral("tag_name")).toString();
      Page = Release.value(QStringLiteral("html_url")).toString();
      if (Latest.isEmpty())
      {
        Error = tr("No release information found.");
      }
    }
    Reply->deleteLater();
    Manager->deleteLater();
    // As TUpdatesConfiguration's LastCheck: checked, whatever it said.
    try
    {
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & Changed) { Changed.UpdatesLastCheck = NowAsDateTime(); });
    }
    catch (const CoreBridge::TCoreError &)
    {
      // Checked again next time.
    }
    const bool Newer = !Latest.isEmpty() && (CompareVersions(Latest, Current) > 0);
    if (OnUpdatesChecked)
    {
      OnUpdatesChecked(Latest, Newer, Error);
      return;
    }
    const QString ReleasePage = Page.isEmpty() ? QStringLiteral("https://github.com/99ecarvalho/linscp/releases") : Page;
    const QString Available = tr("Version %1 of linscp is available. You are using version %2.").arg(Latest, Current);
    if (Automatic && !Newer)
    {
      return;
    }
    if (Automatic && !Preferences.UpdatesShowOnStartup)
    {
      Notify(Available, false, [this, ReleasePage]() { OpenWebPage(ReleasePage); });
    }
    else if (!Error.isEmpty())
    {
      QMessageBox::warning(this, tr("Check for Updates"), tr("Could not check for updates: %1").arg(Error));
    }
    else if (Newer)
    {
      QMessageBox Box(QMessageBox::Information, tr("Check for Updates"), Available, QMessageBox::NoButton, this);
      QPushButton * Download = Box.addButton(tr("&Download Page"), QMessageBox::AcceptRole);
      Box.addButton(QMessageBox::Close);
      Box.exec();
      if (Box.clickedButton() == Download)
      {
        OpenWebPage(ReleasePage);
      }
    }
    else
    {
      QMessageBox::information(this, tr("Check for Updates"),
        tr("You are using the latest version of linscp (%1).").arg(Current));
    }
  });
}

void MainWindow::About()
{
  AboutDialog Dialog(FInfo, this);
  Dialog.exec();
}

// The active panel's focused file: files only, as WinSCP's.
bool MainWindow::CurrentFile(bool & Local, QString & Name, std::int64_t & Size, double & ModifiedTime) const
{
  LocalPanel * Panel = SideLocal(FLocalActive);
  const FileView * View = (Panel != nullptr) ? Panel->View() : FPanel->View();
  const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
  if ((Entry == nullptr) || Entry->Directory || ((Panel == nullptr) && !FSession))
  {
    return false;
  }
  Local = (Panel != nullptr);
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
  return Local ? EditLocal(QDir(ActiveLocalPath()).filePath(Name)) : EditRemote(Name);
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
    return (Local ? EditLocal(QDir(ActiveLocalPath()).filePath(Name)) : EditRemote(Name)) != nullptr;
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
  // The panel as a local one (the right one in a local tab too).
  LocalPanel * Side = SideLocal(Local);
  const bool IsLocal = (Side != nullptr);
  QMenu * Menu = new QMenu(this);
  Menu->setAttribute(Qt::WA_DeleteOnClose, Show);
  if (OnItem)
  {
    const FileView * View = IsLocal ? Side->View() : FPanel->View();
    const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
    // CurrentAddEditLinkContextAction: a link's own.
    if (!IsLocal && (Entry != nullptr) && Entry->Link)
    {
      Menu->addAction(FLinkAction);
    }
    Menu->addAction(tr("&Open"), this, [this]() { OpenCurrent(); });
    Menu->addAction(FEditAction);
    Menu->addMenu(FEditWithMenu);
    Menu->addSeparator();
    Menu->addAction(FCopyAction);
    Menu->addAction(FMoveAction);
    if (!IsLocal)
    {
      Menu->addAction(FDuplicateAction);
      Menu->addAction(FMoveToAction);
    }
    Menu->addAction(IsLocal ? Side->DeleteAction() : FPanel->DeleteAction());
    Menu->addAction(IsLocal ? Side->RenameAction() : FPanel->RenameAction());
    Menu->addSeparator();
    if (!IsLocal)
    {
      Menu->addMenu(FFileCommandsMenu);
    }
    QMenu * Names = Menu->addMenu(tr("&File Names"));
    Names->addAction(tr("&Copy to Clipboard"), this, [this, Local]() { CopyFileNames(Local, false); });
    Names->addAction(tr("Copy to Clipboard (Include &Paths)"), this, [this, Local]() { CopyFileNames(Local, true); });
    if (!IsLocal)
    {
      Names->addAction(FGenerateFileUrlAction);
    }
    Menu->addSeparator();
    Menu->addAction(FPropertiesAction);
    // LocalFilePopup's CurrentSystemMenuFocusedAction: the shell's menu
    // (Windows), where the click was.
    if (IsLocal && Integration::HasSystemContextMenu())
    {
      Menu->addAction(tr("&System Menu"), this, [this, Local, Global]() { SystemContextMenu(Local, Global); });
    }
  }
  else
  {
    QMenu * GoTo = Menu->addMenu(tr("&Go To"));
    GoTo->addAction(FOpenDirectoryAction);
    if (IsLocal)
    {
      GoTo->addAction(tr("&Explore Directory"), this, [this]() { ExploreLocalDirectory(); });
    }
    GoTo->addSeparator();
    for (QAction * Action : IsLocal ?
           std::initializer_list<QAction *>{ Side->ParentAction(), Side->RootAction(), Side->HomeAction() } :
           std::initializer_list<QAction *>{ FPanel->ParentAction(), FPanel->RootAction(), FPanel->HomeAction() })
    {
      GoTo->addAction(Action);
    }
    GoTo->addSeparator();
    GoTo->addAction(IsLocal ? Side->BackAction() : FPanel->BackAction());
    GoTo->addAction(IsLocal ? Side->ForwardAction() : FPanel->ForwardAction());
    Menu->addAction(IsLocal ? Side->RefreshAction() : FPanel->RefreshAction());
    Menu->addAction(FAddBookmarkAction);
    Menu->addAction(FFilterAction);
    Menu->addAction(tr("&Copy Path to Clipboard"), this, [this, Side, IsLocal]()
    {
      QApplication::clipboard()->setText(IsLocal ? QDir::toNativeSeparators(Side->Path()) : FPanel->Path());
    });
    Menu->addSeparator();
    QMenu * New = Menu->addMenu(tr("&New"));
    New->addAction(IsLocal ? Side->NewFolderAction() : FPanel->NewFolderAction());
    if (!IsLocal)
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

// As DisplaySystemContextMenu: the selected local files' shell menu; a
// rename chosen there done in place; the panel read again after.
bool MainWindow::SystemContextMenu(bool Local, const QPoint & Global)
{
  LocalPanel * Panel = SideLocal(Local);
  if ((Panel == nullptr) || !Integration::HasSystemContextMenu())
  {
    return false;
  }
  ActivatePanel(Local);
  QStringList Names = Panel->Selected();
  if (Names.isEmpty() && (Panel->View()->CurrentEntry() != nullptr))
  {
    Names.append(QString::fromStdString(Panel->View()->CurrentEntry()->Name));
  }
  QString Verb;
  if (!Integration::ShowSystemContextMenu(this, Panel->Path(), Names, Global, Verb))
  {
    return false;
  }
  if (Verb == QStringLiteral("rename"))
  {
    Panel->RenameAction()->trigger();
  }
  else
  {
    Panel->Refresh();
  }
  return true;
}

void MainWindow::CopyFileNames(bool Local, bool Paths)
{
  LocalPanel * Panel = SideLocal(Local);
  const QStringList Names = (Panel != nullptr) ? Panel->Selected() : FPanel->Selected();
  QStringList Lines;
  for (const QString & Name : Names)
  {
    if (!Paths)
    {
      Lines.append(Name);
    }
    else if (Panel != nullptr)
    {
      Lines.append(QDir::toNativeSeparators(QDir(Panel->Path()).filePath(Name)));
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
    LocalFile = QDir(ActiveLocalPath()).filePath(Name);
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
  // As the explorer's OnSaveAll: every editor's.
  Editor->OnSaveAll = [this]()
  {
    for (EditorWindow * Any : Editors())
    {
      Any->Save();
    }
  };
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

EditorWindow * MainWindow::EditRemote(const QString & Name, bool New)
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
    File = std::make_shared<CoreBridge::TEditedFile>(New ? FSession->EditNew(Name.toStdString()) :
      FSession->EditDownload(Name.toStdString()));
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
  // As ExecutedFileReload: from the server again, its session's.
  Editor->OnFileReload = [this, File, Session, Name]()
  {
    if (!SessionOpen(Session) || !Session->Active())
    {
      CoreBridge::TError Error;
      Error.Message = tr("Cannot reload '%1': its session is closed.").arg(Name).toStdString();
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(Error);
      return false;
    }
    try
    {
      Session->EditReload(*File);
      return true;
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return false;
    }
  };
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
  if (FBusy)
  {
    return false;
  }
  // A local panel's: the system's own Properties (Integration's).
  if (LocalPanel * Panel = SideLocal(FLocalActive))
  {
    QStringList Names = Panel->Selected();
    QStringList Paths;
    for (const QString & Name : Names)
    {
      Paths.append(QDir(Panel->Path()).filePath(Name));
    }
    if (Paths.isEmpty())
    {
      return false;
    }
    QString Error;
    if (!(FLocalPropertiesShower ? FLocalPropertiesShower(Paths) : Integration::ShowLocalProperties(Paths, Error)))
    {
      if (!Error.isEmpty())
      {
        CoreBridge::TError Failure;
        Failure.Message = Error.toStdString();
        QtFrontend Frontend;
        Frontend.SetWindow(this);
        Frontend.ShowError(Failure);
      }
      return false;
    }
    return true;
  }
  if (!FSession)
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
    // As ExecuteFileOperation's foSetProperties, for Restore Selection.
    FPanel->View()->SaveSelectedNames();
    ::PropertiesDialog Dialog(FSession.get(), List, FPanel->Path(), Info, this);
    // As CalculateChecksum's EnsureCommandSessionFallback(fcCalculatingChecksum).
    Dialog.SetChecksumSession([this]() { return EnsureCommandSession(FSession->CanCalculateChecksumsNow()); });
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

void MainWindow::UpdateHotKeys()
{
  if (FHotKeyCopy == nullptr)
  {
    return;
  }
  const bool Local = LocalBrowser();
  FHotKeyCopy->setText(Local ? tr("F5 Copy") : (FLocalActive ? tr("F5 Upload") : tr("F5 Download")));
  FHotKeyMove->setText(Local ? tr("F6 Move") : (FLocalActive ? tr("F6 Upload and Delete") : tr("F6 Download and Delete")));
  FHotKeyCopy->setEnabled(FCopyAction->isEnabled());
  FHotKeyMove->setEnabled(FMoveAction->isEnabled());
}

void MainWindow::UpdateFilesMenu()
{
  const bool Local = LocalBrowser();
  const bool Upload = FLocalActive;
  FCurrentCopyMenu->menuAction()->setVisible(!Local);
  FCurrentCopyMenu->setTitle(Upload ? tr("&Upload") : tr("Downlo&ad"));
  FCurrentCopyAction->setText(Upload ? tr("Up&load...") : tr("Down&load..."));
  FCurrentCopyQueueAction->setText(Upload ? tr("Upload in &Background...") : tr("Download in &Background..."));
  FCurrentMoveAction->setText(Upload ? tr("Upload and Dele&te...") : tr("Download and Dele&te..."));
  const bool Transfer = !FBusy && FSession && FSession->Active();
  for (QAction * Action : { FCurrentCopyAction, FCurrentCopyQueueAction, FCurrentMoveAction })
  {
    Action->setEnabled(Transfer);
  }
  FCopyAction->setVisible(Local);
  FMoveAction->setVisible(Local);
  const bool Lock = !FBusy && !FLocalActive && !Local && FSession && FSession->CanLock();
  FLockingMenu->setEnabled(Lock);
}

bool MainWindow::LockSelected(bool Lock)
{
  if (FBusy || !FSession || !FSession->CanLock() || FLocalActive || LocalBrowser())
  {
    return false;
  }
  QStringList Names = FPanel->Selected();
  if (Names.isEmpty() && (FPanel->View()->CurrentEntry() != nullptr))
  {
    Names.append(QString::fromStdString(FPanel->View()->CurrentEntry()->Name));
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
  try
  {
    if (Lock)
    {
      FSession->Lock(List);
    }
    else
    {
      FSession->Unlock(List);
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  FPanel->Refresh();
  return true;
}

bool MainWindow::AddEditLink(bool Add)
{
  // The local panel's (either, in a local tab), else the remote one's.
  LocalPanel * Panel = SideLocal(FLocalActive);
  const bool IsLocal = (Panel != nullptr);
  if (FBusy || (!IsLocal && (!FSession || !FSession->CanLink())))
  {
    return false;
  }
  const FileView * View = IsLocal ? Panel->View() : FPanel->View();
  const CoreBridge::TRemoteEntry * Entry = View->CurrentEntry();
  bool Edit = false;
  QString FileName;
  QString PointTo;
  if (Entry != nullptr)
  {
    const QString Name = QString::fromStdString(Entry->Name);
    Edit = !Add && Entry->Link && (IsLocal || FSession->CanEditLinks());
    if (Edit)
    {
      FileName = Name;
      PointTo = IsLocal ? QFileInfo(QDir(Panel->Path()).filePath(Name)).symLinkTarget() : QString::fromStdString(Entry->LinkTarget);
    }
    else
    {
      PointTo = Name;
    }
  }
  LinkDialog Dialog(FileName, PointTo, !IsLocal, Edit, !IsLocal && FSession->CanHardLink(), this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  bool Result = false;
  if (IsLocal)
  {
    const QString Path = QDir(Panel->Path()).filePath(Dialog.FileName());
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
    Panel->Refresh();
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
  LocalPanel * Panel = ActiveLocal();
  return Panel->ChangeDirectory(Directory) ||
    (QDir::cleanPath(QDir::fromNativeSeparators(Directory)) == QDir::cleanPath(Panel->Path()));
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
  const QString Directory = Local ? QDir::toNativeSeparators(ActiveLocalPath()) : FPanel->Path();
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
    OpenDirectoryDialog Dialog(!Local, Name, Local ? ActiveLocal()->History() : FPanel->History(),
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
  LocalPanel * Panel = SideLocal(FLocalActive);
  const bool IsLocal = (Panel != nullptr);
  QString Mask = IsLocal ? Panel->Filter() : FPanel->Filter();
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
  if (IsLocal)
  {
    Panel->SetFilter(Normalized);
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
  return EnsureCommandSession(Shell ? FSession->CanRunShellCommands() : FSession->CanRunCommands());
}

// As EnsureCommandSessionFallback: Capable, or the secondary shell opened
// (asked first, unless told not to).
bool MainWindow::EnsureCommandSession(bool Capable)
{
  if (Capable)
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
  // The custom commands, then the extensions, as CreateCustomCommandsMenu.
  std::vector<CoreBridge::TCustomCommandDefinition> Commands = CoreBridge::CustomCommands();
  for (const CoreBridge::TCustomCommandDefinition & Extension : CoreBridge::Extensions())
  {
    Commands.push_back(Extension);
  }
  for (const CoreBridge::TCustomCommandDefinition & Command : Commands)
  {
    const bool FileCommand = CoreBridge::CheckCustomCommand(Command.Command, !Command.Local).FileCommand;
    QAction * Action = (FileCommand ? FFileCommandsMenu : FStaticCommandsMenu)->addAction(QString::fromStdString(Command.Name));
    const QString ShortCut = ShortCutText(Command.ShortCut);
    if (!ShortCut.isEmpty())
    {
      Action->setShortcut(QKeySequence::fromString(ShortCut, QKeySequence::NativeText));
    }
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
  // The Custom Commands band: each command a button, as WinSCP's.
  if (FCustomCommandsBar != nullptr)
  {
    FCustomCommandsBar->clear();
    for (QMenu * Menu : { FFileCommandsMenu, FStaticCommandsMenu })
    {
      for (QAction * Action : Menu->actions())
      {
        if (!Action->isSeparator() && (Action->menu() == nullptr))
        {
          FCustomCommandsBar->addAction(Action);
        }
      }
    }
  }
}

// As CustomCommand's options part: an extension's options asked as it runs,
// kept for the next time. False when cancelled.
bool MainWindow::AskExtensionRunOptions(const CoreBridge::TCustomCommandDefinition & Command)
{
  try
  {
    const std::vector<CoreBridge::TExtensionOption> Options =
      CoreBridge::TSession::ExtensionRunOptions(LocalBrowser() ? nullptr : FSession.get(), Command,
        QDir::toNativeSeparators(FLocal->Path()).toStdString());
    ExtensionOptionsDialog Dialog(QString::fromStdString(Command.Name), Options, this, nullptr, {},
      QString::fromStdString(!Command.OptionsPage.empty() ? Command.OptionsPage : Command.HomePage));
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    CoreBridge::TSession::SetExtensionRunOptions(LocalBrowser() ? nullptr : FSession.get(), Command, Dialog.Values());
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

bool MainWindow::RunCustomCommand(const CoreBridge::TCustomCommandDefinition & Command)
{
  if (!Command.Id.empty() && Command.RunOptions && !FBusy && !AskExtensionRunOptions(Command))
  {
    return false;
  }
  if (Command.Local)
  {
    return RunLocalCustomCommand(Command);
  }
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
  // As ExecuteFileOperation's foCustomCommand, for Restore Selection.
  FPanel->View()->SaveSelectedNames();
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

// As TCustomScpExplorerForm::LocalCustomCommand: on the active panel's
// files (the remote ones downloaded for it), the local panel's for !^!.
bool MainWindow::RunLocalCustomCommand(const CoreBridge::TCustomCommandDefinition & Command)
{
  if (FBusy)
  {
    return false;
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  const bool LocalSide = (Panel != nullptr);
  if (!LocalSide && !(FSession && FSession->Active()))
  {
    return false;
  }
  LocalPanel * Files = LocalSide ? Panel : FLocal;
  std::vector<std::string> LocalFiles;
  for (const QString & Name : Files->Selected())
  {
    LocalFiles.push_back(QDir::toNativeSeparators(QDir(Files->Path()).filePath(Name)).toStdString());
  }
  std::vector<std::string> Names;
  if (!LocalSide)
  {
    QStringList Selected = FPanel->Selected();
    if (Selected.isEmpty() && (FPanel->View()->CurrentEntry() != nullptr))
    {
      Selected.append(QString::fromStdString(FPanel->View()->CurrentEntry()->Name));
    }
    for (const QString & Name : Selected)
    {
      Names.push_back(Name.toStdString());
    }
  }
  const CoreBridge::TCustomCommandCheck Check = CoreBridge::CheckCustomCommand(Command.Command, false);
  if (Check.FileCommand && (LocalSide ? LocalFiles.empty() : Names.empty()))
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  std::string Output;
  try
  {
    Output = CoreBridge::TSession::RunLocalCustomCommand(FSession.get(), Command, Names, LocalFiles, LocalSide,
      QDir::toNativeSeparators(FLocal->Path()).toStdString(), CommandPrompts(), LocalRunner());
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  if (FSession && FSession->Active())
  {
    FPanel->Refresh();
  }
  Files->Refresh();
  UpdateActions();
  if (Result)
  {
    const QString Text = QString::fromStdString(Output);
    // Even empty output to the clipboard, as WinSCP; no empty message box.
    if (Command.CopyResults)
    {
      QApplication::clipboard()->setText(Text);
    }
    if (Command.ShowResultsInMessageBox && !Text.isEmpty())
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Information;
      Query.Message = Text.toStdString();
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = "OK";
      CoreBridge::TQueryButton Copy;
      Copy.Answer = CoreBridge::AnswerRetry;
      Copy.Caption = "&Copy";
      Copy.OnSubmit = [Text]() { QApplication::clipboard()->setText(Text); return 0u; };
      Query.Buttons = { OK, Copy };
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
  if (Tab.LocalBrowser)
  {
    Tab.OtherLocalPath = FOtherLocal->Path();
  }
  Tab.QueueDone = FQueueDone;
  Tab.SynchronizeBrowsing = (FSyncBrowseAction != nullptr) && FSyncBrowseAction->isChecked();
  Tab.LocalState = FLocal->State();
  Tab.RemoteState = FPanel->State();
  Tab.OtherLocalState = FOtherLocal->State();
  FActiveTab = -1;
}

void MainWindow::ShowLocalBrowser(bool Local)
{
  const bool Focused = FPanel->isAncestorOf(focusWidget()) || FOtherLocal->isAncestorOf(focusWidget());
  if (FOtherLocal->isVisible() == Local)
  {
    return;
  }
  // The shown one takes the hidden one's width.
  QList<int> Sizes = FPanels->sizes();
  const int Right = Sizes.value(FPanels->indexOf(Local ? static_cast<QWidget *>(FPanel) : static_cast<QWidget *>(FOtherLocal)));
  FPanel->setVisible(!Local);
  FOtherLocal->setVisible(Local);
  Sizes = FPanels->sizes();
  Sizes[FPanels->indexOf(Local ? static_cast<QWidget *>(FOtherLocal) : static_cast<QWidget *>(FPanel))] = Right;
  FPanels->setSizes(Sizes);
  if (Focused)
  {
    ActivatePanel(false);
  }
}

bool MainWindow::LocalBrowser() const
{
  return (FActiveTab >= 0) && (FActiveTab < TabCount()) && FTabs[FActiveTab].LocalBrowser;
}

LocalPanel * MainWindow::SideLocal(bool Local) const
{
  return Local ? FLocal : (LocalBrowser() ? FOtherLocal : nullptr);
}

LocalPanel * MainWindow::ActiveLocal() const
{
  LocalPanel * Panel = SideLocal(FLocalActive);
  return (Panel != nullptr) ? Panel : FLocal;
}

QString MainWindow::ActiveLocalPath() const
{
  return ActiveLocal()->Path();
}

void MainWindow::NewLocalTab(const QString & Local, const QString & Other)
{
  // No local tabs in the Explorer.
  if (FBusy || FExplorer)
  {
    return;
  }
  ParkActiveTab();
  TTab Tab;
  Tab.LocalBrowser = true;
  FTabs.push_back(std::move(Tab));
  FActiveTab = TabCount() - 1;
  FPanel->SetSession(nullptr);
  FQueue->SetSession(nullptr);
  FQueueDone = 0;
  FOtherLocal->ApplyPreferences(CoreBridge::Preferences());
  if (!Local.isEmpty())
  {
    FLocal->ChangeDirectory(Local);
  }
  FOtherLocal->ChangeDirectory(Other.isEmpty() ? FLocal->Path() : Other);
  ShowLocalBrowser(true);
  UpdateTabs();
  UpdateTitle();
  UpdateActions();
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
  // As SessionChanged: the tab's local directory and state, unless Panels >
  // Local's "Do not change state when switching sessions" keeps the
  // panel's (a local tab's are always its own).
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  const bool LocalState = !Preferences.PreserveLocalDirectory || Tab.LocalBrowser;
  if (!Tab.LocalPath.isEmpty() && LocalState)
  {
    FLocal->ChangeDirectory(Tab.LocalPath);
  }
  if (Tab.LocalBrowser && !Tab.OtherLocalPath.isEmpty())
  {
    FOtherLocal->ChangeDirectory(Tab.OtherLocalPath);
  }
  ShowLocalBrowser(Tab.LocalBrowser);
  // As PreservePanelState: each panel's focus, selection and scroll, as the
  // tab left them.
  if (Preferences.PreservePanelState)
  {
    if (LocalState)
    {
      FLocal->RestoreState(Tab.LocalState);
    }
    FPanel->RestoreState(Tab.RemoteState);
    FOtherLocal->RestoreState(Tab.OtherLocalState);
  }
  // As the tab's StateData: its synchronized browsing, its directories as
  // loaded (the switch is no change of directory to follow).
  if (FSyncBrowseAction != nullptr)
  {
    FSyncBrowseAction->setChecked(Tab.SynchronizeBrowsing);
  }
  FPrevPath[1] = FLocal->Path();
  FPrevPath[0] = FPanel->Path();
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
  TransferPresetAutoSelect();
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
  // (A local tab has no remote editors of its own.)
  if (FSession && !CloseEditors(true, FSession.get()))
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
    QString Name = (Session != nullptr) ? QString::fromStdString(Session->Name()) : QString();
    // As GetLocalBrowserSessionTitle: the two directories' last parts.
    if (FTabs[Index].LocalBrowser && !FTabs[Index].Name.isEmpty())
    {
      Name = FTabs[Index].Name;
    }
    else if (FTabs[Index].LocalBrowser)
    {
      const bool Active = (Index == FActiveTab);
      Name = tr("%1 \u2013 %2").arg(ShortPath(Active ? FLocal->Path() : FTabs[Index].LocalPath),
        ShortPath(Active ? FOtherLocal->Path() : FTabs[Index].OtherLocalPath));
    }
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
  for (int Index = 0; Index < FTabBar->count(); Index++)
  {
    CoreBridge::TSession * Session = TabSession(Index);
    const int Color = (Session != nullptr) ? Session->Color() : 0;
    QIcon Swatch;
    if (Color != 0)
    {
      QPixmap Pixmap(12, 12);
      Pixmap.fill(QColor(Color & 0xFF, (Color >> 8) & 0xFF, (Color >> 16) & 0xFF));
      Swatch = QIcon(Pixmap);
    }
    FTabBar->setTabIcon(Index, Swatch);
  }
  FTabBar->setVisible(FTabsShown && (TabCount() > 0));
  FUpdatingTabs = false;
  ApplySessionColor();
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
    if (FTabs[Index].LocalBrowser)
    {
      Tab.OtherLocalDirectory =
        QDir::toNativeSeparators((Index == FActiveTab) ? FOtherLocal->Path() : FTabs[Index].OtherLocalPath).toStdString();
      Result.push_back(Tab);
    }
    else if (Tab.Session != nullptr)
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

bool MainWindow::SaveWorkspace(const QString * Name, bool SavePasswords, bool AutoSave, bool EnableAutoSave)
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
      SaveWorkspaceDialog Dialog(WorkspaceName(), CanSavePasswords, false, EnableAutoSave, this);
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
    if (Session->IsLocalBrowser())
    {
      NewLocalTab(QString::fromStdString(Session->LocalDirectory()), QString::fromStdString(Session->OtherLocalDirectory()));
      Any = true;
    }
    else
    {
      Any = Open(std::move(Session)) || Any;
    }
  }
  if (Any)
  {
    FLastWorkspace = Name;
    SwitchTab(First);
    // As the Login dialog's: the workspace first in the jump list.
    Integration::UpdateJumpList(CoreBridge::TJumpListEntry::Workspace, Name.toStdString(), true);
  }
  return Any;
}

// --- Menus --------------------------------------------------------------------

FileView * MainWindow::ActiveView() const
{
  LocalPanel * Panel = SideLocal(FLocalActive);
  return (Panel != nullptr) ? Panel->View() : FPanel->View();
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
// active one; filled as the menu opens, the right side being the remote
// panel or, in a local tab, the other local one.
QMenu * MainWindow::PanelMenu(bool Local)
{
  QMenu * Menu = menuBar()->addMenu(Local ? tr("&Local") : tr("&Right"));
  connect(Menu, &QMenu::aboutToShow, this, [this, Local, Menu]()
  {
    Menu->clear();
    LocalPanel * Side = SideLocal(Local);
    auto OnPanel = [this, Local](auto Function)
    {
      return [this, Local, Function]()
      {
        ActivatePanel(Local);
        Function();
      };
    };
    Menu->addAction(Local ? FChangeLocalPathAction : FChangeRemotePathAction);
    Menu->addSeparator();
    QMenu * GoTo = Menu->addMenu(tr("&Go To"));
    GoTo->addAction(IconSet::Icon(QStringLiteral("open-directory")), tr("&Open Directory/Bookmark..."), this,
      OnPanel([this]() { OpenDirectory(false); }));
    // As ...ExploreDirectoryAction: a local side's only.
    QAction * Explore = GoTo->addAction(FExploreAction->icon(), FExploreAction->text(), this,
      OnPanel([this]() { ExploreLocalDirectory(); }));
    Explore->setShortcut(FExploreAction->shortcut());
    Explore->setEnabled(Side != nullptr);
    GoTo->addSeparator();
    for (QAction * Action : (Side != nullptr) ?
           std::initializer_list<QAction *>{ Side->ParentAction(), Side->RootAction(), Side->HomeAction() } :
           std::initializer_list<QAction *>{ FPanel->ParentAction(), FPanel->RootAction(), FPanel->HomeAction() })
    {
      GoTo->addAction(Action);
    }
    // In a local tab only, both sides local and elsewhere.
    QAction * OtherPath = GoTo->addAction(tr("Path fro&m Other Panel"), this, [this, Local]() { PathFromOtherPanel(Local); });
    OtherPath->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Period));
    OtherPath->setEnabled(!FBusy && LocalBrowser() && (QDir(FLocal->Path()) != QDir(FOtherLocal->Path())));
    GoTo->addSeparator();
    GoTo->addAction((Side != nullptr) ? Side->BackAction() : FPanel->BackAction());
    GoTo->addAction((Side != nullptr) ? Side->ForwardAction() : FPanel->ForwardAction());
    Menu->addAction((Side != nullptr) ? Side->RefreshAction() : FPanel->RefreshAction());
    Menu->addAction(IconSet::Icon(QStringLiteral("bookmark")), tr("&Add Path to Bookmarks"), this,
      OnPanel([this]() { OpenDirectory(true); }));
    Menu->addAction(tr("Copy &Path to Clipboard"), this, [this, Side]()
    {
      QApplication::clipboard()->setText((Side != nullptr) ? QDir::toNativeSeparators(Side->Path()) : FPanel->Path());
    });
    Menu->addSeparator();
    // As ...ReportAction and ...ThumbnailAction: details or thumbnails.
    QMenu * ViewMenu = Menu->addMenu(tr("&View"));
    QActionGroup * ViewGroup = new QActionGroup(ViewMenu);
    const bool Thumbnails = (Side != nullptr) ? Side->Thumbnails() : FPanel->Thumbnails();
    QAction * Details = ViewMenu->addAction(tr("&Details"), this, [this, Local]() { SetPanelThumbnails(Local, false); });
    Details->setStatusTip(tr("View details"));
    ViewMenu->addSeparator();
    QAction * Thumbs = ViewMenu->addAction(tr("&Thumbnails"), this, [this, Local]() { SetPanelThumbnails(Local, true); });
    Thumbs->setStatusTip(tr("View thumbnails"));
    for (QAction * Choice : { Details, Thumbs })
    {
      Choice->setCheckable(true);
      ViewGroup->addAction(Choice);
    }
    (Thumbnails ? Thumbs : Details)->setChecked(true);
    // The view's sort and columns, as its header's.
    FileView * View = (Side != nullptr) ? Side->View() : FPanel->View();
    QMenu * Sort = View->SortMenu(Side != nullptr);
    Sort->setTitle(tr("&Sort"));
    Sort->setParent(Menu, Sort->windowFlags());
    Menu->addMenu(Sort);
    QMenu * Columns = View->ColumnsMenu(true);
    Columns->setTitle(tr("&Columns"));
    Columns->setParent(Menu, Columns->windowFlags());
    Menu->addMenu(Columns);
    Menu->addAction(IconSet::Icon(QStringLiteral("filter")), tr("&Filter..."), this, OnPanel([this]() { Filter(); }));
  });
  return Menu;
}

// As WinSCP's Commander's menu bar: Local, Mark, Files, Commands, Tabs,
// Options, Remote, Help, each in its order. What linscp does not have yet
// is left out, as are the actions WinSCP keeps to keys (Next and Previous
// Tab, Show Hidden Files, Ctrl+O and Ctrl+B on the active panel).
void MainWindow::BuildMenus()
{
  IconSet::SetCurrent(QString::fromStdString(CoreBridge::Preferences().IconSet));
  FLocalMenu = PanelMenu(true);

  QMenu * Mark = menuBar()->addMenu(tr("&Mark"));
  Mark->addAction(tr("&Select/Unselect"), this, [this]() { ActiveView()->ToggleCurrent(); });
  Iconize(Mark->addAction(tr("Sele&ct Files..."), this, [this]() { ActiveView()->SelectByMaskDialog(true); }), "select-files");
  Iconize(Mark->addAction(tr("&Unselect Files..."), this, [this]() { ActiveView()->SelectByMaskDialog(false); }), "unselect-files");
  Iconize(Mark->addAction(tr("Select &All"), this, [this]() { ActiveView()->selectAll(); }), "select-all");
  Mark->addSeparator();
  Iconize(Mark->addAction(tr("&Invert Selection"), this, [this]() { ActiveView()->InvertSelection(); }), "invert-selection");
  Iconize(Mark->addAction(tr("C&lear Selection"), this, [this]() { ActiveView()->clearSelection(); }), "clear-selection");
  QAction * RestoreSelection = Mark->addAction(tr("&Restore Selection"), this, [this]()
  {
    if (ActiveView()->SelectedNamesSaved())
    {
      ActiveView()->RestoreSelectedNames();
    }
  });
  RestoreSelection->setStatusTip(tr("Restore previous selection"));
  RestoreSelection->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  connect(Mark, &QMenu::aboutToShow, this, [this, RestoreSelection]()
  {
    RestoreSelection->setEnabled(ActiveView()->SelectedNamesSaved());
  });
  Mark->addSeparator();
  Mark->addAction(tr("Select Files with Same &Extension"), this, [this]() { SelectSameExtension(true); });
  Mark->addAction(tr("Unselect Files with Same E&xtension"), this, [this]() { SelectSameExtension(false); });

  // The active panel's.
  auto OnActive = [this](QAction * (LocalPanel::*LocalAction)() const, QAction * (RemotePanel::*RemoteAction)() const)
  {
    return [this, LocalAction, RemoteAction]()
    {
      LocalPanel * Panel = SideLocal(FLocalActive);
      ((Panel != nullptr) ? (Panel->*LocalAction)() : (FPanel->*RemoteAction)())->trigger();
    };
  };
  QMenu * Files = menuBar()->addMenu(tr("&Files"));
  QMenu * New = Files->addMenu(tr("&New"));
  FNewFileAction = New->addAction(tr("&File..."), this, [this]() { NewFile(); });
  Iconize(New->addAction(tr("&Directory..."), this, OnActive(&LocalPanel::NewFolderAction, &RemotePanel::NewFolderAction)), "new-folder");
  QAction * NewLink = New->addAction(tr("&Link..."), this, [this]() { AddEditLink(true); });
  NewLink->setStatusTip(tr("Create new link/shortcut"));
  connect(New, &QMenu::aboutToShow, this, [this, NewLink]() { NewLink->setEnabled(FLinkAction->isEnabled()); });
  Files->addSeparator();
  Files->addAction(FOpenCurrentAction);
  Files->addAction(FEditAction);
  Files->addMenu(FEditWithMenu);
  Files->addAction(FLinkAction);
  Files->addSeparator();
  // As CurrentCopyItem: the active panel's Upload or Download, its
  // background and moving variants; in a local tab, Copy and Move instead.
  FCurrentCopyMenu = Files->addMenu(tr("Downlo&ad"));
  FCurrentCopyAction = FCurrentCopyMenu->addAction(tr("Down&load..."), this, [this]() { TransferSelected(false, true, false); });
  FCurrentCopyQueueAction = FCurrentCopyMenu->addAction(tr("Download in &Background..."), this,
    [this]() { TransferSelected(false, true, true); });
  FCurrentCopyMenu->addSeparator();
  FCurrentMoveAction = FCurrentCopyMenu->addAction(tr("Download and Dele&te..."), this, [this]() { TransferSelected(true, true); });
  Files->addAction(FCopyAction);
  Files->addAction(FMoveAction);
  connect(Files, &QMenu::aboutToShow, this, &MainWindow::UpdateFilesMenu);
  Files->addAction(FDuplicateAction);
  Files->addAction(FMoveToAction);
  Iconize(Files->addAction(tr("&Delete"), this, OnActive(&LocalPanel::DeleteAction, &RemotePanel::DeleteAction)), "delete");
  Iconize(Files->addAction(tr("&Rename"), this, OnActive(&LocalPanel::RenameAction, &RemotePanel::RenameAction)), "rename");
  Files->addSeparator();
  FCopyClipboardAction = Files->addAction(tr("&Copy to Clipboard"), this, [this]() { CopyToClipboard(); });
  FCopyClipboardAction->setShortcut(QKeySequence::Copy);
  FPasteAction = Files->addAction(tr("&Paste from Clipboard"), this, [this]() { PasteFromClipboard(); });
  FPasteAction->setShortcut(QKeySequence::Paste);
  Files->addSeparator();
  Files->addMenu(FFileCommandsMenu);
  QMenu * Names = Files->addMenu(tr("&File Names"));
  QAction * InsertNames = Names->addAction(tr("Insert to Command &Line"), this, [this]() { InsertToCommandLine(); });
  InsertNames->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
  InsertNames->setStatusTip(tr("Insert name(s) of selected file(s) to command line"));
  Names->addAction(tr("Copy to &Clipboard"), this, [this]() { CopyFileNames(FLocalActive, false); });
  Names->addAction(tr("Copy to Clipboard (Include &Paths)"), this, [this]() { CopyFileNames(FLocalActive, true); });
  Names->addAction(FGenerateFileUrlAction);
  // As LockAction and UnlockAction: where the protocol locks (WebDAV).
  FLockingMenu = Files->addMenu(tr("Loc&king"));
  FLockingMenu->addAction(tr("&Lock"), this, [this]() { LockSelected(true); })->setStatusTip(tr("Lock selected file(s)"));
  FLockingMenu->addAction(tr("&Unlock"), this, [this]() { LockSelected(false); })->setStatusTip(tr("Unlock selected file(s)"));
  Files->addSeparator();
  Files->addAction(FPropertiesAction);
  FDirectorySizesAction = Files->addAction(tr("&Calculate Directory Sizes"), this, [this]() { CalculateDirectorySizes(); });
  FDirectorySizesAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::ALT | Qt::Key_Return));

  QMenu * Commands = menuBar()->addMenu(tr("&Commands"));
  FCompareAction = Commands->addAction(tr("&Compare Directories"), this, [this]() { CompareDirectories(); });
  FCompareAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F2));
  Commands->addAction(FKeepUpToDateAction);
  Commands->addAction(FSynchronizeAction);
  FSyncBrowseAction = Commands->addAction(tr("Synchronize &Browsing"));
  FSyncBrowseAction->setCheckable(true);
  FSyncBrowseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
  connect(FSyncBrowseAction, &QAction::triggered, this, [this](bool On) { SetSynchronizeBrowsing(On); });
  Commands->addAction(FFindFilesAction);
  // As Queue > Process Queue (QueueEnableAction): the session's queue run or held.
  QMenu * QueueCommands = Commands->addMenu(tr("Q&ueue"));
  FProcessQueueAction = QueueCommands->addAction(tr("&Process Queue"));
  FProcessQueueAction->setCheckable(true);
  FProcessQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q));
  connect(FProcessQueueAction, &QAction::triggered, this, [this](bool Enabled)
  {
    if (FSession)
    {
      FSession->SetQueueEnabled(Enabled);
    }
  });
  // As GoToQueue: the list shown if hidden, and focused.
  QAction * QueueGoTo = QueueCommands->addAction(tr("&Go To"), this, [this]()
  {
    if (!FQueue->isVisible())
    {
      SetQueueShow(CoreBridge::TLayout::TQueueShow::Show);
    }
    FQueue->List()->setFocus();
  });
  QueueGoTo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
  QueueGoTo->setStatusTip(tr("Go to transfer queue list"));
  QueueCommands->addSeparator();
  FQueue->AddCommands(QueueCommands);
  Commands->addMenu(FStaticCommandsMenu);
  Commands->addSeparator();
  Commands->addAction(FConsoleAction);
  Commands->addAction(FPuttyAction);
  Commands->addSeparator();
  FClearCachesAction = Commands->addAction(tr("Clea&r Caches"), this, [this]() { ClearCaches(); });
  Commands->addSeparator();
  Commands->addAction(FQuitAction);

  QMenu * Tabs = menuBar()->addMenu(tr("&Tabs"));
  // As WinSCP's New Tab: a remote one (the Login dialog, Ctrl+N) or a
  // local browser.
  QMenu * NewTab = Tabs->addMenu(tr("&New Tab"));
  Iconize(NewTab->menuAction(), "new-tab");
  QAction * RemoteTab = Iconize(NewTab->addAction(tr("&Remote Tab..."), this, &MainWindow::SiteManagerDialog), "sites");
  RemoteTab->setShortcut(FSitesAction->shortcut());
  FSitesAction->setShortcut(QKeySequence());
  Iconize(NewTab->addAction(tr("&Local Tab"), this, [this]() { NewLocalTab(); }), "local-tab");
  NewTab->addSeparator();
  FDefaultRemoteTabAction = NewTab->addAction(tr("&Default to Remote Tab"));
  FDefaultRemoteTabAction->setCheckable(true);
  FDefaultRemoteTabAction->setChecked(CoreBridge::Layout().DefaultToNewRemoteTab);
  FDefaultRemoteTabAction->setStatusTip(tr("Make the New Tab command open a remote tab rather than a local one"));
  connect(FDefaultRemoteTabAction, &QAction::triggered, this, [this](bool Remote)
  {
    StoreLayoutChoice([Remote](CoreBridge::TLayout & Layout) { Layout.DefaultToNewRemoteTab = Remote; });
  });
  Tabs->addAction(FCloseTabAction);
  Tabs->addAction(FDuplicateTabAction);
  FRenameTabAction = Tabs->addAction(tr("&Rename Tab"), this, [this]() { RenameTab(); });
  Tabs->addSeparator();
  FColorMenu = Tabs->addMenu(tr("Co&lor"));
  FColorMenu->menuAction()->setStatusTip(tr("Change color of the current tab"));
  connect(FColorMenu, &QMenu::aboutToShow, this, &MainWindow::FillColorMenu);
  FillColorMenu();
  Tabs->addSeparator();
  Tabs->addAction(FDisconnectAction);
  FReconnectAction = Tabs->addAction(tr("&Reconnect Session"), this, [this]() { ReconnectSession(); });
  FSaveSiteAction = Tabs->addAction(tr("&Save Session as Site..."), this, [this]() { SaveSessionAsSite(); });
  Tabs->addSeparator();
  Tabs->addAction(FServerInfoAction);
  Tabs->addAction(FGenerateSessionUrlAction);
  FChangePasswordAction = Tabs->addAction(tr("&Change Password..."), this, [this]() { ChangePassword(); });
  FPublicKeyAction = Tabs->addAction(tr("&Install Public Key into Server..."), this, [this]() { InstallPublicKey(); });
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
  Iconize(Workspaces->menuAction(), "workspace");
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
  Iconize(Sites->menuAction(), "sites");
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
  BuildToolbars(Options);
  // As SessionsTabsAction: the tab bar, shown or not.
  FTabsAction = Options->addAction(tr("T&abs"));
  FTabsAction->setCheckable(true);
  FTabsAction->setStatusTip(tr("Hide/show tabs"));
  connect(FTabsAction, &QAction::triggered, this, [this](bool Shown)
  {
    FTabsShown = Shown;
    UpdateTabs();
    const bool Explorer = FExplorer;
    StoreLayoutChoice([Shown, Explorer](CoreBridge::TLayout & Layout) { (Explorer ? Layout.Explorer.Tabs : Layout.Tabs) = Shown; });
  });
  Options->addAction(FCommandLineAction);
  FStatusBarAction = Options->addAction(tr("Status &Bar"));
  FStatusBarAction->setCheckable(true);
  connect(FStatusBarAction, &QAction::triggered, this, [this](bool Shown) { SetStatusBarShown(Shown); });
  // As WinSCP's Queue submenu: where the queue's list shows.
  QMenu * QueueMenu = Options->addMenu(tr("&Queue"));
  QActionGroup * QueueGroup = new QActionGroup(QueueMenu);
  const std::pair<CoreBridge::TLayout::TQueueShow, QString> QueueShows[] = {
    { CoreBridge::TLayout::TQueueShow::Show, tr("&Show") },
    { CoreBridge::TLayout::TQueueShow::HideWhenEmpty, tr("Hide when &Empty") },
    { CoreBridge::TLayout::TQueueShow::Hide, tr("&Hide") } };
  for (const auto & QueueShow : QueueShows)
  {
    QAction * Action = QueueMenu->addAction(QueueShow.second);
    Action->setCheckable(true);
    QueueGroup->addAction(Action);
    const CoreBridge::TLayout::TQueueShow Show = QueueShow.first;
    connect(Action, &QAction::triggered, this, [this, Show]() { SetQueueShow(Show); });
    FQueueShowActions[static_cast<int>(Show)] = Action;
  }
  QueueMenu->addSeparator();
  FQueueToolbarAction = QueueMenu->addAction(tr("&Toolbar"));
  FQueueToolbarAction->setCheckable(true);
  FQueueToolbarAction->setStatusTip(tr("Hide/show queue list toolbar (on queue list panel)"));
  connect(FQueueToolbarAction, &QAction::triggered, this, [this](bool Shown)
  {
    FQueue->SetToolbarVisible(Shown);
    StoreLayoutChoice([Shown](CoreBridge::TLayout & Layout) { Layout.QueueToolbar = Shown; });
  });
  FQueueFileListAction = QueueMenu->addAction(tr("&File List"));
  FQueueFileListAction->setCheckable(true);
  FQueueFileListAction->setStatusTip(tr("Hide/show full queue file list"));
  connect(FQueueFileListAction, &QAction::triggered, this, [this](bool Shown)
  {
    FQueue->SetFileListVisible(Shown);
    StoreLayoutChoice([Shown](CoreBridge::TLayout & Layout) { Layout.QueueFileList = Shown; });
  });
  QueueMenu->addSeparator();
  QueueMenu->addAction(tr("&Reset Columns Layout"), this, [this]() { FQueue->ResetColumnsLayout(); })
    ->setStatusTip(tr("Reset to the default layout of list columns"));
  QueueMenu->addSeparator();
  QMenu * OnceEmpty = QueueMenu->addMenu(tr("&Once Empty"));
  OnceEmpty->menuAction()->setStatusTip(tr("Toggle action to perform once the queue list is empty"));
  QActionGroup * OnceEmptyGroup = new QActionGroup(OnceEmpty);
  const struct
  {
    TOnceEmpty Operation;
    QString Text;
    QString Hint;
  } OnceEmptyChoices[] = {
    { TOnceEmpty::Idle, tr("&Stay Idle"), tr("Stay idle once the queue is empty") },
    { TOnceEmpty::Disconnect, tr("&Disconnect Session"), tr("Disconnect the session once the queue is empty") },
    { TOnceEmpty::Suspend, tr("Slee&p Computer"), tr("Put the computer into sleep mode once the queue is empty") },
    { TOnceEmpty::ShutDown, tr("S&hut Down Computer"), tr("Shut down the computer once the queue is empty") } };
  for (const auto & Choice : OnceEmptyChoices)
  {
    QAction * Action = OnceEmpty->addAction(Choice.Text);
    Action->setCheckable(true);
    Action->setStatusTip(Choice.Hint);
    OnceEmptyGroup->addAction(Action);
    const TOnceEmpty Operation = Choice.Operation;
    connect(Action, &QAction::triggered, this, [this, Operation]() { SetOnceEmpty(Operation); });
    FOnceEmptyActions[static_cast<int>(Operation)] = Action;
  }
  FOnceEmptyActions[0]->setChecked(true);
  // As AllowQueueOperation(qoOnceEmpty): while anything is queued.
  connect(QueueMenu, &QMenu::aboutToShow, this, [this]()
  {
    const bool Queued = FSession && FSession->Active() && !FSession->QueueEmpty();
    for (QAction * Action : FOnceEmptyActions)
    {
      Action->setEnabled(Queued);
    }
  });
  // As QueuePreferencesAction: Preferences' Background page.
  QueueMenu->addAction(tr("&Customize..."), this, [this]() { PreferencesDialog(::PreferencesDialog::tr("Background")); })
    ->setStatusTip(tr("Customize queue list"));
  Options->addSeparator();
  Options->addAction(FPreferencesAction);
  {
    const CoreBridge::TLayout Layout = CoreBridge::Layout();
    FStatusBarAction->setChecked(Layout.StatusBar);
    statusBar()->setVisible(Layout.StatusBar);
    FQueueShowActions[static_cast<int>(Layout.QueueShow)]->setChecked(true);
    FTabsShown = Layout.Tabs;
    FTabsAction->setChecked(Layout.Tabs);
    FQueueToolbarAction->setChecked(Layout.QueueToolbar);
    FQueue->SetToolbarVisible(Layout.QueueToolbar);
    FQueueFileListAction->setChecked(Layout.QueueFileList);
    FQueue->SetFileListVisible(Layout.QueueFileList);
  }

  FRemoteMenu = PanelMenu(false);

  // As WinSCP's Help, its pages linscp's own (the project's GitHub pages),
  // but the documentation, which is WinSCP's: linscp follows its interface.
  // Show Tips (WinSCP's tips, from winscp.net) is not offered.
  QMenu * Help = menuBar()->addMenu(tr("&Help"));
  QAction * Contents = Help->addAction(tr("&Contents"), this,
    [this]() { OpenWebPage(QStringLiteral("https://winscp.net/eng/docs/start")); });
  Contents->setShortcut(QKeySequence(Qt::Key_F1));
  Contents->setStatusTip(tr("Opens web browser and points it to documentation table of contents"));
  Help->addSeparator();
  Help->addAction(tr("Product &Homepage"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp")); })
    ->setStatusTip(tr("Opens web browser and points it to project homepage"));
  Help->addAction(tr("&Support Forum"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp/issues")); })
    ->setStatusTip(tr("Opens web browser and points it to support forum page"));
  Help->addAction(tr("&Version History"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp/releases")); })
    ->setStatusTip(tr("Opens web browser and points it to application history page"));
  Help->addSeparator();
  Help->addAction(tr("Check for &Updates"), this, [this]() { CheckForUpdates(); })
    ->setStatusTip(tr("Queries application homepage for updates"));
  Help->addSeparator();
  Help->addAction(tr("&Donate"), this, [this]() { OpenWebPage(QString::fromLatin1(DonateUrl)); })
    ->setStatusTip(tr("Opens web browser and points it to project donation page"));
  Help->addSeparator();
  Help->addAction(FAboutAction);

  // The icons, after WinSCP's glyphs for the same commands.
  const std::pair<QAction *, const char *> Roles[] = {
    { FSitesAction, "sites" }, { FOpenAction, "link" }, { FDisconnectAction, "disconnect" }, { FQuitAction, "quit" },
    { FAboutAction, "about" }, { FEditAction, "edit" }, { FOpenCurrentAction, "open" }, { FLinkAction, "link" },
    { FPropertiesAction, "properties" }, { FGenerateFileUrlAction, "generate-url" }, { FCopyAction, "copy" },
    { FMoveAction, "move" }, { FDuplicateAction, "duplicate" }, { FMoveToAction, "move" },
    { FOpenDirectoryAction, "open-directory" }, { FAddBookmarkAction, "bookmark" }, { FExploreAction, "explore" },
    { FFindFilesAction, "find" }, { FConsoleAction, "console" }, { FPuttyAction, "putty" },
    { FServerInfoAction, "server-info" }, { FGenerateSessionUrlAction, "generate-url" },
    { FSynchronizeAction, "synchronize" }, { FKeepUpToDateAction, "keep-up-to-date" },
    { FDuplicateTabAction, "duplicate-tab" }, { FCloseTabAction, "close-tab" }, { FSaveWorkspaceAction, "save-workspace" },
    { FFilterAction, "filter" }, { FPreferencesAction, "preferences" }, { FRenameTabAction, "rename" },
    { FReconnectAction, "reconnect" }, { FSaveSiteAction, "save-site" }, { FChangePasswordAction, "change-password" },
    { FPublicKeyAction, "public-key" }, { FClearCachesAction, "clear-caches" }, { FCompareAction, "compare" },
    { FSyncBrowseAction, "sync-browse" }, { FDirectorySizesAction, "calculate-size" }, { FNewFileAction, "new-file" },
    { FCopyClipboardAction, "clipboard-copy" }, { FPasteAction, "paste" },
    { FFileCommandsMenu->menuAction(), "custom-commands" }, { FStaticCommandsMenu->menuAction(), "custom-commands" },
  };
  for (const auto & Role : Roles)
  {
    Iconize(Role.first, Role.second);
  }
  ApplyIcons();

  // The keys of what no menu shows.
  addActions({ FSitesAction, FOpenAction, FOpenDirectoryAction, FAddBookmarkAction, FFilterAction, FHiddenAction,
    FNextTabAction, FPreviousTabAction, FExploreAction });
}

// --- The local browser ----------------------------------------------------------

namespace
{

// One file or directory, copied (Move: moved) to Destination, as the
// shell's file operation WinSCP gives this to (TFileOperator): a directory
// merged into one that is there; a file that is, overwritten after
// asking. Error: what failed; false then, or when cancelled.
bool LocalCopyEntry(QWidget * Window, const QString & Source, const QString & Destination, bool Move,
  bool Confirm, int & OverwriteAll, QString & Error)
{
  const QFileInfo Info(Source);
  if (Info.isDir() && !Info.isSymLink())
  {
    const QString Clean = QDir::cleanPath(Source);
    if ((QDir::cleanPath(Destination) == Clean) || QDir::cleanPath(Destination).startsWith(Clean + QLatin1Char('/')))
    {
      Error = MainWindow::tr("Cannot copy '%1' into itself.").arg(QDir::toNativeSeparators(Source));
      return false;
    }
    // A move within one file system: renamed whole.
    if (Move && !QFileInfo::exists(Destination) && QDir().rename(Source, Destination))
    {
      return true;
    }
    if (!QFileInfo(Destination).isDir() && !QDir().mkpath(Destination))
    {
      Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateLocalDirectoryError))
        .replace(QStringLiteral("%s"), QDir::toNativeSeparators(Destination));
      return false;
    }
    const QFileInfoList Entries = QDir(Source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo & Entry : Entries)
    {
      if (!LocalCopyEntry(Window, Entry.filePath(), QDir(Destination).filePath(Entry.fileName()), Move, Confirm,
            OverwriteAll, Error))
      {
        return false;
      }
    }
    if (Move && !QDir(Source).rmdir(QStringLiteral(".")) && !QDir().rmdir(Source))
    {
      Error = MainWindow::tr("Cannot remove '%1'.").arg(QDir::toNativeSeparators(Source));
      return false;
    }
    return true;
  }
  if (QFileInfo::exists(Destination) || QFileInfo(Destination).isSymLink())
  {
    if (QFileInfo(Destination).canonicalFilePath() == Info.canonicalFilePath())
    {
      Error = MainWindow::tr("Cannot copy '%1' onto itself.").arg(QDir::toNativeSeparators(Source));
      return false;
    }
    int Answer = OverwriteAll;
    if (Confirm && (Answer == 0))
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FileOverwrite))
        .replace(QStringLiteral("%s"), QDir::toNativeSeparators(Destination)).toStdString();
      for (const auto & Button : std::initializer_list<std::pair<unsigned, const char *>>{
             { CoreBridge::AnswerYes, "&Yes" }, { CoreBridge::AnswerYesToAll, "Yes to &All" },
             { CoreBridge::AnswerNo, "&No" }, { CoreBridge::AnswerNoToAll, "N&o to All" },
             { CoreBridge::AnswerCancel, "Cancel" } })
      {
        CoreBridge::TQueryButton QueryButton;
        QueryButton.Answer = Button.first;
        QueryButton.Caption = Button.second;
        Query.Buttons.push_back(QueryButton);
      }
      Query.CancelAnswer = CoreBridge::AnswerCancel;
      QtFrontend Frontend;
      Frontend.SetWindow(Window);
      Answer = static_cast<int>(Frontend.Query(Query).Answer);
      if (Answer == CoreBridge::AnswerYesToAll)
      {
        OverwriteAll = CoreBridge::AnswerYes;
      }
      else if (Answer == CoreBridge::AnswerNoToAll)
      {
        OverwriteAll = CoreBridge::AnswerNo;
      }
    }
    if (Answer == CoreBridge::AnswerCancel)
    {
      return false;
    }
    if ((Answer == CoreBridge::AnswerNo) || (Answer == CoreBridge::AnswerNoToAll))
    {
      return true;
    }
    if (!QFile::remove(Destination))
    {
      Error = MainWindow::tr("Cannot overwrite '%1'.").arg(QDir::toNativeSeparators(Destination));
      return false;
    }
  }
  if (Move && QFile::rename(Source, Destination))
  {
    return true;
  }
  QFile File(Source);
  if (!File.copy(Destination))
  {
    Error = MainWindow::tr("Cannot copy '%1' to '%2': %3")
      .arg(QDir::toNativeSeparators(Source), QDir::toNativeSeparators(Destination), File.errorString());
    return false;
  }
  if (Move && !QFile::remove(Source))
  {
    Error = MainWindow::tr("Cannot remove '%1'.").arg(QDir::toNativeSeparators(Source));
    return false;
  }
  return true;
}

}  // namespace

bool MainWindow::LocalLocalCopy(bool Move, bool Confirm, const QStringList * Sources, const QString * Target,
  const QString & Mask)
{
  // (Given the files and where to: also to paste into a local panel.)
  if (FBusy || (!LocalBrowser() && ((Sources == nullptr) || (Target == nullptr))))
  {
    return false;
  }
  LocalPanel * Source = SideLocal(FLocalActive);
  LocalPanel * Other = SideLocal(!FLocalActive);
  QStringList Paths;
  if (Sources != nullptr)
  {
    Paths = *Sources;
  }
  else
  {
    for (const QString & Name : Source->Selected())
    {
      Paths.append(QDir(Source->Path()).filePath(Name));
    }
  }
  if (Paths.isEmpty())
  {
    return false;
  }
  QString Directory = (Target != nullptr) ? *Target : QDir::toNativeSeparators(Other->Path());
  QString FileMask = Mask;
  // As LocalLocalCopy: asked as Environment's "Transferring of files".
  if (Confirm && CoreBridge::Preferences().ConfirmTransferring)
  {
    CopyLocalDialog Dialog(Move, Paths.size() > 1, Directory, FileMask, this);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Directory = Dialog.Directory();
    FileMask = Dialog.FileMask();
  }
  const QString Destination = QDir::fromNativeSeparators(Directory);
  FBusy = true;
  UpdateActions();
  bool Result = true;
  int OverwriteAll = 0;
  QString Error;
  for (const QString & Path : Paths)
  {
    const QString Name = QString::fromStdString(CoreBridge::MaskFileName(QFileInfo(Path).fileName().toStdString(),
      FileMask.toStdString()));
    if (!LocalCopyEntry(this, Path, QDir(Destination).filePath(Name), Move, true, OverwriteAll, Error))
    {
      Result = false;
      break;
    }
  }
  FBusy = false;
  if (Source != nullptr)
  {
    Source->View()->clearSelection();
  }
  // As ReloadLocalDirectory: both panels, as either may show either.
  FLocal->Refresh();
  FOtherLocal->Refresh();
  UpdateActions();
  if (!Error.isEmpty())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Error;
    Query.Message = Error.toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    Query.Buttons = { OK };
    Query.CancelAnswer = CoreBridge::AnswerOK;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.Query(Query);
  }
  return Result;
}

// --- Session commands (P3-14) ------------------------------------------------------

namespace
{

void Inform(QWidget * Window, const std::string & Message)
{
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Information;
  Query.Message = Message;
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  Query.Buttons = { OK };
  Query.CancelAnswer = CoreBridge::AnswerOK;
  QtFrontend Frontend;
  Frontend.SetWindow(Window);
  Frontend.Query(Query);
}

}  // namespace

bool MainWindow::ReconnectSession()
{
  if (FBusy || !FSession || FSession->Active())
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    statusBar()->showMessage(tr("Connecting to %1...").arg(QString::fromStdString(FSession->Name())));
    FSession->Reconnect();
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
  UpdateTabs();
  UpdateTitle();
  UpdateActions();
  return Result;
}

bool MainWindow::SaveSessionAsSite(const QString * Name, bool SavePassword)
{
  if (FBusy || !FSession)
  {
    return false;
  }
  CoreBridge::TSession::TSaveAsSite Defaults = FSession->SaveAsSiteDefaults();
  while (true)
  {
    QString AName;
    bool ASavePassword = SavePassword;
    bool CreateShortcut = false;
    if (Name != nullptr)
    {
      AName = *Name;
    }
    else
    {
      SaveSiteDialog Dialog(Defaults, this);
      if (Dialog.exec() != QDialog::Accepted)
      {
        return false;
      }
      AName = Dialog.Name();
      ASavePassword = Dialog.SavePassword();
      CreateShortcut = Dialog.CreateShortcut();
    }
    try
    {
      FSession->SaveAsSite(AName.toStdString(), ASavePassword);
      // As DoSaveSession: the site's desktop shortcut, with /desktop and
      // /UploadIfAny.
      if (CreateShortcut)
      {
        Integration::AskAndCreateShortcut(this, QString(), CoreBridge::SessionShortcut(AName.toStdString(),
          CoreBridge::TSessionShortcutKind::Site, std::string(), { "/Desktop", "/UploadIfAny" }), Integration::TShortcutFolder::Desktop);
      }
      return true;
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      // As DoValidate: the dialog again, with what was typed.
      if (Name != nullptr)
      {
        return false;
      }
      Defaults.Name = AName.toStdString();
      Defaults.SavePassword = ASavePassword;
    }
  }
}

// As RenameTab: the name asked for, the session's (or the local tab's).
bool MainWindow::RenameTab(const QString * Name)
{
  if (FBusy || (FActiveTab < 0))
  {
    return false;
  }
  const QString Current = FSession ? QString::fromStdString(FSession->Name()) : FTabBar->tabText(FActiveTab);
  QString NewName;
  if (Name != nullptr)
  {
    NewName = *Name;
  }
  else
  {
    bool OK = false;
    NewName = QInputDialog::getText(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::RenameTabTitle)),
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::RenameTabPrompt)), QLineEdit::Normal, Current, &OK);
    if (!OK)
    {
      return false;
    }
  }
  // When submitting the default name, do not fix it.
  if (NewName == Current)
  {
    return false;
  }
  try
  {
    if (FSession)
    {
      FSession->Rename(NewName.toStdString());
    }
    else
    {
      FTabs[FActiveTab].Name = NewName;
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  UpdateTabs();
  UpdateTitle();
  return true;
}

bool MainWindow::ChangePassword()
{
  if (FBusy || !FSession || !FSession->CanChangePassword())
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = false;
  try
  {
    FSession->ChangePassword();
    Result = true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  if (Result)
  {
    Inform(this, CoreBridge::Text(CoreBridge::TText::PasswordChanged));
  }
  return Result;
}

// As TTerminalManager::UploadPublicKey: the key chosen (the site's to start
// with), converted if need be; a server that is not OpenSSH asked about.
bool MainWindow::InstallPublicKey(const QString * FileName)
{
  if (FBusy || !FSession || !FSession->CanUploadPublicKey())
  {
    return false;
  }
  QString Key;
  if (FileName != nullptr)
  {
    Key = *FileName;
  }
  else
  {
    Key = QFileDialog::getOpenFileName(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::PublicKeyTitle)),
      QString::fromStdString(FSession->PublicKeyFile()),
      tr("PuTTY Private Key Files (*.ppk);;All Private Key Files (*.ppk *.pem *.key id_*);;All Files (*)"));
    if (Key.isEmpty())
    {
      return false;
    }
  }
  if (!VerifyAndConvertKey(this, Key, false, [this](const QString & Suggested) { return ChooseConvertedKeyFile(this, Suggested); }))
  {
    return false;
  }
  if (!FSession->IsOpenSsh())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotOpenSsh))
      .replace(QStringLiteral("%s"), QString::fromStdString(FSession->SshImplementation())).toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
  }
  FBusy = true;
  UpdateActions();
  std::string Installed;
  try
  {
    Installed = FSession->UploadPublicKey(QDir::toNativeSeparators(Key).toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  FBusy = false;
  UpdateActions();
  if (!Installed.empty())
  {
    Inform(this, Installed);
  }
  return !Installed.empty();
}

bool MainWindow::ClearCaches()
{
  if (FBusy || !FSession || !FSession->Active())
  {
    return false;
  }
  FSession->ClearCaches();
  UpdateActions();
  return true;
}

// --- Panel commands (P3-14) ---------------------------------------------------------

namespace
{

// A modification time as wall-clock seconds at the precision given
// (ReduceDateTimePrecision): the remote panel's are the server's wall
// clock; a local file's are made so (Local).
double ComparableTime(const CoreBridge::TRemoteEntry & Entry, bool Local, CoreBridge::TRemoteEntry::TPrecision Precision)
{
  using TPrecision = CoreBridge::TRemoteEntry::TPrecision;
  // To the millisecond first: a TDateTime made seconds can fall just short
  // of a whole one.
  double Time = std::round(Entry.ModifiedTime * 1000) / 1000;
  if (Local)
  {
    Time += QDateTime::fromSecsSinceEpoch(static_cast<qint64>(Time)).offsetFromUtc();
  }
  switch (Precision)
  {
    case TPrecision::None: return 0;
    case TPrecision::Day: return std::floor(Time / 86400) * 86400;
    case TPrecision::Minute: return std::floor(Time / 60) * 60;
    case TPrecision::Second: return std::floor(Time);
  }
  return Time;
}

// TCustomDirView::ProcessChangedFiles: the files of Entries newer than (or,
// by size, unlike) those of Mirror's of the name, or not in Mirror.
QStringList ChangedFiles(const std::vector<CoreBridge::TRemoteEntry> & Entries, bool Local,
  const std::vector<CoreBridge::TRemoteEntry> & Mirror, bool MirrorLocal, bool ByTime, bool BySize)
{
  QStringList Result;
#if defined(Q_OS_WIN)
  const Qt::CaseSensitivity Case = MirrorLocal ? Qt::CaseInsensitive : Qt::CaseSensitive;
#else
  const Qt::CaseSensitivity Case = Qt::CaseSensitive;
#endif
  for (const CoreBridge::TRemoteEntry & Entry : Entries)
  {
    if (Entry.Directory)
    {
      continue;
    }
    const QString Name = QString::fromStdString(Entry.Name);
    const CoreBridge::TRemoteEntry * Other = nullptr;
    for (const CoreBridge::TRemoteEntry & Candidate : Mirror)
    {
      if (QString::fromStdString(Candidate.Name).compare(Name, Case) == 0)
      {
        Other = &Candidate;
        break;
      }
    }
    bool Changed = (Other == nullptr);
    if (Other != nullptr)
    {
      bool SameTime = true;
      if (ByTime)
      {
        const auto Precision = std::min(Entry.ModifiedPrecision, Other->ModifiedPrecision);
        const double Time = ComparableTime(Entry, Local, Precision);
        double OtherTime = ComparableTime(*Other, MirrorLocal, Precision);
        SameTime = (Time == OtherTime);
        if (Precision == CoreBridge::TRemoteEntry::TPrecision::Second)
        {
          // "1 ms more solves the rounding issues."
          OtherTime += 1.001;
        }
        Changed = (Time > OtherTime);
      }
      if (!Changed && SameTime && BySize)
      {
        Changed = (Entry.Size != Other->Size);
      }
    }
    if (Changed)
    {
      Result.append(Name);
    }
  }
  return Result;
}

}  // namespace

bool MainWindow::CompareDirectories()
{
  LocalPanel * Right = SideLocal(false);
  if (FBusy || (!Right && !(FSession && FSession->Active())))
  {
    return false;
  }
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  const std::vector<CoreBridge::TRemoteEntry> LeftEntries = FLocal->Entries();
  const std::vector<CoreBridge::TRemoteEntry> RightEntries = Right ? Right->Entries() : FPanel->Entries();
  const QStringList LeftChanged = ChangedFiles(LeftEntries, true, RightEntries, Right != nullptr,
    Preferences.CompareByTime, Preferences.CompareBySize);
  const QStringList RightChanged = ChangedFiles(RightEntries, Right != nullptr, LeftEntries, true,
    Preferences.CompareByTime, Preferences.CompareBySize);
  FLocal->Select(LeftChanged);
  if (Right != nullptr)
  {
    Right->Select(RightChanged);
  }
  else
  {
    FPanel->Select(RightChanged);
  }
  if (LeftChanged.isEmpty() && RightChanged.isEmpty())
  {
    Inform(this, CoreBridge::Text(CoreBridge::TText::CompareNoDifferences));
    return false;
  }
  return true;
}

bool MainWindow::SynchronizeBrowsing() const
{
  return (FSyncBrowseAction != nullptr) && FSyncBrowseAction->isChecked();
}

// As SynchronizeBrowsingChanged: noted in the status bar.
void MainWindow::SetSynchronizeBrowsing(bool On)
{
  FSyncBrowseAction->setChecked(On);
  FPrevPath[1] = FLocal->Path();
  FPrevPath[0] = FPanel->Path();
  const QStringList Parts = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SyncBrowseToggle)).split(QLatin1Char('|'));
  statusBar()->showMessage(Parts.value(0).replace(QStringLiteral("%s"), Parts.value(On ? 1 : 2)));
}

// SynchronizeBrowsingLocal and ...Remote: the other side's directory for
// the change from PrevPath to where the panel (Local: the local one) is
// now, by what both share: false when they share no common path.
bool MainWindow::SyncBrowse(bool Local, const QString & PrevPath, QString & NewPath, bool Create)
{
  const QString Current = Local ? FLocal->Path() : FPanel->Path();
  // The panels' own paths: '/' separated, the local one's cleaned.
  auto Parent = [](const QString & Path)
  {
    const QString Clean = (Path.size() > 1) && Path.endsWith(QLatin1Char('/')) ? Path.chopped(1) : Path;
    const int Slash = Clean.lastIndexOf(QLatin1Char('/'));
    return (Slash > 0) ? Clean.left(Slash) : ((Slash == 0) ? QStringLiteral("/") : Clean);
  };
  auto WithSlash = [](const QString & Path) { return Path.endsWith(QLatin1Char('/')) ? Path : (Path + QLatin1Char('/')); };
#if defined(Q_OS_WIN)
  const Qt::CaseSensitivity Case = Local ? Qt::CaseInsensitive : Qt::CaseSensitive;
#else
  const Qt::CaseSensitivity Case = Qt::CaseSensitive;
#endif
  // ExtractCommonPath: the longest directory both are in.
  QString Common = WithSlash(PrevPath);
  while (!WithSlash(Current).startsWith(Common, Case))
  {
    const QString Up = WithSlash(Parent(Common));
    if (Up == Common)
    {
      return false;
    }
    Common = Up;
  }
  QString Prev = WithSlash(PrevPath);
  NewPath = Local ? FPanel->Path() : FLocal->Path();
  while (Prev.compare(Common, Case) != 0)
  {
    const QString Up = Parent(NewPath);
    if (Up == NewPath)
    {
      return false;
    }
    NewPath = Up;
    Prev = WithSlash(Parent(Prev));
  }
  const QString Relative = WithSlash(Current).mid(Prev.size());
  NewPath = WithSlash(NewPath) + Relative;
  if ((NewPath.size() > 1) && NewPath.endsWith(QLatin1Char('/')))
  {
    NewPath.chop(1);
  }
  if (Create)
  {
    if (Local)
    {
      FSession->CreateDirectory(NewPath.toStdString());
    }
    else
    {
      QDir().mkpath(NewPath);
    }
  }
  return true;
}

// As TScpCommanderForm::GetTransferPresetAutoSelectData: the session's
// host and user, the panels' directories.
CoreBridge::TTransferPresetRule MainWindow::TransferPresetAutoSelectData() const
{
  CoreBridge::TTransferPresetRule Result;
  Result.HostName = FSession->HostNameExpanded();
  Result.UserName = FSession->UserNameExpanded();
  Result.RemoteDirectory = FPanel->Path().toStdString();
  Result.LocalDirectory = QDir::toNativeSeparators(FLocal->Path()).toStdString();
  return Result;
}

// As TCustomScpExplorerForm::TransferPresetAutoSelect: the first preset
// whose rule matches the session and the panels made the current one; none
// matching, back to the one chosen. Announced either way.
void MainWindow::TransferPresetAutoSelect()
{
  if (!FSession || !FSession->Active() || LocalBrowser() || FSynchronisingBrowse)
  {
    return;
  }
  bool Found = false;
  const QString Name = QString::fromStdString(CoreBridge::FindTransferPreset(TransferPresetAutoSelectData(), Found));
  const QString Current = QString::fromStdString(CoreBridge::CurrentTransferPreset());
  QString New = Current;
  if (!Found)
  {
    FCopyParamAutoSelected.clear();
    New = FCopyParamDefault;
  }
  else if (Name != FCopyParamAutoSelected)
  {
    FCopyParamAutoSelected = Name;
    New = Name;
  }
  if (New == Current)
  {
    return;
  }
  try
  {
    CoreBridge::SetCurrentTransferPreset(New.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return;
  }
  UpdatePresetCombo();
  QString Message;
  if (Found)
  {
    Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamAutoSelected));
  }
  else
  {
    Message = QString::fromStdString(CoreBridge::Text(New.isEmpty() ?
      CoreBridge::TText::CopyParamDefaultNorm : CoreBridge::TText::CopyParamDefaultCustom));
  }
  Message.replace(QStringLiteral("%s"), PresetCaption(New));
  if (!CoreBridge::Preferences().CopyParamAutoSelectNotice)
  {
    // As PostNote: the status bar's.
    statusBar()->showMessage(Message, 30000);
    return;
  }
  QString Info = QString::fromStdString(CoreBridge::TransferSettingsInfo(CoreBridge::CurrentTransferSettings()));
  if (Found)
  {
    for (const CoreBridge::TTransferPreset & Preset : CoreBridge::TransferPresets())
    {
      if (QString::fromStdString(Preset.Name) == Name)
      {
        Info += QStringLiteral("\n \n") + QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamRule))
          .replace(QStringLiteral("%s"), QString::fromStdString(CoreBridge::TransferPresetRuleInfo(Preset.Rule)));
      }
    }
  }
  // As TransferPresetNoteMessage: OK, Configure..., never again.
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Information;
  Query.Message = Message.toStdString();
  Query.MoreMessages = Info.toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  CoreBridge::TQueryButton Configure;
  Configure.Answer = CoreBridge::AnswerIgnore;
  Configure.Caption = CoreBridge::Text(CoreBridge::TText::ConfigureButton);
  Query.Buttons = { OK, Configure };
  Query.CancelAnswer = CoreBridge::AnswerOK;
  Query.NeverAskAgainCheck = true;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  const CoreBridge::TQueryResult Answer = Frontend.Query(Query);
  if ((Answer.Answer == CoreBridge::AnswerOK) && Answer.NeverAskAgain)
  {
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.CopyParamAutoSelectNotice = false;
    CoreBridge::SetPreferences(Preferences);
  }
  else if (Answer.Answer == CoreBridge::AnswerIgnore)
  {
    PreferencesDialog(::PreferencesDialog::tr("Transfer"));
    UpdatePresetCombo();
  }
}

void MainWindow::PanelLoaded(bool Local)
{
  TransferPresetAutoSelect();
  const QString Current = Local ? FLocal->Path() : FPanel->Path();
  const QString PrevPath = FPrevPath[Local ? 1 : 0];
  FPrevPath[Local ? 1 : 0] = Current;
  if (FSynchronisingBrowse || !SynchronizeBrowsing() || PrevPath.isEmpty() || (PrevPath == Current) ||
      LocalBrowser() || !FSession || !FSession->Active() || FBusy)
  {
    return;
  }
  FSynchronisingBrowse = true;
  QString NewPath;
  bool Followed = false;
  bool Known = SyncBrowse(Local, PrevPath, NewPath, false);
  if (Known)
  {
    Followed = Local ? FPanel->ChangeDirectory(NewPath) : FLocal->ChangeDirectory(NewPath);
    const QString There = Local ? FPanel->Path() : FLocal->Path();
    Followed = Followed || (QDir::cleanPath(There) == QDir::cleanPath(NewPath));
    if (!Followed)
    {
      // As WinSCP: offered to be created, else synchronization off.
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SyncBrowseCreate))
        .replace(QStringLiteral("%s"), Local ? NewPath : QDir::toNativeSeparators(NewPath)).toStdString();
      CoreBridge::TQueryButton Yes;
      Yes.Answer = CoreBridge::AnswerYes;
      Yes.Caption = "&Yes";
      CoreBridge::TQueryButton No;
      No.Answer = CoreBridge::AnswerNo;
      No.Caption = "&No";
      Query.Buttons = { Yes, No };
      Query.CancelAnswer = CoreBridge::AnswerNo;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer == CoreBridge::AnswerYes)
      {
        try
        {
          SyncBrowse(Local, PrevPath, NewPath, true);
          Followed = Local ? FPanel->ChangeDirectory(NewPath) : FLocal->ChangeDirectory(NewPath);
        }
        catch (const CoreBridge::TCoreError & E)
        {
          ShowError(E);
        }
      }
      if (!Followed)
      {
        FSyncBrowseAction->setChecked(false);
      }
    }
  }
  else
  {
    FSyncBrowseAction->setChecked(false);
    Inform(this, CoreBridge::Text(CoreBridge::TText::SyncBrowseError));
  }
  FPrevPath[1] = FLocal->Path();
  FPrevPath[0] = FPanel->Path();
  FSynchronisingBrowse = false;
}

bool MainWindow::CalculateDirectorySizes()
{
  if (FBusy)
  {
    return false;
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  if ((Panel == nullptr) && !(FSession && FSession->Active()))
  {
    return false;
  }
  // The selection's directories (or the focused one's).
  QStringList Directories;
  for (const CoreBridge::TRemoteEntry & Entry : (Panel != nullptr) ? Panel->Entries() : FPanel->Entries())
  {
    const QString Name = QString::fromStdString(Entry.Name);
    if (Entry.Directory && ((Panel != nullptr) ? Panel->Selected() : FPanel->Selected()).contains(Name))
    {
      Directories.append(Name);
    }
  }
  if (Directories.isEmpty())
  {
    return false;
  }
  FBusy = true;
  UpdateActions();
  bool Result = true;
  for (const QString & Name : Directories)
  {
    if (Panel != nullptr)
    {
      // As CalculateLocalFilesSize: every file under it.
      std::int64_t Size = 0;
      QDirIterator It(QDir(Panel->Path()).filePath(Name), QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
      while (It.hasNext())
      {
        It.next();
        Size += It.fileInfo().size();
      }
      Panel->SetDirectorySize(Name, Size);
    }
    else
    {
      try
      {
        std::int64_t Size = 0;
        int Files = 0;
        int Subdirectories = 0;
        int SymLinks = 0;
        FSession->CalculateSize({ Name.toStdString() }, Size, Files, Subdirectories, SymLinks);
        FPanel->SetDirectorySize(Name, Size);
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
        Result = false;
        break;
      }
    }
  }
  FBusy = false;
  UpdateActions();
  return Result;
}

bool MainWindow::PathFromOtherPanel(bool Local)
{
  if (FBusy || !LocalBrowser())
  {
    return false;
  }
  return SideLocal(Local)->ChangeDirectory(SideLocal(!Local)->Path());
}

EditorWindow * MainWindow::NewFile(const QString * Name)
{
  if (FBusy)
  {
    return nullptr;
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  if ((Panel == nullptr) && !(FSession && FSession->Active()))
  {
    return nullptr;
  }
  QString Names;
  if (Name != nullptr)
  {
    Names = *Name;
  }
  else
  {
    // The focused file's name, else "New file".
    const CoreBridge::TRemoteEntry * Focused = ((Panel != nullptr) ? Panel->View() : FPanel->View())->CurrentEntry();
    const QString Suggested = ((Focused != nullptr) && !Focused->Directory) ? QString::fromStdString(Focused->Name) :
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NewFileName));
    bool OK = false;
    Names = QInputDialog::getText(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::EditFileCaption)),
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::EditFilePrompt)), QLineEdit::Normal, Suggested, &OK);
    if (!OK)
    {
      return nullptr;
    }
  }
  EditorWindow * Result = nullptr;
  // Several, separated as file masks are.
  for (QString One : Names.split(QLatin1Char(';'), Qt::SkipEmptyParts))
  {
    One = One.trimmed();
    if (One.isEmpty())
    {
      continue;
    }
    if (Panel != nullptr)
    {
      const QString FileName = QDir(Panel->Path()).absoluteFilePath(One);
      QFile File(FileName);
      if (!File.exists() && !File.open(QIODevice::WriteOnly))
      {
        CoreBridge::TError Error;
        Error.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateFileError))
          .replace(QStringLiteral("%s"), QDir::toNativeSeparators(FileName)).toStdString();
        QtFrontend Frontend;
        Frontend.SetWindow(this);
        Frontend.ShowError(Error);
        return Result;
      }
      File.close();
      Panel->Refresh();
      Result = EditLocal(FileName);
    }
    else
    {
      try
      {
        Result = FSession->FileExists(One.toStdString()) ? EditRemote(One) : EditRemote(One, true);
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
      }
    }
  }
  return Result;
}

// --- Clipboard, status bar, queue (P3-14) --------------------------------------------

bool MainWindow::CopyToClipboard()
{
  if (FBusy)
  {
    return false;
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  TFileDrag Drag;
  Drag.Local = (Panel != nullptr);
  Drag.Directory = (Panel != nullptr) ? Panel->Path() : FPanel->Path();
  Drag.Names = (Panel != nullptr) ? Panel->Selected() : FPanel->Selected();
  if (Drag.Names.isEmpty() || ((Panel == nullptr) && !(FSession && FSession->Active())))
  {
    return false;
  }
  if (Panel != nullptr)
  {
    QApplication::clipboard()->setMimeData(MakeFileDrag(Drag));
    FClipboardData = nullptr;
    FClipboardSession = nullptr;
    return true;
  }
  // As CopyFilesToClipboard: the full paths, as the panel may move on;
  // another program's paste downloads them, as a drag out does.
  FDragDirectories.push_back(std::make_unique<QTemporaryDir>(
    QDir(QString::fromStdString(CoreBridge::TemporaryDirectoryBase())).filePath(QStringLiteral("scpXXXXXX"))));
  const QString Directory = FDragDirectories.back()->path();
  std::vector<std::string> Paths;
  for (const QString & Name : Drag.Names)
  {
    Paths.push_back((Drag.Directory + (Drag.Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name).toStdString());
  }
  CoreBridge::TSession * Session = FSession.get();
  RemoteFileDrag * Data = new RemoteFileDrag(Drag, Directory, [this, Session, Paths, Directory]()
  {
    if (!SessionOpen(Session) || !Session->Active())
    {
      return false;
    }
    try
    {
      return Session->DownloadPaths(Paths, QDir::toNativeSeparators(Directory).toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return false;
    }
  });
  QApplication::clipboard()->setMimeData(Data);
  FClipboardData = Data;
  FClipboardSession = Session;
  return true;
}

bool MainWindow::PasteFromClipboard()
{
  if (FBusy)
  {
    return false;
  }
  const QMimeData * Data = QApplication::clipboard()->mimeData();
  if (Data == nullptr)
  {
    return false;
  }
  LocalPanel * Panel = SideLocal(FLocalActive);
  const bool RemoteTarget = (Panel == nullptr);
  if (RemoteTarget && !(FSession && FSession->Active()))
  {
    return false;
  }
  TFileDrag Drag;
  QStringList LocalSources;
  if (ReadFileDrag(Data, Drag))
  {
    if (!Drag.Local && RemoteTarget)
    {
      // As PasteFromClipBoard's foRemoteCopy: our remote files, pasted on
      // the remote panel of the session they were copied from, duplicated
      // into its directory (the Duplicate dialog's). linscp copies between
      // sessions only through a local panel.
      if ((FClipboardSession != FSession.get()) || !FSession->Active())
      {
        return false;
      }
      const QString FileMask = (Drag.Names.size() == 1) ? Drag.Names.first() : QStringLiteral("*.*");
      RemoteTransferDialog Dialog(FSession.get(), false, Drag.Names.size() > 1, FPanel->Path(), FileMask, this);
      if (Dialog.exec() != QDialog::Accepted)
      {
        return false;
      }
      std::vector<std::string> Paths;
      for (const QString & Name : Drag.Names)
      {
        Paths.push_back((Drag.Directory + (Drag.Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name).toStdString());
      }
      FBusy = true;
      UpdateActions();
      bool Result = false;
      try
      {
        Result = FSession->RemoteCopyPaths(Paths, Dialog.Target().toStdString(), Dialog.FileMask().toStdString(), Dialog.DirectCopy());
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
    if (!Drag.Local)
    {
      // Our remote files: downloaded into a local panel, from the session
      // they were copied from (ClipboardDownload).
      if (!SessionOpen(FClipboardSession) || !FClipboardSession->Active())
      {
        return false;
      }
      std::vector<std::string> Paths;
      QStringList Names;
      for (const QString & Name : Drag.Names)
      {
        Paths.push_back((Drag.Directory + (Drag.Directory.endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + Name).toStdString());
        Names.append(Name);
      }
      QString Target = QDir::toNativeSeparators(Panel->Path());
      CoreBridge::TTransferSettings Settings = CoreBridge::CurrentTransferSettings();
      // As ClipboardDownload: asked as Environment's "Transferring of files".
      if (CoreBridge::Preferences().ConfirmTransferring && !CopyDialog(false, Names, Target, false, nullptr, Settings))
      {
        return false;
      }
      FBusy = true;
      UpdateActions();
      bool Result = false;
      try
      {
        Result = FClipboardSession->DownloadPaths(Paths, Target.toStdString(), &Settings);
      }
      catch (const CoreBridge::TCoreError & E)
      {
        ShowError(E);
      }
      FBusy = false;
      Panel->Refresh();
      UpdateActions();
      return Result;
    }
    for (const QString & Name : Drag.Names)
    {
      LocalSources.append(QDir(Drag.Directory).filePath(Name));
    }
  }
  else if (Data->hasUrls())
  {
    // Files another program copied.
    for (const QUrl & Url : Data->urls())
    {
      if (Url.isLocalFile())
      {
        LocalSources.append(QDir::cleanPath(Url.toLocalFile()));
      }
    }
  }
  if (!LocalSources.isEmpty())
  {
    if (RemoteTarget)
    {
      return Transfer(true, LocalSources, FPanel->Path(), false, true, false);
    }
    const QString Target = QDir::toNativeSeparators(Panel->Path());
    return LocalLocalCopy(false, true, &LocalSources, &Target);
  }
  // A line of text: a session URL opened (unsafe settings asked about),
  // else a path to go to.
  const QString Text = Data->text().trimmed();
  if (Text.isEmpty() || Text.contains(QLatin1Char('\n')))
  {
    return false;
  }
  const CoreBridge::TUrlInfo Url = CoreBridge::UrlInfo(Text.toStdString());
  if (Url.ProtocolDefined)
  {
    if (Url.UnsafeSettings)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = CoreBridge::Text(CoreBridge::TText::UnsafeSession);
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = "OK";
      CoreBridge::TQueryButton Cancel;
      Cancel.Answer = CoreBridge::AnswerCancel;
      Cancel.Caption = "Cancel";
      Query.Buttons = { OK, Cancel };
      Query.CancelAnswer = CoreBridge::AnswerCancel;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
      {
        return false;
      }
    }
    return OpenUrl(Text);
  }
  return RemoteTarget ? FPanel->ChangeDirectory(Text) : Panel->ChangeDirectory(Text);
}

void MainWindow::SetStatusBarShown(bool Shown)
{
  FStatusBarAction->setChecked(Shown);
  statusBar()->setVisible(Shown);
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  (FExplorer ? Layout.Explorer.StatusBar : Layout.StatusBar) = Shown;
  try
  {
    CoreBridge::SetLayout(Layout);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
}

void MainWindow::SetQueueShow(CoreBridge::TLayout::TQueueShow Show)
{
  FQueueShowActions[static_cast<int>(Show)]->setChecked(true);
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  Layout.QueueShow = Show;
  try
  {
    CoreBridge::SetLayout(Layout);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  UpdateQueueView();
}

// As TCustomScpExplorerForm::UpdateQueueView: shown, hidden, or shown while
// the queue has items.
void MainWindow::UpdateQueueView()
{
  if (FQueueShowActions[0] == nullptr)
  {
    return;
  }
  const bool Show = FQueueShowActions[static_cast<int>(CoreBridge::TLayout::TQueueShow::Show)]->isChecked() ||
    (FQueueShowActions[static_cast<int>(CoreBridge::TLayout::TQueueShow::HideWhenEmpty)]->isChecked() &&
     (FQueue->List()->topLevelItemCount() > 0));
  if (FQueue->isVisibleTo(this) != Show)
  {
    FQueue->setVisible(Show);
  }
}

// --- Icons (P3-17) ---------------------------------------------------------------------

QAction * MainWindow::Iconize(QAction * Action, const char * Role)
{
  if (Action != nullptr)
  {
    FIconRoles.emplace_back(Action, QString::fromLatin1(Role));
  }
  return Action;
}

void MainWindow::ApplyIcons()
{
  for (const auto & Role : FIconRoles)
  {
    Role.first->setIcon(IconSet::Icon(Role.second));
  }
  FLocal->ApplyIcons();
  FOtherLocal->ApplyIcons();
  FPanel->ApplyIcons();
  FQueue->ApplyIcons();
}

// --- Toolbars (P3-16) ---------------------------------------------------------------

namespace
{

// TWinConfiguration's default ScpCommander.ToolbarsLayout.
QString DefaultToolbarsLayout()
{
  return QStringLiteral("Queue=1::0+-1,Menu=1:TopDock:0+0,Preferences=1:TopDock:1+228,Session=0:TopDock:1+602,"
      "Sort=0:TopDock:2+0,Commands=1:TopDock:1+0,Updates=1:TopDock:1+596,Transfer=1:TopDock:1+341,"
      "CustomCommands=0:TopDock:3+0,RemoteHistory=1:RemoteTopDock:0+172,RemoteNavigation=1:RemoteTopDock:0+252,"
      "RemotePath=1:RemoteTopDock:0+0,RemoteFile=1:RemoteTopDock:1+0,RemoteSelection=1:RemoteTopDock:1+345,"
      "LocalHistory=1:LocalTopDock:0+207,LocalNavigation=1:LocalTopDock:0+287,LocalPath=1:LocalTopDock:0+0,"
      "LocalFile=1:LocalTopDock:1+0,LocalSelection=1:LocalTopDock:1+329,Toolbar2=0:BottomDock:1+0,"
      "CommandLine=0:BottomDock:0+0,");
}

}  // namespace

QToolBar * MainWindow::MainBand(const QString & Name, const QString & Title)
{
  QToolBar * Bar = addToolBar(Title);
  Bar->setObjectName(Name);
  TBand Band;
  Band.Name = Name;
  Band.Bar = Bar;
  FBands.push_back(Band);
  return Bar;
}

// A command of a panel's band: that panel made the active one, then the
// command, as WinSCP's Local... and Remote... actions are the side's.
QAction * MainWindow::SideAction(bool Local, QAction * Source, const QString & Text, const char * Role)
{
  QAction * Action = new QAction(Text, this);
  Iconize(Action, Role);
  connect(Action, &QAction::triggered, this, [this, Local, Source]()
  {
    ActivatePanel(Local);
    UpdateActions();
    if (Source->isEnabled())
    {
      Source->trigger();
    }
  });
  return Action;
}

void MainWindow::BandAction(QToolBar * Bar, QAction * Action, bool Text)
{
  Bar->addAction(Action);
  if (Text)
  {
    FTextButtons.emplace_back(Bar, Action);
  }
}

void MainWindow::BuildToolbars(QMenu * Options)
{
  // The main bands, WinSCP's Commander's (forms/ScpCommander.dfm), in its
  // default layout's order.
  QToolBar * Commands = MainBand(QStringLiteral("Commands"), tr("Commands"));
  BandAction(Commands, FCompareAction);
  BandAction(Commands, FKeepUpToDateAction);
  BandAction(Commands, FSynchronizeAction, true);
  Commands->addSeparator();
  BandAction(Commands, FConsoleAction);
  BandAction(Commands, FPuttyAction);
  Commands->addSeparator();
  BandAction(Commands, FSyncBrowseAction);

  QToolBar * Preferences = MainBand(QStringLiteral("Preferences"), tr("Preferences"));
  BandAction(Preferences, FPreferencesAction);
  Preferences->addSeparator();
  // As QueueToggleShowAction: the queue shown or not, its menu the choices.
  QAction * Queue = new QAction(tr("&Queue"), this);
  Iconize(Queue, "queue");
  // (Options > Queue's choices, made after the bands: listed as it opens.)
  QMenu * QueueMenu = new QMenu(this);
  connect(QueueMenu, &QMenu::aboutToShow, this, [this, QueueMenu]()
  {
    QueueMenu->clear();
    for (int Show = 0; Show < 3; Show++)
    {
      QueueMenu->addAction(FQueueShowActions[Show]);
    }
  });
  Queue->setMenu(QueueMenu);
  connect(Queue, &QAction::triggered, this, [this]()
  {
    SetQueueShow(FQueue->isVisible() ? CoreBridge::TLayout::TQueueShow::Hide : CoreBridge::TLayout::TQueueShow::Show);
  });
  BandAction(Preferences, Queue, true);

  QToolBar * Transfer = MainBand(QStringLiteral("Transfer"), tr("Transfer Settings"));
  // The presets, the one in use current (TransferCombo).
  FPresetCombo = new QComboBox(Transfer);
  FPresetCombo->setAccessibleName(tr("Transfer settings"));
  FPresetCombo->setMinimumContentsLength(14);
  FPresetCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  connect(FPresetCombo, &QComboBox::activated, this, [this](int Index)
  {
    try
    {
      const QString Name = FPresetCombo->itemData(Index).toString();
      // As TransferListChange: chosen while none was autoselected, the
      // one to return to when no rule matches.
      if (FCopyParamAutoSelected.isEmpty())
      {
        FCopyParamDefault = Name;
      }
      CoreBridge::SetCurrentTransferPreset(Name.toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
  });
  Transfer->addWidget(FPresetCombo);
  QAction * Configure = new QAction(tr("&Configure..."), this);
  Iconize(Configure, "preferences");
  connect(Configure, &QAction::triggered, this, [this]() { PreferencesDialog(::PreferencesDialog::tr("Transfer")); UpdatePresetCombo(); });
  BandAction(Transfer, Configure, true);
  UpdatePresetCombo();

  addToolBarBreak();
  QToolBar * Session = MainBand(QStringLiteral("Session"), tr("Sessions and Tabs"));
  QAction * NewTab = new QAction(tr("&New Tab"), this);
  Iconize(NewTab, "new-tab");
  QMenu * NewTabMenu = new QMenu(this);
  for (QAction * Tab : menuBar()->findChildren<QAction *>())
  {
    if ((Tab->text() == tr("&Remote Tab...")) || (Tab->text() == tr("&Local Tab")))
    {
      NewTabMenu->addAction(Tab);
    }
  }
  NewTab->setMenu(NewTabMenu);
  connect(NewTab, &QAction::triggered, this, [this]()
  {
    NewDefaultTab(QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ControlModifier));
  });
  BandAction(Session, NewTab, true);
  BandAction(Session, FSaveSiteAction);
  Session->addSeparator();
  BandAction(Session, FDuplicateTabAction);
  BandAction(Session, FCloseTabAction);
  Session->addSeparator();
  BandAction(Session, FSitesAction, true);

  // Sort: the active panel's, by its columns (WinSCP's CurrentSortBy...).
  QToolBar * Sort = MainBand(QStringLiteral("Sort"), tr("Sort"));
  FSortActions[0] = new QAction(tr("&Ascending"), this);
  FSortActions[0]->setCheckable(true);
  connect(FSortActions[0], &QAction::triggered, this, [this](bool Ascending)
  {
    QHeaderView * Header = ActiveView()->header();
    ActiveView()->sortByColumn(Header->sortIndicatorSection(), Ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
  });
  BandAction(Sort, FSortActions[0]);
  Sort->addSeparator();
  const std::pair<int, QString> Columns[] = {
    { FileListModel::NameColumn, tr("By &Name") }, { FileListModel::SizeColumn, tr("By &Size") },
    { FileListModel::ModifiedColumn, tr("By Date &modified") }, { FileListModel::RightsColumn, tr("By &Permissions/Attributes") },
    { FileListModel::OwnerColumn, tr("By &Owner") }, { FileListModel::GroupColumn, tr("By &Group") } };
  QActionGroup * SortGroup = new QActionGroup(Sort);
  for (int Index = 0; Index < 6; Index++)
  {
    const int Column = Columns[Index].first;
    QAction * By = new QAction(Columns[Index].second, this);
    By->setCheckable(true);
    SortGroup->addAction(By);
    connect(By, &QAction::triggered, this, [this, Column]()
    {
      ActiveView()->sortByColumn(Column, ActiveView()->header()->sortIndicatorOrder());
    });
    FSortActions[Index + 1] = By;
    BandAction(Sort, By);
  }
  const char * const SortRoles[] = { "sort-ascending", "sort-name", "sort-size", "sort-time", "sort-rights", "sort-owner",
    "sort-group" };
  for (int Index = 0; Index < 7; Index++)
  {
    Iconize(FSortActions[Index], SortRoles[Index]);
  }
  ApplyIcons();

  FCustomCommandsBar = MainBand(QStringLiteral("CustomCommands"), tr("Custom Commands"));
  UpdateCustomCommands();

  // As UpdatesToolbar: Check for Updates, its menu the project's pages.
  QToolBar * Updates = MainBand(QStringLiteral("Updates"), tr("Updates"));
  QAction * UpdatesButton = new QAction(tr("&Check for Updates"), this);
  Iconize(UpdatesButton, "download");
  connect(UpdatesButton, &QAction::triggered, this, [this]() { CheckForUpdates(); });
  QMenu * UpdatesMenu = new QMenu(this);
  UpdatesMenu->addAction(tr("&Check for Updates"), this, [this]() { CheckForUpdates(); });
  UpdatesMenu->addSeparator();
  UpdatesMenu->addAction(tr("Product &Homepage"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp")); });
  UpdatesMenu->addAction(tr("&Download"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp/releases/latest")); });
  UpdatesMenu->addAction(tr("&Support Forum"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp/issues")); });
  UpdatesMenu->addAction(tr("&Version History"), this,
    [this]() { OpenWebPage(QStringLiteral("https://github.com/99ecarvalho/linscp/releases")); });
  UpdatesMenu->addAction(tr("D&onate"), this, [this]() { OpenWebPage(QString::fromLatin1(DonateUrl)); });
  UpdatesButton->setMenu(UpdatesMenu);
  BandAction(Updates, UpdatesButton);

  // As Toolbar2Toolbar ("Hot Keys"), at the bottom: the F-keys' commands,
  // the copy and move ones the active side's.
  FHotKeysBar = new QToolBar(tr("Hot Keys"), this);
  FHotKeysBar->setObjectName(QStringLiteral("Toolbar2"));
  FHotKeysBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  addToolBar(Qt::BottomToolBarArea, FHotKeysBar);
  {
    auto OnActiveSide = [this](QAction * (LocalPanel::*LocalAction)() const, QAction * (RemotePanel::*RemoteAction)() const)
    {
      return [this, LocalAction, RemoteAction]()
      {
        LocalPanel * Panel = SideLocal(FLocalActive);
        ((Panel != nullptr) ? (Panel->*LocalAction)() : (FPanel->*RemoteAction)())->trigger();
      };
    };
    FHotKeysBar->addAction(tr("F2 Rename"), this, OnActiveSide(&LocalPanel::RenameAction, &RemotePanel::RenameAction));
    FHotKeysBar->addAction(tr("F4 Edit"), this, [this]() { FEditAction->trigger(); });
    FHotKeyCopy = FHotKeysBar->addAction(tr("F5 Copy"), this, [this]() { FCopyAction->trigger(); });
    FHotKeyMove = FHotKeysBar->addAction(tr("F6 Move"), this, [this]() { FMoveAction->trigger(); });
    FHotKeysBar->addAction(tr("F7 Create Directory"), this, OnActiveSide(&LocalPanel::NewFolderAction, &RemotePanel::NewFolderAction));
    FHotKeysBar->addAction(tr("F8 Delete"), this, OnActiveSide(&LocalPanel::DeleteAction, &RemotePanel::DeleteAction));
    FHotKeysBar->addAction(tr("F9 Properties"), this, [this]() { FPropertiesAction->trigger(); });
    FHotKeysBar->addAction(tr("F10 Quit"), this, [this]() { FQuitAction->trigger(); });
  }
  {
    TBand Band;
    Band.Name = QStringLiteral("Toolbar2");
    Band.Bar = FHotKeysBar;
    FBands.push_back(Band);
  }

  // The command line, at the bottom (WinSCP's BottomDock), as a band.
  FCommandLineBar = new QToolBar(tr("Command Line"), this);
  FCommandLineBar->setObjectName(QStringLiteral("CommandLine"));
  addToolBar(Qt::BottomToolBarArea, FCommandLineBar);
  FCommandPrompt = new QLabel(FCommandLineBar);
  FCommandLine = new QComboBox(FCommandLineBar);
  FCommandLine->setEditable(true);
  FCommandLine->setInsertPolicy(QComboBox::NoInsert);
  FCommandLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  FCommandLine->setAccessibleName(tr("Command line"));
  FCommandPrompt->setBuddy(FCommandLine);
  FCommandLineBar->addWidget(FCommandPrompt);
  FCommandLineBar->addWidget(FCommandLine);
  connect(FCommandLine->lineEdit(), &QLineEdit::returnPressed, this, [this]() { ExecuteCommandLine(); });
  QShortcut * Escape = new QShortcut(QKeySequence(Qt::Key_Escape), FCommandLine->lineEdit(), nullptr, nullptr, Qt::WidgetShortcut);
  connect(Escape, &QShortcut::activated, this, [this]()
  {
    FCommandLine->clearEditText();
    ActivatePanel(FLocalActive);
  });
  QShortcut * Tab = new QShortcut(QKeySequence(Qt::Key_Tab), FCommandLine->lineEdit(), nullptr, nullptr, Qt::WidgetShortcut);
  connect(Tab, &QShortcut::activated, this, [this]() { ActivatePanel(FLocalActive); });
  FCommandLineAction = new QAction(tr("Comma&nd Line"), this);
  FCommandLineAction->setCheckable(true);
  FCommandLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
  FCommandLineAction->setStatusTip(tr("Hide/show command line"));
  connect(FCommandLineAction, &QAction::triggered, this, [this](bool Shown)
  {
    FCommandLineBar->setVisible(Shown);
    if (Shown)
    {
      UpdateCommandLine();
      FCommandLine->setFocus();
    }
    SaveToolbarSettings();
  });
  {
    TBand Band;
    Band.Name = QStringLiteral("CommandLine");
    Band.Bar = FCommandLineBar;
    Band.Toggle = FCommandLineAction;
    FBands.push_back(Band);
  }

  // The panels' bands.
  struct TSide
  {
    bool Left;
    LocalPanel * Local;
    RemotePanel * Remote;
  };
  for (const TSide & Side : { TSide{ true, FLocal, nullptr }, TSide{ false, FOtherLocal, nullptr }, TSide{ false, nullptr, FPanel } })
  {
    auto BandOf = [&Side](TPanelBand Which) { return (Side.Local != nullptr) ? Side.Local->Band(Which) : Side.Remote->Band(Which); };
    const bool Left = Side.Left;
    QToolBar * Path = BandOf(TPanelBand::Path);
    QAction * OpenDirectory = SideAction(Left, FOpenDirectoryAction, tr("&Open Directory/Bookmark..."), "open-directory");
    QMenu * OpenDirMenu = new QMenu(this);
    connect(OpenDirMenu, &QMenu::aboutToShow, this, [this, OpenDirMenu, Left, OpenDirectory]()
    {
      FillOpenDirMenu(OpenDirMenu, Left, OpenDirectory);
    });
    OpenDirectory->setMenu(OpenDirMenu);
    BandAction(Path, OpenDirectory);
    if (QToolButton * Button = qobject_cast<QToolButton *>(Path->widgetForAction(OpenDirectory)))
    {
      Button->setPopupMode(QToolButton::MenuButtonPopup);
    }
    BandAction(Path, SideAction(Left, FFilterAction, tr("&Filter..."), "filter"));
    if (Side.Remote != nullptr)
    {
      // As RemoteNavigationToolbar: Find Files after the panel's own.
      QToolBar * Navigation = BandOf(TPanelBand::Navigation);
      Navigation->addSeparator();
      BandAction(Navigation, SideAction(false, FFindFilesAction, tr("&Find Files..."), "find"), true);
    }
    QToolBar * File = BandOf(TPanelBand::File);
    // Upload from the left, download to it; in a local tab, copy.
    QAction * Copy = SideAction(Left, FCopyAction, Left ? tr("Up&load...") : ((Side.Remote != nullptr) ? tr("Down&load...") : tr("&Copy...")),
      Left ? "upload" : ((Side.Remote != nullptr) ? "download" : "copy"));
    QAction * Move = SideAction(Left, FMoveAction, Left ? tr("Upload and Dele&te...") :
      ((Side.Remote != nullptr) ? tr("Download and Dele&te...") : tr("&Move...")), "move");
    if (Left)
    {
      FLeftCopyAction = Copy;
      FLeftMoveAction = Move;
    }
    BandAction(File, Copy, true);
    File->addSeparator();
    BandAction(File, Move);
    File->addSeparator();
    BandAction(File, SideAction(Left, FEditAction, tr("&Edit"), "edit"), true);
    BandAction(File, (Side.Local != nullptr) ? Side.Local->DeleteAction() : Side.Remote->DeleteAction());
    BandAction(File, (Side.Local != nullptr) ? Side.Local->RenameAction() : Side.Remote->RenameAction());
    BandAction(File, SideAction(Left, FPropertiesAction, tr("&Properties"), "properties"), true);
    File->addSeparator();
    BandAction(File, SideAction(Left, FDirectorySizesAction, tr("&Calculate Directory Sizes"), "calculate-size"), true);
    File->addSeparator();
    BandAction(File, SideAction(Left, FNewFileAction, tr("&File..."), "new-file"));
    BandAction(File, (Side.Local != nullptr) ? Side.Local->NewFolderAction() : Side.Remote->NewFolderAction());
    BandAction(File, SideAction(Left, FLinkAction, (Side.Local != nullptr) ? tr("&Shortcut...") : tr("&Link..."), "link"));
    QToolBar * Selection = BandOf(TPanelBand::Selection);
    for (const auto & Select : { std::make_pair(tr("Sele&ct Files..."), "select-files"),
           std::make_pair(tr("&Unselect Files..."), "unselect-files"), std::make_pair(tr("Select &All"), "select-all") })
    {
      QAction * Action = new QAction(Select.first, this);
      Iconize(Action, Select.second);
      const QString Role = QString::fromLatin1(Select.second);
      connect(Action, &QAction::triggered, this, [this, Left, Role]()
      {
        ActivatePanel(Left);
        FileView * View = ActiveView();
        if (Role == QStringLiteral("select-all"))
        {
          View->selectAll();
        }
        else
        {
          View->SelectByMaskDialog(Role == QStringLiteral("select-files"));
        }
      });
      BandAction(Selection, Action);
    }
  }
  // The bands' names, as WinSCP's layout has them; the other local panel's
  // follow the left one's.
  const std::pair<TPanelBand, const char *> PanelBands[] = {
    { TPanelBand::History, "History" }, { TPanelBand::Navigation, "Navigation" }, { TPanelBand::Path, "Path" },
    { TPanelBand::File, "File" }, { TPanelBand::Selection, "Selection" } };
  for (const auto & Band : PanelBands)
  {
    TBand Local;
    Local.Name = QStringLiteral("Local") + QString::fromLatin1(Band.second);
    Local.Bar = FLocal->Band(Band.first);
    FBands.push_back(Local);
    TBand Remote;
    Remote.Name = QStringLiteral("Remote") + QString::fromLatin1(Band.second);
    Remote.Bar = FPanel->Band(Band.first);
    Remote.Twin = FOtherLocal->Band(Band.first);
    FBands.push_back(Remote);
  }

  // Options > Toolbars, Left Panel, Right Panel (WinSCP's order).
  QMenu * Toolbars = Options->addMenu(tr("&Toolbars"));
  QMenu * LeftPanel = new QMenu(tr("&Left Panel"), this);
  QMenu * RightPanel = new QMenu(tr("&Right Panel"), this);
  const std::pair<const char *, QString> Toggles[] = {
    { "Commands", tr("&Commands Buttons") }, { "Session", tr("Sessio&ns and Tabs Buttons") },
    { "Preferences", tr("&Preferences Buttons") }, { "Sort", tr("So&rt Buttons") }, { "Updates", tr("&Update Button") },
    { "Transfer", tr("&Transfer Settings") }, { "CustomCommands", tr("Custom Co&mmand Buttons") },
    { "LocalHistory", tr("&History Buttons") }, { "LocalNavigation", tr("&Navigation Buttons") },
    { "LocalFile", tr("&File Buttons") }, { "LocalSelection", tr("Se&lection Buttons") },
    { "RemoteHistory", tr("&History Buttons") }, { "RemoteNavigation", tr("&Navigation Buttons") },
    { "RemoteFile", tr("&File Buttons") }, { "RemoteSelection", tr("Se&lection Buttons") } };
  for (const auto & Toggle : Toggles)
  {
    for (TBand & Band : FBands)
    {
      if (Band.Name == QString::fromLatin1(Toggle.first))
      {
        QMenu * Menu = Band.Name.startsWith(QStringLiteral("Local")) ? LeftPanel :
          (Band.Name.startsWith(QStringLiteral("Remote")) ? RightPanel : Toolbars);
        Band.Toggle = Menu->addAction(Toggle.second);
        Band.Toggle->setCheckable(true);
        QToolBar * Bar = Band.Bar;
        QToolBar * Twin = Band.Twin;
        connect(Band.Toggle, &QAction::triggered, this, [this, Bar, Twin](bool Shown)
        {
          Bar->setVisible(Shown);
          if (Twin != nullptr)
          {
            Twin->setVisible(Shown);
          }
          SaveToolbarSettings();
        });
      }
    }
  }
  // As ToolBar2Action: the Hot Keys toolbar.
  Toolbars->addSeparator();
  QAction * HotKeys = Toolbars->addAction(tr("&Hot Keys Toolbar"));
  HotKeys->setCheckable(true);
  HotKeys->setStatusTip(tr("Hide/show hot keys toolbar"));
  for (TBand & Band : FBands)
  {
    if (Band.Name == QStringLiteral("Toolbar2"))
    {
      Band.Toggle = HotKeys;
    }
  }
  connect(HotKeys, &QAction::triggered, this, [this](bool Shown)
  {
    FHotKeysBar->setVisible(Shown);
    SaveToolbarSettings();
  });
  Toolbars->addSeparator();
  FLockToolbarsAction = Toolbars->addAction(tr("&Lock Toolbars"));
  FLockToolbarsAction->setCheckable(true);
  FSelectiveTextAction = Toolbars->addAction(tr("&Show Selective Text Labels"));
  FSelectiveTextAction->setCheckable(true);
  QMenu * IconSize = Toolbars->addMenu(tr("T&oolbar Icons Size"));
  QActionGroup * SizeGroup = new QActionGroup(IconSize);
  const QString Sizes[] = { tr("&Normal"), tr("&Large"), tr("&Very Large") };
  for (int Size = 0; Size < 3; Size++)
  {
    FIconSizeActions[Size] = IconSize->addAction(Sizes[Size]);
    FIconSizeActions[Size]->setCheckable(true);
    SizeGroup->addAction(FIconSizeActions[Size]);
    connect(FIconSizeActions[Size], &QAction::triggered, this, [this]() { ApplyToolbarSettings(); SaveToolbarSettings(); });
  }
  for (QAction * Action : { FLockToolbarsAction, FSelectiveTextAction })
  {
    connect(Action, &QAction::triggered, this, [this]() { ApplyToolbarSettings(); SaveToolbarSettings(); });
  }
  // As LocalTreeAction and RemoteTreeAction: each panel's tree (the right
  // side's both panels', one shown); Ctrl+Alt+T the active side's.
  for (QMenu * Menu : { LeftPanel, RightPanel })
  {
    const bool Left = (Menu == LeftPanel);
    Menu->addSeparator();
    QAction * Tree = Menu->addAction(tr("&Tree"));
    Tree->setCheckable(true);
    Tree->setStatusTip(tr("Hide/show directory tree"));
    (Left ? FLocalTreeAction : FRemoteTreeAction) = Tree;
    connect(Tree, &QAction::triggered, this, [this, Left](bool Shown) { SetTreeVisible(Left, Shown); });
    // As LocalStatusBarAction2 and RemoteStatusBarAction2: the panel's status line.
    Menu->addSeparator();
    QAction * Status = Menu->addAction(tr("&Status Bar"));
    Status->setCheckable(true);
    Status->setStatusTip(tr("Hide/show panel status bar"));
    (Left ? FLocalStatusBarAction : FRemoteStatusBarAction) = Status;
    connect(Status, &QAction::triggered, this, [this, Left](bool Shown)
    {
      SetPanelStatusBarVisible(Left, Shown);
      StoreLayoutChoice([Left, Shown](CoreBridge::TLayout & Layout) { (Left ? Layout.LocalStatusBar : Layout.RemoteStatusBar) = Shown; });
    });
  }
  Options->addMenu(LeftPanel);
  Options->addMenu(RightPanel);
  Options->addSeparator();

  // As stored: WinSCP's default layout when there is none.
  const CoreBridge::TLayout Layout = CoreBridge::Layout();
  QString Stored = QString::fromStdString(Layout.ToolbarsLayout);
  if (Stored.isEmpty())
  {
    Stored = DefaultToolbarsLayout();
  }
  for (TBand & Band : FBands)
  {
    const QRegularExpression Entry(QStringLiteral("(?:^|,)%1=(\\d)").arg(Band.Name));
    const QRegularExpressionMatch Match = Entry.match(Stored);
    const bool Shown = !Match.hasMatch() || (Match.captured(1) != QStringLiteral("0"));
    Band.Bar->setVisible(Shown);
    if (Band.Twin != nullptr)
    {
      Band.Twin->setVisible(Shown);
    }
    if (Band.Toggle != nullptr)
    {
      Band.Toggle->setChecked(Shown);
    }
  }
  FLockToolbarsAction->setChecked(Layout.LockToolbars);
  FSelectiveTextAction->setChecked(Layout.SelectiveToolbarText);
  FIconSizeActions[std::clamp(Layout.LargerToolbar, 0, 2)]->setChecked(true);
  ApplyToolbarSettings();
}

// Locked, the labels, the icons' size: WinSCP's DPI steps (96, 120, 144)
// of its 16-pixel icons.
void MainWindow::ApplyToolbarSettings()
{
  int Larger = 0;
  for (int Size = 0; Size < 3; Size++)
  {
    Larger = FIconSizeActions[Size]->isChecked() ? Size : Larger;
  }
  const int Pixels = 16 * (96 + 24 * Larger) / 96;
  for (const TBand & Band : FBands)
  {
    for (QToolBar * Bar : { Band.Bar, Band.Twin })
    {
      if (Bar != nullptr)
      {
        Bar->setIconSize(QSize(Pixels, Pixels));
        if (Band.Name.indexOf(QStringLiteral("Local")) != 0 && Band.Name.indexOf(QStringLiteral("Remote")) != 0)
        {
          Bar->setMovable(!FLockToolbarsAction->isChecked());
        }
      }
    }
  }
  for (const auto & Text : FTextButtons)
  {
    if (QToolButton * Button = qobject_cast<QToolButton *>(Text.first->widgetForAction(Text.second)))
    {
      Button->setToolButtonStyle(FSelectiveTextAction->isChecked() ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    }
  }
}

// As WinSCP keeps it: each band's entry ("Name=shown:dock:row+pos"), the
// bands linscp has not (Menu, Updates, ...) kept as they were.
void MainWindow::SaveToolbarSettings()
{
  CoreBridge::TLayout Layout = CoreBridge::Layout();
  // (The Explorer's own, its bands all in the top dock.)
  const QString Stored = QString::fromStdString(FExplorer ? Layout.Explorer.ToolbarsLayout : Layout.ToolbarsLayout);
  QStringList Entries = (Stored.isEmpty() ? (FExplorer ? DefaultExplorerToolbarsLayout() : DefaultToolbarsLayout()) : Stored)
    .split(QLatin1Char(','), Qt::SkipEmptyParts);
  for (const TBand & Band : FBands)
  {
    const QString Shown = Band.Bar->isVisibleTo(this) || ((Band.Toggle != nullptr) && Band.Toggle->isChecked()) ?
      QStringLiteral("1") : QStringLiteral("0");
    const QString Dock = Band.Name.startsWith(QStringLiteral("Local")) ? QStringLiteral("LocalTopDock") :
      (Band.Name.startsWith(QStringLiteral("Remote")) ? QStringLiteral("RemoteTopDock") : QStringLiteral("TopDock"));
    bool Found = false;
    for (QString & Entry : Entries)
    {
      if (Entry.startsWith(Band.Name + QLatin1Char('=')))
      {
        // The rest (dock, row, position) as it was.
        const int Colon = Entry.indexOf(QLatin1Char(':'));
        Entry = Band.Name + QLatin1Char('=') + Shown + ((Colon > 0) ? Entry.mid(Colon) : (QLatin1Char(':') + Dock + QStringLiteral(":0+0")));
        Found = true;
      }
    }
    if (!Found)
    {
      Entries.append(Band.Name + QLatin1Char('=') + Shown + QLatin1Char(':') + Dock + QStringLiteral(":0+0"));
    }
  }
  (FExplorer ? Layout.Explorer.ToolbarsLayout : Layout.ToolbarsLayout) = (Entries.join(QLatin1Char(',')) + QLatin1Char(',')).toStdString();
  Layout.LockToolbars = FLockToolbarsAction->isChecked();
  Layout.SelectiveToolbarText = FSelectiveTextAction->isChecked();
  for (int Size = 0; Size < 3; Size++)
  {
    Layout.LargerToolbar = FIconSizeActions[Size]->isChecked() ? Size : Layout.LargerToolbar;
  }
  try
  {
    CoreBridge::SetLayout(Layout);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
}

void MainWindow::UpdatePresetCombo()
{
  if (FPresetCombo == nullptr)
  {
    return;
  }
  FPresetCombo->clear();
  FPresetCombo->addItem(tr("Default transfer settings"), QString());
  for (const CoreBridge::TTransferPreset & Preset : CoreBridge::TransferPresets())
  {
    FPresetCombo->addItem(PresetCaption(Preset.Name), QString::fromStdString(Preset.Name));
  }
  FPresetCombo->setCurrentIndex(std::max(0, FPresetCombo->findData(QString::fromStdString(CoreBridge::CurrentTransferPreset()))));
}

void MainWindow::UpdateSortActions()
{
  if (FSortActions[0] == nullptr)
  {
    return;
  }
  const QHeaderView * Header = ActiveView()->header();
  FSortActions[0]->setChecked(Header->sortIndicatorOrder() == Qt::AscendingOrder);
  const int Columns[] = { FileListModel::NameColumn, FileListModel::SizeColumn, FileListModel::ModifiedColumn,
    FileListModel::RightsColumn, FileListModel::OwnerColumn, FileListModel::GroupColumn };
  for (int Index = 0; Index < 6; Index++)
  {
    FSortActions[Index + 1]->setChecked(Header->sortIndicatorSection() == Columns[Index]);
  }
}

void MainWindow::FillOpenDirMenu(QMenu * Menu, bool Left, QAction * OpenAction)
{
  Menu->clear();
  Menu->addAction(OpenAction);
  Menu->setDefaultAction(OpenAction);
  Menu->addSeparator();
  // (A local tab's right side is local too.)
  const bool LocalSide = Left || LocalBrowser();
  const bool Profiles = CoreBridge::Preferences().UseLocationProfiles && FSession;
  auto Go = [this, Left, LocalSide, Profiles](const CoreBridge::TBookmarkEntry & Bookmark)
  {
    ActivatePanel(Left);
    // As OpenBookmark: a profile's both directories, else the side's.
    if (Profiles)
    {
      if (!Bookmark.Local.empty())
      {
        GoLocal(QString::fromStdString(Bookmark.Local));
      }
      if (!Bookmark.Remote.empty() && !LocalBrowser())
      {
        GoRemote(QString::fromStdString(Bookmark.Remote));
      }
    }
    else
    {
      const std::string & Directory = LocalSide ? Bookmark.Local : Bookmark.Remote;
      LocalSide ? GoLocal(QString::fromStdString(Directory)) : GoRemote(QString::fromStdString(Directory));
    }
  };
  std::vector<std::vector<CoreBridge::TBookmarkEntry>> Lists;
  if (FSession)
  {
    Lists.push_back(CoreBridge::Bookmarks(FSession->SessionKey()));
  }
  Lists.push_back(CoreBridge::Bookmarks(std::string()));
  for (const std::vector<CoreBridge::TBookmarkEntry> & List : Lists)
  {
    bool Any = false;
    if (!Profiles)
    {
      // The side's directories, each once.
      QStringList Seen;
      for (const CoreBridge::TBookmarkEntry & Bookmark : List)
      {
        const QString Directory = QString::fromStdString(LocalSide ? Bookmark.Local : Bookmark.Remote);
        if (!Directory.isEmpty() && !Seen.contains(Directory))
        {
          Seen.append(Directory);
          QString Caption = Directory;
          Menu->addAction(Caption.replace(QLatin1Char('&'), QStringLiteral("&&")), this, [Go, Bookmark]() { Go(Bookmark); });
          Any = true;
        }
      }
    }
    else
    {
      // Profiles in their folders, as the dialog has them.
      QMap<QString, QMenu *> Folders;
      for (const CoreBridge::TBookmarkEntry & Bookmark : List)
      {
        const QString Node = QString::fromStdString(Bookmark.Node);
        if (!Node.isEmpty() && !Folders.contains(Node))
        {
          Folders[Node] = nullptr;
        }
      }
      for (auto It = Folders.begin(); It != Folders.end(); ++It)
      {
        It.value() = Menu->addMenu(IconSet::Icon(QStringLiteral("open-directory")), It.key());
      }
      for (const CoreBridge::TBookmarkEntry & Bookmark : List)
      {
        const QString Node = QString::fromStdString(Bookmark.Node);
        QMenu * Parent = Node.isEmpty() ? Menu : Folders[Node];
        QString Caption = QString::fromStdString(Bookmark.Name);
        QAction * Item = Parent->addAction(Caption.replace(QLatin1Char('&'), QStringLiteral("&&")), this,
          [Go, Bookmark]() { Go(Bookmark); });
        if (Bookmark.ShortCut != 0)
        {
          Item->setShortcut(QKeySequence::fromString(ShortCutText(Bookmark.ShortCut), QKeySequence::NativeText));
        }
        Any = true;
      }
    }
    if (Any)
    {
      Menu->addSeparator();
    }
  }
  Menu->addAction(IconSet::Icon(QStringLiteral("bookmark")), tr("&Add Path to Bookmarks"), this, [this, Left]()
  {
    ActivatePanel(Left);
    OpenDirectory(true);
  });
}

bool MainWindow::FileActivated(bool Local)
{
  if (FBusy)
  {
    return false;
  }
  ActivatePanel(Local);
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  switch (Preferences.DoubleClickAction)
  {
    case CoreBridge::TPreferences::TDoubleClick::Open:
      return OpenCurrent();
    case CoreBridge::TPreferences::TDoubleClick::Copy:
    {
      // The file double-clicked, to the other panel (OnFocused).
      const CoreBridge::TRemoteEntry * Entry = ActiveView()->CurrentEntry();
      if (Entry == nullptr)
      {
        return false;
      }
      const QString Name = QString::fromStdString(Entry->Name);
      const bool Confirm = Preferences.CopyOnDoubleClickConfirmation;
      if (LocalBrowser())
      {
        const QStringList Sources = { QDir(ActiveLocalPath()).filePath(Name) };
        const QString Target = QDir::toNativeSeparators(SideLocal(!FLocalActive)->Path());
        return LocalLocalCopy(false, Confirm, &Sources, &Target);
      }
      const QStringList Names = { FLocalActive ? QDir(FLocal->Path()).filePath(Name) : Name };
      return Transfer(FLocalActive, Names, FLocalActive ? FPanel->Path() : QDir::toNativeSeparators(FLocal->Path()),
        false, Confirm, false);
    }
    default:
      return EditCurrent() != nullptr;
  }
}
