//! The last three goal-giving group commands over the unit store and host calls (RUST_PLAN R5b.4 leaf 4 (c)):
//! Build Unit (`EEMBuildUnit::Execute`), Smart Farmers (`EEMSmartFarmers::Execute`) and Cast Calamity
//! (`EEMCastCalamity::Execute`). Their calls are the original's, in its order, with its draws; the reads of plain
//! state (hit points, tiles, goal kinds, resource amounts, unit types) come from the store, every other read and
//! every world change is a host call (EEUnitHost.cpp). The farm sorts stay the C++ `std::sort`s (an unstable sort:
//! equal keys keep the order the C++ algorithm gives them); the calamity classes stay C++ (`CanCastAt`, the cost,
//! the casters' split are host calls).

use crate::group::{unit_type, world_complex_unit, world_unit, EE_ACTION_PREPARE_TO_MOVE};
use crate::host::{
    EeGoalSpec, GoalHandle, Host, HostRef, EE_GOAL_CTOR_CAST_CALAMITY, EE_GOAL_CTOR_GATHER, EE_GOAL_CTOR_LOCATION_2D,
    EE_UNIT_ID_NONE,
};

/// `kUnitTypeCapitol`, `kUnitTypeGranary`, `kUnitTypeGranaryImproved` (ee/EEUnitClassIDs.h)
pub const EE_UNIT_TYPE_CAPITOL: i64 = 0x29;
pub const EE_UNIT_TYPE_GRANARY: i64 = 0x34;
pub const EE_UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
/// `kGoalBuildRepair` (`EEUnitGoal::mKind`)
pub const EE_GOAL_KIND_VALUE_BUILD_REPAIR: i32 = 4;
/// `kActionIdle`, `kActionMoveToStorage`, `kActionCalamityPreCast` (ee/EEUnitAction.h)
pub const EE_ACTION_IDLE: i32 = 5000;
pub const EE_ACTION_MOVE_TO_STORAGE: i32 = 8009;
pub const EE_ACTION_CALAMITY_PRE_CAST: i32 = 2030;
/// `kTechTeleport`, `kTechConquistadorsSpell` (ee/EETechTreeTechID.h)
pub const EE_TECH_TELEPORT: i32 = 1916;
pub const EE_TECH_CONQUISTADORS_SPELL: i32 = 7020;
/// the calamity class whose cast every selected caster takes a copy of (`mClassEnum` 0x12)
pub const EE_CALAMITY_CLASS_GROUP_CAST: i32 = 0x12;

fn is_granary(unit_type: i64) -> bool {
    unit_type == EE_UNIT_TYPE_GRANARY || unit_type == EE_UNIT_TYPE_GRANARY_IMPROVED
}

/// A unit's tile from the store ((0, 0) for a unit in no list: the bodies read only units of groups just filled).
fn tile(host: &mut impl Host, id: u32) -> (i64, i64) {
    host.unit(id).map_or((0, 0), |u| (u.tile_x, u.tile_y))
}

/// A unit's hit points: the store's, else (a selected unit already out of its list) the object's.
fn hit_points(host: &mut impl Host, id: u32) -> i64 {
    match host.unit(id) {
        Some(unit) => unit.hit_points,
        None => host.object_hit_points(id),
    }
}

/// `EEMBuildUnit::Execute`: with a selection, one active group of the whole selection (slot 0, unsorted) goes to the
/// player's pending groups and keeps its citizens. With some left, the technology's unit class (a capitol only while
/// the player has none) is produced when the player can (`CanProduce(.., 0x20, 1, true, 3, NULL)`) and the class can
/// be placed at the tile (`EECanPlaceOnTile(.., false, 1)`): the building is made there (`EEMakeBuilding`). For a
/// granary the free farms around it (`EEFindFreeFarm` ring by ring) are sorted around the first unit's tile
/// (`EETileDistanceLess`). Each unit of the group, queued, takes the building as a build target (the build marked
/// shift-queued; a unit with another goal than a build goes idle), else is ordered to build it (`OrderRepair`); for a
/// granary it also takes the farm at its place in the order, or past the farms a farm drawn by `GetRandom`. A unit
/// the order moved out of the group is added back after the loop, and one unit, drawn, acknowledges.
pub fn build_unit(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    technology: i32,
    x: i64,
    y: i64,
    queued: bool,
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
    let Some(&first) = ids.first() else { return };
    // Behaviour change: a technology the player has no node of is refused (the original read through NULL)
    let Some(class) = host.technology_unit_class(player, technology) else { return };
    if class == EE_UNIT_TYPE_CAPITOL && host.player_num_capitols(player) != 0 {
        return;
    }
    if !host.can_produce(player, technology) || !host.can_place_on_tile(player, class, x, y) {
        return;
    }
    let building = host.make_building(player, class, x, y);
    let granary = is_granary(class);
    let mut farms = Vec::new();
    if granary {
        let complex = host.unit_complex_id(building);
        host.free_farms(complex, &mut farms);
        if !farms.is_empty() {
            let (fx, fy) = tile(host, first);
            host.sort_farms(&mut farms, fx, fy, false);
        }
    }
    let mut moved = Vec::new();
    let (mut i, mut j) = (0usize, 0usize);
    loop {
        host.group_ids(group, &mut ids);
        let Some(&worker) = ids.get(i) else { break };
        let count = ids.len();
        if queued {
            host.unit_add_build_target(worker, building);
            let kind = host.unit(worker).map_or(-1, |u| u.goal_kind);
            if kind != -1 && kind != EE_GOAL_KIND_VALUE_BUILD_REPAIR {
                host.unit_start_action(worker, EE_ACTION_IDLE);
            }
        } else {
            host.unit_order_build(worker, building);
        }
        if granary {
            if j < farms.len() {
                host.unit_add_farm_target(worker, farms[j]);
            } else if !farms.is_empty() {
                let k = host.random(0, farms.len() as i32 - 1);
                host.unit_add_farm_target(worker, farms[k as usize]);
            }
        }
        host.group_ids(group, &mut ids);
        if ids.len() != count {
            moved.push(worker);
        } else {
            i += 1;
        }
        j += 1;
    }
    for &unit in &moved {
        host.group_add_member(group, unit);
    }
    host.group_ids(group, &mut ids);
    if !ids.is_empty() {
        let index = host.random(0, ids.len() as i32 - 1);
        host.play_acknowledgement(player, group, index);
    }
}

/// `EEMSmartFarmers::Execute`: with a selection and a target that is a complex unit, one active group of the whole
/// selection (slot 0, unsorted) keeps its citizens. With some left, the target's free farms are found
/// (`EEFindFreeFarm` ring by ring), and each unit, in order: a farmer whose farm has the target among its drop sites
/// (`EEGetFarmDropSites`), when the target takes its load, drops it there (`SetGatheredResource(NULL)`, Move To
/// Storage); a farmer keeps its own farm; another unit takes the nearest free farm (the farms sorted with
/// `EEFarmDistanceGreater` around its tile, the last taken); a unit with a farm works it (`EEWorkFarm`); else a unit
/// carrying something the target takes is sent to drop it (`new EEUGGather(NULL, target, true)`, Move To Storage),
/// any other moves to the target's tile (`new EEUGLocation(GE2DPoint(tile), true, true, false, 0, false, 0)`, Prepare
/// To Move). One unit, drawn, acknowledges. The group then goes to the player's pending groups and finishes.
pub fn smart_farmers(host: &mut impl Host, player: HostRef, selection: HostRef, target_id: u32) {
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    let first = ids.first().copied();
    let target = world_complex_unit(host, target_id);
    let (Some(_), Some(target)) = (first, target) else { return };
    let group = host.active_group_new(player);
    host.group_fill(group, selection, 0, 0);
    host.group_keep_citizens(group);
    host.group_ids(group, &mut ids);
    let count = ids.len();
    if count > 0 {
        let mut farms = Vec::new();
        host.free_farms(target, &mut farms);
        let mut sites = Vec::new();
        for i in 0..count {
            host.group_ids(group, &mut ids);
            // (the original read past a group that shrank: none of these calls takes a unit out of it)
            let Some(&unit) = ids.get(i) else { break };
            let mut farm = host.unit_load_farm(unit);
            if farm != EE_UNIT_ID_NONE {
                if host.farm_drop_sites(farm, &mut sites) {
                    if sites.contains(&target) && host.can_take(target, unit) {
                        host.unit_drop_gathered(unit);
                        host.unit_start_action(unit, EE_ACTION_MOVE_TO_STORAGE);
                        continue;
                    }
                } else {
                    farm = EE_UNIT_ID_NONE;
                }
            }
            if farm == EE_UNIT_ID_NONE && !farms.is_empty() {
                let (ux, uy) = tile(host, unit);
                host.sort_farms(&mut farms, ux, uy, true);
                farm = farms.pop().unwrap_or(EE_UNIT_ID_NONE);
            }
            if farm != EE_UNIT_ID_NONE {
                host.work_farm(farm, unit);
                continue;
            }
            let carried = host.unit(unit).map_or(0, |u| u.resource_amount);
            let (goal, action) = if carried > 0 && host.can_take(target, unit) {
                let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_GATHER);
                spec.unit_b = target;
                spec.flag_a = 1;
                (host.goal_new(&spec), EE_ACTION_MOVE_TO_STORAGE)
            } else {
                let (tx, ty) = tile(host, target);
                let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION_2D);
                spec.point = [tx as f32, ty as f32, 0.0];
                (host.goal_new(&spec), EE_ACTION_PREPARE_TO_MOVE)
            };
            if let Some(goal) = goal {
                host.unit_set_goal(unit, goal);
            }
            host.unit_start_action(unit, action);
        }
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let index = host.random(0, ids.len() as i32 - 1);
            host.play_acknowledgement(player, group, index);
        }
    }
    host.pending_push(player, group);
    host.group_finish(group);
}

/// A new cast goal for the unit (`new EEUGCastCalamity(unit, target, x, y, technology, true)`).
fn cast_goal(host: &mut impl Host, unit: u32, target: u32, x: i32, y: i32, technology: i32) -> Option<GoalHandle> {
    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_CAST_CALAMITY);
    spec.unit_a = unit;
    spec.unit_b = target;
    spec.ints = [i64::from(x), i64::from(y), i64::from(technology)];
    spec.flag_a = 1;
    host.goal_new(&spec)
}

/// `EEMCastCalamity::Execute`: the caster is the complex unit with the id, alive; the technology's node and calamity
/// class are the sender's; the target is the unit with the id (none for -1).
/// - Class 0x12: when the caster can cast at the tile (`CanCastAt(.., true, target) == 0`), a new active group of the
///   selection (slot 0, unsorted) splits off the units that cannot cast (`SplitCalamityCasters`: they attack the
///   target); with casters left, each leaves the group, drops its goals and takes a copy of the cast goal (made for
///   the caster), with the action Prepare To Move; the goal is deleted and the group released.
/// - Else a new active group (the caster's player's) takes the casters: for a technology other than Teleport whose
///   class is not cast by all the selected, the first selected unit, in order, that is alive, can cast at the tile,
///   whose class is in the node and whose power pays the class's cost (`CanCastAt` asked first, for every unit tried);
///   for the others every selected unit that is alive, in the node and can cast there. Each gets its own cast goal,
///   the action Calamity Pre Cast (a capitol, or the Conquistadors' spell) or Prepare To Move, and joins the group,
///   which goes to the player's pending groups.
#[allow(clippy::too_many_arguments)]
pub fn cast_calamity(
    host: &mut impl Host,
    player: HostRef,
    selection: HostRef,
    caster_id: u32,
    target_id: i32,
    x: i32,
    y: i32,
    technology: i32,
) {
    let Some(caster) = world_complex_unit(host, caster_id) else { return };
    if host.unit(caster).map_or(0, |u| u.hit_points) <= 0 {
        return;
    }
    // Behaviour change: a node without a calamity class is refused (the original read through NULL)
    let Some((class, class_enum, cast_by_all)) = host.calamity_of(player, technology) else { return };
    let target = if target_id != -1 {
        world_unit(host, target_id as u32).map_or(EE_UNIT_ID_NONE, |_| target_id as u32)
    } else {
        EE_UNIT_ID_NONE
    };
    let mut ids = Vec::new();
    if class_enum == EE_CALAMITY_CLASS_GROUP_CAST {
        if host.calamity_can_cast_at(class, caster, x, y, target) != 0 {
            return;
        }
        let group = host.active_group_new(player);
        host.group_fill(group, selection, 0, 0);
        host.group_split_calamity_casters(group, class, player, technology, target);
        host.group_ids(group, &mut ids);
        if !ids.is_empty() {
            let goal = cast_goal(host, caster, target, x, y, technology);
            let units = ids.clone();
            for unit in units {
                host.group_remove_unit(group, unit);
                host.unit_clear_goals(unit);
                if let Some(copy) = goal.as_ref().and_then(|g| host.goal_clone(g)) {
                    host.unit_set_goal(unit, copy);
                }
                host.unit_start_action(unit, EE_ACTION_PREPARE_TO_MOVE);
            }
            if let Some(goal) = goal {
                host.goal_release(goal);
            }
            host.group_release(group);
        }
        return;
    }
    let caster_player = host.unit_player(caster);
    let group = host.active_group_new(caster_player);
    if technology != EE_TECH_TELEPORT && !cast_by_all {
        host.group_ids(selection, &mut ids);
        let count = ids.len();
        let mut i = 0usize;
        while i < count {
            host.group_ids(selection, &mut ids);
            let Some(&unit) = ids.get(i) else { break };
            let result = host.calamity_can_cast_at(class, unit, x, y, target);
            let casts = hit_points(host, unit) > 0 && result == 0 && {
                let kind = unit_type(host, unit);
                host.technology_contains(player, technology, kind) && {
                    let power = host.unit_power(unit);
                    power >= host.calamity_cost(class)
                }
            };
            if casts {
                if let Some(goal) = cast_goal(host, unit, target, x, y, technology) {
                    host.unit_set_goal(unit, goal);
                }
                let action = if unit_type(host, unit) == EE_UNIT_TYPE_CAPITOL || technology == EE_TECH_CONQUISTADORS_SPELL
                {
                    EE_ACTION_CALAMITY_PRE_CAST
                } else {
                    EE_ACTION_PREPARE_TO_MOVE
                };
                host.unit_start_action(unit, action);
                host.group_add_member(group, unit);
                break;
            }
            i += 1;
        }
    } else {
        let mut i = 0usize;
        loop {
            host.group_ids(selection, &mut ids);
            let Some(&unit) = ids.get(i) else { break };
            let casts = hit_points(host, unit) > 0 && {
                let kind = unit_type(host, unit);
                host.technology_contains(player, technology, kind)
            } && host.calamity_can_cast_at(class, unit, x, y, target) == 0;
            if casts {
                if let Some(goal) = cast_goal(host, unit, target, x, y, technology) {
                    host.unit_set_goal(unit, goal);
                }
                host.unit_start_action(unit, EE_ACTION_PREPARE_TO_MOVE);
                host.group_add_member(group, unit);
            }
            i += 1;
        }
    }
    host.pending_push(player, group);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::host::tests::{Call, MockHost};
    use crate::kinds::EE_OBJECT_COMPLEX_UNIT;
    use crate::store::EeUnitRecord;
    use std::ffi::c_void;

    fn r(n: usize) -> HostRef {
        HostRef(n as *mut c_void)
    }

    fn complex(id: u32) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            valid: 1,
            hit_points: 10,
            tile_x: (id & 0xff) as i64,
            tile_y: 7,
            goal_kind: -1,
            unit_type: 3,
            ..Default::default()
        }
    }

    fn host_with(selection: &[u32]) -> MockHost {
        let mut host = MockHost::default();
        host.groups.insert(0x10, selection.to_vec());
        for &id in selection {
            host.units.insert(id, complex(id));
        }
        host
    }

    const P: usize = 0x20;
    const S: usize = 0x10;
    const G: usize = 0x1001;

    #[test]
    fn build_unit_makes_the_original_calls_in_order() {
        let (a, b, c) = (0x0100_0001, 0x0100_0002, 0x0100_0003);
        let mut host = host_with(&[a, b, c]);
        host.fills.insert(0, vec![a, b, c]);
        host.citizens = vec![a, b, c];
        host.tech_classes.insert(77, EE_UNIT_TYPE_GRANARY);
        host.produce = true;
        host.place = true;
        host.building = 0x0100_0050;
        host.free = vec![0x0100_0062, 0x0100_0061];
        // b leaves the group at its order: it is added back after the loop
        host.leave_on_order = vec![b];
        host.draws = vec![1, 2];
        build_unit(&mut host, r(P), r(S), 77, 30, 40, false);
        let sorted = vec![0x0100_0061, 0x0100_0062];
        assert_eq!(
            host.calls,
            vec![
                Call::NewGroup(r(P), r(G)),
                Call::Fill(r(G), r(S), 0, 0),
                Call::Pending(r(P), r(G)),
                Call::KeepCitizens(r(G)),
                Call::TechClass(77),
                Call::CanProduce(77),
                Call::CanPlace(EE_UNIT_TYPE_GRANARY, 30, 40),
                Call::MakeBuilding(EE_UNIT_TYPE_GRANARY, 30, 40),
                Call::ComplexOf(0x0100_0050),
                Call::FreeFarms(0x0100_0050),
                Call::SortFarms(vec![0x0100_0062, 0x0100_0061], 1, 7, false),
                Call::OrderBuild(a, 0x0100_0050),
                Call::AddFarm(a, sorted[0]),
                Call::OrderBuild(b, 0x0100_0050),
                Call::AddFarm(b, sorted[1]),
                Call::OrderBuild(c, 0x0100_0050),
                Call::Random(0, 1),
                Call::AddFarm(c, sorted[1]),
                Call::AddMember(r(G), b),
                Call::Random(0, 1),
                Call::Acknowledge(r(P), r(G), 2),
            ]
        );
    }

    #[test]
    fn build_unit_queued_idles_a_unit_with_another_goal() {
        let (a, b) = (0x0100_0001, 0x0100_0002);
        let mut host = host_with(&[a, b]);
        host.units.get_mut(&a).unwrap().goal_kind = 6;
        host.units.get_mut(&b).unwrap().goal_kind = EE_GOAL_KIND_VALUE_BUILD_REPAIR;
        host.fills.insert(0, vec![a, b]);
        host.citizens = vec![a, b];
        host.tech_classes.insert(5, 0x25);
        host.produce = true;
        host.place = true;
        host.building = 0x0100_0050;
        build_unit(&mut host, r(P), r(S), 5, 1, 2, true);
        assert_eq!(
            &host.calls[7..],
            &[
                Call::MakeBuilding(0x25, 1, 2),
                Call::AddBuildTarget(a, 0x0100_0050),
                Call::StartAction(a, EE_ACTION_IDLE),
                Call::AddBuildTarget(b, 0x0100_0050),
                Call::Random(0, 1),
                Call::Acknowledge(r(P), r(G), 0),
            ]
        );
    }

    #[test]
    fn build_unit_refusals() {
        let a = 0x0100_0001;
        // no citizen left: no class read
        let mut host = host_with(&[a]);
        host.fills.insert(0, vec![a]);
        build_unit(&mut host, r(P), r(S), 5, 1, 2, false);
        assert_eq!(host.calls.last(), Some(&Call::KeepCitizens(r(G))));
        // a second capitol, no production, no place
        for (capitols, produce, place, last) in [
            (1, true, true, Call::Capitols),
            (0, false, true, Call::CanProduce(5)),
            (0, true, false, Call::CanPlace(EE_UNIT_TYPE_CAPITOL, 1, 2)),
        ] {
            let mut host = host_with(&[a]);
            host.fills.insert(0, vec![a]);
            host.citizens = vec![a];
            host.tech_classes.insert(5, EE_UNIT_TYPE_CAPITOL);
            host.capitols = capitols;
            host.produce = produce;
            host.place = place;
            build_unit(&mut host, r(P), r(S), 5, 1, 2, false);
            assert_eq!(host.calls.last(), Some(&last));
        }
        // no selection: nothing
        let mut host = MockHost::default();
        build_unit(&mut host, r(P), r(S), 5, 1, 2, false);
        assert!(host.calls.is_empty());
    }

    #[test]
    fn smart_farmers_makes_the_original_calls_in_order() {
        let (a, b, c, d) = (0x0100_0001, 0x0100_0002, 0x0100_0003, 0x0100_0004);
        let target = 0x0100_0040;
        let mut host = host_with(&[a, b, c, d]);
        host.units.insert(target, complex(target));
        host.units.get_mut(&d).unwrap().resource_amount = 5;
        host.fills.insert(0, vec![a, b, c, d]);
        host.citizens = vec![a, b, c, d];
        host.free = vec![0x0100_0061, 0x0100_0062];
        // a farms near the target: it drops its load; b's farm drops elsewhere: it keeps it
        host.load_farms.insert(a, 0x0100_0070);
        host.load_farms.insert(b, 0x0100_0071);
        host.drop_sites.insert(0x0100_0070, vec![0x0100_0090, target]);
        host.drop_sites.insert(0x0100_0071, vec![0x0100_0090]);
        host.takes = vec![a, d];
        host.specs = true;
        smart_farmers(&mut host, r(P), r(S), target);
        assert_eq!(
            host.calls,
            vec![
                Call::NewGroup(r(P), r(G)),
                Call::Fill(r(G), r(S), 0, 0),
                Call::KeepCitizens(r(G)),
                Call::FreeFarms(target),
                Call::LoadFarm(a),
                Call::DropSites(0x0100_0070),
                Call::CanTake(target, a),
                Call::DropGathered(a),
                Call::StartAction(a, EE_ACTION_MOVE_TO_STORAGE),
                Call::LoadFarm(b),
                Call::DropSites(0x0100_0071),
                Call::WorkFarm(0x0100_0071, b),
                Call::LoadFarm(c),
                Call::SortFarms(vec![0x0100_0061, 0x0100_0062], 3, 7, true),
                Call::WorkFarm(0x0100_0061, c),
                Call::LoadFarm(d),
                Call::SortFarms(vec![0x0100_0062], 4, 7, true),
                Call::WorkFarm(0x0100_0062, d),
                Call::Random(0, 3),
                Call::Acknowledge(r(P), r(G), 0),
                Call::Pending(r(P), r(G)),
                Call::Finish(r(G)),
            ]
        );
    }

    #[test]
    fn smart_farmers_without_farms_gathers_or_moves() {
        let (a, b) = (0x0100_0001, 0x0100_0002);
        let target = 0x0100_0040;
        let mut host = host_with(&[a, b]);
        host.units.insert(target, EeUnitRecord { tile_x: 12, tile_y: 34, ..complex(target) });
        host.units.get_mut(&a).unwrap().resource_amount = 5;
        host.units.get_mut(&b).unwrap().resource_amount = 5;
        host.fills.insert(0, vec![a, b]);
        host.citizens = vec![a, b];
        host.takes = vec![a];
        host.specs = true;
        smart_farmers(&mut host, r(P), r(S), target);
        let none = EE_UNIT_ID_NONE;
        assert_eq!(
            &host.calls[4..],
            &[
                Call::LoadFarm(a),
                Call::CanTake(target, a),
                Call::GoalNew(EE_GOAL_CTOR_GATHER, 1),
                Call::Spec(EE_GOAL_CTOR_GATHER, none, target, [0; 3], 1, 0),
                Call::UnitSetGoal(a, 6),
                Call::StartAction(a, EE_ACTION_MOVE_TO_STORAGE),
                Call::LoadFarm(b),
                Call::CanTake(target, b),
                Call::GoalNew(EE_GOAL_CTOR_LOCATION_2D, 0),
                Call::Spec(EE_GOAL_CTOR_LOCATION_2D, none, none, [12f32.to_bits(), 34f32.to_bits(), 0], 0, 0),
                Call::UnitSetGoal(b, 1),
                Call::StartAction(b, EE_ACTION_PREPARE_TO_MOVE),
                Call::Random(0, 1),
                Call::Acknowledge(r(P), r(G), 0),
                Call::Pending(r(P), r(G)),
                Call::Finish(r(G)),
            ]
        );
    }

    #[test]
    fn smart_farmers_needs_a_selection_and_a_complex_target() {
        let a = 0x0100_0001;
        let mut host = host_with(&[a]);
        smart_farmers(&mut host, r(P), r(S), 0x0100_0040);
        assert!(host.calls.is_empty());
        // no citizen: the group still goes to the pending groups and finishes
        let mut host = host_with(&[a]);
        host.units.insert(0x0100_0040, complex(0x0100_0040));
        smart_farmers(&mut host, r(P), r(S), 0x0100_0040);
        assert_eq!(
            &host.calls[2..],
            &[Call::KeepCitizens(r(G)), Call::Pending(r(P), r(G)), Call::Finish(r(G))]
        );
    }

    #[test]
    fn cast_calamity_one_caster() {
        let (caster, a, b) = (0x0100_0009, 0x0100_0001, 0x0100_0002);
        let mut host = host_with(&[a, b]);
        host.units.insert(caster, complex(caster));
        host.units.get_mut(&b).unwrap().unit_type = EE_UNIT_TYPE_CAPITOL;
        host.calamity = Some((0x77, 3, false));
        // a cannot cast there; b can and pays
        host.cast_at.insert(b, 0);
        host.contains = vec![EE_UNIT_TYPE_CAPITOL];
        host.power.insert(b, 50);
        host.cost = 40;
        host.specs = true;
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        let none = EE_UNIT_ID_NONE;
        assert_eq!(
            host.calls,
            vec![
                Call::CalamityOf(900),
                Call::UnitPlayer(caster),
                Call::NewGroup(r(0x9000), r(G)),
                Call::CanCast(a, 5, 6, none),
                Call::CanCast(b, 5, 6, none),
                Call::Contains(EE_UNIT_TYPE_CAPITOL),
                Call::Power(b),
                Call::Cost,
                Call::GoalNew(EE_GOAL_CTOR_CAST_CALAMITY, 1),
                Call::Spec(EE_GOAL_CTOR_CAST_CALAMITY, b, none, [0; 3], 1, 0),
                Call::Ints([5, 6, 900]),
                Call::UnitSetGoal(b, 11),
                Call::StartAction(b, EE_ACTION_CALAMITY_PRE_CAST),
                Call::AddMember(r(G), b),
                Call::Pending(r(P), r(G)),
            ]
        );
    }

    #[test]
    fn cast_calamity_by_all_selected() {
        let (caster, a, b) = (0x0100_0009, 0x0100_0001, 0x0100_0002);
        let mut host = host_with(&[a, b]);
        host.units.insert(caster, complex(caster));
        host.units.get_mut(&a).unwrap().hit_points = 0;
        host.calamity = Some((0x77, 3, true));
        host.cast_at.insert(b, 0);
        host.contains = vec![3];
        cast_calamity(&mut host, r(P), r(S), caster, 0x0100_0001, 5, 6, 900);
        assert_eq!(
            &host.calls[3..],
            &[
                Call::Contains(3),
                Call::CanCast(b, 5, 6, a),
                Call::GoalNew(EE_GOAL_CTOR_CAST_CALAMITY, 1),
                Call::UnitSetGoal(b, 11),
                Call::StartAction(b, EE_ACTION_PREPARE_TO_MOVE),
                Call::AddMember(r(G), b),
                Call::Pending(r(P), r(G)),
            ]
        );
    }

    #[test]
    fn cast_calamity_group_cast() {
        let (caster, a, b) = (0x0100_0009, 0x0100_0001, 0x0100_0002);
        let mut host = host_with(&[a, b]);
        host.units.insert(caster, complex(caster));
        host.calamity = Some((0x77, EE_CALAMITY_CLASS_GROUP_CAST, false));
        host.cast_at.insert(caster, 0);
        host.fills.insert(0, vec![a, b]);
        host.mobile = vec![b];
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert_eq!(
            host.calls,
            vec![
                Call::CalamityOf(900),
                Call::CanCast(caster, 5, 6, EE_UNIT_ID_NONE),
                Call::NewGroup(r(P), r(G)),
                Call::Fill(r(G), r(S), 0, 0),
                Call::Split(r(G), 900, EE_UNIT_ID_NONE),
                Call::GoalNew(EE_GOAL_CTOR_CAST_CALAMITY, 1),
                Call::RemoveUnit(r(G), b),
                Call::ClearGoals(b),
                Call::Clone(11),
                Call::UnitSetGoal(b, 11),
                Call::StartAction(b, EE_ACTION_PREPARE_TO_MOVE),
                Call::Release(r(G)),
            ]
        );
        assert_eq!(host.live, 0);
        // the caster cannot cast there: nothing more
        let mut host = host_with(&[a]);
        host.units.insert(caster, complex(caster));
        host.calamity = Some((0x77, EE_CALAMITY_CLASS_GROUP_CAST, false));
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert_eq!(host.calls.len(), 2);
    }

    #[test]
    fn cast_calamity_refusals() {
        let caster = 0x0100_0009;
        // no caster, a dead caster, no class
        let mut host = host_with(&[]);
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert!(host.calls.is_empty());
        host.units.insert(caster, EeUnitRecord { hit_points: 0, ..complex(caster) });
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert!(host.calls.is_empty());
        host.units.insert(caster, complex(caster));
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert_eq!(host.calls, vec![Call::CalamityOf(900)]);
        // a selected unit out of its list: its hit points read on the object
        let a = 0x0100_0001;
        let mut host = host_with(&[]);
        host.groups.insert(0x10, vec![a]);
        host.units.insert(caster, complex(caster));
        host.calamity = Some((0x77, 3, true));
        cast_calamity(&mut host, r(P), r(S), caster, -1, 5, 6, 900);
        assert_eq!(host.calls[3], Call::ObjectHitPoints(a));
        assert_eq!(host.calls.len(), 5);
    }
}
