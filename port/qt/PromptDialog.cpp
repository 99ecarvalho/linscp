#include "PromptDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

PromptDialog::PromptDialog(const CoreBridge::TPrompt & Prompt, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(Prompt.Name.empty() ? tr("Authentication") : QString::fromStdString(Prompt.Name));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  if (!Prompt.Instructions.empty())
  {
    QLabel * Instructions = new QLabel(QString::fromStdString(Prompt.Instructions), this);
    Instructions->setWordWrap(true);
    Instructions->setTextFormat(Qt::PlainText);
    Layout->addWidget(Instructions);
  }
  QFormLayout * Form = new QFormLayout();
  for (const CoreBridge::TPromptField & Field : Prompt.Fields)
  {
    QLineEdit * Edit = new QLineEdit(QString::fromStdString(Field.Value), this);
    Edit->setEchoMode(Field.Echo ? QLineEdit::Normal : QLineEdit::Password);
    // Core's prompts end in ": " already.
    QString Label = QString::fromStdString(Field.Label).trimmed();
    QLabel * Caption = new QLabel(Label, this);
    Caption->setBuddy(Edit);
    Form->addRow(Caption, Edit);
    FFields.append(Edit);
  }
  Layout->addLayout(Form);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
  if (!FFields.isEmpty())
  {
    FFields.first()->setFocus();
  }
  resize(qMax(sizeHint().width(), 400), sizeHint().height());
}

void PromptDialog::Fill(CoreBridge::TPrompt & Prompt) const
{
  for (int Index = 0; (Index < FFields.size()) && (Index < static_cast<int>(Prompt.Fields.size())); Index++)
  {
    Prompt.Fields[Index].Value = FFields[Index]->text().toStdString();
  }
}
