#include "ColorMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QPixmap>

#include <algorithm>

#include "CoreBridge.h"

QIcon SessionColorIcon(int Color)
{
  QPixmap Pixmap(16, 16);
  Pixmap.fill(QColor(Color & 0xFF, (Color >> 8) & 0xFF, (Color >> 16) & 0xFF));
  return QIcon(Pixmap);
}

void FillSessionColorMenu(QMenu * Menu, int Current, QWidget * PickerParent, const std::function<void(int)> & Choose)
{
  Menu->clear();
  QActionGroup * Group = new QActionGroup(Menu);
  QAction * Default = Menu->addAction(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ColorDefault)));
  Default->setStatusTip(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ColorDefaultHint)));
  Default->setCheckable(true);
  Default->setChecked(Current == 0);
  Group->addAction(Default);
  QObject::connect(Default, &QAction::triggered, Menu, [Choose]() { Choose(0); });
  Menu->addSeparator();
  // The names keep the main window's translations, where they were first.
  static const std::pair<const char *, int> Standard[] = {
    { QT_TRANSLATE_NOOP("MainWindow", "Rose"), 0xCC99FF }, { QT_TRANSLATE_NOOP("MainWindow", "Tan"), 0x99CCFF },
    { QT_TRANSLATE_NOOP("MainWindow", "Light Yellow"), 0x99FFFF }, { QT_TRANSLATE_NOOP("MainWindow", "Light Green"), 0xCCFFCC },
    { QT_TRANSLATE_NOOP("MainWindow", "Light Turquoise"), 0xFFFFCC }, { QT_TRANSLATE_NOOP("MainWindow", "Pale Blue"), 0xFFCC99 },
    { QT_TRANSLATE_NOOP("MainWindow", "Lavender"), 0xFF99CC }, { QT_TRANSLATE_NOOP("MainWindow", "Light Orange"), 0xB5D5FB },
    { QT_TRANSLATE_NOOP("MainWindow", "Darker White"), 0xD8D8D8 }, { QT_TRANSLATE_NOOP("MainWindow", "Darker Tan"), 0x97BDC4 },
    { QT_TRANSLATE_NOOP("MainWindow", "Lighter Blue"), 0xE2B38D }, { QT_TRANSLATE_NOOP("MainWindow", "Light Blue"), 0xE4CCB8 },
    { QT_TRANSLATE_NOOP("MainWindow", "Lighter Red"), 0xB7B9E5 }, { QT_TRANSLATE_NOOP("MainWindow", "Light Olive Green"), 0xBCE3D7 },
    { QT_TRANSLATE_NOOP("MainWindow", "Light Purple"), 0xD9C1CC }, { QT_TRANSLATE_NOOP("MainWindow", "Light Aqua"), 0xE8DDB7 } };
  bool Known = (Current == 0);
  for (const auto & Color : Standard)
  {
    QAction * Action = Menu->addAction(SessionColorIcon(Color.second), QCoreApplication::translate("MainWindow", Color.first));
    Action->setCheckable(true);
    Action->setChecked(Current == Color.second);
    Known = Known || (Current == Color.second);
    Group->addAction(Action);
    const int Value = Color.second;
    QObject::connect(Action, &QAction::triggered, Menu, [Choose, Value]() { Choose(Value); });
  }
  if (!Known)
  {
    QAction * Other = Menu->addAction(SessionColorIcon(Current), QCoreApplication::translate("MainWindow", "Custom"));
    Other->setCheckable(true);
    Other->setChecked(true);
    Group->addAction(Other);
  }
  Menu->addSeparator();
  QAction * Pick = Menu->addAction(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ColorPick)));
  QObject::connect(Pick, &QAction::triggered, Menu, [PickerParent, Current, Choose]()
  {
    const QColor Initial = (Current != 0) ? QColor(Current & 0xFF, (Current >> 8) & 0xFF, (Current >> 16) & 0xFF) : QColor(Qt::white);
    const QColor Chosen = QColorDialog::getColor(Initial, PickerParent);
    if (Chosen.isValid())
    {
      // A TColor; black would read as none, so the nearest other.
      Choose(std::max(1, Chosen.red() | (Chosen.green() << 8) | (Chosen.blue() << 16)));
    }
  });
  Pick->setStatusTip(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ColorPickHint)));
}
