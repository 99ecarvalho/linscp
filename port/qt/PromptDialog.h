// Core's prompt for credentials or other input -- username, password,
// passphrase, keyboard-interactive questions -- one field per prompt, hidden
// where the prompt says not to echo (upstream's forms/Authenticate.cpp and
// InputDlg.cpp, as one dialog), with Authenticate's "Change stored
// password to this one" and "Remember password for this session".
#ifndef LINSCP_QT_PROMPT_DIALOG_H
#define LINSCP_QT_PROMPT_DIALOG_H

#include <QDialog>

#include <functional>
#include <QList>

#include "CoreBridge.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QMenu;

class PromptDialog : public QDialog
{
  Q_OBJECT

public:
  PromptDialog(const CoreBridge::TPrompt & Prompt, QWidget * Parent = nullptr);

  // After an accepted exec(): the prompt with the fields' values filled in.
  void Fill(CoreBridge::TPrompt & Prompt) const;
  QList<QLineEdit *> Fields() const { return FFields; }
  // Authenticate's checkboxes, when the prompt offers them (else null).
  QCheckBox * SavePassword() const { return FSavePassword; }
  QCheckBox * RememberPassword() const { return FRememberPassword; }
  // As TAuthenticateForm::ExtractUrl: the first http(s) URL in Text, up to a
  // space; empty for none.
  static QString UrlIn(const QString & Text);
  // The instructions' and prompts' labels, as ExternalLabel has them: a
  // link opened by a click; their menu Copy, Open Link (LabelPopupMenu).
  QList<QLabel *> Labels() const { return FLabels; }
  QMenu * LabelMenu(QLabel * Label);
  // How links open (the desktop's browser; tests replace it).
  std::function<void(const QString & Url)> OpenUrl;

protected:
  bool eventFilter(QObject * Watched, QEvent * Event) override;

private:
  void External(QLabel * Label);
  QList<QLabel *> FLabels;
  QList<QLineEdit *> FFields;
  QCheckBox * FSavePassword = nullptr;
  QCheckBox * FRememberPassword = nullptr;
};

#endif
