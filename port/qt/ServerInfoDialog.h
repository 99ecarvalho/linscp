// Commands > Server/Protocol Information, as WinSCP's
// (forms/FileSystemInfo.cpp): the server and protocol, the host key's and
// certificate's fingerprints, the capabilities, the space available, and
// all of it to the clipboard.
#ifndef LINSCP_QT_SERVER_INFO_DIALOG_H
#define LINSCP_QT_SERVER_INFO_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QGroupBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTreeWidget;

class ServerInfoDialog : public QDialog
{
  Q_OBJECT

public:
  ServerInfoDialog(CoreBridge::TSession * Session, const CoreBridge::TServerInfo & Info, const QString & Path,
    QWidget * Parent = nullptr);

  // Check space: false when core failed (its error shown).
  bool CheckSpace();
  // Copy to Clipboard, as ClipboardButtonClick: the space checked first.
  QString ClipboardText();
  void CopyToClipboard();

  QTabWidget * Pages() const { return FPages; }
  QTreeWidget * ServerView() const { return FServer; }
  QTreeWidget * CapabilitiesView() const { return FCapabilities; }
  QTreeWidget * SpaceView() const { return FSpace; }
  QLineEdit * PathEdit() const { return FPath; }
  QLineEdit * HostKeySha256Edit() const { return FHostKeySha256; }

private:
  CoreBridge::TSession * FSession;
  CoreBridge::TServerInfo FInfo;
  bool FSpaceChecked = false;
  QTabWidget * FPages = nullptr;
  QTreeWidget * FServer = nullptr;
  QGroupBox * FHostKeyGroup = nullptr;
  QLineEdit * FHostKeySha256 = nullptr;
  QGroupBox * FCertificateGroup = nullptr;
  QTreeWidget * FCapabilities = nullptr;
  QGroupBox * FInfoGroup = nullptr;
  QTreeWidget * FSpace = nullptr;
  QLineEdit * FPath = nullptr;
  QPushButton * FCheckSpace = nullptr;
  int FSpacePage = -1;
};

#endif
