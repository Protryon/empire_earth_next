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
    EeGoalSpec, Host, HostRef, EE_GOAL_CTOR_ATTACK_GROUND, EE_GOAL_CTOR_EXPLORE, EE_GOAL_CTOR_PATROL_ROUTE,
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
}
