#include "vcl_streams.h"

#include <algorithm>
#include <cstring>

#include "vcl_strutils.h"

#if defined(_WIN32)
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

// --- TStream ---------------------------------------------------------------

int TStream::Seek(int Offset, Word Origin)
{
  // The legacy overload's origins share values with TSeekOrigin.
  return static_cast<int>(
    Seek(static_cast<__int64>(Offset), static_cast<TSeekOrigin>(Origin)));
}

void TStream::ReadBuffer(void * Buffer, int Count)
{
  if (Count != 0 && Read(Buffer, Count) != Count)
  {
    // Delphi raises here rather than reporting a short read, and core relies on
    // that: treating a truncated transfer as success would corrupt data.
    throw EReadError(UnicodeString(L"Stream read error"));
  }
}

void TStream::WriteBuffer(const void * Buffer, int Count)
{
  if (Count != 0 && Write(Buffer, Count) != Count)
  {
    throw EWriteError(UnicodeString(L"Stream write error"));
  }
}

__int64 TStream::GetPosition()
{
  return Seek(static_cast<__int64>(0), soCurrent);
}

void TStream::SetPosition(__int64 Pos)
{
  Seek(Pos, soBeginning);
}

__int64 TStream::GetSize()
{
  const __int64 Pos = GetPosition();
  const __int64 Result = Seek(static_cast<__int64>(0), soEnd);
  Seek(Pos, soBeginning);
  return Result;
}

void TStream::SetSize(const __int64 NewSize)
{
  // Delphi's base implementation does nothing; descendants that can resize
  // override it.
  (void)NewSize;
}

__int64 TStream::CopyFrom(TStream * Source, __int64 Count)
{
  if (Source == nullptr)
  {
    return 0;
  }
  // Count of 0 means "all of it, from the start", which is Delphi's rule.
  if (Count == 0)
  {
    Source->SetPosition(0);
    Count = Source->GetSize();
  }
  constexpr int ChunkSize = 64 * 1024;
  std::vector<unsigned char> Buffer(
    static_cast<std::size_t>(std::min<__int64>(Count, ChunkSize)));
  __int64 Remaining = Count;
  __int64 Copied = 0;
  while (Remaining > 0)
  {
    const int Want = static_cast<int>(
      std::min<__int64>(Remaining, static_cast<__int64>(Buffer.size())));
    const int Got = Source->Read(Buffer.data(), Want);
    if (Got <= 0)
    {
      break;         // source exhausted early
    }
    WriteBuffer(Buffer.data(), Got);
    Remaining -= Got;
    Copied += Got;
  }
  return Copied;
}

// --- THandleStream ---------------------------------------------------------

namespace {

#if !defined(_WIN32)
int FdOf(THandle H)
{
  return static_cast<int>(static_cast<std::intptr_t>(H));
}
#endif

}  // namespace

int THandleStream::Read(void * Buffer, int Count)
{
  if (FHandle == InvalidHandleValue || Count <= 0)
  {
    return 0;
  }
#if defined(_WIN32)
  DWORD Got = 0;
  if (ReadFile(reinterpret_cast<HANDLE>(FHandle), Buffer,
               static_cast<DWORD>(Count), &Got, nullptr) == 0)
  {
    return 0;
  }
  return static_cast<int>(Got);
#else
  const ssize_t Got = ::read(FdOf(FHandle), Buffer,
                             static_cast<std::size_t>(Count));
  return Got < 0 ? 0 : static_cast<int>(Got);
#endif
}

int THandleStream::Write(const void * Buffer, int Count)
{
  if (FHandle == InvalidHandleValue || Count <= 0)
  {
    return 0;
  }
#if defined(_WIN32)
  DWORD Put = 0;
  if (WriteFile(reinterpret_cast<HANDLE>(FHandle), Buffer,
                static_cast<DWORD>(Count), &Put, nullptr) == 0)
  {
    return 0;
  }
  return static_cast<int>(Put);
#else
  const ssize_t Put = ::write(FdOf(FHandle), Buffer,
                              static_cast<std::size_t>(Count));
  return Put < 0 ? 0 : static_cast<int>(Put);
#endif
}

__int64 THandleStream::Seek(const __int64 Offset, TSeekOrigin Origin)
{
  if (FHandle == InvalidHandleValue)
  {
    return 0;
  }
#if defined(_WIN32)
  DWORD Method = FILE_BEGIN;
  if (Origin == soCurrent) { Method = FILE_CURRENT; }
  else if (Origin == soEnd) { Method = FILE_END; }
  LARGE_INTEGER Move;
  Move.QuadPart = Offset;
  LARGE_INTEGER New;
  New.QuadPart = 0;
  if (SetFilePointerEx(reinterpret_cast<HANDLE>(FHandle), Move, &New,
                       Method) == 0)
  {
    return 0;
  }
  return static_cast<__int64>(New.QuadPart);
#else
  int Whence = SEEK_SET;
  if (Origin == soCurrent) { Whence = SEEK_CUR; }
  else if (Origin == soEnd) { Whence = SEEK_END; }
  const off_t New = ::lseek(FdOf(FHandle), static_cast<off_t>(Offset), Whence);
  return New < 0 ? 0 : static_cast<__int64>(New);
#endif
}

void THandleStream::SetSize(const __int64 NewSize)
{
  if (FHandle == InvalidHandleValue)
  {
    return;
  }
#if defined(_WIN32)
  const __int64 Pos = GetPosition();
  SetPosition(NewSize);
  SetEndOfFile(reinterpret_cast<HANDLE>(FHandle));
  SetPosition(std::min(Pos, NewSize));
#else
  if (::ftruncate(FdOf(FHandle), static_cast<off_t>(NewSize)) != 0)
  {
    return;
  }
  if (GetPosition() > NewSize)
  {
    SetPosition(NewSize);
  }
#endif
}

// --- TFileStream -----------------------------------------------------------

TFileStream::TFileStream(const UnicodeString & FileName, Word Mode)
  : THandleStream(InvalidHandleValue), FFileName(FileName)
{
#if defined(_WIN32)
  DWORD Access = GENERIC_READ;
  DWORD Disposition = OPEN_EXISTING;
  if (Mode == fmCreate)
  {
    Access = GENERIC_READ | GENERIC_WRITE;
    Disposition = CREATE_ALWAYS;
  }
  else
  {
    const Word Open = Mode & 0x000F;
    if (Open == fmOpenWrite) { Access = GENERIC_WRITE; }
    else if (Open == fmOpenReadWrite) { Access = GENERIC_READ | GENERIC_WRITE; }
  }
  DWORD Share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  const Word ShareMode = Mode & 0x00F0;
  if (ShareMode == fmShareExclusive) { Share = 0; }
  else if (ShareMode == fmShareDenyWrite) { Share = FILE_SHARE_READ; }
  else if (ShareMode == fmShareDenyRead) { Share = FILE_SHARE_WRITE; }

  HANDLE H = CreateFileW(FileName.c_str(), Access, Share, nullptr, Disposition,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (H == INVALID_HANDLE_VALUE)
  {
    throw EFOpenError(UnicodeString(L"Cannot open ") + FileName);
  }
  FHandle = reinterpret_cast<THandle>(H);
#else
  int Flags = O_RDONLY;
  if (Mode == fmCreate)
  {
    Flags = O_RDWR | O_CREAT | O_TRUNC;
  }
  else
  {
    const Word Open = Mode & 0x000F;
    if (Open == fmOpenWrite) { Flags = O_WRONLY; }
    else if (Open == fmOpenReadWrite) { Flags = O_RDWR; }
  }
  // POSIX has no equivalent of Windows share modes; advisory locking would
  // change behaviour rather than match it, so the share bits are ignored.
  const int Fd = ::open(StringToUTF8(FileName).c_str(), Flags, 0666);
  if (Fd < 0)
  {
    throw EFOpenError(UnicodeString(L"Cannot open ") + FileName);
  }
  FHandle = static_cast<THandle>(static_cast<std::intptr_t>(Fd));
#endif
}

TFileStream::~TFileStream()
{
  if (FHandle != InvalidHandleValue)
  {
#if defined(_WIN32)
    CloseHandle(reinterpret_cast<HANDLE>(FHandle));
#else
    ::close(FdOf(FHandle));
#endif
    FHandle = InvalidHandleValue;
  }
}

// --- TMemoryStream ---------------------------------------------------------

int TMemoryStream::Read(void * Buffer, int Count)
{
  if (Count <= 0 || FPosition < 0 ||
      FPosition >= static_cast<__int64>(FData.size()))
  {
    return 0;
  }
  const __int64 Available = static_cast<__int64>(FData.size()) - FPosition;
  const int Got = static_cast<int>(std::min<__int64>(Count, Available));
  std::memcpy(Buffer, FData.data() + FPosition, static_cast<std::size_t>(Got));
  FPosition += Got;
  return Got;
}

int TMemoryStream::Write(const void * Buffer, int Count)
{
  if (Count <= 0 || FPosition < 0)
  {
    return 0;
  }
  const std::size_t End = static_cast<std::size_t>(FPosition) +
                          static_cast<std::size_t>(Count);
  if (End > FData.size())
  {
    FData.resize(End);     // writing past the end grows the buffer
  }
  std::memcpy(FData.data() + FPosition, Buffer,
              static_cast<std::size_t>(Count));
  FPosition += Count;
  return Count;
}

__int64 TMemoryStream::Seek(const __int64 Offset, TSeekOrigin Origin)
{
  __int64 Target = Offset;
  if (Origin == soCurrent)
  {
    Target = FPosition + Offset;
  }
  else if (Origin == soEnd)
  {
    Target = static_cast<__int64>(FData.size()) + Offset;
  }
  // Seeking past the end is allowed, as in Delphi; a later write grows the
  // buffer. Seeking before the start clamps to 0.
  FPosition = Target < 0 ? 0 : Target;
  return FPosition;
}

__int64 TMemoryStream::GetSize()
{
  return static_cast<__int64>(FData.size());
}

void TMemoryStream::SetSize(const __int64 NewSize)
{
  FData.resize(NewSize > 0 ? static_cast<std::size_t>(NewSize) : 0);
  if (FPosition > static_cast<__int64>(FData.size()))
  {
    FPosition = static_cast<__int64>(FData.size());
  }
}

void TMemoryStream::Clear()
{
  FData.clear();
  FPosition = 0;
}

void TMemoryStream::SaveToStream(TStream * Stream)
{
  if (Stream != nullptr && !FData.empty())
  {
    Stream->WriteBuffer(FData.data(), static_cast<int>(FData.size()));
  }
}

void TMemoryStream::LoadFromStream(TStream * Stream)
{
  Clear();
  if (Stream == nullptr)
  {
    return;
  }
  Stream->SetPosition(0);
  const __int64 Count = Stream->GetSize();
  if (Count <= 0)
  {
    return;
  }
  FData.resize(static_cast<std::size_t>(Count));
  Stream->ReadBuffer(FData.data(), static_cast<int>(Count));
}
