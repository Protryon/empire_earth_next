//! The group commands over the unit store (RUST_PLAN R5b.4 leaf 4 (c), leaf 5): which of a selection's units an
//! active group takes (`EEActiveGroup::Fill`'s test), and the first goal-giving body, Explore (`EEMExplore::Execute`).
//!
//! `EEActiveGroup::Fill(selection, sort, slot)` empties the active group, then takes each unit of the selection, in
//! the selection's order, that is valid, has hit points and belongs to the formation slot
//! (`EEActiveGroup::IsInFormationSlot`: 0 any; 1 air; 2 land or a land building; 3 sea but not submerged; 4 sea and
//! submerged; 5 the space domain; any other slot none). The test reads only the units' plain state, so it is
//! `fill_members` over the store; the C++ keeps the group's bookkeeping (the clear, the formation, the sort, each
//! taken unit's move out of its old active group) and applies the answer in order.
//!
//! A unit of a selection that is in no player's list has no record: it is not taken. The C++ test read the object;
//! a unit leaves its list only dead (`RTSPlayer::RemoveKilledUnits`), and the capture of the C++ test found no
//! such unit it would take (PORT_LEARNINGS "R5b.4, fifth round").

use crate::host::{
    EeGoalSpec, Host, HostRef, EE_GOAL_CTOR_ATTACK_GROUND, EE_GOAL_CTOR_ATTACK_UNIT, EE_GOAL_CTOR_EXPLORE,
    EE_GOAL_CTOR_GATHER, EE_GOAL_CTOR_HEAL_UNIT, EE_GOAL_CTOR_LOCATION, EE_GOAL_CTOR_PATROL, EE_GOAL_CTOR_PATROL_ROUTE,
    EE_UNIT_ID_NONE,
};
use crate::kinds::EE_OBJECT_COMPLEX_UNIT;
use crate::select::EE_UNIT_TYPE_PATH_POINT;
use crate::store::{EeUnitRecord, UnitView};

/// `kMovement*` (ee/EEUnitClassIDs.h `EEMovementDomain`)
pub const EE_MOVEMENT_AIR: u8 = 1;
pub const EE_MOVEMENT_LAND: u8 = 2;
pub const EE_MOVEMENT_SEA: u8 = 4;
/// `kUnitCategorySubmergedNaval`
pub const EE_UNIT_CATEGORY_SUBMERGED_NAVAL: i64 = 0xa;
/// `kActionFindUnexploredArea` (ee/EEUnitAction.h)
pub const EE_ACTION_FIND_UNEXPLORED_AREA: i32 = 11002;

/// `EEActiveGroup::IsInFormationSlot` over a record.
pub fn in_formation_slot(unit: &EeUnitRecord, slot: i64) -> bool {
    let domains = unit.domains;
    match slot {
        0 => true,
        1 => domains & EE_MOVEMENT_AIR != 0,
        2 => domains & EE_MOVEMENT_LAND != 0 || unit.land_building != 0,
        3 => domains & EE_MOVEMENT_SEA != 0 && unit.category != EE_UNIT_CATEGORY_SUBMERGED_NAVAL,
        4 => domains & EE_MOVEMENT_SEA != 0 && unit.category == EE_UNIT_CATEGORY_SUBMERGED_NAVAL,
        5 => (domains >> 3) & 1 != 0,
        _ => false,
    }
}

/// `EEActiveGroup::Fill`'s test: the indices into `ids` (a selection's units in its order) of the units the group
/// takes for `slot`, in order (a unit listed twice is taken twice, as the original's loop did).
pub fn fill_members(view: &impl UnitView, ids: &[u32], slot: i64, out: &mut Vec<u32>) {
    for (index, &id) in ids.iter().enumerate() {
        let Some(unit) = view.unit(id) else { continue };
        if unit.valid != 0 && unit.hit_points > 0 && in_formation_slot(unit, slot) {
            out.push(index as u32);
        }
    }
}

/// `EEMExplore::Execute`: for each formation slot 1..5 a new active group filled from the sender's selection
/// (slowest first); a group that took units goes to the player's pending groups, keeps its mobile units, gets an
/// Explore goal (`new EEUGExplore(false)`) with the action Find Unexplored Area and plans it, and one of its units,
/// drawn by `GetRandom(0, n - 1)`, acknowledges; an empty group is released. The calls are the original's, in its
/// order, and so is the one draw per group that kept a unit.
pub fn explore(host: &mut impl Host, player: HostRef, selection: HostRef) {
    let mut ids = Vec::new();
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 1, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_keep_mobile(group);
        host.group_clear_goal(group);
        let goal = host.goal_new(&EeGoalSpec::new(EE_GOAL_CTOR_EXPLORE));
        if let Some(goal) = goal {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, EE_ACTION_FIND_UNEXPLORED_AREA);
        host.group_plan(group);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
    }
}

/// `kActionAttackLocation` (ee/EEUnitAction.h)
pub const EE_ACTION_ATTACK_LOCATION: i32 = 2002;
/// `kActionMoveToNextPatrolPoint`
pub const EE_ACTION_MOVE_TO_NEXT_PATROL_POINT: i32 = 8013;
/// `EESendGroupCommand`'s commands (EEClient.cpp: the local command markers)
pub const EE_GROUP_COMMAND_ATTACK: i32 = 0xb9;
pub const EE_GROUP_COMMAND_TARGET: i32 = 0xba;

/// `EEActiveGroup::KeepGroundAttackers`' test: the unit's class can attack the ground. A unit in no list has no
/// record and is not kept (the groups it runs on were just filled from valid, living units).
pub fn keeps_ground_attacker(unit: Option<&EeUnitRecord>) -> bool {
    unit.is_some_and(|u| u.can_attack_ground != 0)
}

/// `EEUnitCast<EEComplexUnit>(gWorld.GetUnit(id))` over the store: `RTSWorld::GetUnit` looks in the list of the
/// player whose index is the id's top byte (`RTSPlayer::GetUnit`: its buckets), so the unit is found when its record
/// is in that player's list; then it must be a complex unit.
pub fn world_complex_unit(host: &mut impl Host, id: u32) -> Option<u32> {
    let unit = host.unit(id)?;
    (unit.owner == id >> 24 && unit.object_kind == EE_OBJECT_COMPLEX_UNIT).then_some(id)
}

/// A unit's class's type (`mUnitClass->mUnitType`): the store's, else (a selected unit already out of its list)
/// the object's through the host.
pub fn unit_type(host: &mut impl Host, id: u32) -> i64 {
    match host.unit(id) {
        Some(unit) => unit.unit_type,
        None => host.object_unit_type(id),
    }
}

/// `EEMAttackGround::Execute`: with a selection, the point (x, y) at the map's height (at least 0); for each formation
/// slot 1..5 a new active group filled from the selection (the selection sorted first); a group that took units goes
/// to the player's pending groups and keeps its ground attackers; when some are left, one is drawn
/// (`GetRandom(0, n - 1)`, before the goal), the group gets an Attack Ground goal (`new EEUGAttackGround(point, true)`)
/// with the action Attack Location, plans it, shows the attack marker (`EESendGroupCommand` 0xb9) and the drawn unit
/// acknowledges; an empty group is released.
pub fn attack_ground(host: &mut impl Host, player: HostRef, selection: HostRef, x: f32, y: f32) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    if ids.is_empty() {
        return;
    }
    let mut z = host.map_height(x, y);
    if z < 0.0 {
        z = 0.0;
    }
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 0, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_keep_ground_attackers(group);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            continue;
        }
        let index = host.random(0, ids.len() as i32 - 1);
        let unit = ids[index as usize];
        host.group_clear_goal(group);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_GROUND);
        spec.point = [x, y, z];
        spec.flag_a = 1;
        if let Some(goal) = host.goal_new(&spec) {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, EE_ACTION_ATTACK_LOCATION);
        host.group_plan(group);
        host.send_group_command(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_ATTACK, 0);
        host.acknowledge_unit(player, unit);
    }
}

/// `EEMPatrol::Execute`: the target is the complex unit with the id; for target -1 and a selection, the patrol
/// point nearest to the first selected unit on its nearest town's patrol route (when the town has one). With a
/// target and a selection: a selected path point adds the target's tile to its own route; else for each formation
/// slot 1..5 a new active group filled from the selection (sorted first) goes to the player's pending groups, keeps
/// its mobile units and patrols the target's route (`new EEUGPatrol(target->mPatrolGroup, target, true)`, made before
/// the group's goal is cleared; the route read once, before the loop), with the action Move To Next Patrol Point; it
/// plans, shows the target marker (0xba) and one unit, drawn after the plan, acknowledges; an empty group is released.
pub fn patrol(host: &mut impl Host, player: HostRef, selection: HostRef, target_id: i32) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let first = ids.first().copied();
    let mut target = world_complex_unit(host, target_id as u32);
    if let Some(first) = first {
        if target_id == -1 {
            if let Some(point) = host.town_patrol_point(first) {
                target = (point != EE_UNIT_ID_NONE).then_some(point);
            }
        }
    }
    let (Some(target), Some(first)) = (target, first) else { return };
    if unit_type(host, first) == EE_UNIT_TYPE_PATH_POINT {
        host.patrol_add_unit_tile(first, target, player);
        return;
    }
    let route = host.unit_patrol_group(target);
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 0, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_keep_mobile(group);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_PATROL_ROUTE);
        spec.unit_a = target;
        spec.object = route.0;
        spec.flag_a = 1;
        let goal = host.goal_new(&spec);
        host.group_clear_goal(group);
        if let Some(goal) = goal {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, EE_ACTION_MOVE_TO_NEXT_PATROL_POINT);
        host.group_plan(group);
        host.send_group_command(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_TARGET, 0);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
    }
}

/// `kActionGatherResource` (ee/EEUnitAction.h)
pub const EE_ACTION_GATHER_RESOURCE: i32 = 4004;

/// `gWorld.GetUnit(id)` over the store: the unit when its record is in the list of the player whose index is the
/// id's top byte (`RTSWorld::GetUnit` -> `RTSPlayer::GetUnit`).
pub fn world_unit(host: &mut impl Host, id: u32) -> Option<EeUnitRecord> {
    host.unit(id).filter(|unit| unit.owner == id >> 24)
}

/// `EEMGatherResource::Execute`: with a selection and a target that has resources left (`mResourceAmount > 0`), one
/// active group of the whole selection (slot 0, the selection sorted first) goes to the player's pending groups,
/// takes the queued flag and spreads around the resource; when it kept units it gets a Gather goal
/// (`new EEUGGather(target, NULL, true)`, queued as the message says) with the action Gather Resource, plans, shows
/// the target marker on the resource (0xba) and one unit, drawn after the plan, acknowledges.
pub fn gather(host: &mut impl Host, player: HostRef, selection: HostRef, target_id: u32, queued: bool) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let Some(target) = world_unit(host, target_id) else { return };
    if ids.is_empty() || target.resource_amount <= 0 {
        return;
    }
    let group = host.active_group_new(player);
    host.group_fill(group, selection, 0, 0);
    host.pending_push(player, group);
    host.group_set_queued(group, queued);
    host.group_spread_around_resource(group, target_id);
    host.group_ids(group, &mut ids);
    if ids.is_empty() {
        return;
    }
    host.group_clear_goal(group);
    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_GATHER);
    spec.unit_a = target_id;
    spec.flag_a = 1;
    if let Some(goal) = host.goal_new(&spec) {
        host.group_set_goal(group, goal, queued);
    }
    host.group_set_action(group, EE_ACTION_GATHER_RESOURCE);
    host.group_plan(group);
    host.send_group_command(group, target_id, player, EE_GROUP_COMMAND_TARGET, 0);
    host.group_ids(group, &mut ids);
    if !ids.is_empty() {
        let index = host.random(0, ids.len() as i32 - 1);
        host.play_acknowledgement(player, group, index);
    }
}

/// `EEMRepairUnit::Execute`: with a selection and a target, one active group of the whole selection (slot 0, sorted
/// first) goes to the player's pending groups and spreads around the target's tile; queued, each of its units adds
/// the target as a build target (and marks the build shift-queued); else each orders the repair (`OrderRepair`, the
/// idle action, the class's `ActionChanged`), and once all have, each is added back to the group (`AddMember(unit,
/// true)`). Then one unit, drawn, acknowledges.
pub fn repair(host: &mut impl Host, player: HostRef, selection: HostRef, target_id: u32, queued: bool) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let Some(target) = world_unit(host, target_id) else { return };
    if ids.is_empty() {
        return;
    }
    let group = host.active_group_new(player);
    host.group_fill(group, selection, 0, 0);
    host.pending_push(player, group);
    host.group_spread_around_tile(group, target.tile_x, target.tile_y);
    host.group_ids(group, &mut ids);
    if ids.is_empty() {
        return;
    }
    let units = ids.clone();
    if queued {
        for &unit in &units {
            host.unit_add_build_target(unit, target_id);
        }
    } else {
        for &unit in &units {
            host.unit_order_repair(unit, target_id);
        }
        for &unit in &units {
            host.group_add_member(group, unit);
        }
    }
    host.group_ids(group, &mut ids);
    if !ids.is_empty() {
        let index = host.random(0, ids.len() as i32 - 1);
        host.play_acknowledgement(player, group, index);
    }
}

/// `EEActiveGroup::KeepNonCitizens`' test over the store: a commandable, living unit that is not a citizen (land or
/// naval). A unit in no list has no record and is not kept.
pub fn keeps_non_citizen(unit: Option<&EeUnitRecord>) -> bool {
    unit.is_some_and(|u| {
        u.commandable != 0
            && u.hit_points > 0
            && u.category != EE_UNIT_CATEGORY_LAND_CITIZEN
            && u.category != EE_UNIT_CATEGORY_NAVAL_CITIZEN
    })
}

/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
pub const EE_UNIT_CATEGORY_LAND_CITIZEN: i64 = 6;
pub const EE_UNIT_CATEGORY_NAVAL_CITIZEN: i64 = 0x16;

/// `EEMAddPatrolPoint::Execute`: with a first selected unit that is a path point, the point goes into its route after
/// it (`InsertPatrolPointAt`); a first unit patrolling adds the point to its goal's route; else for each formation slot
/// 1..5 a new active group filled from the selection goes to the player's pending groups, keeps its mobile units and
/// its non-citizens and gets a new patrol (`new EEUGPatrol(true)`, the point added to its route, the route registered
/// with the player) with the action Move To Next Patrol Point, plans, and one unit, drawn, acknowledges; an empty
/// group is released. With nothing selected, a lone patrol group with the point is registered with the player.
pub fn add_patrol_point(host: &mut impl Host, player: HostRef, selection: HostRef, x: f32, y: f32) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let Some(&first) = ids.first() else {
        host.patrol_new_lone(player, x, y);
        return;
    };
    if unit_type(host, first) == EE_UNIT_TYPE_PATH_POINT {
        host.patrol_insert_point_at(first, x, y, player);
        return;
    }
    let route = host.unit_goal_patrol_route(first);
    if !route.0.is_null() {
        host.patrol_add_point(route, x, y, player);
        return;
    }
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 0, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_keep_mobile(group);
        host.group_keep_non_citizens(group);
        let goal = host.goal_new(&EeGoalSpec { flag_a: 1, ..EeGoalSpec::new(EE_GOAL_CTOR_PATROL) });
        if let Some(goal) = &goal {
            host.goal_patrol_add_point(goal, x, y, player);
        }
        host.group_clear_goal(group);
        if let Some(goal) = goal {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, EE_ACTION_MOVE_TO_NEXT_PATROL_POINT);
        host.group_plan(group);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
    }
}

/// `kActionAttackEnemyUnit`, `kActionHealUnit` (ee/EEUnitAction.h)
pub const EE_ACTION_ATTACK_ENEMY_UNIT: i32 = 2001;
pub const EE_ACTION_HEAL_UNIT: i32 = 16001;

/// `EEMAttackUnit::Execute`: with a selection and a living target (`gWorld.GetUnit`, hit points above 0), for each
/// formation slot 1..5 a new active group filled from the selection (sorted, sort 1) takes the queued flag; one that
/// took units goes to the player's pending groups and spreads around the target (`SpreadAroundUnit(target, its
/// tile)`); when units are left one is drawn, the group gets an Attack Unit goal (`new EEUGAttackUnit(the selection's
/// first unit as it is then, target, 1, convert)`, queued as the message says) with the action Attack Enemy Unit,
/// plans, shows the attack marker on the target (0xb9) and the drawn unit acknowledges; an empty group is released.
pub fn attack_unit(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    target_id: u32,
    queued: bool,
    convert: bool,
) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    if ids.is_empty() {
        return;
    }
    let Some(target) = world_unit(host, target_id) else { return };
    if target.hit_points <= 0 {
        return;
    }
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 1, slot);
        host.group_set_queued(group, queued);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_spread_around_unit(group, target_id, target.tile_x, target.tile_y);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            continue;
        }
        let index = host.random(0, ids.len() as i32 - 1);
        let unit = ids[index as usize];
        host.group_clear_goal(group);
        host.group_ids(selection, &mut ids);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_UNIT);
        spec.unit_a = ids.first().copied().unwrap_or(EE_UNIT_ID_NONE);
        spec.unit_b = target_id;
        spec.flag_a = 1;
        spec.flag_b = convert as u8;
        if let Some(goal) = host.goal_new(&spec) {
            host.group_set_goal(group, goal, queued);
        }
        host.group_set_action(group, EE_ACTION_ATTACK_ENEMY_UNIT);
        host.group_plan(group);
        host.send_group_command(group, target_id, player, EE_GROUP_COMMAND_ATTACK, 0);
        host.acknowledge_unit(player, unit);
    }
}

/// `EEMHealUnit::Execute`: with a target (`gWorld.GetUnit`), for each formation slot 1..5 a new active group filled
/// from the selection (sort 1) that took units goes to the player's pending groups and gets a Heal Unit goal (`new
/// EEUGHealUnit(the target's complex unit, true)`) with the action Heal Unit, plans, and one unit, drawn, acknowledges;
/// an empty group is released. A target with no complex unit is refused before any group (the original built the
/// goal over NULL and its Plan dereferenced it: a crash; the UI sends only complex targets).
pub fn heal_unit(host: &mut impl Host, player: HostRef, selection: HostRef, target_id: u32) {
    if world_unit(host, target_id).is_none() {
        return;
    }
    let complex = host.unit_complex_id(target_id);
    if complex == EE_UNIT_ID_NONE {
        return;
    }
    let mut ids = Vec::new();
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 1, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_clear_goal(group);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_HEAL_UNIT);
        spec.unit_a = complex;
        spec.flag_a = 1;
        if let Some(goal) = host.goal_new(&spec) {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, EE_ACTION_HEAL_UNIT);
        host.group_plan(group);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
    }
}

/// `kActionPrepareToMove` (ee/EEUnitAction.h)
pub const EE_ACTION_PREPARE_TO_MOVE: i32 = 8000;
/// `EESendGroupCommand`'s move marker
pub const EE_GROUP_COMMAND_MOVE: i32 = 0xb8;

/// EEMPathTo's options
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PathToOptions {
    pub facing: f32,
    pub forced: bool,
    pub rotating: bool,
    pub queued: bool,
    pub group_movement: bool,
}

/// `EEMPathTo::Execute`: with a selection, the point is adjusted by the host from the first selected unit (the map's
/// closest tile, a building it cannot pass; the message keeps the adjusted point: returned) and takes the final tile's
/// height; for each formation slot 1..5 a new active group filled from the selection goes to the player's pending
/// groups, keeps its mobile units and gets a Location goal (`new EEUGLocation(&point, true, forced, rotating, facing,
/// false, 0)` with its group movement flag; made before the paths are reset and the goal cleared), queued as the
/// message says, with the action Prepare To Move; it plans, one unit, drawn, acknowledges, and the move marker (0xb8)
/// shows; an empty group is released. Returns the point (None: no selection, the message unchanged).
pub fn path_to(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    x: f32,
    y: f32,
    o: PathToOptions,
) -> Option<(f32, f32)> {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let &first = ids.first()?;
    let (x, y, z) = host.path_to_point(first, x, y);
    for slot in 1..6 {
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 0, slot);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        host.pending_push(player, group);
        host.group_keep_mobile(group);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION);
        spec.point = [x, y, z];
        spec.flag_a = o.forced as u8;
        spec.flag_b = o.rotating as u8;
        spec.value = o.facing;
        spec.flag_c = o.group_movement as u8;
        let goal = host.goal_new(&spec);
        host.group_reset_paths(group);
        host.group_clear_goal(group);
        if let Some(goal) = goal {
            host.group_set_goal(group, goal, o.queued);
        }
        host.group_set_action(group, EE_ACTION_PREPARE_TO_MOVE);
        host.group_plan(group);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
        host.send_group_command(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_MOVE, 0);
    }
    Some((x, y))
}

/// `kActionDropoffResource` (ee/EEUnitAction.h)
pub const EE_ACTION_DROPOFF_RESOURCE: i32 = 4011;

/// `EEMDropoffResource::Execute`: with a selection and a drop-off that is a complex unit
/// (`EEUnitCast<EEComplexUnit>(gWorld.GetUnit(id))`), one active group of the whole selection (slot 0, unsorted) goes
/// to the player's pending groups and spreads around the drop-off's tile; when it kept units it gets a Gather goal
/// with no resource and the drop-off (`new EEUGGather(NULL, dropoff, true)`) with the action Dropoff Resource, plans,
/// and one unit, drawn after the plan, acknowledges.
pub fn dropoff_resource(host: &mut impl Host, player: HostRef, selection: HostRef, dropoff_id: u32) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let dropoff = world_complex_unit(host, dropoff_id).and_then(|id| host.unit(id));
    let Some(dropoff) = dropoff else { return };
    if ids.is_empty() {
        return;
    }
    let group = host.active_group_new(player);
    host.group_fill(group, selection, 0, 0);
    host.pending_push(player, group);
    host.group_spread_around_tile(group, dropoff.tile_x, dropoff.tile_y);
    host.group_ids(group, &mut ids);
    if ids.is_empty() {
        return;
    }
    host.group_clear_goal(group);
    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_GATHER);
    spec.unit_b = dropoff_id;
    spec.flag_a = 1;
    if let Some(goal) = host.goal_new(&spec) {
        host.group_set_goal(group, goal, false);
    }
    host.group_set_action(group, EE_ACTION_DROPOFF_RESOURCE);
    host.group_plan(group);
    host.group_ids(group, &mut ids);
    if !ids.is_empty() {
        let index = host.random(0, ids.len() as i32 - 1);
        host.play_acknowledgement(player, group, index);
    }
}

/// `EEActiveGroup::KeepCitizens`' test over the store: a commandable, living land or naval citizen. A unit in no list
/// has no record and is not kept.
pub fn keeps_citizen(unit: Option<&EeUnitRecord>) -> bool {
    unit.is_some_and(|u| {
        u.commandable != 0
            && u.hit_points > 0
            && (u.category == EE_UNIT_CATEGORY_LAND_CITIZEN || u.category == EE_UNIT_CATEGORY_NAVAL_CITIZEN)
    })
}

/// `EEMBuildWall::Execute` (the sender is the message's player, `gWorld.mPlayers[mPlayerID]`): with a selection, one
/// active group of the whole selection (slot 0, unsorted) goes to the player's pending groups and keeps its citizens;
/// when some are left the wall of the technology's class is planned from start to end as foundations
/// (`EEPlanWall(.., true, false, pieces)`) and one unit, drawn, acknowledges; then, when the plan made pieces, the
/// group builds them (`EEBuildWall`).
pub fn build_wall(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    technology: i32,
    start: (f32, f32),
    end: (f32, f32),
) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    if ids.is_empty() {
        return;
    }
    let group = host.active_group_new(player);
    host.group_fill(group, selection, 0, 0);
    host.pending_push(player, group);
    host.group_keep_citizens(group);
    host.group_ids(group, &mut ids);
    if ids.is_empty() {
        return;
    }
    let (pieces, count) = host.wall_plan(player, technology, start, end);
    host.group_ids(group, &mut ids);
    if !ids.is_empty() {
        let index = host.random(0, ids.len() as i32 - 1);
        host.play_acknowledgement(player, group, index);
    }
    if count > 0 {
        host.wall_build(group, pieces);
    }
    host.wall_release(pieces);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::host::tests::{Call, MockHost};
    use std::collections::BTreeMap;

    fn unit(id: u32, domains: u8) -> EeUnitRecord {
        EeUnitRecord { id, valid: 1, hit_points: 10, domains, category: 1, unit_type: 3, ..Default::default() }
    }

    #[test]
    fn the_formation_slots() {
        let land = unit(1, EE_MOVEMENT_LAND);
        let air = unit(2, EE_MOVEMENT_AIR);
        let sea = unit(3, EE_MOVEMENT_SEA);
        let sub = EeUnitRecord { category: EE_UNIT_CATEGORY_SUBMERGED_NAVAL, ..unit(4, EE_MOVEMENT_SEA) };
        let space = unit(5, 8);
        let building = EeUnitRecord { land_building: 1, ..unit(6, 0) };
        let amphibious = unit(7, EE_MOVEMENT_LAND | EE_MOVEMENT_SEA);
        let all = [land, air, sea, sub, space, building, amphibious];
        let slots: Vec<Vec<u32>> =
            (0..7).map(|s| all.iter().filter(|u| in_formation_slot(u, s)).map(|u| u.id).collect()).collect();
        assert_eq!(
            slots,
            vec![vec![1, 2, 3, 4, 5, 6, 7], vec![2], vec![1, 6, 7], vec![3, 7], vec![4], vec![5], vec![]]
        );
        assert!(!in_formation_slot(&land, -1));
    }

    #[test]
    fn fill_takes_valid_living_units_of_the_slot_in_order() {
        let mut view = BTreeMap::new();
        for u in [
            unit(10, EE_MOVEMENT_LAND),
            EeUnitRecord { valid: 0, ..unit(11, EE_MOVEMENT_LAND) },
            EeUnitRecord { hit_points: 0, ..unit(12, EE_MOVEMENT_LAND) },
            EeUnitRecord { hit_points: -5, ..unit(13, EE_MOVEMENT_LAND) },
            unit(14, EE_MOVEMENT_AIR),
            unit(15, EE_MOVEMENT_LAND),
        ] {
            view.insert(u.id, u);
        }
        let mut out = Vec::new();
        // 99 is in no list: not taken; 10 twice: taken twice
        fill_members(&view, &[15, 99, 10, 11, 12, 13, 14, 10], 2, &mut out);
        assert_eq!(out, vec![0, 2, 7]);
        out.clear();
        fill_members(&view, &[15, 14, 10], 1, &mut out);
        assert_eq!(out, vec![1]);
        out.clear();
        fill_members(&view, &[], 0, &mut out);
        assert!(out.is_empty());
    }

    /// The host calls of `EEMExplore::Execute` for a selection whose land units fill slot 2 (the C++ body's order)
    #[test]
    fn explore_makes_the_original_calls_in_order() {
        let mut host = MockHost::default();
        let player = HostRef(0x500 as *mut std::ffi::c_void);
        let selection = HostRef(0x600 as *mut std::ffi::c_void);
        // the mock's Fill: slot 2 takes three units, KeepMobileUnits keeps two of them
        host.fills.insert(2, vec![0x0100_0001, 0x0100_0002, 0x0100_0003]);
        host.mobile = vec![0x0100_0001, 0x0100_0003];
        host.draws = vec![1];
        explore(&mut host, player, selection);
        let g = |n: usize| HostRef((0x1000 + n) as *mut std::ffi::c_void);
        let mut expected = Vec::new();
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 1, slot));
            if slot != 2 {
                expected.push(Call::Release(group));
                continue;
            }
            expected.extend([
                Call::Pending(player, group),
                Call::KeepMobile(group),
                Call::ClearGoal(group),
                Call::GoalNew(EE_GOAL_CTOR_EXPLORE, 0),
                Call::SetGoal(group, 10, false),
                Call::SetAction(group, EE_ACTION_FIND_UNEXPLORED_AREA),
                Call::Plan(group),
                Call::Random(0, 1),
                Call::Acknowledge(player, group, 1),
            ]);
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0, "the goal went to the group");
    }

    #[test]
    fn explore_draws_nothing_when_no_unit_stays_mobile() {
        let mut host = MockHost::default();
        host.fills.insert(1, vec![7]);
        host.fills.insert(3, vec![8, 9]);
        host.mobile = vec![9];
        explore(&mut host, HostRef(std::ptr::null_mut()), HostRef(std::ptr::null_mut()));
        let draws: Vec<_> = host.calls.iter().filter(|c| matches!(c, Call::Random(..))).collect();
        assert_eq!(draws, vec![&Call::Random(0, 0)]);
        let plans = host.calls.iter().filter(|c| matches!(c, Call::Plan(_))).count();
        assert_eq!(plans, 2);
        let releases = host.calls.iter().filter(|c| matches!(c, Call::Release(_))).count();
        assert_eq!(releases, 3);
    }

    fn complex(id: u32, unit_type: i64) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            valid: 1,
            hit_points: 10,
            unit_type,
            can_attack_ground: 1,
            ..Default::default()
        }
    }

    fn r(n: usize) -> HostRef {
        HostRef(n as *mut std::ffi::c_void)
    }

    /// `EEMAttackGround::Execute`'s calls: the height clamped at 0, the draw before the goal, the drawn unit
    /// acknowledging, a group whose ground attackers are all gone kept pending without a goal
    #[test]
    fn attack_ground_makes_the_original_calls_in_order() {
        let mut host = MockHost { specs: true, height: -2.5, ..Default::default() };
        let (player, selection) = (r(0x500), r(0x600));
        host.groups.insert(0x600, vec![0x0100_0001, 0x0100_0002, 0x0100_0003]);
        host.fills.insert(2, vec![0x0100_0001, 0x0100_0002, 0x0100_0003]);
        host.fills.insert(3, vec![0x0100_0004]);
        host.ground = vec![0x0100_0001, 0x0100_0003];
        host.draws = vec![1];
        attack_ground(&mut host, player, selection, 10.5, 20.25);
        let g = |n: usize| r(0x1000 + n);
        let mut expected = vec![Call::MapHeight(10.5f32.to_bits(), 20.25f32.to_bits())];
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 0, slot));
            match slot {
                2 => expected.extend([
                    Call::Pending(player, group),
                    Call::KeepGround(group),
                    Call::Random(0, 1),
                    Call::ClearGoal(group),
                    Call::GoalNew(EE_GOAL_CTOR_ATTACK_GROUND, 1),
                    Call::Spec(
                        EE_GOAL_CTOR_ATTACK_GROUND,
                        EE_UNIT_ID_NONE,
                        EE_UNIT_ID_NONE,
                        [10.5f32.to_bits(), 20.25f32.to_bits(), 0.0f32.to_bits()],
                        1,
                        0,
                    ),
                    Call::SetGoal(group, 3, false),
                    Call::SetAction(group, EE_ACTION_ATTACK_LOCATION),
                    Call::Plan(group),
                    Call::Send(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_ATTACK, 0),
                    Call::AcknowledgeUnit(player, 0x0100_0003),
                ]),
                3 => expected.extend([Call::Pending(player, group), Call::KeepGround(group)]),
                _ => expected.push(Call::Release(group)),
            }
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0);
    }

    #[test]
    fn attack_ground_without_a_selection_does_nothing() {
        let mut host = MockHost::default();
        host.fills.insert(1, vec![7]);
        attack_ground(&mut host, r(1), r(2), 1.0, 1.0);
        assert!(host.calls.is_empty());
        // a positive height stays; NaN is not below 0
        for (height, z) in [(3.0f32, 3.0f32), (f32::NAN, f32::NAN)] {
            let mut host = MockHost { specs: true, height, ..Default::default() };
            host.groups.insert(2, vec![7]);
            host.fills.insert(1, vec![7]);
            host.ground = vec![7];
            attack_ground(&mut host, r(1), r(2), 1.0, 1.0);
            let spec = host.calls.iter().find_map(|c| match c {
                Call::Spec(_, _, _, bits, ..) => Some(bits[2]),
                _ => None,
            });
            assert_eq!(spec, Some(z.to_bits()));
        }
    }

    #[test]
    fn the_ground_attackers_test() {
        assert!(keeps_ground_attacker(Some(&complex(1, 3))));
        assert!(!keeps_ground_attacker(Some(&EeUnitRecord { can_attack_ground: 0, ..complex(1, 3) })));
        assert!(!keeps_ground_attacker(None));
    }

    /// `EEMPatrol::Execute`'s calls for a target of another player: the route read once, the goal made before the
    /// clear, the marker, the draw after the plan
    #[test]
    fn patrol_makes_the_original_calls_in_order() {
        let mut host = MockHost { specs: true, ..Default::default() };
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0200_0009;
        host.units.insert(target, complex(target, 5));
        host.units.insert(0x0100_0001, complex(0x0100_0001, 3));
        host.routes.insert(target, 0x7700);
        host.groups.insert(0x600, vec![0x0100_0001, 0x0100_0002]);
        host.fills.insert(4, vec![0x0100_0001, 0x0100_0002]);
        host.mobile = vec![0x0100_0001, 0x0100_0002];
        host.draws = vec![1];
        patrol(&mut host, player, selection, target as i32);
        let g = |n: usize| r(0x1000 + n);
        let mut expected = vec![Call::Route(target)];
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 0, slot));
            if slot != 4 {
                expected.push(Call::Release(group));
                continue;
            }
            expected.extend([
                Call::Pending(player, group),
                Call::KeepMobile(group),
                Call::GoalNew(EE_GOAL_CTOR_PATROL_ROUTE, 1),
                Call::Spec(EE_GOAL_CTOR_PATROL_ROUTE, target, EE_UNIT_ID_NONE, [0; 3], 1, 0x7700),
                Call::ClearGoal(group),
                Call::SetGoal(group, 9, false),
                Call::SetAction(group, EE_ACTION_MOVE_TO_NEXT_PATROL_POINT),
                Call::Plan(group),
                Call::Send(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_TARGET, 0),
                Call::Random(0, 1),
                Call::Acknowledge(player, group, 1),
            ]);
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0);
    }

    #[test]
    fn patrol_targets() {
        let (player, selection) = (r(0x500), r(0x600));
        let first = 0x0100_0001;
        let base = || {
            let mut host = MockHost::default();
            host.units.insert(first, complex(first, 3));
            host.groups.insert(0x600, vec![first]);
            host
        };
        // no such unit, a unit of another list than its id names, a simple unit: no target, nothing done
        for (id, rec) in [
            (0x0200_0001u32, None),
            (0x0200_0002, Some(EeUnitRecord { owner: 3, ..complex(0x0200_0002, 5) })),
            (0x0200_0003, Some(EeUnitRecord { object_kind: crate::kinds::EE_OBJECT_UNIT, ..complex(0x0200_0003, 5) })),
        ] {
            let mut host = base();
            if let Some(rec) = rec {
                host.units.insert(id, rec);
            }
            patrol(&mut host, player, selection, id as i32);
            assert!(host.calls.is_empty(), "{id:x}");
        }
        // -1: the nearest town's point; a town without a route keeps the target (none); a route without points
        // clears it
        for (point, calls) in [
            (None, vec![Call::TownPoint(first)]),
            (Some(EE_UNIT_ID_NONE), vec![Call::TownPoint(first)]),
            (Some(0x0100_0005), vec![Call::TownPoint(first), Call::Route(0x0100_0005)]),
        ] {
            let mut host = base();
            host.town_point = point;
            patrol(&mut host, player, selection, -1);
            let head: Vec<_> = host.calls.iter().take(calls.len()).collect();
            assert_eq!(head, calls.iter().collect::<Vec<_>>());
            if point.is_none_or(|p| p == EE_UNIT_ID_NONE) {
                assert_eq!(host.calls.len(), 1);
            }
        }
        // -1 without a selection: no town asked
        let mut host = MockHost { town_point: Some(0x0100_0005), ..Default::default() };
        patrol(&mut host, player, selection, -1);
        assert!(host.calls.is_empty());
        // a selected path point takes the target's tile; a path point out of the store is read on the object
        let mut host = base();
        host.units.insert(first, complex(first, EE_UNIT_TYPE_PATH_POINT));
        host.units.insert(0x0100_0002, complex(0x0100_0002, 3));
        patrol(&mut host, player, selection, 0x0100_0002);
        assert_eq!(host.calls, vec![Call::AddUnitTile(first, 0x0100_0002, player)]);
        let mut host = base();
        host.units.remove(&first);
        host.object_types.insert(first, EE_UNIT_TYPE_PATH_POINT);
        host.units.insert(0x0100_0002, complex(0x0100_0002, 3));
        patrol(&mut host, player, selection, 0x0100_0002);
        assert_eq!(host.calls, vec![Call::ObjectType(first), Call::AddUnitTile(first, 0x0100_0002, player)]);
    }

    fn resource(id: u32, amount: i64) -> EeUnitRecord {
        EeUnitRecord { resource_amount: amount, tile_x: 40, tile_y: 41, ..complex(id, 7) }
    }

    /// `EEMGatherResource::Execute`'s calls: one group of the whole selection, the queued flag before the spread, the
    /// goal queued as the message says, the marker on the resource, the draw after the plan
    #[test]
    fn gather_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0000_0042;
        for queued in [false, true] {
            let mut host = MockHost { specs: true, ..Default::default() };
            host.units.insert(target, resource(target, 300));
            host.groups.insert(0x600, vec![0x0100_0001, 0x0100_0002]);
            host.fills.insert(0, vec![0x0100_0001, 0x0100_0002]);
            host.draws = vec![1];
            gather(&mut host, player, selection, target, queued);
            let group = r(0x1001);
            assert_eq!(
                host.calls,
                vec![
                    Call::NewGroup(player, group),
                    Call::Fill(group, selection, 0, 0),
                    Call::Pending(player, group),
                    Call::Queued(group, queued),
                    Call::SpreadResource(group, target),
                    Call::ClearGoal(group),
                    Call::GoalNew(EE_GOAL_CTOR_GATHER, 1),
                    Call::Spec(EE_GOAL_CTOR_GATHER, target, EE_UNIT_ID_NONE, [0; 3], 1, 0),
                    Call::SetGoal(group, 6, queued),
                    Call::SetAction(group, EE_ACTION_GATHER_RESOURCE),
                    Call::Plan(group),
                    Call::Send(group, target, player, EE_GROUP_COMMAND_TARGET, 0),
                    Call::Random(0, 1),
                    Call::Acknowledge(player, group, 1),
                ]
            );
            assert_eq!(host.live, 0);
        }
    }

    #[test]
    fn gather_needs_a_selection_and_a_resource_left() {
        let (player, selection) = (r(0x500), r(0x600));
        for (amount, selected, owner) in [(0, true, 0), (-1, true, 0), (5, false, 0), (5, true, 3)] {
            let mut host = MockHost::default();
            host.units.insert(0x42, EeUnitRecord { owner, ..resource(0x42, amount) });
            if selected {
                host.groups.insert(0x600, vec![0x0100_0001]);
            }
            host.fills.insert(0, vec![0x0100_0001]);
            gather(&mut host, player, selection, 0x42, false);
            assert!(host.calls.is_empty(), "{amount} {selected} {owner}");
        }
        // the spread left no unit: pending, no goal
        let mut host = MockHost { mobile: vec![0x0100_0009], ..Default::default() };
        host.units.insert(0x42, resource(0x42, 5));
        host.groups.insert(0x600, vec![0x0100_0001]);
        host.fills.insert(0, vec![0x0100_0001]);
        gather(&mut host, player, selection, 0x42, false);
        assert!(matches!(host.calls.last(), Some(Call::SpreadResource(..))));
    }

    /// `EEMRepairUnit::Execute`'s calls: the spread around the target's tile; queued: build targets only; else every
    /// unit orders the repair, then every one is added back; the draw last
    #[test]
    fn repair_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0100_0007;
        let units = [0x0100_0001, 0x0100_0002];
        for queued in [false, true] {
            let mut host = MockHost::default();
            host.units.insert(target, resource(target, 0));
            host.groups.insert(0x600, units.to_vec());
            host.fills.insert(0, units.to_vec());
            repair(&mut host, player, selection, target, queued);
            let group = r(0x1001);
            let mut expected = vec![
                Call::NewGroup(player, group),
                Call::Fill(group, selection, 0, 0),
                Call::Pending(player, group),
                Call::SpreadTile(group, 40, 41),
            ];
            if queued {
                expected.extend(units.iter().map(|&u| Call::AddBuildTarget(u, target)));
            } else {
                expected.extend(units.iter().map(|&u| Call::OrderRepair(u, target)));
                expected.extend(units.iter().map(|&u| Call::AddMember(group, u)));
            }
            expected.extend([Call::Random(0, 1), Call::Acknowledge(player, group, 0)]);
            assert_eq!(host.calls, expected);
        }
        // no target, no selection: nothing
        let mut host = MockHost::default();
        host.groups.insert(0x600, units.to_vec());
        repair(&mut host, player, selection, 0x0100_0099, false);
        assert!(host.calls.is_empty());
        let mut host = MockHost::default();
        host.units.insert(target, resource(target, 0));
        repair(&mut host, player, selection, target, false);
        assert!(host.calls.is_empty());
    }
    /// `EEMAddPatrolPoint::Execute`'s calls: the keeps, the new patrol's point before the clear, the draw last
    #[test]
    fn add_patrol_point_makes_the_original_calls_in_order() {
        let mut host = MockHost { specs: true, ..Default::default() };
        let (player, selection) = (r(0x500), r(0x600));
        let first = 0x0100_0001;
        host.units.insert(first, complex(first, 3));
        host.groups.insert(0x600, vec![first, 0x0100_0002, 0x0100_0003]);
        host.fills.insert(2, vec![first, 0x0100_0002, 0x0100_0003]);
        host.fills.insert(3, vec![0x0100_0004]);
        host.mobile = vec![first, 0x0100_0002, 0x0100_0003];
        host.non_citizens = vec![0x0100_0002, 0x0100_0003];
        host.draws = vec![1];
        add_patrol_point(&mut host, player, selection, 12.5, 30.0);
        let (x, y) = (12.5f32.to_bits(), 30.0f32.to_bits());
        let g = |n: usize| r(0x1000 + n);
        let mut expected = vec![Call::GoalRoute(first)];
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 0, slot));
            match slot {
                2 | 3 => {
                    expected.extend([
                        Call::Pending(player, group),
                        Call::KeepMobile(group),
                        Call::KeepNonCitizens(group),
                        Call::GoalNew(EE_GOAL_CTOR_PATROL, 1),
                        Call::Spec(EE_GOAL_CTOR_PATROL, EE_UNIT_ID_NONE, EE_UNIT_ID_NONE, [0; 3], 1, 0),
                        Call::GoalAddPoint(9, x, y, player),
                        Call::ClearGoal(group),
                        Call::SetGoal(group, 9, false),
                        Call::SetAction(group, EE_ACTION_MOVE_TO_NEXT_PATROL_POINT),
                        Call::Plan(group),
                    ]);
                    if slot == 2 {
                        expected.extend([Call::Random(0, 1), Call::Acknowledge(player, group, 1)]);
                    }
                }
                _ => expected.push(Call::Release(group)),
            }
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0);
    }

    #[test]
    fn add_patrol_point_routes() {
        let (player, selection) = (r(0x500), r(0x600));
        let first = 0x0100_0001;
        let (x, y) = (4.0f32.to_bits(), 5.0f32.to_bits());
        // nothing selected: a lone patrol
        let mut host = MockHost::default();
        add_patrol_point(&mut host, player, selection, 4.0, 5.0);
        assert_eq!(host.calls, vec![Call::LonePatrol(player, x, y)]);
        // a path point first: the point into its route (out of the store: its type read on the object)
        for in_store in [true, false] {
            let mut host = MockHost::default();
            host.groups.insert(0x600, vec![first]);
            if in_store {
                host.units.insert(first, complex(first, EE_UNIT_TYPE_PATH_POINT));
            } else {
                host.object_types.insert(first, EE_UNIT_TYPE_PATH_POINT);
            }
            add_patrol_point(&mut host, player, selection, 4.0, 5.0);
            let tail = host.calls.last().unwrap();
            assert_eq!(tail, &Call::InsertPoint(first, x, y, player));
        }
        // a patrolling first unit: the point added to its goal's route
        let mut host = MockHost::default();
        host.groups.insert(0x600, vec![first]);
        host.units.insert(first, complex(first, 3));
        host.goal_routes.insert(first, 0x7700);
        add_patrol_point(&mut host, player, selection, 4.0, 5.0);
        assert_eq!(host.calls, vec![Call::GoalRoute(first), Call::AddPoint(r(0x7700), x, y, player)]);
    }

    #[test]
    fn the_non_citizens_test() {
        let soldier = EeUnitRecord { commandable: 1, category: 1, ..complex(1, 3) };
        assert!(keeps_non_citizen(Some(&soldier)));
        assert!(!keeps_non_citizen(Some(&EeUnitRecord { commandable: 0, ..soldier })));
        assert!(!keeps_non_citizen(Some(&EeUnitRecord { hit_points: 0, ..soldier })));
        assert!(!keeps_non_citizen(Some(&EeUnitRecord { category: EE_UNIT_CATEGORY_LAND_CITIZEN, ..soldier })));
        assert!(!keeps_non_citizen(Some(&EeUnitRecord { category: EE_UNIT_CATEGORY_NAVAL_CITIZEN, ..soldier })));
        assert!(!keeps_non_citizen(None));
    }
    /// `EEMAttackUnit::Execute`'s calls: the queued flag on every new group, the spread, the draw before the goal,
    /// the goal's attacker the selection's first unit as it is then, the marker on the target, the drawn unit
    #[test]
    fn attack_unit_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0200_0009;
        for (queued, convert) in [(false, false), (true, true)] {
            let mut host = MockHost { specs: true, ..Default::default() };
            host.units.insert(target, EeUnitRecord { tile_x: 7, tile_y: 8, ..complex(target, 5) });
            host.groups.insert(0x600, vec![0x0100_0003, 0x0100_0001]);
            host.fills.insert(2, vec![0x0100_0001, 0x0100_0003]);
            host.fills.insert(4, vec![0x0100_0005]);
            host.mobile = vec![0x0100_0001, 0x0100_0003];
            host.draws = vec![1];
            attack_unit(&mut host, player, selection, target, queued, convert);
            let g = |n: usize| r(0x1000 + n);
            let mut expected = Vec::new();
            for slot in 1..6 {
                let group = g(slot as usize);
                expected.push(Call::NewGroup(player, group));
                expected.push(Call::Fill(group, selection, 1, slot));
                expected.push(Call::Queued(group, queued));
                match slot {
                    2 => expected.extend([
                        Call::Pending(player, group),
                        Call::SpreadUnit(group, target, 7, 8),
                        Call::Random(0, 1),
                        Call::ClearGoal(group),
                        Call::GoalNew(EE_GOAL_CTOR_ATTACK_UNIT, 1),
                        Call::Spec(EE_GOAL_CTOR_ATTACK_UNIT, 0x0100_0003, target, [0; 3], 1, 0),
                        Call::SetGoal(group, 2, queued),
                        Call::SetAction(group, EE_ACTION_ATTACK_ENEMY_UNIT),
                        Call::Plan(group),
                        Call::Send(group, target, player, EE_GROUP_COMMAND_ATTACK, 0),
                        Call::AcknowledgeUnit(player, 0x0100_0003),
                    ]),
                    4 => expected.extend([Call::Pending(player, group), Call::SpreadUnit(group, target, 7, 8)]),
                    _ => expected.push(Call::Release(group)),
                }
            }
            assert_eq!(host.calls, expected);
            assert_eq!(host.live, 0);
        }
    }

    #[test]
    fn attack_unit_needs_a_selection_and_a_living_target() {
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0200_0009;
        for (hit_points, selected, owner) in [(0, true, 2), (-3, true, 2), (5, false, 2), (5, true, 3)] {
            let mut host = MockHost::default();
            host.units.insert(target, EeUnitRecord { hit_points, owner, ..complex(target, 5) });
            if selected {
                host.groups.insert(0x600, vec![0x0100_0001]);
            }
            host.fills.insert(1, vec![0x0100_0001]);
            attack_unit(&mut host, player, selection, target, false, false);
            assert!(host.calls.is_empty(), "{hit_points} {selected} {owner}");
        }
    }

    /// `EEMHealUnit::Execute`'s calls: the target's complex unit read once, a goal per group that took units, the
    /// draw after the plan; a target without a complex unit is refused
    #[test]
    fn heal_unit_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let target = 0x0100_0007;
        let mut host = MockHost { specs: true, ..Default::default() };
        host.units.insert(target, complex(target, 5));
        host.complex_of.insert(target, 0x0100_0008);
        host.fills.insert(5, vec![0x0100_0001, 0x0100_0002]);
        host.draws = vec![1];
        heal_unit(&mut host, player, selection, target);
        let g = |n: usize| r(0x1000 + n);
        let mut expected = vec![Call::ComplexOf(target)];
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 1, slot));
            if slot != 5 {
                expected.push(Call::Release(group));
                continue;
            }
            expected.extend([
                Call::Pending(player, group),
                Call::ClearGoal(group),
                Call::GoalNew(EE_GOAL_CTOR_HEAL_UNIT, 1),
                Call::Spec(EE_GOAL_CTOR_HEAL_UNIT, 0x0100_0008, EE_UNIT_ID_NONE, [0; 3], 1, 0),
                Call::SetGoal(group, 15, false),
                Call::SetAction(group, EE_ACTION_HEAL_UNIT),
                Call::Plan(group),
                Call::Random(0, 1),
                Call::Acknowledge(player, group, 1),
            ]);
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0);
        // no such unit, a unit of another list than its id names: nothing; no complex unit: refused after the read
        for owner in [None, Some(3)] {
            let mut host = MockHost::default();
            if let Some(owner) = owner {
                host.units.insert(target, EeUnitRecord { owner, ..complex(target, 5) });
            }
            heal_unit(&mut host, player, selection, target);
            assert!(host.calls.is_empty());
        }
        let mut host = MockHost::default();
        host.units.insert(target, complex(target, 5));
        host.complex_of.insert(target, EE_UNIT_ID_NONE);
        heal_unit(&mut host, player, selection, target);
        assert_eq!(host.calls, vec![Call::ComplexOf(target)]);
    }
    /// `EEMPathTo::Execute`'s calls: the point adjusted once, the goal before the paths' reset and the clear, the
    /// draw after the plan, the marker after the acknowledgement (also for a group the keep emptied)
    #[test]
    fn path_to_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let first = 0x0100_0001;
        let o = PathToOptions { facing: 0.75, forced: true, rotating: false, queued: true, group_movement: true };
        let mut host = MockHost { specs: true, path_point: Some((20.5, 21.5, 3.25)), ..Default::default() };
        host.groups.insert(0x600, vec![first, 0x0100_0002]);
        host.fills.insert(2, vec![first, 0x0100_0002]);
        host.fills.insert(3, vec![0x0100_0009]);
        host.mobile = vec![first, 0x0100_0002];
        host.draws = vec![1];
        assert_eq!(path_to(&mut host, player, selection, 20.2, 21.9, o), Some((20.5, 21.5)));
        let bits = [20.5f32.to_bits(), 21.5f32.to_bits(), 3.25f32.to_bits()];
        let g = |n: usize| r(0x1000 + n);
        let mut expected = vec![Call::PathPoint(first, 20.2f32.to_bits(), 21.9f32.to_bits())];
        for slot in 1..6 {
            let group = g(slot as usize);
            expected.push(Call::NewGroup(player, group));
            expected.push(Call::Fill(group, selection, 0, slot));
            if slot != 2 && slot != 3 {
                expected.push(Call::Release(group));
                continue;
            }
            expected.extend([
                Call::Pending(player, group),
                Call::KeepMobile(group),
                Call::GoalNew(EE_GOAL_CTOR_LOCATION, 1),
                Call::Spec(EE_GOAL_CTOR_LOCATION, EE_UNIT_ID_NONE, EE_UNIT_ID_NONE, bits, 1, 0),
                Call::Location(0.75f32.to_bits(), 0, 1),
                Call::ResetPaths(group),
                Call::ClearGoal(group),
                Call::SetGoal(group, 1, true),
                Call::SetAction(group, EE_ACTION_PREPARE_TO_MOVE),
                Call::Plan(group),
            ]);
            if slot == 2 {
                expected.extend([Call::Random(0, 1), Call::Acknowledge(player, group, 1)]);
            }
            expected.push(Call::Send(group, EE_UNIT_ID_NONE, player, EE_GROUP_COMMAND_MOVE, 0));
        }
        assert_eq!(host.calls, expected);
        assert_eq!(host.live, 0);
        // no selection: nothing, the point unchanged
        let mut host = MockHost::default();
        assert_eq!(path_to(&mut host, player, selection, 1.0, 2.0, o), None);
        assert!(host.calls.is_empty());
    }

    /// `EEMDropoffResource::Execute`'s calls: one group of the whole selection spread around the drop-off's tile, the
    /// Gather goal with no resource, the draw after the plan; nothing for a drop-off that is not a complex unit of the
    /// id's player, or for no selection; a group the spread emptied stays pending with no goal
    #[test]
    fn dropoff_resource_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let dropoff = 0x0100_0007;
        let mut host = MockHost { specs: true, ..Default::default() };
        host.units.insert(dropoff, EeUnitRecord { tile_x: 30, tile_y: 41, ..complex(dropoff, 0x50) });
        host.groups.insert(0x600, vec![0x0100_0001, 0x0100_0002]);
        host.fills.insert(0, vec![0x0100_0001, 0x0100_0002]);
        host.draws = vec![1];
        dropoff_resource(&mut host, player, selection, dropoff);
        let group = r(0x1001);
        assert_eq!(
            host.calls,
            vec![
                Call::NewGroup(player, group),
                Call::Fill(group, selection, 0, 0),
                Call::Pending(player, group),
                Call::SpreadTile(group, 30, 41),
                Call::ClearGoal(group),
                Call::GoalNew(EE_GOAL_CTOR_GATHER, 1),
                Call::Spec(EE_GOAL_CTOR_GATHER, EE_UNIT_ID_NONE, dropoff, [0; 3], 1, 0),
                Call::SetGoal(group, 6, false),
                Call::SetAction(group, EE_ACTION_DROPOFF_RESOURCE),
                Call::Plan(group),
                Call::Random(0, 1),
                Call::Acknowledge(player, group, 1),
            ]
        );
        assert_eq!(host.live, 0);
        // a drop-off of another player's id, a simple unit, an unknown id, or no selection: nothing
        for (id, record) in [
            (dropoff, EeUnitRecord { owner: 2, ..complex(dropoff, 0x50) }),
            (dropoff, EeUnitRecord { object_kind: crate::kinds::EE_OBJECT_UNIT, ..complex(dropoff, 0x50) }),
            (0x0100_0099, complex(dropoff, 0x50)),
        ] {
            let mut host = MockHost::default();
            host.units.insert(dropoff, record);
            host.groups.insert(0x600, vec![0x0100_0001]);
            dropoff_resource(&mut host, player, selection, id);
            assert!(host.calls.is_empty());
        }
        let mut host = MockHost::default();
        host.units.insert(dropoff, complex(dropoff, 0x50));
        dropoff_resource(&mut host, player, selection, dropoff);
        assert!(host.calls.is_empty());
        // the group is empty after the spread (here: Fill took nobody): pending, no goal, no draw
        let mut host = MockHost::default();
        host.units.insert(dropoff, complex(dropoff, 0x50));
        host.groups.insert(0x600, vec![0x0100_0001]);
        dropoff_resource(&mut host, player, selection, dropoff);
        let group = r(0x1001);
        assert_eq!(
            host.calls,
            vec![
                Call::NewGroup(player, group),
                Call::Fill(group, selection, 0, 0),
                Call::Pending(player, group),
                Call::SpreadTile(group, 0, 0),
            ]
        );
    }

    #[test]
    fn keeps_citizen_tests_the_store() {
        let citizen = EeUnitRecord { commandable: 1, category: EE_UNIT_CATEGORY_LAND_CITIZEN, ..complex(1, 0x20) };
        assert!(keeps_citizen(Some(&citizen)));
        assert!(keeps_citizen(Some(&EeUnitRecord { category: EE_UNIT_CATEGORY_NAVAL_CITIZEN, ..citizen })));
        assert!(!keeps_citizen(Some(&EeUnitRecord { category: 1, ..citizen })));
        assert!(!keeps_citizen(Some(&EeUnitRecord { commandable: 0, ..citizen })));
        assert!(!keeps_citizen(Some(&EeUnitRecord { hit_points: 0, ..citizen })));
        assert!(!keeps_citizen(None));
    }

    /// `EEMBuildWall::Execute`'s calls: the plan after the keep, the draw before the build, the pieces always
    /// released; no plan when no citizen is left, no build when the plan made no piece
    #[test]
    fn build_wall_makes_the_original_calls_in_order() {
        let (player, selection) = (r(0x500), r(0x600));
        let pieces = r(0x7700);
        let bits = [1.5f32.to_bits(), 2.5f32.to_bits(), 9.5f32.to_bits(), 2.5f32.to_bits()];
        for count in [3usize, 0] {
            let mut host = MockHost { wall_pieces: count, ..Default::default() };
            host.groups.insert(0x600, vec![0x0100_0001, 0x0100_0002, 0x0100_0003]);
            host.fills.insert(0, vec![0x0100_0001, 0x0100_0002, 0x0100_0003]);
            host.citizens = vec![0x0100_0001, 0x0100_0003];
            host.draws = vec![1];
            build_wall(&mut host, player, selection, 0x2a, (1.5, 2.5), (9.5, 2.5));
            let group = r(0x1001);
            let mut expected = vec![
                Call::NewGroup(player, group),
                Call::Fill(group, selection, 0, 0),
                Call::Pending(player, group),
                Call::KeepCitizens(group),
                Call::WallPlan(player, 0x2a, bits),
                Call::Random(0, 1),
                Call::Acknowledge(player, group, 1),
            ];
            if count > 0 {
                expected.push(Call::WallBuild(group, pieces));
            }
            expected.push(Call::WallRelease(pieces));
            assert_eq!(host.calls, expected, "{count} pieces");
        }
        // no citizen left: pending, kept, nothing planned
        let mut host = MockHost::default();
        host.groups.insert(0x600, vec![0x0100_0001]);
        host.fills.insert(0, vec![0x0100_0001]);
        build_wall(&mut host, player, selection, 0x2a, (1.5, 2.5), (9.5, 2.5));
        let group = r(0x1001);
        assert_eq!(
            host.calls,
            vec![
                Call::NewGroup(player, group),
                Call::Fill(group, selection, 0, 0),
                Call::Pending(player, group),
                Call::KeepCitizens(group),
            ]
        );
        // no selection: nothing
        let mut host = MockHost::default();
        build_wall(&mut host, player, selection, 0x2a, (1.5, 2.5), (9.5, 2.5));
        assert!(host.calls.is_empty());
    }
}
