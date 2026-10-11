// Winsock over BSD sockets: the ws2_32 module and its event model.
//
// PuTTY's network.c loads ws2_32.dll and takes every socket function through
// GetProcAddress; core calls WSAEventSelect and WSAEnumNetworkEvents directly.
// Both are written against Winsock's semantics, which differ from BSD's in
// ways that matter:
//
//  - Errors are Winsock codes from WSAGetLastError, not errno; a pending
//    non-blocking connect is WSAEWOULDBLOCK, not EINPROGRESS; name lookup
//    fails with WSAHOST_NOT_FOUND and friends, not EAI_*.
//  - Writing to a closed connection fails; it does not raise SIGPIPE.
//  - WSAEventSelect makes the socket non-blocking and, from then on, records
//    network events and signals an event object, with "re-enabling": FD_READ
//    is recorded once and not again until recv is called, FD_WRITE once
//    after a connect and again only after a send has failed with
//    WSAEWOULDBLOCK, FD_ACCEPT once per accept, FD_CLOSE and FD_CONNECT once.
//    Getting this wrong means either a busy loop (re-reporting a level) or a
//    hang (an event never reported again).
//
// The ws2_32 module (GetProcAddress in win32_misc_posix.cpp) returns this
// file's Winsock-signature functions for the libc names. A monitor thread
// polls every selected socket and records events as Winsock would.
#include "win32_on_posix.h"
#include "winsock2.h"

#include <arpa/inet.h>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <map>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct TSocketState
{
  HANDLE Event = nullptr;
  // WSAAsyncSelect's target, instead of Event.
  HWND Window = nullptr;
  UINT Message = 0;
  long Mask = 0;
  long Pending = 0;
  int Errors[FD_MAX_EVENTS] = {};
  bool Connecting = false;
  bool Listening = false;
  // Neither connected, connecting nor listening: Winsock reports nothing.
  // (Linux polls such a socket as writable and hung up.)
  bool Idle = false;
  bool ReadArmed = true;
  bool WriteArmed = true;
  bool AcceptArmed = true;
  bool OobArmed = true;
  bool CloseReported = false;
  bool SendShutdown = false;
};

std::mutex GMutex;
std::map<int, TSocketState> GSockets;
int GWake[2] = { -1, -1 };
bool GMonitorStarted = false;

void Wake()
{
  if (GWake[1] >= 0)
  {
    const char C = 0;
    ssize_t Ignored = write(GWake[1], &C, 1);
    (void)Ignored;
  }
}

int SocketError(int Fd)
{
  int Err = 0;
  socklen_t Len = sizeof(Err);
  getsockopt(Fd, SOL_SOCKET, SO_ERROR, &Err, &Len);
  return Err;
}

// Requires GMutex. Records Event with its error and signals the socket's
// event object -- or, selected with WSAAsyncSelect, posts the event to its
// window.
void Record(int Fd, TSocketState & S, long Event, int Bit, int Error)
{
  if ((S.Mask & Event) == 0)
  {
    return;
  }
  if (S.Window != nullptr)
  {
    PostMessage(S.Window, S.Message, static_cast<WPARAM>(Fd), WSAMAKESELECTREPLY(Event, Error));
    return;
  }
  S.Pending |= Event;
  S.Errors[Bit] = Error;
  if (S.Event != nullptr)
  {
    SetEvent(S.Event);
  }
}

void MonitorLoop()
{
  std::vector<pollfd> Polls;
  std::vector<int> Fds;
  for (;;)
  {
    Polls.clear();
    Fds.clear();
    Polls.push_back({ GWake[0], POLLIN, 0 });
    {
      std::lock_guard<std::mutex> Lock(GMutex);
      for (auto & [Fd, S] : GSockets)
      {
        if (S.Mask == 0)
        {
          continue;
        }
        short Ev = 0;
        if (S.Connecting)
        {
          Ev |= POLLOUT;
        }
        else if (S.Idle)
        {
          continue;
        }
        else
        {
          if (S.Listening)
          {
            if ((S.Mask & FD_ACCEPT) && S.AcceptArmed) { Ev |= POLLIN; }
          }
          else
          {
            if ((S.Mask & FD_READ) && S.ReadArmed) { Ev |= POLLIN; }
            if ((S.Mask & FD_CLOSE) && !S.CloseReported) { Ev |= POLLRDHUP; }
            if ((S.Mask & FD_WRITE) && S.WriteArmed) { Ev |= POLLOUT; }
            if ((S.Mask & FD_OOB) && S.OobArmed) { Ev |= POLLPRI; }
          }
        }
        if (Ev != 0)
        {
          Polls.push_back({ Fd, Ev, 0 });
          Fds.push_back(Fd);
        }
      }
    }
    int N;
    do
    {
      N = poll(Polls.data(), Polls.size(), -1);
    } while ((N < 0) && (errno == EINTR));
    if (Polls[0].revents & POLLIN)
    {
      char Drain[64];
      while (read(GWake[0], Drain, sizeof(Drain)) > 0)
      {
      }
    }
    std::lock_guard<std::mutex> Lock(GMutex);
    for (std::size_t I = 1; I < Polls.size(); ++I)
    {
      const short R = Polls[I].revents;
      auto It = GSockets.find(Polls[I].fd);
      if ((R == 0) || (It == GSockets.end()) || (R & POLLNVAL))
      {
        continue;
      }
      TSocketState & S = It->second;
      const int Fd = Polls[I].fd;
      if (S.Connecting)
      {
        if (R & (POLLOUT | POLLERR | POLLHUP))
        {
          const int Err = SocketError(Fd);
          S.Connecting = false;
          // A failed connect leaves the socket as it was: unconnected.
          S.Idle = (Err != 0);
          Record(Fd, S, FD_CONNECT, FD_CONNECT_BIT, WinsockErrorFromErrno(Err));
          if (Err == 0)
          {
            // Windows posts FD_WRITE once a connect has succeeded.
            S.WriteArmed = false;
            Record(Fd, S, FD_WRITE, FD_WRITE_BIT, 0);
          }
        }
        continue;
      }
      if (S.Listening)
      {
        if (R & POLLIN)
        {
          S.AcceptArmed = false;
          Record(Fd, S, FD_ACCEPT, FD_ACCEPT_BIT, 0);
        }
        continue;
      }
      if ((R & POLLIN) && S.ReadArmed)
      {
        char Peek;
        const ssize_t Got = ::recv(Fd, static_cast<void *>(&Peek), std::size_t{1}, MSG_PEEK | MSG_DONTWAIT);
        if (Got > 0)
        {
          S.ReadArmed = false;
          Record(Fd, S, FD_READ, FD_READ_BIT, 0);
        }
        else if ((Got == 0) && !S.CloseReported)
        {
          S.CloseReported = true;
          Record(Fd, S, FD_CLOSE, FD_CLOSE_BIT, 0);
        }
      }
      if ((R & (POLLRDHUP | POLLHUP | POLLERR)) && !S.CloseReported)
      {
        // A reset is reported as the close's error, as Winsock does.
        const int Err = SocketError(Fd);
        S.CloseReported = true;
        Record(Fd, S, FD_CLOSE, FD_CLOSE_BIT, (Err != 0) ? WinsockErrorFromErrno(Err) : 0);
      }
      if ((R & POLLOUT) && S.WriteArmed)
      {
        S.WriteArmed = false;
        Record(Fd, S, FD_WRITE, FD_WRITE_BIT, 0);
      }
      if ((R & POLLPRI) && S.OobArmed)
      {
        S.OobArmed = false;
        Record(Fd, S, FD_OOB, FD_OOB_BIT, 0);
      }
    }
  }
}

// Requires GMutex.
void StartMonitor()
{
  if (!GMonitorStarted)
  {
    if (pipe2(GWake, O_CLOEXEC | O_NONBLOCK) == 0)
    {
      GMonitorStarted = true;
      std::thread(MonitorLoop).detach();
    }
  }
}

// Requires GMutex: the socket's state, created on first use.
TSocketState & State(int Fd)
{
  return GSockets[Fd];
}

int Fail()
{
  WSASetLastError(WinsockErrorFromErrno(errno));
  return SOCKET_ERROR;
}

int FdOf(SOCKET S)
{
  return static_cast<int>(S);
}

int AddrInfoError(int Err)
{
  switch (Err)
  {
    case 0: return 0;
    case EAI_NONAME: return WSAHOST_NOT_FOUND;
#ifdef EAI_NODATA
    case EAI_NODATA: return WSANO_DATA;
#endif
    case EAI_AGAIN: return WSATRY_AGAIN;
    case EAI_FAIL: return WSANO_RECOVERY;
    case EAI_MEMORY: return ERROR_NOT_ENOUGH_MEMORY;
    case EAI_FAMILY: return WSAEAFNOSUPPORT;
    case EAI_SOCKTYPE: return WSAESOCKTNOSUPPORT;
    case EAI_SERVICE: return WSABASEERR + 109;   // WSATYPE_NOT_FOUND
    case EAI_BADFLAGS: return WSAEINVAL;
    case EAI_SYSTEM: return WinsockErrorFromErrno(errno);
    default: return WSANO_RECOVERY;
  }
}

}  // namespace

// --- the event model -----------------------------------------------------------

WSAEVENT WSACreateEvent()
{
  // Manual reset, initially clear, as Winsock's.
  return CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

BOOL WSACloseEvent(WSAEVENT Event) { return CloseHandle(Event); }
BOOL WSAResetEvent(WSAEVENT Event) { return ResetEvent(Event); }
BOOL WSASetEvent(WSAEVENT Event) { return SetEvent(Event); }

DWORD WSAWaitForMultipleEvents(DWORD Count, const WSAEVENT * Events, BOOL WaitAll, DWORD Timeout, BOOL)
{
  return WaitForMultipleObjects(Count, Events, WaitAll, Timeout);
}

namespace {

// Either selection: events to an event object or to a window.
int Select(SOCKET Socket, WSAEVENT Event, HWND Window, UINT Message, long NetworkEvents)
{
  const int Fd = FdOf(Socket);
  // As Winsock: selecting events makes the socket non-blocking, and stays so.
  const int Flags = fcntl(Fd, F_GETFL);
  if ((Flags < 0) || (fcntl(Fd, F_SETFL, Flags | O_NONBLOCK) < 0))
  {
    return Fail();
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  StartMonitor();
  TSocketState & S = State(Fd);
  S.Event = Event;
  S.Window = Window;
  S.Message = Message;
  S.Mask = NetworkEvents;
  // A new selection clears what was recorded and re-evaluates the socket:
  // Winsock posts FD_READ, FD_WRITE, FD_ACCEPT or FD_CLOSE at once if they
  // already hold.
  S.Pending = 0;
  std::memset(S.Errors, 0, sizeof(S.Errors));
  S.ReadArmed = S.WriteArmed = S.AcceptArmed = S.OobArmed = true;
  S.CloseReported = false;
  int Listening = 0;
  socklen_t Len = sizeof(Listening);
  S.Listening = (getsockopt(Fd, SOL_SOCKET, SO_ACCEPTCONN, &Listening, &Len) == 0) && (Listening != 0);
  sockaddr_storage Peer;
  socklen_t PeerLen = sizeof(Peer);
  S.Idle = !S.Listening && !S.Connecting && (getpeername(Fd, reinterpret_cast<sockaddr *>(&Peer), &PeerLen) != 0);
  Wake();
  return 0;
}

}  // namespace

int WSAEventSelect(SOCKET Socket, WSAEVENT Event, long NetworkEvents)
{
  if ((NetworkEvents != 0) && (Event == nullptr))
  {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  return Select(Socket, Event, nullptr, 0, NetworkEvents);
}

int WSAEnumNetworkEvents(SOCKET Socket, WSAEVENT Event, WSANETWORKEVENTS * Events)
{
  if (Events == nullptr)
  {
    WSASetLastError(WSAEFAULT);
    return SOCKET_ERROR;
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  auto It = GSockets.find(FdOf(Socket));
  std::memset(Events, 0, sizeof(*Events));
  if (It != GSockets.end())
  {
    Events->lNetworkEvents = It->second.Pending;
    std::memcpy(Events->iErrorCode, It->second.Errors, sizeof(Events->iErrorCode));
    It->second.Pending = 0;
    std::memset(It->second.Errors, 0, sizeof(It->second.Errors));
  }
  if (Event != nullptr)
  {
    ResetEvent(Event);
  }
  return 0;
}

int WSAAsyncSelect(SOCKET Socket, HWND Window, UINT Message, LONG Events)
{
  if ((Events != 0) && !IsWindow(Window))
  {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  return Select(Socket, nullptr, Window, Message, Events);
}

// --- asynchronous host lookup --------------------------------------------------

namespace {

std::mutex GLookupMutex;
// Requests in flight, by handle; true once cancelled.
std::map<HANDLE, bool> GLookups;
std::uintptr_t GNextLookup = 1;

// Lays a hostent for Info's IPv4 addresses out in Out, as Winsock does: the
// structure, then the pointer arrays, the addresses and the name, all within
// the buffer. Returns the length that needs; writes only if it fits Size.
std::size_t LayOutHostent(const char * Name, const addrinfo * Info, char * Out, std::size_t Size)
{
  std::vector<in_addr> Addresses;
  const char * Canonical = Name;
  for (const addrinfo * I = Info; I != nullptr; I = I->ai_next)
  {
    if ((I->ai_canonname != nullptr) && (Canonical == Name))
    {
      Canonical = I->ai_canonname;
    }
    if (I->ai_family == AF_INET)
    {
      Addresses.push_back(reinterpret_cast<const sockaddr_in *>(I->ai_addr)->sin_addr);
    }
  }
  const std::size_t NameLength = std::strlen(Canonical) + 1;
  const std::size_t Pointers = (1 + Addresses.size() + 1) * sizeof(char *);
  const std::size_t Needed = sizeof(hostent) + Pointers + Addresses.size() * sizeof(in_addr) + NameLength;
  if (Needed > Size)
  {
    return Needed;
  }
  hostent * H = reinterpret_cast<hostent *>(Out);
  char ** Aliases = reinterpret_cast<char **>(Out + sizeof(hostent));
  char ** List = Aliases + 1;
  char * Data = Out + sizeof(hostent) + Pointers;
  char * NameOut = Data + Addresses.size() * sizeof(in_addr);
  Aliases[0] = nullptr;
  for (std::size_t I = 0; I < Addresses.size(); ++I)
  {
    std::memcpy(Data + I * sizeof(in_addr), &Addresses[I], sizeof(in_addr));
    List[I] = Data + I * sizeof(in_addr);
  }
  List[Addresses.size()] = nullptr;
  std::memcpy(NameOut, Canonical, NameLength);
  H->h_name = NameOut;
  H->h_aliases = Aliases;
  H->h_addrtype = AF_INET;
  H->h_length = sizeof(in_addr);
  H->h_addr_list = List;
  return Needed;
}

}  // namespace

HANDLE WSAAsyncGetHostByName(HWND Window, UINT Message, const char * Name, char * Buffer, int BufferLength)
{
  if ((Name == nullptr) || (Buffer == nullptr) || (BufferLength < 0) || !IsWindow(Window))
  {
    WSASetLastError(WSAEINVAL);
    return nullptr;
  }
  HANDLE Handle;
  {
    std::lock_guard<std::mutex> Lock(GLookupMutex);
    Handle = reinterpret_cast<HANDLE>(GNextLookup++);
    GLookups[Handle] = false;
  }
  std::thread([Handle, Window, Message, Host = std::string(Name), Buffer, BufferLength]()
  {
    addrinfo Hints{};
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_CANONNAME;
    addrinfo * Info = nullptr;
    int Error = AddrInfoError(getaddrinfo(Host.c_str(), nullptr, &Hints, &Info));
    {
      // The buffer is written, and the reply posted, only for a request
      // still wanted: after WSACancelAsyncRequest the caller may free it.
      std::lock_guard<std::mutex> Lock(GLookupMutex);
      const auto It = GLookups.find(Handle);
      if ((It != GLookups.end()) && !It->second)
      {
        std::size_t Needed = 0;
        if (Error == 0)
        {
          Needed = LayOutHostent(Host.c_str(), Info, Buffer, static_cast<std::size_t>(BufferLength));
          if (Needed > static_cast<std::size_t>(BufferLength))
          {
            Error = WSAENOBUFS;
          }
        }
        PostMessage(Window, Message, reinterpret_cast<WPARAM>(Handle),
          WSAMAKEASYNCREPLY((Needed > 0xFFFF) ? 0xFFFF : Needed, Error));
      }
      GLookups.erase(Handle);
    }
    if (Info != nullptr)
    {
      freeaddrinfo(Info);
    }
  }).detach();
  return Handle;
}

int WSACancelAsyncRequest(HANDLE AsyncTaskHandle)
{
  std::lock_guard<std::mutex> Lock(GLookupMutex);
  const auto It = GLookups.find(AsyncTaskHandle);
  if ((It == GLookups.end()) || It->second)
  {
    WSASetLastError((It == GLookups.end()) ? WSAEALREADY : WSAEINVAL);
    return SOCKET_ERROR;
  }
  It->second = true;
  return 0;
}

// --- Winsock-signature socket functions (the ws2_32 module) ---------------------

namespace {

SOCKET WINAPI Ws2Socket(int Family, int Type, int Protocol)
{
  const int Fd = socket(Family, Type | SOCK_CLOEXEC, Protocol);
  if (Fd < 0)
  {
    Fail();
    return INVALID_SOCKET;
  }
  return static_cast<SOCKET>(Fd);
}

int WINAPI Ws2Connect(SOCKET S, const struct sockaddr * Name, int NameLen)
{
  const int Fd = FdOf(S);
  const int Result = connect(Fd, Name, static_cast<socklen_t>(NameLen));
  if ((Result == 0) || (errno == EINPROGRESS))
  {
    // Pending -- or, on a selected socket, done at once: the monitor reports
    // FD_CONNECT and then FD_WRITE once the socket is writable. A blocking
    // connect made before any selection is simply connected; selecting it
    // later reports FD_WRITE alone, as Winsock does.
    std::lock_guard<std::mutex> Lock(GMutex);
    TSocketState & St = State(Fd);
    St.Connecting = (Result != 0) || (St.Mask != 0);
    St.Idle = false;
    Wake();
    if (Result == 0)
    {
      return 0;
    }
    WSASetLastError(WSAEWOULDBLOCK);
    return SOCKET_ERROR;
  }
  return Fail();
}

int WINAPI Ws2Bind(SOCKET S, const struct sockaddr * Name, int NameLen)
{
  return (bind(FdOf(S), Name, static_cast<socklen_t>(NameLen)) == 0) ? 0 : Fail();
}

int WINAPI Ws2Listen(SOCKET S, int Backlog)
{
  const int Fd = FdOf(S);
  if (listen(Fd, Backlog) != 0)
  {
    return Fail();
  }
  std::lock_guard<std::mutex> Lock(GMutex);
  TSocketState & St = State(Fd);
  St.Listening = true;
  St.Idle = false;
  Wake();
  return 0;
}

SOCKET WINAPI Ws2Accept(SOCKET S, struct sockaddr * Address, int * AddressLen)
{
  const int Fd = FdOf(S);
  socklen_t Len = (AddressLen != nullptr) ? static_cast<socklen_t>(*AddressLen) : 0;
  const int New = accept4(Fd, Address, (AddressLen != nullptr) ? &Len : nullptr, SOCK_CLOEXEC);
  std::lock_guard<std::mutex> Lock(GMutex);
  TSocketState & L = State(Fd);
  L.AcceptArmed = true;
  Wake();
  if (New < 0)
  {
    Fail();
    return INVALID_SOCKET;
  }
  if (AddressLen != nullptr)
  {
    *AddressLen = static_cast<int>(Len);
  }
  // An accepted socket takes the listener's selection, as on Windows.
  if (L.Mask != 0)
  {
    const int Flags = fcntl(New, F_GETFL);
    fcntl(New, F_SETFL, Flags | O_NONBLOCK);
    TSocketState & A = State(New);
    A.Event = L.Event;
    A.Window = L.Window;
    A.Message = L.Message;
    A.Mask = L.Mask;
  }
  return static_cast<SOCKET>(New);
}

int WINAPI Ws2Send(SOCKET S, const char * Buffer, int Length, int Flags)
{
  const int Fd = FdOf(S);
  const ssize_t N = ::send(Fd, static_cast<const void *>(Buffer), static_cast<std::size_t>(Length), Flags | MSG_NOSIGNAL);
  if (N >= 0)
  {
    return static_cast<int>(N);
  }
  if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
  {
    // Re-enables FD_WRITE: recorded again once the socket can take data.
    std::lock_guard<std::mutex> Lock(GMutex);
    State(Fd).WriteArmed = true;
    Wake();
  }
  if (errno == EPIPE)
  {
    // POSIX's one code for "this connection can no longer be written to" is
    // two to Winsock: shut down here for sending, or gone at the peer.
    std::lock_guard<std::mutex> Lock(GMutex);
    WSASetLastError(State(Fd).SendShutdown ? WSAESHUTDOWN : WSAECONNRESET);
    return SOCKET_ERROR;
  }
  return Fail();
}

int WINAPI Ws2Recv(SOCKET S, char * Buffer, int Length, int Flags)
{
  const int Fd = FdOf(S);
  const ssize_t N = ::recv(Fd, static_cast<void *>(Buffer), static_cast<std::size_t>(Length), Flags);
  const int Saved = errno;
  {
    // Any recv re-enables FD_READ (and an out-of-band one FD_OOB).
    std::lock_guard<std::mutex> Lock(GMutex);
    TSocketState & St = State(Fd);
    St.ReadArmed = true;
    if (Flags & MSG_OOB)
    {
      St.OobArmed = true;
    }
    Wake();
  }
  errno = Saved;
  return (N >= 0) ? static_cast<int>(N) : Fail();
}

int WINAPI Ws2Shutdown(SOCKET S, int How)
{
  if (shutdown(FdOf(S), How) != 0)
  {
    return Fail();
  }
  if ((How == SD_SEND) || (How == SD_BOTH))
  {
    std::lock_guard<std::mutex> Lock(GMutex);
    State(FdOf(S)).SendShutdown = true;
  }
  return 0;
}

int WINAPI Ws2Getsockopt(SOCKET S, int Level, int Name, char * Value, int * Length)
{
  socklen_t Len = (Length != nullptr) ? static_cast<socklen_t>(*Length) : 0;
  if (getsockopt(FdOf(S), Level, Name, Value, &Len) != 0)
  {
    return Fail();
  }
  if (Length != nullptr)
  {
    *Length = static_cast<int>(Len);
  }
  return 0;
}

int WINAPI Ws2Setsockopt(SOCKET S, int Level, int Name, const char * Value, int Length)
{
  if ((Level == SOL_SOCKET) && (Name == SO_EXCLUSIVEADDRUSE))
  {
    // Already the POSIX default: a bound address is exclusive without
    // SO_REUSEPORT.
    return 0;
  }
  return (::setsockopt(FdOf(S), Level, Name, static_cast<const void *>(Value), static_cast<socklen_t>(Length)) == 0) ?
    0 : Fail();
}

int WINAPI Ws2Getpeername(SOCKET S, struct sockaddr * Name, int * NameLen)
{
  socklen_t Len = static_cast<socklen_t>(*NameLen);
  if (getpeername(FdOf(S), Name, &Len) != 0)
  {
    return Fail();
  }
  *NameLen = static_cast<int>(Len);
  return 0;
}

int WINAPI Ws2Getsockname(SOCKET S, struct sockaddr * Name, int * NameLen)
{
  socklen_t Len = static_cast<socklen_t>(*NameLen);
  if (getsockname(FdOf(S), Name, &Len) != 0)
  {
    return Fail();
  }
  *NameLen = static_cast<int>(Len);
  return 0;
}

int WINAPI Ws2Gethostname(char * Name, int Length)
{
  return (gethostname(Name, static_cast<std::size_t>(Length)) == 0) ? 0 : Fail();
}

struct hostent * WINAPI Ws2Gethostbyname(const char * Name)
{
  struct hostent * H = gethostbyname(Name);
  if (H == nullptr)
  {
    WSASetLastError((h_errno == HOST_NOT_FOUND) ? WSAHOST_NOT_FOUND :
                    (h_errno == TRY_AGAIN) ? WSATRY_AGAIN :
                    (h_errno == NO_DATA) ? WSANO_DATA : WSANO_RECOVERY);
  }
  return H;
}

struct servent * WINAPI Ws2Getservbyname(const char * Name, const char * Proto)
{
  return getservbyname(Name, Proto);
}

ULONG WINAPI Ws2InetAddr(const char * Cp)
{
  return static_cast<ULONG>(inet_addr(Cp));
}

char * WINAPI Ws2InetNtoa(struct in_addr In)
{
  return inet_ntoa(In);
}

const char * WINAPI Ws2InetNtop(int Family, void * Address, char * Buffer, size_t Size)
{
  const char * R = inet_ntop(Family, Address, Buffer, static_cast<socklen_t>(Size));
  if (R == nullptr)
  {
    Fail();
  }
  return R;
}

ULONG WINAPI Ws2Htonl(ULONG V) { return htonl(V); }
ULONG WINAPI Ws2Ntohl(ULONG V) { return ntohl(V); }
USHORT WINAPI Ws2Htons(USHORT V) { return htons(V); }
USHORT WINAPI Ws2Ntohs(USHORT V) { return ntohs(V); }

int WINAPI Ws2Getaddrinfo(const char * Node, const char * Service, const struct addrinfo * Hints,
  struct addrinfo ** Result)
{
  const int Err = AddrInfoError(getaddrinfo(Node, Service, Hints, Result));
  if (Err != 0)
  {
    *Result = nullptr;
    WSASetLastError(Err);
  }
  return Err;
}

void WINAPI Ws2Freeaddrinfo(struct addrinfo * Info)
{
  freeaddrinfo(Info);
}

int WINAPI Ws2Getnameinfo(const struct sockaddr * Address, socklen_t AddressLen, char * Host, DWORD HostLen,
  char * Serv, DWORD ServLen, int Flags)
{
  const int Err = AddrInfoError(getnameinfo(Address, AddressLen, Host, HostLen, Serv, ServLen, Flags));
  if (Err != 0)
  {
    WSASetLastError(Err);
  }
  return Err;
}

int WINAPI Ws2Select(int, fd_set * Read, fd_set * Write, fd_set * Except, const struct timeval * Timeout)
{
  // fd_set is the POSIX one here (network.c compiles against these
  // headers), so the set's size is FD_SETSIZE, not a Winsock count.
  timeval T;
  timeval * P = nullptr;
  if (Timeout != nullptr)
  {
    T = *Timeout;
    P = &T;
  }
  const int N = select(FD_SETSIZE, Read, Write, Except, P);
  return (N >= 0) ? N : Fail();
}

int WINAPI Ws2WSAGetLastError() { return WSAGetLastError(); }
int WINAPI Ws2WSAStartup(WORD Version, LPWSADATA Data) { return WSAStartup(Version, Data); }
int WINAPI Ws2WSACleanup() { return WSACleanup(); }

int WINAPI Ws2Closesocket(SOCKET S)
{
  return closesocket(S);
}

int WINAPI Ws2Ioctlsocket(SOCKET S, LONG Command, ULONG * Argument)
{
  return ioctlsocket(S, Command, Argument);
}

}  // namespace

int closesocket(SOCKET S)
{
  const int Fd = FdOf(S);
  {
    std::lock_guard<std::mutex> Lock(GMutex);
    GSockets.erase(Fd);
    Wake();
  }
  return (close(Fd) == 0) ? 0 : Fail();
}

int ioctlsocket(SOCKET S, LONG Command, ULONG * Argument)
{
  const int Fd = FdOf(S);
  // Winsock's codes, and the POSIX ones the same names have here.
  const unsigned long Code = static_cast<unsigned long>(static_cast<ULONG>(Command));
  if ((Code == FIONBIO) || (Code == 0x8004667Eu))
  {
    const int Flags = fcntl(Fd, F_GETFL);
    const int Wanted = (Argument != nullptr && *Argument) ? (Flags | O_NONBLOCK) : (Flags & ~O_NONBLOCK);
    return ((Flags >= 0) && (fcntl(Fd, F_SETFL, Wanted) == 0)) ? 0 : Fail();
  }
  if ((Code == FIONREAD) || (Code == 0x4004667Fu))
  {
    int Available = 0;
    if (ioctl(Fd, FIONREAD, &Available) != 0)
    {
      return Fail();
    }
    *Argument = static_cast<ULONG>(Available);
    return 0;
  }
  if ((Code == SIOCATMARK) || (Code == 0x40047307u))
  {
    const int Mark = sockatmark(Fd);
    if (Mark < 0)
    {
      return Fail();
    }
    // Winsock reports TRUE when there is no urgent data to read.
    *Argument = Mark ? 0 : 1;
    return 0;
  }
  WSASetLastError(WSAEINVAL);
  return SOCKET_ERROR;
}

int WSAIoctl(SOCKET, DWORD Code, LPVOID, DWORD, LPVOID OutBuffer, DWORD OutSize, LPDWORD BytesReturned,
  LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE)
{
  if (BytesReturned != nullptr)
  {
    *BytesReturned = 0;
  }
  if (Code == SIO_GET_INTERFACE_LIST)
  {
    ifaddrs * List = nullptr;
    if (getifaddrs(&List) != 0)
    {
      return Fail();
    }
    std::vector<INTERFACE_INFO> Out;
    for (ifaddrs * I = List; I != nullptr; I = I->ifa_next)
    {
      if ((I->ifa_addr == nullptr) || ((I->ifa_addr->sa_family != AF_INET) && (I->ifa_addr->sa_family != AF_INET6)))
      {
        continue;
      }
      INTERFACE_INFO Info{};
      // Winsock's flag values: up 1, broadcast 2, loopback 4, point-to-point
      // 8, multicast 16.
      Info.iiFlags = ((I->ifa_flags & IFF_UP) ? 1u : 0u) | ((I->ifa_flags & IFF_BROADCAST) ? 2u : 0u) |
                     ((I->ifa_flags & IFF_LOOPBACK) ? 4u : 0u) | ((I->ifa_flags & IFF_POINTOPOINT) ? 8u : 0u) |
                     ((I->ifa_flags & IFF_MULTICAST) ? 16u : 0u);
      const std::size_t Size = (I->ifa_addr->sa_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
      std::memcpy(&Info.iiAddress, I->ifa_addr, Size);
      if (I->ifa_netmask != nullptr)
      {
        std::memcpy(&Info.iiNetmask, I->ifa_netmask, Size);
      }
      Out.push_back(Info);
    }
    freeifaddrs(List);
    const DWORD Bytes = static_cast<DWORD>(Out.size() * sizeof(INTERFACE_INFO));
    if (OutSize < Bytes)
    {
      WSASetLastError(WSAEFAULT);
      return SOCKET_ERROR;
    }
    std::memcpy(OutBuffer, Out.data(), Bytes);
    if (BytesReturned != nullptr)
    {
      *BytesReturned = Bytes;
    }
    return 0;
  }
  // Notably SIO_IDEAL_SEND_BACKLOG_QUERY: Linux has no ideal-send-backlog
  // query, and core keeps its send buffer as configured.
  WSASetLastError(WSAEOPNOTSUPP);
  return SOCKET_ERROR;
}

int WSAAddressToStringA(LPSOCKADDR Address, DWORD AddressLength, LPWSAPROTOCOL_INFO, LPSTR String,
  LPDWORD StringLength)
{
  char Host[INET6_ADDRSTRLEN] = {};
  std::string Text;
  if ((Address->sa_family == AF_INET) && (AddressLength >= sizeof(sockaddr_in)))
  {
    const auto * A = reinterpret_cast<const sockaddr_in *>(Address);
    inet_ntop(AF_INET, &A->sin_addr, Host, sizeof(Host));
    Text = Host;
    if (A->sin_port != 0)
    {
      Text += ":" + std::to_string(ntohs(A->sin_port));
    }
  }
  else if ((Address->sa_family == AF_INET6) && (AddressLength >= sizeof(sockaddr_in6)))
  {
    const auto * A = reinterpret_cast<const sockaddr_in6 *>(Address);
    inet_ntop(AF_INET6, &A->sin6_addr, Host, sizeof(Host));
    Text = (A->sin6_port != 0) ? "[" + std::string(Host) + "]:" + std::to_string(ntohs(A->sin6_port)) : Host;
  }
  else
  {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  if ((String == nullptr) || (*StringLength < Text.size() + 1))
  {
    *StringLength = static_cast<DWORD>(Text.size() + 1);
    WSASetLastError(WSAEFAULT);
    return SOCKET_ERROR;
  }
  std::memcpy(String, Text.c_str(), Text.size() + 1);
  *StringLength = static_cast<DWORD>(Text.size() + 1);
  return 0;
}

// The same functions under Winsock's names, for C++ calling them directly.
int connect(SOCKET S, const struct sockaddr * Name, int NameLen) { return Ws2Connect(S, Name, NameLen); }
int bind(SOCKET S, const struct sockaddr * Name, int NameLen) { return Ws2Bind(S, Name, NameLen); }
int listen(SOCKET S, int Backlog) { return Ws2Listen(S, Backlog); }
SOCKET accept(SOCKET S, struct sockaddr * Address, int * AddressLen) { return Ws2Accept(S, Address, AddressLen); }
int send(SOCKET S, const char * Buffer, int Length, int Flags) { return Ws2Send(S, Buffer, Length, Flags); }
int recv(SOCKET S, char * Buffer, int Length, int Flags) { return Ws2Recv(S, Buffer, Length, Flags); }
int shutdown(SOCKET S, int How) { return Ws2Shutdown(S, How); }
int getsockopt(SOCKET S, int Level, int Name, char * Value, int * Length)
{
  return Ws2Getsockopt(S, Level, Name, Value, Length);
}
int setsockopt(SOCKET S, int Level, int Name, const char * Value, int Length)
{
  return Ws2Setsockopt(S, Level, Name, Value, Length);
}
int getpeername(SOCKET S, struct sockaddr * Name, int * NameLen) { return Ws2Getpeername(S, Name, NameLen); }
int getsockname(SOCKET S, struct sockaddr * Name, int * NameLen) { return Ws2Getsockname(S, Name, NameLen); }

// The ws2_32 module's exports, for GetProcAddress.
FARPROC linscp_Ws2Proc(LPCSTR Name)
{
  struct TExport { const char * Name; FARPROC Proc; };
#define EXPORT(N, F) { N, reinterpret_cast<FARPROC>(&F) }
  static const TExport Exports[] = {
    EXPORT("socket", Ws2Socket), EXPORT("connect", Ws2Connect), EXPORT("bind", Ws2Bind),
    EXPORT("listen", Ws2Listen), EXPORT("accept", Ws2Accept), EXPORT("send", Ws2Send),
    EXPORT("recv", Ws2Recv), EXPORT("shutdown", Ws2Shutdown), EXPORT("getsockopt", Ws2Getsockopt),
    EXPORT("setsockopt", Ws2Setsockopt), EXPORT("getpeername", Ws2Getpeername),
    EXPORT("getsockname", Ws2Getsockname), EXPORT("gethostname", Ws2Gethostname),
    EXPORT("gethostbyname", Ws2Gethostbyname), EXPORT("getservbyname", Ws2Getservbyname),
    EXPORT("inet_addr", Ws2InetAddr), EXPORT("inet_ntoa", Ws2InetNtoa), EXPORT("inet_ntop", Ws2InetNtop),
    EXPORT("htonl", Ws2Htonl), EXPORT("ntohl", Ws2Ntohl), EXPORT("htons", Ws2Htons),
    EXPORT("ntohs", Ws2Ntohs), EXPORT("getaddrinfo", Ws2Getaddrinfo),
    EXPORT("freeaddrinfo", Ws2Freeaddrinfo), EXPORT("getnameinfo", Ws2Getnameinfo),
    EXPORT("select", Ws2Select), EXPORT("closesocket", Ws2Closesocket),
    EXPORT("ioctlsocket", Ws2Ioctlsocket), EXPORT("WSAGetLastError", Ws2WSAGetLastError),
    EXPORT("WSAStartup", Ws2WSAStartup), EXPORT("WSACleanup", Ws2WSACleanup),
    EXPORT("WSAEventSelect", WSAEventSelect), EXPORT("WSAEnumNetworkEvents", WSAEnumNetworkEvents),
    EXPORT("WSAAsyncSelect", WSAAsyncSelect), EXPORT("WSAIoctl", WSAIoctl),
    EXPORT("WSAAddressToStringA", WSAAddressToStringA), EXPORT("WSACreateEvent", WSACreateEvent),
    EXPORT("WSACloseEvent", WSACloseEvent), EXPORT("WSAWaitForMultipleEvents", WSAWaitForMultipleEvents),
    EXPORT("WSAAsyncGetHostByName", WSAAsyncGetHostByName), EXPORT("WSACancelAsyncRequest", WSACancelAsyncRequest),
  };
#undef EXPORT
  for (const TExport & E : Exports)
  {
    if (std::strcmp(E.Name, Name) == 0)
    {
      return E.Proc;
    }
  }
  return nullptr;
}
