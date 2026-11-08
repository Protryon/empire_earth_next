// StagingLogic: game staging (join requests, ready state, pings, start, kicks); 0x5c bytes
#ifndef EE_WONAPI_WONLOBBYSTAGING_H
#define EE_WONAPI_WONLOBBYSTAGING_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
class CreateGameDialog;
// RTTI: StagingLogic > LobbyLogic > RefCount; vtable 0x855ae0 with six slots.
// Constructor 0x7ef160; members start at 0x40, the last owned pointer is at 0x58.
class StagingLogic : public LobbyLogic {
public:
    StagingLogic();
    virtual ~StagingLogic();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandlePlayerJoined(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleReadyRequest(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleReadyChanged(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandlePingReport(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandlePingChanged(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleGameDissolved(ReadBuffer& inBuffer, LobbyClient* inClient);
    void ResetReadyFlags();
    void SetGameInProgress(bool inInProgress);
    void HandleStartGame(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleClientKicked(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleMessageBuffer(const ByteBuffer* inMessage, LobbyClient* inClient);
    void SendToCaptain(const ByteBuffer* inMessage);
    void BroadcastToGame(const ByteBuffer* inMessage);
    void SendToClient(LobbyClient* inClient, const ByteBuffer* inMessage);
    bool IAmCaptain();
    bool IAmReady();
    void ShowGameOptions(LobbyGame* inGame);
    void HandleGameAction(ComponentEvent* inEvent);
    bool Logout(bool inConfirm);
    void SendReady(bool inReady);
    void SendGameDissolved();
    void SendStartGame();
    void ToggleReady();
    void HostGame();
    bool VerifyPingBeforeJoin(LobbyGame* inGame);
    void HandleJoinGameEvent(ComponentEvent* inEvent);
    void JoinGame(LobbyGame* inGame);
    bool LeaveGame(bool inConfirm);
    void HandleDisconnect(bool inShowError);
    void HandleKicked(LobbyClient* inClient, bool inBan);
    void HandleClientLeft(LobbyClient* inClient);
    void NotifyPingChange(LobbyGame* inGame);
    void KickClient(LobbyClient* inClient, bool inBan);
    void NotifyStagingChange();
    void UpdateReadyState();
    SmartPtr<LobbyGame> mGame; // 0x40
    SmartPtr<LobbyClient> mMyClient; // 0x44
    SmartPtr<AsyncOp> mJoinOp; // 0x48
    short mJoinStatus; // 0x4c
    bool mInStaging; // 0x4e
    bool mLastReady; // 0x4f
    bool mLastAllReady; // 0x50
    long mLastReadyTime; // 0x54
    SmartPtr<CreateGameDialog> mCreateGameDialog; // 0x58
};
}
#endif
