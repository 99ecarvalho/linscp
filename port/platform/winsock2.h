//---------------------------------------------------------------------------
// Winsock names mapped onto BSD sockets.
//
// Core touches Winsock in two ways. Most of it is ordinary BSD socket work
// under different names, which this file aliases directly. The rest is the
// event model -- WSAEventSelect, WSAEnumNetworkEvents and the FD_* bits -- which
// has no BSD equivalent and needs a real event loop behind it; that is task
// P1-08, and the declarations here exist so the code compiles before it lands.
//
// Error codes keep their Winsock values rather than being mapped to errno,
// because core compares against WSAEWOULDBLOCK and friends by name and the
// FileZilla layer stores them in its own state.
//---------------------------------------------------------------------------
#ifndef LINSCP_WINSOCK2_H
#define LINSCP_WINSOCK2_H

#if defined(_WIN32)
  #error "This is the POSIX stand-in; on Windows the real Win32 header must be used. \
Do not put port/platform on the include path for a Windows target."
#endif

#include "win32_on_posix.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// On Windows a SOCKET is an opaque handle; on POSIX it is a descriptor. Keeping
// it unsigned matches Win32, and INVALID_SOCKET is the all-ones value there.
typedef uintptr_t SOCKET;
#define INVALID_SOCKET (((SOCKET)(~0)))
#define SOCKET_ERROR (-1)

typedef HANDLE WSAEVENT;
typedef HANDLE * LPWSAEVENT;
#define WSA_INVALID_EVENT (((WSAEVENT)0))
#define WSA_MAXIMUM_WAIT_EVENTS 64
#define WSA_WAIT_FAILED (((DWORD)(-1)))
#define WSA_WAIT_TIMEOUT 258
#define WSA_WAIT_EVENT_0 0

#define FD_SETSIZE_WINSOCK 64
#define FD_MAX_EVENTS 10

// Network event bits, matching Winsock's values.
#define FD_READ_BIT     0
#define FD_WRITE_BIT    1
#define FD_OOB_BIT      2
#define FD_ACCEPT_BIT   3
#define FD_CONNECT_BIT  4
#define FD_CLOSE_BIT    5

#define FD_READ      (1 << FD_READ_BIT)
#define FD_WRITE     (1 << FD_WRITE_BIT)
#define FD_OOB       (1 << FD_OOB_BIT)
#define FD_ACCEPT    (1 << FD_ACCEPT_BIT)
#define FD_CONNECT   (1 << FD_CONNECT_BIT)
#define FD_CLOSE     (1 << FD_CLOSE_BIT)

typedef struct _WSANETWORKEVENTS
{
  long lNetworkEvents;
  int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS;
typedef WSANETWORKEVENTS * LPWSANETWORKEVENTS;
typedef struct hostent HOSTENT;
typedef struct hostent * LPHOSTENT;
// The LPARAM a select reply carries: the event in the low word, the error
// in the high one, as WSAAsyncSelect packs them.
#define WSAMAKESELECTREPLY(event, error) ((LPARAM)MAKELONG(event, error))
#define WSAGETSELECTEVENT(lParam) LOWORD(lParam)
#define WSAGETSELECTERROR(lParam) HIWORD(lParam)
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in * PSOCKADDR_IN;
typedef struct sockaddr_in * LPSOCKADDR_IN;
typedef struct sockaddr_in6 SOCKADDR_IN6;
typedef struct in_addr IN_ADDR;
typedef struct in_addr * LPIN_ADDR;
// Winsock's name for the bytes of an IPv6 address (in6addr.h); glibc's is
// s6_addr, over the same 16 bytes.
#define s6_bytes s6_addr
typedef struct sockaddr_storage SOCKADDR_STORAGE;

typedef struct WSAData
{
  WORD wVersion;
  WORD wHighVersion;
  char szDescription[257];
  char szSystemStatus[129];
  unsigned short iMaxSockets;
  unsigned short iMaxUdpDg;
  char * lpVendorInfo;
} WSADATA;

// Winsock error codes. Kept at their Winsock values on purpose: core and the
// FileZilla layer compare against these by name and carry them around.
#define WSABASEERR              10000
#define WSAEINTR                (WSABASEERR + 4)
#define WSAEBADF                (WSABASEERR + 9)
#define WSAEACCES               (WSABASEERR + 13)
#define WSAEFAULT               (WSABASEERR + 14)
#define WSAEINVAL               (WSABASEERR + 22)
#define WSAEMFILE               (WSABASEERR + 24)
#define WSAEWOULDBLOCK          (WSABASEERR + 35)
#define WSAEINPROGRESS          (WSABASEERR + 36)
#define WSAEALREADY             (WSABASEERR + 37)
#define WSAENOTSOCK             (WSABASEERR + 38)
#define WSAEDESTADDRREQ         (WSABASEERR + 39)
#define WSAEMSGSIZE             (WSABASEERR + 40)
#define WSAEADDRINUSE           (WSABASEERR + 48)
#define WSAEADDRNOTAVAIL        (WSABASEERR + 49)
#define WSAENETDOWN             (WSABASEERR + 50)
#define WSAENETUNREACH          (WSABASEERR + 51)
#define WSAENETRESET            (WSABASEERR + 52)
#define WSAECONNABORTED         (WSABASEERR + 53)
#define WSAECONNRESET           (WSABASEERR + 54)
#define WSAENOBUFS              (WSABASEERR + 55)
#define WSAEISCONN              (WSABASEERR + 56)
#define WSAENOTCONN             (WSABASEERR + 57)
#define WSAESHUTDOWN            (WSABASEERR + 58)
#define WSAETIMEDOUT            (WSABASEERR + 60)
#define WSAECONNREFUSED         (WSABASEERR + 61)
#define WSAEHOSTDOWN            (WSABASEERR + 64)
#define WSAEHOSTUNREACH         (WSABASEERR + 65)
#define WSAEPROTOTYPE           (WSABASEERR + 41)
#define WSAENOPROTOOPT          (WSABASEERR + 42)
#define WSAEPROTONOSUPPORT      (WSABASEERR + 43)
#define WSAESOCKTNOSUPPORT      (WSABASEERR + 44)
#define WSAEPFNOSUPPORT         (WSABASEERR + 46)
#define WSAEAFNOSUPPORT         (WSABASEERR + 47)
#define WSAEPROCLIM             (WSABASEERR + 67)
#define WSANOTINITIALISED       (WSABASEERR + 93)
#define WSAEDISCON              (WSABASEERR + 101)
#define WSAHOST_NOT_FOUND       (WSABASEERR + 1001)
#define WSATRY_AGAIN            (WSABASEERR + 1002)
#define WSANO_RECOVERY          (WSABASEERR + 1003)
#define WSANO_DATA              (WSABASEERR + 1004)

// Translate errno into the Winsock code with the same meaning.
int WinsockErrorFromErrno(int Err);

int WSAGetLastError();
void WSASetLastError(int Error);
int WSAStartup(WORD VersionRequested, WSADATA * Data);
int WSACleanup();

int closesocket(SOCKET S);
int ioctlsocket(SOCKET S, LONG Cmd, ULONG * Argp);

// shutdown's directions.
#define SD_RECEIVE 0
#define SD_SEND    1
#define SD_BOTH    2
// Windows' exclusive binding has no POSIX equivalent beyond the default:
// a POSIX listener is already exclusive unless SO_REUSEPORT is set.
#define SO_EXCLUSIVEADDRUSE ((int)(~SO_REUSEADDR))

typedef struct sockaddr * LPSOCKADDR;
typedef WSADATA * LPWSADATA;
typedef struct _WSAPROTOCOL_INFOA WSAPROTOCOL_INFOA;
typedef WSAPROTOCOL_INFOA * LPWSAPROTOCOL_INFO;
// The address as text, "a.b.c.d:port" or "[v6]:port" when the port is set.
int WSAAddressToStringA(LPSOCKADDR Address, DWORD AddressLength, LPWSAPROTOCOL_INFO ProtocolInfo,
  LPSTR String, LPDWORD StringLength);
// Notification by window message, as FileZilla's sockets use: each event
// is posted to Window as Message, wParam the socket and lParam
// WSAMAKESELECTREPLY(event, error), under the same re-enabling rules as
// WSAEventSelect, which it replaces (and is replaced by). Events 0 cancels.
int WSAAsyncSelect(SOCKET S, HWND Window, UINT Message, LONG Events);

// An IPv4 host lookup on another thread. The reply is posted to Window as
// Message, wParam the returned handle and lParam
// WSAMAKEASYNCREPLY(length needed, error), with a hostent laid out in
// Buffer (h_addr the first address). After WSACancelAsyncRequest nothing is
// posted and Buffer is not written; cancelling a finished request fails with
// WSAEALREADY.
#define MAXGETHOSTSTRUCT 1024
#define WSAMAKEASYNCREPLY(buflen, error) ((LPARAM)MAKELONG(buflen, error))
#define WSAGETASYNCBUFLEN(lParam) LOWORD(lParam)
#define WSAGETASYNCERROR(lParam) HIWORD(lParam)
#define WSAEALREADY (WSABASEERR + 37)
HANDLE WSAAsyncGetHostByName(HWND Window, UINT Message, const char * Name, char * Buffer, int BufferLength);
int WSACancelAsyncRequest(HANDLE AsyncTaskHandle);

// The local interfaces' addresses (SIO_GET_INTERFACE_LIST), from getifaddrs.
#define SIO_GET_INTERFACE_LIST 0x4004747F
// iiFlags uses Winsock's IFF_* values, which are not defined here: they
// differ from <net/if.h>'s, and neon includes that.
typedef union sockaddr_gen
{
  struct sockaddr Address;
  struct sockaddr_in AddressIn;
  struct sockaddr_in6 AddressIn6;
} sockaddr_gen;
typedef struct _INTERFACE_INFO
{
  ULONG iiFlags;
  sockaddr_gen iiAddress;
  sockaddr_gen iiBroadcastAddress;
  sockaddr_gen iiNetmask;
} INTERFACE_INFO;

// The event model (win32_winsock_posix.cpp).
WSAEVENT WSACreateEvent();
BOOL WSACloseEvent(WSAEVENT Event);
BOOL WSAResetEvent(WSAEVENT Event);
BOOL WSASetEvent(WSAEVENT Event);
int WSAEventSelect(SOCKET S, WSAEVENT Event, long NetworkEvents);
int WSAEnumNetworkEvents(SOCKET S, WSAEVENT Event, WSANETWORKEVENTS * Events);
DWORD WSAWaitForMultipleEvents(DWORD Count, const WSAEVENT * Events,
                               BOOL WaitAll, DWORD Timeout, BOOL Alertable);

#define FIONBIO_WINSOCK 0x8004667E

// Socket ioctls. Linux has no ideal-send-backlog query: WSAIoctl fails with
// WSAEOPNOTSUPP, and core keeps its send buffer as configured.
#define SIO_IDEAL_SEND_BACKLOG_QUERY 0x4004747B
#define WSAEOPNOTSUPP (WSABASEERR + 45)
typedef void * LPWSAOVERLAPPED;
typedef void * LPWSAOVERLAPPED_COMPLETION_ROUTINE;
// The ws2_32 module's exports by name, for GetProcAddress: the socket
// functions with Winsock's signatures and semantics (see
// win32_winsock_posix.cpp), or null.
FARPROC linscp_Ws2Proc(LPCSTR Name);

int WSAIoctl(SOCKET S, DWORD IoControlCode, LPVOID InBuffer, DWORD InSize, LPVOID OutBuffer,
  DWORD OutSize, LPDWORD BytesReturned, LPWSAOVERLAPPED Overlapped,
  LPWSAOVERLAPPED_COMPLETION_ROUTINE Completion);

#ifdef __cplusplus
}  // extern "C"

// The socket functions with Winsock's signatures, for C++ that calls them
// directly (FileZilla) rather than through ws2_32's GetProcAddress (PuTTY):
// the same functions the module exports, so errors are Winsock codes in
// WSAGetLastError, a pending connect is WSAEWOULDBLOCK, sends never raise
// SIGPIPE and the selections' re-enabling rules apply. They overload libc's
// on the SOCKET argument, so a call on a SOCKET reaches them and one on a
// descriptor libc's. Where an argument would match libc's better -- a buffer
// as void * -- the call is deleted rather than quietly taking BSD semantics.
// socket, gethostbyname, getaddrinfo and the rest share libc's signatures,
// so libc's serve; FileZilla tests only their results. C++ linkage even
// where the header is included inside extern "C" (core includes PuTTY's).
extern "C++" {
int connect(SOCKET S, const struct sockaddr * Name, int NameLen);
int bind(SOCKET S, const struct sockaddr * Name, int NameLen);
int listen(SOCKET S, int Backlog);
SOCKET accept(SOCKET S, struct sockaddr * Address, int * AddressLen);
int send(SOCKET S, const char * Buffer, int Length, int Flags);
int send(SOCKET S, const void * Buffer, int Length, int Flags) = delete;
int recv(SOCKET S, char * Buffer, int Length, int Flags);
int recv(SOCKET S, void * Buffer, int Length, int Flags) = delete;
int shutdown(SOCKET S, int How);
int getsockopt(SOCKET S, int Level, int Name, char * Value, int * Length);
int setsockopt(SOCKET S, int Level, int Name, const char * Value, int Length);
int getpeername(SOCKET S, struct sockaddr * Name, int * NameLen);
int getsockname(SOCKET S, struct sockaddr * Name, int * NameLen);
}
#endif

#endif // LINSCP_WINSOCK2_H
