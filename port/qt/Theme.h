// The interface's theme, as Preferences > Interface chooses it (WinSCP's
// DarkTheme): the system's, light or dark. Qt is asked for the colour
// scheme; where the platform cannot switch (X11's generic theme), Fusion
// with a palette of that scheme stands in.
#ifndef LINSCP_QT_THEME_H
#define LINSCP_QT_THEME_H

#include "CoreBridge.h"

namespace Theme
{

// Applied to the whole application, at once.
void Apply(CoreBridge::TPreferences::TAutoSwitch DarkTheme);
// Whether what is shown is dark (the window's colour).
bool IsDark();

}  // namespace Theme

#endif
