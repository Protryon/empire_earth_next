// Network adapter selection for the lobby; the saved adapter changes only after a successful switch
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
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
// FUNCTION: WON 0x0075b9c0
// Switches the local address to an adapter, saves the choice if new and rebinds the UDP port after a switch
void LobbySetNetworkAdapter(long address)
{
    bool changed=false;
    if(WONAPI::IPAddr::GetLocalHost()!=address) {
        changed=true;
        if(!IPAddr::SetLocalHost(address,true)) return;
    }
    if(WONAPI::IPAddr::GetLocalHost()!=LobbyGetNetworkAdapter()) LobbySaveNetworkAdapter();
    if(changed) LobbyBindUDPPort();
}
}
