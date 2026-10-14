// Stand-in for the header C++Builder generates from upstream's TDiscMonitor
// (packages/my/DiscMon.pas), as windows/SynchronizeController.cpp uses it:
// the local directories keep-up-to-date watches, reporting each one that
// changed (ADR 0014, amendment P3-06e).
//
// Upstream's watches from a thread of its own and fires its events through
// TThread::Synchronize. This one has no thread: the watcher is polled from the
// GUI's thread (Pump), and its events are fired there, through OnSynchronize
// when one is set, as upstream's. Watching itself is TDirectoryWatcher's
// (port/frontend), which the command-line client's keepuptodate uses.
#ifndef LINSCP_QT_DISCMON_HPP
#define LINSCP_QT_DISCMON_HPP

#include <vcl.h>
#include <Classes.hpp>

#include <memory>

class TDirectoryWatcher;

namespace Discmon
{

enum TMonitorFilter { moFilename, moDirName, moAttributes, moSize, moLastWrite, moSecurity };
using TMonitorFilters = Set<TMonitorFilter, moFilename, moSecurity>;

using TDiscMonitorNotify = TClosure<void(TObject * Sender, const UnicodeString Directory, bool & SubdirsChanged)>;
using TDiscMonitorInvalid =
  TClosure<void(TObject * Sender, const UnicodeString Directory, const UnicodeString ErrorStr)>;
using TDiscMonitorSynchronize = TClosure<void(TObject * Sender, TThreadMethod Method)>;
using TDiscMonitorFilter = TClosure<void(TObject * Sender, const UnicodeString DirectoryName, bool & Add)>;
using TDiscMonitorTooManyDirectories = TClosure<void(TObject * Sender, int & MaxDirectories)>;
using TDiscMonitorDirectoriesChange = TClosure<void(TObject * Sender, int Directories)>;

class TDiscMonitor : public TComponent
{
public:
  explicit TDiscMonitor(TComponent * AOwner);
  virtual ~TDiscMonitor();

  // Starts and stops watching the directories added.
  void Open();
  void Close();
  // Directory, and with SubDirs each directory below it OnFilter accepts,
  // asking OnTooManyDirectories past MaxDirectories (none: -1). Only one
  // tree is watched: Directory is its root.
  void AddDirectory(UnicodeString Directory, bool SubDirs);
  // linscp's: each open monitor's changes collected and its events fired.
  static void Pump();

  // The directories watched, as AddDirectory found them.
  PROP_R_G(TStrings *, Directories, GetDirectories);
  TStrings * GetDirectories() const;

  // Only those of upstream's properties SynchronizeController sets. Filters
  // is taken as moFilename and moLastWrite, with moDirName as SubDirs has it:
  // what TDirectoryWatcher reports.
  bool SubTree = true;
  TMonitorFilters Filters;
  int MaxDirectories = -1;
  int ChangeDelay = 500;

  TDiscMonitorNotify OnChange;
  TDiscMonitorInvalid OnInvalid;
  TDiscMonitorSynchronize OnSynchronize;
  TDiscMonitorFilter OnFilter;
  TDiscMonitorTooManyDirectories OnTooManyDirectories;
  TDiscMonitorDirectoriesChange OnDirectoriesChange;

private:
  std::unique_ptr<TStringList> FDirectories;
  std::unique_ptr<TDirectoryWatcher> FWatcher;
  UnicodeString FRoot;
  bool FRecursive = false;
  int FWatched = 0;
  // What the event fired through DoSynchronize reports, as upstream's thread.
  UnicodeString FNotifiedDirectory;
  bool FSubdirsChanged = false;
  UnicodeString FInvalidDirectory;
  UnicodeString FInvalidError;

  void Scan(const UnicodeString & Directory);
  bool Filter(const UnicodeString & Directory);
  // False when an event closed or freed the monitor.
  bool Poll();
  bool DoSynchronize(void (TDiscMonitor::*Method)());
  void InformChange();
  void InformInvalid();
  void InformDirectoriesChange();
};

}  // namespace Discmon

using namespace Discmon;

#endif
