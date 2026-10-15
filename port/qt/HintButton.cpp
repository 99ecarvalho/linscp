#include "HintButton.h"

#include <QPushButton>
#include <QToolTip>

QPushButton * MakeHintButton(const QString & Caption, const QString & Hint, QWidget * Parent)
{
  QPushButton * Button = new QPushButton(Caption, Parent);
  Button->setFlat(true);
  Button->setToolTip(Hint);
  QObject::connect(Button, &QPushButton::clicked, Button, [Button]()
  {
    QToolTip::showText(Button->mapToGlobal(QPoint(0, Button->height())), Button->toolTip(), Button);
  });
  return Button;
}

QString MaskHint(std::initializer_list<CoreBridge::TText> Parts)
{
  QStringList Paragraphs;
  for (CoreBridge::TText Part : Parts)
  {
    Paragraphs.append(QString::fromStdString(CoreBridge::Text(Part)));
  }
  return Paragraphs.join(QStringLiteral("\n\n"));
}

QPushButton * MakeMaskHintButton(const QString & Caption, std::initializer_list<CoreBridge::TText> Parts, QWidget * Parent)
{
  return MakeHintButton(Caption, MaskHint(Parts), Parent);
}
