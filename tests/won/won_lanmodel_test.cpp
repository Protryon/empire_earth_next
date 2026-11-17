// The game's LanLogic holding the shell's members (R6 WONLobby round 11): ee_won's LanLogic made by
// ee_won_lan_new_model (its client list, LAN and direct-connect game lists, this player's client, the pending
// sockets, the listen, host and discovery sockets, the timers, the discovery address and the join password held in
// Rust, lanown.rs) against the golden's LanLogic (ee_won_lan_new, whose every op a host answers; won_lanobj_test
// proves it the original LanLogic.cpp) over a host that answers the ops on those members with LanLogicHost.cpp's
// cases as they were before the move (`RefLan` below: their statements, the model's calls made on the same model
// objects, every C++ call left on a socket, timer, event or the lobby the host op that now makes it).
//
// The model's objects (clients, games, the lists) are real modelobj.rs objects over stand-in shells; the sockets,
// timers and ops are counted stand-ins in an arena (so a set of them orders the same way in both runs). Each of
// 20,000 sequences makes both worlds and eight random inward calls; the traces (every host op the C++ still makes,
// its arguments and answer, the messages' bytes, every object's deletion) and a dump after each call (the lists'
// members, this player's client, every live object's reference count) must be the same, and both worlds must free
// everything at the end.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rust/won_lobby/ffi_lan.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_net/ipobj.h"

typedef EeWonLobbyOp Op;
typedef EeWonModelField MF;
typedef EeWonModelCall MC;
typedef EeWonModelKind MK;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sAnswerSeed, sCallSeed, sGameTypeBase;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sAnswerSeed) % n; }
static unsigned C(unsigned n) { return Rand(sCallSeed) % n; }
static std::vector<std::string> sTrace;
static void* P(uintptr_t inValue) { return reinterpret_cast<void*>(inValue); }
static bool sEcho;
static void Line(const std::string& inLine) { sTrace.push_back(inLine); if (sEcho) { fprintf(stderr, "%s\n", inLine.c_str()); fflush(stderr); } }

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::string Hex(const void* inBytes, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inBytes)[i]);
        anOut += aBuffer;
    }
    return anOut;
}
static const uint8_t* U8(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }
static std::string N(long long v) { return std::to_string(v); }

// ---- the model's objects over stand-in shells (the model at the shell's third word)

struct Ref { intptr_t mVt; int32_t mRc; int32_t mHandle; EeWonModel* mModel; int mKind; std::string mText; };
static const int kText = 100;
static std::map<int, Ref*> sRefs;   // the live ones by handle
static std::map<const void*, int> sRefOf;
static int sNextRef;
static Ref* sEmpty;
static intptr_t ModelHost(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c);
static Ref* NewRef(int inKind, const std::string& inText = std::string())
{
    Ref* r = new Ref;
    r->mVt = 0; r->mRc = 0; r->mKind = inKind; r->mText = inText; r->mModel = NULL; r->mHandle = sNextRef++;
    sRefs[r->mHandle] = r;
    sRefOf[r] = r->mHandle;
    if (inKind != kText) r->mModel = ee_won_model_new(r, (MK)inKind, ModelHost, sEmpty);
    return r;
}
static void RefAdd(Ref* r) { if (r) r->mRc++; }
static void RefRel(Ref* r)
{
    if (r == NULL || --r->mRc > 0) return;
    Line("delete h" + N(r->mHandle));
    sRefs.erase(r->mHandle);
    sRefOf.erase(r);
    if (r->mModel != NULL) ee_won_model_free(r->mModel);
    delete r;
}
static Ref* RR(intptr_t p) { return reinterpret_cast<Ref*>(p); }
static void Set(Ref* r, MF f, intptr_t v);
static intptr_t ModelHost(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c)
{
    (void)b; (void)c;
    switch (op) {
    case EeWonModelOp::AddRef: RefAdd(RR(a)); return 0;
    case EeWonModelOp::Release: RefRel(RR(a)); return 0;
    case EeWonModelOp::NewGame: { Ref* g = NewRef((int)MK::Game); Set(g, MF::GameType, a); RefAdd(g); return (intptr_t)g; }
    case EeWonModelOp::NewClient: { Ref* x = NewRef((int)MK::Client); RefAdd(x); return (intptr_t)x; }
    default: Line("model op " + N((int)op)); return 0;
    }
}
static intptr_t Call(Ref* r, MC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0)
{
    return ee_won_model_call(r->mModel, c, a, b, cc, (intptr_t)sEmpty);
}
static intptr_t Get(Ref* r, MF f) { return ee_won_model_get(r->mModel, f); }
static void Set(Ref* r, MF f, intptr_t v) { ee_won_model_set(r->mModel, f, v); }
static Ref* Text(const std::string& inText) { return NewRef(kText, inText); }
static void SetText(Ref* r, MF f, const std::string& inText)
{
    Ref* t = Text(inText);
    RefAdd(t);
    Set(r, f, (intptr_t)t);
    RefRel(t);
}
static std::string TextOf(intptr_t inData) { return inData != 0 ? RR(inData)->mText : std::string("?"); }

// ---- the sockets, timers and ops: counted stand-ins in one arena (their addresses ordered as made)

struct Obj { int mRc; int mHandle; bool mLive; void* mClient; }; // a socket's client (a reference)
static const int kObjs = 4096;
static Obj sObjs[kObjs];
static int sNextObj;
struct Ref;
static void SetClientOf(struct Obj* o, void* inClient);
static Obj* NewObj()
{
    Obj* o = &sObjs[sNextObj % kObjs];
    o->mRc = 0; o->mHandle = sNextObj++; o->mLive = true; o->mClient = NULL;
    return o;
}
static bool IsObj(const void* p) { return p >= (const void*)&sObjs[0] && p < (const void*)&sObjs[kObjs]; }
static Obj* OO(void* p) { return static_cast<Obj*>(p); }
static void ObjAdd(Obj* o) { if (o) o->mRc++; }
static void ObjRel(Obj* o)
{
    if (o == NULL || --o->mRc > 0) return;
    Line("delete o" + N(o->mHandle));
    o->mLive = false;
    SetClientOf(o, NULL);
}
static Obj* ALiveObj()
{
    std::vector<Obj*> aLive;
    for (int i = 0; i < sNextObj && i < kObjs; i++) if (sObjs[i].mLive) aLive.push_back(&sObjs[i]);
    return aLive.empty() ? NULL : aLive[C((unsigned)aLive.size())];
}
static Obj* AnsObj()
{
    std::vector<Obj*> aLive;
    for (int i = 0; i < sNextObj && i < kObjs; i++) if (sObjs[i].mLive) aLive.push_back(&sObjs[i]);
    unsigned r = R((unsigned)aLive.size() + 2);
    return r < aLive.size() ? aLive[r] : NULL;
}

static void SetClientOf(Obj* o, void* inClient)
{
    if (o->mClient == inClient) return;
    void* anOld = o->mClient;
    o->mClient = inClient;
    RefAdd(RR((intptr_t)inClient));
    RefRel(RR((intptr_t)anOld));
}

// a pointer as the trace prints it: a stand-in by its handle, else its value
static std::string PS(const void* p)
{
    if (p == NULL) return "0";
    if (IsObj(p)) return "o" + N(OO(const_cast<void*>(p))->mHandle);
    std::map<const void*, int>::iterator it = sRefOf.find(p);
    if (it != sRefOf.end()) return "h" + N(it->second);
    char aLine[24];
    snprintf(aLine, sizeof(aLine), "%llx", (unsigned long long)(uintptr_t)p);
    return aLine;
}
static std::string AddrText(const EeWonIpAddr* a)
{
    return a == NULL ? "-" : N((long)ee_won_ipaddr_host(a)) + ":" + N(ee_won_ipaddr_port(a));
}
static void SetAddr(EeWonIpAddr* a, unsigned inHost, unsigned inPort) { ee_won_ipaddr_set_host_order(a, 0x0a000000u + inHost, (uint16_t)inPort); }

// ---- the worlds' fixed objects: three clients and two games the calls and answers name

static std::vector<Ref*> sPoolClients, sPoolGames;
static void* AClient() { unsigned r = C(6); return r == 0 ? NULL : sPoolClients[r % sPoolClients.size()]; }
static void* AGame() { return sPoolGames[C((unsigned)sPoolGames.size())]; }
static void* AnsClient()
{
    std::vector<Ref*> aLive;
    for (std::map<int, Ref*>::iterator it = sRefs.begin(); it != sRefs.end(); ++it)
        if (it->second->mKind == (int)MK::Client) aLive.push_back(it->second);
    unsigned r = R((unsigned)aLive.size() + 2);
    return r < aLive.size() ? aLive[r] : NULL;
}
static void* AnsGame() { unsigned r = R(5); return r < 2 ? sPoolGames[r] : r == 2 ? NULL : sPoolGames[r & 1]; }

// ---- the generated messages (as won_lanobj_test's)

static std::string TextUnits()
{
    std::string s;
    unsigned n = R(5);
    for (unsigned i = 0; i < n; i++) {
        uint16_t u = R(6) == 0 ? (uint16_t)0x263a : (uint16_t)('a' + R(26));
        s += (char)(u & 0xff);
        s += (char)(u >> 8);
    }
    return s;
}
static uint16_t Id() { return (uint16_t)(1 + R(3)); }
static std::string GameMessage()
{
    std::string m;
    switch (R(9)) {
    case 0: { std::string a = TextUnits(), b = TextUnits(); ee_won_lan_join_request(U8(a), a.size(), U8(b), b.size(), Append, &m); break; }
    case 1: { std::string a = TextUnits(); ee_won_lan_client_joined(Id(), U8(a), a.size(), Append, &m); break; }
    case 2: ee_won_lan_client_left(Id(), Append, &m); break;
    case 3: { std::string a = TextUnits(); ee_won_lan_chat_request((uint16_t)(R(2) ? 4 : R(2) ? 0x100 : 1), U8(a), a.size(), Id(), Append, &m); break; }
    case 4: { std::string a = TextUnits(); ee_won_lan_chat_line((uint16_t)(R(2) ? 4 : R(2) ? 0x100 : 1), Id(), U8(a), a.size(), Id(), Append, &m); break; }
    case 5: { std::string a = "\x05\x01"; ee_won_lan_staging(U8(a), R(3), Append, &m); break; }
    case 6: ee_won_lan_ping(Append, &m); break;
    case 7: m = std::string(1, (char)R(12)); break;
    default: break;
    }
    if (m.size() >= 2 && R(4) != 0) m = m.substr(2);
    if (R(6) == 0 && !m.empty()) m.resize(R((unsigned)m.size()));
    return m;
}
static std::string JoinReply()
{
    std::string m;
    switch (R(4)) {
    case 0: case 1: {
        unsigned n = R(4);
        std::vector<uint16_t> anIds;
        std::vector<size_t> anOffsets(1, 0);
        std::string aNames;
        for (unsigned i = 0; i < n; i++) { anIds.push_back(Id()); aNames += TextUnits(); anOffsets.push_back(aNames.size()); }
        ee_won_lan_join_accepted(Id(), Id(), anIds.size(), anIds.empty() ? NULL : &anIds[0], U8(aNames), &anOffsets[0],
                                 Append, &m);
        break;
    }
    case 2: ee_won_lan_join_refused((int16_t)(2 + R(4)), Append, &m); break;
    default: m = std::string(1, (char)R(4)); break;
    }
    if (m.size() >= 2 && R(8) != 0) m = m.substr(2);
    if (R(6) == 0 && !m.empty()) m.resize(R((unsigned)m.size()));
    return m;
}
static std::string DiscoveryPacket()
{
    std::string m;
    ee_won_lan_discovery((uint8_t)(1 + R(5)), (uint16_t)(R(3) * 1000), Append, &m);
    if (R(3) == 0) m += "summary";
    if (R(6) == 0) m.resize(R((unsigned)m.size() + 1));
    return m;
}

// ---- the host: every op the C++ makes, traced and answered (the same in both worlds)

static bool IsTake(Op o)
{
    return o == Op::LanRecvMsgTake || o == Op::LanDiscoveryTake || o == Op::LanRecvReplyTake || o == Op::ClientNameTake ||
           o == Op::ParserTextTake || o == Op::LanNameTake || o == Op::LanJoinPasswordTake || o == Op::LanSummaryTake ||
           o == Op::LanPasswordTake || o == Op::LanRecvReply;
}
static bool IsMessage(Op o)
{
    return o == Op::LanSocketSend || o == Op::LanClientSocketSend || o == Op::LanStagingRead || o == Op::LanSendTo;
}
static bool IsText(Op o) { return o == Op::LanNewClient || o == Op::LanGamePasswordIs || o == Op::LanRecvChat; }
static intptr_t Bool(unsigned inTrueIn8) { return R(8) < inTrueIn8 ? 1 : 0; }

static intptr_t Answer(Op op, void* a, void* b, intptr_t c, intptr_t d, std::string& outTaken)
{
    switch (op) {
    case Op::LanSocketClient: return IsObj(a) ? (intptr_t)OO(a)->mClient : 0;
    case Op::LanSocketSetClient: if (IsObj(a)) SetClientOf(OO(a), b); return 0;
    case Op::EventClientActionClient: case Op::ParserTarget: return (intptr_t)AnsClient();
    case Op::LanNewClient: {
        std::string aName(reinterpret_cast<const char*>(c), (size_t)(d & 0xffffffff) * 2);
        Ref* aClient = NewRef((int)MK::Client);
        SetText(aClient, MF::ClientName, aName);
        RefAdd(aClient);
        return (intptr_t)aClient;
    }
    case Op::ClientHold: RefAdd(RR((intptr_t)a)); return (intptr_t)a;
    case Op::ClientRelease: RefRel(RR((intptr_t)a)); return 0;
    case Op::LanOpSocket: case Op::LanClientSocket: return (intptr_t)AnsObj();
    case Op::LanAcceptedSocket: { if (R(4) == 0) return 0; Obj* o = NewObj(); if (R(2)) SetClientOf(o, AnsClient()); return (intptr_t)o; }
    case Op::LanNewTimer: case Op::LanNewSocket: { Obj* o = NewObj(); o->mRc = 1; return (intptr_t)o; }
    case Op::LanNewDiscovery: {
        Obj* o = NewObj();
        o->mRc = 1;
        SetAddr(static_cast<EeWonIpAddr*>(a), 0xffffff, 2300 + R(3));
        *static_cast<intptr_t*>(b) = R(4) == 0 ? 1 : 0;
        return (intptr_t)o;
    }
    case Op::LanDirectAddressAt: SetAddr(static_cast<EeWonIpAddr*>(a), 1 + (unsigned)c % 3, 2300 + R(2)); return 0;
    case Op::LanDirectAsk: if (R(4) == 0) return 0; SetAddr(static_cast<EeWonIpAddr*>(a), 1 + R(3), 2300 + R(2)); return 1;
    case Op::LanRecvFromAddr: SetAddr(static_cast<EeWonIpAddr*>(b), 1 + R(3), R(2) * 1000); return 0;
    case Op::LanDirectNameAt: // the name; the server list's filter (the game's) marks some games
        SetText(RR((intptr_t)a), MF::GameName, "direct" + N(c));
        Set(RR((intptr_t)a), MF::GameFiltered, R(3) == 0);
        return 0;
    case Op::LanDirectSave: SetText(RR((intptr_t)a), MF::GameName, "named" + N(R(3))); return 0;
    case Op::LanReadSummary: { unsigned r = R(6); return r == 0 ? (intptr_t)EeWonLobbyResult::Threw : r < 4 ? 1 : 0; }
    case Op::LanDirectCount: return R(3);
    case Op::LanMyGame: case Op::EventDirectGame: return (intptr_t)AnsGame();
    case Op::ChatParser: return R(8) < 2 ? 0 : 0x7001;
    case Op::EventComponent: return 0x4100;
    case Op::LanSocketLastTime: return 1000 - (intptr_t)R(600);
    case Op::Now: return 1000 + R(40);
    case Op::LanSubnets: return R(2) ? 0x01020102 : 0x01020304;
    case Op::GameScreenType: return R(5);
    case Op::LanNameDialogRun: case Op::LanPortDialogRun: case Op::LanPasswordDialog: return R(8) < 6 ? 0x186a1 : 0x186a2;
    case Op::LanNameDialogLength: return R(3);
    case Op::LanBroadcastPort: case Op::LanPortDialogPort: return 2300 + R(3);
    case Op::ParserRoomFlags: return R(32);
    case Op::ParserCommand: return R(12);
    case Op::EventClientActionRoomSpec: return R(2) ? 8 : R(16);
    case Op::EventClientActionAction: return R(20);
    case Op::LanRecvMsgTake: { intptr_t r = R(4) == 0 ? 0 : 0x3201 + R(2); if (r != 0) outTaken = GameMessage(); return r; }
    case Op::LanDiscoveryTake: outTaken = DiscoveryPacket(); return 0;
    case Op::LanRecvReply: { unsigned r = R(10); if (r == 0) return -1; if (r == 1) return 0; outTaken = JoinReply(); return 1; }
    case Op::LanSummaryTake: outTaken = "sum" + std::string(R(3), 'x'); return 0;
    case Op::ClientNameTake: case Op::ParserTextTake: case Op::LanNameTake: case Op::LanPasswordTake:
        outTaken = TextUnits(); return 0;
    case Op::LanListen: case Op::LanOpOnSocket: case Op::LanConnectTo: case Op::LanRecvOk: case Op::GamePingReplied:
    case Op::DoCDKeyCheck: case Op::LanRefreshCertificate: case Op::LanGamePasswordIs: case Op::EventDirectIsDelete:
    case Op::EventDirectIsAdd: return Bool(6);
    default: return 0;
    }
}

static std::string* sCppSink;
static intptr_t Host(void* inLogic, Op op, void* a, void* b, intptr_t c, intptr_t d)
{
    (void)inLogic;
    std::string aTaken;
    std::string aHead = N((int)op) + " " + PS(a) + " ";
    bool aOutA = op == Op::LanNewDiscovery || op == Op::LanDirectAddressAt || op == Op::LanDirectAsk;
    if (aOutA) aHead = N((int)op) + " @ ";
    std::string aArgs;
    if (IsMessage(op))
        aArgs = (op == Op::LanSendTo ? AddrText(static_cast<const EeWonIpAddr*>(b)) : PS(b)) + " m:" +
                (c != 0 ? Hex(P(c), (size_t)d) : std::string("-"));
    else if (IsText(op))
        aArgs = PS(b) + " u:" + Hex(P(c), (size_t)(d & 0xffffffff) * 2) + " " + N(d >> 32);
    else if (IsTake(op))
        aArgs = "s " + N(c) + " " + N(d);
    else if (op == Op::LanDirectSave)
        aArgs = AddrText(static_cast<const EeWonIpAddr*>(b));
    else if (op == Op::LanRecvFromAddr || op == Op::LanNewDiscovery)
        aArgs = "@";
    else
        aArgs = PS(b) + " " + N(c) + " " + N(d);
    if (op == Op::RefHold) { ObjAdd(OO(a)); return 0; }
    if (op == Op::RefRelease) { ObjRel(OO(a)); return 0; }
    intptr_t aResult = Answer(op, a, b, c, d, aTaken);
    if (IsTake(op)) {
        if (b == sCppSink) sCppSink->append(aTaken); // a take the golden's world makes (a C++ string)
        else ee_won_lobby_sink(b, U8(aTaken), aTaken.size());
    }
    std::string aOut = aHead + aArgs + " -> " + (op == Op::LanNewClient || op == Op::LanSocketClient ||
                       op == Op::ClientHold || op == Op::LanOpSocket || op == Op::LanClientSocket ||
                       op == Op::LanAcceptedSocket || op == Op::LanNewTimer || op == Op::LanNewSocket ||
                       op == Op::LanNewDiscovery || op == Op::LanMyGame || op == Op::EventDirectGame ||
                       op == Op::EventClientActionClient || op == Op::ParserTarget ? PS(P(aResult)) : N(aResult));
    if (IsTake(op)) aOut += " t:" + Hex(aTaken.data(), aTaken.size());
    if (aOutA || op == Op::LanRecvFromAddr) aOut += " @" + AddrText(static_cast<const EeWonIpAddr*>(op == Op::LanRecvFromAddr ? b : a));
    Line(aOut);
    return aResult;
}
static intptr_t T(Op op, void* a = NULL, void* b = NULL, intptr_t c = 0, intptr_t d = 0) { return Host(P(0x52), op, a, b, c, d); }

// LobbyLogicHost.cpp's kTexts as it was (the title's and the details' resource rows by text code)
typedef EeWonLobbyRes LR;
static const LR kTexts[][2] = {
    {LR::RoutingLogic_GameDissolvedTitle_String, LR::RoutingLogic_GameDissolvedDetails_String},
    {LR::RoutingLogic_AlreadyJoinedTitle_String, LR::RoutingLogic_AlreadyInGame_String},
    {LR::GameStagingLogic_JoinGameErrorTitle_String, LR::GameStagingLogic_NoPingReplyError_String},
    {LR::StagingLogic_DisconnectedTitle_String, LR::StagingLogic_DisconnectedDetails_String},
    {LR::StagingLogic_BannedTitle_String, LR::StagingLogic_BannedDetails_String},
    {LR::StagingLogic_KickedTitle_String, LR::StagingLogic_KickedDetails_String},
    {LR::GameStagingLogic_ConnectWarningTitle1_String, LR::GameStagingLogic_ConnectWarningDetails1_String},
    {LR::GameStagingLogic_ConnectWarningTitle2_String, LR::GameStagingLogic_ConnectWarningDetails2_String},
    {LR::GameStagingLogic_ConnectWarningTitle3_String, LR::GameStagingLogic_ConnectWarningDetails3_String},
    {LR::GameStagingLogic_DissolveGame_String, LR::GameStagingLogic_DissolveGameConfirm_String},
    {LR::GameStagingLogic_LeaveGame_String, LR::GameStagingLogic_LeaveGameConfirm_String},
    {LR::GameStagingLogic_VerifyingPingTitle_String, LR::GameStagingLogic_VerifyingPingDetails_String},
    {LR::GameStagingLogic_GettingGameInfoTitle_String, LR::GameStagingLogic_GettingGameInfoDetails_String},
    {LR::GameStagingLogic_JoinGameErrorTitle_String, LR::LobbyGameStatus_InvalidPassword_String},
    {LR::LANLogic_RegisterTitle_String, LR::LANLogic_InvalidSubnet_String},
};
static intptr_t TextRows(intptr_t inCode)
{
    const LR* aText = inCode >= 0 && inCode < (intptr_t)(sizeof(kTexts) / sizeof(kTexts[0])) ? kTexts[inCode] : kTexts[0];
    return (intptr_t)aText[0] | (intptr_t)aText[1] << 16;
}

// ---- the golden's world: LanLogicHost.cpp's cases on the shell's members, as they were

struct RefLan {
    Ref* mLanGames;
    Ref* mDirectGames;
    std::set<Obj*> mPending;
    Obj* mListen;
    Obj* mServer;
    Obj* mNetworkTimer;
    Obj* mDiscovery;
    Obj* mDiscoveryTimer;
    EeWonIpAddr* mDiscoveryAddress;
    Ref* mClients;
    Ref* mMyClient;
    std::string mJoinPassword; // UTF-16 LE
    EeWonIpAddr* mRecvFrom;    // the original's reference into the op (a copy here)

    RefLan(Ref* inClients, Ref* inLan, Ref* inDirect)
        : mLanGames(inLan), mDirectGames(inDirect), mListen(NULL), mServer(NULL), mNetworkTimer(NULL), mDiscovery(NULL),
          mDiscoveryTimer(NULL), mDiscoveryAddress(ee_won_ipaddr_new()), mClients(inClients), mMyClient(NULL),
          mRecvFrom(ee_won_ipaddr_new())
    {
        RefAdd(mClients); RefAdd(mLanGames); RefAdd(mDirectGames);
    }
    void Free()
    {
        SetRef(mMyClient, NULL);
        RefRel(mClients);
        SetObj(mDiscoveryTimer, NULL); SetObj(mDiscovery, NULL); SetObj(mNetworkTimer, NULL); SetObj(mServer, NULL);
        SetObj(mListen, NULL);
        for (std::set<Obj*>::iterator it = mPending.begin(); it != mPending.end(); ++it) ObjRel(*it);
        mPending.clear();
        RefRel(mDirectGames);
        RefRel(mLanGames);
        ee_won_ipaddr_free(mDiscoveryAddress);
        ee_won_ipaddr_free(mRecvFrom);
    }
    // SmartPtr<T>::operator=
    static void SetRef(Ref*& ioSlot, Ref* inValue) { if (ioSlot != inValue) { RefRel(ioSlot); RefAdd(inValue); ioSlot = inValue; } }
    static void SetObj(Obj*& ioSlot, Obj* inValue) { if (ioSlot != inValue) { ObjRel(ioSlot); ObjAdd(inValue); ioSlot = inValue; } }
    // a SmartPtr given a new object (the op's reference is the SmartPtr's)
    static void PutNew(Obj*& ioSlot, Obj* inNew) { Obj* anOld = ioSlot; ioSlot = inNew; ObjRel(anOld); }

    void StartTimer(Obj*& ioTimer, int inWhich, intptr_t inMs)
    {
        if (ioTimer == NULL) PutNew(ioTimer, OO(P((uintptr_t)T(Op::LanNewTimer, NULL, NULL, inWhich))));
        T(Op::LanRunTimer, ioTimer, NULL, inMs);
    }
    void AddPendingSocket(Obj* inSocket)
    {
        T(Op::LanArmSocket, inSocket);
        T(Op::LanSocketStamp, inSocket);
        if (mPending.insert(inSocket).second) ObjAdd(inSocket);
    }
    void StopDiscovery()
    {
        if (!Call(mLanGames, MC::GameListIsEmpty)) {
            Call(mLanGames, MC::GameListClear);
            T(Op::LanSyncGameList, mLanGames);
        }
        if (mDiscoveryTimer != NULL) { T(Op::LanTimerKill, mDiscoveryTimer); SetObj(mDiscoveryTimer, NULL); }
        if (mDiscovery != NULL) { T(Op::LanSocketKill, mDiscovery); SetObj(mDiscovery, NULL); }
    }
    void RecvAddress(void* inOp, EeWonIpAddr* outAddress) { T(Op::LanRecvFromAddr, inOp, outAddress); }
    void HandleGameSummary(void* inOp, unsigned short inPort)
    {
        EeWonIpAddr* anAddress = ee_won_ipaddr_new();
        RecvAddress(inOp, anAddress);
        ee_won_ipaddr_set_port(anAddress, inPort);
        bool aFlag = false;
        Ref* aGame = RR(Call(mLanGames, MC::GameListGetOrCreateAddr, (intptr_t)anAddress, (intptr_t)&aFlag));
        intptr_t aRead = T(Op::LanReadSummary, inOp, aGame);
        if (aRead != (intptr_t)EeWonLobbyResult::Threw) {
            EeWonLanSummary anOutcome = ee_won_lanlogic_summary(aRead != 0, aFlag);
            RefAdd(aGame);
            if (anOutcome.keep) Set(aGame, MF::GameLastSeen, T(Op::Now));
            else RefRel(RR(Call(mLanGames, MC::GameListRemoveAddr, (intptr_t)anAddress)));
            if (anOutcome.event >= 0) T(Op::LanGameChanged, aGame, NULL, anOutcome.event);
            RefRel(aGame);
        }
        ee_won_ipaddr_free(anAddress);
    }
    void HandleGameKeepAlive(void* inOp, unsigned short inPort, bool inRemoved)
    {
        EeWonIpAddr* aFrom = ee_won_ipaddr_new();
        RecvAddress(inOp, aFrom);
        EeWonIpAddr* anAddress = ee_won_ipaddr_clone(aFrom);
        ee_won_ipaddr_set_port(anAddress, inPort);
        if (ee_won_ipaddr_host(anAddress) == ee_won_ipaddr_host(aFrom)) {
            if (inRemoved) {
                Ref* aGame = RR(Call(mLanGames, MC::GameListRemoveAddr, (intptr_t)anAddress));
                if (aGame != NULL) T(Op::LanGameChanged, aGame, NULL, 1);
                RefRel(aGame);
            } else {
                Ref* aGame = RR(Call(mLanGames, MC::GameListGetAddr, (intptr_t)anAddress));
                if (aGame != NULL) Set(aGame, MF::GameLastSeen, T(Op::Now));
            }
        }
        ee_won_ipaddr_free(anAddress);
        ee_won_ipaddr_free(aFrom);
    }
    intptr_t MyId() { return mMyClient != NULL ? Get(mMyClient, MF::ClientId) : 0xffff; } // (the original: NULL->)

    // the case of one op, or false when the op is the host's
    bool Op_(Op op, void* a, void* b, intptr_t c, intptr_t d, intptr_t& outResult)
    {
        Ref* ar = RR((intptr_t)a);
        outResult = 0;
        switch (op) {
        case Op::LanMyClient: outResult = (intptr_t)mMyClient; return true;
        case Op::LanSetMyClient: SetRef(mMyClient, ar); return true;
        case Op::LanListenSocket: outResult = (intptr_t)mListen; return true;
        case Op::LanServerSocket: outResult = (intptr_t)mServer; return true;
        case Op::LanNetworkTimer: outResult = (intptr_t)mNetworkTimer; return true;
        case Op::LanDiscoveryTimer: outResult = (intptr_t)mDiscoveryTimer; return true;
        case Op::LanDiscoverySocket: outResult = (intptr_t)mDiscovery; return true;
        case Op::LanDiscoveryAddress: outResult = (intptr_t)mDiscoveryAddress; return true;
        case Op::LanCloseSockets:
            if (mListen != NULL) { T(Op::LanSocketKill, mListen); SetObj(mListen, NULL); }
            if (mServer != NULL) { T(Op::LanSocketClose, mServer, NULL, -1); SetObj(mServer, NULL); }
            if (mNetworkTimer != NULL) { T(Op::LanTimerKill, mNetworkTimer); SetObj(mNetworkTimer, NULL); }
            for (std::set<Obj*>::iterator it = mPending.begin(); it != mPending.end(); ++it)
                T(Op::LanSocketClose, *it, NULL, -1);
            SetRef(mMyClient, NULL);
            for (std::set<Obj*>::iterator it = mPending.begin(); it != mPending.end(); ++it) ObjRel(*it);
            mPending.clear();
            Call(mClients, MC::ListClear);
            return true;
        case Op::LanForgetSocket: if (mPending.erase(OO(a)) != 0) ObjRel(OO(a)); return true;
        case Op::LanLoseHost:
            T(Op::LanSocketKill, mServer);
            SetObj(mServer, NULL);
            T(Op::LanGameDisconnected);
            return true;
        case Op::LanPendingCount: outResult = (intptr_t)mPending.size(); return true;
        case Op::LanPendingAt: {
            std::set<Obj*>::iterator it = mPending.begin();
            for (intptr_t i = 0; i < c && it != mPending.end(); ++i) ++it;
            outResult = it != mPending.end() ? (intptr_t)*it : 0;
            return true;
        }
        case Op::LanStartNetworkTimer: StartTimer(mNetworkTimer, 0, 120000); return true;
        case Op::LanStartDiscoveryTimer: StartTimer(mDiscoveryTimer, 1, 15000); return true;
        case Op::LanAddClient: Set(ar, MF::ClientId, (uint16_t)c); Call(mClients, MC::ListAdd, (intptr_t)ar); return true;
        case Op::LanClientCount: outResult = Call(mClients, MC::ListCount); return true;
        case Op::LanClientAt: outResult = Call(mClients, MC::ListAt, c); return true;
        case Op::LanFindClient: outResult = Call(mClients, MC::ListFind, (uint16_t)c); return true;
        case Op::LanRemoveClient: outResult = Call(mClients, MC::ListRemove, (uint16_t)c); return true;
        case Op::LanServerClientIsCaptain:
            T(Op::LanSocketSetClient, mServer, a);
            Call(ar, MC::ClientSetCaptain, 1, 1);
            return true;
        case Op::LanAddPending: AddPendingSocket(OO(a)); return true;
        case Op::LanServerSend: outResult = T(Op::LanSocketSend, a, mServer, c, d); return true;
        case Op::LanClearLanGames: Call(mLanGames, MC::GameListClear); T(Op::LanSyncGameList, mLanGames); return true;
        case Op::LanPingDirectGames:
            Call(mDirectGames, MC::GameListRewind);
            while (Call(mDirectGames, MC::GameListHasMore)) {
                Ref* aGame = RR(Call(mDirectGames, MC::GameListNext));
                if (!Get(aGame, MF::GameFiltered)) T(Op::LanPingGame, aGame);
            }
            T(Op::BroadcastLobbyEvent, NULL, NULL, 0x186da);
            return true;
        case Op::LanEnterScreenLists:
            T(Op::LanNoCurrentUser);
            Call(mLanGames, MC::GameListClear);
            Call(mDirectGames, MC::GameListClear);
            for (intptr_t i = 0; i < T(Op::LanDirectCount); ++i) {
                EeWonIpAddr* anAddress = ee_won_ipaddr_new();
                T(Op::LanDirectAddressAt, anAddress, NULL, i);
                Ref* aGame = RR(Call(mDirectGames, MC::GameListGetOrCreateAddr, (intptr_t)anAddress, 0));
                T(Op::LanDirectNameAt, aGame, NULL, i);
                ee_won_ipaddr_free(anAddress);
            }
            T(Op::LanSyncGameList, mDirectGames);
            return true;
        case Op::LanHostMyClient: {
            std::string aName;
            sCppSink = &aName;
            T(Op::LanNameTake, NULL, &aName);
            Ref* aClient = RR(T(Op::LanNewClient, NULL, NULL, (intptr_t)aName.data(), (intptr_t)(aName.size() / 2)));
            SetRef(mMyClient, aClient);
            T(Op::ClientRelease, aClient);
            Set(mMyClient, MF::ClientId, (uint16_t)c);
            Call(mMyClient, MC::ClientSetCaptain, 1, 1);
            Call(mClients, MC::ListAdd, (intptr_t)mMyClient);
            Set(ar, MF::GameClients, (intptr_t)mClients);
            return true;
        }
        case Op::LanHostListen: {
            PutNew(mListen, OO(P((uintptr_t)T(Op::LanNewSocket))));
            if (!T(Op::LanListen, mListen, a)) return true;
            T(Op::LanJoinedRoom, a, NULL, MyId());
            outResult = 1;
            return true;
        }
        case Op::LanSendSummary: {
            std::string aBytes(reinterpret_cast<const char*>(c), (size_t)d);
            std::string aSummary;
            sCppSink = &aSummary;
            T(Op::LanSummaryTake, a, &aSummary);
            aBytes += aSummary;
            outResult = T(Op::LanSendTo, mDiscovery, b, (intptr_t)aBytes.data(), (intptr_t)aBytes.size());
            return true;
        }
        case Op::LanDiscoverySend: outResult = T(Op::LanSendTo, mDiscovery, b, c, d); return true;
        case Op::LanRemoveStaleGames: {
            intptr_t aNow = T(Op::Now);
            if (Call(mLanGames, MC::GameListRemoveStale, 30, aNow)) T(Op::LanSyncGameList, mLanGames);
            return true;
        }
        case Op::LanDiscoveryOk: outResult = T(Op::LanOpOnSocket, a, mDiscovery) != 0; return true;
        case Op::LanGameSummary: HandleGameSummary(a, (unsigned short)c); return true;
        case Op::LanGameKeepAlive: HandleGameKeepAlive(a, (unsigned short)c, false); return true;
        case Op::LanGameRemoved: HandleGameKeepAlive(a, (unsigned short)c, true); return true;
        case Op::LanRecvFrom: RecvAddress(a, mRecvFrom); outResult = (intptr_t)mRecvFrom; return true;
        case Op::LanOpenDiscovery: {
            intptr_t aStatus = 0;
            PutNew(mDiscovery, OO(P((uintptr_t)T(Op::LanNewDiscovery, mDiscoveryAddress, &aStatus))));
            outResult = aStatus;
            return true;
        }
        case Op::LanDiscoveryArm: T(Op::LanArmDiscovery, mDiscovery); return true;
        case Op::LanStopDiscovery: StopDiscovery(); return true;
        case Op::LanClearDirectGames: Call(mDirectGames, MC::GameListClear); return true;
        case Op::LanNewServerSocket: PutNew(mServer, OO(P((uintptr_t)T(Op::LanNewSocket)))); return true;
        case Op::LanJoinPasswordClear: mJoinPassword.clear(); return true;
        case Op::LanPasswordDialogRun:
            outResult = T(Op::LanPasswordDialog);
            if (outResult == 100001) { mJoinPassword.clear(); sCppSink = &mJoinPassword; T(Op::LanPasswordTake, NULL, &mJoinPassword); }
            return true;
        case Op::LanConnect: outResult = T(Op::LanConnectTo, a, mServer, c); return true;
        case Op::LanJoinPasswordTake: ee_won_lobby_sink(b, U8(mJoinPassword), mJoinPassword.size()); return true;
        case Op::LanRecvReplyTake: outResult = T(Op::LanRecvReply, mServer, b); return true;
        case Op::LanJoinArm:
            T(Op::LanArmSocket, mServer);
            StartTimer(mNetworkTimer, 0, 120000);
            Set(ar, MF::GameClients, (intptr_t)mClients);
            T(Op::LanJoinedRoom, a, NULL, MyId());
            return true;
        case Op::LanDirectRemove: {
            T(Op::LanRemoveSavedDirect, a);
            EeWonIpAddr* anAddress = ee_won_ipaddr_clone((const EeWonIpAddr*)Get(ar, MF::GameAddress));
            RefRel(RR(Call(mDirectGames, MC::GameListRemoveAddr, (intptr_t)anAddress)));
            ee_won_ipaddr_free(anAddress);
            T(Op::LanGameChanged, a, NULL, 1);
            return true;
        }
        case Op::LanDirectAdd: {
            EeWonIpAddr* anAddress = ee_won_ipaddr_new();
            if (T(Op::LanDirectAsk, anAddress)) {
                bool aFlag = false;
                Ref* aGame = RR(Call(mDirectGames, MC::GameListGetOrCreateAddr, (intptr_t)anAddress, (intptr_t)&aFlag));
                T(Op::LanDirectSave, aGame, anAddress);
                T(Op::LanGameChanged, aGame, NULL, ee_won_lanlogic_direct_event(aFlag));
            }
            ee_won_ipaddr_free(anAddress);
            return true;
        }
        case Op::LanShowClientOptions: outResult = T(Op::LanClientOptions, a, mMyClient, c); return true;
        // the texts as the game's host takes them (LobbyLogicHost.cpp's table as it was)
        case Op::MessageBox: case Op::YesNo: case Op::RunWaitOp: outResult = Host(P(0x52), op, a, b, TextRows(c), d); return true;
        // the model's values (the game's object answers them in modelobj.rs)
        case Op::ClientId: outResult = Get(ar, MF::ClientId); return true;
        case Op::GameType: outResult = Get(ar, MF::GameType); return true;
        case Op::GameHasPassword: outResult = Get(ar, MF::GameHasPassword); return true;
        case Op::GameAddressPort: outResult = ee_won_ipaddr_port((const EeWonIpAddr*)Get(ar, MF::GameAddress)); return true;
        case Op::GameSetCaptainId: Set(ar, MF::GameCaptainId, (uint16_t)c); return true;
        case Op::ClientIsIgnored: outResult = Get(ar, MF::ClientIgnored); return true;
        case Op::ClientIsTeammate: outResult = Call(ar, MC::ClientIsTeammate, (intptr_t)b); return true;
        default: return false;
        }
    }
};
static RefLan* sRefLan;
static intptr_t RefHost(void* inLogic, Op op, void* a, void* b, intptr_t c, intptr_t d)
{
    intptr_t aResult;
    if (sRefLan->Op_(op, a, b, c, d, aResult)) return aResult;
    return Host(inLogic, op, a, b, c, d);
}

// ---- the sequences

struct CallArgs { EeWonLanCall mCall; void* mA; void* mB; intptr_t mC; std::string mBytes; bool mMessage; };

static CallArgs NextCall()
{
    CallArgs c = {EeWonLanCall::Control, NULL, NULL, 0, std::string(), false};
    switch (C(26)) {
    case 0: c.mCall = EeWonLanCall::Control; c.mC = C(3) ? 3 + C(2) : C(6); break; // the refresh and the LAN screen more often
    case 1: c.mCall = EeWonLanCall::ComponentEvent; c.mA = P(0x4001);
            c.mC = C(4) == 0 ? 100001 : C(3) == 0 ? 100005 : C(2) ? 100008 : 0x186ac; break;
    case 2: case 3: c.mCall = EeWonLanCall::HostGame; c.mA = AGame(); break;
    case 4: case 5: c.mCall = EeWonLanCall::JoinGame; c.mA = AGame(); break;
    case 6: case 7: {
        unsigned k = C(3);
        c.mCall = k == 0 ? EeWonLanCall::SendToCaptain : k == 1 ? EeWonLanCall::BroadcastToGame : EeWonLanCall::SendToClient;
        if (k == 2) c.mB = AClient();
        c.mMessage = true;
        if (C(4) != 0) { c.mA = P(0x3100); c.mBytes = std::string("\x05\x01\x00\x01", C(5)); }
        break;
    }
    case 8: c.mCall = C(2) ? EeWonLanCall::AdvertiseGame : EeWonLanCall::RemoveGameAdvertisement; c.mA = AGame(); break;
    case 9: c.mCall = EeWonLanCall::KickClient; c.mB = sPoolClients[C((unsigned)sPoolClients.size())]; break;
    case 10: c.mCall = C(2) ? EeWonLanCall::LeaveGame : EeWonLanCall::Logout; break;
    case 11: case 12: c.mCall = EeWonLanCall::SocketClosed; c.mA = P(0x6101); break;
    case 13: c.mCall = EeWonLanCall::NetworkTimer; c.mA = C(3) ? (void*)ALiveObj() : P(0x6001); break;
    case 14: case 15: case 16: case 17: c.mCall = EeWonLanCall::RecvMsg; c.mA = P(0x6201); break;
    case 18: c.mCall = EeWonLanCall::DiscoveryTimer; c.mA = C(3) ? (void*)ALiveObj() : P(0x6001); break;
    case 19: case 20: case 21: c.mCall = EeWonLanCall::DiscoveryRecv; c.mA = P(0x6301); break;
    case 22: case 23: {
        c.mCall = EeWonLanCall::AddPending;
        Obj* o = C(2) ? ALiveObj() : NULL;
        if (o == NULL) { o = NewObj(); if (C(2)) SetClientOf(o, sPoolClients[C((unsigned)sPoolClients.size())]); }
        c.mA = o;
        break;
    }
    case 24: c.mCall = EeWonLanCall::MyClient; break;
    default: c.mCall = EeWonLanCall::Accept; c.mA = P(0x6401); break;
    }
    return c;
}

static void Dump(EeWonLan* inSubject, bool inFinal)
{
    std::string s = "D";
    std::vector<Ref*> aLists;
    for (std::map<int, Ref*>::iterator it = sRefs.begin(); it != sRefs.end(); ++it) {
        Ref* r = it->second;
        s += " h" + N(r->mHandle) + ":" + N(r->mRc);
        if (r->mKind == (int)MK::ClientList) {
            s += "[";
            for (intptr_t i = 0; i < Call(r, MC::ListCount); i++) {
                Ref* aClient = RR(Call(r, MC::ListAt, i));
                s += PS(aClient) + "#" + N(Get(aClient, MF::ClientId)) + "/" + Hex(TextOf(Get(aClient, MF::ClientName)).data(),
                     TextOf(Get(aClient, MF::ClientName)).size()) + "/" + N(Call(aClient, MC::ClientIsCaptain, 1)) + " ";
            }
            s += "]";
        } else if (r->mKind == (int)MK::GameList) {
            s += "[";
            Call(r, MC::GameListRewind);
            while (Call(r, MC::GameListHasMore)) {
                Ref* aGame = RR(Call(r, MC::GameListNext));
                s += PS(aGame) + "/" + TextOf(Get(aGame, MF::GameName)) + "/" + N(Get(aGame, MF::GameLastSeen)) + "/" +
                     AddrText((const EeWonIpAddr*)Get(aGame, MF::GameAddress)) + " ";
            }
            s += "]";
        } else if (r->mKind == (int)MK::Game) {
            s += "{" + PS(P((uintptr_t)Get(r, MF::GameClients))) + " " + N(Get(r, MF::GameCaptainId)) + "}";
        }
    }
    for (int i = 0; i < sNextObj && i < kObjs; i++)
        if (sObjs[i].mLive) s += " o" + N(i) + ":" + N(sObjs[i].mRc);
    if (inFinal) {}
    else if (inSubject != NULL) s += " me " + PS(P((uintptr_t)ee_won_lan_call(inSubject, EeWonLanCall::MyClient, NULL, NULL, 0, 0)));
    else s += " me " + PS(sRefLan->mMyClient);
    Line(s);
}

static std::vector<std::string> Sequence(int inSeq, bool inSubject)
{
    sTrace.clear();
    sRefs.clear();
    sRefOf.clear();
    sNextRef = 0;
    sNextObj = 0;
    for (int i = 0; i < kObjs; i++) sObjs[i].mLive = false;
    sAnswerSeed = 0x7a31u + (unsigned)inSeq * 7919u;
    sCallSeed = 0x33u + (unsigned)inSeq * 104729u;
    sGameTypeBase = (unsigned)inSeq % 5;
    sEmpty = new Ref;
    sEmpty->mRc = 1 << 30; sEmpty->mKind = kText; sEmpty->mHandle = -1; sEmpty->mModel = NULL;
    sPoolClients.clear();
    sPoolGames.clear();
    for (int i = 0; i < 3; i++) {
        Ref* aClient = NewRef((int)MK::Client);
        RefAdd(aClient);
        Set(aClient, MF::ClientId, 1 + i);
        SetText(aClient, MF::ClientName, std::string("c\0", 2));
        sPoolClients.push_back(aClient);
    }
    for (int i = 0; i < 2; i++) {
        Ref* aGame = NewRef((int)MK::Game);
        RefAdd(aGame);
        Set(aGame, MF::GameType, (intptr_t)((i + sGameTypeBase) % 5));
        Set(aGame, MF::GameHasPassword, (inSeq >> i) & 1);
        EeWonIpAddr* anAddress = ee_won_ipaddr_new();
        SetAddr(anAddress, 7 + i, 1000 * (i + inSeq % 2));
        Set(aGame, MF::GameAddress, (intptr_t)anAddress);
        ee_won_ipaddr_free(anAddress);
        sPoolGames.push_back(aGame);
    }
    Ref* aClients = NewRef((int)MK::ClientList);
    Ref* aLan = NewRef((int)MK::GameList);
    Set(aLan, MF::GameListType, 1);
    Ref* aDirect = NewRef((int)MK::GameList);
    Set(aDirect, MF::GameListType, 3);
    RefAdd(aClients); RefAdd(aLan); RefAdd(aDirect); // the driver's: the lists outlive both worlds' objects
    EeWonLan* r;
    if (inSubject) {
        r = ee_won_lan_new_model(P(0x52), Host, aClients, aLan, aDirect);
    } else {
        sRefLan = new RefLan(aClients, aLan, aDirect);
        r = ee_won_lan_new(P(0x52), RefHost);
    }
    for (int i = 0; i < 8; i++) {
        CallArgs c = NextCall();
        Line("== " + N((int)c.mCall) + " " + PS(c.mA) + " " + PS(c.mB) + " " + N(c.mMessage ? 0 : c.mC) + " m:" +
             (c.mMessage && c.mA != NULL ? Hex(c.mBytes.data(), c.mBytes.size()) : "-"));
        intptr_t aC = c.mMessage ? (c.mA != NULL ? (intptr_t)(c.mBytes.empty() ? "" : c.mBytes.data()) : 0) : c.mC;
        intptr_t aD = c.mMessage ? (intptr_t)c.mBytes.size() : 0;
        intptr_t aResult = ee_won_lan_call(r, c.mCall, c.mA, c.mB, aC, aD);
        Line("=> " + (c.mCall == EeWonLanCall::MyClient ? PS(P((uintptr_t)aResult)) : N(aResult)));
        Dump(inSubject ? r : NULL, false);
    }
    ee_won_lan_free(r);
    if (!inSubject) { sRefLan->Free(); delete sRefLan; sRefLan = NULL; }
    Dump(NULL, true);
    for (int i = 0; i < sNextObj && i < kObjs; i++) if (sObjs[i].mLive) SetClientOf(&sObjs[i], NULL);
    for (size_t i = 0; i < sPoolClients.size(); i++) RefRel(sPoolClients[i]);
    for (size_t i = 0; i < sPoolGames.size(); i++) RefRel(sPoolGames[i]);
    RefRel(aClients);
    RefRel(aLan);
    RefRel(aDirect);
    int aLeft = 0;
    for (std::map<int, Ref*>::iterator it = sRefs.begin(); it != sRefs.end(); ++it) if (it->second->mKind != kText) aLeft++;
    for (int i = 0; i < sNextObj && i < kObjs; i++) if (sObjs[i].mLive && sObjs[i].mRc > 0) aLeft++;
    Line("left " + N(aLeft));
    delete sEmpty;
    return sTrace;
}

int main(int argc, char** argv)
{
    const int kSequences = 20000;
    for (intptr_t i = -1; i < 17; i++) CHECK(ee_won_lobby_text(i) == TextRows(i));
    size_t aLines = 0;
    std::set<std::string> aSeen;
    bool aVerbose = argc > 1 && strcmp(argv[1], "--verbose") == 0;
    for (int s = 0; s < kSequences; s++) {
        if (aVerbose) { fprintf(stderr, "seq %d\n", s); fflush(stderr); sEcho = true; }
        std::vector<std::string> aWant = Sequence(s, false);
        if (aVerbose) { fprintf(stderr, " ref done\n"); fflush(stderr); }
        std::vector<std::string> aGot = Sequence(s, true);
        aLines += aWant.size();
        CHECK(aWant.size() == aGot.size());
        for (size_t i = 0; i < aWant.size() && i < aGot.size(); i++) {
            sChecks++;
            if (aWant[i] != aGot[i]) {
                sFailures++;
                if (sFailures < 12)
                    printf("FAIL sequence %d line %u:\n want %s\n  got %s\n", s, (unsigned)i, aWant[i].c_str(), aGot[i].c_str());
                break;
            }
        }
        for (size_t i = 0; i < aWant.size(); i++) {
            const std::string& aLine = aWant[i];
            if (aLine.compare(0, 1, "D") == 0 || aLine.compare(0, 2, "=>") == 0) continue;
            aSeen.insert(aLine.substr(0, aLine.find(' ', aLine.compare(0, 2, "==") == 0 ? 3 : 0)));
        }
        CHECK(aWant.back() == "left 0");
    }
    printf("won_lanmodel_test: %d sequences, %u trace lines, %u distinct ops and calls, %d checks, %d failures\n",
           kSequences, (unsigned)aLines, (unsigned)aSeen.size(), sChecks, sFailures);
    return sFailures;
}
