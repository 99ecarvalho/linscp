// Create folder (F7), as WinSCP's (forms/CreateDirectory.cpp): the new
// folder's name and, for a remote one, the permissions to give it, kept
// for next time when asked to.
#ifndef LINSCP_QT_CREATE_DIRECTORY_DIALOG_H
#define LINSCP_QT_CREATE_DIRECTORY_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class RightsFrame;

class CreateDirectoryDialog : public QDialog
{
  Q_OBJECT

public:
  // Remote: with the attributes; CanChangeRights: the server can set them.
  CreateDirectoryDialog(const QString & Name, bool Remote, bool CanChangeRights,
    const CoreBridge::TNewDirectory & Settings, QWidget * Parent = nullptr);

  QString Name() const;
  CoreBridge::TNewDirectory Settings() const;
  bool SaveSettings() const;

  // Its controls, for tests.
  QLineEdit * NameEdit() const { return FName; }
  QCheckBox * SetRightsCheck() const { return FSetRights; }
  RightsFrame * Rights() const { return FRights; }
  QCheckBox * SaveSettingsCheck() const { return FSaveSettings; }
  QGroupBox * AttributesGroup() const { return FAttributes; }
  QPushButton * OkButton() const { return FOk; }

private:
  void UpdateControls();

  bool FCanChangeRights;
  QLineEdit * FName = nullptr;
  QGroupBox * FAttributes = nullptr;
  QCheckBox * FSetRights = nullptr;
  RightsFrame * FRights = nullptr;
  QCheckBox * FSaveSettings = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
