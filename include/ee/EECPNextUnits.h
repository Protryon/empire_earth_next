// The next computer player's per-unit Lua interface (docs/AI_LUA_UNITS.md, stage 1): queries over single units, the
// commands a human gives (run through the human's own messages, EEMessage.cpp's Execute, on the computer player's
// selection), helpers, events, the unit takeover from the classic machinery (the held set) and a micro step. New in
// this port (no original address).
//
// The functions go into the script's `ai` table (EECPLuaConfig::mOpen) beside the coarse verbs, which keep working.
// Fog-honest: other players' units only while their tile is seen (EETile::IsSeenBy), resources only on explored
// tiles. Every binding charges its cost to the call's instruction budget (ee_ai lua_host.rs charge).
#ifndef EE_EECPNEXTUNITS_H
#define EE_EECPNEXTUNITS_H

#include <string>
#include <vector>


class EEComputerPlayer;
class EEUnit;

// the player's per-unit state (ee_ai units_state.rs; NULL until used): the Rust engine's handle of it (the ai.*
// functions are registered by ee_ai units_bind.rs ee_ai_units_open)
void* EECPUnitsRust(const EEComputerPlayer* inPlayer);

// whether the script holds the unit (ai.take, or any command on it): the classic runners leave it alone
void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim);
void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend);

// the save (EEPPlayerHelper, from version 0.254): the held set, the pending events, the forwarding map, the reflex
// switches, the enemy memory; empty: none
void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes);
void EECPUnitsDelete(EEComputerPlayer* inPlayer);

#endif
