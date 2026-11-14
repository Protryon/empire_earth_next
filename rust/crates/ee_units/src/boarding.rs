//! The load orders (RUST_PLAN R5b.4, the game messages lane, round 2): `EEMLoadGarrison::Execute` and
//! `EEMLoadTransport::Execute` over the store and host calls (`src/Empire Earth/Units/EEUnitHost.cpp`). The C++
//! `Execute` hands the sender, its selection and the message's ids; the bodies read the units' plain state from the
//! store (a selected unit already out of its list on the object), pick the units that board, join the carrier's load
//! goal or start one (the load goals are Rust since the core lane's round 18: `load`), give each boarding unit a
//! clone and send the others, by formation slot, to the carrier's tile (the transport's: the pickup tile).
//!
//! Determinism: the goals are created and cloned in the original's order (each `Clone` makes a goal id); a new load
//! goal takes its units sorted by id with no id twice, and the boarding units take their clones in that order
//! (`EEUGRust::NewLoad` sorted the caller's vector in place).

use crate::group::{in_formation_slot, world_unit};
use crate::host::{
    EeGoalSpec, Host, HostRef, EE_GOAL_CTOR_LOCATION_2D, EE_UNITS_LOAD_CAN_GARRISON, EE_UNITS_LOAD_MOVES,
    EE_UNITS_LOAD_TRANSPORT, EE_UNIT_ID_NONE,
};
use crate::rally::selected;

/// `kGoalLoadTransport`, `kGoalLoadGarrison` (ee/EEUnitGoal.h)
pub const GOAL_LOAD_TRANSPORT: u8 = 7;
pub const GOAL_LOAD_GARRISON: u8 = 12;
/// `kActionPrepareToMove`, `kActionPickedUpLoadableUnits` (ee/EEUnitAction.h)
const ACTION_PREPARE_TO_MOVE: i32 = 8000;
const ACTION_PICKED_UP_LOADABLE_UNITS: i32 = 12003;
/// the event "cannot garrison there"
const EVENT_GARRISON_INVALID: i32 = 0x7c3a;

/// The sender's selection as ids; None when there is none or its first unit is none (`GetFirstUnit() == NULL`).
fn selection_ids(host: &mut impl Host, selection: HostRef) -> Option<Vec<u32>> {
    if selection.0.is_null() {
        return None;
    }
    let mut ids = Vec::new();
    host.group_ids(selection, &mut ids);
    match ids.first() {
        Some(&first) if first != EE_UNIT_ID_NONE => Some(ids),
        _ => None,
    }
}

fn alive(host: &mut impl Host, unit: u32) -> bool {
    selected(host, unit).is_some_and(|u| u.hit_points > 0)
}

/// `EEUGRust::NewLoad`'s order of the units: sorted by id, no id twice
fn sorted(units: &mut Vec<u32>) {
    units.sort_unstable();
    units.dedup();
}

/// Each boarding unit takes a clone of the load goal and prepares to move (`SetGoal(goal->Clone(), 0, 0)`,
/// `StartAction(kActionPrepareToMove)`, `ActionChanged`).
fn board(host: &mut impl Host, goal: HostRef, units: &[u32]) {
    for &unit in units {
        if let Some(clone) = host.load_goal_clone(goal) {
            host.unit_set_goal(unit, clone);
        }
        host.unit_start_action(unit, ACTION_PREPARE_TO_MOVE);
    }
}

/// The units that do not board: for each formation slot 1..5 a new active group of those in the slot keeps its
/// mobile units and, when any is left, moves to the tile (`new EEUGLocation(GE2DPoint(x, y), true, true, false, 0,
/// false, 0)`, unqueued), prepares to move, goes to the player's pending groups and plans; else it is released.
fn send_rejected(host: &mut impl Host, player: HostRef, rejected: &[u32], x: f32, y: f32) {
    if rejected.is_empty() {
        return;
    }
    let mut ids = Vec::new();
    for slot in 1..6 {
        let group = host.active_group_new(player);
        for &unit in rejected {
            if selected(host, unit).is_some_and(|u| in_formation_slot(&u, slot)) {
                host.group_add_unit(group, unit);
            }
        }
        host.group_keep_mobile(group);
        host.group_ids(group, &mut ids);
        if ids.is_empty() {
            host.group_release(group);
            continue;
        }
        let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_LOCATION_2D);
        spec.point = [x, y, 0.0];
        if let Some(goal) = host.goal_new(&spec) {
            host.group_set_goal(group, goal, false);
        }
        host.group_set_action(group, ACTION_PREPARE_TO_MOVE);
        host.pending_push(player, group);
        host.group_plan(group);
    }
}

/// `EEMLoadGarrison::Execute`: the garrison (a unit on the map with hit points, whose complex unit's class can hold
/// units) takes the sender's living selected units other than itself that can garrison: they join its load goal
/// when it has one, else it starts one (and, when it is valid, takes it: it prepares to move or, when it cannot
/// move, has picked up its units). When the goal is valid each takes its clone and prepares to move; else the
/// sender hears "cannot garrison there" at the garrison's tile. The units that cannot garrison move to its tile.
pub fn load_garrison(host: &mut impl Host, player: HostRef, selection: HostRef, garrison_id: u32) {
    let Some(target) = world_unit(host, garrison_id) else { return };
    if !host.unit_has_tile_node(target.id) || target.hit_points <= 0 {
        return;
    }
    let garrison = host.unit_complex_id(target.id);
    if garrison == EE_UNIT_ID_NONE {
        return;
    }
    let old = host.unit_load_goal(garrison, GOAL_LOAD_GARRISON);
    let Some(ids) = selection_ids(host, selection) else { return };
    if host.unit_garrison_capacity(garrison) <= 0 {
        return;
    }
    let (mut units, mut rejected) = (Vec::new(), Vec::new());
    for &unit in &ids {
        if unit == garrison || unit == EE_UNIT_ID_NONE || !alive(host, unit) {
            continue;
        }
        if host.unit_load_traits(unit) & EE_UNITS_LOAD_CAN_GARRISON != 0 {
            units.push(unit);
        } else {
            rejected.push(unit);
        }
    }
    if !units.is_empty() {
        let goal = if !old.0.is_null() {
            host.load_goal_add_units(old, &units);
            old
        } else {
            sorted(&mut units);
            let goal = host.load_goal_new(GOAL_LOAD_GARRISON, garrison, &units, false);
            if host.load_goal_fields(goal).0 {
                host.unit_take_load_goal(garrison, goal);
                let moves = host.unit_load_traits(garrison) & EE_UNITS_LOAD_MOVES != 0;
                host.unit_start_action(
                    garrison,
                    if moves { ACTION_PREPARE_TO_MOVE } else { ACTION_PICKED_UP_LOADABLE_UNITS },
                );
            }
            goal
        };
        if host.load_goal_fields(goal).0 {
            board(host, goal, &units);
        } else {
            host.player_event(player, target.tile_x, target.tile_y, EVENT_GARRISON_INVALID);
        }
    }
    if !rejected.is_empty() {
        let (x, y) = selected(host, garrison).map_or((0, 0), |g| (g.tile_x, g.tile_y));
        send_rejected(host, player, &rejected, x as f32, y as f32);
    }
}

/// `EEMLoadTransport::Execute`: the transport (a unit on the map with hit points whose complex unit is a transport)
/// takes the sender's living selected units other than itself (or, with a passenger id other than -1, that living
/// unit's complex unit) that it can carry (`FilterLoadable`): they join its load goal when it has one, else it starts
/// one, takes it and prepares to move (or picks up its units when it cannot move); each takes the goal's clone and
/// prepares to move. The others move to the goal's pickup tile (none boarding: the transport's tile); the sender
/// hears the transport's acknowledgement.
pub fn load_transport(host: &mut impl Host, player: HostRef, selection: HostRef, transport_id: u32, passenger_id: i32) {
    let Some(target) = world_unit(host, transport_id) else { return };
    if !host.unit_has_tile_node(target.id) || target.hit_points <= 0 {
        return;
    }
    let transport = host.unit_complex_id(target.id);
    if transport == EE_UNIT_ID_NONE {
        return;
    }
    let old = host.unit_load_goal(transport, GOAL_LOAD_TRANSPORT);
    let Some(ids) = selection_ids(host, selection) else { return };
    if host.unit_load_traits(transport) & EE_UNITS_LOAD_TRANSPORT == 0 {
        return;
    }
    let mut units = Vec::new();
    if passenger_id == -1 {
        for &unit in &ids {
            if unit != transport && unit != EE_UNIT_ID_NONE && alive(host, unit) {
                units.push(unit);
            }
        }
    } else if let Some(passenger) = world_unit(host, passenger_id as u32) {
        // the passenger may be gone by the time the order runs
        let unit = host.unit_complex_id(passenger.id);
        if unit != EE_UNIT_ID_NONE && alive(host, unit) {
            units.push(unit);
        }
    }
    let mut rejected = Vec::new();
    host.transport_filter(transport, &mut units, &mut rejected);
    let (x, y) = if !units.is_empty() {
        let goal = if !old.0.is_null() {
            host.load_goal_add_units(old, &units);
            old
        } else {
            sorted(&mut units);
            let goal = host.load_goal_new(GOAL_LOAD_TRANSPORT, transport, &units, true);
            host.unit_take_load_goal(transport, goal);
            let moves = host.unit_load_traits(transport) & EE_UNITS_LOAD_MOVES != 0;
            host.unit_start_action(
                transport,
                if moves { ACTION_PREPARE_TO_MOVE } else { ACTION_PICKED_UP_LOADABLE_UNITS },
            );
            goal
        };
        let (_, x, y) = host.load_goal_fields(goal);
        board(host, goal, &units);
        (x, y)
    } else {
        selected(host, transport).map_or((0, 0), |t| (t.tile_x, t.tile_y))
    };
    send_rejected(host, player, &rejected, x as f32, y as f32);
    host.acknowledge_unit(player, transport);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::host::tests::{Call, MockHost};
    use crate::kinds::EE_OBJECT_COMPLEX_UNIT;
    use crate::store::EeUnitRecord;
    use std::ffi::c_void;

    const P: usize = 0x500;
    const S: usize = 0x10;
    const CARRIER: u32 = 0x0100_0001;
    const A: u32 = 0x0100_0005;
    const B: u32 = 0x0100_0003;
    const C: u32 = 0x0100_0004;

    fn r(a: usize) -> HostRef {
        HostRef(a as *mut c_void)
    }

    fn unit(id: u32, domains: u8) -> EeUnitRecord {
        EeUnitRecord {
            id,
            owner: id >> 24,
            player: P as u64,
            object_kind: EE_OBJECT_COMPLEX_UNIT,
            hit_points: 100,
            tile_x: 10,
            tile_y: 20,
            domains,
            goal_kind: -1,
            ..Default::default()
        }
    }

    /// a carrier, two land units (A, B) and an air unit (C), all selected after the carrier
    fn host(carrier_traits: u8) -> MockHost {
        let mut h = MockHost::default();
        h.units.insert(CARRIER, unit(CARRIER, 2));
        for (id, d) in [(A, 2), (B, 2), (C, 1)] {
            h.units.insert(id, unit(id, d));
        }
        h.groups.insert(S, vec![CARRIER, A, B, C]);
        h.load_traits.insert(CARRIER, carrier_traits);
        h.garrison_capacity.insert(CARRIER, 10);
        h.new_load = (true, 3, 4);
        h
    }

    fn started(h: &MockHost) -> Vec<(u32, i32)> {
        h.calls
            .iter()
            .filter_map(|c| match c {
                Call::StartAction(u, a) => Some((*u, *a)),
                _ => None,
            })
            .collect()
    }

    #[test]
    fn garrison_starts_a_sorted_load_and_sends_the_others_by_slot() {
        let mut h = host(EE_UNITS_LOAD_MOVES);
        h.load_traits.insert(A, EE_UNITS_LOAD_CAN_GARRISON);
        h.load_traits.insert(B, EE_UNITS_LOAD_CAN_GARRISON);
        h.mobile = vec![C];
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert!(h.calls.contains(&Call::NewLoad(GOAL_LOAD_GARRISON, CARRIER, vec![B, A], false)));
        assert!(h.calls.contains(&Call::TakeLoadGoal(CARRIER, 0x9001)));
        assert_eq!(
            started(&h),
            vec![(CARRIER, ACTION_PREPARE_TO_MOVE), (B, ACTION_PREPARE_TO_MOVE), (A, ACTION_PREPARE_TO_MOVE)]
        );
        // C (air) goes in slot 1's group, which moves; slots 2-5 have none and are released
        assert!(h.calls.contains(&Call::AddUnit(r(0x1001), C)));
        assert!(h.calls.contains(&Call::Plan(r(0x1001))));
        assert_eq!(h.calls.iter().filter(|c| matches!(c, Call::Release(_))).count(), 4);
        assert_eq!(h.live, 0);
    }

    #[test]
    fn garrison_joins_its_goal_or_reports_an_invalid_one() {
        let mut h = host(0);
        h.load_traits.insert(A, EE_UNITS_LOAD_CAN_GARRISON);
        h.load_traits.insert(C, EE_UNITS_LOAD_CAN_GARRISON);
        h.load_goals.insert(CARRIER, 0x7000);
        h.load_fields.insert(0x7000, (true, 0, 0));
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert!(h.calls.contains(&Call::AddLoadUnits(0x7000, vec![A, C])));
        assert_eq!(started(&h), vec![(A, ACTION_PREPARE_TO_MOVE), (C, ACTION_PREPARE_TO_MOVE)]);
        // a new invalid goal: not taken, the event at the garrison unit's tile, the units do not board
        let mut h = host(0);
        h.load_traits.insert(A, EE_UNITS_LOAD_CAN_GARRISON);
        h.new_load = (false, 0, 0);
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert!(!h.calls.iter().any(|c| matches!(c, Call::TakeLoadGoal(..))));
        assert!(h.calls.contains(&Call::Event(r(P), 10, 20, EVENT_GARRISON_INVALID)));
        assert!(started(&h).is_empty());
        // a stationary garrison has picked up its units
        let mut h = host(0);
        h.load_traits.insert(A, EE_UNITS_LOAD_CAN_GARRISON);
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert_eq!(started(&h)[0], (CARRIER, ACTION_PICKED_UP_LOADABLE_UNITS));
    }

    #[test]
    fn garrison_needs_a_capacity_a_tile_and_a_selection() {
        let mut h = host(0);
        h.garrison_capacity.insert(CARRIER, 0);
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert_eq!(
            h.calls,
            vec![
                Call::TileNode(CARRIER),
                Call::ComplexOf(CARRIER),
                Call::LoadGoal(CARRIER, 12),
                Call::GarrisonCapacity(CARRIER)
            ]
        );
        let mut h = host(0);
        h.off_tiles.push(CARRIER);
        load_garrison(&mut h, r(P), r(S), CARRIER);
        assert_eq!(h.calls, vec![Call::TileNode(CARRIER)]);
        let mut h = host(0);
        load_garrison(&mut h, r(P), r(0), CARRIER);
        load_garrison(&mut h, r(P), r(S), 0x0300_0001);
        assert!(!h.calls.iter().any(|c| matches!(c, Call::GarrisonCapacity(_))));
    }

    #[test]
    fn transport_boards_what_it_can_carry_and_sends_the_rest_to_the_pickup_tile() {
        let mut h = host(EE_UNITS_LOAD_TRANSPORT | EE_UNITS_LOAD_MOVES);
        h.unloadable = vec![C];
        h.mobile = vec![C];
        load_transport(&mut h, r(P), r(S), CARRIER, -1);
        assert!(h.calls.contains(&Call::Filter(CARRIER, vec![A, B, C])));
        assert!(h.calls.contains(&Call::NewLoad(GOAL_LOAD_TRANSPORT, CARRIER, vec![B, A], true)));
        assert_eq!(
            started(&h),
            vec![(CARRIER, ACTION_PREPARE_TO_MOVE), (B, ACTION_PREPARE_TO_MOVE), (A, ACTION_PREPARE_TO_MOVE)]
        );
        // the move goes to the pickup tile (3, 4)
        h.specs = true;
        h.calls.clear();
        load_transport(&mut h, r(P), r(S), CARRIER, -1);
        let bits = [3.0f32.to_bits(), 4.0f32.to_bits(), 0];
        assert!(h.calls.contains(&Call::Spec(EE_GOAL_CTOR_LOCATION_2D, EE_UNIT_ID_NONE, EE_UNIT_ID_NONE, bits, 0, 0)));
        assert_eq!(h.calls.last(), Some(&Call::AcknowledgeUnit(r(P), CARRIER)));
        assert_eq!(h.live, 0);
    }

    #[test]
    fn transport_with_a_passenger_or_nothing_to_board() {
        let mut h = host(EE_UNITS_LOAD_TRANSPORT);
        h.load_goals.insert(CARRIER, 0x7000);
        h.load_fields.insert(0x7000, (false, 5, 6));
        load_transport(&mut h, r(P), r(S), CARRIER, B as i32);
        assert!(h.calls.contains(&Call::Filter(CARRIER, vec![B])));
        assert!(h.calls.contains(&Call::AddLoadUnits(0x7000, vec![B])));
        assert_eq!(started(&h), vec![(B, ACTION_PREPARE_TO_MOVE)]);
        // a passenger no unit is: nothing boards, the acknowledgement still comes
        h.calls.clear();
        load_transport(&mut h, r(P), r(S), CARRIER, 0x0100_0077);
        assert!(h.calls.contains(&Call::Filter(CARRIER, vec![])));
        assert!(started(&h).is_empty());
        assert_eq!(h.calls.last(), Some(&Call::AcknowledgeUnit(r(P), CARRIER)));
        // not a transport
        let mut h = host(0);
        load_transport(&mut h, r(P), r(S), CARRIER, -1);
        assert!(!h.calls.iter().any(|c| matches!(c, Call::Filter(..) | Call::AcknowledgeUnit(..))));
    }
}
