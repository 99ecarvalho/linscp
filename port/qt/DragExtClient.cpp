#include "DragExtClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QThread>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <shlobj.h>
#include "DragExt.h"
#endif

namespace DragExt
{

#if defined(Q_OS_WIN)
namespace
{

// The program's mutex guarding the mapping, which the extension opens
// (TCustomScpExplorerForm's FDDExtMutex).
HANDLE ProgramMutex()
{
  static HANDLE Mutex = CreateMutexW(nullptr, FALSE, DRAG_EXT_MUTEX);
  return Mutex;
}

}  // namespace
#endif

bool Installed()
{
#if defined(Q_OS_WIN)
  // As GetDDExtInstalled: the class made, and let go.
  const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IUnknown * Unknown = nullptr;
  const bool Result =
    SUCCEEDED(CoCreateInstance(CLSID_ShellExtension, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IUnknown,
      reinterpret_cast<void **>(&Unknown)));
  if (Unknown != nullptr)
  {
    Unknown->Release();
  }
  CoFreeUnusedLibraries();
  if (SUCCEEDED(Init))
  {
    CoUninitialize();
  }
  return Result;
#else
  return false;
#endif
}

bool Running()
{
#if defined(Q_OS_WIN)
  if (!Installed())
  {
    return false;
  }
  HANDLE Mutex = OpenMutexW(SYNCHRONIZE, FALSE, DRAG_EXT_RUNNING_MUTEX);
  if (Mutex == nullptr)
  {
    return false;
  }
  CloseHandle(Mutex);
  return true;
#else
  return false;
#endif
}

bool Broken()
{
#if defined(Q_OS_WIN)
  // As IsDDExtBroken: Windows 10 1803 (builds 17134 to 17762).
  typedef LONG (WINAPI * TRtlGetVersion)(OSVERSIONINFOW *);
  TRtlGetVersion GetVersion = reinterpret_cast<TRtlGetVersion>(
    reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
  OSVERSIONINFOW Info = {};
  Info.dwOSVersionInfoSize = sizeof(Info);
  if ((GetVersion == nullptr) || (GetVersion(&Info) != 0))
  {
    return false;
  }
  return (Info.dwBuildNumber >= 17134) && (Info.dwBuildNumber < 17763);
#else
  return false;
#endif
}

QStringList MonitorRoots(const std::vector<std::string> & DDDrives)
{
  QStringList Result;
#if defined(Q_OS_WIN)
  QString Excluded;
  for (const std::string & Entry : DDDrives)
  {
    const QString Drive = QString::fromStdString(Entry).trimmed();
    if (Drive.startsWith(QLatin1Char('-')))
    {
      const QString Letter = Drive.mid(1).trimmed();
      if (!Letter.isEmpty())
      {
        Excluded += Letter.at(0).toUpper();
      }
    }
    else if (!Drive.isEmpty() && Drive.at(0).isLetter())
    {
      Result.append(QString(Drive.at(0).toUpper()) + QStringLiteral(":\\"));
    }
  }
  const DWORD Drives = GetLogicalDrives();
  for (int Index = 0; Index < 26; Index++)
  {
    const QChar Letter(static_cast<char16_t>(u'A' + Index));
    if (((Drives & (1u << Index)) == 0) || Excluded.contains(Letter))
    {
      continue;
    }
    const QString Root = QString(Letter) + QStringLiteral(":\\");
    const UINT Type = GetDriveTypeW(reinterpret_cast<const wchar_t *>(Root.utf16()));
    // As its drive checks: ready, not a CD, removable only from C: on.
    if ((Type == DRIVE_FIXED) || (Type == DRIVE_RAMDISK) || ((Type == DRIVE_REMOVABLE) && (Index >= 2)))
    {
      if (!Result.contains(Root))
      {
        Result.append(Root);
      }
    }
  }
  for (const QChar & Letter : Excluded)
  {
    Result.removeAll(QString(Letter) + QStringLiteral(":\\"));
  }
#else
  Q_UNUSED(DDDrives);
#endif
  return Result;
}

struct TCreationMonitor::TImpl
{
#if defined(Q_OS_WIN)
  struct TWatch
  {
    QString Root;
    HANDLE Directory = INVALID_HANDLE_VALUE;
    OVERLAPPED Overlapped = {};
    alignas(DWORD) char Buffer[32 * 1024];
  };
  std::vector<std::unique_ptr<TWatch>> Watches;

  static bool Read(TWatch & Watch)
  {
    ResetEvent(Watch.Overlapped.hEvent);
    return ReadDirectoryChangesW(Watch.Directory, Watch.Buffer, sizeof(Watch.Buffer), TRUE, FILE_NOTIFY_CHANGE_DIR_NAME,
      nullptr, &Watch.Overlapped, nullptr);
  }
#endif
};

TCreationMonitor::TCreationMonitor(const QStringList & Roots) : F(new TImpl())
{
#if defined(Q_OS_WIN)
  for (const QString & Root : Roots)
  {
    std::unique_ptr<TImpl::TWatch> Watch(new TImpl::TWatch());
    Watch->Root = QDir::toNativeSeparators(Root);
    if (!Watch->Root.endsWith(QLatin1Char('\\')))
    {
      Watch->Root += QLatin1Char('\\');
    }
    Watch->Directory = CreateFileW(reinterpret_cast<const wchar_t *>(Watch->Root.utf16()), FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    // Not-ready drives ignored, as upstream's.
    if (Watch->Directory == INVALID_HANDLE_VALUE)
    {
      continue;
    }
    Watch->Overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (TImpl::Read(*Watch))
    {
      F->Watches.push_back(std::move(Watch));
    }
    else
    {
      CloseHandle(Watch->Overlapped.hEvent);
      CloseHandle(Watch->Directory);
    }
  }
#else
  Q_UNUSED(Roots);
#endif
}

TCreationMonitor::~TCreationMonitor()
{
#if defined(Q_OS_WIN)
  for (const std::unique_ptr<TImpl::TWatch> & Watch : F->Watches)
  {
    CancelIo(Watch->Directory);
    DWORD Bytes = 0;
    GetOverlappedResult(Watch->Directory, &Watch->Overlapped, &Bytes, TRUE);
    CloseHandle(Watch->Overlapped.hEvent);
    CloseHandle(Watch->Directory);
  }
#endif
}

int TCreationMonitor::Count() const
{
#if defined(Q_OS_WIN)
  return static_cast<int>(F->Watches.size());
#else
  return 0;
#endif
}

QString TCreationMonitor::Found(const QString & Name)
{
  QString Result;
#if defined(Q_OS_WIN)
  for (const std::unique_ptr<TImpl::TWatch> & Watch : F->Watches)
  {
    if (WaitForSingleObject(Watch->Overlapped.hEvent, 0) != WAIT_OBJECT_0)
    {
      continue;
    }
    DWORD Bytes = 0;
    if (GetOverlappedResult(Watch->Directory, &Watch->Overlapped, &Bytes, FALSE) && (Bytes > 0))
    {
      const char * At = Watch->Buffer;
      while (true)
      {
        const FILE_NOTIFY_INFORMATION * Info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(At);
        const QString Relative = QString::fromWCharArray(Info->FileName, static_cast<int>(Info->FileNameLength / sizeof(WCHAR)));
        if (((Info->Action == FILE_ACTION_ADDED) || (Info->Action == FILE_ACTION_RENAMED_NEW_NAME)) && Result.isEmpty() &&
            (QFileInfo(QDir::fromNativeSeparators(Relative)).fileName().compare(Name, Qt::CaseInsensitive) == 0))
        {
          Result = Watch->Root + Relative;
        }
        if (Info->NextEntryOffset == 0)
        {
          break;
        }
        At += Info->NextEntryOffset;
      }
    }
    TImpl::Read(*Watch);
  }
#else
  Q_UNUSED(Name);
#endif
  return Result;
}

struct TDrag::TImpl
{
#if defined(Q_OS_WIN)
  HANDLE Mapping = nullptr;
#endif
  QString FakeName;
  std::unique_ptr<TCreationMonitor> Monitor;
};

TDrag::TDrag() : F(new TImpl())
{
}

TDrag::~TDrag()
{
#if defined(Q_OS_WIN)
  if (F->Mapping != nullptr)
  {
    CloseHandle(F->Mapping);
  }
#endif
}

// As DDFakeFileInitDrag: the mapping made, dragging, the fake directory the
// drop destination to recognize.
bool TDrag::Begin(const QString & FakeDirectory)
{
  F->FakeName = QFileInfo(FakeDirectory).fileName();
#if defined(Q_OS_WIN)
  SetFileAttributesW(QDir::toNativeSeparators(FakeDirectory).toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
  F->Mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(TDragExtCommStruct), DRAG_EXT_MAPPING);
  if (F->Mapping == nullptr)
  {
    return false;
  }
  HANDLE Mutex = ProgramMutex();
  WaitForSingleObject(Mutex, 1000);
  TDragExtCommStruct * CommStruct = static_cast<TDragExtCommStruct *>(MapViewOfFile(F->Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
  if (CommStruct != nullptr)
  {
    CommStruct->Version = TDragExtCommStruct::CurrentVersion;
    CommStruct->Dragging = true;
    const std::wstring Path = QDir::toNativeSeparators(FakeDirectory).toStdWString();
    wcsncpy(CommStruct->DropDest, Path.c_str(), MAX_PATH - 1);
    CommStruct->DropDest[MAX_PATH - 1] = L'\0';
    UnmapViewOfFile(CommStruct);
  }
  ReleaseMutex(Mutex);
  return (CommStruct != nullptr);
#else
  Q_UNUSED(FakeDirectory);
  return false;
#endif
}

void TDrag::Monitor(const QStringList & Roots)
{
  F->Monitor.reset(new TCreationMonitor(Roots));
}

// As DDGetTarget: the monitors' find (DDFakeCreated: the copy removed,
// its folder the target), else the copy hook's answer, the folder the fake
// directory was to be copied into.
QString TDrag::Target(int Timeout)
{
#if defined(Q_OS_WIN)
  if (F->Mapping == nullptr)
  {
    return QString();
  }
  QElapsedTimer Waited;
  Waited.start();
  while (true)
  {
    QString Result;
    bool Done = false;
    if (F->Monitor)
    {
      const QString Copy = F->Monitor->Found(F->FakeName);
      if (!Copy.isEmpty())
      {
        QDir().rmdir(Copy);
        return QDir::toNativeSeparators(QFileInfo(Copy).path());
      }
    }
    HANDLE Mutex = ProgramMutex();
    WaitForSingleObject(Mutex, 1000);
    TDragExtCommStruct * CommStruct = static_cast<TDragExtCommStruct *>(MapViewOfFile(F->Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (CommStruct != nullptr)
    {
      Done = !CommStruct->Dragging;
      if (Done)
      {
        // DropDest: the fake directory's path in its target folder.
        Result = QDir::toNativeSeparators(QFileInfo(QString::fromWCharArray(CommStruct->DropDest)).path());
      }
      UnmapViewOfFile(CommStruct);
    }
    ReleaseMutex(Mutex);
    if (Done || (Waited.elapsed() >= Timeout))
    {
      return Result;
    }
    QThread::msleep(50);
    QCoreApplication::processEvents();
  }
#else
  Q_UNUSED(Timeout);
  return QString();
#endif
}

}  // namespace DragExt
