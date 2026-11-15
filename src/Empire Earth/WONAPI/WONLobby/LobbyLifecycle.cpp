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

// FUNCTION: WON 0x00761fb0
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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00762150
// (void __cdecl WONAPI::LobbySetProductName(class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned )

// Dynamic initializers and exit handlers of the globals above:
// TEMPLATE: WON 0x00761ef0
// TEMPLATE: WON 0x00761f00
// TEMPLATE: WON 0x00761f10
// TEMPLATE: WON 0x00761f20
// TEMPLATE: WON 0x00761f30
// TEMPLATE: WON 0x00761f40
// TEMPLATE: WON 0x00761f50
// TEMPLATE: WON 0x00761f60
// TEMPLATE: WON 0x00761f70
// TEMPLATE: WON 0x00761f80
// TEMPLATE: WON 0x00761f90
// TEMPLATE: WON 0x00761fa0
