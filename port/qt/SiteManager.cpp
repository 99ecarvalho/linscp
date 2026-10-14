#include "SiteManager.h"

#include <QCheckBox>
#include <QComboBox>
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
#include <QMenu>
#include <QToolButton>

namespace
{

using TProtocol = CoreBridge::TSite::TProtocol;
using TEncryption = CoreBridge::TSite::TEncryption;

// Tree items: a site carries its full name, a folder its path; "New Site"
// neither.
enum { NameRole = Qt::UserRole, KindRole };
enum TKind { NewSiteKind, FolderKind, SiteKind };

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
  ChooseConvertedKeyFile = [this](const QString & Suggested)
  {
    return QFileDialog::getSaveFileName(this, tr("Save converted private key"), Suggested,
      tr("PuTTY Private Key Files (*.ppk)"));
  };

  QVBoxLayout * Layout = new QVBoxLayout(this);
  QSplitter * Splitter = new QSplitter(this);
  Layout->addWidget(Splitter, 1);

  QWidget * TreePanel = new QWidget(Splitter);
  QVBoxLayout * TreeLayout = new QVBoxLayout(TreePanel);
  TreeLayout->setContentsMargins(0, 0, 0, 0);
  FTree = new QTreeWidget(TreePanel);
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
  QHBoxLayout * TreeButtons = new QHBoxLayout();
  FNewFolder = new QPushButton(tr("New &Folder..."), TreePanel);
  FRename = new QPushButton(tr("Re&name..."), TreePanel);
  FDelete = new QPushButton(tr("&Delete"), TreePanel);
  TreeButtons->addWidget(FNewFolder);
  TreeButtons->addWidget(FRename);
  TreeButtons->addWidget(FDelete);
  TreeLayout->addLayout(TreeButtons);
  connect(FNewFolder, &QPushButton::clicked, this, [this]()
  {
    bool OK = false;
    const QString Name = QInputDialog::getText(this, CoreText(CoreBridge::TText::NewFolderCaption),
      CoreText(CoreBridge::TText::NewFolderPrompt), QLineEdit::Normal, QString(), &OK).trimmed();
    if (OK && !Name.isEmpty())
    {
      NewFolder(Name);
    }
  });
  connect(FRename, &QPushButton::clicked, this, [this]()
  {
    const QString Current = !SelectedName().isEmpty() ? SelectedName() : SelectedFolder();
    bool OK = false;
    const QString Name = QInputDialog::getText(this, tr("Rename"), tr("New &name:"), QLineEdit::Normal, Current, &OK).trimmed();
    if (OK && !Name.isEmpty() && (Name != Current))
    {
      RenameSelected(Name);
    }
  });
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
  FPort = new QSpinBox(FormPanel);
  FPort->setRange(1, 65535);
  Form->addRow(tr("Po&rt number:"), FPort);
  FUserName = new QLineEdit(FormPanel);
  Form->addRow(tr("&User name:"), FUserName);
  FPassword = new QLineEdit(FormPanel);
  FPassword->setEchoMode(QLineEdit::Password);
  Form->addRow(tr("&Password:"), FPassword);
  FormLayout->addLayout(Form);
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
  FSave = Box->addButton(tr("&Save"), QDialogButtonBox::ActionRole);
  QPushButton * Close = Box->addButton(tr("Close"), QDialogButtonBox::RejectRole);
  connect(FLogin, &QPushButton::clicked, this, &SiteManager::accept);
  connect(FSave, &QPushButton::clicked, this, &SiteManager::Save);
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
  FLoading = false;
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
  const CoreBridge::TKeyCheck Check = CoreBridge::CheckKey(Site.PrivateKeyFile);
  QtFrontend Frontend;
  Frontend.SetWindow(this);
  if (Check.Status == CoreBridge::TKeyCheck::TStatus::Convertible)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = Check.Message;
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    CoreBridge::TQueryButton Cancel;
    Cancel.Answer = CoreBridge::AnswerCancel;
    Cancel.Caption = "Cancel";
    Query.Buttons = { OK, Cancel };
    Query.CancelAnswer = CoreBridge::AnswerCancel;
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerOK)
    {
      return false;
    }
    const QString Target = ChooseConvertedKeyFile(QString::fromStdString(Check.ConvertedFileName));
    if (Target.isEmpty())
    {
      return false;
    }
    try
    {
      ShowMessage(CoreBridge::TQueryType::Information,
        QString::fromStdString(CoreBridge::ConvertKey(Site.PrivateKeyFile, Target.toStdString())));
    }
    catch (const CoreBridge::TCoreError & E)
    {
      ShowError(E);
      return false;
    }
    CoreBridge::TSite Converted = Site;
    Converted.PrivateKeyFile = Target.toStdString();
    FSite = Converted;
    return true;
  }
  if (Check.Status == CoreBridge::TKeyCheck::TStatus::Unusable)
  {
    // Upstream's: Ignore to go on anyway, Abort to stop.
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Warning;
    Query.Message = Check.Message;
    CoreBridge::TQueryButton Abort;
    Abort.Answer = CoreBridge::AnswerAbort;
    Abort.Caption = "&Abort";
    CoreBridge::TQueryButton Ignore;
    Ignore.Answer = CoreBridge::AnswerIgnore;
    Ignore.Caption = "&Ignore";
    Query.Buttons = { Abort, Ignore };
    Query.CancelAnswer = CoreBridge::AnswerAbort;
    return Frontend.Query(Query).Answer == CoreBridge::AnswerIgnore;
  }
  return true;
}

void SiteManager::accept()
{
  if (VerifyKey())
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
  const QString Name = SelectedName();
  if (!Name.isEmpty())
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
  QDialog Dialog(this);
  Dialog.setWindowTitle(CoreText(CoreBridge::TText::SaveSiteCaption));
  QFormLayout * Layout = new QFormLayout(&Dialog);
  QLineEdit * NameEdit = new QLineEdit(&Dialog);
  const CoreBridge::TSite Site = Edited();
  const QString Folder = SelectedFolder();
  const QString Suggested =
    QString::fromStdString(Site.UserName.empty() ? Site.HostName : Site.UserName + "@" + Site.HostName);
  NameEdit->setText(Folder.isEmpty() ? Suggested : (Folder + QLatin1Char('/') + Suggested));
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
  FLogin->setEnabled(Kind != FolderKind);
  FSave->setEnabled(Kind != FolderKind);
  FAdvanced->setEnabled(Kind != FolderKind);
  FRename->setEnabled(Kind != NewSiteKind);
  FDelete->setEnabled(Kind != NewSiteKind);
  for (QWidget * Field : std::initializer_list<QWidget *>{ FProtocol, FEncryption, FHostName, FPort, FUserName, FPassword })
  {
    Field->setEnabled(Kind != FolderKind);
  }
}
