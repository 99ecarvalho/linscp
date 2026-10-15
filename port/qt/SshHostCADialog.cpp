#include "SshHostCADialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

namespace
{

QString TextOf(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

SshHostCADialog::SshHostCADialog(bool Add, const CoreBridge::TSshHostCA & CA, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(TextOf(Add ? CoreBridge::TText::SshHostCAAdd : CoreBridge::TText::SshHostCAEdit));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  Layout->addLayout(Form);
  FName = new QLineEdit(QString::fromStdString(CA.Name), this);
  QLabel * NameLabel = new QLabel(TextOf(CoreBridge::TText::SshHostCAName), this);
  NameLabel->setBuddy(FName);
  Form->addRow(NameLabel, FName);
  FPublicKey = new QLineEdit(QString::fromStdString(CA.PublicKey), this);
  QPushButton * Browse = new QPushButton(TextOf(CoreBridge::TText::SshHostCABrowse), this);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    // SSH_HOST_CA_BROWSE_FILTER's, in Qt's form.
    const QString FileName = QFileDialog::getOpenFileName(this, TextOf(CoreBridge::TText::SshHostCABrowseTitle), QString(),
      tr("Public key files (*.pub);;All Files (*)"));
    if (!FileName.isEmpty())
    {
      LoadKeyFile(FileName);
    }
  });
  QHBoxLayout * KeyRow = new QHBoxLayout();
  KeyRow->addWidget(FPublicKey, 1);
  KeyRow->addWidget(Browse);
  QLabel * KeyLabel = new QLabel(TextOf(CoreBridge::TText::SshHostCAPublicKey), this);
  KeyLabel->setBuddy(FPublicKey);
  Form->addRow(KeyLabel, KeyRow);
  // As PublicKeyLabel: the key's fingerprint, or what is wrong with it.
  FStatus = new QLabel(this);
  FStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  FStatus->setWordWrap(true);
  Form->addRow(QString(), FStatus);
  FHosts = new QLineEdit(QString::fromStdString(CA.ValidityExpression), this);
  QLabel * HostsLabel = new QLabel(TextOf(CoreBridge::TText::SshHostCAPublicHosts), this);
  HostsLabel->setBuddy(FHosts);
  Form->addRow(HostsLabel, FHosts);
  const QStringList Signatures = TextOf(CoreBridge::TText::SshHostCASignatures).split(QLatin1Char('|'));
  QHBoxLayout * SignatureRow = new QHBoxLayout();
  FSha1 = new QCheckBox(Signatures.value(0), this);
  FSha256 = new QCheckBox(Signatures.value(1), this);
  FSha512 = new QCheckBox(Signatures.value(2), this);
  FSha1->setChecked(CA.PermitRsaSha1);
  FSha256->setChecked(CA.PermitRsaSha256);
  FSha512->setChecked(CA.PermitRsaSha512);
  for (QCheckBox * Check : { FSha1, FSha256, FSha512 })
  {
    SignatureRow->addWidget(Check);
  }
  SignatureRow->addStretch(1);
  Form->addRow(TextOf(CoreBridge::TText::SshHostCASignatureTypes), SignatureRow);
  FButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(FButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(FButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(FButtons);
  for (QLineEdit * Edit : { FName, FPublicKey, FHosts })
  {
    connect(Edit, &QLineEdit::textChanged, this, &SshHostCADialog::UpdateControls);
  }
  resize(577, sizeHint().height());
  UpdateControls();
}

CoreBridge::TSshHostCA SshHostCADialog::CA() const
{
  CoreBridge::TSshHostCA Result;
  Result.Name = FName->text().toStdString();
  Result.PublicKey = FPublicKey->text().trimmed().toStdString();
  Result.ValidityExpression = FHosts->text().toStdString();
  Result.PermitRsaSha1 = FSha1->isChecked();
  Result.PermitRsaSha256 = FSha256->isChecked();
  Result.PermitRsaSha512 = FSha512->isChecked();
  return Result;
}

void SshHostCADialog::LoadKeyFile(const QString & FileName)
{
  try
  {
    std::string Key;
    std::string Comment;
    CoreBridge::LoadSshHostCAKey(FileName.toStdString(), Key, Comment);
    if (FName->text().isEmpty())
    {
      FName->setText(QString::fromStdString(Comment));
    }
    FPublicKey->setText(QString::fromStdString(Key));
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

// As DoChange.
void SshHostCADialog::UpdateControls()
{
  std::string Status;
  CoreBridge::SshHostCAKeyStatus(FPublicKey->text().toStdString(), Status);
  FStatus->setText(QString::fromStdString(Status));
  FButtons->button(QDialogButtonBox::Ok)->setEnabled(!FName->text().trimmed().isEmpty() && !FPublicKey->text().trimmed().isEmpty());
}

// As DoValidate.
void SshHostCADialog::accept()
{
  std::string Status;
  if (!CoreBridge::SshHostCAKeyStatus(FPublicKey->text().toStdString(), Status))
  {
    QMessageBox::warning(this, windowTitle(), QString::fromStdString(Status));
    FPublicKey->setFocus();
    return;
  }
  if (FHosts->text().trimmed().isEmpty())
  {
    QMessageBox::warning(this, windowTitle(), TextOf(CoreBridge::TText::SshHostCANoHosts));
    FHosts->setFocus();
    return;
  }
  std::string Error;
  int Start = 0;
  int Length = 0;
  if (!CoreBridge::SshHostCAValidityValid(FHosts->text().toStdString(), Error, Start, Length))
  {
    CoreBridge::TError Message;
    Message.Message = CoreBridge::Text(CoreBridge::TText::SshHostCAHostsInvalid);
    Message.MoreMessages = Error;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(Message);
    FHosts->setFocus();
    FHosts->setSelection(Start, Length);
    return;
  }
  QDialog::accept();
}
