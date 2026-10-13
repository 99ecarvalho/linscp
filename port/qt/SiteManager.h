// The site manager: core's stored sites in a tree, and the site under the
// cursor in a form -- upstream's Login dialog (forms/Login.cpp), the part of
// it P2-04 needs: protocol, encryption, server, credentials, remote
// directory; new, save, save as, delete, login. Advanced settings are P2-05.
#ifndef LINSCP_QT_SITE_MANAGER_H
#define LINSCP_QT_SITE_MANAGER_H

#include <QDialog>

#include "CoreBridge.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

class SiteManager : public QDialog
{
  Q_OBJECT

public:
  explicit SiteManager(QWidget * Parent = nullptr);

  // After an accepted exec(): the site to log in to, as edited.
  CoreBridge::TSite Site() const;

  // What the buttons do, for tests and the buttons alike.
  bool SelectSite(const QString & Name);
  void SelectNewSite();
  // Saves the form as Name (Folder/Site), the password only if SavePassword;
  // asks before overwriting another site. False when it did not save.
  bool SaveAs(const QString & Name, bool SavePassword);
  // Saves the selected site in place, or asks for a name for a new one.
  void Save();
  // Deletes the selected site, after asking unless Confirm is false.
  bool DeleteSelected(bool Confirm = true);

  QTreeWidget * Tree() const { return FTree; }
  QComboBox * Protocol() const { return FProtocol; }
  QComboBox * Encryption() const { return FEncryption; }
  QLineEdit * HostName() const { return FHostName; }
  QSpinBox * Port() const { return FPort; }
  QLineEdit * UserName() const { return FUserName; }
  QLineEdit * Password() const { return FPassword; }
  QLineEdit * RemoteDirectory() const { return FRemoteDirectory; }

private:
  void Fill(const QString & Select);
  void CurrentChanged();
  void ProtocolChanged();
  void Load(const CoreBridge::TSite & Site);
  CoreBridge::TSite FromForm() const;
  QString SelectedName() const;
  void ShowError(const CoreBridge::TCoreError & E);
  void UpdateButtons();

  QTreeWidget * FTree = nullptr;
  QComboBox * FProtocol = nullptr;
  QLabel * FEncryptionLabel = nullptr;
  QComboBox * FEncryption = nullptr;
  QLineEdit * FHostName = nullptr;
  QSpinBox * FPort = nullptr;
  QLineEdit * FUserName = nullptr;
  QLineEdit * FPassword = nullptr;
  QLineEdit * FRemoteDirectory = nullptr;
  QPushButton * FLogin = nullptr;
  QPushButton * FSave = nullptr;
  QPushButton * FDelete = nullptr;
  // The port the form's protocol and encryption default to, to follow it.
  int FDefaultPort = 0;
  bool FLoading = false;
};

#endif
