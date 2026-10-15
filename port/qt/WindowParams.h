// A window's place as WinSCP keeps it (StoreForm, RestoreForm):
// "left;top;right;bottom;state;ppi", relative to its monitor's work area,
// the normal bounds even when maximized (state 2); "-1;-1;width;height" when
// never placed. Scaled from the pixels per inch saved to the screen's.
#ifndef LINSCP_QT_WINDOW_PARAMS_H
#define LINSCP_QT_WINDOW_PARAMS_H

#include <QString>

class QWidget;

QString StoreWindowParams(const QWidget * Window);
// False (nothing done) for params that are not one.
bool RestoreWindowParams(QWidget * Window, const QString & Params);
// A dialog's size alone (StoreFormSize, RestoreFormSize): "width,height,ppi".
QString StoreWindowSize(const QWidget * Window);
bool RestoreWindowSize(QWidget * Window, const QString & Size);

#endif
