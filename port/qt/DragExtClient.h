// The program's side of WinSCP's drag & drop shell extension (P3-24), as
// TCustomScpExplorerForm's DDFakeFile* and TWinConfiguration's DDExt*: a
// fake directory dragged, which the extension's copy hook recognizes in
// Explorer and refuses, telling where it was dropped (the shared
// WinSCPDragExtMapping). Windows only; elsewhere none is installed.
#ifndef LINSCP_QT_DRAG_EXT_CLIENT_H
#define LINSCP_QT_DRAG_EXT_CLIENT_H

#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <vector>

namespace DragExt
{

// Its COM class can be made (registered), and is loaded in a process
// (Explorer's, its log mutex there).
bool Installed();
bool Running();
// The Windows 10 builds that broke it (1803).
bool Broken();

// As StartCreationDirectoryMonitorsOnEachDrive: the roots to watch for
// the fake directory's copy, where the extension is not there to tell: the
// fixed drives (not CDs, nor A: and B:), the network drives DDDrives lists,
// less those it lists as "-X". Windows only; elsewhere none.
QStringList MonitorRoots(const std::vector<std::string> & DDDrives);

// TDirectoryMonitor's, for the roots' whole trees: the directories made
// in them since. Windows only.
class TCreationMonitor
{
public:
  explicit TCreationMonitor(const QStringList & Roots);
  ~TCreationMonitor();
  // How many roots are watched.
  int Count() const;
  // A directory named Name made since (or since the last look), its full
  // path; empty for none.
  QString Found(const QString & Name);
private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

// One drag: Begin with the fake directory (named with the "scp" prefix the
// hook looks for), then, once the drop is over, Target: the folder it was
// dropped into, waited for up to Timeout milliseconds (DDExtTimeout), or
// empty.
class TDrag
{
public:
  TDrag();
  ~TDrag();
  // (The directory hidden, as CreateFakeTransferDirectory has it.)
  bool Begin(const QString & FakeDirectory);
  // Without the extension running (or with it broken): the roots watched
  // for the fake directory's copy too, the copy removed once found.
  void Monitor(const QStringList & Roots);
  QString Target(int Timeout = 1000);
private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

}  // namespace DragExt

#endif
