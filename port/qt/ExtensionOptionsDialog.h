// An extension's options, as WinSCP's TCustomCommandOptionsDialog
// (forms/Custom.cpp): each "@option" its control (a label, a link, a group,
// a separator, a text or file box with its history, a list, a check box),
// and in Preferences its keyboard shortcut.
#ifndef LINSCP_QT_EXTENSION_OPTIONS_DIALOG_H
#define LINSCP_QT_EXTENSION_OPTIONS_DIALOG_H

#include <QDialog>

#include <string>
#include <vector>

#include "CoreBridge.h"

class QComboBox;

class ExtensionOptionsDialog : public QDialog
{
  Q_OBJECT

public:
  // Title: the extension's name. ShortCut: offered when given, the others'
  // (Used) left out. HelpPage: Help's, its options page or home page.
  ExtensionOptionsDialog(const QString & Title, const std::vector<CoreBridge::TExtensionOption> & Options,
    QWidget * Parent = nullptr, const unsigned * ShortCut = nullptr, const std::vector<unsigned> & Used = {},
    const QString & HelpPage = QString());

  // Each option's value, in the options' order (as they were for those
  // without a control), the text and file boxes' kept in their histories.
  std::vector<std::string> Values() const;
  unsigned ShortCut() const;
  // Each option's control (null for a group or separator), for tests.
  QWidget * Control(int Index) const { return FControls[Index]; }

  void accept() override;

private:
  std::vector<CoreBridge::TExtensionOption> FOptions;
  std::vector<QWidget *> FControls;
  QComboBox * FShortCut = nullptr;
};

#endif
