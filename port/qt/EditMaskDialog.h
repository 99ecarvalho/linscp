// Edit file mask, as WinSCP's (forms/EditMask.cpp): a mask in its parts,
// files and directories to include and to exclude, one mask a line, and
// the mask they make, or core's error.
#ifndef LINSCP_QT_EDIT_MASK_DIALOG_H
#define LINSCP_QT_EDIT_MASK_DIALOG_H

#include <QDialog>

class QCheckBox;
class QPlainTextEdit;
class QPushButton;

class EditMaskDialog : public QDialog
{
  Q_OBJECT

public:
  explicit EditMaskDialog(const QString & Mask, QWidget * Parent = nullptr);

  // The mask the parts make (empty while they do not make one).
  QString Mask() const;

  QPlainTextEdit * IncludeFiles() const { return FIncludeFiles; }
  QPlainTextEdit * ExcludeFiles() const { return FExcludeFiles; }
  QPlainTextEdit * IncludeDirectories() const { return FIncludeDirectories; }
  QPlainTextEdit * ExcludeDirectories() const { return FExcludeDirectories; }
  QCheckBox * ExcludeAllDirectoriesCheck() const { return FExcludeAll; }
  QPlainTextEdit * Preview() const { return FPreview; }
  QPushButton * OkButton() const { return FOk; }
  void Clear();

  // The dialog for a mask field: Mask edited in place; false when
  // cancelled.
  static bool Edit(QString & Mask, QWidget * Parent);

private:
  void UpdateControls();

  QPlainTextEdit * FIncludeFiles = nullptr;
  QPlainTextEdit * FExcludeFiles = nullptr;
  QPlainTextEdit * FIncludeDirectories = nullptr;
  QPlainTextEdit * FExcludeDirectories = nullptr;
  QCheckBox * FExcludeAll = nullptr;
  QPlainTextEdit * FPreview = nullptr;
  QPushButton * FOk = nullptr;
  QString FMask;
  bool FValid = true;
  QString FExcludeDirectoriesBefore;
  bool FChanging = false;
};

#endif
