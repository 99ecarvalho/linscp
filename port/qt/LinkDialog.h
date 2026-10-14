// Add or edit a link (Alt+F6), as WinSCP's (forms/Symlink.cpp): the link's
// name, where it points, and, remotely where the server can, a hard link
// instead of a symbolic one.
#ifndef LINSCP_QT_LINK_DIALOG_H
#define LINSCP_QT_LINK_DIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QPushButton;

class LinkDialog : public QDialog
{
  Q_OBJECT

public:
  // Edit: an existing link, its name fixed.
  LinkDialog(const QString & FileName, const QString & PointTo, bool Remote, bool Edit, bool AllowHardLink,
    QWidget * Parent = nullptr);

  QString FileName() const;
  QString PointTo() const;
  bool SymbolicLink() const;

  QLineEdit * FileNameEdit() const { return FFileName; }
  QLineEdit * PointToEdit() const { return FPointTo; }
  QCheckBox * HardLinkCheck() const { return FHardLink; }
  QPushButton * OkButton() const { return FOk; }

private:
  void UpdateControls();

  QLineEdit * FFileName = nullptr;
  QLineEdit * FPointTo = nullptr;
  QCheckBox * FHardLink = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
