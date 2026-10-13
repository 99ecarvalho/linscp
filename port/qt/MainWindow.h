// The shell's main window, in WinSCP's Commander layout: the local panel
// and, beside it, the remote panel of the session opened from the site
// manager or a URL. Transfers between them and the queue are P2-07's.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include <memory>

#include "CoreBridge.h"

class QAction;
class QLineEdit;
class LocalPanel;
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
  // Transfers between the panels, as WinSCP's Commander's F5 (copy) and F6
  // (move) from the active panel to the other's directory, after asking for
  // the target unless Confirm is false. False when nothing was transferred
  // in full.
  bool Upload(const QStringList & Names, const QString & Target, bool Move);
  bool Download(const QStringList & Names, const QString & Target, bool Move);
  bool TransferSelected(bool Move, bool Confirm = true);
  // The panel transfers start from: the one the focus last went to.
  void ActivatePanel(bool Local);
  bool LocalActive() const { return FLocalActive; }
  CoreBridge::TSession * Session() const { return FSession.get(); }
  RemotePanel * Panel() const { return FPanel; }
  LocalPanel * Local() const { return FLocal; }

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
  bool FLocalActive = false;
  QAction * FCopyAction = nullptr;
  QAction * FMoveAction = nullptr;
  QAction * FDisconnectAction = nullptr;
  RemotePanel * FPanel = nullptr;
  LocalPanel * FLocal = nullptr;
};

#endif
