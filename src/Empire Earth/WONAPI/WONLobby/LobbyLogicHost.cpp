// The lobby logic objects' C++ host (NEW, no original address: units.EXTRA_UNITS): the shells of the logic modules
// that are Rust objects (StagingLogic: rust/crates/ee_won_lobby/src/stagingobj.rs) and ee_won_lobbycpp_call, the one
// table of their calls out (lobbyhost.rs EeWonLobbyOp): each op is a run of the original's C++ statements with no
// decision between them (LanLogic's in LanLogicHost.cpp). It replaces StagingLogic.cpp.
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
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/stagingobj.h"
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

intptr_t LanLogicHostCall(LanLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD);
extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD);

// ---- StagingLogic's shell

StagingLogic::StagingLogic() : mRust(ee_won_staging_new_model(this, ee_won_lobbycpp_call)) {}

StagingLogic::~StagingLogic() { ee_won_staging_free(this->mRust); }

// Looks up the create-game dialog from the lobby layout
void StagingLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetDialog(inConfig, this->mCreateGameDialog, EeWonLobbyDlgKind::CreateGame, "CreateGameDialog", "StagingLogic.cpp", 41);
}

// The host, leave, logout and start/ready buttons
void StagingLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_HostGame ? 1 : inControlId == ID_LeaveGame ? 2 : inControlId == ID_Logout ? 3
               : inControlId == ID_StartGame ? 4 : 0;
    if (aWhich != 0) this->Call(EeWonStagingCall::Control, NULL, NULL, aWhich);
}

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

// The room message from the buffer's position (ee_won reads its type and fixed fields)
void StagingLogic::HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    ee_won_staging_call(this->mRust, EeWonStagingCall::HandleMessage, &inBuffer, inClient, (intptr_t)(reinterpret_cast<const uint8_t*>(inBuffer.data()) + inBuffer.pos()), (intptr_t)(inBuffer.length() - inBuffer.pos()));
}

void StagingLogic::HandleMessageBuffer(const ByteBuffer* inMessage, LobbyClient* inClient)
{
    if (inMessage != NULL) {
        ReadBuffer aBuffer(inMessage->data(), inMessage->length());
        this->HandleMessage(aBuffer, inClient);
    }
}

// ---- the calls out


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
    // the C++ object the Rust one belongs to (a StagingLogic for the St ops; a LanLogic's ops go to LanLogicHost.cpp)
    StagingLogic* st = static_cast<StagingLogic*>(inLogic);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyClient* clB = static_cast<LobbyClient*>(inB);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    // a box's or status wait's texts: the title's and the details' resource rows (lobbyhost.rs `lobby_text`)
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    ByteBufferPtr aHold;
    switch (inOp) {
    // StagingLogic's members
    case EeWonLobbyOp::StGame: return (intptr_t)st->mGame.get();
    case EeWonLobbyOp::StMyClient: return (intptr_t)st->mMyClient.get();
    case EeWonLobbyOp::StSetMyClientFromNet: st->mMyClient = LobbyGetNetMyClient(); return (intptr_t)st->mMyClient.get();
    case EeWonLobbyOp::StSetGame: st->mGame = g; return 0;
    case EeWonLobbyOp::StJoinOpNew: st->mJoinOp = new AsyncOp; return 0;
    case EeWonLobbyOp::StSendJoinRequest: st->SendToCaptain(st->mGame->GetJoinRequest().get()); return 0;
    // StCreateDialogRun, StTakeDialogGame and GameNewCopy are lobbyops.rs's over these (R6 WON round 32)
    case EeWonLobbyOp::StCreateSetType: st->mCreateGameDialog->LCall(EeWonLobbyDlgCall::SetGameType, (int)inC); return 0;
    case EeWonLobbyOp::StCreateReset: st->mCreateGameDialog->LCall(EeWonLobbyDlgCall::Reset, false); return 0;
    case EeWonLobbyOp::StCreateDo: return LobbyDoDialog(st->mCreateGameDialog.get());
    case EeWonLobbyOp::StDialogGameHeld: return Held(st->mCreateGameDialog->GetGame().get());
    case EeWonLobbyOp::GameSetMyAddress: g->SetAddress(LobbyGetMyAddress((int)inC)); return 0;
    case EeWonLobbyOp::GameCreateHeld: return Held(LobbyGame::CreateGame(static_cast<LobbyGameType>(g->GetGameType())));
    case EeWonLobbyOp::GameCopyFrom: g->CopyFrom(static_cast<LobbyGame*>(inB)); return 0;
    case EeWonLobbyOp::StClientModerated: LobbyBroadcastEvent(ClientModeratedEvent(cl, (int)inC, st->mGame->GetName(), (int)(inD >> 1), 0, (inD & 1) != 0, gEmptyGUIString), true); return 0;
    // a game
    case EeWonLobbyOp::GameHold: return Held(g);
    case EeWonLobbyOp::GameRelease: g->Release(); return 0;
    case EeWonLobbyOp::GameCanReady: return g->CanReady();
    case EeWonLobbyOp::GameCanStartAndTry: return g->CanStartGame() && g->TryStartGame();
    case EeWonLobbyOp::GamePreNetCreate: return g->PreNetCreateGame();
    case EeWonLobbyOp::GamePostNetCreate: return g->PostNetCreateGame();
    case EeWonLobbyOp::GamePreNetJoin: return g->PreNetJoinGame();
    case EeWonLobbyOp::GamePostNetJoin: return g->PostNetJoinGame();
    case EeWonLobbyOp::GameHandlePlayerJoined: g->HandlePlayerJoined(clB); return 0;
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
        LobbyMessageBox(LOBBY_STRING(GameStagingLogic_JoinGameErrorTitle_String), g->GetStatusString((short)inC));
        return 0;
    case EeWonLobbyOp::GameDirectConnect: LobbyDispatchEvent(ChangeDirectConnectEvent(1, g)); return 0;
    case EeWonLobbyOp::GameShowOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Game, (intptr_t)g);
        return 0;
    }
    // a client
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
    case EeWonLobbyOp::ClientChanged: LobbyBroadcastEvent(PlayerChangedEvent(cl, (int)inC), true); return 0;
    case EeWonLobbyOp::ClientTeamChanged: LobbyBroadcastEvent(PlayerTeamChangedEvent(cl, (int)inC), true); return 0;
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
    // the lobby
    case EeWonLobbyOp::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)inC, true); return 0;
    case EeWonLobbyOp::MessageBox: LobbyMessageBox(aTitle, aDetails); return 0;
    case EeWonLobbyOp::YesNo: return LobbyDoYesNoDialog(aTitle, aDetails, true);
    case EeWonLobbyOp::RunWaitOp: {
        SmartPtr<AsyncOp> anOp = new AsyncOp;
        LobbyRunOpWithStatus(anOp.get(), aTitle, aDetails, (int)inD, 500);
        return anOp->IsKilled();
    }
    case EeWonLobbyOp::PlaySound: LobbyPlaySound(gLobbyResources[inC].mSound.get()); return 0; // a resource row
    case EeWonLobbyOp::FirewallTestDone: return LobbyIsFirewallTestDone();
    case EeWonLobbyOp::BehindFirewall: return LobbyIsBehindFirewall();
    case EeWonLobbyOp::GameScreenType: return LobbyGetGameScreenType();
    // the statements of the cases lobbyops.rs answers (R6 WONLobby round 29: the join op, the logout, leaving a game,
    // the game's progress and details, the join without a ping)
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
    case EeWonLobbyOp::Exit: LobbyExit((int)inC); return 0;
    case EeWonLobbyOp::Now: return (intptr_t)time(NULL);
    default: return LanLogicHostCall(static_cast<LanLogic*>(inLogic), inOp, inA, inB, inC, inD);
    }
}
