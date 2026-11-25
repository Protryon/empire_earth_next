// The new computer player's tactics layer (docs/AI_LUA_PLAN.md §2, stage S1; docs/AI_LUA_AUDIT.md). New in this port
// (no original address).
//
// Every computer player runs this engine (the original's decisions were deleted in stage 5): a Lua strategy (its
// AI's script, ee/EECPEngine.h) reads a view of what the player may know and drives the units through the per-unit
// interface (ee/EECPNextUnits.h). The engine is Rust (ee_sim::ai next_state.rs, next_value.rs, next_game.rs: the
// per-player state, the view, the army's scouting memory, the gather safety, the strategy step, the save); the C++
// keeps the computer player's own bookkeeping, the names, and one host (EECPNext.cpp, ee_ai_next_host) for what the
// world views do not hold.
#ifndef EE_EECPNEXT_H
#define EE_EECPNEXT_H

#include <string>
#include <vector>

#include "rust/sim/ai.h"   // ee_ai_next_* (next_state.rs, next_game.rs, next_save.rs)

class EEComputerPlayer;


// (the script's results, the gather safety's tests and limits, the event kinds, the placement searches: ee_ai
// names.rs, next_game.rs, next_value.rs)

// the unit id of the threat at a tile (0: none) and its kind; inNow: only soldiers in sight now, no towers
inline long EECPNextSiteThreat(EEComputerPlayer* inPlayer, long inX, long inY, bool inNow, long* outKind)
{
    return ee_ai_next_site_threat(inPlayer, inX, inY, inNow, outKind);
}

// ---------------------------------------------------------------------------------------------------------------
// The per-player state (EEComputerPlayer::mNextState: an opaque Rust record, ee_ai next_state.rs; created on its first
// tick, or by a loaded game). Saved with the computer player from save version 0.252 (EECPNextSaveState).

struct EECPNextState;                                                 // (Rust's)
EECPNextState* EECPNextGetState(EEComputerPlayer* inPlayer);        // created when missing
void EECPNextDeleteState(EEComputerPlayer* inPlayer);
std::string EECPNextAI(const EEComputerPlayer* inPlayer);           // the AI's id ("" none or no state)
inline void EECPNextAppend(void* outBytes, const std::string& inText)   // to a Rust Vec<u8>
{
    ee_ai_next_append(outBytes, (const uint8_t*)inText.data(), inText.size());
}
inline void EECPNextSetAI(EEComputerPlayer* inPlayer, const std::string& inAI)
{
    ee_ai_next_state_set_ai(EECPNextGetState(inPlayer), (const uint8_t*)inAI.data(), inAI.size());
}

// the save (EEPPlayerHelper, from version 0.252): the state that must survive as bytes, and back. Load creates the
// state; a Lua script gets its memory back at the first step (LoadPersonality, RestoreMemory, never init)
bool EECPNextSaveState(EEComputerPlayer* inPlayer, std::string& outBytes);
inline bool EECPNextLoadState(EEComputerPlayer* inPlayer, const std::string& inBytes)
{
    return ee_ai_next_load(inPlayer, (const uint8_t*)inBytes.data(), inBytes.size());
}

#endif
