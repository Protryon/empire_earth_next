// ee/wonapi/WONLobbyEventTypes.h. Named integer constants; names PROVISIONAL unless the comment says T1.
#ifndef EE_WONLOBBYEVENTTYPES_H
#define EE_WONLOBBYEVENTTYPES_H

// the lobby's event ids (ComponentEvent::mType >= 100001, Event_User_Start + n); names = the event class that passes the id to its LobbyEvent/LobbyChangeEvent base (RTTI class names). Ids without an own class (posted as a bare LobbyEvent: LobbyFn359120) are not named
// the lobby events' types (each event class's constructor's): Rust's constants since R6 WON lobby and network round 35
#include "rust/won_lobby/lobbyids.h"

#endif // EE_WONLOBBYEVENTTYPES_H
