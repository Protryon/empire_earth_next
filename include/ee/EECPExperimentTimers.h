// port (new): the timers of the computer players' experiments (docs/AI_RESEARCH.md §8), per player id. They were
// function statics of EEComputerPlayer.cpp and EECPResourceManager.cpp, outside every saved object, so a game loaded
// from a save started them at 0 where the saved game had them running (seed 2's 2100 s save: the loaded game queued a
// settlement at its first AI update that the saved game queued 20 s later). From save version 0.258 they are saved
// (EEPWorldHelper, ee/EESaveGameFiles.h EESaveHasExperimentTimers). Zero at program start, as the statics were.
#ifndef EE_EECPEXPERIMENTTIMERS_H
#define EE_EECPEXPERIMENTTIMERS_H

#include <stdint.h>
#include "rust/sim/cp.h"   // ee_cp_pl_state_block (ee_sim::cp player_game.rs)

class FSFile;
int32_t EESaveFilePrimitive(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);   // EEPlayer.cpp

// port (lane B of the AI Thread): the timers themselves are Rust's (ee_sim::cp::timers, in this order: the boats
// counted and their hold time ("boatrush2"), the last patch settlement, guard check, militia call and naval flee,
// the schedule's epoch and start, the prehistoric army's threat check and lift, the claim ranges' steps and last
// step), 64 players each

#endif
