// The transfer queue, as WinSCP's queue panel lists it (forms/
// CustomScpExplorer's QueueView): each background transfer with its
// operation, source, destination, progress and state, and the actions on the
// selected one: cancel, pause, resume.
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
  QTreeWidget * List() const { return FList; }

private:
  std::uintptr_t SelectedId() const;
  void UpdateActions();

  CoreBridge::TSession * FSession = nullptr;
  QTreeWidget * FList = nullptr;
  QAction * FCancelAction = nullptr;
  QAction * FPauseAction = nullptr;
  QAction * FResumeAction = nullptr;
};

#endif
