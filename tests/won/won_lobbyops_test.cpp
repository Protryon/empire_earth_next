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
#include "rust/won_net/status_codes.h"
#include "rust/won_net/hostop.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbyops.h"
#include "rust/won_lobby/resource.h"
#include "won_lobbyops_world.h"
#include "won_lobbyops_orig.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/netlogicops.h"
#include "won_netlogic_world.h"
#include "won_netlogic_orig.h"
#include "won_netlogic_new.h"
#include "rust/won_lobby/lobbyconf.h"
// the Rust's record of the world's config (lobbyconf.rs: the Internet logics' compositions read it, R6 WON lobby/net
// round 40), the world's values or none
static EeWonLobbyConf* sRustConf;
static void SyncConf(const LoWorld::LobbyConfig* c)
{
    ee_won_lobbyconf_free(sRustConf);
    sRustConf = NULL;
    if (c == NULL) return;
    sRustConf = ee_won_lobbyconf_new(NULL);
    typedef EeWonLobbyConfList L;
    for (const std::string& n : c->mDirServers) ee_won_lobbyconf_list_push(sRustConf, L::DirServers, (const uint8_t*)n.data(), n.size());
    for (const std::string& n : c->mRoomDirServers) ee_won_lobbyconf_list_push(sRustConf, L::RoomDirServers, (const uint8_t*)n.data(), n.size());
    for (const std::string& n : c->mAuthServers) ee_won_lobbyconf_list_push(sRustConf, L::AuthServers, (const uint8_t*)n.data(), n.size());
    ee_won_lobbyconf_set_text(sRustConf, EeWonLobbyConfText::ProductName, (const uint8_t*)c->mProductName.data(), c->mProductName.size());
    ee_won_lobbyconf_set_text(sRustConf, EeWonLobbyConfText::Language, (const uint8_t*)c->mLanguage.data(), c->mLanguage.size());
    ee_won_lobbyconf_set_text(sRustConf, EeWonLobbyConfText::Version, (const uint8_t*)c->mVersion.data(), c->mVersion.size());
    ee_won_lobbyconf_set_product_dir(sRustConf, c->mProductDirectory.data(), c->mProductDirectory.size());
    ee_won_lobbyconf_set_num(sRustConf, EeWonLobbyConfNum::AllowJoinGameWithoutPing, c->mAllowJoinGameWithoutPing);
    ee_won_lobbyconf_set_current(sRustConf);
}

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
    // R6 WON round 32's LanPortDialogRun statements (LanLogicHost.cpp)
    case EeWonLobbyOp::LanPortSetGamePort: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetGamePort, (unsigned short)LobbyGetLobbyPort()); return 0;
    case EeWonLobbyOp::LanPortSetPort: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetPort, (unsigned short)(int)inC); return 0;
    case EeWonLobbyOp::LanPortReset: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case EeWonLobbyOp::LanPortDo: return LobbyDoDialog(l->mLanPortDialog.get());
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
    s.mLan.mLanPortDialog = new Dialog;
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
    sDoDialogResult = (k >> K_NAME & 1) ? 100001 : 100002;
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

// ---- the Internet logics' moved cases (R6 WON round 32: AccountLogic's and InitLogic's, netlogicops.rs) against
// the deleted cases (won_netlogic_orig.h), the Rust's calls out answered by the hosts' new cases as they are
// (won_netlogic_new.h), over won_netlogic_world.h; each case for every combination of the world's answers its
// callers allow (the config, the six server lists, the user, the terms of use, the remember and newsletter flags,
// the accept op and its socket, the decoded picture, the dialogs' answer)

using namespace NlWorld;
struct NSide {
    AccountLogic mAcct;
    InitLogic mInit;
    ComponentEvent mLogin, mLostPassword, mLostName, mCreate, mUpdate;
    ServerContext* mLists[7];
    AuthContext* mAuth;
    LobbyUserInfo* mUser;
    NativeImage* mImage;
    RustServerOp* mOp;
    AsyncSocket* mSocket;
    SocketOp* mSocketOp;
    DetectFirewallOp* mFirewallOp;
    GUIString mText;
    size_t mSlot;
    LobbyConfig mConfig;
};
// (the list bits: sLists[1..6] in order, auth, account, firewall detect, patch, directory, room directory)
enum { NK_CONFIG, NK_AUTH_, NK_ACCOUNT_, NK_FIREWALL, NK_PATCH, NK_DIR, NK_ROOMDIR, NK_USER, NK_NEWER, NK_REMEMBER, NK_NEWS,
       NK_ACCEPT, NK_SOCKET, NK_IMAGE, NK_DIALOG, NK_COUNT };
static std::vector<intptr_t> Bytes(const std::wstring& w) { return {(intptr_t)w.data(), (intptr_t)(w.size() * sizeof(wchar_t))}; }
static std::vector<intptr_t> Bytes(const std::string& s) { return {(intptr_t)s.data(), (intptr_t)s.size()}; }
static std::vector<intptr_t> Cat(std::initializer_list<std::vector<intptr_t>> inParts)
{
    std::vector<intptr_t> v;
    for (const auto& p : inParts) v.insert(v.end(), p.begin(), p.end());
    return v;
}
static void NMake(NSide& s, unsigned k)
{
    sObjects.clear();
    for (int i = 1; i <= 6; i++) s.mLists[i] = new ServerContext;
    s.mAuth = new AuthContext;
    s.mUser = new LobbyUserInfo;
    s.mImage = new NativeImage;
    s.mOp = new RustServerOp("the op");
    s.mSocket = new AsyncSocket;
    s.mSocketOp = new SocketOp(EeWonCoreSocketOp::Accept, s.mSocket);
    s.mFirewallOp = new DetectFirewallOp(s.mLists[3]);
    s.mAcct.mCreditsDialog = new Dialog;
    s.mAcct.mTOUDialog = new Dialog;
    s.mInit.mNetworkAdapterDialog = new Dialog;
    s.mInit.mHTTPSession = new HTTPSession;
    s.mInit.mFirewallSocket = s.mSocket;
    s.mInit.mFirewallAddress.mValue = 0xf00;
    s.mInit.mCrossPromotion = "http://promo";
    if (k >> NK_ACCEPT & 1) s.mInit.mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, (k >> NK_SOCKET & 1) ? s.mSocket : NULL);
    GUIStringData* anEmail = new GUIStringData("e@x.org");
    GUIStringData* aPassword = new GUIStringData("newpw");
    static const std::wstring kLoginName(L"@alice"), kLoginPassword(L"pw1"), kLostName(L"carol"), kCreateName(L"dave"),
        kCreatePassword(L"pw2");
    static const std::string kLostEmail("c@x.org"), kCreateEmail("d@x.org");
    EvtMake(s.mLogin, EeWonLobbyEvtKind::Login, Cat({Bytes(kLoginName), Bytes(kLoginPassword), {(intptr_t)(k >> NK_REMEMBER & 1)}}));
    EvtMake(s.mLostPassword, EeWonLobbyEvtKind::GetLostPassword, Bytes(kLostName));
    EvtMake(s.mLostName, EeWonLobbyEvtKind::GetLostUserName, Bytes(kLostEmail));
    EvtMake(s.mCreate, EeWonLobbyEvtKind::CreateAccount,
            Cat({Bytes(kCreateName), Bytes(kCreatePassword), Bytes(kCreateEmail), {3, 14, 1999, (intptr_t)(k >> NK_NEWS & 1)}}));
    EvtMake(s.mUpdate, EeWonLobbyEvtKind::UpdateAccount, {(intptr_t)anEmail, (intptr_t)aPassword, (intptr_t)(k >> NK_NEWS & 1)});
    s.mText = GUIString("a text");
    s.mSlot = 777;
    s.mConfig.mDirServers = {"dir1:15101", "dir2:15101"};
    s.mConfig.mRoomDirServers = {"room1:15101"};
    s.mConfig.mAuthServers = {"auth1:15200", "auth2:15200", "auth3:15200"};
    s.mConfig.mProductName = "EmpireEarth";
    s.mConfig.mLanguage = "English";
    s.mConfig.mVersion = "2.0";
    s.mConfig.mProductDirectory = L"/EmpireEarth";
    sConfig = (k >> NK_CONFIG & 1) ? &s.mConfig : NULL;
    SyncConf(sConfig);
    for (int i = 1; i <= 6; i++) sLists[i] = (k >> (NK_AUTH_ + i - 1) & 1) ? s.mLists[i] : NULL;
    sUser = (k >> NK_USER & 1) ? s.mUser : NULL;
    sNewer = (k >> NK_NEWER & 1) != 0;
    sImage = (k >> NK_IMAGE & 1) ? s.mImage : NULL;
    sDialogResult = (k >> NK_DIALOG & 1) ? 100001 : 100002;
    sDoDialogResult = sDialogResult;
    sTrace.clear();
}
// an argument: a number, or an object of the side (-1000 - n)
enum { A_LOGIN = 1, A_LOSTPW, A_LOSTNAME, A_CREATE, A_UPDATE, A_ACCOUNTS, A_AUTH, A_OP, A_SOCKETOP, A_FWOP, A_TEXT, A_SLOT,
       A_FWLIST, A_URL };
static intptr_t O(int n) { return -1000 - n; }
static const char kUrl[] = "http://img.example/promo.png";
static intptr_t NArg(NSide& s, intptr_t v)
{
    switch (v) {
    case -1000 - A_LOGIN: return (intptr_t)&s.mLogin;
    case -1000 - A_LOSTPW: return (intptr_t)&s.mLostPassword;
    case -1000 - A_LOSTNAME: return (intptr_t)&s.mLostName;
    case -1000 - A_CREATE: return (intptr_t)&s.mCreate;
    case -1000 - A_UPDATE: return (intptr_t)&s.mUpdate;
    case -1000 - A_ACCOUNTS: return (intptr_t)s.mLists[2];
    case -1000 - A_AUTH: return (intptr_t)s.mAuth;
    case -1000 - A_OP: return (intptr_t) static_cast<AsyncOp*>(s.mOp);
    case -1000 - A_SOCKETOP: return (intptr_t) static_cast<AsyncOp*>(s.mSocketOp);
    case -1000 - A_FWOP: return (intptr_t) static_cast<AsyncOp*>(s.mFirewallOp);
    case -1000 - A_TEXT: return (intptr_t)&s.mText;
    case -1000 - A_SLOT: return (intptr_t)&s.mSlot;
    case -1000 - A_FWLIST: return (intptr_t)s.mLists[3];
    case -1000 - A_URL: return (intptr_t)kUrl;
    default: return v;
    }
}
static void NDump(intptr_t inResult, NSide& s, EeWonLogicOp inOp)
{
    // LoginName's result is the event's own copy of the name: its units
    if (inOp == EeWonLogicOp::LoginName) {
        Log("name " + Narrow(std::wstring((const wchar_t*)inResult, s.mSlot)));
        inResult = 0;
    }
    for (RefCount* o : sObjects)
        if (inResult != 0 && inResult == (intptr_t)o) {
            Log("result " + Id(o));
            inResult = INTPTR_MAX;
        }
    if (inResult != INTPTR_MAX) Log("result " + N(inResult));
    for (RefCount* o : sObjects) Log("refs " + Id(o) + " " + N(o->mRefs));
    Log("acceptop " + Id(s.mInit.mAcceptOp.get()) + " fwsocket " + Id(s.mInit.mFirewallSocket.get()) + " fwaddr " +
        N(s.mInit.mFirewallAddress.mValue) + " slot " + N((long long)s.mSlot));
    Log("login name " + EvtDump(&s.mLogin, EeWonLobbyEvtField::UserName));
}
struct NCase {
    int mLogic; // 0 AccountLogic, 1 InitLogic
    EeWonLogicOp mOp;
    intptr_t mA, mB, mC, mD;
    unsigned mNeed; // world bits the callers guarantee
};
static int sNChecks, sNFailures;
static std::map<std::string, int> sNFailedOps;
static void NRun(const NCase& c, unsigned k)
{
    if ((k & c.mNeed) != c.mNeed) return;
    // the world bits each logic reads: the others stay 0
    const unsigned kAccount = 1u << NK_USER | 1u << NK_NEWER | 1u << NK_REMEMBER | 1u << NK_NEWS | 1u << NK_DIALOG;
    const unsigned kInit = (1u << NK_COUNT) - 1 - (1u << NK_USER | 1u << NK_NEWER | 1u << NK_REMEMBER | 1u << NK_NEWS);
    if ((k & ~(c.mLogic == 0 ? kAccount : kInit)) != 0) return;
    std::vector<std::string> aTraces[2];
    for (int aSide = 0; aSide < 2; aSide++) {
        NSide* aNew = new NSide;
        NSide& s = *aNew;
        NMake(s, k);
        intptr_t a = NArg(s, c.mA), b = NArg(s, c.mB), cc = NArg(s, c.mC), d = NArg(s, c.mD);
        intptr_t r;
        if (aSide == 0) r = c.mLogic == 0 ? NlOrig::Account(&s.mAcct, c.mOp, a, b, cc, d) : NlOrig::Init(&s.mInit, c.mOp, a, b, cc, d);
        else if (c.mLogic == 0) r = ee_won_lobby_netlogic_op(&s.mAcct, c.mOp, a, b, cc, d, NlNew::Account);
        else r = ee_won_lobby_netlogic_op(&s.mInit, c.mOp, a, b, cc, d, NlNew::Init);
        NDump(r, s, c.mOp);
        EvtFree(s.mLogin);
        EvtFree(s.mLostPassword);
        EvtFree(s.mLostName);
        EvtFree(s.mCreate);
        EvtFree(s.mUpdate);
        aTraces[aSide].swap(sTrace);
        // the side's objects freed: its members' references first, then every object made
        delete aNew;
        std::vector<RefCount*> anObjects;
        anObjects.swap(sObjects);
        for (RefCount* o : anObjects) delete o;
    }
    sNChecks++;
    if (aTraces[0] == aTraces[1]) return;
    sNFailedOps[N(c.mLogic) + ":" + N((long long)c.mOp)]++;
    if (sNFailures++ < 12) {
        printf("FAIL logic %d op %lld world %#x:\n", c.mLogic, (long long)c.mOp, k);
        for (size_t i = 0; i < aTraces[0].size() || i < aTraces[1].size(); i++)
            printf("  %-60s | %s\n", i < aTraces[0].size() ? aTraces[0][i].c_str() : "", i < aTraces[1].size() ? aTraces[1][i].c_str() : "");
    }
}

// ---- RoutingLogic's moved cases: its objects made alike for the two sides, numbered after NMake's
struct RSide {
    RoutingLogic mLogic;
    LobbyServerList mList;
    RoutingConnection* mConn;
    LobbyGame* mGame;
    LobbyServer* mServer;
    LobbyGroup* mGroup;
    LobbyClient* mClient;
    RoutingOp* mRouteOp;
    RoutingGetUserStatusOp* mStatusOp;
    UDPManager* mUdp;
    ComponentEvent mCreateRoom, mJoinRoom, mOther;
    DirEntityList* mDirList;
    LobbyClientIterator* mIter;
    void* mSink;
    int mStatus;
    intptr_t mSlot;
};
static const uint32_t kStatusIds[] = {5, 3, 5, 9, 0x80000001u};
static void RMake(RSide& r, unsigned k)
{
    r.mLogic.mServerList = &r.mList;
    r.mConn = new RoutingConnection;
    r.mGame = new LobbyGame;
    r.mServer = new LobbyServer;
    r.mGroup = new LobbyGroup;
    r.mClient = new LobbyClient;
    r.mRouteOp = new RoutingOp;
    r.mUdp = new UDPManager;
    r.mStatusOp = new RoutingGetUserStatusOp(IPAddr(), r.mUdp);
    ee_won_userstatus_put(r.mStatusOp->GetRust(), 7, 2);
    ee_won_userstatus_put(r.mStatusOp->GetRust(), 3, 1);
    ee_won_userstatus_put(r.mStatusOp->GetRust(), 0x1ffffffffull, 4);
    r.mCreateRoom.mType = 100011;
    r.mJoinRoom.mType = 100028;
    r.mOther.mType = 100035;
    r.mOther.mComponent = r.mGroup;
    r.mDirList = new DirEntityList{3, 2, 1};
    r.mIter = new LobbyClientIterator{8};
    r.mSink = ee_won_lobby_netlogic_sink_new();
    r.mStatus = -1;
    r.mSlot = -1;
    sServer = (k >> NK_ACCEPT & 1) ? r.mServer : NULL;
    sConnected = (k >> NK_SOCKET & 1) != 0;
    sOnInternet = (k >> NK_DIALOG & 1) != 0;
    sTrace.clear();
}
enum { R_CONN = 40, R_GAME, R_SERVER, R_GROUP, R_CLIENT, R_ROUTEOP, R_STATUSOP, R_UDP, R_CREATEROOM, R_JOINROOM, R_OTHER,
       R_DIRLIST, R_ITER, R_SINK, R_STATUS, R_RSLOT, R_IDS };
static intptr_t RArg(NSide& s, RSide& r, intptr_t v)
{
    switch (v) {
    case -1000 - R_CONN: return (intptr_t)r.mConn;
    case -1000 - R_GAME: return (intptr_t)r.mGame;
    case -1000 - R_SERVER: return (intptr_t)r.mServer;
    case -1000 - R_GROUP: return (intptr_t)r.mGroup;
    case -1000 - R_CLIENT: return (intptr_t)r.mClient;
    case -1000 - R_ROUTEOP: return (intptr_t)r.mRouteOp;
    case -1000 - R_STATUSOP: return (intptr_t) static_cast<AsyncOp*>(r.mStatusOp);
    case -1000 - R_UDP: return (intptr_t)r.mUdp;
    case -1000 - R_CREATEROOM: return (intptr_t)&r.mCreateRoom;
    case -1000 - R_JOINROOM: return (intptr_t)&r.mJoinRoom;
    case -1000 - R_OTHER: return (intptr_t)&r.mOther;
    case -1000 - R_DIRLIST: return (intptr_t)r.mDirList;
    case -1000 - R_ITER: return (intptr_t)r.mIter;
    case -1000 - R_SINK: return (intptr_t)r.mSink;
    case -1000 - R_STATUS: return (intptr_t)&r.mStatus;
    case -1000 - R_RSLOT: return (intptr_t)&r.mSlot;
    case -1000 - R_IDS: return (intptr_t)kStatusIds;
    default: return NArg(s, v);
    }
}
struct RCase {
    EeWonRouteLogicOp mOp;
    intptr_t mA, mB, mC, mD, mE;
    unsigned mNeed;
};
static void RRun(const RCase& c, unsigned k)
{
    if ((k & c.mNeed) != c.mNeed) return;
    const unsigned kRoute = 1u << NK_CONFIG | 1u << NK_ACCEPT | 1u << NK_SOCKET | 1u << NK_DIALOG;
    if ((k & ~kRoute) != 0) return;
    std::vector<std::string> aTraces[2];
    for (int aSide = 0; aSide < 2; aSide++) {
        NSide* aNew = new NSide;
        NSide& s = *aNew;
        NMake(s, k);
        RSide* aRoute = new RSide;
        RSide& r = *aRoute;
        RMake(r, k);
        intptr_t a = RArg(s, r, c.mA), b = RArg(s, r, c.mB), cc = RArg(s, r, c.mC), d = RArg(s, r, c.mD), e = RArg(s, r, c.mE);
        intptr_t res;
        if (aSide == 0) res = NlOrig::Route::Route(&r.mLogic, c.mOp, a, b, cc, d, e);
        else res = ee_won_lobby_netlogic_route_op(&r.mLogic, c.mOp, a, b, cc, d, e, NlNew::Route::Route);
        if (c.mOp == EeWonRouteLogicOp::DirEntities && res != 0) {
            const DirEntityList* aList = reinterpret_cast<const DirEntityList*>(res);
            Log("list #" + N(aList->mOp) + " " + N(aList->mWhich) + " " + N(aList->mIndex));
            res = 0;
        }
        NDump(res, s, EeWonLogicOp::AddRef);
        uint64_t aValues[16];
        size_t n = ee_won_lobby_netlogic_sink_take(r.mSink, aValues, 16);
        std::string aSink;
        for (size_t i = 0; i < n && i < 16; i++) aSink += N((long long)aValues[i]) + ",";
        Log("sink " + aSink + " status " + N(r.mStatus) + " slot " + N((long long)r.mSlot) + " map " + r.mStatusOp->Dump());
        EvtFree(s.mLogin);
        EvtFree(s.mLostPassword);
        EvtFree(s.mLostName);
        EvtFree(s.mCreate);
        EvtFree(s.mUpdate);
        aTraces[aSide].swap(sTrace);
        delete aRoute;
        delete aNew;
        std::vector<RefCount*> anObjects;
        anObjects.swap(sObjects);
        for (RefCount* o : anObjects) delete o;
    }
    sNChecks++;
    if (aTraces[0] == aTraces[1]) return;
    sNFailedOps["2:" + N((long long)c.mOp)]++;
    if (sNFailures++ < 12) {
        printf("FAIL routing op %lld world %#x:\n", (long long)c.mOp, k);
        for (size_t i = 0; i < aTraces[0].size() || i < aTraces[1].size(); i++)
            printf("  %-60s | %s\n", i < aTraces[0].size() ? aTraces[0][i].c_str() : "", i < aTraces[1].size() ? aTraces[1][i].c_str() : "");
    }
}
static void RouteCases()
{
    typedef EeWonRouteLogicOp R;
    const unsigned kConfig = 1u << NK_CONFIG;
    static const RCase kCases[] = {
        {R::ConnDo, O(R_CONN), 1, O(A_AUTH), 0, 0, 0}, {R::ConnDo, O(R_CONN), 2, 0, 0, 0, 0}, {R::ConnDo, O(R_CONN), 3, 0, 0, 0, 0},
        {R::ConnDo, O(R_CONN), 4, 0, 0, 0, 0}, {R::ConnDo, O(R_CONN), 5, 0, 0, 0, 0},
        {R::UserIdLookup, O(A_ACCOUNTS), O(A_TEXT), O(R_STATUS), 0, 0, 0}, {R::DirOpRun, O(A_ACCOUNTS), 0, 0, 0, 0, kConfig},
        {R::ConnAdvertise, O(R_CONN), O(R_GAME), 7, 0, 0, 0}, {R::EventServer, O(R_CREATEROOM), 0, 0, 0, 0, 0},
        {R::EventServer, O(R_JOINROOM), 0, 0, 0, 0, 0}, {R::EventServer, O(R_OTHER), 0, 0, 0, 0, 0},
        {R::StatusOpSetMap, O(R_STATUSOP), O(R_IDS), 5, 0, 0, 0}, {R::StatusOpSetMap, O(R_STATUSOP), O(R_IDS), 0, 0, 0, 0},
        {R::StatusOpMap, O(R_STATUSOP), O(R_SINK), 0, 0, 0, 0},
        {R::EvModerated, O(R_CLIENT), 4, O(R_GROUP), 0x103, O(R_ROUTEOP), 0},
        {R::EvModerated, O(R_CLIENT), 5, O(R_SERVER), 0x2, O(R_ROUTEOP), 0}, {R::ListFree, O(R_DIRLIST), 2, 0, 0, 0, 0},
        {R::ListFree, O(R_ITER), 3, 0, 0, 0, 0}, {R::EvAlert, 1, O(R_ROUTEOP), 0, 0, 0, 0}, {R::EvAlert, 2, O(R_ROUTEOP), 0, 0, 0, 0},
        {R::DirEntities, O(A_OP), O(R_RSLOT), 0, 0, 0, 0}, {R::GroupQueryRun, O(R_SERVER), 0, 0, 0, 0, 0},
        {R::EventNum, O(R_OTHER), 13, 0, 0, 0, 0}, {R::EventNum, O(R_OTHER), 14, 0, 0, 0, 0}, {R::Pref, 8, 0, 0, 0, 0, 0},
        {R::Pref, 9, 0, 0, 0, 0, 0}, {R::Pref, 10, 0, 0, 0, 0, 0}, {R::StatusOpNew, 1, O(R_UDP), 0, 0, 0, 0},
        {R::ConnectOpNew, O(R_CONN), O(R_SERVER), 0, 0, 0, 0}, {R::RNew, O(R_CONN), 12, 0, 0, 0, 0},
    };
    for (const RCase& c : kCases)
        for (unsigned k = 0; k < (1u << NK_COUNT); k++) RRun(c, k);
}
static int NetLogicCases()
{
    typedef EeWonLogicOp L;
    const unsigned kDns = 1u << NK_CONFIG | 1u << NK_AUTH_ | 1u << NK_DIR | 1u << NK_ROOMDIR;
    const unsigned kConfig = 1u << NK_CONFIG;
    static const NCase kCases[] = {
        {0, L::OutOfDateDialog, 0, 0, 0, 0, 0}, {0, L::ShowCredits, 0, 0, 0, 0, 0}, {0, L::ShowTOU, 0, 0, 0, 0, 0},
        {0, L::ShowScreen, 1, 0, 0, 0, 0}, {0, L::ShowScreen, 2, 0, 0, 0, 0}, {0, L::ShowScreen, 3, 0, 0, 0, 0},
        {0, L::RequestPasswordEmail, O(A_LOSTPW), O(A_ACCOUNTS), 0, 0, 0},
        {0, L::RequestInfoEmail, O(A_LOSTNAME), O(A_ACCOUNTS), 0, 0, 0},
        {0, L::QueryAccount, O(A_ACCOUNTS), O(A_AUTH), 0, 0, 0},
        {0, L::UpdateAccount, O(A_ACCOUNTS), O(A_AUTH), O(A_UPDATE), 0, 0},
        {0, L::CreateAccount, O(A_ACCOUNTS), O(A_CREATE), 0, 0, 0}, {0, L::AccountCreated, O(A_CREATE), 0, 0, 0, 0},
        {0, L::LoginName, O(A_LOGIN), O(A_SLOT), 0, 0, 0}, {0, L::LoginStripAt, O(A_LOGIN), 0, 0, 0, 0},
        {0, L::UserNeedsTOU, O(A_LOGIN), 0, 0, 0, 0}, {0, L::SetCredentials, O(A_AUTH), O(A_LOGIN), 0, 0, 0},
        {0, L::InvalidCDKeyBox, 0, 0, 0, 0, 0}, {0, L::SetAdmin, O(A_AUTH), 0, 0, 0, 0}, {0, L::SetAdmin, O(A_AUTH), 1, 0, 0, 0},
        {0, L::GetCert, O(A_AUTH), 0, 0, 0, 0}, {0, L::LoginSaveUser, O(A_LOGIN), 0, 0, 0, 0},
        {1, L::InitStatus, 5, O(A_TEXT), -1, 1 | 3 << 8, 0}, {1, L::InitStatus, 5, 0, 7, 0, 0},
        {1, L::InitStatus, 6, 0, -1, 1, 0}, {1, L::ClearServerLists, 0, 0, 0, 0, 0}, {1, L::LoadTOUTimes, 0, 0, 0, 0, 0},
        {1, L::Music, 0, 0, 0, 0, 0}, {1, L::Music, 1, 0, 0, 0, 0}, {1, L::Music, 2, 0, 0, 0, 0},
        {1, L::DnsReady, 0, 0, 0, 0, 0}, {1, L::FillDnsServers, 0, 0, 0, 0, kDns},
        {1, L::PromotionImage, O(A_OP), 0, 0, 0, 0}, {1, L::FetchPromotionImage, O(A_URL), sizeof(kUrl) - 1, 0, 0, 0},
        {1, L::DirReady, 0, 0, 0, 0, 0}, {1, L::AddDirAddresses, 1, O(A_OP), 2, 0, 0}, {1, L::AddDirAddresses, 2, O(A_OP), 0, 0, 0},
        {1, L::AddDirAddresses, 3, O(A_OP), 1, 0, 0}, {1, L::AddDirAddresses, 4, O(A_OP), 3, 0, 0},
        {1, L::VersionNotifyFailed, O(A_OP), 0, 0, 0, 0}, {1, L::RestartAccept, O(A_SOCKETOP), 0, 0, 0, 0},
        {1, L::StartAccept, O(A_FWOP), 0, 0, 0, 0}, {1, L::RunDnsTest, 0, 0, 0, 0, kConfig},
        {1, L::StartDocuments, 0, 0, 0, 0, 0}, {1, L::RunDirQuery, O(A_ACCOUNTS), 0, 0, 0, 0},
        {1, L::RunVersionCheck, 0, 0, 0, 0, kConfig}, {1, L::RunFirewallDetect, O(A_FWLIST), 0, 0, 0, 0},
        {1, L::KillAcceptOp, 0, 0, 0, 0, 0}, {1, L::AdapterDialog, 0, 0, 0, 0, 0},
    };
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_LobbyMusic_Sound].mSound = new Sound;
    gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_LobbyMusic_Sound].mSound->mName = "music";
    for (const NCase& c : kCases)
        for (unsigned k = 0; k < (1u << NK_COUNT); k++) NRun(c, k);
    RouteCases();
    // the control maps over every number near the lobby's
    int aMaps = 0, aMapFailures = 0;
    for (int id = 199990; id <= 200040; id++) {
        aMaps += 3;
        if (NlOrig::AccountControl(id) != ee_won_lobby_netlogic_control_of(0, id)) aMapFailures++;
        if (NlOrig::Route::RouteControl(id) != ee_won_lobby_netlogic_control_of(2, id)) aMapFailures++;
        if (NlOrig::InitControl(id) != ee_won_lobby_netlogic_control_of(1, id)) aMapFailures++;
    }
    for (const auto& f : sNFailedOps) printf("mismatching: logic:op %s x %d\n", f.first.c_str(), f.second);
    printf("won_lobbyops_test: the Internet logics: %d cases, %d mismatches; control maps %d, %d mismatches\n", sNChecks,
           sNFailures, aMaps, aMapFailures);
    return sNFailures + aMapFailures;
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
        // R6 WON round 32's LanPortDialogRun (lobbyops.rs), a port past 16 bits and a negative one cut as the original's
        {Op::LanPortDialogRun, true, 0, 0, 2301}, {Op::LanPortDialogRun, true, 0, 0, 0x12345}, {Op::LanPortDialogRun, true, 0, 0, -7},
    };
    for (const Case& c : kCases)
        for (unsigned k = 0; k < (1u << K_COUNT); k++) Run(c, k);
    for (const auto& f : sFailedOps) printf("mismatching: op %s x %d\n", f.first.c_str(), f.second);
    printf("won_lobbyops_test: %d cases, %d mismatches\n", sChecks, sFailures);
    return sFailures + NetLogicCases();
}
