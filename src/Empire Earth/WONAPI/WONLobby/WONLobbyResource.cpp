// The lobby's resources: their slots, the table by id, registration, load and unload. The 469 resources are the
// rows of ee_won's table (rust/crates/ee_won/src/resource.rs: each one's section, key and kind in the original's load
// order, EeWonLobbyRes their names); the original's named globals are one slot per row (LOBBY_STRING(name) etc.).
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
#include "ee/wonapi/WONLobbyResourceGlobals.h"
#include "rust/ee_won.h"
using namespace WONAPI;

// the original's globals (0x5663f4..0x566a9c) and its id table (0x566944..), one slot per row
LobbyResourceSlot gLobbyResources[EE_WON_LOBBYRES_COUNT];

// Registers the lobby's resource loader and control ids on the given (or a new) resource table and returns it
ResourceConfigTablePtr LobbyResource_Init(ResourceConfigTable* theTable)
{
    ResourceConfigTablePtr aTable=theTable;
    if(theTable==NULL)
        aTable=theTable=new ResourceConfigTable;
    theTable->RegisterGlueResourceFunc(LobbyResource_SafeGlueResources);
    theTable->RegisterControlId("ShowCreateAccount",ID_ShowCreateAccount);
    theTable->RegisterControlId("ShowLostPassword",ID_ShowLostPassword);
    theTable->RegisterControlId("ShowLostUserName",ID_ShowLostUserName);
    theTable->RegisterControlId("ShowMOTD",ID_ShowMOTD);
    theTable->RegisterControlId("ShowTOU",ID_ShowTOU);
    theTable->RegisterControlId("LoginScreenBack",ID_LoginScreenBack);
    theTable->RegisterControlId("ShowLoginHelp",ID_ShowLoginHelp);
    theTable->RegisterControlId("ShowInternetScreen",ID_ShowInternetScreen);
    theTable->RegisterControlId("ShowLanScreen",ID_ShowLanScreen);
    theTable->RegisterControlId("Logout",ID_Logout);
    theTable->RegisterControlId("HostGame",ID_HostGame);
    theTable->RegisterControlId("ChangeRoom",ID_ChangeRoom);
    theTable->RegisterControlId("JoinGame",ID_JoinGame);
    theTable->RegisterControlId("LeaveGame",ID_LeaveGame);
    theTable->RegisterControlId("StartGame",ID_StartGame);
    theTable->RegisterControlId("RefreshGameList",ID_RefreshGameList);
    theTable->RegisterControlId("AddFriend",ID_AddFriend);
    theTable->RegisterControlId("RefreshFriends",ID_RefreshFriends);
    theTable->RegisterControlId("AddLANName",ID_AddLANName);
    theTable->RegisterControlId("ChangeLANBroadcast",ID_ChangeLANBroadcast);
    theTable->RegisterControlId("FilterDirtyWordsCheck",ID_FilterDirtyWordsCheck);
    theTable->RegisterControlId("AnonymousToFriendsCheck",ID_AnonymousToFriendsCheck);
    theTable->RegisterControlId("ChooseNetworkAdapter",ID_ChooseNetworkAdapter);
    return aTable;
}

// The string and background getters fold to one body, the pointer and value getters to another.
// Returns the lobby string resource with the given id
const GUIString& LobbyResource_GetString(LobbyResourceId id) { return gLobbyResources[id].mString; }
// Returns the lobby image resource with the given id (the original's served sounds, fonts, colours and ints too)
Image* LobbyResource_GetImage(LobbyResourceId id) { return gLobbyResources[id].mImage.get(); }
Sound* LobbyResource_GetSound(LobbyResourceId id) { return gLobbyResources[id].mSound.get(); }

// Loads every lobby string, image and sound from its section of the resource table (ee_won's rows in order: a
// section is looked up again where it changes, as the original's statements did)
void LobbyResource_SafeGlueResources(ResourceConfigTable* theTable)
{
    ResourceConfig* aConfig = NULL;
    for (size_t i = 0; i < EE_WON_LOBBYRES_COUNT; ++i) {
        EeWonResource aRow;
        if (!ee_won_resource_row(i, &aRow))
            break;
        LobbyResourceSlot& aSlot = gLobbyResources[i];
        if (aRow.new_section)
            aConfig = theTable->SafeGetConfig(aRow.section);
        switch (aRow.kind) {
        case EeWonResKind::String: aSlot.mString = aConfig->SafeGetString(aRow.key); break;
        case EeWonResKind::Image: aSlot.mImage = aConfig->SafeGetImage(aRow.key); break;
        case EeWonResKind::Sound: aSlot.mSound = aConfig->SafeGetSound(aRow.key); break;
        }
    }
}

// Releases every lobby image and sound and empties every lobby string (ee_won's rows in order)
void LobbyResource_UnloadResources()
{
    for (size_t i = 0; i < EE_WON_LOBBYRES_COUNT; ++i) {
        LobbyResourceSlot& aSlot = gLobbyResources[i];
        switch (ee_won_lobbyres_kind((EeWonLobbyRes)i)) {
        case EeWonResKind::String: aSlot.mString.erase(); break;
        case EeWonResKind::Image: aSlot.mImage = NULL; break;
        case EeWonResKind::Sound: aSlot.mSound = NULL; break;
        }
    }
}
