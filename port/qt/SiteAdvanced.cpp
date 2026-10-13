#include "SiteAdvanced.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{

using TProxy = CoreBridge::TSite::TProxy;
using TProtocol = CoreBridge::TSite::TProtocol;

}  // namespace

SiteAdvanced::SiteAdvanced(const CoreBridge::TSite & Site, QWidget * Parent) :
  QDialog(Parent),
  FSite(Site)
{
  setWindowTitle(tr("Advanced Site Settings"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FPages = new QTabWidget(this);
  Layout->addWidget(FPages);
  const bool Ssh = (Site.Protocol == TProtocol::Sftp) || (Site.Protocol == TProtocol::Scp);

  // Environment > Directories.
  QWidget * Directories = new QWidget(FPages);
  QFormLayout * DirectoriesForm = new QFormLayout(Directories);
  FRemoteDirectory = new QLineEdit(QString::fromStdString(Site.RemoteDirectory), Directories);
  DirectoriesForm->addRow(tr("Remote &directory:"), FRemoteDirectory);
  FLocalDirectory = new QLineEdit(QString::fromStdString(Site.LocalDirectory), Directories);
  QPushButton * BrowseLocal = new QPushButton(tr("&Browse..."), Directories);
  connect(BrowseLocal, &QPushButton::clicked, this, [this]()
  {
    const QString Directory = QFileDialog::getExistingDirectory(this, tr("Local directory"), FLocalDirectory->text());
    if (!Directory.isEmpty())
    {
      FLocalDirectory->setText(QDir::toNativeSeparators(Directory));
    }
  });
  QHBoxLayout * LocalRow = new QHBoxLayout();
  LocalRow->addWidget(FLocalDirectory, 1);
  LocalRow->addWidget(BrowseLocal);
  DirectoriesForm->addRow(tr("&Local directory:"), LocalRow);
  FUpdateDirectories = new QCheckBox(tr("Re&member last used directory"), Directories);
  FUpdateDirectories->setChecked(Site.UpdateDirectories);
  DirectoriesForm->addRow(FUpdateDirectories);
  FPages->addTab(Directories, tr("Directories"));

  // Connection.
  QWidget * Connection = new QWidget(FPages);
  QFormLayout * ConnectionForm = new QFormLayout(Connection);
  FTimeout = new QSpinBox(Connection);
  FTimeout->setRange(1, 6000);
  FTimeout->setSuffix(tr(" seconds"));
  FTimeout->setValue(Site.Timeout);
  ConnectionForm->addRow(tr("Server &response timeout:"), FTimeout);
  FFtpPassive = new QCheckBox(tr("&Passive mode"), Connection);
  FFtpPassive->setChecked(Site.FtpPassive);
  FFtpPassive->setEnabled(Site.Protocol == TProtocol::Ftp);
  ConnectionForm->addRow(FFtpPassive);
  FPages->addTab(Connection, tr("Connection"));

  // Connection > Proxy.
  QWidget * ProxyPage = new QWidget(FPages);
  QFormLayout * ProxyForm = new QFormLayout(ProxyPage);
  FProxy = new QComboBox(ProxyPage);
  FProxy->addItem(tr("None"), static_cast<int>(TProxy::None));
  FProxy->addItem(QStringLiteral("SOCKS4"), static_cast<int>(TProxy::Socks4));
  FProxy->addItem(QStringLiteral("SOCKS5"), static_cast<int>(TProxy::Socks5));
  FProxy->addItem(QStringLiteral("HTTP"), static_cast<int>(TProxy::Http));
  FProxy->setCurrentIndex(FProxy->findData(static_cast<int>(Site.Proxy)));
  ProxyForm->addRow(tr("Proxy &type:"), FProxy);
  FProxyHost = new QLineEdit(QString::fromStdString(Site.ProxyHost), ProxyPage);
  ProxyForm->addRow(tr("Pro&xy host name:"), FProxyHost);
  FProxyPort = new QSpinBox(ProxyPage);
  FProxyPort->setRange(1, 65535);
  // As the Proxy page: the site's port, whatever the type (core's default 80).
  FProxyPort->setValue((Site.ProxyPort > 0) ? Site.ProxyPort : 80);
  ProxyForm->addRow(tr("Po&rt number:"), FProxyPort);
  FProxyUserName = new QLineEdit(QString::fromStdString(Site.ProxyUserName), ProxyPage);
  ProxyForm->addRow(tr("&User name:"), FProxyUserName);
  FProxyPassword = new QLineEdit(QString::fromStdString(Site.ProxyPassword), ProxyPage);
  FProxyPassword->setEchoMode(QLineEdit::Password);
  ProxyForm->addRow(tr("&Password:"), FProxyPassword);
  FProxyLocalhost = new QCheckBox(tr("Consult proxy for &local addresses"), ProxyPage);
  FProxyLocalhost->setChecked(Site.ProxyLocalhost);
  ProxyForm->addRow(FProxyLocalhost);
  connect(FProxy, &QComboBox::currentIndexChanged, this, &SiteAdvanced::ProxyChanged);
  FPages->addTab(ProxyPage, tr("Proxy"));

  // SSH > Authentication.
  QWidget * Authentication = new QWidget(FPages);
  QFormLayout * AuthenticationForm = new QFormLayout(Authentication);
  FTryAgent = new QCheckBox(tr("Attempt authentication using &Pageant or ssh-agent"), Authentication);
  FTryAgent->setChecked(Site.TryAgent);
  AuthenticationForm->addRow(FTryAgent);
  FKeyboardInteractive = new QCheckBox(tr("Attempt '&keyboard-interactive' authentication"), Authentication);
  FKeyboardInteractive->setChecked(Site.KeyboardInteractive);
  AuthenticationForm->addRow(FKeyboardInteractive);
  FAgentForwarding = new QCheckBox(tr("Allow agent &forwarding"), Authentication);
  FAgentForwarding->setChecked(Site.AgentForwarding);
  AuthenticationForm->addRow(FAgentForwarding);
  FPrivateKeyFile = new QLineEdit(QString::fromStdString(Site.PrivateKeyFile), Authentication);
  QPushButton * BrowseKey = new QPushButton(tr("B&rowse..."), Authentication);
  connect(BrowseKey, &QPushButton::clicked, this, [this]()
  {
    const QString FileName = QFileDialog::getOpenFileName(this, tr("Private key file"), FPrivateKeyFile->text(),
      tr("PuTTY Private Key Files (*.ppk);;All Private Key Files (*.ppk *.pem *.key id_*);;All Files (*)"));
    if (!FileName.isEmpty())
    {
      FPrivateKeyFile->setText(QDir::toNativeSeparators(FileName));
    }
  });
  QHBoxLayout * KeyRow = new QHBoxLayout();
  KeyRow->addWidget(FPrivateKeyFile, 1);
  KeyRow->addWidget(BrowseKey);
  AuthenticationForm->addRow(tr("Private &key file:"), KeyRow);
  const int AuthenticationIndex = FPages->addTab(Authentication, tr("SSH Authentication"));
  FPages->setTabEnabled(AuthenticationIndex, Ssh);

  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
  ProxyChanged();
}

// As the Proxy page: the proxy's fields only with a proxy.
void SiteAdvanced::ProxyChanged()
{
  const bool Any = (static_cast<TProxy>(FProxy->currentData().toInt()) != TProxy::None);
  for (QWidget * Field : std::initializer_list<QWidget *>{ FProxyHost, FProxyPort, FProxyUserName, FProxyPassword, FProxyLocalhost })
  {
    Field->setEnabled(Any);
  }
}

CoreBridge::TSite SiteAdvanced::Site() const
{
  CoreBridge::TSite Site = FSite;
  Site.RemoteDirectory = FRemoteDirectory->text().trimmed().toStdString();
  Site.LocalDirectory = FLocalDirectory->text().trimmed().toStdString();
  Site.UpdateDirectories = FUpdateDirectories->isChecked();
  Site.Timeout = FTimeout->value();
  Site.FtpPassive = FFtpPassive->isChecked();
  Site.Proxy = static_cast<TProxy>(FProxy->currentData().toInt());
  Site.ProxyHost = FProxyHost->text().trimmed().toStdString();
  Site.ProxyPort = FProxyPort->value();
  Site.ProxyUserName = FProxyUserName->text().toStdString();
  Site.ProxyPassword = FProxyPassword->text().toStdString();
  Site.ProxyLocalhost = FProxyLocalhost->isChecked();
  Site.PrivateKeyFile = FPrivateKeyFile->text().trimmed().toStdString();
  Site.TryAgent = FTryAgent->isChecked();
  Site.KeyboardInteractive = FKeyboardInteractive->isChecked();
  Site.AgentForwarding = FAgentForwarding->isChecked();
  return Site;
}
