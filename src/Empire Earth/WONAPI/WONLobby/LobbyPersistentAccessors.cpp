
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
// Makes the given saved user (or the LAN user for NULL) current and loads that user's preferences
void LobbySetCurrentUser(LobbyUserInfo* user)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL)
        return;
    if(user==NULL) data->SetCurrentUserId(0);
    else data->SetCurrentUserId(user->mUserId);
    data->LoadUserPrefs();
}

// The current user's ignored player names (none without lobby data), in their order
size_t LobbyGetIgnoredNameCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->IgnoredNames().size():0;
}
GUIString LobbyGetIgnoredNameAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->IgnoredNames().KeyAt(inPos):gEmptyGUIString;
}

// Returns whether a player name is on the current user's ignore list
bool LobbyIsNameIgnored(const GUIString& name)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    return data->IgnoredNames().Contains(name);
}

// Returns whether a CD key id is in the saved blocked set (loaded and saved but never filled by the lobby)
bool LobbyIsCDKeyBlocked(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return false;
    return data->BlockedCDKeyIds().Contains(id);
}

// The current user's friends (none without lobby data), by user id
size_t LobbyGetFriendCount()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Friends().size():0;
}
LobbyFriend* LobbyGetFriendAt(size_t inPos)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->Friends().At(inPos):NULL;
}

// Returns the friend entry for a user id, or NULL if that user is not a friend
LobbyFriend* LobbyFindFriend(unsigned long id)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL) return NULL;
    return data->Friends().Get(id);
}

// Returns the current LAN player name
GUIString LobbyGetLanName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLanName():gEmptyGUIString;
}

// Returns the name of the server the user last joined
GUIString LobbyGetLastServerName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLastServerName():gEmptyGUIString;
}

// Returns the name of the room the user last joined
GUIString LobbyGetLastRoomName()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetLastRoomName():gEmptyGUIString;
}

// Returns the name of the lobby tab to open at start (Quick Play, LAN ...)
GUIString LobbyGetTabSelect()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetTabSelect():gEmptyGUIString;
}

// Returns whether chat profanity filtering is on
bool LobbyGetFilterDirtyWords()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kFilterDirtyWords):false;
}

// Returns whether the user is marked away automatically when idle
bool LobbyGetDoAutoAFK()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kDoAutoAFK):false;
}

// Returns whether cross-promotion messages are shown
bool LobbyGetCrossPromotion()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kCrossPromotion):false;
}

// Returns whether lobby sound effects are on
bool LobbyGetLobbySoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kLobbySoundEffects):false;
}

// Returns whether chat sound effects are on
bool LobbyGetChatSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kChatSoundEffects):false;
}

// Returns whether staging-room sound effects are on
bool LobbyGetStagingSoundEffects()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kStagingSoundEffects):false;
}

// Returns whether lobby music is on
bool LobbyGetLobbyMusic()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kLobbyMusic):false;
}

// Returns whether confirmation dialogs are shown
bool LobbyGetShowConfirmations()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kShowConfirmations):false;
}

// Returns whether listed games are pinged automatically
bool LobbyGetAutoPingGames()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kAutoPingGames):false;
}

// Returns whether games already in progress appear in the game list
bool LobbyGetShowGamesInProgress()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kShowGamesInProgress):false;
}

// Returns whether the user hides their location from friends
bool LobbyGetAnonymousToFriends()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetFlag(LobbyPersistentData::kAnonymousToFriends):false;
}

// Returns the user's connection speed setting (50 without lobby data)
unsigned short LobbyGetConnectionSpeed()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->GetConnectionSpeed();
    return 50;
}

// Returns the LAN broadcast port (8585 without lobby data)
unsigned short LobbyGetLanBroadcastPort()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL) return data->GetLanBroadcastPort();
    return 8585;
}

// Returns the saved network adapter's IPv4 address (0 for the default)
long LobbyGetNetworkAdapter()
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    return data!=NULL?data->GetNetworkAdapter():0;
}
}
