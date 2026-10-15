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

struct TDrag::TImpl
{
#if defined(Q_OS_WIN)
  HANDLE Mapping = nullptr;
#endif
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

// As DDGetTarget: the copy hook's answer, the folder the fake directory was
// to be copied into.
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
