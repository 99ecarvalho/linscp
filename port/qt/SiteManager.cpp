#include "SiteManager.h"
#include <QTreeWidgetItemIterator>
#include "SaveSiteDialog.h"
#include "Integration.h"
#include "KeyVerify.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "QtFrontend.h"
#include "SiteAdvanced.h"
#include "SiteRawDialog.h"
#include "ImportSitesDialog.h"
#include "CleanupDialog.h"
#include "CustomCommandDialog.h"
#include "GenerateUrlDialog.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QToolButton>

namespace
{

using TProtocol = CoreBridge::TSite::TProtocol;
using TEncryption = CoreBridge::TSite::TEncryption;

// Tree items: a site carries its full name, a folder its path, a workspace
// its name; "New Site" neither.
enum { NameRole = Qt::UserRole, KindRole };
enum TKind { NewSiteKind, FolderKind, SiteKind, WorkspaceKind };

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

// Core's texts with their "%s" (and "%d") filled in.
QString CoreText(CoreBridge::TText Text, const QString & Arg, int Number = 0)
{
  return CoreText(Text).replace(QStringLiteral("%s"), Arg).replace(QStringLiteral("%d"), QString::number(Number));
}

}  // namespace

SiteManager::SiteManager(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Login"));
  resize(780, 460);
  ChooseConvertedKeyFile = [this](const QString & Suggested) { return ::ChooseConvertedKeyFile(this, Suggested); };

  QVBoxLayout * Layout = new QVBoxLayout(this);
  QSplitter * Splitter = new QSplitter(this);
  Layout->addWidget(Splitter, 1);

  QWidget * TreePanel = new QWidget(Splitter);
  QVBoxLayout * TreeLayout = new QVBoxLayout(TreePanel);
  TreeLayout->setContentsMargins(0, 0, 0, 0);
  FTree = new QTreeWidget(TreePanel);
  FTree->setAccessibleName(tr("Sites"));
  FTree->setHeaderHidden(true);
  connect(FTree, &QTreeWidget::currentItemChanged, this, &SiteManager::CurrentChanged);
  connect(FTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem * Item)
  {
    if (Item->data(0, KindRole).toInt() != FolderKind)
    {
      accept();
    }
  });
  TreeLayout->addWidget(FTree, 1);
  // As upstream's: the item's menu, on the tree and its Manage button.
  FTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(FTree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint & Pos)
  {
    QTreeWidgetItem * Item = FTree->itemAt(Pos);
    if (Item != nullptr)
    {
      FTree->setCurrentItem(Item);
    }
    QMenu Menu(this);
    FillManageMenu(&Menu);
    Menu.exec(FTree->viewport()->mapToGlobal(Pos));
  });
  QHBoxLayout * TreeButtons = new QHBoxLayout();
  FManage = new QPushButton(tr("&Manage"), TreePanel);
  QMenu * ManageMenu = new QMenu(FManage);
  connect(ManageMenu, &QMenu::aboutToShow, this, [this, ManageMenu]()
  {
    ManageMenu->clear();
    FillManageMenu(ManageMenu);
  });
  FManage->setMenu(ManageMenu);
  TreeButtons->addWidget(FManage);
  FNewFolder = new QPushButton(tr("Ne&w Folder..."), TreePanel);
  FRename = new QPushButton(tr("Re&name..."), TreePanel);
  FDelete = new QPushButton(tr("&Delete"), TreePanel);
  TreeButtons->addWidget(FNewFolder);
  TreeButtons->addWidget(FRename);
  TreeButtons->addWidget(FDelete);
  TreeLayout->addLayout(TreeButtons);
  connect(FNewFolder, &QPushButton::clicked, this, &SiteManager::AskNewFolder);
  connect(FRename, &QPushButton::clicked, this, &SiteManager::AskRename);
  connect(FDelete, &QPushButton::clicked, this, [this]() { DeleteSelected(true); });

  QWidget * FormPanel = new QWidget(Splitter);
  QVBoxLayout * FormLayout = new QVBoxLayout(FormPanel);
  QFormLayout * Form = new QFormLayout();
  FProtocol = new QComboBox(FormPanel);
  FProtocol->addItem(QStringLiteral("SFTP"), static_cast<int>(TProtocol::Sftp));
  FProtocol->addItem(QStringLiteral("SCP"), static_cast<int>(TProtocol::Scp));
  FProtocol->addItem(QStringLiteral("FTP"), static_cast<int>(TProtocol::Ftp));
  FProtocol->addItem(QStringLiteral("WebDAV"), static_cast<int>(TProtocol::WebDav));
  FProtocol->addItem(QStringLiteral("Amazon S3"), static_cast<int>(TProtocol::S3));
  Form->addRow(tr("&File protocol:"), FProtocol);
  FEncryption = new QComboBox(FormPanel);
  FEncryptionLabel = new QLabel(tr("&Encryption:"), FormPanel);
  FEncryptionLabel->setBuddy(FEncryption);
  Form->addRow(FEncryptionLabel, FEncryption);
  FHostName = new QLineEdit(FormPanel);
  Form->addRow(tr("&Host name:"), FHostName);
  connect(FHostName, &QLineEdit::editingFinished, this, &SiteManager::ParseHostName);
  FPort = new QSpinBox(FormPanel);
  FPort->setRange(1, 65535);
  Form->addRow(tr("Po&rt number:"), FPort);
  FUserName = new QLineEdit(FormPanel);
  FUserNameLabel = new QLabel(tr("&User name:"), FormPanel);
  FUserNameLabel->setBuddy(FUserName);
  Form->addRow(FUserNameLabel, FUserName);
  FPassword = new QLineEdit(FormPanel);
  FPassword->setEchoMode(QLineEdit::Password);
  FPasswordLabel = new QLabel(tr("&Password:"), FormPanel);
  FPasswordLabel->setBuddy(FPassword);
  Form->addRow(FPasswordLabel, FPassword);
  FormLayout->addLayout(Form);
  // BasicFtpPanel: anonymous login, the user and password it implies.
  FAnonymous = new QCheckBox(tr("A&nonymous login"), FormPanel);
  connect(FAnonymous, &QCheckBox::clicked, this, [this](bool Checked)
  {
    if (Checked)
    {
      FUserName->setText(QString::fromStdString(CoreBridge::FtpAnonymousUserName()));
      FPassword->setText(QString::fromStdString(CoreBridge::FtpAnonymousPassword()));
    }
    UpdateCredentials();
  });
  FormLayout->addWidget(FAnonymous);
  // BasicS3Panel: the credentials from the AWS environment, by profile.
  QHBoxLayout * S3Row = new QHBoxLayout();
  FS3CredentialsEnv = new QCheckBox(tr("&Credentials from AWS environment:"), FormPanel);
  FS3Profile = new QComboBox(FormPanel);
  FS3Profile->setEditable(true);
  FS3Profile->setAccessibleName(tr("AWS profile"));
  S3Row->addWidget(FS3CredentialsEnv);
  S3Row->addWidget(FS3Profile, 1);
  FormLayout->addLayout(S3Row);
  connect(FS3CredentialsEnv, &QCheckBox::clicked, this, [this]() { UpdateS3Credentials(); UpdateCredentials(); });
  connect(FS3Profile, &QComboBox::currentTextChanged, this, [this]()
  {
    if (!FLoading)
    {
      UpdateS3Credentials();
    }
  });
  for (QLineEdit * Edit : { FUserName, FPassword })
  {
    connect(Edit, &QLineEdit::textChanged, this, [this]() { UpdateCredentials(); });
  }
  // As upstream's: Advanced..., with Edit Raw Settings beside it.
  FAdvanced = new QToolButton(FormPanel);
  FAdvanced->setText(tr("Ad&vanced..."));
  FAdvanced->setPopupMode(QToolButton::MenuButtonPopup);
  QMenu * AdvancedMenu = new QMenu(FAdvanced);
  AdvancedMenu->addAction(tr("A&dvanced..."), this, &SiteManager::Advanced);
  AdvancedMenu->addAction(tr("Edit &Raw Settings..."), this, [this]() { RawSettings(); });
  FAdvanced->setMenu(AdvancedMenu);
  connect(FAdvanced, &QToolButton::clicked, this, &SiteManager::Advanced);
  QHBoxLayout * AdvancedRow = new QHBoxLayout();
  AdvancedRow->addStretch(1);
  AdvancedRow->addWidget(FAdvanced);
  FormLayout->addLayout(AdvancedRow);
  FormLayout->addStretch(1);
  connect(FProtocol, &QComboBox::currentIndexChanged, this, &SiteManager::ProtocolChanged);
  connect(FEncryption, &QComboBox::currentIndexChanged, this, &SiteManager::ProtocolChanged);
  Splitter->setStretchFactor(0, 2);
  Splitter->setStretchFactor(1, 3);

  QDialogButtonBox * Box = new QDialogButtonBox(this);
  FLogin = Box->addButton(tr("&Login"), QDialogButtonBox::AcceptRole);
  FLogin->setDefault(true);
  // As upstream's Save, its drop-down (SaveDropDownMenu): Save, Save As...,
  // Set Defaults.
  FSave = new QToolButton(this);
  FSave->setText(tr("&Save"));
  FSave->setPopupMode(QToolButton::MenuButtonPopup);
  FSave->setToolButtonStyle(Qt::ToolButtonTextOnly);
  QMenu * SaveMenu = new QMenu(FSave);
  QAction * SaveAction = SaveMenu->addAction(tr("&Save"), this, &SiteManager::Save);
  SaveMenu->setDefaultAction(SaveAction);
  SaveMenu->addAction(tr("Save &As..."), this, [this]() { SaveAsDialog(true); });
  SaveMenu->addSeparator();
  SaveMenu->addAction(tr("Set De&faults"), this, [this]() { SetDefaults(); });
  FSave->setMenu(SaveMenu);
  Box->addButton(FSave, QDialogButtonBox::ActionRole);
  QPushButton * Close = Box->addButton(tr("Close"), QDialogButtonBox::RejectRole);
  // As upstream's Login: Tools, its menu.
  QPushButton * Tools = Box->addButton(tr("&Tools"), QDialogButtonBox::ResetRole);
  QMenu * ToolsMenu = new QMenu(Tools);
  ToolsMenu->addAction(tr("&Import Sites..."), this, [this]() { ImportSites(); });
  ToolsMenu->addSeparator();
  ToolsMenu->addAction(tr("&Clean Up..."), this, [this]() { Cleanup(); });
  // As Manage's DesktopIconAction and SendToHookAction.
  ToolsMenu->addSeparator();
  FDesktopIconAction = ToolsMenu->addAction(tr("Desktop &Icon"), this, [this]() { CreateSessionShortcut(false); });
#if defined(Q_OS_WIN)
  FSendToAction = ToolsMenu->addAction(tr("Explorer's 'Send To' Shortcut"), this, [this]() { CreateSessionShortcut(true); });
#endif
  connect(ToolsMenu, &QMenu::aboutToShow, this, [this]()
  {
    const QTreeWidgetItem * Item = FTree->currentItem();
    const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : NewSiteKind;
    FDesktopIconAction->setEnabled((Kind == SiteKind) || (Kind == FolderKind) || (Kind == WorkspaceKind));
    if (FSendToAction != nullptr)
    {
      FSendToAction->setEnabled(Kind == SiteKind);
    }
  });
  Tools->setMenu(ToolsMenu);
  connect(FLogin, &QPushButton::clicked, this, &SiteManager::accept);
  connect(FSave, &QToolButton::clicked, this, &SiteManager::Save);
  connect(Close, &QPushButton::clicked, this, &QDialog::reject);
  Layout->addWidget(Box);

  Fill(QString());
}

void SiteManager::Fill(const QString & Select, const QStringList & ExtraFolders)
{
  const QSignalBlocker Blocker(FTree);
  FTree->clear();
  QTreeWidgetItem * NewSite = new QTreeWidgetItem(FTree, { CoreText(CoreBridge::TText::NewSiteNode) });
  NewSite->setData(0, KindRole, NewSiteKind);
  NewSite->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  const QIcon FolderIcon = style()->standardIcon(QStyle::SP_DirIcon);
  const QIcon SiteIcon = style()->standardIcon(QStyle::SP_DriveNetIcon);
  QTreeWidgetItem * Selected = NewSite;

  // The folder of Path, made as needed.
  auto Folder = [this, &FolderIcon](const QString & Path) -> QTreeWidgetItem *
  {
    QTreeWidgetItem * Parent = nullptr;
    QString SoFar;
    for (const QString & Part : Path.split(QLatin1Char('/'), Qt::SkipEmptyParts))
    {
      SoFar = SoFar.isEmpty() ? Part : (SoFar + QLatin1Char('/') + Part);
      QTreeWidgetItem * Found = nullptr;
      const int Count = (Parent != nullptr) ? Parent->childCount() : FTree->topLevelItemCount();
      for (int Child = 0; (Child < Count) && (Found == nullptr); Child++)
      {
        QTreeWidgetItem * Item = (Parent != nullptr) ? Parent->child(Child) : FTree->topLevelItem(Child);
        if ((Item->data(0, KindRole).toInt() == FolderKind) && (Item->text(0) == Part))
        {
          Found = Item;
        }
      }
      if (Found == nullptr)
      {
        Found = (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Part }) : new QTreeWidgetItem(FTree, { Part });
        Found->setData(0, KindRole, FolderKind);
        Found->setData(0, NameRole, SoFar);
        Found->setIcon(0, FolderIcon);
      }
      Parent = Found;
    }
    return Parent;
  };

  for (const std::string & Name : CoreBridge::SiteNames())
  {
    const QString Path = QString::fromStdString(Name);
    const int Slash = Path.lastIndexOf(QLatin1Char('/'));
    QTreeWidgetItem * Parent = (Slash > 0) ? Folder(Path.left(Slash)) : nullptr;
    const QString Leaf = Path.mid(Slash + 1);
    QTreeWidgetItem * Item = (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Leaf }) : new QTreeWidgetItem(FTree, { Leaf });
    Item->setData(0, KindRole, SiteKind);
    Item->setData(0, NameRole, Path);
    Item->setIcon(0, SiteIcon);
    if (Path == Select)
    {
      Selected = Item;
    }
  }
  for (const QString & Path : FNewFolders + ExtraFolders)
  {
    QTreeWidgetItem * Item = Folder(Path);
    if (Path == Select)
    {
      Selected = Item;
    }
  }
  // As the Login dialog's tree: the workspaces after the sites.
  const QIcon WorkspaceIcon = style()->standardIcon(QStyle::SP_DesktopIcon);
  for (const std::string & Name : CoreBridge::WorkspaceNames())
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FTree, { QString::fromStdString(Name) });
    Item->setData(0, KindRole, WorkspaceKind);
    Item->setData(0, NameRole, QString::fromStdString(Name));
    Item->setIcon(0, WorkspaceIcon);
  }
  FTree->expandAll();
  FTree->setCurrentItem(Selected);
  CurrentChanged();
}

QString SiteManager::SelectedName() const
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  return ((Item != nullptr) && (Item->data(0, KindRole).toInt() == SiteKind)) ? Item->data(0, NameRole).toString() : QString();
}

// The folder under the cursor, or the selected site's.
QString SiteManager::SelectedFolder() const
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  if (Item == nullptr)
  {
    return QString();
  }
  const int Kind = Item->data(0, KindRole).toInt();
  if (Kind == FolderKind)
  {
    return Item->data(0, NameRole).toString();
  }
  if (Kind == SiteKind)
  {
    const QString Name = Item->data(0, NameRole).toString();
    const int Slash = Name.lastIndexOf(QLatin1Char('/'));
    return (Slash > 0) ? Name.left(Slash) : QString();
  }
  return QString();
}

QStringList SiteManager::SitesIn(const QString & Folder) const
{
  QStringList Result;
  for (const std::string & Name : CoreBridge::SiteNames())
  {
    const QString Path = QString::fromStdString(Name);
    if (Path.startsWith(Folder + QLatin1Char('/')))
    {
      Result.append(Path);
    }
  }
  return Result;
}

bool SiteManager::SelectSite(const QString & Name)
{
  for (QTreeWidgetItemIterator It(FTree); *It != nullptr; ++It)
  {
    if (((*It)->data(0, KindRole).toInt() == SiteKind) && ((*It)->data(0, NameRole).toString() == Name))
    {
      FTree->setCurrentItem(*It);
      return true;
    }
  }
  return false;
}

QString SiteManager::SelectedWorkspace() const
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  return ((Item != nullptr) && (Item->data(0, KindRole).toInt() == WorkspaceKind)) ? Item->data(0, NameRole).toString() : QString();
}

QString SiteManager::Workspace() const
{
  return SelectedWorkspace();
}

bool SiteManager::SelectWorkspace(const QString & Name)
{
  for (QTreeWidgetItemIterator It(FTree); *It != nullptr; ++It)
  {
    if (((*It)->data(0, KindRole).toInt() == WorkspaceKind) && ((*It)->data(0, NameRole).toString() == Name))
    {
      FTree->setCurrentItem(*It);
      return true;
    }
  }
  return false;
}

bool SiteManager::SelectFolder(const QString & Path)
{
  for (QTreeWidgetItemIterator It(FTree); *It != nullptr; ++It)
  {
    if (((*It)->data(0, KindRole).toInt() == FolderKind) && ((*It)->data(0, NameRole).toString() == Path))
    {
      FTree->setCurrentItem(*It);
      return true;
    }
  }
  return false;
}

void SiteManager::SelectNewSite()
{
  FTree->setCurrentItem(FTree->topLevelItem(0));
}

void SiteManager::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

void SiteManager::ShowMessage(CoreBridge::TQueryType Type, const QString & Message)
{
  CoreBridge::TQuery Query;
  Query.Type = Type;
  Query.Message = Message.toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  Query.Buttons.push_back(OK);
  Query.CancelAnswer = CoreBridge::AnswerOK;
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  Frontend.Query(Query);
}

void SiteManager::CurrentChanged()
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : FolderKind;
  try
  {
    if (Kind == SiteKind)
    {
      Load(CoreBridge::LoadSite(Item->data(0, NameRole).toString().toStdString()));
    }
    else if (Kind == NewSiteKind)
    {
      Load(CoreBridge::NewSite());
      FNewSiteName.clear();
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  UpdateButtons();
}

void SiteManager::Load(const CoreBridge::TSite & Site)
{
  FLoading = true;
  FSite = Site;
  FProtocol->setCurrentIndex(FProtocol->findData(static_cast<int>(Site.Protocol)));
  ProtocolChanged();
  const int Encryption = FEncryption->findData(static_cast<int>(Site.Encryption));
  FEncryption->setCurrentIndex((Encryption >= 0) ? Encryption : 0);
  FDefaultPort = CoreBridge::DefaultPort(Site.Protocol, Site.Encryption);
  FPort->setValue((Site.Port > 0) ? Site.Port : FDefaultPort);
  FHostName->setText(QString::fromStdString(Site.HostName));
  FUserName->setText(QString::fromStdString(Site.UserName));
  FPassword->setText(QString::fromStdString(Site.Password));
  // LoadS3Profiles: the general credentials first.
  {
    const QSignalBlocker Blocker(FS3Profile);
    FS3Profile->clear();
    FS3Profile->addItem(QString::fromStdString(CoreBridge::S3GeneralName()));
    for (const std::string & Profile : CoreBridge::S3Profiles())
    {
      FS3Profile->addItem(QString::fromStdString(Profile));
    }
    FS3Profile->setCurrentText(Site.S3Profile.empty() ? FS3Profile->itemText(0) : QString::fromStdString(Site.S3Profile));
  }
  FS3CredentialsEnv->setChecked(Site.S3CredentialsEnv);
  FLoading = false;
  UpdateS3Credentials();
  UpdateCredentials();
}

// UpdateS3Credentials: from the environment, the profile's.
void SiteManager::UpdateS3Credentials()
{
  if (FS3CredentialsEnv->isChecked())
  {
    const QString Profile = (FS3Profile->currentText() == FS3Profile->itemText(0)) ? QString() : FS3Profile->currentText();
    const CoreBridge::TS3EnvCredentials Credentials = CoreBridge::S3EnvCredentials(Profile.toStdString());
    FUserName->setText(QString::fromStdString(Credentials.UserName));
    FPassword->setText(QString::fromStdString(Credentials.Password));
  }
}

// As UpdateControls: FTP's anonymous login checked by its user and
// password; S3's credentials from the environment instead of typed ones,
// its labels the access key's.
void SiteManager::UpdateCredentials()
{
  const TProtocol Protocol = static_cast<TProtocol>(FProtocol->currentData().toInt());
  const bool Ftp = (Protocol == TProtocol::Ftp);
  const bool S3 = (Protocol == TProtocol::S3);
  FAnonymous->setVisible(Ftp);
  FAnonymous->setChecked(
    (FUserName->text().compare(QString::fromStdString(CoreBridge::FtpAnonymousUserName()), Qt::CaseInsensitive) == 0) &&
    (FPassword->text().compare(QString::fromStdString(CoreBridge::FtpAnonymousPassword()), Qt::CaseInsensitive) == 0));
  FS3CredentialsEnv->setVisible(S3);
  FS3Profile->setVisible(S3);
  const QTreeWidgetItem * Item = FTree->currentItem();
  const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : FolderKind;
  const bool Form = (Kind == SiteKind) || (Kind == NewSiteKind);
  const bool Environment = S3 && FS3CredentialsEnv->isChecked();
  FS3Profile->setEnabled(Form && Environment);
  FUserName->setEnabled(Form && !Environment);
  FPassword->setEnabled(Form && !Environment);
  FUserNameLabel->setText(S3 ? CoreText(CoreBridge::TText::S3AccessKeyIdPrompt) : tr("&User name:"));
  FPasswordLabel->setText(S3 ? CoreText(CoreBridge::TText::S3SecretAccessKeyPrompt) : tr("&Password:"));
  FUserNameLabel->setEnabled(FUserName->isEnabled());
  FPasswordLabel->setEnabled(FPassword->isEnabled());
}

// As the Login dialog: the encryption choices follow the protocol, and the
// port follows the protocol's default unless it was changed.
void SiteManager::ProtocolChanged()
{
  const TProtocol Protocol = static_cast<TProtocol>(FProtocol->currentData().toInt());
  if ((sender() == FProtocol) || FLoading)
  {
    const int Previous = FEncryption->currentData().isValid() ? FEncryption->currentData().toInt() : 0;
    const QSignalBlocker Blocker(FEncryption);
    FEncryption->clear();
    if ((Protocol == TProtocol::Ftp) || (Protocol == TProtocol::WebDav) || (Protocol == TProtocol::S3))
    {
      FEncryption->addItem(tr("No encryption"), static_cast<int>(TEncryption::None));
      FEncryption->addItem(CoreText(CoreBridge::TText::FtpsImplicit), static_cast<int>(TEncryption::Implicit));
      if (Protocol == TProtocol::Ftp)
      {
        FEncryption->addItem(CoreText(CoreBridge::TText::FtpsExplicit), static_cast<int>(TEncryption::Explicit));
      }
      const int Index = FEncryption->findData(Previous);
      // S3 is TLS unless asked otherwise, as core's default.
      FEncryption->setCurrentIndex((Index >= 0) ? Index : ((Protocol == TProtocol::S3) ? 1 : 0));
    }
  }
  if ((sender() == FProtocol) && !FLoading)
  {
    // TransferProtocolComboChange: a new protocol, no environment's credentials.
    FS3CredentialsEnv->setChecked(false);
  }
  UpdateCredentials();
  const bool HasEncryption = (FEncryption->count() > 0);
  FEncryptionLabel->setVisible(HasEncryption);
  FEncryption->setVisible(HasEncryption);
  if (!FLoading)
  {
    const TEncryption Encryption =
      HasEncryption ? static_cast<TEncryption>(FEncryption->currentData().toInt()) : TEncryption::None;
    const int Default = CoreBridge::DefaultPort(Protocol, Encryption);
    if (FPort->value() == FDefaultPort)
    {
      FPort->setValue(Default);
    }
    FDefaultPort = Default;
  }
}

CoreBridge::TSite SiteManager::Edited() const
{
  CoreBridge::TSite Site = FSite;
  Site.Name = SelectedName().toStdString();
  Site.Protocol = static_cast<TProtocol>(FProtocol->currentData().toInt());
  // SFTP and SCP have no encryption choice: SSH is always encrypted.
  Site.Encryption = (FEncryption->count() == 0) ? TEncryption::None : static_cast<TEncryption>(FEncryption->currentData().toInt());
  Site.HostName = FHostName->text().trimmed().toStdString();
  Site.Port = (FPort->value() == CoreBridge::DefaultPort(Site.Protocol, Site.Encryption)) ? 0 : FPort->value();
  Site.UserName = FUserName->text().toStdString();
  Site.Password = FPassword->text().toStdString();
  Site.S3CredentialsEnv = (Site.Protocol == TProtocol::S3) && FS3CredentialsEnv->isChecked();
  Site.S3Profile = (FS3Profile->currentText() == FS3Profile->itemText(0)) ? std::string() : FS3Profile->currentText().toStdString();
  return Site;
}

void SiteManager::SetEdited(const CoreBridge::TSite & Site)
{
  const QString Name = SelectedName();
  Load(Site);
  FSite.Name = Name.toStdString();
}

CoreBridge::TSite SiteManager::Site() const
{
  return Edited();
}

void SiteManager::Advanced()
{
  SiteAdvanced Dialog(Edited(), this);
  if (Dialog.exec() == QDialog::Accepted)
  {
    FSite = Dialog.Site();
  }
}

bool SiteManager::ImportSites()
{
  ImportSitesDialog Dialog(this);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  // As upstream: the first imported selected.
  Fill(Dialog.Imported().empty() ? QString() : QString::fromStdString(Dialog.Imported().front()));
  return true;
}

// As TLoginDialog::CleanUpActionExecute: the sites reloaded after.
bool SiteManager::Cleanup()
{
  bool Result = false;
  try
  {
    CleanupDialog Dialog(this);
    Result = Dialog.Execute();
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  if (Result)
  {
    Fill(QString());
  }
  return Result;
}

bool SiteManager::RawSettings(const QString * Raw)
{
  const CoreBridge::TSite Site = Edited();
  // The site's stored settings under the dialog's fields.
  const std::string Base = Site.Name;
  QString Settings;
  try
  {
    Settings = QString::fromStdString(Site.HasRawSettings ? Site.RawSettings : CoreBridge::SiteRawSettings(Site, Base));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  if (Raw != nullptr)
  {
    Settings = *Raw;
  }
  else
  {
    SiteRawDialog Dialog(Settings, this);
    if (Dialog.exec() != QDialog::Accepted)
    {
      return false;
    }
    Settings = Dialog.Settings();
  }
  try
  {
    SetEdited(CoreBridge::ApplySiteRawSettings(Site, Base, Settings.toStdString()));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  return true;
}

// As upstream's VerifyAndConvertKey on login and save: an OpenSSH key is
// offered for conversion, one that cannot be used is warned of.
bool SiteManager::VerifyKey()
{
  const CoreBridge::TSite Site = Edited();
  if (Site.PrivateKeyFile.empty() || ((Site.Protocol != TProtocol::Sftp) && (Site.Protocol != TProtocol::Scp)))
  {
    return true;
  }
  QString FileName = QString::fromStdString(Site.PrivateKeyFile);
  if (!VerifyAndConvertKey(this, FileName, true, ChooseConvertedKeyFile))
  {
    return false;
  }
  if (FileName.toStdString() != Site.PrivateKeyFile)
  {
    CoreBridge::TSite Converted = Site;
    Converted.PrivateKeyFile = FileName.toStdString();
    FSite = Converted;
  }
  return true;
}

void SiteManager::AskNewFolder()
{
  bool OK = false;
  const QString Name = QInputDialog::getText(this, CoreText(CoreBridge::TText::NewFolderCaption),
    CoreText(CoreBridge::TText::NewFolderPrompt), QLineEdit::Normal, QString(), &OK).trimmed();
  if (OK && !Name.isEmpty())
  {
    NewFolder(Name);
  }
}

void SiteManager::AskRename()
{
  const QString Current = !SelectedName().isEmpty() ? SelectedName() : SelectedFolder();
  bool OK = false;
  const QString Name = QInputDialog::getText(this, tr("Rename"), tr("New &name:"), QLineEdit::Normal, Current, &OK).trimmed();
  if (OK && !Name.isEmpty() && (Name != Current))
  {
    RenameSelected(Name);
  }
}

void SiteManager::FillManageMenu(QMenu * Menu)
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : NewSiteKind;
  const bool Form = (Kind == SiteKind) || (Kind == NewSiteKind);
  QAction * Login = Menu->addAction(tr("&Login"), this, &SiteManager::accept);
  Login->setEnabled(FLogin->isEnabled());
  if (Kind != WorkspaceKind)
  {
    Menu->addAction(tr("Open in &PuTTY"), QKeySequence(Qt::CTRL | Qt::Key_P), this, [this]() { OpenInPutty(); })->setEnabled(Form);
  }
  Menu->addSeparator();
  if (Kind == SiteKind)
  {
    // The form is always editable here: Edit goes to it.
    Menu->addAction(tr("&Edit"), this, [this]()
    {
      FHostName->setFocus();
      FHostName->selectAll();
    });
  }
  if (Kind == NewSiteKind)
  {
    Menu->addAction(tr("Save &As..."), this, [this]() { SaveAsDialog(true); });
    Menu->addAction(tr("&Reset"), this, &SiteManager::ResetNewSite);
    Menu->addAction(tr("Paste Session &URL"), QKeySequence(Qt::CTRL | Qt::Key_V), this, &SiteManager::PasteUrl);
  }
  else
  {
    Menu->addAction(tr("&Delete"), this, [this]() { DeleteSelected(true); });
    Menu->addAction(tr("&Rename"), this, &SiteManager::AskRename)->setEnabled(FRename->isEnabled());
  }
  if (Kind == SiteKind)
  {
    Menu->addAction(tr("&Clone to New Site"), this, &SiteManager::CloneToNewSite);
  }
  if (Form)
  {
    Menu->addAction(tr("&Generate Session URL/Code..."), this, &SiteManager::GenerateUrl);
    Menu->addSeparator();
    Menu->addAction(tr("Set De&faults"), this, [this]() { SetDefaults(); });
  }
  if (Kind != WorkspaceKind)
  {
    Menu->addSeparator();
    Menu->addAction(tr("Ne&w Folder..."), this, &SiteManager::AskNewFolder);
  }
  if (Kind != NewSiteKind)
  {
    Menu->addAction(tr("Desktop &Icon"), this, [this]() { CreateSessionShortcut(false); });
#if defined(Q_OS_WIN)
    if (Kind == SiteKind)
    {
      Menu->addAction(tr("Explorer's 'Send To' Shortcut"), this, [this]() { CreateSessionShortcut(true); });
    }
#endif
  }
}

// As TLoginDialog::CloneToNewSite.
void SiteManager::CloneToNewSite()
{
  const QString Name = SelectedName();
  if (Name.isEmpty())
  {
    return;
  }
  CoreBridge::TSite Site = Edited();
  SelectNewSite();
  Site.Name.clear();
  Load(Site);
  FNewSiteName = QString::fromStdString(CoreBridge::UniqueSiteName(Name.toStdString()));
  FHostName->setFocus();
}

bool SiteManager::ParseUrl(const QString & Url)
{
  try
  {
    bool Unsafe = false;
    const CoreBridge::TSite Site = CoreBridge::ParseSiteUrl(Edited(), Url.toStdString(), Unsafe);
    if (Unsafe)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = CoreBridge::Text(CoreBridge::TText::UnsafeSession);
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = "OK";
      CoreBridge::TQueryButton Cancel;
      Cancel.Answer = CoreBridge::AnswerCancel;
      Cancel.Caption = "Cancel";
      Query.Buttons = { OK, Cancel };
      Query.CancelAnswer = CoreBridge::AnswerCancel;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
      {
        return false;
      }
    }
    Load(Site);
    UpdateButtons();
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

// As PasteUrlActionExecute.
void SiteManager::PasteUrl()
{
  const QString Url = QGuiApplication::clipboard()->text().trimmed();
  if (Url.isEmpty())
  {
    return;
  }
  const QTreeWidgetItem * Item = FTree->currentItem();
  if ((Item == nullptr) || (Item->data(0, KindRole).toInt() != NewSiteKind))
  {
    SelectNewSite();
  }
  ParseUrl(Url);
  // Shown: the host name pasted.
  FHostName->setFocus();
  FHostName->selectAll();
}

void SiteManager::ParseHostName()
{
  const QString HostName = FHostName->text().trimmed();
  if (!FLoading && CoreBridge::HostNameIsUrl(HostName.toStdString()))
  {
    ParseUrl(HostName);
  }
}

// As ResetNewSessionActionExecute.
void SiteManager::ResetNewSite()
{
  SelectNewSite();
  Load(CoreBridge::NewSite());
  FNewSiteName.clear();
}

// As GenerateUrlAction2Execute.
void SiteManager::GenerateUrl()
{
  CoreBridge::TSite Site = Edited();
  Site.Name = SelectedName().toStdString();
  GenerateUrlDialog Dialog(nullptr, {}, this, nullptr, &Site);
  Dialog.exec();
}

// As PuttyActionExecute; this dialog stays.
bool SiteManager::OpenInPutty()
{
  CoreBridge::TSite Site = Edited();
  Site.Name = SelectedName().toStdString();
  try
  {
    CoreBridge::OpenSiteInPutty(Site, WindowCommandPrompts(this));
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
}

void SiteManager::accept()
{
  // As upstream, before saving or logging in: leaving the host name may
  // not have parsed it.
  if (SelectedWorkspace().isEmpty())
  {
    ParseHostName();
  }
  if (!SelectedWorkspace().isEmpty() || VerifyKey())
  {
    QDialog::accept();
  }
}

bool SiteManager::SaveAs(const QString & Name, bool SavePassword)
{
  const QString Original = SelectedName();
  if (CoreBridge::SiteExists(Name.toStdString()) && (Name != Original))
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreText(CoreBridge::TText::ConfirmOverwriteSite, Name).toStdString();
    CoreBridge::TQueryButton Yes;
    Yes.Answer = CoreBridge::AnswerYes;
    Yes.Caption = "&Yes";
    CoreBridge::TQueryButton No;
    No.Answer = CoreBridge::AnswerNo;
    No.Caption = "&No";
    Query.Buttons = { Yes, No };
    Query.CancelAnswer = CoreBridge::AnswerNo;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerYes)
    {
      return false;
    }
  }
  if (!VerifyKey())
  {
    return false;
  }
  CoreBridge::TSite Site = Edited();
  Site.Name = Name.toStdString();
  if (!SavePassword)
  {
    Site.Password.clear();
  }
  try
  {
    // A new site from this one's settings (or the defaults); the original stays.
    CoreBridge::SaveSite(Site, Original.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  // A folder made here holds a site now: the tree has it without help.
  const int Slash = Name.lastIndexOf(QLatin1Char('/'));
  FNewFolders.removeAll((Slash > 0) ? Name.left(Slash) : QString());
  Fill(Name);
  return true;
}

// Upstream's "Save": a site in place; a new site through its save dialog
// (forms/SaveSession.cpp): the name, with the folder under the cursor, and
// whether to keep the password.
void SiteManager::Save()
{
  // As SaveSessionActionExecute: for the new site, Save As.
  SaveAsDialog(SelectedName().isEmpty());
}

// As SaveAsSession: in place unless Force, else through the save dialog,
// with the site's own name for a stored site (Save As).
void SiteManager::SaveAsDialog(bool Force)
{
  ParseHostName();
  const QString Name = SelectedName();
  if (!Force)
  {
    if (!VerifyKey())
    {
      return;
    }
    try
    {
      CoreBridge::SaveSite(Edited(), Name.toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
    Fill(Name);
    return;
  }
  // As DoSaveSessionDialog (TSaveSessionDialog): the name, and its folder,
  // the selected one to start with, chosen among the sites' folders and
  // the new ones.
  const CoreBridge::TSite Site = Edited();
  const QString Folder = SelectedFolder();
  CoreBridge::TSession::TSaveAsSite Defaults;
  const QString Suggested =
    QString::fromStdString(Site.UserName.empty() ? Site.HostName : Site.UserName + "@" + Site.HostName);
  Defaults.Name = !Name.isEmpty() ? Name.toStdString() : !FNewSiteName.isEmpty() ? FNewSiteName.toStdString() :
    (Folder.isEmpty() ? Suggested : (Folder + QLatin1Char('/') + Suggested)).toStdString();
  Defaults.CanSavePassword = !FPassword->text().isEmpty();
  QStringList Folders;
  for (QTreeWidgetItemIterator It(FTree); *It; ++It)
  {
    if ((*It)->data(0, KindRole).toInt() == FolderKind)
    {
      Folders.append((*It)->data(0, NameRole).toString());
    }
  }
  Folders.sort(Qt::CaseInsensitive);
  for (const QString & Each : Folders)
  {
    Defaults.Folders.push_back(Each.toStdString());
  }
  SaveSiteDialog Dialog(Defaults, this);
  if (Dialog.exec() == QDialog::Accepted)
  {
    SaveAs(Dialog.Name(), Dialog.SavePassword());
    if (Dialog.CreateShortcut())
    {
      CreateSessionShortcut(false);
    }
  }
}

// As SetDefaultSessionActionExecute: the form as the new sites' settings,
// after asking; then the new site, from them.
bool SiteManager::SetDefaults(bool Confirm)
{
  if (Confirm)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreBridge::Text(CoreBridge::TText::SetDefaultSiteSettings);
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
  }
  if (!VerifyKey())
  {
    return false;
  }
  try
  {
    CoreBridge::SaveDefaultSite(Edited());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  // As Default: the new site, reset.
  SelectNewSite();
  Load(CoreBridge::NewSite());
  return true;
}

// As the Login dialog renames in its tree: a site, or a folder with every
// site in it.
bool SiteManager::RenameSelected(const QString & NewName)
{
  const QString Site = SelectedName();
  const QString Folder = Site.isEmpty() ? SelectedFolder() : QString();
  try
  {
    if (!Site.isEmpty())
    {
      if (CoreBridge::SiteExists(NewName.toStdString()))
      {
        ShowMessage(CoreBridge::TQueryType::Error, tr("Site '%1' already exists.").arg(NewName));
        return false;
      }
      CoreBridge::RenameSite(Site.toStdString(), NewName.toStdString());
      Fill(NewName);
      return true;
    }
    if (!Folder.isEmpty())
    {
      for (const QString & Name : SitesIn(Folder))
      {
        CoreBridge::RenameSite(Name.toStdString(), (NewName + Name.mid(Folder.length())).toStdString());
      }
      for (QString & Path : FNewFolders)
      {
        if ((Path == Folder) || Path.startsWith(Folder + QLatin1Char('/')))
        {
          Path = NewName + Path.mid(Folder.length());
        }
      }
      Fill(NewName);
      return true;
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    Fill(QString());
  }
  return false;
}

void SiteManager::NewFolder(const QString & Name)
{
  const QString Parent = SelectedFolder();
  const QString Path = Parent.isEmpty() ? Name : (Parent + QLatin1Char('/') + Name);
  if (!FNewFolders.contains(Path))
  {
    FNewFolders.append(Path);
  }
  Fill(Path);
}

bool SiteManager::DeleteSelected(bool Confirm)
{
  // A workspace: its sessions, as the Login dialog's delete.
  const QString Workspace = SelectedWorkspace();
  if (!Workspace.isEmpty())
  {
    if (Confirm)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Confirmation;
      Query.Message = CoreText(CoreBridge::TText::ConfirmDeleteWorkspace, Workspace,
        CoreBridge::WorkspaceSessionCount(Workspace.toStdString())).toStdString();
      CoreBridge::TQueryButton OK;
      OK.Answer = CoreBridge::AnswerOK;
      OK.Caption = "OK";
      CoreBridge::TQueryButton Cancel;
      Cancel.Answer = CoreBridge::AnswerCancel;
      Cancel.Caption = "Cancel";
      Query.Buttons = { OK, Cancel };
      Query.CancelAnswer = CoreBridge::AnswerCancel;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
      {
        return false;
      }
    }
    try
    {
      CoreBridge::DeleteWorkspace(Workspace.toStdString());
      Integration::UpdateJumpList(CoreBridge::TJumpListEntry::Workspace, Workspace.toStdString(), false);
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
    Fill(QString());
    return true;
  }
  const QString Name = SelectedName();
  const QString Folder = Name.isEmpty() ? SelectedFolder() : QString();
  if (Name.isEmpty() && Folder.isEmpty())
  {
    return false;
  }
  const QStringList Sites = Name.isEmpty() ? SitesIn(Folder) : QStringList{ Name };
  if (Confirm)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = (Name.isEmpty() ? CoreText(CoreBridge::TText::ConfirmDeleteFolder, Folder, Sites.size())
      : CoreText(CoreBridge::TText::ConfirmDeleteSite, Name)).toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
  }
  try
  {
    for (const QString & Site : Sites)
    {
      CoreBridge::DeleteSite(Site.toStdString());
      // As the Login dialog's: out of the jump list too.
      Integration::UpdateJumpList(CoreBridge::TJumpListEntry::Site, Site.toStdString(), false);
    }
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  if (!Folder.isEmpty())
  {
    FNewFolders.erase(std::remove_if(FNewFolders.begin(), FNewFolders.end(), [&Folder](const QString & Path)
    {
      return (Path == Folder) || Path.startsWith(Folder + QLatin1Char('/'));
    }), FNewFolders.end());
  }
  Fill(QString());
  return true;
}

void SiteManager::UpdateButtons()
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : FolderKind;
  // A workspace: logged in to or deleted, not edited.
  const bool Form = (Kind == SiteKind) || (Kind == NewSiteKind);
  FLogin->setEnabled(Kind != FolderKind);
  FSave->setEnabled(Form);
  FAdvanced->setEnabled(Form);
  FRename->setEnabled((Kind == SiteKind) || (Kind == FolderKind));
  FDelete->setEnabled(Kind != NewSiteKind);
  for (QWidget * Field : std::initializer_list<QWidget *>{ FProtocol, FEncryption, FHostName, FPort, FUserName, FPassword,
         FAnonymous, FS3CredentialsEnv })
  {
    Field->setEnabled(Form);
  }
  UpdateCredentials();
}

bool SiteManager::CreateSessionShortcut(bool SendTo)
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  if (Item == nullptr)
  {
    return false;
  }
  const int Kind = Item->data(0, KindRole).toInt();
  const QString Name = Item->data(0, NameRole).toString();
  try
  {
    if (SendTo && (Kind == SiteKind))
    {
      const CoreBridge::TShortcut Shortcut = CoreBridge::SessionShortcut(Name.toStdString(), CoreBridge::TSessionShortcutKind::Site,
        CoreBridge::SiteSendToShortcutName(Name.toStdString()), { "/SendToHook", "/Upload" });
      return Integration::AskAndCreateShortcut(this,
        QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ConfirmCreateSendTo)).replace(QStringLiteral("%s"), Name),
        Shortcut, Integration::TShortcutFolder::SendTo);
    }
    CoreBridge::TSessionShortcutKind ShortcutKind = CoreBridge::TSessionShortcutKind::Site;
    CoreBridge::TText Question = CoreBridge::TText::ConfirmCreateShortcut;
    std::vector<std::string> Arguments = { "/Desktop" };
    if (Kind == FolderKind)
    {
      ShortcutKind = CoreBridge::TSessionShortcutKind::Folder;
      Question = CoreBridge::TText::ConfirmCreateShortcutFolder;
    }
    else if (Kind == WorkspaceKind)
    {
      ShortcutKind = CoreBridge::TSessionShortcutKind::Workspace;
      Question = CoreBridge::TText::ConfirmCreateShortcutWorkspace;
    }
    else if (Kind == SiteKind)
    {
      Arguments.push_back("/UploadIfAny");
    }
    else
    {
      return false;
    }
    const CoreBridge::TShortcut Shortcut = CoreBridge::SessionShortcut(Name.toStdString(), ShortcutKind, std::string(), Arguments);
    return Integration::AskAndCreateShortcut(this,
      QString::fromStdString(CoreBridge::Text(Question)).replace(QStringLiteral("%s"), Name), Shortcut,
      Integration::TShortcutFolder::Desktop);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
    return false;
  }
}
