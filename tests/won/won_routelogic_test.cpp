// RoutingLogic as a Rust object (R6 WONLobby round 9): the original RoutingLogic.cpp, its statements copied from the
// C++ before the port with every call on a C++ object (the servers, rooms, players, friends and games, the routing
// connection and its ops, the dialogs, the parser, the events, the Lobby* helpers) replaced by the host op that
// makes it (the unit's host runs the same calls), against ee_won's routelogic.rs over one scripted host.
//
// Each of 120,000 sequences makes the logic and makes 1 to 20 random calls on it (every public method, the control
// and component events, the twenty-one routing notices on random ops, the three completions), then frees it. The
// host answers every op from a seeded generator (the same stream for both sides) out of small pools of servers,
// rooms, players, friends, games and connections, so the identity tests (the op's server, the current server, the
// joined rooms) meet both ways. A modal op (a dialog, a status box, a message box, a yes/no question) re-enters
// the logic with another random call a quarter of the time. AddRef, Release and the texts' and lists' frees are
// not traced (the original's SmartPtr temporaries take and give back ones the Rust does not); each object's count
// is, at the end of the sequence. The traces must be the same.
//
// The first 60 sequences and every later one that makes an op, a call or a notice none before made are
// rust/crates/ee_won/src/golden_routelogic.txt, which the Rust tests replay; `--print` prints them.
// `--mutate` is for the mutation runs (the count of differing sequences, exit 0).
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include "rust/ee_won.h"

typedef EeWonRouteLogicOp Op;
typedef EeWonRouteLogicCall Call;

static int sFailures;
static int sChecks;

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;
static std::map<intptr_t, int> sRefs;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0,
                 long long f = 0, long long g = 0)
{
    char aBuffer[400];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f, g);
    sTrace.push_back(aBuffer);
}

static const intptr_t kObj = 0x7700;
static bool sRust;
static EeWonRouteLogic* sRL;
static int sDepth;
static intptr_t sNextHandle;
static int sLuck; // 0: the answers fail often; 1, 2: they mostly succeed (the connected states' branches)

static intptr_t LogicCall(Call inCall, intptr_t a = 0, intptr_t b = 0);
static void RandomCall();

// the pools
static intptr_t Server() { return 0x1000 + 0x10 * (intptr_t)(sLuck ? R(5) / 4 : R(3)); }
static intptr_t Group() { return 0x2000 + 0x10 * (intptr_t)R(4); }
static intptr_t Client() { return 0x3000 + 0x10 * (intptr_t)R(4); }
static intptr_t Friend() { return 0x4000 + 0x10 * (intptr_t)R(3); }
static intptr_t Game() { return 0x5000 + 0x10 * (intptr_t)R(3); }
static intptr_t Conn() { return 0x6000 + 0x10 * (intptr_t)(sLuck ? R(8) / 7 : R(3)); }
static intptr_t Maybe(intptr_t inValue) { return R(sLuck == 0 ? 5 : sLuck == 1 ? 12 : 40) ? inValue : 0; }
static bool Fails(unsigned inOneIn) { return R(sLuck == 0 ? inOneIn : inOneIn * 4) == 0; }
static intptr_t New(intptr_t inBase)
{
    intptr_t r = inBase + 0x10 * sNextHandle++;
    sRefs[r]++;
    return r;
}
static intptr_t Status()
{
    static const intptr_t kStatus[] = {0, 0, 0, 0, -2524, -2504, -2520, -1, 5};
    if (sLuck == 2 && R(8) != 0) return 0;
    return kStatus[R(9)];
}

static intptr_t Host(void* inObj, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    intptr_t r = 0;
    (void)inObj;
    switch (inOp) {
    case Op::AddRef: sRefs[a]++; return 0;
    case Op::Release: case Op::TextFree: case Op::ListFree: sRefs[a]--; return 0;
    case Op::BadUserIds: case Op::StatusOpMap: {
        void* aSink = reinterpret_cast<void*>(inOp == Op::BadUserIds ? a : b);
        unsigned n = R(4);
        Line("op %lld %lld = %lld", (long long)inOp, inOp == Op::BadUserIds ? 0 : (long long)a, n);
        for (unsigned i = 0; i < n * (inOp == Op::BadUserIds ? 1u : 2u); i++) {
            uint64_t v = inOp == Op::StatusOpMap && i % 2 == 0 ? (uint64_t)(R(3) * 7) : (uint64_t)R(3);
            sTrace.back() += " " + std::to_string((unsigned long long)v);
            if (sRust) ee_won_routelogic_push(aSink, v);
            else static_cast<std::vector<uint64_t>*>(aSink)->push_back(v);
        }
        return 0;
    }
    case Op::ConnSendChatTo: case Op::StatusOpSetMap: {
        intptr_t n = inOp == Op::StatusOpSetMap ? c : d;
        Line("op %lld %lld %lld %lld [", (long long)inOp, a, inOp == Op::StatusOpSetMap ? 0 : b, inOp == Op::StatusOpSetMap ? 0 : c);
        for (intptr_t i = 0; i < n; i++)
            sTrace.back() += " " + std::to_string(inOp == Op::StatusOpSetMap ? (long long)reinterpret_cast<const uint32_t*>(b)[i]
                                                                                : (long long)reinterpret_cast<const uint16_t*>(e)[i]);
        sTrace.back() += " ]";
        return 0;
    }
    // the ops that write through a pointer
    case Op::GroupSetCaptain: {
        intptr_t anOld = Maybe(Client());
        *reinterpret_cast<intptr_t*>(c) = anOld;
        r = Maybe(Client());
        Line("op %lld %lld %lld = %lld %lld", (long long)inOp, a, b, r, anOld);
        return r;
    }
    case Op::GameList:
        switch (a) {
        case 3: r = R(5) != 0; break;
        case 4: r = R(2) ? Maybe(Game()) : 0x5100 + 0x10 * (intptr_t)R(8); break;
        case 5: r = Maybe(Game()); break;
        case 7: r = R(2) ? New(0xA000) : 0; break;
        case 8: {
            bool anExisted = R(2) != 0;
            *reinterpret_cast<bool*>(c) = anExisted;
            r = Game();
            Line("op %lld %lld %lld = %lld %lld", (long long)inOp, a, b, r, anExisted);
            return r;
        }
        }
        break;
    case Op::RDataObjects: {
        intptr_t n = R(4);
        *reinterpret_cast<intptr_t*>(b) = n;
        r = New(0xE000);
        Line("op %lld %lld = %lld %lld", (long long)inOp, a, r, n);
        return r;
    }
    case Op::DirEntities: {
        intptr_t anEmpty = R(3) == 0;
        *reinterpret_cast<intptr_t*>(b) = anEmpty;
        r = New(0xE000);
        Line("op %lld %lld = %lld %lld", (long long)inOp, a, r, anEmpty);
        return r;
    }
    case Op::UserIdLookup: {
        int aStatus = (int)Status();
        *reinterpret_cast<int*>(c) = aStatus;
        r = R(3) ? 7 * (intptr_t)R(3) : 0;
        Line("op %lld %lld %lld = %lld %lld", (long long)inOp, a, b, r, aStatus);
        return r;
    }
    // the objects made (held)
    case Op::AsyncOpNew: case Op::RNew: case Op::ConnNew: case Op::ConnectOpNew: case Op::StatusOpNew: case Op::GameCreate:
        r = New(0xA000); break;
    case Op::ServerRemoveGroup: r = R(3) ? New(0xA000) : 0; break;
    case Op::ServerRemoveClient: case Op::ServerRemoveClientFromGroup: r = R(4) ? New(0xA000) : 0; break;
    case Op::RoomDirServers: case Op::DirOpRun: r = R(6) ? New(0xA000) : 0; break;
    case Op::GameText: case Op::DialogText: case Op::ParserText: r = New(0xD000); break;
    case Op::IterNew: r = New(0xE000); break;
    // the answers
    case Op::TextLength: r = R(3); break;
    case Op::Killed: r = Fails(8); break;
    case Op::OpStatus: r = Status(); break;
    case Op::OpKilled: r = Fails(5); break;
    case Op::Now: r = 1000000 + R(400); break;
    case Op::Tick: r = (intptr_t)(uint32_t)(R(2) ? 400000 + R(1000) : 5000 + R(1000)); break;
    case Op::LastInputTick: r = R(4) == 0 ? 0xfffff000 : R(100000); break;
    case Op::Pref: r = R(2); break;
    case Op::YesNo: case Op::DialogDo: r = Fails(3) ? 100002 : 100001; break;
    case Op::HasConfig: r = !Fails(6); break;
    case Op::ConfigPingCeiling: r = R(4) == 0 ? -1 : R(2) ? 0 : 900; break;
    case Op::Parser: r = R(5) ? 0x900 + 0x10 * (intptr_t)inOp : 0; break;
    case Op::AuthContext: case Op::UdpManager: case Op::AccountServers: case Op::LobbyServerList:
        r = Fails(6) ? 0 : 0x900 + 0x10 * (intptr_t)inOp; break;
    case Op::MyGame: r = Maybe(Game()); break;
    case Op::MyClient: r = R(4) ? Client() : 0; break;
    case Op::Lobby: r = a == 3 ? !Fails(4) : a == 6 ? (intptr_t)R(3) : 0; break;
    case Op::ReconnectId: r = R(100); break;
    case Op::ServerNum:
        switch (b) {
        case 0: r = 0x7000 + a; break;
        case 1: r = R(5) ? Conn() : 0; break;
        case 2: r = R(5) ? Client() : 0; break;
        case 3: r = R(4); break;
        case 4: r = R(5) == 0; break;
        case 5: r = R(60); break;
        case 8: r = R(4); break;
        case 10: r = 1000000 + R(400) - 200; break;
        default: r = R(2); break;
        }
        break;
    case Op::ServerFindByWonId: r = R(4) ? Client() : 0; break;
    case Op::ServerClient: r = R(5) ? Client() : 0; break;
    case Op::GroupFindClient: case Op::ServerAddClient: case Op::IterNext:
        r = Maybe(Client()); break;
    case Op::ServerGroup: case Op::ServerGroupAt: case Op::ServerAddGroup: r = Maybe(Group()); break;
    case Op::FriendAtServer: case Op::ListAddressIs: case Op::RIsGameData: case Op::ClientIsTeammate: case Op::GameReadSummary:
    case Op::GameReadDataObject: case Op::EventAddressIs: case Op::GameNameIsEvent: case Op::FriendAddressValid: case Op::IterHasMore:
        r = R(4) != 0; break;
    case Op::ListNum: r = b == 0 ? (intptr_t)R(4) : R(4) == 0; break;
    case Op::ListAt: r = Server(); break;
    case Op::GroupNum:
        switch (b) {
        case 0: r = R(4); break;
        case 3: r = R(4) == 0 ? 49 + R(3) : R(60); break;
        case 6: r = R(5); break;
        case 7: r = R(3); break;
        case 9: r = 0x7100 + a; break;
        default: r = R(2); break;
        }
        break;
    case Op::ClientNum:
        switch (b) {
        case 0: r = R(5); break;
        case 1: case 13: r = R(3) * 7; break;
        case 7: case 9: r = R(4); break;
        default: r = R(2); break;
        }
        break;
    case Op::FriendNum:
        switch (b) {
        case 0: r = R(3) == 0 ? 4 : R(2); break;
        case 1: r = 1000000 + R(400) - 200; break;
        case 2: r = R(3) * 7; break;
        case 4: r = R(3); break;
        case 5: r = R(3); break;
        default: r = R(2); break;
        }
        break;
    case Op::FindFriend: case Op::AddFriendOfClient: r = R(4) ? Friend() : 0; break;
    case Op::AddFriendNamed: case Op::FriendAt: r = Maybe(Friend()); break;
    case Op::FriendCount: r = R(4) == 0 ? 255 : R(3); break;
    case Op::GameNum:
        switch (b) {
        case 2: r = R(3); break;
        case 3: r = R(4); break;
        case 8: r = R(3) == 0 ? 0xfffe : R(4) == 0 ? 0xf800 : R(2) ? 100 * (intptr_t)R(2) : R(1200); break;
        case 9: r = R(2) ? 1 : R(4); break;
        case 7: r = R(4) != 0; break;
        default: r = R(3) == 0; break;
        }
        break;
    case Op::RNum: r = b == 0 && R(6) == 0 ? 0xffff : b == 29 ? R(2) : b == 27 ? (intptr_t)(R(2) ? R(3) * 6 : 9 + R(3)) : b == 10 ? (R(2) ? 0x100 : 0) | (R(2) ? 4 : 0) | (R(2) ? 0x10 : 0) : R(4); break;
    case Op::RRecipient: r = R(4) == 0 ? -1 : R(2) ? (intptr_t)R(4) : 0x4002 + (intptr_t)R(2); break;
    case Op::RConnection: r = Conn(); break;
    case Op::DataObjectLink: r = R(4); break;
    case Op::OpSocket: r = R(3) ? Conn() : 0x6100; break;
    case Op::ConnDo: r = b == 5 ? !Fails(5) : 0; break;
    case Op::ParserNum:
        switch (b) {
        case 0: r = (R(3) ? 4 : 0) | R(4) | (R(2) ? 0x10 : 0); break;
        case 1: r = R(26); break;
        case 2: r = R(100); break;
        case 3: r = R(4) ? Client() : 0; break;
        case 4: r = 0x7200; break;
        }
        break;
    case Op::EventNum:
        switch (b) {
        case 1: case 9: r = Maybe(Client()); break;
        case 3: case 12: r = Maybe(Friend()); break;
        case 4: r = R(7); break;
        case 5: r = R(2) ? 0 : 3; break;
        case 8: r = (R(4) ? 4 : 0) | R(4); break;
        case 10: r = R(20); break;
        case 11: r = R(4) == 0 ? 0xffff : R(4); break;
        case 13: r = 0x7300; break;
        default: r = R(4); break;
        }
        break;
    case Op::EventServer: case Op::FriendServer: case Op::StatusOpServer: case Op::ServerListFind: r = Maybe(Server()); break;
    case Op::DialogNum: r = a == 2 ? (intptr_t)R(100) : R(2); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d, e);
    sTrace.back() += " = " + std::to_string((long long)r);
    // a modal op re-enters the logic
    if (sDepth == 0 && (inOp == Op::DialogDo || inOp == Op::RunWithStatus || inOp == Op::YesNo || inOp == Op::MessageBox ||
                        inOp == Op::ErrorDialog) && R(4) == 0) {
        sDepth++;
        Line("reenter");
        RandomCall();
        sDepth--;
    }
    return r;
}

static intptr_t I(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0, intptr_t e = 0)
{
    return Host((void*)kObj, inOp, a, b, c, d, e);
}

// ---- the original, its calls the host's ops

// SmartPtr<T> as the original's: the old one released, the new one taken
struct Ref {
    intptr_t p;
    Ref() : p(0) {}
    Ref(intptr_t inP, bool inTake) : p(inP) { if (inTake && p != 0) I(Op::AddRef, p); }
    ~Ref() { if (p != 0) I(Op::Release, p); }
    Ref& operator=(intptr_t inP)
    {
        if (p != inP) {
            if (p != 0) I(Op::Release, p);
            p = inP;
            if (p != 0) I(Op::AddRef, p);
        }
        return *this;
    }
    intptr_t get() const { return p; }
private:
    Ref(const Ref&);
};
// a GUIString (0: empty)
struct Txt {
    intptr_t p;
    explicit Txt(intptr_t inP = 0) : p(inP) {}
    ~Txt() { if (p != 0) I(Op::TextFree, p); }
    Txt& operator=(intptr_t inP) { if (p != 0) I(Op::TextFree, p); p = inP; return *this; }
private:
    Txt(const Txt&);
};

#define SN(s, f) I(Op::ServerNum, s, f)
#define GN(g, f) I(Op::GroupNum, g, f)
#define CN(c, f) I(Op::ClientNum, c, f)
#define FN(f, k) I(Op::FriendNum, f, k)
#define GMN(g, k) I(Op::GameNum, g, k)
#define RN(r, f) I(Op::RNum, r, f)
#define RES(name) ((intptr_t)EeWonLobbyRes::name)
#define EVT(e, f) I(Op::EventNum, e, f)

struct OrigRouting;
static OrigRouting* sOR;

struct OrigRouting {
    long long mLastServerRefresh;
    bool mSuppressIdle, mIdle, mHaveGameList, mWantToBeModerator;
    Ref mGameListWaitOp, mCurrentServer, mPendingServer, mChatGroup, mGameGroup;

    OrigRouting() : mLastServerRefresh(0), mSuppressIdle(false), mIdle(false), mHaveGameList(false), mWantToBeModerator(false) {}
    ~OrigRouting() {} // the members' references given back, the game room's first

    struct PendingServerCleanup {
        OrigRouting* mLogic;
        PendingServerCleanup(OrigRouting* inLogic) : mLogic(inLogic) {}
        ~PendingServerCleanup() { mLogic->ClearPendingServer(); }
    };

    bool IsConnecting() { return mPendingServer.get() != 0; }
    bool IsConnected() { return mCurrentServer.get() != 0; }
    intptr_t GetConnection()
    {
        if (mCurrentServer.get() == 0) return 0;
        return SN(mCurrentServer.get(), 1);
    }
    intptr_t GetClientList()
    {
        if (mCurrentServer.get() != 0) return SN(mCurrentServer.get(), 0);
        return 0;
    }
    intptr_t GetMyClient() { return mCurrentServer.get() == 0 ? 0 : SN(mCurrentServer.get(), 2); }

    bool HostGame(intptr_t inGame)
    {
        if (IsConnecting() || mGameGroup.get() != 0) return false;
        intptr_t aServer = mCurrentServer.get();
        if (aServer == 0) aServer = GetDefaultServer();
        if (aServer == 0) return false;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(aServer, false)) return false;
        unsigned long aFlags = 0;
        if (GMN(inGame, 0)) aFlags = 0x100;
        if (GMN(inGame, 1)) aFlags |= 0x80;
        unsigned short aGroupId = 0;
        {
            Txt aName(I(Op::GameText, inGame, 0));
            Txt aPassword(I(Op::GameText, inGame, 1));
            if (!CreateGroup(mPendingServer.get(), aName.p, aPassword.p, aFlags, aGroupId)) return false;
        }
        JoinedGroup(aGroupId, true);
        if (GetConnection() == 0) return false;
        I(Op::GameDo, inGame, 1, aGroupId);
        I(Op::ServerDo, aServer, 2);
        if (mGameGroup.get() != 0) I(Op::GameDo, inGame, 2, GN(mGameGroup.get(), 9));
        return true;
    }
    bool JoinGame(intptr_t inGame)
    {
        if (IsConnecting() || mGameGroup.get() != 0 || mCurrentServer.get() == 0 || GMN(inGame, 2) != 2) return false;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(mCurrentServer.get(), false)) return false;
        if (!JoinGroup(mPendingServer.get(), (unsigned short)GMN(inGame, 3))) return false;
        JoinedGroup((unsigned short)GMN(inGame, 3), true);
        if (mGameGroup.get() != 0) I(Op::GameDo, inGame, 2, GN(mGameGroup.get(), 9));
        return true;
    }
    void ClearPendingServer()
    {
        if (mPendingServer.get() != 0 && mPendingServer.get() != mCurrentServer.get()) I(Op::ServerDo, mPendingServer.get(), 1);
        mPendingServer = 0;
    }
    int GetRoomSpec(unsigned short inGroupId)
    {
        int32_t aChat = mChatGroup.get() != 0 ? (int32_t)(unsigned short)GN(mChatGroup.get(), 0) : -1;
        int32_t aGame = mGameGroup.get() != 0 ? (int32_t)(unsigned short)GN(mGameGroup.get(), 0) : -1;
        return ee_won_routing_room_spec(inGroupId, aChat, aGame);
    }
    int GetRoomSpec(intptr_t inGroup, intptr_t inServer)
    {
        if (inServer == mCurrentServer.get() && inGroup != 0) return GetRoomSpec((unsigned short)GN(inGroup, 0));
        return 0;
    }
    intptr_t GetGroup(int inRoomSpec)
    {
        switch (ee_won_routing_group_of_spec(inRoomSpec)) {
        case 1: return mChatGroup.get();
        case 2: return mGameGroup.get();
        }
        return 0;
    }
    intptr_t GetServer(intptr_t inOp)
    {
        if (mCurrentServer.get() != 0) {
            intptr_t anOpConnection = I(Op::RConnection, inOp);
            if (anOpConnection == SN(mCurrentServer.get(), 1)) return mCurrentServer.get();
        }
        if (mPendingServer.get() != 0) {
            intptr_t anOpConnection = I(Op::RConnection, inOp);
            if (anOpConnection == SN(mPendingServer.get(), 1)) return mPendingServer.get();
        }
        return 0;
    }
    intptr_t GetDefaultServer()
    {
        if (!I(Op::HasConfig)) return 0;
        std::vector<intptr_t> aList;
        std::vector<EeWonServerPick> aPicks;
        for (size_t anItr = 0; (intptr_t)anItr < I(Op::ListNum, 0); ++anItr) {
            intptr_t aServer = I(Op::ListAt, (intptr_t)anItr);
            EeWonServerPick aPick;
            aPick.last_used = SN(aServer, 4) != 0;
            aPick.clients = (int)SN(aServer, 5);
            aPick.language = SN(aServer, 6) != 0;
            aList.push_back(aServer);
            aPicks.push_back(aPick);
            if (aPick.last_used) break; // the original returned it at once
        }
        int64_t anIndex = ee_won_routing_default_server(aPicks.empty() ? NULL : &aPicks[0], aPicks.size());
        return anIndex >= 0 ? aList[(size_t)anIndex] : 0;
    }
    void SendToCaptain(intptr_t inData)
    {
        intptr_t aConnection = GetConnection();
        if (aConnection != 0 && mGameGroup.get() != 0) I(Op::ConnSendData, aConnection, inData, (unsigned short)GN(mGameGroup.get(), 6));
    }
    void BroadcastToGame(intptr_t inData)
    {
        intptr_t aConnection = GetConnection();
        if (aConnection != 0 && mGameGroup.get() != 0) I(Op::ConnSendData, aConnection, inData, (unsigned short)GN(mGameGroup.get(), 0));
    }
    void SendToClient(intptr_t inClient, intptr_t inData)
    {
        intptr_t aConnection = GetConnection();
        if (aConnection != 0 && mGameGroup.get() != 0) I(Op::ConnSendData, aConnection, inData, (unsigned short)CN(inClient, 0));
    }
    void AdvertiseGame(intptr_t inGame)
    {
        intptr_t aConnection = GetConnection();
        if (aConnection == 0 || mGameGroup.get() == 0) return;
        I(Op::ConnAdvertise, aConnection, inGame, (unsigned short)GN(mGameGroup.get(), 0));
    }
    void RemoveGameAdvertisement()
    {
        intptr_t aConnection = GetConnection();
        if (aConnection == 0 || mGameGroup.get() == 0) return;
        I(Op::ConnDo, aConnection, 10, (unsigned short)GN(mGameGroup.get(), 0));
    }
    void KickClient(intptr_t inClient, bool inBan)
    {
        if (mGameGroup.get() == 0) return;
        intptr_t aConnection = GetConnection();
        if (aConnection == 0) return;
        intptr_t aGroup = (unsigned short)GN(mGameGroup.get(), 0);
        I(Op::ConnBan, aConnection, aGroup | 1 << 16, !inBan, 0, (unsigned short)CN(inClient, 0));
    }
    void SetGameInProgress(bool inClose)
    {
        intptr_t aConnection = GetConnection();
        if (aConnection == 0 || mGameGroup.get() == 0) return;
        I(Op::ConnSetGroupFlags, aConnection, (unsigned short)GN(mGameGroup.get(), 0), 2, inClose ? 2 : 0);
    }
    void MemberFlagsChanged(intptr_t inOp, intptr_t inServer)
    {
        int aGroupId = (unsigned short)RN(inOp, 0);
        intptr_t aGroup = I(Op::ServerGroup, inServer, aGroupId);
        if (aGroup == 0) return;
        intptr_t aClient = I(Op::GroupFindClient, aGroup, (unsigned short)RN(inOp, 1));
        if (aClient == 0) return;
        unsigned char aFlags = (unsigned char)RN(inOp, 14);
        I(Op::ClientDo, aClient, 1, aFlags, GN(aGroup, 1));
        if (inServer != mCurrentServer.get()) return;
        unsigned int aRoomSpec = GetRoomSpec(aGroup, inServer);
        if (aRoomSpec != 0) I(Op::EvClientChanged, aClient, aRoomSpec, 2);
    }
    void ClientFlagsChanged(intptr_t inOp, intptr_t inServer)
    {
        intptr_t aClient = I(Op::ServerClient, inServer, (unsigned short)RN(inOp, 1));
        if (aClient == 0) return;
        unsigned long aFlags = (unsigned long)RN(inOp, 20);
        I(Op::ClientDo, aClient, 2, (intptr_t)aFlags);
        unsigned int aRoomSpec = 4;
        I(Op::EvClientChanged, aClient, aRoomSpec, 2);
        if (!CN(aClient, 2)) return;
        intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(aClient, 1));
        if (aFriend == 0) return;
        bool aFriendAnonymous = FN(aFriend, 3) != 0;
        if (aFriendAnonymous == (CN(aClient, 3) != 0)) return;
        I(Op::FriendDo, aFriend, 1, CN(aClient, 3));
        I(Op::EvFriendChanged, aFriend, 2);
    }
    bool ShouldLocateFriend(intptr_t inFriend, unsigned long inNow)
    {
        bool aLocating = (FN(inFriend, 0) & 4) != 0;
        bool aHere = !aLocating && mCurrentServer.get() != 0 && I(Op::FriendAtServer, inFriend, mCurrentServer.get());
        size_t aCount = (size_t)I(Op::ListNum, 0);
        uint32_t aLast = (uint32_t)FN(inFriend, 1);
        return ee_won_routing_should_locate_friend(aLocating, aHere, aCount, mCurrentServer.get() != 0, (uint32_t)inNow, aLast);
    }
    bool AnnounceClientLocation(intptr_t inClient, bool inForce, int inRoomSpec, bool inFindByUserId)
    {
        if (mCurrentServer.get() == 0 || inClient == 0) return false;
        intptr_t aChatRoom = 0; // the rooms whose names the GUIStrings took
        intptr_t aGameRoom = 0;
        if (inFindByUserId && CN(inClient, 1) != 0) {
            intptr_t aClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)CN(inClient, 1));
            if (aClient != 0) inClient = aClient;
            else goto announce;
        }
        if (CN(inClient, 6)) {
            int aKey = (unsigned short)CN(inClient, 7);
            intptr_t aGroup = I(Op::ServerGroup, mCurrentServer.get(), aKey);
            if (aGroup != 0) aChatRoom = aGroup;
        }
        if (CN(inClient, 8)) {
            int aKey = (unsigned short)CN(inClient, 9);
            intptr_t aGroup = I(Op::ServerGroup, mCurrentServer.get(), aKey);
            if (aGroup != 0) aGameRoom = aGroup;
        }
announce:
        if ((aChatRoom ? GN(aChatRoom, 7) : 0) == 0 && (aGameRoom ? GN(aGameRoom, 7) : 0) == 0 && !inForce) return false;
        int aRoomSpec = inRoomSpec | 4;
        I(Op::EvAnnounceClient, inClient, aChatRoom, aGameRoom, aRoomSpec);
        return true;
    }
    void PeerData(intptr_t inOp, intptr_t inServer)
    {
        if (inServer != mCurrentServer.get() || mGameGroup.get() == 0) return;
        intptr_t aClient = I(Op::GroupFindClient, mGameGroup.get(), (unsigned short)RN(inOp, 1));
        if (aClient == 0) return;
        unsigned short anId = (unsigned short)CN(aClient, 0);
        if (anId == (unsigned short)SN(mCurrentServer.get(), 3)) return;
        I(Op::HandleStaging, inOp, aClient);
    }
    void ClientJoinedGroup(intptr_t inOp, intptr_t inServer)
    {
        int aKey = (unsigned short)RN(inOp, 0);
        intptr_t aGroup = I(Op::ServerGroup, inServer, aKey);
        if (aGroup == 0) return;
        intptr_t aClient = I(Op::ServerClient, inServer, (unsigned short)RN(inOp, 1));
        if (aClient == 0) return;
        unsigned char aFlags = (unsigned char)RN(inOp, 14);
        I(Op::ServerAddClientToGroup, inServer, aClient, aFlags, aGroup);
        unsigned int aRoomSpec = GetRoomSpec(aGroup, inServer);
        if (aRoomSpec != 0) I(Op::EvClientChanged, aClient, aRoomSpec, 0);
        I(Op::EvGroupChanged, aGroup, inServer, 2);
        UpdateFriend(aClient);
    }
    void ClientLeftGroup(intptr_t inOp, intptr_t inServer)
    {
        intptr_t aGroup = I(Op::ServerGroup, inServer, (unsigned short)RN(inOp, 0));
        if (aGroup == 0) return;
        Ref aClient(I(Op::ServerRemoveClientFromGroup, inServer, (unsigned short)RN(inOp, 1), aGroup), false);
        if (aClient.get() == 0) return;
        unsigned int aRoomSpec = GetRoomSpec(aGroup, inServer);
        if (aRoomSpec != 0) {
            I(Op::EvClientChanged, aClient.get(), aRoomSpec, 1);
            if (aRoomSpec & 2) I(Op::ClientLeftGame, aClient.get());
        }
        I(Op::EvGroupChanged, aGroup, inServer, 2);
        UpdateFriend(aClient.get());
    }
    void GroupCreated(intptr_t inOp, intptr_t inServer)
    {
        intptr_t aGroup = I(Op::ServerAddGroup, inServer, inOp);
        if (aGroup != 0) I(Op::EvGroupChanged, aGroup, inServer, 0);
    }
    void GroupDeleted(intptr_t inOp, intptr_t inServer)
    {
        Ref aGroup(I(Op::ServerRemoveGroup, inServer, (unsigned short)RN(inOp, 0)), false);
        if (aGroup.get() != 0) I(Op::EvGroupChanged, aGroup.get(), inServer, 1);
    }
    void UpdateFriend(intptr_t inClient)
    {
        if (!CN(inClient, 2)) return;
        intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(inClient, 1));
        if (aFriend == 0) return;
        I(Op::FriendDo, aFriend, 2, inClient);
        I(Op::EvFriendChanged, aFriend, 2);
    }
    void AnnounceFriendLocation(intptr_t inFriend, int inRoomSpec)
    {
        if (inFriend == 0) return;
        if (mCurrentServer.get() != 0 && I(Op::FriendAtServer, inFriend, mCurrentServer.get())) {
            intptr_t aClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)FN(inFriend, 2));
            if (aClient != 0 && AnnounceClientLocation(aClient, false, inRoomSpec, false)) return;
        }
        I(Op::EvAnnounceFriend, inFriend, inRoomSpec, 2);
    }
    void ClientJoinedServer(intptr_t inOp, intptr_t inServer)
    {
        intptr_t aClient = I(Op::ServerAddClient, inServer, inOp);
        intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(aClient, 1));
        if (aFriend == 0) return;
        I(Op::ClientDo, aClient, 3, 1);
        I(Op::FriendDo, aFriend, 3, inServer, 0);
        I(Op::FriendDo, aFriend, 1, CN(aClient, 3));
        I(Op::EvFriendChanged, aFriend, 2);
        if (inServer != mCurrentServer.get() || CN(aClient, 3)) return;
        if (I(Op::Pref, 1)) I(Op::Sound, RES(LobbyGlobal_FriendJoined_Sound));
        I(Op::EvAnnounceFriend, aFriend, 4, 0);
    }
    void ClientLeftServer(intptr_t inOp, intptr_t inServer)
    {
        Ref aClient(I(Op::ServerRemoveClient, inServer, (unsigned short)RN(inOp, 1)), false);
        if (aClient.get() != 0) {
        if (!CN(aClient.get(), 2)) return;
        intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(aClient.get(), 1));
        if (aFriend == 0) return;
        I(Op::FriendDo, aFriend, 4);
        I(Op::EvFriendChanged, aFriend, 2);
        if (inServer == mCurrentServer.get() && !CN(aClient.get(), 3)) {
        if (I(Op::Pref, 1)) I(Op::Sound, RES(LobbyGlobal_FriendLeft_Sound));
        I(Op::EvAnnounceFriend, aFriend, 4, 1);
        }
        return;
        }
    }
    void GroupInvitation(intptr_t inOp, intptr_t inServer)
    {
        bool anInvited = RN(inOp, 29) != 0;
        if (inServer != mCurrentServer.get()) return;
        intptr_t aClient = I(Op::ServerClient, inServer, (unsigned short)RN(inOp, 1));
        if (aClient == 0 || CN(aClient, 4) || CN(aClient, 5)) return;
        intptr_t aGame = I(Op::GameList, 5, (unsigned short)RN(inOp, 0));
        if (aGame == 0) return;
        I(Op::GameDo, aGame, 3, anInvited);
        I(Op::EvGameChanged, aGame, 2);
        if (I(Op::Pref, 2)) I(Op::Sound, RES(LobbyGlobal_InviteRecv_Sound));
        I(Op::EvInvitationRecv, aGame, aClient, anInvited, inOp);
    }
    void GroupJoinAttempt(intptr_t inOp, intptr_t inServer)
    {
        if (inServer != mCurrentServer.get() || mGameGroup.get() == 0) return;
        unsigned short anOpGroup = (unsigned short)RN(inOp, 0);
        if (anOpGroup != (unsigned short)GN(mGameGroup.get(), 0)) return;
        intptr_t aClient = I(Op::ServerClient, inServer, (unsigned short)RN(inOp, 1));
        if (aClient == 0 || CN(aClient, 4) || CN(aClient, 5)) return;
        if (I(Op::Pref, 2)) I(Op::Sound, RES(LobbyGlobal_AskToJoinRecv_Sound));
        I(Op::EvJoinAttempt, aClient, inOp);
    }
    void Reset()
    {
        if (mSuppressIdle || !IsConnected()) return;
        uint32_t aLastInputTick = (uint32_t)I(Op::LastInputTick);
        if ((uint32_t)I(Op::Tick) - aLastInputTick > 300000) {
            if (mIdle) return;
            intptr_t aConnection = GetConnection();
            if (aConnection != 0) {
                I(Op::ConnSetClientFlags, aConnection, 8, 8, 0, 0);
                mIdle = true;
            }
        } else if (mIdle) {
            intptr_t aConnection = GetConnection();
            if (aConnection != 0) {
                I(Op::ConnSetClientFlags, aConnection, 8, 0, 0, 0);
                mIdle = false;
            }
        }
    }
    void PeerChat(intptr_t inOp, intptr_t inServer)
    {
        intptr_t aFront = I(Op::RRecipient, inOp); // Recipients(): empty, or its front
        unsigned short aFlags = (unsigned short)RN(inOp, 10);
        if (inServer != mCurrentServer.get() || (mChatGroup.get() == 0 && mGameGroup.get() == 0) || aFront < 0) return;
        int aRecipientId = (unsigned short)aFront;
        intptr_t aSender = I(Op::ServerClient, inServer, (unsigned short)RN(inOp, 1));
        if (aSender == 0 || CN(aSender, 4) || CN(aSender, 5)) return;
        intptr_t aTarget = 0;
        unsigned int aRoomSpec = 4;
        if (aFlags & 0x100) {
            if (mGameGroup.get() == 0) return;
            aRoomSpec = 0x16;
        } else if (aRecipientId < 0x4002) {
            aRoomSpec = GetRoomSpec((unsigned short)aRecipientId);
            if (aRoomSpec == 0) return;
        } else {
            aTarget = I(Op::ServerClient, inServer, aRecipientId);
            if (aTarget == 0) return;
        }
        bool aToMe = false;
        if ((aFlags & 4) && aTarget != 0) {
            unsigned short anId = (unsigned short)CN(aTarget, 0);
            aToMe = anId == (unsigned short)SN(inServer, 3);
        }
        if (aToMe) {
            if (I(Op::Pref, 1)) I(Op::Sound, RES(LobbyGlobal_WhisperRecv_Sound));
            intptr_t aParser = I(Op::Parser);
            if (aParser != 0) I(Op::ParserSetInput, aParser, aSender);
        }
        if ((aFlags & 0x10) && I(Op::Pref, 3)) I(Op::Sound, RES(LobbyGlobal_Error_Sound));
        I(Op::EvRecvChat, aRoomSpec, aFlags, aSender, inOp, aTarget);
    }
    void ServerAlert(intptr_t inOp)
    {
        if (I(Op::Pref, 3)) I(Op::Sound, RES(LobbyGlobal_Error_Sound));
        I(Op::EvAlert, 0, inOp);
    }
    void ServerShutdownStarted(intptr_t inOp)
    {
        if (I(Op::Pref, 3)) I(Op::Sound, RES(LobbyGlobal_Error_Sound));
        I(Op::EvAlert, 1, inOp);
    }
    void ServerShutdownAborted(intptr_t inOp)
    {
        if (I(Op::Pref, 3)) I(Op::Sound, RES(LobbyGlobal_Error_Sound));
        I(Op::EvAlert, 2, inOp);
    }
    static int ModeratedRoomSpec(int inRoomSpec) { return inRoomSpec | 4; }
    void YouWereMuted(intptr_t inOp, intptr_t inServer)
    {
        int aKind = RN(inOp, 29) != 0 ? 2 : 3;
        unsigned short aGroupId = (unsigned short)RN(inOp, 0);
        int aRoomSpec;
        intptr_t aRoomName; // the server or group whose name the GUIString took
        int aIsGroup;
        if (aGroupId == 0xffff) {
            aRoomSpec = 0;
            aRoomName = inServer; aIsGroup = 0;
        } else if (mGameGroup.get() != 0 && aGroupId == (unsigned short)GN(mGameGroup.get(), 0)) {
            aRoomSpec = 2;
            aRoomName = mGameGroup.get(); aIsGroup = 1;
        } else if (mChatGroup.get() != 0 && aGroupId == (unsigned short)GN(mChatGroup.get(), 0)) {
            aRoomSpec = 1;
            aRoomName = mChatGroup.get(); aIsGroup = 1;
        } else return;
        intptr_t aClient = SN(inServer, 2);
        I(Op::EvModerated, aClient, ModeratedRoomSpec(aRoomSpec), aRoomName, aKind | aIsGroup << 8, inOp);
    }
    void LeaveGame()
    {
        if (mGameGroup.get() == 0) return;
        if (mChatGroup.get() == 0) {
            mGameGroup = 0;
            LeaveServer();
        } else {
            intptr_t aConnection = GetConnection();
            if (aConnection != 0) I(Op::ConnDo, aConnection, 8, (unsigned short)GN(mGameGroup.get(), 0));
            mGameGroup = 0;
        }
        I(Op::EvLeftRoom, 6, false, true);
    }
    void Suspend()
    {
        intptr_t aConnection = GetConnection();
        if (aConnection == 0) return;
        if (mChatGroup.get() != 0) {
            I(Op::ConnDo, aConnection, 8, (unsigned short)GN(mChatGroup.get(), 0));
            mChatGroup = 0;
            I(Op::EvLeftRoom, 5, false, true);
        }
        I(Op::ConnSetClientFlags, aConnection, 0, 0, 0xffffffff, 0xfffbffff);
    }
    void Resume()
    {
        intptr_t aConnection = GetConnection();
        if (aConnection != 0) I(Op::ConnSetClientFlags, aConnection, 0x40, 0x40, 0, 0);
        I(Op::Lobby, 3, false);
        LeaveServer();
        JoinDefaultRoom(true);
    }
    void LeaveServer()
    {
        if (mCurrentServer.get() != 0) {
            I(Op::ServerDo, mCurrentServer.get(), 1);
            I(Op::Lobby, 1);
            I(Op::EvLeftRoom, 4, false, false);
        }
        if (mGameListWaitOp.get() != 0) I(Op::OpKill, mGameListWaitOp.get());
        mHaveGameList = false;
        mCurrentServer = 0;
        mChatGroup = 0;
        I(Op::GameList, 1);
        I(Op::EvSyncGameList);
        if (mGameGroup.get() != 0) {
            mGameGroup = 0;
            I(Op::Lobby, 2, true);
        }
    }
    void GroupCaptainChanged(intptr_t inOp, intptr_t inServer)
    {
        int aKey = (unsigned short)RN(inOp, 0);
        intptr_t aGroup = I(Op::ServerGroup, inServer, aKey);
        if (aGroup == 0) return;
        intptr_t anOldCaptain = 0;
        unsigned short aCaptain = (unsigned short)RN(inOp, 7);
        intptr_t aNewCaptain = I(Op::GroupSetCaptain, aGroup, aCaptain, (intptr_t)&anOldCaptain);
        if (inServer != mCurrentServer.get()) return;
        unsigned int aRoomSpec = GetRoomSpec(aGroup, inServer);
        if (aRoomSpec == 0) return;
        if (anOldCaptain != 0) I(Op::EvClientChanged, anOldCaptain, aRoomSpec, 2);
        if (aNewCaptain != 0) I(Op::EvClientChanged, aNewCaptain, aRoomSpec, 2);
        if (aGroup == mGameGroup.get() && anOldCaptain != 0) I(Op::Lobby, 2, true);
    }
    void DataObjectDeleted(intptr_t inOp, intptr_t inServer)
    {
        if (inServer != mCurrentServer.get() || !I(Op::RIsGameData, inOp)) return;
        Ref aGame(I(Op::GameList, 7, (unsigned short)RN(inOp, 5)), false);
        if (aGame.get() != 0) {
            I(Op::EvGameChanged, aGame.get(), 1);
            return;
        }
    }
    void DataObjectCreated(intptr_t inOp, intptr_t inServer)
    {
        if (inServer != mCurrentServer.get() || !I(Op::RIsGameData, inOp)) return;
        int aLinkId = (unsigned short)RN(inOp, 5);
        intptr_t aGroup = I(Op::ServerGroup, inServer, aLinkId);
        if (aGroup == 0) return;
        bool wasExisting = false;
        intptr_t aGame = I(Op::GameList, 8, (unsigned short)GN(aGroup, 0), (intptr_t)&wasExisting);
        if (!I(Op::GameReadSummary, aGame, inOp)) {
            Ref aHoldGame(aGame, true);
            { Ref aRemoved(I(Op::GameList, 7, (unsigned short)GN(aGroup, 0)), false); }
            if (wasExisting) I(Op::EvGameChanged, aGame, 1);
            return;
        }
        if (!wasExisting) {
            I(Op::GameDo, aGame, 4, aGroup);
            if (mGameGroup.get() != 0) {
                intptr_t aMyGame = I(Op::MyGame);
                if (aMyGame != 0 && GMN(aMyGame, 4)) {
                    unsigned short aGameId = (unsigned short)GMN(aGame, 3);
                    if (aGameId == (unsigned short)GMN(aMyGame, 3)) I(Op::GameDo, aGame, 3, true);
                }
            }
            if (I(Op::Pref, 4)) I(Op::PingGame, aGame);
        }
        I(Op::EvGameChanged, aGame, wasExisting ? 2 : 0);
    }
    static bool IsGameRoomSpec(unsigned long inRoomSpec) { return (inRoomSpec >> 1) & 1; }
    static bool IsChatRoomSpec(unsigned long inRoomSpec) { return inRoomSpec & 1; }
    void YouWereBanned(intptr_t inOp, intptr_t inServer)
    {
        bool aBanned = RN(inOp, 29) != 0;
        unsigned short aGroupId = (unsigned short)RN(inOp, 0);
        if (!aBanned) return;
        int aKind;
        unsigned long aDuration = (unsigned long)RN(inOp, 27);
        aKind = aDuration != 0 && aDuration < 10 ? 1 : 0;
        intptr_t aRoomName;
        int aIsGroup;
        int aRoomSpec;
        if (aGroupId == 0xffff) {
            aRoomSpec = 0;
            aRoomName = inServer; aIsGroup = 0;
        } else if (mGameGroup.get() != 0 && aGroupId == (unsigned short)GN(mGameGroup.get(), 0)) {
            aRoomSpec = 2;
            aRoomName = mGameGroup.get(); aIsGroup = 1;
        } else if (mChatGroup.get() != 0 && aGroupId == (unsigned short)GN(mChatGroup.get(), 0)) {
            aRoomSpec = 1;
            aRoomName = mChatGroup.get(); aIsGroup = 1;
        } else return;
        intptr_t aMyClient = SN(inServer, 2);
        if (IsGameRoomSpec(aRoomSpec)) {
            I(Op::ClientKicked, aMyClient, !aKind);
            return;
        }
        I(Op::EvModerated, aMyClient, aRoomSpec | 4, aRoomName, aKind | aIsGroup << 8, inOp);
        if (IsChatRoomSpec(aRoomSpec)) {
            if (mGameGroup.get() == 0) LeaveServer();
            else {
                mChatGroup = 0;
                I(Op::EvLeftRoom, 5, false, false);
            }
        } else {
            bool hadGameGroup = mGameGroup.get() != 0;
            if (hadGameGroup) I(Op::Lobby, 3, false);
            LeaveServer();
            if (hadGameGroup) I(Op::Lobby, 4);
        }
    }
    bool JoinDefaultRoom(bool inShowStatus)
    {
        intptr_t aServer = GetDefaultServer();
        if (aServer == 0) return false;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(aServer, inShowStatus)) return false;
        intptr_t aBestGroup = 0;
        size_t anItr = 0;
        while ((intptr_t)anItr < SN(aServer, 8)) {
            intptr_t aGroup = I(Op::ServerGroupAt, aServer, (intptr_t)anItr);
            if (GN(aGroup, 2)) {
                if (GN(aGroup, 8)) {
                    aBestGroup = aGroup;
                    break;
                }
                bool aBusier = false;
                if (aBestGroup != 0) {
                    int aClients = (int)GN(aGroup, 3);
                    aBusier = aClients > (int)GN(aBestGroup, 3) && (int)GN(aGroup, 3) < 50;
                }
                if (aBestGroup == 0 || aBusier) aBestGroup = aGroup;
            }
            anItr++;
        }
        int aGroupId = 0;
        if (aBestGroup != 0) aGroupId = (unsigned short)GN(aBestGroup, 0);
        if (!JoinGroup(mPendingServer.get(), aGroupId)) return false;
        JoinedGroup((unsigned short)aGroupId, false);
        return true;
    }
    void RefreshGameList()
    {
        if (!I(Op::Pref, 8)) return;
        I(Op::GameList, 2);
        while (I(Op::GameList, 3)) {
            intptr_t aGame = I(Op::GameList, 4);
            if (!GMN(aGame, 5)) I(Op::PingGame, aGame);
        }
        I(Op::BroadcastLobby, 100058);
    }
    void JoinedGroup(unsigned short inGroupId, bool inGameGroup)
    {
        intptr_t anOldGroup = (inGameGroup ? mGameGroup : mChatGroup).get();
        if (mPendingServer.get() != 0 && mPendingServer.get() != mCurrentServer.get()) {
            LeaveServer();
            mIdle = false;
            mSuppressIdle = false;
            mHaveGameList = false;
            mCurrentServer = mPendingServer.get();
            intptr_t aConnection = GetConnection();
            if (aConnection != 0) I(Op::ConnDo, aConnection, 4);
        } else if (anOldGroup != 0 && mCurrentServer.get() != 0 && SN(mCurrentServer.get(), 1) != 0) {
            intptr_t aConnection = SN(mCurrentServer.get(), 1); // the object before the arguments (C++17)
            I(Op::ConnDo, aConnection, 8, (unsigned short)GN(anOldGroup, 0));
        }
        if (mCurrentServer.get() == 0) return;
        intptr_t aGroup = I(Op::ServerGroup, mCurrentServer.get(), inGroupId);
        if (inGameGroup) mGameGroup = aGroup; else mChatGroup = aGroup;
        if (aGroup == 0) return;
        if (GN(aGroup, 2) && mCurrentServer.get() != 0) I(Op::SetLastRoom, mCurrentServer.get(), aGroup);
        unsigned int aRoomSpec = (inGameGroup ? 2 : 1) | 4;
        I(Op::EvJoinedRoom, mCurrentServer.get(), aGroup, aRoomSpec);
    }
    void ReadGameListComplete(intptr_t inOp, intptr_t inServer)
    {
        if (inServer != mCurrentServer.get()) return;
        I(Op::GameList, 1);
        bool doAlert = I(Op::Pref, 4) != 0;
        intptr_t aCount = 0;
        intptr_t anObjects = I(Op::RDataObjects, inOp, (intptr_t)&aCount);
        for (intptr_t anItr = 0; anItr != aCount; ++anItr) {
            intptr_t aGroup = I(Op::ServerGroup, inServer, I(Op::DataObjectLink, anObjects, anItr));
            if (aGroup == 0) continue;
            Ref aGame(I(Op::GameCreate), false);
            I(Op::GameDo, aGame.get(), 4, aGroup);
            if (!I(Op::GameReadDataObject, aGame.get(), anObjects, anItr)) continue;
            I(Op::GameList, 6, aGame.get());
            if (doAlert) I(Op::PingGame, aGame.get());
        }
        I(Op::EvSyncGameList);
        mHaveGameList = true;
        if (mGameListWaitOp.get() != 0) I(Op::OpFinish, mGameListWaitOp.get());
        I(Op::ListFree, anObjects, 1);
    }
    void AcceptClient(intptr_t inEvent)
    {
        if (mCurrentServer.get() == 0 || mGameGroup.get() == 0) return;
        intptr_t aConnection = SN(mCurrentServer.get(), 1);
        if (aConnection == 0 || EVT(inEvent, 1) == 0) return;
        intptr_t aClient = I(Op::ServerClient, mCurrentServer.get(), (unsigned short)CN(EVT(inEvent, 1), 0));
        if (aClient != EVT(inEvent, 1)) return;
        intptr_t aGroup = (unsigned short)GN(mGameGroup.get(), 0);
        intptr_t anId = (unsigned short)CN(aClient, 0);
        I(Op::ConnAccept, aConnection, aGroup, anId, EVT(inEvent, 2));
    }
    void RemoveFriend(intptr_t inFriend)
    {
        if (inFriend == 0) return;
        if (mCurrentServer.get() != 0) {
            intptr_t aClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)FN(inFriend, 2));
            if (aClient != 0) {
                I(Op::ClientDo, aClient, 3, false);
                I(Op::EvClientChanged, aClient, 4, 2);
            }
        }
        I(Op::EvFriendChanged, inFriend, 1);
        I(Op::RemoveFriend, (unsigned long)FN(inFriend, 2));
    }
    void FriendAction(intptr_t inEvent)
    {
        intptr_t aFriend = EVT(inEvent, 3);
        if (aFriend == 0) return;
        int anAction = (int)EVT(inEvent, 4);
        int aRoomSpec = (int)EVT(inEvent, 5);
        if (aRoomSpec == 0) aRoomSpec = 5;
        switch (anAction) {
        case 5:
            if (mCurrentServer.get() != 0) {
                intptr_t aClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)FN(aFriend, 2));
                if (aClient != 0) {
                    ShowClientOptions(aClient, aRoomSpec);
                    return;
                }
            }
        case 4:
            I(Op::FriendOptions, aFriend, aRoomSpec);
            return;
        case 1:
            LocateFriend(aFriend, aRoomSpec);
            return;
        case 2:
            LocateFriends();
            return;
        case 3:
            RemoveFriend(aFriend);
            return;
        }
    }
    intptr_t SelectGame(int inGameType, unsigned short inPingLimit)
    {
        intptr_t aConfig = I(Op::ConfigPingCeiling);
        unsigned short aPingCeiling = ee_won_routing_ping_ceiling(aConfig >= 0, aConfig >= 0 ? (uint16_t)aConfig : 0);
        typedef std::multimap<unsigned short, intptr_t> GameScoreMap;
        GameScoreMap aScores;
        I(Op::GameList, 2);
        while (I(Op::GameList, 3)) {
            intptr_t aGame = I(Op::GameList, 4);
            bool aPassword = GMN(aGame, 6) != 0;
            bool anAsk = !aPassword && GMN(aGame, 0);
            bool aCanJoin = !aPassword && !anAsk && GMN(aGame, 7);
            unsigned short aPing = (unsigned short)GMN(aGame, 8);
            long aSkill = (long)GMN(aGame, 9);
            EeWonQuickPlay aPick = ee_won_routing_quick_play_rank(aPassword, anAsk, aCanJoin, aPing, aSkill, inGameType,
                                                                  inPingLimit, aPingCeiling);
            if (aPick.action == 1)
                aScores.insert(GameScoreMap::value_type((unsigned short)aPick.rank, aGame));
            else if (aPick.action == 2)
                I(Op::PingGame, aGame);
        }
        GameScoreMap::iterator anItr = aScores.begin();
        if (anItr != aScores.end()) return anItr->second;
        else return 0;
    }
    void JumpToRoom(intptr_t inEvent)
    {
        if (IsConnecting() || mCurrentServer.get() == 0 || !I(Op::EventAddressIs, inEvent, mCurrentServer.get())) return;
        intptr_t aGroup = I(Op::ServerGroup, mCurrentServer.get(), EVT(inEvent, 6));
        if (aGroup == 0) return;
        if (GN(aGroup, 1)) {
            intptr_t aGame = I(Op::GameList, 5, EVT(inEvent, 6));
            if (aGame != 0 && I(Op::GameNameIsEvent, aGame, inEvent))
                I(Op::JoinGame, aGame);
        } else {
            JoinChatRoom(mCurrentServer.get(), (unsigned short)EVT(inEvent, 6));
        }
    }
    void QuickPlay(intptr_t inEvent)
    {
        if (!I(Op::Lobby, 3, true)) return;
        if (mCurrentServer.get() == 0) JoinDefaultRoom(false);
        if (mCurrentServer.get() == 0) return;
        if (!mHaveGameList) {
            { Ref aNew(I(Op::AsyncOpNew), false); mGameListWaitOp = aNew.get(); } // = new AsyncOp
            I(Op::RunWithStatus, mGameListWaitOp.get(), RES(RoutingLogic_GettingGameListTitle_String),
              RES(RoutingLogic_GettingGameListDetails_String), 1 | 0 << 8);
            mGameListWaitOp = 0;
            if (!mHaveGameList) return;
        }
        while (true) {
            if (mCurrentServer.get() == 0) return;
            intptr_t aGame = SelectGame((int)EVT(inEvent, 7), 1000);
            if (aGame != 0) {
                I(Op::JoinGame, aGame);
                return;
            }
            if ((int)I(Op::Lobby, 6) > 0) {
                Ref aWaitOp(I(Op::AsyncOpNew), false);
                I(Op::RunWithStatus, aWaitOp.get(), RES(RoutingLogic_CollectingPingsTitle_String),
                  RES(RoutingLogic_CollectingPingsDetails_String), 2 | 1000 << 8);
                if (I(Op::OpKilled, aWaitOp.get())) return;
            } else {
                if (I(Op::YesNo, RES(GameStagingLogic_NoQuickPlayGameTitle_String), RES(GameStagingLogic_NoQuickPlayGameDetails_String)) == 100001)
                    I(Op::Lobby, 5);
                return;
            }
        }
    }
    void ClientAction(intptr_t inEvent)
    {
        if (mCurrentServer.get() == 0) return;
        intptr_t aConnection = SN(mCurrentServer.get(), 1);
        if (aConnection == 0) return;
        if (!((EVT(inEvent, 8) & 4) ? true : false)) return;
        intptr_t aClient = EVT(inEvent, 9);
        if (aClient == 0) return;
        switch ((int)EVT(inEvent, 10)) {
        case 18:
            ShowClientOptions(aClient, (int)EVT(inEvent, 8));
            break;
        case 2: case 7: case 8: case 14: case 15: {
            int anAction = (int)EVT(inEvent, 10);
            DoAdminAction(anAction, (int)EVT(inEvent, 8), aClient);
            break;
        }
        case 1: {
            if (CN(aClient, 1) == 0 || CN(aClient, 2) || !CanAddFriend()) return;
            I(Op::ClientDo, aClient, 3, true);
            I(Op::EvClientChanged, aClient, 4, 2);
            intptr_t aFriend = I(Op::AddFriendOfClient, aClient);
            if (aFriend == 0) return;
            intptr_t aCurrentClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)CN(aClient, 1));
            if (aCurrentClient != 0) {
                I(Op::FriendDo, aFriend, 3, mCurrentServer.get(), 0);
                I(Op::FriendDo, aFriend, 2, aCurrentClient);
            }
            I(Op::EvFriendChanged, aFriend, 0);
            break;
        }
        case 9: {
            if (!CN(aClient, 2)) return;
            I(Op::ClientDo, aClient, 3, false);
            I(Op::EvClientChanged, aClient, 4, 2);
            intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(aClient, 1));
            if (aFriend != 0)
                I(Op::EvFriendChanged, aFriend, 1);
            I(Op::RemoveFriend, (unsigned long)CN(aClient, 1));
            break;
        }
        case 3:
            I(Op::SetBlocked, aClient, true);
            break;
        case 4:
            mSuppressIdle = false;
            I(Op::ConnSetClientFlags, aConnection, 8, 0, 0, 0);
            break;
        case 5:
            I(Op::SetIgnored, aClient, true);
            break;
        case 6:
            InviteClient(aClient, true, 0);
            break;
        case 13:
            InviteClient(aClient, false, 0);
            break;
        case 10:
            mSuppressIdle = true;
            I(Op::ConnSetClientFlags, aConnection, 8, 8, 0, 0);
            break;
        case 11:
            I(Op::SetBlocked, aClient, false);
            break;
        case 12:
            I(Op::SetIgnored, aClient, false);
            break;
        case 16:
            I(Op::EvSetChatInput, aClient, EVT(inEvent, 8));
            break;
        case 17: {
            if (CN(aClient, 2)) {
                intptr_t aFriend = I(Op::FindFriend, (unsigned long)CN(aClient, 1));
                if (aFriend != 0) {
                    LocateFriend(aFriend, (int)EVT(inEvent, 8));
                    return;
                }
            }
            AnnounceClientLocation(aClient, true, (int)EVT(inEvent, 8), true);
            break;
        }
        }
    }
    bool InviteClient(intptr_t inClient, bool inInvite, intptr_t inComment)
    {
        if (inClient == 0 || mGameGroup.get() == 0) return false;
        intptr_t aMyGame = I(Op::MyGame);
        if (aMyGame == 0 || !GMN(aMyGame, 4)) return false;
        intptr_t aConnection = GetConnection();
        if (aConnection == 0) return false;
        I(Op::ClientDo, inClient, 4, inInvite);
        intptr_t aGroup = (unsigned short)GN(mGameGroup.get(), 0);
        intptr_t anId = (unsigned short)CN(inClient, 0);
        I(Op::ConnInvite, aConnection, aGroup, anId, inInvite, inComment);
        I(Op::EvInvitationSent, mGameGroup.get(), inClient, inInvite, inComment);
        return true;
    }
    void ChatInput(intptr_t inTarget)
    {
        if (mCurrentServer.get() == 0) return;
        intptr_t aConnection = SN(mCurrentServer.get(), 1);
        if (aConnection == 0) return;
        intptr_t aParser = I(Op::Parser);
        if (aParser == 0 || !(I(Op::ParserNum, aParser, 0) & 4)) return;
        intptr_t aGroup = GetGroup((int)I(Op::ParserNum, aParser, 0));
        if (aGroup == 0) return;
        int aGroupId = (unsigned short)GN(aGroup, 0);
        intptr_t aTarget = I(Op::ParserNum, aParser, 3);
#define GetDuration() ((intptr_t)(uint32_t)I(Op::ParserNum, aParser, 2))
#define GetText() Txt aText(I(Op::ParserText, aParser))
#define GetRoomFlags() ((int)(uint32_t)I(Op::ParserNum, aParser, 0))
        switch ((int)I(Op::ParserNum, aParser, 1)) {
        case 1: case 3: {
            int aFlags = I(Op::ParserNum, aParser, 1) == 3 ? 2 : 0;
            if (I(Op::ParserNum, aParser, 0) & 0x10) {
                intptr_t aMyClient = I(Op::MyClient);
                if (aMyClient == 0) return;
                std::vector<uint16_t> aRecipients;
                intptr_t anItr = I(Op::IterNew, I(Op::ParserNum, aParser, 4));
                while (I(Op::IterHasMore, anItr)) {
                    intptr_t aClient = I(Op::IterNext, anItr);
                    if (I(Op::ClientIsTeammate, aMyClient, aClient))
                        aRecipients.push_back((unsigned short)CN(aClient, 0));
                }
                {
                    GetText();
                    I(Op::ConnSendChatTo, aConnection, aText.p, aFlags | 0x100, (intptr_t)aRecipients.size(),
                      aRecipients.empty() ? 1 : (intptr_t)&aRecipients[0]);
                }
                I(Op::ListFree, anItr, 3);
            } else {
                GetText();
                I(Op::ConnSendChat, aConnection, aText.p, aFlags, aGroupId);
            }
            break;
        }
        case 2:
            if (aTarget != 0) { GetText(); I(Op::ConnSendChat, aConnection, aText.p, 4, (unsigned short)CN(aTarget, 0)); }
            break;
        case 6: {
            intptr_t aMyClient = SN(mCurrentServer.get(), 2);
            if (aMyClient == 0) return;
            mSuppressIdle = !CN(aMyClient, 10);
            I(Op::ConnSetClientFlags, aConnection, 8, mSuppressIdle ? 8 : 0, 0, 0);
            break;
        }
        case 16: case 17:
            if (aTarget != 0) {
                bool aFailed;
                {
                    bool anInvite = I(Op::ParserNum, aParser, 1) == 16;
                    GetText();
                    aFailed = !InviteClient(aTarget, anInvite, aText.p);
                }
                if (aFailed) I(Op::EvNotCaptain, inTarget);
            }
            break;
        case 7: {
            intptr_t aMyClient = SN(mCurrentServer.get(), 2);
            if (aMyClient == 0) return;
            mWantToBeModerator = !CN(aMyClient, 11);
            I(Op::ConnDo, aConnection, 7, mWantToBeModerator);
            break;
        }
        case 10:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnMute, aConnection, aGroupId | 1 << 16, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            break;
        case 8:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnMute, aConnection, 0xffff | 1 << 16, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            break;
        case 11:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnBan, aConnection, aGroupId | 1 << 16, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            break;
        case 9:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnBan, aConnection, 0xffff | 1 << 16, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            break;
        case 12:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnMute, aConnection, aGroupId, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            else EditBadUserList(true, false, GetRoomFlags());
            break;
        case 13:
            if (aTarget != 0) { intptr_t d = GetDuration(); GetText(); I(Op::ConnMute, aConnection, 0xffff, d, aText.p, (unsigned short)CN(aTarget, 0)); }
            else EditBadUserList(true, true, GetRoomFlags());
            break;
        case 5:
            if (aTarget != 0) I(Op::SetIgnored, aTarget, !CN(aTarget, 4));
            break;
        case 4:
            if (aTarget != 0) I(Op::SetBlocked, aTarget, !CN(aTarget, 5));
            break;
        case 14:
            EditBadUserList(false, false, GetRoomFlags());
            break;
        case 15:
            EditBadUserList(false, true, GetRoomFlags());
            break;
        case 22: {
            intptr_t aMyClient = SN(mCurrentServer.get(), 2);
            if (aMyClient != 0 && CN(aMyClient, 12)) { GetText(); I(Op::ConnAlert, aConnection, aText.p); }
            break;
        }
        case 23: {
            intptr_t aMyClient = SN(mCurrentServer.get(), 2);
            if (aMyClient != 0 && CN(aMyClient, 12)) { GetText(); I(Op::ConnStartShutdown, aConnection, aText.p, GetDuration()); }
            break;
        }
        case 24: {
            intptr_t aMyClient = SN(mCurrentServer.get(), 2);
            if (aMyClient != 0 && CN(aMyClient, 12)) I(Op::ConnDo, aConnection, 6);
            break;
        }
        case 25:
            if (aTarget != 0) { GetText(); I(Op::ConnSendChat, aConnection, aText.p, 0x10, (unsigned short)CN(aTarget, 0)); }
            break;
        }
#undef GetDuration
#undef GetText
#undef GetRoomFlags
    }
    void ConnectionClosed(intptr_t inOp)
    {
        intptr_t aSocket = I(Op::OpSocket, inOp);
        intptr_t aConnection = GetConnection();
        if (aSocket != aConnection) return;
        I(Op::EvLeftRoom, 4, true, true);
        if (mGameGroup.get() != 0) I(Op::Lobby, 2, false);
        LeaveServer();
        I(Op::MessageBox, RES(RoutingLogic_DisconnectedTitle_String), RES(RoutingLogic_DisconnectedDetails_String));
    }
    void CreateRoom(intptr_t inEvent)
    {
        I(Op::DialogReset, 4, false);
        if (I(Op::DialogDo, 4) != 100001 || IsConnecting()) return;
        Txt aName(I(Op::DialogText, 1));
        Txt aPassword(I(Op::DialogText, 2));
        if (I(Op::TextLength, aName.p) == 0) return;
        intptr_t aServer = I(Op::EventServer, inEvent);
        if (aServer == 0) aServer = mCurrentServer.get();
        if (aServer == 0) aServer = GetDefaultServer();
        if (aServer == 0) return;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(aServer, false)) return;
        unsigned short aGroupId = 0;
        if (!CreateGroup(mPendingServer.get(), aName.p, aPassword.p, 0x800, aGroupId)) return;
        JoinedGroup(aGroupId, false);
        if (I(Op::DialogNum, 3) != 0)
            I(Op::RoomDialogEnd);
    }
    bool JoinChatRoom(intptr_t inServer, unsigned short inGroupId)
    {
        if (inServer == 0) return false;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(inServer, false)) return false;
        if (mChatGroup.get() != 0 && inServer == mCurrentServer.get() &&
            inGroupId == (unsigned short)GN(mChatGroup.get(), 0)) {
            I(Op::MessageBox, RES(RoutingLogic_AlreadyJoinedTitle_String), RES(RoutingLogic_AlreadyInRoom_String));
            return false;
        }
        if (!JoinGroup(mPendingServer.get(), inGroupId)) return false;
        JoinedGroup(inGroupId, false);
        return true;
    }
    void JoinRoom(intptr_t inEvent)
    {
        if (IsConnecting()) return;
        intptr_t aServer = I(Op::EventServer, inEvent);
        if (aServer == 0) return;
        unsigned long aGroupId = (unsigned long)EVT(inEvent, 11);
        if (aGroupId == 0xffff) aGroupId = 0;
        if (JoinChatRoom(aServer, (unsigned short)aGroupId) && I(Op::DialogNum, 3) != 0)
            I(Op::RoomDialogEnd);
    }
    void JoinFriendsRoom(intptr_t inEvent)
    {
        if (IsConnecting()) return;
        intptr_t aFriend = EVT(inEvent, 12);
        if (aFriend == 0) return;
        if (!I(Op::FriendAddressValid, aFriend)) return;
        intptr_t aServer = I(Op::FriendServer, aFriend);
        if (aServer == 0) return;
        PendingServerCleanup aCleanup(this);
        if (!ConnectToServer(aServer, false)) return;
        intptr_t aClient = I(Op::ServerFindByWonId, aServer, (unsigned long)FN(aFriend, 2));
        if (aClient == 0) return;
        unsigned short aGroupId = 0;
        if (CN(aClient, 6)) aGroupId = (unsigned short)CN(aClient, 7);
        if (mChatGroup.get() != 0 && aServer == mCurrentServer.get() &&
            aGroupId == (unsigned short)GN(mChatGroup.get(), 0)) {
            I(Op::MessageBox, RES(RoutingLogic_AlreadyJoinedTitle_String), RES(RoutingLogic_AlreadyInRoom_String));
            return;
        }
        if (!JoinGroup(aServer, aGroupId)) return;
        JoinedGroup(aGroupId, false);
    }
    bool CreateGroup(intptr_t inServer, intptr_t inName, intptr_t inPassword, int inRoomFlags, unsigned short& outGroupId)
    {
        if (inServer != 0) {
        Ref aConnection(SN(inServer, 1), true);
        if (aConnection.get() == 0) return false;
        Ref anOp(I(Op::RNew, aConnection.get(), 12), false);
        I(Op::RSetText, anOp.get(), 2, inPassword);
        I(Op::RSetText, anOp.get(), 1, inName);
        I(Op::RSetNum, anOp.get(), 16, (intptr_t)(unsigned long)inRoomFlags);
        I(Op::RSetNum, anOp.get(), 13, 1);
        I(Op::RSetNum, anOp.get(), 33, 1);
        I(Op::RunWithStatus, anOp.get(), RES(RoutingLogic_CreateGroupTitle_String), RES(RoutingLogic_CreateGroupDetails_String), 1 | 0 << 8);
        if ((int)I(Op::OpStatus, anOp.get()) == 0) {
            outGroupId = (unsigned short)RN(anOp.get(), 0);
            return true;
        }
        }
        return false;
    }
    bool JoinGroup(intptr_t inServer, int inGroupId)
    {
        if (inServer == 0) return false;
        Ref aConnection(SN(inServer, 1), true);
        if (aConnection.get() == 0) return false;
        Ref aGroup(I(Op::ServerGroup, inServer, inGroupId), true);
        if (aGroup.get() == 0) {
            I(Op::ErrorDialog, RES(RoutingLogic_JoinGroupTitle_String), -2520);
            return false;
        }
        Txt aPassword;
        if (GN(aGroup.get(), 4)) {
            I(Op::DialogReset, 5);
            if (I(Op::DialogDo, 5) != 0x186a1) return false;
            aPassword = I(Op::DialogText, 3);
        }
        if (!I(Op::ConnDo, aConnection.get(), 5)) return false;
        Ref anOp(I(Op::RNew, aConnection.get(), 34), false);
        I(Op::RSetNum, anOp.get(), 0, (unsigned short)inGroupId);
        I(Op::RSetText, anOp.get(), 2, aPassword.p);
        while (true) {
            if (aGroup.get() != 0 && GN(aGroup.get(), 5))
                I(Op::RunWithStatus, anOp.get(), RES(RoutingLogic_AskingCaptainTitle_String), RES(RoutingLogic_AskingCaptainDetails_String), 1 | 0 << 8);
            else
                I(Op::RunWithStatus, anOp.get(), RES(RoutingLogic_JoinGroupTitle_String), RES(RoutingLogic_JoinGroupDetails_String), 1 | 0 << 8);
            if ((int)I(Op::OpStatus, anOp.get()) == 0) break;
            if (!I(Op::ConnDo, aConnection.get(), 5)) return false;
            if (I(Op::OpKilled, anOp.get())) {
                I(Op::ConnDo, aConnection.get(), 9, (unsigned short)inGroupId);
                return false;
            }
            if ((int)I(Op::OpStatus, anOp.get()) == -2524) {
                I(Op::DialogReset, 5);
                if (I(Op::DialogDo, 5) != 0x186a1) return false;
                Txt aNew(I(Op::DialogText, 3));
                I(Op::RSetText, anOp.get(), 2, aNew.p);
            } else {
                if ((int)I(Op::OpStatus, anOp.get()) == -2504) return true;
                return false;
            }
        }
        return true;
    }
    void ShowClientOptions(intptr_t inClient, int inRoomSpec)
    {
        if (mCurrentServer.get() == 0 || inClient == 0) return;
        intptr_t myClient = SN(mCurrentServer.get(), 2);
        if (myClient == 0) return;
        I(Op::ClientOptions, inClient, myClient, inRoomSpec);
    }
    static void SetModerationTarget(intptr_t inOp, intptr_t inCurrent, intptr_t inClient)
    {
        if (inCurrent != 0) {
            I(Op::RSetNum, inOp, 1, (unsigned short)CN(inCurrent, 0));
            I(Op::RSetNum, inOp, 30, 0);
            return;
        }
        unsigned long id = (unsigned long)CN(inClient, 13);
        if (id == 0) id = (unsigned long)CN(inClient, 1);
        if (id == 0) return;
        I(Op::RSetNum, inOp, 26, (intptr_t)id);
        I(Op::RSetNum, inOp, 30, 1);
        I(Op::RSetNum, inOp, 31, 1);
    }
    void DoAdminAction(int inAction, int inRoomSpec, intptr_t inClient)
    {
        intptr_t connection = GetConnection();
        if (connection == 0) return;
        intptr_t myClient = SN(mCurrentServer.get(), 2);
        if (myClient == 0) return;
        bool showDialog = CN(myClient, 11) || CN(myClient, 12);
        if (showDialog) {
            I(Op::DialogReset, 2, inAction, inClient);
            if (I(Op::DialogDo, 2) != 100001) return;
            connection = GetConnection();
            if (connection == 0) return;
        }
        intptr_t currentClient = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)CN(inClient, 1));
        int groupId = 0xffff;
        if (inRoomSpec & 2) {
            if (mGameGroup.get() == 0) return;
            groupId = (unsigned short)GN(mGameGroup.get(), 0);
        } else if (inRoomSpec & 1) {
            if (mChatGroup.get() == 0) return;
            groupId = (unsigned short)GN(mChatGroup.get(), 0);
        }
        Txt explanation;
        unsigned long seconds;
        if (showDialog) {
            if (I(Op::DialogNum, 1)) groupId = 0xffff;
            else if (groupId == 0xffff && inAction != 15) return;
            explanation = I(Op::DialogText, 4);
            seconds = (unsigned long)I(Op::DialogNum, 2);
        } else {
            if (groupId == 0xffff) return;
            seconds = inAction == 7;
        }
        Ref op;
        intptr_t title = 0;
        switch (inAction) {
        case 2:
        case 7: {
            if (!showDialog && (inRoomSpec & 2)) {
                I(Op::KickClient, inClient, inAction == 2);
                return;
            }
            Ref ban(I(Op::RNew, connection, 3), false);
            if (inAction == 2) {
                I(Op::RSetText, ban.get(), 3, explanation.p);
                I(Op::RSetNum, ban.get(), 27, (intptr_t)seconds);
                title = RES(RoutingLogic_BanClientTitle_String);
            } else if (inAction == 7) {
                I(Op::RSetNum, ban.get(), 27, 1);
                title = RES(RoutingLogic_KickClientTitle_String);
            }
            I(Op::RSetNum, ban.get(), 0, (unsigned short)groupId);
            op = ban.get();
            SetModerationTarget(ban.get(), currentClient, inClient);
            break;
        }
        case 8:
        case 14: {
            Ref mute(I(Op::RNew, connection, 38), false);
            I(Op::RSetText, mute.get(), 3, explanation.p);
            I(Op::RSetNum, mute.get(), 27, (intptr_t)seconds);
            I(Op::RSetNum, mute.get(), 0, (unsigned short)groupId);
            op = mute.get();
            SetModerationTarget(mute.get(), currentClient, inClient);
            if (inAction == 14) {
                title = RES(RoutingLogic_UnmuteClientTitle_String);
                I(Op::RSetNum, mute.get(), 29, 0);
            } else {
                title = RES(RoutingLogic_MuteClientTitle_String);
                I(Op::RSetNum, mute.get(), 29, 1);
            }
            break;
        }
        case 15:
            if (currentClient != 0)
                I(Op::ConnSendChat, connection, explanation.p, 0x10, (unsigned short)CN(currentClient, 0));
            return;
        }
        if (op.get() == 0) return;
        if (showDialog) I(Op::RunWithStatus, op.get(), title, RES(RoutingLogic_ModerateDetails_String), 1 | 0 << 8);
        else I(Op::RunOp, op.get());
    }
    void EditBadUserList(bool inMuted, bool inServerWide, int inRoomSpec)
    {
        intptr_t connection = GetConnection();
        if (connection == 0) return;
        intptr_t myClient = SN(mCurrentServer.get(), 2);
        if (myClient == 0) return;
        bool moderator = CN(myClient, 11) || CN(myClient, 12);
        int listType = inMuted ? 2 : 1;
        unsigned short groupId = 0xffff;
        if (!inServerWide) {
            intptr_t group = GetGroup(inRoomSpec);
            if (group == 0) return;
            groupId = (unsigned short)GN(group, 0);
        }
        Ref op(I(Op::RNew, connection, 19), false);
        I(Op::RSetNum, op.get(), 38, listType);
        I(Op::RSetNum, op.get(), 0, groupId);
        I(Op::RSetNum, op.get(), 12, 0x0f);
        I(Op::RunWithStatus, op.get(), RES(RoutingLogic_GetBadUsersTitle_String), RES(RoutingLogic_GetBadUsersDetails_String), 1 | 0 << 8);
        if ((int)I(Op::OpStatus, op.get()) != 0) return;
        I(Op::BadUserSet, op.get(), !moderator);
        if (I(Op::DialogDo, 3) != 100001) return;
        connection = GetConnection();
        if (connection == 0) return;

        std::vector<uint64_t> ids;
        I(Op::BadUserIds, (intptr_t)&ids);
        if (ids.empty()) return;
        std::vector<uint64_t>::iterator i = ids.begin();
        while (i != ids.end()) {
            if (listType == 1)
                I(Op::ConnBanByWonId, connection, groupId, (intptr_t)*i);
            else if (listType == 2)
                I(Op::ConnMuteByWonId, connection, groupId, (intptr_t)*i);
            ++i;
        }
    }
    void LocateFriend(intptr_t inFriend, int inRoomSpec)
    {
        if (inFriend == 0) return;
        if (!ShouldLocateFriend(inFriend, (unsigned long)I(Op::Now))) {
            if (!(FN(inFriend, 0) & 4)) AnnounceFriendLocation(inFriend, inRoomSpec);
            return;
        }
        intptr_t manager = I(Op::UdpManager);
        if (manager == 0) return;
        int count = 0;
        for (size_t it = 0; (intptr_t)it < I(Op::ListNum, 0); ++it) {
            if (mCurrentServer.get() != 0 && I(Op::ListAddressIs, (intptr_t)it, mCurrentServer.get())) continue;
            Ref op(I(Op::StatusOpNew, (intptr_t)it, manager), false);
            I(Op::StatusOpAddId, op.get(), (unsigned long)FN(inFriend, 2));
            I(Op::StatusOpRun, op.get());
            ++count;
        }
        if (count <= 0)
            return;
        I(Op::FriendDo, inFriend, 5, inRoomSpec);
        I(Op::FriendDo, inFriend, 6, count);
        I(Op::EvFriendChanged, inFriend, 2);
    }
    void GetUserStatusComplete(intptr_t inOp)
    {
        intptr_t server = I(Op::StatusOpServer, inOp);
        unsigned long now = (unsigned long)I(Op::Now);
        Ref lastChanged;
        int changed = 0;
        std::vector<uint64_t> statuses;
        I(Op::StatusOpMap, inOp, (intptr_t)&statuses);
        for (size_t it = 0; it + 1 < statuses.size(); it += 2) {
            intptr_t friendInfo = I(Op::FindFriend, (intptr_t)statuses[it]);
            unsigned char aStatus = (unsigned char)statuses[it + 1];
            if (friendInfo == 0 || !(FN(friendInfo, 0) & 4)) continue;
            bool found = false;
            if (aStatus != 0 && server != 0) {
                I(Op::FriendDo, friendInfo, 3, server, aStatus);
                found = true;
            }
            I(Op::FriendDo, friendInfo, 6, (intptr_t)(unsigned long)((unsigned long)FN(friendInfo, 4) - 1));
            int remaining = (int)(unsigned long)FN(friendInfo, 4);
            if (remaining > 0 && !found) continue;
            lastChanged = friendInfo;
            ++changed;
            I(Op::FriendDo, friendInfo, 7, (intptr_t)now);
            if (FN(friendInfo, 5) != 0)
                I(Op::EvAnnounceFriend, friendInfo, FN(friendInfo, 5), 2);
        }
        if (changed > 1) I(Op::BroadcastLobby, 100056);
        else if (changed == 1 && lastChanged.get() != 0)
            I(Op::EvFriendChanged, lastChanged.get(), 2);
    }
    void LocateFriends()
    {
        intptr_t manager = I(Op::UdpManager);
        if (manager == 0) return;
        unsigned long now = (unsigned long)I(Op::Now);
        int count = 0;
        for (size_t scanServer = 0; (intptr_t)scanServer < I(Op::ListNum, 0); ++scanServer) {
            if (mCurrentServer.get() != 0 && I(Op::ListAddressIs, (intptr_t)scanServer, mCurrentServer.get()))
                continue;
            ++count;
        }
        if (count == 0) return;
        std::map<uint32_t, unsigned char> requested;
        size_t friendItr = 0;
        while ((intptr_t)friendItr < I(Op::FriendCount)) {
            intptr_t friendInfo = I(Op::FriendAt, (intptr_t)friendItr);
            if (ShouldLocateFriend(friendInfo, now)) {
                I(Op::FriendDo, friendInfo, 5, 0);
                I(Op::FriendDo, friendInfo, 6, count);
                requested[(uint32_t)FN(friendInfo, 2)] = 0;
            }
            ++friendItr;
        }
        if (requested.empty()) return;
        std::vector<uint32_t> anIds;
        for (std::map<uint32_t, unsigned char>::iterator i = requested.begin(); i != requested.end(); ++i) anIds.push_back(i->first);
        for (size_t queryServer = 0; (intptr_t)queryServer < I(Op::ListNum, 0); ++queryServer) {
            if (mCurrentServer.get() != 0 && I(Op::ListAddressIs, (intptr_t)queryServer, mCurrentServer.get()))
                continue;
            Ref op(I(Op::StatusOpNew, (intptr_t)queryServer, manager), false);
            I(Op::StatusOpSetMap, op.get(), (intptr_t)&anIds[0], (intptr_t)anIds.size());
            I(Op::StatusOpRun, op.get());
        }
        I(Op::BroadcastLobby, 100056);
    }
    bool ConnectToServer(intptr_t inServer, bool inFlag)
    {
        if (inServer == 0) return false;
        mPendingServer = inServer;
        if (inServer == mCurrentServer.get() && SN(inServer, 7)) return true;
        if (mGameGroup.get() != 0 && !I(Op::Lobby, 3, true)) return false;

        Ref connection(I(Op::ConnNew), false);
        I(Op::ServerDo, inServer, 8, connection.get());
        intptr_t auth = I(Op::AuthContext);
        if (auth == 0) return false;
        I(Op::ConnDo, connection.get(), 1, auth);
        I(Op::ConnDo, connection.get(), 2); // the twenty notices

        Ref connectOp(I(Op::ConnectOpNew, connection.get(), inServer), false);
        I(Op::RunWithStatus, connectOp.get(), RES(RoutingLogic_ConnectTitle_String), RES(RoutingLogic_ConnectDetails_String), 1 | 0 << 8);
        if ((int)I(Op::OpStatus, connectOp.get()) != 0) return false;

        unsigned long registerFlags = 0x15;
        if (mWantToBeModerator) registerFlags = 0x1d;
        Ref registerOp(I(Op::RNew, connection.get(), 42), false);
        I(Op::RSetNum, registerOp.get(), 24, (intptr_t)registerFlags);
        I(Op::RSetNum, registerOp.get(), 25, (intptr_t)(unsigned long)I(Op::ReconnectId, inServer));
        unsigned long clientFlags = 0;
        if (!I(Op::Pref, 7)) clientFlags = 4;
        if (I(Op::Pref, 6) || inFlag) clientFlags |= 0x40;
        I(Op::RSetNum, registerOp.get(), 20, (intptr_t)clientFlags);
        I(Op::RunWithStatus, registerOp.get(), RES(RoutingLogic_RegisterTitle_String), RES(RoutingLogic_RegisterDetails_String), 1 | 0 << 8);
        if ((int)I(Op::OpStatus, registerOp.get()) != 0) return false;

        I(Op::SetReconnectId, inServer, (intptr_t)(unsigned long)RN(registerOp.get(), 25));
        I(Op::ServerSyncClients, inServer, registerOp.get());
        I(Op::ServerDo, inServer, 7, (unsigned short)RN(registerOp.get(), 1));
        I(Op::ConnDo, connection.get(), 3);
        if (inFlag && !I(Op::Pref, 6)) I(Op::ConnSetClientFlags, connection.get(), 0x40, 0, 0, 0);

        Ref groupOp(I(Op::RNew, connection.get(), 21), false);
        I(Op::RSetNum, groupOp.get(), 10, 0xff);
        I(Op::RunWithStatus, groupOp.get(), RES(RoutingLogic_QueryGroupsTitle_String), RES(RoutingLogic_QueryGroupsDetails_String), 1 | 0 << 8);
        if ((int)I(Op::OpStatus, groupOp.get()) != 0) return false;
        I(Op::ServerSyncGroups, inServer, groupOp.get(), true);
        I(Op::EvServerChanged, inServer, 2);
        I(Op::ServerDo, inServer, 3);
        I(Op::BroadcastLobby, 100056);
        return true;
    }
    void AddFriend()
    {
        if (!CanAddFriend()) return;
        I(Op::DialogReset, 1);
        if (I(Op::DialogDo, 1) != 100001) return;
        intptr_t servers = I(Op::AccountServers);
        if (!servers) return;
        Txt name(I(Op::DialogText, 5));
        int aStatus = 0;
        unsigned long aUserId = (unsigned long)I(Op::UserIdLookup, servers, name.p, (intptr_t)&aStatus);
        if (aStatus != 0 || aUserId == 0) return;
        intptr_t friendEntry = I(Op::AddFriendNamed, name.p, (intptr_t)aUserId);
        if (friendEntry) {
        if (mCurrentServer.get()) {
            intptr_t client = I(Op::ServerFindByWonId, mCurrentServer.get(), (unsigned long)FN(friendEntry, 2));
            if (client) {
                I(Op::ClientDo, client, 3, true);
                I(Op::FriendDo, friendEntry, 3, mCurrentServer.get(), 0);
                I(Op::FriendDo, friendEntry, 2, client);
                I(Op::EvClientChanged, client, 4, 2);
            }
        }
        I(Op::EvFriendChanged, friendEntry, 0);
        }
    }
    void QueryServerGroupsDone(intptr_t inOp)
    {
        intptr_t servers = I(Op::LobbyServerList);
        if (!servers) return;
        intptr_t server = I(Op::ServerListFind, servers, inOp);
        if (!server) return;
        I(Op::ServerDo, server, 5, false);
        if ((int)I(Op::OpStatus, inOp) != 0) {
            mLastServerRefresh = 0;
            I(Op::ServerDo, server, 6, 0);
        }
        I(Op::SyncGroupsOfQuery, server, inOp);
        I(Op::EvServerChanged, server, 2);
    }
    void HandleQueryServerGroups(intptr_t event)
    {
        intptr_t server = I(Op::EventServer, event);
        if (!server) return;
        long long now = (long long)I(Op::Now);
        if (SN(server, 7) || SN(server, 9) || (int32_t)(uint32_t)(now - (unsigned long)SN(server, 10)) < 30)
            return;
        I(Op::ServerDo, server, 4);
        I(Op::EvServerChanged, server, 2);
        I(Op::ServerDo, server, 5, true);
        I(Op::ServerDo, server, 6, (intptr_t)(uint32_t)now);
        I(Op::GroupQueryRun, server);
    }
    void RefreshServerList()
    {
        Ref context(I(Op::RoomDirServers), false);
        bool config = I(Op::HasConfig) != 0;
        if (!context.get() || !config) return;
        long long refreshTime = (long long)I(Op::Now);
        if ((int32_t)((uint32_t)refreshTime - (uint32_t)mLastServerRefresh) >= 300 || I(Op::ListNum, 1)) {
            Ref op(I(Op::DirOpRun, context.get()), false);
            if ((int)I(Op::OpStatus, op.get()) != 0) return;
            intptr_t anEmpty = 0;
            intptr_t entities = I(Op::DirEntities, op.get(), (intptr_t)&anEmpty);
            if (anEmpty) I(Op::DirNotifyFailed, context.get(), op.get());
            I(Op::DirUpdate, entities);
            I(Op::ListFree, entities, 2);
            mLastServerRefresh = refreshTime;
        }
        long long now = (long long)I(Op::Now);
        size_t it = 0;
        while ((intptr_t)it < I(Op::ListNum, 0)) {
            intptr_t server = I(Op::ListAt, (intptr_t)it);
            if (!SN(server, 7) && (int32_t)((uint32_t)now - (uint32_t)SN(server, 10)) >= 30)
                I(Op::ServerDo, server, 4);
            ++it;
        }
        I(Op::BroadcastLobby, 100053);
    }
    void EnterInternetScreen(bool joinDefaultRoom)
    {
        RefreshServerList();
        if (I(Op::ListNum, 1)) {
            I(Op::MessageBox, RES(RoutingLogic_NoServersTitle_String), RES(RoutingLogic_NoServersDetails_String));
            return;
        }
        if (joinDefaultRoom) {
            if (!JoinDefaultRoom(false)) {
                if (I(Op::Pref, 9)) {
                    I(Op::Lobby, 7);
                    return;
                }
                DoRoomDialog();
            }
        } else DoRoomDialog();
        I(Op::Lobby, 8);
        if (I(Op::Pref, 3) && !I(Op::Pref, 5)) I(Op::Sound, RES(LobbyGlobal_SierraLogon_Sound));
    }
    void DoRoomDialog() { I(Op::DialogDo, 6); }
    void HandleControlEvent(int controlId)
    {
        switch (controlId) {
        case 1: RefreshServerList(); DoRoomDialog(); break;
        case 2: AddFriend(); break;
        case 3: HandleAnonymousModeChange(); break;
        case 4: HandleDirtyWordFilterChange(); break;
        case 5: LocateFriends(); break;
        case 6: RefreshGameList(); break;
        }
    }
    void HandleComponentEvent(intptr_t event, int inType)
    {
        switch (inType) {
        case 100002: AcceptClient(event); return;
        case 100005: ClientAction(event); return;
        case 100011: CreateRoom(event); return;
        case 100012: ChatInput(EVT(event, 13)); return;
        case 100015: FriendAction(event); return;
        case 100028: JoinRoom(event); return;
        case 100026: JoinFriendsRoom(event); return;
        case 100031: JumpToRoom(event); return;
        case 100042: HandleQueryServerGroups(event); return;
        case 100043: QuickPlay(event); return;
        }
    }
    void HandleAnonymousModeChange()
    {
        intptr_t connection = GetConnection();
        if (connection == 0) return;
        uint32_t mask = 0, flags = 0;
        ee_won_routing_client_flags(0, I(Op::Pref, 6) != 0, &mask, &flags);
        I(Op::ConnSetClientFlags, connection, mask, flags, 0, 0);
    }
    void HandleDirtyWordFilterChange()
    {
        intptr_t connection = GetConnection();
        if (connection == 0) return;
        uint32_t mask = 0, flags = 0;
        ee_won_routing_client_flags(1, I(Op::Pref, 7) != 0, &mask, &flags);
        I(Op::ConnSetClientFlags, connection, mask, flags, 0, 0);
    }
    bool CanAddFriend()
    {
        if (!ee_won_routing_can_add_friend((size_t)I(Op::FriendCount))) {
            I(Op::MessageBox, RES(RoutingLogic_TooManyFriendsTitle_String), RES(RoutingLogic_TooManyFriendsDetails_String));
            return false;
        }
        return true;
    }
    void Logout()
    {
        intptr_t context = I(Op::AuthContext);
        if (context != 0) I(Op::AuthStopRefresh, context);
        mWantToBeModerator = false;
        I(Op::DialogReset, 4, true);
        ClearPendingServer();
        LeaveServer();
    }
    // RoutingOpCompletion::Run
    void Notify(int inCode, intptr_t inOp)
    {
        if (I(Op::Killed) || I(Op::OpKilled, inOp)) return;
        intptr_t aServer = GetServer(inOp);
        if (aServer == 0) return;
        switch (inCode) {
        case 1: ClientJoinedServer(inOp, aServer); break;
        case 2: ClientLeftServer(inOp, aServer); break;
        case 3: ClientJoinedGroup(inOp, aServer); break;
        case 4: ClientLeftGroup(inOp, aServer); break;
        case 5: PeerChat(inOp, aServer); break;
        case 6: PeerData(inOp, aServer); break;
        case 7: ClientFlagsChanged(inOp, aServer); break;
        case 8: MemberFlagsChanged(inOp, aServer); break;
        case 9: GroupCreated(inOp, aServer); break;
        case 10: GroupDeleted(inOp, aServer); break;
        case 11: GroupCaptainChanged(inOp, aServer); break;
        case 12: DataObjectCreated(inOp, aServer); break;
        case 13: DataObjectDeleted(inOp, aServer); break;
        case 14: GroupInvitation(inOp, aServer); break;
        case 15: GroupJoinAttempt(inOp, aServer); break;
        case 16: YouWereBanned(inOp, aServer); break;
        case 17: YouWereMuted(inOp, aServer); break;
        case 18: ServerAlert(inOp); break;
        case 19: ServerShutdownStarted(inOp); break;
        case 20: ServerShutdownAborted(inOp); break;
        case 21: ReadGameListComplete(inOp, aServer); break;
        }
    }
    intptr_t Field(intptr_t a)
    {
        switch (a) {
        case 0: return mCurrentServer.get();
        case 1: return mChatGroup.get();
        case 2: return mPendingServer.get();
        case 3: return mGameGroup.get();
        case 4: return mGameListWaitOp.get();
        case 5: return mSuppressIdle;
        case 6: return mIdle;
        case 7: return mHaveGameList;
        case 8: return mWantToBeModerator;
        case 9: return (intptr_t)mLastServerRefresh;
        }
        return 0;
    }
    void Free() // the members, the game room's first (the Rust's order; references are counted, not traced)
    {
        mGameGroup = 0;
        mChatGroup = 0;
        mPendingServer = 0;
        mCurrentServer = 0;
        mGameListWaitOp = 0;
    }
};

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b)
{
    OrigRouting* o = sOR;
    switch (inCall) {
    case Call::HostGame: return o->HostGame(a);
    case Call::JoinGame: return o->JoinGame(a);
    case Call::ClientList: return o->GetClientList();
    case Call::MyClient: return o->GetMyClient();
    case Call::SendToCaptain: o->SendToCaptain(a); return 0;
    case Call::BroadcastToGame: o->BroadcastToGame(a); return 0;
    case Call::SendToClient: o->SendToClient(a, b); return 0;
    case Call::Advertise: o->AdvertiseGame(a); return 0;
    case Call::RemoveAdvertisement: o->RemoveGameAdvertisement(); return 0;
    case Call::KickClient: o->KickClient(a, b != 0); return 0;
    case Call::SetGameInProgress: o->SetGameInProgress(a != 0); return 0;
    case Call::Reset: o->Reset(); return 0;
    case Call::LeaveGame: o->LeaveGame(); return 0;
    case Call::Suspend: o->Suspend(); return 0;
    case Call::Resume: o->Resume(); return 0;
    case Call::Logout: o->Logout(); return 0;
    case Call::EnterInternet: o->EnterInternetScreen(a != 0); return 0;
    case Call::Control: o->HandleControlEvent((int)a); return 0;
    case Call::Event: o->HandleComponentEvent(a, (int)b); return 0;
    case Call::Notify: o->Notify((int)a, b); return 0;
    case Call::UserStatusDone: o->GetUserStatusComplete(a); return 0;
    case Call::GroupsDone: o->QueryServerGroupsDone(a); return 0;
    case Call::ConnectionClosed: o->ConnectionClosed(a); return 0;
    case Call::Field: return o->Field(a);
    }
    return 0;
}

static intptr_t LogicCall(Call inCall, intptr_t a, intptr_t b)
{
    if (sRust) return ee_won_routelogic_call(sRL, inCall, a, b);
    return OrigCall(inCall, a, b);
}

// ---- the sequences

static const int kEvents[] = {100002, 100005, 100011, 100012, 100015, 100028, 100026, 100031, 100042, 100043, 7,
                              100012, 100005, 100012};

static void MakeCall(Call* outCall, intptr_t* outA, intptr_t* outB)
{
    intptr_t a = 0, b = 0;
    Call c;
    switch (R(36)) {
    case 0: c = Call::HostGame; a = Game(); break;
    case 1: c = Call::JoinGame; a = Game(); break;
    case 2: c = Call::ClientList; break;
    case 3: c = Call::MyClient; break;
    case 4: c = R(2) ? Call::SendToCaptain : Call::BroadcastToGame; a = 0x8800; break;
    case 5: c = Call::SendToClient; a = Client(); b = 0x8800; break;
    case 6: c = R(2) ? Call::Advertise : Call::RemoveAdvertisement; a = Game(); break;
    case 7: c = Call::KickClient; a = Client(); b = R(2); break;
    case 8: c = Call::SetGameInProgress; a = R(2); break;
    case 9: case 10: c = Call::Reset; break;
    case 11: c = Call::LeaveGame; break;
    case 12: c = R(2) ? Call::Suspend : Call::Resume; break;
    case 13: c = Call::Logout; break;
    case 14: c = Call::EnterInternet; a = R(2); break;
    case 15: case 16: c = Call::Control; a = 1 + R(7); break;
    case 17: case 18: case 19: case 20: case 21: case 22: case 23:
        c = Call::Event; a = 0x8000 + R(4); b = kEvents[R(14)]; break;
    case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
        c = Call::Notify; a = 1 + R(22); b = 0x8100 + R(4); break;
    case 32: c = Call::UserStatusDone; a = 0x8200; break;
    case 33: c = Call::GroupsDone; a = 0x8300; break;
    case 34: c = Call::ConnectionClosed; a = 0x8400; break;
    default: c = Call::Field; a = R(11); break;
    }
    *outCall = c;
    *outA = a;
    *outB = b;
}

static void RandomCall()
{
    Call c;
    intptr_t a, b;
    MakeCall(&c, &a, &b);
    Line("call %lld %lld %lld", (long long)c, a, b);
    long long r = LogicCall(c, a, b);
    Line("ret %lld", r);
}

static void Fields()
{
    std::string s = "fields";
    for (int i = 0; i < 10; i++) s += " " + std::to_string((long long)LogicCall(Call::Field, i));
    sTrace.push_back(s);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, unsigned inStepSeed, int inCount)
{
    sTrace.clear();
    sRefs.clear();
    sRust = inRust;
    sDepth = 0;
    sNextHandle = 0;
    if (inRust) sRL = ee_won_routelogic_new((void*)kObj, Host);
    else sOR = new OrigRouting();
    // the steps come from their own stream, the answers from the sequence's
    unsigned aStepSeed = inStepSeed;
    for (int i = 0; i < inCount; i++) {
        sSeed = aStepSeed;
        Call c;
        intptr_t a, b;
        MakeCall(&c, &a, &b);
        aStepSeed = sSeed;
        sSeed = inSeed + (unsigned)i * 104729u;
        Line("call %lld %lld %lld", (long long)c, a, b);
        long long r = LogicCall(c, a, b);
        Line("ret %lld", r);
    }
    Fields();
    Line("free");
    if (inRust) ee_won_routelogic_free(sRL);
    else {
        sOR->Free();
        delete sOR;
    }
    for (std::map<intptr_t, int>::iterator it = sRefs.begin(); it != sRefs.end(); ++it)
        if (it->second != 0) Line("refs %lld %lld", it->first, it->second);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    bool aMutate = argc > 1 && strcmp(argv[1], "--mutate") == 0;
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 120000;
    for (int n = 0; n < kSequences; n++) {
        sSeed = aStepSeed;
        int aCount = 1 + (int)R(24);
        sLuck = (int)R(3);
        unsigned aSteps = sSeed;
        aStepSeed = sSeed * 69069u + 1u;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps, aCount);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps, aCount);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3 && !aMutate) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) {
                        for (size_t j = i > 4 ? i - 4 : 0; j < i; j++) printf("    %s\n", a[j].c_str());
                        printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y);
                        break;
                    }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            size_t aSecond = a[i].find(' ', aSpace + 1);
            std::string aKey;
            if (aHead == "ret" || aHead == "free" || aHead == "refs" || aHead == "fields" || aHead == "reenter")
                aKey = aHead;
            else if (aHead == "call") // a call and its first argument (the notice's code, the control, the event type)
                aKey = a[i].substr(0, a[i].find(' ', a[i].find(' ', aSecond + 1) + 1));
            else
                aKey = a[i].substr(0, aSecond);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d %u %u %d\n", n, aSeed, aSteps, aCount);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aMutate) {
        printf("won_routelogic_test: %d of %d sequences differ\n", aDiffer, kSequences);
        return 0;
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_routelogic_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
