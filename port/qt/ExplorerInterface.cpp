// The Explorer interface (P3-21), as WinSCP's TScpExplorerForm
// (forms/ScpExplorer.cpp, .dfm): the remote panel alone, its tree beside
// it, the address bar, Explorer's menus, toolbars and keys
// (NonVisual's ExplorerShortcuts), downloads to the last local target
// directory. The same window and actions as the Commander's, laid out as
// the Explorer has them, from its start (Preferences > Interface applies on
// the next one, as WinSCP's).
#include "MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

#include "FileList.h"
#include "IconSet.h"
#include "LocalPanel.h"
#include "PreferencesDialog.h"
#include "QueuePanel.h"
#include "RemotePanel.h"

namespace
{

// A menu's submenu, by its caption.
QMenu * SubMenu(const QList<QAction *> & Actions, const QString & Title)
{
  for (QAction * Action : Actions)
  {
    if ((Action->menu() != nullptr) && (Action->text() == Title))
    {
      return Action->menu();
    }
  }
  return nullptr;
}

QAction * ActionOf(const QList<QAction *> & Actions, const QString & Text)
{
  for (QAction * Action : Actions)
  {
    if (Action->text() == Text)
    {
      return Action;
    }
  }
  return nullptr;
}

}  // namespace

// TWinConfiguration's default ScpExplorer.ToolbarsLayout.
QString MainWindow::DefaultExplorerToolbarsLayout()
{
  return QStringLiteral(
    "Queue=1::0+-1,Menu=1:TopDock:0+0,Buttons=1:TopDock:2+0,Selection=0:TopDock:3+0,Session=0:TopDock:6+0,"
    "Preferences=1:TopDock:4+0,Sort=0:TopDock:5+0,Address=1:TopDock:1+0,Updates=1:TopDock:4+393,"
    "Transfer=1:TopDock:4+171,CustomCommands=0:TopDock:7+0,");
}

QString MainWindow::DocumentsDirectory()
{
  const QString Documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  return QDir(Documents).exists() ? Documents : QDir::homePath();
}

void MainWindow::ApplyExplorerInterface()
{
  const CoreBridge::TLayout Layout = CoreBridge::Layout();

  // The remote panel alone; the local one hidden, kept where downloads go
  // (ScpExplorer.LastLocalTargetDirectory).
  FLocal->hide();
  FOtherLocal->hide();
  QString Target = QString::fromStdString(Layout.Explorer.LastLocalTargetDirectory);
  if (Target.isEmpty() || !QDir(Target).exists())
  {
    Target = DocumentsDirectory();
  }
  FLocal->ChangeDirectory(Target);
  FPanel->SetTreeBeside(true);
  for (TPanelBand Which : { TPanelBand::History, TPanelBand::Navigation, TPanelBand::Path, TPanelBand::File, TPanelBand::Selection })
  {
    FPanel->Band(Which)->hide();
  }
  // The address bar in the panel's path's place.
  FPanel->SetPathBarVisible(false);

  // The Commander's bands out; those the Explorer shares kept.
  QHash<QString, QToolBar *> Shared;
  for (const TBand & Band : FBands)
  {
    Band.Bar->hide();
    if (Band.Twin != nullptr)
    {
      Band.Twin->hide();
    }
    if (QStringList({ QStringLiteral("Session"), QStringLiteral("Sort"), QStringLiteral("Transfer"),
          QStringLiteral("CustomCommands"), QStringLiteral("Updates") }).contains(Band.Name))
    {
      Shared.insert(Band.Name, Band.Bar);
    }
    else if (Band.Bar->parentWidget() == this)
    {
      removeToolBar(Band.Bar);
    }
  }
  // (The Commander's Queue button, for the Explorer's Preferences band.)
  QToolBar * CommanderPreferences = nullptr;
  for (const TBand & Band : FBands)
  {
    if (Band.Name == QStringLiteral("Preferences"))
    {
      CommanderPreferences = Band.Bar;
    }
  }
  QAction * QueueButton = (CommanderPreferences != nullptr) ? ActionOf(CommanderPreferences->actions(), tr("&Queue")) : nullptr;
  FBands.clear();
  for (QToolBar * Bar : Shared)
  {
    removeToolBar(Bar);
  }

  // The view styles (Remote...Action: Icon, SmallIcon, List, Report,
  // Thumbnail), as the View menu and the Preferences band have them.
  QActionGroup * Styles = new QActionGroup(this);
  const std::pair<QString, const char *> StyleTexts[] = {
    { tr("&Large Icons"), "view-icons" }, { tr("&Small Icons"), "view-small-icons" }, { tr("Lis&t"), "view-list" },
    { tr("&Details"), "view-details" }, { tr("&Thumbnails"), "view-thumbnails" } };
  for (int Style = 0; Style < 5; Style++)
  {
    QAction * Action = new QAction(StyleTexts[Style].first, this);
    Iconize(Action, StyleTexts[Style].second);
    Action->setCheckable(true);
    Styles->addAction(Action);
    connect(Action, &QAction::triggered, this, [this, Style]()
    {
      const auto ViewStyle = static_cast<RemotePanel::TViewStyle>(Style);
      FPanel->SetViewStyle(ViewStyle);
      StoreLayoutChoice([ViewStyle](CoreBridge::TLayout & Stored) { Stored.Explorer.ViewStyle = ViewStyle; });
    });
    FViewStyleActions[Style] = Action;
  }
  // RemoteCycleStyleAction: the next style, its button's menu the styles.
  QAction * Cycle = new QAction(tr("View"), this);
  Iconize(Cycle, "view-icons");
  QMenu * CycleMenu = new QMenu(this);
  for (QAction * Action : FViewStyleActions)
  {
    CycleMenu->addAction(Action);
  }
  Cycle->setMenu(CycleMenu);
  connect(Cycle, &QAction::triggered, this, [this]()
  {
    FViewStyleActions[(static_cast<int>(FPanel->ViewStyle()) + 1) % 5]->trigger();
  });

  // The bands, in the default layout's rows: Address; Buttons;
  // Selection; Preferences, Transfer, Updates; Sort; Session; Custom
  // commands.
  auto Band = [this](QToolBar * Bar, const QString & Name)
  {
    if (Bar->parentWidget() != this)
    {
      Bar->setParent(this);
    }
    addToolBar(Bar);
    Bar->setObjectName(Name);
    TBand Entry;
    Entry.Name = Name;
    Entry.Bar = Bar;
    FBands.push_back(Entry);
    return Bar;
  };
  auto NewBand = [this, &Band](const QString & Name, const QString & Title)
  {
    return Band(new QToolBar(Title, this), Name);
  };

  // AddressToolbar: the path, typed or picked (ShowFullAddress: the whole
  // path, else the last directory's name), Open Directory/Bookmark with
  // its menu, the filter, File colors.
  QToolBar * Address = NewBand(QStringLiteral("Address"), tr("Address"));
  QLabel * AddressLabel = new QLabel(tr("Address"), Address);
  Address->addWidget(AddressLabel);
  FAddress = new QComboBox(Address);
  FAddress->setEditable(true);
  FAddress->setInsertPolicy(QComboBox::NoInsert);
  FAddress->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  FAddress->setAccessibleName(tr("Address"));
  AddressLabel->setBuddy(FAddress);
  Address->addWidget(FAddress);
  connect(FAddress->lineEdit(), &QLineEdit::returnPressed, this, [this]() { GoToAddress(FAddress->currentText()); });
  connect(FAddress, &QComboBox::activated, this, [this](int Index) { GoToAddress(FAddress->itemData(Index).toString()); });
  QAction * OpenDirectory = SideAction(false, FOpenDirectoryAction, tr("&Open Directory/Bookmark..."), "open-directory");
  QMenu * OpenDirMenu = new QMenu(this);
  connect(OpenDirMenu, &QMenu::aboutToShow, this, [this, OpenDirMenu, OpenDirectory]()
  {
    FillOpenDirMenu(OpenDirMenu, false, OpenDirectory);
  });
  OpenDirectory->setMenu(OpenDirMenu);
  BandAction(Address, OpenDirectory, false, QStringLiteral("RemoteOpenDir"));
  if (QToolButton * Button = qobject_cast<QToolButton *>(Address->widgetForAction(OpenDirectory)))
  {
    Button->setPopupMode(QToolButton::MenuButtonPopup);
  }
  BandAction(Address, SideAction(false, FFilterAction, tr("&Filter..."), "filter"), false, QStringLiteral("RemoteFilter"));
  Address->addSeparator();
  QAction * FileColors = new QAction(tr("&File Colors..."), this);
  Iconize(FileColors, "preferences");
  connect(FileColors, &QAction::triggered, this, [this]() { PreferencesDialog(::PreferencesDialog::tr("File colors")); });
  BandAction(Address, FileColors, false, QStringLiteral("FileColorsPreferences"));
  addToolBarBreak();

  // ButtonsToolbar ("Standard Buttons").
  QToolBar * Buttons = NewBand(QStringLiteral("Buttons"), tr("Standard Buttons"));
  // (With ButtonsToolbar's names, as WinSCP's Explorer.)
  BandAction(Buttons, FPanel->BackAction(), false, QStringLiteral("RemoteBack"));
  BandAction(Buttons, FPanel->ForwardAction(), false, QStringLiteral("RemoteForward"));
  Buttons->addSeparator();
  BandAction(Buttons, FPanel->ParentAction(), false, QStringLiteral("RemoteParentDir"));
  BandAction(Buttons, FPanel->RootAction(), false, QStringLiteral("RemoteRootDir"));
  BandAction(Buttons, FPanel->HomeAction(), false, QStringLiteral("RemoteHomeDir"));
  BandAction(Buttons, FPanel->RefreshAction(), false, QStringLiteral("RemoteRefresh"));
  Buttons->addSeparator();
  BandAction(Buttons, FFindFilesAction, false, QStringLiteral("RemoteFindFiles"));
  Buttons->addSeparator();
  QAction * Download = SideAction(false, FCopyAction, tr("Down&load..."), "download");
  QMenu * DownloadMenu = new QMenu(this);
  DownloadMenu->addAction(FCurrentCopyAction);
  DownloadMenu->addAction(FCurrentCopyQueueAction);
  Download->setMenu(DownloadMenu);
  BandAction(Buttons, Download, true, QStringLiteral("RemoteCopy"));
  if (QToolButton * Button = qobject_cast<QToolButton *>(Buttons->widgetForAction(Download)))
  {
    Button->setPopupMode(QToolButton::MenuButtonPopup);
  }
  Buttons->addSeparator();
  BandAction(Buttons, SideAction(false, FMoveAction, tr("Download and Dele&te..."), "move"), false, QStringLiteral("RemoteMove"));
  Buttons->addSeparator();
  BandAction(Buttons, FEditAction, true, QStringLiteral("RemoteEdit"));
  BandAction(Buttons, FOpenCurrentAction, false, QStringLiteral("CurrentOpen"));
  BandAction(Buttons, FPanel->DeleteAction(), false, QStringLiteral("RemoteDelete"));
  BandAction(Buttons, FPropertiesAction, true, QStringLiteral("RemoteProperties"));
  Buttons->addSeparator();
  BandAction(Buttons, FDirectorySizesAction, false, QStringLiteral("RemoteCalculateDirectorySizes"));
  BandAction(Buttons, FPanel->RenameAction(), false, QStringLiteral("RemoteRename"));
  Buttons->addSeparator();
  const std::pair<QAction *, const char *> Rest[] = { { FNewFileAction, "RemoteNewFile" }, { FPanel->NewFolderAction(), "RemoteCreateDir" },
    { FLinkAction, "RemoteAddEditLink" }, { FConsoleAction, "Console" }, { FPuttyAction, "Putty" } };
  for (const auto & Each : Rest)
  {
    BandAction(Buttons, Each.first, false, QString::fromLatin1(Each.second));
  }
  Buttons->addSeparator();
  BandAction(Buttons, FKeepUpToDateAction, false, QStringLiteral("Synchronize"));
  BandAction(Buttons, FSynchronizeAction, true, QStringLiteral("FullSynchronize"));
  addToolBarBreak();

  // SelectionToolbar: the Mark menu's selections.
  QToolBar * Selection = NewBand(QStringLiteral("Selection"), tr("Selection"));
  if (QMenu * Mark = SubMenu(menuBar()->actions(), tr("&Mark")))
  {
    const QList<QAction *> Marks = Mark->actions();
    const std::pair<QString, const char *> Items[] = { { tr("Sele&ct Files..."), "Select" }, { tr("&Unselect Files..."), "Unselect" },
      { QString(), nullptr }, { tr("Select &All"), "SelectAll" }, { tr("&Invert Selection"), "InvertSelection" },
      { tr("C&lear Selection"), "ClearSelection" }, { tr("&Restore Selection"), "RestoreSelection" } };
    for (const auto & Item : Items)
    {
      if (Item.first.isEmpty())
      {
        Selection->addSeparator();
      }
      else if (QAction * Action = ActionOf(Marks, Item.first))
      {
        BandAction(Selection, Action, false, QString::fromLatin1(Item.second));
      }
    }
  }
  addToolBarBreak();

  // PreferencesToolbar: Preferences, the views, the queue, the tree.
  QToolBar * Preferences = NewBand(QStringLiteral("Preferences"), tr("Preferences"));
  BandAction(Preferences, FPreferencesAction, false, QStringLiteral("Preferences"));
  Preferences->addSeparator();
  BandAction(Preferences, Cycle, false, QStringLiteral("RemoteCycleStyle"));
  if (QToolButton * Button = qobject_cast<QToolButton *>(Preferences->widgetForAction(Cycle)))
  {
    Button->setPopupMode(QToolButton::MenuButtonPopup);
  }
  if (QueueButton != nullptr)
  {
    BandAction(Preferences, QueueButton, true, QStringLiteral("QueueToggleShow"));
  }
  Preferences->addSeparator();
  Iconize(FQueueFileListAction, "queue-file-list");
  Iconize(FRemoteTreeAction, "tree");
  BandAction(Preferences, FQueueFileListAction, false, QStringLiteral("QueueFileList"));
  BandAction(Preferences, FRemoteTreeAction, false, QStringLiteral("RemoteTree"));
  for (const char * Name : { "Transfer", "Updates" })
  {
    Band(Shared.value(QString::fromLatin1(Name)), QString::fromLatin1(Name));
  }
  addToolBarBreak();
  Band(Shared.value(QStringLiteral("Sort")), QStringLiteral("Sort"));
  addToolBarBreak();
  QToolBar * Session = Band(Shared.value(QStringLiteral("Session")), QStringLiteral("Session"));
  addToolBarBreak();
  Band(Shared.value(QStringLiteral("CustomCommands")), QStringLiteral("CustomCommands"));
  // No local tabs in the Explorer: New Tab opens a remote one.
  for (QAction * Action : Session->actions())
  {
    if (Action->menu() != nullptr)
    {
      for (QAction * Tab : Action->menu()->actions())
      {
        Tab->setVisible(Tab->text() != tr("&Local Tab"));
      }
    }
  }

  // View > Toolbars: each band, then the toolbars' own settings.
  QMenu * OldOptions = SubMenu(menuBar()->actions(), tr("&Options"));
  QMenu * OldToolbars = (OldOptions != nullptr) ? SubMenu(OldOptions->actions(), tr("&Toolbars")) : nullptr;
  QMenu * Toolbars = new QMenu(tr("&Toolbars"), this);
  FToolbarsMenu = Toolbars;
  FLeftPanelMenu = nullptr;
  FRightPanelMenu = nullptr;
  const std::pair<const char *, QString> Toggles[] = {
    { "Address", tr("&Address") }, { "Buttons", tr("&Standard Buttons") }, { "Selection", tr("Se&lection Buttons") },
    { "Session", tr("Sessio&ns and Tabs Buttons") }, { "Preferences", tr("&Preferences Buttons") },
    { "Sort", tr("So&rt Buttons") }, { "Updates", tr("&Update Button") }, { "Transfer", tr("&Transfer Settings") },
    { "CustomCommands", tr("Custom Co&mmand Buttons") } };
  for (const auto & Toggle : Toggles)
  {
    for (TBand & Entry : FBands)
    {
      if (Entry.Name == QString::fromLatin1(Toggle.first))
      {
        Entry.Toggle = Toolbars->addAction(Toggle.second);
        Entry.Toggle->setCheckable(true);
        QToolBar * Bar = Entry.Bar;
        connect(Entry.Toggle, &QAction::triggered, this, [this, Bar](bool Shown)
        {
          Bar->setVisible(Shown);
          SaveToolbarSettings();
        });
      }
    }
  }
  Toolbars->addSeparator();
  Toolbars->addAction(FLockToolbarsAction);
  Toolbars->addAction(FSelectiveTextAction);
  if (QMenu * IconSize = (OldToolbars != nullptr) ? SubMenu(OldToolbars->actions(), tr("T&oolbar Icons Size")) : nullptr)
  {
    Toolbars->addMenu(IconSize);
  }
  // Shown as the Explorer's layout has them.
  QString Stored = QString::fromStdString(Layout.Explorer.ToolbarsLayout);
  if (Stored.isEmpty())
  {
    Stored = DefaultExplorerToolbarsLayout();
  }
  for (TBand & Entry : FBands)
  {
    const QRegularExpressionMatch Match = QRegularExpression(QStringLiteral("(?:^|,)%1=(\\d)").arg(Entry.Name)).match(Stored);
    const bool Shown = !Match.hasMatch() || (Match.captured(1) != QStringLiteral("0"));
    Entry.Bar->setVisible(Shown);
    if (Entry.Toggle != nullptr)
    {
      Entry.Toggle->setChecked(Shown);
    }
  }
  if (FHotKeysBar != nullptr)
  {
    FHotKeysBar->hide();
    removeToolBar(FHotKeysBar);
  }
  if (FCommandLineBar != nullptr)
  {
    FCommandLineBar->hide();
    removeToolBar(FCommandLineBar);
  }
  ApplyToolbarsButtons(QString::fromStdString(Layout.Explorer.ToolbarsButtons));
  ApplyToolbarSettings();

  // The menus, as ScpExplorer.dfm's MenuToolbar: Files, Commands, Mark,
  // Tabs, View, Help.
  const QList<QAction *> Menus = menuBar()->actions();
  QMenu * OldFiles = SubMenu(Menus, tr("&Files"));
  QMenu * OldCommands = SubMenu(Menus, tr("&Commands"));
  QMenu * Mark = SubMenu(Menus, tr("&Mark"));
  QMenu * Tabs = SubMenu(Menus, tr("&Tabs"));
  QMenu * Help = SubMenu(Menus, tr("&Help"));
  menuBar()->clear();

  QMenu * Files = menuBar()->addMenu(tr("&Files"));
  if (QMenu * New = (OldFiles != nullptr) ? SubMenu(OldFiles->actions(), tr("&New")) : nullptr)
  {
    Files->addMenu(New);
  }
  Files->addSeparator();
  Files->addAction(FOpenCurrentAction);
  Files->addAction(FEditAction);
  Files->addAction(FLinkAction);
  Files->addSeparator();
  Files->addAction(FPanel->DeleteAction());
  Files->addAction(FPanel->RenameAction());
  Files->addAction(FPropertiesAction);
  Files->addAction(FDirectorySizesAction);
  Files->addSeparator();
  Files->addMenu(FCurrentCopyMenu);
  Files->addAction(FDuplicateAction);
  Files->addAction(FMoveToAction);
  Files->addSeparator();
  Files->addAction(FCopyClipboardAction);
  Files->addAction(FPasteAction);
  Files->addSeparator();
  Files->addMenu(FFileCommandsMenu);
  QMenu * Names = Files->addMenu(tr("&File Names"));
  Names->addAction(tr("Copy to &Clipboard"), this, [this]() { CopyFileNames(false, false); });
  Names->addAction(tr("Copy to Clipboard (Include &Paths)"), this, [this]() { CopyFileNames(false, true); });
  Names->addAction(FGenerateFileUrlAction);
  Files->addMenu(FLockingMenu);
  connect(Files, &QMenu::aboutToShow, this, &MainWindow::UpdateFilesMenu);
  Files->addSeparator();
  Files->addAction(FCloseTabAction);
  Files->addAction(FQuitAction);

  QMenu * Commands = menuBar()->addMenu(tr("&Commands"));
  Commands->addAction(FKeepUpToDateAction);
  Commands->addAction(FSynchronizeAction);
  Commands->addAction(FFindFilesAction);
  if (QMenu * Queue = (OldCommands != nullptr) ? SubMenu(OldCommands->actions(), tr("Q&ueue")) : nullptr)
  {
    Commands->addMenu(Queue);
  }
  Commands->addMenu(FStaticCommandsMenu);
  Commands->addSeparator();
  Commands->addAction(IconSet::Icon(QStringLiteral("bookmark")), tr("&Add Path to Bookmarks"), this, [this]() { this->OpenDirectory(true); });
  QAction * CopyPath = Commands->addAction(tr("Copy &Path to Clipboard"), this,
    [this]() { QApplication::clipboard()->setText(FPanel->Path()); });
  Commands->addSeparator();
  Commands->addAction(FConsoleAction);
  Commands->addAction(FPuttyAction);
  Commands->addSeparator();
  Commands->addAction(FClearCachesAction);

  if (Mark != nullptr)
  {
    menuBar()->addMenu(Mark);
  }
  if (Tabs != nullptr)
  {
    // New Tab: a remote one alone (no local tabs in the Explorer).
    if (QMenu * NewTab = SubMenu(Tabs->actions(), tr("&New Tab")))
    {
      QAction * Remote = ActionOf(NewTab->actions(), tr("&Remote Tab..."));
      QAction * Plain = new QAction(NewTab->menuAction()->icon(), tr("&New Tab"), this);
      if (Remote != nullptr)
      {
        Plain->setShortcut(Remote->shortcut());
        Remote->setShortcut(QKeySequence());
        connect(Plain, &QAction::triggered, Remote, &QAction::trigger);
      }
      Tabs->insertAction(NewTab->menuAction(), Plain);
      Tabs->removeAction(NewTab->menuAction());
    }
    menuBar()->addMenu(Tabs);
  }

  QMenu * View = menuBar()->addMenu(tr("&View"));
  View->addMenu(Toolbars);
  View->addAction(FTabsAction);
  View->addAction(FStatusBarAction);
  if (QMenu * Queue = (OldOptions != nullptr) ? SubMenu(OldOptions->actions(), tr("&Queue")) : nullptr)
  {
    View->addMenu(Queue);
  }
  View->addSeparator();
  View->addAction(FRemoteTreeAction);
  View->addSeparator();
  for (QAction * Action : FViewStyleActions)
  {
    View->addAction(Action);
  }
  View->addSeparator();
  QMenu * GoTo = View->addMenu(tr("&Go To"));
  GoTo->addAction(FOpenDirectoryAction);
  GoTo->addSeparator();
  for (QAction * Action : { FPanel->ParentAction(), FPanel->RootAction(), FPanel->HomeAction() })
  {
    GoTo->addAction(Action);
  }
  GoTo->addSeparator();
  GoTo->addAction(FPanel->BackAction());
  GoTo->addAction(FPanel->ForwardAction());
  View->addAction(FPanel->RefreshAction());
  QMenu * Sort = FPanel->View()->SortMenu(false);
  Sort->setTitle(tr("&Sort"));
  Sort->setParent(View, Sort->windowFlags());
  View->addMenu(Sort);
  QMenu * Columns = FPanel->View()->ColumnsMenu(true);
  Columns->setTitle(tr("&Columns"));
  Columns->setParent(View, Columns->windowFlags());
  View->addMenu(Columns);
  View->addAction(FFilterAction);
  View->addSeparator();
  View->addAction(FPreferencesAction);

  if (Help != nullptr)
  {
    menuBar()->addMenu(Help);
  }
  Iconize(CopyPath, "clipboard-copy");
  ApplyIcons();

  // The view style, the tree, the status bar and tabs: the Explorer's.
  FPanel->SetViewStyle(Layout.Explorer.ViewStyle);
  FViewStyleActions[static_cast<int>(Layout.Explorer.ViewStyle)]->setChecked(true);
  FStatusBarAction->setChecked(Layout.Explorer.StatusBar);
  statusBar()->setVisible(Layout.Explorer.StatusBar);
  FTabsShown = Layout.Explorer.Tabs;
  FTabsAction->setChecked(Layout.Explorer.Tabs);
  UpdateTabs();
  connect(FPanel, &RemotePanel::Changed, this, &MainWindow::UpdateAddress);
  UpdateAddress();
  ApplyExplorerShortcuts();
}

// As NonVisual's ExplorerShortcuts.
void MainWindow::ApplyExplorerShortcuts()
{
  if (!FExplorer)
  {
    return;
  }
  FPanel->NewFolderAction()->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
  FPanel->RenameAction()->setShortcut(QKeySequence(Qt::Key_F2));
  FEditAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  FLinkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_L));
  FCopyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
  FMoveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
  FPanel->DeleteAction()->setShortcut(QKeySequence(Qt::Key_Delete));
  FPropertiesAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
  FMoveToAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_M));
  FOpenDirectoryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
  FPanel->RefreshAction()->setShortcut(QKeySequence(Qt::Key_F5));
  FPanel->HomeAction()->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
  FNewFileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
  FFindFilesAction->setShortcut(QKeySequence(Qt::Key_F3));
  FQuitAction->setShortcuts({ QKeySequence(Qt::ALT | Qt::Key_F4), QKeySequence(QKeySequence::Quit) });
}

// UpdateRemotePathComboBox: the path, the address's history its parents.
void MainWindow::UpdateAddress()
{
  if (FAddress == nullptr)
  {
    return;
  }
  const QString Path = FPanel->Path();
  const bool Full = CoreBridge::Layout().Explorer.ShowFullAddress;
  FAddress->clear();
  // The root and each directory down to this one, as the combo's list.
  QStringList Parents;
  QString Walk = Path;
  while (!Walk.isEmpty())
  {
    Parents.prepend(Walk);
    if (Walk == QStringLiteral("/"))
    {
      break;
    }
    const int Slash = Walk.lastIndexOf(QLatin1Char('/'));
    Walk = (Slash <= 0) ? QStringLiteral("/") : Walk.left(Slash);
  }
  for (const QString & Each : Parents)
  {
    FAddress->addItem(Each, Each);
  }
  QString Shown = Path;
  if (!Full && (Path != QStringLiteral("/")))
  {
    Shown = Path.mid(Path.lastIndexOf(QLatin1Char('/')) + 1);
  }
  FAddress->setCurrentIndex(FAddress->count() - 1);
  FAddress->setEditText(Shown);
}

void MainWindow::GoToAddress(const QString & Path)
{
  const QString Target = Path.trimmed();
  if (!Target.isEmpty() && FSession && FSession->Active())
  {
    FPanel->ChangeDirectory(Target);
  }
  UpdateAddress();
}
