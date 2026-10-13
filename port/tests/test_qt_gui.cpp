// The Qt shell's dialogs and its whole path to core.
//
//   test_qt_gui dialogs -platform offscreen
//     the query and prompt dialogs alone: buttons, cancel, OnSubmit that
//     keeps the dialog open, never-ask-again, the timeout, hidden echo
//   test_qt_gui language -platform offscreen
//     the shell's and Qt's own texts in Brazilian Portuguese
//   test_qt_gui session /ini=nul <port> -platform offscreen
//     the main window opening a session (run_qt_gui.py's rclone SFTP
//     server: user linscp, password secret), every question core asks
//     answered by a responder that acts on the modal dialog in front
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

#include "Language.h"
#include "MainWindow.h"
#include "PromptDialog.h"
#include "QtFrontend.h"
#include "QueryDialog.h"

namespace
{

int Failures = 0;

void Check(bool Condition, const char * What)
{
  if (!Condition)
  {
    std::printf("FAIL %s\n", What);
    Failures++;
  }
}

CoreBridge::TQueryButton Button(unsigned int Answer, const char * Caption)
{
  CoreBridge::TQueryButton Result;
  Result.Answer = Answer;
  Result.Caption = Caption;
  return Result;
}

CoreBridge::TQuery YesNoCancel()
{
  CoreBridge::TQuery Query;
  Query.Type = CoreBridge::TQueryType::Confirmation;
  Query.Message = "Overwrite?";
  Query.MoreMessages = "file.txt";
  Query.Buttons = { Button(CoreBridge::AnswerYes, "&Yes"), Button(CoreBridge::AnswerNo, "&No"),
    Button(CoreBridge::AnswerCancel, "Cancel") };
  Query.CancelAnswer = CoreBridge::AnswerCancel;
  return Query;
}

// Runs Action once the dialog's event loop is up, then the dialog.
template <typename TDialog, typename TAction>
int ExecWith(TDialog & Dialog, TAction Action)
{
  QTimer::singleShot(0, &Dialog, [&Dialog, Action]() { Action(Dialog); });
  return Dialog.exec();
}

void Dialogs()
{
  {
    QueryDialog Dialog(YesNoCancel());
    Check(Dialog.Buttons().size() == 3, "a button per answer");
    Check((Dialog.Buttons()[0]->text() == "&Yes") && (Dialog.Buttons()[2]->text() == "Cancel"),
      "in order, with core's captions");
    Check(Dialog.Buttons()[0]->isDefault(), "the first is the default");
    Check(Dialog.NeverAskAgain() == nullptr, "no never-ask box unless asked for");
    ExecWith(Dialog, [](QueryDialog & D) { D.Buttons()[1]->click(); });
    Check(Dialog.Result().Answer == CoreBridge::AnswerNo, "a click answers with its button");
  }
  {
    QueryDialog Dialog(YesNoCancel());
    ExecWith(Dialog, [](QueryDialog & D) { D.reject(); });
    Check(Dialog.Result().Answer == CoreBridge::AnswerCancel, "closing answers Cancel");
  }
  {
    CoreBridge::TQuery Query = YesNoCancel();
    int Submits = 0;
    CoreBridge::TQueryButton Copy = Button(CoreBridge::AnswerRetry, "&Copy key");
    Copy.OnSubmit = [&Submits]() { Submits++; return 0u; };
    Query.Buttons.push_back(Copy);
    QueryDialog Dialog(Query);
    ExecWith(Dialog, [&Submits](QueryDialog & D)
    {
      D.Buttons()[3]->click();
      Check(Submits == 1, "an OnSubmit button acts");
      Check(D.isVisible(), "and leaves the dialog open");
      D.Buttons()[0]->click();
    });
    Check(Dialog.Result().Answer == CoreBridge::AnswerYes, "the dialog then answers as usual");
  }
  {
    CoreBridge::TQuery Query = YesNoCancel();
    CoreBridge::TQueryButton Retry = Button(CoreBridge::AnswerRetry, "&Retry");
    Retry.OnSubmit = []() { return static_cast<unsigned int>(CoreBridge::AnswerAbort); };
    Query.Buttons.push_back(Retry);
    QueryDialog Dialog(Query);
    ExecWith(Dialog, [](QueryDialog & D) { D.Buttons()[3]->click(); });
    Check(Dialog.Result().Answer == CoreBridge::AnswerAbort, "an OnSubmit's answer closes the dialog with it");
  }
  {
    CoreBridge::TQuery Query = YesNoCancel();
    Query.NeverAskAgainCheck = true;
    QueryDialog Dialog(Query);
    ExecWith(Dialog, [](QueryDialog & D)
    {
      D.NeverAskAgain()->setChecked(true);
      D.Buttons()[0]->click();
    });
    Check(Dialog.Result().NeverAskAgain && (Dialog.Result().Answer == CoreBridge::AnswerYes),
      "never-ask-again comes back with the answer");
  }
  {
    CoreBridge::TQuery Query = YesNoCancel();
    Query.Timeout = 1000;
    Query.TimeoutAnswer = CoreBridge::AnswerNo;
    QueryDialog Dialog(Query);
    Check(Dialog.Buttons()[1]->text().contains(" s)"), "the timeout counts down on its answer");
    Dialog.exec();
    Check(Dialog.Result().Answer == CoreBridge::AnswerNo, "and answers by itself");
  }
  {
    CoreBridge::TPrompt Prompt;
    Prompt.Kind = CoreBridge::TPromptKind::KeybInteractive;
    Prompt.Name = "Server prompt";
    Prompt.Instructions = "Two questions";
    CoreBridge::TPromptField User;
    User.Label = "Username: ";
    User.Echo = true;
    User.Value = "linscp";
    CoreBridge::TPromptField Code;
    Code.Label = "Code: ";
    Code.Echo = false;
    Prompt.Fields = { User, Code };
    PromptDialog Dialog(Prompt);
    Check(Dialog.Fields().size() == 2, "a field per prompt");
    Check(Dialog.Fields()[0]->text() == "linscp", "starting from core's value");
    Check(Dialog.Fields()[0]->echoMode() == QLineEdit::Normal, "echoed where the prompt says");
    Check(Dialog.Fields()[1]->echoMode() == QLineEdit::Password, "hidden where it does not");
    const int Result = ExecWith(Dialog, [](PromptDialog & D)
    {
      D.Fields()[1]->setText("123456");
      D.accept();
    });
    Check(Result == QDialog::Accepted, "OK accepts");
    Dialog.Fill(Prompt);
    Check(Prompt.Fields[1].Value == "123456", "and the answers come back");
  }
  {
    CoreBridge::TPrompt Prompt;
    Prompt.Fields.resize(1);
    PromptDialog Dialog(Prompt);
    Check(ExecWith(Dialog, [](PromptDialog & D) { D.reject(); }) == QDialog::Rejected, "Cancel cancels");
  }
}

void Language()
{
  const TLanguage Language = SetUpLanguage(QStringLiteral("pt_BR"));
  Check(Language.Shell, "the shell's pt_BR translation loads");
  Check(Language.Qt, "and Qt's own");
  QueryDialog Query(YesNoCancel());
  Check(Query.windowTitle() == QStringLiteral("Confirmar"), "a query's title is the shell's translation");
  // Qt's own texts. (A standard button's label itself comes from the
  // platform theme: on a GTK desktop GTK's, in the desktop's language.)
  Check(QCoreApplication::translate("QPlatformTheme", "Cancel") == QStringLiteral("Cancelar"),
    "Qt's own texts are its pt_BR translation");
  CoreBridge::TPrompt Prompt;
  Prompt.Fields.resize(1);
  PromptDialog Dialog(Prompt);
  Check(Dialog.windowTitle() == QStringLiteral("Autenticação"), "and the prompt's title the shell's");
}

// Acts on whatever modal dialog is in front, as a user would.
class TResponder : public QObject
{
public:
  std::deque<QString> Passwords;
  int HostKeyQueries = 0;
  int PasswordPrompts = 0;
  int Errors = 0;

  TResponder()
  {
    QTimer * Timer = new QTimer(this);
    connect(Timer, &QTimer::timeout, this, &TResponder::Act);
    Timer->start(50);
  }

private:
  void Act()
  {
    QWidget * Modal = QApplication::activeModalWidget();
    if (QueryDialog * Query = qobject_cast<QueryDialog *>(Modal))
    {
      if (Query->windowTitle() == QueryDialog::tr("Error"))
      {
        Errors++;
      }
      else
      {
        // The unknown host key: its first button, Accept.
        HostKeyQueries++;
      }
      Query->Buttons()[0]->click();
    }
    else if (PromptDialog * Prompt = qobject_cast<PromptDialog *>(Modal))
    {
      PasswordPrompts++;
      if (Passwords.empty() || Prompt->Fields().isEmpty())
      {
        Prompt->reject();
      }
      else
      {
        Prompt->Fields()[0]->setText(Passwords.front());
        Passwords.pop_front();
        Prompt->accept();
      }
    }
  }
};

void Session(const char * Port)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Passwords = { "wrong", "secret" };
    const bool Opened = Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port)));
    Check(Opened, "the main window opens the session");
    Check(Responder.HostKeyQueries == 1, "asking about the host key in a dialog");
    Check(Responder.PasswordPrompts == 2, "and for the password, again after a wrong one");
    Check(Responder.Errors == 0, "with no error shown");
    Check((Window.Session() != nullptr) && Window.Session()->Active(), "the session is active");

    Responder.Passwords.clear();
    Check(!Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))),
      "a cancelled password prompt does not connect");
    Check(Responder.Errors == 1, "and core's error is shown");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

}  // namespace

int main(int argc, char ** argv)
{
  QApplication App(argc, argv);
  if ((argc >= 2) && (std::strcmp(argv[1], "dialogs") == 0))
  {
    Dialogs();
  }
  else if ((argc >= 2) && (std::strcmp(argv[1], "language") == 0))
  {
    Language();
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "session") == 0))
  {
    Session(argv[3]);
  }
  else
  {
    std::fprintf(stderr, "usage: test_qt_gui dialogs | session /ini=nul <port>\n");
    return 2;
  }
  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures ? 1 : 0;
}
