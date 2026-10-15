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
#include <QSystemTrayIcon>
#include <QProcess>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QClipboard>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTest>
#include <QSettings>
#include <QSplitter>
#include <QListWidget>
#include <algorithm>
#include <set>
#include <QComboBox>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
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
#include "Theme.h"
#include "ExtensionOptionsDialog.h"
#include "QueuePanel.h"
#include "TransferDialog.h"

#include <functional>
#include "RemotePanel.h"

#include <QDir>
#include <QElapsedTimer>
#include <cmath>
#include <QFileInfo>
#include "SiteAdvanced.h"

#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include "SiteManager.h"
#include "CopyLocalDialog.h"
#include "AccessibilityAudit.h"
#include "EditorPreferencesDialog.h"
#include "SaveWorkspaceDialog.h"
#include "SaveSiteDialog.h"
#include "IconSet.h"
#include "PreferencesDialog.h"
#include "Integration.h"
#include "DirectoryTree.h"
#include <QSettings>
#include "SynchronizeDialog.h"
#include "TransferSettingsDialog.h"
#include "SingleInstance.h"
#include "MasterPasswordDialog.h"
#include "SshHostCADialog.h"
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
#include <QTabBar>
#include "KeepUpToDateDialog.h"
#include <QThread>

#include <QPlainTextEdit>
#include <QTextBlock>

#include <QFile>

#include <QComboBox>
#include <QSpinBox>
#include <QTreeWidget>

// What the thumbnails' delegate asks of each entry as it paints: its image's
// file and, for that, its preview.
void RequestPreviews(ThumbnailView * View)
{
  for (int Row = 0; Row < View->model()->rowCount(); Row++)
  {
    const CoreBridge::TRemoteEntry * Entry = View->EntryAt(View->model()->index(Row, FileListModel::NameColumn));
    if ((Entry != nullptr) && !Entry->Directory && View->ImagePath)
    {
      const QString Path = View->ImagePath(*Entry);
      if (!Path.isEmpty())
      {
        View->Preview(Path);
      }
    }
  }
}

// The menu bar's action at Path (captions without their & and ...).
QAction * MenuAction(QMainWindow & Window, const QStringList & Path);

namespace
{

int Failures = 0;
// The audit of every window shown (main()).
AccessibilityAudit * GAudit = nullptr;

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
    // Save's own dialog, as WinSCP's TSaveSessionDialog: the name and a
    // folder chosen among the sites' ones; saved there, a child in the tree.
    {
      SiteManager Dialog(&Window);
      Dialog.SelectNewSite();
      Dialog.HostName()->setText(QStringLiteral("127.0.0.2"));
      Dialog.UserName()->setText(QStringLiteral("other"));
      bool Asked = false;
      Responder.Custom = [&Asked](QWidget * Modal)
      {
        SaveSiteDialog * Save = qobject_cast<SaveSiteDialog *>(Modal);
        if (Save == nullptr)
        {
          return false;
        }
        Asked = true;
        Check((Save->NameEdit()->text() == QStringLiteral("other@127.0.0.2")) && (Save->FolderCombo()->findText(QStringLiteral("Tests")) > 0),
          "Save asks for the name, the sites' folders offered");
        Save->FolderCombo()->setEditText(QStringLiteral("Tests"));
        Save->accept();
        return true;
      };
      Dialog.Save();
      Responder.Custom = nullptr;
      const QTreeWidgetItem * Folder = Dialog.Tree()->topLevelItem(1);
      Check(Asked && (Folder != nullptr) && (Folder->text(0) == QStringLiteral("Tests")) && (Folder->childCount() == 2),
        "saved in the folder chosen, under it in the tree");
      Check(Dialog.SelectSite(QStringLiteral("Tests/other@127.0.0.2")), "as Tests/other@127.0.0.2");
      Dialog.DeleteSelected(false);
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

    // Commands > Queue, on a held queue (Process Queue off): two entries
    // wait, the second can move up and run now but not be suspended; Cancel
    // All empties it.
    {
      using TOperation = QueuePanel::TOperation;
      for (const QString & Name : { QStringLiteral("held1.txt"), QStringLiteral("held2.txt") })
      {
        QFile File(Dir.filePath(Name));
        File.open(QIODevice::WriteOnly);
        File.write("held\n");
      }
      Window.Session()->SetQueueEnabled(false);
      Local->Refresh();
      for (const QString & Name : { QStringLiteral("held1.txt"), QStringLiteral("held2.txt") })
      {
        Local->Select({ Name });
        Window.ActivatePanel(true);
        Check(Window.TransferSelected(false, false, true), "an upload is queued while the queue is held");
      }
      QueuePanel * Queue = Window.Queue();
      Check(WaitFor([Queue]() { return Queue->List()->topLevelItemCount() == 2; }), "both wait in the queue");
      const auto Source = [Queue](int Row) { return Queue->List()->topLevelItem(Row)->text(1); };
      Check(Source(0).endsWith(QStringLiteral("held1.txt")) && Source(1).endsWith(QStringLiteral("held2.txt")),
        "in the order queued");
      Queue->List()->topLevelItem(0)->setSelected(true);
      Check(!Queue->Action(TOperation::Up)->isEnabled() && Queue->Action(TOperation::Down)->isEnabled(),
        "the first can move down, not up");
      Queue->List()->clearSelection();
      Queue->List()->topLevelItem(1)->setSelected(true);
      Check(Queue->Action(TOperation::Up)->isEnabled() && !Queue->Action(TOperation::Down)->isEnabled() &&
        Queue->Action(TOperation::Execute)->isEnabled() && !Queue->Action(TOperation::Pause)->isEnabled() &&
        !Queue->Action(TOperation::Query)->isEnabled() && Queue->Action(TOperation::DeleteAll)->isEnabled(),
        "the second can move up and execute now; a waiting entry cannot be suspended or asked about");
      Queue->Action(TOperation::Up)->trigger();
      Check(WaitFor([&Source]() { return Source(0).endsWith(QStringLiteral("held2.txt")); }),
        "Move Up puts it first");
      // File List: a single file's upload lists none (parallel ones do).
      Queue->SetFileListVisible(true);
      Check(Queue->FileList()->isVisible() && (Queue->FileList()->topLevelItemCount() == 0),
        "the file list beside the queue, empty for a single file");
      Queue->SetFileListVisible(false);
      Queue->Action(TOperation::DeleteAll)->trigger();
      Check(WaitFor([Queue]() { return Queue->List()->topLevelItemCount() == 0; }), "Cancel All empties the queue");
      Window.Session()->SetQueueEnabled(true);
      for (const QString & Name : { QStringLiteral("held1.txt"), QStringLiteral("held2.txt") })
      {
        QFile::remove(Dir.filePath(Name));
      }
      Local->Refresh();
    }

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

    // P3-15: Remote > View > Thumbnails: a remote image downloaded in the
    // background for its preview.
    {
      QImage Image(40, 30, QImage::Format_RGB32);
      Image.fill(Qt::blue);
      Check(Image.save(Dir.filePath(QStringLiteral("remote.png"))), "(an image)");
      Local->Refresh();
      Local->Select({ QStringLiteral("remote.png") });
      Window.ActivatePanel(true);
      Check(Window.TransferSelected(false, false), "uploaded");
      Window.Panel()->Refresh();
      Window.SetPanelThumbnails(false, true);
      Check(Window.Panel()->Thumbnails(), "the remote panel's thumbnails");
      Check(WaitFor([&Window]()
      {
        RequestPreviews(Window.Panel()->Thumbs());
        return !Window.Session()->ThumbnailFile("remote.png").empty();
      }), "its file downloaded in the background");
      const QString Copy = QString::fromStdString(Window.Session()->ThumbnailFile("remote.png"));
      Check(WaitFor([&Window, &Copy]()
      {
        RequestPreviews(Window.Panel()->Thumbs());
        return Window.Panel()->Thumbs()->HasPreview(Copy);
      }) && !Window.Panel()->Thumbs()->Preview(Copy).isNull(), "and previewed");
      Window.SetPanelThumbnails(false, false);
      QFile::remove(Dir.filePath(QStringLiteral("remote.png")));
      // (The thumbnails' queue item gone before the next test's queue.)
      Check(WaitFor([&Window]() { return Window.Session()->QueueEmpty(); }), "the thumbnails' download ends");
    }

    // P3-15: Options > Queue > Once Empty, Sleep Computer: once the queue
    // empties, the session closed, the computer asleep (here a hook), then
    // the window closed (ConfirmExitOnCompletion off: not asked), and Stay
    // Idle chosen again.
    {
      CoreBridge::TLayout Layout = CoreBridge::Layout();
      Layout.ConfirmExitOnCompletion = false;
      CoreBridge::SetLayout(Layout);
      std::vector<MainWindow::TOnceEmpty> Powered;
      Window.PowerAction = [&Powered](MainWindow::TOnceEmpty What) { Powered.push_back(What); return true; };
      {
        QFile File(Dir.filePath(QStringLiteral("once.txt")));
        File.open(QIODevice::WriteOnly);
        File.write("once empty\n");
      }
      Local->Refresh();
      Local->Select({ QStringLiteral("once.txt") });
      Window.ActivatePanel(true);
      Window.SetOnceEmpty(MainWindow::TOnceEmpty::Suspend);
      Check(Window.TransferSelected(false, false, true), "an upload is queued, to sleep once done");
      Check(WaitFor([&Powered]() { return !Powered.empty(); }), "once the queue empties, the computer sleeps");
      Check((Powered == std::vector<MainWindow::TOnceEmpty>{ MainWindow::TOnceEmpty::Suspend }) &&
        (Window.Session() == nullptr) && !Window.isVisible() && (Window.OnceEmpty() == MainWindow::TOnceEmpty::Idle),
        "after the session closed, then the window, Stay Idle again");
    }
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

    // With the mouse, as a user drags: the press and move start the panel's
    // drag (its items draggable), and its drop on the other panel copies,
    // after the Copy dialog.
    {
      QFile(Dir.filePath(QStringLiteral("realdrag.txt"))).open(QIODevice::WriteOnly);
      Local->Refresh();
      FileView * View = Local->View();
      QModelIndex Index;
      for (int Row = 0; Row < View->model()->rowCount(); Row++)
      {
        const QModelIndex Candidate = View->model()->index(Row, 0);
        if (Candidate.data().toString() == QStringLiteral("realdrag.txt"))
        {
          Index = Candidate;
        }
      }
      Check(Index.isValid() && (View->model()->flags(Index) & Qt::ItemIsDragEnabled), "a file's row is draggable");
      std::unique_ptr<QMimeData> Started;
      View->OnDragStarted = [&Started](QMimeData * Data) { Started.reset(Data); };
      View->scrollTo(Index);
      const QPoint Pos = View->visualRect(Index).center();
      QTest::mousePress(View->viewport(), Qt::LeftButton, Qt::NoModifier, Pos);
      QTest::mouseMove(View->viewport(), Pos + QPoint(QApplication::startDragDistance() * 3, 4));
      QTest::mouseMove(View->viewport(), Pos + QPoint(QApplication::startDragDistance() * 6, 8));
      QTest::mouseRelease(View->viewport(), Qt::LeftButton, Qt::NoModifier, Pos + QPoint(QApplication::startDragDistance() * 6, 8));
      View->OnDragStarted = nullptr;
      TFileDrag Dragged;
      Check(Started && ReadFileDrag(Started.get(), Dragged) && Dragged.Local &&
        (Dragged.Names == QStringList{ QStringLiteral("realdrag.txt") }), "pressing and moving the mouse starts its drag");
      if (Started)
      {
        QWidget * Target = Remote->View()->viewport();
        const QPoint At(Target->width() / 2, Target->height() - 4);
        QDragEnterEvent Enter(At, Qt::CopyAction | Qt::MoveAction, Started.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(Target, &Enter);
        Check(Enter.isAccepted(), "the other panel takes it");
        QDragMoveEvent Move(At, Qt::CopyAction | Qt::MoveAction, Started.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(Target, &Move);
        QDropEvent DropEvent(At, Qt::CopyAction | Qt::MoveAction, Started.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(Target, &DropEvent);
        Responder.AcceptDialogs = true;
        Check(WaitUntil([Remote]() { return Remote->Names().contains(QStringLiteral("realdrag.txt")); }),
          "dropped there, the file is copied (the Copy dialog's OK)");
        Responder.AcceptDialogs = false;
        Check(QFileInfo::exists(Dir.filePath(QStringLiteral("realdrag.txt"))), "a copy: the local file stays");
      }
    }

    // The right click: WinSCP's popups, over a file and elsewhere.
    {
      const auto Texts = [](QMenu * Menu)
      {
        QStringList Result;
        for (QAction * Action : Menu->actions())
        {
          Result.append(Action->text().remove(QLatin1Char('&')));
        }
        delete Menu;
        return Result;
      };
      Remote->Select({ QStringLiteral("realdrag.txt") });
      const QStringList File = Texts(Window.ContextMenu(false, true, QPoint(), false));
      for (const char * Item : { "Open", "Edit", "Edit With", "Copy...", "Move...", "Duplicate...", "Move To...", "Delete",
             "Rename", "File Custom Commands", "File Names", "Properties" })
      {
        Check(File.contains(QString::fromLatin1(Item)), Item);
      }
      Check(!Window.LocalActive(), "the panel clicked is the active one");
      const QStringList Directory = Texts(Window.ContextMenu(true, false, QPoint(), false));
      Check(Directory.contains(QStringLiteral("Go To")) && Directory.contains(QStringLiteral("Refresh")) &&
        Directory.contains(QStringLiteral("New")) && Window.LocalActive(), "and the directory's, on the local panel");
      Local->Select({ QStringLiteral("realdrag.txt") });
      Window.CopyFileNames(true, true);
      Check(QDir::fromNativeSeparators(QApplication::clipboard()->text()) == Dir.filePath(QStringLiteral("realdrag.txt")),
        "File Names: the full path to the clipboard");
    }

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

    // P3-16: selected files only, the options kept for next time, checksum
    // as the server can.
    {
      const QDir Root(Dir.filePath(QStringLiteral("../root")));
      for (const char * Name : { "sel1.txt", "sel2.txt" })
      {
        QFile File(Dir.filePath(QString::fromLatin1(Name)));
        File.open(QIODevice::WriteOnly);
        File.write(Name);
      }
      Window.Local()->ChangeDirectory(Directory);
      Window.Local()->Refresh();
      Remote->ChangeDirectory(QStringLiteral("/sub"));
      Window.Local()->Select({ QStringLiteral("sel1.txt") });
      Window.ActivatePanel(true);
      bool Shown = false;
      bool ChecksumAsServer = false;
      CoreBridge::TSession * Session = Window.Session();
      Responder.Custom = [&Shown, &ChecksumAsServer, Session](QWidget * Modal)
      {
        SynchronizeDialog * Dialog = qobject_cast<SynchronizeDialog *>(Modal);
        if (Dialog == nullptr)
        {
          return false;
        }
        Shown = true;
        ChecksumAsServer = (Dialog->ByChecksumCheck()->isEnabled() == Session->CanSyncChecksum());
        Dialog->RemoteButton()->setChecked(true);
        Dialog->PreviewCheck()->setChecked(false);
        Dialog->SelectedOnlyCheck()->setChecked(true);
        Dialog->SaveSettingsCheck()->setChecked(true);
        Dialog->accept();
        return true;
      };
      Check(Window.Synchronize(true) && Shown, "Synchronize with a selection");
      Check(ChecksumAsServer, "checksum offered as the server can");
      Check(WaitUntil([&Root]() { return QFileInfo::exists(Root.filePath(QStringLiteral("sub/sel1.txt"))); }) &&
        !QFileInfo::exists(Root.filePath(QStringLiteral("sub/sel2.txt"))), "selected files only: the selected one");
      const CoreBridge::TSyncDefaults Defaults = CoreBridge::SyncDefaults();
      Check(Defaults.Params.SelectedOnly && !Defaults.Params.PreviewChanges, "the options kept for next time");
      Check(!Defaults.SaveMode, "not the direction, the active panel's");
      SynchronizeDialog Next(Directory, QStringLiteral("/sub"), Defaults.Params, true, nullptr, false, true);
      Check(Next.SelectedOnlyCheck()->isChecked() && !Next.PreviewCheck()->isChecked(), "the next dialog with them");
      SynchronizeDialog NoSelection(Directory, QStringLiteral("/sub"), Defaults.Params, true, nullptr, false, false);
      Check(!NoSelection.SelectedOnlyCheck()->isEnabled() && !NoSelection.Params().SelectedOnly, "no selection: not offered");
    }

    // P3-16: the checklist's Tools: Find Move Candidate, Move, Calculate.
    {
      const QDir Root(Dir.filePath(QStringLiteral("../root")));
      QDir().mkpath(Dir.filePath(QStringLiteral("mv/tree/deep")));
      auto Write = [](const QString & Path, const QByteArray & Data)
      {
        QFile File(Path);
        File.open(QIODevice::WriteOnly);
        File.write(Data);
      };
      // Through the session: the server's listings are cached.
      QDir().mkpath(Dir.filePath(QStringLiteral("stage")));
      Write(Dir.filePath(QStringLiteral("stage/old-name.txt")), "same twelve!");
      Remote->ChangeDirectory(QStringLiteral("/"));
      Check(Remote->NewFolder(QStringLiteral("mv")), "(its directory)");
      Check(Window.Upload({ Dir.filePath(QStringLiteral("stage/old-name.txt")) }, QStringLiteral("/mv"), false),
        "(a file on the server only)");
      Write(Dir.filePath(QStringLiteral("mv/new-name.txt")), "same twelve!");
      Write(Dir.filePath(QStringLiteral("mv/tree/deep/f.bin")), QByteArray(1000, 'x'));
      CoreBridge::TSyncParams Params;
      Params.Direction = TSyncParams::TDirection::Remote;
      Params.Delete = true;
      std::unique_ptr<CoreBridge::TSyncChecklist> Checklist;
      try
      {
        Checklist = Window.Session()->SyncCollect(QDir::toNativeSeparators(Dir.filePath(QStringLiteral("mv"))).toStdString(),
          "/mv", Params);
      }
      catch (const CoreBridge::TCoreError & E)
      {
        std::printf("FAIL collecting: %s\n", E.Error.Message.c_str());
      }
      Check(Checklist && (Checklist->Items().size() == 3), "(an upload, a delete, a new directory)");
      if (Checklist)
      {
        SyncChecklistDialog Dialog(*Checklist, Dir.filePath(QStringLiteral("mv")), QStringLiteral("/mv"), &Window, Window.Session());
        Check(Dialog.FindMoveCandidate() && (Dialog.SelectedIndexes().size() == 2), "Find Move Candidate: the pair selected");
        Check(Checklist->CanMove(Dialog.SelectedIndexes()), "which Move can take");
        Check(Dialog.MoveSelected() && (Checklist->Items().size() == 1), "Move: both entries done with");
        Check(QFileInfo::exists(Root.filePath(QStringLiteral("mv/new-name.txt"))) &&
          !QFileInfo::exists(Root.filePath(QStringLiteral("mv/old-name.txt"))), "moved on the server, not transferred");
        Check(Checklist->Items()[0].Directory && !Checklist->Items()[0].HasSize, "(the directory left, its size unknown)");
        Check(Dialog.CalculateSizes(true) && Checklist->Items()[0].HasSize && (Checklist->Items()[0].LocalSize == 1000),
          "Calculate All: its size");
        Check(Dialog.List()->topLevelItem(0)->text(2).contains(QStringLiteral("1")), "shown in the list");
      }
    }
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
    Check(Local->Selection().isEmpty(), "Insert on \"..\" selects nothing");
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
    Check(Local->Selection().isEmpty(), "Ctrl+Shift+L clears the selection");
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

    // P3-15: the local panel shows WinSCP's Type column after the size; the
    // main menu's Columns lists the name (always shown), then Size
    // Automatically and Reset Layout, which brings the defaults back.
    Check(!View->isColumnHidden(FileListModel::TypeColumn) &&
      (View->header()->visualIndex(FileListModel::TypeColumn) == View->header()->visualIndex(FileListModel::SizeColumn) + 1),
      "the local panel shows the type column after the size");
    {
      bool Typed = false;
      for (int Row = 0; Row < View->model()->rowCount(); Row++)
      {
        if (View->model()->index(Row, FileListModel::NameColumn).data().toString() == QStringLiteral("d.txt"))
        {
          Typed = !View->model()->index(Row, FileListModel::TypeColumn).data().toString().isEmpty();
        }
      }
      Check(Typed, "with each entry's type name");
    }
    {
      std::unique_ptr<QMenu> Menu(View->ColumnsMenu(true));
      QStringList Texts;
      for (QAction * Action : Menu->actions())
      {
        Texts.append(Action->text());
      }
      Check(Texts.startsWith(QStringLiteral("&Name")) && !Menu->actions().first()->isEnabled() &&
        Texts.contains(QStringLiteral("Size A&utomatically")) && (Texts.last() == QStringLiteral("&Reset Layout")),
        "the main menu's Columns: the name, then Size Automatically and Reset Layout");
      View->setColumnHidden(FileListModel::GroupColumn, false);
      Menu->actions().last()->trigger();
      Check(View->isColumnHidden(FileListModel::GroupColumn) && !View->isColumnHidden(FileListModel::TypeColumn) &&
        (View->header()->sortIndicatorSection() == FileListModel::NameColumn) &&
        (View->header()->sortIndicatorOrder() == Qt::AscendingOrder),
        "Reset Layout goes back to the default columns, sorted by name");
    }
    {
      std::unique_ptr<QMenu> Menu(View->SortMenu(true));
      emit Menu->aboutToShow();
      for (QAction * Action : Menu->actions())
      {
        if (Action->text() == QStringLiteral("By &Size"))
        {
          Action->trigger();
        }
      }
      Check(View->header()->sortIndicatorSection() == FileListModel::SizeColumn, "Sort > By Size");
      Menu->actions().first()->trigger();
      Check(View->header()->sortIndicatorOrder() == Qt::DescendingOrder, "Sort > Ascending unchecked: descending");
      View->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
    }
    // Mark > Restore Selection: the names selected when an operation
    // started, selected again.
    {
      const QStringList Two = { QStringLiteral("b.log"), QStringLiteral("d.txt") };
      Local->Select(Two);
      Check(!View->SelectedNamesSaved(), "nothing to restore at first");
      View->SaveSelectedNames();
      View->clearSelection();
      View->RestoreSelectedNames();
      QStringList Selected = Local->Selected();
      Selected.sort();
      Check(Selected == Two, "Restore Selection selects them again");
    }
    // Local > Change Drive (Alt+F1): the documents and desktop folders, then
    // the drives (the root and home here), the current one checked; one
    // chosen is changed to.
    {
      std::unique_ptr<QMenu> Menu(Local->PathMenu());
      const QList<QAction *> Places = Menu->actions();
      Check((Places.size() >= 3) &&
        (Places[0]->text() == QString::fromStdString(CoreBridge::Text(CoreBridge::TText::SpecialFolderDocuments))),
        "Change Drive: the documents folder first");
      int Checked = 0;
      QAction * Root = nullptr;
      for (QAction * Place : Places)
      {
        Checked += Place->isChecked() ? 1 : 0;
#if defined(_WIN32)
        Root = ((Root == nullptr) && Place->text().startsWith(QLatin1Char('&'))) ? Place : Root;
#else
        Root = (Place->text() == QStringLiteral("/")) ? Place : Root;
#endif
      }
      Check((Checked == 1) && (Root != nullptr), "one checked, the drives listed");
      const QString Before = Local->Path();
      Root->trigger();
      Check(Local->Path() != Before, "a drive chosen is gone to");
      Local->ChangeDirectory(Before);
    }
    // P3-15: the command line (Options > Command Line, Shift+Ctrl+M): a
    // local command started in the local panel's directory, kept in the
    // local commands' history; Insert to Command Line adds the names.
    {
      QStringList Started;
      Frontend.SetStartProgram([&Started](const QString & Program, const QString & Params, const QString & In)
      {
        Started = { Program, Params, In };
        return true;
      });
      Window.ActivatePanel(true);
      QAction * Shown = Window.CommandLineAction();
      Check(!Shown->isChecked(), "the command line hidden at first, as WinSCP's");
      Shown->trigger();
      Check(Shown->isChecked() && Window.CommandLine()->isVisible() &&
        Window.CommandPrompt()->text().endsWith(QStringLiteral(" >")), "Command Line shows it, the local prompt");
      Local->Select({ QStringLiteral("b.log"), QStringLiteral("d.txt") });
      Window.CommandLine()->setEditText(QStringLiteral("cat "));
      Window.InsertToCommandLine();
      // (In the panel's order, as WinSCP's CreateFileList.)
      const QString Line = Window.CommandLine()->currentText();
      Check((Line == QStringLiteral("cat b.log d.txt ")) || (Line == QStringLiteral("cat d.txt b.log ")),
        "Insert to Command Line adds the names");
      Check(Window.ExecuteCommandLine() && (Started.value(0) == QStringLiteral("cat")) &&
        (Started.value(1).trimmed() == Line.mid(4).trimmed()) && (QDir(Started.value(2)) == QDir(Local->Path())),
        "Enter starts it, in the local directory");
      const std::vector<std::string> History = CoreBridge::History("LocalCommands");
      Check(!History.empty() && (History.front() == Line.toStdString()) && Window.CommandLine()->currentText().isEmpty(),
        "kept in the local commands' history, the line cleared");
      Window.ActivatePanel(false);
      Check(Window.CommandPrompt()->text().endsWith(QStringLiteral(" $")) && !Window.CommandLine()->isEnabled() &&
        !Window.ExecuteCommandLine(), "the remote side's prompt, not usable without a session");
      Window.ActivatePanel(true);
      Frontend.SetStartProgram(nullptr);
      Shown->trigger();
      Check(!Window.CommandLine()->isVisible(), "Command Line hides it again");
    }
    // P3-15: Local > View > Thumbnails: a grid over the same selection, an
    // image's preview loaded in the background; Details back.
    {
      QImage Image(32, 24, QImage::Format_RGB32);
      Image.fill(Qt::red);
      Check(Image.save(Dir.filePath(QStringLiteral("picture.png"))), "(an image)");
      Local->Refresh();
      Window.SetPanelThumbnails(true, true);
      Check(Local->Thumbnails() && Local->Thumbs()->isVisible() && !Local->View()->isVisible() &&
        CoreBridge::Layout().LocalThumbnails, "View > Thumbnails shows the grid, and is kept");
      Local->Select({ QStringLiteral("picture.png") });
      Check(Local->Thumbs()->selectionModel() == Local->View()->selectionModel(), "the same selection");
      const QString Picture = QDir(Local->Path()).filePath(QStringLiteral("picture.png"));
      QElapsedTimer Timer;
      Timer.start();
      // Asked for as the delegate asks (a window not exposed, as over SSH
      // on Windows, is never painted).
      while (!Local->Thumbs()->HasPreview(Picture) && (Timer.elapsed() < 10000))
      {
        RequestPreviews(Local->Thumbs());
        QApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      Check(Local->Thumbs()->HasPreview(Picture) && !Local->Thumbs()->Preview(Picture).isNull(), "the image's preview loaded");
      Window.SetPanelThumbnails(true, false);
      Check(!Local->Thumbnails() && Local->View()->isVisible() && !CoreBridge::Layout().LocalThumbnails, "View > Details back");
      QFile::remove(Dir.filePath(QStringLiteral("picture.png")));
      Local->Refresh();
    }

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
      for (int Index = 0; Index < Dialog.PageCount(); Index++)
      {
        Pages.append(Dialog.PageName(Index));
      }
      Check(Pages == QStringList({ PreferencesDialog::tr("Environment"), PreferencesDialog::tr("Interface"),
        PreferencesDialog::tr("Window"), PreferencesDialog::tr("Commander"),
        PreferencesDialog::tr("Languages"), PreferencesDialog::tr("Panels"), PreferencesDialog::tr("Remote"),
        PreferencesDialog::tr("Local"), PreferencesDialog::tr("File colors"),
        PreferencesDialog::tr("Editors"), PreferencesDialog::tr("Internal editor"), PreferencesDialog::tr("Transfer"),
        PreferencesDialog::tr("Drag & Drop"), PreferencesDialog::tr("Background"), PreferencesDialog::tr("Endurance"),
        PreferencesDialog::tr("Network"), PreferencesDialog::tr("Security"), PreferencesDialog::tr("Logging"),
        PreferencesDialog::tr("Integration"), PreferencesDialog::tr("Applications"), PreferencesDialog::tr("Commands"),
        PreferencesDialog::tr("Storage"), PreferencesDialog::tr("Updates") }),
        "the dialog's pages, in the order of WinSCP's tree");
      Check((Dialog.Navigation()->topLevelItemCount() == 10) && (Dialog.Navigation()->topLevelItem(0)->childCount() == 4) &&
        (Dialog.Navigation()->topLevelItem(3)->child(0)->text(0) == PreferencesDialog::tr("Drag & Drop")),
        "as its tree: each section with its subpages");
      // Environment, Interface, Remote, Local, Drag & Drop, Updates: WinSCP's
      // defaults.
      Check(Dialog.ConfirmTransferringCheck()->isChecked() && Dialog.ConfirmDeletingCheck()->isChecked() &&
        Dialog.ConfirmRecyclingCheck()->isChecked() && Dialog.ConfirmClosingSessionCheck()->isChecked() &&
        Dialog.SynchronizeSummaryCheck()->isChecked() && !Dialog.BeepOnFinishCheck()->isChecked() &&
        !Dialog.BeepOnFinishAfterSpin()->isEnabled() && Dialog.BalloonNotificationsCheck()->isChecked(),
        "the confirmations on, no beep");
      Check(Dialog.ConfirmDDTransferCheck()->checkState() == Qt::PartiallyChecked, "drags' confirmation Auto (grayed)");
      Dialog.ConfirmTransferringCheck()->setChecked(false);
      Check(!Dialog.ConfirmDDTransferCheck()->isEnabled(), "drags' only with transfers'");
      Dialog.ConfirmTransferringCheck()->setChecked(true);
      Check((Dialog.ThemeCombo()->currentIndex() == 0) && (Dialog.UpdatesPeriodCombo()->currentIndex() == 0) &&
        (Dialog.UpdatesBetaVersionsCombo()->currentIndex() == 0) && !Dialog.UpdatesProxyHostEdit()->isEnabled(),
        "automatic theme, no automatic updates");
      Check(Dialog.ShowInaccesibleDirectoriesCheck()->isChecked() && !Dialog.RefreshRemotePanelCheck()->isChecked() &&
        !Dialog.RefreshRemotePanelIntervalSpin()->isEnabled() && !Dialog.PreserveLocalDirectoryCheck()->isChecked() &&
        Dialog.DeleteToRecycleBinCheck()->isChecked() && Dialog.DDWarnLackOfTempSpaceCheck()->isChecked(),
        "the panels' and drag and drop's defaults");
      Check(Dialog.ShowPage(PreferencesDialog::tr("Updates")) && !Dialog.ShowPage(QStringLiteral("Explorer")),
        "pages shown by name; no Explorer's");
      Dialog.ConfirmDeletingCheck()->setChecked(false);
      Dialog.ConfirmDDTransferCheck()->setCheckState(Qt::Unchecked);
      Check(!Dialog.ConfirmDDTransferCheck()->isTristate(), "once changed, never grayed again");
      Dialog.BeepOnFinishCheck()->setChecked(true);
      Check(Dialog.BeepOnFinishAfterSpin()->isEnabled(), "the beep's seconds with the beep");
      Dialog.BeepOnFinishAfterSpin()->setValue(5);
      Dialog.RefreshRemotePanelCheck()->setChecked(true);
      Dialog.RefreshRemotePanelIntervalSpin()->setValue(30);
      Dialog.PreserveLocalDirectoryCheck()->setChecked(true);
      Dialog.UpdatesPeriodCombo()->setCurrentIndex(2);
      Dialog.UpdatesProxyButton()->setChecked(true);
      Check(Dialog.UpdatesProxyHostEdit()->isEnabled(), "the proxy's host with the proxy");
      Dialog.UpdatesProxyHostEdit()->setText(QStringLiteral("proxy.example"));
      Dialog.UpdatesBetaVersionsCombo()->setCurrentIndex(1);
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
      Dialog.ShowPage(PreferencesDialog::tr("Languages"));
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
      // Single files over several connections: only with parallel transfers.
      Check(Dialog.ParallelThresholdCheck()->isEnabled() && !Dialog.ParallelThresholdSpin()->isEnabled() &&
        (Dialog.ParallelThresholdSpin()->value() == 100), "the threshold off, WinSCP's 100 MB offered");
      Dialog.ParallelThresholdCheck()->setChecked(true);
      Dialog.ParallelThresholdSpin()->setValue(50);
      Dialog.QueueParallelCheck()->setChecked(false);
      Check(!Dialog.ParallelThresholdCheck()->isEnabled() && !Dialog.ParallelThresholdSpin()->isEnabled(),
        "no threshold without parallel transfers");
      Dialog.QueueParallelCheck()->setChecked(true);
      Check(Dialog.EnableQueueByDefaultCheck()->isChecked(), "queue processing on by default");
      Dialog.EnableQueueByDefaultCheck()->setChecked(false);
      Check(Dialog.QueueViewButton(1)->isChecked(), "the queue list hidden when empty, as Options > Queue");
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
      CopyOnCtrlK = CopyOnCtrlK || ((Action->text() == MainWindow::tr("Cop&y...")) &&
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
    Check((Saved(Interface, QStringLiteral("ConfirmDeleting")) == QStringLiteral("0")) &&
      (Saved(Interface, QStringLiteral("DDTransferConfirmation2")) == QStringLiteral("1")) &&
      (Saved(Interface, QStringLiteral("BeepOnFinish")) == QStringLiteral("1")) &&
      (Saved(Interface, QStringLiteral("RefreshRemotePanel")) == QStringLiteral("1")) &&
      (Saved(Interface, QStringLiteral("ConfirmTransferring")) == QStringLiteral("1")),
      "Environment's and Remote's in WinSCP's keys");
    const QString Updates = QStringLiteral("Configuration\\Interface\\Updates");
    Check((Saved(Updates, QStringLiteral("Period")) == QStringLiteral("7")) &&
      (Saved(Updates, QStringLiteral("ConnectionType")) == QStringLiteral("2")) &&
      (Saved(Updates, QStringLiteral("ProxyHost")) == QStringLiteral("proxy.example")) &&
      (Saved(Updates, QStringLiteral("BetaVersions")) == QStringLiteral("1")), "Updates' in Interface\\Updates");
    {
      const CoreBridge::TPreferences Now = CoreBridge::Preferences();
      Check(!Now.ConfirmDeleting && (Now.DDTransferConfirmation == CoreBridge::TPreferences::TAutoSwitch::Off) &&
        Now.BeepOnFinish && (Now.BeepOnFinishAfter == 5) && Now.RefreshRemotePanel && (Now.RefreshRemotePanelInterval == 30) &&
        Now.PreserveLocalDirectory && (Now.UpdatesPeriod == 7), "and read back");
    }
    const QString Commander = QStringLiteral("Configuration\\Interface\\Commander");
    Check((Saved(Commander, QStringLiteral("PreserveLocalDirectory")) == QStringLiteral("1")), "Local's in Interface\\Commander");
    Check((Saved(Commander, QStringLiteral("SwappedPanels")) == QStringLiteral("1")) &&
      (Saved(Commander, QStringLiteral("ExplorerKeyboardShortcuts")) == QStringLiteral("1")),
      "and Interface\\Commander's");
    // The jump list's recent sites and workspaces (P3-22), as WinSCP keeps
    // them: newest first, once each, at most 30; those the user removed
    // from the taskbar's dropped.
    {
      CoreBridge::TJumpList Shown;
      const auto Keep = [&Shown](CoreBridge::TJumpList & Lists) { Shown = Lists; };
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "first", true, Keep);
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "second", true, Keep);
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "first", true, Keep);
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Workspace, "work", true, Keep);
      Check((Shown.Sites == std::vector<std::string>({ "first", "second" })) &&
        (Shown.Workspaces == std::vector<std::string>({ "work" })), "recent sites newest first, once each");
      Check(Saved(QStringLiteral("Configuration"), QStringLiteral("JumpList")) == QStringLiteral("first,second"),
        "kept under WinSCP's key");
      for (int Index = 0; Index < 35; Index++)
      {
        CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "site" + std::to_string(Index), true, Keep);
      }
      Check((Shown.Sites.size() == 30) && (Shown.Sites.front() == "site34"), "at most 30");
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "site34", false,
        [](CoreBridge::TJumpList & Lists) { Lists.Sites.erase(Lists.Sites.begin()); });
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, std::string(), true, Keep);
      Check((Shown.Sites.size() == 28) && (Shown.Sites.front() == "site32") && (Shown.Workspaces.size() == 1),
        "a deleted one out, and those removed from the taskbar's");
      // The taskbar's own, on Windows (elsewhere nothing).
      Integration::UpdateJumpList(CoreBridge::TJumpListEntry::Site, "user@example.com", true);
      CoreBridge::UpdateJumpList(CoreBridge::TJumpListEntry::Site, std::string(), true, Keep);
#if defined(_WIN32)
      Check(!Shown.Sites.empty() && (Shown.Sites.front() == "user@example.com"), "shown in the taskbar's jump list");
      // What Windows keeps of it (not Wine's): the program's
      // .customDestinations-ms, the site's link's title in it.
      if (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") == nullptr)
      {
        const QDir Destinations(qEnvironmentVariable("APPDATA") + QStringLiteral("/Microsoft/Windows/Recent/CustomDestinations"));
        bool Found = false;
        for (const QFileInfo & File : Destinations.entryInfoList({ QStringLiteral("*.customDestinations-ms") }, QDir::Files))
        {
          QFile Data(File.filePath());
          const QString Title = QStringLiteral("user@example.com");
          if (Data.open(QIODevice::ReadOnly) &&
              Data.readAll().contains(QByteArray(reinterpret_cast<const char *>(Title.utf16()), Title.size() * 2)))
          {
            Found = true;
          }
        }
        Check(Found, "Windows has the link in the program's jump list");
      }
#else
      Check(Shown.Sites.front() == "site32", "no jump list elsewhere");
#endif
    }
    Check(Saved(Interface, QStringLiteral("LocaleSafe")).toUInt() == CoreBridge::PreferredLocale(),
      "the language chosen too");
    Check((Saved(Interface, QStringLiteral("QueueTransfersLimit")) == QStringLiteral("3")) &&
      (Saved(Interface, QStringLiteral("QueueKeepDoneItemsFor")) == QStringLiteral("60")) &&
      (Saved(Interface, QStringLiteral("SessionReopenAuto")) == QStringLiteral("0")) &&
      (Saved(Interface, QStringLiteral("ParallelTransferThreshold")) == QStringLiteral("51200")) &&
      (Saved(Interface, QStringLiteral("EnableQueueByDefault")) == QStringLiteral("0")),
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
        if (Action->text() == MainWindow::tr("&Disconnect Session"))
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

    // Integration: registered as the protocols' URL handler, and not.
    {
#if defined(Q_OS_WIN)
      QString Error;
      Check(Integration::RegisterUrlHandler(Error), "registered to handle URL addresses");
      // The machine's classes, or the user's when those cannot be written;
      // through the 32-bit view upstream writes with (KEY_WOW64_32KEY), which
      // Windows shares for classes and Wine does not.
      QString Command;
      for (const char * Root : { "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE" })
      {
        const QSettings Key(QStringLiteral("%1\\Software\\Classes\\sftp\\shell\\open\\command").arg(QString::fromLatin1(Root)),
          QSettings::Registry32Format);
        Command += Key.value(QStringLiteral("Default")).toString() + QStringLiteral(" | ");
      }
      if (!Command.contains(QStringLiteral("/Unsafe")))
      {
        std::printf("sftp's command: %s\n", Command.toUtf8().constData());
      }
      Check(Command.contains(QFileInfo(QCoreApplication::applicationFilePath()).fileName(), Qt::CaseInsensitive) &&
        Command.contains(QStringLiteral("/Unsafe")), "sftp:// opens the program, with /Unsafe and the URL");
      Check(Integration::UnregisterUrlHandler(Error), "and unregistered");
#else
      const QDir Home(QDir(Directory).filePath(QStringLiteral("home")));
      QDir().mkpath(Home.path());
      const QByteArray SavedData = qgetenv("XDG_DATA_HOME");
      const QByteArray SavedConfig = qgetenv("XDG_CONFIG_HOME");
      qputenv("XDG_DATA_HOME", Home.filePath(QStringLiteral("data")).toLocal8Bit());
      qputenv("XDG_CONFIG_HOME", Home.filePath(QStringLiteral("config")).toLocal8Bit());
      QDir().mkpath(Home.filePath(QStringLiteral("config")));
      WriteAll(Integration::MimeAppsPath(), "[Default Applications]\ntext/plain=gedit.desktop\nx-scheme-handler/sftp=other.desktop\n");
      QString Error;
      Check(Integration::RegisterUrlHandler(Error), "registered to handle URL addresses");
      const QString Entry = QString::fromUtf8(ReadAll(Integration::DesktopEntryPath()));
      Check(Entry.contains(QStringLiteral("x-scheme-handler/sftp;")) && Entry.contains(QStringLiteral("x-scheme-handler/winscp-sftp;")) &&
        Entry.contains(QStringLiteral("/Unsafe %u")), "a desktop entry for the protocols' URLs, with /Unsafe as upstream's");
      QString MimeApps = QString::fromUtf8(ReadAll(Integration::MimeAppsPath()));
      Check(MimeApps.contains(QStringLiteral("x-scheme-handler/sftp=linscp.desktop")) &&
        !MimeApps.contains(QStringLiteral("other.desktop")) && MimeApps.contains(QStringLiteral("text/plain=gedit.desktop")),
        "the default for them, the rest kept");
      Check(Integration::UnregisterUrlHandler(Error) && !QFileInfo::exists(Integration::DesktopEntryPath()), "unregistered");
      MimeApps = QString::fromUtf8(ReadAll(Integration::MimeAppsPath()));
      Check(!MimeApps.contains(QStringLiteral("linscp.desktop")) && MimeApps.contains(QStringLiteral("text/plain=gedit.desktop")),
        "its defaults gone, the rest kept");
      qputenv("XDG_DATA_HOME", SavedData);
      qputenv("XDG_CONFIG_HOME", SavedConfig);
      if (SavedData.isEmpty())
      {
        qunsetenv("XDG_DATA_HOME");
      }
      if (SavedConfig.isEmpty())
      {
        qunsetenv("XDG_CONFIG_HOME");
      }
#endif
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

    // P3-16: Reload from the server; Save All; the first editor's place kept.
    {
      const QDir Root(Dir.filePath(QStringLiteral("../root")));
      Check(Window.CloseEditors(false), "(no editors open)");
      Remote->ChangeDirectory(QStringLiteral("/"));
      Remote->Select({ QStringLiteral("hello.txt") });
      Window.ActivatePanel(false);
      EditorWindow * First = Window.EditCurrent();
      Check(First != nullptr, "(a remote file edited)");
      if (First != nullptr)
      {
        WriteAll(Root.filePath(QStringLiteral("hello.txt")), "changed on the server\n");
        Check(First->Reload() && First->Editor()->toPlainText().contains(QStringLiteral("changed on the server")),
          "Reload: from the server again");
        WriteAll(Dir.filePath(QStringLiteral("saveall.txt")), "local\n");
        EditorWindow * Second = Window.EditLocal(Dir.filePath(QStringLiteral("saveall.txt")));
        Check(Second != nullptr, "(a local one too)");
        if (Second != nullptr)
        {
          First->Editor()->moveCursor(QTextCursor::End);
          First->Editor()->insertPlainText(QStringLiteral("remote edit\n"));
          Second->Editor()->moveCursor(QTextCursor::End);
          Second->Editor()->insertPlainText(QStringLiteral("local edit\n"));
          First->OnSaveAll();
          Check(!First->Modified() && !Second->Modified() &&
            ReadAll(Dir.filePath(QStringLiteral("saveall.txt"))).contains("local edit"), "Save All saves every editor");
          Check(WaitUntil([&Window]() { return Window.Session()->QueueEmpty(); }) &&
            WaitUntil([&Root]() { return ReadAll(Root.filePath(QStringLiteral("hello.txt"))).contains("remote edit"); }),
            "the remote one uploaded");
        }
        First->resize(655, 455);
        Check(Window.CloseEditors(false), "(closed)");
        QApplication::processEvents();
        const QStringList Kept = QString::fromStdString(CoreBridge::EditorSettings().WindowParams).split(QLatin1Char(';'));
        Check((Kept.size() >= 6) && (Kept[2].toInt() - Kept[0].toInt() == 655), "the first editor's place kept");
        EditorWindow * Next = Window.EditLocal(Dir.filePath(QStringLiteral("saveall.txt")));
        Check((Next != nullptr) && (Next->width() == 655) && (Next->height() == 455), "the next first one opens there");
        Window.CloseEditors(false);
      }
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
    // P3-15: Files > New > Link makes a new link even on a link (Alt+F6
    // would edit it).
#if !defined(_WIN32)
    Local->Refresh();
    Local->Select({ LinkName });
    Window.ActivatePanel(true);
    bool Asked = false;
    Responder.Custom = [&LinkName, &Asked](QWidget * Modal)
    {
      LinkDialog * Dialog = qobject_cast<LinkDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      Asked = true;
      Check(!Dialog->FileNameEdit()->isReadOnly() && Dialog->FileNameEdit()->text().isEmpty() &&
        (Dialog->PointToEdit()->text() == LinkName), "New > Link: a new link to the focused one, not an edit");
      Dialog->reject();
      return true;
    };
    QAction * NewLink = MenuAction(Window, { "Files", "New", "Link" });
    Check(NewLink != nullptr, "Files > New > Link");
    if (NewLink != nullptr)
    {
      NewLink->trigger();
    }
    Check(Asked, "asks for the link");
    Responder.Custom = nullptr;
#endif
    // Files > Upload/Download, as the active side is; Locking not for FTP.
    {
      QAction * Files = MenuAction(Window, { "Files" });
      Window.ActivatePanel(true);
      emit Files->menu()->aboutToShow();
      Check(MenuAction(Window, { "Files", "Upload" }) != nullptr &&
        MenuAction(Window, { "Files", "Upload", "Upload in Background" }) != nullptr, "the local side's Upload submenu");
      Window.ActivatePanel(false);
      emit Files->menu()->aboutToShow();
      Check(MenuAction(Window, { "Files", "Download" }) != nullptr &&
        MenuAction(Window, { "Files", "Download", "Download and Delete" }) != nullptr, "the remote side's Download");
      Check(!Session->CanLock() && !MenuAction(Window, { "Files", "Locking" })->isEnabled() && !Window.LockSelected(true),
        "FTP locks nothing: Locking disabled");
    }
    // Remote > Change Directory (Alt+F2): the root and each directory down
    // to the current one, which is checked.
    {
      Check(Remote->ChangeDirectory(QStringLiteral("/sub")), "(in /sub)");
      std::unique_ptr<QMenu> Menu(Remote->PathMenu());
      const QList<QAction *> Paths = Menu->actions();
      Check((Paths.size() == 2) && (Paths[0]->text() == QStringLiteral("/")) && Paths[1]->text().trimmed() == QStringLiteral("sub") &&
        Paths[1]->isChecked() && !Paths[0]->isChecked(), "Change Directory: / and sub, sub checked");
      Paths[0]->trigger();
      Check(Remote->Path() == QStringLiteral("/"), "the root chosen is gone to");
    }

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
      Check(Console.size() == QSize(570, 430), "WinSCP's default size");
      Console.resize(611, 433);
    }
    Check(QString::fromStdString(CoreBridge::Layout().ConsoleWindowSize).startsWith(QStringLiteral("611,433,")),
      "its size kept, as WinSCP's ConsoleWin.WindowSize");
    {
      ConsoleDialog Console(Session);
      Check(Console.size() == QSize(611, 433), "and opened at it");
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
    // P3-16: a folder left expanded is expanded next time (NodeOpened).
    {
      LocationProfilesDialog Dialog(QDir::toNativeSeparators(Directory), QStringLiteral("/sub"), QStringList(), QStringList(),
        true, Key, false);
      QTreeWidgetItem * Folder = Dialog.Tree(0)->topLevelItem(0);
      Check((Folder != nullptr) && !Folder->isExpanded(), "a folder collapsed, as left");
      if (Folder != nullptr)
      {
        Folder->setExpanded(true);
      }
      Dialog.reject();
    }
    {
      LocationProfilesDialog Dialog(QDir::toNativeSeparators(Directory), QStringLiteral("/sub"), QStringList(), QStringList(),
        true, Key, false);
      Check((Dialog.Tree(0)->topLevelItem(0) != nullptr) && Dialog.Tree(0)->topLevelItem(0)->isExpanded(),
        "expanded next time, as left");
      Dialog.reject();
    }
    Profiles = CoreBridge::Bookmarks(Key);
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

    // P3-16: the Open Directory button's menu, the side's bookmarks.
    {
      const QString Target = QDir(Directory).filePath(QStringLiteral("folder"));
      QDir().mkpath(Target);
      CoreBridge::TBookmarkEntry Mark;
      Mark.Name = "menu mark";
      Mark.Local = QDir::toNativeSeparators(Target).toStdString();
      CoreBridge::SetBookmarks(std::string(), { Mark });
      QAction * Open = Window.Local()->Band(TPanelBand::Path)->actions().value(0);
      Check((Open != nullptr) && (Open->menu() != nullptr), "the Open Directory button has its menu");
      if ((Open != nullptr) && (Open->menu() != nullptr))
      {
        emit Open->menu()->aboutToShow();
        QAction * Item = nullptr;
        for (QAction * Action : Open->menu()->actions())
        {
          Item = (Action->text() == QDir::toNativeSeparators(Target)) ? Action : Item;
        }
        Check(Item != nullptr, "listing the side's bookmarked directory");
        if (Item != nullptr)
        {
          Item->trigger();
          Check(QDir(Window.Local()->Path()) == QDir(Target), "which it goes to");
        }
      }
    }
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
    // The Copy dialog's Generate Code: the transfer, as a script and code.
    {
      CoreBridge::TSession::TTransferCode Transfer;
      Transfer.ToRemote = true;
      Transfer.Move = true;
      Transfer.Paths = { QDir::toNativeSeparators(QDir(Directory).filePath(QStringLiteral("one.txt"))).toStdString(),
        QDir::toNativeSeparators(QDir(Directory).filePath(QStringLiteral("two.txt"))).toStdString() };
      Transfer.Target = "/sub/";
      Transfer.Settings = CoreBridge::CurrentTransferSettings();
      GenerateUrlDialog Dialog(Session, {}, nullptr, &Transfer);
      Check((Dialog.windowTitle() == QStringLiteral("Generate transfer code")) && !Dialog.Pages()->isTabVisible(0) &&
        (Dialog.Pages()->currentIndex() != 0), "WinSCP's title, no URL page");
      Dialog.Pages()->setCurrentIndex(1);
      Dialog.ScriptFormatCombo()->setCurrentIndex(0);
      const QString Script = Dialog.Text();
      Check(Script.contains(QStringLiteral("lcd ")) && Script.contains(QStringLiteral("cd /sub")) &&
        Script.contains(QStringLiteral("put -delete")) && Script.contains(QStringLiteral("one.txt")) && Script.contains(QStringLiteral("two.txt")),
        "a script: to the directory, each file put, moved");
      Dialog.Pages()->setCurrentIndex(2);
      Dialog.LanguageCombo()->setCurrentIndex(0);
      Check(Dialog.Text().contains(QStringLiteral("PutFiles")) && Dialog.Text().contains(QStringLiteral("remotePath")),
        ".NET code: PutFiles, a shared target");
      Transfer.ToRemote = false;
      Transfer.Move = false;
      Transfer.Paths = { "hello.txt" };
      Transfer.Target = QDir::toNativeSeparators(Directory).toStdString();
      GenerateUrlDialog Download(Session, {}, nullptr, &Transfer);
      Download.Pages()->setCurrentIndex(1);
      Download.ScriptFormatCombo()->setCurrentIndex(0);
      Check(Download.Text().contains(QStringLiteral("get hello.txt")) && !Download.Text().contains(QStringLiteral("-delete")),
        "a download: get");
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

    // SFTP to OpenSSH, which calculates no checksums: through the secondary
    // shell (CanCalculateChecksum), opened for it.
    CoreBridge::TSite Sftp = Site;
    Sftp.Protocol = CoreBridge::TSite::TProtocol::Sftp;
    Check(Window.OpenSite(Sftp) && Window.Panel()->ChangeDirectory(HostDirectory + QStringLiteral("/commands")), "an SFTP session");
    CoreBridge::TSession * SftpSession = Window.Session();
    const CoreBridge::TPropertiesInfo Info = SftpSession->PropertiesOf({ "data.txt" });
    Check(!Info.ChecksumAlgorithms.empty() && !SftpSession->CanCalculateChecksumsNow(), "checksums offered through the shell");
    {
      PropertiesDialog Dialog(SftpSession, { "data.txt" }, Window.Panel()->Path(), Info, &Window);
      bool Opened = false;
      Dialog.SetChecksumSession([SftpSession, &Opened]()
      {
        if (!SftpSession->CanCalculateChecksumsNow())
        {
          SftpSession->OpenShellSession();
          Opened = true;
        }
        return true;
      });
      const int Md5 = Dialog.ChecksumAlgorithmCombo()->findText(QStringLiteral("md5"), Qt::MatchFixedString);
      Dialog.ChecksumAlgorithmCombo()->setCurrentIndex(Md5);
      Check((Md5 >= 0) && Dialog.CalculateChecksum() && Opened &&
        Dialog.ChecksumEdit()->text().contains(QStringLiteral("827ccb0eea8a706c4c34a16891f84e7b")),
        "the shell opened for it, the checksum calculated there");
    }
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// Preferences' Environment, Panels > Remote and Local, Drag & Drop: what
// their settings do (P3-19).
void EnvironmentTest(const char * Port, const QString & Directory)
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
    for (const char * Name : { "e1.txt", "e2.txt", "e3.txt", "e4.txt", "e5.txt", "e6.txt" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    LocalPanel * Local = Window.Local();
    RemotePanel * Remote = Window.Panel();
    Local->ChangeDirectory(Directory);
    Remote->ChangeDirectory(QStringLiteral("/sub"));
    Local->DeleteToTrash = false;
    const auto Change = [](const std::function<void(CoreBridge::TPreferences &)> & What)
    {
      CoreBridge::ChangePreferences(What);
    };

    // Deleting: Environment's confirmation, then "Never ask me again".
    int Asked = 0;
    Responder.Custom = [&Asked](QWidget * Modal)
    {
      QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
      if ((Query == nullptr) || (Query->NeverAskAgain() == nullptr))
      {
        return false;
      }
      Asked++;
      Query->NeverAskAgain()->setChecked(true);
      Query->Buttons()[0]->click();
      return true;
    };
    Local->Select({ QStringLiteral("e1.txt") });
    Check(Local->DeleteSelected(true) && !Dir.exists(QStringLiteral("e1.txt")) && (Asked == 1) &&
      !CoreBridge::Preferences().ConfirmDeleting, "a delete asked, with never ask again");
    Local->Select({ QStringLiteral("e2.txt") });
    Check(Local->DeleteSelected(true) && !Dir.exists(QStringLiteral("e2.txt")) && (Asked == 1), "and then not asked");
    Check(CoreBridge::Preferences().ConfirmRecycling, "to the trash still asked");
    Change([](CoreBridge::TPreferences & P) { P.ConfirmDeleting = true; });
    Responder.Custom = nullptr;

    // Transferring: the Copy dialog's "Do not show this dialog box again",
    // then none.
    int Copies = 0;
    bool CheckedAtFirst = false;
    Responder.Custom = [&Copies, &CheckedAtFirst](QWidget * Modal)
    {
      QCheckBox * NeverShowAgain = Modal->findChild<QCheckBox *>(QStringLiteral("NeverShowAgain"));
      QDialog * Dialog = qobject_cast<QDialog *>(Modal);
      if ((NeverShowAgain == nullptr) || (Dialog == nullptr))
      {
        return false;
      }
      Copies++;
      CheckedAtFirst = NeverShowAgain->isChecked();
      NeverShowAgain->setChecked(true);
      Dialog->accept();
      return true;
    };
    Check(Window.Transfer(true, { QStringLiteral("e3.txt") }, QStringLiteral("/sub"), false, true, false) &&
      (Copies == 1) && !CheckedAtFirst && !CoreBridge::Preferences().ConfirmTransferring,
      "the Copy dialog, its \"do not show again\" unchecked, turns it off");
    Check(Window.Transfer(true, { QStringLiteral("e4.txt") }, QStringLiteral("/sub"), false, true, false) && (Copies == 1),
      "then transfers are not confirmed");
    Change([](CoreBridge::TPreferences & P) { P.ConfirmTransferring = true; });
    // A drag's, while Auto: "do not show again" checked to begin with; kept,
    // drags are not confirmed from then on.
    Check(Window.Transfer(true, { QStringLiteral("e5.txt") }, QStringLiteral("/sub"), false, true, false, true) &&
      (Copies == 2) && CheckedAtFirst &&
      (CoreBridge::Preferences().DDTransferConfirmation == CoreBridge::TPreferences::TAutoSwitch::Off) &&
      CoreBridge::Preferences().ConfirmTransferring, "a drag's dialog turns drags' off, not the rest");
    Check(Window.Transfer(true, { QStringLiteral("e6.txt") }, QStringLiteral("/sub"), false, true, false, true) && (Copies == 2),
      "a drag then not confirmed");
    Responder.Custom = nullptr;
    Check(Remote->Names().contains(QStringLiteral("e6.txt")), "uploaded all the same");

    // The beep, for work longer than its seconds.
    Change([](CoreBridge::TPreferences & P) { P.BeepOnFinish = true; P.BeepOnFinishAfter = 0; });
    const int Beeps = Window.Beeps();
    Window.OperationComplete(1);
    Check(Window.Beeps() == Beeps + 1, "a beep when work finishes");
    Change([](CoreBridge::TPreferences & P) { P.BeepOnFinishAfter = 30; });
    Window.OperationComplete(1);
    Check(Window.Beeps() == Beeps + 1, "not for shorter work");

    // Synchronizing: the summary, never shown again once asked.
    {
      QFile File(Dir.filePath(QStringLiteral("e7.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("e7");
    }
    QString Summary;
    Responder.Custom = [&Summary](QWidget * Modal)
    {
      QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
      if ((Query == nullptr) || (Query->NeverAskAgain() == nullptr) || (Query->Buttons().size() != 1))
      {
        return false;
      }
      for (const QLabel * Label : Query->findChildren<QLabel *>())
      {
        Summary += Label->text();
      }
      Summary += Query->NeverAskAgain()->text();
      Query->NeverAskAgain()->setChecked(true);
      Query->Buttons()[0]->click();
      return true;
    };
    CoreBridge::TSyncParams Params;
    Params.PreviewChanges = false;
    // Its first line, "Synchronization was completed."
    const QString Completed = QString::fromStdString(CoreBridge::SyncSummary(CoreBridge::TSyncStatistics(), true, 0, 0,
      [](std::int64_t) { return std::string(); })).section(QLatin1Char('\n'), 0, 0);
    Check(Window.SynchronizeDirectories(Directory, QStringLiteral("/sub"), Params) &&
      !Completed.isEmpty() && Summary.contains(Completed) && !CoreBridge::Preferences().SynchronizeSummary,
      "the synchronization summary, and never again");
    Check(Summary.contains(QueryDialog::tr("Never show this message a&gain")), "with \"Never show this message again\"");
    Responder.Custom = nullptr;

    // Panels > Remote: directories that cannot be entered, hidden when not shown.
    {
      FileListModel Model;
      DirectoriesFirst Sort;
      Sort.setSourceModel(&Model);
      CoreBridge::TRemoteEntry Open;
      Open.Name = "open";
      Open.Directory = true;
      CoreBridge::TRemoteEntry Closed = Open;
      Closed.Name = "closed";
      Closed.Inaccessible = true;
      Model.SetEntries({ Open, Closed }, false);
      Check(Sort.rowCount() == 2, "inaccessible directories shown by default");
      Sort.SetShowInaccessible(false);
      Check(Sort.rowCount() == 1, "and not, when not shown");
    }

    // Panels > Remote's periodic refresh: the panel read again once its
    // seconds pass, and not when off. (Counted: rclone's server caches
    // its listings, so what changes on its disk shows later.)
    int Reads = 0;
    const QMetaObject::Connection Counting = QObject::connect(Remote, &RemotePanel::Changed, [&Reads]() { Reads++; });
    QTest::qWait(1500);
    Check(Reads == 0, "not refreshed by default");
    Change([](CoreBridge::TPreferences & P) { P.RefreshRemotePanel = true; P.RefreshRemotePanelInterval = 1; });
    Window.ApplyPreferences();
    QTest::qWait(2500);
    Check(Reads >= 1, "refreshed every second, when on");
    Change([](CoreBridge::TPreferences & P) { P.RefreshRemotePanel = false; });
    Window.ApplyPreferences();
    Reads = 0;
    QTest::qWait(1500);
    Check(Reads == 0, "and no more when off");
    QObject::disconnect(Counting);

    // Closing with a session: asked, and cancelled, nothing closes.
    QString Closing;
    Responder.Custom = [&Closing](QWidget * Modal)
    {
      QueryDialog * Query = qobject_cast<QueryDialog *>(Modal);
      if ((Query == nullptr) || (Query->NeverAskAgain() == nullptr))
      {
        return false;
      }
      for (const QLabel * Label : Query->findChildren<QLabel *>())
      {
        Closing += Label->text();
      }
      Query->Buttons().back()->click();
      return true;
    };
    Window.close();
    Check(!Closing.isEmpty() && Window.isVisible() && Window.Session() && Window.Session()->Active(),
      "closing with a session asks, and Cancel keeps it");
    Change([](CoreBridge::TPreferences & P) { P.ConfirmClosingSession = false; });
    Closing.clear();
    Window.close();
    Check(Closing.isEmpty() && (Window.TabCount() == 0), "not asked when turned off");
    Responder.Custom = nullptr;
  }
  CoreBridge::Finalize();
}

// Custom command extensions (P3-20): added from a file, configured, run with
// their options, in the menus, removed.
void ExtensionsTest(const char * Port, const QString & Directory)
{
  const QDir Dir(Directory);
#if !defined(_WIN32)
  // The user's extensions folder, the configuration's: the test's own.
  qputenv("XDG_CONFIG_HOME", Dir.filePath(QStringLiteral("config")).toLocal8Bit());
#endif
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
    const auto Write = [&Dir](const QString & Name, const QByteArray & Text)
    {
      QFile File(Dir.filePath(Name));
      File.open(QIODevice::WriteOnly);
      File.write(Text);
      return Dir.filePath(Name);
    };
    // A local extension on the selected file: its run option (the greeting)
    // asked, its Preferences one (loud) and a list kept.
#if defined(_WIN32)
    const QString Extension = Write(QStringLiteral("hello.WinSCPextension.cmd"),
      "@echo off\r\n"
      "rem @name &Say Hello...\r\n"
      "rem @command cmd.exe /c echo %Greeting% %Loud%> \"!.out\"\r\n"
#else
    const QString Extension = Write(QStringLiteral("hello.WinSCPextension.sh"),
      "#!/bin/sh\n"
      "# @name &Say Hello...\n"
      "# @command /bin/sh -c 'echo \"$0 $1\" > \"$2.out\"' \"%Greeting%\" \"%Loud%\" \"!\"\n"
#endif
      "# @description Writes a greeting beside the file\n"
      "# @side Local\n"
      "# @option Greeting -run textbox \"&Greeting:\" \"Hello\"\n"
      "# @option Loud -config checkbox \"&Loud\" \"\" \"-loud\" \"\"\n"
      "# @option - -config label \"Says hello.\"\n"
      "# @option Mode -config dropdownlist \"&Mode:\" \"plain\" \"plain=Plain\" \"fancy=Fancy\"\n");
    Write(QStringLiteral("a.txt"), "a");
    {
      PreferencesDialog Dialog(&Window);
      Dialog.ShowPage(PreferencesDialog::tr("Commands"));
      const int Customs = static_cast<int>(Dialog.Commands().size());
      Check(Dialog.AddExtension(&Extension), "an extension added from its file");
      Check((Dialog.CommandsList()->topLevelItemCount() == Customs + 1) &&
        (Dialog.CommandsList()->currentItem()->text(0) == QStringLiteral("Say Hello...")) &&
        (Dialog.CommandsList()->currentItem()->toolTip(0) == QStringLiteral("Writes a greeting beside the file")),
        "listed after the custom commands, its description its hint");
      Check(!Dialog.ConfigureCommandButton()->isHidden() && Dialog.ConfigureCommandButton()->isEnabled(),
        "Configure in Edit's place");
      const std::vector<CoreBridge::TExtensionOption> Options = Dialog.ExtensionsEdit()->Options(0, 2, std::string());
      Check((Options.size() == 3) && (Options[0].Kind == CoreBridge::TExtensionOption::TKind::CheckBox) &&
        (Options[1].Kind == CoreBridge::TExtensionOption::TKind::Label) &&
        (Options[2].Kind == CoreBridge::TExtensionOption::TKind::DropDownList) && (Options[2].Value == "plain") &&
        (Options[2].Items.size() == 2) && (Options[2].Items[1].second == "Fancy"), "its Preferences options, with their defaults");
      {
        ExtensionOptionsDialog Configure(QStringLiteral("&Say Hello..."), Options, &Dialog);
        QCheckBox * Loud = qobject_cast<QCheckBox *>(Configure.Control(0));
        QComboBox * Mode = qobject_cast<QComboBox *>(Configure.Control(2));
        Check((Configure.windowTitle() == QStringLiteral("Say Hello")) && (Loud != nullptr) && !Loud->isChecked() &&
          (Mode != nullptr) && (Mode->currentText() == QStringLiteral("Plain")), "the options dialog, its controls");
        Loud->setChecked(true);
        Mode->setCurrentIndex(1);
        Check(Configure.Values() == std::vector<std::string>({ "-loud", "", "fancy" }), "and their values, a list's its item's value");
      }
      const std::vector<std::string> Values = { "-loud", "", "fancy" };
      Check(Dialog.ConfigureCommand(&Values), "configured");
      Check(Dialog.ExtensionsEdit()->Options(0, 2, std::string())[2].Value == "fancy", "the values kept");
      // The same again, and one whose requirements are not met: refused.
      const int Errors = Responder.Errors;
      Check(!Dialog.AddExtension(&Extension) && (Responder.Errors == Errors + 1), "not twice");
      const QString Unmet = Write(QStringLiteral("unmet.WinSCPextension.txt"),
        "# @name Never\n# @command echo\n# @require pwsh 999\n");
      Check(!Dialog.AddExtension(&Unmet) && (Responder.Errors == Errors + 2), "nor with its requirements unmet");
      Dialog.Save();
    }
    const std::vector<CoreBridge::TCustomCommandDefinition> Extensions = CoreBridge::Extensions();
    Check((Extensions.size() == 1) && (QString::fromStdString(Extensions[0].Id).startsWith(QStringLiteral("userext"))) &&
      Extensions[0].RunOptions && Extensions[0].ConfigOptions && Extensions[0].Local, "saved, the user's");
    const QString Installed = QString::fromStdString(Extensions.empty() ? std::string() : Extensions[0].FileName);
    Check(QFileInfo::exists(Installed) && (QFileInfo(Installed).absolutePath() != QFileInfo(Extension).absolutePath()),
      "copied into the user's extensions folder");
    // In the Commands menus, after the custom commands.
    Window.UpdateCustomCommands();
    bool Listed = false;
    for (QAction * Action : Window.findChildren<QAction *>())
    {
      Listed = Listed || (Action->text() == QStringLiteral("&Say Hello..."));
    }
    Check(Listed, "in the menus");
    // Run: its run option asked (and kept), the others' values in it.
    Window.Local()->ChangeDirectory(Directory);
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("a.txt") });
    int Asked = 0;
    Responder.Custom = [&Asked](QWidget * Modal)
    {
      ExtensionOptionsDialog * Options = qobject_cast<ExtensionOptionsDialog *>(Modal);
      if (Options == nullptr)
      {
        return false;
      }
      Asked++;
      qobject_cast<QComboBox *>(Options->Control(0))->setEditText(QStringLiteral("Hi"));
      Options->accept();
      return true;
    };
    Check(!Extensions.empty() && Window.RunCustomCommand(Extensions[0]) && (Asked == 1), "run, its option asked");
    Responder.Custom = nullptr;
    QFile Out(Dir.filePath(QStringLiteral("a.txt.out")));
    Check(Out.open(QIODevice::ReadOnly) && QString::fromLocal8Bit(Out.readAll()).trimmed().startsWith(QStringLiteral("Hi -loud")),
      "with the options' values");
    Check(CoreBridge::TSession::ExtensionRunOptions(nullptr, Extensions[0], std::string())[0].Value == "Hi",
      "the run option kept for next time");
    // Removed: its file too.
    {
      PreferencesDialog Dialog(&Window);
      Dialog.CommandsList()->setCurrentItem(Dialog.CommandsList()->topLevelItem(Dialog.CommandsList()->topLevelItemCount() - 1));
      Dialog.RemoveCommand();
      Dialog.Save();
    }
    Check(CoreBridge::Extensions().empty() && !QFileInfo::exists(Installed), "removed, its file deleted");

#if defined(_WIN32)
    // As WinSCP's own extensions: PowerShell over WinSCP's .NET assembly,
    // loaded from %WINSCP_PATH% (the package's dotnet folder, which the
    // shell points it to), the session given as its URL (!E). Native
    // Windows only (Wine has no PowerShell), with the deploy folder's
    // assembly beside the tests (port/tools/vm_tests.py).
    const QDir Program(QCoreApplication::applicationDirPath());
    const bool Wine = (GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") != nullptr);
    if (!Wine && Program.exists(QStringLiteral("dotnet/WinSCPnet.dll")))
    {
      qputenv("WINSCP_PATH", QDir::toNativeSeparators(Program.filePath(QStringLiteral("dotnet"))).toLocal8Bit());
      const QString Listing = QDir::toNativeSeparators(Dir.filePath(QStringLiteral("listing.txt")));
      const QString PowerShell = Write(QStringLiteral("list.WinSCPextension.ps1"),
        "# @name &List Remote\n"
        "# @command powershell.exe -ExecutionPolicy Bypass -File \"%EXTENSION_PATH%\" -sessionUrl \"!E\" -out \"%Out%\"\n"
        "# @require WinSCP 5.15\n"
        "# @require .NET 4.0\n"
        "# @require PowerShell 3.0\n"
        "# @option Out -config file \"&Output:\" \"\"\n"
        "param ($sessionUrl, $out)\n"
        "$assemblyPath = if ($env:WINSCP_PATH) { $env:WINSCP_PATH } else { $PSScriptRoot }\n"
        "Add-Type -Path (Join-Path $assemblyPath \"WinSCPnet.dll\")\n"
        "$options = New-Object WinSCP.SessionOptions\n"
        "$options.ParseUrl($sessionUrl)\n"
        "$options.GiveUpSecurityAndAcceptAnySshHostKey = $true\n"
        "$session = New-Object WinSCP.Session\n"
        "try { $session.Open($options); ($session.ListDirectory('/').Files | ForEach-Object { $_.Name }) -join ',' | Out-File $out }\n"
        "catch { 'ERROR ' + $_.Exception.Message | Out-File $out }\n"
        "finally { $session.Dispose() }\n");
      {
        PreferencesDialog Dialog(&Window);
        Check(Dialog.AddExtension(&PowerShell), "a PowerShell extension, its requirements met");
        const std::vector<std::string> Values = { Listing.toStdString() };
        Check(Dialog.ConfigureCommand(&Values), "its output configured");
        Dialog.Save();
      }
      const std::vector<CoreBridge::TCustomCommandDefinition> Added = CoreBridge::Extensions();
      if (!Added.empty())
      {
        std::printf("powershell extension at %s (%s): %s\n", Added[0].FileName.c_str(),
          QFileInfo::exists(QString::fromStdString(Added[0].FileName)) ? "there" : "missing", Added[0].Command.c_str());
      }
      Check((Added.size() == 1) && Window.RunCustomCommand(Added[0]), "run");
      // Started, not waited for (as WinSCP's local commands without files to
      // upload back): its output, once there.
      QElapsedTimer Waited;
      Waited.start();
      while ((QFileInfo(Listing).size() == 0) && (Waited.elapsed() < 120000))
      {
        QTest::qWait(250);
      }
      QTest::qWait(500);
      QFile Result(Listing);
      QString Names;
      // (powershell.exe's Out-File: UTF-16.)
      if (Result.open(QIODevice::ReadOnly))
      {
        const QByteArray Bytes = Result.readAll();
        Names = (Bytes.startsWith("\xff\xfe") ?
          QString::fromUtf16(reinterpret_cast<const char16_t *>(Bytes.constData() + 2), (Bytes.size() - 2) / 2) :
          QString::fromUtf8(Bytes)).trimmed();
      }
      std::printf("powershell extension: %s\n", Names.toUtf8().constData());
      Check(Names.split(QLatin1Char(',')).contains(QStringLiteral("hello.txt")),
        "it listed the server through WinSCP's assembly driving linscp");
      {
        PreferencesDialog Dialog(&Window);
        Dialog.CommandsList()->setCurrentItem(Dialog.CommandsList()->topLevelItem(Dialog.CommandsList()->topLevelItemCount() - 1));
        Dialog.RemoveCommand();
        Dialog.Save();
      }
    }
#endif
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

    // Start in New Window: what starts the other instance, and what that
    // instance reads of it (a hidden copy of the site, the directories,
    // started at once).
    QStringList Started;
    Window.SetNewInstanceStarter([&Started](const QStringList & Arguments) { Started = Arguments; return true; });
    bool Offered = false;
    QTimer::singleShot(0, &Window, [&Window, &Offered]()
    {
      KeepUpToDateDialog * Dialog = Window.KeepUpToDateWindow();
      Offered = (Dialog != nullptr) && Dialog->StartInNewWindowAction()->isEnabled() && Dialog->StartInNewWindow();
    });
    Window.KeepUpToDate();
    Check(Offered && Started.contains(QStringLiteral("/NewInstance")) && Started.contains(QStringLiteral("/KeepUpToDate")) &&
      Started.contains(QStringLiteral("/sub")) && (Started.indexOf(QStringLiteral("//")) == Started.indexOf(QStringLiteral("/KeepUpToDate")) + 1),
      "started in a new window: its arguments, the directories after \"//\"");
    std::vector<std::string> Arguments;
    for (const QString & Argument : Started)
    {
      Arguments.push_back(Argument.toStdString());
    }
    CoreBridge::SetArguments(Arguments);
    CoreBridge::TKeepUpToDateParams Read;
    bool UseDefaults = false;
    Check(CoreBridge::CommandLineKeepUpToDate(Read, UseDefaults) && UseDefaults && (Read.RemoteDirectory == "/sub") &&
      (QDir(QString::fromStdString(Read.LocalDirectory)) == QDir(Window.Local()->Path())), "read back by the new instance");
    bool CanOpen = false;
    const std::unique_ptr<CoreBridge::TSession> Hidden = CoreBridge::TSession::FromCommandLine(CanOpen);
    Check(Hidden && CanOpen, "its session, the hidden copy of this one");
  }
  CoreBridge::Finalize();
}

// S3's tags, in the Properties dialog's Tags page, against the suite's S3
// server (its bucket "bucket").
// P3-15/P3-18: Files > Locking over WebDAV, against Apache's mod_dav: a
// file locked (locked again, the server refuses), unlocked, locked again.
void LockingTest(const char * Port)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(5, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    Check(Window.OpenUrl(QStringLiteral("dav://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "a WebDAV session");
    CoreBridge::TSession * Session = Window.Session();
    Check((Session != nullptr) && Session->CanLock(), "the server locks (fcLocking)");
    Window.Panel()->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    QAction * Files = MenuAction(Window, { "Files" });
    emit Files->menu()->aboutToShow();
    Check(MenuAction(Window, { "Files", "Locking" })->isEnabled(), "Files > Locking enabled");
    Check(Window.LockSelected(true), "Lock locks it");
    bool Refused = false;
    try
    {
      Session->Lock({ "hello.txt" });
    }
    catch (const CoreBridge::TCoreError &)
    {
      Refused = true;
    }
    Check(Refused, "locked again: the server refuses, it is locked");
    Window.Panel()->Select({ QStringLiteral("hello.txt") });
    Check(Window.LockSelected(false), "Unlock unlocks it");
    Window.Panel()->Select({ QStringLiteral("hello.txt") });
    Check(Window.LockSelected(true) && Window.LockSelected(false), "and it can be locked again");
    Frontend.SetWindow(nullptr);
  }
  CoreBridge::Finalize();
}

// Acl: the server keeps object ACLs (versitygw keeps none: Properties
// still open, its tags too, with no ACL to show).
void Tags(const char * Port, bool Acl)
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
      Check((Info.Properties.Tags.size() == 2) &&
        std::any_of(Info.Properties.Tags.begin(), Info.Properties.Tags.end(),
          [](const auto & Tag) { return (Tag.first == "owner") && (Tag.second == "linscp"); }), "and read back from the server");

      PropertiesDialog Dialog(Session, { "hello.txt" }, QStringLiteral("/bucket"), Info, &Window);
      Check((Dialog.TagList() != nullptr) && (Dialog.TagList()->topLevelItemCount() == 2), "the Tags page lists them");
      Check(!Dialog.OkButton()->isEnabled() && !Dialog.EditTagButton()->isEnabled(), "nothing changed, nothing selected");
      // The env tag, wherever the server listed it (versitygw in any order).
      for (int Index = 0; Index < Dialog.TagList()->topLevelItemCount(); Index++)
      {
        if (Dialog.TagList()->topLevelItem(Index)->text(0) == QStringLiteral("env"))
        {
          Dialog.TagList()->setCurrentItem(Dialog.TagList()->topLevelItem(Index));
        }
      }
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
      // (In any order: S3 promises none, versitygw sorts them.)
      {
        std::set<std::string> Keys;
        for (const auto & Tag : Info.Properties.Tags)
        {
          Keys.insert(Tag.first);
        }
        std::string Seen;
        for (const auto & Tag : Info.Properties.Tags)
        {
          Seen += Tag.first + "=" + Tag.second + ";";
        }
        Check((Info.Properties.Tags.size() == 2) && (Keys == std::set<std::string>{ "owner", "new" }),
          ("as the dialog had them (" + Seen + ")").c_str());
      }
      // S3's ACL, as the rights frame shows it (DisplayAsAcl): files only.
      if (!Acl)
      {
        CoreBridge::TPropertiesInfo Info = Session->PropertiesOf({ "hello.txt" });
        Check(Info.Properties.HasTags, "no object ACLs on the server: the properties still read");
      }
      else
      {
        CoreBridge::TPropertiesInfo Info = Session->PropertiesOf({ "hello.txt" });
        Check(Info.Acl && Info.CanChangeRights && Info.Properties.HasRights, "a file's ACL");
        Check(!Session->PropertiesOf({ "sub" }).Acl, "a folder's none");
        // The owner's full control: read, read ACL and write ACL.
        Check((Info.Properties.Rights.Set & 0400) && (Info.Properties.Rights.Set & 0100) && !(Info.Properties.Rights.Set & 04),
          "the owner's full control, nothing for everyone");
        PropertiesDialog Dialog(Session, { "hello.txt" }, QStringLiteral("/bucket"), Info, &Window);
        RightsFrame * Rights = Dialog.Rights();
        Check(Rights->Acl() && Rights->Check(4)->isHidden() && (Rights->Check(5)->text() == QStringLiteral("R ACL")) &&
          (Rights->Check(0)->text() == QStringLiteral("W ACL")), "shown as WinSCP's: R, R ACL, W ACL");
        // Everyone may read it.
        Rights->Check(9)->setCheckState(Qt::Checked);
        Check(Session->SetProperties({ "hello.txt" }, Dialog.Properties()), "the ACL set");
        Info = Session->PropertiesOf({ "hello.txt" });
        Check((Info.Properties.Rights.Set & 04) && (Info.Properties.Rights.Set & 0400), "read back: everyone reads it too");
      }
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

// Session tabs: sessions side by side, each with its directories; an editor
// saving to its own session whichever tab is active.
void TabsTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(10, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    const QString Url = QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port));
    const QDir Root(QDir(Directory).filePath(QStringLiteral("../root")));
    QDir().mkpath(QDir(Directory).filePath(QStringLiteral("first")));
    QDir().mkpath(QDir(Directory).filePath(QStringLiteral("second")));
    Check(Window.OpenUrl(Url) && (Window.TabCount() == 1) && Window.TabBar()->isVisible(), "a session, a tab");
    CoreBridge::TSession * First = Window.Session();
    Window.Local()->ChangeDirectory(QDir(Directory).filePath(QStringLiteral("first")));
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/sub")), "the first tab in /sub");
    Check(Window.OpenUrl(Url) && (Window.TabCount() == 2) && (Window.ActiveTab() == 1) && (Window.Session() != First),
      "another session opens in a tab of its own");
    CoreBridge::TSession * Second = Window.Session();
    Check(Window.TabSession(0) == First, "the first one kept");
    Window.Local()->ChangeDirectory(QDir(Directory).filePath(QStringLiteral("second")));
    Check(Window.Panel()->Path() == QStringLiteral("/"), "the second at its start");

    Check(Window.SwitchTab(0) && (Window.Session() == First) && (Window.Panel()->Path() == QStringLiteral("/sub")) &&
      (QDir(Window.Local()->Path()) == QDir(QDir(Directory).filePath(QStringLiteral("first")))),
      "back on the first tab: its remote and local directories");
    Check(Window.TabBar()->currentIndex() == 0, "the tab bar says so");

    // P3-15: Tabs > Color: the first tab's color, its swatch, the remote
    // panel's background; the second tab keeps none. Default to Remote Tab
    // off: the New Tab button opens a local tab.
    Window.SetSessionColor(0xCC99FF);
    Check((First->Color() == 0xCC99FF) && !Window.TabBar()->tabIcon(0).isNull() && Window.TabBar()->tabIcon(1).isNull(),
      "Tabs > Color colors the tab, not the other");
    const QColor Rose(0xFF, 0x99, 0xCC);
    const QColor Base = Window.Panel()->View()->palette().color(QPalette::Base);
    Check((Base == Rose) || (Base == Rose.darker(300)), "and the remote panel");
    QAction * DefaultRemote = MenuAction(Window, { "Tabs", "New Tab", "Default to Remote Tab" });
    Check((DefaultRemote != nullptr) && DefaultRemote->isChecked(), "New Tab defaults to a remote tab");
    DefaultRemote->trigger();
    Check(!CoreBridge::Layout().DefaultToNewRemoteTab, "Default to Remote Tab off, kept");
    Window.NewDefaultTab();
    Check((Window.TabCount() == 3) && Window.LocalBrowser(), "the New Tab button opens a local tab then");
    Window.CloseTab(Window.ActiveTab());
    DefaultRemote->trigger();
    Check(Window.SwitchTab(0) && (Window.Session() == First), "(back on the first tab)");
    Window.SetSessionColor(0);
    Check((First->Color() == 0) && Window.TabBar()->tabIcon(0).isNull(), "Default takes the color away");

    // An editor of the first tab's, saved while the second is active.
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/")), "(the root)");
    Window.Panel()->Select({ QStringLiteral("hello.txt") });
    Window.ActivatePanel(false);
    EditorWindow * Editor = Window.EditCurrent();
    Check(Editor != nullptr, "a remote file edited in the first tab");
    Window.CycleTabs(true);
    Check(Window.Session() == Second, "Ctrl+Tab: the next tab");
    if (Editor != nullptr)
    {
      Editor->Editor()->moveCursor(QTextCursor::End);
      Editor->Editor()->insertPlainText(QStringLiteral("from a tab\n"));
      Check(Editor->Save(), "saved with the other tab active");
      Check(WaitUntil([First]() { return First->QueueEmpty(); }) &&
        WaitUntil([&Root]() { return QFile(Root.filePath(QStringLiteral("hello.txt"))).size() > 14; }),
        "uploaded through its own session");
    }

    // Duplicate, close.
    Check(Window.DuplicateTab() && (Window.TabCount() == 3) && (Window.ActiveTab() == 2), "Duplicate Tab: a third tab");
    Check(Window.CloseTab(1) && (Window.TabCount() == 2) && (Window.TabSession(0) == First), "closing one keeps the rest");
    Check(Window.CloseTab(0) && (Window.TabCount() == 1) && !Window.Editors(true).size(), "closing the first closes its editors");
    Check(Window.Session() != nullptr, "the last tab active");

    // A local tab beside the session's: the right panel local there only.
    CoreBridge::TSession * Remaining = Window.Session();
    Window.NewLocalTab();
    Check((Window.TabCount() == 2) && (Window.Session() == nullptr) && Window.OtherLocal()->isVisible(),
      "a local tab beside a session's");
    Check(Window.SwitchTab(0) && (Window.Session() == Remaining) && Window.Panel()->isVisible() &&
      !Window.OtherLocal()->isVisible(), "back on the session's tab, the remote panel");
    Check(Window.SwitchTab(1) && Window.CloseTab(1) && (Window.TabCount() == 1) && (Window.Session() == Remaining) &&
      Window.Panel()->isVisible(), "the local tab closed, the session's active again");

    // A workspace: the tabs saved together, closed, and opened again, each
    // with its directories; then deleted in the site manager.
    Window.Local()->ChangeDirectory(QDir(Directory).filePath(QStringLiteral("first")));
    if (Window.Panel()->Path() != QStringLiteral("/"))
    {
      Check(Window.Panel()->ChangeDirectory(QStringLiteral("/")), "(the first at the root)");
    }
    Check(Window.OpenUrl(Url) && (Window.TabCount() == 2), "(a second tab)");
    Window.Local()->ChangeDirectory(QDir(Directory).filePath(QStringLiteral("second")));
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/sub")), "(the second in /sub)");
    const QString Name = QStringLiteral("Linscp Workspace");
    Check(Window.SaveWorkspace(&Name, false, true), "Save Workspace");
    const std::vector<std::string> Workspaces = CoreBridge::WorkspaceNames();
    Check((Workspaces.size() == 1) && (QString::fromStdString(Workspaces[0]) == Name), "the workspace stored");
    Check(CoreBridge::WorkspaceSessionCount(Name.toStdString()) == 2, "with both tabs' sessions");
    Check(QString::fromStdString(CoreBridge::WorkspaceSettings().AutoWorkspace) == Name, "saved automatically from now on");
    Check(Window.WorkspaceName() == Name, "the name offered next time");
    Check(Window.CloseTab(1) && Window.CloseTab(0) && (Window.TabCount() == 0), "(both tabs closed)");
    Check(Window.OpenWorkspace(Name) && (Window.TabCount() == 2) && (Window.ActiveTab() == 0), "Open the workspace: two tabs");
    Check((Window.Panel()->Path() == QStringLiteral("/")) &&
      (QDir(Window.Local()->Path()) == QDir(QDir(Directory).filePath(QStringLiteral("first")))),
      "the first with its directories");
    Check(Window.SwitchTab(1) && (Window.Panel()->Path() == QStringLiteral("/sub")) &&
      (QDir(Window.Local()->Path()) == QDir(QDir(Directory).filePath(QStringLiteral("second")))),
      "the second with its directories");
    {
      SiteManager Sites(&Window);
      Check(Sites.SelectWorkspace(Name) && Sites.Workspace() == Name, "the site manager lists the workspace");
      Check(!Sites.HostName()->isEnabled(), "not edited there");
      Check(Sites.DeleteSelected(false) && CoreBridge::WorkspaceNames().empty() && !Sites.SelectWorkspace(Name),
        "deleted with its sessions");
    }
    // Saved automatically on closing, and opened again at the next start.
    Window.close();
    Check(Window.TabCount() == 0, "closing the window closes the tabs");
    Check(CoreBridge::WorkspaceSessionCount(Name.toStdString()) == 2, "the workspace saved on closing");
    Check(QString::fromStdString(CoreBridge::WorkspaceSettings().LastStoredSession) == Name, "noted as the last stored");
    MainWindow Next(CoreBridge::Info());
    Frontend.SetWindow(&Next);
    Next.show();
    // (Not this test's arguments.)
    CoreBridge::SetArguments({ "/ini=nul" });
    Next.StartUp();
    Check((Next.TabCount() == 2) && (Next.Panel()->Path() == QStringLiteral("/")) && Next.SwitchTab(1) &&
      (Next.Panel()->Path() == QStringLiteral("/sub")), "the next start opens it");
    Next.close();
    // Named on the command line: opened whatever the automatic one.
    CoreBridge::SetArguments({ "/ini=nul", Name.toStdString() });
    MainWindow Named(CoreBridge::Info());
    Frontend.SetWindow(&Named);
    Named.show();
    CoreBridge::TWorkspaceSettings Workspace = CoreBridge::WorkspaceSettings();
    Workspace.AutoSave = false;
    CoreBridge::SetWorkspaceSettings(Workspace);
    Check(CoreBridge::TSession::CommandLineFolderOrWorkspace() == Name.toStdString(), "a workspace on the command line");
    Named.StartUp();
    Check(Named.TabCount() == 2, "opened at start");
    Named.close();
    Frontend.SetWindow(&Window);
  }
  CoreBridge::Finalize();
}

// The local browser: a tab with a local directory in each panel, copying
// and moving between them (F5, F6, the Copy dialog's mask, a drag), as
// WinSCP's local-local mode.
void LocalTabTest(const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    const QDir A(QDir(Directory).filePath(QStringLiteral("a")));
    const QDir B(QDir(Directory).filePath(QStringLiteral("b")));
    QDir().mkpath(A.filePath(QStringLiteral("sub")));
    QDir().mkpath(B.path());
    auto Write = [](const QString & Path, const QByteArray & Data)
    {
      QFile File(Path);
      File.open(QIODevice::WriteOnly);
      File.write(Data);
    };
    auto Read = [](const QString & Path)
    {
      QFile File(Path);
      return File.open(QIODevice::ReadOnly) ? File.readAll() : QByteArray();
    };
    Write(A.filePath(QStringLiteral("x.txt")), "x\n");
    Write(A.filePath(QStringLiteral("sub/y.txt")), "y\n");

    Window.Local()->ChangeDirectory(A.path());
    Window.NewLocalTab();
    Check(Window.LocalBrowser() && (Window.TabCount() == 1) && (Window.Session() == nullptr), "Local Tab: a tab with no session");
    Check(Window.OtherLocal()->isVisible() && !Window.Panel()->isVisible(), "the right panel local too");
    Check(QDir(Window.OtherLocal()->Path()) == A, "on the local panel's directory to start with");
    Window.OtherLocal()->ChangeDirectory(B.path());
    Check(Window.TabBar()->tabText(0) == QStringLiteral("a – b"), "its title the two directories'");
    Check(Window.SideLocal(false) == Window.OtherLocal(), "the right side is the other local panel");

    // F5: the selection to the other panel, without the dialog.
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("x.txt"), QStringLiteral("sub") });
    Check(Window.TransferSelected(false, false), "Copy between the local panels");
    Check((Read(B.filePath(QStringLiteral("x.txt"))) == "x\n") && (Read(B.filePath(QStringLiteral("sub/y.txt"))) == "y\n"),
      "a file and a directory with its contents copied");
    Check(QFileInfo::exists(A.filePath(QStringLiteral("x.txt"))), "the source kept");
    Check(Window.OtherLocal()->Names().contains(QStringLiteral("x.txt")), "the other panel refreshed");

    // Again: the file there, overwritten after asking (Yes).
    Write(A.filePath(QStringLiteral("x.txt")), "x2\n");
    Window.Local()->Select({ QStringLiteral("x.txt") });
    Check(Window.LocalLocalCopy(false, false) && (Read(B.filePath(QStringLiteral("x.txt"))) == "x2\n") &&
      Responder.LastQuery.contains(QStringLiteral("x.txt")), "an existing file overwritten after asking");

    // The Copy dialog: the target path's mask renames.
    bool DialogShown = false;
    Responder.Custom = [&DialogShown](QWidget * Modal)
    {
      CopyLocalDialog * Dialog = qobject_cast<CopyLocalDialog *>(Modal);
      if (Dialog == nullptr)
      {
        return false;
      }
      DialogShown = true;
      Dialog->DirectoryEdit()->setText(Dialog->Directory() + QStringLiteral("*.bak"));
      Dialog->accept();
      return true;
    };
    Window.Local()->Select({ QStringLiteral("x.txt") });
    Check(Window.LocalLocalCopy(false, true) && DialogShown && QFileInfo::exists(B.filePath(QStringLiteral("x.bak"))),
      "the Copy dialog, its mask renaming");
    Responder.Custom = nullptr;

    // F6 the other way: moved from the right panel to the left.
    Window.ActivatePanel(false);
    Window.OtherLocal()->Select({ QStringLiteral("x.bak") });
    Check(Window.TransferSelected(true, false) && QFileInfo::exists(A.filePath(QStringLiteral("x.bak"))) &&
      !QFileInfo::exists(B.filePath(QStringLiteral("x.bak"))), "Move from the right panel to the left");

    // A drag from the left panel dropped on the right.
    Write(A.filePath(QStringLiteral("z.txt")), "z\n");
    Window.Local()->Refresh();
    TFileDrag Drag;
    Drag.Local = true;
    Drag.Directory = A.path();
    Drag.Names = { QStringLiteral("z.txt") };
    std::unique_ptr<QMimeData> Data(MakeFileDrag(Drag));
    Check(Window.Drop(false, Data.get(), QString(), false, false) && (Read(B.filePath(QStringLiteral("z.txt"))) == "z\n"),
      "a drag between the panels copies");

    // Copying a directory into itself is refused.
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("sub") });
    const QString Into = QDir::toNativeSeparators(A.filePath(QStringLiteral("sub")));
    const QStringList Sub = { A.filePath(QStringLiteral("sub")) };
    Check(!Window.LocalLocalCopy(false, false, &Sub, &Into) && (Responder.Errors > 0), "not into itself");

    // The right side's menus are the local panel's.
    QMenu * Menu = Window.ContextMenu(false, false, QPoint(), false);
    bool Explore = false;
    for (const QAction * Action : Menu->findChildren<QMenu *>().value(0)->actions())
    {
      Explore = Explore || (Action->text() == MainWindow::tr("&Explore Directory"));
    }
    delete Menu;
    Check(Explore, "the right panel's popup is a local one");

    // Tabs: another local tab, each keeping its directories.
    Window.NewLocalTab();
    Window.Local()->ChangeDirectory(B.path());
    Window.OtherLocal()->ChangeDirectory(A.filePath(QStringLiteral("sub")));
    Check((Window.TabCount() == 2) && Window.SwitchTab(0) && (QDir(Window.Local()->Path()) == A) &&
      (QDir(Window.OtherLocal()->Path()) == B), "each tab its own directories");
    Check(Window.SwitchTab(1) && (QDir(Window.Local()->Path()) == B) &&
      (QDir(Window.OtherLocal()->Path()) == QDir(A.filePath(QStringLiteral("sub")))), "and the other's");
    // Kept in a workspace, as WinSCP's (both local directories, no host),
    // and opened again as local tabs.
    const QString Locals = QStringLiteral("Locals");
    Check(Window.SaveWorkspace(&Locals), "a workspace of local tabs saved");
    {
      MainWindow Other(CoreBridge::Info());
      Frontend.SetWindow(&Other);
      Other.show();
      Check(Other.OpenWorkspace(Locals) && (Other.TabCount() == 2) && Other.LocalBrowser() &&
        (QDir(Other.Local()->Path()) == A) && (QDir(Other.OtherLocal()->Path()) == B), "opened: its local tabs");
      Check(Other.SwitchTab(1) && (QDir(Other.Local()->Path()) == B) &&
        (QDir(Other.OtherLocal()->Path()) == QDir(A.filePath(QStringLiteral("sub")))), "each with its directories");
      Frontend.SetWindow(&Window);
    }
    Check(Window.CloseTab(1) && Window.CloseTab(0) && (Window.TabCount() == 0) && Window.Panel()->isVisible() &&
      !Window.OtherLocal()->isVisible(), "the last closed, the remote panel back");
  }
  CoreBridge::Finalize();
}

// P3-09: the dialogs the other tests drive without showing them, shown
// and audited, each page of those with pages; the audit of every other
// window shown runs in all the tests.
void AccessibilityTest(const char * Port)
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
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    const int Before = GAudit->Issues();
    auto Audit = [](QWidget & Dialog)
    {
      Dialog.show();
      QApplication::processEvents();
      GAudit->Audit(&Dialog);
      Dialog.hide();
    };
    {
      AboutDialog Dialog(CoreBridge::Info(), &Window);
      Audit(Dialog);
    }
    {
      LicenseDialog Dialog(LicenseDialog::Product, &Window);
      Audit(Dialog);
    }
    {
      EditMaskDialog Dialog(QStringLiteral("*.txt"), &Window);
      Audit(Dialog);
    }
    {
      EditorPreferencesDialog Dialog(EditorPreferencesDialog::TMode::Add, CoreBridge::TEditorDefinition(), true, &Window);
      Audit(Dialog);
    }
    {
      ImportSitesDialog Dialog(&Window);
      Audit(Dialog);
    }
    {
      SaveWorkspaceDialog Dialog(QStringLiteral("My Workspace"), true, false, false, &Window);
      Audit(Dialog);
    }
    {
      SiteRawDialog Dialog(QStringLiteral("HostName=example.com"), &Window);
      Audit(Dialog);
    }
    {
      TransferSettingsDialog Dialog(CoreBridge::DefaultTransferSettings(), nullptr, &Window);
      Audit(Dialog);
    }
    {
      GenerateUrlDialog Dialog(Window.Session(), { "/hello.txt" }, &Window);
      Audit(Dialog);
    }
    {
      ServerInfoDialog Dialog(Window.Session(), Window.Session()->ServerInfo(), QStringLiteral("/"), &Window);
      Audit(Dialog);
    }
    {
      PreferencesDialog Dialog(&Window);
      const QString Shots = qEnvironmentVariable("LINSCP_SHOTS");
      for (int Page = 0; Page < Dialog.PageCount(); Page++)
      {
        Dialog.ShowPageAt(Page);
        Audit(Dialog);
        // Each page, for a person to look at (the audit keeps the first).
        if (!Shots.isEmpty())
        {
          QDir().mkpath(Shots);
          Dialog.grab().save(QDir(Shots).filePath(QStringLiteral("Preferences_%1.png").arg(Page, 2, 10, QLatin1Char('0'))));
        }
      }
    }
    {
      SiteAdvanced Dialog(CoreBridge::NewSite(), &Window);
      for (int Page = 0; Page < Dialog.Pages()->count(); Page++)
      {
        Dialog.Pages()->setCurrentIndex(Page);
        Audit(Dialog);
      }
    }
    Check(GAudit->Issues() == Before, "every dialog accessible");
  }
  CoreBridge::Finalize();
}

// P3-14's session commands: Save Session as Site, Rename Tab, Clear
// Caches, Reconnect Session, Install Public Key into Server (into the test
// server's root, not this account's).
void SessionCommandsTest(const char * Port, const QString & Key, const QString & Root)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/sub")), "(in /sub)");

    // Save Session as Site: the session as it is, in a folder, no password.
    const QString Site = QStringLiteral("Saved/Mine");
    Check(Window.SaveSessionAsSite(&Site, false) && CoreBridge::SiteExists(Site.toStdString()), "Save Session as Site");
    const CoreBridge::TSite Saved = CoreBridge::LoadSite(Site.toStdString());
    Check((Saved.HostName == "127.0.0.1") && (Saved.Port == std::atoi(Port)) && Saved.Password.empty(),
      "its host and port, not its password");
    Check(Saved.RemoteDirectory == "/sub", "and the directory it is in");
    {
      SaveSiteDialog Dialog(Window.Session()->SaveAsSiteDefaults(), &Window);
      Check(Dialog.SavePasswordCheck()->isEnabled() && !Dialog.SavePasswordCheck()->isChecked(),
        "the dialog offers the password, not checked (not recommended)");
      Dialog.NameEdit()->setText(QStringLiteral("Other"));
      Dialog.FolderCombo()->setEditText(QStringLiteral("Saved"));
      Check(Dialog.Name() == QStringLiteral("Saved/Other"), "its name with the folder");
    }

    // Rename Tab.
    const QString Tab = QStringLiteral("Renamed");
    Check(Window.RenameTab(&Tab) && (Window.TabBar()->tabText(0) == Tab), "Rename Tab");

    // Clear Caches: the directories read are cached; cleared.
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Window.Panel()->ChangeDirectory(QStringLiteral("/sub"));
    Check(!Window.Session()->CachesEmpty(), "(the directories read are cached)");
    Check(Window.ClearCaches() && Window.Session()->CachesEmpty(), "Clear Caches");

    // A lost connection: reconnected, in the directory it was in.
    Window.Session()->DropConnection();
    Check(!Window.Session()->Active() && (Window.TabCount() == 1), "(the connection lost, the tab kept)");
    Check(Window.ReconnectSession() && Window.Session()->Active(), "Reconnect Session");
    Check(QString::fromStdString(Window.Session()->CurrentDirectory()) == QStringLiteral("/sub"), "where it was");

    // Install Public Key: the test server is not OpenSSH (asked, OK); the
    // key, converted to PuTTY's format first, added to the server's
    // authorized keys.
    const QString Ppk = QDir(QFileInfo(Key).path()).filePath(QStringLiteral("install.ppk"));
    try
    {
      CoreBridge::ConvertKey(Key.toStdString(), Ppk.toStdString());
    }
    catch (const CoreBridge::TCoreError &)
    {
    }
    Check(QFileInfo::exists(Ppk), "(a PuTTY key)");
    Check(Window.Session()->CanUploadPublicKey() && !Window.Session()->IsOpenSsh(), "an SFTP server, not OpenSSH");
    Check(Window.InstallPublicKey(&Ppk), "Install Public Key into Server");
    QFile Authorized(QDir(Root).filePath(QStringLiteral(".ssh/authorized_keys")));
    Check(Authorized.open(QIODevice::ReadOnly) && Authorized.readAll().contains("ssh-ed25519 "), "the key is authorized");
    // (SSH changes passwords as it authenticates: offered, as WinSCP does.)
    Check(Window.Session()->CanChangePassword(), "Change Password offered over SSH");
  }
  CoreBridge::Finalize();
}

// P3-14's panel commands: Compare Directories, Synchronize Browsing,
// Calculate Directory Sizes, New File, Path from Other Panel.
void PanelCommandsTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    const QDir Local(Directory);
    const QDir Root(Local.filePath(QStringLiteral("../root")));
    auto Write = [](const QString & Path, const QByteArray & Data, const QDateTime & Time)
    {
      QFile File(Path);
      File.open(QIODevice::WriteOnly);
      File.write(Data);
      File.flush();
      File.setFileTime(Time, QFileDevice::FileModificationTime);
    };
    QDir().mkpath(Local.path());
    QDir().mkpath(Root.filePath(QStringLiteral("cmp")));
    QDir().mkpath(Local.filePath(QStringLiteral("cmp")));
    const QDateTime Old = QDateTime::currentDateTime().addDays(-2);
    const QDateTime New = QDateTime::currentDateTime().addDays(-1);
    // same.txt alike; newer.txt newer locally; remote.txt remote only;
    // local.txt local only; size.txt alike in time, not in size.
    Write(Local.filePath(QStringLiteral("cmp/same.txt")), "same", Old);
    Write(Root.filePath(QStringLiteral("cmp/same.txt")), "same", Old);
    Write(Local.filePath(QStringLiteral("cmp/newer.txt")), "newer", New);
    Write(Root.filePath(QStringLiteral("cmp/newer.txt")), "older", Old);
    Write(Root.filePath(QStringLiteral("cmp/remote.txt")), "remote", Old);
    Write(Local.filePath(QStringLiteral("cmp/local.txt")), "local", Old);
    Write(Local.filePath(QStringLiteral("cmp/size.txt")), "size, longer", Old);
    Write(Root.filePath(QStringLiteral("cmp/size.txt")), "size", Old);

    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Window.Local()->ChangeDirectory(Local.filePath(QStringLiteral("cmp")));
    Check(Window.Panel()->ChangeDirectory(QStringLiteral("/cmp")), "(both in cmp)");
    Check(Window.CompareDirectories(), "Compare Directories finds differences");
    QStringList LocalSelected = Window.Local()->Selected();
    LocalSelected.sort();
    Check(LocalSelected == QStringList({ QStringLiteral("local.txt"), QStringLiteral("newer.txt") }),
      "locally: the newer and the one missing remotely");
    Check(Window.Panel()->Selected() == QStringList({ QStringLiteral("remote.txt") }), "remotely: the one missing locally");
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.CompareBySize = true;
    CoreBridge::SetPreferences(Preferences);
    Window.CompareDirectories();
    Check(Window.Local()->Selected().contains(QStringLiteral("size.txt")) && Window.Panel()->Selected().contains(QStringLiteral("size.txt")),
      "by size too, as Preferences say");
    Preferences.CompareBySize = false;
    CoreBridge::SetPreferences(Preferences);

    // Calculate Directory Sizes: the sizes in the Size column.
    Window.Local()->ChangeDirectory(Local.path());
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Window.ActivatePanel(false);
    Window.Panel()->Select({ QStringLiteral("cmp") });
    Check(Window.CalculateDirectorySizes(), "Calculate Directory Sizes (remote)");
    bool Sized = false;
    for (const CoreBridge::TRemoteEntry & Entry : Window.Panel()->Entries())
    {
      Sized = Sized || ((Entry.Name == "cmp") && (Entry.Size == 4 + 5 + 6 + 4));
    }
    Check(Sized, "the directory's size, its files'");
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("cmp") });
    Check(Window.CalculateDirectorySizes(), "Calculate Directory Sizes (local)");
    Sized = false;
    for (const CoreBridge::TRemoteEntry & Entry : Window.Local()->Entries())
    {
      Sized = Sized || ((Entry.Name == "cmp") && (Entry.Size == 4 + 5 + 5 + 12));
    }
    Check(Sized, "and locally");

    // New File: a local one made and edited; a remote one edited empty,
    // on the server once saved.
    const QString LocalNew = QStringLiteral("new-local.txt");
    EditorWindow * Editor = Window.NewFile(&LocalNew);
    Check((Editor != nullptr) && QFileInfo::exists(Local.filePath(LocalNew)), "New File (local): made and edited");
    if (Editor != nullptr)
    {
      Editor->close();
    }
    Window.ActivatePanel(false);
    const QString RemoteNew = QStringLiteral("new-remote.txt");
    Editor = Window.NewFile(&RemoteNew);
    Check((Editor != nullptr) && !QFileInfo::exists(Root.filePath(RemoteNew)), "New File (remote): edited, not on the server yet");
    if (Editor != nullptr)
    {
      // (Typed: setPlainText would leave it unmodified.)
      Editor->Editor()->insertPlainText(QStringLiteral("new\n"));
      Check(Editor->Save(), "(saved)");
      Check(WaitUntil([&Root, &RemoteNew]() { return QFileInfo::exists(Root.filePath(RemoteNew)); }), "uploaded when saved");
      Editor->close();
    }

    // Synchronize Browsing: into a subdirectory, the other follows; one
    // missing there is offered to be created (Yes).
    QDir().mkpath(Local.filePath(QStringLiteral("cmp/sub")));
    Window.Local()->ChangeDirectory(Local.path());
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Window.SetSynchronizeBrowsing(true);
    Check(Window.SynchronizeBrowsing(), "Synchronize Browsing on");
    Window.Local()->ChangeDirectory(Local.filePath(QStringLiteral("cmp")));
    Check(Window.Panel()->Path() == QStringLiteral("/cmp"), "the remote panel follows the local one");
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Check(QDir(Window.Local()->Path()) == Local, "and back, the local one the remote one");
    Window.Local()->ChangeDirectory(Local.filePath(QStringLiteral("cmp/sub")));
    Check((Window.Panel()->Path() == QStringLiteral("/cmp/sub")) && QFileInfo(Root.filePath(QStringLiteral("cmp/sub"))).isDir(),
      "a directory missing there created, after asking");
    Window.SetSynchronizeBrowsing(false);

    // Path from Other Panel: a local tab's.
    Window.NewLocalTab();
    Window.OtherLocal()->ChangeDirectory(Local.filePath(QStringLiteral("cmp")));
    Check(Window.PathFromOtherPanel(true) && (QDir(Window.Local()->Path()) == QDir(Local.filePath(QStringLiteral("cmp")))),
      "Path from Other Panel");
  }
  CoreBridge::Finalize();
}

// P3-14's clipboard, status bar and queue: files copied and pasted between
// the panels, a path and a URL pasted; the status bar and the queue's
// visibility kept.
void ClipboardTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    // The Copy dialogs, accepted as they are.
    Responder.Custom = [](QWidget * Modal)
    {
      QDialog * Dialog = qobject_cast<QDialog *>(Modal);
      if ((Dialog == nullptr) || ((Dialog->windowTitle() != MainWindow::tr("Copy")) &&
          (qobject_cast<CopyLocalDialog *>(Modal) == nullptr)))
      {
        return false;
      }
      Dialog->accept();
      return true;
    };
    const QDir Local(Directory);
    const QDir Root(Local.filePath(QStringLiteral("../root")));
    QDir().mkpath(Local.filePath(QStringLiteral("into")));
    QFile Up(Local.filePath(QStringLiteral("up.txt")));
    Up.open(QIODevice::WriteOnly);
    Up.write("up");
    Up.close();
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Window.Local()->ChangeDirectory(Local.path());

    // Local files copied, pasted on the remote panel: uploaded.
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("up.txt") });
    Check(Window.CopyToClipboard(), "Copy to Clipboard (local)");
    Check(QApplication::clipboard()->mimeData()->hasUrls(), "as files other programs take");
    Window.ActivatePanel(false);
    Check(Window.PasteFromClipboard() && WaitUntil([&Root]() { return QFileInfo::exists(Root.filePath(QStringLiteral("up.txt"))); }),
      "pasted on the remote panel: uploaded");
    // ... and on a local panel's other directory: copied.
    Window.ActivatePanel(true);
    Window.Local()->ChangeDirectory(Local.filePath(QStringLiteral("into")));
    Check(Window.PasteFromClipboard() && QFileInfo::exists(Local.filePath(QStringLiteral("into/up.txt"))),
      "pasted on a local panel: copied");

    // Remote files copied, the remote panel moved on, pasted locally:
    // downloaded by their paths.
    Window.ActivatePanel(false);
    Window.Panel()->Refresh();
    Window.Panel()->Select({ QStringLiteral("hello.txt") });
    Check(Window.CopyToClipboard(), "Copy to Clipboard (remote)");
    Window.Panel()->ChangeDirectory(QStringLiteral("/sub"));
    // ... pasted on the remote panel: duplicated here (the Duplicate
    // dialog, accepted).
    Check(Window.PasteFromClipboard() && WaitUntil([&Root]() { return QFileInfo::exists(Root.filePath(QStringLiteral("sub/hello.txt"))); }),
      "pasted on the remote panel: duplicated into its directory");
    Window.ActivatePanel(true);
    Check(Window.PasteFromClipboard() && QFileInfo::exists(Local.filePath(QStringLiteral("into/hello.txt"))),
      "pasted on the local panel: downloaded from where it was copied");

    // Text: a path to go to.
    QApplication::clipboard()->setText(QDir::toNativeSeparators(Local.path()));
    Check(Window.PasteFromClipboard() && (QDir(Window.Local()->Path()) == Local), "a pasted path is gone to");
    Window.ActivatePanel(false);
    QApplication::clipboard()->setText(QStringLiteral("/"));
    Check(Window.PasteFromClipboard() && (Window.Panel()->Path() == QStringLiteral("/")), "remotely too");
    // ... a session URL: opened in a tab.
    QApplication::clipboard()->setText(QStringLiteral("sftp://linscp@127.0.0.1:%1/sub/").arg(QString::fromLatin1(Port)));
    Check(Window.PasteFromClipboard() && (Window.TabCount() == 2) && (Window.Panel()->Path() == QStringLiteral("/sub")),
      "a pasted URL is opened");

    // Status Bar and Queue, kept.
    Check(Window.statusBar()->isVisible(), "the status bar shown by default");
    Window.SetStatusBarShown(false);
    Check(!Window.statusBar()->isVisible() && !CoreBridge::Layout().StatusBar, "Status Bar off, kept");
    Window.SetStatusBarShown(true);
    Check(!Window.Queue()->isVisible(), "the queue hidden while empty (WinSCP's default)");
    Window.SetQueueShow(CoreBridge::TLayout::TQueueShow::Show);
    Check(Window.Queue()->isVisible() && (CoreBridge::Layout().QueueShow == CoreBridge::TLayout::TQueueShow::Show),
      "Queue > Show, kept");
    Window.SetQueueShow(CoreBridge::TLayout::TQueueShow::Hide);
    Check(!Window.Queue()->isVisible(), "Queue > Hide");
    Window.SetQueueShow(CoreBridge::TLayout::TQueueShow::HideWhenEmpty);

    // Queue > Process Queue off: a background upload waits, until it is on.
    Check(Window.ProcessQueueAction()->isEnabled() && Window.ProcessQueueAction()->isChecked(),
      "the queue processed by default (EnableQueueByDefault)");
    Window.ProcessQueueAction()->trigger();
    Check(!Window.Session()->QueueEnabled(), "Process Queue off");
    Window.Session()->QueueUpload({ QDir::toNativeSeparators(Local.filePath(QStringLiteral("up.txt"))).toStdString() }, "/sub");
    QTest::qWait(1500);
    Check(!QFileInfo::exists(Root.filePath(QStringLiteral("sub/up.txt"))), "the queued upload waits");
    Window.ProcessQueueAction()->trigger();
    Check(WaitUntil([&Root]() { return QFileInfo::exists(Root.filePath(QStringLiteral("sub/up.txt"))); }),
      "and runs once the queue is processed again");
  }
  CoreBridge::Finalize();
}

// P3-17: every role of every bundled set drawn, in the palette's colour;
// the main window's actions given them, and again as the set changes.
// Out (given): each set's Files menu drawn there, to look at.
void IconsTest(const QString & Out)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    {
      MainWindow Window(CoreBridge::Info());
      Check(!QApplication::windowIcon().isNull() && !Window.windowIcon().isNull() &&
        (QApplication::windowIcon().availableSizes().size() >= 8), "linscp's own icon, every size, on its windows");
    }
    const QStringList Roles = QDir(QStringLiteral(":/iconsets/lucide")).entryList({ QStringLiteral("*.svg") });
    Check(Roles.size() >= 60, "the roles bundled");
    for (const QString & Set : { QStringLiteral("lucide"), QStringLiteral("tabler"), QStringLiteral("fluent") })
    {
      Check(QFile::exists(QStringLiteral(":/iconsets/%1/LICENSE").arg(Set)), "each set with its license");
      IconSet::SetCurrent(Set);
      int Drawn = 0;
      for (const QString & File : Roles)
      {
        const QImage Image = IconSet::Icon(File.chopped(4)).pixmap(QSize(16, 16)).toImage();
        bool Ink = false;
        for (int Y = 0; (Y < Image.height()) && !Ink; Y++)
        {
          for (int X = 0; (X < Image.width()) && !Ink; X++)
          {
            Ink = (qAlpha(Image.pixel(X, Y)) > 0);
          }
        }
        Drawn += Ink ? 1 : 0;
      }
      Check(Drawn == Roles.size(), "every role drawn in every set");
    }
    // In the palette's text colour, whichever the theme.
    IconSet::SetCurrent(QStringLiteral("lucide"));
    const QImage Image = IconSet::Icon(QStringLiteral("delete")).pixmap(QSize(32, 32)).toImage();
    QColor Ink;
    for (int Y = 0; (Y < Image.height()) && !Ink.isValid(); Y++)
    {
      for (int X = 0; (X < Image.width()) && !Ink.isValid(); X++)
      {
        if (qAlpha(Image.pixel(X, Y)) == 255)
        {
          Ink = Image.pixelColor(X, Y);
        }
      }
    }
    Check(Ink.isValid() && (Ink.rgb() == QApplication::palette().color(QPalette::WindowText).rgb()), "in the text colour");
    Check(IconSet::Icon(QStringLiteral("no-such-role")).isNull(), "a role no set has: none");
    {
      AboutDialog About(CoreBridge::Info());
      const QString Html = About.findChild<QTextBrowser *>()->toHtml();
      Check(Html.contains(QStringLiteral("Lucide icons")) && Html.contains(QStringLiteral("iconset:fluent")),
        "About credits the sets, each with its license");
    }

    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.IconSet = "tabler";
    CoreBridge::SetPreferences(Preferences);
    MainWindow Window(CoreBridge::Info());
    Window.show();
    Check(IconSet::Current() == QStringLiteral("tabler"), "the set the preferences say");
    QAction * Find = nullptr;
    for (QAction * Action : Window.findChildren<QAction *>())
    {
      if (Action->text() == MainWindow::tr("&Find Files..."))
      {
        Find = Action;
      }
    }
    Check((Find != nullptr) && !Find->icon().isNull(), "the commands have icons");
    Check(!Window.Panel()->RefreshAction()->icon().isNull(), "the panels' too");
    Preferences.IconSet = "none";
    CoreBridge::SetPreferences(Preferences);
    Window.ApplyPreferences();
    Check((Find != nullptr) && Find->icon().isNull(), "none: the menus without");
    Check(!Window.Panel()->RefreshAction()->icon().isNull(), "the panels' toolbars keep Qt's");
    if (!Out.isEmpty())
    {
      for (const QString & Set : { QStringLiteral("lucide"), QStringLiteral("tabler"), QStringLiteral("fluent") })
      {
        Preferences.IconSet = Set.toStdString();
        CoreBridge::SetPreferences(Preferences);
        Window.ApplyPreferences();
        for (QAction * Action : Window.menuBar()->actions())
        {
          if (Action->text() == MainWindow::tr("&Files"))
          {
            QMenu * Menu = Action->menu();
            Menu->popup(QPoint(0, 0));
            QApplication::processEvents();
            Menu->grab().save(QDir(Out).filePath(Set + QStringLiteral("-files.png")));
            Menu->hide();
          }
        }
        Window.grab().save(QDir(Out).filePath(Set + QStringLiteral("-window.png")));
      }
    }
    Preferences.IconSet.clear();
    CoreBridge::SetPreferences(Preferences);
  }
  CoreBridge::Finalize();
}

// P3-16's toolbars: WinSCP's bands, shown as its default layout has them,
// each toggled and kept, locked, labelled and sized as Options > Toolbars says.
// P3-15: Help's pages and Check for Updates; Options' Tabs, the panels'
// Status Bar, and Queue's Toolbar.
QAction * MenuAction(QMainWindow & Window, const QStringList & Path)
{
  QList<QAction *> Actions = Window.menuBar()->actions();
  QAction * Found = nullptr;
  for (const QString & Name : Path)
  {
    Found = nullptr;
    for (QAction * Action : Actions)
    {
      if (QString(Action->text()).remove(QLatin1Char('&')).remove(QStringLiteral("...")) == Name)
      {
        Found = Action;
        break;
      }
    }
    if (Found == nullptr)
    {
      return nullptr;
    }
    Actions = (Found->menu() != nullptr) ? Found->menu()->actions() : QList<QAction *>();
  }
  return Found;
}

void OptionsHelpTest(const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Window.show();
    QApplication::processEvents();
    QList<QUrl> Opened;
    Window.SetUrlOpener([&Opened](const QUrl & Url) { Opened.append(Url); return true; });
    for (const QStringList & Path : { QStringList{ "Help", "Contents" }, QStringList{ "Help", "Product Homepage" },
           QStringList{ "Help", "Support Forum" }, QStringList{ "Help", "Version History" }, QStringList{ "Help", "Donate" } })
    {
      QAction * Action = MenuAction(Window, Path);
      Check(Action != nullptr, "Help has WinSCP's pages");
      if (Action != nullptr)
      {
        Action->trigger();
      }
    }
    Check((Opened.size() == 5) && Opened[4].toString().contains(QStringLiteral("sponsors")), "Donate: the project's donation page");
    Check((Opened.size() == 5) && (Opened[0].host() == QStringLiteral("winscp.net")) &&
      Opened[1].toString().endsWith(QStringLiteral("/linscp")) && Opened[2].toString().endsWith(QStringLiteral("/issues")) &&
      Opened[3].toString().endsWith(QStringLiteral("/releases")),
      "the documentation WinSCP's, the rest linscp's own pages");
    Check((MenuAction(Window, { "Help", "Contents" })->shortcut() == QKeySequence(Qt::Key_F1)), "Contents is F1");

    Check((MainWindow::CompareVersions(QStringLiteral("v0.10.0"), QStringLiteral("0.9.1")) > 0) &&
      (MainWindow::CompareVersions(QStringLiteral("0.2.0"), QStringLiteral("v0.2.0")) == 0) &&
      (MainWindow::CompareVersions(QStringLiteral("0.2"), QStringLiteral("0.2.1-rc1")) < 0), "versions compare by number");
    // Check for Updates against a releases list of its own.
    const auto Check4 = [&Window, &Directory](const QByteArray & Json, const QString & File)
    {
      const QString Path = QDir(Directory).filePath(File);
      if (!Json.isNull())
      {
        QFile Out(Path);
        Out.open(QIODevice::WriteOnly);
        Out.write(Json);
      }
      Window.UpdatesUrl = QUrl::fromLocalFile(Path);
      bool Done = false;
      QString Latest;
      QString Error;
      bool Newer = false;
      Window.OnUpdatesChecked = [&](const QString & L, bool N, const QString & E) { Done = true; Latest = L; Newer = N; Error = E; };
      Window.CheckForUpdates();
      QElapsedTimer Timer;
      Timer.start();
      while (!Done && (Timer.elapsed() < 10000))
      {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      return std::make_tuple(Done, Latest, Newer, Error);
    };
    {
      const auto [Done, Latest, Newer, Error] =
        Check4(R"([{"tag_name":"v99.0.0","html_url":"https://example/r"}])", QStringLiteral("newer.json"));
      Check(Done && (Latest == QStringLiteral("v99.0.0")) && Newer && Error.isEmpty(), "a newer release is found");
    }
    {
      const auto [Done, Latest, Newer, Error] = Check4(R"([{"tag_name":"v0.0.1"}])", QStringLiteral("older.json"));
      Check(Done && !Newer && Error.isEmpty(), "an older one is not newer");
    }
    {
      const auto [Done, Latest, Newer, Error] = Check4(QByteArray(), QStringLiteral("missing.json"));
      Check(Done && !Error.isEmpty(), "and a failure is an error");
    }
    // "latest"'s single release, as with beta versions off.
    {
      const auto [Done, Latest, Newer, Error] = Check4(R"({"tag_name":"v99.1.0"})", QStringLiteral("latest.json"));
      Check(Done && (Latest == QStringLiteral("v99.1.0")) && Newer, "a single release read too");
    }
    // Updates' period: due when that long since the last check, which the
    // check notes.
    {
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & P) { P.UpdatesPeriod = 0; });
      Check(!Window.CheckForUpdatesIfDue(), "never, by default");
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & P) { P.UpdatesPeriod = 7; P.UpdatesLastCheck = 0; });
      bool Done = false;
      Window.OnUpdatesChecked = [&Done](const QString &, bool, const QString &) { Done = true; };
      Window.UpdatesUrl = QUrl::fromLocalFile(QDir(Directory).filePath(QStringLiteral("older.json")));
      Check(Window.CheckForUpdatesIfDue(), "weekly, never checked: due");
      QElapsedTimer Timer;
      Timer.start();
      while (!Done && (Timer.elapsed() < 10000))
      {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      Check(Done && (std::abs(CoreBridge::Preferences().UpdatesLastCheck - MainWindow::NowAsDateTime()) < 0.01),
        "checked, and noted when");
      Check(!Window.CheckForUpdatesIfDue(), "not due again within the week");
      CoreBridge::ChangePreferences([](CoreBridge::TPreferences & P) { P.UpdatesPeriod = 0; });
    }
    // Interface's theme: dark, light, and back to the system's.
    {
      const bool WasDark = Theme::IsDark();
      Theme::Apply(CoreBridge::TPreferences::TAutoSwitch::On);
      Check(Theme::IsDark(), "the dark theme");
      Theme::Apply(CoreBridge::TPreferences::TAutoSwitch::Off);
      Check(!Theme::IsDark(), "the light one");
      Theme::Apply(CoreBridge::TPreferences::TAutoSwitch::Auto);
      Check(Theme::IsDark() == WasDark, "and the system's again");
    }
    // Over HTTPS for real, when asked (the network is not the suite's).
    if (!qEnvironmentVariableIsEmpty("LINSCP_TEST_NETWORK"))
    {
      Window.UpdatesUrl = QUrl();
      bool Done = false;
      QString Latest, Error;
      Window.OnUpdatesChecked = [&](const QString & L, bool, const QString & E) { Done = true; Latest = L; Error = E; };
      Window.CheckForUpdates();
      QElapsedTimer Timer;
      Timer.start();
      while (!Done && (Timer.elapsed() < 30000))
      {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      std::printf("network: done %d latest %s error %s\n", int(Done), Latest.toUtf8().constData(), Error.toUtf8().constData());
      Check(Done && Error.isEmpty() && !Latest.isEmpty(), "GitHub's releases read over HTTPS");
    }

    QAction * Status = MenuAction(Window, { "Options", "Left Panel", "Status Bar" });
    Check((Status != nullptr) && Status->isChecked() && !Window.Local()->Status()->isHidden(), "the left panel's status bar shown");
    Status->trigger();
    Check(Window.Local()->Status()->isHidden() && !CoreBridge::Layout().LocalStatusBar, "Status Bar hides it, and is kept");
    Status->trigger();
    QAction * RightStatus = MenuAction(Window, { "Options", "Right Panel", "Status Bar" });
    RightStatus->trigger();
    Check(Window.Panel()->Status()->isHidden() && !CoreBridge::Layout().RemoteStatusBar, "the right panel's too");
    RightStatus->trigger();
    QAction * Tabs = MenuAction(Window, { "Options", "Tabs" });
    Check((Tabs != nullptr) && Tabs->isChecked(), "Options > Tabs shown");
    Tabs->trigger();
    Check(Window.TabBar()->isHidden() && !CoreBridge::Layout().Tabs, "Tabs hides the tab bar, and is kept");
    Tabs->trigger();
    QAction * QueueToolbar = MenuAction(Window, { "Options", "Queue", "Toolbar" });
    Check((QueueToolbar != nullptr) && QueueToolbar->isChecked() && Window.Queue()->ToolbarVisible(), "the queue's toolbar shown");
    QueueToolbar->trigger();
    Check(!Window.Queue()->ToolbarVisible() && !CoreBridge::Layout().QueueToolbar, "Queue > Toolbar hides it, and is kept");
    QAction * FileList = MenuAction(Window, { "Options", "Queue", "File List" });
    Check((FileList != nullptr) && !FileList->isChecked() && !Window.Queue()->FileListVisible(), "the queue's file list hidden at first");
    FileList->trigger();
    Check(Window.Queue()->FileListVisible() && CoreBridge::Layout().QueueFileList, "Queue > File List shows it, and is kept");
    FileList->trigger();
    Check(MenuAction(Window, { "Options", "Queue", "Reset Columns Layout" }) != nullptr &&
      MenuAction(Window, { "Options", "Queue", "Customize" }) != nullptr, "Queue's Reset Columns Layout and Customize");
  }
  CoreBridge::Finalize();
}

void ToolbarsTest()
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Window.show();
    QApplication::processEvents();
    auto Bar = [&Window](const QString & Name) { return Window.findChild<QToolBar *>(Name); };
    for (const QString & Name : { QStringLiteral("Commands"), QStringLiteral("Preferences"), QStringLiteral("Transfer") })
    {
      Check((Bar(Name) != nullptr) && Bar(Name)->isVisible(), "WinSCP's default bands shown");
    }
    for (const QString & Name : { QStringLiteral("Session"), QStringLiteral("Sort"), QStringLiteral("CustomCommands") })
    {
      Check((Bar(Name) != nullptr) && !Bar(Name)->isVisible(), "and its hidden ones hidden");
    }
    // P3-15: the Update button shown, the Hot Keys toolbar not, as WinSCP's
    // defaults; Toolbars > Hot Keys Toolbar shows it, its F5 the side's.
    Check((Bar(QStringLiteral("Updates")) != nullptr) && Bar(QStringLiteral("Updates"))->isVisible(), "the Update button shown");
    Check((Bar(QStringLiteral("Toolbar2")) != nullptr) && !Bar(QStringLiteral("Toolbar2"))->isVisible(), "the Hot Keys toolbar hidden");
    QAction * HotKeys = MenuAction(Window, { "Options", "Toolbars", "Hot Keys Toolbar" });
    Check((HotKeys != nullptr) && !HotKeys->isChecked(), "Toolbars > Hot Keys Toolbar unchecked");
    HotKeys->trigger();
    Check(Bar(QStringLiteral("Toolbar2"))->isVisible(), "and shows it");
    {
      QStringList Texts;
      for (QAction * Action : Bar(QStringLiteral("Toolbar2"))->actions())
      {
        Texts.append(Action->text());
      }
      Window.ActivatePanel(true);
      QStringList Local;
      for (QAction * Action : Bar(QStringLiteral("Toolbar2"))->actions())
      {
        Local.append(Action->text());
      }
      Check(Texts.contains(QStringLiteral("F2 Rename")) && Texts.contains(QStringLiteral("F10 Quit")) &&
        Local.contains(QStringLiteral("F5 Upload")), "F2 to F10, the local side's F5 Upload");
    }
    HotKeys->trigger();
    for (TPanelBand Band : { TPanelBand::Path, TPanelBand::History, TPanelBand::Navigation, TPanelBand::File, TPanelBand::Selection })
    {
      Check(Window.Local()->Band(Band)->isVisible() && Window.Panel()->Band(Band)->isVisible() &&
        !Window.Local()->Band(Band)->actions().isEmpty(), "each panel's five bands, each with its buttons");
    }
    // The selective labels: Synchronize's text beside its icon.
    QAction * Synchronize = nullptr;
    for (QAction * Action : Bar(QStringLiteral("Commands"))->actions())
    {
      Synchronize = (Action->text() == MainWindow::tr("&Synchronize...")) ? Action : Synchronize;
    }
    QToolButton * Button = (Synchronize != nullptr) ?
      qobject_cast<QToolButton *>(Bar(QStringLiteral("Commands"))->widgetForAction(Synchronize)) : nullptr;
    Check((Button != nullptr) && (Button->toolButtonStyle() == Qt::ToolButtonTextBesideIcon), "selective text labels");
    Check(Bar(QStringLiteral("Commands"))->iconSize() == QSize(20, 20), "WinSCP's default icon size, Large");
    // The Preferences band's Queue button: its menu, Options > Queue's choices.
    QAction * QueueButton = nullptr;
    for (QAction * Action : Bar(QStringLiteral("Preferences"))->actions())
    {
      QueueButton = (Action->text() == MainWindow::tr("&Queue")) ? Action : QueueButton;
    }
    Check((QueueButton != nullptr) && (QueueButton->menu() != nullptr), "the Queue button has its menu");
    if ((QueueButton != nullptr) && (QueueButton->menu() != nullptr))
    {
      emit QueueButton->menu()->aboutToShow();
      Check((QueueButton->menu()->actions().size() == 3) && !QueueButton->menu()->actions().contains(nullptr),
        "show, hide when empty, hide");
    }

    // Toggled from Options: the Sort band shown, the left File band hidden.
    QAction * SortToggle = nullptr;
    QAction * FileToggle = nullptr;
    QAction * Lock = nullptr;
    QAction * Labels = nullptr;
    QAction * Normal = nullptr;
    for (QAction * Action : Window.findChildren<QAction *>())
    {
      SortToggle = (Action->text() == MainWindow::tr("So&rt Buttons")) ? Action : SortToggle;
      if ((Action->text() == MainWindow::tr("&File Buttons")) && (FileToggle == nullptr))
      {
        FileToggle = Action;
      }
      Lock = (Action->text() == MainWindow::tr("&Lock Toolbars")) ? Action : Lock;
      Labels = (Action->text() == MainWindow::tr("&Show Selective Text Labels")) ? Action : Labels;
      Normal = (Action->text() == MainWindow::tr("&Normal")) ? Action : Normal;
    }
    Check((SortToggle != nullptr) && (FileToggle != nullptr) && (Lock != nullptr) && (Labels != nullptr) && (Normal != nullptr),
      "Options > Toolbars and Left Panel");
    if ((SortToggle != nullptr) && (FileToggle != nullptr) && (Lock != nullptr) && (Labels != nullptr) && (Normal != nullptr))
    {
      SortToggle->trigger();
      FileToggle->trigger();
      Lock->trigger();
      Labels->trigger();
      Normal->trigger();
      Check(Bar(QStringLiteral("Sort"))->isVisible() && !Window.Local()->Band(TPanelBand::File)->isVisible(), "bands toggled");
      Check(!Bar(QStringLiteral("Commands"))->isMovable(), "locked");
      Check(Button->toolButtonStyle() == Qt::ToolButtonIconOnly, "labels off");
      Check(Bar(QStringLiteral("Commands"))->iconSize() == QSize(16, 16), "normal icons");
      const CoreBridge::TLayout Layout = CoreBridge::Layout();
      const QString Stored = QString::fromStdString(Layout.ToolbarsLayout);
      Check(Stored.contains(QStringLiteral("Sort=1:")) && Stored.contains(QStringLiteral("LocalFile=0:")) &&
        Stored.contains(QStringLiteral("Menu=1:TopDock")), "kept as WinSCP's layout, its own bands as they were");
      Check(Layout.LockToolbars && !Layout.SelectiveToolbarText && (Layout.LargerToolbar == 0), "and its settings");
    }
    // The next window, as kept.
    MainWindow Next(CoreBridge::Info());
    Next.show();
    QApplication::processEvents();
    Check(Next.findChild<QToolBar *>(QStringLiteral("Sort"))->isVisible() && !Next.Local()->Band(TPanelBand::File)->isVisible(),
      "the next window as kept");
    // The Sort band sorts the active panel.
    Window.ActivatePanel(true);
    QAction * BySize = nullptr;
    for (QAction * Action : Bar(QStringLiteral("Sort"))->actions())
    {
      BySize = (Action->text() == MainWindow::tr("By &Size")) ? Action : BySize;
    }
    if (BySize != nullptr)
    {
      BySize->trigger();
    }
    Check(Window.Local()->View()->header()->sortIndicatorSection() == FileListModel::SizeColumn, "Sort > By Size");
  }
  CoreBridge::Finalize();
}

// P3-16's Preferences > Panels and Commander: Explorer-style selection,
// incremental search, the double-click, the panel font, the panels' state
// kept per tab.
// P3-16: transfer presets' autoselection rules: kept, edited in the
// preset's dialog, and matched as the panels move.
void PresetRulesTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    const QDir Dir(Directory);
    QDir().mkpath(Dir.filePath(QStringLiteral("into")));

    // Kept with the presets.
    CoreBridge::TTransferPreset Sub;
    Sub.Name = "Sub";
    Sub.Settings = CoreBridge::DefaultTransferSettings();
    Sub.HasRule = true;
    Sub.Rule.RemoteDirectory = "/sub";
    CoreBridge::TTransferPreset Into;
    Into.Name = "Into";
    Into.Settings = Sub.Settings;
    Into.HasRule = true;
    Into.Rule.LocalDirectory = QDir::toNativeSeparators(Dir.filePath(QStringLiteral("into"))).toStdString();
    CoreBridge::TTransferPreset Plain;
    Plain.Name = "Plain";
    Plain.Settings = Sub.Settings;
    CoreBridge::SetTransferPresets({ Sub, Into, Plain });
    const std::vector<CoreBridge::TTransferPreset> Presets = CoreBridge::TransferPresets();
    Check((Presets.size() == 3) && Presets[0].HasRule && (Presets[0].Rule.RemoteDirectory == "/sub") &&
      Presets[1].HasRule && !Presets[2].HasRule, "the rules kept with the presets");
    Check(CoreBridge::TransferPresetRuleInfo(Presets[0].Rule).find("/sub") != std::string::npos, "a rule described");

    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Window.Local()->ChangeDirectory(Directory);
    Check(CoreBridge::CurrentTransferPreset().empty(), "no rule matching: the default");
    // Announced, as WinSCP's default, the responder taking its OK.
    Window.Panel()->ChangeDirectory(QStringLiteral("/sub"));
    Check(CoreBridge::CurrentTransferPreset() == "Sub", "a remote directory's rule: its preset selected");
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Check(CoreBridge::CurrentTransferPreset().empty(), "back where none matches: the one chosen");
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.CopyParamAutoSelectNotice = false;
    CoreBridge::SetPreferences(Preferences);
    Window.Local()->ChangeDirectory(Dir.filePath(QStringLiteral("into")));
    Check(CoreBridge::CurrentTransferPreset() == "Into", "a local directory's rule too");
    Check(Window.statusBar()->currentMessage().contains(QStringLiteral("Into")), "unannounced: noted in the status bar");
    Window.Local()->ChangeDirectory(Directory);

    // The preset's dialog: the rule's masks, Current filling them in.
    QString Name = QStringLiteral("Sub");
    TransferSettingsDialog Dialog(Presets[0].Settings, &Name, &Window);
    CoreBridge::TTransferPresetRule Current;
    Current.HostName = "127.0.0.1";
    Current.RemoteDirectory = "/";
    Dialog.SetRule(true, Presets[0].Rule, &Current);
    Check(Dialog.HasRuleCheck()->isChecked() && (Dialog.RuleEdit(2)->text() == QStringLiteral("/sub")),
      "the rule shown");
    Dialog.CurrentRuleButton()->click();
    Check((Dialog.Rule().HostName == "127.0.0.1") && (Dialog.Rule().RemoteDirectory == "/"), "Current fills in the session's");
    Dialog.HasRuleCheck()->setChecked(false);
    Check(!Dialog.HasRule() && !Dialog.RuleEdit(0)->isEnabled(), "no rule: its masks off");
    TransferSettingsDialog Custom(Presets[0].Settings, nullptr, &Window);
    Check(Custom.HasRuleCheck() == nullptr, "a transfer's own settings have no rule");
  }
  CoreBridge::Finalize();
}

// P3-16: Window's single instance and tray. A second process (this one's
// "instancesend") hands a session URL to the window's server: opened in a
// tab; refused while the window is hidden, that process then opening it.
void InstanceTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    // Its own name, apart from a linscp the user runs.
    const QString Suffix = QDir(Directory).absolutePath();
    InstanceServer Server([&Window](const QStringList & Arguments) { return Window.CommandLineFromAnotherInstance(Arguments); },
      Suffix);
    Check(Server.Listening(), "the window's server listens");
    const QString Url = QStringLiteral("sftp://linscp@127.0.0.1:%1/sub/").arg(QString::fromLatin1(Port));
    const auto SendFrom = [&Suffix, &Url]()
    {
      QProcess Process;
      // The same platform as this run (CI has no display).
      Process.start(QCoreApplication::applicationFilePath(),
        { QStringLiteral("instancesend"), Suffix, Url, QStringLiteral("-platform"), QGuiApplication::platformName() });
      Process.waitForStarted();
      WaitUntil([&Process]() { return Process.state() == QProcess::NotRunning; });
      const int Code = (Process.exitStatus() == QProcess::NormalExit) ? Process.exitCode() : -1;
      if (Code != 0)
      {
        std::printf("instancesend: %d, %s\n", Code, Process.readAllStandardError().constData());
      }
      return Code;
    };
    Check(SendFrom() == 0, "a second instance's command line taken");
    Check(WaitUntil([&Window]() { return (Window.TabCount() == 1) && (Window.Panel()->Path() == QStringLiteral("/sub")); }),
      "its session opened here");
    Window.hide();
    Check(SendFrom() == 3, "refused while the window is hidden");
    Window.show();

    // The tray: where the platform has one, the window hidden in it.
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.MinimizeToTray = true;
    CoreBridge::SetPreferences(Preferences);
    Window.MinimizeToTray();
    std::printf("tray: %s\n", QSystemTrayIcon::isSystemTrayAvailable() ? "available" : "none");
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
      Check(!Window.isVisible() && Window.TrayIcon()->isVisible() && (Window.TrayIcon()->toolTip() == Window.windowTitle()),
        "minimized to the tray, the title its tip");
      Check(SendFrom() == 0, "the tray's window takes command lines too");
      Check(WaitUntil([&Window]() { return Window.isVisible() && (Window.TabCount() == 2); }), "and comes back from the tray");
    }
    else
    {
      Check(Window.isVisible() && (Window.TrayIcon() == nullptr), "no tray: the window stays");
    }
    Window.RestoreFromTray();
    Check(Window.isVisible(), "restored");
    // As Keep Up To Date's Minimize to System Tray: what is open over the
    // window goes with it, and comes back.
    {
      QDialog Over(&Window);
      Over.show();
      Window.MinimizeApplication(true);
      if (QSystemTrayIcon::isSystemTrayAvailable())
      {
        Check(!Over.isVisible() && !Window.isVisible(), "the dialog over it to the tray too");
        Window.RestoreFromTray();
        Check(Over.isVisible() && Window.isVisible(), "and back");
      }
      else
      {
        Check(Window.isMinimized() || !Window.isVisible() || (QGuiApplication::platformName() == QStringLiteral("offscreen")),
          "no tray: minimized");
        Window.showNormal();
      }
    }
  }
  CoreBridge::Finalize();
}

// P3-16: the panels' directory trees (Options > Left/Right Panel > Tree):
// following the panel, taking it where a folder is chosen, kept.
void TreesTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    const QDir Dir(Directory);
    QDir().mkpath(Dir.filePath(QStringLiteral("inner/deeper")));
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Check(!Window.Panel()->TreeVisible() && !Window.TreeAction(false)->isChecked(), "hidden by default, as WinSCP's");

    // The remote one.
    Window.SetTreeVisible(false, true);
    RemoteDirectoryTree * Remote = Window.Panel()->Tree();
    Check(Window.Panel()->TreeVisible() && Window.TreeAction(false)->isChecked() && (Remote->CurrentPath() == QStringLiteral("/")),
      "shown, on the panel's directory");
    const QModelIndex Root = Remote->model()->index(0, 0);
    Remote->expand(Root);
    QModelIndex Sub;
    for (int Row = 0; Row < Remote->model()->rowCount(Root); Row++)
    {
      if (Remote->model()->index(Row, 0, Root).data().toString() == QStringLiteral("sub"))
      {
        Sub = Remote->model()->index(Row, 0, Root);
      }
    }
    Check(Sub.isValid(), "its folders listed");
    Remote->setCurrentIndex(Sub);
    Check(WaitUntil([&Window]() { return Window.Panel()->Path() == QStringLiteral("/sub"); }), "a folder chosen: the panel goes there");
    Window.Panel()->ChangeDirectory(QStringLiteral("/"));
    Check(Remote->CurrentPath() == QStringLiteral("/"), "and it follows the panel");

    // The local one.
    Window.SetTreeVisible(true, true);
    Window.Local()->ChangeDirectory(Directory);
    LocalDirectoryTree * Local = Window.Local()->Tree();
    Check(QDir(Local->CurrentPath()) == Dir, "the local one on the local panel's directory");
    Window.Local()->ChangeDirectory(Dir.filePath(QStringLiteral("inner")));
    Check(QDir(Local->CurrentPath()) == QDir(Dir.filePath(QStringLiteral("inner"))), "following it");
    Window.Local()->ChangeDirectory(Directory);

    // Kept.
    Window.SaveLayout();
    Check(CoreBridge::Layout().LocalTree && CoreBridge::Layout().RemoteTree, "kept, as DriveView");
    Window.ActivatePanel(false);
    Check(Window.TreeAction(false)->shortcut() == QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T) &&
      Window.TreeAction(true)->shortcut().isEmpty(), "Ctrl+Alt+T the active side's");
    Window.SetTreeVisible(true, false);
    Window.SetTreeVisible(false, false);
    Check(!Window.Local()->TreeVisible() && !Window.Panel()->TreeVisible(), "hidden again");
  }
  CoreBridge::Finalize();
}

// P3-16: local custom commands: on a remote file (downloaded for it, what
// the program changed uploaded back), their output copied, on local files.
// Run through the shell on POSIX, as a command line (cmd's) on Windows.
void LocalCommandsTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    const QDir Dir(Directory);
    QDir().mkpath(Dir.path());
    const QDir Root(Dir.filePath(QStringLiteral("../root")));
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    Window.Local()->ChangeDirectory(Directory);
    const auto Read = [](const QString & Path)
    {
      QFile File(Path);
      File.open(QIODevice::ReadOnly);
      return QString::fromUtf8(File.readAll()).trimmed();
    };
    const QString Seen = QDir::toNativeSeparators(Dir.filePath(QStringLiteral("seen.txt")));
    CoreBridge::TCustomCommandDefinition Command;
    Command.Name = "Look";
    Command.Local = true;
#if defined(Q_OS_WIN)
    Command.Command = QStringLiteral("cmd /c type \"!\" > \"%1\"").arg(Seen).toStdString();
#else
    Command.Command = QStringLiteral("cat \"!\" > \"%1\"").arg(Seen).toStdString();
#endif
    // A remote file made through the session (rclone's server lists only
    // those reliably).
    {
      QFile File(Dir.filePath(QStringLiteral("work.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("work\n");
    }
    Window.Local()->Refresh();
    Check(Window.Upload({ QStringLiteral("work.txt") }, QStringLiteral("/"), false), "(a remote file)");
    QFile::remove(Dir.filePath(QStringLiteral("work.txt")));
    Window.Local()->Refresh();
    Window.ActivatePanel(false);
    Window.Panel()->Refresh();
    Window.Panel()->Select({ QStringLiteral("work.txt") });
    Check(Window.RunCustomCommand(Command) && (Read(Seen) == QStringLiteral("work")),
      "on a remote file: run on its downloaded copy");
    Command.Name = "Change";
#if defined(Q_OS_WIN)
    Command.Command = "cmd /c echo changed> \"!\"";
#else
    Command.Command = "echo changed > \"!\"";
#endif
    Check(Window.RunCustomCommand(Command) &&
      WaitUntil([&Read, &Root]() { return Read(Root.filePath(QStringLiteral("work.txt"))) == QStringLiteral("changed"); }),
      "what it changed uploaded back");
    Command.Name = "Say";
    Command.Command = "echo said";
#if defined(Q_OS_WIN)
    Command.Command = "cmd /c echo said";
#endif
    Command.CopyResults = true;
    Check(Window.RunCustomCommand(Command) && (QApplication::clipboard()->text().trimmed() == QStringLiteral("said")),
      "a command's output copied");
    Command.CopyResults = false;
    // On the local panel's files.
    {
      QFile File(Dir.filePath(QStringLiteral("mine.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("mine");
    }
    Window.Local()->Refresh();
    Window.ActivatePanel(true);
    Window.Local()->Select({ QStringLiteral("mine.txt") });
    Command.Name = "Look";
#if defined(Q_OS_WIN)
    Command.Command = QStringLiteral("cmd /c type \"!\" > \"%1\"").arg(Seen).toStdString();
#else
    Command.Command = QStringLiteral("cat \"!\" > \"%1\"").arg(Seen).toStdString();
#endif
    Check(Window.RunCustomCommand(Command) && (Read(Seen) == QStringLiteral("mine")), "on a local file");
    // Properties on the local panel: the system's own, given the files.
    {
      QStringList Shown;
      Window.SetLocalPropertiesShower([&Shown](const QStringList & Paths) { Shown = Paths; return true; });
      Check(Window.Properties() && (Shown.size() == 1) && (QFileInfo(Shown.first()) == QFileInfo(Dir.filePath(QStringLiteral("mine.txt")))),
        "a local file's Properties: the system's");
      Window.SetLocalPropertiesShower(nullptr);
    }
    // A failing one, its output asked for: its exit code an error.
    Command.Name = "Fail";
    Command.CopyResults = true;
#if defined(Q_OS_WIN)
    Command.Command = "cmd /c exit 3";
#else
    Command.Command = "exit 3";
#endif
    const int Errors = Responder.Errors;
    Check(!Window.RunCustomCommand(Command) && (Responder.Errors > Errors), "a failure's exit code an error");
    Command.CopyResults = false;

    // The checklist's Custom Commands: a "diff"-style one (!^!), on a file
    // changed on both sides.
    const QDir Pair(Dir.filePath(QStringLiteral("pair")));
    QDir().mkpath(Pair.path());
    {
      QFile File(Pair.filePath(QStringLiteral("both.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("remote\n");
    }
    Window.Local()->ChangeDirectory(Pair.path());
    Window.ActivatePanel(true);
    Check(Window.Upload({ QStringLiteral("both.txt") }, QStringLiteral("/sub"), false), "(the remote side)");
    {
      QFile File(Pair.filePath(QStringLiteral("both.txt")));
      File.open(QIODevice::WriteOnly);
      File.write("local, longer\n");
    }
    const QString Both = QDir::toNativeSeparators(Dir.filePath(QStringLiteral("both-seen.txt")));
    CoreBridge::TCustomCommandDefinition Diff;
    Diff.Name = "Diff";
    Diff.Local = true;
#if defined(Q_OS_WIN)
    Diff.Command = QStringLiteral("cmd /c type \"!^!\" \"!\" > \"%1\"").arg(Both).toStdString();
#else
    Diff.Command = QStringLiteral("cat \"!^!\" \"!\" > \"%1\"").arg(Both).toStdString();
#endif
    std::vector<CoreBridge::TCustomCommandDefinition> Commands = CoreBridge::CustomCommands();
    Commands.push_back(Diff);
    CoreBridge::SetCustomCommands(Commands);
    CoreBridge::TSyncParams Params;
    // The sizes tell them apart, whatever the second they were written in.
    Params.BySize = true;
    std::unique_ptr<CoreBridge::TSyncChecklist> List =
      Window.Session()->SyncCollect(QDir::toNativeSeparators(Pair.path()).toStdString(), "/sub", Params);
    Check(List && (List->Items().size() == 1) && (List->Items()[0].Action == CoreBridge::TSyncItem::TAction::UploadUpdate),
      "(an update)");
    SyncChecklistDialog Dialog(*List, Pair.path(), QStringLiteral("/sub"), &Window, Window.Session());
    Dialog.SetCustomCommandRunner([&Window](const CoreBridge::TCustomCommandDefinition & Run,
      const std::vector<std::pair<std::string, std::string>> & Pairs)
    {
      for (const auto & Files : Pairs)
      {
        CoreBridge::TSession::RunLocalCustomCommand(Window.Session(), Run, { Files.second }, { Files.first }, false,
          std::string(), CoreBridge::TCustomCommandPrompts(), Window.LocalRunner());
      }
    });
    Dialog.List()->topLevelItem(0)->setSelected(true);
    Check(!Dialog.CustomCommandsButton()->isHidden() && Dialog.CustomCommandsButton()->isEnabled(), "offered for it");
    Dialog.CustomCommandsButton()->menu()->popup(QPoint());
    QStringList Offered;
    for (QAction * Action : Dialog.CustomCommandsButton()->menu()->actions())
    {
      Offered.append(Action->text());
    }
    Dialog.CustomCommandsButton()->menu()->hide();
    Check(Offered.contains(QStringLiteral("Diff")) && !Offered.contains(QStringLiteral("Look")), "the diff-style ones only");
    Check(Dialog.RunCustomCommand(Diff) && (Read(Both).remove(QLatin1Char('\r')) == QStringLiteral("local, longer\nremote")),
      "run on the pair: the local file and the remote one's copy");
  }
  CoreBridge::Finalize();
}

// P3-16: OpenSSH's sites imported, their OpenSSH keys offered for
// conversion to PuTTY's format first (POSIX: core finds ~/.ssh through
// HOME; Windows' profile folder cannot be moved for a test).
int ImportKeysTest(const QString & Key, const QString & Directory)
{
#if defined(Q_OS_WIN)
  Q_UNUSED(Key);
  Q_UNUSED(Directory);
  return 77;
#else
  const QDir Dir(Directory);
  QDir().mkpath(Dir.filePath(QStringLiteral("home/.ssh")));
  {
    QFile Config(Dir.filePath(QStringLiteral("home/.ssh/config")));
    Config.open(QIODevice::WriteOnly);
    Config.write(QStringLiteral("Host keyhost\n  HostName k.example\n  User bob\n  IdentityFile %1\n").arg(Key).toUtf8());
  }
  qputenv("HOME", Dir.filePath(QStringLiteral("home")).toLocal8Bit());
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.AcceptDialogs = true;
    const QString Converted = Dir.filePath(QStringLiteral("converted.ppk"));
    ImportSitesDialog Import(&Window);
    Import.SetKeyFileChooser([&Converted](const QString &) { return Converted; });
    Import.SourceCombo()->setCurrentIndex(CoreBridge::TImportSites::OpenSsh);
    Check((Import.List()->count() == 1) && (Import.List()->item(0)->text() == QStringLiteral("keyhost")), "the config's host");
    Import.OkButton()->click();
    Check(Import.Imported().size() == 1, "imported");
    const CoreBridge::TSite Site = CoreBridge::LoadSite(Import.Imported().front());
    Check((QString::fromStdString(Site.PrivateKeyFile) == Converted) && QFileInfo::exists(Converted),
      "its OpenSSH key converted first, the site given the converted one");
  }
  CoreBridge::Finalize();
  return 0;
#endif
}

// P3-16: shortcuts to the program: a site's (its URL, /desktop), the Send
// To one (/upload). Made in folders of the test's own: the desktop's
// (POSIX, through XDG's user-dirs.dirs) and Send To's (Windows, APPDATA).
void ShortcutsTest(const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    const QDir Dir(Directory);
    QDir().mkpath(Dir.filePath(QStringLiteral("Desktop")));
#if defined(Q_OS_WIN)
    qputenv("APPDATA", QDir::toNativeSeparators(Dir.path()).toLocal8Bit());
    const Integration::TShortcutFolder Folder = Integration::TShortcutFolder::SendTo;
    const QString Expected = Dir.filePath(QStringLiteral("Microsoft/Windows/SendTo"));
#else
    QDir().mkpath(Dir.filePath(QStringLiteral("config")));
    {
      QFile Dirs(Dir.filePath(QStringLiteral("config/user-dirs.dirs")));
      Dirs.open(QIODevice::WriteOnly);
      Dirs.write(QStringLiteral("XDG_DESKTOP_DIR=\"%1\"\n").arg(Dir.filePath(QStringLiteral("Desktop"))).toUtf8());
    }
    qputenv("XDG_CONFIG_HOME", Dir.filePath(QStringLiteral("config")).toLocal8Bit());
    const Integration::TShortcutFolder Folder = Integration::TShortcutFolder::Desktop;
    const QString Expected = Dir.filePath(QStringLiteral("Desktop"));
#endif
    Check(QDir(Integration::ShortcutFolder(Folder)) == QDir(Expected), "(the test's own folder)");

    CoreBridge::TSite Site = CoreBridge::NewSite();
    Site.Name = "Short Cut";
    Site.HostName = "example.com";
    Site.UserName = "user";
    CoreBridge::SaveSite(Site);
    const CoreBridge::TShortcut Shortcut = CoreBridge::SessionShortcut("Short Cut", CoreBridge::TSessionShortcutKind::Site,
      std::string(), { "/Desktop", "/UploadIfAny" });
    Check((Shortcut.Name == "Short Cut") && (std::find(Shortcut.Arguments.begin(), Shortcut.Arguments.end(), "Short%20Cut") != Shortcut.Arguments.end()) &&
      (Shortcut.Arguments.back() == "/UploadIfAny") && (Shortcut.Description.find("Short Cut") != std::string::npos),
      "a site's: named after it, opening its URL");
    const CoreBridge::TShortcut Workspace = CoreBridge::SessionShortcut("Work", CoreBridge::TSessionShortcutKind::Workspace,
      std::string(), {});
    Check(std::find(Workspace.Arguments.begin(), Workspace.Arguments.end(), "/Workspace") != Workspace.Arguments.end(),
      "a workspace's: /workspace");
    const CoreBridge::TShortcut SendTo = CoreBridge::AppShortcut(CoreBridge::SendToShortcutName(), { "/Upload" });
    Check(QString::fromStdString(SendTo.Name).contains(QStringLiteral("(for upload)")), "Send To's, as WinSCP names it");

    QString Path;
    QString Error;
    Check(Integration::CreateShortcut(Shortcut, Folder, Path, Error) && QFileInfo::exists(Path) &&
      (QFileInfo(Path).dir() == QDir(Expected)), "made");
#if !defined(Q_OS_WIN)
    QFile Entry(Path);
    Entry.open(QIODevice::ReadOnly);
    const QString Text = QString::fromUtf8(Entry.readAll());
    Check(Text.contains(QStringLiteral("\"Short%%20Cut\" \"/Desktop\" \"/UploadIfAny\"")) && Text.contains(QStringLiteral("Name=Short Cut")),
      "a desktop entry, its arguments quoted (and % doubled)");
#endif
    Check(Integration::CreateShortcut(SendTo, Folder, Path, Error) && QFileInfo::exists(Path), "and Send To's");
  }
  CoreBridge::Finalize();
}

// P3-16: /upload (what Explorer's "Send to" starts): the session named,
// the files uploaded to its directory without the Copy dialog (/defaults),
// then the window closes.
void UploadSwitchTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    const QDir Dir(Directory);
    QDir().mkpath(Dir.path());
    const QDir Root(Dir.filePath(QStringLiteral("../root")));
    const QString File = Dir.filePath(QStringLiteral("sent.txt"));
    {
      QFile Out(File);
      Out.open(QIODevice::WriteOnly);
      Out.write("sent");
    }
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    CoreBridge::SetArguments({ "/ini=nul", QStringLiteral("sftp://linscp@127.0.0.1:%1/sub/").arg(QString::fromLatin1(Port)).toStdString(),
      "/upload", QDir::toNativeSeparators(File).toStdString(), "/defaults" });
    Window.StartUp();
    Check(QFileInfo::exists(Root.filePath(QStringLiteral("sub/sent.txt"))), "uploaded to the session's directory");
    Check(!Window.isVisible(), "then the window closes");
  }
  CoreBridge::Finalize();
}

// P3-16: trusted host certification authorities: the dialog (a key typed
// or loaded from a file, the hosts checked), kept with the configuration.
void HostCAsTest(const QString & Ini, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.AcceptDialogs = true;
    QDir().mkpath(Directory);
    // A throwaway key's public half.
    const QString Pub = QDir(Directory).filePath(QStringLiteral("ca.pub"));
    {
      QFile File(Pub);
      File.open(QIODevice::WriteOnly);
      File.write("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIN9nUd7u2xYElrRkEPMTdqMDA+tRlTXqos8hCFvvtNk7 linscp-test-ca\n");
    }
    SshHostCADialog Dialog(true, CoreBridge::TSshHostCA(), &Window);
    Check(!Dialog.Buttons()->button(QDialogButtonBox::Ok)->isEnabled(), "a name and key needed");
    Dialog.PublicKeyEdit()->setText(QStringLiteral("not a key"));
    Check(!Dialog.StatusLabel()->text().isEmpty() && !Dialog.StatusLabel()->text().contains(QStringLiteral("SHA256")),
      "a bad key said so");
    Dialog.LoadKeyFile(Pub);
    Check((Dialog.NameEdit()->text() == QStringLiteral("linscp-test-ca")) &&
      Dialog.PublicKeyEdit()->text().startsWith(QStringLiteral("AAAAC3NzaC1lZDI1NTE5")) &&
      Dialog.StatusLabel()->text().contains(QStringLiteral("SHA256")), "loaded from its file: named after its comment, fingerprinted");
    Dialog.HostsEdit()->setText(QStringLiteral("*.example.com"));
    std::string Error;
    int Start = 0;
    int Length = 0;
    Check(CoreBridge::SshHostCAValidityValid("*.example.com", Error, Start, Length) &&
      !CoreBridge::SshHostCAValidityValid("*.example.com &&", Error, Start, Length) && !Error.empty(),
      "validity expressions checked");
    const CoreBridge::TSshHostCA CA = Dialog.CA();
    Check(CA.PermitRsaSha256 && CA.PermitRsaSha512 && !CA.PermitRsaSha1, "core's signature defaults");

    PreferencesDialog Preferences(&Window);
    Preferences.AddHostCA(CA);
    Check(Preferences.HostCAsView()->topLevelItemCount() == 1, "listed");
    Preferences.Save();
    const std::vector<CoreBridge::TSshHostCA> Saved = CoreBridge::SshHostCAs();
    Check((Saved.size() == 1) && (Saved[0].Name == "linscp-test-ca") && (Saved[0].ValidityExpression == "*.example.com") &&
      (Saved[0].PublicKey == CA.PublicKey), "kept");
    QString Text;
    {
      QFile File(Ini);
      File.open(QIODevice::ReadOnly);
      Text = QString::fromUtf8(File.readAll());
    }
    Check(Text.contains(QStringLiteral("SshHostCAs")) && Text.contains(QStringLiteral("Validity=*.example.com")),
      "under core's SshHostCAs key");
  }
  CoreBridge::Finalize();
}

// MasterPasswordTest's other runs: a site read with the master password
// given ("right"), refused ("cancel-save", the site then saved), or with
// none asked for ("none"). Prints what was read.
int MasterPasswordRead(const QString & Mode)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  int Asked = 0;
  CoreBridge::SetMasterPasswordPrompt([&Asked, &Mode]()
  {
    Asked++;
    if (Mode == QStringLiteral("right"))
    {
      CoreBridge::SetMasterPassword("Str0ng!Pass#1");
      return true;
    }
    return false;
  });
  const CoreBridge::TSite Site = CoreBridge::LoadSite("Secret");
  int Saved = -1;
  if (Mode == QStringLiteral("cancel-save"))
  {
    try
    {
      CoreBridge::SaveSite(Site, "Secret");
      Saved = 1;
    }
    catch (const CoreBridge::TCoreError & E)
    {
      Saved = E.Aborted ? 0 : 2;
    }
  }
  std::printf("password=%s locked=%d asked=%d saved=%d\n", Site.Password.c_str(), Site.PasswordsLocked ? 1 : 0, Asked, Saved);
  CoreBridge::SetMasterPasswordPrompt(nullptr);
  CoreBridge::Finalize();
  return 0;
}

// P3-16: the master password. Set, the stored passwords encrypted with it,
// other runs asking for it once (or keeping a site's password they could
// not read); cleared, plain again.
void MasterPasswordTest(const QString & Ini)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.AcceptDialogs = true;
    CoreBridge::TSite Site = CoreBridge::NewSite();
    Site.Name = "Secret";
    Site.HostName = "example.com";
    Site.UserName = "user";
    Site.Password = "pw1";
    CoreBridge::SaveSite(Site);
    const auto Read = [&Ini](const char * Mode)
    {
      QProcess Process;
      // The same platform as this run (CI has no display).
      Process.start(QCoreApplication::applicationFilePath(),
        { QStringLiteral("masterpasswordread"), QStringLiteral("/ini=") + Ini, QString::fromLatin1(Mode),
          QStringLiteral("-platform"), QGuiApplication::platformName() });
      Process.waitForStarted();
      WaitUntil([&Process]() { return Process.state() == QProcess::NotRunning; });
      const QString Output = QString::fromUtf8(Process.readAllStandardOutput()).trimmed();
      const QString Result = Output.split(QLatin1Char('\n')).last().trimmed();
      return Result;
    };
    Check(!CoreBridge::UseMasterPassword() && (Read("none") == QStringLiteral("password=pw1 locked=0 asked=0 saved=-1")),
      "no master password: read as ever");

    // The dialog: a new one, twice; no current one to give yet.
    MasterPasswordDialog Dialog(true, &Window);
    Check(!Dialog.CurrentEdit()->isEnabled() && !Dialog.Buttons()->button(QDialogButtonBox::Ok)->isEnabled(),
      "none set: only the new one asked, OK once given");
    Dialog.NewEdit()->setText(QStringLiteral("a"));
    Dialog.ConfirmEdit()->setText(QStringLiteral("a"));
    Check(Dialog.Buttons()->button(QDialogButtonBox::Ok)->isEnabled(), "both given: OK");
    Check((CoreBridge::MasterPasswordQuality("") < 0) && (CoreBridge::MasterPasswordQuality("abc") == 0) &&
      (CoreBridge::MasterPasswordQuality("Str0ng!Pass#1") > 0), "a password's quality, as WinSCP's");

    Check(CoreBridge::ChangeMasterPassword("Str0ng!Pass#1").empty() && CoreBridge::UseMasterPassword(), "set");
    QString Text;
    {
      // Closed at once: Windows would not let core write it.
      QFile File(Ini);
      File.open(QIODevice::ReadOnly);
      Text = QString::fromUtf8(File.readAll());
    }
    Check(Text.contains(QStringLiteral("UseMasterPassword=1")) && Text.contains(QStringLiteral("MasterPasswordVerifier=")),
      "kept under Security, as WinSCP's");
    Check(CoreBridge::ValidateMasterPassword("Str0ng!Pass#1") && !CoreBridge::ValidateMasterPassword("wrong"), "checked");
    Check(CoreBridge::LoadSite("Secret").Password == "pw1", "this run still reads the site");
    Check(Read("right") == QStringLiteral("password=pw1 locked=0 asked=1 saved=-1"), "another run asks once");
    // Refused: the site's passwords unknown, and it is not saved without it
    // (as WinSCP's Login dialog), so they are not lost.
    Check(Read("cancel-save") == QStringLiteral("password= locked=1 asked=2 saved=0"), "refused: the password unknown, not saved");
    Check(Read("right") == QStringLiteral("password=pw1 locked=0 asked=1 saved=-1"), "the site's password kept");

    Check(CoreBridge::ClearMasterPassword().empty() && !CoreBridge::UseMasterPassword(), "cleared");
    Check(Read("none") == QStringLiteral("password=pw1 locked=0 asked=0 saved=-1"), "plain again");
  }
  CoreBridge::Finalize();
}

void PanelPreferencesTest(const char * Port, const QString & Directory)
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    Frontend.SetWindow(&Window);
    Window.show();
    TResponder Responder;
    Responder.Frontend = &Frontend;
    Responder.Passwords = std::deque<QString>(4, QStringLiteral("secret"));
    Responder.AcceptDialogs = true;
    const QDir Dir(Directory);
    QDir().mkpath(Dir.path());
    for (const char * Name : { "alpha.txt", "beta.log", "gamma.txt" })
    {
      QFile File(Dir.filePath(QString::fromLatin1(Name)));
      File.open(QIODevice::WriteOnly);
      File.write(Name);
    }
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))), "(connected)");
    LocalPanel * Local = Window.Local();
    Local->ChangeDirectory(Directory);
    FileView * View = Local->View();
    Window.ActivatePanel(true);

    // WinSCP's default, Mouse only: the arrows move the focus alone.
    View->setCurrentIndex(View->model()->index(1, 0));
    View->clearSelection();
    QTest::keyClick(View, Qt::Key_Down);
    Check(Local->Selection().isEmpty() && (Local->Selected().size() == 1), "the arrows leave the selection; commands take the focused");
    CoreBridge::TPreferences Preferences = CoreBridge::Preferences();
    Preferences.ExplorerSelection = CoreBridge::TPreferences::TExplorerSelection::MouseAndKeyboard;
    CoreBridge::SetPreferences(Preferences);
    Window.ApplyPreferences();
    QTest::keyClick(View, Qt::Key_Down);
    Check(Local->Selection().size() == 1, "Explorer style: the arrows select");

    // Incremental search, by the name's start, then any part.
    QTest::keyClicks(View, QStringLiteral("gam"));
    Check((View->CurrentEntry() != nullptr) && (View->CurrentEntry()->Name == "gamma.txt"), "typing finds the name");
    Check(Local->Status()->text().contains(QStringLiteral("gam")), "the status says what is typed");
    QTest::keyClick(View, Qt::Key_Escape);
    Check(View->SearchText().isEmpty() && !Local->Status()->text().contains(QStringLiteral("gam")), "Esc ends it");
    Preferences.PanelSearch = CoreBridge::TPreferences::TPanelSearch::AnyPart;
    CoreBridge::SetPreferences(Preferences);
    Window.ApplyPreferences();
    QTest::keyClicks(View, QStringLiteral(".log"));
    Check((View->CurrentEntry() != nullptr) && (View->CurrentEntry()->Name == "beta.log"), "any part of the name");
    QTest::keyClick(View, Qt::Key_Escape);

    // The double-click: Edit (WinSCP's default), Open, Copy.
    Local->Select({ QStringLiteral("alpha.txt") });
    Check(Window.FileActivated(true) && (Window.Editors().size() == 1), "Edit: the editor");
    Window.CloseEditors(false);
    QUrl Opened;
    Window.SetUrlOpener([&Opened](const QUrl & Url) { Opened = Url; return true; });
    Preferences.DoubleClickAction = CoreBridge::TPreferences::TDoubleClick::Open;
    CoreBridge::SetPreferences(Preferences);
    Check(Window.FileActivated(true) && Opened.toLocalFile().endsWith(QStringLiteral("alpha.txt")), "Open: the associated application");
    Window.SetUrlOpener(nullptr);
    Preferences.DoubleClickAction = CoreBridge::TPreferences::TDoubleClick::Copy;
    CoreBridge::SetPreferences(Preferences);
    const QDir Root(Dir.filePath(QStringLiteral("../root")));
    Check(Window.FileActivated(true) && WaitUntil([&Root]() { return QFileInfo::exists(Root.filePath(QStringLiteral("alpha.txt"))); }),
      "Copy: to the other panel, unconfirmed by default");

    // The panel font.
    Preferences.PanelFontName = "Monospace";
    Preferences.PanelFontSize = 15;
    CoreBridge::SetPreferences(Preferences);
    Window.ApplyPreferences();
    Check(View->font().pointSize() == 15, "the panel font");

    // The panels' state kept per tab.
    Local->Select({ QStringLiteral("beta.log"), QStringLiteral("gamma.txt") });
    Check(Window.OpenUrl(QStringLiteral("sftp://linscp@127.0.0.1:%1/").arg(QString::fromLatin1(Port))) && (Window.TabCount() == 2),
      "(a second tab)");
    Local->Select({ QStringLiteral("alpha.txt") });
    Check(Window.SwitchTab(0), "(back on the first)");
    QStringList Kept = Local->Selection();
    Kept.sort();
    Check(Kept == QStringList({ QStringLiteral("beta.log"), QStringLiteral("gamma.txt") }), "its selection as left");
    Preferences = CoreBridge::TPreferences();
    CoreBridge::SetPreferences(Preferences);
  }
  CoreBridge::Finalize();
}

// The menu bar as text, a menu's items indented under it, separators as
// "----": for comparing with WinSCP's (forms/ScpCommander.dfm).
void DumpMenu(QMenu * Menu, int Depth, QString & Out)
{
  // What is filled as the menu opens.
  emit Menu->aboutToShow();
  for (const QAction * Action : Menu->actions())
  {
    if (!Action->isVisible())
    {
      continue;
    }
    Out += QString(Depth * 2, QLatin1Char(' ')) + (Action->isSeparator() ? QStringLiteral("----") : Action->text()) + QLatin1Char('\n');
    if (Action->menu() != nullptr)
    {
      DumpMenu(Action->menu(), Depth + 1, Out);
    }
  }
}

void Menus()
{
  QtFrontend Frontend;
  CoreBridge::Initialize(&Frontend);
  {
    MainWindow Window(CoreBridge::Info());
    QString Out;
    for (const QAction * Action : Window.menuBar()->actions())
    {
      Out += Action->text() + QLatin1Char('\n');
      DumpMenu(Action->menu(), 1, Out);
    }
    std::fputs(Out.toUtf8().constData(), stdout);
  }
  CoreBridge::Finalize();
}

// Qt's complaints that are the shell's bugs fail the run: an action added
// before it exists, say. The rest pass through.
QtMessageHandler GPreviousMessageHandler = nullptr;

void FailOnShellWarnings(QtMsgType Type, const QMessageLogContext & Context, const QString & Message)
{
  if (Message.contains(QStringLiteral("Attempt to insert null action")) ||
      Message.contains(QStringLiteral("Attempt to add null action")))
  {
    Check(false, Message.toUtf8().constData());
  }
  if (GPreviousMessageHandler != nullptr)
  {
    GPreviousMessageHandler(Type, Context, Message);
  }
}

int main(int argc, char ** argv)
{
  GPreviousMessageHandler = qInstallMessageHandler(FailOnShellWarnings);
  QApplication App(argc, argv);
  if ((argc >= 4) && (std::strcmp(argv[1], "instancesend") == 0))
  {
    // InstanceTest's second instance: 0 when the window took the URL.
    QString Error;
    if (SingleInstance::Send({ QString::fromLocal8Bit(argv[3]) }, QString::fromLocal8Bit(argv[2]), &Error))
    {
      return 0;
    }
    std::fprintf(stderr, "%s\n", Error.toUtf8().constData());
    return 3;
  }
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  // Every window shown, audited (P3-09).
  AccessibilityAudit Audit(Failures);
  App.installEventFilter(&Audit);
  GAudit = &Audit;
  if ((argc >= 2) && (std::strcmp(argv[1], "dialogs") == 0))
  {
    Dialogs();
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "localtab") == 0))
  {
    LocalTabTest(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "accessibility") == 0))
  {
    AccessibilityTest(argv[3]);
  }
  else if ((argc >= 6) && (std::strcmp(argv[1], "sessioncommands") == 0))
  {
    SessionCommandsTest(argv[3], QString::fromLocal8Bit(argv[4]), QString::fromLocal8Bit(argv[5]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "panelcommands") == 0))
  {
    PanelCommandsTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "clipboard") == 0))
  {
    ClipboardTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 3) && (std::strcmp(argv[1], "icons") == 0))
  {
    IconsTest((argc >= 4) && (argv[3][0] != '-') ? QString::fromLocal8Bit(argv[3]) : QString());
  }
  else if ((argc >= 3) && (std::strcmp(argv[1], "toolbars") == 0))
  {
    ToolbarsTest();
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "extensions") == 0))
  {
    ExtensionsTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "options") == 0))
  {
    OptionsHelpTest(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "masterpasswordread") == 0))
  {
    return MasterPasswordRead(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "trees") == 0))
  {
    TreesTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "localcommands") == 0))
  {
    LocalCommandsTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "importkeys") == 0))
  {
    const int Result = ImportKeysTest(QString::fromLocal8Bit(argv[3]), QString::fromLocal8Bit(argv[4]));
    if (Result != 0)
    {
      return Result;
    }
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "shortcuts") == 0))
  {
    ShortcutsTest(QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "uploadswitch") == 0))
  {
    UploadSwitchTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "hostcas") == 0))
  {
    // argv[2] is /ini=<file>.
    HostCAsTest(QString::fromLocal8Bit(argv[2] + 5), QString::fromLocal8Bit(argv[3]));
  }
  else if ((argc >= 3) && (std::strcmp(argv[1], "masterpassword") == 0))
  {
    // argv[2] is /ini=<file>.
    MasterPasswordTest(QString::fromLocal8Bit(argv[2] + 5));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "instance") == 0))
  {
    InstanceTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "presetrules") == 0))
  {
    PresetRulesTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 5) && (std::strcmp(argv[1], "panelprefs") == 0))
  {
    PanelPreferencesTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 3) && (std::strcmp(argv[1], "menus") == 0))
  {
    Menus();
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
  else if ((argc >= 5) && (std::strcmp(argv[1], "environment") == 0))
  {
    EnvironmentTest(argv[3], QString::fromLocal8Bit(argv[4]));
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
  else if ((argc >= 5) && (std::strcmp(argv[1], "tabs") == 0))
  {
    TabsTest(argv[3], QString::fromLocal8Bit(argv[4]));
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "startup") == 0))
  {
    StartUpTest(argv[3]);
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "locking") == 0))
  {
    LockingTest(argv[3]);
  }
  else if ((argc >= 4) && (std::strcmp(argv[1], "tags") == 0))
  {
    Tags(argv[3], !((argc >= 5) && (std::strcmp(argv[4], "noacl") == 0)));
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
