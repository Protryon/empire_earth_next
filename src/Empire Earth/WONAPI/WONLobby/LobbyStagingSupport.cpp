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
// FUNCTION: WON 0x00761be0
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
// FUNCTION: WON 0x00761c10
// Returns the network logic for the current game type
NetLogic* LobbyGetCurNetLogic()
{
    if(gLobbyData.get()==NULL) return NULL;
    return LobbyGetNetLogic(gLobbyData->mCurGameType);
}
// FUNCTION: WON 0x00761c30
// Makes the game's type current and hosts it on that network; returns whether hosting succeeded
bool LobbyNetHostGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->mGameType);
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->HostGame(game);
    return false;
}
// FUNCTION: WON 0x00761c60
// Makes the game's type current and joins it on that network; returns whether joining succeeded
bool LobbyNetJoinGame(LobbyGame* game)
{
    if(gLobbyData.get()==NULL)
        return false;
    gLobbyData->mCurGameType=static_cast<LobbyGameType>(game->mGameType);
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) return logic->JoinGame(game);
    return false;
}
// FUNCTION: WON 0x00761c90
// Returns our own client on the current network, or NULL
LobbyClient* LobbyGetNetMyClient()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic==NULL) return NULL;
    return logic->GetMyClient();
}
// FUNCTION: WON 0x00761dd0
// Logs out of the Internet lobby or the LAN, whichever screen is showing
void LobbyNetLogout()
{
    if(gLobbyData.get()==NULL) return;
    if(LobbyOnInternetScreen()) gLobbyData->mRoutingLogic->Logout();
    else if(LobbyOnLanScreen()) gLobbyData->mLanLogic->Logout();
}
// FUNCTION: WON 0x007612d0
// Joins a game through the staging logic
void LobbyJoinGame(LobbyGame* game) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->JoinGame(game); }
// FUNCTION: WON 0x007612f0
// Hosts a new game through the staging logic
void LobbyHostGame() { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HostGame(); }
// FUNCTION: WON 0x00761330
// Logs out through the staging logic; returns false when cancelled or without a lobby
bool LobbyLogout(bool confirm) { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->Logout(confirm):false; }
// FUNCTION: WON 0x00761420
// Sets the port the lobby uses
void LobbySetLobbyPort(unsigned short port) { if(gLobbyData.get()!=NULL) gLobbyData->mLobbyPort=port; }
// FUNCTION: WON 0x00761bc0
// Kicks or bans a client from our game
void LobbyKickClient(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->KickClient(client,ban); }
// FUNCTION: WON 0x00761e10
// Passes a game room message from a client to the staging logic
void LobbyHandleStagingMessage(const ByteBuffer* data,LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleMessageBuffer(data,client); }
// FUNCTION: WON 0x00761e30
// Passes a game room message being read from a client to the staging logic
void LobbyHandleStagingReadBuffer(ReadBuffer& data,LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleMessage(data,client); }
// FUNCTION: WON 0x00761e50
// Leaves the game after losing the connection, optionally telling the player
void LobbyGameDisconnected(bool showError) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleDisconnect(showError); }
// FUNCTION: WON 0x00761e70
// Removes a departed client's player from the game
void LobbyClientLeftGame(LobbyClient* client) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleClientLeft(client); }
// FUNCTION: WON 0x00761e90
// Handles a client's kick or ban from the game
void LobbyClientKicked(LobbyClient* client,bool ban) { if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->HandleKicked(client,ban); }
// FUNCTION: WON 0x00761ed0
// Enters the Internet lobby screen, optionally joining the default room
void LobbyEnterInternetScreen(bool joinDefault) { if(gLobbyData.get()!=NULL) gLobbyData->mRoutingLogic->EnterInternetScreen(joinDefault); }
}

namespace WONAPI {
// FUNCTION: WON 0x00761260
// Host: advertises our game on the current network
void LobbyNetAdvertiseGame()
{
    if(!LobbyIAmCaptain()) return;
    NetLogic* logic=LobbyGetCurNetLogic();
    LobbyGame* game=GetMyLobbyGame();
    if(logic!=NULL && game!=NULL) logic->AdvertiseGame(game);
}
// FUNCTION: WON 0x00761290
// Clears every player's ready flag in our game
void LobbyResetReadyFlags()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->ResetReadyFlags();
}
// FUNCTION: WON 0x007612b0
// Marks our game as in progress or not
void LobbySetGameInProgress(bool inProgress)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SetGameInProgress(inProgress);
}
// FUNCTION: WON 0x00761310
// Leaves the current game; returns false when cancelled or without a lobby
bool LobbyLeaveGame(bool confirm)
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->LeaveGame(confirm):false;
}
// FUNCTION: WON 0x00761370
// Returns our own client in the current game, or NULL
LobbyClient* GetMyLobbyClient()
{
    return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->mMyClient.get():NULL;
}
// FUNCTION: WON 0x007613e0
// Tells the lobby screens that the game room changed
void LobbyNotifyStagingChange()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->NotifyStagingChange();
}
// FUNCTION: WON 0x00761b60
// Sends a game room message to the host
void LobbySendToCaptain(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SendToCaptain(message);
}
// FUNCTION: WON 0x00761b80
// Host: sends a game room message to everyone in the game
void LobbyBroadcastToGame(const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->BroadcastToGame(message);
}
// FUNCTION: WON 0x00761ba0
// Host: sends a game room message to one client
void LobbySendToClient(LobbyClient* client,const ByteBuffer* message)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->SendToClient(client,message);
}
}

namespace WONAPI {
// (SDK LobbyStaging::GetGame)
// FUNCTION: WON 0x00761350
// Returns the game we are in, or NULL
LobbyGame* GetMyLobbyGame() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->mGame.get():NULL; }
// (SDK LobbyStaging::IAmCaptain)
// FUNCTION: WON 0x007613c0
// Returns whether we host the current game
bool LobbyIAmCaptain() { return gLobbyData.get()!=NULL?gLobbyData->mStagingLogic->IAmCaptain():false; }
// (SDK LobbyStaging::IsBehindFirewall)
// FUNCTION: WON 0x00761b20
// Returns whether the firewall test found us behind a firewall
bool LobbyIsBehindFirewall() { return gLobbyData.get()!=NULL?gLobbyData->mBehindFirewall:false; }
// (SDK LobbyStaging::IsFirewallTestDone)
// FUNCTION: WON 0x00761b40
// Returns whether the firewall test has finished
bool LobbyIsFirewallTestDone() { return gLobbyData.get()!=NULL?gLobbyData->mFirewallTestDone:false; }
}

namespace WONAPI {
// (SDK LobbyStaging::GetMyPlayer)
// FUNCTION: WON 0x00761390
// Returns our own player in the current game, or NULL
LobbyPlayer* GetMyLobbyPlayer()
{
    LobbyClient* client=GetMyLobbyClient();
    return client!=NULL ? client->mPlayer.get() : NULL;
}
// (SDK LobbyStaging::IAmReady)
// FUNCTION: WON 0x007613a0
// Returns whether our own player is ready
bool LobbyIAmReady()
{
    return gLobbyData.get()!=NULL ? gLobbyData->mStagingLogic->IAmReady() : false;
}
// (SDK LobbyStagingPrv network dispatch)
// FUNCTION: WON 0x00761cb0
// Sends a message to the host over the current network
void LobbyNetSendToCaptain(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToCaptain(message);
}
// FUNCTION: WON 0x00761cd0
// Sends a message to everyone in the game over the current network
void LobbyNetBroadcastToGame(const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->BroadcastToGame(message);
}
// FUNCTION: WON 0x00761cf0
// Sends a message to one client over the current network
void LobbyNetSendToClient(LobbyClient* client,const ByteBuffer* message)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SendToClient(client,message);
}
// FUNCTION: WON 0x00761d10
// Withdraws a game's advertisement on its own network
void LobbyNetRemoveGameAdvertisement(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->mGameType));
    if(logic!=NULL) logic->RemoveGameAdvertisement(game);
}
// FUNCTION: WON 0x00761d30
// Leaves a game on its own network
void LobbyNetLeaveGame(LobbyGame* game)
{
    NetLogic* logic=LobbyGetNetLogic(static_cast<LobbyGameType>(game->mGameType));
    if(logic!=NULL) logic->LeaveGame(game);
}
// FUNCTION: WON 0x00761d50
// Kicks or bans a client over the current network
void LobbyNetKickClient(LobbyClient* client,bool inBan)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->KickClient(client,inBan);
}
// FUNCTION: WON 0x00761d70
// Tells the current network whether our game is in progress
void LobbyNetSetGameInProgress(bool inClose)
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->SetGameInProgress(inClose);
}
// FUNCTION: WON 0x00761d90
// Suspends the current network logic
void LobbyNetSuspend()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Suspend();
}
// FUNCTION: WON 0x00761db0
// Resumes the current network logic
void LobbyNetResume()
{
    NetLogic* logic=LobbyGetCurNetLogic();
    if(logic!=NULL) logic->Resume();
}
// (SDK LobbyStagingPrv::NotifyPingChange)
// FUNCTION: WON 0x00761eb0
// Reports our new ping to the host of a game we are in
void LobbyNotifyPingChange(LobbyGame* game)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mStagingLogic->NotifyPingChange(game);
}
}

