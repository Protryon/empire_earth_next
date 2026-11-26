// Binds the lobby's UDP port (pings and friend searches)
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
#include "rust/won_net/udpmgr.h"

namespace WONAPI {
// Binds the lobby UDP port from the config (8888 by default; + the harness slot's offset, TEST ONLY), else a random
// port 10000-19999, and starts receiving (ee_won udpmgr.rs ee_won_udpmanager_bind_lobby)
void LobbyBindUDPPort()
{
    LobbyConfig* aConfig=GetLobbyConfig();
    unsigned short aPort=(unsigned short)((aConfig?aConfig->mLobbyPort:8888)+LobbyTestPortOffset());
    UDPManager* aMgr=LobbyGetUDPManager();
    LobbySetLobbyPort(ee_won_udpmanager_bind_lobby(aMgr!=NULL?aMgr->GetManagerObject():NULL,aPort));
}
}
