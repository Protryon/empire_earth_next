// WONAPI routing-server API as the game's WON front end (EEWonNet.cpp, 0xcd4ee..0xd3ff4) uses it. Class names
// come from RTTI (.?AVRoutingConnection@WONAPI@@, ...), members and offsets from EEWonNet.cpp's use; methods are
// named by address where the WONAPI name is unknown (provisional names). Requires <string>, <list>, <map>,
// ee/EEWonNet.h.
#ifndef EE_WONAPI_WONROUTING_H
#define EE_WONAPI_WONROUTING_H

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

// AsyncOp: 0x44 bytes in the original (ctor 29ef80, dtor 29f000, three vtable slots). A Rust object since round 13
// (rust/crates/ee_won/src/opobj.rs EeWonAsyncOp: the run state, the lock, the completion's reference, the
// container, and the run, finish, kill and completion steps) behind this shim; the virtual hooks, the completion's
// call, the references and WONAPICore's queue and timer come back through AsyncOp.cpp's hook table. The timer
// thread's entry of the op is the Rust timer thread's (thread.rs)
class AsyncOp : public RefCount {
public:
    WONStatus Run(long inMode,unsigned long inTimeout);      //29f500, returns the status

    AsyncOp();                              // 0x29ef80
    virtual ~AsyncOp();                     // 0x29f000
    virtual void RunHook();
    virtual void CleanupHook();
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
    void SetContainer(AsyncOpContainer* inContainer);
    void SetMode(AsyncRunMode); //29f190, conditional setter (enum argument)
    void ForceFinish(WONStatus inStatus); //29f2f0, takes the full status
    void RunAsTimer(unsigned long inTimeout); //29f610, DWORD timeout/ret4
    void Finish(WONStatus inStatus);   // original29f220: completes pending status under lock
    void SetTimeout(unsigned long inTimeout); //29f1b0, timeout DWORD at2c/ret4
    bool RunBlock(unsigned long inTimeout); //29f6e0, 32-byte wrapper
    void RunAsync(long inTimeout);          // 0x29f6d0
    void Kill(bool inFlag);                 // 0x29f1e0
    void SetRunAsyncImmediately(bool inFlag);  // 0x29f1d0
    void QueueInOpContainer(); //29f200, queues the op in its container
    void Complete(); //29f3a0
    void StartRunAsync(); //29f4c0
    unsigned long TimeLeft();               // 0x29f700
    DWORD RealTimeLeft();
    // the run mode, start and timeout for the Rust op algorithms (rust/crates/ee_won/src/ops.rs)
    EeWonOpTiming GetTiming() const { return ee_won_asyncop_timing(this->mOp); }
    bool IsAsync() const { return this->GetMode()==1; }
    //: 0x291983 (an inline COMDAT of EELobbyFileTransfer.cpp): finished without error
    bool Succeeded() { return this->GetStatus()==0 && !this->IsKilled(); }

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
    AsyncOpContainer();
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

class SocketOp;
class MsgTransform {
public:
    // Empty virtual dtor (WON SDK); the 32-byte deleting dtor 2ad880 stores the own vtable 44ca2c
    virtual ~MsgTransform() {}
    virtual WONStatus Send(ByteBufferPtr&) { return 0; } //VT+4=2ad870, ret4
    virtual WONStatus Recv(ByteBufferPtr&) { return 0; } //VT+8=2ad870, ret4
};

// WON SDK NetStats: static functions only. The counters are Rust's (rust/crates/ee_won/src/netstats.rs)
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

class AsyncSocket : public RefCount {
public:
    enum SocketType { TCP, UDP }; //WON SDK type, 32-bit enum
    SocketType GetType(); //2b0a40, 4-byte nonvirtual function
    static int TimeLeft(DWORD,DWORD);
    void RemoveAllMsgTransforms();
    bool IsOpen();
    void SetType(SocketType);
    WONStatus SetNagling(bool);
    static WONStatus StartWinsock(); //2b0940, cdecl (WON SDK); the user count is Rust's
    static WONStatus StopWinsock(); //2b0980, cdecl (WON SDK)
    static bool HasInternetConnection();
    WONStatus SetBroadcast(bool); //2b0a50, sets the broadcast socket option
    WONStatus Open();     //2b0a90, creates the socket
    WONStatus RecvFrom(void*,int,IPAddr*,int*); //2b1840, signed count/output on success
    void Disconnect(); //2b1980, disconnect
    WONStatus Bind(const IPAddr&,bool);     //2b0c90, address bind/reuse
    WONStatus Fn2B0E60(const IPAddr&); //2b0e60, connect
    WONStatus Accept(SmartPtr<AsyncSocket>&);   //2b1060, owning accepted output reference
    WONStatus WaitForConnect(unsigned long); //2b1220, waits for the connection
    void WaitForAccept(unsigned long); //2b1350/13 (WON SDK)

    AsyncSocket(int inType); //2b0340; fresh factory2b0420 new74
    virtual ~AsyncSocket(); //ordinary2b04a0; scalar deleting2b0480
    virtual void OpenHook();  //326b60; called on open at 2b0c69, SDK OpenHook
    virtual void CloseHook(); //326b60; close2b1b1d/kill2b1b9d, SDK CloseHook
    virtual void KillHook();  //326b60; kill2b1ba4, SDK KillHook
    virtual AsyncSocket* Duplicate(); //2b0420, factory result consumed2b111c
    WONStatus Fn2B0D90(unsigned short,bool); // bind to the local host address: (port, allowReuse)
    WONStatus Listen(int);   //2b0fc0
    void WaitForWrite(unsigned long); //2b1360
    WONStatus Send(const void*,int,int*); //2b14c0
    void Close();    //2b1ad0
    void Kill();  //2b1b50; separate from the SDK's Kill
    unsigned short GetLocalPort(); //2b08f0, called at 2cf882; the SDK takes no arguments
    unsigned char GetLengthFieldSize() const; //2b0610, called at 2b5b10
    void SetLengthFieldSize(unsigned char); //2b0600, bytearg->50/ret4
    bool IsConnected(); //2b08d0, byte51 ->AL; SDK IsConnected corroboration
    IPAddr GetDestAddr(); //2b08e0; a copy taken under the lock (the original returned a reference to the member)
    void AddMsgTransform(MsgTransform*); //2b0620, owns raw transform pushed into list
    void WaitForRead(unsigned long); //2b1410, wait-for-read timeout
    void ClosePrv(); //2b1a10, ClosePrv handle shutdown/close
    WONStatus GetLastSocketError(); //2b0890, socket WSA status mapping
    bool IsValid();  //2b08c0, handle20 != PLAT_SOCKET_INVALID
    WONStatus TransformRecvMsg(ByteBufferPtr&); //recv transforms reverse-list slot08
    WONStatus TransformSendMsg(ByteBufferPtr&); //send transforms forward-list slot04
    WONStatus SendTo(const void*,int,const IPAddr&);   //sendto
    WONStatus Recv(void*,int,int*);     //recv

    typedef void (*CloseCallback)(AsyncSocket*);
    // the socket's lock (the original's mDataCrit, a recursive mutex the Rust object owns), which the subclasses
    // (QueueSocket, the routing and server connections) take around their own state too: AutoCrit takes it
    PlatMutex* GetDataLock() const { return static_cast<PlatMutex*>(ee_won_asyncsocket_mutex(this->mSock)); }
    void SetDestAddr(const IPAddr& inAddr) { ee_won_asyncsocket_set_dest_addr(this->mSock,inAddr.GetValue()); }
    void SetThreadCloseCallback(CloseCallback inCallback);
    WONStatus GetSocketStatus() const; // the last socket error recorded (the original's +0x54)
    EeWonAsyncSocket* GetSocketObject() const { return this->mSock; }
private:
    AsyncSocket(const AsyncSocket&);
    AsyncSocket& operator=(const AsyncSocket&);
    // A Rust object since round 14 (rust/crates/ee_won/src/sockobj.rs EeWonAsyncSocket): the handle, type, flags,
    // status and times (the original's +0x20..+0x5c), the length field size (+0x50), the message transforms (+0x60,
    // owned) and the socket thread's close callback (+0x6c), and every step of AsyncSocket.cpp; since round 15 also
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
    SocketOp(AsyncSocket* inSocket); // SocketRunHooks.cpp (the original's was inline)
    virtual ~SocketOp(); // the Rust object (the original's was implicit, 29153d)
    virtual void RunHook();                 // +0x04 0x2b6070 RunHook
    virtual void CleanupHook();             // +0x08 0x2b60c0
    //: +0x0c starts the operation, +0x10 continues it (SendFileOp 0x2915d2 / 0x291613 return a WONStatus)
    virtual WONStatus Start() { return this->Continue(); }  // +0x0c 0x29151c
    virtual WONStatus Continue() = 0;       // +0x10 (_purecall)
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
private:
    static void HookAddToSocketThread(void* inOp); // WONAPICore's friend
    static const EeWonSocketOpBaseHooks kHooks;
    // A Rust object since round 14 (rust/crates/ee_won/src/sockop.rs EeWonSocketOp): the socket thread flag (+0x44),
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
// (a Rust object since round 15, rust/crates/ee_won/src/sockops.rs: the accepted socket, +0x5c, a reference held)
class AcceptOp : public SocketOp {
public:
    AcceptOp(AsyncSocket* inSocket) : SocketOp(inSocket)
    { this->SetWaitForEvent(0,true); ee_won_socketop_init_accept(this->GetSocketOpObject()); }
    AsyncSocket* GetAcceptedSocket()
    { return static_cast<AsyncSocket*>(static_cast<RefCount*>(ee_won_socketop_accepted(this->GetSocketOpObject()))); }
    virtual WONStatus Continue();           // +0x10 0x2b0310 (WONAPI)
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
    virtual WONStatus Start();              // +0x0c 0x2b2320 (WONAPI)
    virtual WONStatus Continue();           // +0x10 0x2b2330 (WONAPI)

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
    RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket = NULL); //2b4de0
    virtual void CleanupHook(); //2b5580
    virtual WONStatus Start();     //2b4ee0
    virtual WONStatus Continue();  //2b4ef0
    virtual AsyncOp* Duplicate(); //2b4f00
    void SetProgressCompletion(CompletionBase<AsyncOpPtr>*, long inInterval);
    ByteBufferPtr GetBytes() const; //2b1df0, owning return by value
    void SetFilePath(const std::string& theFilePath)
    { ee_won_socketop_set_path(this->GetSocketOpObject(),reinterpret_cast<const uint8_t*>(theFilePath.data()),theFilePath.size()); }
    void SetAppendFile(bool theVal) { ee_won_socketop_set_flag(this->GetSocketOpObject(),0,theVal); }
    void SetBinaryFile(bool isBinary) { ee_won_socketop_set_flag(this->GetSocketOpObject(),1,isBinary); }
    void SetFileModifyTime(time_t theTime) { ee_won_socketop_set_modify_time(this->GetSocketOpObject(),static_cast<int64_t>(theTime)); }
    void SetSkipBytes(bool skip) { ee_won_socketop_set_flag(this->GetSocketOpObject(),2,skip); }
    std::string GetFilePath() const; // a copy (the original returned a reference to the member)
    unsigned long GetBytesRead() const { return ee_won_socketop_bytes_read(this->GetSocketOpObject()); }
protected:
    struct MessageTag {};
    RecvBytesOp(AsyncSocket* inSocket, MessageTag); // RecvMsgOp's
};
class RecvMsgOp : public RecvBytesOp {
public:
    RecvMsgOp(AsyncSocket* inSocket = NULL) : RecvBytesOp(inSocket, MessageTag()) {}
    virtual WONStatus Start();     //2b5b00
    virtual WONStatus Continue();  //2b5e20
    virtual AsyncOp* Duplicate(); //2ab3d0
    ByteBufferPtr GetMsg() const; //2ab770, owning return by value
};

// vtable 44ca78, six slots
class RecvCRMsgOp : public SocketOp {
public:
    RecvCRMsgOp(AsyncSocket* inSocket=NULL) : SocketOp(inSocket) {this->SetWaitForEvent(0,true);}
    virtual WONStatus Start();     //2b5970,62
    virtual WONStatus Continue();  //2b59b0,330 ending ret2b5af9, excludes6align
    virtual AsyncOp* Duplicate() {return new RecvCRMsgOp(this->GetSocket());} //2b2180, 209 bytes
    ByteBufferPtr GetMsg() const {return this->mMessage;} //accessor 2b1df0 (ICF-folded)
private:
    // the line so far (the original's WriteBuffer at 0x5c) is the op's Rust object's (sockop.rs)
    ByteBufferPtr mMessage; //74, total78
    //Implicit normal2b2280/157 has NO own-vptr store: SP74,Buffer5c,SocketSP54,AsyncBase.
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

class QueueSocket : public AsyncSocket {
public:
    void StartCloseTimer(long inTimeout); //2b3d50, StartCloseTimer
    void CancelCloseTimer(); //2b3ed0, CancelCloseTimer
    static void StaticCloseTimerCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam); //owning value callback
    void CloseTimerCallback(AsyncOp* inTimer); //2b4010
    static void StaticOpCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam); //owning value callback
    void OpCallback(SocketOp* inOp); //2b4220
    void RunRepeatOp(); //2b45b0, RunRepeatOp
    void RunQueueOp(); //2b48b0, RunQueueOp
    QueueSocket(int inType); //2b3930; fresh factory2b39d0 newa0
    virtual ~QueueSocket(); // the Rust object (the original's was implicit, 2b3a60)
    virtual void OpenHook();  //2b4c00, byte9c=false,ret
    virtual void CloseHook(); //2b4c10,noargs/void, SDK CloseHook
    virtual void KillHook();  //2b4d40,noargs/void, SDK KillHook
    virtual AsyncSocket* Duplicate(); //2b39d0
    void SetRepeatCompletion(CompletionBase<AsyncOpPtr>*); //2b3bf0 writes7c; repeat completion
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>*); //2b3ca0 writes80
    void SetQueueCompletion(CompletionBase<AsyncOpPtr>*);
    void QueueOpFast(SocketOp*,DWORD inTimeout=0xffffffff);
    void SetRepeatOp(SocketOp*); //2b4500, repeat op84; never broad AsyncOp*
    void QueueOp(SocketOp*,long inTimeout);  //2b4740, accesses/rebinds op54
    void QueueClose(long inTimeout); //2b4a70, distinct from Close() 2b1b50

private:
    // A Rust object since round 14 (rust/crates/ee_won/src/qsock.rs EeWonQueueSocket): the default, current, repeat
    // and close completions (+0x74..+0x80), the repeat and queued ops (+0x84, +0x88), the close timer (+0x8c), the
    // op queue (+0x90) and the queued-close flag (+0x9c), each a reference held, and every step of QueueSocket.cpp
    EeWonQueueSocket* mQueue;
};

class BlockingSocket : public QueueSocket {
public:
    WONStatus Connect(const IPAddr&,unsigned long); //2b1c10
    WONStatus SendBytes(const ByteBuffer*,unsigned long); //2b1d00
    WONStatus SendBytesTo(const ByteBuffer*,const IPAddr&,unsigned long);
    WONStatus RecvBytesFrom(ByteBufferPtr&,unsigned short,IPAddr&,unsigned long);
    WONStatus RecvMsg(ByteBufferPtr&,unsigned long); //2b1f10
    WONStatus RecvBytes(ByteBufferPtr&,DWORD,DWORD inTimeout=0xffffffff);
    WONStatus RecvCRMsg(ByteBufferPtr&,unsigned long); //2b2040
    WONStatus SendMsg(const ByteBuffer*,unsigned long); //2b1e30 (WON SDK)
    BlockingSocket(int inType); //2b1bd0, just QueueSocket(inType), ownvptr
    // IMPLICIT destructor normal2aad90(375B), no own-vptr; inlines Queue members
    // then calls AsyncSocket::~AsyncSocket 2b04a0. Scalar deleting 2b1bf0.
    virtual AsyncSocket* Duplicate(); //2aab50, newa0 and forwards type24
};

// further socket operations and managers

// TD4d4198, VT44c9bc, total6c. A Rust object since round 15 (sockops.rs: the bytes +0x68, a reference held, and the
// read position of the original's ReadBuffer +0x5c)
class SendBytesOp : public SocketOp {
public:
    SendBytesOp(const ByteBuffer* inData, AsyncSocket* inSocket = NULL); // SendMsgOp passes NULL
    virtual WONStatus Start();     //2b5e50
    virtual WONStatus Continue();  //2b5e70
};

// TD4d41c0, VT44c9a0, total70 (the message +0x6c is the Rust object's)
class SendMsgOp : public SendBytesOp {
public:
    SendMsgOp(const ByteBuffer* inMessage, AsyncSocket* inSocket = NULL);
    virtual WONStatus Start();     //2b5fa0
    virtual WONStatus Continue();  //2b6020
};

// TD4d4040, VT44c948, total88 (the bytes +0x5c and the address +0x60 are the Rust object's)
class SendBytesToOp : public SocketOp {
public:
    SendBytesToOp(const ByteBuffer* inData, const IPAddr& inAddress, AsyncSocket* inSocket = NULL);
    virtual WONStatus Continue();  //2b5f10
};

// TD4d4440, VT44caf4, total90. Original out-of-line constructor2b55a0. A Rust object since round 15 (sockops.rs:
// the buffer +0x5c, the datagram +0x60, the size +0x64, the sender +0x68)
class RecvBytesFromOp : public SocketOp {
public:
    RecvBytesFromOp(unsigned short inMaxLength, AsyncSocket* inSocket = NULL);
    virtual ~RecvBytesFromOp(); //2b56f0
    virtual void RunHook();   //2b57d0
    virtual WONStatus Continue();  //2b5820
    virtual AsyncOp* Duplicate(); //2b5660, fresh allocation of the same class
    SmartPtr<ByteBuffer> GetBytes() const; //2aa920, owning51B sret/AddRef, field60
    IPAddr GetAddr() const; // a copy (the original returned a reference to the member)
};

// AoC UDP records: four slots each; the archived UDP header's extra
// RetransmitHook is a version difference and MUST NOT be inserted here.
// A Rust object since round 16 (rust/crates/ee_won/src/udpops.rs EeWonUdpRecvOp: the socket +0x44, the manager
// +0x48, the own-socket flag +0x4c, the address +0x50, the datagram +0x78, the check +0x7c and its context +0x80, and
// RunHook, CleanupHook, QueueRecv and RecvCallback); the class is a shim the lobby subclasses
class UDPManager;
class UDPManagerRecvOp : public AsyncOp {
public:
    UDPManagerRecvOp(); //2b68b0, default, 91 bytes
    UDPManagerRecvOp(const IPAddr&); //2b6a10
    virtual ~UDPManagerRecvOp(); // the original's was implicit (2b6930)
    bool HasVerifier();
    void SetUDPManager(UDPManager* theManager);
    void SetSocket(QueueSocket* theSocket);
    IPAddr GetFromAddr() const; // a copy (the original returned a reference to the member)
    void SetFromAddr(const IPAddr& theAddr);
    const ByteBuffer* GetMsg() const;
    void SetMsg(const ByteBuffer* theMsg);
    QueueSocket* GetSocket() const;
    UDPManager* GetUDPManager() const;
    EeWonUdpRecvOp* GetUdpObject() const { return this->mUdp; }
    virtual void RunHook();   //2b6fb0
    virtual void CleanupHook(); //2b6f60
    virtual WONStatus CheckMessage(ReadBuffer&); //2b6c10, new slot 0c
    typedef WONStatus (*MessageVerifier)(ReadBuffer&, void*);
    void SetMessageVerifier(MessageVerifier,void*); //2b6bf0
    void QueueRecv(); //2b6c30
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b6d70, owning values
    void RecvCallback(RecvBytesFromOp*); //2b6e60
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
    static WONStatus StaticCheckResponse(ReadBuffer&,void*); //2b7250
    void SendRequest(); //2b7260
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b7550, owning values
    void RecvCallback(AsyncOp*); //2b75e0

    UDPManagerSendRecvOp(); //2b70b0
    virtual ~UDPManagerSendRecvOp(); // the original's was implicit (2b7140)
    void SetUDPManager(UDPManager* theManager);
    void SetMsgTimeout(DWORD theTimeout);
    void SetNumRetransmits(int theNum);
    void SetDestAddr(const IPAddr& theAddr);
    IPAddr GetDestAddr() const; // a copy (the original returned a reference)
    void SetOutgoing(const ByteBuffer* theMsg); // the request datagram
    const ByteBuffer* GetOutgoing() const;
    QueueSocket* GetSocket() const; // the op's own socket (none with a manager)
    UDPManagerRecvOp* GetRecvOp() const; // the reply wait
    int GetAttemptCount() const;
    int GetNumRetransmits() const;
    unsigned long GetMsgTimeout() const;
    virtual void RunHook();   //2b7470
    virtual void CleanupHook(); //2b7630
    virtual WONStatus CheckResponse(ReadBuffer&); //2ad870, returns status 0 by default (ret4)
private:
    UDPManagerSendRecvOp(const UDPManagerSendRecvOp&);
    UDPManagerSendRecvOp& operator=(const UDPManagerSendRecvOp&);
    EeWonUdpSendRecvOp* mUdp;
};

// A Rust object since round 15 (rust/crates/ee_won/src/udpmgr.rs EeWonUdpManager: the socket +0x08, the default
// completion +0x0c, the auto-receive op +0x10, the clean-up cursors +0x14/+0x18, the list +0x1c and the multimap
// +0x28 of waiting receive ops, the receive size +0x34, and Close, SetAutoRecv, QueueOp, Send, CleanOps, Bind, the
// erases 0x2b8160/0x2b81b0 and RecvCallback's dispatch). The original's mAutoRecv (+0x38) was never read: gone
class UDPManager : public RefCount {
public:
    WONStatus Bind(int,bool);     //2b79c0, signed int port as in the SDK
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b8230, owning values
    void RecvCallback(RecvBytesFromOp*); //2b8320, receive callback

    UDPManager(); //2b76b0
    virtual ~UDPManager(); //2b77b0
    void SetDefaultCompletion(CompletionBase<AsyncOpPtr>* theCompletion);
    void Send(const ByteBuffer*, const IPAddr&); //2b7ec0, void/ret8 (provisional name; the SDK has SendBytesTo)
    void Close(); //2b78e0
    void SetAutoRecv(bool); //2b7b40
    void QueueOp(UDPManagerRecvOp*,unsigned long); //2b7c50
    void CleanOps(); //2b8000
    QueueSocket* GetSocket() const; // the socket (NULL when unbound)
private:
    UDPManager(const UDPManager&);
    UDPManager& operator=(const UDPManager&);
    EeWonUdpManager* mManager;
};


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
// ee_won's (rust/crates/ee_won/src/httpengine.rs EeWonHttpConn): this class is the RefCount its ops' completions hold
class HTTPConnection : public RefCount {
public:
    HTTPConnection(HTTPSession* inSession = NULL); //2d2c80,new64 at2d2a97
    virtual ~HTTPConnection(); //2d2d50, the members
    void Kill(); //2d3600
    void QueueOp(RustServerOp*); //2d33a0,ret4
    void QueueOp(RustServerOp*,int); //2d3290,ret8
    void RunOp(int); //2d3430,ret4
    int GetNumOps();
    static void StaticOpCallback(AsyncOpPtr,SmartPtr<RefCount>); //2d2df0, two owning by-value args
protected:
    void OpCallback(RustServerOp*); //2d2e80, 1030 bytes; protected in the SDK
private:
    friend struct HTTPEngineHooks;
    CriticalSection mDataCrit; //08..20
    HTTPSession* mSession; //20 NONOWNING, ctor raw assignment
    EeWonHttpConn* mImpl;   // owned: 0x24..0x64 (socket, counts, flag, last use, queues, retry, running ops)
};
typedef SmartPtr<HTTPConnection> HTTPConnectionPtr;

// 0x2c bytes: constructor 2d27b0, map destruction 2d2870. The connections by host (StringLessNoCase) are ee_won's
// (httpengine.rs EeWonHttpSession).
class HTTPSession : public RefCount {
public:
    HTTPSession();                          //2d27b0, out of line
    virtual ~HTTPSession();                 //2d2870, the map
    void AddOp(RustServerOp*);               //2d28f0, queues a completed op
    void KillAll();                         //2d2bc0
    int GetNumOps();
private:
    friend struct HTTPEngineHooks;
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

// Every routing op, request or notification, is one Rust object since net round 18 (rust/crates/ee_won/src/
// routeop.rs EeWonRouteOp: the op's members by field number EE_WON_RTF_* and text EE_WON_RTT_*, its data type,
// data and recipients, the records its replies build, SendRequest's message and HandleReply's read, and
// RoutingCreateGroupOp::CheckLeaveGroup). This class is the AsyncOp the connection's list and the completions hold,
// one per op type (RoutingOpType, WONRoutingOpTypes.h); its readers read the members through the accessors and the
// records as copies. The original's 55 op classes (RunHook 2c7d20, HandleReply 2bcd30, the request builders and
// notification parsers 2c4170..2c8f00) are gone; RoutingDetectFirewallOp keeps its listening socket below.
class RoutingOp : public AsyncOp {
public:
    RoutingOp(RoutingConnection* inConnection, RoutingOpType inType); // a connection-less op with NULL
    virtual void RunHook();   //RunHook2c7d20: in blocking mode finishes BlockMode_NotSupported, else SendRequest
    virtual void SendRequest(); // the op's request (Rust's), sent; added to the connection's list when it waits
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // Rust's read; a cut short read throws
    virtual RoutingOpType GetType() const;
    bool IsDerivedServerOp() const { return this->GetType() == 60; } // RoutingOp_DerivedServerOp
    void SendMsg(const std::string& inMessage); //2c7c00: queued on the connection with its length field
    void AddOp(); //2c7d10
    RoutingConnection* GetConnection() { return this->mConnection.get(); }

    // the members (EE_WON_RTF_* numbers, EE_WON_RTT_* texts)
    long long Num(int inField) const { return ee_won_routeop_num(this->mRust, inField); }
    void SetNum(int inField, long long inValue) { ee_won_routeop_set_num(this->mRust, inField, inValue); }
    std::wstring Text(int inWhich) const;
    void SetText(int inWhich, const std::wstring& inText);
    std::string DataType() const;
    void SetDataType(const std::string& inType);
    ByteBufferPtr GetData() const; // NULL when the op has no buffer
    void SetData(const ByteBuffer* inData);
    std::list<unsigned short> Recipients() const;
    void SetRecipients(const std::list<unsigned short>& inIds);
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
    bool ParseGroupListObj(const void*, unsigned long);  //2c5f70, the group list op's body into its map

    SmartPtr<RoutingConnection> mConnection; //60, destructor owns release
    EeWonRouteOp* mRust; // owned
protected:
    virtual ~RoutingOp();
};
typedef SmartPtr<RoutingOp> RoutingOpPtr;
#define EE_WON_ROUTE_OPS_RUST 1 // the ops are Rust objects (won_routeops_test reads them so)

class RoutingGetUserStatusOp : public UDPManagerSendRecvOp {
public:
    typedef std::map<unsigned long,unsigned char> FriendStatusMap;
    RoutingGetUserStatusOp(const IPAddr&,UDPManager*); //2c6630/ret8
    virtual void RunHook();   //RunHook2c6970
    virtual WONStatus CheckResponse(ReadBuffer&); //UnpackMsg2c6ab0
    void AddWONUserId(unsigned long); //2c68c0/ret4
    const FriendStatusMap& GetFriendStatusMap() const { return this->mFriendStatusMap; }
    FriendStatusMap mFriendStatusMap; //88 allocator/header8c/count90, full94
    // No explicit destructor: ordinary2c6750 has NOown-vptr write and inlines
    // the map destruction, then the base (0x88) member/base cleanup.
};


inline const char* GetBufferData(const ByteBufferPtr& inBuffer)
{
    return inBuffer->data();
}

// the WON server list the account ops use (??_G 0xceaec, dtor 0xceb08; sizeof 0x3c).: layout from
// the dtor and the base game's out-of-line ctor (base exe 0x4c772a; AoC's WONAPI inlines it: 0x2ddb70, 0x3628c0);
// (provisional member names)
typedef std::map<std::wstring,DirEntityList> DirEntityMap;
typedef std::set<IPAddr> AddrSet;
// A shim over ee_won's server list (rust/crates/ee_won/src/servctx.rs: the addresses, their order, the one-time
// shuffle and the lock); the original's 0x3c-byte layout is gone
class ServerContext : public RefCount {
public:
    ServerContext();
    virtual ~ServerContext();
    bool AddAddress(const IPAddr& inAddress); //2ac4b0, AL = success
    void Clear(); //2ac390
    void NotifyFailed(const IPAddr&); //2ac980
    bool AddAddressesFromDir(const DirEntityList&,const wchar_t* inNameFilter=NULL); //2acab0/332
    void RemoveAllAddresses(); //2acc00, SDK name, 5-byte body
    unsigned long GetNumAddresses();
    typedef std::list<IPAddr> AddrList;
    void CopyAddresses(AddrList&); //2ac6d0, 685 bytes
    bool RemoveAddress(const IPAddr&);
    void CopyAddresses(AddrSet&);
    bool AddAddressesFromDir(const DirEntityMap&,const std::wstring&,const wchar_t* inNameFilter=NULL);
    bool AtomicClearAndAddAddressesFromDir(const DirEntityList&,const wchar_t* inNameFilter=NULL);
    bool AtomicClearAndAddAddressesFromDir(const DirEntityMap&,const std::wstring&,const wchar_t* inNameFilter=NULL);
    // the original's public mShuffleAddresses (copies shuffle once while it is on)
    void SetShuffleAddresses(bool inShuffle);
    bool GetShuffleAddresses();

private:
    ServerContext(const ServerContext&);
    ServerContext& operator=(const ServerContext&);
    EeWonServerList* mList;
};

// AsyncOpWithTracker's op tracker: a Rust object since round 13 (rust/crates/ee_won/src/opobj.rs EeWonOpTracker: the
// ops tracked by id, a reference each, in id order, the latest first among equal ids; an op tracked once; its lock).
// The original's two multimaps (AsyncOpTrackerMaps, AsyncOpTrackerEntry) are gone.
const EeWonAsyncOpHooks* AsyncOpHooks(); // AsyncOp.cpp's hook table
class AsyncOpTracker {
public:
    AsyncOpTracker() : mTracker(ee_won_op_tracker_new(AsyncOpHooks())) {}
    ~AsyncOpTracker() { ee_won_op_tracker_free(this->mTracker); }
    int GetNumTrack() { return static_cast<int>(ee_won_op_tracker_len(this->mTracker)); }
    bool IsEmpty() { return this->GetNumTrack()==0; }
    // the op tracked at inIndex in id order (NULL past the end)
    AsyncOp* GetTrackedOp(int inIndex)
    { return static_cast<AsyncOp*>(ee_won_op_tracker_at(this->mTracker,static_cast<size_t>(inIndex))); }
    AsyncOp* AddOp(AsyncOp* inOp, unsigned long inId); // original29fc80
    bool RemoveOp(AsyncOp* inOp);             // original29fe50
    void KillAll();                          // original29ffa0
private:
    AsyncOpTracker(const AsyncOpTracker&);
    AsyncOpTracker& operator=(const AsyncOpTracker&);
    EeWonOpTracker* mTracker;               // owned
};

// an op that tracks sub-ops
class AsyncOpWithTracker : public AsyncOp {
private:
    class TrackerCompletion; //nested16 owner, RTTI44c854 / 2a0100
    friend class TrackerCompletion;
    void Callback(AsyncOp*,int); //2a00d0 pending/remove/virtualcallback dispatcher
public:
    AsyncOp* Track(AsyncOp*,int); //2a0100, protected in the SDK; returns the tracker's AddOp result
    virtual void CleanupHook();             //slot08 CleanupHook2a0290
    virtual bool CallbackHook(AsyncOp* inOp,int inParam); //slot0c, bool/ret8
    AsyncOpTracker mTracker;                //44..78
};

class BlockingSocket;
// PeerAuthOp (0x2244 bytes) below: constructors, destructors and protocol are out of line.
// RTTI: PeerAuthOp > ServerOp > AsyncOpWithTracker > AsyncOp > RefCount.
// vtables: PeerAuthOp 44d4b0 (four slots), ServerOp 44c9e8 (four slots).
// AsyncOpWithTracker (0x78) must be complete before this point.
// ServerOp, 0x94 bytes: ctor 2acc10, implicit dtor 2accf0 (no own-vptr store).
class ServerOp : public AsyncOpWithTracker {
public:
    void SetMaxTimes(int,int,int); //2ace20
    BlockingSocket* GetSocket() { return this->mSocket.get(); } //WON SDK accessor, borrowed (no refcount change)
    void SetAuthType(AuthType theType,AuthContext* theContext=NULL) { this->mAuthType=theType; this->mAuthContext=theContext; }
    void SetLengthFieldSize(unsigned char theLengthFieldSize) { this->mLengthFieldSize=theLengthFieldSize; }
    unsigned char GetLengthFieldSize() { return this->mLengthFieldSize; }
protected:
    void SendMsgAsync(ByteBufferPtr,int inTrackId=0);
    void RecvMsgAsync(int inTrackId=0);
public:
    DWORD ConnectTime(); //2ace50
    DWORD SendTime(); //2ace80
    DWORD RecvTime(); //2aceb0
    void CopyMaxTimes(ServerOp*); //2acee0
    ServerOp(); //2acc10, constructor leaves mAuthType/mLengthFieldSize untouched
    // CallbackHook slot0c inherited from AsyncOpWithTracker; ServerOp has no override.
    SmartPtr<BlockingSocket> mSocket; //78
    SmartPtr<AuthContext> mAuthContext; //7c
    AuthType mAuthType; //80
    unsigned char mLengthFieldSize; //84
    unsigned char mClientKeySize; //85=8
    bool mUseClientKey; //86=false, natural alignment87
    void SetClientKeySize(unsigned char size,bool useClientKey) { this->mClientKeySize=size;this->mUseClientKey=useClientKey; }
    DWORD mMaxConnectTime; //88=60000
    DWORD mMaxSendTime; //8c=60000
    DWORD mMaxRecvTime; //90=60000
};

// An op of the Rust op base (rust/crates/ee_won/src/hostop.rs): its members and steps are the Rust op's
// (MultiPingOp: ee_won_multiping_new, FilePushOp: ee_won_filepush_new, FTPGetOp: ee_won_ftpget_new and _new_url;
// the ops derived from ServerRequestOp: NewRequest over reqhost.rs); this class runs its hooks there and answers
// its calls on the C++ objects (ServerOp.cpp's HostCall). Results and setters by number: ee_won_hostop_num,
// ee_won_hostop_bytes, ee_won_hostop_set.
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
    virtual void RunHook();
    virtual void CleanupHook();
    virtual bool CallbackHook(AsyncOp* inOp, int inParam);
    // the Rust op's calls (EeWonHostCall), and a completion this op made, with its id
    static void HostCall(void* inOp, EeWonHostCall* inCall);
    void Completed(AsyncOp* inOp, int inId) { ee_won_hostop_callback(this->mRustOp, inId, inOp); }
protected:
    virtual ~RustServerOp();
private:
    static void StrSink(void* inContext, const uint8_t* inData, size_t inLength);
    IPAddr AddrOf(int inWhich) const;
    EeWonHostOp* mRustOp; // owned
    FILE* mFile;          // the op's file (EE_WON_HOST_FILE_*)
};
typedef SmartPtr<RustServerOp> RustServerOpPtr;

// AuthSession (0x10cc bytes in the original: type, length field size, id, sequence numbers, the by-value Blowfish
// and the last use, constructor 2e27b0, dtor 2e2a80): a RefCount shim over ee_won's EeWonAuthSession
// (rust/crates/ee_won/src/peerauth.rs), which owns all of it.
class AuthSession : public RefCount {
protected:
    EeWonAuthSession* mImpl;
    virtual ~AuthSession(); //2e2a80
public:
    AuthSession(AuthType, unsigned short, Blowfish, unsigned char); //2e27b0/RET10c0
    explicit AuthSession(EeWonAuthSession* inImpl) : mImpl(inImpl) {} // adopts a session ee_won made
    WONStatus Encrypt(ByteBufferPtr&); //2e2b40
    WONStatus Decrypt(ByteBufferPtr&); //2e2d60
    time_t GetLastUseTime() const { return static_cast<time_t>(ee_won_auth_session_get(this->mImpl, 2)); }
    // the Rust session's key, laid out as Blowfish's one member
    const Blowfish& GetKey() const { return *reinterpret_cast<const Blowfish*>(ee_won_auth_session_key(this->mImpl)); }
    AuthType GetAuthType() const { return static_cast<AuthType>(ee_won_auth_session_get(this->mImpl, 0)); }
    unsigned short GetSessionId() const { return static_cast<unsigned short>(ee_won_auth_session_get(this->mImpl, 1)); }
    void SetLengthFieldSize(unsigned char size) { ee_won_auth_session_set_lfs(this->mImpl, size); }
    bool IsEncrypted() { AuthType t = this->GetAuthType(); return (t==AUTH_TYPE_SESSION || t==AUTH_TYPE_PERSISTENT); } //inline (WON SDK); same AuthSession+08 test as PeerAuthServerOp::Success
};
// CryptTransform: 8 bytes; new 8 at 2ad6d7, VT44ca1c, SDK CryptTransform.h.
// Holds a reference to the AuthSession only.
class CryptTransform : public MsgTransform {
    SmartPtr<AuthSession> mSession; //04, size08; implicit dtor2ad8e0 releases then MsgTransform dtor
public:
    CryptTransform(AuthSession* inSession) : mSession(inSession) {} //inline
    virtual WONStatus Send(ByteBufferPtr& inMessage) { return this->mSession->Encrypt(inMessage); } //2ad8b0
    virtual WONStatus Recv(ByteBufferPtr& inMessage) { return this->mSession->Decrypt(inMessage); } //2ad8a0
};


// The directory server's entity replies: the reading is ee_won's (dir.rs); a list of what it read as DirEntity
// objects (DirEntityReplyParser.cpp), for RustServerOp::GetEntities. GetEntityRequest, GetDirOp, GetMultiDirOp,
// GetAliasDirOp, FindEntityOp and GetServiceOp are Rust ops (rust/crates/ee_won/src/reqdir.rs).
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
    AuthSession* GetSession() const
    {
        return static_cast<AuthSession*>(reinterpret_cast<RefCount*>((intptr_t)this->GetNum(EE_WON_PEEROP_SESSION)));
    }
    WONStatus GetGetCertStatus() const { return (WONStatus)this->GetNum(EE_WON_PEEROP_GET_CERT_STATUS); }
};

// HTTP operation, 0x15c bytes.
class FileReader;
class FileWriter;
// Cache entry, 0x68 bytes; ctor 2d1260; implicit dtor 2d1330 (no own-vtable store).
template <class Cache, class Entry> struct HttpCacheMsg;
class HTTPCacheEntry : public RefCount {
    friend class HTTPCache;
    template <class Cache, class Entry> friend struct HttpCacheMsg;
    int mId; //08
    bool mIsValid; //0c,naturalpadding0d..0f
    std::string mRemoteLocation; //10
    std::string mLocalPath; //20
    std::string mFileName; //30
    std::string mFileType; //40
    std::string mEntityTag; //50
    time_t mExpirationTime; //60
    time_t mLastAccessTime; //64
public:
    HTTPCacheEntry(); //2d1260
    // ReadFromFile 2d1450 and WriteToFile 2d14c0 are ee_won's (cachemap.rs, through WONHttpCacheMsg.h)
    bool IsValid(); //2d1540, small getter, out of line
    void SetValid(bool inValid) {this->mIsValid=inValid;}
    void SetExpirationTime(time_t inTime) {this->mExpirationTime=inTime;}
    void SetEntityTag(const std::string& inTag) {this->mEntityTag=inTag;}
    const std::string& GetLocalPath() {return this->mLocalPath;}
    const std::string& GetEntityTag() {return this->mEntityTag;}
    time_t GetExpirationTime() {return this->mExpirationTime;}
};
// Cache, 0x34 bytes. No mCreatedDirectory boolean (added in a later SDK).
// Constructor 2d1670 and the singleton allocation at 2d192d give sizeof 0x34.
class HTTPCache : public RefCount {
protected:
    std::string mCachePath; //08
    typedef std::map<std::string,SmartPtr<HTTPCacheEntry> > CacheLocationMap;
    typedef std::map<int,SmartPtr<HTTPCacheEntry> > CacheIdMap;
    CacheLocationMap mCacheLocationMap; //18
    CacheIdMap mCacheIdMap; //24
    int mNextFreeId; //30
    void ReadCacheMap(); //2d1a10
    void WriteCacheMap(); //2d1ca0
    void DeleteCachedFile(HTTPCacheEntry*); // WriteCacheMap's deletion of a dropped entry's file
    template <class Cache, class Entry> friend struct HttpCacheMsg;
    int GetFreeId(); //2d2170
    virtual ~HTTPCache(); //2d1820 explicitownVT then WriteCacheMap
public:
    HTTPCache(const std::string&,bool inFullPath=false); //2d1670
    void SetCachePath(const std::string&,bool); //2d1ef0 always creates dir; empty path returns before read
    HTTPCacheEntry* GetEntry(const std::string&); //2d2210 rawborrowed ret4, mapsretain
    static HTTPCache* GetGlobalCache(); //2d1900, no-argument getter
    static void SetGlobalCache(HTTPCache*);
    void UpdateEntry(HTTPCacheEntry*); //2fa500, called with an empty version
};
// HTTPGetOp is a Rust op on the base (rust/crates/ee_won/src/httpget.rs): RustServerOp::NewHttpGet, its members by
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
// WON SDK static-only diagnostics API; no instance fields, bases, or virtual slots.
class WONDebug {
public:
    static void SetStackFileName(const std::string &theName);
    static const std::string& GetStackFileName();
    static int DumpStackToFile(void *theInfo = NULL);
};
class Thread : public ThreadBase {
public:
    void SetAutoDelete(bool inAutoDelete) { ee_won_thread_set_auto_delete(this->mThread,inAutoDelete); }
};
class DNSTestOp : public AsyncOp {
    friend class DNSTestThread;
public:
    DNSTestOp(const std::string& inHost);    //2d95a0
    // Accessor as in the SDK's DNSTestOp.h; the LAN family 3e28d0 reads
    // receiver+54 directly (no out-of-line helper).
    const IPAddr& GetHostAddr() { return this->mHostAddr; }
protected:
    virtual ~DNSTestOp();                   //2d9740 explicit/out of line
    virtual void RunHook();                 //2d97f0 RunHook,slot04
    std::string mHostString;                //44..54
    IPAddr mHostAddr;                       //54..7c
};
typedef SmartPtr<DNSTestOp> DNSTestOpPtr;
class DNSTestThread : public Thread {
public:
    DNSTestThread(DNSTestOp* inOp) : mOp(inOp) {} //inline constructor 2d9826..985d
    virtual void ThreadFunc(); //slot10 2d9920
    DNSTestOpPtr mOp; //1c,owned; full20. Implicit dtor2d98b0 no ownvptr store.
};


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

// GetHTTPDocumentOp (and GetMOTDOp, its MOTD) is a Rust op on the base (rust/crates/ee_won/src/httpdoc.rs):
// RustServerOp::NewHttpDocument, its members by EE_WON_HTTPDOC_* number and owner, GetHttpDocument

// WONAPI's CD key (dead in AoC; from the base game's Login: ctor base exe 0x68ab90, an 0x1c-byte local;
// implicit dtor 0x4cff2a: the SmartPtr at +0x14, the string at +4; provisional names)
// The full CDKey declaration comes earlier (EEWonNet), before AuthContext's mapped value.
// Storage, methods and inline virtual bodies live there; no duplicate class definition.


// AoC GetCertOp (0x11fc bytes in the original; RTTI GetCertOp > ServerRequestOp > ...): a Rust op on the base
// (ee_won certhost.rs over getcert.rs: its members, every step and the session key). The class is its constructor
// (the auth context and its server list, ServerOp.cpp) and the two accessors the game uses.
class AuthPeerData;
class GetCertOp : public RustServerOp {
public:
    explicit GetCertOp(AuthContext*); //2e3c30
    const AuthPeerData* GetPeerData() const; // borrowed; NULL before a login
    void SetCreateAccount(bool value) { this->SetNum(EE_WON_GETCERT_CREATE_ACCOUNT, value); }
};


// DBProxyOp is a Rust op base (rust/crates/ee_won/src/reqop.rs DbProxy)
// Patch-server records: VersionData 0x48 / PatchData 0xa4,
// constructors 2d83a0/2d86f0, allocations 2d93a7/2d9079.
enum VersionState { VersionState_Invalid=0, VersionState_Inactive=1, VersionState_Active=2, VersionState_BadState=3 };
typedef std::list<std::string> VersionTypeList;
class VersionData : public RefCount {
protected:
    virtual ~VersionData(); //2d8510, explicit ownVT; clears list then owns members
private:
    std::string mConfigName; //08
    std::string mVersion; //18
    std::string mDescriptionURL; //28
    VersionState mState; //38
    VersionTypeList mVersionTypeList; //3c..48
public:
    VersionData(); //2d83a0
    void WriteToBuffer(WriteBuffer&,DWORD inFlags=0); //2d8660
    enum { WriteFlag_AppendState=1, WriteFlag_AppendDescripURL=2 };
    void AddVersionType(const std::string&);
    void ReadFromBuffer(ReadBuffer&);
    bool operator==(const VersionData&) const;
    bool operator!=(const VersionData&) const;
    void SetDescriptionURL	(const std::string& theDescriptionURL)	{ this->mDescriptionURL	= theDescriptionURL;}
    const std::string& GetDescriptionURL()		const { return this->mDescriptionURL;	 }
    const VersionState GetState()				const { return this->mState;			 }
    const VersionTypeList& GetVersionTypeList() const { return this->mVersionTypeList; }
    void SetConfigName(const std::string& inName) { this->mConfigName=inName; }
    void SetVersion(const std::string& inVersion) { this->mVersion=inVersion; }
    void SetState(VersionState inState) { this->mState=inState; }
    const std::string& GetConfigName() const { return this->mConfigName; }
    const std::string& GetVersion() const { return this->mVersion; }
};
class PatchData : public RefCount {
protected:
    virtual ~PatchData(); //2d8a50, explicit own vtable store, then 9 string members
private:
    std::string mConfigName; //08
    std::string mFromVersion; //18
    std::string mToVersion; //28
    std::string mDisplayName; //38
    std::string mNetAddress; //48
    unsigned long mCheckSum; //58
    unsigned long mPatchSize; //5c
    std::string mHostName; //60
    std::string mHostURL; //70
    std::string mHostText; //80
    std::string mHostImageURL; //90
    bool mMustVisitHost; //a0
    bool mIsActive; //a1, naturalpaddinga2..a3
public:
    PatchData(); //2d86f0
    void ReadFromBuffer(ReadBuffer&,bool inGetPatchList=false); //2d8bc0
    void WriteToBuffer(WriteBuffer&,bool inUpdateOnly=false);
    bool operator==(const PatchData&) const;
    bool operator!=(const PatchData&) const;
    void SetDisplayName	(const std::string& theDisplayName)	{ this->mDisplayName = theDisplayName;	}
    void SetNetAddress	(const std::string& theNetAddress)	{ this->mNetAddress = theNetAddress;		}
    void SetCheckSum	(unsigned long theCheckSum)			{ this->mCheckSum = theCheckSum;			}
    void SetPatchSize	(unsigned long thePatchSize)		{ this->mPatchSize = thePatchSize;		}
    void SetHostName	(const std::string& theHostName)	{ this->mHostName = theHostName;			}
    void SetHostURL		(const std::string& theHostURL)		{ this->mHostURL = theHostURL;			}
    void SetHostText	(const std::string& theHostText)	{ this->mHostText = theHostText;			}
    void SetHostImageURL(const std::string& theHostImageURL){ this->mHostImageURL = theHostImageURL;	}
    void SetMustVisitHost(bool theMustVisitHost)			{ this->mMustVisitHost = theMustVisitHost;}
    const std::string&  GetConfigName()		const	{ return this->mConfigName;	}
    const std::string&  GetFromVersion()	const	{ return this->mFromVersion;	}
    const std::string&  GetToVersion()		const	{ return this->mToVersion;	}
    const std::string&  GetDisplayName()	const	{ return this->mDisplayName;	}
    const std::string&  GetNetAddress()		const	{ return this->mNetAddress;	}
    const unsigned long GetCheckSum()		const	{ return this->mCheckSum;		}
    const unsigned long GetPatchSize()		const	{ return this->mPatchSize;	}
    const std::string&  GetHostName()		const	{ return this->mHostName;		}
    const std::string&  GetHostURL()		const	{ return this->mHostURL;		}
    const std::string&  GetHostText()		const	{ return this->mHostText;		}
    const std::string&  GetHostImageURL()	const	{ return this->mHostImageURL; }
    bool  GetMustVisitHost()				const	{ return this->mMustVisitHost;}
    bool  GetIsActive()						const	{ return this->mIsActive;		}
    void SetConfigName(const std::string& inName) { this->mConfigName=inName; }
    void SetFromVersion(const std::string& inVersion) { this->mFromVersion=inVersion; }
    void SetToVersion(const std::string& inVersion) { this->mToVersion=inVersion; }
    void SetIsActive(bool inActive) { this->mIsActive=inActive; }
};
typedef std::list<SmartPtr<PatchData> > PatchDataList;
// CheckValidVersionOp is a Rust op (rust/crates/ee_won/src/reqops.rs)
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

// RequestPasswordEmailOp and RequestAccountInfoEmailOp are Rust ops (rust/crates/ee_won/src/adminop.rs)

// GetUserIdFromNameOp is a Rust op (rust/crates/ee_won/src/reqops.rs)


// GetAccountOp and UpdateAccountOp are Rust ops (rust/crates/ee_won/src/reqdir.rs), CreateAccountOp and
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
class SocketThread : public Thread {
public:
    SocketThread(); //2b60d0
    virtual ~SocketThread(); //2b6120, explicit own vtable 44cb10
    virtual void ThreadFunc(); //base slot10,2b6140
    virtual void AddSocketOp(SocketOp*) {} //new slot1c, 2fa500 (empty)
    virtual void RemoveSocketOp(SocketOp*) {} //20, version-empty2fa500
    virtual void Pump(unsigned long) {} //24, version-empty2fa500
    virtual void PurgeOps() {} //28, version-empty4142a0
    virtual bool NeedThread() {return true;} //2c, true2f30d0
    void SetSingleThreaded(bool value) { this->mSingleThreaded=value; } //inline bool 34 (WON SDK)
    static bool DoOp(SocketOp*); //2b6160, cdecl, the caller pops (Rust's: ee_won sockthread.rs)
    bool mSingleThreaded; //34; total38 (the original's lock at 1c is SocketThreadSimple's Rust object's)
};
// SocketThreadSimple: a Rust object since round 13 (rust/crates/ee_won/src/sockthread.rs EeWonSocketThread: the ops
// held, a reference each, in address order as the original's std::set of SmartPtrs; the lock; DoOp, the pump, the
// purge, the adds and removes) behind this shim; the ops' steps come back through SocketThread.cpp's hook table. The
// thread is the Thread shim's; ThreadFunc pumps the object, then waits on the thread's signal
class SocketThreadSimple : public SocketThread {
public:
    SocketThreadSimple();
    virtual ~SocketThreadSimple(); //2b61a0, explicit ownVT44cb44 and PurgeOps
    virtual void ThreadFunc(); //2b6890
    virtual void AddSocketOp(SocketOp*); //2b64f0
    virtual void RemoveSocketOp(SocketOp*); //2b6660
    virtual void Pump(unsigned long); //2b6760
    virtual void PurgeOps(); //2b6380
private:
    EeWonSocketThread* mOps; //owned
};

// A Rust object since net round 16 (rust/crates/ee_won/src/servconn.rs EeWonServerConn: the auth context +0xa0 and
// type +0xa4, the peer authentication op +0xa8, the connect completion +0xac, the keep-alive timer +0xb0 and period
// +0xb4, and every step of ServerConnection.cpp); this class is the shim RoutingConnection and the lobby derive from
class ServerConnection : public BlockingSocket {
public:
    void StartKeepAlive(); //2aaf10
    static void StaticConnectCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab160
    static void StaticMsgCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab6a0
    static void StaticKeepAliveCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab7b0
    void KeepAliveCallback(AsyncOp*); //2ab840
    void SendNoOp(); //2ab910
    ServerConnection(); //2aab10
    virtual ~ServerConnection(); // the Rust object (the original's was implicit, 2aabe0)
    virtual void CloseHook(); //CloseHook2abe70
    virtual void KillHook();  //KillHook2abf50
    void ConnectCallback(PeerAuthOp*); //ConnectCallback2ab1f0 (virtual in the SDK; nothing overrides it)
    virtual void MsgCallback(const ByteBuffer*); //MsgCallback2fa500,ret4
    void SetAuth(AuthContext*, AuthType);  //SetAuth
    PeerAuthOp* ConnectAsync(const IPAddr&, unsigned long); //2abc40; raw borrowed member result
    WONStatus ConnectBlock(const IPAddr&,DWORD inTimeout=0xffffffff);
    void SetKeepAlivePeriod(DWORD);
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>*); //2abdc0
private:
    ServerConnection(const ServerConnection&);
    ServerConnection& operator=(const ServerConnection&);
    EeWonServerConn* mConn;
};

class ServerConnectionConnectOp : public AsyncOp {
public:
    static void StaticConnectCompletion(AsyncOpPtr,SmartPtr<RefCount>); //2ac240
    void ConnectCompletion(AsyncOp*); //2ac2d0
    ServerConnectionConnectOp(ServerConnection*, const IPAddr&); //2abfc0/ret8
    virtual void RunHook();   //2ac2f0 RunHook
    virtual void CleanupHook(); //2ac380 CleanupHook
    // Destructor stays implicit: ordinary2ac1a0 has NOown-vptr store.
    SmartPtr<ServerConnection> mConnection; //44
    IPAddr mAddr; //48..6f, sizeof70, IPAddr 0x28
};

// The op list, the completions and the reply dispatch are a Rust object since net round 16
// (rust/crates/ee_won/src/routeconn.rs EeWonRouteConn: the list +0xb8, the completions +0xc8 and the default one
// +0x1bc, and AddOp 2ba630, CalcCompletion 2ba740, GetUnsolicitedOp 2ba830, MsgCallback 2bc850, CloseHook 2bcb20,
// KillHook 2bcbb0); each request helper below makes a RoutingOp (a Rust op since net round 18) and runs it
class RoutingConnection : public ServerConnection {
public:
    void AddOp(RoutingOp*); //2ba630, AutoCrit/list ownership
    RoutingConnection(); //2ba050; allocation1c0 at3eddd4
    // the completion an op of a type (RoutingOpType, below 61) gets when it has none, and the default one
    void SetOpCompletion(int inType, CompletionBase<AsyncOpPtr>* inCompletion);
    void SetDefaultOpCompletion(CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bf1d0: subscribes to the data objects of a type (EEWonNet's room and game objects)
    void SubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag,
                        CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bf2e0
    void UnsubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bcc30: registers the client
    void Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inReconnectId,
                  unsigned long inAsyncFlags, unsigned long inRegisterFlags, unsigned long inFlags,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bd430: creates a game group
    void CreateGroup(unsigned short inParent, const std::wstring& inName, const std::wstring& inPassword,
                     unsigned short inMaxPlayers, unsigned long inGroupFlags, unsigned char inFlag, unsigned long inArg7,
                     CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2becd0: creates a data object
    void CreateDataObject(unsigned char inFlag, unsigned short inLinkId, const std::string& inDataType,
                          const std::wstring& inName, const ByteBuffer* inData,
                          CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bee30: replaces a data object
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                           CompletionBase<AsyncOpPtr>* inCompletion);
    void GetGroupList(unsigned char inFlags, CompletionBase<AsyncOpPtr>* inCompletion);       // 0x2bcf90
    // 0x2c35a0: the client of a name
    // 0x2be540: chat to a group
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inGroup,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2be6a0: chat to clients
    void SendChatTo(const std::wstring& inText, unsigned short inFlags, const std::list<unsigned short>& inClients,
                    CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bef50: replaces a data object's data
    void ModifyDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                          unsigned short inOffset, bool inFlag, const ByteBuffer* inData,
                          CompletionBase<AsyncOpPtr>* inCompletion);
    // 0x2bd580: joins a group
    void JoinGroup(unsigned short inId, const std::wstring& inPassword, const std::wstring& inComment, unsigned char inFlag4,
                   unsigned char inFlag5, CompletionBase<AsyncOpPtr>* inCompletion);
    void CancelJoinGroup(unsigned short inId, bool inFlag2, CompletionBase<AsyncOpPtr>* inCompletion);      // 0x2bd6b0
    void LeaveGroup(unsigned short inId, CompletionBase<AsyncOpPtr>* inCompletion);              // 0x2bd850
    // 0x2bf0c0: subscribes to a group's data objects
    void ReadDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag,
                  CompletionBase<AsyncOpPtr>* inCompletion);
    void GetClientList(CompletionBase<AsyncOpPtr>* inCompletion);  // 0x2bcd40
    void GetMembersOfGroup(unsigned short inId, CompletionBase<AsyncOpPtr>* inCompletion);    // 0x2bd1e0


// Members of RoutingConnection (0x1c0).
// All return void.
void SetClientFlags(unsigned long clientMask, unsigned long clientFlags,
    unsigned long asyncMask, unsigned long asyncFlags,
    CompletionBase<AsyncOpPtr>* completion); //2be390 ret14
void SetGroupFlags(unsigned short groupId, unsigned long groupMask,
    unsigned long groupFlags, unsigned long asyncMask, unsigned long asyncFlags,
    CompletionBase<AsyncOpPtr>* completion); //2bdc10 ret18
void SendData(const ByteBuffer* data, unsigned char flags, unsigned short recipient,
    CompletionBase<AsyncOpPtr>* completion); //2be910 ret10; flags lowbyte in2bea70
void SendData(const ByteBuffer* data,unsigned char flags,const std::list<unsigned short>& recipients,
    CompletionBase<AsyncOpPtr>* completion); //2bea70 ret10; SDK overload
void BanClient(unsigned short groupId, bool isBanned, unsigned long seconds,
    const std::wstring& explanation, unsigned short clientId,
    CompletionBase<AsyncOpPtr>* completion); //2bf590 ret18; SDKBanClientByClientId
void AcceptClient(unsigned short groupId, unsigned short clientId, bool accepted,
    const std::wstring& explanation,
    CompletionBase<AsyncOpPtr>* completion); //2bd9f0 ret14
void InviteClient(unsigned short groupId, unsigned short clientId, bool invited,
    const std::wstring& explanation,
    CompletionBase<AsyncOpPtr>* completion); //2bdb00 ret14
void BanClientByCDKeyId(unsigned short groupId,bool on,unsigned long seconds,
    const std::wstring& explanation,unsigned long cdKeyId,
    CompletionBase<AsyncOpPtr>* completion); //dispatch (WON SDK); no original address identified
void BanClientByWONId(unsigned short groupId, bool on, unsigned long seconds,
    const std::wstring& explanation, unsigned long wonId,
    CompletionBase<AsyncOpPtr>* completion); //2bf6b0 ret18
void MuteClientByWONId(unsigned short groupId, bool on, unsigned long seconds,
    const std::wstring& explanation, unsigned long wonId,
    CompletionBase<AsyncOpPtr>* completion); //2bf8f0 ret18
void BecomeModerator(bool moderatorOn,
    CompletionBase<AsyncOpPtr>* completion); //2bf3f0 ret8
void MuteClient(unsigned short groupId, bool isMuted, unsigned long seconds,
    const std::wstring& explanation, unsigned short clientId,
    CompletionBase<AsyncOpPtr>* completion); //2bf7d0 ret18; SDKMuteClientByClientId
void SendServerAlert(const std::wstring& text,
    CompletionBase<AsyncOpPtr>* completion); //2bddd0 ret8
void StartServerShutdown(const std::wstring& text, unsigned long seconds,
    CompletionBase<AsyncOpPtr>* completion); //2be010 ret0c
void AbortServerShutdown(CompletionBase<AsyncOpPtr>* completion); //2be200 ret4

// Further WON SDK operation functions; no original address identified.
    void DisconnectClient(CompletionBase<AsyncOpPtr> *theCompletion);
    void GetClientCDKeyIdList(CompletionBase<AsyncOpPtr> *theCompletion, bool bDoSubscribe);
    void GetClientListEx(CompletionBase<AsyncOpPtr> *theCompletion);
    void SetGroupName(unsigned short theGroupId, const std::wstring &theName, CompletionBase<AsyncOpPtr> *theCompletion);
    void SetGroupMaxPlayers(unsigned short theGroupId, unsigned short theMaxPlayers, CompletionBase<AsyncOpPtr> *theCompletion);
    void RelinquishCaptaincy(unsigned short theGroupId, unsigned short theNewCaptainId, CompletionBase<AsyncOpPtr> *theCompletion);
    void SendServerAlertWithRecipientList(const std::wstring &theAlertText, std::list<unsigned short>& recipientList, CompletionBase<AsyncOpPtr> *theCompletion);
    void DetectFirewall(unsigned short theListenPort, unsigned long theMaxConnectSeconds, bool doListen, CompletionBase<AsyncOpPtr> *theCompletion);
    void MuteClientByCDKeyId(unsigned short theGroupId, bool isMuted, unsigned long theSeconds, 
	const std::wstring &theComment, unsigned long theCDKeyId, CompletionBase<AsyncOpPtr> *theCompletion);
    void GetBadUserList(RoutingBadUserListType theListType, unsigned short theGroupId, unsigned char theFlags,
									   CompletionBase<AsyncOpPtr> *theCompletion);


    virtual void CloseHook(); //CloseHook2bcb20
    virtual void KillHook();  //KillHook2bcbb0
    virtual void MsgCallback(const ByteBuffer*); //MsgCallback2bc850, ret4
    // (the record factories GetNewClientInfo 2ba100, GetNewMemberInfo 2ba230 and GetNewGroupInfo 2ba300 are gone:
    // the ops' records are Rust's since net round 18)
    virtual SmartPtr<RoutingOp> GetUnsolicitedDerivedOp(unsigned char); //GetUnsolicitedDerivedOp2bc6c0, sret+arg ret8
    virtual void RoutingOpCompleteHook(RoutingOp*) {} //inline RoutingOpCompleteHook (WON SDK), shared 30a800 ret4
    // 04/10/14 are inherited unchanged; no generic vtable padding or aliases.
    bool mAllDerivedOpsAreUnsolicited; //c4 (GetUnsolicitedDerivedOp reads it)
protected:
    virtual ~RoutingConnection(); //ordinary2ba4b0; explicit own-vptr store
private:
    RoutingConnection(const RoutingConnection&);
    RoutingConnection& operator=(const RoutingConnection&);
    EeWonRouteConn* mRoute; // owned
};

// The client, group and member model, the ignore list, the chat commands and the snapshots are a Rust object since
// net round 17 (rust/crates/ee_won/src/smartroute.rs EeWonSmartRoute: the maps +0x1c0..0x1d7, the iterators, the
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
    virtual void CloseHook(); //CloseHook2c4120
    virtual void RoutingOpCompleteHook(RoutingOp*); //2c0be0
    bool GetClient(unsigned short,ClientInfo&);
    bool GetGroup(unsigned short,GroupInfo&);
    bool GetMember(unsigned short,unsigned short,MemberInfo&);
    void StartClientItr();
    bool GetNextClient(ClientInfo&);
    void StartGroupItr();
    bool GetNextGroup(GroupInfo&);
    bool StartMemberItr(unsigned short);
    bool GetNextMember(MemberInfo&);
    bool GetLastTargetMember(MemberInfo&);
    bool GetLastTargetClient(ClientInfo&);
    // 2c35a0: the client a typed name refers to (exact, unique prefix, ambiguous or not found); outFound when the
    // search set one (the original's out pointer)
    MatchResult FindClient(const std::wstring&, ClientInfo&, bool& outFound);
    void IgnoreClient(unsigned short inClientId, bool inOn); //2c37d0, the client GetClient(id) finds
    void AddCommandString(const std::wstring&, InputCommand); //2c3970
    void ClearCommandStrings();
    void SetAllowPartialNameMatch(bool); //2c40a0
    void SetServerWideCommands(bool); //2c40f0
    void SetDoColonEmote(bool);
    bool HandleInput(const std::wstring&,int=-1);
    void SetProductName(const std::string&); //2c2fc0, the ignore file's path (provisional name)
    void ClearProductName() { this->SetProductName(""); }
    unsigned short GetMyClientId(); // OnChat 0x4d040c
    EeWonSmartRoute* mModel; // owned
protected:
    virtual ~SmartRoutingConnection();
private:
    SmartRoutingConnection(const SmartRoutingConnection&);
    SmartRoutingConnection& operator=(const SmartRoutingConnection&);
};

// No class here duplicates the old flattened RoutingConnection (0x258).
// No zero-valued gap fields or placement/partial object lifetime is introduced.

typedef SmartPtr<RoutingConnection> RoutingConnectionPtr;
typedef SmartPtr<SmartRoutingConnection> SmartRoutingConnectionPtr;


}

#endif
