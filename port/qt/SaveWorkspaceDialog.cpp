#include "SaveWorkspaceDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
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

SaveWorkspaceDialog::SaveWorkspaceDialog(const QString & Name, bool CanSavePasswords, bool SavePasswords, bool AutoSave,
  QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(CoreText(CoreBridge::TText::SaveWorkspaceCaption));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FName = new QComboBox(this);
  FName->setEditable(true);
  for (const std::string & Workspace : CoreBridge::WorkspaceNames())
  {
    FName->addItem(QString::fromStdString(Workspace));
  }
  FName->setEditText(Name);
  FName->setMinimumWidth(320);
  QLabel * Label = new QLabel(CoreText(CoreBridge::TText::SaveWorkspacePrompt), this);
  Label->setBuddy(FName);
  Layout->addWidget(Label);
  Layout->addWidget(FName);
  // As SaveWorkspace: saving passwords is recommended only with a master
  // password, which the shell does not have yet.
  FSavePasswords = new QCheckBox(CoreText(CoreBridge::TText::SaveWorkspacePasswords), this);
  FSavePasswords->setEnabled(CanSavePasswords);
  FSavePasswords->setChecked(CanSavePasswords && SavePasswords);
  Layout->addWidget(FSavePasswords);
  FAutoSave = new QCheckBox(CoreText(CoreBridge::TText::SaveWorkspaceAuto), this);
  FAutoSave->setChecked(AutoSave);
  Layout->addWidget(FAutoSave);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &SaveWorkspaceDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  // As DoChange: a name is needed.
  const auto Update = [this]() { FOk->setEnabled(!FName->currentText().isEmpty()); };
  connect(FName, &QComboBox::editTextChanged, this, Update);
  Update();
}

QString SaveWorkspaceDialog::Name() const
{
  return FName->currentText();
}

bool SaveWorkspaceDialog::SavePasswords() const
{
  return FSavePasswords->isEnabled() && FSavePasswords->isChecked();
}

bool SaveWorkspaceDialog::AutoSave() const
{
  return FAutoSave->isChecked();
}

void SaveWorkspaceDialog::accept()
{
  try
  {
    CoreBridge::ValidateWorkspaceName(Name().toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
    return;
  }
  QDialog::accept();
}
