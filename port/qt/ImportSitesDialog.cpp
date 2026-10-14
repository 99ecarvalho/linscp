#include "ImportSitesDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "QtFrontend.h"

using TImportSites = CoreBridge::TImportSites;

ImportSitesDialog::ImportSitesDialog(QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ImportCaption)));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Top = new QHBoxLayout();
  QLabel * Label = new QLabel(tr("&Import from:"), this);
  FSource = new QComboBox(this);
  Label->setBuddy(FSource);
  Top->addWidget(Label);
  Top->addWidget(FSource, 1);
  FBrowse = new QPushButton(tr("B&rowse..."), this);
  FPaste = new QPushButton(tr("&Paste"), this);
  Top->addWidget(FBrowse);
  Top->addWidget(FPaste);
  Layout->addLayout(Top);
  FList = new QListWidget(this);
  FList->setAccessibleName(tr("Sites"));
  Layout->addWidget(FList, 1);
  FError = new QLabel(this);
  FError->setWordWrap(true);
  Layout->addWidget(FError);
  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  FCheckAll = Buttons->addButton(tr("Un/check &all"), QDialogButtonBox::ResetRole);
  connect(FCheckAll, &QPushButton::clicked, this, [this]()
  {
    // As ListViewCheckAll's toggle: all checked, unless they all are.
    bool All = true;
    for (int Row = 0; Row < FList->count(); Row++)
    {
      All = All && (FList->item(Row)->checkState() == Qt::Checked);
    }
    for (int Row = 0; Row < FList->count(); Row++)
    {
      FList->item(Row)->setCheckState(All ? Qt::Unchecked : Qt::Checked);
    }
    UpdateControls();
  });
  connect(Buttons, &QDialogButtonBox::accepted, this, &ImportSitesDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  connect(FBrowse, &QPushButton::clicked, this, [this]()
  {
    const QString FileName = QFileDialog::getOpenFileName(this,
      QString::fromStdString(CoreBridge::Text(CoreBridge::TText::ImportIniTitle)), QString(), tr("INI files (*.ini);;All files (*)"));
    if (!FileName.isEmpty())
    {
      LoadIni(FileName);
    }
  });
  connect(FPaste, &QPushButton::clicked, this, [this]() { LoadKnownHosts(QApplication::clipboard()->text()); });
  connect(FList, &QListWidget::itemChanged, this, &ImportSitesDialog::UpdateControls);

  try
  {
    FSites.reset(new TImportSites());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
  int First = -1;
  for (int Source = 0; Source < TImportSites::SourceCount; Source++)
  {
    FSource->addItem(FSites ? QString::fromStdString(FSites->SourceName(Source)) : QString());
    // As Init: the first source with any sites.
    if ((First < 0) && FSites && !FSites->Sites(Source).empty())
    {
      First = Source;
    }
  }
  FSource->setCurrentIndex(qMax(First, 0));
  connect(FSource, &QComboBox::currentIndexChanged, this, [this]()
  {
    SaveSelection();
    LoadSites();
  });
  LoadSites();
  resize(440, 380);
}

void ImportSitesDialog::SaveSelection()
{
  for (int Row = 0; FSites && (FShown >= 0) && (Row < FList->count()); Row++)
  {
    FSites->Select(FShown, Row, FList->item(Row)->checkState() == Qt::Checked);
  }
}

// As TImportSessionsDialog::LoadSessions: the source's sites, or why it
// has none.
void ImportSitesDialog::LoadSites()
{
  FList->blockSignals(true);
  FList->clear();
  FShown = FSource->currentIndex();
  if (FSites)
  {
    const auto Sites = FSites->Sites(FShown);
    for (int Index = 0; Index < static_cast<int>(Sites.size()); Index++)
    {
      QListWidgetItem * Item = new QListWidgetItem(QString::fromStdString(Sites[Index].first), FList);
      Item->setFlags(Item->flags() | Qt::ItemIsUserCheckable);
      Item->setCheckState(FSites->Selected(FShown, Index) ? Qt::Checked : Qt::Unchecked);
      Item->setToolTip(QString::fromStdString(Sites[Index].second));
    }
    const QString Error = QString::fromStdString(FSites->Error(FShown));
    FError->setText(Error);
    FError->setVisible(Sites.empty() && !Error.isEmpty());
  }
  FList->blockSignals(false);
  UpdateControls();
}

void ImportSitesDialog::UpdateControls()
{
  FBrowse->setVisible(FSource->currentIndex() == TImportSites::Ini);
  FPaste->setVisible(FSource->currentIndex() == TImportSites::KnownHosts);
  bool Any = false;
  for (int Row = 0; Row < FList->count(); Row++)
  {
    Any = Any || (FList->item(Row)->checkState() == Qt::Checked);
  }
  FOk->setEnabled(Any);
  FCheckAll->setEnabled(FList->count() > 0);
}

void ImportSitesDialog::LoadIni(const QString & FileName)
{
  if (FSites)
  {
    FSites->LoadIni(QDir::toNativeSeparators(FileName).toStdString());
    LoadSites();
  }
}

void ImportSitesDialog::LoadKnownHosts(const QString & Text)
{
  if (FSites)
  {
    FSites->LoadKnownHosts(Text.toStdString());
    LoadSites();
  }
}

void ImportSitesDialog::accept()
{
  if (!FSites)
  {
    return;
  }
  SaveSelection();
  try
  {
    FImported = FSites->Import(FSource->currentIndex());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
    return;
  }
  QDialog::accept();
}
