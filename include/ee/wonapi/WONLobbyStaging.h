// StagingLogic: game staging (join requests, ready state, pings, start, kicks); 0x5c bytes in the original
#ifndef EE_WONAPI_WONLOBBYSTAGING_H
#define EE_WONAPI_WONLOBBYSTAGING_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "rust/ee_won.h"
namespace WONAPI {
class CreateGameDialog;
// RTTI: StagingLogic > LobbyLogic > RefCount; vtable 0x855ae0 with six slots; constructor 0x7ef160.
// A Rust object since round 20 (rust/crates/ee_won/src/stagingobj.rs): its methods and scalar state (the join
// status, the staging and ready flags, the last all-ready time) are Rust's, its calls out the host's ops
// (WONLobby/LobbyLogicHost.cpp); this shell keeps the SmartPtrs the lobby reads and the virtual slots.
class StagingLogic : public LobbyLogic {
public:
    StagingLogic();
    virtual ~StagingLogic();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    intptr_t Call(EeWonStagingCall inCall, void* inA = NULL, void* inB = NULL, intptr_t inC = 0)
    { return ee_won_staging_call(this->mRust, inCall, inA, inB, inC, 0); }
    // a message call: the ByteBuffer, its bytes and their length
    void CallMessage(EeWonStagingCall inCall, const ByteBuffer* inMessage, LobbyClient* inClient);
    void HandleMessage(ReadBuffer& inBuffer, LobbyClient* inClient);
    void HandleMessageBuffer(const ByteBuffer* inMessage, LobbyClient* inClient);
    void SendToCaptain(const ByteBuffer* inMessage) { this->CallMessage(EeWonStagingCall::SendToCaptain, inMessage, NULL); }
    void BroadcastToGame(const ByteBuffer* inMessage) { this->CallMessage(EeWonStagingCall::BroadcastToGame, inMessage, NULL); }
    void SendToClient(LobbyClient* inClient, const ByteBuffer* inMessage)
    { this->CallMessage(EeWonStagingCall::SendToClient, inMessage, inClient); }
    bool IAmCaptain() { return this->Call(EeWonStagingCall::IAmCaptain) != 0; }
    bool IAmReady() { return this->Call(EeWonStagingCall::IAmReady) != 0; }
    void JoinGame(LobbyGame* inGame) { this->Call(EeWonStagingCall::JoinGame, inGame); }
    void HostGame() { this->Call(EeWonStagingCall::HostGame); }
    bool Logout(bool inConfirm) { return this->Call(EeWonStagingCall::Logout, NULL, NULL, inConfirm) != 0; }
    void KickClient(LobbyClient* inClient, bool inBan) { this->Call(EeWonStagingCall::KickClient, NULL, inClient, inBan); }
    void HandleDisconnect(bool inShowError) { this->Call(EeWonStagingCall::HandleDisconnect, NULL, NULL, inShowError); }
    void HandleClientLeft(LobbyClient* inClient) { this->Call(EeWonStagingCall::HandleClientLeft, NULL, inClient); }
    void HandleKicked(LobbyClient* inClient, bool inBan) { this->Call(EeWonStagingCall::HandleKicked, NULL, inClient, inBan); }
    void ResetReadyFlags() { this->Call(EeWonStagingCall::ResetReadyFlags); }
    void SetGameInProgress(bool inInProgress) { this->Call(EeWonStagingCall::SetGameInProgress, NULL, NULL, inInProgress); }
    bool LeaveGame(bool inConfirm) { return this->Call(EeWonStagingCall::LeaveGame, NULL, NULL, inConfirm) != 0; }
    void NotifyStagingChange() { this->Call(EeWonStagingCall::NotifyStagingChange); }
    void NotifyPingChange(LobbyGame* inGame) { this->Call(EeWonStagingCall::NotifyPingChange, inGame); }
    SmartPtr<LobbyGame> mGame; // 0x40
    SmartPtr<LobbyClient> mMyClient; // 0x44
    SmartPtr<AsyncOp> mJoinOp; // 0x48
    SmartPtr<CreateGameDialog> mCreateGameDialog; // 0x58
private:
    EeWonStaging* mRust;
};
}
#endif
