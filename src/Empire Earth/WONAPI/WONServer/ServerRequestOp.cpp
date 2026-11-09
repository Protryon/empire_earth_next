// ServerRequestOp: request/reply exchange with the first answering server of a list, with optional peer auth
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
// FUNCTION: WON 0x006acf10
// Sets the defaults: 4-byte length field, no auth, 20-second limits, auth2 and close-after-use
void ServerRequestOp::Init()
{
    this->mLengthFieldSize=4;
    this->mAuthType=AUTH_TYPE_NONE;
    this->mMaxRecvTime=this->mMaxSendTime=this->mMaxConnectTime=20000;
    this->mLastServerError=this->mGetCertStatus=WS_None;
    this->mUseAuth2=true;
    this->mUseLastServerErrorForFinishStatus=false;
    this->mCloseConnection=true;
}
// FUNCTION: WON 0x006acf60
// Creates a request that tries the servers of a server context
ServerRequestOp::ServerRequestOp(ServerContext* inContext) : mAddrItr(AddrList::iterator())
{
    this->Init();
    this->SetServerContext(inContext);
}
// FUNCTION: WON 0x006ad050
// Creates a request for a single server address
ServerRequestOp::ServerRequestOp(const IPAddr& inAddress) : mAddrItr(AddrList::iterator())
{
    this->Init();
    this->SetAddr(inAddress);
}
// FUNCTION: WON 0x006ad110
// Destroys the request (members only)
ServerRequestOp::~ServerRequestOp()
{
}
// FUNCTION: WON 0x006ad2e0
// Sets the server context whose addresses are tried
void ServerRequestOp::SetServerContext(ServerContext* inContext)
{
    this->mServerContext=inContext;
}
// FUNCTION: WON 0x006ad340
// Targets a single server address, dropping the server context
void ServerRequestOp::SetAddr(const IPAddr& inAddress)
{
    this->mServerContext=NULL;
    this->mAddrList.clear();
    this->mAddrList.push_back(inAddress);
}
// FUNCTION: WON 0x006ad410
// Returns the first server address, or an empty address
IPAddr ServerRequestOp::GetAddr() const
{
    if(this->mAddrList.empty()) return IPAddr();
    return this->mAddrList.front();
}
// FUNCTION: WON 0x006ad5b0
// Finishes with the status and caches the auth session for reuse when sessioned
void ServerRequestOp::FinishSaveSession(WONStatus inStatus)
{
    this->Finish(inStatus);
    if(this->mAuthType==AUTH_TYPE_SESSION) this->mAuthContext->PutSession(this->mCurAddr,this->mCurSession.get());
    this->mUsedSession=this->mCurSession;
    this->mCurSession=NULL;
}
// FUNCTION: WON 0x006ad660
// Records a server error; the final status uses it only while every server failed the same way
void ServerRequestOp::SetLastServerError(WONStatus inError)
{
    if(this->mLastServerError==WS_None) this->mUseLastServerErrorForFinishStatus=true;
    else if(inError!=this->mLastServerError) this->mUseLastServerErrorForFinishStatus=false;
    this->mLastServerError=inError;
}
// FUNCTION: WON 0x006ad6a0
// Records an invalid reply header and asks to try the next server
WONStatus ServerRequestOp::InvalidReplyHeader()
{
    this->SetLastServerError(WS_ServerReq_InvalidReplyHeader);
    return WS_ServerReq_TryNextServer;
}
// FUNCTION: WON 0x006ad6b0
// Connects to the current server (reusing a cached session), then sends when asynchronous
bool ServerRequestOp::TryConnect()
{
    if(this->mCurSession.get()!=NULL) {
        this->mSocket->AddMsgTransform(new CryptTransform(this->mCurSession.get()));
        this->mReusingSession=true;
    }
    if(this->mRunMode==OP_MODE_ASYNC) {
        ConnectOp* aConnectOp=new ConnectOp(this->mCurAddr);
        this->QueueSocketOp(aConnectOp,this->ConnectTime());
        this->Send();
        return true;
    }
    WONStatus aStatus=this->mSocket->Connect(this->mCurAddr,this->ConnectTime());
    if(aStatus==0) return true;
    this->SetLastServerError(aStatus);
    return false;
}
// FUNCTION: WON 0x006add80
// Moves to the next server address and connects or authenticates; finishes when none are left
bool ServerRequestOp::TryNextServer()
{
    if(this->mStatus!=WS_Pending) return false;
    for(;;) {
        this->mTracker.KillAll();
        this->Reset();
        if(this->mSocket.get()!=NULL) {
            this->mSocket->Close();
            this->mSocket=NULL;
        }
        if(this->mAddrItr!=this->mAddrList.begin() && !this->mSessionExpired && this->mServerContext.get()!=NULL)
            this->mServerContext->NotifyFailed(this->mCurAddr);
        if(this->mAddrItr==this->mAddrList.end()) {
            if(this->mUseLastServerErrorForFinishStatus)
                this->Finish(this->mLastServerError);
            else
                this->Finish(WS_ServerReq_FailedAllServers);
            return false;
        }
        this->mSocket=new BlockingSocket(0);
        this->mSocket->SetLengthFieldSize(this->mLengthFieldSize);
        this->mCurAddr=*this->mAddrItr;
        bool wasReusingSession=this->mReusingSession;
        this->mReusingSession=false;
        this->mSessionExpired=false;
        ++this->mAddrItr;
        if(!wasReusingSession && this->mAuthType==AUTH_TYPE_SESSION)
            this->mCurSession=this->mAuthContext->GetSession(this->mCurAddr);
        else this->mCurSession=NULL;
        if(this->mCurSession.get()!=NULL || this->mAuthType==AUTH_TYPE_NONE) {
            if(this->TryConnect()) return true;
        } else {
            if(this->TryPeerToPeerAuth()) return true;
            if(this->GetStatus()!=WS_Pending) return false;
        }
        if(this->TimeLeft()<=0) {
            this->Finish(WS_TimedOut);
            return false;
        }
    }
}
// FUNCTION: WON 0x006ae050
// Sends the requests GetNextRequest builds until one needs a reply; returns the next step
WONStatus ServerRequestOp::Send()
{
    for(;;) {
        WONStatus aReqStatus=this->GetNextRequest();
        if(aReqStatus!=WS_ServerReq_Send && aReqStatus!=WS_ServerReq_Recv) return aReqStatus;
        if(this->mRunMode==OP_MODE_ASYNC) {
            SendMsgOp* aSendOp=new SendMsgOp(this->mRequest.get());
            this->QueueSocketOp(aSendOp,this->SendTime());
        } else {
            WONStatus aStatus=this->mSocket->SendMsg(this->mRequest.get(),this->SendTime());
            if(aStatus!=0) {
                this->SetLastServerError(aStatus);
                return WS_ServerReq_TryNextServer;
            }
        }
        if(aReqStatus!=WS_ServerReq_Recv)
            continue;
        if(this->IsAsync()) {
            RecvMsgOp* aRecvOp=new RecvMsgOp;
            this->QueueSocketOp(aRecvOp,this->RecvTime());
        }
        return aReqStatus;
    }
}
// FUNCTION: WON 0x006ae200
// Returns true (and retries the same server) when a reused session was rejected as expired
bool ServerRequestOp::CheckExpiredSession()
{
    if(!this->mReusingSession || this->mResponse->length()<10) return false;
    try {
        ReadBuffer aBuf(this->mResponse->data(),this->mResponse->length());
        unsigned long aServiceType=aBuf.ReadLong();
        unsigned long aMessageType=aBuf.ReadLong();
        if(aServiceType==1 && aMessageType==15) {
            this->mSessionExpired=true;
            --this->mAddrItr;
            return true;
        }
    } catch(ReadBufferException&) {}
    return false;
}
// FUNCTION: WON 0x006ae2d0
// Receives replies and checks them until the exchange moves on; returns the next step
WONStatus ServerRequestOp::Recv()
{
    for(;;) {
        WONStatus aStatus=this->mSocket->RecvMsg(this->mResponse,this->RecvTime());
        if(aStatus!=0) {
            this->SetLastServerError(aStatus);
            return WS_ServerReq_TryNextServer;
        }
        if(this->CheckExpiredSession()) return WS_ServerReq_SessionExpired;
        try {
            aStatus=this->CheckResponse();
            if(aStatus!=100059) return aStatus;
        } catch(ReadBufferException&) {
            this->SetLastServerError(WS_ServerReq_UnpackFailure);
            return WS_ServerReq_TryNextServer;
        }
    }
}
// FUNCTION: WON 0x006ae390
// Runs the request: asynchronously, or blocking over each server until one completes
void ServerRequestOp::RunHook()
{
    this->mLastServerError=this->mGetCertStatus=WS_None;
    this->mUseLastServerErrorForFinishStatus=false;
    this->mUsedSession=NULL;
    if(this->mServerContext.get()!=NULL) this->mServerContext->CopyAddresses(this->mAddrList);
    if(this->mAddrList.empty()) {
        this->Finish(WS_ServerReq_NoServersSpecified);
        return;
    }
    this->mAddrItr=this->mAddrList.begin();
    this->mReusingSession=false;
    this->mSessionExpired=false;
    if(this->mAuthType!=AUTH_TYPE_NONE && this->mAuthContext.get()==NULL) {
        this->Finish(WS_ServerReq_NeedAuthContext);
        return;
    }
    if(this->IsAsync()) {
        this->TryNextServer();
        return;
    }
    while(this->TryNextServer()) {
        for(;;) {
            WONStatus aStatus;
            do { aStatus=this->Send(); } while(aStatus==100058);
            if(aStatus==100057) break;
            do { aStatus=this->Recv(); } while(aStatus==100059);
            if(aStatus==100057 || aStatus==100061) break;
            if(aStatus==100058) continue;
            this->FinishSaveSession(aStatus);
            return;
        }
        if(this->TimeLeft()<=0) {
            this->Finish(WS_TimedOut);
            return;
        }
    }
}
// FUNCTION: WON 0x006ae4c0
// Marks a timed-out or killed server as failed and closes the socket unless it should stay open
void ServerRequestOp::CleanupHook()
{
    AsyncOpWithTracker::CleanupHook();
    if(this->GetStatus()!=WS_Success && (this->GetStatus()==WS_TimedOut || this->mKilled) && this->mServerContext.get()!=NULL)
        this->mServerContext->NotifyFailed(this->mCurAddr);
    if(this->mSocket.get()!=NULL && (this->mCloseConnection || this->GetStatus()!=WS_Success || this->mKilled)) {
        this->mSocket->Close();
        this->mSocket=NULL;
    }
    this->mCurSession=NULL;
}

// FUNCTION: WON 0x006ad950
// Runs peer authentication with the current server; true when started or done, false on failure
bool ServerRequestOp::TryPeerToPeerAuth()
{
    SmartPtr<PeerAuthOp> peerAuth=new PeerAuthOp(this->mCurAddr,this->mAuthContext.get(),this->mAuthType,this->mSocket.get(),this->mUseAuth2);
    peerAuth->CopyMaxTimes(this);
    peerAuth->SetClientKeySize(this->mClientKeySize,this->mUseClientKey);
    if(this->mRunMode==OP_MODE_ASYNC) {
        this->Track(peerAuth.get(),2);
        peerAuth->RunAsync(-1);
        return true;
    }
    if(peerAuth->RunBlock(this->TimeLeft())) {
        this->mCurSession=peerAuth->GetSession();
        return true;
    }
    if(peerAuth->GetStatus()==WS_PeerAuth_GetCertFailure) {
        this->mGetCertStatus=peerAuth->GetGetCertStatus();
        this->Finish(WS_ServerReq_GetCertFailure);
    } else this->SetLastServerError(peerAuth->GetStatus());
    return false;
}
// FUNCTION: WON 0x006adb10
// Advances the asynchronous exchange on peer auth and socket completions; false for unknown ops
bool ServerRequestOp::CallbackHook(AsyncOp* op,int param)
{
    if(op->mKilled) {
        this->Kill(true);
        return true;
    }
    if(op->GetStatus()!=WS_Success) {
        if(param==2 && op->GetStatus()==WS_PeerAuth_GetCertFailure) {
            this->mGetCertStatus=static_cast<PeerAuthOp*>(op)->GetGetCertStatus();
            this->Finish(WS_ServerReq_GetCertFailure);
            return true;
        }
        this->SetLastServerError(op->GetStatus());
        this->TryNextServer();
        return true;
    }
    switch(param) {
    case 2:
        this->mCurSession=static_cast<PeerAuthOp*>(op)->GetSession();
        this->Send();
        break;
    case 1: {
        RecvMsgOp* receive=dynamic_cast<RecvMsgOp*>(op);
        if(receive!=NULL) {
            this->mResponse=receive->GetMsg();
            if(this->CheckExpiredSession()) {
                this->TryNextServer();
                return true;
            }
            try {
                WONStatus status=this->CheckResponse();
                if(status==WS_ServerReq_TryNextServer) this->TryNextServer();
                else if(status==WS_ServerReq_Send) this->Send();
                else if(status==WS_ServerReq_Recv) this->QueueSocketOp(new RecvMsgOp,this->RecvTime());
                else if(status!=WS_ServerReq_ExitCommunicationLoop) this->FinishSaveSession(status);
            } catch(ReadBufferException&) {
                this->SetLastServerError(WS_ServerReq_UnpackFailure);
                this->TryNextServer();
            }
        }
        break;
    }
    default: return false;
    }
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x006adae0
// Queues a tracked socket op on the current socket
void ServerRequestOp::QueueSocketOp(SocketOp* op,DWORD timeout)
{
    this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(op,1)),timeout);
}
}

namespace WONAPI {
// WON SDK default virtual methods

// FUNCTION: WON 0x006ad020
// Builds no request and asks to receive (default)
WONStatus ServerRequestOp::GetNextRequest() { return WS_ServerReq_Recv; }
WONStatus ServerRequestOp::CheckResponse() { return WS_Success; }
void ServerRequestOp::Reset() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ad030
// (protected: virtual void * __thiscall WONAPI::ServerRequestOp::`scalar deleting destructor'(unsigned int)    [??_GServerRequestOp@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006ad8e0
// (public: virtual __thiscall WONAPI::CryptTransform::~CryptTransform(void)    [??1CryptTransform@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ad940
// (public: virtual __thiscall WONAPI::MsgTransform::~MsgTransform(void)    [??1MsgTransform@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ae940
// (public: __thiscall std::multimap<unsigned long,struct WONAPI::AsyncOpTrackerEntry *,struct std::less<unsigned long>,class std::allocator<struct std::p)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ad8c0
// (public: virtual void * __thiscall WONAPI::CryptTransform::`scalar deleting destructor'(unsigned int)    [??_GCryptTransform@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ad8a0
// (public: virtual long __thiscall WONAPI::CryptTransform::Recv(class WONAPI::SmartPtr<class WONAPI::ByteBuffer> &)    [?Recv@CryptTransform@WONAPI@@UAEJ)
// TEMPLATE: WON 0x006ad8b0
// (public: virtual long __thiscall WONAPI::CryptTransform::Send(class WONAPI::SmartPtr<class WONAPI::ByteBuffer> &)    [?Send@CryptTransform@WONAPI@@UAEJ)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ad870
// (public: virtual long __thiscall WONAPI::MsgTransform::Send(class WONAPI::SmartPtr<class WONAPI::ByteBuffer> &)    [?Send@MsgTransform@WONAPI@@UAEJAAV?)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ad880
// (public: virtual void * __thiscall WONAPI::MsgTransform::`scalar deleting destructor'(unsigned int))

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f2670
// (public: class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::iterator __thiscall std::list<class WONAPI::IPAddr,class st)
