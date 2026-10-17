#include "GenerateUrlDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "QtFrontend.h"
#include "RtfText.h"

GenerateUrlDialog::GenerateUrlDialog(CoreBridge::TSession * Session, const std::vector<std::string> & Paths, QWidget * Parent,
  const CoreBridge::TSession::TTransferCode * Transfer, const CoreBridge::TSite * Site) :
  QDialog(Parent),
  FSession(Session),
  FPaths(Paths)
{
  if (Site != nullptr)
  {
    FHasSite = true;
    FSite = *Site;
    FPaths.clear();
  }
  if (Transfer != nullptr)
  {
    FHasTransfer = true;
    FTransfer = *Transfer;
    FPaths.clear();
  }
  const CoreBridge::TGenerateCodeOptions Settings = CoreBridge::GenerateCodeSettings();
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FPages = new QTabWidget(this);
  QWidget * Url = new QWidget(FPages);
  QGridLayout * UrlLayout = new QGridLayout(Url);
  // WinSCP's checkboxes, in its order, with its tags.
  const std::pair<int, QString> Checks[] = {
    { 1, tr("&Username") }, { 2, tr("&Password") }, { 4, tr("SSH &host key") }, { 8, tr("Initial &directory") },
    { 16, tr("&WinSCP-specific") }, { 32, tr("&Save extension") }, { 64, tr("&Advanced settings") } };
  int Row = 0;
  for (const auto & Check : Checks)
  {
    QCheckBox * Box = new QCheckBox(Check.second, Url);
    Box->setChecked((Settings.Components & Check.first) != 0);
    UrlLayout->addWidget(Box, Row % 4, Row / 4);
    connect(Box, &QCheckBox::toggled, this, &GenerateUrlDialog::Generate);
    FChecks.emplace_back(Check.first, Box);
    Row++;
  }
  UrlLayout->setRowStretch(4, 1);
  FPages->addTab(Url, tr("URL"));
  if (Paths.empty())
  {
    QWidget * Script = new QWidget(FPages);
    QFormLayout * ScriptLayout = new QFormLayout(Script);
    FScriptFormat = new QComboBox(Script);
    FScriptFormat->addItems({ tr("Script file"), tr("Batch file"), tr("Command-line"), tr("PowerShell script") });
    FScriptFormat->setCurrentIndex(qBound(0, Settings.ScriptFormat, 3));
    ScriptLayout->addRow(tr("&Format:"), FScriptFormat);
    FScriptDescription = new QLabel(Script);
    FScriptDescription->setWordWrap(true);
    ScriptLayout->addRow(FScriptDescription);
    FPages->addTab(Script, tr("Script"));
    QWidget * Assembly = new QWidget(FPages);
    QFormLayout * AssemblyLayout = new QFormLayout(Assembly);
    FLanguage = new QComboBox(Assembly);
    FLanguage->addItems({ tr("C#"), tr("VB.NET"), tr("PowerShell") });
    FLanguage->setCurrentIndex(qBound(0, Settings.AssemblyLanguage, 2));
    AssemblyLayout->addRow(tr("&Language:"), FLanguage);
    FAssemblyDescription = new QLabel(Assembly);
    FAssemblyDescription->setWordWrap(true);
    AssemblyLayout->addRow(FAssemblyDescription);
    FPages->addTab(Assembly, tr(".NET assembly code"));
    connect(FScriptFormat, &QComboBox::currentIndexChanged, this, &GenerateUrlDialog::Generate);
    connect(FLanguage, &QComboBox::currentIndexChanged, this, &GenerateUrlDialog::Generate);
    FPages->setCurrentIndex(static_cast<int>(Settings.Target));
    // As UrlSheet->TabVisible: a transfer has no URL.
    if (FHasTransfer)
    {
      FPages->setTabVisible(0, false);
      if (FPages->currentIndex() == 0)
      {
        FPages->setCurrentIndex(1);
      }
    }
  }
  connect(FPages, &QTabWidget::currentChanged, this, &GenerateUrlDialog::Generate);
  FPages->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  Layout->addWidget(FPages);
  FResultGroup = new QGroupBox(this);
  QVBoxLayout * ResultLayout = new QVBoxLayout(FResultGroup);
  FResult = new QTextBrowser(FResultGroup);
  FResult->setOpenExternalLinks(true);
  // WinSCP's colours are for a light background, whatever the theme.
  QPalette Palette = FResult->palette();
  Palette.setColor(QPalette::Base, Qt::white);
  Palette.setColor(QPalette::Text, Qt::black);
  FResult->setPalette(Palette);
  ResultLayout->addWidget(FResult);
  Layout->addWidget(FResultGroup, 1);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(Buttons->addButton(tr("&Copy to Clipboard"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this]()
  {
    QApplication::clipboard()->setText(Text());
  });
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  Generate();
  resize(720, (FPages->currentIndex() == 0) ? 420 : 600);
}

QCheckBox * GenerateUrlDialog::Check(int Tag) const
{
  for (const auto & Check : FChecks)
  {
    if (Check.first == Tag)
    {
      return Check.second;
    }
  }
  return nullptr;
}

CoreBridge::TGenerateCodeOptions GenerateUrlDialog::Options() const
{
  CoreBridge::TGenerateCodeOptions Result;
  Result.Components = 0;
  for (const auto & Check : FChecks)
  {
    if (Check.second->isChecked())
    {
      Result.Components |= Check.first;
    }
  }
  Result.Target = static_cast<CoreBridge::TGenerateCodeOptions::TTarget>(FPages->currentIndex());
  Result.ScriptFormat = (FScriptFormat != nullptr) ? FScriptFormat->currentIndex() : 0;
  Result.AssemblyLanguage = (FLanguage != nullptr) ? FLanguage->currentIndex() : 0;
  Result.Paths = FPaths;
  return Result;
}

// As TGenerateUrlDialog::UpdateControls.
void GenerateUrlDialog::Generate()
{
  CoreBridge::TGeneratedCode Code;
  try
  {
    Code = FHasSite ? CoreBridge::GenerateSiteCode(FSite, Options()) :
      FHasTransfer ? FSession->GenerateTransferCode(Options(), FTransfer) : FSession->GenerateCode(Options());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
    return;
  }
  setWindowTitle(QString::fromStdString(Code.Title));
  FResultGroup->setTitle(FPages->currentIndex() == 1 ? FScriptFormat->currentText() : QString::fromStdString(Code.ResultCaption));
  if ((FPages->currentIndex() == 1) && (FScriptFormat->currentIndex() == 2))
  {
    FResultGroup->setTitle(QString::fromStdString(Code.ResultCaption));
  }
  for (const auto & Check : FChecks)
  {
    const bool Enabled =
      (Check.first == 1) ? Code.CanUserName : (Check.first == 2) ? Code.CanPassword : (Check.first == 4) ? Code.CanHostKey :
      (Check.first == 8) ? Code.CanRemoteDirectory : (Check.first == 32) ? Code.CanSaveExtension :
      (Check.first == 64) ? Code.CanRawSettings : true;
    Check.second->setEnabled(Enabled);
  }
  const QString Description = QString::fromStdString(Code.Description).trimmed();
  if (FScriptDescription != nullptr)
  {
    FScriptDescription->setText(Description);
    FAssemblyDescription->setText(Description);
  }
  FRtf = QString::fromStdString(Code.Rtf);
  QFont Font = Code.FixedWidth ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : font();
  FResult->setFont(Font);
  FResult->setLineWrapMode(Code.WordWrap ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
  FResult->setHtml(QStringLiteral("<div style=\"white-space:pre-wrap\">%1</div>").arg(RtfToHtml(FRtf)));
}

// As ClipboardButtonClick: the text, a trailing line end when several.
QString GenerateUrlDialog::Text() const
{
  QString Result = RtfToText(FRtf);
  while (Result.endsWith(QLatin1Char('\n')) && Result.count(QLatin1Char('\n')) == 1)
  {
    Result.chop(1);
  }
  return Result;
}

void GenerateUrlDialog::done(int Result)
{
  if (FPaths.empty())
  {
    try
    {
      CoreBridge::TGenerateCodeOptions Options = this->Options();
      // As upstream: the components are kept when the URL was last shown.
      if (Options.Target != CoreBridge::TGenerateCodeOptions::TTarget::Url)
      {
        Options.Components = CoreBridge::GenerateCodeSettings().Components;
      }
      CoreBridge::SetGenerateCodeSettings(Options);
    }
    catch (const CoreBridge::TCoreError &)
    {
      // Not kept: nothing else lost.
    }
  }
  QDialog::done(Result);
}
