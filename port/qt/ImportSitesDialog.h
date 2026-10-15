// Tools > Import Sites in the site manager, as WinSCP's
// (forms/ImportSessions.cpp): another program's sites (PuTTY, KiTTY,
// FileZilla, OpenSSH), an INI file's, or known_hosts' host keys, checked
// and imported.
#ifndef LINSCP_QT_IMPORT_SITES_DIALOG_H
#define LINSCP_QT_IMPORT_SITES_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QSet>

#include <functional>
#include <memory>

#include "CoreBridge.h"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class ImportSitesDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ImportSitesDialog(QWidget * Parent = nullptr);

  // The sites imported by OK, under their names.
  const std::vector<std::string> & Imported() const { return FImported; }
  void accept() override;

  // Browse... and Paste, given what they would read, for tests and them
  // alike.
  void LoadIni(const QString & FileName);
  void LoadKnownHosts(const QString & Text);
  QComboBox * SourceCombo() const { return FSource; }
  QListWidget * List() const { return FList; }
  QLabel * ErrorLabel() const { return FError; }
  QPushButton * OkButton() const { return FOk; }

  // Where a converted key goes (tests); the file dialog by default.
  void SetKeyFileChooser(std::function<QString(const QString & Suggested)> Chooser) { FKeyFileChooser = std::move(Chooser); }

private:
  std::function<QString(const QString & Suggested)> FKeyFileChooser;
  void LoadSites();
  void SaveSelection();
  bool ConvertKeyFile(QString & KeyFile, QMap<QString, QString> & Converted, QSet<QString> & NotConverted);
  void ConvertKeys();
  void UpdateControls();

  std::unique_ptr<CoreBridge::TImportSites> FSites;
  QComboBox * FSource = nullptr;
  QListWidget * FList = nullptr;
  QLabel * FError = nullptr;
  QPushButton * FCheckAll = nullptr;
  QPushButton * FBrowse = nullptr;
  QPushButton * FPaste = nullptr;
  QPushButton * FOk = nullptr;
  int FShown = -1;
  std::vector<std::string> FImported;
};

#endif
