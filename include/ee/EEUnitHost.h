// New (port): the world calls a Rust decision makes (src/Empire Earth/Units/EEUnitHost.cpp; rust/crates/ee_sim/src/units//
// host.rs, which declares them on its side), declared for the C++ that calls them too: the TEST ONLY group command
// sweep (EEGroupCommandSweep.cpp) runs the bodies over the dry host (TEST ONLY; ee_sim units/dry.rs: every call
// logged, nothing changed; ee_units_dry_begin / ee_units_dry_end), which reads through these calls.
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
void* ee_units_host_player_selection(void* inPlayer);
uint8_t ee_units_host_unit_seen_by(uint32_t inUnit, void* inPlayer);
uint8_t ee_units_host_unit_ghost_seen(uint32_t inUnit);
uint8_t ee_units_host_unit_attackable(uint32_t inUnit, int64_t inDamageIndex);
uint8_t ee_units_host_unit_class_targetable(uint32_t inUnit);
uint8_t ee_units_host_unit_can_attack_class(uint32_t inUnit, uint32_t inTarget);
int64_t ee_units_host_unit_max_hit_points(uint32_t inUnit);
int64_t ee_units_host_unit_garrison_capacity(uint32_t inUnit);
uint8_t ee_units_host_unit_has_tile_node(uint32_t inUnit);
uint8_t ee_units_host_unit_shares_rally_point(uint32_t inUnit, int64_t inUnitType);
int64_t ee_units_host_unit_num_goal_queues(uint32_t inUnit);
void ee_units_host_unit_clear_goal_queue(uint32_t inUnit, int64_t inQueue);
void ee_units_host_unit_clear_all_goal_queues(uint32_t inUnit);
void ee_units_host_unit_queue_goal(uint32_t inUnit, void* inGoal, int64_t inQueue);
void ee_units_host_unit_queue_timer(uint32_t inUnit, int64_t inQueue, int64_t inMs);
void* ee_units_host_unit_goal_clone(uint32_t inUnit);
void* ee_units_host_garrison_rally_load(uint32_t inGarrison);
void ee_units_host_unit_queue_clear(uint32_t inUnit, int64_t inQueue);
void ee_units_host_unit_clear_air_traffic(uint32_t inUnit);
void ee_units_host_unit_refresh_goals(uint32_t inUnit);
void ee_units_host_unit_clear_build_targets(uint32_t inUnit);
void ee_units_host_unit_action_changed(uint32_t inUnit);
void ee_units_host_unit_halt(uint32_t inUnit);
int64_t ee_units_host_unit_air_traffic_type(uint32_t inUnit);
}

// the farm sorts of EEMBuildUnit (EETileDistanceLess) and EEMSmartFarmers (EEFarmDistanceGreater: inGreater) around
// the tile (inX, inY): the bodies' std::sorts, in EEMessage.cpp with their comparators
void EESortFarms(std::vector<EEComplexUnit*>& ioFarms, long inX, long inY, bool inGreater);

// TEST ONLY: the group command sweep (EEGroupCommandSweep.cpp; USGC records; EE-SIM, every five game minutes)
void EEGroupCommandSweep();

#endif
