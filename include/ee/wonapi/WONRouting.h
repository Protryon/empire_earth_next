// WONAPI routing-server API as the game's WON front end (EEWonNet.cpp, 0xcd4ee..0xd3ff4) uses it. Class names
// come from RTTI (.?AVRoutingConnection@WONAPI@@, ...), members and offsets from EEWonNet.cpp's use; methods are
// named by address where the WONAPI name is unknown (provisional names). Requires <string>, <list>, <map>,
// ee/EEWonNet.h.
#ifndef EE_WONAPI_WONROUTING_H
#define EE_WONAPI_WONROUTING_H

#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONAuthPeerData.h" //complete type needed by LightGetCertOp's inline setter
#include "ee/wonapi/WONStringCompare.h"
#include <stdio.h> //FILE* member (RecvBytesOp::mFile)
#include <set> //SmartRoutingConnection IgnoreSet
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
    const IPAddr& GetDestAddr(); //2b08e0, lea28/ret; SDK nonconst method
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
    // the lock the subclasses (QueueSocket, the routing and server connections) take around their own state too
    CriticalSection& GetDataCrit() { return this->mDataCrit; }
    void SetDestAddr(const IPAddr& inAddr) { this->mDestAddr=inAddr; }
    void SetThreadCloseCallback(CloseCallback inCallback);
    WONStatus GetSocketStatus() const; // the last socket error recorded (the original's +0x54)
    EeWonAsyncSocket* GetSocketObject() const { return this->mSock; }
private:
    AsyncSocket(const AsyncSocket&);
    AsyncSocket& operator=(const AsyncSocket&);
    CriticalSection mDataCrit; //08..1f; its mutex is the Rust object's lock
    // A Rust object since round 14 (rust/crates/ee_won/src/sockobj.rs EeWonAsyncSocket): the handle, type, flags,
    // status and times (the original's +0x20..+0x5c), the length field size (+0x50), the message transforms (+0x60,
    // owned) and the socket thread's close callback (+0x6c), and every step of AsyncSocket.cpp
    EeWonAsyncSocket* mSock;
    IPAddr mDestAddr; //28..4f, complete28 (a C++ IPAddr until IPAddr is Rust's)
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
class AcceptOp : public SocketOp {
public:
    AcceptOp(AsyncSocket* inSocket) : SocketOp(inSocket) { this->SetWaitForEvent(0,true); }
    AsyncSocket* GetAcceptedSocket() { return this->mAcceptedSocket.get(); }
    virtual WONStatus Continue();           // +0x10 0x2b0310 (WONAPI)
    virtual AsyncOp* Duplicate() { return new AcceptOp(this->GetSocket()); }  // +0x14 0x291ab7

private:
    SmartPtr<AsyncSocket> mAcceptedSocket;  // +0x5c
};

// connects its socket to an address (RTTI ConnectOp > SocketOp, vtable 0x84c480; 0x84 bytes: EELobbyFileTransfer
// 0x291b79 news it and inlines the ctor: SocketOp(NULL), the IPAddr copy ctor 0x291d59 at +0x5c, the vptr, then
// +0x59 and +0x5a = true; argument order inferred)
class ConnectOp : public SocketOp {
public:
    ConnectOp(const IPAddr& inAddr, AsyncSocket* inSocket = NULL) : SocketOp(inSocket), mAddr(inAddr)
    {
        this->SetWaitForEvent(1,true);
        this->SetWaitForEvent(2,true);
    }
    virtual WONStatus Start();              // +0x0c 0x2b2320 (WONAPI)
    virtual WONStatus Continue();           // +0x10 0x2b2330 (WONAPI)

    const IPAddr& GetAddr() const { return this->mAddr; }
private:
    IPAddr mAddr;                           // +0x5c
};

// receives bytes from its socket into a buffer or a file
class RecvBytesOp : public SocketOp {
public:
    WONStatus StartRecv(); //2b5000
    WONStatus ContinueRecv(); //2b50c0
    WONStatus CloseFile(WONStatus); //2b5410, finishes the file
    WONStatus FinishRecv(WONStatus); //2b54a0, finishes the receive

    RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket = NULL); //2b4de0
    // Implicit dtor2ab5a0 must naturally inline in RecvMsgOp dtor2ab470.
    virtual void CleanupHook(); //2b5580
    virtual WONStatus Start();     //2b4ee0
    virtual WONStatus Continue();  //2b4ef0
    virtual AsyncOp* Duplicate(); //2b4f00
    void SetProgressCompletion(CompletionBase<AsyncOpPtr>*, long inInterval);
    ByteBufferPtr GetBytes() const; //2b1df0, owning return by value
    void SetFilePath(const std::string& theFilePath) { this->mPath=theFilePath; }
    void SetAppendFile(bool theVal) { this->mAppend=theVal; }
    void SetBinaryFile(bool isBinary) { this->mBinary=isBinary; }
    void SetFileModifyTime(time_t theTime) { this->mFileModifyTime=theTime; }
    void SetSkipBytes(bool skip) { this->mSkipBytes=skip; }
    const std::string& GetFilePath() const { return this->mPath; }
    unsigned long GetBytesRead() const { return this->mBytesRead; }
private:
    friend class RecvMsgOp; // its length prefix and body steps
    WriteBuffer mWriteBuffer; //5c,18
    ByteBufferPtr mReceivedData; //74
    unsigned long mLength; //78
    unsigned long mBytesRead; //7c
    bool mSkipBytes; //80 (inferred)
    FILE* mFile; //84, natural alignment before pointer
    std::string mPath; //88
    bool mAppend; //98
    bool mOwnFile; //99
    bool mBinary; //9a
    long mFileModifyTime; //9c, 32-bit time_t, natural alignment before scalar
    SmartPtr<CompletionBase<AsyncOpPtr> > mProgressCompletion; //a0
    long mProgressInterval; //a4
    unsigned long mBytesSinceProgress; //a8
};
class RecvMsgOp : public RecvBytesOp {
public:
    WONStatus StartRecvBody(); //2b5b50
    WONStatus FinishRecvMsg(); //2b5d20

    RecvMsgOp(AsyncSocket* inSocket = NULL) : RecvBytesOp(0, inSocket) {}
    // Implicit dtor2ab470; no own-vptr store, full base cleanup inlined.
    virtual WONStatus Start();     //2b5b00
    virtual WONStatus Continue();  //2b5e20
    virtual AsyncOp* Duplicate(); //2ab3d0
    ByteBufferPtr GetMsg() const; //2ab770, owning return by value
private:
    ByteBufferPtr mMessage; //ac
    int mState; //b0, ctor uninitialized, RunHook sets1
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
// LightGetCertOp: 0x48 bytes, AsyncOp (0x44) plus the peer data pointer at 0x44
// PeerAuthOp::AsyncRefreshCert 2e74a0 allocates 0x48 bytes and calls the AsyncOp constructor
// Implicit ctor/dtor (2e74e0..2e74f2, 2e7580: owned pointer, then base cleanup)

class LightGetCertOp : public AsyncOp {
    ConstSmartPtr<AuthPeerData> mPeerData; //44..48, set by GetCertCallback 2de677
public:
    void SetPeerData(const AuthPeerData* data) { this->mPeerData=data; }
    const AuthPeerData* GetPeerData() const { return this->mPeerData; }
};

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

// TD4d4198, VT44c9bc, total6c. Constructor body3e0e80 is ordinary
// compiler-emitted inline support. Implicit destructor2abad0 has no own-vptr store.
class SendBytesOp : public SocketOp {
public:
    // SendMsgOp passes NULL (its Start sets the data). ByteBuffer's `this ? ...` getters are undefined behaviour for
    // NULL, which clang folds away (the clang-built op crashed here), so the test is made here
    SendBytesOp(const ByteBuffer* inData, AsyncSocket* inSocket = NULL)
        : SocketOp(inSocket),
          mReadBuffer(inData != NULL ? inData->data() : NULL, inData != NULL ? inData->length() : 0),
          mBuffer(inData)
    { this->SetWaitForEvent(1,true); }
    virtual WONStatus Start();     //2b5e50
    virtual WONStatus Continue();  //2b5e70
private:
    friend class SendMsgOp; // its transformed message's send
    ReadBuffer mReadBuffer; //5c, 0x0c bytes
    ByteBufferPtr mBuffer; //68
};

// TD4d41c0, VT44c9a0, total70. Implicit destructor2abb80 has no own-vptr store.
class SendMsgOp : public SendBytesOp {
public:
    SendMsgOp(const ByteBuffer* inMessage, AsyncSocket* inSocket = NULL)
        : SendBytesOp(NULL, inSocket), mMessage(inMessage) {}
    virtual WONStatus Start();     //2b5fa0
    virtual WONStatus Continue();  //2b6020
private:
    ByteBufferPtr mMessage; //6c
};

// TD4d4040, VT44c948, total88. Allocation and inline ctor at 3e469e..4793.
// Implicit destructor2aa980 destroys IPAddr60, buffer5c, socket54, AsyncOp.
class SendBytesToOp : public SocketOp {
public:
    SendBytesToOp(const ByteBuffer* inData, const IPAddr& inAddress,
                  AsyncSocket* inSocket = NULL)
        : SocketOp(inSocket), mBuffer(inData), mAddress(inAddress)
    { this->SetWaitForEvent(1,true); }
    virtual WONStatus Continue();  //2b5f10
private:
    ByteBufferPtr mBuffer; //5c
    IPAddr mAddress; //60, 0x28 bytes
};

// TD4d4440, VT44caf4, total90. Original out-of-line constructor2b55a0.
// Explicit destructor 2b56f0 stores the own vtable and deletes the raw array at 5c before the owned members.
class RecvBytesFromOp : public SocketOp {
public:
    RecvBytesFromOp(unsigned short inMaxLength, AsyncSocket* inSocket = NULL);
    virtual ~RecvBytesFromOp(); //2b56f0, explicit: own vtable store, raw-array delete before the owned members
    virtual void RunHook();   //2b57d0
    virtual WONStatus Continue();  //2b5820
    virtual AsyncOp* Duplicate(); //2b5660, fresh allocation of the same class
    SmartPtr<ByteBuffer> GetBytes() const; //2aa920, owning51B sret/AddRef, field60
    const IPAddr& GetAddr() const { return this->mFromAddress; }
private:
    char* mRawBuffer; //5c, owned array: allocate/transfer/clear; the explicit destructor deletes the rest
    SmartPtr<ByteBuffer> mReceivedData; //60, initializednull
    unsigned short mMaxLength; //64, first constructor argument
    IPAddr mFromAddress; //68, natural two-byte alignment before this member
};

// AoC UDP records: four slots each; the archived UDP header's extra
// RetransmitHook is a version difference and MUST NOT be inserted here.
class UDPManager;
class UDPManagerRecvOp : public AsyncOp {
public:
    UDPManagerRecvOp(); //2b68b0, default, 91 bytes
    UDPManagerRecvOp(const IPAddr&); //2b6a10
    bool HasVerifier();
    void SetUDPManager(UDPManager* theManager);
    void SetSocket(QueueSocket* theSocket);
    const IPAddr& GetFromAddr();
    void SetFromAddr(const IPAddr& theAddr);
    const ByteBuffer* GetMsg() { return this->mReceivedData.get(); }
    void SetMsg(const ByteBuffer* theMsg) { this->mReceivedData=theMsg; }
    // Implicit destructor2b6930, no own-vptr store; natural owned-member cleanup.
    virtual void RunHook();   //2b6fb0
    virtual void CleanupHook(); //2b6f60
    virtual WONStatus CheckMessage(ReadBuffer&); //2b6c10, new slot 0c
    SmartPtr<QueueSocket> mSocket; //44
    SmartPtr<UDPManager> mManager; //48
    bool mOwnSocket; //4c; the ctor leaves it unset until Run/setup
    IPAddr mAddress; //50..77, naturalalignment4d..4f
    ByteBufferPtr mReceivedData; //78
    typedef WONStatus (*MessageVerifier)(ReadBuffer&, void*);
    MessageVerifier mVerifier; //7c, initializedNULL
    void* mVerifierContext; //80; originalctor leaves until setter
    void SetMessageVerifier(MessageVerifier,void*); //2b6bf0
    void QueueRecv(); //2b6c30
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b6d70, owning values
    void RecvCallback(RecvBytesFromOp*); //2b6e60

};

class UDPManagerSendRecvOp : public AsyncOp {
public:
    static WONStatus StaticCheckResponse(ReadBuffer&,void*); //2b7250
    void SendRequest(); //2b7260
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b7550, owning values
    void RecvCallback(AsyncOp*); //2b75e0

    UDPManagerSendRecvOp(); //2b70b0
    void SetUDPManager(UDPManager* theManager);
    void SetMsgTimeout(DWORD theTimeout);
    void SetNumRetransmits(int theNum);
    void SetDestAddr(const IPAddr& theAddr);
    const IPAddr& GetDestAddr() { return this->mAddress; }
    // Implicit destructor2b7140, no own-vptr; naturally inlined into globalPingGameOp.
    virtual void RunHook();   //2b7470
    virtual void CleanupHook(); //2b7630
    virtual WONStatus CheckResponse(ReadBuffer&); //2ad870, returns status 0 by default (ret4)
    SmartPtr<UDPManager> mManager; //44
    SmartPtr<QueueSocket> mSocket; //48
    ByteBufferPtr mOutgoing; //4c
    IPAddr mAddress; //50..77
    SmartPtr<UDPManagerRecvOp> mRecvOp; //78, NOT a ByteBuffer
    int mRetryLimit; //7c, signed compare; int as in the SDK
    int mAttemptCount; //80, RunHook clears it; the ctor leaves it unset
    unsigned long mReceiveTimeout; //84, initialized to 10000; DWORD in the SDK
};

class UDPManager : public RefCount {
public:
    WONStatus Bind(int,bool);     //2b79c0, signed int port as in the SDK
    static void StaticRecvCallback(AsyncOpPtr,SmartPtr<RefCount>); //2b8230, owning values
    void RecvCallback(RecvBytesFromOp*); //2b8320, receive callback

    UDPManager(); //2b76b0
    void SetDefaultCompletion(CompletionBase<AsyncOpPtr>* theCompletion) { this->mDefaultCompletion=theCompletion; }
    void Send(const ByteBuffer*, const IPAddr&); //2b7ec0, void/ret8 (provisional name; the SDK has SendBytesTo)
    void Close(); //2b78e0
    void SetAutoRecv(bool); //2b7b40
    void QueueOp(UDPManagerRecvOp*,unsigned long); //2b7c50
    void CleanOps(); //2b8000
    // Implicit virtual destructor; 2b77b0 has no own-derived-vptr store.
    typedef std::list<SmartPtr<UDPManagerRecvOp> > OpList;
    typedef std::multimap<IPAddr, SmartPtr<UDPManagerRecvOp> > OpMap;
    void Erase(const OpList::iterator&); //2b8160, iterator-preserving erase from the clean list
    void Erase(const OpMap::iterator&); //2b81b0, iterator-preserving erase from the clean map
    SmartPtr<QueueSocket> mSocket; //08
    SmartPtr<CompletionBase<AsyncOpPtr> > mDefaultCompletion; //0c
    SmartPtr<UDPManagerRecvOp> mAutoRecvOp; //10
    OpList::iterator mCleanListItr; //14, initialized mOpList.begin()
    OpMap::iterator mCleanMapItr; //18, initialized mOpMap.begin()
    OpList mOpList; //1c, list node 0x0c with the pointer at +08
    OpMap mOpMap; //28, map node 0x3c: IPAddr key 0c, pointer 34, tree flags 38/39
    int mMaxRecvBytes; //34, initialized1024
    bool mAutoRecv; //38, initializedfalse; naturalpadding39..3b
};

// WON SDK inline UDP functions; defined after UDPManager is complete (typed SmartPtr)
inline bool UDPManagerRecvOp::HasVerifier() { return this->mVerifier!=NULL; }
inline void UDPManagerRecvOp::SetUDPManager(UDPManager* theManager) { this->mManager=theManager; }
inline void UDPManagerRecvOp::SetSocket(QueueSocket* theSocket) { this->mSocket=theSocket; this->mOwnSocket=false; }
inline const IPAddr& UDPManagerRecvOp::GetFromAddr() { return this->mAddress; }
inline void UDPManagerRecvOp::SetFromAddr(const IPAddr& theAddr) { this->mAddress=theAddr; }
inline void UDPManagerSendRecvOp::SetUDPManager(UDPManager* theManager) { this->mManager=theManager; }
inline void UDPManagerSendRecvOp::SetMsgTimeout(DWORD theTimeout) { this->mReceiveTimeout=theTimeout; }
inline void UDPManagerSendRecvOp::SetNumRetransmits(int theNum) { this->mRetryLimit=theNum; }
inline void UDPManagerSendRecvOp::SetDestAddr(const IPAddr& theAddr) { this->mAddress=theAddr; }

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
class HTTPGetOp;
class HTTPSession;
class HTTPGetOp;
class HTTPConnection : public RefCount {
public:
    HTTPConnection(HTTPSession* inSession = NULL); //2d2c80,new64 at2d2a97
    // Implicit ordinary destructor 2d2d50 (150 bytes), no own-vptr store; one-slot vtable 44d3a4.
    void Kill(); //2d3600
    void QueueOp(HTTPGetOp*); //2d33a0,ret4
    void QueueOp(HTTPGetOp*,int); //2d3290,ret8
    void RunOp(int); //2d3430,ret4
    int GetNumOps();
    static void StaticOpCallback(AsyncOpPtr,SmartPtr<RefCount>); //2d2df0, two owning by-value args
protected:
    void OpCallback(HTTPGetOp*); //2d2e80, 1030 bytes; protected in the SDK
public:
    CriticalSection mDataCrit; //08..20
    HTTPSession* mSession; //20 NONOWNING, ctor raw assignment
    SmartPtr<BlockingSocket> mSocket; //24
    int mNumPipelineFailures; //28
    int mNumPipelineSuccesses; //2c
    bool mDoPipeline; //30; natural alignment31..33
    time_t mLastUseTime; //34, constructor leaves uninitialized
    typedef std::list<SmartPtr<HTTPGetOp> > OpQueue;
    OpQueue mOpQueue[2]; //38,44
    bool mRetryMode; //50; natural alignment51..53
    HTTPGetOp* mCurOp[2]; //54,58 NONOWNING, Kill nulls them after killing
    SmartPtr<CompletionBase<AsyncOpPtr> > mCurOpCompletion[2]; //5c,60
};
typedef SmartPtr<HTTPConnection> HTTPConnectionPtr;

// 0x2c bytes: constructor 2d27b0, map destruction 2d2870.
// Implicit dtor:2d2870 has no own vptr store; final store is RefCount at2d28d0.
// SDK HTTPEngine.h independently has no explicit destructor declaration.
class HTTPSession : public RefCount {
public:
    HTTPSession();                          //2d27b0, out of line
    void AddOp(HTTPGetOp*);                  //2d28f0, queues a completed op
    void KillAll();                         //2d2bc0
    int GetNumOps();
    CriticalSection mDataCrit;              //08..20
    typedef std::map<std::string,HTTPConnectionPtr,StringLessNoCase> ConnectionMap;
    ConnectionMap mConnectionMap;           //20..2c
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

// SmartRoutingClientInfo: 0x30 bytes, held by pointer; factory 2bfe60 allocates 0x30.
// Destructor stays implicit.
struct SmartRoutingClientInfo : public RoutingClientInfo {
    SmartRoutingClientInfo() : RoutingClientInfo(),mNumGroups(0),mIgnored(false) {}
    unsigned short mNumGroups; //2c
    bool mIgnored; //2e, naturalalignment2f
};
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

// RoutingOp base and the known prefixes of its derived ops.
// Partial derived operations are NEVER allocated, sized, constructed, destroyed or emitted as RTTI/vtables here.
class RoutingOp : public AsyncOp {
public:
    RoutingOp();
    RoutingOp(RoutingConnection*);
    // Destructor remains implicit; do not force a separate out-of-line body.
    virtual void RunHook();   //RunHook2c7d20
    // Virtual08 CleanupHook inherited from AsyncOp; no extra slot.
    virtual void SendRequest(); //SendRequest default326b60, void()
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //HandleReply2bcd30 ret8
    virtual RoutingOpType GetType() const; //2c7b10, slot14
    void InitSendMsg(unsigned char); //2c7bc0
    void SendMsg(); //2c7c00
    void AddOp(); //2c7d10
    RoutingMemberInfoPtr GetNewMemberInfo(); //2c4660, delegates to connection slot20
    RoutingGroupInfoPtr GetNewGroupInfo(); //2c5bb0, delegates to connection slot24, owned sret
    RoutingClientInfoPtr GetNewClientInfo(); // 2c4640; owned sret, delegates to connection slot1c
    RoutingConnection* GetConnection() { return this->mConnection.get(); }
    WriteBuffer mSendMsg; //44..5b, size 0x18
    bool mIsDerivedServerOp; //5c, natural alignment5d..5f
    SmartPtr<RoutingConnection> mConnection; //60, destructor owns release
};

// Further known prefixes of derived ops; never allocated here, no RTTI/vtable emission.
// Names follow the WON SDK headers; layouts follow the original.
class RoutingGetClientListOp : public RoutingOp {
public:
    RoutingGetClientListOp(RoutingConnection* c) : RoutingOp(c),mIsEx(false) {} //0x74 bytes, 2c88e4..2c8917
    virtual RoutingOpType GetType()const { return (RoutingOpType)20; } //type20, 6 bytes
    virtual void SendRequest(); // 2c56d0
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 2c57a0
    RoutingClientMap mClientMap; //64, typed dtor2bcecb->d2add; typed insert2c58b0->d1c02
    bool mIsEx; //70, 2bcdab stores false; allocation 0x74
};
// Administrative operation records: 0x88/0x88/0x78 bytes
class RoutingBanClientOp : public RoutingOp {
public:
    RoutingBanClientOp(RoutingConnection*); //2c4320,111B,ret4
    virtual void SendRequest(); //six-slot vtable 44d0c8
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&);
    virtual RoutingOpType GetType() const; //type3, 6 bytes
    unsigned short mGroupId; //64, ctorffff
    bool mIsBanned; //66, ctortrue; alignment67
    unsigned long mBanTime; //68, ctor0
    std::wstring mBanComment; //6c..7b
    void SetBanComment(const std::wstring& v) { this->mBanComment=v; }
    void SetGroupId(unsigned short v) { this->mGroupId=v; }
    void SetBanTime(unsigned long v) { this->mBanTime=v; }
    void SetClientId(unsigned short v) { this->mClientId=v; this->mBanByWONId=false; }
    void SetWONId(unsigned long v,bool inIsCDKeyId) { this->mWONId=v; this->mBanByWONId=true; this->mWONIdIsCDKeyId=inIsCDKeyId; }
    bool mBanByWONId; //7c, ctortrue
    bool mWONIdIsCDKeyId; //7d, ctorfalse; alignment7e..7f
    unsigned long mWONId; //80, ctor0
    unsigned short mClientId; //84, ctor0; natural alignment86..87
    // 2c43c0 is an ordinary implicit dtor (no own-vptr write). No explicit dtor.
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short GetClientId() const { return this->mClientId; }
};
class RoutingMuteClientOp : public RoutingOp {
public:
    RoutingMuteClientOp(RoutingConnection*); //2c78c0,111B,ret4
    virtual void SendRequest(); //six-slot vtable 44d1c0
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&);
    virtual RoutingOpType GetType() const; //type38, 6 bytes
    unsigned short mGroupId; //64
    bool mIsMuted; //66
    unsigned long mMuteTime; //68
    std::wstring mMuteComment; //6c
    void SetMuteComment(const std::wstring& v) { this->mMuteComment=v; }
    void SetGroupId(unsigned short v) { this->mGroupId=v; }
    void SetMuteTime(unsigned long v) { this->mMuteTime=v; }
    void SetClientId(unsigned short v) { this->mClientId=v; this->mMuteByWONId=false; }
    void SetWONId(unsigned long v,bool inIsCDKeyId) { this->mWONId=v; this->mMuteByWONId=true; this->mWONIdIsCDKeyId=inIsCDKeyId; }
    void SetIsMuted(bool v) { this->mIsMuted=v; }
    bool mMuteByWONId; //7c
    bool mWONIdIsCDKeyId; //7d
    unsigned long mWONId; //80
    unsigned short mClientId; //84
    // 2c7960 is an ordinary implicit dtor (no own-vptr write). Allocation 0x88.
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    bool GetIsMuted() const { return this->mIsMuted; }
    unsigned short GetClientId() const { return this->mClientId; }
};

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

class RoutingGetBadUserListOp : public RoutingOp {
public:
    RoutingGetBadUserListOp(RoutingConnection* c,RoutingBadUserListType kind,
                          unsigned short group,unsigned char flags)
      :RoutingOp(c),mListType(kind),mGroupId(group),mFlags(flags) {}
    virtual void SendRequest(); //2c5320, slot0c
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c5370, slot10
    virtual RoutingOpType GetType()const { return (RoutingOpType)19; } //2bfa10 type19
    RoutingBadUserListType mListType; //64, fullenum32
    unsigned short mGroupId; //68
    unsigned char mFlags; //6a
    RoutingBadUserList mBadUserList; //6c allocator byte/head70/count74, full78
    // Implicit normal2bfa40 has no own-vptr write; deleting2bfa20 calls it.
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned char GetFlags() const { return this->mFlags; }
};


// type22, VT44cf20, ordinary dtor 2bd320.
// Reply2c63f0 comparesgroup64, writesflags68/69 andobservercount66.
// Record/list input for dialogs; never allocated, sized or copied here.


class RoutingGetMembersOfGroupOp : public RoutingOp {
public:
    RoutingGetMembersOfGroupOp(RoutingConnection* c,unsigned short id) : RoutingOp(c),mGroupId(id) {}
    RoutingGetMembersOfGroupOp(RoutingConnection* c) : RoutingOp(c) {}
    virtual void SendRequest(); //44cf20+0c
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c63f0
    virtual RoutingOpType GetType() const { return (RoutingOpType)22; } //22

    unsigned short mGroupId; //64, reply2c6447 comparesrequestgroup
    unsigned short GetGroupId() const { return this->mGroupId; }
    void SetGroupId(unsigned short v) { this->mGroupId=v; }
    unsigned short mObserverCount; //66, response2c6491 fullwordstore
    unsigned short GetObserverCount() const { return this->mObserverCount; }
    bool mHasClientNames; //68, response2c6480 storesReadByte&1
    bool mHasClientFlags; //69, response2c6487 stores(ReadByte>>1)&1
    RoutingMemberMap mMembers; //6c, typed dtor2bd36b->d2a1e; naturalalignment6a..6b
    const RoutingMemberMap& GetMembers() const { return this->mMembers; }
};

class RoutingGetGroupListOp : public RoutingOp {
public:
    RoutingMemberInfoPtr GetNewMemberInfo(); //ownhelper2c5a60, nullconnection fallback
    RoutingGroupInfoPtr GetNewGroupInfo(); //ownhelper2c5ad0, nullconnection fallback
    // Only the inline constructor its callers need; everything else is out of line:
    RoutingGetGroupListOp() : RoutingOp(),mFlags(0),mParentGroupId(0xffff) {}
    bool ParseGroupListObj(const void*,unsigned long); //2c5f70
    RoutingGetGroupListOp(RoutingConnection* inConnection,unsigned short inFlags)
      : RoutingOp(inConnection),mFlags(inFlags),mParentGroupId(0xffff) {}
    virtual void SendRequest(); //SendRequest, slot from 44cf04
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c5bd0
    virtual RoutingOpType GetType() const { return (RoutingOpType)21; } //type21
    bool IncludeName() const { return (this->mFlags & 1) ? true : false; }
    bool IncludeCaptain() const { return (this->mFlags & 2) ? true : false; }
    bool IncludeMaxPlayers() const { return (this->mFlags & 4) ? true : false; }
    bool IncludeFlags() const { return (this->mFlags & 8) ? true : false; }
    bool IncludeAsyncFlags() const { return (this->mFlags & 0x10) ? true : false; }
    bool IncludeMemberCount() const { return (this->mFlags & 0x20) ? true : false; }
    bool IncludeObserverCount() const { return (this->mFlags & 0x40) ? true : false; }
    bool IncludeMembers() const { return (this->mFlags & 0x80) ? true : false; }
    unsigned short mFlags; //64
    unsigned short mParentGroupId; //66
    RoutingGroupMap mGroupMap; //68..73
    const RoutingGroupMap& GetGroupMap() const { return this->mGroupMap; }
    // Implicit destructor, no explicit empty/out-of-line decl requested.
public:
    unsigned short GetFlags() const { return this->mFlags; }
    void SetFlags(unsigned short v) { this->mFlags=v; }
};

class RoutingLeaveGroupOp : public RoutingOp {
public:
    RoutingLeaveGroupOp(RoutingConnection* c,unsigned short id=0)
      : RoutingOp(c),mGroupId(id) {}
    virtual void SendRequest(); //SendRequest from VT44cf58
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c8e90, shared body
    virtual RoutingOpType GetType()const; //35
    unsigned short mGroupId; //64
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
};

// RTTI .?AVRoutingRegisterClientOp@WONAPI@@, VT44d21c, type42.
// Base RoutingOp 0x64; ctor 2c8450; ordinary dtor 2c8570; allocation 0xac at 3ee504.

class RoutingRegisterClientOp : public RoutingOp {
public:
    RoutingRegisterClientOp(RoutingConnection*); //2c8450
    RoutingRegisterClientOp(RoutingConnection*,const std::wstring&,const std::wstring&,
        unsigned long,unsigned long,unsigned long,unsigned long); //2c8680 ret1c; leavesclientId94 untouched
    virtual void SendRequest(); //2c87a0
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c8840
    virtual RoutingOpType GetType() const; //42
    std::wstring mClientName; //64..73
    std::wstring mServerPassword; //74..83
    unsigned long mReconnectId; //84: ctor2c8502 DWORD, request2c87f2 DWORD->AppendLong; reply2c88a6 DWORD
    unsigned long GetReconnectId() const { return this->mReconnectId; }
    unsigned long mAsyncMessageFlags; //88: ctor2c8508,request2c8800 DWORD
    unsigned long mRegisterFlags; //8c: ctor2c850e,request2c880e DWORD
    unsigned long mClientFlags; //90: ctor2c8514,request2c881c DWORD
    unsigned short mClientId; //94: ctor2c851a WORD, reply2c888b stores ReadShort AX
    RoutingClientMap mClientMap; //98: typed map (head 9c, count a0)
    int mReplyCount; //a4: uninitialized by ctor, SendRequest2c87aa sets0
    int mNumRepliesNeeded; //a8: SendRequest2c87b6 sets1,2c87c2 sets2
public:
    unsigned short GetClientId() const { return this->mClientId; }
    const std::wstring& GetClientName() const { return this->mClientName; }
    unsigned long GetClientFlags() const { return this->mClientFlags; }
    unsigned long GetAsyncMessageFlags() const { return this->mAsyncMessageFlags; }
    const RoutingClientMap& GetClientMap() const { return this->mClientMap; }
};

// EEWonNet reads +84 as a ULONG "mClientId"; it holds the reconnect ID. That read stays a
// DWORD at +84; the client ID itself is the ushort at +94.



// RTTI44ceb0; type6; processor2c4580 reads short64,long68.
// Routing3e7a30 GetClient64, writes client flags+10 from68.
// .?AVRoutingClientFlagsChangedOp@WONAPI@@; vtable0x44ceb0; processor0x2c4580; type6
class RoutingClientFlagsChangedOp : public RoutingOp {
public:
    RoutingClientFlagsChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 6; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4580
    unsigned short mClientId; //64
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned long mClientFlags; //68, compiler's natural 2-byte alignment hole
    unsigned long GetClientFlags() const { return this->mClientFlags; }
};

// RTTI44ce94; type7; processor2c45c0 reads group64/client66/memberbyte68.
// Later fields (6c wstring, 7c flags, 80 bool) exist but this prefix does not need them.
// Routing3e8030 separately resolves group64/client66 and applies member68.
// .?AVRoutingClientJoinedGroupOp@WONAPI@@; vtable0x44ce94; processor0x2c45c0; type7
class RoutingClientJoinedGroupOp : public RoutingOp {
public:
    RoutingClientJoinedGroupOp(RoutingConnection* connection) : RoutingOp(connection),mClientFlags(0),mHasClientFlags(false) {}
    virtual RoutingOpType GetType() const { return 7; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c45c0
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short mClientId; //66
    unsigned char mMemberFlags; //68
    std::wstring mClientName; //6c, 2c4610
    unsigned long mClientFlags; //7c, 2c4621
    bool mHasClientFlags; //80, 2c4624
public:
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned char GetMemberFlags() const { return this->mMemberFlags; }
    const std::wstring& GetClientName() const { return this->mClientName; }
    unsigned long GetClientFlags() const { return this->mClientFlags; }
    bool HasClientFlags() const { return this->mHasClientFlags; }
};

// RTTI44ce78; type8; processor2c4700 calls the info getter 2c4640, then
// owns RoutingClientInfo64; body parses client fields through that pointee.
// Routing3e7d60 passes raw64 to LobbyServer::AddClient(RoutingClientInfo*).
// .?AVRoutingClientJoinedServerOp@WONAPI@@; vtable0x44ce78; processor0x2c4700; type8
class RoutingClientJoinedServerOp : public RoutingOp {
public:
    RoutingClientJoinedServerOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 8; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4700
    SmartPtr<RoutingClientInfo> mClientInfo; //64
    const std::wstring& GetClientName() const { if(this->mClientInfo.get()!=NULL) return this->mClientInfo->mName; else return GetEmptyWStr(); }
};

// RTTI44ce5c; type9; processor2c4800 reads two shorts.
// Routing3e8160 resolves group64 then removes client66 with owned returned SmartPtr.
// .?AVRoutingClientLeftGroupOp@WONAPI@@; vtable0x44ce5c; processor0x2c4800; type9
class RoutingClientLeftGroupOp : public RoutingOp {
public:
    RoutingClientLeftGroupOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 9; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4800
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short mClientId; //66
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned char mMemberFlags; //68, WON SDK field; the factory allocates 0x6c; the parser leaves it unwritten
public:
    unsigned char GetMemberFlags() const { return this->mMemberFlags; }
};

// RTTI44ce40; type10; processor2c4840 reads one short; Routing3e7e80 removes client64.
// .?AVRoutingClientLeftServerOp@WONAPI@@; vtable0x44ce40; processor0x2c4840; type10
class RoutingClientLeftServerOp : public RoutingOp {
public:
    RoutingClientLeftServerOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 10; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4840
    unsigned short mClientId; //64
public:
    unsigned short GetClientId() const { return this->mClientId; }
};

// RTTI44ce24; type13; processor2c4e40 reads short64,string68,wstring78,
// ReadBuffer2a4110 returned intrusive ByteBuffer assigned88 with full ownership.
// Routing3e8ad0 calls the by-value GetData helper d06f1 and releases the result after the call.
// .?AVRoutingDataObjectCreatedOp@WONAPI@@; vtable0x44ce24; processor0x2c4e40; type13
class RoutingDataObjectCreatedOp : public RoutingOp {
public:
    RoutingDataObjectCreatedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 13; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4e40
    unsigned short mLinkId; //64
    std::string mDataType; //68
    std::wstring mName; //78
    ConstSmartPtr<ByteBuffer> mData; //88
    // d06f1 returns an owned pointer in a caller-provided sret; no class lifetime.
    ByteBufferPtr GetData() const { return this->mData; }
    unsigned short GetLinkId() const { return this->mLinkId; } //value accessor
};

// type14 (RTTI, processor 2c4f30); two strings.
// Routing3e8d10 compares string68 and removes game link64; does not touch string78.
// .?AVRoutingDataObjectDeletedOp@WONAPI@@; vtable0x44ce08; processor0x2c4f30; type14
class RoutingDataObjectDeletedOp : public RoutingOp {
public:
    RoutingDataObjectDeletedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 14; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4f30
    unsigned short mLinkId; //64
    std::string mDataType; //68
    std::wstring mName; //78
    unsigned short GetLinkId() const { return this->mLinkId; } //value accessor
};

// RTTI44cdd0; type23; processor2c6c10 reads group64/newcaptain66.
// Routing3e7c20 passes captain66 to LobbyGroup::SetCaptain with an old-client out pointer.
// .?AVRoutingGroupCaptainChangedOp@WONAPI@@; vtable0x44cdd0; processor0x2c6c10; type23
class RoutingGroupCaptainChangedOp : public RoutingOp {
public:
    RoutingGroupCaptainChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 23; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6c10
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short mCaptainId; //66
    unsigned short GetCaptainId() const { return this->mCaptainId; }
};

// RTTI44cdb4; type24; processor2c6c50:short64,wstring68,short78,long7c,long80.
// Getter 2c6cb0 returns SmartPtr<RoutingGroupInfo> BY VALUE (sret use
// and release in Routing3e8f80. Return is neither raw pointer nor caller-owned info.
// .?AVRoutingGroupCreatedOp@WONAPI@@; vtable0x44cdb4; processor0x2c6c50; type24
class RoutingGroupCreatedOp : public RoutingOp {
public:
    RoutingGroupCreatedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 24; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6c50
    unsigned short mGroupId; //64
    std::wstring mName; //68
    unsigned short mMaxClients; //78 (meaning inferred)
    unsigned long mFlags; //7c (meaning inferred)
    unsigned long mAsyncFlags; //80 (meaning inferred)
    RoutingGroupInfoPtr GetGroupInfo(); //2c6cb0
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned long GetFlags() const { return this->mFlags; }
    unsigned long GetAsyncFlags() const { return this->mAsyncFlags; }
    unsigned short GetMaxClients() const { return this->mMaxClients; }
    const std::wstring& GetName() const { return this->mName; }
};

// RTTI44cd98; type25; processor2c6da0 reads one short; Routing3e9040 removes group64.
// .?AVRoutingGroupDeletedOp@WONAPI@@; vtable0x44cd98; processor0x2c6da0; type25
class RoutingGroupDeletedOp : public RoutingOp {
public:
    RoutingGroupDeletedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 25; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6da0
    unsigned short mGroupId; //64
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
};

// RTTI44cd60; type27; processor2c6e10 reads two shorts,bool and wstring.
// Routing3e9100 resolves GAME by64 and CLIENT by66 (different order from JoinAttempt).
// .?AVRoutingGroupInvitationOp@WONAPI@@; vtable0x44cd60; processor0x2c6e10; type27
class RoutingGroupInvitationOp : public RoutingOp {
public:
    RoutingGroupInvitationOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 27; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6e10
    unsigned short mGroupId; //64
    unsigned short mClientId; //66
    bool mInvited; //68
    std::wstring mComment; //6c
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short GetClientId() const { return this->mClientId; }
    bool GetInvited() const { return this->mInvited; }
    const std::wstring& GetComment() const { return this->mComment; }
};

// RTTI44cd44; type28; processor2c6e60 reads two shorts,wstring68,byte78.
// Routing3e92d0 compares66 to current game group, resolves CLIENT by64. Order matters.
// .?AVRoutingGroupJoinAttemptOp@WONAPI@@; vtable0x44cd44; processor0x2c6e60; type28
class RoutingGroupJoinAttemptOp : public RoutingOp {
public:
    RoutingGroupJoinAttemptOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 28; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6e60
    unsigned short mClientId; //64
    unsigned short mGroupId; //66
    std::wstring mComment; //68
    unsigned char mFlags; //78
public:
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned short GetGroupId() const { return this->mGroupId; }
    const std::wstring& GetComment() const { return this->mComment; }
    unsigned char GetFlags() const { return this->mFlags; }
};

// RTTI44ccb8; type36; processor2c7650 reads group64,client66,memberbyte68.
// Routing3e7b40 writes LobbyClient+1c or +1d based on the LobbyGroup kind.
// .?AVRoutingMemberFlagsChangedOp@WONAPI@@; vtable0x44ccb8; processor0x2c7650; type36
class RoutingMemberFlagsChangedOp : public RoutingOp {
public:
    RoutingMemberFlagsChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 36; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c7650
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short mClientId; //66
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned char mMemberFlags; //68
    unsigned char GetMemberFlags() const { return this->mMemberFlags; }
};

// RTTI44cc9c; type39; processor2c7d40 explicitly reads FLAGS AS A WORD.
// Flags100 drives Routing3e7770: old uchar bitfield+gap is not a faithful representation.
// List78 has 0c-byte nodes, ushort at node+8, count80. The game adapter
// must read list.size/empty instead of mL80 and flag&4 instead of bit2.
// .?AVRoutingPeerChatOp@WONAPI@@; vtable0x44cc9c; processor0x2c7d40; type39
class RoutingPeerChatOp : public RoutingOp {
public:
    RoutingPeerChatOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 39; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c7d40
    unsigned short mClientId; //64 sender
    unsigned short mFlags; //66
    std::wstring mText; //68
    std::list<unsigned short> mRecipients; //78
public:
    unsigned short GetClientId() const { return this->mClientId; }
    unsigned short GetFlags() const { return this->mFlags; }
    bool IsWhisper() const { return (this->mFlags & 4) ? true : false; }
    const std::wstring& GetText() const { return this->mText; }
    const std::list<unsigned short>& GetRecipients() const { return this->mRecipients; }
};

// RTTI44cc80; type40; processor2c7e00 ReadShort64, ReadBuffer2a4110 thenowned buffer68.
// Getter 3e79f0 returns SmartPtr by value; Routing3e7940 releases it after use.
// .?AVRoutingPeerDataOp@WONAPI@@; vtable0x44cc80; processor0x2c7e00; type40
class RoutingPeerDataOp : public RoutingOp {
public:
    RoutingPeerDataOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 40; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c7e00
    unsigned short mClientId; //64
    ByteBufferPtr mData; //68
    ByteBufferPtr GetData() const { return this->mData; }
public:
    unsigned short GetClientId() const { return this->mClientId; }
};

// RTTI44cc64; type48; processor2c8b80 ReadWString into64.
// .?AVRoutingServerAlertOp@WONAPI@@; vtable0x44cc64; processor0x2c8b80; type48
class RoutingServerAlertOp : public RoutingOp {
public:
    RoutingServerAlertOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 48; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c8b80
    std::wstring mText; //64
};

// RTTI44cc48; type49; processor2c8bb0 validates only, but dtor 2bc370
// destroys the default-constructed COW WSTRING64 (pointer68,length6c,reserved70).
// Routing3e89b0 reads this member even though no text came from the wire.
// .?AVRoutingServerShutdownAbortedOp@WONAPI@@; vtable0x44cc48; processor0x2c8bb0; type49
class RoutingServerShutdownAbortedOp : public RoutingOp {
public:
    RoutingServerShutdownAbortedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 49; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c8bb0
    std::wstring mText; //64
};

// RTTI44cc2c; type50; processor2c8bd0 ReadWString64 then ReadLong74.
// .?AVRoutingServerShutdownStartedOp@WONAPI@@; vtable0x44cc2c; processor0x2c8bd0; type50
class RoutingServerShutdownStartedOp : public RoutingOp {
public:
    RoutingServerShutdownStartedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 50; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c8bd0
    std::wstring mText; //64
    unsigned long mSeconds; //74
public:
    unsigned long GetSeconds() const { return this->mSeconds; }
};

// RTTI44cc10; type58; processor2c8eb0 reads short64,bool66,wstring6c,long68.
// Routing3e82b0 ignores an unban; duration0 or>=10 meanskind0,1..9 meanskind1.
// .?AVRoutingYouWereBannedOp@WONAPI@@; vtable0x44cc10; processor0x2c8eb0; type58
class RoutingYouWereBannedOp : public RoutingOp {
public:
    RoutingYouWereBannedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 58; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c8eb0
    unsigned short mGroupId; //64
    bool mIsBanned; //66
    unsigned long mDuration; //68
    std::wstring mComment; //6c
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned long GetDuration() const { return this->mDuration; }
    const std::wstring& GetComment() const { return this->mComment; }
};

// type59 (RTTI, processor 2c8f00).
// Same wire widths as banned; Routing3e8580 uses bool66 to selectkind2or3.
// .?AVRoutingYouWereMutedOp@WONAPI@@; vtable0x44cbf4; processor0x2c8f00; type59
class RoutingYouWereMutedOp : public RoutingOp {
public:
    RoutingYouWereMutedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 59; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c8f00
    unsigned short mGroupId; //64
    bool mIsMuted; //66
    unsigned long mDuration; //68
    std::wstring mComment; //6c
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    bool GetIsMuted() const { return this->mIsMuted; }
    unsigned long GetDuration() const { return this->mDuration; }
    const std::wstring& GetComment() const { return this->mComment; }
};

// RTTI type41 VT44d1f8, ctor 2c8000 / 2bf1d0.
// The original allocates 0x88 bytes; this partial declaration is never allocated.
// .?AVRoutingReadDataObjectOp@WONAPI@@; vtable0x44d1f8; processor0x2c8130; type41
class RoutingReadDataObjectOp : public RoutingOp {
public:
    RoutingReadDataObjectOp(RoutingConnection*);
    virtual void SendRequest();
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&);
    virtual RoutingOpType GetType() const;
    unsigned short mLinkId; //64, 2bf231
    std::string mDataType; //68, ctor zeroes 6c/70/74, assigned at 2bf24d
    void SetDataType(const std::string& v) { this->mDataType=v; }
    unsigned char mFlag; //78 flag mask; AppendByte 2c8117, SDK mFlags
    bool mSubscribe; //79, 2bf22d
    RoutingDataObjectList mDataObjects; //7c, head80,count84; reader2c8170
public:
    unsigned short GetLinkId() const { return this->mLinkId; }
};


// Known prefixes of further ops; never allocated here, no RTTI emission.
class RoutingDataObjectModifiedOp : public RoutingOp {
public:
    RoutingDataObjectModifiedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 15; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c4f80
    unsigned short mLinkId; //64 ReadShort2c4fb8
    std::string mDataType; //68 ReadString2c4fc4
    std::wstring mName; //78 ReadWString2c4fd1
    unsigned short mOffset; //88 ReadShort2c4fdf, not ULONG
    bool mIsInsert; //8a ReadByte2c4ff4; bool in the SDK
    ByteBufferPtr mData; //8c owned parserassign2c5045
    unsigned short GetLinkId() const { return this->mLinkId; } //BY VALUE, preserves map-key temporary
    unsigned short GetOffset() const { return this->mOffset; }
    bool GetIsInsert() const { return this->mIsInsert; }
    ByteBufferPtr GetData() const { return this->mData; } //owned BY VALUE, keep temporary lifetime
};
// Notification receivers; held by pointer only, the parser does not allocate them.
class RoutingGroupFlagsChangedOp : public RoutingOp {
public:
    RoutingGroupFlagsChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 26; } //slot14
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c6dd0
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned long mGroupFlags; //68
    unsigned long GetGroupFlags() const { return this->mGroupFlags; }
    unsigned long mAsyncFlags; //6c
};
class RoutingGroupMemberCountOp : public RoutingOp {
public:
    RoutingGroupMemberCountOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 30; } //slot14
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c6eb0
    unsigned short mGroupId; //64
    unsigned short mMemberCount; //66
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
};
class RoutingGroupNameChangedOp : public RoutingOp {
public:
    RoutingGroupNameChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 31; } //slot14
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c6f30
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    std::wstring mName; //68
};
class RoutingGroupObserverCountOp : public RoutingOp {
public:
    RoutingGroupObserverCountOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 32; } //slot14
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c6f70
    unsigned short mGroupId; //64
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short mObserverCount; //66
    unsigned short GetObserverCount() const { return this->mObserverCount; }
};

// type29, not 30. VT44cd28, wire25, processor2c6ef0.
class RoutingGroupMaxPlayersChangedOp : public RoutingOp {
public:
    RoutingGroupMaxPlayersChangedOp(RoutingConnection* connection) : RoutingOp(connection) {}
    virtual RoutingOpType GetType() const { return 29; } //slot14
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); // 0x2c6ef0
    unsigned short mGroupId; //64 store2c6f12
    unsigned short mMaxPlayers; //66 store2c6f1b
    unsigned short GetMaxPlayers() const { return this->mMaxPlayers; }
    unsigned short GetGroupId() const { return this->mGroupId; } //BY VALUE, not constref
    unsigned short GetNewMaxPlayers() const { return this->mMaxPlayers; }
};
// 0x98-byte records: allocations 3ed6c9/3ed9a1, six-slot tables.
// Implicit ordinary destructors:2c4b40/2c7250 have no own-vptr store.
class RoutingCreateGroupOp : public RoutingOp {
public:
    void CheckLeaveGroup(short); //2c4cb0

    RoutingCreateGroupOp(RoutingConnection*); //2c4a80
    virtual void SendRequest(); //2c4c20 SendRequest
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c4d90 HandleReply
    virtual RoutingOpType GetType() const { return (RoutingOpType)12; } //2c4b10 type12
    unsigned short mGroupId; //64
    unsigned short mParentGroupId; //66
    std::wstring mGroupName; //68
    void SetGroupName(const std::wstring& v) { this->mGroupName=v; }
    std::wstring mGroupPassword; //78
    void SetGroupPassword(const std::wstring& v) { this->mGroupPassword=v; }
    unsigned short mMaxPlayers; //88
    unsigned char mJoinFlags; //8a, alignment8b
    unsigned long mGroupFlags; //8c
    unsigned long mAsyncFlags; //90
    bool mLeaveGroupOnCancel; //94, natural tail95..97
    void SetGroupFlags(unsigned long v) { this->mGroupFlags=v; }
    void SetJoinFlags(unsigned char v) { this->mJoinFlags=v; }
    void SetLeaveGroupOnCancel(bool v) { this->mLeaveGroupOnCancel=v; }
    unsigned short GetGroupId() const { return this->mGroupId; }
};
class RoutingJoinGroupOp : public RoutingOp {
public:
    void SetGroupId(unsigned short v) { this->mGroupId=v; }
    RoutingJoinGroupOp(RoutingConnection*); //2c7140
    virtual void SendRequest(); //2c7370 SendRequest
    virtual WONStatus HandleReply(unsigned char,ReadBuffer&); //2c73e0 HandleReply
    virtual RoutingOpType GetType() const { return (RoutingOpType)34; } //2c7220 type34
    unsigned short mGroupId; //64, alignment66..67
    std::wstring mGroupPassword; //68
    void SetGroupPassword(const std::wstring& v) { this->mGroupPassword=v; }
    std::wstring mJoinComment; //78
    void SetJoinComment(const std::wstring& v) { this->mJoinComment=v; }
    unsigned char mJoinFlags; //88
    unsigned char GetJoinFlags() const { return this->mJoinFlags; }
    unsigned char mMemberFlags; //89, alignment8a..8b
    RoutingMemberMap mMemberMap; //8c, size98
    unsigned short GetGroupId() const { return this->mGroupId; }
public:
    unsigned char GetMemberFlags() const { return this->mMemberFlags; }
};
// Request operation receivers; held by pointer only, never allocated here.
class RoutingCancelJoinGroupOp : public RoutingOp {
public:
    RoutingCancelJoinGroupOp(RoutingConnection* c,unsigned short id=0,bool leave=true) : RoutingOp(c),mGroupId(id),mLeaveIfAlreadyInGroup(leave) {}
    virtual void SendRequest(); //0x2c4540, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type5, slot14
    unsigned short mGroupId; //64
    bool mLeaveIfAlreadyInGroup; //66
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
};
class RoutingSetGroupFlagsOp : public RoutingOp {
public:
    RoutingSetGroupFlagsOp(RoutingConnection* c,unsigned short id=0,unsigned long mask=0,unsigned long flags=0,unsigned long asyncMask=0,unsigned long asyncFlags=0) : RoutingOp(c),mGroupId(id),mGroupFlagMask(mask),mGroupFlags(flags),mAsyncFlagMask(asyncMask),mAsyncFlags(asyncFlags) {}
    virtual void SendRequest(); //0x2c8c60, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type52, slot14
    unsigned short mGroupId; //64
    unsigned long mGroupFlagMask; //68
    unsigned long mGroupFlags; //6c
    unsigned long mAsyncFlagMask; //70
    unsigned long mAsyncFlags; //74
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
};
class RoutingSendServerAlertOp : public RoutingOp {
public:
    RoutingSendServerAlertOp(RoutingConnection* c,const std::wstring& text) : RoutingOp(c),mAlertText(text) {}
    virtual void SendRequest(); //0x2c8b20, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type47, slot14
    std::wstring mAlertText; //64
    std::list<unsigned short> mRecipientList; //74
};
class RoutingStartServerShutdownOp : public RoutingOp {
public:
    RoutingStartServerShutdownOp(RoutingConnection* c,const std::wstring& text,unsigned long seconds) : RoutingOp(c),mAlertText(text),mSecondsUntilShutdown(seconds) {}
    virtual void SendRequest(); //0x2c8cc0, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type56, slot14
    std::wstring mAlertText; //64
    unsigned long mSecondsUntilShutdown; //74
};
class RoutingAbortServerShutdownOp : public RoutingOp {
public:
    RoutingAbortServerShutdownOp(RoutingConnection* c) : RoutingOp(c) {}
    virtual void SendRequest(); //0x2c4170, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type1, slot14
};
class RoutingSetClientFlagsOp : public RoutingOp {
public:
    RoutingSetClientFlagsOp(RoutingConnection* c,unsigned short mask=0,unsigned long flags=0,unsigned long asyncMask=0,unsigned long asyncFlags=0) : RoutingOp(c),mClientFlagMask(mask),mClientFlags(flags),mAsyncMessageFlagMask(asyncMask),mAsyncMessageFlags(asyncFlags) {}
    virtual void SendRequest(); //0x2c8c10, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type51, slot14
    unsigned long mClientFlagMask; //64
    unsigned long mClientFlags; //68
    unsigned long mAsyncMessageFlagMask; //6c
    unsigned long mAsyncMessageFlags; //70
};
class RoutingSendChatOp : public RoutingOp {
public:
    RoutingSendChatOp(RoutingConnection* c) : RoutingOp(c),mFlags(0) {}
    virtual void SendRequest(); //0x2c8a20, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type44, slot14
    unsigned short mFlags; //64
    std::wstring mText; //68
    void SetFlags(unsigned short v) { this->mFlags=v; }
    void SetRecipients(const std::list<unsigned short>& v) { this->mRecipients=v; }
    void SetText(const std::wstring& v) { this->mText=v; }
    std::list<unsigned short> mRecipients; //78
public:
    unsigned short GetFlags() const { return this->mFlags; }
};
class RoutingSendDataOp : public RoutingOp {
public:
    RoutingSendDataOp(RoutingConnection* c) : RoutingOp(c),mFlags(0) {}
    virtual void SendRequest(); //0x2c8aa0, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type46, slot14
    unsigned char mFlags; //64
    void SetFlags(unsigned char v) { this->mFlags=v; }
    void SetData(const ByteBuffer* v) { this->mData=v; }
    void SetRecipients(const std::list<unsigned short>& v) { this->mRecipients=v; }
    ConstSmartPtr<ByteBuffer> mData; //68
    std::list<unsigned short> mRecipients; //6c
public:
    unsigned char GetFlags() const { return this->mFlags; }
};
class RoutingBecomeModeratorOp : public RoutingOp {
public:
    RoutingBecomeModeratorOp(RoutingConnection* c,bool on=false) : RoutingOp(c),mModeratorOn(on) {}
    virtual void SendRequest(); //0x2c4510, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type4, slot14
    bool mModeratorOn; //64
};
class RoutingAcceptClientOp : public RoutingOp {
public:
    RoutingAcceptClientOp(RoutingConnection*); //2c4190, 89 bytes
    virtual void SendRequest(); //0x2c42d0, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type2, slot14
    unsigned short mGroupId; //64
    unsigned short mClientId; //66
    bool mAccepted; //68
    std::wstring mAcceptComment; //6c
    void SetAcceptComment(const std::wstring& v) { this->mAcceptComment=v; }
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short GetClientId() const { return this->mClientId; }
};
class RoutingDeleteDataObjectOp : public RoutingOp {
public:
    RoutingDeleteDataObjectOp(RoutingConnection*);
    virtual void SendRequest(); //0x2c5210, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type16, slot14
    unsigned short mLinkId; //64
    std::string mDataType; //68
    void SetDataType(const std::string& v) { this->mDataType=v; }
    std::wstring mDataName; //78
    void SetDataName(const std::wstring& v) { this->mDataName=v; }
public:
    unsigned short GetLinkId() const { return this->mLinkId; }
};
class RoutingInviteClientOp : public RoutingOp {
public:
    RoutingInviteClientOp(RoutingConnection*); //2c6fb0, 89 bytes
    virtual void SendRequest(); //0x2c70f0, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type33, slot14
    unsigned short mGroupId; //64
    unsigned short mClientId; //66
    bool mInvited; //68
    std::wstring mInviteComment; //6c
    void SetInviteComment(const std::wstring& v) { this->mInviteComment=v; }
public:
    unsigned short GetGroupId() const { return this->mGroupId; }
    unsigned short GetClientId() const { return this->mClientId; }
    bool GetInvited() const { return this->mInvited; }
};
class RoutingModifyDataObjectOp : public RoutingOp {
public:
    RoutingModifyDataObjectOp(RoutingConnection*);
    virtual void SendRequest(); //0x2c7840, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type37, slot14
    unsigned short mLinkId; //64
    std::string mDataType; //68
    void SetDataType(const std::string& v) { this->mDataType=v; }
    std::wstring mDataName; //78
    void SetDataName(const std::wstring& v) { this->mDataName=v; }
    unsigned short mOffset; //88
    bool mIsInsert; //8a
    ByteBufferPtr mData; //8c
public:
    unsigned short GetLinkId() const { return this->mLinkId; }
};
class RoutingUnsubscribeDataObjectOp : public RoutingOp {
public:
    RoutingUnsubscribeDataObjectOp(RoutingConnection*);
    virtual void SendRequest(); //0x2c8e40, slot0c
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&); //2c8e90 (shared body), slot10
    virtual RoutingOpType GetType() const; //type57, slot14
    unsigned short mLinkId; //64
    std::string mDataType; //68
    void SetDataType(const std::string& v) { this->mDataType=v; }
    unsigned char mFlags; //78
public:
    unsigned short GetLinkId() const { return this->mLinkId; }
    unsigned char GetFlags() const { return this->mFlags; }
};



// REQUEST 11, distinct from notification Created 13 and read/subscription 41.
// 2becd0 allocates 0x8c, ctor 2c4870; the ctor writes VT44d0e4. Matches the SDK.
class RoutingCreateDataObjectOp : public RoutingOp {
public:
    RoutingCreateDataObjectOp(RoutingConnection*);
    virtual void SendRequest();
    virtual WONStatus HandleReply(unsigned char, ReadBuffer&);
    virtual RoutingOpType GetType() const;
    unsigned char mFlags; //64 2bed34; NOT linkID64
    unsigned short mLinkId; //66 2bed37, ctor 2c48c4 zeroes the WORD
    std::string mDataType; //68
    void SetDataType(const std::string& v) { this->mDataType=v; }
    std::wstring mDataName; //78
    void SetDataName(const std::wstring& v) { this->mDataName=v; }
    ByteBufferPtr mData; //88 ctor 2c48be zeroes it; owned assignment at 2bedad
    unsigned short GetLinkId() const { return this->mLinkId; } //BYVALUE, trueoffset66
    ByteBufferPtr GetData() const { return this->mData; } //ownedBYVALUE, preservetemporary releases
public:
    unsigned char GetFlags() const { return this->mFlags; }
};

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

// AuthSession: 0x10cc bytes; constructor 2e27b0 copies the by-value Blowfish (0x10b4);
// dtor 2e2a80 destroys the RawBuffers and the RefCount base.
class AuthSession : public RefCount {
protected:
    AuthType mAuthType; //08
    unsigned char mLengthFieldSize; //0c, natural alignment0d
    unsigned short mId; //0e
    unsigned short mInSeq; //10
    unsigned short mOutSeq; //12
    Blowfish mKey; //14..10c8
    time_t mLastUseTime; //10c8, extent10cc
    virtual ~AuthSession(); //2e2a80
public:
    AuthSession(AuthType, unsigned short, Blowfish, unsigned char); //2e27b0/RET10c0
    WONStatus Encrypt(ByteBufferPtr&); //2e2b40
    WONStatus Decrypt(ByteBufferPtr&); //2e2d60
    time_t GetLastUseTime() const { return this->mLastUseTime; }
    const Blowfish& GetKey() const { return this->mKey; }
    AuthType GetAuthType() const { return this->mAuthType; }
    unsigned short GetSessionId() const { return this->mId; }
    void SetLengthFieldSize(unsigned char size) { this->mLengthFieldSize = size; }
    bool IsEncrypted() { return (this->mAuthType==AUTH_TYPE_SESSION || this->mAuthType==AUTH_TYPE_PERSISTENT); } //inline (WON SDK); same AuthSession+08 test as PeerAuthServerOp::Success
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

class ServerRequestOp : public ServerOp {
public:
    ServerRequestOp(const IPAddr&); //2ad050
    void Init(); //2acf10
    void SetServerContext(ServerContext*); //2ad2e0
    void SetAddr(const IPAddr&); //2ad340
    IPAddr GetAddr() const; //2ad410
    void FinishSaveSession(WONStatus); //2ad5b0
    void SetLastServerError(WONStatus); //2ad660
    bool TryConnect(); //2ad6b0
    bool TryPeerToPeerAuth(); //2ad950, out of line, the full PeerAuth op
    bool TryNextServer(); //2add80
    void QueueSocketOp(SocketOp*,DWORD); //2adae0, out of line
    WONStatus Send(); //2ae050
    WONStatus Recv(); //2ae2d0
    bool CheckExpiredSession(); //2ae200
    ServerRequestOp(ServerContext*); //2acf60
    const IPAddr& GetCurAddr() const { return this->mCurAddr; } //borrowed b0 getter
    virtual void RunHook();   //2ae390
    virtual void CleanupHook(); //2ae4c0
    virtual bool CallbackHook(AsyncOp*,int); //2adb10
    virtual WONStatus GetNextRequest(); //2ad020
    virtual WONStatus CheckResponse(); //2c7b10
    virtual void Reset();     //326b60
protected:
    WONStatus InvalidReplyHeader(); //2ad6a0, protected, 16 bytes; returns 100057
    virtual ~ServerRequestOp(); //2ad110, explicit own-vptr store
public:
    typedef std::list<IPAddr> AddrList;
    AddrList mAddrList; //94
    AddrList::iterator mAddrItr; //a0
    SmartPtr<AuthSession> mCurSession; //a4
    SmartPtr<AuthSession> mUsedSession; //a8
    bool mReusingSession; //ac
    bool mSessionExpired; //ad, natural alignmentae/af
    IPAddr mCurAddr; //b0..d8
    bool mUseAuth2; //d8
    bool mCloseConnection; //d9, natural alignmentda/db
    WONStatus mGetCertStatus; //dc
    bool mUseLastServerErrorForFinishStatus; //e0, natural alignmente1..e3
    WONStatus mLastServerError; //e4
    SmartPtr<ServerContext> mServerContext; //e8
    ByteBufferPtr mRequest; //ec
    ByteBufferPtr mResponse; //f0..f4
};

// 0xf8 bytes, RTTI/VT44d138, functions 2c5ff0/6150/6270.
class RoutingGetGroupListReqOp : public ServerRequestOp {
public:
    RoutingGetGroupListReqOp(const IPAddr&,unsigned char inFlags=0);
    virtual WONStatus GetNextRequest(); //GetNextRequest
    virtual WONStatus CheckResponse(); //CheckResponse
    SmartPtr<RoutingGetGroupListOp> mOp; //f4, implicit destructor releases before base
};

// AoC directory operation types. The base-game GetDirOp above is a different version.
// Reuses DirEntityList/SmartPtr from above; do not duplicate these aliases.
typedef std::set<std::string> DirDataTypeSet;
// WON SDK stateless parser; static cdecl 2f0510.
class DirEntityReplyParser {
public:
    static WONStatus ParseSingleEntityReply(const void*, unsigned long, DirEntityPtr&); //single-entity parser (WON SDK); no original address identified
    static WONStatus ParseMultiEntityReply(const void*, unsigned long, DirEntityList&, unsigned char* inSequence = NULL);
    static bool OnlyDirs(DWORD); //2f04f0
    static bool OnlyServices(DWORD); //2f0500
};

class GetEntityRequest : public RefCount {
protected:
    virtual ~GetEntityRequest(); //2f0da0, explicit ownVT44d4f0 store2f0dc2
    DWORD mFlags; //08, the inline default constructor sets flags 06020837
    std::wstring mPath; //0c..1c
    DirDataTypeSet mDataTypes; //1c..28
    DirEntityList mDirEntityList; //28..34
    DirEntityMap mDirEntityMap; //34..40
    WONStatus mStatus; //40..44; constructor leaves untouched
public:
    // The constructor is inline in AddPath 2f18f0 (no out-of-line address).
    // Never allocated or constructed here.
    GetEntityRequest() { this->mFlags=0x06020837; }
    enum PACKFLAG { Pack_GetDirOp, Pack_GetMultiDirOp };
    void SetPath(const std::wstring& inPath) { this->mPath=inPath; }
    // the query's fields, for the requests ee_won builds (rust/crates/ee_won/src/dirops.rs)
    DWORD GetFlags() const { return this->mFlags; }
    const std::wstring& GetPath() const { return this->mPath; }
    const DirDataTypeSet& GetDataTypes() const { return this->mDataTypes; }
    void SetFlags(DWORD inFlags) { this->mFlags=inFlags; }
    void SetDataTypes(const DirDataTypeSet& inTypes) { this->mDataTypes=inTypes; }
    void AddDataType(const std::string& type) { this->mDataTypes.insert(type); }
    void Reset(); //2f1070, clears result list/map only
    void Pack(WriteBuffer*,PACKFLAG); //2f0fa0
    WONStatus ParseMultiEntityReply(const void*,unsigned long); //2f1160, original parser only
    WONStatus GetStatus() { return this->mStatus; }
    const DirEntityList& GetDirEntityList() { return this->mDirEntityList; }
    const DirEntityMap& GetDirEntityMap(); //lazy grouping algorithm (WON SDK); no original address identified
};
typedef SmartPtr<GetEntityRequest> GetEntityRequestPtr;
// AoC GetDirOp, 0xf8 bytes: RTTI .?AVGetDirOp@WONAPI@@, VT44d4f8.
// Replaces the base game's 0x118-byte layout.
class GetDirOp : public ServerRequestOp {
private:
    void Init(); //2f1190, initializes the owned GetEntityRequest (0x44)
    GetEntityRequestPtr mGetEntityRequest; //f4..f8
protected:
    virtual WONStatus GetNextRequest(); //2f13d0, GetNextRequest
    virtual WONStatus CheckResponse(); //2f14e0, CheckResponse
    virtual void Reset();     //2f13c0, Reset
public:
    GetDirOp(ServerContext*); //2f12d0, 89 bytes; the only out-of-line constructor
    void SetPath(const std::wstring& path) { this->mGetEntityRequest->SetPath(path); }
    void SetFlags(DWORD flags) { this->mGetEntityRequest->SetFlags(flags); }
    void AddDataType(const std::string& type) { this->mGetEntityRequest->AddDataType(type); }
    const DirEntityList& GetDirEntityList() { return this->mGetEntityRequest->GetDirEntityList(); }
    // Implicit ordinary dtor 2f1350 releases the child, then ServerRequestOp; no own-vptr write.
};
typedef std::list<GetEntityRequestPtr> GetEntityRequestList;
class GetMultiDirOp : public ServerRequestOp {
protected:
    virtual WONStatus GetNextRequest(); //2f1a30
    virtual WONStatus CheckResponse(); //2f1b90
    virtual void Reset();     //2f1790
private:
    void Init(); //2f1520, protocol initialization
    GetEntityRequestList mRequestList; //f4..100
    GetEntityRequestList::iterator mCurRequest; //100=0
    DWORD mFlags; //104, the ctor leaves it untouched
    DirDataTypeSet mDataTypes; //108..114
    int mNumFailedRequests; //114=0, complete118
public:
    GetMultiDirOp(ServerContext* inContext); //2f1540, 207 bytes
    // Implicit destructor 2f1630/350, no own-vptr store.
    void SetFlags(DWORD inFlags) { this->mFlags=inFlags; }
    void AddPath(const std::wstring& inPath); //2f18f0
    void AddRequest(GetEntityRequest*); //2f17c0
    const GetEntityRequestList& GetRequestList() { return this->mRequestList; }
    // The inherited current address is at b0.
};

// The direct RTTI parent is ServerOp, not AsyncOpWithTracker or RoutingOp.
// The game uses no own fields; PeerAuthOp's own IPAddr begins at 94.
// Producer ServerConnection::Connect2abc40 new2244 at2abc81, ctor2e6a10 at2abcb3.
// Ctor calls ServerOp2acc10 at2e6a30; inherited pointer78 release/addref/write
// shown 2e6b10..2e6b61, source argument BlockingSocket* (as in the SDK header).
// PeerAuthClient: 0x2180 bytes, no vptr, two Blowfish (0x10b4) members.
class PeerAuthClient {
    ConstSmartPtr<AuthPeerData> mPeerData; //00
    AuthType mAuthType; //04, the constructor leaves it untouched
    Blowfish mSecretA; //08..10bc
    Blowfish mSecretB; //10bc..2170
    unsigned char mLengthFieldSize; //2170, the constructor leaves it untouched
    bool mUseClientSecret; //2171=false
    unsigned char mClientKeySize; //2172=8
    bool mUseAuth2; //2173=true
    SmartPtr<AuthCertificateBase> mServerCertificate; //2174, mutable (WON SDK)
    SmartPtr<AuthSession> mSession; //2178
public:
    enum PeerAuthClientState { STATE_NOT_STARTED=0, STATE_AWAITING_CHALLENGE=1, STATE_AWAITING_COMPLETE=2 } mState; //217c
private:
    WONStatus HandleChallenge1(ReadBuffer&,ByteBufferPtr&); //2e5cc0 RET8
    WONStatus GetChallenge2(ByteBufferPtr&); //2e5fb0 RET4
    WONStatus HandleComplete(ReadBuffer&); //2e61d0 RET4
public:
    ByteBufferPtr Start(const AuthPeerData*,AuthType,unsigned char); //2e6730 owning sret RET10
    WONStatus HandleRecvMsg(const void*,unsigned long,ByteBufferPtr&); //2e68d0 RET0c
    PeerAuthClient(); //2e5c30, 125 bytes
    void Reset(); //2e5cb0 resets the state
    AuthSession* GetSession() { return this->mSession.get(); }
    const AuthCertificateBase* GetServerCertificate() { return this->mServerCertificate.get(); }
    PeerAuthClientState GetState() { return this->mState; }
    void SetUseAuth2(bool value) { this->mUseAuth2=value; }
    void SetClientKeySize(unsigned char size,bool useSecret=true) { this->mClientKeySize=size;this->mUseClientSecret=useSecret; }
};
// RTTI PeerAuthOp > ServerOp > AsyncOpWithTracker > AsyncOp > RefCount.
// 0x2244 bytes; ctor/dtor/protocol are out of line.
// PeerAuthServer: 0x2184 bytes, no vptr or opaque storage.
class PeerAuthServer {
    ConstSmartPtr<AuthPeerData> mPeerData; //00
    AuthType mAuthType; //04, the constructor leaves it untouched
    Blowfish mSecretA; //08..10bc
    Blowfish mSecretB; //10bc..2170
    unsigned char mLengthFieldSize; //2170, the constructor leaves it untouched; natural alignment2171..2173
    SmartPtr<AuthCertificateBase> mClientCertificate; //2174, mutable SDK BasePtr
    SmartPtr<AuthSession> mSession; //2178
    bool mUseAuth2; //217c=true; natural alignment217d..217f
public:
    enum PeerAuthServerState { STATE_NOT_STARTED=0,STATE_AWAITING_REQUEST=1,STATE_AWAITING_CHALLENGE2=2 } mState; //2180=0
private:
    WONStatus GetChallenge1(ByteBufferPtr&); //2e7dd0 RET4, wire/encryption body
    WONStatus HandleRequest(ReadBuffer&,ByteBufferPtr&); //2e80a0 RET8, allocates the certificates
    WONStatus GetComplete(WONStatus,ByteBufferPtr&); //2e8300 RET8, allocates the AuthSession
    WONStatus HandleChallenge2(ReadBuffer&,ByteBufferPtr&); //2e8630 RET8
public:
    void Start(const AuthPeerData*,unsigned char); //2e7d30 RET8 protocol initializer
    WONStatus HandleRecvMsg(const void*,unsigned long,ByteBufferPtr&); //2e87c0 RET0c protocol dispatcher
    PeerAuthServer(); //2e7cc0, 106 bytes
    const AuthCertificateBase* GetClientCertificate() { return this->mClientCertificate.get(); }
    AuthSession* GetSession() { return this->mSession.get(); }
    PeerAuthServerState GetState() { return this->mState; }
    void SetUseAuth2(bool value) { this->mUseAuth2=value; }
};
// RTTI PeerAuthServerOp > ServerOp > AsyncOpWithTracker > AsyncOp > RefCount.
// 0x2240 bytes; protocol/constructor/destructor are out of line.
class PeerAuthServerOp : public ServerOp {
    bool CheckStatus(WONStatus); //2e8fa0 original boolRET4 status/Finish helper
    void AsyncRun(); //2e8fc0 original asynchronous receive setup
    void Success(); //2e9370 original session transform installation/completion
    virtual bool CallbackHook(AsyncOp*,int); //2e8d70 fourth slot
protected:
    virtual ~PeerAuthServerOp(); //2e8bc0, 422 bytes, own-vptr store then the members
    virtual void RunHook();   //2e9050 RunHook
    virtual void CleanupHook(); //2e7cb0 shared CleanupHook
public:
    PeerAuthServerOp(AuthContext*,BlockingSocket*); //2e8950, 230 bytes, RET8
    // Typed field view for the game's use; access and names inferred.
    IPAddr mConnectAddr; //94..bc, default IPAddr (0x28) constructed at 2b2350
    PeerAuthServer mPeerAuthServer; //bc..2240; clientCertificate2230/session2234/state223c
};

class PeerAuthOp : public ServerOp {
    IPAddr mConnectAddr; //94..bc, owned address/string (0x28)
    WONStatus mGetCertStatus; //bc, the constructor leaves it untouched
    ConstSmartPtr<AuthPeerData> mPeerData; //c0
    PeerAuthClient mPeerAuthClient; //c4..2244
    enum PeerAuthTrack { PeerAuth_Track_Socket=1, PeerAuth_Track_GetCert=2 };
    bool CheckStatus(WONStatus); //2e8fa0 ICF-shared bool/Finish helper, RET4
    void AsyncRetry(); //2e71a0
    void AsyncSend(const ByteBuffer*); //2e7240, RET4
    void AsyncConnect(); //2e7370
    void AsyncRefreshCert(); //2e74a0
    void AsyncRun(); //2e75f0
    void Success(); //2e7c40
    virtual bool CallbackHook(AsyncOp*,int); //2e6ed0 fourth slot, no new slots
protected:
    virtual ~PeerAuthOp(); //2e6d10, 437 bytes, own-vptr destructor
    virtual void RunHook();   //2e76d0 RunHook
    virtual void CleanupHook(); //2e7cb0 CleanupHook
public:
    PeerAuthOp(const IPAddr&,AuthContext*,AuthType,BlockingSocket*,bool=true); //2e6a10, 370 bytes, ret20
    AuthSession* GetSession() { return this->mPeerAuthClient.GetSession(); }
    BlockingSocket* GetSocket() { return this->mSocket.get(); }
    Auth2Certificate* GetServerCertificate() { return (Auth2Certificate*)this->mPeerAuthClient.GetServerCertificate(); }
    WONStatus GetGetCertStatus() const { return this->mGetCertStatus; }
};

// HTTP operation, 0x15c bytes.
class FileReader;
class FileWriter;
// Cache entry, 0x68 bytes; ctor 2d1260; implicit dtor 2d1330 (no own-vtable store).
class HTTPCacheEntry : public RefCount {
    friend class HTTPCache;
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
    void ReadFromFile(FileReader&); //2d1450
    void WriteToFile(FileWriter&); //2d14c0
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
class HTTPGetOp : public AsyncOpWithTracker {
private:
    enum TrackType { HTTP_Track_Connect=1,HTTP_Track_RecvStatus=2,HTTP_Track_RecvHeader=3,
        HTTP_Track_RecvContent=4,HTTP_Track_Redirect=5,HTTP_Track_RecvChunkLength=6,
        HTTP_Track_RecvChunkCRLF=7,HTTP_Track_RecvFooter=8,HTTP_Track_SendRequest=9 };
    bool ExtractHeaderLine(const char*); //2d47e0
    bool SendRequest(); //2d5270
    void AsyncRecvCRMsg(TrackType); //2d6170
    bool DoConnect(); //2d5090
    bool ExtractChunkLength(const char*); //2d6240
    bool RecvChunkLength(); //2d6430
    bool DoneRecvContent(RecvBytesOp*); //2d65c0
    void RecvContent(); //2d6730, full receiver
    void FinishRedirect(HTTPGetOp*); //2d6d90
    void FollowRedirect(); //2d6f80
    void DoneRecvHeader(); //2d7140
    static std::string mStaticProxyHost; //565fb0
public:
    time_t GetLastModifiedTime(); //2d7870, lazy file timestamp cache
    void SetSocket(BlockingSocket*); //2d77f0, owning assignment of member 78
    BlockingSocket* GetSocket(); //2d7850, returns NULL unless socket is connected
    void DoFinish(); //2d4370
    void SetProxy(const std::string&); //2d4390/325 (WON SDK)
    bool CheckStatus(WONStatus); //2d4550, forwards the status
    bool ExtractStatus(const char*); //2d4570
    bool ExtractRedirect(const char*); //2d45f0

    HTTPGetOp(const std::string& inURL); //2d3f90, called at 3ddd81
    void Init();
    HTTPGetOp(const std::string& inHost,const std::string& inPath); //2d3b10
    void SetRecvChunkCompletion(CompletionBase<AsyncOpPtr>*,DWORD); //2d44e0, 107-byte ownership setter
    static void ReadProxyHostFile(bool inForce=true); //2d3830
    static bool WriteProxyHostFile(const std::string&);
    static const std::string& GetStaticProxyHost();
    static void SetProxyHostFileName(std::string&);
    void SetProxy(const std::string&,unsigned short);
    virtual void RunHook();   //2d7330
    virtual void CleanupHook(); //2d7770
    virtual bool CallbackHook(AsyncOp* inOp,int inParam); //2d6a50
    // Implicit destructor: 2d3d50 (567 bytes) has no own-vptr store; member/base cleanup below.
    const std::string& GetLocalPath() { return this->mLocalPath; }
    const std::string& GetHost() {return this->mHost;}
    const std::string& GetRemotePath() {return this->mRemotePath;}
    const std::string& GetEndingHost() {return this->mRedirectHost.empty()?this->mHost:this->mRedirectHost;}
    const std::string& GetEndingPath() {return this->mRedirectPath.empty()?this->mRemotePath:this->mRedirectPath;}
    int GetHTTPStatus() {return this->mHTTPStatus;}
    void SetHTTPCache(HTTPCache* inCache) { this->mHTTPCache=inCache; }
    void SetLocalPath(const std::string& thePath) { this->mLocalPath=thePath; }
    void SetBinaryFile(bool isBinary) { this->mBinaryFile=isBinary; }
    void SetOnlySend() { this->mOnlySend=true; this->mOnlyRecv=false; }
    void SetOnlyRecv() { this->mOnlyRecv=true; this->mOnlySend=false; }
    void SetSendAndRecv() { this->mOnlySend=false; this->mOnlyRecv=false; }
    bool GetOnlySend() { return this->mOnlySend; }
    bool GetOnlyRecv() { return this->mOnlyRecv; }
    void SetDoRangeResume(bool doRangeResume) { this->mDoRangeResume=doRangeResume; }
    void SetCloseConnection(bool closeConnection) { this->mCloseConnection=closeConnection; }
    bool ContentIsNew() { return this->mContentIsNew; }
    SmartPtr<BlockingSocket> mSocket; //78
    SmartPtr<HTTPCache> mHTTPCache; //7c
    SmartPtr<HTTPCacheEntry> mHTTPCacheEntry; //80
    std::string mHost; //84
    std::string mRemotePath; //94
    std::string mLocalPath; //a4
    bool mIfRange; //b4
    bool mContentIsChunked; //b5
    bool mIsFirstChunk; //b6
    bool mBinaryFile; //b7
    bool mCloseConnection; //b8
    bool mDoClose; //b9
    bool mDoRangeResume; //ba,naturalpadbb
    std::string mProxyHost; //bc
    std::string mRedirectHost; //cc
    std::string mRedirectPath; //dc
    time_t mServerTime; //ec
    time_t mExpireTime; //f0
    time_t mLastModified; //f4
    time_t mLocalFileModifyTime; //f8
    size_t mLocalFileSize; //fc
    std::string mEntityTag; //100
    std::string mContentType; //110
    DWORD mContentLength; //120
    DWORD mChunkLength; //124
    ByteBufferPtr mContent; //128
    WriteBuffer mContentAccumulator; //12c..144
    int mHTTPStatus; //144
    bool mContentIsNew; //148,naturalpad149..14b
    OpCompletionBasePtr mRecvChunkCompletion; //14c
    DWORD mRecvChunkSize; //150
    bool mOnlySend; //154
    bool mOnlyRecv; //155
    bool mAutoRedirect; //156,naturalpad157
    int mNumRedirects; //158..15c
};

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

// 0xdc bytes. The implicit dtor cff46 is emitted by its users;
// it must not be replaced by an explicit empty or out-of-line dtor.
// vtable 44d378: delete 2d0320, Run 2d1070, Cleanup 2a0290, Callback 2d0810.
class GetHTTPDocumentOp : public AsyncOpWithTracker {
public:
    void SetDocType(HTTPDocType); //2d0340
    void DoFinish(); //2d06c0
    void OpFinished(HTTPGetOp*,int); //2d0700
    void TryHTTPOp(HTTPDocOwner,bool=false); //2d0840
    static void CheckHostAndPath(HTTPDocType); //2cffb0, static

    GetHTTPDocumentOp(const std::string& inProduct,HTTPDocType inType); //2d0170
    HTTPDocType GetDocType() { return this->mDocumentType; }
    void SetExtraConfig(const std::string& theExtraConfig) { this->mPath=theExtraConfig; }
    const std::string& GetDocumentPath(HTTPDocOwner theOwner) const { return this->mDocuments[theOwner]; }
    time_t GetModifiedTime(HTTPDocOwner theOwner) const { return this->mLastModified[theOwner]; }
    WONStatus GetDocumentStatus(HTTPDocOwner theOwner) { return this->mDocumentStatus[theOwner]; }
    void SetHTTPSession(HTTPSession* theSession) { this->mSession=theSession; }
    virtual void RunHook();                 //slot04,2d1070
    virtual bool CallbackHook(AsyncOp* inOp,int inParam); //slot0c,2d0810
    ByteBufferPtr GetDocument(HTTPDocOwner inOwner) const; //2d10e0
    HTTPDocType mDocumentType;              //78
    std::string mProduct;                   //7c
    std::string mPath;                      //8c
    HTTPSessionPtr mSession;                //9c
    bool mIsNew[2];                         //a0
    bool mRetried[2];                       //a2
    WONStatus mDocumentStatus[2];           //a4,ac
    int mHTTPStatus[2];                     //ac,b4
    time_t mLastModified[2];                //b4,bc; VC6 signed32-bit
    std::string mDocuments[2];              //bc,cc,dc; FILE PATHS
};


// the message of the day
class GetMOTDOp : public GetHTTPDocumentOp {
public:
    GetMOTDOp(const std::string& inProduct)
        : GetHTTPDocumentOp(inProduct, HTTPDocType_MOTD) {} //base 4c965b pushes 0
};

// WONAPI's CD key (dead in AoC; from the base game's Login: ctor base exe 0x68ab90, an 0x1c-byte local;
// implicit dtor 0x4cff2a: the SmartPtr at +0x14, the string at +4; provisional names)
// The full CDKey declaration comes earlier (EEWonNet), before AuthContext's mapped value.
// Storage, methods and inline virtual bodies live there; no duplicate class definition.


// AoC GetCertOp, 0x11fc bytes. Constructor/destructor/protocol are out of line.
// RTTI GetCertOp > ServerRequestOp > ServerOp > AsyncOpWithTracker > AsyncOp > RefCount.
class AuthPubKeyBlock;
class AuthPeerData;
class GetCertOp : public ServerRequestOp {
public:
    struct COMMUNITY_STATUS {
        short mStatus; //00
        unsigned long mCommunityID; //04
        std::wstring mCommunityName; //08..18, list node 0x20
    };
    typedef std::vector<std::string> ERROR_LIST;
    typedef std::list<COMMUNITY_STATUS> COMMUNITY_STATUS_LIST;
private:
    SmartPtr<AuthContext> mAuthContext; //f4
    Blowfish mSessionKey; //f8..11ac, Blowfish (0x10b4)
    ConstSmartPtr<AuthPubKeyBlock> mKeyBlock; //11ac
    time_t mAuthDelta; //11b0, the constructor leaves it untouched
    ConstSmartPtr<AuthPeerData> mPeerData; //11b4
    RawBuffer mChallengeSeed; //11b8..11c8
    std::wstring mNewPassword; //11c8..11d8
    bool mCreateAccount; //11d8, the constructor sets false
    enum State { GETTING_PUB_KEYS=1,GETTING_CHALLENGE=2,GETTING_CERTIFICATE=3 } mState; //11dc, ctor untouched
    ERROR_LIST mErrorList; //11e0..11f0
    COMMUNITY_STATUS_LIST mCommunityStatusList; //11f0..11fc
    WONStatus HandleLoginReply(ReadBuffer&); //2e4880 RET4; full reply parser
    WONStatus HandleLoginChallenge(ReadBuffer&); //2e5800 RET4
    WONStatus SendPubKeyRequest(); //2e4270 RET0
    WONStatus SendLoginRequest(); //2e4380 RET0
    WONStatus SendLoginConfirm(); //2e46a0 RET0
protected:
    virtual ~GetCertOp(); //2e3d90 own-vptr destructor, cleans up every field
    virtual void RunHook();   //2e3f40 RunHook
    virtual void CleanupHook(); //2e4050 CleanupHook
    // Inherited CallbackHook 0c = 2adb10, no added slot.
    virtual WONStatus GetNextRequest(); //2e4850 GetNextRequest
    virtual WONStatus CheckResponse(); //2e5980 CheckResponse
    virtual void Reset();     //2e4130 Reset
public:
    GetCertOp(AuthContext*); //2e3c30, 319 bytes, out of line
    const AuthPeerData* GetPeerData() const { return this->mPeerData; }
    void SetCreateAccount(bool value) { this->mCreateAccount=value; }
    void SetNewPassword(const std::wstring& password) { this->mNewPassword=password; }
    void SetAuthContext(AuthContext* context) { this->mAuthContext=context; }
    const Blowfish& GetSessionKey() const { return this->mSessionKey; }
    const ERROR_LIST& GetErrorList() { return this->mErrorList; }
    const COMMUNITY_STATUS_LIST& GetCommunityStatusList() { return this->mCommunityStatusList; }
};


class ProfileData : public RefCount {
public:
    ProfileData(); //2cd770, 150 bytes, initializes birth8/9/a=0, mapc, id18=0, data1c=NULL
    void ClearNewsletterSubs(); //2cdc20, clears the map
    void ClearNewsletterSub(unsigned short);
    void SetCustomAction(unsigned short,const ByteBuffer*);
    void ClearCustomAction();
    unsigned long GetBirthDate();
    void AddNewsletterSub(unsigned short inId, bool inSubscribe = false); //2cd9e0
    bool GetNewsletterSub(unsigned short inId); //2cdae0
    void PackRequestData(WriteBuffer&,bool); //2cdcc0
    void UnpackGetData(ReadBuffer&); //2cde20
    void SetBirthDate(unsigned char inMonth,unsigned char inDay,unsigned short inYear); //2ce070
    // the Rust profile (ee_won db.rs), for the requests that pack it
    const EeWonProfile* GetProfile() const { return this->mProfile; }
protected:
    virtual ~ProfileData(); //2cd830, explicit own-vtable store at 2cd852
private:
    ProfileData(const ProfileData&);
    ProfileData& operator=(const ProfileData&);
    static void Append(void*,const uint8_t*,size_t);
    // the birth date, the newsletter subscriptions and the custom action: a Rust object (ee_won db.rs)
    EeWonProfile* mProfile;
};
class DBProxyOp : public ServerRequestOp {
public:
    DBProxyOp(ServerContext* inContext); //2cca10,235B; dtor implicit224B2ccb20,noownVT
    DBProxyOp(const IPAddr&); //direct-address constructor (WON SDK)
    virtual WONStatus GetNextRequest(); //2ccc00
    virtual WONStatus CheckResponse(); //2ccd30
    virtual void Reset();     //4142a0
    short mMessageType; //f4
    short mMessageReplyType; //f6
    short mSubMessageType; //f8
    short mSubMessageReplyType; //fa
    ByteBufferPtr mRequestData; //fc
    WriteBuffer mReceivedData; //100..118
    ByteBufferPtr mReplyData; //118
    std::string mReplyErrorString; //11c..12c
};
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
class CheckValidVersionOp : public DBProxyOp {
    void Init(); //2d9380
protected:
    virtual ~CheckValidVersionOp(); //2d8d90/458 explicitownVTstore2d8db0
    virtual void RunHook();   //2d9250
    virtual WONStatus CheckResponse(); //2d8f60
private:
    std::string mProductName; //12c..13c
    SmartPtr<VersionData> mVersionData; //13c
    PatchDataList mPatchDataList; //140..14c
    bool mGetPatchList; //14c, naturalalignment14d..14f
    std::string mVersionTo; //150..160
    std::string mVersionToDescriptionURL; //160..170
    const unsigned short mMsgType; //170=1, naturalalignment172..173,full174
public:
    CheckValidVersionOp(const std::string& inProduct,ServerContext* inContext); //2d8c80
    void SetConfigName(const std::string& inName) { this->mVersionData->SetConfigName(inName); }
    void SetVersion(const std::string& inVersion) { this->mVersionData->SetVersion(inVersion); }
};
class DetectFirewallOp : public ServerRequestOp {
protected:
    void Init(); //2cf5c0, constructor called at 2cf651
    virtual void RunHook();   //2cf760
    virtual void CleanupHook(); //2cf920
    virtual bool CallbackHook(AsyncOp*,int); //2cf8b0
    virtual WONStatus GetNextRequest(); //2cf970
    virtual WONStatus CheckResponse(); //2cfad0
    virtual WONStatus RecvServerDatagram(); //2cfba0, eighth slot SetupReceiveTracker
private:
    unsigned short mListenPort; //f4, naturalalignmentf6/f7
    unsigned long mMaxConnectWaitTime; //f8
    SmartPtr<BlockingSocket> mListenSocket; //fc
    bool mDoListen; //100
    bool mWaitForConnect; //101
    bool mUseUDP; //102,naturalalignment103
    SmartPtr<RecvBytesFromOp> mRecvBytesFromOp; //104, 90 bytes
    IPAddr mPublicAddr; //108..130,full130
public:
    DetectFirewallOp(ServerContext* inContext); //2cf600
    // The 208-byte dtor 2cf690 has NO own-vptr store; it stays implicit.
    void SetListenPort(unsigned short inPort) { this->mListenPort=inPort; }
    void SetDoListen(bool inDoListen) { this->mDoListen=inDoListen; }
    void SetWaitForConnect(bool inWait) { this->mWaitForConnect=inWait; }
    IPAddr& GetPublicAddr() { return this->mPublicAddr; }
};

class RequestPasswordEmailOp : public DBProxyOp {
public:
    RequestPasswordEmailOp(ServerContext* inContext); //2ce480,126B
    virtual void RunHook();   //2ce670, RunHook
    virtual WONStatus CheckResponse(); //2ce280,shared CheckResponse
protected:
    void Init(); //163 bytes, length84=4, then a new Profile (0x20)
    virtual ~RequestPasswordEmailOp(); //2ce520,329B, explicit ownVT2ce540
public:
    void SetUserName(const std::wstring& inUser) { this->mUserName=inUser; }
    std::wstring mUserName; //12c..13c
    SmartPtr<ProfileData> mProfileData; //13c
    const unsigned short mMsgType; //140=8,naturalpadding142..143
};
class RequestAccountInfoEmailOp : public DBProxyOp {
public:
    RequestAccountInfoEmailOp(ServerContext* inContext); //2ce090,126B
    virtual void RunHook();   //2ce2b0
    virtual WONStatus CheckResponse(); //2ce280
protected:
    void Init(); //163 bytes, length84=4, then a new Profile (0x20)
    virtual ~RequestAccountInfoEmailOp(); //2ce130,328B,explicit ownVT2ce150
public:
    void SetEmail(const std::string& inEmail) { this->mEmail=inEmail; }
    std::string mEmail; //12c..13c
    SmartPtr<ProfileData> mProfileData; //13c
    const unsigned short mMsgType; //140=14,naturalpadding142..143
};

// GetUserIdFromNameOp, 0x140 bytes; ctor 2cd3a0 / destructor 2cd4d0,
// RTTI44d2b8 six inherited operation slots plus Reset18; no additional slots.
class GetUserIdFromNameOp : public DBProxyOp {
protected:
    virtual ~GetUserIdFromNameOp(); //2cd4d0 explicit ownVT44d2b8
    virtual void RunHook();   //2cd600, serialized lookup request
    virtual WONStatus CheckResponse(); //2cd700, vtable-derived response
public:
    GetUserIdFromNameOp(ServerContext*); //2cd3a0
    void SetUserName(const std::wstring& v) { this->mUserName=v; }
    unsigned long GetUserId() const { return this->mUserId; }
    std::wstring mUserName; //12c
    unsigned long mUserId; //13c, constructorzero
};

// Account creator, 0x1398 bytes. Embedded ElGamal 108..2b0 and Blowfish 2b0..1364.
class CreateAccountOp : public ServerRequestOp {
    SmartPtr<ProfileData> mProfileData; //f4
    std::string mErrorString; //f8
    ElGamal mPubKey; //108,8aligned
    Blowfish mSymKey; //2b0
    enum State { State_GettingPubKey,State_SettingSessionKey,State_CreatingAccount } mState; //1364
    std::wstring mUserName; //1368
    std::wstring mPassword; //1378
    std::string mEmail; //1388..1398
    void Init(); //2cbe70
protected:
    virtual WONStatus GetNextRequest(); //2cc4f0
    virtual WONStatus CheckResponse(); //2cc8d0,vtable44d254 slot14
    virtual void Reset();     //2cc4e0, Reset
public:
    CreateAccountOp(ServerContext*); //2cbf20
    void SetProfileData(ProfileData* inProfile) {this->mProfileData=inProfile;}
    void SetUserName(const std::wstring& inUser) {this->mUserName=inUser;}
    void SetPassword(const std::wstring& inPassword) {this->mPassword=inPassword;}
    void SetEmail(const std::string& inEmail) {this->mEmail=inEmail;}
    const std::wstring& GetUserName() const {return this->mUserName;}
    const std::wstring& GetPassword() const {return this->mPassword;}
    const std::string& GetEmail() const {return this->mEmail;}
    const std::string& GetErrorString() const {return this->mErrorString;}
    ProfileData* GetProfileData() {return this->mProfileData.get();}
    // Implicit ordinary dtor 2cc2e0, no own-vtable store; all members destroyed normally.
};

class GetAccountOp : public ServerRequestOp {
public:
    GetAccountOp(ServerContext* inContext,AuthContext* inAuth); //2ccfa0
    void Init(); //2ccef0, 163 bytes, length4 before a new Profile (0x20)
    // Destructor stays implicit: 2cd0a0 has no own-vptr store; releases 114,
    // string 104 then string f4, calls ServerRequestOp::~ServerRequestOp 2ad110.
    virtual WONStatus GetNextRequest();     //2cd190 GetNextRequest
    virtual WONStatus CheckResponse();      //2cd2b0 CheckResponse
    virtual void Reset();                   //4142a0 Reset, override
    std::string mEmail;                     //f4..104
    std::string mErrorString;               //104..114
    SmartPtr<ProfileData> mProfileData;      //114..118
};
class UpdateAccountOp : public ServerRequestOp {
public:
    UpdateAccountOp(ServerContext* inContext,AuthContext* inAuth); //2ce8f0
    // Destructor remains implicit:2cea10 has no own-vptr store; string118,
    // wstring108,stringf8,ProfilePtrf4, then ServerRequestOp::~2ad110.
    virtual WONStatus GetNextRequest();     //2ceb30 GetNextRequest
    virtual WONStatus CheckResponse();      //2cec80 CheckResponse
    virtual void Reset();                   //4142a0 Reset, override
    void SetEmail(const std::string& inEmail) { this->mEmail=inEmail; }
    void SetNewPassword(const std::wstring& inPassword) { this->mNewPassword=inPassword; }
    SmartPtr<ProfileData> mProfileData;      //f4
    std::string mErrorString;               //f8..108
    std::wstring mNewPassword;              //108..118
    std::string mEmail;                     //118..128
private:
    void Init(); //2ce840, 163 bytes, length4 before a new Profile (0x20)
};

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

class ServerConnection : public BlockingSocket {
public:
    void StartKeepAlive(); //2aaf10
    static void StaticConnectCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab160
    static void StaticMsgCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab6a0
    static void StaticKeepAliveCallback(AsyncOpPtr,SmartPtr<RefCount>); //2ab7b0
    void KeepAliveCallback(AsyncOp*); //2ab840
    void SendNoOp(); //2ab910
    ServerConnection(); //2aab10; implicit ordinary dtor2aabe0, no explicit body
    virtual void CloseHook(); //CloseHook2abe70
    virtual void KillHook();  //KillHook2abf50
    virtual void ConnectCallback(PeerAuthOp*); //ConnectCallback2ab1f0, ret4
    virtual void MsgCallback(const ByteBuffer*); //MsgCallback2fa500,ret4
    void SetAuth(AuthContext*, AuthType);  //SetAuth
    PeerAuthOp* ConnectAsync(const IPAddr&, unsigned long); //2abc40; raw borrowed member result
    WONStatus ConnectBlock(const IPAddr&,DWORD inTimeout=0xffffffff);
    void SetKeepAlivePeriod(DWORD);
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>*); //2abdc0
    SmartPtr<AuthContext> mAuthContext; //a0; ctor2aab35zero, dtor owned
    AuthType mAuthType; //a4; scalar32, never decref
    SmartPtr<PeerAuthOp> mPeerAuthOp; //a8, owned
    OpCompletionBasePtr mConnectCompletion; //ac, owned
    AsyncOpPtr mKeepAliveOp; //b0, owned plainAsyncOp44
    unsigned long mKeepAlivePeriod; //b4=300, sizeb8
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

class RoutingConnection : public ServerConnection {
public:
    void AddOp(RoutingOp*); //2ba630, AutoCrit/list ownership
    void CalcCompletion(RoutingOp*); //2ba740, type-index/default completion
    SmartPtr<RoutingOp> GetUnsolicitedOp(unsigned char,bool); //2ba830, sret/ret0c; call-only
    RoutingConnection(); //2ba050; allocation1c0 at3eddd4
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
    virtual RoutingClientInfoPtr GetNewClientInfo(); //GetNewClientInfo2ba100; owned sret ret4
    virtual RoutingMemberInfoPtr GetNewMemberInfo(); //GetNewMemberInfo2ba230; owned sret ret4
    virtual RoutingGroupInfoPtr GetNewGroupInfo(); //GetNewGroupInfo2ba300; owned sret ret4
    virtual SmartPtr<RoutingOp> GetUnsolicitedDerivedOp(unsigned char); //GetUnsolicitedDerivedOp2bc6c0, sret+arg ret8
    virtual void RoutingOpCompleteHook(RoutingOp*) {} //inline RoutingOpCompleteHook (WON SDK), shared 30a800 ret4
    // 04/10/14 are inherited unchanged; no generic vtable padding or aliases.
    typedef std::list<SmartPtr<RoutingOp> > RoutingOpList;
    RoutingOpList mOpList; //b8 allocator/headbc/countc0; owns elements
    bool mAllDerivedOpsAreUnsolicited; //c4
    OpCompletionBasePtr mCompletions[61]; //c8..1b8, 61-element vector built by the ctor
    OpCompletionBasePtr mDefaultCompletion; //1bc; extent1c0
protected:
    virtual ~RoutingConnection(); //ordinary2ba4b0; explicit own-vptr store
};

// IgnoreClient 2c3848/2c38a5 both call 2a6b60; a negative compare orders the tree.

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
struct ClientInfo
	{
		std::wstring mName;
		unsigned short mId;
		unsigned long mFlags;
		unsigned long mCDKeyId;

		ClientInfo() : mId(0), mFlags(0), mCDKeyId(0) {}
	};
struct MemberInfo
	{
		std::wstring mName;
		unsigned short mClientId;
		unsigned long mClientFlags;
		unsigned char mMemberFlags;

		MemberInfo() : mClientId(0), mClientFlags(0), mMemberFlags(0) {}
	};
struct GroupInfo
	{
		std::wstring mName;
		unsigned short mId;
		unsigned short mCaptainId;
		unsigned short mMaxPlayers;
		unsigned long mFlags;
		unsigned long mAsyncFlags;
		unsigned short mObserverCount; // only applicable if non-detailed observers is on

		GroupInfo() : mId(0), mCaptainId(0), mMaxPlayers(0), mFlags(0), mAsyncFlags(0), mObserverCount(0) {}
	};
    RoutingMemberInfoPtr GetMemberRef(unsigned short,int=-1);
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
    SmartRoutingConnection(); //2bfb00; the 0x258 allocation is this class
    // Destructor stays implicit: ordinary2c0130 lacks own-vptr store, natural member cleanup.
    virtual void CloseHook(); //CloseHook2c4120
    virtual RoutingClientInfoPtr GetNewClientInfo(); //2bfe60 SAME base smartptr result, not covariantSP
    virtual void RoutingOpCompleteHook(RoutingOp*); //2c0be0, calls slot14 for the op type
    void InitHandlerVariables(RoutingClientInfo*); //2c0a10
    void InitHandlerVariables(RoutingGroupInfo*,RoutingMemberInfo* =NULL); //2c0ac0
    void HandleClientFlagsChanged(RoutingClientFlagsChangedOp*); //2c24f0
    void HandleGroupCaptainChanged(RoutingGroupCaptainChangedOp*); //2c2860
    void HandleGroupMaxPlayersChanged(RoutingGroupMaxPlayersChangedOp*); //2c2900
    void HandleGroupFlagsChanged(RoutingGroupFlagsChangedOp*); //2c2bb0
    void HandleGroupNameChanged(RoutingGroupNameChangedOp*); //2c2c50
    void HandleGroupObserverCount(RoutingGroupObserverCountOp*); //2c2d00
    void HandleMemberFlagsChanged(RoutingMemberFlagsChangedOp*); //2c2da0
    RoutingClientInfoPtr GetClient(unsigned short); //2c0330, owned BY VALUE
    RoutingGroupInfoPtr GetGroup(unsigned short); //2c0450, owned BY VALUE
    void AddCommandString(const std::wstring&, InputCommand); //2c3970
    void AddDefaultUserCommandStrings(); //2c3a50
    void AddDefaultModeratorCommandStrings(); //2c3d90
    void SetAllowPartialNameMatch(bool); //2c40a0, locked, enum 2/3
    void SetServerWideCommands(bool); //2c40f0
    void EraseClient(const RoutingClientMap::iterator&); //2c0570
    void EraseClient(unsigned short); //2c0600
    void EraseGroup(const RoutingGroupMap::iterator&); //2c0670
    void EraseMember(RoutingMemberMap&,const RoutingMemberMap::iterator&); //2c0740
    void SetClientName(RoutingClientInfo*,const std::wstring&); //2c07d0
    SmartRoutingClientInfo* EnsureInClientMap(unsigned short); //2c0860, borrowedraw
    void HandleGetClientList(RoutingGetClientListOp*); //2c1130
    void SyncMemberMapWithClientMap(RoutingMemberMap&,const RoutingMemberMap&); //2c1bd0
    void SyncMemberMapWithClientMap(RoutingMemberMap&); //2c1f20
    void HandleGetGroupList(RoutingGetGroupListOp*); //2c2070
    void SyncClientMapWithIgnoreList(); //2c2740
    void HandleRegisterClient(RoutingRegisterClientOp *theOp);
    void HandleSetClientFlags(RoutingSetClientFlagsOp *theOp);
    void HandleReadDataObject(RoutingReadDataObjectOp *theOp);
    void HandleDataObjectCreated(RoutingDataObjectCreatedOp *theOp);
    void HandleDataObjectModified(RoutingDataObjectModifiedOp *theOp);
    void SetClientCDKeyIdFromDataObject(unsigned short theClientID, ByteBufferPtr theBuf);
    void HandleClientJoinedGroup(RoutingClientJoinedGroupOp *theOp);
    void HandleClientLeftGroup(RoutingClientLeftGroupOp *theOp);
    void HandleGetMembersOfGroup(RoutingGetMembersOfGroupOp *theOp);
    void HandleJoinGroup(RoutingJoinGroupOp *theOp);
    void HandleClientJoinedServer(RoutingClientJoinedServerOp *theOp);
    void HandleClientLeftServer(RoutingClientLeftServerOp *theOp);
    void HandleGroupCreated(RoutingGroupCreatedOp *theOp);
    void HandleGroupDeleted(RoutingGroupDeletedOp *theOp);
    void CheckIgnore(RoutingOp *theOp, unsigned short theSenderId);
    void HandlePeerChat(RoutingPeerChatOp *theOp);
    void HandleGroupInvitation(RoutingGroupInvitationOp *theOp);
    void HandleGroupJoinAttempt(RoutingGroupJoinAttemptOp *theOp);
    void HandleLeaveGroup(RoutingLeaveGroupOp *);
    void HandleDataObjectDeleted(RoutingDataObjectDeletedOp	*theOp);
    MatchResult FindClient(const std::wstring&, RoutingClientInfoPtr&); //2c35a0
    MatchResult GetMemberByNameRef(RoutingGroupInfoPtr,const std::wstring&,RoutingMemberInfoPtr&);
    MatchResult GetMemberByNameRef(unsigned short,const std::wstring&,RoutingMemberInfoPtr&);
    bool HandleInput(const std::wstring&,int=-1);
    void ClearCommandStrings();
    void SetDoColonEmote(bool);
protected:
    bool HandleCommand(const std::wstring&,int);
public:
    void IgnoreClient(RoutingClientInfo*, bool); //2c37d0 RAW pointer, not SmartPtr&
    void LoadIgnoreSet(); // 2c3110
    void SaveIgnoreSet(); // 2c3460
    void SetProductName(const std::string&); //2c2fc0, IgnoreFilePath setter (provisional name)
    void ClearProductName() { this->SetProductName(""); } //protected helper, the ignore path setter
    const RoutingGroupMap& GetGroupMap() const { return this->mGroupMap; }
    unsigned short GetMyClientId() const { return this->mClientId; }  // inline (OnChat 0x4d040c: loaded into si first)
    RoutingGroupMap mGroupMap; //1c0..1cb
    RoutingClientMap mClientMap; //1cc..1d7
    RoutingClientMap::iterator mClientItr; //1d8
    RoutingGroupMap::iterator mGroupItr; //1dc
    RoutingMemberMap::iterator mMemberItr; //1e0
    RoutingGroupInfoPtr mMemberItrGroup; //1e4, CORRECT group ptr (not member ptr)
    unsigned short mDefaultGroupId; //1e8
    RoutingClientInfoPtr mMyClient; //1ec
    unsigned short mClientId; //1f0, mMyClientId (name kept for old readers)
    unsigned long mAsyncFlags; //1f4
    typedef std::map<std::wstring,InputCommand> CommandStringMap;
    CommandStringMap mCommandStringMap; //1f8..203
    MatchResult mMatchTolerance; //204
    bool mServerWideCommands; //208
    bool mDoColonEmote; //209
    InputError mLastInputError; //20c
    InputCommand mLastInputCommand; //210
    RoutingMemberInfoPtr mLastTargetMember; //214
    RoutingClientInfoPtr mLastTargetClient; //218
    std::wstring mLastMatchName; //21c..22b
    RoutingClientInfoPtr mHandlerClient; //22c, OWNING
    RoutingGroupInfoPtr mHandlerGroup; //230, OWNING
    RoutingMemberInfoPtr mHandlerMember; //234, OWNING
    bool mReadClientCDKeyId; //238
    typedef std::set<std::wstring,WStringLessNoCase> IgnoreSet;
    IgnoreSet mIgnoreSet; //23c..247; comparator2a6b60
    std::string mIgnoreFilePath; //248..257
};

// No class here duplicates the old flattened RoutingConnection (0x258).
// No zero-valued gap fields or placement/partial object lifetime is introduced.

typedef SmartPtr<RoutingConnection> RoutingConnectionPtr;
typedef SmartPtr<SmartRoutingConnection> SmartRoutingConnectionPtr;


}

#endif
