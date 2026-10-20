#include "PreferencesDialog.h"
#include "IconSet.h"
#include <QApplication>
#include "HintButton.h"
#include "MasterPasswordDialog.h"
#include "SshHostCADialog.h"
#include "QtFrontend.h"
#include "SingleInstance.h"

#include "TransferSettingsDialog.h"
#include "ExtensionOptionsDialog.h"
#include "DragExtClient.h"
#include <QToolButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QRegularExpression>
#include "CustomCommandDialog.h"
#include "ShortCutDialog.h"
#include "EditorPreferencesDialog.h"
#include "Integration.h"
#include "QtFrontend.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QColorDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QToolTip>
#include <QRadioButton>
#include <QSpinBox>
#include <QGridLayout>
#include <QCoreApplication>
#include <QDir>
#include <QFontDialog>
#include <QFrame>
#include <QPalette>
#include <QFontDatabase>
#include <QMenu>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QShortcut>
#include <QScrollBar>
#include <QScrollArea>
#include <QTextDocumentFragment>
#include <QAbstractButton>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QDesktopServices>
#include <QUrl>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <algorithm>

using TPreferences = CoreBridge::TPreferences;

namespace
{

enum { LocaleRole = Qt::UserRole };

// As ComboAutoSwitchInitialize, ComboAutoSwitchLoad and ComboAutoSwitchSave:
// Auto, Off, On, in that order (TPreferences::TAutoSwitch's On, Off, Auto).
int IndexOfAutoSwitch(TPreferences::TAutoSwitch Value)
{
  return (Value == TPreferences::TAutoSwitch::Auto) ? 0 : ((Value == TPreferences::TAutoSwitch::Off) ? 1 : 2);
}

TPreferences::TAutoSwitch AutoSwitchOfIndex(int Index)
{
  return (Index == 0) ? TPreferences::TAutoSwitch::Auto :
    ((Index == 1) ? TPreferences::TAutoSwitch::Off : TPreferences::TAutoSwitch::On);
}

// As StripHotkey, with the "(&X)" the CJK translations append.
QString StripHotkey(const QString & Text)
{
  static const QRegularExpression Appended(QStringLiteral("\\(&[^&]\\)"));
  QString Result = Text;
  return PresetCaption(Result.remove(Appended));
}

}  // namespace

PreferencesDialog::PreferencesDialog(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Preferences"));
  resize(720, 540);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Main = new QHBoxLayout();
  // WinSCP's tree of pages, each section's own page with its subpages
  // under it.
  FNavigation = new QTreeWidget(this);
  FNavigation->setAccessibleName(tr("Pages"));
  FNavigation->setHeaderHidden(true);
  FNavigation->setRootIsDecorated(false);
  FNavigation->setIndentation(12);
  // Its names are WinSCP's tree's, "Drag & Drop" with a plain "&".
  FNavigation->setProperty("literalText", true);
  FPages = new QStackedWidget(this);
  // WinSCP's SearchEdit above the tree: the pages' captions with its text
  // on a page of their own, links to them.
  FSearchEdit = new QLineEdit(this);
  FSearchEdit->setPlaceholderText(tr("Search"));
  FSearchEdit->setAccessibleName(tr("Search"));
  FSearchEdit->setClearButtonEnabled(true);
  FSearchEdit->installEventFilter(this);
  FNavigation->installEventFilter(this);
  connect(FSearchEdit, &QLineEdit::textChanged, this, [this](const QString & Text) { UpdateSearching(!Text.isEmpty()); });
  QShortcut * Find = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
  connect(Find, &QShortcut::activated, FSearchEdit, [this]() { FSearchEdit->setFocus(Qt::ShortcutFocusReason); });
  QVBoxLayout * NavigationLayout = new QVBoxLayout();
  NavigationLayout->addWidget(FSearchEdit);
  NavigationLayout->addWidget(FNavigation, 1);
  Main->addLayout(NavigationLayout);
  Main->addWidget(FPages, 1);
  Layout->addLayout(Main, 1);

  const TPreferences Preferences = CoreBridge::Preferences();
  FDefaultSettings = CoreBridge::DefaultTransferSettings();
  FPresets = CoreBridge::TransferPresets();
  FFileColors = Preferences.FileColors;
  // In the order of WinSCP's tree: Environment (Interface, Window,
  // Commander, Languages), Panels (Remote, Local, File colors), Editors
  // (Internal editor), Transfer (Drag & Drop, Background, Endurance),
  // Network, Security, Logging, Integration (Applications, Commands),
  // Storage, Updates.
  AddPage(tr("Environment"), MakeEnvironmentPage(Preferences));
  AddPage(tr("Interface"), MakeInterfacePage(Preferences), true);
  AddPage(tr("Window"), MakeWindowPage(Preferences), true);
  AddPage(tr("Commander"), MakeCommanderPage(Preferences), true);
  AddPage(tr("Explorer"), MakeExplorerPage(), true);
  AddPage(tr("Languages"), MakeLanguagesPage(), true);
  AddPage(tr("Panels"), MakePanelsPage(Preferences));
  AddPage(tr("Remote"), MakeRemotePage(Preferences), true);
  AddPage(tr("Local"), MakeLocalPage(Preferences), true);
  AddPage(tr("File colors"), MakeFileColorsPage(), true);
  AddPage(tr("Editors"), MakeEditorsPage());
  AddPage(tr("Internal editor"), MakeInternalEditorPage(), true);
  AddPage(tr("Transfer"), MakeTransferPage(Preferences));
  AddPage(tr("Drag & Drop"), MakeDragDropPage(Preferences), true);
  AddPage(tr("Background"), MakeBackgroundPage(Preferences), true);
  AddPage(tr("Endurance"), MakeEndurancePage(Preferences), true);
  AddPage(tr("Network"), MakeNetworkPage(Preferences));
  AddPage(tr("Security"), MakeSecurityPage(Preferences));
  AddPage(tr("Logging"), MakeLoggingPage(Preferences));
  AddPage(tr("Integration"), MakeIntegrationPage());
  AddPage(tr("Applications"), MakeApplicationsPage(Preferences), true);
  AddPage(tr("Commands"), MakeCommandsPage(), true);
  AddPage(tr("Storage"), MakeStoragePage(Preferences));
  // Updates only where linscp checks for them (Integration::UpdateChecksAllowed);
  // otherwise its controls made, as they keep the settings, but not shown.
  QWidget * UpdatesPage = MakeUpdatesPage(Preferences);
  if (Integration::UpdateChecksAllowed())
  {
    AddPage(tr("Updates"), UpdatesPage);
  }
  else
  {
    UpdatesPage->hide();
  }
  FNavigation->expandAll();
  // As wide as its names (subpages indented), with room for a scroll bar.
  int NamesWidth = 0;
  for (const QTreeWidgetItem * Item : FPageItems)
  {
    NamesWidth = std::max(NamesWidth, FNavigation->fontMetrics().horizontalAdvance(Item->text(0)) +
      ((Item->parent() != nullptr) ? FNavigation->indentation() : 0));
  }
  FNavigation->setFixedWidth(NamesWidth + 2 * FNavigation->frameWidth() +
    FNavigation->verticalScrollBar()->sizeHint().width() + 16);
  FSearchEdit->setFixedWidth(FNavigation->width());
  FSearchPage = new QWidget(this);
  QVBoxLayout * SearchPageLayout = new QVBoxLayout(FSearchPage);
  SearchPageLayout->setContentsMargins(0, 0, 0, 0);
  QGroupBox * SearchGroup = new QGroupBox(tr("Search"), FSearchPage);
  QVBoxLayout * SearchGroupLayout = new QVBoxLayout(SearchGroup);
  QScrollArea * SearchScroll = new QScrollArea(SearchGroup);
  SearchScroll->setFrameShape(QFrame::NoFrame);
  SearchScroll->setWidgetResizable(true);
  QWidget * SearchList = new QWidget(SearchScroll);
  FSearchLayout = new QVBoxLayout(SearchList);
  FSearchLayout->setContentsMargins(0, 0, 0, 0);
  FSearchLayout->addStretch(1);
  SearchScroll->setWidget(SearchList);
  SearchGroupLayout->addWidget(SearchScroll);
  SearchPageLayout->addWidget(SearchGroup);
  FPages->addWidget(FSearchPage);
  connect(FNavigation, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem * Item)
  {
    if (Item != nullptr)
    {
      FPages->setCurrentIndex(Item->data(0, Qt::UserRole).toInt());
    }
  });
  ShowPageAt(0);

  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
  FBuilt = true;
  UpdateControls();
}

void PreferencesDialog::AddPage(const QString & Name, QWidget * Page, bool Subpage)
{
  QTreeWidgetItem * Parent = (Subpage && (FNavigation->topLevelItemCount() > 0)) ?
    FNavigation->topLevelItem(FNavigation->topLevelItemCount() - 1) : nullptr;
  QTreeWidgetItem * Item = (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Name }) : new QTreeWidgetItem(FNavigation, { Name });
  Item->setData(0, Qt::UserRole, FPages->addWidget(Page));
  FPageItems.push_back(Item);
}

// As TPreferencesDialog::GetControlText: a page's is its name in the tree.
QString PreferencesDialog::ControlText(QWidget * Control) const
{
  QString Result;
  const int Page = FPages->indexOf(Control);
  if (Page >= 0)
  {
    for (const QTreeWidgetItem * Item : FPageItems)
    {
      if (Item->data(0, Qt::UserRole).toInt() == Page)
      {
        Result = Item->text(0);
      }
    }
  }
  else if (QLabel * Label = qobject_cast<QLabel *>(Control))
  {
    Result = Label->text();
    if (Label->textFormat() != Qt::PlainText && Qt::mightBeRichText(Result))
    {
      Result = QTextDocumentFragment::fromHtml(Result).toPlainText();
    }
  }
  else if (QAbstractButton * Button = qobject_cast<QAbstractButton *>(Control))
  {
    Result = Button->text();
  }
  else if (QGroupBox * Group = qobject_cast<QGroupBox *>(Control))
  {
    Result = Group->title();
  }
  return Result;
}

// As TPreferencesDialog::GetSearchParent: a page's is its section's page.
QWidget * PreferencesDialog::SearchParent(QWidget * Control) const
{
  QWidget * Result = nullptr;
  const int Page = FPages->indexOf(Control);
  if (Page >= 0)
  {
    for (const QTreeWidgetItem * Item : FPageItems)
    {
      if ((Item->data(0, Qt::UserRole).toInt() == Page) && (Item->parent() != nullptr))
      {
        Result = FPages->widget(Item->parent()->data(0, Qt::UserRole).toInt());
      }
    }
  }
  else
  {
    Result = Control->parentWidget();
    if (Result == FPages)
    {
      Result = nullptr;
    }
  }
  return Result;
}

// As TPreferencesDialog::Search: the captions (a drop-down list's items)
// of the page's shown controls, not what is typed in.
void PreferencesDialog::Search(QWidget * Control, std::vector<TSearchResult> & Results) const
{
  const QString Text = FSearchEdit->text();
  // As ContainsTextSemiCaseSensitive.
  const Qt::CaseSensitivity Sensitivity = (Text.toLower() == Text) ? Qt::CaseInsensitive : Qt::CaseSensitive;
  QComboBox * Combo = qobject_cast<QComboBox *>(Control);
  if ((Combo != nullptr) && !Combo->isEditable())
  {
    for (int Index = 0; Index < Combo->count(); Index++)
    {
      if (Combo->itemText(Index).contains(Text, Sensitivity))
      {
        Results.push_back({ Combo->itemText(Index), Control });
      }
    }
  }
  else if ((Combo == nullptr) && (qobject_cast<QLineEdit *>(Control) == nullptr) &&
           (qobject_cast<QAbstractSpinBox *>(Control) == nullptr) &&
           (qobject_cast<QAbstractScrollArea *>(Control) == nullptr))
  {
    const QString Caption = ((FPages->indexOf(Control) >= 0) || Control->property("literalText").toBool()) ?
      ControlText(Control) : StripHotkey(ControlText(Control));
    // Not the unit labels (as "s").
    if ((Caption.length() >= 4) && Caption.contains(Text, Sensitivity))
    {
      Results.push_back({ Caption, Control });
    }
    for (QWidget * Child : Control->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    {
      if (!Child->isHidden() && !Child->isWindow())
      {
        Search(Child, Results);
      }
    }
  }
}

bool PreferencesDialog::Searching() const
{
  return (FSearchResults != nullptr);
}

QStringList PreferencesDialog::SearchResults() const
{
  QStringList Result;
  if (FSearchResults != nullptr)
  {
    for (const TSearchResult & SearchResult : *FSearchResults)
    {
      // As the results page's labels: the context, then the caption.
      QString Context;
      for (QWidget * Parent = SearchParent(SearchResult.Control); Parent != nullptr; Parent = SearchParent(Parent))
      {
        const QString ParentCaption = (FPages->indexOf(Parent) >= 0) ? ControlText(Parent) : StripHotkey(ControlText(Parent));
        if (!ParentCaption.isEmpty())
        {
          Context = ParentCaption + QStringLiteral(" \u00BB ") + Context;
        }
      }
      Result.append((Context.isEmpty() ? QStringLiteral("\u00BB") : Context) + QLatin1Char('\n') + SearchResult.Caption);
    }
  }
  return Result;
}

void PreferencesDialog::UpdateSearching(bool Searching)
{
  if (!Searching)
  {
    if (FSearchResults != nullptr)
    {
      FSearchResults.reset();
      if (QTreeWidgetItem * Item = FNavigation->currentItem())
      {
        FPages->setCurrentIndex(Item->data(0, Qt::UserRole).toInt());
      }
    }
  }
  else
  {
    auto Results = std::make_unique<std::vector<TSearchResult>>();
    // The pages in the tree's order.
    for (const QTreeWidgetItem * Item : FPageItems)
    {
      Search(FPages->widget(Item->data(0, Qt::UserRole).toInt()), *Results);
    }
    if ((FSearchResults == nullptr) || (*FSearchResults != *Results))
    {
      FSearchResults = std::move(Results);
      while (FSearchLayout->count() > 1)
      {
        QLayoutItem * LayoutItem = FSearchLayout->takeAt(0);
        delete LayoutItem->widget();
        delete LayoutItem;
      }
      const QStringList Texts = SearchResults();
      if (Texts.isEmpty())
      {
        FSearchLayout->insertWidget(0, new QLabel(tr("No search results found.")));
      }
      for (int Index = 0; Index < Texts.size(); Index++)
      {
        const int Separator = Texts[Index].indexOf(QLatin1Char('\n'));
        QLabel * ContextLabel = new QLabel(Texts[Index].left(Separator));
        ContextLabel->setTextFormat(Qt::PlainText);
        QLabel * LinkLabel = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>").arg(Index).arg(Texts[Index].mid(Separator + 1).toHtmlEscaped()));
        LinkLabel->setWordWrap(true);
        LinkLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
        LinkLabel->setFocusPolicy(Qt::TabFocus);
        LinkLabel->setAccessibleName(Texts[Index].mid(Separator + 1));
        connect(LinkLabel, &QLabel::linkActivated, this, [this](const QString & Link) { OpenSearchResult(Link.toInt()); });
        const int At = FSearchLayout->count() - 1;
        FSearchLayout->insertWidget(At, ContextLabel);
        FSearchLayout->insertWidget(At + 1, LinkLabel);
        FSearchLayout->insertSpacing(At + 2, 8);
      }
    }
    FPages->setCurrentWidget(FSearchPage);
  }
}

void PreferencesDialog::OpenSearchResult(int Index)
{
  if ((FSearchResults != nullptr) && (Index >= 0) && (Index < static_cast<int>(FSearchResults->size())))
  {
    const TSearchResult Result = (*FSearchResults)[Index];
    FSearchResults.reset();
    QWidget * Page = Result.Control;
    while (FPages->indexOf(Page) < 0)
    {
      Page = Page->parentWidget();
    }
    for (QTreeWidgetItem * Item : FPageItems)
    {
      if (Item->data(0, Qt::UserRole).toInt() == FPages->indexOf(Page))
      {
        FNavigation->setCurrentItem(Item);
      }
    }
    FPages->setCurrentWidget(Page);
    FocusAndHighlightControl(Result.Control, Result.Caption);
  }
}

// As TPreferencesDialog::FocusAndHighlightControl.
void PreferencesDialog::FocusAndHighlightControl(QWidget * Control, const QString & Text)
{
  bool HighlightInside = true;
  QLabel * Label = qobject_cast<QLabel *>(Control);
  if ((Label != nullptr) && (Label->buddy() != nullptr))
  {
    Control = Label->buddy();
    HighlightInside = false;
  }
  QRadioButton * Radio = qobject_cast<QRadioButton *>(Control);
  // Not focusing an unchecked radio button, that would check it.
  if ((Radio != nullptr) && !Radio->isChecked())
  {
    QWidget * Focus = Radio->parentWidget();
    for (QRadioButton * Sibling : Radio->parentWidget()->findChildren<QRadioButton *>(QString(), Qt::FindDirectChildrenOnly))
    {
      if (Sibling->isChecked())
      {
        Focus = Sibling;
      }
    }
    if (Focus->isEnabled())
    {
      Focus->setFocus(Qt::OtherFocusReason);
    }
  }
  else
  {
    if (Control->isEnabled())
    {
      Control->setFocus(Qt::OtherFocusReason);
    }
    QComboBox * Combo = qobject_cast<QComboBox *>(Control);
    if (HighlightInside && (Combo != nullptr) && !Combo->isEditable() && (Combo->currentText() != Text) && Combo->isVisible())
    {
      Combo->showPopup();
    }
  }
  // Where it is, on a page that scrolls.
  for (QWidget * Parent = Control->parentWidget(); Parent != nullptr; Parent = Parent->parentWidget())
  {
    if (QScrollArea * Scroll = qobject_cast<QScrollArea *>(Parent))
    {
      Scroll->ensureWidgetVisible(Control);
      break;
    }
  }
}

bool PreferencesDialog::eventFilter(QObject * Object, QEvent * Event)
{
  if (Event->type() == QEvent::FocusIn)
  {
    if (Object == FSearchEdit)
    {
      UpdateSearching(!FSearchEdit->text().isEmpty());
    }
    else if (Object == FNavigation)
    {
      UpdateSearching(false);
    }
  }
  return QDialog::eventFilter(Object, Event);
}

void PreferencesDialog::keyPressEvent(QKeyEvent * Event)
{
  // Escape leaves the results, not the dialog.
  if ((Event->key() == Qt::Key_Escape) && (Event->modifiers() == Qt::NoModifier) && (FSearchResults != nullptr))
  {
    FSearchEdit->clear();
    FNavigation->setFocus(Qt::OtherFocusReason);
    Event->accept();
  }
  else
  {
    QDialog::keyPressEvent(Event);
  }
}

QString PreferencesDialog::PageName(int Index) const
{
  return ((Index >= 0) && (Index < PageCount())) ? FPageItems[Index]->text(0) : QString();
}

void PreferencesDialog::ShowPageAt(int Index)
{
  if ((Index >= 0) && (Index < PageCount()))
  {
    FNavigation->setCurrentItem(FPageItems[Index]);
  }
}

bool PreferencesDialog::ShowPage(const QString & Name)
{
  for (int Index = 0; Index < PageCount(); Index++)
  {
    if (PageName(Index) == Name)
    {
      ShowPageAt(Index);
      return true;
    }
  }
  return false;
}

QWidget * PreferencesDialog::MakePanelsPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Common = new QGroupBox(tr("Common"), Page);
  QVBoxLayout * CommonLayout = new QVBoxLayout(Common);
  FShowHiddenFiles = new QCheckBox(tr("&Show hidden files (Ctrl+Alt+H)"), Common);
  FShowHiddenFiles->setChecked(Preferences.ShowHiddenFiles);
  FDefaultDirIsHome = new QCheckBox(tr("Default directory is &home directory"), Common);
  FDefaultDirIsHome->setChecked(Preferences.DefaultDirIsHome);
  FRenameWholeName = new QCheckBox(tr("Select &whole name when renaming file"), Common);
  FRenameWholeName->setChecked(Preferences.RenameWholeName);
  FNaturalSorting = new QCheckBox(tr("Use &natural order numerical sorting"), Common);
  FNaturalSorting->setChecked(Preferences.NaturalOrderNumericalSorting);
  FDirectoriesByName = new QCheckBox(tr("Always sort &directories by name"), Common);
  FDirectoriesByName->setChecked(Preferences.AlwaysSortDirectoriesByName);
  FPreservePanelState = new QCheckBox(tr("&Remember panels' state when switching sessions"), Common);
  FPreservePanelState->setChecked(Preferences.PreservePanelState);
  FFullRowSelect = new QCheckBox(tr("&Full row select"), Common);
  FFullRowSelect->setChecked(Preferences.FullRowSelect);
  // In Panels.dfm's order.
  for (QCheckBox * Check : { FShowHiddenFiles, FDefaultDirIsHome, FPreservePanelState, FRenameWholeName, FFullRowSelect,
         FNaturalSorting, FDirectoriesByName })
  {
    CommonLayout->addWidget(Check);
  }
  QFormLayout * Sizes = new QFormLayout();
  FSizeFormat = new QComboBox(Common);
  // TFormatBytesStyle's order: fbNone, fbKilobytes, fbShort.
  FSizeFormat->addItems({ tr("Bytes"), tr("Kilobytes"), tr("Short format") });
  FSizeFormat->setCurrentIndex(static_cast<int>(Preferences.SizeFormat));
  Sizes->addRow(tr("Show file si&zes in:"), FSizeFormat);
  FPanelSearch = new QComboBox(Common);
  FPanelSearch->addItems({ tr("Beginning of name only"), tr("Any part of name"), tr("All columns") });
  FPanelSearch->setCurrentIndex(static_cast<int>(Preferences.PanelSearch));
  Sizes->addRow(tr("&Incremental search:"), FPanelSearch);
  CommonLayout->addLayout(Sizes);
  Layout->addWidget(Common);
  QGroupBox * DoubleClick = new QGroupBox(tr("Double-click"), Page);
  QFormLayout * DoubleClickLayout = new QFormLayout(DoubleClick);
  FDoubleClickAction = new QComboBox(DoubleClick);
  FDoubleClickAction->addItems({ tr("Open"), tr("Copy"), tr("Edit") });
  FDoubleClickAction->setCurrentIndex(static_cast<int>(Preferences.DoubleClickAction));
  DoubleClickLayout->addRow(tr("&Operation to perform on double-click:"), FDoubleClickAction);
  FCopyOnDoubleClickConfirmation = new QCheckBox(tr("&Confirm copy on double-click operation"), DoubleClick);
  FCopyOnDoubleClickConfirmation->setChecked(Preferences.CopyOnDoubleClickConfirmation);
  DoubleClickLayout->addRow(FCopyOnDoubleClickConfirmation);
  connect(FDoubleClickAction, &QComboBox::currentIndexChanged, this, [this]()
  {
    FCopyOnDoubleClickConfirmation->setEnabled(FDoubleClickAction->currentIndex() == 1);
  });
  FCopyOnDoubleClickConfirmation->setEnabled(FDoubleClickAction->currentIndex() == 1);
  Layout->addWidget(DoubleClick);
  // As PanelFontGroup: a custom font, else the system's.
  QGroupBox * FontGroup = new QGroupBox(tr("Panel font"), Page);
  QGridLayout * FontLayout = new QGridLayout(FontGroup);
  FPanelFontCheck = new QCheckBox(tr("&Use custom font"), FontGroup);
  FPanelFontCheck->setChecked(!Preferences.PanelFontName.empty());
  FPanelFont = QApplication::font();
  if (!Preferences.PanelFontName.empty())
  {
    FPanelFont.setFamily(QString::fromStdString(Preferences.PanelFontName));
    if (Preferences.PanelFontSize > 0)
    {
      FPanelFont.setPointSize(Preferences.PanelFontSize);
    }
    FPanelFont.setBold(Preferences.PanelFontBold);
    FPanelFont.setItalic(Preferences.PanelFontItalic);
  }
  FPanelFontLabel = new QLabel(FontGroup);
  QPushButton * FontButton = new QPushButton(tr("Select fon&t..."), FontGroup);
  auto UpdateFont = [this]()
  {
    FPanelFontLabel->setText(FPanelFontCheck->isChecked() ?
      QStringLiteral("%1, %2 pt").arg(FPanelFont.family()).arg(FPanelFont.pointSize()) : QString());
    FPanelFontLabel->setFont(FPanelFontCheck->isChecked() ? FPanelFont : QApplication::font());
  };
  connect(FontButton, &QPushButton::clicked, this, [this, UpdateFont]()
  {
    bool OK = false;
    const QFont Chosen = QFontDialog::getFont(&OK, FPanelFont, this);
    if (OK)
    {
      FPanelFont = Chosen;
      FPanelFontCheck->setChecked(true);
      UpdateFont();
    }
  });
  connect(FPanelFontCheck, &QCheckBox::toggled, this, UpdateFont);
  FontLayout->addWidget(FPanelFontCheck, 0, 0);
  FontLayout->addWidget(FontButton, 0, 1, Qt::AlignRight);
  FontLayout->addWidget(FPanelFontLabel, 1, 0, 1, 2);
  UpdateFont();
  Layout->addWidget(FontGroup);
  Layout->addStretch(1);
  return Page;
}

QWidget * PreferencesDialog::MakeCommanderPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  Layout->addWidget(new QLabel(tr("Preferences on this page apply to Commander interface only."), Page));
  QGroupBox * Panels = new QGroupBox(tr("Panels"), Page);
  QVBoxLayout * PanelsLayout = new QVBoxLayout(Panels);
  QFormLayout * SelectionLayout = new QFormLayout();
  FExplorerSelection = new QComboBox(Panels);
  FExplorerSelection->addItems({ tr("Never"), tr("Mouse only"), tr("Mouse and Keyboard") });
  FExplorerSelection->setCurrentIndex(static_cast<int>(Preferences.ExplorerSelection));
  SelectionLayout->addRow(tr("&Explorer-style selection:"), FExplorerSelection);
  PanelsLayout->addLayout(SelectionLayout);
  FSwappedPanels = new QCheckBox(tr("S&wap panels (local on right, remote on left)"), Panels);
  FSwappedPanels->setChecked(Preferences.SwappedPanels);
  PanelsLayout->addWidget(FSwappedPanels);
  FTreeOnLeft = new QCheckBox(tr("Show &directory tree left of file list"), Panels);
  FTreeOnLeft->setChecked(Preferences.TreeOnLeft);
  PanelsLayout->addWidget(FTreeOnLeft);
  Layout->addWidget(Panels);
  QGroupBox * Misc = new QGroupBox(tr("Miscellaneous"), Page);
  QFormLayout * MiscLayout = new QFormLayout(Misc);
  FKeyboardShortcuts = new QComboBox(Misc);
  FKeyboardShortcuts->addItems({ tr("Commander"), tr("Explorer") });
  FKeyboardShortcuts->setCurrentIndex(Preferences.ExplorerKeyboardShortcuts ? 1 : 0);
  MiscLayout->addRow(tr("&Keyboard shortcuts"), FKeyboardShortcuts);
  FUseLocationProfiles = new QCheckBox(tr("&Use Location Profiles instead of Directory Bookmarks"), Misc);
  FUseLocationProfiles->setChecked(Preferences.UseLocationProfiles);
  MiscLayout->addRow(FUseLocationProfiles);
  Layout->addWidget(Misc);
  QGroupBox * Compare = new QGroupBox(tr("Compare directory criteria"), Page);
  QVBoxLayout * CompareLayout = new QVBoxLayout(Compare);
  FCompareByTime = new QCheckBox(tr("Compare by &time"), Compare);
  FCompareByTime->setChecked(Preferences.CompareByTime);
  CompareLayout->addWidget(FCompareByTime);
  FCompareBySize = new QCheckBox(tr("Compare by &size"), Compare);
  FCompareBySize->setChecked(Preferences.CompareBySize);
  CompareLayout->addWidget(FCompareBySize);
  Layout->addWidget(Compare);
  Layout->addStretch(1);
  return Page;
}

// As TPreferencesDialog::LoadLanguages: each translation, the one chosen
// current; one that does not load greyed, as its custom draw has it.
// As ExplorerSheet.
QWidget * PreferencesDialog::MakeExplorerPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  Layout->addWidget(new QLabel(tr("Preferences on this page apply to Explorer interface only."), Page));
  QGroupBox * View = new QGroupBox(tr("View"), Page);
  QVBoxLayout * ViewLayout = new QVBoxLayout(View);
  FShowFullAddress = new QCheckBox(tr("&Show full path on address bar"), View);
  FShowFullAddress->setChecked(CoreBridge::Layout().Explorer.ShowFullAddress);
  ViewLayout->addWidget(FShowFullAddress);
  Layout->addWidget(View);
  Layout->addStretch(1);
  return Page;
}

QWidget * PreferencesDialog::MakeLanguagesPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Languages"), Page);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  FLanguages = new QListWidget(Group);
  FLanguages->setAccessibleName(Group->title());
  const unsigned Chosen = CoreBridge::PreferredLocale();
  for (const CoreBridge::TLocaleOption & Option : CoreBridge::Locales())
  {
    QListWidgetItem * Item = new QListWidgetItem(QString::fromStdString(Option.Name), FLanguages);
    Item->setData(LocaleRole, Option.Locale);
    if (Option.Completeness < 0)
    {
      Item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    }
    if (Option.Locale == Chosen)
    {
      FLanguages->setCurrentItem(Item);
    }
  }
  connect(FLanguages, &QListWidget::currentItemChanged, this, &PreferencesDialog::UpdateControls);
  GroupLayout->addWidget(FLanguages, 1);
  FLanguageChange = new QLabel(tr("Changes will apply on the next start."), Group);
  GroupLayout->addWidget(FLanguageChange);
  // As LanguagesGetMoreButton: linscp's translations, and how to add one.
  QPushButton * GetMore = new QPushButton(tr("Get &more..."), Group);
  connect(GetMore, &QPushButton::clicked, this, []()
  {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/99ecarvalho/linscp/blob/main/doc/qt/translations.md")));
  });
  QHBoxLayout * GetMoreRow = new QHBoxLayout();
  GetMoreRow->addStretch(1);
  GetMoreRow->addWidget(GetMore);
  GroupLayout->addLayout(GetMoreRow);
  Layout->addWidget(Group, 1);
  return Page;
}

// As TPreferencesDialog::UpdateControls' languages, presets and
// endurance parts.
void PreferencesDialog::UpdateControls()
{
  if (!FBuilt)
  {
    return;
  }
  FLanguageChange->setVisible((FLanguages->currentItem() != nullptr) && (Locale() != CoreBridge::AppliedLocale()));
  const int Row = FPresetsList->currentRow();
  FEditPreset->setEnabled(Row >= 0);
  FDuplicatePreset->setEnabled(Row >= 0);
  FRemovePreset->setEnabled(Row >= 1);
  FUpPreset->setEnabled(Row > 1);
  FDownPreset->setEnabled((Row >= 1) && (Row < FPresetsList->count() - 1));
  FResumeThreshold->setEnabled(FResumeSmart->isChecked());
  const int ColorRow = FFileColorsList->currentRow();
  FEditFileColor->setEnabled(ColorRow >= 0);
  FRemoveFileColor->setEnabled(ColorRow >= 0);
  FUpFileColor->setEnabled(ColorRow > 0);
  FDownFileColor->setEnabled((ColorRow >= 0) && (ColorRow < FFileColorsList->count() - 1));
  FReopenAutoSeconds->setEnabled(FReopenAuto->isChecked());
  FReopenAutoIdleSeconds->setEnabled(FReopenAutoIdle->isChecked());
  FReopenAutoStallSeconds->setEnabled(FReopenAutoStall->isChecked());
  // As TPreferencesDialog::UpdateControls' logging, network and storage.
  for (QWidget * Widget : std::initializer_list<QWidget *>{ FLogLevel, FLogFile, FLogAppend, FLogOverwrite, FLogMaxSize, FLogMaxCount })
  {
    Widget->setEnabled(FLogging->isChecked());
  }
  FLogMaxSizeCombo->setEnabled(FLogging->isChecked() && FLogMaxSize->isChecked());
  FLogMaxCountSpin->setEnabled(FLogging->isChecked() && FLogMaxCount->isChecked());
  FActionsLogFile->setEnabled(FLogActions->isChecked());
  FEditorLargeFileSize->setEnabled(FEditorWarnLargeFile->isChecked());
  // As UpdateControls: the interface's change noted.
  FInterfaceChange->setVisible(FExplorerInterface->isChecked() != FExplorerAtStart);
  // As UpdateControls' environment, panels and updates.
  FConfirmDDTransfer->setEnabled(FConfirmTransferring->isChecked());
  FBeepOnFinishAfter->setEnabled(FBeepOnFinish->isChecked());
  FRefreshRemotePanelInterval->setEnabled(FRefreshRemotePanel->isChecked());
  // As UpdateControls: the warning for the temporary folder's way only.
  FDDWarnLackOfTempSpace->setEnabled(!FDDFakeFile->isChecked());
  FDragExtStatus->setEnabled(FDDFakeFile->isEnabled() && FDDFakeFile->isChecked());
  FDDDrives->setEnabled(FDDFakeFile->isEnabled() && FDDFakeFile->isChecked());
  FDDDrivesLabel->setEnabled(FDDDrives->isEnabled());
  FUpdatesProxyHost->setEnabled(FUpdatesProxy->isChecked());
  FUpdatesProxyPort->setEnabled(FUpdatesProxy->isChecked());
  // As UpdateControls' editors.
  const int EditorRow = FEditorsList->indexOfTopLevelItem(FEditorsList->currentItem());
  FEditEditor->setEnabled(EditorRow >= 0);
  FRemoveEditor->setEnabled(EditorRow >= 0);
  FUpEditor->setEnabled(EditorRow > 0);
  FDownEditor->setEnabled((EditorRow >= 0) && (EditorRow < FEditorsList->topLevelItemCount() - 1));
  // As UpdateControls' PuTTY part.
  bool SiteCommand = false;
  bool PasswordCommand = false;
  CoreBridge::PuttyCommandPatterns(FPuttyPath->text().toStdString(), SiteCommand, PasswordCommand);
  const bool AnyPuttyPath = !FPuttyPath->text().isEmpty();
  FPuttyPassword->setEnabled(AnyPuttyPath && !PasswordCommand);
  FAutoOpenInPutty->setEnabled(AnyPuttyPath);
  FTelnetForFtpInPutty->setEnabled(AnyPuttyPath && !SiteCommand);
  if (FPuttyRegistryStorageKey != nullptr)
  {
    FPuttyRegistryStorageKey->setEnabled(AnyPuttyPath && !SiteCommand);
    FPuttyRegistryStorageKeyLabel->setEnabled(FPuttyRegistryStorageKey->isEnabled());
  }
  // As UpdateControls' commands: Edit for a custom command, Configure (in its
  // place) for an extension; up and down within its own list.
  const int CommandRow = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  const int Extension = ExtensionIndex(CommandRow);
  const int CustomCount = static_cast<int>(FCommands.size());
  FEditCommand->setEnabled((CommandRow >= 0) && (Extension < 0));
  FEditCommand->setVisible(Extension < 0);
  FConfigureCommand->setVisible(Extension >= 0);
  FConfigureCommand->setEnabled(Extension >= 0);
  FRemoveCommand->setEnabled(CommandRow >= 0);
  FUpCommand->setEnabled((CommandRow >= 0) && ((Extension >= 0) ? (Extension > 0) : (CommandRow > 0)));
  FDownCommand->setEnabled((CommandRow >= 0) &&
    ((Extension >= 0) ? (CommandRow < FCommandsList->topLevelItemCount() - 1) : (CommandRow < CustomCount - 1)));
  FCustomIpEdit->setEnabled(FCustomIp->isChecked());
  FPortMin->setEnabled(FPortLimit->isChecked());
  FPortMax->setEnabled(FPortLimit->isChecked());
  if (FCustomIniEdit != nullptr)
  {
    FCustomIniEdit->setEnabled(FCustomIniStorage->isChecked());
  }
  FTemporaryEdit->setEnabled(FCustomTemporary->isChecked());
  FConfirmTemporaryCleanup->setEnabled(FTemporaryCleanup->isChecked());
  // As UpdateControls: the workspace's name and passwords with the saving.
  FAutoWorkspace->setEnabled(FAutoSaveWorkspace->isChecked());
  FAutoSaveWorkspacePasswords->setEnabled(FAutoSaveWorkspace->isChecked() && !CoreBridge::PasswordStoringDisabled());
}

// As TPreferencesDialog::UpdateCopyParamListView: the default first, then
// each preset.
void PreferencesDialog::LoadPresets()
{
  const int Row = FPresetsList->currentRow();
  FPresetsList->clear();
  FPresetsList->addItem(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamDefault)).remove(QLatin1Char('&')));
  for (const CoreBridge::TTransferPreset & Preset : FPresets)
  {
    // WinSCP's Auto column, as the item's tip.
    QListWidgetItem * Item = new QListWidgetItem(PresetCaption(Preset.Name), FPresetsList);
    if (Preset.HasRule)
    {
      Item->setToolTip(QString::fromStdString(CoreBridge::TransferPresetRuleInfo(Preset.Rule)));
    }
  }
  FPresetsList->setCurrentRow(qBound(0, Row, FPresetsList->count() - 1));
  if (FCopyParamAutoSelectNotice != nullptr)
  {
    FCopyParamAutoSelectNotice->setEnabled(std::any_of(FPresets.begin(), FPresets.end(),
      [](const CoreBridge::TTransferPreset & Preset) { return Preset.HasRule; }));
  }
}

QWidget * PreferencesDialog::MakeWindowPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  // As WorkspacesGroup: the workspace saved on exit, its name (the one in
  // use, else New Workspace), with passwords; and how a workspace opens.
  QGroupBox * Workspaces = new QGroupBox(tr("Workspaces"), Page);
  QGridLayout * WorkspacesLayout = new QGridLayout(Workspaces);
  FAutoSaveWorkspace = new QCheckBox(tr("&Automatically save workspace on exit"), Workspaces);
  FAutoSaveWorkspace->setChecked(Preferences.AutoSaveWorkspace);
  QLabel * AutoWorkspaceLabel = new QLabel(tr("&Default workspace name:"), Workspaces);
  FAutoWorkspace = new QComboBox(Workspaces);
  FAutoWorkspace->setEditable(true);
  FAutoWorkspace->setInsertPolicy(QComboBox::NoInsert);
  for (const std::string & Name : CoreBridge::WorkspaceNames())
  {
    FAutoWorkspace->addItem(QString::fromStdString(Name));
  }
  FAutoWorkspace->setCurrentText(!Preferences.AutoWorkspace.empty() ? QString::fromStdString(Preferences.AutoWorkspace) :
    QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NewWorkspace)));
  AutoWorkspaceLabel->setBuddy(FAutoWorkspace);
  FAutoSaveWorkspacePasswords = new QCheckBox(tr("Save &passwords (not recommended)"), Workspaces);
  FAutoSaveWorkspacePasswords->setChecked(Preferences.AutoSaveWorkspacePasswords);
  FWorkspaceConnectAll = new QCheckBox(tr("&Connect all sessions of opened workspace or site folder"), Workspaces);
  FWorkspaceConnectAll->setChecked(Preferences.WorkspaceConnectAll);
  WorkspacesLayout->addWidget(FAutoSaveWorkspace, 0, 0, 1, 3);
  WorkspacesLayout->addItem(new QSpacerItem(20, 0, QSizePolicy::Fixed), 1, 0);
  WorkspacesLayout->addWidget(AutoWorkspaceLabel, 1, 1);
  WorkspacesLayout->addWidget(FAutoWorkspace, 1, 2);
  WorkspacesLayout->addWidget(FAutoSaveWorkspacePasswords, 2, 1, 1, 2);
  WorkspacesLayout->addWidget(FWorkspaceConnectAll, 3, 0, 1, 3);
  WorkspacesLayout->setColumnStretch(2, 1);
  connect(FAutoSaveWorkspace, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  Layout->addWidget(Workspaces);
  QGroupBox * Path = new QGroupBox(tr("Path in window title"), Page);
  QVBoxLayout * PathLayout = new QVBoxLayout(Path);
  FPathFull = new QRadioButton(tr("Show &full path"), Path);
  FPathShort = new QRadioButton(tr("Sho&w short path"), Path);
  FPathNone = new QRadioButton(tr("Do &not show"), Path);
  FPathFull->setChecked(Preferences.PathInCaption == TPreferences::TPathInCaption::Full);
  FPathShort->setChecked(Preferences.PathInCaption == TPreferences::TPathInCaption::Short);
  FPathNone->setChecked(Preferences.PathInCaption == TPreferences::TPathInCaption::None);
  PathLayout->addWidget(FPathFull);
  PathLayout->addWidget(FPathShort);
  PathLayout->addWidget(FPathNone);
  Layout->addWidget(Path);
  QGroupBox * Misc = new QGroupBox(tr("Miscellaneous"), Page);
  QVBoxLayout * MiscLayout = new QVBoxLayout(Misc);
  // A tray only where the desktop has one (most Linux desktops do, GNOME's
  // only with an extension).
  FMinimizeToTray = new QCheckBox(tr("&Minimize main window to taskbar status area (system tray)"), Misc);
  FMinimizeToTray->setChecked(Preferences.MinimizeToTray);
  FMinimizeToTray->setEnabled(QSystemTrayIcon::isSystemTrayAvailable() || Preferences.MinimizeToTray);
  MiscLayout->addWidget(FMinimizeToTray);
  FExternalSessionInExistingInstance = new QCheckBox(tr("Open new externally initiated sessions in &existing window"), Misc);
  FExternalSessionInExistingInstance->setChecked(Preferences.ExternalSessionInExistingInstance);
  MiscLayout->addWidget(FExternalSessionInExistingInstance);
  FShowLogin = new QCheckBox(tr("&Show Login dialog on startup and when the last session is closed"), Misc);
  FShowLogin->setChecked(Preferences.ShowLoginWhenNoSession);
  MiscLayout->addWidget(FShowLogin);
  FKeepOpen = new QCheckBox(tr("&Keep main window open when the last session is closed"), Misc);
  FKeepOpen->setChecked(Preferences.KeepOpenWhenNoSession);
  MiscLayout->addWidget(FKeepOpen);
  FSilentDisconnect = new QCheckBox(tr("Do not p&opup error message when idle session is disconnected"), Misc);
  FSilentDisconnect->setChecked(Preferences.SessionSilentDisconnect);
  MiscLayout->addWidget(FSilentDisconnect);
  FTabTruncation = new QCheckBox(tr("&Truncate tab titles when they do not fit to window"), Misc);
  FTabTruncation->setChecked(Preferences.SessionTabCaptionTruncation);
  MiscLayout->addWidget(FTabTruncation);
  Layout->addWidget(Misc);
  // linscp's: WinSCP's own icons are not reusable (IconSet.h).
  QGroupBox * Icons = new QGroupBox(tr("Icons"), Page);
  QFormLayout * IconsLayout = new QFormLayout(Icons);
  FIconSet = new QComboBox(Icons);
  for (const IconSet::TSet & Set : IconSet::Sets())
  {
    FIconSet->addItem(Set.Name, Set.Id);
  }
  const int IconSetIndex = FIconSet->findData(Preferences.IconSet.empty() ? IconSet::DefaultSet() :
    QString::fromStdString(Preferences.IconSet));
  FIconSet->setCurrentIndex(std::max(IconSetIndex, 0));
  IconsLayout->addRow(tr("&Icon set:"), FIconSet);
  Layout->addWidget(Icons);
  Layout->addStretch(1);
  return Page;
}

// As the File colors page: the rules in order, each its mask shown in its
// color.
void PreferencesDialog::LoadFileColors()
{
  const int Row = FFileColorsList->currentRow();
  FFileColorsList->clear();
  for (const TPreferences::TFileColor & Color : FFileColors)
  {
    QListWidgetItem * Item = new QListWidgetItem(QString::fromStdString(Color.Mask), FFileColorsList);
    Item->setForeground(QColor(static_cast<QRgb>(Color.Color)));
  }
  FFileColorsList->setCurrentRow(qBound(-1, Row, FFileColorsList->count() - 1));
}

QWidget * PreferencesDialog::MakeFileColorsPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("File colors"), Page);
  QHBoxLayout * GroupLayout = new QHBoxLayout(Group);
  FFileColorsList = new QListWidget(Group);
  FFileColorsList->setAccessibleName(Group->title());
  connect(FFileColorsList, &QListWidget::currentRowChanged, this, &PreferencesDialog::UpdateControls);
  GroupLayout->addWidget(FFileColorsList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  QPushButton * Add = new QPushButton(tr("&Add..."), Group);
  connect(Add, &QPushButton::clicked, this, [this]()
  {
    TPreferences::TFileColor Color;
    Color.Mask = "*.*";
    if (FileColorDialog(Color))
    {
      AddFileColor(Color);
    }
  });
  FEditFileColor = new QPushButton(tr("&Edit..."), Group);
  connect(FEditFileColor, &QPushButton::clicked, this, [this]()
  {
    const int Row = FFileColorsList->currentRow();
    if (Row >= 0)
    {
      TPreferences::TFileColor Color = FFileColors[Row];
      if (FileColorDialog(Color))
      {
        EditFileColor(Color);
      }
    }
  });
  FRemoveFileColor = new QPushButton(tr("&Remove"), Group);
  connect(FRemoveFileColor, &QPushButton::clicked, this, &PreferencesDialog::RemoveFileColor);
  FUpFileColor = new QPushButton(tr("&Up"), Group);
  connect(FUpFileColor, &QPushButton::clicked, this, [this]() { MoveFileColor(-1); });
  FDownFileColor = new QPushButton(tr("&Down"), Group);
  connect(FDownFileColor, &QPushButton::clicked, this, [this]() { MoveFileColor(1); });
  for (QPushButton * Button : { Add, FEditFileColor, FRemoveFileColor, FUpFileColor, FDownFileColor })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  GroupLayout->addLayout(Buttons);
  Layout->addWidget(Group, 1);
  LoadFileColors();
  return Page;
}

// As WinSCP's file color dialog (the Select dialog with its color): the
// mask, and the color to show matching files in.
bool PreferencesDialog::FileColorDialog(TPreferences::TFileColor & Color)
{
  QDialog Dialog(this);
  Dialog.setWindowTitle(tr("File color"));
  QFormLayout * Form = new QFormLayout(&Dialog);
  QLineEdit * Mask = new QLineEdit(QString::fromStdString(Color.Mask), &Dialog);
  Mask->setMinimumWidth(300);
  Form->addRow(tr("File &mask:"), Mask);
  QColor Chosen(static_cast<QRgb>(Color.Color));
  QPushButton * ColorButton = new QPushButton(tr("&Color"), &Dialog);
  const auto Show = [ColorButton, &Chosen]()
  {
    ColorButton->setStyleSheet(QStringLiteral("color: %1").arg(Chosen.name()));
  };
  Show();
  connect(ColorButton, &QPushButton::clicked, &Dialog, [&Dialog, &Chosen, Show]()
  {
    const QColor Picked = QColorDialog::getColor(Chosen, &Dialog);
    if (Picked.isValid())
    {
      Chosen = Picked;
      Show();
    }
  });
  Form->addRow(ColorButton);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Form->addRow(Box);
  if ((Dialog.exec() != QDialog::Accepted) || Mask->text().trimmed().isEmpty())
  {
    return false;
  }
  Color.Mask = Mask->text().trimmed().toStdString();
  Color.Color = Chosen.rgb() & 0xffffff;
  return true;
}

void PreferencesDialog::AddFileColor(const TPreferences::TFileColor & Color)
{
  const int At = FFileColorsList->currentRow() + 1;
  FFileColors.insert(FFileColors.begin() + At, Color);
  LoadFileColors();
  FFileColorsList->setCurrentRow(At);
}

void PreferencesDialog::EditFileColor(const TPreferences::TFileColor & Color)
{
  const int Row = FFileColorsList->currentRow();
  if (Row >= 0)
  {
    FFileColors[Row] = Color;
    LoadFileColors();
  }
}

void PreferencesDialog::RemoveFileColor()
{
  const int Row = FFileColorsList->currentRow();
  if (Row >= 0)
  {
    FFileColors.erase(FFileColors.begin() + Row);
    LoadFileColors();
  }
}

void PreferencesDialog::MoveFileColor(int By)
{
  const int Row = FFileColorsList->currentRow();
  const int To = Row + By;
  if ((Row >= 0) && (To >= 0) && (To < FFileColorsList->count()))
  {
    std::swap(FFileColors[Row], FFileColors[To]);
    LoadFileColors();
    FFileColorsList->setCurrentRow(To);
  }
}

QWidget * PreferencesDialog::MakeNetworkPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Incoming = new QGroupBox(tr("Incoming FTP connections (active mode)"), Page);
  QGridLayout * IncomingLayout = new QGridLayout(Incoming);
  FRetrieveIp = new QRadioButton(tr("Retrieve the external IP address from &operating system"), Incoming);
  FCustomIp = new QRadioButton(tr("Use the &following external IP address:"), Incoming);
  FCustomIpEdit = new QLineEdit(QString::fromStdString(Preferences.ExternalIpAddress), Incoming);
  FRetrieveIp->setChecked(Preferences.ExternalIpAddress.empty());
  FCustomIp->setChecked(!Preferences.ExternalIpAddress.empty());
  FPortLimit = new QCheckBox(tr("Limit listening &ports to:"), Incoming);
  FPortLimit->setChecked((Preferences.LocalPortNumberMin > 0) && (Preferences.LocalPortNumberMax >= Preferences.LocalPortNumberMin));
  FPortMin = new QSpinBox(Incoming);
  FPortMax = new QSpinBox(Incoming);
  for (QSpinBox * Spin : { FPortMin, FPortMax })
  {
    Spin->setRange(1024, 65535);
  }
  // As TPreferencesDialog: none set shows the private ports.
  FPortMin->setValue((Preferences.LocalPortNumberMin > 0) ? Preferences.LocalPortNumberMin : 49152);
  FPortMax->setValue((Preferences.LocalPortNumberMax >= FPortMin->value()) ? Preferences.LocalPortNumberMax : 65535);
  IncomingLayout->addWidget(FRetrieveIp, 0, 0, 1, 3);
  IncomingLayout->addWidget(FCustomIp, 1, 0);
  IncomingLayout->addWidget(FCustomIpEdit, 1, 1, 1, 2);
  IncomingLayout->addWidget(FPortLimit, 2, 0);
  IncomingLayout->addWidget(FPortMin, 2, 1);
  IncomingLayout->addWidget(FPortMax, 2, 2);
  Layout->addWidget(Incoming);
  QGroupBox * Connections = new QGroupBox(tr("Connections"), Page);
  QVBoxLayout * ConnectionsLayout = new QVBoxLayout(Connections);
  FTryFtp = new QCheckBox(tr("When SFTP connection is &rejected, knock FTP port"), Connections);
  FTryFtp->setChecked(Preferences.TryFtpWhenSshFails);
  ConnectionsLayout->addWidget(FTryFtp);
  Layout->addWidget(Connections);
  Layout->addStretch(1);
  for (QAbstractButton * Button : std::initializer_list<QAbstractButton *>{ FRetrieveIp, FCustomIp, FPortLimit })
  {
    connect(Button, &QAbstractButton::toggled, this, &PreferencesDialog::UpdateControls);
  }
  return Page;
}

// The master password and the trusted certification authorities are not
// here yet (ADR 0014).
QWidget * PreferencesDialog::MakeSecurityPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  // As MasterPasswordGroup: what it does is done at once, not on OK.
  QGroupBox * Master = new QGroupBox(tr("Master password"), Page);
  QHBoxLayout * MasterLayout = new QHBoxLayout(Master);
  FUseMasterPassword = new QCheckBox(tr("&Use master password"), Master);
  FUseMasterPassword->setChecked(CoreBridge::UseMasterPassword());
  connect(FUseMasterPassword, &QCheckBox::clicked, this, &PreferencesDialog::UseMasterPasswordClicked);
  MasterLayout->addWidget(FUseMasterPassword, 1);
  FSetMasterPassword = new QPushButton(tr("&Change master password..."), Master);
  FSetMasterPassword->setEnabled(CoreBridge::UseMasterPassword());
  connect(FSetMasterPassword, &QPushButton::clicked, this, [this]()
  {
    if (CanSetMasterPassword())
    {
      ChangeMasterPassword(CoreBridge::TText::MasterPasswordChanged);
    }
  });
  MasterLayout->addWidget(FSetMasterPassword);
  Layout->addWidget(Master);
  QGroupBox * Authentication = new QGroupBox(tr("Authentication"), Page);
  QFormLayout * Form = new QFormLayout(Authentication);
  FRememberPassword = new QCheckBox(tr("Remember &password for duration of session"), Authentication);
  FRememberPassword->setChecked(Preferences.SessionRememberPassword);
  Form->addRow(FRememberPassword);
  FAuthAgent = new QComboBox(Authentication);
  FAuthAgent->addItems({ tr("PuTTY Pageant"), tr("OpenSSH ssh-agent") });
  FAuthAgent->setCurrentIndex(qBound(0, Preferences.AuthAgent, 1));
  Form->addRow(tr("Aut&hentication agent:"), FAuthAgent);
  Layout->addWidget(Authentication);
  // As SshHostCAsGroup.
  QGroupBox * HostCAs = new QGroupBox(tr("Trusted host certification authorities"), Page);
  QGridLayout * HostCAsLayout = new QGridLayout(HostCAs);
  FHostCAs = new QTreeWidget(HostCAs);
  FHostCAs->setAccessibleName(HostCAs->title());
  FHostCAs->setHeaderLabels({ tr("Name"), tr("Hosts") });
  FHostCAs->setRootIsDecorated(false);
  connect(FHostCAs, &QTreeWidget::itemSelectionChanged, this, &PreferencesDialog::UpdateHostCAControls);
  connect(FHostCAs, &QTreeWidget::itemDoubleClicked, this, [this]() { if (FEditHostCA->isEnabled()) EditHostCA(false); });
  HostCAsLayout->addWidget(FHostCAs, 0, 0, 4, 1);
  FAddHostCA = new QPushButton(tr("&Add..."), HostCAs);
  connect(FAddHostCA, &QPushButton::clicked, this, [this]() { EditHostCA(true); });
  FRemoveHostCA = new QPushButton(tr("&Remove"), HostCAs);
  connect(FRemoveHostCA, &QPushButton::clicked, this, [this]()
  {
    const int Index = FHostCAs->indexOfTopLevelItem(FHostCAs->currentItem());
    if (Index >= 0)
    {
      FHostCAList.erase(FHostCAList.begin() + Index);
      LoadHostCAs();
    }
  });
  FEditHostCA = new QPushButton(tr("&Edit..."), HostCAs);
  connect(FEditHostCA, &QPushButton::clicked, this, [this]() { EditHostCA(false); });
  HostCAsLayout->addWidget(FAddHostCA, 0, 1);
  HostCAsLayout->addWidget(FEditHostCA, 1, 1);
  HostCAsLayout->addWidget(FRemoveHostCA, 2, 1);
  FHostCAList = CoreBridge::SshHostCAs();
  FHostCAsFromPuTTY = new QCheckBox(tr("&Load authorities from PuTTY"), HostCAs);
  FHostCAsFromPuTTY->setChecked(CoreBridge::SshHostCAsFromPuTTY());
  connect(FHostCAsFromPuTTY, &QCheckBox::toggled, this, &PreferencesDialog::LoadHostCAs);
  HostCAsLayout->addWidget(FHostCAsFromPuTTY, 4, 0);
  FConfigureHostCAs = new QPushButton(tr("&Edit in PuTTY..."), HostCAs);
  connect(FConfigureHostCAs, &QPushButton::clicked, this, [this]()
  {
    try
    {
      CoreBridge::ConfigureSshHostCAsInPutty();
    }
    catch (const CoreBridge::TCoreError & E)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
  });
  HostCAsLayout->addWidget(FConfigureHostCAs, 0, 1);
#if !defined(Q_OS_WIN)
  // PuTTY keeps them in Windows' registry; elsewhere there are none to load.
  FHostCAsFromPuTTY->setVisible(FHostCAsFromPuTTY->isChecked());
#endif
  Layout->addWidget(HostCAs, 1);
  LoadHostCAs();
  return Page;
}

QWidget * PreferencesDialog::MakeLoggingPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Session = new QGroupBox(tr("Session log"), Page);
  QGridLayout * SessionLayout = new QGridLayout(Session);
  FLogging = new QCheckBox(tr("Enable &session logging on level:"), Session);
  FLogging->setChecked(Preferences.Logging);
  FLogLevel = new QComboBox(Session);
  FLogLevel->addItems({ tr("Reduced"), tr("Normal"), tr("Debug 1"), tr("Debug 2") });
  FLogLevel->setCurrentIndex(qBound(0, Preferences.LogLevel, 3));
  SessionLayout->addWidget(FLogging, 0, 0);
  SessionLayout->addWidget(FLogLevel, 0, 1);
  FLogFile = new QLineEdit(QString::fromStdString(!Preferences.LogFileName.empty() ? Preferences.LogFileName : Preferences.DefaultLogFileName), Session);
  QLabel * FileLabel = new QLabel(tr("&Log path:"), Session);
  FileLabel->setBuddy(FLogFile);
  SessionLayout->addWidget(FileLabel, 1, 0);
  SessionLayout->addWidget(FLogFile, 1, 1);
  FLogAppend = new QRadioButton(tr("Appe&nd"), Session);
  FLogOverwrite = new QRadioButton(tr("&Overwrite"), Session);
  FLogAppend->setChecked(Preferences.LogFileAppend);
  FLogOverwrite->setChecked(!Preferences.LogFileAppend);
  QHBoxLayout * Mode = new QHBoxLayout();
  Mode->addWidget(FLogAppend);
  Mode->addWidget(FLogOverwrite);
  SessionLayout->addLayout(Mode, 2, 1);
  FLogMaxSize = new QCheckBox(tr("&Rotate log files after reaching"), Session);
  FLogMaxSize->setChecked(Preferences.LogMaxSize > 0);
  FLogMaxSizeCombo = new QComboBox(Session);
  FLogMaxSizeCombo->setEditable(true);
  FLogMaxSizeCombo->addItems({ QStringLiteral("1M"), QStringLiteral("10M"), QStringLiteral("100M"), QStringLiteral("1G") });
  FLogMaxSizeCombo->setEditText(QString::fromStdString(CoreBridge::TextOfSize((Preferences.LogMaxSize > 0) ? Preferences.LogMaxSize : 1024 * 1024)));
  SessionLayout->addWidget(FLogMaxSize, 3, 0);
  SessionLayout->addWidget(FLogMaxSizeCombo, 3, 1);
  FLogMaxCount = new QCheckBox(tr("&Delete old log files, keep"), Session);
  FLogMaxCount->setChecked(Preferences.LogMaxCount > 0);
  FLogMaxCountSpin = new QSpinBox(Session);
  FLogMaxCountSpin->setRange(1, 999);
  FLogMaxCountSpin->setSuffix(tr(" files"));
  FLogMaxCountSpin->setValue((Preferences.LogMaxCount > 0) ? Preferences.LogMaxCount : 5);
  SessionLayout->addWidget(FLogMaxCount, 4, 0);
  SessionLayout->addWidget(FLogMaxCountSpin, 4, 1);
  FLogSensitive = new QCheckBox(tr("Log pass&words and other sensitive information"), Session);
  FLogSensitive->setChecked(Preferences.LogSensitive);
  SessionLayout->addWidget(FLogSensitive, 5, 0, 1, 2);
  Layout->addWidget(Session);
  QGroupBox * Xml = new QGroupBox(tr("XML log"), Page);
  QVBoxLayout * XmlLayout = new QVBoxLayout(Xml);
  FLogActions = new QCheckBox(tr("Enable &XML logging to file:"), Xml);
  FLogActions->setChecked(Preferences.LogActions);
  FActionsLogFile = new QLineEdit(QString::fromStdString(Preferences.ActionsLogFileName), Xml);
  XmlLayout->addWidget(FLogActions);
  XmlLayout->addWidget(FActionsLogFile);
  Layout->addWidget(Xml);
  Layout->addStretch(1);
  for (QAbstractButton * Button : std::initializer_list<QAbstractButton *>{ FLogging, FLogMaxSize, FLogMaxCount, FLogActions })
  {
    connect(Button, &QAbstractButton::toggled, this, &PreferencesDialog::UpdateControls);
  }
  return Page;
}

QWidget * PreferencesDialog::MakeStoragePage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Storage = new QGroupBox(tr("Configuration storage"), Page);
  QGridLayout * StorageLayout = new QGridLayout(Storage);
  FStorage = Preferences.Storage;
  if ((Preferences.Storage == TPreferences::TStorage::Explicit) || (Preferences.Storage == TPreferences::TStorage::None))
  {
    // A file given on the command line (and on POSIX, always the INI file,
    // ADR 0009): the configuration is there, nothing to move.
    FStorageLabel = new QLabel((Preferences.Storage == TPreferences::TStorage::None) ? tr("Not stored.") :
      QString::fromStdString(Preferences.StorageFile), Storage);
    FStorageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    StorageLayout->addWidget(FStorageLabel, 0, 0);
    Storage->setEnabled(Preferences.Storage != TPreferences::TStorage::None);
  }
  else
  {
    FRegistryStorage = new QRadioButton(tr("Windows re&gistry"), Storage);
    FAutomaticIniStorage = new QRadioButton(tr("&Automatic INI file"), Storage);
    FStorageLabel = new QLabel(QString::fromStdString(Preferences.StorageFile), Storage);
    FCustomIniStorage = new QRadioButton(tr("Custo&m INI file:"), Storage);
    FCustomIniEdit = new QLineEdit(QString::fromStdString(Preferences.CustomIniFile), Storage);
    FRegistryStorage->setChecked(Preferences.Storage == TPreferences::TStorage::Registry);
    FAutomaticIniStorage->setChecked(Preferences.Storage == TPreferences::TStorage::AutomaticIni);
    FCustomIniStorage->setChecked(Preferences.Storage == TPreferences::TStorage::CustomIni);
    StorageLayout->addWidget(FRegistryStorage, 0, 0, 1, 2);
    StorageLayout->addWidget(FAutomaticIniStorage, 1, 0);
    StorageLayout->addWidget(FStorageLabel, 1, 1);
    StorageLayout->addWidget(FCustomIniStorage, 2, 0);
    StorageLayout->addWidget(FCustomIniEdit, 2, 1);
    connect(FCustomIniStorage, &QRadioButton::toggled, this, &PreferencesDialog::UpdateControls);
  }
  Layout->addWidget(Storage);
  QGroupBox * Temporary = new QGroupBox(tr("Temporary directory"), Page);
  QGridLayout * TemporaryLayout = new QGridLayout(Temporary);
  TemporaryLayout->addWidget(new QLabel(tr("Specify where to temporarily store edited and downloaded files."), Temporary), 0, 0, 1, 2);
  FSystemTemporary = new QRadioButton(tr("&Use temporary directory of system"), Temporary);
  FCustomTemporary = new QRadioButton(tr("Use this &directory:"), Temporary);
  FTemporaryEdit = new QLineEdit(Temporary);
  // As TPreferencesDialog: none set shows the system's.
  FSystemTemporary->setChecked(Preferences.TemporaryDirectory.empty());
  FCustomTemporary->setChecked(!Preferences.TemporaryDirectory.empty());
  FTemporaryEdit->setText(Preferences.TemporaryDirectory.empty() ? QString::fromStdString(CoreBridge::TemporaryDirectoryBase()) :
    QString::fromStdString(Preferences.TemporaryDirectory));
  TemporaryLayout->addWidget(FSystemTemporary, 1, 0, 1, 2);
  TemporaryLayout->addWidget(FCustomTemporary, 2, 0);
  TemporaryLayout->addWidget(FTemporaryEdit, 2, 1);
  connect(FCustomTemporary, &QRadioButton::toggled, this, &PreferencesDialog::UpdateControls);
  // As TemporaryDirectoryCleanupCheck and the rest.
  FTemporaryCleanup = new QCheckBox(tr("&Cleanup obsolete temporary directories on startup"), Temporary);
  FTemporaryCleanup->setChecked(Preferences.TemporaryDirectoryCleanup);
  FConfirmTemporaryCleanup = new QCheckBox(tr("&Ask before cleanup"), Temporary);
  FConfirmTemporaryCleanup->setChecked(Preferences.ConfirmTemporaryDirectoryCleanup);
  FTemporaryAppendSession = new QCheckBox(tr("Append &session name to temporary path"), Temporary);
  FTemporaryAppendSession->setChecked(Preferences.TemporaryDirectoryAppendSession);
  FTemporaryAppendPath = new QCheckBox(tr("Append remote &path to temporary path"), Temporary);
  FTemporaryAppendPath->setChecked(Preferences.TemporaryDirectoryAppendPath);
  FTemporaryDeterministic = new QCheckBox(tr("Keep temporary copies of remote files in &deterministic paths"), Temporary);
  FTemporaryDeterministic->setChecked(Preferences.TemporaryDirectoryDeterministic);
  TemporaryLayout->addWidget(FTemporaryCleanup, 3, 0, 1, 2);
  QHBoxLayout * ConfirmRow = new QHBoxLayout();
  ConfirmRow->addSpacing(20);
  ConfirmRow->addWidget(FConfirmTemporaryCleanup);
  TemporaryLayout->addLayout(ConfirmRow, 4, 0, 1, 2);
  TemporaryLayout->addWidget(FTemporaryAppendSession, 5, 0, 1, 2);
  TemporaryLayout->addWidget(FTemporaryAppendPath, 6, 0, 1, 2);
  TemporaryLayout->addWidget(FTemporaryDeterministic, 7, 0, 1, 2);
  connect(FTemporaryCleanup, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  Layout->addWidget(Temporary);
  QGroupBox * Other = new QGroupBox(tr("Other"), Page);
  QFormLayout * OtherLayout = new QFormLayout(Other);
  FRandomSeedEdit = new QLineEdit(QString::fromStdString(Preferences.RandomSeedFile), Other);
  OtherLayout->addRow(tr("&Random seed file:"), FRandomSeedEdit);
  Layout->addWidget(Other);
  Layout->addStretch(1);
  return Page;
}

QWidget * PreferencesDialog::MakeTransferPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Transfer settings presets"), Page);
  QHBoxLayout * GroupLayout = new QHBoxLayout(Group);
  FPresetsList = new QListWidget(Group);
  FPresetsList->setAccessibleName(Group->title());
  connect(FPresetsList, &QListWidget::currentRowChanged, this, &PreferencesDialog::UpdateControls);
  connect(FPresetsList, &QListWidget::itemDoubleClicked, this, [this]() { EditPresetDialog(false, false); });
  GroupLayout->addWidget(FPresetsList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  QPushButton * Add = new QPushButton(tr("&Add..."), Group);
  connect(Add, &QPushButton::clicked, this, [this]() { EditPresetDialog(true, false); });
  FEditPreset = new QPushButton(tr("&Edit..."), Group);
  connect(FEditPreset, &QPushButton::clicked, this, [this]() { EditPresetDialog(false, false); });
  FDuplicatePreset = new QPushButton(tr("Du&plicate..."), Group);
  connect(FDuplicatePreset, &QPushButton::clicked, this, [this]() { EditPresetDialog(true, true); });
  FRemovePreset = new QPushButton(tr("&Remove"), Group);
  connect(FRemovePreset, &QPushButton::clicked, this, &PreferencesDialog::RemoveSelected);
  FUpPreset = new QPushButton(tr("&Up"), Group);
  connect(FUpPreset, &QPushButton::clicked, this, [this]() { MoveSelected(-1); });
  FDownPreset = new QPushButton(tr("&Down"), Group);
  connect(FDownPreset, &QPushButton::clicked, this, [this]() { MoveSelected(1); });
  for (QPushButton * Button : { Add, FEditPreset, FDuplicatePreset, FRemovePreset, FUpPreset, FDownPreset })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  GroupLayout->addLayout(Buttons);
  Layout->addWidget(Group, 1);
  // As CopyParamAutoSelectNoticeCheck: enabled once a preset has a rule.
  FCopyParamAutoSelectNotice = new QCheckBox(tr("&Announce when transfer settings preset is autoselected"), Page);
  FCopyParamAutoSelectNotice->setChecked(Preferences.CopyParamAutoSelectNotice);
  Layout->addWidget(FCopyParamAutoSelectNotice);
  LoadPresets();
  return Page;
}

// As TPreferencesDialog::AddEditCopyParam: the preset's dialog, a name that
// is not taken, then into the list.
void PreferencesDialog::EditPresetDialog(bool Add, bool Duplicate)
{
  const int Row = FPresetsList->currentRow();
  CoreBridge::TTransferSettings Settings = (Row <= 0) ? FDefaultSettings : FPresets[Row - 1].Settings;
  const bool Default = !Add && (Row <= 0);
  // The preset's own name (its accelerator kept), not its caption.
  QString Name = (Add && !Duplicate) ? QString() :
    ((Row > 0) ? QString::fromStdString(FPresets[Row - 1].Name) : FPresetsList->item(Row)->text());
  TransferSettingsDialog Dialog(Settings, Default ? nullptr : &Name, this);
  const bool Existing = (Row > 0) && (!Add || Duplicate);
  Dialog.SetRule(Existing && FPresets[Row - 1].HasRule, Existing ? FPresets[Row - 1].Rule : CoreBridge::TTransferPresetRule(),
    FHasCurrentRuleData ? &FCurrentRuleData : nullptr);
  if (Dialog.exec() == QDialog::Accepted)
  {
    const bool HasRule = Dialog.HasRule();
    const CoreBridge::TTransferPresetRule Rule = Dialog.Rule();
    if (Add)
    {
      AddPreset(Dialog.PresetName(), Dialog.Settings(), &HasRule, &Rule);
    }
    else
    {
      EditSelected(Dialog.PresetName(), Dialog.Settings(), &HasRule, &Rule);
    }
  }
}

bool PreferencesDialog::AddPreset(const QString & Name, const CoreBridge::TTransferSettings & Settings,
  const bool * HasRule, const CoreBridge::TTransferPresetRule * Rule)
{
  for (const CoreBridge::TTransferPreset & Preset : FPresets)
  {
    if (QString::fromStdString(Preset.Name).compare(Name, Qt::CaseInsensitive) == 0)
    {
      QMessageBox::warning(this, windowTitle(),
        QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamDuplicate)).replace(QStringLiteral("%s"), Name));
      return false;
    }
  }
  if (Name.trimmed().isEmpty())
  {
    return false;
  }
  CoreBridge::TTransferPreset Preset;
  Preset.Name = Name.toStdString();
  Preset.Settings = Settings;
  if ((HasRule != nullptr) && (Rule != nullptr))
  {
    Preset.HasRule = *HasRule;
    Preset.Rule = *Rule;
  }
  const int Row = FPresetsList->currentRow();
  // As WinSCP: after the selected one.
  const int At = qBound(0, Row, static_cast<int>(FPresets.size()));
  FPresets.insert(FPresets.begin() + At, Preset);
  LoadPresets();
  FPresetsList->setCurrentRow(At + 1);
  return true;
}

bool PreferencesDialog::EditSelected(const QString & Name, const CoreBridge::TTransferSettings & Settings,
  const bool * HasRule, const CoreBridge::TTransferPresetRule * Rule)
{
  const int Row = FPresetsList->currentRow();
  if (Row <= 0)
  {
    FDefaultSettings = Settings;
    return true;
  }
  for (std::size_t Index = 0; Index < FPresets.size(); Index++)
  {
    if ((static_cast<int>(Index) != Row - 1) &&
        (QString::fromStdString(FPresets[Index].Name).compare(Name, Qt::CaseInsensitive) == 0))
    {
      QMessageBox::warning(this, windowTitle(),
        QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamDuplicate)).replace(QStringLiteral("%s"), Name));
      return false;
    }
  }
  FPresets[Row - 1].Name = Name.toStdString();
  FPresets[Row - 1].Settings = Settings;
  if ((HasRule != nullptr) && (Rule != nullptr))
  {
    FPresets[Row - 1].HasRule = *HasRule;
    FPresets[Row - 1].Rule = *Rule;
  }
  LoadPresets();
  return true;
}

void PreferencesDialog::RemoveSelected()
{
  const int Row = FPresetsList->currentRow();
  if (Row >= 1)
  {
    FPresets.erase(FPresets.begin() + (Row - 1));
    LoadPresets();
  }
}

void PreferencesDialog::MoveSelected(int By)
{
  const int Row = FPresetsList->currentRow();
  const int To = Row + By;
  if ((Row >= 1) && (To >= 1) && (To < FPresetsList->count()))
  {
    std::swap(FPresets[Row - 1], FPresets[To - 1]);
    LoadPresets();
    FPresetsList->setCurrentRow(To);
  }
}

QWidget * PreferencesDialog::MakeBackgroundPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Background transfers"), Page);
  QFormLayout * Form = new QFormLayout(Group);
  FQueueTransfersLimit = new QSpinBox(Group);
  // WinConfiguration's QueueTransferLimitMax.
  FQueueTransfersLimit->setRange(1, 9);
  FQueueTransfersLimit->setValue(Preferences.QueueTransfersLimit);
  Form->addRow(tr("&Maximal number of transfers at the same time:"), FQueueTransfersLimit);
  FQueueKeepDone = new QComboBox(Group);
  // As TPreferencesDialog's: never, 15 s, 1 min, 15 min, 1 h, forever.
  const std::pair<QString, int> Durations[] = { { tr("Never"), 0 }, { tr("15 seconds"), 15 }, { tr("1 minute"), 60 },
    { tr("15 minutes"), 15 * 60 }, { tr("1 hour"), 60 * 60 }, { tr("Forever"), -1 } };
  for (const auto & Duration : Durations)
  {
    FQueueKeepDone->addItem(Duration.first, Duration.second);
  }
  int KeepIndex = 5;
  const int Keep = Preferences.QueueKeepDoneItemsFor;
  if (Keep == 0) KeepIndex = 0;
  else if (Keep < 0) KeepIndex = 5;
  else if (Keep <= 15) KeepIndex = 1;
  else if (Keep <= 60) KeepIndex = 2;
  else if (Keep <= 15 * 60) KeepIndex = 3;
  else if (Keep <= 60 * 60) KeepIndex = 4;
  FQueueKeepDone->setCurrentIndex(KeepIndex);
  Form->addRow(tr("Display &completed transfers in queue for:"), FQueueKeepDone);
  FQueue = new QCheckBox(tr("Transfer in &background by default"), Group);
  FQueue->setChecked(FDefaultSettings.Queue);
  FQueueNoConfirmation = new QCheckBox(tr("&No confirmations for background transfers"), Group);
  FQueueNoConfirmation->setChecked(FDefaultSettings.QueueNoConfirmation);
  FQueueParallel = new QCheckBox(tr("&Use multiple connections for single transfer"), Group);
  FQueueParallel->setChecked(FDefaultSettings.QueueParallel);
  FQueueAutoPopup = new QCheckBox(tr("&Automatically popup prompts of background transfers when idle"), Group);
  FQueueAutoPopup->setChecked(Preferences.QueueAutoPopup);
  // As ParallelTransferCheck: above a size (WinSCP's 100 MB when off).
  FParallelThresholdCheck = new QCheckBox(tr("Use multiple connections for single files abo&ve:"), Group);
  FParallelThresholdCheck->setChecked(Preferences.ParallelTransferThreshold > 0);
  FParallelThreshold = new QSpinBox(Group);
  FParallelThreshold->setRange(1, 1024 * 1024);
  FParallelThreshold->setSuffix(tr(" MB"));
  FParallelThreshold->setValue((Preferences.ParallelTransferThreshold > 0) ? std::max(1, Preferences.ParallelTransferThreshold / 1024) : 100);
  FParallelThreshold->setAccessibleName(tr("Size"));
  FEnableQueueByDefault = new QCheckBox(tr("&Enable queue processing by default"), Group);
  FEnableQueueByDefault->setChecked(Preferences.EnableQueueByDefault);
  for (QCheckBox * Check : { FQueue, FQueueNoConfirmation, FQueueParallel })
  {
    Form->addRow(Check);
  }
  Form->addRow(FParallelThresholdCheck, FParallelThreshold);
  auto UpdateParallel = [this]()
  {
    FParallelThresholdCheck->setEnabled(FQueueParallel->isChecked());
    FParallelThreshold->setEnabled(FQueueParallel->isChecked() && FParallelThresholdCheck->isChecked());
  };
  connect(FQueueParallel, &QCheckBox::toggled, this, UpdateParallel);
  connect(FParallelThresholdCheck, &QCheckBox::toggled, this, UpdateParallel);
  UpdateParallel();
  Form->addRow(FQueueAutoPopup);
  Form->addRow(FEnableQueueByDefault);
  Layout->addWidget(Group);
  // As QueueViewGroup: Options > Queue's, here too.
  QGroupBox * View = new QGroupBox(tr("Queue list"), Page);
  QVBoxLayout * ViewLayout = new QVBoxLayout(View);
  const QString Shows[] = { tr("&Show"), tr("Hide &when empty"), tr("&Hide") };
  const int QueueShow = static_cast<int>(CoreBridge::Layout().QueueShow);
  for (int Show = 0; Show < 3; Show++)
  {
    FQueueView[Show] = new QRadioButton(Shows[Show], View);
    FQueueView[Show]->setChecked(Show == QueueShow);
    ViewLayout->addWidget(FQueueView[Show]);
  }
  Layout->addWidget(View);
  Layout->addStretch(1);
  return Page;
}

QWidget * PreferencesDialog::MakeEndurancePage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Resume = new QGroupBox(tr("Enable transfer resume/transfer to temporary filename for"), Page);
  QGridLayout * ResumeLayout = new QGridLayout(Resume);
  FResumeOn = new QRadioButton(tr("A&ll files"), Resume);
  FResumeSmart = new QRadioButton(tr("Files abo&ve:"), Resume);
  FResumeOff = new QRadioButton(tr("Di&sable"), Resume);
  FResumeThreshold = new QSpinBox(Resume);
  FResumeThreshold->setRange(0, 1024 * 1024 * 1024);
  FResumeThreshold->setSuffix(tr(" KB"));
  FResumeThreshold->setValue(static_cast<int>(FDefaultSettings.ResumeThreshold / 1024));
  FResumeOn->setChecked(FDefaultSettings.Resume == CoreBridge::TTransferSettings::TResume::On);
  FResumeSmart->setChecked(FDefaultSettings.Resume == CoreBridge::TTransferSettings::TResume::Smart);
  FResumeOff->setChecked(FDefaultSettings.Resume == CoreBridge::TTransferSettings::TResume::Off);
  ResumeLayout->addWidget(FResumeOn, 0, 0);
  ResumeLayout->addWidget(FResumeSmart, 1, 0);
  ResumeLayout->addWidget(FResumeThreshold, 1, 1);
  ResumeLayout->addWidget(FResumeOff, 2, 0);
  Layout->addWidget(Resume);

  QGroupBox * Reopen = new QGroupBox(tr("Automatic reconnect"), Page);
  QGridLayout * ReopenLayout = new QGridLayout(Reopen);
  const auto Seconds = [Reopen](int Value, int Default)
  {
    QSpinBox * Spin = new QSpinBox(Reopen);
    Spin->setRange(1, 24 * 60 * 60);
    Spin->setSuffix(PreferencesDialog::tr(" seconds"));
    Spin->setValue((Value > 0) ? Value : Default);
    return Spin;
  };
  // As TPreferencesDialog::LoadConfiguration: a delay that is off shows
  // upstream's default.
  FReopenAuto = new QCheckBox(tr("&Automatically reconnect session, if it breaks during transfer"), Reopen);
  FReopenAuto->setChecked(Preferences.SessionReopenAuto > 0);
  FReopenAutoSeconds = Seconds(Preferences.SessionReopenAuto, 5);
  FReopenAutoIdle = new QCheckBox(tr("Automatically reconnect session, if it breaks &while idle"), Reopen);
  FReopenAutoIdle->setChecked(Preferences.SessionReopenAutoIdleOn);
  FReopenAutoIdleSeconds = Seconds(Preferences.SessionReopenAutoIdle, 9);
  FReopenAutoInactive = new QCheckBox(tr("Automatically reconnect session, if it breaks &while inactive"), Reopen);
  FReopenAutoInactive->setChecked(Preferences.SessionReopenAutoInactive);
  FReopenAutoStall = new QCheckBox(tr("Automatically reconnect session, if it &stalls"), Reopen);
  FReopenAutoStall->setChecked(Preferences.SessionReopenAutoStall > 0);
  FReopenAutoStallSeconds = Seconds(Preferences.SessionReopenAutoStall, 60);
  FReopenTimeout = new QSpinBox(Reopen);
  FReopenTimeout->setRange(0, 24 * 60 * 60);
  FReopenTimeout->setSuffix(tr(" seconds"));
  FReopenTimeout->setSpecialValueText(tr("Forever"));
  FReopenTimeout->setValue(Preferences.SessionReopenTimeout);
  ReopenLayout->addWidget(FReopenAuto, 0, 0);
  ReopenLayout->addWidget(FReopenAutoSeconds, 0, 1);
  ReopenLayout->addWidget(FReopenAutoIdle, 1, 0);
  ReopenLayout->addWidget(FReopenAutoIdleSeconds, 1, 1);
  ReopenLayout->addWidget(FReopenAutoInactive, 2, 0);
  ReopenLayout->addWidget(FReopenAutoStall, 3, 0);
  ReopenLayout->addWidget(FReopenAutoStallSeconds, 3, 1);
  QLabel * ReopenTimeoutLabel = new QLabel(tr("&Keep reconnecting for:"), Reopen);
  ReopenTimeoutLabel->setBuddy(FReopenTimeout);
  ReopenLayout->addWidget(ReopenTimeoutLabel, 4, 0);
  ReopenLayout->addWidget(FReopenTimeout, 4, 1);
  Layout->addWidget(Reopen);
  Layout->addStretch(1);
  for (QAbstractButton * Button : std::initializer_list<QAbstractButton *>{ FResumeOn, FResumeSmart, FResumeOff, FReopenAuto, FReopenAutoIdle, FReopenAutoStall })
  {
    connect(Button, &QAbstractButton::toggled, this, &PreferencesDialog::UpdateControls);
  }
  return Page;
}

TPreferences PreferencesDialog::Preferences() const
{
  TPreferences Result = CoreBridge::Preferences();
  Result.ShowHiddenFiles = FShowHiddenFiles->isChecked();
  Result.DefaultDirIsHome = FDefaultDirIsHome->isChecked();
  Result.RenameWholeName = FRenameWholeName->isChecked();
  Result.PreservePanelState = FPreservePanelState->isChecked();
  Result.FullRowSelect = FFullRowSelect->isChecked();
  Result.PanelSearch = static_cast<TPreferences::TPanelSearch>(FPanelSearch->currentIndex());
  Result.DoubleClickAction = static_cast<TPreferences::TDoubleClick>(FDoubleClickAction->currentIndex());
  Result.CopyOnDoubleClickConfirmation = FCopyOnDoubleClickConfirmation->isChecked();
  Result.PanelFontName = FPanelFontCheck->isChecked() ? FPanelFont.family().toStdString() : std::string();
  Result.PanelFontSize = FPanelFontCheck->isChecked() ? FPanelFont.pointSize() : 0;
  Result.PanelFontBold = FPanelFontCheck->isChecked() && FPanelFont.bold();
  Result.PanelFontItalic = FPanelFontCheck->isChecked() && FPanelFont.italic();
  // The combo's order (NortonLikeModeCombo): Never, Mouse only, Mouse and Keyboard.
  Result.ExplorerSelection = static_cast<TPreferences::TExplorerSelection>(FExplorerSelection->currentIndex());
  Result.NaturalOrderNumericalSorting = FNaturalSorting->isChecked();
  Result.AlwaysSortDirectoriesByName = FDirectoriesByName->isChecked();
  Result.SizeFormat = static_cast<TPreferences::TSizeFormat>(FSizeFormat->currentIndex());
  Result.SwappedPanels = FSwappedPanels->isChecked();
  Result.ExplorerKeyboardShortcuts = (FKeyboardShortcuts->currentIndex() == 1);
  Result.TreeOnLeft = FTreeOnLeft->isChecked();
  Result.CompareByTime = FCompareByTime->isChecked();
  Result.CompareBySize = FCompareBySize->isChecked();
  Result.UseLocationProfiles = FUseLocationProfiles->isChecked();
  Result.QueueTransfersLimit = FQueueTransfersLimit->value();
  Result.QueueKeepDoneItemsFor = FQueueKeepDone->currentData().toInt();
  Result.QueueAutoPopup = FQueueAutoPopup->isChecked();
  Result.EnableQueueByDefault = FEnableQueueByDefault->isChecked();
  Result.CopyParamAutoSelectNotice = FCopyParamAutoSelectNotice->isChecked();
  // In KB, as Configuration keeps it; -1 when off.
  Result.ParallelTransferThreshold = FParallelThresholdCheck->isChecked() ? (FParallelThreshold->value() * 1024) : -1;
  Result.SessionReopenAuto = FReopenAuto->isChecked() ? FReopenAutoSeconds->value() : 0;
  Result.SessionReopenAutoIdleOn = FReopenAutoIdle->isChecked();
  Result.SessionReopenAutoIdle = FReopenAutoIdleSeconds->value();
  Result.SessionReopenAutoInactive = FReopenAutoInactive->isChecked();
  Result.SessionReopenAutoStall = FReopenAutoStall->isChecked() ? FReopenAutoStallSeconds->value() : 0;
  Result.SessionReopenTimeout = FReopenTimeout->value();
  Result.IconSet = FIconSet->currentData().toString().toStdString();
  Result.PathInCaption = FPathFull->isChecked() ? TPreferences::TPathInCaption::Full :
    (FPathNone->isChecked() ? TPreferences::TPathInCaption::None : TPreferences::TPathInCaption::Short);
  Result.KeepOpenWhenNoSession = FKeepOpen->isChecked();
  Result.ShowLoginWhenNoSession = FShowLogin->isChecked();
  Result.SessionTabCaptionTruncation = FTabTruncation->isChecked();
  Result.SessionSilentDisconnect = FSilentDisconnect->isChecked();
  Result.WorkspaceConnectAll = FWorkspaceConnectAll->isChecked();
  Result.AutoSaveWorkspace = FAutoSaveWorkspace->isChecked();
  Result.AutoSaveWorkspacePasswords = FAutoSaveWorkspacePasswords->isChecked();
  Result.AutoWorkspace = FAutoWorkspace->currentText().trimmed().toStdString();
  Result.MinimizeToTray = FMinimizeToTray->isChecked();
  Result.ExternalSessionInExistingInstance = FExternalSessionInExistingInstance->isChecked();
  Result.FileColors = FFileColors;
  // As TPreferencesDialog::SaveConfiguration.
  Result.Logging = FLogging->isChecked();
  Result.LogLevel = FLogLevel->currentIndex();
  Result.LogFileName = FLogFile->text().trimmed().toStdString();
  Result.LogFileAppend = FLogAppend->isChecked();
  Result.LogSensitive = FLogSensitive->isChecked();
  Result.LogMaxSize = FLogMaxSize->isChecked() ? CoreBridge::SizeOfText(FLogMaxSizeCombo->currentText().toStdString()) : 0;
  Result.LogMaxCount = FLogMaxCount->isChecked() ? FLogMaxCountSpin->value() : 0;
  Result.LogActions = FLogActions->isChecked();
  Result.ActionsLogFileName = FActionsLogFile->text().trimmed().toStdString();
  Result.ExternalIpAddress = FCustomIp->isChecked() ? FCustomIpEdit->text().trimmed().toStdString() : std::string();
  Result.LocalPortNumberMin = FPortLimit->isChecked() ? FPortMin->value() : 0;
  Result.LocalPortNumberMax = FPortLimit->isChecked() ? FPortMax->value() : 0;
  Result.TryFtpWhenSshFails = FTryFtp->isChecked();
  Result.SessionRememberPassword = FRememberPassword->isChecked();
  Result.AuthAgent = FAuthAgent->currentIndex();
  Result.Storage = FStorage;
  if ((FRegistryStorage != nullptr) && FRegistryStorage->isChecked())
  {
    Result.Storage = TPreferences::TStorage::Registry;
  }
  else if ((FAutomaticIniStorage != nullptr) && FAutomaticIniStorage->isChecked())
  {
    Result.Storage = TPreferences::TStorage::AutomaticIni;
  }
  else if ((FCustomIniStorage != nullptr) && FCustomIniStorage->isChecked())
  {
    Result.Storage = TPreferences::TStorage::CustomIni;
    Result.CustomIniFile = FCustomIniEdit->text().trimmed().toStdString();
  }
  Result.TemporaryDirectory = FCustomTemporary->isChecked() ? FTemporaryEdit->text().trimmed().toStdString() : std::string();
  Result.RandomSeedFile = FRandomSeedEdit->text().trimmed().toStdString();
  Result.TemporaryDirectoryCleanup = FTemporaryCleanup->isChecked();
  Result.ConfirmTemporaryDirectoryCleanup = FConfirmTemporaryCleanup->isChecked();
  Result.TemporaryDirectoryAppendSession = FTemporaryAppendSession->isChecked();
  Result.TemporaryDirectoryAppendPath = FTemporaryAppendPath->isChecked();
  Result.TemporaryDirectoryDeterministic = FTemporaryDeterministic->isChecked();
  Result.PuttyPath = FPuttyPath->text().toStdString();
  Result.PuttyPassword = FPuttyPassword->isChecked();
  Result.AutoOpenInPutty = FAutoOpenInPutty->isChecked();
  Result.TelnetForFtpInPutty = FTelnetForFtpInPutty->isChecked();
  if ((FPuttyRegistryStorageKey != nullptr) && (FPuttyRegistryStorageKey->currentIndex() >= 0))
  {
    Result.PuttyRegistryStorageKey = FPuttyRegistryStorageKey->currentText().toStdString();
  }
  // As TPreferencesDialog::SaveConfiguration: the environment, the
  // interface, drag & drop, the panels' pages, the updates.
  Result.ConfirmTransferring = FConfirmTransferring->isChecked();
  Result.DDTransferConfirmation = (FConfirmDDTransfer->checkState() == Qt::PartiallyChecked) ? TPreferences::TAutoSwitch::Auto :
    (FConfirmDDTransfer->isChecked() ? TPreferences::TAutoSwitch::On : TPreferences::TAutoSwitch::Off);
  Result.ConfirmOverwriting = FConfirmOverwriting->isChecked();
  Result.ConfirmResume = FConfirmResume->isChecked();
  Result.ConfirmDeleting = FConfirmDeleting->isChecked();
  Result.ConfirmRecycling = FConfirmRecycling->isChecked();
  Result.ConfirmClosingSession = FConfirmClosingSession->isChecked();
  Result.ConfirmExitOnCompletion = FConfirmExitOnCompletion->isChecked();
  Result.ConfirmCommandSession = FConfirmCommandSession->isChecked();
  Result.SynchronizeSummary = FSynchronizeSummary->isChecked();
  Result.ContinueOnError = FContinueOnError->isChecked();
  Result.BeepOnFinish = FBeepOnFinish->isChecked();
  Result.BeepOnFinishAfter = FBeepOnFinishAfter->value();
  Result.BalloonNotifications = FBalloonNotifications->isChecked();
  Result.FlashTaskbar = FFlashTaskbar->isChecked();
  Result.DarkTheme = AutoSwitchOfIndex(FTheme->currentIndex());
  Result.ExplorerInterface = FExplorerInterface->isChecked();
  Result.DDWarnLackOfTempSpace = FDDWarnLackOfTempSpace->isChecked();
  if (FDDFakeFile->isEnabled())
  {
    Result.DDFakeFile = FDDFakeFile->isChecked();
    Result.DDDrives.clear();
    for (const QString & Line : FDDDrives->toPlainText().split(QLatin1Char('\n')))
    {
      if (!Line.trimmed().isEmpty())
      {
        Result.DDDrives.push_back(Line.trimmed().toStdString());
      }
    }
  }
  Result.ShowInaccesibleDirectories = FShowInaccesibleDirectories->isChecked();
  Result.AutoReadDirectoryAfterOp = FAutoReadDirectoryAfterOp->isChecked();
  Result.RefreshRemotePanel = FRefreshRemotePanel->isChecked();
  Result.RefreshRemotePanelInterval = FRefreshRemotePanelInterval->value();
  Result.PreserveLocalDirectory = FPreserveLocalDirectory->isChecked();
  Result.DeleteToRecycleBin = FDeleteToRecycleBin->isChecked();
  Result.SystemContextMenu = FSystemContextMenu->isChecked();
  // As SaveUpdates.
  static const int Periods[] = { 0, 1, 7, 30 };
  Result.UpdatesPeriod = Periods[std::clamp(FUpdatesPeriod->currentIndex(), 0, 3)];
  Result.UpdatesShowOnStartup = FUpdatesShowOnStartup->isChecked();
  Result.UpdatesBetaVersions = AutoSwitchOfIndex(FUpdatesBetaVersions->currentIndex());
  Result.UpdatesProxyHost = FUpdatesProxyHost->text().trimmed().toStdString();
  Result.UpdatesProxyPort = FUpdatesProxyPort->value();
  if (FUpdatesAuto->isChecked())
  {
    Result.UpdatesConnectionType = 1;
  }
  else if (FUpdatesProxy->isChecked() && !Result.UpdatesProxyHost.empty())
  {
    Result.UpdatesConnectionType = 2;
  }
  else
  {
    Result.UpdatesConnectionType = 0;
  }
  return Result;
}

unsigned PreferencesDialog::Locale() const
{
  return (FLanguages->currentItem() != nullptr) ? FLanguages->currentItem()->data(LocaleRole).toUInt() :
    CoreBridge::PreferredLocale();
}

void PreferencesDialog::Save()
{
  CoreBridge::SetPreferences(Preferences());
  // Background's and Endurance's are the default transfer settings', as
  // TPreferencesDialog::SaveConfiguration has them.
  CoreBridge::TTransferSettings Default = FDefaultSettings;
  Default.Queue = FQueue->isChecked();
  Default.QueueNoConfirmation = FQueueNoConfirmation->isChecked();
  Default.QueueParallel = FQueueParallel->isChecked();
  Default.Resume = FResumeOn->isChecked() ? CoreBridge::TTransferSettings::TResume::On :
    (FResumeOff->isChecked() ? CoreBridge::TTransferSettings::TResume::Off : CoreBridge::TTransferSettings::TResume::Smart);
  Default.ResumeThreshold = static_cast<std::int64_t>(FResumeThreshold->value()) * 1024;
  CoreBridge::SetDefaultTransferSettings(Default);
  CoreBridge::SetTransferPresets(FPresets);
  CoreBridge::SetSshHostCAs(FHostCAList, FHostCAsFromPuTTY->isChecked());
  CoreBridge::SetCustomCommands(FCommands);
  FExtensions->Commit();
  CoreBridge::SetEditorSettings(EditorSettings());
  CoreBridge::SetEditors(FEditors);
  // Explorer's: the layout's (TScpExplorerConfiguration).
  if (CoreBridge::Layout().Explorer.ShowFullAddress != FShowFullAddress->isChecked())
  {
    CoreBridge::TLayout Layout = CoreBridge::Layout();
    Layout.Explorer.ShowFullAddress = FShowFullAddress->isChecked();
    CoreBridge::SetLayout(Layout);
  }
  if (FLanguages->currentItem() != nullptr)
  {
    CoreBridge::SetPreferredLocale(Locale());
  }
}

// Integration > Applications, as WinSCP's IntegrationAppSheet.
QWidget * PreferencesDialog::MakeApplicationsPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("External applications"), Page);
  QGridLayout * GroupLayout = new QGridLayout(Group);
  FPuttyPath = new QLineEdit(QString::fromStdString(Preferences.PuttyPath), Group);
  QLabel * PathLabel = new QLabel(tr("PuTTY/Terminal &client path:"), Group);
  PathLabel->setBuddy(FPuttyPath);
  QPushButton * Browse = new QPushButton(tr("B&rowse..."), Group);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    const QString Program = QFileDialog::getOpenFileName(this, tr("PuTTY/Terminal client"));
    if (!Program.isEmpty())
    {
      // As FormatCommand: the program quoted, before its parameters.
      FPuttyPath->setText(QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(Program)));
    }
  });
  // WinSCP's hint label, as a button so the keyboard reaches it: the
  // patterns shown when pressed, as on hover.
  QPushButton * Patterns = MakeHintButton(tr("&patterns"), QString::fromStdString(CoreBridge::PuttyPatternsHint()), Group);
  GroupLayout->addWidget(PathLabel, 0, 0, 1, 2);
  GroupLayout->addWidget(FPuttyPath, 1, 0);
  GroupLayout->addWidget(Browse, 1, 1);
  GroupLayout->addWidget(Patterns, 2, 0, 1, 2, Qt::AlignRight);
  FPuttyPassword = new QCheckBox(tr("&Remember session password and pass it to PuTTY (SSH)"), Group);
  FPuttyPassword->setChecked(Preferences.PuttyPassword);
  FTelnetForFtpInPutty = new QCheckBox(tr("Open &Telnet sessions in PuTTY for FTP sessions"), Group);
  FTelnetForFtpInPutty->setChecked(Preferences.TelnetForFtpInPutty);
  FAutoOpenInPutty = new QCheckBox(tr("Automatically &open new sessions in PuTTY"), Group);
  FAutoOpenInPutty->setChecked(Preferences.AutoOpenInPutty);
  GroupLayout->addWidget(FPuttyPassword, 3, 0, 1, 2);
  GroupLayout->addWidget(FTelnetForFtpInPutty, 4, 0, 1, 2);
  GroupLayout->addWidget(FAutoOpenInPutty, 5, 0, 1, 2);
#if defined(Q_OS_WIN)
  // PuTTY's sessions are in the registry on Windows only.
  FPuttyRegistryStorageKey = new QComboBox(Group);
  for (const std::string & Key : CoreBridge::PuttyRegistryStorageKeys())
  {
    FPuttyRegistryStorageKey->addItem(QString::fromStdString(Key));
  }
  FPuttyRegistryStorageKey->setCurrentIndex(
    FPuttyRegistryStorageKey->findText(QString::fromStdString(Preferences.PuttyRegistryStorageKey)));
  FPuttyRegistryStorageKeyLabel = new QLabel(tr("PuTTY registry &key:"), Group);
  FPuttyRegistryStorageKeyLabel->setBuddy(FPuttyRegistryStorageKey);
  GroupLayout->addWidget(FPuttyRegistryStorageKeyLabel, 6, 0, 1, 2);
  GroupLayout->addWidget(FPuttyRegistryStorageKey, 7, 0, 1, 2);
#endif
  connect(FPuttyPath, &QLineEdit::textChanged, this, &PreferencesDialog::UpdateControls);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

// Commands, as WinSCP's CustomCommandsSheet: the custom commands, their
// order the menus'. (Extensions are not ported yet.)
QWidget * PreferencesDialog::MakeCommandsPage()
{
  FCommands = CoreBridge::CustomCommands();
  FExtensions = std::make_unique<CoreBridge::TExtensionsEdit>();
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Custom commands"), Page);
  QHBoxLayout * GroupLayout = new QHBoxLayout(Group);
  FCommandsList = new QTreeWidget(Group);
  FCommandsList->setProperty("literalText", true);
  FCommandsList->setAccessibleName(Group->title());
  FCommandsList->setRootIsDecorated(false);
  FCommandsList->setHeaderLabels({ tr("Description"), tr("Command"), tr("L/R") });
  FCommandsList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  FCommandsList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  FCommandsList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  FCommandsList->header()->setStretchLastSection(false);
  GroupLayout->addWidget(FCommandsList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  // As WinSCP's split Add button: what the selection is a list of, or
  // either from its menu.
  FAddCommand = new QToolButton(Group);
  FAddCommand->setText(tr("&Add..."));
  FAddCommand->setPopupMode(QToolButton::MenuButtonPopup);
  FAddCommand->setToolButtonStyle(Qt::ToolButtonTextOnly);
  FAddCommand->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  QMenu * AddMenu = new QMenu(FAddCommand);
  AddMenu->addAction(tr("Add &Custom Command..."), this, [this]() { AddEditCommand(false); });
  AddMenu->addAction(tr("Add &Extension..."), this, [this]() { AddExtension(); });
  FAddCommand->setMenu(AddMenu);
  FEditCommand = new QPushButton(tr("&Edit..."), Group);
  FConfigureCommand = new QPushButton(tr("&Configure..."), Group);
  FRemoveCommand = new QPushButton(tr("&Remove"), Group);
  FUpCommand = new QPushButton(tr("&Up"), Group);
  FDownCommand = new QPushButton(tr("&Down"), Group);
  Buttons->addWidget(FAddCommand);
  for (QPushButton * Button : { FEditCommand, FConfigureCommand, FRemoveCommand, FUpCommand, FDownCommand })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  GroupLayout->addLayout(Buttons);
  connect(FAddCommand, &QToolButton::clicked, this, [this]()
  {
    if (ExtensionIndex(FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem())) >= 0)
    {
      AddExtension();
    }
    else
    {
      AddEditCommand(false);
    }
  });
  connect(FEditCommand, &QPushButton::clicked, this, [this]() { AddEditCommand(true); });
  connect(FConfigureCommand, &QPushButton::clicked, this, [this]() { ConfigureCommand(); });
  connect(FRemoveCommand, &QPushButton::clicked, this, &PreferencesDialog::RemoveCommand);
  connect(FUpCommand, &QPushButton::clicked, this, [this]() { MoveCommand(-1); });
  connect(FDownCommand, &QPushButton::clicked, this, [this]() { MoveCommand(1); });
  connect(FCommandsList, &QTreeWidget::currentItemChanged, this, &PreferencesDialog::UpdateControls);
  connect(FCommandsList, &QTreeWidget::itemDoubleClicked, this, [this]()
  {
    if (!FEditCommand->isHidden())
    {
      AddEditCommand(true);
    }
    else
    {
      ConfigureCommand();
    }
  });
  // As CustomCommandsViewKeyDown: Insert adds, Delete removes.
  QShortcut * Insert = new QShortcut(QKeySequence(Qt::Key_Insert), FCommandsList, nullptr, nullptr, Qt::WidgetShortcut);
  connect(Insert, &QShortcut::activated, this, [this]() { AddEditCommand(false); });
  QShortcut * Delete = new QShortcut(QKeySequence(Qt::Key_Delete), FCommandsList, nullptr, nullptr, Qt::WidgetShortcut);
  connect(Delete, &QShortcut::activated, this, [this]()
  {
    if (FRemoveCommand->isEnabled())
    {
      RemoveCommand();
    }
  });
  Layout->addWidget(Group);
  LoadCommands(-1);
  return Page;
}

int PreferencesDialog::ExtensionIndex(int Row) const
{
  const int CustomCount = static_cast<int>(FCommands.size());
  return (Row >= CustomCount) ? (Row - CustomCount) : -1;
}

// As CustomCommandsViewData: the custom commands, then the extensions (an
// extension's description its hint).
void PreferencesDialog::LoadCommands(int Select)
{
  FCommandsList->clear();
  std::vector<CoreBridge::TCustomCommandDefinition> All = FCommands;
  for (const CoreBridge::TCustomCommandDefinition & Extension : FExtensions->Items())
  {
    All.push_back(Extension);
  }
  for (const CoreBridge::TCustomCommandDefinition & Command : All)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FCommandsList);
    Item->setText(0, QString::fromStdString(Command.Name).remove(QRegularExpression(QStringLiteral("&(?!&)")))
      .replace(QStringLiteral("&&"), QStringLiteral("&")));
    Item->setText(1, QString::fromStdString(Command.Command));
    Item->setText(2, QString::fromStdString(CoreBridge::CustomCommandSide(Command.Local)));
    if (!Command.Description.empty())
    {
      Item->setToolTip(0, QString::fromStdString(Command.Description));
    }
  }
  if ((Select >= 0) && (Select < FCommandsList->topLevelItemCount()))
  {
    FCommandsList->setCurrentItem(FCommandsList->topLevelItem(Select));
  }
  UpdateControls();
}

// As TPreferencesDialog::AddEditCommand: a new command goes before the
// selected one, or last; the shortcuts the shared bookmarks use are not
// offered.
bool PreferencesDialog::AddEditCommand(bool Edit, const CoreBridge::TCustomCommandDefinition * Command)
{
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  if (Edit && (Row < 0))
  {
    return false;
  }
  CoreBridge::TCustomCommandDefinition Result = Edit ? FCommands[Row] : CoreBridge::TCustomCommandDefinition();
  if (Command != nullptr)
  {
    Result = *Command;
  }
  else
  {
    std::vector<unsigned> Bookmarks;
    for (const CoreBridge::TBookmarkEntry & Bookmark : CoreBridge::Bookmarks(std::string()))
    {
      if (Bookmark.ShortCut != 0)
      {
        Bookmarks.push_back(Bookmark.ShortCut);
      }
    }
    CustomCommandDialog Dialog(Edit ? CustomCommandDialog::TMode::Edit : CustomCommandDialog::TMode::Add, Result, FCommands,
      this, Bookmarks);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Result = Dialog.Command();
  }
  int Index = Row;
  if (Edit)
  {
    FCommands[Row] = Result;
  }
  else if (Row >= 0)
  {
    FCommands.insert(FCommands.begin() + Row, Result);
  }
  else
  {
    FCommands.push_back(Result);
    Index = static_cast<int>(FCommands.size()) - 1;
  }
  LoadCommands(Index);
  return true;
}

void PreferencesDialog::RemoveCommand()
{
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  if (Row < 0)
  {
    return;
  }
  const int Extension = ExtensionIndex(Row);
  if (Extension >= 0)
  {
    FExtensions->Remove(Extension);
  }
  else
  {
    FCommands.erase(FCommands.begin() + Row);
  }
  LoadCommands(qMin(Row, FCommandsList->topLevelItemCount() - 2));
}

void PreferencesDialog::MoveCommand(int By)
{
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  const int Extension = ExtensionIndex(Row);
  if (Extension >= 0)
  {
    const int To = Extension + By;
    if ((To < 0) || (To >= static_cast<int>(FExtensions->Items().size())))
    {
      return;
    }
    FExtensions->Move(Extension, To);
    LoadCommands(Row + By);
    return;
  }
  const int To = Row + By;
  if ((Row < 0) || (To < 0) || (To >= static_cast<int>(FCommands.size())))
  {
    return;
  }
  std::swap(FCommands[Row], FCommands[To]);
  LoadCommands(To);
}

// As TPreferencesDialog::AddExtension: a path, or a URL downloaded (asking
// winscp.net for one that suits), added before the selected extension, or
// last; one not from a trusted source only when confirmed.
bool PreferencesDialog::AddExtension(const QString * APath)
{
  QString Path;
  if (APath != nullptr)
  {
    Path = *APath;
  }
  else
  {
    QDialog Dialog(this);
    Dialog.setWindowTitle(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddExtensionCaption)));
    QVBoxLayout * Layout = new QVBoxLayout(&Dialog);
    QLabel * Prompt = new QLabel(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddExtensionPrompt)), &Dialog);
    QComboBox * Edit = new QComboBox(&Dialog);
    Edit->setEditable(true);
    Edit->setInsertPolicy(QComboBox::NoInsert);
    Edit->setMinimumWidth(444);
    for (const std::string & Item : CoreBridge::History("ExtensionPath"))
    {
      Edit->addItem(QString::fromStdString(Item));
    }
    Edit->setEditText(QString());
    Prompt->setBuddy(Edit);
    QPushButton * Browse = new QPushButton(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionOptionsBrowse)), &Dialog);
    connect(Browse, &QPushButton::clicked, &Dialog, [&Dialog, Edit]()
    {
      const QString File = QFileDialog::getOpenFileName(&Dialog, QString(), QString(),
        QStringLiteral("WinSCP extensions (*.WinSCPextension*);;All files (*)"));
      if (!File.isEmpty())
      {
        Edit->setEditText(QDir::toNativeSeparators(File));
      }
    });
    QHBoxLayout * Row = new QHBoxLayout();
    Row->addWidget(Edit, 1);
    Row->addWidget(Browse);
    Layout->addWidget(Prompt);
    Layout->addLayout(Row);
    QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
    connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    Layout->addWidget(Box);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Path = Edit->currentText().trimmed();
    if (Path.isEmpty())
    {
      return false;
    }
    CoreBridge::AddToHistory("ExtensionPath", Path.toStdString());
  }
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  const int Before = ExtensionIndex(Row);
  auto ConfirmUntrusted = [this]()
  {
    return QMessageBox::warning(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddExtensionCaption)),
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionUntrusted)),
      QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok;
  };
  try
  {
    int Index = -1;
    const bool Url = Path.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
      Path.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    bool Latest = true;
    if (Url)
    {
      // As THttp's Get, with AddExtension's Accept header; winscp.net's
      // headers name the extension and say whether it is trusted.
      bool WinScpUrl = false;
      const QUrl Address(QString::fromStdString(CoreBridge::ExtensionDownloadUrl(Path.toStdString(), WinScpUrl)));
      QNetworkAccessManager Manager;
      QNetworkRequest Request(Address);
      Request.setRawHeader("Accept", "text/winscpextension,text/plain");
      Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
      QNetworkReply * Reply = Manager.get(Request);
      QEventLoop Loop;
      connect(Reply, &QNetworkReply::finished, &Loop, &QEventLoop::quit);
      QApplication::setOverrideCursor(Qt::BusyCursor);
      Loop.exec();
      QApplication::restoreOverrideCursor();
      Reply->deleteLater();
      if (Reply->error() != QNetworkReply::NoError)
      {
        QMessageBox::warning(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddExtensionCaption)),
          Reply->errorString());
        return false;
      }
      const bool Trusted = WinScpUrl && (QString::fromLatin1(Reply->rawHeader("WinSCP-Extension-Trusted")).toInt() != 0);
      Latest = Reply->rawHeader("WinSCP-Extension-Skipped").trimmed().isEmpty();
      Index = FExtensions->AddFromUrl(Path.toStdString(), QString::fromUtf8(Reply->readAll()).toStdString(),
        QString::fromLatin1(Reply->rawHeader("WinSCP-Extension-Id")).toStdString(), Trusted, Before, ConfirmUntrusted);
    }
    else
    {
      Index = FExtensions->AddFromFile(QDir::fromNativeSeparators(Path).toStdString(), Before, ConfirmUntrusted);
    }
    if (Index < 0)
    {
      return false;
    }
    LoadCommands(static_cast<int>(FCommands.size()) + Index);
    if (!Latest)
    {
      QMessageBox::information(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AddExtensionCaption)),
        QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ExtensionNotLatest)));
    }
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
    return false;
  }
}

// As TPreferencesDialog::ConfigureCommand: the extension's Preferences
// options (the site's for those kept per site) and its keyboard shortcut.
bool PreferencesDialog::ConfigureCommand(const std::vector<std::string> * Values, const unsigned * AShortCut)
{
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  const int Extension = ExtensionIndex(Row);
  if (Extension < 0)
  {
    return false;
  }
  const CoreBridge::TCustomCommandDefinition Command = FExtensions->Items()[Extension];
  // TCustomCommandType's ofConfig.
  const unsigned Flags = 2;
  if (Command.SiteOptions && FSessionKey.empty())
  {
    QMessageBox::warning(this, windowTitle(), QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NoSiteForCommand)));
    return false;
  }
  std::vector<std::string> NewValues;
  unsigned ShortCut = Command.ShortCut;
  if (Values != nullptr)
  {
    NewValues = *Values;
    ShortCut = (AShortCut != nullptr) ? *AShortCut : ShortCut;
  }
  else
  {
    std::vector<unsigned> Used;
    for (const CoreBridge::TCustomCommandDefinition & Other : FCommands)
    {
      if (Other.ShortCut != 0)
      {
        Used.push_back(Other.ShortCut);
      }
    }
    for (const CoreBridge::TCustomCommandDefinition & Other : FExtensions->Items())
    {
      if ((Other.ShortCut != 0) && (Other.Id != Command.Id))
      {
        Used.push_back(Other.ShortCut);
      }
    }
    ExtensionOptionsDialog Dialog(QString::fromStdString(Command.Name), FExtensions->Options(Extension, Flags, FSessionKey),
      this, &ShortCut, Used, QString::fromStdString(!Command.OptionsPage.empty() ? Command.OptionsPage : Command.HomePage));
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    NewValues = Dialog.Values();
    ShortCut = Dialog.ShortCut();
  }
  FExtensions->SetOptions(Extension, Flags, FSessionKey, NewValues);
  FExtensions->SetShortCut(Extension, ShortCut);
  LoadCommands(Row);
  return true;
}

// Editors, as WinSCP's EditorSheet: the editors files open in, the first
// whose mask matches; the editing options.
QWidget * PreferencesDialog::MakeEditorsPage()
{
  FEditor = CoreBridge::EditorSettings();
  FEditors = CoreBridge::Editors();
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Preference = new QGroupBox(tr("Editor preference"), Page);
  QHBoxLayout * PreferenceLayout = new QHBoxLayout(Preference);
  FEditorsList = new QTreeWidget(Preference);
  FEditorsList->setAccessibleName(Preference->title());
  FEditorsList->setRootIsDecorated(false);
  FEditorsList->setHeaderLabels({ tr("Editor"), tr("Mask"), tr("Text") });
  FEditorsList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  FEditorsList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  FEditorsList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  FEditorsList->header()->setStretchLastSection(false);
  PreferenceLayout->addWidget(FEditorsList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  QPushButton * Add = new QPushButton(tr("&Add..."), Preference);
  FEditEditor = new QPushButton(tr("&Edit..."), Preference);
  FUpEditor = new QPushButton(tr("&Up"), Preference);
  FDownEditor = new QPushButton(tr("&Down"), Preference);
  FRemoveEditor = new QPushButton(tr("&Remove"), Preference);
  for (QPushButton * Button : { Add, FEditEditor, FUpEditor, FDownEditor, FRemoveEditor })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  PreferenceLayout->addLayout(Buttons);
  connect(Add, &QPushButton::clicked, this, [this]() { AddEditEditor(false); });
  connect(FEditEditor, &QPushButton::clicked, this, [this]() { AddEditEditor(true); });
  connect(FRemoveEditor, &QPushButton::clicked, this, &PreferencesDialog::RemoveEditor);
  connect(FUpEditor, &QPushButton::clicked, this, [this]() { MoveEditor(-1); });
  connect(FDownEditor, &QPushButton::clicked, this, [this]() { MoveEditor(1); });
  connect(FEditorsList, &QTreeWidget::currentItemChanged, this, &PreferencesDialog::UpdateControls);
  connect(FEditorsList, &QTreeWidget::itemDoubleClicked, this, [this]() { AddEditEditor(true); });
  Layout->addWidget(Preference, 1);
  QGroupBox * Options = new QGroupBox(tr("Editing options"), Page);
  QVBoxLayout * OptionsLayout = new QVBoxLayout(Options);
  FEditorCheckNotModified = new QCheckBox(tr("&Check that edited remote file has not changed before saving it"), Options);
  FEditorCheckNotModified->setChecked(FEditor.CheckNotModified);
  OptionsLayout->addWidget(FEditorCheckNotModified);
  Layout->addWidget(Options);
  LoadEditors(-1);
  return Page;
}

// As EditorListView3Data: the name, the mask, and for an external editor
// whether it is given text.
void PreferencesDialog::LoadEditors(int Select)
{
  FEditorsList->clear();
  for (const CoreBridge::TEditorDefinition & Editor : FEditors)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FEditorsList);
    Item->setText(0, QString::fromStdString(CoreBridge::EditorName(Editor)));
    Item->setText(1, QString::fromStdString(Editor.Mask));
    if (Editor.Kind == CoreBridge::TEditorDefinition::TKind::External)
    {
      Item->setText(2, Editor.Text ? tr("Yes") : tr("No"));
    }
  }
  if ((Select >= 0) && (Select < FEditorsList->topLevelItemCount()))
  {
    FEditorsList->setCurrentItem(FEditorsList->topLevelItem(Select));
  }
  UpdateControls();
}

// As AddEditEditorButtonClick: a new one before the selected, or last.
bool PreferencesDialog::AddEditEditor(bool Edit, const CoreBridge::TEditorDefinition * Editor)
{
  const int Row = FEditorsList->indexOfTopLevelItem(FEditorsList->currentItem());
  if (Edit && (Row < 0))
  {
    return false;
  }
  CoreBridge::TEditorDefinition Result = Edit ? FEditors[Row] : CoreBridge::TEditorDefinition();
  if (Editor != nullptr)
  {
    Result = *Editor;
  }
  else
  {
    EditorPreferencesDialog Dialog(Edit ? EditorPreferencesDialog::TMode::Edit : EditorPreferencesDialog::TMode::Add,
      Result, true, this);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Result = Dialog.Editor();
  }
  int Index = Row;
  if (Edit)
  {
    FEditors[Row] = Result;
  }
  else if (Row >= 0)
  {
    FEditors.insert(FEditors.begin() + Row, Result);
  }
  else
  {
    FEditors.push_back(Result);
    Index = static_cast<int>(FEditors.size()) - 1;
  }
  LoadEditors(Index);
  return true;
}

void PreferencesDialog::RemoveEditor()
{
  const int Row = FEditorsList->indexOfTopLevelItem(FEditorsList->currentItem());
  if (Row < 0)
  {
    return;
  }
  FEditors.erase(FEditors.begin() + Row);
  LoadEditors(qMin(Row, static_cast<int>(FEditors.size()) - 1));
}

void PreferencesDialog::MoveEditor(int By)
{
  const int Row = FEditorsList->indexOfTopLevelItem(FEditorsList->currentItem());
  const int To = Row + By;
  if ((Row < 0) || (To < 0) || (To >= static_cast<int>(FEditors.size())))
  {
    return;
  }
  std::swap(FEditors[Row], FEditors[To]);
  LoadEditors(To);
}

// Internal editor, as WinSCP's EditorInternalSheet.
QWidget * PreferencesDialog::MakeInternalEditorPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Display = new QGroupBox(tr("Display"), Page);
  QGridLayout * DisplayLayout = new QGridLayout(Display);
  FEditorWordWrap = new QCheckBox(tr("&Wrap long lines"), Display);
  FEditorWordWrap->setChecked(FEditor.WordWrap);
  DisplayLayout->addWidget(FEditorWordWrap, 0, 0, 1, 2);
  FEditorTabSize = new QSpinBox(Display);
  FEditorTabSize->setRange(1, 99);
  FEditorTabSize->setValue(FEditor.TabSize);
  QLabel * TabLabel = new QLabel(tr("&Tabulator size:"), Display);
  TabLabel->setBuddy(FEditorTabSize);
  DisplayLayout->addWidget(TabLabel, 1, 0);
  DisplayLayout->addWidget(FEditorTabSize, 1, 1);
  FEditorEncoding = new QComboBox(Display);
  FEditorEncoding->addItem(QString::fromStdString(CoreBridge::DefaultEncodingName()), false);
  FEditorEncoding->addItem(tr("UTF-8"), true);
  FEditorEncoding->setCurrentIndex(FEditor.Utf8 ? 1 : 0);
  QLabel * EncodingLabel = new QLabel(tr("Default en&coding:"), Display);
  EncodingLabel->setBuddy(FEditorEncoding);
  DisplayLayout->addWidget(EncodingLabel, 2, 0);
  DisplayLayout->addWidget(FEditorEncoding, 2, 1);
  Layout->addWidget(Display);

  QGroupBox * FontGroup = new QGroupBox(tr("Font"), Page);
  QGridLayout * FontLayout = new QGridLayout(FontGroup);
  FEditorFontLabel = new QLabel(FontGroup);
  FEditorFontLabel->setAutoFillBackground(true);
  FEditorFontLabel->setFrameShape(QFrame::StyledPanel);
  FEditorFontLabel->setMinimumHeight(48);
  FontLayout->addWidget(FEditorFontLabel, 0, 0, 3, 1);
  QPushButton * SelectFont = new QPushButton(tr("&Select font..."), FontGroup);
  connect(SelectFont, &QPushButton::clicked, this, [this]()
  {
    bool OK = false;
    const QFont Font = QFontDialog::getFont(&OK, FEditorFontLabel->font(), this);
    if (OK)
    {
      SetEditorFont(Font);
    }
  });
  // As CreateColorPalette's menus: the default, or a colour of one's own.
  const auto ColorMenu = [this](QPushButton * Button, bool Background)
  {
    QMenu * Menu = new QMenu(Button);
    Menu->addAction(tr("&Default"), this, [this, Background]()
    {
      SetEditorColors(Background ? FEditor.FontColor : -1, Background ? -1 : FEditor.BackgroundColor);
    });
    Menu->addAction(tr("&More colors..."), this, [this, Background]()
    {
      const int Current = Background ? FEditor.BackgroundColor : FEditor.FontColor;
      const QColor Color = QColorDialog::getColor((Current >= 0) ? QColor(QRgb(Current)) : QColor(), this);
      if (Color.isValid())
      {
        const int Rgb = static_cast<int>(Color.rgb() & 0xffffff);
        SetEditorColors(Background ? FEditor.FontColor : Rgb, Background ? Rgb : FEditor.BackgroundColor);
      }
    });
    Button->setMenu(Menu);
  };
  QPushButton * TextColor = new QPushButton(tr("&Text color"), FontGroup);
  ColorMenu(TextColor, false);
  QPushButton * Background = new QPushButton(tr("Default &background"), FontGroup);
  ColorMenu(Background, true);
  FontLayout->addWidget(SelectFont, 0, 1);
  FontLayout->addWidget(TextColor, 1, 1);
  FontLayout->addWidget(Background, 2, 1);
  FontLayout->setColumnStretch(0, 1);
  Layout->addWidget(FontGroup);

  QGroupBox * Behaviour = new QGroupBox(tr("Behaviour"), Page);
  QHBoxLayout * BehaviourLayout = new QHBoxLayout(Behaviour);
  FEditorWarnLargeFile = new QCheckBox(tr("Warn when &editing files larger than:"), Behaviour);
  FEditorWarnLargeFile->setChecked(FEditor.WarnOnLargeFile);
  FEditorLargeFileSize = new QSpinBox(Behaviour);
  FEditorLargeFileSize->setRange(1, 1024 * 1024);
  FEditorLargeFileSize->setValue(FEditor.LargeFileSize);
  FEditorLargeFileSize->setSuffix(tr(" KB"));
  connect(FEditorWarnLargeFile, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  BehaviourLayout->addWidget(FEditorWarnLargeFile);
  BehaviourLayout->addWidget(FEditorLargeFileSize);
  BehaviourLayout->addStretch(1);
  Layout->addWidget(Behaviour);
  Layout->addStretch(1);
  UpdateEditorFont();
  return Page;
}

// As TPreferencesDialog::UpdateEditorFont: the font shown in it, in its
// colours.
void PreferencesDialog::UpdateEditorFont()
{
  QFont Font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  if (!FEditor.FontName.empty())
  {
    Font.setFamily(QString::fromStdString(FEditor.FontName));
  }
  if (FEditor.FontSize > 0)
  {
    Font.setPointSize(FEditor.FontSize);
  }
  Font.setBold(FEditor.FontBold);
  Font.setItalic(FEditor.FontItalic);
  Font.setUnderline(FEditor.FontUnderline);
  Font.setStrikeOut(FEditor.FontStrikeOut);
  FEditorFontLabel->setFont(Font);
  FEditorFontLabel->setText(QStringLiteral("%1, %2 pt").arg(Font.family()).arg(Font.pointSize()));
  QPalette Palette = palette();
  Palette.setColor(QPalette::Window, (FEditor.BackgroundColor >= 0) ? QColor(QRgb(FEditor.BackgroundColor)) : Palette.color(QPalette::Base));
  Palette.setColor(QPalette::WindowText, (FEditor.FontColor >= 0) ? QColor(QRgb(FEditor.FontColor)) : Palette.color(QPalette::Text));
  FEditorFontLabel->setPalette(Palette);
}

void PreferencesDialog::SetEditorFont(const QFont & Font)
{
  FEditor.FontName = Font.family().toStdString();
  FEditor.FontSize = Font.pointSize();
  FEditor.FontBold = Font.bold();
  FEditor.FontItalic = Font.italic();
  FEditor.FontUnderline = Font.underline();
  FEditor.FontStrikeOut = Font.strikeOut();
  UpdateEditorFont();
}

void PreferencesDialog::SetEditorColors(int FontColor, int BackgroundColor)
{
  FEditor.FontColor = FontColor;
  FEditor.BackgroundColor = BackgroundColor;
  UpdateEditorFont();
}

CoreBridge::TEditorSettings PreferencesDialog::EditorSettings() const
{
  CoreBridge::TEditorSettings Result = FEditor;
  // What the editor itself keeps (the last find) as it is now.
  const CoreBridge::TEditorSettings Current = CoreBridge::EditorSettings();
  Result.FindText = Current.FindText;
  Result.ReplaceText = Current.ReplaceText;
  Result.FindMatchCase = Current.FindMatchCase;
  Result.FindWholeWord = Current.FindWholeWord;
  Result.CheckNotModified = FEditorCheckNotModified->isChecked();
  Result.WordWrap = FEditorWordWrap->isChecked();
  Result.TabSize = FEditorTabSize->value();
  Result.Utf8 = FEditorEncoding->currentData().toBool();
  Result.WarnOnLargeFile = FEditorWarnLargeFile->isChecked();
  Result.LargeFileSize = FEditorLargeFileSize->value();
  return Result;
}

namespace
{

// WinSCP's questions name it; here, the program.
QString IntegrationText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text)).replace(QStringLiteral("WinSCP"),
    QString::fromStdString(CoreBridge::Info().AppName)).remove(QStringLiteral("**"));
}

}  // namespace

// Integration, as WinSCP's IntegrationSheet (its "Windows Shell" group;
// Explorer's Send To on Windows only).
QWidget * PreferencesDialog::MakeIntegrationPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
#if defined(Q_OS_WIN)
  QGroupBox * Shell = new QGroupBox(tr("Windows Shell"), Page);
#else
  QGroupBox * Shell = new QGroupBox(tr("Desktop"), Page);
#endif
  QVBoxLayout * ShellLayout = new QVBoxLayout(Shell);
  const QString AppName = QString::fromStdString(CoreBridge::Info().AppName);
  QPushButton * DesktopIcon = new QPushButton(tr("Create a &desktop icon"), Shell);
  connect(DesktopIcon, &QPushButton::clicked, this, [this]()
  {
    bool AllUsers = false;
#if defined(Q_OS_WIN)
    // As IconButtonClick: for all users, or the current one.
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = IntegrationText(CoreBridge::TText::CreateDesktopIconAllUsers).toStdString();
    for (unsigned Answer : { CoreBridge::AnswerYes, CoreBridge::AnswerNo, CoreBridge::AnswerCancel })
    {
      CoreBridge::TQueryButton Button;
      Button.Answer = Answer;
      Button.Caption = (Answer == CoreBridge::AnswerYes) ? "&Yes" : ((Answer == CoreBridge::AnswerNo) ? "&No" : "Cancel");
      Query.Buttons.push_back(Button);
    }
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const unsigned Answer = Frontend.Query(Query).Answer;
    if (Answer == CoreBridge::AnswerCancel)
    {
      return;
    }
    AllUsers = (Answer == CoreBridge::AnswerYes);
#endif
    QString Error;
    if (!Integration::CreateDesktopIcon(AllUsers, Error))
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      CoreBridge::TError Failure;
      Failure.Message = Error.toStdString();
      Frontend.ShowError(Failure);
    }
  });
#if defined(Q_OS_WIN)
  // As SendToHookButton: Explorer's Send To uploads with the program.
  QPushButton * SendTo = new QPushButton(tr("Add upload shortcut to Explorer's '&Send to' context menu"), Shell);
  connect(SendTo, &QPushButton::clicked, this, [this]()
  {
    Integration::AskAndCreateShortcut(this, IntegrationText(CoreBridge::TText::ConfirmCreateIcon),
      CoreBridge::AppShortcut(CoreBridge::SendToShortcutName(), { "/Upload" }), Integration::TShortcutFolder::SendTo);
  });
#endif
  QPushButton * Register = new QPushButton(tr("Register to handle &URL addresses"), Shell);
  QMenu * RegisterMenu = new QMenu(Register);
  RegisterMenu->addAction(tr("Register"), this, [this]()
  {
    Integrate(IntegrationText(CoreBridge::TText::ConfirmRegisterUrl), &Integration::RegisterUrlHandler);
  });
#if defined(Q_OS_WIN)
  RegisterMenu->addAction(tr("Make %1 &default handler...").arg(AppName), this, [this]()
  {
    Integrate(QString(), &Integration::MakeDefaultUrlHandler);
  });
#endif
  RegisterMenu->addAction(tr("Unregister"), this, [this]()
  {
    Integrate(IntegrationText(CoreBridge::TText::ConfirmUnregisterUrl), &Integration::UnregisterUrlHandler);
  });
  Register->setMenu(RegisterMenu);
  QPushButton * SearchPath = new QPushButton(tr("Add %1 to &search path").arg(AppName), Shell);
  connect(SearchPath, &QPushButton::clicked, this, [this]()
  {
#if defined(Q_OS_WIN)
    const QString Path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()) + QLatin1Char('\\');
#else
    const QString Path = Integration::SearchPathDirectory();
#endif
    Integrate(IntegrationText(CoreBridge::TText::ConfirmAddSearchPath).replace(QStringLiteral("%s"), Path)
      .replace(QStringLiteral("%%"), QStringLiteral("%")), &Integration::AddToSearchPath);
  });
  ShellLayout->addWidget(DesktopIcon);
#if defined(Q_OS_WIN)
  ShellLayout->addWidget(SendTo);
#endif
  ShellLayout->addWidget(Register);
  ShellLayout->addWidget(SearchPath);
  Layout->addWidget(Shell);
  Layout->addStretch(1);
  return Page;
}

bool PreferencesDialog::Integrate(const QString & Question, bool (*Action)(QString & Error))
{
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  if (!Question.isEmpty())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = Question.toStdString();
    CoreBridge::TQueryButton Yes;
    Yes.Answer = CoreBridge::AnswerYes;
    Yes.Caption = "&Yes";
    CoreBridge::TQueryButton No;
    No.Answer = CoreBridge::AnswerNo;
    No.Caption = "&No";
    Query.Buttons = { Yes, No };
    Query.CancelAnswer = CoreBridge::AnswerNo;
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerYes)
    {
      return false;
    }
  }
  QString Error;
  if (!Action(Error))
  {
    CoreBridge::TError Failure;
    Failure.Message = Error.toStdString();
    Frontend.ShowError(Failure);
    return false;
  }
  return true;
}

int PreferencesDialog::QueueViewChoice() const
{
  for (int Show = 0; Show < 3; Show++)
  {
    if ((FQueueView[Show] != nullptr) && FQueueView[Show]->isChecked())
    {
      return Show;
    }
  }
  return -1;
}

// As TPreferencesDialog::CanSetMasterPassword: not while another instance
// runs, unless told so.
bool PreferencesDialog::CanSetMasterPassword()
{
  while (SingleInstance::AnyOtherInstance())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreBridge::Text(CoreBridge::TText::MasterPasswordOtherInstance);
    for (unsigned int Answer : { CoreBridge::AnswerRetry, CoreBridge::AnswerIgnore, CoreBridge::AnswerCancel })
    {
      CoreBridge::TQueryButton Button;
      Button.Answer = Answer;
      Button.Caption = (Answer == CoreBridge::AnswerRetry) ? "&Retry" : ((Answer == CoreBridge::AnswerIgnore) ? "&Ignore" : "Cancel");
      Query.Buttons.push_back(Button);
    }
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    const unsigned int Answer = Frontend.Query(Query).Answer;
    if (Answer == CoreBridge::AnswerIgnore)
    {
      return true;
    }
    if (Answer != CoreBridge::AnswerRetry)
    {
      return false;
    }
  }
  return true;
}

// As MasterPasswordChanged: what was done, and what could not be.
void PreferencesDialog::MasterPasswordChanged(CoreBridge::TText Message, const std::vector<std::string> & Errors)
{
  CoreBridge::TQuery Query;
  Query.Type = Errors.empty() ? CoreBridge::TQueryType::Information : CoreBridge::TQueryType::Warning;
  QString Text = QString::fromStdString(CoreBridge::Text(Message));
  if (!Errors.empty())
  {
    Text = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::MasterPasswordRecryptErrors)).replace(QStringLiteral("%s"), Text);
    for (const std::string & Error : Errors)
    {
      Query.MoreMessages += Error + "\n";
    }
  }
  Query.Message = Text.toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  Query.Buttons = { OK };
  Query.CancelAnswer = CoreBridge::AnswerOK;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  Frontend.Query(Query);
}

void PreferencesDialog::ChangeMasterPassword(CoreBridge::TText Message)
{
  std::string New;
  if (MasterPasswordDialog::AskChange(this, New))
  {
    try
    {
      MasterPasswordChanged(Message, CoreBridge::ChangeMasterPassword(New));
    }
    catch (const CoreBridge::TCoreError & E)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
  }
}

// As UseMasterPasswordCheckClick: set, or cleared once the current one is
// given; the box then says what is.
void PreferencesDialog::UseMasterPasswordClicked()
{
  if (FUseMasterPassword->isChecked() != CoreBridge::UseMasterPassword())
  {
    if (CanSetMasterPassword())
    {
      if (FUseMasterPassword->isChecked())
      {
        ChangeMasterPassword(CoreBridge::TText::MasterPasswordSet);
      }
      else if (MasterPasswordDialog::AskCurrent(this))
      {
        try
        {
          MasterPasswordChanged(CoreBridge::TText::MasterPasswordCleared, CoreBridge::ClearMasterPassword());
        }
        catch (const CoreBridge::TCoreError & E)
        {
          QtFrontend Frontend;
          Frontend.SetWindow(this);
          Frontend.ShowError(E.Error);
        }
      }
    }
  }
  FUseMasterPassword->setChecked(CoreBridge::UseMasterPassword());
  FSetMasterPassword->setEnabled(CoreBridge::UseMasterPassword());
}

// As UpdateSshHostCAsViewView: PuTTY's, read again, or linscp's own list.
void PreferencesDialog::LoadHostCAs()
{
  const bool FromPuTTY = FHostCAsFromPuTTY->isChecked();
  const std::vector<CoreBridge::TSshHostCA> List = FromPuTTY ? CoreBridge::PuttySshHostCAs() : FHostCAList;
  const int Current = FHostCAs->indexOfTopLevelItem(FHostCAs->currentItem());
  FHostCAs->clear();
  for (const CoreBridge::TSshHostCA & CA : List)
  {
    new QTreeWidgetItem(FHostCAs, { QString::fromStdString(CA.Name), QString::fromStdString(CA.ValidityExpression) });
  }
  if ((Current >= 0) && (Current < FHostCAs->topLevelItemCount()))
  {
    FHostCAs->setCurrentItem(FHostCAs->topLevelItem(Current));
  }
  UpdateHostCAControls();
}

// As UpdateControls' host CAs part: the list's buttons for linscp's own,
// PuTTY's edited in PuTTY.
void PreferencesDialog::UpdateHostCAControls()
{
  const bool Own = !FHostCAsFromPuTTY->isChecked();
  FHostCAs->setEnabled(Own);
  const bool Selected = (FHostCAs->currentItem() != nullptr);
  FAddHostCA->setVisible(Own);
  FEditHostCA->setVisible(Own);
  FEditHostCA->setEnabled(Selected);
  FRemoveHostCA->setVisible(Own);
  FRemoveHostCA->setEnabled(Selected);
  FConfigureHostCAs->setVisible(!Own);
}

void PreferencesDialog::EditHostCA(bool Add)
{
  const int Index = FHostCAs->indexOfTopLevelItem(FHostCAs->currentItem());
  if (!Add && (Index < 0))
  {
    return;
  }
  SshHostCADialog Dialog(Add, Add ? CoreBridge::TSshHostCA() : FHostCAList[Index], this);
  if (Dialog.exec() == QDialog::Accepted)
  {
    if (Add)
    {
      FHostCAList.push_back(Dialog.CA());
      LoadHostCAs();
      FHostCAs->setCurrentItem(FHostCAs->topLevelItem(FHostCAs->topLevelItemCount() - 1));
    }
    else
    {
      FHostCAList[Index] = Dialog.CA();
      LoadHostCAs();
    }
  }
}

// Environment, as WinSCP's PreferencesSheet: the confirmations (in its
// order) and the notifications.
QWidget * PreferencesDialog::MakeEnvironmentPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Confirmations = new QGroupBox(tr("Confirmations"), Page);
  QVBoxLayout * ConfirmationsLayout = new QVBoxLayout(Confirmations);
  auto Add = [Confirmations, ConfirmationsLayout](const QString & Caption, bool Checked)
  {
    QCheckBox * Check = new QCheckBox(Caption, Confirmations);
    Check->setChecked(Checked);
    ConfirmationsLayout->addWidget(Check);
    return Check;
  };
  FConfirmTransferring = Add(tr("&Transferring of files"), Preferences.ConfirmTransferring);
  connect(FConfirmTransferring, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  // Indented under it; grayed (the default, asAuto) only while it has
  // never been changed, as WinSCP's AllowGrayed.
  FConfirmDDTransfer = new QCheckBox(tr("D&rag && drop operations and paste to other applications"), Confirmations);
  FConfirmDDTransfer->setTristate(Preferences.DDTransferConfirmation == TPreferences::TAutoSwitch::Auto);
  FConfirmDDTransfer->setCheckState((Preferences.DDTransferConfirmation == TPreferences::TAutoSwitch::Auto) ? Qt::PartiallyChecked :
    ((Preferences.DDTransferConfirmation == TPreferences::TAutoSwitch::On) ? Qt::Checked : Qt::Unchecked));
  connect(FConfirmDDTransfer, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState State)
  {
    if (State != Qt::PartiallyChecked)
    {
      FConfirmDDTransfer->setTristate(false);
    }
  });
  QHBoxLayout * DDLayout = new QHBoxLayout();
  DDLayout->addSpacing(16);
  DDLayout->addWidget(FConfirmDDTransfer, 1);
  ConfirmationsLayout->addLayout(DDLayout);
  FConfirmOverwriting = Add(tr("&Overwriting of files"), Preferences.ConfirmOverwriting);
  FConfirmResume = Add(tr("Trans&fer resuming"), Preferences.ConfirmResume);
  // BackgroundConfirmationsLink: to the Background page.
  QLabel * BackgroundLink = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(tr("Change confirmations of background transfers").toHtmlEscaped()),
    Confirmations);
  BackgroundLink->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  connect(BackgroundLink, &QLabel::linkActivated, this, [this]()
  {
    if (ShowPage(tr("Background")))
    {
      FQueueNoConfirmation->setFocus();
    }
  });
  ConfirmationsLayout->addWidget(BackgroundLink);
  FConfirmDeleting = Add(tr("&Deleting of files (recommended)"), Preferences.ConfirmDeleting);
  FConfirmRecycling = Add(tr("&Moving files to Recycle bin"), Preferences.ConfirmRecycling);
  FConfirmClosingSession = Add(tr("Closing sessions when exiting appli&cation"), Preferences.ConfirmClosingSession);
  FConfirmExitOnCompletion = Add(tr("Exiting application on o&peration completion"), Preferences.ConfirmExitOnCompletion);
  FConfirmCommandSession = Add(tr("Opening separate &shell session"), Preferences.ConfirmCommandSession);
  FSynchronizeSummary = Add(tr("Sync&hronization summary"), Preferences.SynchronizeSummary);
  FContinueOnError = Add(tr("Continue on &error (advanced users)"), Preferences.ContinueOnError);
  Layout->addWidget(Confirmations);
  QGroupBox * Notifications = new QGroupBox(tr("Notifications"), Page);
  QGridLayout * NotificationsLayout = new QGridLayout(Notifications);
  FBeepOnFinish = new QCheckBox(tr("&Beep when work finishes, if it lasted more than"), Notifications);
  FBeepOnFinish->setChecked(Preferences.BeepOnFinish);
  connect(FBeepOnFinish, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  FBeepOnFinishAfter = new QSpinBox(Notifications);
  FBeepOnFinishAfter->setRange(0, 24 * 60 * 60);
  FBeepOnFinishAfter->setSuffix(tr(" s"));
  FBeepOnFinishAfter->setValue(Preferences.BeepOnFinishAfter);
  FBeepOnFinishAfter->setAccessibleName(tr("Beep when work lasted more than seconds"));
  NotificationsLayout->addWidget(FBeepOnFinish, 0, 0);
  NotificationsLayout->addWidget(FBeepOnFinishAfter, 0, 1);
  // The tray's, where the desktop has one.
  FBalloonNotifications = new QCheckBox(tr("Show balloon &notifications in taskbar status area (system tray)"), Notifications);
  FBalloonNotifications->setChecked(Preferences.BalloonNotifications);
  NotificationsLayout->addWidget(FBalloonNotifications, 1, 0, 1, 2);
  FFlashTaskbar = new QCheckBox(tr("Flash taskbar button when &attention is needed while on background"), Notifications);
  FFlashTaskbar->setChecked(Preferences.FlashTaskbar);
  NotificationsLayout->addWidget(FFlashTaskbar, 2, 0, 1, 2);
  NotificationsLayout->setColumnStretch(0, 1);
  Layout->addWidget(Notifications);
  Layout->addStretch(1);
  return Page;
}

// Interface, as WinSCP's GeneralSheet: the Commander or the Explorer
// interface (the choice applies on the next start) and the theme, applied at
// once.
QWidget * PreferencesDialog::MakeInterfacePage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  // InterfaceChangeLabel: the interface is the window's from its start.
  FInterfaceChange = new QLabel(tr("Changes will apply on the next start."), Page);
  Layout->addWidget(FInterfaceChange);
  QGroupBox * Interface = new QGroupBox(tr("User Interface"), Page);
  QGridLayout * InterfaceLayout = new QGridLayout(Interface);
  QRadioButton * Commander = new QRadioButton(tr("&Commander"), Interface);
  FCommanderInterface = Commander;
  QLabel * CommanderDescription = new QLabel(
    tr("- two panels (left for local directory, right for remote directory)\n"
       "- keyboard shortcuts like in Norton Commander (and other similar programs as Total Commander, Midnight Commander...)\n"
       "- drag && drop to/from both panels").replace(QStringLiteral("&&"), QStringLiteral("&")), Interface);
  CommanderDescription->setWordWrap(true);
  CommanderDescription->setProperty("literalText", true);
  QRadioButton * Explorer = new QRadioButton(tr("&Explorer"), Interface);
  FExplorerInterface = Explorer;
  FExplorerAtStart = Preferences.ExplorerInterface;
  Explorer->setChecked(Preferences.ExplorerInterface);
  Commander->setChecked(!Preferences.ExplorerInterface);
  connect(Explorer, &QRadioButton::toggled, this, &PreferencesDialog::UpdateControls);
  QLabel * ExplorerDescription = new QLabel(
    tr("- only remote directory\n- keyboard shortcuts like in Windows File Explorer\n- drag && drop").replace(QStringLiteral("&&"), QStringLiteral("&")),
    Interface);
  ExplorerDescription->setWordWrap(true);
  ExplorerDescription->setProperty("literalText", true);
  InterfaceLayout->addWidget(Commander, 0, 0);
  InterfaceLayout->addWidget(CommanderDescription, 1, 0);
  InterfaceLayout->addWidget(Explorer, 0, 1);
  InterfaceLayout->addWidget(ExplorerDescription, 1, 1);
  InterfaceLayout->setRowStretch(1, 1);
  InterfaceLayout->setColumnStretch(0, 1);
  InterfaceLayout->setColumnStretch(1, 1);
  Layout->addWidget(Interface);
  QGroupBox * Theme = new QGroupBox(tr("Theme"), Page);
  QFormLayout * ThemeLayout = new QFormLayout(Theme);
  FTheme = new QComboBox(Theme);
  FTheme->addItems({ tr("Automatic"), tr("Light"), tr("Dark") });
  FTheme->setCurrentIndex(IndexOfAutoSwitch(Preferences.DarkTheme));
  ThemeLayout->addRow(tr("Interface &theme:"), FTheme);
  Layout->addWidget(Theme);
  Layout->addStretch(1);
  return Page;
}

// Panels > Remote, as WinSCP's PanelRemoteSheet.
QWidget * PreferencesDialog::MakeRemotePage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Remote panel"), Page);
  QGridLayout * GroupLayout = new QGridLayout(Group);
  FShowInaccesibleDirectories = new QCheckBox(tr("Show in&accessible directories"), Group);
  FShowInaccesibleDirectories->setChecked(Preferences.ShowInaccesibleDirectories);
  GroupLayout->addWidget(FShowInaccesibleDirectories, 0, 0, 1, 2);
  FAutoReadDirectoryAfterOp = new QCheckBox(tr("Auto&matically refresh directory after operation (Ctrl+Alt+R)"), Group);
  FAutoReadDirectoryAfterOp->setChecked(Preferences.AutoReadDirectoryAfterOp);
  GroupLayout->addWidget(FAutoReadDirectoryAfterOp, 1, 0, 1, 2);
  FRefreshRemotePanel = new QCheckBox(tr("Refresh remote panel &every"), Group);
  FRefreshRemotePanel->setChecked(Preferences.RefreshRemotePanel);
  connect(FRefreshRemotePanel, &QCheckBox::toggled, this, &PreferencesDialog::UpdateControls);
  FRefreshRemotePanelInterval = new QSpinBox(Group);
  FRefreshRemotePanelInterval->setRange(1, 24 * 60 * 60);
  FRefreshRemotePanelInterval->setSuffix(tr(" s"));
  FRefreshRemotePanelInterval->setValue(std::max(1, Preferences.RefreshRemotePanelInterval));
  FRefreshRemotePanelInterval->setAccessibleName(tr("Refresh remote panel every seconds"));
  GroupLayout->addWidget(FRefreshRemotePanel, 2, 0);
  GroupLayout->addWidget(FRefreshRemotePanelInterval, 2, 1);
  GroupLayout->setColumnStretch(0, 1);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

// Panels > Local, as WinSCP's PanelLocalSheet; its "Use system file context
// menu" (the Windows shell's) on Windows only.
QWidget * PreferencesDialog::MakeLocalPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Local panel"), Page);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  FPreserveLocalDirectory = new QCheckBox(tr("Do not &change state when switching sessions"), Group);
  FPreserveLocalDirectory->setChecked(Preferences.PreserveLocalDirectory);
  GroupLayout->addWidget(FPreserveLocalDirectory);
  FSystemContextMenu = new QCheckBox(tr("Use &system file context menu"), Group);
  FSystemContextMenu->setChecked(Preferences.SystemContextMenu);
  FSystemContextMenu->setVisible(Integration::HasSystemContextMenu());
  GroupLayout->addWidget(FSystemContextMenu);
  FDeleteToRecycleBin = new QCheckBox(tr("&Delete files to recycle bin"), Group);
  FDeleteToRecycleBin->setChecked(Preferences.DeleteToRecycleBin);
  GroupLayout->addWidget(FDeleteToRecycleBin);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

// Transfer > Drag & Drop, as WinSCP's DragDropSheet: remote files dragged
// to other programs go through a temporary folder, or on Windows to where
// WinSCP's shell extension says the fake file was dropped.
QWidget * PreferencesDialog::MakeDragDropPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Drag && Drop downloads"), Page);
  QGridLayout * GroupLayout = new QGridLayout(Group);
  // The fake file: Windows only (WinSCP's shell extension, P3-24), its
  // status as DragExtStatusLabel's.
  const bool Windows = Integration::HasSystemContextMenu();
  FDDFakeFile = new QRadioButton(tr("Determine drop target by dragging a &fake file"), Group);
  QRadioButton * FakeFile = FDDFakeFile;
  FakeFile->setEnabled(Windows);
  QLabel * FakeFileLabel = new QLabel(tr("Allows direct downloads to regular local folders (e.g. Windows File Explorer). "
    "Does not allow downloads to other destinations (ZIP archives,  FTP, etc.). Uses drag&&drop shell extension, when available.")
    .replace(QStringLiteral("&&"), QStringLiteral("&")), Group);
  FakeFileLabel->setWordWrap(true);
  FakeFileLabel->setEnabled(Windows);
  FakeFileLabel->setProperty("literalText", true);
  FDragExtStatus = new QLabel(Group);
  FDragExtStatus->setWordWrap(true);
  using CoreBridge::TText;
  FDragExtStatus->setText(QString::fromStdString(CoreBridge::Text(
    !Windows ? TText::DragExtNotInstalled : (DragExt::Broken() ? TText::DragExtBroken :
      (!DragExt::Installed() ? TText::DragExtNotInstalled : (DragExt::Running() ? TText::DragExtRunning : TText::DragExtNotRunning))))));
  // As DDDrivesMemo: the network drives (fixed ones are watched anyway).
  FDDDrivesLabel = new QLabel(tr("Allow dropping files to these &network drives:"), Group);
  FDDDrives = new QPlainTextEdit(Group);
  FDDDrives->setMaximumHeight(FDDDrives->fontMetrics().height() * 4);
  QStringList Drives;
  for (const std::string & Drive : Preferences.DDDrives)
  {
    Drives.append(QString::fromStdString(Drive));
  }
  FDDDrives->setPlainText(Drives.join(QLatin1Char('\n')));
  FDDDrivesLabel->setBuddy(FDDDrives);
  QRadioButton * Temporary = new QRadioButton(tr("Download files via &temporary folder"), Group);
  FDDFakeFile->setChecked(Windows && Preferences.DDFakeFile);
  Temporary->setChecked(!FDDFakeFile->isChecked());
  connect(Temporary, &QRadioButton::toggled, this, &PreferencesDialog::UpdateControls);
  QLabel * TemporaryLabel = new QLabel(tr("Allows downloads to any destinations (regular folders, ZIP archives,  FTP, etc.). "
    "Files are downloaded first to temporary folder, from where they are delivered to the destination."), Group);
  TemporaryLabel->setWordWrap(true);
  FDDWarnLackOfTempSpace = new QCheckBox(tr("&Warn when there is not enough free space"), Group);
  FDDWarnLackOfTempSpace->setChecked(Preferences.DDWarnLackOfTempSpace);
  GroupLayout->addWidget(FakeFile, 0, 0, 1, 2);
  GroupLayout->addWidget(FakeFileLabel, 1, 1);
  GroupLayout->addWidget(FDragExtStatus, 2, 1);
  GroupLayout->addWidget(FDDDrivesLabel, 3, 1);
  GroupLayout->addWidget(FDDDrives, 4, 1);
  GroupLayout->addWidget(Temporary, 5, 0, 1, 2);
  GroupLayout->addWidget(TemporaryLabel, 6, 1);
  GroupLayout->addWidget(FDDWarnLackOfTempSpace, 7, 1);
  GroupLayout->setColumnMinimumWidth(0, 16);
  GroupLayout->setColumnStretch(1, 1);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

// Updates, as WinSCP's UpdatesSheet, for linscp's releases (GitHub's):
// no authorized e-mail (WinSCP's donors' automatic updates) and no usage
// statistics (linscp collects none).
QWidget * PreferencesDialog::MakeUpdatesPage(const TPreferences & Preferences)
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Automatic = new QGroupBox(tr("Automatic updates"), Page);
  QFormLayout * AutomaticLayout = new QFormLayout(Automatic);
  FUpdatesPeriod = new QComboBox(Automatic);
  FUpdatesPeriod->addItems({ tr("Never"), tr("Daily"), tr("Weekly"), tr("Monthly") });
  // As LoadConfiguration: the period rounded up to one of the four.
  const int Period = Preferences.UpdatesPeriod;
  FUpdatesPeriod->setCurrentIndex((Period <= 0) ? 0 : ((Period <= 1) ? 1 : ((Period <= 7) ? 2 : 3)));
  AutomaticLayout->addRow(tr("Automatic check &period:"), FUpdatesPeriod);
  FUpdatesShowOnStartup = new QCheckBox(tr("&Display information about update on startup"), Automatic);
  FUpdatesShowOnStartup->setChecked(Preferences.UpdatesShowOnStartup);
  AutomaticLayout->addRow(FUpdatesShowOnStartup);
  Layout->addWidget(Automatic);
  QGroupBox * Connection = new QGroupBox(tr("Connection"), Page);
  QGridLayout * ConnectionLayout = new QGridLayout(Connection);
  FUpdatesDirect = new QRadioButton(tr("&No proxy"), Connection);
  FUpdatesAuto = new QRadioButton(tr("&Automatically detect proxy settings"), Connection);
  FUpdatesProxy = new QRadioButton(tr("&Use proxy server"), Connection);
  FUpdatesAuto->setChecked(Preferences.UpdatesConnectionType == 1);
  FUpdatesProxy->setChecked(Preferences.UpdatesConnectionType == 2);
  FUpdatesDirect->setChecked(!FUpdatesAuto->isChecked() && !FUpdatesProxy->isChecked());
  for (QRadioButton * Button : { FUpdatesDirect, FUpdatesAuto, FUpdatesProxy })
  {
    connect(Button, &QRadioButton::toggled, this, &PreferencesDialog::UpdateControls);
  }
  FUpdatesProxyHost = new QLineEdit(QString::fromStdString(Preferences.UpdatesProxyHost), Connection);
  FUpdatesProxyPort = new QSpinBox(Connection);
  FUpdatesProxyPort->setRange(1, 65535);
  FUpdatesProxyPort->setValue(std::clamp(Preferences.UpdatesProxyPort, 1, 65535));
  QLabel * HostLabel = new QLabel(tr("Proxy &hostname:"), Connection);
  HostLabel->setBuddy(FUpdatesProxyHost);
  QLabel * PortLabel = new QLabel(tr("Po&rt number:"), Connection);
  PortLabel->setBuddy(FUpdatesProxyPort);
  ConnectionLayout->addWidget(FUpdatesDirect, 0, 0, 1, 3);
  ConnectionLayout->addWidget(FUpdatesAuto, 1, 0, 1, 3);
  ConnectionLayout->addWidget(FUpdatesProxy, 2, 0, 1, 3);
  ConnectionLayout->addWidget(HostLabel, 3, 1);
  ConnectionLayout->addWidget(PortLabel, 3, 2);
  ConnectionLayout->addWidget(FUpdatesProxyHost, 4, 1);
  ConnectionLayout->addWidget(FUpdatesProxyPort, 4, 2);
  ConnectionLayout->setColumnMinimumWidth(0, 16);
  ConnectionLayout->setColumnStretch(1, 1);
  Layout->addWidget(Connection);
  QGroupBox * Options = new QGroupBox(tr("Options"), Page);
  QFormLayout * OptionsLayout = new QFormLayout(Options);
  FUpdatesBetaVersions = new QComboBox(Options);
  FUpdatesBetaVersions->addItems({ tr("Auto"), tr("Off"), tr("On") });
  FUpdatesBetaVersions->setCurrentIndex(IndexOfAutoSwitch(Preferences.UpdatesBetaVersions));
  OptionsLayout->addRow(tr("Check for &beta versions:"), FUpdatesBetaVersions);
  Layout->addWidget(Options);
  Layout->addStretch(1);
  return Page;
}
