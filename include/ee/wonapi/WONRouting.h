// WONAPI routing-server API as the game's WON front end (EEWonNet.cpp, 0xcd4ee..0xd3ff4) uses it. Class names
// come from RTTI (.?AVRoutingConnection@WONAPI@@, ...), members and offsets from EEWonNet.cpp's use; methods are
// named by address where the WONAPI name is unknown (provisional names). Requires <string>, <list>, <map>,
// ee/EEWonNet.h.
#ifndef EE_WONAPI_WONROUTING_H
#define EE_WONAPI_WONROUTING_H

#include "ee/wonapi/WONStatus.h"
#include <time.h>
#include <stdlib.h>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONAuthPeerData.h" //complete type needed by GetCertOp's users
#include "ee/wonapi/WONStringCompare.h"
#include <stdio.h> //FILE* member (RecvBytesOp::mFile)
#include <set> //AddrSet
#include <stdint.h>
#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "rust/won_net/admin.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/debug.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/dirlist.h"
#include "rust/won_net/ffi_net.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpcpp.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpengine.h"
#include "rust/won_net/ops.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/qsock.h"
#include "rust/won_net/reqop.h"
#include "rust/won_net/routeconn.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/routereq.h"
#include "rust/won_net/servconn.h"
#include "rust/won_net/smartroute.h"
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

// the status text (ee_won's status table; WONStatus.cpp, 0x29deb0, is deleted): a WS_ name, or a number valid until
// this thread's next unnamed status
inline const char* WONStatusToString(WONStatus inStatus) { return ee_won_status_text(inStatus); }

// WON's millisecond tick (was GetTickCount): real time, wraps every 2^32 ms like GetTickCount; compare by unsigned
// 32-bit difference
inline uint32_t WONTickCount() { return (uint32_t)PlatTimeMilliseconds64(); }


template <class T> class CompletionBase;

class AsyncOp;
class AsyncOpContainer;
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
    // (: AsyncOp's own vtable 0x84c834 has these three slots only; SocketOp and RoutingOp each introduce their
    // +0x0c/+0x10/+0x14: SocketOp's +0x14 returns 0, AcceptOp's a new AcceptOp; RoutingOp's +0x14 is GetType)

    WONStatus GetStatus() const { return ee_won_asyncop_status(this->mOp); }
    CompletionBase<SmartPtr<AsyncOp> >* GetCompletion() const
    { return static_cast<CompletionBase<SmartPtr<AsyncOp> >*>(ee_won_asyncop_completion(this->mOp)); }
    unsigned long GetTimeout() const { return ee_won_asyncop_timeout(this->mOp); }
    // takes a reference to the new completion and drops the old one's (a SmartPtr assignment)
    void SetCompletion(CompletionBase<SmartPtr<AsyncOp> >* theCompletion)
    { ee_won_asyncop_set_completion(this->mOp,theCompletion); }
    bool IsKilled() const { return ee_won_asyncop_killed(this->mOp); }
    bool IsCompletionPending() const { return ee_won_asyncop_completion_pending(this->mOp); }
    long GetMode() const { return ee_won_asyncop_run_mode(this->mOp); }
    uint32_t GetStartTime() const { return ee_won_asyncop_start_time(this->mOp); }
    bool IsRunAsyncImmediately() const { return ee_won_asyncop_run_async_immediately(this->mOp); }
    AsyncOpContainer* GetContainer() const { return static_cast<AsyncOpContainer*>(ee_won_asyncop_container(this->mOp)); }
    void SetContainer(AsyncOpContainer* inContainer); // NULL: WONAPICore's queue
    // the mode, unless the op runs (29f190)
    void SetMode(AsyncRunMode inMode) { ee_won_asyncop_set_mode(this->mOp,static_cast<int32_t>(inMode)); }
    // finishes even a finished op, unless its completion is pending (29f2f0)
    void ForceFinish(WONStatus inStatus) { ee_won_asyncop_force_finish(this->mOp,inStatus); }
    // an idle op as a pure timer firing after the timeout (29f610)
    void RunAsTimer(unsigned long inTimeout) { ee_won_asyncop_run_as_timer(this->mOp,static_cast<uint32_t>(inTimeout)); }
    // finishes a running op, its completion queued for async runs (29f220)
    void Finish(WONStatus inStatus) { ee_won_asyncop_finish(this->mOp,inStatus); }
    // unless the op runs (29f1b0)
    void SetTimeout(unsigned long inTimeout) { ee_won_asyncop_set_timeout(this->mOp,static_cast<uint32_t>(inTimeout)); }
    bool RunBlock(unsigned long inTimeout) { return this->Run(2,inTimeout)==0; } // 29f6e0: whether it succeeded
    void RunAsync(long inTimeout) { this->Run(1,inTimeout); }
    // marks the op killed and, when asked, finishes it killed (0x29f1e0)
    void Kill(bool inFlag) { ee_won_asyncop_kill(this->mOp,inFlag); }
    // whether an async run starts at once instead of being queued (0x29f1d0)
    void SetRunAsyncImmediately(bool inFlag) { ee_won_asyncop_set_run_async_immediately(this->mOp,inFlag); }
    void QueueInOpContainer() { ee_won_asyncop_queue_in_container(this->mOp); } // 29f200, or WONAPICore's queue
    // a pending completion: the timer entry dropped, the cleanup hook run, the completion called (29f3a0)
    void Complete() { ee_won_asyncop_complete(this->mOp); }
    // the time before the time-out; 0 for async runs or once it passed (0x29f700)
    unsigned long TimeLeft() { return ee_won_asyncop_time_left(this->mOp); }
    DWORD RealTimeLeft() { return ee_won_asyncop_real_time_left(this->mOp); }
    // the run mode, start and timeout for the Rust op algorithms (rust/crates/ee_won_net/src/ops.rs)
    EeWonOpTiming GetTiming() const { return ee_won_asyncop_timing(this->mOp); }
    bool IsAsync() const { return this->GetMode()==1; }
    //: 0x291983 (an inline COMDAT of EELobbyFileTransfer.cpp): finished without error
    bool Succeeded() { return this->GetStatus()==0 && !this->IsKilled(); }
    EeWonAsyncOp* GetOpObject() const { return this->mOp; }

private:
    AsyncOp(const AsyncOp&);
    AsyncOp& operator=(const AsyncOp&);
    EeWonAsyncOp* mOp;                      // owned
};
typedef SmartPtr<AsyncOp> AsyncOpPtr;

// Event: 8 bytes (vptr, HANDLE at 4); 29f7c0, embedded in AsyncOpContainer at 0x1c. A platform event (the
// original's alertable wait had no APC user); the timeout is 32-bit as WaitForSingleObjectEx's was
class Event {
public:
    explicit Event(bool manualReset=false,bool initiallySet=false)
        { this->mEvent=PlatEventCreate(manualReset,initiallySet); }
    virtual ~Event() { if(this->mEvent!=NULL) PlatEventDestroy(this->mEvent); }
    void Set() {PlatEventSet(this->mEvent);}
    void Reset() {PlatEventReset(this->mEvent);}
    bool WaitFor(unsigned long milliseconds)
        {return PlatEventWait(this->mEvent,(uint32_t)milliseconds)==0;}
    PlatEvent* mEvent; //04
};
// AsyncOpContainer: a Rust object (ee_won thread.rs EeWonOpContainer: the queue holding a reference per op, the op
// being pumped, the lock, the signal, the pump and the purge) behind this shim; each op's steps come back through
// AsyncOp.cpp's hooks. The original unit (AsyncOpContainer.cpp) is deleted.
class AsyncOpContainer {
public:
    AsyncOpContainer() : mContainer(ee_won_container_new(WonCore())) {} // the ops' calls answered in Rust
    virtual ~AsyncOpContainer() { ee_won_container_free(this->mContainer); }
    void QueueOp(AsyncOp* inOp) { ee_won_container_queue(this->mContainer,inOp); }
    void PurgeOps() { ee_won_container_purge(this->mContainer); }
    void Pump(unsigned long inWaitTime,int inMaxOps=0)
    { ee_won_container_pump(this->mContainer,static_cast<uint32_t>(inWaitTime),inMaxOps); }
    EeWonOpContainer* mContainer;
private:
    AsyncOpContainer(const AsyncOpContainer&);
    AsyncOpContainer& operator=(const AsyncOpContainer&);
};
inline void AsyncOp::SetContainer(AsyncOpContainer* inContainer)
{
    ee_won_asyncop_set_container(this->mOp,inContainer,inContainer!=NULL ? inContainer->mContainer : NULL);
}

class SocketOp;
// (MsgTransform, 2ad880, and its one class, CryptTransform, are Rust's since net round 29: ee_won cppcore.rs)

// WON SDK NetStats: static functions only. The counters are Rust's (rust/crates/ee_won_net/src/netstats.rs)
class NetStats
{
public:
	static void Enable(bool isEnabled) { ee_won_netstats_enable(isEnabled); }

	static void IncrementNumConstructedSockets(int theAmount) { ee_won_netstats_add(0, theAmount); }
	static void IncrementNumOpenSockets(int theAmount) { ee_won_netstats_add(1, theAmount); }

	static void IncrementBytesSent(int theAmount) { ee_won_netstats_add(2, theAmount); }
	static void IncrementBytesReceived(int theAmount) { ee_won_netstats_add(3, theAmount); }
	static void IncrementMessagesSent(int theAmount) { ee_won_netstats_add(4, theAmount); }
	static void IncrementMessagesReceived(int theAmount) { ee_won_netstats_add(5, theAmount); }

	static int GetNumConstructedSockets() { return static_cast<int>(ee_won_netstats_get(0)); }
	static int GetNumOpenSockets() { return static_cast<int>(ee_won_netstats_get(1)); }

	static __int64 GetNumBytesSent() { return ee_won_netstats_get(2); }
	static __int64 GetNumBytesReceived() { return ee_won_netstats_get(3); }
	static __int64 GetNumMessagesSent() { return ee_won_netstats_get(4); }
	static __int64 GetNumMessagesReceived() { return ee_won_netstats_get(5); }
};

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
    void RemoveAllMsgTransforms() { ee_won_asyncsocket_remove_all_transforms(this->mSock); }
    bool IsOpen() { return ee_won_asyncsocket_is_open(this->mSock); }
    void SetType(SocketType theType) { ee_won_asyncsocket_set_type(this->mSock, theType); }
    WONStatus SetNagling(bool on) { return ee_won_asyncsocket_set_nagling(this->mSock, on); }
    // the socket system started on first use, its users counted (Winsock's numbering), cdecl in the SDK
    static WONStatus StartWinsock() { return ee_won_socket_start(); }
    static WONStatus StopWinsock() { return ee_won_socket_stop(); }
    // an up network adapter with an IPv4 address neither 0.0.0.0 nor loopback (the original asked wininet)
    static bool HasInternetConnection() { return ee_won_has_internet_connection(); }
    WONStatus SetBroadcast(bool on) { return ee_won_asyncsocket_set_broadcast(this->mSock, on); }
    // a fresh non-blocking handle (broadcast for UDP, no linger for TCP), the open hook run
    WONStatus Open() { return ee_won_asyncsocket_open(this->mSock); }
    WONStatus RecvFrom(void*,int,IPAddr*,int*); // a signed count; the sender on success
    // closes a stream socket; dissolves a datagram socket's association
    void Disconnect() { ee_won_asyncsocket_disconnect(this->mSock); }
    WONStatus Bind(const IPAddr&,bool); // opened first; address reuse optional
    // connects: an existing stream connection dropped, opened and bound to the local host if needed
    WONStatus Fn2B0E60(const IPAddr& inAddress)
    { return ee_won_asyncsocket_connect_to(this->mSock, inAddress.GetValue(), static_cast<uint32_t>(IPAddr::GetLocalHost())); }
    WONStatus Accept(SmartPtr<AsyncSocket>&); // a new socket of the same type (the virtual Duplicate), a reference
    // success, the socket error or WS_TimedOut
    WONStatus WaitForConnect(unsigned long inWaitTime)
    { return ee_won_asyncsocket_wait_for_connect(this->mSock, static_cast<uint32_t>(inWaitTime)); }
    void WaitForAccept(unsigned long inTimeout) { this->WaitForRead(inTimeout); }
    // binds to the local host address on the port (the SDK's second parameter is bool)
    WONStatus Fn2B0D90(unsigned short inPort, bool inReuse) { return this->Bind(IPAddr(IPAddr::GetLocalHost(), inPort), inReuse); }
    WONStatus Listen(int inBacklog) { return ee_won_asyncsocket_listen(this->mSock, inBacklog); }
    void WaitForWrite(unsigned long); // up to the time (none for 0 or a closed socket)
    // WS_TimedOut (PartialSendTo for datagrams) on a short send
    WONStatus Send(const void* inData, int inLength, int* outSent)
    { return ee_won_asyncsocket_send(this->mSock, inData, inLength, outSent); }
    // after the socket thread is told and the close hook run
    void Close() { ee_won_asyncsocket_close(this->mSock); }
    void Kill() { ee_won_asyncsocket_kill(this->mSock); } // the close and kill hooks; separate from the SDK's Kill
    unsigned short GetLocalPort() { return ee_won_asyncsocket_local_port(this->mSock); } // or 0
    unsigned char GetLengthFieldSize() const { return ee_won_asyncsocket_length_field_size(this->mSock); }
    void SetLengthFieldSize(unsigned char inSize) { ee_won_asyncsocket_set_length_field_size(this->mSock, inSize); }
    bool IsConnected() { return ee_won_asyncsocket_is_connected(this->mSock); }
    // a copy taken under the lock (the original returned a reference to the member)
    IPAddr GetDestAddr() { return IPAddr::FromValue(ee_won_asyncsocket_dest_addr(this->mSock)); }
    void WaitForRead(unsigned long);
    // Winsock's numbering; would-block and in-progress are WS_TimedOut, any other recorded
    bool IsValid() { return ee_won_asyncsocket_is_valid(this->mSock); }
    // the transform chain, the received message in reverse order, the outgoing in order; the first failure
    WONStatus TransformRecvMsg(ByteBufferPtr& inMessage) { return ee_won_asyncsocket_transform_recv(this->mSock, &inMessage); }
    WONStatus TransformSendMsg(ByteBufferPtr& inMessage) { return ee_won_asyncsocket_transform_send(this->mSock, &inMessage); }
    WONStatus SendTo(const void*,int,const IPAddr&); // opened first if needed; fails on stream sockets
    // Shutdown when the peer closed, WS_TimedOut on a short stream read
    WONStatus Recv(void* outData, int inLength, int* outReceived)
    { return ee_won_asyncsocket_recv(this->mSock, outData, inLength, outReceived); }

    typedef void (*CloseCallback)(AsyncSocket*);
    // the socket's lock (the original's mDataCrit, a recursive mutex the Rust object owns), which the subclasses
    // (QueueSocket, the routing and server connections) take around their own state too: AutoCrit takes it
    PlatMutex* GetDataLock() const { return static_cast<PlatMutex*>(ee_won_asyncsocket_mutex(this->mSock)); }
    void SetDestAddr(const IPAddr& inAddr) { ee_won_asyncsocket_set_dest_addr(this->mSock,inAddr.GetValue()); }
    EeWonAsyncSocket* GetSocketObject() const { return this->mSock; }
private:
    AsyncSocket(const AsyncSocket&);
    AsyncSocket& operator=(const AsyncSocket&);
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
    static bool mRunAsyncImmediatelyDef; //WON SDK bool static at 565f08, zero-initialized
    // the op's socket (a reference held)
    AsyncSocket* GetSocket() { return static_cast<AsyncSocket*>(ee_won_socketop_socket(this->mSockOp)); }
    void SetSocket(AsyncSocket* theSocket) { ee_won_socketop_set_socket(this->mSockOp,theSocket); }
    // an op on the socket (a reference held): no socket thread, no waits, the event indices -1
    SocketOp(AsyncSocket* inSocket) : mSockOp(ee_won_socketop_new(static_cast<void*>(this),WonCore()))
    {
        this->SetSocket(inSocket);
        this->SetRunAsyncImmediately(SocketOp::mRunAsyncImmediatelyDef);
    }
    // an op of a class on the socket: the class's bytes (a reference taken), address and length or size
    SocketOp(EeWonCoreSocketOp inKind, AsyncSocket* inSocket = NULL, const ByteBuffer* inBytes = NULL,
             const IPAddr* inAddr = NULL, unsigned long inSize = 0) : mSockOp(ee_won_socketop_new(static_cast<void*>(this),WonCore()))
    {
        this->SetSocket(inSocket);
        this->SetRunAsyncImmediately(inKind != EeWonCoreSocketOp::QueueClose && SocketOp::mRunAsyncImmediatelyDef);
        ee_won_socketop_init_kind(this->mSockOp, inKind, const_cast<RefCount*>(static_cast<const RefCount*>(inBytes)),
                                  inAddr != NULL ? inAddr->GetValue() : NULL, static_cast<uint32_t>(inSize));
    }
    virtual ~SocketOp() { ee_won_socketop_free(this->mSockOp); } // the original's was implicit, 29153d
    // +0x04 0x2b6070: started (finished InvalidSocket first without a socket); a pending start goes to the socket
    // thread (async) or is continued and finished (blocking), any other result finishes it
    virtual void RunHook() { ee_won_socketop_run_hook(this->mSockOp); }
    virtual void CleanupHook();             // +0x08 0x2b60c0: out of the socket thread (AsyncOp.cpp)
    //: +0x0c starts the operation, +0x10 continues it: the kind's (SendFileOp 0x2915d2 / 0x291613 its own)
    virtual WONStatus Start() { return ee_won_socketop_start(this->mSockOp,this->GetTiming()); }
    virtual WONStatus Continue() { return ee_won_socketop_continue(this->mSockOp,this->GetTiming()); }
    // +0x14: a fresh op of the kind on the same socket, none for the others (provisional name)
    virtual AsyncOp* Duplicate()
    { return static_cast<AsyncOp*>(static_cast<RefCount*>(ee_won_socketop_duplicate(this->mSockOp))); }

    bool IsInSocketThread() const { return ee_won_socketop_in_socket_thread(this->mSockOp); }
    void SetWaitForEvent(int i,bool inFlag) { ee_won_socketop_set_wait_for_event(this->mSockOp,i,inFlag); }
    EeWonSocketOp* GetSocketOpObject() const { return this->mSockOp; }
    int GetKind() const { return ee_won_socketop_class(this->mSockOp); } // an EeWonCoreSocketOp, -1 for none
    static void HookAddToSocketThread(void* inOp); // WONAPICore's friend (AsyncOp.cpp: the core table's)
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

// what a receive's progress completion gets: 0x2b520d news one (0x48 bytes; RTTI RecvChunkOp : AsyncOp, vtable
// 0x84cae4) with +0x44 = the receiving op
class RecvChunkOp : public AsyncOp {
public:
    RecvChunkOp(SocketOp* inOp) : mRecvOp(inOp) {}
    SocketOp* GetRecvOp() const { return this->mRecvOp.get(); }
    virtual void RunHook() { this->Finish(0); }  //2b5370, 8-byte override of slot 04
private:
    SmartPtr<SocketOp> mRecvOp;          // +0x44
};


// the completion interface (vtable 0x43e4a0: ??_G, pure Complete)
template <class T> class CompletionBase : public RefCount {
public:
    virtual void Complete(T theParam) = 0;  // +0x04
};
// LightGetCertOp (0x48 bytes: AsyncOp plus the peer data at 0x44; PeerAuthOp::AsyncRefreshCert 2e74a0) is a plain
// AsyncOp for its run and ee_won's EeWonLightCert for its member (authflow.rs: the peer data GetCertCallback sets)

// ParamCompletion<AsyncOpPtr,SmartPtr<RefCount>>: 0x10 bytes,
// callback 8, owned param c, vtable 44ca3c -> deleting dtor 2aeab0, Complete 2ae840.
// Ctor 2ae7e0 consumes the by-value parameter and copies it into mParam.
// Normal destructor2aead0 has no own-vptr store; natural implicit cleanup required.
template <class ResultType, class ParamType>
class ParamCompletion : public CompletionBase<ResultType> {
    typedef void (*Callback)(ResultType, ParamType);
    Callback mCallback;
    ParamType mParam;
public:
    ParamCompletion(Callback inCallback, ParamType inParam)
        : mCallback(inCallback), mParam(inParam) {}
    virtual void Complete(ResultType inResult) { this->mCallback(inResult, this->mParam); }
};

typedef SmartPtr<CompletionBase<AsyncOpPtr> > OpCompletionBasePtr;

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
    void StartCloseTimer(long inTimeout) { ee_won_queuesocket_start_close_timer(this->mQueue, inTimeout); }
    void CancelCloseTimer() { ee_won_queuesocket_cancel_close_timer(this->mQueue); }
    void SetRepeatCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_queuesocket_set_repeat_completion(this->mQueue, inCompletion); }
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_queuesocket_set_close_completion(this->mQueue, inCompletion); }
    void SetQueueCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_queuesocket_set_queue_completion(this->mQueue, inCompletion); }
    void QueueOpFast(SocketOp* inOp, DWORD inTimeout=0xffffffff)
    { ee_won_queuesocket_queue_op_fast(this->mQueue, static_cast<AsyncOp*>(inOp), static_cast<uint32_t>(inTimeout)); }
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

// a queue socket the WON ops make and hold. Its blocking calls (Connect 2b1c10, SendBytes 2b1d00, SendMsg 2b1e30,
// RecvMsg 2b1f10, RecvCRMsg 2b2040, the SDK's SendBytesTo, RecvBytesFrom, RecvBytes: a socket op run blocking) are
// the op base's (ee_won hostsock.rs) since net round 23; no caller was left, and they went in round 24 (the test
// drivers keep the statements: tests/won/won_blocking.h)
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
class UDPManager;
// A Rust object since round 16 (rust/crates/ee_won_net/src/udpops.rs EeWonUdpRecvOp: the socket +0x44, the manager
// +0x48, the own-socket flag +0x4c, the address +0x50, the datagram +0x78, the check +0x7c and its context +0x80, and
// RunHook, CleanupHook, QueueRecv and RecvCallback); the class is a shim the lobby subclasses
class UDPManagerRecvOp : public AsyncOp {
public:
    UDPManagerRecvOp() : mUdp(ee_won_udprecvop_new(static_cast<RefCount*>(this), WonCore(), NULL)) {}
    UDPManagerRecvOp(const IPAddr& inAddress)
        : mUdp(ee_won_udprecvop_new(static_cast<RefCount*>(this), WonCore(), inAddress.GetValue())) {}
    virtual ~UDPManagerRecvOp() { ee_won_udprecvop_free(this->mUdp); } // the original's was implicit (2b6930)
    bool HasVerifier() { return ee_won_udprecvop_verifier(this->mUdp) != NULL; }
    void SetUDPManager(UDPManager* theManager);
    void SetSocket(QueueSocket* theSocket) { ee_won_udprecvop_set_socket(this->mUdp, static_cast<RefCount*>(theSocket)); }
    // a copy (the original returned a reference to the member)
    IPAddr GetFromAddr() const { return IPAddr::FromValue(ee_won_udprecvop_address(this->mUdp)); }
    void SetFromAddr(const IPAddr& theAddr) { ee_won_udprecvop_set_address(this->mUdp, theAddr.GetValue()); }
    const ByteBuffer* GetMsg() const
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_udprecvop_msg(this->mUdp))); }
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udprecvop_socket(this->mUdp)))); }
    UDPManager* GetUDPManager() const;
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
    void SetMessageVerifier(MessageVerifier inVerifier, void* inContext)
    { ee_won_udprecvop_set_verifier(this->mUdp, reinterpret_cast<void*>(inVerifier), inContext); }
private:
    UDPManagerRecvOp(const UDPManagerRecvOp&);
    UDPManagerRecvOp& operator=(const UDPManagerRecvOp&);
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
    virtual ~UDPManagerSendRecvOp() { ee_won_udpsendrecvop_free(this->mUdp); } // the original's was implicit (2b7140)
    void SetUDPManager(UDPManager* theManager);
    void SetMsgTimeout(DWORD theTimeout) { ee_won_udpsendrecvop_set_timeout(this->mUdp, static_cast<uint32_t>(theTimeout)); }
    void SetNumRetransmits(int theNum) { ee_won_udpsendrecvop_set_retry_limit(this->mUdp, theNum); }
    void SetDestAddr(const IPAddr& theAddr) { ee_won_udpsendrecvop_set_address(this->mUdp, theAddr.GetValue()); }
    IPAddr GetDestAddr() const { return IPAddr::FromValue(ee_won_udpsendrecvop_address(this->mUdp)); } // a copy
    // the request datagram
    void SetOutgoing(const ByteBuffer* theMsg)
    { ee_won_udpsendrecvop_set_outgoing(this->mUdp, const_cast<RefCount*>(static_cast<const RefCount*>(theMsg))); }
    const ByteBuffer* GetOutgoing() const
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_udpsendrecvop_outgoing(this->mUdp))); }
    // the op's own socket (none with a manager)
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udpsendrecvop_socket(this->mUdp)))); }
    // the reply wait
    UDPManagerRecvOp* GetRecvOp() const
    { return static_cast<UDPManagerRecvOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(ee_won_udpsendrecvop_recv_op(this->mUdp)))); }
    int GetAttemptCount() const { return ee_won_udpsendrecvop_attempts(this->mUdp); }
    int GetNumRetransmits() const { return ee_won_udpsendrecvop_retry_limit(this->mUdp); }
    unsigned long GetMsgTimeout() const { return ee_won_udpsendrecvop_timeout(this->mUdp); }
    virtual void RunHook() { ee_won_udpsendrecvop_run(this->mUdp); }
    virtual void CleanupHook() { ee_won_udpsendrecvop_cleanup(this->mUdp); }
    virtual WONStatus CheckResponse(ReadBuffer&) { return 0; } //2ad870: any response (folded with others)
    EeWonUdpSendRecvOp* GetUdpObject() const { return this->mUdp; }
private:
    UDPManagerSendRecvOp(const UDPManagerSendRecvOp&);
    UDPManagerSendRecvOp& operator=(const UDPManagerSendRecvOp&);
    EeWonUdpSendRecvOp* mUdp;
};

// A Rust object since round 15 (rust/crates/ee_won_net/src/udpmgr.rs EeWonUdpManager: the socket +0x08, the default
// completion +0x0c, the auto-receive op +0x10, the clean-up cursors +0x14/+0x18, the list +0x1c and the multimap
// +0x28 of waiting receive ops, the receive size +0x34, and Close, SetAutoRecv, QueueOp, Send, CleanOps, Bind, the
// erases 0x2b8160/0x2b81b0 and RecvCallback's dispatch). The original's mAutoRecv (+0x38) was never read: gone
class UDPManager : public RefCount {
public:
    UDPManager() : mManager(ee_won_udpmanager_new(static_cast<void*>(this), WonCore())) {}
    virtual ~UDPManager() { ee_won_udpmanager_free(this->mManager); } // closes nothing (the original's 2b77b0)
    // rebinds to a local port (optionally broadcast) and receives datagrams repeatedly
    WONStatus Bind(int inPort, bool inBroadcast) { return ee_won_udpmanager_bind(this->mManager, inPort, inBroadcast); }
    void SetDefaultCompletion(CompletionBase<AsyncOpPtr>* theCompletion)
    { ee_won_udpmanager_set_default_completion(this->mManager, static_cast<RefCount*>(theCompletion)); }
    void Send(const ByteBuffer* inData, const IPAddr& inAddress) // provisional name (the SDK has SendBytesTo)
    { ee_won_udpmanager_send(this->mManager, const_cast<RefCount*>(static_cast<const RefCount*>(inData)), inAddress.GetValue()); }
    void Close() { ee_won_udpmanager_close(this->mManager); }
    void SetAutoRecv(bool inAutoRecv) { ee_won_udpmanager_set_auto_recv(this->mManager, inAutoRecv); }
    void QueueOp(UDPManagerRecvOp* inOp, unsigned long inTimeout)
    { ee_won_udpmanager_queue_op(this->mManager, static_cast<RefCount*>(inOp), static_cast<uint32_t>(inTimeout)); }
    void CleanOps() { ee_won_udpmanager_clean_ops(this->mManager); }
    // the socket (NULL when unbound)
    QueueSocket* GetSocket() const
    { return static_cast<QueueSocket*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_udpmanager_socket(this->mManager)))); }
    EeWonUdpManager* GetManagerObject() const { return this->mManager; }
private:
    UDPManager(const UDPManager&);
    UDPManager& operator=(const UDPManager&);
    EeWonUdpManager* mManager;
};
inline void UDPManagerRecvOp::SetUDPManager(UDPManager* theManager)
{ ee_won_udprecvop_set_manager(this->mUdp, static_cast<RefCount*>(theManager)); }
inline UDPManager* UDPManagerRecvOp::GetUDPManager() const
{ return static_cast<UDPManager*>(static_cast<RefCount*>(ee_won_udprecvop_manager(this->mUdp))); }
inline void UDPManagerSendRecvOp::SetUDPManager(UDPManager* theManager)
{ ee_won_udpsendrecvop_set_manager(this->mUdp, static_cast<RefCount*>(theManager)); }


// a completion calling a member of T (RTTI LobbyCompletion<T> > LobbyCompletionBase > CompletionBase<AsyncOpPtr>;
// vtables 0x84c49c / 0x84c4ac; provisional member names)
class LobbyCompletionBase : public CompletionBase<AsyncOpPtr> {
public:
    LobbyCompletionBase() throw() {}
    virtual void Complete(AsyncOpPtr theParam);     // +0x04 0x357d70 (calls +0x08)
    virtual void Run(AsyncOp* inOp) {}      // +0x08
};

template <class T> class LobbyCompletion : public LobbyCompletionBase {
public:
    typedef void (T::*Callback)(AsyncOp* inOp);
    LobbyCompletion(Callback inCallback, T* inObject) : mCallback(inCallback), mObject(inObject) {}
    virtual void Run(AsyncOp* inOp) { (this->mObject.mPtr->*mCallback)(inOp); }

    Callback mCallback;                     // +0x08
    SmartPtr<T> mObject;                    // +0x0c
};


// RTTI LogicCompletionBase4558cc derives LobbyCompletionBase; distinct flag-bearing subclass.
class LogicCompletionBase : public LobbyCompletionBase {
public:
    LogicCompletionBase() : mCheckTracker(false) {}
    bool mCheckTracker;                    // +0x08; original3df809 and3e5b6e
};

template <class T> class LogicCompletion : public LogicCompletionBase {
public:
    typedef void (T::*Callback)(AsyncOp* inOp);
    LogicCompletion(Callback inCallback, T* inObject) : mCallback(inCallback), mObject(inObject) {}
    // unless the logic was killed or (checked) the op was not its tracked one (opobj.rs decides, R6 WON round 33)
    virtual void Run(AsyncOp* inOp)
    {
        if (ee_won_logic_completion_runs(this->mObject->mKilled, this->mCheckTracker, &Untrack, this->mObject.mPtr, inOp))
            (this->mObject.mPtr->*mCallback)(inOp);
    }
    static bool Untrack(void* inLogic, void* inOp) { return static_cast<T*>(inLogic)->RemoveTrackedOp(static_cast<AsyncOp*>(inOp)); }
    Callback mCallback;                    // +0x0c
    SmartPtr<T> mObject;                    // +0x10; original allocation0x14
};

// HTTPConnection (partial), RTTI 44d3a4 > RefCount
// Held only through SmartPtr (maps); never constructed, sized or copied here.
class RustServerOp; // an HTTPGetOp (httpget.rs)
class HTTPSession;
// HTTPConnection: a host's keep-alive connection; its socket, queues, running ops, pipelining and retry state are
// ee_won's (rust/crates/ee_won_net/src/httpengine.rs EeWonHttpConn): this class is the RefCount its ops' completions hold.
// Its calls back are Rust's since net round 26 (httpcpp.rs, over the core table; HTTPEngine.cpp is deleted); the
// forwarders that take an op are below RustServerOp.
class HTTPConnection : public RefCount {
public:
    // 2d2c80 (new 64 at 2d2a97): idle, pipelining on
    HTTPConnection(HTTPSession* inSession = NULL) : mSession(inSession)
    { this->mImpl = ee_won_http_conn_new_core(static_cast<RefCount*>(this), WonCore()); }
    virtual ~HTTPConnection() { ee_won_http_conn_free(this->mImpl); } //2d2d50: the socket, queues and completions
    void Kill() { ee_won_http_conn_kill(this->mImpl); } //2d3600: the running and queued requests killed (finished)
    void QueueOp(RustServerOp* inOp); //2d33a0: send-only on the pipeline queue, a full request without pipelining
    void QueueOp(RustServerOp* inOp, int inId); //2d3290: on the send (0) or receive (1) queue, started when it was empty
    void RunOp(int inId) { ee_won_http_conn_run_op(this->mImpl, inId); } //2d3430: a queue's next request on the socket
    int GetNumOps() { return ee_won_http_conn_num_ops(this->mImpl); }
    PlatMutex* GetLock() const { return this->mDataCrit.mCrit; }
    EeWonHttpConn* GetConnObject() const { return this->mImpl; }
protected:
    // 2d2e80: a sent request queued for its reply, or completed; all requeued when the pipeline drops
    void OpCallback(AsyncOp* inOp) { ee_won_http_conn_op_callback(this->mImpl, static_cast<RefCount*>(inOp)); }
private:
    CriticalSection mDataCrit; //08..20
    HTTPSession* mSession; //20 NONOWNING, ctor raw assignment
    EeWonHttpConn* mImpl;   // owned: 0x24..0x64 (socket, counts, flag, last use, queues, retry, running ops)
};
typedef SmartPtr<HTTPConnection> HTTPConnectionPtr;

// 0x2c bytes: constructor 2d27b0, map destruction 2d2870. The connections by host (StringLessNoCase) are ee_won's
// (httpengine.rs EeWonHttpSession).
class HTTPSession : public RefCount {
public:
    HTTPSession() { this->mImpl = ee_won_http_session_new_core(static_cast<RefCount*>(this), WonCore()); } //2d27b0
    virtual ~HTTPSession() { ee_won_http_session_free(this->mImpl); } //2d2870, the map
    void AddOp(RustServerOp*); //2d28f0: on its host's connection, made at the first use
    void KillAll() { ee_won_http_session_kill_all(this->mImpl); } //2d2bc0: every connection's requests killed
    int GetNumOps() { return ee_won_http_session_num_ops(this->mImpl); }
    PlatMutex* GetLock() const { return this->mDataCrit.mCrit; }
    EeWonHttpSession* GetSessionObject() const { return this->mImpl; }
private:
    CriticalSection mDataCrit;              //08..20
    EeWonHttpSession* mImpl;                //20..2c, owned
};
typedef SmartPtr<HTTPSession> HTTPSessionPtr;


class RoutingConnection;
class PeerAuthOp;

// (the routing server's records, RoutingClientInfo .. RoutingBadUser, are the Rust ops' since net round 18 and
// are read in Rust since R6 WONLobby round 12: ee_won_lobby modelobj.rs, EEWonNet.cpp and the lobby's bad user list
// over ee_won_routeop_records; the drivers keep the C++ records, tests/won/won_routerecords.h)
enum RoutingBadUserListType {
    RoutingBadUserListType_BannedUsers=1,
    RoutingBadUserListType_MutedUsers=2
};

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
    //2c7d20: in blocking mode finishes BlockMode_NotSupported (100009), else SendRequest
    virtual void RunHook() { if (this->GetMode() == OP_MODE_BLOCK) this->Finish(100009); else this->SendRequest(); }
    // the op's request (Rust's), sent on the connection; added to its list when it waits for a reply
    virtual void SendRequest() { ee_won_routeop_send(this->mRust, static_cast<RefCount*>(this)); }
    virtual RoutingOpType GetType() const { return (RoutingOpType)ee_won_routeop_type(this->mRust); }
    RoutingConnection* GetConnection() { return this->mConnection.get(); }

    // the members (EE_WON_RTF_* numbers, EE_WON_RTT_* texts)
    long long Num(int inField) const { return ee_won_routeop_num(this->mRust, inField); }
    void SetNum(int inField, long long inValue) { ee_won_routeop_set_num(this->mRust, inField, inValue); }
    std::wstring Text(int inWhich) const { std::wstring s; ee_won_routeop_text(this->mRust, inWhich, WonWideSink, &s); return s; }
    void SetText(int inWhich, const std::wstring& inText)
    { ee_won_routeop_set_text_wide(this->mRust, inWhich, inText.data(), sizeof(wchar_t), inText.length()); }
    std::string DataType() const { std::string s; ee_won_routeop_data_type(this->mRust, WonStringSink, &s); return s; }
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
    unsigned short GetLinkId() const { return (unsigned short)this->Num(EE_WON_RTF_LINK_ID); }

    //2c5f70, the group list op's body into its map (false when it runs short; what was read stays)
    bool ParseGroupListObj(const void* inData, unsigned long inSize)
    { return ee_won_routeop_parse_group_list(this->mRust, static_cast<const uint8_t*>(inData), inSize); }

    SmartPtr<RoutingConnection> mConnection; //60, destructor owns release
    EeWonRouteOp* mRust; // owned
protected:
    virtual ~RoutingOp() { ee_won_routeop_free(this->mRust); }
};
typedef SmartPtr<RoutingOp> RoutingOpPtr;
#define EE_WON_ROUTE_OPS_RUST 1 // the ops are Rust objects (won_routeops_test reads them so)

// The friend status map (the original's +0x88), the request datagram and the reply's read are a Rust object since
// net round 19 (rust/crates/ee_won_net/src/userstatus.rs EeWonUserStatus); the map is read and set in Rust
class RoutingGetUserStatusOp : public UDPManagerSendRecvOp {
public:
    typedef std::map<unsigned long,unsigned char> FriendStatusMap;
    // 2c6630: a query to the server address, 3 retransmits, a 1 s message timeout
    RoutingGetUserStatusOp(const IPAddr& inAddress, UDPManager* inManager) : mStatus(ee_won_userstatus_new())
    { ee_won_udpsendrecvop_setup(this->GetUdpObject(), inAddress.GetValue(), static_cast<RefCount*>(inManager), 3, 1000); }
    virtual ~RoutingGetUserStatusOp() { ee_won_userstatus_free(this->mStatus); } // (the original's implicit 2c6750)
    // 2c6970: the datagram listing every queried id, then the send and receive
    virtual void RunHook()
    {
        ByteBufferPtr aMessage;
        ee_won_userstatus_request(this->mStatus, WonByteBufferSink, &aMessage);
        this->SetOutgoing(aMessage);
        UDPManagerSendRecvOp::RunHook();
    }
    // 2c6ab0: the reply's header, count and each user's status (a reply cut short: an unpack failure, what was
    // read kept)
    virtual WONStatus CheckResponse(ReadBuffer& inMessage)
    {
        size_t aPos = 0;
        WONStatus aStatus = ee_won_userstatus_reply(this->mStatus, reinterpret_cast<const uint8_t*>(inMessage.data()),
                                                    inMessage.length(), (size_t)inMessage.pos(), &aPos);
        inMessage.SetPos((int)aPos);
        return aStatus;
    }
    void AddWONUserId(unsigned long inId) { ee_won_userstatus_add(this->mStatus, inId); } //2c68c0, status 0 until the reply
    // the friend status map, read and set by the routing logic in Rust (R6 WON round 32: netlogicops.rs; the
    // original's GetFriendStatusMap / SetFriendStatusMap copies)
    EeWonUserStatus* GetRust() const { return this->mStatus; }
private:
    EeWonUserStatus* mStatus; // owned
};
#define EE_WON_USER_STATUS_RUST 1 // the map is a Rust object (won_userstatus_test sets it so)


inline const char* GetBufferData(const ByteBufferPtr& inBuffer)
{
    return inBuffer->data();
}

// the WON server list the account ops use (??_G 0xceaec, dtor 0xceb08; sizeof 0x3c).: layout from
// the dtor and the base game's out-of-line ctor (base exe 0x4c772a; AoC's WONAPI inlines it: 0x2ddb70, 0x3628c0);
// (provisional member names)
typedef std::set<IPAddr> AddrSet;
// A shim over ee_won's server list (rust/crates/ee_won_net/src/servctx.rs: the addresses, their order, the one-time
// shuffle and the lock); the original's 0x3c-byte layout is gone
// a shim over ee_won's server list (servctx.rs: the addresses, their order, the one-time shuffle and the lock);
// header-only since net round 27 (ServerContext.cpp deleted): it converts IPAddr values; the directory's entities are
// walked in Rust (dirlist.rs, R6 WON round 34: the path-map overloads had no caller and went with DirEntityMap)
class ServerContext : public RefCount {
public:
    EeWonServerList* GetListObject() const { return this->mList; }
    ServerContext() : mList(ee_won_ctx_new()) {}
    virtual ~ServerContext() { ee_won_ctx_free(this->mList); }
    // a valid address not yet in the list appended; whether it was
    bool AddAddress(const IPAddr& inAddress) { EeWonAddr a = inAddress.ToRaw(); return ee_won_ctx_add(this->mList, &a); } //2ac4b0
    void Clear() { ee_won_ctx_clear(this->mList); } //2ac390: the one-time shuffle re-enabled
    // a server that failed to the end of the list
    void NotifyFailed(const IPAddr& inAddress) { EeWonAddr a = inAddress.ToRaw(); ee_won_ctx_notify_failed(this->mList, &a); } //2ac980
    // the directory's services' addresses (only those of the name when one is given)
    bool AddAddressesFromDir(const DirList& inDir, const wchar_t* inNameFilter=NULL) //2acab0/332
    { return ee_won_ctx_add_from_dir(this->mList, inDir.get(), inNameFilter, WideLength(inNameFilter), sizeof(wchar_t)); }
    void RemoveAllAddresses() { this->Clear(); } //2acc00
    unsigned long GetNumAddresses() { return (unsigned long)ee_won_ctx_len(this->mList); }
    typedef std::list<IPAddr> AddrList;
    // shuffled the first time (the CRT's srand(time(NULL)) and rand, as random_shuffle drew)
    void CopyAddresses(AddrList& outCopy) { outCopy.clear(); ee_won_ctx_copy(this->mList, Rand, ToList, &outCopy); } //2ac6d0
    bool RemoveAddress(const IPAddr& inAddress) { EeWonAddr a = inAddress.ToRaw(); return ee_won_ctx_remove(this->mList, &a); }
    void CopyAddresses(AddrSet& outCopy) { outCopy.clear(); ee_won_ctx_copy_sorted(this->mList, ToSet, &outCopy); }
    // cleared and refilled under the list's one lock
    bool AtomicClearAndAddAddressesFromDir(const DirList& inDir, const wchar_t* inNameFilter=NULL)
    { return ee_won_ctx_replace_from_dir(this->mList, inDir.get(), inNameFilter, WideLength(inNameFilter), sizeof(wchar_t)); }
    // the original's public mShuffleAddresses (copies shuffle once while it is on)
    void SetShuffleAddresses(bool inShuffle) { ee_won_ctx_set_shuffle(this->mList, inShuffle); }

private:
    ServerContext(const ServerContext&);
    ServerContext& operator=(const ServerContext&);
    static size_t WideLength(const wchar_t* inText) { return inText != NULL ? wcslen(inText) : 0; }
    static IPAddr FromRaw(const EeWonAddr* inRaw) { IPAddr a; a.FromRaw(*inRaw); return a; }
    static void ToList(void* inList, const EeWonAddr* inRaw) { static_cast<AddrList*>(inList)->push_back(FromRaw(inRaw)); }
    static void ToSet(void* inSet, const EeWonAddr* inRaw)
    { AddrSet* s = static_cast<AddrSet*>(inSet); s->insert(s->end(), FromRaw(inRaw)); }
    static int32_t Rand(bool inSeed) { if (inSeed) { srand((unsigned)time(NULL)); return 0; } return rand(); }
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

// an op that tracks sub-ops (the tracked sub-ops' completion is Rust's: ee_won opobj.rs EeWonAsyncOp::track; the
// original's TrackerCompletion, 16 bytes, and its Callback, 0x2a00d0, are gone)
class AsyncOpWithTracker : public AsyncOp {
public:
    // tracks a sub-op under the parameter, its completion back to this op (2a0100, protected in the SDK): while this
    // op is pending and still tracks it, the sub-op leaves the tracker and CallbackHook runs
    AsyncOp* Track(AsyncOp* op,int param)
    {
        return static_cast<AsyncOp*>(ee_won_asyncop_track(this->GetOpObject(),this->mTracker.GetTrackerObject(),op,
                                                          op->GetOpObject(),param));
    }
    virtual void CleanupHook() { this->mTracker.KillAll(); } //slot08 2a0290
    virtual bool CallbackHook(AsyncOp*,int) { return false; } //slot0c 2accc0: ignores the sub-op
    AsyncOpTracker mTracker;                //44..78
};

class BlockingSocket;
// ServerOp (0x94 bytes, ctor 2acc10): the base of the server ops. RTTI: PeerAuthOp > ServerOp > AsyncOpWithTracker >
// AsyncOp > RefCount. Its members (mSocket +0x78, mAuthContext +0x7c, mAuthType +0x80, mLengthFieldSize +0x84,
// mClientKeySize +0x85, mUseClientKey +0x86, mMaxConnectTime, mMaxSendTime, mMaxRecvTime +0x88..+0x90) and its
// ConnectTime 2ace50, SendTime 2ace80 and RecvTime 2aceb0 are the Rust op's since net round 24 (ee_won hostbase.rs):
// RustServerOp's accessors. The SDK's SendMsgAsync / RecvMsgAsync (no caller) went with them: the Rust ops send and
// receive through their own calls.
class ServerOp : public AsyncOpWithTracker {
protected:
    ServerOp() {}
};

// An op of the Rust op base (rust/crates/ee_won_net/src/hostop.rs): its members and steps are the Rust op's
// (MultiPingOp: ee_won_multiping_new, FilePushOp: ee_won_filepush_new, FTPGetOp: ee_won_ftpget_new and _new_url;
// the ops derived from ServerRequestOp: NewRequest over reqhost.rs); this class runs its hooks there, and its calls
// on the C++ objects are answered over the core table (hostcore.rs). Results and setters by number:
// ee_won_hostop_num, ee_won_hostop_bytes, ee_won_hostop_set.
class RustServerOp : public ServerOp {
public:
    explicit RustServerOp(EeWonHostOp* inRustOp); // takes the op; its length field and time limits, no auth
    // ServerRequestOp(context or address) with a Rust op derived from it (ee_won_reqop_* constructors), taken
    static RustServerOp* NewRequest(EeWonReqOp* inOp, ServerContext* inContext);
    static RustServerOp* NewRequest(EeWonReqOp* inOp, const IPAddr& inAddr);
    IPAddr GetCurAddr() const; // a request's current server (EE_WON_REQHOST_CUR_ADDR)
    IPAddr GetAddr() const; // a request's first server (an invalid address: none)
    void Set(int inWhich, RefCount* inObj); // an object setter (the op takes its own reference)
    void SetNum(int inWhich, long long inNum); // a number setter
    EeWonHostOp* GetRustOp() const { return this->mRustOp; }
    // the results of a directory or account op (ee_won reqdir.rs): its entities by number and element
    // (EE_WON_REQOP_DIR_LIST and a query's index, EE_WON_REQOP_DIR_SERVICE), a number, bytes
    DirList GetEntities(int inWhich, long long inIndex = 0) const { return DirList(ee_won_dirlist_of_op(this->mRustOp, inWhich, inIndex)); }
    long long GetNum(int inWhich, long long inIndex = 0) const { return ee_won_hostop_num(this->mRustOp, inWhich, inIndex); }
    std::string GetBytes(int inWhich, long long inIndex = 0) const;
    // the directory and account ops the game makes (ee_won reqdir.rs), with their members
    static RustServerOp* NewGetDir(ServerContext* inContext, const std::wstring& inPath, DWORD inFlags,
                                   const char* const* inTypes = NULL, int inTypeCount = 0);
    static RustServerOp* NewGetMultiDir(ServerContext* inContext, DWORD inFlags, const std::wstring* inPaths, int inCount);
    static RustServerOp* NewGetAccount(ServerContext* inContext, AuthContext* inAuth);
    static RustServerOp* NewUpdateAccount(ServerContext* inContext, AuthContext* inAuth, const std::wstring& inPassword,
                                          const std::string& inEmail);
    // the create account and the group list request (ee_won reqlast.rs): the lobby's profile (newsletter 1 and
    // the birth date); a routing server's group list flags, its replies read again by RouteGroupListOf
    static RustServerOp* NewCreateAccount(ServerContext* inContext, const std::wstring& inUserName,
                                          const std::wstring& inPassword, const std::string& inEmail,
                                          bool inNewsletter, unsigned char inMonth, unsigned char inDay,
                                          unsigned short inYear);
    static RustServerOp* NewGroupListReq(const IPAddr& inAddr, unsigned char inFlags);
    // HTTPGetOp(host, path) and HTTPGetOp(url) (ee_won httpget.rs; the web proxy file read at the first one made),
    // a bytes setter, and its readers that are the C++ side's: GetSocket() (NULL unless connected) and
    // GetLastModifiedTime() (the local file's time when the reply had none)
    static RustServerOp* NewHttpGet(const std::string& inHost, const std::string& inPath);
    static RustServerOp* NewHttpGet(const std::string& inURL);
    void SetBytes(int inWhich, const std::string& inBytes);
    BlockingSocket* GetHttpSocket() const;
    // GetHTTPDocumentOp(product, type) (ee_won httpdoc.rs; the type an HTTPDocType, its files in the WON directory)
    static RustServerOp* NewHttpDocument(const std::string& inProduct, int inType);
    ByteBufferPtr GetHttpDocument(int inOwner) const // GetDocument(owner): the cache file, NUL-ended
    {
        std::string aBytes = this->GetBytes(EE_WON_HTTPDOC_DOCUMENT, inOwner);
        return new ByteBuffer(aBytes.data(), (unsigned long)aBytes.size(), false);
    }
    // ServerOp's members (ee_won hostbase.rs): the time limits (2ace20, 2acee0), the socket and auth context
    // (borrowed; the op holds a reference), the auth type, the length field, the client key
    void SetMaxTimes(int inConnect, int inSend, int inRecv);
    BlockingSocket* GetSocket() const;
    void SetSocket(BlockingSocket* inSocket);
    AuthType GetAuthType() const { return (AuthType)ee_won_hostop_base_get(this->mRustOp, EE_WON_HOSTBASE_AUTH_TYPE); }
    void SetAuthType(AuthType inType, AuthContext* inContext = NULL);
    void SetLengthFieldSize(unsigned char inSize) { ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_LFS, inSize, NULL); }
    void SetClientKeySize(unsigned char inSize, bool inUseClientKey);
    virtual void RunHook();
    virtual void CleanupHook();
    virtual bool CallbackHook(AsyncOp* inOp, int inParam);
protected:
    virtual ~RustServerOp();
private:
    static void StrSink(void* inContext, const uint8_t* inData, size_t inLength);
    IPAddr AddrOf(int inWhich) const;
    EeWonHostOp* mRustOp; // owned (with the op's file: ee_won hostnative.rs)
};
typedef SmartPtr<RustServerOp> RustServerOpPtr;
// the HTTP engine's forwarders that take an HTTP get op (HTTPEngine.cpp's, 2d33a0, 2d3290, 2d28f0)
inline void HTTPConnection::QueueOp(RustServerOp* inOp)
{ ee_won_http_conn_queue_op(this->mImpl, static_cast<RefCount*>(static_cast<AsyncOp*>(inOp))); }
inline void HTTPConnection::QueueOp(RustServerOp* inOp, int inId)
{ ee_won_http_conn_queue_op_id(this->mImpl, static_cast<RefCount*>(static_cast<AsyncOp*>(inOp)), inId); }
inline void HTTPSession::AddOp(RustServerOp* inOp)
{ ee_won_http_session_add_op(this->mImpl, static_cast<RefCount*>(static_cast<AsyncOp*>(inOp))); }

// AuthSession (0x10cc bytes in the original) is an ee_won handle (hostauth.rs over peerauth.rs's session, round 22):
// the peer auth op that makes it, the auth context's session cache and the sockets' transforms hold it.


// The directory server's entity replies: the reading and the lists of what it read are ee_won's (dir.rs, dirlist.rs:
// DirEntityReplyParser::TakeEntity is gone with DirEntity, R6 WON round 34). GetEntityRequest, GetDirOp,
// GetMultiDirOp, GetAliasDirOp, FindEntityOp and GetServiceOp are Rust ops (rust/crates/ee_won_net/src/reqdir.rs).

// PeerAuthOp (0x2244 bytes, ctor 2e6a10) and PeerAuthServerOp (0x2240, ctor 2e8950): Rust ops on the base (ee_won
// peerop.rs over peerobj.rs: the address, the certificate refresh status, the peer data, the handshake of
// PeerAuthClient / PeerAuthServer, the session and every step); these classes are their constructors (the auth
// context, the auth type and the socket are ServerOp's members) and the accessors the game reads.
class PeerAuthServerOp : public RustServerOp {
public:
    PeerAuthServerOp(AuthContext* inContext, BlockingSocket* inSocket);
    // the client's certificate's VerifyPermissions(0, community, 0) after the handshake (LanLogic's check)
    bool ClientPermits(DWORD inCommunity) const { return this->GetNum(EE_WON_PEEROP_CLIENT_PERMITS, inCommunity) != 0; }
};
class PeerAuthOp : public RustServerOp {
public:
    PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket, bool inUseAuth2 = true);
};

// HTTP operation, 0x15c bytes.
class FileReader;
class FileWriter;
// HTTPCache and HTTPCacheEntry (cache 0x34 bytes, entry 0x68) are Rust's whole (rust/crates/ee_won_net/src/httpcache.rs):
// the HTTP get op's cache and entry are Rust handles; the global cache is ee_won_http_cache_global
// HTTPGetOp is a Rust op on the base (rust/crates/ee_won_net/src/httpget.rs): RustServerOp::NewHttpGet, its members by
// EE_WON_HTTPGET_* number

// DNS operation, 0x7c bytes; explicit destructor and borrowed address getter.
// ThreadBase 0x14 / Thread 0x1c. Event (8 bytes) comes from AsyncOpContainer above.
// Base ctor 2a6c10 and base dtor 2a6c80; DNSThread allocation 0x20 at 2d980a.
enum ThreadPriority { ThreadPriority_Highest,ThreadPriority_High,ThreadPriority_Normal,ThreadPriority_Low,ThreadPriority_Lowest };
// ThreadBase / Thread: a Rust object (ee_won thread.rs EeWonThread: the run flags, priority, signal event and the
// platform thread) behind these shims; the object's thread calls the virtual ThreadFunc (and deletes an
// auto-deleting thread after it). The original units (Thread.cpp: the start, stop and priority hooks) are deleted.
class ThreadBase {
protected:
    virtual void ThreadFunc()=0;
public:
    ThreadBase();
    virtual ~ThreadBase() { ee_won_thread_free(this->mThread); }
    virtual void Signal() { ee_won_thread_signal(this->mThread); }
    void Start() { ee_won_thread_start(this->mThread); }
    // marks the thread stopped, signals it and waits for it when asked (nothing when it was stopped)
    void Stop(bool waitForStop=true) { ee_won_thread_stop(this->mThread,waitForStop); } // (thread.rs; Signal has no override)
    bool InThisThread() { return ee_won_thread_in_this_thread(this->mThread); }
    void WaitForStop() { ee_won_thread_wait_for_stop(this->mThread); }
    void SetPriority(ThreadPriority thePriority)
    { ee_won_thread_set_priority(this->mThread,static_cast<int32_t>(thePriority)); }
protected:
    // the original's mStopped and mSignalEvent, as a ThreadFunc reads them
    bool IsStopped() const { return ee_won_thread_is_stopped(this->mThread); }
    bool WaitForSignal(unsigned long inMilliseconds)
    { return ee_won_thread_wait_signal(this->mThread,static_cast<uint32_t>(inMilliseconds)); }
    EeWonThread* mThread;
private:
    ThreadBase(const ThreadBase&);
    ThreadBase& operator=(const ThreadBase&);
    static void RunThreadFunc(void* inThread);     // the Rust thread's body (AsyncOp.cpp: the crash dump's SEH filter)
    static void DeleteThread(void* inThread);
};
// WON SDK static-only diagnostics API (ee_won debug.rs: the stack file and the crash dump; the trace is Rust's of
// the calling thread, the filter's: theInfo is not read)
class WONDebug {
public:
    static int DumpStackToFile(void* = NULL) { return ee_won_debug_dump_stack(); }
};
class Thread : public ThreadBase {
public:
    void SetAutoDelete(bool inAutoDelete) { ee_won_thread_set_auto_delete(this->mThread,inAutoDelete); }
};
class DNSTestOp : public AsyncOp {
    friend class DNSTestThread;
public:
    DNSTestOp(const std::string& inHost) { this->mHostString = inHost; } //2d95a0 (header-only since net round 27)
    // Accessor as in the SDK's DNSTestOp.h; the LAN family 3e28d0 reads
    // receiver+54 directly (no out-of-line helper).
    const IPAddr& GetHostAddr() { return this->mHostAddr; }
protected:
    virtual ~DNSTestOp() {}                 //2d9740
    virtual void RunHook();                 //2d97f0: a self-deleting thread resolves the host
    std::string mHostString;                //44..54
    IPAddr mHostAddr;                       //54..7c
};
typedef SmartPtr<DNSTestOp> DNSTestOpPtr;
class DNSTestThread : public Thread {
public:
    DNSTestThread(DNSTestOp* inOp) : mOp(inOp) {} //inline constructor 2d9826..985d
    // the op finished with success or the host-lookup failure status
    virtual void ThreadFunc() //slot10 2d9920
    {
        this->mOp->mHostAddr.SetWithDefaultPort(this->mOp->mHostString, 0);
        this->mOp->Finish(this->mOp->mHostAddr.IsValid() ? WS_Success : WS_InvalidAddress);
    }
    DNSTestOpPtr mOp; //1c,owned; full20. Implicit dtor2d98b0 no ownvptr store.
};
inline void DNSTestOp::RunHook()
{
    DNSTestThread* aThread = new DNSTestThread(this);
    aThread->SetAutoDelete(true);
    aThread->Start();
}


// WONAPI's HTTP document request (sizeof 0xdc; implicit dtor 0x4cff46; ctor out of line in the base game,
// base exe 0x6cc4d0; provisional member names)
// All enums are 32-bit int under VC6; no narrowed bool getter argument.
enum HTTPDocType {
    HTTPDocType_MOTD=0,
    HTTPDocType_TOU=1,
    HTTPDocType_ADV=2,
    HTTPDocType_Count=3
};
enum HTTPDocOwner {
    HTTPDocOwner_Sys=0,
    HTTPDocOwner_Game=1,
    HTTPDocOwner_Count=2
};

// GetHTTPDocumentOp (and GetMOTDOp, its MOTD) is a Rust op on the base (rust/crates/ee_won_net/src/httpdoc.rs):
// RustServerOp::NewHttpDocument, its members by EE_WON_HTTPDOC_* number and owner, GetHttpDocument

// WONAPI's CD key (dead in AoC; from the base game's Login: ctor base exe 0x68ab90, an 0x1c-byte local;
// implicit dtor 0x4cff2a: the SmartPtr at +0x14, the string at +4; provisional names)
// The full CDKey declaration comes earlier (EEWonNet), before AuthContext's mapped value.
// Storage, methods and inline virtual bodies live there; no duplicate class definition.


// AoC GetCertOp (0x11fc bytes in the original; RTTI GetCertOp > ServerRequestOp > ...): a Rust op on the base
// (ee_won certhost.rs over getcert.rs: its members, every step and the session key; its certificates, key blocks,
// private key and peer data are Rust handles, hostauth.rs). The class is its constructor (the auth context and its
// server list, ServerOp.cpp) and the two accessors the game uses.
class AuthPeerData;
class GetCertOp : public RustServerOp {
public:
    explicit GetCertOp(AuthContext*); //2e3c30
    ConstSmartPtr<AuthPeerData> GetPeerData() const; // a shim over the Rust peer data; NULL before a login
    void SetCreateAccount(bool value) { this->SetNum(EE_WON_GETCERT_CREATE_ACCOUNT, value); }
};


// DBProxyOp is a Rust op base (rust/crates/ee_won_net/src/reqop.rs DbProxy)
// The patch server's records (VersionData 0x48, PatchData 0xa4; constructors 2d83a0/2d86f0) are Rust's: their wire
// forms are ee_won patch.rs, the ops that read them verop.rs; no game code made the C++ classes, deleted in WON
// Internet round 21 (their statements are tests/won/won_patch_msg.h, over the drivers' stand-ins)
// CheckValidVersionOp is a Rust op (rust/crates/ee_won_net/src/reqops.rs)
// DetectFirewallOp (2cf600): a Rust op on the base (ee_won fwhost.rs over fwop.rs: its members and every step);
// the class is its constructor (ServerOp.cpp) and the setters and the public address the lobby uses.
class DetectFirewallOp : public RustServerOp {
public:
    explicit DetectFirewallOp(ServerContext* inContext); //2cf600
    void SetListenPort(unsigned short inPort) { this->SetNum(EE_WON_FWOP_LISTEN_PORT, inPort); }
    void SetDoListen(bool inDoListen) { this->SetNum(EE_WON_FWOP_DO_LISTEN, inDoListen); }
    void SetWaitForConnect(bool inWait) { this->SetNum(EE_WON_FWOP_WAIT_FOR_CONNECT, inWait); }
    IPAddr GetPublicAddr() const; // a copy (the original returned a reference to the member)
};

// RequestPasswordEmailOp and RequestAccountInfoEmailOp are Rust ops (rust/crates/ee_won_net/src/adminop.rs)

// GetUserIdFromNameOp is a Rust op (rust/crates/ee_won_net/src/reqops.rs)


// GetAccountOp and UpdateAccountOp are Rust ops (rust/crates/ee_won_net/src/reqdir.rs), CreateAccountOp and
// RoutingGetGroupListReqOp too (reqlast.rs: RustServerOp::NewCreateAccount, NewGroupListReq), and no C++ op derives
// from ServerRequestOp any more (its exchange is reqhost.rs's)

// The +20 EEWonNet member is WONAPICore (WONAPICore.h); +24 is a separate window interface.
class WONLobbyWindow {
public:
    virtual void Virtual04();
    virtual void Virtual08();
    virtual ~WONLobbyWindow();              // +0x14
    virtual void Virtual18();
    void Close(bool inFlag);                // base exe 0x6a2220

    bool mClosed;                           // +0x04 (the base dtor waits for it after Close)
};

// the connection to a routing server (sizeof 0x258; ctor 0x2bfb00)
// Names follow the WON SDK headers. All bases at 0; sizes as in the original.
// BlockingSocket (0xa0) and the AuthContext/PeerAuthOp pointees must be declared before use.
// Socket virtual04/08/0c are void(), factory10 returns AsyncSocket* (LAN owner).

// SocketThread (0x38) / SocketThreadSimple (0x44).
// Needs Thread (0x1c) / Event (8) from above.
// SocketThread.cpp's original addresses: SocketThread 2b60d0 / 2b6120 / ThreadFunc 2b6140 / DoOp 2b6160,
// SocketThreadSimple 2b61a0 / PurgeOps 2b6380 / AddSocketOp 2b64f0 / RemoveSocketOp 2b6660 / Pump 2b6760 / ThreadFunc
// 2b6890. The unit is deleted (net round 25): the ops' calls are answered in Rust (ee_won cppcore.rs)
class SocketThread : public Thread {
public:
    SocketThread() { this->mSingleThreaded=false; } //2b60d0: on its own thread
    virtual ~SocketThread() {} //2b6120, explicit own vtable 44cb10
    virtual void ThreadFunc() { while(!this->IsStopped()) this->Pump(1000); } //base slot10,2b6140
    virtual void AddSocketOp(SocketOp*) {} //new slot1c, 2fa500 (empty)
    virtual void RemoveSocketOp(SocketOp*) {} //20, version-empty2fa500
    virtual void Pump(unsigned long) {} //24, version-empty2fa500
    virtual void PurgeOps() {} //28, version-empty4142a0
    virtual bool NeedThread() {return true;} //2c, true2f30d0
    void SetSingleThreaded(bool value) { this->mSingleThreaded=value; } //inline bool 34 (WON SDK)
    // continues a pending op and finishes it unless it still waits; whether it is done (2b6160, cdecl)
    bool mSingleThreaded; //34; total38 (the original's lock at 1c is SocketThreadSimple's Rust object's)
};
// SocketThreadSimple: a Rust object since round 13 (rust/crates/ee_won_net/src/sockthread.rs EeWonSocketThread: the ops
// held, a reference each, in address order as the original's std::set of SmartPtrs; the lock; DoOp, the pump, the
// purge, the adds and removes) behind this shim; the ops' steps come back through SocketThread.cpp's hook table. The
// thread is the Thread shim's; ThreadFunc pumps the object, then waits on the thread's signal
class SocketThreadSimple : public SocketThread {
public:
    SocketThreadSimple() : mOps(ee_won_socket_thread_new(WonCore())) {}
    virtual ~SocketThreadSimple() { ee_won_socket_thread_free(this->mOps); } //2b61a0: every op still held killed
    virtual void ThreadFunc() { while(!this->IsStopped()) this->Pump(20); } //2b6890
    virtual void AddSocketOp(SocketOp* inOp) { ee_won_socket_thread_add(this->mOps,static_cast<AsyncOp*>(inOp)); }
    virtual void RemoveSocketOp(SocketOp* inOp) { ee_won_socket_thread_remove(this->mOps,static_cast<AsyncOp*>(inOp)); }
    // every op continued once, the finished dropped, then a wait of up to 20 ms for a signal (2b6760)
    virtual void Pump(unsigned long inWaitTime)
    { ee_won_socket_thread_pump_wait(this->mOps,this->mThread,static_cast<uint32_t>(inWaitTime)); }
    virtual void PurgeOps() { ee_won_socket_thread_purge(this->mOps); } //2b6380: killed and forgotten
private:
    EeWonSocketThread* mOps; //owned
};

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
    // the connect's result; on success the messages and the keep-alive start (2ab1f0, virtual in the SDK; nothing
    // overrides it)
    void ConnectCallback(PeerAuthOp* inOp)
    { ee_won_servconn_connect_callback(this->mConn,reinterpret_cast<RefCount*>(inOp)); }
    virtual void MsgCallback(const ByteBuffer*) {} //MsgCallback2fa500,ret4
    void SetAuth(AuthContext* inContext, AuthType inType)
    { ee_won_servconn_set_auth(this->mConn,reinterpret_cast<RefCount*>(inContext),inType); }
    // the socket closed and an authenticated connect started; the peer authentication op (2abc40, borrowed)
    PeerAuthOp* ConnectAsync(const IPAddr& address, unsigned long timeout)
    {
        return reinterpret_cast<PeerAuthOp*>(static_cast<RefCount*>(
            ee_won_servconn_connect_async(this->mConn,static_cast<const void*>(&address),(long)timeout)));
    }
    // the socket closed, a blocking connect, then the connect callback's report; its status
    WONStatus ConnectBlock(const IPAddr& theAddr,DWORD theTimeout=0xffffffff)
    { return ee_won_servconn_connect_block(this->mConn,static_cast<const void*>(&theAddr),theTimeout); }
    void SetKeepAlivePeriod(DWORD theSeconds) { ee_won_servconn_set_keep_alive_period(this->mConn,theSeconds); }
    // the completion called when a connect finishes (2abdc0)
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
    { ee_won_servconn_set_connect_completion(this->mConn,static_cast<RefCount*>(inCompletion)); }
    EeWonServerConn* GetConnObject() const { return this->mConn; }
private:
    ServerConnection(const ServerConnection&);
    ServerConnection& operator=(const ServerConnection&);
    EeWonServerConn* mConn;
};

// connects a server connection to an address, finishing with the connect's status (2abfc0; its run and completion
// are Rust's: ee_won cppcore.rs ee_won_servconn_connect_op_run)
class ServerConnectionConnectOp : public AsyncOp {
public:
    ServerConnectionConnectOp(ServerConnection* inConnection, const IPAddr& inAddr) : mConnection(inConnection), mAddr(inAddr) {}
    virtual void RunHook() //2ac2f0
    {
        ee_won_servconn_connect_op_run(static_cast<RefCount*>(this->mConnection.get()), static_cast<RefCount*>(this),
                                       &this->mAddr, (uint32_t)this->GetTimeout());
    }
    virtual void CleanupHook() { this->mConnection->SetConnectCompletion(NULL); } //2ac380
    SmartPtr<ServerConnection> mConnection; //44
    IPAddr mAddr; //48
};

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
    const EeWonRouteReq* GetReq() const { return this->mReq; }
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
    RoutingConnection() : mAllDerivedOpsAreUnsolicited(true)
    {
        this->SetLengthFieldSize(2);
        this->mRoute = ee_won_routeconn_new_core(static_cast<RefCount*>(this), WonCore(), static_cast<void*>(this->GetDataLock()));
    }
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
    // 2bc6c0: a new derived-server op when every derived message is unsolicited, else NULL
    virtual SmartPtr<RoutingOp> GetUnsolicitedDerivedOp(unsigned char)
    { return this->mAllDerivedOpsAreUnsolicited ? new RoutingOp(this, 60) : NULL; } // RoutingOp_DerivedServerOp
    virtual void RoutingOpCompleteHook(RoutingOp*) {} //inline RoutingOpCompleteHook (WON SDK), shared 30a800 ret4
    EeWonRouteConn* GetRouteObject() const { return this->mRoute; }
    bool mAllDerivedOpsAreUnsolicited; //c4 (GetUnsolicitedDerivedOp reads it)
protected:
    virtual ~RoutingConnection() { ee_won_routeconn_free(this->mRoute); } //2ba4b0
private:
    RoutingConnection(const RoutingConnection&);
    RoutingConnection& operator=(const RoutingConnection&);
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
    // Enum declarations as in the SDK; widths and map operations agree with the original.
    enum MatchResult { MatchResult_NotFound, MatchResult_Ambiguous, MatchResult_Partial, MatchResult_Exact };
    enum InputError { InputError_None, InputError_InvalidCommand, InputError_ClientNotFound,
        InputError_ClientAmbiguous, InputError_NoChat, InputError_NotLoggedIn, InputError_BadTime };
    enum InputCommand { InputCommand_None, InputCommand_Broadcast, InputCommand_Whisper,
        InputCommand_Emote, InputCommand_Ignore, InputCommand_Away, InputCommand_BecomeModerator,
        InputCommand_ServerMute, InputCommand_ServerBan, InputCommand_Mute, InputCommand_Ban,
        InputCommand_Unmute, InputCommand_ServerUnmute, InputCommand_Invite, InputCommand_Uninvite };
    // 2bfb00 (the 0x258 allocation is this class): the ignore file under the WON directory, the default chat commands
    SmartRoutingConnection() : mModel(ee_won_smart_new_default(sizeof(wchar_t))) {}
    // 2c4120: this client's id and record forgotten, then the routing connection closed
    virtual void CloseHook()
    {
        { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_close(this->mModel); }
        RoutingConnection::CloseHook();
    }
    // 2c0be0: the maps updated from a finished op by its type; a chat, invitation or join attempt from an ignored
    // client loses its completion
    virtual void RoutingOpCompleteHook(RoutingOp* inOp)
    {
        AutoCrit aCrit(this->GetDataLock());
        if (ee_won_smart_routeop(this->mModel, inOp->mRust, inOp->GetStatus())) inOp->SetCompletion(NULL);
    }
    // (each call below holds the socket's lock around the model's call, as the original's AutoCrit: ee_won_smart_locked)
    void StartClientItr() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 0, 0, 0, 0); }
    void StartGroupItr() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 1, 0, 0, 0); }
    bool StartMemberItr(unsigned short inId) { return ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Start, 2, inId, 0, 0) != 0; }
    // 2c37d0: the client GetClient(id) finds ignored or not, the ignore list saved
    void IgnoreClient(unsigned short inClientId, bool inOn) { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::Ignore, inClientId, inOn, 0, 0); }
    // 2c3970: a chat command word (case-insensitive)
    void AddCommandString(const std::wstring& inText, InputCommand inCommand)
    { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::AddCommand, (intptr_t)inText.data(), (intptr_t)inText.length(), inCommand, 0); }
    void ClearCommandStrings() { ee_won_smart_locked(this->GetDataLock(), this->mModel, EeWonSmartLocked::ClearCommands, 0, 0, 0, 0); }
    void SetAllowPartialNameMatch(bool inAllow) { this->SetSmart(EeWonSmartNumber::PartialNames, inAllow); } //2c40a0
    void SetServerWideCommands(bool inOn) { this->SetSmart(EeWonSmartNumber::ServerWide, inOn); } //2c40f0
    void SetDoColonEmote(bool inOn) { this->SetSmart(EeWonSmartNumber::ColonEmote, inOn); }
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
    SmartRoutingConnection(const SmartRoutingConnection&);
    SmartRoutingConnection& operator=(const SmartRoutingConnection&);
};

// No class here duplicates the old flattened RoutingConnection (0x258).
// No zero-valued gap fields or placement/partial object lifetime is introduced.

typedef SmartPtr<RoutingConnection> RoutingConnectionPtr;
typedef SmartPtr<SmartRoutingConnection> SmartRoutingConnectionPtr;


}

// CDKey and AuthContext (their forwarders need the classes above)
#include "ee/wonapi/WONAuthContext.h"

#endif
