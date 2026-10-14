#include "TagDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

TagDialog::TagDialog(bool Add, const QStringList & Keys, const QString & Key, const QString & Value, QWidget * Parent) :
  QDialog(Parent),
  FKeys(Keys)
{
  setWindowTitle(CoreText(Add ? CoreBridge::TText::TagAdd : CoreBridge::TText::TagEdit));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  FKey = new QLineEdit(Key, this);
  // As TTagDialog::Execute: longer than S3 allows only if it already is.
  FKey->setMaxLength(qMax(128, Key.length()));
  FValue = new QLineEdit(Value, this);
  FValue->setMaxLength(qMax(256, Value.length()));
  QLabel * KeyLabel = new QLabel(CoreText(CoreBridge::TText::TagKey), this);
  KeyLabel->setBuddy(FKey);
  QLabel * ValueLabel = new QLabel(CoreText(CoreBridge::TText::TagValue), this);
  ValueLabel->setBuddy(FValue);
  Form->addRow(KeyLabel, FKey);
  Form->addRow(ValueLabel, FValue);
  Layout->addLayout(Form);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &TagDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  // As DoChange: a key is needed.
  const auto Update = [this]() { FOk->setEnabled(!FKey->text().isEmpty()); };
  connect(FKey, &QLineEdit::textChanged, this, Update);
  Update();
  resize(qMax(sizeHint().width(), 380), sizeHint().height());
}

QString TagDialog::Key() const
{
  return FKey->text();
}

QString TagDialog::Value() const
{
  return FValue->text();
}

bool TagDialog::ValidateTag(QLineEdit * Edit)
{
  // WinSCP's list; "there are lot more in various scripts".
  static const QString InvalidChars =
    QStringLiteral("!\"#$%&'()*,;<>?[\\]^`{|}~¡¢£¤¥¦§¨©«¬­®¯"
      "°±´¶·¸»¿×÷");
  for (const QChar Ch : Edit->text())
  {
    if ((Ch.category() == QChar::Other_Control) || InvalidChars.contains(Ch))
    {
      CoreBridge::TQuery Query;
      Query.Type = CoreBridge::TQueryType::Warning;
      Query.Message = CoreText(CoreBridge::TText::TagInvalidChar).replace(QStringLiteral("%s"), QString(Ch)).toStdString();
      CoreBridge::TQueryButton Ignore;
      Ignore.Answer = CoreBridge::AnswerIgnore;
      Ignore.Caption = "&Ignore";
      CoreBridge::TQueryButton Abort;
      Abort.Answer = CoreBridge::AnswerAbort;
      Abort.Caption = "&Abort";
      Query.Buttons = { Ignore, Abort };
      Query.CancelAnswer = CoreBridge::AnswerAbort;
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      if (Frontend.Query(Query).Answer != CoreBridge::AnswerIgnore)
      {
        Edit->setFocus();
        return false;
      }
      break;
    }
  }
  return true;
}

// As TTagDialog::DoValidate.
void TagDialog::accept()
{
  if (FKeys.contains(FKey->text()))
  {
    CoreBridge::TError Error;
    Error.Message = CoreBridge::Text(CoreBridge::TText::TagNotUnique);
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(Error);
    FKey->setFocus();
    return;
  }
  if (ValidateTag(FKey) && ValidateTag(FValue))
  {
    QDialog::accept();
  }
}
