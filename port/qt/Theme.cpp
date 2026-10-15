#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

namespace
{

// What the application started with, to go back to for the system's.
struct TStartup
{
  bool Saved = false;
  QString Style;
} Startup;

// Fusion's light colours (its standard palette follows the system's scheme,
// so it is no light one on a dark desktop).
QPalette LightPalette()
{
  QPalette Palette;
  const QColor Window(0xef, 0xef, 0xef);
  const QColor Text(Qt::black);
  const QColor Disabled(0x9e, 0x9e, 0x9e);
  Palette.setColor(QPalette::Window, Window);
  Palette.setColor(QPalette::WindowText, Text);
  Palette.setColor(QPalette::Base, Qt::white);
  Palette.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf7, 0xf7));
  Palette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xdc));
  Palette.setColor(QPalette::ToolTipText, Text);
  Palette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
  Palette.setColor(QPalette::Text, Text);
  Palette.setColor(QPalette::Button, Window);
  Palette.setColor(QPalette::ButtonText, Text);
  Palette.setColor(QPalette::BrightText, Qt::white);
  Palette.setColor(QPalette::Light, Qt::white);
  Palette.setColor(QPalette::Midlight, QColor(0xca, 0xca, 0xca));
  Palette.setColor(QPalette::Mid, QColor(0xb8, 0xb8, 0xb8));
  Palette.setColor(QPalette::Dark, QColor(0x9f, 0x9f, 0x9f));
  Palette.setColor(QPalette::Shadow, QColor(0x76, 0x76, 0x76));
  Palette.setColor(QPalette::Highlight, QColor(0x30, 0x8c, 0xc6));
  Palette.setColor(QPalette::HighlightedText, Qt::white);
  Palette.setColor(QPalette::Link, QColor(0x00, 0x00, 0xff));
  Palette.setColor(QPalette::LinkVisited, QColor(0xff, 0x00, 0xff));
  for (QPalette::ColorRole Role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
  {
    Palette.setColor(QPalette::Disabled, Role, Disabled);
  }
  Palette.setColor(QPalette::Disabled, QPalette::Base, Window);
  Palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x91, 0x91, 0x91));
  return Palette;
}

QPalette FusionPalette(bool Dark)
{
  if (!Dark)
  {
    return LightPalette();
  }
  // Fusion's dark colours, as Qt's own dark scheme has them.
  QPalette Palette;
  const QColor Window(0x2d, 0x2d, 0x2d);
  const QColor Base(0x1e, 0x1e, 0x1e);
  const QColor Text(0xf0, 0xf0, 0xf0);
  const QColor Disabled(0x80, 0x80, 0x80);
  const QColor Highlight(0x30, 0x7c, 0xd6);
  Palette.setColor(QPalette::Window, Window);
  Palette.setColor(QPalette::WindowText, Text);
  Palette.setColor(QPalette::Base, Base);
  Palette.setColor(QPalette::AlternateBase, QColor(0x26, 0x26, 0x26));
  Palette.setColor(QPalette::ToolTipBase, Window);
  Palette.setColor(QPalette::ToolTipText, Text);
  Palette.setColor(QPalette::PlaceholderText, Disabled);
  Palette.setColor(QPalette::Text, Text);
  Palette.setColor(QPalette::Button, QColor(0x35, 0x35, 0x35));
  Palette.setColor(QPalette::ButtonText, Text);
  Palette.setColor(QPalette::BrightText, Qt::red);
  Palette.setColor(QPalette::Light, QColor(0x50, 0x50, 0x50));
  Palette.setColor(QPalette::Midlight, QColor(0x40, 0x40, 0x40));
  Palette.setColor(QPalette::Mid, QColor(0x25, 0x25, 0x25));
  Palette.setColor(QPalette::Dark, QColor(0x18, 0x18, 0x18));
  Palette.setColor(QPalette::Shadow, Qt::black);
  Palette.setColor(QPalette::Highlight, Highlight);
  Palette.setColor(QPalette::HighlightedText, Qt::white);
  Palette.setColor(QPalette::Link, QColor(0x5a, 0xa9, 0xff));
  Palette.setColor(QPalette::LinkVisited, QColor(0xb0, 0x8c, 0xff));
  for (QPalette::ColorRole Role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText })
  {
    Palette.setColor(QPalette::Disabled, Role, Disabled);
  }
  Palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x50, 0x50, 0x50));
  return Palette;
}

}  // namespace

namespace Theme
{

void Apply(CoreBridge::TPreferences::TAutoSwitch DarkTheme)
{
  if (!Startup.Saved)
  {
    Startup.Saved = true;
    Startup.Style = QApplication::style()->name();
  }
  using TAutoSwitch = CoreBridge::TPreferences::TAutoSwitch;
  const Qt::ColorScheme Wanted = (DarkTheme == TAutoSwitch::On) ? Qt::ColorScheme::Dark :
    ((DarkTheme == TAutoSwitch::Off) ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown);
  QApplication::styleHints()->setColorScheme(Wanted);
  if (Wanted == Qt::ColorScheme::Unknown)
  {
    // The system's: as started.
    if (QApplication::style()->name() != Startup.Style)
    {
      QApplication::setStyle(Startup.Style);
    }
    // (An empty palette: the platform's own, following it.)
    QApplication::setPalette(QPalette());
    return;
  }
  if (QApplication::styleHints()->colorScheme() == Wanted)
  {
    // The platform switched (Windows, the portal's desktops): its palette.
    if (QApplication::style()->name() != Startup.Style)
    {
      QApplication::setStyle(Startup.Style);
    }
    QApplication::setPalette(QPalette());
    return;
  }
  if (QApplication::style()->name().compare(QStringLiteral("fusion"), Qt::CaseInsensitive) != 0)
  {
    QApplication::setStyle(QStringLiteral("Fusion"));
  }
  QApplication::setPalette(FusionPalette(Wanted == Qt::ColorScheme::Dark));
}

bool IsDark()
{
  return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

}  // namespace Theme
