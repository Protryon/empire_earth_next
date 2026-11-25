// The logics' host cases R6 WON lobby and network round 36 composed in Rust (ee_won_lobby lobbyops.rs) that no LAN
// game reaches, against the deleted cases as they were at 97ff55e16: LobbyLogicHost.cpp's GameResetReadyFlags (the
// LAN game never changes a setting after a join), ClientReadPlayer (a LAN joiner's player comes with the join reply)
// and RunWaitOp (the connect warnings' wait), LanLogicHost.cpp's LanDirectAsk (the direct-connect dialog); a probe
// over one LAN game found these four unreached. "WON goldens by reach": the round's other moved cases (the join reply,
// the joined broadcast, the subnets, the staging bytes, the summaries) run in every LAN game.
//
// Each case runs on both sides over worlds made alike: the original's statements (Orig), then lobbyops.rs through
// the drivers' entry over the hosts' new one-statement cases, copied as they are (World), and netops.rs's answers
// as the C++ calls they make. The traces (calls, results, the objects' state and references after) must be equal.
// Then every new op is skipped in turn (the World answers 0 and does nothing): each skip must make some case
// differ. The exit code is the real pass's mismatches plus the skips not caught.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "rust/won_net/ipobj.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbyops.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/resource.h"

typedef EeWonLobbyOp Op;

// ---- the world

static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }

static int sLive; // the objects made and not freed
struct RefCount {
    int mId = 0;
    int mRefs = 0;
    RefCount() { sLive++; }
    virtual ~RefCount() { sLive--; }
    void AddRef() { this->mRefs++; }
    void Release() { if (--this->mRefs == 0) delete this; }
};
template <class T> struct SmartPtr {
    T* mPtr = NULL;
    SmartPtr() {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); if (mPtr) mPtr->Release(); mPtr = p; return *this; }
    SmartPtr& operator=(const SmartPtr& o) { return *this = o.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
};
struct GUIString { std::string s; };
struct LobbyResource { GUIString mString; };
static LobbyResource gLobbyResources[1024];
#define LOBBY_STRING(x) gLobbyResources[(int)EeWonLobbyRes::x].mString

// the clients of a game and their players
struct ReadBufferException {};
struct ReadBuffer { int mPos = 0; };
static int sReadOutcome; // a player's read: 0 fails, 1 succeeds, 2 throws
static int sNextOpId;
struct LobbyPlayer : RefCount {
    bool mReady = false;
    void SetReady(bool r) { Log("SetReady " + N(mId) + " " + N(r)); mReady = r; }
    static LobbyPlayer* CreatePlayer() { LobbyPlayer* p = new LobbyPlayer; p->mId = 900 + ++sNextOpId; Log("CreatePlayer " + N(p->mId)); return p; }
    bool ReadFromBuffer(ReadBuffer& b)
    {
        Log("ReadFromBuffer " + N(mId) + " at " + N(b.mPos));
        b.mPos += 4;
        if (sReadOutcome == 2) throw ReadBufferException();
        return sReadOutcome == 1;
    }
};
struct LobbyClient {
    int mId = 0;
    LobbyPlayer* mPlayer = NULL;
    SmartPtr<LobbyPlayer> mHeld;
    void SetPlayer(LobbyPlayer* p) { Log("SetPlayer " + N(mId) + " " + (p != NULL ? N(p->mId) : std::string("null"))); mHeld = p; }
};
struct LobbyClientList : RefCount { std::vector<LobbyClient*> mClients; };
struct LobbyGame { SmartPtr<LobbyClientList> mList; SmartPtr<LobbyClientList> GetClients() { return mList; } };
static int sIterators; // live iterators (the new side's are made and freed by ops)
struct LobbyClientSetIterator {
    LobbyClientSetIterator(LobbyClientList* inList) : mList(inList), mNext(0) { sIterators++; }
    ~LobbyClientSetIterator() { sIterators--; }
    bool HasMore() { return this->mList != NULL && this->mNext < this->mList->mClients.size(); }
    // the next entry's player; a hole of the set leaves the client alone
    LobbyPlayer* Next(LobbyClient** outClient)
    {
        LobbyClient* aClient = this->mList->mClients[this->mNext++];
        if (aClient == NULL) return NULL;
        *outClient = aClient;
        return aClient->mPlayer;
    }
    LobbyClientList* mList;
    size_t mNext;
};
struct PlayerChangedEvent { LobbyClient* mClient; int mHow; PlayerChangedEvent(LobbyClient* c, int h) : mClient(c), mHow(h) {} };
static void LobbyBroadcastEvent(const PlayerChangedEvent& e, bool f) { Log("broadcast PlayerChanged " + N(e.mClient->mId) + " " + N(e.mHow) + " " + N(f)); }

// ops, the status box
static bool sKillInBox, sDnsSucceeds;
struct AsyncOp : RefCount {
    bool mKilled = false;
    AsyncOp() { this->mId = ++sNextOpId; }
    bool IsKilled() { return this->mKilled; }
};
static void LobbyRunOpWithStatus(AsyncOp* o, const GUIString& t, const GUIString& d, int a, int b)
{
    Log("RunOpWithStatus op" + N(o->mId) + "/" + N(o->mRefs) + " " + t.s + " " + d.s + " " + N(a) + " " + N(b));
    o->mKilled = sKillInBox;
}

// the address (the game's IPAddr over its Rust object), the DNS op, the dialog, the config
struct IPAddr {
    IPAddr() : mAddr(ee_won_ipaddr_new()) {}
    IPAddr(const IPAddr& o) : mAddr(ee_won_ipaddr_clone(o.mAddr)) {}
    ~IPAddr() { ee_won_ipaddr_free(this->mAddr); }
    IPAddr& operator=(const IPAddr& o) { ee_won_ipaddr_assign(this->mAddr, o.mAddr); return *this; }
    bool SetWithDefaultPortNoDNS(const std::string& inText, unsigned short inPort)
    { return ee_won_ipaddr_set_no_dns(this->mAddr, reinterpret_cast<const uint8_t*>(inText.data()), inText.length(), inPort); }
    unsigned short GetPort() const { return ee_won_ipaddr_port(this->mAddr); }
    void SetPort(unsigned short inPort) { ee_won_ipaddr_set_port(this->mAddr, inPort); }
    bool IsValid() const { return ee_won_ipaddr_valid(this->mAddr); }
    const EeWonIpAddr* GetValue() const { return this->mAddr; }
    EeWonIpAddr* mAddr;
};
static const int EE_WON_DNSTEST_ADDR = 1;
static std::string sDnsAnswer; // the address the DNS op found ("host[:port]", dotted)
struct RustServerOp : AsyncOp {
    std::string mHost;
    bool Succeeded() { Log("Succeeded op" + N(this->mId)); return sDnsSucceeds; }
    IPAddr AddrOf(int inWhich) { Log("AddrOf " + N(inWhich)); IPAddr a; a.SetWithDefaultPortNoDNS(sDnsAnswer, 0); return a; }
};
static RustServerOp* NewDnsTestOp(const std::string& inHost) { Log("NewDnsTestOp " + inHost); RustServerOp* o = new RustServerOp; o->mHost = inHost; return o; }
enum class EeWonLobbyDlgCall { Reset = 1 };
static std::string sDialogHost;
struct LobbyRustDialog {
    void LCall(EeWonLobbyDlgCall c) { Log("dialog LCall " + N((int)c)); }
    std::string InputText(int inMember) { Log("dialog InputText " + N(inMember)); return sDialogHost; }
    LobbyRustDialog* get() { return this; }
    LobbyRustDialog* operator->() { return this; }
};
static int sDoDialogResult;
static int LobbyDoDialog(LobbyRustDialog*) { Log("DoDialog"); return sDoDialogResult; }
struct LobbyConfig { unsigned short mLobbyPort; };
static LobbyConfig* sConfig;
static LobbyConfig* GetLobbyConfig() { Log("GetLobbyConfig"); return sConfig; }
static unsigned short LobbyTestPortOffset() { return 7; }
static unsigned short LobbyGetLobbyPort() { Log("GetLobbyPort"); return 2300; }
enum { kLanDirectConnect };
struct RustLogic { LobbyRustDialog mDialog[1]; };
static RustLogic sLogic;

// ---- the original's cases (LobbyLogicHost.cpp and LanLogicHost.cpp at 97ff55e16)

static intptr_t Orig(Op inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    RustLogic* l = &sLogic;
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    switch (inOp) {
    case EeWonLobbyOp::GameResetReadyFlags: {
        LobbyClientSetIterator anItr(g->GetClients().get());
        while (anItr.HasMore()) {
            LobbyClient* aClient = NULL;
            LobbyPlayer* aPlayer = anItr.Next(&aClient);
            if (aPlayer != NULL && aClient != NULL) {
                aPlayer->SetReady(false);
                LobbyBroadcastEvent(PlayerChangedEvent(aClient, 2), true);
            }
        }
        return 0;
    }
    case EeWonLobbyOp::ClientReadPlayer:
        try {
            SmartPtr<LobbyPlayer> aPlayer = LobbyPlayer::CreatePlayer();
            if (!aPlayer->ReadFromBuffer(*buf)) return 0;
            cl->SetPlayer(aPlayer.get());
            return 1;
        } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    case EeWonLobbyOp::RunWaitOp: {
        SmartPtr<AsyncOp> anOp = new AsyncOp;
        LobbyRunOpWithStatus(anOp.get(), aTitle, aDetails, (int)inD, 500);
        return anOp->IsKilled();
    }
    case EeWonLobbyOp::LanDirectAsk: {
        l->mDialog[kLanDirectConnect]->LCall(EeWonLobbyDlgCall::Reset);
        if (LobbyDoDialog(l->mDialog[kLanDirectConnect].get()) != 0x186a1)
            return 0;
        IPAddr anAddress;
        LobbyConfig* aConfig = GetLobbyConfig();
        // (the configured port moved by the harness slot's offset, as LobbyBindUDPPort's: TEST ONLY)
        int aDefaultPort = ee_won_lanlogic_direct_default_port(aConfig != NULL,
                                                               aConfig != NULL ? aConfig->mLobbyPort + LobbyTestPortOffset() : 0,
                                                               LobbyGetLobbyPort());
        anAddress.SetWithDefaultPortNoDNS(l->mDialog[kLanDirectConnect]->InputText(1), aDefaultPort);
        if (!anAddress.IsValid()) {
            SmartPtr<RustServerOp> anOp = NewDnsTestOp(l->mDialog[kLanDirectConnect]->InputText(1));
            LobbyRunOpWithStatus(anOp.get(), LOBBY_STRING(LANLogic_CheckingAddressTitle_String), LOBBY_STRING(LANLogic_CheckingAddressDetails_String), 1, 0);
            if (!anOp->Succeeded())
                return 0;
            anAddress = anOp->AddrOf(EE_WON_DNSTEST_ADDR);
            if (anAddress.GetPort() == 0)
                anAddress.SetPort(aDefaultPort);
        }
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), anAddress.GetValue());
        return 1;
    }
    default: return 0;
    }
}

// ---- the new side: the hosts' new one-statement cases as they are, and netops.rs's answers

static int sSkip = -1; // the op skipped (-1: none)
static bool sSkipHit;
static intptr_t sRows; // the rows RunWaitOp's status box was run with (the original's inC)
static void Take(void* inSink, const std::string& inBytes)
{
    ee_won_lobby_sink(inSink, reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.length());
}
template <class T> static intptr_t Held(T* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
}
// WONLobbyStaging.h's, as it is
template <class F> intptr_t LobbyReadGuard(F inRead)
{
    try { return (intptr_t)inRead(); } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
}
static bool NeedsObject(EeWonLobbyOp inOp)
{
    switch (inOp) {
    case EeWonLobbyOp::RefRelease: case EeWonLobbyOp::OpSucceededOf: case EeWonLobbyOp::RunOpWithStatusRows:
    case EeWonLobbyOp::ClientsIterHasMore: case EeWonLobbyOp::ClientsIterNext: case EeWonLobbyOp::ClientsIterFree:
    case EeWonLobbyOp::PlayerSetReady: case EeWonLobbyOp::RunOpWithStatusMode: case EeWonLobbyOp::OpIsKilled:
    case EeWonLobbyOp::PlayerReadFrom:
    case EeWonLobbyOp::ConfigLobbyPortTest: case EeWonLobbyOp::DnsOpAddrAssign: return true;
    default: return false;
    }
}
static intptr_t World(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    RustLogic* l = static_cast<RustLogic*>(inLogic);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    if ((int)inOp == sSkip) {
        sSkipHit = true;
        // (a skipped Next still steps, or the Rust's loop would never end: its player and client are lost)
        if (inOp == EeWonLobbyOp::ClientsIterNext) static_cast<LobbyClientSetIterator*>(inA)->mNext++;
        return 0;
    }
    // a skipped maker's object is missing from the later ops of its case
    if (sSkip >= 0 && (inA == NULL || (inOp == EeWonLobbyOp::DnsOpAddrAssign && inB == NULL)) && NeedsObject(inOp)) { Log("NULL object for op " + N((int)inOp)); return 0; }
    switch (inOp) {
    // netops.rs's (the network objects' Rust in the game) and the hosts' older cases
    case EeWonLobbyOp::RefRelease: static_cast<RefCount*>(inA)->Release(); return 0;
    case EeWonLobbyOp::OpSucceededOf: return static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)))->Succeeded() ? 1 : 0;
    case EeWonLobbyOp::LanTimerAlloc: return Held(static_cast<RefCount*>(new AsyncOp));
    case EeWonLobbyOp::ClientChanged: LobbyBroadcastEvent(PlayerChangedEvent(cl, (int)inC), true); return 0;
    case EeWonLobbyOp::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case EeWonLobbyOp::RunOpWithStatusRows: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, 1, 0);
        return 0;
    // LobbyLogicHost.cpp's new ones
    case EeWonLobbyOp::GameClientsIterNew: return (intptr_t) new LobbyClientSetIterator(g->GetClients().get());
    case EeWonLobbyOp::ClientsIterHasMore: return static_cast<LobbyClientSetIterator*>(inA)->HasMore();
    case EeWonLobbyOp::ClientsIterNext: return (intptr_t) static_cast<LobbyClientSetIterator*>(inA)->Next(static_cast<LobbyClient**>(inB));
    case EeWonLobbyOp::ClientsIterFree: delete static_cast<LobbyClientSetIterator*>(inA); return 0;
    case EeWonLobbyOp::PlayerSetReady: static_cast<LobbyPlayer*>(inA)->SetReady(inC != 0); return 0;
    case EeWonLobbyOp::PlayerCreateHeld: return Held(LobbyPlayer::CreatePlayer());
    case EeWonLobbyOp::PlayerReadFrom: return LobbyReadGuard([&] { return static_cast<LobbyPlayer*>(inA)->ReadFromBuffer(*buf); });
    case EeWonLobbyOp::ClientSetPlayer: cl->SetPlayer(static_cast<LobbyPlayer*>(inB)); return 0;
    case EeWonLobbyOp::RunOpWithStatusMode: sRows = inC; LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, (int)inD, 500); return 0;
    case EeWonLobbyOp::OpIsKilled: return static_cast<AsyncOp*>(static_cast<RefCount*>(inA))->IsKilled();
    // LanLogicHost.cpp's new ones
    case EeWonLobbyOp::LanDirectReset: l->mDialog[kLanDirectConnect]->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case EeWonLobbyOp::LanDirectDo: return LobbyDoDialog(l->mDialog[kLanDirectConnect].get());
    case EeWonLobbyOp::ConfigLobbyPortTest: return static_cast<LobbyConfig*>(inA)->mLobbyPort + LobbyTestPortOffset();
    case EeWonLobbyOp::LobbyPort: return LobbyGetLobbyPort();
    case EeWonLobbyOp::LanDirectHostTake: Take(inB, std::string(l->mDialog[kLanDirectConnect]->InputText(1))); return 0;
    case EeWonLobbyOp::LanDirectDnsHeld: return Held(static_cast<RefCount*>(NewDnsTestOp(l->mDialog[kLanDirectConnect]->InputText(1))));
    case EeWonLobbyOp::DnsOpAddrAssign: ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inB)))->AddrOf(EE_WON_DNSTEST_ADDR).GetValue()); return 0;
    default: Log("UNANSWERED op " + N((int)inOp)); return 0;
    }
}
static intptr_t New(Op inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    return ee_won_lobby_lobbyops_op(&sLogic, inOp, inA, inB, inC, inD, World);
}

// ---- the cases

static int sCases, sMismatches;
static bool sReport;
static void Compare(const std::vector<std::string>& inOrig, const std::string& inWhat)
{
    sCases++;
    if (inOrig == sTrace) return;
    sMismatches++;
    if (!sReport || sMismatches > 10) return;
    printf("MISMATCH %s\n", inWhat.c_str());
    for (size_t i = 0; i < inOrig.size() || i < sTrace.size(); i++)
        printf("  orig %-64s new %s\n", i < inOrig.size() ? inOrig[i].c_str() : "-", i < sTrace.size() ? sTrace[i].c_str() : "-");
}
static void Begin()
{
    sTrace.clear();
    sNextOpId = 0;
}
static void LogAddress(const EeWonIpAddr* a)
{
    Log(std::string("address valid ") + N(ee_won_ipaddr_valid(a)) + " host " + N(ee_won_ipaddr_host(a)) + " port " + N(ee_won_ipaddr_port(a)));
}

// GameResetReadyFlags: a game of inCount clients; inShape's base-3 digits: a client with a player, without one, a hole
static void ResetCase(bool inOrig, int inCount, int inShape)
{
    std::vector<LobbyPlayer> aPlayers(inCount);
    std::vector<LobbyClient> aClients(inCount);
    LobbyGame aGame{SmartPtr<LobbyClientList>(new LobbyClientList)};
    for (int i = 0; i < inCount; i++, inShape /= 3) {
        aPlayers[i].mId = 100 + i;
        aPlayers[i].mReady = true;
        aClients[i].mId = 10 + i;
        aClients[i].mPlayer = inShape % 3 == 1 ? NULL : &aPlayers[i];
        aGame.mList->mClients.push_back(inShape % 3 == 2 ? NULL : &aClients[i]);
    }
    Begin();
    intptr_t aResult = inOrig ? Orig(Op::GameResetReadyFlags, &aGame, NULL, 0, 0) : New(Op::GameResetReadyFlags, &aGame, NULL, 0, 0);
    Log("= " + N(aResult) + " list refs " + N(aGame.mList->mRefs) + " iterators " + N(sIterators));
    for (int i = 0; i < inCount; i++) Log("player " + N(i) + " ready " + N(aPlayers[i].mReady));
}
// ClientReadPlayer: a new player read from the buffer (failing, succeeding or throwing) and set on the client
static void ReadCase(bool inOrig, int inOutcome, bool inHadPlayer)
{
    int aLive = sLive;
    {
        LobbyClient aClient;
        aClient.mId = 31;
        Begin();
        if (inHadPlayer) aClient.mHeld = LobbyPlayer::CreatePlayer();
        ReadBuffer aBuffer;
        aBuffer.mPos = 5;
        sReadOutcome = inOutcome;
        intptr_t aResult = inOrig ? Orig(Op::ClientReadPlayer, &aClient, &aBuffer, 0, 0) : New(Op::ClientReadPlayer, &aClient, &aBuffer, 0, 0);
        Log("= " + N(aResult) + " buffer at " + N(aBuffer.mPos) + " player " + (aClient.mHeld.get() != NULL ? N(aClient.mHeld->mId) + "/" + N(aClient.mHeld->mRefs) : std::string("null")) + " live " + N(sLive - aLive));
    }
    Log("after: live " + N(sLive - aLive));
}
// RunWaitOp: the text code (the new side's; the original's host took its rows), the mode, the box cancelled or not
static void WaitCase(bool inOrig, int inCode, int inMode, bool inKill)
{
    sKillInBox = inKill;
    int aLive = sLive;
    Begin();
    intptr_t aResult = inOrig ? Orig(Op::RunWaitOp, NULL, NULL, sRows, inMode) : New(Op::RunWaitOp, NULL, NULL, inCode, inMode);
    Log("= " + N(aResult) + " live " + N(sLive - aLive));
}
// LanDirectAsk: the dialog's answer and host text, the config, the DNS op's outcome and address
static void AskCase(bool inOrig, int inDialog, const char* inHost, int inConfig, bool inDns, const char* inAnswer)
{
    static LobbyConfig sTheConfig;
    sDoDialogResult = inDialog;
    sDialogHost = inHost;
    sTheConfig.mLobbyPort = (unsigned short)inConfig;
    sConfig = inConfig != 0 ? &sTheConfig : NULL;
    sDnsSucceeds = inDns;
    sDnsAnswer = inAnswer;
    int aLive = sLive;
    EeWonIpAddr* anOut = ee_won_ipaddr_new();
    Begin();
    intptr_t aResult = inOrig ? Orig(Op::LanDirectAsk, anOut, NULL, 0, 0) : New(Op::LanDirectAsk, anOut, NULL, 0, 0);
    Log("= " + N(aResult) + " live " + N(sLive - aLive));
    LogAddress(anOut);
    ee_won_ipaddr_free(anOut);
}

static void AllCases()
{
    std::vector<std::string> aTrace;
    for (int aCount = 0; aCount <= 3; aCount++) {
        int aShapes = 1;
        for (int i = 0; i < aCount; i++) aShapes *= 3;
        for (int aShape = 0; aShape < aShapes; aShape++) {
            ResetCase(true, aCount, aShape);
            aTrace = sTrace;
            ResetCase(false, aCount, aShape);
            Compare(aTrace, "reset count " + N(aCount) + " shape " + N(aShape));
        }
    }
    for (int anOutcome = 0; anOutcome < 3; anOutcome++)
        for (int aHad = 0; aHad < 2; aHad++) {
            ReadCase(true, anOutcome, aHad != 0);
            aTrace = sTrace;
            ReadCase(false, anOutcome, aHad != 0);
            Compare(aTrace, "read outcome " + N(anOutcome) + " had " + N(aHad));
        }
    for (int aCode = 0; aCode < 15; aCode++)
        for (int aMode = 0; aMode < 3; aMode++)
            for (int aKill = 0; aKill < 2; aKill++) {
                // the new side first: it gives the rows the original's host was called with (lobbyhost.rs's
                // lobby_text; the mapping itself is lobbyops.rs's unit test's)
                WaitCase(false, aCode, aMode, aKill != 0);
                std::vector<std::string> aNew = sTrace;
                WaitCase(true, aCode, aMode, aKill != 0);
                aTrace = sTrace;
                sTrace = aNew;
                Compare(aTrace, "wait code " + N(aCode) + " mode " + N(aMode) + " kill " + N(aKill));
            }
    static const char* const kHosts[] = {"1.2.3.4", "1.2.3.4:999", "10.0.0.7:65535", "some.host", "some.host:81", "", "300.1.1.1", "1.2.3"};
    static const char* const kAnswers[] = {"10.1.2.3", "10.1.2.3:4242", ""};
    static const int kDialogs[] = {0x186a1, 0x186a2, 0};
    static const int kConfigs[] = {0, 2500, 65530};
    for (int aDialog : kDialogs)
        for (const char* aHost : kHosts)
            for (int aConfig : kConfigs)
                for (int aDns = 0; aDns < 2; aDns++)
                    for (const char* anAnswer : kAnswers) {
                        AskCase(true, aDialog, aHost, aConfig, aDns != 0, anAnswer);
                        aTrace = sTrace;
                        AskCase(false, aDialog, aHost, aConfig, aDns != 0, anAnswer);
                        Compare(aTrace, std::string("ask dialog ") + N(aDialog) + " host '" + aHost + "' config " + N(aConfig) + " dns " + N(aDns) + " answer '" + anAnswer + "'");
                    }
}

int main()
{
    for (int i = 0; i < 1024; i++) gLobbyResources[i].mString.s = "res" + N(i);
    sReport = true;
    AllCases();
    int aCases = sCases, aFailures = sMismatches;
    printf("won_host36_test: %d cases, %d mismatches, %d objects and %d iterators left\n", aCases, aFailures, sLive, sIterators);
    aFailures += sLive + sIterators;
    // every new op skipped in turn
    static const Op kNew[] = {Op::GameClientsIterNew, Op::ClientsIterHasMore, Op::ClientsIterNext, Op::ClientsIterFree, Op::PlayerSetReady,
                              Op::PlayerCreateHeld, Op::PlayerReadFrom, Op::ClientSetPlayer, Op::RunOpWithStatusMode, Op::OpIsKilled, Op::LanDirectReset, Op::LanDirectDo, Op::ConfigLobbyPortTest,
                              Op::LobbyPort, Op::LanDirectHostTake, Op::LanDirectDnsHeld, Op::DnsOpAddrAssign};
    int aCaught = 0, aSkips = (int)(sizeof(kNew) / sizeof(kNew[0]));
    sReport = false;
    for (int i = 0; i < aSkips; i++) {
        sSkip = (int)kNew[i];
        sSkipHit = false;
        sCases = sMismatches = 0;
        sIterators = 0;
        AllCases();
        if (sSkipHit && sMismatches != 0) aCaught++;
        else printf("NOT CAUGHT: op %d skipped (%s)\n", sSkip, sSkipHit ? "no case differs" : "no case calls it");
    }
    printf("won_host36_test: %d of %d skips caught\n", aCaught, aSkips);
    return aFailures + (aSkips - aCaught);
}
