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

typedef struct WSANETWORKEVENTS
{
  long lNetworkEvents;
  int iErrorCode[FD_MAX_EVENTS];
} WSANETWORKEVENTS;

typedef struct WSADATA
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
int ioctlsocket(SOCKET S, long Cmd, unsigned long * Argp);

// The event model. Declared so core compiles; the implementation lands with
// P1-08, which replaces FileZilla's hidden-window message pump.
WSAEVENT WSACreateEvent();
BOOL WSACloseEvent(WSAEVENT Event);
BOOL WSAResetEvent(WSAEVENT Event);
BOOL WSASetEvent(WSAEVENT Event);
int WSAEventSelect(SOCKET S, WSAEVENT Event, long NetworkEvents);
int WSAEnumNetworkEvents(SOCKET S, WSAEVENT Event, WSANETWORKEVENTS * Events);
DWORD WSAWaitForMultipleEvents(DWORD Count, const WSAEVENT * Events,
                               BOOL WaitAll, DWORD Timeout, BOOL Alertable);

#define FIONBIO_WINSOCK 0x8004667E

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // LINSCP_WINSOCK2_H
