// Commands > Open Terminal (Shift+Ctrl+T), as WinSCP's console
// (forms/Console.cpp): a command run on the server (the protocol's own,
// or a separate shell session's), its output below, in the current
// directory, which a "cd" changes.
#ifndef LINSCP_QT_CONSOLE_DIALOG_H
#define LINSCP_QT_CONSOLE_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class ConsoleDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ConsoleDialog(CoreBridge::TSession * Session, QWidget * Parent = nullptr);
  ~ConsoleDialog() override;

  // Execute: the command, its output added. False when core failed (its
  // error shown).
  bool Execute(const QString & Command);
  bool Executing() const { return FExecuting; }
  // A command's output shown, as DoConsoleDialog's Log.
  void AddOutput(const QStringList & Lines);

  QComboBox * CommandCombo() const { return FCommand; }
  QPlainTextEdit * Output() const { return FOutput; }
  QLabel * DirectoryLabel() const { return FDirectory; }
  QPushButton * ExecuteButton() const { return FExecute; }

  // The output's menu, as PopupMenu: Copy, Select All, Adjust Window
  // (DoAdjustWindow: the window sized to the output's longest line and
  // its lines, within the screen).
  class QMenu * OutputMenu();
  void AdjustWindow();

  void reject() override;

private:
  QSize FAutoSize;
  void UpdateControls();

  CoreBridge::TSession * FSession;
  QLabel * FDirectory = nullptr;
  QComboBox * FCommand = nullptr;
  QPushButton * FExecute = nullptr;
  QPushButton * FClose = nullptr;
  QPlainTextEdit * FOutput = nullptr;
  bool FExecuting = false;
};

#endif
