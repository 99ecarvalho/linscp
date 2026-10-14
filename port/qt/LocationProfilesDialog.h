// Location profiles, as WinSCP's (forms/LocationProfiles.cpp): Open
// Directory when the Commander's preference says so, each bookmark a local
// and a remote directory under a name, in folders, the site's or shared
// (core's TBookmarks, as the Open directory dialog's).
#ifndef LINSCP_QT_LOCATION_PROFILES_DIALOG_H
#define LINSCP_QT_LOCATION_PROFILES_DIALOG_H

#include <QDialog>

#include <vector>

#include "CoreBridge.h"

class QComboBox;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

class LocationProfilesDialog : public QDialog
{
  Q_OBJECT

public:
  // Remote: started from the remote panel. AddBookmark: the directories
  // added as a profile first (its name asked).
  LocationProfilesDialog(const QString & LocalDirectory, const QString & RemoteDirectory, const QStringList & LocalHistory,
    const QStringList & RemoteHistory, bool Remote, const std::string & SessionKey, bool AddBookmark, QWidget * Parent = nullptr);

  QString LocalDirectory() const;
  QString RemoteDirectory() const;
  // Bookmarks...: the Open directory dialog instead, from now on.
  bool Switched() const { return FSwitched; }

  // The pages' buttons, for tests and them alike (0 the site's, 1 shared):
  // with the name or folder the dialogs would ask for given.
  bool AddProfile(int Page, const QString & Name, bool Initial = false, bool Shared = false);
  void RemoveSelected(int Page, bool Confirm = true);
  bool RenameSelected(int Page, const QString & Name);
  bool MoveSelectedTo(int Page, const QString & Folder);
  void MoveSelected(int Page, int By);
  // The shared profiles' Shortcut...: false when cancelled.
  bool ShortCutSelected();
  QPushButton * ShortCutButton() const { return FShortCut; }
  // Selects the profile of that name (in a folder: "folder/name").
  bool Select(int Page, const QString & Path);
  QTreeWidget * Tree(int Page) const { return FTrees[Page]; }
  QTabWidget * Pages() const { return FPages; }
  QComboBox * LocalCombo() const { return FLocal; }
  QComboBox * RemoteCombo() const { return FRemote; }
  QPushButton * OkButton() const { return FOk; }
  const std::vector<CoreBridge::TBookmarkEntry> Profiles(int Page) const;

  void done(int Result) override;

protected:
  // Delete removes, F2 renames, as ProfilesViewKeyDown.
  bool eventFilter(QObject * Object, QEvent * Event) override;

private:
  struct TProfile
  {
    CoreBridge::TBookmarkEntry Entry;
    int Id = 0;
  };
  int Page() const;
  QWidget * MakePage(int Page);
  void LoadTree(int Page, int SelectId, const QString & SelectFolder = QString());
  // The selected profile's index in FProfiles, -1 for none (or a folder).
  int SelectedIndex(int Page) const;
  QString SelectedFolder(int Page) const;
  void FindProfile();
  void Selected(int Page);
  void UpdateControls();
  bool AskName(int Page, QString & Name, bool Initial, bool & Shared);
  void Error(const QString & Message);
  QString ProfileText(const CoreBridge::TBookmarkEntry & Entry) const;

  bool FRemoteSide;
  std::string FSessionKey;
  QComboBox * FLocal = nullptr;
  QComboBox * FRemote = nullptr;
  QTabWidget * FPages = nullptr;
  QWidget * FPageWidgets[2] = {};
  QTreeWidget * FTrees[2] = {};
  QPushButton * FAdd[2] = {};
  QPushButton * FRemove[2] = {};
  QPushButton * FRename[2] = {};
  QPushButton * FMoveTo[2] = {};
  QPushButton * FUp[2] = {};
  QPushButton * FDown[2] = {};
  QPushButton * FOk = nullptr;
  QPushButton * FShortCut = nullptr;
  std::vector<TProfile> FProfiles[2];
  int FNextId = 1;
  bool FChanging = false;
  bool FSwitched = false;
};

#endif
