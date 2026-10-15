// Tabs > Save Session as Site, as WinSCP's (forms/Custom.cpp,
// TSaveSessionDialog): the site's name, its folder (one of the sites' or a
// new one), whether its password is saved. Accepted, the name has been
// checked, and overwriting another site confirmed. WinSCP's "Create
// desktop shortcut" is not here.
#ifndef LINSCP_QT_SAVE_SITE_DIALOG_H
#define LINSCP_QT_SAVE_SITE_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

class SaveSiteDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SaveSiteDialog(const CoreBridge::TSession::TSaveAsSite & Defaults, QWidget * Parent = nullptr);

  // Folder/Name.
  QString Name() const;
  bool SavePassword() const;
  // SAVE_SITE_WORKSPACE_SHORTCUT: a desktop shortcut to the site.
  bool CreateShortcut() const;
  void accept() override;

  QLineEdit * NameEdit() const { return FName; }
  QComboBox * FolderCombo() const { return FFolder; }
  QCheckBox * SavePasswordCheck() const { return FSavePassword; }
  QCheckBox * CreateShortcutCheck() const { return FCreateShortcut; }

private:
  QLineEdit * FName = nullptr;
  QComboBox * FFolder = nullptr;
  QCheckBox * FSavePassword = nullptr;
  QCheckBox * FCreateShortcut = nullptr;
  QPushButton * FOk = nullptr;
  QString FRootFolder;
};

#endif
