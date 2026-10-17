// Generate session URL/code and Generate file URL, as WinSCP's
// (forms/GenerateUrl.cpp): the session's URL with the parts asked for, a
// script (file, batch file, command line, PowerShell) for linscp's
// command-line client, or .NET assembly code; for files, each one's URL.
#ifndef LINSCP_QT_GENERATE_URL_DIALOG_H
#define LINSCP_QT_GENERATE_URL_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QTabWidget;
class QTextBrowser;

class GenerateUrlDialog : public QDialog
{
  Q_OBJECT

public:
  // Paths: files' full remote paths, for their URLs; empty for the
  // session's.
  // Transfer: the Copy dialog's transfer's code (no URL page).
  // Site: the Login dialog's, a site's code without a session (Session
  // null, no Paths).
  GenerateUrlDialog(CoreBridge::TSession * Session, const std::vector<std::string> & Paths, QWidget * Parent = nullptr,
    const CoreBridge::TSession::TTransferCode * Transfer = nullptr, const CoreBridge::TSite * Site = nullptr);

  // What it shows, as text (Copy to Clipboard's).
  QString Text() const;
  CoreBridge::TGenerateCodeOptions Options() const;

  QTabWidget * Pages() const { return FPages; }
  QCheckBox * Check(int Tag) const;
  QComboBox * ScriptFormatCombo() const { return FScriptFormat; }
  QComboBox * LanguageCombo() const { return FLanguage; }
  QTextBrowser * Result() const { return FResult; }

  // Closing keeps the choices, as TGenerateUrlDialog::Execute (not for
  // files: their page was not chosen).
  void done(int Result) override;

private:
  void Generate();

  CoreBridge::TSession * FSession;
  bool FHasSite = false;
  CoreBridge::TSite FSite;
  std::vector<std::string> FPaths;
  bool FHasTransfer = false;
  CoreBridge::TSession::TTransferCode FTransfer;
  QTabWidget * FPages = nullptr;
  std::vector<std::pair<int, QCheckBox *>> FChecks;
  QComboBox * FScriptFormat = nullptr;
  QComboBox * FLanguage = nullptr;
  QLabel * FScriptDescription = nullptr;
  QLabel * FAssemblyDescription = nullptr;
  QGroupBox * FResultGroup = nullptr;
  QTextBrowser * FResult = nullptr;
  QString FRtf;
  bool FChanging = false;
};

#endif
