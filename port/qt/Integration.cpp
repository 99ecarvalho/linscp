#include "Integration.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QGuiApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QMimeDatabase>
#include <QPixmap>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QWidget>

#include "CoreBridge.h"
#include "QtFrontend.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objectarray.h>
#include <propkey.h>
#include <propsys.h>
#include <propvarutil.h>
#else
#include <QDBusConnection>
#include <QDBusMessage>
#include <QUrl>
#endif

namespace Integration
{

namespace
{

// The desktop entry's and icon's names: the application ID's; in older
// versions linscp's own (cleaned up at start, CleanUpOldRegistration).
QString DesktopFileName()
{
  return QString::fromLatin1(AppId) + QStringLiteral(".desktop");
}
// SetProgramForTests'.
QString TestProgram;

#if !defined(Q_OS_WIN)
const char OldDesktopFileName[] = "linscp.desktop";
const char OldIconName[] = "linscp";

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
    "Icon=%5\n"
    "Terminal=false\n"
    "Categories=Network;FileTransfer;\n").arg(AppName(), QCoreApplication::translate("Integration", "SFTP, SCP, FTP, WebDAV and S3 client"),
      ProgramCommand(), Handler ? QStringLiteral(" /Unsafe %u") : QString(), QString::fromLatin1(AppId));
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

// The user's icon (hicolor, 256 px) named Name.
QString IconPath(const QString & Name)
{
  return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
    QStringLiteral("/icons/hicolor/256x256/apps/") + Name + QStringLiteral(".png");
}

// The icon, where the desktop entry's Icon=<AppId> finds it: linscp's own
// when it registers itself; a package's and a Flatpak's are installed.
void InstallIcon()
{
  if (!SelfRegisters())
  {
    return;
  }
  const QString Path = IconPath(QString::fromLatin1(AppId));
  QDir().mkpath(QFileInfo(Path).path());
  QIcon(QStringLiteral(":/icons/linscp.ico")).pixmap(256, 256).save(Path, "PNG");
}

// A desktop entry an older linscp wrote (Icon=linscp; with Renamed, one
// whose icon CleanUpOldRegistration renamed too), Handler whether it was the
// URL handler's (its MimeType=).
bool IsOldEntry(const QString & Path, bool * Handler = nullptr, bool Renamed = false)
{
  QFile File(Path);
  if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    return false;
  }
  const QStringList Lines = QString::fromUtf8(File.readAll()).split(QLatin1Char('\n'));
  const bool Result = Lines.contains(QStringLiteral("[Desktop Entry]")) &&
    (Lines.contains(QStringLiteral("Icon=") + QString::fromLatin1(OldIconName)) ||
     (Renamed && Lines.contains(QStringLiteral("Icon=") + QString::fromLatin1(AppId))));
  if (Handler != nullptr)
  {
    *Handler = Result &&
      std::any_of(Lines.begin(), Lines.end(), [](const QString & Line) { return Line.startsWith(QStringLiteral("MimeType=")); });
  }
  return Result;
}

// A shortcut's old file (named after it alone, as older versions had it) gone.
void RemoveOldShortcut(const QString & Path)
{
  if (IsOldEntry(Path, nullptr, true))
  {
    QFile::remove(Path);
  }
}

enum class TMimeApps { Set, Remove, Rename };

// mimeapps.list's [Default Applications], each scheme's entry this desktop
// file (Set), or the entries naming it removed (Remove); the rest kept as it
// is. The old desktop file's entries renamed to this one first (Rename: only
// that, and only when there are some).
bool UpdateMimeApps(TMimeApps Change, QString & Error)
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
  const QStringList Original = Lines;
  const bool Set = (Change == TMimeApps::Set);
  QStringList Types;
  for (const QString & Scheme : Schemes())
  {
    Types.append(QStringLiteral("x-scheme-handler/") + Scheme);
  }
  const QString Desktop = DesktopFileName();
  const QString OldDesktop = QString::fromLatin1(OldDesktopFileName);
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
        const QStringList Before = Trimmed.mid(Equals + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        QStringList Handlers = Before;
        Handlers.replaceInStrings(QRegularExpression(QStringLiteral("^") + QRegularExpression::escape(OldDesktop) +
          QStringLiteral("$")), Desktop);
        Handlers.removeDuplicates();
        if (Change != TMimeApps::Rename)
        {
          Handlers.removeAll(Desktop);
        }
        if (Handlers.isEmpty() || (Set && (Section == QStringLiteral("[Default Applications]"))))
        {
          continue;  // dropped, or replaced below
        }
        Result.append((Handlers == Before) ? Line : (Key + QLatin1Char('=') + Handlers.join(QLatin1Char(';')) + QLatin1Char(';')));
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
  if ((Change == TMimeApps::Rename) && (Result == Original))
  {
    return true;  // nothing of the old name's
  }
  return WriteFile(Path, (Result.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), Error);
}

void UpdateDesktopDatabase()
{
  // Only the user's own directory, where linscp writes its entry.
  const QString Tool = QStandardPaths::findExecutable(QStringLiteral("update-desktop-database"));
  if (!Tool.isEmpty() && SelfRegisters())
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
  return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + DesktopFileName();
}

QString MimeAppsPath()
{
  // In a Flatpak, XDG_CONFIG_HOME is the sandbox's (~/.var/app/<id>/config),
  // which the desktop never reads: the host's (Flatpak gives its location,
  // else XDG's default).
  if (Install() == TInstall::Flatpak)
  {
    const QString Host = qEnvironmentVariable("HOST_XDG_CONFIG_HOME");
    return (Host.isEmpty() ? (QDir::homePath() + QStringLiteral("/.config")) : Host) + QStringLiteral("/mimeapps.list");
  }
  return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/mimeapps.list");
}

void SetUpDesktopIdentity()
{
  QGuiApplication::setDesktopFileName(QString::fromLatin1(AppId));
}

void SetProgramForTests(const QString & Program)
{
  TestProgram = Program;
}

TInstall Install()
{
  if (!qEnvironmentVariableIsEmpty("FLATPAK_ID"))
  {
    return TInstall::Flatpak;
  }
#if defined(Q_OS_WIN)
  return TInstall::Archive;
#else
  if (QFile::exists(QStringLiteral("/.flatpak-info")))
  {
    return TInstall::Flatpak;
  }
  // (An AppImage's own tree, mounted, has a package's layout.)
  if (!qEnvironmentVariableIsEmpty("APPIMAGE"))
  {
    return TInstall::AppImage;
  }
  const QFileInfo Program(TestProgram.isEmpty() ? QCoreApplication::applicationFilePath() : TestProgram);
  const QString Path = Program.absoluteFilePath();
  if (Path.startsWith(QStringLiteral("/usr/")) ||
      QFileInfo::exists(Program.dir().filePath(QStringLiteral("../../share/applications/") + DesktopFileName())))
  {
    return TInstall::Package;
  }
  return TInstall::Archive;
#endif
}

bool SelfRegisters()
{
  const TInstall Kind = Install();
  return (Kind == TInstall::AppImage) || (Kind == TInstall::Archive);
}

bool CleanUpOldRegistration()
{
#if defined(Q_OS_WIN)
  return false;
#else
  bool Changed = false;
  // The old menu entry, the URL handler registered again under the new name
  // when it was one and linscp registers itself (a package's and a
  // Flatpak's desktop file handles the URLs already).
  const QString OldEntry = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') +
    QString::fromLatin1(OldDesktopFileName);
  bool Handler = false;
  if (IsOldEntry(OldEntry, &Handler))
  {
    QFile::remove(OldEntry);
    Changed = true;
    QString Error;
    if (Handler && SelfRegisters() && !QFileInfo::exists(DesktopEntryPath()))
    {
      InstallIcon();
      WriteFile(DesktopEntryPath(), DesktopEntry(true).toUtf8(), Error);
    }
  }
  // The desktop's shortcuts linscp made: the new icon's name.
  const QString Desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  bool Shortcuts = false;
  if (!Desktop.isEmpty() && (QDir(Desktop) != QDir::home()))
  {
    QDirIterator Each(Desktop, { QStringLiteral("*.desktop") }, QDir::Files);
    while (Each.hasNext())
    {
      const QString Path = Each.next();
      if (IsOldEntry(Path))
      {
        QFile File(Path);
        if (File.open(QIODevice::ReadOnly | QIODevice::Text))
        {
          QStringList Lines = QString::fromUtf8(File.readAll()).split(QLatin1Char('\n'));
          File.close();
          Lines.replaceInStrings(QRegularExpression(QStringLiteral("^Icon=") + QString::fromLatin1(OldIconName) + QStringLiteral("$")),
            QStringLiteral("Icon=") + QString::fromLatin1(AppId));
          QString Error;
          // (In place: the file keeps its permissions, and the desktop its
          // trust in it.)
          WriteFile(Path, Lines.join(QLatin1Char('\n')).toUtf8(), Error);
          Shortcuts = true;
        }
      }
    }
  }
  const QString OldIcon = IconPath(QString::fromLatin1(OldIconName));
  if (QFile::exists(OldIcon))
  {
    QFile::remove(OldIcon);
    Changed = true;
  }
  if (Shortcuts)
  {
    InstallIcon();
    Changed = true;
  }
  // mimeapps.list's old entries: the new desktop file's.
  QFile MimeApps(MimeAppsPath());
  const QRegularExpression Old(QStringLiteral("[=;]") + QRegularExpression::escape(QString::fromLatin1(OldDesktopFileName)) +
    QStringLiteral("(;|$)"), QRegularExpression::MultilineOption);
  if (MimeApps.open(QIODevice::ReadOnly) && Old.match(QString::fromUtf8(MimeApps.readAll())).hasMatch())
  {
    MimeApps.close();
    QString Error;
    UpdateMimeApps(TMimeApps::Rename, Error);
    Changed = true;
  }
  if (Changed)
  {
    UpdateDesktopDatabase();
  }
  return Changed;
#endif
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
  // linscp's own desktop entry only where nothing installed one (a
  // package's and a Flatpak's declare the URL schemes already): then only the
  // default made, as asked.
  InstallIcon();
  if ((SelfRegisters() && !WriteFile(DesktopEntryPath(), DesktopEntry(true).toUtf8(), Error)) ||
      !UpdateMimeApps(TMimeApps::Set, Error))
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
  if (SelfRegisters())
  {
    QFile::remove(DesktopEntryPath());
  }
  if (!UpdateMimeApps(TMimeApps::Remove, Error))
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
  const QString Folder = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + QLatin1Char('/');
  const QString Path = Folder + DesktopFileName();
  RemoveOldShortcut(Folder + QString::fromLatin1(OldDesktopFileName));
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

#if defined(Q_OS_WIN)
namespace
{

// As CreateDesktopShortCut: the program, its arguments quoted as its
// command line has them, its own icon (which jump lists need set), and the
// name as the title (which they show). Null when it cannot be made.
IShellLinkW * MakeShellLink(const CoreBridge::TShortcut & Shortcut)
{
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
  const std::wstring Exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
  const std::wstring Arguments = Quoted.join(QLatin1Char(' ')).toStdWString();
  const std::wstring Description = QString::fromStdString(Shortcut.Description).toStdWString();
  const std::wstring Title = QString::fromStdString(Shortcut.Name).toStdWString();
  IShellLinkW * Link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&Link))))
  {
    return nullptr;
  }
  Link->SetPath(Exe.c_str());
  Link->SetArguments(Arguments.c_str());
  Link->SetDescription(Description.c_str());
  Link->SetShowCmd(SW_SHOW);
  Link->SetIconLocation(Exe.c_str(), 0);
  IPropertyStore * Store = nullptr;
  if (SUCCEEDED(Link->QueryInterface(IID_IPropertyStore, reinterpret_cast<void **>(&Store))))
  {
    PROPVARIANT Value;
    if (SUCCEEDED(InitPropVariantFromString(Title.c_str(), &Value)))
    {
      Store->SetValue(PKEY_Title, Value);
      Store->Commit();
      PropVariantClear(&Value);
    }
    Store->Release();
  }
  return Link;
}

}  // namespace
#endif

bool CreateShortcut(const CoreBridge::TShortcut & Shortcut, TShortcutFolder Folder, QString & Path, QString & Error)
{
  const QString ShortcutError = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::CreateShortcutError));
  const QString Directory = ShortcutFolder(Folder);
  QDir().mkpath(Directory);
  const QString Name = QString::fromStdString(Shortcut.Name);
#if defined(Q_OS_WIN)
  Path = QDir(Directory).filePath(Name + QStringLiteral(".lnk"));
  const std::wstring LinkPath = QDir::toNativeSeparators(Path).toStdWString();
  const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool Result = false;
  if (IShellLinkW * Link = MakeShellLink(Shortcut))
  {
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
  // <AppId>-<name>.desktop (its Name= what the desktop shows), the name's
  // own file that older versions made gone.
  const QString FileName = QString(Name).replace(QLatin1Char('/'), QLatin1Char('_'));
  RemoveOldShortcut(QDir(Directory).filePath(FileName + QStringLiteral(".desktop")));
  Path = QDir(Directory).filePath(QString::fromLatin1(AppId) + QLatin1Char('-') + FileName + QStringLiteral(".desktop"));
  const QString Entry = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=%1\n"
    "Comment=%2\n"
    "Exec=%3 %4\n"
    "Icon=%5\n"
    "Terminal=false\n").arg(Name, QString(QString::fromStdString(Shortcut.Description)).replace(QLatin1Char('\n'), QLatin1Char(' ')),
      ProgramCommand(), Exec.join(QLatin1Char(' ')), QString::fromLatin1(AppId));
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

QString FileTypeName(const QString & Name, bool Directory)
{
  static QHash<QString, QString> Cache;
  const int Dot = Name.lastIndexOf(QLatin1Char('.'));
  const QString Key = Directory ? QStringLiteral("/") : ((Dot > 0) ? Name.mid(Dot).toLower() : QString());
  const auto Found = Cache.constFind(Key);
  if (Found != Cache.constEnd())
  {
    return Found.value();
  }
  QString Result;
#if defined(Q_OS_WIN)
  SHFILEINFOW Info = {};
  const std::wstring Wide = (Directory ? QStringLiteral("folder") : Name).toStdWString();
  if (SHGetFileInfoW(Wide.c_str(), Directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL, &Info, sizeof(Info),
        SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES) != 0)
  {
    Result = QString::fromWCharArray(Info.szTypeName);
  }
#else
  const QMimeDatabase Database;
  const QMimeType Type = Directory ? Database.mimeTypeForName(QStringLiteral("inode/directory")) :
    Database.mimeTypeForFile(Name, QMimeDatabase::MatchExtension);
  Result = Type.comment();
#endif
  Cache.insert(Key, Result);
  return Result;
}

#if defined(Q_OS_WIN)
namespace
{

bool AcquireShutDownPrivileges(QString & Error)
{
  HANDLE Token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token))
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported));
    return false;
  }
  TOKEN_PRIVILEGES Privileges = {};
  LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &Privileges.Privileges[0].Luid);
  Privileges.PrivilegeCount = 1;
  Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  const bool Result = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, nullptr, nullptr) && (GetLastError() == ERROR_SUCCESS);
  CloseHandle(Token);
  if (!Result)
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported));
  }
  return Result;
}

}  // namespace
#else
namespace
{

bool CallLogind(const QString & Method, QString & Error)
{
  QDBusMessage Call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
    QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), Method);
  Call << true;
  const QDBusMessage Reply = QDBusConnection::systemBus().call(Call, QDBus::Block, 30000);
  if (Reply.type() == QDBusMessage::ErrorMessage)
  {
    Error = Reply.errorMessage();
    return false;
  }
  return true;
}

}  // namespace
#endif

bool SuspendComputer(QString & Error)
{
#if defined(Q_OS_WIN)
  if (!AcquireShutDownPrivileges(Error))
  {
    return false;
  }
  // powrprof's, loaded where it is called, as nothing else needs it.
  using TSetSuspendState = BOOLEAN (WINAPI *)(BOOLEAN, BOOLEAN, BOOLEAN);
  HMODULE PowrProf = LoadLibraryW(L"powrprof.dll");
  TSetSuspendState SetSuspendState = (PowrProf != nullptr) ?
    reinterpret_cast<TSetSuspendState>(reinterpret_cast<void *>(GetProcAddress(PowrProf, "SetSuspendState"))) : nullptr;
  const bool Result = (SetSuspendState != nullptr) && SetSuspendState(FALSE, FALSE, FALSE);
  if (!Result)
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported));
  }
  return Result;
#else
  return CallLogind(QStringLiteral("Suspend"), Error);
#endif
}

bool ShutDownComputer(QString & Error)
{
#if defined(Q_OS_WIN)
  if (!AcquireShutDownPrivileges(Error))
  {
    return false;
  }
  if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_POWEROFF,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED))
  {
    Error = QString::fromStdString(CoreBridge::Text(CoreBridge::TText::NotSupported));
    return false;
  }
  return true;
#else
  return CallLogind(QStringLiteral("PowerOff"), Error);
#endif
}

bool PowerActionsAvailable()
{
  return (Install() != TInstall::Flatpak);
}

// As TWinConfiguration::UpdateEntryInJumpList and Setup.cpp's UpdateJumpList:
// the recent workspaces and sites as the taskbar's categories, each a link
// opening it (/JumpList; a site's /UploadIfAny takes files dropped on it),
// those the user removed from the list dropped from it.
void UpdateJumpList(CoreBridge::TJumpListEntry Kind, const std::string & Name, bool Add)
{
#if defined(Q_OS_WIN)
  try
  {
    CoreBridge::UpdateJumpList(Kind, Name, Add, [](CoreBridge::TJumpList & Lists)
    {
      const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
      ICustomDestinationList * List = nullptr;
      if (SUCCEEDED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_ICustomDestinationList,
            reinterpret_cast<void **>(&List))))
      {
        UINT MinSlots = 0;
        IObjectArray * RemovedArray = nullptr;
        if (SUCCEEDED(List->BeginList(&MinSlots, IID_IObjectArray, reinterpret_cast<void **>(&RemovedArray))))
        {
          QStringList Removed;
          UINT Count = 0;
          if (SUCCEEDED(RemovedArray->GetCount(&Count)))
          {
            for (UINT Index = 0; Index < Count; Index++)
            {
              IShellLinkW * Link = nullptr;
              wchar_t Description[2048];
              if (SUCCEEDED(RemovedArray->GetAt(Index, IID_IShellLinkW, reinterpret_cast<void **>(&Link))))
              {
                if (SUCCEEDED(Link->GetDescription(Description, 2047)))
                {
                  Removed.append(QString::fromWCharArray(Description));
                }
                Link->Release();
              }
            }
          }
          RemovedArray->Release();
          struct TCategory
          {
            std::vector<std::string> * Names;
            CoreBridge::TSessionShortcutKind Kind;
            std::vector<std::string> Arguments;
            CoreBridge::TText Title;
          };
          for (const TCategory & Category : std::initializer_list<TCategory>{
                 { &Lists.Workspaces, CoreBridge::TSessionShortcutKind::Workspace, { "/JumpList" }, CoreBridge::TText::JumpListWorkspaces },
                 { &Lists.Sites, CoreBridge::TSessionShortcutKind::Site, { "/UploadIfAny", "/JumpList" }, CoreBridge::TText::JumpListRecent } })
          {
            IObjectCollection * Collection = nullptr;
            if (FAILED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                  reinterpret_cast<void **>(&Collection))))
            {
              continue;
            }
            int Added = 0;
            for (auto Each = Category.Names->begin(); Each != Category.Names->end();)
            {
              IShellLinkW * Link = nullptr;
              try
              {
                Link = MakeShellLink(CoreBridge::SessionShortcut(*Each, Category.Kind, std::string(), Category.Arguments));
              }
              catch (const CoreBridge::TCoreError &)
              {
                Link = nullptr;
              }
              wchar_t Description[2048];
              if ((Link != nullptr) && SUCCEEDED(Link->GetDescription(Description, 2047)) &&
                  Removed.contains(QString::fromWCharArray(Description)))
              {
                Each = Category.Names->erase(Each);
              }
              else
              {
                if (Link != nullptr)
                {
                  Collection->AddObject(Link);
                  Added++;
                }
                ++Each;
              }
              if (Link != nullptr)
              {
                Link->Release();
              }
            }
            IObjectArray * Array = nullptr;
            if ((Added > 0) && SUCCEEDED(Collection->QueryInterface(IID_IObjectArray, reinterpret_cast<void **>(&Array))))
            {
              List->AppendCategory(QString::fromStdString(CoreBridge::Text(Category.Title)).toStdWString().c_str(), Array);
              Array->Release();
            }
            Collection->Release();
          }
          List->CommitList();
        }
        List->Release();
      }
      if (SUCCEEDED(Init))
      {
        CoUninitialize();
      }
    });
  }
  catch (const CoreBridge::TCoreError &)
  {
    // As SetupInitialize: not worth more than the log.
  }
#else
  Q_UNUSED(Kind);
  Q_UNUSED(Name);
  Q_UNUSED(Add);
#endif
}

bool HasSystemContextMenu()
{
#if defined(Q_OS_WIN)
  return true;
#else
  return false;
#endif
}

#if defined(Q_OS_WIN)
namespace
{

// MenuCallbackProc: the menu's owner draws and fills the shell's owner-drawn
// submenus (Open With, Send To) through IContextMenu2.
LRESULT CALLBACK SystemMenuCallback(HWND Wnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
  switch (Msg)
  {
    case WM_CREATE:
      SetWindowLongPtrW(Wnd, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(LParam)->lpCreateParams));
      break;
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_INITMENUPOPUP:
    {
      IContextMenu2 * Menu2 = reinterpret_cast<IContextMenu2 *>(GetWindowLongPtrW(Wnd, GWLP_USERDATA));
      if (Menu2 != nullptr)
      {
        Menu2->HandleMenuMsg(Msg, WParam, LParam);
      }
      return (Msg == WM_INITMENUPOPUP) ? 0 : 1;
    }
  }
  return DefWindowProcW(Wnd, Msg, WParam, LParam);
}

}  // namespace
#endif

bool ShowSystemContextMenu(QWidget * Window, const QString & Directory, const QStringList & Names, const QPoint & Global,
  QString & Verb, QStringList * Items)
{
  Verb.clear();
#if defined(Q_OS_WIN)
  if (Names.isEmpty())
  {
    return false;
  }
  const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool Result = false;
  // The files' parent folder, and each one's PIDL in it.
  PIDLIST_ABSOLUTE FolderPidl = nullptr;
  IShellFolder * Desktop = nullptr;
  IShellFolder * Folder = nullptr;
  std::vector<PITEMID_CHILD> Children;
  const std::wstring FolderPath = QDir::toNativeSeparators(Directory).toStdWString();
  if (SUCCEEDED(SHParseDisplayName(FolderPath.c_str(), nullptr, &FolderPidl, 0, nullptr)) &&
      SUCCEEDED(SHGetDesktopFolder(&Desktop)) &&
      SUCCEEDED(Desktop->BindToObject(FolderPidl, nullptr, IID_IShellFolder, reinterpret_cast<void **>(&Folder))))
  {
    for (const QString & Name : Names)
    {
      std::wstring Child = Name.toStdWString();
      PIDLIST_RELATIVE Pidl = nullptr;
      if (SUCCEEDED(Folder->ParseDisplayName(nullptr, nullptr, &Child[0], nullptr, &Pidl, nullptr)))
      {
        Children.push_back(static_cast<PITEMID_CHILD>(Pidl));
      }
    }
  }
  HWND Owner = (Window != nullptr) ? reinterpret_cast<HWND>(Window->window()->winId()) : nullptr;
  IContextMenu * Menu = nullptr;
  if (!Children.empty() && (Children.size() == static_cast<std::size_t>(Names.size())) &&
      SUCCEEDED(Folder->GetUIObjectOf(Owner, static_cast<UINT>(Children.size()), const_cast<PCUITEMID_CHILD_ARRAY>(Children.data()),
        IID_IContextMenu, nullptr, reinterpret_cast<void **>(&Menu))))
  {
    const UINT MaxEntries = 1000;
    HMENU Popup = CreatePopupMenu();
    IContextMenu2 * Menu2 = nullptr;
    HWND Callback = nullptr;
    const UINT Flags = CMF_EXPLORE | CMF_CANRENAME | (((GetKeyState(VK_SHIFT) & 0x8000) != 0) ? CMF_EXTENDEDVERBS : 0);
    if (SUCCEEDED(Menu->QueryInterface(IID_IContextMenu2, reinterpret_cast<void **>(&Menu2))))
    {
      Menu2->QueryContextMenu(Popup, 0, 1, MaxEntries, Flags);
      static const wchar_t ClassName[] = L"linscpSystemMenuCallback";
      WNDCLASSW Class = {};
      Class.lpfnWndProc = SystemMenuCallback;
      Class.hInstance = GetModuleHandleW(nullptr);
      Class.lpszClassName = ClassName;
      RegisterClassW(&Class);
      Callback = CreateWindowW(ClassName, L"", WS_POPUPWINDOW, 0, 0, 0, 0, nullptr, nullptr, Class.hInstance, Menu2);
    }
    else
    {
      Menu->QueryContextMenu(Popup, 0, 1, 0x7FFF, Flags);
    }
    int Command = 0;
    if (Items != nullptr)
    {
      for (int Index = 0; Index < GetMenuItemCount(Popup); Index++)
      {
        wchar_t Text[256] = {};
        if (GetMenuStringW(Popup, static_cast<UINT>(Index), Text, 255, MF_BYPOSITION) > 0)
        {
          Items->append(QString::fromWCharArray(Text));
        }
      }
    }
    else
    {
      // (The point in device pixels, as the shell's.)
      const qreal Scale = (Window != nullptr) ? Window->devicePixelRatioF() : 1.0;
      Command = static_cast<int>(TrackPopupMenuEx(Popup, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        qRound(Global.x() * Scale), qRound(Global.y() * Scale), (Callback != nullptr) ? Callback : Owner, nullptr));
    }
    Result = true;
    if (Command > 0)
    {
      char AnsiVerb[256] = {};
      if (SUCCEEDED(Menu->GetCommandString(static_cast<UINT_PTR>(Command - 1), GCS_VERBA, nullptr, AnsiVerb, sizeof(AnsiVerb) - 1)))
      {
        Verb = QString::fromLatin1(AnsiVerb).toLower();
      }
      if (Verb != QStringLiteral("rename"))
      {
        CMINVOKECOMMANDINFO Info = {};
        Info.cbSize = sizeof(Info);
        Info.hwnd = Owner;
        Info.lpVerb = MAKEINTRESOURCEA(Command - 1);
        Info.nShow = SW_SHOWNORMAL;
        Menu->InvokeCommand(&Info);
      }
    }
    if (Callback != nullptr)
    {
      DestroyWindow(Callback);
    }
    if (Menu2 != nullptr)
    {
      Menu2->Release();
    }
    DestroyMenu(Popup);
    Menu->Release();
  }
  for (PITEMID_CHILD Child : Children)
  {
    CoTaskMemFree(Child);
  }
  if (Folder != nullptr)
  {
    Folder->Release();
  }
  if (Desktop != nullptr)
  {
    Desktop->Release();
  }
  CoTaskMemFree(FolderPidl);
  if (SUCCEEDED(Init))
  {
    CoUninitialize();
  }
  return Result;
#else
  Q_UNUSED(Window);
  Q_UNUSED(Directory);
  Q_UNUSED(Names);
  Q_UNUSED(Global);
  Q_UNUSED(Items);
  return false;
#endif
}

bool UpdateChecksAllowed()
{
#if LINSCP_UPDATE_CHECK
  return (Install() != TInstall::Flatpak);
#else
  return false;
#endif
}

}  // namespace Integration
