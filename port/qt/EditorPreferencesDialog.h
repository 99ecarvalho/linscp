// An editor added, edited, or chosen for one file (Edit With), as WinSCP's
// Editor preferences dialog (forms/EditorPreferences.cpp): the internal
// editor, an external one, or the file's associated application; the files
// it is for; how an external one is given remote files.
#ifndef LINSCP_QT_EDITOR_PREFERENCES_DIALOG_H
#define LINSCP_QT_EDITOR_PREFERENCES_DIALOG_H

#include <QDialog>

#include "CoreBridge.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

class EditorPreferencesDialog : public QDialog
{
  Q_OBJECT

public:
  // As TEditorPreferencesMode. AdHoc: an external editor for one file,
  // with "remember this editor". MayRemote: the external editor's options
  // apply (a remote file may be edited).
  enum class TMode { Add, Edit, AdHoc };
  EditorPreferencesDialog(TMode Mode, const CoreBridge::TEditorDefinition & Editor, bool MayRemote,
    QWidget * Parent = nullptr);

  CoreBridge::TEditorDefinition Editor() const;
  bool Remember() const;
  void accept() override;

  QRadioButton * InternalButton() const { return FInternal; }
  QRadioButton * ExternalButton() const { return FExternal; }
  QRadioButton * OpenButton() const { return FOpen; }
  QLineEdit * CommandEdit() const { return FCommand; }
  QLineEdit * MaskEdit() const { return FMask; }
  QCheckBox * TextCheck() const { return FText; }
  QCheckBox * SdiCheck() const { return FSdi; }
  QCheckBox * RememberCheck() const { return FRemember; }
  QPushButton * OkButton() const { return FOk; }

private:
  void UpdateControls();
  // As ExternalEditorOptionsAutodetect: the options a known editor needs,
  // when the command changes to or from one.
  void Autodetect();

  bool FMayRemote;
  CoreBridge::TEditorDefinition FInitial;
  CoreBridge::TEditorDefinition FDefaults;
  QRadioButton * FInternal = nullptr;
  QRadioButton * FExternal = nullptr;
  QRadioButton * FOpen = nullptr;
  QLineEdit * FCommand = nullptr;
  QPushButton * FBrowse = nullptr;
  QGroupBox * FMaskGroup = nullptr;
  QLineEdit * FMask = nullptr;
  QGroupBox * FExternalGroup = nullptr;
  QCheckBox * FText = nullptr;
  QCheckBox * FSdi = nullptr;
  QCheckBox * FRemember = nullptr;
  QPushButton * FOk = nullptr;
};

#endif
