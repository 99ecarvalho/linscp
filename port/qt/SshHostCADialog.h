// WinSCP's trusted host certification authority dialog (forms/Custom.cpp's
// TSshHostCADialog): its name, public key (typed, or a file's), the hosts it
// may certify, and the RSA signature types allowed.
#ifndef LINSCP_QT_SSH_HOST_CA_DIALOG_H
#define LINSCP_QT_SSH_HOST_CA_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

class SshHostCADialog : public QDialog
{
  Q_OBJECT

public:
  SshHostCADialog(bool Add, const CoreBridge::TSshHostCA & CA, QWidget * Parent = nullptr);

  CoreBridge::TSshHostCA CA() const;

  QLineEdit * NameEdit() const { return FName; }
  QLineEdit * PublicKeyEdit() const { return FPublicKey; }
  QLabel * StatusLabel() const { return FStatus; }
  QLineEdit * HostsEdit() const { return FHosts; }
  QDialogButtonBox * Buttons() const { return FButtons; }
  // As BrowseButtonClick, for a file given.
  void LoadKeyFile(const QString & FileName);

  void accept() override;

private:
  void UpdateControls();

  QLineEdit * FName = nullptr;
  QLineEdit * FPublicKey = nullptr;
  QLabel * FStatus = nullptr;
  QLineEdit * FHosts = nullptr;
  QCheckBox * FSha1 = nullptr;
  QCheckBox * FSha256 = nullptr;
  QCheckBox * FSha512 = nullptr;
  QDialogButtonBox * FButtons = nullptr;
};

#endif
