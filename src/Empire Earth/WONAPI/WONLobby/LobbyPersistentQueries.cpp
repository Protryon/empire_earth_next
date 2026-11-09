
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
    if(data==NULL || data->mCurrentUserId==0)
        return NULL;
    for(LobbyUserInfoMap::iterator it=data->mUsers.begin();it!=data->mUsers.end();++it)
    {
        LobbyUserInfo& user=it->second;
        if(user.mUserId==data->mCurrentUserId)
            return &user;
    }
    return NULL;
}
// FUNCTION: WON 0x0075ce60
// Returns all saved login users by name (an empty map without lobby data)
const LobbyUserInfoMap& LobbyFn35CE60()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        return data->mUsers;
    static LobbyUserInfoMap empty;
    return empty;
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
    LobbyUserInfoMap::iterator it=data->mUsers.find(name);
    if(it==data->mUsers.end()) return NULL;
    return &it->second;
}
// FUNCTION: WON 0x0075d660
// Returns the recent LAN names with their last use times (an empty map without lobby data)
const LobbyLanNameInfoMap& LobbyFn35D660()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        return data->mLanNameInfoMap;
    static LobbyLanNameInfoMap empty;
    return empty;
}
// FUNCTION: WON 0x0075d8e0
// Returns the saved direct-connect addresses and names (an empty map without lobby data)
const LobbyDirectConnectMap& LobbyFn35D8E0()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        return data->mDirectConnectMap;
    static LobbyDirectConnectMap empty;
    return empty;
}
// FUNCTION: WON 0x0075dc80
// Returns the user's filter values for a game list column (an empty set if the column has none)
const LobbyOptionSet& LobbyFn35DC80(const GUIString& column)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) {
        std::map<GUIString,LobbyOptionSet,GUIStringLessNoCase>::iterator it=data->mGameOptions.find(column);
        if(it!=data->mGameOptions.end())
            return it->second;
    }
    static LobbyOptionSet empty;
    return empty;
}
// FUNCTION: WON 0x0075e470
// Sets the lobby tab to open at start, marking the user's preferences changed
void LobbySetTabSelect(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && name!=data->mTabSelect) {
        data->mTabSelect=name;
        data->mUserDirty=true;
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007682a0
// (public: __thiscall std::map<class WONAPI::IPAddr,class WONAPI::GUIString,struct std::less<class WONAPI::IPAddr>,class std::allocator<struct std::pair<)
// TEMPLATE: WON 0x0076d9f0
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<c)
