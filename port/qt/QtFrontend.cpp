#include "QtFrontend.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "PromptDialog.h"
#include "QueryDialog.h"
#include "TransferDialog.h"

QtFrontend::QtFrontend() = default;
QtFrontend::~QtFrontend() = default;

CoreBridge::TQueryResult QtFrontend::Query(const CoreBridge::TQuery & Query)
{
  QueryDialog Dialog(Query, FWindow);
  Dialog.exec();
  return Dialog.Result();
}

bool QtFrontend::Prompt(CoreBridge::TPrompt & Prompt)
{
  PromptDialog Dialog(Prompt, FWindow);
  if (Dialog.exec() != QDialog::Accepted)
  {
    return false;
  }
  Dialog.Fill(Prompt);
  return true;
}

void QtFrontend::ShowError(const CoreBridge::TError & Error)
{
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Error;
  Query.Message = Error.Message;
  Query.MoreMessages = Error.MoreMessages;
  CoreBridge::TQueryButton OK;
  OK.Answer = CoreBridge::AnswerOK;
  OK.Caption = "OK";
  Query.Buttons.push_back(OK);
  Query.CancelAnswer = CoreBridge::AnswerOK;
  QueryDialog Dialog(Query, FWindow);
  Dialog.exec();
}

// As upstream's forms/Banner.cpp: the text in a fixed font, as servers lay
// banners out for a terminal.
void QtFrontend::Banner(const std::string & SessionName, const std::string & Banner,
  bool CanNeverShowAgain, bool & NeverShowAgain)
{
  QDialog Dialog(FWindow);
  Dialog.setWindowTitle(QString::fromStdString(SessionName));
  QVBoxLayout * Layout = new QVBoxLayout(&Dialog);
  QPlainTextEdit * Text = new QPlainTextEdit(QString::fromStdString(Banner), &Dialog);
  Text->setReadOnly(true);
  Text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  Text->setMinimumSize(560, 300);
  Layout->addWidget(Text);
  QCheckBox * Never = nullptr;
  if (CanNeverShowAgain)
  {
    Never = new QCheckBox(QDialog::tr("&Never show this banner again"), &Dialog);
    Layout->addWidget(Never);
  }
  QDialogButtonBox * Box = new QDialogButtonBox(QDialogButtonBox::Ok, &Dialog);
  QObject::connect(Box, &QDialogButtonBox::accepted, &Dialog, &QDialog::accept);
  Layout->addWidget(Box);
  Dialog.exec();
  NeverShowAgain = (Never != nullptr) && Never->isChecked();
}

void QtFrontend::Information(const std::string & Text)
{
  if (FStatus)
  {
    FStatus(QString::fromStdString(Text));
  }
}

void QtFrontend::TransferStarted(bool Upload, int FileCount)
{
  FTransfer.reset(new TransferDialog(Upload, FileCount, FWindow));
  FTransfer->show();
}

bool QtFrontend::TransferProgress(const CoreBridge::TTransferProgress & Progress)
{
  if (FTransfer)
  {
    FTransfer->Update(Progress);
    return !FTransfer->Cancelled();
  }
  return true;
}

void QtFrontend::TransferFinished()
{
  FTransfer.reset();
}

void QtFrontend::BusyStart()
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
}

void QtFrontend::BusyEnd()
{
  QApplication::restoreOverrideCursor();
}

void QtFrontend::ProcessEvents()
{
  QApplication::processEvents();
}

void QtFrontend::SetClipboard(const std::string & Text)
{
  QApplication::clipboard()->setText(QString::fromStdString(Text));
}

bool QtFrontend::GetClipboard(std::string & Text)
{
  Text = QApplication::clipboard()->text().toStdString();
  return !Text.empty();
}
