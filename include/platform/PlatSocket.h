// Platform layer: IPv4 sockets (see platform/Platform.h). The subset of Winsock the TCP protocol plug-in uses
// (src/Low-Level Engine/Network Engine/TCP Protocol): stream and datagram sockets, non-blocking mode, broadcast, a
// readiness event per socket (WSAEventSelect), name lookups and the network adapter list.
//
// Conventions: a function returning int -> nonzero on success; a byte count -> -1 on failure. After a failure
// PlatSocketLastError gives the reason in Winsock's numbering on every system (PLAT_SOCK_E*: the values the game
// compares and records, e.g. PLAT_SOCK_EWOULDBLOCK = WSAEWOULDBLOCK = 10035). Addresses are PlatSockAddrIn, laid out
// as Windows' sockaddr_in (16 bytes; port and address in network byte order), so the game's records holding one
// keep their size and meaning on every system.
//
// Readiness events: PlatSocketSelectEvent ties a socket to a PlatEvent (platform/PlatThread.h) that is set when the
// socket has data to read (PLAT_SOCKET_EVENT_READ) or a connection to accept (PLAT_SOCKET_EVENT_ACCEPT). As with
// WSAEventSelect the socket becomes non-blocking, the event is set again by a later receive/accept while more is
// waiting, and resetting the event is the caller's business.
#ifndef PLATFORM_PLATSOCKET_H
#define PLATFORM_PLATSOCKET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlatEvent;

typedef intptr_t PlatSocket;
#define PLAT_SOCKET_INVALID ((PlatSocket)-1)

// error codes (Winsock's values)
#define PLAT_SOCK_EINTR 10004
#define PLAT_SOCK_EBADF 10009
#define PLAT_SOCK_EACCES 10013
#define PLAT_SOCK_EFAULT 10014
#define PLAT_SOCK_EINVAL 10022
#define PLAT_SOCK_EMFILE 10024
#define PLAT_SOCK_EWOULDBLOCK 10035
#define PLAT_SOCK_EINPROGRESS 10036
#define PLAT_SOCK_EALREADY 10037
#define PLAT_SOCK_ENOTSOCK 10038
#define PLAT_SOCK_EMSGSIZE 10040
#define PLAT_SOCK_EOPNOTSUPP 10045
#define PLAT_SOCK_EADDRINUSE 10048
#define PLAT_SOCK_EADDRNOTAVAIL 10049
#define PLAT_SOCK_ENETDOWN 10050
#define PLAT_SOCK_ENETUNREACH 10051
#define PLAT_SOCK_ECONNABORTED 10053
#define PLAT_SOCK_ECONNRESET 10054
#define PLAT_SOCK_ENOBUFS 10055
#define PLAT_SOCK_EISCONN 10056
#define PLAT_SOCK_ENOTCONN 10057
#define PLAT_SOCK_ETIMEDOUT 10060
#define PLAT_SOCK_ECONNREFUSED 10061
#define PLAT_SOCK_EHOSTUNREACH 10065
#define PLAT_SOCK_NOTINITIALISED 10093
#define PLAT_SOCK_HOST_NOT_FOUND 11001
#define PLAT_SOCK_OTHER 10100               // a native error without a Winsock twin

#define PLAT_AF_INET 2
#define PLAT_INADDR_ANY 0u
#define PLAT_INADDR_NONE 0xFFFFFFFFu
#define PLAT_INADDR_BROADCAST 0xFFFFFFFFu

typedef struct PlatSockAddrIn {
    uint16_t mFamily;                       // PLAT_AF_INET
    uint16_t mPort;                         // network byte order
    uint32_t mAddress;                      // network byte order
    uint8_t mZero[8];
} PlatSockAddrIn;

typedef enum PlatSocketType { PLAT_SOCKET_STREAM = 1, PLAT_SOCKET_DGRAM = 2 } PlatSocketType;

// byte order (the network's is big-endian)
static inline uint16_t PlatHostToNet16(uint16_t inValue)
{
    const uint16_t theProbe = 1;
    return *(const uint8_t*)&theProbe ? (uint16_t)((inValue >> 8) | (inValue << 8)) : inValue;
}
static inline uint32_t PlatHostToNet32(uint32_t inValue)
{
    const uint16_t theProbe = 1;
    return *(const uint8_t*)&theProbe ? ((inValue >> 24) | ((inValue >> 8) & 0xFF00u) | ((inValue << 8) & 0xFF0000u)
                                         | (inValue << 24))
                                      : inValue;
}
#define PlatNetToHost16 PlatHostToNet16
#define PlatNetToHost32 PlatHostToNet32

// Starts / ends the socket system (Win32: WSAStartup 2.0 / WSACleanup; a no-op elsewhere) -> nonzero on success
int PlatSocketStartup(void);
void PlatSocketCleanup(void);
// The calling thread's last socket error (PLAT_SOCK_E*)
uint32_t PlatSocketLastError(void);

PlatSocket PlatSocketOpen(PlatSocketType inType);                 // an AF_INET socket, PLAT_SOCKET_INVALID on failure
int PlatSocketClose(PlatSocket inSocket);
int PlatSocketBind(PlatSocket inSocket, const PlatSockAddrIn* inAddress);
int PlatSocketListen(PlatSocket inSocket, int inBacklog);
PlatSocket PlatSocketAccept(PlatSocket inSocket, PlatSockAddrIn* outPeer);   // outPeer may be NULL
int PlatSocketConnect(PlatSocket inSocket, const PlatSockAddrIn* inAddress);
int PlatSocketGetName(PlatSocket inSocket, PlatSockAddrIn* outAddress);        // getsockname
int PlatSocketSetNonBlocking(PlatSocket inSocket, int inNonBlocking);
int PlatSocketSetBroadcast(PlatSocket inSocket, int inBroadcast);
// -> the bytes sent / received (0: the peer closed a stream), -1 on failure (PLAT_SOCK_EWOULDBLOCK: nothing yet)
int32_t PlatSocketSend(PlatSocket inSocket, const void* inData, uint32_t inLength);
int32_t PlatSocketRecv(PlatSocket inSocket, void* outData, uint32_t inLength);
int32_t PlatSocketSendTo(PlatSocket inSocket, const void* inData, uint32_t inLength, const PlatSockAddrIn* inTo);
int32_t PlatSocketRecvFrom(PlatSocket inSocket, void* outData, uint32_t inLength, PlatSockAddrIn* outFrom);

// Readiness events (see above): inMask PLAT_SOCKET_EVENT_* bits (Winsock's FD_READ / FD_ACCEPT values), 0 ends
// the association.
#define PLAT_SOCKET_EVENT_READ 0x01u
#define PLAT_SOCKET_EVENT_ACCEPT 0x08u
int PlatSocketSelectEvent(PlatSocket inSocket, struct PlatEvent* inEvent, uint32_t inMask);

// Names. GetHostName -> the machine's name. ResolveIPv4 -> how many IPv4 addresses inName has (up to inMax into
// outAddresses, network order; a dotted address resolves to itself), 0 on failure (PLAT_SOCK_HOST_NOT_FOUND).
// ReverseLookup -> the host name of an address. ParseIPv4 (inet_addr) -> the address of "a.b.c.d",
// PLAT_INADDR_NONE when it is not one. FormatIPv4 (inet_ntoa) -> "a.b.c.d" into outText (16 bytes).
int PlatSocketGetHostName(char* outName, uint32_t inSize);
uint32_t PlatSocketResolveIPv4(const char* inName, uint32_t* outAddresses, uint32_t inMax);
int PlatSocketReverseLookup(uint32_t inAddress, char* outName, uint32_t inSize);
uint32_t PlatSocketParseIPv4(const char* inText);
void PlatSocketFormatIPv4(uint32_t inAddress, char* outText);

// The network adapters with an IPv4 address, in the system's order (Win32: GetAdaptersInfo's) -> how many (up to
// inMax written).
typedef struct PlatNetAdapter {
    char mDescription[132];                 // a human-readable name (Win32: the adapter description)
    uint32_t mAddress;                      // its first IPv4 address, network order
    uint32_t mNetmask;                      // network order
} PlatNetAdapter;
uint32_t PlatSocketGetAdapters(PlatNetAdapter* outAdapters, uint32_t inMax);

#ifdef __cplusplus
}
#endif

#endif
