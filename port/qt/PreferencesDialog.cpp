#include "PreferencesDialog.h"
#include "IconSet.h"
#include <QApplication>
#include "HintButton.h"

#include "TransferSettingsDialog.h"
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
#include <QTreeWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

using TPreferences = CoreBridge::TPreferences;

namespace
{

enum { LocaleRole = Qt::UserRole };

}  // namespace

PreferencesDialog::PreferencesDialog(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Preferences"));
  resize(640, 440);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Main = new QHBoxLayout();
  FNavigation = new QListWidget(this);
  FNavigation->setAccessibleName(tr("Pages"));
  FNavigation->setMaximumWidth(170);
  FPages = new QStackedWidget(this);
  Main->addWidget(FNavigation);
  Main->addWidget(FPages, 1);
  Layout->addLayout(Main, 1);

  const TPreferences Preferences = CoreBridge::Preferences();
  FDefaultSettings = CoreBridge::DefaultTransferSettings();
  FPresets = CoreBridge::TransferPresets();
  FFileColors = Preferences.FileColors;
  // In the order of WinSCP's tree: Environment (Window, Commander,
  // Languages), Panels (File colors), Transfer (Background, Endurance).
  AddPage(tr("Window"), MakeWindowPage(Preferences));
  AddPage(tr("Commander"), MakeCommanderPage(Preferences));
  AddPage(tr("Languages"), MakeLanguagesPage());
  AddPage(tr("Panels"), MakePanelsPage(Preferences));
  AddPage(tr("File colors"), MakeFileColorsPage());
  AddPage(tr("Editors"), MakeEditorsPage());
  AddPage(tr("Internal editor"), MakeInternalEditorPage());
  AddPage(tr("Transfer"), MakeTransferPage(Preferences));
  AddPage(tr("Background"), MakeBackgroundPage(Preferences));
  AddPage(tr("Endurance"), MakeEndurancePage(Preferences));
  AddPage(tr("Network"), MakeNetworkPage(Preferences));
  AddPage(tr("Security"), MakeSecurityPage(Preferences));
  AddPage(tr("Logging"), MakeLoggingPage(Preferences));
  // Integration, its Applications, and Commands, as WinSCP's tree has them.
  AddPage(tr("Integration"), MakeIntegrationPage());
  AddPage(tr("Applications"), MakeApplicationsPage(Preferences));
  AddPage(tr("Commands"), MakeCommandsPage());
  AddPage(tr("Storage"), MakeStoragePage(Preferences));
  connect(FNavigation, &QListWidget::currentRowChanged, FPages, &QStackedWidget::setCurrentIndex);
  FNavigation->setCurrentRow(0);

  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
  FBuilt = true;
  UpdateControls();
}

void PreferencesDialog::AddPage(const QString & Name, QWidget * Page)
{
  FNavigation->addItem(Name);
  FPages->addWidget(Page);
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
  const int CommandRow = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  FEditCommand->setEnabled(CommandRow >= 0);
  FRemoveCommand->setEnabled(CommandRow >= 0);
  FUpCommand->setEnabled(CommandRow > 0);
  FDownCommand->setEnabled((CommandRow >= 0) && (CommandRow < FCommandsList->topLevelItemCount() - 1));
  FCustomIpEdit->setEnabled(FCustomIp->isChecked());
  FPortMin->setEnabled(FPortLimit->isChecked());
  FPortMax->setEnabled(FPortLimit->isChecked());
  if (FCustomIniEdit != nullptr)
  {
    FCustomIniEdit->setEnabled(FCustomIniStorage->isChecked());
  }
  FTemporaryEdit->setEnabled(FCustomTemporary->isChecked());
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
    QListWidgetItem * Item = new QListWidgetItem(QString::fromStdString(Preset.Name), FPresetsList);
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
  FKeepOpen = new QCheckBox(tr("&Keep main window open when the last session is closed"), Misc);
  FKeepOpen->setChecked(Preferences.KeepOpenWhenNoSession);
  MiscLayout->addWidget(FKeepOpen);
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
  IconsLayout->addRow(tr("Icon &set:"), FIconSet);
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
  Layout->addStretch(1);
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
  QString Name = (Add && !Duplicate) ? QString() : FPresetsList->item(Row)->text();
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
  Result.PuttyPath = FPuttyPath->text().toStdString();
  Result.PuttyPassword = FPuttyPassword->isChecked();
  Result.AutoOpenInPutty = FAutoOpenInPutty->isChecked();
  Result.TelnetForFtpInPutty = FTelnetForFtpInPutty->isChecked();
  if ((FPuttyRegistryStorageKey != nullptr) && (FPuttyRegistryStorageKey->currentIndex() >= 0))
  {
    Result.PuttyRegistryStorageKey = FPuttyRegistryStorageKey->currentText().toStdString();
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
  CoreBridge::SetCustomCommands(FCommands);
  CoreBridge::SetEditorSettings(EditorSettings());
  CoreBridge::SetEditors(FEditors);
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
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Custom commands"), Page);
  QHBoxLayout * GroupLayout = new QHBoxLayout(Group);
  FCommandsList = new QTreeWidget(Group);
  FCommandsList->setAccessibleName(Group->title());
  FCommandsList->setRootIsDecorated(false);
  FCommandsList->setHeaderLabels({ tr("Description"), tr("Command"), tr("L/R") });
  FCommandsList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  FCommandsList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  FCommandsList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  FCommandsList->header()->setStretchLastSection(false);
  GroupLayout->addWidget(FCommandsList, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  QPushButton * Add = new QPushButton(tr("&Add..."), Group);
  FEditCommand = new QPushButton(tr("&Edit..."), Group);
  FRemoveCommand = new QPushButton(tr("&Remove"), Group);
  FUpCommand = new QPushButton(tr("&Up"), Group);
  FDownCommand = new QPushButton(tr("&Down"), Group);
  for (QPushButton * Button : { Add, FEditCommand, FRemoveCommand, FUpCommand, FDownCommand })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  GroupLayout->addLayout(Buttons);
  connect(Add, &QPushButton::clicked, this, [this]() { AddEditCommand(false); });
  connect(FEditCommand, &QPushButton::clicked, this, [this]() { AddEditCommand(true); });
  connect(FRemoveCommand, &QPushButton::clicked, this, &PreferencesDialog::RemoveCommand);
  connect(FUpCommand, &QPushButton::clicked, this, [this]() { MoveCommand(-1); });
  connect(FDownCommand, &QPushButton::clicked, this, [this]() { MoveCommand(1); });
  connect(FCommandsList, &QTreeWidget::currentItemChanged, this, &PreferencesDialog::UpdateControls);
  connect(FCommandsList, &QTreeWidget::itemDoubleClicked, this, [this]() { AddEditCommand(true); });
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

// As CustomCommandsViewData.
void PreferencesDialog::LoadCommands(int Select)
{
  FCommandsList->clear();
  for (const CoreBridge::TCustomCommandDefinition & Command : FCommands)
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FCommandsList);
    Item->setText(0, QString::fromStdString(Command.Name).remove(QLatin1Char('&')));
    Item->setText(1, QString::fromStdString(Command.Command));
    Item->setText(2, QString::fromStdString(CoreBridge::CustomCommandSide(Command.Local)));
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
  FCommands.erase(FCommands.begin() + Row);
  LoadCommands(qMin(Row, static_cast<int>(FCommands.size()) - 1));
}

void PreferencesDialog::MoveCommand(int By)
{
  const int Row = FCommandsList->indexOfTopLevelItem(FCommandsList->currentItem());
  const int To = Row + By;
  if ((Row < 0) || (To < 0) || (To >= static_cast<int>(FCommands.size())))
  {
    return;
  }
  std::swap(FCommands[Row], FCommands[To]);
  LoadCommands(To);
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

// Integration, as WinSCP's IntegrationSheet (its "Windows Shell" group).
// "Send to" is not here: it needs the command line's /upload.
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
