#include "AboutDialog.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "LicenseDialog.h"
#include "IconSet.h"

namespace
{

// The bundled icon sets (P3-17), each its license shown by the link.
QString IconSetsHtml()
{
  QString Result;
  for (const IconSet::TSet & Set : IconSet::Sets())
  {
    if (!Set.License.isEmpty())
    {
      Result += QStringLiteral("<p>%1<br><a href=\"iconset:%2\">%3</a></p>").arg(
        AboutDialog::tr("%1 icons").arg(Set.Name), Set.Id, AboutDialog::tr("License"));
    }
  }
  return Result;
}

const char WebSite[] = "https://linscp.org/";
const char WinScpWebSite[] = "https://winscp.net/";

QString Link(const QString & Url)
{
  return QStringLiteral("<a href=\"%1\">%1</a>").arg(Url);
}

// Qt's licence, as LGPL-3.0 4(c) asks of a work showing Qt's copyright: the
// copy Windows packages ship beside the program (licenses/third-party/qt6-base,
// with the GPL it builds on), else GNU's.
QUrl QtLicenseUrl()
{
  const QFileInfo Local(QDir(QCoreApplication::applicationDirPath()).filePath(
    QStringLiteral("licenses/third-party/qt6-base/LGPL-3.0-only.txt")));
  return Local.isFile() ? QUrl::fromLocalFile(Local.absoluteFilePath()) :
    QUrl(QStringLiteral("https://www.gnu.org/licenses/lgpl-3.0.html"));
}

QLabel * TextLabel(const QString & Text, QWidget * Parent)
{
  QLabel * Label = new QLabel(Text, Parent);
  Label->setWordWrap(true);
  Label->setTextFormat(Qt::RichText);
  Label->setOpenExternalLinks(true);
  Label->setTextInteractionFlags(Qt::TextBrowserInteraction);
  return Label;
}

}  // namespace

AboutDialog::AboutDialog(const CoreBridge::TInfo & Info, QWidget * Parent) :
  QDialog(Parent)
{
  const QString AppName = QString::fromStdString(Info.AppName);
  const QString Version = QString::fromStdString(Info.Version);
  setWindowTitle(tr("About %1").arg(AppName));

  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Body = new QHBoxLayout();
  Body->setSpacing(24);
  // WinSCP's icon with an "L" on the padlock (port/tools/make_icon.py).
  QLabel * Icon = new QLabel(this);
  Icon->setPixmap(QIcon(QStringLiteral(":/icons/linscp.ico")).pixmap(64, 64));
  Icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  Body->addWidget(Icon, 0, Qt::AlignTop);

  QVBoxLayout * Texts = new QVBoxLayout();
  Texts->setSpacing(12);
  FName = new QLabel(AppName, this);
  QFont NameFont = FName->font();
  NameFont.setPointSizeF(NameFont.pointSizeF() * 2.4);
  NameFont.setBold(true);
  FName->setFont(NameFont);
  Texts->addWidget(FName);
  QLabel * Description = TextLabel(tr("Transfer files to and from servers, on Linux and Windows"), this);
  QFont DescriptionFont = Description->font();
  DescriptionFont.setPointSizeF(DescriptionFont.pointSizeF() * 1.2);
  Description->setFont(DescriptionFont);
  Texts->addWidget(Description);
  FVersion = TextLabel(tr("Version: <b>%1</b>").arg(Version.toHtmlEscaped()), this);
  Texts->addWidget(FVersion);
  Texts->addWidget(TextLabel(tr("Using: Qt %1 (headers: %2)").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)), this));
  Texts->addWidget(TextLabel(tr("Website: %1").arg(Link(QString::fromLatin1(WebSite))), this));
  QLabel * Copyright = TextLabel(
    tr("Copyright © 2026 Eduardo Correia &lt;%1&gt;.").arg(QStringLiteral("ecorreia@apliant.com.br")), this);
  Texts->addWidget(Copyright);
  QLabel * Licence = TextLabel(
    tr("This program is free software; you can redistribute it and/or modify it under the terms of the "
       "GNU General Public License version 3, as WinSCP."), this);
  QFont SmallFont = Licence->font();
  SmallFont.setPointSizeF(SmallFont.pointSizeF() * 0.9);
  Licence->setFont(SmallFont);
  Copyright->setFont(SmallFont);
  Texts->addWidget(Licence);
  Body->addLayout(Texts, 1);
  Layout->addLayout(Body);

  // WinSCP's third-party list, core's (CoreBridge::ThirdPartyHtml), then Qt,
  // which the shell is built on, then WinSCP itself, after it.
  FThirdParty = new QTextBrowser(this);
  FThirdParty->setAccessibleName(tr("Third party software"));
  FThirdParty->setOpenLinks(false);
  FThirdParty->setHtml(QString::fromStdString(CoreBridge::ThirdPartyHtml(QLocale().nativeLanguageName().toStdString())) +
    QStringLiteral("<p>%1<br>%2<br><a href=\"%3\">%4</a><br>%5</p>").arg(
      tr("Qt %1").arg(QString::fromLatin1(qVersion())),
      tr("Copyright © The Qt Company Ltd. and other contributors"),
      QtLicenseUrl().toString(QUrl::FullyEncoded).toHtmlEscaped(),
      tr("Licensed under the GNU LGPL version 3").toHtmlEscaped(),
      Link(QStringLiteral("https://www.qt.io/"))) +
    QStringLiteral("<p>%1<br>%2<br>%3</p>").arg(
      tr("WinSCP %1").arg(QString::fromStdString(Info.WinScpVersion).toHtmlEscaped()),
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::WinScpCopyright)).toHtmlEscaped(),
      Link(QString::fromLatin1(WinScpWebSite))) + IconSetsHtml());
  FThirdParty->setMinimumHeight(160);
  connect(FThirdParty, &QTextBrowser::anchorClicked, this, [this](const QUrl & Url)
  {
    // As TAboutDialog::ExpatLicenceHandler.
    if (Url.toString() == QString::fromLatin1(CoreBridge::ExpatLicenseUrl))
    {
      LicenseDialog Dialog(LicenseDialog::Expat, this);
      Dialog.exec();
    }
    else if (Url.scheme() == QStringLiteral("iconset"))
    {
      for (const IconSet::TSet & Set : IconSet::Sets())
      {
        if ((Set.Id == Url.path()) && !Set.License.isEmpty())
        {
          LicenseDialog Dialog(Set.Name, Set.License, this);
          Dialog.exec();
        }
      }
    }
    else
    {
      QDesktopServices::openUrl(Url);
    }
  });
  Layout->addSpacing(12);
  Layout->addWidget(FThirdParty, 1);

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  Buttons->button(QDialogButtonBox::Ok)->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  FLicense = Buttons->addButton(tr("&License..."), QDialogButtonBox::ActionRole);
  connect(FLicense, &QPushButton::clicked, this, [this]()
  {
    LicenseDialog Dialog(LicenseDialog::Product, this);
    Dialog.exec();
  });
  Layout->addSpacing(12);
  Layout->addWidget(Buttons);
  Layout->setContentsMargins(24, 24, 24, 16);
  // As tall as its wrapped texts are at its width, with no gaps between,
  // and the third-party list's minimum.
  const int Width = 600;
  resize(Width, Layout->totalHeightForWidth(Width) + 120);
}
