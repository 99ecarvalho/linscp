// Open directory and bookmarks (Ctrl+O, Ctrl+B), as WinSCP's
// (forms/OpenDirectory.cpp): a directory to go to, typed, from the panel's
// history, or from the site's or the shared bookmarks, which it manages
// (core's TBookmarks, kept as TWinConfiguration keeps them).
#ifndef LINSCP_QT_OPEN_DIRECTORY_DIALOG_H
#define LINSCP_QT_OPEN_DIRECTORY_DIALOG_H

#include <QDialog>

#include <vector>

#include "CoreBridge.h"

class QComboBox;
class QListWidget;
class QPushButton;
class QTabWidget;

class OpenDirectoryDialog : public QDialog
{
  Q_OBJECT

public:
  // Remote: the bookmarks' remote directories, else their local ones.
  // SessionKey: the site's, empty for no site bookmarks. AddBookmark:
  // WinSCP's "Manage bookmarks", the directory added to the page Shared
  // says.
  OpenDirectoryDialog(bool Remote, const QString & Directory, const QStringList & History, const std::string & SessionKey,
    bool AddBookmark, bool Shared, QWidget * Parent = nullptr, bool AllowSwitch = false);

  QString Directory() const;
  // Location Profiles... (AllowSwitch: with a session): the profiles
  // instead, from now on.
  bool Switched() const { return FSwitched; }

  // The pages' buttons, for tests and them alike: 0 the site's, 1 the
  // shared bookmarks.
  void AddAsBookmark(int Page);
  void RemoveBookmark(int Page);
  void MoveBookmark(int Page, int By);
  // The shared bookmarks' Shortcut...: false when cancelled.
  bool ShortCutBookmark();
  QPushButton * ShortCutButton() const { return FShortCut; }
  QComboBox * DirectoryCombo() const { return FDirectory; }
  QTabWidget * Pages() const { return FPages; }
  QListWidget * List(int Page) const { return FLists[Page]; }
  QPushButton * OkButton() const { return FOk; }

  // Closing keeps the bookmarks however it closes, as upstream's.
  void done(int Result) override;

protected:
  bool eventFilter(QObject * Object, QEvent * Event) override;

private:
  QString BookmarkDirectory(const CoreBridge::TBookmarkEntry & Bookmark) const;
  void LoadList(int Page);
  int FindBookmark(int Page, const QString & Directory) const;
  void Selected(int Page);
  void UpdateControls();
  QWidget * MakePage(int Page);

  bool FRemote;
  std::string FSessionKey;
  QComboBox * FDirectory = nullptr;
  QTabWidget * FPages = nullptr;
  QWidget * FPageWidgets[2] = {};
  QListWidget * FLists[2] = {};
  QPushButton * FAdd[2] = {};
  QPushButton * FRemove[2] = {};
  QPushButton * FUp[2] = {};
  QPushButton * FDown[2] = {};
  QPushButton * FOk = nullptr;
  QPushButton * FShortCut = nullptr;
  // Each page's bookmarks, the other side's included; the list shows
  // those with this side's directory (FRows: their indexes here).
  std::vector<CoreBridge::TBookmarkEntry> FBookmarks[2];
  std::vector<int> FRows[2];
  bool FUpdating = false;
  bool FSwitched = false;
};

// The VCL's TShortCut as text ("Ctrl+Shift+F5"), as ShortCutToText.
QString ShortCutText(unsigned ShortCut);

#endif
