// A computer player's placement searches (stage 6, docs/AI_LUA_VERBS.md): what was left of the original's build
// manager (EECPBuildManager, 0x66af0..0x6e8d4) once its queues, builders and runner moved into Lua (data/ai/lib/verbs.lua):
// the town centre tile, the shore and space site tiles, the "unit near" tests, CanBuildAt and FindSite with its finders.
// The Lua verbs call them through ai.place. Their bodies and the class's own data (the site tiles, the classes seen)
// are Rust's (ee_sim::cp::placement; AI lane B, round 2), and so is the record (EeCpPlacementObj).
#ifndef EE_EECPPLACEMENT_H
#define EE_EECPPLACEMENT_H

#include "rust/sim/cp.h"   // EeCpPlacementObj

class EEPlayer;


// The placement's record: Rust's (ee_sim::cp placement.rs EeCpPlacementObj: the town centre tile, -10000 none; the
// player; the dock and space site tiles, whether they were collected and UnitAdded's classes). Rust makes it at Init
// and frees it with the player; the town tile's Load and Save are Rust's too
struct EECPPlacement : EeCpPlacementObj {};

// (the epoch's classes, the original civilization manager's GetAntiAirClass .. GetWallClass: ee_sim::cp placement.rs)

#endif
