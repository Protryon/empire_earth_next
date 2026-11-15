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
#include "ee/wonapi/EELobbySound.h"
namespace WONAPI {
// Returns the network logic for a game type: LAN for LAN and direct-connect games, routing for Internet
NetLogic* LobbyGetNetLogic(LobbyGameType type)
{
    if(gLobbyData.get()==NULL) return NULL;
    switch(static_cast<int>(type)) {
    case 1: case 3: return gLobbyData->mLanLogic.get();
    case 2: return gLobbyData->mRoutingLogic.get();
    default: return NULL;
    }
}
// Returns the network logic for the current game type
NetLogic* LobbyGetCurNetLogic()
{
    if(gLobbyData.get()==NULL) return NULL;
    return LobbyGetNetLogic(gLobbyData->mCurGameType);
}
// Makes the game's type current and hosts it on that network; returns whether hosting succeeded
bool LobbyNetHostGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->GetGameType());
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->HostGame(game);
    return false;
}
// Makes the game's type current and joins it on that network; returns whether joining succeeded
bool LobbyNetJoinGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->GetGameType());
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->JoinGame(game);
    return false;
}
// Returns our own client on the current network, or NULL
LobbyClient* LobbyGetNetMyClient()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic==NULL) return NULL;
    return logic->GetMyClient();
}
// Logs out of the Internet lobby or the LAN, whichever screen is showing
void LobbyNetLogout()
{
    if(gLobbyData.get()==NULL) return;
    if(LobbyOnInternetScreen()) gLobbyData->mRoutingLogic->Logout();
    else if(LobbyOnLanScreen()) gLobbyData->mLanLogic->Logout();
}
// Joins a game through the staging logic
void LobbyJoinGame(LobbyGame* game) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->JoinGame(game); }
// Hosts a new game through the staging logic
void LobbyHostGame() { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HostGame(); }
// Logs out through the staging logic; returns false when cancelled or without a lobby
bool LobbyLogout(bool confirm) { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Logout(confirm):false; }
// Sets the port the lobby uses
void LobbySetLobbyPort(unsigned short port) { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyPort=port; }
// Kicks or bans a client from our game
void LobbyKickClient(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->KickClient(client,ban); }
// Passes a game room message from a client to the staging logic
void LobbyHandleStagingMessage(const ByteBuffer* data,LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleMessageBuffer(data,client); }
// Passes a game room message being read from a client to the staging logic
void LobbyHandleStagingReadBuffer(ReadBuffer& data,LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleMessage(data,client); }
// Leaves the game after losing the connection, optionally telling the player
void LobbyGameDisconnected(bool showError) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleDisconnect(showError); }
// Removes a departed client's player from the game
void LobbyClientLeftGame(LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleClientLeft(client); }
// Handles a client's kick or ban from the game
void LobbyClientKicked(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleKicked(client,ban); }
// Enters the Internet lobby screen, optionally joining the default room
void LobbyEnterInternetScreen(bool joinDefault) { if(gLobbyData.get()!=NULL) gLobbyData->mRoutingLogic->EnterInternetScreen(joinDefault); }
}

namespace WONAPI {
// Host: advertises our game on the current network
void LobbyNetAdvertiseGame()
{
    if(!LobbyIAmCaptain()) return;
    NetLogic* logic=LobbyGetCurNetLogic();
    LobbyGame* game=GetMyLobbyGame();
    if(logic!=NULL && game!=NULL) logic->AdvertiseGame(game);
}
// Clears every player's ready flag in our game
void LobbyResetReadyFlags()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->ResetReadyFlags();
}
// Marks our game as in progress or not
void LobbySetGameInProgress(bool inProgress)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SetGameInProgress(inProgress);
}
// Leaves the current game; returns false when cancelled or without a lobby
bool LobbyLeaveGame(bool confirm)
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->LeaveGame(confirm):false;
}
// Returns our own client in the current game, or NULL
LobbyClient* GetMyLobbyClient()
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->mMyClient.get():NULL;
}
// Tells the lobby screens that the game room changed
void LobbyNotifyStagingChange()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->NotifyStagingChange();
}
// Sends a game room message to the host
void LobbySendToCaptain(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SendToCaptain(message);
}
// Host: sends a game room message to everyone in the game
void LobbyBroadcastToGame(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->BroadcastToGame(message);
}
// Host: sends a game room message to one client
void LobbySendToClient(LobbyClient* client,const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SendToClient(client,message);
}
}

namespace WONAPI {
// (SDK LobbyStaging::GetGame)
// Returns the game we are in, or NULL
LobbyGame* GetMyLobbyGame() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->mGame.get():NULL; }
// (SDK LobbyStaging::IAmCaptain)
// Returns whether we host the current game
bool LobbyIAmCaptain() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->IAmCaptain():false; }
// (SDK LobbyStaging::IsBehindFirewall)
// Returns whether the firewall test found us behind a firewall
bool LobbyIsBehindFirewall() { return gLobbyData.get()!=NULL?gLobbyData->mBehindFirewall:false; }
// (SDK LobbyStaging::IsFirewallTestDone)
// Returns whether the firewall test has finished
bool LobbyIsFirewallTestDone() { return gLobbyData.get()!=NULL?gLobbyData->mFirewallTestDone:false; }
}

namespace WONAPI {
// (SDK LobbyStaging::GetMyPlayer)
// Returns our own player in the current game, or NULL
LobbyPlayer* GetMyLobbyPlayer()
{
    LobbyClient* client=GetMyLobbyClient();
    return client!=NULL ? client->GetPlayer().get() : NULL;
}
// (SDK LobbyStaging::IAmReady)
// Returns whether our own player is ready
bool LobbyIAmReady()
{
    return gLobbyData.get()!=NULL ? gLobbyData->mStagingLogic->IAmReady() : false;
}
// (SDK LobbyStagingPrv network dispatch)
// Sends a message to the host over the current network
void LobbyNetSendToCaptain(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToCaptain(message);
}
// Sends a message to everyone in the game over the current network
void LobbyNetBroadcastToGame(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->BroadcastToGame(message);
}
// Sends a message to one client over the current network
void LobbyNetSendToClient(LobbyClient* client,const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToClient(client,message);
}
// Withdraws a game's advertisement on its own network
void LobbyNetRemoveGameAdvertisement(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->GetGameType()));
    if(logic!=NULL) logic->RemoveGameAdvertisement(game);
}
// Leaves a game on its own network
void LobbyNetLeaveGame(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->GetGameType()));
    if(logic!=NULL) logic->LeaveGame(game);
}
// Kicks or bans a client over the current network
void LobbyNetKickClient(LobbyClient* client,bool inBan)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->KickClient(client,inBan);
}
// Tells the current network whether our game is in progress
void LobbyNetSetGameInProgress(bool inClose)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SetGameInProgress(inClose);
}
// Suspends the current network logic
void LobbyNetSuspend()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Suspend();
}
// Resumes the current network logic
void LobbyNetResume()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Resume();
}
// (SDK LobbyStagingPrv::NotifyPingChange)
// Reports our new ping to the host of a game we are in
void LobbyNotifyPingChange(LobbyGame* game)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->NotifyPingChange(game);
}
}

