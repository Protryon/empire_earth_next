
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
// FUNCTION: WON 0x0075cf90
// Makes the given saved user (or the LAN user for NULL) current and loads that user's preferences
void LobbySetCurrentUser(LobbyUserInfo* user)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL)
        return;
    if(user==NULL) data->mCurrentUserId=0;
    else data->mCurrentUserId=user->mUserId;
    data->LoadUserPrefs();
}

// FUNCTION: WON 0x0075cfc0
// Returns the current user's ignored player names (an empty set without lobby data)
const LobbyIgnoredNameSet& LobbyFn35CFC0()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->mIgnoredNames;
    static LobbyIgnoredNameSet empty;
    return empty;
}

// FUNCTION: WON 0x0075d010
// Returns whether a player name is on the current user's ignore list
bool LobbyIsNameIgnored(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    LobbyIgnoredNameSet::iterator it=data->mIgnoredNames.find(name);
    return it!=data->mIgnoredNames.end();
}

// FUNCTION: WON 0x0075d090
// Returns whether a CD key id is in the saved blocked set (loaded and saved but never filled by the lobby)
bool LobbyIsCDKeyBlocked(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    std::set<unsigned long>::iterator it=data->mBlockedCDKeyIds.find(id);
    return it!=data->mBlockedCDKeyIds.end();
}

// FUNCTION: WON 0x0075d270
// Returns the current user's friend list (an empty map without lobby data)
const LobbyFriendMap& LobbyFn35D270()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->mFriends;
    static LobbyFriendMap empty;
    return empty;
}

// FUNCTION: WON 0x0075d3f0
// Returns the friend entry for a user id, or NULL if that user is not a friend
LobbyFriend* LobbyFindFriend(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return NULL;
    LobbyFriendMap::iterator it=data->mFriends.find(id);
    if(it==data->mFriends.end()) return NULL;
    return it->second.get();
}

// FUNCTION: WON 0x0075d640
// Returns the current LAN player name
const GUIString& LobbyGetLanName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mLanName:gEmptyGUIString;
}

// FUNCTION: WON 0x0075dc40
// Returns the name of the server the user last joined
const GUIString& LobbyGetLastServerName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mLastServerName:gEmptyGUIString;
}

// FUNCTION: WON 0x0075dc60
// Returns the name of the room the user last joined
const GUIString& LobbyGetLastRoomName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mLastRoomName:gEmptyGUIString;
}

// FUNCTION: WON 0x0075e2d0
// Returns the name of the lobby tab to open at start (Quick Play, LAN ...)
const GUIString& LobbyGetTabSelect()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mTabSelect:gEmptyGUIString;
}

// FUNCTION: WON 0x0075e2f0
// Returns whether chat profanity filtering is on
bool LobbyGetFilterDirtyWords()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mFilterDirtyWords:false;
}

// FUNCTION: WON 0x0075e300
// Returns whether the user is marked away automatically when idle
bool LobbyGetDoAutoAFK()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mDoAutoAFK:false;
}

// FUNCTION: WON 0x0075e310
// Returns whether cross-promotion messages are shown
bool LobbyGetCrossPromotion()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mCrossPromotion:false;
}

// FUNCTION: WON 0x0075e320
// Returns whether lobby sound effects are on
bool LobbyGetLobbySoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mLobbySoundEffects:false;
}

// FUNCTION: WON 0x0075e330
// Returns whether chat sound effects are on
bool LobbyGetChatSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mChatSoundEffects:false;
}

// FUNCTION: WON 0x0075e340
// Returns whether staging-room sound effects are on
bool LobbyGetStagingSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mStagingSoundEffects:false;
}

// FUNCTION: WON 0x0075e350
// Returns whether lobby music is on
bool LobbyGetLobbyMusic()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mLobbyMusic:false;
}

// FUNCTION: WON 0x0075e360
// Returns whether confirmation dialogs are shown
bool LobbyGetShowConfirmations()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mShowConfirmations:false;
}

// FUNCTION: WON 0x0075e370
// Returns whether listed games are pinged automatically
bool LobbyGetAutoPingGames()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mAutoPingGames:false;
}

// FUNCTION: WON 0x0075e380
// Returns whether games already in progress appear in the game list
bool LobbyGetShowGamesInProgress()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mShowGamesInProgress:false;
}

// FUNCTION: WON 0x0075e390
// Returns whether the user hides their location from friends
bool LobbyGetAnonymousToFriends()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mAnonymousToFriends:false;
}

// FUNCTION: WON 0x0075e3a0
// Returns the user's connection speed setting (50 without lobby data)
unsigned short LobbyGetConnectionSpeed()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->mConnectionSpeed;
    return 50;
}

// FUNCTION: WON 0x0075e3c0
// Returns the LAN broadcast port (8585 without lobby data)
unsigned short LobbyGetLanBroadcastPort()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->mLanBroadcastPort;
    return 8585;
}

// FUNCTION: WON 0x0075e3e0
// Returns the saved network adapter's IPv4 address (0 for the default)
long LobbyGetNetworkAdapter()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->mNetworkAdapter:0;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0076c580
// (public: class std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCase,)
// TEMPLATE: WON 0x0076ca70
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStri)
// TEMPLATE: WON 0x0076cab0
// (protected: struct std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoC)
// TEMPLATE: WON 0x0076cb00
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::LobbyFriend> >,struct std::_Mapk<uns)
// TEMPLATE: WON 0x00767050
// (public: __thiscall std::set<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCase,class std::allocator<class WONAPI::GUIString> >::set<class WONA)
