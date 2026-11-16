// The next computer player's per-unit Lua interface (docs/AI_LUA_UNITS.md, stage 1): queries over single units, the
// commands a human gives (run through the human's own messages, EEMessage.cpp's Execute, on the computer player's
// selection), helpers, events, the unit takeover from the classic machinery (the held set) and a micro step. New in
// this port (no original address).
//
// The functions go into the script's `ai` table (EECPLuaConfig::mOpen) beside the coarse verbs, which keep working.
// Fog-honest: other players' units only while their tile is seen (EETile::IsSeenBy), resources only on explored
// tiles. Every binding charges its cost to the call's instruction budget (EECPLuaState::Charge).
#ifndef EE_EECPNEXTUNITS_H
#define EE_EECPNEXTUNITS_H

#include <string>
#include <vector>

struct lua_State;
class EEComputerPlayer;
class EEComplexUnit;
class EEUnit;
struct EEPUnitSwap;

// the ai.* functions (EECPLuaConfig::mOpen; the context is the EEComputerPlayer)
void EECPUnitsOpen(lua_State* inL, void* inPlayer);

// whether the script holds the unit (ai.take, or any command on it): the classic runners leave it alone
bool EECPUnitsHolds(const EEComputerPlayer* inPlayer, const EEComplexUnit* inUnit);
bool EECPUnitsHoldsID(const EEComputerPlayer* inPlayer, long inID);
// the ids held (for EECPNextHeld: the next engine's groups never take them)
void EECPUnitsHeld(const EEComputerPlayer* inPlayer, std::vector<long>& outIDs);
// the reflexes the script left on (ai.reflex): "naval_flee", "danger" (the citizens' danger zones and militia)
bool EECPUnitsReflexOn(const EEComputerPlayer* inPlayer, const char* inName);
// whether the script has called the per-unit interface (then a save's shadow step cannot replay it: its commands
// would run on the live player)
bool EECPUnitsUsed(const EEComputerPlayer* inPlayer);

// the events (EEComputerPlayer's handlers; nothing is decided there: they are queued for the script)
void EECPUnitsOnDamage(EEComputerPlayer* inPlayer, long inAttackerPlayer, long inAmount, EEUnit* inUnit);
// a hit one of the player's units dealt to inVictim (EEUnit::TakeHit): the lost layer's "dealt" side
void EECPUnitsOnDealt(EEComputerPlayer* inPlayer, long inAmount, EEUnit* inVictim);
void EECPUnitsOnPlaced(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit);
void EECPUnitsOnCompleted(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit);
void EECPUnitsOnAttackPlayer(EEComputerPlayer* inPlayer, long inPlayerID, bool inDefend);
void EECPUnitsOnFlare(EEComputerPlayer* inPlayer, long inPlayerID, long inX, long inY);
void EECPUnitsOnTaken(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit, bool inSwapped);
void EECPUnitsOnGoalDone(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit);
void EECPUnitsOnSwapped(EEComputerPlayer* inPlayer, std::vector<EEPUnitSwap>& ioSwaps);

// every physics tick (EEArtificialIntelligence::Update): the enemy memory once a game second (what the player sees
// now) and the scripts' `micro` every 250 game ms
void EECPUnitsMicroTick(unsigned long inTime);

// the save (EEPPlayerHelper, from version 0.254): the held set, the pending events, the forwarding map, the reflex
// switches, the enemy memory; empty: none
void EECPUnitsSave(const EEComputerPlayer* inPlayer, std::string& outBytes);
bool EECPUnitsLoad(EEComputerPlayer* inPlayer, const std::string& inBytes);
void EECPUnitsDelete(EEComputerPlayer* inPlayer);

#endif
