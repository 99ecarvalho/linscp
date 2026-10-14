// The local browser's Copy and Move (F5, F6), as WinSCP's (forms/CopyLocal.cpp,
// TCopyLocalDialog): the target path, a directory and a file mask in one
// ("/home/me/backup/*.*"), with Browse. Accepted, the directory exists
// (created, if asked) and a mask that is a name for several files has been
// confirmed. WinSCP's "Do not show this dialog box again" is not here, as
// linscp's transfers always ask.
#ifndef LINSCP_QT_COPY_LOCAL_DIALOG_H
#define LINSCP_QT_COPY_LOCAL_DIALOG_H

#include <QDialog>

class QLineEdit;

class CopyLocalDialog : public QDialog
{
  Q_OBJECT

public:
  // MultipleFiles: more than one file is copied (a mask must then be one).
  CopyLocalDialog(bool Move, bool MultipleFiles, const QString & Directory, const QString & FileMask,
    QWidget * Parent = nullptr);

  // With native separators and a trailing one.
  QString Directory() const;
  QString FileMask() const;
  void accept() override;

  QLineEdit * DirectoryEdit() const { return FDirectory; }

private:
  void SetDirectoryAndFileMask(const QString & Directory, const QString & FileMask);
  void ValidateDirectoryEdit();
  bool ValidateLocalDirectory();
  bool ValidateFileMask();
  void Failed();

  QLineEdit * FDirectory = nullptr;
  bool FMultipleFiles = false;
};

#endif
