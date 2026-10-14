#include "CopyLocalDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "CoreBridge.h"
#include "QtFrontend.h"

namespace
{

QString CoreText(CoreBridge::TText Text, const QString & Arg = QString())
{
  return QString::fromStdString(CoreBridge::Text(Text)).replace(QStringLiteral("%s"), Arg);
}

QString WithSeparator(const QString & Directory)
{
  const QString Native = QDir::toNativeSeparators(Directory);
  return (Native.isEmpty() || Native.endsWith(QDir::separator())) ? Native : (Native + QDir::separator());
}

// A query with OK and Cancel, as WinSCP's MessageDialog(qaOK | qaCancel).
bool Confirm(QWidget * Parent, const QString & Message)
{
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Confirmation;
  Query.Message = Message.toStdString();
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  CoreBridge::TQueryButton Cancel;
  Cancel.Answer = CoreBridge::AnswerCancel;
  Cancel.Caption = "Cancel";
  Query.Buttons = { OK, Cancel };
  Query.CancelAnswer = CoreBridge::AnswerCancel;
  QtFrontend Frontend;
  Frontend.SetWindow(Parent);
  return Frontend.Query(Query).Answer != CoreBridge::AnswerCancel;
}

}  // namespace

CopyLocalDialog::CopyLocalDialog(bool Move, bool MultipleFiles, const QString & Directory, const QString & FileMask,
  QWidget * Parent) :
  QDialog(Parent),
  FMultipleFiles(MultipleFiles)
{
  setWindowTitle(CoreText(Move ? CoreBridge::TText::CopyLocalMoveCaption : CoreBridge::TText::CopyLocalCopyCaption));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FDirectory = new QLineEdit(this);
  FDirectory->setMinimumWidth(420);
  QLabel * Label = new QLabel(tr("&Target path:"), this);
  Label->setBuddy(FDirectory);
  Layout->addWidget(Label);
  QHBoxLayout * Row = new QHBoxLayout();
  Row->addWidget(FDirectory, 1);
  QPushButton * Browse = new QPushButton(tr("B&rowse..."), this);
  Row->addWidget(Browse);
  Layout->addLayout(Row);
  connect(Browse, &QPushButton::clicked, this, [this]()
  {
    const QString Chosen = QFileDialog::getExistingDirectory(this, CoreText(CoreBridge::TText::SelectLocalDirectory),
      this->Directory());
    if (!Chosen.isEmpty())
    {
      SetDirectoryAndFileMask(Chosen, this->FileMask());
    }
  });
  // As DirectoryEditExit: a directory typed alone gets the mask.
  connect(FDirectory, &QLineEdit::editingFinished, this, &CopyLocalDialog::ValidateDirectoryEdit);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(Buttons, &QDialogButtonBox::accepted, this, &CopyLocalDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  SetDirectoryAndFileMask(Directory, FileMask);
}

void CopyLocalDialog::SetDirectoryAndFileMask(const QString & Directory, const QString & FileMask)
{
  FDirectory->setText(WithSeparator(Directory) + FileMask);
}

// As GetDirectory (ExtractFilePath): up to the last separator.
QString CopyLocalDialog::Directory() const
{
  const QString Text = QDir::toNativeSeparators(FDirectory->text());
  const int Separator = Text.lastIndexOf(QDir::separator());
  return (Separator >= 0) ? Text.left(Separator + 1) : QString();
}

QString CopyLocalDialog::FileMask() const
{
  const QString Text = QDir::toNativeSeparators(FDirectory->text());
  return Text.mid(Text.lastIndexOf(QDir::separator()) + 1);
}

void CopyLocalDialog::ValidateDirectoryEdit()
{
  if (QFileInfo(FDirectory->text()).isDir())
  {
    FDirectory->setText(WithSeparator(FDirectory->text()) + QStringLiteral("*.*"));
  }
}

void CopyLocalDialog::Failed()
{
  FDirectory->selectAll();
  FDirectory->setFocus();
}

// As CopyDialogValidateLocalDirectory: one that is not there, created after
// asking.
bool CopyLocalDialog::ValidateLocalDirectory()
{
  const QString Path = Directory();
  if (QFileInfo(Path).isDir())
  {
    return true;
  }
  if (!Confirm(this, CoreText(CoreBridge::TText::CreateLocalDirectory, Path)))
  {
    return false;
  }
  if (!QDir().mkpath(QDir::fromNativeSeparators(Path)))
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Error;
    Query.Message = CoreText(CoreBridge::TText::CreateLocalDirectoryError, Path).toStdString();
    CoreBridge::TQueryButton OK;
    OK.Answer = CoreBridge::AnswerOK;
    OK.Caption = "OK";
    Query.Buttons = { OK };
    Query.CancelAnswer = CoreBridge::AnswerOK;
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.Query(Query);
    return false;
  }
  return true;
}

// As CopyDialogValidateFileMask: several files to one name, confirmed.
bool CopyLocalDialog::ValidateFileMask()
{
  if (!FMultipleFiles || CoreBridge::IsFileNameMask(FileMask().toStdString()))
  {
    return true;
  }
  return Confirm(this, QString::fromStdString(CoreBridge::MultiFilesToOneConfirmation(FDirectory->text().toStdString(), false)));
}

void CopyLocalDialog::accept()
{
  ValidateDirectoryEdit();
  if (!ValidateLocalDirectory() || !ValidateFileMask())
  {
    Failed();
    return;
  }
  QDialog::accept();
}
