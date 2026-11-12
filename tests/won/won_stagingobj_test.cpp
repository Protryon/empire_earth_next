// StagingLogic as a whole object (R6 WON round 20): the original StagingLogic.cpp's methods (the statements copied
// from the C++ before the port, every call on a C++ object replaced by the host op that makes it: LobbyLogicHost.cpp
// runs the same calls in the same order) against ee_won's Rust StagingLogic (stagingobj.rs) over one scripted host.
// Each of 20,000 sequences creates both objects and makes six random inward calls (the buttons, the events, room
// messages of every type cut at random lengths, the sends with C++ and with no messages, joins, hosts, leaves, kicks,
// pings, the ready flags); the host answers every op from a seeded generator, the same stream for both. The
// traces (every op with its arguments and answer, the room messages' bytes) must be the same. One deliberate
// difference: the original's HandleMessageBuffer re-entered HandleMessage itself, here (and in the host) it is the
// op HandleBytes, which the scripted host records without re-entering. The first 100 sequences and every later one
// that makes an op or a call none before made are rust/crates/ee_won/src/golden_stagingobj.txt, which the Rust
// tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

typedef EeWonLobbyOp Op;
typedef EeWonLobbyText LT;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- the scripted host

static unsigned sAnswerSeed;
static unsigned sGameTypeBase; // per sequence
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sAnswerSeed) % n; }
static std::vector<std::string> sTrace;

static void* P(uintptr_t inValue) { return reinterpret_cast<void*>(inValue); }
static const intptr_t kThrew = (intptr_t)EeWonLobbyResult::Threw;

static bool IsMessageOp(Op inOp)
{
    return inOp == Op::HandleBytes || inOp == Op::NetSendToCaptain || inOp == Op::NetBroadcastToGame ||
           inOp == Op::NetSendToClient;
}

static std::string Hex(const void* inBytes, intptr_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (intptr_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inBytes)[i]);
        anOut += aBuffer;
    }
    return anOut;
}

// a pointer answer: none (one in four), or one of two objects of a kind
static intptr_t Obj(uintptr_t inBase) { unsigned r = R(8); return r < 2 ? 0 : (intptr_t)(inBase + (r & 1) + 1); }
static intptr_t Bool(unsigned inTrueIn8) { return R(8) < inTrueIn8 ? 1 : 0; }

static intptr_t Answer(Op inOp, void* inA)
{
    switch (inOp) {
    case Op::StGame: case Op::GameNewCopy: case Op::StTakeDialogGame:
    case Op::EventJoinGame: case Op::EventActionGame: return Obj(0x1000);
    case Op::StMyClient: case Op::StSetMyClientFromNet: case Op::GameFindClient: return Obj(0x2000);
    case Op::ClientPlayer: return Obj(0x2800);
    case Op::GameHold: return (intptr_t)inA;
    case Op::GameType: return (intptr_t)((((uintptr_t)inA & 3) + sGameTypeBase) % 5); // a game's type stays
    case Op::GameScreenType: return R(5);
    case Op::AllowJoinWithoutPing: return (intptr_t)R(3) - 1;
    case Op::EventAction: return (intptr_t)R(11) - 1;
    case Op::ClientId: case Op::GamePing: return Rand(sAnswerSeed) * 3;
    case Op::Now: return 1000 + R(40);
    case Op::YesNo: case Op::StCreateDialogRun: return R(8) < 6 ? 0x186a1 : 0x186a2;
    case Op::GameJoinStatus: return R(8) < 5 ? 0 : (intptr_t)R(9) - 2;
    case Op::GameHandleJoinReply: { unsigned r = R(8); return r < 4 ? 0 : r == 7 ? kThrew : (intptr_t)r - 5; }
    case Op::GameJoinReplySend: case Op::ClientReadPlayer: { unsigned r = R(8); return r < 5 ? 1 : r == 7 ? kThrew : 0; }
    case Op::GameHandleGameMessage: return R(4) == 0 ? kThrew : 0;
    case Op::RunWaitOp: return Bool(2);
    case Op::GameIsPinging: return Bool(4);
    case Op::GameRelease: case Op::StSetGame: case Op::StJoinOpNew: case Op::StJoinOpFinish: case Op::StJoinOpRun:
    case Op::StJoinOpKill: case Op::StSendJoinRequest: case Op::StLogoutTail: case Op::StClientModerated:
    case Op::GameBecomeHost: case Op::GameAddHostClient: case Op::GameLeaveNet: case Op::GameRemoveClient:
    case Op::GameHandlePlayerJoined: case Op::GameSetInProgressNet: case Op::GameResetReadyFlags:
    case Op::GameShowStatus: case Op::GamePingAndNotify: case Op::GameShowDetails: case Op::GameHideDetails:
    case Op::GameDirectConnect: case Op::GameShowOptions: case Op::ClientPlayerSetReady: case Op::ClientPlayerSetPing:
    case Op::ClientBroadcastJoined: case Op::ClientChanged: case Op::ClientTeamChanged: case Op::BufSkip:
    case Op::HandleBytes: case Op::NetSendToCaptain: case Op::NetBroadcastToGame: case Op::NetSendToClient:
    case Op::NetKickClient: case Op::NetRemoveGameAdvertisement: case Op::NetAdvertiseGame:
    case Op::BroadcastLobbyEvent: case Op::MessageBox: case Op::PlaySound: case Op::Exit:
        return 0;
    default: return Bool(6); // the predicates and the steps that can fail
    }
}

static intptr_t ScriptedHost(void* inLogic, Op inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    intptr_t aResult = Answer(inOp, inA);
    char aLine[160];
    if (IsMessageOp(inOp)) {
        snprintf(aLine, sizeof(aLine), "%u %llx %llx m:", (unsigned)inOp, (unsigned long long)(uintptr_t)inA,
                 (unsigned long long)(uintptr_t)inB);
        sTrace.push_back(std::string(aLine) + (inC != 0 ? Hex(P(inC), inD) : std::string("-")) + " -> " +
                         std::to_string((long long)aResult));
    } else {
        snprintf(aLine, sizeof(aLine), "%u %llx %llx %lld %lld -> %lld", (unsigned)inOp,
                 (unsigned long long)(uintptr_t)inA, (unsigned long long)(uintptr_t)inB, (long long)inC,
                 (long long)inD, (long long)aResult);
        sTrace.push_back(aLine);
    }
    (void)inLogic;
    return aResult;
}

static intptr_t H(Op inOp, void* inA = NULL, void* inB = NULL, intptr_t inC = 0, intptr_t inD = 0)
{
    return ScriptedHost(P(0x51), inOp, inA, inB, inC, inD);
}

// ---- the original StagingLogic.cpp (its statements; the objects' calls are the host's ops)

// a message: the C++ ByteBuffer (NULL for one StagingMessage built), its bytes, and whether there is one at all
struct Msg {
    void* mBuffer;
    std::string mBytes;
    bool mPresent;
};
static void AppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static Msg StagingMessage(unsigned char inKind, unsigned short inId, bool inFlag, unsigned short inPing)
{
    EeWonRoomMsg aMsg;
    memset(&aMsg, 0, sizeof(aMsg));
    aMsg.kind = inKind;
    aMsg.id = inId;
    aMsg.flag = inFlag;
    aMsg.ping = inPing;
    Msg aResult = {NULL, std::string(), true};
    ee_won_room_message(&aMsg, AppendBytes, &aResult.mBytes);
    return aResult;
}
static intptr_t HM(Op inOp, const Msg& inMsg, void* inClient)
{
    return H(inOp, inMsg.mBuffer, inClient, inMsg.mPresent ? (intptr_t)inMsg.mBytes.data() : 0,
             (intptr_t)inMsg.mBytes.length());
}
static void* HP(Op inOp, void* inA = NULL, void* inB = NULL, intptr_t inC = 0, intptr_t inD = 0)
{
    return P((uintptr_t)H(inOp, inA, inB, inC, inD));
}

struct ReadBufferException {};
static intptr_t HT(Op inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    intptr_t r = H(inOp, inA, inB, inC, inD);
    if (r == kThrew) throw ReadBufferException();
    return r;
}

// a held game: the original's local SmartPtr<LobbyGame>
struct Held {
    void* mGame;
    explicit Held(void* inGame) : mGame(inGame) {}
    ~Held() { if (mGame != NULL) H(Op::GameRelease, mGame); }
    void* get() const { return mGame; }
};

struct Orig;
struct StagingLeaveOnError {
    Orig* mLogic;
    bool mLeave;
    explicit StagingLeaveOnError(Orig* inLogic) : mLogic(inLogic), mLeave(true) {}
    ~StagingLeaveOnError();
};

struct Orig {
    short mJoinStatus;
    bool mInStaging, mLastReady, mLastAllReady;
    long long mLastReadyTime;
    Orig() : mJoinStatus(0), mInStaging(false), mLastReady(false), mLastAllReady(false), mLastReadyTime(0) {}

    void* Game() { return HP(Op::StGame); }
    void* MyClient() { return HP(Op::StMyClient); }

    void HandleJoinRequest(void* inBuffer, void* inClient)
    {
        if (HT(Op::GameJoinReplySend, Game(), inBuffer, 0, (intptr_t)inClient) == 0) return;
        void* aPlayer = HP(Op::ClientPlayer, inClient);
        if (aPlayer != NULL) {
            H(Op::ClientBroadcastJoined, inClient);
            H(Op::NetAdvertiseGame);
            return;
        }
    }
    void HandleJoinReply(void* inBuffer, void* inClient)
    {
        this->mJoinStatus = (short)HT(Op::GameHandleJoinReply, Game(), inBuffer, 0, 0);
        H(Op::StJoinOpFinish);
    }
    void HandlePlayerJoined(const EeWonRoomMsg& inMsg, void* inBuffer, void* inClient)
    {
        void* aClient = HP(Op::GameFindClient, Game(), NULL, inMsg.id);
        if (aClient == NULL) return;
        bool aHas = H(Op::ClientHasPlayer, aClient) != 0;
        bool aHasAndCaptain = H(Op::ClientHasPlayer, aClient) != 0 && this->IAmCaptain();
        switch (ee_won_stage_player_joined(aHas, aHasAndCaptain)) {
        case 0: {
            if (HT(Op::ClientReadPlayer, aClient, inBuffer, 0, 0) == 0) return;
            break;
        }
        case 2: return;
        }
        H(Op::ClientChanged, aClient, NULL, 0);
        void* aMe = MyClient();
        if (H(Op::ClientIsTeammate, aClient, aMe) != 0)
            H(Op::ClientTeamChanged, aClient, NULL, 0);
        H(Op::GameHandlePlayerJoined, Game(), aClient);
        if (H(Op::StagingSoundEffects)) H(Op::PlaySound, NULL, NULL, (intptr_t)EeWonLobbySound::PlayerJoined);
        if (this->mLastAllReady) this->mLastAllReady = ee_won_stage_joined_keeps_all_ready(true, H(Op::ClientIsReady, aClient) != 0);
        this->NotifyStagingChange();
    }
    void HandleReadyRequest(const EeWonRoomMsg& inMsg, void* inClient)
    {
        this->BroadcastToGame(StagingMessage(5, (unsigned short)H(Op::ClientId, inClient), inMsg.flag, 0));
    }
    void HandleReadyChanged(const EeWonRoomMsg& inMsg, void* inClient)
    {
        void* aClient = HP(Op::GameFindClient, Game(), NULL, inMsg.id);
        if (aClient == NULL || HP(Op::ClientPlayer, aClient) == NULL)
            return;
        H(Op::ClientPlayerSetReady, aClient, NULL, inMsg.flag);
        this->UpdateReadyState();
        H(Op::ClientChanged, aClient, NULL, 2);
    }
    void HandlePingReport(const EeWonRoomMsg& inMsg, void* inClient)
    {
        this->BroadcastToGame(StagingMessage(9, (unsigned short)H(Op::ClientId, inClient), false, inMsg.ping));
    }
    void HandlePingChanged(const EeWonRoomMsg& inMsg, void* inClient)
    {
        void* aClient = HP(Op::GameFindClient, Game(), NULL, inMsg.id);
        if (aClient != NULL && HP(Op::ClientPlayer, aClient) != NULL) {
            H(Op::ClientPlayerSetPing, aClient, NULL, inMsg.ping);
            H(Op::ClientChanged, aClient, NULL, 2);
        }
    }
    void HandleGameDissolved(void* inClient)
    {
        this->LeaveGame(false);
        H(Op::MessageBox, NULL, NULL, (intptr_t)LT::GameDissolved);
    }
    void ResetReadyFlags()
    {
        void* aGame = Game();
        if (aGame == NULL) return;
        H(Op::GameResetReadyFlags, aGame);
        this->UpdateReadyState();
    }
    void SetGameInProgress(bool inInProgress)
    {
        void* aGame = Game();
        if (aGame == NULL) return;
        bool aCurrent = H(Op::GameIsInProgress, aGame) != 0;
        if (!ee_won_stage_sets_in_progress(true, aCurrent, inInProgress, this->IAmCaptain()))
            return;
        H(Op::GameSetInProgressNet, aGame, NULL, inInProgress);
    }
    void HandleStartGame(void* inClient)
    {
        bool isCaptain = this->IAmCaptain();
        if (isCaptain)
            this->SetGameInProgress(true);
        H(Op::Exit, NULL, NULL, ee_won_room_start_exit_code(isCaptain));
    }
    void HandleClientKicked(const EeWonRoomMsg& inMsg, void* inClient)
    {
        void* aClient = HP(Op::GameFindClient, Game(), NULL, inMsg.id);
        if (aClient != NULL) this->HandleKicked(aClient, inMsg.flag);
    }
    void HandleMessage(void* inBuffer, const std::string& inBytes, void* inClient)
    {
        if (Game() == NULL || inClient == NULL) return;
        EeWonRoomMsg aMsg;
        if (ee_won_room_read(reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.length(),
                             this->mJoinStatus == 1, &aMsg) != 0)
            return;
        H(Op::BufSkip, NULL, inBuffer, (intptr_t)aMsg.rest);
        EeWonRoomRoles aRoles;
        aRoles.sender_is_captain = H(Op::ClientIsGameCaptain, inClient) != 0;
        aRoles.i_am_captain = this->IAmCaptain();
        aRoles.sender_has_player = H(Op::ClientHasPlayer, inClient) != 0;
        aRoles.sender_ready = H(Op::ClientIsReady, inClient) != 0;
        if (!ee_won_room_accepts(&aMsg, &aRoles)) return;
        try {
            switch (aMsg.kind) {
            case 1: this->HandleJoinRequest(inBuffer, inClient); break;
            case 2: this->HandleJoinReply(inBuffer, inClient); break;
            case 3: this->HandlePlayerJoined(aMsg, inBuffer, inClient); break;
            case 4: this->HandleReadyRequest(aMsg, inClient); break;
            case 5: this->HandleReadyChanged(aMsg, inClient); break;
            case 6: this->HandleGameDissolved(inClient); break;
            case 7: this->HandleStartGame(inClient); break;
            case 8: this->HandlePingReport(aMsg, inClient); break;
            case 9: this->HandlePingChanged(aMsg, inClient); break;
            case 10: this->HandleClientKicked(aMsg, inClient); break;
            default: HT(Op::GameHandleGameMessage, Game(), inBuffer, aMsg.kind, (intptr_t)inClient); break;
            }
        } catch (ReadBufferException&) {}
    }
    // the original built a ReadBuffer and called HandleMessage itself: the host's op HandleBytes does
    void HandleMessageBuffer(const Msg& inMessage, void* inClient)
    {
        if (inMessage.mPresent)
            HM(Op::HandleBytes, inMessage, inClient);
    }
    void SendToCaptain(const Msg& inMessage)
    {
        bool aHave = Game() != NULL;
        switch (ee_won_stage_send_to_captain(aHave, this->IAmCaptain())) {
        case EeWonStageRoute::Local: this->HandleMessageBuffer(inMessage, MyClient()); break;
        case EeWonStageRoute::Net: HM(Op::NetSendToCaptain, inMessage, NULL); break;
        default: break;
        }
    }
    void BroadcastToGame(const Msg& inMessage)
    {
        bool aHave = Game() != NULL;
        if (ee_won_stage_broadcast(aHave, this->IAmCaptain()) == EeWonStageRoute::LocalAndNet) {
            this->HandleMessageBuffer(inMessage, MyClient());
            HM(Op::NetBroadcastToGame, inMessage, NULL);
        }
    }
    void SendToClient(void* inClient, const Msg& inMessage)
    {
        bool aHave = Game() != NULL;
        if (ee_won_stage_send_to_client(aHave, this->IAmCaptain()) == EeWonStageRoute::Net)
            HM(Op::NetSendToClient, inMessage, inClient);
    }
    bool IAmCaptain()
    {
        void* aGame = Game();
        return aGame != NULL && H(Op::GameIsHost, aGame) != 0;
    }
    bool IAmReady()
    {
        void* aClient = MyClient();
        return aClient != NULL && H(Op::ClientIsReady, aClient) != 0;
    }
    void HandleControlEvent(int inWhich)
    {
        switch (inWhich) {
        case 1: this->HostGame(); return;
        case 2: this->LeaveGame(true); return;
        case 3: this->Logout(true); return;
        case 4: this->ToggleReady(); return;
        }
    }
    void HandleComponentEvent(int inType, void* inEvent)
    {
        switch (inType) {
        case 100027: this->JoinGame(HP(Op::EventJoinGame, inEvent)); break;
        case 100017: this->HandleGameAction(inEvent); break;
        }
    }
    void HandleGameAction(void* inEvent)
    {
        void* aGame = HP(Op::EventActionGame, inEvent);
        if (aGame == NULL) return;
        int anAction = (int)H(Op::EventAction, inEvent);
        switch (ee_won_stage_game_action(anAction, (int)H(Op::GameType, aGame))) {
        case 7: H(Op::GameShowOptions, aGame); break;
        case 1: this->JoinGame(aGame); break;
        case 2: H(Op::GamePingAndNotify, aGame); break;
        case 3: H(Op::GameShowDetails, aGame); break;
        case 5: H(Op::GameHideDetails, aGame); break;
        case 6: H(Op::GameDirectConnect, aGame); break;
        }
    }
    bool Logout(bool inConfirm)
    {
        if (!this->LeaveGame(inConfirm)) return false;
        H(Op::StLogoutTail);
        return true;
    }
    void SendReady(bool inReady) { this->SendToCaptain(StagingMessage(4, 0, inReady, 0)); }
    void SendGameDissolved()
    {
        if (!this->IAmCaptain())
            return;
        this->BroadcastToGame(StagingMessage(6, 0, false, 0));
    }
    void SendStartGame() { this->BroadcastToGame(StagingMessage(7, 0, false, 0)); }
    void ToggleReady()
    {
        bool aHave = MyClient() != NULL && Game() != NULL;
        bool aCaptain = aHave && this->IAmCaptain();
        bool aReady = aHave && H(Op::ClientIsReady, MyClient()) != 0;
        EeWonToggleReady aToggle = ee_won_stage_toggle_ready(aHave, aHave, aCaptain, aReady);
        if (aToggle.action == 1) {
            if (H(Op::GameCanStartAndTry, Game())) this->SendStartGame();
        } else if (aToggle.action == 2) {
            if (aToggle.ask_can_ready && !H(Op::GameCanReady, Game())) return;
            this->mLastReady = aToggle.want_ready;
            this->SendReady(aToggle.want_ready);
        }
    }
    void HostGame()
    {
        if (!this->LeaveGame(true)) return;
        int aGameType = (int)H(Op::GameScreenType);
        bool aConfirm = aGameType != 0 && aGameType != 1 && H(Op::ShowConfirmations);
        bool aDone = aConfirm && H(Op::FirewallTestDone);
        int aWarning = ee_won_stage_host_warning(aGameType, aConfirm, aDone, aDone && H(Op::BehindFirewall));
        if (aWarning < 0) return;
        if (aWarning != 0) {
            intptr_t aResult = aWarning == 1
                ? H(Op::YesNo, NULL, NULL, (intptr_t)LT::ConnectWarning1)
                : H(Op::YesNo, NULL, NULL, (intptr_t)LT::ConnectWarning2);
            if (aResult != 0x186a1) return;
        }
        if (H(Op::StCreateDialogRun, NULL, NULL, aGameType) != 0x186a1) return;
        Held aGame(HP(Op::StTakeDialogGame, NULL, NULL, aGameType));
        this->mJoinStatus = 0;
        StagingLeaveOnError aCleanup(this);
        if (!H(Op::GamePreNetCreate, aGame.get())) return;
        if (!H(Op::NetHostGame, aGame.get())) return;
        void* aMe = HP(Op::StSetMyClientFromNet);
        if (aMe == NULL) return;
        H(Op::GameBecomeHost, aGame.get(), aMe);
        if (!H(Op::GamePostNetCreate, aGame.get())) return;
        H(Op::GameAddHostClient, aGame.get(), MyClient());
        aCleanup.mLeave = false;
        this->mInStaging = true;
        this->mLastReadyTime = 0;
        this->mLastAllReady = H(Op::GameAreAllReady, aGame.get()) != 0;
        this->mLastReady = this->IAmReady();
        H(Op::NetAdvertiseGame);
        H(Op::BroadcastLobbyEvent, NULL, NULL, 0x186bd);
    }
    bool VerifyPingBeforeJoin(void* inGame)
    {
        if (H(Op::GameType, inGame) != 2 || H(Op::GamePingReplied, inGame))
            return true;
        intptr_t aConfig = H(Op::AllowJoinWithoutPing);
        bool aWait = aConfig >= 0 && !(aConfig == 1);
        int aType = (int)H(Op::GameType, inGame);
        bool aReplied = H(Op::GamePingReplied, inGame) != 0;
        int aStep = ee_won_stage_verify_ping(aType, aReplied, aConfig >= 0, aConfig >= 0 && aConfig == 1,
                                             !aWait && H(Op::ShowConfirmations));
        if (aStep == 1) {
            H(Op::GamePingAndNotify, inGame);
            while (H(Op::GameIsPinging, inGame)) {
                if (H(Op::RunWaitOp, NULL, NULL, (intptr_t)LT::VerifyingPing, 2)) return false;
            }
            if (!H(Op::GamePingReplied, inGame)) {
                H(Op::MessageBox, NULL, NULL, (intptr_t)LT::NoPingReply);
                return false;
            }
        } else if (aStep == 2) {
            return H(Op::YesNo, NULL, NULL, (intptr_t)LT::ConnectWarning3) == 0x186a1;
        }
        return true;
    }
    void JoinGame(void* inGame)
    {
        if (inGame == NULL) return;
        void* aCurrent = Game();
        if (aCurrent != NULL && H(Op::GameIsSameGame, inGame, aCurrent)) {
            H(Op::MessageBox, NULL, NULL, (intptr_t)LT::AlreadyJoined);
            return;
        }
        if (!this->VerifyPingBeforeJoin(inGame)) return;
        short aStatus = (short)H(Op::GameJoinStatus, inGame);
        if (aStatus != 0) {
            if (ee_won_stage_join_status_shown(aStatus)) H(Op::GameShowStatus, inGame, NULL, aStatus);
            return;
        }
        if (!this->LeaveGame(true)) return;
        StagingLeaveOnError aCleanup(this);
        Held aGame(HP(Op::GameNewCopy, inGame));
        H(Op::StSetGame, aGame.get());
        this->mJoinStatus = 1;
        if (!H(Op::GamePreNetJoin, aGame.get())) return;
        if (!H(Op::NetJoinGame, aGame.get())) return;
        if (HP(Op::StSetMyClientFromNet) == NULL) return;
        if (!H(Op::GamePostNetJoin, aGame.get())) return;
        H(Op::StJoinOpNew);
        H(Op::StSendJoinRequest);
        H(Op::StJoinOpRun);
        if (this->mJoinStatus != 0) {
            if (ee_won_stage_join_status_shown(this->mJoinStatus)) H(Op::GameShowStatus, inGame, NULL, this->mJoinStatus);
            return;
        }
        aCleanup.mLeave = false;
        this->mInStaging = true;
        this->mLastReadyTime = 0;
        this->mLastAllReady = H(Op::GameAreAllReady, aGame.get()) != 0;
        this->mLastReady = this->IAmReady();
        H(Op::BroadcastLobbyEvent, NULL, NULL, 0x186bd);
    }
    bool LeaveGame(bool inConfirm)
    {
        if (Game() == NULL)
            return true;
        bool aShow = inConfirm && H(Op::ShowConfirmations);
        int aConfirm = ee_won_stage_leave_confirm(inConfirm, aShow, this->IAmCaptain());
        if (aConfirm != 0) {
            intptr_t aResult;
            if (aConfirm == 1) aResult = H(Op::YesNo, NULL, NULL, (intptr_t)LT::DissolveGame);
            else aResult = H(Op::YesNo, NULL, NULL, (intptr_t)LT::LeaveGame);
            if (aResult != 0x186a1) return false;
        }
        if (Game() == NULL)
            return true;
        if (this->IAmCaptain()) {
            this->SendGameDissolved();
            H(Op::NetRemoveGameAdvertisement, Game());
        }
        if (Game() == NULL)
            return true;
        H(Op::StJoinOpKill);
        Held aGame(HP(Op::GameHold, Game()));
        H(Op::StSetGame, NULL);
        H(Op::GameLeaveNet, aGame.get());
        this->mLastReadyTime = 0;
        if (this->mInStaging) {
            H(Op::BroadcastLobbyEvent, NULL, NULL, 0x186c0);
            this->mInStaging = false;
        }
        return true;
    }
    void HandleDisconnect(bool inShowError)
    {
        if (Game() != NULL) {
            this->LeaveGame(false);
            if (inShowError) H(Op::MessageBox, NULL, NULL, (intptr_t)LT::Disconnected);
        }
    }
    void HandleKicked(void* inClient, bool inBan)
    {
        void* aGame = Game();
        if (inClient == NULL || aGame == NULL) return;
        bool isMe = inClient == MyClient();
        EeWonKicked aKicked = ee_won_stage_kicked(inBan, (int)H(Op::GameType, aGame), isMe);
        H(Op::StClientModerated, inClient, NULL, aKicked.flags, (aKicked.kind << 1) | (isMe ? 1 : 0));
        if (aKicked.message == 0)
            return;
        this->LeaveGame(false);
        if (aKicked.message == 1) H(Op::MessageBox, NULL, NULL, (intptr_t)LT::Banned);
        else H(Op::MessageBox, NULL, NULL, (intptr_t)LT::Kicked);
    }
    void HandleClientLeft(void* inClient)
    {
        bool aHas = H(Op::ClientHasPlayer, inClient) != 0;
        if (!ee_won_stage_client_left(aHas, Game() != NULL))
            return;
        H(Op::ClientChanged, inClient, NULL, 1);
        void* aMe = MyClient();
        if (H(Op::ClientIsTeammate, inClient, aMe))
            H(Op::ClientTeamChanged, inClient, NULL, 1);
        H(Op::GameRemoveClient, Game(), inClient);
        if (this->IAmCaptain()) H(Op::NetAdvertiseGame);
        this->NotifyStagingChange();
    }
    void NotifyPingChange(void* inGame)
    {
        bool aHave = Game() != NULL;
        bool aCaptain = aHave && this->IAmCaptain();
        bool aSame = false;
        if (aHave && !aCaptain) { void* aCurrent = Game(); aSame = H(Op::GameIsSameGame, inGame, aCurrent) != 0; }
        if (!ee_won_stage_reports_ping(aHave, aCaptain, aSame))
            return;
        this->SendToCaptain(StagingMessage(8, 0, false, (unsigned short)H(Op::GamePing, inGame)));
    }
    void KickClient(void* inClient, bool inBan)
    {
        bool aHave = Game() != NULL;
        if (!ee_won_stage_kicks(inClient != NULL, aHave, this->IAmCaptain()))
            return;
        this->BroadcastToGame(StagingMessage(10, (unsigned short)H(Op::ClientId, inClient), inBan, 0));
        H(Op::NetKickClient, NULL, inClient, inBan);
    }
    void NotifyStagingChange() { H(Op::BroadcastLobbyEvent, NULL, NULL, 0x186dd); }
    void UpdateReadyState()
    {
        this->NotifyStagingChange();
        void* aGame = Game();
        if (aGame == NULL) return;
        EeWonReadyState aState;
        aState.all_ready = H(Op::GameAreAllReady, aGame) != 0;
        aState.last_all_ready = this->mLastAllReady;
        aState.is_host = H(Op::GameIsHost, aGame) != 0;
        aState.last_ready = this->mLastReady;
        aState.i_am_ready = this->IAmReady();
        aState.now = (int64_t)H(Op::Now);
        aState.last_ready_time = this->mLastReadyTime;
        ee_won_room_ready_state(&aState);
        this->mLastAllReady = aState.all_ready;
        this->mLastReady = aState.last_ready;
        this->mLastReadyTime = aState.last_ready_time;
        if (aState.sound != EeWonReadySound::Silent && H(Op::StagingSoundEffects))
            H(Op::PlaySound, NULL, NULL, aState.sound == EeWonReadySound::AllReady
                ? (intptr_t)EeWonLobbySound::AllReady : (intptr_t)EeWonLobbySound::AllNotReady);
    }
};

StagingLeaveOnError::~StagingLeaveOnError() { if (this->mLeave) this->mLogic->LeaveGame(false); }

// ---- the sequences

struct Call {
    EeWonStagingCall mCall;
    void* mA;
    void* mB;
    intptr_t mC;
    Msg mMsg; // the message calls' and HandleMessage's bytes
};

static unsigned sCallSeed;
static unsigned C(unsigned n) { return Rand(sCallSeed) % n; }
static void* Client() { unsigned r = C(6); return r == 0 ? NULL : P(0x2001 + (r & 1)); }
static void* AGame() { unsigned r = C(6); return r == 0 ? NULL : P(0x1001 + (r & 1)); }

static Msg RoomBytes()
{
    EeWonRoomMsg aMsg;
    memset(&aMsg, 0, sizeof(aMsg));
    unsigned k = C(14);
    aMsg.kind = (unsigned char)(k < 12 ? k + 1 : 0x64 + C(8));
    aMsg.id = (unsigned short)(C(4) == 0 ? 0x2001 : C(3));
    aMsg.flag = C(2) != 0;
    aMsg.ping = (unsigned short)(C(3) * 1000);
    Msg aResult = {P(0x3000), std::string(), true};
    ee_won_room_message(&aMsg, AppendBytes, &aResult.mBytes);
    if (C(3) == 0) aResult.mBytes += "tail";
    if (C(5) == 0) aResult.mBytes.resize(C((unsigned)aResult.mBytes.length() + 1));
    return aResult;
}

static Call NextCall()
{
    Call c = {EeWonStagingCall::HandleMessage, NULL, NULL, 0, {NULL, std::string(), false}};
    unsigned k = C(21);
    switch (k) {
    case 0: c.mCall = EeWonStagingCall::Control; c.mC = C(6); break;
    case 1: c.mCall = EeWonStagingCall::ComponentEvent; c.mC = C(3) == 0 ? 100001 : C(2) ? 100027 : 100017;
            c.mA = P(0x4001); break;
    case 2: case 3: case 4: case 5: c.mCall = EeWonStagingCall::HandleMessage; c.mB = Client(); c.mMsg = RoomBytes(); break;
    case 6: case 7: case 8: {
        c.mCall = k == 6 ? EeWonStagingCall::SendToCaptain : k == 7 ? EeWonStagingCall::BroadcastToGame
                         : EeWonStagingCall::SendToClient;
        if (k == 8) c.mB = Client();
        if (C(4) != 0) { c.mMsg = RoomBytes(); c.mMsg.mBuffer = P(0x3100); }
        break;
    }
    case 9: c.mCall = EeWonStagingCall::IAmCaptain; break;
    case 10: c.mCall = EeWonStagingCall::IAmReady; break;
    case 11: c.mCall = EeWonStagingCall::JoinGame; c.mA = AGame(); break;
    case 12: c.mCall = EeWonStagingCall::HostGame; break;
    case 13: c.mCall = EeWonStagingCall::Logout; c.mC = C(2); break;
    case 14: c.mCall = C(2) ? EeWonStagingCall::KickClient : EeWonStagingCall::HandleKicked; c.mB = Client(); c.mC = C(2); break;
    case 15: c.mCall = EeWonStagingCall::HandleDisconnect; c.mC = C(2); break;
    case 16: c.mCall = EeWonStagingCall::HandleClientLeft; c.mB = P(0x2001 + C(2)); break;
    case 17: c.mCall = C(2) ? EeWonStagingCall::ResetReadyFlags : EeWonStagingCall::NotifyStagingChange; break;
    case 18: c.mCall = EeWonStagingCall::SetGameInProgress; c.mC = C(2); break;
    case 19: c.mCall = EeWonStagingCall::LeaveGame; c.mC = C(2); break;
    default: c.mCall = EeWonStagingCall::NotifyPingChange; c.mA = P(0x1001 + C(2)); break;
    }
    return c;
}

static bool IsMessageCall(EeWonStagingCall inCall)
{
    return inCall == EeWonStagingCall::HandleMessage || inCall == EeWonStagingCall::SendToCaptain ||
           inCall == EeWonStagingCall::BroadcastToGame || inCall == EeWonStagingCall::SendToClient;
}

// the C ABI's arguments: a message is its ByteBuffer (HandleMessage: the ReadBuffer), its bytes and their length
static void Args(const Call& c, void*& outA, intptr_t& outC, intptr_t& outD)
{
    outA = c.mA;
    outC = c.mC;
    outD = 0;
    if (IsMessageCall(c.mCall)) {
        outA = c.mMsg.mBuffer;
        outC = c.mMsg.mPresent ? (intptr_t)c.mMsg.mBytes.data() : 0;
        outD = (intptr_t)c.mMsg.mBytes.length();
    }
}

static std::string CallLine(const Call& c)
{
    void* aA;
    intptr_t aC, aD;
    Args(c, aA, aC, aD);
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "== %u %llx %llx %lld m:", (unsigned)c.mCall, (unsigned long long)(uintptr_t)aA,
             (unsigned long long)(uintptr_t)c.mB, (long long)(IsMessageCall(c.mCall) ? 0 : aC));
    return std::string(aLine) + (c.mMsg.mPresent ? Hex(c.mMsg.mBytes.data(), (intptr_t)c.mMsg.mBytes.length()) : "-");
}

static intptr_t RunOrig(Orig& o, const Call& c)
{
    switch (c.mCall) {
    case EeWonStagingCall::Control: o.HandleControlEvent((int)c.mC); break;
    case EeWonStagingCall::ComponentEvent: o.HandleComponentEvent((int)c.mC, c.mA); break;
    case EeWonStagingCall::HandleMessage: o.HandleMessage(c.mMsg.mBuffer, c.mMsg.mBytes, c.mB); break;
    case EeWonStagingCall::SendToCaptain: o.SendToCaptain(c.mMsg); break;
    case EeWonStagingCall::BroadcastToGame: o.BroadcastToGame(c.mMsg); break;
    case EeWonStagingCall::SendToClient: o.SendToClient(c.mB, c.mMsg); break;
    case EeWonStagingCall::IAmCaptain: return o.IAmCaptain();
    case EeWonStagingCall::IAmReady: return o.IAmReady();
    case EeWonStagingCall::JoinGame: o.JoinGame(c.mA); break;
    case EeWonStagingCall::HostGame: o.HostGame(); break;
    case EeWonStagingCall::Logout: return o.Logout(c.mC != 0);
    case EeWonStagingCall::KickClient: o.KickClient(c.mB, c.mC != 0); break;
    case EeWonStagingCall::HandleDisconnect: o.HandleDisconnect(c.mC != 0); break;
    case EeWonStagingCall::HandleClientLeft: o.HandleClientLeft(c.mB); break;
    case EeWonStagingCall::HandleKicked: o.HandleKicked(c.mB, c.mC != 0); break;
    case EeWonStagingCall::ResetReadyFlags: o.ResetReadyFlags(); break;
    case EeWonStagingCall::SetGameInProgress: o.SetGameInProgress(c.mC != 0); break;
    case EeWonStagingCall::LeaveGame: return o.LeaveGame(c.mC != 0);
    case EeWonStagingCall::NotifyStagingChange: o.NotifyStagingChange(); break;
    case EeWonStagingCall::NotifyPingChange: o.NotifyPingChange(c.mA); break;
    }
    return 0;
}

static intptr_t RunRust(EeWonStaging* r, const Call& c)
{
    void* aA;
    intptr_t aC, aD;
    Args(c, aA, aC, aD);
    return ee_won_staging_call(r, c.mCall, aA, c.mB, aC, aD);
}

static std::vector<std::string> Sequence(int inSeq, bool inRust)
{
    sTrace.clear();
    sAnswerSeed = 0x9e37u + (unsigned)inSeq * 7919u;
    sCallSeed = 0x51u + (unsigned)inSeq * 104729u;
    sGameTypeBase = (unsigned)inSeq % 5;
    Orig o;
    EeWonStaging* r = ee_won_staging_new(P(0x51), ScriptedHost);
    for (int i = 0; i < 6; i++) {
        Call c = NextCall();
        sTrace.push_back(CallLine(c));
        intptr_t aResult = inRust ? RunRust(r, c) : RunOrig(o, c);
        sTrace.push_back("=> " + std::to_string((long long)aResult));
    }
    ee_won_staging_free(r);
    return sTrace;
}

#include <set>
static std::set<std::string> sSeen;

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000, kGolden = 100;
    size_t aLines = 0;
    for (int s = 0; s < kSequences; s++) {
        std::vector<std::string> aWant = Sequence(s, false);
        std::vector<std::string> aGot = Sequence(s, true);
        aLines += aWant.size();
        CHECK(aWant.size() == aGot.size());
        for (size_t i = 0; i < aWant.size() && i < aGot.size(); i++) {
            sChecks++;
            if (aWant[i] != aGot[i]) {
                sFailures++;
                if (sFailures < 20) printf("FAIL sequence %d line %u: want %s got %s\n", s, (unsigned)i, aWant[i].c_str(), aGot[i].c_str());
                break;
            }
        }
        // the golden: the first sequences, and every later one that makes an op or a call none before made
        bool aNew = false;
        for (size_t i = 0; i < aWant.size(); i++) {
            const std::string& aLine = aWant[i];
            std::string aKey = aLine.substr(0, aLine.find(' ', aLine.compare(0, 2, "==") == 0 ? 3 : 0));
            if (aKey.compare(0, 2, "=>") != 0 && sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && (s < kGolden || aNew)) {
            printf("## %d\n", s);
            for (size_t i = 0; i < aWant.size(); i++) printf("%s\n", aWant[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_stagingobj_test: %d sequences, %u trace lines, %u distinct ops and calls, %d checks, %d failures\n",
               kSequences, (unsigned)aLines, (unsigned)sSeen.size(), sChecks, sFailures);
    return sFailures;
}
