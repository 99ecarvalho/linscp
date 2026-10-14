// Edit Raw Settings, as WinSCP's (forms/Custom.cpp's TSiteRawDialog): a
// site's settings as core's "Name=Value" options, edited as text, with
// Add... for the options not there yet.
#ifndef LINSCP_QT_SITE_RAW_DIALOG_H
#define LINSCP_QT_SITE_RAW_DIALOG_H

#include <QDialog>

class QPlainTextEdit;

class SiteRawDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SiteRawDialog(const QString & Settings, QWidget * Parent = nullptr);

  QString Settings() const;
  QPlainTextEdit * Editor() const { return FSettings; }
  // Add...: the option, with its default selected for editing. False when
  // it is not one (or there).
  bool AddSetting(const QString & Name);

private:
  QPlainTextEdit * FSettings = nullptr;
};

#endif
