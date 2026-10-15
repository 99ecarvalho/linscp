// WinSCP hands a command line naming a session to the instance already
// running, whose window opens it (WinMain's SendToAnotherInstance, over
// WM_COPYDATA). Here over a local socket, one per user: Send offers the
// arguments, InstanceServer takes them for the window.
#ifndef LINSCP_QT_SINGLE_INSTANCE_H
#define LINSCP_QT_SINGLE_INSTANCE_H

#include <QObject>
#include <QStringList>

#include <functional>

class QLocalServer;

namespace SingleInstance
{

// The socket's name: the user's; Suffix sets apart others (tests').
QString ServerName(const QString & Suffix = QString());
// Offers the arguments to the instance listening, true once its window took
// them (it then opens them on its own); false with none, or one busy.
// Error, when given, says why not.
bool Send(const QStringList & Arguments, const QString & Suffix = QString(), QString * Error = nullptr);

// As AnyOtherInstanceOfSelf: another process of this program running
// (setting the master password then may lose the passwords it saves).
bool AnyOtherInstance();

}  // namespace SingleInstance

class InstanceServer : public QObject
{
  Q_OBJECT

public:
  // Listens; Handler says whether the window takes a command line
  // (CommandLineFromAnotherInstance).
  InstanceServer(std::function<bool(const QStringList &)> Handler, const QString & Suffix = QString(),
    QObject * Parent = nullptr);
  bool Listening() const;

private:
  void Accept();

  std::function<bool(const QStringList &)> FHandler;
  QLocalServer * FServer = nullptr;
};

#endif
