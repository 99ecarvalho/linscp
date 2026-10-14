#include "ServerInfoDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "FileList.h"
#include "QtFrontend.h"

namespace
{

QLineEdit * View(const QString & Text, QWidget * Parent)
{
  QLineEdit * Edit = new QLineEdit(Text, Parent);
  Edit->setReadOnly(true);
  return Edit;
}

QTreeWidget * ItemView(QWidget * Parent)
{
  QTreeWidget * Tree = new QTreeWidget(Parent);
  Tree->setHeaderLabels({ ServerInfoDialog::tr("Item"), ServerInfoDialog::tr("Value") });
  Tree->setRootIsDecorated(false);
  Tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  return Tree;
}

void Fill(QTreeWidget * Tree, const std::vector<std::pair<std::string, std::string>> & Rows)
{
  Tree->clear();
  for (const auto & Row : Rows)
  {
    if (!Row.second.empty())
    {
      Tree->addTopLevelItem(new QTreeWidgetItem({ QString::fromStdString(Row.first), QString::fromStdString(Row.second) }));
    }
  }
  Tree->resizeColumnToContents(0);
}

// TFileSystemInfoDialog::SpaceStr.
QString SpaceText(std::int64_t Bytes)
{
  if (Bytes == 0)
  {
    return ServerInfoDialog::tr("Unknown");
  }
  QString Result = FormatBytes(Bytes);
  const QString Unordered = QStringLiteral("%1 B").arg(QLocale().toString(static_cast<qlonglong>(Bytes)));
  return (Result != Unordered) ? QStringLiteral("%1 (%2)").arg(Result, Unordered) : Result;
}

}  // namespace

ServerInfoDialog::ServerInfoDialog(CoreBridge::TSession * Session, const CoreBridge::TServerInfo & Info, const QString & Path,
    QWidget * Parent) :
  QDialog(Parent),
  FSession(Session),
  FInfo(Info)
{
  setWindowTitle(tr("Server and protocol information"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FPages = new QTabWidget(this);
  Layout->addWidget(FPages, 1);

  // Protocol.
  QWidget * Protocol = new QWidget(FPages);
  QVBoxLayout * ProtocolLayout = new QVBoxLayout(Protocol);
  FServer = ItemView(Protocol);
  FServer->setAccessibleName(tr("Protocol"));
  Fill(FServer, Info.Server);
  ProtocolLayout->addWidget(FServer, 1);
  FHostKeyGroup = new QGroupBox(tr("Server host key fingerprints"), Protocol);
  QFormLayout * HostKey = new QFormLayout(FHostKeyGroup);
  HostKey->addRow(tr("Algorithm:"), View(QString::fromStdString(Info.HostKeyAlgorithm), FHostKeyGroup));
  FHostKeySha256 = View(QString::fromStdString(Info.HostKeySha256), FHostKeyGroup);
  HostKey->addRow(tr("SHA-256:"), FHostKeySha256);
  HostKey->addRow(tr("MD5:"), View(QString::fromStdString(Info.HostKeyMd5), FHostKeyGroup));
  FHostKeyGroup->setVisible(!Info.HostKeySha256.empty());
  ProtocolLayout->addWidget(FHostKeyGroup);
  FCertificateGroup = new QGroupBox(tr("Certificate fingerprint"), Protocol);
  QFormLayout * Certificate = new QFormLayout(FCertificateGroup);
  Certificate->addRow(tr("SHA-256:"), View(QString::fromStdString(Info.CertificateSha256), FCertificateGroup));
  QHBoxLayout * Sha1Row = new QHBoxLayout();
  Sha1Row->addWidget(View(QString::fromStdString(Info.CertificateSha1), FCertificateGroup), 1);
  QPushButton * Full = new QPushButton(tr("&Full certificate"), FCertificateGroup);
  connect(Full, &QPushButton::clicked, this, [this]()
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Information;
    Query.Message = FInfo.Certificate;
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    Query.Buttons = { OK };
    Query.CancelAnswer = CoreBridge::AnswerOK;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.Query(Query);
  });
  Full->setEnabled(!Info.Certificate.empty());
  Sha1Row->addWidget(Full);
  Certificate->addRow(tr("SHA-1:"), Sha1Row);
  FCertificateGroup->setVisible(!Info.CertificateSha256.empty());
  ProtocolLayout->addWidget(FCertificateGroup);
  FPages->addTab(Protocol, tr("Protocol"));

  // Capabilities.
  QWidget * Capabilities = new QWidget(FPages);
  QVBoxLayout * CapabilitiesLayout = new QVBoxLayout(Capabilities);
  FCapabilities = ItemView(Capabilities);
  FCapabilities->setAccessibleName(tr("Capabilities"));
  Fill(FCapabilities, Info.Capabilities);
  CapabilitiesLayout->addWidget(FCapabilities, 1);
  FInfoGroup = new QGroupBox(tr("Additional information"), Capabilities);
  QVBoxLayout * InfoLayout = new QVBoxLayout(FInfoGroup);
  QPlainTextEdit * InfoText = new QPlainTextEdit(QString::fromStdString(Info.AdditionalInfo), FInfoGroup);
  InfoText->setReadOnly(true);
  InfoLayout->addWidget(InfoText);
  FInfoGroup->setVisible(!Info.AdditionalInfo.empty());
  CapabilitiesLayout->addWidget(FInfoGroup);
  FPages->addTab(Capabilities, tr("Capabilities"));

  // Space available.
  QWidget * Space = new QWidget(FPages);
  QVBoxLayout * SpaceLayout = new QVBoxLayout(Space);
  QHBoxLayout * PathRow = new QHBoxLayout();
  QLabel * PathLabel = new QLabel(tr("&Path:"), Space);
  FPath = new QLineEdit(Path, Space);
  PathLabel->setBuddy(FPath);
  FCheckSpace = new QPushButton(tr("Check &space"), Space);
  connect(FCheckSpace, &QPushButton::clicked, this, [this]() { CheckSpace(); });
  connect(FPath, &QLineEdit::textChanged, this, [this]() { FCheckSpace->setEnabled(!FPath->text().isEmpty()); });
  connect(FPath, &QLineEdit::returnPressed, this, [this]() { CheckSpace(); });
  PathRow->addWidget(PathLabel);
  PathRow->addWidget(FPath, 1);
  PathRow->addWidget(FCheckSpace);
  SpaceLayout->addLayout(PathRow);
  FSpace = ItemView(Space);
  FSpace->setAccessibleName(tr("Space available"));
  SpaceLayout->addWidget(FSpace, 1);
  FSpacePage = FPages->addTab(Space, tr("Space available"));
  FPages->setTabEnabled(FSpacePage, Info.CanCheckSpace);
  // As PageControlChange: checked when the page is first shown.
  connect(FPages, &QTabWidget::currentChanged, this, [this](int Page)
  {
    if ((Page == FSpacePage) && !FSpaceChecked)
    {
      CheckSpace();
    }
  });

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(Buttons->addButton(tr("&Copy to Clipboard"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this,
    &ServerInfoDialog::CopyToClipboard);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  resize(560, 520);
}

bool ServerInfoDialog::CheckSpace()
{
  if (!FInfo.CanCheckSpace || FPath->text().isEmpty())
  {
    return false;
  }
  FSpaceChecked = true;
  try
  {
    const CoreBridge::TSpaceInfo Space = FSession->SpaceAvailable(FPath->text().toStdString());
    FSpace->clear();
    for (const auto & Item : Space.Items)
    {
      FSpace->addTopLevelItem(new QTreeWidgetItem({ QString::fromStdString(Item.first), SpaceText(Item.second) }));
    }
    FSpace->resizeColumnToContents(0);
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    if (!E.Aborted)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
    return false;
  }
}

// As ClipboardAddItem: the groups apart, "label = value" each.
QString ServerInfoDialog::ClipboardText()
{
  if (!FSpaceChecked)
  {
    CheckSpace();
  }
  QStringList Sections;
  const auto Rows = [](QTreeWidget * Tree)
  {
    QString Text;
    for (int Row = 0; Row < Tree->topLevelItemCount(); Row++)
    {
      Text += QStringLiteral("%1 = %2\r\n").arg(Tree->topLevelItem(Row)->text(0), Tree->topLevelItem(Row)->text(1));
    }
    return Text;
  };
  Sections.append(Rows(FServer));
  if (!FInfo.HostKeySha256.empty())
  {
    Sections.append(FHostKeyGroup->title() + QStringLiteral("\r\n") +
      QStringLiteral("SHA-256 = %1\r\nMD5 = %2\r\n").arg(QString::fromStdString(FInfo.HostKeySha256), QString::fromStdString(FInfo.HostKeyMd5)));
  }
  if (!FInfo.CertificateSha256.empty())
  {
    Sections.append(FCertificateGroup->title() + QStringLiteral("\r\n") +
      QStringLiteral("SHA-256 = %1\r\nSHA-1 = %2\r\n").arg(QString::fromStdString(FInfo.CertificateSha256), QString::fromStdString(FInfo.CertificateSha1)));
  }
  Sections.append(Rows(FCapabilities));
  if (!FInfo.AdditionalInfo.empty())
  {
    Sections.append(FInfoGroup->title() + QStringLiteral("\r\n") + QString::fromStdString(FInfo.AdditionalInfo).trimmed() + QStringLiteral("\r\n"));
  }
  if (FSpace->topLevelItemCount() > 0)
  {
    Sections.append(Rows(FSpace));
  }
  Sections.removeAll(QString());
  return Sections.join(QString(60, QLatin1Char('-')) + QStringLiteral("\r\n"));
}

void ServerInfoDialog::CopyToClipboard()
{
  QApplication::clipboard()->setText(ClipboardText());
}
