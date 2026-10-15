// Preferences > Integration's actions, as WinSCP's (forms/Preferences.cpp's
// IntegrationSheet): the program registered to handle the protocols' URL
// addresses (and unregistered), a desktop icon, the program's directory on
// the search path. On Windows upstream's own (Setup.cpp, through the
// bridge; the icon a shortcut); on POSIX their XDG counterparts: a desktop
// entry for the protocols' x-scheme-handler types, the default for them
// in mimeapps.list, and links in ~/.local/bin.
#ifndef LINSCP_QT_INTEGRATION_H
#define LINSCP_QT_INTEGRATION_H

#include <QString>
#include <QStringList>

#include "CoreBridge.h"

class QWidget;

namespace Integration
{

// The URL schemes registered: WinSCP's protocols, and its winscp- prefixed
// ones for browsers.
QStringList Schemes();
// Each false with Error set when it failed.
bool RegisterUrlHandler(QString & Error);
bool UnregisterUrlHandler(QString & Error);
// Windows only: its default apps settings (LaunchAdvancedAssociationUI).
bool MakeDefaultUrlHandler(QString & Error);
// AllUsers: the public desktop (Windows).
bool CreateDesktopIcon(bool AllUsers, QString & Error);
bool AddToSearchPath(QString & Error);
// A shortcut to the program (CreateDesktopShortCut): on the desktop, or in
// Explorer's Send To (Windows only). A .lnk on Windows, a desktop entry
// elsewhere. Its path in Path.
enum class TShortcutFolder { Desktop, SendTo };
bool CreateShortcut(const CoreBridge::TShortcut & Shortcut, TShortcutFolder Folder, QString & Path, QString & Error);
// The folder (for tests).
QString ShortcutFolder(TShortcutFolder Folder);
// As the Login dialog's DesktopIconActionExecute and Preferences'
// IconButtonClick: Question asked (empty: none), then the shortcut, its
// error shown. False when not made.
bool AskAndCreateShortcut(QWidget * Parent, const QString & Question, const CoreBridge::TShortcut & Shortcut,
  TShortcutFolder Folder);
// Where they go (POSIX): the desktop entry, the user's mimeapps.list, the
// search path's directory.
QString DesktopEntryPath();
QString MimeAppsPath();
QString SearchPathDirectory();

}  // namespace Integration

#endif
