// The custom command dialog, as WinSCP's (forms/CustomCommand.cpp): a
// command's description, text with its patterns, local or remote, what it
// applies to and does with its output, and keyboard shortcut; ad hoc
// (Commands > Custom Commands > Enter) without the description and
// shortcut. And its parameters' prompts as the command runs
// (TCustomCommandPromptsDialog).
#ifndef LINSCP_QT_CUSTOM_COMMAND_DIALOG_H
#define LINSCP_QT_CUSTOM_COMMAND_DIALOG_H

#include <QDialog>

#include <vector>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

class CustomCommandDialog : public QDialog
{
  Q_OBJECT

public:
  enum class TMode { Add, Edit, AdHoc };
  // Commands: the list, for its names and duplicates; the shortcuts they
  // use are not offered.
  // OtherShortCuts: those others use (the shared bookmarks'), not offered
  // either.
  CustomCommandDialog(TMode Mode, const CoreBridge::TCustomCommandDefinition & Command,
    const std::vector<CoreBridge::TCustomCommandDefinition> & Commands, QWidget * Parent = nullptr,
    const std::vector<unsigned> & OtherShortCuts = {});

  CoreBridge::TCustomCommandDefinition Command() const;
  // OK's checks, as FormCloseQuery: false (the error shown) when the
  // command is not one.
  bool Validate();
  void accept() override;

  QLineEdit * DescriptionEdit() const { return FDescription; }
  QComboBox * CommandCombo() const { return FCommand; }
  QRadioButton * RemoteButton() const { return FRemote; }
  QCheckBox * ApplyToDirectoriesCheck() const { return FApplyToDirectories; }
  QCheckBox * RecursiveCheck() const { return FRecursive; }
  QCheckBox * ShowResultsCheck() const { return FShowResults; }
  QComboBox * ShortCutCombo() const { return FShortCut; }
  QPushButton * OkButton() const { return FOk; }

private:
  void UpdateControls();

  TMode FMode;
  CoreBridge::TCustomCommandDefinition FOriginal;
  std::vector<CoreBridge::TCustomCommandDefinition> FCommands;
  QLineEdit * FDescription = nullptr;
  QComboBox * FCommand = nullptr;
  QRadioButton * FLocal = nullptr;
  QRadioButton * FRemote = nullptr;
  QCheckBox * FApplyToDirectories = nullptr;
  QCheckBox * FRecursive = nullptr;
  QCheckBox * FShowResults = nullptr;
  QCheckBox * FCopyResults = nullptr;
  QCheckBox * FRemoteFiles = nullptr;
  QComboBox * FShortCut = nullptr;
  QPushButton * FOk = nullptr;
};

// TCustomCommandPromptsDialog: each prompt with its default; false when
// cancelled.
bool CustomCommandPrompts(QWidget * Parent, const std::string & Name,
  const std::vector<std::pair<std::string, std::string>> & Prompts, std::vector<std::string> & Values);

#endif
