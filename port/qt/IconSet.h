// The shell's icons (P3-17): WinSCP's own may not be reused (its license.txt,
// part B), so its commands take theirs from a bundled, freely licensed set
// (assets/icons/<set>/<role>.svg, port/tools/import_icons.py), chosen in
// Preferences, or (POSIX) from the desktop's icon theme. A role is the
// command's, after WinSCP's glyphs ("refresh", "new-folder", ...).
//
// The sets' icons are single-coloured: drawn from the SVG at the size and
// screen scale asked for, in the palette's text colour (dimmed when
// disabled), so they suit light and dark themes alike.
#ifndef LINSCP_QT_ICON_SET_H
#define LINSCP_QT_ICON_SET_H

#include <QIcon>
#include <QString>

#include <vector>

namespace IconSet
{

struct TSet
{
  QString Id;
  QString Name;
  // Its license, for About; empty for the theme and none.
  QString License;
};

// The sets offered here: the bundled ones, the desktop's theme (POSIX
// only), and none.
std::vector<TSet> Sets();
// The platform's default: Fluent UI on Windows (its own look), Lucide
// elsewhere.
QString DefaultSet();
// The set used: one of Sets() (an unknown one is the default's).
QString Current();
void SetCurrent(const QString & Id);
// The role's icon in the current set; a null icon for "none" (and for a
// role no set has).
QIcon Icon(const QString & Role);
// The roles' names in freedesktop.org's icon naming, for the theme.
QString ThemeName(const QString & Role);

}  // namespace IconSet

#endif
