// Lobby start-up: creates the lobby singleton and records why its initialization failed
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
// The lobby singleton and its initialization failure state.
SmartPtr<WONLobbyPrv> gLobbyData;
GUIString gLobbyInitError;
SmartPtr<Dialog> gLobbyFailureDialog;
bool gLobbyInitFailed;
void LobbyInstallEventHooks(); // defined in another unit

// Creates and initializes the lobby; on failure keeps its error text, drops it and returns false
bool LobbyAddComponentLoader(ResourceConfigTable* table,void (*initFunc)(ComponentConfig*))
{
    gLobbyInitError=GetEmptyGUIStringData();
    gLobbyInitFailed=false;
    gLobbyFailureDialog=NULL;
    gLobbyData=new WONLobbyPrv;
    if(!gLobbyData->Init(table,initFunc)) {
        if(WONAPI::WindowManager::GetDefaultWindowManager()!=NULL)
            WONAPI::WindowManager::GetDefaultWindowManager()->ClearSubscriptions();
        gLobbyInitError=gLobbyData->mInitError;
        gLobbyInitFailed=true;
        gLobbyData=NULL;
        return false;
    }
    LobbyInstallEventHooks();
    return true;
}
}

namespace WONAPI {
// The SDK's WONLobby::InitAuth (26 bytes): passes the login community and the hash file list to the lobby
// when it is running
void LobbySetProductName(const std::wstring& inLoginCommunity,const char* inHashFileListStr)
{
    if(gLobbyData.get()!=NULL) gLobbyData->InitAuth(inLoginCommunity,inHashFileListStr);
}
}
