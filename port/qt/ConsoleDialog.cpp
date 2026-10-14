#include "ConsoleDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

ConsoleDialog::ConsoleDialog(CoreBridge::TSession * Session, QWidget * Parent) :
  QDialog(Parent),
  FSession(Session)
{
  setWindowTitle(tr("Console"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Fields = new QFormLayout();
  QHBoxLayout * Row = new QHBoxLayout();
  FCommand = new QComboBox(this);
  FCommand->setEditable(true);
  FCommand->setInsertPolicy(QComboBox::InsertAtTop);
  Row->addWidget(FCommand, 1);
  FExecute = new QPushButton(tr("&Execute"), this);
  FExecute->setDefault(true);
  Row->addWidget(FExecute);
  Fields->addRow(tr("Enter &command:"), Row);
  FDirectory = new QLabel(this);
  FDirectory->setTextInteractionFlags(Qt::TextSelectableByMouse);
  Fields->addRow(tr("Current directory:"), FDirectory);
  Layout->addLayout(Fields);
  Layout->addWidget(new QLabel(tr("Do not execute commands that require user-input or data transfer"), this));
  FOutput = new QPlainTextEdit(this);
  FOutput->setReadOnly(true);
  FOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
  FOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  // As upstream's: light grey on black.
  QPalette Palette = FOutput->palette();
  Palette.setColor(QPalette::Base, Qt::black);
  Palette.setColor(QPalette::Text, QColor(0xBB, 0xBB, 0xBB));
  FOutput->setPalette(Palette);
  Layout->addWidget(FOutput, 1);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  FClose = Buttons->button(QDialogButtonBox::Close);
  connect(Buttons, &QDialogButtonBox::rejected, this, &ConsoleDialog::reject);
  Layout->addWidget(Buttons);
  connect(FExecute, &QPushButton::clicked, this, [this]() { Execute(FCommand->currentText()); });
  connect(FCommand->lineEdit(), &QLineEdit::returnPressed, this, [this]() { Execute(FCommand->currentText()); });
  connect(FCommand, &QComboBox::editTextChanged, this, &ConsoleDialog::UpdateControls);
  resize(640, 420);
  UpdateControls();
}

// As TConsoleDialog::UpdateControls: no command while one runs.
void ConsoleDialog::UpdateControls()
{
  FDirectory->setText(QString::fromStdString(FSession->CurrentDirectory()));
  FExecute->setEnabled(!FCommand->currentText().trimmed().isEmpty() && !FExecuting);
  FClose->setEnabled(!FExecuting);
}

// As TConsoleDialog::DoExecuteCommand.
bool ConsoleDialog::Execute(const QString & Command)
{
  if (FExecuting || Command.trimmed().isEmpty())
  {
    return false;
  }
  // The history: this command first, once.
  const int Index = FCommand->findText(Command);
  if (Index >= 0)
  {
    FCommand->removeItem(Index);
  }
  FCommand->insertItem(0, Command);
  FCommand->setCurrentIndex(0);
  FCommand->lineEdit()->selectAll();
  FOutput->appendPlainText(QStringLiteral("%1$ %2").arg(QString::fromStdString(FSession->CurrentDirectory()), Command));
  FExecuting = true;
  UpdateControls();
  QApplication::processEvents();
  bool Result = true;
  try
  {
    FSession->RunCommand(Command.toStdString(), [this](const std::string & Line)
    {
      FOutput->appendPlainText(QString::fromStdString(Line));
    });
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Result = false;
    if (!E.Aborted)
    {
      QtFrontend Frontend;
      Frontend.SetWindow(this);
      Frontend.ShowError(E.Error);
    }
  }
  FExecuting = false;
  UpdateControls();
  return Result;
}

void ConsoleDialog::reject()
{
  // As FormCloseQuery: not while a command runs.
  if (!FExecuting)
  {
    QDialog::reject();
  }
}

void ConsoleDialog::AddOutput(const QStringList & Lines)
{
  for (const QString & Line : Lines)
  {
    FOutput->appendPlainText(Line);
  }
}
