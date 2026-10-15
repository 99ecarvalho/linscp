#include "PanelBands.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QVBoxLayout>

void TPanelBands::Make(QWidget * Parent)
{
  const char * const Names[] = { "Path", "History", "Navigation", "File", "Selection" };
  for (int Index = 0; Index < static_cast<int>(TPanelBand::Count); Index++)
  {
    QToolBar * Bar = new QToolBar(Parent);
    Bar->setObjectName(QString::fromLatin1(Names[Index]));
    Bar->setIconSize(QSize(16, 16));
    Bar->setFloatable(false);
    Bar->setMovable(false);
    Bands[Index] = Bar;
  }
}

void TPanelBands::LayOut(QVBoxLayout * Layout, QLineEdit * Path)
{
  QHBoxLayout * First = new QHBoxLayout();
  First->setSpacing(0);
  // The path keeps room for itself; the bands overflow (their chevron).
  Path->setMinimumWidth(Path->fontMetrics().averageCharWidth() * 24);
  First->addWidget(Path, 1);
  for (TPanelBand Band : { TPanelBand::Path, TPanelBand::History, TPanelBand::Navigation })
  {
    First->addWidget((*this)[Band]);
  }
  QHBoxLayout * Second = new QHBoxLayout();
  Second->setSpacing(0);
  for (TPanelBand Band : { TPanelBand::File, TPanelBand::Selection })
  {
    Second->addWidget((*this)[Band]);
  }
  Second->addStretch(1);
  Layout->addLayout(First);
  Layout->addLayout(Second);
}
