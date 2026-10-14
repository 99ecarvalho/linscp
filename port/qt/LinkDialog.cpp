#include "LinkDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"

LinkDialog::LinkDialog(const QString & FileName, const QString & PointTo, bool Remote, bool Edit, bool AllowHardLink,
    QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(QString::fromStdString(CoreBridge::Text(Edit ? CoreBridge::TText::LinkEditCaption : CoreBridge::TText::LinkAddCaption)));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QGroupBox * Group = new QGroupBox(this);
  QVBoxLayout * Fields = new QVBoxLayout(Group);
  QLabel * FileNameLabel = new QLabel(tr("&Link/shortcut file:"), Group);
  FFileName = new QLineEdit(FileName, Group);
  FFileName->setReadOnly(Edit);
  FileNameLabel->setBuddy(FFileName);
  QLabel * PointToLabel = new QLabel(tr("&Point link/shortcut to:"), Group);
  FPointTo = new QLineEdit(PointTo, Group);
  PointToLabel->setBuddy(FPointTo);
  FHardLink = new QCheckBox(tr("&Hard link"), Group);
  FHardLink->setEnabled(Remote && !Edit && AllowHardLink);
  for (QWidget * Widget : std::initializer_list<QWidget *>{ FileNameLabel, FFileName, PointToLabel, FPointTo, FHardLink })
  {
    Fields->addWidget(Widget);
  }
  Layout->addWidget(Group);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  connect(FFileName, &QLineEdit::textChanged, this, &LinkDialog::UpdateControls);
  connect(FPointTo, &QLineEdit::textChanged, this, &LinkDialog::UpdateControls);
  (Edit ? FPointTo : FFileName)->setFocus();
  resize(qMax(sizeHint().width(), 400), sizeHint().height());
  UpdateControls();
}

// As TSymlinkDialog::UpdateControls.
void LinkDialog::UpdateControls()
{
  FOk->setEnabled(!FFileName->text().isEmpty() && !FPointTo->text().isEmpty());
}

QString LinkDialog::FileName() const
{
  return FFileName->text();
}

QString LinkDialog::PointTo() const
{
  return FPointTo->text();
}

bool LinkDialog::SymbolicLink() const
{
  return !FHardLink->isChecked();
}
