// Lobby shutdown (WON SDK entry point); the failure globals are defined elsewhere
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
extern SmartPtr<Dialog> gLobbyFailureDialog;
extern GUIString gLobbyInitError;
extern bool gLobbyInitFailed;

// FUNCTION: WON 0x00762170
// Clears the start-up failure state, then logs out, destroys the lobby, stops its music and unloads its resources
void LobbyShutdown()
{
    gLobbyFailureDialog=NULL;
    gLobbyInitFailed=false;
    gLobbyInitError=GetEmptyGUIStringData();
    if(gLobbyData.get()==NULL)
        return;
    LobbyLogout(false);
    gLobbyData->Destroy();
    LobbyRemoveEventHooks();
    ::LobbyGlobal_LobbyMusic_Sound->Stop();
    LobbyResource_UnloadResources();
    gLobbyData=NULL;
}
}
