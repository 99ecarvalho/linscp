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
#include <optional>

#include <functional>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
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
  // Save As: asks for a name (the site's own for a stored site) even for a
  // stored site when Force.
  void SaveAsDialog(bool Force);
  // Set Defaults: the form as the new sites' settings, after asking unless
  // Confirm is false. False when it did not save.
  bool SetDefaults(bool Confirm = true);
  QToolButton * SaveButton() const { return FSave; }
  QToolButton * LoginDropDown() const { return FLoginDropDown; }
  // As ShowAgainPanel, when shown by itself: "Show Login dialog on startup
  // and when the last session is closed", unchecked only after asking.
  QCheckBox * ShowAgainCheck() const { return FShowAgain; }
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
  // FTP's "Anonymous login"; S3's "Credentials from AWS environment" and
  // its profile (upstream's BasicFtpPanel, BasicS3Panel).
  QCheckBox * AnonymousLogin() const { return FAnonymous; }
  QCheckBox * S3CredentialsEnv() const { return FS3CredentialsEnv; }
  QComboBox * S3Profile() const { return FS3Profile; }
  // The site with the form applied: what Site() returns, and what Advanced
  // edits.
  CoreBridge::TSite Edited() const;
  void SetEdited(const CoreBridge::TSite & Site);

  // Manage's menu, and the tree's context menu (ManageSitePopupMenu,
  // ManageFolderPopupMenu, ManageNewSitePopupMenu,
  // ManageWorkspacePopupMenu): the item under the cursor's actions,
  // filled into Menu.
  void FillManageMenu(QMenu * Menu);
  QPushButton * ManageButton() const { return FManage; }
  // Clone to New Site: the selected site's settings in the new site, its
  // name kept (made unique) for Save.
  void CloneToNewSite();
  // As ParseUrl: Url into the form, after asking about unsafe settings;
  // false when not.
  bool ParseUrl(const QString & Url);
  // Paste Session URL: the clipboard's URL into the new site (the form,
  // for the new site already selected).
  void PasteUrl();
  // As HostNameEditExit: a URL in the host name parsed into the form.
  void ParseHostName();
  // Rename (F2): the selected site's, folder's or workspace's label edited
  // in place; renamed when the edit ends.
  void RenameInPlace();
  QGroupBox * NoteGroup() const { return FNoteGroup; }
  // A12, drag & drop in the tree (SessionTreeDragDrop): a site into the
  // folder of the item it is dropped on (the top level for none);
  // DropFolder: that folder, empty for the top level, null when it cannot
  // go there (a workspace, or where it is already).
  std::optional<QString> DropFolder(const QString & Site, const QTreeWidgetItem * Target) const;
  bool MoveSite(const QString & Site, const QString & Folder);
  // A10, the site search (SitesIncrementalSearch): Text from the item
  // under the cursor (after it when SkipCurrent; backwards when Reverse),
  // in collapsed folders too, but first in what shows unless Expanding;
  // false when nothing matches. As the tree searches what is typed in it.
  bool SearchSites(const QString & Text, bool SkipCurrent = false, bool Reverse = false, bool Expanding = false);
  // Find Site (Ctrl+F): the search started, nothing typed yet.
  void StartSiteSearch();
  void ResetSiteSearch();
  bool SiteSearching() const { return FSearching; }
  QLabel * SiteSearchStatus() const { return FSearchStatus; }
  // Search > Options: what the search matches (LoginDialog.SiteSearch).
  void SetSiteSearch(CoreBridge::TPreferences::TPanelSearch Search);
  // A7, Advanced's Transfer Settings Rule (CopyParamRuleAction): the preset
  // whose rule matches the site's host and user (else its host) edited,
  // else one added with that rule. False when cancelled.
  bool TransferSettingsRule();
  // Reset: the new site from the defaults.
  void ResetNewSite();
  // Generate Session URL/Code, for the site as edited.
  void GenerateUrl();
  // Open in PuTTY, the site as edited.
  bool OpenInPutty();

  // What the main window gives: Tools > Preferences (and Advanced's Global
  // Preferences > Logging, its page), Check for Updates, About; not in the
  // menus without.
  std::function<void(const QString & Page)> OnPreferences;
  std::function<void()> OnCheckForUpdates;
  std::function<void()> OnAbout;
  // After Import/Restore Configuration: this application started again
  // (with its /ini), this one quit; tests replace it.
  std::function<void()> RestartApplication;
  // Tools > Run Pageant, Run PuTTYgen, as FindTool: next to linscp, in its
  // PuTTY folder, or on the PATH; empty when none.
  static QString FindTool(const QString & Name);
  bool RunTool(const QString & Name);
  // Tools > Export/Backup Configuration (to FileName, else asked) and
  // Import/Restore Configuration (after asking, unless FileName is given).
  bool ExportConfiguration(const QString * FileName = nullptr);
  bool ImportConfiguration(const QString * FileName = nullptr);
  QMenu * ToolsMenu() const { return FToolsMenu; }

  // Tools > Desktop Icon, and Explorer's 'Send To' Shortcut (SendTo,
  // Windows): the selected site's, folder's or workspace's, after asking.
  bool CreateSessionShortcut(bool SendTo);

private:
  class QAction * FDesktopIconAction = nullptr;
  QAction * FSendToAction = nullptr;
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
  QLabel * FUserNameLabel = nullptr;
  QLabel * FPasswordLabel = nullptr;
  class QCheckBox * FAnonymous = nullptr;
  class QCheckBox * FS3CredentialsEnv = nullptr;
  class QComboBox * FS3Profile = nullptr;
  void UpdateS3Credentials();
  void UpdateCredentials();
  QPushButton * FLogin = nullptr;
  QToolButton * FSave = nullptr;
  QPushButton * FRename = nullptr;
  QPushButton * FDelete = nullptr;
  QPushButton * FNewFolder = nullptr;
  QPushButton * FManage = nullptr;
  QToolButton * FLoginDropDown = nullptr;
  QCheckBox * FShowAgain = nullptr;
  QMenu * FToolsMenu = nullptr;
  bool eventFilter(QObject * Watched, QEvent * Event) override;
  class QTreeWidgetItem * SearchSite(const QString & Text, bool AllowExpanding, bool SkipCurrent, bool Reverse);
  bool SiteMatches(const QTreeWidgetItem * Item, const QString & Text) const;
  void UpdateSiteSearch();
  QLabel * FSearchStatus = nullptr;
  bool FSearching = false;
  bool FSearchHaveNext = false;
  bool FIncrementalSearching = false;
  QString FSearchText;
  CoreBridge::TPreferences::TPanelSearch FSiteSearch = CoreBridge::TPreferences::TPanelSearch::AnyPart;
  void AskNewFolder();
  // A13, rename in place: the label edited, then renamed as it says.
  void Renamed(QTreeWidgetItem * Item);
  QTreeWidgetItem * FRenameItem = nullptr;
  QString FRenameText;
  // A15: the site's note, read-only (NoteGroup).
  QGroupBox * FNoteGroup = nullptr;
  QPlainTextEdit * FNote = nullptr;
  // The new site's name from Clone to New Site, for Save; empty otherwise.
  QString FNewSiteName;
  QToolButton * FAdvanced = nullptr;
  // The site under the cursor, with what the form does not show.
  CoreBridge::TSite FSite;
  // Folders made in this dialog, with no site in them yet.
  QStringList FNewFolders;
  // The port the form's protocol and encryption default to, to follow it.
  int FDefaultPort = 0;
  bool FLoading = false;
};

#endif
