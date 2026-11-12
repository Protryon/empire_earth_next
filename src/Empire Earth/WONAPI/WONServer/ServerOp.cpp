// ServerOp: base of the server operations (per-step connect/send/receive time limits)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONPing.h" // the MultiPingOp helpers over RustServerOp
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAuthCertificate.h"
#include "ee/wonapi/WONAuthPubKeyBlock.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
// FUNCTION: WON 0x006acc10
// Creates a server op with 60-second connect, send and receive limits and an 8-byte key size
ServerOp::ServerOp()
{
    this->mMaxConnectTime=60000;
    this->mMaxSendTime=60000;
    this->mMaxRecvTime=60000;
    this->mClientKeySize=8;
    this->mUseClientKey=false;
}
// FUNCTION: WON 0x006ace20
// Sets the connect, send and receive time limits
void ServerOp::SetMaxTimes(int inConnect,int inSend,int inRecv)
{
    this->mMaxConnectTime=inConnect;
    this->mMaxSendTime=inSend;
    this->mMaxRecvTime=inRecv;
}
// FUNCTION: WON 0x006ace50
// Returns the connect time limit, capped by the time left when blocking
DWORD ServerOp::ConnectTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxConnectTime;
    return ee_won_server_step_time(this->mMaxConnectTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006ace80
// Returns the send time limit, capped by the time left when blocking
DWORD ServerOp::SendTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxSendTime;
    return ee_won_server_step_time(this->mMaxSendTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006aceb0
// Returns the receive time limit, capped by the time left when blocking
DWORD ServerOp::RecvTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxRecvTime;
    return ee_won_server_step_time(this->mMaxRecvTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006acee0
// Copies the time limits of another server op
void ServerOp::CopyMaxTimes(ServerOp* inSource)
{
    this->SetMaxTimes(inSource->mMaxConnectTime,inSource->mMaxSendTime,inSource->mMaxRecvTime);
}
}

// No original address identified: WON SDK methods
namespace WONAPI {
void ServerOp::SendMsgAsync(ByteBufferPtr theMsg, int theTrackId)
{
	SmartPtr<SendMsgOp> anOp = new SendMsgOp(theMsg, this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}

void ServerOp::RecvMsgAsync(int theTrackId)
{
	SmartPtr<RecvMsgOp> anOp = new RecvMsgOp(this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}
}

// ---- RustServerOp: an op of the Rust op base (ee_won hostop.rs), the Rust op's calls answered on the C++ objects

#include <time.h>
#include <stdlib.h>
#include <vector>
#include <sys/stat.h>
#include "platform/PlatFile.h"
namespace WONAPI {
namespace {
// the param of a completion this op made: the op (a reference, as the original's completions held theirs) and the id
struct HostParam : public RefCount {
    SmartPtr<RustServerOp> mOp;
    int mId;
    HostParam(RustServerOp* inOp, int inId) : mOp(inOp), mId(inId) {}
};
void HostCompletion(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    HostParam* aParam = static_cast<HostParam*>(inParam.get());
    aParam->mOp->Completed(inOp.get(), aParam->mId);
}
CompletionBase<AsyncOpPtr>* NewHostCompletion(RustServerOp* inOp, int inId)
{
    return new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(HostCompletion, new HostParam(inOp, inId));
}
RefCount* Obj(void* inObj) { return static_cast<RefCount*>(inObj); }
BlockingSocket* Sock(void* inObj) { return inObj ? static_cast<BlockingSocket*>(Obj(inObj)) : NULL; }
AsyncOp* Op(void* inObj) { return static_cast<AsyncOp*>(Obj(inObj)); }
void* Out(RefCount* inObj) { inObj->AddRef(); return inObj; } // a new object: the Rust op's reference
IPAddr Addr(const EeWonHostCall* c) { IPAddr a; a.SetSixByte(c->addr); return a; }
void GiveAddr(EeWonHostCall* c, const IPAddr& inAddr) { memcpy(c->out_addr, inAddr.GetSixByte(), 6); }
ByteBufferPtr Buf(const EeWonHostCall* c) { return new ByteBuffer(c->data, (unsigned long)c->len, false); }
void Give(EeWonHostCall* c, const void* inData, size_t inLength)
{
    c->out_sink(c->out_context, static_cast<const uint8_t*>(inData), inLength);
}
void Give(EeWonHostCall* c, const ByteBuffer* inBytes)
{
    if (inBytes != NULL) Give(c, inBytes->data(), inBytes->length());
}
// GetCertOp's objects (certhost.rs): every one a RefCount* (the private key an ElGamal*)
void* V(const RefCount* inObj) { return const_cast<RefCount*>(inObj); }
AuthContext* Ctx(void* p) { return static_cast<AuthContext*>(Obj(p)); }
const AuthPeerData* Peer(void* p) { return static_cast<const AuthPeerData*>(Obj(p)); }
const AuthPubKeyBlock* Block(void* p) { return static_cast<const AuthPubKeyBlock*>(Obj(p)); }
const AuthCertificate* Cert1(void* p) { return static_cast<const AuthCertificate*>(Obj(p)); }
const Auth2Certificate* Cert2(void* p) { return static_cast<const Auth2Certificate*>(Obj(p)); }
const AuthCertificateBase* Cert(long long inKind, void* p)
{
    return inKind == 1 ? static_cast<const AuthCertificateBase*>(Cert1(p)) : static_cast<const AuthCertificateBase*>(Cert2(p));
}
void GiveText(EeWonHostCall* c, const std::wstring& inText)
{
    std::string b;
    WideAppendUtf16LE(b, inText);
    Give(c, b.data(), b.size());
}
SocketOp* NewSocketOp(const EeWonHostCall* c)
{
    AsyncSocket* s = Sock(c->obj[0]);
    switch (c->num[0]) {
    case EE_WON_HOST_OP_CONNECT: return new ConnectOp(Addr(c), s);
    case EE_WON_HOST_OP_SEND_BYTES: return new SendBytesOp(Buf(c), s);
    case EE_WON_HOST_OP_SEND_BYTES_TO: return new SendBytesToOp(Buf(c), Addr(c), s);
    case EE_WON_HOST_OP_SEND_MSG: return new SendMsgOp(Buf(c), s);
    case EE_WON_HOST_OP_RECV_BYTES: return new RecvBytesOp((unsigned long)c->num[1], s);
    case EE_WON_HOST_OP_RECV_MSG: return new RecvMsgOp(s);
    case EE_WON_HOST_OP_RECV_CR_MSG: return new RecvCRMsgOp(s);
    default: return new RecvBytesFromOp((unsigned short)c->num[1], s);
    }
}
}

RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp), mFile(NULL)
{
    uint32_t aTimes[3];
    ee_won_hostop_setup(inRustOp, &this->mLengthFieldSize, aTimes);
    this->SetMaxTimes(aTimes[0], aTimes[1], aTimes[2]);
    this->mAuthType = AUTH_TYPE_NONE;
    ee_won_hostop_bind(inRustOp, RustServerOp::HostCall, this);
}
RustServerOp* RustServerOp::NewRequest(EeWonReqOp* inOp, ServerContext* inContext)
{
    RustServerOp* anOp = new RustServerOp(ee_won_hostop_request(inOp));
    anOp->Set(EE_WON_REQHOST_CONTEXT, static_cast<RefCount*>(inContext));
    return anOp;
}
RustServerOp* RustServerOp::NewRequest(EeWonReqOp* inOp, const IPAddr& inAddr)
{
    RustServerOp* anOp = new RustServerOp(ee_won_hostop_request(inOp));
    ee_won_hostop_set(anOp->mRustOp, EE_WON_REQHOST_ADDR, 0, NULL, 0, reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()),
                      NULL);
    return anOp;
}
ServerRequestOp::ServerRequestOp(ServerContext* inContext) : RustServerOp(ee_won_hostop_cpp_request())
{
    this->Set(EE_WON_REQHOST_CONTEXT, inContext);
}
ServerRequestOp::ServerRequestOp(const IPAddr& inAddr) : RustServerOp(ee_won_hostop_cpp_request())
{
    ee_won_hostop_set(this->GetRustOp(), EE_WON_REQHOST_ADDR, 0, NULL, 0,
                      reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()), NULL);
}
void ServerRequestOp::SetLastServerError(WONStatus inError) { this->SetNum(EE_WON_REQHOST_LAST_ERROR, inError); }
WONStatus ServerRequestOp::InvalidReplyHeader()
{
    this->SetLastServerError(WS_ServerReq_InvalidReplyHeader);
    return WS_ServerReq_TryNextServer;
}
GetCertOp::GetCertOp(AuthContext* inContext) : RustServerOp(ee_won_hostop_getcert())
{
    this->Set(EE_WON_REQHOST_CONTEXT, inContext->GetServerContext().get());
    this->Set(EE_WON_GETCERT_AUTH_CONTEXT, inContext);
}
const AuthPeerData* GetCertOp::GetPeerData() const
{
    return Peer(reinterpret_cast<void*>((intptr_t)ee_won_hostop_num(this->GetRustOp(), EE_WON_GETCERT_PEER, 0)));
}
DetectFirewallOp::DetectFirewallOp(ServerContext* inContext) : RustServerOp(ee_won_hostop_firewall())
{
    this->Set(EE_WON_REQHOST_CONTEXT, inContext);
}
IPAddr DetectFirewallOp::GetPublicAddr() const
{
    IPAddr anAddr;
    if (ee_won_hostop_num(this->GetRustOp(), EE_WON_FWOP_HAS_PUBLIC, 0))
        anAddr.Fn2B2C20(static_cast<long>(ee_won_hostop_num(this->GetRustOp(), EE_WON_FWOP_PUBLIC_HOST, 0)), 0);
    return anAddr;
}
void RustServerOp::SetNum(int inWhich, long long inNum)
{
    ee_won_hostop_set(this->mRustOp, inWhich, inNum, NULL, 0, NULL, NULL);
}
void RustServerOp::Set(int inWhich, RefCount* inObj)
{
    ee_won_hostop_set(this->mRustOp, inWhich, 0, NULL, 0, NULL, inObj);
}
IPAddr RustServerOp::GetCurAddr() const { return this->AddrOf(EE_WON_REQHOST_CUR_ADDR); }
IPAddr RustServerOp::GetAddr() const { return this->AddrOf(EE_WON_REQHOST_ADDR); }
IPAddr RustServerOp::AddrOf(int inWhich) const
{
    std::string anAddr;
    ee_won_hostop_bytes(this->mRustOp, inWhich, 0, StrSink, &anAddr);
    IPAddr a;
    if (anAddr.size() == 6) a.SetSixByte(anAddr.data());
    return a;
}
void RustServerOp::StrSink(void* inContext, const uint8_t* inData, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inData), inLength);
}
RustServerOp::~RustServerOp()
{
    ee_won_hostop_free(this->mRustOp); // its objects released through HostCall
    if (this->mFile != NULL) fclose(this->mFile);
}
void RustServerOp::RunHook() { ee_won_hostop_run(this->mRustOp); }
void RustServerOp::CleanupHook() { ee_won_hostop_cleanup(this->mRustOp); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam)
{
    return ee_won_hostop_callback(this->mRustOp, inParam, inOp);
}
void RustServerOp::HostCall(void* inOp, EeWonHostCall* c)
{
    RustServerOp* o = static_cast<RustServerOp*>(inOp);
    void* o0 = c->obj[0];
    long long n0 = c->num[0], n1 = c->num[1];
    switch (c->code) {
    case EE_WON_HOST_STATUS: c->out_num = o->GetStatus(); break;
    case EE_WON_HOST_MODE: c->out_num = o->GetMode(); break;
    case EE_WON_HOST_TIME_LEFT: c->out_num = (DWORD)o->TimeLeft(); break;
    case EE_WON_HOST_TICK: c->out_num = (DWORD)WONTickCount(); break;
    case EE_WON_HOST_RAND: c->out_num = rand(); break;
    case EE_WON_HOST_SRAND: srand((unsigned)time(NULL)); break;
    case EE_WON_HOST_LFS: c->out_num = o->mLengthFieldSize; break;
    case EE_WON_HOST_SEND_TIME: c->out_num = o->SendTime(); break;
    case EE_WON_HOST_RECV_TIME: c->out_num = o->RecvTime(); break;
    case EE_WON_HOST_NEW_SOCKET: c->out_obj = Out(new BlockingSocket(n0 ? AsyncSocket::UDP : AsyncSocket::TCP)); break;
    case EE_WON_HOST_NEW_OP: c->out_obj = Out(NewSocketOp(c)); break;
    case EE_WON_HOST_NEW_TIMER: {
        AsyncOp* aTimer = new AsyncOp;
        aTimer->SetCompletion(NewHostCompletion(o, (int)n0));
        c->out_obj = Out(aTimer);
        break;
    }
    case EE_WON_HOST_NEW_PEER_AUTH: {
        PeerAuthOp* anOp = new PeerAuthOp(Addr(c), o->mAuthContext, o->mAuthType, Sock(o0), n0 != 0);
        anOp->CopyMaxTimes(o);
        c->out_obj = Out(anOp);
        break;
    }
    case EE_WON_HOST_OP_STATUS: c->out_num = Op(o0)->GetStatus(); break;
    case EE_WON_HOST_OP_BYTES: {
        AsyncOp* anOp = Op(o0);
        if (RecvBytesFromOp* f = dynamic_cast<RecvBytesFromOp*>(anOp)) Give(c, f->GetBytes().get());
        else if (RecvMsgOp* m = dynamic_cast<RecvMsgOp*>(anOp)) Give(c, m->GetMsg().get());
        else if (RecvCRMsgOp* l = dynamic_cast<RecvCRMsgOp*>(anOp)) Give(c, l->GetMsg().get());
        else if (RecvBytesOp* b = dynamic_cast<RecvBytesOp*>(anOp)) Give(c, b->GetBytes().get());
        break;
    }
    case EE_WON_HOST_OP_ADDR:
        if (RecvBytesFromOp* f = dynamic_cast<RecvBytesFromOp*>(Op(o0))) GiveAddr(c, f->GetAddr());
        break;
    case EE_WON_HOST_OP_SOCKET:
        if (SocketOp* s = dynamic_cast<SocketOp*>(Op(o0))) c->out_obj = s->GetSocket() ? Obj(s->GetSocket()) : NULL;
        break;
    case EE_WON_HOST_PEER_AUTH_KEY: {
        AuthSession* aSession = static_cast<PeerAuthOp*>(Op(o0))->GetSession();
        if (aSession != NULL) Give(c, aSession->GetKey().GetKey(), aSession->GetKey().GetKeyLen());
        break;
    }
    case EE_WON_HOST_OP_KILLED: c->out_num = (o0 != NULL ? Op(o0) : o)->IsKilled() ? 1 : 0; break;
    case EE_WON_HOST_RESOLVE: {
        IPAddr anAddr;
        c->out_num = anAddr.SetWithDefaultPort(std::string(reinterpret_cast<const char*>(c->data), c->len),
                                               (unsigned short)n0) ? 1 : 0;
        GiveAddr(c, anAddr);
        break;
    }
    case EE_WON_HOST_RECV_PROGRESS:
        static_cast<RecvBytesOp*>(Op(o0))->SetProgressCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(Obj(c->obj[1])),
                                                                 (long)n0);
        break;
    case EE_WON_HOST_RECV_FILE: {
        RecvBytesOp* anOp = static_cast<RecvBytesOp*>(Op(o0));
        anOp->SetFilePath(std::string(reinterpret_cast<const char*>(c->data), c->len));
        anOp->SetAppendFile(n0 != 0);
        break;
    }
    case EE_WON_HOST_CONNECT: c->out_num = Sock(o0)->Connect(Addr(c), (unsigned long)n0); break;
    case EE_WON_HOST_FILE_SIZE: {
        std::string aPath(reinterpret_cast<const char*>(c->data), c->len);
        struct stat anInfo;
        c->out_num = stat(aPath.c_str(), &anInfo) == 0 ? (long long)anInfo.st_size : -1;
        break;
    }
    case EE_WON_HOST_SOCKET_BIND: c->out_num = Sock(o0)->Fn2B0D90((unsigned short)n0, n1 != 0); break;
    case EE_WON_HOST_SOCKET_SET_LFS: Sock(o0)->SetLengthFieldSize((unsigned char)n0); break;
    case EE_WON_HOST_SEND_BYTES_TO: c->out_num = Sock(o0)->SendBytesTo(Buf(c), Addr(c), (unsigned long)n0); break;
    case EE_WON_HOST_RECV_BYTES_FROM: {
        ByteBufferPtr aBytes;
        IPAddr anAddr;
        c->out_num = Sock(o0)->RecvBytesFrom(aBytes, (unsigned short)n0, anAddr, (unsigned long)n1);
        Give(c, aBytes.get());
        GiveAddr(c, anAddr);
        break;
    }
    case EE_WON_HOST_SEND_BYTES: c->out_num = Sock(o0)->SendBytes(Buf(c), (unsigned long)n0); break;
    case EE_WON_HOST_SEND_MSG: c->out_num = Sock(o0)->SendMsg(Buf(c), (unsigned long)n0); break;
    case EE_WON_HOST_RECV_BYTES: {
        ByteBufferPtr aBytes;
        c->out_num = Sock(o0)->RecvBytes(aBytes, (DWORD)n0, (DWORD)n1);
        Give(c, aBytes.get());
        break;
    }
    case EE_WON_HOST_RECV_MSG: {
        ByteBufferPtr aBytes;
        c->out_num = Sock(o0)->RecvMsg(aBytes, (unsigned long)n0);
        Give(c, aBytes.get());
        break;
    }
    case EE_WON_HOST_RUN_BLOCK: c->out_num = Op(o0)->RunBlock((unsigned long)n0) ? 1 : 0; break;
    case EE_WON_HOST_FILE_OPEN:
        if (o->mFile != NULL) fclose(o->mFile);
        o->mFile = PlatFOpen(std::string(reinterpret_cast<const char*>(c->data), c->len).c_str(), "rb");
        c->out_num = o->mFile != NULL;
        break;
    case EE_WON_HOST_FILE_READ: {
        std::vector<char> aBuf((size_t)n0 + 1);
        size_t aLen = o->mFile != NULL ? fread(&aBuf[0], 1, (size_t)n0, o->mFile) : 0;
        Give(c, &aBuf[0], aLen);
        break;
    }
    case EE_WON_HOST_FILE_CLOSE:
        if (o->mFile != NULL) fclose(o->mFile);
        o->mFile = NULL;
        break;
    case EE_WON_HOST_FINISH: o->Finish((WONStatus)n0); break;
    case EE_WON_HOST_KILL: (o0 != NULL ? Op(o0) : o)->Kill(n0 != 0); break;
    case EE_WON_HOST_TRACK: o->Track(Op(o0), (int)n0); break;
    case EE_WON_HOST_QUEUE: Sock(o0)->QueueOp(static_cast<SocketOp*>(Op(c->obj[1])), (long)n0); break;
    case EE_WON_HOST_RUN_ASYNC: Op(o0)->RunAsync((long)n0); break;
    case EE_WON_HOST_CLOSE: Sock(o0)->Close(); break;
    case EE_WON_HOST_SET_REPEAT:
        Sock(o0)->SetRepeatCompletion(NewHostCompletion(o, (int)n0));
        Sock(o0)->SetRepeatOp(static_cast<SocketOp*>(Op(c->obj[1])));
        break;
    case EE_WON_HOST_COMPLETE: static_cast<CompletionBase<AsyncOpPtr>*>(Obj(o0))->Complete(o); break;
    case EE_WON_HOST_BASE_CLEANUP: o->AsyncOpWithTracker::CleanupHook(); break;
    case EE_WON_HOST_RELEASE: Obj(o0)->Release(); break;
    case EE_WON_HOST_ADD_REF: Obj(o0)->AddRef(); break;
    // the request exchange (ee_won reqhost.rs: ServerRequestOp's statements)
    case EE_WON_HOST_AUTH_TYPE: c->out_num = o->mAuthType; break;
    case EE_WON_HOST_HAS_AUTH: c->out_num = o->mAuthContext.get() != NULL; break;
    case EE_WON_HOST_CONNECT_TIME: c->out_num = o->ConnectTime(); break;
    case EE_WON_HOST_TIMEOUT: c->out_num = (DWORD)o->GetTimeout(); break;
    case EE_WON_HOST_CERT_USER_ID:
        c->out_num = (uint32_t)o->mAuthContext->GetPeerData()->GetCertificate()->GetUserId(); // WONUserSeq
        break;
    case EE_WON_HOST_CTX_ADDRS: {
        ServerContext::AddrList aList;
        static_cast<ServerContext*>(Obj(o0))->CopyAddresses(aList);
        std::string aBytes;
        for (ServerContext::AddrList::const_iterator i = aList.begin(); i != aList.end(); ++i)
            aBytes.append(reinterpret_cast<const char*>(i->GetSixByte()), 6);
        Give(c, aBytes.data(), aBytes.size());
        break;
    }
    case EE_WON_HOST_CTX_FAILED: static_cast<ServerContext*>(Obj(o0))->NotifyFailed(Addr(c)); break;
    case EE_WON_HOST_GET_SESSION: {
        SmartPtr<AuthSession> aSession = o->mAuthContext->GetSession(Addr(c));
        c->out_obj = aSession.get() != NULL ? Out(aSession.get()) : NULL;
        break;
    }
    case EE_WON_HOST_PUT_SESSION: o->mAuthContext->PutSession(Addr(c), static_cast<AuthSession*>(Obj(o0))); break;
    case EE_WON_HOST_ADD_CRYPT:
        Sock(o0)->AddMsgTransform(new CryptTransform(static_cast<AuthSession*>(Obj(c->obj[1]))));
        break;
    case EE_WON_HOST_KILL_ALL: o->mTracker.KillAll(); break;
    case EE_WON_HOST_PEER_AUTH_SESSION: {
        AuthSession* aSession = static_cast<PeerAuthOp*>(Op(o0))->GetSession();
        c->out_obj = aSession != NULL ? Out(aSession) : NULL;
        break;
    }
    case EE_WON_HOST_PEER_AUTH_CERT: c->out_num = static_cast<PeerAuthOp*>(Op(o0))->GetGetCertStatus(); break;
    case EE_WON_HOST_OP_IS_RECV_MSG: c->out_num = dynamic_cast<RecvMsgOp*>(Op(o0)) != NULL; break;
    // an op derived in C++ (reqhost.rs's CppReq): its virtuals
    case EE_WON_HOST_REQ_NEXT: {
        ServerRequestOp* r = static_cast<ServerRequestOp*>(o);
        c->out_num = r->GetNextRequest();
        Give(c, r->mRequest.get());
        break;
    }
    case EE_WON_HOST_REQ_CHECK: {
        ServerRequestOp* r = static_cast<ServerRequestOp*>(o);
        r->mResponse = n0 ? Buf(c) : ByteBufferPtr();
        try {
            c->out_num = r->CheckResponse();
        } catch (ReadBufferException&) {
            c->out_addr[0] = 1;
        }
        break;
    }
    case EE_WON_HOST_REQ_RESET: static_cast<ServerRequestOp*>(o)->Reset(); break;
    // DetectFirewallOp's (ee_won fwhost.rs)
    case EE_WON_HOST_SOCKET_LISTEN: c->out_num = Sock(o0)->Listen(1000); break;
    case EE_WON_HOST_SOCKET_PORT: c->out_num = Sock(o0)->GetLocalPort(); break;
    case EE_WON_HOST_RUN: c->out_num = Op(o0)->Run((long)n0, (unsigned long)n1); break;
    // GetCertOp's objects (ee_won certhost.rs): round 6's hooks, one statement each
    case EE_WON_HOST_CERT_VERIFIER_PUBLIC: c->out_num = AuthContext::GetVerifierKey().IsPublic(); break;
    case EE_WON_HOST_CERT_CTX_PEER: {
        ConstSmartPtr<AuthPeerData> aPeer = Ctx(o0)->GetPeerData();
        c->out_obj = aPeer.get() != NULL ? Out(const_cast<AuthPeerData*>(aPeer.get())) : NULL;
        break;
    }
    case EE_WON_HOST_CERT_PEER_BLOCK: c->out_obj = V(Peer(o0)->GetPubKeyBlock()); break;
    case EE_WON_HOST_CERT_PEER_DELTA: c->out_num = Peer(o0)->GetAuthDelta(); break;
    case EE_WON_HOST_CERT_LOAD_SECRETS: Ctx(o0)->LoadSecretList(); break;
    case EE_WON_HOST_CERT_BLOCK_VALID: c->out_num = Block(o0)->IsValid(); break;
    case EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK: Ctx(o0)->SetPeerData(new AuthPeerData(Block(c->obj[1]))); break;
    case EE_WON_HOST_CERT_BLOCK_ID: c->out_num = Block(o0)->GetId(); break;
    case EE_WON_HOST_CERT_BLOCK_ENCRYPT: Give(c, Block(o0)->GetCurrentKey().Encrypt(c->data, (int)c->len).get()); break;
    case EE_WON_HOST_CERT_USER: GiveText(c, Ctx(o0)->GetUserName()); break;
    case EE_WON_HOST_CERT_PASSWORD: GiveText(c, Ctx(o0)->GetPassword()); break;
    case EE_WON_HOST_CERT_COMMUNITY: {
        WriteBuffer w(0, true);
        Ctx(o0)->AppendCommunityData(w);
        Give(c, w.data(), w.length());
        break;
    }
    case EE_WON_HOST_CERT_HASHES: {
        WriteBuffer w(0, true);
        Ctx(o0)->AppendHashes(w, RawBuffer(c->data, c->len));
        Give(c, w.data(), w.length());
        break;
    }
    case EE_WON_HOST_CERT_ADD_SECRET: Ctx(o0)->AddSecret(c->data); break;
    case EE_WON_HOST_CERT_REMOVE_SECRET: Ctx(o0)->RemoveSecret(c->data); break;
    case EE_WON_HOST_CERT_NEW:
        c->out_obj = Out(n0 == 1 ? static_cast<RefCount*>(new AuthCertificate(c->data, (unsigned long)c->len))
                                 : static_cast<RefCount*>(new Auth2Certificate(c->data, (unsigned long)c->len)));
        break;
    case EE_WON_HOST_CERT_VALID: c->out_num = Cert(n0, o0)->IsValid(); break;
    case EE_WON_HOST_CERT_BLOCK_VERIFY:
        c->out_num = n0 == 1 ? Block(o0)->Verify(Cert1(c->obj[1])) : Block(o0)->Verify(Cert2(c->obj[1]));
        break;
    case EE_WON_HOST_CERT_DELTA: c->out_num = n0 == 1 ? Cert1(o0)->ComputeAuthDelta() : Cert2(o0)->ComputeAuthDelta(); break;
    case EE_WON_HOST_CERT_TIMES: {
        int64_t aTimes[2] = {(int64_t)Cert(n0, o0)->GetIssueTime(), (int64_t)Cert(n0, o0)->GetExpireTime()};
        Give(c, aTimes, sizeof(aTimes));
        break;
    }
    case EE_WON_HOST_CERT_NEW_BLOCK: c->out_obj = Out(new AuthPubKeyBlock(c->data, (unsigned long)c->len)); break;
    case EE_WON_HOST_CERT_BLOCK_VERIFIED: c->out_num = Block(o0)->IsValidAndVerified(AuthContext::GetVerifierKey()); break;
    case EE_WON_HOST_CERT_COMMIT: Ctx(o0)->CommitSecrets((time_t)n0, (time_t)n1); break;
    case EE_WON_HOST_CERT_PRIV_NEW: c->out_obj = new ElGamal; break;
    case EE_WON_HOST_CERT_PRIV_SET: c->out_num = static_cast<ElGamal*>(o0)->SetPrivateKey(c->data, (int)c->len); break;
    case EE_WON_HOST_CERT_PRIV_FREE: delete static_cast<ElGamal*>(o0); break;
    case EE_WON_HOST_CERT_NEW_PEER:
        c->out_obj = Out(new AuthPeerData(Cert1(o0), Block(c->obj[1]), *reinterpret_cast<ElGamal*>((intptr_t)n0), (time_t)n1,
                                          Cert2(c->obj[2])));
        break;
    case EE_WON_HOST_CERT_SET_PASSWORD: {
        std::wstring aPassword;
        WideAssignUtf16LE(aPassword, c->data, (unsigned long)(c->len / 2));
        Ctx(o0)->SetPassword(aPassword);
        break;
    }
    case EE_WON_HOST_CERT_SET_PEER: Ctx(o0)->SetPeerData(Peer(c->obj[1])); break;
    case EE_WON_HOST_CERT_CLEAR_ONE_TIME: Ctx(o0)->ClearOneTimeData(); break;
    default: break;
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accd0
// (public: virtual void * __thiscall WONAPI::ServerOp::`scalar deleting destructor'(unsigned int)    [??_GServerOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accf0
// (public: virtual __thiscall WONAPI::ServerOp::~ServerOp(void)    [??1ServerOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae9f0
// (public: __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,clas)
