// The transfer queue, as WinSCP's queue panel lists it (forms/
// CustomScpExplorer's QueueView): each background transfer with its
// operation, source, destination, progress and state, and WinSCP's queue
// commands (TQueueController's operations): on the selected entry (show its
// query, error or prompt, execute now, suspend, resume, cancel, move up or
// down) and on all of them. The toolbar, the list's context menu and the
// window's Commands > Queue share the actions.
#ifndef LINSCP_QT_QUEUE_PANEL_H
#define LINSCP_QT_QUEUE_PANEL_H

#include <QWidget>

#include <vector>

#include "CoreBridge.h"

class QAction;
class QTreeWidget;

class QueuePanel : public QWidget
{
  Q_OBJECT

public:
  explicit QueuePanel(QWidget * Parent = nullptr);

  void SetSession(CoreBridge::TSession * Session);
  // Shows the session's queue as it now is.
  void Update();

  // The actions, on the selected entry: for tests and the buttons alike.
  bool CancelSelected();
  bool PauseSelected();
  bool ResumeSelected();
  using TOperation = CoreBridge::TSession::TQueueOperation;
  // The action doing Operation, enabled when it applies.
  QAction * Action(TOperation Operation) const;
  // Commands > Queue's items after Process Queue and Go To, as WinSCP's
  // (the list's context menu has the same).
  void AddCommands(class QMenu * Menu) const;
  QTreeWidget * List() const { return FList; }
  // The icons, of the set chosen (IconSet).
  void ApplyIcons();

private:
  std::uintptr_t SelectedId() const;
  void UpdateActions();
  void Execute(TOperation Operation);

  CoreBridge::TSession * FSession = nullptr;
  QTreeWidget * FList = nullptr;
  QAction * FCancelAction = nullptr;
  QAction * FPauseAction = nullptr;
  QAction * FResumeAction = nullptr;
  QAction * FActions[static_cast<int>(TOperation::DeleteAll) + 1] = {};
};

#endif
