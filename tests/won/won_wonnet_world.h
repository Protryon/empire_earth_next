// won_wonnet_test's stand-in world (R6 WON round 31): every object, global and helper the game's WON front end
// names (EEWonNet.cpp before the round, won_wonnet_orig.h; and the host's statements, ee/EEWonNetHost.inl), each
// effect logged, each answer a knob of the world. Both sides run over it: the deleted C++ (namespace W::Ori) and the
// Rust over the host's cases (namespace W::Rus). Getters answer from the knobs and are not logged (the two sides may
// read them in a different order); every call that changes something outside the object is.
#ifndef WON_WONNET_WORLD_H
#define WON_WONNET_WORLD_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <functional>
#include "rust/won_net/reqdir.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/smartroute.h"

namespace W {
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
static std::string WS(const std::wstring& s)
{
    std::string o;
    for (wchar_t c : s) o += c < 128 ? std::string(1, (char)c) : "<" + N((long long)c) + ">";
    return o;
}

// ---- the world's knobs (the test sets them before each step)
struct GroupKnob { std::wstring mName; unsigned short mId; unsigned long mFlags; unsigned short mMembers; std::vector<unsigned short> mMemberIds; };
struct ClientKnob { unsigned short mId; std::wstring mName; };
struct ObjectKnob { unsigned short mLink; std::string mType; std::string mData; };
struct Knobs {
    std::vector<GroupKnob> mGroups;
    std::vector<ClientKnob> mClients;
    unsigned short mMyClientId = 1;
    long mCurrentForm = 0;
    bool mChecks[3] = {false, false, false};
    bool mEmailChecked = false;
    long mRoomsSelection = -1;      // < 0: no selection
    unsigned long mRoomsLines = 0;
    bool mWonNetExists = true;
    bool mKeyValid = true;
    std::vector<std::string> mFileLines;
    bool mFileOpens = true;
    std::string mLocalAddress = "1.2.3.4";
    std::wstring mUserName = L"me";
};
static Knobs sK;

// the op a completion finishes with (the test fills it)
struct OpKnob {
    long mStatus = 0;
    int mType = 0;
    unsigned short mGroup = 0, mLink = 0, mClient = 0;
    long long mFlags16 = 0, mMaxPlayers = 0, mReconnect = 0;
    std::wstring mText;
    bool mRecipients = true;
    bool mHasData = true;
    std::string mData;
    std::vector<ObjectKnob> mObjects;
    std::vector<unsigned short> mMembers;
    std::vector<ClientKnob> mClients;
    bool mOnRouting = true;          // the op's connection is the front end's routing connection
    bool mSocketIsRouting = true;
    int mWhichOp = 0;                // 1: the login op, 2: the directory op
    long mNewsletter = 0;
    std::string mEmail;
    std::vector<std::wstring> mEntities;
};

typedef long WONStatus;
typedef int RoutingOpType;
// the room list's value (EEWonNet.h's before the round)
struct EEWonNetRoomInfo {
    UWideString mName;
    UWideString mDescription;
    unsigned long mMaxPlayers;
    unsigned long mId;
};

void ESVerify(const char*, long line, const char* expr) { Log("verify " + N(line) + " " + expr); }
inline unsigned long TMGetTime() { return 0; }
inline const char* WONStatusToString(long) { return ""; }
enum { keseOK = 0 };
struct TSCriticalSection {
    int mId;
    void Enter() { Log("lock" + N(mId) + " enter"); }
    void Leave() { Log("lock" + N(mId) + " leave"); }
};
static int sLockIds;
struct TSScheduler {
    static long CreateCriticalSection(TSCriticalSection*& out) { out = new TSCriticalSection{++sLockIds}; Log("lock" + N(out->mId) + " create"); return 0; }
    static void DestroyCriticalSection(TSCriticalSection*& c) { Log("lock" + N(c->mId) + " destroy"); delete c; c = NULL; }
};

struct RefCount {
    int mRefs = 0;
    virtual ~RefCount() {}
    RefCount* CreateRef() { this->mRefs++; return this; }
    void AddRef() { this->mRefs++; }
    void Release() { if (--this->mRefs <= 0) delete this; }
};
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (p) p->AddRef();
        if (this->mPtr) this->mPtr->Release();
        this->mPtr = p;
        return p;
    }
    SmartPtr& operator=(const SmartPtr& o) { this->operator=(o.mPtr); return *this; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    operator T*() const { return this->mPtr; }
    T* mPtr;
};

struct PlatMutexW { int mId; };
inline void PlatMutexLock(PlatMutexW* m) { Log("data lock"); }
inline void PlatMutexUnlock(PlatMutexW* m) { Log("data unlock"); }
struct AutoCrit {
    PlatMutexW* m;
    explicit AutoCrit(PlatMutexW* c) : m(c) { PlatMutexLock(m); }
    ~AutoCrit() { PlatMutexUnlock(m); }
};

class AsyncOp;
typedef SmartPtr<AsyncOp> AsyncOpPtr;
template <class T> class CompletionBase : public RefCount {
public:
    virtual void Complete(T) = 0;
};
typedef CompletionBase<AsyncOpPtr> RouteCompletion;
// a completion's name for the trace (the test resolves both sides' classes)
static std::function<std::string(CompletionBase<AsyncOpPtr>*)> sCompletionName;
static std::vector<SmartPtr<CompletionBase<AsyncOpPtr> > > sPending;   // the completions handed out, in order
static std::string Keep(CompletionBase<AsyncOpPtr>* c)
{
    if (c == NULL) return "none";
    sPending.push_back(c);
    return sCompletionName(c);
}

struct SmartModel { int mItr = -1; int mKind = -1; size_t mGroupAt = 0; size_t mMemberAt = 0; unsigned short mGroupId = 0; };
class SmartRoutingConnection;
class AsyncOp : public RefCount {
public:
    OpKnob mK;
    long GetStatus() const { return mK.mStatus; }
    void Kill(bool) { Log("op kill"); }
    void SetCompletion(CompletionBase<AsyncOpPtr>* c) { Log("op completion " + Keep(c)); }
    void RunAsync(unsigned long t) { Log("op run " + N(t)); }
};
struct ByteBuffer : RefCount {
    std::string mBytes;
    ByteBuffer(const void* d, unsigned long n, bool) : mBytes((const char*)d, n) {}
    const char* data() const { return mBytes.data(); }
    unsigned long length() const { return (unsigned long)mBytes.size(); }
};
typedef SmartPtr<ByteBuffer> ByteBufferPtr;
inline const char* GetBufferData(const ByteBufferPtr& b) { return b.get() ? b->data() : NULL; }
struct EeWonRouteOpW {};
class RoutingOp : public AsyncOp {
public:
    EeWonRouteOpW* mRust = NULL;
    struct Conn { const OpKnob* k; RefCount* get() const; } mConnection = {&mK};
    int GetType() const { return mK.mType; }
    long long Num(int f) const
    {
        switch (f) {
        case EE_WON_RTF_GROUP_ID: return mK.mGroup;
        case EE_WON_RTF_CLIENT_ID: return mK.mClient;
        case EE_WON_RTF_LINK_ID: return mK.mLink;
        case EE_WON_RTF_FLAGS16: return mK.mFlags16;
        case EE_WON_RTF_MAX_PLAYERS: return mK.mMaxPlayers;
        case EE_WON_RTF_RECONNECT_ID: return mK.mReconnect;
        }
        return 0;
    }
    std::wstring Text(int) const { return mK.mText; }
    std::list<unsigned short> Recipients() const { return mK.mRecipients ? std::list<unsigned short>(1, 3) : std::list<unsigned short>(); }
    ByteBufferPtr GetData() const { return mK.mHasData ? ByteBufferPtr(new ByteBuffer(mK.mData.data(), (unsigned long)mK.mData.size(), false)) : ByteBufferPtr(); }
    unsigned short GetGroupId() const { return mK.mGroup; }
    unsigned short GetClientId() const { return mK.mClient; }
    unsigned short GetLinkId() const { return mK.mLink; }
    RefCount* GetSocket() const;
};
typedef RoutingOp SocketOp;
typedef RoutingOp PeerAuthOp;
// the routing op records (routeop.rs's ee_won_routeop_records over the knobs)
static RoutingOp* sRecordOp;
inline void ee_won_routeop_records(EeWonRouteOpW*, int kind, void (*cb)(void*, const EeWonRouteRecord*), void* ctx)
{
    const OpKnob& k = sRecordOp->mK;
    if (kind == EE_WON_RTR_OBJECT) {
        for (const ObjectKnob& o : k.mObjects) {
            EeWonRouteRecord r = {};
            r.kind = EE_WON_RTR_OBJECT;
            r.id = o.mLink;
            r.data_type = (const uint8_t*)o.mType.data();
            r.data_type_len = o.mType.size();
            r.has_data = true;
            r.data = (const uint8_t*)o.mData.data();
            r.data_len = o.mData.size();
            cb(ctx, &r);
        }
    } else if (kind == EE_WON_RTR_MEMBER) {
        for (unsigned short m : k.mMembers) {
            EeWonRouteRecord r = {};
            r.kind = EE_WON_RTR_MEMBER;
            r.id = m;
            cb(ctx, &r);
        }
    } else if (kind == EE_WON_RTR_CLIENT) {
        for (const ClientKnob& c : k.mClients) {
            std::vector<uint16_t> u(c.mName.begin(), c.mName.end());
            EeWonRouteRecord r = {};
            r.kind = EE_WON_RTR_CLIENT;
            r.key = c.mId;
            r.id = c.mId;
            r.name = u.data();
            r.name_len = u.size();
            cb(ctx, &r);
        }
    }
}
inline void WonWideSink(void* ctx, const uint16_t* u, size_t n) { static_cast<std::wstring*>(ctx)->assign(u, u + n); }

struct AuthContext { std::wstring GetName() const { return sK.mUserName; } };
enum { AUTH_TYPE_PERSISTENT = 3 };
struct IPAddr {
    std::string mText;
    IPAddr() {}
    explicit IPAddr(const std::string& s) : mText(s) {}
};
class SmartRoutingConnection : public RefCount {
public:
    SmartModel* mModel = new SmartModel;
    PlatMutexW mLock = {0};
    ~SmartRoutingConnection() { Log("routing deleted"); delete mModel; }
    PlatMutexW* GetDataLock() { return &mLock; }
    void SetAuth(AuthContext*, int t) { Log("routing auth " + N(t)); }
    void SetConnectCompletion(CompletionBase<AsyncOpPtr>* c) { Log("routing connect completion " + Keep(c)); }
    void SetCloseCompletion(CompletionBase<AsyncOpPtr>* c) { Log("routing close completion " + Keep(c)); }
    void SetOpCompletion(int t, CompletionBase<AsyncOpPtr>* c) { Log("routing op completion " + N(t) + " " + Keep(c)); }
    void SetDefaultOpCompletion(CompletionBase<AsyncOpPtr>* c) { Log("routing default completion " + Keep(c)); }
    void ClearProductName() { Log("routing clear product"); }
    void ConnectAsync(const IPAddr& a, unsigned long t) { Log("routing connect " + a.mText + " " + N(t)); }
    unsigned short GetMyClientId() const { return sK.mMyClientId; }
    void IgnoreClient(unsigned short id, bool on) { Log("routing ignore " + N(id) + " " + N(on)); }
    void Kill() { Log("routing kill"); }
    void StartGroupItr() { AutoCrit c(&mLock); Log("routing group itr"); mModel->mKind = 1; mModel->mGroupAt = 0; }
    bool StartMemberItr(unsigned short id)
    {
        AutoCrit c(&mLock);
        Log("routing member itr " + N(id));
        for (size_t i = 0; i < sK.mGroups.size(); i++)
            if (sK.mGroups[i].mId == id) { mModel->mKind = 2; mModel->mGroupAt = i; mModel->mMemberAt = 0; return true; }
        return false;
    }
};
// the routing connection of the side running (the test sets it), and a socket that is not it
static std::function<RefCount*()> sRoutingNow;
static RefCount sOtherSocket;
RefCount* RoutingOp::GetSocket() const { return mK.mSocketIsRouting ? sRoutingNow() : &sOtherSocket; }
RefCount* RoutingOp::Conn::get() const { return k->mOnRouting ? sRoutingNow() : &sOtherSocket; }
// the model's reads (smartroute.rs's ee_won_smart_get / ee_won_smart_find over the knobs)
static std::vector<uint32_t> sNameUnits;
static void SetName(EeWonSmartRecord* r, const std::wstring& n)
{
    sNameUnits.assign(n.begin(), n.end());
    r->name = sNameUnits.data();
    r->name_len = sNameUnits.size();
}
inline bool ee_won_smart_get(SmartModel* m, EeWonSmartGet which, unsigned short id, int, EeWonSmartRecord* out)
{
    Log("smart get " + N((int)which) + " " + N(id));
    *out = EeWonSmartRecord();
    if (which == EeWonSmartGet::NextGroup) {
        if (m->mKind != 1 || m->mGroupAt >= sK.mGroups.size()) return false;
        const GroupKnob& g = sK.mGroups[m->mGroupAt++];
        out->id = g.mId; out->flags = (uint32_t)g.mFlags; out->member_count = g.mMembers;
        SetName(out, g.mName);
        return true;
    }
    if (which == EeWonSmartGet::NextMember) {
        if (m->mKind != 2) return false;
        const GroupKnob& g = sK.mGroups[m->mGroupAt];
        if (m->mMemberAt >= g.mMemberIds.size()) return false;
        unsigned short mid = g.mMemberIds[m->mMemberAt++];
        out->id = mid;
        for (const ClientKnob& c : sK.mClients)
            if (c.mId == mid) { out->has_client = true; SetName(out, c.mName); }
        return true;
    }
    if (which == EeWonSmartGet::Client) {
        for (const ClientKnob& c : sK.mClients)
            if (c.mId == id) { out->id = c.mId; out->has_client = true; SetName(out, c.mName); return true; }
        return false;
    }
    return false;
}
inline void ee_won_smart_find(SmartModel*, const wchar_t* name, size_t n, EeWonSmartRecord* out, bool* found)
{
    std::wstring s(name, n);
    Log("smart find " + WS(s));
    for (const ClientKnob& c : sK.mClients)
        if (c.mName == s) { out->id = c.mId; out->has_client = true; SetName(out, c.mName); *found = true; return; }
}

// the request helpers (EEWonNet.cpp's WonRequests), each logged with its arguments
static std::string L(const std::list<unsigned short>& l) { std::string s; for (unsigned short v : l) s += N(v) + ","; return s; }
static std::string H(const std::string& d) { std::string s; char b[4]; for (unsigned char c : d) { snprintf(b, 4, "%02x", c); s += b; } return s; }
static unsigned short sLastJoin = 10;   // the last group joined (the test's draws reuse it)
class WonRequests {
public:
    explicit WonRequests(SmartRoutingConnection* c) { if (c == NULL) Log("req on NULL"); }
    void SubscribeDataObject(unsigned short l, const std::string& t, unsigned long f, RouteCompletion* c) { Log("req subscribe " + N(l) + " " + t + " " + N(f) + " " + Keep(c)); }
    void UnsubscribeDataObject(unsigned short l, const std::string& t, unsigned long f, RouteCompletion* c) { Log("req unsubscribe " + N(l) + " " + t + " " + N(f) + " " + Keep(c)); }
    void Register(const std::wstring& n, const std::wstring& p, unsigned long r, unsigned long a, unsigned long rf, unsigned long f, RouteCompletion* c)
    { Log("req register " + WS(n) + " " + WS(p) + " " + N(r) + " " + N(a) + " " + N(rf) + " " + N(f) + " " + Keep(c)); }
    void CreateGroup(unsigned short g, const std::wstring& n, const std::wstring& p, unsigned short m, unsigned long gf, unsigned char jf, unsigned long af, RouteCompletion* c)
    { Log("req create group " + N(g) + " " + WS(n) + " " + WS(p) + " " + N(m) + " " + N(gf) + " " + N(jf) + " " + N(af) + " " + Keep(c)); }
    void CreateDataObject(unsigned char f, unsigned short l, const std::string& t, const std::wstring& n, const ByteBuffer* d, RouteCompletion* c)
    { Log("req create data " + N(f) + " " + N(l) + " " + t + " " + WS(n) + " " + H(d->mBytes) + " " + Keep(c)); delete d; }
    void ReplaceDataObject(unsigned short l, const std::string& t, const std::wstring& n, RouteCompletion* c) { Log("req replace data " + N(l) + " " + t + " " + WS(n) + " " + Keep(c)); }
    void GetGroupList(unsigned char f, RouteCompletion* c) { Log("req group list " + N(f) + " " + Keep(c)); }
    void SendChat(const std::wstring& t, unsigned long f, unsigned short r, RouteCompletion* c) { this->SendChatTo(t, (unsigned short)f, std::list<unsigned short>(1, r), c); }
    void SendChatTo(const std::wstring& t, unsigned short f, const std::list<unsigned short>& l, RouteCompletion* c) { Log("req chat " + WS(t) + " " + N(f) + " " + L(l) + " " + Keep(c)); }
    void ModifyDataObject(unsigned short l, const std::string& t, const std::wstring& n, unsigned short o, bool i, const ByteBuffer* d, RouteCompletion* c)
    { Log("req modify data " + N(l) + " " + t + " " + WS(n) + " " + N(o) + " " + N(i) + " " + H(d->mBytes) + " " + Keep(c)); delete d; }
    void JoinGroup(unsigned short i, const std::wstring& p, const std::wstring& m, unsigned char jf, unsigned char mf, RouteCompletion* c)
    { sLastJoin = i; Log("req join " + N(i) + " " + WS(p) + " " + WS(m) + " " + N(jf) + " " + N(mf) + " " + Keep(c)); }
    void CancelJoinGroup(unsigned short i, bool l, RouteCompletion* c) { Log("req cancel join " + N(i) + " " + N(l) + " " + Keep(c)); }
    void LeaveGroup(unsigned short i, RouteCompletion* c) { Log("req leave " + N(i) + " " + Keep(c)); }
    void ReadDataObject(unsigned short l, const std::string& t, unsigned long f, RouteCompletion* c) { Log("req read data " + N(l) + " " + t + " " + N(f) + " " + Keep(c)); }
    void GetClientList(RouteCompletion* c) { Log("req client list " + Keep(c)); }
    void GetMembersOfGroup(unsigned short i, RouteCompletion* c) { Log("req members " + N(i) + " " + Keep(c)); }
};

static char kWonGameKey[] = "GAME";
static char kWonGameObject[] = "EE Game Object";
static char kWonRoomObject[] = "EE Room Object";
static char kWonProductName[] = "EmpireEarth";
static wchar_t kWonRoutingServerType[] = L"TitanRoutingServerG2";
static wchar_t kWonProductDirectory[] = L"/EmpireEarth";
static unsigned long gWonClientId;
static wchar_t gEmptyWideString[] = L"";
static unsigned long gNetworkAdapter = 2;
static UWideString gHostedGameName;

// the UI
enum { kFormMainMenu = 12, kFormLobby = 26, kFormMultiplayer = 30, kFormAvailableRooms = 31, kFormAvailableGames = 32,
       kFormUpdateSierraAccount = 37, kFormSelectServer = 38 };
enum { kAvailableGamesInProgressToggle = 32, kAvailableGamesPasswordToggle = 33, kAvailableGamesPrivateToggle = 34,
       kUpdateAccountEnterEmailEditBox = 6, kUpdateAccountSendMeEmailCheckbox = 7 };
struct UIEvent { int mId; void Dereference() { Log("event " + N(mId) + " deref"); } };
struct UIControl { long mForm, mId; };
struct UICheckBox : UIControl {
    bool GetIsChecked() const { return mForm == kFormUpdateSierraAccount ? sK.mEmailChecked : sK.mChecks[(mId - 32) % 3]; }
    void SetChecked(bool b) { Log("checkbox " + N(mForm) + "/" + N(mId) + " " + N(b)); }
};
struct UIEditBox : UIControl { void SetText(const UWideString& t) { Log("edit " + N(mForm) + "/" + N(mId) + " " + (const char*)t); } };
struct UITextLabel : UIControl { void SetText(const UWideString& t) { Log("label " + N(mForm) + "/" + N(mId) + " " + (const char*)t); } };
struct UIForm {
    long mId;
    UICheckBox mControls[40];
    UIForm(long id) : mId(id) { for (int i = 0; i < 40; i++) { mControls[i].mForm = id; mControls[i].mId = i; } }
    UIControl* GetControl(long i) { return &mControls[i]; }
    void SetFormOpenEvent(UIEvent* e) { Log("form " + N(mId) + " open event " + N(e->mId)); }
};
static UIForm* sForms[64];
struct UIManagerW {
    UIForm* GetFormByID(long id) { if (!sForms[id]) sForms[id] = new UIForm(id); return sForms[id]; }
    UIForm* GetCurrentForm() { return GetFormByID(sK.mCurrentForm); }
    void LockUIForCopyData() { Log("ui lock"); }
    void UnlockUIForCopyData() { Log("ui unlock"); }
    void PopTo(UIForm* f) { Log("ui pop to " + N(f->mId)); }
    void Push(UIForm* f) { Log("ui push " + N(f->mId)); }
};
static UIManagerW sUIManager;
static UIManagerW* gUIManager = &sUIManager;
struct EEAction {
    std::string mText;
    void Dereference() { Log("action deref " + mText); }
};
struct UserInterfaceW {
    void CloseForm(UIForm* f) { Log("ui close " + (f ? N(f->mId) : std::string("NULL"))); }
    void SwitchToForm(UIForm* f) { Log("ui switch " + N(f->mId)); }
    void PostAction(EEAction* a) { Log("ui post " + a->mText); }
    void PostLoginDone() { Log("ui login done"); }
};
static UserInterfaceW sUserInterface;
static UserInterfaceW* gUserInterface = &sUserInterface;
template <class T> struct UIListBox {};
struct EEAJoinOrHostAGame : EEAction {
    EEAJoinOrHostAGame(long m, const UWideString& p, const UWideString& g) { mText = "join/host " + N(m) + " " + (const char*)p + " " + (const char*)g; }
};
struct EEAWonNet : EEAction {
    EEAWonNet(long t, UIListBox<UWideString>* l, const UWideString& s, bool b) { mText = "won " + N(t) + " " + N(l != NULL) + " " + (const char*)s + " " + N(b); }
    EEAWonNet(long t, const UWideString& s, long h, long p, bool a, bool b, bool c)
    { mText = "won " + N(t) + " " + (const char*)s + " " + N(h) + " " + N(p) + " " + N(a) + N(b) + N(c); }
    EEAWonNet(long t, UITextLabel* l, const UWideString& p, const UWideString& s, bool m)
    { mText = "won " + N(t) + " " + (l ? N(l->mForm) + "/" + N(l->mId) : std::string("NULL")) + " " + (const char*)p + " " + (const char*)s + " " + N(m); }
};
struct EEUICWNRBLine { UWideString mName; UWideString mDescription; long mNumPlayers; long mNumGames; };
struct EEUICWonNetRoomsBox {
    long GetCurrentSelection(unsigned long& out) const { if (sK.mRoomsSelection < 0) return 1; out = (unsigned long)sK.mRoomsSelection; return 0; }
    void DeleteAllEntries() { Log("rooms clear"); }
    void AppendLine(const EEUICWNRBLine& l) { Log(std::string("rooms line ") + (const char*)l.mName + "|" + (const char*)l.mDescription + "|" + N(l.mNumPlayers) + "|" + N(l.mNumGames)); }
    unsigned long GetLineCount() const { return sK.mRoomsLines; }
    void ChangeSelectionIndex(unsigned long i, bool a, bool b) { Log("rooms select " + N(i) + " " + N(a) + N(b)); }
};
struct ResourceW { void GetString(long id, UWideString& out) { out = ("S" + N(id)).c_str(); } };
static ResourceW sResource;
static ResourceW* gResource = &sResource;
struct EEWonNetUI {
    long (*mSavePreferences)();
    void (*mDisplayMessage)(const UWideString&, unsigned long, UIForm*, void (*)(UIForm*), bool);
    void (*mDisplayCancel)(const UWideString&, unsigned long, UIForm*, void (*)(UIForm*), UIForm*);
    UIEvent* (*mNewRoomsOpenScreen)();
    UIEvent* (*mNewGamesOpenScreen)();
};
static std::function<std::string(void (*)(UIForm*))> sCallbackName;   // the test names both sides' callbacks
static std::function<std::string(UIForm*)> sFormArgName;
static int sEventIds;
static long SavePreferences() { Log("save preferences"); return 0; }
static void DisplayMessage(const UWideString& m, unsigned long a, UIForm* f, void (*cb)(UIForm*), bool b)
{ Log(std::string("message ") + (const char*)m + " " + N(a) + " " + sFormArgName(f) + " " + sCallbackName(cb) + " " + N(b)); }
static void DisplayCancel(const UWideString& m, unsigned long a, UIForm* f, void (*cb)(UIForm*), UIForm* c)
{ Log(std::string("cancel box ") + (const char*)m + " " + N(a) + " " + sFormArgName(f) + " " + sCallbackName(cb) + " " + N(c->mId)); }
static UIEvent* NewRooms() { UIEvent* e = new UIEvent{++sEventIds}; Log("new rooms event " + N(e->mId)); return e; }
static UIEvent* NewGames() { UIEvent* e = new UIEvent{++sEventIds}; Log("new games event " + N(e->mId)); return e; }
static EEWonNetUI gEEWonNetUI = {SavePreferences, DisplayMessage, DisplayCancel, NewRooms, NewGames};

// the network and the directory
struct NEProtocol {
    static void GetProtocol(int i, NEProtocol*& out) { static NEProtocol p; Log("protocol get " + N(i)); out = &p; }
    UWideString GetLocalAddress(unsigned long a) { return UWideString((sK.mLocalAddress + ":" + N(a)).c_str()); }
};
struct CDKey {
    bool mValid = false;
    explicit CDKey(const char*) {}   // the real constructor only stores the product name
    bool LoadFromRegistry() { Log("key load"); mValid = sK.mKeyValid; return true; }
    void SaveToRegistry() { Log("key save"); }
};
struct GetCertOp : AsyncOp { explicit GetCertOp(AuthContext*) { Log("cert op new"); } ~GetCertOp() { Log("cert op deleted"); } void SetCreateAccount(bool b) { Log("cert op create " + N(b)); } };
struct DirEntity : RefCount { std::wstring mName; };
// the original's list, and the reads the host makes of the Rust list since R6 WON round 34 (ee/EEWonNet.h DirList)
struct DirEntityList : std::list<SmartPtr<DirEntity> > {
    std::wstring Name(size_t i) const { return (*std::next(this->begin(), i))->mName; }
    void Keep(const DirEntityList& f, size_t i) { this->push_back(*std::next(f.begin(), i)); }
};
enum { EE_WON_REQOP_DIR_LIST = 1, EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS = 2, EE_WON_REQOP_ACCOUNT_NEWSLETTER = 3, EE_WON_REQOP_ACCOUNT_EMAIL = 4 };
struct ServerContext : RefCount {
    ~ServerContext() { Log("dir servers deleted"); }
    void AddAddress(const IPAddr& a) { Log("dir servers add " + a.mText); }
};
struct RustServerOp : AsyncOp {
    // (the ops' lifetimes: the front end's members and slots hold them; R6 WON lobby and network round 35)
    // a certificate op (RustServerOp::NewGetCert since round 36) logs as the original's GetCertOp stand-in
    bool mCert = false;
    ~RustServerOp() { Log(mCert ? "cert op deleted" : "server op deleted"); }
    static RustServerOp* NewGetCert(AuthContext*) { Log("cert op new"); RustServerOp* o = new RustServerOp; o->mCert = true; return o; }
    static RustServerOp* NewGetDir(ServerContext* c, const wchar_t* d, long f) { Log("dir op new " + N(c != NULL) + " " + WS(d) + " " + N(f)); return new RustServerOp; }
    static RustServerOp* NewUpdateAccount(ServerContext*, AuthContext*, const std::wstring& n, const std::string& p) { Log("update op new " + WS(n) + " " + p); return new RustServerOp; }
    static RustServerOp* NewGetAccount(ServerContext*, AuthContext*) { Log("account op new"); return new RustServerOp; }
    void SetNum(int f, long long v) { Log(mCert ? "cert op create " + N(v != 0) : "op num " + N(f) + " " + N(v)); }
    long long GetNum(int, long long) const { return mK.mNewsletter; }
    std::string GetBytes(int) const { return mK.mEmail; }
    DirEntityList GetEntities(int) const
    {
        DirEntityList l;
        for (const std::wstring& n : mK.mEntities) { DirEntity* e = new DirEntity; e->mName = n; l.push_back(e); }
        return l;
    }
};
struct WONAPICore { void Pump(unsigned long t, int) { Log("lobby pump " + N(t)); } };
struct WONLobbyWindow {
    bool mClosed = false;
    virtual ~WONLobbyWindow() { Log("lobby window deleted"); }
    void Virtual18() { Log("lobby window 18"); }
    void Close(bool) { Log("lobby window close"); mClosed = true; }
};
struct FSFileSpecW {
    size_t mAt = 0;
    void SetVolume(int) { Log("file volume"); }
    void SetDirectory(int) { Log("file directory"); }
    void SetFile(const UWideString& f) { Log(std::string("file name ") + (const char*)f); }
    int FOpen(const char* m) { Log(std::string("file open ") + m); mAt = 0; return sK.mFileOpens ? 0 : 1; }
    void Close() { Log("file close"); }
};
static int gStartupVolume, gStartupDirectory;
typedef UWideString UString;
inline char* fgets(char* b, int n, FSFileSpecW& f)
{
    Log("file gets");
    if (f.mAt >= sK.mFileLines.size()) return NULL;
    snprintf(b, n, "%s", sK.mFileLines[f.mAt++].c_str());
    return b;
}
typedef FSFileSpecW FSFileSpec;
}

// ---- the screens' world (R6 WON round 32): the forms, controls, list boxes, texts, fonts, the poll, the UI manager
// and a gWonNet the WON front end's screens (won_wonfront_orig.h, the deleted C++; and the host's statements,
// ee/EEWonFrontHost.inl, under the Rust wonfront.rs) call. Its own namespace, FE: the two sides (FE::Ori, FE::Rus)
// declare their own event, box and manager classes over these. Getters answer from the step's knobs and are not
// logged; every change is.
// the game's name check (ee/EEWonNet.h declares it): not empty, not starting with 'x'
bool EEIsValidName(const UWideString& s) { return s.GetLength() > 0 && ((const char*)s)[0] != 'x'; }

namespace FE {
using W::Log;
using W::N;
static std::string X(float f) { char b[48]; snprintf(b, sizeof b, "%a", (double)f); return b; }
static std::string T(const char* s) { return s ? std::string("\"") + s + "\"" : std::string("NULL"); }

enum { kErrorOK = 0, kDisplayModeHidden = 0, kDisplayModeShown = 1, kTextAlignLeft = 0, kFontStyleBold = 1 };
enum { kFormAvailableRooms = 31, kFormAvailableGames = 32, kFormUpdateSierraAccount = 37, kFormSelectServer = 38, kFormWonStats = 40 };
enum {
    kAvailableGamesPlayersNameLabel = 2, kAvailableGamesRoomsNameLabel = 4, kAvailableGamesJoinGameTab = 5,
    kAvailableGamesHostGameTab = 6, kAvailableGamesPlayersInRoomListBox = 8, kAvailableGamesAvalGamesListBox = 13,
    kAvailableGamesChatEditBox = 16, kAvailableGamesJoinHostGameButton = 17, kAvailableGamesBackButton = 18,
    kAvailableGamesHostGameNameEditBox = 24, kAvailableGamesPWForHostedGameEditBox = 27,
    kAvailableGamesPWForHostedGameCheckbox = 28, kAvailableGamesPWRetypeForHostedGameEditBox = 29,
    kAvailableGamesPWForHostedGameLabel = 30, kAvailableGamesPWRetypeForHostedGameLabel = 31,
    kAvailableGamesInProgressToggle = 32, kAvailableGamesPasswordToggle = 33, kAvailableGamesPrivateToggle = 34,
    kAvailableRoomsPlayersNameLabel = 2, kAvailableRoomsAvailRoomsListBox = 6, kAvailableRoomsJoinRoomButton = 7,
    kAvailableRoomsBackButton = 8, kAvailableRoomsUpdateMyAccount = 9, kAvailableRoomsTotalGamesNumber = 14,
    kAvailableRoomsServersNameLabel = 16, kUpdateAccountNewPasswordEditBox = 2, kUpdateAccountConfirmPasswordEditBox = 4,
    kUpdateAccountEnterEmailEditBox = 6, kUpdateAccountSendMeEmailCheckbox = 7, kUpdateAccountUpdateButton = 8,
    kUpdateAccountDontUpdateButton = 9, kSelectServerListBox = 1, kSelectServerOKButton = 2,
    kSelectServerCancelButton = 3, kWonStatsStatBox = 2, kWonStatsMyStatsLine = 22,
};
enum { kControlTriggerChecked = 0x3e9, kControlTriggerUnchecked = 0x3ea, kControlTriggerDoubleClicked = 0x3ec,
       kControlTriggerButtonUp = 0x3f0, kControlTriggerEnterPressed = 0x3f1 };
// (the strings' ids: ee/EEStringIDs.h's macros)

// ---- the step's knobs (the test draws them before each step)
struct StatKnob { std::string mText; unsigned long mL14, mL18, mNumGames, mL20, mL24, mL28; long mL2C; unsigned long mTotalTime; };
struct GameKnob { std::string mName; long mHumans, mPlayers; bool mHasPassword, mIsPrivate, mIsInProgress; };
struct RoomKnob { std::string mName, mDescription; long mNumPlayers, mNumGames; };
struct ServerKnob { std::wstring mDisplayName, mName; };
struct Knobs {
    unsigned long mTime = 0;
    bool mWonNet = true;
    bool mManager = false, mPollRunning = false, mPollFires = false;
    bool mUpdateEvent = false;
    bool mGroup2 = false;
    std::map<long, bool> mChecked;
    std::map<long, std::string> mTexts;
    long mUsersSel = -1; std::vector<std::string> mUsers;
    long mGamesSel = -1; std::vector<GameKnob> mGames;
    long mRoomsSel = -1; std::vector<RoomKnob> mRooms;
    std::vector<StatKnob> mStats;
    std::vector<ServerKnob> mServers;
    bool mServerHasSel = false; long mServerSel = 0;
    unsigned short mGameListPort = 0xffff;
    bool mRoomListChanged = false;
    long mPop = 0;
    unsigned long mDrawCount = 0, mLineHeight = 12, mLinesDisplayed = 0, mFontHeight = 12;
    float mCopyLeft = 0, mCopyTop = 0;
    long mScrollBar = 0, mBaseCopy = 0;
    std::string mHosted = "last game";
    std::wstring mUser = L"me", mRoom = L"Room A";
};
static Knobs sK;
static int sTextIds, sEventIds;

// ---- geometry, colours, fonts and texts
struct GE2DPoint { float x, y; GE2DPoint(float a, float b) : x(a), y(b) {} };
struct GERectangle {
    float mBottom = 0, mLeft = 0, mRight = 0, mTop = 0;   // lle/GERectangle.h's order
    GERectangle() {}
    GERectangle(const GE2DPoint& p, float w, float h) : mBottom(p.y + h), mLeft(p.x), mRight(p.x + w), mTop(p.y) {}
    float GetLeft() const { return mLeft; }
    float GetTop() const { return mTop; }
    float GetWidth() const { return mRight - mLeft; }
    void SetBottom(float v) { mBottom = v; }
    void SetLeft(float v) { mLeft = v; }
    void SetRight(float v) { mRight = v; }
    void SetTop(float v) { mTop = v; }
    void SetBottom(long v) { mBottom = (float)v; }
    void SetLeft(long v) { mLeft = (float)v; }
    void SetRight(long v) { mRight = (float)v; }
    void SetTop(long v) { mTop = (float)v; }
    GE2DPoint GetTopLeft() const { return GE2DPoint(mLeft, mTop); }
    std::string S() const { return X(mLeft) + "," + X(mTop) + "," + X(mRight) + "," + X(mBottom); }
};
struct GEColor {
    std::string mText;
    GEColor() : mText("default") {}
    GEColor(float r, float g, float b) : mText(X(r) + "/" + X(g) + "/" + X(b)) {}
};
struct GEFont {
    unsigned long mStyle;
    unsigned long GetHeight() const { return sK.mFontHeight; }
    const char* GetFontName() const { return "Arial"; }
    unsigned long GetFontStyle() const { return mStyle; }
    unsigned long GetFontQuality() const { return 3; }
    unsigned long GetFontSize() const { return 10; }
};
static GEFont sBaseFont = {0};
struct UIFontTable {
    static GEFont* FindOrCreateFont(const char* n, unsigned long style, unsigned long q, unsigned long size)
    {
        static std::map<unsigned long, GEFont*> sFonts;
        Log(std::string("font find ") + n + " " + N(style) + " " + N(q) + " " + N(size));
        GEFont*& f = sFonts[style];
        if (f == NULL) f = new GEFont{style};
        return f;
    }
};
struct GEText {
    int mId;
    GEFont* mFont;
    std::string mText;
    explicit GEText(GEFont* f) : mId(++sTextIds), mFont(f) { Log("text" + N(mId) + " new font " + N(f->mStyle)); }
    GEText(const GEText&) = delete;
    ~GEText() { Log("text" + N(mId) + " delete"); }
    GEText& operator=(const GEText& o) { Log("text" + N(mId) + " = text" + N(o.mId)); mFont = o.mFont; mText = o.mText; return *this; }
    void SetText(const UWideString& t) { mText = (const char*)t; Log("text" + N(mId) + " set " + T(t)); }
    void SetFgColor(const GEColor& c) { Log("text" + N(mId) + " fg " + c.mText); }
    void SetBgColor(const GEColor& c) { Log("text" + N(mId) + " bg " + c.mText); }
    void SetLocation(const GE2DPoint& p) { Log("text" + N(mId) + " at " + X(p.x) + "," + X(p.y)); }
    void SetIsTransparent(bool b) { Log("text" + N(mId) + " transparent " + N(b)); }
    void SetAlignment(int a) { Log("text" + N(mId) + " align " + N(a)); }
    GEFont* GetFont() const { return mFont; }
    void SetFont(GEFont* f) { mFont = f; Log("text" + N(mId) + " font " + N(f->mStyle)); }
    GERectangle GetTextExtent() const
    {
        float w = 0;
        for (unsigned char c : mText) w += 0.25f * (float)c;   // the width follows the characters, not only their count
        return GERectangle(GE2DPoint(0, 0), w, 10.0f);
    }
};

// ---- events, controls and forms
struct UIForm;
struct UIEntity {};
struct UIEvent {
    std::string mName;
    int mId;
    explicit UIEvent(const char* n) : mName(n), mId(++sEventIds) { Log("event new " + mName + "#" + N(mId)); }
    virtual ~UIEvent() {}
    virtual long ProcessEvent(UIForm*, UIEntity*) = 0;
    void Dereference() { Log("event deref " + mName + "#" + N(mId)); }
    std::string S() const { return mName + "#" + N(mId); }
};
struct UIControl {
    long mForm, mId;
    UIControl(long f, long i) : mForm(f), mId(i) {}
    virtual ~UIControl() {}
    std::string S() const { return N(mForm) + "/" + N(mId); }
    void SetTriggerEvent(unsigned long t, UIEvent* e) { Log("ctl " + S() + " trigger " + N(t) + " " + e->S()); }
    void SetDisplayMode(int m) { Log("ctl " + S() + " mode " + N(m)); }
    virtual long DrawTextA() { Log("ctl " + S() + " base draw text"); return 0; }
};
static GEText* sButtonText;
// an edit box, check box, radio button, button or label (the screens cast a form's control to the one they know)
struct UIAnyControl : UIControl {
    std::string mText;
    bool mChecked = false;
    UIAnyControl(long f, long i) : UIControl(f, i)
    {
        mText = sK.mTexts.count(i) ? sK.mTexts[i] : std::string();
        mChecked = sK.mChecked.count(i) ? sK.mChecked[i] : false;
    }
    const char* GetText(unsigned long) const { return mText.c_str(); }
    void DeleteText() { mText.clear(); Log("edit " + S() + " delete text"); }
    void SetText(const UWideString& t) { mText = (const char*)t; Log("ctl " + S() + " text " + T(t)); }
    void SetText(const UWideString& t, GEFont* f) { Log("button " + S() + " text " + T(t) + " font " + N(f->mStyle)); }
    void SetCaretLocation(unsigned long a, unsigned long b) { Log("edit " + S() + " caret " + N(a) + " " + N(b)); }
    bool GetIsChecked() const { return mChecked; }
    void SetChecked(bool b) { mChecked = b; Log("check " + S() + " " + N(b)); }
    void Check() { Log("radio " + S() + " check"); }
    GEText* GetGEText() const { return sButtonText; }
};
typedef UIAnyControl UIEditBox;
typedef UIAnyControl UICheckBox;
typedef UIAnyControl UIRadioButton;
typedef UIAnyControl UIButton;
typedef UIAnyControl UITextLabel;

struct EEUICWNGBLine { UWideString mName; long mHumans; long mPlayers; bool mHasPassword; bool mIsPrivate; bool mIsInProgress; };
struct EEUICWNRBLine { UWideString mName; UWideString mDescription; long mNumPlayers; long mNumGames; };
struct EEUICWNSBStat {
    UWideString mText;
    unsigned long mL14, mL18, mNumGames, mL20, mL24, mL28;
    long mL2C;
    unsigned long mTotalTime;
};
static std::string Describe(const UWideString& s) { return T(s); }
static std::string Describe(const EEUICWNGBLine& s) { return T(s.mName); }
static std::string Describe(const EEUICWNRBLine& s) { return T(s.mName); }
static std::string Describe(const EEUICWNSBStat& s)
{
    return T(s.mText) + " " + N(s.mL14) + " " + N(s.mL18) + " " + N(s.mNumGames) + " " + N(s.mL20) + " " + N(s.mL24) + " " +
           N(s.mL28) + " " + N(s.mL2C) + " " + N(s.mTotalTime);
}
struct UIBackground { long mForm, mId; void SetColor(const GEColor& c) { Log("ctl " + N(mForm) + "/" + N(mId) + " selected colour " + c.mText); } };
template <class T> struct UIListBox : UIControl {
    std::vector<T> mData;
    std::vector<T> mCopyLineArray;
    UIForm* mOwner;
    UIBackground mBackground;
    UIListBox(unsigned long id, const std::vector<T>&, unsigned long lineHeight, bool, const GERectangle& r, UIForm* f, unsigned long index, unsigned long border);
    UIForm* GetForm() const { return mOwner; }
    long GetCurrentSelection(unsigned long& out) const;
    const T& GetData(unsigned long i) const { return mData[i]; }
    bool GetHasSelection() const { return sK.mServerHasSel; }
    void DeleteAllEntries() { mData.clear(); Log("list " + S() + " clear"); }
    void AppendLine(const T& l) { mData.push_back(l); Log("list " + S() + " append " + Describe(l)); }
    void ChangeSelectionIndex(unsigned long i, bool a, bool b) { Log("list " + S() + " select " + N(i) + " " + N(a) + N(b)); }
    unsigned long GetNumberLinesDisplayed() const { return sK.mLinesDisplayed; }
    void SetCopiedHasSelection(bool b) { Log("list " + S() + " copied selection " + N(b)); }
    long CopyData() { Log("list " + S() + " base copy"); return sK.mBaseCopy; }
    long DrawPolygons() { Log("list " + S() + " base polygons"); return 0; }
    long DrawScrollBarText() { Log("list " + S() + " scroll bar text"); return sK.mScrollBar; }
    unsigned long GetDrawCount() const { return sK.mDrawCount < mCopyLineArray.size() ? sK.mDrawCount : (unsigned long)mCopyLineArray.size(); }
    unsigned long GetLineHeight() const { return sK.mLineHeight; }
    mutable GERectangle mCopyRect;   // the real one answers a reference to the Rust object's rectangle
    const GERectangle& GetCopyTextRectangle() const { mCopyRect.mLeft = sK.mCopyLeft; mCopyRect.mTop = sK.mCopyTop; mCopyRect.mRight = mCopyRect.mBottom = 999; return mCopyRect; }
    UIBackground* GetSelectedBackground() { return &mBackground; }
    void SetFocusedRectColor(const GEColor& c) { Log("list " + S() + " focus colour " + c.mText); }
};
struct UIEntityGroup {
    long mForm, mGroup;
    void Enable() { Log("form " + N(mForm) + " group " + N(mGroup) + " enable"); }
    void Disable() { Log("form " + N(mForm) + " group " + N(mGroup) + " disable"); }
    bool GetIsEnabled() const { return mGroup == 2 ? sK.mGroup2 : !sK.mGroup2; }
};
struct UIFormManager { virtual ~UIFormManager() {} };
struct FormPopper { long mForm; long Pop() { Log("form " + N(mForm) + " pop"); return sK.mPop; } };
struct UIForm {
    long mId;
    std::map<long, UIControl*> mControls;
    UIEntityGroup mGroups[3];
    UIEvent* mUpdateEvent = NULL;
    UIFormManager* mManager = NULL;
    FormPopper mPopper;
    explicit UIForm(long id) : mId(id), mPopper{id} { for (int g = 0; g < 3; g++) mGroups[g] = UIEntityGroup{id, g}; }
    UIControl* GetControl(unsigned long i) { return mControls.count((long)i) ? mControls[(long)i] : NULL; }
    UIEntityGroup* GetEntityGroup(unsigned long g) { return &mGroups[g]; }
    void SetTabbing(bool b) { Log("form " + N(mId) + " tabbing " + N(b)); }
    void SetTabStop(unsigned long t) { Log("form " + N(mId) + " tab stop " + N(t)); }
    UIEvent* GetFormUpdateEvent() const { return mUpdateEvent; }
    void SetFormUpdateEvent(UIEvent* e) { mUpdateEvent = e; Log("form " + N(mId) + " update event " + (e ? e->S() : std::string("NULL"))); }
    void SetFormActivateEvent(UIEvent* e) { Log("form " + N(mId) + " activate event " + e->S()); }
    void SetFormDeactivateEvent(UIEvent* e) { Log("form " + N(mId) + " deactivate event " + e->S()); }
    void SetFormCloseEvent(UIEvent* e) { Log("form " + N(mId) + " close event " + e->S()); }
    void SetFormOpenEvent(UIEvent* e) { Log("form " + N(mId) + " open event " + e->S()); }
    FormPopper* GetManager() { return &mPopper; }
    UIFormManager* GetFormManager() const { return mManager; }
    void SetFormManager(UIFormManager* m) { mManager = m; Log("form " + N(mId) + " manager set"); }
    long DrawClippedText(const GERectangle& r, GEText* t, UIControl* c)
    {
        Log("form " + N(mId) + " draw " + r.S() + " text" + N(t->mId) + " " + T(t->mText.c_str()) + " by " + c->S());
        return (long)((int)r.mLeft & 3);
    }
};
template <class T> UIListBox<T>::UIListBox(unsigned long id, const std::vector<T>&, unsigned long lineHeight, bool, const GERectangle& r, UIForm* f, unsigned long index, unsigned long border)
    : UIControl(f->mId, (long)id), mOwner(f), mBackground{f->mId, (long)id}
{
    Log("list " + S() + " new " + N(lineHeight) + " " + r.S() + " " + N(index) + " " + N(border));
}
template <> long UIListBox<UWideString>::GetCurrentSelection(unsigned long& out) const
{
    long s = mForm == kFormSelectServer ? sK.mServerSel : sK.mUsersSel;
    if (s < 0) return 1;
    out = (unsigned long)s;
    return 0;
}
template <> long UIListBox<EEUICWNGBLine>::GetCurrentSelection(unsigned long& out) const { if (sK.mGamesSel < 0) return 1; out = (unsigned long)sK.mGamesSel; return 0; }
template <> long UIListBox<EEUICWNRBLine>::GetCurrentSelection(unsigned long& out) const { if (sK.mRoomsSel < 0) return 1; out = (unsigned long)sK.mRoomsSel; return 0; }
template <> long UIListBox<EEUICWNSBStat>::GetCurrentSelection(unsigned long&) const { return 1; }

// ---- the poll, the UI manager, the resources and gWonNet
struct TMPoll {
    bool mRunning;
    TMPoll(unsigned long a, unsigned long b, bool c) : mRunning(sK.mPollRunning) { Log("poll new " + N(a) + " " + N(b) + " " + N(c)); }
    ~TMPoll() { Log("poll delete"); }
    bool Poll() { Log("poll poll"); return sK.mPollFires; }
    bool GetIsRunning() const { return mRunning; }
    void Start() { mRunning = true; Log("poll start"); }
    void Stop() { mRunning = false; Log("poll stop"); }
};
inline unsigned long TMGetTime() { return sK.mTime; }
static std::map<long, UIForm*> sForms;   // the side's forms this step
typedef void (*PromptCallback)(const char*);
static PromptCallback sPrompt;           // the password prompt's callback (the test answers it)
static std::function<std::string(PromptCallback)> sPromptName;
struct UIManagerFE {
    UIForm* GetFormByID(unsigned long id) { return sForms.count((long)id) ? sForms[(long)id] : NULL; }
    UIForm* GetCurrentForm() { return sForms[12]; }
    void LockUIForCopyData() { Log("ui lock"); }
    void UnlockUIForCopyData() { Log("ui unlock"); }
    void Push(UIForm* f) { Log("ui push " + N(f->mId)); }
    long Pop() { Log("ui pop"); return sK.mPop; }
    void DisplayMessage(const UWideString& m, unsigned long a, UIForm* f, void (*cb)(UIForm*), bool b)
    { Log("ui message " + T(m) + " " + N(a) + " " + N(f != NULL) + " " + N(cb != NULL) + " " + N(b)); }
    void DisplayPasswordPrompt(const UWideString& m, bool a, PromptCallback cb, bool b)
    { sPrompt = cb; Log("ui password prompt " + T(m) + " " + N(a) + " " + sPromptName(cb) + " " + N(b)); }
};
static UIManagerFE sUIManagerFE;
static UIManagerFE* gUIManager = &sUIManagerFE;
struct ResourceFE { long GetString(unsigned long id, UWideString& out) { out = ("S" + N((long long)id)).c_str(); return 0; } };
static ResourceFE sResourceFE;
static ResourceFE* gResource = &sResourceFE;
static UWideString gHostedGameName;
static void EEUIEWNSetTriggerEvent(UIEvent* inEvent, UIControl* inControl, unsigned long inTrigger)
{
    inControl->SetTriggerEvent(inTrigger, inEvent);
    inEvent->Dereference();
}
namespace WONAPI {
inline std::string WStringToString(const std::wstring& w) { std::string s; for (wchar_t c : w) s += (char)c; return s; }
struct IPAddrFE { std::string mText; };
struct DirEntity : W::RefCount {
    std::wstring mDisplayName, mName;
    IPAddrFE GetNetAddrAsIP() const { return IPAddrFE{W::WS(mName) + ":15101"}; }
};
// the original's list, and the reads the host makes of the Rust list since R6 WON round 34 (ee/EEWonNet.h DirList)
struct DirEntityList : std::list<W::SmartPtr<DirEntity> > {
    const DirEntity* At(size_t i) const { return std::next(this->begin(), i)->get(); }
    std::wstring Name(size_t i) const { return At(i)->mName; }
    std::wstring DisplayName(size_t i) const { return At(i)->mDisplayName; }
    IPAddrFE Addr(size_t i) const { return At(i)->GetNetAddrAsIP(); }
};
}
struct UserFE { std::wstring GetName() const { return sK.mUser; } };
struct FEWonNet {
    UserFE* mUser = new UserFE;
    WONAPI::DirEntityList mServers;
    static std::string C(UIControl* c) { return c ? c->S() : std::string("NULL"); }
    unsigned short GetGameListPort() { return sK.mGameListPort; }
    void RefreshGameList(UIControl* c) { Log("net refresh games " + C(c)); }
    void UpdateGameList() { Log("net update games"); }
    void LeaveRoom() { Log("net leave room"); }
    void HostGame(const char* n, const char* p) { Log("net host " + T(n) + " " + T(p)); }
    void JoinGame(const char* n, const char* p) { Log("net join game " + T(n) + " " + T(p)); }
    void JoinRoom(const char* n, const char* p) { Log("net join room " + T(n) + " " + T(p)); }
    void SendChat(const UWideString& t) { Log("net chat " + T(t)); }
    std::wstring GetRoomName() { return sK.mRoom; }
    bool GetRoomListChanged() { return sK.mRoomListChanged; }
    void UpdateRoomList(UIControl* r, UIControl* l16, UIControl* l14) { Log("net update rooms " + C(r) + " " + C(l16) + " " + C(l14)); }
    void OpenUpdateAccount() { Log("net open account"); }
    void ConnectToLobby() { Log("net connect lobby"); }
    void UpdateAccount(const UWideString& a, const UWideString& b, bool f) { Log("net update account " + T(a) + " " + T(b) + " " + N(f)); }
    void SelectServer(const std::wstring& n, WONAPI::IPAddrFE a) { Log("net select server " + W::WS(n) + " " + a.mText); }
};
static FEWonNet* gWonNet;
}
#endif
