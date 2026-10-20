//---------------------------------------------------------------------------
// CFile for linscp's MFC-compatible layer (see include/afx.h).
//
// Original code, written for linscp (GPL-3.0-or-later). The Win32 file API:
// the system's on Windows, the platform layer's on POSIX. Failures raise
// EOSError through the shim's RaiseLastOSError, as core reports OS errors.
//---------------------------------------------------------------------------
#include "afx.h"

#include <atomic>

#include "vcl.h"

const HANDLE CFile::hFileNull = INVALID_HANDLE_VALUE;

namespace {

std::atomic<unsigned long> DestructorFailures(0);

DWORD AccessOf(unsigned int Flags)
{
  if ((Flags & CFile::modeReadWrite) != 0)
  {
    return GENERIC_READ | GENERIC_WRITE;
  }
  if ((Flags & CFile::modeWrite) != 0)
  {
    return GENERIC_WRITE;
  }
  return GENERIC_READ;
}

DWORD ShareOf(unsigned int Flags)
{
  DWORD Share = 0;
  // shareDenyWrite = others may read; shareDenyRead = others may write;
  // shareDenyNone = both; shareExclusive = neither.
  if ((Flags & CFile::shareDenyWrite) != 0)
  {
    Share |= FILE_SHARE_READ;
  }
  if ((Flags & CFile::shareDenyRead) != 0)
  {
    Share |= FILE_SHARE_WRITE;
  }
  return Share;
}

DWORD DispositionOf(unsigned int Flags)
{
  if ((Flags & CFile::modeCreate) != 0)
  {
    return ((Flags & CFile::modeNoTruncate) != 0) ? OPEN_ALWAYS : CREATE_ALWAYS;
  }
  return OPEN_EXISTING;
}

DWORD AttributesOf(unsigned int Flags)
{
  DWORD Result = FILE_ATTRIBUTE_NORMAL;
#ifdef FILE_FLAG_NO_BUFFERING
  if ((Flags & CFile::osNoBuffer) != 0)
  {
    Result |= FILE_FLAG_NO_BUFFERING;
  }
#endif
#ifdef FILE_FLAG_WRITE_THROUGH
  if ((Flags & CFile::osWriteThrough) != 0)
  {
    Result |= FILE_FLAG_WRITE_THROUGH;
  }
#endif
#ifdef FILE_FLAG_RANDOM_ACCESS
  if ((Flags & CFile::osRandomAccess) != 0)
  {
    Result |= FILE_FLAG_RANDOM_ACCESS;
  }
#endif
#ifdef FILE_FLAG_SEQUENTIAL_SCAN
  if ((Flags & CFile::osSequentialScan) != 0)
  {
    Result |= FILE_FLAG_SEQUENTIAL_SCAN;
  }
#endif
  (void)Flags;
  return Result;
}

}  // namespace

CFile::CFile() : m_hFile(INVALID_HANDLE_VALUE)
{
}

CFile::CFile(HANDLE File) : m_hFile(File)
{
}

CFile::CFile(const wchar_t * FileName, unsigned int OpenFlags) : m_hFile(INVALID_HANDLE_VALUE)
{
  if (!Open(FileName, OpenFlags))
  {
    RaiseError(GetLastError());
  }
}

CFile::~CFile()
{
  if (m_hFile != INVALID_HANDLE_VALUE)
  {
    const HANDLE File = m_hFile;
    m_hFile = INVALID_HANDLE_VALUE;
    if (!CloseHandle(File))
    {
      // A destructor must not raise; the failure is recorded instead.
      const DWORD Code = GetLastError();
      ++DestructorFailures;
#if defined(_WIN32)
      const UnicodeString Message =
        UnicodeString(L"linscp: CFile could not close") + FErrorText + UnicodeString(L"\r\n") +
        SysErrorMessage(static_cast<int>(Code)) + UnicodeString(L"\r\n");
      OutputDebugStringW(Message.c_str());
#else
      (void)Code;
#endif
    }
  }
}

unsigned long CFile::DestructorCloseFailures()
{
  return DestructorFailures.load();
}

void CFile::RaiseError(DWORD Code) const
{
  RaiseLastOSError(static_cast<int>(Code), FErrorText);
}

BOOL CFile::Open(const wchar_t * FileName, unsigned int OpenFlags)
{
  Abort();
  FErrorText = UnicodeString(L"\r\n") +
    ExpandFileName((FileName != nullptr) ? UnicodeString(FileName) : UnicodeString());
  if (FileName == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  // Documented default: child processes may inherit the handle unless
  // modeNoInherit is given. (The POSIX layer opens with O_CLOEXEC regardless.)
  SECURITY_ATTRIBUTES Security;
  Security.nLength = sizeof(Security);
  Security.lpSecurityDescriptor = nullptr;
  Security.bInheritHandle = ((OpenFlags & modeNoInherit) == 0) ? TRUE : FALSE;
  const HANDLE File = CreateFile(FileName, AccessOf(OpenFlags), ShareOf(OpenFlags), &Security,
                                 DispositionOf(OpenFlags), AttributesOf(OpenFlags), nullptr);
  if (File == INVALID_HANDLE_VALUE)
  {
    // CreateFile's last error stays for the caller.
    return FALSE;
  }
  m_hFile = File;
  return TRUE;
}

unsigned int CFile::Read(void * Buffer, unsigned int Count)
{
  if (Count == 0)
  {
    return 0;
  }
  DWORD Done = 0;
  if (!ReadFile(m_hFile, Buffer, Count, &Done, nullptr))
  {
    RaiseError(GetLastError());
  }
  return Done;
}

void CFile::Write(const void * Buffer, unsigned int Count)
{
  if (Count == 0)
  {
    return;
  }
  DWORD Done = 0;
  if (!WriteFile(m_hFile, Buffer, Count, &Done, nullptr))
  {
    RaiseError(GetLastError());
  }
  if (Done != Count)
  {
    RaiseError(ERROR_DISK_FULL);
  }
}

void CFile::Flush()
{
  if (m_hFile == INVALID_HANDLE_VALUE)
  {
    return;
  }
  if (!FlushFileBuffers(m_hFile))
  {
    RaiseError(GetLastError());
  }
}

ULONGLONG CFile::Seek(LONGLONG Offset, unsigned int From)
{
  const DWORD Method = (From == end) ? FILE_END : ((From == current) ? FILE_CURRENT : FILE_BEGIN);
  LONG High = static_cast<LONG>(Offset >> 32);
  SetLastError(NO_ERROR);
  const DWORD Low = SetFilePointer(m_hFile, static_cast<LONG>(Offset & 0xFFFFFFFF), &High, Method);
  const DWORD SeekError = GetLastError();
  if ((Low == INVALID_SET_FILE_POINTER) && (SeekError != NO_ERROR))
  {
    RaiseError(SeekError);
  }
  return (static_cast<ULONGLONG>(static_cast<DWORD>(High)) << 32) | Low;
}

ULONGLONG CFile::GetPosition() const
{
  LONG High = 0;
  SetLastError(NO_ERROR);
  const DWORD Low = SetFilePointer(m_hFile, 0, &High, FILE_CURRENT);
  const DWORD SeekError = GetLastError();
  if ((Low == INVALID_SET_FILE_POINTER) && (SeekError != NO_ERROR))
  {
    RaiseError(SeekError);
  }
  return (static_cast<ULONGLONG>(static_cast<DWORD>(High)) << 32) | Low;
}

ULONGLONG CFile::GetLength() const
{
  DWORD High = 0;
  SetLastError(NO_ERROR);
  const DWORD Low = GetFileSize(m_hFile, &High);
  const DWORD SizeError = GetLastError();
  if ((Low == INVALID_FILE_SIZE) && (SizeError != NO_ERROR))
  {
    RaiseError(SizeError);
  }
  return (static_cast<ULONGLONG>(High) << 32) | Low;
}

void CFile::Close()
{
  if (m_hFile == INVALID_HANDLE_VALUE)
  {
    FErrorText = UnicodeString();
    return;
  }
  const HANDLE File = m_hFile;
  m_hFile = INVALID_HANDLE_VALUE;
  const UnicodeString Text = FErrorText;
  FErrorText = UnicodeString();
  if (!CloseHandle(File))
  {
    RaiseLastOSError(static_cast<int>(GetLastError()), Text);
  }
}

void CFile::Abort()
{
  if (m_hFile != INVALID_HANDLE_VALUE)
  {
    CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
  }
}
