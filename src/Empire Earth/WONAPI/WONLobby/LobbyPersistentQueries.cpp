
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
// FUNCTION: WON 0x0075cde0
// Returns the saved entry of the logged-in user, or NULL for the LAN user or an unknown id
LobbyUserInfo* LobbyGetCurrentUser()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL || data->GetCurrentUserId()==0)
        return NULL;
    for(size_t it=0;it<data->Users().size();++it)
    {
        LobbyUserInfo& user=data->Users().At(it);
        if(user.mUserId==data->GetCurrentUserId())
            return &user;
    }
    return NULL;
}
// FUNCTION: WON 0x0075ce60
// The saved login users by name (none without lobby data): the count, a name, its last use
size_t LobbyGetUserCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().size():0;
}
GUIString LobbyGetUserNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().KeyAt(inPos):gEmptyGUIString;
}
long LobbyGetUserLastUseAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Users().At(inPos).mLastUseTime:0;
}
// FUNCTION: WON 0x0075cec0
// Returns the saved password of a login user, or an empty string if the user is unknown
GUIString LobbyGetUserPassword(const GUIString& name)
{
    LobbyUserInfo* info=LobbyFindUser(name);
    if(info!=NULL) return info->mPassword;
    return gEmptyGUIString;
}
// FUNCTION: WON 0x0075cf20
// Returns the saved login user with the given name, or NULL
LobbyUserInfo* LobbyFindUser(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL)
        return NULL;
    return data->Users().Get(name);
}
// FUNCTION: WON 0x0075d660
// The recent LAN names (none without lobby data), in their order
size_t LobbyGetLanNameCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->LanNameInfoMap().size():0;
}
GUIString LobbyGetLanNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->LanNameInfoMap().KeyAt(inPos):gEmptyGUIString;
}
// FUNCTION: WON 0x0075d8e0
// The saved direct-connect addresses and names (none without lobby data), by address
size_t LobbyGetDirectConnectCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().size():0;
}
IPAddr LobbyGetDirectConnectAddressAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().KeyAt(inPos):IPAddr();
}
GUIString LobbyGetDirectConnectNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->DirectConnectMap().TextAt(inPos):gEmptyGUIString;
}
// FUNCTION: WON 0x0075dc80
// The user's filter values for a game list column (empty if the column has none); a copy
LobbyOptionSet LobbyGetFilterValues(const GUIString& column)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) {
        LobbyOptionSet* anOptions=data->GameOptions().Get(column);
        if(anOptions!=NULL)
            return *anOptions;
    }
    return LobbyOptionSet();
}
// FUNCTION: WON 0x0075e470
// Sets the lobby tab to open at start, marking the user's preferences changed
void LobbySetTabSelect(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeTabSelect(name);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007682a0
// (public: __thiscall std::map<class WONAPI::IPAddr,class WONAPI::GUIString,struct std::less<class WONAPI::IPAddr>,class std::allocator<struct std::pair<)
// TEMPLATE: WON 0x0076d9f0
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<c)
