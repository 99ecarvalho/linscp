// The Preferences dialog, as WinSCP's (forms/Preferences.cpp): its pages
// listed on the left, each one's settings on the right. The pages are
// WinSCP's, with the settings the shell has so far (P3-04a): Panels,
// Commander, Languages. What they set is upstream's configuration
// (ADR 0014).
#ifndef LINSCP_QT_PREFERENCES_DIALOG_H
#define LINSCP_QT_PREFERENCES_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;

class PreferencesDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PreferencesDialog(QWidget * Parent = nullptr);

  // What the pages say now, and the language chosen on Languages.
  CoreBridge::TPreferences Preferences() const;
  unsigned Locale() const;
  // Saves what the pages say, as the dialog's OK does.
  void Save();

  // The pages and their controls, for tests.
  QListWidget * Navigation() const { return FNavigation; }
  QCheckBox * ShowHiddenFilesCheck() const { return FShowHiddenFiles; }
  QCheckBox * DefaultDirIsHomeCheck() const { return FDefaultDirIsHome; }
  QCheckBox * RenameWholeNameCheck() const { return FRenameWholeName; }
  QCheckBox * NaturalSortingCheck() const { return FNaturalSorting; }
  QCheckBox * DirectoriesByNameCheck() const { return FDirectoriesByName; }
  QComboBox * SizeFormatCombo() const { return FSizeFormat; }
  QCheckBox * SwappedPanelsCheck() const { return FSwappedPanels; }
  QComboBox * KeyboardShortcutsCombo() const { return FKeyboardShortcuts; }
  QListWidget * LanguagesList() const { return FLanguages; }
  QLabel * LanguageChangeLabel() const { return FLanguageChange; }

private:
  void AddPage(const QString & Name, QWidget * Page);
  QWidget * MakePanelsPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeCommanderPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeLanguagesPage();
  void UpdateControls();

  QListWidget * FNavigation = nullptr;
  QStackedWidget * FPages = nullptr;
  QCheckBox * FShowHiddenFiles = nullptr;
  QCheckBox * FDefaultDirIsHome = nullptr;
  QCheckBox * FRenameWholeName = nullptr;
  QCheckBox * FNaturalSorting = nullptr;
  QCheckBox * FDirectoriesByName = nullptr;
  QComboBox * FSizeFormat = nullptr;
  QCheckBox * FSwappedPanels = nullptr;
  QComboBox * FKeyboardShortcuts = nullptr;
  QListWidget * FLanguages = nullptr;
  QLabel * FLanguageChange = nullptr;
};

#endif
