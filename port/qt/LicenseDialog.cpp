#include "LicenseDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"

LicenseDialog::LicenseDialog(TLicense License, QWidget * Parent) :
  QDialog(Parent)
{
  // The texts are the shell's resources: linscp's LICENSE, and WinSCP's
  // LicenseExpat.txt, whose first line, as WinSCP's resources have it, is
  // the product's name.
  QFile File((License == Expat) ? QStringLiteral(":/licenses/LicenseExpat.txt") : QStringLiteral(":/licenses/LICENSE"));
  File.open(QIODevice::ReadOnly);
  QString Text = QString::fromUtf8(File.readAll());
  Text.remove(QLatin1Char('\r'));
  QString Product = QString::fromStdString(CoreBridge::Info().AppName);
  if (License == Expat)
  {
    const int Break = Text.indexOf(QLatin1Char('\n'));
    Product = Text.left(Break);
    Text = Text.mid(Break + 1);
  }
  setWindowTitle(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::LicenseCaption)).replace(QStringLiteral("%s"), Product));

  QVBoxLayout * Layout = new QVBoxLayout(this);
  FText = new QPlainTextEdit(Text, this);
  FText->setAccessibleName(windowTitle());
  FText->setReadOnly(true);
  FText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  FText->setLineWrapMode(QPlainTextEdit::NoWrap);
  Layout->addWidget(FText);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  resize(640, 520);
}
