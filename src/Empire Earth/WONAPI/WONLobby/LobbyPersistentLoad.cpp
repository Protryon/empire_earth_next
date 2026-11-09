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
namespace WONAPI {
// FUNCTION: WON 0x0075e640
// Loads the login users and global lobby settings from _wonlobbypersistent.dat
void LobbyLoadGlobalPrefs()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) data->LoadGlobalPrefs();
}
}
