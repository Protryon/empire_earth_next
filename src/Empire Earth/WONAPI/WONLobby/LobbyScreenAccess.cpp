
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
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyLan.h"
namespace WONAPI {
// FUNCTION: WON 0x0075b730
// Returns the chat command parser, pointed at the user list of the Internet or LAN screen showing
ChatCommandParser* LobbyGetRoomChatParser(int flags)
{
    if(gLobbyData.get()==NULL) return NULL;
    ChatCommandParser* parser=gLobbyData->mChatCommandParser.get();
    LobbyClientList* clients=NULL;
    if(gLobbyData->mLobbyContainer->OnInternetScreen())
        clients=gLobbyData->mRoutingLogic->GetClientList();
    else if(gLobbyData->mLobbyContainer->OnLanScreen())
        clients=gLobbyData->mLanLogic->GetClientList();
    parser->SetClientList(clients,flags);
    return parser;
}
// FUNCTION: WON 0x00760ea0
// Returns the Internet, LAN or login screen, whichever is showing, or NULL
Component* LobbyGetCurrentScreen()
{
    if(gLobbyData.get()==NULL) return NULL;
    if(LobbyOnInternetScreen()) return gLobbyData->mLobbyContainer->mInternetScreen.get();
    if(LobbyOnLanScreen()) return gLobbyData->mLobbyContainer->mLanScreen.get();
    if(LobbyOnLoginScreen()) return gLobbyData->mLobbyContainer->mLoginScreen.get();
    return NULL;
}
// FUNCTION: WON 0x00760f00
// Switches the lobby to the Internet screen and tells its controls they were entered
void LobbyShowInternetScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowInternetScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mInternetScreen->BroadcastEventDown(event);
}
// FUNCTION: WON 0x00760f90
// Switches the lobby to the LAN screen and tells its controls they were entered
void LobbyShowLanScreen()
{
    if(gLobbyData.get()==NULL)
        return;
    gLobbyData->mLobbyContainer->ShowLanScreen();
    LobbyEvent* event=new LobbyEvent(100014);
    gLobbyData->mLobbyContainer->mLanScreen->BroadcastEventDown(event);
}
// FUNCTION: WON 0x007611b0
// Returns the lobby's root container, or NULL before the lobby exists
Container* GetLobbyRoot()
{
    return gLobbyData.get()!=NULL?gLobbyData->mLobbyContainer:SmartPtr<LobbyContainer>();
}
}
