#include "TransferDialog.h"

#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

TransferDialog::TransferDialog(bool Upload, int FileCount, QWidget * Parent) :
  QDialog(Parent)
{
  setWindowTitle(Upload ? tr("Uploading") : tr("Downloading"));
  setWindowModality(Qt::WindowModal);
  setMinimumWidth(460);
  QVBoxLayout * Layout = new QVBoxLayout(this);
  QFormLayout * Form = new QFormLayout();
  FFile = new QLabel(this);
  FFile->setTextFormat(Qt::PlainText);
  FFile->setMinimumWidth(320);
  Form->addRow(tr("File:"), FFile);
  FFileBar = new QProgressBar(this);
  FFileBar->setRange(0, 100);
  Form->addRow(tr("File progress:"), FFileBar);
  FOverallBar = new QProgressBar(this);
  FOverallBar->setRange(0, 100);
  Form->addRow(tr("Overall progress:"), FOverallBar);
  FBytes = new QLabel(this);
  Form->addRow(tr("Transferred:"), FBytes);
  FSpeed = new QLabel(this);
  Form->addRow(tr("Speed:"), FSpeed);
  Layout->addLayout(Form);
  FCancel = new QPushButton(tr("Cancel"), this);
  connect(FCancel, &QPushButton::clicked, this, &TransferDialog::reject);
  QHBoxLayout * Buttons = new QHBoxLayout();
  Buttons->addStretch(1);
  Buttons->addWidget(FCancel);
  Layout->addLayout(Buttons);
  FOverallBar->setFormat(tr("%p% of %1 file(s)").arg(FileCount));
}

void TransferDialog::Update(const CoreBridge::TTransferProgress & Progress)
{
  FFile->setText(QString::fromStdString(Progress.FileName));
  FFileBar->setValue(Progress.FilePercent);
  FOverallBar->setValue(Progress.OverallPercent);
  const QLocale Locale;
  FBytes->setText((Progress.Total >= 0) ?
    tr("%1 of %2").arg(Locale.formattedDataSize(Progress.Transferred), Locale.formattedDataSize(Progress.Total)) :
    Locale.formattedDataSize(Progress.Transferred));
  FSpeed->setText(tr("%1/s").arg(Locale.formattedDataSize(Progress.Speed)));
}

// Cancel, Escape and closing the window cancel the transfer; core stops at
// its next progress report, and the window closes when the transfer ends.
void TransferDialog::reject()
{
  FCancelled = true;
  FCancel->setEnabled(false);
}
