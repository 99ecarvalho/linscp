// Windows, message queues and window-message sockets, as FileZilla uses them.
//
// FileZilla runs its sockets through a hidden helper window per thread
// (WSAAsyncSelect, layer notifications, a timer) and its worker thread on a
// GetMessage loop fed by PostThreadMessage. On Windows this runs against
// user32 and ws2_32 (under Wine when cross-compiling), on POSIX against the
// emulation, with the same assertions -- the order messages come out in, what
// the filters select, and Winsock's re-enabling rules decide whether FileZilla
// hangs or spins.
#if defined(_WIN32)
  #include <winsock2.h>
  #include <windows.h>
#else
  #include "win32_on_posix.h"
  #include "winsock2.h"
#endif

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int Failures = 0;

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);             \
      ++Failures;                                                             \
    }                                                                         \
  } while (0)

const UINT MsgA = WM_USER + 1;
const UINT MsgB = WM_USER + 2;
const UINT MsgThread = WM_USER + 3;
const UINT MsgLate = WM_USER + 5;
const UINT MsgWorker = WM_USER + 10;
const UINT MsgListen = WM_USER + 20;
const UINT MsgClient = WM_USER + 21;
const UINT MsgServer = WM_USER + 22;
const UINT MsgLookup = WM_USER + 30;

std::vector<UINT> Seen;

LRESULT CALLBACK Proc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
  if ((Message == WM_NCCREATE) || (Message == WM_CREATE) || (Message == WM_DESTROY) ||
      (Message == WM_NCDESTROY) || (Message == MsgA))
  {
    Seen.push_back(Message);
  }
  if ((Message == MsgA) && (WParam == 1) && (LParam == 2))
  {
    return 42;
  }
  return DefWindowProc(Window, Message, WParam, LParam);
}

int IndexOf(UINT Message)
{
  for (std::size_t I = 0; I < Seen.size(); ++I)
  {
    if (Seen[I] == Message)
    {
      return static_cast<int>(I);
    }
  }
  return -1;
}

// Pumps Window's messages numbered Message for up to Timeout ms, OR-ing the
// select events seen for Socket until all of Wanted have been; errors land
// in Errors by event bit.
long Collect(HWND Window, UINT Message, SOCKET Socket, long Wanted, DWORD Timeout, int * Errors = nullptr)
{
  long Got = 0;
  const DWORD Start = GetTickCount();
  while (((Got & Wanted) != Wanted) && (GetTickCount() - Start < Timeout))
  {
    MSG M;
    if (PeekMessage(&M, Window, Message, Message, PM_REMOVE))
    {
      if (M.wParam == Socket)
      {
        const long Event = WSAGETSELECTEVENT(M.lParam);
        Got |= Event;
        if (Errors != nullptr)
        {
          for (int Bit = 0; Bit < FD_MAX_EVENTS; ++Bit)
          {
            if (Event == (1L << Bit))
            {
              Errors[Bit] = WSAGETSELECTERROR(M.lParam);
            }
          }
        }
      }
    }
    else
    {
      Sleep(5);
    }
  }
  return Got;
}

// The select events for Socket posted within Timeout ms, all of them.
long Drain(HWND Window, UINT Message, SOCKET Socket, DWORD Timeout)
{
  return Collect(Window, Message, Socket, ~0L, Timeout);
}

HANDLE GReady;
HANDLE GGo;
int GWorkerCount = 0;
BOOL GWorkerQuit = TRUE;

DWORD WINAPI Worker(LPVOID)
{
  // No queue until a message function is first called.
  WaitForSingleObject(GGo, INFINITE);
  MSG M;
  PeekMessage(&M, NULL, 0, 0, PM_NOREMOVE);
  SetEvent(GReady);
  BOOL R;
  while ((R = GetMessage(&M, NULL, 0, 0)) != 0)
  {
    if ((M.hwnd == NULL) && (M.message == MsgWorker))
    {
      ++GWorkerCount;
    }
  }
  GWorkerQuit = (M.message == WM_QUIT) ? FALSE : TRUE;
  return 0;
}

// Names each section as it starts, so a hang shows where.
void Section(const char * Name)
{
  std::printf("-- %s\n", Name);
  std::fflush(stdout);
}

}  // namespace

int main()
{
  // --- a window ----------
  Section("a window");
  WNDCLASSEX Class;
  std::memset(&Class, 0, sizeof(Class));
  Class.cbSize = sizeof(Class);
  Class.lpfnWndProc = Proc;
  Class.hInstance = GetModuleHandle(NULL);
  Class.lpszClassName = L"linscp test window";
  CHECK(RegisterClassEx(&Class) != 0);
  CHECK(RegisterClassEx(&Class) == 0);
  CHECK(GetLastError() == ERROR_CLASS_ALREADY_EXISTS);

  HWND Window = CreateWindow(L"LINSCP TEST WINDOW", L"", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);
  CHECK(Window != NULL);
  CHECK(IsWindow(Window));
  CHECK(IndexOf(WM_NCCREATE) >= 0);
  CHECK(IndexOf(WM_CREATE) > IndexOf(WM_NCCREATE));
  CHECK(SetWindowLongPtr(Window, GWLP_USERDATA, 1234) == 0);
  CHECK(GetWindowLongPtr(Window, GWLP_USERDATA) == 1234);
  CHECK(SetWindowLongPtr(Window, GWLP_USERDATA, 99) == 1234);

  // --- posting and filters ----------
  Section("posting and filters");
  MSG M;
  CHECK(PostMessage(Window, MsgA, 1, 2));
  CHECK(PostMessage(Window, MsgB, 3, 4));
  CHECK(PostThreadMessage(GetCurrentThreadId(), MsgThread, 5, 6));
  // A range picks MsgB past MsgA; without PM_REMOVE it stays.
  CHECK(PeekMessage(&M, Window, MsgB, MsgB, PM_NOREMOVE) && (M.message == MsgB) && (M.wParam == 3));
  // (HWND)-1: thread messages only.
  CHECK(PeekMessage(&M, reinterpret_cast<HWND>(static_cast<LONG_PTR>(-1)), WM_USER, WM_USER + 100, PM_REMOVE));
  CHECK((M.hwnd == NULL) && (M.message == MsgThread) && (M.lParam == 6));
  // In order, then nothing.
  CHECK(PeekMessage(&M, NULL, WM_USER, WM_USER + 100, PM_REMOVE) && (M.message == MsgA) && (M.hwnd == Window));
  Seen.clear();
  CHECK(DispatchMessage(&M) == 42);
  CHECK(IndexOf(MsgA) == 0);
  CHECK(PeekMessage(&M, NULL, WM_USER, WM_USER + 100, PM_REMOVE) && (M.message == MsgB));
  CHECK(!PeekMessage(&M, NULL, WM_USER, WM_USER + 100, PM_REMOVE));

  // --- timers ----------
  Section("timers");
  CHECK(SetTimer(Window, 7, 30, NULL) != 0);
  const DWORD TimerStart = GetTickCount();
  CHECK(GetMessage(&M, Window, WM_TIMER, WM_TIMER) && (M.message == WM_TIMER) && (M.wParam == 7));
  CHECK(GetTickCount() - TimerStart >= 15);
  CHECK(KillTimer(Window, 7));
  CHECK(!KillTimer(Window, 7));
  // A due timer waits behind posted messages.
  Section("timer behind posted messages");
  CHECK(SetTimer(Window, 8, 10, NULL) != 0);
  Sleep(60);
  CHECK(PostMessage(Window, MsgLate, 0, 0));
  CHECK(PeekMessage(&M, Window, 0, 0, PM_REMOVE) && (M.message == MsgLate));
  CHECK(PeekMessage(&M, Window, 0, 0, PM_REMOVE) && (M.message == WM_TIMER) && (M.wParam == 8));
  CHECK(KillTimer(Window, 8));
  // A thread timer gets its id from SetTimer and fires with no window.
  Section("thread timer");
  const UINT_PTR ThreadTimer = SetTimer(NULL, 0, 20, NULL);
  CHECK(ThreadTimer != 0);
  Sleep(60);
  // Due, but not a thread message: (HWND)-1 selects posted ones only.
  CHECK(!PeekMessage(&M, reinterpret_cast<HWND>(static_cast<LONG_PTR>(-1)), WM_TIMER, WM_TIMER, PM_NOREMOVE));
  CHECK(GetMessage(&M, NULL, WM_TIMER, WM_TIMER));
  CHECK((M.hwnd == NULL) && (M.wParam == ThreadTimer));
  CHECK(KillTimer(NULL, ThreadTimer));

  // --- registered messages ----------
  Section("registered messages");
  const UINT Registered = RegisterWindowMessage(L"linscp-test-message");
  CHECK(Registered >= 0xC000);
  CHECK(RegisterWindowMessage(L"LINSCP-TEST-MESSAGE") == Registered);
  CHECK(RegisterWindowMessage(L"linscp-other-message") != Registered);

  // --- another thread's queue ----------
  Section("another thread's queue");
  GReady = CreateEvent(NULL, FALSE, FALSE, NULL);
  GGo = CreateEvent(NULL, FALSE, FALSE, NULL);
  DWORD WorkerId = 0;
  HANDLE Thread = CreateThread(NULL, 0, Worker, NULL, 0, &WorkerId);
  CHECK(Thread != NULL);
  // It has no queue yet.
  CHECK(!PostThreadMessage(WorkerId, MsgWorker, 0, 0));
  CHECK(GetLastError() == ERROR_INVALID_THREAD_ID);
  SetEvent(GGo);
  CHECK(WaitForSingleObject(GReady, 5000) == WAIT_OBJECT_0);
  for (int I = 0; I < 3; ++I)
  {
    CHECK(PostThreadMessage(WorkerId, MsgWorker, 0, 0));
  }
  CHECK(PostThreadMessage(WorkerId, WM_QUIT, 0, 0));
  CHECK(WaitForSingleObject(Thread, 5000) == WAIT_OBJECT_0);
  CHECK(GWorkerCount == 3);
  CHECK(GWorkerQuit == FALSE);
  CloseHandle(Thread);

  // --- WSAAsyncSelect ----------
  Section("WSAAsyncSelect");
  WSADATA Data;
  CHECK(WSAStartup(MAKEWORD(2, 2), &Data) == 0);
  SOCKET Listener = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in Address;
  std::memset(&Address, 0, sizeof(Address));
  Address.sin_family = AF_INET;
  Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  CHECK(bind(Listener, reinterpret_cast<sockaddr *>(&Address), sizeof(Address)) == 0);
  CHECK(listen(Listener, 4) == 0);
  int Len = sizeof(Address);
  CHECK(getsockname(Listener, reinterpret_cast<sockaddr *>(&Address), &Len) == 0);
  CHECK(WSAAsyncSelect(Listener, Window, MsgListen, FD_ACCEPT) == 0);

  SOCKET Client = socket(AF_INET, SOCK_STREAM, 0);
  CHECK(WSAAsyncSelect(Client, Window, MsgClient, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == 0);
  const int Connect = connect(Client, reinterpret_cast<sockaddr *>(&Address), sizeof(Address));
  CHECK((Connect == 0) || (WSAGetLastError() == WSAEWOULDBLOCK));
  int Errors[FD_MAX_EVENTS];
  std::memset(Errors, 0xFF, sizeof(Errors));
  CHECK(Collect(Window, MsgClient, Client, FD_CONNECT | FD_WRITE, 3000, Errors) == (FD_CONNECT | FD_WRITE));
  CHECK(Errors[FD_CONNECT_BIT] == 0);
  // FD_WRITE is not posted again until a send would have blocked.
  CHECK((Drain(Window, MsgClient, Client, 200) & FD_WRITE) == 0);

  CHECK(Collect(Window, MsgListen, Listener, FD_ACCEPT, 3000) == FD_ACCEPT);
  SOCKET Server = accept(Listener, NULL, NULL);
  CHECK(Server != INVALID_SOCKET);
  CHECK(WSAAsyncSelect(Server, Window, MsgServer, FD_READ | FD_CLOSE) == 0);

  CHECK(send(Client, "hello", 5, 0) == 5);
  CHECK(Collect(Window, MsgServer, Server, FD_READ, 3000) == FD_READ);
  // FD_READ once, until recv is called...
  CHECK((Drain(Window, MsgServer, Server, 200) & FD_READ) == 0);
  char Buffer[16];
  CHECK(recv(Server, Buffer, 2, 0) == 2);
  // ...when, data remaining, it is posted again.
  CHECK(Collect(Window, MsgServer, Server, FD_READ, 3000) == FD_READ);
  CHECK(recv(Server, Buffer + 2, static_cast<int>(sizeof(Buffer)) - 2, 0) == 3);
  CHECK(std::memcmp(Buffer, "hello", 5) == 0);
  CHECK(recv(Server, Buffer, static_cast<int>(sizeof(Buffer)), 0) == SOCKET_ERROR);
  CHECK(WSAGetLastError() == WSAEWOULDBLOCK);
  // Winsock's error is Win32's last error, which FileZilla reads.
  CHECK(GetLastError() == static_cast<DWORD>(WSAEWOULDBLOCK));

  CHECK(shutdown(Client, SD_SEND) == 0);
  std::memset(Errors, 0xFF, sizeof(Errors));
  CHECK((Collect(Window, MsgServer, Server, FD_CLOSE, 3000, Errors) & FD_CLOSE) != 0);
  CHECK(Errors[FD_CLOSE_BIT] == 0);
  CHECK(recv(Server, Buffer, static_cast<int>(sizeof(Buffer)), 0) == 0);
  closesocket(Server);
  closesocket(Client);
  closesocket(Listener);

  // --- WSAAsyncGetHostByName ----------
  Section("WSAAsyncGetHostByName");
  std::vector<char> HostBuffer(MAXGETHOSTSTRUCT);
  HANDLE Lookup = WSAAsyncGetHostByName(Window, MsgLookup, "localhost", HostBuffer.data(), MAXGETHOSTSTRUCT);
  CHECK(Lookup != NULL);
  bool Replied = false;
  for (DWORD Start = GetTickCount(); !Replied && (GetTickCount() - Start < 10000);)
  {
    if (PeekMessage(&M, Window, MsgLookup, MsgLookup, PM_REMOVE))
    {
      Replied = true;
      CHECK(M.wParam == reinterpret_cast<WPARAM>(Lookup));
      CHECK(WSAGETASYNCERROR(M.lParam) == 0);
      CHECK(WSAGETASYNCBUFLEN(M.lParam) > sizeof(hostent));
      const hostent * Host = reinterpret_cast<const hostent *>(HostBuffer.data());
      CHECK(Host->h_addrtype == AF_INET);
      CHECK(Host->h_length == 4);
      CHECK(Host->h_addr_list[0] != NULL);
      if (Host->h_addr_list[0] != NULL)
      {
        in_addr A;
        std::memcpy(&A, Host->h_addr_list[0], sizeof(A));
        CHECK(A.s_addr == htonl(INADDR_LOOPBACK));
        // Everything lies within the caller's buffer.
        CHECK((Host->h_addr_list[0] > HostBuffer.data()) &&
              (Host->h_addr_list[0] < HostBuffer.data() + HostBuffer.size()));
      }
    }
    else
    {
      Sleep(5);
    }
  }
  CHECK(Replied);
#if !defined(_WIN32)
  // A finished request can no longer be cancelled.
  CHECK(WSACancelAsyncRequest(Lookup) == SOCKET_ERROR);
  CHECK(WSAGetLastError() == WSAEALREADY);
#endif
  WSACleanup();

  // --- destruction ----------
  Section("destruction");
  Seen.clear();
  CHECK(DestroyWindow(Window));
  CHECK(IndexOf(WM_DESTROY) >= 0);
  CHECK(IndexOf(WM_NCDESTROY) > IndexOf(WM_DESTROY));
  CHECK(!IsWindow(Window));
  CHECK(!PostMessage(Window, MsgA, 0, 0));
  CHECK(GetLastError() == ERROR_INVALID_WINDOW_HANDLE);

  if (Failures == 0)
  {
    std::printf("OK\n");
  }
  return Failures == 0 ? 0 : 1;
}
