#include "CustomCommandDialog.h"
#include "ShortCutDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "OpenDirectoryDialog.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

void Error(QWidget * Parent, const QString & Message)
{
  CoreBridge::TError Error;
  Error.Message = Message.toStdString();
  QtFrontend Frontend;
  Frontend.SetWindow(Parent);
  Frontend.ShowError(Error);
}

// The patterns' hint, as the dialog's constructor assembles it.
QString PatternsHint()
{
  QStringList Lines = CoreText(CoreBridge::TText::CustomCommandPatternsHint).split(QLatin1Char('\n'));
  Lines.insert(qMin(6, int(Lines.size())), CoreText(CoreBridge::TText::CustomCommandPatternsHint6));
  Lines.insert(qMin(11, int(Lines.size())), CoreText(CoreBridge::TText::PatternsHintK));
  Lines.insert(qMin(18, int(Lines.size())), CoreText(CoreBridge::TText::PatternsHintBackslash));
  return Lines.join(QLatin1Char('\n')).trimmed();
}

}  // namespace

CustomCommandDialog::CustomCommandDialog(TMode Mode, const CoreBridge::TCustomCommandDefinition & Command,
    const std::vector<CoreBridge::TCustomCommandDefinition> & Commands, QWidget * Parent) :
  QDialog(Parent),
  FMode(Mode),
  FOriginal(Command),
  FCommands(Commands)
{
  setWindowTitle(CoreText((Mode == TMode::Add) ? CoreBridge::TText::CustomCommandAdd :
    ((Mode == TMode::Edit) ? CoreBridge::TText::CustomCommandEdit : CoreBridge::TText::CustomCommandAdHoc)));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QGroupBox * Group = new QGroupBox(this);
  QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
  QFormLayout * Fields = new QFormLayout();
  FDescription = new QLineEdit(QString::fromStdString(Command.Name), Group);
  if (Mode != TMode::AdHoc)
  {
    Fields->addRow(tr("&Description:"), FDescription);
  }
  else
  {
    FDescription->hide();
  }
  FCommand = new QComboBox(Group);
  FCommand->setEditable(true);
  for (const CoreBridge::TCustomCommandDefinition & Other : Commands)
  {
    FCommand->addItem(QString::fromStdString(Other.Command));
  }
  FCommand->setEditText(QString::fromStdString(Command.Command));
  FCommand->setMinimumWidth(FCommand->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 50);
  FCommand->setToolTip(PatternsHint());
  Fields->addRow(tr("&Custom command:"), FCommand);
  GroupLayout->addLayout(Fields);
  QHBoxLayout * Sides = new QHBoxLayout();
  FRemote = new QRadioButton(tr("&Remote command"), Group);
  FLocal = new QRadioButton(tr("&Local command"), Group);
  (Command.Local ? FLocal : FRemote)->setChecked(true);
  Sides->addWidget(FRemote);
  Sides->addWidget(FLocal);
  Sides->addStretch(1);
  GroupLayout->addLayout(Sides);
  FApplyToDirectories = new QCheckBox(tr("&Apply to directories"), Group);
  FApplyToDirectories->setChecked(Command.ApplyToDirectories);
  FRecursive = new QCheckBox(tr("&Execute recursively"), Group);
  FRecursive->setChecked(Command.Recursive);
  FShowResults = new QCheckBox(tr("&Show results in terminal"), Group);
  FShowResults->setChecked(Command.ShowResults);
  FCopyResults = new QCheckBox(tr("Copy results to clip&board"), Group);
  FCopyResults->setChecked(Command.CopyResults);
  FRemoteFiles = new QCheckBox(tr("&Use remote files"), Group);
  FRemoteFiles->setChecked(Command.RemoteFiles);
  for (QCheckBox * Check : { FApplyToDirectories, FRecursive, FShowResults, FCopyResults, FRemoteFiles })
  {
    GroupLayout->addWidget(Check);
    connect(Check, &QCheckBox::toggled, this, &CustomCommandDialog::UpdateControls);
  }
  QLabel * Hint = new QLabel(tr("&patterns"), Group);
  Hint->setToolTip(PatternsHint());
  GroupLayout->addWidget(Hint);
  FShortCut = new QComboBox(Group);
  if (Mode != TMode::AdHoc)
  {
    // As InitializeShortCutCombo: those the other commands use left out.
    std::vector<unsigned> Used;
    for (const CoreBridge::TCustomCommandDefinition & Other : Commands)
    {
      if ((Other.ShortCut != 0) && (Other.Name != Command.Name))
      {
        Used.push_back(Other.ShortCut);
      }
    }
    FillShortCutCombo(FShortCut, Used, Command.ShortCut);
    QFormLayout * ShortCutRow = new QFormLayout();
    ShortCutRow->addRow(tr("&Keyboard shortcut:"), FShortCut);
    GroupLayout->addLayout(ShortCutRow);
  }
  else
  {
    FShortCut->hide();
  }
  Layout->addWidget(Group);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &CustomCommandDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  connect(FDescription, &QLineEdit::textChanged, this, &CustomCommandDialog::UpdateControls);
  connect(FCommand, &QComboBox::editTextChanged, this, &CustomCommandDialog::UpdateControls);
  connect(FRemote, &QRadioButton::toggled, this, &CustomCommandDialog::UpdateControls);
  if (Mode == TMode::AdHoc)
  {
    // Ad hoc: its own name, so the description check passes.
    FDescription->setText(CoreText(CoreBridge::TText::CustomCommandAdHocName));
  }
  UpdateControls();
}

// As TCustomCommandDialog::UpdateControls.
void CustomCommandDialog::UpdateControls()
{
  const QString Text = FCommand->currentText();
  FOk->setEnabled(!Text.isEmpty() && !FDescription->text().isEmpty());
  const bool Remote = FRemote->isChecked();
  const CoreBridge::TCustomCommandCheck Check = CoreBridge::CheckCustomCommand(Text.toStdString(), Remote);
  FRecursive->setEnabled(Check.AllowRecursive && (!FRemoteFiles->isEnabled() || !FRemoteFiles->isChecked()));
  FApplyToDirectories->setEnabled(Check.AllowApplyToDirectories);
  FShowResults->setEnabled(Remote);
  FRemoteFiles->setEnabled(Check.AllowRemoteFiles && (!FRecursive->isEnabled() || !FRecursive->isChecked()));
}

CoreBridge::TCustomCommandDefinition CustomCommandDialog::Command() const
{
  CoreBridge::TCustomCommandDefinition Result = FOriginal;
  Result.Name = FDescription->text().toStdString();
  Result.Command = FCommand->currentText().toStdString();
  Result.Local = !FRemote->isChecked();
  Result.ApplyToDirectories = FApplyToDirectories->isChecked();
  Result.Recursive = FRecursive->isChecked() && FRecursive->isEnabled();
  Result.ShowResults = FShowResults->isChecked() && FShowResults->isEnabled();
  Result.CopyResults = FCopyResults->isChecked() && FCopyResults->isEnabled();
  Result.RemoteFiles = FRemoteFiles->isChecked() && FRemoteFiles->isEnabled();
  if (FMode != TMode::AdHoc)
  {
    Result.ShortCut = FShortCut->currentData().toUInt();
  }
  return Result;
}

// As TCustomCommandDialog::FormCloseQuery.
bool CustomCommandDialog::Validate()
{
  if (FMode != TMode::AdHoc)
  {
    const QString Description = FDescription->text();
    if (Description.contains(QLatin1Char('=')))
    {
      FDescription->setFocus();
      Error(this, CoreText(CoreBridge::TText::CustomCommandInvalid).replace(QStringLiteral("%s"), QStringLiteral("=")));
      return false;
    }
    if ((FMode == TMode::Add) || (Description.toStdString() != FOriginal.Name))
    {
      for (const CoreBridge::TCustomCommandDefinition & Other : FCommands)
      {
        if (Other.Name == Description.toStdString())
        {
          FDescription->setFocus();
          Error(this, CoreText(CoreBridge::TText::CustomCommandDuplicate).replace(QStringLiteral("%s"), Description));
          return false;
        }
      }
    }
  }
  const CoreBridge::TCustomCommandCheck Check =
    CoreBridge::CheckCustomCommand(FCommand->currentText().toStdString(), FRemote->isChecked());
  if (!Check.Error.empty())
  {
    FCommand->setFocus();
    Error(this, QString::fromStdString(Check.Error));
    return false;
  }
  return true;
}

void CustomCommandDialog::accept()
{
  if (Validate())
  {
    QDialog::accept();
  }
}

bool CustomCommandPrompts(QWidget * Parent, const std::string & Name,
  const std::vector<std::pair<std::string, std::string>> & Prompts, std::vector<std::string> & Values)
{
  QDialog Dialog(Parent);
  Dialog.setWindowTitle(CoreText(CoreBridge::TText::CustomCommandsParamsTitle).replace(QStringLiteral("%s"), QString::fromStdString(Name)));
  QVBoxLayout * Layout = new QVBoxLayout(&Dialog);
  std::vector<QLineEdit *> Edits;
  for (const auto & Prompt : Prompts)
  {
    QLabel * Label = new QLabel(Prompt.first.empty() ? CoreText(CoreBridge::TText::CustomCommandsParamPrompt) :
      QString::fromStdString(Prompt.first), &Dialog);
    QLineEdit * Edit = new QLineEdit(QString::fromStdString(Prompt.second), &Dialog);
    Edit->setMinimumWidth(Edit->fontMetrics().horizontalAdvance(QLatin1Char('x')) * 40);
    Label->setBuddy(Edit);
    Layout->addWidget(Label);
    Layout->addWidget(Edit);
    Edits.push_back(Edit);
  }
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
  QObject::connect(Buttons, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  QObject::connect(Buttons, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
  Layout->addWidget(Buttons);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Values.clear();
  for (QLineEdit * Edit : Edits)
  {
    Values.push_back(Edit->text().toStdString());
  }
  return true;
}
