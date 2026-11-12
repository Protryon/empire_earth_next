// WONAPI AsyncSocket: non-blocking TCP/UDP socket wrapper (file grouping inferred). A Rust object since round 14
// (rust/crates/ee_won/src/sockobj.rs EeWonAsyncSocket: the core, the length prefix, the message transforms, the close
// callback and every step below; since round 15 the lock, which the subclasses take as well, and the destination
// address); the C++ class keeps the virtual hooks the object calls back.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "platform/PlatSocket.h"
// The socket error codes WON reports are Winsock's; the platform layer reports them in that numbering everywhere
static_assert(WS_WSAEINTR == PLAT_SOCK_EINTR, "Winsock numbering");
static_assert(WS_WSAEBADF == PLAT_SOCK_EBADF, "Winsock numbering");
static_assert(WS_WSAEACCES == PLAT_SOCK_EACCES, "Winsock numbering");
static_assert(WS_WSAEFAULT == PLAT_SOCK_EFAULT, "Winsock numbering");
static_assert(WS_WSAEINVAL == PLAT_SOCK_EINVAL, "Winsock numbering");
static_assert(WS_WSAEMFILE == PLAT_SOCK_EMFILE, "Winsock numbering");
static_assert(WS_WSAEWOULDBLOCK == PLAT_SOCK_EWOULDBLOCK, "Winsock numbering");
static_assert(WS_WSAEINPROGRESS == PLAT_SOCK_EINPROGRESS, "Winsock numbering");
static_assert(WS_WSAEALREADY == PLAT_SOCK_EALREADY, "Winsock numbering");
static_assert(WS_WSAENOTSOCK == PLAT_SOCK_ENOTSOCK, "Winsock numbering");
static_assert(WS_WSAEDESTADDRREQ == PLAT_SOCK_EDESTADDRREQ, "Winsock numbering");
static_assert(WS_WSAEMSGSIZE == PLAT_SOCK_EMSGSIZE, "Winsock numbering");
static_assert(WS_WSAEPROTOTYPE == PLAT_SOCK_EPROTOTYPE, "Winsock numbering");
static_assert(WS_WSAENOPROTOOPT == PLAT_SOCK_ENOPROTOOPT, "Winsock numbering");
static_assert(WS_WSAEPROTONOSUPPORT == PLAT_SOCK_EPROTONOSUPPORT, "Winsock numbering");
static_assert(WS_WSAESOCKTNOSUPPORT == PLAT_SOCK_ESOCKTNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEOPNOTSUPP == PLAT_SOCK_EOPNOTSUPP, "Winsock numbering");
static_assert(WS_WSAEPFNOSUPPORT == PLAT_SOCK_EPFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEAFNOSUPPORT == PLAT_SOCK_EAFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEADDRINUSE == PLAT_SOCK_EADDRINUSE, "Winsock numbering");
static_assert(WS_WSAEADDRNOTAVAIL == PLAT_SOCK_EADDRNOTAVAIL, "Winsock numbering");
static_assert(WS_WSAENETDOWN == PLAT_SOCK_ENETDOWN, "Winsock numbering");
static_assert(WS_WSAENETUNREACH == PLAT_SOCK_ENETUNREACH, "Winsock numbering");
static_assert(WS_WSAENETRESET == PLAT_SOCK_ENETRESET, "Winsock numbering");
static_assert(WS_WSAECONNABORTED == PLAT_SOCK_ECONNABORTED, "Winsock numbering");
static_assert(WS_WSAECONNRESET == PLAT_SOCK_ECONNRESET, "Winsock numbering");
static_assert(WS_WSAENOBUFS == PLAT_SOCK_ENOBUFS, "Winsock numbering");
static_assert(WS_WSAEISCONN == PLAT_SOCK_EISCONN, "Winsock numbering");
static_assert(WS_WSAENOTCONN == PLAT_SOCK_ENOTCONN, "Winsock numbering");
static_assert(WS_WSAESHUTDOWN == PLAT_SOCK_ESHUTDOWN, "Winsock numbering");
static_assert(WS_WSAETOOMANYREFS == PLAT_SOCK_ETOOMANYREFS, "Winsock numbering");
static_assert(WS_WSAETIMEDOUT == PLAT_SOCK_ETIMEDOUT, "Winsock numbering");
static_assert(WS_WSAECONNREFUSED == PLAT_SOCK_ECONNREFUSED, "Winsock numbering");
static_assert(WS_WSAELOOP == PLAT_SOCK_ELOOP, "Winsock numbering");
static_assert(WS_WSAENAMETOOLONG == PLAT_SOCK_ENAMETOOLONG, "Winsock numbering");
static_assert(WS_WSAEHOSTDOWN == PLAT_SOCK_EHOSTDOWN, "Winsock numbering");
static_assert(WS_WSAEHOSTUNREACH == PLAT_SOCK_EHOSTUNREACH, "Winsock numbering");
static_assert(WS_WSAENOTEMPTY == PLAT_SOCK_ENOTEMPTY, "Winsock numbering");
static_assert(WS_WSAEPROCLIM == PLAT_SOCK_EPROCLIM, "Winsock numbering");
static_assert(WS_WSAEUSERS == PLAT_SOCK_EUSERS, "Winsock numbering");
static_assert(WS_WSAEDQUOT == PLAT_SOCK_EDQUOT, "Winsock numbering");
static_assert(WS_WSAESTALE == PLAT_SOCK_ESTALE, "Winsock numbering");
static_assert(WS_WSAEREMOTE == PLAT_SOCK_EREMOTE, "Winsock numbering");
static_assert(WS_WSASYSNOTREADY == PLAT_SOCK_SYSNOTREADY, "Winsock numbering");
static_assert(WS_WSAVERNOTSUPPORTED == PLAT_SOCK_VERNOTSUPPORTED, "Winsock numbering");
static_assert(WS_WSANOTINITIALISED == PLAT_SOCK_NOTINITIALISED, "Winsock numbering");
static_assert(WS_WSAEDISCON == PLAT_SOCK_EDISCON, "Winsock numbering");
namespace WONAPI {

static uint32_t AddressOf(const IPAddr& inAddress) { return inAddress.GetSockAddrIn().mAddress; }
static uint16_t PortOf(const IPAddr& inAddress) { return inAddress.GetSockAddrIn().mPort; }
static IPAddr MakeAddr(uint32_t inAddress, uint16_t inPort)
{
    PlatSockAddrIn anAddress;
    memset(&anAddress, 0, sizeof(anAddress));
    anAddress.mFamily = PLAT_AF_INET;
    anAddress.mAddress = inAddress;
    anAddress.mPort = inPort;
    return IPAddr(anAddress);
}

// What the socket object calls back: the virtual hooks, the transforms' Send, Recv and deletion
static void HookOpen(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->OpenHook(); }
static void HookClose(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->CloseHook(); }
static void HookKill(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->KillHook(); }
static long HookTransformSend(void* inTransform, void* inMessage)
{ return static_cast<MsgTransform*>(inTransform)->Send(*static_cast<ByteBufferPtr*>(inMessage)); }
static long HookTransformRecv(void* inTransform, void* inMessage)
{ return static_cast<MsgTransform*>(inTransform)->Recv(*static_cast<ByteBufferPtr*>(inMessage)); }
static void HookTransformFree(void* inTransform) { delete static_cast<MsgTransform*>(inTransform); }
static const EeWonAsyncSocketHooks kSocketHooks = {HookOpen, HookClose, HookKill, HookTransformSend,
                                                   HookTransformRecv, HookTransformFree};

// FUNCTION: WON 0x006b0340
// Creates a closed socket of the given type (0 stream, 1 datagram) with a 4-byte length field and counts it
AsyncSocket::AsyncSocket(int inType)
{
    this->mSock = ee_won_asyncsocket_new(static_cast<void*>(this), &kSocketHooks, inType);
}

// FUNCTION: WON 0x006b0420
// Returns a new, unopened socket of the same type
AsyncSocket* AsyncSocket::Duplicate()
{
    return new AsyncSocket(this->GetType());
}

// FUNCTION: WON 0x006b04a0
// Destroys the socket: closes it, deletes every message transform and uncounts it
AsyncSocket::~AsyncSocket()
{
    ee_won_asyncsocket_free(this->mSock);
}

// FUNCTION: WON 0x006b0600
// Sets the size in bytes of the length prefix on framed messages
void AsyncSocket::SetLengthFieldSize(unsigned char inSize)
{
    ee_won_asyncsocket_set_length_field_size(this->mSock, inSize);
}

// FUNCTION: WON 0x006b0620
// Appends a message transform (e.g. encryption) to the socket's transform chain
void AsyncSocket::AddMsgTransform(MsgTransform* inTransform)
{
    ee_won_asyncsocket_add_transform(this->mSock, inTransform);
}

// FUNCTION: WON 0x006b0730
// Runs a received message through the transform chain in reverse order; returns the first failure
WONStatus AsyncSocket::TransformRecvMsg(ByteBufferPtr& inMessage)
{
    return ee_won_asyncsocket_transform_recv(this->mSock, &inMessage);
}

// FUNCTION: WON 0x006b07e0
// Runs an outgoing message through the transform chain in order; returns the first failure
WONStatus AsyncSocket::TransformSendMsg(ByteBufferPtr& inMessage)
{
    return ee_won_asyncsocket_transform_send(this->mSock, &inMessage);
}

// FUNCTION: WON 0x006b0890
// Returns the last socket error (Winsock's numbering), mapping would-block/in-progress to WS_TimedOut and recording
// any other
WONStatus AsyncSocket::GetLastSocketError()
{
    return ee_won_asyncsocket_last_error(this->mSock);
}

// FUNCTION: WON 0x006b08c0
// Returns whether the socket holds an open platform socket
bool AsyncSocket::IsValid()
{
    return ee_won_asyncsocket_is_valid(this->mSock);
}

// FUNCTION: WON 0x006b08d0
// Returns whether the socket has been connected to a destination
bool AsyncSocket::IsConnected()
{
    return ee_won_asyncsocket_is_connected(this->mSock);
}

// FUNCTION: WON 0x006b08e0
// Returns the address the socket is connected to (a copy)
IPAddr AsyncSocket::GetDestAddr()
{
    return IPAddr::FromValue(ee_won_asyncsocket_dest_addr(this->mSock));
}

// FUNCTION: WON 0x006b1360
// Blocks up to the given milliseconds until the socket is writable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForWrite(unsigned long inWaitTime)
{
    ee_won_asyncsocket_wait(this->mSock, PLAT_SOCKET_WAIT_WRITE, static_cast<uint32_t>(inWaitTime));
}

// FUNCTION: WON 0x006b1410
// Blocks up to the given milliseconds until the socket is readable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForRead(unsigned long inWaitTime)
{
    ee_won_asyncsocket_wait(this->mSock, PLAT_SOCKET_WAIT_READ, static_cast<uint32_t>(inWaitTime));
}

// Lengths and counts are signed int, as in the original
// FUNCTION: WON 0x006b14c0
// Sends bytes on the socket; returns WS_TimedOut (or PartialSendTo for datagrams) on a short send
// and records when sends first started blocking
WONStatus AsyncSocket::Send(const void* inData, int inLength, int* outSent)
{
    return ee_won_asyncsocket_send(this->mSock, inData, inLength, outSent);
}

// FUNCTION: WON 0x006b15e0
// Receives bytes from the socket; returns Shutdown when the peer closed, WS_TimedOut on a short stream read
WONStatus AsyncSocket::Recv(void* outData, int inLength, int* outReceived)
{
    return ee_won_asyncsocket_recv(this->mSock, outData, inLength, outReceived);
}

// FUNCTION: WON 0x006b1a10
// Shuts down and closes the handle (forcing a hard close if it would block) and marks the socket closed
void AsyncSocket::ClosePrv()
{
    ee_won_asyncsocket_close_handle(this->mSock);
}

// FUNCTION: WON 0x006b1ad0
// Closes the socket after notifying the socket thread and running the close hook
void AsyncSocket::Close()
{
    ee_won_asyncsocket_close(this->mSock);
}

// FUNCTION: WON 0x006b1b50
// Closes the socket after notifying the socket thread and running the close and kill hooks
void AsyncSocket::Kill()
{
    ee_won_asyncsocket_kill(this->mSock);
}

// FUNCTION: WON 0x006b1700
// Sends a datagram to an address, opening the socket first if needed; fails on stream sockets
WONStatus AsyncSocket::SendTo(const void* inData, int inLength, const IPAddr& inAddress)
{
    return ee_won_asyncsocket_send_to(this->mSock, inData, inLength, AddressOf(inAddress), PortOf(inAddress));
}

// FUNCTION: WON 0x006b1840
// Receives a datagram and its sender's address; fails on stream sockets
WONStatus AsyncSocket::RecvFrom(void* outData, int inLength, IPAddr* outAddress, int* outReceived)
{
    uint32_t anAddress = 0;
    uint16_t aPort = 0;
    bool aFrom = false;
    WONStatus aStatus = ee_won_asyncsocket_recv_from(this->mSock, outData, inLength, &anAddress, &aPort, &aFrom,
                                                     outReceived);
    if (aFrom && outAddress != NULL)
        *outAddress = MakeAddr(anAddress, aPort);
    return aStatus;
}

// FUNCTION: WON 0x006b1980
// Closes a stream socket; dissolves a datagram socket's association (closing it if that fails)
void AsyncSocket::Disconnect()
{
    ee_won_asyncsocket_disconnect(this->mSock);
}

// FUNCTION: WON 0x006b0a90
// Opens a fresh non-blocking TCP or UDP handle (broadcast on for UDP, no linger for TCP) and runs the open hook
WONStatus AsyncSocket::Open()
{
    return ee_won_asyncsocket_open(this->mSock);
}

// FUNCTION: WON 0x006b0c90
// Binds the socket to a local address, opening it first and optionally allowing address reuse
WONStatus AsyncSocket::Bind(const IPAddr& inAddress, bool inReuse)
{
    return ee_won_asyncsocket_bind(this->mSock, AddressOf(inAddress), PortOf(inAddress), inReuse);
}

// FUNCTION: WON 0x006b0e60
// Starts connecting to an address: drops an existing stream connection, opens and binds to the local host if needed
WONStatus AsyncSocket::Fn2B0E60(const IPAddr& inAddress)
{
    return ee_won_asyncsocket_connect_to(this->mSock, inAddress.GetValue(),
                                         static_cast<uint32_t>(WONAPI::IPAddr::GetLocalHost()));
}

// FUNCTION: WON 0x006b0fc0
// Puts an open stream socket into listening mode
WONStatus AsyncSocket::Listen(int inBacklog)
{
    return ee_won_asyncsocket_listen(this->mSock, inBacklog);
}

// FUNCTION: WON 0x006b1060
// Accepts a pending connection into a new non-blocking socket of the same type, connected to the caller
WONStatus AsyncSocket::Accept(SmartPtr<AsyncSocket>& outSocket)
{
    AutoCrit aCrit(this->GetDataLock());
    PlatSocket aSocket = PLAT_SOCKET_INVALID;
    uint32_t anAddress = 0;
    uint16_t aPort = 0;
    WONStatus aStatus = ee_won_asyncsocket_accept(this->mSock, &aSocket, &anAddress, &aPort);
    if (aStatus != 0)
        return aStatus;
    outSocket = this->Duplicate();
    ee_won_asyncsocket_adopt(outSocket->mSock, aSocket, anAddress, aPort);
    return 0;
}

// FUNCTION: WON 0x006b1220
// Waits up to the given milliseconds for a connect to finish; returns success, the socket error or WS_TimedOut
WONStatus AsyncSocket::WaitForConnect(unsigned long inWaitTime)
{
    return ee_won_asyncsocket_wait_for_connect(this->mSock, static_cast<uint32_t>(inWaitTime));
}

// The SDK declares the second parameter bool
// FUNCTION: WON 0x006b0d90
// Binds the socket to the local host address on the given port
WONStatus AsyncSocket::Fn2B0D90(unsigned short inPort, bool inReuse)
{
    return this->Bind(IPAddr(WONAPI::IPAddr::GetLocalHost(), inPort), inReuse);
}

}

namespace WONAPI {
// FUNCTION: WON 0x006b0610
// Returns the size in bytes of the length prefix on framed messages
unsigned char AsyncSocket::GetLengthFieldSize() const
{
    return ee_won_asyncsocket_length_field_size(this->mSock);
}
// FUNCTION: WON 0x006b08f0
// Returns the local port the socket is bound to, or 0
unsigned short AsyncSocket::GetLocalPort()
{
    return ee_won_asyncsocket_local_port(this->mSock);
}
// FUNCTION: WON 0x006b0a50
// Turns datagram broadcast on or off for the socket
WONStatus AsyncSocket::SetBroadcast(bool inAllowBroadcast)
{
    return ee_won_asyncsocket_set_broadcast(this->mSock, inAllowBroadcast);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006b0940
// Starts the socket system on first use and counts the user; returns the error (Winsock's numbering)
WONStatus AsyncSocket::StartWinsock()
{
    return ee_won_socket_start();
}
// FUNCTION: WON 0x006b0980
// Releases one user and cleans the socket system up when the last one goes
WONStatus AsyncSocket::StopWinsock()
{
    return ee_won_socket_stop();
}
}

namespace WONAPI {

// FUNCTION: WON 0x006b1350
// Waits up to the given milliseconds for an incoming connection
void AsyncSocket::WaitForAccept(unsigned long timeout)
{
    this->WaitForRead(timeout);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006b0a40
// Returns the socket type (stream or datagram)
AsyncSocket::SocketType AsyncSocket::GetType()
{
    return (SocketType)ee_won_asyncsocket_kind(this->mSock);
}
}

// No original address identified: WON SDK AsyncSocket methods
namespace WONAPI {
int AsyncSocket::TimeLeft(DWORD theTotalTime, DWORD theStartTime)
{
	return static_cast<int>(ee_won_real_time_left(theStartTime, theTotalTime));
}

void AsyncSocket::RemoveAllMsgTransforms()
{
	ee_won_asyncsocket_remove_all_transforms(this->mSock);
}

bool AsyncSocket::IsOpen()
{
	return ee_won_asyncsocket_is_open(this->mSock);
}

void AsyncSocket::SetType(SocketType theType)
{
	ee_won_asyncsocket_set_type(this->mSock, theType);
}

WONStatus AsyncSocket::SetNagling(bool on)
{
	return ee_won_asyncsocket_set_nagling(this->mSock, on);
}

// the last socket error recorded (the original's +0x54)
WONStatus AsyncSocket::GetSocketStatus() const
{
	return ee_won_asyncsocket_status(this->mSock);
}

void AsyncSocket::SetThreadCloseCallback(CloseCallback inCallback)
{
	ee_won_asyncsocket_set_close_callback(this->mSock, reinterpret_cast<void*>(inCallback));
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b0480
// (public: virtual void * __thiscall WONAPI::AsyncSocket::`scalar deleting destructor'(unsigned int)    [??_GAsyncSocket@WONAPI@@UAEPAXI@Z])
