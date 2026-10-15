#include "SingleInstance.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcessEnvironment>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#endif

namespace
{

// How long a running instance has to answer: its window busy with a modal
// dialog still answers, as the event loop runs.
const int Timeout = 5000;

}  // namespace

namespace SingleInstance
{

QString ServerName(const QString & Suffix)
{
  const QProcessEnvironment Environment = QProcessEnvironment::systemEnvironment();
  const QString User = Environment.value(QStringLiteral("USER"), Environment.value(QStringLiteral("USERNAME")));
  const QByteArray Hash = QCryptographicHash::hash((User + QLatin1Char('|') + Suffix).toUtf8(), QCryptographicHash::Sha256);
  return QStringLiteral("linscp-") + QString::fromLatin1(Hash.toHex().left(16));
}

bool Send(const QStringList & Arguments, const QString & Suffix, QString * Error)
{
  const auto Fail = [&Error](const QLocalSocket & Socket, const char * What)
  {
    if (Error != nullptr)
    {
      *Error = QString::fromLatin1(What) + QStringLiteral(": ") + Socket.errorString();
    }
    return false;
  };
  QLocalSocket Socket;
  Socket.connectToServer(ServerName(Suffix));
  if (!Socket.waitForConnected(Timeout))
  {
    return Fail(Socket, "connect");
  }
  QByteArray Message = QJsonDocument(QJsonArray::fromStringList(Arguments)).toJson(QJsonDocument::Compact);
  Message.append('\n');
  // Written at once, more often than not; then there is nothing to wait
  // for, and Windows' pipes say so as an error.
  if ((Socket.write(Message) != Message.size()) ||
      ((Socket.bytesToWrite() > 0) && !Socket.waitForBytesWritten(Timeout) && (Socket.bytesToWrite() > 0)))
  {
    return Fail(Socket, "write");
  }
  while (Socket.bytesAvailable() < 1)
  {
    if (!Socket.waitForReadyRead(Timeout))
    {
      return Fail(Socket, "answer");
    }
  }
  if (Socket.read(1) != "1")
  {
    if (Error != nullptr)
    {
      *Error = QStringLiteral("refused");
    }
    return false;
  }
  return true;
}

bool AnyOtherInstance()
{
  const QString Self = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
#if defined(Q_OS_WIN)
  // As AnyOtherInstanceOfSelf: a process of the same executable's name.
  bool Result = false;
  const QString Name = QFileInfo(Self).fileName();
  HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (Snapshot != INVALID_HANDLE_VALUE)
  {
    PROCESSENTRY32W Entry;
    Entry.dwSize = sizeof(Entry);
    for (BOOL More = Process32FirstW(Snapshot, &Entry); More && !Result; More = Process32NextW(Snapshot, &Entry))
    {
      Result = (Entry.th32ProcessID != GetCurrentProcessId()) &&
        (QString::fromWCharArray(Entry.szExeFile).compare(Name, Qt::CaseInsensitive) == 0);
    }
    CloseHandle(Snapshot);
  }
  return Result;
#else
  // The user's processes running this executable.
  const QString Own = QString::number(QCoreApplication::applicationPid());
  for (const QString & Pid : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
  {
    bool Number = false;
    Pid.toLongLong(&Number);
    if (Number && (Pid != Own) &&
        (QFileInfo(QStringLiteral("/proc/%1").arg(Pid)).ownerId() == static_cast<uint>(getuid())) &&
        (QFileInfo(QStringLiteral("/proc/%1/exe").arg(Pid)).canonicalFilePath() == Self))
    {
      return true;
    }
  }
  return false;
#endif
}

}  // namespace SingleInstance

InstanceServer::InstanceServer(std::function<bool(const QStringList &)> Handler, const QString & Suffix, QObject * Parent) :
  QObject(Parent),
  FHandler(std::move(Handler))
{
  FServer = new QLocalServer(this);
  FServer->setSocketOptions(QLocalServer::UserAccessOption);
  const QString Name = SingleInstance::ServerName(Suffix);
  if (!FServer->listen(Name))
  {
    // Another instance's, alive: it stays the one that takes command lines.
    QLocalSocket Probe;
    Probe.connectToServer(Name);
    if (Probe.waitForConnected(1000))
    {
      Probe.disconnectFromServer();
    }
    else
    {
      // A socket left by an instance that did not end cleanly (POSIX).
      QLocalServer::removeServer(Name);
      FServer->listen(Name);
    }
  }
  connect(FServer, &QLocalServer::newConnection, this, &InstanceServer::Accept);
}

bool InstanceServer::Listening() const
{
  return FServer->isListening();
}

void InstanceServer::Accept()
{
  while (QLocalSocket * Socket = FServer->nextPendingConnection())
  {
    connect(Socket, &QLocalSocket::disconnected, Socket, &QObject::deleteLater);
    connect(Socket, &QLocalSocket::readyRead, this, [this, Socket]()
    {
      if (!Socket->canReadLine())
      {
        return;
      }
      const QJsonDocument Document = QJsonDocument::fromJson(Socket->readLine());
      QStringList Arguments;
      for (const QJsonValue & Value : Document.array())
      {
        Arguments.append(Value.toString());
      }
      const bool Taken = !Arguments.isEmpty() && FHandler && FHandler(Arguments);
      Socket->write(Taken ? "1" : "0");
      Socket->flush();
      Socket->disconnectFromServer();
    });
  }
}
