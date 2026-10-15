// The Preferences dialog, as WinSCP's (forms/Preferences.cpp): its pages
// listed on the left, each one's settings on the right. The pages are
// WinSCP's, with the settings the shell has (P3-04a…e, P3-19): all of its
// pages but Explorer's (linscp has no Explorer interface).
// What they set is upstream's configuration (ADR 0014).
#ifndef LINSCP_QT_PREFERENCES_DIALOG_H
#define LINSCP_QT_PREFERENCES_DIALOG_H

#include <QDialog>
#include <QFont>

#include <memory>
#include <vector>

#include "CoreBridge.h"

class QCheckBox;
class QFont;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;
class QListWidget;
class QStackedWidget;
class QTreeWidget;

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
  // The tree of pages, as WinSCP's; the pages in its order, by index.
  QTreeWidget * Navigation() const { return FNavigation; }
  int PageCount() const { return static_cast<int>(FPageItems.size()); }
  QString PageName(int Index) const;
  void ShowPageAt(int Index);
  // Shows the page of that name (as the tree's), if there is one.
  bool ShowPage(const QString & Name);
  // Environment, Interface, the panels' Remote and Local, Drag & Drop,
  // Updates.
  QCheckBox * ConfirmTransferringCheck() const { return FConfirmTransferring; }
  QCheckBox * ConfirmDDTransferCheck() const { return FConfirmDDTransfer; }
  QCheckBox * ConfirmDeletingCheck() const { return FConfirmDeleting; }
  QCheckBox * ConfirmRecyclingCheck() const { return FConfirmRecycling; }
  QCheckBox * ConfirmClosingSessionCheck() const { return FConfirmClosingSession; }
  QCheckBox * SynchronizeSummaryCheck() const { return FSynchronizeSummary; }
  QCheckBox * BeepOnFinishCheck() const { return FBeepOnFinish; }
  QSpinBox * BeepOnFinishAfterSpin() const { return FBeepOnFinishAfter; }
  QCheckBox * BalloonNotificationsCheck() const { return FBalloonNotifications; }
  QComboBox * ThemeCombo() const { return FTheme; }
  QCheckBox * ShowInaccesibleDirectoriesCheck() const { return FShowInaccesibleDirectories; }
  QCheckBox * RefreshRemotePanelCheck() const { return FRefreshRemotePanel; }
  QSpinBox * RefreshRemotePanelIntervalSpin() const { return FRefreshRemotePanelInterval; }
  QCheckBox * PreserveLocalDirectoryCheck() const { return FPreserveLocalDirectory; }
  QCheckBox * DeleteToRecycleBinCheck() const { return FDeleteToRecycleBin; }
  QCheckBox * SystemContextMenuCheck() const { return FSystemContextMenu; }
  QCheckBox * DDWarnLackOfTempSpaceCheck() const { return FDDWarnLackOfTempSpace; }
  QComboBox * UpdatesPeriodCombo() const { return FUpdatesPeriod; }
  QRadioButton * UpdatesProxyButton() const { return FUpdatesProxy; }
  QLineEdit * UpdatesProxyHostEdit() const { return FUpdatesProxyHost; }
  QComboBox * UpdatesBetaVersionsCombo() const { return FUpdatesBetaVersions; }
  QCheckBox * ShowHiddenFilesCheck() const { return FShowHiddenFiles; }
  QCheckBox * DefaultDirIsHomeCheck() const { return FDefaultDirIsHome; }
  QCheckBox * RenameWholeNameCheck() const { return FRenameWholeName; }
  QCheckBox * NaturalSortingCheck() const { return FNaturalSorting; }
  QCheckBox * DirectoriesByNameCheck() const { return FDirectoriesByName; }
  QComboBox * SizeFormatCombo() const { return FSizeFormat; }
  QCheckBox * SwappedPanelsCheck() const { return FSwappedPanels; }
  QComboBox * IconSetCombo() const { return FIconSet; }
  QComboBox * KeyboardShortcutsCombo() const { return FKeyboardShortcuts; }
  QCheckBox * UseLocationProfilesCheck() const { return FUseLocationProfiles; }
  QListWidget * LanguagesList() const { return FLanguages; }
  QLabel * LanguageChangeLabel() const { return FLanguageChange; }
  // Transfer: the default settings (row 0) and the presets.
  QListWidget * PresetsList() const { return FPresetsList; }
  CoreBridge::TTransferSettings DefaultSettings() const { return FDefaultSettings; }
  const std::vector<CoreBridge::TTransferPreset> & Presets() const { return FPresets; }
  // Its buttons' actions, for tests and them alike: what the dialogs would
  // edit is given.
  // With HasRule and Rule given, the preset's autoselection rule too;
  // else an edited preset keeps its own.
  bool AddPreset(const QString & Name, const CoreBridge::TTransferSettings & Settings,
    const bool * HasRule = nullptr, const CoreBridge::TTransferPresetRule * Rule = nullptr);
  bool EditSelected(const QString & Name, const CoreBridge::TTransferSettings & Settings,
    const bool * HasRule = nullptr, const CoreBridge::TTransferPresetRule * Rule = nullptr);
  // What a preset's Current button fills in: the active session's.
  void SetCurrentRuleData(const CoreBridge::TTransferPresetRule & Data)
  {
    FCurrentRuleData = Data;
    FHasCurrentRuleData = true;
  }
  QCheckBox * CopyParamAutoSelectNoticeCheck() const { return FCopyParamAutoSelectNotice; }
  QCheckBox * MinimizeToTrayCheck() const { return FMinimizeToTray; }
  QCheckBox * UseMasterPasswordCheck() const { return FUseMasterPassword; }
  QTreeWidget * HostCAsView() const { return FHostCAs; }
  const std::vector<CoreBridge::TSshHostCA> & HostCAs() const { return FHostCAList; }
  void AddHostCA(const CoreBridge::TSshHostCA & CA) { FHostCAList.push_back(CA); LoadHostCAs(); }
  QPushButton * SetMasterPasswordButton() const { return FSetMasterPassword; }
  QCheckBox * ExternalSessionInExistingInstanceCheck() const { return FExternalSessionInExistingInstance; }
  void RemoveSelected();
  void MoveSelected(int By);
  class QSpinBox * QueueTransfersLimitSpin() const { return FQueueTransfersLimit; }
  QComboBox * QueueKeepDoneCombo() const { return FQueueKeepDone; }
  QCheckBox * QueueAutoPopupCheck() const { return FQueueAutoPopup; }
  QCheckBox * QueueCheck() const { return FQueue; }
  QCheckBox * QueueParallelCheck() const { return FQueueParallel; }
  QCheckBox * ParallelThresholdCheck() const { return FParallelThresholdCheck; }
  QSpinBox * ParallelThresholdSpin() const { return FParallelThreshold; }
  QCheckBox * EnableQueueByDefaultCheck() const { return FEnableQueueByDefault; }
  QRadioButton * QueueViewButton(int Show) const { return FQueueView[Show]; }
  QCheckBox * QueueNoConfirmationCheck() const { return FQueueNoConfirmation; }
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
  QCheckBox * LoggingCheck() const { return FLogging; }
  QComboBox * LogLevelCombo() const { return FLogLevel; }
  QLineEdit * LogFileEdit() const { return FLogFile; }
  QCheckBox * LogMaxSizeCheck() const { return FLogMaxSize; }
  QComboBox * LogMaxSizeCombo() const { return FLogMaxSizeCombo; }
  QCheckBox * LogActionsCheck() const { return FLogActions; }
  QRadioButton * CustomIpButton() const { return FCustomIp; }
  QLineEdit * CustomIpEdit() const { return FCustomIpEdit; }
  QCheckBox * PortLimitCheck() const { return FPortLimit; }
  QSpinBox * PortMinSpin() const { return FPortMin; }
  QSpinBox * PortMaxSpin() const { return FPortMax; }
  QCheckBox * RememberPasswordCheck() const { return FRememberPassword; }
  QComboBox * AuthAgentCombo() const { return FAuthAgent; }
  QLabel * StorageLabel() const { return FStorageLabel; }
  QRadioButton * CustomTemporaryButton() const { return FCustomTemporary; }
  QLineEdit * TemporaryEdit() const { return FTemporaryEdit; }
  // Editors and Internal editor.
  QCheckBox * EditorCheckNotModifiedCheck() const { return FEditorCheckNotModified; }
  // The editor preference list, and its buttons' actions (Add and Edit
  // through the Editor preferences dialog; Editor given, without one).
  QTreeWidget * EditorsList() const { return FEditorsList; }
  const std::vector<CoreBridge::TEditorDefinition> & EditorDefinitions() const { return FEditors; }
  bool AddEditEditor(bool Edit, const CoreBridge::TEditorDefinition * Editor = nullptr);
  void RemoveEditor();
  void MoveEditor(int By);
  QCheckBox * EditorWordWrapCheck() const { return FEditorWordWrap; }
  QSpinBox * EditorTabSizeSpin() const { return FEditorTabSize; }
  QComboBox * EditorEncodingCombo() const { return FEditorEncoding; }
  QLabel * EditorFontLabel() const { return FEditorFontLabel; }
  QCheckBox * EditorWarnLargeFileCheck() const { return FEditorWarnLargeFile; }
  QSpinBox * EditorLargeFileSizeSpin() const { return FEditorLargeFileSize; }
  CoreBridge::TEditorSettings EditorSettings() const;
  // The font and colour buttons' choices, given.
  void SetEditorFont(const QFont & Font);
  void SetEditorColors(int FontColor, int BackgroundColor);
  // Applications.
  QLineEdit * PuttyPathEdit() const { return FPuttyPath; }
  QCheckBox * PuttyPasswordCheck() const { return FPuttyPassword; }
  QCheckBox * AutoOpenInPuttyCheck() const { return FAutoOpenInPutty; }
  QCheckBox * TelnetForFtpInPuttyCheck() const { return FTelnetForFtpInPutty; }
  QComboBox * PuttyRegistryStorageKeyCombo() const { return FPuttyRegistryStorageKey; }
  // Commands: the list, and its buttons' actions (Add and Edit through the
  // Custom command dialog; Command given, without one).
  class QTreeWidget * CommandsList() const { return FCommandsList; }
  const std::vector<CoreBridge::TCustomCommandDefinition> & Commands() const { return FCommands; }
  bool AddEditCommand(bool Edit, const CoreBridge::TCustomCommandDefinition * Command = nullptr);
  void RemoveCommand();
  void MoveCommand(int By);
  // The extensions, after the custom commands in the list (Commands' rows
  // from Commands().size() on), as WinSCP's: Add Extension (a path or a
  // URL; with Path given, that instead of asking), Configure (the
  // extension's Preferences options and its keyboard shortcut; with
  // Values given, those instead of the dialog).
  CoreBridge::TExtensionsEdit * ExtensionsEdit() const { return FExtensions.get(); }
  bool AddExtension(const QString * Path = nullptr);
  bool ConfigureCommand(const std::vector<std::string> * Values = nullptr, const unsigned * ShortCut = nullptr);
  // The session's site, for the options kept per site (ofSite).
  void SetSessionKey(const std::string & Key) { FSessionKey = Key; }
  class QToolButton * AddCommandButton() const { return FAddCommand; }
  QPushButton * ConfigureCommandButton() const { return FConfigureCommand; }

private:
  void AddPage(const QString & Name, QWidget * Page, bool Subpage = false);
  QWidget * MakePanelsPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeCommanderPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeLanguagesPage();
  QWidget * MakeTransferPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeBackgroundPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeEndurancePage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeWindowPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeFileColorsPage();
  QWidget * MakeNetworkPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeSecurityPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeLoggingPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeStoragePage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeIntegrationPage();
  // An Integration action, after its question; the error shown.
  bool Integrate(const QString & Question, bool (*Action)(QString & Error));
  QWidget * MakeApplicationsPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeEditorsPage();
  QWidget * MakeInternalEditorPage();
  void UpdateEditorFont();
  void LoadEditors(int Select);
  QWidget * MakeCommandsPage();
  QWidget * MakeEnvironmentPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeInterfacePage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeRemotePage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeLocalPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeDragDropPage(const CoreBridge::TPreferences & Preferences);
  QWidget * MakeUpdatesPage(const CoreBridge::TPreferences & Preferences);
  QCheckBox * FConfirmTransferring = nullptr;
  QCheckBox * FConfirmDDTransfer = nullptr;
  QCheckBox * FConfirmOverwriting = nullptr;
  QCheckBox * FConfirmResume = nullptr;
  QCheckBox * FConfirmDeleting = nullptr;
  QCheckBox * FConfirmRecycling = nullptr;
  QCheckBox * FConfirmClosingSession = nullptr;
  QCheckBox * FConfirmExitOnCompletion = nullptr;
  QCheckBox * FConfirmCommandSession = nullptr;
  QCheckBox * FSynchronizeSummary = nullptr;
  QCheckBox * FContinueOnError = nullptr;
  QCheckBox * FBeepOnFinish = nullptr;
  QSpinBox * FBeepOnFinishAfter = nullptr;
  QCheckBox * FBalloonNotifications = nullptr;
  QCheckBox * FFlashTaskbar = nullptr;
  QComboBox * FTheme = nullptr;
  QCheckBox * FShowInaccesibleDirectories = nullptr;
  QCheckBox * FAutoReadDirectoryAfterOp = nullptr;
  QCheckBox * FRefreshRemotePanel = nullptr;
  QSpinBox * FRefreshRemotePanelInterval = nullptr;
  QCheckBox * FPreserveLocalDirectory = nullptr;
  QCheckBox * FDeleteToRecycleBin = nullptr;
  QCheckBox * FSystemContextMenu = nullptr;
  QCheckBox * FDDWarnLackOfTempSpace = nullptr;
  QComboBox * FUpdatesPeriod = nullptr;
  QCheckBox * FUpdatesShowOnStartup = nullptr;
  QRadioButton * FUpdatesDirect = nullptr;
  QRadioButton * FUpdatesAuto = nullptr;
  QRadioButton * FUpdatesProxy = nullptr;
  QLineEdit * FUpdatesProxyHost = nullptr;
  QSpinBox * FUpdatesProxyPort = nullptr;
  QComboBox * FUpdatesBetaVersions = nullptr;
  void LoadCommands(int Select);
  void LoadFileColors();
  bool FileColorDialog(CoreBridge::TPreferences::TFileColor & Color);
  void LoadPresets();
  void EditPresetDialog(bool Add, bool Duplicate);
  void UpdateControls();

  QTreeWidget * FNavigation = nullptr;
  std::vector<class QTreeWidgetItem *> FPageItems;
  QStackedWidget * FPages = nullptr;
  QCheckBox * FShowHiddenFiles = nullptr;
  QCheckBox * FDefaultDirIsHome = nullptr;
  QCheckBox * FRenameWholeName = nullptr;
  QCheckBox * FNaturalSorting = nullptr;
  QCheckBox * FDirectoriesByName = nullptr;
  QComboBox * FSizeFormat = nullptr;
  QCheckBox * FSwappedPanels = nullptr;
  QComboBox * FIconSet = nullptr;
  QCheckBox * FParallelThresholdCheck = nullptr;
  QSpinBox * FParallelThreshold = nullptr;
  QCheckBox * FEnableQueueByDefault = nullptr;
  QCheckBox * FCopyParamAutoSelectNotice = nullptr;
  QCheckBox * FMinimizeToTray = nullptr;
  QCheckBox * FUseMasterPassword = nullptr;
  QTreeWidget * FHostCAs = nullptr;
  QPushButton * FAddHostCA = nullptr;
  QPushButton * FEditHostCA = nullptr;
  QPushButton * FRemoveHostCA = nullptr;
  QPushButton * FConfigureHostCAs = nullptr;
  QCheckBox * FHostCAsFromPuTTY = nullptr;
  std::vector<CoreBridge::TSshHostCA> FHostCAList;
  void LoadHostCAs();
  void UpdateHostCAControls();
  void EditHostCA(bool Add);
  QPushButton * FSetMasterPassword = nullptr;
  bool CanSetMasterPassword();
  void MasterPasswordChanged(CoreBridge::TText Message, const std::vector<std::string> & Errors);
  void ChangeMasterPassword(CoreBridge::TText Message);
  void UseMasterPasswordClicked();
  QCheckBox * FExternalSessionInExistingInstance = nullptr;
  CoreBridge::TTransferPresetRule FCurrentRuleData;
  bool FHasCurrentRuleData = false;
  QRadioButton * FQueueView[3] = {};
public:
  // The Background page's queue list choice (Options > Queue's).
  int QueueViewChoice() const;
private:
  QCheckBox * FPreservePanelState = nullptr;
  QCheckBox * FFullRowSelect = nullptr;
  QComboBox * FPanelSearch = nullptr;
  QComboBox * FDoubleClickAction = nullptr;
  QCheckBox * FCopyOnDoubleClickConfirmation = nullptr;
  QCheckBox * FPanelFontCheck = nullptr;
  class QLabel * FPanelFontLabel = nullptr;
  QFont FPanelFont;
  QComboBox * FExplorerSelection = nullptr;
  QCheckBox * FCompareByTime = nullptr;
  QCheckBox * FCompareBySize = nullptr;
  QComboBox * FKeyboardShortcuts = nullptr;
  QCheckBox * FUseLocationProfiles = nullptr;
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
  QCheckBox * FLogging = nullptr;
  QComboBox * FLogLevel = nullptr;
  class QLineEdit * FLogFile = nullptr;
  QRadioButton * FLogAppend = nullptr;
  QRadioButton * FLogOverwrite = nullptr;
  QCheckBox * FLogSensitive = nullptr;
  QCheckBox * FLogMaxSize = nullptr;
  QComboBox * FLogMaxSizeCombo = nullptr;
  QCheckBox * FLogMaxCount = nullptr;
  QSpinBox * FLogMaxCountSpin = nullptr;
  QCheckBox * FLogActions = nullptr;
  QLineEdit * FActionsLogFile = nullptr;
  QRadioButton * FRetrieveIp = nullptr;
  QRadioButton * FCustomIp = nullptr;
  QLineEdit * FCustomIpEdit = nullptr;
  QCheckBox * FPortLimit = nullptr;
  QSpinBox * FPortMin = nullptr;
  QSpinBox * FPortMax = nullptr;
  QCheckBox * FTryFtp = nullptr;
  QCheckBox * FRememberPassword = nullptr;
  QComboBox * FAuthAgent = nullptr;
  CoreBridge::TPreferences::TStorage FStorage = CoreBridge::TPreferences::TStorage::AutomaticIni;
  QLabel * FStorageLabel = nullptr;
  QRadioButton * FRegistryStorage = nullptr;
  QRadioButton * FAutomaticIniStorage = nullptr;
  QRadioButton * FCustomIniStorage = nullptr;
  QLineEdit * FCustomIniEdit = nullptr;
  QRadioButton * FSystemTemporary = nullptr;
  QRadioButton * FCustomTemporary = nullptr;
  QLineEdit * FTemporaryEdit = nullptr;
  QLineEdit * FRandomSeedEdit = nullptr;
  CoreBridge::TEditorSettings FEditor;
  QCheckBox * FEditorCheckNotModified = nullptr;
  std::vector<CoreBridge::TEditorDefinition> FEditors;
  QTreeWidget * FEditorsList = nullptr;
  QPushButton * FEditEditor = nullptr;
  QPushButton * FRemoveEditor = nullptr;
  QPushButton * FUpEditor = nullptr;
  QPushButton * FDownEditor = nullptr;
  QCheckBox * FEditorWordWrap = nullptr;
  QSpinBox * FEditorTabSize = nullptr;
  QComboBox * FEditorEncoding = nullptr;
  QLabel * FEditorFontLabel = nullptr;
  QCheckBox * FEditorWarnLargeFile = nullptr;
  QSpinBox * FEditorLargeFileSize = nullptr;
  QLineEdit * FPuttyPath = nullptr;
  QCheckBox * FPuttyPassword = nullptr;
  QCheckBox * FAutoOpenInPutty = nullptr;
  QCheckBox * FTelnetForFtpInPutty = nullptr;
  QComboBox * FPuttyRegistryStorageKey = nullptr;
  QLabel * FPuttyRegistryStorageKeyLabel = nullptr;
  std::vector<CoreBridge::TCustomCommandDefinition> FCommands;
  QTreeWidget * FCommandsList = nullptr;
  QPushButton * FEditCommand = nullptr;
  QPushButton * FConfigureCommand = nullptr;
  class QToolButton * FAddCommand = nullptr;
  std::unique_ptr<CoreBridge::TExtensionsEdit> FExtensions;
  std::string FSessionKey;
  // A row's extension's index; -1 for a custom command's.
  int ExtensionIndex(int Row) const;
  QPushButton * FRemoveCommand = nullptr;
  QPushButton * FUpCommand = nullptr;
  QPushButton * FDownCommand = nullptr;
  // The pages exist: their controls can be updated.
  bool FBuilt = false;
};

#endif
