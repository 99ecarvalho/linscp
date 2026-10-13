// The shell's language: core's texts (WinSCP's translations, through the
// bridge), Qt's own (standard dialogs and buttons) and the shell's (Qt
// Linguist), all for one locale. doc/qt/translations.md.
#ifndef LINSCP_QT_LANGUAGE_H
#define LINSCP_QT_LANGUAGE_H

#include <QString>

struct TLanguage
{
  // The locale asked for, as Qt names it ("pt_BR").
  QString Locale;
  // WinSCP's code for core's translation in use ("PT"); empty for English.
  QString CoreCode;
  bool Qt = false;
  bool Shell = false;
};

// The locale to use: `--lang=<locale>` on the command line, else the system's.
QString RequestedLocale(int argc, char ** argv);
// Loads and installs the three translations for Locale; call after the
// QApplication exists and before CoreBridge::Initialize.
TLanguage SetUpLanguage(const QString & Locale);

#endif
