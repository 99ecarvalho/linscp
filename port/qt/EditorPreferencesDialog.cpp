#include "EditorPreferencesDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

using TKind = CoreBridge::TEditorDefinition::TKind;

EditorPreferencesDialog::EditorPreferencesDialog(TMode Mode, const CoreBridge::TEditorDefinition & Editor,
  bool MayRemote, QWidget * Parent) :
  QDialog(Parent),
  FMayRemote(MayRemote),
  FInitial(Editor)
{
  setWindowTitle(QString::fromStdString(CoreBridge::Text((Mode == TMode::Edit) ? CoreBridge::TText::EditorEdit :
    ((Mode == TMode::Add) ? CoreBridge::TText::EditorAdd : CoreBridge::TText::EditorAdHoc))));
  QVBoxLayout * Layout = new QVBoxLayout(this);

  QGroupBox * EditorGroup = new QGroupBox(tr("Editor"), this);
  QGridLayout * EditorLayout = new QGridLayout(EditorGroup);
  FInternal = new QRadioButton(tr("&Internal editor"), EditorGroup);
  FExternal = new QRadioButton(tr("&External editor:"), EditorGroup);
  FCommand = new QLineEdit(QString::fromStdString(Editor.Command), EditorGroup);
  FCommand->setMinimumWidth(320);
  FBrowse = new QPushButton(tr("B&rowse..."), EditorGroup);
  FOpen = new QRadioButton(tr("&Associated application"), EditorGroup);
  if (Mode == TMode::AdHoc)
  {
    // As Init's epmAdHoc: the external editor only.
    QLabel * Label = new QLabel(tr("&External editor:"), EditorGroup);
    Label->setBuddy(FCommand);
    EditorLayout->addWidget(Label, 0, 0, 1, 2);
    FInternal->hide();
    FExternal->hide();
    FOpen->hide();
  }
  else
  {
    EditorLayout->addWidget(FInternal, 0, 0, 1, 2);
    EditorLayout->addWidget(FExternal, 1, 0, 1, 2);
  }
  EditorLayout->addWidget(FCommand, 2, 0);
  EditorLayout->addWidget(FBrowse, 2, 1);
  if (Mode != TMode::AdHoc)
  {
    EditorLayout->addWidget(FOpen, 3, 0, 1, 2);
  }
  Layout->addWidget(EditorGroup);

  FMaskGroup = new QGroupBox(tr("Editor autoselection"), this);
  QVBoxLayout * MaskLayout = new QVBoxLayout(FMaskGroup);
  FMask = new QLineEdit(QString::fromStdString(Editor.Mask), FMaskGroup);
  QLabel * MaskLabel = new QLabel(tr("Use this editor for &following files:"), FMaskGroup);
  MaskLabel->setBuddy(FMask);
  MaskLayout->addWidget(MaskLabel);
  MaskLayout->addWidget(FMask);
  FMaskGroup->setVisible(Mode != TMode::AdHoc);
  Layout->addWidget(FMaskGroup);

  FExternalGroup = new QGroupBox(tr("External editor options (affects editing remote files only)"), this);
  QVBoxLayout * ExternalLayout = new QVBoxLayout(FExternalGroup);
  FText = new QCheckBox(tr("Force &text transfer mode for files edited in external editor"), FExternalGroup);
  FSdi = new QCheckBox(tr("E&xternal editor opens each file in separate window (process)"), FExternalGroup);
  ExternalLayout->addWidget(FText);
  ExternalLayout->addWidget(FSdi);
  Layout->addWidget(FExternalGroup);

  FRemember = new QCheckBox(tr("&Remember this editor"), this);
  FRemember->setVisible(Mode == TMode::AdHoc);
  Layout->addWidget(FRemember);

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &EditorPreferencesDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);

  // As Execute.
  FInternal->setChecked(Editor.Kind == TKind::Internal);
  FExternal->setChecked((Editor.Kind == TKind::External) || (Mode == TMode::AdHoc));
  FOpen->setChecked(Editor.Kind == TKind::Open);
  FText->setChecked(Editor.Text);
  FSdi->setChecked(Editor.Sdi);
  CoreBridge::TEditorDefinition Defaults;
  Defaults.Command = Editor.Command;
  CoreBridge::AutodetectEditorOptions(Defaults);
  FDefaults = Defaults;

  connect(FBrowse, &QPushButton::clicked, this, [this]()
  {
    const QString Program =
      QFileDialog::getOpenFileName(this, QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SelectExternalEditor)));
    if (!Program.isEmpty())
    {
      // As FormatCommand: the program quoted.
      FCommand->setText(QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(Program)));
      Autodetect();
    }
  });
  for (QRadioButton * Button : { FInternal, FExternal, FOpen })
  {
    connect(Button, &QRadioButton::toggled, this, &EditorPreferencesDialog::UpdateControls);
  }
  connect(FCommand, &QLineEdit::textChanged, this, &EditorPreferencesDialog::UpdateControls);
  connect(FCommand, &QLineEdit::editingFinished, this, &EditorPreferencesDialog::Autodetect);
  UpdateControls();
}

void EditorPreferencesDialog::Autodetect()
{
  if (!FExternal->isChecked())
  {
    return;
  }
  CoreBridge::TEditorDefinition Defaults;
  Defaults.Command = FCommand->text().toStdString();
  CoreBridge::AutodetectEditorOptions(Defaults);
  if ((Defaults.Text != FDefaults.Text) || (Defaults.Sdi != FDefaults.Sdi))
  {
    FText->setChecked(Defaults.Text);
    FSdi->setChecked(Defaults.Sdi);
  }
  FDefaults = Defaults;
}

void EditorPreferencesDialog::UpdateControls()
{
  FOk->setEnabled(FInternal->isChecked() || FOpen->isChecked() || !FCommand->text().isEmpty());
  FCommand->setEnabled(FExternal->isChecked());
  FBrowse->setEnabled(FExternal->isChecked());
  FExternalGroup->setEnabled(FExternal->isChecked() && FMayRemote);
}

CoreBridge::TEditorDefinition EditorPreferencesDialog::Editor() const
{
  CoreBridge::TEditorDefinition Result = FInitial;
  Result.Kind = FExternal->isChecked() ? TKind::External : (FOpen->isChecked() ? TKind::Open : TKind::Internal);
  Result.Command = FCommand->text().toStdString();
  Result.Mask = FMask->text().toStdString();
  Result.Text = FText->isChecked();
  Result.Sdi = FSdi->isChecked();
  return Result;
}

bool EditorPreferencesDialog::Remember() const
{
  return FRemember->isChecked();
}

// As MaskEditExit's ValidateMaskEdit: a mask that is not one is core's error.
void EditorPreferencesDialog::accept()
{
  if (FMaskGroup->isVisible())
  {
    try
    {
      CoreBridge::SplitMask(FMask->text().toStdString());
    }
    catch (const CoreBridge::TCoreError & E)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
      FMask->setFocus();
      return;
    }
  }
  QDialog::accept();
}
