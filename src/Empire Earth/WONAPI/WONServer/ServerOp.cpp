// RustServerOp: an op of the Rust op base (ee_won hostop.rs) with ServerOp's members (hostbase.rs), the calls that
// stay C++ (new objects, an object's Rust object) answered on the C++ objects
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
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONElGamal.h"
#include <vector>
#include "ee/wonapi/WONLobbyResource.h" // WONFile
#include "ee/wonapi/WONLobbyGUI.h" // WONAPICore::GetDefaultFileDirectory
namespace WONAPI {
namespace {
RefCount* Obj(void* inObj) { return static_cast<RefCount*>(inObj); }
BlockingSocket* Sock(void* inObj) { return inObj ? static_cast<BlockingSocket*>(Obj(inObj)) : NULL; }
AsyncOp* Op(void* inObj) { return static_cast<AsyncOp*>(Obj(inObj)); }
void* Out(RefCount* inObj) { inObj->AddRef(); return inObj; } // a new object: the Rust op's reference
IPAddr Addr(const EeWonHostCall* c) { IPAddr a; a.SetSixByte(c->addr); return a; }
ByteBufferPtr Buf(const EeWonHostCall* c) { return new ByteBuffer(c->data, (unsigned long)c->len, false); }
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

RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp)
{
    // the calls that need no C++ object (the clock, rand, an address, the op's file, the files it measures, the web
    // proxy host: ee_won hostnative.rs) and the op's own (its status and the like, ServerOp's members, the tracker,
    // its completions, references: hostbase.rs) are the Rust op's
    ee_won_hostop_bind_native(inRustOp, RustServerOp::HostCall, this, static_cast<AsyncOp*>(this), this->GetOpObject(),
                              this->mTracker.GetTrackerObject());
}
// FUNCTION: WON 0x006ace20
// Sets the connect, send and receive time limits
void RustServerOp::SetMaxTimes(int inConnect, int inSend, int inRecv)
{
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_CONNECT, (DWORD)inConnect, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_SEND, (DWORD)inSend, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_RECV, (DWORD)inRecv, NULL);
}
// FUNCTION: WON 0x006acee0
// Copies the time limits of another server op
void RustServerOp::CopyMaxTimes(const RustServerOp* inSource)
{
    this->SetMaxTimes((int)ee_won_hostop_base_get(inSource->mRustOp, EE_WON_HOSTBASE_MAX_CONNECT),
                      (int)ee_won_hostop_base_get(inSource->mRustOp, EE_WON_HOSTBASE_MAX_SEND),
                      (int)ee_won_hostop_base_get(inSource->mRustOp, EE_WON_HOSTBASE_MAX_RECV));
}
BlockingSocket* RustServerOp::GetSocket() const
{
    return static_cast<BlockingSocket*>(
        reinterpret_cast<RefCount*>((intptr_t)ee_won_hostop_base_get(this->mRustOp, EE_WON_HOSTBASE_SOCKET)));
}
void RustServerOp::SetSocket(BlockingSocket* inSocket)
{
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_SOCKET, 0, static_cast<RefCount*>(inSocket));
}
AuthContext* RustServerOp::GetAuthContext() const
{
    return static_cast<AuthContext*>(
        reinterpret_cast<RefCount*>((intptr_t)ee_won_hostop_base_get(this->mRustOp, EE_WON_HOSTBASE_AUTH_CONTEXT)));
}
void RustServerOp::SetAuthType(AuthType inType, AuthContext* inContext)
{
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_AUTH_TYPE, inType, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_AUTH_CONTEXT, 0, static_cast<RefCount*>(inContext));
}
void RustServerOp::SetClientKeySize(unsigned char inSize, bool inUseClientKey)
{
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_CLIENT_KEY_SIZE, inSize, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_USE_CLIENT_KEY, inUseClientKey, NULL);
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
GetCertOp::GetCertOp(AuthContext* inContext) : RustServerOp(ee_won_hostop_getcert())
{
    this->Set(EE_WON_REQHOST_CONTEXT, inContext->GetServerContext().get());
    this->Set(EE_WON_GETCERT_AUTH_CONTEXT, inContext);
}
ConstSmartPtr<AuthPeerData> GetCertOp::GetPeerData() const
{
    return AuthPeerData::Adopt(reinterpret_cast<EeWonAuthObj*>((intptr_t)ee_won_hostop_num(this->GetRustOp(), EE_WON_GETCERT_PEER_DATA, 0)));
}
// FUNCTION: WON 0x006e6a10
// A peer auth op for an address, auth context, auth type and socket (ee_won peerop.rs)
PeerAuthOp::PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket,
                       bool inUseAuth2)
    : RustServerOp(ee_won_hostop_peerauth(reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()), inUseAuth2))
{
    this->SetAuthType(inType, inContext);
    this->SetSocket(inSocket);
}
// The session's Encrypt (send) or Decrypt (receive): the message replaced when the Rust side made a new one
WONStatus CryptTransform::Crypt(bool inDecrypt, ByteBufferPtr& ioMessage)
{
    ByteBufferPtr anOut;
    bool replaced = false;
    WONStatus aStatus = (WONStatus)ee_won_session_crypt(this->mSession, inDecrypt,
                                                        reinterpret_cast<const uint8_t*>(ioMessage->data()),
                                                        ioMessage->length(), WonByteBufferSink, &anOut, &replaced);
    if (replaced)
        ioMessage = anOut;
    return aStatus;
}
// FUNCTION: WON 0x006e8950
// A server-side peer auth op for an auth context and an accepted socket (ee_won peerop.rs)
PeerAuthServerOp::PeerAuthServerOp(AuthContext* inContext, BlockingSocket* inSocket)
    : RustServerOp(ee_won_hostop_peerauth_server())
{
    this->SetAuthType(AUTH_TYPE_NONE, inContext);
    this->SetSocket(inSocket);
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
DirEntityList RustServerOp::GetEntities(int inWhich, long long inIndex) const
{
    DirEntityList aList;
    ee_won_hostop_entities(this->mRustOp, inWhich, inIndex, DirEntityReplyParser::TakeEntity, &aList);
    return aList;
}
std::string RustServerOp::GetBytes(int inWhich, long long inIndex) const
{
    std::string aBytes;
    ee_won_hostop_bytes(this->mRustOp, inWhich, inIndex, StrSink, &aBytes);
    return aBytes;
}
namespace {
// a Rust request op's members in its kind's order (ee_won_reqops_new)
struct ReqMembers {
    std::list<std::string> mKeep;
    std::vector<EeWonAdminValue> mValues;
    ReqMembers& N(long long inNum)
    {
        EeWonAdminValue v = {static_cast<int64_t>(inNum), NULL, 0};
        this->mValues.push_back(v);
        return *this;
    }
    ReqMembers& S(const std::string& inBytes)
    {
        this->mKeep.push_back(inBytes);
        EeWonAdminValue v = {0, reinterpret_cast<const uint8_t*>(this->mKeep.back().data()), this->mKeep.back().size()};
        this->mValues.push_back(v);
        return *this;
    }
    ReqMembers& W(const std::wstring& inText)
    {
        std::string b;
        WideAppendUtf16LE(b, inText);
        return this->S(b);
    }
    // an empty profile (no newsletters, birth date or custom action)
    ReqMembers& Profile() { return this->N(0).N(0).N(0).N(0).N(0).N(0).S(std::string()); }
    EeWonReqOp* Make(int inKind) { return ee_won_reqops_new(inKind, this->mValues.empty() ? NULL : &this->mValues[0], this->mValues.size()); }
};
}
RustServerOp* RustServerOp::NewGetDir(ServerContext* inContext, const std::wstring& inPath, DWORD inFlags,
                                      const char* const* inTypes, int inTypeCount)
{
    ReqMembers m;
    m.N(inFlags).W(inPath).N(inTypeCount);
    for (int i = 0; i < inTypeCount; i++) m.S(inTypes[i]);
    return NewRequest(m.Make(EE_WON_REQOPS_DIR_GET), inContext);
}
RustServerOp* RustServerOp::NewGetMultiDir(ServerContext* inContext, DWORD inFlags, const std::wstring* inPaths, int inCount)
{
    // GetMultiDirOp::AddPath: each path with the op's flags and (no) data types
    ReqMembers m;
    m.N(inCount);
    for (int i = 0; i < inCount; i++) m.N(inFlags).W(inPaths[i]).N(0);
    return NewRequest(m.Make(EE_WON_REQOPS_DIR_MULTI), inContext);
}
RustServerOp* RustServerOp::NewGetService(ServerContext* inContext, const std::wstring& inPath, const std::wstring& inName,
                                          const std::string& inNetAddr)
{
    // GetServiceOp::Init's flags: the services, their display names, data objects, names and addresses
    ReqMembers m;
    m.N(0x06000822).W(inPath).W(inName).N(1).S(inNetAddr).N(0);
    return NewRequest(m.Make(EE_WON_REQOPS_DIR_SERVICE), inContext);
}
RustServerOp* RustServerOp::NewGetAccount(ServerContext* inContext, AuthContext* inAuth)
{
    ReqMembers m;
    RustServerOp* anOp = NewRequest(m.Profile().Make(EE_WON_REQOPS_ACCOUNT_GET), inContext);
    anOp->SetAuthType(AUTH_TYPE_SESSION, inAuth);
    return anOp;
}
RustServerOp* RustServerOp::NewUpdateAccount(ServerContext* inContext, AuthContext* inAuth, const std::wstring& inPassword,
                                             const std::string& inEmail)
{
    ReqMembers m;
    m.W(inPassword).S(inEmail).Profile();
    RustServerOp* anOp = NewRequest(m.Make(EE_WON_REQOPS_ACCOUNT_UPDATE), inContext);
    anOp->SetAuthType(AUTH_TYPE_SESSION, inAuth);
    return anOp;
}
RustServerOp* RustServerOp::NewCreateAccount(ServerContext* inContext, const std::wstring& inUserName,
                                             const std::wstring& inPassword, const std::string& inEmail,
                                             bool inNewsletter, unsigned char inMonth, unsigned char inDay,
                                             unsigned short inYear)
{
    // the lobby's profile: newsletter 1 and the birth date
    ReqMembers m;
    m.W(inUserName).W(inPassword).S(inEmail).N(1).N(1).N(inNewsletter).N(inMonth).N(inDay).N(inYear).N(0).N(0);
    return NewRequest(m.S(std::string()).Make(EE_WON_REQOPS_CREATE_ACCOUNT), inContext);
}
RustServerOp* RustServerOp::NewGroupListReq(const IPAddr& inAddr, unsigned char inFlags)
{
    ReqMembers m;
    return NewRequest(m.N(inFlags).Make(EE_WON_REQOPS_GROUP_LIST), inAddr);
}
namespace {
// HTTPGetOp::ReadProxyHostFile(false) as its constructors ran it: _wonwebproxy.txt in the WON directory, its first
// line, once per process (ee_won hostnative.rs)
void ReadHttpProxyFile()
{
    const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
    ee_won_http_proxy_read(reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size());
}
}
RustServerOp* RustServerOp::NewHttpGet(const std::string& inHost, const std::string& inPath)
{
    RustServerOp* anOp = new RustServerOp(ee_won_httpget_new(reinterpret_cast<const uint8_t*>(inHost.data()), inHost.size(),
                                                             reinterpret_cast<const uint8_t*>(inPath.data()), inPath.size()));
    ReadHttpProxyFile();
    return anOp;
}
RustServerOp* RustServerOp::NewHttpGet(const std::string& inURL)
{
    RustServerOp* anOp = new RustServerOp(ee_won_httpget_new_url(reinterpret_cast<const uint8_t*>(inURL.data()), inURL.size()));
    ReadHttpProxyFile();
    return anOp;
}
void RustServerOp::SetBytes(int inWhich, const std::string& inBytes)
{
    ee_won_hostop_set(this->mRustOp, inWhich, 0, reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.size(), NULL,
                      NULL);
}
BlockingSocket* RustServerOp::GetHttpSocket() const
{
    BlockingSocket* aSocket = Sock(reinterpret_cast<void*>((intptr_t)this->GetNum(EE_WON_HTTPGET_SOCKET)));
    if (aSocket == NULL || !aSocket->IsConnected()) return NULL;
    return aSocket;
}
time_t RustServerOp::GetHttpLastModifiedTime()
{
    time_t aTime = (time_t)this->GetNum(EE_WON_HTTPGET_LAST_MODIFIED);
    if (aTime != 0) return aTime;
    std::string aLocal = this->GetBytes(EE_WON_HTTPGET_LOCAL_PATH);
    if (aLocal.empty()) return 0;
    WONFile aFile(aLocal);
    aTime = aFile.GetModifyTime();
    this->SetNum(EE_WON_HTTPGET_LAST_MODIFIED, (long long)aTime);
    return aTime;
}
RustServerOp* RustServerOp::NewHttpDocument(const std::string& inProduct, int inType)
{
    const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
    return new RustServerOp(ee_won_httpdoc_new(inType, reinterpret_cast<const uint8_t*>(inProduct.data()), inProduct.size(),
                                               reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size()));
}
RustServerOp::~RustServerOp()
{
    ee_won_hostop_free(this->mRustOp); // its objects released through HostCall, its file closed
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
    long long n0 = c->num[0];
    switch (c->code) {
    // new objects (one reference the Rust op owns)
    case EE_WON_HOST_NEW_SOCKET: c->out_obj = Out(new BlockingSocket(n0 ? AsyncSocket::UDP : AsyncSocket::TCP)); break;
    case EE_WON_HOST_NEW_OP: c->out_obj = Out(NewSocketOp(c)); break;
    case EE_WON_HOST_PEER_NEW_LIGHT: c->out_obj = Out(new AsyncOp); break; // a plain op (a timer: its completion Rust's)
    case EE_WON_HOST_NEW_PEER_AUTH: {
        PeerAuthOp* anOp = new PeerAuthOp(Addr(c), o->GetAuthContext(), o->GetAuthType(), Sock(o0), n0 != 0);
        anOp->CopyMaxTimes(o);
        c->out_obj = Out(anOp);
        break;
    }
    case EE_WON_HOST_NEW_HTTP_GET: {
        std::string aTarget(reinterpret_cast<const char*>(c->data), c->len);
        c->out_obj = Out(NewHttpGet(aTarget.substr(0, (size_t)n0), aTarget.substr((size_t)n0)));
        break;
    }
    // C++ objects the Rust op drives: a session's transform, an HTTP session's queue
    case EE_WON_HOST_ADD_CRYPT:
        Sock(o0)->AddMsgTransform(new CryptTransform(c->obj[1]));
        break;
    case EE_WON_HOST_SESSION_ADD:
        static_cast<HTTPSession*>(Obj(o0))->AddOp(static_cast<RustServerOp*>(Op(c->obj[1])));
        break;
    // an object's Rust object: an auth context's (ee_won hostauth.rs), and by kind a socket's, a queue socket's, an
    // op's, a socket op's, a Rust op's, a server context's list (hostsock.rs, hostbase.rs), or NULL
    case EE_WON_HOST_AUTH_CORE:
        c->out_obj = o0 != NULL ? const_cast<EeWonAuthCtx*>(static_cast<AuthContext*>(Obj(o0))->Core()) : NULL;
        break;
    case EE_WON_HOST_SOCK_CORE: {
        RefCount* r = o0 != NULL ? Obj(o0) : o;
        AsyncSocket* s = dynamic_cast<AsyncSocket*>(r);
        QueueSocket* q = dynamic_cast<QueueSocket*>(r);
        SocketOp* so = dynamic_cast<SocketOp*>(r);
        AsyncOp* op = dynamic_cast<AsyncOp*>(r);
        RustServerOp* h = dynamic_cast<RustServerOp*>(r);
        ServerContext* x = dynamic_cast<ServerContext*>(r);
        c->out_obj = n0 == EE_WON_HOST_CORE_SOCKET ? (s != NULL ? (void*)s->GetSocketObject() : NULL)
                     : n0 == EE_WON_HOST_CORE_QUEUE ? (q != NULL ? (void*)q->GetQueueObject() : NULL)
                     : n0 == EE_WON_HOST_CORE_OP ? (op != NULL ? (void*)op->GetOpObject() : NULL)
                     : n0 == EE_WON_HOST_CORE_HOSTOP ? (h != NULL ? (void*)h->GetRustOp() : NULL)
                     : n0 == EE_WON_HOST_CORE_CONTEXT ? (x != NULL ? (void*)x->GetListObject() : NULL)
                     : (so != NULL ? (void*)so->GetSocketOpObject() : NULL);
        break;
    }
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
