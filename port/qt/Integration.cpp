#include "Integration.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QWidget>

#include "CoreBridge.h"
#include "QtFrontend.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#else
#include <QDBusConnection>
#include <QDBusMessage>
#include <QUrl>
#endif

namespace Integration
{

namespace
{

const char DesktopFileName[] = "linscp.desktop";

#if !defined(Q_OS_WIN)
QString AppName()
{
  return QString::fromStdString(CoreBridge::Info().AppName);
}

// What starts linscp, as a desktop entry's Exec: the AppImage itself (not
// its temporary mount), the Flatpak through flatpak run, else the program.
QString ProgramCommand()
{
  const QString AppImage = qEnvironmentVariable("APPIMAGE");
  if (!AppImage.isEmpty())
  {
    return QLatin1Char('"') + AppImage + QLatin1Char('"');
  }
  const QString Flatpak = qEnvironmentVariable("FLATPAK_ID");
  if (!Flatpak.isEmpty())
  {
    return QStringLiteral("flatpak run ") + Flatpak;
  }
  return QLatin1Char('"') + QCoreApplication::applicationFilePath() + QLatin1Char('"');
}

// The desktop entry, as upstream's registration has the command: the
// program with /Unsafe and the URL (RegisterAsUrlHandler).
QString DesktopEntry(bool Handler)
{
  QStringList Types;
  for (const QString & Scheme : Schemes())
  {
    Types.append(QStringLiteral("x-scheme-handler/") + Scheme);
  }
  QString Result = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=%1\n"
    "Comment=%2\n"
    "Exec=%3%4\n"
    "Icon=linscp\n"
    "Terminal=false\n"
    "Categories=Network;FileTransfer;\n").arg(AppName(), QCoreApplication::translate("Integration", "SFTP, SCP, FTP, WebDAV and S3 client"),
      ProgramCommand(), Handler ? QStringLiteral(" /Unsafe %u") : QString());
  if (Handler)
  {
    Result += QStringLiteral("MimeType=%1;\n").arg(Types.join(QLatin1Char(';')));
  }
  return Result;
}

bool WriteFile(const QString & Path, const QByteArray & Data, QString & Error)
{
  QDir().mkpath(QFileInfo(Path).path());
  QFile File(Path);
  if (!File.open(QIODevice::WriteOnly | QIODevice::Truncate) || (File.write(Data) != Data.size()))
  {
    Error = File.errorString();
    return false;
  }
  return true;
}

// The icon, where the desktop entry's Icon=linscp finds it.
void InstallIcon()
{
  const QString Path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
    QStringLiteral("/icons/hicolor/256x256/apps/linscp.png");
  QDir().mkpath(QFileInfo(Path).path());
  QIcon(QStringLiteral(":/icons/linscp.ico")).pixmap(256, 256).save(Path, "PNG");
}

// mimeapps.list's [Default Applications], each scheme's entry this desktop
// file (Set), or the entries naming it removed; the rest kept as it is.
bool UpdateMimeApps(bool Set, QString & Error)
{
  const QString Path = MimeAppsPath();
  QStringList Lines;
  {
    QFile File(Path);
    if (File.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      Lines = QString::fromUtf8(File.readAll()).split(QLatin1Char('\n'));
      while (!Lines.isEmpty() && Lines.last().isEmpty())
      {
        Lines.removeLast();
      }
    }
  }
  QStringList Types;
  for (const QString & Scheme : Schemes())
  {
    Types.append(QStringLiteral("x-scheme-handler/") + Scheme);
  }
  const QString Desktop = QString::fromLatin1(DesktopFileName);
  QString Section;
  QStringList Result;
  int DefaultsEnd = -1;
  for (const QString & Line : Lines)
  {
    const QString Trimmed = Line.trimmed();
    if (Trimmed.startsWith(QLatin1Char('[')))
    {
      Section = Trimmed;
      Result.append(Line);
      if (Section == QStringLiteral("[Default Applications]"))
      {
        DefaultsEnd = Result.size();
      }
      continue;
    }
    const int Equals = Trimmed.indexOf(QLatin1Char('='));
    const QString Key = (Equals > 0) ? Trimmed.left(Equals) : QString();
    if ((Section == QStringLiteral("[Default Applications]")) || (Section == QStringLiteral("[Added Associations]")))
    {
      if (Types.contains(Key))
      {
        QStringList Handlers = Trimmed.mid(Equals + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        Handlers.removeAll(Desktop);
        if (Handlers.isEmpty() || (Set && (Section == QStringLiteral("[Default Applications]"))))
        {
          continue;  // dropped, or replaced below
        }
        Result.append(Key + QLatin1Char('=') + Handlers.join(QLatin1Char(';')) + QLatin1Char(';'));
        continue;
      }
    }
    Result.append(Line);
    if (Section == QStringLiteral("[Default Applications]"))
    {
      DefaultsEnd = Result.size();
    }
  }
  if (Set)
  {
    QStringList Entries;
    for (const QString & Type : Types)
    {
      Entries.append(Type + QLatin1Char('=') + Desktop);
    }
    if (DefaultsEnd < 0)
    {
      if (!Result.isEmpty())
      {
        Result.append(QString());
      }
      Result.append(QStringLiteral("[Default Applications]"));
      DefaultsEnd = Result.size();
    }
    for (int Index = 0; Index < Entries.size(); Index++)
    {
      Result.insert(DefaultsEnd + Index, Entries[Index]);
    }
  }
  return WriteFile(Path, (Result.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), Error);
}

void UpdateDesktopDatabase()
{
  const QString Tool = QStandardPaths::findExecutable(QStringLiteral("update-desktop-database"));
  if (!Tool.isEmpty())
  {
    QProcess::execute(Tool, { QFileInfo(DesktopEntryPath()).path() });
  }
}
#endif

}  // namespace

QStringList Schemes()
{
  // RegisterProtocolsForDefaultPrograms', then RegisterForDefaultProtocols'
  // prefixed ones.
  QStringList Result = { QStringLiteral("ftp"), QStringLiteral("ftps"), QStringLiteral("ftpes"), QStringLiteral("sftp"),
    QStringLiteral("scp"), QStringLiteral("ssh"), QStringLiteral("dav"), QStringLiteral("davs"), QStringLiteral("s3") };
  for (const char * Scheme : { "ftp", "ftps", "ftpes", "http", "https", "ssh", "sftp", "scp" })
  {
    Result.append(QStringLiteral("winscp-") + QString::fromLatin1(Scheme));
  }
  return Result;
}

QString DesktopEntryPath()
{
  return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') +
    QString::fromLatin1(DesktopFileName);
}

QString MimeAppsPath()
{
  return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list");
}

QString SearchPathDirectory()
{
  return QDir::homePath() + QStringLiteral("/.local/bin");
}

bool RegisterUrlHandler(QString & Error)
{
#if defined(Q_OS_WIN)
  try
  {
    CoreBridge::RegisterUrlHandler();
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Error = QString::fromStdString(E.Error.Message);
    return false;
  }
#else
  InstallIcon();
  if (!WriteFile(DesktopEntryPath(), DesktopEntry(true).toUtf8(), Error) || !UpdateMimeApps(true, Error))
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::RegisterUrlError)) + QLatin1Char('\n') + Error;
    return false;
  }
  UpdateDesktopDatabase();
  return true;
#endif
}

bool UnregisterUrlHandler(QString & Error)
{
#if defined(Q_OS_WIN)
  try
  {
    CoreBridge::UnregisterUrlHandler();
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Error = QString::fromStdString(E.Error.Message);
    return false;
  }
#else
  QFile::remove(DesktopEntryPath());
  if (!UpdateMimeApps(false, Error))
  {
    return false;
  }
  UpdateDesktopDatabase();
  return true;
#endif
}

bool MakeDefaultUrlHandler(QString & Error)
{
#if defined(Q_OS_WIN)
  try
  {
    CoreBridge::MakeDefaultUrlHandler();
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Error = QString::fromStdString(E.Error.Message);
    return false;
  }
#else
  // Registering makes it the default already.
  return RegisterUrlHandler(Error);
#endif
}

bool CreateDesktopIcon(bool AllUsers, QString & Error)
{
#if defined(Q_OS_WIN)
  QString Desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  if (AllUsers)
  {
    Desktop = qEnvironmentVariable("PUBLIC") + QStringLiteral("\\Desktop");
  }
  const QString Link = QDir(Desktop).filePath(QString::fromStdString(CoreBridge::Info().AppName) + QStringLiteral(".lnk"));
  QFile::remove(Link);
  if (!QFile::link(QCoreApplication::applicationFilePath(), Link))
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateShortcutError));
    return false;
  }
  return true;
#else
  Q_UNUSED(AllUsers);
  InstallIcon();
  const QString Path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + QLatin1Char('/') +
    QString::fromLatin1(DesktopFileName);
  if (!WriteFile(Path, DesktopEntry(false).toUtf8(), Error))
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateShortcutError)) + QLatin1Char('\n') + Error;
    return false;
  }
  // Desktops start only executable entries.
  QFile::setPermissions(Path, QFile::permissions(Path) | QFileDevice::ExeOwner | QFileDevice::ExeUser);
  return true;
#endif
}

bool AddToSearchPath(QString & Error)
{
#if defined(Q_OS_WIN)
  try
  {
    CoreBridge::AddToSearchPath(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdString());
    return true;
  }
  catch (const CoreBridge::TCoreError & E)
  {
    Error = QString::fromStdString(E.Error.Message);
    return false;
  }
#else
  // The programs linked into ~/.local/bin, which the user's PATH has.
  const QDir Bin(SearchPathDirectory());
  QDir().mkpath(Bin.path());
  for (const QString & Name : { QStringLiteral("linscp"), QStringLiteral("linscp-cli") })
  {
    const QString Target = QDir(QCoreApplication::applicationDirPath()).filePath(Name);
    if (!QFileInfo::exists(Target))
    {
      continue;
    }
    const QString Link = Bin.filePath(Name);
    QFile::remove(Link);
    if (!QFile::link(Target, Link))
    {
      Error = QStringLiteral("%1 -> %2").arg(Link, Target);
      return false;
    }
  }
  return true;
#endif
}

QString ShortcutFolder(TShortcutFolder Folder)
{
#if defined(Q_OS_WIN)
  if (Folder == TShortcutFolder::SendTo)
  {
    // FOLDERID_SendTo.
    return QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA")) + QStringLiteral("/Microsoft/Windows/SendTo");
  }
#else
  Q_UNUSED(Folder);
#endif
  return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

bool CreateShortcut(const CoreBridge::TShortcut & Shortcut, TShortcutFolder Folder, QString & Path, QString & Error)
{
  const QString ShortcutError = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateShortcutError));
  const QString Directory = ShortcutFolder(Folder);
  QDir().mkpath(Directory);
  const QString Name = QString::fromStdString(Shortcut.Name);
#if defined(Q_OS_WIN)
  // As CreateDesktopShortCut: the program, its arguments quoted as its
  // command line has them, its own icon.
  QStringList Quoted;
  for (const std::string & Argument : Shortcut.Arguments)
  {
    QString Text = QString::fromStdString(Argument);
    if (Text.contains(QLatin1Char(' ')) || Text.contains(QLatin1Char('"')) || Text.isEmpty())
    {
      Text = QLatin1Char('"') + Text.replace(QLatin1Char('"'), QStringLiteral("\\\"")) + QLatin1Char('"');
    }
    Quoted.append(Text);
  }
  Path = QDir(Directory).filePath(Name + QStringLiteral(".lnk"));
  const std::wstring Exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
  const std::wstring Arguments = Quoted.join(QLatin1Char(' ')).toStdWString();
  const std::wstring Description = QString::fromStdString(Shortcut.Description).toStdWString();
  const std::wstring LinkPath = QDir::toNativeSeparators(Path).toStdWString();
  const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool Result = false;
  IShellLinkW * Link = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&Link))))
  {
    Link->SetPath(Exe.c_str());
    Link->SetArguments(Arguments.c_str());
    Link->SetDescription(Description.c_str());
    Link->SetShowCmd(SW_SHOW);
    Link->SetIconLocation(Exe.c_str(), 0);
    IPersistFile * File = nullptr;
    if (SUCCEEDED(Link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&File))))
    {
      Result = SUCCEEDED(File->Save(LinkPath.c_str(), TRUE));
      File->Release();
    }
    Link->Release();
  }
  if (SUCCEEDED(Init))
  {
    CoUninitialize();
  }
  if (!Result)
  {
    Error = ShortcutError;
  }
  return Result;
#else
  Q_UNUSED(Folder);
  // A desktop entry's Exec, quoted as the specification has it.
  QStringList Exec;
  for (const std::string & Argument : Shortcut.Arguments)
  {
    QString Text = QString::fromStdString(Argument);
    for (const QChar Special : { QLatin1Char('\\'), QLatin1Char('"'), QLatin1Char('`'), QLatin1Char('$') })
    {
      Text.replace(Special, QStringLiteral("\\") + Special);
    }
    Exec.append(QLatin1Char('"') + Text.replace(QLatin1Char('%'), QStringLiteral("%%")) + QLatin1Char('"'));
  }
  InstallIcon();
  Path = QDir(Directory).filePath(Name + QStringLiteral(".desktop"));
  const QString Entry = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=%1\n"
    "Comment=%2\n"
    "Exec=%3 %4\n"
    "Icon=linscp\n"
    "Terminal=false\n").arg(Name, QString(QString::fromStdString(Shortcut.Description)).replace(QLatin1Char('\n'), QLatin1Char(' ')),
      ProgramCommand(), Exec.join(QLatin1Char(' ')));
  if (!WriteFile(Path, Entry.toUtf8(), Error))
  {
    Error = ShortcutError + QLatin1Char('\n') + Error;
    return false;
  }
  // Desktops start only executable entries.
  QFile::setPermissions(Path, QFile::permissions(Path) | QFileDevice::ExeOwner | QFileDevice::ExeUser);
  return true;
#endif
}

bool AskAndCreateShortcut(QWidget * Parent, const QString & Question, const CoreBridge::TShortcut & Shortcut,
  TShortcutFolder Folder)
{
  QtFrontend Frontend;
  Frontend.SetWindow(Parent);
  if (!Question.isEmpty())
  {
    CoreBridge::TQuery Query;
    Query.Type = CoreBridge::TQueryType::Confirmation;
    Query.Message = Question.toStdString();
    CoreBridge::TQueryButton Yes;
    Yes.Answer = CoreBridge::AnswerYes;
    Yes.Caption = "&Yes";
    CoreBridge::TQueryButton No;
    No.Answer = CoreBridge::AnswerNo;
    No.Caption = "&No";
    Query.Buttons = { Yes, No };
    Query.CancelAnswer = CoreBridge::AnswerNo;
    if (Frontend.Query(Query).Answer != CoreBridge::AnswerYes)
    {
      return false;
    }
  }
  QString Path;
  QString Error;
  if (!CreateShortcut(Shortcut, Folder, Path, Error))
  {
    CoreBridge::TError Message;
    Message.Message = Error.toStdString();
    Frontend.ShowError(Message);
    return false;
  }
  return true;
}

bool ShowLocalProperties(const QStringList & Paths, QString & Error)
{
  if (Paths.isEmpty())
  {
    return false;
  }
#if defined(Q_OS_WIN)
  // SHMultiFileProperties over the files' absolute item IDs.
  std::vector<PIDLIST_ABSOLUTE> Items;
  for (const QString & Path : Paths)
  {
    PIDLIST_ABSOLUTE Item = nullptr;
    if (SUCCEEDED(SHParseDisplayName(QDir::toNativeSeparators(Path).toStdWString().c_str(), nullptr, &Item, 0, nullptr)))
    {
      Items.push_back(Item);
    }
  }
  bool Result = false;
  if (!Items.empty())
  {
    // Absolute IDs, the desktop their parent.
    std::vector<PCIDLIST_ABSOLUTE> Absolute(Items.begin(), Items.end());
    IDataObject * Data = nullptr;
    if (SUCCEEDED(SHCreateDataObject(nullptr, static_cast<UINT>(Absolute.size()),
          reinterpret_cast<PCUITEMID_CHILD_ARRAY>(Absolute.data()), nullptr, IID_IDataObject, reinterpret_cast<void **>(&Data))))
    {
      Result = SUCCEEDED(SHMultiFileProperties(Data, 0));
      Data->Release();
    }
  }
  for (PIDLIST_ABSOLUTE Item : Items)
  {
    CoTaskMemFree(Item);
  }
  if (!Result)
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported));
  }
  return Result;
#else
  QStringList Uris;
  for (const QString & Path : Paths)
  {
    Uris.append(QUrl::fromLocalFile(Path).toString());
  }
  QDBusMessage Call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
    QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1"),
    QStringLiteral("ShowItemProperties"));
  Call << Uris << QString();
  const QDBusMessage Reply = QDBusConnection::sessionBus().call(Call, QDBus::Block, 10000);
  if (Reply.type() == QDBusMessage::ErrorMessage)
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported)) + QLatin1Char('\n') + Reply.errorMessage();
    return false;
  }
  return true;
#endif
}

}  // namespace Integration
