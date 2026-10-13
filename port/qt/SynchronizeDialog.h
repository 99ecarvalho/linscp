// Synchronization's dialogs, as WinSCP's: the Synchronize dialog
// (forms/FullSynchronize.cpp) choosing the directories, the direction, the
// mode, the options and the comparison criteria; the progress window while
// the directories are compared (forms/SynchronizeProgress.cpp); and the
// checklist of what was found (forms/SynchronizeChecklist.cpp), whose entries
// can be unchecked or reversed before they are applied, here or in the queue.
#ifndef LINSCP_QT_SYNCHRONIZE_DIALOG_H
#define LINSCP_QT_SYNCHRONIZE_DIALOG_H

#include <QDialog>

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
  SynchronizeDialog(const QString & LocalDirectory, const QString & RemoteDirectory,
    const CoreBridge::TSyncParams & Params, bool CanTimestamps, QWidget * Parent = nullptr);

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

private:
  void UpdateControls();

  bool FCanTimestamps;
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

  SyncChecklistDialog(CoreBridge::TSyncChecklist & Checklist, const QString & LocalDirectory,
    const QString & RemoteDirectory, QWidget * Parent = nullptr);
  TResult Result() const { return FResult; }

  // The buttons' actions, for tests and them alike.
  void CheckAll(bool Check);
  // The selected entries' actions reversed.
  void ReverseSelected();
  QTreeWidget * List() const { return FList; }
  QLabel * Status() const { return FStatus; }
  QPushButton * StartButton() const { return FStart; }
  QPushButton * QueueButton() const { return FQueue; }

private:
  void Load(int Index);
  void ItemChanged(QTreeWidgetItem * Item, int Column);
  void UpdateStatus();

  CoreBridge::TSyncChecklist & FChecklist;
  QTreeWidget * FList = nullptr;
  QLabel * FStatus = nullptr;
  QPushButton * FStart = nullptr;
  QPushButton * FQueue = nullptr;
  QPushButton * FReverse = nullptr;
  TResult FResult = Cancelled;
  bool FLoading = false;
};

#endif
