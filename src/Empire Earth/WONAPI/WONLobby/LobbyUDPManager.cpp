// Binds the lobby's UDP port (pings and friend searches)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"

namespace WONAPI {
// FUNCTION: WON 0x0075ba00
// Binds the lobby UDP port from the config (8888 by default), else a random port 10000-19999, and starts receiving
void LobbyBindUDPPort()
{
    LobbyConfig* aConfig=GetLobbyConfig();
    unsigned short aPort=aConfig?aConfig->mLobbyPort:8888;
    LobbySetLobbyPort(aPort);
    UDPManager* aMgr=LobbyGetUDPManager();
    if(aMgr==NULL)
        return;
    WONStatus aStatus=aMgr->Bind(aPort,true);
    if(aStatus!=0) {
        srand(time(NULL));
        aPort=rand()%10000+10000;
        aStatus=aMgr->Bind(aPort,false);
        LobbySetLobbyPort(aPort);
    }
    if(aStatus==0)
        aMgr->SetAutoRecv(true);
}
}
