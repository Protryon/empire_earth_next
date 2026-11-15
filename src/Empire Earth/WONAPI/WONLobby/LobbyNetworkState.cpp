// Lobby address state: firewall test result, the public address seen from outside, and the address to give peers
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
namespace WONAPI {
// Forgets the firewall test result and the public address
void LobbyResetFirewallTest()
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mFirewallTestDone=false;
    gLobbyData->mPublicAddr=IPAddr();
}
// Records the public address seen from outside
void LobbySetPublicAddress(const IPAddr& address)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mPublicAddr=address;
}
// Records whether the player is behind a firewall and marks the test done
void LobbySetBehindFirewall(bool behindFirewall)
{
    if(gLobbyData.get()==NULL) return;
    gLobbyData->mBehindFirewall=behindFirewall;
    gLobbyData->mFirewallTestDone=true;
}
// Returns the address to give peers on the lobby port: public unless room spec 1 or UseLocalIP, else local
IPAddr LobbyGetMyAddress(int roomSpec)
{
    IPAddr address;
    if(gLobbyData.get()!=NULL) {
        if(roomSpec!=1 && gLobbyData->mPublicAddr.IsValid() && !gLobbyData->mConfig->mUseLocalIP)
            address=gLobbyData->mPublicAddr;
        else
            address=IPAddr::GetLocalAddr();
        address.SetPort(gLobbyData->mLobbyPort);
    }
    return address;
}
// Returns the public address seen from outside, or an empty address
IPAddr LobbyGetPublicAddress()
{
    if(gLobbyData.get()!=NULL) return gLobbyData->mPublicAddr;
    return IPAddr();
}
}

