// An S3 tag added or edited, as WinSCP's Tag dialog (forms/Custom.cpp,
// TTagDialog), from the Properties dialog's Tags page: its key, unique
// among the file's, and its value, both checked for characters S3 does not
// allow.
#ifndef LINSCP_QT_TAG_DIALOG_H
#define LINSCP_QT_TAG_DIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QPushButton;

class TagDialog : public QDialog
{
  Q_OBJECT

public:
  // Keys: the file's other tags' keys.
  TagDialog(bool Add, const QStringList & Keys, const QString & Key, const QString & Value, QWidget * Parent = nullptr);

  QString Key() const;
  QString Value() const;

  QLineEdit * KeyEdit() const { return FKey; }
  QLineEdit * ValueEdit() const { return FValue; }
  QPushButton * OkButton() const { return FOk; }

  void accept() override;

private:
  // As TTagDialog::ValidateTag: false when the user chose not to go on.
  bool ValidateTag(QLineEdit * Edit);

  QStringList FKeys;
  QLineEdit * FKey = nullptr;
  QLineEdit * FValue = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
