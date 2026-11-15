// The lobby's data model as Rust objects (R6 WONLobby round 10): LobbyModel.cpp's classes, their statements copied
// from the C++ before the port (the containers WONLobbyMap.h's: ordered, a removed object released after the
// container let it go, in order; the decisions ee_won's model and staging functions, as the C++ called them) with
// every call on a C++ object outside the model (the factories, the game code's hooks, the routing records, the
// persistent ignore and friend lists, the connection's close, GUIStringData's construction) made the way
// LobbyModelHost.cpp makes it, against ee_won's modelobj.rs over stand-in shells (the model at the shell's third
// word, as the C++ asserts).
//
// Each of 100,000 sequences makes a few root objects of every class (clients, players, a client list, friends,
// rooms, games, an id and an address game list, a server, a server list, connections) and makes 1 to 40 random
// calls on the k-th live object of a class (every method and member the shells forward, the routing merges over
// random records, the iterators over random cursors), then dumps every live object's members and lets the roots go.
// Every object's and text's creation, deletion and final reference count, every hook and host op, every result
// and the dumps are traced; the traces must be the same. Host answers are functions of their arguments.
//
// The first 60 sequences and every later one that makes a call none before made are
// rust/crates/ee_won/src/golden_modelobj.txt (`--print`); the Rust tests replay its steps and compare the traces.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "rust/ee_won.h"

typedef EeWonModelCall MC;
typedef EeWonModelField MF;
typedef EeWonModelKind MK;

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0)
{
    char aBuffer[300];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d);
    sTrace.push_back(aBuffer);
}

static const int kText = 100, kConn = 50;

// ---- the routing records and addresses both sides read

struct CRec { int mName; unsigned long mFlags; unsigned short mId; unsigned long mUser, mKey; };
struct GRec {
    int mName; unsigned short mId, mCount; unsigned long mFlags; unsigned short mCaptain, mMax;
    std::vector<std::pair<unsigned short, unsigned char> > mMembers; // the member map, by id
};
static std::vector<CRec> sCRecs;
static std::vector<GRec> sGRecs;
static EeWonIpAddr* sAddrs[6];

// the deterministic answers
static bool Ignored(int inText) { return inText % 3 == 0; }
static bool Blocked(unsigned long inKey) { return inKey % 4 == 1; }
static bool Teammates(int a, int b) { return (a + b) % 2 == 0; }
static short JoinHook(int g) { return g % 3 == 0 ? 7 : 0; }
static bool CanStartHook(int g) { return g % 2 != 0; }

// ---- the original: LobbyModel.cpp's classes

namespace O {
static int sNextObj, sNextText;
struct Ref;
static std::map<int, Ref*> sObjs; // live objects by handle
struct Ref {
    int mRc, mHandle, mKind;
    Ref(int inKind) : mRc(0), mKind(inKind) { mHandle = inKind == kText ? sNextText++ : sNextObj++; if (inKind != kText) sObjs[mHandle] = this; }
    virtual ~Ref() { if (mKind != kText) sObjs.erase(mHandle); }
    void AddRef() { ++mRc; }
    void Release()
    {
        if (--mRc <= 0) {
            Line(mKind == kText ? "delete t%lld" : "delete h%lld", mHandle);
            delete this;
        }
    }
};
template <class T> struct P {
    T* p;
    P() : p(NULL) {}
    P(T* x) : p(x) { if (p) p->AddRef(); }
    P(const P& o) : p(o.p) { if (p) p->AddRef(); }
    ~P() { if (p) p->Release(); }
    P& operator=(T* x) { if (p != x) { if (p) p->Release(); p = x ? (x->AddRef(), x) : NULL; } return *this; }
    P& operator=(const P& o) { return *this = o.p; }
    T* get() const { return p; }
    T* operator->() const { return p; }
};
struct Text : Ref { int mValue; Text(int v) : Ref(kText), mValue(v) {} };
typedef P<Text> GS;
static Text* sEmpty;
struct Conn : Ref { Conn() : Ref(kConn) {} };

// an address value (IPAddr: the Rust value, copied at every copy)
struct Addr {
    EeWonIpAddr* v;
    Addr() : v(ee_won_ipaddr_new()) {}
    Addr(const Addr& o) : v(ee_won_ipaddr_clone(o.v)) {}
    Addr(const EeWonIpAddr* o) : v(ee_won_ipaddr_clone(o)) {}
    Addr& operator=(const Addr& o) { ee_won_ipaddr_assign(v, o.v); return *this; }
    ~Addr() { ee_won_ipaddr_free(v); }
    bool operator<(const Addr& o) const { return ee_won_ipaddr_compare(v, o.v) < 0; }
    bool operator==(const Addr& o) const { return ee_won_ipaddr_compare(v, o.v) == 0; }
};
static bool Less(unsigned short a, unsigned short b) { return a < b; }
static bool Less(const Addr& a, const Addr& b) { return a < b; }

// WONLobbyMap.h's LobbyRefMap: an ordered map of references
template <class K, class T> struct RefMap {
    std::vector<std::pair<K, T*> > v;
    ~RefMap() { this->clear(); }
    size_t size() const { return v.size(); }
    bool empty() const { return v.empty(); }
    size_t LowerBound(const K& k) const { size_t i = 0; while (i < v.size() && Less(v[i].first, k)) i++; return i; }
    size_t UpperBound(const K& k) const { size_t i = 0; while (i < v.size() && !Less(k, v[i].first)) i++; return i; }
    long Find(const K& k) const { size_t i = LowerBound(k); return i < v.size() && !Less(k, v[i].first) ? (long)i : -1; }
    size_t Insert(const K& k, bool* outNew = NULL)
    {
        size_t i = LowerBound(k);
        bool aNew = !(i < v.size() && !Less(k, v[i].first));
        if (aNew) v.insert(v.begin() + i, std::make_pair(k, (T*)NULL));
        if (outNew) *outNew = aNew;
        return i;
    }
    T* At(size_t i) const { return i < v.size() ? v[i].second : NULL; }
    K KeyAt(size_t i) const { return v[i].first; }
    T* Get(const K& k) const { long i = Find(k); return i >= 0 ? At(i) : NULL; }
    void SetAt(size_t i, T* o) { if (o) o->AddRef(); T* old = v[i].second; v[i].second = o; if (old) old->Release(); }
    size_t Set(const K& k, T* o) { size_t i = Insert(k); SetAt(i, o); return i; }
    void EraseAt(size_t i) { if (i >= v.size()) return; T* o = v[i].second; v.erase(v.begin() + i); if (o) o->Release(); }
    void EraseFrom(size_t i)
    {
        std::vector<T*> gone;
        for (size_t j = i; j < v.size(); j++) gone.push_back(v[j].second);
        if (i < v.size()) v.erase(v.begin() + i, v.end());
        for (size_t j = 0; j < gone.size(); j++) if (gone[j]) gone[j]->Release();
    }
    void clear() { EraseFrom(0); }
    P<T> Remove(const K& k) { long i = Find(k); if (i < 0) return P<T>(); P<T> o = At(i); EraseAt(i); return o; }
};
template <class M, class K> struct Cursor {
    bool mStarted; K mLast;
    Cursor() : mStarted(false) {}
    void Rewind() { mStarted = false; }
    size_t Peek(const M& m) const { return mStarted ? m.UpperBound(mLast) : 0; }
    bool HasMore(const M& m) const { return Peek(m) < m.size(); }
    size_t Next(const M& m) { size_t i = Peek(m); if (i < m.size()) { mLast = m.KeyAt(i); mStarted = true; } return i; }
};

static int Handle(const Ref* r) { return r ? r->mHandle : -1; }

struct Player : Ref {
    unsigned short mPing; bool mIsReady;
    Player() : Ref((int)MK::Player) { mPing = 0xfffe; mIsReady = false; }
    virtual bool IsTeammate(Player* o) { Line("op teammate h%lld h%lld", mHandle, Handle(o)); return Teammates(mHandle, Handle(o)); }
    bool IsSameTeam(Player* o) { if (this == o) return true; if (o == NULL) return false; return this->IsTeammate(o); }
};
struct Client;
struct ClientList;
static bool LobbyIsNameIgnored(const GS& t) { Line("op ignored t%lld", t->mHandle); return Ignored(t->mValue); }
static bool LobbyIsCDKeyBlocked(unsigned long k) { Line("op blocked %lld", k); return Blocked(k); }

struct Client : Ref {
    GS mName; unsigned short mClientId; unsigned long mClientFlags, mWONUserId, mCDKeyId;
    unsigned char mChatMemberFlags, mGameMemberFlags; unsigned short mChatGroupId, mGameGroupId;
    bool mIgnored, mBlocked, mIsFriend, mIsGameCaptain, mIsChatCaptain, mInvited; P<Player> mPlayer;
    Client() : Ref((int)MK::Client), mName(sEmpty)
    {
        mClientId = 0xffff; mChatGroupId = 0xffff; mGameGroupId = 0xffff; mClientFlags = 0; mWONUserId = 0; mCDKeyId = 0;
        mChatMemberFlags = 0; mGameMemberFlags = 0; mIgnored = false; mBlocked = false; mIsFriend = false;
        mIsGameCaptain = false; mIsChatCaptain = false; mInvited = false;
    }
    void ClearGroup(unsigned short inId, bool inGame)
    {
        if (inGame) {
            if (inId == mGameGroupId) { mGameGroupId = 0xffff; mGameMemberFlags = 0; mIsGameCaptain = false; }
        } else if (inId == mChatGroupId) { mChatGroupId = 0xffff; mChatMemberFlags = 0; mIsChatCaptain = false; }
    }
    bool IsInChatGroup() { return mChatGroupId != 0xffff; }
    bool IsInGameGroup() { return mGameGroupId != 0xffff; }
    void ReadRoutingInfo(const CRec* inInfo)
    {
        this->mName = GS(new Text(inInfo->mName)); // GUIString(new GUIStringData(inInfo->mName))
        mClientFlags = inInfo->mFlags; mClientId = inInfo->mId; mWONUserId = inInfo->mUser; mCDKeyId = inInfo->mKey;
    }
    void UpdateIgnoreStatus() { mIgnored = LobbyIsNameIgnored(mName); mBlocked = LobbyIsCDKeyBlocked(mCDKeyId); }
    EeWonClientStatus St() { return ee_won_model_client_status((uint32_t)mClientFlags, mChatMemberFlags, mGameMemberFlags); }
    bool IsAnonymous() { return St().anonymous; }
    bool IsMuted(bool g) { EeWonClientStatus s = St(); return g ? s.muted_game : s.muted_chat; }
    bool IsAway() { return ee_won_model_client_status((uint32_t)mClientFlags, 0, 0).away; }
    bool IsAdmin() { return ee_won_model_client_status((uint32_t)mClientFlags, 0, 0).admin; }
    bool IsModerator() { return ee_won_model_client_status((uint32_t)mClientFlags, 0, 0).moderator; }
    bool IsCaptain(bool g) { return g ? mIsGameCaptain : mIsChatCaptain; }
    void SetCaptain(bool c, bool g) { if (g) mIsGameCaptain = c; else mIsChatCaptain = c; }
    void SetPlayer(Player* p) { mPlayer = p; }
    bool HasPlayer() { return mPlayer.get() != NULL; }
    bool IsReady() { return mPlayer.get() != NULL ? mPlayer->mIsReady : false; }
    unsigned short GetPing() { if (mPlayer.get() != NULL) return mPlayer->mPing; return 0xfffe; }
    bool IsTeammate(Client* o)
    {
        if (o == NULL || mPlayer.get() == NULL || o->mPlayer.get() == NULL) return false;
        return mPlayer->IsSameTeam(o->mPlayer.get());
    }
    void SetGroupId(unsigned short id, bool g) { if (g) mGameGroupId = id; else mChatGroupId = id; }
    void SetMemberFlags(unsigned char f, bool g) { if (g) mGameMemberFlags = f; else mChatMemberFlags = f; }
};

static std::vector<EeWonSyncStep> Plan(const std::vector<uint16_t>& e, const std::vector<uint16_t>& in)
{
    std::vector<EeWonSyncStep> aPlan(e.size() + in.size() + 1);
    size_t n = ee_won_model_sync_plan(e.empty() ? NULL : &e[0], e.size(), in.empty() ? NULL : &in[0], in.size(), &aPlan[0]);
    aPlan.resize(n);
    return aPlan;
}

struct ClientList : Ref {
    RefMap<unsigned short, Client> mClients;
    ClientList() : Ref((int)MK::ClientList) {}
    void Clear() { mClients.clear(); }
    void SyncClients(const std::vector<std::pair<unsigned short, const CRec*> >& inMap)
    {
        std::vector<uint16_t> e, in;
        for (size_t i = 0; i < mClients.size(); ++i) e.push_back(mClients.KeyAt(i));
        for (size_t i = 0; i < inMap.size(); ++i) in.push_back(inMap[i].first);
        std::vector<EeWonSyncStep> aPlan = Plan(e, in);
        size_t aClient = 0;
        for (size_t k = 0; k < aPlan.size(); ++k) {
            switch (aPlan[k].kind) {
            case 0: mClients.EraseAt(aClient); break;
            case 1: {
                P<Client> aNewClient = new Client;
                aNewClient->ReadRoutingInfo(inMap[aPlan[k].incoming].second);
                bool aNew = false;
                size_t anAt = mClients.Insert(inMap[aPlan[k].incoming].first, &aNew);
                if (aNew) { mClients.SetAt(anAt, aNewClient.get()); if (anAt <= aClient) ++aClient; }
                break;
            }
            case 2: { P<Client> aCopy = mClients.At(aClient); aCopy->ReadRoutingInfo(inMap[aPlan[k].incoming].second); ++aClient; break; }
            case 3: mClients.EraseFrom(aClient); aClient = mClients.size(); break;
            }
        }
    }
    void UpdateIgnoreStatus() { for (size_t i = 0; i < mClients.size(); ++i) mClients.At(i)->UpdateIgnoreStatus(); }
    void AddClient(Client* c) { unsigned short id = c->mClientId; mClients.Set(id, c); }
    Client* FindClient(unsigned short id) { return mClients.Get(id); }
    P<Client> RemoveClient(unsigned short id) { return mClients.Remove(id); }
    Client* FindClientByWONId(unsigned long id)
    {
        for (size_t i = 0; i < mClients.size(); ++i) { Client* c = mClients.At(i); if (c->mWONUserId == id) return c; }
        return NULL;
    }
    void ClearInvited() { for (size_t i = 0; i < mClients.size(); ++i) mClients.At(i)->mInvited = false; }
    size_t GetCount() const { return mClients.size(); }
    Client* ClientAt(size_t i) const { return mClients.At(i); }
};
typedef Cursor<RefMap<unsigned short, Client>, unsigned short> ClientCursor;
// LobbyClientIterator / LobbyClientSetIterator over a list (NULL: none)
struct Iter {
    ClientList* mList; ClientCursor mItr; bool mReady;
    bool HasMore() { return mList != NULL && mItr.HasMore(mList->mClients); }
    Client* Next() { if (mList == NULL) return NULL; size_t i = mItr.Next(mList->mClients); return i < mList->mClients.size() ? mList->mClients.At(i) : NULL; }
    bool SetHasMore()
    {
        if (mList == NULL) return false;
        if (mReady) return true;
        size_t i;
        while ((i = mItr.Peek(mList->mClients)) < mList->mClients.size()) {
            if (mList->mClients.At(i)->mPlayer.get() != NULL) { mReady = true; return true; }
            mItr.Next(mList->mClients);
        }
        return false;
    }
    Player* SetNext(Client** outClient)
    {
        if (mList == NULL) return NULL;
        if (SetHasMore()) {
            Client* c = mList->mClients.At(mItr.Next(mList->mClients));
            if (outClient != NULL) *outClient = c;
            Player* p = c->mPlayer.get();
            mReady = false;
            return p;
        }
        return NULL;
    }
    void Rewind() { mItr.Rewind(); mReady = false; }
};

struct Friend : Ref {
    GS mName; unsigned long mUserId; Addr mAddress; GS mLocation; unsigned long mLastSearch; unsigned char mFlags;
    unsigned long mLastLocate, mSearchStart; bool mAnonymous;
    Friend() : Ref((int)MK::Friend), mName(sEmpty), mLocation(sEmpty)
    { mUserId = 0; mFlags = 0; mLastSearch = 0; mLastLocate = 0; mSearchStart = 0; mAnonymous = false; }
    void SetLocation(const Addr& a, Text* l, int f) { mAddress = a; mLocation = l; mFlags = f; }
    void ClearLocation() { mAddress = Addr(); mLocation = sEmpty; mFlags = 0; mLastLocate = 0; mAnonymous = false; }
    void SetStatusFromClient(Client* c) { mFlags = ee_won_model_friend_status(c->IsInChatGroup(), c->IsInGameGroup()); mAnonymous = c->IsAnonymous(); }
    void StartLocate(unsigned long t) { mFlags = ee_won_model_friend_start_locate(); mSearchStart = t; }
    void EndLocate(unsigned long t) { mFlags = ee_won_model_friend_end_locate(mFlags); mLastLocate = t; }
};
static std::vector<int> sFriendRoots; // the persistent friend list: the root friends' handles
static Friend* LobbyFindFriend(unsigned long id)
{
    Line("op findfriend %lld", id);
    for (size_t i = 0; i < sFriendRoots.size(); i++) {
        std::map<int, Ref*>::iterator it = sObjs.find(sFriendRoots[i]);
        if (it != sObjs.end() && static_cast<Friend*>(it->second)->mUserId == id) return static_cast<Friend*>(it->second);
    }
    return NULL;
}

struct Group : Ref {
    GS mName; unsigned short mId, mCaptainId; unsigned long mNumClients, mMaxClients, mFlags; P<ClientList> mClients;
    Group() : Ref((int)MK::Group), mName(sEmpty)
    { mNumClients = 0; mMaxClients = 0; mFlags = 0; mId = 0xffff; mCaptainId = 0xffff; mClients = new ClientList; }
    EeWonGroupKind K() { return ee_won_model_group_kind((uint32_t)mFlags); }
    bool IsGameGroup() { return !K().chat; }
    void ReadRoutingInfo(const GRec* inInfo, ClientList* inClients)
    {
        mId = inInfo->mId;
        mName = GS(new Text(inInfo->mName));
        mNumClients = inInfo->mCount; mFlags = inInfo->mFlags; mCaptainId = inInfo->mCaptain; mMaxClients = inInfo->mMax;
        if (inClients != NULL) SetMembers(inInfo->mMembers, inClients);
    }
    void SetMembers(const std::vector<std::pair<unsigned short, unsigned char> >& m, ClientList* inClients)
    {
        mClients->Clear();
        for (size_t k = 0; k < m.size(); k++) {
            Client* client = inClients->FindClient(m[k].first);
            if (client != NULL) {
                client->SetMemberFlags(m[k].second, IsGameGroup());
                client->SetCaptain(client->mClientId == mCaptainId, IsGameGroup());
                client->SetGroupId(mId, IsGameGroup());
                mClients->AddClient(client);
            }
        }
        mNumClients = mClients->GetCount();
    }
    void AddClient(Client* c, unsigned char f)
    {
        mClients->AddClient(c);
        c->SetGroupId(mId, IsGameGroup());
        c->SetMemberFlags(f, IsGameGroup());
        c->SetCaptain(c->mClientId == mCaptainId, IsGameGroup());
        mNumClients = mClients->GetCount();
    }
    P<Client> RemoveClient(unsigned short id)
    {
        P<Client> c = mClients->RemoveClient(id);
        mNumClients = mClients->GetCount();
        if (c.get() != NULL) c->ClearGroup(mId, IsGameGroup());
        return c;
    }
    Client* GetCaptain() { if (mCaptainId != 0xffff) return mClients->FindClient(mCaptainId); return NULL; }
    Client* SetCaptain(unsigned short id, Client** outOld)
    {
        Client* o = GetCaptain();
        if (o != NULL) o->SetCaptain(false, IsGameGroup());
        Client* n = NULL;
        if (id != 0xffff) { n = mClients->FindClient(id); if (n != NULL) n->SetCaptain(true, IsGameGroup()); }
        if (outOld != NULL) *outOld = o;
        mCaptainId = id;
        return n;
    }
};

struct Game : Ref {
    GS mName, mPassword; bool mHasPassword, mInviteOnly, mAskToJoin, mInProgress, mInvited, mIsHost, mShowDetails,
        mHaveDetails, mPingReplied;
    long mLastSeenTime; bool mIsFiltered; long mNumPlayers, mNumOpenSlots, mGameType, mSkill; P<ClientList> mClients;
    Addr mAddress; unsigned short mCaptainId, mGroupId, mPing;
    Game() : Ref((int)MK::Game), mName(sEmpty), mPassword(sEmpty)
    {
        mCaptainId = mGroupId = 0xffff; mGameType = 0; mInviteOnly = false; mAskToJoin = false; mInProgress = false;
        mHasPassword = false; mInvited = false; mIsHost = false; mShowDetails = false; mHaveDetails = false;
        mPingReplied = false; mSkill = 0; mNumPlayers = 0; mNumOpenSlots = 0; mPing = 0xfffe; mLastSeenTime = 0;
        mIsFiltered = false;
    }
    void CopyFromHook(Game* o) { Line("op copyhook h%lld h%lld", mHandle, Handle(o)); }
    void HandleHostGameHook(Client* c) { Line("op hosthook h%lld h%lld", mHandle, Handle(c)); }
    void HandlePlayerLeftHook(Client* c, Player* p) { Line("op lefthook h%lld h%lld h%lld", mHandle, Handle(c), Handle(p)); }
    short GetJoinStatusHook() { Line("op joinhook h%lld", mHandle); return JoinHook(mHandle); }
    bool CanStartGameHook() { Line("op starthook h%lld", mHandle); return CanStartHook(mHandle); }
    void CopyFrom(Game* g)
    {
        mName = g->mName; mPassword = g->mPassword; mHasPassword = g->mHasPassword; mInviteOnly = g->mInviteOnly;
        mAskToJoin = g->mAskToJoin; mInProgress = g->mInProgress; mInvited = g->mInvited; mIsHost = g->mIsHost;
        mShowDetails = g->mShowDetails; mHaveDetails = g->mHaveDetails; mPingReplied = g->mPingReplied;
        mLastSeenTime = g->mLastSeenTime; mNumPlayers = g->mNumPlayers; mNumOpenSlots = g->mNumOpenSlots;
        mGameType = g->mGameType; mSkill = g->mSkill; mClients = g->mClients.get(); mAddress = g->mAddress;
        mCaptainId = g->mCaptainId; mGroupId = g->mGroupId; mPing = g->mPing;
        CopyFromHook(g);
    }
    Client* FindClient(unsigned short id) { if (mClients.get() != NULL) return mClients->FindClient(id); return NULL; }
    bool IsFull() { return ee_won_game_is_full((int32_t)mNumPlayers, (int32_t)mNumOpenSlots); }
    short GetJoinStatus()
    {
        short s = ee_won_game_join_status(IsFull(), mInviteOnly, mInvited, mIsHost, mInProgress);
        return s >= 0 ? s : GetJoinStatusHook();
    }
    void RemoveClient(Client* c)
    {
        P<Player> aPlayer(c->mPlayer);
        if (aPlayer.get() == NULL) return;
        --mNumPlayers;
        c->SetPlayer(NULL);
        HandlePlayerLeftHook(c, aPlayer.get());
    }
    void ClearPlayers()
    {
        ClientList* cl = mClients.get();
        if (cl == NULL) return;
        for (size_t i = 0; i < cl->GetCount(); ++i) cl->ClientAt(i)->SetPlayer(NULL);
    }
    bool IsSameGame(Game* o)
    {
        return ee_won_model_same_game((int32_t)mGameType, (int32_t)o->mGameType, mGroupId, o->mGroupId, o->mAddress == mAddress);
    }
    void ReadGroup(Group* g)
    {
        mName = g->mName; mHasPassword = g->K().password; mInviteOnly = g->K().invite_only; mAskToJoin = g->K().ask_to_join;
        mGroupId = g->mId; mCaptainId = g->mCaptainId; mClients = g->mClients;
    }
    void HandleHostGame(Client* c)
    {
        P<Player> player = new Player; // LobbyPlayer::CreatePlayer()
        c->SetPlayer(player.get());
        HandleHostGameHook(c);
        mNumPlayers = 1;
    }
    bool AreAllReady()
    {
        ClientList* cl = mClients.get();
        std::vector<EeWonMemberReady> m;
        for (size_t i = 0; i < cl->GetCount(); ++i) {
            Client* c = cl->ClientAt(i);
            EeWonMemberReady r; r.has_player = c->HasPlayer(); r.game_captain = c->IsCaptain(1); r.ready = c->IsReady();
            m.push_back(r);
        }
        return ee_won_model_all_ready(m.empty() ? NULL : &m[0], m.size());
    }
    bool CanStartGame() { if (!AreAllReady()) return false; return CanStartGameHook(); }
    // ReadSummary's stores (the name a new GUIStringData when read)
    void StoreSummary(const EeWonGameSummary& s, int inName)
    {
        if (s.read & 1) ee_won_ipaddr_set_six(mAddress.v, s.address);
        if (s.read & 2) mInProgress = s.in_progress;
        if (s.read & 4) mName = GS(new Text(inName));
        if (s.read & 8) mSkill = s.skill;
        if (s.read & 0x10) { mHasPassword = s.has_password; mInviteOnly = s.invite_only; mAskToJoin = s.ask_to_join; }
        if (s.read & 0x20) mNumPlayers = s.players;
        if (s.read & 0x40) mNumOpenSlots = s.open_slots;
    }
};
static Game* CreateGame(long t) { Game* g = new Game; g->mGameType = t; return g; }

struct GameList : Ref {
    long mType; RefMap<unsigned short, Game> mIdGames; RefMap<Addr, Game> mAddrGames;
    Cursor<RefMap<unsigned short, Game>, unsigned short> mIdItr; Cursor<RefMap<Addr, Game>, Addr> mAddrItr;
    GameList(int t) : Ref((int)MK::GameList) { mType = t; }
    bool ById() { return ee_won_model_game_list_by_id(mType); }
    void Clear() { mIdGames.clear(); mAddrGames.clear(); }
    void AddGame(Game* g)
    {
        g->mGameType = mType;
        if (ById()) { unsigned short id = g->mGroupId; mIdGames.Set(id, g); }
        else mAddrGames.Set(g->mAddress, g);
    }
    Game* GetOrCreateGame(unsigned short id, bool* out)
    {
        size_t it = mIdGames.Insert(id);
        Game* g = mIdGames.At(it);
        if (g == NULL) { g = CreateGame(mType); mIdGames.SetAt(it, g); g->mGroupId = id; if (out) *out = false; }
        else if (out) *out = true;
        return g;
    }
    Game* GetOrCreateGame(const Addr& a, bool* out)
    {
        size_t it = mAddrGames.Insert(a);
        Game* g = mAddrGames.At(it);
        if (g == NULL) { g = CreateGame(mType); mAddrGames.SetAt(it, g); g->mAddress = a; if (out) *out = false; }
        else if (out) *out = true;
        return g;
    }
    P<Game> RemoveGame(unsigned short id) { P<Game> g = mIdGames.Remove(id); if (g.get()) g->mPing = 0xfffe; return g; }
    P<Game> RemoveGame(const Addr& a) { P<Game> g = mAddrGames.Remove(a); if (g.get()) g->mPing = 0xfffe; return g; }
    void Rewind() { if (ById()) mIdItr.Rewind(); else mAddrItr.Rewind(); }
    bool HasMore() { if (ById()) return mIdItr.HasMore(mIdGames); return mAddrItr.HasMore(mAddrGames); }
    Game* Next() { if (ById()) return mIdGames.At(mIdItr.Next(mIdGames)); return mAddrGames.At(mAddrItr.Next(mAddrGames)); }
    bool IsEmpty() { if (ById()) return mIdGames.empty(); return mAddrGames.empty(); }
    bool RemoveStaleGames(long inMaxAge, long long now)
    {
        if (ById()) return false;
        unsigned long oldSize = mAddrGames.size();
        size_t it = 0;
        while (it < mAddrGames.size()) {
            Game* g = mAddrGames.At(it);
            if (ee_won_model_game_stale((int64_t)now, (int32_t)g->mLastSeenTime, (int32_t)inMaxAge)) { g->mPing = 0xfffe; mAddrGames.EraseAt(it); }
            else ++it;
        }
        return mAddrGames.size() < oldSize;
    }
};

static int UpperCmp(const std::string& a, const std::string& b)
{
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        int x = toupper((unsigned char)a[i]), y = toupper((unsigned char)b[i]);
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}
struct NoCase { bool operator()(const std::string& a, const std::string& b) const { return UpperCmp(a, b) < 0; } };

struct Server : Ref {
    GS mName; std::set<std::string, NoCase> mAddresses; Addr mAddress; int mNumChatClients, mNumClients;
    unsigned long mLastPing; bool mQueryingGroups; RefMap<unsigned short, Group> mGroups; P<ClientList> mClients;
    P<Conn> mConnection; unsigned short mMyClientId;
    Server() : Ref((int)MK::Server), mName(sEmpty)
    {
        mNumChatClients = 0; mNumClients = 0; mLastPing = 0; mQueryingGroups = false; mMyClientId = 0xffff;
        mClients = new ClientList;
    }
    void UpdateFriends()
    {
        for (size_t i = 0; i < mClients->GetCount(); ++i) {
            Client* c = mClients->ClientAt(i);
            Friend* f = LobbyFindFriend(c->mWONUserId);
            if (f != NULL) { c->mIsFriend = true; f->SetLocation(mAddress, mName.get(), 0); f->SetStatusFromClient(c); }
            else c->mIsFriend = false;
        }
    }
    void ClearFriendLocations(long long now)
    {
        for (size_t i = 0; i < mClients->GetCount(); ++i) {
            Client* c = mClients->ClientAt(i);
            if (c->mIsFriend) {
                Friend* f = LobbyFindFriend(c->mWONUserId);
                if (f != NULL) { if (c->IsAnonymous()) f->ClearLocation(); else f->mLastLocate = (unsigned long)now; }
            }
        }
    }
    void SyncGroups(const std::vector<std::pair<unsigned short, const GRec*> >& inGroups, bool inUpdate)
    {
        EeWonPopulation pop; memset(&pop, 0, sizeof(pop));
        ClientList* clients = inUpdate ? mClients.get() : NULL;
        std::vector<uint16_t> e, in;
        for (size_t i = 0; i < mGroups.size(); ++i) e.push_back(mGroups.KeyAt(i));
        for (size_t i = 0; i < inGroups.size(); ++i) in.push_back(inGroups[i].first);
        std::vector<EeWonSyncStep> aPlan = Plan(e, in);
        size_t groupIt = 0;
        for (size_t k = 0; k < aPlan.size(); ++k) {
            switch (aPlan[k].kind) {
            case 0: mGroups.EraseAt(groupIt); break;
            case 1: {
                P<Group> g = new Group;
                g->ReadRoutingInfo(inGroups[aPlan[k].incoming].second, clients);
                bool aNew = false;
                size_t at = mGroups.Insert(inGroups[aPlan[k].incoming].first, &aNew);
                if (aNew) { mGroups.SetAt(at, g.get()); if (at <= groupIt) ++groupIt; }
                ee_won_model_population(&pop, 0, (uint32_t)g->mFlags, (uint32_t)g->mNumClients);
                break;
            }
            case 2: {
                P<Group> g = mGroups.At(groupIt);
                g->ReadRoutingInfo(inGroups[aPlan[k].incoming].second, clients);
                ee_won_model_population(&pop, 0, (uint32_t)g->mFlags, (uint32_t)g->mNumClients);
                ++groupIt;
                break;
            }
            case 3: mGroups.EraseFrom(groupIt); groupIt = mGroups.size(); break;
            }
        }
        mNumChatClients = pop.chat;
        mNumClients = pop.total;
    }
    Client* AddClient(const CRec* r)
    {
        P<Client> c = new Client;
        c->ReadRoutingInfo(r);
        c->UpdateIgnoreStatus();
        mClients->AddClient(c.get());
        return c.get();
    }
    void AddClientToGroup(Client* c, unsigned char f, Group* g)
    {
        g->AddClient(c, f);
        EeWonPopulation pop = {mNumChatClients, mNumClients};
        ee_won_model_population(&pop, 1, (uint32_t)g->mFlags, 0);
        mNumChatClients = pop.chat;
    }
    Group* AddGroup(const GRec* r)
    {
        P<Group> g = new Group;
        g->ReadRoutingInfo(r, mClients.get());
        mGroups.Set(g->mId, g.get());
        return g.get();
    }
    P<Client> RemoveClientFromGroup(unsigned short id, Group* g)
    {
        P<Client> c = g->RemoveClient(id);
        if (c.get() != NULL) {
            EeWonPopulation pop = {mNumChatClients, mNumClients};
            ee_won_model_population(&pop, 2, (uint32_t)g->mFlags, 0);
            mNumChatClients = pop.chat;
        }
        return c;
    }
    bool IsConnected() { return mConnection.get() != NULL; }
    void Disconnect(long long now)
    {
        if (mConnection.get() != NULL) {
            ClearFriendLocations(now);
            mNumClients = mClients->GetCount();
            Line("op close h%lld", mConnection->mHandle);
            mConnection = NULL;
        }
        mMyClientId = 0xffff;
        mClients->Clear();
    }
};
struct ServerList : Ref {
    RefMap<Addr, Server> mServers;
    ServerList() : Ref((int)MK::ServerList) {}
    Server* Insert(const Addr& a)
    {
        size_t i = mServers.Insert(a);
        if (mServers.At(i) == NULL) mServers.SetAt(i, new Server);
        return mServers.At(i);
    }
};
}

// ---- the Rust side: stand-in shells, the host LobbyModelHost.cpp is

namespace RS {
struct Ref { intptr_t mVt; int32_t mRc; int32_t mHandle; EeWonModel* mModel; int mKind; int mValue; };
static int sNextObj, sNextText;
static std::map<int, Ref*> sObjs;
static Ref* sEmpty;
static intptr_t Host(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c);
static Ref* New(int inKind, int inValue = 0)
{
    Ref* r = new Ref;
    r->mVt = 0; r->mRc = 0; r->mKind = inKind; r->mValue = inValue; r->mModel = NULL;
    r->mHandle = inKind == kText ? sNextText++ : sNextObj++;
    if (inKind != kText) sObjs[r->mHandle] = r;
    if (inKind != kText && inKind != kConn)
        r->mModel = ee_won_model_new(r, (EeWonModelKind)inKind, Host, sEmpty);
    return r;
}
static void AddRef(Ref* r) { r->mRc++; }
static void Release(Ref* r)
{
    if (--r->mRc <= 0) {
        Line(r->mKind == kText ? "delete t%lld" : "delete h%lld", r->mHandle);
        if (r->mKind != kText) sObjs.erase(r->mHandle);
        if (r->mModel != NULL) ee_won_model_free(r->mModel);
        delete r;
    }
}
static int Handle(intptr_t p) { return p ? reinterpret_cast<Ref*>(p)->mHandle : -1; }
static intptr_t Call(Ref* r, MC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0)
{
    return ee_won_model_call(r->mModel, c, a, b, cc, (intptr_t)sEmpty);
}
static intptr_t Held(Ref* r) { r->mRc++; return (intptr_t)r; }
static std::vector<int> sFriendRoots;
static intptr_t Host(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonModelOp Op;
    Ref* ra = reinterpret_cast<Ref*>(a);
    switch (op) {
    case Op::AddRef: AddRef(ra); return 0;
    case Op::Release: Release(ra); return 0;
    case Op::NewClient: return Held(New((int)MK::Client));
    case Op::NewClientList: return Held(New((int)MK::ClientList));
    case Op::NewGroup: return Held(New((int)MK::Group));
    case Op::NewServer: return Held(New((int)MK::Server));
    case Op::NewGame: { Ref* g = New((int)MK::Game); ee_won_model_set(g->mModel, MF::GameType, a); return Held(g); }
    case Op::NewPlayer: return Held(New((int)MK::Player));
    case Op::ClientReadRouting: {
        const CRec* r = reinterpret_cast<const CRec*>(b);
        intptr_t v[4] = {(intptr_t)r->mFlags, r->mId, (intptr_t)r->mUser, (intptr_t)r->mKey};
        Ref* t = New(kText, r->mName);
        AddRef(t);
        Call(ra, MC::ClientSetRouting, (intptr_t)t, (intptr_t)v);
        Release(t);
        return 0;
    }
    case Op::GroupReadRouting: {
        const GRec* r = reinterpret_cast<const GRec*>(b);
        intptr_t v[5] = {r->mId, r->mCount, (intptr_t)r->mFlags, r->mCaptain, r->mMax};
        Ref* t = New(kText, r->mName);
        AddRef(t);
        Call(ra, MC::GroupSetRouting, (intptr_t)t, (intptr_t)v);
        Release(t);
        if (c != 0) {
            std::vector<uint16_t> ids;
            std::vector<uint8_t> flags;
            for (size_t i = 0; i < r->mMembers.size(); i++) { ids.push_back(r->mMembers[i].first); flags.push_back(r->mMembers[i].second); }
            Call(ra, MC::GroupSetMembersList, c);
            Call(ra, MC::GroupSetMembers, ids.empty() ? 0 : (intptr_t)&ids[0], flags.empty() ? 0 : (intptr_t)&flags[0], ids.size());
        }
        return 0;
    }
    case Op::NameIgnored: Line("op ignored t%lld", ra->mHandle); return Ignored(ra->mValue);
    case Op::CdKeyBlocked: Line("op blocked %lld", (long long)(unsigned long)a); return Blocked((unsigned long)a);
    case Op::FindFriend: {
        Line("op findfriend %lld", (long long)(unsigned long)a);
        for (size_t i = 0; i < sFriendRoots.size(); i++) {
            std::map<int, Ref*>::iterator it = sObjs.find(sFriendRoots[i]);
            if (it != sObjs.end() && (unsigned long)ee_won_model_get(it->second->mModel, MF::FriendUserId) == (unsigned long)a)
                return (intptr_t)it->second;
        }
        return 0;
    }
    case Op::PlayerIsTeammate: Line("op teammate h%lld h%lld", ra->mHandle, Handle(b)); return Teammates(ra->mHandle, Handle(b));
    case Op::GameCopyFromHook: Line("op copyhook h%lld h%lld", ra->mHandle, Handle(b)); return 0;
    case Op::GameHostHook: Line("op hosthook h%lld h%lld", ra->mHandle, Handle(b)); return 0;
    case Op::GamePlayerLeftHook: Line("op lefthook h%lld h%lld h%lld", ra->mHandle, Handle(b), Handle(c)); return 0;
    case Op::GameJoinStatusHook: Line("op joinhook h%lld", ra->mHandle); return JoinHook(ra->mHandle);
    case Op::GameCanStartHook: Line("op starthook h%lld", ra->mHandle); return CanStartHook(ra->mHandle);
    case Op::ConnectionClose: Line("op close h%lld", ra->mHandle); return 0;
    }
    return 0;
}
}

// ---- the steps

// a step: a call `mCall` (an EeWonModelCall, or a member get/set, or a root's creation or release) on the k-th live
// object of a kind, with arguments (numbers, the k-th live object of a kind, a record or an address)
enum StepKind { kNew, kRelease, kCall, kGet, kSet, kSetText, kSetObj, kSetAddr, kIter };
struct Step { int mWhat; int mKind; int mK; int mCall; long long a, b, c; int mArgKind, mArgK; std::vector<int> mRecs; };

static std::string Addr6(const EeWonIpAddr* v)
{
    char s[40];
    snprintf(s, sizeof(s), "a%u.%ld", (unsigned)ee_won_ipaddr_port(v), (long)ee_won_ipaddr_host(v));
    return s;
}

// the k-th live object of a kind (by handle), or none
template <class M> static int Nth(M& inObjs, int inKind, int inK, bool inRust)
{
    int n = 0;
    for (typename M::iterator it = inObjs.begin(); it != inObjs.end(); ++it) {
        int kind = inRust ? reinterpret_cast<RS::Ref*>(it->second)->mKind : reinterpret_cast<O::Ref*>(it->second)->mKind;
        if (kind == inKind && n++ == inK) return it->first;
    }
    return -1;
}

// each class's members, dumped
static const MF kFields[][24] = {
    {MF::PlayerPing, MF::PlayerReady},
    {MF::ClientName, MF::ClientId, MF::ClientFlags, MF::ClientWonUserId, MF::ClientCdKeyId, MF::ClientChatMemberFlags,
     MF::ClientGameMemberFlags, MF::ClientChatGroupId, MF::ClientGameGroupId, MF::ClientIgnored, MF::ClientBlocked,
     MF::ClientIsFriend, MF::ClientGameCaptain, MF::ClientChatCaptain, MF::ClientInvited, MF::ClientPlayer},
    {},
    {MF::FriendName, MF::FriendUserId, MF::FriendAddress, MF::FriendLocation, MF::FriendLastSearch, MF::FriendFlags,
     MF::FriendLastLocate, MF::FriendSearchStart, MF::FriendAnonymous},
    {MF::GroupName, MF::GroupId, MF::GroupCaptainId, MF::GroupNumClients, MF::GroupMaxClients, MF::GroupFlags, MF::GroupClients},
    {MF::GameName, MF::GamePassword, MF::GameHasPassword, MF::GameInviteOnly, MF::GameAskToJoin, MF::GameInProgress,
     MF::GameInvited, MF::GameIsHost, MF::GameShowDetails, MF::GameHaveDetails, MF::GamePingReplied, MF::GameLastSeen,
     MF::GameFiltered, MF::GameNumPlayers, MF::GameOpenSlots, MF::GameType, MF::GameSkill, MF::GameClients,
     MF::GameAddress, MF::GameCaptainId, MF::GameGroupId, MF::GamePing},
    {MF::GameListType},
    {MF::ServerName, MF::ServerAddress, MF::ServerNumChat, MF::ServerNumClients, MF::ServerLastPing, MF::ServerQuerying,
     MF::ServerClients, MF::ServerConnection, MF::ServerMyClientId},
    {},
};
static const int kFieldCount[] = {2, 16, 0, 9, 7, 22, 1, 9, 0};
enum FieldType { kNum, kTextF, kObjF, kAddrF };
static FieldType TypeOf(MF f)
{
    switch (f) {
    case MF::ClientName: case MF::FriendName: case MF::FriendLocation: case MF::GroupName: case MF::GameName:
    case MF::GamePassword: case MF::ServerName: return kTextF;
    case MF::ClientPlayer: case MF::GroupClients: case MF::GameClients: case MF::ServerClients: case MF::ServerConnection:
        return kObjF;
    case MF::FriendAddress: case MF::GameAddress: case MF::ServerAddress: return kAddrF;
    default: return kNum;
    }
}
// the kind an object member holds
static int ObjKind(MF f)
{
    switch (f) {
    case MF::ClientPlayer: return (int)MK::Player;
    case MF::ServerConnection: return kConn;
    default: return (int)MK::ClientList;
    }
}

// ---- the original's member reads and writes

static long long OGet(O::Ref* r, MF f, std::string& outText)
{
    using namespace O;
    char s[32];
    long long v = 0;
    const Ref* obj = NULL;
    const Text* t = NULL;
    const Addr* a = NULL;
    bool isObj = false;
    switch (r->mKind) {
    case (int)MK::Player: { Player* p = (Player*)r; v = f == MF::PlayerPing ? p->mPing : p->mIsReady; break; }
    case (int)MK::Client: {
        Client* c = (Client*)r;
        switch (f) {
        case MF::ClientName: t = c->mName.get(); break;
        case MF::ClientId: v = c->mClientId; break;
        case MF::ClientFlags: v = c->mClientFlags; break;
        case MF::ClientWonUserId: v = c->mWONUserId; break;
        case MF::ClientCdKeyId: v = c->mCDKeyId; break;
        case MF::ClientChatMemberFlags: v = c->mChatMemberFlags; break;
        case MF::ClientGameMemberFlags: v = c->mGameMemberFlags; break;
        case MF::ClientChatGroupId: v = c->mChatGroupId; break;
        case MF::ClientGameGroupId: v = c->mGameGroupId; break;
        case MF::ClientIgnored: v = c->mIgnored; break;
        case MF::ClientBlocked: v = c->mBlocked; break;
        case MF::ClientIsFriend: v = c->mIsFriend; break;
        case MF::ClientGameCaptain: v = c->mIsGameCaptain; break;
        case MF::ClientChatCaptain: v = c->mIsChatCaptain; break;
        case MF::ClientInvited: v = c->mInvited; break;
        default: obj = c->mPlayer.get(); isObj = true; break;
        }
        break;
    }
    case (int)MK::Friend: {
        Friend* x = (Friend*)r;
        switch (f) {
        case MF::FriendName: t = x->mName.get(); break;
        case MF::FriendUserId: v = x->mUserId; break;
        case MF::FriendAddress: a = &x->mAddress; break;
        case MF::FriendLocation: t = x->mLocation.get(); break;
        case MF::FriendLastSearch: v = x->mLastSearch; break;
        case MF::FriendFlags: v = x->mFlags; break;
        case MF::FriendLastLocate: v = x->mLastLocate; break;
        case MF::FriendSearchStart: v = x->mSearchStart; break;
        default: v = x->mAnonymous; break;
        }
        break;
    }
    case (int)MK::Group: {
        Group* g = (Group*)r;
        switch (f) {
        case MF::GroupName: t = g->mName.get(); break;
        case MF::GroupId: v = g->mId; break;
        case MF::GroupCaptainId: v = g->mCaptainId; break;
        case MF::GroupNumClients: v = g->mNumClients; break;
        case MF::GroupMaxClients: v = g->mMaxClients; break;
        case MF::GroupFlags: v = g->mFlags; break;
        default: obj = g->mClients.get(); isObj = true; break;
        }
        break;
    }
    case (int)MK::Game: {
        Game* g = (Game*)r;
        switch (f) {
        case MF::GameName: t = g->mName.get(); break;
        case MF::GamePassword: t = g->mPassword.get(); break;
        case MF::GameHasPassword: v = g->mHasPassword; break;
        case MF::GameInviteOnly: v = g->mInviteOnly; break;
        case MF::GameAskToJoin: v = g->mAskToJoin; break;
        case MF::GameInProgress: v = g->mInProgress; break;
        case MF::GameInvited: v = g->mInvited; break;
        case MF::GameIsHost: v = g->mIsHost; break;
        case MF::GameShowDetails: v = g->mShowDetails; break;
        case MF::GameHaveDetails: v = g->mHaveDetails; break;
        case MF::GamePingReplied: v = g->mPingReplied; break;
        case MF::GameLastSeen: v = g->mLastSeenTime; break;
        case MF::GameFiltered: v = g->mIsFiltered; break;
        case MF::GameNumPlayers: v = g->mNumPlayers; break;
        case MF::GameOpenSlots: v = g->mNumOpenSlots; break;
        case MF::GameType: v = g->mGameType; break;
        case MF::GameSkill: v = g->mSkill; break;
        case MF::GameClients: obj = g->mClients.get(); isObj = true; break;
        case MF::GameAddress: a = &g->mAddress; break;
        case MF::GameCaptainId: v = g->mCaptainId; break;
        case MF::GameGroupId: v = g->mGroupId; break;
        default: v = g->mPing; break;
        }
        break;
    }
    case (int)MK::GameList: v = ((GameList*)r)->mType; break;
    case (int)MK::Server: {
        Server* x = (Server*)r;
        switch (f) {
        case MF::ServerName: t = x->mName.get(); break;
        case MF::ServerAddress: a = &x->mAddress; break;
        case MF::ServerNumChat: v = x->mNumChatClients; break;
        case MF::ServerNumClients: v = x->mNumClients; break;
        case MF::ServerLastPing: v = x->mLastPing; break;
        case MF::ServerQuerying: v = x->mQueryingGroups; break;
        case MF::ServerClients: obj = x->mClients.get(); isObj = true; break;
        case MF::ServerConnection: obj = x->mConnection.get(); isObj = true; break;
        default: v = x->mMyClientId; break;
        }
        break;
    }
    }
    if (t != NULL) { snprintf(s, sizeof(s), "t%d", t->mHandle); outText = s; }
    else if (isObj) { snprintf(s, sizeof(s), "h%d", Handle(obj)); outText = s; }
    else if (a != NULL) outText = Addr6(a->v);
    else { snprintf(s, sizeof(s), "%lld", v); outText = s; }
    return v;
}

static long long RGet(RS::Ref* r, MF f, std::string& outText)
{
    char s[32];
    intptr_t v = ee_won_model_get(r->mModel, f);
    switch (TypeOf(f)) {
    case kTextF: snprintf(s, sizeof(s), "t%d", reinterpret_cast<RS::Ref*>(v)->mHandle); outText = s; break;
    case kObjF: snprintf(s, sizeof(s), "h%d", RS::Handle(v)); outText = s; break;
    case kAddrF: outText = Addr6(reinterpret_cast<const EeWonIpAddr*>(v)); break;
    default: snprintf(s, sizeof(s), "%lld", (long long)v); outText = s; break;
    }
    return v;
}

static void Dump(bool inRust)
{
    std::vector<int> aHandles;
    if (inRust) for (std::map<int, RS::Ref*>::iterator it = RS::sObjs.begin(); it != RS::sObjs.end(); ++it) aHandles.push_back(it->first);
    else for (std::map<int, O::Ref*>::iterator it = O::sObjs.begin(); it != O::sObjs.end(); ++it) aHandles.push_back(it->first);
    for (size_t i = 0; i < aHandles.size(); i++) {
        int kind = inRust ? RS::sObjs[aHandles[i]]->mKind : O::sObjs[aHandles[i]]->mKind;
        int rc = inRust ? RS::sObjs[aHandles[i]]->mRc : O::sObjs[aHandles[i]]->mRc;
        std::string aLine = "state h" + std::to_string(aHandles[i]) + " k" + std::to_string(kind) + " rc" + std::to_string(rc);
        if (kind < 9) {
            for (int f = 0; f < kFieldCount[kind]; f++) {
                std::string t;
                if (inRust) RGet(RS::sObjs[aHandles[i]], kFields[kind][f], t);
                else OGet(O::sObjs[aHandles[i]], kFields[kind][f], t);
                aLine += " " + t;
            }
            if (kind == (int)MK::ClientList || kind == (int)MK::ServerList || kind == (int)MK::GameList || kind == (int)MK::Server) {
                // the containers' contents, in order
                aLine += " [";
                if (inRust) {
                    RS::Ref* r = RS::sObjs[aHandles[i]];
                    if (kind == (int)MK::ClientList) for (intptr_t j = 0; j < RS::Call(r, MC::ListCount); j++) aLine += " h" + std::to_string(RS::Handle(RS::Call(r, MC::ListAt, j)));
                    if (kind == (int)MK::Server) for (intptr_t j = 0; j < RS::Call(r, MC::ServerGroupCount); j++) aLine += " h" + std::to_string(RS::Handle(RS::Call(r, MC::ServerGroupAt, j)));
                    if (kind == (int)MK::ServerList) for (intptr_t j = 0; j < RS::Call(r, MC::ServerListCount); j++)
                        aLine += " h" + std::to_string(RS::Handle(RS::Call(r, MC::ServerListAt, j))) + "@" + Addr6((const EeWonIpAddr*)RS::Call(r, MC::ServerListAddressAt, j));
                } else {
                    O::Ref* r = O::sObjs[aHandles[i]];
                    if (kind == (int)MK::ClientList) for (size_t j = 0; j < ((O::ClientList*)r)->GetCount(); j++) aLine += " h" + std::to_string(((O::ClientList*)r)->ClientAt(j)->mHandle);
                    if (kind == (int)MK::Server) for (size_t j = 0; j < ((O::Server*)r)->mGroups.size(); j++) aLine += " h" + std::to_string(((O::Server*)r)->mGroups.At(j)->mHandle);
                    if (kind == (int)MK::ServerList) for (size_t j = 0; j < ((O::ServerList*)r)->mServers.size(); j++)
                        aLine += " h" + std::to_string(((O::ServerList*)r)->mServers.At(j)->mHandle) + "@" + Addr6(((O::ServerList*)r)->mServers.KeyAt(j).v);
                }
                aLine += " ]";
            }
        }
        sTrace.push_back(aLine);
    }
}

// a removed object's members (a held result: what the caller gets)
static std::string OFields(O::Ref* r)
{
    std::string out;
    if (r == NULL || r->mKind >= 9) return out;
    for (int f = 0; f < kFieldCount[r->mKind]; f++) { std::string t; OGet(r, kFields[r->mKind][f], t); out += " " + t; }
    return out;
}
static std::string RFields(RS::Ref* r)
{
    std::string out;
    if (r == NULL || r->mKind >= 9) return out;
    for (int f = 0; f < kFieldCount[r->mKind]; f++) { std::string t; RGet(r, kFields[r->mKind][f], t); out += " " + t; }
    return out;
}

// ---- the run

static std::vector<CRec> sStepC;

static std::vector<std::string> RunSide(bool inRust, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    O::sObjs.clear(); RS::sObjs.clear();
    O::sNextObj = O::sNextText = RS::sNextObj = RS::sNextText = 0;
    O::sFriendRoots.clear(); RS::sFriendRoots.clear();
    O::sEmpty = new O::Text(0); O::sEmpty->mRc = 1000000;
    RS::sEmpty = RS::New(kText, 0); RS::sEmpty->mRc = 1000000;
    std::vector<int> aRoots;
    // the cursors: per step's list, a cursor each side keeps (four of them)
    O::Iter anOIters[4]; EeWonModelCursor anRIters[4]; int anRIterList[4], anOIterList[4];
    for (int i = 0; i < 4; i++) { anOIters[i].mList = NULL; anOIters[i].Rewind(); anRIterList[i] = anOIterList[i] = -1; ee_won_model_cursor_reset(&anRIters[i]); }
    for (size_t n = 0; n < inSteps.size(); n++) {
        const Step& s = inSteps[n];
        if (s.mWhat == kNew) {
            int h;
            if (inRust) {
                RS::Ref* r = RS::New(s.mKind, s.mKind == kText ? (int)s.a : 0);
                r->mRc = 1;
                if (s.mKind == (int)MK::GameList) ee_won_model_set(r->mModel, MF::GameListType, s.a);
                if (s.mKind == (int)MK::Friend) {
                    RS::Ref* t = RS::New(kText, (int)s.b); RS::AddRef(t);
                    ee_won_model_set(r->mModel, MF::FriendName, (intptr_t)t);
                    ee_won_model_set(r->mModel, MF::FriendUserId, s.a);
                    RS::Release(t);
                    RS::sFriendRoots.push_back(r->mHandle);
                }
                h = r->mHandle;
            } else {
                O::Ref* r;
                switch (s.mKind) {
                case (int)MK::Player: r = new O::Player; break;
                case (int)MK::Client: r = new O::Client; break;
                case (int)MK::ClientList: r = new O::ClientList; break;
                case (int)MK::Friend: {
                    O::Friend* f = new O::Friend;
                    O::GS t(new O::Text((int)s.b));
                    f->mName = t; f->mUserId = (unsigned long)s.a;
                    r = f;
                    break;
                }
                case (int)MK::Group: r = new O::Group; break;
                case (int)MK::Game: r = new O::Game; break;
                case (int)MK::GameList: r = new O::GameList((int)s.a); break;
                case (int)MK::Server: r = new O::Server; break;
                case (int)MK::ServerList: r = new O::ServerList; break;
                default: r = new O::Conn; break;
                }
                r->mRc = 1;
                if (s.mKind == (int)MK::Friend) O::sFriendRoots.push_back(r->mHandle);
                h = r->mHandle;
            }
            aRoots.push_back(h);
            Line("new h%lld k%lld", h, s.mKind);
            continue;
        }
        if (s.mWhat == kRelease) {
            for (size_t i = 0; i < aRoots.size(); i++) {
                if (inRust) { if (RS::sObjs.count(aRoots[i])) RS::Release(RS::sObjs[aRoots[i]]); }
                else if (O::sObjs.count(aRoots[i])) O::sObjs[aRoots[i]]->Release();
            }
            aRoots.clear();
            continue;
        }
        if (s.mWhat == kGet && s.mCall < 0) { Dump(inRust); continue; }
        int h = inRust ? Nth(RS::sObjs, s.mKind, s.mK, true) : Nth(O::sObjs, s.mKind, s.mK, false);
        if (h < 0) { Line("skip %lld", (long long)n); continue; }
        int ah = s.mArgKind >= 0 ? (inRust ? Nth(RS::sObjs, s.mArgKind, s.mArgK, true) : Nth(O::sObjs, s.mArgKind, s.mArgK, false)) : -1;
        if (s.mArgKind >= 0 && ah < 0 && s.mArgKind != kConn && s.mWhat != kSetObj) { Line("skip %lld", (long long)n); continue; }
        // a list member is never NULL (the original dereferenced them); a game's list may be, but not for its walks
        if (s.mWhat == kSetObj && ah < 0 && s.mCall != (int)MF::ClientPlayer && s.mCall != (int)MF::GameClients) { Line("skip %lld", (long long)n); continue; }
        int ch = s.mCall == (int)MC::ServerAddClientToGroup && s.mWhat == kCall
                     ? (inRust ? Nth(RS::sObjs, (int)MK::Client, (int)s.c, true) : Nth(O::sObjs, (int)MK::Client, (int)s.c, false)) : 0;
        if (ch < 0) { Line("skip %lld", (long long)n); continue; }
        if (s.mWhat == kCall && (s.mCall == (int)MC::GameAllReady || s.mCall == (int)MC::GameCanStart)) {
            bool none = inRust ? ee_won_model_get(RS::sObjs[h]->mModel, MF::GameClients) == 0
                               : static_cast<O::Game*>(O::sObjs[h])->mClients.get() == NULL;
            if (none) { Line("skip %lld", (long long)n); continue; }
        }
        // the routing records
        std::vector<std::pair<unsigned short, const CRec*> > aCMap;
        std::vector<std::pair<unsigned short, const GRec*> > aGMap;
        for (size_t i = 0; i < s.mRecs.size(); i++) {
            if (s.mCall == (int)MC::ServerSyncGroups || s.mCall == (int)MC::ServerAddGroup) aGMap.push_back(std::make_pair(sGRecs[s.mRecs[i]].mId, &sGRecs[s.mRecs[i]]));
            else aCMap.push_back(std::make_pair(sCRecs[s.mRecs[i]].mId, &sCRecs[s.mRecs[i]]));
        }
        long long r = 0, out = 0;
        std::string rs;
        char buf[64];
        if (inRust) {
            RS::Ref* o = RS::sObjs[h];
            intptr_t arg = ah >= 0 ? (intptr_t)RS::sObjs[ah] : 0;
            if (s.mWhat == kGet) { RGet(o, (MF)s.mCall, rs); Line("get %lld %s", s.mCall, 0); sTrace.back() += " " + rs; continue; }
            if (s.mWhat == kSet) { ee_won_model_set(o->mModel, (MF)s.mCall, (intptr_t)s.a); Line("set %lld", s.mCall); continue; }
            if (s.mWhat == kSetText) {
                RS::Ref* t = RS::New(kText, (int)s.a); RS::AddRef(t);
                ee_won_model_set(o->mModel, (MF)s.mCall, (intptr_t)t); RS::Release(t);
                Line("settext %lld", s.mCall); continue;
            }
            if (s.mWhat == kSetObj) {
                if (s.mArgKind == kConn && ah < 0) { Line("skip %lld", (long long)n); continue; }
                ee_won_model_set(o->mModel, (MF)s.mCall, arg); Line("setobj %lld h%lld", s.mCall, RS::Handle(arg)); continue;
            }
            if (s.mWhat == kSetAddr) { ee_won_model_set(o->mModel, (MF)s.mCall, (intptr_t)sAddrs[s.a]); Line("setaddr %lld", s.mCall); continue; }
            if (s.mWhat == kIter) {
                int i = (int)s.a;
                if (anRIterList[i] != h) { anRIterList[i] = h; ee_won_model_cursor_reset(&anRIters[i]); }
                intptr_t c = 0;
                switch (s.mCall) {
                case 0: ee_won_model_cursor_reset(&anRIters[i]); break;
                case 1: r = RS::Call(o, MC::ListIterHasMore, (intptr_t)&anRIters[i]); break;
                case 2: r = RS::Handle(RS::Call(o, MC::ListIterNext, (intptr_t)&anRIters[i])); break;
                case 3: r = RS::Call(o, MC::ListSetHasMore, (intptr_t)&anRIters[i]); break;
                default: r = RS::Handle(RS::Call(o, MC::ListSetNext, (intptr_t)&anRIters[i], (intptr_t)&c)); out = RS::Handle(c); break;
                }
                Line("iter %lld %lld = %lld %lld", s.mCall, i, r, out);
                continue;
            }
            std::vector<uint16_t> ids; std::vector<intptr_t> infos;
            for (size_t i = 0; i < aCMap.size(); i++) { ids.push_back(aCMap[i].first); infos.push_back((intptr_t)aCMap[i].second); }
            for (size_t i = 0; i < aGMap.size(); i++) { ids.push_back(aGMap[i].first); infos.push_back((intptr_t)aGMap[i].second); }
            intptr_t pid = ids.empty() ? 0 : (intptr_t)&ids[0], pinf = infos.empty() ? 0 : (intptr_t)&infos[0];
            intptr_t old = 0; bool existed = false;
            bool held = false, ptr = false;
            switch ((MC)s.mCall) {
            case MC::ListSync: case MC::ServerSyncClients: r = RS::Call(o, (MC)s.mCall, pid, pinf, ids.size()); break;
            case MC::ServerSyncGroups: r = RS::Call(o, MC::ServerSyncGroups, pid, pinf, (intptr_t)((uint64_t)ids.size() | (uint64_t)(s.a & 1) << 32)); break;
            case MC::ServerAddClient: r = RS::Call(o, MC::ServerAddClient, (intptr_t)&sCRecs[s.mRecs[0]]); ptr = true; break;
            case MC::ServerAddGroup: r = RS::Call(o, MC::ServerAddGroup, (intptr_t)&sGRecs[s.mRecs[0]]); ptr = true; break;
            case MC::GroupSetCaptain: r = RS::Call(o, MC::GroupSetCaptain, (intptr_t)s.a, (intptr_t)&old); ptr = true; out = RS::Handle(old); break;
            case MC::GameListGetOrCreateId: r = RS::Call(o, (MC)s.mCall, (intptr_t)s.a, (intptr_t)&existed); ptr = true; out = existed; break;
            case MC::GameListGetOrCreateAddr: r = RS::Call(o, (MC)s.mCall, (intptr_t)sAddrs[s.a], (intptr_t)&existed); ptr = true; out = existed; break;
            case MC::GameListGetAddr: case MC::ServerListFind: case MC::ServerListInsert:
                r = RS::Call(o, (MC)s.mCall, (intptr_t)sAddrs[s.a]); ptr = true; break;
            case MC::GameListRemoveAddr: r = RS::Call(o, (MC)s.mCall, (intptr_t)sAddrs[s.a]); held = true; break;
            case MC::FriendSetLocation: {
                RS::Ref* t = RS::New(kText, (int)s.b); RS::AddRef(t);
                r = RS::Call(o, MC::FriendSetLocation, (intptr_t)sAddrs[s.a], (intptr_t)t, (intptr_t)s.c);
                RS::Release(t);
                break;
            }
            case MC::ServerHasAddress: case MC::ServerAddressAdd: {
                static const char* kNames[] = {"en", "EN", "fr", "De", "de", "it"};
                r = RS::Call(o, (MC)s.mCall, (intptr_t)kNames[s.a], (intptr_t)strlen(kNames[s.a]));
                break;
            }
            case MC::GameStoreSummary: {
                EeWonGameSummary aS; memset(&aS, 0, sizeof(aS));
                aS.read = (uint32_t)s.a; aS.in_progress = s.b & 1; aS.has_password = (s.b >> 1) & 1; aS.invite_only = (s.b >> 2) & 1;
                aS.ask_to_join = (s.b >> 3) & 1; aS.skill = (int)(s.b >> 4) & 7; aS.players = (int)(s.c & 7); aS.open_slots = (int)((s.c >> 3) & 7);
                for (int i = 0; i < 6; i++) aS.address[i] = (unsigned char)(s.c >> (6 + i));
                RS::Ref* t = (aS.read & 4) ? RS::New(kText, (int)(s.c % 50)) : RS::sEmpty;
                RS::AddRef(t);
                r = RS::Call(o, MC::GameStoreSummary, (intptr_t)&aS, (intptr_t)t);
                RS::Release(t);
                break;
            }
            case MC::ServerAddClientToGroup: r = RS::Call(o, MC::ServerAddClientToGroup, (intptr_t)RS::sObjs[ch], (intptr_t)s.b, arg); break;
            case MC::ServerRemoveClientFromGroup: r = RS::Call(o, MC::ServerRemoveClientFromGroup, (intptr_t)s.a, arg); held = true; break;
            default: {
                bool objArg = s.mArgKind >= 0;
                r = RS::Call(o, (MC)s.mCall, objArg ? arg : (intptr_t)s.a, objArg ? (intptr_t)s.a : (intptr_t)s.b, (intptr_t)s.c);
                switch ((MC)s.mCall) {
                case MC::ListFind: case MC::ListFindByWonId: case MC::ListAt: case MC::GroupCaptain: case MC::GroupFind:
                case MC::GameFindClient: case MC::GameListGetId: case MC::GameListNext: case MC::ServerGetClient:
                case MC::ServerFindByWonId: case MC::ServerMyClient: case MC::ServerGroup: case MC::ServerGroupAt:
                case MC::ServerListAt: ptr = true; break;
                case MC::ListRemove: case MC::GroupRemove: case MC::GameListRemoveId: case MC::ServerRemoveGroup:
                case MC::ServerRemoveClient: case MC::ServerRemoveClientFromGroup: held = true; break;
                default: break;
                }
            }
            }
            if (ptr || held) { long long x = RS::Handle(r); if (held && r) { rs = RFields(reinterpret_cast<RS::Ref*>(r)); RS::Release(reinterpret_cast<RS::Ref*>(r)); } r = x; }
        } else {
            using namespace O;
            Ref* o = sObjs[h];
            Ref* arg = ah >= 0 ? sObjs[ah] : NULL;
            if (s.mWhat == kGet) { OGet(o, (MF)s.mCall, rs); Line("get %lld %s", s.mCall, 0); sTrace.back() += " " + rs; continue; }
            if (s.mWhat == kSet || s.mWhat == kSetText || s.mWhat == kSetObj || s.mWhat == kSetAddr) {
                if (s.mWhat == kSetObj && s.mArgKind == kConn && ah < 0) { Line("skip %lld", (long long)n); continue; }
                MF f = (MF)s.mCall;
                GS t;
                if (s.mWhat == kSetText) t = new Text((int)s.a);
                long long v = s.a;
                switch (o->mKind) {
                case (int)MK::Player: { Player* p = (Player*)o; if (f == MF::PlayerPing) p->mPing = (unsigned short)v; else p->mIsReady = v != 0; break; }
                case (int)MK::Client: {
                    Client* c = (Client*)o;
                    switch (f) {
                    case MF::ClientName: c->mName = t; break;
                    case MF::ClientId: c->mClientId = (unsigned short)v; break;
                    case MF::ClientFlags: c->mClientFlags = (unsigned long)v; break;
                    case MF::ClientWonUserId: c->mWONUserId = (unsigned long)v; break;
                    case MF::ClientCdKeyId: c->mCDKeyId = (unsigned long)v; break;
                    case MF::ClientChatMemberFlags: c->mChatMemberFlags = (unsigned char)v; break;
                    case MF::ClientGameMemberFlags: c->mGameMemberFlags = (unsigned char)v; break;
                    case MF::ClientChatGroupId: c->mChatGroupId = (unsigned short)v; break;
                    case MF::ClientGameGroupId: c->mGameGroupId = (unsigned short)v; break;
                    case MF::ClientIgnored: c->mIgnored = v != 0; break;
                    case MF::ClientBlocked: c->mBlocked = v != 0; break;
                    case MF::ClientIsFriend: c->mIsFriend = v != 0; break;
                    case MF::ClientGameCaptain: c->mIsGameCaptain = v != 0; break;
                    case MF::ClientChatCaptain: c->mIsChatCaptain = v != 0; break;
                    case MF::ClientInvited: c->mInvited = v != 0; break;
                    default: c->mPlayer = (Player*)arg; break;
                    }
                    break;
                }
                case (int)MK::Friend: {
                    Friend* x = (Friend*)o;
                    switch (f) {
                    case MF::FriendName: x->mName = t; break;
                    case MF::FriendUserId: x->mUserId = (unsigned long)v; break;
                    case MF::FriendAddress: x->mAddress = Addr(sAddrs[v]); break;
                    case MF::FriendLocation: x->mLocation = t; break;
                    case MF::FriendLastSearch: x->mLastSearch = (unsigned long)v; break;
                    case MF::FriendFlags: x->mFlags = (unsigned char)v; break;
                    case MF::FriendLastLocate: x->mLastLocate = (unsigned long)v; break;
                    case MF::FriendSearchStart: x->mSearchStart = (unsigned long)v; break;
                    default: x->mAnonymous = v != 0; break;
                    }
                    break;
                }
                case (int)MK::Group: {
                    Group* g = (Group*)o;
                    switch (f) {
                    case MF::GroupName: g->mName = t; break;
                    case MF::GroupId: g->mId = (unsigned short)v; break;
                    case MF::GroupCaptainId: g->mCaptainId = (unsigned short)v; break;
                    case MF::GroupNumClients: g->mNumClients = (unsigned long)v; break;
                    case MF::GroupMaxClients: g->mMaxClients = (unsigned long)v; break;
                    case MF::GroupFlags: g->mFlags = (unsigned long)v; break;
                    default: g->mClients = (ClientList*)arg; break;
                    }
                    break;
                }
                case (int)MK::Game: {
                    Game* g = (Game*)o;
                    switch (f) {
                    case MF::GameName: g->mName = t; break;
                    case MF::GamePassword: g->mPassword = t; break;
                    case MF::GameHasPassword: g->mHasPassword = v != 0; break;
                    case MF::GameInviteOnly: g->mInviteOnly = v != 0; break;
                    case MF::GameAskToJoin: g->mAskToJoin = v != 0; break;
                    case MF::GameInProgress: g->mInProgress = v != 0; break;
                    case MF::GameInvited: g->mInvited = v != 0; break;
                    case MF::GameIsHost: g->mIsHost = v != 0; break;
                    case MF::GameShowDetails: g->mShowDetails = v != 0; break;
                    case MF::GameHaveDetails: g->mHaveDetails = v != 0; break;
                    case MF::GamePingReplied: g->mPingReplied = v != 0; break;
                    case MF::GameLastSeen: g->mLastSeenTime = (long)v; break;
                    case MF::GameFiltered: g->mIsFiltered = v != 0; break;
                    case MF::GameNumPlayers: g->mNumPlayers = (long)v; break;
                    case MF::GameOpenSlots: g->mNumOpenSlots = (long)v; break;
                    case MF::GameType: g->mGameType = (long)v; break;
                    case MF::GameSkill: g->mSkill = (long)v; break;
                    case MF::GameClients: g->mClients = (ClientList*)arg; break;
                    case MF::GameAddress: g->mAddress = Addr(sAddrs[v]); break;
                    case MF::GameCaptainId: g->mCaptainId = (unsigned short)v; break;
                    case MF::GameGroupId: g->mGroupId = (unsigned short)v; break;
                    default: g->mPing = (unsigned short)v; break;
                    }
                    break;
                }
                case (int)MK::GameList: ((GameList*)o)->mType = (long)v; break;
                case (int)MK::Server: {
                    Server* x = (Server*)o;
                    switch (f) {
                    case MF::ServerName: x->mName = t; break;
                    case MF::ServerAddress: x->mAddress = Addr(sAddrs[v]); break;
                    case MF::ServerNumChat: x->mNumChatClients = (int)v; break;
                    case MF::ServerNumClients: x->mNumClients = (int)v; break;
                    case MF::ServerLastPing: x->mLastPing = (unsigned long)v; break;
                    case MF::ServerQuerying: x->mQueryingGroups = v != 0; break;
                    case MF::ServerClients: x->mClients = (ClientList*)arg; break;
                    case MF::ServerConnection: x->mConnection = (Conn*)arg; break;
                    default: x->mMyClientId = (unsigned short)v; break;
                    }
                    break;
                }
                }
                if (s.mWhat == kSet) Line("set %lld", s.mCall);
                else if (s.mWhat == kSetText) Line("settext %lld", s.mCall);
                else if (s.mWhat == kSetObj) Line("setobj %lld h%lld", s.mCall, Handle(arg));
                else Line("setaddr %lld", s.mCall);
                continue;
            }
            if (s.mWhat == kIter) {
                int i = (int)s.a;
                if (anOIterList[i] != h) { anOIterList[i] = h; anOIters[i].mList = (ClientList*)o; anOIters[i].Rewind(); }
                Client* c = NULL;
                switch (s.mCall) {
                case 0: anOIters[i].Rewind(); break;
                case 1: r = anOIters[i].HasMore(); break;
                case 2: r = Handle(anOIters[i].Next()); break;
                case 3: r = anOIters[i].SetHasMore(); break;
                default: r = Handle(anOIters[i].SetNext(&c)); out = Handle(c); break;
                }
                Line("iter %lld %lld = %lld %lld", s.mCall, i, r, out);
                continue;
            }
            Client* cl = (Client*)o; ClientList* li = (ClientList*)o; Friend* fr = (Friend*)o; Group* gr = (Group*)o;
            Game* ga = (Game*)o; GameList* gl = (GameList*)o; Server* sv = (Server*)o; ServerList* sl = (ServerList*)o;
            Player* pl = (Player*)o;
            long long a = s.a, b = s.b, c = s.c;
            static const char* kNames[] = {"en", "EN", "fr", "De", "de", "it"};
            bool ptr = false;
            P<Ref> keep;
            Ref* rp = NULL;
            switch ((MC)s.mCall) {
            case MC::PlayerIsSameTeam: r = pl->IsSameTeam((Player*)arg); break;
            case MC::ClientClearGroup: cl->ClearGroup((unsigned short)a, b != 0); break;
            case MC::ClientInChat: r = cl->IsInChatGroup(); break;
            case MC::ClientInGame: r = cl->IsInGameGroup(); break;
            case MC::ClientUpdateIgnore: cl->UpdateIgnoreStatus(); break;
            case MC::ClientAnonymous: r = cl->IsAnonymous(); break;
            case MC::ClientMuted: r = cl->IsMuted(a != 0); break;
            case MC::ClientAway: r = cl->IsAway(); break;
            case MC::ClientAdmin: r = cl->IsAdmin(); break;
            case MC::ClientModerator: r = cl->IsModerator(); break;
            case MC::ClientIsCaptain: r = cl->IsCaptain(a != 0); break;
            case MC::ClientSetCaptain: cl->SetCaptain(a != 0, b != 0); break;
            case MC::ClientHasPlayer: r = cl->HasPlayer(); break;
            case MC::ClientIsReady: r = cl->IsReady(); break;
            case MC::ClientPing: r = cl->GetPing(); break;
            case MC::ClientIsTeammate: r = cl->IsTeammate((Client*)arg); break;
            case MC::ClientSetGroupId: cl->SetGroupId((unsigned short)a, b != 0); break;
            case MC::ClientSetMemberFlags: cl->SetMemberFlags((unsigned char)a, b != 0); break;
            case MC::ListClear: li->Clear(); break;
            case MC::ListSync: li->SyncClients(aCMap); break;
            case MC::ListUpdateIgnore: li->UpdateIgnoreStatus(); break;
            case MC::ListAdd: li->AddClient((Client*)arg); break;
            case MC::ListFind: rp = li->FindClient((unsigned short)a); ptr = true; break;
            case MC::ListRemove: { P<Client> x = li->RemoveClient((unsigned short)a); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::ListFindByWonId: rp = li->FindClientByWONId((unsigned long)a); ptr = true; break;
            case MC::ListClearInvited: li->ClearInvited(); break;
            case MC::ListCount: r = li->GetCount(); break;
            case MC::ListAt: rp = li->ClientAt((size_t)a); ptr = true; break;
            case MC::FriendSetLocation: { GS t(new Text((int)b)); fr->SetLocation(Addr(sAddrs[a]), t.get(), (int)c); break; }
            case MC::FriendClearLocation: fr->ClearLocation(); break;
            case MC::FriendSetStatusFromClient: fr->SetStatusFromClient((Client*)arg); break;
            case MC::FriendStartLocate: fr->StartLocate((unsigned long)a); break;
            case MC::FriendEndLocate: fr->EndLocate((unsigned long)a); break;
            case MC::GroupAdd: gr->AddClient((Client*)arg, (unsigned char)a); break;
            case MC::GroupRemove: { P<Client> x = gr->RemoveClient((unsigned short)a); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::GroupSetCaptain: { Client* old = NULL; rp = gr->SetCaptain((unsigned short)a, &old); ptr = true; out = Handle(old); break; }
            case MC::GroupCaptain: rp = gr->GetCaptain(); ptr = true; break;
            case MC::GroupFind: rp = gr->mClients->FindClient((unsigned short)a); ptr = true; break;
            case MC::GroupKind: { EeWonGroupKind k = gr->K(); r = k.password | k.ask_to_join << 1 | k.invite_only << 2 | k.chat << 3 | k.public_chat << 4; break; }
            case MC::GameCopyFrom: ga->CopyFrom((Game*)arg); break;
            case MC::GameFindClient: rp = ga->FindClient((unsigned short)a); ptr = true; break;
            case MC::GameMatchesRoomSpec: r = ee_won_game_matches_room_spec((int32_t)a, (int32_t)ga->mGameType); break;
            case MC::GamePinging: r = ee_won_model_game_pinging(ga->mPing); break;
            case MC::GameQuerying: r = ee_won_model_game_querying(ga->mPing); break;
            case MC::GameRemoveClient: ga->RemoveClient((Client*)arg); break;
            case MC::GameIsFull: r = ga->IsFull(); break;
            case MC::GameJoinStatus: r = ga->GetJoinStatus(); break;
            case MC::GameClearPlayers: ga->ClearPlayers(); break;
            case MC::GameIsSameGame: r = ga->IsSameGame((Game*)arg); break;
            case MC::GameReadGroup: ga->ReadGroup((Group*)arg); break;
            case MC::GameHandleHost: ga->HandleHostGame((Client*)arg); break;
            case MC::GameAllReady: r = ga->AreAllReady(); break;
            case MC::GameCanStart: r = ga->CanStartGame(); break;
            case MC::GameStoreSummary: {
                EeWonGameSummary aS; memset(&aS, 0, sizeof(aS));
                aS.read = (uint32_t)a; aS.in_progress = b & 1; aS.has_password = (b >> 1) & 1; aS.invite_only = (b >> 2) & 1;
                aS.ask_to_join = (b >> 3) & 1; aS.skill = (int)(b >> 4) & 7; aS.players = (int)(c & 7); aS.open_slots = (int)((c >> 3) & 7);
                for (int i = 0; i < 6; i++) aS.address[i] = (unsigned char)(c >> (6 + i));
                ga->StoreSummary(aS, (int)(c % 50));
                break;
            }
            case MC::GameListClear: gl->Clear(); break;
            case MC::GameListAdd: gl->AddGame((Game*)arg); break;
            case MC::GameListGetOrCreateId: { bool e = false; rp = gl->GetOrCreateGame((unsigned short)a, &e); ptr = true; out = e; break; }
            case MC::GameListGetOrCreateAddr: { bool e = false; rp = gl->GetOrCreateGame(Addr(sAddrs[a]), &e); ptr = true; out = e; break; }
            case MC::GameListGetId: rp = gl->mIdGames.Get((unsigned short)a); ptr = true; break;
            case MC::GameListGetAddr: rp = gl->mAddrGames.Get(Addr(sAddrs[a])); ptr = true; break;
            case MC::GameListRemoveId: { P<Game> x = gl->RemoveGame((unsigned short)a); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::GameListRemoveAddr: { P<Game> x = gl->RemoveGame(Addr(sAddrs[a])); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::GameListRewind: gl->Rewind(); break;
            case MC::GameListHasMore: r = gl->HasMore(); break;
            case MC::GameListNext: rp = gl->Next(); ptr = true; break;
            case MC::GameListIsEmpty: r = gl->IsEmpty(); break;
            case MC::GameListRemoveStale: r = gl->RemoveStaleGames((long)a, b); break;
            case MC::ServerHasAddress: r = sv->mAddresses.count(kNames[a]) != 0; break;
            case MC::ServerSyncClients: sv->mClients->SyncClients(aCMap); sv->mClients->UpdateIgnoreStatus(); break;
            case MC::ServerUpdateFriends: sv->UpdateFriends(); break;
            case MC::ServerClearFriendLocations: sv->ClearFriendLocations(a); break;
            case MC::ServerSyncGroups: sv->SyncGroups(aGMap, (a & 1) != 0); break;
            case MC::ServerAddClient: rp = sv->AddClient(&sCRecs[s.mRecs[0]]); ptr = true; break;
            case MC::ServerAddClientToGroup: sv->AddClientToGroup((Client*)sObjs[ch], (unsigned char)b, (Group*)arg); break;
            case MC::ServerAddGroup: rp = sv->AddGroup(&sGRecs[s.mRecs[0]]); ptr = true; break;
            case MC::ServerRemoveGroup: { P<Group> x = sv->mGroups.Remove((unsigned short)a); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::ServerRemoveClient: { P<Client> x = sv->mClients->RemoveClient((unsigned short)a); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::ServerRemoveClientFromGroup: { P<Client> x = sv->RemoveClientFromGroup((unsigned short)a, (Group*)arg); r = Handle(x.get()); rs = OFields(x.get()); break; }
            case MC::ServerGetClient: rp = sv->mClients->FindClient((unsigned short)a); ptr = true; break;
            case MC::ServerFindByWonId: rp = sv->mClients->FindClientByWONId((unsigned long)a); ptr = true; break;
            case MC::ServerMyClient: rp = sv->mMyClientId == 0xffff ? NULL : sv->mClients->FindClient(sv->mMyClientId); ptr = true; break;
            case MC::ServerGroup: rp = sv->mGroups.Get((unsigned short)a); ptr = true; break;
            case MC::ServerNumClientsNow: r = ee_won_model_server_num_clients(sv->IsConnected(), (uint32_t)sv->mClients->GetCount(), sv->mNumClients); break;
            case MC::ServerConnected: r = sv->IsConnected(); break;
            case MC::ServerClearGroups: sv->mGroups.clear(); break;
            case MC::ServerGroupCount: r = sv->mGroups.size(); break;
            case MC::ServerGroupAt: rp = sv->mGroups.At((size_t)a); ptr = true; break;
            case MC::ServerSetListed: sv->mNumClients = (uint16_t)a; break;
            case MC::ServerAddressesClear: sv->mAddresses.clear(); break;
            case MC::ServerAddressAdd: sv->mAddresses.insert(kNames[a]); break;
            case MC::ServerDisconnect: sv->Disconnect(a); break;
            case MC::ServerListInsert: rp = sl->Insert(Addr(sAddrs[a])); ptr = true; break;
            case MC::ServerListFind: rp = sl->mServers.Get(Addr(sAddrs[a])); ptr = true; break;
            case MC::ServerListCount: r = sl->mServers.size(); break;
            case MC::ServerListAt: rp = sl->mServers.At((size_t)a); ptr = true; break;
            default: break;
            }
            if (ptr) r = Handle(rp);
        }
        (void)buf;
        Line("call %lld %lld = %lld %lld", s.mCall, h, r, out);
        sTrace.back() += rs;
    }
    // the leftovers (none, but for an object a root's release did not reach)
    if (inRust) {
        for (std::map<int, RS::Ref*>::iterator it = RS::sObjs.begin(); it != RS::sObjs.end(); ++it) Line("left h%lld rc%lld", it->first, it->second->mRc);
        int n = 0;
        (void)n;
        RS::sObjs.clear();
    } else {
        for (std::map<int, O::Ref*>::iterator it = O::sObjs.begin(); it != O::sObjs.end(); ++it) Line("left h%lld rc%lld", it->first, it->second->mRc);
        O::sObjs.clear();
    }
    Line("texts %lld", inRust ? RS::sNextText : O::sNextText);
    return sTrace;
}

// ---- the generator

static const MC kCalls[] = {
    MC::PlayerIsSameTeam, MC::ClientClearGroup, MC::ClientInChat, MC::ClientInGame, MC::ClientUpdateIgnore, MC::ClientAnonymous,
    MC::ClientMuted, MC::ClientAway, MC::ClientAdmin, MC::ClientModerator, MC::ClientIsCaptain, MC::ClientSetCaptain,
    MC::ClientHasPlayer, MC::ClientIsReady, MC::ClientPing, MC::ClientIsTeammate, MC::ClientSetGroupId,
    MC::ClientSetMemberFlags, MC::ListClear, MC::ListSync, MC::ListUpdateIgnore, MC::ListAdd, MC::ListFind, MC::ListRemove,
    MC::ListFindByWonId, MC::ListClearInvited, MC::ListCount, MC::ListAt, MC::FriendSetLocation, MC::FriendClearLocation,
    MC::FriendSetStatusFromClient, MC::FriendStartLocate, MC::FriendEndLocate, MC::GroupAdd, MC::GroupRemove,
    MC::GroupSetCaptain, MC::GroupCaptain, MC::GroupFind, MC::GroupKind, MC::GameCopyFrom, MC::GameFindClient,
    MC::GameMatchesRoomSpec, MC::GamePinging, MC::GameQuerying, MC::GameRemoveClient, MC::GameIsFull, MC::GameJoinStatus,
    MC::GameClearPlayers, MC::GameIsSameGame, MC::GameReadGroup, MC::GameHandleHost, MC::GameAllReady, MC::GameCanStart,
    MC::GameStoreSummary, MC::GameListClear, MC::GameListAdd, MC::GameListGetOrCreateId, MC::GameListGetOrCreateAddr,
    MC::GameListGetId, MC::GameListGetAddr, MC::GameListRemoveId, MC::GameListRemoveAddr, MC::GameListRewind,
    MC::GameListHasMore, MC::GameListNext, MC::GameListIsEmpty, MC::GameListRemoveStale, MC::ServerHasAddress,
    MC::ServerSyncClients, MC::ServerUpdateFriends, MC::ServerClearFriendLocations, MC::ServerSyncGroups, MC::ServerAddClient,
    MC::ServerAddClientToGroup, MC::ServerAddGroup, MC::ServerRemoveGroup, MC::ServerRemoveClient,
    MC::ServerRemoveClientFromGroup, MC::ServerGetClient, MC::ServerFindByWonId, MC::ServerMyClient, MC::ServerGroup,
    MC::ServerNumClientsNow, MC::ServerConnected, MC::ServerClearGroups, MC::ServerGroupCount, MC::ServerGroupAt,
    MC::ServerSetListed, MC::ServerAddressesClear, MC::ServerAddressAdd, MC::ServerDisconnect, MC::ServerListInsert,
    MC::ServerListFind, MC::ServerListCount, MC::ServerListAt,
};
static int KindOfCall(MC c)
{
    int v = (int)c;
    if (v < 10) return (int)MK::Player;
    if (v < 40) return (int)MK::Client;
    if (v < 70) return (int)MK::ClientList;
    if (v < 80) return (int)MK::Friend;
    if (v < 100) return (int)MK::Group;
    if (v < 130) return (int)MK::Game;
    if (v < 150) return (int)MK::GameList;
    if (v < 190) return (int)MK::Server;
    return (int)MK::ServerList;
}
// the object argument a call takes (its kind), or -1
static int ArgKind(MC c)
{
    switch (c) {
    case MC::PlayerIsSameTeam: return (int)MK::Player;
    case MC::ClientIsTeammate: case MC::ListAdd: case MC::FriendSetStatusFromClient: case MC::GroupAdd:
    case MC::GameRemoveClient: case MC::GameHandleHost: return (int)MK::Client;
    case MC::GameCopyFrom: case MC::GameIsSameGame: case MC::GameListAdd: return (int)MK::Game;
    case MC::GameReadGroup: case MC::ServerAddClientToGroup: case MC::ServerRemoveClientFromGroup: return (int)MK::Group;
    default: return -1;
    }
}
static long long Id() { return R(10) == 0 ? 0xffff : R(7); }

static std::vector<Step> Generate()
{
    std::vector<Step> v;
    Step s;
    s.mWhat = kNew; s.mK = 0; s.mCall = 0; s.a = s.b = s.c = 0; s.mArgKind = -1; s.mArgK = 0;
    static const int kRoots[] = {1, 1, 0, 0, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, kConn, kConn};
    for (size_t i = 0; i < sizeof(kRoots) / sizeof(kRoots[0]); i++) {
        s.mKind = kRoots[i];
        s.a = s.mKind == 6 ? (i % 2 ? 2 : 1) : s.mKind == 3 ? (long long)R(6) * 10 : 0;
        s.b = R(40);
        v.push_back(s);
    }
    int n = 1 + (int)R(40);
    for (int i = 0; i < n; i++) {
        Step t;
        t.mRecs.clear();
        t.mArgKind = -1; t.mArgK = 0; t.mK = (int)R(3); t.a = t.b = t.c = 0;
        unsigned w = R(20);
        if (w < 3) {
            // a member set
            t.mKind = (int)R(9);
            if (t.mKind == 2 || t.mKind == 8) t.mKind = 1;
            int nf = kFieldCount[t.mKind];
            MF f = kFields[t.mKind][R(nf)];
            t.mCall = (int)f;
            switch (TypeOf(f)) {
            case kTextF: t.mWhat = kSetText; t.a = R(40); break;
            case kObjF: t.mWhat = kSetObj; t.mArgKind = ObjKind(f); t.mArgK = R(5) == 0 ? 9 : (int)R(3); break;
            case kAddrF: t.mWhat = kSetAddr; t.a = R(6); break;
            default: t.mWhat = kSet; t.a = R(4) == 0 ? (long long)R(0x8000) * 3 : R(3) == 0 ? 0xffff : R(9); break;
            }
        } else if (w == 5 || w == 6) {
            // a game's ping or address (what a list's removal resets and keys)
            t.mKind = (int)MK::Game; t.mK = (int)R(5);
            if (w == 5) { t.mWhat = kSet; t.mCall = (int)MF::GamePing; t.a = R(5); }
            else { t.mWhat = kSetAddr; t.mCall = (int)MF::GameAddress; t.a = R(6); }
        } else if (w == 3) {
            t.mWhat = kIter; t.mKind = (int)MK::ClientList; t.mCall = (int)R(5); t.a = R(4);
        } else if (w == 4) {
            t.mWhat = kGet; t.mKind = 0; t.mCall = -1; // a dump
        } else {
            MC c = kCalls[R(sizeof(kCalls) / sizeof(kCalls[0]))];
            t.mWhat = kCall; t.mCall = (int)c; t.mKind = KindOfCall(c);
            t.mArgKind = ArgKind(c); t.mArgK = (int)R(3);
            t.a = Id(); t.b = R(2); t.c = R(256);
            switch (c) {
            case MC::ClientSetMemberFlags: case MC::GroupAdd: t.a = R(256); break;
            case MC::ListFindByWonId: case MC::ServerFindByWonId: t.a = R(6) * 10; break;
            case MC::FriendSetLocation: t.a = R(6); t.b = R(40); t.c = R(8); break;
            case MC::FriendStartLocate: case MC::FriendEndLocate: case MC::ServerClearFriendLocations: case MC::ServerDisconnect: t.a = R(1000); break;
            case MC::GameMatchesRoomSpec: t.a = R(16); break;
            case MC::GameStoreSummary: t.a = R(128); t.b = R(128); t.c = (long long)R(0x8000) << 8 | R(256); break;
            case MC::GameListGetOrCreateAddr: case MC::GameListGetAddr: case MC::GameListRemoveAddr:
            case MC::ServerListInsert: case MC::ServerListFind: t.a = R(6); break;
            case MC::GameListRemoveStale: t.a = R(40); t.b = R(80); break;
            case MC::ServerHasAddress: case MC::ServerAddressAdd: t.a = R(6); break;
            case MC::ServerSetListed: t.a = R(70000); break;
            case MC::ServerAddClientToGroup: t.b = R(256); t.c = R(3); break; // the k-th client
            case MC::ListAt: case MC::ServerGroupAt: case MC::ServerListAt: t.a = R(5); break;
            case MC::ListSync: case MC::ServerSyncClients: {
                std::set<unsigned short> ids;
                int k = (int)R(6);
                for (int j = 0; j < k; j++) { int r = (int)R((unsigned)sCRecs.size()); if (ids.insert(sCRecs[r].mId).second) t.mRecs.push_back(r); }
                std::sort(t.mRecs.begin(), t.mRecs.end(), [](int x, int y) { return sCRecs[x].mId < sCRecs[y].mId; });
                break;
            }
            case MC::ServerSyncGroups: {
                std::set<unsigned short> ids;
                int k = (int)R(5);
                for (int j = 0; j < k; j++) { int r = (int)R((unsigned)sGRecs.size()); if (ids.insert(sGRecs[r].mId).second) t.mRecs.push_back(r); }
                std::sort(t.mRecs.begin(), t.mRecs.end(), [](int x, int y) { return sGRecs[x].mId < sGRecs[y].mId; });
                t.a = R(2);
                break;
            }
            case MC::ServerAddClient: t.mRecs.push_back((int)R((unsigned)sCRecs.size())); break;
            case MC::ServerAddGroup: t.mRecs.push_back((int)R((unsigned)sGRecs.size())); break;
            default: break;
            }
        }
        v.push_back(t);
    }
    Step d; d.mWhat = kGet; d.mKind = 0; d.mK = 0; d.mCall = -1; d.a = d.b = d.c = 0; d.mArgKind = -1; d.mArgK = 0;
    v.push_back(d);
    Step e = d; e.mWhat = kRelease;
    v.push_back(e);
    return v;
}

static void MakeRecords()
{
    unsigned aSave = sSeed;
    sSeed = 4242;
    for (int i = 0; i < 24; i++) {
        CRec c = {(int)R(40), (unsigned long)R(0x8000) * 7, (unsigned short)R(8), (unsigned long)R(6) * 10, (unsigned long)R(9)};
        sCRecs.push_back(c);
    }
    for (int i = 0; i < 16; i++) {
        GRec g;
        g.mName = (int)R(40); g.mId = (unsigned short)R(6); g.mCount = (unsigned short)R(9);
        g.mFlags = (R(2) ? 0x800 : 0) | (R(2) ? 4 : 0) | (R(3) == 0 ? 0x80 : 0) | (R(3) == 0 ? 0x100 : 0);
        g.mCaptain = R(4) == 0 ? 0xffff : (unsigned short)R(8); g.mMax = (unsigned short)R(20);
        int k = (int)R(5);
        std::set<unsigned short> ids;
        for (int j = 0; j < k; j++) ids.insert((unsigned short)R(8));
        for (std::set<unsigned short>::iterator it = ids.begin(); it != ids.end(); ++it) g.mMembers.push_back(std::make_pair(*it, (unsigned char)R(256)));
        sGRecs.push_back(g);
    }
    for (int i = 0; i < 6; i++) {
        sAddrs[i] = ee_won_ipaddr_new();
        ee_won_ipaddr_set_host_order(sAddrs[i], 0x0a000000u + (unsigned)(i % 3), (unsigned short)(80 + i / 3));
    }
    sSeed = aSave;
}

static void PrintStep(const Step& s)
{
    printf("step %d %d %d %d %lld %lld %lld %d %d", s.mWhat, s.mKind, s.mK, s.mCall, s.a, s.b, s.c, s.mArgKind, s.mArgK);
    for (size_t i = 0; i < s.mRecs.size(); i++) printf(" %d", s.mRecs[i]);
    printf("\n");
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    bool aMutate = argc > 1 && strcmp(argv[1], "--mutate") == 0;
    MakeRecords();
    if (aPrint) {
        for (size_t i = 0; i < sCRecs.size(); i++)
            printf("crec %d %lu %u %lu %lu\n", sCRecs[i].mName, sCRecs[i].mFlags, sCRecs[i].mId, sCRecs[i].mUser, sCRecs[i].mKey);
        for (size_t i = 0; i < sGRecs.size(); i++) {
            printf("grec %d %u %u %lu %u %u", sGRecs[i].mName, sGRecs[i].mId, sGRecs[i].mCount, sGRecs[i].mFlags, sGRecs[i].mCaptain, sGRecs[i].mMax);
            for (size_t j = 0; j < sGRecs[i].mMembers.size(); j++) printf(" %u:%u", sGRecs[i].mMembers[j].first, sGRecs[i].mMembers[j].second);
            printf("\n");
        }
    }
    const int kSequences = 100000;
    int aDiffer = 0;
    unsigned long long aLines = 0;
    std::set<std::string> aSeen;
    sSeed = 777;
    for (int n = 0; n < kSequences; n++) {
        std::vector<Step> aSteps = Generate();
        unsigned aSave = sSeed;
        std::vector<std::string> a = RunSide(false, aSteps);
        std::vector<std::string> b = RunSide(true, aSteps);
        sSeed = aSave;
        aLines += a.size();
        if (a != b) {
            if (++aDiffer <= 3 && !aMutate) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s'\n           Rust '%s'\n", i, x, y); break; }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < aSteps.size(); i++) {
            std::string k = std::to_string(aSteps[i].mWhat) + ":" + std::to_string(aSteps[i].mCall);
            if (aSeen.insert(k).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t i = 0; i < aSteps.size(); i++) PrintStep(aSteps[i]);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aMutate) { printf("mutate: %d of %d sequences differ\n", aDiffer, kSequences); return 0; }
    if (!aPrint)
        printf("won_modelobj_test: %d sequences, %llu trace lines, %zu distinct steps; %d differ\n", kSequences, aLines,
               aSeen.size(), aDiffer);
    return aDiffer ? 1 : 0;
}
