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
// the slots bound to the Rust objects, which read a string's GUIStringData in place (resource.rs ee_won_lobbyres_bind;
// R6 WONLobby round 17): once, wherever the array is defined (the game's WONLobbyResource.cpp or a driver)
static_assert(sizeof(WONAPI::GUIString) == sizeof(void*), "resource.rs string_data: a slot's GUIString is its data pointer");
inline const bool gLobbyResourcesBound = (ee_won_lobbyres_bind(gLobbyResources, sizeof(LobbyResourceSlot)), true);
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)
#define LOBBY_IMAGE(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mImage)
#define LOBBY_SOUND(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mSound)

// the lobby's control numbers (from 200001): Rust's constants since R6 WON lobby and network round 35
#include "rust/won_lobby/lobbyids.h"

typedef int LobbyResourceId; // a row of EeWonLobbyRes

WONAPI::ResourceConfigTablePtr LobbyResource_Init(WONAPI::ResourceConfigTable* = NULL);
void LobbyResource_SafeGlueResources(WONAPI::ResourceConfigTable*);
void LobbyResource_UnloadResources();

#endif
