// ServerConnection: persistent authenticated server socket with keep-alive and message callbacks. A Rust object
// since net round 16 (rust/crates/ee_won/src/servconn.rs EeWonServerConn: the auth, the peer authentication op, the
// connect completion, the keep-alive timer and period, and every step); the C++ class keeps the objects' calls the
// Rust object makes (the hooks below) and ServerConnectionConnectOp
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"
namespace WONAPI {
// Every object crosses as its RefCount*
static RefCount* Ref(void* inObject) { return static_cast<RefCount*>(inObject); }
static AsyncOp* Op(void* inObject) { return static_cast<AsyncOp*>(Ref(inObject)); }
static ServerConnection* Conn(void* inConn) { return static_cast<ServerConnection*>(inConn); }
typedef ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> > ConnCompletion;

static void HookAddRef(void* inObject) { Ref(inObject)->AddRef(); }
static void HookRelease(void* inObject) { Ref(inObject)->Release(); }
static long HookOpStatus(void* inOp) { return Op(inOp)->GetStatus(); }
static bool HookOpKilled(void* inOp) { return Op(inOp)->IsKilled(); }
static void HookOpKill(void* inOp) { Op(inOp)->Kill(true); }
static void HookOpSetCompletion(void* inOp,void* inCompletion)
{ Op(inOp)->SetCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(Ref(inCompletion))); }
static void HookOpRunAsync(void* inOp,long inTimeout) { Op(inOp)->RunAsync(inTimeout); }
static void HookOpRunBlock(void* inOp,uint32_t inTimeout) { Op(inOp)->RunBlock(inTimeout); }
static void HookComplete(void* inCompletion,void* inOp)
{ static_cast<CompletionBase<AsyncOpPtr>*>(Ref(inCompletion))->Complete(AsyncOpPtr(Op(inOp))); }
static void* HookNewPeerAuth(void* inConn,const void* inAddr,void* inContext,int32_t inAuthType)
{
    PeerAuthOp* anOp=new PeerAuthOp(*static_cast<const IPAddr*>(inAddr),static_cast<AuthContext*>(Ref(inContext)),
                                    static_cast<AuthType>(inAuthType),Conn(inConn),true);
    return static_cast<RefCount*>(anOp);
}
static void HookRouteConnect(void* inConn,void* inOp)
{
    Op(inOp)->SetCompletion(new ConnCompletion(ServerConnection::StaticConnectCallback,
                                               SmartPtr<RefCount>(Conn(inConn))));
}
static void* HookNewKeepAlive(void* inConn)
{
    AsyncOp* anOp=new AsyncOp;
    anOp->SetCompletion(new ConnCompletion(ServerConnection::StaticKeepAliveCallback,SmartPtr<RefCount>(Conn(inConn))));
    return static_cast<RefCount*>(anOp);
}
static void HookStartMessages(void* inConn)
{
    Conn(inConn)->SetRepeatCompletion(new ConnCompletion(ServerConnection::StaticMsgCallback,
                                                         SmartPtr<RefCount>(Conn(inConn))));
    Conn(inConn)->SetRepeatOp(new RecvMsgOp);
}
static void HookStopMessages(void* inConn) { Conn(inConn)->SetRepeatCompletion(NULL); }
static void HookQueueMessage(void* inConn,const uint8_t* inBytes,size_t inLength)
{
    ByteBufferPtr aMsg=new ByteBuffer(reinterpret_cast<const char*>(inBytes),(unsigned long)inLength,false);
    Conn(inConn)->QueueOp(new SendMsgOp(aMsg.get()),-1);
}
static void HookClose(void* inConn) { Conn(inConn)->Close(); }
static uint8_t HookLengthFieldSize(void* inConn) { return Conn(inConn)->GetLengthFieldSize(); }
static void HookQueueCloseHook(void* inConn) { Conn(inConn)->QueueSocket::CloseHook(); }
static void HookQueueKillHook(void* inConn) { Conn(inConn)->QueueSocket::KillHook(); }
static const EeWonServerConnHooks kHooks={HookAddRef,HookRelease,HookOpStatus,HookOpKilled,HookOpKill,
    HookOpSetCompletion,HookOpRunAsync,HookOpRunBlock,HookComplete,HookNewPeerAuth,HookRouteConnect,HookNewKeepAlive,
    HookStartMessages,HookStopMessages,HookQueueMessage,HookClose,HookLengthFieldSize,HookQueueCloseHook,
    HookQueueKillHook};

// FUNCTION: WON 0x006aab10
// Creates an unauthenticated server connection with a 300-second keep-alive
ServerConnection::ServerConnection() : BlockingSocket(0)
{
    this->mConn=ee_won_servconn_new(static_cast<void*>(this),&kHooks,static_cast<void*>(this->GetDataLock()));
}
// Releases the timer, the connect completion, the peer authentication op and the auth context
ServerConnection::~ServerConnection() { ee_won_servconn_free(this->mConn); }
// FUNCTION: WON 0x006aaf10
// Restarts the keep-alive timer (none when the period is 0)
void ServerConnection::StartKeepAlive() { ee_won_servconn_start_keep_alive(this->mConn); }
// FUNCTION: WON 0x006ab0a0
// Sets the auth context and auth type used when connecting
void ServerConnection::SetAuth(AuthContext* inContext,AuthType inType)
{
    ee_won_servconn_set_auth(this->mConn,static_cast<RefCount*>(inContext),inType);
}
// FUNCTION: WON 0x006ab160
// Forwards the peer auth completion to its connection
void ServerConnection::StaticConnectCallback(AsyncOpPtr inOp,SmartPtr<RefCount> inParam)
{
    ((ServerConnection*)inParam.get())->ConnectCallback((PeerAuthOp*)inOp.get());
}
// FUNCTION: WON 0x006ab1f0
// Reports the connect result; on success starts receiving messages and the keep-alive timer
void ServerConnection::ConnectCallback(PeerAuthOp* inOp)
{
    ee_won_servconn_connect_callback(this->mConn,static_cast<RefCount*>(static_cast<AsyncOp*>(inOp)));
}
// FUNCTION: WON 0x006ab6a0
// Passes each successfully received message to the connection's MsgCallback
void ServerConnection::StaticMsgCallback(AsyncOpPtr inOp,SmartPtr<RefCount> inParam)
{
    RecvMsgOp* anOp=(RecvMsgOp*)inOp.get();
    ServerConnection* aConnection=(ServerConnection*)inParam.get();
    if(anOp->GetStatus()==WS_Success) aConnection->MsgCallback(anOp->GetMsg().get());
}
// FUNCTION: WON 0x006ab7b0
// Forwards the keep-alive timer completion to its connection
void ServerConnection::StaticKeepAliveCallback(AsyncOpPtr inOp,SmartPtr<RefCount> inParam)
{
    ((ServerConnection*)inParam.get())->KeepAliveCallback(inOp.get());
}
// FUNCTION: WON 0x006ab840
// Sends a no-op message and restarts the timer when the current keep-alive timer fires
void ServerConnection::KeepAliveCallback(AsyncOp* inOp)
{
    ee_won_servconn_keep_alive_callback(this->mConn,static_cast<RefCount*>(inOp));
}
// FUNCTION: WON 0x006ab910
// Queues a no-op message to keep the connection alive
void ServerConnection::SendNoOp() { ee_won_servconn_send_no_op(this->mConn); }
// FUNCTION: WON 0x006abdc0
// Sets the completion called when a connect finishes
void ServerConnection::SetConnectCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    ee_won_servconn_set_connect_completion(this->mConn,static_cast<RefCount*>(inCompletion));
}
// FUNCTION: WON 0x006abe70
// Stops the keep-alive timer and a pending connect, drops the message callback and closes the socket
void ServerConnection::CloseHook() { ee_won_servconn_close_hook(this->mConn); }
// FUNCTION: WON 0x006abf50
// Kills the socket and drops the connect completion and auth context
void ServerConnection::KillHook() { ee_won_servconn_kill_hook(this->mConn); }
// FUNCTION: WON 0x006abc40
// Closes the socket and starts an asynchronous authenticated connect; returns the peer auth op
PeerAuthOp* ServerConnection::ConnectAsync(const IPAddr& address,unsigned long timeout)
{
    void* anOp=ee_won_servconn_connect_async(this->mConn,static_cast<const void*>(&address),(long)timeout);
    return static_cast<PeerAuthOp*>(Op(anOp));
}
// No original address identified: WON SDK methods
void ServerConnection::SetKeepAlivePeriod(DWORD theSeconds)
{
    ee_won_servconn_set_keep_alive_period(this->mConn,theSeconds);
}
// Closes the socket, connects blocking, then reports it as the connect callback does; returns its status
WONStatus ServerConnection::ConnectBlock(const IPAddr &theAddr, DWORD theTimeout)
{
    return ee_won_servconn_connect_block(this->mConn,static_cast<const void*>(&theAddr),theTimeout);
}

// FUNCTION: WON 0x006abfc0
// Creates an op that connects a server connection to an address
ServerConnectionConnectOp::ServerConnectionConnectOp(ServerConnection* inConnection,const IPAddr& inAddr)
    : mConnection(inConnection),mAddr(inAddr)
{
}
// FUNCTION: WON 0x006ac240
// Forwards the connect completion to its op
void ServerConnectionConnectOp::StaticConnectCompletion(AsyncOpPtr inOp,SmartPtr<RefCount> inParam)
{
    ((ServerConnectionConnectOp*)inParam.get())->ConnectCompletion(inOp.get());
}
// FUNCTION: WON 0x006ac2d0
// Finishes the pending op with the connect status
void ServerConnectionConnectOp::ConnectCompletion(AsyncOp* inOp)
{
    if(this->GetStatus()!=WS_Pending) return;
    this->Finish(inOp->GetStatus());
}
// FUNCTION: WON 0x006ac2f0
// Starts an asynchronous connect, completing the op when it finishes
void ServerConnectionConnectOp::RunHook()
{
    this->mConnection->SetConnectCompletion(new ConnCompletion(ServerConnectionConnectOp::StaticConnectCompletion,
                                                               SmartPtr<RefCount>(this)));
    this->mConnection->ConnectAsync(this->mAddr,this->GetTimeout());
}
// FUNCTION: WON 0x006ac380
// Removes the op's connect completion from the connection
void ServerConnectionConnectOp::CleanupHook()
{
    this->mConnection->SetConnectCompletion(NULL);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ac1a0
// (public: virtual __thiscall WONAPI::ServerConnectionConnectOp::~ServerConnectionConnectOp(void)    [??1ServerConnectionConnectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006aeab0
// (public: virtual void * __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount>)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aabc0
// (public: virtual void * __thiscall WONAPI::ServerConnection::`scalar deleting destructor'(unsigned int)    [??_GServerConnection@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006aabe0
// (public: virtual __thiscall WONAPI::ServerConnection::~ServerConnection(void)    [??1ServerConnection@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006abab0
// (public: virtual void * __thiscall WONAPI::SendBytesOp::`scalar deleting destructor'(unsigned int)    [??_GSendBytesOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006abb60
// (public: virtual void * __thiscall WONAPI::SendMsgOp::`scalar deleting destructor'(unsigned int)    [??_GSendMsgOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ac180
// (public: virtual void * __thiscall WONAPI::ServerConnectionConnectOp::`scalar deleting destructor'(unsigned int)    [??_GServerConnectionConnectOp@WONA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae7e0
// (public: __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >::ParamComple)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae840
// (public: virtual void __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >)
// TEMPLATE: WON 0x006aead0
// (public: virtual __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >::~Pa)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aad90
// (public: virtual __thiscall WONAPI::BlockingSocket::~BlockingSocket(void)    [??1BlockingSocket@WONAPI@@UAE@XZ])
