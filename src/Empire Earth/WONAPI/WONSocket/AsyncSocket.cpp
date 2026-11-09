// WONAPI AsyncSocket: non-blocking TCP/UDP socket wrapper (file grouping inferred). The socket's state and its
// platform socket calls are Rust's (rust/crates/ee_won/src/socket.rs, the core mCore); these keep the lock, the
// virtual hooks, the destination address and the message transforms.
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

// FUNCTION: WON 0x006b0340
// Creates a closed socket of the given type (0 stream, 1 datagram) with a 4-byte length field and counts it
AsyncSocket::AsyncSocket(int inType)
{
    ee_won_socket_init(&this->mCore, inType);
    this->mLengthFieldSize = 4;
    this->mThreadCloseCallback = NULL;
}

// FUNCTION: WON 0x006b0420
// Returns a new, unopened socket of the same type
AsyncSocket* AsyncSocket::Duplicate()
{
    return new AsyncSocket(this->mCore.kind);
}

// FUNCTION: WON 0x006b04a0
// Destroys the socket: closes it, deletes every message transform and uncounts it
AsyncSocket::~AsyncSocket()
{
    this->Close();
    for (std::list<MsgTransform*>::iterator anItr = this->mMsgTransformQueue.begin();
         anItr != this->mMsgTransformQueue.end(); ++anItr)
        delete *anItr;
    ee_won_socket_destroyed(&this->mCore);
}

// FUNCTION: WON 0x006b0600
// Sets the size in bytes of the length prefix on framed messages
void AsyncSocket::SetLengthFieldSize(unsigned char inSize)
{
    this->mLengthFieldSize = inSize;
}

// FUNCTION: WON 0x006b0620
// Appends a message transform (e.g. encryption) to the socket's transform chain
void AsyncSocket::AddMsgTransform(MsgTransform* inTransform)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mMsgTransformQueue.push_back(inTransform);
}

// FUNCTION: WON 0x006b0730
// Runs a received message through the transform chain in reverse order; returns the first failure
WONStatus AsyncSocket::TransformRecvMsg(ByteBufferPtr& inMessage)
{
    AutoCrit aCrit(this->mDataCrit);
    std::list<MsgTransform*>::iterator anItr = this->mMsgTransformQueue.end();
    if (anItr != this->mMsgTransformQueue.begin()) {
        do {
            --anItr;
            WONStatus aStatus = (*anItr)->Recv(inMessage);
            if (aStatus != 0)
                return aStatus;
        } while (anItr != this->mMsgTransformQueue.begin());
    }
    return 0;
}

// FUNCTION: WON 0x006b07e0
// Runs an outgoing message through the transform chain in order; returns the first failure
WONStatus AsyncSocket::TransformSendMsg(ByteBufferPtr& inMessage)
{
    AutoCrit aCrit(this->mDataCrit);
    for (std::list<MsgTransform*>::iterator anItr = this->mMsgTransformQueue.begin();
         anItr != this->mMsgTransformQueue.end(); ++anItr) {
        WONStatus aStatus = (*anItr)->Send(inMessage);
        if (aStatus != 0)
            return aStatus;
    }
    return 0;
}

// FUNCTION: WON 0x006b0890
// Returns the last socket error (Winsock's numbering), mapping would-block/in-progress to WS_TimedOut and recording
// any other
WONStatus AsyncSocket::GetLastSocketError()
{
    return ee_won_socket_last_error(&this->mCore);
}

// FUNCTION: WON 0x006b08c0
// Returns whether the socket holds an open platform socket
bool AsyncSocket::IsValid()
{
    return this->mCore.handle != PLAT_SOCKET_INVALID;
}

// FUNCTION: WON 0x006b08d0
// Returns whether the socket has been connected to a destination
bool AsyncSocket::IsConnected()
{
    return this->mCore.connected;
}

// FUNCTION: WON 0x006b08e0
// Returns the address the socket is connected to
const IPAddr& AsyncSocket::GetDestAddr()
{
    return this->mDestAddr;
}

// FUNCTION: WON 0x006b1360
// Blocks up to the given milliseconds until the socket is writable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForWrite(unsigned long inWaitTime)
{
    if (inWaitTime == 0)
        return;
    AutoCrit aCrit(this->mDataCrit);
    PlatSocket aSocket = this->mCore.handle;
    aCrit.Leave();
    ee_won_socket_wait(aSocket, PLAT_SOCKET_WAIT_WRITE, static_cast<uint32_t>(inWaitTime));
}

// FUNCTION: WON 0x006b1410
// Blocks up to the given milliseconds until the socket is readable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForRead(unsigned long inWaitTime)
{
    if (inWaitTime == 0)
        return;
    AutoCrit aCrit(this->mDataCrit);
    PlatSocket aSocket = this->mCore.handle;
    aCrit.Leave();
    ee_won_socket_wait(aSocket, PLAT_SOCKET_WAIT_READ, static_cast<uint32_t>(inWaitTime));
}

// Lengths and counts are signed int, as in the original
// FUNCTION: WON 0x006b14c0
// Sends bytes on the socket; returns WS_TimedOut (or PartialSendTo for datagrams) on a short send
// and records when sends first started blocking
WONStatus AsyncSocket::Send(const void* inData, int inLength, int* outSent)
{
    AutoCrit aCrit(this->mDataCrit);
    return ee_won_socket_send(&this->mCore, inData, inLength, outSent);
}

// FUNCTION: WON 0x006b15e0
// Receives bytes from the socket; returns Shutdown when the peer closed, WS_TimedOut on a short stream read
WONStatus AsyncSocket::Recv(void* outData, int inLength, int* outReceived)
{
    AutoCrit aCrit(this->mDataCrit);
    return ee_won_socket_recv(&this->mCore, outData, inLength, outReceived);
}

// FUNCTION: WON 0x006b1a10
// Shuts down and closes the handle (forcing a hard close if it would block) and marks the socket closed
void AsyncSocket::ClosePrv()
{
    ee_won_socket_close(&this->mCore);
}

// FUNCTION: WON 0x006b1ad0
// Closes the socket after notifying the socket thread and running the close hook
void AsyncSocket::Close()
{
    if (this->mThreadCloseCallback != NULL)
        this->mThreadCloseCallback(this);
    AutoCrit aCrit(this->mDataCrit);
    this->CloseHook();
    this->ClosePrv();
}

// FUNCTION: WON 0x006b1b50
// Closes the socket after notifying the socket thread and running the close and kill hooks
void AsyncSocket::Kill()
{
    if (this->mThreadCloseCallback != NULL)
        this->mThreadCloseCallback(this);
    AutoCrit aCrit(this->mDataCrit);
    this->CloseHook();
    this->KillHook();
    this->ClosePrv();
}

// FUNCTION: WON 0x006b1700
// Sends a datagram to an address, opening the socket first if needed; fails on stream sockets
WONStatus AsyncSocket::SendTo(const void* inData, int inLength, const IPAddr& inAddress)
{
    AutoCrit aCrit(this->mDataCrit);
    WONStatus aStatus = ee_won_socket_datagram_only(&this->mCore);
    if (aStatus != 0)
        return aStatus;
    if (!this->IsValid()) {
        aStatus = this->Open();
        if (aStatus != 0)
            return aStatus;
    }
    return ee_won_socket_send_to(&this->mCore, inData, inLength, AddressOf(inAddress), PortOf(inAddress));
}

// FUNCTION: WON 0x006b1840
// Receives a datagram and its sender's address; fails on stream sockets
WONStatus AsyncSocket::RecvFrom(void* outData, int inLength, IPAddr* outAddress, int* outReceived)
{
    AutoCrit aCrit(this->mDataCrit);
    uint32_t anAddress = 0;
    uint16_t aPort = 0;
    bool aFrom = false;
    WONStatus aStatus = ee_won_socket_recv_from(&this->mCore, outData, inLength, &anAddress, &aPort, &aFrom,
                                                outReceived);
    if (aFrom && outAddress != NULL)
        *outAddress = MakeAddr(anAddress, aPort);
    return aStatus;
}

// FUNCTION: WON 0x006b1980
// Closes a stream socket; dissolves a datagram socket's association (closing it if that fails)
void AsyncSocket::Disconnect()
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->mCore.kind != 0) {
        if (!ee_won_socket_dissolve_peer(&this->mCore))
            this->Close();
    }
    else
        this->Close();
}

// FUNCTION: WON 0x006b0a90
// Opens a fresh non-blocking TCP or UDP handle (broadcast on for UDP, no linger for TCP) and runs the open hook
WONStatus AsyncSocket::Open()
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->IsValid())
        this->Close();
    WONStatus aStatus = ee_won_socket_open(&this->mCore);
    if (aStatus != 0)
        return aStatus;
    this->OpenHook();
    return 0;
}

// FUNCTION: WON 0x006b0c90
// Binds the socket to a local address, opening it first and optionally allowing address reuse
WONStatus AsyncSocket::Bind(const IPAddr& inAddress, bool inReuse)
{
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid()) {
        WONStatus aStatus = this->Open();
        if (aStatus != 0) return aStatus;
    }
    return ee_won_socket_bind(&this->mCore, AddressOf(inAddress), PortOf(inAddress), inReuse);
}

// FUNCTION: WON 0x006b0e60
// Starts connecting to an address: drops an existing stream connection, opens and binds to the local host if needed
WONStatus AsyncSocket::Fn2B0E60(const IPAddr& inAddress)
{
    if (!inAddress.IsValid())
        return WS_InvalidAddress;
    AutoCrit aCrit(this->mDataCrit);
    if (this->mCore.kind == 0 && this->mCore.connected)
        this->Disconnect();
    if (!this->IsValid()) {
        WONStatus aStatus = this->Open();
        if (aStatus != 0) return aStatus;
        if (WONAPI::IPAddr::GetLocalHost() != 0) {
            aStatus = this->Fn2B0D90(0, 0);
            if (aStatus != 0) return aStatus;
        }
    }
    this->mDestAddr = inAddress;
    return ee_won_socket_connect(&this->mCore, AddressOf(inAddress), PortOf(inAddress));
}

// FUNCTION: WON 0x006b0fc0
// Puts an open stream socket into listening mode
WONStatus AsyncSocket::Listen(int inBacklog)
{
    AutoCrit aCrit(this->mDataCrit);
    return ee_won_socket_listen(&this->mCore, inBacklog);
}

// FUNCTION: WON 0x006b1060
// Accepts a pending connection into a new non-blocking socket of the same type, connected to the caller
WONStatus AsyncSocket::Accept(SmartPtr<AsyncSocket>& outSocket)
{
    AutoCrit aCrit(this->mDataCrit);
    PlatSocket aSocket = PLAT_SOCKET_INVALID;
    uint32_t anAddress = 0;
    uint16_t aPort = 0;
    WONStatus aStatus = ee_won_socket_accept(&this->mCore, &aSocket, &anAddress, &aPort);
    if (aStatus != 0)
        return aStatus;
    outSocket = this->Duplicate();
    ee_won_socket_adopt(&outSocket->mCore, aSocket);
    outSocket->mDestAddr = MakeAddr(anAddress, aPort);
    return 0;
}

// FUNCTION: WON 0x006b1220
// Waits up to the given milliseconds for a connect to finish; returns success, the socket error or WS_TimedOut
WONStatus AsyncSocket::WaitForConnect(unsigned long inWaitTime)
{
    AutoCrit aCrit(this->mDataCrit);
    PlatSocket aSocket = this->mCore.handle;
    aCrit.Leave();
    return ee_won_socket_wait_for_connect(aSocket, static_cast<uint32_t>(inWaitTime));
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
    return this->mLengthFieldSize;
}
// FUNCTION: WON 0x006b08f0
// Returns the local port the socket is bound to, or 0
unsigned short AsyncSocket::GetLocalPort()
{
    return ee_won_socket_local_port(&this->mCore);
}
// FUNCTION: WON 0x006b0a50
// Turns datagram broadcast on or off for the socket
WONStatus AsyncSocket::SetBroadcast(bool inAllowBroadcast)
{
    return ee_won_socket_set_broadcast(&this->mCore, inAllowBroadcast);
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
    return (SocketType)this->mCore.kind;
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
	AutoCrit aCrit(this->mDataCrit);
	std::list<MsgTransform*>::iterator anItr = this->mMsgTransformQueue.begin();
	while(anItr!=this->mMsgTransformQueue.end())
	{
		delete *anItr;
		++anItr;
	}
	this->mMsgTransformQueue.clear();
}

bool AsyncSocket::IsOpen()
{
	return this->mCore.open;
}

void AsyncSocket::SetType(SocketType theType)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mCore.kind!=theType)
		this->Close();

	this->mCore.kind = theType;
}

WONStatus AsyncSocket::SetNagling(bool on)
{
	return ee_won_socket_set_nagling(&this->mCore, on);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b0480
// (public: virtual void * __thiscall WONAPI::AsyncSocket::`scalar deleting destructor'(unsigned int)    [??_GAsyncSocket@WONAPI@@UAEPAXI@Z])
