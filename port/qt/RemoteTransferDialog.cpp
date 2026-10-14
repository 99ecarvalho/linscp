#include "RemoteTransferDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

// UnixExtractFilePath and UnixExtractFileName.
QString PathPart(const QString & Path)
{
  return Path.left(Path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString NamePart(const QString & Path)
{
  return Path.mid(Path.lastIndexOf(QLatin1Char('/')) + 1);
}

}  // namespace

RemoteTransferDialog::RemoteTransferDialog(CoreBridge::TSession * Session, bool Move, bool Multi, const QString & Target,
    const QString & FileMask, QWidget * Parent) :
  QDialog(Parent),
  FSession(Session),
  FMove(Move),
  FMulti(Multi)
{
  setWindowTitle(CoreText(Move ? CoreBridge::TText::RemoteMoveTitle : CoreBridge::TText::RemoteCopyTitle));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QGroupBox * Group = new QGroupBox(this);
  QFormLayout * Fields = new QFormLayout(Group);
  if (!Move)
  {
    // One session: this one (the shell has no other open at once).
    QComboBox * Sessions = new QComboBox(Group);
    Sessions->addItem(QString::fromStdString(Session->Name()));
    Sessions->setEnabled(false);
    Fields->addRow(tr("Target &session:"), Sessions);
  }
  FPath = new QComboBox(Group);
  FPath->setEditable(true);
  FPath->setEditText((Target.endsWith(QLatin1Char('/')) ? Target : (Target + QLatin1Char('/'))) + FileMask);
  FPath->setMinimumWidth(FPath->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 50);
  Fields->addRow(CoreText(CoreBridge::TText::RemoteTransferPrompt), FPath);
  FNotDirect = new QCheckBox(tr("Duplicate via local &temporary copy"), Group);
  // As upstream: through a temporary copy where the server cannot copy.
  const bool Direct = Session->CanRemoteCopy();
  FNotDirect->setChecked(!Direct);
  FNotDirect->setEnabled(Direct);
  FNotDirect->setVisible(!Move);
  Fields->addRow(FNotDirect);
  Layout->addWidget(Group);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &RemoteTransferDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  connect(FPath, &QComboBox::editTextChanged, this, &RemoteTransferDialog::UpdateControls);
  FPath->setFocus();
  FPath->lineEdit()->selectAll();
  UpdateControls();
}

void RemoteTransferDialog::UpdateControls()
{
  FOk->setEnabled(!FPath->currentText().isEmpty());
}

QString RemoteTransferDialog::Target() const
{
  return PathPart(FPath->currentText());
}

QString RemoteTransferDialog::FileMask() const
{
  return NamePart(FPath->currentText());
}

bool RemoteTransferDialog::DirectCopy() const
{
  return !FMove && !FNotDirect->isChecked();
}

bool RemoteTransferDialog::Validate()
{
  QString Path = FPath->currentText();
  if (!CoreBridge::IsFileNameMask(NamePart(Path).toStdString()) && FSession->DirectoryExists(Path.toStdString()))
  {
    // UnixCombinePaths(Path, AnyMask).
    Path = (Path.endsWith(QLatin1Char('/')) ? Path : (Path + QLatin1Char('/'))) + QStringLiteral("*.*");
    FPath->setEditText(Path);
  }
  if (!CoreBridge::IsFileNameMask(NamePart(Path).toStdString()) && FMulti)
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = CoreBridge::MultiFilesToOneConfirmation(Path.toStdString(), true);
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
  return true;
}

void RemoteTransferDialog::accept()
{
  if (Validate())
  {
    QDialog::accept();
  }
}
