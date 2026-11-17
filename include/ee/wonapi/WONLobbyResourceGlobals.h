// Lobby resources (strings, images, sounds), their IDs and the control IDs; global scope as in the SDK
#ifndef EE_WONAPI_WONLOBBYRESOURCEGLOBALS_H
#define EE_WONAPI_WONLOBBYRESOURCEGLOBALS_H
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/won_lobby/resource.h"

// The lobby's 469 resources (the original's named globals and their ids) are the rows of ee_won's table
// (rust/crates/ee_won_lobby/src/resource.rs: EeWonLobbyRes names them in order); one slot each, read by name through these
// macros (EeWonLobbyRes is rust/ee_won.h's, included here as tools/rust_includes.py asks)
struct LobbyResourceSlot {
    WONAPI::GUIString mString;
    WONAPI::SmartPtr<WONAPI::Image> mImage;
    WONAPI::SmartPtr<WONAPI::Sound> mSound;
};
extern LobbyResourceSlot gLobbyResources[];
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)
#define LOBBY_IMAGE(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mImage)
#define LOBBY_SOUND(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mSound)

enum LobbyResourceCtrlId {
    LobbyResourceCtrlId_Min=200000, // ControlId_UserMin; the first control ID is 200001
    ID_ShowCreateAccount,
    ID_ShowLostPassword,
    ID_ShowLostUserName,
    ID_ShowMOTD,
    ID_ShowTOU,
    ID_LoginScreenBack,
    ID_ShowLoginHelp,
    ID_ShowInternetScreen,
    ID_ShowLanScreen,
    ID_Logout,
    ID_HostGame,
    ID_ChangeRoom,
    ID_JoinGame,
    ID_LeaveGame,
    ID_StartGame,
    ID_RefreshGameList,
    ID_AddFriend,
    ID_RefreshFriends,
    ID_AddLANName,
    ID_ChangeLANBroadcast,
    ID_FilterDirtyWordsCheck,
    ID_AnonymousToFriendsCheck,
    ID_ChooseNetworkAdapter,
};

typedef int LobbyResourceId; // a row of EeWonLobbyRes

WONAPI::ResourceConfigTablePtr LobbyResource_Init(WONAPI::ResourceConfigTable* = NULL);
const WONAPI::GUIString& LobbyResource_GetString(LobbyResourceId);
WONAPI::Image* LobbyResource_GetImage(LobbyResourceId);
WONAPI::Sound* LobbyResource_GetSound(LobbyResourceId);
void LobbyResource_SafeGlueResources(WONAPI::ResourceConfigTable*);
void LobbyResource_UnloadResources();

#endif
