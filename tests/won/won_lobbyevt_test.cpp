// The lobby's events as Rust objects (R6 WONLobby round 18: rust/crates/ee_won_lobby/src/lobbyevt.rs under the
// makers and readers of include/ee/wonapi/WONLobbyEvents.h): the original 43 event classes (won_lobbyevt_orig.h,
// unchanged) against the new events, over one stand-in world of reference-counted model objects, operations, texts,
// images and addresses.
//
// Each sequence makes one event of a random class with random values (objects and texts from the world's pools or
// NULL, numbers over their members' ranges, addresses with random hosts and ports) on both sides: the event's type,
// every member the receivers read (the original member against LobbyEvtGet / LobbyEvtText / LobbyEvtAddr /
// LobbyEvtStr, the change kind's IsAdd / IsDelete / IsModify, the chat line's IsWhisper / IsEmote / IsWarning /
// IsTeam), the writes the hosts make (mHandled, CreateAccountEvent's members, LoginEvent's user name cut) and the
// references: each object's count after the event is made, and the order the event gives them back at its end.
// The shared empty text (the original's default GUIString members, assigned in AlertEvent's and
// ClientModeratedEvent's bodies) is not counted.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/won_net/ipobj.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL seq %d line %d: %s\n", sSeq, __LINE__, #c); } } while (0)
static int sSeq;

static unsigned sSeed = 4711;
static unsigned R(unsigned n)
{
    sSeed = sSeed * 1103515245u + 12345u;
    return ((sSeed >> 16) & 0x7fff) % n;
}

// the releases an event's end makes, in order
static std::vector<int> sReleased;

namespace WONAPI {
// ---- the world
class RefCount {
public:
    RefCount() : mRefCount(0), mId(0) {}
    virtual ~RefCount() {}
    void AddRef() { this->mRefCount++; }
    void Release() { this->mRefCount--; if (this->mId > 0) sReleased.push_back(this->mId); }
    int mRefCount;
    int mId; // 0: the shared empty text (not counted)
};
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (this->mPtr) this->mPtr->AddRef(); }
    ~SmartPtr() { if (this->mPtr) this->mPtr->Release(); }
    SmartPtr& operator=(T* p)
    {
        if (p != this->mPtr) { if (this->mPtr) this->mPtr->Release(); this->mPtr = p; if (p) p->AddRef(); }
        return *this;
    }
    SmartPtr& operator=(const SmartPtr& o) { return this->operator=(o.mPtr); }
    T* get() const { return this->mPtr; }
    T* operator->() const { return this->mPtr; }
    T* mPtr;
};
class GUIStringData : public RefCount {
public:
    std::wstring mText;
};
static GUIStringData sEmpty;
class GUIString {
public:
    GUIString() : mData(&sEmpty) {}
    GUIString(GUIStringData* d) : mData(d) {}
    GUIString(const char* t) : mData(&sEmpty) { (void)t; }
    bool operator==(const GUIString& o) const { return this->mData.get()->mText == o.mData.get()->mText; }
    operator std::wstring() const { return this->mData.get()->mText; }
    operator std::string() const
    {
        std::string s;
        for (wchar_t c : this->mData.get()->mText) s += (char)c;
        return s;
    }
    SmartPtr<GUIStringData> mData;
};
class IPAddr {
public:
    struct AdoptTag {};
    IPAddr() : mAddr(ee_won_ipaddr_new()) {}
    IPAddr(EeWonIpAddr* a, AdoptTag) : mAddr(a) {}
    IPAddr(const IPAddr& o) : mAddr(ee_won_ipaddr_clone(o.mAddr)) {}
    IPAddr& operator=(const IPAddr& o) { ee_won_ipaddr_assign(this->mAddr, o.mAddr); return *this; }
    ~IPAddr() { ee_won_ipaddr_free(this->mAddr); }
    const EeWonIpAddr* GetValue() const { return this->mAddr; }
    static IPAddr FromValue(EeWonIpAddr* v) { return IPAddr(v, AdoptTag()); }
    bool operator==(const IPAddr& o) const { return ee_won_ipaddr_compare(this->mAddr, o.mAddr) == 0; }
    EeWonIpAddr* mAddr;
};
class Component;
class ComponentEvent : public RefCount {
public:
    ComponentEvent(Component* c, int t) : mType(t), mComponent(c) {}
    int mType;
    Component* mComponent;
};
class LobbyEvent : public ComponentEvent {
public:
    LobbyEvent(int inType) : ComponentEvent(NULL, inType) {}
};
class LobbyClient : public RefCount {};
class LobbyFriend : public RefCount {};
class LobbyGame : public RefCount {};
class LobbyServer : public RefCount {};
class LobbyGroup : public RefCount {};
class LobbyClientList : public RefCount {};
class LobbyGameList : public RefCount {};
class RustServerOp : public RefCount {};
class Image {};
}
#define EE_LOBBYEVT_STANDIN
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbyhost.h"
#include "won_lobbyevt_orig.h"

using namespace WONAPI;
typedef LobbyEvtField F;
typedef LobbyEvtKind K;

static LobbyClient sClients[4];
static LobbyFriend sFriends[3];
static LobbyGame sGames[3];
static LobbyServer sServers[3];
static LobbyGroup sGroups[3];
static LobbyClientList sLists[2];
static LobbyGameList sGameLists[2];
static RustServerOp sOps[2];
static GUIStringData sTexts[6];
static Image sImages[2];
static std::vector<RefCount*> sAll;

template <class T, size_t N> static T* Pick(T (&inPool)[N]) { return R(5) == 0 ? NULL : &inPool[R(N)]; }
static GUIString Text() { return GUIString(&sTexts[R(6)]); }
static int Int() { int v[] = {0, 1, 2, -1, 7, 100049, 0x7fffffff, (int)0x80000000}; return R(3) ? v[R(8)] : (int)(R(60000) - 30000); }
static unsigned long ULong() { return R(4) ? (unsigned long)R(1000000) : (unsigned long)0xffffffffu - R(5); }
static bool Bool() { return R(2) != 0; }
static unsigned short UShort() { return (unsigned short)R(65536); }
static IPAddr Addr()
{
    IPAddr a;
    if (R(6)) ee_won_ipaddr_set_host_order(a.mAddr, 0x0a000000u + R(30000), (unsigned short)R(65536));
    return a;
}

static std::vector<int> Counts()
{
    std::vector<int> c;
    for (RefCount* r : sAll) c.push_back(r->mRefCount);
    return c;
}

// one sequence: the class's values drawn once, then the same event on each side
struct Values {
    LobbyClient* c[2];
    LobbyFriend* f;
    LobbyGame* g;
    LobbyServer* s;
    LobbyGroup* gr;
    LobbyClientList* cl;
    LobbyGameList* gl;
    RustServerOp* op;
    Image* img;
    GUIString t[3];
    int i[4];
    unsigned long ul;
    unsigned short us;
    bool b[3];
    IPAddr addr;
    std::string link;
};

static void Draw(Values& v)
{
    v.c[0] = Pick(sClients);
    v.c[1] = Pick(sClients);
    v.f = Pick(sFriends);
    v.g = Pick(sGames);
    v.s = Pick(sServers);
    v.gr = Pick(sGroups);
    v.cl = Pick(sLists);
    v.gl = Pick(sGameLists);
    v.op = Pick(sOps);
    v.img = Pick(sImages);
    for (int k = 0; k < 3; k++) v.t[k] = Text();
    for (int k = 0; k < 4; k++) v.i[k] = Int();
    v.ul = ULong();
    v.us = UShort();
    for (int k = 0; k < 3; k++) v.b[k] = Bool();
    v.addr = Addr();
    v.link = R(3) ? std::string("http://x/") + std::to_string((long long)R(1000)) : std::string();
}

// makes the event of class `k` on one side
static ComponentEvent* Make(bool inRust, int k, Values& v)
{
#define BOTH(cls, ...) return inRust ? (ComponentEvent*)WONAPI::cls(__VA_ARGS__) : (ComponentEvent*)new Orig::cls(__VA_ARGS__)
    switch ((K)k) {
    case K::AcceptClient: BOTH(AcceptClientEvent, v.c[0], v.b[0]);
    case K::Alert: BOTH(AlertEvent, v.i[0], v.i[1], v.i[2], v.t[0]);
    case K::AnnounceClientLocation: BOTH(AnnounceClientLocationEvent, v.c[0], v.t[0], v.t[1], v.i[0]);
    case K::AnnounceFriend: BOTH(AnnounceFriendEvent, v.f, v.i[0], v.i[1]);
    case K::FriendAction: BOTH(FriendActionEvent, v.i[0], v.f, v.i[1]);
    case K::FriendChanged: BOTH(FriendChangedEvent, v.f, v.i[0]);
    case K::GameAction: BOTH(GameActionEvent, v.i[0], v.g);
    case K::GameChanged: BOTH(GameChangedEvent, v.g, v.i[0]);
    case K::JoinFriendsRoom: BOTH(JoinFriendsRoomEvent, v.f);
    case K::JoinGame: BOTH(JoinGameEvent, v.g);
    case K::PingGame: BOTH(PingGameEvent, v.g, v.b[0]);
    case K::PlayerChanged: BOTH(PlayerChangedEvent, v.c[0], v.i[0]);
    case K::PlayerTeamChanged: BOTH(PlayerTeamChangedEvent, v.c[0], v.i[0]);
    case K::ServerChanged: BOTH(ServerChangedEvent, v.s, v.i[0]);
    case K::ClientAction: BOTH(ClientActionEvent, v.i[0], v.i[1], v.c[0]);
    case K::GotHttpDocument: BOTH(GotHTTPDocumentEvent, v.op);
    case K::ChangeDirectConnect: BOTH(ChangeDirectConnectEvent, v.i[0], v.g);
    case K::ClientChanged: BOTH(ClientChangedEvent, v.c[0], v.i[0], v.i[1]);
    case K::ClientModerated: BOTH(ClientModeratedEvent, v.c[0], v.i[0], v.t[0], v.i[1], v.ul, v.b[0], v.t[1]);
    case K::GroupChanged: BOTH(GroupChangedEvent, v.gr, v.s, v.i[0]);
    case K::GroupInvitation: BOTH(GroupInvitationEvent, v.i[0], v.t[0], v.us, v.c[0], v.b[0], v.t[1], v.b[1]);
    case K::GroupJoinAttempt: BOTH(GroupJoinAttemptEvent, v.i[0], v.c[0], v.t[0]);
    case K::JoinedRoom: BOTH(JoinedRoomEvent, v.t[0], v.t[1], v.i[0], v.us, v.cl);
    case K::RecvChat: BOTH(RecvChatEvent, v.i[0], v.ul, v.c[0], v.t[0], v.c[1]);
    case K::SetRoomSpecFlags: BOTH(SetRoomSpecFlagsEvent, v.i[0]);
    case K::LoginInitStatus: BOTH(LoginInitStatusEvent, v.t[0], v.t[1], v.b[0], v.i[0]);
    case K::SetLoginFields: BOTH(SetLoginFieldsEvent, v.t[0], v.t[1], v.b[0]);
    case K::QueryAccountResult: BOTH(QueryAccountResultEvent, v.t[0], v.b[0]);
    case K::Login: BOTH(LoginEvent, v.t[0], v.b[0], v.t[1]);
    case K::GetLostPassword: BOTH(GetLostPasswordEvent, v.t[0]);
    case K::GetLostUserName: BOTH(GetLostUserNameEvent, v.t[0]);
    case K::LeftRoom: BOTH(LeftRoomEvent, v.i[0], v.b[0], v.b[1]);
    case K::SetChatInput: BOTH(SetChatInputEvent, v.i[0], v.t[0], v.i[1], v.b[0]);
    case K::ChatCommandError: BOTH(ChatCommandErrorEvent, v.t[0]);
    case K::CreateRoom: BOTH(CreateRoomEvent, v.addr);
    case K::JoinRoom: BOTH(JoinRoomEvent, v.addr, v.ul);
    case K::CreateAccount: BOTH(CreateAccountEvent);
    case K::SyncGameList: BOTH(SyncGameListEvent, v.gl);
    case K::JumpToRoom: BOTH(JumpToRoomEvent, v.t[0], v.us, v.addr);
    case K::QuickPlay: BOTH(QuickPlayEvent, v.i[0]);
    case K::SetCrossPromotion: BOTH(SetCrossPromotionEvent, v.img, v.link);
    case K::UpdateAccount: BOTH(UpdateAccountEvent, v.t[0], v.t[1], v.b[0]);
    case K::QueryServerGroups: BOTH(QueryServerGroupsEvent, v.addr);
    }
#undef BOTH
    return NULL;
}

static intptr_t G(ComponentEvent* e, F f) { return LobbyEvtGet(e, f); }
static bool T(ComponentEvent* e, F f, const GUIString& o) { return LobbyEvtText(e, f).mData.get() == o.mData.get(); }
template <class S> static S Str(ComponentEvent* e, F f) { return LobbyEvtStr<S>(e, f); }
static bool A(ComponentEvent* e, F f, const IPAddr& o) { return LobbyEvtAddr(e, f) == o; }
static void ChangeKind(ComponentEvent* n, Orig::LobbyChangeEvent* o)
{
    CHECK(G(n, F::ChangeKind) == o->mChangeKind);
    CHECK((G(n, F::ChangeKind) == 0) == o->IsAdd());
    CHECK((G(n, F::ChangeKind) == 1) == o->IsDelete());
    CHECK((G(n, F::ChangeKind) == 2) == o->IsModify());
}

// every member of the class, read on both sides; then the hosts' writes on both
static void Compare(int k, ComponentEvent* oe, ComponentEvent* n)
{
    CHECK(oe->mType == n->mType);
    switch ((K)k) {
    case K::AcceptClient: { auto* o = (Orig::AcceptClientEvent*)oe; CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(G(n, F::Accepted) == o->mAccepted); break; }
    case K::Alert: {
        auto* o = (Orig::AlertEvent*)oe;
        CHECK(G(n, F::Kind) == o->GetKind()); CHECK(G(n, F::Value) == o->GetValue()); CHECK(G(n, F::Severity) == o->mSeverity);
        CHECK(T(n, F::Text, o->GetText()));
        break;
    }
    case K::AnnounceClientLocation: {
        auto* o = (Orig::AnnounceClientLocationEvent*)oe;
        CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(T(n, F::Server, o->mServer)); CHECK(T(n, F::Room, o->mRoom));
        CHECK(G(n, F::RoomId) == o->mRoomId); CHECK(G(n, F::Handled) == o->mHandled);
        if (R(2)) { o->mHandled = true; LobbyEvtSet(n, F::Handled, 1); CHECK(G(n, F::Handled) == o->mHandled); }
        break;
    }
    case K::AnnounceFriend: {
        auto* o = (Orig::AnnounceFriendEvent*)oe;
        ChangeKind(n, o); CHECK(G(n, F::Friend) == (intptr_t)o->mFriend.get()); CHECK(G(n, F::Location) == o->mLocation);
        CHECK(G(n, F::Handled) == o->mHandled);
        if (R(2)) { o->mHandled = true; LobbyEvtSet(n, F::Handled, 1); CHECK(G(n, F::Handled) == o->mHandled); }
        break;
    }
    case K::FriendAction: {
        auto* o = (Orig::FriendActionEvent*)oe;
        CHECK(G(n, F::Action) == o->mAction); CHECK(G(n, F::Friend) == (intptr_t)o->mFriend.get()); CHECK(G(n, F::Value) == o->mValue);
        break;
    }
    case K::FriendChanged: { auto* o = (Orig::FriendChangedEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Friend) == (intptr_t)o->mFriend.get()); break; }
    case K::GameAction: { auto* o = (Orig::GameActionEvent*)oe; CHECK(G(n, F::Action) == o->mAction); CHECK(G(n, F::Game) == (intptr_t)o->mGame.get()); break; }
    case K::GameChanged: { auto* o = (Orig::GameChangedEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Game) == (intptr_t)o->mGame.get()); break; }
    case K::JoinFriendsRoom: { auto* o = (Orig::JoinFriendsRoomEvent*)oe; CHECK(G(n, F::Friend) == (intptr_t)o->mFriend.get()); break; }
    case K::JoinGame: { auto* o = (Orig::JoinGameEvent*)oe; CHECK(G(n, F::Game) == (intptr_t)o->mGame.get()); break; }
    case K::PingGame: { auto* o = (Orig::PingGameEvent*)oe; CHECK(G(n, F::Game) == (intptr_t)o->mGame.get()); CHECK(G(n, F::Flag) == o->mFlag); break; }
    case K::PlayerChanged: { auto* o = (Orig::PlayerChangedEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); break; }
    case K::PlayerTeamChanged: { auto* o = (Orig::PlayerTeamChangedEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); break; }
    case K::ServerChanged: { auto* o = (Orig::ServerChangedEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Server) == (intptr_t)o->mServer.get()); break; }
    case K::ClientAction: {
        auto* o = (Orig::ClientActionEvent*)oe;
        CHECK(G(n, F::Action) == o->GetAction()); CHECK(G(n, F::RoomSpec) == o->GetRoomSpec()); CHECK(G(n, F::Client) == (intptr_t)o->GetClient());
        break;
    }
    case K::GotHttpDocument: { auto* o = (Orig::GotHTTPDocumentEvent*)oe; CHECK(G(n, F::Op) == (intptr_t)o->mOp.get()); break; }
    case K::ChangeDirectConnect: { auto* o = (Orig::ChangeDirectConnectEvent*)oe; ChangeKind(n, o); CHECK(G(n, F::Game) == (intptr_t)o->mGame.get()); break; }
    case K::ClientChanged: {
        auto* o = (Orig::ClientChangedEvent*)oe;
        ChangeKind(n, o); CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(G(n, F::RoomSpec) == o->GetRoomSpec());
        break;
    }
    case K::ClientModerated: {
        auto* o = (Orig::ClientModeratedEvent*)oe;
        CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(G(n, F::RoomSpec) == o->mRoomSpec); CHECK(T(n, F::RoomName, o->mRoomName));
        CHECK(G(n, F::ModerationKind) == o->mModerationKind); CHECK((unsigned long)G(n, F::Duration) == o->mDuration);
        CHECK(G(n, F::Flag) == o->mFlag); CHECK(T(n, F::Comment, o->mComment));
        break;
    }
    case K::GroupChanged: {
        auto* o = (Orig::GroupChangedEvent*)oe;
        ChangeKind(n, o); CHECK(G(n, F::Server) == (intptr_t)o->mServer.get()); CHECK(G(n, F::Group) == (intptr_t)o->mGroup.get());
        break;
    }
    case K::GroupInvitation: {
        auto* o = (Orig::GroupInvitationEvent*)oe;
        CHECK(G(n, F::Kind) == o->mKind); CHECK(T(n, F::GameName, o->mGameName)); CHECK(G(n, F::GameId) == o->mGameId);
        CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(G(n, F::Invite) == o->mInvite); CHECK(G(n, F::Flag) == o->mFlag);
        CHECK(T(n, F::Comment, o->mComment));
        break;
    }
    case K::GroupJoinAttempt: {
        auto* o = (Orig::GroupJoinAttemptEvent*)oe;
        CHECK(G(n, F::Kind) == o->mKind); CHECK(G(n, F::Client) == (intptr_t)o->mClient.get()); CHECK(T(n, F::Comment, o->mComment));
        break;
    }
    case K::JoinedRoom: {
        auto* o = (Orig::JoinedRoomEvent*)oe;
        CHECK(G(n, F::Clients) == (intptr_t)o->mClients.get()); CHECK(T(n, F::ServerName, o->mServerName));
        CHECK(T(n, F::GroupName, o->mGroupName)); CHECK(G(n, F::ClientId) == o->mClientId); CHECK(G(n, F::RoomSpec) == o->GetRoomSpec());
        break;
    }
    case K::RecvChat: {
        auto* o = (Orig::RecvChatEvent*)oe;
        CHECK(G(n, F::Sender) == (intptr_t)o->mSender.get()); CHECK(G(n, F::Target) == (intptr_t)o->mTarget.get());
        CHECK(G(n, F::RoomSpec) == o->mRoomSpec); CHECK((unsigned long)G(n, F::Flags) == o->mFlags); CHECK(T(n, F::Text, o->mText));
        CHECK(((G(n, F::Flags) & 4) != 0) == o->IsWhisper()); CHECK(((G(n, F::Flags) & 2) != 0) == o->IsEmote());
        CHECK(((G(n, F::Flags) & 16) != 0) == o->IsWarning()); CHECK(((G(n, F::Flags) & 256) != 0) == o->IsTeam());
        break;
    }
    case K::SetRoomSpecFlags: { auto* o = (Orig::SetRoomSpecFlagsEvent*)oe; CHECK(G(n, F::Flags) == o->mFlags); break; }
    case K::LoginInitStatus: {
        auto* o = (Orig::LoginInitStatusEvent*)oe;
        CHECK(T(n, F::Status, o->mStatus)); CHECK(T(n, F::Description, o->mDescription)); CHECK(G(n, F::AllowIgnore) == o->mAllowIgnore);
        CHECK(G(n, F::Kind) == o->mKind);
        break;
    }
    case K::SetLoginFields: {
        auto* o = (Orig::SetLoginFieldsEvent*)oe;
        CHECK(T(n, F::UserName, o->mUserName)); CHECK(T(n, F::Password, o->mPassword)); CHECK(G(n, F::Remember) == o->mRemember);
        break;
    }
    case K::QueryAccountResult: { auto* o = (Orig::QueryAccountResultEvent*)oe; CHECK(T(n, F::Email, o->mEmail)); CHECK(G(n, F::Newsletter) == o->mNewsletter); break; }
    case K::Login: {
        auto* o = (Orig::LoginEvent*)oe;
        CHECK(Str<std::wstring>(n, F::UserName) == o->mUserName); CHECK(Str<std::wstring>(n, F::Password) == o->mPassword);
        CHECK(G(n, F::Remember) == o->mRemember);
        // AccountLogic's LoginName (the data and length) and LoginStripAt
        CHECK(memcmp((const void*)G(n, F::UserName), o->mUserName.data(), o->mUserName.size() * sizeof(wchar_t)) == 0);
        if (!o->mUserName.empty() && R(2)) {
            o->mUserName = o->mUserName.substr(1);
            LobbyEvtSetStr(n, F::UserName, Str<std::wstring>(n, F::UserName).substr(1));
            CHECK(Str<std::wstring>(n, F::UserName) == o->mUserName);
        }
        break;
    }
    case K::GetLostPassword: { auto* o = (Orig::GetLostPasswordEvent*)oe; CHECK(Str<std::wstring>(n, F::UserName) == o->mUserName); break; }
    case K::GetLostUserName: { auto* o = (Orig::GetLostUserNameEvent*)oe; CHECK(Str<std::string>(n, F::Email) == o->mEmail); break; }
    case K::LeftRoom: {
        auto* o = (Orig::LeftRoomEvent*)oe;
        CHECK(G(n, F::RoomSpec) == o->GetRoomSpec()); CHECK(G(n, F::Forced) == o->mForced); CHECK(G(n, F::Flag) == o->mFlag);
        break;
    }
    case K::SetChatInput: {
        auto* o = (Orig::SetChatInputEvent*)oe;
        CHECK(G(n, F::Command) == o->mCommand); CHECK(T(n, F::Text, o->mText)); CHECK(G(n, F::RoomSpec) == o->mRoomSpec);
        CHECK(G(n, F::Handled) == o->mHandled);
        if (R(2)) { o->mHandled = true; LobbyEvtSet(n, F::Handled, 1); CHECK(G(n, F::Handled) == o->mHandled); }
        break;
    }
    case K::ChatCommandError: { auto* o = (Orig::ChatCommandErrorEvent*)oe; CHECK(T(n, F::Text, o->mText)); break; }
    case K::CreateRoom: { auto* o = (Orig::CreateRoomEvent*)oe; CHECK(A(n, F::Address, o->mAddress)); break; }
    case K::JoinRoom: { auto* o = (Orig::JoinRoomEvent*)oe; CHECK(A(n, F::Address, o->mAddress)); CHECK((unsigned long)G(n, F::GroupId) == o->mGroupId); break; }
    case K::CreateAccount: {
        // LobbyNetCtrlHost.cpp's writes, then AccountLogic.cpp's reads
        auto* o = (Orig::CreateAccountEvent*)oe;
        GUIString u = Text(), p = Text(), m = Text();
        int d = Int(), mo = Int(), y = Int();
        bool nl = Bool();
        o->mUserName = u; o->mPassword = p; o->mEmail = m; o->mDay = d; o->mMonth = mo; o->mYear = y; o->mNewsletter = nl;
        LobbyEvtSetStr(n, F::UserName, (std::wstring)u); LobbyEvtSetStr(n, F::Password, (std::wstring)p);
        LobbyEvtSetStr(n, F::Email, (std::string)m); LobbyEvtSet(n, F::Day, (unsigned char)d);
        LobbyEvtSet(n, F::Month, (unsigned char)mo); LobbyEvtSet(n, F::Year, (unsigned short)y); LobbyEvtSet(n, F::Newsletter, nl);
        CHECK(Str<std::wstring>(n, F::UserName) == o->mUserName); CHECK(Str<std::wstring>(n, F::Password) == o->mPassword);
        CHECK(Str<std::string>(n, F::Email) == o->mEmail); CHECK((unsigned char)G(n, F::Day) == o->mDay);
        CHECK((unsigned char)G(n, F::Month) == o->mMonth); CHECK((unsigned short)G(n, F::Year) == o->mYear);
        CHECK((G(n, F::Newsletter) != 0) == o->mNewsletter);
        break;
    }
    case K::SyncGameList: { auto* o = (Orig::SyncGameListEvent*)oe; CHECK(G(n, F::Games) == (intptr_t)o->mGames.get()); break; }
    case K::JumpToRoom: {
        auto* o = (Orig::JumpToRoomEvent*)oe;
        CHECK(T(n, F::RoomName, o->mRoomName)); CHECK(G(n, F::GroupId) == o->mGroupId); CHECK(A(n, F::Address, o->mAddress));
        break;
    }
    case K::QuickPlay: { auto* o = (Orig::QuickPlayEvent*)oe; CHECK(G(n, F::GameType) == o->mGameType); break; }
    case K::SetCrossPromotion: {
        auto* o = (Orig::SetCrossPromotionEvent*)oe;
        CHECK(G(n, F::Image) == (intptr_t)o->mImage); CHECK(Str<std::string>(n, F::Link) == o->mLink);
        break;
    }
    case K::UpdateAccount: {
        auto* o = (Orig::UpdateAccountEvent*)oe;
        CHECK(T(n, F::Email, o->mEmail)); CHECK(T(n, F::NewPassword, o->mNewPassword)); CHECK(G(n, F::Newsletter) == o->mNewsletter);
        break;
    }
    case K::QueryServerGroups: { auto* o = (Orig::QueryServerGroupsEvent*)oe; CHECK(A(n, F::Address, o->mAddress)); break; }
    }
}

// the receivers' reads lobbyevt.rs answers for the game's objects (ctl_op, lobby_op, route_event_num) against the
// host cases they replaced, for every read of the event's class
static const char* const kCtlPrefix[] = {"Acl", "Af", "Tla", "Ptc", "Rc", "Jr", "Lr", "Cce", "Al", "Sci", "Gi", "Gja", "Cm",
                                         "Gc", "Sc", "Fc", "Lis", "Slf", "Qar", "Ghd"};
static const K kCtlKind[] = {K::AnnounceClientLocation, K::AnnounceFriend, K::QuickPlay, K::PlayerTeamChanged, K::RecvChat,
                             K::JoinedRoom, K::LeftRoom, K::ChatCommandError, K::Alert, K::SetChatInput, K::GroupInvitation,
                             K::GroupJoinAttempt, K::ClientModerated, K::GroupChanged, K::ServerChanged, K::FriendChanged,
                             K::LoginInitStatus, K::SetLoginFields, K::QueryAccountResult, K::GotHttpDocument};
// the EeWonLobbyCtlEvField values' prefixes, in order (lobbyctl.rs)
static const int kCtlFirst[] = {0, 5, 8, 9, 10, 18, 23, 26, 27, 31, 35, 42, 45, 52, 54, 55, 56, 60, 62, 64, 65};
static bool IsChange(K k)
{
    return k == K::AnnounceFriend || k == K::FriendChanged || k == K::GameChanged || k == K::PlayerChanged ||
           k == K::PlayerTeamChanged || k == K::ServerChanged || k == K::ChangeDirectConnect || k == K::ClientChanged ||
           k == K::GroupChanged;
}
static int sReads;
static void Reads(int ki, ComponentEvent* o, ComponentEvent* n)
{
    K k = (K)ki;
    intptr_t ne = (intptr_t)n;
    for (int p = 0; p < 20; p++) {
        if (kCtlKind[p] != k || p == 2) continue;
        for (int f = kCtlFirst[p]; f < kCtlFirst[p + 1]; f++) {
            sReads++;
            CHECK(ee_won_lobbyevt_ctl_op(EeWonLobbyCtlOp::EvGet, ne, f) == Orig::EvField(o, (EeWonLobbyCtlEvField)f));
        }
    }
    struct { EeWonLobbyCtlOp op; bool ok; } aCtl[] = {
        {EeWonLobbyCtlOp::EventFlags, k == K::SetRoomSpecFlags}, {EeWonLobbyCtlOp::EventRoomSpec, k == K::ClientChanged},
        {EeWonLobbyCtlOp::EventIsModify, IsChange(k)}, {EeWonLobbyCtlOp::EventIsAdd, IsChange(k)},
        {EeWonLobbyCtlOp::EventIsDelete, IsChange(k)}, {EeWonLobbyCtlOp::ClientChangedClient, k == K::ClientChanged},
        {EeWonLobbyCtlOp::PlayerChangedClient, k == K::PlayerChanged}, {EeWonLobbyCtlOp::EventGame, k == K::GameChanged},
        {EeWonLobbyCtlOp::EventGames, k == K::SyncGameList}};
    for (auto& c : aCtl)
        if (c.ok) { sReads++; CHECK(ee_won_lobbyevt_ctl_op(c.op, ne, 0) == Orig::CtlEvent(c.op, o)); }
    struct { EeWonLobbyOp op; bool ok; } aLogic[] = {
        {EeWonLobbyOp::EventDirectIsDelete, k == K::ChangeDirectConnect}, {EeWonLobbyOp::EventDirectIsAdd, k == K::ChangeDirectConnect},
        {EeWonLobbyOp::EventDirectGame, k == K::ChangeDirectConnect}, {EeWonLobbyOp::EventClientActionClient, k == K::ClientAction},
        {EeWonLobbyOp::EventClientActionRoomSpec, k == K::ClientAction}, {EeWonLobbyOp::EventClientActionAction, k == K::ClientAction},
        {EeWonLobbyOp::EventJoinGame, k == K::JoinGame}, {EeWonLobbyOp::EventActionGame, k == K::GameAction},
        {EeWonLobbyOp::EventAction, k == K::GameAction}};
    for (auto& c : aLogic)
        if (c.ok) { sReads++; CHECK(ee_won_lobbyevt_lobby_op(c.op, ne) == Orig::LogicEvent(c.op, o)); }
    K aNum[12] = {K::AcceptClient, K::AcceptClient, K::FriendAction, K::FriendAction, K::FriendAction, K::JumpToRoom,
                  K::QuickPlay, K::ClientAction, K::ClientAction, K::ClientAction, K::JoinRoom, K::JoinFriendsRoom};
    for (int i = 1; i <= 12; i++)
        if (aNum[i - 1] == k) { sReads++; CHECK(ee_won_lobbyevt_route_num(ne, i) == Orig::RouteNum(o, i)); }
    // the controls' EvSetHandled
    EeWonLobbyCtlEvField aHandled = k == K::AnnounceClientLocation ? EeWonLobbyCtlEvField::AclHandled
                                  : k == K::AnnounceFriend ? EeWonLobbyCtlEvField::AfHandled
                                  : EeWonLobbyCtlEvField::SciHandled;
    if ((k == K::AnnounceClientLocation || k == K::AnnounceFriend || k == K::SetChatInput) && R(2)) {
        CHECK(ee_won_lobbyevt_ctl_op(EeWonLobbyCtlOp::EvSetHandled, ne, (intptr_t)aHandled) == 0);
        if (k == K::AnnounceClientLocation) ((Orig::AnnounceClientLocationEvent*)o)->mHandled = true;
        else if (k == K::AnnounceFriend) ((Orig::AnnounceFriendEvent*)o)->mHandled = true;
        else ((Orig::SetChatInputEvent*)o)->mHandled = true;
        CHECK(ee_won_lobbyevt_ctl_op(EeWonLobbyCtlOp::EvGet, ne, (intptr_t)aHandled) == Orig::EvField(o, aHandled));
    }
    // not a Rust event: not answered
    CHECK(ee_won_lobbyevt_ctl_op(EeWonLobbyCtlOp::EvGet, (intptr_t)o, 1) == INTPTR_MIN);
}

int main(int argc, char** argv)
{
    int kSequences = argc > 1 ? atoi(argv[1]) : 60000;
    int aId = 1;
#define POOL(p) for (auto& x : p) { x.mId = aId++; x.mRefCount = 1; sAll.push_back(&x); }
    POOL(sClients) POOL(sFriends) POOL(sGames) POOL(sServers) POOL(sGroups) POOL(sLists) POOL(sGameLists) POOL(sOps)
    POOL(sTexts)
#undef POOL
    for (int k = 0; k < 6; k++) { sTexts[k].mText = L"t" + std::to_wstring((long long)k); if (k == 5) sTexts[k].mText.clear(); }
    int aKinds[43] = {0};
    for (sSeq = 0; sSeq < kSequences; sSeq++) {
        int k = (int)R(43);
        aKinds[k]++;
        Values v;
        Draw(v);
        std::vector<int> aStart = Counts();
        ComponentEvent* o = Make(false, k, v);
        std::vector<int> aOrig = Counts();
        ComponentEvent* n = Make(true, k, v);
        std::vector<int> aBoth = Counts();
        // the new event took what the original took
        for (size_t i = 0; i < aStart.size(); i++) CHECK(aBoth[i] - aOrig[i] == aOrig[i] - aStart[i]);
        Compare(k, o, n);
        Reads(k, o, n);
        bool aNewFirst = R(2) != 0;
        std::vector<int> aRelOrig, aRelNew;
        for (int side = 0; side < 2; side++) {
            bool aNew = (side == 0) == aNewFirst;
            sReleased.clear();
            delete (aNew ? n : o);
            (aNew ? aRelNew : aRelOrig) = sReleased;
        }
        CHECK(aRelOrig == aRelNew);
        CHECK(Counts() == aStart);
    }
    for (int k = 0; k < 43; k++) CHECK(aKinds[k] > 0);
    printf("won_lobbyevt_test: %d sequences over 43 classes, %d receiver reads, %d checks, %d failures\n", kSequences, sReads, sChecks, sFailures);
    return sFailures ? 1 : 0;
}
