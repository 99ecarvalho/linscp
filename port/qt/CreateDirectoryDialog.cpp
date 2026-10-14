#include "CreateDirectoryDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "RightsFrame.h"

CreateDirectoryDialog::CreateDirectoryDialog(const QString & Name, bool Remote, bool CanChangeRights,
    const CoreBridge::TNewDirectory & Settings, QWidget * Parent) :
  QDialog(Parent),
  FCanChangeRights(Remote && CanChangeRights)
{
  setWindowTitle(tr("Create folder"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QLabel * Label = new QLabel(tr("New &folder name:"), this);
  FName = new QLineEdit(Name, this);
  Label->setBuddy(FName);
  Layout->addWidget(Label);
  Layout->addWidget(FName);
  FAttributes = new QGroupBox(tr("Attributes"), this);
  QVBoxLayout * Attributes = new QVBoxLayout(FAttributes);
  FSetRights = new QCheckBox(tr("Set pe&rmissions"), FAttributes);
  FSetRights->setChecked(Settings.SetRights);
  FRights = new RightsFrame(FAttributes);
  FRights->SetAllowAddXToDirectories(false);
  FRights->SetRights(Settings.Rights);
  Attributes->addWidget(FSetRights);
  Attributes->addWidget(FRights);
  Layout->addWidget(FAttributes);
  FAttributes->setVisible(Remote);
  FSaveSettings = new QCheckBox(tr("Use &same settings next time"), this);
  FSaveSettings->setVisible(Remote);
  Layout->addWidget(FSaveSettings);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  connect(FName, &QLineEdit::textChanged, this, &CreateDirectoryDialog::UpdateControls);
  connect(FSetRights, &QCheckBox::toggled, this, &CreateDirectoryDialog::UpdateControls);
  FName->selectAll();
  UpdateControls();
}

// As TCreateDirectoryDialog::UpdateControls.
void CreateDirectoryDialog::UpdateControls()
{
  FOk->setEnabled(!FName->text().trimmed().isEmpty());
  FSetRights->setEnabled(FCanChangeRights);
  FRights->setEnabled(FCanChangeRights && FSetRights->isChecked());
  FSaveSettings->setEnabled(FCanChangeRights);
}

QString CreateDirectoryDialog::Name() const
{
  return FName->text();
}

CoreBridge::TNewDirectory CreateDirectoryDialog::Settings() const
{
  CoreBridge::TNewDirectory Result;
  Result.SetRights = FSetRights->isEnabled() && FSetRights->isChecked();
  Result.Rights = FRights->Rights();
  return Result;
}

bool CreateDirectoryDialog::SaveSettings() const
{
  return FSaveSettings->isEnabled() && FSaveSettings->isChecked();
}
