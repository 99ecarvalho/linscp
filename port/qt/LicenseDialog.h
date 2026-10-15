// A license's text, as WinSCP's License dialog (forms/License.cpp) shows
// it: linscp's own (About's License button), or that of a library it
// includes whose license it shows itself (Expat's, from About's
// third-party list).
#ifndef LINSCP_QT_LICENSE_DIALOG_H
#define LINSCP_QT_LICENSE_DIALOG_H

#include <QDialog>

class QPlainTextEdit;

class LicenseDialog : public QDialog
{
  Q_OBJECT

public:
  // As WinSCP's TLicense: the product's, or Expat's.
  enum TLicense { Product, Expat };

  explicit LicenseDialog(TLicense License, QWidget * Parent = nullptr);
  // Another's: an icon set's (About's list), Product named, from FileName.
  LicenseDialog(const QString & Product, const QString & FileName, QWidget * Parent = nullptr);

  // The text, for tests.
  QPlainTextEdit * Text() const { return FText; }

private:
  void Show(const QString & Text);

  QPlainTextEdit * FText = nullptr;
};

#endif
