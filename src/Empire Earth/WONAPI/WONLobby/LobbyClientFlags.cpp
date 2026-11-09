// Blocks or ignores players in the lobby lists
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
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
// Saves a player name's ignored state in the persistent settings (0x75d100)
void LobbySetNameIgnored(const GUIString&,bool);
// FUNCTION: WON 0x0075b850
// Blocks or unblocks a player and refreshes the player lists; does nothing unless the config's mCDKeyCheck is set
void LobbySetClientBlocked(LobbyClient* client,bool inBlocked,unsigned int roomSpec)
{
    WONLobbyPrv* data=gLobbyData.get();
    if(data==NULL || !data->mConfig->mCDKeyCheck) return;
    if(client->IsBlocked()==inBlocked) return;
    client->mBlocked=inBlocked;
    LobbyBroadcastEvent(new ClientChangedEvent(client,roomSpec,2),true);
}
// FUNCTION: WON 0x0075b7b0
// Ignores or unignores a player, remembers the name in the settings and refreshes the player lists
void LobbySetClientIgnored(LobbyClient* client,bool inIgnored,unsigned int roomSpec)
{
    if(gLobbyData.get()==NULL) return;
    if(client->IsIgnored()==inIgnored) return;
    client->mIgnored=inIgnored;
    LobbySetNameIgnored(client->mName,inIgnored);
    LobbyBroadcastEvent(new ClientChangedEvent(client,roomSpec,2),true);
}

}
