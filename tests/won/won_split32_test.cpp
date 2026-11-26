// The bodies R6 WON round 32 moved to Rust that no LAN game runs, against their deleted C++ (as they were at
// af02fb9a5), each over a stand-in world that logs every call (R6 WON round 33; "WON goldens by reach"):
//   Disp  LobbyStagingSupport.cpp's network dispatch (netdisp.rs): every entry point, Logout, LeaveGame,
//         RemoveGameAdvertisement, KickClient and Resume among them;
//   Run   LobbyRunState.cpp (lobbyrun.rs): LobbyRun, LobbyExit, LobbySuspend, LobbyResume, LobbyStart;
//   Cfg   LobbyConfig::PostParse (lobbycfg.rs): its four error branches and the defaults;
//   Root  RootScreen (rootscreen.rs): random call sequences on the Internet and LAN screens, SWAT layouts among them;
//   Glue  the EE glue's cases gluesplit.rs composes in the game (TextWidth, SaveNew, SaveLength, SaveLoadHeader,
//         AddressesGet, DeleteCommunications, and round 33's StringUnits), through ee_won_eeglue_split_op.
// The new side is the Rust over the hosts' one-statement cases, copied as they are in the game's files. Each case runs
// on both sides over worlds made alike; the traces (calls, results, the objects' state after) must be equal. Then
// every listed mutation of a new host (a statement skipped, an answer flipped) must make some case differ: the
// mutation passes run after the real one in the same process (the Rust's function-local statics carry over). The
// exit code is the real pass's mismatches plus the mutations not caught.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "ee/wonapi/WONEventTypes.h"
#include "rust/won_lobby/netdisp.h"
#include "rust/won_lobby/lobbyrun.h"
#include "rust/won_lobby/lobbyprv.h"
#include "rust/won_lobby/lobbycfg.h"
#include "rust/won_lobby/rootscreen.h"
#include "rust/won_lobby/eeglue.h"

// ---- the common part: the trace, the numbers, the random draws, the mutation

static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
static uint64_t sRand = 1;
static unsigned Draw(unsigned n)
{
    sRand ^= sRand << 13;
    sRand ^= sRand >> 7;
    sRand ^= sRand << 17;
    return (unsigned)(sRand % n);
}
// the mutation of this pass (0: none): the phase's number * 1000 + the op
static int sMutate;
// the worlds' stride (the mutation passes run every 7th)
static unsigned sStride = 1;
// (an existence check is only made false: made true it would read through NULL)
enum { MUT_SKIP, MUT_FLIP, MUT_INC, MUT_FALSE };
struct Mutation {
    int mKey;
    int mKind;
};
// a mutated op's answer, or -1 when the host runs it as it is
static bool Mutated(int inPhase, int inOp) { return sMutate == inPhase * 1000 + inOp; }
static intptr_t Mutate(int inKind, intptr_t r) { return inKind == MUT_FLIP ? (r ? 0 : 1) : inKind == MUT_INC ? r + 1 : 0; } // MUT_FALSE, MUT_SKIP: 0

struct RefCount {
    int mRefs = 0;
    virtual ~RefCount() {}
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
};
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    SmartPtr& operator=(T* p)
    {
        if (p) p->AddRef();
        if (this->mPtr) this->mPtr->Release();
        this->mPtr = p;
        return *this;
    }
    SmartPtr& operator=(const SmartPtr& o) { return *this = o.mPtr; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    T* mPtr;
};

static int sChecks, sFailures;
static std::map<std::string, int> sFailed;
// one case: both sides' traces compared (only the real pass prints)
static void Compare(const std::string& inWhat, std::vector<std::string>& a, std::vector<std::string>& b)
{
    sChecks++;
    if (a == b) return;
    sFailures++;
    sFailed[inWhat]++;
    if (sMutate == 0 && sFailures <= 10) {
        printf("FAIL %s:\n", inWhat.c_str());
        for (size_t i = 0; i < a.size() || i < b.size(); i++)
            printf("  %-58s | %s\n", i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
    }
}

// ---- Disp: the network dispatch

namespace Disp {
enum LobbyGameType { LobbyGameType_None };
struct ByteBuffer { int mId; };
struct LobbyPlayer : RefCount { int mId; };
struct LobbyClient : RefCount {
    int mId;
    SmartPtr<LobbyPlayer> mPlayer;
    SmartPtr<LobbyPlayer> GetPlayer() { Log("client " + N(mId) + " GetPlayer"); return mPlayer; }
};
struct LobbyGame : RefCount {
    int mId, mType;
    int GetGameType() { Log("game " + N(mId) + " GetGameType"); return mType; }
};
static std::string Name(const void* p);
struct NetLogic : RefCount {
    int mId;
    bool mOk;
    LobbyClient* mClient;
    std::string L() { return "logic " + N(mId) + " "; }
    bool HostGame(LobbyGame* g) { Log(L() + "HostGame " + Name(g)); return mOk; }
    bool JoinGame(LobbyGame* g) { Log(L() + "JoinGame " + Name(g)); return !mOk; }
    LobbyClient* GetMyClient() { Log(L() + "GetMyClient"); return mClient; }
    void Logout() { Log(L() + "Logout"); }
    void AdvertiseGame(LobbyGame* g) { Log(L() + "AdvertiseGame " + Name(g)); }
    void SendToCaptain(const ByteBuffer* m) { Log(L() + "SendToCaptain " + Name(m)); }
    void BroadcastToGame(const ByteBuffer* m) { Log(L() + "BroadcastToGame " + Name(m)); }
    void SendToClient(LobbyClient* c, const ByteBuffer* m) { Log(L() + "SendToClient " + Name(c) + " " + Name(m)); }
    void RemoveGameAdvertisement(LobbyGame* g) { Log(L() + "RemoveGameAdvertisement " + Name(g)); }
    void LeaveGame(LobbyGame* g) { Log(L() + "LeaveGame " + Name(g)); }
    void KickClient(LobbyClient* c, bool b) { Log(L() + "KickClient " + Name(c) + " " + N(b)); }
    void SetGameInProgress(bool b) { Log(L() + "SetGameInProgress " + N(b)); }
    void Suspend() { Log(L() + "Suspend"); }
    void Resume() { Log(L() + "Resume"); }
};
struct LanLogic : NetLogic {};
struct RoutingLogic : NetLogic {};
struct LobbyData : RefCount {
    SmartPtr<LanLogic> mLanLogic;
    SmartPtr<RoutingLogic> mRoutingLogic;
    LobbyGameType mCurGameType;
};
static SmartPtr<LobbyData> gLobbyData;
// the world: its objects and answers
static LobbyData sData;
static LanLogic sLan;
static RoutingLogic sRouting;
static LobbyGame sGame;
static LobbyClient sClient, sLogicClient;
static LobbyPlayer sPlayer;
static ByteBuffer sBuffer = {7};
static bool sInternet, sLanScreen, sCaptain, sHasGame, sHasClient;
static std::string Name(const void* p)
{
    if (p == NULL) return "null";
    if (p == &sLan || p == static_cast<const NetLogic*>(&sLan)) return "lan";
    if (p == &sRouting || p == static_cast<const NetLogic*>(&sRouting)) return "routing";
    if (p == &sGame) return "game";
    if (p == &sClient) return "client";
    if (p == &sLogicClient) return "logicclient";
    if (p == &sPlayer) return "player";
    if (p == &sBuffer) return "buffer";
    return "?";
}
static bool LobbyOnInternetScreen() { Log("OnInternetScreen"); return sInternet; }
static bool LobbyOnLanScreen() { Log("OnLanScreen"); return sLanScreen; }
static bool LobbyIAmCaptain() { Log("IAmCaptain"); return sCaptain; }
static LobbyGame* GetMyLobbyGame() { Log("GetMyLobbyGame"); return sHasGame ? &sGame : NULL; }
static LobbyClient* GetMyLobbyClient() { Log("GetMyLobbyClient"); return sHasClient ? &sClient : NULL; }

namespace Orig {
// LobbyStagingSupport.cpp's, as they were at af02fb9a5
NetLogic* LobbyGetNetLogic(LobbyGameType type)
{
    if(gLobbyData.get()==NULL) return NULL;
    switch(static_cast<int>(type)) {
    case 1: case 3: return gLobbyData->mLanLogic.get();
    case 2: return gLobbyData->mRoutingLogic.get();
    default: return NULL;
    }
}
NetLogic* LobbyGetCurNetLogic()
{
    if(gLobbyData.get()==NULL) return NULL;
    return LobbyGetNetLogic(gLobbyData->mCurGameType);
}
bool LobbyNetHostGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->GetGameType());
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->HostGame(game);
    return false;
}
bool LobbyNetJoinGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->GetGameType());
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->JoinGame(game);
    return false;
}
LobbyClient* LobbyGetNetMyClient()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic==NULL) return NULL;
    return logic->GetMyClient();
}
void LobbyNetLogout()
{
    if(gLobbyData.get()==NULL) return;
    if(LobbyOnInternetScreen()) gLobbyData->mRoutingLogic->Logout();
    else if(LobbyOnLanScreen()) gLobbyData->mLanLogic->Logout();
}
void LobbyNetAdvertiseGame()
{
    if(!LobbyIAmCaptain()) return;
    NetLogic* logic=LobbyGetCurNetLogic();
    LobbyGame* game=GetMyLobbyGame();
    if(logic!=NULL && game!=NULL) logic->AdvertiseGame(game);
}
LobbyPlayer* GetMyLobbyPlayer()
{
    LobbyClient* client=GetMyLobbyClient();
    return client!=NULL ? client->GetPlayer().get() : NULL;
}
void LobbyNetSendToCaptain(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToCaptain(message);
}
void LobbyNetBroadcastToGame(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->BroadcastToGame(message);
}
void LobbyNetSendToClient(LobbyClient* client,const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToClient(client,message);
}
void LobbyNetRemoveGameAdvertisement(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->GetGameType()));
    if(logic!=NULL) logic->RemoveGameAdvertisement(game);
}
void LobbyNetLeaveGame(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->GetGameType()));
    if(logic!=NULL) logic->LeaveGame(game);
}
void LobbyNetKickClient(LobbyClient* client,bool inBan)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->KickClient(client,inBan);
}
void LobbyNetSetGameInProgress(bool inClose)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SetGameInProgress(inClose);
}
void LobbyNetSuspend()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Suspend();
}
void LobbyNetResume()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Resume();
}
}

// LobbyStagingSupport.cpp's NetHost, as it is
static intptr_t NetHostCases(EeWonNetDispOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonNetDispOp Op;
    NetLogic* logic = reinterpret_cast<NetLogic*>(a);
    switch (inOp) {
    case Op::HasData: return gLobbyData.get() != NULL;
    case Op::LanLogic: return (intptr_t) static_cast<NetLogic*>(gLobbyData->mLanLogic.get());
    case Op::RoutingLogic: return (intptr_t) static_cast<NetLogic*>(gLobbyData->mRoutingLogic.get());
    case Op::CurGameType: return (intptr_t)gLobbyData->mCurGameType;
    case Op::SetCurGameType: gLobbyData->mCurGameType = static_cast<LobbyGameType>(a); return 0;
    case Op::GameType: return (intptr_t)reinterpret_cast<LobbyGame*>(a)->GetGameType();
    case Op::HostGame: return logic->HostGame(reinterpret_cast<LobbyGame*>(b)) ? 1 : 0;
    case Op::JoinGame: return logic->JoinGame(reinterpret_cast<LobbyGame*>(b)) ? 1 : 0;
    case Op::MyClient: return (intptr_t)logic->GetMyClient();
    case Op::OnInternetScreen: return LobbyOnInternetScreen() ? 1 : 0;
    case Op::OnLanScreen: return LobbyOnLanScreen() ? 1 : 0;
    case Op::RoutingLogout: gLobbyData->mRoutingLogic->Logout(); return 0;
    case Op::LanLogout: gLobbyData->mLanLogic->Logout(); return 0;
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::AdvertiseGame: logic->AdvertiseGame(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::MyLobbyClient: return (intptr_t)GetMyLobbyClient();
    case Op::ClientPlayer: return (intptr_t)reinterpret_cast<LobbyClient*>(a)->GetPlayer().get();
    case Op::SendToCaptain: logic->SendToCaptain(reinterpret_cast<const ByteBuffer*>(b)); return 0;
    case Op::BroadcastToGame: logic->BroadcastToGame(reinterpret_cast<const ByteBuffer*>(b)); return 0;
    case Op::SendToClient: logic->SendToClient(reinterpret_cast<LobbyClient*>(b), reinterpret_cast<const ByteBuffer*>(c)); return 0;
    case Op::RemoveGameAdvertisement: logic->RemoveGameAdvertisement(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::LeaveGame: logic->LeaveGame(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::KickClient: logic->KickClient(reinterpret_cast<LobbyClient*>(b), c != 0); return 0;
    case Op::SetGameInProgress: logic->SetGameInProgress(b != 0); return 0;
    case Op::Suspend: logic->Suspend(); return 0;
    case Op::Resume: logic->Resume(); return 0;
    }
    return 0;
}
static const Mutation kMutations[] = {
    {(int)EeWonNetDispOp::HasData, MUT_FALSE}, {(int)EeWonNetDispOp::SetCurGameType, MUT_SKIP},
    {(int)EeWonNetDispOp::OnInternetScreen, MUT_FLIP}, {(int)EeWonNetDispOp::OnLanScreen, MUT_FLIP},
    {(int)EeWonNetDispOp::RoutingLogout, MUT_SKIP}, {(int)EeWonNetDispOp::LanLogout, MUT_SKIP},
    {(int)EeWonNetDispOp::IAmCaptain, MUT_FLIP}, {(int)EeWonNetDispOp::HostGame, MUT_FLIP},
    {(int)EeWonNetDispOp::GameType, MUT_INC}, {(int)EeWonNetDispOp::CurGameType, MUT_INC},
    {(int)EeWonNetDispOp::RemoveGameAdvertisement, MUT_SKIP}, {(int)EeWonNetDispOp::LeaveGame, MUT_SKIP},
    {(int)EeWonNetDispOp::KickClient, MUT_SKIP}, {(int)EeWonNetDispOp::Resume, MUT_SKIP},
    {(int)EeWonNetDispOp::Suspend, MUT_SKIP}, {(int)EeWonNetDispOp::SendToClient, MUT_SKIP},
};
static intptr_t NetHost(EeWonNetDispOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    for (const Mutation& m : kMutations)
        if (m.mKey == (int)inOp && Mutated(1, (int)inOp))
            return m.mKind == MUT_SKIP ? 0 : Mutate(m.mKind, NetHostCases(inOp, a, b, c));
    return NetHostCases(inOp, a, b, c);
}

typedef EeWonNetDispCall C;
// the world `k`: data, the two logics, the current and the game's type (0..4 each), the screens, captain, game,
// client, the client's player, the logics' answers and their own client
static void Make(unsigned k)
{
    sTrace.clear();
    sLan.mId = 1;
    sRouting.mId = 2;
    sGame.mId = 3;
    sClient.mId = 4;
    sLogicClient.mId = 5;
    sData.mLanLogic = (k & 1) ? &sLan : NULL;
    sData.mRoutingLogic = (k & 2) ? &sRouting : NULL;
    sData.mCurGameType = static_cast<LobbyGameType>((k >> 2) % 5);
    sGame.mType = (int)((k >> 2) / 5 % 5);
    unsigned r = (k >> 2) / 25; // the remaining bits
    gLobbyData = (r & 1) ? &sData : NULL;
    sInternet = (r >> 1 & 1) != 0;
    sLanScreen = (r >> 2 & 1) != 0;
    sCaptain = (r >> 3 & 1) != 0;
    sHasGame = (r >> 4 & 1) != 0;
    sHasClient = (r >> 5 & 1) != 0;
    sClient.mPlayer = (r >> 6 & 1) ? &sPlayer : NULL;
    sLan.mOk = sRouting.mOk = (r >> 7 & 1) != 0;
    sLan.mClient = sRouting.mClient = (r >> 8 & 1) ? &sLogicClient : NULL;
}
static const unsigned kWorlds = 4 * 25 * 512;
static intptr_t Call(bool inOrig, C inCall, intptr_t a, intptr_t b)
{
    using namespace Orig;
    if (!inOrig) return ee_won_netdisp_call(NetHost, inCall, a, b);
    LobbyGame* g = reinterpret_cast<LobbyGame*>(a);
    LobbyClient* cl = reinterpret_cast<LobbyClient*>(a);
    const ByteBuffer* m = reinterpret_cast<const ByteBuffer*>(a);
    switch (inCall) {
    case C::NetLogic: return (intptr_t)LobbyGetNetLogic(static_cast<LobbyGameType>(a));
    case C::CurNetLogic: return (intptr_t)LobbyGetCurNetLogic();
    case C::HostGame: return LobbyNetHostGame(g);
    case C::JoinGame: return LobbyNetJoinGame(g);
    case C::MyClient: return (intptr_t)LobbyGetNetMyClient();
    case C::Logout: LobbyNetLogout(); return 0;
    case C::AdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    case C::MyPlayer: return (intptr_t)GetMyLobbyPlayer();
    case C::SendToCaptain: LobbyNetSendToCaptain(m); return 0;
    case C::BroadcastToGame: LobbyNetBroadcastToGame(m); return 0;
    case C::SendToClient: LobbyNetSendToClient(cl, reinterpret_cast<const ByteBuffer*>(b)); return 0;
    case C::RemoveGameAdvertisement: LobbyNetRemoveGameAdvertisement(g); return 0;
    case C::LeaveGame: LobbyNetLeaveGame(g); return 0;
    case C::KickClient: LobbyNetKickClient(cl, b != 0); return 0;
    case C::SetGameInProgress: LobbyNetSetGameInProgress(a != 0); return 0;
    case C::Suspend: LobbyNetSuspend(); return 0;
    case C::Resume: LobbyNetResume(); return 0;
    }
    return 0;
}
static void Run()
{
    struct Case {
        C mCall;
        intptr_t mA, mB;
    };
    const Case kCases[] = {
        {C::NetLogic, 0, 0}, {C::NetLogic, 1, 0}, {C::NetLogic, 2, 0}, {C::NetLogic, 3, 0}, {C::NetLogic, 4, 0},
        {C::CurNetLogic, 0, 0}, {C::HostGame, (intptr_t)&sGame, 0}, {C::JoinGame, (intptr_t)&sGame, 0},
        {C::MyClient, 0, 0}, {C::Logout, 0, 0}, {C::AdvertiseGame, 0, 0}, {C::MyPlayer, 0, 0},
        {C::SendToCaptain, (intptr_t)&sBuffer, 0}, {C::BroadcastToGame, (intptr_t)&sBuffer, 0},
        {C::SendToClient, (intptr_t)&sClient, (intptr_t)&sBuffer}, {C::RemoveGameAdvertisement, (intptr_t)&sGame, 0},
        {C::LeaveGame, (intptr_t)&sGame, 0}, {C::KickClient, (intptr_t)&sClient, 0}, {C::KickClient, (intptr_t)&sClient, 1},
        {C::SetGameInProgress, 1, 0}, {C::SetGameInProgress, 0, 0}, {C::Suspend, 0, 0}, {C::Resume, 0, 0},
    };
    for (const Case& c : kCases)
        for (unsigned k = 0; k < kWorlds; k += sStride) {
            // Logout reads the logic of the screen showing without a test: both need it there
            if (c.mCall == C::Logout && (k & 3) != 3) continue;
            std::vector<std::string> t[2];
            for (int aSide = 0; aSide < 2; aSide++) {
                Make(k);
                intptr_t r = Call(aSide == 0, c.mCall, c.mA, c.mB);
                bool aPointer = c.mCall == C::NetLogic || c.mCall == C::CurNetLogic || c.mCall == C::MyClient || c.mCall == C::MyPlayer;
                Log("result " + (aPointer ? Name(reinterpret_cast<void*>(r)) : N(r)));
                Log("curtype " + N((int)sData.mCurGameType));
                t[aSide].swap(sTrace);
            }
            Compare("disp " + N((int)c.mCall), t[0], t[1]);
        }
    gLobbyData = NULL;
}
}

// ---- Run: the lobby's run functions

namespace RunPh {
enum LobbyStatus { LobbyStatus_None };
enum LobbyRunMode { LobbyRunMode_None };
struct Dialog;
struct Window : RefCount {
    bool mVisible;
    bool PlatformIsVisible() { Log("window PlatformIsVisible"); return mVisible; }
    void RunDialog(Dialog*) { Log("window RunDialog"); }
};
struct Dialog : RefCount {
    Window* mRoot;
    Window* GetRootWindow() { Log("dialog GetRootWindow"); return mRoot; }
};
struct InitLogic : RefCount { void Init() { Log("InitLogic Init"); } };
struct LobbyData : RefCount {
    LobbyStatus mLobbyStatus;
    LobbyRunMode mRunMode;
    bool mSuspended;
    SmartPtr<InitLogic> mInitLogic;
};
static SmartPtr<LobbyData> gLobbyData;
static SmartPtr<Dialog> gLobbyFailureDialog;
static bool gLobbyInitFailed;
static LobbyData sData;
static Dialog sDialog;
static Window sWindow;
static InitLogic sInit;
static bool sHasManager, sLoopEndsData, sMusic;
static int sLoopStatus, sPings;
struct WindowManager {
    void MessageLoop()
    {
        Log("MessageLoop");
        if (gLobbyData.get() != NULL) gLobbyData->mLobbyStatus = static_cast<LobbyStatus>(sLoopStatus);
        // the new side's run state is lobbyprv.rs's (R6 WON lobby/net round 40)
        if (gLobbyData.get() != NULL) ee_won_lobbyprv_set(EeWonLobbyPrvField::LobbyStatus, sLoopStatus);
        if (sLoopEndsData) gLobbyData = NULL;
    }
    void ExitMessageLoop() { Log("ExitMessageLoop"); }
};
static WindowManager sManager;
static WindowManager* GetLobbyWindow() { Log("GetLobbyWindow"); return sHasManager ? &sManager : NULL; }
struct Sound {
    const char* mName;
    void Stop() { Log(std::string("sound Stop ") + mName); }
    void Resume() { Log(std::string("sound Resume ") + mName); }
};
static Sound sMusicSound = {"music"}, sLogonSound = {"logon"};
#define LOBBY_SOUND(n) (strcmp(#n, "LobbyGlobal_LobbyMusic_Sound") == 0 ? &sMusicSound : &sLogonSound)
static void LobbyBroadcastLobbyEvent(int e, bool b) { Log("BroadcastLobbyEvent " + N(e) + " " + N(b)); }
static int LobbyGetNumPingsToDo() { Log("NumPingsToDo"); return sPings; }
static void LobbyKillPings(int n) { Log("KillPings " + N(n)); }
static void LobbyNetSuspend() { Log("NetSuspend"); }
static void LobbyNetResume() { Log("NetResume"); }
static bool LobbyGetLobbyMusic() { Log("GetLobbyMusic"); return sMusic; }

namespace Orig {
// LobbyRunState.cpp's, as they were at af02fb9a5
int LobbyRun()
{
    if(gLobbyInitFailed && gLobbyFailureDialog.get()!=NULL) {
        Window* window=gLobbyFailureDialog->GetRootWindow();
        if(window!=NULL && window->PlatformIsVisible())
            window->RunDialog(gLobbyFailureDialog.get());
        return 0;
    }
    if(gLobbyData.get()==NULL) return 0;
    gLobbyData->mLobbyStatus=static_cast<LobbyStatus>(0);
    gLobbyData->mRunMode=static_cast<LobbyRunMode>(1);
    gLobbyData->mSuspended=false;
    WindowManager* manager=GetLobbyWindow();
    if(manager==NULL) return 0;
    manager->MessageLoop();
    if(gLobbyData.get()==NULL) return 0;
    return gLobbyData->mLobbyStatus;
}
void LobbyExit(int status)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mLobbyStatus=static_cast<LobbyStatus>(status);
    LobbyRunMode mode=gLobbyData->mRunMode;
    LobbyBroadcastLobbyEvent(100013,true);
    if(mode!=0) {
        WindowManager* manager=GetLobbyWindow();
        if(manager!=NULL) manager->ExitMessageLoop();
    }
}
void LobbySuspend()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=true;
    LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop();
    LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop();
    if(LobbyGetNumPingsToDo()>0) {
        LobbyKillPings(1);
        LobbyBroadcastLobbyEvent(100059,true);
    }
    LobbyNetSuspend();
}
void LobbyResume()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=false;
    if(LobbyGetLobbyMusic()) LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Resume();
    LobbyNetResume();
}
void LobbyStart()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mSuspended=false;
    gLobbyData->mInitLogic->Init();
}
}

// LobbyRunState.cpp's RunHost, as it is
static intptr_t RunHostCases(EeWonRunOp inOp, intptr_t a)
{
    typedef EeWonRunOp Op;
    switch (inOp) {
    case Op::InitFailedWithDialog: return gLobbyInitFailed && gLobbyFailureDialog.get() != NULL;
    case Op::FailureWindow: return (intptr_t)gLobbyFailureDialog->GetRootWindow();
    case Op::WindowVisible: return reinterpret_cast<Window*>(a)->PlatformIsVisible() ? 1 : 0;
    case Op::WindowRunFailureDialog: reinterpret_cast<Window*>(a)->RunDialog(gLobbyFailureDialog.get()); return 0;
    case Op::HasData: return gLobbyData.get() != NULL;
    case Op::LobbyWindow: return (intptr_t)GetLobbyWindow();
    case Op::MessageLoop: reinterpret_cast<WindowManager*>(a)->MessageLoop(); return 0;
    case Op::ExitMessageLoop: reinterpret_cast<WindowManager*>(a)->ExitMessageLoop(); return 0;
    case Op::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case Op::MusicStop: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop(); return 0;
    case Op::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop(); return 0;
    case Op::PingsToDo: return LobbyGetNumPingsToDo();
    case Op::KillPings: LobbyKillPings(1); return 0;
    case Op::NetSuspend: LobbyNetSuspend(); return 0;
    case Op::MusicOn: return LobbyGetLobbyMusic() ? 1 : 0;
    case Op::MusicResume: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Resume(); return 0;
    case Op::NetResume: LobbyNetResume(); return 0;
    case Op::InitLogicInit: gLobbyData->mInitLogic->Init(); return 0;
    }
    return 0;
}
static const Mutation kMutations[] = {
    {(int)EeWonRunOp::InitFailedWithDialog, MUT_FALSE}, {(int)EeWonRunOp::WindowVisible, MUT_FLIP},
    {(int)EeWonRunOp::WindowRunFailureDialog, MUT_SKIP},
    {(int)EeWonRunOp::MusicOn, MUT_FLIP}, {(int)EeWonRunOp::MusicResume, MUT_SKIP}, {(int)EeWonRunOp::NetResume, MUT_SKIP},
    {(int)EeWonRunOp::PingsToDo, MUT_INC}, {(int)EeWonRunOp::KillPings, MUT_SKIP}, {(int)EeWonRunOp::InitLogicInit, MUT_SKIP},
};
static intptr_t RunHost(EeWonRunOp inOp, intptr_t a)
{
    for (const Mutation& m : kMutations)
        if (m.mKey == (int)inOp && Mutated(2, (int)inOp))
            return m.mKind == MUT_SKIP ? 0 : Mutate(m.mKind, RunHostCases(inOp, a));
    return RunHostCases(inOp, a);
}

static void Make(unsigned k)
{
    sTrace.clear();
    gLobbyInitFailed = (k & 1) != 0;
    gLobbyFailureDialog = (k >> 1 & 1) ? &sDialog : NULL;
    sDialog.mRoot = (k >> 2 & 1) ? &sWindow : NULL;
    sWindow.mVisible = (k >> 3 & 1) != 0;
    gLobbyData = (k >> 4 & 1) ? &sData : NULL;
    sHasManager = (k >> 5 & 1) != 0;
    sLoopEndsData = (k >> 6 & 1) != 0;
    sMusic = (k >> 7 & 1) != 0;
    sPings = (k >> 8 & 1) ? 3 : (k >> 9 & 1) ? -1 : 0;
    sLoopStatus = (int)(k >> 10 & 3) + 2;
    sData.mLobbyStatus = static_cast<LobbyStatus>(9);
    sData.mRunMode = static_cast<LobbyRunMode>(k >> 12 & 1);
    sData.mSuspended = (k >> 13 & 1) != 0;
    sData.mInitLogic = &sInit;
}
static void Run()
{
    typedef EeWonRunCall RC;
    const RC kCalls[] = {RC::Run, RC::Exit, RC::Suspend, RC::Resume, RC::Start};
    for (RC c : kCalls)
        for (unsigned k = 0; k < (1u << 14); k += sStride) {
            std::vector<std::string> t[2];
            for (int aSide = 0; aSide < 2; aSide++) {
                Make(k);
                intptr_t aStatus = (intptr_t)(k % 7) - 1;
                intptr_t r = 0;
                if (aSide == 1) {
                    // the run state in lobbyprv.rs's lobby while the world has one (R6 WON lobby/net round 40)
                    typedef EeWonLobbyPrvField PF;
                    bool aHad = gLobbyData.get() != NULL;
                    if (aHad) {
                        ee_won_lobbyprv_call(&sData, NULL, EeWonLobbyPrvCall::Construct, 0, 0);
                        ee_won_lobbyprv_set(PF::LobbyStatus, sData.mLobbyStatus);
                        ee_won_lobbyprv_set(PF::RunMode, sData.mRunMode);
                        ee_won_lobbyprv_set(PF::Suspended, sData.mSuspended);
                    }
                    r = ee_won_lobbyrun_call(RunHost, c, c == RC::Exit ? aStatus : 0);
                    if (aHad) {
                        sData.mLobbyStatus = static_cast<LobbyStatus>(ee_won_lobbyprv_get(PF::LobbyStatus));
                        sData.mRunMode = static_cast<LobbyRunMode>(ee_won_lobbyprv_get(PF::RunMode));
                        sData.mSuspended = ee_won_lobbyprv_get(PF::Suspended) != 0;
                        ee_won_lobbyprv_drop(&sData);
                    }
                }
                else if (c == RC::Run) r = Orig::LobbyRun();
                else if (c == RC::Exit) Orig::LobbyExit((int)aStatus);
                else if (c == RC::Suspend) Orig::LobbySuspend();
                else if (c == RC::Resume) Orig::LobbyResume();
                else Orig::LobbyStart();
                Log("result " + N(r));
                Log("data " + N(gLobbyData.get() != NULL) + " status " + N((int)sData.mLobbyStatus) + " mode " +
                    N((int)sData.mRunMode) + " suspended " + N(sData.mSuspended));
                t[aSide].swap(sTrace);
            }
            Compare("run " + N((int)c), t[0], t[1]);
        }
    gLobbyData = NULL;
    gLobbyFailureDialog = NULL;
}
}

// ---- Cfg: LobbyConfig::PostParse

namespace Cfg {
struct ConfigParser {
    void SetError(const std::string& s, bool b) { Log("SetError " + s + " " + N(b)); }
};
static std::wstring StringToWString(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::string W(const std::wstring& w) { return std::string(w.begin(), w.end()); }
struct LobbyConfig {
    std::list<std::string> mDirServers;
    std::string mProductName;
    const std::string& GetProductName() const { return this->mProductName; }
    std::wstring mProductDirectory;
    std::string mVersion, mLanVersion, mLanguage, mResourceDirectory;
    std::map<std::string, std::string> mDefines;
    // LobbyConfig.cpp's, as it was at af02fb9a5
    bool PostParse(ConfigParser* parser)
    {
        if (this->mDirServers.empty()) {
            parser->SetError("No Directory Servers specified.", false);
            return false;
        }
        if (this->mProductName.empty()) {
            parser->SetError("No product name specified.", false);
            return false;
        }
        if (this->mVersion.empty()) {
            parser->SetError("No version specified.", false);
            return false;
        }
        if (this->mLanguage.empty()) {
            parser->SetError("No language specified.", false);
            return false;
        }
        if (this->mLanVersion.empty())
            this->mLanVersion = this->mVersion;
        if (this->mResourceDirectory.empty())
            this->mResourceDirectory = ".\\" + this->mProductName;
        if (this->mProductDirectory.empty())
            this->mProductDirectory = std::wstring(L"/") + StringToWString(this->mProductName);
        this->mDefines["Product"] = this->GetProductName();
        return true;
    }
};
// LobbyConfig.cpp's CfgHost, as it is
static intptr_t CfgHostCases(void* inConfig, void* inParser, EeWonLobbyCfgOp inOp)
{
    typedef EeWonLobbyCfgOp Op;
    LobbyConfig* c = static_cast<LobbyConfig*>(inConfig);
    ConfigParser* parser = static_cast<ConfigParser*>(inParser);
    switch (inOp) {
    case Op::DirServersEmpty: return c->mDirServers.empty();
    case Op::ProductNameEmpty: return c->mProductName.empty();
    case Op::VersionEmpty: return c->mVersion.empty();
    case Op::LanguageEmpty: return c->mLanguage.empty();
    case Op::ErrorNoDirServers: parser->SetError("No Directory Servers specified.", false); return 0;
    case Op::ErrorNoProductName: parser->SetError("No product name specified.", false); return 0;
    case Op::ErrorNoVersion: parser->SetError("No version specified.", false); return 0;
    case Op::ErrorNoLanguage: parser->SetError("No language specified.", false); return 0;
    case Op::LanVersionEmpty: return c->mLanVersion.empty();
    case Op::LanVersionFromVersion: c->mLanVersion = c->mVersion; return 0;
    case Op::ResourceDirEmpty: return c->mResourceDirectory.empty();
    case Op::ResourceDirDefault: c->mResourceDirectory = ".\\" + c->mProductName; return 0;
    case Op::ProductDirEmpty: return c->mProductDirectory.empty();
    case Op::ProductDirDefault: c->mProductDirectory = std::wstring(L"/") + StringToWString(c->mProductName); return 0;
    case Op::DefineProduct: c->mDefines["Product"] = c->GetProductName(); return 0;
    }
    return 0;
}
static intptr_t CfgHost(void* inConfig, void* inParser, EeWonLobbyCfgOp inOp)
{
    if (Mutated(3, (int)inOp)) {
        // the checks flipped, the statements skipped
        int o = (int)inOp;
        bool aCheck = o <= 4 || o == 9 || o == 11 || o == 13;
        return aCheck ? Mutate(MUT_FLIP, CfgHostCases(inConfig, inParser, inOp)) : 0;
    }
    return CfgHostCases(inConfig, inParser, inOp);
}
static void Run()
{
    for (unsigned k = 0; k < (1u << 8); k++) {
        std::vector<std::string> t[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            sTrace.clear();
            LobbyConfig c;
            ConfigParser p;
            if (k & 1) c.mDirServers = {"dir1:15101", "dir2:15101"};
            if (k >> 1 & 1) c.mProductName = "EmpireEarth";
            if (k >> 2 & 1) c.mVersion = "2.0";
            if (k >> 3 & 1) c.mLanguage = "English";
            if (k >> 4 & 1) c.mLanVersion = "1.5";
            if (k >> 5 & 1) c.mResourceDirectory = "res";
            if (k >> 6 & 1) c.mProductDirectory = L"/EE";
            if (k >> 7 & 1) c.mDefines["Product"] = "old";
            bool r = aSide == 0 ? c.PostParse(&p) : ee_won_lobbycfg_post_parse(&c, &p, CfgHost);
            Log("result " + N(r) + " lan " + c.mLanVersion + " res " + c.mResourceDirectory + " dir " + W(c.mProductDirectory));
            for (const auto& d : c.mDefines) Log("define " + d.first + "=" + d.second);
            t[aSide].swap(sTrace);
        }
        Compare("cfg", t[0], t[1]);
    }
}
}

// ---- Root: RootScreen

namespace Root {
enum class EeWonShellKind { MsSeperator };
struct WONRectangle {
    int x, y, width, height;
};
struct ComponentEvent;
struct Component : RefCount {
    std::string mName;
    int mHeight = 0, mWidth = 0, mLeft = 0;
    std::string C() { return mName + " "; }
    void SetVisible(bool b) { Log(C() + "SetVisible " + N(b)); }
    int Height() { Log(C() + "Height"); return mHeight; }
    int Width() { Log(C() + "Width"); return mWidth; }
    int Left() { Log(C() + "Left"); return mLeft; }
    void SetPos(int x, int y) { Log(C() + "SetPos " + N(x) + " " + N(y)); }
    void SetSize(int w, int h) { Log(C() + "SetSize " + N(w) + " " + N(h)); }
    void HandleComponentEvent(ComponentEvent* e);
};
struct Container : Component {
    void Virtual08() { Log(C() + "Container::Virtual08"); }
};
struct ComponentEvent {
    int mType;
    Component* mComponent;
    int mControlId;
    int GetControlId() { Log("event GetControlId"); return mControlId; }
};
void Component::HandleComponentEvent(ComponentEvent* e) { Log(C() + "Component::HandleComponentEvent " + N(e->mType)); }
struct TabCtrl : Component {
    int mCur = 0;
    void EnableTab(int t, bool b) { Log("tabs EnableTab " + N(t) + " " + N(b)); }
    int GetCurTabId() { Log("tabs GetCurTabId"); return mCur; }
    void SetCurTab(int t, bool b) { Log("tabs SetCurTab " + N(t) + " " + N(b)); mCur = t; }
    void SetCurTabByName(const std::string& n, bool b) { Log("tabs SetCurTabByName " + n + " " + N(b)); }
    int GetTabIdForContainer(Container* c) { Log("tabs GetTabIdForContainer " + (c ? c->mName : "null")); return c ? c->mLeft : -1; }
};
struct Frame : Component { int mThickness = 0; };
struct Browser : Container { void ResetBrowser() { Log(C() + "ResetBrowser"); } };
struct LayoutPoint : Component {};
struct Window : RefCount {
    WONRectangle mRect, mClient;
    void GetRect(WONRectangle& r, bool inClient) { Log("window GetRect " + N(inClient)); r = inClient ? mClient : mRect; }
    void SetPosSize(int x, int y, int w, int h) { Log("window SetPosSize " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
};
struct ComponentConfig {
    std::map<std::string, Component*> mParts;
    bool mSwat;
    void* Fn386860(const char* n) { Log(std::string("config Fn386860 ") + n); return mSwat ? (void*)this : NULL; }
};
struct ComponentInit { int mType; };
struct ComponentConfigInit : ComponentInit { ComponentConfig* mConfig; };
struct SetRoomSpecFlagsEvent {
    int mFlags;
    explicit SetRoomSpecFlagsEvent(int f) : mFlags(f) {}
};
template <class T> static void SafeGetComponent(ComponentConfig* c, SmartPtr<T>& p, const char* n, const char*, int line)
{
    Log(std::string("SafeGetComponent ") + n + " " + N(line));
    p = static_cast<T*>(c->mParts[n]);
}
template <class T> static void SafeGetShell(ComponentConfig* c, SmartPtr<T>& p, EeWonShellKind, const char* n, const char*, int line)
{
    Log(std::string("SafeGetShell ") + n + " " + N(line));
    p = static_cast<T*>(c->mParts[n]);
}
struct LobbyGame {
    bool mMatches;
    bool MatchesRoomSpec(int f) { Log("game MatchesRoomSpec " + N(f)); return mMatches; }
};
static LobbyGame sGame;
static bool sHasGame, sCrossPromotion;
static LobbyGame* GetMyLobbyGame() { Log("GetMyLobbyGame"); return sHasGame ? &sGame : NULL; }
static bool LobbyGetCrossPromotion() { Log("GetCrossPromotion"); return sCrossPromotion; }
static void LobbySaveUserPrefs(bool b) { Log("SaveUserPrefs " + N(b)); }
static std::string LobbyGetTabSelect() { Log("GetTabSelect"); return "LobbyTab"; }
static Window sWindow;
static int sHeight;

// the members, as RootScreen has them (WONLobbyRoomServer.h)
struct RootBase : Container {
    SmartPtr<TabCtrl> mTabCtrl;
    SmartPtr<LayoutPoint> mLowerLayoutPoint;
    SmartPtr<Component> mLowerDivider, mSierraLogo, mLowerBanner, mBackButton;
    SmartPtr<Frame> mLowerBannerFrame;
    SmartPtr<Browser> mHelpTab, mNewsTab;
    bool mIsLan, mHideLowerControls, mIsSwat;
    int mRoomSpecFlags, mLobbyTab, mLanTab, mDirectConnectTab, mLastJoinTab, mGameTab;
    RootBase() { mName = "root"; mIsLan = mHideLowerControls = mIsSwat = true; mRoomSpecFlags = mLobbyTab = mLanTab = mDirectConnectTab = mLastJoinTab = mGameTab = 77; }
    int Height() { Log("root Height"); return sHeight; }
    void Relayout() { Log("root Relayout"); }
    void BroadcastEventDown(const SetRoomSpecFlagsEvent& e) { Log("root BroadcastEventDown " + N(e.mFlags)); }
    void SubscribeToBroadcast(int n) { Log("root SubscribeToBroadcast " + N(n)); }
    Window* GetRoot() { Log("root GetRoot"); return &sWindow; }
    void Dump()
    {
        Log("fields " + N(mIsLan) + " " + N(mHideLowerControls) + " " + N(mIsSwat) + " " + N(mRoomSpecFlags) + " " +
            N(mLobbyTab) + " " + N(mLanTab) + " " + N(mDirectConnectTab) + " " + N(mLastJoinTab) + " " + N(mGameTab) +
            " tab " + N(mTabCtrl.get() ? mTabCtrl->mCur : -9) + " news " + N(mNewsTab.get() != NULL));
    }
};

// RootScreen.cpp's, as it was at af02fb9a5 (the methods' bodies)
struct OrigRoot : RootBase {
    void EnterGameTab(bool enter)
    {
        if(enter)
        {
            this->mTabCtrl->EnableTab(this->mGameTab,true);
            if(this->mTabCtrl->GetCurTabId()!=this->mGameTab)
            {
                this->mLastJoinTab=this->mTabCtrl->GetCurTabId();
                this->mTabCtrl->SetCurTab(this->mGameTab,true);
            }
        }
        else
        {
            if(this->mTabCtrl->GetCurTabId()==this->mGameTab)
                this->mTabCtrl->SetCurTab(this->mLastJoinTab,true);
            this->mTabCtrl->EnableTab(this->mGameTab,false);
        }
    }
    void ShowLobbyTab(){this->mTabCtrl->SetCurTab(this->mLobbyTab,true);}
    void HandleJoinedGame()
    {
        LobbyGame* aGame=GetMyLobbyGame();
        if(aGame!=NULL && aGame->MatchesRoomSpec(this->mRoomSpecFlags)) this->EnterGameTab(true);
    }
    void HandleLeftGame(){this->EnterGameTab(false);}
    void Virtual08()
    {
        if(this->mLowerLayoutPoint.get()!=NULL)
        {
            if(this->mHideLowerControls)
            {
                this->mLowerDivider->SetVisible(false);
                this->mSierraLogo->SetVisible(false);
                this->mLowerBanner->SetVisible(false);
                this->mLowerBannerFrame->SetVisible(false);
                this->mBackButton->SetVisible(false);
                this->mLowerLayoutPoint->SetPos(this->mLowerLayoutPoint->Left(),this->Height());
                Container::Virtual08();
                return;
            }
            static int aBannerHeight=this->mLowerBanner->Height();
            this->mLowerDivider->SetVisible(true);
            this->mSierraLogo->SetVisible(true);
            this->mBackButton->SetVisible(true);
            int aLowerHeight=this->mLowerDivider->Height();
            if(LobbyGetCrossPromotion())
            {
                this->mLowerBannerFrame->SetVisible(true);
                this->mLowerBanner->SetVisible(true);
                this->mLowerBanner->SetSize(this->mLowerBanner->Width(),aBannerHeight);
                aLowerHeight+=aBannerHeight+2*this->mLowerBannerFrame->mThickness;
            }
            else
            {
                this->mLowerBannerFrame->SetVisible(false);
                this->mLowerBanner->SetVisible(false);
                int aHeight=this->mBackButton->Height();
                if(this->mSierraLogo->Height()>aHeight) aHeight=this->mSierraLogo->Height();
                aLowerHeight+=aHeight;
                this->mLowerBanner->SetSize(this->mLowerBanner->Width(),aHeight);
            }
            this->mLowerLayoutPoint->SetPos(this->mLowerLayoutPoint->Left(),this->Height()-(this->mLowerDivider->Height()-2)/2-aLowerHeight);
        }
        Container::Virtual08();
    }
    void HandleComponentEvent(ComponentEvent* inEvent)
    {
        switch(inEvent->mType)
        {
        case WONAPI::ComponentEvent_TabCtrlTabSelected: this->HandleTabCtrlTabSelected(inEvent); break;
        case WONAPI::ComponentEvent_ButtonPressed:
            if(inEvent->GetControlId()==3) { this->Relayout(); return; }
            break;
        case 100014: this->HandleEnteredScreen(); return;
        case 100029: this->HandleJoinedGame(); return;
        case 100032: this->HandleLeftGame(); return;
        }
        Component::HandleComponentEvent(inEvent);
    }
    void Construct(bool inIsLan)
    {
        this->mIsLan=inIsLan;
        if(inIsLan) this->mRoomSpecFlags=8;
        else this->mRoomSpecFlags=4;
        this->mLobbyTab=this->mLanTab=this->mDirectConnectTab=this->mLastJoinTab=-1;
        this->mHideLowerControls=false;
        this->mIsSwat=false;
    }
    bool OnDirectConnectTab()
    {
        return this->mIsLan && this->mTabCtrl->GetCurTabId()==this->mDirectConnectTab;
    }
    bool OnLanTab()
    {
        return this->mIsLan && this->mTabCtrl->GetCurTabId()==this->mLanTab;
    }
    void HandleEnteredScreen()
    {
        this->mHelpTab->ResetBrowser();
        if(this->mNewsTab.get()!=NULL) this->mNewsTab->ResetBrowser();
        TabCtrl* aTabs=this->mTabCtrl.get();
        aTabs->SetCurTabByName(LobbyGetTabSelect(),true);
    }
    void InitComponent(ComponentInit& inInit)
    {
        if(inInit.mType!=2) return;
        ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
        this->mIsSwat=aConfig->Fn386860("SWATUJDEF")!=NULL;
        SafeGetComponent(aConfig,this->mTabCtrl,"TabCtrl",__FILE__,79);
        SafeGetComponent(aConfig,this->mLowerLayoutPoint,"LowerLayoutPoint",__FILE__,80);
        SafeGetShell(aConfig,this->mLowerDivider,EeWonShellKind::MsSeperator,"LowerDivider",__FILE__,81);
        SafeGetComponent(aConfig,this->mSierraLogo,"SierraLogo",__FILE__,82);
        SafeGetComponent(aConfig,this->mLowerBanner,"LowerBanner",__FILE__,83);
        SafeGetComponent(aConfig,this->mLowerBannerFrame,"LowerBannerFrame",__FILE__,84);
        SafeGetComponent(aConfig,this->mBackButton,"BackButton",__FILE__,85);
        SafeGetComponent(aConfig,this->mHelpTab,"HelpTab",__FILE__,86);
        SmartPtr<Container> aTab;
        SafeGetComponent(aConfig,aTab,"GameTab",__FILE__,89);
        this->mGameTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
        this->mTabCtrl->EnableTab(this->mGameTab,false);
        if(!this->mIsLan)
        {
            SafeGetComponent(aConfig,this->mNewsTab,"NewsTab",__FILE__,95);
            SafeGetComponent(aConfig,aTab,"LobbyTab",__FILE__,96);
            this->mLobbyTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
            this->BroadcastEventDown(SetRoomSpecFlagsEvent(4));
        }
        else
        {
            SafeGetComponent(aConfig,aTab,"LanTab",__FILE__,101);
            this->mLanTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
            SafeGetComponent(aConfig,aTab,"DirectConnectTab",__FILE__,102);
            this->mDirectConnectTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
            this->BroadcastEventDown(SetRoomSpecFlagsEvent(8));
        }
        this->SubscribeToBroadcast(1);
    }
    void HandleTabCtrlTabSelected(ComponentEvent* inEvent)
    {
        if(inEvent->mComponent!=this->mTabCtrl.get()) return;
        if(this->mIsSwat)
        {
            static bool aResized=false;
            static int aOldHeight;
            Window* aWindow=this->GetRoot();
            WONRectangle aWindowRect;
            WONRectangle aClientRect;
            aWindow->GetRect(aWindowRect,false);
            aWindow->GetRect(aClientRect,true);
            int aWindowHeight=aWindowRect.height;
            int aClientHeight=aClientRect.height;
            if(this->mTabCtrl->GetCurTabId()==2)
            {
                this->mHideLowerControls=true;
                aOldHeight=aWindowRect.height;
                aResized=true;
                aWindow->SetPosSize(aWindowRect.x,aWindowRect.y,aWindowRect.width,
                    aWindowHeight-aClientHeight+150);
            }
            else if(aResized)
            {
                this->mHideLowerControls=false;
                aResized=false;
                aWindow->SetPosSize(aWindowRect.x,aWindowRect.y,aWindowRect.width,aOldHeight);
            }
            this->Relayout();
        }
        LobbySaveUserPrefs(false);
    }
};

// RootScreen.cpp's host, as it is
typedef RootBase RootScreen;
typedef EeWonRootScreenOp RO;
typedef EeWonRootScreenCall RC;
static SmartPtr<Container> sRootTab;
static WONRectangle sRootWindowRect;
static WONRectangle sRootClientRect;
static Window* sRootWindow;
static Component* RootPart(RootScreen* s, intptr_t inPart)
{
    switch ((EeWonRootScreenPart)inPart) {
    case EeWonRootScreenPart::LowerLayoutPoint: return s->mLowerLayoutPoint.get();
    case EeWonRootScreenPart::LowerDivider: return s->mLowerDivider.get();
    case EeWonRootScreenPart::SierraLogo: return s->mSierraLogo.get();
    case EeWonRootScreenPart::LowerBanner: return s->mLowerBanner.get();
    case EeWonRootScreenPart::LowerBannerFrame: return s->mLowerBannerFrame.get();
    case EeWonRootScreenPart::BackButton: return s->mBackButton.get();
    }
    return NULL;
}
static intptr_t RootField(RootScreen* s, intptr_t inField, bool inSet, intptr_t v)
{
    typedef EeWonRootScreenField F;
    switch ((F)inField) {
    case F::IsLan: if (inSet) s->mIsLan = v != 0; return s->mIsLan;
    case F::HideLowerControls: if (inSet) s->mHideLowerControls = v != 0; return s->mHideLowerControls;
    case F::IsSwat: if (inSet) s->mIsSwat = v != 0; return s->mIsSwat;
    case F::LastJoinTab: if (inSet) s->mLastJoinTab = (int)v; return s->mLastJoinTab;
    case F::GameTab: if (inSet) s->mGameTab = (int)v; return s->mGameTab;
    case F::DirectConnectTab: if (inSet) s->mDirectConnectTab = (int)v; return s->mDirectConnectTab;
    case F::LanTab: if (inSet) s->mLanTab = (int)v; return s->mLanTab;
    case F::LobbyTab: if (inSet) s->mLobbyTab = (int)v; return s->mLobbyTab;
    case F::RoomSpecFlags: if (inSet) s->mRoomSpecFlags = (int)v; return s->mRoomSpecFlags;
    }
    return 0;
}
static intptr_t RootHostCases(void* inScreen, EeWonRootScreenOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    RootScreen* s = static_cast<RootScreen*>(inScreen);
    ComponentConfig* aConfig = reinterpret_cast<ComponentConfig*>(a);
    switch (inOp) {
    case RO::FieldGet: return RootField(s, a, false, 0);
    case RO::FieldSet: return RootField(s, a, true, b);
    case RO::TabCurId: return s->mTabCtrl->GetCurTabId();
    case RO::TabEnable: s->mTabCtrl->EnableTab((int)a, b != 0); return 0;
    case RO::TabSetCur: s->mTabCtrl->SetCurTab((int)a, true); return 0;
    case RO::TabSetCurByPref: s->mTabCtrl->SetCurTabByName(LobbyGetTabSelect(), true); return 0;
    case RO::TabIdOfFound: return s->mTabCtrl->GetTabIdForContainer(sRootTab.get());
    case RO::TabFoundClear: sRootTab = NULL; return 0;
    case RO::PartExists: return RootPart(s, a) != NULL;
    case RO::PartSetVisible: RootPart(s, a)->SetVisible(b != 0); return 0;
    case RO::PartHeight: return RootPart(s, a)->Height();
    case RO::PartWidth: return RootPart(s, a)->Width();
    case RO::PartLeft: return RootPart(s, a)->Left();
    case RO::PartSetPos: RootPart(s, a)->SetPos((int)b, (int)c); return 0;
    case RO::PartSetSize: RootPart(s, a)->SetSize((int)b, (int)c); return 0;
    case RO::FrameThickness: return s->mLowerBannerFrame->mThickness;
    case RO::BaseSizeChanged: s->Container::Virtual08(); return 0;
    case RO::Height: return s->Height();
    case RO::Relayout: s->Relayout(); return 0;
    case RO::BaseEvent: s->Component::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(a)); return 0;
    case RO::EventControlId: return reinterpret_cast<ComponentEvent*>(a)->GetControlId();
    case RO::EventFromTabs: return reinterpret_cast<ComponentEvent*>(a)->mComponent == s->mTabCtrl.get();
    case RO::MyGame: return (intptr_t)GetMyLobbyGame();
    case RO::GameMatches: return reinterpret_cast<LobbyGame*>(a)->MatchesRoomSpec((int)b) ? 1 : 0;
    case RO::CrossPromotion: return LobbyGetCrossPromotion() ? 1 : 0;
    case RO::SaveUserPrefs: LobbySaveUserPrefs(false); return 0;
    case RO::HelpReset: s->mHelpTab->ResetBrowser(); return 0;
    case RO::NewsExists: return s->mNewsTab.get() != NULL;
    case RO::NewsReset: s->mNewsTab->ResetBrowser(); return 0;
    case RO::InitType: return reinterpret_cast<ComponentInit*>(a)->mType;
    case RO::InitConfig: return (intptr_t) static_cast<ComponentConfigInit*>(reinterpret_cast<ComponentInit*>(a))->mConfig;
    case RO::ConfigHasSwat: return aConfig->Fn386860("SWATUJDEF") != NULL;
    case RO::FindTabCtrl: SafeGetComponent(aConfig, s->mTabCtrl, "TabCtrl", __FILE__, 79); return 0;
    case RO::FindLowerLayoutPoint: SafeGetComponent(aConfig, s->mLowerLayoutPoint, "LowerLayoutPoint", __FILE__, 80); return 0;
    case RO::FindLowerDivider: SafeGetShell(aConfig, s->mLowerDivider, EeWonShellKind::MsSeperator, "LowerDivider", __FILE__, 81); return 0;
    case RO::FindSierraLogo: SafeGetComponent(aConfig, s->mSierraLogo, "SierraLogo", __FILE__, 82); return 0;
    case RO::FindLowerBanner: SafeGetComponent(aConfig, s->mLowerBanner, "LowerBanner", __FILE__, 83); return 0;
    case RO::FindLowerBannerFrame: SafeGetComponent(aConfig, s->mLowerBannerFrame, "LowerBannerFrame", __FILE__, 84); return 0;
    case RO::FindBackButton: SafeGetComponent(aConfig, s->mBackButton, "BackButton", __FILE__, 85); return 0;
    case RO::FindHelpTab: SafeGetComponent(aConfig, s->mHelpTab, "HelpTab", __FILE__, 86); return 0;
    case RO::FindGameTab: SafeGetComponent(aConfig, sRootTab, "GameTab", __FILE__, 89); return 0;
    case RO::FindNewsTab: SafeGetComponent(aConfig, s->mNewsTab, "NewsTab", __FILE__, 95); return 0;
    case RO::FindLobbyTab: SafeGetComponent(aConfig, sRootTab, "LobbyTab", __FILE__, 96); return 0;
    case RO::FindLanTab: SafeGetComponent(aConfig, sRootTab, "LanTab", __FILE__, 101); return 0;
    case RO::FindDirectConnectTab: SafeGetComponent(aConfig, sRootTab, "DirectConnectTab", __FILE__, 102); return 0;
    case RO::BroadcastRoomSpec: s->BroadcastEventDown(SetRoomSpecFlagsEvent((int)a)); return 0;
    case RO::Subscribe: s->SubscribeToBroadcast(1); return 0;
    case RO::WindowGet: sRootWindow = s->GetRoot(); return 0;
    case RO::WindowRectGet: sRootWindow->GetRect(sRootWindowRect, false); return 0;
    case RO::ClientRectGet: sRootWindow->GetRect(sRootClientRect, true); return 0;
    case RO::WindowHeight: return sRootWindowRect.height;
    case RO::ClientHeight: return sRootClientRect.height;
    case RO::WindowSetHeight: sRootWindow->SetPosSize(sRootWindowRect.x, sRootWindowRect.y, sRootWindowRect.width, (int)a); return 0;
    }
    return 0;
}
static const Mutation kMutations[] = {
    {(int)RO::TabCurId, MUT_INC}, {(int)RO::TabEnable, MUT_SKIP}, {(int)RO::TabSetCur, MUT_SKIP},
    {(int)RO::PartSetVisible, MUT_SKIP}, {(int)RO::PartHeight, MUT_INC}, {(int)RO::PartLeft, MUT_INC},
    {(int)RO::FrameThickness, MUT_INC}, {(int)RO::Height, MUT_INC}, {(int)RO::Relayout, MUT_SKIP},
    {(int)RO::EventControlId, MUT_INC}, {(int)RO::EventFromTabs, MUT_FLIP}, {(int)RO::GameMatches, MUT_FLIP},
    {(int)RO::CrossPromotion, MUT_FLIP}, {(int)RO::SaveUserPrefs, MUT_SKIP}, {(int)RO::NewsExists, MUT_FALSE},
    {(int)RO::ConfigHasSwat, MUT_FLIP}, {(int)RO::BroadcastRoomSpec, MUT_SKIP}, {(int)RO::WindowHeight, MUT_INC},
    {(int)RO::ClientHeight, MUT_INC}, {(int)RO::WindowSetHeight, MUT_SKIP}, {(int)RO::FieldSet, MUT_SKIP},
    {(int)RO::BaseEvent, MUT_SKIP}, {(int)RO::TabIdOfFound, MUT_INC},
};
static intptr_t RootHost(void* inScreen, EeWonRootScreenOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    for (const Mutation& m : kMutations)
        if (m.mKey == (int)inOp && Mutated(4, (int)inOp))
            return m.mKind == MUT_SKIP ? 0 : Mutate(m.mKind, RootHostCases(inScreen, inOp, a, b, c));
    return RootHostCases(inScreen, inOp, a, b, c);
}

// one side's screen and layout (made alike for the two)
struct World {
    TabCtrl mTabs;
    LayoutPoint mPoint;
    Component mDivider, mLogo, mBanner, mBack;
    Frame mFrame;
    Browser mHelp, mNews;
    Container mGameTab, mLobbyTab, mLanTab, mDirectTab;
    ComponentConfig mConfig;
    World(bool inSwat)
    {
        const char* kNames[] = {"tabs", "point", "divider", "logo", "banner", "back", "frame", "help", "news", "gametab", "lobbytab", "lantab", "directtab"};
        Component* kParts[] = {&mTabs, &mPoint, &mDivider, &mLogo, &mBanner, &mBack, &mFrame, &mHelp, &mNews, &mGameTab, &mLobbyTab, &mLanTab, &mDirectTab};
        const char* kKeys[] = {"TabCtrl", "LowerLayoutPoint", "LowerDivider", "SierraLogo", "LowerBanner", "BackButton", "LowerBannerFrame", "HelpTab", "NewsTab", "GameTab", "LobbyTab", "LanTab", "DirectConnectTab"};
        for (int i = 0; i < 13; i++) {
            kParts[i]->mName = kNames[i];
            kParts[i]->mHeight = (int)Draw(60) + 1;
            kParts[i]->mWidth = (int)Draw(400);
            kParts[i]->mLeft = (int)Draw(4); // the tab containers' ids
            mConfig.mParts[kKeys[i]] = kParts[i];
        }
        mFrame.mThickness = (int)Draw(4);
        mTabs.mCur = (int)Draw(4);
        mConfig.mSwat = inSwat;
    }
};
static void Run(int inSequences)
{
    for (int aSeq = 0; aSeq < inSequences; aSeq++) {
        uint64_t aSeed = 0x9e3779b97f4a7c15ull * (uint64_t)(aSeq + 1);
        std::vector<std::string> t[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            sRand = aSeed;
            sTrace.clear();
            bool aLan = Draw(2) != 0, aSwat = Draw(3) != 0;
            World w(aSwat);
            OrigRoot o;
            RootBase n;
            RootBase* s = aSide == 0 ? static_cast<RootBase*>(&o) : &n;
            auto call = [&](RC inCall, intptr_t a, intptr_t b) { return ee_won_rootscreen_call(&n, RootHost, inCall, a, b); };
            if (aSide == 0) o.Construct(aLan);
            else call(RC::Construct, aLan, 0);
            ComponentConfigInit anInit;
            anInit.mType = 2;
            anInit.mConfig = &w.mConfig;
            if (aSide == 0) o.InitComponent(anInit);
            else call(RC::InitComponent, (intptr_t)&anInit, 0);
            s->Dump();
            int aSteps = 1 + (int)Draw(12);
            for (int i = 0; i < aSteps; i++) {
                // the world between calls
                if (Draw(3) == 0) w.mTabs.mCur = (int)Draw(4);
                sHasGame = Draw(3) != 0;
                sGame.mMatches = Draw(2) != 0;
                sCrossPromotion = Draw(2) != 0;
                sHeight = 300 + (int)Draw(400);
                sWindow.mRect = {(int)Draw(50), (int)Draw(50), 600 + (int)Draw(200), 400 + (int)Draw(300)};
                sWindow.mClient = {0, 0, 590, 380 + (int)Draw(300)};
                for (Component* p : {(Component*)&w.mDivider, (Component*)&w.mLogo, (Component*)&w.mBack, (Component*)&w.mBanner})
                    if (Draw(4) == 0) p->mHeight = (int)Draw(60) + 1;
                ComponentEvent e;
                static const int kTypes[] = {WONAPI::ComponentEvent_TabCtrlTabSelected, WONAPI::ComponentEvent_ButtonPressed,
                                             100014, 100029, 100032, WONAPI::ComponentEvent_InputReturn};
                e.mType = kTypes[Draw(6)];
                e.mComponent = Draw(4) ? static_cast<Component*>(&w.mTabs) : &w.mBack;
                e.mControlId = Draw(2) ? 3 : 4;
                ComponentConfigInit anOther;
                anOther.mType = 1;
                anOther.mConfig = &w.mConfig;
                intptr_t r = 0;
                switch (Draw(12)) {
                case 0: if (aSide == 0) o.Virtual08(); else call(RC::SizeChanged, 0, 0); break;
                case 1: if (aSide == 0) o.HandleComponentEvent(&e); else call(RC::Event, (intptr_t)&e, e.mType); break;
                case 2: r = aSide == 0 ? o.OnDirectConnectTab() : call(RC::OnDirectConnectTab, 0, 0); break;
                case 3: r = aSide == 0 ? o.OnLanTab() : call(RC::OnLanTab, 0, 0); break;
                case 4: if (aSide == 0) o.EnterGameTab(true); else call(RC::EnterGameTab, 1, 0); break;
                case 5: if (aSide == 0) o.EnterGameTab(false); else call(RC::EnterGameTab, 0, 0); break;
                case 6: if (aSide == 0) o.ShowLobbyTab(); else call(RC::ShowLobbyTab, 0, 0); break;
                case 7: if (aSide == 0) o.HandleJoinedGame(); else call(RC::JoinedGame, 0, 0); break;
                case 8: if (aSide == 0) o.HandleLeftGame(); else call(RC::LeftGame, 0, 0); break;
                case 9: if (aSide == 0) o.HandleEnteredScreen(); else call(RC::EnteredScreen, 0, 0); break;
                case 10: if (aSide == 0) o.HandleTabCtrlTabSelected(&e); else call(RC::TabSelected, (intptr_t)&e, 0); break;
                default: if (aSide == 0) o.InitComponent(anOther); else call(RC::InitComponent, (intptr_t)&anOther, 0); break;
                }
                Log("result " + N(r != 0));
                s->Dump();
            }
            t[aSide].swap(sTrace);
            sRootTab = NULL;
        }
        Compare("root", t[0], t[1]);
    }
}
}

// ---- Glue: the EE glue's cases gluesplit.rs composes in the game

namespace Glue {
static int sFrees;
struct GUIStringData : RefCount {
    wchar_t* mText;
    unsigned short mLength;
    explicit GUIStringData(const std::wstring& s) : mLength((unsigned short)s.size())
    {
        mText = new wchar_t[s.size() + 1];
        wmemcpy(mText, s.c_str(), s.size() + 1);
    }
    ~GUIStringData() { delete[] mText; }
};
static void Drop(GUIStringData* d)
{
    if (d != NULL && d->mRefs == 0) {
        Log("text freed " + std::string(d->mText, d->mText + d->mLength));
        delete d;
    }
}
struct GUIString {
    GUIStringData* mData;
    explicit GUIString(GUIStringData* d) : mData(d) { d->AddRef(); }
    GUIString(const GUIString& o) : mData(o.mData) { mData->AddRef(); }
    ~GUIString() { mData->Release(); Drop(mData); }
    int length() const { return mData->mLength; }
};
static GUIString GlueText(intptr_t inKind, intptr_t inValue)
{
    Log("GlueText " + N(inKind) + " " + N(inValue));
    std::wstring s(1 + (size_t)(inValue % 5), (wchar_t)(L'a' + inKind % 26));
    return GUIString(new GUIStringData(s));
}
struct Font {
    int GetStringWidth(const GUIString& s, int start, int len)
    {
        Log("GetStringWidth " + std::string(s.mData->mText, s.mData->mText + s.mData->mLength) + " " + N(start) + " " + N(len));
        return len * 7 + start;
    }
};
struct Graphics { Font* mFont; };
static const int keseOK = 0;
static const unsigned long kSaveGameMajorVersion = 3, kSaveGameMinorVersion = 14;
static bool sLengthWrites, sLoadOk;
static unsigned long sLength;
struct FSFileSpec {
    void SetFile(const char* f) { Log(std::string("SetFile ") + f); }
    int GetLength(unsigned long& l) { Log("GetLength"); if (sLengthWrites) l = sLength; return sLengthWrites ? 0 : 5; }
};
struct Header {
    void SetMajorVersion(unsigned long v) { Log("SetMajorVersion " + N((long long)v)); }
    void SetMinorVersion(unsigned long v) { Log("SetMinorVersion " + N((long long)v)); }
    int LoadFromFile(FSFileSpec&) { Log("LoadFromFile"); return sLoadOk ? keseOK : 3; }
};
struct EELobbySavedGame {
    FSFileSpec mSpec;
    Header mHeader;
};
struct LobbyNetAddress {
    unsigned long mHandle;
    std::string mName;
};
static int sAddresses;
static void LobbyGetNetAddresses(std::list<LobbyNetAddress>& l, bool b)
{
    Log("GetNetAddresses " + N(l.size()) + " " + N(b));
    for (int i = 0; i < sAddresses; i++) l.push_back({0x0a000001ul + (unsigned long)i, "adapter" + N(i)});
}
static bool sCommDeleted;
struct NECommunications {
    virtual ~NECommunications() { Log("communications deleted"); sCommDeleted = true; }
};
static NECommunications* gCommunications;

// EELobbyGlueHost.cpp's deleted cases, as they were at af02fb9a5 (StringUnits: at round 33's start)
static intptr_t OrigCase(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonEeGlueOp Op;
    EELobbySavedGame* aSave = reinterpret_cast<EELobbySavedGame*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    switch (inOp) {
    case Op::TextWidth: {
        GUIString aText = GlueText(b, c);
        return g->mFont->GetStringWidth(aText, 0, aText.length());
    }
    case Op::StringUnits: {
        GUIStringData* aData = reinterpret_cast<GUIStringData*>(a);
        if (b != 0)
            for (unsigned i = 0; i < aData->mLength; i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)aData->mText[i];
        return aData->mLength;
    }
    case Op::SaveNew: {
        EELobbySavedGame* aNew = new EELobbySavedGame;
        aNew->mSpec.SetFile(reinterpret_cast<const char*>(a));
        return (intptr_t)aNew;
    }
    case Op::SaveLength: {
        unsigned long theLength = 0;
        aSave->mSpec.GetLength(theLength);
        return (intptr_t)theLength;
    }
    case Op::SaveLoadHeader:
        aSave->mHeader.SetMajorVersion(kSaveGameMajorVersion);
        aSave->mHeader.SetMinorVersion(kSaveGameMinorVersion);
        return aSave->mHeader.LoadFromFile(aSave->mSpec) == keseOK ? 1 : 0;
    case Op::AddressesGet: {
        std::list<LobbyNetAddress> theAddresses;
        LobbyGetNetAddresses(theAddresses, false);
        return (intptr_t)new std::vector<LobbyNetAddress>(theAddresses.begin(), theAddresses.end());
    }
    case Op::DeleteCommunications:
        delete gCommunications;
        gCommunications = NULL;
        return 0;
    default: return 0;
    }
}

// the host's new cases, as they are (EELobbyGlueHost.cpp)
static unsigned long sEELobbySaveLength;
static std::list<LobbyNetAddress> sEELobbyAddressList;
static intptr_t HeldData(const GUIString& inText)
{
    inText.mData->AddRef();
    return (intptr_t)inText.mData;
}
static intptr_t NewCases(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, intptr_t, intptr_t)
{
    typedef EeWonEeGlueOp Op;
    EELobbySavedGame* aSave = reinterpret_cast<EELobbySavedGame*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    switch (inOp) {
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); Drop(reinterpret_cast<GUIStringData*>(a)); return 0;
    case Op::TextHeld: return HeldData(GlueText(a, b));
    case Op::GfxTextWidthHeld: return g->mFont->GetStringWidth(GUIString(reinterpret_cast<GUIStringData*>(b)), 0, reinterpret_cast<GUIStringData*>(b)->mLength);
    case Op::SaveAlloc: return (intptr_t)new EELobbySavedGame;
    case Op::SaveSetFile: aSave->mSpec.SetFile(reinterpret_cast<const char*>(b)); return 0;
    case Op::SaveLengthClear: sEELobbySaveLength = 0; return 0;
    case Op::SaveGetLength: aSave->mSpec.GetLength(sEELobbySaveLength); return 0;
    case Op::SaveLengthValue: return (intptr_t)sEELobbySaveLength;
    case Op::SaveSetMajor: aSave->mHeader.SetMajorVersion(kSaveGameMajorVersion); return 0;
    case Op::SaveSetMinor: aSave->mHeader.SetMinorVersion(kSaveGameMinorVersion); return 0;
    case Op::SaveLoadFile: return aSave->mHeader.LoadFromFile(aSave->mSpec) == keseOK ? 1 : 0;
    case Op::AddressListFill: LobbyGetNetAddresses(sEELobbyAddressList, false); return 0;
    case Op::AddressVectorNew: return (intptr_t)new std::vector<LobbyNetAddress>(sEELobbyAddressList.begin(), sEELobbyAddressList.end());
    case Op::AddressListClear: sEELobbyAddressList.clear(); return 0;
    case Op::CommDelete: delete gCommunications;
        return 0;
    case Op::CommClear: gCommunications = NULL;
        return 0;
    case Op::StringLength: return (intptr_t)reinterpret_cast<GUIStringData*>(a)->mLength;
    case Op::StringUnitAt: return (intptr_t)(uint32_t)reinterpret_cast<GUIStringData*>(a)->mText[b];
    default: Log("unexpected op " + N((long long)inOp)); return 0;
    }
}
typedef EeWonEeGlueOp GO;
static const Mutation kMutations[] = {
    {(int)GO::SaveSetFile, MUT_SKIP}, {(int)GO::SaveLengthClear, MUT_SKIP}, {(int)GO::SaveLengthValue, MUT_INC},
    {(int)GO::SaveSetMajor, MUT_SKIP}, {(int)GO::SaveSetMinor, MUT_SKIP}, {(int)GO::SaveLoadFile, MUT_FLIP},
    {(int)GO::AddressListFill, MUT_SKIP}, {(int)GO::AddressListClear, MUT_SKIP}, {(int)GO::CommDelete, MUT_SKIP},
    {(int)GO::CommClear, MUT_SKIP}, {(int)GO::GfxTextWidthHeld, MUT_INC}, {(int)GO::StringLength, MUT_INC},
    {(int)GO::StringUnitAt, MUT_INC}, {(int)GO::Release, MUT_SKIP},
};
static intptr_t NewHost(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    for (const Mutation& m : kMutations)
        if (m.mKey == (int)inOp && Mutated(5, (int)inOp))
            return m.mKind == MUT_SKIP ? 0 : Mutate(m.mKind, NewCases(inOp, a, b, c, d, e, f));
    return NewCases(inOp, a, b, c, d, e, f);
}

static void Run()
{
    Font aFont;
    Graphics g = {&aFont};
    sEELobbyAddressList.clear();
    static const char kFile[] = "Users\\Saved Games\\game1.ees";
    for (unsigned k = 0; k < 64; k++) {
        const GO kOps[] = {GO::TextWidth, GO::StringUnits, GO::SaveNew, GO::SaveLength, GO::SaveLoadHeader, GO::AddressesGet,
                           GO::DeleteCommunications};
        for (GO op : kOps) {
            std::vector<std::string> t[2];
            for (int aSide = 0; aSide < 2; aSide++) {
                sTrace.clear();
                sLengthWrites = (k & 1) != 0;
                sLength = 1000 + k * 977;
                sLoadOk = (k >> 1 & 1) != 0;
                sAddresses = (int)(k >> 2 & 3);
                gCommunications = (k >> 4 & 1) ? new NECommunications : NULL;
                sCommDeleted = false;
                EELobbySavedGame aSave;
                GUIStringData* aText = new GUIStringData(std::wstring(L"Name\x00e9\x4e2d", (size_t)(k % 8 > 6 ? 0 : k % 8)));
                aText->AddRef();
                uint32_t aUnits[16];
                for (uint32_t& u : aUnits) u = 0xeeee;
                intptr_t a = 0, b = 0, c = 0;
                switch (op) {
                case GO::TextWidth: a = (intptr_t)&g; b = (intptr_t)(k % 3); c = (intptr_t)k; break;
                case GO::StringUnits: a = (intptr_t)aText; b = (k >> 5 & 1) ? 0 : (intptr_t)aUnits; break;
                case GO::SaveNew: a = (intptr_t)kFile; break;
                default: a = (intptr_t)&aSave; break;
                }
                intptr_t r = aSide == 0 ? OrigCase(op, a, b, c) : ee_won_eeglue_split_op(NewHost, op, a, b, c, 0, 0, 0);
                if (op == GO::SaveNew) {
                    Log("result a save");
                    delete reinterpret_cast<EELobbySavedGame*>(r);
                } else if (op == GO::AddressesGet) {
                    auto* v = reinterpret_cast<std::vector<LobbyNetAddress>*>(r);
                    Log("result " + N((long long)v->size()) + " addresses");
                    for (const LobbyNetAddress& x : *v) Log("address " + N((long long)x.mHandle) + " " + x.mName);
                    delete v;
                } else {
                    Log("result " + N(r));
                }
                std::string u;
                for (uint32_t x : aUnits) u += N(x) + " ";
                Log("units " + u + " communications " + N(gCommunications != NULL) + " text refs " + N(aText->mRefs));
                if (!sCommDeleted) delete gCommunications; // (a mutation may leave it deleted but set)
                gCommunications = NULL;
                aText->Release();
                Drop(aText);
                t[aSide].swap(sTrace);
            }
            Compare("glue " + N((int)op), t[0], t[1]);
        }
    }
    // the static list is the original's local: empty after each call
    if (!sEELobbyAddressList.empty()) {
        sEELobbyAddressList.clear();
        std::vector<std::string> a = {"list empty"}, b = {"list not empty"};
        Compare("glue address list", a, b);
    }
}
}

static int RunAll(int inRootSequences)
{
    sChecks = sFailures = 0;
    struct Phase {
        const char* mName;
        void (*mRun)(int);
    };
    const Phase kPhases[] = {{"disp", [](int) { Disp::Run(); }}, {"run", [](int) { RunPh::Run(); }},
                             {"cfg", [](int) { Cfg::Run(); }}, {"root", [](int n) { Root::Run(n); }},
                             {"glue", [](int) { Glue::Run(); }}};
    for (const Phase& p : kPhases) {
        int aBefore = sChecks, aFailed = sFailures;
        p.mRun(inRootSequences);
        if (sMutate == 0) printf("phase %s: %d cases, %d mismatches\n", p.mName, sChecks - aBefore, sFailures - aFailed);
        fflush(stdout);
    }
    return sFailures;
}

int main()
{
    int aFailures = RunAll(20000);
    for (const auto& f : sFailed) printf("mismatching: %s x %d\n", f.first.c_str(), f.second);
    printf("won_split32_test: %d cases, %d mismatches\n", sChecks, aFailures);
    // the mutations: each must be caught
    std::vector<int> aKeys;
    for (const Mutation& m : Disp::kMutations) aKeys.push_back(1000 + m.mKey);
    for (const Mutation& m : RunPh::kMutations) aKeys.push_back(2000 + m.mKey);
    for (int o = 1; o <= (int)EeWonLobbyCfgOp::DefineProduct; o++) aKeys.push_back(3000 + o);
    for (const Mutation& m : Root::kMutations) aKeys.push_back(4000 + m.mKey);
    for (const Mutation& m : Glue::kMutations) aKeys.push_back(5000 + m.mKey);
    int aMissed = 0;
    for (int aKey : aKeys) {
        sMutate = aKey;
        sStride = 7;
        sFailed.clear();
        int n = RunAll(3000);
        if (n == 0) {
            printf("mutation %d not caught\n", aKey);
            aMissed++;
        }
    }
    sMutate = 0;
    printf("won_split32_test: %d of %d mutations caught\n", (int)aKeys.size() - aMissed, (int)aKeys.size());
    return aFailures + aMissed;
}
