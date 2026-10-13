// The Preferences dialog, as WinSCP's (forms/Preferences.cpp): its pages
// listed on the left, each one's settings on the right. The pages are
// WinSCP's, with the settings the shell has so far (P3-04a…c): Window,
// Commander, Languages, Panels, File colors, Transfer, Background,
// Endurance. What they set is upstream's configuration (ADR 0014).
#ifndef LINSCP_QT_PREFERENCES_DIALOG_H
#define LINSCP_QT_PREFERENCES_DIALOG_H

#include <QDialog>

#include <vector>

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
  // Transfer: the default settings (row 0) and the presets.
  QListWidget * PresetsList() const { return FPresetsList; }
  CoreBridge::TTransferSettings DefaultSettings() const { return FDefaultSettings; }
  const std::vector<CoreBridge::TTransferPreset> & Presets() const { return FPresets; }
  // Its buttons' actions, for tests and them alike: what the dialogs would
  // edit is given.
  bool AddPreset(const QString & Name, const CoreBridge::TTransferSettings & Settings);
  bool EditSelected(const QString & Name, const CoreBridge::TTransferSettings & Settings);
  void RemoveSelected();
  void MoveSelected(int By);
  class QSpinBox * QueueTransfersLimitSpin() const { return FQueueTransfersLimit; }
  QComboBox * QueueKeepDoneCombo() const { return FQueueKeepDone; }
  QCheckBox * QueueAutoPopupCheck() const { return FQueueAutoPopup; }
  QCheckBox * QueueCheck() const { return FQueue; }
  QCheckBox * QueueNoConfirmationCheck() const { return FQueueNoConfirmation; }
  QCheckBox * QueueParallelCheck() const { return FQueueParallel; }
  class QRadioButton * ResumeOnButton() const { return FResumeOn; }
  QRadioButton * ResumeSmartButton() const { return FResumeSmart; }
  QRadioButton * ResumeOffButton() const { return FResumeOff; }
  QSpinBox * ResumeThresholdSpin() const { return FResumeThreshold; }
  QCheckBox * ReopenAutoCheck() const { return FReopenAuto; }
  QSpinBox * ReopenAutoSpin() const { return FReopenAutoSeconds; }
  QRadioButton * PathFullButton() const { return FPathFull; }
  QRadioButton * PathShortButton() const { return FPathShort; }
  QRadioButton * PathNoneButton() const { return FPathNone; }
  QCheckBox * KeepOpenCheck() const { return FKeepOpen; }
  // File colors, and their buttons' actions (what the dialog would edit
  // given).
  QListWidget * FileColorsList() const { return FFileColorsList; }
  const std::vector<CoreBridge::TPreferences::TFileColor> & FileColors() const { return FFileColors; }
  void AddFileColor(const CoreBridge::TPreferences::TFileColor & Color);
  void EditFileColor(const CoreBridge::TPreferences::TFileColor & Color);
  void RemoveFileColor();
  void MoveFileColor(int By);

private:
  void AddPage(const QString & Name, QWidget * Page);
  QWidget * MakePanelsPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeCommanderPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeLanguagesPage();
  QWidget * MakeTransferPage();
  QWidget * MakeBackgroundPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeEndurancePage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeWindowPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeFileColorsPage();
  void LoadFileColors();
  bool FileColorDialog(CoreBridge::TPreferences::TFileColor & Color);
  void LoadPresets();
  void EditPresetDialog(bool Add, bool Duplicate);
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
  CoreBridge::TTransferSettings FDefaultSettings;
  std::vector<CoreBridge::TTransferPreset> FPresets;
  QListWidget * FPresetsList = nullptr;
  class QPushButton * FEditPreset = nullptr;
  QPushButton * FDuplicatePreset = nullptr;
  QPushButton * FRemovePreset = nullptr;
  QPushButton * FUpPreset = nullptr;
  QPushButton * FDownPreset = nullptr;
  QSpinBox * FQueueTransfersLimit = nullptr;
  QComboBox * FQueueKeepDone = nullptr;
  QCheckBox * FQueueAutoPopup = nullptr;
  QCheckBox * FQueue = nullptr;
  QCheckBox * FQueueNoConfirmation = nullptr;
  QCheckBox * FQueueParallel = nullptr;
  QRadioButton * FResumeOn = nullptr;
  QRadioButton * FResumeSmart = nullptr;
  QRadioButton * FResumeOff = nullptr;
  QSpinBox * FResumeThreshold = nullptr;
  QCheckBox * FReopenAuto = nullptr;
  QSpinBox * FReopenAutoSeconds = nullptr;
  QCheckBox * FReopenAutoIdle = nullptr;
  QSpinBox * FReopenAutoIdleSeconds = nullptr;
  QCheckBox * FReopenAutoInactive = nullptr;
  QCheckBox * FReopenAutoStall = nullptr;
  QSpinBox * FReopenAutoStallSeconds = nullptr;
  QSpinBox * FReopenTimeout = nullptr;
  QRadioButton * FPathFull = nullptr;
  QRadioButton * FPathShort = nullptr;
  QRadioButton * FPathNone = nullptr;
  QCheckBox * FKeepOpen = nullptr;
  std::vector<CoreBridge::TPreferences::TFileColor> FFileColors;
  QListWidget * FFileColorsList = nullptr;
  QPushButton * FEditFileColor = nullptr;
  QPushButton * FRemoveFileColor = nullptr;
  QPushButton * FUpFileColor = nullptr;
  QPushButton * FDownFileColor = nullptr;
  // The pages exist: their controls can be updated.
  bool FBuilt = false;
};

#endif
