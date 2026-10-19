// Help > About: the program, its version, what it runs on, where it comes
// from (WinSCP, which it ports), its authors and licence (the License
// dialog), and the third-party code it includes, as WinSCP's About lists it.
#ifndef LINSCP_QT_ABOUT_DIALOG_H
#define LINSCP_QT_ABOUT_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QLabel;
class QPushButton;
class QTextBrowser;

class AboutDialog : public QDialog
{
  Q_OBJECT

public:
  explicit AboutDialog(const CoreBridge::TInfo & Info, QWidget * Parent = nullptr);

  // Its texts, for tests.
  QLabel * NameLabel() const { return FName; }
  QLabel * VersionLabel() const { return FVersion; }
  QTextBrowser * ThirdParty() const { return FThirdParty; }
  QPushButton * LicenseButton() const { return FLicense; }

private:
  QLabel * FName = nullptr;
  QLabel * FVersion = nullptr;
  QTextBrowser * FThirdParty = nullptr;
  QPushButton * FLicense = nullptr;
};

#endif
