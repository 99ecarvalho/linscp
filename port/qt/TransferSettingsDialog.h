// WinSCP's transfer settings: the frame its dialogs share
// (forms/CopyParams.cpp), the Transfer settings dialog of one transfer
// (forms/CopyParamCustom.cpp), and a preset's with its autoselection rule
// (forms/CopyParamPreset.cpp).
#ifndef LINSCP_QT_TRANSFER_SETTINGS_DIALOG_H
#define LINSCP_QT_TRANSFER_SETTINGS_DIALOG_H

#include <QDialog>
#include <QWidget>

#include "CoreBridge.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;

class TransferSettingsFrame : public QWidget
{
  Q_OBJECT

public:
  explicit TransferSettingsFrame(QWidget * Parent = nullptr);

  void SetSettings(const CoreBridge::TTransferSettings & Settings);
  // What the frame says, over what it was given (so what it does not
  // show stays).
  CoreBridge::TTransferSettings Settings() const;

  // For tests.
  QRadioButton * TextButton() const { return FText; }
  QRadioButton * BinaryButton() const { return FBinary; }
  QRadioButton * AutomaticButton() const { return FAutomatic; }
  QLineEdit * TextMaskEdit() const { return FTextMask; }
  QCheckBox * PreserveTimeCheck() const { return FPreserveTime; }
  QCheckBox * PreserveRightsCheck() const { return FPreserveRights; }
  QLineEdit * RightsEdit() const { return FRights; }
  QLineEdit * IncludeFileMaskEdit() const { return FIncludeFileMask; }
  QCheckBox * NewerOnlyCheck() const { return FNewerOnly; }
  QSpinBox * SpeedSpin() const { return FSpeed; }

private:
  void UpdateControls();

  CoreBridge::TTransferSettings FSettings;
  QRadioButton * FText = nullptr;
  QRadioButton * FBinary = nullptr;
  QRadioButton * FAutomatic = nullptr;
  QLineEdit * FTextMask = nullptr;
  QRadioButton * FNoChange = nullptr;
  QRadioButton * FUpperCase = nullptr;
  QRadioButton * FLowerCase = nullptr;
  QRadioButton * FLowerCaseShort = nullptr;
  QCheckBox * FReplaceInvalidChars = nullptr;
  QCheckBox * FPreserveRights = nullptr;
  QLineEdit * FRights = nullptr;
  QCheckBox * FAddXToDirectories = nullptr;
  QCheckBox * FIgnorePermErrors = nullptr;
  QCheckBox * FClearArchive = nullptr;
  QCheckBox * FRemoveBomAndEof = nullptr;
  QCheckBox * FEncryptNewFiles = nullptr;
  QCheckBox * FPreserveReadOnly = nullptr;
  QCheckBox * FPreserveTime = nullptr;
  QCheckBox * FPreserveTimeDirs = nullptr;
  QCheckBox * FCalculateSize = nullptr;
  QSpinBox * FSpeed = nullptr;
  QLineEdit * FIncludeFileMask = nullptr;
  QCheckBox * FNewerOnly = nullptr;
  QCheckBox * FExcludeHiddenFiles = nullptr;
  QCheckBox * FExcludeEmptyDirectories = nullptr;
};

// One transfer's settings, or a preset's (with its name).
class TransferSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  // With PresetName non-null, a preset's dialog, its name editable.
  TransferSettingsDialog(const CoreBridge::TTransferSettings & Settings, const QString * PresetName,
    QWidget * Parent = nullptr);

  CoreBridge::TTransferSettings Settings() const;
  QString PresetName() const;
  // A preset's autoselection rule: Current (when given) is what the
  // Current button fills in, the active session's.
  void SetRule(bool HasRule, const CoreBridge::TTransferPresetRule & Rule,
    const CoreBridge::TTransferPresetRule * Current);
  bool HasRule() const;
  CoreBridge::TTransferPresetRule Rule() const;
  TransferSettingsFrame * Frame() const { return FFrame; }
  QLineEdit * NameEdit() const { return FName; }
  QCheckBox * HasRuleCheck() const { return FHasRule; }
  QLineEdit * RuleEdit(int Index) const { return FRuleEdits[Index]; }
  class QPushButton * CurrentRuleButton() const { return FCurrentRule; }

  void accept() override;

private:
  void SetRuleEdits(const CoreBridge::TTransferPresetRule & Rule);

  TransferSettingsFrame * FFrame = nullptr;
  QLineEdit * FName = nullptr;
  QCheckBox * FHasRule = nullptr;
  class QGroupBox * FRuleGroup = nullptr;
  // Hostname, Username, Remote directory, Local directory.
  QLineEdit * FRuleEdits[4] = {};
  QPushButton * FCurrentRule = nullptr;
  CoreBridge::TTransferPresetRule FCurrentRuleData;
};

// The Transfer settings group of WinSCP's Copy and Synchronize dialogs:
// what is in use, summarised as WinSCP's (GetInfoStr), the preset to use
// (which becomes the current one, as WinSCP's preset menu has it), and the
// settings of this transfer alone.
class TransferSettingsBox : public QWidget
{
  Q_OBJECT

public:
  explicit TransferSettingsBox(QWidget * Parent = nullptr);
  CoreBridge::TTransferSettings Settings() const { return FSettings; }
  // Makes the preset chosen the current one, if it changed. Throws
  // TCoreError.
  void Commit();

  class QComboBox * PresetCombo() const { return FPresets; }
  class QLabel * InfoLabel() const { return FInfo; }
  class QPushButton * SettingsButton() const { return FButton; }

signals:
  void SettingsChanged();

private:
  void ShowInfo();

  CoreBridge::TTransferSettings FSettings;
  QComboBox * FPresets = nullptr;
  QLabel * FInfo = nullptr;
  QPushButton * FButton = nullptr;
};

#endif
