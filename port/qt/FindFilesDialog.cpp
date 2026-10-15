#include "FindFilesDialog.h"
#include "HintButton.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EditMaskDialog.h"
#include "FileList.h"
#include "MainWindow.h"

namespace
{

QString CoreText(CoreBridge::TText Text)
{
  return QString::fromStdString(CoreBridge::Text(Text));
}

enum TColumn { NameColumn, DirectoryColumn, SizeColumn, ModifiedColumn };
const int IndexRole = Qt::UserRole;
const int SortRole = Qt::UserRole + 1;

// Sorted by the value, not the text.
class TResultItem : public QTreeWidgetItem
{
public:
  using QTreeWidgetItem::QTreeWidgetItem;

  bool operator<(const QTreeWidgetItem & Other) const override
  {
    const int Column = (treeWidget() != nullptr) ? treeWidget()->sortColumn() : 0;
    const QVariant Mine = data(Column, SortRole);
    const QVariant Theirs = Other.data(Column, SortRole);
    if (Mine.isValid() && Theirs.isValid())
    {
      return Mine.toDouble() < Theirs.toDouble();
    }
    return QTreeWidgetItem::operator<(Other);
  }
};

}  // namespace

FindFilesDialog::FindFilesDialog(MainWindow * Window, const QString & Directory) :
  QDialog(Window),
  FWindow(Window)
{
  setWindowTitle(CoreText(CoreBridge::TText::FindFileTitle));
  setAttribute(Qt::WA_DeleteOnClose);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QHBoxLayout * Top = new QHBoxLayout();
  QGroupBox * Filter = new QGroupBox(tr("Filter"), this);
  QFormLayout * Fields = new QFormLayout(Filter);
  FMask = new QComboBox(Filter);
  FMask->setEditable(true);
  FMask->setEditText(FileView::LastMask());
  QHBoxLayout * MaskRow = new QHBoxLayout();
  MaskRow->addWidget(FMask, 1);
  QPushButton * EditMask = new QPushButton(tr("&Edit..."), Filter);
  connect(EditMask, &QPushButton::clicked, this, [this]()
  {
    QString Mask = FMask->currentText();
    if (EditMaskDialog::Edit(Mask, this))
    {
      FMask->setEditText(Mask);
    }
  });
  MaskRow->addWidget(EditMask);
  QLabel * MaskLabel = new QLabel(tr("&File mask:"), Filter);
  MaskLabel->setBuddy(FMask);
  Fields->addRow(MaskLabel, MaskRow);
  Fields->addRow(QString(), MakeMaskHintButton(tr("mask hi&nts"),
    { CoreBridge::TText::MaskHint, CoreBridge::TText::FileMaskExHint, CoreBridge::TText::CombiningMasksHint,
      CoreBridge::TText::PathMaskHint, CoreBridge::TText::DirectoryMaskHint }, Filter));
  FDirectory = new QComboBox(Filter);
  FDirectory->setEditable(true);
  FDirectory->setEditText(Directory);
  Fields->addRow(tr("Sear&ch in:"), FDirectory);
  Top->addWidget(Filter, 1);
  FStart = new QPushButton(CoreText(CoreBridge::TText::FindFileStart), this);
  FStart->setDefault(true);
  connect(FStart, &QPushButton::clicked, this, [this]() { FFinding ? Stop() : Start(); });
  QVBoxLayout * Side = new QVBoxLayout();
  Side->addWidget(FStart);
  Side->addStretch(1);
  Top->addLayout(Side);
  Layout->addLayout(Top);

  QHBoxLayout * Middle = new QHBoxLayout();
  FResults = new QTreeWidget(this);
  FResults->setHeaderLabels({ tr("Name"), tr("Directory"), tr("Size"), tr("Date modified") });
  FResults->setAccessibleName(tr("Found files"));
  FResults->setRootIsDecorated(false);
  FResults->setSelectionMode(QAbstractItemView::ExtendedSelection);
  FResults->setSortingEnabled(true);
  FResults->sortByColumn(NameColumn, Qt::AscendingOrder);
  connect(FResults, &QTreeWidget::itemSelectionChanged, this, &FindFilesDialog::UpdateControls);
  connect(FResults, &QTreeWidget::itemDoubleClicked, this, [this]() { Focus(); });
  Middle->addWidget(FResults, 1);
  QVBoxLayout * Buttons = new QVBoxLayout();
  FFocus = new QPushButton(tr("Fo&cus"), this);
  FEdit = new QPushButton(tr("&Edit"), this);
  FDownload = new QPushButton(tr("Down&load..."), this);
  FDelete = new QPushButton(tr("&Delete"), this);
  FCopy = new QPushButton(tr("&Copy Results"), this);
  connect(FFocus, &QPushButton::clicked, this, &FindFilesDialog::Focus);
  connect(FEdit, &QPushButton::clicked, this, &FindFilesDialog::Edit);
  connect(FDownload, &QPushButton::clicked, this, &FindFilesDialog::Download);
  connect(FDelete, &QPushButton::clicked, this, &FindFilesDialog::Delete);
  connect(FCopy, &QPushButton::clicked, this, &FindFilesDialog::CopyResults);
  for (QPushButton * Button : { FFocus, FEdit, FDownload, FDelete, FCopy })
  {
    Buttons->addWidget(Button);
  }
  Buttons->addStretch(1);
  QPushButton * Close = new QPushButton(tr("Close"), this);
  connect(Close, &QPushButton::clicked, this, &QWidget::close);
  Buttons->addWidget(Close);
  Middle->addLayout(Buttons);
  Layout->addLayout(Middle, 1);
  FStatus = new QLabel(this);
  Layout->addWidget(FStatus);
  // The action keys, as the dialog's (F4 edit, F5 download, Del delete).
  for (const auto & Key : std::initializer_list<std::pair<QKeySequence, QPushButton *>>{
         { QKeySequence(Qt::Key_F4), FEdit }, { QKeySequence(Qt::Key_F5), FDownload }, { QKeySequence(Qt::Key_Delete), FDelete } })
  {
    Key.second->setShortcut(Key.first);
  }
  resize(720, 480);
  UpdateControls();
}

void FindFilesDialog::Start()
{
  if (FFinding)
  {
    return;
  }
  FileView::SetLastMask(FMask->currentText());
  FFinding = true;
  FAbort = false;
  FFound.clear();
  FResults->clear();
  FResults->setSortingEnabled(false);
  FStart->setText(CoreText(CoreBridge::TText::FindFileStop));
  FStatus->setText(CoreText(CoreBridge::TText::FindFileFinding));
  UpdateControls();
  QString Directory = FDirectory->currentText();
  while ((Directory.size() > 1) && Directory.endsWith(QLatin1Char('/')))
  {
    Directory.chop(1);
  }
  const bool Done = FWindow->FindFiles(Directory, FMask->currentText(),
    [this](const std::string & In, const CoreBridge::TRemoteEntry & Entry)
    {
      const int Index = static_cast<int>(FFound.size());
      FFound.push_back({ QString::fromStdString(In), Entry });
      QTreeWidgetItem * Item = new TResultItem(FResults);
      Item->setText(NameColumn, QString::fromStdString(Entry.Name));
      Item->setText(DirectoryColumn, QString::fromStdString(In));
      Item->setText(SizeColumn, Entry.Directory ? QString() : FormatBytes(Entry.Size));
      Item->setData(SizeColumn, SortRole, static_cast<double>(Entry.Size));
      Item->setText(ModifiedColumn, QString::fromStdString(Entry.Modified));
      Item->setData(ModifiedColumn, SortRole, Entry.ModifiedTime);
      Item->setData(NameColumn, IndexRole, Index);
      return !FAbort;
    },
    [this](const std::string & In)
    {
      FStatus->setText(CoreText(CoreBridge::TText::FindFileInDirectory).replace(QStringLiteral("%s"), QString::fromStdString(In)));
      // As upstream: the dialog stays usable (Stop) while it searches.
      QApplication::processEvents();
      return !FAbort;
    });
  FFinding = false;
  FResults->setSortingEnabled(true);
  for (int Column = 0; Column < FResults->columnCount(); Column++)
  {
    FResults->resizeColumnToContents(Column);
  }
  FStart->setText(CoreText(CoreBridge::TText::FindFileStart));
  FStatus->setText(CoreText((FAbort || !Done) ? CoreBridge::TText::FindFileAborted : CoreBridge::TText::FindFileDone));
  UpdateControls();
}

void FindFilesDialog::Stop()
{
  FAbort = true;
}

std::vector<int> FindFilesDialog::SelectedIndexes() const
{
  std::vector<int> Result;
  for (QTreeWidgetItem * Item : FResults->selectedItems())
  {
    Result.push_back(Item->data(NameColumn, IndexRole).toInt());
  }
  return Result;
}

void FindFilesDialog::UpdateControls()
{
  const std::vector<int> Selected = SelectedIndexes();
  const bool One = (Selected.size() == 1);
  const bool OneFile = One && !FFound[Selected.front()].Entry.Directory;
  FMask->setEnabled(!FFinding);
  FDirectory->setEnabled(!FFinding);
  FFocus->setEnabled(!FFinding && One);
  FEdit->setEnabled(!FFinding && OneFile);
  FDownload->setEnabled(!FFinding && !Selected.empty());
  FDelete->setEnabled(!FFinding && !Selected.empty());
  FCopy->setEnabled(!FFinding && (FResults->topLevelItemCount() > 0));
}

void FindFilesDialog::Focus()
{
  const std::vector<int> Selected = SelectedIndexes();
  if (!FFinding && (Selected.size() == 1))
  {
    const TFound & Found = FFound[Selected.front()];
    FWindow->FocusRemote(Found.Directory, QString::fromStdString(Found.Entry.Name));
  }
}

void FindFilesDialog::Edit()
{
  const std::vector<int> Selected = SelectedIndexes();
  if (!FFinding && (Selected.size() == 1) && !FFound[Selected.front()].Entry.Directory)
  {
    const TFound & Found = FFound[Selected.front()];
    if (FWindow->FocusRemote(Found.Directory, QString::fromStdString(Found.Entry.Name)))
    {
      FWindow->EditRemote(QString::fromStdString(Found.Entry.Name));
    }
  }
}

void FindFilesDialog::Download()
{
  const std::vector<int> Selected = SelectedIndexes();
  if (!FFinding && !Selected.empty())
  {
    QStringList Names;
    for (int Index : Selected)
    {
      Names.append(QString::fromStdString(FFound[Index].Entry.Name));
    }
    FWindow->DownloadFound(Selected, Names);
  }
}

void FindFilesDialog::Delete()
{
  const std::vector<int> Selected = SelectedIndexes();
  if (!FFinding && !Selected.empty())
  {
    QStringList Names;
    for (int Index : Selected)
    {
      Names.append(QString::fromStdString(FFound[Index].Entry.Name));
    }
    if (FWindow->DeleteFound(Selected, Names))
    {
      // Gone: off the list too.
      for (QTreeWidgetItem * Item : FResults->selectedItems())
      {
        delete Item;
      }
    }
    UpdateControls();
  }
}

// As upstream's Copy Results: each file's full path, a line each.
void FindFilesDialog::CopyResults()
{
  QStringList Lines;
  for (int Row = 0; Row < FResults->topLevelItemCount(); Row++)
  {
    const TFound & Found = FFound[FResults->topLevelItem(Row)->data(NameColumn, IndexRole).toInt()];
    Lines.append((Found.Directory.endsWith(QLatin1Char('/')) ? Found.Directory : (Found.Directory + QLatin1Char('/'))) +
      QString::fromStdString(Found.Entry.Name));
  }
  QApplication::clipboard()->setText(Lines.join(QLatin1Char('\n')));
}

void FindFilesDialog::closeEvent(QCloseEvent * Event)
{
  if (FFinding)
  {
    FAbort = true;
    Event->ignore();
    return;
  }
  QDialog::closeEvent(Event);
}
