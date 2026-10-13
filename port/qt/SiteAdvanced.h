// A site's advanced settings, the pages of upstream's Site Advanced dialog
// (forms/SiteAdvanced.cpp) the shell offers: Directories, Connection, Proxy,
// and SSH authentication.
#ifndef LINSCP_QT_SITE_ADVANCED_H
#define LINSCP_QT_SITE_ADVANCED_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;

class SiteAdvanced : public QDialog
{
  Q_OBJECT

public:
  SiteAdvanced(const CoreBridge::TSite & Site, QWidget * Parent = nullptr);
  // After an accepted exec(): the site with the settings changed.
  CoreBridge::TSite Site() const;

  QTabWidget * Pages() const { return FPages; }
  QLineEdit * LocalDirectory() const { return FLocalDirectory; }
  QSpinBox * Timeout() const { return FTimeout; }
  QCheckBox * FtpPassive() const { return FFtpPassive; }
  QCheckBox * UpdateDirectories() const { return FUpdateDirectories; }
  QComboBox * Proxy() const { return FProxy; }
  QLineEdit * ProxyHost() const { return FProxyHost; }
  QSpinBox * ProxyPort() const { return FProxyPort; }
  QLineEdit * ProxyUserName() const { return FProxyUserName; }
  QLineEdit * ProxyPassword() const { return FProxyPassword; }
  QCheckBox * ProxyLocalhost() const { return FProxyLocalhost; }
  QLineEdit * PrivateKeyFile() const { return FPrivateKeyFile; }
  QCheckBox * TryAgent() const { return FTryAgent; }
  QCheckBox * KeyboardInteractive() const { return FKeyboardInteractive; }
  QCheckBox * AgentForwarding() const { return FAgentForwarding; }

private:
  void ProxyChanged();

  CoreBridge::TSite FSite;
  QTabWidget * FPages = nullptr;
  QLineEdit * FRemoteDirectory = nullptr;
  QLineEdit * FLocalDirectory = nullptr;
  QSpinBox * FTimeout = nullptr;
  QCheckBox * FFtpPassive = nullptr;
  QCheckBox * FUpdateDirectories = nullptr;
  QComboBox * FProxy = nullptr;
  QLineEdit * FProxyHost = nullptr;
  QSpinBox * FProxyPort = nullptr;
  QLineEdit * FProxyUserName = nullptr;
  QLineEdit * FProxyPassword = nullptr;
  QCheckBox * FProxyLocalhost = nullptr;
  QLineEdit * FPrivateKeyFile = nullptr;
  QCheckBox * FTryAgent = nullptr;
  QCheckBox * FKeyboardInteractive = nullptr;
  QCheckBox * FAgentForwarding = nullptr;
};

#endif
