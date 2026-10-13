#include "SiteManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "QtFrontend.h"

namespace
{

using TProtocol = CoreBridge::TSite::TProtocol;
using TEncryption = CoreBridge::TSite::TEncryption;

// Tree items: a site carries its full name; "New Site" and folders do not.
enum { NameRole = Qt::UserRole, KindRole };
enum TKind { NewSiteKind, FolderKind, SiteKind };

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

// Core's texts with their "%s" filled in.
QString CoreText(CoreBridge::TText Text, const QString & Arg)
{
  return CoreText(Text).replace(QStringLiteral("%s"), Arg);
}

}  // namespace

SiteManager::SiteManager(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Login"));
  resize(760, 440);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QSplitter * Splitter = new QSplitter(this);
  Layout->addWidget(Splitter, 1);

  FTree = new QTreeWidget(Splitter);
  FTree->setHeaderHidden(true);
  connect(FTree, &QTreeWidget::currentItemChanged, this, &SiteManager::CurrentChanged);
  connect(FTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem * Item)
  {
    if (Item->data(0, KindRole).toInt() != FolderKind)
    {
      accept();
    }
  });

  QWidget * FormPanel = new QWidget(Splitter);
  QFormLayout * Form = new QFormLayout(FormPanel);
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
  FPort = new QSpinBox(FormPanel);
  FPort->setRange(1, 65535);
  Form->addRow(tr("Po&rt number:"), FPort);
  FUserName = new QLineEdit(FormPanel);
  Form->addRow(tr("&User name:"), FUserName);
  FPassword = new QLineEdit(FormPanel);
  FPassword->setEchoMode(QLineEdit::Password);
  Form->addRow(tr("&Password:"), FPassword);
  FRemoteDirectory = new QLineEdit(FormPanel);
  Form->addRow(tr("Remote &directory:"), FRemoteDirectory);
  connect(FProtocol, &QComboBox::currentIndexChanged, this, &SiteManager::ProtocolChanged);
  connect(FEncryption, &QComboBox::currentIndexChanged, this, &SiteManager::ProtocolChanged);
  Splitter->setStretchFactor(0, 2);
  Splitter->setStretchFactor(1, 3);

  QDialogButtonBox * Box = new QDialogButtonBox(this);
  FLogin = Box->addButton(tr("&Login"), QDialogButtonBox::AcceptRole);
  FLogin->setDefault(true);
  FSave = Box->addButton(tr("&Save"), QDialogButtonBox::ActionRole);
  FDelete = Box->addButton(tr("&Delete"), QDialogButtonBox::ActionRole);
  QPushButton * Close = Box->addButton(tr("Close"), QDialogButtonBox::RejectRole);
  connect(FLogin, &QPushButton::clicked, this, &QDialog::accept);
  connect(FSave, &QPushButton::clicked, this, &SiteManager::Save);
  connect(FDelete, &QPushButton::clicked, this, [this]() { DeleteSelected(true); });
  connect(Close, &QPushButton::clicked, this, &QDialog::reject);
  Layout->addWidget(Box);

  Fill(QString());
}

void SiteManager::Fill(const QString & Select)
{
  FTree->clear();
  QTreeWidgetItem * NewSite = new QTreeWidgetItem(FTree, { CoreText(CoreBridge::TText::NewSiteNode) });
  NewSite->setData(0, KindRole, NewSiteKind);
  NewSite->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  const QIcon FolderIcon = style()->standardIcon(QStyle::SP_DirIcon);
  const QIcon SiteIcon = style()->standardIcon(QStyle::SP_DriveNetIcon);
  QTreeWidgetItem * Selected = NewSite;
  for (const std::string & Name : CoreBridge::SiteNames())
  {
    const QString Path = QString::fromStdString(Name);
    const QStringList Parts = Path.split(QLatin1Char('/'));
    QTreeWidgetItem * Parent = nullptr;
    for (int Index = 0; Index < Parts.size() - 1; Index++)
    {
      QTreeWidgetItem * Folder = nullptr;
      const int Count = (Parent != nullptr) ? Parent->childCount() : FTree->topLevelItemCount();
      for (int Child = 0; (Child < Count) && (Folder == nullptr); Child++)
      {
        QTreeWidgetItem * Item = (Parent != nullptr) ? Parent->child(Child) : FTree->topLevelItem(Child);
        if ((Item->data(0, KindRole).toInt() == FolderKind) && (Item->text(0) == Parts[Index]))
        {
          Folder = Item;
        }
      }
      if (Folder == nullptr)
      {
        Folder = (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Parts[Index] }) : new QTreeWidgetItem(FTree, { Parts[Index] });
        Folder->setData(0, KindRole, FolderKind);
        Folder->setIcon(0, FolderIcon);
      }
      Parent = Folder;
    }
    QTreeWidgetItem * Item =
      (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Parts.last() }) : new QTreeWidgetItem(FTree, { Parts.last() });
    Item->setData(0, KindRole, SiteKind);
    Item->setData(0, NameRole, Path);
    Item->setIcon(0, SiteIcon);
    if (Path == Select)
    {
      Selected = Item;
    }
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
  FProtocol->setCurrentIndex(FProtocol->findData(static_cast<int>(Site.Protocol)));
  ProtocolChanged();
  const int Encryption = FEncryption->findData(static_cast<int>(Site.Encryption));
  FEncryption->setCurrentIndex((Encryption >= 0) ? Encryption : 0);
  FDefaultPort = CoreBridge::DefaultPort(Site.Protocol, Site.Encryption);
  FPort->setValue((Site.Port > 0) ? Site.Port : FDefaultPort);
  FHostName->setText(QString::fromStdString(Site.HostName));
  FUserName->setText(QString::fromStdString(Site.UserName));
  FPassword->setText(QString::fromStdString(Site.Password));
  FRemoteDirectory->setText(QString::fromStdString(Site.RemoteDirectory));
  FLoading = false;
}

// As the Login dialog: the encryption choices follow the protocol, and the
// port follows the protocol's default unless it was changed.
void SiteManager::ProtocolChanged()
{
  const TProtocol Protocol = static_cast<TProtocol>(FProtocol->currentData().toInt());
  if (sender() == FProtocol || FLoading)
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
  const bool HasEncryption = (FEncryption->count() > 0) &&
    ((Protocol == TProtocol::Ftp) || (Protocol == TProtocol::WebDav) || (Protocol == TProtocol::S3));
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

CoreBridge::TSite SiteManager::FromForm() const
{
  CoreBridge::TSite Site;
  Site.Name = SelectedName().toStdString();
  Site.Protocol = static_cast<TProtocol>(FProtocol->currentData().toInt());
  // SFTP and SCP have no encryption choice: SSH is always encrypted.
  Site.Encryption = ((Site.Protocol == TProtocol::Sftp) || (Site.Protocol == TProtocol::Scp) || (FEncryption->count() == 0)) ?
    TEncryption::None : static_cast<TEncryption>(FEncryption->currentData().toInt());
  Site.HostName = FHostName->text().trimmed().toStdString();
  Site.Port = (FPort->value() == CoreBridge::DefaultPort(Site.Protocol, Site.Encryption)) ? 0 : FPort->value();
  Site.UserName = FUserName->text().toStdString();
  Site.Password = FPassword->text().toStdString();
  Site.RemoteDirectory = FRemoteDirectory->text().trimmed().toStdString();
  return Site;
}

CoreBridge::TSite SiteManager::Site() const
{
  return FromForm();
}

bool SiteManager::SaveAs(const QString & Name, bool SavePassword)
{
  const QString Original = SelectedName();
  if (CoreBridge::SiteExists(Name.toStdString()) && (Name != Original) &&
      (QMessageBox::question(this, windowTitle(), CoreText(CoreBridge::TText::ConfirmOverwriteSite, Name)) != QMessageBox::Yes))
  {
    return false;
  }
  CoreBridge::TSite Site = FromForm();
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
  Fill(Name);
  return true;
}

// Upstream's "Save": a site in place; a new site through its save dialog
// (forms/SaveSession.cpp): the name, the folder in it, and whether to keep
// the password.
void SiteManager::Save()
{
  const QString Name = SelectedName();
  if (!Name.isEmpty())
  {
    try
    {
      CoreBridge::SaveSite(FromForm(), Name.toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
    }
    Fill(Name);
    return;
  }
  QDialog Dialog(this);
  Dialog.setWindowTitle(CoreText(CoreBridge::TText::SaveSiteCaption));
  QFormLayout * Layout = new QFormLayout(&Dialog);
  QLineEdit * NameEdit = new QLineEdit(&Dialog);
  const CoreBridge::TSite Site = FromForm();
  NameEdit->setText(QString::fromStdString(Site.UserName.empty() ? Site.HostName : Site.UserName + "@" + Site.HostName));
  Layout->addRow(CoreText(CoreBridge::TText::SaveSitePrompt), NameEdit);
  QCheckBox * SavePassword = new QCheckBox(CoreText(CoreBridge::TText::SaveSitePassword), &Dialog);
  SavePassword->setEnabled(!FPassword->text().isEmpty());
  Layout->addRow(SavePassword);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addRow(Box);
  if ((Dialog.exec() == QDialog::Accepted) && !NameEdit->text().trimmed().isEmpty())
  {
    SaveAs(NameEdit->text().trimmed(), SavePassword->isChecked());
  }
}

bool SiteManager::DeleteSelected(bool Confirm)
{
  const QString Name = SelectedName();
  if (Name.isEmpty() ||
      (Confirm && (QMessageBox::question(this, windowTitle(), CoreText(CoreBridge::TText::ConfirmDeleteSite, Name)) != QMessageBox::Yes)))
  {
    return false;
  }
  try
  {
    CoreBridge::DeleteSite(Name.toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
  }
  Fill(QString());
  return true;
}

void SiteManager::UpdateButtons()
{
  const QTreeWidgetItem * Item = FTree->currentItem();
  const int Kind = (Item != nullptr) ? Item->data(0, KindRole).toInt() : FolderKind;
  FLogin->setEnabled(Kind != FolderKind);
  FSave->setEnabled(Kind != FolderKind);
  FDelete->setEnabled(Kind == SiteKind);
  for (QWidget * Field : std::initializer_list<QWidget *>{ FProtocol, FEncryption, FHostName, FPort, FUserName, FPassword, FRemoteDirectory })
  {
    Field->setEnabled(Kind != FolderKind);
  }
}
