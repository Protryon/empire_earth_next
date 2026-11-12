// New (port): the world calls a Rust decision makes (src/Empire Earth/Units/EEUnitHost.cpp; rust/crates/ee_units/src/
// host.rs, which declares them on its side), declared for the C++ that calls them too: the TEST ONLY group command
// sweep (EEGroupCommandSweep.cpp) runs the original bodies' statements over these calls.
//
// TEST ONLY, the dry host: between EEUnitHostDryBegin and EEUnitHostDryEnd every call logs what it would do and
// answers without changing the world (no group, goal, draw, order or effect: groups are plain unit lists, a goal a
// record of its arguments, a draw a counter), so a body's decisions over the live world can be recorded at any point
// of a game without changing it. The reads (the map's height, the nearest town's patrol point, a route, a unit's
// type) are the real ones; Fill takes the selection's units by EEFillMembers (the store), the keeps test the objects
// as the originals did (KeepGroundAttackers' test is the store's: the dry keep compares it with the object's); the
// spreads keep the group as it is (logged).
#ifndef EE_EEUNITHOST_H
#define EE_EEUNITHOST_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

struct EeGoalSpec;
class EEComplexUnit;

extern "C" {
size_t ee_units_host_group_ids(void* inGroup, uint32_t* outIDs, size_t inCapacity);
void* ee_units_host_goal_new(const EeGoalSpec* inSpec);
uint8_t ee_units_host_goal_kind(void* inGoal);
void ee_units_host_goal_release(void* inGoal);
void* ee_units_host_active_group_new(void* inPlayer);
void ee_units_host_group_fill(void* inGroup, void* inSelection, int32_t inSort, int32_t inSlot);
void ee_units_host_group_release(void* inGroup);
void ee_units_host_pending_push(void* inPlayer, void* inGroup);
void ee_units_host_group_keep_mobile(void* inGroup);
void ee_units_host_group_keep_ground_attackers(void* inGroup);
void ee_units_host_group_clear_goal(void* inGroup);
void ee_units_host_group_set_goal(void* inGroup, void* inGoal, uint8_t inQueued);
void ee_units_host_group_set_action(void* inGroup, int32_t inAction);
void ee_units_host_group_plan(void* inGroup);
int32_t ee_units_host_random(int32_t inLow, int32_t inHigh);
void ee_units_host_play_acknowledgement(void* inPlayer, void* inGroup, int32_t inIndex);
float ee_units_host_map_height(float inX, float inY);
void ee_units_host_send_group_command(void* inGroup, uint32_t inTarget, void* inPlayer, int32_t inCommand, int32_t inQueue);
void ee_units_host_acknowledge_unit(void* inPlayer, uint32_t inUnit);
uint32_t ee_units_host_town_patrol_point(uint32_t inUnit, uint8_t* outFound);
void ee_units_host_patrol_add_unit_tile(uint32_t inUnit, uint32_t inTarget, void* inPlayer);
void* ee_units_host_unit_patrol_group(uint32_t inUnit);
int64_t ee_units_host_object_unit_type(uint32_t inUnit);
void ee_units_host_group_set_queued(void* inGroup, uint8_t inQueued);
void ee_units_host_group_spread_around_resource(void* inGroup, uint32_t inTarget);
void ee_units_host_group_spread_around_tile(void* inGroup, int64_t inX, int64_t inY);
void ee_units_host_unit_add_build_target(uint32_t inUnit, uint32_t inTarget);
void ee_units_host_unit_order_repair(uint32_t inUnit, uint32_t inTarget);
void ee_units_host_group_add_member(void* inGroup, uint32_t inUnit);
}

// the farm sorts of EEMBuildUnit (EETileDistanceLess) and EEMSmartFarmers (EEFarmDistanceGreater: inGreater) around
// the tile (inX, inY): the bodies' std::sorts, in EEMessage.cpp with their comparators
void EESortFarms(std::vector<EEComplexUnit*>& ioFarms, long inX, long inY, bool inGreater);

// TEST ONLY: the dry host (above). Begin makes the selection (a dry group of inUnits, returned as the reference the
// bodies take); End returns the log (words: an event code and its values) and the keeps' disagreements (the store's
// test against the object's), and frees the dry groups and goals.
void* EEUnitHostDryBegin(const std::vector<EEComplexUnit*>& inUnits);
void EEUnitHostDryEnd(std::vector<uint32_t>& outLog, unsigned long& outMismatches);

// TEST ONLY: the group command sweep (EEGroupCommandSweep.cpp; USGC records; EE-SIM, every five game minutes)
void EEGroupCommandSweep();

#endif
