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
// Host: answers a client's request to join and announces the new player to the game, then re-advertises it
void StagingLogic::HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (inClient->HasPlayer()) return;
    ConstSmartPtr<ByteBuffer> aReply = this->mGame->GetJoinReply(inBuffer, inClient);
    if (aReply.get() == NULL) return;
    this->SendToClient(inClient, aReply.get());
    LobbyPlayer* aPlayer = inClient->mPlayer.get();
    if (aPlayer != NULL) {
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendByte(3);
        aBuffer.AppendShort(inClient->GetClientId());
        aPlayer->WriteToBuffer(aBuffer);
        this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
        LobbyNetAdvertiseGame();
        return;
    }
}

// FUNCTION: WON 0x007ef410
// Takes the host's answer to our join request and finishes the waiting join
void StagingLogic::HandleJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (inClient->IsCaptain(1)) {
        this->mJoinStatus = this->mGame->HandleJoinReply(inBuffer);
        if (this->mJoinOp.get() != NULL) this->mJoinOp->Finish(WS_Success);
    }
}

// FUNCTION: WON 0x007ef450
// Adds a player announced by the host to its client, reports the change and plays the joined sound
void StagingLogic::HandlePlayerJoined(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!inClient->IsCaptain(1)) return;
    unsigned short aClientId = inBuffer.ReadShort();
    LobbyClient* aClient = this->mGame->FindClient(aClientId);
    if (aClient == NULL) return;
    if (!aClient->HasPlayer()) {
        SmartPtr<LobbyPlayer> aPlayer = LobbyPlayer::CreatePlayer();
        if (!aPlayer->ReadFromBuffer(inBuffer)) return;
        aClient->SetPlayer(aPlayer.get());
    } else if (!this->IAmCaptain()) return;
    LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 0), true);
    if (aClient->IsTeammate(this->mMyClient.get()))
        LobbyBroadcastEvent(new PlayerTeamChangedEvent(aClient, 0), true);
    this->mGame->HandlePlayerJoined(aClient);
    if (LobbyGetStagingSoundEffects()) LobbyPlaySound(::LobbyGlobal_PlayerJoined_Sound.get());
    if (this->mLastAllReady && !aClient->IsReady()) this->mLastAllReady = false;
    this->NotifyStagingChange();
}

// FUNCTION: WON 0x007ef620
// Host: broadcasts a player's changed ready flag to the game
void StagingLogic::HandleReadyRequest(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!this->IAmCaptain() || !inClient->HasPlayer())
        return;
    bool aReady = inBuffer.ReadBool();
    if (inClient->IsReady() == aReady)
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(5);
    aBuffer.AppendShort(inClient->GetClientId());
    aBuffer.AppendBool(aReady);
    this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007ef720
// Sets a player's ready flag as announced by the host and updates the all-ready state
void StagingLogic::HandleReadyChanged(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!inClient->IsCaptain(1))
        return;
    unsigned short anId = inBuffer.ReadShort();
    bool aReady = inBuffer.ReadBool();
    LobbyClient* aClient = this->mGame->FindClient(anId);
    if (aClient == NULL || aClient->mPlayer.get() == NULL)
        return;
    aClient->mPlayer->mIsReady = aReady;
    this->UpdateReadyState();
    LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
}

// FUNCTION: WON 0x007ef7d0
// Host: broadcasts the ping a player reported to the game
void StagingLogic::HandlePingReport(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!this->IAmCaptain())
        return;
    unsigned short aPing = inBuffer.ReadShort();
    if (inClient->mPlayer.get() == NULL)
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(9);
    aBuffer.AppendShort(inClient->GetClientId());
    aBuffer.AppendShort(aPing);
    this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007ef8c0
// Sets a player's ping as announced by the host
void StagingLogic::HandlePingChanged(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!inClient->IsCaptain(1))
        return;
    unsigned short anId = inBuffer.ReadShort();
    unsigned short aPing = inBuffer.ReadShort();
    LobbyClient* aClient = this->mGame->FindClient(anId);
    if (aClient != NULL && aClient->mPlayer.get() != NULL) {
        aClient->mPlayer->mPing = aPing;
        LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
    }
}

// FUNCTION: WON 0x007ef970
// Leaves the game and tells the player when the host dissolved it
void StagingLogic::HandleGameDissolved(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (inClient->IsCaptain(1) && !this->IAmCaptain()) {
        this->LeaveGame(false);
        LobbyMessageBox(::RoutingLogic_GameDissolvedTitle_String, ::RoutingLogic_GameDissolvedDetails_String);
    }
}

// FUNCTION: WON 0x007ef9b0
// Clears every player's ready flag in the game and updates the all-ready state
void StagingLogic::ResetReadyFlags()
{
    if (this->mGame.get() == NULL) return;
    LobbyClientSetIterator anItr(this->mGame->mClients.get());
    while (anItr.HasMore()) {
        LobbyClient* aClient = NULL;
        LobbyPlayer* aPlayer = anItr.Next(&aClient);
        if (aPlayer != NULL && aClient != NULL) {
            aPlayer->mIsReady = false;
            LobbyBroadcastEvent(new PlayerChangedEvent(aClient, 2), true);
        }
    }
    this->UpdateReadyState();
}

// FUNCTION: WON 0x007efa80
// Host: marks the game as in progress or not and re-advertises it
void StagingLogic::SetGameInProgress(bool inInProgress)
{
    if (this->mGame.get() == NULL || this->mGame->mInProgress == inInProgress || !this->IAmCaptain())
        return;
    this->mGame->mInProgress = inInProgress;
    LobbyNetSetGameInProgress(inInProgress);
    LobbyNetAdvertiseGame();
}

// FUNCTION: WON 0x007efac0
// Leaves the lobby to start the game when the host says so; the host also marks the game in progress
void StagingLogic::HandleStartGame(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!inClient->IsCaptain(1))
        return;
    if (this->IAmCaptain()) {
        this->SetGameInProgress(true);
        LobbyExit(2);
    } else LobbyExit(1);
}

// FUNCTION: WON 0x007efb00
// Handles the host's announcement that a client was kicked or banned
void StagingLogic::HandleClientKicked(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (!inClient->IsCaptain(1))
        return;
    unsigned short anId = inBuffer.ReadShort();
    bool aFlag = inBuffer.ReadBool();
    LobbyClient* aClient = this->mGame->FindClient(anId);
    if (aClient != NULL) this->HandleKicked(aClient, aFlag);
}

// FUNCTION: WON 0x007efb50
// Dispatches a game room message by type, passing unknown types to the game; while joining only the reply counts
void StagingLogic::HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    if (this->mGame.get() == NULL || inClient == NULL) return;
    try {
        unsigned char aType = inBuffer.ReadByte();
        if (this->mJoinStatus == 1 && aType != 2) return;
        switch (aType) {
        case 1: this->HandleJoinRequest(inBuffer, inClient); break;
        case 2: this->HandleJoinReply(inBuffer, inClient); break;
        case 3: this->HandlePlayerJoined(inBuffer, inClient); break;
        case 4: this->HandleReadyRequest(inBuffer, inClient); break;
        case 5: this->HandleReadyChanged(inBuffer, inClient); break;
        case 6: this->HandleGameDissolved(inBuffer, inClient); break;
        case 7: this->HandleStartGame(inBuffer, inClient); break;
        case 8: this->HandlePingReport(inBuffer, inClient); break;
        case 9: this->HandlePingChanged(inBuffer, inClient); break;
        case 10: this->HandleClientKicked(inBuffer, inClient); break;
        default: this->mGame->HandleGameMessage(aType, inBuffer, inClient); break;
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
    if (this->mGame.get() != NULL) {
        if (this->IAmCaptain()) this->HandleMessageBuffer(inMessage, this->mMyClient.get());
        else LobbyNetSendToCaptain(inMessage);
    }
}

// FUNCTION: WON 0x007efda0
// Host: handles a message itself and sends it to every client in the game
void StagingLogic::BroadcastToGame(const ByteBuffer* inMessage)
{
    if (this->mGame.get() != NULL && this->IAmCaptain()) {
        this->HandleMessageBuffer(inMessage, this->mMyClient.get());
        LobbyNetBroadcastToGame(inMessage);
    }
}

// FUNCTION: WON 0x007efde0
// Host: sends a message to one client in the game
void StagingLogic::SendToClient(LobbyClient* inClient, const ByteBuffer* inMessage)
{
    if (this->mGame.get() != NULL && this->IAmCaptain()) LobbyNetSendToClient(inClient, inMessage);
}

// FUNCTION: WON 0x007efe10
// Returns whether we host the current game
bool StagingLogic::IAmCaptain()
{
    return this->mGame.get() != NULL && this->mGame->mIsHost;
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
    switch (anEvent->mAction) {
    case 7: this->ShowGameOptions(aGame); break;
    case 1: this->JoinGame(aGame); break;
    case 2:
        LobbyPingGame(aGame, true);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 3:
    case 4:
        LobbyQueryGameDetails(aGame);
        aGame->mShowDetails = true;
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 5:
        aGame->mShowDetails = false;
        LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
        break;
    case 6:
        if (aGame->GetGameType() == 3)
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
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(4);
    aBuffer.AppendBool(inReady);
    this->SendToCaptain(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007f0230
// Host: tells every client in the game that it is dissolved
void StagingLogic::SendGameDissolved()
{
    if (!this->IAmCaptain())
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(6);
    this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007f02e0
// Tells every client in the game to start it
void StagingLogic::SendStartGame()
{
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(7);
    this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007f0390
// Host: starts the game when it can; others toggle their ready flag if the game allows it
void StagingLogic::ToggleReady()
{
    if (this->mMyClient.get() == NULL || this->mGame.get() == NULL) return;
    if (this->IAmCaptain()) {
        if (this->mGame->CanStartGame() && this->mGame->TryStartGame()) this->SendStartGame();
    } else {
        bool aReady = !this->mMyClient->IsReady();
        if (aReady && !this->mGame->CanReady()) return;
        this->mLastReady = aReady;
        this->SendReady(aReady);
    }
}

// FUNCTION: WON 0x007f0410
// Hosts a new game from the create-game dialog after firewall warnings, entering its game room
void StagingLogic::HostGame()
{
    if (!this->LeaveGame(true)) return;
    int aGameType = LobbyGetGameScreenType();
    if (aGameType == 0) return;
    if (aGameType != 1 && LobbyGetShowConfirmations()) {
        int aResult = 0x186a1;
        if (!LobbyIsFirewallTestDone()) aResult = LobbyDoYesNoDialog(::GameStagingLogic_ConnectWarningTitle1_String, ::GameStagingLogic_ConnectWarningDetails1_String, true);
        else if (LobbyIsBehindFirewall()) aResult = LobbyDoYesNoDialog(::GameStagingLogic_ConnectWarningTitle2_String, ::GameStagingLogic_ConnectWarningDetails2_String, true);
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
    aGame->mClients->AddClient(this->mMyClient.get());
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
    if (inGame->GetGameType() != 2 || inGame->mPingReplied)
        return true;
    LobbyConfig* aConfig = GetLobbyConfig();
    if (aConfig != NULL && !aConfig->mAllowJoinGameWithoutPing) {
        LobbyPingGame(inGame, true);
        LobbyBroadcastEvent(new GameChangedEvent(inGame, 2), true);
        while (inGame->IsPinging()) {
            SmartPtr<AsyncOp> anOp = new AsyncOp;
            LobbyRunOpWithStatus(anOp.get(), ::GameStagingLogic_VerifyingPingTitle_String, ::GameStagingLogic_VerifyingPingDetails_String, 2, 500);
            if (anOp->mKilled) return false;
        }
        if (!inGame->mPingReplied) {
            LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, ::GameStagingLogic_NoPingReplyError_String);
            return false;
        }
    } else if (LobbyGetShowConfirmations()) {
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
        if (aStatus != 1) LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, inGame->GetStatusString(aStatus));
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
        if (this->mJoinStatus != 1) LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String, inGame->GetStatusString(this->mJoinStatus));
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
    if (inConfirm && LobbyGetShowConfirmations()) {
        int aResult;
        if (this->IAmCaptain()) aResult = LobbyDoYesNoDialog(::GameStagingLogic_DissolveGame_String, ::GameStagingLogic_DissolveGameConfirm_String, true);
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
    int aKind = !inBan;
    bool isMe = inClient == this->mMyClient.get();
    int aFlags = aGame->GetGameType() == 2 ? 6 : 10;
    LobbyBroadcastEvent(new ClientModeratedEvent(inClient, aFlags, this->mGame->mName,
        aKind, 0, isMe, gEmptyGUIString), true);
    if (!isMe)
        return;
    this->LeaveGame(false);
    if (inBan) LobbyMessageBox(::StagingLogic_BannedTitle_String, ::StagingLogic_BannedDetails_String);
    else LobbyMessageBox(::StagingLogic_KickedTitle_String, ::StagingLogic_KickedDetails_String);
}

// FUNCTION: WON 0x007f0f10
// Removes a departed player from the game, reports it and re-advertises the game when hosting
void StagingLogic::HandleClientLeft(LobbyClient* inClient)
{
    if (!inClient->HasPlayer() || this->mGame.get() == NULL)
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
    if (this->mGame.get() == NULL || this->IAmCaptain() || !inGame->IsSameGame(this->mGame.get()))
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(8);
    aBuffer.AppendShort(inGame->GetPing());
    this->SendToCaptain(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007f10f0
// Host: kicks or bans a client from the game and tells everyone
void StagingLogic::KickClient(LobbyClient* inClient, bool inBan)
{
    if (inClient == NULL || this->mGame.get() == NULL || !this->IAmCaptain())
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(10);
    aBuffer.AppendShort(inClient->GetClientId());
    aBuffer.AppendBool(inBan);
    this->BroadcastToGame(aBuffer.ToByteBuffer(true).get());
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
void StagingLogic::UpdateReadyState()
{
    this->NotifyStagingChange();
    LobbyGame* aGame = this->mGame.get();
    if (aGame == NULL) return;
    bool isReady = aGame->AreAllReady();
    if (isReady) {
        if (this->mLastAllReady != isReady) {
            long aTime = time(NULL);
            if (aTime - this->mLastReadyTime >= 15) {
                this->mLastReadyTime = aTime;
                if (LobbyGetStagingSoundEffects()) {
                    LobbyPlaySound(::LobbyGlobal_AllReady_Sound.get());
                }
            }
        }
    } else if (!aGame->mIsHost && this->mLastReady != LobbyIAmReady()) {
        this->mLastReady = LobbyIAmReady();
        if (!this->mLastReady && LobbyGetStagingSoundEffects()) LobbyPlaySound(::LobbyGlobal_AllNotReady_Sound.get());
    }
    this->mLastAllReady = isReady;
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
