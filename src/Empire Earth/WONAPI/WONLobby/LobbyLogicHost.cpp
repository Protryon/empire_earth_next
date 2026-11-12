// The lobby logic objects' C++ host (NEW, no original address: units.EXTRA_UNITS): the shells of the logic modules
// that are Rust objects (StagingLogic: rust/crates/ee_won/src/stagingobj.rs) and ee_won_lobbycpp_call, the one
// table of their calls out (lobbyhost.rs EeWonLobbyOp): each op is a run of the original's C++ statements with no
// decision between them. It replaces StagingLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <time.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"
namespace WONAPI {
void LobbyNetSendToCaptain(const ByteBuffer*);
void LobbyNetBroadcastToGame(const ByteBuffer*);
void LobbyNetSendToClient(LobbyClient*, const ByteBuffer*);
void LobbyNetKickClient(LobbyClient*, bool);
void LobbyNetSetGameInProgress(bool);
void LobbyMessageBox(const GUIString&, const GUIString&);
void LobbyNetRemoveGameAdvertisement(LobbyGame*);
void LobbyNetLeaveGame(LobbyGame*);
}
using namespace WONAPI;

extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD);

// ---- StagingLogic's shell

// FUNCTION: WON 0x007ef160
StagingLogic::StagingLogic() : mRust(ee_won_staging_new(this, ee_won_lobbycpp_call)) {}

// FUNCTION: WON 0x007ef1b0
StagingLogic::~StagingLogic() { ee_won_staging_free(this->mRust); }

// FUNCTION: WON 0x007ef290
// Looks up the create-game dialog from the lobby layout
void StagingLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mCreateGameDialog, "CreateGameDialog", "StagingLogic.cpp", 41);
}

// FUNCTION: WON 0x007efe50
// The host, leave, logout and start/ready buttons
void StagingLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_HostGame ? 1 : inControlId == ID_LeaveGame ? 2 : inControlId == ID_Logout ? 3
               : inControlId == ID_StartGame ? 4 : 0;
    if (aWhich != 0) this->Call(EeWonStagingCall::Control, NULL, NULL, aWhich);
}

// FUNCTION: WON 0x007efeb0
void StagingLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->Call(EeWonStagingCall::ComponentEvent, inEvent, NULL, inEvent->mType);
}

void StagingLogic::CallMessage(EeWonStagingCall inCall, const ByteBuffer* inMessage, LobbyClient* inClient)
{
    ee_won_staging_call(this->mRust, inCall, const_cast<ByteBuffer*>(inMessage), inClient,
                        inMessage != NULL ? (intptr_t)inMessage->data() : 0,
                        inMessage != NULL ? (intptr_t)inMessage->length() : 0);
}

// FUNCTION: WON 0x007efb50
// The room message from the buffer's position (ee_won reads its type and fixed fields)
void StagingLogic::HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    long aStart = inBuffer.pos();
    ee_won_staging_call(this->mRust, EeWonStagingCall::HandleMessage, &inBuffer, inClient,
                        (intptr_t)(reinterpret_cast<const uint8_t*>(inBuffer.data()) + aStart),
                        (intptr_t)(inBuffer.length() - aStart));
}

// FUNCTION: WON 0x007efd20
void StagingLogic::HandleMessageBuffer(const ByteBuffer* inMessage, LobbyClient* inClient)
{
    if (inMessage != NULL) {
        ReadBuffer aBuffer(inMessage->data(), inMessage->length());
        this->HandleMessage(aBuffer, inClient);
    }
}

// ---- the calls out

static const GUIString* const kTexts[][2] = {
    {&::RoutingLogic_GameDissolvedTitle_String, &::RoutingLogic_GameDissolvedDetails_String},
    {&::RoutingLogic_AlreadyJoinedTitle_String, &::RoutingLogic_AlreadyInGame_String},
    {&::GameStagingLogic_JoinGameErrorTitle_String, &::GameStagingLogic_NoPingReplyError_String},
    {&::StagingLogic_DisconnectedTitle_String, &::StagingLogic_DisconnectedDetails_String},
    {&::StagingLogic_BannedTitle_String, &::StagingLogic_BannedDetails_String},
    {&::StagingLogic_KickedTitle_String, &::StagingLogic_KickedDetails_String},
    {&::GameStagingLogic_ConnectWarningTitle1_String, &::GameStagingLogic_ConnectWarningDetails1_String},
    {&::GameStagingLogic_ConnectWarningTitle2_String, &::GameStagingLogic_ConnectWarningDetails2_String},
    {&::GameStagingLogic_ConnectWarningTitle3_String, &::GameStagingLogic_ConnectWarningDetails3_String},
    {&::GameStagingLogic_DissolveGame_String, &::GameStagingLogic_DissolveGameConfirm_String},
    {&::GameStagingLogic_LeaveGame_String, &::GameStagingLogic_LeaveGameConfirm_String},
    {&::GameStagingLogic_VerifyingPingTitle_String, &::GameStagingLogic_VerifyingPingDetails_String},
    {&::GameStagingLogic_GettingGameInfoTitle_String, &::GameStagingLogic_GettingGameInfoDetails_String},
};

static void AppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

// a message op's message: the C++ ByteBuffer, or one over the bytes Rust built, or none
static const ByteBuffer* Message(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

static intptr_t Held(LobbyGame* inGame)
{
    if (inGame != NULL) inGame->AddRef();
    return (intptr_t)inGame;
}

extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD)
{
    StagingLogic* st = static_cast<StagingLogic*>(static_cast<LobbyLogic*>(inLogic));
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyClient* clB = static_cast<LobbyClient*>(inB);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    const GUIString* const* aText = inC >= 0 && inC < (intptr_t)(sizeof(kTexts) / sizeof(kTexts[0])) ? kTexts[inC] : kTexts[0];
    ByteBufferPtr aHold;
    switch (inOp) {
    // StagingLogic's members
    case EeWonLobbyOp::StGame: return (intptr_t)st->mGame.get();
    case EeWonLobbyOp::StMyClient: return (intptr_t)st->mMyClient.get();
    case EeWonLobbyOp::StSetMyClientFromNet: st->mMyClient = LobbyGetNetMyClient(); return (intptr_t)st->mMyClient.get();
    case EeWonLobbyOp::StSetGame: st->mGame = g; return 0;
    case EeWonLobbyOp::StJoinOpNew: st->mJoinOp = new AsyncOp; return 0;
    case EeWonLobbyOp::StJoinOpFinish: if (st->mJoinOp.get() != NULL) st->mJoinOp->Finish(WS_Success); return 0;
    case EeWonLobbyOp::StJoinOpRun:
        aText = kTexts[(int)EeWonLobbyText::GettingGameInfo];
        LobbyRunOpWithStatus(st->mJoinOp.get(), *aText[0], *aText[1], 1, 0);
        st->mJoinOp = NULL;
        return 0;
    case EeWonLobbyOp::StJoinOpKill:
        if (st->mJoinOp.get() != NULL) { st->mJoinOp->Kill(true); st->mJoinOp = NULL; }
        return 0;
    case EeWonLobbyOp::StSendJoinRequest: {
        ConstSmartPtr<ByteBuffer> aMessage = st->mGame->GetJoinRequest();
        st->SendToCaptain(aMessage.get());
        return 0;
    }
    case EeWonLobbyOp::StCreateDialogRun:
        st->mCreateGameDialog->SetGameType((int)inC);
        st->mCreateGameDialog->Reset(false);
        return LobbyDoDialog(st->mCreateGameDialog.get());
    case EeWonLobbyOp::StTakeDialogGame: {
        SmartPtr<LobbyGame> aGame = st->mCreateGameDialog->GetGame();
        aGame->SetAddress(LobbyGetMyAddress((int)inC));
        st->mGame = aGame;
        return Held(aGame.get());
    }
    case EeWonLobbyOp::StLogoutTail:
        st->mCreateGameDialog->Reset(true);
        LobbyNetLogout();
        LobbyKillPings(0);
        LobbyShowLoginScreen();
        LobbySaveGlobalPrefs(false);
        LobbySaveUserPrefs(false);
        ::LobbyGlobal_SierraLogon_Sound->Stop();
        return 0;
    case EeWonLobbyOp::StClientModerated:
        LobbyBroadcastEvent(new ClientModeratedEvent(cl, (int)inC, st->mGame->GetName(), (int)(inD >> 1), 0,
                                                     (inD & 1) != 0, gEmptyGUIString), true);
        return 0;
    // a game
    case EeWonLobbyOp::GameHold: return Held(g);
    case EeWonLobbyOp::GameRelease: g->Release(); return 0;
    case EeWonLobbyOp::GameIsHost: return g->GetIsHost();
    case EeWonLobbyOp::GameFindClient: return (intptr_t)g->FindClient((unsigned short)inC);
    case EeWonLobbyOp::GameType: return g->GetGameType();
    case EeWonLobbyOp::GameIsInProgress: return g->IsInProgress();
    case EeWonLobbyOp::GameAreAllReady: return g->AreAllReady();
    case EeWonLobbyOp::GameCanReady: return g->CanReady();
    case EeWonLobbyOp::GameCanStartAndTry: return g->CanStartGame() && g->TryStartGame();
    case EeWonLobbyOp::GamePingReplied: return g->GetPingReplied();
    case EeWonLobbyOp::GameIsPinging: return g->IsPinging();
    case EeWonLobbyOp::GameIsSameGame: return g->IsSameGame(static_cast<LobbyGame*>(inB));
    case EeWonLobbyOp::GameJoinStatus: return g->GetJoinStatus();
    case EeWonLobbyOp::GamePing: return g->GetPing();
    case EeWonLobbyOp::GameNewCopy: {
        SmartPtr<LobbyGame> aGame = LobbyGame::CreateGame(static_cast<LobbyGameType>(g->GetGameType()));
        aGame->CopyFrom(g);
        return Held(aGame.get());
    }
    case EeWonLobbyOp::GamePreNetCreate: return g->PreNetCreateGame();
    case EeWonLobbyOp::GamePostNetCreate: return g->PostNetCreateGame();
    case EeWonLobbyOp::GamePreNetJoin: return g->PreNetJoinGame();
    case EeWonLobbyOp::GamePostNetJoin: return g->PostNetJoinGame();
    case EeWonLobbyOp::GameBecomeHost:
        g->SetIsHost(true);
        g->SetCaptainId(clB->GetClientId());
        clB->SetCaptain(true, true);
        return 0;
    case EeWonLobbyOp::GameAddHostClient: g->GetClients()->AddClient(clB); g->HandleHostGame(clB); return 0;
    case EeWonLobbyOp::GameLeaveNet:
        g->ClearPlayers();
        st->mMyClient = NULL;
        g->PreNetLeaveGame();
        LobbyNetLeaveGame(g);
        g->PostNetLeaveGame();
        return 0;
    case EeWonLobbyOp::GameRemoveClient: g->RemoveClient(clB); return 0;
    case EeWonLobbyOp::GameHandlePlayerJoined: g->HandlePlayerJoined(clB); return 0;
    case EeWonLobbyOp::GameSetInProgressNet:
        g->SetInProgress(inC != 0);
        LobbyNetSetGameInProgress(inC != 0);
        LobbyNetAdvertiseGame();
        return 0;
    case EeWonLobbyOp::GameResetReadyFlags: {
        LobbyClientSetIterator anItr(g->GetClients().get());
        while (anItr.HasMore()) {
            LobbyClient* aClient = NULL;
            LobbyPlayer* aPlayer = anItr.Next(&aClient);
            if (aPlayer != NULL && aClient != NULL) {
                aPlayer->SetReady(false);
                LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
            }
        }
        return 0;
    }
    case EeWonLobbyOp::GameJoinReplySend:
        try {
            LobbyClient* aClient = reinterpret_cast<LobbyClient*>(inD);
            ConstSmartPtr<ByteBuffer> aReply = g->GetJoinReply(*buf, aClient);
            if (aReply.get() == NULL) return 0;
            st->SendToClient(aClient, aReply.get());
            return 1;
        } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    case EeWonLobbyOp::GameHandleJoinReply:
        try { return g->HandleJoinReply(*buf); } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    case EeWonLobbyOp::GameHandleGameMessage:
        try { g->HandleGameMessage((unsigned char)inC, *buf, reinterpret_cast<LobbyClient*>(inD)); return 0; }
        catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    case EeWonLobbyOp::GameShowStatus:
        LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, g->GetStatusString((short)inC));
        return 0;
    case EeWonLobbyOp::GamePingAndNotify:
        LobbyPingGame(g, true);
        LobbyBroadcastEvent(new GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::GameShowDetails:
        LobbyQueryGameDetails(g);
        g->SetShowDetails(true);
        LobbyBroadcastEvent(new GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::GameHideDetails:
        g->SetShowDetails(false);
        LobbyBroadcastEvent(new GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::GameDirectConnect: LobbyDispatchEvent(new ChangeDirectConnectEvent(1, g)); return 0;
    case EeWonLobbyOp::GameShowOptions: {
        SmartPtr<GameOptionsPopup> aPopup = new GameOptionsPopup;
        aPopup->Init(g);
        LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
        return 0;
    }
    // a client
    case EeWonLobbyOp::ClientHasPlayer: return cl->HasPlayer();
    case EeWonLobbyOp::ClientPlayer: return (intptr_t)cl->GetPlayer().get();
    case EeWonLobbyOp::ClientIsReady: return cl->IsReady();
    case EeWonLobbyOp::ClientId: return cl->GetClientId();
    case EeWonLobbyOp::ClientIsGameCaptain: return cl->IsCaptain(1);
    case EeWonLobbyOp::ClientIsTeammate: return cl->IsTeammate(clB);
    case EeWonLobbyOp::ClientPlayerSetReady: cl->GetPlayer()->SetReady(inC != 0); return 0;
    case EeWonLobbyOp::ClientPlayerSetPing: cl->GetPlayer()->SetPing((unsigned short)inC); return 0;
    case EeWonLobbyOp::ClientReadPlayer:
        try {
            SmartPtr<LobbyPlayer> aPlayer = LobbyPlayer::CreatePlayer();
            if (!aPlayer->ReadFromBuffer(*buf)) return 0;
            cl->SetPlayer(aPlayer.get());
            return 1;
        } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    case EeWonLobbyOp::ClientBroadcastJoined: {
        EeWonRoomMsg aMsg;
        memset(&aMsg, 0, sizeof(aMsg));
        aMsg.kind = 3;
        aMsg.id = cl->GetClientId();
        std::string aHead;
        ee_won_room_message(&aMsg, AppendBytes, &aHead);
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendBytes(aHead.data(), (unsigned long)aHead.length());
        cl->GetPlayer()->WriteToBuffer(aBuffer);
        st->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
        return 0;
    }
    case EeWonLobbyOp::ClientChanged: LobbyBroadcastEvent(new PlayerChangedEvent(cl, (int)inC), true); return 0;
    case EeWonLobbyOp::ClientTeamChanged: LobbyBroadcastEvent(new PlayerTeamChangedEvent(cl, (int)inC), true); return 0;
    // a message, a read buffer, events
    case EeWonLobbyOp::BufSkip: buf->SetPos((int)(buf->pos() + inC)); return 0;
    case EeWonLobbyOp::HandleBytes: {
        ReadBuffer aBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD);
        st->HandleMessage(aBuffer, clB);
        return 0;
    }
    case EeWonLobbyOp::NetSendToCaptain: LobbyNetSendToCaptain(Message(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetBroadcastToGame: LobbyNetBroadcastToGame(Message(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetSendToClient: LobbyNetSendToClient(clB, Message(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetKickClient: LobbyNetKickClient(clB, inC != 0); return 0;
    case EeWonLobbyOp::NetRemoveGameAdvertisement: LobbyNetRemoveGameAdvertisement(g); return 0;
    case EeWonLobbyOp::NetHostGame: return LobbyNetHostGame(g);
    case EeWonLobbyOp::NetJoinGame: return LobbyNetJoinGame(g);
    case EeWonLobbyOp::NetAdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    case EeWonLobbyOp::EventJoinGame: return (intptr_t)static_cast<JoinGameEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventActionGame: return (intptr_t)static_cast<GameActionEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventAction: return static_cast<GameActionEvent*>(inA)->mAction;
    // the lobby
    case EeWonLobbyOp::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)inC, true); return 0;
    case EeWonLobbyOp::MessageBox: LobbyMessageBox(*aText[0], *aText[1]); return 0;
    case EeWonLobbyOp::YesNo: return LobbyDoYesNoDialog(*aText[0], *aText[1], true);
    case EeWonLobbyOp::RunWaitOp: {
        SmartPtr<AsyncOp> anOp = new AsyncOp;
        LobbyRunOpWithStatus(anOp.get(), *aText[0], *aText[1], (int)inD, 500);
        return anOp->IsKilled();
    }
    case EeWonLobbyOp::PlaySound:
        LobbyPlaySound(inC == (intptr_t)EeWonLobbySound::PlayerJoined ? ::LobbyGlobal_PlayerJoined_Sound.get()
                       : inC == (intptr_t)EeWonLobbySound::AllReady ? ::LobbyGlobal_AllReady_Sound.get()
                       : ::LobbyGlobal_AllNotReady_Sound.get());
        return 0;
    case EeWonLobbyOp::StagingSoundEffects: return LobbyGetStagingSoundEffects();
    case EeWonLobbyOp::ShowConfirmations: return LobbyGetShowConfirmations();
    case EeWonLobbyOp::FirewallTestDone: return LobbyIsFirewallTestDone();
    case EeWonLobbyOp::BehindFirewall: return LobbyIsBehindFirewall();
    case EeWonLobbyOp::GameScreenType: return LobbyGetGameScreenType();
    case EeWonLobbyOp::AllowJoinWithoutPing: {
        LobbyConfig* aConfig = GetLobbyConfig();
        return aConfig == NULL ? -1 : aConfig->mAllowJoinGameWithoutPing;
    }
    case EeWonLobbyOp::Exit: LobbyExit((int)inC); return 0;
    case EeWonLobbyOp::Now: return (intptr_t)time(NULL);
    }
    return 0;
}
