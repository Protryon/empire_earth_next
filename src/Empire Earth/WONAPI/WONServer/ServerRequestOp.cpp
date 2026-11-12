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
#include "ee/wonapi/WONAuthPeerData.h"
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
// Records a server error; the final status uses it only while every server failed the same way (Rust:
// ee_won server.rs)
void ServerRequestOp::SetLastServerError(WONStatus inError)
{
    int32_t aLast = this->mLastServerError;
    ee_won_server_req_last_error(&aLast, &this->mUseLastServerErrorForFinishStatus, inError);
    this->mLastServerError = static_cast<WONStatus>(aLast);
}
// FUNCTION: WON 0x006ad6a0
// Records an invalid reply header and asks to try the next server
WONStatus ServerRequestOp::InvalidReplyHeader()
{
    this->SetLastServerError(WS_ServerReq_InvalidReplyHeader);
    return WS_ServerReq_TryNextServer;
}

// The exchange's steps run in Rust (ee_won server.rs) over these primitives of the op; the members it keeps
// (mLastServerError, mGetCertStatus, mUseLastServerErrorForFinishStatus, mReusingSession, mSessionExpired) are
// handed over as pointers and read and written at each step.
struct ServerRequestGlue {
    ServerRequestOp* mOp;
    AsyncOp* mDone; // CallbackHook's completed op
};
static ServerRequestOp* Op(void* inGlue) { return static_cast<ServerRequestGlue*>(inGlue)->mOp; }
static int32_t ReqStatus(void* g) { return Op(g)->GetStatus(); }
static uint32_t ReqTimeLeft(void* g) { return static_cast<uint32_t>(Op(g)->TimeLeft()); }
static bool ReqIsAsync(void* g) { return Op(g)->GetMode() == OP_MODE_ASYNC; }
static int32_t ReqAuthType(void* g) { return Op(g)->mAuthType; }
static void ReqFinish(void* g, int32_t s) { Op(g)->Finish(static_cast<WONStatus>(s)); }
static void ReqFinishSave(void* g, int32_t s) { Op(g)->FinishSaveSession(static_cast<WONStatus>(s)); }
static void ReqKill(void* g) { Op(g)->Kill(true); }
static bool ReqPrepare(void* g)
{
    ServerRequestOp* o = Op(g);
    o->mUsedSession = NULL;
    if (o->mServerContext.get() != NULL) o->mServerContext->CopyAddresses(o->mAddrList);
    return !o->mAddrList.empty();
}
static void ReqRewind(void* g) { Op(g)->mAddrItr = Op(g)->mAddrList.begin(); }
static bool ReqAtBegin(void* g) { return Op(g)->mAddrItr == Op(g)->mAddrList.begin(); }
static bool ReqAtEnd(void* g) { return Op(g)->mAddrItr == Op(g)->mAddrList.end(); }
static bool ReqHasContext(void* g) { return Op(g)->mServerContext.get() != NULL; }
static bool ReqHasAuthContext(void* g) { return Op(g)->mAuthContext.get() != NULL; }
static void ReqNotify(void* g) { Op(g)->mServerContext->NotifyFailed(Op(g)->mCurAddr); }
static void ReqNextAddr(void* g)
{
    ServerRequestOp* o = Op(g);
    o->mSocket = new BlockingSocket(0);
    o->mSocket->SetLengthFieldSize(o->mLengthFieldSize);
    o->mCurAddr = *o->mAddrItr;
    ++o->mAddrItr;
}
static void ReqRetreat(void* g) { --Op(g)->mAddrItr; }
static void ReqReset(void* g)
{
    ServerRequestOp* o = Op(g);
    o->mTracker.KillAll();
    o->Reset();
    if (o->mSocket.get() != NULL) {
        o->mSocket->Close();
        o->mSocket = NULL;
    }
}
static void ReqLoadSession(void* g) { Op(g)->mCurSession = Op(g)->mAuthContext->GetSession(Op(g)->mCurAddr); }
static void ReqClearSession(void* g) { Op(g)->mCurSession = NULL; }
static bool ReqHasSession(void* g) { return Op(g)->mCurSession.get() != NULL; }
static void ReqAddCrypt(void* g) { Op(g)->mSocket->AddMsgTransform(new CryptTransform(Op(g)->mCurSession.get())); }
static void ReqQueueConnect(void* g)
{
    ServerRequestOp* o = Op(g);
    o->QueueSocketOp(new ConnectOp(o->mCurAddr), o->ConnectTime());
}
static int32_t ReqConnectBlock(void* g)
{
    ServerRequestOp* o = Op(g);
    return o->mSocket->Connect(o->mCurAddr, o->ConnectTime());
}
static SmartPtr<PeerAuthOp> ReqNewPeerAuth(ServerRequestOp* o)
{
    SmartPtr<PeerAuthOp> peerAuth = new PeerAuthOp(o->mCurAddr, o->mAuthContext.get(), o->mAuthType, o->mSocket.get(),
                                                   o->mUseAuth2);
    peerAuth->CopyMaxTimes(o);
    peerAuth->SetClientKeySize(o->mClientKeySize, o->mUseClientKey);
    return peerAuth;
}
static void ReqPeerAuthAsync(void* g)
{
    SmartPtr<PeerAuthOp> peerAuth = ReqNewPeerAuth(Op(g));
    Op(g)->Track(peerAuth.get(), 2);
    peerAuth->RunAsync(-1);
}
static bool ReqPeerAuthBlock(void* g, int32_t* outStatus, int32_t* outCert)
{
    ServerRequestOp* o = Op(g);
    SmartPtr<PeerAuthOp> peerAuth = ReqNewPeerAuth(o);
    if (peerAuth->RunBlock(o->TimeLeft())) {
        o->mCurSession = peerAuth->GetSession();
        return true;
    }
    *outStatus = peerAuth->GetStatus();
    *outCert = peerAuth->GetGetCertStatus();
    return false;
}
static int32_t ReqGetNextRequest(void* g) { return Op(g)->GetNextRequest(); }
static void ReqQueueSend(void* g)
{
    ServerRequestOp* o = Op(g);
    o->QueueSocketOp(new SendMsgOp(o->mRequest.get()), o->SendTime());
}
static int32_t ReqSendBlock(void* g)
{
    ServerRequestOp* o = Op(g);
    return o->mSocket->SendMsg(o->mRequest.get(), o->SendTime());
}
static void ReqQueueRecv(void* g) { Op(g)->QueueSocketOp(new RecvMsgOp, Op(g)->RecvTime()); }
static int32_t ReqRecvBlock(void* g)
{
    ServerRequestOp* o = Op(g);
    return o->mSocket->RecvMsg(o->mResponse, o->RecvTime());
}
static void ReqResponse(void* g, const uint8_t** outData, size_t* outLength)
{
    ServerRequestOp* o = Op(g);
    if (o->mResponse.get() == NULL) {
        *outData = NULL;
        *outLength = 0;
        return;
    }
    *outData = reinterpret_cast<const uint8_t*>(o->mResponse->data());
    *outLength = o->mResponse->length();
}
static int32_t ReqCheckResponse(void* g, bool* outThrew)
{
    try {
        *outThrew = false;
        return Op(g)->CheckResponse();
    } catch (ReadBufferException&) {
        *outThrew = true;
        return 0;
    }
}
static void ReqTakeOpSession(void* g)
{
    Op(g)->mCurSession = static_cast<PeerAuthOp*>(static_cast<ServerRequestGlue*>(g)->mDone)->GetSession();
}
static void ReqTakeOpResponse(void* g)
{
    Op(g)->mResponse = static_cast<RecvMsgOp*>(static_cast<ServerRequestGlue*>(g)->mDone)->GetMsg();
}
static EeWonServerReqHooks ReqHooks(ServerRequestGlue* inGlue)
{
    ServerRequestOp* o = inGlue->mOp;
    EeWonServerReqHooks h = {inGlue,
                             reinterpret_cast<int32_t*>(&o->mLastServerError),
                             reinterpret_cast<int32_t*>(&o->mGetCertStatus),
                             &o->mUseLastServerErrorForFinishStatus,
                             &o->mReusingSession,
                             &o->mSessionExpired,
                             ReqStatus, ReqTimeLeft, ReqIsAsync, ReqAuthType, ReqFinish, ReqFinishSave, ReqKill,
                             ReqPrepare, ReqRewind, ReqAtBegin, ReqAtEnd, ReqHasContext, ReqHasAuthContext,
                             ReqNotify, ReqNextAddr, ReqRetreat, ReqReset, ReqLoadSession, ReqClearSession,
                             ReqHasSession, ReqAddCrypt, ReqQueueConnect, ReqConnectBlock, ReqPeerAuthAsync,
                             ReqPeerAuthBlock, ReqGetNextRequest, ReqQueueSend, ReqSendBlock, ReqQueueRecv,
                             ReqRecvBlock, ReqResponse, ReqCheckResponse, ReqTakeOpSession, ReqTakeOpResponse};
    return h;
}

// FUNCTION: WON 0x006ad6b0
// Connects to the current server (reusing a cached session), then sends when asynchronous
bool ServerRequestOp::TryConnect()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return ee_won_server_req_try_connect(&aHooks);
}
// FUNCTION: WON 0x006add80
// Moves to the next server address and connects or authenticates; finishes when none are left
bool ServerRequestOp::TryNextServer()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return ee_won_server_req_try_next(&aHooks);
}
// FUNCTION: WON 0x006ae050
// Sends the requests GetNextRequest builds until one needs a reply; returns the next step
WONStatus ServerRequestOp::Send()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return static_cast<WONStatus>(ee_won_server_req_send(&aHooks));
}
// FUNCTION: WON 0x006ae200
// Returns true (and retries the same server) when a reused session was rejected as expired
bool ServerRequestOp::CheckExpiredSession()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return ee_won_server_req_check_expired(&aHooks);
}
// FUNCTION: WON 0x006ae2d0
// Receives replies and checks them until the exchange moves on; returns the next step
WONStatus ServerRequestOp::Recv()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return static_cast<WONStatus>(ee_won_server_req_recv(&aHooks));
}
// FUNCTION: WON 0x006ae390
// Runs the request: asynchronously, or blocking over each server until one completes
void ServerRequestOp::RunHook()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    ee_won_server_req_run(&aHooks);
}
// FUNCTION: WON 0x006ae4c0
// Marks a timed-out or killed server as failed and closes the socket unless it should stay open
void ServerRequestOp::CleanupHook()
{
    AsyncOpWithTracker::CleanupHook();
    uint32_t aPlan = ee_won_server_req_cleanup_plan(this->GetStatus(), this->IsKilled(), this->mServerContext.get() != NULL,
                                                    this->mSocket.get() != NULL, this->mCloseConnection);
    if (aPlan & 1) this->mServerContext->NotifyFailed(this->mCurAddr);
    if (aPlan & 2) {
        this->mSocket->Close();
        this->mSocket = NULL;
    }
    this->mCurSession = NULL;
}

// FUNCTION: WON 0x006ad950
// Runs peer authentication with the current server; true when started or done, false on failure
bool ServerRequestOp::TryPeerToPeerAuth()
{
    ServerRequestGlue aGlue = {this, NULL};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return ee_won_server_req_try_peer_auth(&aHooks);
}
// FUNCTION: WON 0x006adb10
// Advances the asynchronous exchange on peer auth and socket completions; false for unknown ops
bool ServerRequestOp::CallbackHook(AsyncOp* op,int param)
{
    EeWonServerReqDone aDone;
    aDone.killed = op->IsKilled();
    aDone.status = op->GetStatus();
    aDone.param = param;
    aDone.cert_status = 0;
    if (param == 2 && !op->IsKilled() && op->GetStatus() == WS_PeerAuth_GetCertFailure)
        aDone.cert_status = static_cast<PeerAuthOp*>(op)->GetGetCertStatus();
    aDone.is_recv = param == 1 && dynamic_cast<RecvMsgOp*>(op) != NULL;
    ServerRequestGlue aGlue = {this, op};
    EeWonServerReqHooks aHooks = ReqHooks(&aGlue);
    return ee_won_server_req_callback(&aHooks, &aDone);
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
