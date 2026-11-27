// WONAPI routing-server API as the game's WON front end (EEWonNet.cpp, 0xcd4ee..0xd3ff4) uses it. Class names
// come from RTTI (.?AVRoutingConnection@WONAPI@@, ...), members and offsets from EEWonNet.cpp's use; methods are
// named by address where the WONAPI name is unknown (provisional names). Requires <string>, <list>, <map>,
// ee/EEWonNet.h.
#ifndef EE_WONAPI_WONROUTING_H
#define EE_WONAPI_WONROUTING_H

#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONDriver.h"
#include <time.h>
#include <stdlib.h>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include EE_WON_DRV(Routing_crypto) // the drivers' crypto and peer data shims (no game user since R6 WON round 39)
#include <stdio.h> //FILE* member (RecvBytesOp::mFile)
#include "platform/PlatTime.h"
#include "rust/won_net/admin.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/debug.h"
#include "rust/won_net/ffi_net.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostnative.h"
#include "rust/won_net/httpcpp.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/qsock.h"
#include "rust/won_net/reqop.h"
#include "rust/won_net/routeconn.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routereq.h"
#include "rust/won_net/rustref.h"
#include "rust/won_net/dnstest.h"
#include "rust/won_net/servconn.h"
#include "rust/won_net/sockobj.h"
#include "rust/won_net/sockop.h"
#include "rust/won_net/sockops.h"
#include "rust/won_net/sockthread.h"
#include "rust/won_net/udpmgr.h"
#include "rust/won_net/udpops.h"
#include "rust/won_net/userstatus.h"

namespace WONAPI {

typedef long WONStatus;
enum AsyncRunMode { OP_MODE_ASYNC=1, OP_MODE_BLOCK=2 }; //32-bit enum (WON SDK)
typedef long RoutingOpType;
enum AuthType { AUTH_TYPE_NONE=0, AUTH_TYPE_SESSION=1, AUTH_TYPE_PERSISTENT=2, AUTH_TYPE_PERSISTENT_NOCRYPT=3 };

inline std::wstring DirList::Name(size_t inIndex) const
{ std::wstring s; ee_won_dirlist_text(this->mList, inIndex, EeWonDirText::Name, WonWideSink, &s); return s; }
inline std::wstring DirList::DisplayName(size_t inIndex) const
{ std::wstring s; ee_won_dirlist_text(this->mList, inIndex, EeWonDirText::DisplayName, WonWideSink, &s); return s; }

// the status text (ee_won's status table; WONStatus.cpp, 0x29deb0, is deleted): a WS_ name, or a number valid until
// this thread's next unnamed status
inline const char* WONStatusToString(WONStatus inStatus) { return ee_won_status_text(inStatus); }

// WON's millisecond tick (was GetTickCount): real time, wraps every 2^32 ms like GetTickCount; compare by unsigned
// 32-bit difference
inline uint32_t WONTickCount() { return (uint32_t)PlatTimeMilliseconds64(); }


template <class T> class CompletionBase;

class AsyncOp;
// the library's one table of C++ calls (AsyncOp.cpp): what the Rust objects still need the C++ for (references,
// virtual calls, the classes' news); every other call of theirs is answered in Rust (ee_won cppcore.rs)
const EeWonCoreHooks* WonCore();
const EeWonAsyncOpHooks* AsyncOpHooks(); // the ops' own table (AsyncOp.cpp: the virtual hooks, the core's queue and timer)

// AsyncOp: 0x44 bytes in the original (ctor 29ef80, dtor 29f000, three vtable slots). A Rust object since round 13
// (rust/crates/ee_won_net/src/opobj.rs EeWonAsyncOp: the run state, the lock, the completion's reference, the
// container, and the run, finish, kill and completion steps) behind this shim; the virtual hooks, the completion's
// call, the references and WONAPICore's queue and timer come back through AsyncOp.cpp's hook table. The timer
// thread's entry of the op is the Rust timer thread's (thread.rs)
class AsyncOp : public RefCount {
public:
    // starts the op in the mode with the timeout (async queued or at once, else blocking); returns its status
    WONStatus Run(long inMode,unsigned long inTimeout)       //29f500
    { return ee_won_asyncop_run(this->mOp,static_cast<int32_t>(inMode),static_cast<uint32_t>(inTimeout)); }

    // an idle op: an infinite timeout, async, no container or timer entry
    AsyncOp() { this->mOp=ee_won_asyncop_new(static_cast<void*>(this),AsyncOpHooks()); }
    virtual ~AsyncOp() { ee_won_asyncop_free(this->mOp); } // 0x29f000: the completion and lock go with it
    virtual void RunHook() {}
    virtual void CleanupHook() {}
    virtual bool CallbackHook(AsyncOp*,int) { return false; } // a tracked sub-op completed (2accc0: ignored)
    // (: AsyncOp's own vtable 0x84c834 has these three slots only; SocketOp and RoutingOp each introduce their
    // +0x0c/+0x10/+0x14: SocketOp's +0x14 returns 0, AcceptOp's a new AcceptOp; RoutingOp's +0x14 is GetType)

    WONStatus GetStatus() const { return ee_won_asyncop_status(this->mOp); }
    #include EE_WON_DRV(Routing_AsyncOp) // the drivers' members
    unsigned long GetTimeout() const { return ee_won_asyncop_timeout(this->mOp); }
    // takes a reference to the new completion and drops the old one's (a SmartPtr assignment)
    void SetCompletion(CompletionBase<SmartPtr<AsyncOp> >* theCompletion)
    { ee_won_asyncop_set_completion(this->mOp,theCompletion); }
    bool IsKilled() const { return ee_won_asyncop_killed(this->mOp); }
    long GetMode() const { return ee_won_asyncop_run_mode(this->mOp); }
    uint32_t GetStartTime() const { return ee_won_asyncop_start_time(this->mOp); }
    // the mode, unless the op runs (29f190)
    void SetMode(AsyncRunMode inMode) { ee_won_asyncop_set_mode(this->mOp,static_cast<int32_t>(inMode)); }
    // finishes even a finished op, unless its completion is pending (29f2f0)
    void ForceFinish(WONStatus inStatus) { ee_won_asyncop_force_finish(this->mOp,inStatus); }
    // finishes a running op, its completion queued for async runs (29f220)
    void Finish(WONStatus inStatus) { ee_won_asyncop_finish(this->mOp,inStatus); }
    void RunAsync(long inTimeout) { this->Run(1,inTimeout); }
    // marks the op killed and, when asked, finishes it killed (0x29f1e0)
    void Kill(bool inFlag) { ee_won_asyncop_kill(this->mOp,inFlag); }
    // a pending completion: the timer entry dropped, the cleanup hook run, the completion called (29f3a0)
    void Complete() { ee_won_asyncop_complete(this->mOp); }
    // the time before the time-out; 0 for async runs or once it passed (0x29f700)
    unsigned long TimeLeft() { return ee_won_asyncop_time_left(this->mOp); }
    // the run mode, start and timeout for the Rust op algorithms (rust/crates/ee_won_net/src/ops.rs)
    EeWonOpTiming GetTiming() const { return ee_won_asyncop_timing(this->mOp); }
    EeWonAsyncOp* GetOpObject() const { return this->mOp; }
    // a receive's progress chunk's receiving op (the original's RecvChunkOp::GetRecvOp), NULL for any other op
    RefCount* GetSubject() const { return static_cast<RefCount*>(ee_won_asyncop_subject(this->mOp)); }

private:
    EeWonAsyncOp* mOp;                      // owned
};
typedef SmartPtr<AsyncOp> AsyncOpPtr;

// AsyncSocket.cpp's original addresses: the constructor 2b0340, Duplicate 2b0420, the destructor 2b04a0, the length
// field 2b0600 / 2b0610, AddMsgTransform 2b0620, the transforms 2b0730 / 2b07e0, GetLastSocketError 2b0890, IsValid
// 2b08c0, IsConnected 2b08d0, GetDestAddr 2b08e0, GetLocalPort 2b08f0, Start/StopWinsock 2b0940 / 2b0980,
// HasInternetConnection 2b09a0, GetType 2b0a40, SetBroadcast 2b0a50, Open 2b0a90, Bind 2b0c90, Fn2B0D90, Fn2B0E60,
// Listen 2b0fc0, Accept 2b1060, WaitForConnect 2b1220, WaitForAccept 2b1350, WaitForWrite 2b1360, WaitForRead 2b1410,
// Send 2b14c0, Recv 2b15e0, SendTo 2b1700, RecvFrom 2b1840, Disconnect 2b1980, ClosePrv 2b1a10, Close 2b1ad0, Kill
// 2b1b50; the empty hooks fold into 326b60. The unit is deleted (net round 25): the class is this shim over its Rust
// object, the calls taking platform addresses are the core unit's (AsyncOp.cpp)
class AsyncSocket : public RefCount {
public:
    enum SocketType { TCP, UDP }; //WON SDK type, 32-bit enum
    // a closed socket of the type (0 stream, 1 datagram) with a 4-byte length field, counted
    AsyncSocket(int inType) : mSock(ee_won_asyncsocket_new(static_cast<void*>(this), WonCore(), inType)) {}
    // closes the socket, deletes every message transform and uncounts it
    virtual ~AsyncSocket() { ee_won_asyncsocket_free(this->mSock); }
    virtual void OpenHook() {}
    virtual void CloseHook() {}
    virtual void KillHook() {}
    virtual AsyncSocket* Duplicate() { return new AsyncSocket(this->GetType()); } // a new, unopened one
    SocketType GetType() { return (SocketType)ee_won_asyncsocket_kind(this->mSock); }
    static int TimeLeft(DWORD theTotalTime, DWORD theStartTime)
    { return static_cast<int>(ee_won_real_time_left(theStartTime, theTotalTime)); }
    #include EE_WON_DRV(Routing_AsyncSocket) // the drivers' members
    // an up network adapter with an IPv4 address neither 0.0.0.0 nor loopback (the original asked wininet)
    static bool HasInternetConnection() { return ee_won_has_internet_connection(); }
    // a fresh non-blocking handle (broadcast for UDP, no linger for TCP), the open hook run
    WONStatus Open() { return ee_won_asyncsocket_open(this->mSock); }
    // closes a stream socket; dissolves a datagram socket's association
    void Disconnect() { ee_won_asyncsocket_disconnect(this->mSock); }
    WONStatus Bind(const IPAddr& inAddress, bool inReuse) // opened first; address reuse optional
    { return ee_won_asyncsocket_bind(this->mSock, inAddress.GetSockAddrIn().mAddress, inAddress.GetSockAddrIn().mPort, inReuse); }
    // a new socket of the same type (the virtual Duplicate) with the accepted handle (cppcore.rs n_accept)
    WONStatus Accept(SmartPtr<AsyncSocket>& outSocket) { return ee_won_asyncsocket_accept_to(static_cast<RefCount*>(this), AcceptSink, &outSocket); }
    static void AcceptSink(void* inPtr, void* inSocket) { *static_cast<SmartPtr<AsyncSocket>*>(inPtr) = static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket)); }
    // binds to the local host address on the port (the SDK's second parameter is bool)
    WONStatus Fn2B0D90(unsigned short inPort, bool inReuse) { return this->Bind(IPAddr(IPAddr::GetLocalHost(), inPort), inReuse); }
    WONStatus Listen(int inBacklog) { return ee_won_asyncsocket_listen(this->mSock, inBacklog); }
    // up to the time until writable or readable (no wait for 0 or a closed socket)
    void WaitForWrite(unsigned long inWaitTime) { ee_won_asyncsocket_wait(this->mSock, PLAT_SOCKET_WAIT_WRITE, static_cast<uint32_t>(inWaitTime)); }
    // WS_TimedOut (PartialSendTo for datagrams) on a short send
    WONStatus Send(const void* inData, int inLength, int* outSent)
    { return ee_won_asyncsocket_send(this->mSock, inData, inLength, outSent); }
    // after the socket thread is told and the close hook run
    void Close() { ee_won_asyncsocket_close(this->mSock); }
    void Kill() { ee_won_asyncsocket_kill(this->mSock); } // the close and kill hooks; separate from the SDK's Kill
    void SetLengthFieldSize(unsigned char inSize) { ee_won_asyncsocket_set_length_field_size(this->mSock, inSize); }
    bool IsConnected() { return ee_won_asyncsocket_is_connected(this->mSock); }
    // a copy taken under the lock (the original returned a reference to the member)
    IPAddr GetDestAddr() { return IPAddr::FromValue(ee_won_asyncsocket_dest_addr(this->mSock)); }
    // Winsock's numbering; would-block and in-progress are WS_TimedOut, any other recorded
    bool IsValid() { return ee_won_asyncsocket_is_valid(this->mSock); }

    // the socket's lock (the original's mDataCrit, a recursive mutex the Rust object owns), which the subclasses
    // (QueueSocket, the routing and server connections) take around their own state too: AutoCrit takes it
    PlatMutex* GetDataLock() const { return static_cast<PlatMutex*>(ee_won_asyncsocket_mutex(this->mSock)); }
    EeWonAsyncSocket* GetSocketObject() const { return this->mSock; }
private:
    // A Rust object since round 14 (rust/crates/ee_won_net/src/sockobj.rs EeWonAsyncSocket): the handle, type, flags,
    // status and times (the original's +0x20..+0x5c), the length field size (+0x50), the message transforms (+0x60,
    // owned) and the socket thread's close callback (+0x6c), and every step of the class; since round 15 also
    // the lock (mDataCrit, +0x08) and the destination address (mDestAddr, +0x28)
    EeWonAsyncSocket* mSock;
};



// a WON operation on a socket (RTTI SocketOp > AsyncOp, vtable 0x84c424). A Rust object since round 14
// (rust/crates/ee_won_net/src/sockop.rs EeWonSocketOp: the socket thread flag, the event indices, the socket, the
// wait flags and RunHook); since R6 WONLobby round 28 the original's op classes (CloseOp, AcceptOp, ConnectOp,
// RecvBytesOp, RecvMsgOp, RecvCRMsgOp, SendBytesOp, SendMsgOp, SendBytesToOp, RecvBytesFromOp and the queue socket's
// QueueCloseOp) are kinds of this one class (sockops.rs: each constructor's wait flags and members, Start, Continue,
// Duplicate, RecvBytesFromOp's RunHook and RecvBytesOp's CleanupHook by the kind): New makes one. A C++ class of its
// own (the lobby's SendFileOp) has no kind and its virtual Continue.
class SocketOp : public AsyncOp {
public:
    #include EE_WON_DRV(Routing_SocketOp) // the drivers' members
    // the op's socket (a reference held)
    AsyncSocket* GetSocket() { return static_cast<AsyncSocket*>(ee_won_socketop_socket(this->mSockOp)); }
    void SetSocket(AsyncSocket* theSocket) { ee_won_socketop_set_socket(this->mSockOp,theSocket); }
    // an op on the socket (a reference held): no socket thread, no waits, the event indices -1; queued when run async
    // (the WON SDK's mRunAsyncImmediatelyDef, 565f08, was never set)
    SocketOp(AsyncSocket* inSocket) : mSockOp(ee_won_socketop_new(static_cast<void*>(this),WonCore())) { this->SetSocket(inSocket); }
    // an op of a class on the socket: the class's bytes (a reference taken), address (an IPAddr's value, copied; NULL:
    // an empty one) and length or size
    SocketOp(EeWonCoreSocketOp inKind, AsyncSocket* inSocket = NULL, const ByteBuffer* inBytes = NULL,
             const EeWonIpAddr* inAddr = NULL, unsigned long inSize = 0)
        : mSockOp(ee_won_socketop_new_kind(static_cast<void*>(this), WonCore(), inKind, inSocket,
                                           const_cast<RefCount*>(static_cast<const RefCount*>(inBytes)), inAddr,
                                           static_cast<uint32_t>(inSize))) {}
    virtual ~SocketOp() { ee_won_socketop_free(this->mSockOp); } // the original's was implicit, 29153d
    // +0x04 0x2b6070: started (finished InvalidSocket first without a socket); a pending start goes to the socket
    // thread (async) or is continued and finished (blocking), any other result finishes it
    virtual void RunHook() { ee_won_socketop_run_hook(this->mSockOp); }
    // +0x08 0x2b60c0: out of the socket thread if it was handed there, a receive's target file closed (sockops.rs)
    virtual void CleanupHook() { ee_won_socketop_cleanup(this->mSockOp, static_cast<void*>(this), this->GetStatus()); }
    //: +0x0c starts the operation, +0x10 continues it: the kind's (SendFileOp 0x2915d2 / 0x291613 its own)
    virtual WONStatus Start() { return ee_won_socketop_start(this->mSockOp,this->GetTiming()); }
    virtual WONStatus Continue() { return ee_won_socketop_continue(this->mSockOp,this->GetTiming()); }
    // +0x14: a fresh op of the kind on the same socket, none for the others (provisional name)
    virtual AsyncOp* Duplicate()
    { return static_cast<AsyncOp*>(static_cast<RefCount*>(ee_won_socketop_duplicate(this->mSockOp))); }

    void SetWaitForEvent(int i,bool inFlag) { ee_won_socketop_set_wait_for_event(this->mSockOp,i,inFlag); }
    EeWonSocketOp* GetSocketOpObject() const { return this->mSockOp; }
    int GetKind() const { return ee_won_socketop_class(this->mSockOp); } // an EeWonCoreSocketOp, -1 for none
    // the kinds' members: a receive's bytes or datagram, a message or line, the address (a copy), the accepted
    // socket, the bytes read, the progress completion called after every interval bytes
    ByteBufferPtr GetBytes() const
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_received(this->mSockOp))); }
    ByteBufferPtr GetMsg() const
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_message(this->mSockOp))); }
    IPAddr GetAddr() const { return IPAddr::FromValue(ee_won_socketop_address(this->mSockOp)); }
    AsyncSocket* GetAcceptedSocket()
    { return static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_socketop_accepted(this->mSockOp))); }
    unsigned long GetBytesRead() const { return ee_won_socketop_bytes_read(this->mSockOp); }
    void SetProgressCompletion(CompletionBase<SmartPtr<AsyncOp> >* completion, long interval)
    { ee_won_socketop_set_progress(this->mSockOp, static_cast<void*>(completion), static_cast<int32_t>(interval)); }
    void SetFilePath(const std::string& theFilePath) // a receive's target file
    { ee_won_socketop_set_path(this->mSockOp,reinterpret_cast<const uint8_t*>(theFilePath.data()),theFilePath.size()); }
private:
    EeWonSocketOp* mSockOp;
};

// what a receive's progress completion gets (the original's RecvChunkOp, 0x48 bytes: an AsyncOp with the receiving
// op at +0x44, its RunHook 2b5370 Finish(0)) is a plain AsyncOp whose subject is the receiving op since R6 WON lobby
// and network round 36 (ee_won opobj.rs, cppcore.rs n_progress); NULL for an op that is no progress chunk
inline SocketOp* RecvChunkOf(AsyncOp* inOp) { return static_cast<SocketOp*>(static_cast<AsyncOp*>(inOp->GetSubject())); }


// the completion interface (vtable 0x43e4a0: ??_G, pure Complete)
template <class T> class CompletionBase : public RefCount {
public:
    virtual void Complete(T theParam) = 0;  // +0x04
};
// QueueSocket.cpp's original addresses: the constructor 2b3930, Duplicate 2b39d0, the setters 2b3bf0 / 2b3ca0, the
// close timer 2b3d50 / 2b3ed0 / 2b4010, OpCallback 2b4220, SetRepeatOp 2b4500, RunRepeatOp 2b45b0, QueueOp 2b4740,
// RunQueueOp 2b48b0, QueueClose 2b4a70, the hooks 2b4c00 / 2b4c10 / 2b4d40. The unit is deleted (net round 25): the
// class is this shim, its completions (ParamCompletion over the static callbacks, QueueOpFastCompletion, the close
// timer's) and the calls its object makes are Rust's (ee_won cppcore.rs), QueueCloseOp the core's (AsyncOp.cpp)
class QueueSocket : public AsyncSocket {
public:
    QueueSocket(int inType) : AsyncSocket(inType)
    { this->mQueue = ee_won_queuesocket_new(static_cast<void*>(this), WonCore(), this->GetDataLock()); }
    virtual ~QueueSocket() { ee_won_queuesocket_free(this->mQueue); } // the original's was implicit, 2b3a60
    // the queue's state is the open, close and kill hooks' (the queued-close flag; the ops killed; the completions)
    virtual void OpenHook() { ee_won_queuesocket_open_hook(this->mQueue); }
    virtual void CloseHook() { ee_won_queuesocket_close_hook(this->mQueue); }
    virtual void KillHook() { ee_won_queuesocket_kill_hook(this->mQueue); }
    virtual AsyncSocket* Duplicate() { return new QueueSocket(this->GetType()); }
    #include EE_WON_DRV(Routing_QueueSocket) // the drivers' members
    void SetRepeatCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_queuesocket_set_repeat_completion(this->mQueue, inCompletion); }
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_queuesocket_set_close_completion(this->mQueue, inCompletion); }
    void SetRepeatOp(SocketOp* inOp) { ee_won_queuesocket_set_repeat_op(this->mQueue, static_cast<AsyncOp*>(inOp)); }
    void QueueOp(SocketOp* inOp, long inTimeout)
    { ee_won_queuesocket_queue_op(this->mQueue, static_cast<AsyncOp*>(inOp), inTimeout); }
    void QueueClose(long inTimeout) { ee_won_queuesocket_queue_close(this->mQueue, inTimeout); }
    EeWonQueueSocket* GetQueueObject() const { return this->mQueue; }

private:
    // A Rust object since round 14 (rust/crates/ee_won_net/src/qsock.rs EeWonQueueSocket): the default, current, repeat
    // and close completions (+0x74..+0x80), the repeat and queued ops (+0x84, +0x88), the close timer (+0x8c), the
    // op queue (+0x90) and the queued-close flag (+0x9c), each a reference held, and every step of the class
    EeWonQueueSocket* mQueue;
};

// a queue socket the WON ops make and hold (its blocking calls are the op base's: ee_won hostsock.rs)
class BlockingSocket : public QueueSocket {
public:
    BlockingSocket(int inType) : QueueSocket(inType) {} //2b1bd0: stream or datagram
    // IMPLICIT destructor normal2aad90(375B), no own-vptr; inlines Queue members
    // then calls AsyncSocket::~AsyncSocket 2b04a0. Scalar deleting 2b1bf0.
    virtual AsyncSocket* Duplicate() { return new BlockingSocket(this->GetType()); } //2aab50: a new, unopened one
};

// AoC UDP records: four slots each; the archived UDP header's extra
// RetransmitHook is a version difference and MUST NOT be inserted here.
// UDPManager.cpp's original addresses: UDPManagerRecvOp 2b68b0 / 2b6a10, SetMessageVerifier 2b6bf0, CheckMessage
// 2b6c10, QueueRecv 2b6c30, RecvCallback 2b6d70 / 2b6e60, CleanupHook 2b6f60, RunHook 2b6fb0; UDPManagerSendRecvOp
// 2b70b0, StaticCheckResponse 2b7250, SendRequest 2b7260, RunHook 2b7470, RecvCallback 2b7550 / 2b75e0, CleanupHook
// 2b7630; UDPManager 2b76b0, Close 2b78e0, Bind 2b79c0, SetAutoRecv 2b7b40, QueueOp 2b7c50, Send 2b7ec0, CleanOps
// 2b8000, RecvCallback 2b8230 / 2b8320. The unit is deleted (net round 25): the classes are these shims over their
// Rust objects (udpmgr.rs, udpops.rs), whose calls back (the ops, sockets, completions) are Rust's (cppcore.rs)
// but the virtual message checks
// A Rust object since round 15 (rust/crates/ee_won_net/src/udpmgr.rs EeWonUdpManager: the socket +0x08, the default
// completion +0x0c, the auto-receive op +0x10, the clean-up cursors +0x14/+0x18, the list +0x1c and the multimap
// +0x28 of waiting receive ops, the receive size +0x34, and Close, SetAutoRecv, QueueOp, Send, CleanOps, Bind, the
// erases 0x2b8160/0x2b81b0 and RecvCallback's dispatch). The original's mAutoRecv (+0x38) was never read: gone
class UDPManager : public RefCount {
public:
    #include EE_WON_DRV(Routing_UDPManager) // the drivers' members
    UDPManager() : mManager(ee_won_udpmanager_new(static_cast<void*>(this), WonCore())) {}
    virtual ~UDPManager() { ee_won_udpmanager_free(this->mManager); } // closes nothing (the original's 2b77b0)
    // rebinds to a local port (optionally broadcast) and receives datagrams repeatedly
    WONStatus Bind(int inPort, bool inBroadcast) { return ee_won_udpmanager_bind(this->mManager, inPort, inBroadcast); }
    void SetDefaultCompletion(CompletionBase<AsyncOpPtr>* theCompletion)
    { ee_won_udpmanager_set_default_completion(this->mManager, static_cast<RefCount*>(theCompletion)); }
    void Send(const ByteBuffer* inData, const IPAddr& inAddress) // provisional name (the SDK has SendBytesTo)
    { ee_won_udpmanager_send(this->mManager, const_cast<RefCount*>(static_cast<const RefCount*>(inData)), inAddress.GetValue()); }
    void Close() { ee_won_udpmanager_close(this->mManager); }
    void QueueOp(AsyncOp* inOp, unsigned long inTimeout) // a UDPManagerRecvOp
    { ee_won_udpmanager_queue_op(this->mManager, static_cast<RefCount*>(inOp), static_cast<uint32_t>(inTimeout)); }
    // the socket (NULL when unbound)
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udpmanager_socket(this->mManager)))); }
    EeWonUdpManager* GetManagerObject() const { return this->mManager; }
private:
    EeWonUdpManager* mManager;
};
// A Rust object since round 16 (rust/crates/ee_won_net/src/udpops.rs EeWonUdpRecvOp: the socket +0x44, the manager
// +0x48, the own-socket flag +0x4c, the address +0x50, the datagram +0x78, the check +0x7c and its context +0x80, and
// RunHook, CleanupHook, QueueRecv and RecvCallback); the class is a shim the lobby subclasses
class UDPManagerRecvOp : public AsyncOp {
public:
    // from an IPAddr's value (copied; NULL: none)
    explicit UDPManagerRecvOp(const EeWonIpAddr* inAddr = NULL) : mUdp(ee_won_udprecvop_new(static_cast<RefCount*>(this), WonCore(), inAddr)) {}
    virtual ~UDPManagerRecvOp() { ee_won_udprecvop_free(this->mUdp); } // the original's was implicit (2b6930)
    #include EE_WON_DRV(Routing_UDPManagerRecvOp) // the drivers' members
    // a copy (the original returned a reference to the member)
    IPAddr GetFromAddr() const { return IPAddr::FromValue(ee_won_udprecvop_address(this->mUdp)); }
    const ByteBuffer* GetMsg() const
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_udprecvop_msg(this->mUdp))); }
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udprecvop_socket(this->mUdp)))); }
    EeWonUdpRecvOp* GetUdpObject() const { return this->mUdp; }
    virtual void RunHook() { ee_won_udprecvop_run(this->mUdp); }
    virtual void CleanupHook() { ee_won_udprecvop_cleanup(this->mUdp); }
    typedef WONStatus (*MessageVerifier)(ReadBuffer&, void*);
    // whether a datagram answers this op (WS_None = no): the check, or any without one
    virtual WONStatus CheckMessage(ReadBuffer& inMessage) //2b6c10, new slot 0c
    {
        MessageVerifier aVerifier = reinterpret_cast<MessageVerifier>(ee_won_udprecvop_verifier(this->mUdp));
        return aVerifier ? aVerifier(inMessage, ee_won_udprecvop_verifier_context(this->mUdp)) : 0;
    }
private:
    EeWonUdpRecvOp* mUdp;
};

// A Rust object since round 16 (udpops.rs EeWonUdpSendRecvOp: the manager +0x44, the own socket +0x48, the request
// +0x4c, the address +0x50, the reply wait +0x78, the retry limit +0x7c, the attempts +0x80, the reply time-out +0x84,
// and RunHook, CleanupHook, SendRequest and RecvCallback); the class is a shim the lobby subclasses
class UDPManagerSendRecvOp : public AsyncOp {
public:
    // the reply wait's check: the request's virtual CheckResponse (cppcore.rs gives a wait this function)
    static WONStatus StaticCheckResponse(ReadBuffer& inMessage, void* inParam)
    { return static_cast<UDPManagerSendRecvOp*>(inParam)->CheckResponse(inMessage); }
    UDPManagerSendRecvOp() : mUdp(ee_won_udpsendrecvop_new(static_cast<RefCount*>(this), WonCore())) {}
    // a request with a Rust body (udpops.rs EeWonUdpBody: what the lobby's subclasses PingGameOp and
    // RoutingGetUserStatusOp added, both deleted in R6 WON lobby and network round 36), its address and manager set;
    // the body's members by kind are ee_won_udp_body_of(op, EE_WON_UDP_BODY_*)
    UDPManagerSendRecvOp(EeWonUdpBody inBody, const IPAddr& inAddr, UDPManager* inManager)
        : mUdp(ee_won_udpsendrecvop_new_body(static_cast<RefCount*>(this), WonCore(), inBody, inAddr.GetValue(), static_cast<RefCount*>(inManager))) {}
    virtual ~UDPManagerSendRecvOp() { ee_won_udpsendrecvop_free(this->mUdp); } // the original's was implicit (2b7140)
    #include EE_WON_DRV(Routing_UDPManagerSendRecvOp) // the drivers' members
    void SetDestAddr(const IPAddr& theAddr) { ee_won_udpsendrecvop_set_address(this->mUdp, theAddr.GetValue()); }
    IPAddr GetDestAddr() const { return IPAddr::FromValue(ee_won_udpsendrecvop_address(this->mUdp)); } // a copy
    // the op's own socket (none with a manager)
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udpsendrecvop_socket(this->mUdp)))); }
    virtual void RunHook() { ee_won_udpsendrecvop_run(this->mUdp); }
    virtual void CleanupHook() { ee_won_udpsendrecvop_cleanup(this->mUdp); }
    virtual WONStatus CheckResponse(ReadBuffer&) { return 0; } //2ad870: any response (folded with others)
    EeWonUdpSendRecvOp* GetUdpObject() const { return this->mUdp; }
private:
    EeWonUdpSendRecvOp* mUdp;
};



// a completion calling a member of T (RTTI LobbyCompletion<T> > LobbyCompletionBase > CompletionBase<AsyncOpPtr>;
// vtables 0x84c49c / 0x84c4ac; provisional member names). The templates LobbyCompletion<T> and LogicCompletion<T>
// have no game user since R6 WON round 39 (PingLogic's two are the logic shell's: ShellCompletion); their
// LogicCompletionBase (RTTI 4558cc, the tracker flag +0x08) is ShellCompletion's since round 40
class LobbyCompletionBase : public CompletionBase<AsyncOpPtr> {
public:
    LobbyCompletionBase() throw() {}
    virtual void Complete(AsyncOpPtr theParam);     // +0x04 0x357d70 (calls +0x08)
    virtual void Run(AsyncOp* inOp) {}      // +0x08
};

// A Rust object a C++ reference holds (R6 WON round 34): HTTPConnection (RTTI 44d3a4 > RefCount; ctor 2d2c80, dtor
// 2d2d50: a host's keep-alive connection, its socket, queues, running ops, pipelining and retry) and HTTPSession (0x2c
// bytes; ctor 2d27b0, dtor 2d2870: the connections by host) are wholly ee_won's (httpengine.rs EeWonHttpConn and
// EeWonHttpSession, their calls httpcpp.rs, made and freed by rustref.rs). This is the RefCount the ops' completions
// and the session hold, of one kind (EeWonCoreObject::HttpConn or HttpSession), with the lock the Rust takes; the
// library's core table finds both (AsyncOp.cpp CoreObjectOf).
class WonRustRef : public RefCount {
public:
    explicit WonRustRef(EeWonCoreObject inKind)
        : mKind(inKind), mImpl(ee_won_rustref_new(inKind, static_cast<RefCount*>(this), WonCore())) {}
    virtual ~WonRustRef() { ee_won_rustref_free(this->mKind, this->mImpl); }
    // the kind's Rust object, or the lock (NULL for another kind)
    void* Get(EeWonCoreObject inKind) const
    { return inKind == this->mKind ? this->mImpl : inKind == EeWonCoreObject::Lock ? static_cast<void*>(this->mDataCrit.mCrit) : NULL; }
private:
    CriticalSection mDataCrit; // before the Rust object, which may lock it
    EeWonCoreObject mKind;
    void* mImpl; // owned
};
typedef WonRustRef HTTPSession; // of the kind EeWonCoreObject::HttpSession (HttpSessionNew)


class RoutingConnection;

// Every routing op, request or notification, is one Rust object since net round 18 (rust/crates/ee_won_net/src//
// routeop.rs EeWonRouteOp: the op's members by field number EE_WON_RTF_* and text EE_WON_RTT_*, its data type,
// data and recipients, the records its replies build, SendRequest's message and HandleReply's read, and
// RoutingCreateGroupOp::CheckLeaveGroup). This class is the AsyncOp the connection's list and the completions hold,
// one per op type (RoutingOpType, WONRoutingOpTypes.h); its readers read the members through the accessors and the
// records in Rust (ee_won_routeop_records). The original's 55 op classes (RunHook 2c7d20, HandleReply 2bcd30, the request builders and
// notification parsers 2c4170..2c8f00) are gone; RoutingDetectFirewallOp keeps its listening socket below. Since
// net round 26 the op's calls from the connection (its type, HandleReply 2bcd30, its end) and SendRequest's base
// (SendMsg 2c7c00, AddOp 2c7d10) are Rust's too (routecpp.rs); the Rust op knows its connection.
inline void WonStringSink(void* inContext, const uint8_t* inBytes, size_t inLength) // bytes into the std::string
{ static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
class RoutingOp : public AsyncOp {
public:
    RoutingOp(RoutingConnection* inConnection, RoutingOpType inType); // a connection-less op with NULL
    //2c7d20: in blocking mode finishes BlockMode_NotSupported (100009), else the op's request (Rust's) sent on the
    // connection, the op added to its list when it waits for a reply. A firewall check (the original's
    // RoutingDetectFirewallOp, whose class went in R6 WON lobby and network round 36) listens first when asked and
    // closes its listening socket in the clean-up (routecpp.rs); its settings are EE_WON_RTF_LISTEN_PORT,
    // EE_WON_RTF_MAX_WAIT and EE_WON_RTF_DO_LISTEN
    virtual void RunHook() { ee_won_routeop_run(this->mRust, static_cast<RefCount*>(this)); }
    virtual void CleanupHook() { ee_won_routeop_cleanup(this->mRust); }
    virtual RoutingOpType GetType() const { return (RoutingOpType)ee_won_routeop_type(this->mRust); }
    RoutingConnection* GetConnection() { return this->mConnection.get(); }

    // the members (EE_WON_RTF_* numbers, EE_WON_RTT_* texts)
    long long Num(int inField) const { return ee_won_routeop_num(this->mRust, inField); }
    void SetNum(int inField, long long inValue) { ee_won_routeop_set_num(this->mRust, inField, inValue); }
    std::wstring Text(int inWhich) const { std::wstring s; ee_won_routeop_text(this->mRust, inWhich, WonWideSink, &s); return s; }
    void SetText(int inWhich, const std::wstring& inText)
    { ee_won_routeop_set_text_wide(this->mRust, inWhich, inText.data(), sizeof(wchar_t), inText.length()); }
    #include EE_WON_DRV(Routing_RoutingOp) // the drivers' members
    // NULL when the op has no buffer
    ByteBufferPtr GetData() const { ByteBufferPtr b; ee_won_routeop_data(this->mRust, WonByteBufferSink, &b); return b; }
    void SetData(const ByteBuffer* inData) // (NULL: no buffer)
    {
        ee_won_routeop_set_data(this->mRust, inData != NULL, inData != NULL ? reinterpret_cast<const uint8_t*>(inData->data()) : NULL,
                                inData != NULL ? inData->length() : 0);
    }
    std::list<unsigned short> Recipients() const
    { std::list<unsigned short> l; ee_won_routeop_ids_each(this->mRust, IdSink, &l); return l; }
    static void IdSink(void* inList, uint16_t inId) { static_cast<std::list<unsigned short>*>(inList)->push_back(inId); }
    unsigned short GetGroupId() const { return (unsigned short)this->Num(EE_WON_RTF_GROUP_ID); }
    unsigned short GetClientId() const { return (unsigned short)this->Num(EE_WON_RTF_CLIENT_ID); }


    SmartPtr<RoutingConnection> mConnection; //60, destructor owns release
    EeWonRouteOp* mRust; // owned
protected:
    virtual ~RoutingOp() { ee_won_routeop_free(this->mRust); }
};
#define EE_WON_ROUTE_OPS_RUST 1 // the ops are Rust objects (won_routeops_test reads them so)

// RoutingGetUserStatusOp (ctor 2c6630) is a UDPManagerSendRecvOp with a Rust body (ee_won userstatus.rs): new
// UDPManagerSendRecvOp(ee_won_userstatus_body(), addr, manager); its map (AddWONUserId: ee_won_userstatus_add)
inline EeWonUserStatus* UserStatusOf(AsyncOp* inOp)
{ return static_cast<EeWonUserStatus*>(ee_won_udp_body_of(static_cast<RefCount*>(inOp), EE_WON_UDP_BODY_USER_STATUS)); }
#define EE_WON_USER_STATUS_RUST 1 // the map is a Rust object (won_userstatus_test sets it so)

// the WON server list the account ops use (??_G 0xceaec, dtor 0xceb08; sizeof 0x3c): a shim over ee_won's
// (servctx.rs: the addresses, their order, the one-time shuffle and the lock) converting IPAddr values
class ServerContext : public RefCount {
public:
    EeWonServerList* GetListObject() const { return this->mList; }
    ServerContext() : mList(ee_won_ctx_new()) {}
    virtual ~ServerContext() { ee_won_ctx_free(this->mList); }
    // a valid address not yet in the list appended; whether it was
    bool AddAddress(const IPAddr& inAddress) { return ee_won_ctx_ip(this->mList, 0, inAddress.GetValue()); } //2ac4b0
    void Clear() { ee_won_ctx_clear(this->mList); } //2ac390: the one-time shuffle re-enabled
    // a server that failed to the end of the list
    void NotifyFailed(const IPAddr& inAddress) { ee_won_ctx_ip(this->mList, 1, inAddress.GetValue()); } //2ac980
    // the directory's services' addresses (only those of the name when one is given)
    bool AddAddressesFromDir(const DirList& inDir, const wchar_t* inNameFilter=NULL) //2acab0/332
    { return ee_won_ctx_add_from_dir(this->mList, inDir.get(), inNameFilter, WideLength(inNameFilter), sizeof(wchar_t)); }
    void RemoveAllAddresses() { this->Clear(); } //2acc00
    unsigned long GetNumAddresses() { return (unsigned long)ee_won_ctx_len(this->mList); }
    #include EE_WON_DRV(Routing_ServerContext) // the drivers' members

private:
    static size_t WideLength(const wchar_t* inText) { return inText != NULL ? wcslen(inText) : 0; }
    EeWonServerList* mList;
};

// AsyncOpWithTracker's op tracker: a Rust object since round 13 (rust/crates/ee_won_net/src/opobj.rs EeWonOpTracker: the
// ops tracked by id, a reference each, in id order, the latest first among equal ids; an op tracked once; its lock).
// The original's two multimaps (AsyncOpTrackerMaps, AsyncOpTrackerEntry) are gone.
class AsyncOpTracker {
public:
    AsyncOpTracker() : mTracker(ee_won_op_tracker_new(AsyncOpHooks())) {}
    ~AsyncOpTracker() { ee_won_op_tracker_free(this->mTracker); }
    int GetNumTrack() { return static_cast<int>(ee_won_op_tracker_len(this->mTracker)); }
    bool IsEmpty() { return this->GetNumTrack()==0; }
    // the op tracked at inIndex in id order (NULL past the end)
    AsyncOp* GetTrackedOp(int inIndex)
    { return static_cast<AsyncOp*>(ee_won_op_tracker_at(this->mTracker,static_cast<size_t>(inIndex))); }
    // tracks an op under the id (refused when tracked already); returns the op (original 29fc80)
    AsyncOp* AddOp(AsyncOp* inOp, unsigned long inId)
    { return static_cast<AsyncOp*>(ee_won_op_tracker_add(this->mTracker,inOp,static_cast<uint32_t>(inId))); }
    bool RemoveOp(AsyncOp* inOp) { return ee_won_op_tracker_remove(this->mTracker,inOp); } // original29fe50
    void KillAll() { ee_won_op_tracker_kill_all(this->mTracker); } // each op killed, with completion (29ffa0)
    EeWonOpTracker* GetTrackerObject() const { return this->mTracker; }
private:
    AsyncOpTracker(const AsyncOpTracker&);
    AsyncOpTracker& operator=(const AsyncOpTracker&);
    EeWonOpTracker* mTracker;               // owned
};

}
// CDKey and AuthContext (their forwarders need the classes above; the ops' makers below need them)
#include "ee/wonapi/WONAuthContext.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/authcpp.h"
namespace WONAPI {
// An op of the Rust op base (rust/crates/ee_won_net/src/hostop.rs): its members and steps are the Rust op's
// (MultiPingOp: ee_won_multiping_new, FilePushOp: ee_won_filepush_new, FTPGetOp: ee_won_ftpget_new and _new_url;
// the ops derived from ServerRequestOp: NewRequest over reqhost.rs); this class runs its hooks there, and its calls
// on the C++ objects are answered over the core table (hostcore.rs). Results and setters by number:
// ee_won_hostop_num, ee_won_hostop_bytes, ee_won_hostop_set.
class RustServerOp : public AsyncOp {
public:
    // takes the op; its length field and time limits, no auth. The calls that need no C++ object (the clock, rand,
    // an address, the op's file, the files it measures, the web proxy host: ee_won hostnative.rs) and the op's own
    // (its status and the like, ServerOp's members, the tracker, its completions, references: hostbase.rs) are the
    // Rust op's
    explicit RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp)
    { ee_won_hostop_bind_native(inRustOp, NULL, this, static_cast<AsyncOp*>(this), this->GetOpObject(), this->mTracker.GetTrackerObject()); }
    // and the init's members (hostbase.rs)
    RustServerOp(EeWonHostOp* inRustOp, EeWonHostInit inInit, RefCount* inObj, RefCount* inObj2 = NULL, int inNum = 0) : mRustOp(inRustOp)
    { ee_won_hostop_bind_init(inRustOp, static_cast<AsyncOp*>(this), this->GetOpObject(), this->mTracker.GetTrackerObject(), inInit, inObj, inObj2, inNum); }
    // GetCertOp(auth) (2e3c30; certhost.rs), PeerAuthOp(addr, auth, type, socket) (2e6a10) and
    // PeerAuthServerOp(auth, socket) (2e8950; peerop.rs): their C++ classes went in round 36. A server op's
    // ClientPermits(community) is GetNum(EE_WON_PEEROP_CLIENT_PERMITS, community), a certificate op's
    // SetCreateAccount SetNum(EE_WON_GETCERT_CREATE_ACCOUNT, flag)
    static RustServerOp* NewGetCert(AuthContext* inContext)
    { return new RustServerOp(ee_won_hostop_getcert(), EeWonHostInit::GetCert, inContext->GetServerContext().get(), inContext); }
    static RustServerOp* NewPeerAuth(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket, bool inUseAuth2 = true)
    { return new RustServerOp(ee_won_hostop_peerauth(reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()), inUseAuth2), EeWonHostInit::PeerAuth, inContext, inSocket, inType); }
    static RustServerOp* NewPeerAuthServer(AuthContext* inContext, BlockingSocket* inSocket)
    { return new RustServerOp(ee_won_hostop_peerauth_server(), EeWonHostInit::PeerAuthServer, inContext, inSocket); }
    // DetectFirewallOp(servers) (fwhost.rs): the public address is AddrOf(EE_WON_FWOP_PUBLIC_ADDR), port 0
    static RustServerOp* NewFirewall(ServerContext* inContext)
    { return new RustServerOp(ee_won_hostop_firewall(), EeWonHostInit::Firewall, inContext); }
    // a request's C++ op as reqhost.rs made it (the maker's: ServerOp.cpp)
    static RustServerOp* MadeOp(void* inOp) { return static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp))); }
    // ServerRequestOp(context or address) with a Rust op derived from it (ee_won_reqop_* constructors), taken
    static RustServerOp* NewRequest(EeWonReqOp* inOp, ServerContext* inContext)
    { return MadeOp(ee_won_hostop_new_request(inOp, true, static_cast<RefCount*>(inContext), NULL, -1, NULL)); }
    static RustServerOp* NewRequest(EeWonReqOp* inOp, const IPAddr& inAddr)
    { return MadeOp(ee_won_hostop_new_request(inOp, false, NULL, reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()), -1, NULL)); }
    IPAddr GetCurAddr() const { return this->AddrOf(EE_WON_REQHOST_CUR_ADDR); } // a request's current server
    IPAddr GetAddr() const { return this->AddrOf(EE_WON_REQHOST_ADDR); } // a request's first server (an invalid address: none)
    // an address's six bytes by number (an invalid address: none)
    IPAddr AddrOf(int inWhich) const { return IPAddr::FromValue(ee_won_hostop_addr(this->mRustOp, inWhich)); }
    // an object setter (the op takes its own reference), a number setter
    void Set(int inWhich, RefCount* inObj) { ee_won_hostop_set(this->mRustOp, inWhich, 0, NULL, 0, NULL, inObj); }
    void SetNum(int inWhich, long long inNum) { ee_won_hostop_set(this->mRustOp, inWhich, inNum, NULL, 0, NULL, NULL); }
    EeWonHostOp* GetRustOp() const { return this->mRustOp; }
    // the results of a directory or account op (ee_won reqdir.rs): its entities by number and element
    // (EE_WON_REQOP_DIR_LIST and a query's index, EE_WON_REQOP_DIR_SERVICE), a number, bytes
    DirList GetEntities(int inWhich, long long inIndex = 0) const { return DirList(ee_won_dirlist_of_op(this->mRustOp, inWhich, inIndex)); }
    long long GetNum(int inWhich, long long inIndex = 0) const { return ee_won_hostop_num(this->mRustOp, inWhich, inIndex); }
    std::string GetBytes(int inWhich, long long inIndex = 0) const
    { std::string aBytes; ee_won_hostop_bytes(this->mRustOp, inWhich, inIndex, WonStringSink, &aBytes); return aBytes; }
    // the directory and account ops the game makes (ee_won reqdir.rs, their members reqmake.rs's; wide texts as
    // wchar_t units of their size)
    static RustServerOp* NewGetDir(ServerContext* inContext, const std::wstring& inPath, DWORD inFlags,
                                   const char* const* inTypes = NULL, int inTypeCount = 0)
    { return NewRequest(ee_won_reqmake_get_dir(inFlags, inPath.data(), inPath.size(), sizeof(wchar_t), inTypes, inTypeCount), inContext); }
    // (NewGetMultiDir went in R6 WON lobby and network round 37: InitLogic's query is ee_won_reqmake_init_dirs)
    static RustServerOp* NewGetAccount(ServerContext* inContext, AuthContext* inAuth)
    { return MadeOp(ee_won_hostop_new_request(ee_won_reqmake_get_account(), true, static_cast<RefCount*>(inContext), NULL, AUTH_TYPE_SESSION, static_cast<RefCount*>(inAuth))); }
    static RustServerOp* NewUpdateAccount(ServerContext* inContext, AuthContext* inAuth, const std::wstring& inPassword,
                                          const std::string& inEmail)
    { return MadeOp(ee_won_hostop_new_request(ee_won_reqmake_update_account(inPassword.data(), inPassword.size(), sizeof(wchar_t), WonBytes(inEmail), inEmail.size()), true, static_cast<RefCount*>(inContext), NULL, AUTH_TYPE_SESSION, static_cast<RefCount*>(inAuth))); }
    // the create account and the group list request (ee_won reqlast.rs): the lobby's profile (newsletter 1 and
    // the birth date); a routing server's group list flags, its replies read again by RouteGroupListOf
    static RustServerOp* NewCreateAccount(ServerContext* inContext, const std::wstring& inUserName,
                                          const std::wstring& inPassword, const std::string& inEmail,
                                          bool inNewsletter, unsigned char inMonth, unsigned char inDay,
                                          unsigned short inYear)
    { return NewRequest(ee_won_reqmake_create_account(inUserName.data(), inUserName.size(), inPassword.data(), inPassword.size(), sizeof(wchar_t), WonBytes(inEmail), inEmail.size(), inNewsletter, inMonth, inDay, inYear), inContext); }
    static RustServerOp* NewGroupListReq(const IPAddr& inAddr, unsigned char inFlags) { return NewRequest(ee_won_reqmake_group_list(inFlags), inAddr); }
    // HTTPGetOp(host, path) and HTTPGetOp(url) (ee_won httpget.rs; the web proxy file read at the first one made),
    // a bytes setter, and its readers that are the C++ side's: GetSocket() (NULL unless connected) and
    // GetLastModifiedTime() (the local file's time when the reply had none)
    static RustServerOp* NewHttpGet(const std::string& inHost, const std::string& inPath)
    { return new RustServerOp(ee_won_httpget_open(WonBytes(inHost), inHost.size(), WonBytes(inPath), inPath.size())); }
    static RustServerOp* NewHttpGet(const std::string& inURL) { return new RustServerOp(ee_won_httpget_open_url(WonBytes(inURL), inURL.size())); }
    void SetBytes(int inWhich, const std::string& inBytes) { ee_won_hostop_set(this->mRustOp, inWhich, 0, WonBytes(inBytes), inBytes.size(), NULL, NULL); }
    #include EE_WON_DRV(Routing_RustServerOp) // the drivers' members
    // GetHTTPDocumentOp(product, type) (ee_won httpdoc.rs; the type an HTTPDocType, its files in the WON directory,
    // the library's default file directory)
    static RustServerOp* NewHttpDocument(const std::string& inProduct, int inType)
    { return new RustServerOp(ee_won_httpdoc_new(inType, WonBytes(inProduct), inProduct.size(), NULL, 0)); }
    // (GetDocument(owner), the cache file, is GetBytes(EE_WON_HTTPDOC_DOCUMENT, owner))
    BlockingSocket* GetSocket() const
    { return static_cast<BlockingSocket*>(reinterpret_cast<RefCount*>((intptr_t)ee_won_hostop_base_get(this->mRustOp, EE_WON_HOSTBASE_SOCKET))); }
    void SetSocket(BlockingSocket* inSocket) { ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_SOCKET, 0, static_cast<RefCount*>(inSocket)); }
    void SetLengthFieldSize(unsigned char inSize) { ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_LFS, inSize, NULL); }
    virtual void RunHook() { ee_won_hostop_run(this->mRustOp); }
    virtual void CleanupHook() { ee_won_hostop_cleanup(this->mRustOp); }
    virtual bool CallbackHook(AsyncOp* inOp, int inParam) { return ee_won_hostop_callback(this->mRustOp, inParam, inOp); }
    AsyncOpTracker mTracker; // its sub-ops (the original's AsyncOpWithTracker, 44..78)
protected:
    virtual ~RustServerOp() { ee_won_hostop_free(this->mRustOp); } // its objects released, its file closed
private:
    EeWonHostOp* mRustOp; // owned (with the op's file: ee_won hostnative.rs)
};
typedef SmartPtr<RustServerOp> RustServerOpPtr;
inline void* AuthContext::NewCertOp(void* inContext) // the certificate op the auth flow makes (authcpp.rs)
{ return static_cast<RefCount*>(static_cast<AsyncOp*>(RustServerOp::NewGetCert(static_cast<AuthContext*>(static_cast<RefCount*>(inContext))))); }
inline void AuthContext::Init()
{ this->mCore = ee_won_authctx_new_default(static_cast<RefCount*>(this), WonCore(), NewCertOp, static_cast<RefCount*>(new ServerContext)); }
// HTTPSession's calls (2d27b0, 2d28f0: an HTTP get op on its host's connection, made at the first use; 2d2bc0: every
// connection's requests killed); a connection's are the Rust's own (httpcpp.rs)
inline HTTPSession* HttpSessionNew() { return new WonRustRef(EeWonCoreObject::HttpSession); }
inline EeWonHttpSession* HttpSessionOf(HTTPSession* inSession)
{ return static_cast<EeWonHttpSession*>(inSession->Get(EeWonCoreObject::HttpSession)); }
inline void HttpSessionAddOp(HTTPSession* inSession, RustServerOp* inOp)
{ ee_won_http_session_add_op(HttpSessionOf(inSession), static_cast<RefCount*>(static_cast<AsyncOp*>(inOp))); }
inline void HttpSessionKillAll(HTTPSession* inSession) { ee_won_http_session_kill_all(HttpSessionOf(inSession)); }

// The WON SDK's other classes have no C++ left (ee_won_net): AuthSession (hostauth.rs), the directory parsers and ops
// (dir.rs, dirlist.rs, reqdir.rs), PeerAuthOp and PeerAuthServerOp (peerop.rs), HTTPCache (httpcache.rs), HTTPGetOp
// (httpget.rs), the threads (sockthread.rs, thread.rs), GetHTTPDocumentOp and its enums (httpdoc.rs), GetCertOp
// (certhost.rs), DBProxyOp (reqop.rs), the patch records (patch.rs, verop.rs), CheckValidVersionOp and
// GetUserIdFromNameOp (reqops.rs), DetectFirewallOp (fwhost.rs), the e-mail ops (adminop.rs), the account ops
// (reqdir.rs, reqlast.rs); the ops on the base are made by RustServerOp's New* makers.
// DNSTestOp (ctor 2d95a0; dnstest.rs): resolves the host on a thread; GetHostAddr is AddrOf(EE_WON_DNSTEST_ADDR)
inline RustServerOp* NewDnsTestOp(const std::string& inHost) { return new RustServerOp(ee_won_dnstest_new(reinterpret_cast<const uint8_t*>(inHost.data()), inHost.size())); }


// A Rust object since net round 16 (rust/crates/ee_won_net/src/servconn.rs EeWonServerConn: the auth context +0xa0 and
// type +0xa4, the peer authentication op +0xa8, the connect completion +0xac, the keep-alive timer +0xb0 and period
// +0xb4, and every step of ServerConnection.cpp); this class is the shim RoutingConnection and the lobby derive from
// Since net round 25 its calls back are Rust's (cppcore.rs NATIVE_SERVCONN: the ops, the completions the original's
// static callbacks were, 2ab160 / 2ab6a0 / 2ab7b0, the messages, the socket); since R6 WONLobby round 12 the peer
// authentication op's maker (hostcore.rs) and the virtual MsgCallback (RoutingConnection's, the one override) too:
// ServerConnection.cpp is deleted (the drivers bind its table: tests/won/won_servconn_cpp.h)
class ServerConnection : public BlockingSocket {
public:
    ServerConnection() : BlockingSocket(0) //2aab10: unauthenticated, a 300-second keep-alive
    { this->mConn=ee_won_servconn_new(static_cast<void*>(this),WonCore(),NULL,static_cast<void*>(this->GetDataLock())); }
    virtual ~ServerConnection() { ee_won_servconn_free(this->mConn); } // the original's was implicit, 2aabe0
    // the keep-alive timer and a pending connect stopped, the message callback dropped, the socket closed (2abe70)
    virtual void CloseHook() { ee_won_servconn_close_hook(this->mConn); }
    // the socket killed, the connect completion and auth context dropped (2abf50)
    virtual void KillHook() { ee_won_servconn_kill_hook(this->mConn); }
    #include EE_WON_DRV(Routing_ServerConnection) // the drivers' members
    virtual void MsgCallback(const ByteBuffer*) {} //MsgCallback2fa500,ret4
    void SetAuth(AuthContext* inContext, AuthType inType)
    { ee_won_servconn_set_auth(this->mConn,reinterpret_cast<RefCount*>(inContext),inType); }
    // the socket closed and an authenticated connect started; the peer authentication op (2abc40, borrowed)
    AsyncOp* ConnectAsync(const IPAddr& address, unsigned long timeout)
    {
        return static_cast<AsyncOp*>(static_cast<RefCount*>(
            ee_won_servconn_connect_async(this->mConn,static_cast<const void*>(&address),(long)timeout)));
    }
    // the completion called when a connect finishes (2abdc0)
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_servconn_set_connect_completion(this->mConn,static_cast<RefCount*>(inCompletion)); }
    EeWonServerConn* GetConnObject() const { return this->mConn; }
private:
    EeWonServerConn* mConn;
};

// ServerConnectionConnectOp (2abfc0: a server connection connected to an address, finishing with the connect's
// status) is a Rust op since R6 WON lobby and network round 40: RustServerOp::MadeOp(ee_won_servconn_connect_op_new(
// connection, address)) (ee_won cppcore.rs ServConnConnect)

// The op list, the completions and the reply dispatch are a Rust object since net round 16
// (rust/crates/ee_won_net/src/routeconn.rs EeWonRouteConn: the list +0xb8, the completions +0xc8 and the default one
// +0x1bc, and AddOp 2ba630, CalcCompletion 2ba740, GetUnsolicitedOp 2ba830, MsgCallback 2bc850, CloseHook 2bcb20,
// KillHook 2bcbb0). Since net round 26 its calls back are Rust's (routecpp.rs NATIVE_ROUTECONN) but a new op and
// the two virtuals below (AsyncOp.cpp's core table), and every request helper is Rust's (ee_won_routeconn_request:
// a RoutingOp of the helper's type, its members from the values in the helper's parameter order, run async; the
// original addresses are in the comments); RoutingConnection.cpp is deleted.
// A request helper's values in parameter order: numbers, texts (UTF-16), data types, a buffer (or none), recipients
class RoutingRequestValues {
public:
    #include EE_WON_DRV(Routing_RoutingRequestValues) // the drivers' members
    // the values are a Rust builder's (rust/crates/ee_won_net/src/routereq.rs, R6 WON round 34)
    RoutingRequestValues() : mReq(ee_won_routereq_new()) {}
    ~RoutingRequestValues() { ee_won_routereq_free(this->mReq); }
    RoutingRequestValues& N(long long inNum) { return this->Add(inNum, NULL, 0); }
    RoutingRequestValues& W(const std::wstring& inText)
    { ee_won_routereq_wide(this->mReq, inText.data(), sizeof(wchar_t), inText.length()); return *this; }
    RoutingRequestValues& S(const std::string& inBytes) { return this->Add(0, inBytes.data(), inBytes.size()); }
    RoutingRequestValues& D(const ByteBuffer* inData)
    { return inData == NULL ? this->Add(0, NULL, 0) : this->Add(1, inData->data(), inData->length()); }
    RoutingRequestValues& L(const std::list<unsigned short>& inIds)
    { ee_won_routereq_ids(this->mReq, std::vector<uint16_t>(inIds.begin(), inIds.end()).data(), inIds.size()); return *this; }
    void Run(RefCount* inConn, int inKind, CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_routereq_run(this->mReq, inConn, inKind, static_cast<RefCount*>(inCompletion)); }
private:
    RoutingRequestValues(const RoutingRequestValues&);
    RoutingRequestValues& operator=(const RoutingRequestValues&);
    RoutingRequestValues& Add(long long inNum, const void* inBytes, size_t inLength)
    { ee_won_routereq_add(this->mReq, inNum, static_cast<const uint8_t*>(inBytes), inLength); return *this; }
    EeWonRouteReq* mReq; // owned
};
typedef CompletionBase<AsyncOpPtr> RouteCompletion;
class RoutingConnection : public ServerConnection {
public:
    // 2ba630: the op (its completion resolved) waits for its replies
    void AddOp(RoutingOp* inOp) { ee_won_routeconn_add_op(this->mRoute, static_cast<RefCount*>(inOp)); }
    // 2ba050 (allocation 1c0 at 3eddd4): 2-byte length fields; every derived-server message unsolicited
    RoutingConnection() : mRoute(ee_won_routeconn_new_core(static_cast<RefCount*>(this), WonCore(), static_cast<void*>(this->GetDataLock())))
    { this->SetLengthFieldSize(2); }
    // the completion an op of a type (RoutingOpType, below 61) gets when it has none, and the default one
    void SetOpCompletion(int inType, RouteCompletion* inCompletion)
    { ee_won_routeconn_set_completion(this->mRoute, inType, static_cast<RefCount*>(inCompletion)); }
    void SetDefaultOpCompletion(RouteCompletion* inCompletion)
    { ee_won_routeconn_set_completion(this->mRoute, -1, static_cast<RefCount*>(inCompletion)); }
    // (the request helpers, 2bcc30..2bf8f0 and the SDK's others, are gone since WONLobby round 14: RoutingLogic
    // makes its requests in Rust, routelogic.rs conn_request, EEWonNet.cpp and the drivers with RoutingRequestValues)

    virtual void CloseHook() { ee_won_routeconn_close_hook(this->mRoute); } // 2bcb20: the ops killed, the list emptied
    virtual void KillHook() { ee_won_routeconn_kill_hook(this->mRoute); } // 2bcbb0: the completions dropped
    // 2bc850: the message to the first pending or unsolicited op that takes it
    virtual void MsgCallback(const ByteBuffer* inData)
    { ee_won_routeconn_msg_callback(this->mRoute, reinterpret_cast<const uint8_t*>(inData->data()), inData->length()); }
    // (the record factories GetNewClientInfo 2ba100, GetNewMemberInfo 2ba230 and GetNewGroupInfo 2ba300 are gone:
    // the ops' records are Rust's since net round 18)
    // 2bc6c0: a new derived-server op (RoutingOp_DerivedServerOp; the original's mAllDerivedOpsAreUnsolicited, +0xc4,
    // was never cleared: every derived message is unsolicited), NULL for none
    virtual SmartPtr<RoutingOp> GetUnsolicitedDerivedOp(unsigned char) { return new RoutingOp(this, 60); }
    virtual void RoutingOpCompleteHook(RoutingOp*) {} //inline RoutingOpCompleteHook (WON SDK), shared 30a800 ret4
    EeWonRouteConn* GetRouteObject() const { return this->mRoute; }
protected:
    virtual ~RoutingConnection() { ee_won_routeconn_free(this->mRoute); } //2ba4b0
private:
    EeWonRouteConn* mRoute; // owned
};
// 6c7b40: a routing op of a type on a connection (or none): the Rust op with its constructor's members
inline RoutingOp::RoutingOp(RoutingConnection* inConnection, RoutingOpType inType)
    : mConnection(inConnection), mRust(ee_won_routeop_new_on((int32_t)inType, static_cast<RefCount*>(inConnection))) {}

// The client, group and member model, the ignore list, the chat commands and the snapshots are a Rust object since
// net round 17 (rust/crates/ee_won_net/src/smartroute.rs EeWonSmartRoute: the maps +0x1c0..0x1d7, the iterators, the
// default group, this client's record, id and async flags, the command table and settings, the last input's state,
// the handled records, the ignore set and its file; RoutingOpCompleteHook 2c0be0's steps, HandleInput, FindClient
// 2c35a0, IgnoreClient 2c37d0, LoadIgnoreSet 2c3110, SaveIgnoreSet 2c3460 and the copies). The class hands each
// finished op's fields in and sends the request a chat input makes; its readers read the model's records over the
// C ABI (ee_won_smart_get, ee_won_smart_find; SmartRoutingConnection.cpp's copies are gone since R6 WONLobby round 12)
#define EE_WON_SMART_ROUTE_RUST 1 // the model is a Rust object (won_smartroute_test reads it so)
class SmartRoutingConnection : public RoutingConnection {
public:
    // (the SDK's MatchResult and InputCommand enums name the model's numbers, smartroute.rs; no game code names
    // one: the drivers' are tests/won/won_net37_orig.h)
    // 2bfb00 (the 0x258 allocation is this class): the ignore file under the WON directory, the default chat commands
    SmartRoutingConnection() : mModel(ee_won_smart_new_default(sizeof(wchar_t))) {}
    // 2c4120: this client's id and record forgotten, then the routing connection closed
    virtual void CloseHook() { ee_won_smart_close_hook(this->mModel, this->GetDataLock(), this->GetRouteObject()); }
    // 2c0be0: the maps updated from a finished op by its type; a chat, invitation or join attempt from an ignored
    // client loses its completion
    virtual void RoutingOpCompleteHook(RoutingOp* inOp)
    { ee_won_smart_routeop_hook(this->mModel, this->GetDataLock(), inOp->mRust, static_cast<RefCount*>(inOp)); }
    #include EE_WON_DRV(Routing_SmartRoutingConnection) // the drivers' members
    void StartGroupItr() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 1, 0, 0, 0); }
    bool StartMemberItr(unsigned short inId) { return ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 2, inId, 0, 0) != 0; }
    // 2c37d0: the client GetClient(id) finds ignored or not, the ignore list saved
    void IgnoreClient(unsigned short inClientId, bool inOn) { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Ignore, inClientId, inOn, 0, 0); }
    // a line of chat input: a command, a colon emote or a broadcast; its request sent (ee_won routecpp.rs)
    bool HandleInput(const std::wstring& inInput, int inGroupIdContext = -1)
    { return ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::HandleInput, (intptr_t) static_cast<RefCount*>(this), (intptr_t)inInput.data(), (intptr_t)inInput.length(), inGroupIdContext) != 0; }
    // 2c2fc0, the ignore file's path (provisional name)
    void SetProductName(const std::string& inPath) { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::SetPath, (intptr_t)inPath.data(), (intptr_t)inPath.length(), 0, 0); }
    void ClearProductName() { this->SetProductName(""); }
    unsigned short GetMyClientId() { return (unsigned short)ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::ClientId, 0, 0, 0, 0); } // OnChat 0x4d040c
    EeWonSmartRoute* mModel; // owned
protected:
    virtual ~SmartRoutingConnection() { ee_won_smart_free(this->mModel); }
private:
    void SetSmart(EeWonSmartNumber inWhich, bool inOn) { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Set, (intptr_t)inWhich, inOn, 0, 0); }
};

#include EE_WON_DRV(Routing_ns) // the drivers' functions
}

#endif
