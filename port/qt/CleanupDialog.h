// Cleanup application data, as WinSCP's (forms/Cleanup.cpp, behind the
// site manager's Tools): what the program keeps and where, each to check
// for removal (CoreBridge::TCleanupData).
#ifndef LINSCP_QT_CLEANUP_DIALOG_H
#define LINSCP_QT_CLEANUP_DIALOG_H

#include <QDialog>

#include <memory>

#include "CoreBridge.h"

class QPushButton;
class QTreeWidget;

class CleanupDialog : public QDialog
{
  Q_OBJECT

public:
  // Throws TCoreError, as gathering the data does.
  explicit CleanupDialog(QWidget * Parent = nullptr);
  ~CleanupDialog() override;

  // As TCleanupDialog::Execute, once accepted: the checked items removed,
  // each one's error shown. False when cancelled.
  bool Execute();

  // The controls, for tests.
  QTreeWidget * List() const { return FList; }
  QPushButton * OkButton() const { return FOk; }
  QPushButton * CheckAllButton() const { return FCheckAll; }

private:
  void UpdateControls();

  std::unique_ptr<CoreBridge::TCleanupData> FData;
  QTreeWidget * FList = nullptr;
  QPushButton * FOk = nullptr;
  QPushButton * FCheckAll = nullptr;
};

#endif
