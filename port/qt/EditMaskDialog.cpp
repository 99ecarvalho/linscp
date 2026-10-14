#include "EditMaskDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"
#include "QtFrontend.h"

namespace
{

std::vector<std::string> LinesOf(const QPlainTextEdit * Edit)
{
  std::vector<std::string> Result;
  for (const QString & Line : Edit->toPlainText().split(QLatin1Char('\n')))
  {
    if (!Line.trimmed().isEmpty())
    {
      Result.push_back(Line.trimmed().toStdString());
    }
  }
  return Result;
}

void SetLines(QPlainTextEdit * Edit, const std::vector<std::string> & Lines)
{
  QStringList Text;
  for (const std::string & Line : Lines)
  {
    Text.append(QString::fromStdString(Line));
  }
  Edit->setPlainText(Text.join(QLatin1Char('\n')));
}

QPlainTextEdit * MaskEdit(QWidget * Parent)
{
  QPlainTextEdit * Edit = new QPlainTextEdit(Parent);
  Edit->setTabChangesFocus(true);
  Edit->setMinimumHeight(Edit->fontMetrics().lineSpacing() * 5);
  return Edit;
}

}  // namespace

EditMaskDialog::EditMaskDialog(const QString & Mask, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(tr("Edit file mask"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QGroupBox * Files = new QGroupBox(tr("Files masks"), this);
  QGridLayout * FilesLayout = new QGridLayout(Files);
  FIncludeFiles = MaskEdit(Files);
  FExcludeFiles = MaskEdit(Files);
  QLabel * IncludeFilesLabel = new QLabel(tr("&Include files:"), Files);
  QLabel * ExcludeFilesLabel = new QLabel(tr("&Exclude files:"), Files);
  IncludeFilesLabel->setBuddy(FIncludeFiles);
  ExcludeFilesLabel->setBuddy(FExcludeFiles);
  FilesLayout->addWidget(IncludeFilesLabel, 0, 0);
  FilesLayout->addWidget(ExcludeFilesLabel, 0, 1);
  FilesLayout->addWidget(FIncludeFiles, 1, 0);
  FilesLayout->addWidget(FExcludeFiles, 1, 1);
  Layout->addWidget(Files);
  QGroupBox * Directories = new QGroupBox(tr("Directories masks"), this);
  QGridLayout * DirectoriesLayout = new QGridLayout(Directories);
  FIncludeDirectories = MaskEdit(Directories);
  FExcludeDirectories = MaskEdit(Directories);
  QLabel * IncludeDirectoriesLabel = new QLabel(tr("I&nclude directories:"), Directories);
  QLabel * ExcludeDirectoriesLabel = new QLabel(tr("E&xclude directories:"), Directories);
  IncludeDirectoriesLabel->setBuddy(FIncludeDirectories);
  ExcludeDirectoriesLabel->setBuddy(FExcludeDirectories);
  FExcludeAll = new QCheckBox(tr("&All (do not recurse)"), Directories);
  DirectoriesLayout->addWidget(IncludeDirectoriesLabel, 0, 0);
  DirectoriesLayout->addWidget(ExcludeDirectoriesLabel, 0, 1);
  DirectoriesLayout->addWidget(FIncludeDirectories, 1, 0);
  DirectoriesLayout->addWidget(FExcludeDirectories, 1, 1);
  DirectoriesLayout->addWidget(FExcludeAll, 2, 1);
  Layout->addWidget(Directories);
  QGroupBox * MaskGroup = new QGroupBox(tr("Mask"), this);
  QVBoxLayout * MaskLayout = new QVBoxLayout(MaskGroup);
  FPreview = new QPlainTextEdit(MaskGroup);
  FPreview->setReadOnly(true);
  FPreview->setMaximumHeight(FPreview->fontMetrics().lineSpacing() * 4);
  MaskLayout->addWidget(FPreview);
  Layout->addWidget(MaskGroup);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons->addButton(tr("&Clear"), QDialogButtonBox::ResetRole), &QPushButton::clicked, this, &EditMaskDialog::Clear);
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);

  try
  {
    const CoreBridge::TMaskParts Parts = CoreBridge::SplitMask(Mask.toStdString());
    SetLines(FIncludeFiles, Parts.IncludeFiles);
    SetLines(FExcludeFiles, Parts.ExcludeFiles);
    SetLines(FIncludeDirectories, Parts.IncludeDirectories);
    SetLines(FExcludeDirectories, Parts.ExcludeDirectories);
  }
  catch (const CoreBridge::TCoreError &)
  {
    // Not a mask: the whole of it to include, so nothing is lost.
    FIncludeFiles->setPlainText(Mask);
  }
  for (QPlainTextEdit * Edit : { FIncludeFiles, FExcludeFiles, FIncludeDirectories })
  {
    connect(Edit, &QPlainTextEdit::textChanged, this, &EditMaskDialog::UpdateControls);
  }
  // As ExcludeDirectoryMasksMemoChange and ExcludeDirectoryAllCheckClick.
  connect(FExcludeDirectories, &QPlainTextEdit::textChanged, this, [this]()
  {
    if (!FChanging)
    {
      FChanging = true;
      const QString Text = FExcludeDirectories->toPlainText().trimmed();
      FExcludeAll->setChecked((Text == QStringLiteral("*")) || (Text == QStringLiteral("*/")));
      FChanging = false;
    }
    UpdateControls();
  });
  connect(FExcludeAll, &QCheckBox::toggled, this, [this](bool All)
  {
    if (!FChanging)
    {
      FChanging = true;
      if (All)
      {
        FExcludeDirectoriesBefore = FExcludeDirectories->toPlainText();
        FExcludeDirectories->setPlainText(QStringLiteral("*"));
      }
      else
      {
        FExcludeDirectories->setPlainText(FExcludeDirectoriesBefore);
      }
      FChanging = false;
    }
    UpdateControls();
  });
  {
    const QString Text = FExcludeDirectories->toPlainText().trimmed();
    FExcludeAll->setChecked((Text == QStringLiteral("*")) || (Text == QStringLiteral("*/")));
  }
  UpdateControls();
}

// As TEditMaskDialog::UpdateControls: the mask, or why it is not one.
void EditMaskDialog::UpdateControls()
{
  CoreBridge::TMaskParts Parts;
  Parts.IncludeFiles = LinesOf(FIncludeFiles);
  Parts.ExcludeFiles = LinesOf(FExcludeFiles);
  Parts.IncludeDirectories = LinesOf(FIncludeDirectories);
  Parts.ExcludeDirectories = LinesOf(FExcludeDirectories);
  try
  {
    FMask = QString::fromStdString(CoreBridge::ComposeMask(Parts));
    FValid = true;
    FPreview->setPlainText(FMask);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    FMask.clear();
    FValid = false;
    FPreview->setPlainText(QString::fromStdString(E.Error.Message));
  }
  FOk->setEnabled(FValid);
}

QString EditMaskDialog::Mask() const
{
  return FMask;
}

void EditMaskDialog::Clear()
{
  for (QPlainTextEdit * Edit : { FIncludeFiles, FExcludeFiles, FIncludeDirectories, FExcludeDirectories })
  {
    Edit->clear();
  }
  FExcludeAll->setChecked(false);
  UpdateControls();
}

bool EditMaskDialog::Edit(QString & Mask, QWidget * Parent)
{
  EditMaskDialog Dialog(Mask, Parent);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Mask = Dialog.Mask();
  return true;
}
