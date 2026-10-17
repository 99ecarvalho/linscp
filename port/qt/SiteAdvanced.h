// A site's advanced settings, as upstream's Site Advanced dialog
// (forms/SiteAdvanced.cpp): its tree of pages, those of the site's
// protocol shown -- Environment (Directories, Recycle bin, Encryption,
// SFTP, SCP/Shell, FTP, S3, WebDAV), Connection (Proxy, Tunnel, TLS/SSL),
// SSH (Key exchange, Authentication, Bugs), Note -- and the site's color.
#ifndef LINSCP_QT_SITE_ADVANCED_H
#define LINSCP_QT_SITE_ADVANCED_H

#include <QDialog>

#include <vector>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

class SiteAdvanced : public QDialog
{
  Q_OBJECT

public:
  SiteAdvanced(const CoreBridge::TSite & Site, QWidget * Parent = nullptr);
  // After an accepted exec(): the site with the settings changed.
  CoreBridge::TSite Site() const;

  // The pages the tree shows, in its order; one shown by its name.
  int PageCount() const;
  QString PageName(int Index) const;
  bool ShowPage(const QString & Name);
  void ShowPageAt(int Index);
  QString CurrentPage() const;

  QLineEdit * RemoteDirectory() const { return FRemoteDirectory; }
  QLineEdit * LocalDirectory() const { return FLocalDirectory; }
  QCheckBox * UpdateDirectories() const { return FUpdateDirectories; }
  QCheckBox * CacheDirectoryChanges() const { return FCacheDirectoryChanges; }
  QCheckBox * PreserveDirectoryChanges() const { return FPreserveDirectoryChanges; }
  QComboBox * EolType() const { return FEolType; }
  QComboBox * Utf() const { return FUtf; }
  QSpinBox * TimeDifferenceHours() const { return FTimeDifferenceHours; }
  QSpinBox * TimeDifferenceMinutes() const { return FTimeDifferenceMinutes; }
  QCheckBox * DeleteToRecycleBin() const { return FDeleteToRecycleBin; }
  QCheckBox * OverwrittenToRecycleBin() const { return FOverwrittenToRecycleBin; }
  QLineEdit * RecycleBinPath() const { return FRecycleBinPath; }
  QCheckBox * EncryptFiles() const { return FEncryptFiles; }
  QLineEdit * EncryptKey() const { return FEncryptKey; }
  QComboBox * SftpServer() const { return FSftpServer; }
  QCheckBox * AllowScpFallback() const { return FAllowScpFallback; }
  QComboBox * SftpMaxVersion() const { return FSftpMaxVersion; }
  QComboBox * Shell() const { return FShell; }
  QComboBox * ReturnVar() const { return FReturnVar; }
  QLineEdit * FtpAccount() const { return FFtpAccount; }
  QComboBox * FtpUseMlsd() const { return FFtpUseMlsd; }
  QComboBox * FtpListAll() const { return FFtpListAll; }
  QComboBox * S3DefaultRegion() const { return FS3DefaultRegion; }
  QComboBox * S3UrlStyle() const { return FS3UrlStyle; }
  QCheckBox * WebDavLiberalEscaping() const { return FWebDavLiberalEscaping; }
  QSpinBox * Timeout() const { return FTimeout; }
  QCheckBox * FtpPassive() const { return FFtpPassive; }
  QRadioButton * PingNullPacket() const { return FPingNullPacket; }
  QSpinBox * PingInterval() const { return FPingInterval; }
  QRadioButton * IPv6() const { return FIPv6; }
  QComboBox * Proxy() const { return FProxy; }
  QLineEdit * ProxyHost() const { return FProxyHost; }
  QSpinBox * ProxyPort() const { return FProxyPort; }
  QLineEdit * ProxyUserName() const { return FProxyUserName; }
  QLineEdit * ProxyPassword() const { return FProxyPassword; }
  QLineEdit * ProxyLocalCommand() const { return FProxyLocalCommand; }
  QLineEdit * ProxyTelnetCommand() const { return FProxyTelnetCommand; }
  QCheckBox * ProxyLocalhost() const { return FProxyLocalhost; }
  QCheckBox * Tunnel() const { return FTunnel; }
  QLineEdit * TunnelHostName() const { return FTunnelHostName; }
  QComboBox * TunnelLocalPort() const { return FTunnelLocalPort; }
  QComboBox * MinTls() const { return FMinTls; }
  QComboBox * MaxTls() const { return FMaxTls; }
  QCheckBox * Compression() const { return FCompression; }
  QListWidget * Ciphers() const { return FCiphers; }
  QPushButton * CipherUp() const { return FCipherUp; }
  QListWidget * Kex() const { return FKex; }
  QComboBox * BugRekey() const { return FBugs[4]; }
  QGroupBox * KexReexchange() const { return FKexReexchange; }
  QCheckBox * SshNoUserAuth() const { return FSshNoUserAuth; }
  QCheckBox * TryAgent() const { return FTryAgent; }
  QCheckBox * KeyboardInteractive() const { return FKeyboardInteractive; }
  QCheckBox * KeyboardInteractivePassword() const { return FKeyboardInteractivePassword; }
  QCheckBox * AgentForwarding() const { return FAgentForwarding; }
  QLineEdit * PrivateKeyFile() const { return FPrivateKeyFile; }
  QCheckBox * AuthGSSAPI() const { return FAuthGSSAPI; }
  QCheckBox * GSSAPIFwdTGT() const { return FGSSAPIFwdTGT; }
  QPlainTextEdit * Note() const { return FNote; }
  QPushButton * ColorButton() const { return FColorButton; }
  int Color() const { return FColor; }
  void SetColor(int Color);

  void accept() override;

private:
  enum TPage
  {
    EnvironmentPage, DirectoriesPage, RecycleBinPage, EncryptionPage, SftpPage, ScpPage, FtpPage, S3Page,
    WebDavPage, ConnectionPage, ProxyPage, TunnelPage, TlsPage, SshPage, KexPage, AuthenticationPage,
    BugsPage, NotePage, PageTotal
  };
  QWidget * AddPage(TPage Page, const QString & Name, bool Subpage);
  QWidget * MakeEnvironmentPage();
  QWidget * MakeDirectoriesPage();
  QWidget * MakeRecycleBinPage();
  QWidget * MakeEncryptionPage();
  QWidget * MakeSftpPage();
  QWidget * MakeScpPage();
  QWidget * MakeFtpPage();
  QWidget * MakeS3Page();
  QWidget * MakeWebDavPage();
  QWidget * MakeConnectionPage();
  QWidget * MakeProxyPage();
  QWidget * MakeTunnelPage();
  QWidget * MakeTlsPage();
  QWidget * MakeSshPage();
  QWidget * MakeKexPage();
  QWidget * MakeAuthenticationPage();
  QWidget * MakeBugsPage();
  QWidget * MakeNotePage();
  QComboBox * AutoSwitchCombo(QWidget * Parent, CoreBridge::TSite::TAutoSwitch Value);
  QWidget * BrowseRow(QLineEdit * Edit, const QString & Caption, bool Directory, const QString & Filter = QString());
  QListWidget * AlgorithmList(QWidget * Parent, const std::vector<std::string> & Names, const std::vector<int> & Order);
  void UpdateNavigation();
  void UpdateControls();
  // The proxy method of the combo the protocol uses (GetProxyMethod), and
  // FTP's logon type (GetFtpProxyLogonType).
  CoreBridge::TSite::TProxy ProxyMethod() const;
  int FtpProxyLogonType() const;
  void ProxyAutodetect();
  void FillColorMenu();
  void ShowPublicKey();
  void GenerateEncryptKey();
  bool ValidEncryptKey();
  void ShowError(const CoreBridge::TCoreError & E);

  CoreBridge::TSite FSite;
  bool FSsh = false;
  bool FSftp = false;
  bool FScp = false;
  bool FFtp = false;
  bool FWebDav = false;
  bool FS3 = false;
  bool FNeon = false;
  bool FSsl = false;
  bool FUpdating = false;
  int FColor = 0;
  QTreeWidget * FNavigation = nullptr;
  QStackedWidget * FPages = nullptr;
  QTreeWidgetItem * FPageItems[PageTotal] = {};
  bool FPageShown[PageTotal] = {};
  // Directories.
  QLineEdit * FRemoteDirectory = nullptr;
  QLineEdit * FLocalDirectory = nullptr;
  QCheckBox * FUpdateDirectories = nullptr;
  QCheckBox * FSynchronizeBrowsing = nullptr;
  QCheckBox * FCacheDirectories = nullptr;
  QCheckBox * FCacheDirectoryChanges = nullptr;
  QCheckBox * FPreserveDirectoryChanges = nullptr;
  QCheckBox * FResolveSymlinks = nullptr;
  QCheckBox * FFollowDirectorySymlinks = nullptr;
  // Environment.
  QComboBox * FEolType = nullptr;
  QComboBox * FUtf = nullptr;
  QSpinBox * FTimeDifferenceHours = nullptr;
  QSpinBox * FTimeDifferenceMinutes = nullptr;
  QCheckBox * FTimeDifferenceAuto = nullptr;
  QCheckBox * FTrimVMSVersions = nullptr;
  QGroupBox * FDstGroup = nullptr;
  QRadioButton * FDstUnix = nullptr;
  QRadioButton * FDstWindows = nullptr;
  // Recycle bin.
  QCheckBox * FDeleteToRecycleBin = nullptr;
  QCheckBox * FOverwrittenToRecycleBin = nullptr;
  QLineEdit * FRecycleBinPath = nullptr;
  // Encryption.
  QCheckBox * FEncryptFiles = nullptr;
  QGroupBox * FEncryptGroup = nullptr;
  QLineEdit * FEncryptKey = nullptr;
  QCheckBox * FShowEncryptKey = nullptr;
  // SFTP.
  QComboBox * FSftpServer = nullptr;
  QCheckBox * FAllowScpFallback = nullptr;
  QComboBox * FSftpMaxVersion = nullptr;
  QComboBox * FSftpRealPath = nullptr;
  QCheckBox * FUsePosixRename = nullptr;
  QComboBox * FSftpBugSymlink = nullptr;
  QComboBox * FSftpBugSignedTS = nullptr;
  // SCP/Shell.
  QComboBox * FShell = nullptr;
  QComboBox * FReturnVar = nullptr;
  QGroupBox * FLsGroup = nullptr;
  QComboBox * FListingCommand = nullptr;
  QCheckBox * FIgnoreLsWarnings = nullptr;
  QCheckBox * FScpLsFullTime = nullptr;
  QGroupBox * FOtherShellGroup = nullptr;
  QCheckBox * FLookupUserGroups = nullptr;
  QCheckBox * FClearAliases = nullptr;
  QCheckBox * FUnsetNationalVars = nullptr;
  QCheckBox * FScp1Compatibility = nullptr;
  // FTP.
  QPlainTextEdit * FPostLoginCommands = nullptr;
  QLineEdit * FFtpAccount = nullptr;
  QComboBox * FFtpListAll = nullptr;
  QComboBox * FFtpUseMlsd = nullptr;
  QComboBox * FFtpForcePasvIp = nullptr;
  QComboBox * FFtpHost = nullptr;
  QCheckBox * FVMSAllRevisions = nullptr;
  // S3.
  QComboBox * FS3DefaultRegion = nullptr;
  QComboBox * FS3UrlStyle = nullptr;
  QCheckBox * FS3RequesterPays = nullptr;
  QPlainTextEdit * FS3SessionToken = nullptr;
  QLineEdit * FS3RoleArn = nullptr;
  CoreBridge::TS3Credentials FS3Credentials;
  // WebDAV.
  QCheckBox * FWebDavLiberalEscaping = nullptr;
  QCheckBox * FWebDavCrossDomainRedirects = nullptr;
  // Connection.
  QCheckBox * FFtpPassive = nullptr;
  QCheckBox * FOptimizeBuffers = nullptr;
  QSpinBox * FTimeout = nullptr;
  QGroupBox * FPingGroup = nullptr;
  QRadioButton * FPingOff = nullptr;
  QRadioButton * FPingNullPacket = nullptr;
  QRadioButton * FPingDummyCommand = nullptr;
  QSpinBox * FPingInterval = nullptr;
  QGroupBox * FFtpPingGroup = nullptr;
  QRadioButton * FFtpPingOff = nullptr;
  QRadioButton * FFtpPingDummyCommand = nullptr;
  QRadioButton * FFtpPingDirectoryListing = nullptr;
  QSpinBox * FFtpPingInterval = nullptr;
  QGroupBox * FIpGroup = nullptr;
  QRadioButton * FIPAuto = nullptr;
  QRadioButton * FIPv4 = nullptr;
  QRadioButton * FIPv6 = nullptr;
  // Proxy: the combo of the protocol's methods (SSH's, FTP's or neon's).
  QComboBox * FProxy = nullptr;
  QLineEdit * FProxyHost = nullptr;
  QSpinBox * FProxyPort = nullptr;
  QLineEdit * FProxyUserName = nullptr;
  QLineEdit * FProxyPassword = nullptr;
  QGroupBox * FProxySettings = nullptr;
  QLineEdit * FProxyTelnetCommand = nullptr;
  QLineEdit * FProxyLocalCommand = nullptr;
  QWidget * FProxyTelnetRow = nullptr;
  QWidget * FProxyLocalRow = nullptr;
  QLabel * FProxyCommandLabel = nullptr;
  QComboBox * FProxyDns = nullptr;
  QCheckBox * FProxyLocalhost = nullptr;
  // Tunnel.
  QCheckBox * FTunnel = nullptr;
  QGroupBox * FTunnelSession = nullptr;
  QGroupBox * FTunnelOptions = nullptr;
  QGroupBox * FTunnelAuthentication = nullptr;
  QLineEdit * FTunnelHostName = nullptr;
  QSpinBox * FTunnelPortNumber = nullptr;
  QLineEdit * FTunnelUserName = nullptr;
  QLineEdit * FTunnelPassword = nullptr;
  QLineEdit * FTunnelPrivateKeyFile = nullptr;
  QComboBox * FTunnelLocalPort = nullptr;
  // TLS/SSL.
  QComboBox * FMinTls = nullptr;
  QComboBox * FMaxTls = nullptr;
  QCheckBox * FSslSessionReuse = nullptr;
  QGroupBox * FTlsAuthentication = nullptr;
  QLineEdit * FTlsCertificateFile = nullptr;
  // SSH.
  QCheckBox * FCompression = nullptr;
  QListWidget * FCiphers = nullptr;
  QPushButton * FCipherUp = nullptr;
  QPushButton * FCipherDown = nullptr;
  QCheckBox * FSsh2DES = nullptr;
  // Key exchange.
  QListWidget * FKex = nullptr;
  QPushButton * FKexUp = nullptr;
  QPushButton * FKexDown = nullptr;
  QCheckBox * FAuthGSSAPIKEX = nullptr;
  QGroupBox * FKexReexchange = nullptr;
  QSpinBox * FRekeyTime = nullptr;
  QLineEdit * FRekeyData = nullptr;
  // Authentication.
  QCheckBox * FSshNoUserAuth = nullptr;
  QGroupBox * FAuthenticationGroup = nullptr;
  QCheckBox * FTryAgent = nullptr;
  QCheckBox * FKeyboardInteractive = nullptr;
  QCheckBox * FKeyboardInteractivePassword = nullptr;
  QGroupBox * FAuthenticationParams = nullptr;
  QCheckBox * FAgentForwarding = nullptr;
  QLineEdit * FPrivateKeyFile = nullptr;
  QPushButton * FShowPublicKey = nullptr;
  QLineEdit * FDetachedCertificate = nullptr;
  QWidget * FDetachedCertificateRow = nullptr;
  QLabel * FDetachedCertificateLabel = nullptr;
  QString FLastPrivateKey;
  bool FKeyHasCertificate = false;
  QCheckBox * FAuthGSSAPI = nullptr;
  QCheckBox * FGSSAPIFwdTGT = nullptr;
  // Bugs.
  QComboBox * FBugs[CoreBridge::TSite::SshBugCount] = {};
  // Note.
  QPlainTextEdit * FNote = nullptr;
  QPushButton * FColorButton = nullptr;
};

#endif
