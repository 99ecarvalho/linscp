// A transfer's progress, as upstream's Progress window shows it
// (forms/Progress.cpp): the operation, the file and how far along it is, the
// whole operation's progress, bytes and speed, and Cancel. Shown modal to its
// window while core transfers on the GUI thread, which keeps it alive
// through ProcessGUI.
#ifndef LINSCP_QT_TRANSFER_DIALOG_H
#define LINSCP_QT_TRANSFER_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QLabel;
class QProgressBar;
class QPushButton;

class TransferDialog : public QDialog
{
  Q_OBJECT

public:
  TransferDialog(bool Upload, int FileCount, QWidget * Parent = nullptr);

  void Update(const CoreBridge::TTransferProgress & Progress);
  bool Cancelled() const { return FCancelled; }
  QPushButton * CancelButton() const { return FCancel; }

  void reject() override;

private:
  QLabel * FFile = nullptr;
  QProgressBar * FFileBar = nullptr;
  QProgressBar * FOverallBar = nullptr;
  QLabel * FBytes = nullptr;
  QLabel * FSpeed = nullptr;
  QPushButton * FCancel = nullptr;
  bool FCancelled = false;
};

#endif
