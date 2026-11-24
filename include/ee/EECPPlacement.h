// A computer player's placement searches (stage 6, docs/AI_LUA_VERBS.md): what was left of the original's build
// manager (EECPBuildManager, 0x66af0..0x6e8d4) once its queues, builders and runner moved into Lua (data/ai/lib/verbs.lua):
// the town centre tile, the shore and space site tiles, the "unit near" tests, CanBuildAt and FindSite with its finders.
// The Lua verbs call them through ai.place. Their bodies and the class's own data (the site tiles, the classes seen)
// are Rust's (ee_sim::cp::placement; AI lane B, round 2): the members below are the forwarders. Member names are
// provisional. Requires <deque>, <vector>, <utility>, lle/U2DPoint.h (through lle/LowLevelEngine.h), ee/EETileCoordinate.h.
#ifndef EE_EECPPLACEMENT_H
#define EE_EECPPLACEMENT_H

#include <deque>
#include <vector>
#include <utility>
#include "ee/EETileCoordinate.h"

class EEComputerPlayer;
class EEPlayer;
struct EeCpPlacement;                       // the Rust data (rust/sim/cp.h)

// 0x23d1f1 (cdecl; callers EECPBuildManager::FindSpaceSite for class 0x2d8 and 0x2037c6): whether (inX, inY) suits inPlayer's
// class-0x2d8 site (provisional name)
bool EEIsSpaceDockSiteOpen(long inX, long inY, EEPlayer* inPlayer);

// The placement's record (ee_sim::cp player_game.rs EeCpPlacementObj: Rust makes it at Init and frees it with the
// player; the town tile's Load and Save are Rust's too)
struct EECPPlacement {
    U2DPoint<long> mTownCenterTile;         // (-10000: none)
    EEComputerPlayer* mPlayer;
    EeCpPlacement* mRust;                   // the dock and space site tiles (CollectSiteTiles(2), (4)), whether they
                                            // were collected, UnitAdded's classes (not saved)
};
static_assert(sizeof(EECPPlacement) == 2 * sizeof(long) + 2 * sizeof(void*), "ee_sim::cp EeCpPlacementObj");

// (the epoch's classes, the original civilization manager's GetAntiAirClass .. GetWallClass: ee_sim::cp placement.rs)

#endif
