#include "AboutDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace
{

const char WebSite[] = "https://github.com/99ecarvalho/linscp";
const char WinScpWebSite[] = "https://winscp.net/";

QString Link(const QString & Url)
{
  return QStringLiteral("<a href=\"%1\">%1</a>").arg(Url);
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
  // WinSCP's own icon, the program's until it has one.
  QLabel * Icon = new QLabel(this);
  Icon->setPixmap(QIcon(QStringLiteral(":/icons/Application.ico")).pixmap(64, 64));
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
  QLabel * Description = TextLabel(tr("A Qt port of WinSCP for Linux and Windows"), this);
  QFont DescriptionFont = Description->font();
  DescriptionFont.setPointSizeF(DescriptionFont.pointSizeF() * 1.2);
  Description->setFont(DescriptionFont);
  Texts->addWidget(Description);
  FVersion = TextLabel(tr("Version: <b>%1</b>").arg(Version.toHtmlEscaped()), this);
  Texts->addWidget(FVersion);
  Texts->addWidget(TextLabel(tr("Using: Qt %1 (headers: %2)").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)), this));
  // Where it comes from: WinSCP, the version its core is.
  FWinScp = TextLabel(tr("Based on WinSCP %1: %2").arg(Version.toHtmlEscaped(), Link(QString::fromLatin1(WinScpWebSite))), this);
  Texts->addWidget(FWinScp);
  Texts->addWidget(TextLabel(tr("Website: %1").arg(Link(QString::fromLatin1(WebSite))), this));
  Texts->addWidget(TextLabel(tr("Configuration: %1").arg(QString::fromStdString(Info.ConfigurationStorage).toHtmlEscaped()), this));
  QLabel * Copyright = TextLabel(
    tr("Copyright (C) 2026 Eduardo Correia &lt;%1&gt;.").arg(QStringLiteral("ecorreia@apliant.com.br")) + QStringLiteral("<br>") +
    tr("WinSCP: %1.").arg(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::WinScpCopyright)).toHtmlEscaped()), this);
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

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
  Buttons->button(QDialogButtonBox::Ok)->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  Layout->addSpacing(12);
  Layout->addWidget(Buttons);
  Layout->setContentsMargins(24, 24, 24, 16);
  // As tall as its wrapped texts are at its width, with no gaps between.
  const int Width = 600;
  resize(Width, Layout->totalHeightForWidth(Width));
}
