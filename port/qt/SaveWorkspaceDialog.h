// Tabs > Save Workspace, as WinSCP's (forms/Custom.cpp,
// TSaveWorkspaceDialog): the name, one of the workspaces' or a new one;
// whether the sessions' passwords are saved too (when any has one); whether
// the workspace is saved automatically from then on.
#ifndef LINSCP_QT_SAVE_WORKSPACE_DIALOG_H
#define LINSCP_QT_SAVE_WORKSPACE_DIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;

class SaveWorkspaceDialog : public QDialog
{
  Q_OBJECT

public:
  // CanSavePasswords: some session has a password to save.
  SaveWorkspaceDialog(const QString & Name, bool CanSavePasswords, bool SavePasswords, bool AutoSave,
    QWidget * Parent = nullptr);

  QString Name() const;
  bool SavePasswords() const;
  bool AutoSave() const;
  // As DoValidate: the name checked by core first.
  void accept() override;

  QComboBox * NameCombo() const { return FName; }
  QCheckBox * SavePasswordsCheck() const { return FSavePasswords; }
  QCheckBox * AutoSaveCheck() const { return FAutoSave; }
  QPushButton * OkButton() const { return FOk; }

private:
  QComboBox * FName = nullptr;
  QCheckBox * FSavePasswords = nullptr;
  QCheckBox * FAutoSave = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
