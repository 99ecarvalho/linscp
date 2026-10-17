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
  // As HandleErrorList: what "Continue on error" collected, if anything,
  // offered and then gone through one by one.
  void ShowErrorList(const std::vector<CoreBridge::TError> & Errors);
  void Banner(const std::string & SessionName, const std::string & Banner,
    bool CanNeverShowAgain, bool & NeverShowAgain, bool & MonospacedFont) override;
  void Information(const std::string & Text) override;
  void TransferStarted(bool Upload, int FileCount) override;
  bool TransferProgress(const CoreBridge::TTransferProgress & Progress) override;
  CoreBridge::TTransferAnswer TransferControl(const CoreBridge::TTransferProgress & Progress) override;
  void TransferCompleted() override;
  void TransferFinished() override;
  // Where a completed transfer's "Once finished" goes (the main window
  // runs it as the queue's once-empty action): TransferDialog::TOnceFinished.
  void SetOnceFinished(std::function<void(int)> OnceFinished) { FOnceFinished = std::move(OnceFinished); }
  // The progress window of the transfer under way, if any: for tests.
  TransferDialog * Transfer() const { return FTransfer.get(); }
  void BusyStart() override;
  void BusyEnd() override;
  void ProcessEvents() override;
  void SetClipboard(const std::string & Text) override;
  bool GetClipboard(std::string & Text) override;
  bool StartProgram(const std::string & Program, const std::string & Params, const std::string & Directory) override;
  // Programs started go here instead (tests).
  void SetStartProgram(std::function<bool(const QString & Program, const QString & Params, const QString & Directory)> Start)
  {
    FStartProgram = std::move(Start);
  }

private:
  QPointer<QWidget> FWindow;
  std::function<void(const QString &)> FStatus;
  std::function<bool(const QString &, const QString &, const QString &)> FStartProgram;
  std::unique_ptr<TransferDialog> FTransfer;
  std::function<void(int)> FOnceFinished;
};

#endif
