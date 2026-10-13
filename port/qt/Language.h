// The shell's language: core's texts (WinSCP's translations, through the
// bridge), Qt's own (standard dialogs and buttons) and the shell's (Qt
// Linguist), all for one locale. doc/qt/translations.md.
#ifndef LINSCP_QT_LANGUAGE_H
#define LINSCP_QT_LANGUAGE_H

#include <QString>

struct TLanguage
{
  // The locale the texts are in, as Qt names it ("pt_BR"): core's.
  QString Locale;
  // WinSCP's code for core's translation in use ("PT"); empty for English.
  QString CoreCode;
  bool Qt = false;
  bool Shell = false;
};

// `--lang=<locale>` from the command line, for this run only; empty without
// one (the configuration's language, else the system's: core decides, as
// upstream's GUI does).
QString RequestedLocale(int argc, char ** argv);
// Applies Requested to core, if any, then loads and installs the shell's and
// Qt's translations for the locale core's texts are in; call after
// CoreBridge::Initialize. The number formats follow the system's, unless a
// locale was asked for.
TLanguage SetUpLanguage(const QString & Requested);

#endif
