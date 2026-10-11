// Winsock's event model, as PuTTY's network.c and core's SSH loop use it.
//
// Every function comes through GetProcAddress(LoadLibrary("ws2_32.dll")), as
// network.c takes them, so on Windows this runs against real Winsock (under
// Wine when cross-compiling) and on POSIX against the emulation, with the
// same assertions. The re-enabling rules are what matter most: an event that
// is re-posted while its condition holds makes a busy loop, and one that is
// not re-posted when it should be makes a hang.
#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include "vcl_win32_undef.h"
#else
  #include "win32_on_posix.h"
  #include "winsock2.h"
#endif

#include <cstdio>
#include <cstring>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

typedef SOCKET (WINAPI * TSocket)(int, int, int);
typedef int (WINAPI * TBind)(SOCKET, const struct sockaddr *, int);
typedef int (WINAPI * TListen)(SOCKET, int);
typedef int (WINAPI * TConnect)(SOCKET, const struct sockaddr *, int);
typedef SOCKET (WINAPI * TAccept)(SOCKET, struct sockaddr *, int *);
typedef int (WINAPI * TSend)(SOCKET, const char *, int, int);
typedef int (WINAPI * TRecv)(SOCKET, char *, int, int);
typedef int (WINAPI * TShutdown)(SOCKET, int);
typedef int (WINAPI * TGetsockname)(SOCKET, struct sockaddr *, int *);
typedef int (WINAPI * TClosesocket)(SOCKET);
typedef int (WINAPI * TGetLastError)(void);
typedef int (WINAPI * TStartup)(WORD, WSADATA *);
typedef int (WINAPI * TEventSelect)(SOCKET, WSAEVENT, long);
typedef int (WINAPI * TEnum)(SOCKET, WSAEVENT, WSANETWORKEVENTS *);
typedef int (WINAPI * TGetaddrinfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **);

struct TWs2
{
  TSocket socket_;
  TBind bind_;
  TListen listen_;
  TConnect connect_;
  TAccept accept_;
  TSend send_;
  TRecv recv_;
  TShutdown shutdown_;
  TGetsockname getsockname_;
  TClosesocket closesocket_;
  TGetLastError lasterror_;
  TStartup startup_;
  TEventSelect eventselect_;
  TEnum enumevents_;
  TGetaddrinfo getaddrinfo_;
};

template <typename T>
void Load(HMODULE Module, const char * Name, T & Proc)
{
  Proc = reinterpret_cast<T>(GetProcAddress(Module, Name));
  if (Proc == nullptr)
  {
    std::printf("FAIL: ws2_32 has no %s\n", Name);
    ++Failures;
  }
}

// Waits for the event, then returns what was recorded for the socket.
long Events(const TWs2 & W, SOCKET S, HANDLE Event, DWORD Timeout, int * Errors = nullptr)
{
  if (WaitForSingleObject(Event, Timeout) != WAIT_OBJECT_0)
  {
    return 0;
  }
  WSANETWORKEVENTS E;
  std::memset(&E, 0, sizeof(E));
  CHECK(W.enumevents_(S, Event, &E) == 0);
  if (Errors != nullptr)
  {
    std::memcpy(Errors, E.iErrorCode, sizeof(E.iErrorCode));
  }
  return E.lNetworkEvents;
}

// Waits until every event in Wanted has been recorded, OR-ing them together.
long Collect(const TWs2 & W, SOCKET S, HANDLE Event, long Wanted, int * Errors = nullptr)
{
  long Seen = 0;
  for (int I = 0; (I < 20) && ((Seen & Wanted) != Wanted); ++I)
  {
    Seen |= Events(W, S, Event, 250, Errors);
  }
  return Seen;
}

}  // namespace

int main()
{
  HMODULE Module = LoadLibrary(L"ws2_32.dll");
  CHECK(Module != NULL);
  if (Module == NULL)
  {
    return 1;
  }
  TWs2 W;
  Load(Module, "socket", W.socket_);
  Load(Module, "bind", W.bind_);
  Load(Module, "listen", W.listen_);
  Load(Module, "connect", W.connect_);
  Load(Module, "accept", W.accept_);
  Load(Module, "send", W.send_);
  Load(Module, "recv", W.recv_);
  Load(Module, "shutdown", W.shutdown_);
  Load(Module, "getsockname", W.getsockname_);
  Load(Module, "closesocket", W.closesocket_);
  Load(Module, "WSAGetLastError", W.lasterror_);
  Load(Module, "WSAStartup", W.startup_);
  Load(Module, "WSAEventSelect", W.eventselect_);
  Load(Module, "WSAEnumNetworkEvents", W.enumevents_);
  Load(Module, "getaddrinfo", W.getaddrinfo_);
  if (Failures != 0)
  {
    return 1;
  }
  WSADATA Data;
  CHECK(W.startup_(MAKEWORD(2, 2), &Data) == 0);

  // A listener on loopback, selecting FD_ACCEPT.
  SOCKET Listener = W.socket_(AF_INET, SOCK_STREAM, 0);
  CHECK(Listener != INVALID_SOCKET);
  sockaddr_in Address;
  std::memset(&Address, 0, sizeof(Address));
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(W.bind_(Listener, reinterpret_cast<sockaddr *>(&Address), sizeof(Address)) == 0);
  CHECK(W.listen_(Listener, 4) == 0);
  int Len = sizeof(Address);
  CHECK(W.getsockname_(Listener, reinterpret_cast<sockaddr *>(&Address), &Len) == 0);
  HANDLE ListenEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  CHECK(W.eventselect_(Listener, ListenEvent, FD_ACCEPT) == 0);

  // A client: selecting makes it non-blocking, so connect is pending.
  SOCKET Client = W.socket_(AF_INET, SOCK_STREAM, 0);
  HANDLE ClientEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  CHECK(W.eventselect_(Client, ClientEvent, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == 0);
  const int Connect = W.connect_(Client, reinterpret_cast<sockaddr *>(&Address), sizeof(Address));
  CHECK((Connect == 0) || (W.lasterror_() == WSAEWOULDBLOCK));

  // FD_CONNECT without error, then FD_WRITE.
  int Errors[FD_MAX_EVENTS];
  std::memset(Errors, 0xFF, sizeof(Errors));
  long Seen = Collect(W, Client, ClientEvent, FD_CONNECT | FD_WRITE, Errors);
  CHECK((Seen & FD_CONNECT) != 0);
  CHECK(Errors[FD_CONNECT_BIT] == 0);
  CHECK((Seen & FD_WRITE) != 0);
  // FD_WRITE is not posted again while nothing has failed with WSAEWOULDBLOCK.
  CHECK((Events(W, Client, ClientEvent, 300) & FD_WRITE) == 0);

  // The listener reports the connection; accept it.
  CHECK((Collect(W, Listener, ListenEvent, FD_ACCEPT) & FD_ACCEPT) != 0);
  SOCKET Server = W.accept_(Listener, NULL, NULL);
  CHECK(Server != INVALID_SOCKET);
  HANDLE ServerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  // Reselecting re-evaluates: a connected socket is writable at once.
  CHECK(W.eventselect_(Server, ServerEvent, FD_READ | FD_WRITE | FD_CLOSE) == 0);
  CHECK((Collect(W, Server, ServerEvent, FD_WRITE) & FD_WRITE) != 0);

  // FD_READ once data arrives, and not again until recv is called...
  CHECK(W.send_(Client, "hello", 5, 0) == 5);
  CHECK((Collect(W, Server, ServerEvent, FD_READ) & FD_READ) != 0);
  CHECK((Events(W, Server, ServerEvent, 300) & FD_READ) == 0);
  // ...when, data remaining, it is posted again.
  char Buffer[16];
  CHECK(W.recv_(Server, Buffer, 2, 0) == 2);
  CHECK((Collect(W, Server, ServerEvent, FD_READ) & FD_READ) != 0);
  CHECK(W.recv_(Server, Buffer + 2, sizeof(Buffer) - 2, 0) == 3);
  CHECK(std::memcmp(Buffer, "hello", 5) == 0);
  // Nothing left: recv on the non-blocking socket would block.
  CHECK(W.recv_(Server, Buffer, sizeof(Buffer), 0) == SOCKET_ERROR);
  CHECK(W.lasterror_() == WSAEWOULDBLOCK);

  // A graceful close is FD_CLOSE without error, and recv then reads 0.
  CHECK(W.shutdown_(Client, SD_SEND) == 0);
  std::memset(Errors, 0xFF, sizeof(Errors));
  CHECK((Collect(W, Server, ServerEvent, FD_CLOSE, Errors) & FD_CLOSE) != 0);
  CHECK(Errors[FD_CLOSE_BIT] == 0);
  CHECK(W.recv_(Server, Buffer, sizeof(Buffer), 0) == 0);

  // Writing to a connection the peer has torn down fails -- it does not kill
  // the process with SIGPIPE, as a BSD write would.
  CHECK(W.closesocket_(Server) == 0);
  int SendResult = 0;
  for (int I = 0; (I < 50) && (SendResult != SOCKET_ERROR); ++I)
  {
    SendResult = W.send_(Client, "x", 1, 0);
    Sleep(10);
  }
  CHECK(SendResult == SOCKET_ERROR);
  // The client shut down sending above, so this is Winsock's code for that.
  CHECK(W.lasterror_() == WSAESHUTDOWN);

  // Name lookup fails with a Winsock code (which one depends on whether this
  // machine has a resolver to ask).
  addrinfo * Result = NULL;
  const int Lookup = W.getaddrinfo_("nonexistent.invalid", NULL, NULL, &Result);
  CHECK((Lookup == WSAHOST_NOT_FOUND) || (Lookup == WSATRY_AGAIN) || (Lookup == WSANO_DATA));
  CHECK(Result == NULL);

  W.closesocket_(Client);
  W.closesocket_(Listener);
  CloseHandle(ListenEvent);
  CloseHandle(ClientEvent);
  CloseHandle(ServerEvent);

  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
