// RoutingLogic: internet play over WON routing servers (rooms, games, friends, chat, moderation)
#ifndef EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#define EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#include <time.h>
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
namespace WONAPI {
// RTTI: RoutingLogic > NetLogic > LobbyLogic > RefCount; vtable 0x855a60.
// A Rust object since R6 WONLobby round 9 (rust/crates/ee_won_lobby/src/routelogic.rs: the servers it is on and
// connecting to, the joined rooms, the idle and moderator state, the last server refresh and every method) and,
// since R6 WON round 36, the RustLogic of kind Routing (ee/wonapi/WONLobbyLogic.h): the lists and dialogs the class
// held are in the shell's slots, named here (the dialogs in the members' order; the game data type was "G").
enum { kRoutingServerList, kRoutingGameList }; // mSlot: mServerList (0x54), mGameList (0x58)
// mDialog: 0x74 (AddFriend) .. 0x88 (Password)
enum { kRoutingAddFriend, kRoutingAdminAction, kRoutingBadUser, kRoutingCreateRoom, kRoutingRoom, kRoutingPassword };
}
#endif
