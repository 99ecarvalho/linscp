// Commands > Keep Remote Directory up to Date, as WinSCP's
// (forms/Synchronize.cpp, TSynchronizeDialog): the two directories, the
// options and the transfer settings; then, started, the log of what the
// watch found and did, until Stop. The watching and synchronizing are
// upstream's TSynchronizeController's, through the bridge.
#ifndef LINSCP_QT_KEEP_UP_TO_DATE_DIALOG_H
#define LINSCP_QT_KEEP_UP_TO_DATE_DIALOG_H

#include <QDialog>

#include <string>
#include <vector>

#include "CoreBridge.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class TransferSettingsBox;

class KeepUpToDateDialog : public QDialog
{
  Q_OBJECT

public:
  // Selected: the names selected in the panels, which "selected files only"
  // limits the watch to (disabled when there are none).
  KeepUpToDateDialog(CoreBridge::TSession & Session, const CoreBridge::TKeepUpToDateParams & Params,
    const std::vector<std::string> & Selected, QWidget * Parent = nullptr);
  ~KeepUpToDateDialog() override;

  CoreBridge::TKeepUpToDateParams Params() const;
  bool SaveSettings() const;
  bool Synchronizing() const { return FSynchronizing; }

  // The Start and Stop buttons' actions (TSynchronizeDialog's Start and
  // Stop): Start asks first when "synchronize on start" is grayed; false
  // when it did not start.
  bool Start();
  void Stop();

  // The controls, for tests.
  QLineEdit * LocalEdit() const { return FLocalEdit; }
  QLineEdit * RemoteEdit() const { return FRemoteEdit; }
  QCheckBox * DeleteCheck() const { return FDelete; }
  QCheckBox * ExistingOnlyCheck() const { return FExistingOnly; }
  QCheckBox * RecurseCheck() const { return FRecurse; }
  QCheckBox * SynchronizeCheck() const { return FSynchronize; }
  QCheckBox * SelectedOnlyCheck() const { return FSelectedOnly; }
  QCheckBox * ContinueOnErrorCheck() const { return FContinueOnError; }
  QCheckBox * SaveSettingsCheck() const { return FSaveSettings; }
  QPushButton * StartButton() const { return FStart; }
  QPushButton * StopButton() const { return FStop; }
  QPushButton * CloseButton() const { return FClose; }
  QTreeWidget * Log() const { return FLog; }
  TransferSettingsBox * TransferSettings() const { return FTransferSettings; }

protected:
  void reject() override;

private:
  void UpdateControls();
  void AddLog(CoreBridge::TKeepUpToDateLog Entry, const QString & Message, const QString & MoreMessages);
  void Abort(bool Close);
  void Idle();
  void LogDoubleClicked(QTreeWidgetItem * Item);

  CoreBridge::TSession & FSession;
  std::vector<std::string> FSelected;
  bool FSynchronizing = false;
  QLineEdit * FLocalEdit = nullptr;
  QLineEdit * FRemoteEdit = nullptr;
  QCheckBox * FDelete = nullptr;
  QCheckBox * FExistingOnly = nullptr;
  QCheckBox * FRecurse = nullptr;
  QCheckBox * FSynchronize = nullptr;
  QCheckBox * FSelectedOnly = nullptr;
  QCheckBox * FContinueOnError = nullptr;
  QCheckBox * FSaveSettings = nullptr;
  QWidget * FDirectories = nullptr;
  QWidget * FOptions = nullptr;
  TransferSettingsBox * FTransferSettings = nullptr;
  QPushButton * FStart = nullptr;
  QPushButton * FStop = nullptr;
  QPushButton * FMinimize = nullptr;
  QPushButton * FClose = nullptr;
  QTreeWidget * FLog = nullptr;
  QTimer * FTimer = nullptr;
  bool FInIdle = false;
};

#endif
