#include "CleanupDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "QtFrontend.h"

CleanupDialog::CleanupDialog(QWidget * Parent) :
  QDialog(Parent),
  FData(new CoreBridge::TCleanupData())
{
  setWindowTitle(tr("Cleanup application data"));
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QLabel * Label = new QLabel(tr("Following list contains all application data possibly stored on this computer. "
    "Check those you want to remove.\n\nIf another instance of application is running, please close it before "
    "cleaning data.\n\nNotice: Opening session and/or next execution of application will recreate some of data."), this);
  Label->setWordWrap(true);
  Label->setTextFormat(Qt::PlainText);
  Layout->addWidget(Label);

  FList = new QTreeWidget(this);
  FList->setRootIsDecorated(false);
  FList->setHeaderLabels({ tr("Data"), tr("Location") });
  FList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  for (const CoreBridge::TCleanupData::TItem & Data : FData->Items())
  {
    QTreeWidgetItem * Item = new QTreeWidgetItem(FList);
    Item->setText(0, QString::fromStdString(Data.Caption));
    Item->setText(1, QString::fromStdString(Data.Location));
    Item->setCheckState(0, Qt::Unchecked);
    // As DataListViewInfoTip.
    Item->setToolTip(0, tr("%1\nLocation: %2").arg(Item->text(0), Item->text(1)));
    Item->setToolTip(1, Item->toolTip(0));
  }
  connect(FList, &QTreeWidget::itemChanged, this, &CleanupDialog::UpdateControls);
  Layout->addWidget(FList, 1);

  QDialogButtonBox * Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close, this);
  FOk = Buttons->button(QDialogButtonBox::Ok);
  FCheckAll = Buttons->addButton(tr("Un/check &all"), QDialogButtonBox::ResetRole);
  connect(FCheckAll, &QPushButton::clicked, this, [this]()
  {
    // As ListViewCheckAll's caToggle: all checked, unless all already are.
    bool All = true;
    for (int Index = 0; Index < FList->topLevelItemCount(); Index++)
    {
      All = All && (FList->topLevelItem(Index)->checkState(0) == Qt::Checked);
    }
    for (int Index = 0; Index < FList->topLevelItemCount(); Index++)
    {
      FList->topLevelItem(Index)->setCheckState(0, All ? Qt::Unchecked : Qt::Checked);
    }
  });
  connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Layout->addWidget(Buttons);
  resize(560, 400);
  UpdateControls();
}

CleanupDialog::~CleanupDialog() = default;

void CleanupDialog::UpdateControls()
{
  bool Any = false;
  for (int Index = 0; Index < FList->topLevelItemCount(); Index++)
  {
    Any = Any || (FList->topLevelItem(Index)->checkState(0) == Qt::Checked);
  }
  FOk->setEnabled(Any);
}

bool CleanupDialog::Execute()
{
  if (exec() != QDialog::Accepted)
  {
    return false;
  }
  for (int Index = 0; Index < FList->topLevelItemCount(); Index++)
  {
    if (FList->topLevelItem(Index)->checkState(0) == Qt::Checked)
    {
      try
      {
        FData->Cleanup(static_cast<std::size_t>(Index));
      }
      catch (const CoreBridge::TCoreError & E)
      {
        if (!E.Aborted)
        {
          QtFrontend Frontend;
          Frontend.SetWindow(this);
          Frontend.ShowError(E.Error);
        }
      }
    }
  }
  return true;
}
