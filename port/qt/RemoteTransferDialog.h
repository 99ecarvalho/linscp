// Duplicate and Move to (Shift+F5, Shift+F6) within the server, as WinSCP's
// (forms/RemoteTransfer.cpp, Custom.cpp's TRemoteMoveDialog): the target
// path and file mask, and for a copy whether it goes through a local
// temporary copy (where the server cannot copy, it must).
#ifndef LINSCP_QT_REMOTE_TRANSFER_DIALOG_H
#define LINSCP_QT_REMOTE_TRANSFER_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QPushButton;

class RemoteTransferDialog : public QDialog
{
  Q_OBJECT

public:
  // Target: a directory; FileMask: "*.*", or the single file's name.
  RemoteTransferDialog(CoreBridge::TSession * Session, bool Move, bool Multi, const QString & Target,
    const QString & FileMask, QWidget * Parent = nullptr);

  // As accepted: the directory (with its slash) and the file mask.
  QString Target() const;
  QString FileMask() const;
  bool DirectCopy() const;

  QComboBox * PathCombo() const { return FPath; }
  QCheckBox * NotDirectCopyCheck() const { return FNotDirect; }
  QPushButton * OkButton() const { return FOk; }

  // OK's checks, as TRemoteTransferDialog::FormCloseQuery: an existing
  // directory takes the files in under their names, and several files to
  // one name are asked about. False when that was declined.
  bool Validate();

  void accept() override;

private:
  void UpdateControls();

  CoreBridge::TSession * FSession;
  bool FMove;
  bool FMulti;
  QComboBox * FPath = nullptr;
  QCheckBox * FNotDirect = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
