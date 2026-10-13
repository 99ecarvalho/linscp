// The shell's main window: a session, opened from the site manager or a
// URL, and its remote panel; the local panel and the queue (P2-07, P2-08)
// join it.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include <memory>

#include "CoreBridge.h"

class QAction;
class QLineEdit;
class RemotePanel;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);
  ~MainWindow() override;

  // Connects to Url, as File > Open URL does. False when it did not.
  bool OpenUrl(const QString & Url);
  // Connects to a site, saved or as edited in the site manager.
  bool OpenSite(const CoreBridge::TSite & Site);
  // File > Site Manager: false when it was closed without logging in.
  bool SiteManagerDialog();
  CoreBridge::TSession * Session() const { return FSession.get(); }
  RemotePanel * Panel() const { return FPanel; }

private slots:
  void OpenUrlDialog();
  void Disconnect();
  void About();

private:
  bool Open(std::unique_ptr<CoreBridge::TSession> Session);
  void ShowError(const CoreBridge::TCoreError & E);
  void UpdateActions();

  CoreBridge::TInfo FInfo;
  std::unique_ptr<CoreBridge::TSession> FSession;
  bool FBusy = false;
  QAction * FOpenAction = nullptr;
  QAction * FSitesAction = nullptr;
  QAction * FDisconnectAction = nullptr;
  RemotePanel * FPanel = nullptr;
};

#endif
