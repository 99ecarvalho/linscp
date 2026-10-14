// The site manager: core's stored sites in a tree, and the site under the
// cursor in a form -- upstream's Login dialog (forms/Login.cpp): protocol,
// encryption, server, credentials; the advanced settings in their own dialog
// (SiteAdvanced); new site, save, save as, rename, delete, new folder; the
// private key checked and offered for conversion on login and save, as
// upstream does.
#ifndef LINSCP_QT_SITE_MANAGER_H
#define LINSCP_QT_SITE_MANAGER_H

#include <QDialog>

#include <functional>

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
  // After an accepted exec() on a workspace: its name (empty for a site).
  QString Workspace() const;

  // What the buttons do, for tests and the buttons alike.
  bool SelectSite(const QString & Name);
  bool SelectFolder(const QString & Path);
  bool SelectWorkspace(const QString & Name);
  void SelectNewSite();
  // Saves the form as Name (Folder/Site), the password only if SavePassword;
  // asks before overwriting another site. False when it did not save.
  bool SaveAs(const QString & Name, bool SavePassword);
  // Saves the selected site in place, or asks for a name for a new one.
  void Save();
  // Renames the selected site or folder (a folder: every site in it).
  bool RenameSelected(const QString & NewName);
  // A folder under the selected one, in the tree until a site is saved in it.
  void NewFolder(const QString & Name);
  // Deletes the selected site, folder or workspace, after asking unless
  // Confirm is false.
  bool DeleteSelected(bool Confirm = true);
  // Edits the advanced settings in their dialog.
  void Advanced();
  // Advanced's other choice, Edit Raw Settings (TSiteRawDialog). Raw: the
  // options as edited, given (else the dialog asks); false when cancelled
  // or they did not apply.
  bool RawSettings(const QString * Raw = nullptr);
  // Tools > Import Sites: false when cancelled.
  bool ImportSites();
  // Tools > Clean Up... (CleanupDialog); the sites reloaded after.
  bool Cleanup();
  // Checks the private key as upstream does before logging in or saving, and
  // offers converting an OpenSSH one. False to stop.
  bool VerifyKey();

  // Where a converted key is saved: asks the user by default.
  std::function<QString(const QString & Suggested)> ChooseConvertedKeyFile;

  void accept() override;

  QTreeWidget * Tree() const { return FTree; }
  QComboBox * Protocol() const { return FProtocol; }
  QComboBox * Encryption() const { return FEncryption; }
  QLineEdit * HostName() const { return FHostName; }
  QSpinBox * Port() const { return FPort; }
  QLineEdit * UserName() const { return FUserName; }
  QLineEdit * Password() const { return FPassword; }
  // The site with the form applied: what Site() returns, and what Advanced
  // edits.
  CoreBridge::TSite Edited() const;
  void SetEdited(const CoreBridge::TSite & Site);

private:
  void Fill(const QString & Select, const QStringList & ExtraFolders = QStringList());
  void CurrentChanged();
  void ProtocolChanged();
  void Load(const CoreBridge::TSite & Site);
  QString SelectedName() const;
  QString SelectedFolder() const;
  QString SelectedWorkspace() const;
  QStringList SitesIn(const QString & Folder) const;
  void ShowError(const CoreBridge::TCoreError & E);
  void ShowMessage(CoreBridge::TQueryType Type, const QString & Message);
  void UpdateButtons();

  QTreeWidget * FTree = nullptr;
  QComboBox * FProtocol = nullptr;
  QLabel * FEncryptionLabel = nullptr;
  QComboBox * FEncryption = nullptr;
  QLineEdit * FHostName = nullptr;
  QSpinBox * FPort = nullptr;
  QLineEdit * FUserName = nullptr;
  QLineEdit * FPassword = nullptr;
  QPushButton * FLogin = nullptr;
  QPushButton * FSave = nullptr;
  QPushButton * FRename = nullptr;
  QPushButton * FDelete = nullptr;
  QPushButton * FNewFolder = nullptr;
  class QToolButton * FAdvanced = nullptr;
  // The site under the cursor, with what the form does not show.
  CoreBridge::TSite FSite;
  // Folders made in this dialog, with no site in them yet.
  QStringList FNewFolders;
  // The port the form's protocol and encryption default to, to follow it.
  int FDefaultPort = 0;
  bool FLoading = false;
};

#endif
