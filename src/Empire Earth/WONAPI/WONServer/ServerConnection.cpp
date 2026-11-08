// ServerConnection: persistent authenticated server socket with keep-alive and message callbacks
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006aab10
// Creates an unauthenticated server connection with a 300-second keep-alive
ServerConnection::ServerConnection() : BlockingSocket(0)
{
    this->mAuthType=AUTH_TYPE_NONE;
    this->mKeepAlivePeriod=300;
}
// FUNCTION: WON 0x006aaf10
// Restarts the keep-alive timer (none when the period is 0)
void ServerConnection::StartKeepAlive()
{
    AutoCrit aCrit(this->mDataCrit);
    if(this->mKeepAliveOp.get()!=NULL) {
        this->mKeepAliveOp->Kill(true);
        this->mKeepAliveOp=NULL;
    }
    if(this->mKeepAlivePeriod!=0) {
        this->mKeepAliveOp=new AsyncOp;
        this->mKeepAliveOp->mCompletion=new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
            ServerConnection::StaticKeepAliveCallback,SmartPtr<RefCount>(this));
        this->mKeepAliveOp->RunAsync(this->mKeepAlivePeriod*1000);
    }
}
// FUNCTION: WON 0x006ab0a0
// Sets the auth context and auth type used when connecting
void ServerConnection::SetAuth(AuthContext* inContext,AuthType inType)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mAuthContext=inContext;
    this->mAuthType=inType;
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
    AutoCrit aCrit(this->mDataCrit);
    if(inOp!=this->mPeerAuthOp.get()) return;
    OpCompletionBasePtr aCompletion=this->mConnectCompletion;
    this->mPeerAuthOp=NULL;
    aCrit.Leave();
    if(aCompletion.get()!=NULL) aCompletion->Complete(AsyncOpPtr(inOp));
    if(inOp->GetStatus()==WS_Success) {
        this->SetRepeatCompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
            ServerConnection::StaticMsgCallback,SmartPtr<RefCount>(this)));
        this->SetRepeatOp(new RecvMsgOp);
        this->StartKeepAlive();
    }
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
    AutoCrit aCrit(this->mDataCrit);
    if(inOp==this->mKeepAliveOp.get()) {
        this->mKeepAliveOp=NULL;
        if(inOp->mKilled) return;
        this->SendNoOp();
        this->StartKeepAlive();
    }
}
// FUNCTION: WON 0x006ab910
// Queues a no-op message to keep the connection alive
void ServerConnection::SendNoOp()
{
    WriteBuffer aBuf(this->mLengthFieldSize,true);
    aBuf.AppendByte(3);
    aBuf.AppendByte(1);
    aBuf.AppendByte(7);
    this->QueueOp(new SendMsgOp(aBuf.ToByteBuffer(true).get()),-1);
}
// FUNCTION: WON 0x006abdc0
// Sets the completion called when a connect finishes
void ServerConnection::SetConnectCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mConnectCompletion=inCompletion;
}
// FUNCTION: WON 0x006abe70
// Stops the keep-alive timer and a pending connect, drops the message callback and closes the socket
void ServerConnection::CloseHook()
{
    if(this->mKeepAliveOp.get()!=NULL) {
        this->mKeepAliveOp->Kill(true);
        this->mKeepAliveOp=NULL;
    }
    if(this->mPeerAuthOp.get()!=NULL) {
        this->mPeerAuthOp->mCompletion=this->mConnectCompletion;
        if(this->mPeerAuthOp->GetStatus()==WS_Pending) this->mPeerAuthOp->Kill(true);
        this->mPeerAuthOp=NULL;
    }
    this->SetRepeatCompletion(NULL);
    QueueSocket::CloseHook();
}
// FUNCTION: WON 0x006abf50
// Kills the socket and drops the connect completion and auth context
void ServerConnection::KillHook()
{
    QueueSocket::KillHook();
    this->mConnectCompletion=NULL;
    this->mAuthContext=NULL;
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
    this->mConnection->SetConnectCompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
        ServerConnectionConnectOp::StaticConnectCompletion,SmartPtr<RefCount>(this)));
    this->mConnection->ConnectAsync(this->mAddr,this->mTimeout);
}
// FUNCTION: WON 0x006ac380
// Removes the op's connect completion from the connection
void ServerConnectionConnectOp::CleanupHook()
{
    this->mConnection->SetConnectCompletion(NULL);
}

// FUNCTION: WON 0x006abc40
// Closes the socket and starts an asynchronous authenticated connect; returns the peer auth op
PeerAuthOp* ServerConnection::ConnectAsync(const IPAddr& address,unsigned long timeout)
{
    AutoCrit aCrit(this->mDataCrit);
    this->Close();
    this->mPeerAuthOp=new PeerAuthOp(address,this->mAuthContext.get(),this->mAuthType,this,true);
    this->mPeerAuthOp->mCompletion=new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
        ServerConnection::StaticConnectCallback,SmartPtr<RefCount>(this));
    this->mPeerAuthOp->RunAsync(timeout);
    return this->mPeerAuthOp.get();
}
}

// No original address identified: WON SDK methods
namespace WONAPI {
void ServerConnection::SetKeepAlivePeriod(DWORD theSeconds)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mKeepAlivePeriod = theSeconds;
}

WONStatus ServerConnection::ConnectBlock(const IPAddr &theAddr, DWORD theTimeout)
{
	AutoCrit aCrit(this->mDataCrit);
	this->Close();
	this->mPeerAuthOp = new PeerAuthOp(theAddr, this->mAuthContext, this->mAuthType, this,true);
	SmartPtr<PeerAuthOp> aPeerAuthOp = this->mPeerAuthOp;
	aCrit.Leave();

	aPeerAuthOp->RunBlock(theTimeout);
	this->ConnectCallback(aPeerAuthOp);
	return aPeerAuthOp->GetStatus();
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
