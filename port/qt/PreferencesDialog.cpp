#include "PreferencesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
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
  AddPage(tr("Panels"), MakePanelsPage(Preferences));
  AddPage(tr("Commander"), MakeCommanderPage(Preferences));
  AddPage(tr("Languages"), MakeLanguagesPage());
  connect(FNavigation, &QListWidget::currentRowChanged, FPages, &QStackedWidget::setCurrentIndex);
  FNavigation->setCurrentRow(0);

  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
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

// As TPreferencesDialog::UpdateControls' languages part.
void PreferencesDialog::UpdateControls()
{
  FLanguageChange->setVisible((FLanguages->currentItem() != nullptr) && (Locale() != CoreBridge::AppliedLocale()));
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
  if (FLanguages->currentItem() != nullptr)
  {
    CoreBridge::SetPreferredLocale(Locale());
  }
}
