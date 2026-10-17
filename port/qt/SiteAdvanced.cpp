#include "SiteAdvanced.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkProxyFactory>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "ColorMenu.h"
#include "QtFrontend.h"

namespace
{

using TSite = CoreBridge::TSite;
using TProxy = TSite::TProxy;
using TProtocol = TSite::TProtocol;
using TAutoSwitch = TSite::TAutoSwitch;

// As upstream's DefaultRecycleBinPath.
const QString DefaultRecycleBinPath = QStringLiteral("/tmp");
// The FTP proxy combo's logon types, after its methods (FtpProxyMethodCombo).
const int FtpLogonTypeBase = 100;

QString Q(const std::string & Text)
{
  return QString::fromStdString(Text);
}

QString CoreText(CoreBridge::TText Text)
{
  return Q(CoreBridge::Text(Text));
}

// ComboAutoSwitchLoad and Save: Auto, Off, On.
void SetAutoSwitch(QComboBox * Combo, TAutoSwitch Value)
{
  Combo->setCurrentIndex(2 - static_cast<int>(Value));
}

TAutoSwitch AutoSwitch(const QComboBox * Combo)
{
  return static_cast<TAutoSwitch>(2 - Combo->currentIndex());
}

QLabel * Buddy(const QString & Caption, QWidget * Control, QWidget * Parent)
{
  QLabel * Label = new QLabel(Caption, Parent);
  Label->setBuddy(Control);
  return Label;
}

// A combo whose first item stands for "none given" (Default,
// Autodetect, Autoselect): Text is empty for it.
QComboBox * EditableCombo(QWidget * Parent, const QStringList & Items, const QString & Text)
{
  QComboBox * Combo = new QComboBox(Parent);
  Combo->setEditable(true);
  Combo->addItems(Items);
  Combo->setCurrentText(Text.isEmpty() ? Items.front() : Text);
  return Combo;
}

QString EditableComboText(const QComboBox * Combo)
{
  const QString Text = Combo->currentText();
  return (Text == Combo->itemText(0)) ? QString() : Text;
}

}  // namespace

SiteAdvanced::SiteAdvanced(const CoreBridge::TSite & Site, QWidget * Parent) :
  QDialog(Parent),
  FSite(Site)
{
  setWindowTitle(tr("Advanced Site Settings"));
  resize(700, 520);
  FSsh = (Site.Protocol == TProtocol::Sftp) || (Site.Protocol == TProtocol::Scp);
  FSftp = (Site.Protocol == TProtocol::Sftp);
  FScp = (Site.Protocol == TProtocol::Scp);
  FFtp = (Site.Protocol == TProtocol::Ftp);
  FWebDav = (Site.Protocol == TProtocol::WebDav);
  FS3 = (Site.Protocol == TProtocol::S3);
  FNeon = FWebDav || FS3;
  FSsl = (FFtp || FNeon) && (Site.Encryption != TSite::TEncryption::None);
  FColor = Site.Color;
  if (FS3)
  {
    FS3Credentials = CoreBridge::S3Credentials(Site);
  }

  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Main = new QHBoxLayout();
  FNavigation = new QTreeWidget(this);
  FNavigation->setAccessibleName(tr("Pages"));
  FNavigation->setHeaderHidden(true);
  FNavigation->setRootIsDecorated(false);
  FNavigation->setIndentation(12);
  FNavigation->setMaximumWidth(170);
  FPages = new QStackedWidget(this);
  Main->addWidget(FNavigation);
  Main->addWidget(FPages, 1);
  Layout->addLayout(Main, 1);

  FUpdating = true;
  MakeEnvironmentPage();
  MakeDirectoriesPage();
  MakeRecycleBinPage();
  MakeEncryptionPage();
  MakeSftpPage();
  MakeScpPage();
  MakeFtpPage();
  MakeS3Page();
  MakeWebDavPage();
  MakeConnectionPage();
  MakeProxyPage();
  MakeTunnelPage();
  MakeTlsPage();
  MakeSshPage();
  MakeKexPage();
  MakeAuthenticationPage();
  MakeBugsPage();
  MakeNotePage();
  FNavigation->expandAll();
  // As NavigationTreeCollapsing: the tree stays open.
  FNavigation->setItemsExpandable(false);
  connect(FNavigation, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem * Item)
  {
    if (Item != nullptr)
    {
      FPages->setCurrentIndex(Item->data(0, Qt::UserRole).toInt());
    }
  });

  QHBoxLayout * Buttons = new QHBoxLayout();
  FColorButton = new QPushButton(tr("&Color"), this);
  QMenu * ColorMenu = new QMenu(FColorButton);
  connect(ColorMenu, &QMenu::aboutToShow, this, &SiteAdvanced::FillColorMenu);
  FColorButton->setMenu(ColorMenu);
  Buttons->addWidget(FColorButton);
  Buttons->addStretch(1);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &SiteAdvanced::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Buttons->addWidget(Box);
  Layout->addLayout(Buttons);
  SetColor(FColor);

  // Every control's change updates the others, as DataChange.
  for (QCheckBox * Check : findChildren<QCheckBox *>())
  {
    connect(Check, &QCheckBox::toggled, this, &SiteAdvanced::UpdateControls);
  }
  for (QRadioButton * Radio : findChildren<QRadioButton *>())
  {
    connect(Radio, &QRadioButton::toggled, this, &SiteAdvanced::UpdateControls);
  }
  for (QComboBox * Combo : findChildren<QComboBox *>())
  {
    connect(Combo, &QComboBox::currentIndexChanged, this, &SiteAdvanced::UpdateControls);
    connect(Combo, &QComboBox::currentTextChanged, this, &SiteAdvanced::UpdateControls);
  }
  for (QLineEdit * Edit : { FPrivateKeyFile, FProxyTelnetCommand, FProxyLocalCommand })
  {
    connect(Edit, &QLineEdit::textChanged, this, &SiteAdvanced::UpdateControls);
  }
  for (QListWidget * List : { FCiphers, FKex })
  {
    connect(List, &QListWidget::currentRowChanged, this, &SiteAdvanced::UpdateControls);
  }
  FUpdating = false;
  UpdateNavigation();
  UpdateControls();
  ShowPageAt(0);
}

QWidget * SiteAdvanced::AddPage(TPage Page, const QString & Name, bool Subpage)
{
  QWidget * Widget = new QWidget(FPages);
  QTreeWidgetItem * Parent = nullptr;
  if (Subpage)
  {
    for (int Index = Page - 1; (Index >= 0) && (Parent == nullptr); Index--)
    {
      if ((FPageItems[Index] != nullptr) && (FPageItems[Index]->parent() == nullptr))
      {
        Parent = FPageItems[Index];
      }
    }
  }
  QTreeWidgetItem * Item = (Parent != nullptr) ? new QTreeWidgetItem(Parent, { Name }) : new QTreeWidgetItem(FNavigation, { Name });
  Item->setData(0, Qt::UserRole, FPages->addWidget(Widget));
  FPageItems[Page] = Item;
  return Widget;
}

QComboBox * SiteAdvanced::AutoSwitchCombo(QWidget * Parent, TAutoSwitch Value)
{
  // ComboAutoSwitchInitialize.
  QComboBox * Combo = new QComboBox(Parent);
  Combo->addItems({ CoreText(CoreBridge::TText::AutoSwitchAuto), CoreText(CoreBridge::TText::AutoSwitchOff),
    CoreText(CoreBridge::TText::AutoSwitchOn) });
  SetAutoSwitch(Combo, Value);
  return Combo;
}

QWidget * SiteAdvanced::BrowseRow(QLineEdit * Edit, const QString & Caption, bool Directory, const QString & Filter)
{
  QWidget * Row = new QWidget(Edit->parentWidget());
  QHBoxLayout * Layout = new QHBoxLayout(Row);
  Layout->setContentsMargins(0, 0, 0, 0);
  Edit->setParent(Row);
  // Its label is outside the row, where Qt does not look for it.
  if (Edit->accessibleName().isEmpty())
  {
    Edit->setAccessibleName(Caption);
  }
  // TFilenameEdit's and TDirectoryEdit's ellipsis button: no caption.
  QToolButton * Browse = new QToolButton(Row);
  Browse->setText(QStringLiteral("..."));
  Browse->setAccessibleName(Caption);
  Browse->setToolTip(Caption);
  connect(Browse, &QToolButton::clicked, this, [this, Edit, Caption, Directory, Filter]()
  {
    const QString Chosen = Directory ?
      QFileDialog::getExistingDirectory(this, Caption, Edit->text()) :
      QFileDialog::getOpenFileName(this, Caption, Edit->text(), Filter);
    if (!Chosen.isEmpty())
    {
      Edit->setText(QDir::toNativeSeparators(Chosen));
    }
  });
  Layout->addWidget(Edit, 1);
  Layout->addWidget(Browse);
  return Row;
}

// The cipher and key exchange lists: their names in the site's order,
// each item its value.
QListWidget * SiteAdvanced::AlgorithmList(QWidget * Parent, const std::vector<std::string> & Names, const std::vector<int> & Order)
{
  QListWidget * List = new QListWidget(Parent);
  List->setDragDropMode(QAbstractItemView::InternalMove);
  for (int Value : Order)
  {
    if ((Value >= 0) && (Value < static_cast<int>(Names.size())))
    {
      QListWidgetItem * Item = new QListWidgetItem(Q(Names[Value]), List);
      Item->setData(Qt::UserRole, Value);
    }
  }
  return List;
}

QWidget * SiteAdvanced::MakeEnvironmentPage()
{
  QWidget * Page = AddPage(EnvironmentPage, tr("Environment"), false);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Server = new QGroupBox(tr("Server environment"), Page);
  QGridLayout * Grid = new QGridLayout(Server);
  FEolType = new QComboBox(Server);
  FEolType->addItems({ QStringLiteral("LF"), QStringLiteral("CR/LF") });
  FEolType->setCurrentIndex(FSite.EolCrLf ? 1 : 0);
  Grid->addWidget(Buddy(tr("End-&of-line characters (if not indicated by server):"), FEolType, Server), 0, 0, 1, 4);
  Grid->addWidget(FEolType, 0, 4);
  FUtf = AutoSwitchCombo(Server, FSite.Utf);
  Grid->addWidget(Buddy(tr("&UTF-8 encoding for filenames:"), FUtf, Server), 1, 0, 1, 4);
  Grid->addWidget(FUtf, 1, 4);
  FTimeDifferenceHours = new QSpinBox(Server);
  FTimeDifferenceHours->setRange(-25, 25);
  FTimeDifferenceHours->setValue(FSite.TimeDifferenceMinutes / 60);
  FTimeDifferenceMinutes = new QSpinBox(Server);
  FTimeDifferenceMinutes->setRange(-45, 45);
  FTimeDifferenceMinutes->setSingleStep(15);
  FTimeDifferenceMinutes->setValue(FSite.TimeDifferenceMinutes % 60);
  Grid->addWidget(Buddy(tr("Time &zone offset:"), FTimeDifferenceHours, Server), 2, 0);
  Grid->addWidget(FTimeDifferenceHours, 2, 1);
  QLabel * Hours = new QLabel(tr("hours"), Server);
  FTimeDifferenceHours->setAccessibleName(tr("Time zone offset hours"));
  Grid->addWidget(Hours, 2, 2);
  Grid->addWidget(FTimeDifferenceMinutes, 2, 3);
  FTimeDifferenceMinutes->setAccessibleName(tr("Time zone offset minutes"));
  Grid->addWidget(new QLabel(tr("minutes"), Server), 2, 4);
  FTimeDifferenceAuto = new QCheckBox(tr("Detect &automatically"), Server);
  FTimeDifferenceAuto->setChecked(FSite.TimeDifferenceAuto);
  Grid->addWidget(FTimeDifferenceAuto, 3, 1, 1, 4);
  FTrimVMSVersions = new QCheckBox(tr("&Trim VMS version numbers"), Server);
  FTrimVMSVersions->setChecked(FSite.TrimVMSVersions);
  Grid->addWidget(FTrimVMSVersions, 4, 0, 1, 5);
  Grid->setColumnStretch(0, 1);
  Layout->addWidget(Server);
  FDstGroup = new QGroupBox(tr("Daylight saving time"), Page);
  QVBoxLayout * Dst = new QVBoxLayout(FDstGroup);
  FDstUnix = new QRadioButton(tr("Adjust remote timestamp to local co&nventions"), FDstGroup);
  FDstWindows = new QRadioButton(tr("Adjust remote timestamp with &DST"), FDstGroup);
  (FSite.DstWindows ? FDstWindows : FDstUnix)->setChecked(true);
  Dst->addWidget(FDstUnix);
  Dst->addWidget(FDstWindows);
  Layout->addWidget(FDstGroup);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeDirectoriesPage()
{
  QWidget * Page = AddPage(DirectoriesPage, tr("Directories"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Directories = new QGroupBox(tr("Directories"), Page);
  QVBoxLayout * DirectoriesLayout = new QVBoxLayout(Directories);
  FSynchronizeBrowsing = new QCheckBox(tr("&Synchronize browsing"), Directories);
  FSynchronizeBrowsing->setChecked(FSite.SynchronizeBrowsing);
  DirectoriesLayout->addWidget(FSynchronizeBrowsing);
  FRemoteDirectory = new QLineEdit(Q(FSite.RemoteDirectory), Directories);
  DirectoriesLayout->addWidget(Buddy(tr("&Remote directory:"), FRemoteDirectory, Directories));
  DirectoriesLayout->addWidget(FRemoteDirectory);
  FLocalDirectory = new QLineEdit(Q(FSite.LocalDirectory), Directories);
  DirectoriesLayout->addWidget(Buddy(tr("&Local directory:"), FLocalDirectory, Directories));
  DirectoriesLayout->addWidget(BrowseRow(FLocalDirectory, tr("Local directory"), true));
  QLabel * Explorer = new QLabel(tr("Local directory is not used with Explorer interface."), Directories);
  Explorer->setWordWrap(true);
  DirectoriesLayout->addWidget(Explorer);
  FUpdateDirectories = new QCheckBox(tr("Re&member last used directory"), Directories);
  FUpdateDirectories->setChecked(FSite.UpdateDirectories);
  DirectoriesLayout->addWidget(FUpdateDirectories);
  Layout->addWidget(Directories);
  QGroupBox * Options = new QGroupBox(tr("Directory reading options"), Page);
  QVBoxLayout * OptionsLayout = new QVBoxLayout(Options);
  FCacheDirectories = new QCheckBox(tr("Cache &visited remote directories"), Options);
  FCacheDirectories->setChecked(FSite.CacheDirectories);
  FCacheDirectoryChanges = new QCheckBox(tr("Cache &directory changes"), Options);
  FCacheDirectoryChanges->setChecked(FSite.CacheDirectoryChanges);
  FPreserveDirectoryChanges = new QCheckBox(tr("&Permanent cache"), Options);
  FPreserveDirectoryChanges->setChecked(FSite.PreserveDirectoryChanges);
  FResolveSymlinks = new QCheckBox(tr("Resolve symbolic li&nks"), Options);
  FResolveSymlinks->setChecked(FSite.ResolveSymlinks);
  FFollowDirectorySymlinks = new QCheckBox(tr("&Follow symbolic links to directories"), Options);
  FFollowDirectorySymlinks->setChecked(FSite.FollowDirectorySymlinks);
  for (QCheckBox * Check : { FCacheDirectories, FCacheDirectoryChanges, FPreserveDirectoryChanges, FResolveSymlinks,
         FFollowDirectorySymlinks })
  {
    OptionsLayout->addWidget(Check);
  }
  FPreserveDirectoryChanges->setContentsMargins(20, 0, 0, 0);
  Layout->addWidget(Options);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeRecycleBinPage()
{
  QWidget * Page = AddPage(RecycleBinPage, tr("Recycle bin"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Recycle bin"), Page);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  FDeleteToRecycleBin = new QCheckBox(tr("&Preserve deleted remote files to recycle bin"), Group);
  FDeleteToRecycleBin->setChecked(FSite.DeleteToRecycleBin);
  FOverwrittenToRecycleBin = new QCheckBox(tr("Preserve &overwritten remote files to recycle bin (SFTP only)"), Group);
  FOverwrittenToRecycleBin->setChecked(FSite.OverwrittenToRecycleBin);
  FRecycleBinPath = new QLineEdit(!FSite.RecycleBinPath.empty() ? Q(FSite.RecycleBinPath) : DefaultRecycleBinPath, Group);
  GroupLayout->addWidget(FDeleteToRecycleBin);
  GroupLayout->addWidget(FOverwrittenToRecycleBin);
  GroupLayout->addWidget(Buddy(tr("&Remote recycle bin:"), FRecycleBinPath, Group));
  GroupLayout->addWidget(FRecycleBinPath);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeEncryptionPage()
{
  QWidget * Page = AddPage(EncryptionPage, tr("Encryption"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  FEncryptFiles = new QCheckBox(tr("&Encrypt files"), Page);
  FEncryptFiles->setChecked(!FSite.EncryptKey.empty());
  Layout->addWidget(FEncryptFiles);
  FEncryptGroup = new QGroupBox(tr("Encryption options"), Page);
  QGridLayout * Grid = new QGridLayout(FEncryptGroup);
  FEncryptKey = new QLineEdit(Q(FSite.EncryptKey), FEncryptGroup);
  FEncryptKey->setEchoMode(QLineEdit::Password);
  FEncryptKey->setMaxLength(64);
  Grid->addWidget(Buddy(tr("Encryption &key:"), FEncryptKey, FEncryptGroup), 0, 0, 1, 2);
  Grid->addWidget(FEncryptKey, 1, 0, 1, 2);
  FShowEncryptKey = new QCheckBox(tr("&Show key"), FEncryptGroup);
  connect(FShowEncryptKey, &QCheckBox::toggled, this, [this](bool Show)
  {
    FEncryptKey->setEchoMode(Show ? QLineEdit::Normal : QLineEdit::Password);
  });
  Grid->addWidget(FShowEncryptKey, 2, 0);
  QPushButton * Generate = new QPushButton(tr("&Generate Key"), FEncryptGroup);
  connect(Generate, &QPushButton::clicked, this, &SiteAdvanced::GenerateEncryptKey);
  Grid->addWidget(Generate, 2, 1, Qt::AlignRight);
  Layout->addWidget(FEncryptGroup);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeSftpPage()
{
  QWidget * Page = AddPage(SftpPage, tr("SFTP"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Protocol = new QGroupBox(tr("Protocol options"), Page);
  QFormLayout * Form = new QFormLayout(Protocol);
  FSftpServer = EditableCombo(Protocol, { tr("Default"), QStringLiteral("/bin/sftp-server"),
    QStringLiteral("sudo su -c /bin/sftp-server") }, Q(FSite.SftpServer));
  Form->addRow(tr("SFTP ser&ver:"), FSftpServer);
  FSftpMaxVersion = new QComboBox(Protocol);
  FSftpMaxVersion->addItem(tr("Auto"));
  for (int Version = 0; Version <= 6; Version++)
  {
    FSftpMaxVersion->addItem(QString::number(Version));
  }
  FSftpMaxVersion->setCurrentIndex(FSite.SftpMaxVersion + 1);
  Form->addRow(tr("&Preferred SFTP protocol version:"), FSftpMaxVersion);
  FSftpRealPath = AutoSwitchCombo(Protocol, FSite.SftpRealPath);
  Form->addRow(tr("Ca&nonicalize paths on the server"), FSftpRealPath);
  FAllowScpFallback = new QCheckBox(tr("Allow SCP &fallback"), Protocol);
  FAllowScpFallback->setChecked(FSite.AllowScpFallback);
  Form->addRow(FAllowScpFallback);
  FUsePosixRename = new QCheckBox(tr("&Use POSIX rename"), Protocol);
  FUsePosixRename->setChecked(FSite.UsePosixRename);
  Form->addRow(FUsePosixRename);
  Layout->addWidget(Protocol);
  QGroupBox * Bugs = new QGroupBox(tr("Detection of known bugs in SFTP servers"), Page);
  QFormLayout * BugsForm = new QFormLayout(Bugs);
  FSftpBugSymlink = AutoSwitchCombo(Bugs, FSite.SftpBugSymlink);
  BugsForm->addRow(tr("&Reverses order of link command arguments:"), FSftpBugSymlink);
  FSftpBugSignedTS = AutoSwitchCombo(Bugs, FSite.SftpBugSignedTS);
  BugsForm->addRow(tr("&Misinterprets file timestamps prior to 1970:"), FSftpBugSignedTS);
  Layout->addWidget(Bugs);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeScpPage()
{
  // "Shell" for SFTP (with SCP as its fallback), "SCP/Shell" for SCP.
  QWidget * Page = AddPage(ScpPage, CoreText(FScp ? CoreBridge::TText::ScpShellPage : CoreBridge::TText::ShellPage), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * ShellGroup = new QGroupBox(tr("Shell"), Page);
  QFormLayout * Form = new QFormLayout(ShellGroup);
  FShell = EditableCombo(ShellGroup, { tr("Default"), QStringLiteral("/bin/bash"), QStringLiteral("/bin/ksh"),
    QStringLiteral("/bin/sh"), QStringLiteral("sudo su -") }, Q(FSite.Shell));
  Form->addRow(tr("S&hell:"), FShell);
  FReturnVar = EditableCombo(ShellGroup, { tr("Autodetect"), QStringLiteral("?"), QStringLiteral("status") },
    Q(FSite.ReturnVar));
  Form->addRow(tr("&Return code variable:"), FReturnVar);
  Layout->addWidget(ShellGroup);
  // As upstream: only for SCP itself, not SFTP's fallback.
  FLsGroup = new QGroupBox(tr("Directory listing"), Page);
  QFormLayout * LsForm = new QFormLayout(FLsGroup);
  FListingCommand = new QComboBox(FLsGroup);
  FListingCommand->setEditable(true);
  FListingCommand->addItems({ QStringLiteral("ls -la"), QStringLiteral("ls -gla") });
  FListingCommand->setCurrentText(Q(FSite.ListingCommand));
  LsForm->addRow(tr("Listing &command:"), FListingCommand);
  FIgnoreLsWarnings = new QCheckBox(tr("Ignore LS &warnings"), FLsGroup);
  FIgnoreLsWarnings->setChecked(FSite.IgnoreLsWarnings);
  LsForm->addRow(FIgnoreLsWarnings);
  FScpLsFullTime = new QCheckBox(tr("Try to get &full timestamp"), FLsGroup);
  FScpLsFullTime->setChecked(FSite.ScpLsFullTime);
  LsForm->addRow(FScpLsFullTime);
  Layout->addWidget(FLsGroup);
  FOtherShellGroup = new QGroupBox(tr("Other options"), Page);
  QVBoxLayout * Other = new QVBoxLayout(FOtherShellGroup);
  FLookupUserGroups = new QCheckBox(tr("Lookup &user groups"), FOtherShellGroup);
  // CheckBoxAutoSwitchLoad: partly checked for Auto.
  FLookupUserGroups->setTristate(true);
  FLookupUserGroups->setCheckState((FSite.LookupUserGroups == TAutoSwitch::On) ? Qt::Checked :
    ((FSite.LookupUserGroups == TAutoSwitch::Off) ? Qt::Unchecked : Qt::PartiallyChecked));
  FClearAliases = new QCheckBox(tr("Clear a&liases"), FOtherShellGroup);
  FClearAliases->setChecked(FSite.ClearAliases);
  FUnsetNationalVars = new QCheckBox(tr("Clear &national variables"), FOtherShellGroup);
  FUnsetNationalVars->setChecked(FSite.UnsetNationalVars);
  FScp1Compatibility = new QCheckBox(tr("Use scp&2 with scp1 compatibility"), FOtherShellGroup);
  FScp1Compatibility->setChecked(FSite.Scp1Compatibility);
  for (QCheckBox * Check : { FLookupUserGroups, FClearAliases, FUnsetNationalVars, FScp1Compatibility })
  {
    Other->addWidget(Check);
  }
  Layout->addWidget(FOtherShellGroup);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeFtpPage()
{
  QWidget * Page = AddPage(FtpPage, tr("FTP"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Protocol options"), Page);
  QFormLayout * Form = new QFormLayout(Group);
  FPostLoginCommands = new QPlainTextEdit(Q(FSite.PostLoginCommands), Group);
  FPostLoginCommands->setTabChangesFocus(true);
  FPostLoginCommands->setMaximumHeight(70);
  QLabel * Commands = Buddy(tr("Post login &commands:"), FPostLoginCommands, Group);
  Form->addRow(Commands);
  Form->addRow(FPostLoginCommands);
  FFtpAccount = new QLineEdit(Q(FSite.FtpAccount), Group);
  FFtpAccount->setMaxLength(100);
  Form->addRow(tr("&Account:"), FFtpAccount);
  FFtpListAll = AutoSwitchCombo(Group, FSite.FtpListAll);
  Form->addRow(tr("&Support for listing of hidden files:"), FFtpListAll);
  FFtpUseMlsd = AutoSwitchCombo(Group, FSite.FtpUseMlsd);
  Form->addRow(tr("Use &MLSD command for directory listing"), FFtpUseMlsd);
  FFtpForcePasvIp = AutoSwitchCombo(Group, FSite.FtpForcePasvIp);
  Form->addRow(tr("&Force IP address for passive mode connections:"), FFtpForcePasvIp);
  FFtpHost = AutoSwitchCombo(Group, FSite.FtpHost);
  Form->addRow(tr("Use &HOST command to select host on the server"), FFtpHost);
  FVMSAllRevisions = new QCheckBox(tr("Display all file &revisions on VMS servers"), Group);
  FVMSAllRevisions->setChecked(FSite.VMSAllRevisions);
  Form->addRow(FVMSAllRevisions);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeS3Page()
{
  QWidget * Page = AddPage(S3Page, tr("S3"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Protocol options"), Page);
  QFormLayout * Form = new QFormLayout(Group);
  FS3DefaultRegion = new QComboBox(Group);
  FS3DefaultRegion->setEditable(true);
  FS3DefaultRegion->addItems({ QStringLiteral("af-south-1"), QStringLiteral("ap-east-1"), QStringLiteral("ap-east-2"),
    QStringLiteral("ap-northeast-1"), QStringLiteral("ap-northeast-2"), QStringLiteral("ap-northeast-3"),
    QStringLiteral("ap-south-1"), QStringLiteral("ap-south-2"), QStringLiteral("ap-southeast-1"),
    QStringLiteral("ap-southeast-2"), QStringLiteral("ap-southeast-3"), QStringLiteral("ap-southeast-4"),
    QStringLiteral("ap-southeast-5"), QStringLiteral("ap-southeast-6"), QStringLiteral("ap-southeast-7"),
    QStringLiteral("ca-central-1"), QStringLiteral("ca-west-1"), QStringLiteral("cn-north-1"),
    QStringLiteral("cn-northwest-1"), QStringLiteral("eu-central-1"), QStringLiteral("eu-central-2"),
    QStringLiteral("eu-north-1"), QStringLiteral("eu-south-1"), QStringLiteral("eu-south-2"), QStringLiteral("eu-west-1"),
    QStringLiteral("eu-west-2"), QStringLiteral("eu-west-3"), QStringLiteral("il-central-1"), QStringLiteral("me-central-1"),
    QStringLiteral("me-south-1"), QStringLiteral("mx-central-1"), QStringLiteral("sa-east-1"), QStringLiteral("us-east-1"),
    QStringLiteral("us-east-2"), QStringLiteral("us-gov-east-1"), QStringLiteral("us-gov-west-1"), QStringLiteral("us-west-1"),
    QStringLiteral("us-west-2") });
  FS3DefaultRegion->setCurrentText(Q(FSite.S3DefaultRegion));
  Form->addRow(tr("&Default region:"), FS3DefaultRegion);
  FS3UrlStyle = new QComboBox(Group);
  FS3UrlStyle->addItems({ tr("Virtual Host"), tr("Path") });
  FS3UrlStyle->setCurrentIndex(FSite.S3UrlStylePath ? 1 : 0);
  Form->addRow(tr("&URL style:"), FS3UrlStyle);
  FS3RequesterPays = new QCheckBox(tr("Requester &pays"), Group);
  FS3RequesterPays->setChecked(FSite.S3RequesterPays);
  Form->addRow(FS3RequesterPays);
  Layout->addWidget(Group);
  QGroupBox * Authentication = new QGroupBox(tr("Authentication"), Page);
  QVBoxLayout * AuthenticationLayout = new QVBoxLayout(Authentication);
  // As LoadSession: with automatic credentials, the environment's.
  FS3SessionToken = new QPlainTextEdit(FS3 ? Q(FS3Credentials.SessionToken) : Q(FSite.S3SessionToken), Authentication);
  FS3SessionToken->setTabChangesFocus(true);
  FS3SessionToken->setMaximumHeight(70);
  AuthenticationLayout->addWidget(Buddy(tr("&Session token:"), FS3SessionToken, Authentication));
  AuthenticationLayout->addWidget(FS3SessionToken);
  FS3RoleArn = new QLineEdit(FS3 ? Q(FS3Credentials.RoleArn) : Q(FSite.S3RoleArn), Authentication);
  AuthenticationLayout->addWidget(Buddy(tr("&Role ARN:"), FS3RoleArn, Authentication));
  AuthenticationLayout->addWidget(FS3RoleArn);
  Layout->addWidget(Authentication);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeWebDavPage()
{
  QWidget * Page = AddPage(WebDavPage, tr("WebDAV"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Protocol options"), Page);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  FWebDavLiberalEscaping = new QCheckBox(tr("&Tolerate non-encoded special characters in filenames"), Group);
  FWebDavLiberalEscaping->setChecked(FSite.WebDavLiberalEscaping);
  FWebDavCrossDomainRedirects = new QCheckBox(tr("&Allow redirects to other hosts"), Group);
  FWebDavCrossDomainRedirects->setChecked(FSite.WebDavCrossDomainRedirects);
  GroupLayout->addWidget(FWebDavLiberalEscaping);
  GroupLayout->addWidget(FWebDavCrossDomainRedirects);
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeConnectionPage()
{
  QWidget * Page = AddPage(ConnectionPage, tr("Connection"), false);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Connection = new QGroupBox(tr("Connection"), Page);
  QVBoxLayout * ConnectionLayout = new QVBoxLayout(Connection);
  FFtpPassive = new QCheckBox(tr("&Passive mode"), Connection);
  FFtpPassive->setChecked(FSite.FtpPassive);
  FOptimizeBuffers = new QCheckBox(tr("Optimize connection &buffer size"), Connection);
  FOptimizeBuffers->setChecked(FSite.OptimizeBuffers);
  ConnectionLayout->addWidget(FFtpPassive);
  ConnectionLayout->addWidget(FOptimizeBuffers);
  Layout->addWidget(Connection);
  QGroupBox * Timeouts = new QGroupBox(tr("Timeouts"), Page);
  QHBoxLayout * TimeoutsLayout = new QHBoxLayout(Timeouts);
  FTimeout = new QSpinBox(Timeouts);
  FTimeout->setRange(5, 6000);
  FTimeout->setSingleStep(5);
  FTimeout->setValue(FSite.Timeout);
  TimeoutsLayout->addWidget(Buddy(tr("Server &response timeout:"), FTimeout, Timeouts));
  TimeoutsLayout->addWidget(FTimeout);
  TimeoutsLayout->addWidget(new QLabel(tr("seconds"), Timeouts));
  TimeoutsLayout->addStretch(1);
  Layout->addWidget(Timeouts);
  // SSH's keepalives, or FTP's.
  FPingGroup = new QGroupBox(tr("Keepalives"), Page);
  QVBoxLayout * Ping = new QVBoxLayout(FPingGroup);
  FPingOff = new QRadioButton(tr("&Off"), FPingGroup);
  FPingNullPacket = new QRadioButton(tr("Sending of &null SSH packets"), FPingGroup);
  FPingDummyCommand = new QRadioButton(tr("Executing &dummy protocol commands"), FPingGroup);
  ((FSite.Ping == TSite::TPing::NullPacket) ? FPingNullPacket :
   ((FSite.Ping == TSite::TPing::DummyCommand) ? FPingDummyCommand : FPingOff))->setChecked(true);
  FPingInterval = new QSpinBox(FPingGroup);
  FPingInterval->setRange(1, 3600);
  FPingInterval->setValue(FSite.PingInterval);
  for (QRadioButton * Radio : { FPingOff, FPingNullPacket, FPingDummyCommand })
  {
    Ping->addWidget(Radio);
  }
  QHBoxLayout * PingRow = new QHBoxLayout();
  PingRow->addWidget(Buddy(tr("Seconds &between keepalives:"), FPingInterval, FPingGroup));
  PingRow->addWidget(FPingInterval);
  PingRow->addStretch(1);
  Ping->addLayout(PingRow);
  Layout->addWidget(FPingGroup);
  FFtpPingGroup = new QGroupBox(tr("Keepalives"), Page);
  QVBoxLayout * FtpPing = new QVBoxLayout(FFtpPingGroup);
  FFtpPingOff = new QRadioButton(tr("&Off"), FFtpPingGroup);
  FFtpPingDummyCommand = new QRadioButton(tr("Executing &dummy protocol commands"), FFtpPingGroup);
  FFtpPingDirectoryListing = new QRadioButton(tr("&And additionally reading the current directory"), FFtpPingGroup);
  ((FSite.FtpPing == TSite::TFtpPing::DummyCommand) ? FFtpPingDummyCommand :
   ((FSite.FtpPing == TSite::TFtpPing::DirectoryListing) ? FFtpPingDirectoryListing : FFtpPingOff))->setChecked(true);
  FFtpPingInterval = new QSpinBox(FFtpPingGroup);
  FFtpPingInterval->setRange(1, 3600);
  FFtpPingInterval->setValue(FSite.FtpPingInterval);
  for (QRadioButton * Radio : { FFtpPingOff, FFtpPingDummyCommand, FFtpPingDirectoryListing })
  {
    FtpPing->addWidget(Radio);
  }
  FFtpPingDirectoryListing->setContentsMargins(20, 0, 0, 0);
  QHBoxLayout * FtpPingRow = new QHBoxLayout();
  FtpPingRow->addWidget(Buddy(tr("Seconds &between keepalives:"), FFtpPingInterval, FFtpPingGroup));
  FtpPingRow->addWidget(FFtpPingInterval);
  FtpPingRow->addStretch(1);
  FtpPing->addLayout(FtpPingRow);
  Layout->addWidget(FFtpPingGroup);
  FIpGroup = new QGroupBox(tr("Internet protocol version"), Page);
  QHBoxLayout * Ip = new QHBoxLayout(FIpGroup);
  FIPAuto = new QRadioButton(tr("A&uto"), FIpGroup);
  FIPv4 = new QRadioButton(tr("IPv&4"), FIpGroup);
  FIPv6 = new QRadioButton(tr("IPv&6"), FIpGroup);
  ((FSite.AddressFamily == TSite::TAddressFamily::IPv4) ? FIPv4 :
   ((FSite.AddressFamily == TSite::TAddressFamily::IPv6) ? FIPv6 : FIPAuto))->setChecked(true);
  for (QRadioButton * Radio : { FIPAuto, FIPv4, FIPv6 })
  {
    Ip->addWidget(Radio);
  }
  Ip->addStretch(1);
  Layout->addWidget(FIpGroup);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeProxyPage()
{
  QWidget * Page = AddPage(ProxyPage, tr("Proxy"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Type = new QGroupBox(tr("Proxy"), Page);
  QGridLayout * Grid = new QGridLayout(Type);
  // The methods the protocol's code supports: SshProxyMethodCombo,
  // FtpProxyMethodCombo (with its logon types) or NeonProxyMethodCombo.
  FProxy = new QComboBox(Type);
  FProxy->addItem(tr("None"), static_cast<int>(TProxy::None));
  FProxy->addItem(QStringLiteral("SOCKS4"), static_cast<int>(TProxy::Socks4));
  FProxy->addItem(QStringLiteral("SOCKS5"), static_cast<int>(TProxy::Socks5));
  FProxy->addItem(QStringLiteral("HTTP"), static_cast<int>(TProxy::Http));
  int Current = static_cast<int>(FSite.Proxy);
  if (FSsh)
  {
    FProxy->addItem(QStringLiteral("Telnet"), static_cast<int>(TProxy::Telnet));
    FProxy->addItem(tr("Local"), static_cast<int>(TProxy::Command));
  }
  else
  {
    if (FFtp)
    {
      int LogonType = 1;
      for (const char * Logon : { "SITE %host", "USER %proxyuser, USER %user@%host", "OPEN %host",
             "USER %proxyuser, USER %user", "USER %user@%host", "USER %proxyuser@%host", "USER %user@%host %proxyuser",
             "USER %user@%proxyuser@%host" })
      {
        FProxy->addItem(QString::fromLatin1(Logon), FtpLogonTypeBase + LogonType++);
      }
      if (FSite.FtpProxyLogonType != 0)
      {
        Current = FtpLogonTypeBase + FSite.FtpProxyLogonType;
      }
    }
    // A method the protocol does not support: none.
    if ((Current > static_cast<int>(TProxy::Http)) && (Current < FtpLogonTypeBase))
    {
      Current = static_cast<int>(TProxy::None);
    }
  }
  FProxy->setCurrentIndex(std::max(0, FProxy->findData(Current)));
  Grid->addWidget(Buddy(tr("Proxy &type:"), FProxy, Type), 0, 0);
  Grid->addWidget(FProxy, 1, 0);
  QPushButton * Autodetect = new QPushButton(tr("&Autodetect"), Type);
  connect(Autodetect, &QPushButton::clicked, this, &SiteAdvanced::ProxyAutodetect);
  Grid->addWidget(Autodetect, 1, 1);
  FProxyHost = new QLineEdit(Q(FSite.ProxyHost), Type);
  FProxyHost->setMaxLength(255);
  Grid->addWidget(Buddy(tr("Pro&xy hostname:"), FProxyHost, Type), 2, 0);
  Grid->addWidget(FProxyHost, 3, 0);
  FProxyPort = new QSpinBox(Type);
  FProxyPort->setRange(1, 65535);
  // As the Proxy page: the site's port, whatever the type (core's default 80).
  FProxyPort->setValue((FSite.ProxyPort > 0) ? FSite.ProxyPort : 80);
  Grid->addWidget(Buddy(tr("Po&rt number:"), FProxyPort, Type), 2, 1);
  Grid->addWidget(FProxyPort, 3, 1);
  FProxyUserName = new QLineEdit(Q(FSite.ProxyUserName), Type);
  FProxyUserName->setMaxLength(100);
  Grid->addWidget(Buddy(tr("&Username:"), FProxyUserName, Type), 4, 0);
  Grid->addWidget(FProxyUserName, 5, 0);
  FProxyPassword = new QLineEdit(Q(FSite.ProxyPassword), Type);
  FProxyPassword->setEchoMode(QLineEdit::Password);
  FProxyPassword->setMaxLength(100);
  Grid->addWidget(Buddy(tr("&Password:"), FProxyPassword, Type), 4, 1);
  Grid->addWidget(FProxyPassword, 5, 1);
  Grid->setColumnStretch(0, 1);
  Layout->addWidget(Type);

  // SSH's proxy settings.
  FProxySettings = new QGroupBox(tr("Proxy settings"), Page);
  QVBoxLayout * Settings = new QVBoxLayout(FProxySettings);
  const QString Patterns = CoreText(CoreBridge::TText::ProxyCommandPatternsHint);
  const auto PatternsLabel = [&Patterns](QWidget * Parent)
  {
    // HintLabel: "patterns", the patterns its hint.
    QLabel * Label = new QLabel(tr("patterns"), Parent);
    Label->setToolTip(Patterns);
    Label->setCursor(Qt::WhatsThisCursor);
    return Label;
  };
  FProxyTelnetRow = new QWidget(FProxySettings);
  QHBoxLayout * TelnetRow = new QHBoxLayout(FProxyTelnetRow);
  TelnetRow->setContentsMargins(0, 0, 0, 0);
  FProxyTelnetCommand = new QLineEdit(Q(FSite.ProxyTelnetCommand), FProxyTelnetRow);
  FProxyTelnetCommand->setMaxLength(255);
  FProxyTelnetCommand->setAccessibleName(tr("Telnet command"));
  TelnetRow->addWidget(FProxyTelnetCommand, 1);
  TelnetRow->addWidget(PatternsLabel(FProxyTelnetRow));
  FProxyLocalRow = new QWidget(FProxySettings);
  QHBoxLayout * LocalRow = new QHBoxLayout(FProxyLocalRow);
  LocalRow->setContentsMargins(0, 0, 0, 0);
  FProxyLocalCommand = new QLineEdit(Q(FSite.ProxyLocalCommand), FProxyLocalRow);
  FProxyLocalCommand->setAccessibleName(tr("Local proxy command"));
  LocalRow->addWidget(BrowseRow(FProxyLocalCommand, tr("Local proxy command"), false), 1);
  LocalRow->addWidget(PatternsLabel(FProxyLocalRow));
  FProxyCommandLabel = new QLabel(FProxySettings);
  Settings->addWidget(FProxyCommandLabel);
  Settings->addWidget(FProxyTelnetRow);
  Settings->addWidget(FProxyLocalRow);
  FProxyLocalhost = new QCheckBox(tr("Co&nsider proxying local host connections"), FProxySettings);
  FProxyLocalhost->setChecked(FSite.ProxyLocalhost);
  Settings->addWidget(FProxyLocalhost);
  QHBoxLayout * DnsRow = new QHBoxLayout();
  FProxyDns = new QComboBox(FProxySettings);
  // ProxyDNSCombo: Auto, No, Yes, as the AutoSwitch's 2 - value.
  FProxyDns->addItems({ tr("Auto"), tr("No"), tr("Yes") });
  SetAutoSwitch(FProxyDns, FSite.ProxyDns);
  DnsRow->addWidget(Buddy(tr("Do &DNS name lookup at proxy end:"), FProxyDns, FProxySettings));
  DnsRow->addWidget(FProxyDns);
  DnsRow->addStretch(1);
  Settings->addLayout(DnsRow);
  Layout->addWidget(FProxySettings);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeTunnelPage()
{
  QWidget * Page = AddPage(TunnelPage, tr("Tunnel"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  FTunnel = new QCheckBox(tr("Connect &through SSH tunnel"), Page);
  FTunnel->setChecked(FSite.Tunnel);
  Layout->addWidget(FTunnel);
  FTunnelSession = new QGroupBox(tr("Host to setup tunnel on"), Page);
  QGridLayout * Grid = new QGridLayout(FTunnelSession);
  FTunnelHostName = new QLineEdit(Q(FSite.TunnelHostName), FTunnelSession);
  FTunnelHostName->setMaxLength(255);
  Grid->addWidget(Buddy(tr("&Hostname:"), FTunnelHostName, FTunnelSession), 0, 0);
  Grid->addWidget(FTunnelHostName, 1, 0);
  FTunnelPortNumber = new QSpinBox(FTunnelSession);
  FTunnelPortNumber->setRange(1, 65535);
  FTunnelPortNumber->setValue(FSite.TunnelPortNumber);
  Grid->addWidget(Buddy(tr("Po&rt number:"), FTunnelPortNumber, FTunnelSession), 0, 1);
  Grid->addWidget(FTunnelPortNumber, 1, 1);
  FTunnelUserName = new QLineEdit(Q(FSite.TunnelUserName), FTunnelSession);
  FTunnelUserName->setMaxLength(100);
  Grid->addWidget(Buddy(tr("&Username:"), FTunnelUserName, FTunnelSession), 2, 0);
  Grid->addWidget(FTunnelUserName, 3, 0);
  FTunnelPassword = new QLineEdit(Q(FSite.TunnelPassword), FTunnelSession);
  FTunnelPassword->setEchoMode(QLineEdit::Password);
  FTunnelPassword->setMaxLength(100);
  Grid->addWidget(Buddy(tr("&Password:"), FTunnelPassword, FTunnelSession), 2, 1);
  Grid->addWidget(FTunnelPassword, 3, 1);
  Grid->setColumnStretch(0, 1);
  Layout->addWidget(FTunnelSession);
  FTunnelOptions = new QGroupBox(tr("Tunnel options"), Page);
  QHBoxLayout * Options = new QHBoxLayout(FTunnelOptions);
  FTunnelLocalPort = new QComboBox(FTunnelOptions);
  FTunnelLocalPort->setEditable(true);
  FTunnelLocalPort->addItem(tr("Autoselect"));
  for (int Port = CoreBridge::TunnelLocalPortNumberLow(); Port <= CoreBridge::TunnelLocalPortNumberHigh(); Port++)
  {
    FTunnelLocalPort->addItem(QString::number(Port));
  }
  FTunnelLocalPort->setCurrentText((FSite.TunnelLocalPortNumber > 0) ? QString::number(FSite.TunnelLocalPortNumber) :
    FTunnelLocalPort->itemText(0));
  Options->addWidget(Buddy(tr("&Local tunnel port:"), FTunnelLocalPort, FTunnelOptions));
  Options->addWidget(FTunnelLocalPort);
  Options->addStretch(1);
  Layout->addWidget(FTunnelOptions);
  FTunnelAuthentication = new QGroupBox(tr("Tunnel authentication parameters"), Page);
  QVBoxLayout * Authentication = new QVBoxLayout(FTunnelAuthentication);
  FTunnelPrivateKeyFile = new QLineEdit(Q(FSite.TunnelPrivateKeyFile), FTunnelAuthentication);
  Authentication->addWidget(Buddy(tr("Private &key file:"), FTunnelPrivateKeyFile, FTunnelAuthentication));
  Authentication->addWidget(BrowseRow(FTunnelPrivateKeyFile, tr("Private key file"), false,
    tr("PuTTY Private Key Files (*.ppk);;All Private Key Files (*.ppk *.pem *.key id_*);;All Files (*)")));
  Layout->addWidget(FTunnelAuthentication);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeTlsPage()
{
  QWidget * Page = AddPage(TlsPage, tr("TLS/SSL"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("TLS options"), Page);
  QFormLayout * Form = new QFormLayout(Group);
  const QStringList Versions = { QStringLiteral("TLS 1.0"), QStringLiteral("TLS 1.1"), QStringLiteral("TLS 1.2"),
    QStringLiteral("TLS 1.3") };
  FMinTls = new QComboBox(Group);
  FMinTls->addItems(Versions);
  FMinTls->setCurrentIndex(static_cast<int>(FSite.MinTls));
  FMaxTls = new QComboBox(Group);
  FMaxTls->addItems(Versions);
  FMaxTls->setCurrentIndex(static_cast<int>(FSite.MaxTls));
  // MinTlsVersionComboChange, MaxTlsVersionComboChange: kept in order.
  connect(FMinTls, &QComboBox::currentIndexChanged, this, [this](int Index)
  {
    if (FMaxTls->currentIndex() < Index)
    {
      FMaxTls->setCurrentIndex(Index);
    }
  });
  connect(FMaxTls, &QComboBox::currentIndexChanged, this, [this](int Index)
  {
    if (FMinTls->currentIndex() > Index)
    {
      FMinTls->setCurrentIndex(Index);
    }
  });
  Form->addRow(tr("Mi&nimum TLS version:"), FMinTls);
  Form->addRow(tr("Ma&ximum TLS version:"), FMaxTls);
  FSslSessionReuse = new QCheckBox(tr("&Reuse TLS session ID for data connections"), Group);
  FSslSessionReuse->setChecked(FSite.SslSessionReuse);
  Form->addRow(FSslSessionReuse);
  Layout->addWidget(Group);
  FTlsAuthentication = new QGroupBox(tr("Authentication parameters"), Page);
  QVBoxLayout * Authentication = new QVBoxLayout(FTlsAuthentication);
  FTlsCertificateFile = new QLineEdit(Q(FSite.TlsCertificateFile), FTlsAuthentication);
  Authentication->addWidget(Buddy(tr("&Client certificate file:"), FTlsCertificateFile, FTlsAuthentication));
  Authentication->addWidget(BrowseRow(FTlsCertificateFile, tr("Client certificate file"), false,
    tr("Certificate Files (*.pfx *.p12 *.pem *.crt *.key);;All Files (*)")));
  Layout->addWidget(FTlsAuthentication);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeSshPage()
{
  QWidget * Page = AddPage(SshPage, tr("SSH"), false);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Protocol = new QGroupBox(tr("Protocol options"), Page);
  QVBoxLayout * ProtocolLayout = new QVBoxLayout(Protocol);
  FCompression = new QCheckBox(tr("&Enable compression"), Protocol);
  FCompression->setChecked(FSite.Compression);
  ProtocolLayout->addWidget(FCompression);
  Layout->addWidget(Protocol);
  QGroupBox * Encryption = new QGroupBox(tr("Encryption options"), Page);
  QGridLayout * Grid = new QGridLayout(Encryption);
  FCiphers = AlgorithmList(Encryption, CoreBridge::CipherNames(), FSite.Ciphers);
  Grid->addWidget(Buddy(tr("Encryption cipher selection &policy:"), FCiphers, Encryption), 0, 0, 1, 2);
  FCiphers->setAccessibleName(tr("Encryption cipher selection policy"));
  Grid->addWidget(FCiphers, 1, 0, 3, 1);
  FCipherUp = new QPushButton(tr("&Up"), Encryption);
  FCipherDown = new QPushButton(tr("&Down"), Encryption);
  Grid->addWidget(FCipherUp, 1, 1);
  Grid->addWidget(FCipherDown, 2, 1);
  Grid->setRowStretch(3, 1);
  FSsh2DES = new QCheckBox(tr("Enable legacy use of single-&DES"), Encryption);
  FSsh2DES->setChecked(FSite.Ssh2DES);
  Grid->addWidget(FSsh2DES, 4, 0, 1, 2);
  Layout->addWidget(Encryption, 1);
  for (QPushButton * Button : { FCipherUp, FCipherDown })
  {
    const int Step = (Button == FCipherUp) ? -1 : 1;
    connect(Button, &QPushButton::clicked, this, [this, Step]()
    {
      const int Row = FCiphers->currentRow();
      if ((Row + Step >= 0) && (Row + Step < FCiphers->count()))
      {
        FCiphers->insertItem(Row + Step, FCiphers->takeItem(Row));
        FCiphers->setCurrentRow(Row + Step);
      }
    });
  }
  return Page;
}

QWidget * SiteAdvanced::MakeKexPage()
{
  QWidget * Page = AddPage(KexPage, tr("Key exchange"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Options = new QGroupBox(tr("Key exchange algorithm options"), Page);
  QGridLayout * Grid = new QGridLayout(Options);
  FKex = AlgorithmList(Options, CoreBridge::KexNames(), FSite.Kex);
  Grid->addWidget(Buddy(tr("Algorithm selection &policy:"), FKex, Options), 0, 0, 1, 2);
  FKex->setAccessibleName(tr("Algorithm selection policy"));
  Grid->addWidget(FKex, 1, 0, 3, 1);
  FKexUp = new QPushButton(tr("&Up"), Options);
  FKexDown = new QPushButton(tr("&Down"), Options);
  Grid->addWidget(FKexUp, 1, 1);
  Grid->addWidget(FKexDown, 2, 1);
  Grid->setRowStretch(3, 1);
  FAuthGSSAPIKEX = new QCheckBox(tr("Attempt &GSSAPI key exchange"), Options);
  FAuthGSSAPIKEX->setChecked(FSite.AuthGSSAPIKEX);
  Grid->addWidget(FAuthGSSAPIKEX, 4, 0, 1, 2);
  Layout->addWidget(Options, 1);
  for (QPushButton * Button : { FKexUp, FKexDown })
  {
    const int Step = (Button == FKexUp) ? -1 : 1;
    connect(Button, &QPushButton::clicked, this, [this, Step]()
    {
      const int Row = FKex->currentRow();
      if ((Row + Step >= 0) && (Row + Step < FKex->count()))
      {
        FKex->insertItem(Row + Step, FKex->takeItem(Row));
        FKex->setCurrentRow(Row + Step);
      }
    });
  }
  FKexReexchange = new QGroupBox(tr("Options controlling key re-exchange"), Page);
  QFormLayout * Form = new QFormLayout(FKexReexchange);
  FRekeyTime = new QSpinBox(FKexReexchange);
  FRekeyTime->setRange(0, 1440);
  FRekeyTime->setValue(FSite.RekeyTime);
  Form->addRow(tr("Max &minutes before rekey (0 for no limit):"), FRekeyTime);
  FRekeyData = new QLineEdit(Q(FSite.RekeyData), FKexReexchange);
  FRekeyData->setMaxLength(10);
  Form->addRow(tr("Ma&x data before rekey (0 for no limit):"), FRekeyData);
  Layout->addWidget(FKexReexchange);
  return Page;
}

QWidget * SiteAdvanced::MakeAuthenticationPage()
{
  QWidget * Page = AddPage(AuthenticationPage, tr("Authentication"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  FSshNoUserAuth = new QCheckBox(tr("&Bypass authentication entirely"), Page);
  FSshNoUserAuth->setChecked(FSite.SshNoUserAuth);
  Layout->addWidget(FSshNoUserAuth);
  FAuthenticationGroup = new QGroupBox(tr("Authentication options"), Page);
  QVBoxLayout * Options = new QVBoxLayout(FAuthenticationGroup);
  FTryAgent = new QCheckBox(tr("&Attempt authentication using agent"), FAuthenticationGroup);
  FTryAgent->setChecked(FSite.TryAgent);
  FKeyboardInteractive = new QCheckBox(tr("Attempt 'keyboard-&interactive' authentication"), FAuthenticationGroup);
  FKeyboardInteractive->setChecked(FSite.KeyboardInteractive);
  FKeyboardInteractivePassword = new QCheckBox(tr("Respond with a pass&word to the first prompt"), FAuthenticationGroup);
  FKeyboardInteractivePassword->setChecked(FSite.KeyboardInteractivePassword);
  FKeyboardInteractivePassword->setContentsMargins(20, 0, 0, 0);
  for (QCheckBox * Check : { FTryAgent, FKeyboardInteractive, FKeyboardInteractivePassword })
  {
    Options->addWidget(Check);
  }
  Layout->addWidget(FAuthenticationGroup);
  FAuthenticationParams = new QGroupBox(tr("Authentication parameters"), Page);
  QVBoxLayout * Params = new QVBoxLayout(FAuthenticationParams);
  FAgentForwarding = new QCheckBox(tr("Allow agent &forwarding"), FAuthenticationParams);
  FAgentForwarding->setChecked(FSite.AgentForwarding);
  Params->addWidget(FAgentForwarding);
  FPrivateKeyFile = new QLineEdit(Q(FSite.PrivateKeyFile), FAuthenticationParams);
  Params->addWidget(Buddy(tr("Private &key file:"), FPrivateKeyFile, FAuthenticationParams));
  Params->addWidget(BrowseRow(FPrivateKeyFile, tr("Private key file"), false,
    tr("PuTTY Private Key Files (*.ppk);;All Private Key Files (*.ppk *.pem *.key id_*);;All Files (*)")));
  FShowPublicKey = new QPushButton(tr("&Display Public Key"), FAuthenticationParams);
  connect(FShowPublicKey, &QPushButton::clicked, this, &SiteAdvanced::ShowPublicKey);
  Params->addWidget(FShowPublicKey, 0, Qt::AlignLeft);
  FDetachedCertificate = new QLineEdit(Q(FSite.DetachedCertificate), FAuthenticationParams);
  FDetachedCertificateLabel = Buddy(tr("Certificate to &use with the private key:"), FDetachedCertificate, FAuthenticationParams);
  Params->addWidget(FDetachedCertificateLabel);
  FDetachedCertificateRow = BrowseRow(FDetachedCertificate, tr("Certificate"), false,
    tr("OpenSSH certificates (*-cert.pub);;All Files (*)"));
  Params->addWidget(FDetachedCertificateRow);
  Layout->addWidget(FAuthenticationParams);
  QGroupBox * Gssapi = new QGroupBox(tr("GSSAPI"), Page);
  QVBoxLayout * GssapiLayout = new QVBoxLayout(Gssapi);
  FAuthGSSAPI = new QCheckBox(tr("Attempt &GSSAPI authentication"), Gssapi);
  FAuthGSSAPI->setChecked(FSite.AuthGSSAPI);
  FGSSAPIFwdTGT = new QCheckBox(tr("Allow GSSAPI c&redential delegation"), Gssapi);
  FGSSAPIFwdTGT->setChecked(FSite.GSSAPIFwdTGT);
  FGSSAPIFwdTGT->setContentsMargins(20, 0, 0, 0);
  GssapiLayout->addWidget(FAuthGSSAPI);
  GssapiLayout->addWidget(FGSSAPIFwdTGT);
  Layout->addWidget(Gssapi);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeBugsPage()
{
  QWidget * Page = AddPage(BugsPage, tr("Bugs"), true);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Detection of known bugs in SSH servers"), Page);
  QFormLayout * Form = new QFormLayout(Group);
  const QString Captions[TSite::SshBugCount] = {
    tr("Miscomputes SSH H&MAC keys:"), tr("Miscomputes SSH &encryption keys:"),
    tr("Requires &padding on SSH RSA signatures:"), tr("Misuses the sessio&n ID in SSH PK auth:"),
    tr("Handles SSH &key re-exchange badly:"), tr("Ignores SSH ma&ximum packet size:"),
    tr("Chokes on SSH i&gnore messages:"), tr("Chokes on WinSCP's SSH '&winadj' requests") };
  for (int Index = 0; Index < TSite::SshBugCount; Index++)
  {
    FBugs[Index] = AutoSwitchCombo(Group,
      (Index < static_cast<int>(FSite.SshBugs.size())) ? FSite.SshBugs[Index] : TAutoSwitch::Auto);
    Form->addRow(Captions[Index], FBugs[Index]);
  }
  Layout->addWidget(Group);
  Layout->addStretch(1);
  return Page;
}

QWidget * SiteAdvanced::MakeNotePage()
{
  QWidget * Page = AddPage(NotePage, tr("Note"), false);
  QVBoxLayout * Layout = new QVBoxLayout(Page);
  QGroupBox * Group = new QGroupBox(tr("Note"), Page);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  FNote = new QPlainTextEdit(Q(FSite.Note), Group);
  FNote->setAccessibleName(tr("Note"));
  FNote->setTabChangesFocus(true);
  GroupLayout->addWidget(FNote);
  Layout->addWidget(Group, 1);
  return Page;
}

// As UpdateNavigationTree: the pages of the site's protocol (the sheets
// UpdateControls enables).
void SiteAdvanced::UpdateNavigation()
{
  FPageShown[EnvironmentPage] = true;
  FPageShown[DirectoriesPage] = true;
  FPageShown[RecycleBinPage] = true;
  FPageShown[EncryptionPage] = FSftp;
  FPageShown[SftpPage] = FSftp;
  FPageShown[ScpPage] = FSsh;
  FPageShown[FtpPage] = FFtp;
  FPageShown[S3Page] = FS3;
  FPageShown[WebDavPage] = FWebDav;
  FPageShown[ConnectionPage] = true;
  FPageShown[ProxyPage] = true;
  FPageShown[TunnelPage] = FSsh;
  FPageShown[TlsPage] = FSsl;
  FPageShown[SshPage] = FSsh;
  FPageShown[KexPage] = FSsh;
  FPageShown[AuthenticationPage] = FSsh;
  FPageShown[BugsPage] = FSsh;
  FPageShown[NotePage] = true;
  for (int Page = 0; Page < PageTotal; Page++)
  {
    FPageItems[Page]->setHidden(!FPageShown[Page]);
  }
}

void SiteAdvanced::UpdateControls()
{
  if (FUpdating)
  {
    return;
  }
  FUpdating = true;
  const TProxy Method = ProxyMethod();
  // Connection.
  FFtpPassive->setEnabled(FFtp);
  if (FFtp && (Method != TProxy::None) && (FProxy->currentData().toInt() < FtpLogonTypeBase) && !FFtpPassive->isChecked())
  {
    FFtpPassive->setChecked(true);
    QMessageBox::information(this, windowTitle(), CoreText(CoreBridge::TText::FtpPasvModeRequired));
  }
  FOptimizeBuffers->setEnabled(FSsh || FFtp || FS3);
  FPingGroup->setVisible(!FFtp);
  FPingGroup->setEnabled(FSsh);
  FPingInterval->setEnabled(FSsh && !FPingOff->isChecked());
  FFtpPingGroup->setVisible(FFtp);
  FFtpPingInterval->setEnabled(!FFtpPingOff->isChecked());
  FIpGroup->setEnabled(FSsh || FFtp);
  FIPAuto->setEnabled(FSsh);

  // SSH > Authentication.
  FAuthenticationGroup->setEnabled(!FSshNoUserAuth->isChecked());
  FKeyboardInteractivePassword->setEnabled(FKeyboardInteractive->isChecked());
  FAuthenticationParams->setEnabled(FAuthenticationGroup->isEnabled());
  FAgentForwarding->setEnabled(FTryAgent->isChecked());
  if (FPrivateKeyFile->text() != FLastPrivateKey)
  {
    FLastPrivateKey = FPrivateKeyFile->text();
    FKeyHasCertificate = !FLastPrivateKey.trimmed().isEmpty() && CoreBridge::KeyHasCertificate(FLastPrivateKey.trimmed().toStdString());
  }
  FShowPublicKey->setEnabled(!FPrivateKeyFile->text().trimmed().isEmpty());
  FDetachedCertificateRow->setEnabled(FShowPublicKey->isEnabled() && !FKeyHasCertificate);
  FDetachedCertificateLabel->setEnabled(FDetachedCertificateRow->isEnabled());
  FAuthGSSAPI->setEnabled(FAuthenticationGroup->isEnabled());
  FGSSAPIFwdTGT->setEnabled(FAuthGSSAPI->isChecked());

  // SSH, Key exchange.
  FCipherUp->setEnabled(FCiphers->currentRow() > 0);
  FCipherDown->setEnabled((FCiphers->currentRow() >= 0) && (FCiphers->currentRow() < FCiphers->count() - 1));
  FKexUp->setEnabled(FKex->currentRow() > 0);
  FKexDown->setEnabled((FKex->currentRow() >= 0) && (FKex->currentRow() < FKex->count() - 1));
  // Rekey2's "On": the server cannot re-exchange keys.
  FKexReexchange->setEnabled(FBugs[4]->currentIndex() != 2);

  // Connection > Proxy.
  const int LogonType = FtpProxyLogonType();
  const QString Command = (Method == TProxy::Command) ? FProxyLocalCommand->text() : FProxyTelnetCommand->text();
  const bool Hosted = (Method == TProxy::Socks4) || (Method == TProxy::Socks5) || (Method == TProxy::Http) ||
    (Method == TProxy::Telnet) || (LogonType > 0);
  FProxyHost->setEnabled(Hosted ||
    ((Method == TProxy::Command) && Command.contains(QStringLiteral("%proxyhost"), Qt::CaseInsensitive)));
  FProxyPort->setEnabled(Hosted ||
    ((Method == TProxy::Command) && Command.contains(QStringLiteral("%proxyport"), Qt::CaseInsensitive)));
  const bool Commanded = (Method == TProxy::Telnet) || (Method == TProxy::Command);
  const bool LogonCredentials = (LogonType > 0) && (LogonType != 3) && (LogonType != 5);
  // FZAPI does not support username for SOCKS4.
  FProxyUserName->setEnabled(((Method == TProxy::Socks4) && (FSsh || FNeon)) || (Method == TProxy::Socks5) ||
    (Method == TProxy::Http) || (Commanded && Command.contains(QStringLiteral("%user"), Qt::CaseInsensitive)) ||
    LogonCredentials);
  FProxyPassword->setEnabled((Method == TProxy::Socks5) || (Method == TProxy::Http) ||
    (Commanded && Command.contains(QStringLiteral("%pass"), Qt::CaseInsensitive)) || LogonCredentials);
  const bool ProxySettings = (Method != TProxy::None) && FSsh;
  FProxySettings->setVisible(FSsh);
  FProxySettings->setEnabled(ProxySettings);
  FProxyTelnetRow->setEnabled(ProxySettings && (Method == TProxy::Telnet));
  FProxyLocalRow->setVisible(Method == TProxy::Command);
  FProxyTelnetRow->setVisible(!FProxyLocalRow->isVisibleTo(FProxySettings));
  FProxyCommandLabel->setText((Method == TProxy::Command) ? tr("Local proxy co&mmand:") : tr("Telnet co&mmand:"));
  FProxyCommandLabel->setBuddy((Method == TProxy::Command) ? FProxyLocalCommand : FProxyTelnetCommand);
  FProxyCommandLabel->setEnabled((Method == TProxy::Command) || FProxyTelnetRow->isEnabled());

  // Environment > Directories: synchronized browsing only where the
  // Commander keeps each session's panels (as upstream's conditions).
  const CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
  FSynchronizeBrowsing->setEnabled(!Preferences.ExplorerInterface && Preferences.PreservePanelState &&
    !Preferences.PreserveLocalDirectory);
  FCacheDirectoryChanges->setEnabled(!FScp || FCacheDirectories->isChecked());
  FPreserveDirectoryChanges->setEnabled(FCacheDirectoryChanges->isEnabled() && FCacheDirectoryChanges->isChecked());
  FResolveSymlinks->setEnabled(FSftp || FScp);
  FFollowDirectorySymlinks->setEnabled(FSftp || FScp || FFtp);

  // Environment.
  FEolType->setEnabled(FSftp || FScp);
  FDstGroup->setEnabled(FSftp || FScp);
  FUtf->setEnabled(FSftp || FFtp || FScp);
  // Should be enabled for SFTP with the SCP fallback too, but it would
  // cause confusion (upstream).
  const bool FtpMlsdOff = FFtp && (AutoSwitch(FFtpUseMlsd) == TAutoSwitch::Off);
  FTimeDifferenceAuto->setEnabled(FtpMlsdOff);
  FTimeDifferenceHours->setEnabled((FtpMlsdOff && !FTimeDifferenceAuto->isChecked()) || FScp);
  FTimeDifferenceMinutes->setEnabled(FTimeDifferenceHours->isEnabled());
  FTrimVMSVersions->setEnabled(!FS3);

  // Environment > Recycle bin.
  FOverwrittenToRecycleBin->setEnabled(FSftp);
  FRecycleBinPath->setEnabled(FDeleteToRecycleBin->isChecked() ||
    (FOverwrittenToRecycleBin->isEnabled() && FOverwrittenToRecycleBin->isChecked()));

  // Environment > SFTP, SCP/Shell, FTP, S3.
  FAllowScpFallback->setEnabled(EditableComboText(FSftpServer).isEmpty());
  // As upstream: hidden also for SFTP with SCP as its fallback; who wants
  // these means SCP, and should choose it.
  FLsGroup->setVisible(FScp);
  FOtherShellGroup->setVisible(FScp);
  FFtpListAll->setEnabled(AutoSwitch(FFtpUseMlsd) == TAutoSwitch::Off);
  FFtpForcePasvIp->setEnabled(FFtpPassive->isChecked() && (FIPAuto->isChecked() || FIPv4->isChecked()));
  FS3SessionToken->setEnabled(FS3 && !FS3Credentials.Automatic);
  FS3RoleArn->setEnabled(FS3SessionToken->isEnabled() && FS3Credentials.Amazon);

  // Connection > Tunnel, TLS/SSL; Environment > Encryption.
  FTunnelSession->setEnabled(FTunnel->isChecked());
  FTunnelOptions->setEnabled(FTunnelSession->isEnabled());
  FTunnelAuthentication->setEnabled(FTunnelSession->isEnabled());
  // TLS session reuse is not configurable for WebDAV/S3 yet (upstream).
  FSslSessionReuse->setEnabled(FFtp);
  FTlsAuthentication->setVisible(FFtp || FWebDav);
  FEncryptGroup->setEnabled(FEncryptFiles->isChecked());
  // Each label as its control (EnableControl on each label upstream).
  for (QLabel * Label : findChildren<QLabel *>())
  {
    if (Label->buddy() != nullptr)
    {
      Label->setEnabled(Label->buddy()->isEnabled());
    }
  }
  FUpdating = false;
}

TProxy SiteAdvanced::ProxyMethod() const
{
  const int Data = FProxy->currentData().toInt();
  return (Data < FtpLogonTypeBase) ? static_cast<TProxy>(Data) : TProxy::None;
}

int SiteAdvanced::FtpProxyLogonType() const
{
  const int Data = FProxy->currentData().toInt();
  return (Data >= FtpLogonTypeBase) ? (Data - FtpLogonTypeBase) : 0;
}

// ProxyAutodetectButtonClick: the system's HTTP proxy (on Windows its
// WinHTTP settings, elsewhere the environment's and the desktop's).
void SiteAdvanced::ProxyAutodetect()
{
  const QList<QNetworkProxy> Proxies =
    QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl(QStringLiteral("http://example.com/"))));
  for (const QNetworkProxy & Proxy : Proxies)
  {
    if ((Proxy.type() == QNetworkProxy::HttpProxy) && !Proxy.hostName().isEmpty())
    {
      FProxyHost->setText(Proxy.hostName());
      FProxyPort->setValue(Proxy.port());
      FProxy->setCurrentIndex(FProxy->findData(static_cast<int>(TProxy::Http)));
      UpdateControls();
      return;
    }
  }
}

void SiteAdvanced::FillColorMenu()
{
  FillSessionColorMenu(FColorButton->menu(), FColor, this, [this](int Color) { SetColor(Color); });
}

void SiteAdvanced::SetColor(int Color)
{
  FColor = Color;
  // As UpdateControls: the color's swatch on the button, once there is one.
  FColorButton->setIcon((FColor != 0) ? SessionColorIcon(FColor) : QIcon());
}

// PrivateKeyViewButtonClick: the key's public line, to copy.
void SiteAdvanced::ShowPublicKey()
{
  std::string Line;
  try
  {
    Line = CoreBridge::PublicKeyLine(FPrivateKeyFile->text().trimmed().toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return;
  }
  QMessageBox Box(QMessageBox::Information, windowTitle(),
    CoreText(FKeyHasCertificate ? CoreBridge::TText::KeyWithCertificate : CoreBridge::TText::AuthorizedKeys), QMessageBox::Ok, this);
  Box.setInformativeText(Q(Line));
  Box.setTextInteractionFlags(Qt::TextSelectableByMouse);
  QPushButton * Copy = Box.addButton(CoreText(CoreBridge::TText::CopyKeyButton), QMessageBox::ActionRole);
  Box.exec();
  if (Box.clickedButton() == Copy)
  {
    QGuiApplication::clipboard()->setText(Q(Line));
  }
}

// GenerateKeyButtonClick: a new key, to back up (with Copy Key).
void SiteAdvanced::GenerateEncryptKey()
{
  const QString Key = Q(CoreBridge::GenerateEncryptKey());
  FEncryptKey->setText(Key);
  QMessageBox Box(QMessageBox::Information, windowTitle(), CoreText(CoreBridge::TText::EncryptKeyGenerated), QMessageBox::Ok, this);
  QPushButton * Copy = Box.addButton(CoreText(CoreBridge::TText::CopyKeyButton), QMessageBox::ActionRole);
  Box.exec();
  if (Box.clickedButton() == Copy)
  {
    QGuiApplication::clipboard()->setText(Key);
  }
}

// EncryptKeyEditExit: a key given must be one.
bool SiteAdvanced::ValidEncryptKey()
{
  if (!FEncryptFiles->isChecked() || FEncryptKey->text().isEmpty())
  {
    return true;
  }
  try
  {
    CoreBridge::ValidateEncryptKey(FEncryptKey->text().toStdString());
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowPage(FPageItems[EncryptionPage]->text(0));
    ShowError(E);
    FEncryptKey->setFocus();
    return false;
  }
}

void SiteAdvanced::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

void SiteAdvanced::accept()
{
  if (ValidEncryptKey())
  {
    QDialog::accept();
  }
}

int SiteAdvanced::PageCount() const
{
  int Result = 0;
  for (int Page = 0; Page < PageTotal; Page++)
  {
    Result += FPageShown[Page] ? 1 : 0;
  }
  return Result;
}

QString SiteAdvanced::PageName(int Index) const
{
  for (int Page = 0; Page < PageTotal; Page++)
  {
    if (FPageShown[Page] && (Index-- == 0))
    {
      return FPageItems[Page]->text(0);
    }
  }
  return QString();
}

void SiteAdvanced::ShowPageAt(int Index)
{
  for (int Page = 0; Page < PageTotal; Page++)
  {
    if (FPageShown[Page] && (Index-- == 0))
    {
      FNavigation->setCurrentItem(FPageItems[Page]);
      return;
    }
  }
}

bool SiteAdvanced::ShowPage(const QString & Name)
{
  for (int Index = 0; Index < PageCount(); Index++)
  {
    if (PageName(Index) == Name)
    {
      ShowPageAt(Index);
      return true;
    }
  }
  return false;
}

QString SiteAdvanced::CurrentPage() const
{
  return (FNavigation->currentItem() != nullptr) ? FNavigation->currentItem()->text(0) : QString();
}

CoreBridge::TSite SiteAdvanced::Site() const
{
  CoreBridge::TSite Site = FSite;
  // Environment > Directories.
  Site.SynchronizeBrowsing = FSynchronizeBrowsing->isChecked();
  Site.RemoteDirectory = FRemoteDirectory->text().trimmed().toStdString();
  Site.LocalDirectory = FLocalDirectory->text().trimmed().toStdString();
  Site.UpdateDirectories = FUpdateDirectories->isChecked();
  Site.CacheDirectories = FCacheDirectories->isChecked();
  Site.CacheDirectoryChanges = FCacheDirectoryChanges->isChecked();
  Site.PreserveDirectoryChanges = FPreserveDirectoryChanges->isChecked();
  Site.ResolveSymlinks = FResolveSymlinks->isChecked();
  Site.FollowDirectorySymlinks = FFollowDirectorySymlinks->isChecked();
  // Environment.
  Site.DstWindows = FDstWindows->isChecked();
  Site.EolCrLf = (FEolType->currentIndex() == 1);
  Site.Utf = AutoSwitch(FUtf);
  Site.TimeDifferenceMinutes = (FTimeDifferenceHours->value() * 60) + FTimeDifferenceMinutes->value();
  Site.TimeDifferenceAuto = FTimeDifferenceAuto->isChecked();
  Site.TrimVMSVersions = FTrimVMSVersions->isChecked();
  Site.VMSAllRevisions = FVMSAllRevisions->isChecked();
  // Environment > Recycle bin: the path kept only when used or changed.
  Site.DeleteToRecycleBin = FDeleteToRecycleBin->isChecked();
  Site.OverwrittenToRecycleBin = FOverwrittenToRecycleBin->isChecked();
  Site.RecycleBinPath = (Site.DeleteToRecycleBin || Site.OverwrittenToRecycleBin ||
    (FRecycleBinPath->text() != DefaultRecycleBinPath)) ? FRecycleBinPath->text().toStdString() : std::string();
  // Environment > Encryption.
  Site.EncryptKey = FEncryptFiles->isChecked() ? FEncryptKey->text().toStdString() : std::string();
  // Environment > SCP/Shell.
  Site.Shell = EditableComboText(FShell).toStdString();
  Site.ReturnVar = EditableComboText(FReturnVar).toStdString();
  Site.ListingCommand = FListingCommand->currentText().toStdString();
  Site.LookupUserGroups = (FLookupUserGroups->checkState() == Qt::Checked) ? TAutoSwitch::On :
    ((FLookupUserGroups->checkState() == Qt::Unchecked) ? TAutoSwitch::Off : TAutoSwitch::Auto);
  Site.ClearAliases = FClearAliases->isChecked();
  Site.IgnoreLsWarnings = FIgnoreLsWarnings->isChecked();
  Site.Scp1Compatibility = FScp1Compatibility->isChecked();
  Site.UnsetNationalVars = FUnsetNationalVars->isChecked();
  Site.ScpLsFullTime = FScpLsFullTime->isChecked();
  // Environment > SFTP: the fallback only for the default server.
  Site.SftpServer = EditableComboText(FSftpServer).toStdString();
  Site.SftpMaxVersion = FSftpMaxVersion->currentIndex() - 1;
  Site.AllowScpFallback = FAllowScpFallback->isChecked();
  Site.UsePosixRename = FUsePosixRename->isChecked();
  Site.SftpRealPath = AutoSwitch(FSftpRealPath);
  Site.SftpBugSymlink = AutoSwitch(FSftpBugSymlink);
  Site.SftpBugSignedTS = AutoSwitch(FSftpBugSignedTS);
  // Environment > FTP.
  Site.FtpAccount = FFtpAccount->text().toStdString();
  Site.PostLoginCommands = FPostLoginCommands->toPlainText().toStdString();
  Site.FtpListAll = AutoSwitch(FFtpListAll);
  Site.FtpUseMlsd = AutoSwitch(FFtpUseMlsd);
  Site.FtpForcePasvIp = AutoSwitch(FFtpForcePasvIp);
  Site.FtpHost = AutoSwitch(FFtpHost);
  // Environment > S3: with automatic credentials the token stays the
  // site's (the environment's is shown only).
  Site.S3DefaultRegion = FS3DefaultRegion->currentText().toStdString();
  Site.S3UrlStylePath = (FS3UrlStyle->currentIndex() == 1);
  Site.S3RequesterPays = FS3RequesterPays->isChecked();
  if (!FS3 || !FS3Credentials.Automatic)
  {
    Site.S3SessionToken = FS3SessionToken->toPlainText().trimmed().toStdString();
    Site.S3RoleArn = FS3RoleArn->text().toStdString();
  }
  // Environment > WebDAV.
  Site.WebDavLiberalEscaping = FWebDavLiberalEscaping->isChecked();
  Site.WebDavCrossDomainRedirects = FWebDavCrossDomainRedirects->isChecked();
  // Connection.
  Site.FtpPassive = FFtpPassive->isChecked();
  Site.OptimizeBuffers = FOptimizeBuffers->isChecked();
  Site.Timeout = FTimeout->value();
  Site.Ping = FPingNullPacket->isChecked() ? TSite::TPing::NullPacket :
    (FPingDummyCommand->isChecked() ? TSite::TPing::DummyCommand : TSite::TPing::Off);
  Site.PingInterval = FPingInterval->value();
  Site.FtpPing = FFtpPingDummyCommand->isChecked() ? TSite::TFtpPing::DummyCommand :
    (FFtpPingDirectoryListing->isChecked() ? TSite::TFtpPing::DirectoryListing : TSite::TFtpPing::Off);
  Site.FtpPingInterval = FFtpPingInterval->value();
  Site.AddressFamily = FIPv4->isChecked() ? TSite::TAddressFamily::IPv4 :
    (FIPv6->isChecked() ? TSite::TAddressFamily::IPv6 : TSite::TAddressFamily::Auto);
  // Connection > Proxy.
  Site.Proxy = ProxyMethod();
  Site.FtpProxyLogonType = FtpProxyLogonType();
  Site.ProxyHost = FProxyHost->text().trimmed().toStdString();
  Site.ProxyPort = FProxyPort->value();
  Site.ProxyUserName = FProxyUserName->text().toStdString();
  Site.ProxyPassword = FProxyPassword->text().toStdString();
  Site.ProxyTelnetCommand = FProxyTelnetCommand->text().toStdString();
  Site.ProxyLocalCommand = FProxyLocalCommand->text().toStdString();
  Site.ProxyLocalhost = FProxyLocalhost->isChecked();
  Site.ProxyDns = AutoSwitch(FProxyDns);
  // Connection > Tunnel.
  Site.Tunnel = FTunnel->isChecked();
  Site.TunnelHostName = FTunnelHostName->text().trimmed().toStdString();
  Site.TunnelPortNumber = FTunnelPortNumber->value();
  Site.TunnelUserName = FTunnelUserName->text().toStdString();
  Site.TunnelPassword = FTunnelPassword->text().toStdString();
  Site.TunnelPrivateKeyFile = FTunnelPrivateKeyFile->text().trimmed().toStdString();
  Site.TunnelLocalPortNumber = (FTunnelLocalPort->currentText() == FTunnelLocalPort->itemText(0)) ? 0 :
    FTunnelLocalPort->currentText().toInt();
  // Connection > TLS/SSL.
  Site.MinTls = static_cast<TSite::TTls>(FMinTls->currentIndex());
  Site.MaxTls = static_cast<TSite::TTls>(FMaxTls->currentIndex());
  Site.SslSessionReuse = FSslSessionReuse->isChecked();
  Site.TlsCertificateFile = FTlsCertificateFile->text().trimmed().toStdString();
  // SSH, Key exchange.
  Site.Compression = FCompression->isChecked();
  Site.Ssh2DES = FSsh2DES->isChecked();
  Site.Ciphers.clear();
  for (int Row = 0; Row < FCiphers->count(); Row++)
  {
    Site.Ciphers.push_back(FCiphers->item(Row)->data(Qt::UserRole).toInt());
  }
  Site.Kex.clear();
  for (int Row = 0; Row < FKex->count(); Row++)
  {
    Site.Kex.push_back(FKex->item(Row)->data(Qt::UserRole).toInt());
  }
  Site.AuthGSSAPIKEX = FAuthGSSAPIKEX->isChecked();
  Site.RekeyTime = FRekeyTime->value();
  Site.RekeyData = FRekeyData->text().toStdString();
  // SSH > Authentication: a key with its certificate keeps no other.
  Site.SshNoUserAuth = FSshNoUserAuth->isChecked();
  Site.TryAgent = FTryAgent->isChecked();
  Site.KeyboardInteractive = FKeyboardInteractive->isChecked();
  Site.KeyboardInteractivePassword = FKeyboardInteractivePassword->isChecked();
  Site.AuthGSSAPI = FAuthGSSAPI->isChecked();
  Site.GSSAPIFwdTGT = FGSSAPIFwdTGT->isChecked();
  Site.AgentForwarding = FAgentForwarding->isChecked();
  Site.PrivateKeyFile = FPrivateKeyFile->text().trimmed().toStdString();
  Site.DetachedCertificate = !FKeyHasCertificate ? FDetachedCertificate->text().trimmed().toStdString() : std::string();
  // SSH > Bugs.
  Site.SshBugs.assign(TSite::SshBugCount, TAutoSwitch::Auto);
  for (int Index = 0; Index < TSite::SshBugCount; Index++)
  {
    Site.SshBugs[Index] = AutoSwitch(FBugs[Index]);
  }
  // Note, color.
  Site.Note = FNote->toPlainText().toStdString();
  Site.Color = FColor;
  return Site;
}
