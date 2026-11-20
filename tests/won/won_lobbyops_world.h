// won_lobbyops_test's stand-in world (R6 WONLobby round 29): the classes and lobby functions StagingLogic's and
// LanLogic's moved host cases name (LobbyLogicHost.cpp's, LanLogicHost.cpp's), each call logged in one trace. The
// deleted statements (won_lobbyops_orig.h) and the host's new one-statement cases (the test's World) run over it.
// Every object made is numbered in order; the references are compared at the end of each case, not traced.
#ifndef WON_LOBBYOPS_WORLD_H
#define WON_LOBBYOPS_WORLD_H
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace LoWorld {
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }

struct RefCount;
static std::vector<RefCount*> sObjects;
struct RefCount {
    int mRefs = 0;
    int mId;
    RefCount() : mId((int)sObjects.size()) { sObjects.push_back(this); }
    virtual ~RefCount() {}
    RefCount* CreateRef() { this->mRefs++; return this; }
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
};
static std::string Id(const void* p) { return p == NULL ? "null" : "#" + N(static_cast<const RefCount*>(p)->mId); }
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p ? (T*)p->CreateRef() : NULL) {}
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr ? (T*)o.mPtr->CreateRef() : NULL) {}
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (this->mPtr != p) {
            if (this->mPtr != NULL) this->mPtr->Release();
            this->mPtr = p ? (T*)p->CreateRef() : NULL;
        }
        return p;
    }
    SmartPtr& operator=(const SmartPtr& o) { this->operator=(o.mPtr); return *this; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    T* mPtr;
};

struct GUIString {
    std::string s;
    GUIString() {}
    GUIString(const char* t) : s(t) {}
    int length() const { return (int)this->s.size(); }
};
typedef long WONStatus;
typedef unsigned long DWORD;
static wchar_t gEmptyWideString[1];

struct IPAddr {
    intptr_t mValue = 0;
    intptr_t GetValue() const { return this->mValue; }
    static IPAddr GetBroadcastAddr(int inPort) { Log("GetBroadcastAddr " + N(inPort)); IPAddr a; a.mValue = 0xb000 + inPort; return a; }
};
static void IpAssign(::EeWonIpAddr* a, intptr_t v) { Log("ipaddr assign " + N((intptr_t)a) + " " + N(v)); }
static IPAddr Addr(void* v) { IPAddr a; a.mValue = (intptr_t)v; return a; }

struct ByteBuffer : RefCount {
    std::string mBytes;
    const void* data() const { return this->mBytes.data(); }
    unsigned long length() const { return (unsigned long)this->mBytes.size(); }
};
static void Sink(void* inSink, const uint8_t* inBytes, size_t inLength)
{
    Log("sink " + N((intptr_t)inSink) + " " + std::string((const char*)inBytes, inLength));
}

// the ops: their status (a world's answer) and their calls
static long sNextStatus;
struct Completion { std::string mName; };
struct AsyncOp : RefCount {
    long mStatus;
    AsyncOp() : mStatus(sNextStatus) {}
    void Finish(long s) { Log(Id(this) + " Finish " + N(s)); }
    void Kill(bool f) { Log(Id(this) + " Kill " + N(f)); }
    long GetStatus() { Log(Id(this) + " GetStatus"); return this->mStatus; }
    bool Succeeded() { Log(Id(this) + " Succeeded"); return this->mStatus == 0; }
};
struct AsyncSocket;
static SmartPtr<ByteBuffer> sReplyMessage;
struct SocketOp : AsyncOp {
    SocketOp(EeWonCoreSocketOp k, AsyncSocket* s = NULL, const ByteBuffer* b = NULL, const IPAddr* a = NULL, unsigned long n = 0)
        : mSocket(s)
    {
        Log("new SocketOp " + Id(this) + " kind " + N((long long)k) + " socket " + Id((RefCount*)s) + " bytes " +
            Id(b) + " addr " + (a != NULL ? N(a->mValue) : std::string("none")) + " size " + N(n));
    }
    AsyncSocket* GetSocket() { return this->mSocket; }
    SmartPtr<ByteBuffer> GetMsg() { Log(Id(this) + " GetMsg"); return sReplyMessage; }
    AsyncSocket* mSocket;
};
struct AuthPeerData { DWORD GetFirstCommunity() { Log("GetFirstCommunity"); return 0x1234; } };
struct AuthContext : RefCount {
    bool mExpired = false;
    AuthPeerData mPeer;
    void SetDoAutoRefresh(bool b) { Log("SetDoAutoRefresh " + N(b)); }
    bool IsExpired(time_t t) { Log("IsExpired " + N((long long)t)); return this->mExpired; }
    void SetUserName(const std::wstring& w) { Log("SetUserName " + N((long long)w.size())); }
    void SetPassword(const std::wstring& w) { Log("SetPassword " + N((long long)w.size())); }
    AuthPeerData* GetPeerData() { return &this->mPeer; }
};
struct GetCertOp : AsyncOp {
    GetCertOp(AuthContext* a) { Log("new GetCertOp " + Id(this) + " " + Id(a)); }
};
struct LobbyLanSocket;
static bool sPermits;
struct PeerAuthServerOp : AsyncOp {
    PeerAuthServerOp(AuthContext* a, LobbyLanSocket* s) : mSocket((AsyncSocket*)s)
    {
        Log("new PeerAuthServerOp " + Id(this) + " " + Id(a) + " " + Id((RefCount*)s));
    }
    AsyncSocket* GetSocket() { Log(Id(this) + " GetSocket"); return this->mSocket; }
    bool ClientPermits(DWORD c) { Log(Id(this) + " ClientPermits " + N(c)); return sPermits; }
    AsyncSocket* mSocket;
};
enum AuthType { AUTH_TYPE_NONE = 0, AUTH_TYPE_SESSION = 1, AUTH_TYPE_PERSISTENT = 2 };
struct PeerAuthOp : AsyncOp {
    PeerAuthOp(const IPAddr& a, AuthContext* c, AuthType t, LobbyLanSocket* s, bool f)
    {
        Log("new PeerAuthOp " + Id(this) + " addr " + N(a.mValue) + " " + Id(c) + " " + N(t) + " " + Id((RefCount*)s) + " " + N(f));
    }
};
static long sBindStatus;
struct AsyncSocket : RefCount {
    long Fn2B0D90(int p, int f) { Log(Id(this) + " bind " + N(p) + " " + N(f)); return sBindStatus; }
    long Listen(int n) { Log(Id(this) + " Listen " + N(n)); return sNextStatus; }
    void SetRepeatCompletion(Completion* c) { Log(Id(this) + " SetRepeatCompletion " + c->mName); delete c; }
    void SetCloseCompletion(Completion* c) { Log(Id(this) + " SetCloseCompletion " + c->mName); delete c; }
    void SetRepeatOp(SocketOp* o) { Log(Id(this) + " SetRepeatOp " + Id(o)); }
    void QueueClose(long t) { Log(Id(this) + " QueueClose " + N(t)); }
};
struct LobbyLanSocket : AsyncSocket {};
struct QueueSocket : AsyncSocket {
    QueueSocket(int k) { Log("new QueueSocket " + Id(this) + " " + N(k)); }
};

// the lobby's objects
struct LobbyGame : RefCount {
    void ClearPlayers() { Log(Id(this) + " ClearPlayers"); }
    void PreNetLeaveGame() { Log(Id(this) + " PreNetLeaveGame"); }
    void PostNetLeaveGame() { Log(Id(this) + " PostNetLeaveGame"); }
    void SetInProgress(bool b) { Log(Id(this) + " SetInProgress " + N(b)); }
    void SetShowDetails(bool b) { Log(Id(this) + " SetShowDetails " + N(b)); }
    IPAddr GetAddress() const { Log(Id(this) + " GetAddress"); IPAddr a; a.mValue = 0xa000 + this->mId; return a; }
    void SetName(const GUIString& n) { Log(Id(this) + " SetName " + n.s); }
};
struct LobbyClient : RefCount {
    GUIString GetName() { return GUIString(("client" + N(this->mId)).c_str()); }
};
struct ChatCommandParser : RefCount {
    void SetInput(const GUIString& s) { Log("parser SetInput " + s.s); }
};
struct GameChangedEvent {
    LobbyGame* mGame;
    int mHow;
    GameChangedEvent(LobbyGame* g, int h) : mGame(g), mHow(h) {}
};
static void LobbyBroadcastEvent(const GameChangedEvent& e, bool f) { Log("broadcast GameChanged " + Id(e.mGame) + " " + N(e.mHow) + " " + N(f)); }
struct Dialog : RefCount {
    GUIString mInputs[2];
    void LCall(EeWonLobbyDlgCall c, intptr_t a = 0) { Log("dialog " + N(this->mId) + " LCall " + N((long long)c) + " " + N(a)); }
    // (a getter: not traced, the Rust reads the direct dialog's name once per use where the original kept it)
    const GUIString& InputText(intptr_t i) { return this->mInputs[i]; }
};
struct Sound : RefCount {
    std::string mName;
    void Stop() { Log("sound Stop " + mName); }
};
struct LobbyResource {
    GUIString mString;
    SmartPtr<Sound> mSound;
};
static LobbyResource gLobbyResources[1024];
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)
#define LOBBY_SOUND(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mSound)
struct LobbyConfig {
    int mAllowJoinGameWithoutPing = 0;
};

struct StagingLogic {
    SmartPtr<AsyncOp> mJoinOp;
    SmartPtr<LobbyClient> mMyClient;
    SmartPtr<Dialog> mCreateGameDialog;
};
struct LanLogic;
template <class T> struct LogicCompletion : Completion {
    LogicCompletion(void (T::*inFn)(AsyncOp*), T* inLogic);
};
struct LanLogic {
    SmartPtr<Dialog> mLanNameDialog, mDirectConnectDialog;
    void HandleAccept(AsyncOp*) {}
    void HandleDiscoveryRecv(AsyncOp*) {}
    void HandleSocketClosed(AsyncOp*) {}
    void HandleRecvMsg(AsyncOp*) {}
    void HandlePeerAuthDone(AsyncOp* inOp);
    void RunTrackedOp(AsyncOp* o, Completion* c, int t) { Log("RunTrackedOp " + Id(o) + " " + c->mName + " " + N(t)); delete c; }
    void Call(EeWonLanCall c, void* a) { Log("Call " + N((long long)c) + " " + Id(a)); }
};
template <class T> LogicCompletion<T>::LogicCompletion(void (T::*inFn)(AsyncOp*), T*)
{
    this->mName = inFn == &LanLogic::HandleAccept ? "HandleAccept" : inFn == &LanLogic::HandleDiscoveryRecv ? "HandleDiscoveryRecv"
                : inFn == &LanLogic::HandleSocketClosed ? "HandleSocketClosed" : inFn == &LanLogic::HandleRecvMsg ? "HandleRecvMsg"
                : inFn == &LanLogic::HandlePeerAuthDone ? "HandlePeerAuthDone" : "?";
}

// the lobby's functions
static LobbyConfig* sConfig;
static AuthContext* sAuth;
static ChatCommandParser* sParser;
static bool sChatSounds, sCdKeyValid;
static void LobbyRunOpWithStatus(AsyncOp* o, const GUIString& t, const GUIString& d, int a, int b)
{
    Log("RunOpWithStatus " + Id(o) + " " + t.s + " " + d.s + " " + N(a) + " " + N(b));
}
static void LobbyNetLogout() { Log("NetLogout"); }
static void LobbyKillPings(int n) { Log("KillPings " + N(n)); }
static void LobbyShowLoginScreen() { Log("ShowLoginScreen"); }
static void LobbySaveGlobalPrefs(bool b) { Log("SaveGlobalPrefs " + N(b)); }
static void LobbySaveUserPrefs(bool b) { Log("SaveUserPrefs " + N(b)); }
static void LobbyNetLeaveGame(LobbyGame* g) { Log("NetLeaveGame " + Id(g)); }
static void LobbyNetSetGameInProgress(bool b) { Log("NetSetGameInProgress " + N(b)); }
static void LobbyNetAdvertiseGame() { Log("NetAdvertiseGame"); }
static void LobbyPingGame(LobbyGame* g, bool b) { Log("PingGame " + Id(g) + " " + N(b)); }
static void LobbyQueryGameDetails(LobbyGame* g) { Log("QueryGameDetails " + Id(g)); }
static LobbyConfig* GetLobbyConfig() { Log("GetLobbyConfig"); return sConfig; }
static AuthContext* GetLobbyAuthContext() { Log("GetLobbyAuthContext"); return sAuth; }
static bool LobbyGetChatSoundEffects() { Log("GetChatSoundEffects"); return sChatSounds; }
static void LobbyPlaySound(Sound* s) { Log("PlaySound " + (s != NULL ? s->mName : std::string("none"))); }
static ChatCommandParser* LobbyGetChatCommandParser() { Log("GetChatCommandParser"); return sParser; }
static void LobbySetLanName(const GUIString& s) { Log("SetLanName " + s.s); }
static void LobbyBroadcastLobbyEvent(int e, bool f) { Log("BroadcastLobbyEvent " + N(e) + " " + N(f)); }
static void LobbySetLanBroadcastPort(int p) { Log("SetLanBroadcastPort " + N(p)); }
static bool LobbyIsCDKeyValid() { Log("IsCDKeyValid"); return sCdKeyValid; }
static void LobbyMessageBox(const GUIString& t, const GUIString& d) { Log("MessageBox " + t.s + " " + d.s); }
static void LobbyDoErrorDialog(const GUIString& t, WONStatus s) { Log("ErrorDialog " + t.s + " " + N(s)); }
static int LobbyGetLobbyPort() { Log("GetLobbyPort"); return 3456; }
static int LobbyGetLanBroadcastPort() { Log("GetLanBroadcastPort"); return 4567; }
static void LobbyAddDirectConnect(const IPAddr& a, const GUIString& n) { Log("AddDirectConnect " + N(a.mValue) + " " + n.s); }
} // namespace LoWorld
#endif
