// The shell's side of the bridge: core's questions as Qt dialogs, its
// progress lines in the status bar, its busy states as the wait cursor.
#ifndef LINSCP_QT_QT_FRONTEND_H
#define LINSCP_QT_QT_FRONTEND_H

#include <QPointer>

#include <functional>
#include <memory>

#include "CoreBridge.h"

class QWidget;
class TransferDialog;

class QtFrontend : public CoreBridge::IFrontend
{
public:
  QtFrontend();
  ~QtFrontend() override;

  // The window dialogs belong to, once there is one.
  void SetWindow(QWidget * Window) { FWindow = Window; }
  // Where progress lines go (the main window's status bar).
  void SetStatus(std::function<void(const QString &)> Status) { FStatus = std::move(Status); }

  CoreBridge::TQueryResult Query(const CoreBridge::TQuery & Query) override;
  bool Prompt(CoreBridge::TPrompt & Prompt) override;
  void ShowError(const CoreBridge::TError & Error) override;
  void Banner(const std::string & SessionName, const std::string & Banner,
    bool CanNeverShowAgain, bool & NeverShowAgain) override;
  void Information(const std::string & Text) override;
  void TransferStarted(bool Upload, int FileCount) override;
  bool TransferProgress(const CoreBridge::TTransferProgress & Progress) override;
  void TransferFinished() override;
  // The progress window of the transfer under way, if any: for tests.
  TransferDialog * Transfer() const { return FTransfer.get(); }
  void BusyStart() override;
  void BusyEnd() override;
  void ProcessEvents() override;
  void SetClipboard(const std::string & Text) override;
  bool GetClipboard(std::string & Text) override;

private:
  QPointer<QWidget> FWindow;
  std::function<void(const QString &)> FStatus;
  std::unique_ptr<TransferDialog> FTransfer;
};

#endif
