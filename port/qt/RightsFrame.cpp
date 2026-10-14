#include "RightsFrame.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpressionValidator>

namespace
{

// Core's TRight order, as flags.
const unsigned Flags[12] = {
  04000, 02000, 01000,
  00400, 00200, 00100,
  00040, 00020, 00010,
  00004, 00002, 00001 };

}  // namespace

RightsFrame::RightsFrame(QWidget * Parent) :
  QWidget(Parent)
{
  QGridLayout * Layout = new QGridLayout(this);
  Layout->setContentsMargins(0, 0, 0, 0);
  const QString Groups[3] = { tr("&Owner"), tr("&Group"), tr("Ot&hers") };
  const QString Levels[3] = { tr("R"), tr("W"), tr("X") };
  const QString Specials[3] = { tr("Set UID"), tr("Set GID"), tr("Sticky bit") };
  for (int Group = 0; Group < 3; Group++)
  {
    // The group's name cycles its row, as WinSCP's buttons do.
    QPushButton * Button = new QPushButton(Groups[Group], this);
    Button->setFlat(true);
    Button->setFocusPolicy(Qt::NoFocus);
    Layout->addWidget(Button, Group, 0);
    connect(Button, &QPushButton::clicked, this, [this, Group]()
    {
      bool All = true;
      for (int Level = 0; Level < 3; Level++)
      {
        All = All && (FChecks[3 + Group * 3 + Level]->checkState() == Qt::Checked);
      }
      for (int Level = 0; Level < 3; Level++)
      {
        FChecks[3 + Group * 3 + Level]->setCheckState(All ? Qt::Unchecked : Qt::Checked);
      }
    });
    for (int Level = 0; Level < 3; Level++)
    {
      QCheckBox * Box = new QCheckBox(Levels[Level], this);
      FChecks[3 + Group * 3 + Level] = Box;
      Layout->addWidget(Box, Group, 1 + Level);
    }
    QCheckBox * Special = new QCheckBox(Specials[Group], this);
    FChecks[Group] = Special;
    Layout->addWidget(Special, Group, 4);
  }
  QLabel * OctalLabel = new QLabel(tr("O&ctal:"), this);
  FOctal = new QLineEdit(this);
  FOctal->setMaxLength(4);
  FOctal->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-7]{0,4}")), FOctal));
  FOctal->setMaximumWidth(FOctal->fontMetrics().horizontalAdvance(QStringLiteral("00000")) + 16);
  OctalLabel->setBuddy(FOctal);
  Layout->addWidget(OctalLabel, 3, 0);
  Layout->addWidget(FOctal, 3, 1, 1, 3, Qt::AlignLeft);
  FDirectoriesX = new QCheckBox(tr("Add &X to directories"), this);
  FDirectoriesX->setVisible(false);
  Layout->addWidget(FDirectoriesX, 3, 4);
  Layout->setColumnStretch(5, 1);
  for (QCheckBox * Box : FChecks)
  {
    connect(Box, &QCheckBox::checkStateChanged, this, [this]()
    {
      if (!FUpdating)
      {
        UpdateControls();
      }
    });
  }
  connect(FDirectoriesX, &QCheckBox::toggled, this, &RightsFrame::Changed);
  connect(FOctal, &QLineEdit::textEdited, this, &RightsFrame::OctalEdited);
  UpdateControls();
}

void RightsFrame::SetAllowUndef(bool Allow)
{
  FAllowUndef = Allow;
  for (QCheckBox * Box : FChecks)
  {
    Box->setTristate(Allow);
  }
}

void RightsFrame::SetRights(const CoreBridge::TRightsState & Rights)
{
  FUpdating = true;
  for (int Right = 0; Right < 12; Right++)
  {
    const unsigned Flag = Flags[Right];
    Qt::CheckState State = (Rights.Set & Flag) ? Qt::Checked : ((Rights.Unset & Flag) ? Qt::Unchecked : Qt::PartiallyChecked);
    if ((State == Qt::PartiallyChecked) && !FAllowUndef)
    {
      State = Qt::Unchecked;
    }
    FChecks[Right]->setCheckState(State);
  }
  FUpdating = false;
  UpdateControls();
}

CoreBridge::TRightsState RightsFrame::Rights() const
{
  CoreBridge::TRightsState Result;
  Result.Set = 0;
  Result.Unset = 0;
  for (int Right = 0; Right < 12; Right++)
  {
    switch (FChecks[Right]->checkState())
    {
      case Qt::Checked: Result.Set |= Flags[Right]; break;
      case Qt::Unchecked: Result.Unset |= Flags[Right]; break;
      default: break;
    }
  }
  return Result;
}

void RightsFrame::SetAllowAddXToDirectories(bool Allow)
{
  FDirectoriesX->setVisible(Allow);
  UpdateControls();
}

void RightsFrame::SetAddXToDirectories(bool Add)
{
  FDirectoriesX->setChecked(Add);
}

bool RightsFrame::AddXToDirectories() const
{
  return FDirectoriesX->isChecked();
}

QString RightsFrame::Text() const
{
  QString Result = QString::fromStdString(CoreBridge::RightsText(Rights()));
  if (!FDirectoriesX->isHidden() && FDirectoriesX->isEnabled() && FDirectoriesX->isChecked())
  {
    Result += QStringLiteral(" (+x)");
  }
  return Result;
}

// As TRightsFrame::UpdateControls: the octal form unless being typed, and
// "add X" only where it would add something.
void RightsFrame::UpdateControls()
{
  const CoreBridge::TRightsState State = Rights();
  const bool Undef = ((State.Set | State.Unset) & 07777) != 07777;
  if (!FOctal->hasFocus())
  {
    FOctal->setText(Undef ? QString() : QStringLiteral("%1").arg(State.Set, 4, 8, QLatin1Char('0')));
  }
  FDirectoriesX->setEnabled((State.Set & 00111) != 00111);
  emit Changed();
}

void RightsFrame::OctalEdited()
{
  // As TRightsFrame::OctalEditChange: applied once it is three digits.
  const QString Text = FOctal->text();
  if (Text.size() >= 3)
  {
    CoreBridge::TRightsState State;
    State.Set = Text.toUInt(nullptr, 8);
    State.Unset = 07777 & ~State.Set;
    SetRights(State);
  }
}

void RightsFrame::NoRights()
{
  CoreBridge::TRightsState State;
  State.Set = 0;
  State.Unset = 07777;
  SetRights(State);
}

void RightsFrame::DefaultRights()
{
  CoreBridge::TRightsState State;
  State.Set = 00644;
  State.Unset = 07777 & ~State.Set;
  SetRights(State);
}

void RightsFrame::AllRights()
{
  CoreBridge::TRightsState State;
  State.Set = 00777;
  State.Unset = 07777 & ~State.Set;
  SetRights(State);
}

void RightsFrame::LeaveAsIs()
{
  CoreBridge::TRightsState State;
  State.Set = 0;
  State.Unset = 0;
  SetRights(State);
}

void RightsFrame::CopyAsText()
{
  QApplication::clipboard()->setText(Text());
}

void RightsFrame::CopyAsOctal()
{
  QApplication::clipboard()->setText(FOctal->text());
}

void RightsFrame::Paste()
{
  QString Value = QApplication::clipboard()->text().trimmed();
  // As TRightsFrame::SetText: "add X" may come with it.
  const QString Suffix = QStringLiteral(" (+x)");
  const bool AddX = Value.endsWith(Suffix, Qt::CaseInsensitive);
  if (AddX)
  {
    Value.chop(Suffix.size());
  }
  CoreBridge::TRightsState State;
  if (CoreBridge::RightsFromText(Value.toStdString(), State) && (FAllowUndef || (((State.Set | State.Unset) & 07777) == 07777)))
  {
    SetRights(State);
    if (!FDirectoriesX->isHidden())
    {
      FDirectoriesX->setChecked(AddX);
    }
  }
}

void RightsFrame::contextMenuEvent(QContextMenuEvent * Event)
{
  QMenu Menu(this);
  Menu.addAction(tr("&No Rights"), this, &RightsFrame::NoRights);
  Menu.addAction(tr("&Default Rights"), this, &RightsFrame::DefaultRights);
  Menu.addAction(tr("&All Rights"), this, &RightsFrame::AllRights);
  if (FAllowUndef)
  {
    Menu.addAction(tr("&Leave as Is"), this, &RightsFrame::LeaveAsIs);
  }
  Menu.addSeparator();
  Menu.addAction(tr("&Copy as Text"), this, &RightsFrame::CopyAsText);
  Menu.addAction(tr("Copy as &Octal"), this, &RightsFrame::CopyAsOctal)->setEnabled(!FOctal->text().isEmpty());
  Menu.addAction(tr("&Paste"), this, &RightsFrame::Paste);
  Menu.exec(Event->globalPos());
}
