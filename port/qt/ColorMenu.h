// The session color menu, as WinSCP's CreateSessionColorMenu: the default,
// WinSCP's standard session colors (GetStandardSessionColorInfo), the
// current one when it is none of them, and any other through a picker.
// Tabs > Color's and Site Advanced's Color button's.
#ifndef LINSCP_QT_COLOR_MENU_H
#define LINSCP_QT_COLOR_MENU_H

#include <functional>

class QIcon;
class QMenu;
class QWidget;

// A TColor (0x00BBGGRR; 0 none) as a swatch.
QIcon SessionColorIcon(int Color);
// Fills Menu for Current; Choose gets the color picked, 0 for the default.
void FillSessionColorMenu(QMenu * Menu, int Current, QWidget * PickerParent, const std::function<void(int)> & Choose);

#endif
