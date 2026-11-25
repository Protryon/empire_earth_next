//! The save versions' predicates (were ee/EESaveGameFiles.h's inlines): whether a save of version major.minor holds
//! a block or field the port added. Exported under their C++ names: the C++ callers keep calling them as before;
//! the Rust callers call them directly.
#![allow(non_snake_case)]

use std::ffi::c_ulong;

/// port: from 0.254 a save holds the running game's own values where the original wrote others or nothing, so a game
/// loaded from it plays on as the game that was saved ("load equals continue", docs/RUST_PLAN.md R5a.2). It was 0.253
/// on the rust branch until the merge of main ce3df9e4, whose 0.253 is the next engine's AI id (EEPPlayerHelper)
#[no_mangle]
pub extern "C" fn EESaveHasLiveState(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 254
}

/// port (decision D6): from 0.255 a save's strings are UTF-8; an older save's are Windows-1252 and are converted as
/// they load (USetLegacyTextLoad around the load: EEPSaveGameHeader::Load, EEGameStateLoader::LoadGame)
#[no_mangle]
pub extern "C" fn EESaveTextIsUtf8(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 255
}

/// port: from 0.256 a save holds more of the running state ("load equals continue", second round): the computer
/// players' round-robin schedule (EEArtificialIntelligence's next player and next update time, EEPWorldHelper; an older
/// save's loaded game updated its first computer player at its first tick), each computer player's own timers and
/// flags (EEComputerPlayer: pop cap, enable-time flag, regroup flag, population and damage times), and per unit the
/// contacts, the animation frame, the exact flight time, rotting time, research, last hit points, garrison and power
/// (EEComplexUnit; the original saved fractions or rebuilt them at load); the resource manager's timers and the
/// research manager's flags, the path manager's history, regions and timers, the area effects' locations and lists,
/// the units' order on each tile (EEPWorldHelper) and each attack sub group's targets, timers and path request
/// (EECPAttackSubGroup). 0.255 is the UTF-8 text (EESaveTextIsUtf8): a 0.255 save loads without these fields
#[no_mangle]
pub extern "C" fn EESaveHasRunningState(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 256
}

/// port: from 0.257 a save holds the order of the units in each map region (EEMap::mUnitRegions, EEPWorldHelper): a
/// load added them in its own order, and the projectiles' impact test and the target searches take the first match
#[no_mangle]
pub extern "C" fn EESaveHasRegionOrder(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 257
}

/// port: from 0.258 a save holds the computer players' experiment timers (ee/EECPExperimentTimers.h, EEPWorldHelper):
/// they were function statics, 0 in a loaded game's new process
#[no_mangle]
pub extern "C" fn EESaveHasExperimentTimers(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 258
}

/// port: from 0.260 a save holds the towns' ids, order and counters (EESaveTowns): a load rebuilt the towns with new
/// ids in its own order
#[no_mangle]
pub extern "C" fn EESaveHasTowns(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 260
}

/// port: from 0.262 a save holds the dead units the units' last attackers still reference (EEPWorldHelper): a load
/// read NULL where the running game kept the dead attacker
#[no_mangle]
pub extern "C" fn EESaveHasGhosts(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 262
}

/// port: from 0.263 a save holds the continent manager as it is (EEAIContinentSave.cpp): its areas with their serials,
/// the sectors and sub sectors, the grids and counters, and the serials of the herds and towns. A load rebuilt it from
/// the map as it then was, numbering the herds and towns before the regions: other path areas, sub sectors and choke
/// points under other serials than the running game's, which the computer players' tasks name
#[no_mangle]
pub extern "C" fn EESaveHasContinentRegions(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 263
}

/// port: from 0.265 a save holds the computer players' claim ranges and danger zones after the continent manager
/// (EECPSaveResourceState, ee/EECPExperimentTimers.h): FindResourceTarget reads them, a load started them empty
#[no_mangle]
pub extern "C" fn EESaveHasResourceState(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 265
}

/// port: from 0.268 the towns' block holds each town's area as it is (its tiles, hull, bounds and continent): a load
/// rebuilt the towns from their buildings in its own order, and a building the saved town's hull had taken could start
/// a town of its own (seed 6's 2100 s save: two towns and an extra patrol group where the saved game had one)
#[no_mangle]
pub extern "C" fn EESaveHasTownAreas(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 268
}

/// port: from 0.270 a save holds the order of each player's unit list (EEPWorldHelper): the load inserted the units by
/// hash bucket index, where the running list holds the buckets in the order they last filled
#[no_mangle]
pub extern "C" fn EESaveHasPlayerUnitOrder(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 270
}

/// port: from 0.272 each ghost (EESaveHasGhosts) also holds its resource type and amount, and the ghosts include the
/// resources and drop sites the gather goals still hold out of the world (a hunted animal's carcass gone, its gatherer
/// still walking to it): a load read NULL there, and the gatherer was counted with no resource (GetGatherCapacity)
#[no_mangle]
pub extern "C" fn EESaveHasGhostResources(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 272
}

/// port: from 0.273 a save holds the contagion manager's last helper id after its helpers (EEContagionManager): a load
/// went on from the highest saved helper's id (0 with none), so a calamity cast after it kept another helper id in its
/// placeholder than the running game's (the calamity's out-of-synch checksum differed from the next cast on)
#[no_mangle]
pub extern "C" fn EESaveHasContagionCounter(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 273
}

/// port: from 0.275 an attack goal writes its target while it holds one, dying or dead (the original wrote -1 for a
/// target with no hit points left, and the loaded unit lost the goal: RepairPointers failed and cleared the queue), and
/// the ghosts (EESaveHasGhosts) include the attack goals' targets and the gather goals' simple resources (a tree or a
/// mine used up while its gatherer still holds it) out of the world, each record with a complex flag
#[no_mangle]
pub extern "C" fn EESaveHasGoalGhosts(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 275
}

/// port: from 0.276 a save holds the units' tile searches in progress (EEPathManager::SaveSearches: the machine, its
/// goal and its storage as ee_astar's bytes): a load asked each of those units' path again (RequestPendingPaths), so
/// the path came later, through a graph search first, or another one
#[no_mangle]
pub extern "C" fn EESaveHasPathSearches(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 276
}

/// port: from 0.278 a save holds the graph searches in progress (EEPathManager::SaveGraphSearches: node, machine,
/// goal, modifier, request and the storage as ee_astar's bytes), and an attack sub group's running request waits for
/// its own (EEAwaitAutoPathRequest): a load asked each again at the first tick (the sub groups) or from the load (the
/// units, RequestPendingPaths), so the path came later or another one
#[no_mangle]
pub extern "C" fn EESaveHasGraphSearches(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 278
}

/// port: from 0.279 a save holds the players' active groups (EESaveActiveGroups, EEActiveGroup.cpp: units, formation,
/// speeds, reserved destinations, flags and path requests; not the goal), written before the graph searches: a load had
/// none (the units moved at their own speeds, the groups' requests and finishes were gone)
#[no_mangle]
pub extern "C" fn EESaveHasActiveGroups(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 279
}

/// port: from 0.280 a save holds each computer player's per-unit Lua interface state after its next engine's
/// (EEPPlayerHelper, ee/EECPNextUnits.h: a uint32 length, 0 for none, then the bytes). It was 0.254 on main until the
/// merge of main 79be8619, whose 0.254 here is EESaveHasLiveState
#[no_mangle]
pub extern "C" fn EESaveHasUnitsLuaState(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 280
}

/// port: from 0.282 a save holds each player's tech nodes that are not allowed (EETechTreeNode::mIsAllowed false: a
/// one-off technology queued or researched; EEPWorldHelper.cpp EESaveTechAllowed, after the graph searches): a load
/// allowed every node again, and a researched one-off technology could be researched a second time. Also from 0.282
/// each location goal holds its group path state (ee_units location.rs GROUP_PATH_STATE_MINOR; the original never
/// saved it)
#[no_mangle]
pub extern "C" fn EESaveHasTechAllowed(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 282
}

/// port: from 0.283 a save holds every map tile's terrain type (EEPWorldHelper.cpp EESaveTileTypes, after the tech
/// flags). EEMap::Load re-types each tile from the terrain table of its mesh vertex's texture, which is not the type the
/// random map's painting gave it (a painted vertex types the four tiles around it): a loaded map had about 1,000 tiles
/// of another type (grass, forest floor, cliff), and a unit that the continued game let onto a tile stopped at its edge
#[no_mangle]
pub extern "C" fn EESaveHasTileTypes(major: c_ulong, minor: c_ulong) -> bool {
    major > 0 || minor >= 283
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn each_predicate_starts_at_its_minor_version() {
        assert!(!EESaveHasLiveState(0, 253) && EESaveHasLiveState(0, 254) && EESaveHasLiveState(1, 0));
        assert!(!EESaveTextIsUtf8(0, 254) && EESaveTextIsUtf8(0, 255) && EESaveTextIsUtf8(1, 0));
        assert!(!EESaveHasRunningState(0, 255) && EESaveHasRunningState(0, 256) && EESaveHasRunningState(1, 0));
        assert!(!EESaveHasRegionOrder(0, 256) && EESaveHasRegionOrder(0, 257) && EESaveHasRegionOrder(1, 0));
        assert!(
            !EESaveHasExperimentTimers(0, 257) && EESaveHasExperimentTimers(0, 258) && EESaveHasExperimentTimers(1, 0)
        );
        assert!(!EESaveHasTowns(0, 259) && EESaveHasTowns(0, 260) && EESaveHasTowns(1, 0));
        assert!(!EESaveHasGhosts(0, 261) && EESaveHasGhosts(0, 262) && EESaveHasGhosts(1, 0));
        assert!(
            !EESaveHasContinentRegions(0, 262) && EESaveHasContinentRegions(0, 263) && EESaveHasContinentRegions(1, 0)
        );
        assert!(!EESaveHasResourceState(0, 264) && EESaveHasResourceState(0, 265) && EESaveHasResourceState(1, 0));
        assert!(!EESaveHasTownAreas(0, 267) && EESaveHasTownAreas(0, 268) && EESaveHasTownAreas(1, 0));
        assert!(
            !EESaveHasPlayerUnitOrder(0, 269) && EESaveHasPlayerUnitOrder(0, 270) && EESaveHasPlayerUnitOrder(1, 0)
        );
        assert!(!EESaveHasGhostResources(0, 271) && EESaveHasGhostResources(0, 272) && EESaveHasGhostResources(1, 0));
        assert!(
            !EESaveHasContagionCounter(0, 272) && EESaveHasContagionCounter(0, 273) && EESaveHasContagionCounter(1, 0)
        );
        assert!(!EESaveHasGoalGhosts(0, 274) && EESaveHasGoalGhosts(0, 275) && EESaveHasGoalGhosts(1, 0));
        assert!(!EESaveHasPathSearches(0, 275) && EESaveHasPathSearches(0, 276) && EESaveHasPathSearches(1, 0));
        assert!(!EESaveHasGraphSearches(0, 277) && EESaveHasGraphSearches(0, 278) && EESaveHasGraphSearches(1, 0));
        assert!(!EESaveHasActiveGroups(0, 278) && EESaveHasActiveGroups(0, 279) && EESaveHasActiveGroups(1, 0));
        assert!(!EESaveHasUnitsLuaState(0, 279) && EESaveHasUnitsLuaState(0, 280) && EESaveHasUnitsLuaState(1, 0));
        assert!(!EESaveHasTechAllowed(0, 281) && EESaveHasTechAllowed(0, 282) && EESaveHasTechAllowed(1, 0));
        assert!(!EESaveHasTileTypes(0, 282) && EESaveHasTileTypes(0, 283) && EESaveHasTileTypes(1, 0));
    }
}
