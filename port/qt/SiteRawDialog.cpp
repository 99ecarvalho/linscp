#include "SiteRawDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QVBoxLayout>

#include "CoreBridge.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

}  // namespace

SiteRawDialog::SiteRawDialog(const QString & Initial, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(CoreText(CoreBridge::TText::SiteRawCaption));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FSettings = new QPlainTextEdit(Initial.trimmed(), this);
  FSettings->setLineWrapMode(QPlainTextEdit::NoWrap);
  FSettings->setTabChangesFocus(true);
  Layout->addWidget(FSettings, 1);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  QPushButton * Add = Buttons->addButton(CoreText(CoreBridge::TText::SiteRawAdd), QDialogButtonBox::ActionRole);
  connect(Add, &QPushButton::clicked, this, [this]()
  {
    // As AddButtonClick: the options not there yet.
    const auto Options = CoreBridge::RawSettingsToAdd(Settings().toStdString());
    QDialog Dialog(this);
    Dialog.setWindowTitle(CoreText(CoreBridge::TText::SiteRawAddCaption));
    QFormLayout * Fields = new QFormLayout(&Dialog);
    QComboBox * Names = new QComboBox(&Dialog);
    for (const auto & Option : Options)
    {
      Names->addItem(QString::fromStdString(Option.first));
    }
    Fields->addRow(CoreText(CoreBridge::TText::SiteRawAddLabel), Names);
    QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &Dialog);
    connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
    connect(Box, &QDialogButtonBox::rejected, &Dialog, &QDialog::reject);
    Fields->addRow(Box);
    if ((Dialog.exec() == QDialog::Accepted) && (Names->currentIndex() >= 0))
    {
      AddSetting(Names->currentText());
    }
  });
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  resize(480, 440);
}

QString SiteRawDialog::Settings() const
{
  return FSettings->toPlainText();
}

bool SiteRawDialog::AddSetting(const QString & Name)
{
  for (const auto & Option : CoreBridge::RawSettingsToAdd(Settings().toStdString()))
  {
    if (QString::fromStdString(Option.first) == Name)
    {
      const QString Value = QString::fromStdString(Option.second);
      QString Text = FSettings->toPlainText();
      if (!Text.isEmpty() && !Text.endsWith(QLatin1Char('\n')))
      {
        Text += QLatin1Char('\n');
      }
      const int Start = Text.size() + Name.size() + 1;
      FSettings->setPlainText(Text + Name + QLatin1Char('=') + Value);
      // The value selected, to type over.
      QTextCursor Cursor(FSettings->document());
      Cursor.setPosition(Start);
      Cursor.setPosition(Start + Value.size(), QTextCursor::KeepAnchor);
      FSettings->setTextCursor(Cursor);
      FSettings->setFocus();
      return true;
    }
  }
  return false;
}
