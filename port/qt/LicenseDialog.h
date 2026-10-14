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

  // The text, for tests.
  QPlainTextEdit * Text() const { return FText; }

private:
  QPlainTextEdit * FText = nullptr;
};

#endif
