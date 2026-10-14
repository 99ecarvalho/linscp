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
//   test_qt_gui edit /ini=nul <port> <directory> -platform offscreen
//     the internal editor (F4): a local file edited in place, its line
//     endings, BOM and encoding kept (Latin-1 falling back), find, replace
//     all, go to line, the status bar; a remote file downloaded, saved back
//     through the queue, its temporary copy gone once closed; editors
//     closed with the session, saving first
//   test_qt_gui properties /ini=nul <port> -platform offscreen
//     Files > Properties over the suite's FTP server (which keeps SITE
//     CHMOD's modes): the dialog's texts, the rights frame (octal, menu,
//     mixed bits for several files), permissions changed, a directory's
//     size calculated, recursion offered for directories; Create folder
//     (F7) with permissions, kept for next time; links (Alt+F6): the
//     dialog, local links made and edited (FTP makes none); Duplicate
//     and Move To (Shift+F5, Shift+F6) within the server, through a
//     temporary copy where the server copies nothing itself; the console
//     (FTP's protocol commands)
//   test_qt_gui bookmarks /ini=nul <port> <directory> -platform offscreen
//     Open directory and bookmarks (Ctrl+O, Ctrl+B): the site's and the
//     shared bookmarks added, ordered, removed and kept; a bookmark
//     opened; a directory that cannot be opened asked for again; the
//     local panel's too; Location Profiles (the Commander's preference):
//     profiles added, foldered, renamed, ordered, removed, opened into
//     both panels, and the switch between the two dialogs; Find files
//     (Alt+F7): found below a directory, focused, copied, downloaded,
//     deleted, a mask that is not one; Edit file mask; the panels' filter
//     (Ctrl+Alt+F)
//   test_qt_gui tools /ini=nul <port> <directory> -platform offscreen
//     the session tools: server and protocol information (its rows, the
//     host key, the space available where the server tells it, the
//     clipboard text); Generate URL/code: the session's URL with its
//     parts, a script, .NET code, files' URLs, links to WinSCP's
//     documentation, the choices kept
//   test_qt_gui commands /ini=nul <port> <key> <user> <directory> -platform offscreen
//     custom commands over OpenSSH's sshd (SCP: a shell): WinSCP's
//     defaults in the menus, the Custom command dialog's checks, commands
//     run on files and not, their parameters asked, a local !`command`,
//     their results copied, an ad hoc one kept as Last
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
#include <QClipboard>
#include <QTest>
#include <QSettings>
#include <QSplitter>
#include <QListWidget>
#include <QComboBox>
#include <QHeaderView>
#include <QMenu>
#include <QRadioButton>
#include <QDragEnterEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QIcon>
#include <QGroupBox>

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
#include "PreferencesDialog.h"
#include "SynchronizeDialog.h"
#include "TransferSettingsDialog.h"
#include "AboutDialog.h"
#include "LicenseDialog.h"
#include <QTextBrowser>
#include "PropertiesDialog.h"
#include "TagDialog.h"
#include "RightsFrame.h"
#include "CreateDirectoryDialog.h"
#include "LinkDialog.h"
#include "RemoteTransferDialog.h"
#include "OpenDirectoryDialog.h"
#include "ShortCutDialog.h"
#include "LocationProfilesDialog.h"
#include "FindFilesDialog.h"
#include "EditMaskDialog.h"
#include "ServerInfoDialog.h"
#include "GenerateUrlDialog.h"
#include "ConsoleDialog.h"
#include "SiteRawDialog.h"
#include "CustomCommandDialog.h"
#include <QMenu>
#include "ImportSitesDialog.h"
#include "CleanupDialog.h"
#include "RtfText.h"
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include "EditorWindow.h"
#include "KeepUpToDateDialog.h"
#include <QThread>

#include <QPlainTextEdit>
#include <QTextBlock>

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

void Language(const QString & Translations)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  CoreBridge::SetLanguage(std::string(), Translations.toStdString());
  const TLanguage Language = SetUpLanguage(QStringLiteral("pt_BR"));
  Check(Language.Locale == QStringLiteral("pt_BR"), "the shell follows core's locale");
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
  Check(Language.CoreCode == QStringLiteral("PT"), "core's texts are WinSCP's PT translation");
  Check(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NewSiteNode)) != QStringLiteral("New Site"),
    "which core's own strings come from");
  CoreBridge::Finalize();
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
  // Asked first about any modal dialog: true when it dealt with it.
  std::function<bool(QWidget *)> Custom;

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
    if ((Modal != nullptr) && Custom && Custom(Modal))
    {
      return;
    }
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
      if (Box->button(QMessageBox::Yes) != nullptr)
      {
        Box->button(QMessageBox::Yes)->click();
      }
      else
      {
        Box->accept();
      }
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
      // "Remember last used directory", on by default: kept when the
      // session ends, and where the site opens next.
      const QString Remembered = QDir(Directory).filePath(QStringLiteral("remembered"));
      QDir().mkpath(Remembered);
      Check(CoreBridge::LoadSite("Tests/rclone").UpdateDirectories, "a site remembers its directories by default");
      Check(Window.Panel()->ChangeDirectory(QStringLiteral("/sub")) && Window.Local()->ChangeDirectory(Remembered),
        "the panels go elsewhere");
      QMetaObject::invokeMethod(&Window, "Disconnect");
      CoreBridge::TSite Site = CoreBridge::LoadSite("Tests/rclone");
      Check(Site.RemoteDirectory == "/sub", "disconnecting keeps the remote directory in the site");
      Check(QFileInfo(QString::fromStdString(Site.LocalDirectory)) == QFileInfo(Remembered), "and the local one");
      Check(Window.OpenSite(Site), "the site opens again");
      Check((Window.Panel()->Path() == QStringLiteral("/sub")) && (QFileInfo(Window.Local()->Path()) == QFileInfo(Remembered)),
        "in the directories it was left in");
      QMetaObject::invokeMethod(&Window, "Disconnect");
      // Out of it first: on Windows, a directory being watched cannot go.
      Window.Local()->ChangeDirectory(QDir::homePath());
      Check(QDir(Remembered).removeRecursively() && !QFileInfo::exists(Remembered), "(the local directory removed)");
      const int Errors = Responder.Errors;
      Check(Window.OpenSite(CoreBridge::LoadSite("Tests/rclone")), "with the local one gone");
      Check((QFileInfo(Window.Local()->Path()) == QFileInfo(Directory)) && (Responder.Errors == Errors),
        "it opens the nearest that is there, quietly");
      QMetaObject::invokeMethod(&Window, "Disconnect");

      Site = CoreBridge::LoadSite("Tests/rclone");
      Site.UpdateDirectories = false;
      CoreBridge::SaveSite(Site);
      Check(!CoreBridge::LoadSite("Tests/rclone").UpdateDirectories, "the option turns off and saves");
      Check(Window.OpenSite(CoreBridge::LoadSite("Tests/rclone")) && Window.Panel()->ChangeDirectory(QStringLiteral("/")),
        "a session of it goes elsewhere");
      QMetaObject::invokeMethod(&Window, "Disconnect");
      Check(CoreBridge::LoadSite("Tests/rclone").RemoteDirectory == "/sub", "and, off, nothing is kept");
      // "Default directory is home" (on by default): Home is the site's own
      // directory when it does not remember the last used, as WinSCP's.
      Check(Window.OpenSite(CoreBridge::LoadSite("Tests/rclone")) && Window.Panel()->ChangeDirectory(QStringLiteral("/")) &&
        Window.Panel()->Home() && (Window.Panel()->Path() == QStringLiteral("/sub")), "Home is the site's directory");
      CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
      Preferences.DefaultDirIsHome = false;
      CoreBridge::SetPreferences(Preferences);
      Window.ApplyPreferences();
      Check(Window.Panel()->Home() && (Window.Panel()->Path() == QStringLiteral("/")), "without it, the server's home");
      Preferences.DefaultDirIsHome = true;
      CoreBridge::SetPreferences(Preferences);
      Window.ApplyPreferences();
      QMetaObject::invokeMethod(&Window, "Disconnect");
      SiteAdvanced Advanced(CoreBridge::LoadSite("Tests/rclone"), &Window);
      Check(!Advanced.UpdateDirectories()->isChecked(), "the advanced settings show it");
    }
    // Edit Raw Settings: the site's settings as core's options.
    {
      SiteManager Dialog(&Window);
      Dialog.SelectSite(QStringLiteral("Tests/rclone"));
      const CoreBridge::TSite Site = Dialog.Site();
      const QString Raw = QString::fromStdString(CoreBridge::SiteRawSettings(Site, Site.Name));
      Check(!Raw.contains(QStringLiteral("HostName=")) && !Raw.contains(QStringLiteral("UserName=")),
        "the host and user are the form's, not raw settings");
      {
        SiteRawDialog Raws(Raw);
        Check(!Raws.AddSetting(QStringLiteral("HostName")), "the form's fields are not offered");
        QString Default;
        for (const auto & Option : CoreBridge::RawSettingsToAdd(Raw.toStdString()))
        {
          if (Option.first == "PingIntervalSecs")
          {
            Default = QString::fromStdString(Option.second);
          }
        }
        Check(!Default.isEmpty() && Raws.AddSetting(QStringLiteral("PingIntervalSecs")) &&
          (Raws.Editor()->textCursor().selectedText() == Default) &&
          Raws.Settings().endsWith(QStringLiteral("PingIntervalSecs=") + Default), "an option added with its default, selected");
        Check(!Raws.AddSetting(QStringLiteral("PingIntervalSecs")), "but not twice");
      }
      const QString Edited = Raw.trimmed() + QStringLiteral("\nTimeout=33\nPingIntervalSecs=21\n");
      Check(Dialog.RawSettings(&Edited) && (Dialog.Site().Timeout == 33) && Dialog.Site().HasRawSettings,
        "raw settings applied to the form's site");
      Check(Dialog.HostName()->text() == QStringLiteral("127.0.0.1"), "its host as it was");
      Dialog.Save();
      Check(CoreBridge::LoadSite("Tests/rclone").Timeout == 33, "saved with the site");
      const QString Again = QString::fromStdString(CoreBridge::SiteRawSettings(CoreBridge::LoadSite("Tests/rclone"), "Tests/rclone"));
      Check(Again.contains(QStringLiteral("PingIntervalSecs=21")), "an option the form has no field for too");
    }
    // Import sites: from an INI file, and known_hosts' host keys.
    {
      const QString Ini = QDir(Directory).filePath(QStringLiteral("import.ini"));
      {
        QFile File(Ini);
        File.open(QIODevice::WriteOnly);
        File.write("[Sessions\\Imported%20site]\nHostName=import.example\nUserName=bob\n");
      }
      ImportSitesDialog Import;
      Check(Import.SourceCombo()->count() == 6, "WinSCP's six sources");
      Import.SourceCombo()->setCurrentIndex(CoreBridge::TImportSites::Kitty);
      Check((Import.List()->count() > 0) || !Import.ErrorLabel()->isHidden(), "a source with nothing says why");
      Import.SourceCombo()->setCurrentIndex(CoreBridge::TImportSites::Ini);
      Import.LoadIni(Ini);
      Check((Import.List()->count() == 1) && (Import.List()->item(0)->text() == QStringLiteral("Imported site")),
        "the INI file's site");
      // As core selects them: a site not stored yet, checked.
      Check((Import.List()->item(0)->checkState() == Qt::Checked) && Import.OkButton()->isEnabled(), "new: checked");
      Import.List()->item(0)->setCheckState(Qt::Unchecked);
      Check(!Import.OkButton()->isEnabled(), "none checked: nothing to import");
      Import.List()->item(0)->setCheckState(Qt::Checked);
      Import.OkButton()->click();
      Check((Import.Imported().size() == 1) && CoreBridge::SiteExists(Import.Imported().front()) &&
        (CoreBridge::LoadSite(Import.Imported().front()).HostName == "import.example"), "imported");
      CoreBridge::DeleteSite(Import.Imported().front());
    }
    if (!Key.isEmpty())
    {
      QFile Public(Key + QStringLiteral(".pub"));
      if (Public.open(QIODevice::ReadOnly))
      {
        const QString Line = QStringLiteral("knownhost.example ") + QString::fromUtf8(Public.readAll()).trimmed();
        ImportSitesDialog Import;
        Import.SourceCombo()->setCurrentIndex(CoreBridge::TImportSites::KnownHosts);
        Import.LoadKnownHosts(Line);
        Check((Import.List()->count() == 1) && Import.List()->item(0)->text().contains(QStringLiteral("knownhost.example")),
          "a known_hosts line pasted");
        Check(Import.List()->item(0)->toolTip().contains(QStringLiteral("ssh-ed25519")), "its host key's algorithm");
      }
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
    {
      // Tools > Clean Up: the INI file the sites are in, listed and removed.
      const QString Ini = QDir(Directory).filePath(QStringLiteral("WinSCP.ini"));
      CleanupDialog Dialog(&Window);
      int IniItem = -1;
      for (int Index = 0; Index < Dialog.List()->topLevelItemCount(); Index++)
      {
        if (QDir::fromNativeSeparators(Dialog.List()->topLevelItem(Index)->text(1)) == QDir::fromNativeSeparators(Ini))
        {
          IniItem = Index;
        }
      }
      Check(Dialog.List()->topLevelItem(0)->text(0) == QStringLiteral("General configuration"), "the configuration listed first, as WinSCP");
      Check(IniItem > 0, "the INI file, where it is");
      Check(!Dialog.OkButton()->isEnabled(), "OK only once something is checked");
      Dialog.CheckAllButton()->click();
      Check(Dialog.OkButton()->isEnabled(), "Un/check all checks all");
      Dialog.CheckAllButton()->click();
      Check(!Dialog.OkButton()->isEnabled(), "and again unchecks them");
      if (IniItem > 0)
      {
        Dialog.List()->topLevelItem(IniItem)->setCheckState(0, Qt::Checked);
      }
      QTimer::singleShot(0, &Dialog, [&Dialog]() { Dialog.OkButton()->click(); });
      Check(Dialog.Execute() && !QFile::exists(Ini), "OK removes what is checked");
    }
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// F2 pressed in a panel's view, the new name typed into the editor in place,
// Enter; then waits for the panel to list it.
bool RenameInPlace(QWidget * Window, FileView * View, const QString & NewName, const std::function<bool()> & Done)
{
  QTest::keyClick(Window->windowHandle(), Qt::Key_F2);
  QLineEdit * Editor = View->viewport()->findChild<QLineEdit *>();
  if (Editor == nullptr)
  {
    return false;
  }
  Editor->selectAll();
  QTest::keyClicks(Editor, NewName);
  QTest::keyClick(Window->windowHandle(), Qt::Key_Return);
  QElapsedTimer Timer;
  Timer.start();
  while (!Done() && (Timer.elapsed() < 10000))
  {
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  return Done();
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
    Check(Window.windowTitle().startsWith(QStringLiteral("/ - ")), "the title shows the path (short: the root as is)");
    Check(Panel->ChangeDirectory(QStringLiteral("/sub")) && Window.windowTitle().startsWith(QStringLiteral("sub - ")),
      "and a directory by its last part");
    Check(Panel->ChangeDirectory(QStringLiteral("/")), "(back to the root)");
    Check(Panel->Names() == QStringList({ QStringLiteral("sub"), QStringLiteral("hello.txt") }),
      "directories first, no \"..\" at the root");
    Panel->View()->sortByColumn(FileListModel::SizeColumn, Qt::DescendingOrder);
    Check(Panel->Names().first() == QStringLiteral("sub"), "directories first sorted the other way too");
    Panel->View()->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);

    Check(Panel->NewFolder(QStringLiteral("made")) && Panel->Names().contains(QStringLiteral("made")), "a new folder appears");
    Check(Panel->Selected() == QStringList({ QStringLiteral("made") }), "selected");
    Check(Panel->Rename(QStringLiteral("made"), QStringLiteral("renamed")) && Panel->Names().contains(QStringLiteral("renamed")),
      "a rename shows");
    QApplication::setActiveWindow(&Window);
    Check(QTest::qWaitForWindowActive(&Window), "the window is active");
    Panel->Select({ QStringLiteral("renamed") });
    Panel->View()->setFocus();
    Check(RenameInPlace(&Window, Panel->View(), QStringLiteral("inplace"),
      [Panel]() { return Panel->Names().contains(QStringLiteral("inplace")); }), "F2 renames in place, remotely");
    Check(!Panel->Names().contains(QStringLiteral("renamed")), "the old name gone");
    Check(Panel->Rename(QStringLiteral("inplace"), QStringLiteral("renamed")), "(and back)");
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

void Synchronization(const char * Port, const QString & Directory)
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
    using TSyncParams = CoreBridge::TSyncParams;

    // The dialog's rules, as TFullSynchronizeDialog::UpdateControls.
    {
      SynchronizeDialog Dialog(Directory, QStringLiteral("/sub"), TSyncParams(), true);
      Check(Dialog.RemoteButton()->isChecked() && Dialog.FilesButton()->isChecked() && Dialog.PreviewCheck()->isChecked() &&
        Dialog.ByTimeCheck()->isChecked() && !Dialog.DeleteCheck()->isChecked(), "the defaults: remote, files, preview, by time");
      Dialog.MirrorButton()->setChecked(true);
      Dialog.BothButton()->setChecked(true);
      Check(Dialog.FilesButton()->isChecked() && !Dialog.MirrorButton()->isEnabled(), "both directions: no mirror");
      Check(!Dialog.DeleteCheck()->isEnabled() && !Dialog.ByTimeCheck()->isEnabled() && Dialog.ByTimeCheck()->isChecked() &&
        !Dialog.BySizeCheck()->isEnabled(), "nor delete, and by time only");
      Dialog.RemoteButton()->setChecked(true);
      Dialog.DeleteCheck()->setChecked(true);
      Dialog.TimestampsButton()->setChecked(true);
      Check(Dialog.ExistingOnlyCheck()->isChecked() && !Dialog.ExistingOnlyCheck()->isEnabled() &&
        !Dialog.DeleteCheck()->isChecked() && !Dialog.DeleteCheck()->isEnabled(), "timestamps: existing files only, no delete");
      Check(Dialog.BySizeCheck()->text().contains(QStringLiteral("Same")), "and \"same size only\"");
      Check(Dialog.Params().Mode == TSyncParams::TMode::Timestamps, "which the params say");
      Dialog.FilesButton()->setChecked(true);
      Dialog.LocalButton()->setChecked(true);
      Check(Dialog.Params().Direction == TSyncParams::TDirection::Local, "the direction too");
      SynchronizeDialog NoTimes(Directory, QString(), TSyncParams(), false);
      Check(!NoTimes.TimestampsButton()->isEnabled(), "a server that cannot set times: no timestamps");
      Check(!NoTimes.OkButton()->isEnabled(), "and no OK without both directories");
    }

    const QDir Dir(Directory);
    for (const char * Name : { "s1.txt", "s2.txt" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    LocalPanel * Local = Window.Local();
    RemotePanel * Remote = Window.Panel();
    Local->ChangeDirectory(Directory);
    Remote->ChangeDirectory(QStringLiteral("/sub"));

    // The checklist, over what the session collects.
    {
      std::unique_ptr<CoreBridge::TSyncChecklist> List =
        Window.Session()->SyncCollect(Directory.toStdString(), "/sub", TSyncParams());
      Check(List && (List->Items().size() == 2), "two files to upload");
      SyncChecklistDialog Dialog(*List, Directory, QStringLiteral("/sub"));
      Check(Dialog.List()->topLevelItemCount() == 2, "the checklist lists them");
      Check(Dialog.Status()->text().contains(QStringLiteral("2 of 2")), "counting them, checked of all");
      Dialog.List()->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
      Check(List->CheckedCount() == 1, "an entry unchecked in the list is unchecked in the checklist");
      Dialog.CheckAll(false);
      Check(!Dialog.StartButton()->isEnabled() && (List->CheckedCount() == 0), "none checked: nothing to start");
      Dialog.CheckAll(true);
      Check(Dialog.StartButton()->isEnabled() && (List->CheckedCount() == 2), "all checked again");
      Dialog.List()->topLevelItem(1)->setSelected(true);
      Dialog.ReverseSelected();
      Check(List->Items()[1].Action == CoreBridge::TSyncItem::TAction::DeleteLocal, "a new upload reversed is a local delete");
      Check(Dialog.List()->topLevelItem(1)->toolTip(4) == SyncChecklistDialog::tr("Delete local"), "shown so");
    }

    // Commands > Synchronize, from the local panel: the remote directory updated.
    Responder.AcceptDialogs = true;
    int Checklists = 0;
    Responder.Custom = [&Checklists](QWidget * Modal)
    {
      if (SyncChecklistDialog * Dialog = qobject_cast<SyncChecklistDialog *>(Modal))
      {
        Checklists++;
        Dialog->StartButton()->click();
        return true;
      }
      return false;
    };
    Window.ActivatePanel(true);
    Check(Window.Synchronize(true), "synchronizing applies");
    Check(Responder.Accepted.contains(SynchronizeDialog::tr("Synchronize")) && (Checklists == 1),
      "after the Synchronize dialog and the checklist");
    Check(Remote->Names().contains(QStringLiteral("s1.txt")) && Remote->Names().contains(QStringLiteral("s2.txt")),
      "and the remote panel shows the files");
    Responder.Accepted.clear();
    Check(!Window.Synchronize(true), "again, nothing to do");
    Check(Responder.Accepted.count(MainWindow::tr("Synchronize")) == 2, "\"no differences\" is told");

    {
      QFile File(Dir.filePath(QStringLiteral("s3.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("queued");
    }
    Responder.Custom = [](QWidget * Modal)
    {
      if (SyncChecklistDialog * Dialog = qobject_cast<SyncChecklistDialog *>(Modal))
      {
        Dialog->QueueButton()->click();
        return true;
      }
      return false;
    };
    Check(Window.Synchronize(true), "started in the background");
    QElapsedTimer Timer;
    Timer.start();
    while (!Remote->Names().contains(QStringLiteral("s3.txt")) && (Timer.elapsed() < 30000))
    {
      QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    Check(Remote->Names().contains(QStringLiteral("s3.txt")), "the queue uploads it, and the panel shows it");
    Responder.Custom = nullptr;
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// The Commander's keys, pressed as a user presses them.
void Keys(const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    QApplication::setActiveWindow(&Window);
    Check(QTest::qWaitForWindowActive(&Window), "the window is active");
    TResponder Responder;
    const QDir Dir(Directory);
    for (const char * Name : { "a.txt", "b.log", "c.txt", "gone.txt", "kept.txt" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    Dir.mkpath(QStringLiteral("d.txt"));
    Dir.mkpath(QStringLiteral("sub"));
#ifndef _WIN32
    {
      QFile File(Dir.filePath(QStringLiteral(".hidden")));
      File.open(QIODevice::WriteOnly);
    }
#endif
    LocalPanel * Local = Window.Local();
    Local->DeleteToTrash = false;
    Local->ChangeDirectory(Directory);
    FileView * View = Local->View();

    // The status line, as WinSCP's, with its sizes as WinSCP formats them.
    QLocale::setDefault(QLocale::c());
    Check(FormatBytes(512) == QStringLiteral("512 B"), "bytes below 1 KB");
    Check(FormatBytes(1536) == QStringLiteral("1.50 KB"), "then KB, to three digits");
    Check(FormatBytes(12 * 1024 + 128) == QStringLiteral("12.1 KB"), "12 KB + 128 B is 12.1 KB, as WinSCP's table");
    Check(FormatBytes(1000 * 1024) == QStringLiteral("0.97 MB"), "1000 KB is 0.97 MB");
    Check(FormatBytes(1024LL * 1024 * 1024 + 12 * 1024 * 1024) == QStringLiteral("1.01 GB"), "1 GB + 12 MB is 1.01 GB");
    Local->Select({ QStringLiteral("a.txt"), QStringLiteral("b.log") });
    Check(Local->Status()->text().startsWith(QStringLiteral("10 B of 31 B in 2 of 7")), "the status line counts the selection");
#ifndef _WIN32
    Check(Local->Status()->text().contains(QStringLiteral("1 hidden")), "and the hidden");
    Local->SetShowHidden(true);
    Check(Local->Status()->text().startsWith(QStringLiteral("10 B of 31 B in 2 of 8")) &&
      !Local->Status()->text().contains(QStringLiteral("hidden")), "which, shown, count");
    Local->SetShowHidden(false);
#endif
    View->clearSelection();
    Window.ActivatePanel(true);
    const auto Sorted = [Local]() { QStringList S = Local->Selected(); S.sort(); return S; };
    // After a dialog closes, the offscreen platform leaves no window active
    // (a window manager would give it back).
    const auto Activate = [&Window]()
    {
      QApplication::setActiveWindow(&Window);
      Check(QTest::qWaitForWindowActive(&Window), "the window is active again");
      Window.ActivatePanel(true);
    };

    // Insert: the focused entry toggled, the focus down.
    View->setCurrentIndex(View->model()->index(0, 0));
    View->clearSelection();
    QTest::keyClick(Window.windowHandle(), Qt::Key_Insert);
    Check(Local->Selected().isEmpty(), "Insert on \"..\" selects nothing");
    Check(View->currentIndex().row() == 1, "but moves the focus down");
    QTest::keyClick(Window.windowHandle(), Qt::Key_Insert);
    QTest::keyClick(Window.windowHandle(), Qt::Key_Insert);
    Check(Local->Selected().size() == 2, "Insert selects the focused entry and moves on");
    View->selectionModel()->setCurrentIndex(View->model()->index(1, 0), QItemSelectionModel::NoUpdate);
    QTest::keyClick(Window.windowHandle(), Qt::Key_Insert);
    Check(Local->Selected().size() == 1, "and unselects a selected one");

    // The keypad: + and - by mask, * inverts.
    View->clearSelection();
    Check(View->SelectByMask(true, QStringLiteral("*.txt"), false), "a mask selects");
    Check(Sorted() == QStringList({ QStringLiteral("a.txt"), QStringLiteral("c.txt"), QStringLiteral("gone.txt"),
      QStringLiteral("kept.txt") }), "files matching it, not a directory named so");
    Check(View->SelectByMask(true, QStringLiteral("*.txt"), true) && Local->Selected().contains(QStringLiteral("d.txt")),
      "applied to directories, that one too");
    Check(View->SelectByMask(false, QStringLiteral("a*"), false) && !Local->Selected().contains(QStringLiteral("a.txt")),
      "a mask unselects");
    View->clearSelection();
    Check(View->SelectByMask(true, QStringLiteral("*.txt | gone*"), false) && !Local->Selected().contains(QStringLiteral("gone.txt")) &&
      Local->Selected().contains(QStringLiteral("a.txt")), "in core's mask syntax, with exclusions");
    View->clearSelection();
    View->SelectByMask(true, QStringLiteral("*.log"), false);
    QTest::keyClick(Window.windowHandle(), Qt::Key_Asterisk, Qt::KeypadModifier);
    Check(!Local->Selected().contains(QStringLiteral("b.log")) && (Local->Selected().size() == Local->Names().size() - 2),
      "keypad * inverts the selection, never \"..\"");
    int MaskDialogs = 0;
    Responder.Custom = [&MaskDialogs](QWidget * Modal)
    {
      QDialog * Dialog = qobject_cast<QDialog *>(Modal);
      if ((Dialog == nullptr) || (Dialog->windowTitle() != FileView::tr("Deselect")))
      {
        return false;
      }
      MaskDialogs++;
      Dialog->findChild<QLineEdit *>()->setText(QStringLiteral("*.txt"));
      Dialog->accept();
      return true;
    };
    QTest::keyClick(Window.windowHandle(), Qt::Key_Minus, Qt::KeypadModifier);
    Check((MaskDialogs == 1) && (Sorted() == QStringList({ QStringLiteral("d.txt"), QStringLiteral("sub") })),
      "keypad - asks for a mask and unselects by it");
    Responder.Custom = nullptr;
    Activate();
    QTest::keyClick(Window.windowHandle(), Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
    Check(Local->Selected().isEmpty(), "Ctrl+Shift+L clears the selection");
    QTest::keyClick(Window.windowHandle(), Qt::Key_A, Qt::ControlModifier);
    Check(Local->Selected().size() == Local->Names().size() - 1, "Ctrl+A selects all");
    View->clearSelection();

    // Navigation.
    Local->Open(QStringLiteral("sub"));
    QTest::keyClick(Window.windowHandle(), Qt::Key_Up, Qt::AltModifier);
    Check(QFileInfo(Local->Path()) == QFileInfo(Directory), "Alt+Up: the parent");
    Local->Open(QStringLiteral("sub"));
    QTest::keyClick(Window.windowHandle(), Qt::Key_PageUp, Qt::ControlModifier);
    Check(QFileInfo(Local->Path()) == QFileInfo(Directory), "Ctrl+PgUp: the parent");
    QTest::keyClick(Window.windowHandle(), Qt::Key_Left, Qt::AltModifier);
    Check(QFileInfo(Local->Path()) == QFileInfo(Dir.filePath(QStringLiteral("sub"))), "Alt+Left: back");
    QTest::keyClick(Window.windowHandle(), Qt::Key_Right, Qt::AltModifier);
    Check(QFileInfo(Local->Path()) == QFileInfo(Directory), "Alt+Right: forward");

    // F8 deletes, Shift+Del the other way.
    Responder.IgnoreWarnings = false;
    Local->Select({ QStringLiteral("gone.txt") });
    QTest::keyClick(Window.windowHandle(), Qt::Key_F8);
    Check(!Dir.exists(QStringLiteral("gone.txt")), "F8 deletes, after core's confirmation");
    Activate();
    Local->DeleteToTrash = true;
    Local->Select({ QStringLiteral("kept.txt") });
    QTest::keyClick(Window.windowHandle(), Qt::Key_Delete, Qt::ShiftModifier);
    Check(!Dir.exists(QStringLiteral("kept.txt")) && Responder.LastQuery.contains(QStringLiteral("kept.txt")) &&
      (Responder.LastQuery == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ConfirmDeleteFile)).replace(QStringLiteral("%s"), QStringLiteral("kept.txt"))),
      "Shift+Del: for good, not to the trash, asking so");
    Activate();

    Local->Select({ QStringLiteral("a.txt") });
    Check(RenameInPlace(&Window, View, QStringLiteral("z.txt"), [&Dir]() { return Dir.exists(QStringLiteral("z.txt")); }),
      "F2 renames in place");
    Check(!Dir.exists(QStringLiteral("a.txt")) && Local->Names().contains(QStringLiteral("z.txt")), "on disk and in the panel");
    Local->Select({ QStringLiteral("b.log") });
    QTest::keyClick(Window.windowHandle(), Qt::Key_F2);
    QTest::keyClick(Window.windowHandle(), Qt::Key_Escape);
    QApplication::processEvents();
    Check(Dir.exists(QStringLiteral("b.log")) && (View->viewport()->findChild<QLineEdit *>() == nullptr ||
      !View->viewport()->findChild<QLineEdit *>()->isVisible()), "Escape leaves the name");
    Activate();
    // Ctrl+F6: by size; again, the other way; Ctrl+F3: by name.
    QTest::keyClick(Window.windowHandle(), Qt::Key_F6, Qt::ControlModifier);
    Check((View->header()->sortIndicatorSection() == FileListModel::SizeColumn) &&
      (View->header()->sortIndicatorOrder() == Qt::AscendingOrder), "Ctrl+F6 sorts by size");
    QTest::keyClick(Window.windowHandle(), Qt::Key_F6, Qt::ControlModifier);
    Check(View->header()->sortIndicatorOrder() == Qt::DescendingOrder, "again, the other way");
    QTest::keyClick(Window.windowHandle(), Qt::Key_F3, Qt::ControlModifier);
    Check((View->header()->sortIndicatorSection() == FileListModel::NameColumn) &&
      (View->header()->sortIndicatorOrder() == Qt::AscendingOrder), "Ctrl+F3 by name");

    // Columns: group and link target hidden until shown from the header.
    Check(View->isColumnHidden(FileListModel::GroupColumn) && View->isColumnHidden(FileListModel::LinkTargetColumn) &&
      !View->isColumnHidden(FileListModel::SizeColumn), "group and link target start hidden");
    {
      std::unique_ptr<QMenu> Menu(View->ColumnsMenu());
      const QList<QAction *> Actions = Menu->actions();
      Check(Actions.size() == FileListModel::ColumnCount - 1, "the header's menu lists every column but the name");
      for (QAction * Action : Actions)
      {
        if (Action->text() == FileListModel::tr("Group"))
        {
          Action->setChecked(true);
        }
        else if (Action->text() == FileListModel::tr("Size"))
        {
          Action->setChecked(false);
        }
      }
      Check(!View->isColumnHidden(FileListModel::GroupColumn) && View->isColumnHidden(FileListModel::SizeColumn),
        "and shows and hides them");
    }
    View->setColumnHidden(FileListModel::SizeColumn, false);

    // Tab: to the other panel and back; Ctrl+[: the local path copied.
    Window.ActivatePanel(true);
    QTest::keyClick(Window.windowHandle(), Qt::Key_Tab);
    Check(!Window.LocalActive() && Window.Panel()->View()->hasFocus(), "Tab goes to the remote panel");
    QTest::keyClick(Window.windowHandle(), Qt::Key_Tab);
    Check(Window.LocalActive() && View->hasFocus(), "and back");
    QTest::keyClick(Window.windowHandle(), Qt::Key_BracketLeft, Qt::ControlModifier);
    Check(QApplication::clipboard()->text() == QDir::toNativeSeparators(Local->Path()), "Ctrl+[ copies the local path");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// Options > Preferences, over upstream's configuration (ADR 0014).
void Preferences(const QString & Ini, const QString & Translations, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  CoreBridge::SetLanguage(std::string(), Translations.toStdString());
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    using TPreferences = CoreBridge::TPreferences;
    const TPreferences Defaults = CoreBridge::Preferences();
    Check(!Defaults.ShowHiddenFiles && (Defaults.SizeFormat == TPreferences::TSizeFormat::Kilobytes) &&
      Defaults.NaturalOrderNumericalSorting && !Defaults.AlwaysSortDirectoriesByName && !Defaults.RenameWholeName &&
      Defaults.DefaultDirIsHome && !Defaults.SwappedPanels && !Defaults.ExplorerKeyboardShortcuts,
      "the defaults are WinSCP's");

    const QDir Dir(Directory);
    for (const char * Name : { "file2.txt", "file10.txt", "a.bin", ".hidden" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(QByteArray(1500, 'x'));
    }
    Dir.mkpath(QStringLiteral("zdir"));
    Dir.mkpath(QStringLiteral("adir"));
#if defined(Q_OS_WIN)
    // On Windows a file is hidden by its attribute, not its dot.
    SetFileAttributesW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(Dir.filePath(QStringLiteral(".hidden"))).utf16()),
      FILE_ATTRIBUTE_HIDDEN);
#endif
    LocalPanel * Local = Window.Local();
    Local->ChangeDirectory(Directory);
    QLocale::setDefault(QLocale::c());
    Check(Local->Names() == QStringList({ QStringLiteral(".."), QStringLiteral("adir"), QStringLiteral("zdir"),
      QStringLiteral("a.bin"), QStringLiteral("file2.txt"), QStringLiteral("file10.txt") }),
      "natural order: file2 before file10; hidden files not shown");
    const QModelIndex First = Local->View()->model()->index(3, FileListModel::SizeColumn);
    Check(First.data().toString() == QStringLiteral("2 KB"), "sizes in kilobytes, rounded up");

    // The dialog, as WinSCP's pages.
    {
      PreferencesDialog Dialog(&Window);
      QStringList Pages;
      for (int Row = 0; Row < Dialog.Navigation()->count(); Row++)
      {
        Pages.append(Dialog.Navigation()->item(Row)->text());
      }
      Check(Pages == QStringList({ PreferencesDialog::tr("Window"), PreferencesDialog::tr("Commander"),
        PreferencesDialog::tr("Languages"), PreferencesDialog::tr("Panels"), PreferencesDialog::tr("File colors"),
        PreferencesDialog::tr("Editors"), PreferencesDialog::tr("Internal editor"), PreferencesDialog::tr("Transfer"), PreferencesDialog::tr("Background"), PreferencesDialog::tr("Endurance"),
        PreferencesDialog::tr("Network"), PreferencesDialog::tr("Security"), PreferencesDialog::tr("Logging"),
        PreferencesDialog::tr("Applications"), PreferencesDialog::tr("Commands"), PreferencesDialog::tr("Storage") }),
        "the dialog's pages, in the order of WinSCP's tree");
      Check(!Dialog.LoggingCheck()->isChecked() && !Dialog.LogFileEdit()->text().isEmpty() && !Dialog.LogFileEdit()->isEnabled(),
        "logging off, its default file shown, not editable until on");
      Check(!Dialog.StorageLabel()->text().isEmpty(), "the storage shown");
      Check(Dialog.RememberPasswordCheck()->isChecked(), "passwords remembered for the session by default");
      Check(Dialog.PathShortButton()->isChecked() && Dialog.KeepOpenCheck()->isChecked() && Dialog.FileColors().empty(),
        "the Window and File colors pages show WinSCP's defaults");
      Check(!Dialog.ShowHiddenFilesCheck()->isChecked() && (Dialog.SizeFormatCombo()->currentIndex() == 1) &&
        Dialog.NaturalSortingCheck()->isChecked(), "showing the settings");
      bool Portuguese = false;
      for (int Row = 0; Row < Dialog.LanguagesList()->count(); Row++)
      {
        Portuguese = Portuguese || Dialog.LanguagesList()->item(Row)->text().contains(QStringLiteral("Brazil"));
      }
      Check(Dialog.LanguagesList()->count() > 40, "the languages WinSCP is translated into");
      Check(Portuguese, "Brazilian Portuguese among them");
      Check(Dialog.LanguagesList()->currentItem() != nullptr, "the one in use chosen");
      Dialog.Navigation()->setCurrentRow(2);
      Check(Dialog.LanguageChangeLabel()->isHidden(), "no change to apply");
      for (int Row = 0; Row < Dialog.LanguagesList()->count(); Row++)
      {
        if (Dialog.LanguagesList()->item(Row)->text().contains(QStringLiteral("Brazil")))
        {
          Dialog.LanguagesList()->setCurrentRow(Row);
        }
      }
      Check(!Dialog.LanguageChangeLabel()->isHidden(), "another language applies on the next start");
      Dialog.ShowHiddenFilesCheck()->setChecked(true);
      Dialog.SizeFormatCombo()->setCurrentIndex(0);
      Dialog.NaturalSortingCheck()->setChecked(false);
      Dialog.DirectoriesByNameCheck()->setChecked(true);
      Dialog.SwappedPanelsCheck()->setChecked(true);
      Dialog.KeyboardShortcutsCombo()->setCurrentIndex(1);
      Dialog.RenameWholeNameCheck()->setChecked(true);

      // Transfer: the default first, then the presets: WinSCP's own to begin
      // with (TGUIConfiguration::DefaultLocalized: text, binary, changed
      // files only, no directories).
      const int Initial = static_cast<int>(Dialog.Presets().size());
      Check(Initial == 4, "WinSCP's four presets to begin with");
      Check((Dialog.PresetsList()->count() == Initial + 1) &&
        (Dialog.PresetsList()->item(0)->text() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CopyParamDefault)).remove(QLatin1Char('&'))),
        "the presets list has the default");
      CoreBridge::TTransferSettings Text = Dialog.DefaultSettings();
      Text.Mode = CoreBridge::TTransferSettings::TMode::Text;
      Dialog.PresetsList()->setCurrentRow(0);
      Check(Dialog.AddPreset(QStringLiteral("Mine"), Text) && Dialog.AddPreset(QStringLiteral("Other"), Text),
        "presets are added");
      Check((Dialog.Presets()[0].Name == "Mine") && (Dialog.Presets()[1].Name == "Other"),
        "each after the one selected");
      Responder.AcceptDialogs = true;
      Check(!Dialog.AddPreset(QStringLiteral("mine"), Text), "not twice under one name");
      Responder.AcceptDialogs = false;
      Dialog.MoveSelected(-1);
      Check((Dialog.Presets().size() == static_cast<std::size_t>(Initial + 2)) && (Dialog.Presets()[0].Name == "Other"),
        "and move");
      Dialog.PresetsList()->setCurrentRow(0);
      CoreBridge::TTransferSettings Default = Dialog.DefaultSettings();
      Default.NameCase = CoreBridge::TTransferSettings::TNameCase::Lower;
      Check(Dialog.EditSelected(QString(), Default), "the default edits");
      Dialog.PresetsList()->setCurrentRow(1);
      Dialog.RemoveSelected();
      Check((Dialog.Presets().size() == static_cast<std::size_t>(Initial + 1)) && (Dialog.Presets()[0].Name == "Mine"),
        "a preset removes");
      // Background and Endurance.
      Dialog.QueueTransfersLimitSpin()->setValue(3);
      Dialog.QueueKeepDoneCombo()->setCurrentIndex(2);
      Dialog.QueueCheck()->setChecked(true);
      Dialog.ResumeOffButton()->setChecked(true);
      Check(!Dialog.ResumeThresholdSpin()->isEnabled(), "a threshold only for \"files above\"");
      Dialog.ReopenAutoCheck()->setChecked(false);
      Check(!Dialog.ReopenAutoSpin()->isEnabled(), "a delay only when reconnecting");
      // File colors: the first matching rule colors a file.
      CoreBridge::TPreferences::TFileColor Red;
      Red.Mask = "*.txt";
      Red.Color = 0xff0000;
      CoreBridge::TPreferences::TFileColor Blue;
      Blue.Mask = "file*";
      Blue.Color = 0x0000ff;
      Dialog.AddFileColor(Blue);
      Dialog.AddFileColor(Red);
      Dialog.FileColorsList()->setCurrentRow(1);
      Dialog.MoveFileColor(-1);
      Check((Dialog.FileColors().size() == 2) && (Dialog.FileColors()[0].Mask == "*.txt"), "file colors add and move");
      Dialog.PathFullButton()->setChecked(true);
      // Logging, Network, Security.
      Dialog.LoggingCheck()->setChecked(true);
      Check(Dialog.LogFileEdit()->isEnabled(), "logging on, its file editable");
      Dialog.LogLevelCombo()->setCurrentIndex(2);
      Dialog.LogMaxSizeCheck()->setChecked(true);
      Dialog.LogMaxSizeCombo()->setEditText(QStringLiteral("10M"));
      Dialog.CustomIpButton()->setChecked(true);
      Dialog.CustomIpEdit()->setText(QStringLiteral("192.0.2.1"));
      Dialog.PortLimitCheck()->setChecked(true);
      Dialog.PortMinSpin()->setValue(50000);
      Dialog.PortMaxSpin()->setValue(50100);
      Dialog.AuthAgentCombo()->setCurrentIndex(1);
      Dialog.CustomTemporaryButton()->setChecked(true);
      Dialog.TemporaryEdit()->setText(Directory);
      Dialog.Save();
      Window.ApplyPreferences();
    }
    Check(CoreBridge::AppliedLocale() != CoreBridge::PreferredLocale(), "the language waits for the next start");
    Check(QString::fromStdString(CoreBridge::LanguageCode()).isEmpty(), "core keeps its language until then");

    // Applied to the panels.
    Check(Local->Names().contains(QStringLiteral(".hidden")), "hidden files shown");
    bool CopyOnCtrlK = false;
    for (QAction * Action : Window.findChildren<QAction *>())
    {
      CopyOnCtrlK = CopyOnCtrlK || ((Action->text() == MainWindow::tr("&Copy...")) &&
        (Action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_K)));
    }
    Check(CopyOnCtrlK, "Explorer's keys: Copy is Ctrl+K");
    Check(Local->Names().indexOf(QStringLiteral("file10.txt")) < Local->Names().indexOf(QStringLiteral("file2.txt")),
      "plain order: file10 before file2");
    Check(Local->View()->model()->index(Local->Names().indexOf(QStringLiteral("a.bin")), FileListModel::SizeColumn).data().toString() ==
      QStringLiteral("1500"), "sizes in bytes");
    Local->View()->sortByColumn(FileListModel::ModifiedColumn, Qt::DescendingOrder);
    Check(Local->Names().indexOf(QStringLiteral("zdir")) < Local->Names().indexOf(QStringLiteral("adir")),
      "directories by name, the way the column sorts, whatever the column");
    Local->View()->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
    QSplitter * Panels = qobject_cast<QSplitter *>(Local->parentWidget());
    Check((Panels != nullptr) && (Panels->widget(0) == Window.Panel()), "the panels swapped");

    // Saved under WinSCP's keys.
    const auto Saved = [&Ini](const QString & Section, const QString & Key)
    {
      // WinSCP's INI: [Configuration\Interface] sections, Key=Value lines.
      QFile File(Ini);
      File.open(QIODevice::ReadOnly | QIODevice::Text);
      QString Current;
      for (const QString & Line : QString::fromUtf8(File.readAll()).split(QLatin1Char('\n')))
      {
        const QString Trimmed = Line.trimmed();
        if (Trimmed.startsWith(QLatin1Char('[')))
        {
          Current = Trimmed.mid(1, Trimmed.size() - 2);
        }
        else if ((Current == Section) && Trimmed.startsWith(Key + QLatin1Char('=')))
        {
          return Trimmed.mid(Key.size() + 1);
        }
      }
      return QStringLiteral("(none)");
    };
    const QString Interface = QStringLiteral("Configuration\\Interface");
    Check((Saved(Interface, QStringLiteral("ShowHiddenFiles")) == QStringLiteral("1")) &&
      (Saved(Interface, QStringLiteral("FormatSizeBytes")) == QStringLiteral("0")) &&
      (Saved(Interface, QStringLiteral("NaturalOrderNumericalSorting")) == QStringLiteral("0")),
      "saved in WinSCP's Interface keys");
    const QString Commander = QStringLiteral("Configuration\\Interface\\Commander");
    Check((Saved(Commander, QStringLiteral("SwappedPanels")) == QStringLiteral("1")) &&
      (Saved(Commander, QStringLiteral("ExplorerKeyboardShortcuts")) == QStringLiteral("1")),
      "and Interface\\Commander's");
    Check(Saved(Interface, QStringLiteral("LocaleSafe")).toUInt() == CoreBridge::PreferredLocale(),
      "the language chosen too");
    Check((Saved(Interface, QStringLiteral("QueueTransfersLimit")) == QStringLiteral("3")) &&
      (Saved(Interface, QStringLiteral("QueueKeepDoneItemsFor")) == QStringLiteral("60")) &&
      (Saved(Interface, QStringLiteral("SessionReopenAuto")) == QStringLiteral("0")),
      "Background's and Endurance's, under core's and the GUI's keys");
    const QString CopyParam = QStringLiteral("Configuration\\Interface\\CopyParam");
    Check((Saved(CopyParam, QStringLiteral("Queue")) == QStringLiteral("1")) &&
      (Saved(CopyParam, QStringLiteral("ResumeSupport")) == QStringLiteral("2")) &&
      (Saved(CopyParam, QStringLiteral("FileNameCase")) == QStringLiteral("2")),
      "the default transfer settings, under Interface\\CopyParam");
    const auto ColorOf = [Local](const QString & Name)
    {
      const int Row = Local->Names().indexOf(Name);
      return Local->View()->model()->index(Row, FileListModel::NameColumn).data(Qt::ForegroundRole).value<QColor>();
    };
    Check(ColorOf(QStringLiteral("file2.txt")) == QColor(0xff0000), "a file in its first matching rule's color");
    Check(ColorOf(QStringLiteral("a.bin")) == QColor(), "one matching none in the panel's");
    Check(Saved(Interface, QStringLiteral("FileColors")).contains(QStringLiteral("0000FF:*.txt")),
      "kept as WinSCP keeps them (BBGGRR:mask)");
    Check(Saved(Interface, QStringLiteral("PathInCaption")) == QStringLiteral("1"), "the window title's path");
    const QString Logging = QStringLiteral("Configuration\\Logging");
    Check((Saved(Logging, QStringLiteral("Logging")) == QStringLiteral("1")) &&
      (Saved(Logging, QStringLiteral("LogProtocol")) == QStringLiteral("1")) &&
      (Saved(Logging, QStringLiteral("LogMaxSize")) == QString::number(10 * 1024 * 1024)),
      "Logging's, under core's Logging keys");
    Check((Saved(Interface, QStringLiteral("ExternalIpAddress")) == QStringLiteral("192.0.2.1")) &&
      (Saved(Interface, QStringLiteral("LocalPortNumberMin")) == QStringLiteral("50000")) &&
      (Saved(Interface, QStringLiteral("AuthAgent")) == QStringLiteral("1")), "Network's and Security's, under core's");
    Check(QFileInfo(QString::fromStdString(CoreBridge::TemporaryDirectoryBase())) == QFileInfo(Directory),
      "the temporary directory, where temporary files go now");
    {
      CoreBridge::TPreferences Off = CoreBridge::Preferences();
      Off.Logging = false;
      Off.TemporaryDirectory.clear();
      CoreBridge::SetPreferences(Off);
    }
    Check((CoreBridge::TransferPresets().size() == 5) && (CoreBridge::TransferPresets()[0].Name == "Mine") &&
      (CoreBridge::TransferPresets()[0].Settings.Mode == CoreBridge::TTransferSettings::TMode::Text), "the presets");
    Check(Saved(CopyParam, QStringLiteral("CopyParamList")) == QStringLiteral("5"), "saved as WinSCP's list");

    // The Copy dialog's settings: the presets to choose, the summary.
    {
      TransferSettingsBox Box;
      Check(Box.PresetCombo()->count() == 6, "the Copy dialog offers the default and the presets");
      Check(Box.Settings().Queue, "the background default comes from the settings");
      Box.PresetCombo()->setCurrentIndex(1);
      Check(Box.Settings().Mode == CoreBridge::TTransferSettings::TMode::Text, "choosing a preset uses it");
      Check(!Box.InfoLabel()->text().isEmpty(), "summarised");
      Box.Commit();
      Check(CoreBridge::CurrentTransferPreset() == "Mine", "and makes it the one in use, as WinSCP's");
      Box.PresetCombo()->setCurrentIndex(0);
      Box.Commit();
    }

    // Ctrl+Alt+H: kept, as WinSCP's.
    QApplication::setActiveWindow(&Window);
    QTest::qWaitForWindowActive(&Window);
    Window.ActivatePanel(true);
    QTest::keyClick(Window.windowHandle(), Qt::Key_H, Qt::ControlModifier | Qt::AltModifier);
    Check(!CoreBridge::Preferences().ShowHiddenFiles && !Local->Names().contains(QStringLiteral(".hidden")),
      "Ctrl+Alt+H hides them again, and keeps it");

    // The layout, kept as WinSCP keeps it, on closing.
    Local->View()->setColumnHidden(FileListModel::SizeColumn, true);
    Local->View()->sortByColumn(FileListModel::ModifiedColumn, Qt::DescendingOrder);
    Window.close();
    const QString LocalPanel = QStringLiteral("Configuration\\Interface\\Commander\\LocalPanel");
    const QString Params = Saved(LocalPanel, QStringLiteral("DirViewParams"));
    // Changed is WinSCP's column 3 of the local panel's six; size its 1.
    Check(Params.startsWith(QStringLiteral("3;0|")), "the sort, in WinSCP's columns");
    Check(Params.section(QLatin1Char('|'), 1, 1).section(QLatin1Char(';'), 1, 1).endsWith(QStringLiteral(",0")),
      "the hidden column, hidden");
    Check(Params.section(QLatin1Char('|'), 1, 1).section(QLatin1Char(';'), 5, 5) == QStringLiteral("55,0"),
      "the extension column linscp has not, kept as WinSCP's");
    Check(!Saved(QStringLiteral("Configuration\\Interface\\Commander"), QStringLiteral("WindowParams")).isEmpty() &&
      !Saved(QStringLiteral("Configuration\\Interface\\Commander"), QStringLiteral("LocalPanelWidth")).isEmpty(),
      "the window's placement and the panels' split");
    {
      MainWindow Again(CoreBridge::Info());
      Check(Again.Local()->View()->isColumnHidden(FileListModel::SizeColumn) &&
        (Again.Local()->View()->header()->sortIndicatorSection() == FileListModel::ModifiedColumn) &&
        (Again.Local()->View()->header()->sortIndicatorOrder() == Qt::DescendingOrder), "a new window restores them");
      Again.Local()->View()->setColumnHidden(FileListModel::SizeColumn, false);
    }

    // "Keep main window open when the last session is closed", off: closing
    // it closes the window.
    {
      CoreBridge::TPreferences Off = CoreBridge::Preferences();
      Off.KeepOpenWhenNoSession = false;
      CoreBridge::SetPreferences(Off);
      MainWindow Closing(CoreBridge::Info());
      Closing.show();
      for (QAction * Action : Closing.findChildren<QAction *>())
      {
        if (Action->text() == MainWindow::tr("&Disconnect"))
        {
          Action->setEnabled(true);
          Action->trigger();
        }
      }
      Check(!Closing.isVisible(), "without keeping it open, the window closes with the session");
      Off.KeepOpenWhenNoSession = true;
      CoreBridge::SetPreferences(Off);
    }

    // What the preferences do not set survives their saving.
    {
      CoreBridge::TEditorSettings Editor = CoreBridge::EditorSettings();
      Editor.WordWrap = !Editor.WordWrap;
      CoreBridge::SetEditorSettings(Editor);
      CoreBridge::SetPreferences(CoreBridge::Preferences());
      Check(CoreBridge::EditorSettings().WordWrap == Editor.WordWrap, "the editor's settings kept when preferences are saved");
      Editor.WordWrap = !Editor.WordWrap;
      CoreBridge::SetEditorSettings(Editor);
    }

    // Editors and Internal editor: saved, and what the editor then is.
    {
      const CoreBridge::TEditorSettings Before = CoreBridge::EditorSettings();
      PreferencesDialog Dialog;
      Check(Dialog.EditorWarnLargeFileCheck()->isChecked() && (Dialog.EditorLargeFileSizeSpin()->value() == 10240) &&
        (Dialog.EditorTabSizeSpin()->value() == 8), "WinSCP's editor defaults");
      Dialog.SetEditorFont(QFont(QStringLiteral("Monospace"), 13));
      Dialog.SetEditorColors(0x112233, 0xffeedd);
      Dialog.EditorTabSizeSpin()->setValue(4);
      Dialog.EditorLargeFileSizeSpin()->setValue(1);
      Dialog.EditorCheckNotModifiedCheck()->setChecked(true);
      Dialog.Save();
      const CoreBridge::TEditorSettings After = CoreBridge::EditorSettings();
      Check((After.FontSize == 13) && (After.FontColor == 0x112233) && (After.BackgroundColor == 0xffeedd) &&
        (After.TabSize == 4) && (After.LargeFileSize == 1) && After.CheckNotModified, "saved");
      // The editor preference list.
      Check(Dialog.EditorsList()->topLevelItemCount() == 2, "WinSCP's two editors listed");
      CoreBridge::TEditorDefinition Vim;
      Vim.Kind = CoreBridge::TEditorDefinition::TKind::External;
      Vim.Command = "vim";
      Vim.Mask = "*.conf";
      Vim.Text = true;
      Dialog.EditorsList()->setCurrentItem(Dialog.EditorsList()->topLevelItem(0));
      Check(Dialog.AddEditEditor(false, &Vim) && (Dialog.EditorDefinitions()[0].Command == "vim") &&
        (Dialog.EditorsList()->topLevelItem(0)->text(0) == QStringLiteral("Vim")) &&
        (Dialog.EditorsList()->topLevelItem(0)->text(2) == PreferencesDialog::tr("Yes")), "an editor added before the selected one");
      Dialog.Save();
      Check((CoreBridge::Editors().size() == 3) && (CoreBridge::Editors()[0].Mask == "*.conf"), "the list saved");
      Dialog.EditorsList()->setCurrentItem(Dialog.EditorsList()->topLevelItem(0));
      Dialog.RemoveEditor();
      Dialog.Save();
      Check(CoreBridge::Editors().size() == 2, "and removed");
      const QString Large = QDir(Directory).filePath(QStringLiteral("large.txt"));
      {
        QFile File(Large);
        File.open(QIODevice::WriteOnly);
        File.write(QByteArray(4096, 'x'));
      }
      EditorWindow Editor(Large, QString(), QStringLiteral("linscp"));
      Check((Editor.Editor()->font().pointSize() == 13) &&
        (Editor.Editor()->palette().color(QPalette::Base) == QColor(QRgb(0xffeedd))), "the editor in that font and colours");
      QString Asked;
      Responder.Custom = [&Asked](QWidget * Modal)
      {
        QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
        if (Query == nullptr)
        {
          return false;
        }
        for (const QLabel * Label : Query->findChildren<QLabel *>())
        {
          Asked += Label->text();
        }
        Query->Buttons()[1]->click();
        return true;
      };
      Check(!Editor.Load() && Asked.contains(QStringLiteral("large")), "a file over the size asked about, and declined");
      Responder.Custom = nullptr;
      CoreBridge::SetEditorSettings(Before);
    }

    // Applications: the PuTTY command, its options as upstream enables them.
    {
      PreferencesDialog Dialog;
      // PuTTY, or without it on POSIX ssh in a terminal, a site command.
      Check(!Dialog.PuttyPathEdit()->text().isEmpty() && Dialog.PuttyPasswordCheck()->isEnabled(),
        "a PuTTY command by default");
      Dialog.PuttyPathEdit()->setText(QStringLiteral("putty"));
      Check(Dialog.TelnetForFtpInPuttyCheck()->isEnabled(), "PuTTY itself: Telnet for FTP");
      Dialog.PuttyPathEdit()->setText(QStringLiteral("ssh -p !# !U@!@"));
      Check(!Dialog.TelnetForFtpInPuttyCheck()->isEnabled() && Dialog.PuttyPasswordCheck()->isEnabled(),
        "a site command: no Telnet for FTP");
      Dialog.PuttyPathEdit()->setText(QStringLiteral("sshpass -p !P ssh !U@!@"));
      Check(!Dialog.PuttyPasswordCheck()->isEnabled(), "one with the password: no passing it");
      Dialog.PuttyPathEdit()->clear();
      Check(!Dialog.AutoOpenInPuttyCheck()->isEnabled(), "none: nothing to open");
      Dialog.PuttyPathEdit()->setText(QStringLiteral("myputty"));
      Dialog.AutoOpenInPuttyCheck()->setChecked(true);
      const CoreBridge::TPreferences Saved = CoreBridge::Preferences();
      Dialog.Save();
      const CoreBridge::TPreferences Now = CoreBridge::Preferences();
      Check((Now.PuttyPath == "myputty") && Now.AutoOpenInPutty, "saved");
      CoreBridge::SetPreferences(Saved);
    }

    // Commands: the custom commands, edited.
    {
      const std::vector<CoreBridge::TCustomCommandDefinition> Before = CoreBridge::CustomCommands();
      PreferencesDialog Dialog;
      Check(Dialog.CommandsList()->topLevelItemCount() == static_cast<int>(Before.size()) && !Before.empty(),
        "WinSCP's commands listed");
      Check(Dialog.CommandsList()->topLevelItem(0)->text(2) == QStringLiteral("R"), "remote ones, R");
      CoreBridge::TCustomCommandDefinition Command;
      Command.Name = "Disk usage";
      Command.Command = "du -sh \"!\"";
      Dialog.CommandsList()->setCurrentItem(nullptr);
      Check(Dialog.AddEditCommand(false, &Command) &&
        (Dialog.CommandsList()->topLevelItemCount() == static_cast<int>(Before.size()) + 1) &&
        (Dialog.Commands().back().Name == "Disk usage"), "added last, nothing selected");
      Dialog.MoveCommand(-1);
      Check(Dialog.Commands()[Before.size() - 1].Name == "Disk usage", "moved up");
      Dialog.CommandsList()->setCurrentItem(Dialog.CommandsList()->topLevelItem(0));
      Dialog.RemoveCommand();
      Check(Dialog.Commands().size() == Before.size(), "one removed");
      Dialog.Save();
      Check((CoreBridge::CustomCommands().size() == Before.size()) &&
        (CoreBridge::CustomCommands()[Before.size() - 2].Name == "Disk usage"), "and saved");
      CoreBridge::SetCustomCommands(Before);
    }
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

QByteArray ReadAll(const QString & FileName)
{
  QFile File(FileName);
  return File.open(QIODevice::ReadOnly) ? File.readAll() : QByteArray();
}

void WriteAll(const QString & FileName, const QByteArray & Data)
{
  QFile File(FileName);
  File.open(QIODevice::WriteOnly | QIODevice::Truncate);
  File.write(Data);
}

bool WaitUntil(const std::function<bool()> & Condition)
{
  QElapsedTimer Timer;
  Timer.start();
  while (!Condition() && (Timer.elapsed() < 30000))
  {
    QApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  return Condition();
}

void Editing(const char * Port, const QString & Directory)
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
    // The editor's messages and questions: OK, Yes.
    Responder.AcceptDialogs = true;
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "the session opens");
    const QDir Dir(Directory);
    LocalPanel * Local = Window.Local();
    RemotePanel * Remote = Window.Panel();

    // A local file, edited in place: its CRLF line endings kept.
    WriteAll(Dir.filePath(QStringLiteral("crlf.txt")), "one\r\ntwo\r\n");
    Local->ChangeDirectory(Directory);
    Local->Select({ QStringLiteral("crlf.txt") });
    Window.ActivatePanel(true);
    EditorWindow * Editor = Window.EditCurrent();
    Check((Editor != nullptr) && Editor->isVisible(), "F4 opens the local file in the editor");
    if (Editor == nullptr)
    {
      return;
    }
    Check(Editor->Editor()->toPlainText() == QStringLiteral("one\ntwo\n"), "its text, lines as lines");
    Check(Editor->CrLf() && Editor->Utf8() && !Editor->Bom() && !Editor->Modified(), "CRLF, UTF-8, no BOM, unmodified");
    Check(Editor->windowTitle().contains(QStringLiteral("crlf.txt")) && Editor->windowTitle().contains(EditorWindow::tr("Editor")),
      "the title names the file and the editor");
    Check(Window.EditCurrent() == Editor, "F4 again brings the same editor up");
    Editor->Editor()->moveCursor(QTextCursor::End);
    Editor->Editor()->insertPlainText(QStringLiteral("three\n"));
    Check(Editor->Modified() && (Editor->StatusText(4) == EditorWindow::tr("Modified")), "typing modifies it, as the status says");
    Check(Editor->Save() && !Editor->Modified(), "saved");
    Check(ReadAll(Dir.filePath(QStringLiteral("crlf.txt"))) == "one\r\ntwo\r\nthree\r\n", "in place, CRLF kept");

    // Find, replace all, go to line, the status bar.
    Editor->Editor()->moveCursor(QTextCursor::Start);
    EditorWindow::TFind Find;
    Find.Text = QStringLiteral("TWO");
    Check((Editor->Find(EditorWindow::TFindMode::Find, Find) == 1) && (Editor->Editor()->textCursor().selectedText() == QStringLiteral("two")),
      "find, in any case by default");
    Find.MatchCase = true;
    Editor->Editor()->moveCursor(QTextCursor::Start);
    Responder.Accepted.clear();
    Check((Editor->Find(EditorWindow::TFindMode::Find, Find) == 0) && !Responder.Accepted.isEmpty(), "matching case, not found, and said so");
    Check(CoreBridge::EditorSettings().FindMatchCase && (CoreBridge::EditorSettings().FindText == "TWO"), "the search kept for next time");
    EditorWindow::TFind Replace;
    Replace.Text = QStringLiteral("o");
    Replace.ReplaceText = QStringLiteral("0");
    Editor->Editor()->moveCursor(QTextCursor::Start);
    Check(Editor->Find(EditorWindow::TFindMode::ReplaceAll, Replace) == 2, "replace all");
    Check(Editor->Editor()->toPlainText() == QStringLiteral("0ne\ntw0\nthree\n"), "every one");
    Editor->Editor()->undo();
    Check(Editor->Editor()->toPlainText() == QStringLiteral("one\ntwo\nthree\n"), "undone in one step");
    Check(Editor->GoToLine(3) && (Editor->Editor()->textCursor().blockNumber() == 2), "go to line");
    Check(!Editor->GoToLine(9) && !Editor->GoToLine(0), "not to one there is not");
    Editor->Editor()->moveCursor(QTextCursor::Right);
    Check(Editor->StatusText(0) == EditorWindow::tr("Line: %1/%2").arg(3).arg(4), "the status: the line");
    Check(Editor->StatusText(1) == EditorWindow::tr("Column: %1").arg(2), "the column");
    Check(Editor->StatusText(2) == EditorWindow::tr("Character: %1 (0x%2)").arg(104).arg(QStringLiteral("68")), "the character");
    Check(Editor->StatusText(3) == EditorWindow::tr("Encoding: %1").arg(EditorWindow::tr("UTF-8")), "the encoding");
    // Closing a modified file asks to save it (the responder says Yes).
    Editor->Editor()->moveCursor(QTextCursor::End);
    Editor->Editor()->insertPlainText(QStringLiteral("four"));
    Check(Window.CloseEditors(false), "closing asks, and saves");
    Check(ReadAll(Dir.filePath(QStringLiteral("crlf.txt"))) == "one\r\ntwo\r\nthree\r\nfour", "what was typed");
    Check(Window.Editors().empty(), "and the editor is gone");

    // Not UTF-8: read in the system's code page instead, with a message;
    // saved in it.
    WriteAll(Dir.filePath(QStringLiteral("latin.txt")), "caf\xe9\n");
    Responder.Accepted.clear();
    Editor = Window.EditLocal(Dir.filePath(QStringLiteral("latin.txt")));
    Check((Editor != nullptr) && !Responder.Accepted.isEmpty(), "a file not in UTF-8 falls back, saying so");
    if (Editor != nullptr)
    {
      Check(!Editor->Utf8() && (Editor->Editor()->toPlainText() == QStringLiteral("caf\u00e9\n")), "and reads in the other encoding");
      Editor->Editor()->moveCursor(QTextCursor::End);
      Editor->Editor()->insertPlainText(QStringLiteral("cr\u00e8me\n"));
      Check(Editor->Save() && (ReadAll(Dir.filePath(QStringLiteral("latin.txt"))) == "caf\xe9\ncr\xe8me\n"), "and saves in it");
      Check(Editor->SetUtf8(true) == false, "it is not UTF-8: it stays as it was");
      Check(!Editor->Utf8(), "in its encoding");
      Editor->close();
    }
    // A BOM says UTF-8, and is kept.
    WriteAll(Dir.filePath(QStringLiteral("bom.txt")), "\xEF\xBB\xBFx\n");
    Editor = Window.EditLocal(Dir.filePath(QStringLiteral("bom.txt")));
    Check((Editor != nullptr) && Editor->Bom() && (Editor->Editor()->toPlainText() == QStringLiteral("x\n")), "a BOM: UTF-8, not in the text");
    if (Editor != nullptr)
    {
      Editor->Editor()->insertPlainText(QStringLiteral("y"));
      Check(Editor->Save() && (ReadAll(Dir.filePath(QStringLiteral("bom.txt"))) == "\xEF\xBB\xBFyx\n"), "and kept when saved");
      Editor->close();
    }

    // A remote file: downloaded to a temporary directory, saved back
    // through the queue.
    WriteAll(Dir.filePath(QStringLiteral("remote.txt")), "remote\n");
    Check(Window.Upload({ QStringLiteral("remote.txt") }, QStringLiteral("/"), false), "a file to edit remotely");
    Remote->Select({ QStringLiteral("remote.txt") });
    Window.ActivatePanel(false);
    Editor = Window.EditCurrent();
    Check(Editor != nullptr, "F4 opens a remote file");
    if (Editor == nullptr)
    {
      return;
    }
    const QString Temporary = Editor->FileName();
    Check(QFileInfo(Temporary).fileName() == QStringLiteral("remote.txt") && !Temporary.startsWith(Directory),
      "from a temporary copy");
    Check(Editor->windowTitle().startsWith(QStringLiteral("/remote.txt")), "its title the remote path");
    // Text mode: the platform's line endings locally.
    Check(Editor->Editor()->toPlainText() == QStringLiteral("remote\n") && (Editor->CrLf() == (QDir::separator() == QLatin1Char('\\'))),
      "with its text, as the platform's lines");
    Check(Window.EditCurrent() == Editor, "F4 again brings it up");
    Editor->Editor()->moveCursor(QTextCursor::End);
    Editor->Editor()->insertPlainText(QStringLiteral("edited\n"));
    Check(Editor->Save(), "saving uploads");
    Check(WaitUntil([&Window]() { return Window.Session()->QueueEmpty(); }), "through the queue");
    QDir().mkpath(Dir.filePath(QStringLiteral("down")));
    Check(Window.Download({ QStringLiteral("remote.txt") }, Dir.filePath(QStringLiteral("down")), false) &&
      (ReadAll(Dir.filePath(QStringLiteral("down/remote.txt"))) == "remote\nedited\n"), "the server has it");
    Editor->close();
    Check(WaitUntil([&Temporary]() { return !QFileInfo::exists(Temporary); }), "closed, its temporary copy is removed");

    // Disconnecting closes remote editors: a modified one saved first, here.
    Remote->Select({ QStringLiteral("remote.txt") });
    Editor = Window.EditCurrent();
    Check(Editor != nullptr, "opened again");
    if (Editor != nullptr)
    {
      Editor->Editor()->moveCursor(QTextCursor::End);
      Editor->Editor()->insertPlainText(QStringLiteral("again\n"));
      Check(Window.CloseEditors(true) && Window.Editors().empty(), "closed with the session, after saving");
      Check(Window.Session()->QueueEmpty(), "uploaded at once, not queued");
      QFile::remove(Dir.filePath(QStringLiteral("down/remote.txt")));
      Check(Window.Download({ QStringLiteral("remote.txt") }, Dir.filePath(QStringLiteral("down")), false) &&
        (ReadAll(Dir.filePath(QStringLiteral("down/remote.txt"))) == "remote\nedited\nagain\n"), "the server has that too");
    }

    // Preferences > Editors: an external editor by mask, its copy uploaded
    // whenever it changes, until the session closes.
    {
      const std::vector<CoreBridge::TEditorDefinition> Defaults = CoreBridge::Editors();
      Check((Defaults.size() == 2) && (Defaults[0].Kind == CoreBridge::TEditorDefinition::TKind::Internal),
        "WinSCP's editors by default: the internal one first");
      CoreBridge::TEditorDefinition External;
      External.Kind = CoreBridge::TEditorDefinition::TKind::External;
      External.Command = "myeditor --file !.! --wait";
      External.Mask = "*.ext";
      std::vector<CoreBridge::TEditorDefinition> Editors = Defaults;
      Editors.insert(Editors.begin(), External);
      CoreBridge::SetEditors(Editors);
      Check((CoreBridge::FindEditor(CoreBridge::Editors(), "a.ext", false, 0, 0) == 0) &&
        (CoreBridge::FindEditor(CoreBridge::Editors(), "a.txt", false, 0, 0) == 1), "each file its editor by mask");
      Check(CoreBridge::Editors()[0].Name == "Myeditor", "named as WinSCP names it");
      WriteAll(Dir.filePath(QStringLiteral("up.ext")), "external\n");
      Check(Window.Upload({ Dir.filePath(QStringLiteral("up.ext")) }, QStringLiteral("/"), false), "a file for it");
      QString Started;
      QString Params;
      Frontend.SetStartProgram([&Started, &Params](const QString & Program, const QString & AParams, const QString &)
      {
        Started = Program;
        Params = AParams;
        return true;
      });
      Remote->Refresh();
      Remote->Select({ QStringLiteral("up.ext") });
      Window.ActivatePanel(false);
      Check((Window.EditCurrent() == nullptr) && (Started == QStringLiteral("myeditor")) &&
        Params.startsWith(QStringLiteral("--file ")) && Params.endsWith(QStringLiteral(" --wait")) &&
        (Window.ExternalEditedCount() == 1), "F4: the external editor, the temporary copy for !.!");
      QString Copy = Params.mid(7, Params.length() - 7 - 7);
      if (Copy.startsWith(QLatin1Char('"')))
      {
        Copy = Copy.mid(1, Copy.length() - 2);
      }
      Check(QFileInfo::exists(Copy), "a copy that exists");
      QThread::msleep(1100);
      WriteAll(Copy, "external\nsaved\n");
      Window.CheckExternalEdited();
      Check(WaitUntil([&Window]() { return Window.Session()->QueueEmpty(); }), "its change uploaded");
      QFile::remove(Dir.filePath(QStringLiteral("down/up.ext")));
      Check(Window.Download({ QStringLiteral("up.ext") }, Dir.filePath(QStringLiteral("down")), false) &&
        (ReadAll(Dir.filePath(QStringLiteral("down/up.ext"))) == "external\nsaved\n"), "the server has it");
      // The associated application (Files > Open), a local file as it is.
      QUrl Opened;
      Window.SetUrlOpener([&Opened](const QUrl & Url) { Opened = Url; return true; });
      Local->Select({ QStringLiteral("crlf.txt") });
      Window.ActivatePanel(true);
      Check(Window.OpenCurrent() && (QDir(QFileInfo(Opened.toLocalFile()).path()) == QDir(Directory)), "Open: the local file itself");
      Window.SetUrlOpener(nullptr);
      Check(Window.CloseEditors(true) && (Window.ExternalEditedCount() == 0) &&
        WaitUntil([&Copy]() { return !QFileInfo::exists(Copy); }), "the session closing ends it, its copy removed");
      Frontend.SetStartProgram(nullptr);
      CoreBridge::SetEditors(Defaults);
    }
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void Properties(const char * Port)
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
    Check(Window.OpenUrl(QStringLiteral("ftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "the session opens");
    CoreBridge::TSession * Session = Window.Session();
    RemotePanel * Remote = Window.Panel();

    // The rights frame alone.
    {
      RightsFrame Frame;
      CoreBridge::TRightsState State;
      State.Set = 0644;
      State.Unset = 07777 & ~0644u;
      Frame.SetRights(State);
      Check(Frame.OctalEdit()->text() == QStringLiteral("0644"), "the octal form");
      Check(Frame.Text() == QStringLiteral("rw-r--r--"), "and core's text");
      Frame.OctalEdit()->setText(QStringLiteral("750"));
      Q_EMIT Frame.OctalEdit()->textEdited(QStringLiteral("750"));
      Check((Frame.Rights().Set == 0750) && (Frame.Check(5)->checkState() == Qt::Checked) &&
        (Frame.Check(11)->checkState() == Qt::Unchecked), "typing octal sets the boxes");
      QApplication::clipboard()->setText(QStringLiteral("rwxrwx---"));
      Frame.Paste();
      Check(Frame.Rights().Set == 0770, "pasted as text");
      Frame.CopyAsOctal();
      Check(QApplication::clipboard()->text() == QStringLiteral("0770"), "copied as octal");
      Frame.AllRights();
      Check(Frame.Rights().Set == 0777, "all rights");
      Frame.SetAllowUndef(true);
      Frame.LeaveAsIs();
      Check((Frame.Rights().Set == 0) && (Frame.Rights().Unset == 0) && Frame.OctalEdit()->text().isEmpty(),
        "left as is: every bit mixed, no octal");
    }

    // One file.
    Remote->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    CoreBridge::TPropertiesInfo Info = Session->PropertiesOf({ "hello.txt" });
    Check(Info.Properties.HasRights && (Info.Properties.Rights.Set == 0644) && Info.CanChangeRights,
      "a file's permissions, which the server can change");
    Check((Info.Files == 1) && (Info.Directories == 0) && (Info.Size == 14) && !Info.CanCalculateSize, "its kind and size");
    {
      PropertiesDialog Dialog(Session, { "hello.txt" }, QStringLiteral("/"), Info);
      Check(Dialog.windowTitle() == QStringLiteral("hello.txt Properties"), "the dialog's title, WinSCP's");
      Check(Dialog.FileEdit()->text() == QStringLiteral("hello.txt"), "names the file");
      Check(Dialog.SizeEdit()->text() == QStringLiteral("14 B"), "its size");
      Check(!Dialog.OkButton()->isEnabled() && Dialog.RecursiveCheck()->isHidden(), "nothing to apply yet, no recursion");
      Dialog.Rights()->Check(5)->setCheckState(Qt::Checked);
      Check(Dialog.OkButton()->isEnabled() && (Dialog.Rights()->OctalEdit()->text() == QStringLiteral("0744")), "a change can be");
    }
    // Through Files > Properties: the dialog's change applied.
    Responder.Custom = [](QWidget * Modal)
    {
      PropertiesDialog * Dialog = qobject_cast<PropertiesDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      CoreBridge::TRightsState State;
      State.Set = 0600;
      State.Unset = 07777 & ~0600u;
      Dialog->Rights()->SetRights(State);
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.Properties(), "Properties changes the permissions");
    Info = Session->PropertiesOf({ "hello.txt" });
    Check(Info.Properties.Rights.Set == 0600, "as the server now lists them");

    // A file and a directory: mixed bits, the size calculated, recursion.
    Responder.Custom = nullptr;
    Info = Session->PropertiesOf({ "hello.txt", "sub" });
    Check((Info.Files == 1) && (Info.Directories == 1) && Info.CanCalculateSize, "a directory's size is to be calculated");
    {
      PropertiesDialog Dialog(Session, { "hello.txt", "sub" }, QStringLiteral("/"), Info);
      Check(Dialog.windowTitle() == QStringLiteral("hello.txt, ... Properties"), "several: WinSCP's title");
      Check(Dialog.SizeEdit()->text() == CoreBridge::Text(CoreBridge::TText::PropertiesUnknownSize).c_str(), "size unknown");
      Check(Dialog.FileEdit()->text() == QStringLiteral("1 file, 1 folder"), "counted");
      Check(Dialog.Rights()->AllowUndef() && (Dialog.Rights()->Check(5)->checkState() == Qt::PartiallyChecked) &&
        Dialog.Rights()->OctalEdit()->text().isEmpty(), "bits they differ in mixed");
      Check(!Dialog.RecursiveCheck()->isHidden(), "recursion offered");
      Check(Dialog.CalculateSize() && (Dialog.SizeEdit()->text() == QStringLiteral("14 B")), "the size calculated");
      Dialog.RecursiveCheck()->setChecked(true);
      Check(Dialog.OkButton()->isEnabled(), "recursion alone is a change");
    }
    CoreBridge::TFileProperties Change = Session->PropertiesOf({ "sub" }).Properties;
    Change.Rights.Set = 0700;
    Change.Rights.Unset = 07777 & ~0700u;
    Check(Session->SetProperties({ "sub" }, Change), "a directory's permissions");
    Check(Session->PropertiesOf({ "sub" }).Properties.Rights.Set == 0700, "changed");
    Check(!Session->SetProperties({ "sub" }, Change), "and nothing to change the second time");

    // Create folder (F7): the name, and permissions for a remote one.
    {
      CreateDirectoryDialog Local(QStringLiteral("New folder"), false, false, CoreBridge::TNewDirectory());
      Check(Local.AttributesGroup()->isHidden() && Local.SaveSettingsCheck()->isHidden(), "a local folder: a name only");
      Local.NameEdit()->setText(QStringLiteral("  "));
      Check(!Local.OkButton()->isEnabled(), "and one is needed");
      CreateDirectoryDialog Dialog(QStringLiteral("New folder"), true, Session->CanChangeRights(), CoreBridge::NewDirectorySettings());
      Check(!Dialog.SetRightsCheck()->isChecked() && !Dialog.Rights()->isEnabled() && Dialog.SetRightsCheck()->isEnabled(),
        "a remote one: permissions not set by default, settable");
      Dialog.SetRightsCheck()->setChecked(true);
      Check(Dialog.Rights()->isEnabled(), "set, the rights frame is on");
    }
    Responder.Custom = [](QWidget * Modal)
    {
      CreateDirectoryDialog * Dialog = qobject_cast<CreateDirectoryDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Dialog->NameEdit()->setText(QStringLiteral("made"));
      Dialog->SetRightsCheck()->setChecked(true);
      CoreBridge::TRightsState State;
      State.Set = 0750;
      State.Unset = 07777 & ~0750u;
      Dialog->Rights()->SetRights(State);
      Dialog->SaveSettingsCheck()->setChecked(true);
      Dialog->OkButton()->click();
      return true;
    };
    Check(Remote->NewFolderDialog() && Remote->Names().contains(QStringLiteral("made")), "F7 makes the folder");
    Check(Session->PropertiesOf({ "made" }).Properties.Rights.Set == 0750, "with its permissions");
    Check(CoreBridge::NewDirectorySettings().SetRights && (CoreBridge::NewDirectorySettings().Rights.Set == 0750),
      "kept for next time");
    Responder.Custom = nullptr;

    // Links (Alt+F6).
    {
      LinkDialog Dialog(QString(), QStringLiteral("hello.txt"), true, false, true);
      Check(Dialog.windowTitle() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::LinkAddCaption)), "WinSCP's title");
      Check(!Dialog.OkButton()->isEnabled() && Dialog.HardLinkCheck()->isEnabled(), "a name is needed; hard links offered");
      Dialog.FileNameEdit()->setText(QStringLiteral("link"));
      Check(Dialog.OkButton()->isEnabled() && Dialog.SymbolicLink(), "symbolic by default");
      LinkDialog Editing(QStringLiteral("link"), QStringLiteral("hello.txt"), true, true, true);
      Check(Editing.FileNameEdit()->isReadOnly() && !Editing.HardLinkCheck()->isEnabled() &&
        (Editing.windowTitle() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::LinkEditCaption))),
        "editing: the name fixed, no hard link");
    }
    Check(!Session->CanLink(), "FTP makes no links");
    Remote->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    Check(!Window.AddEditLink(), "so Alt+F6 does nothing there");
    QTemporaryDir LocalDirectory;
    {
      QFile Target(QDir(LocalDirectory.path()).filePath(QStringLiteral("target.txt")));
      Target.open(QIODevice::WriteOnly);
      Target.write("t");
    }
    LocalPanel * Local = Window.Local();
    Local->ChangeDirectory(LocalDirectory.path());
    Local->Select({ QStringLiteral("target.txt") });
    Window.ActivatePanel(true);
#if defined(_WIN32)
    const QString LinkName = QStringLiteral("target.lnk");
#else
    const QString LinkName = QStringLiteral("target-link");
#endif
    Responder.Custom = [&LinkName](QWidget * Modal)
    {
      LinkDialog * Dialog = qobject_cast<LinkDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Check((Dialog->PointToEdit()->text() == QStringLiteral("target.txt")) && Dialog->FileNameEdit()->text().isEmpty() &&
        !Dialog->HardLinkCheck()->isEnabled(), "a local link to the focused file, symbolic");
      Dialog->FileNameEdit()->setText(LinkName);
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.AddEditLink(), "Alt+F6 makes a local link");
    Check(QFileInfo(QDir(LocalDirectory.path()).filePath(LinkName)).isSymLink() &&
      QFileInfo(QDir(LocalDirectory.path()).filePath(LinkName)).symLinkTarget().endsWith(QStringLiteral("target.txt")),
      "pointing to the file");
    Responder.Custom = nullptr;

    // Duplicate and Move To.
    {
      RemoteTransferDialog Dialog(Session, false, false, QStringLiteral("/"), QStringLiteral("hello.txt"));
      Check(Dialog.windowTitle() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::RemoteCopyTitle)), "Duplicate's title");
      Check(Dialog.PathCombo()->currentText() == QStringLiteral("/hello.txt"), "the file's own path to start with");
      Check(!Session->CanRemoteCopy() && Dialog.NotDirectCopyCheck()->isChecked() && !Dialog.NotDirectCopyCheck()->isEnabled(),
        "FTP copies nothing itself: through a temporary copy");
      Dialog.PathCombo()->setEditText(QStringLiteral("/sub"));
      Check(Dialog.Validate() && (Dialog.PathCombo()->currentText() == QStringLiteral("/sub/*.*")) &&
        (Dialog.Target() == QStringLiteral("/sub/")) && (Dialog.FileMask() == QStringLiteral("*.*")),
        "an existing directory takes the file in");
      RemoteTransferDialog Several(Session, true, true, QStringLiteral("/"), QStringLiteral("*.*"));
      Several.PathCombo()->setEditText(QStringLiteral("/one.txt"));
      Responder.LastQuery.clear();
      Responder.AcceptDialogs = false;
      Check(Several.Validate() && !Responder.LastQuery.isEmpty(), "several files to one name: asked first");
    }
    Remote->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    Responder.Custom = [](QWidget * Modal)
    {
      RemoteTransferDialog * Dialog = qobject_cast<RemoteTransferDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Dialog->PathCombo()->setEditText(QStringLiteral("/copy.txt"));
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.RemoteTransfer(false) && Remote->Names().contains(QStringLiteral("copy.txt")) &&
      Remote->Names().contains(QStringLiteral("hello.txt")), "Shift+F5 duplicates under a new name");
    Remote->Select({ QStringLiteral("copy.txt") });
    Responder.Custom = [](QWidget * Modal)
    {
      RemoteTransferDialog * Dialog = qobject_cast<RemoteTransferDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Dialog->PathCombo()->setEditText(QStringLiteral("/sub"));
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.RemoteTransfer(true) && !Remote->Names().contains(QStringLiteral("copy.txt")), "Shift+F6 moves it away");
    Check(Window.Session()->DirectoryExists("/sub") && Remote->ChangeDirectory(QStringLiteral("/sub")) &&
      Remote->Names().contains(QStringLiteral("copy.txt")), "into the directory, under its name");
    Remote->ChangeDirectory(QStringLiteral("/"));
    Responder.Custom = nullptr;

    // The console: FTP runs its protocol's commands.
    Check(Session->CanRunCommands(), "FTP runs commands itself");
    {
      ConsoleDialog Console(Session);
      Check(Console.DirectoryLabel()->text() == QStringLiteral("/"), "the current directory");
      Check(Console.Execute(QStringLiteral("PWD")) && Console.Output()->toPlainText().contains(QStringLiteral("257")),
        "a command's output");
      Check(Console.Output()->toPlainText().startsWith(QStringLiteral("/$ PWD")), "after the prompt, as WinSCP's");
      // FTP keeps its own idea of the directory, as upstream's: a raw CWD
      // leaves the session's.
      Check(Console.Execute(QStringLiteral("SYST")) && Console.Output()->toPlainText().contains(QStringLiteral("215")),
        "another");
      Console.Execute(QStringLiteral("PWD"));
      Check(Console.CommandCombo()->count() == 2, "the commands kept, each once");
    }
    Responder.Custom = [](QWidget * Modal)
    {
      ConsoleDialog * Console = qobject_cast<ConsoleDialog *>(Modal);
      if (Console == nullptr)
      {
        return false;
      }
      Console->Execute(QStringLiteral("PWD"));
      Console->reject();
      return true;
    };
    Check(Window.OpenConsole(), "Open Terminal");
    Responder.Custom = nullptr;
    Remote->ChangeDirectory(QStringLiteral("/"));
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void BookmarksTest(const char * Port, const QString & Directory)
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
    const std::string Key = Window.Session()->SessionKey();
    Check(!Key.empty(), "the site has a key for its bookmarks");
    RemotePanel * Remote = Window.Panel();

    {
      OpenDirectoryDialog Dialog(true, QStringLiteral("/sub"), Remote->History(), Key, false, false);
      Check(Dialog.Pages()->count() == 2, "the site's and the shared bookmarks");
      Check(Dialog.windowTitle() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::OpenDirectoryCaption)), "WinSCP's title");
      Dialog.AddAsBookmark(0);
      Check((Dialog.List(0)->count() == 1) && (Dialog.List(0)->item(0)->text() == QStringLiteral("/sub")), "the directory added");
      Dialog.DirectoryCombo()->setEditText(QStringLiteral("/"));
      Dialog.AddAsBookmark(0);
      Check((Dialog.List(0)->count() == 2) && (Dialog.List(0)->item(0)->text() == QStringLiteral("/")),
        "another, before the one focused");
      Dialog.AddAsBookmark(0);
      Check(Dialog.List(0)->count() == 2, "but not twice");
      Dialog.List(0)->setCurrentRow(0);
      Dialog.MoveBookmark(0, 1);
      Check((Dialog.List(0)->item(1)->text() == QStringLiteral("/")) && (Dialog.List(0)->currentRow() == 1), "moved down");
      Dialog.List(0)->setCurrentRow(0);
      Check(Dialog.DirectoryCombo()->currentText() == QStringLiteral("/sub"), "choosing one fills the directory in");
      Dialog.DirectoryCombo()->setEditText(QStringLiteral("/shared"));
      Dialog.Pages()->setCurrentIndex(1);
      Dialog.AddAsBookmark(1);
      Dialog.reject();
    }
    std::vector<CoreBridge::TBookmarkEntry> Site = CoreBridge::Bookmarks(Key);
    Check((Site.size() == 2) && (Site[0].Remote == "/sub") && (Site[1].Remote == "/"), "the site's kept, even cancelled");
    std::vector<CoreBridge::TBookmarkEntry> Shared = CoreBridge::Bookmarks(std::string());
    Check((Shared.size() == 1) && (Shared[0].Remote == "/shared"), "the shared ones too");
    Check(CoreBridge::UseSharedBookmarks(), "and the shared page as the one last used");
    {
      OpenDirectoryDialog Dialog(true, QStringLiteral("/"), QStringList(), Key, false, false);
      Dialog.List(0)->setCurrentRow(1);
      Dialog.RemoveBookmark(0);
      Check((Dialog.List(0)->count() == 1) && (Dialog.List(0)->item(0)->text() == QStringLiteral("/sub")), "removed");
      OpenDirectoryDialog Local(false, QStringLiteral("/"), QStringList(), Key, false, false);
      Check(Local.List(0)->count() == 0, "a remote bookmark is not a local one");
      Local.reject();
      Dialog.reject();
    }
    Check(CoreBridge::Bookmarks(Key).size() == 1, "and kept so");

    // A shared bookmark's keyboard shortcut (Select keyboard shortcut).
    {
      OpenDirectoryDialog Dialog(true, QStringLiteral("/sub"), QStringList(), Key, false, true);
      Dialog.Pages()->setCurrentIndex(1);
      Dialog.AddAsBookmark(1);
      Check(Dialog.ShortCutButton()->isEnabled(), "a shared bookmark selected: Shortcut...");
      bool Offered = true;
      Responder.Custom = [&Offered](QWidget * Modal)
      {
        ShortCutDialog * ShortCut = qobject_cast<ShortCutDialog *>(Modal);
        if (ShortCut == nullptr)
        {
          return false;
        }
        Offered = (ShortCut->windowTitle() == QStringLiteral("Select keyboard shortcut"));
        const int Index = ShortCut->Combo()->findData(0x4000u | '1');
        if (Index < 0)
        {
          Offered = false;
          ShortCut->reject();
          return true;
        }
        ShortCut->Combo()->setCurrentIndex(Index);
        ShortCut->accept();
        return true;
      };
      Check(Dialog.ShortCutBookmark() && Offered, "Ctrl+1 chosen");
      Responder.Custom = nullptr;
      const QString Text = Dialog.List(1)->item(Dialog.List(1)->currentRow())->text();
      Check(Text.startsWith(QStringLiteral("/sub")) && Text.contains(QStringLiteral("Ctrl+1")), "shown with the bookmark");
      Dialog.List(1)->setCurrentRow(0);
      ShortCutDialog Other({ 0x4000u | '1' }, 0);
      Check((Other.Combo()->findData(0x4000u | '1') < 0) && (Other.Combo()->findData(0x4000u | '2') >= 0), "taken shortcuts not offered again");
      Dialog.reject();
    }
    Window.ActivatePanel(false);
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Check(Window.OpenBookmarkShortCut(0x4000u | '1') && (Window.Panel()->Path() == QStringLiteral("/sub")),
      "the shortcut opens the bookmark on the active side");
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));

    // Ctrl+O: a bookmark opened.
    Window.ActivatePanel(false);
    Responder.Custom = [](QWidget * Modal)
    {
      OpenDirectoryDialog * Dialog = qobject_cast<OpenDirectoryDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Dialog->Pages()->setCurrentIndex(0);
      Dialog->List(0)->setCurrentRow(0);
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.OpenDirectory() && (Remote->Path() == QStringLiteral("/sub")), "Ctrl+O opens the bookmark");
    // A directory that cannot be opened: the error, then the dialog again.
    int Shown = 0;
    Responder.Custom = [&Shown](QWidget * Modal)
    {
      OpenDirectoryDialog * Dialog = qobject_cast<OpenDirectoryDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Shown++;
      if (Shown == 1)
      {
        Dialog->DirectoryCombo()->setEditText(QStringLiteral("/missing"));
        Dialog->OkButton()->click();
      }
      else
      {
        Dialog->reject();
      }
      return true;
    };
    Responder.Errors = 0;
    Check(!Window.OpenDirectory() && (Shown == 2) && (Responder.Errors == 1), "asked for again after the error");
    // Ctrl+B: asked, then the dialog with the directory added.
    Responder.LastQuery.clear();
    Responder.Custom = [](QWidget * Modal)
    {
      OpenDirectoryDialog * Dialog = qobject_cast<OpenDirectoryDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Check(Dialog->windowTitle() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ManageBookmarksCaption)),
        "Manage bookmarks");
      Dialog->reject();
      return true;
    };
    Window.OpenDirectory(true);
    Check(!Responder.LastQuery.isEmpty(), "Ctrl+B asks first");
    Check(!CoreBridge::UseSharedBookmarks() && (CoreBridge::Bookmarks(Key).size() == 1) &&
      (CoreBridge::Bookmarks(Key)[0].Remote == "/sub"), "into the site's bookmarks, the directory already one");
    // The local panel's.
    Window.ActivatePanel(true);
    Responder.Custom = [&Directory](QWidget * Modal)
    {
      OpenDirectoryDialog * Dialog = qobject_cast<OpenDirectoryDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Dialog->DirectoryCombo()->setEditText(QDir::toNativeSeparators(Directory));
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.OpenDirectory() && (QDir(Window.Local()->Path()) == QDir(Directory)), "Ctrl+O on the local panel");
    Responder.Custom = nullptr;

    // Location Profiles.
    {
      LocationProfilesDialog Dialog(QDir::toNativeSeparators(Directory), QStringLiteral("/sub"), QStringList(), QStringList(),
        true, Key, false);
      Dialog.Pages()->setCurrentIndex(0);
      Check(!Dialog.AddProfile(0, QStringLiteral("12")), "a number is no profile name");
      Check(Dialog.AddProfile(0, QStringLiteral("work")), "a profile added");
      Check(Dialog.MoveSelectedTo(0, QStringLiteral("projects")) && (Dialog.Tree(0)->topLevelItem(0)->text(0) == QStringLiteral("projects")) &&
        (Dialog.Tree(0)->topLevelItem(0)->child(0)->text(0) == QStringLiteral("work")), "moved into a folder, listed first");
      Dialog.RemoteCombo()->setEditText(QStringLiteral("/"));
      Check(Dialog.Tree(0)->currentItem() == nullptr, "other directories: no profile selected, as upstream's");
      Dialog.Tree(0)->setCurrentItem(Dialog.Tree(0)->topLevelItem(0));
      Check(Dialog.AddProfile(0, QStringLiteral("root")), "another, into the folder selected");
      Check(Dialog.Tree(0)->topLevelItem(0)->childCount() == 2, "the folder has both");
      Check(Dialog.Select(0, QStringLiteral("projects/work")) && (Dialog.RemoteCombo()->currentText() == QStringLiteral("/sub")),
        "choosing one fills both directories in");
      Dialog.MoveSelected(0, 1);
      Check(Dialog.Tree(0)->topLevelItem(0)->child(1)->text(0) == QStringLiteral("work"), "moved down within its folder");
      Dialog.MoveSelected(0, 1);
      Check(Dialog.Tree(0)->topLevelItem(0)->child(1)->text(0) == QStringLiteral("work"), "last already: stays");
      Dialog.MoveSelected(0, -1);
      Check(Dialog.Tree(0)->topLevelItem(0)->child(0)->text(0) == QStringLiteral("work"), "and up again");
      Dialog.Tree(0)->setCurrentItem(Dialog.Tree(0)->topLevelItem(0));
      Check(!Dialog.RenameSelected(0, QStringLiteral("a\\b")) && Dialog.RenameSelected(0, QStringLiteral("clients")),
        "a folder renamed, a backslash refused");
      Check(Dialog.Select(0, QStringLiteral("clients/root")), "its profiles go with it");
      Check(Dialog.RenameSelected(0, QStringLiteral("top")) && Dialog.Select(0, QStringLiteral("clients/top")), "a profile renamed");
      Dialog.reject();
    }
    std::vector<CoreBridge::TBookmarkEntry> Profiles = CoreBridge::Bookmarks(Key);
    const auto FindProfile = [&Profiles](const std::string & Name) -> const CoreBridge::TBookmarkEntry *
    {
      for (const CoreBridge::TBookmarkEntry & Entry : Profiles)
      {
        if (Entry.Name == Name)
        {
          return &Entry;
        }
      }
      return nullptr;
    };
    Check((FindProfile("work") != nullptr) && (FindProfile("work")->Node == "clients") &&
      (FindProfile("work")->Remote == "/sub") && !FindProfile("work")->Local.empty(), "kept with both directories and the folder");
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.UseLocationProfiles = true;
    CoreBridge::SetPreferences(Preferences);
    Remote->ChangeDirectory(QStringLiteral("/"));
    Window.ActivatePanel(false);
    Responder.Custom = [](QWidget * Modal)
    {
      LocationProfilesDialog * Dialog = qobject_cast<LocationProfilesDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      const bool Found = Dialog->Select(0, QStringLiteral("clients/work"));
      Check(Found, "the profile is there");
      Dialog->OkButton()->click();
      return true;
    };
    const bool OpenedProfile = Window.OpenDirectory();
    Check(OpenedProfile && (Remote->Path() == QStringLiteral("/sub")) && (QDir(Window.Local()->Path()) == QDir(Directory)),
      "Ctrl+O opens a profile into both panels");
    // Bookmarks... and back.
    int Profile = 0;
    int Bookmark = 0;
    Responder.Custom = [&Profile, &Bookmark](QWidget * Modal)
    {
      if (LocationProfilesDialog * Dialog = qobject_cast<LocationProfilesDialog *>(Modal))
      {
        Profile++;
        for (QPushButton * Button : Dialog->findChildren<QPushButton *>())
        {
          if (Button->text() == LocationProfilesDialog::tr("&Bookmarks..."))
          {
            Button->click();
            return true;
          }
        }
        Dialog->reject();
        return true;
      }
      if (OpenDirectoryDialog * Dialog = qobject_cast<OpenDirectoryDialog *>(Modal))
      {
        Bookmark++;
        Dialog->reject();
        return true;
      }
      return false;
    };
    Check(!Window.OpenDirectory() && (Profile == 1) && (Bookmark == 1) && !CoreBridge::Preferences().UseLocationProfiles,
      "Bookmarks... switches to the Open directory dialog, from now on");
    Responder.Custom = nullptr;

    // Find files.
    {
      QFile File(QDir(Directory).filePath(QStringLiteral("found.log")));
      File.open(QIODevice::WriteOnly);
      File.write("log\n");
    }
    Window.Local()->Refresh();
    Check(Window.Upload({ QStringLiteral("found.log") }, QStringLiteral("/sub"), false), "a file to find, down a directory");
    QFile::remove(QDir(Directory).filePath(QStringLiteral("found.log")));
    Remote->ChangeDirectory(QStringLiteral("/"));
    FindFilesDialog * Find = new FindFilesDialog(&Window, Remote->Path());
    Find->show();
    Find->MaskCombo()->setEditText(QStringLiteral("*.log"));
    Find->Start();
    Check((Find->Results()->topLevelItemCount() == 1) && (Find->Results()->topLevelItem(0)->text(0) == QStringLiteral("found.log")) &&
      (Find->Results()->topLevelItem(0)->text(1) == QStringLiteral("/sub/")), "found below, with its directory");
    Check(Find->Status()->text() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FindFileDone)), "and done");
    Check(FileView::LastMask() == QStringLiteral("*.log"), "the mask kept, as Select's");
    Find->Results()->topLevelItem(0)->setSelected(true);
    Find->CopyResults();
    Check(QApplication::clipboard()->text() == QStringLiteral("/sub/found.log"), "results copied as paths");
    Find->Focus();
    Check((Remote->Path() == QStringLiteral("/sub")) && Remote->Selected().contains(QStringLiteral("found.log")), "focused in the panel");
    Remote->ChangeDirectory(QStringLiteral("/"));
    Responder.AcceptDialogs = true;
    Find->Download();
    Check(QFile::exists(QDir(Directory).filePath(QStringLiteral("found.log"))), "downloaded from where it was found");
    Find->Delete();
    Responder.AcceptDialogs = false;
    Check(Find->Results()->topLevelItemCount() == 0, "deleted, off the list");
    Remote->ChangeDirectory(QStringLiteral("/sub"));
    Check(!Remote->Names().contains(QStringLiteral("found.log")), "and off the server");
    Responder.Errors = 0;
    Find->MaskCombo()->setEditText(QStringLiteral("*.log>xyz"));
    Find->Start();
    Check((Responder.Errors == 1) &&
      (Find->Status()->text() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FindFileAborted))), "a mask that is not one");
    Find->close();

    // Edit file mask: a mask in its parts, and back.
    {
      EditMaskDialog Dialog(QStringLiteral("*.txt; *.log | old/"));
      Check((Dialog.IncludeFiles()->toPlainText() == QStringLiteral("*.txt\n*.log")) &&
        Dialog.ExcludeDirectories()->toPlainText().contains(QStringLiteral("old")), "split into its parts");
      Check(Dialog.Mask() == QString::fromStdString(CoreBridge::ComposeMask(CoreBridge::SplitMask("*.txt; *.log | old/"))),
        "and put back as core does");
      Dialog.ExcludeAllDirectoriesCheck()->setChecked(true);
      Check(Dialog.ExcludeDirectories()->toPlainText() == QStringLiteral("*"), "all directories excluded");
      Dialog.ExcludeAllDirectoriesCheck()->setChecked(false);
      Check(Dialog.ExcludeDirectories()->toPlainText().contains(QStringLiteral("old")), "and back as they were");
      Dialog.IncludeFiles()->setPlainText(QStringLiteral("*.txt>xyz"));
      Check(!Dialog.OkButton()->isEnabled() && !Dialog.Preview()->toPlainText().isEmpty(), "not a mask: core's error, no OK");
      Dialog.Clear();
      Check(Dialog.OkButton()->isEnabled() && Dialog.Mask().isEmpty(), "cleared");
    }

    // The filter.
    for (const char * Name : { "a.txt", "b.dat" })
    {
      QFile File(QDir(Directory).filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    QDir(Directory).mkpath(QStringLiteral("folder"));
    Window.Local()->Refresh();
    Window.ActivatePanel(true);
    QString FilterMask = QStringLiteral("*.txt");
    Responder.Custom = [&FilterMask](QWidget * Modal)
    {
      QDialog * Dialog = qobject_cast<QDialog *>(Modal);
      if ((Dialog == nullptr) || (Dialog->windowTitle() != QString::fromStdString(CoreBridge::Text(CoreBridge::TText::FilterMaskCaption))))
      {
        return false;
      }
      Dialog->findChild<QLineEdit *>()->setText(FilterMask);
      Dialog->accept();
      return true;
    };
    Check(Window.Filter(), "Ctrl+Alt+F filters the panel");
    const QStringList Filtered = Window.Local()->Names();
    Check(Filtered.contains(QStringLiteral("a.txt")) && !Filtered.contains(QStringLiteral("b.dat")) &&
      Filtered.contains(QStringLiteral("folder")), "what matches, and directories, show");
    Check(Window.Local()->Filter() == QStringLiteral("*.txt"), "the panel's filter");
    FilterMask = QStringLiteral("*.*");
    Check(Window.Filter() && Window.Local()->Filter().isEmpty() && Window.Local()->Names().contains(QStringLiteral("b.dat")),
      "*.* is no filter");
    Responder.Custom = nullptr;
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

void Tools(const char * Port, const QString & Directory)
{
  Q_UNUSED(Directory);
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
    CoreBridge::TSession * Session = Window.Session();

    // Server and protocol information.
    const CoreBridge::TServerInfo Info = Session->ServerInfo();
    bool Protocol = false;
    for (const auto & Row : Info.Server)
    {
      Protocol = Protocol || (Row.second.find("SFTP") != std::string::npos);
    }
    Check(Protocol, "the protocol, as core names it");
    Check(!Info.HostKeySha256.empty() && !Info.HostKeyMd5.empty() && !Info.HostKeyAlgorithm.empty() &&
      (Info.HostKeySha256.find(Info.HostKeyAlgorithm) == std::string::npos), "the host key's algorithm apart from its fingerprints");
    Check(Info.Capabilities.size() == 10, "WinSCP's capabilities");
    {
      ServerInfoDialog Dialog(Session, Info, QStringLiteral("/"));
      Check(Dialog.ServerView()->topLevelItemCount() >= 3, "the server's rows shown");
      Check(Dialog.HostKeySha256Edit()->text() == QString::fromStdString(Info.HostKeySha256), "the fingerprint");
      const QString Text = Dialog.ClipboardText();
      Check(Text.contains(QStringLiteral("SHA-256 = ")) && Text.contains(QString(60, QLatin1Char('-'))), "the clipboard's text, WinSCP's");
      if (Info.CanCheckSpace)
      {
        Check(Dialog.CheckSpace() && (Dialog.SpaceView()->topLevelItemCount() == 5), "the space available");
      }
      else
      {
        Check(!Dialog.Pages()->isTabEnabled(2), "no space to check: its page off");
      }
    }

    // Generate URL and code.
    const QString Url = QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port));
    {
      GenerateUrlDialog Dialog(Session, {});
      Dialog.Pages()->setCurrentIndex(0);
      Check(Dialog.windowTitle() == QStringLiteral("Generate session URL/code"), "WinSCP's title");
      Check(Dialog.Text().startsWith(QStringLiteral("sftp://linscp@127.0.0.1:")), "the session's URL, with the user");
      Dialog.Check(1)->setChecked(false);
      Check(Dialog.Text().startsWith(QStringLiteral("sftp://127.0.0.1:")) && !Dialog.Check(2)->isEnabled(), "without: no password either");
      Dialog.Check(1)->setChecked(true);
      Dialog.Check(4)->setChecked(true);
      Check(Dialog.Text().contains(QStringLiteral("fingerprint=")), "the host key, WinSCP's URL parameter");
      Dialog.Pages()->setCurrentIndex(1);
      Dialog.ScriptFormatCombo()->setCurrentIndex(0);
      Check(Dialog.Text().startsWith(QStringLiteral("open sftp://")) && Dialog.Text().contains(QStringLiteral("\nexit\n")) &&
        Dialog.Text().contains(QStringLiteral("linscp-cli")), "a script for the command-line client");
      Check(Dialog.Result()->toHtml().contains(QStringLiteral("https://winscp.net/eng/docs/scriptcommand_open")),
        "its commands linked to WinSCP's documentation");
      Dialog.ScriptFormatCombo()->setCurrentIndex(2);
      Check(Dialog.Text().contains(QStringLiteral("/command")), "command-line arguments");
      Dialog.Pages()->setCurrentIndex(2);
      Dialog.LanguageCombo()->setCurrentIndex(0);
      Check(Dialog.Text().contains(QStringLiteral("SessionOptions")), ".NET code");
      Dialog.reject();
    }
    Check((CoreBridge::GenerateCodeSettings().Target == CoreBridge::TGenerateCodeOptions::TTarget::Assembly) &&
      (CoreBridge::GenerateCodeSettings().ScriptFormat == 2), "the choices kept");
    {
      GenerateUrlDialog Dialog(Session, { "/hello.txt" });
      Check((Dialog.Pages()->count() == 1) && (Dialog.windowTitle() == QStringLiteral("Generate file URL")), "a file's URL only");
      Check(Dialog.Text().endsWith(QStringLiteral("/hello.txt")), "the file's URL");
    }
    // Commands > Open in PuTTY: the program Preferences name, given the
    // session on its command line (POSIX) or as a PuTTY session (Windows).
    {
#if defined(Q_OS_WIN)
      const QString Program = QDir::toNativeSeparators(QDir(Directory).filePath(QStringLiteral("putty.exe")));
#else
      const QString Program = QDir(Directory).filePath(QStringLiteral("putty"));
#endif
      {
        QFile File(Program);
        File.open(QIODevice::WriteOnly);
      }
      const CoreBridge::TPreferences Saved = CoreBridge::Preferences();
      Check(!Saved.PuttyPath.empty(), "a default PuTTY command");
      CoreBridge::TPreferences Preferences = Saved;
      Preferences.PuttyPath = QStringLiteral("\"%1\" -extra").arg(Program).toStdString();
      CoreBridge::SetPreferences(Preferences);
      QString Started;
      QString Params;
      Frontend.SetStartProgram([&Started, &Params](const QString & AProgram, const QString & AParams, const QString &)
      {
        Started = AProgram;
        Params = AParams;
        return true;
      });
      Check(Window.OpenInPutty() && (QDir::fromNativeSeparators(Started) == QDir::fromNativeSeparators(Program)),
        "PuTTY started");
#if defined(Q_OS_WIN)
      Check(Params.contains(QStringLiteral("-load ")) && Params.endsWith(QStringLiteral("-extra")),
        "with the session exported to PuTTY's, and the command's own parameters");
#else
      Check(Params.startsWith(QStringLiteral("127.0.0.1 -l linscp -P %1").arg(QString::fromLatin1(Port))) &&
        Params.endsWith(QStringLiteral("-extra")), "with the session on its command line, and the command's own parameters");
#endif
      // A command with the session's patterns gets no PuTTY options.
      Preferences.PuttyPath = QStringLiteral("\"%1\" !U@!@:!#").arg(Program).toStdString();
      CoreBridge::SetPreferences(Preferences);
      Check(Window.OpenInPutty() && (Params == QStringLiteral("linscp@127.0.0.1:%1").arg(QString::fromLatin1(Port))),
        "a site command, its patterns completed");
      const int Errors = Responder.Errors;
      Preferences.PuttyPath = QStringLiteral("\"%1\"").arg(QDir(Directory).filePath(QStringLiteral("missing/putty"))).toStdString();
      CoreBridge::SetPreferences(Preferences);
      Check(!Window.OpenInPutty() && (Responder.Errors == Errors + 1), "a program not found: an error");
      Frontend.SetStartProgram(nullptr);
      CoreBridge::SetPreferences(Saved);
    }

    // The console over SFTP: a separate shell session, asked first (rclone
    // has no shell, so it fails to open).
    Check(!Session->CanRunCommands() && Session->CanOpenShellSession(), "SFTP needs a shell session for commands");
    Responder.LastQuery.clear();
    Responder.Errors = 0;
    Check(!Window.OpenConsole() && Responder.LastQuery.contains(QStringLiteral("shell")) && (Responder.Errors == 1),
      "asked first, and the shell's failure shown");
    Check(RtfToText(QStringLiteral("a\\\\b\\{c\\}\\par\nd\\u228?e")) == QStringLiteral("a\\b{c}\nd\u00e4e"), "RTF escapes as text");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// HostDirectory: Directory as the server sees it, the host's path (under
// Wine, Directory is a Windows one).
void Commands(const char * Port, const QString & Key, const QString & User, const QString & Directory,
  const QString & HostDirectory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    // WinSCP's defaults, before anything is opened.
    const std::vector<CoreBridge::TCustomCommandDefinition> Defaults = CoreBridge::CustomCommands();
    Check(Defaults.size() == 6, "WinSCP's six commands by default");
    bool Local = false;
    for (const auto & Command : Defaults)
    {
      Local = Local || Command.Local;
    }
    Check(Local, "one of them local");
    Check(!Window.FileCommandsMenu()->actions().isEmpty() && !Window.StaticCommandsMenu()->actions().isEmpty(),
      "the file and static commands' menus");
    // The dialog's checks.
    {
      CustomCommandDialog Dialog(CustomCommandDialog::TMode::Add, CoreBridge::TCustomCommandDefinition(), Defaults);
      Dialog.DescriptionEdit()->setText(QString::fromStdString(Defaults[0].Name));
      Dialog.CommandCombo()->setEditText(QStringLiteral("echo !"));
      Responder.Errors = 0;
      Check(!Dialog.Validate() && (Responder.Errors == 1), "a description already used refused");
      Dialog.DescriptionEdit()->setText(QStringLiteral("a=b"));
      Check(!Dialog.Validate(), "and one with =");
      Dialog.DescriptionEdit()->setText(QStringLiteral("Mine"));
      Dialog.CommandCombo()->setEditText(QStringLiteral("echo !?unterminated"));
      Check(!Dialog.Validate(), "an unterminated pattern refused");
      Dialog.CommandCombo()->setEditText(QStringLiteral("echo !&"));
      Check(Dialog.Validate() && !Dialog.RecursiveCheck()->isEnabled() && Dialog.ApplyToDirectoriesCheck()->isEnabled(),
        "a file list: no recursion");
      Dialog.CommandCombo()->setEditText(QStringLiteral("uptime"));
      Check(!Dialog.ApplyToDirectoriesCheck()->isEnabled(), "not a file command: not for directories");
    }

    CoreBridge::TSite Site = CoreBridge::NewSite();
    Site.Protocol = CoreBridge::TSite::TProtocol::Scp;
    Site.HostName = "127.0.0.1";
    Site.Port = QString::fromLatin1(Port).toInt();
    Site.UserName = User.toStdString();
    // The fixture's OpenSSH key, converted as the site manager would.
    const std::string Ppk = QDir(Directory).filePath(QStringLiteral("client.ppk")).toStdString();
    try
    {
      CoreBridge::ConvertKey(Key.toStdString(), Ppk);
    }
    catch (const CoreBridge::TCoreError &)
    {
      Check(false, "the key converts");
    }
    Site.PrivateKeyFile = Ppk;
    Check(Window.OpenSite(Site), "an SCP session over sshd");
    RemotePanel * Remote = Window.Panel();
    // Only the test's directory: commands run there.
    const QString Work = QDir(Directory).filePath(QStringLiteral("commands"));
    QDir().mkpath(Work);
    {
      QFile File(QDir(Work).filePath(QStringLiteral("data.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("12345");
    }
    Check(Remote->ChangeDirectory(HostDirectory + QStringLiteral("/commands")), "in the test's directory");
    Check(Window.Session()->CanRunShellCommands(), "SCP runs commands in its shell");

    CoreBridge::TCustomCommandDefinition Static;
    Static.Name = "Static";
    Static.Command = "echo static";
    Static.CopyResults = true;
    Check(Window.RunCustomCommand(Static) && (QApplication::clipboard()->text() == QStringLiteral("static")),
      "a static command run, its output copied");
    CoreBridge::TCustomCommandDefinition Count;
    Count.Name = "Count";
    Count.Command = "wc -c \"!\"";
    Count.CopyResults = true;
    Remote->Select({ QStringLiteral("data.txt") });
    Check(Window.RunCustomCommand(Count) && QApplication::clipboard()->text().startsWith(QStringLiteral("5")),
      "a file command on the selected file");
    Responder.Custom = [](QWidget * Modal)
    {
      QDialog * Dialog = qobject_cast<QDialog *>(Modal);
      if ((Dialog == nullptr) || !Dialog->windowTitle().contains(QStringLiteral("Prompted")))
      {
        return false;
      }
      Dialog->findChild<QLineEdit *>()->setText(QStringLiteral("typed"));
      Dialog->accept();
      return true;
    };
    CoreBridge::TCustomCommandDefinition Prompted;
    Prompted.Name = "Prompted";
    Prompted.Command = "echo \"!?&Say:?default!\"";
    Prompted.CopyResults = true;
    Check(Window.RunCustomCommand(Prompted) && (QApplication::clipboard()->text() == QStringLiteral("typed")),
      "its parameter asked first");
    Responder.Custom = nullptr;
    CoreBridge::TCustomCommandDefinition Executed;
    Executed.Name = "Executed";
    Executed.Command = "echo !`echo here`";
    Executed.CopyResults = true;
    Check(Window.RunCustomCommand(Executed) && (QApplication::clipboard()->text() == QStringLiteral("here")),
      "a local command's output in it");
    // Enter...: ad hoc, then Last.
    Responder.Custom = [](QWidget * Modal)
    {
      CustomCommandDialog * Dialog = qobject_cast<CustomCommandDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Check(Dialog->DescriptionEdit()->isHidden() && Dialog->ShortCutCombo()->isHidden(), "ad hoc: no description, no key");
      Dialog->CommandCombo()->setEditText(QStringLiteral("touch adhoc.txt"));
      Dialog->OkButton()->click();
      return true;
    };
    Check(Window.EnterCustomCommand(false) && QFileInfo::exists(QDir(Work).filePath(QStringLiteral("adhoc.txt"))),
      "an ad hoc command run");
    Responder.Custom = nullptr;
    bool Last = false;
    for (QAction * Action : Window.StaticCommandsMenu()->actions())
    {
      Last = Last || Action->text().contains(QStringLiteral("touch adhoc.txt"));
    }
    Check(Last, "and kept as Last");
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

    // Help > About: the program, and WinSCP it is based on.
    {
      AboutDialog About(CoreBridge::Info(), &Window);
      const CoreBridge::TInfo Info = CoreBridge::Info();
      Check(About.NameLabel()->text() == QString::fromStdString(Info.AppName), "About names the program");
      Check(About.VersionLabel()->text().contains(QString::fromStdString(Info.Version)), "its version");

      Check(About.WinScpLabel()->text().contains(QStringLiteral("https://winscp.net/")) &&
        About.WinScpLabel()->text().contains(QString::fromStdString(Info.WinScpVersion)) && (Info.WinScpVersion != Info.Version),
        "and WinSCP's, with its site");
      Check(!QIcon(QStringLiteral(":/icons/linscp.ico")).pixmap(64, 64).isNull(), "with its icon");
      const QString ThirdParty = About.ThirdParty()->toPlainText();
      Check(ThirdParty.contains(QStringLiteral("PuTTY")) && ThirdParty.contains(QStringLiteral("OpenSSL")) &&
        ThirdParty.contains(QStringLiteral("eXpat")) && ThirdParty.contains(QStringLiteral("Qt ")),
        "WinSCP's third-party list, with Qt");
      Check(About.ThirdParty()->toHtml().contains(QString::fromLatin1(CoreBridge::ExpatLicenseUrl)), "Expat's license linked");
      Check(About.LicenseButton() != nullptr, "and a License button");
    }
    {
      LicenseDialog Product(LicenseDialog::Product);
      Check(Product.windowTitle() == QString::fromStdString(CoreBridge::Info().AppName) + QStringLiteral(" license") &&
        Product.Text()->toPlainText().contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")), "the License dialog: linscp's GPL");
      LicenseDialog Expat(LicenseDialog::Expat);
      Check((Expat.windowTitle() == QStringLiteral("Expat license")) &&
        Expat.Text()->toPlainText().contains(QStringLiteral("Permission is hereby granted")), "and Expat's, as WinSCP ships it");
    }

    Responder.Passwords.clear();
    Check(!Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))),
      "a cancelled password prompt does not connect");
    Check(Responder.Errors == 1, "and core's error is shown");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

}  // namespace

// Commands > Keep Remote Directory up to Date: the dialog, and upstream's
// controller watching the local directory and uploading what changes to
// rclone's tree ({tmp}/root), deleting what goes.
void KeepUpToDateTest(const char * Port, const QString & Tmp)
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
    CoreBridge::TSession * Session = Window.Session();
    const QDir Local(QDir(Tmp).filePath(QStringLiteral("local")));
    const QDir Remote(QDir(Tmp).filePath(QStringLiteral("root/sub")));
    const auto Write = [](const QString & Path, const QByteArray & Data)
    {
      QFile File(Path);
      File.open(QIODevice::WriteOnly);
      File.write(Data);
    };
    const auto WaitFor = [](const std::function<bool()> & Condition)
    {
      QElapsedTimer Timer;
      Timer.start();
      while (!Condition() && (Timer.elapsed() < 15000))
      {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
      }
      return Condition();
    };
    const auto Logged = [](KeepUpToDateDialog & Dialog, const QString & Text)
    {
      for (int Index = 0; Index < Dialog.Log()->topLevelItemCount(); Index++)
      {
        if (Dialog.Log()->topLevelItem(Index)->text(1).contains(Text))
        {
          return true;
        }
      }
      return false;
    };
    Write(Local.filePath(QStringLiteral("first.txt")), "first");

    CoreBridge::TKeepUpToDateParams Params = CoreBridge::KeepUpToDateDefaults();
    Params.LocalDirectory = QDir::toNativeSeparators(Local.path()).toStdString();
    Params.RemoteDirectory = "/sub";
    Params.Delete = true;
    Params.Recurse = true;
    Params.SynchronizeFirst = CoreBridge::TKeepUpToDateParams::TSynchronizeFirst::Yes;
    {
      KeepUpToDateDialog Dialog(*Session, Params, {}, &Window);
      Dialog.show();
      Check(Dialog.windowTitle() == QStringLiteral("Keep remote directory up to date"), "WinSCP's title");
      Check(!Dialog.SelectedOnlyCheck()->isEnabled(), "nothing selected: no \"selected files only\"");
      Check(Dialog.DeleteCheck()->isChecked() && Dialog.RecurseCheck()->isChecked() &&
        (Dialog.SynchronizeCheck()->checkState() == Qt::Checked), "the params shown");
      Check(Dialog.Start(), "it starts");
      Check(Dialog.Synchronizing() && Dialog.StopButton()->isVisible() && !Dialog.StartButton()->isVisible() &&
        !Dialog.CloseButton()->isVisible(), "Stop in Start's place, no Close");
      Check(Dialog.windowTitle() == QStringLiteral("Keeping remote directory up to date ..."), "the title says so");
      Check(QFileInfo::exists(Remote.filePath(QStringLiteral("first.txt"))), "synchronized on start: the file is up");
      Check(Logged(Dialog, QStringLiteral("Watching for changes")), "the log says what is watched");

      Write(Local.filePath(QStringLiteral("second.txt")), "second");
      Check(WaitFor([&Remote]() { return QFileInfo::exists(Remote.filePath(QStringLiteral("second.txt"))); }),
        "a new file is uploaded");
      Check(WaitFor([&]() { return Logged(Dialog, QStringLiteral("uploaded")); }), "and logged");
      Local.mkdir(QStringLiteral("deeper"));
      Write(Local.filePath(QStringLiteral("deeper/third.txt")), "third");
      Check(WaitFor([&Remote]() { return QFileInfo::exists(Remote.filePath(QStringLiteral("deeper/third.txt"))); }),
        "a new subdirectory's file too");
      QFile::remove(Local.filePath(QStringLiteral("first.txt")));
      Check(WaitFor([&Remote]() { return !QFileInfo::exists(Remote.filePath(QStringLiteral("first.txt"))); }),
        "a deleted file is deleted");
      Check(WaitFor([&]() { return Logged(Dialog, QStringLiteral("deleted")); }), "and logged");
      Dialog.Stop();
      Check(!Dialog.Synchronizing() && !Session->KeepingUpToDate() && Dialog.StartButton()->isVisible(), "Stop stops");
      Write(Local.filePath(QStringLiteral("after.txt")), "after");
      QElapsedTimer Timer;
      Timer.start();
      while (Timer.elapsed() < 1500)
      {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      Check(!QFileInfo::exists(Remote.filePath(QStringLiteral("after.txt"))), "and nothing is uploaded after");

      // Grayed "synchronize on start": asked, never again.
      Dialog.SynchronizeCheck()->setCheckState(Qt::PartiallyChecked);
      QString Asked;
      Responder.Custom = [&Asked](QWidget * Modal)
      {
        QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
        if ((Query == nullptr) || (Query->NeverAskAgain() == nullptr))
        {
          return false;
        }
        for (const QLabel * Label : Query->findChildren<QLabel *>())
        {
          Asked += Label->text();
        }
        Query->NeverAskAgain()->setChecked(true);
        Query->Buttons()[0]->click();
        return true;
      };
      Check(Dialog.Start(), "it starts again");
      Responder.Custom = nullptr;
      Check(Asked.contains(QStringLiteral("synchronisation")), "after WinSCP's question");
      Check(Dialog.SynchronizeCheck()->checkState() == Qt::Checked, "never again: the box checked");
      Check(QFileInfo::exists(Remote.filePath(QStringLiteral("after.txt"))), "and synchronized first");

      // The directory gone: the watch fails, and stops with an error.
      const int Errors = Responder.Errors;
      QDir(Local.filePath(QStringLiteral("deeper"))).removeRecursively();
      Check(WaitFor([&Remote]() { return !QFileInfo::exists(Remote.filePath(QStringLiteral("deeper"))); }),
        "a deleted directory is deleted");
      QDir(Local.path()).removeRecursively();
      // Wine refuses to remove a directory a handle is open on (Windows
      // marks it to go once closed): there the watch loses nothing.
      if (!QFileInfo::exists(Local.path()))
      {
        Check(WaitFor([&Dialog]() { return !Dialog.Synchronizing(); }), "the watched directory gone: it stops");
        Check(Responder.Errors > Errors, "with an error");
      }
      Dialog.Stop();
      QDir().mkpath(Local.path());

      // Its settings, kept "next time".
      CoreBridge::TKeepUpToDateParams Kept = Dialog.Params();
      CoreBridge::SaveKeepUpToDateDefaults(Kept, true);
      const CoreBridge::TKeepUpToDateParams Saved = CoreBridge::KeepUpToDateDefaults();
      Check(Saved.Delete && Saved.Recurse && (Saved.SynchronizeFirst == CoreBridge::TKeepUpToDateParams::TSynchronizeFirst::Yes),
        "the options kept");
    }
    {
      // "Never ask again" without "use same options" still saves Yes.
      Params.SynchronizeFirst = CoreBridge::TKeepUpToDateParams::TSynchronizeFirst::Ask;
      CoreBridge::SaveKeepUpToDateDefaults(Params, true);
      Params.SynchronizeFirst = CoreBridge::TKeepUpToDateParams::TSynchronizeFirst::Yes;
      Params.Delete = false;
      CoreBridge::SaveKeepUpToDateDefaults(Params, false);
      const CoreBridge::TKeepUpToDateParams Saved = CoreBridge::KeepUpToDateDefaults();
      Check((Saved.SynchronizeFirst == CoreBridge::TKeepUpToDateParams::TSynchronizeFirst::Yes) && Saved.Delete,
        "an answered question remembered, nothing else");
    }

    // From the menu: the panels' directories, and what is selected.
    Window.Local()->ChangeDirectory(Local.path());
    Window.Panel()->ChangeDirectory(QStringLiteral("/sub"));
    bool Shown = false;
    QTimer::singleShot(0, &Window, [&Window, &Shown]()
    {
      KeepUpToDateDialog * Dialog = Window.KeepUpToDateWindow();
      Shown = (Dialog != nullptr) && (Dialog->RemoteEdit()->text() == QStringLiteral("/sub"));
      if (Dialog != nullptr)
      {
        Dialog->CloseButton()->click();
      }
    });
    Check(Window.KeepUpToDate() && Shown, "Commands > Keep Remote Directory up to Date opens it on the panels' directories");
  }
  CoreBridge::Finalize();
}

// S3's tags, in the Properties dialog's Tags page, against the suite's S3
// server (its bucket "bucket").
void Tags(const char * Port)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    CoreBridge::TSite Site;
    Site.Protocol = CoreBridge::TSite::TProtocol::S3;
    Site.HostName = "127.0.0.1";
    Site.Port = QString::fromLatin1(Port).toInt();
    Site.UserName = "linscp";
    Site.Password = "secret";
    // Plain HTTP, and path-style URLs, as an IP address needs.
    Site.HasRawSettings = true;
    Site.RawSettings = "Ftps=0\nS3UrlStyle=1";
    Check(Window.OpenSite(Site), "an S3 session");
    CoreBridge::TSession * Session = Window.Session();
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/bucket")), "in the bucket");
    try
    {
      CoreBridge::TPropertiesInfo Info = Session->PropertiesOf({ "hello.txt" });
      Check(Info.ShowTags && Info.Properties.HasTags && Info.Properties.Tags.empty(), "a file has a Tags page, no tags yet");
      Check(!Session->PropertiesOf({ "sub" }).ShowTags, "a folder none");
      CoreBridge::TFileProperties Properties = Info.Properties;
      Properties.Tags = { { "env", "test" }, { "owner", "linscp" } };
      Check(Session->SetProperties({ "hello.txt" }, Properties), "tags set");
      Info = Session->PropertiesOf({ "hello.txt" });
      Check((Info.Properties.Tags.size() == 2) && (Info.Properties.Tags[1].first == "owner") &&
        (Info.Properties.Tags[1].second == "linscp"), "and read back from the server");

      PropertiesDialog Dialog(Session, { "hello.txt" }, QStringLiteral("/bucket"), Info, &Window);
      Check((Dialog.TagList() != nullptr) && (Dialog.TagList()->topLevelItemCount() == 2), "the Tags page lists them");
      Check(!Dialog.OkButton()->isEnabled() && !Dialog.EditTagButton()->isEnabled(), "nothing changed, nothing selected");
      Dialog.TagList()->setCurrentItem(Dialog.TagList()->topLevelItem(0));
      Check(Dialog.EditTagButton()->isEnabled() && Dialog.RemoveTagButton()->isEnabled(), "a tag selected: edit, remove");
      Dialog.RemoveTag();
      Check(Dialog.OkButton()->isEnabled() && (Dialog.Properties().Tags.size() == 1), "a tag removed: OK");
      Responder.Custom = [](QWidget * Modal)
      {
        TagDialog * Tag = qobject_cast<TagDialog *>(Modal);
        if (Tag == nullptr)
        {
          return false;
        }
        Check(Tag->windowTitle() == QStringLiteral("Add tag"), "WinSCP's Add tag");
        Tag->KeyEdit()->setText(QStringLiteral("new"));
        Tag->ValueEdit()->setText(QStringLiteral("value"));
        Tag->OkButton()->click();
        return true;
      };
      Check(Dialog.AddEditTag(true) && (Dialog.TagList()->topLevelItemCount() == 2), "a tag added");
      Responder.Custom = nullptr;
      Check(Session->SetProperties({ "hello.txt" }, Dialog.Properties()), "and saved");
      Info = Session->PropertiesOf({ "hello.txt" });
      Check((Info.Properties.Tags.size() == 2) && (Info.Properties.Tags[0].first == "owner") &&
        (Info.Properties.Tags[1].first == "new"), "as the dialog had them");
    }
    catch (const CoreBridge::TCoreError & E)
    {
      std::printf("error: %s\n", E.Error.Message.c_str());
      Check(false, "no core error");
    }
    {
      // The key's checks (TTagDialog::DoValidate).
      TagDialog Dialog(true, { QStringLiteral("env") }, QStringLiteral("env"), QString(), &Window);
      const int Errors = Responder.Errors;
      Dialog.accept();
      Check((Dialog.result() != QDialog::Accepted) && (Responder.Errors == Errors + 1), "a key taken: an error, and the dialog stays");
      Dialog.KeyEdit()->setText(QString());
      Check(!Dialog.OkButton()->isEnabled(), "no key, no OK");
      Dialog.KeyEdit()->setText(QStringLiteral("a!b"));
      QString Asked;
      Responder.Custom = [&Asked](QWidget * Modal)
      {
        QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
        if (Query == nullptr)
        {
          return false;
        }
        for (const QLabel * Label : Query->findChildren<QLabel *>())
        {
          Asked += Label->text();
        }
        Query->Buttons()[1]->click();
        return true;
      };
      Dialog.accept();
      Check(Asked.contains(QStringLiteral("\"!\"")) && (Dialog.result() != QDialog::Accepted), "a character S3 does not allow: asked, and aborted");
      Responder.Custom = nullptr;
      Dialog.accept();
      Check(Dialog.result() == QDialog::Accepted, "or ignored");
    }
  }
  CoreBridge::Finalize();
}

// The command line's session, as WinSCP.exe's: linscp sftp://... opens it,
// in the URL's directory, with no site manager.
void StartUpTest(const char * Port)
{
  CoreBridge::SetArguments({ "/ini=nul", QStringLiteral("sftp://linscp@127.0.0.1:%1/sub/").arg(QString::fromLatin1(Port)).toStdString() });
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = { "secret" };
    bool SiteManagerShown = false;
    Responder.Custom = [&SiteManagerShown](QWidget * Modal)
    {
      if (qobject_cast<SiteManager *>(Modal) == nullptr)
      {
        return false;
      }
      SiteManagerShown = true;
      static_cast<QDialog *>(Modal)->reject();
      return true;
    };
    bool CanOpen = false;
    Check(CoreBridge::TSession::FromCommandLine(CanOpen) && CanOpen, "the command line names a session that can open");
    Window.StartUp();
    Check((Window.Session() != nullptr) && Window.Session()->Active(), "it is opened at start");
    Check(Window.Panel()->Path() == QStringLiteral("/sub"), "in the URL's directory");
    Check(!SiteManagerShown && (Responder.PasswordPrompts == 1), "asked only the password, no site manager");
    // Go To > Explore Directory: the local directory, to the desktop.
    QUrl Opened;
    Window.SetUrlOpener([&Opened](const QUrl & Url) { Opened = Url; return true; });
    Check(Window.ExploreLocalDirectory() && Opened.isLocalFile() &&
      (QDir(Opened.toLocalFile()) == QDir(Window.Local()->Path())), "Explore Directory opens the local directory");
  }
  CoreBridge::Finalize();
}

int main(int argc, char ** argv)
{
  QApplication App(argc, argv);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if ((argc >= 2) && (std::strcmp(argv[1], "dialogs") == 0))
  {
    Dialogs();
  }
  else if ((argc >= 3) && (std::strcmp(argv[1], "language") == 0))
  {
    Language(QString::fromLocal8Bit(argv[2]));
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
  else if ((argc >= 5) && (std::strcmp(argv[1], "preferences") == 0))
  {
    // argv[2] is /ini=<file>.
    Preferences(QString::fromLocal8Bit(argv[2] + 5), QString::fromLocal8Bit(argv[3]), QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "keys") == 0))
  {
    Keys(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "sync") == 0))
  {
    Synchronization(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "drop") == 0))
  {
    DragAndDrop(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 7) && (std::strcmp(argv[1], "commands") == 0))
  {
    Commands(argv[3], QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[5]), QString::fromLocal8Bit(argv[6]),
      QString::fromLocal8Bit((argc >= 8) ? argv[7] : argv[6]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "startup") == 0))
  {
    StartUpTest(argv[3]);
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "tags") == 0))
  {
    Tags(argv[3]);
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "keep") == 0))
  {
    KeepUpToDateTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "tools") == 0))
  {
    Tools(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "bookmarks") == 0))
  {
    BookmarksTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "properties") == 0))
  {
    Properties(argv[3]);
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "edit") == 0))
  {
    Editing(argv[3], QString::fromLocal8Bit(argv[4]));
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
