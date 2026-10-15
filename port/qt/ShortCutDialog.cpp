#include "ShortCutDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

#include "CoreBridge.h"
#include "OpenDirectoryDialog.h"

void FillShortCutCombo(QComboBox * Combo, const std::vector<unsigned> & Used, unsigned Current)
{
  Combo->clear();
  Combo->addItem(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ShortCutNone)), 0u);
  const auto Offer = [Combo, &Used](unsigned ShortCut)
  {
    if (std::find(Used.begin(), Used.end(), ShortCut) == Used.end())
    {
      Combo->addItem(ShortCutText(ShortCut), ShortCut);
    }
  };
  // FirstCtrlNumberShortCut..LastCtrlNumberShortCut, then
  // FirstShiftCtrlAltLetterShortCut..LastShiftCtrlAltLetterShortCut.
  for (unsigned Key = '0'; Key <= '9'; Key++)
  {
    Offer(0x4000u | Key);
  }
  for (unsigned Key = 'A'; Key <= 'Z'; Key++)
  {
    Offer(0xE000u | Key);
  }
  // As SetShortCutCombo: one not offered goes after the last smaller one.
  for (int Index = Combo->count() - 1; Index >= 0; Index--)
  {
    const unsigned ShortCut = Combo->itemData(Index).toUInt();
    if (ShortCut == Current)
    {
      Combo->setCurrentIndex(Index);
      break;
    }
    else if (ShortCut < Current)
    {
      Combo->insertItem(Index + 1, ShortCutText(Current), Current);
      Combo->setCurrentIndex(Index + 1);
      break;
    }
  }
}

std::vector<unsigned> CustomCommandShortCuts()
{
  std::vector<unsigned> Result;
  for (const auto & List : { CoreBridge::CustomCommands(), CoreBridge::Extensions() })
  {
    for (const CoreBridge::TCustomCommandDefinition & Command : List)
    {
      if (Command.ShortCut != 0)
      {
        Result.push_back(Command.ShortCut);
      }
    }
  }
  return Result;
}

ShortCutDialog::ShortCutDialog(const std::vector<unsigned> & Used, unsigned Current, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ShortCutCaption)));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  FCombo = new QComboBox(this);
  FillShortCutCombo(FCombo, Used, Current);
  QLabel * Label = new QLabel(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ShortCutLabel)), this);
  Label->setBuddy(FCombo);
  Form->addRow(Label, FCombo);
  Layout->addLayout(Form);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  resize(qMax(sizeHint().width(), 320), sizeHint().height());
}

unsigned ShortCutDialog::ShortCut() const
{
  return FCombo->currentData().toUInt();
}
