// Synchronization's dialogs, as WinSCP's: the Synchronize dialog
// (forms/FullSynchronize.cpp) choosing the directories, the direction, the
// mode, the options and the comparison criteria; the progress window while
// the directories are compared (forms/SynchronizeProgress.cpp); and the
// checklist of what was found (forms/SynchronizeChecklist.cpp), whose entries
// can be unchecked or reversed before they are applied, here or in the queue.
#ifndef LINSCP_QT_SYNCHRONIZE_DIALOG_H
#define LINSCP_QT_SYNCHRONIZE_DIALOG_H

#include <QDialog>

#include <functional>

#include "CoreBridge.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;

class SynchronizeDialog : public QDialog
{
  Q_OBJECT

public:
  // CanTimestamps: whether the server can set times ("synchronize
  // timestamps" is disabled otherwise).
  // CanChecksum: comparing by checksum is possible; AllowSelectedOnly: the
  // panels have a selection; SaveMode: the direction is kept with the rest
  // (SynchronizeModeAuto), so is not asked about.
  SynchronizeDialog(const QString & LocalDirectory, const QString & RemoteDirectory,
    const CoreBridge::TSyncParams & Params, bool CanTimestamps, QWidget * Parent = nullptr,
    bool CanChecksum = false, bool AllowSelectedOnly = false, bool SaveMode = false);
  // "Use same options next time", and the direction too (asked about on OK
  // when changed, as FormCloseQuery).
  bool SaveSettings() const;
  bool SaveMode() const { return FSaveMode; }
  void accept() override;

  QString LocalDirectory() const;
  QString RemoteDirectory() const;
  CoreBridge::TSyncParams Params() const;
  // The transfer settings group (WinSCP's "Transfer settings").
  class TransferSettingsBox * TransferSettings() const { return FTransferSettings; }

  // The controls, for tests.
  QRadioButton * BothButton() const { return FBoth; }
  QRadioButton * RemoteButton() const { return FRemote; }
  QRadioButton * LocalButton() const { return FLocal; }
  QRadioButton * FilesButton() const { return FFiles; }
  QRadioButton * MirrorButton() const { return FMirror; }
  QRadioButton * TimestampsButton() const { return FTimestamps; }
  QCheckBox * DeleteCheck() const { return FDelete; }
  QCheckBox * ExistingOnlyCheck() const { return FExistingOnly; }
  QCheckBox * PreviewCheck() const { return FPreview; }
  QCheckBox * ByTimeCheck() const { return FByTime; }
  QCheckBox * BySizeCheck() const { return FBySize; }
  QPushButton * OkButton() const { return FOk; }
  QCheckBox * SelectedOnlyCheck() const { return FSelectedOnly; }
  QCheckBox * ByChecksumCheck() const { return FByChecksum; }
  QCheckBox * SaveSettingsCheck() const { return FSaveSettings; }

private:
  void UpdateControls();

  bool FCanTimestamps;
  bool FCanChecksum = false;
  bool FSaveMode = false;
  CoreBridge::TSyncParams::TDirection FOrigDirection = CoreBridge::TSyncParams::TDirection::Remote;
  QCheckBox * FSelectedOnly = nullptr;
  QCheckBox * FByChecksum = nullptr;
  QCheckBox * FSaveSettings = nullptr;
  QLineEdit * FLocalEdit = nullptr;
  QLineEdit * FRemoteEdit = nullptr;
  QRadioButton * FBoth = nullptr;
  QRadioButton * FRemote = nullptr;
  QRadioButton * FLocal = nullptr;
  QRadioButton * FFiles = nullptr;
  QRadioButton * FMirror = nullptr;
  QRadioButton * FTimestamps = nullptr;
  QCheckBox * FDelete = nullptr;
  QCheckBox * FExistingOnly = nullptr;
  QCheckBox * FPreview = nullptr;
  QCheckBox * FByTime = nullptr;
  QCheckBox * FBySize = nullptr;
  QCheckBox * FCaseSensitive = nullptr;
  QPushButton * FOk = nullptr;
  class TransferSettingsBox * FTransferSettings = nullptr;
  QString FBySizeCaption;
};

// Shown while the directories are compared: which pair, and Cancel.
class SynchronizeProgress : public QDialog
{
  Q_OBJECT

public:
  explicit SynchronizeProgress(QWidget * Parent = nullptr);
  // Core's directory callback: false once cancelled.
  bool Directory(const QString & Local, const QString & Remote);
  bool Cancelled() const { return FCancelled; }
  QPushButton * CancelButton() const { return FCancel; }

private:
  QLabel * FLocal = nullptr;
  QLabel * FRemote = nullptr;
  QPushButton * FCancel = nullptr;
  bool FCancelled = false;
};

class SyncChecklistDialog : public QDialog
{
  Q_OBJECT

public:
  // Started: applied here; Queued: "start in background".
  enum TResult { Cancelled, Started, Queued };

  // Session: the Tools menu's Calculate and Move (none: not offered).
  SyncChecklistDialog(CoreBridge::TSyncChecklist & Checklist, const QString & LocalDirectory,
    const QString & RemoteDirectory, QWidget * Parent = nullptr, CoreBridge::TSession * Session = nullptr);
  TResult Result() const { return FResult; }

  // The buttons' actions, for tests and them alike.
  void CheckAll(bool Check);
  // The selected entries' actions reversed.
  void ReverseSelected();
  // ListViewPopupMenu's: the selection checked or not; every action under
  // the selection's directories checked or not (CheckDirectory); the
  // selection's local or remote paths, a line each, to the clipboard
  // (PathToClipboard).
  void CheckSelected(bool Check);
  void CheckDirectory(bool Check);
  QStringList SelectedPaths(bool Local) const;
  // The list's popup, as it opens (for tests).
  class QMenu * PopupMenu();
  // The Tools menu's (WinSCP's): the selected (All: every) directory's size;
  // the selection moved instead of transferred and deleted; the next pair
  // that could be, selected.
  bool CalculateSizes(bool All);
  bool MoveSelected();
  bool FindMoveCandidate();
  // The entries selected, by the checklist's index.
  std::vector<std::size_t> SelectedIndexes() const;
  void SelectIndexes(const std::vector<std::size_t> & Indexes);
  QTreeWidget * List() const { return FList; }
  QLabel * Status() const { return FStatus; }
  QPushButton * StartButton() const { return FStart; }
  QPushButton * QueueButton() const { return FQueue; }

  // As CustomCommandsAction (FOnCustomCommandMenu): the "diff"-style
  // local commands, on each selected pair of files (both sides there); the
  // window runs them (Run: the command, local and remote full paths).
  using TRunCustomCommand = std::function<void(const CoreBridge::TCustomCommandDefinition & Command,
    const std::vector<std::pair<std::string, std::string>> & Pairs)>;
  void SetCustomCommandRunner(TRunCustomCommand Run);
  class QPushButton * CustomCommandsButton() const { return FCustomCommands; }
  bool RunCustomCommand(const CoreBridge::TCustomCommandDefinition & Command);

private:
  QPushButton * FCustomCommands = nullptr;
  TRunCustomCommand FRunCustomCommand;
  void FillCustomCommands(class QMenu * Menu);
  void Load(int Index);
  void Reload();
  void UpdateTools();
  void ShowError(const CoreBridge::TCoreError & E);
  void ItemChanged(QTreeWidgetItem * Item, int Column);
  void UpdateStatus();

  CoreBridge::TSyncChecklist & FChecklist;
  CoreBridge::TSession * FSession = nullptr;
  class QAction * FCalculateAction = nullptr;
  QAction * FCalculateAllAction = nullptr;
  QAction * FMoveAction = nullptr;
  QAction * FFindMoveAction = nullptr;
  QTreeWidget * FList = nullptr;
  QLabel * FStatus = nullptr;
  QPushButton * FStart = nullptr;
  QPushButton * FQueue = nullptr;
  QPushButton * FReverse = nullptr;
  TResult FResult = Cancelled;
  bool FLoading = false;
};

#endif
