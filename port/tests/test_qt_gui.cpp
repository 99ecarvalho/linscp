// The Qt shell's dialogs and its whole path to core.
//
//   test_qt_gui dialogs -platform offscreen
//     the query and prompt dialogs alone: buttons, cancel, OnSubmit that
//     keeps the dialog open, never-ask-again, the timeout, hidden echo
//   test_qt_gui language -platform offscreen
//     the shell's and Qt's own texts in Brazilian Portuguese
//   test_qt_gui sites /ini=<file> <port> <OpenSSH key> <directory> -platform offscreen
//     the site manager: a site filled in, saved with its password, read
//     back, its port following the protocol, opened from the dialog with no
//     prompt, deleted; folders made, saved into, renamed and deleted; the
//     advanced settings; an OpenSSH key converted on the way
//   test_qt_gui panel /ini=nul <port> -platform offscreen
//     the remote panel over a session: order and sorting, new folder,
//     rename, delete (core's confirmation), the path box, back, forward,
//     parent, root, home
//   test_qt_gui local /ini=nul <directory> -platform offscreen
//     the local panel in an empty directory: order, hidden files, new
//     folder, rename, a change made outside it, delete (for good: the
//     trash is the user's), opening a file, navigation, a missing directory
//   test_qt_gui transfer /ini=nul <port> <directory> -platform offscreen
//     transfers between the panels: an upload from the local panel, a
//     download from the remote one, a remote directory moved down, an upload
//     cancelled from the progress window
//   test_qt_gui session /ini=nul <port> -platform offscreen
//     the main window opening a session (run_qt_gui.py's rclone SFTP
//     server: user linscp, password secret), every question core asks
//     answered by a responder that acts on the modal dialog in front
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QDragEnterEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

#include <memory>

#include <cstdio>
#include <cstring>
#if defined(_WIN32)
  #include <windows.h>
#endif
#include <deque>
#include <string>

#include "Language.h"
#include "MainWindow.h"
#include "PromptDialog.h"
#include "QtFrontend.h"
#include "LocalPanel.h"
#include "QueryDialog.h"
#include "QueuePanel.h"
#include "TransferDialog.h"

#include <functional>
#include "RemotePanel.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include "SiteAdvanced.h"

#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include "SiteManager.h"

#include <QFile>

#include <QComboBox>
#include <QSpinBox>
#include <QTreeWidget>

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
  QString LastQuery;
  // Answer a warning's Ignore rather than its first button.
  bool IgnoreWarnings = false;
  // Watches the frontend's progress window, and cancels from it if asked.
  QtFrontend * Frontend = nullptr;
  int TransfersSeen = 0;
  bool CancelTransfers = false;
  // Other dialogs (a message box's Yes, a plain dialog's OK), with their
  // titles.
  bool AcceptDialogs = false;
  QStringList Accepted;

  TResponder()
  {
    QTimer * Timer = new QTimer(this);
    connect(Timer, &QTimer::timeout, this, &TResponder::Act);
    Timer->start(50);
  }

private:
  void Act()
  {
    if ((Frontend != nullptr) && (Frontend->Transfer() != nullptr))
    {
      TransfersSeen++;
      if (CancelTransfers)
      {
        Frontend->Transfer()->CancelButton()->click();
      }
      return;
    }
    QWidget * Modal = QApplication::activeModalWidget();
    if (QueryDialog * Query = qobject_cast<QueryDialog *>(Modal))
    {
      const QList<QLabel *> Labels = Query->findChildren<QLabel *>();
      for (const QLabel * Label : Labels)
      {
        if (!Label->text().isEmpty())
        {
          LastQuery = Label->text();
        }
      }
      if (Query->windowTitle() == QueryDialog::tr("Error"))
      {
        Errors++;
      }
      else if (IgnoreWarnings && (Query->windowTitle() == QueryDialog::tr("Warning")) && (Query->Buttons().size() > 1))
      {
        Query->Buttons()[1]->click();
        return;
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
    else if (QMessageBox * Box = qobject_cast<QMessageBox *>(Modal); AcceptDialogs && (Box != nullptr))
    {
      Accepted.append(Box->windowTitle());
      Box->button(QMessageBox::Yes)->click();
    }
    else if (QDialog * Dialog = qobject_cast<QDialog *>(Modal); AcceptDialogs && (Dialog != nullptr))
    {
      Accepted.append(Dialog->windowTitle());
      Dialog->accept();
    }
  }
};

void Sites(const char * Port, const QString & Key, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    {
      SiteManager Dialog(&Window);
      Check(Dialog.Tree()->topLevelItemCount() == 1, "an empty site manager has only \"New Site\"");
      Dialog.SelectNewSite();
      Check(Dialog.Port()->value() == 22, "a new site is SFTP on port 22");
      Dialog.Protocol()->setCurrentIndex(Dialog.Protocol()->findText(QStringLiteral("FTP")));
      Check(Dialog.Port()->value() == 21, "the port follows the protocol");
      Check(Dialog.Encryption()->count() == 3, "FTP offers no encryption, implicit and explicit TLS");
      Dialog.Encryption()->setCurrentIndex(1);
      Check(Dialog.Port()->value() == 990, "and the encryption: implicit TLS on 990");
      Dialog.Protocol()->setCurrentIndex(Dialog.Protocol()->findText(QStringLiteral("SFTP")));
      Check(Dialog.Port()->value() == 22, "back to SFTP, back to 22");
      Dialog.HostName()->setText(QStringLiteral("127.0.0.1"));
      Dialog.Port()->setValue(QString::fromLatin1(Port).toInt());
      Dialog.UserName()->setText(QStringLiteral("linscp"));
      Dialog.Password()->setText(QStringLiteral("secret"));
      Check(Dialog.SaveAs(QStringLiteral("Tests/rclone"), true), "the site saves as Tests/rclone");
      const QTreeWidgetItem * Folder = Dialog.Tree()->topLevelItem(1);
      Check((Folder != nullptr) && (Folder->text(0) == QStringLiteral("Tests")) && (Folder->childCount() == 1),
        "under its folder in the tree");
    }
    {
      SiteManager Dialog(&Window);
      Check(Dialog.SelectSite(QStringLiteral("Tests/rclone")), "a new site manager has it");
      Check((Dialog.HostName()->text() == QStringLiteral("127.0.0.1")) && (Dialog.UserName()->text() == QStringLiteral("linscp")) &&
        (Dialog.Password()->text() == QStringLiteral("secret")) && (Dialog.Port()->value() == QString::fromLatin1(Port).toInt()),
        "with its fields as saved");
      QTimer::singleShot(0, &Dialog, [&Dialog]() { Dialog.accept(); });
      Check(Dialog.exec() == QDialog::Accepted, "Login closes the dialog");
      Check(Window.OpenSite(Dialog.Site()), "and the main window opens the site");
      Check(Responder.PasswordPrompts == 0, "with its saved password, no prompt");
      Check(Responder.HostKeyQueries == 1, "the host key asked about once");
    }
    {
      SiteManager Dialog(&Window);
      Dialog.SelectSite(QStringLiteral("Tests/rclone"));
      Check(Dialog.DeleteSelected(false), "the site deletes");
      Check(Dialog.Tree()->topLevelItemCount() == 1, "and the tree is back to \"New Site\" alone");
    }
    {
      SiteManager Dialog(&Window);
      Dialog.NewFolder(QStringLiteral("Work"));
      Check(Dialog.SelectFolder(QStringLiteral("Work")), "a new folder is in the tree");
      for (const QString & Name : { QStringLiteral("Work/a"), QStringLiteral("Work/b") })
      {
        Dialog.SelectNewSite();
        Dialog.HostName()->setText(QStringLiteral("example.com"));
        Dialog.SaveAs(Name, false);
      }
      Check(CoreBridge::SiteExists("Work/a") && CoreBridge::SiteExists("Work/b"), "sites save into it");
      Dialog.SelectFolder(QStringLiteral("Work"));
      Check(Dialog.RenameSelected(QStringLiteral("Office")), "the folder renames");
      Check(CoreBridge::SiteExists("Office/a") && CoreBridge::SiteExists("Office/b") && !CoreBridge::SiteExists("Work/a"),
        "and its sites with it");
      Dialog.SelectSite(QStringLiteral("Office/a"));
      Check(Dialog.RenameSelected(QStringLiteral("Office/c")) && CoreBridge::SiteExists("Office/c"), "a site renames");
      Dialog.SelectFolder(QStringLiteral("Office"));
      Check(Dialog.DeleteSelected(false) && !CoreBridge::SiteExists("Office/b") && !CoreBridge::SiteExists("Office/c"),
        "a deleted folder takes its sites");
    }
    {
      SiteManager Dialog(&Window);
      Dialog.SelectNewSite();
      Dialog.HostName()->setText(QStringLiteral("example.com"));
      SiteAdvanced Advanced(Dialog.Edited(), &Dialog);
      Advanced.Timeout()->setValue(45);
      Advanced.Proxy()->setCurrentIndex(Advanced.Proxy()->findText(QStringLiteral("HTTP")));
      Check(Advanced.ProxyHost()->isEnabled(), "a proxy's fields open with one");
      Advanced.ProxyHost()->setText(QStringLiteral("proxy.example.com"));
      Advanced.ProxyPort()->setValue(3128);
      Advanced.PrivateKeyFile()->setText(Directory + QStringLiteral("/none.ppk"));
      Dialog.SetEdited(Advanced.Site());
      // The key does not exist: upstream warns, and Ignore saves anyway.
      Responder.IgnoreWarnings = true;
      Check(Dialog.SaveAs(QStringLiteral("Advanced"), false), "a site with advanced settings saves");
      Responder.IgnoreWarnings = false;
      const CoreBridge::TSite Saved = CoreBridge::LoadSite("Advanced");
      Check((Saved.Timeout == 45) && (Saved.Proxy == CoreBridge::TSite::TProxy::Http) && (Saved.ProxyPort == 3128) &&
        (Saved.ProxyHost == "proxy.example.com"), "and they persist");
      CoreBridge::DeleteSite("Advanced");
    }
    {
      SiteManager Dialog(&Window);
      Dialog.SelectNewSite();
      Dialog.HostName()->setText(QStringLiteral("example.com"));
      CoreBridge::TSite Site = Dialog.Edited();
      Site.PrivateKeyFile = Key.toStdString();
      Dialog.SetEdited(Site);
      const QString Converted = Directory + QStringLiteral("/converted.ppk");
      Dialog.ChooseConvertedKeyFile = [Converted](const QString &) { return Converted; };
      Check(Dialog.VerifyKey(), "an OpenSSH key is converted when asked to");
      Check(QFile::exists(Converted), "to the file chosen");
      Check(Dialog.Edited().PrivateKeyFile == Converted.toStdString(), "and the site uses it from then on");
    }
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void Panel(const char * Port)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Passwords = { "secret" };
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "the session opens");
    RemotePanel * Panel = Window.Panel();
    Check(Panel->Path() == QStringLiteral("/"), "the panel shows the root");
    Check(Panel->Names() == QStringList({ QStringLiteral("sub"), QStringLiteral("hello.txt") }),
      "directories first, no \"..\" at the root");
    Panel->View()->sortByColumn(FileListModel::SizeColumn, Qt::DescendingOrder);
    Check(Panel->Names().first() == QStringLiteral("sub"), "directories first sorted the other way too");
    Panel->View()->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);

    Check(Panel->NewFolder(QStringLiteral("made")) && Panel->Names().contains(QStringLiteral("made")), "a new folder appears");
    Check(Panel->Selected() == QStringList({ QStringLiteral("made") }), "selected");
    Check(Panel->Rename(QStringLiteral("made"), QStringLiteral("renamed")) && Panel->Names().contains(QStringLiteral("renamed")),
      "a rename shows");
    Panel->Select({ QStringLiteral("renamed"), QStringLiteral("hello.txt") });
    Check(Panel->Selected().size() == 2, "two entries select");
    Check(Panel->DeleteSelected(true), "and delete, confirmed");
    Check(Responder.LastQuery.contains(QStringLiteral("2")), "with core's confirmation counting them");
    Check(Panel->Names() == QStringList({ QStringLiteral("sub") }), "leaving the rest");

    Panel->PathEdit()->setText(QStringLiteral("/sub"));
    emit Panel->PathEdit()->returnPressed();
    Check(Panel->Path() == QStringLiteral("/sub"), "the path box changes directory");
    Check(Panel->Names().first() == QStringLiteral(".."), "with \"..\" first below the root");
    Check(Panel->Back() && (Panel->Path() == QStringLiteral("/")), "back");
    Check(Panel->Forward() && (Panel->Path() == QStringLiteral("/sub")), "forward");
    Check(Panel->Parent() && (Panel->Path() == QStringLiteral("/")), "parent");
    Panel->Open(QStringLiteral("sub"));
    Check(Panel->Path() == QStringLiteral("/sub"), "opening a directory enters it");
    Check(Panel->Root() && (Panel->Path() == QStringLiteral("/")), "root");
    Panel->ChangeDirectory(QStringLiteral("sub"));
    Check(Panel->Home() && (Panel->Path() == QStringLiteral("/")), "home");
    const bool Changed = Panel->ChangeDirectory(QStringLiteral("/nowhere"));
    Check(!Changed && (Responder.Errors == 1), "a missing directory is core's error");
    Check(Panel->Path() == QStringLiteral("/"), "and the panel stays");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void Local(const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    const QDir Dir(Directory);
    Dir.mkdir(QStringLiteral("sub"));
    for (const QString & Name : { QStringLiteral("a.txt"), QStringLiteral("b.bin"), QStringLiteral(".hidden") })
    {
      QFile File(Dir.filePath(Name));
      File.open(QIODevice::WriteOnly);
      File.write(QByteArray(Name == QStringLiteral("b.bin") ? 100 : 1, 'x'));
    }
#if defined(Q_OS_WIN)
    // On Windows a file is hidden by its attribute, not its dot.
    SetFileAttributesW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(Dir.filePath(QStringLiteral(".hidden"))).utf16()),
      FILE_ATTRIBUTE_HIDDEN);
#endif
    LocalPanel * Panel = Window.Local();
    Panel->DeleteToTrash = false;
    QString Opened;
    Panel->OpenFile = [&Opened](const QString & FileName) { Opened = FileName; };
    Check(Panel->ChangeDirectory(Directory), "the local panel enters the directory");
    Check(Panel->Names() == QStringList({ QStringLiteral(".."), QStringLiteral("sub"), QStringLiteral("a.txt"), QStringLiteral("b.bin") }),
      "\"..\", directories first, hidden files not shown");
    Panel->SetShowHidden(true);
    Check(Panel->Names().contains(QStringLiteral(".hidden")), "until asked for");
    Panel->SetShowHidden(false);

    Check(Panel->NewFolder(QStringLiteral("made")) && Dir.exists(QStringLiteral("made")), "a new folder is made on disk");
    Check(Panel->Rename(QStringLiteral("made"), QStringLiteral("renamed")) && Dir.exists(QStringLiteral("renamed")) &&
      !Dir.exists(QStringLiteral("made")), "and renamed");
    Check(!Panel->Rename(QStringLiteral("a.txt"), QStringLiteral("b.bin")) && (Responder.Errors == 1),
      "renaming onto an existing file is refused");
    {
      QFile File(Dir.filePath(QStringLiteral("external.txt")));
      File.open(QIODevice::WriteOnly);
    }
    QElapsedTimer Timer;
    Timer.start();
    while (!Panel->Names().contains(QStringLiteral("external.txt")) && (Timer.elapsed() < 5000))
    {
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    Check(Panel->Names().contains(QStringLiteral("external.txt")), "a file made outside the panel appears by itself");

    Panel->Select({ QStringLiteral("renamed"), QStringLiteral("a.txt") });
    Check(Panel->DeleteSelected(true), "two entries delete, confirmed");
    Check(Responder.LastQuery.contains(QStringLiteral("2")), "with core's confirmation counting them");
    Check(!Dir.exists(QStringLiteral("renamed")) && !Dir.exists(QStringLiteral("a.txt")), "gone from disk");

    Panel->Open(QStringLiteral("b.bin"));
    Check(QFileInfo(Opened) == QFileInfo(Dir.filePath(QStringLiteral("b.bin"))), "a file opens with its application");
    Panel->Open(QStringLiteral("sub"));
    Check(QFileInfo(Panel->Path()) == QFileInfo(Dir.filePath(QStringLiteral("sub"))), "a directory is entered");
    Check(Panel->Back() && (QFileInfo(Panel->Path()) == QFileInfo(Directory)), "back");
    Check(Panel->Forward() && (QFileInfo(Panel->Path()) == QFileInfo(Dir.filePath(QStringLiteral("sub")))), "forward");
    Check(Panel->Parent() && (QFileInfo(Panel->Path()) == QFileInfo(Directory)), "parent");
    Check(Panel->Home() && (QFileInfo(Panel->Path()) == QFileInfo(QDir::homePath())), "home");
    Check(Panel->Root() && QDir(Panel->Path()).isRoot(), "root");
    Check(!Panel->ChangeDirectory(Dir.filePath(QStringLiteral("nowhere"))) && (Responder.Errors == 2), "a missing directory is an error");
    Check(QDir(Panel->Path()).isRoot(), "and the panel stays");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void Transfer(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = { "secret" };
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "the session opens");
    const QDir Dir(Directory);
    {
      QFile File(Dir.filePath(QStringLiteral("up.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("uploaded through the panels\n");
    }
    LocalPanel * Local = Window.Local();
    RemotePanel * Remote = Window.Panel();
    Local->ChangeDirectory(Directory);

    Local->Select({ QStringLiteral("up.txt") });
    Window.ActivatePanel(true);
    Check(Window.TransferSelected(false, false), "the local panel's selection uploads (F5)");
    Check(Remote->Names().contains(QStringLiteral("up.txt")), "and shows in the remote panel");
    Check(Frontend.Transfer() == nullptr, "and gone after");

    Remote->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    Check(Window.TransferSelected(false, false), "the remote panel's selection downloads (F5)");
    QFile Down(Dir.filePath(QStringLiteral("hello.txt")));
    Check(Down.open(QIODevice::ReadOnly) && (Down.readAll() == "Hello, linscp\n"), "into the local panel's directory, whole");
    Check(Local->Names().contains(QStringLiteral("hello.txt")), "which shows it");

    Remote->Select({ QStringLiteral("sub") });
    Window.ActivatePanel(false);
    Check(Window.TransferSelected(true, false), "a remote directory moves down (F6)");
    Check(QFileInfo(Dir.filePath(QStringLiteral("sub"))).isDir() && !Remote->Names().contains(QStringLiteral("sub")),
      "there locally, gone remotely");

    {
      QFile File(Dir.filePath(QStringLiteral("big.bin")));
      File.open(QIODevice::WriteOnly);
      File.write(QByteArray(40 * 1024 * 1024, 'b'));
    }
    Local->Refresh();
    Local->Select({ QStringLiteral("big.bin") });
    Window.ActivatePanel(true);
    Responder.CancelTransfers = true;
    Check(!Window.TransferSelected(false, false), "an upload cancelled from the progress window does not complete");
    Check(Responder.TransfersSeen > 0, "the progress window was up while it ran");

    // In the background, through the queue: the panels catch up by themselves.
    const auto WaitFor = [](const std::function<bool()> & Condition)
    {
      QElapsedTimer Timer;
      Timer.start();
      while (!Condition() && (Timer.elapsed() < 30000))
      {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      return Condition();
    };
    {
      QFile File(Dir.filePath(QStringLiteral("queued.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("through the queue\n");
    }
    Local->Refresh();
    Local->Select({ QStringLiteral("queued.txt") });
    Window.ActivatePanel(true);
    Check(Window.TransferSelected(false, false, true), "an upload goes to the queue");
    Check(Window.Queue()->List()->topLevelItemCount() == 1, "the queue panel lists it");
    Check(WaitFor([Remote]() { return Remote->Names().contains(QStringLiteral("queued.txt")); }),
      "it completes, and the remote panel shows it by itself");
    Check(WaitFor([&Window]() { return Window.Queue()->List()->topLevelItemCount() == 0; }), "the queue empties");

    QFile::remove(Dir.filePath(QStringLiteral("queued.txt")));
    Remote->Select({ QStringLiteral("queued.txt") });
    Window.ActivatePanel(false);
    Check(Window.TransferSelected(false, false, true), "a download goes to the queue");
    Check(WaitFor([Local]() { return Local->Names().contains(QStringLiteral("queued.txt")); }),
      "and the local panel shows it when done");

    Local->Select({ QStringLiteral("big.bin") });
    Window.ActivatePanel(true);
    Check(Window.TransferSelected(false, false, true), "a big upload goes to the queue");
    Window.Queue()->List()->topLevelItem(0)->setSelected(true);
    Check(Window.Queue()->CancelSelected(), "and is cancelled from the queue panel");
    Check(WaitFor([&Window]() { return Window.Queue()->List()->topLevelItemCount() == 0; }), "leaving the queue");
    Window.Panel()->Refresh();
    bool Whole = false;
    for (const CoreBridge::TRemoteEntry & Entry : Window.Session()->List())
    {
      Whole = Whole || ((Entry.Name == "big.bin") && (Entry.Size == 40 * 1024 * 1024));
    }
    Check(!Whole, "and no whole file behind");
    Responder.CancelTransfers = false;
    Check(Frontend.Transfer() == nullptr, "and the window closes");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// Where a row of a panel is, for a drop onto it.
QPoint RowCenter(FileView * View, const QString & Name)
{
  for (int Row = 0; Row < View->model()->rowCount(); Row++)
  {
    const QModelIndex Index = View->model()->index(Row, FileListModel::NameColumn);
    if (Index.data(Qt::DisplayRole).toString() == Name)
    {
      return View->visualRect(Index).center();
    }
  }
  return QPoint();
}

// A drag of Data dropped at Pos of View, as the system delivers it.
void DragOnto(FileView * View, QMimeData * Data, const QPoint & Pos, Qt::KeyboardModifiers Modifiers)
{
  if (Data == nullptr)
  {
    return;
  }
  const Qt::DropActions Actions = Qt::CopyAction | Qt::MoveAction;
  QDragEnterEvent Enter(Pos, Actions, Data, Qt::LeftButton, Modifiers);
  QApplication::sendEvent(View->viewport(), &Enter);
  QDragMoveEvent Move(Pos, Actions, Data, Qt::LeftButton, Modifiers);
  QApplication::sendEvent(View->viewport(), &Move);
  QDropEvent Drop(QPointF(Pos), Actions, Data, Qt::LeftButton, Modifiers);
  QApplication::sendEvent(View->viewport(), &Drop);
}

void DragAndDrop(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = { "secret" };
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "the session opens");
    const QDir Dir(Directory);
    for (const char * Name : { "dragged.txt", "moveme.txt", "local.txt", "external.txt" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    Dir.mkpath(QStringLiteral("down"));
    LocalPanel * Local = Window.Local();
    RemotePanel * Remote = Window.Panel();
    Local->ChangeDirectory(Directory);
    const auto Drag = [](FileView * View) { return std::unique_ptr<QMimeData>(View->MakeDrag()); };

    Local->Select({ QStringLiteral("moveme.txt") });
    std::unique_ptr<QMimeData> Data = Drag(Local->View());
    TFileDrag Read;
    Check(ReadFileDrag(Data.get(), Read) && Read.Local && (Read.Names == QStringList{ QStringLiteral("moveme.txt") }),
      "a local drag carries the panel's selection");
    Check((Data->urls().size() == 1) && (QFileInfo(Data->urls().first().toLocalFile()) == QFileInfo(Dir.filePath(QStringLiteral("moveme.txt")))),
      "and its file URLs, for other programs");
    Check(Window.Drop(false, Data.get(), QString(), false, false) && Remote->Names().contains(QStringLiteral("moveme.txt")),
      "dropped on the remote panel, it uploads there");
    Check(QFileInfo::exists(Dir.filePath(QStringLiteral("moveme.txt"))), "a copy: the local file stays");

    Local->Select({ QStringLiteral("dragged.txt") });
    Data = Drag(Local->View());
    Check(Window.Drop(false, Data.get(), QStringLiteral("sub"), true, false), "dropped on a remote directory with Shift");
    Check(!QFileInfo::exists(Dir.filePath(QStringLiteral("dragged.txt"))) && !Remote->Names().contains(QStringLiteral("dragged.txt")),
      "it moves: gone locally, not in the remote directory itself");
    Check(Remote->ChangeDirectory(QStringLiteral("/sub")) && Remote->Names().contains(QStringLiteral("dragged.txt")),
      "but in the one dropped on");
    Check(Remote->ChangeDirectory(QStringLiteral("/")), "back to the root");

    Remote->Select({ QStringLiteral("hello.txt") });
    Data = Drag(Remote->View());
    Check(ReadFileDrag(Data.get(), Read) && !Read.Local && Data->hasUrls(), "a remote drag offers file URLs too");
    Check(Window.Drop(true, Data.get(), QStringLiteral("down"), false, false), "dropped on a local directory");
    QFile Down(Dir.filePath(QStringLiteral("down/hello.txt")));
    Check(Down.open(QIODevice::ReadOnly) && (Down.readAll() == "Hello, linscp\n"), "it downloads there, whole");
    Down.close();
    Check(Remote->Names().contains(QStringLiteral("hello.txt")), "and stays remotely");
    Check(Window.DragOuts() == 0, "a drop on a panel downloads nothing for other programs");

    // To another program: the URLs it asks for, downloaded then.
    const QList<QUrl> Urls = Data->urls();
    Check(Window.DragOuts() == 1, "another program asking for the URLs downloads them");
    QFile Out((Urls.size() == 1) ? Urls.first().toLocalFile() : QString());
    Check(Out.open(QIODevice::ReadOnly) && (Out.readAll() == "Hello, linscp\n"), "into a temporary directory, whole");
    Out.close();
    Check(QFileInfo(Out.fileName()).fileName() == QStringLiteral("hello.txt"), "under the file's name");
    Check((Data->urls() == Urls) && (Window.DragOuts() == 1), "asked again, the same URLs, not downloaded again");

    Remote->Select({ QStringLiteral("moveme.txt") });
    Data = Drag(Remote->View());
    Check(Window.Drop(false, Data.get(), QStringLiteral("sub"), false, false), "within the remote panel, onto a directory");
    Check(!Remote->Names().contains(QStringLiteral("moveme.txt")), "it moves into it");
    Check(Remote->ChangeDirectory(QStringLiteral("/sub")) && Remote->Names().contains(QStringLiteral("moveme.txt")), "and is there");
    Remote->Select({ QStringLiteral("moveme.txt") });
    Data = Drag(Remote->View());
    Check(Window.Drop(false, Data.get(), QStringLiteral(".."), false, false), "onto \"..\", the parent");
    Check(Remote->ChangeDirectory(QStringLiteral("/")) && Remote->Names().contains(QStringLiteral("moveme.txt")), "back in the root");
    Check(!Window.Drop(false, Data.get(), QString(), false, false), "onto the panel itself, nothing");

    QFile::remove(Dir.filePath(QStringLiteral("moveme.txt")));
    Local->Refresh();
    Remote->Select({ QStringLiteral("moveme.txt") });
    Data = Drag(Remote->View());
    Check(Window.Drop(true, Data.get(), QString(), true, false), "a remote file dropped on the local panel with Shift");
    Check(QFileInfo::exists(Dir.filePath(QStringLiteral("moveme.txt"))) && !Remote->Names().contains(QStringLiteral("moveme.txt")),
      "moves down");

    std::unique_ptr<QMimeData> External(new QMimeData());
    External->setUrls({ QUrl::fromLocalFile(Dir.filePath(QStringLiteral("external.txt"))) });
    Check(Window.Drop(false, External.get(), QString(), false, false) && Remote->Names().contains(QStringLiteral("external.txt")),
      "files from another program upload when dropped on the remote panel");
    Check(!Window.Drop(true, External.get(), QString(), false, false), "and are not taken by the local panel");

    // Through the view, as the system delivers a drop: with the questions.
    Responder.AcceptDialogs = true;
    Local->Refresh();
    Local->Select({ QStringLiteral("local.txt") });
    Data = Drag(Local->View());
    Check(Data != nullptr, "the local panel drags its selection");
    QString DroppedOn;
    Qt::DropAction DroppedAs = Qt::IgnoreAction;
    QObject::connect(Local->View(), &FileView::Dropped, Local->View(),
      [&DroppedOn, &DroppedAs](const QMimeData *, const QString & OnDirectory, Qt::DropAction Action)
      {
        DroppedOn = OnDirectory;
        DroppedAs = Action;
      });
    DragOnto(Local->View(), Data.get(), RowCenter(Local->View(), QStringLiteral("down")), Qt::NoModifier);
    Check((DroppedOn == QStringLiteral("down")) && (DroppedAs == Qt::CopyAction), "a drop on a row names its directory");
    QElapsedTimer Timer;
    Timer.start();
    while (!QFileInfo::exists(Dir.filePath(QStringLiteral("down/local.txt"))) && (Timer.elapsed() < 10000))
    {
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    Check(QFileInfo::exists(Dir.filePath(QStringLiteral("down/local.txt"))) && !QFileInfo::exists(Dir.filePath(QStringLiteral("local.txt"))),
      "and, after asking, the file moves into it");
    Check(Responder.Accepted.contains(QStringLiteral("Move")), "the question was the move's");

    Check(QFile::remove(Dir.filePath(QStringLiteral("down/hello.txt"))), "(the earlier download removed)");
    Remote->Refresh();
    Remote->Select({ QStringLiteral("hello.txt") });
    Data = Drag(Remote->View());
    DragOnto(Local->View(), Data.get(), RowCenter(Local->View(), QStringLiteral("down")), Qt::NoModifier);
    Timer.start();
    while (!QFileInfo::exists(Dir.filePath(QStringLiteral("down/hello.txt"))) && (Timer.elapsed() < 10000))
    {
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    Check(QFileInfo::exists(Dir.filePath(QStringLiteral("down/hello.txt"))) && Responder.Accepted.contains(QStringLiteral("Copy")),
      "a remote drag dropped through the view downloads there, after the Copy dialog");
    Check(Window.DragOuts() == 1, "and not to a temporary directory");

    Local->Refresh();
    Local->Select({ QStringLiteral("moveme.txt") });
    Data = Drag(Local->View());
    Check(Data != nullptr, "and drags it again");
    DragOnto(Remote->View(), Data.get(), QPoint(5, Remote->View()->viewport()->height() - 5), Qt::ShiftModifier);
    Timer.start();
    while (!Remote->Names().contains(QStringLiteral("moveme.txt")) && (Timer.elapsed() < 10000))
    {
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    Check(Remote->Names().contains(QStringLiteral("moveme.txt")) && !QFileInfo::exists(Dir.filePath(QStringLiteral("moveme.txt"))),
      "a Shift-drop on the remote panel's free space moves up, after the Move dialog");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

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
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if ((argc >= 2) && (std::strcmp(argv[1], "dialogs") == 0))
  {
    Dialogs();
  }
  else if ((argc >= 2) && (std::strcmp(argv[1], "language") == 0))
  {
    Language();
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "sites") == 0))
  {
    Sites(argv[3], (argc > 4) ? QString::fromLocal8Bit(argv[4]) : QString(), (argc > 5) ? QString::fromLocal8Bit(argv[5]) : QString());
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "panel") == 0))
  {
    Panel(argv[3]);
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "local") == 0))
  {
    Local(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "transfer") == 0))
  {
    Transfer(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "drop") == 0))
  {
    DragAndDrop(argv[3], QString::fromLocal8Bit(argv[4]));
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
