// StagingLogic's and LanLogic's host cases R6 WONLobby round 29 made Rust (ee_won_lobby lobbyops.rs) against the
// deleted cases as they were (won_lobbyops_orig.h: LobbyLogicHost.cpp's, LanLogicHost.cpp's and
// LanLogic::HandlePeerAuthDone), over one stand-in world (won_lobbyops_world.h) that logs every call. The Rust's
// calls out are answered by World below: the hosts' new one-statement cases, their statements copied, and the four
// netops.rs answers over the network objects (an op's status and success, a socket's listen, a received message
// taken) as the C++ calls they make. Each op runs for every combination of the world's answers (the op and bind
// statuses, the auth context and its expiry, the CD key, the chat sounds, the parser, the permission, the config
// and its flag, the join op, the reply message, the direct dialog's name): on objects made alike for the two
// sides, then the traces and every object's references are compared. The exit code is the number of mismatches.
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>
#include "ee/wonapi/WONStatus.h"
#include "rust/won_net/hostop.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbyops.h"
#include "rust/won_lobby/resource.h"
#include "won_lobbyops_world.h"
#include "won_lobbyops_orig.h"

using namespace LoWorld;
typedef EeWonLobbyOp Op;

static intptr_t Held(RefCount* inObj)
{
    inObj->AddRef();
    return (intptr_t)inObj;
}
// LanLogicHost.cpp's helper for LanNewConnect, as it is
static SocketOp* LanNewConnectOp(LobbyLanSocket* inSocket, IPAddr inAddress) { return new SocketOp(EeWonCoreSocketOp::Connect, inSocket, NULL, &inAddress); }

static bool sLan; // the logic is a LanLogic (else a StagingLogic)

// the host's cases the Rust calls (LobbyLogicHost.cpp's and LanLogicHost.cpp's, as they are), and netops.rs's
static intptr_t World(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    StagingLogic* st = static_cast<StagingLogic*>(inLogic);
    LanLogic* l = static_cast<LanLogic*>(inLogic);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyLanSocket* sock = static_cast<LobbyLanSocket*>(inA);
    AsyncOp* op = static_cast<AsyncOp*>(inA);
    ChatCommandParser* parser = static_cast<ChatCommandParser*>(inA);
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    if (sLan) st = NULL;
    else l = NULL;
    switch (inOp) {
    // netops.rs's (the network objects' Rust in the game)
    case EeWonLobbyOp::OpStatusOf: return op->GetStatus();
    case EeWonLobbyOp::OpSucceededOf: return op->Succeeded() ? 1 : 0;
    case EeWonLobbyOp::SocketListen: return sock->Listen((int)inC);
    case EeWonLobbyOp::RefRelease: static_cast<RefCount*>(inA)->Release(); return 0;
    case EeWonLobbyOp::LanSocketClose: if (sock != NULL) sock->QueueClose((long)inC); return 0;
    case EeWonLobbyOp::LanRecvMsgTake: {
        ByteBuffer* aMsg = static_cast<SocketOp*>(op)->GetMsg().get();
        if (aMsg != NULL) Sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return (intptr_t)aMsg;
    }
    // the hosts' older cases
    case EeWonLobbyOp::ChatParser: return (intptr_t)LobbyGetChatCommandParser();
    case EeWonLobbyOp::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)inC, true); return 0;
    case EeWonLobbyOp::NetAdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    case EeWonLobbyOp::LanGameChanged: LobbyBroadcastEvent(GameChangedEvent(g, (int)inC), true); return 0;
    // LobbyLogicHost.cpp's new ones
    case EeWonLobbyOp::StJoinOpIs: return st->mJoinOp.get() != NULL;
    case EeWonLobbyOp::StJoinOpGet: return (intptr_t)st->mJoinOp.get();
    case EeWonLobbyOp::StJoinOpFinishSuccess: st->mJoinOp->Finish(WS_Success);
        return 0;
    case EeWonLobbyOp::StJoinOpClear: st->mJoinOp = NULL;
        return 0;
    case EeWonLobbyOp::StJoinOpKillNow: st->mJoinOp->Kill(true);
        return 0;
    case EeWonLobbyOp::RunOpWithStatusRows: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, 1, 0);
        return 0;
    case EeWonLobbyOp::MessageBoxRows: LobbyMessageBox(aTitle, aDetails);
        return 0;
    case EeWonLobbyOp::StCreateDialogResetAll: st->mCreateGameDialog->LCall(EeWonLobbyDlgCall::Reset, true);
        return 0;
    case EeWonLobbyOp::NetLogout: LobbyNetLogout();
        return 0;
    case EeWonLobbyOp::KillPings: LobbyKillPings(0);
        return 0;
    case EeWonLobbyOp::ShowLoginScreen: LobbyShowLoginScreen();
        return 0;
    case EeWonLobbyOp::SaveGlobalPrefs: LobbySaveGlobalPrefs(false);
        return 0;
    case EeWonLobbyOp::SaveUserPrefs: LobbySaveUserPrefs(false);
        return 0;
    case EeWonLobbyOp::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop();
        return 0;
    case EeWonLobbyOp::GameClearPlayers: g->ClearPlayers();
        return 0;
    case EeWonLobbyOp::StClearMyClient: st->mMyClient = NULL;
        return 0;
    case EeWonLobbyOp::GamePreNetLeave: g->PreNetLeaveGame();
        return 0;
    case EeWonLobbyOp::NetLeaveGame: LobbyNetLeaveGame(g);
        return 0;
    case EeWonLobbyOp::GamePostNetLeave: g->PostNetLeaveGame();
        return 0;
    case EeWonLobbyOp::GameSetInProgress: g->SetInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::NetSetGameInProgress: LobbyNetSetGameInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::LanPingGameNotify: LobbyPingGame(g, true);
        return 0;
    case EeWonLobbyOp::QueryGameDetails: LobbyQueryGameDetails(g);
        return 0;
    case EeWonLobbyOp::GameSetShowDetails: g->SetShowDetails(inC != 0);
        return 0;
    case EeWonLobbyOp::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case EeWonLobbyOp::ConfigAllowJoinWithoutPing: return static_cast<LobbyConfig*>(inA)->mAllowJoinGameWithoutPing;
    // LanLogicHost.cpp's new ones
    case EeWonLobbyOp::AuthGet: return (intptr_t)GetLobbyAuthContext();
    case EeWonLobbyOp::AuthSetAutoRefresh: static_cast<AuthContext*>(inA)->SetDoAutoRefresh(inC != 0);
        return 0;
    case EeWonLobbyOp::AuthIsExpired: return static_cast<AuthContext*>(inA)->IsExpired((time_t)inC) ? 1 : 0;
    case EeWonLobbyOp::AuthClearUserName: static_cast<AuthContext*>(inA)->SetUserName(std::wstring(gEmptyWideString));
        return 0;
    case EeWonLobbyOp::AuthClearPassword: static_cast<AuthContext*>(inA)->SetPassword(std::wstring(gEmptyWideString));
        return 0;
    case EeWonLobbyOp::AuthCommunity: return (intptr_t)static_cast<AuthContext*>(inA)->GetPeerData()->GetFirstCommunity();
    case EeWonLobbyOp::CdKeyValid: return LobbyIsCDKeyValid() ? 1 : 0;
    case EeWonLobbyOp::GetCertNew: return Held(new GetCertOp(static_cast<AuthContext*>(inA)));
    case EeWonLobbyOp::LanRunPeerAuth: l->RunTrackedOp(SmartPtr<PeerAuthServerOp>(new PeerAuthServerOp(static_cast<AuthContext*>(inB), sock)).get(), new LogicCompletion<LanLogic>(&LanLogic::HandlePeerAuthDone, l), 30000);
        return 0;
    case EeWonLobbyOp::PeerAuthSocket: return (intptr_t) static_cast<LobbyLanSocket*>(static_cast<PeerAuthServerOp*>(op)->GetSocket());
    case EeWonLobbyOp::PeerAuthPermits: return static_cast<PeerAuthServerOp*>(op)->ClientPermits((DWORD)inC) ? 1 : 0;
    case EeWonLobbyOp::ChatSoundEffects: return LobbyGetChatSoundEffects() ? 1 : 0;
    case EeWonLobbyOp::PlayWhisperSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_WhisperRecv_Sound).get());
        return 0;
    case EeWonLobbyOp::ParserSetInputName: parser->SetInput(static_cast<LobbyClient*>(inB)->GetName());
        return 0;
    case EeWonLobbyOp::LanSetLanNameFromDialog: LobbySetLanName(l->mLanNameDialog->InputText(0));
        return 0;
    case EeWonLobbyOp::LanSetBroadcastPortTo: LobbySetLanBroadcastPort((int)inC);
        return 0;
    case EeWonLobbyOp::LanBindLobbyPort: return sock->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case EeWonLobbyOp::ErrorDialogRow: LobbyDoErrorDialog(gLobbyResources[inC].mString, (WONStatus)inD);
        return 0;
    case EeWonLobbyOp::LanArmAcceptCompletion: sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleAccept, l));
        return 0;
    case EeWonLobbyOp::LanArmAcceptOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept));
        return 0;
    case EeWonLobbyOp::LanBroadcastAddrAssign: IpAssign(static_cast<EeWonIpAddr*>(inA), IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort()).GetValue());
        return 0;
    case EeWonLobbyOp::LanNewQueueSocket: return Held(new QueueSocket(1));
    case EeWonLobbyOp::LanBindBroadcast: return static_cast<QueueSocket*>(inA)->Fn2B0D90(LobbyGetLanBroadcastPort(), 1);
    case EeWonLobbyOp::LanArmDiscoveryCompletion: static_cast<QueueSocket*>(inA)->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryRecv, l));
        return 0;
    case EeWonLobbyOp::LanArmDiscoveryOp: static_cast<QueueSocket*>(inA)->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvFrom, NULL, NULL, NULL, 1024));
        return 0;
    case EeWonLobbyOp::LanNewPeerAuthConnect: return Held(new PeerAuthOp(g->GetAddress(), reinterpret_cast<AuthContext*>(inC), AUTH_TYPE_PERSISTENT, static_cast<LobbyLanSocket*>(inB), true));
    case EeWonLobbyOp::LanNewConnect: return Held(LanNewConnectOp(static_cast<LobbyLanSocket*>(inB), g->GetAddress()));
    case EeWonLobbyOp::LanNewRecvMsg: return Held(new SocketOp(EeWonCoreSocketOp::RecvMsg, sock));
    case EeWonLobbyOp::LanArmCloseCompletion: sock->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, l));
        return 0;
    case EeWonLobbyOp::LanArmRecvCompletion: sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, l));
        return 0;
    case EeWonLobbyOp::LanArmRecvOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvMsg));
        return 0;
    case EeWonLobbyOp::LanDirectInputLength: return (intptr_t)l->mDirectConnectDialog->InputText(0).length();
    case EeWonLobbyOp::GameSetDirectName: g->SetName(l->mDirectConnectDialog->InputText((int)inC));
        return 0;
    case EeWonLobbyOp::AddDirectConnectNamed: LobbyAddDirectConnect(Addr(inB), l->mDirectConnectDialog->InputText((int)inC));
        return 0;
    default: Log("unexpected op " + N((long long)inOp)); return 0;
    }
}

// ---- the sides: their objects made alike, numbered from 0

struct Side {
    StagingLogic mSt;
    LanLogic mLan;
    LobbyGame* mGame;
    LobbyClient* mClient;
    LobbyLanSocket* mSocket;
    ChatCommandParser* mParser;
    AuthContext* mAuth;
    PeerAuthServerOp* mPeer;
    LobbyConfig mConfig;
};
// the world's answers, one bit each
enum { K_STATUS, K_BIND, K_AUTH, K_EXPIRED, K_CDKEY, K_SOUNDS, K_PARSER, K_PERMITS, K_CONFIG, K_ALLOW, K_JOINOP,
       K_MESSAGE, K_NAME, K_COUNT };
static void Make(Side& s, unsigned k)
{
    sObjects.clear();
    s.mGame = new LobbyGame;
    s.mGame->AddRef();
    s.mClient = new LobbyClient;
    s.mClient->AddRef();
    s.mSocket = new LobbyLanSocket;
    s.mSocket->AddRef();
    s.mParser = new ChatCommandParser;
    s.mAuth = new AuthContext;
    s.mAuth->mExpired = (k >> K_EXPIRED & 1) != 0;
    sNextStatus = (k >> K_STATUS & 1) ? 0 : -5;
    s.mPeer = new PeerAuthServerOp(s.mAuth, s.mSocket);
    s.mPeer->AddRef();
    s.mSt.mCreateGameDialog = new Dialog;
    s.mLan.mLanNameDialog = new Dialog;
    s.mLan.mDirectConnectDialog = new Dialog;
    s.mLan.mLanNameDialog->mInputs[0] = GUIString("lanname");
    s.mLan.mDirectConnectDialog->mInputs[0] = GUIString((k >> K_NAME & 1) ? "My game" : "");
    s.mLan.mDirectConnectDialog->mInputs[1] = GUIString("10.0.0.7:2300");
    if (k >> K_JOINOP & 1) s.mSt.mJoinOp = new AsyncOp;
    s.mSt.mMyClient = s.mClient;
    s.mConfig.mAllowJoinGameWithoutPing = (k >> K_ALLOW & 1) ? 1 : 0;
    sBindStatus = (k >> K_BIND & 1) ? 0 : 10048;
    sAuth = (k >> K_AUTH & 1) ? s.mAuth : NULL;
    sCdKeyValid = (k >> K_CDKEY & 1) != 0;
    sChatSounds = (k >> K_SOUNDS & 1) != 0;
    sParser = (k >> K_PARSER & 1) ? s.mParser : NULL;
    sPermits = (k >> K_PERMITS & 1) != 0;
    sConfig = (k >> K_CONFIG & 1) ? &s.mConfig : NULL;
    if (k >> K_MESSAGE & 1) {
        ByteBuffer* aMessage = new ByteBuffer;
        aMessage->mBytes = "reply";
        sReplyMessage = aMessage;
    } else {
        sReplyMessage = NULL;
    }
    sTrace.clear();
}
static void Dump(intptr_t inResult, Side& s)
{
    // a result that is an object as its number
    for (RefCount* o : sObjects)
        if (inResult == (intptr_t)o) {
            Log("result " + Id(o));
            inResult = INTPTR_MAX;
        }
    if (inResult != INTPTR_MAX) Log("result " + N(inResult));
    for (RefCount* o : sObjects) Log("refs " + Id(o) + " " + N(o->mRefs));
    Log("joinop " + Id(s.mSt.mJoinOp.get()) + " myclient " + Id(s.mSt.mMyClient.get()));
}

// the arguments of each op, as its callers give them
struct Case {
    Op mOp;
    bool mLan;
    int mA, mB; // 0 none, 1 the game, 2 the socket, 3 the client, 4 the peer op, 5 the address slot, 6 the status slot, 7 a sink
    intptr_t mC;
};
static void* ArgOf(Side& s, int inKind, intptr_t* ioSlot)
{
    switch (inKind) {
    case 1: return s.mGame;
    case 2: return s.mSocket;
    case 3: return s.mClient;
    case 4: return s.mPeer;
    case 5: return reinterpret_cast<void*>(0x5a5a);
    case 6: return ioSlot;
    case 7: return reinterpret_cast<void*>(0x7777);
    default: return NULL;
    }
}

static int sChecks, sFailures;
static std::map<std::string, int> sFailedOps;
static void Run(const Case& c, unsigned k)
{
    std::vector<std::string> aTraces[2];
    for (int aSide = 0; aSide < 2; aSide++) {
        Side s;
        Make(s, k);
        sLan = c.mLan;
        intptr_t aSlot = -1;
        void* a = ArgOf(s, c.mA, &aSlot);
        void* b = ArgOf(s, c.mB, &aSlot);
        void* aLogic = c.mLan ? static_cast<void*>(&s.mLan) : static_cast<void*>(&s.mSt);
        intptr_t r;
        if (c.mOp == Op::LanPeerAuthDone) {
            if (aSide == 0) {
                s.mLan.HandlePeerAuthDone(s.mPeer);
                r = 0;
            } else {
                // lanobj.rs's PeerAuthDone: the socket the checks accept added as pending
                r = ee_won_lobby_lobbyops_op(aLogic, c.mOp, a, b, c.mC, 0, World);
                if (r != 0) s.mLan.Call(EeWonLanCall::AddPending, reinterpret_cast<void*>(r));
                r = 0;
            }
        } else if (aSide == 0) {
            r = c.mLan ? LoOrig::Lan(&s.mLan, c.mOp, a, b, c.mC, 0) : LoOrig::St(&s.mSt, c.mOp, a, b, c.mC, 0);
        } else {
            r = ee_won_lobby_lobbyops_op(aLogic, c.mOp, a, b, c.mC, 0, World);
        }
        if (c.mB == 6) Log("slot " + N(aSlot));
        Dump(r, s);
        aTraces[aSide].swap(sTrace);
        sReplyMessage = NULL;
    }
    sChecks++;
    if (aTraces[0] == aTraces[1]) return;
    sFailedOps[N((long long)c.mOp)]++;
    if (sFailures++ < 12) {
        printf("FAIL op %lld world %#x:\n", (long long)c.mOp, k);
        for (size_t i = 0; i < aTraces[0].size() || i < aTraces[1].size(); i++)
            printf("  %-60s | %s\n", i < aTraces[0].size() ? aTraces[0][i].c_str() : "", i < aTraces[1].size() ? aTraces[1][i].c_str() : "");
    }
}

int main()
{
    for (int i = 0; i < 1024; i++) gLobbyResources[i].mString = GUIString(("r" + N(i)).c_str());
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_SierraLogon_Sound].mSound = new Sound;
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_SierraLogon_Sound].mSound->mName = "logon";
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_WhisperRecv_Sound].mSound = new Sound;
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_WhisperRecv_Sound].mSound->mName = "whisper";
    static const Case kCases[] = {
        {Op::StJoinOpFinish, false, 0, 0, 0}, {Op::StJoinOpRun, false, 0, 0, 0}, {Op::StJoinOpKill, false, 0, 0, 0},
        {Op::StLogoutTail, false, 0, 0, 0}, {Op::GameLeaveNet, false, 1, 0, 0}, {Op::GameSetInProgressNet, false, 1, 0, 0},
        {Op::GameSetInProgressNet, false, 1, 0, 5}, {Op::GamePingAndNotify, false, 1, 0, 0},
        {Op::GameShowDetails, false, 1, 0, 0}, {Op::GameHideDetails, false, 1, 0, 0},
        {Op::AllowJoinWithoutPing, false, 0, 0, 0},
        {Op::LanAuthAutoRefreshOff, true, 0, 0, 0}, {Op::LanWhisperReceived, true, 3, 0, 0},
        {Op::LanNameDialogSave, true, 0, 0, 0}, {Op::LanSetBroadcastPort, true, 0, 0, 2302},
        {Op::LanRefreshCertificate, true, 0, 0, 0}, {Op::LanStartPeerAuth, true, 2, 0, 0},
        {Op::LanPeerAuthDone, true, 4, 0, 0}, {Op::LanListen, true, 2, 0, 0}, {Op::LanNewDiscovery, true, 5, 6, 0},
        {Op::LanArmDiscovery, true, 2, 0, 0}, {Op::LanConnectTo, true, 1, 2, 0}, {Op::LanConnectTo, true, 1, 2, 1},
        {Op::LanRecvReply, true, 2, 7, 0}, {Op::LanArmSocket, true, 2, 0, 0}, {Op::LanDirectSave, true, 1, 5, 0},
    };
    for (const Case& c : kCases)
        for (unsigned k = 0; k < (1u << K_COUNT); k++) Run(c, k);
    for (const auto& f : sFailedOps) printf("mismatching: op %s x %d\n", f.first.c_str(), f.second);
    printf("won_lobbyops_test: %d cases, %d mismatches\n", sChecks, sFailures);
    return sFailures;
}
