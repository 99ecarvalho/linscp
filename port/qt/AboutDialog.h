// Help > About: the program, its version, what it runs on, where it comes
// from (WinSCP, which it ports), its authors and licence.
#ifndef LINSCP_QT_ABOUT_DIALOG_H
#define LINSCP_QT_ABOUT_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QLabel;

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);

  // Its texts, for tests.
  QLabel * NameLabel() const { return FName; }
  QLabel * VersionLabel() const { return FVersion; }
  QLabel * WinScpLabel() const { return FWinScp; }

private:
  QLabel * FName = nullptr;
  QLabel * FVersion = nullptr;
  QLabel * FWinScp = nullptr;
};

#endif
