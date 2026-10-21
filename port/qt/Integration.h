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
// Where they go (POSIX): the desktop entry, the user's mimeapps.list (in a
// Flatpak the host's, $HOST_XDG_CONFIG_HOME or ~/.config, not the sandbox's),
// the search path's directory.
QString DesktopEntryPath();
QString MimeAppsPath();
QString SearchPathDirectory();

// linscp's application ID: the name of its desktop file, icon and metainfo,
// of everything it writes into the user's data directory, and its Wayland
// app_id (port/packaging/CMakeLists.txt's LINSCP_APP_ID).
inline constexpr char AppId[] = "org.linscp.linscp";
// What the program starts with: Qt's desktop file name (the Wayland app_id,
// X11's _GTK_APPLICATION_ID and _KDE_NET_WM_DESKTOP_FILE), so that docks and
// task bars match its windows to the desktop file. Before any window.
void SetUpDesktopIdentity();
// How linscp is installed (POSIX; Windows always Archive): a Flatpak
// (FLATPAK_ID, /.flatpak-info), an AppImage (APPIMAGE), a package (the
// program under /usr, or the desktop file installed beside it,
// <prefix>/share/applications/<AppId>.desktop for <prefix>/lib/linscp/linscp),
// else an archive or build of its own.
enum class TInstall { Archive, AppImage, Package, Flatpak };
TInstall Install();
// Tests: the program Install() looks at, instead of this one (empty: this).
void SetProgramForTests(const QString & Program);
// Whether linscp writes its own desktop entry and icon into the user's data
// directory when it registers as the URL handler or makes a shortcut: only
// an AppImage's and an archive's. A package and a Flatpak install their own,
// which already declare the URL schemes; registering then only makes it the
// default in mimeapps.list.
bool SelfRegisters();
// At start (POSIX): what older versions wrote under their names removed or
// renamed -- the desktop entry linscp.desktop (written again as <AppId>.desktop
// when it was the URL handler and linscp registers itself), the icon
// linscp.png, mimeapps.list's linscp.desktop, the desktop's shortcuts' Icon=
// -- so that an upgrade leaves no second launcher. True when it changed
// anything.
bool CleanUpOldRegistration();

// Whether linscp may check for updates (Help > Check for Updates, the
// Updates button, Preferences > Updates' periodic check): built with
// LINSCP_UPDATE_CHECK on (the default; distributions' packages turn it off,
// their package manager updating linscp) and not running as a Flatpak
// (Install()), whose updates come from its remote.
bool UpdateChecksAllowed();

}  // namespace Integration

#endif
