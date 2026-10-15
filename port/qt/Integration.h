// Preferences > Integration's actions, as WinSCP's (forms/Preferences.cpp's
// IntegrationSheet): the program registered to handle the protocols' URL
// addresses (and unregistered), a desktop icon, the program's directory on
// the search path. On Windows upstream's own (Setup.cpp, through the
// bridge; the icon a shortcut); on POSIX their XDG counterparts: a desktop
// entry for the protocols' x-scheme-handler types, the default for them
// in mimeapps.list, and links in ~/.local/bin.
#ifndef LINSCP_QT_INTEGRATION_H
#define LINSCP_QT_INTEGRATION_H

#include <QPoint>
#include <QStringList>
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
// Windows: the shell's context menu of a folder's files (as WinSCP's
// ShellDisplayContextMenu), at Global; the verb chosen ("rename" is left to
// the caller, as WinSCP's DirView renames in place), empty when none; false
// when it could not be shown (or elsewhere).
// With Items given, its items' texts instead, not shown (tests).
bool ShowSystemContextMenu(QWidget * Window, const QString & Directory, const QStringList & Names, const QPoint & Global,
  QString & Verb, QStringList * Items = nullptr);
bool HasSystemContextMenu();
// The taskbar's jump list of recent sites and workspaces (Windows; nothing
// elsewhere): Name added first or removed, else shown again (at start).
void UpdateJumpList(CoreBridge::TJumpListEntry Kind = CoreBridge::TJumpListEntry::Site, const std::string & Name = std::string(),
  bool Add = true);
// The folder (for tests).
QString ShortcutFolder(TShortcutFolder Folder);
// Local files' Properties, as the local panel's (DisplayPropertiesMenu): the
// Windows shell's dialog; elsewhere the desktop's file manager's
// (org.freedesktop.FileManager1's ShowItemProperties). False with Error
// set when there is none to show it.
bool ShowLocalProperties(const QStringList & Paths, QString & Error);
// The panels' Type column, as WinSCP's (GetFileTypeName, by name only, the
// file need not exist): the Windows shell's type name; elsewhere the
// freedesktop MIME type's description, by extension. Cached by extension.
QString FileTypeName(const QString & Name, bool Directory);
// Options > Queue > Once Empty's Sleep Computer and Shut Down Computer, as
// WinSCP's SuspendWindows and ShutDownWindows (Tools.cpp): on Windows the
// same calls, the shutdown privilege acquired first; elsewhere logind's
// Suspend and PowerOff (org.freedesktop.login1, interactive, so that polkit
// may ask). False with Error set when refused.
bool SuspendComputer(QString & Error);
bool ShutDownComputer(QString & Error);

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
