// The shell's main window: a session opened from a URL and its current
// remote directory -- what P2-03 needs to exercise core's questions -- in the
// frame the site manager, file panels and queue (P2-04 to P2-08) fill in.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include <memory>

#include "CoreBridge.h"

class QAction;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);
  ~MainWindow() override;

  // Connects to Url, as File > Open URL does. False when it did not.
  bool OpenUrl(const QString & Url);
  CoreBridge::TSession * Session() const { return FSession.get(); }

private slots:
  void OpenUrlDialog();
  void Disconnect();
  void About();
  void ItemActivated(QTreeWidgetItem * Item, int Column);

private:
  void ShowError(const CoreBridge::TCoreError & E);
  void Refresh();
  void UpdateActions();

  CoreBridge::TInfo FInfo;
  std::unique_ptr<CoreBridge::TSession> FSession;
  bool FBusy = false;
  QAction * FOpenAction = nullptr;
  QAction * FDisconnectAction = nullptr;
  QLineEdit * FPath = nullptr;
  QTreeWidget * FFiles = nullptr;
};

#endif
