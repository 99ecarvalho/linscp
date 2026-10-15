// A panel's toolbars, as WinSCP's Commander docks them above each panel
// (forms/ScpCommander.dfm): the path with its band (Open Directory,
// Filter), History and Navigation on the first row; Files and Selection on
// the second. The panel's own actions go in History and Navigation; the
// window fills Path, Files and Selection with its commands for that side,
// and shows or hides each band (Options > Left/Right Panel).
#ifndef LINSCP_QT_PANEL_BANDS_H
#define LINSCP_QT_PANEL_BANDS_H

#include <QToolBar>

class QLineEdit;
class QVBoxLayout;

enum class TPanelBand { Path, History, Navigation, File, Selection, Count };

struct TPanelBands
{
  QToolBar * Bands[static_cast<int>(TPanelBand::Count)] = {};
  QToolBar * operator[](TPanelBand Band) const { return Bands[static_cast<int>(Band)]; }
  // Made, Parent's; laid out in Layout around Path.
  void Make(QWidget * Parent);
  void LayOut(QVBoxLayout * Layout, QLineEdit * Path);
};

#endif
