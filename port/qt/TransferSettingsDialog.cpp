#include "TransferSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

using TTransferSettings = CoreBridge::TTransferSettings;

TransferSettingsFrame::TransferSettingsFrame(QWidget * Parent) :
  QWidget(Parent)
{
  QGridLayout * Layout = new QGridLayout(this);

  QGroupBox * Mode = new QGroupBox(tr("Transfer mode"), this);
  QVBoxLayout * ModeLayout = new QVBoxLayout(Mode);
  FText = new QRadioButton(tr("&Text (plain text, html, scripts, ...)"), Mode);
  FBinary = new QRadioButton(tr("&Binary (archives, doc, ...)"), Mode);
  FAutomatic = new QRadioButton(tr("&Automatic"), Mode);
  FTextMask = new QLineEdit(Mode);
  for (QRadioButton * Button : { FText, FBinary, FAutomatic })
  {
    ModeLayout->addWidget(Button);
    connect(Button, &QRadioButton::toggled, this, &TransferSettingsFrame::UpdateControls);
  }
  ModeLayout->addWidget(new QLabel(tr("Transfer following &files in text mode:"), Mode));
  ModeLayout->addWidget(FTextMask);
  Layout->addWidget(Mode, 0, 0);

  QGroupBox * Case = new QGroupBox(tr("Filename modification"), this);
  QVBoxLayout * CaseLayout = new QVBoxLayout(Case);
  FNoChange = new QRadioButton(tr("No chan&ge"), Case);
  FUpperCase = new QRadioButton(tr("&Upper case"), Case);
  FLowerCase = new QRadioButton(tr("Lo&wer case"), Case);
  FLowerCaseShort = new QRadioButton(tr("Lower case &8.3"), Case);
  FReplaceInvalidChars = new QCheckBox(tr("Rep&lace '\\:*?'..."), Case);
  for (QWidget * Widget : std::initializer_list<QWidget *>{ FNoChange, FUpperCase, FLowerCase, FLowerCaseShort, FReplaceInvalidChars })
  {
    CaseLayout->addWidget(Widget);
  }
  Layout->addWidget(Case, 0, 1);

  QGroupBox * Upload = new QGroupBox(tr("Upload options"), this);
  QVBoxLayout * UploadLayout = new QVBoxLayout(Upload);
  QHBoxLayout * RightsRow = new QHBoxLayout();
  FPreserveRights = new QCheckBox(tr("Set pe&rmissions:"), Upload);
  connect(FPreserveRights, &QCheckBox::toggled, this, &TransferSettingsFrame::UpdateControls);
  FRights = new QLineEdit(Upload);
  // Octal, as WinSCP's rights edit takes them.
  FRights->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-7]{3,4}")), FRights));
  RightsRow->addWidget(FPreserveRights);
  RightsRow->addWidget(FRights);
  UploadLayout->addLayout(RightsRow);
  FAddXToDirectories = new QCheckBox(tr("Add &X to directories"), Upload);
  FIgnorePermErrors = new QCheckBox(tr("Ign&ore permission errors"), Upload);
  FClearArchive = new QCheckBox(tr("Clear 'Arc&hive' attribute"), Upload);
  FRemoveBomAndEof = new QCheckBox(tr("Remo&ve BOM and EOF marks"), Upload);
  FEncryptNewFiles = new QCheckBox(tr("&Encrypt new files"), Upload);
  for (QWidget * Widget : std::initializer_list<QWidget *>{ FAddXToDirectories, FIgnorePermErrors, FClearArchive, FRemoveBomAndEof, FEncryptNewFiles })
  {
    UploadLayout->addWidget(Widget);
  }
  Layout->addWidget(Upload, 1, 0);

  QVBoxLayout * Right = new QVBoxLayout();
  QGroupBox * Download = new QGroupBox(tr("Download options"), this);
  QVBoxLayout * DownloadLayout = new QVBoxLayout(Download);
  FPreserveReadOnly = new QCheckBox(tr("Preserve rea&d-only"), Download);
  DownloadLayout->addWidget(FPreserveReadOnly);
  Right->addWidget(Download);
  QGroupBox * Common = new QGroupBox(tr("Common options"), this);
  QVBoxLayout * CommonLayout = new QVBoxLayout(Common);
  FPreserveTime = new QCheckBox(tr("&Preserve timestamp"), Common);
  connect(FPreserveTime, &QCheckBox::toggled, this, &TransferSettingsFrame::UpdateControls);
  FPreserveTimeDirs = new QCheckBox(tr("Including directories"), Common);
  FCalculateSize = new QCheckBox(tr("&Calculate total size"), Common);
  QHBoxLayout * SpeedRow = new QHBoxLayout();
  FSpeed = new QSpinBox(Common);
  FSpeed->setRange(0, 1024 * 1024);
  FSpeed->setSpecialValueText(tr("Unlimited"));
  QLabel * SpeedLabel = new QLabel(tr("&Speed (KB/s):"), Common);
  SpeedLabel->setBuddy(FSpeed);
  SpeedRow->addWidget(SpeedLabel);
  SpeedRow->addWidget(FSpeed);
  for (QCheckBox * Check : { FPreserveTime, FPreserveTimeDirs, FCalculateSize })
  {
    CommonLayout->addWidget(Check);
  }
  CommonLayout->addLayout(SpeedRow);
  Right->addWidget(Common);
  Layout->addLayout(Right, 1, 1);

  QGroupBox * Other = new QGroupBox(tr("Other"), this);
  QFormLayout * OtherLayout = new QFormLayout(Other);
  FIncludeFileMask = new QLineEdit(Other);
  OtherLayout->addRow(tr("File &mask:"), FIncludeFileMask);
  FNewerOnly = new QCheckBox(tr("&New and updated files only"), Other);
  FExcludeHiddenFiles = new QCheckBox(tr("Exclude h&idden files"), Other);
  FExcludeEmptyDirectories = new QCheckBox(tr("E&xclude empty directories"), Other);
  OtherLayout->addRow(FNewerOnly);
  OtherLayout->addRow(FExcludeHiddenFiles);
  OtherLayout->addRow(FExcludeEmptyDirectories);
  Layout->addWidget(Other, 2, 0, 1, 2);

  SetSettings(TTransferSettings());
}

void TransferSettingsFrame::SetSettings(const TTransferSettings & Settings)
{
  FSettings = Settings;
  FText->setChecked(Settings.Mode == TTransferSettings::TMode::Text);
  FBinary->setChecked(Settings.Mode == TTransferSettings::TMode::Binary);
  FAutomatic->setChecked(Settings.Mode == TTransferSettings::TMode::Automatic);
  FTextMask->setText(QString::fromStdString(Settings.TextMask));
  FNoChange->setChecked((Settings.NameCase == TTransferSettings::TNameCase::NoChange) ||
    (Settings.NameCase == TTransferSettings::TNameCase::FirstUpper));
  FUpperCase->setChecked(Settings.NameCase == TTransferSettings::TNameCase::Upper);
  FLowerCase->setChecked(Settings.NameCase == TTransferSettings::TNameCase::Lower);
  FLowerCaseShort->setChecked(Settings.NameCase == TTransferSettings::TNameCase::LowerShort);
  FReplaceInvalidChars->setChecked(Settings.ReplaceInvalidChars);
  FPreserveRights->setChecked(Settings.PreserveRights);
  FRights->setText(QString::fromStdString(Settings.Rights));
  FAddXToDirectories->setChecked(Settings.AddXToDirectories);
  FIgnorePermErrors->setChecked(Settings.IgnorePermErrors);
  FClearArchive->setChecked(Settings.ClearArchive);
  FRemoveBomAndEof->setChecked(Settings.RemoveBomAndEof);
  FEncryptNewFiles->setChecked(Settings.EncryptNewFiles);
  FPreserveReadOnly->setChecked(Settings.PreserveReadOnly);
  FPreserveTime->setChecked(Settings.PreserveTime);
  FPreserveTimeDirs->setChecked(Settings.PreserveTimeDirs);
  FCalculateSize->setChecked(Settings.CalculateSize);
  FSpeed->setValue(static_cast<int>(Settings.SpeedLimit / 1024));
  FIncludeFileMask->setText(QString::fromStdString(Settings.IncludeFileMask));
  FNewerOnly->setChecked(Settings.NewerOnly);
  FExcludeHiddenFiles->setChecked(Settings.ExcludeHiddenFiles);
  FExcludeEmptyDirectories->setChecked(Settings.ExcludeEmptyDirectories);
  UpdateControls();
}

TTransferSettings TransferSettingsFrame::Settings() const
{
  TTransferSettings Result = FSettings;
  Result.Mode = FText->isChecked() ? TTransferSettings::TMode::Text :
    (FAutomatic->isChecked() ? TTransferSettings::TMode::Automatic : TTransferSettings::TMode::Binary);
  Result.TextMask = FTextMask->text().toStdString();
  if (FUpperCase->isChecked())
  {
    Result.NameCase = TTransferSettings::TNameCase::Upper;
  }
  else if (FLowerCase->isChecked())
  {
    Result.NameCase = TTransferSettings::TNameCase::Lower;
  }
  else if (FLowerCaseShort->isChecked())
  {
    Result.NameCase = TTransferSettings::TNameCase::LowerShort;
  }
  else if (Result.NameCase != TTransferSettings::TNameCase::FirstUpper)
  {
    Result.NameCase = TTransferSettings::TNameCase::NoChange;
  }
  Result.ReplaceInvalidChars = FReplaceInvalidChars->isChecked();
  Result.PreserveRights = FPreserveRights->isChecked();
  Result.Rights = FRights->text().toStdString();
  Result.AddXToDirectories = FAddXToDirectories->isChecked();
  Result.IgnorePermErrors = FIgnorePermErrors->isChecked();
  Result.ClearArchive = FClearArchive->isChecked();
  Result.RemoveBomAndEof = FRemoveBomAndEof->isChecked();
  Result.EncryptNewFiles = FEncryptNewFiles->isChecked();
  Result.PreserveReadOnly = FPreserveReadOnly->isChecked();
  Result.PreserveTime = FPreserveTime->isChecked();
  Result.PreserveTimeDirs = FPreserveTimeDirs->isChecked();
  Result.CalculateSize = FCalculateSize->isChecked();
  Result.SpeedLimit = static_cast<unsigned long>(FSpeed->value()) * 1024;
  Result.IncludeFileMask = FIncludeFileMask->text().toStdString();
  Result.NewerOnly = FNewerOnly->isChecked();
  Result.ExcludeHiddenFiles = FExcludeHiddenFiles->isChecked();
  Result.ExcludeEmptyDirectories = FExcludeEmptyDirectories->isChecked();
  return Result;
}

// As TCopyParamsFrame::UpdateControls, for what the frame has.
void TransferSettingsFrame::UpdateControls()
{
  FTextMask->setEnabled(FAutomatic->isChecked());
  FRights->setEnabled(FPreserveRights->isChecked());
  FAddXToDirectories->setEnabled(FPreserveRights->isChecked());
  FPreserveTimeDirs->setEnabled(FPreserveTime->isChecked());
}

TransferSettingsDialog::TransferSettingsDialog(const TTransferSettings & Settings, const QString * PresetName,
  QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle((PresetName != nullptr) ? tr("Transfer settings preset") : tr("Transfer settings"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  if (PresetName != nullptr)
  {
    QFormLayout * NameRow = new QFormLayout();
    FName = new QLineEdit(*PresetName, this);
    NameRow->addRow(tr("Preset &description:"), FName);
    Layout->addLayout(NameRow);
  }
  FFrame = new TransferSettingsFrame(this);
  FFrame->SetSettings(Settings);
  Layout->addWidget(FFrame);
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Box, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Box);
}

TTransferSettings TransferSettingsDialog::Settings() const
{
  return FFrame->Settings();
}

QString TransferSettingsDialog::PresetName() const
{
  return (FName != nullptr) ? FName->text().trimmed() : QString();
}

TransferSettingsBox::TransferSettingsBox(QWidget * Parent) :
  QWidget(Parent)
{
  QGroupBox * Group = new QGroupBox(tr("Transfer settings"), this);
  QVBoxLayout * Outer = new QVBoxLayout(this);
  Outer->setContentsMargins(0, 0, 0, 0);
  Outer->addWidget(Group);
  QVBoxLayout * Layout = new QVBoxLayout(Group);
  FInfo = new QLabel(Group);
  FInfo->setWordWrap(true);
  Layout->addWidget(FInfo);
  QHBoxLayout * Row = new QHBoxLayout();
  FPresets = new QComboBox(Group);
  FPresets->addItem(tr("Default transfer settings"), QString());
  for (const CoreBridge::TTransferPreset & Preset : CoreBridge::TransferPresets())
  {
    FPresets->addItem(QString::fromStdString(Preset.Name), QString::fromStdString(Preset.Name));
  }
  const int Current = FPresets->findData(QString::fromStdString(CoreBridge::CurrentTransferPreset()));
  FPresets->setCurrentIndex((Current >= 0) ? Current : 0);
  FSettings = CoreBridge::CurrentTransferSettings();
  connect(FPresets, &QComboBox::currentIndexChanged, this, [this]()
  {
    const std::string Name = FPresets->currentData().toString().toStdString();
    FSettings = CoreBridge::DefaultTransferSettings();
    for (const CoreBridge::TTransferPreset & Preset : CoreBridge::TransferPresets())
    {
      if (Preset.Name == Name)
      {
        FSettings = Preset.Settings;
      }
    }
    ShowInfo();
    emit SettingsChanged();
  });
  FButton = new QPushButton(tr("Transfer settin&gs..."), Group);
  connect(FButton, &QPushButton::clicked, this, [this]()
  {
    TransferSettingsDialog Dialog(FSettings, nullptr, this);
    if (Dialog.exec() == QDialog::Accepted)
    {
      FSettings = Dialog.Settings();
      ShowInfo();
      emit SettingsChanged();
    }
  });
  Row->addWidget(FPresets, 1);
  Row->addWidget(FButton);
  Layout->addLayout(Row);
  ShowInfo();
}

void TransferSettingsBox::ShowInfo()
{
  const QString Info = QString::fromStdString(CoreBridge::TransferSettingsInfo(FSettings));
  FInfo->setText(Info.isEmpty() ? tr("Default transfer settings") : Info);
}

void TransferSettingsBox::Commit()
{
  const std::string Name = FPresets->currentData().toString().toStdString();
  if (Name != CoreBridge::CurrentTransferPreset())
  {
    CoreBridge::SetCurrentTransferPreset(Name);
  }
}
