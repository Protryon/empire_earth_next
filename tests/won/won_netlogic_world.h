// won_lobbyops_test's stand-ins for the Internet logics' moved host cases (R6 WON round 32: AccountLogic's and
// InitLogic's, ee_won_lobby netlogicops.rs), over won_lobbyops_world.h's: the classes and lobby functions the deleted
// cases (won_netlogic_orig.h) and the hosts' new one-statement cases (won_netlogic_new.h) name, each call logged in
// the one trace. The lobby's events are real Rust events (lobbyevt.rs) under a shell the Rust finds by its address,
// read by both sides through the header's readers (LobbyEvtGet, LobbyEvtStr, ...). A request op made from a Rust
// request (`RustServerOp::NewRequest`) logs the request's description.
#ifndef WON_NETLOGIC_WORLD_H
#define WON_NETLOGIC_WORLD_H
#include <string.h>
#include <list>
#include "rust/won_lobby/lobbyevt.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/ffi.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/userstatus.h"
#include "rust/won_lobby/routelogic.h"
#include <map>
#include "rust/won_net/httpcache.h"

namespace LoWorld {
struct GUIStringData : RefCount {
    std::string s;
    explicit GUIStringData(const std::wstring& w) : s(Narrow(w)) {}
    explicit GUIStringData(const char* t) : s(t) {}
};
inline GUIString::GUIString(GUIStringData* d) : s(d != NULL ? d->s : std::string()) {}
} // namespace LoWorld

namespace NlWorld {
using namespace LoWorld;
static GUIStringData* GetEmptyGUIStringData() { return NULL; }
static GUIString gEmptyGUIString;
static unsigned long gLobbyAcceptTimeout = 20000;
// WONLobbyResourceGlobals.h's control numbers, as they are
enum LobbyResourceCtrlId {
    LobbyResourceCtrlId_Min = 200000, ID_ShowCreateAccount, ID_ShowLostPassword, ID_ShowLostUserName, ID_ShowMOTD, ID_ShowTOU,
    ID_LoginScreenBack, ID_ShowLoginHelp, ID_ShowInternetScreen, ID_ShowLanScreen, ID_Logout, ID_HostGame, ID_ChangeRoom,
    ID_JoinGame, ID_LeaveGame, ID_StartGame, ID_RefreshGameList, ID_AddFriend, ID_RefreshFriends, ID_AddLANName,
    ID_ChangeLANBroadcast, ID_FilterDirtyWordsCheck, ID_AnonymousToFriendsCheck, ID_ChooseNetworkAdapter,
};
enum HTTPDocType { HTTPDocType_MOTD = 0, HTTPDocType_TOU = 1, HTTPDocType_ADV = 2 };

// ---- the lobby's events: real Rust events under a shell
struct ComponentEvent {
    EeWonLobbyEvt* mEvt = NULL;
    int mType = 0;
    void* mComponent = NULL;
};
typedef EeWonLobbyEvtField LobbyEvtField;
inline intptr_t LobbyEvtGet(const ComponentEvent* e, LobbyEvtField f) { return ee_won_lobbyevt_get(e->mEvt, f); }
inline GUIString LobbyEvtText(const ComponentEvent* e, LobbyEvtField f) { return GUIString((GUIStringData*)LobbyEvtGet(e, f)); }
template <class S> S LobbyEvtStr(const ComponentEvent* e, LobbyEvtField f)
{
    size_t n = ee_won_lobbyevt_len(e->mEvt, f);
    return S((const typename S::value_type*)LobbyEvtGet(e, f), n / sizeof(typename S::value_type));
}
template <class S> void LobbyEvtSetStr(ComponentEvent* e, LobbyEvtField f, const S& s)
{ ee_won_lobbyevt_set_bytes(e->mEvt, f, (const uint8_t*)s.data(), s.size() * sizeof(typename S::value_type)); }
static void EvtRefs(EeWonLobbyEvtRef inOp, intptr_t inObj)
{
    if (inOp == EeWonLobbyEvtRef::AddRef) ((RefCount*)inObj)->AddRef();
    else ((RefCount*)inObj)->Release();
}
// a new event of `inKind` over its maker's values, bound to `ioShell`
static void EvtMake(ComponentEvent& ioShell, EeWonLobbyEvtKind inKind, const std::vector<intptr_t>& inArgs)
{
    ioShell.mEvt = ee_won_lobbyevt_new(inKind, inArgs.data(), inArgs.size(), EvtRefs);
    ee_won_lobbyevt_bind(ioShell.mEvt, &ioShell);
}
static void EvtFree(ComponentEvent& ioShell)
{
    ee_won_lobbyevt_free(ioShell.mEvt);
    ioShell.mEvt = NULL;
}
// the event's copy of a string member, logged
static std::string EvtDump(const ComponentEvent* e, LobbyEvtField f)
{
    const char* p = (const char*)LobbyEvtGet(e, f);
    size_t n = ee_won_lobbyevt_len(e->mEvt, f);
    std::string s;
    for (size_t i = 0; i < n; i++) s += N((unsigned char)p[i]) + ".";
    return s;
}

// WONBuffer.h's, as it is
inline void WideAppendUtf16LE(std::string& outBytes, const std::wstring& inText)
{
    size_t at = outBytes.length();
    size_t count = ee_won_utf16le_from_wide(inText.data(), sizeof(wchar_t), inText.length(), NULL, 0);
    outBytes.resize(at + count);
    if (count > 0)
        ee_won_utf16le_from_wide(inText.data(), sizeof(wchar_t), inText.length(), reinterpret_cast<uint8_t*>(&outBytes[at]), count);
}

// the events the cases broadcast: their values, logged
struct WEvent {
    std::string mDesc;
};
static void LobbyBroadcastEvent(WEvent* e, bool f)
{
    Log("broadcast " + e->mDesc + " " + N(f));
    delete e;
}
struct NativeImage : RefCount {};
static WEvent* SetLoginFieldsEvent(const GUIString& n, const GUIString& p, bool r) { return new WEvent{"SetLoginFields " + n.s + " " + p.s + " " + N(r)}; }
static WEvent* LoginInitStatusEvent(const GUIString& s, const GUIString& d, bool a, int k)
{
    return new WEvent{"LoginInitStatus " + s.s + " [" + d.s + "] " + N(a) + " " + N(k)};
}
static WEvent* SetCrossPromotionEvent(NativeImage* i, const std::string& l) { return new WEvent{"SetCrossPromotion " + Id(i) + " " + l}; }

// ---- the dialogs and screens
static int sDialogResult;
static int LobbyDoYesNoDialog(const GUIString& t, const GUIString& d, bool f)
{
    Log("YesNo " + t.s + " " + d.s + " " + N(f));
    return sDialogResult;
}
static int LobbyDoDialog(Dialog* d)
{
    Log("DoDialog " + Id(d));
    return sDialogResult;
}
static void LobbyShowCreateAccount() { Log("ShowCreateAccount"); }
static void LobbyShowLostPassword() { Log("ShowLostPassword"); }
static void LobbyShowLostUsername() { Log("ShowLostUsername"); }

// ---- the servers and ops
struct DirEntityList {
    int mOp, mWhich;
    long long mIndex;
    bool empty() const { Log("DirEntityList empty"); return this->mOp % 2 == 0; }
    ~DirEntityList() { Log("delete DirEntityList #" + N(this->mOp)); }
};
struct ServerContext : RefCount {
    void Clear() { Log(Id(this) + " Clear"); }
    void RemoveAllAddresses() { Log(Id(this) + " RemoveAllAddresses"); }
    void AddAddressesFromDir(const DirEntityList& l, const wchar_t* f)
    {
        Log(Id(this) + " AddAddressesFromDir #" + N(l.mOp) + " " + N(l.mWhich) + " " + N(l.mIndex) + " " + N(f != NULL));
    }
    void NotifyFailed(const IPAddr& a) { Log(Id(this) + " NotifyFailed " + N(a.mValue)); }
};
static void InitAddAll(ServerContext* inList, const std::list<std::string>& inNames)
{
    std::string s;
    for (const std::string& n : inNames) s += n + ",";
    Log("InitAddAll " + Id(inList) + " " + s);
}
// a Rust request's description (the op it makes, described and freed)
static std::string ReqText(EeWonReqOp* inOp)
{
    EeWonHostOp* h = ee_won_hostop_request(inOp);
    std::string s;
    ee_won_hostop_describe(h, [](void* c, const uint8_t* d, size_t n) { static_cast<std::string*>(c)->assign((const char*)d, n); }, &s);
    ee_won_hostop_free(h);
    return s;
}
static std::string Hex(const std::string& b)
{
    std::string s;
    for (unsigned char c : b) s += N(c) + ".";
    return s;
}
struct RustServerOp : AsyncOp {
    explicit RustServerOp(const std::string& inWhat) { Log("new RustServerOp " + Id(this) + " " + inWhat); }
    static RustServerOp* NewRequest(EeWonReqOp* inOp, ServerContext* c) { return new RustServerOp("request " + ReqText(inOp) + " ctx " + Id(c)); }
    static RustServerOp* NewGetAccount(ServerContext* c, AuthContext* a) { return new RustServerOp("get account " + Id(c) + " " + Id(a)); }
    static RustServerOp* NewUpdateAccount(ServerContext* c, AuthContext* a, const GUIString& p, const GUIString& e)
    {
        return new RustServerOp("update account " + Id(c) + " " + Id(a) + " " + p.s + " " + e.s);
    }
    static RustServerOp* NewCreateAccount(ServerContext* c, const std::wstring& u, const std::wstring& p, const std::string& e,
                                          bool n, unsigned char m, unsigned char d, unsigned short y)
    {
        return new RustServerOp("create account " + Id(c) + " " + Narrow(u) + " " + Narrow(p) + " " + e + " " + N(n) + " " + N(m) +
                                " " + N(d) + " " + N(y));
    }
    static RustServerOp* NewHttpGet(const std::string& u) { return new RustServerOp("http get " + u); }
    static RustServerOp* NewHttpDocument(const std::string& p, int t) { return new RustServerOp("http document " + p + " " + N(t)); }
    static RustServerOp* NewGetMultiDir(ServerContext* c, DWORD f, const std::wstring* p, int n)
    {
        std::string s = "multi dir " + Id(c) + " " + N(f);
        for (int i = 0; i < n; i++) s += " " + Narrow(p[i]);
        return new RustServerOp(s);
    }
    void SetNum(int w, long long v) { Log(Id(this) + " SetNum " + N(w) + " " + N(v)); }
    void Set(int w, RefCount* o) { Log(Id(this) + " Set " + N(w) + " " + Id(o)); }
    void SetBytes(int w, const std::string& b) { Log(Id(this) + " SetBytes " + N(w) + " " + b); }
    std::string GetBytes(int w) { Log(Id(this) + " GetBytes " + N(w)); return "path" + N(this->mId); }
    static RustServerOp* NewGetDir(ServerContext* c, const std::wstring& p, DWORD f, const char* const* t, int n)
    {
        std::string s = "dir " + Id(c) + " " + Narrow(p) + " " + N((long long)f);
        for (int i = 0; i < n; i++) s += std::string(" ") + t[i];
        return new RustServerOp(s);
    }
    static RustServerOp* NewGroupListReq(const IPAddr& a, unsigned char f) { return new RustServerOp("group list " + N(a.mValue) + " " + N(f)); }
    DirEntityList GetEntities(int w, long long i = 0) { Log(Id(this) + " GetEntities " + N(w) + " " + N(i)); return DirEntityList{this->mId, w, i}; }
    IPAddr GetCurAddr() { Log(Id(this) + " GetCurAddr"); IPAddr a; a.mValue = 0xc000 + this->mId; return a; }
    RustServerOp* GetRustOp() { return this; }
    void SetCompletion(Completion* c) { Log(Id(this) + " SetCompletion " + c->mName); delete c; }
};
// the cache setter and the global cache (InitLogic's HttpGetSetCache)
struct WONAPICore {
    static const std::string& GetDefaultFileDirectory() { static std::string s("wondir"); return s; }
};
static RefCount* WorldCache(const uint8_t* d, size_t n) { Log("http cache global " + std::string((const char*)d, n)); return NULL; }
static void WorldHostopSet(RustServerOp* op, int w, RefCount* o) { Log(Id(op) + " hostop set " + N(w) + " " + Id(o)); }
#define ee_won_http_cache_global(d, n) NlWorld::WorldCache(d, n)
#define ee_won_hostop_set(op, w, n, d, l, a, o) NlWorld::WorldHostopSet(op, w, o)
struct DNSTestOp : AsyncOp {
    explicit DNSTestOp(const std::string& h) { Log("new DNSTestOp " + Id(this) + " " + h); }
};
struct DetectFirewallOp : AsyncOp {
    explicit DetectFirewallOp(ServerContext* c) { Log("new DetectFirewallOp " + Id(this) + " " + Id(c)); }
    void SetListenPort(int p) { Log(Id(this) + " SetListenPort " + N(p)); }
    void SetDoListen(bool b) { Log(Id(this) + " SetDoListen " + N(b)); }
    void SetWaitForConnect(bool b) { Log(Id(this) + " SetWaitForConnect " + N(b)); }
    IPAddr GetCurAddr() { Log(Id(this) + " GetCurAddr"); IPAddr a; a.mValue = 0xd000 + this->mId; return a; }
};
struct HTTPSession : RefCount {
    void AddOp(RustServerOp* o) { Log(Id(this) + " AddOp " + Id(o)); }
};
static NativeImage* sImage;
struct Window {
    static Window* GetDefaultWindow() { static Window w; return &w; }
    SmartPtr<NativeImage> DecodeImage(const char* p) { Log(std::string("DecodeImage ") + p); return SmartPtr<NativeImage>(sImage); }
};
struct WONFile {
    std::string mPath;
    explicit WONFile(const std::string& p) : mPath(p) {}
    void SetFilePath(const std::string& p) { this->mPath = p; }
    long GetModifyTime() { Log("GetModifyTime " + this->mPath); return (long)this->mPath.size() * 1000; }
};
static std::string LobbyGetSysTOUPath() { Log("GetSysTOUPath"); return "sys.tou"; }
static std::string LobbyGetGameTOUPath() { Log("GetGameTOUPath"); return "game_terms.tou"; }
static void LobbySetTOUTimes(long s, long g) { Log("SetTOUTimes " + N(s) + " " + N(g)); }
static void LobbyGetTOUTimes(long& s, long& g)
{
    Log("GetTOUTimes");
    s = 1111;
    g = 2222;
}

// ---- the users
struct LobbyUserInfo : RefCount {};
static LobbyUserInfo* sUser;
static bool sNewer;
static LobbyUserInfo* LobbyAddUser(const std::wstring& n, const GUIString& p, bool f)
{
    Log("AddUser " + Narrow(n) + " [" + p.s + "] " + N(f));
    return sUser;
}
static void LobbyUserSetTOU(LobbyUserInfo* u, long s, long g) { Log("UserSetTOU " + Id(u) + " " + N(s) + " " + N(g)); }
static void LobbySetCurrentUser(LobbyUserInfo* u) { Log("SetCurrentUser " + Id(u)); }
static LobbyUserInfo* LobbyFindUser(const GUIString& n)
{
    Log("FindUser " + n.s);
    return sUser;
}
static long LobbyUserTOU(LobbyUserInfo* u, int i)
{
    Log("UserTOU " + Id(u) + " " + N(i));
    return 100 + i;
}
static bool LobbyIsTOUNewer(long a, long b)
{
    Log("IsTOUNewer " + N(a) + " " + N(b));
    return sNewer;
}

// ---- the server lists (1 auth, 2 account, 3 firewall detect, 4 patch, 5 directory, 6 room directory)
static ServerContext* sLists[7];
static ServerContext* LobbyGetAuthServers() { Log("GetAuthServers"); return sLists[1]; }
static ServerContext* LobbyGetAccountServers() { Log("GetAccountServers"); return sLists[2]; }
static ServerContext* LobbyGetFirewallDetectServers() { Log("GetFirewallDetectServers"); return sLists[3]; }
static ServerContext* LobbyGetPatchServers() { Log("GetPatchServers"); return sLists[4]; }
static ServerContext* LobbyGetDirServers() { Log("GetDirServers"); return sLists[5]; }
static ServerContext* LobbyGetRoomDirServers() { Log("GetRoomDirServers"); return sLists[6]; }
// InitLogic.cpp's, as it is
static ServerContext* InitList(intptr_t inWhich)
{
    switch (inWhich) {
    case 1: return LobbyGetAuthServers();
    case 2: return LobbyGetAccountServers();
    case 3: return LobbyGetFirewallDetectServers();
    case 4: return LobbyGetPatchServers();
    }
    return NULL;
}

// ---- the logics' shells
struct AccountLogic {
    SmartPtr<Dialog> mCreditsDialog, mTOUDialog;
};
struct InitLogic {
    SmartPtr<SocketOp> mAcceptOp;
    SmartPtr<AsyncSocket> mFirewallSocket;
    IPAddr mFirewallAddress;
    SmartPtr<Dialog> mNetworkAdapterDialog;
    SmartPtr<HTTPSession> mHTTPSession;
    std::string mCrossPromotion;
    void AcceptCompletion(AsyncOp*) {}
    void DNSCompletion(AsyncOp*) {}
    void HTTPDocumentCompletion(AsyncOp*) {}
    void DirectoryCompletion(AsyncOp*) {}
    void VersionCompletion(AsyncOp*) {}
    void FirewallCompletion(AsyncOp*) {}
    void CrossPromotionImageCompletion(AsyncOp*) {}
    void RunOp(AsyncOp* o, Completion* c, unsigned long t) { Log("RunOp " + Id(o) + " " + c->mName + " " + N((long long)t)); delete c; }
    void RunTrackedOp(AsyncOp* o, Completion* c, int t) { Log("RunTrackedOp " + Id(o) + " " + c->mName + " " + N(t)); delete c; }
};
} // namespace NlWorld

namespace LoWorld {
template <> inline LogicCompletion<NlWorld::InitLogic>::LogicCompletion(void (NlWorld::InitLogic::*inFn)(AsyncOp*), NlWorld::InitLogic*)
{
    typedef NlWorld::InitLogic L;
    this->mName = inFn == &L::AcceptCompletion ? "AcceptCompletion" : inFn == &L::DNSCompletion ? "DNSCompletion"
                : inFn == &L::HTTPDocumentCompletion ? "HTTPDocumentCompletion" : inFn == &L::DirectoryCompletion ? "DirectoryCompletion"
                : inFn == &L::VersionCompletion ? "VersionCompletion" : inFn == &L::FirewallCompletion ? "FirewallCompletion"
                : inFn == &L::CrossPromotionImageCompletion ? "CrossPromotionImageCompletion" : "?";
}
} // namespace LoWorld

// ---- RoutingLogic's (R6 WON round 32: its moved cases)
namespace NlWorld {
typedef WEvent LobbyEvent;
typedef long RoutingOpType;
enum { LobbyEvent_CreateRoom = 100011, LobbyEvent_JoinRoom = 100028, LobbyEvent_QueryServerGroups = 100042 };
static void Broadcast(LobbyEvent* inEvent) { LobbyBroadcastEvent(inEvent, true); }
static WEvent* AlertEvent(int k, int v, unsigned long s, const GUIString& t)
{
    return new WEvent{"Alert " + N(k) + " " + N(v) + " " + N((long long)s) + " " + t.s};
}
static WEvent* ClientModeratedEvent(LobbyClient* c, int spec, const GUIString& name, int kind, unsigned long time, bool f,
                                    const GUIString& comment)
{
    return new WEvent{"ClientModerated " + Id(c) + " " + N(spec) + " " + name.s + " " + N(kind) + " " + N((long long)time) +
                      " " + N(f) + " " + comment.s};
}
static IPAddr LobbyEvtAddr(const ComponentEvent* e, LobbyEvtField)
{
    IPAddr a;
    a.mValue = 0xe000 + e->mType % 0x1000;
    return a;
}
static bool sConnected, sOnInternet;
static int LobbyOnInternetScreen() { Log("OnInternetScreen"); return sOnInternet; }
static int LobbyUserCanceledLastStatusOp() { Log("UserCanceledLastStatusOp"); return !sOnInternet; }
struct LobbyServer : RefCount {
    GUIString GetName() { return GUIString(("server" + N(this->mId)).c_str()); }
    IPAddr GetAddress() { Log(Id(this) + " GetAddress"); IPAddr a; a.mValue = 0xa100 + this->mId; return a; }
};
struct LobbyGroup : RefCount {
    GUIString GetName() { return GUIString(("group" + N(this->mId)).c_str()); }
};
static LobbyServer* sServer;
struct LobbyServerList {
    LobbyServer* FindServer(const IPAddr& a) { Log("FindServer " + N(a.mValue)); return sServer; }
    IPAddr AddressAt(size_t i) { Log("AddressAt " + N((long long)i)); IPAddr a; a.mValue = 0xa200 + (intptr_t)i; return a; }
};
struct UDPManager : RefCount {};
struct WriteBuffer {
    std::string mData;
    WriteBuffer(int n, bool f) { Log("new WriteBuffer " + N(n) + " " + N(f)); }
    ~WriteBuffer() { Log("delete WriteBuffer " + this->mData); }
    SmartPtr<ByteBuffer> ToByteBuffer(bool f)
    {
        Log("ToByteBuffer " + N(f));
        ByteBuffer* b = new ByteBuffer;
        b->mBytes = this->mData;
        return SmartPtr<ByteBuffer>(b);
    }
};
// the routing connection's requests' values, logged
struct RoutingRequestValues {
    std::string mValues;
    RoutingRequestValues& N(long long v) { this->mValues += " n" + LoWorld::N(v); return *this; }
    RoutingRequestValues& S(const std::string& v) { this->mValues += " s" + v; return *this; }
    RoutingRequestValues& W(const std::wstring& v) { this->mValues += " w" + Narrow(v); return *this; }
    RoutingRequestValues& D(const ByteBuffer* v) { this->mValues += " d" + (v != NULL ? v->mBytes : std::string("null")); return *this; }
    void Run(RefCount* c, int k, Completion* d)
    {
        Log("request " + Id(c) + " " + LoWorld::N(k) + mValues + " " + (d != NULL ? d->mName : std::string("none")));
        delete d;
    }
};
struct RoutingConnection : RefCount {
    void SetAuth(AuthContext* a, AuthType t) { Log(Id(this) + " SetAuth " + Id(a) + " " + N(t)); }
    void SetOpCompletion(int t, Completion* c) { Log(Id(this) + " SetOpCompletion " + N(t) + " " + c->mName); delete c; }
    void SetCloseCompletion(Completion* c) { Log(Id(this) + " SetCloseCompletion " + c->mName); delete c; }
    bool IsConnected() { Log(Id(this) + " IsConnected"); return sConnected; }
};
struct RoutingOp : AsyncOp {
    RoutingOp() {}
    RoutingOp(RoutingConnection* c, RoutingOpType t) { Log("new RoutingOp " + Id(this) + " " + Id(c) + " " + N(t)); }
    long long Num(int w) { Log(Id(this) + " Num " + N(w)); return 5000 + w; }
    std::wstring Text(int w) { Log(Id(this) + " Text " + N(w)); return L"text" + std::to_wstring(w); }
};
struct ServerConnectionConnectOp : AsyncOp {
    ServerConnectionConnectOp(RoutingConnection* c, const IPAddr& a) { Log("new ServerConnectionConnectOp " + Id(this) + " " + Id(c) + " " + N(a.mValue)); }
};
// RoutingGetUserStatusOp: the friend status map a real Rust object, GetFriendStatusMap / SetFriendStatusMap as they
// were in WONRouting.h (the deleted copies)
struct RoutingGetUserStatusOp : AsyncOp {
    typedef std::map<unsigned long, unsigned char> FriendStatusMap;
    EeWonUserStatus* mStatus;
    RoutingGetUserStatusOp(const IPAddr& a, UDPManager* m) : mStatus(ee_won_userstatus_new())
    {
        Log("new RoutingGetUserStatusOp " + Id(this) + " " + N(a.mValue) + " " + Id(m));
    }
    ~RoutingGetUserStatusOp() { ee_won_userstatus_free(this->mStatus); }
    EeWonUserStatus* GetRust() const { return this->mStatus; }
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
    static void StatusSink(void* inContext, uint64_t inId, uint8_t inStatus)
    { (*static_cast<FriendStatusMap*>(inContext))[(unsigned long)inId] = inStatus; }
    std::string Dump() const
    {
        std::string s;
        ee_won_userstatus_each(this->mStatus, [](void* c, uint64_t i, uint8_t v) { *static_cast<std::string*>(c) += N((long long)i) + "=" + N(v) + ","; }, &s);
        return s;
    }
};
struct LobbyClientIterator {
    int mId;
    ~LobbyClientIterator() { Log("delete LobbyClientIterator " + N(this->mId)); }
};
static long long WorldHostopNum(RustServerOp* op, int w, long long i) { Log(Id(op) + " hostop num " + N(w) + " " + N(i)); return 4242 + op->mId; }
#define ee_won_hostop_num(op, w, i) NlWorld::WorldHostopNum(op, w, i)
struct RoutingLogic {
    LobbyServerList* mServerList;
    std::string mGameDataType = "G";
    void ConnectionClosed(AsyncOp*) {}
    void QueryServerGroupsDone(AsyncOp*) {}
    void GetUserStatusComplete(AsyncOp*) {}
    void RunTrackedOp(AsyncOp* o, Completion* c, int t) { Log("RunTrackedOp " + Id(o) + " " + c->mName + " " + N(t)); delete c; }
};
} // namespace NlWorld
// RoutingLogic.cpp's notice completion (global, as the original's RTTI names it)
struct RoutingOpCompletion : LoWorld::Completion {
    RoutingOpCompletion(int inCode, NlWorld::RoutingLogic*) { this->mName = "notice " + LoWorld::N(inCode); }
};
namespace LoWorld {
template <> inline LogicCompletion<NlWorld::RoutingLogic>::LogicCompletion(void (NlWorld::RoutingLogic::*inFn)(AsyncOp*), NlWorld::RoutingLogic*)
{
    typedef NlWorld::RoutingLogic L;
    this->mName = inFn == &L::ConnectionClosed ? "ConnectionClosed" : inFn == &L::QueryServerGroupsDone ? "QueryServerGroupsDone"
                : inFn == &L::GetUserStatusComplete ? "GetUserStatusComplete" : "?";
}
} // namespace LoWorld
#endif
