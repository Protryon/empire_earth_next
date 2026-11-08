// WONAPI AsyncSocket: non-blocking Winsock TCP/UDP socket wrapper (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x006b0340
// Creates a closed socket of the given type (0 stream, 1 datagram) with a 4-byte length field and counts it
AsyncSocket::AsyncSocket(int inType)
{
    this->mSocket = INVALID_SOCKET;
    this->mLengthFieldSize = 4;
    this->mType = inType;
    this->mConnected = false;
    this->mOpen = false;
    this->mThreadCloseCallback = NULL;
    this->mFirstBlockedSendTime = 0;
    this->mLastRecvTime = time(NULL);
    this->mStatus = WS_None;
    NetStats::IncrementNumConstructedSockets(1);
}

// FUNCTION: WON 0x006b0420
// Returns a new, unopened socket of the same type
AsyncSocket* AsyncSocket::Duplicate()
{
    return new AsyncSocket(this->mType);
}

// FUNCTION: WON 0x006b04a0
// Destroys the socket: closes it, deletes every message transform and uncounts it
AsyncSocket::~AsyncSocket()
{
    this->Close();
    for (std::list<MsgTransform*>::iterator anItr = this->mMsgTransformQueue.begin();
         anItr != this->mMsgTransformQueue.end(); ++anItr)
        delete *anItr;
    NetStats::IncrementNumConstructedSockets(-1);
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
// Returns the last Winsock error, mapping would-block/in-progress to WS_TimedOut and recording any other
WONStatus AsyncSocket::GetLastSocketError()
{
    WONStatus aStatus = WSAGetLastError();
    if (aStatus == WSAEWOULDBLOCK || aStatus == WSAEINPROGRESS)
        return WS_TimedOut;
    this->mStatus = aStatus;
    return aStatus;
}

// FUNCTION: WON 0x006b08c0
// Returns whether the socket holds an open Winsock handle
bool AsyncSocket::IsValid()
{
    return this->mSocket != INVALID_SOCKET;
}

// FUNCTION: WON 0x006b08d0
// Returns whether the socket has been connected to a destination
bool AsyncSocket::IsConnected()
{
    return this->mConnected;
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
    if (!this->IsValid())
        return;
    timeval aTime;
    aTime.tv_sec = inWaitTime / 1000;
    aTime.tv_usec = (inWaitTime % 1000) * 1000;
    fd_set aSet;
    FD_ZERO(&aSet);
    FD_SET(this->mSocket, &aSet);
    aCrit.Leave();
    select(FD_SETSIZE, NULL, &aSet, NULL, &aTime);
}

// FUNCTION: WON 0x006b1410
// Blocks up to the given milliseconds until the socket is readable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForRead(unsigned long inWaitTime)
{
    if (inWaitTime == 0)
        return;
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid())
        return;
    timeval aTime;
    aTime.tv_sec = inWaitTime / 1000;
    aTime.tv_usec = (inWaitTime % 1000) * 1000;
    fd_set aSet;
    FD_ZERO(&aSet);
    FD_SET(this->mSocket, &aSet);
    aCrit.Leave();
    select(FD_SETSIZE, &aSet, NULL, NULL, &aTime);
}

// Lengths and counts are signed int, as in the original
// FUNCTION: WON 0x006b14c0
// Sends bytes on the socket; returns WS_TimedOut (or PartialSendTo for datagrams) on a short send
// and records when sends first started blocking
WONStatus AsyncSocket::Send(const void* inData, int inLength, int* outSent)
{
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid())
        return WS_AsyncSocket_InvalidSocket;
    int aSent = send(this->mSocket, static_cast<const char*>(inData), inLength, 0);
    if (aSent < 0) {
        if (this->mFirstBlockedSendTime == 0)
            this->mFirstBlockedSendTime = time(NULL);
        return this->GetLastSocketError();
    }
    this->mFirstBlockedSendTime = 0;
    NetStats::IncrementBytesSent(aSent);
    if (outSent != NULL)
        *outSent = aSent;
    if (aSent < inLength) {
        if (this->mType == 1)
            return WS_AsyncSocket_PartialSendTo;
        return WS_TimedOut;
    }
    return WS_Success;
}

// FUNCTION: WON 0x006b15e0
// Receives bytes from the socket; returns Shutdown when the peer closed, WS_TimedOut on a short stream read
WONStatus AsyncSocket::Recv(void* outData, int inLength, int* outReceived)
{
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid())
        return WS_AsyncSocket_InvalidSocket;
    int aReceived = recv(this->mSocket, static_cast<char*>(outData), inLength, 0);
    if (aReceived < 0)
        return this->GetLastSocketError();
    if (aReceived == 0)
        return WS_AsyncSocket_Shutdown;
    NetStats::IncrementBytesReceived(aReceived);
    this->mLastRecvTime = time(NULL);
    if (outReceived != NULL)
        *outReceived = aReceived;
    if (aReceived != inLength && this->mType == 0)
        return WS_TimedOut;
    return WS_Success;
}

// FUNCTION: WON 0x006b1a10
// Shuts down and closes the Winsock handle (forcing a hard close if it would block) and marks the socket closed
void AsyncSocket::ClosePrv()
{
    if (this->IsValid()) {
        shutdown(this->mSocket, 1); // Winsock1 send-only shutdown, 2b1a26
        if (closesocket(this->mSocket) != 0 && this->GetLastSocketError() == WS_TimedOut) {
            linger aLinger;
            aLinger.l_onoff = 1;
            aLinger.l_linger = 0;
            setsockopt(this->mSocket, SOL_SOCKET, SO_LINGER,
                reinterpret_cast<const char*>(&aLinger), sizeof(aLinger));
            closesocket(this->mSocket);
        }
        this->mSocket = INVALID_SOCKET;
        NetStats::IncrementNumOpenSockets(-1);
    }
    this->mConnected = false;
    this->mOpen = false;
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
    if (this->mType == 0)
        return WS_AsyncSocket_StreamNotAllowed;
    if (!this->IsValid()) {
        WONStatus aStatus = this->Open();
        if (aStatus != 0)
            return aStatus;
    }
    int aSent = sendto(this->mSocket, static_cast<const char*>(inData), inLength, 0,
        reinterpret_cast<const sockaddr*>(&inAddress.GetSockAddrIn()), 16);
    if (aSent < 0)
        return this->GetLastSocketError();
    NetStats::IncrementBytesSent(aSent);
    if (aSent == inLength)
        return WS_Success;
    return WS_AsyncSocket_PartialSendTo;
}

// FUNCTION: WON 0x006b1840
// Receives a datagram and its sender's address; fails on stream sockets
WONStatus AsyncSocket::RecvFrom(void* outData, int inLength, IPAddr* outAddress, int* outReceived)
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->mType == 0)
        return WS_AsyncSocket_StreamNotAllowed;
    if (!this->IsValid())
        return WS_AsyncSocket_InvalidSocket;
    sockaddr_in aFrom;
    int aFromLength = 16;
    int aReceived = recvfrom(this->mSocket, static_cast<char*>(outData), inLength, 0,
        reinterpret_cast<sockaddr*>(&aFrom), &aFromLength);
    if (aReceived >= 0) {
        NetStats::IncrementBytesReceived(aReceived);
        if (outAddress != NULL)
            outAddress->Fn2B2B00(aFrom);
        if (outReceived != NULL)
            *outReceived = aReceived;
        return WS_Success;
    }
    return this->GetLastSocketError();
}

// FUNCTION: WON 0x006b1980
// Closes a stream socket; dissolves a datagram socket's association (closing it if that fails)
void AsyncSocket::Disconnect()
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->mType != 0) {
        sockaddr_in anAddress;
        memset(&anAddress, 0, sizeof(anAddress));
        if (connect(this->mSocket, reinterpret_cast<const sockaddr*>(&anAddress), 16) != 0)
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
    this->mOpen = true;
    switch (this->mType) {
    case 0:
        this->mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    case 1:
        this->mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        break;
    }
    if (this->mSocket == INVALID_SOCKET)
        return this->GetLastSocketError();
    this->mStatus = WS_None;
    NetStats::IncrementNumOpenSockets(1);
    linger aLinger;
    int aBroadcast;
    if (this->mType == 0) {
        aLinger.l_onoff = 0;
        if (setsockopt(this->mSocket, SOL_SOCKET, SO_LINGER,
            reinterpret_cast<const char*>(&aLinger), 4) == SOCKET_ERROR)
            return this->GetLastSocketError();
    }
    if (this->mType == 1) {
        aBroadcast = 1;
        if (setsockopt(this->mSocket, SOL_SOCKET, SO_BROADCAST,
            reinterpret_cast<const char*>(&aBroadcast), 4) == SOCKET_ERROR)
            return this->GetLastSocketError();
    }
    unsigned long aNonBlocking = 1;
    if (ioctlsocket(this->mSocket, FIONBIO, &aNonBlocking) == SOCKET_ERROR)
        return this->GetLastSocketError();
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
    if (inReuse) {
        int aReuse = 1;
        if (setsockopt(this->mSocket, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&aReuse), 4) != 0)
            return this->GetLastSocketError();
    }
    if (bind(this->mSocket, reinterpret_cast<const sockaddr*>(&inAddress.GetSockAddrIn()), 16) != 0)
        return this->GetLastSocketError();
    return 0;
}

// FUNCTION: WON 0x006b0e60
// Starts connecting to an address: drops an existing stream connection, opens and binds to the local host if needed
WONStatus AsyncSocket::Fn2B0E60(const IPAddr& inAddress)
{
    if (!inAddress.IsValid())
        return WS_InvalidAddress;
    AutoCrit aCrit(this->mDataCrit);
    if (this->mType == 0 && this->mConnected)
        this->Disconnect();
    if (this->mSocket == INVALID_SOCKET) {
        WONStatus aStatus = this->Open();
        if (aStatus != 0) return aStatus;
        if (WONAPI::IPAddr::GetLocalHost() != 0) {
            aStatus = this->Fn2B0D90(0, 0);
            if (aStatus != 0) return aStatus;
        }
    }
    this->mDestAddr = inAddress;
    this->mConnected = true;
    if (connect(this->mSocket, reinterpret_cast<const sockaddr*>(&inAddress.GetSockAddrIn()), 16) == SOCKET_ERROR)
        return this->GetLastSocketError();
    return WS_Success;
}

// FUNCTION: WON 0x006b0fc0
// Puts an open stream socket into listening mode
WONStatus AsyncSocket::Listen(int inBacklog)
{
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid()) return WS_AsyncSocket_InvalidSocket;
    if (this->mType == 1) return WS_AsyncSocket_DatagramNotAllowed;
    if (listen(this->mSocket, inBacklog) != 0)
        return this->GetLastSocketError();
    return WS_Success;
}

// FUNCTION: WON 0x006b1060
// Accepts a pending connection into a new non-blocking socket of the same type, connected to the caller
WONStatus AsyncSocket::Accept(SmartPtr<AsyncSocket>& outSocket)
{
    AutoCrit aCrit(this->mDataCrit);
    sockaddr_in aFrom;
    int aFromLength = 16;
    SOCKET aSocket = accept(this->mSocket, reinterpret_cast<sockaddr*>(&aFrom), &aFromLength);
    if (aSocket == INVALID_SOCKET)
        return this->GetLastSocketError();
    linger aLinger;
    aLinger.l_onoff = 0;
    setsockopt(aSocket, SOL_SOCKET, SO_LINGER,
        reinterpret_cast<const char*>(&aLinger), 4);
    unsigned long aNonBlocking = 1;
    ioctlsocket(aSocket, FIONBIO, &aNonBlocking);
    outSocket = this->Duplicate();
    outSocket->mSocket = aSocket;
    outSocket->mDestAddr = IPAddr(aFrom);
    outSocket->mOpen = true;
    outSocket->mConnected = true;
    NetStats::IncrementNumOpenSockets(1);
    return 0;
}

// FUNCTION: WON 0x006b1220
// Waits up to the given milliseconds for a connect to finish; returns success, the socket error or WS_TimedOut
WONStatus AsyncSocket::WaitForConnect(unsigned long inWaitTime)
{
    AutoCrit aCrit(this->mDataCrit);
    if (!this->IsValid()) return WS_AsyncSocket_InvalidSocket;
    SOCKET aSocket = this->mSocket;
    aCrit.Leave();
    timeval aTime;
    aTime.tv_sec = inWaitTime / 1000;
    aTime.tv_usec = (inWaitTime % 1000) * 1000;
    fd_set aWriteSet;
    fd_set anExceptSet;
    FD_ZERO(&aWriteSet);
    FD_ZERO(&anExceptSet);
    FD_SET(aSocket, &aWriteSet);
    FD_SET(aSocket, &anExceptSet);
    int aResult = select(FD_SETSIZE, NULL, &aWriteSet, &anExceptSet, &aTime);
    int anError = 0;
    int aLength = 4;
    if (FD_ISSET(aSocket, &anExceptSet)) {
        getsockopt(this->mSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&anError), &aLength);
        return anError == 0 ? 100021 : anError;
    }
    if (aResult == 1)
        return WS_Success;
    return WS_TimedOut;
}

// The SDK declares the second parameter bool
// FUNCTION: WON 0x006b0d90
// Binds the socket to the local host address on the given port
WONStatus AsyncSocket::Fn2B0D90(unsigned short inPort, bool inReuse)
{
    sockaddr_in anAddress;
    memset(&anAddress, 0, sizeof(anAddress));
    anAddress.sin_family = AF_INET;
    anAddress.sin_addr.s_addr = htonl(WONAPI::IPAddr::GetLocalHost());
    anAddress.sin_port = htons(inPort);
    return this->Bind(IPAddr(anAddress), inReuse);
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
    if(!this->IsValid()) return 0;
    sockaddr address;
    int length=sizeof(address);
    int result=getsockname(this->mSocket,&address,&length);
    if(result!=0) return 0;
    return ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
}
// FUNCTION: WON 0x006b0a50
// Turns datagram broadcast on or off for the socket
WONStatus AsyncSocket::SetBroadcast(bool inAllowBroadcast)
{
    int option=inAllowBroadcast;
    int result=setsockopt(this->mSocket,SOL_SOCKET,SO_BROADCAST,
        reinterpret_cast<const char*>(&option),sizeof(option));
    if(result==SOCKET_ERROR) return this->GetLastSocketError();
    return 0;
}
}

namespace WONAPI {
// SDK static counter; in the original a zero-initialized DWORD at 565f34, no CRT constructor
int AsyncSocket::mWinsockStartCount=0;
// FUNCTION: WON 0x006b0940
// Starts Winsock 1.1 on first use and counts the user; returns the WSAStartup error
WONStatus AsyncSocket::StartWinsock()
{
    if(AsyncSocket::mWinsockStartCount!=0)
    {
        ++AsyncSocket::mWinsockStartCount;
        return 0;
    }
    WSADATA data;
    int result=WSAStartup(MAKEWORD(1,1),&data);
    if(result!=0) return result;
    ++AsyncSocket::mWinsockStartCount;
    return 0;
}
// FUNCTION: WON 0x006b0980
// Releases one Winsock user and cleans Winsock up when the last one goes
WONStatus AsyncSocket::StopWinsock()
{
    if(AsyncSocket::mWinsockStartCount<=0 || --AsyncSocket::mWinsockStartCount!=0)
        return 0;
    return WSACleanup();
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
    return (SocketType)this->mType;
}
}

// No original address identified: WON SDK AsyncSocket methods
namespace WONAPI {
int AsyncSocket::TimeLeft(DWORD theTotalTime, DWORD theStartTime)
{
	int deltaTime = GetTickCount() - theStartTime;
	if(deltaTime >= theTotalTime)
		return 0;
	else
		return theTotalTime - deltaTime;
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
	return this->mOpen;
}

void AsyncSocket::SetType(SocketType theType) 
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mType!=theType)
		this->Close();

	this->mType = theType;
}

WONStatus AsyncSocket::SetNagling(bool on) 
{
	int aVal, aSockOpt = !on;
	aVal = setsockopt(this->mSocket,IPPROTO_TCP,TCP_NODELAY,(char*) &aSockOpt,sizeof(aSockOpt));

	if(aVal==SOCKET_ERROR) 
		return this->GetLastSocketError();
	else
		return (WONStatus)0;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b0480
// (public: virtual void * __thiscall WONAPI::AsyncSocket::`scalar deleting destructor'(unsigned int)    [??_GAsyncSocket@WONAPI@@UAEPAXI@Z])
