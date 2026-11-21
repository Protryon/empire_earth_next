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
    ~SmartRoutingConnection() { delete mModel; }
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
struct GetCertOp : AsyncOp { explicit GetCertOp(AuthContext*) { Log("cert op new"); } void SetCreateAccount(bool b) { Log("cert op create " + N(b)); } };
struct DirEntity : RefCount { std::wstring mName; };
typedef std::list<SmartPtr<DirEntity> > DirEntityList;
enum { EE_WON_REQOP_DIR_LIST = 1, EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS = 2, EE_WON_REQOP_ACCOUNT_NEWSLETTER = 3, EE_WON_REQOP_ACCOUNT_EMAIL = 4 };
struct ServerContext : RefCount { void AddAddress(const IPAddr& a) { Log("dir servers add " + a.mText); } };
struct RustServerOp : AsyncOp {
    static RustServerOp* NewGetDir(ServerContext* c, const wchar_t* d, long f) { Log("dir op new " + N(c != NULL) + " " + WS(d) + " " + N(f)); return new RustServerOp; }
    static RustServerOp* NewUpdateAccount(ServerContext*, AuthContext*, const std::wstring& n, const std::string& p) { Log("update op new " + WS(n) + " " + p); return new RustServerOp; }
    static RustServerOp* NewGetAccount(ServerContext*, AuthContext*) { Log("account op new"); return new RustServerOp; }
    void SetNum(int f, long long v) { Log("op num " + N(f) + " " + N(v)); }
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
#endif
