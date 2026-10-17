#include "QtFrontend.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QVBoxLayout>

#include "PromptDialog.h"
#include "QueryDialog.h"
#include "TransferDialog.h"

QtFrontend::QtFrontend() = default;
QtFrontend::~QtFrontend() = default;

namespace
{

// As FlashOnBackground: a question while another program is in front,
// with Environment's "Flash taskbar button when attention is needed".
void FlashOnBackground(QWidget * Window)
{
  if ((Window == nullptr) || (QApplication::activeWindow() != nullptr))
  {
    return;
  }
  try
  {
    if (CoreBridge::Preferences().FlashTaskbar)
    {
      QApplication::alert(Window->window());
    }
  }
  catch (const std::exception &)
  {
    // No shell's configuration (a tool's own frontend): not flashed.
  }
}

}  // namespace

CoreBridge::TQueryResult QtFrontend::Query(const CoreBridge::TQuery & Query)
{
  QueryDialog Dialog(Query, FWindow);
  FlashOnBackground(FWindow);
  Dialog.exec();
  return Dialog.Result();
}

bool QtFrontend::Prompt(CoreBridge::TPrompt & Prompt)
{
  PromptDialog Dialog(Prompt, FWindow);
  FlashOnBackground(FWindow);
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

void QtFrontend::ShowErrorList(const std::vector<CoreBridge::TError> & Errors)
{
  if (Errors.empty())
  {
    return;
  }
  auto Button = [](unsigned Answer, const std::string & Caption)
  {
    CoreBridge::TQueryButton Result;
    Result.Answer = Answer;
    Result.Caption = Caption;
    return Result;
  };
  const int Count = static_cast<int>(Errors.size());
  CoreBridge::TQuery Ask;
  Ask.Type = CoreBridge::TQueryType::Error;
  Ask.Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ErrorListCount))
    .replace(QStringLiteral("%d"), QString::number(Count)).toStdString();
  Ask.Buttons = { Button(CoreBridge::AnswerOK, "OK"), Button(CoreBridge::AnswerCancel, "Cancel") };
  Ask.CancelAnswer = CoreBridge::AnswerCancel;
  if (Query(Ask).Answer != CoreBridge::AnswerOK)
  {
    return;
  }
  int Index = 0;
  while (true)
  {
    // "Error %d of %d:\n%s", Previous and Next as there are.
    QString Message = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ErrorListNumber));
    Message.replace(Message.indexOf(QStringLiteral("%d")), 2, QString::number(Index + 1));
    Message.replace(Message.indexOf(QStringLiteral("%d")), 2, QString::number(Count));
    Message.replace(QStringLiteral("%s"), QString::fromStdString(Errors[Index].Message));
    CoreBridge::TQuery Item;
    Item.Type = CoreBridge::TQueryType::Error;
    Item.Message = Message.toStdString();
    Item.MoreMessages = Errors[Index].MoreMessages;
    if (Index > 0)
    {
      Item.Buttons.push_back(Button(CoreBridge::AnswerYes, CoreBridge::Text(CoreBridge::TText::PreviousButton)));
    }
    if (Index < Count - 1)
    {
      Item.Buttons.push_back(Button(CoreBridge::AnswerNo, CoreBridge::Text(CoreBridge::TText::NextButton)));
    }
    Item.Buttons.push_back(Button(CoreBridge::AnswerOK, "OK"));
    Item.CancelAnswer = CoreBridge::AnswerOK;
    const unsigned Answer = Query(Item).Answer;
    if (Answer == CoreBridge::AnswerYes)
    {
      Index--;
    }
    else if (Answer == CoreBridge::AnswerNo)
    {
      Index++;
    }
    else
    {
      break;
    }
  }
}

// As TAuthenticateForm::Banner: the text, in the window's font or (Use
// Monospaced Font, as last chosen for the banner) a fixed one, as servers
// often lay banners out for a terminal; its menu as BannerPopupMenu: Copy,
// Select All, Use Monospaced Font.
void QtFrontend::Banner(const std::string & SessionName, const std::string & Banner,
  bool CanNeverShowAgain, bool & NeverShowAgain, bool & MonospacedFont)
{
  QDialog Dialog(FWindow);
  Dialog.setWindowTitle(QString::fromStdString(SessionName));
  QVBoxLayout * Layout = new QVBoxLayout(&Dialog);
  QPlainTextEdit * Text = new QPlainTextEdit(QString::fromStdString(Banner), &Dialog);
  Text->setObjectName(QStringLiteral("Banner"));
  Text->setAccessibleName(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::AuthenticationBanner)));
  Text->setReadOnly(true);
  const QFont Proportional = Text->font();
  bool Monospaced = MonospacedFont;
  auto ApplyFont = [Text, &Monospaced, Proportional]()
  {
    Text->setFont(Monospaced ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : Proportional);
  };
  ApplyFont();
  Text->setContextMenuPolicy(Qt::CustomContextMenu);
  QAction * MonospacedAction = new QAction(QDialog::tr("Use &Monospaced Font"), &Dialog);
  MonospacedAction->setObjectName(QStringLiteral("BannerMonospacedFont"));
  MonospacedAction->setCheckable(true);
  MonospacedAction->setChecked(Monospaced);
  QObject::connect(MonospacedAction, &QAction::toggled, &Dialog, [&Monospaced, ApplyFont](bool On)
  {
    Monospaced = On;
    ApplyFont();
  });
  QObject::connect(Text, &QWidget::customContextMenuRequested, &Dialog, [Text, MonospacedAction](const QPoint & Pos)
  {
    QMenu Menu;
    Menu.addAction(QDialog::tr("&Copy"), QKeySequence::Copy, Text, &QPlainTextEdit::copy)->setEnabled(Text->textCursor().hasSelection());
    Menu.addAction(QDialog::tr("Select &All"), QKeySequence::SelectAll, Text, &QPlainTextEdit::selectAll);
    Menu.addSeparator();
    Menu.addAction(MonospacedAction);
    Menu.exec(Text->viewport()->mapToGlobal(Pos));
  });
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
  MonospacedFont = Monospaced;
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

CoreBridge::TTransferAnswer QtFrontend::TransferControl(const CoreBridge::TTransferProgress & Progress)
{
  if (FTransfer)
  {
    FTransfer->Update(Progress);
    return FTransfer->Answer();
  }
  return CoreBridge::TTransferAnswer();
}

void QtFrontend::TransferCompleted()
{
  if (FTransfer && (FTransfer->OnceFinished() != TransferDialog::TOnceFinished::Idle) && FOnceFinished)
  {
    FOnceFinished(static_cast<int>(FTransfer->OnceFinished()));
  }
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

bool QtFrontend::StartProgram(const std::string & Program, const std::string & Params, const std::string & Directory)
{
  const QString AProgram = QString::fromStdString(Program);
  const QString AParams = QString::fromStdString(Params);
  const QString ADirectory = QString::fromStdString(Directory);
  if (FStartProgram)
  {
    return FStartProgram(AProgram, AParams, ADirectory);
  }
  QProcess Process;
  Process.setProgram(AProgram);
#if defined(Q_OS_WIN)
  // As ShellExecute has them: the program parses its own command line.
  Process.setNativeArguments(AParams);
#else
  Process.setArguments(QProcess::splitCommand(AParams));
#endif
  if (!ADirectory.isEmpty())
  {
    Process.setWorkingDirectory(ADirectory);
  }
  return Process.startDetached();
}
