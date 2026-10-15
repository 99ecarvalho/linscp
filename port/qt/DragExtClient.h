// The program's side of WinSCP's drag & drop shell extension (P3-24), as
// TCustomScpExplorerForm's DDFakeFile* and TWinConfiguration's DDExt*: a
// fake directory dragged, which the extension's copy hook recognizes in
// Explorer and refuses, telling where it was dropped (the shared
// WinSCPDragExtMapping). Windows only; elsewhere none is installed.
#ifndef LINSCP_QT_DRAG_EXT_CLIENT_H
#define LINSCP_QT_DRAG_EXT_CLIENT_H

#include <QString>

#include <memory>

namespace DragExt
{

// Its COM class can be made (registered), and is loaded in a process
// (Explorer's, its log mutex there).
bool Installed();
bool Running();
// The Windows 10 builds that broke it (1803).
bool Broken();

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
  QString Target(int Timeout = 1000);
private:
  struct TImpl;
  std::unique_ptr<TImpl> F;
};

}  // namespace DragExt

#endif
