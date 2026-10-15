#include "MasterPasswordDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"
#include "QtFrontend.h"

namespace
{

QString TextOf(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

MasterPasswordDialog::MasterPasswordDialog(bool Change, QWidget * Parent) :
  QDialog(Parent),
  FChange(Change)
{
  setWindowTitle(TextOf(CoreBridge::TText::MasterPasswordCaption));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  Layout->addLayout(Form);
  FCurrent = MakeEdit(TextOf(CoreBridge::TText::MasterPasswordCurrent), Form);
  FCurrent->setEnabled(!Change || CoreBridge::UseMasterPassword());
  if (Change)
  {
    FNew = MakeEdit(TextOf(CoreBridge::TText::MasterPasswordNew), Form);
    FConfirm = MakeEdit(TextOf(CoreBridge::TText::MasterPasswordConfirm), Form);
    if (!CoreBridge::UseMasterPassword())
    {
      FNew->setFocus();
    }
  }
  FButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(FButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(FButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(FButtons);
  UpdateControls();
}

QLineEdit * MasterPasswordDialog::MakeEdit(const QString & Label, QFormLayout * Form)
{
  QLineEdit * Result = new QLineEdit(this);
  Result->setEchoMode(QLineEdit::Password);
  QLabel * Caption = new QLabel(Label, this);
  Caption->setBuddy(Result);
  Form->addRow(Caption, Result);
  connect(Result, &QLineEdit::textChanged, this, &MasterPasswordDialog::UpdateControls);
  return Result;
}

// As DoChange: each password one at all (IsValidPassword).
void MasterPasswordDialog::UpdateControls()
{
  const auto Valid = [](const QLineEdit * Edit)
  {
    return (Edit == nullptr) || (CoreBridge::MasterPasswordQuality(Edit->text().toStdString()) >= 0);
  };
  const bool CurrentAsked = !FChange || CoreBridge::UseMasterPassword();
  FButtons->button(QDialogButtonBox::Ok)->setEnabled((!CurrentAsked || Valid(FCurrent)) && Valid(FNew) && Valid(FConfirm));
}

// As DoValidate.
void MasterPasswordDialog::accept()
{
  if (CoreBridge::UseMasterPassword() && !CoreBridge::ValidateMasterPassword(FCurrent->text().toStdString()))
  {
    QMessageBox::warning(this, windowTitle(), TextOf(CoreBridge::TText::MasterPasswordIncorrect));
    FCurrent->setFocus();
    FCurrent->selectAll();
    return;
  }
  if (FNew != nullptr)
  {
    if (FNew->text() != FConfirm->text())
    {
      QMessageBox::warning(this, windowTitle(), TextOf(CoreBridge::TText::MasterPasswordDifferent));
      FConfirm->setFocus();
      FConfirm->selectAll();
      return;
    }
    if (CoreBridge::MasterPasswordQuality(FNew->text().toStdString()) <= 0)
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Warning;
      Query.Message = CoreBridge::Text(CoreBridge::TText::MasterPasswordSimple);
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
        FNew->setFocus();
        FNew->selectAll();
        return;
      }
    }
  }
  Commit();
  QDialog::accept();
}

void MasterPasswordDialog::Commit()
{
  if ((!FChange || CoreBridge::UseMasterPassword()) && !FCurrent->text().isEmpty())
  {
    CoreBridge::SetMasterPassword(FCurrent->text().toStdString());
  }
}

bool MasterPasswordDialog::AskCurrent(QWidget * Parent)
{
  MasterPasswordDialog Dialog(false, Parent);
  return (Dialog.exec() == QDialog::Accepted);
}

bool MasterPasswordDialog::AskChange(QWidget * Parent, std::string & New)
{
  MasterPasswordDialog Dialog(true, Parent);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  New = Dialog.NewEdit()->text().toStdString();
  return true;
}
