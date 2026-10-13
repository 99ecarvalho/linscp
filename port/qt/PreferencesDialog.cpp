#include "PreferencesDialog.h"

#include "TransferSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QGridLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

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
  FNavigation->setMaximumWidth(170);
  FPages = new QStackedWidget(this);
  Main->addWidget(FNavigation);
  Main->addWidget(FPages, 1);
  Layout->addLayout(Main, 1);

  const TPreferences Preferences = CoreBridge::Preferences();
  FDefaultSettings = CoreBridge::DefaultTransferSettings();
  FPresets = CoreBridge::TransferPresets();
  AddPage(tr("Panels"), MakePanelsPage(Preferences));
  AddPage(tr("Commander"), MakeCommanderPage(Preferences));
  AddPage(tr("Transfer"), MakeTransferPage());
  AddPage(tr("Background"), MakeBackgroundPage(Preferences));
  AddPage(tr("Endurance"), MakeEndurancePage(Preferences));
  AddPage(tr("Languages"), MakeLanguagesPage());
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
  for (QCheckBox * Check : { FShowHiddenFiles, FDefaultDirIsHome, FRenameWholeName, FNaturalSorting, FDirectoriesByName })
  {
    CommonLayout->addWidget(Check);
  }
  QFormLayout * Sizes = new QFormLayout();
  FSizeFormat = new QComboBox(Common);
  // TFormatBytesStyle's order: fbNone, fbKilobytes, fbShort.
  FSizeFormat->addItems({ tr("Bytes"), tr("Kilobytes"), tr("Short format") });
  FSizeFormat->setCurrentIndex(static_cast<int>(Preferences.SizeFormat));
  Sizes->addRow(tr("Show file si&zes in:"), FSizeFormat);
  CommonLayout->addLayout(Sizes);
  Layout->addWidget(Common);
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
  Layout->addWidget(Misc);
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
  FReopenAutoSeconds->setEnabled(FReopenAuto->isChecked());
  FReopenAutoIdleSeconds->setEnabled(FReopenAutoIdle->isChecked());
  FReopenAutoStallSeconds->setEnabled(FReopenAutoStall->isChecked());
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
    FPresetsList->addItem(QString::fromStdString(Preset.Name));
  }
  FPresetsList->setCurrentRow(qBound(0, Row, FPresetsList->count() - 1));
}

QWidget * PreferencesDialog::MakeTransferPage()
{
  QWidget * Page = new QWidget(this);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Transfer settings presets"), Page);
  QHBoxLayout * GroupLayout = new QHBoxLayout(Group);
  FPresetsList = new QListWidget(Group);
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
  if (Dialog.exec() == QDialog::Accepted)
  {
    if (Add)
    {
      AddPreset(Dialog.PresetName(), Dialog.Settings());
    }
    else
    {
      EditSelected(Dialog.PresetName(), Dialog.Settings());
    }
  }
}

bool PreferencesDialog::AddPreset(const QString & Name, const CoreBridge::TTransferSettings & Settings)
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
  const int Row = FPresetsList->currentRow();
  // As WinSCP: after the selected one.
  const int At = qBound(0, Row, static_cast<int>(FPresets.size()));
  FPresets.insert(FPresets.begin() + At, Preset);
  LoadPresets();
  FPresetsList->setCurrentRow(At + 1);
  return true;
}

bool PreferencesDialog::EditSelected(const QString & Name, const CoreBridge::TTransferSettings & Settings)
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
  for (QCheckBox * Check : { FQueue, FQueueNoConfirmation, FQueueParallel, FQueueAutoPopup })
  {
    Form->addRow(Check);
  }
  Layout->addWidget(Group);
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
  ReopenLayout->addWidget(new QLabel(tr("&Keep reconnecting for:"), Reopen), 4, 0);
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
  Result.NaturalOrderNumericalSorting = FNaturalSorting->isChecked();
  Result.AlwaysSortDirectoriesByName = FDirectoriesByName->isChecked();
  Result.SizeFormat = static_cast<TPreferences::TSizeFormat>(FSizeFormat->currentIndex());
  Result.SwappedPanels = FSwappedPanels->isChecked();
  Result.ExplorerKeyboardShortcuts = (FKeyboardShortcuts->currentIndex() == 1);
  Result.QueueTransfersLimit = FQueueTransfersLimit->value();
  Result.QueueKeepDoneItemsFor = FQueueKeepDone->currentData().toInt();
  Result.QueueAutoPopup = FQueueAutoPopup->isChecked();
  Result.SessionReopenAuto = FReopenAuto->isChecked() ? FReopenAutoSeconds->value() : 0;
  Result.SessionReopenAutoIdleOn = FReopenAutoIdle->isChecked();
  Result.SessionReopenAutoIdle = FReopenAutoIdleSeconds->value();
  Result.SessionReopenAutoInactive = FReopenAutoInactive->isChecked();
  Result.SessionReopenAutoStall = FReopenAutoStall->isChecked() ? FReopenAutoStallSeconds->value() : 0;
  Result.SessionReopenTimeout = FReopenTimeout->value();
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
  if (FLanguages->currentItem() != nullptr)
  {
    CoreBridge::SetPreferredLocale(Locale());
  }
}
