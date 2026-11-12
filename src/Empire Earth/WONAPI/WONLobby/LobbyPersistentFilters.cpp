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
#include "ee/wonapi/EELobbySound.h"
namespace WONAPI {
// FUNCTION: WON 0x0075e620
// Records the network adapter now in use (local host address), marking the global settings changed
void LobbySaveNetworkAdapter()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->SaveNetworkAdapter(WONAPI::IPAddr::GetLocalHost());
}
// FUNCTION: WON 0x0075dea0
// Adds a value to the user's filter for a game list column
void LobbyAddFilterValue(const GUIString& column,const GUIString& value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) {
        data->GameOptions()[column].Add(value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}
// FUNCTION: WON 0x0075ded0
// Removes a value from the user's filter for a game list column (an emptied column stays in the map)
void LobbyRemoveFilterValue(const GUIString& column,const GUIString& value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return;
    LobbyOptionSet& options=data->GameOptions()[column];
    if(options.Erase(value)) data->SetFlag(LobbyPersistentData::kUserDirty, true);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007670a0
// (public: class WONAPI::LobbyOptionSet & __thiscall std::map<class WONAPI::GUIString,class WONAPI::LobbyOptionSet,struct WONAPI::GUIStringLessNoCase,cla)
