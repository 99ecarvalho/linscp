#include "Language.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <cstring>

#include "CoreBridge.h"

QString RequestedLocale(int argc, char ** argv)
{
  for (int Index = 1; Index < argc; Index++)
  {
    if (std::strncmp(argv[Index], "--lang=", 7) == 0)
    {
      return QString::fromLocal8Bit(argv[Index] + 7);
    }
  }
  return QLocale::system().name();
}

TLanguage SetUpLanguage(const QString & Locale)
{
  TLanguage Result;
  Result.Locale = Locale;
  const QLocale QLocaleValue(Locale);
  QLocale::setDefault(QLocaleValue);

  Result.CoreCode = QString::fromStdString(CoreBridge::SetLanguage(Locale.toStdString()));

  // Qt's own texts: from Qt's installation (Linux), else beside the
  // executable (a Windows deploy folder).
  static QTranslator QtTranslator;
  const QString AppTranslations = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"));
  Result.Qt =
    QtTranslator.load(QLocaleValue, QStringLiteral("qtbase"), QStringLiteral("_"),
      QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
    QtTranslator.load(QLocaleValue, QStringLiteral("qtbase"), QStringLiteral("_"), AppTranslations);
  if (Result.Qt)
  {
    QCoreApplication::installTranslator(&QtTranslator);
  }

  // The shell's, compiled into it; pt_BR falls back to pt, and so on.
  static QTranslator ShellTranslator;
  Result.Shell = ShellTranslator.load(QLocaleValue, QStringLiteral("linscp"), QStringLiteral("_"), QStringLiteral(":/i18n"));
  if (Result.Shell)
  {
    QCoreApplication::installTranslator(&ShellTranslator);
  }
  return Result;
}
