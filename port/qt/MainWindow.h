// The shell's main window. P2-02's skeleton: what core it runs on, and the
// frame the site manager, file panels and queue (P2-04 to P2-08) fill in.
#ifndef LINSCP_QT_MAIN_WINDOW_H
#define LINSCP_QT_MAIN_WINDOW_H

#include <QMainWindow>

#include "CoreBridge.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);

private slots:
  void About();

private:
  CoreBridge::TInfo FInfo;
};

#endif
