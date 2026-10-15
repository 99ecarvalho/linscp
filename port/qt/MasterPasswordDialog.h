// WinSCP's master password dialog (windows/UserInterface.cpp's
// TMasterPasswordDialog): the current one asked for, or a new one (with the
// current one, when there is one) and it again.
#ifndef LINSCP_QT_MASTER_PASSWORD_DIALOG_H
#define LINSCP_QT_MASTER_PASSWORD_DIALOG_H

#include <QDialog>

#include <string>

class QDialogButtonBox;
class QLineEdit;

class MasterPasswordDialog : public QDialog
{
  Q_OBJECT

public:
  // Change: a new one; else the current one.
  MasterPasswordDialog(bool Change, QWidget * Parent = nullptr);

  // As DoMasterPasswordDialog: the current one given for this run, once
  // checked.
  static bool AskCurrent(QWidget * Parent);
  // As DoChangeMasterPasswordDialog: the current one given, the new one in
  // New.
  static bool AskChange(QWidget * Parent, std::string & New);

  QLineEdit * CurrentEdit() const { return FCurrent; }
  QLineEdit * NewEdit() const { return FNew; }
  QLineEdit * ConfirmEdit() const { return FConfirm; }
  QDialogButtonBox * Buttons() const { return FButtons; }

  void accept() override;

private:
  QLineEdit * MakeEdit(const QString & Label, class QFormLayout * Form);
  void UpdateControls();
  // What accepting it means: the current one given (when it was asked).
  void Commit();

  QLineEdit * FCurrent = nullptr;
  QLineEdit * FNew = nullptr;
  QLineEdit * FConfirm = nullptr;
  QDialogButtonBox * FButtons = nullptr;
  bool FChange = false;
};

#endif
