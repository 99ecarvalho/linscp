// A transfer's progress, as upstream's Progress window shows it
// (forms/Progress.cpp): the operation, the file and how far along it is, the
// whole operation's progress, bytes, speed and times; and its toolbar:
// Cancel, Skip this file, Minimize, Proceed in Background, Once Finished,
// the speed limit. Shown modal to its window while core transfers on the
// GUI thread, which keeps it alive through ProcessGUI.
#ifndef LINSCP_QT_TRANSFER_DIALOG_H
#define LINSCP_QT_TRANSFER_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QToolButton;

class TransferDialog : public QDialog
{
  Q_OBJECT

public:
  // Once Finished's choices, as MainWindow's TOnceEmpty.
  enum class TOnceFinished { Idle, Disconnect, Suspend, ShutDown };

  TransferDialog(bool Upload, int FileCount, QWidget * Parent = nullptr);

  void Update(const CoreBridge::TTransferProgress & Progress);
  // The answer to the report just shown: what the user asked since the
  // last one (a skip asked once), and the speed limit chosen.
  CoreBridge::TTransferAnswer Answer();
  bool Cancelled() const { return FCancelled; }
  TOnceFinished OnceFinished() const { return FOnceFinished; }
  void SetOnceFinished(TOnceFinished OnceFinished);

  QPushButton * CancelButton() const { return FCancel; }
  QPushButton * SkipButton() const { return FSkip; }
  QPushButton * MinimizeButton() const { return FMinimize; }
  QPushButton * BackgroundButton() const { return FBackground; }
  QToolButton * OnceFinishedButton() const { return FOnceFinishedButton; }
  QComboBox * SpeedLimit() const { return FSpeedLimit; }
  QLabel * TimeLeft() const { return FTimeLeft; }
  QLabel * TimeElapsed() const { return FTimeElapsed; }
  QLabel * StartTime() const { return FStartTime; }

  void reject() override;

private:
  void SpeedLimitEntered();

  QLabel * FFile = nullptr;
  QProgressBar * FFileBar = nullptr;
  QProgressBar * FOverallBar = nullptr;
  QLabel * FBytes = nullptr;
  QLabel * FSpeed = nullptr;
  QLabel * FTimeLeftLabel = nullptr;
  QLabel * FTimeLeft = nullptr;
  QLabel * FStartTimeLabel = nullptr;
  QLabel * FStartTime = nullptr;
  QLabel * FTimeElapsed = nullptr;
  QPushButton * FCancel = nullptr;
  QPushButton * FSkip = nullptr;
  QPushButton * FMinimize = nullptr;
  QPushButton * FBackground = nullptr;
  QToolButton * FOnceFinishedButton = nullptr;
  QComboBox * FSpeedLimit = nullptr;
  bool FCancelled = false;
  bool FSkipAsked = false;
  bool FBackgroundAsked = false;
  bool FSpeedLimitSet = false;
  bool FSpeedLimitShown = false;
  unsigned long FSpeedLimitValue = 0;
  TOnceFinished FOnceFinished = TOnceFinished::Idle;
};

#endif
