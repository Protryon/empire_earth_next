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

namespace WONAPI {

typedef long WONStatus;
enum AsyncRunMode { OP_MODE_ASYNC=1, OP_MODE_BLOCK=2 }; //32-bit enum (WON SDK)
void StringToUpperCaseInPlace(std::string&); //2a69a0
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
    // the hook run and the timeout armed, or a time-out at once for 0 (29f4c0)
    void StartRunAsync() { ee_won_asyncop_start_run_async(this->mOp); }
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
    void ClosePrv() { ee_won_asyncsocket_close_handle(this->mSock); } // a hard close if it would block
    // Winsock's numbering; would-block and in-progress are WS_TimedOut, any other recorded
    WONStatus GetLastSocketError() { return ee_won_asyncsocket_last_error(this->mSock); }
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
    void SetThreadCloseCallback(CloseCallback inCallback)
    { ee_won_asyncsocket_set_close_callback(this->mSock, reinterpret_cast<void*>(inCallback)); }
    // the last socket error recorded (the original's +0x54)
    WONStatus GetSocketStatus() const { return ee_won_asyncsocket_status(this->mSock); }
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



// a WON operation on a socket (RTTI SocketOp > AsyncOp, vtable 0x84c424; its inline members are emitted by
// EELobbyFileTransfer.cpp 0x29151c..0x291555, 0x2914c2)
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
    virtual ~SocketOp() { ee_won_socketop_free(this->mSockOp); } // the original's was implicit, 29153d
    // +0x04 0x2b6070: started (finished InvalidSocket first without a socket); a pending start goes to the socket
    // thread (async) or is continued and finished (blocking), any other result finishes it
    virtual void RunHook() { ee_won_socketop_run_hook(this->mSockOp); }
    virtual void CleanupHook();             // +0x08 0x2b60c0: out of the socket thread (AsyncOp.cpp)
    //: +0x0c starts the operation, +0x10 continues it (SendFileOp 0x2915d2 / 0x291613 return a WONStatus). Since net
    // round 23 the socket op classes' own Start and Continue (ConnectOp, AcceptOp, the send and receive ops,
    // RecvCRMsgOp: one-line forwarders each) are these: the op's Rust object runs its class's step (sockops.rs); a
    // class without one of its own starts by the virtual Continue, as the original's Start (0x29151c) did
    virtual WONStatus Start() { return ee_won_socketop_start(this->mSockOp,this->GetTiming()); }
    virtual WONStatus Continue() { return ee_won_socketop_continue(this->mSockOp,this->GetTiming()); }
    // +0x14 (vtable 0x84c424: 0x1ad67e `xor eax, eax`, ICF): a fresh copy of the op, none by default (AcceptOp's
    // 0x291ab7 news one on the same socket; provisional name)
    virtual AsyncOp* Duplicate() { return NULL; }

    bool IsInSocketThread() const { return ee_won_socketop_in_socket_thread(this->mSockOp); }
    void SetInSocketThread(bool inFlag) { ee_won_socketop_set_in_socket_thread(this->mSockOp,inFlag); }
    bool GetWaitForEvent(int i) const { return ee_won_socketop_wait_for_event(this->mSockOp,i); }
    void SetWaitForEvent(int i,bool inFlag) { ee_won_socketop_set_wait_for_event(this->mSockOp,i,inFlag); }
    long GetEventIndex(int i) const { return ee_won_socketop_event_index(this->mSockOp,i); }
    void SetEventIndex(int i,long inIndex) { ee_won_socketop_set_event_index(this->mSockOp,i,inIndex); }
    EeWonSocketOp* GetSocketOpObject() const { return this->mSockOp; }
    static void HookAddToSocketThread(void* inOp); // WONAPICore's friend (AsyncOp.cpp: the core table's)
private:
    // A Rust object since round 14 (rust/crates/ee_won_net/src/sockop.rs EeWonSocketOp): the socket thread flag (+0x44),
    // the event indices (+0x48), the socket (+0x54, a reference) and the wait flags (+0x58), and RunHook
    EeWonSocketOp* mSockOp;
};

// closes its socket
class CloseOp : public SocketOp {
public:
    CloseOp(AsyncSocket* inSocket) : SocketOp(inSocket) {}
    virtual WONStatus Continue() { this->GetSocket()->Close(); return 0; }  // +0x10 0x291993
};

// waits for a connection on its (listening) socket
// (a Rust object since round 15, rust/crates/ee_won_net/src/sockops.rs: the accepted socket, +0x5c, a reference held)
class AcceptOp : public SocketOp {
public:
    AcceptOp(AsyncSocket* inSocket) : SocketOp(inSocket)
    { this->SetWaitForEvent(0,true); ee_won_socketop_init_accept(this->GetSocketOpObject()); }
    AsyncSocket* GetAcceptedSocket()
    { return static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_socketop_accepted(this->GetSocketOpObject()))); }
    virtual AsyncOp* Duplicate() { return new AcceptOp(this->GetSocket()); }  // +0x14 0x291ab7
};

// connects its socket to an address (RTTI ConnectOp > SocketOp, vtable 0x84c480; 0x84 bytes: EELobbyFileTransfer
// 0x291b79 news it and inlines the ctor: SocketOp(NULL), the IPAddr copy ctor 0x291d59 at +0x5c, the vptr, then
// +0x59 and +0x5a = true; argument order inferred). A Rust object since round 15 (sockops.rs: the address, +0x5c)
class ConnectOp : public SocketOp {
public:
    ConnectOp(const IPAddr& inAddr, AsyncSocket* inSocket = NULL) : SocketOp(inSocket)
    {
        this->SetWaitForEvent(1,true);
        this->SetWaitForEvent(2,true);
        ee_won_socketop_init_connect(this->GetSocketOpObject(),inAddr.GetValue());
    }
    // a copy (the original returned a reference to the member)
    IPAddr GetAddr() const { return IPAddr::FromValue(ee_won_socketop_address(this->GetSocketOpObject())); }
};

// receives bytes from its socket into a buffer or a file. A Rust object since round 15 (sockops.rs: the write buffer
// +0x5c, the received bytes +0x74, the length, count, skip flag, file, path, file flags and modify time +0x78..+0x9c,
// the progress completion, interval and count +0xa0..+0xa8; RecvMsgOp's message +0xac and stage +0xb0; and StartRecv
// 0x2b5000, ContinueRecv 0x2b50c0, CloseFile 0x2b5410, FinishRecv 0x2b54a0, StartRecvBody 0x2b5b50, FinishRecvMsg
// 0x2b5d20)
class RecvBytesOp : public SocketOp {
public:
    // the number of bytes (0: until the peer closes) into memory (2b4de0)
    RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket = NULL) : SocketOp(inSocket)
    {
        this->SetWaitForEvent(0,true);
        ee_won_socketop_init_recv(this->GetSocketOpObject(), static_cast<uint32_t>(inLength), false);
    }
    // out of the socket thread, the target file closed (2b5580)
    virtual void CleanupHook()
    {
        SocketOp::CleanupHook();
        ee_won_socketop_close_file(this->GetSocketOpObject(), this->GetStatus());
    }
    virtual AsyncOp* Duplicate() //2b4f00: the same length and socket
    { return new RecvBytesOp(ee_won_socketop_length(this->GetSocketOpObject()),this->GetSocket()); }
    // the completion called after every interval bytes (2b4f90)
    void SetProgressCompletion(CompletionBase<AsyncOpPtr>* completion, long interval)
    {
        ee_won_socketop_set_progress(this->GetSocketOpObject(), static_cast<void*>(completion), // its RefCount
                                     static_cast<int32_t>(interval));
    }
    ByteBufferPtr GetBytes() const //2b1df0 (folded)
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_received(this->GetSocketOpObject()))); }
    void SetFilePath(const std::string& theFilePath)
    { ee_won_socketop_set_path(this->GetSocketOpObject(),reinterpret_cast<const uint8_t*>(theFilePath.data()),theFilePath.size()); }
    void SetAppendFile(bool theVal) { ee_won_socketop_set_flag(this->GetSocketOpObject(),0,theVal); }
    void SetBinaryFile(bool isBinary) { ee_won_socketop_set_flag(this->GetSocketOpObject(),1,isBinary); }
    void SetFileModifyTime(time_t theTime) { ee_won_socketop_set_modify_time(this->GetSocketOpObject(),static_cast<int64_t>(theTime)); }
    void SetSkipBytes(bool skip) { ee_won_socketop_set_flag(this->GetSocketOpObject(),2,skip); }
    std::string GetFilePath() const // a copy (the original returned a reference to the member)
    {
        std::string aPath;
        ee_won_socketop_path(this->GetSocketOpObject(), [](void* inContext, const uint8_t* inBytes, size_t inLength) {
            static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
        }, &aPath);
        return aPath;
    }
    unsigned long GetBytesRead() const { return ee_won_socketop_bytes_read(this->GetSocketOpObject()); }
protected:
    struct MessageTag {};
    RecvBytesOp(AsyncSocket* inSocket, MessageTag) : SocketOp(inSocket) // RecvMsgOp's: length-prefixed messages
    {
        this->SetWaitForEvent(0,true);
        ee_won_socketop_init_recv(this->GetSocketOpObject(), 0, true);
    }
};
class RecvMsgOp : public RecvBytesOp {
public:
    RecvMsgOp(AsyncSocket* inSocket = NULL) : RecvBytesOp(inSocket, MessageTag()) {}
    virtual AsyncOp* Duplicate() { return new RecvMsgOp(this->GetSocket()); } //2ab3d0
    ByteBufferPtr GetMsg() const //2ab770
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_message(this->GetSocketOpObject()))); }
};

// vtable 44ca78, six slots. The line so far (the original's WriteBuffer at 0x5c) and the message (+0x74) are the
// op's Rust object's (sockop.rs, sockops.rs)
class RecvCRMsgOp : public SocketOp {
public:
    RecvCRMsgOp(AsyncSocket* inSocket=NULL) : SocketOp(inSocket)
    { this->SetWaitForEvent(0,true); ee_won_socketop_init_line(this->GetSocketOpObject()); }
    virtual AsyncOp* Duplicate() {return new RecvCRMsgOp(this->GetSocket());} //2b2180, 209 bytes
    ByteBufferPtr GetMsg() const //accessor 2b1df0 (ICF-folded)
    { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_message(this->GetSocketOpObject()))); }
};

// what a RecvBytesOp's progress completion (+0xa0) gets: 0x2b520d news one (0x48 bytes; RTTI RecvChunkOp : AsyncOp,
// vtable 0x84cae4) with +0x44 = the receiving op
class RecvChunkOp : public AsyncOp {
public:
    RecvChunkOp(RecvBytesOp* inOp) : mRecvOp(inOp) {} //0x48 bytes: AsyncOp (0x44) plus the owning pointer at 0x44
    RecvBytesOp* GetRecvOp() const { return this->mRecvOp.get(); }
    virtual void RunHook() { this->Finish(0); }  //2b5370, 8-byte override of slot 04

private:
    SmartPtr<RecvBytesOp> mRecvOp;          // +0x44
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
    void RunRepeatOp() { ee_won_queuesocket_run_repeat_op(this->mQueue); }
    void RunQueueOp() { ee_won_queuesocket_run_queue_op(this->mQueue); }
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

// further socket operations and managers

// TD4d4198, VT44c9bc, total6c. A Rust object since round 15 (sockops.rs: the bytes +0x68, a reference held, and the
// read position of the original's ReadBuffer +0x5c)
class SendBytesOp : public SocketOp {
public:
    SendBytesOp(const ByteBuffer* inData, AsyncSocket* inSocket = NULL) : SocketOp(inSocket) // NULL sends nothing
    {
        this->SetWaitForEvent(1,true);
        ee_won_socketop_init_send(this->GetSocketOpObject(), const_cast<ByteBuffer*>(inData), NULL, false);
    }
};

// TD4d41c0, VT44c9a0, total70 (the message +0x6c is the Rust object's): through the socket's send transforms
class SendMsgOp : public SendBytesOp {
public:
    SendMsgOp(const ByteBuffer* inMessage, AsyncSocket* inSocket = NULL) : SendBytesOp(NULL, inSocket)
    { ee_won_socketop_init_send(this->GetSocketOpObject(), NULL, const_cast<ByteBuffer*>(inMessage), true); }
};

// TD4d4040, VT44c948, total88 (the bytes +0x5c and the address +0x60 are the Rust object's): to the address, or on
// the connection when the socket is connected
class SendBytesToOp : public SocketOp {
public:
    SendBytesToOp(const ByteBuffer* inData, const IPAddr& inAddress, AsyncSocket* inSocket = NULL) : SocketOp(inSocket)
    {
        this->SetWaitForEvent(1,true);
        ee_won_socketop_init_send_to(this->GetSocketOpObject(), const_cast<ByteBuffer*>(inData), inAddress.GetValue());
    }
};

// TD4d4440, VT44caf4, total90. Original out-of-line constructor2b55a0. A Rust object since round 15 (sockops.rs:
// the buffer +0x5c, the datagram +0x60, the size +0x64, the sender +0x68)
class RecvBytesFromOp : public SocketOp {
public:
    RecvBytesFromOp(unsigned short inMaxLength, AsyncSocket* inSocket = NULL) : SocketOp(inSocket) // one datagram
    {
        this->SetWaitForEvent(0,true);
        ee_won_socketop_init_recv_from(this->GetSocketOpObject(), inMaxLength);
    }
    virtual ~RecvBytesFromOp() {} //2b56f0: the buffer goes with the Rust object
    // the buffer allocated if needed, the last datagram cleared, then the run (2b57d0)
    virtual void RunHook()
    {
        ee_won_socketop_recv_from_prepare(this->GetSocketOpObject());
        SocketOp::RunHook();
    }
    virtual AsyncOp* Duplicate() //2b5660: the same size and socket
    {
        return new RecvBytesFromOp(static_cast<unsigned short>(ee_won_socketop_length(this->GetSocketOpObject())),
                                   this->GetSocket());
    }
    SmartPtr<ByteBuffer> GetBytes() const //2aa920
    {
        return const_cast<ByteBuffer*>(static_cast<const ByteBuffer*>(
            static_cast<RefCount*>(ee_won_socketop_received(this->GetSocketOpObject()))));
    }
    IPAddr GetAddr() const { return IPAddr::FromValue(ee_won_socketop_address(this->GetSocketOpObject())); } // a copy
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
    void SetMsg(const ByteBuffer* theMsg)
    { ee_won_udprecvop_set_msg(this->mUdp, const_cast<RefCount*>(static_cast<const RefCount*>(theMsg))); }
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
    virtual void Run(AsyncOp* inOp)
    {
        if (this->mObject->mKilled) return;
        if (this->mCheckTracker && !this->mObject->RemoveTrackedOp(inOp)) return;
        (this->mObject.mPtr->*mCallback)(inOp);
    }
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
    static void StaticOpCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inConnection) //2d2df0
    { static_cast<HTTPConnection*>(inConnection.get())->OpCallback(inOp.get()); }
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

// a client of the routing server (+8 its name)
struct RoutingClientInfo : public RefCount {
    RoutingClientInfo() : mClientId(0),mFlags(0),mUserId(0),mAddress(0),mTrustLevel(0) {} //inline default (WON SDK)
    std::wstring mName;                     // +0x08
    unsigned short mClientId;               // +0x18
    unsigned long mFlags;                   // +0x1c (Client init 0x3a5350)
    unsigned long mUserId;                  // +0x20
    unsigned long mAddress;                 // +0x24, CDKeyId (historical spelling retained)
    unsigned short mTrustLevel; //28, the base factory allocates 0x2c and stores the WORD at 28
};
typedef SmartPtr<RoutingClientInfo> RoutingClientInfoPtr;

typedef std::map<unsigned short, RoutingClientInfoPtr> RoutingClientMap;

// a member of a group: the client it is
struct RoutingMemberInfo : public RefCount {
    RoutingMemberInfo() : mClientId(0),mFlags(0) {} //inline default (WON SDK); mClientInfo defaults to NULL
    unsigned short mClientId;               // +0x08
    unsigned char mFlags; //0a, BYTE; natural padding 0b; 2c5da5/2c64e7 write a BYTE
    RoutingClientInfoPtr mClientInfo;       // +0x0c
};
typedef SmartPtr<RoutingMemberInfo> RoutingMemberInfoPtr;
typedef std::map<unsigned short, RoutingMemberInfoPtr> RoutingMemberMap;

// a group (room or game) of the routing server
struct RoutingGroupInfo : public RefCount {
    RoutingGroupInfo() : mId(0),mParentId(0xffff),mCaptainId(0),mMaxClients(0),mFlags(0),mAsyncFlags(0),mObserverCount(0),mMaxPlayers(0) {} //WON SDK; historical protected names retained
    std::wstring mName;                     // +0x08
    unsigned short mId;                     // +0x18
    unsigned short mParentId; //1a, SDK mParentId; 2ba361 stores ffff
    unsigned short mCaptainId;             // +0x1c
    unsigned short mMaxClients;            // +0x1e
    unsigned long mFlags;                   // +0x20 (0x800: the room is public; 0x8 in byte +0x21: a chat room)
    unsigned long mAsyncFlags; //24, the group reply stores a DWORD
    unsigned short mObserverCount; //28, stores a WORD
    unsigned short mMaxPlayers;             // +0x2a
    RoutingMemberMap mMembers;              // +0x2c
};
typedef SmartPtr<RoutingGroupInfo> RoutingGroupInfoPtr;
typedef std::map<unsigned short, RoutingGroupInfoPtr> RoutingGroupMap;

// a data object of the routing server (the game's room/game descriptions)
struct RoutingDataObject : public RefCount {
    RoutingDataObject() : mLinkId(0) {} //new 0x30, inline ctor 2c81c9..2c8209
    unsigned short mLinkId;                 // +0x08
    std::string mDataType;                  // +0x0c
    std::wstring mName;                     // +0x1c
    ByteBufferPtr mData;                    // +0x2c

    unsigned long GetDataLength() const { return this->mData->length(); }
    const char* GetData() const { return this->mData->data(); }
};
typedef SmartPtr<RoutingDataObject> RoutingDataObjectPtr;
typedef std::list<RoutingDataObjectPtr> RoutingDataObjectList;

struct RoutingBadUser {
    RoutingBadUser() : mWONId(0), mExpirationDiff(0), mModeratorWONId(0) {} // 2c5429..2c5475
    std::wstring mName; //0..f
    unsigned long mWONId; //10
    unsigned long mExpirationDiff; //14
    unsigned long mModeratorWONId; //18
    std::wstring mModeratorName; //1c..2b
    std::wstring mModeratorComment; //2c..3b
    // Implicit cleanup/copy; no out-of-line destructor.
};
typedef std::list<RoutingBadUser> RoutingBadUserList;

enum RoutingBadUserListType {
    RoutingBadUserListType_BannedUsers=1,
    RoutingBadUserListType_MutedUsers=2
};

// Every routing op, request or notification, is one Rust object since net round 18 (rust/crates/ee_won_net/src//
// routeop.rs EeWonRouteOp: the op's members by field number EE_WON_RTF_* and text EE_WON_RTT_*, its data type,
// data and recipients, the records its replies build, SendRequest's message and HandleReply's read, and
// RoutingCreateGroupOp::CheckLeaveGroup). This class is the AsyncOp the connection's list and the completions hold,
// one per op type (RoutingOpType, WONRoutingOpTypes.h); its readers read the members through the accessors and the
// records as copies. The original's 55 op classes (RunHook 2c7d20, HandleReply 2bcd30, the request builders and
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
    bool IsDerivedServerOp() const { return this->GetType() == 60; } // RoutingOp_DerivedServerOp
    RoutingConnection* GetConnection() { return this->mConnection.get(); }

    // the members (EE_WON_RTF_* numbers, EE_WON_RTT_* texts)
    long long Num(int inField) const { return ee_won_routeop_num(this->mRust, inField); }
    void SetNum(int inField, long long inValue) { ee_won_routeop_set_num(this->mRust, inField, inValue); }
    std::wstring Text(int inWhich) const { std::wstring s; ee_won_routeop_text(this->mRust, inWhich, WonWideSink, &s); return s; }
    void SetText(int inWhich, const std::wstring& inText)
    {
        std::string b;
        WideAppendUtf16LE(b, inText);
        ee_won_routeop_set_text(this->mRust, inWhich, reinterpret_cast<const uint16_t*>(b.data()), b.size() / 2);
    }
    std::string DataType() const { std::string s; ee_won_routeop_data_type(this->mRust, WonStringSink, &s); return s; }
    void SetDataType(const std::string& inType)
    { ee_won_routeop_set_data_type(this->mRust, reinterpret_cast<const uint8_t*>(inType.data()), inType.size()); }
    // NULL when the op has no buffer
    ByteBufferPtr GetData() const { ByteBufferPtr b; ee_won_routeop_data(this->mRust, WonByteBufferSink, &b); return b; }
    void SetData(const ByteBuffer* inData)
    {
        if (inData == NULL) ee_won_routeop_set_data(this->mRust, false, NULL, 0);
        else ee_won_routeop_set_data(this->mRust, true, reinterpret_cast<const uint8_t*>(inData->data()), inData->length());
    }
    std::list<unsigned short> Recipients() const
    {
        std::vector<uint16_t> anIds(ee_won_routeop_ids(this->mRust, NULL, 0) + 1);
        size_t n = ee_won_routeop_ids(this->mRust, &anIds[0], anIds.size() - 1);
        return std::list<unsigned short>(anIds.begin(), anIds.begin() + n);
    }
    void SetRecipients(const std::list<unsigned short>& inIds)
    {
        std::vector<uint16_t> anIds(inIds.begin(), inIds.end());
        ee_won_routeop_set_ids(this->mRust, anIds.empty() ? NULL : &anIds[0], anIds.size());
    }
    unsigned short GetGroupId() const { return (unsigned short)this->Num(EE_WON_RTF_GROUP_ID); }
    unsigned short GetClientId() const { return (unsigned short)this->Num(EE_WON_RTF_CLIENT_ID); }
    unsigned short GetLinkId() const { return (unsigned short)this->Num(EE_WON_RTF_LINK_ID); }

    // the records, copied (each map's records are new C++ records)
    void GetClients(RoutingClientMap& outMap) const;   // mClientMap
    void GetMembers(RoutingMemberMap& outMap) const;   // mMembers, the join op's mMemberMap
    void GetGroups(RoutingGroupMap& outMap) const;     // mGroupMap
    void GetDataObjects(RoutingDataObjectList& outList) const; // mDataObjects
    void GetBadUsers(RoutingBadUserList& outList) const; // mBadUserList
    RoutingClientInfoPtr GetClientInfo() const;          // the client joined server notice's mClientInfo (or NULL)
    RoutingGroupInfoPtr GetGroupInfo() const;            //2c6cb0: the group created notice's group
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
// net round 19 (rust/crates/ee_won_net/src/userstatus.rs EeWonUserStatus); the map is read and set through copies
class RoutingGetUserStatusOp : public UDPManagerSendRecvOp {
public:
    typedef std::map<unsigned long,unsigned char> FriendStatusMap;
    // 2c6630: a query to the server address, 3 retransmits, a 1 s message timeout
    RoutingGetUserStatusOp(const IPAddr& inAddress, UDPManager* inManager) : mStatus(ee_won_userstatus_new())
    {
        this->SetDestAddr(inAddress);
        this->SetUDPManager(inManager);
        this->SetNumRetransmits(3);
        this->SetMsgTimeout(1000);
    }
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
    FriendStatusMap GetFriendStatusMap() const // a copy
    {
        FriendStatusMap aMap;
        ee_won_userstatus_each(this->mStatus, StatusSink, &aMap);
        return aMap;
    }
    void SetFriendStatusMap(const FriendStatusMap& inMap)
    {
        ee_won_userstatus_clear(this->mStatus);
        for (FriendStatusMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
            ee_won_userstatus_put(this->mStatus, i->first, i->second);
    }
private:
    static void StatusSink(void* inContext, uint64_t inId, uint8_t inStatus)
    { (*static_cast<FriendStatusMap*>(inContext))[(unsigned long)inId] = inStatus; }
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
typedef std::map<std::wstring,DirEntityList> DirEntityMap;
typedef std::set<IPAddr> AddrSet;
// A shim over ee_won's server list (rust/crates/ee_won_net/src/servctx.rs: the addresses, their order, the one-time
// shuffle and the lock); the original's 0x3c-byte layout is gone
// a shim over ee_won's server list (servctx.rs: the addresses, their order, the one-time shuffle and the lock);
// header-only since net round 27 (ServerContext.cpp deleted): it converts IPAddr values and walks the entities
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
    bool AddAddressesFromDir(const DirEntityList& inDir, const wchar_t* inNameFilter=NULL) //2acab0/332
    {
        std::vector<IPAddr> anAddrs = DirAddresses(inDir, inNameFilter);
        bool added = false;
        for (size_t i = 0; i < anAddrs.size(); i++) if (this->AddAddress(anAddrs[i])) added = true;
        return added;
    }
    void RemoveAllAddresses() { this->Clear(); } //2acc00
    unsigned long GetNumAddresses() { return (unsigned long)ee_won_ctx_len(this->mList); }
    typedef std::list<IPAddr> AddrList;
    // shuffled the first time (the CRT's srand(time(NULL)) and rand, as random_shuffle drew)
    void CopyAddresses(AddrList& outCopy) { outCopy.clear(); ee_won_ctx_copy(this->mList, Rand, ToList, &outCopy); } //2ac6d0
    bool RemoveAddress(const IPAddr& inAddress) { EeWonAddr a = inAddress.ToRaw(); return ee_won_ctx_remove(this->mList, &a); }
    void CopyAddresses(AddrSet& outCopy) { outCopy.clear(); ee_won_ctx_copy_sorted(this->mList, ToSet, &outCopy); }
    bool AddAddressesFromDir(const DirEntityMap& inDir, const std::wstring& inPath, const wchar_t* inNameFilter=NULL)
    {
        DirEntityMap::const_iterator i = inDir.find(inPath);
        return i != inDir.end() && this->AddAddressesFromDir(i->second, inNameFilter);
    }
    // cleared and refilled under the list's one lock
    bool AtomicClearAndAddAddressesFromDir(const DirEntityList& inDir, const wchar_t* inNameFilter=NULL)
    {
        std::vector<IPAddr> anAddrs = DirAddresses(inDir, inNameFilter);
        std::vector<EeWonAddr> aRaw;
        for (size_t i = 0; i < anAddrs.size(); i++) aRaw.push_back(anAddrs[i].ToRaw());
        return ee_won_ctx_replace(this->mList, aRaw.empty() ? NULL : &aRaw[0], aRaw.size());
    }
    bool AtomicClearAndAddAddressesFromDir(const DirEntityMap& inDir, const std::wstring& inPath, const wchar_t* inNameFilter=NULL)
    {
        DirEntityMap::const_iterator i = inDir.find(inPath);
        if (i != inDir.end()) return this->AtomicClearAndAddAddressesFromDir(i->second, inNameFilter);
        ee_won_ctx_replace(this->mList, NULL, 0); // the original cleared the list before looking the path up
        return false;
    }
    // the original's public mShuffleAddresses (copies shuffle once while it is on)
    void SetShuffleAddresses(bool inShuffle) { ee_won_ctx_set_shuffle(this->mList, inShuffle); }
    bool GetShuffleAddresses() { return ee_won_ctx_shuffle(this->mList); }

private:
    ServerContext(const ServerContext&);
    ServerContext& operator=(const ServerContext&);
    static std::vector<IPAddr> DirAddresses(const DirEntityList& inDir, const wchar_t* inNameFilter)
    {
        std::wstring aFilter;
        if (inNameFilter != NULL) aFilter = inNameFilter;
        std::vector<IPAddr> anAddrs;
        for (DirEntityList::const_iterator i = inDir.begin(); i != inDir.end(); ++i) {
            DirEntity* e = i->get();
            if (e->mType == 'D' || (!aFilter.empty() && !(aFilter == e->mName))) continue;
            anAddrs.push_back(e->GetNetAddrAsIP());
        }
        return anAddrs;
    }
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
    DirEntityList GetEntities(int inWhich, long long inIndex = 0) const;
    long long GetNum(int inWhich, long long inIndex = 0) const { return ee_won_hostop_num(this->mRustOp, inWhich, inIndex); }
    std::string GetBytes(int inWhich, long long inIndex = 0) const;
    // the directory and account ops the game makes (ee_won reqdir.rs), with their members
    static RustServerOp* NewGetDir(ServerContext* inContext, const std::wstring& inPath, DWORD inFlags,
                                   const char* const* inTypes = NULL, int inTypeCount = 0);
    static RustServerOp* NewGetMultiDir(ServerContext* inContext, DWORD inFlags, const std::wstring* inPaths, int inCount);
    static RustServerOp* NewGetService(ServerContext* inContext, const std::wstring& inPath, const std::wstring& inName,
                                       const std::string& inNetAddr);
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
    time_t GetHttpLastModifiedTime();
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
    void CopyMaxTimes(const RustServerOp* inSource);
    BlockingSocket* GetSocket() const;
    void SetSocket(BlockingSocket* inSocket);
    AuthContext* GetAuthContext() const;
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


// The directory server's entity replies: the reading is ee_won's (dir.rs); a list of what it read as DirEntity
// objects (DirEntityReplyParser.cpp), for RustServerOp::GetEntities. GetEntityRequest, GetDirOp, GetMultiDirOp,
// GetAliasDirOp, FindEntityOp and GetServiceOp are Rust ops (rust/crates/ee_won_net/src/reqdir.rs).
class DirEntityReplyParser {
public:
    static void TakeEntity(void* inList, const EeWonDirEntity* inEntity); // appends to the DirEntityList
};

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
    WONStatus GetGetCertStatus() const { return (WONStatus)this->GetNum(EE_WON_PEEROP_GET_CERT_STATUS); }
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
    void Stop(bool waitForStop=true)
    {
        if(!ee_won_thread_mark_stopped(this->mThread))
            return;
        this->Signal();
        if(waitForStop)
            this->WaitForStop();
    }
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
    static void SetStackFileName(const std::string &theName)
    { ee_won_debug_set_stack_file(reinterpret_cast<const uint8_t*>(theName.data()), theName.size()); }
    static std::string GetStackFileName() { std::string s; ee_won_debug_stack_file(WonStringSink, &s); return s; }
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
    virtual void Virtual00();
    virtual void Virtual04();
    virtual void Virtual08();
    virtual void Virtual0C();
    virtual void Virtual10();
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
    static bool DoOp(SocketOp* inOp) { return ee_won_socket_thread_do_op(WonCore(),static_cast<AsyncOp*>(inOp)); }
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
    {
        uint32_t aWait=ee_won_socket_thread_pump(this->mOps,static_cast<uint32_t>(inWaitTime));
        if(aWait>0) this->WaitForSignal(aWait);
    }
    virtual void PurgeOps() { ee_won_socket_thread_purge(this->mOps); } //2b6380: killed and forgotten
private:
    EeWonSocketThread* mOps; //owned
};

// A Rust object since net round 16 (rust/crates/ee_won_net/src/servconn.rs EeWonServerConn: the auth context +0xa0 and
// type +0xa4, the peer authentication op +0xa8, the connect completion +0xac, the keep-alive timer +0xb0 and period
// +0xb4, and every step of ServerConnection.cpp); this class is the shim RoutingConnection and the lobby derive from
// Since net round 25 its calls back are Rust's (cppcore.rs NATIVE_SERVCONN: the ops, the completions the original's
// static callbacks were, 2ab160 / 2ab6a0 / 2ab7b0, the messages, the socket) but the peer authentication op's maker
// and the virtual MsgCallback (ServerConnection.cpp)
class ServerConnection : public BlockingSocket {
public:
    // restarts the keep-alive timer (none for a 0 period) (2aaf10)
    void StartKeepAlive() { ee_won_servconn_start_keep_alive(this->mConn); }
    // a no-op message, the timer restarted, when the current keep-alive timer fires (2ab840)
    void KeepAliveCallback(AsyncOp* inOp) { ee_won_servconn_keep_alive_callback(this->mConn,static_cast<RefCount*>(inOp)); }
    void SendNoOp() { ee_won_servconn_send_no_op(this->mConn); } // a no-op message queued (2ab910)
    ServerConnection(); //2aab10: unauthenticated, a 300-second keep-alive
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
    RoutingRequestValues& N(long long inNum) { return this->Add(inNum, std::string()); }
    RoutingRequestValues& W(const std::wstring& inText) { std::string b; WideAppendUtf16LE(b, inText); return this->Add(0, b); }
    RoutingRequestValues& S(const std::string& inBytes) { return this->Add(0, inBytes); }
    RoutingRequestValues& D(const ByteBuffer* inData)
    { return inData == NULL ? this->Add(0, std::string()) : this->Add(1, std::string(inData->data(), inData->length())); }
    RoutingRequestValues& L(const std::list<unsigned short>& inIds)
    {
        std::string b;
        for (std::list<unsigned short>::const_iterator i = inIds.begin(); i != inIds.end(); ++i)
            b.append(1, (char)(*i & 0xff)).append(1, (char)(*i >> 8));
        return this->Add(0, b);
    }
    void Run(RefCount* inConn, int inKind, CompletionBase<AsyncOpPtr>* inCompletion)
    {
        std::vector<EeWonAdminValue> v;
        for (std::list<std::pair<long long, std::string> >::iterator i = mValues.begin(); i != mValues.end(); ++i) {
            EeWonAdminValue x = {i->first, reinterpret_cast<const uint8_t*>(i->second.data()), i->second.size()};
            v.push_back(x);
        }
        ee_won_routeconn_request(inConn, inKind, v.empty() ? NULL : &v[0], v.size(), static_cast<RefCount*>(inCompletion));
    }
private:
    RoutingRequestValues& Add(long long inNum, const std::string& inBytes)
    { this->mValues.push_back(std::make_pair(inNum, inBytes)); return *this; }
    std::list<std::pair<long long, std::string> > mValues;
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
    void SubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(1).N(inLinkId).S(inDataType).N(inFlag).Run(this, EE_WON_RTQ_READ_DATA, c); } // 2bf1d0
    void UnsubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(inLinkId).S(inDataType).N(inFlag).Run(this, EE_WON_RTQ_UNSUBSCRIBE_DATA, c); } // 2bf2e0
    void Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inReconnectId,
                  unsigned long inAsyncFlags, unsigned long inRegisterFlags, unsigned long inFlags, RouteCompletion* c)
    {
        RoutingRequestValues().W(inName).W(inPassword).N(inReconnectId).N(inAsyncFlags).N(inRegisterFlags).N(inFlags)
            .Run(this, EE_WON_RTQ_REGISTER, c);
    } // 2bcc30
    void CreateGroup(unsigned short inGroupId, const std::wstring& inName, const std::wstring& inPassword,
                     unsigned short inMaxPlayers, unsigned long inGroupFlags, unsigned char inJoinFlags,
                     unsigned long inAsyncFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inGroupId).W(inName).W(inPassword).N(inMaxPlayers).N(inGroupFlags).N(inJoinFlags)
            .N(inAsyncFlags).Run(this, EE_WON_RTQ_CREATE_GROUP, c);
    } // 2bd430
    void CreateDataObject(unsigned char inFlags, unsigned short inLinkId, const std::string& inDataType,
                          const std::wstring& inName, const ByteBuffer* inData, RouteCompletion* c)
    { RoutingRequestValues().N(inFlags).N(inLinkId).S(inDataType).W(inName).D(inData).Run(this, EE_WON_RTQ_CREATE_DATA, c); } // 2becd0
    // 2bee30: the delete request
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName, RouteCompletion* c)
    { RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).Run(this, EE_WON_RTQ_DELETE_DATA, c); }
    void GetGroupList(unsigned char inFlags, RouteCompletion* c)
    { RoutingRequestValues().N(inFlags).Run(this, EE_WON_RTQ_GROUP_LIST, c); } // 2bcf90
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inRecipient, RouteCompletion* c)
    { this->SendChatTo(inText, (unsigned short)inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be540
    void SendChatTo(const std::wstring& inText, unsigned short inFlags, const std::list<unsigned short>& inClients,
                    RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inFlags).L(inClients).Run(this, EE_WON_RTQ_CHAT, c); } // 2be6a0
    void ModifyDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                          unsigned short inOffset, bool inInsert, const ByteBuffer* inData, RouteCompletion* c)
    {
        RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).N(inOffset).N(inInsert).D(inData)
            .Run(this, EE_WON_RTQ_MODIFY_DATA, c);
    } // 2bef50
    void JoinGroup(unsigned short inId, const std::wstring& inPassword, const std::wstring& inComment,
                   unsigned char inJoinFlags, unsigned char inMemberFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inId).W(inPassword).W(inComment).N(inJoinFlags).N(inMemberFlags)
            .Run(this, EE_WON_RTQ_JOIN_GROUP, c);
    } // 2bd580
    void CancelJoinGroup(unsigned short inId, bool inLeave, RouteCompletion* c)
    { RoutingRequestValues().N(inId).N(inLeave).Run(this, EE_WON_RTQ_CANCEL_JOIN, c); } // 2bd6b0
    void LeaveGroup(unsigned short inId, RouteCompletion* c) { RoutingRequestValues().N(inId).Run(this, EE_WON_RTQ_LEAVE, c); } // 2bd850
    void ReadDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(0).N(inLinkId).S(inDataType).N(inFlag).Run(this, EE_WON_RTQ_READ_DATA, c); } // 2bf0c0
    void GetClientList(RouteCompletion* c) { RoutingRequestValues().Run(this, EE_WON_RTQ_CLIENT_LIST, c); } // 2bcd40
    void GetMembersOfGroup(unsigned short inId, RouteCompletion* c)
    { RoutingRequestValues().N(inId).Run(this, EE_WON_RTQ_MEMBERS, c); } // 2bd1e0
    // 2be390: the client mask cut to 16 bits
    void SetClientFlags(unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask, unsigned long inAsyncFlags,
                        RouteCompletion* c)
    { RoutingRequestValues().N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags).Run(this, EE_WON_RTQ_SET_CLIENT_FLAGS, c); }
    void SetGroupFlags(unsigned short inId, unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask,
                       unsigned long inAsyncFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inId).N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags)
            .Run(this, EE_WON_RTQ_SET_GROUP_FLAGS, c);
    } // 2bdc10
    void SendData(const ByteBuffer* inData, unsigned char inFlags, unsigned short inRecipient, RouteCompletion* c)
    { this->SendData(inData, inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be910
    void SendData(const ByteBuffer* inData, unsigned char inFlags, const std::list<unsigned short>& inRecipients,
                  RouteCompletion* c)
    { RoutingRequestValues().D(inData).N(inFlags).L(inRecipients).Run(this, EE_WON_RTQ_DATA, c); } // 2bea70
    // a ban or a mute: the group, on, the seconds, the explanation, the client (by client id, WON id or CD key id)
    void BanClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                   unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf590
    void BanClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                          unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf6b0
    void BanClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                            const std::wstring& inExplanation, unsigned long inCDKeyId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN_CDKEY_ID, inGroupId, inOn, inSeconds, inExplanation, inCDKeyId, c); } // WON SDK
    void MuteClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                    unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf7d0
    void MuteClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                           const std::wstring& inExplanation, unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf8f0
    void MuteClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                             const std::wstring& inExplanation, unsigned long inCDKeyId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE_CDKEY_ID, inGroupId, inOn, inSeconds, inExplanation, inCDKeyId, c); } // WON SDK
    // 2bd9f0 (accept), 2bdb00 (invite): the answer to a client's join attempt, an invitation
    void AcceptClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(this, EE_WON_RTQ_ACCEPT, c); }
    void InviteClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(this, EE_WON_RTQ_INVITE, c); }
    void BecomeModerator(bool inOn, RouteCompletion* c) { RoutingRequestValues().N(inOn).Run(this, EE_WON_RTQ_MODERATOR, c); } // 2bf3f0
    void SendServerAlert(const std::wstring& inText, RouteCompletion* c)
    { RoutingRequestValues().W(inText).Run(this, EE_WON_RTQ_ALERT, c); } // 2bddd0
    void StartServerShutdown(const std::wstring& inText, unsigned long inSeconds, RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inSeconds).Run(this, EE_WON_RTQ_START_SHUTDOWN, c); } // 2be010
    void AbortServerShutdown(RouteCompletion* c) { RoutingRequestValues().Run(this, EE_WON_RTQ_ABORT_SHUTDOWN, c); } // 2be200

    // Further WON SDK requests; no original address identified.
    void DisconnectClient(RouteCompletion* c) { RoutingRequestValues().Run(this, EE_WON_RTQ_DISCONNECT, c); }
    // the _KeyId data objects (link 0xffff, flags 0)
    void GetClientCDKeyIdList(RouteCompletion* c, bool inSubscribe)
    { RoutingRequestValues().N(inSubscribe).N(0xffff).S("_KeyId").N(0).Run(this, EE_WON_RTQ_READ_DATA, c); }
    void GetClientListEx(RouteCompletion* c) { RoutingRequestValues().Run(this, EE_WON_RTQ_CLIENT_LIST_EX, c); }
    void SetGroupName(unsigned short inGroupId, const std::wstring& inName, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).W(inName).Run(this, EE_WON_RTQ_SET_GROUP_NAME, c); }
    void SetGroupMaxPlayers(unsigned short inGroupId, unsigned short inMaxPlayers, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inMaxPlayers).Run(this, EE_WON_RTQ_SET_GROUP_MAX_PLAYERS, c); }
    void RelinquishCaptaincy(unsigned short inGroupId, unsigned short inNewCaptainId, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inNewCaptainId).Run(this, EE_WON_RTQ_RELINQUISH, c); }
    // nothing for an empty list
    void SendServerAlertWithRecipientList(const std::wstring& inText, std::list<unsigned short>& inRecipients,
                                          RouteCompletion* c)
    { RoutingRequestValues().W(inText).L(inRecipients).Run(this, EE_WON_RTQ_ALERT_TO, c); }
    // a RoutingDetectFirewallOp
    void DetectFirewall(unsigned short inListenPort, unsigned long inMaxConnectSeconds, bool inDoListen, RouteCompletion* c)
    { RoutingRequestValues().N(inListenPort).N(inMaxConnectSeconds).N(inDoListen).Run(this, EE_WON_RTQ_DETECT_FIREWALL, c); }
    void GetBadUserList(RoutingBadUserListType inListType, unsigned short inGroupId, unsigned char inFlags, RouteCompletion* c)
    { RoutingRequestValues().N(inListType).N(inGroupId).N(inFlags).Run(this, EE_WON_RTQ_BAD_USERS, c); }

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
    void Moderate(int inKind, unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                  const std::wstring& inExplanation, unsigned long inId, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inOn).N(inSeconds).W(inExplanation).N(inId).Run(this, inKind, c); }
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
// finished op's fields in and sends the request a chat input makes.
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
    // WON SDK value-copy records; independent, no enclosing-object fields.
    struct ClientInfo {
        std::wstring mName;
        unsigned short mId;
        unsigned long mFlags;
        unsigned long mCDKeyId;
        ClientInfo() : mId(0), mFlags(0), mCDKeyId(0) {}
    };
    struct MemberInfo {
        std::wstring mName;
        unsigned short mClientId;
        unsigned long mClientFlags;
        unsigned char mMemberFlags;
        MemberInfo() : mClientId(0), mClientFlags(0), mMemberFlags(0) {}
    };
    struct GroupInfo {
        std::wstring mName;
        unsigned short mId;
        unsigned short mCaptainId;
        unsigned short mMaxPlayers;     // the group's client limit (RoutingGroupInfo::mMaxClients)
        unsigned long mFlags;
        unsigned long mAsyncFlags;
        unsigned short mObserverCount; // only applicable if non-detailed observers is on
        unsigned short mMemberCount;   // RoutingGroupInfo::mMaxPlayers (+0x2a): the group list's member count
        GroupInfo() : mId(0), mCaptainId(0), mMaxPlayers(0), mFlags(0), mAsyncFlags(0), mObserverCount(0),
            mMemberCount(0) {}
    };
    SmartRoutingConnection(); //2bfb00; the 0x258 allocation is this class
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
    bool GetClient(unsigned short,ClientInfo&);
    bool GetGroup(unsigned short,GroupInfo&);
    bool GetMember(unsigned short,unsigned short,MemberInfo&);
    void StartClientItr() { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_start(this->mModel, 0, 0); }
    bool GetNextClient(ClientInfo&);
    void StartGroupItr() { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_start(this->mModel, 1, 0); }
    bool GetNextGroup(GroupInfo&);
    bool StartMemberItr(unsigned short inId)
    { AutoCrit aCrit(this->GetDataLock()); return ee_won_smart_start(this->mModel, 2, inId); }
    bool GetNextMember(MemberInfo&);
    bool GetLastTargetMember(MemberInfo&);
    bool GetLastTargetClient(ClientInfo&);
    // 2c35a0: the client a typed name refers to (exact, unique prefix, ambiguous or not found); outFound when the
    // search set one (the original's out pointer)
    MatchResult FindClient(const std::wstring&, ClientInfo&, bool& outFound);
    // 2c37d0: the client GetClient(id) finds ignored or not, the ignore list saved
    void IgnoreClient(unsigned short inClientId, bool inOn)
    { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_ignore(this->mModel, inClientId, inOn); }
    // 2c3970: a chat command word (case-insensitive)
    void AddCommandString(const std::wstring& inText, InputCommand inCommand)
    {
        AutoCrit aCrit(this->GetDataLock());
        ee_won_smart_add_command(this->mModel, inText.data(), inText.length(), inCommand);
    }
    void ClearCommandStrings() { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_clear_commands(this->mModel); }
    void SetAllowPartialNameMatch(bool inAllow) { this->SetSmart(EeWonSmartNumber::PartialNames, inAllow); } //2c40a0
    void SetServerWideCommands(bool inOn) { this->SetSmart(EeWonSmartNumber::ServerWide, inOn); } //2c40f0
    void SetDoColonEmote(bool inOn) { this->SetSmart(EeWonSmartNumber::ColonEmote, inOn); }
    // a line of chat input: a command, a colon emote or a broadcast; its request sent (ee_won routecpp.rs)
    bool HandleInput(const std::wstring& inInput, int inGroupIdContext = -1)
    {
        AutoCrit aCrit(this->GetDataLock());
        return ee_won_smart_handle_input(this->mModel, static_cast<RefCount*>(this), inInput.data(), inInput.length(),
                                         inGroupIdContext);
    }
    // 2c2fc0, the ignore file's path (provisional name)
    void SetProductName(const std::string& inPath)
    {
        AutoCrit aCrit(this->GetDataLock());
        ee_won_smart_set_path(this->mModel, reinterpret_cast<const uint8_t*>(inPath.data()), inPath.length());
    }
    void ClearProductName() { this->SetProductName(""); }
    unsigned short GetMyClientId() // OnChat 0x4d040c
    { AutoCrit aCrit(this->GetDataLock()); return (unsigned short)ee_won_smart_number(this->mModel, EeWonSmartNumber::ClientId); }
    EeWonSmartRoute* mModel; // owned
protected:
    virtual ~SmartRoutingConnection() { ee_won_smart_free(this->mModel); }
private:
    void SetSmart(EeWonSmartNumber inWhich, bool inOn) { AutoCrit aCrit(this->GetDataLock()); ee_won_smart_set(this->mModel, inWhich, inOn); }
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
