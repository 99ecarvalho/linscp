// Win32 functions WinSCP's PuTTY calls from its Windows platform layer:
// formatting, clocks and resource usage, code pages, system messages, the
// narrow (A) file and module forms, directory search, pipes and processes,
// the narrow registry, and CryptoAPI's random numbers. See win32_on_posix.h.
#include "win32_on_posix.h"
#include "win32_kernel_internal.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <mutex>
#include <pwd.h>
#include <spawn.h>
#include <string>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char ** environ;

namespace {

int FdOf(HANDLE H)
{
  const auto V = reinterpret_cast<std::intptr_t>(H);
  return ((H == INVALID_HANDLE_VALUE) || (V < 0) || (static_cast<std::uintptr_t>(V) >= LinscpKernelHandleBase))
    ? -1 : static_cast<int>(V);
}

HANDLE HandleOf(int Fd)
{
  return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(Fd));
}

BOOL Fail()
{
  SetLastError(Win32ErrorFromErrno(errno));
  return FALSE;
}

// UTF-8 <-> UTF-16, for the narrow forms: on POSIX the file system's names
// are bytes, conventionally UTF-8, so a narrow name is taken as UTF-8.
std::u16string Widen(const char * S, std::size_t N, bool & Invalid)
{
  std::u16string R;
  Invalid = false;
  for (std::size_t I = 0; I < N;)
  {
    const unsigned char C = static_cast<unsigned char>(S[I]);
    char32_t CP;
    std::size_t Len;
    if (C < 0x80) { CP = C; Len = 1; }
    else if ((C >> 5) == 0x6) { CP = C & 0x1F; Len = 2; }
    else if ((C >> 4) == 0xE) { CP = C & 0x0F; Len = 3; }
    else if ((C >> 3) == 0x1E) { CP = C & 0x07; Len = 4; }
    else { CP = 0xFFFD; Len = 1; Invalid = true; }
    for (std::size_t K = 1; K < Len; ++K)
    {
      if ((I + K >= N) || ((static_cast<unsigned char>(S[I + K]) >> 6) != 0x2))
      {
        CP = 0xFFFD;
        Len = K;
        Invalid = true;
        break;
      }
      CP = (CP << 6) | (static_cast<unsigned char>(S[I + K]) & 0x3F);
    }
    // Overlong forms and surrogates are invalid UTF-8 too.
    if ((Len == 2 && CP < 0x80) || (Len == 3 && CP < 0x800) || (Len == 4 && (CP < 0x10000 || CP > 0x10FFFF)) ||
        (CP >= 0xD800 && CP < 0xE000))
    {
      CP = 0xFFFD;
      Invalid = true;
    }
    I += Len;
    if (CP >= 0x10000)
    {
      CP -= 0x10000;
      R += static_cast<char16_t>(0xD800 + (CP >> 10));
      R += static_cast<char16_t>(0xDC00 + (CP & 0x3FF));
    }
    else
    {
      R += static_cast<char16_t>(CP);
    }
  }
  return R;
}

std::u16string WidenZ(LPCSTR S)
{
  bool Invalid;
  return (S == nullptr) ? std::u16string() : Widen(S, std::strlen(S), Invalid);
}

const wchar_t * W(const std::u16string & S)
{
  return reinterpret_cast<const wchar_t *>(S.c_str());
}

// Copies a narrow result out, Win32 style: the length on success, or the
// size needed (terminator included) when the buffer is too small.
DWORD CopyNarrow(const std::string & S, LPSTR Dst, DWORD Size)
{
  if ((Dst == nullptr) || (Size <= S.size()))
  {
    return static_cast<DWORD>(S.size() + 1);
  }
  std::memcpy(Dst, S.c_str(), S.size() + 1);
  return static_cast<DWORD>(S.size());
}

FILETIME FromTimeval(const timeval & T)
{
  return TicksToFileTime(static_cast<std::uint64_t>(T.tv_sec) * FileTimeTicksPerSecond +
                         static_cast<std::uint64_t>(T.tv_usec) * 10);
}

}  // namespace

// --- formatting, clocks, resources ------------------------------------------

int _vsnprintf(char * Buffer, size_t Count, const char * Format, va_list Args)
{
  const int N = std::vsnprintf(Buffer, Count, Format, Args);
  return ((N < 0) || (static_cast<size_t>(N) >= Count)) ? -1 : N;
}

int _snprintf(char * Buffer, size_t Count, const char * Format, ...)
{
  va_list Args;
  va_start(Args, Format);
  const int N = _vsnprintf(Buffer, Count, Format, Args);
  va_end(Args);
  return N;
}

BOOL QueryPerformanceCounter(LARGE_INTEGER * Count)
{
  timespec T{};
  clock_gettime(CLOCK_MONOTONIC, &T);
  Count->QuadPart = static_cast<LONGLONG>(T.tv_sec) * 1000000000LL + T.tv_nsec;
  return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER * Frequency)
{
  Frequency->QuadPart = 1000000000LL;
  return TRUE;
}

void GlobalMemoryStatus(MEMORYSTATUS * Status)
{
  struct sysinfo Info{};
  sysinfo(&Info);
  const SIZE_T Unit = Info.mem_unit ? Info.mem_unit : 1;
  Status->dwLength = sizeof(*Status);
  Status->dwTotalPhys = Info.totalram * Unit;
  Status->dwAvailPhys = Info.freeram * Unit;
  Status->dwTotalPageFile = (Info.totalram + Info.totalswap) * Unit;
  Status->dwAvailPageFile = (Info.freeram + Info.freeswap) * Unit;
  Status->dwTotalVirtual = static_cast<SIZE_T>(1) << 47;
  Status->dwAvailVirtual = Status->dwTotalVirtual;
  Status->dwMemoryLoad = (Info.totalram == 0) ? 0 :
    static_cast<DWORD>(100 - (Info.freeram * 100) / Info.totalram);
}

HANDLE GetCurrentThread(void)
{
  return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-2));
}

BOOL GetProcessTimes(HANDLE, FILETIME * Creation, FILETIME * Exit, FILETIME * Kernel, FILETIME * User)
{
  rusage U{};
  getrusage(RUSAGE_SELF, &U);
  *Creation = FILETIME();
  *Exit = FILETIME();
  *Kernel = FromTimeval(U.ru_stime);
  *User = FromTimeval(U.ru_utime);
  return TRUE;
}

BOOL GetThreadTimes(HANDLE, FILETIME * Creation, FILETIME * Exit, FILETIME * Kernel, FILETIME * User)
{
  rusage U{};
  getrusage(RUSAGE_THREAD, &U);
  *Creation = FILETIME();
  *Exit = FILETIME();
  *Kernel = FromTimeval(U.ru_stime);
  *User = FromTimeval(U.ru_utime);
  return TRUE;
}

HWND GetForegroundWindow(void) { return nullptr; }
HWND GetCapture(void) { return nullptr; }
HWND GetClipboardOwner(void) { return nullptr; }
DWORD GetQueueStatus(UINT) { return 0; }

BOOL GetCursorPos(POINT * Point)
{
  if (Point != nullptr)
  {
    Point->x = 0;
    Point->y = 0;
  }
  SetLastError(ERROR_NOT_SUPPORTED);
  return FALSE;
}

// --- code pages -----------------------------------------------------------------

UINT GetACP(void) { return 28591; }
UINT GetOEMCP(void) { return 28591; }
BOOL IsDBCSLeadByteEx(UINT, BYTE) { return FALSE; }

namespace {

bool IsLatin1(UINT CodePage)
{
  return (CodePage == CP_ACP) || (CodePage == CP_OEMCP) || (CodePage == 28591);
}

}  // namespace

int MultiByteToWideChar(UINT CodePage, DWORD Flags, LPCSTR Src, int SrcLength, LPWSTR Dst, int DstLength)
{
  if ((Src == nullptr) || ((CodePage != CP_UTF8) && !IsLatin1(CodePage)))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  // -1: the string and its terminator, which is then converted too.
  const std::size_t N = (SrcLength < 0) ? std::strlen(Src) + 1 : static_cast<std::size_t>(SrcLength);
  std::u16string Out;
  if (CodePage == CP_UTF8)
  {
    bool Invalid = false;
    Out = Widen(Src, N, Invalid);
    if (Invalid && ((Flags & MB_ERR_INVALID_CHARS) != 0))
    {
      SetLastError(1113);   // ERROR_NO_UNICODE_TRANSLATION
      return 0;
    }
  }
  else
  {
    for (std::size_t I = 0; I < N; ++I)
    {
      Out += static_cast<char16_t>(static_cast<unsigned char>(Src[I]));
    }
  }
  if (DstLength == 0)
  {
    return static_cast<int>(Out.size());
  }
  if ((Dst == nullptr) || (static_cast<std::size_t>(DstLength) < Out.size()))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  for (std::size_t I = 0; I < Out.size(); ++I)
  {
    Dst[I] = static_cast<wchar_t>(Out[I]);
  }
  return static_cast<int>(Out.size());
}

int WideCharToMultiByte(UINT CodePage, DWORD, LPCWSTR Src, int SrcLength, LPSTR Dst, int DstLength,
  LPCSTR DefaultChar, BOOL * UsedDefaultChar)
{
  if ((Src == nullptr) || ((CodePage != CP_UTF8) && !IsLatin1(CodePage)))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return 0;
  }
  const std::size_t N = (SrcLength < 0) ? linscp_WideLength(Src) + 1 : static_cast<std::size_t>(SrcLength);
  std::string Out;
  if (UsedDefaultChar != nullptr)
  {
    *UsedDefaultChar = FALSE;
  }
  if (CodePage == CP_UTF8)
  {
    // Up to and including the terminator when it is in range.
    std::u16string In(reinterpret_cast<const char16_t *>(Src), N);
    std::string Converted;
    std::size_t Start = 0;
    for (std::size_t I = 0; I <= In.size(); ++I)
    {
      if ((I == In.size()) || (In[I] == 0))
      {
        std::u16string Part = In.substr(Start, I - Start);
        Converted += linscp::WideToUtf8(reinterpret_cast<const wchar_t *>(Part.c_str()));
        if (I < In.size())
        {
          Converted += '\0';
        }
        Start = I + 1;
      }
    }
    Out = Converted;
  }
  else
  {
    const char Default = ((DefaultChar != nullptr) && (*DefaultChar != 0)) ? *DefaultChar : '?';
    for (std::size_t I = 0; I < N; ++I)
    {
      const unsigned C = static_cast<unsigned short>(Src[I]);
      if (C <= 0xFF)
      {
        Out += static_cast<char>(C);
      }
      else
      {
        Out += Default;
        if (UsedDefaultChar != nullptr)
        {
          *UsedDefaultChar = TRUE;
        }
      }
    }
  }
  if (DstLength == 0)
  {
    return static_cast<int>(Out.size());
  }
  if ((Dst == nullptr) || (static_cast<std::size_t>(DstLength) < Out.size()))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  std::memcpy(Dst, Out.data(), Out.size());
  return static_cast<int>(Out.size());
}

// --- system messages ------------------------------------------------------------

const char * linscp_SystemMessage(DWORD Code)
{
  // Windows' own English wording, for the codes the platform layer produces
  // and the Winsock and certificate codes core and PuTTY report.
  struct TMessage { DWORD Code; const char * Text; };
  static const TMessage Messages[] = {
    {0, "The operation completed successfully."},
    {2, "The system cannot find the file specified."},
    {3, "The system cannot find the path specified."},
    {5, "Access is denied."},
    {6, "The handle is invalid."},
    {8, "Not enough memory resources are available to process this command."},
    {18, "There are no more files."},
    {32, "The process cannot access the file because it is being used by another process."},
    {38, "Reached the end of the file."},
    {39, "The disk is full."},
    {50, "The request is not supported."},
    {80, "The file exists."},
    {87, "The parameter is incorrect."},
    {109, "The pipe has been ended."},
    {112, "There is not enough space on the disk."},
    {122, "The data area passed to a system call is too small."},
    {126, "The specified module could not be found."},
    {127, "The specified procedure could not be found."},
    {183, "Cannot create a file when that file already exists."},
    {203, "The system could not find the environment option that was entered."},
    {234, "More data is available."},
    {259, "No more data is available."},
    {997, "Overlapped I/O operation is in progress."},
    {1113, "No mapping for the Unicode character exists in the target multi-byte code page."},
    {1223, "The operation was canceled by the user."},
    {1332, "No mapping between account names and security IDs was done."},
    {1812, "The specified image file did not contain a resource section."},
    {1813, "The specified resource type cannot be found in the image file."},
    {4390, "The file or directory is not a reparse point."},
    {10004, "A blocking operation was interrupted by a call to WSACancelBlockingCall."},
    {10009, "The file handle supplied is not valid."},
    {10013, "An attempt was made to access a socket in a way forbidden by its access permissions."},
    {10014, "The system detected an invalid pointer address in attempting to use a pointer argument in a call."},
    {10022, "An invalid argument was supplied."},
    {10024, "Too many open sockets."},
    {10035, "A non-blocking socket operation could not be completed immediately."},
    {10036, "A blocking operation is currently executing."},
    {10037, "An operation was attempted on a non-blocking socket that already had an operation in progress."},
    {10038, "An operation was attempted on something that is not a socket."},
    {10039, "A required address was omitted from an operation on a socket."},
    {10040, "A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself."},
    {10041, "A protocol was specified in the socket function call that does not support the semantics of the socket type requested."},
    {10042, "An unknown, invalid, or unsupported option or level was specified in a getsockopt or setsockopt call."},
    {10043, "The requested protocol has not been configured into the system, or no implementation for it exists."},
    {10044, "The support for the specified socket type does not exist in this address family."},
    {10045, "The attempted operation is not supported for the type of object referenced."},
    {10046, "The protocol family has not been configured into the system or no implementation for it exists."},
    {10047, "An address incompatible with the requested protocol was used."},
    {10048, "Only one usage of each socket address (protocol/network address/port) is normally permitted."},
    {10049, "The requested address is not valid in its context."},
    {10050, "A socket operation encountered a dead network."},
    {10051, "A socket operation was attempted to an unreachable network."},
    {10052, "The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress."},
    {10053, "An established connection was aborted by the software in your host machine."},
    {10054, "An existing connection was forcibly closed by the remote host."},
    {10055, "An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full."},
    {10056, "A connect request was made on an already connected socket."},
    {10057, "A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied."},
    {10058, "A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call."},
    {10060, "A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond."},
    {10061, "No connection could be made because the target machine actively refused it."},
    {10064, "A socket operation failed because the destination host was down."},
    {10065, "A socket operation was attempted to an unreachable host."},
    {10101, "Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence."},
    {11001, "No such host is known."},
    {11002, "This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server."},
    {11003, "A non-recoverable error occurred during a database lookup."},
    {11004, "The requested name is valid, but no data of the requested type was found."},
    {0x800B0101, "A required certificate is not within its validity period when verifying against the current system clock or the timestamp in the signed file."},
    {0x800B0109, "A certificate chain processed, but terminated in a root certificate which is not trusted by the trust provider."},
    {0x800B010A, "A certificate chain could not be built to a trusted root authority."},
    {0x800B0110, "The certificate is not valid for the requested usage."},
    {0x80092010, "The certificate is revoked."},
    {0x80096004, "The signature of the certificate cannot be verified."},
  };
  for (const TMessage & M : Messages)
  {
    if (M.Code == Code)
    {
      return M.Text;
    }
  }
  return nullptr;
}

namespace {

// The system message for FormatMessage: Windows ends one with CRLF unless a
// maximum line width is given, which also folds it onto one line.
bool SystemMessageText(DWORD Flags, DWORD Code, std::string & Text)
{
  if ((Flags & FORMAT_MESSAGE_FROM_SYSTEM) == 0)
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;
  }
  const char * M = linscp_SystemMessage(Code);
  if (M == nullptr)
  {
    SetLastError(317);   // ERROR_MR_MID_NOT_FOUND
    return false;
  }
  Text = M;
  if ((Flags & FORMAT_MESSAGE_MAX_WIDTH_MASK) == 0)
  {
    Text += "\r\n";
  }
  return true;
}

}  // namespace

DWORD FormatMessageA(DWORD Flags, LPCVOID, DWORD MessageId, DWORD, LPSTR Buffer, DWORD Size, va_list *)
{
  std::string Text;
  if (!SystemMessageText(Flags, MessageId, Text))
  {
    return 0;
  }
  if ((Flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0)
  {
    // Buffer is really an LPSTR *, filled with LocalAlloc'd (here malloc'd)
    // memory the caller frees with LocalFree.
    char * Out = static_cast<char *>(std::malloc(Text.size() + 1));
    std::memcpy(Out, Text.c_str(), Text.size() + 1);
    *reinterpret_cast<LPSTR *>(Buffer) = Out;
    return static_cast<DWORD>(Text.size());
  }
  if ((Buffer == nullptr) || (Size <= Text.size()))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  std::memcpy(Buffer, Text.c_str(), Text.size() + 1);
  return static_cast<DWORD>(Text.size());
}

DWORD FormatMessageW(DWORD Flags, LPCVOID, DWORD MessageId, DWORD, LPWSTR Buffer, DWORD Size, va_list *)
{
  std::string Text;
  if (!SystemMessageText(Flags, MessageId, Text))
  {
    return 0;
  }
  const std::u16string Wide = WidenZ(Text.c_str());
  wchar_t * Out = Buffer;
  if ((Flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0)
  {
    Out = static_cast<wchar_t *>(std::malloc((Wide.size() + 1) * sizeof(wchar_t)));
    *reinterpret_cast<LPWSTR *>(Buffer) = Out;
  }
  else if ((Buffer == nullptr) || (Size <= Wide.size()))
  {
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }
  std::memcpy(Out, Wide.c_str(), (Wide.size() + 1) * sizeof(wchar_t));
  return static_cast<DWORD>(Wide.size());
}

// --- environment, directories, modules (narrow) ------------------------------------

DWORD GetEnvironmentVariableA(LPCSTR Name, LPSTR Buffer, DWORD Size)
{
  const char * V = (Name != nullptr) ? getenv(Name) : nullptr;
  if (V == nullptr)
  {
    SetLastError(ERROR_ENVVAR_NOT_FOUND);
    return 0;
  }
  return CopyNarrow(V, Buffer, Size);
}

UINT GetSystemDirectoryA(LPSTR Buffer, UINT Size)
{
  return CopyNarrow("/usr/lib", Buffer, Size);
}

UINT GetWindowsDirectoryA(LPSTR Buffer, UINT Size)
{
  return CopyNarrow("/usr", Buffer, Size);
}

HMODULE LoadLibraryA(LPCSTR FileName)
{
  return LoadLibrary(W(WidenZ(FileName)));
}

HMODULE GetModuleHandleA(LPCSTR ModuleName)
{
  return GetModuleHandle(W(WidenZ(ModuleName)));
}

HRESULT SHGetFolderPathA(HWND Owner, int Folder, HANDLE Token, DWORD Flags, LPSTR Path)
{
  wchar_t Wide[MAX_PATH];
  const HRESULT R = SHGetFolderPath(Owner, Folder, Token, Flags, Wide);
  if (R == S_OK)
  {
    const std::string N = linscp::WideToUtf8(Wide);
    if (N.size() >= MAX_PATH)
    {
      return E_FAIL;
    }
    std::memcpy(Path, N.c_str(), N.size() + 1);
  }
  return R;
}

// --- files (narrow forms and the rest) --------------------------------------------

namespace {

// Writing to a pipe whose reader is gone raises SIGPIPE on POSIX; Win32
// returns ERROR_BROKEN_PIPE instead. The signal is blocked around the write
// and any one it left pending consumed, so the process is never killed.
ssize_t WriteNoSigpipe(int Fd, const void * Data, std::size_t Count)
{
  sigset_t Pipe, Old;
  sigemptyset(&Pipe);
  sigaddset(&Pipe, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &Pipe, &Old);
  const ssize_t N = write(Fd, Data, Count);
  const int Saved = errno;
  if ((N < 0) && (Saved == EPIPE))
  {
    const timespec Zero{0, 0};
    sigtimedwait(&Pipe, nullptr, &Zero);
  }
  pthread_sigmask(SIG_SETMASK, &Old, nullptr);
  errno = Saved;
  return N;
}

}  // namespace

BOOL ReadFile(HANDLE File, LPVOID Buffer, DWORD Count, LPDWORD Read, LPOVERLAPPED Overlapped)
{
  const int Fd = FdOf(File);
  if (Read != nullptr)
  {
    *Read = 0;
  }
  if (Fd < 0)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  ssize_t N;
  do
  {
    N = read(Fd, Buffer, Count);
  } while ((N < 0) && (errno == EINTR));
  if (N < 0)
  {
    return Fail();
  }
  if (Overlapped != nullptr)
  {
    Overlapped->Internal = 0;
    Overlapped->InternalHigh = static_cast<ULONG_PTR>(N);
  }
  if (Read != nullptr)
  {
    *Read = static_cast<DWORD>(N);
  }
  // A pipe's end of data, which Win32 reports as a broken pipe; a file's is
  // a successful read of nothing.
  struct stat St{};
  if ((N == 0) && (Count > 0) && (fstat(Fd, &St) == 0) && (S_ISFIFO(St.st_mode) || S_ISSOCK(St.st_mode)))
  {
    SetLastError(ERROR_BROKEN_PIPE);
    return FALSE;
  }
  return TRUE;
}

BOOL WriteFile(HANDLE File, LPCVOID Buffer, DWORD Count, LPDWORD Written, LPOVERLAPPED Overlapped)
{
  const int Fd = FdOf(File);
  if (Written != nullptr)
  {
    *Written = 0;
  }
  if (Fd < 0)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  // Win32 writes everything to a blocking handle before returning.
  DWORD Done = 0;
  while (Done < Count)
  {
    const ssize_t N = WriteNoSigpipe(Fd, static_cast<const char *>(Buffer) + Done, Count - Done);
    if (N < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      SetLastError((errno == EPIPE) ? ERROR_BROKEN_PIPE : Win32ErrorFromErrno(errno));
      if (Written != nullptr)
      {
        *Written = Done;
      }
      return FALSE;
    }
    Done += static_cast<DWORD>(N);
  }
  if (Overlapped != nullptr)
  {
    Overlapped->Internal = 0;
    Overlapped->InternalHigh = Done;
  }
  if (Written != nullptr)
  {
    *Written = Done;
  }
  return TRUE;
}

BOOL GetOverlappedResult(HANDLE, LPOVERLAPPED Overlapped, LPDWORD Transferred, BOOL)
{
  if (Overlapped == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  if (Transferred != nullptr)
  {
    *Transferred = static_cast<DWORD>(Overlapped->InternalHigh);
  }
  return TRUE;
}

HANDLE CreateFileA(LPCSTR FileName, DWORD DesiredAccess, DWORD ShareMode,
  LPSECURITY_ATTRIBUTES SecurityAttributes, DWORD CreationDisposition, DWORD FlagsAndAttributes,
  HANDLE TemplateFile)
{
  return CreateFile(W(WidenZ(FileName)), DesiredAccess, ShareMode, SecurityAttributes,
    CreationDisposition, FlagsAndAttributes, TemplateFile);
}

DWORD GetFileAttributesA(LPCSTR FileName)
{
  return GetFileAttributes(W(WidenZ(FileName)));
}

BOOL DeleteFileA(LPCSTR FileName)
{
  return (unlink(FileName) == 0) ? TRUE : Fail();
}

namespace {

void FillAttributeData(const struct stat & St, const char * Name, WIN32_FILE_ATTRIBUTE_DATA * D)
{
  D->dwFileAttributes = linscp::AttributesFromStat(St, Name);
  auto FromTs = [](const timespec & T)
  {
    return TicksToFileTime(static_cast<std::uint64_t>(T.tv_sec) * FileTimeTicksPerSecond +
                           static_cast<std::uint64_t>(T.tv_nsec) / 100 + FileTimeUnixEpochDelta);
  };
  D->ftLastWriteTime = FromTs(St.st_mtim);
  D->ftLastAccessTime = FromTs(St.st_atim);
  D->ftCreationTime = D->ftLastWriteTime;
  const auto Size = static_cast<std::uint64_t>(St.st_size);
  D->nFileSizeHigh = static_cast<DWORD>(Size >> 32);
  D->nFileSizeLow = static_cast<DWORD>(Size & 0xFFFFFFFFu);
}

BOOL AttributesEx(const std::string & Path, GET_FILEEX_INFO_LEVELS Level, LPVOID Info)
{
  if ((Level != GetFileExInfoStandard) || (Info == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  struct stat St{};
  if (stat(Path.c_str(), &St) != 0)
  {
    return Fail();
  }
  const std::size_t Slash = Path.rfind('/');
  FillAttributeData(St, Path.c_str() + ((Slash == std::string::npos) ? 0 : Slash + 1),
    static_cast<WIN32_FILE_ATTRIBUTE_DATA *>(Info));
  return TRUE;
}

}  // namespace

BOOL GetFileAttributesExW(LPCWSTR FileName, GET_FILEEX_INFO_LEVELS Level, LPVOID Info)
{
  return AttributesEx(linscp::WideToUtf8(FileName), Level, Info);
}

BOOL GetFileAttributesExA(LPCSTR FileName, GET_FILEEX_INFO_LEVELS Level, LPVOID Info)
{
  return AttributesEx(FileName, Level, Info);
}

// --- directory search ----------------------------------------------------------------

namespace {

struct TFind final : linscp::TKernelObject
{
  DIR * Dir = nullptr;
  std::string Directory;
  std::string Pattern;
  ~TFind() override { if (Dir != nullptr) { closedir(Dir); } }
  bool IsSignaled() const override { return false; }
};

// Windows' wildcards: * and ? as fnmatch's, except that "*.*" matches every
// name, dotted or not.
bool Matches(const std::string & Pattern, const char * Name)
{
  return (Pattern == "*.*") || (Pattern == "*") || (fnmatch(Pattern.c_str(), Name, 0) == 0);
}

// The next matching entry of a search, as attributes, times, size and name.
bool NextMatch(TFind & F, WIN32_FILE_ATTRIBUTE_DATA & Data, std::string & Name)
{
  while (dirent * E = readdir(F.Dir))
  {
    if (!Matches(F.Pattern, E->d_name))
    {
      continue;
    }
    struct stat St{};
    const std::string Full = F.Directory + "/" + E->d_name;
    if (lstat(Full.c_str(), &St) != 0)
    {
      continue;
    }
    FillAttributeData(St, E->d_name, &Data);
    Name = E->d_name;
    return true;
  }
  return false;
}

HANDLE StartFind(const std::string & Spec, WIN32_FILE_ATTRIBUTE_DATA & Data, std::string & Name)
{
  // Either separator: Windows code builds patterns with backslashes.
  const std::size_t Sep = Spec.find_last_of("/\\");
  auto F = std::make_shared<TFind>();
  F->Directory = (Sep == std::string::npos) ? std::string(".") : Spec.substr(0, (Sep == 0) ? 1 : Sep);
  F->Pattern = (Sep == std::string::npos) ? Spec : Spec.substr(Sep + 1);
  F->Dir = opendir(F->Directory.c_str());
  if (F->Dir == nullptr)
  {
    Fail();
    return INVALID_HANDLE_VALUE;
  }
  if (!NextMatch(*F, Data, Name))
  {
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
  }
  return linscp::RegisterKernelObject(F);
}

template <typename TData>
void CopyCommon(const WIN32_FILE_ATTRIBUTE_DATA & A, TData * D)
{
  D->dwFileAttributes = A.dwFileAttributes;
  D->ftCreationTime = A.ftCreationTime;
  D->ftLastAccessTime = A.ftLastAccessTime;
  D->ftLastWriteTime = A.ftLastWriteTime;
  D->nFileSizeHigh = A.nFileSizeHigh;
  D->nFileSizeLow = A.nFileSizeLow;
  D->dwReserved0 = 0;
  D->dwReserved1 = 0;
}

void FillA(const WIN32_FILE_ATTRIBUTE_DATA & A, const std::string & Name, WIN32_FIND_DATAA * D)
{
  std::memset(D, 0, sizeof(*D));
  CopyCommon(A, D);
  std::strncpy(D->cFileName, Name.c_str(), MAX_PATH - 1);
}

void FillW(const WIN32_FILE_ATTRIBUTE_DATA & A, const std::string & Name, WIN32_FIND_DATAW * D)
{
  std::memset(D, 0, sizeof(*D));
  CopyCommon(A, D);
  const std::u16string N = WidenZ(Name.c_str());
  for (std::size_t I = 0; (I < N.size()) && (I < MAX_PATH - 1); ++I)
  {
    D->cFileName[I] = static_cast<wchar_t>(N[I]);
  }
}

template <typename TData, typename TFill>
BOOL ContinueFind(HANDLE Find, TData * Data, TFill Fill)
{
  auto F = std::dynamic_pointer_cast<TFind>(linscp::FindKernelObject(Find));
  if ((F == nullptr) || (Data == nullptr))
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  WIN32_FILE_ATTRIBUTE_DATA A;
  std::string Name;
  if (!NextMatch(*F, A, Name))
  {
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
  }
  Fill(A, Name, Data);
  return TRUE;
}

}  // namespace

HANDLE FindFirstFileA(LPCSTR FileName, WIN32_FIND_DATAA * Data)
{
  WIN32_FILE_ATTRIBUTE_DATA A;
  std::string Name;
  const HANDLE H = StartFind(FileName, A, Name);
  if (H != INVALID_HANDLE_VALUE)
  {
    FillA(A, Name, Data);
  }
  return H;
}

BOOL FindNextFileA(HANDLE Find, WIN32_FIND_DATAA * Data)
{
  return ContinueFind(Find, Data, FillA);
}

HANDLE FindFirstFileW(LPCWSTR FileName, WIN32_FIND_DATAW * Data)
{
  WIN32_FILE_ATTRIBUTE_DATA A;
  std::string Name;
  const HANDLE H = StartFind(linscp::WideToUtf8(FileName), A, Name);
  if (H != INVALID_HANDLE_VALUE)
  {
    FillW(A, Name, Data);
  }
  return H;
}

BOOL FindNextFileW(HANDLE Find, WIN32_FIND_DATAW * Data)
{
  return ContinueFind(Find, Data, FillW);
}

BOOL FindClose(HANDLE Find)
{
  return CloseHandle(Find);
}

// --- pipes and processes ----------------------------------------------------------------

namespace {

struct TChild final : linscp::TKernelObject
{
  pid_t Pid = 0;
  bool Exited = false;
  DWORD ExitCode = STILL_ACTIVE;
  bool IsSignaled() const override { return Exited; }
};

}  // namespace

BOOL CreatePipe(HANDLE * Read, HANDLE * Write, LPSECURITY_ATTRIBUTES Attributes, DWORD)
{
  int Fds[2];
  // Not inheritable unless asked: inheritance is exactly !FD_CLOEXEC.
  const bool Inherit = (Attributes != nullptr) && Attributes->bInheritHandle;
  if (pipe2(Fds, Inherit ? 0 : O_CLOEXEC) != 0)
  {
    return Fail();
  }
  *Read = HandleOf(Fds[0]);
  *Write = HandleOf(Fds[1]);
  return TRUE;
}

BOOL SetHandleInformation(HANDLE Object, DWORD Mask, DWORD Flags)
{
  const int Fd = FdOf(Object);
  if (Fd < 0)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  if ((Mask & HANDLE_FLAG_INHERIT) != 0)
  {
    const int Current = fcntl(Fd, F_GETFD);
    const int Wanted = ((Flags & HANDLE_FLAG_INHERIT) != 0) ? (Current & ~FD_CLOEXEC) : (Current | FD_CLOEXEC);
    if ((Current < 0) || (fcntl(Fd, F_SETFD, Wanted) != 0))
    {
      return Fail();
    }
  }
  return TRUE;
}

BOOL CreateProcessA(LPCSTR Application, LPSTR CommandLine, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
  BOOL InheritHandles, DWORD, LPVOID Environment, LPCSTR CurrentDirectory, STARTUPINFOA * StartupInfo,
  PROCESS_INFORMATION * ProcessInformation)
{
  const char * Command = (CommandLine != nullptr) ? CommandLine : Application;
  if ((Command == nullptr) || (ProcessInformation == nullptr))
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  posix_spawn_file_actions_t Actions;
  posix_spawn_file_actions_init(&Actions);
  if ((StartupInfo != nullptr) && ((StartupInfo->dwFlags & STARTF_USESTDHANDLES) != 0))
  {
    const HANDLE Std[3] = { StartupInfo->hStdInput, StartupInfo->hStdOutput, StartupInfo->hStdError };
    for (int Target = 0; Target < 3; ++Target)
    {
      const int Fd = FdOf(Std[Target]);
      if (Fd >= 0)
      {
        posix_spawn_file_actions_adddup2(&Actions, Fd, Target);
      }
    }
  }
  if (CurrentDirectory != nullptr)
  {
    posix_spawn_file_actions_addchdir_np(&Actions, CurrentDirectory);
  }
  (void)InheritHandles;   // inheritance is per descriptor, as FD_CLOEXEC
  char Sh[] = "/bin/sh", Dash[] = "-c";
  std::string Cmd = Command;
  char * Argv[] = { Sh, Dash, Cmd.data(), nullptr };
  pid_t Pid = 0;
  const int Err = posix_spawn(&Pid, Sh, &Actions, nullptr, Argv,
    (Environment != nullptr) ? static_cast<char **>(Environment) : environ);
  posix_spawn_file_actions_destroy(&Actions);
  if (Err != 0)
  {
    SetLastError(Win32ErrorFromErrno(Err));
    return FALSE;
  }
  auto Child = std::make_shared<TChild>();
  Child->Pid = Pid;
  // The exit is reaped where waits can see it: a thread blocked in waitpid
  // marks the object signalled and wakes every waiter.
  std::thread([Child]()
  {
    int Status = 0;
    while ((waitpid(Child->Pid, &Status, 0) < 0) && (errno == EINTR))
    {
    }
    linscp::ChangeKernelObject([&]()
    {
      Child->ExitCode = WIFEXITED(Status) ? static_cast<DWORD>(WEXITSTATUS(Status))
                                          : static_cast<DWORD>(128 + WTERMSIG(Status));
      Child->Exited = true;
    });
  }).detach();
  ProcessInformation->hProcess = linscp::RegisterKernelObject(Child);
  ProcessInformation->hThread = linscp::RegisterKernelObject(Child);
  ProcessInformation->dwProcessId = static_cast<DWORD>(Pid);
  ProcessInformation->dwThreadId = static_cast<DWORD>(Pid);
  return TRUE;
}

BOOL GetExitCodeProcess(HANDLE Process, LPDWORD ExitCode)
{
  auto Child = std::dynamic_pointer_cast<TChild>(linscp::FindKernelObject(Process));
  if ((Child == nullptr) || (ExitCode == nullptr))
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  linscp::ChangeKernelObject([&]() { *ExitCode = Child->ExitCode; });
  return TRUE;
}

BOOL TerminateProcess(HANDLE Process, UINT)
{
  auto Child = std::dynamic_pointer_cast<TChild>(linscp::FindKernelObject(Process));
  if (Child == nullptr)
  {
    SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
  }
  return (kill(Child->Pid, SIGKILL) == 0) ? TRUE : Fail();
}

// --- registry (narrow) ------------------------------------------------------------------

LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, HKEY * Result)
{
  if (Result != nullptr) { *Result = nullptr; }
  return ERROR_FILE_NOT_FOUND;
}

LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, HKEY * Result, LPDWORD)
{
  if (Result != nullptr) { *Result = nullptr; }
  return ERROR_ACCESS_DENIED;
}

LONG RegCloseKey(HKEY) { return ERROR_INVALID_HANDLE; }
LONG RegEnumKeyA(HKEY, DWORD, LPSTR, DWORD) { return ERROR_INVALID_HANDLE; }
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return ERROR_INVALID_HANDLE; }
LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD) { return ERROR_INVALID_HANDLE; }
LONG RegDeleteKeyA(HKEY, LPCSTR) { return ERROR_INVALID_HANDLE; }
LONG RegDeleteValueA(HKEY, LPCSTR) { return ERROR_INVALID_HANDLE; }

// --- CryptoAPI random numbers ------------------------------------------------------------

BOOL CryptAcquireContextA(HCRYPTPROV * Provider, LPCSTR, LPCSTR, DWORD, DWORD)
{
  if (Provider == nullptr)
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  *Provider = 1;
  return TRUE;
}

BOOL CryptGenRandom(HCRYPTPROV, DWORD Length, BYTE * Buffer)
{
  DWORD Done = 0;
  while (Done < Length)
  {
    const ssize_t N = getrandom(Buffer + Done, Length - Done, 0);
    if (N < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      return Fail();
    }
    Done += static_cast<DWORD>(N);
  }
  return TRUE;
}

BOOL CryptReleaseContext(HCRYPTPROV, DWORD)
{
  return TRUE;
}

BOOL GetNamedPipeClientProcessId(HANDLE, PULONG)
{
  SetLastError(ERROR_INVALID_HANDLE);
  return FALSE;
}

BOOL GetUserNameA(LPSTR Buffer, LPDWORD Size)
{
  const passwd * Pw = getpwuid(geteuid());
  const std::string Name = (Pw != nullptr) ? Pw->pw_name : std::to_string(geteuid());
  // Win32 counts the terminator here, on success and when reporting the
  // size needed.
  if ((Size == nullptr) || (Buffer == nullptr) || (*Size < Name.size() + 1))
  {
    if (Size != nullptr)
    {
      *Size = static_cast<DWORD>(Name.size() + 1);
    }
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return FALSE;
  }
  std::memcpy(Buffer, Name.c_str(), Name.size() + 1);
  *Size = static_cast<DWORD>(Name.size() + 1);
  return TRUE;
}

BOOL IsBadWritePtr(LPVOID Block, UINT_PTR Size)
{
  return ((Block == nullptr) && (Size > 0)) ? TRUE : FALSE;
}

int MessageBoxW(HWND, LPCWSTR Text, LPCWSTR Caption, UINT)
{
  auto Utf8 = [](LPCWSTR W) -> std::string
  {
    if (W == nullptr)
    {
      return std::string();
    }
    const int N = WideCharToMultiByte(CP_UTF8, 0, W, -1, nullptr, 0, nullptr, nullptr);
    std::string R(static_cast<std::size_t>((N > 0) ? N : 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, W, -1, &R[0], N, nullptr, nullptr);
    R.resize(std::strlen(R.c_str()));
    return R;
  };
  std::fprintf(stderr, "%s: %s\n", Utf8(Caption).c_str(), Utf8(Text).c_str());
  return IDOK;
}
