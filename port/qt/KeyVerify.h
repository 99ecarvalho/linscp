// Upstream's VerifyAndConvertKey (WinInterface's), for the site manager's
// login and save and for Install Public Key into Server: an OpenSSH or
// ssh.com key offered for conversion to PuTTY's format (where to, from
// ChooseTarget), one that cannot be used warned of (Ignore goes on, when
// CanIgnore). FileName becomes the converted key's. False to stop.
#ifndef LINSCP_QT_KEY_VERIFY_H
#define LINSCP_QT_KEY_VERIFY_H

#include <QString>

#include <functional>

class QWidget;

bool VerifyAndConvertKey(QWidget * Window, QString & FileName, bool CanIgnore,
  const std::function<QString(const QString & Suggested)> & ChooseTarget);
// The file dialog ChooseTarget is by default.
QString ChooseConvertedKeyFile(QWidget * Window, const QString & Suggested);

#endif
