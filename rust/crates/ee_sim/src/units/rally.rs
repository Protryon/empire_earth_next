//! The airports' and rally points' orders (RUST_PLAN R5b.4, the game messages lane, round 1):
//! `EEMSetAirportCommand::Execute`, `EEMSetRallyGoal::Execute`, `EEMClearAirportCommands::Execute` and
//! `EEMStopActiveGroup::Execute` over the store and host calls
//! (`src/Empire Earth/Units/EEUnitHost.cpp`). The C++ `Execute` hands the sender, its selection (the airport order)
//! or the building's id (the rally point) and the message's members; the bodies read the units' plain state from the
//! store, the players' diplomacy tables from `players`, and make every world call (goals, queues, actions, the
//! command markers) through the host. The class reads the store does not hold (visibility, the damage table, the
//! garrison capacity, the maximum hit points, the rally point's sharing, the queue count) are host reads.
//!
//! Determinism: the goals are created, cloned and queued in the original's order (each `Clone` makes a goal id).

use crate::units::group::{unit_type, world_unit};
use crate::units::host::{
    EeGoalSpec, GoalHandle, Host, HostRef, EE_GOAL_CTOR_ATTACK_UNIT, EE_GOAL_CTOR_BUILD_REPAIR, EE_GOAL_CTOR_GATHER,
    EE_GOAL_CTOR_LOCATION_2D, EE_GOAL_CTOR_LOCATION_XY, EE_GOAL_CTOR_PATROL_ROUTE, EE_UNIT_ID_NONE,
};
use crate::units::store::EeUnitRecord;

/// `kUnitTypeAirport`, `kUnitTypeNavyYard`, `kUnitTypeFarm`, `kUnitTypePathPoint` (ee/EEUnitClassIDs.h)
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_NAVY_YARD: i64 = 0x41;
const UNIT_TYPE_FARM: i64 = 0x2a;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
/// `kUnitCategoryAirplane`, `kUnitCategoryPlanetSpaceship`, `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
/// `kUnitFamilyFish`
const FAMILY_FISH: i64 = 0x12;
/// `kGoalAttackUnit`, `kGoalLoadGarrison` (ee/EEUnitGoal.h)
const GOAL_ATTACK_UNIT: u8 = 2;
const GOAL_LOAD_GARRISON: i32 = 12;
/// `kActionTakeoff`, `kActionPrepareToMove`, `kActionIdle` (ee/EEUnitAction.h)
const ACTION_TAKEOFF: i32 = 1000;
const ACTION_PREPARE_TO_MOVE: i32 = 8000;
const ACTION_IDLE: i64 = 5000;

/// the airport commands' and rally points' timed queues: 60 s
const QUEUE_TIME_MS: i64 = 60000;

/// The command markers of `EESendGroupCommand` the rally point shows: a move, an attack, a target.
const COMMAND_MOVE: i32 = 0xb8;
const COMMAND_ATTACK: i32 = 0xb9;
const COMMAND_TARGET: i32 = 0xba;

/// The location goal both bodies make: `new EEUGLocation(GE2DPoint(x, y), true, true, false, 0.0f, false, 0)`.
fn location_goal(host: &mut impl Host, x: f32, y: f32) -> Option<GoalHandle> {
    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION_2D);
    spec.point = [x, y, 0.0];
    host.goal_new(&spec)
}

/// `target->mPlayer != player && target->mPlayer->mEnemies[player->mPlayerID] != 0`: the target's owner is at war
/// with the player (the tables by player object; a player with no table is at war with no one).
fn at_war(host: &mut impl Host, target: &EeUnitRecord, player: u64) -> bool {
    if target.player == player {
        return false;
    }
    let Some(me) = host.player_record(player) else { return false };
    host.player_record(target.player).and_then(|p| p.enemies.get(me.id as usize).copied()).unwrap_or(0) != 0
}

/// The attack goal's copy for one airport (`EEAttackUnitCast(theGoal->Clone())`) and the goal a queue takes: the
/// copy once, else a new clone.
fn take(host: &mut impl Host, copy: &mut Option<GoalHandle>, goal: &GoalHandle) -> Option<GoalHandle> {
    match copy.take() {
        Some(c) => Some(c),
        None => host.goal_clone(goal),
    }
}

fn queue(host: &mut impl Host, unit: u32, goal: Option<GoalHandle>, queue: i64) {
    if let Some(g) = goal {
        host.unit_queue_goal(unit, g, queue);
    }
}

fn timer(host: &mut impl Host, unit: u32, q: i64) {
    host.unit_queue_timer(unit, q, QUEUE_TIME_MS);
}

/// `EEMSetAirportCommand::Execute`: the sender's selected airports get an order for their aircraft. The goal is an
/// attack on the target (a unit of a player at war with the sender that the sender sees and whose class can be
/// attacked: the damage table's row 0xc for the commands 8 and 0xe, else 0; command 0xf attacks any such unit) made
/// with the selection's first unit as the attacker (else the target's complex unit), or a move to the point; a
/// target id no unit has gives no goal. Each airport of the selection then takes a clone (an attack's own copy
/// first) in the queue(s) the command picks: 10/12 queue 3, 9/13 queue 2, 8/14 queue 1 (12-14 restart the queue's
/// 60 s timer), 15 all four (3 with the copy, then 2, 1, 4; the four timers restart), 20 a new move in queue 4;
/// unqueued orders clear the queue(s) first. The goal is deleted at the end.
#[allow(clippy::too_many_arguments)]
pub fn set_airport_command(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    x: f32,
    y: f32,
    command: i32,
    target_id: i32,
    queued: bool,
) {
    let mut ids = Vec::new();
    if !selection.0.is_null() {
        host.group_ids(selection, &mut ids);
    }
    let first = ids.first().copied();
    let damage_index: i64 = if command == 8 || command == 0xe { 0xc } else { 0 };
    let mut goal: Option<GoalHandle> = None;
    let mut attack = false;
    let mut found = true;
    if target_id != -1 {
        match world_unit(host, target_id as u32) {
            None => found = false,
            Some(target) => {
                if at_war(host, &target, player.0 as u64)
                    && host.unit_seen_by(target.id, player)
                    && (host.unit_attackable(target.id, damage_index) || command == 0xf)
                {
                    let attacker = match first {
                        Some(id) => id,
                        None => host.unit_complex_id(target.id),
                    };
                    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_UNIT);
                    spec.unit_a = attacker;
                    spec.unit_b = target.id;
                    spec.flag_a = 1;
                    goal = host.goal_new(&spec);
                    attack = true;
                }
            }
        }
    }
    if found && !attack {
        goal = location_goal(host, x, y);
    }
    if !selection.0.is_null() {
        for &unit in &ids {
            if unit_type(host, unit) != UNIT_TYPE_AIRPORT {
                continue;
            }
            let Some(g) = goal.as_ref() else { continue };
            let mut copy = if host.goal_kind(g) == GOAL_ATTACK_UNIT { host.goal_clone(g) } else { None };
            match command {
                10 | 12 | 9 | 13 | 8 | 14 => {
                    let q = match command {
                        10 | 12 => 3,
                        9 | 13 => 2,
                        _ => 1,
                    };
                    if !queued {
                        host.unit_clear_goal_queue(unit, q);
                    }
                    let t = take(host, &mut copy, g);
                    queue(host, unit, t, q);
                    if command >= 12 {
                        timer(host, unit, q);
                    }
                }
                15 => {
                    if !queued {
                        host.unit_clear_all_goal_queues(unit);
                    }
                    // each queue owns its own goal: the attack copy goes to the first one only
                    let t = take(host, &mut copy, g);
                    queue(host, unit, t, 3);
                    for q in [2, 1, 4] {
                        let c = host.goal_clone(g);
                        queue(host, unit, c, q);
                    }
                    for q in [4, 3, 2, 1] {
                        timer(host, unit, q);
                    }
                }
                20 => {
                    if !queued {
                        host.unit_clear_goal_queue(unit, 4);
                    }
                    let m = location_goal(host, x, y);
                    queue(host, unit, m, 4);
                }
                _ => {}
            }
            if let Some(c) = copy {
                host.goal_release(c);
            }
        }
    }
    if let Some(g) = goal {
        host.goal_release(g);
    }
}

/// What the rally point's target gives: the goal (none for a cleared rally point), the command marker and its
/// target, whether the rally point is cleared.
struct Rally {
    goal: Option<GoalHandle>,
    has_goal: bool,
    command: i32,
    target: u32,
    clear: bool,
}

/// The rally goal for a target (`EEMSetRallyGoal::Execute` up to its `issue` label): None when the body returns
/// before issuing (no goal, not cleared).
#[allow(clippy::too_many_arguments)]
fn rally_goal(
    host: &mut impl Host,
    building: &EeUnitRecord,
    owner: HostRef,
    x: f32,
    y: f32,
    target: &EeUnitRecord,
) -> Option<Rally> {
    let mut rally = Rally { goal: None, has_goal: false, command: 0, target: EE_UNIT_ID_NONE, clear: false };
    // a resource left that is not a living citizen: fish, a resource off the tiles, or one of the owner's built
    // units (a farm)
    if target.resource_amount > 0
        && !((target.category == CATEGORY_LAND_CITIZEN || target.category == CATEGORY_NAVAL_CITIZEN)
            && target.hit_points > 0)
        && (target.family == FAMILY_FISH
            || !host.unit_has_tile_node(target.id)
            || ({
                let complex = host.unit_complex_id(target.id);
                host.unit(complex).is_some_and(|c| c.built != 0)
            } && target.player == building.player))
    {
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_GATHER);
        spec.unit_a = target.id;
        spec.flag_a = 1;
        rally.goal = host.goal_new(&spec);
        rally.has_goal = true;
        rally.command = COMMAND_TARGET;
        rally.target = target.id;
        return Some(rally);
    }
    let location = |host: &mut _, mut rally: Rally| {
        rally.goal = location_goal(host, x, y);
        rally.has_goal = true;
        rally.command = COMMAND_MOVE;
        Some(rally)
    };
    if target.player != building.player {
        if !at_war(host, target, building.player) {
            return location(host, rally);
        }
        let visible = host.unit_seen_by(target.id, owner) || host.unit_ghost_seen(target.id);
        if visible
            && host.unit_class_targetable(target.id)
            && ((building.category != CATEGORY_AIRPLANE && building.category != CATEGORY_PLANET_SPACESHIP)
                || host.unit_can_attack_class(building.id, target.id))
        {
            let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_UNIT);
            spec.unit_a = building.id;
            spec.unit_b = target.id;
            spec.flag_a = 1;
            rally.goal = host.goal_new(&spec);
            rally.has_goal = true;
            rally.command = COMMAND_ATTACK;
            rally.target = target.id;
            return Some(rally);
        }
        return location(host, rally);
    }
    let goal = if target.unit_type == UNIT_TYPE_FARM
        || (target.land_building != 0 && target.hit_points < host.unit_max_hit_points(target.id))
    {
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_BUILD_REPAIR);
        spec.unit_a = target.id;
        spec.flag_a = 1;
        host.goal_new(&spec)
    } else if target.unit_type == UNIT_TYPE_PATH_POINT {
        let complex = host.unit_complex_id(target.id);
        let route = host.unit_patrol_group(complex);
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_PATROL_ROUTE);
        spec.unit_a = complex;
        spec.object = route.0;
        spec.flag_a = 1;
        host.goal_new(&spec)
    } else if host.unit_garrison_capacity(target.id) > 0 && host.unit_has_tile_node(target.id) && target.hit_points > 0
    {
        let garrison = host.unit_complex_id(target.id);
        if host.unit(garrison).is_some_and(|g| g.goal_kind == GOAL_LOAD_GARRISON) {
            host.unit_goal_clone(garrison)
        } else {
            // a new load goal the garrison takes; its clone is the rally goal (an invalid one gives none: return)
            rally.goal = host.garrison_rally_load(garrison);
            rally.goal.as_ref()?;
            rally.has_goal = true;
            rally.command = COMMAND_TARGET;
            rally.target = target.id;
            return Some(rally);
        }
    } else if target.id == building.id {
        rally.clear = true;
        rally.command = COMMAND_TARGET;
        rally.target = target.id;
        return Some(rally);
    } else {
        return location(host, rally);
    };
    // the goal is listed even when none was made (the original pushes the NULL and clones it)
    rally.goal = goal;
    rally.has_goal = true;
    rally.command = COMMAND_TARGET;
    rally.target = target.id;
    Some(rally)
}

/// `EEMSetRallyGoal::Execute`: the rally point of the building (`building_id`, its complex unit) and of the
/// building owner's selected units of its type (or sharing its rally point). The target gives the goal: a resource
/// to gather (0xba), an enemy the owner sees (or whose doppelganger is on its tile) that the building can attack
/// (0xb9; aircraft only a class they can attack), a farm or damaged land building to work or repair, a path point's
/// route to patrol, a garrison to enter (its load goal's clone; a new load goal the garrison takes first), the
/// building itself (the rally point cleared), else a move to the point (0xb8). Airports queue it in queues 3, 2, 1
/// and 4, navy yards in 1, 0 and 2 (both clear all queues unqueued), aircraft in 0 and 1 and take off (idle) or
/// prepare to move, the others in queue 1 (none without a second queue); then the marker and the sender's
/// acknowledgement of the selection's first unit. The goal is deleted at the end.
#[allow(clippy::too_many_arguments)]
pub fn set_rally_goal(
    host: &mut impl Host,
    player: HostRef,
    building_id: u32,
    x: f32,
    y: f32,
    target_id: i32,
    queued: bool,
) {
    let Some(building_unit) = world_unit(host, building_id) else { return };
    let complex = host.unit_complex_id(building_unit.id);
    let Some(building) = host.unit(complex) else { return };
    let owner = HostRef(building.player as usize as *mut std::ffi::c_void);
    let rally = if target_id != -1 {
        match world_unit(host, target_id as u32) {
            // no unit: nothing to issue
            None => return,
            Some(target) => rally_goal(host, &building, owner, x, y, &target),
        }
    } else {
        let goal = location_goal(host, x, y);
        Some(Rally { goal, has_goal: true, command: COMMAND_MOVE, target: EE_UNIT_ID_NONE, clear: false })
    };
    let Some(mut rally) = rally else { return };
    if !rally.has_goal && !rally.clear {
        return;
    }
    // the goals' list: the one goal (cleared: none)
    let listed = rally.has_goal && !rally.clear;
    let building_type = building.unit_type;
    let selection = host.player_selection(owner);
    if !selection.0.is_null() {
        let mut ids = Vec::new();
        host.group_ids(selection, &mut ids);
        for &unit in &ids {
            if unit_type(host, unit) != building_type && !host.unit_shares_rally_point(unit, building_type) {
                continue;
            }
            let category = host.unit(unit).map_or(-1, |u| u.category);
            if building_type == UNIT_TYPE_AIRPORT {
                if !queued {
                    host.unit_clear_all_goal_queues(unit);
                }
                if listed {
                    for q in [3, 2, 1, 4] {
                        clone_into(host, unit, rally.goal.as_ref(), q);
                    }
                }
            } else if building_type == UNIT_TYPE_NAVY_YARD {
                if !queued {
                    host.unit_clear_all_goal_queues(unit);
                }
                if listed {
                    for q in [1, 0, 2] {
                        clone_into(host, unit, rally.goal.as_ref(), q);
                    }
                }
            } else if category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP {
                if !queued {
                    host.unit_clear_goal_queue(unit, 0);
                    host.unit_clear_goal_queue(unit, 1);
                }
                if listed {
                    clone_into(host, unit, rally.goal.as_ref(), 0);
                    clone_into(host, unit, rally.goal.as_ref(), 1);
                    let command = host.unit(unit).map_or(0, |u| u.command);
                    let action = if command == ACTION_IDLE { ACTION_TAKEOFF } else { ACTION_PREPARE_TO_MOVE };
                    host.unit_start_action(unit, action);
                }
            } else {
                // a unit with a single goal queue (a ship that can set a rally point, e.g. a carrier) has no rally
                // queue: queue 1 would be past the end of its queues
                if host.unit_num_goal_queues(unit) < 2 {
                    continue;
                }
                if !queued || rally.clear {
                    host.unit_clear_goal_queue(unit, 1);
                }
                if listed {
                    clone_into(host, unit, rally.goal.as_ref(), 1);
                }
            }
        }
        if rally.command != 0 {
            host.send_group_command(selection, rally.target, player, rally.command, 1);
            let first = ids.first().copied().unwrap_or(EE_UNIT_ID_NONE);
            host.acknowledge_unit(player, first);
        }
    }
    if let Some(g) = rally.goal.take() {
        host.goal_release(g);
    }
}

/// `unit->QueueGoal(goal->Clone(), queue)` (a listed goal that was never made is not queued)
fn clone_into(host: &mut impl Host, unit: u32, goal: Option<&GoalHandle>, q: i64) {
    if let Some(g) = goal {
        let c = host.goal_clone(g);
        queue(host, unit, c, q);
    }
}

/// `kUnitCategoryNavalBuilding`, `kUnitCategorySpaceBuilding`
const CATEGORY_NAVAL_BUILDING: i64 = 0x15;
const CATEGORY_SPACE_BUILDING: i64 = 0x20;
/// `kActionAirportOpen`, `kActionFly`
const ACTION_AIRPORT_OPEN: i32 = 13000;
const ACTION_FLY: i64 = 1006;

/// A selected unit's plain state: the store's record, else (a unit already out of its list) the object's.
pub(crate) fn selected(host: &mut impl Host, id: u32) -> Option<EeUnitRecord> {
    match host.unit(id) {
        Some(unit) => Some(unit),
        None => host.object_record(id),
    }
}

/// `EEMClearAirportCommands::Execute`: each unit of the sender's selection (null: none) has its orders cleared: an
/// airport its five queues, a naval or space building with air traffic its queues 0 and 1, a navy yard its queues
/// 0 and 2 (each then opens: Airport Open, and its air traffic controller's queue for it is cleared), an aircraft
/// its queue 1; then every unit refreshes its goals. The queues are cleared alone (`mGoalQueues[i].Clear()`).
pub fn clear_airport_commands(host: &mut impl Host, selection: HostRef) {
    if selection.0.is_null() {
        return;
    }
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    for &unit in &ids {
        let (category, unit_type) = selected(host, unit).map_or((-1, -1), |u| (u.category, u.unit_type));
        let queues: &[i64] = if unit_type == UNIT_TYPE_AIRPORT {
            &[0, 1, 2, 3, 4]
        } else if (category == CATEGORY_NAVAL_BUILDING || category == CATEGORY_SPACE_BUILDING)
            && host.unit_air_traffic_type(unit) != 0
        {
            &[0, 1]
        } else if unit_type == UNIT_TYPE_NAVY_YARD {
            &[0, 2]
        } else {
            &[]
        };
        if !queues.is_empty() {
            for &q in queues {
                host.unit_queue_clear(unit, q);
            }
            host.unit_start_action(unit, ACTION_AIRPORT_OPEN);
            host.unit_clear_air_traffic(unit);
        } else if category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP {
            host.unit_queue_clear(unit, 1);
        }
        host.unit_refresh_goals(unit);
    }
}

/// An aircraft's command that keeps it as it is when stopped (taking off, taxiing, climbing, flying on, circling,
/// waiting for or requesting landing, holding, landing, descending, braking, boarding, searching): `kActionTakeoff`
/// and 1002-1005, 1007-1022 (`kActionFly`, 1006, is not one).
fn airborne(command: i64) -> bool {
    command == 1000 || (1002..=1005).contains(&command) || (1007..=1022).contains(&command)
}

/// `EEMStopActiveGroup::Execute`: when the sender's selection has a first unit, each of its living units stops: an
/// aircraft clears its build targets, its queue 0 (`ClearGoalQueue`) and its goals, and unless it is airborne
/// (`airborne`) takes a move to its own tile (`new EEUGLocation(tile x, tile y, 0, false, true, false, 0, false,
/// false)`) and prepares to move; then its class hears of the action. Any other unit halts. A null selection does
/// nothing (the original read through it).
pub fn stop_active_group(host: &mut impl Host, selection: HostRef) {
    if selection.0.is_null() {
        return;
    }
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    if ids.first().is_none_or(|&id| id == EE_UNIT_ID_NONE) {
        return;
    }
    for &unit in &ids {
        let Some(record) = selected(host, unit) else { continue };
        if record.hit_points <= 0 {
            continue;
        }
        if record.category != CATEGORY_AIRPLANE && record.category != CATEGORY_PLANET_SPACESHIP {
            host.unit_halt(unit);
            continue;
        }
        host.unit_clear_build_targets(unit);
        host.unit_clear_goal_queue(unit, 0);
        host.unit_clear_goals(unit);
        if record.command == ACTION_FLY || !airborne(record.command) {
            let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION_XY);
            spec.point = [record.tile_x as f32, record.tile_y as f32, 0.0];
            if let Some(goal) = host.goal_new(&spec) {
                host.unit_set_goal(unit, goal);
            }
            // StartAction(PrepareToMove), then the class's ActionChanged
            host.unit_start_action(unit, ACTION_PREPARE_TO_MOVE);
        } else {
            host.unit_action_changed(unit);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::units::host::tests::{Call, MockHost};
    use crate::units::kinds::EE_OBJECT_COMPLEX_UNIT;
    use crate::units::players::PlayerRecord;
    use std::ffi::c_void;

    const P: usize = 0x500;
    const Q: usize = 0x600;
    const S: usize = 0x10;

    fn r(a: usize) -> HostRef {
        HostRef(a as *mut c_void)
    }

    fn unit(id: u32, player: usize, unit_type: i64, category: i64) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            player: player as u64,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            unit_type,
            category,
            hit_points: 100,
            built: 1,
            goal_kind: -1,
            ..Default::default()
        }
    }

    fn host() -> MockHost {
        let mut h = MockHost::default();
        h.players.insert(P as u64, PlayerRecord { id: 1, ..Default::default() });
        let mut enemy = PlayerRecord { id: 2, ..Default::default() };
        enemy.enemies[1] = 1;
        h.players.insert(Q as u64, enemy);
        h
    }

    fn queues(h: &MockHost) -> Vec<(u32, u8, i64)> {
        h.calls
            .iter()
            .filter_map(|c| match c {
                Call::QueueGoal(u, k, q) => Some((*u, *k, *q)),
                _ => None,
            })
            .collect()
    }

    #[test]
    fn airport_attack_goes_to_every_queue_with_the_copy_first() {
        let mut h = host();
        let airport = 0x0100_0001;
        let plane = 0x0100_0002;
        let enemy = 0x0200_0001;
        h.units.insert(airport, unit(airport, P, UNIT_TYPE_AIRPORT, 0x13));
        h.units.insert(plane, unit(plane, P, 0x100, CATEGORY_AIRPLANE));
        h.units.insert(enemy, unit(enemy, Q, 0x10, 1));
        h.groups.insert(S, vec![airport, plane]);
        h.seen.push(enemy);
        h.attackable.push(enemy);
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 15, enemy as i32, false);
        assert_eq!(queues(&h), vec![(airport, 2, 3), (airport, 2, 2), (airport, 2, 1), (airport, 2, 4)]);
        assert!(h.calls.contains(&Call::ClearAllQueues(airport)));
        let timers: Vec<_> =
            h.calls.iter().filter(|c| matches!(c, Call::QueueTimer(..))).map(|c| format!("{c:?}")).collect();
        assert_eq!(timers.len(), 4);
        assert!(h.calls.contains(&Call::Attackable(enemy, 0)));
        assert_eq!(h.live, 0);
    }

    #[test]
    fn airport_moves_when_the_target_is_not_an_enemy_and_gives_nothing_for_an_unknown_id() {
        let mut h = host();
        let airport = 0x0100_0001;
        let own = 0x0100_0003;
        h.units.insert(airport, unit(airport, P, UNIT_TYPE_AIRPORT, 0x13));
        h.units.insert(own, unit(own, P, 0x10, 1));
        h.groups.insert(S, vec![airport]);
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 12, own as i32, true);
        assert_eq!(queues(&h), vec![(airport, 1, 3)]);
        assert!(h.calls.contains(&Call::QueueTimer(airport, 3, 60000)));
        assert!(!h.calls.contains(&Call::ClearQueue(airport, 3)));
        h.calls.clear();
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 12, 0x0300_0009, false);
        assert!(queues(&h).is_empty());
        // command 20: a new move in queue 4; the shared move goal is deleted
        h.calls.clear();
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 20, -1, false);
        assert_eq!(queues(&h), vec![(airport, 1, 4)]);
        assert!(h.calls.contains(&Call::ClearQueue(airport, 4)));
        // no selection
        set_airport_command(&mut h, r(P), r(0), 3.0, 4.0, 8, -1, false);
        assert_eq!(h.live, 0);
    }

    #[test]
    fn airport_attack_of_an_unseen_enemy_moves_and_damage_row_follows_the_command() {
        let mut h = host();
        let airport = 0x0100_0001;
        let enemy = 0x0200_0001;
        h.units.insert(airport, unit(airport, P, UNIT_TYPE_AIRPORT, 0x13));
        h.units.insert(enemy, unit(enemy, Q, 0x10, 1));
        h.groups.insert(S, vec![airport]);
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 8, enemy as i32, false);
        assert_eq!(queues(&h), vec![(airport, 1, 1)]);
        h.seen.push(enemy);
        h.calls.clear();
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 8, enemy as i32, false);
        assert!(h.calls.contains(&Call::Attackable(enemy, 0xc)));
        assert_eq!(queues(&h), vec![(airport, 1, 1)]);
        h.calls.clear();
        // 0xf (15) attacks a seen enemy whatever the damage table says, in all four queues
        set_airport_command(&mut h, r(P), r(S), 3.0, 4.0, 0xf, enemy as i32, false);
        assert_eq!(queues(&h), vec![(airport, 2, 3), (airport, 2, 2), (airport, 2, 1), (airport, 2, 4)]);
        assert_eq!(h.live, 0);
    }

    #[test]
    fn rally_point_by_target() {
        let mut h = host();
        let barracks = 0x0100_0001;
        let soldier = 0x0100_0002;
        let tree = 0x0000_0005;
        let enemy = 0x0200_0001;
        h.units.insert(barracks, unit(barracks, P, 0x30, 0x13));
        h.units.insert(soldier, unit(soldier, P, 0x30, 0x13));
        let mut t = unit(tree, 0, 0x99, 0x1f);
        t.resource_amount = 50;
        h.units.insert(tree, t);
        // gaia's tree, not the owner's: gathered when it is off the tiles
        h.off_tiles.push(tree);
        h.units.insert(enemy, unit(enemy, Q, 0x10, 1));
        h.groups.insert(S, vec![barracks, soldier]);
        h.selections.insert(P, S);
        // a resource: gather, target marker
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, tree as i32, false);
        assert_eq!(queues(&h), vec![(barracks, 6, 1), (soldier, 6, 1)]);
        assert!(h.calls.contains(&Call::Send(r(S), tree, r(P), COMMAND_TARGET, 1)));
        assert!(h.calls.contains(&Call::AcknowledgeUnit(r(P), barracks)));
        // an enemy seen and targetable: attack
        h.calls.clear();
        h.seen.push(enemy);
        h.targetable.push(enemy);
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, enemy as i32, true);
        assert_eq!(queues(&h), vec![(barracks, 2, 1), (soldier, 2, 1)]);
        assert!(!h.calls.contains(&Call::ClearQueue(barracks, 1)));
        // the building itself: cleared
        h.calls.clear();
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, barracks as i32, true);
        assert!(queues(&h).is_empty());
        assert!(h.calls.contains(&Call::ClearQueue(barracks, 1)));
        assert!(h.calls.contains(&Call::Send(r(S), barracks, r(P), COMMAND_TARGET, 1)));
        // no target: move
        h.calls.clear();
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, -1, false);
        assert_eq!(queues(&h), vec![(barracks, 1, 1), (soldier, 1, 1)]);
        assert!(h.calls.contains(&Call::Send(r(S), EE_UNIT_ID_NONE, r(P), COMMAND_MOVE, 1)));
        // an unknown target, an unknown building: nothing
        h.calls.clear();
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, 0x0300_0001, false);
        set_rally_goal(&mut h, r(P), 0x0100_0077, 1.0, 2.0, -1, false);
        assert!(queues(&h).is_empty());
        assert_eq!(h.live, 0);
    }

    #[test]
    fn rally_point_at_an_unseen_enemy_in_a_carrier_moves() {
        // the enemy is inside a carrier: unseen, off the tiles; the ghost test answers "not seen" (the original read
        // the NULL tile) and the rally point is a move, not an attack
        let mut h = host();
        let barracks = 0x0100_0001;
        let pilot = 0x0200_0003;
        h.units.insert(barracks, unit(barracks, P, 0x30, 0x13));
        h.units.insert(pilot, unit(pilot, Q, 0x10, CATEGORY_AIRPLANE));
        h.off_tiles.push(pilot);
        h.ghosts.push(pilot);
        h.targetable.push(pilot);
        h.groups.insert(S, vec![barracks]);
        h.selections.insert(P, S);
        set_rally_goal(&mut h, r(P), barracks, 1.0, 2.0, pilot as i32, false);
        assert!(h.calls.contains(&Call::SeenBy(pilot, r(P))));
        assert!(h.calls.contains(&Call::GhostSeen(pilot)));
        assert_eq!(queues(&h), vec![(barracks, 1, 1)]);
        assert!(h.calls.contains(&Call::Send(r(S), EE_UNIT_ID_NONE, r(P), COMMAND_MOVE, 1)));
        assert_eq!(h.live, 0);
    }

    #[test]
    fn rally_point_garrison_and_airport_and_aircraft() {
        let mut h = host();
        let airport = 0x0100_0001;
        let plane = 0x0100_0002;
        let tower = 0x0100_0003;
        h.units.insert(airport, unit(airport, P, UNIT_TYPE_AIRPORT, 0x13));
        let mut p = unit(plane, P, 0x101, CATEGORY_AIRPLANE);
        p.command = ACTION_IDLE;
        h.units.insert(plane, p);
        h.units.insert(tower, unit(tower, P, 0x31, 0x13));
        h.groups.insert(S, vec![airport, plane]);
        h.selections.insert(P, S);
        h.shares_rally.push(plane);
        h.garrison_capacity.insert(tower, 5);
        // an invalid load goal: nothing issued
        set_rally_goal(&mut h, r(P), airport, 1.0, 2.0, tower as i32, false);
        assert!(queues(&h).is_empty());
        assert!(!h.calls.iter().any(|c| matches!(c, Call::Send(..))));
        // a valid one: the clone in the four queues of the airport and of the aircraft sharing its rally point (the
        // building's type picks the queues)
        h.garrison_valid = true;
        h.calls.clear();
        set_rally_goal(&mut h, r(P), airport, 1.0, 2.0, tower as i32, false);
        let four = |u| [(u, 12, 3), (u, 12, 2), (u, 12, 1), (u, 12, 4)];
        assert_eq!(queues(&h), [four(airport), four(plane)].concat());
        // a carrier's rally point: the aircraft's queues 0 and 1, take off from idle
        let carrier = 0x0100_0004;
        h.units.insert(carrier, unit(carrier, P, 0x60, 0x9));
        h.groups.insert(S, vec![carrier, plane]);
        h.calls.clear();
        set_rally_goal(&mut h, r(P), carrier, 1.0, 2.0, -1, false);
        assert_eq!(queues(&h), vec![(carrier, 1, 1), (plane, 1, 0), (plane, 1, 1)]);
        assert!(h.calls.contains(&Call::StartAction(plane, ACTION_TAKEOFF)));
        h.groups.insert(S, vec![airport, plane]);
        // the tower already loading: its goal's clone
        h.units.get_mut(&tower).unwrap().goal_kind = GOAL_LOAD_GARRISON;
        h.calls.clear();
        set_rally_goal(&mut h, r(P), airport, 1.0, 2.0, tower as i32, true);
        assert!(h.calls.contains(&Call::GoalStateClone(tower)));
        assert!(!h.calls.contains(&Call::GarrisonLoad(tower)));
        assert_eq!(h.live, 0);
    }

    #[test]
    fn rally_point_repairs_a_damaged_building_and_skips_single_queue_units() {
        let mut h = host();
        let dock = 0x0100_0001;
        let ship = 0x0100_0002;
        let house = 0x0100_0003;
        h.units.insert(dock, unit(dock, P, 0x50, 0x13));
        h.units.insert(ship, unit(ship, P, 0x50, 0x9));
        let mut b = unit(house, P, 0x33, 0x13);
        b.land_building = 1;
        b.hit_points = 10;
        h.units.insert(house, b);
        h.max_hp.insert(house, 100);
        h.num_queues.insert(ship, 1);
        h.groups.insert(S, vec![dock, ship]);
        h.selections.insert(P, S);
        set_rally_goal(&mut h, r(P), dock, 1.0, 2.0, house as i32, false);
        assert_eq!(queues(&h), vec![(dock, 4, 1)]);
        assert!(h.calls.contains(&Call::MaxHitPoints(house)));
        assert_eq!(h.live, 0);
    }

    #[test]
    fn clearing_airport_commands_by_kind() {
        let mut h = host();
        let airport = 0x0100_0001;
        let carrier = 0x0100_0002;
        let yard = 0x0100_0003;
        let plane = 0x0100_0004;
        let soldier = 0x0100_0005;
        h.units.insert(airport, unit(airport, P, UNIT_TYPE_AIRPORT, 0x13));
        h.units.insert(carrier, unit(carrier, P, 0x77, CATEGORY_NAVAL_BUILDING));
        h.units.insert(yard, unit(yard, P, UNIT_TYPE_NAVY_YARD, CATEGORY_NAVAL_BUILDING));
        h.units.insert(plane, unit(plane, P, 0x101, CATEGORY_AIRPLANE));
        h.objects.insert(soldier, unit(soldier, P, 0x10, 1));
        h.air_traffic.insert(carrier, 2);
        h.groups.insert(S, vec![airport, carrier, yard, plane, soldier]);
        clear_airport_commands(&mut h, r(S));
        use Call::*;
        assert_eq!(
            h.calls,
            vec![
                QueueClear(airport, 0),
                QueueClear(airport, 1),
                QueueClear(airport, 2),
                QueueClear(airport, 3),
                QueueClear(airport, 4),
                StartAction(airport, ACTION_AIRPORT_OPEN),
                ClearAirTraffic(airport),
                RefreshGoals(airport),
                QueueClear(carrier, 0),
                QueueClear(carrier, 1),
                StartAction(carrier, ACTION_AIRPORT_OPEN),
                ClearAirTraffic(carrier),
                RefreshGoals(carrier),
                QueueClear(yard, 0),
                QueueClear(yard, 2),
                StartAction(yard, ACTION_AIRPORT_OPEN),
                ClearAirTraffic(yard),
                RefreshGoals(yard),
                QueueClear(plane, 1),
                RefreshGoals(plane),
                ObjectRecord(soldier),
                RefreshGoals(soldier),
            ]
        );
        h.calls.clear();
        clear_airport_commands(&mut h, r(0));
        assert!(h.calls.is_empty());
    }

    #[test]
    fn stopping_the_selection() {
        let mut h = host();
        let flying = 0x0100_0001;
        let landing = 0x0100_0002;
        let soldier = 0x0100_0003;
        let dead = 0x0100_0004;
        let mut f = unit(flying, P, 0x101, CATEGORY_AIRPLANE);
        f.command = ACTION_FLY;
        f.tile_x = 7;
        f.tile_y = 9;
        h.units.insert(flying, f);
        let mut l = unit(landing, P, 0x101, CATEGORY_PLANET_SPACESHIP);
        l.command = 1011;
        h.units.insert(landing, l);
        h.units.insert(soldier, unit(soldier, P, 0x10, 1));
        let mut d = unit(dead, P, 0x10, 1);
        d.hit_points = 0;
        h.units.insert(dead, d);
        h.groups.insert(S, vec![flying, landing, soldier, dead]);
        h.specs = true;
        stop_active_group(&mut h, r(S));
        use Call::*;
        let seven = 7.0f32.to_bits();
        let nine = 9.0f32.to_bits();
        assert_eq!(
            h.calls,
            vec![
                ClearBuildTargets(flying),
                ClearQueue(flying, 0),
                ClearGoals(flying),
                GoalNew(EE_GOAL_CTOR_LOCATION_XY, 0),
                Spec(EE_GOAL_CTOR_LOCATION_XY, EE_UNIT_ID_NONE, EE_UNIT_ID_NONE, [seven, nine, 0], 0, 0),
                UnitSetGoal(flying, 1),
                StartAction(flying, ACTION_PREPARE_TO_MOVE),
                ClearBuildTargets(landing),
                ClearQueue(landing, 0),
                ClearGoals(landing),
                ActionChanged(landing),
                Halt(soldier),
            ]
        );
        assert!(airborne(1000) && !airborne(1001) && !airborne(1006) && airborne(1022) && !airborne(1023));
        h.calls.clear();
        h.groups.insert(S, vec![]);
        stop_active_group(&mut h, r(S));
        stop_active_group(&mut h, r(0));
        assert!(h.calls.is_empty());
        assert_eq!(h.live, 0);
    }
}
