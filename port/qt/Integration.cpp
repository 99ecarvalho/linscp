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

#include "CoreBridge.h"

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

// The desktop entry, as upstream's registration has the command: the
// program with /Unsafe and the URL (RegisterAsUrlHandler).
QString DesktopEntry(bool Handler)
{
  QStringList Types;
  for (const QString & Scheme : Schemes())
  {
    Types.append(QStringLiteral("x-scheme-handler/") + Scheme);
  }
  const QString Exe = QCoreApplication::applicationFilePath();
  QString Result = QStringLiteral(
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=%1\n"
    "Comment=%2\n"
    "Exec=\"%3\"%4\n"
    "Icon=linscp\n"
    "Terminal=false\n"
    "Categories=Network;FileTransfer;\n").arg(AppName(), QCoreApplication::translate("Integration", "SFTP, SCP, FTP, WebDAV and S3 client"),
      Exe, Handler ? QStringLiteral(" /Unsafe %u") : QString());
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

}  // namespace Integration
