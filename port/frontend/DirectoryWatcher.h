// Watching a local directory tree for changes, for keepuptodate: what
// upstream's TDiscMonitor (a Delphi component, packages/my/DiscMon.pas)
// gives TSynchronizeController.
//
// Changes are reported per directory -- the directory a file was created,
// written, deleted or renamed in, or a subdirectory created or removed in
// -- once they have settled for the change delay, as TDiscMonitor's
// ChangeDelay has it. A directory the filter rejects is not watched, nor is
// anything below it. Single-threaded: the caller waits on the watcher in its
// own loop. inotify on Linux (one watch per directory, new ones added as
// they appear); ReadDirectoryChangesW on Windows (one recursive watch).
#ifndef LINSCP_DIRECTORY_WATCHER_H
#define LINSCP_DIRECTORY_WATCHER_H

#include <Classes.hpp>
#include <functional>
#include <memory>
#include <vector>

class TDirectoryWatcher
{
public:
  // Whether a directory (a full path, below the root) is to be watched.
  using TFilter = std::function<bool(const UnicodeString & Directory)>;

  // Raises an exception when the root cannot be watched.
  TDirectoryWatcher(const UnicodeString & Root, bool Recursive, TFilter Filter);
  ~TDirectoryWatcher();

  // How many directories are watched.
  int Directories() const;
  // Waits up to Timeout milliseconds for changes and collects them.
  void Wait(unsigned int Timeout);
  // The directories whose last change is at least Delay milliseconds old,
  // each once, in the order they first changed; they are forgotten.
  std::vector<UnicodeString> TakeSettled(unsigned int Delay);
  // Empty while watching works; otherwise why it stopped, with the
  // directory concerned (or empty) in ErrorDirectory.
  UnicodeString Error() const;
  UnicodeString ErrorDirectory() const;

private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

#endif
