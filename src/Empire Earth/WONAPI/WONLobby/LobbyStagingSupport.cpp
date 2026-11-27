// The lobby's global staging and network entry points: forward to the staging logic or the current game type's
// network logic (LAN or Internet)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyPing.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONCRC16.h"
#include "rust/won_lobby/netdisp.h"
#include "rust/won_lobby/lobbyprv.h"
namespace WONAPI {
// the network entry points' statements (netdisp.rs composes them, R6 WON round 32)
static intptr_t NetHost(EeWonNetDispOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonNetDispOp Op;
    NetLogic* logic = reinterpret_cast<NetLogic*>(a);
    switch (inOp) {
    case Op::HasData: return gLobbyData.get() != NULL;
    case Op::LanLogic: return (intptr_t) static_cast<NetLogic*>(gLobbyData->mLanLogic.get());
    case Op::RoutingLogic: return (intptr_t) static_cast<NetLogic*>(gLobbyData->mRoutingLogic.get());
    case Op::CurGameType: return (intptr_t)gLobbyData->mCurGameType;
    case Op::SetCurGameType: gLobbyData->mCurGameType = static_cast<LobbyGameType>(a); return 0;
    case Op::GameType: return (intptr_t)reinterpret_cast<LobbyGame*>(a)->GetGameType();
    case Op::HostGame: return logic->HostGame(reinterpret_cast<LobbyGame*>(b)) ? 1 : 0;
    case Op::JoinGame: return logic->JoinGame(reinterpret_cast<LobbyGame*>(b)) ? 1 : 0;
    case Op::MyClient: return (intptr_t)logic->GetMyClient();
    case Op::OnInternetScreen: return LobbyOnInternetScreen() ? 1 : 0;
    case Op::OnLanScreen: return LobbyOnLanScreen() ? 1 : 0;
    case Op::RoutingLogout: gLobbyData->mRoutingLogic->Logout(); return 0;
    case Op::LanLogout: gLobbyData->mLanLogic->Logout(); return 0;
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::AdvertiseGame: logic->AdvertiseGame(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::MyLobbyClient: return (intptr_t)GetMyLobbyClient();
    case Op::ClientPlayer: return (intptr_t)reinterpret_cast<LobbyClient*>(a)->GetPlayer().get();
    case Op::SendToCaptain: logic->SendToCaptain(reinterpret_cast<const ByteBuffer*>(b)); return 0;
    case Op::BroadcastToGame: logic->BroadcastToGame(reinterpret_cast<const ByteBuffer*>(b)); return 0;
    case Op::SendToClient: logic->SendToClient(reinterpret_cast<LobbyClient*>(b), reinterpret_cast<const ByteBuffer*>(c)); return 0;
    case Op::RemoveGameAdvertisement: logic->RemoveGameAdvertisement(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::LeaveGame: logic->LeaveGame(reinterpret_cast<LobbyGame*>(b)); return 0;
    case Op::KickClient: logic->KickClient(reinterpret_cast<LobbyClient*>(b), c != 0); return 0;
    case Op::SetGameInProgress: logic->SetGameInProgress(b != 0); return 0;
    case Op::Suspend: logic->Suspend(); return 0;
    case Op::Resume: logic->Resume(); return 0;
    }
    return 0;
}
// Makes the game's type current and hosts it on that network; returns whether hosting succeeded
bool LobbyNetHostGame(LobbyGame* game) { return ee_won_netdisp_call(NetHost, EeWonNetDispCall::HostGame, (intptr_t)game, 0) != 0; }
// Makes the game's type current and joins it on that network; returns whether joining succeeded
bool LobbyNetJoinGame(LobbyGame* game) { return ee_won_netdisp_call(NetHost, EeWonNetDispCall::JoinGame, (intptr_t)game, 0) != 0; }
// Returns our own client on the current network, or NULL
LobbyClient* LobbyGetNetMyClient() { return (LobbyClient*)ee_won_netdisp_call(NetHost, EeWonNetDispCall::MyClient, 0, 0); }
// Logs out of the Internet lobby or the LAN, whichever screen is showing
void LobbyNetLogout() { ee_won_netdisp_call(NetHost, EeWonNetDispCall::Logout, 0, 0); }
// Joins a game through the staging logic
void LobbyJoinGame(LobbyGame* game) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::JoinGame, game); }
// Hosts a new game through the staging logic
void LobbyHostGame() { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::HostGame); }
// Logs out through the staging logic; returns false when cancelled or without a lobby
bool LobbyLogout(bool confirm) { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Call(EeWonStagingCall::Logout, NULL, NULL, confirm)!=0:false; }
// Sets the port the lobby uses
void LobbySetLobbyPort(unsigned short port) { ee_won_lobbyprv_set(EeWonLobbyPrvField::LobbyPort, port); }
// Kicks or bans a client from our game
void LobbyKickClient(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::KickClient, NULL, client, ban); }
// Passes a game room message from a client to the staging logic
void LobbyHandleStagingMessage(const ByteBuffer* data,LobbyClient* client) { if(gLobbyData.get()!=NULL && data!=NULL) StagingHandleBytes(gLobbyData->mStagingLogic.get(), ReadBuffer(data->data(), data->length()), client); }
// Passes a game room message being read from a client to the staging logic
void LobbyHandleStagingReadBuffer(ReadBuffer& data,LobbyClient* client) { if(gLobbyData.get()!=NULL) StagingHandleMessage(gLobbyData->mStagingLogic.get(), data, client); }
// Leaves the game after losing the connection, optionally telling the player
void LobbyGameDisconnected(bool showError) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::HandleDisconnect, NULL, NULL, showError); }
// Removes a departed client's player from the game
void LobbyClientLeftGame(LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::HandleClientLeft, NULL, client); }
// Handles a client's kick or ban from the game
void LobbyClientKicked(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::HandleKicked, NULL, client, ban); }
// Enters the Internet lobby screen, optionally joining the default room
void LobbyEnterInternetScreen(bool joinDefault) { if(gLobbyData.get()!=NULL) gLobbyData->mRoutingLogic->Call(EeWonRouteLogicCall::EnterInternet, joinDefault); }
}

namespace WONAPI {
// Host: advertises our game on the current network
void LobbyNetAdvertiseGame() { ee_won_netdisp_call(NetHost, EeWonNetDispCall::AdvertiseGame, 0, 0); }
// Clears every player's ready flag in our game
void LobbyResetReadyFlags()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::ResetReadyFlags);
}
// Marks our game as in progress or not
void LobbySetGameInProgress(bool inProgress)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::SetGameInProgress, NULL, NULL, inProgress);
}
// Leaves the current game; returns false when cancelled or without a lobby
bool LobbyLeaveGame(bool confirm)
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Call(EeWonStagingCall::LeaveGame, NULL, NULL, confirm)!=0:false;
}
// Returns our own client in the current game, or NULL
LobbyClient* GetMyLobbyClient()
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Slot<LobbyClient>(kStagingMyClient):NULL;
}
// Tells the lobby screens that the game room changed
void LobbyNotifyStagingChange()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::NotifyStagingChange);
}
// Sends a game room message to the host
void LobbySendToCaptain(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->CallMessage(EeWonStagingCall::SendToCaptain, message, NULL);
}
// Host: sends a game room message to everyone in the game
void LobbyBroadcastToGame(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->CallMessage(EeWonStagingCall::BroadcastToGame, message, NULL);
}
// Host: sends a game room message to one client
void LobbySendToClient(LobbyClient* client,const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->CallMessage(EeWonStagingCall::SendToClient, message, client);
}
}

namespace WONAPI {
// (SDK LobbyStaging::GetGame)
// Returns the game we are in, or NULL
LobbyGame* GetMyLobbyGame() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Slot<LobbyGame>(kStagingGame):NULL; }
// (SDK LobbyStaging::IAmCaptain)
// Returns whether we host the current game
bool LobbyIAmCaptain() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Call(EeWonStagingCall::IAmCaptain)!=0:false; }
// (SDK LobbyStaging::IsBehindFirewall)
// Returns whether the firewall test found us behind a firewall
bool LobbyIsBehindFirewall() { return ee_won_lobbyprv_get(EeWonLobbyPrvField::BehindFirewall) != 0; }
// (SDK LobbyStaging::IsFirewallTestDone)
// Returns whether the firewall test has finished
bool LobbyIsFirewallTestDone() { return ee_won_lobbyprv_get(EeWonLobbyPrvField::FirewallTestDone) != 0; }
}

namespace WONAPI {
// (SDK LobbyStaging::GetMyPlayer)
// Returns our own player in the current game, or NULL
LobbyPlayer* GetMyLobbyPlayer() { return (LobbyPlayer*)ee_won_netdisp_call(NetHost, EeWonNetDispCall::MyPlayer, 0, 0); }
// (SDK LobbyStaging::IAmReady)
// Returns whether our own player is ready
bool LobbyIAmReady()
{
    return gLobbyData.get()!=NULL ? gLobbyData->mStagingLogic->Call(EeWonStagingCall::IAmReady) != 0 : false;
}
// (SDK LobbyStagingPrv network dispatch)
// Sends a message to the host over the current network
void LobbyNetSendToCaptain(const ByteBuffer* message) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::SendToCaptain, (intptr_t)message, 0); }
// Sends a message to everyone in the game over the current network
void LobbyNetBroadcastToGame(const ByteBuffer* message) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::BroadcastToGame, (intptr_t)message, 0); }
// Sends a message to one client over the current network
void LobbyNetSendToClient(LobbyClient* client,const ByteBuffer* message) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::SendToClient, (intptr_t)client, (intptr_t)message); }
// Withdraws a game's advertisement on its own network
void LobbyNetRemoveGameAdvertisement(LobbyGame* game) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::RemoveGameAdvertisement, (intptr_t)game, 0); }
// Leaves a game on its own network
void LobbyNetLeaveGame(LobbyGame* game) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::LeaveGame, (intptr_t)game, 0); }
// Kicks or bans a client over the current network
void LobbyNetKickClient(LobbyClient* client,bool inBan) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::KickClient, (intptr_t)client, inBan); }
// Tells the current network whether our game is in progress
void LobbyNetSetGameInProgress(bool inClose) { ee_won_netdisp_call(NetHost, EeWonNetDispCall::SetGameInProgress, inClose, 0); }
// Suspends the current network logic
void LobbyNetSuspend() { ee_won_netdisp_call(NetHost, EeWonNetDispCall::Suspend, 0, 0); }
// Resumes the current network logic
void LobbyNetResume() { ee_won_netdisp_call(NetHost, EeWonNetDispCall::Resume, 0, 0); }
// (SDK LobbyStagingPrv::NotifyPingChange)
// Reports our new ping to the host of a game we are in
void LobbyNotifyPingChange(LobbyGame* game)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->Call(EeWonStagingCall::NotifyPingChange, game);
}
}

