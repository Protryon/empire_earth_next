
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
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {
// FUNCTION: WON 0x00758df0
// Opens a popup over the lobby at the given position and lets it receive lobby logic events
void LobbyOpenPopup(Component* popup,int flags,int x,int y)
{
    if(gLobbyData.get()==NULL)
        return;
    Window* window=LobbyGetRootWindow();
    if(window!=NULL) {
        LobbyAddLogicListener(popup);
        window->OpenPopup(PopupParams(popup,flags,gLobbyData->mLobbyContainer.get(),x,y));
    }
}
}
