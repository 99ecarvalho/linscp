#include "WindowParams.h"

#include <QScreen>
#include <QStringList>
#include <QWidget>

QString StoreWindowParams(const QWidget * Window)
{
  const QScreen * Screen = Window->screen();
  const QRect Area = (Screen != nullptr) ? Screen->availableGeometry() : QRect();
  const QRect Bounds = Window->isMaximized() ? Window->normalGeometry() : Window->geometry();
  return QStringLiteral("%1;%2;%3;%4;%5;%6")
    .arg(Bounds.left() - Area.left()).arg(Bounds.top() - Area.top())
    .arg(Bounds.right() + 1 - Area.left()).arg(Bounds.bottom() + 1 - Area.top())
    .arg(Window->isMaximized() ? 2 : 0).arg(Window->logicalDpiX());
}

bool RestoreWindowParams(QWidget * Window, const QString & Params)
{
  const QStringList Parts = Params.split(QLatin1Char(';'));
  if (Parts.size() < 6)
  {
    return false;
  }
  const int Dpi = Window->logicalDpiX();
  const QScreen * Screen = Window->screen();
  const QRect Area = (Screen != nullptr) ? Screen->availableGeometry() : QRect(0, 0, 1280, 800);
  const int Saved = (Parts[5].toInt() > 0) ? Parts[5].toInt() : Dpi;
  const auto Scale = [Dpi, Saved](int Value) { return Value * Dpi / Saved; };
  if (Parts[0].toInt() == -1)
  {
    Window->resize(Scale(Parts[2].toInt()), Scale(Parts[3].toInt()));
  }
  else
  {
    Window->setGeometry(QRect(QPoint(Area.left() + Scale(Parts[0].toInt()), Area.top() + Scale(Parts[1].toInt())),
      QPoint(Area.left() + Scale(Parts[2].toInt()) - 1, Area.top() + Scale(Parts[3].toInt()) - 1)));
  }
  if (Parts[4].toInt() == 2)
  {
    Window->setWindowState(Window->windowState() | Qt::WindowMaximized);
  }
  return true;
}

QString StoreWindowSize(const QWidget * Window)
{
  return QStringLiteral("%1,%2,%3").arg(Window->width()).arg(Window->height()).arg(Window->logicalDpiX());
}

bool RestoreWindowSize(QWidget * Window, const QString & Size)
{
  const QStringList Parts = Size.split(QLatin1Char(','));
  if ((Parts.size() < 2) || (Parts[0].toInt() <= 0) || (Parts[1].toInt() <= 0))
  {
    return false;
  }
  const int Dpi = Window->logicalDpiX();
  const int Saved = ((Parts.size() >= 3) && (Parts[2].toInt() > 0)) ? Parts[2].toInt() : Dpi;
  Window->resize(Parts[0].toInt() * Dpi / Saved, Parts[1].toInt() * Dpi / Saved);
  return true;
}
