// RustServerOp: an op of the Rust op base (ee_won hostop.rs) with ServerOp's members (hostbase.rs); its calls on C++
// objects are answered over the core table (hostcore.rs: HostCall is gone, net round 29)
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
#include "rust/won_net/certhost.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/reqop.h"
namespace WONAPI {
namespace {
// the Rust objects' maker of a Rust op's C++ op (ee_won cppcore.rs new_host_op: the peer auth and HTTP get ops
// a Rust op makes, GameSpySupport's service lookup)
void* NewHostOp(EeWonHostOp* inOp) { return static_cast<RefCount*>(static_cast<AsyncOp*>(new RustServerOp(inOp))); }
struct HostOpMaker {
    HostOpMaker() { ee_won_core_set_new_host_op(NewHostOp); }
} sHostOpMaker;
}
RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp)
{
    // the calls that need no C++ object (the clock, rand, an address, the op's file, the files it measures, the web
    // proxy host: ee_won hostnative.rs) and the op's own (its status and the like, ServerOp's members, the tracker,
    // its completions, references: hostbase.rs) are the Rust op's
    ee_won_hostop_bind_native(inRustOp, NULL, this, static_cast<AsyncOp*>(this), this->GetOpObject(),
                              this->mTracker.GetTrackerObject());
}
// Sets the connect, send and receive time limits
void RustServerOp::SetMaxTimes(int inConnect, int inSend, int inRecv)
{
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_CONNECT, (DWORD)inConnect, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_SEND, (DWORD)inSend, NULL);
    ee_won_hostop_base(this->mRustOp, EE_WON_HOSTBASE_MAX_RECV, (DWORD)inRecv, NULL);
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
// A peer auth op for an address, auth context, auth type and socket (ee_won peerop.rs)
PeerAuthOp::PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket,
                       bool inUseAuth2)
    : RustServerOp(ee_won_hostop_peerauth(reinterpret_cast<const uint8_t*>(inAddr.GetSixByte()), inUseAuth2))
{
    this->SetAuthType(inType, inContext);
    this->SetSocket(inSocket);
}
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
// the requests' members are Rust's (ee_won reqmake.rs); wide texts as wchar_t units of their size
#define EE_WON_WIDE(t) (t).data(), (t).size()
RustServerOp* RustServerOp::NewGetDir(ServerContext* inContext, const std::wstring& inPath, DWORD inFlags,
                                      const char* const* inTypes, int inTypeCount)
{
    return NewRequest(ee_won_reqmake_get_dir(inFlags, EE_WON_WIDE(inPath), sizeof(wchar_t), inTypes, inTypeCount), inContext);
}
RustServerOp* RustServerOp::NewGetMultiDir(ServerContext* inContext, DWORD inFlags, const std::wstring* inPaths, int inCount)
{
    std::vector<const void*> aPaths(inCount + 1);
    std::vector<size_t> aLengths(inCount + 1);
    for (int i = 0; i < inCount; i++) { aPaths[i] = inPaths[i].data(); aLengths[i] = inPaths[i].size(); }
    return NewRequest(ee_won_reqmake_get_multi_dir(inFlags, &aPaths[0], &aLengths[0], inCount, sizeof(wchar_t)), inContext);
}
RustServerOp* RustServerOp::NewGetAccount(ServerContext* inContext, AuthContext* inAuth)
{
    RustServerOp* anOp = NewRequest(ee_won_reqmake_get_account(), inContext);
    anOp->SetAuthType(AUTH_TYPE_SESSION, inAuth);
    return anOp;
}
RustServerOp* RustServerOp::NewUpdateAccount(ServerContext* inContext, AuthContext* inAuth, const std::wstring& inPassword,
                                             const std::string& inEmail)
{
    RustServerOp* anOp = NewRequest(ee_won_reqmake_update_account(EE_WON_WIDE(inPassword), sizeof(wchar_t),
                                                                  WonBytes(inEmail), inEmail.size()), inContext);
    anOp->SetAuthType(AUTH_TYPE_SESSION, inAuth);
    return anOp;
}
RustServerOp* RustServerOp::NewCreateAccount(ServerContext* inContext, const std::wstring& inUserName,
                                             const std::wstring& inPassword, const std::string& inEmail,
                                             bool inNewsletter, unsigned char inMonth, unsigned char inDay,
                                             unsigned short inYear)
{
    return NewRequest(ee_won_reqmake_create_account(EE_WON_WIDE(inUserName), EE_WON_WIDE(inPassword), sizeof(wchar_t),
                                                    WonBytes(inEmail), inEmail.size(), inNewsletter, inMonth, inDay,
                                                    inYear), inContext);
}
RustServerOp* RustServerOp::NewGroupListReq(const IPAddr& inAddr, unsigned char inFlags)
{
    return NewRequest(ee_won_reqmake_group_list(inFlags), inAddr);
}
RustServerOp* RustServerOp::NewHttpGet(const std::string& inHost, const std::string& inPath)
{
    return new RustServerOp(ee_won_httpget_open(reinterpret_cast<const uint8_t*>(inHost.data()), inHost.size(),
                                                reinterpret_cast<const uint8_t*>(inPath.data()), inPath.size()));
}
RustServerOp* RustServerOp::NewHttpGet(const std::string& inURL)
{
    return new RustServerOp(ee_won_httpget_open_url(reinterpret_cast<const uint8_t*>(inURL.data()), inURL.size()));
}
void RustServerOp::SetBytes(int inWhich, const std::string& inBytes)
{
    ee_won_hostop_set(this->mRustOp, inWhich, 0, reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.size(), NULL,
                      NULL);
}
BlockingSocket* RustServerOp::GetHttpSocket() const
{
    BlockingSocket* aSocket = static_cast<BlockingSocket*>(reinterpret_cast<RefCount*>((intptr_t)this->GetNum(EE_WON_HTTPGET_SOCKET)));
    if (aSocket == NULL || !aSocket->IsConnected()) return NULL;
    return aSocket;
}
RustServerOp* RustServerOp::NewHttpDocument(const std::string& inProduct, int inType)
{
    const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
    return new RustServerOp(ee_won_httpdoc_new(inType, reinterpret_cast<const uint8_t*>(inProduct.data()), inProduct.size(),
                                               reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size()));
}
RustServerOp::~RustServerOp()
{
    ee_won_hostop_free(this->mRustOp); // its objects released, its file closed
}
void RustServerOp::RunHook() { ee_won_hostop_run(this->mRustOp); }
void RustServerOp::CleanupHook() { ee_won_hostop_cleanup(this->mRustOp); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam)
{
    return ee_won_hostop_callback(this->mRustOp, inParam, inOp);
}
}
