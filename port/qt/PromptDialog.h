// Core's prompt for credentials or other input -- username, password,
// passphrase, keyboard-interactive questions -- one field per prompt, hidden
// where the prompt says not to echo (upstream's forms/Authenticate.cpp and
// InputDlg.cpp, as one dialog).
#ifndef LINSCP_QT_PROMPT_DIALOG_H
#define LINSCP_QT_PROMPT_DIALOG_H

#include <QDialog>
#include <QList>

#include "CoreBridge.h"

class QLineEdit;

class PromptDialog : public QDialog
{
  Q_OBJECT

public:
  PromptDialog(const CoreBridge::TPrompt & Prompt, QWidget * Parent = nullptr);

  // After an accepted exec(): the prompt with the fields' values filled in.
  void Fill(CoreBridge::TPrompt & Prompt) const;
  QList<QLineEdit *> Fields() const { return FFields; }

private:
  QList<QLineEdit *> FFields;
};

#endif
