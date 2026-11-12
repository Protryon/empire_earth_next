// The lobby's game staging: hosting, joining and leaving games, ready flags, pings, kicks and the start message
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
// The room messages' bytes, their reads, who acts on which and the ready state's sounds are ee_won's staging module
// (rust/crates/ee_won/src/staging.rs), the flow's decisions (where a message goes, what hosting, joining, leaving,
// readying, a kick or a game action does) its flow module (flow.rs); this class keeps the game, the clients, the
// join op, the dialogs, the events and sounds.
namespace WONAPI {
void LobbyNetSendToCaptain(const ByteBuffer*);
void LobbyNetBroadcastToGame(const ByteBuffer*);
void LobbyNetSendToClient(LobbyClient*, const ByteBuffer*);
void LobbyNetKickClient(LobbyClient*, bool);
void LobbyNetSetGameInProgress(bool);
void LobbyMessageBox(const GUIString&, const GUIString&);
bool LobbyIAmReady();
class Sound;
void LobbyNetRemoveGameAdvertisement(LobbyGame*);
void LobbyNetLeaveGame(LobbyGame*);

static void StagingAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
// a room message (ee_won_room_message) as the ByteBuffer the game is sent
static ByteBufferPtr StagingMessage(unsigned char inKind, unsigned short inId, bool inFlag, unsigned short inPing)
{
    EeWonRoomMsg aMsg;
    memset(&aMsg, 0, sizeof(aMsg));
    aMsg.kind = inKind;
    aMsg.id = inId;
    aMsg.flag = inFlag;
    aMsg.ping = inPing;
    std::string aBytes;
    ee_won_room_message(&aMsg, StagingAppendBytes, &aBytes);
    return new ByteBuffer(aBytes.data(), (unsigned long)aBytes.length(), false);
}

// Leaves the half-joined or half-hosted game when it goes out of scope, unless cleared (the original's cleanup at
// 0x7f06d0 holds only this bool) (provisional name)
struct StagingLeaveOnError {
    bool mLeave;
    StagingLeaveOnError() : mLeave(true) {}
    ~StagingLeaveOnError() { if (this->mLeave) LobbyLeaveGame(false); }
};

// FUNCTION: WON 0x007ef160
// Creates the staging logic, not in a game room
StagingLogic::StagingLogic()
{
    this->mInStaging = false;
    this->mLastReadyTime = 0;
}

// FUNCTION: WON 0x007ef1b0
// Destroys the staging logic (members only)
StagingLogic::~StagingLogic() {}

// FUNCTION: WON 0x007ef290
// Looks up the create-game dialog from the lobby layout
void StagingLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mCreateGameDialog, "CreateGameDialog", __FILE__, 41);
}

// FUNCTION: WON 0x007ef2b0
// Host: answers a client's request to join and announces the new player to the game, then re-advertises it (a
// client with a player already: room_accepts)
void StagingLogic::HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    ConstSmartPtr<ByteBuffer> aReply = this->mGame->GetJoinReply(inBuffer, inClient);
    if (aReply.get() == NULL) return;
    this->SendToClient(inClient, aReply.get());
    LobbyPlayer* aPlayer = inClient->GetPlayer().get();
    if (aPlayer != NULL) {
        WriteBuffer aBuffer(0, true);
        ByteBufferPtr aHead = StagingMessage(3, inClient->GetClientId(), false, 0);
        aBuffer.AppendBytes(aHead->data(), aHead->length());
        aPlayer->WriteToBuffer(aBuffer);
        this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
        LobbyNetAdvertiseGame();
        return;
    }
}

// FUNCTION: WON 0x007ef410
// Takes the host's answer to our join request and finishes the waiting join (from the captain: room_accepts)
void StagingLogic::HandleJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    this->mJoinStatus = this->mGame->HandleJoinReply(inBuffer);
    if (this->mJoinOp.get() != NULL) this->mJoinOp->Finish(WS_Success);
}

// FUNCTION: WON 0x007ef450
// Adds a player announced by the host (room_accepts: the captain) to its client, reports the change and plays the
// joined sound; the player's bytes follow the id read in HandleMessage
void StagingLogic::HandlePlayerJoined(const EeWonRoomMsg& inMsg, ReadBuffer& inBuffer, LobbyClient* inClient)
{
    LobbyClient* aClient = this->mGame->FindClient(inMsg.id);
    if (aClient == NULL) return;
    switch (ee_won_stage_player_joined(aClient->HasPlayer(), aClient->HasPlayer() && this->IAmCaptain())) {
    case 0: {
        SmartPtr<LobbyPlayer> aPlayer = LobbyPlayer::CreatePlayer();
        if (!aPlayer->ReadFromBuffer(inBuffer)) return;
        aClient->SetPlayer(aPlayer.get());
        break;
    }
    case 2: return;
    }
    LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 0), true);
    if (aClient->IsTeammate(this->mMyClient.get()))
        LobbyBroadcastEvent(new PlayerTeamChangedEvent(aClient, 0), true);
    this->mGame->HandlePlayerJoined(aClient);
    if (LobbyGetStagingSoundEffects()) LobbyPlaySound(::LobbyGlobal_PlayerJoined_Sound.get());
    if (this->mLastAllReady) this->mLastAllReady = ee_won_stage_joined_keeps_all_ready(true, aClient->IsReady());
    this->NotifyStagingChange();
}

// FUNCTION: WON 0x007ef620
// Host: broadcasts a player's changed ready flag to the game (room_accepts: we host, the sender has a player whose
// flag changes)
void StagingLogic::HandleReadyRequest(const EeWonRoomMsg& inMsg, LobbyClient* inClient)
{
    this->BroadcastToGame(StagingMessage(5, inClient->GetClientId(), inMsg.flag, 0).get());
}

// FUNCTION: WON 0x007ef720
// Sets a player's ready flag as announced by the host (room_accepts) and updates the all-ready state
void StagingLogic::HandleReadyChanged(const EeWonRoomMsg& inMsg, LobbyClient* inClient)
{
    LobbyClient* aClient = this->mGame->FindClient(inMsg.id);
    if (aClient == NULL || aClient->GetPlayer().get() == NULL)
        return;
    aClient->GetPlayer()->SetReady(inMsg.flag);
    this->UpdateReadyState();
    LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
}

// FUNCTION: WON 0x007ef7d0
// Host: broadcasts the ping a player reported to the game (room_accepts: we host, the sender has a player)
void StagingLogic::HandlePingReport(const EeWonRoomMsg& inMsg, LobbyClient* inClient)
{
    this->BroadcastToGame(StagingMessage(9, inClient->GetClientId(), false, inMsg.ping).get());
}

// FUNCTION: WON 0x007ef8c0
// Sets a player's ping as announced by the host (room_accepts)
void StagingLogic::HandlePingChanged(const EeWonRoomMsg& inMsg, LobbyClient* inClient)
{
    LobbyClient* aClient = this->mGame->FindClient(inMsg.id);
    if (aClient != NULL && aClient->GetPlayer().get() != NULL) {
        aClient->GetPlayer()->SetPing(inMsg.ping);
        LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
    }
}

// FUNCTION: WON 0x007ef970
// Leaves the game and tells the player when the host dissolved it (room_accepts: from the captain, not us)
void StagingLogic::HandleGameDissolved(LobbyClient* inClient)
{
    this->LeaveGame(false);
    LobbyMessageBox(::RoutingLogic_GameDissolvedTitle_String, ::RoutingLogic_GameDissolvedDetails_String);
}

// FUNCTION: WON 0x007ef9b0
// Clears every player's ready flag in the game and updates the all-ready state
void StagingLogic::ResetReadyFlags()
{
    if (this->mGame.get() == NULL) return;
    LobbyClientSetIterator anItr(this->mGame->GetClients().get());
    while (anItr.HasMore()) {
        LobbyClient* aClient = NULL;
        LobbyPlayer* aPlayer = anItr.Next(&aClient);
        if (aPlayer != NULL && aClient != NULL) {
            aPlayer->SetReady(false);
            LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
        }
    }
    this->UpdateReadyState();
}

// FUNCTION: WON 0x007efa80
// Host: marks the game as in progress or not and re-advertises it
void StagingLogic::SetGameInProgress(bool inInProgress)
{
    if (this->mGame.get() == NULL ||
        !ee_won_stage_sets_in_progress(true, this->mGame->IsInProgress(), inInProgress, this->IAmCaptain()))
        return;
    this->mGame->SetInProgress(inInProgress);
    LobbyNetSetGameInProgress(inInProgress);
    LobbyNetAdvertiseGame();
}

// FUNCTION: WON 0x007efac0
// Leaves the lobby to start the game when the host says so (room_accepts); the host also marks the game in progress
void StagingLogic::HandleStartGame(LobbyClient* inClient)
{
    bool isCaptain = this->IAmCaptain();
    if (isCaptain)
        this->SetGameInProgress(true);
    LobbyExit(ee_won_room_start_exit_code(isCaptain));
}

// FUNCTION: WON 0x007efb00
// Handles the host's announcement (room_accepts) that a client was kicked or banned
void StagingLogic::HandleClientKicked(const EeWonRoomMsg& inMsg, LobbyClient* inClient)
{
    LobbyClient* aClient = this->mGame->FindClient(inMsg.id);
    if (aClient != NULL) this->HandleKicked(aClient, inMsg.flag);
}

// FUNCTION: WON 0x007efb50
// Dispatches a game room message by type, passing unknown types to the game; while joining only the reply counts.
// ee_won reads the type and the fixed fields (a short message does nothing) and says whether the receiver acts on
// it; the game code's part is read on from the buffer
void StagingLogic::HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (this->mGame.get() == NULL || inClient == NULL) return;
    EeWonRoomMsg aMsg;
    long aStart = inBuffer.pos();
    if (ee_won_room_read(reinterpret_cast<const uint8_t*>(inBuffer.data()) + aStart,
                         (size_t)(inBuffer.length() - aStart), this->mJoinStatus == 1, &aMsg) != 0)
        return;
    inBuffer.SetPos((int)(aStart + aMsg.rest));
    EeWonRoomRoles aRoles;
    aRoles.sender_is_captain = inClient->IsCaptain(1);
    aRoles.i_am_captain = this->IAmCaptain();
    aRoles.sender_has_player = inClient->HasPlayer();
    aRoles.sender_ready = inClient->IsReady();
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
        default: this->mGame->HandleGameMessage(aMsg.kind, inBuffer, inClient); break;
        }
    } catch (ReadBufferException&) {}
}

// FUNCTION: WON 0x007efd20
// Handles a game room message held in a byte buffer
void StagingLogic::HandleMessageBuffer(const ByteBuffer* inMessage, LobbyClient* inClient)
{
    if (inMessage != NULL) {
        ReadBuffer aBuffer(inMessage->data(), inMessage->length());
        this->HandleMessage(aBuffer, inClient);
    }
}

// FUNCTION: WON 0x007efd60
// Sends a message to the host; the host handles its own message directly
void StagingLogic::SendToCaptain(const ByteBuffer* inMessage)
{
    switch (ee_won_stage_send_to_captain(this->mGame.get() != NULL, this->IAmCaptain())) {
    case EeWonStageRoute::Local: this->HandleMessageBuffer(inMessage, this->mMyClient.get()); break;
    case EeWonStageRoute::Net: LobbyNetSendToCaptain(inMessage); break;
    default: break;
    }
}

// FUNCTION: WON 0x007efda0
// Host: handles a message itself and sends it to every client in the game
void StagingLogic::BroadcastToGame(const ByteBuffer* inMessage)
{
    if (ee_won_stage_broadcast(this->mGame.get() != NULL, this->IAmCaptain()) == EeWonStageRoute::LocalAndNet) {
        this->HandleMessageBuffer(inMessage, this->mMyClient.get());
        LobbyNetBroadcastToGame(inMessage);
    }
}

// FUNCTION: WON 0x007efde0
// Host: sends a message to one client in the game
void StagingLogic::SendToClient(LobbyClient* inClient, const ByteBuffer* inMessage)
{
    if (ee_won_stage_send_to_client(this->mGame.get() != NULL, this->IAmCaptain()) == EeWonStageRoute::Net)
        LobbyNetSendToClient(inClient, inMessage);
}

// FUNCTION: WON 0x007efe10
// Returns whether we host the current game
bool StagingLogic::IAmCaptain()
{
    return this->mGame.get() != NULL && this->mGame->GetIsHost();
}

// FUNCTION: WON 0x007efe30
// Returns whether our own player is ready
bool StagingLogic::IAmReady()
{
    return this->mMyClient.get() != NULL && this->mMyClient->IsReady();
}

// FUNCTION: WON 0x007efe50
// Runs the host, leave, logout and start/ready buttons
void StagingLogic::HandleControlEvent(int inControlId)
{
    switch (inControlId) {
    case ID_HostGame: this->HostGame(); return;
    case ID_LeaveGame: this->LeaveGame(true); return;
    case ID_Logout: this->Logout(true); return;
    case ID_StartGame: this->ToggleReady(); return;
    }
}

// FUNCTION: WON 0x007efeb0
// Handles join-game and game-action events from the game lists
void StagingLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case LobbyEvent_JoinGame: this->HandleJoinGameEvent(inEvent); break;
    case LobbyEvent_GameAction: this->HandleGameAction(inEvent); break;
    }
}

// FUNCTION: WON 0x007efee0
// Opens a popup with a game's options
void StagingLogic::ShowGameOptions(LobbyGame* inGame)
{
    if (inGame == NULL)
        return;
    SmartPtr<GameOptionsPopup> aPopup = new GameOptionsPopup;
    aPopup->Init(inGame);
    LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
}

// FUNCTION: WON 0x007effa0
// Runs a game list action: options, join, ping, show or hide details, direct connect
void StagingLogic::HandleGameAction(ComponentEvent* inEvent)
{
    GameActionEvent* anEvent = static_cast<GameActionEvent*>(inEvent);
    LobbyGame* aGame = anEvent->mGame.get();
    if (aGame == NULL) return;
    switch (ee_won_stage_game_action(anEvent->mAction, aGame->GetGameType())) {
    case 7: this->ShowGameOptions(aGame); break;
    case 1: this->JoinGame(aGame); break;
    case 2:
        LobbyPingGame(aGame, true);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 3:
        LobbyQueryGameDetails(aGame);
        aGame->SetShowDetails(true);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 5:
        aGame->SetShowDetails(false);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 6:
        LobbyDispatchEvent(new ChangeDirectConnectEvent(1, aGame));
        break;
    }
}

// FUNCTION: WON 0x007f0110
// Leaves the game (confirming if asked), logs out, saves the settings and returns to the login screen
bool StagingLogic::Logout(bool inConfirm)
{
    if (!this->LeaveGame(inConfirm)) return false;
    this->mCreateGameDialog->Reset(true);
    LobbyNetLogout();
    LobbyKillPings(0);
    LobbyShowLoginScreen();
    LobbySaveGlobalPrefs(false);
    LobbySaveUserPrefs(false);
    ::LobbyGlobal_SierraLogon_Sound->Stop();
    return true;
}

// FUNCTION: WON 0x007f0170
// Asks the host to set our ready flag
void StagingLogic::SendReady(bool inReady)
{
    this->SendToCaptain(StagingMessage(4, 0, inReady, 0).get());
}

// FUNCTION: WON 0x007f0230
// Host: tells every client in the game that it is dissolved
void StagingLogic::SendGameDissolved()
{
    if (!this->IAmCaptain())
        return;
    this->BroadcastToGame(StagingMessage(6, 0, false, 0).get());
}

// FUNCTION: WON 0x007f02e0
// Tells every client in the game to start it
void StagingLogic::SendStartGame()
{
    this->BroadcastToGame(StagingMessage(7, 0, false, 0).get());
}

// FUNCTION: WON 0x007f0390
// Host: starts the game when it can; others toggle their ready flag if the game allows it
void StagingLogic::ToggleReady()
{
    bool aHave = this->mMyClient.get() != NULL && this->mGame.get() != NULL;
    EeWonToggleReady aToggle = ee_won_stage_toggle_ready(aHave, aHave, aHave && this->IAmCaptain(),
                                                         aHave && this->mMyClient->IsReady());
    if (aToggle.action == 1) {
        if (this->mGame->CanStartGame() && this->mGame->TryStartGame()) this->SendStartGame();
    } else if (aToggle.action == 2) {
        if (aToggle.ask_can_ready && !this->mGame->CanReady()) return;
        this->mLastReady = aToggle.want_ready;
        this->SendReady(aToggle.want_ready);
    }
}

// FUNCTION: WON 0x007f0410
// Hosts a new game from the create-game dialog after firewall warnings, entering its game room
void StagingLogic::HostGame()
{
    if (!this->LeaveGame(true)) return;
    int aGameType = LobbyGetGameScreenType();
    // the firewall state is asked only where the original asked it (an Internet or direct-connect game with
    // confirmations on; behind the firewall only once the test is done)
    bool aConfirm = aGameType != 0 && aGameType != 1 && LobbyGetShowConfirmations();
    bool aDone = aConfirm && LobbyIsFirewallTestDone();
    int aWarning = ee_won_stage_host_warning(aGameType, aConfirm, aDone, aDone && LobbyIsBehindFirewall());
    if (aWarning < 0) return;
    if (aWarning != 0) {
        int aResult = aWarning == 1
            ? LobbyDoYesNoDialog(::GameStagingLogic_ConnectWarningTitle1_String, ::GameStagingLogic_ConnectWarningDetails1_String, true)
            : LobbyDoYesNoDialog(::GameStagingLogic_ConnectWarningTitle2_String, ::GameStagingLogic_ConnectWarningDetails2_String, true);
        if (aResult != 0x186a1) return;
    }
    this->mCreateGameDialog->SetGameType(aGameType);
    this->mCreateGameDialog->Reset(false);
    if (LobbyDoDialog(this->mCreateGameDialog.get()) != 0x186a1) return;
    SmartPtr<LobbyGame> aGame = this->mCreateGameDialog->GetGame();
    aGame->SetAddress(LobbyGetMyAddress(aGameType));
    this->mGame = aGame;
    this->mJoinStatus = 0;
    StagingLeaveOnError aCleanup;
    if (!aGame->PreNetCreateGame()) return;
    if (!LobbyNetHostGame(aGame.get())) return;
    this->mMyClient = LobbyGetNetMyClient();
    if (this->mMyClient.get() == NULL) return;
    aGame->SetIsHost(true);
    aGame->SetCaptainId(this->mMyClient->GetClientId());
    this->mMyClient->SetCaptain(true, true);
    if (!aGame->PostNetCreateGame()) return;
    aGame->GetClients()->AddClient(this->mMyClient.get());
    aGame->HandleHostGame(this->mMyClient.get());
    aCleanup.mLeave = false;
    this->mInStaging = true;
    this->mLastReadyTime = 0;
    this->mLastAllReady = aGame->AreAllReady();
    this->mLastReady = LobbyIAmReady();
    LobbyNetAdvertiseGame();
    LobbyBroadcastLobbyEvent(0x186bd, true);
}

// FUNCTION: WON 0x007f06e0
// For an Internet game not yet pinged, waits for a ping reply or asks to join anyway; returns whether to join
bool StagingLogic::VerifyPingBeforeJoin(LobbyGame* inGame)
{
    if (inGame->GetGameType() != 2 || inGame->GetPingReplied())
        return true;
    LobbyConfig* aConfig = GetLobbyConfig();
    bool aWait = aConfig != NULL && !aConfig->mAllowJoinGameWithoutPing;
    // confirmations are asked only when the ping is not waited for, as the original asked
    int aStep = ee_won_stage_verify_ping(inGame->GetGameType(), inGame->GetPingReplied(), aConfig != NULL,
                                         aConfig != NULL && aConfig->mAllowJoinGameWithoutPing,
                                         !aWait && LobbyGetShowConfirmations());
    if (aStep == 1) {
        LobbyPingGame(inGame, true);
        LobbyBroadcastEvent(new GameChangedEvent(inGame, 2), true);
        while (inGame->IsPinging()) {
            SmartPtr<AsyncOp> anOp = new AsyncOp;
            LobbyRunOpWithStatus(anOp.get(), ::GameStagingLogic_VerifyingPingTitle_String, ::GameStagingLogic_VerifyingPingDetails_String, 2, 500);
            if (anOp->IsKilled()) return false;
        }
        if (!inGame->GetPingReplied()) {
            LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, ::GameStagingLogic_NoPingReplyError_String);
            return false;
        }
    } else if (aStep == 2) {
        return LobbyDoYesNoDialog(::GameStagingLogic_ConnectWarningTitle3_String, ::GameStagingLogic_ConnectWarningDetails3_String, true) == 0x186a1;
    }
    return true;
}

// FUNCTION: WON 0x007f08a0
// Joins the game named by a join-game event
void StagingLogic::HandleJoinGameEvent(ComponentEvent* inEvent)
{
    this->JoinGame(static_cast<JoinGameEvent*>(inEvent)->mGame.get());
}

// FUNCTION: WON 0x007f08b0
// Joins a game from the list: checks ping and status, connects, waits for the host's reply and enters the room
void StagingLogic::JoinGame(LobbyGame* inGame)
{
    if (inGame == NULL) return;
    if (this->mGame.get() != NULL && inGame->IsSameGame(this->mGame.get())) {
        LobbyMessageBox(::RoutingLogic_AlreadyJoinedTitle_String, ::RoutingLogic_AlreadyInGame_String);
        return;
    }
    if (!this->VerifyPingBeforeJoin(inGame)) return;
    short aStatus = inGame->GetJoinStatus();
    if (aStatus != 0) {
        if (ee_won_stage_join_status_shown(aStatus)) LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, inGame->GetStatusString(aStatus));
        return;
    }
    if (!this->LeaveGame(true)) return;
    StagingLeaveOnError aCleanup;
    SmartPtr<LobbyGame> aGame = LobbyGame::CreateGame(static_cast<LobbyGameType>(inGame->GetGameType()));
    aGame->CopyFrom(inGame);
    this->mGame = aGame;
    this->mJoinStatus = 1;
    if (!aGame->PreNetJoinGame()) return;
    if (!LobbyNetJoinGame(aGame.get())) return;
    this->mMyClient = LobbyGetNetMyClient();
    if (this->mMyClient.get() == NULL) return;
    if (!aGame->PostNetJoinGame()) return;
    this->mJoinOp = new AsyncOp;
    ConstSmartPtr<ByteBuffer> aMessage = this->mGame->GetJoinRequest();
    this->SendToCaptain(aMessage.get());
    LobbyRunOpWithStatus(this->mJoinOp.get(), ::GameStagingLogic_GettingGameInfoTitle_String, ::GameStagingLogic_GettingGameInfoDetails_String, 1, 0);
    this->mJoinOp = NULL;
    if (this->mJoinStatus != 0) {
        if (ee_won_stage_join_status_shown(this->mJoinStatus)) LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, inGame->GetStatusString(this->mJoinStatus));
        return;
    }
    aCleanup.mLeave = false;
    this->mInStaging = true;
    this->mLastReadyTime = 0;
    this->mLastAllReady = aGame->AreAllReady();
    this->mLastReady = LobbyIAmReady();
    LobbyBroadcastLobbyEvent(0x186bd, true);
}

// FUNCTION: WON 0x007f0c60
// Leaves or dissolves the current game, confirming if asked; returns false only when the player cancels
bool StagingLogic::LeaveGame(bool inConfirm)
{
    if (this->mGame.get() == NULL)
        return true;
    int aConfirm = ee_won_stage_leave_confirm(inConfirm, inConfirm && LobbyGetShowConfirmations(), this->IAmCaptain());
    if (aConfirm != 0) {
        int aResult;
        if (aConfirm == 1) aResult = LobbyDoYesNoDialog(::GameStagingLogic_DissolveGame_String, ::GameStagingLogic_DissolveGameConfirm_String, true);
        else aResult = LobbyDoYesNoDialog(::GameStagingLogic_LeaveGame_String, ::GameStagingLogic_LeaveGameConfirm_String, true);
        if (aResult != 0x186a1) return false;
    }
    if (this->mGame.get() == NULL)
        return true;
    if (this->IAmCaptain()) {
        this->SendGameDissolved();
        LobbyNetRemoveGameAdvertisement(this->mGame.get());
    }
    if (this->mGame.get() == NULL)
        return true;
    if (this->mJoinOp.get() != NULL) {
        this->mJoinOp->Kill(true);
        this->mJoinOp = NULL;
    }
    SmartPtr<LobbyGame> aGame = this->mGame;
    this->mGame = NULL;
    aGame->ClearPlayers();
    this->mMyClient = NULL;
    aGame->PreNetLeaveGame();
    LobbyNetLeaveGame(aGame.get());
    aGame->PostNetLeaveGame();
    this->mLastReadyTime = 0;
    if (this->mInStaging) {
        LobbyBroadcastLobbyEvent(0x186c0, true);
        this->mInStaging = false;
    }
    return true;
}

// FUNCTION: WON 0x007f0de0
// Leaves the game after losing the connection, optionally telling the player
void StagingLogic::HandleDisconnect(bool inShowError)
{
    if (this->mGame.get() != NULL) {
        this->LeaveGame(false);
        if (inShowError) LobbyMessageBox(::StagingLogic_DisconnectedTitle_String, ::StagingLogic_DisconnectedDetails_String);
    }
}

// FUNCTION: WON 0x007f0e10
// Reports a kick or ban from the game; when it is us, leaves the game and tells the player
void StagingLogic::HandleKicked(LobbyClient* inClient, bool inBan)
{
    LobbyGame* aGame = this->mGame.get();
    if (inClient == NULL || aGame == NULL) return;
    bool isMe = inClient == this->mMyClient.get();
    EeWonKicked aKicked = ee_won_stage_kicked(inBan, aGame->GetGameType(), isMe);
    LobbyBroadcastEvent(new ClientModeratedEvent(inClient, aKicked.flags, this->mGame->GetName(),
        aKicked.kind, 0, isMe, gEmptyGUIString), true);
    if (aKicked.message == 0)
        return;
    this->LeaveGame(false);
    if (aKicked.message == 1) LobbyMessageBox(::StagingLogic_BannedTitle_String, ::StagingLogic_BannedDetails_String);
    else LobbyMessageBox(::StagingLogic_KickedTitle_String, ::StagingLogic_KickedDetails_String);
}

// FUNCTION: WON 0x007f0f10
// Removes a departed player from the game, reports it and re-advertises the game when hosting
void StagingLogic::HandleClientLeft(LobbyClient* inClient)
{
    if (!ee_won_stage_client_left(inClient->HasPlayer(), this->mGame.get() != NULL))
        return;
    LobbyBroadcastEvent(new PlayerChangedEvent(inClient, 1), true);
    if (inClient->IsTeammate(this->mMyClient.get()))
        LobbyBroadcastEvent(new PlayerTeamChangedEvent(inClient, 1), true);
    this->mGame->RemoveClient(inClient);
    if (this->IAmCaptain()) LobbyNetAdvertiseGame();
    this->NotifyStagingChange();
}

// FUNCTION: WON 0x007f1000
// Reports our new ping to the current game's host
void StagingLogic::NotifyPingChange(LobbyGame* inGame)
{
    bool aHave = this->mGame.get() != NULL;
    bool aCaptain = aHave && this->IAmCaptain();
    if (!ee_won_stage_reports_ping(aHave, aCaptain, aHave && !aCaptain && inGame->IsSameGame(this->mGame.get())))
        return;
    this->SendToCaptain(StagingMessage(8, 0, false, inGame->GetPing()).get());
}

// FUNCTION: WON 0x007f10f0
// Host: kicks or bans a client from the game and tells everyone
void StagingLogic::KickClient(LobbyClient* inClient, bool inBan)
{
    if (!ee_won_stage_kicks(inClient != NULL, this->mGame.get() != NULL, this->IAmCaptain()))
        return;
    this->BroadcastToGame(StagingMessage(10, inClient->GetClientId(), inBan, 0).get());
    LobbyNetKickClient(inClient, inBan);
}

// FUNCTION: WON 0x007f11f0
// Tells the lobby screens that the game room changed
void StagingLogic::NotifyStagingChange()
{
    LobbyBroadcastLobbyEvent(0x186dd, true);
}

// FUNCTION: WON 0x007f1200
// Plays the all-ready sound (at most every 15 s) or the not-ready sound when the game's ready state changes
// (ee_won_room_ready_state decides)
void StagingLogic::UpdateReadyState()
{
    this->NotifyStagingChange();
    LobbyGame* aGame = this->mGame.get();
    if (aGame == NULL) return;
    EeWonReadyState aState;
    aState.all_ready = aGame->AreAllReady();
    aState.last_all_ready = this->mLastAllReady;
    aState.is_host = aGame->GetIsHost();
    aState.last_ready = this->mLastReady;
    aState.i_am_ready = LobbyIAmReady();
    aState.now = (int64_t)time(NULL);
    aState.last_ready_time = this->mLastReadyTime;
    ee_won_room_ready_state(&aState);
    this->mLastAllReady = aState.all_ready;
    this->mLastReady = aState.last_ready;
    this->mLastReadyTime = (long)aState.last_ready_time;
    if (aState.sound != EeWonReadySound::Silent && LobbyGetStagingSoundEffects())
        LobbyPlaySound(aState.sound == EeWonReadySound::AllReady ? ::LobbyGlobal_AllReady_Sound.get()
                                                            : ::LobbyGlobal_AllNotReady_Sound.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ef190
// (public: virtual void * __thiscall WONAPI::StagingLogic::`scalar deleting destructor'(unsigned int)    [??_GStagingLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f06d0
// (public: __thiscall WONAPI::StagingLeaveOnError::~StagingLeaveOnError(void)    [??1StagingLeaveOnError@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f45d0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f16f0
// (public: class WONAPI::LobbyGame const * __thiscall WONAPI::SmartPtrRep<class WONAPI::LobbyGame>::operator=(class WONAPI::LobbyGame const *))
