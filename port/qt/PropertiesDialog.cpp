#include "PropertiesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "FileList.h"
#include "QtFrontend.h"
#include "RightsFrame.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

// WinSCP's "%d files" and the like.
QString Count(CoreBridge::TText One, CoreBridge::TText Many, int Number)
{
  return CoreText((Number == 1) ? One : Many).replace(QStringLiteral("%d"), QLocale().toString(Number));
}

QLineEdit * View(QWidget * Parent)
{
  QLineEdit * Edit = new QLineEdit(Parent);
  Edit->setReadOnly(true);
  Edit->setFrame(false);
  return Edit;
}

QFrame * Line(QWidget * Parent)
{
  QFrame * Frame = new QFrame(Parent);
  Frame->setFrameShape(QFrame::HLine);
  Frame->setFrameShadow(QFrame::Sunken);
  return Frame;
}

}  // namespace

PropertiesDialog::PropertiesDialog(CoreBridge::TSession * Session, const std::vector<std::string> & Names,
    const QString & Directory, const CoreBridge::TPropertiesInfo & Info, QWidget * Parent) :
  QDialog(Parent),
  FSession(Session),
  FNames(Names),
  FInfo(Info),
  FMultiple(Names.size() > 1)
{
  const QString First = Names.empty() ? QString() : QString::fromStdString(Names.front());
  setWindowTitle(CoreText(FMultiple ? CoreBridge::TText::PropertiesFilesCaption : CoreBridge::TText::PropertiesFileCaption)
    .replace(QStringLiteral("%s"), First));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  FTabs = new QTabWidget(this);
  Layout->addWidget(FTabs);

  // Common.
  QWidget * Common = new QWidget(FTabs);
  QVBoxLayout * CommonLayout = new QVBoxLayout(Common);
  QHBoxLayout * Head = new QHBoxLayout();
  QLabel * Icon = new QLabel(Common);
  QFileIconProvider Icons;
  const bool Directory1 = !FMultiple && (Info.Directories == 1);
  Icon->setPixmap(Icons.icon(FMultiple ? QFileIconProvider::File :
    (Directory1 ? QFileIconProvider::Folder : QFileIconProvider::File)).pixmap(32, 32));
  Head->addWidget(Icon);
  FFile = View(Common);
  Head->addWidget(FFile, 1);
  CommonLayout->addLayout(Head);
  CommonLayout->addWidget(Line(Common));
  QFormLayout * Fields = new QFormLayout();
  QLineEdit * Location = View(Common);
  Location->setText(Directory);
  Fields->addRow(tr("Location:"), Location);
  QHBoxLayout * SizeRow = new QHBoxLayout();
  FSize = View(Common);
  SizeRow->addWidget(FSize, 1);
  FCalculate = new QPushButton(tr("C&alculate"), Common);
  FCalculate->setVisible(Info.CanCalculateSize);
  connect(FCalculate, &QPushButton::clicked, this, [this]() { CalculateSize(); });
  SizeRow->addWidget(FCalculate);
  Fields->addRow(tr("Size:"), SizeRow);
  FLink = View(Common);
  FLink->setText(QString::fromStdString(Info.LinkTarget));
  if (!Info.LinkTarget.empty())
  {
    Fields->addRow(tr("Links to:"), FLink);
  }
  else
  {
    FLink->hide();
  }
  CommonLayout->addLayout(Fields);
  CommonLayout->addWidget(Line(Common));

  // Group, owner, permissions: what the server can change, else shown.
  QFormLayout * Change = new QFormLayout();
  const CoreBridge::TFileProperties & Current = Info.Properties;
  const auto Token = [this, Common, Change](const QString & Caption, bool Has, const std::string & Value,
    bool CanChange, const std::vector<std::string> & List) -> QComboBox *
  {
    QComboBox * Combo = new QComboBox(Common);
    Combo->setEditable(true);
    for (const std::string & Item : List)
    {
      Combo->addItem(QString::fromStdString(Item));
    }
    Combo->setEditText(Has ? QString::fromStdString(Value) : QString());
    Combo->setEnabled(CanChange);
    if (CanChange || Has)
    {
      Change->addRow(Caption, Combo);
    }
    else
    {
      Combo->hide();
    }
    connect(Combo, &QComboBox::editTextChanged, this, &PropertiesDialog::UpdateControls);
    return Combo;
  };
  FGroup = Token(tr("Group:"), Current.HasGroup, Current.Group, Info.CanChangeGroup, Info.Groups);
  FOwner = Token(tr("Owner:"), Current.HasOwner, Current.Owner, Info.CanChangeOwner, Info.Owners);
  FRights = new RightsFrame(Common);
  FRights->SetAllowUndef(FMultiple);
  FRights->SetAllowAddXToDirectories(Info.Directories > 0);
  if (Current.HasRights)
  {
    FRights->SetRights(Current.Rights);
    FRights->SetAddXToDirectories(Current.AddXToDirectories);
    Change->addRow(tr("Permissions:"), FRights);
  }
  else
  {
    FRights->hide();
  }
  FRights->setEnabled(Info.CanChangeRights);
  connect(FRights, &RightsFrame::Changed, this, &PropertiesDialog::UpdateControls);
  CommonLayout->addLayout(Change);
  FRecursive = new QCheckBox(tr("Set owner, group and permissions &recursively"), Common);
  const bool AnyChange = Info.CanChangeGroup || Info.CanChangeOwner || Info.CanChangeRights;
  FRecursive->setVisible(AnyChange && (Info.Directories > 0));
  connect(FRecursive, &QCheckBox::toggled, this, &PropertiesDialog::UpdateControls);
  CommonLayout->addWidget(Line(Common));
  CommonLayout->addWidget(FRecursive);
  CommonLayout->addStretch(1);
  FTabs->addTab(Common, tr("Common"));

  // Checksum, where the server calculates them.
  if (!Info.ChecksumAlgorithms.empty())
  {
    QWidget * Checksum = new QWidget(FTabs);
    QVBoxLayout * ChecksumLayout = new QVBoxLayout(Checksum);
    QHBoxLayout * Row = new QHBoxLayout();
    QLabel * AlgorithmLabel = new QLabel(tr("&Algorithm:"), Checksum);
    FChecksumAlgorithm = new QComboBox(Checksum);
    FChecksumAlgorithm->setEditable(true);
    for (const std::string & Algorithm : Info.ChecksumAlgorithms)
    {
      FChecksumAlgorithm->addItem(QString::fromStdString(Algorithm));
    }
    AlgorithmLabel->setBuddy(FChecksumAlgorithm);
    QPushButton * Calculate = new QPushButton(tr("&Calculate checksum"), Checksum);
    connect(Calculate, &QPushButton::clicked, this, [this]() { CalculateChecksum(); });
    Row->addWidget(AlgorithmLabel);
    Row->addWidget(FChecksumAlgorithm, 1);
    Row->addWidget(Calculate);
    ChecksumLayout->addLayout(Row);
    // One file: its checksum; several (or directories): a list.
    if (!FMultiple && (Info.Directories == 0))
    {
      QGroupBox * Group = new QGroupBox(tr("Checksum"), Checksum);
      QVBoxLayout * GroupLayout = new QVBoxLayout(Group);
      FChecksum = View(Group);
      FChecksum->setText(CoreText(CoreBridge::TText::PropertiesChecksumUnknown));
      GroupLayout->addWidget(FChecksum);
      ChecksumLayout->addWidget(Group);
    }
    else
    {
      FChecksums = new QTreeWidget(Checksum);
      FChecksums->setHeaderLabels({ tr("File"), tr("Checksum") });
      FChecksums->setRootIsDecorated(false);
      ChecksumLayout->addWidget(FChecksums, 1);
    }
    ChecksumLayout->addStretch(FChecksum != nullptr ? 1 : 0);
    FTabs->addTab(Checksum, tr("Checksum"));
  }

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);

  // A single entry shows its name until a directory's size is calculated.
  LoadStats(Info.Size, Info.Files, Info.Directories, Info.SymLinks, false);
  UpdateControls();
  resize(qMax(sizeHint().width(), 420), sizeHint().height());
}

// As TPropertiesDialog::LoadStats.
void PropertiesDialog::LoadStats(std::int64_t Size, int Files, int Directories, int SymLinks, bool Calculated)
{
  QString SizeText;
  if (FInfo.CanCalculateSize && !Calculated)
  {
    SizeText = CoreText(CoreBridge::TText::PropertiesUnknownSize);
  }
  else
  {
    SizeText = FormatBytes(Size);
    const QString Unordered = QStringLiteral("%1 B").arg(QLocale().toString(static_cast<qlonglong>(Size)));
    if (SizeText != Unordered)
    {
      SizeText = QStringLiteral("%1 (%2)").arg(SizeText, Unordered);
    }
  }
  FSize->setText(SizeText);
  QString FilesText;
  if (!FMultiple && !Calculated)
  {
    FilesText = QString::fromStdString(FNames.front());
  }
  else
  {
    if (Files > 0)
    {
      FilesText = Count(CoreBridge::TText::PropertiesFile, CoreBridge::TText::PropertiesFiles, Files);
      if (Directories > 0)
      {
        FilesText += QStringLiteral(", ");
      }
    }
    if (Directories > 0)
    {
      FilesText += Count(CoreBridge::TText::PropertiesDirectory, CoreBridge::TText::PropertiesDirectories, Directories);
    }
    if (SymLinks > 0)
    {
      FilesText = QStringLiteral("%1 (%2)").arg(FilesText,
        Count(CoreBridge::TText::PropertiesSymlink, CoreBridge::TText::PropertiesSymlinks, SymLinks));
    }
  }
  FFile->setText(FilesText);
  FFile->setCursorPosition(0);
}

CoreBridge::TFileProperties PropertiesDialog::Properties() const
{
  CoreBridge::TFileProperties Result;
  Result.HasRights = !FRights->isHidden();
  Result.Rights = FRights->Rights();
  Result.AddXToDirectories = FRights->AddXToDirectories();
  Result.HasOwner = !FOwner->isHidden();
  Result.Owner = FOwner->currentText().toStdString();
  Result.HasGroup = !FGroup->isHidden();
  Result.Group = FGroup->currentText().toStdString();
  Result.Recursive = !FRecursive->isHidden() && FRecursive->isChecked();
  return Result;
}

// As TPropertiesDialog::UpdateControls: OK once something would change.
void PropertiesDialog::UpdateControls()
{
  if (FOk == nullptr)
  {
    return;
  }
  const CoreBridge::TFileProperties Now = Properties();
  const CoreBridge::TFileProperties & Was = FInfo.Properties;
  const bool RightsChanged = FInfo.CanChangeRights && Was.HasRights &&
    ((Now.Rights.Set != Was.Rights.Set) || (Now.Rights.Unset != Was.Rights.Unset) ||
     (Now.AddXToDirectories != Was.AddXToDirectories));
  const bool OwnerChanged = FInfo.CanChangeOwner && (Now.Owner != (Was.HasOwner ? Was.Owner : std::string()));
  const bool GroupChanged = FInfo.CanChangeGroup && (Now.Group != (Was.HasGroup ? Was.Group : std::string()));
  FOk->setEnabled(RightsChanged || OwnerChanged || GroupChanged || Now.Recursive);
}

void PropertiesDialog::ShowError(const CoreBridge::TCoreError & E)
{
  if (!E.Aborted)
  {
    QtFrontend Frontend;
    Frontend.SetWindow(this);
    Frontend.ShowError(E.Error);
  }
}

bool PropertiesDialog::CalculateSize()
{
  std::int64_t Size = 0;
  int Files = 0;
  int Directories = 0;
  int SymLinks = 0;
  try
  {
    FSession->CalculateSize(FNames, Size, Files, Directories, SymLinks);
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  LoadStats(Size, Files, Directories, SymLinks, true);
  FCalculate->setEnabled(false);
  return true;
}

bool PropertiesDialog::CalculateChecksum()
{
  if (FChecksumAlgorithm == nullptr)
  {
    return false;
  }
  std::vector<std::pair<std::string, std::string>> Checksums;
  try
  {
    Checksums = FSession->Checksums(FNames, FChecksumAlgorithm->currentText().toStdString());
  }
  catch (const CoreBridge::TCoreError & E)
  {
    ShowError(E);
    return false;
  }
  if (FChecksum != nullptr)
  {
    FChecksum->setText(Checksums.empty() ? CoreText(CoreBridge::TText::PropertiesChecksumUnknown) :
      QString::fromStdString(Checksums.front().second));
  }
  else
  {
    FChecksums->clear();
    for (const auto & Checksum : Checksums)
    {
      FChecksums->addTopLevelItem(new QTreeWidgetItem({ QString::fromStdString(Checksum.first), QString::fromStdString(Checksum.second) }));
    }
    FChecksums->resizeColumnToContents(0);
  }
  return true;
}
