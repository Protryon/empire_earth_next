//! The goals' world searches (RUST_PLAN R5b.4, round 22): the helpers the build/repair and gather goals' C++ host
//! kept after round 19 (EEUGRust.cpp), now Rust over finer host calls: the radius searches of the next job
//! (`EEHelpBuildNearby` 0x61eb31, `BuildFarms`' search 0x62333b, `FindUnfinishedFarm` 0x623675) and the footprint tests
//! (`IsBuildSiteTaken` 0x61f4f5, `IsFootprintClear` 0x61f5fc, `IsSiteFreeOfUnits` 0x61f85e). A unit's fields come
//! from the goals' unit record ([`EeUnitsBuildUnit`]); the tile and radius queries, the tile standing test and the
//! goal calls are host operations.

use crate::build::BuildHost;
use crate::rust_goal::EeUnitsBuildUnit;

/// `kActionMoveToRepairSite`
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;
/// `kUnitCategoryGate`
const CATEGORY_GATE: i32 = 0x19;
/// `kUnitTypeTownCenter`, `kUnitTypeSettlement`, `kUnitTypeAirportRunway`
const UNIT_TYPE_TOWN_CENTER: i32 = 0x23;
const UNIT_TYPE_SETTLEMENT: i32 = 0x2b;
const UNIT_TYPE_AIRPORT_RUNWAY: i32 = 0xf0;
/// the build/repair search radius (0x447404)
const BUILD_REPAIR_RADIUS: f32 = 4.0;

/// The world queries of the searches (the FFI adapter; the tests' mock).
pub trait WorldHost: BuildHost {
    /// `EEGetUnitsInRadius(center, radius, units)`: the units, in its order
    fn units_in_radius(&mut self, center: [f32; 2], radius: f32) -> Vec<u64>;
    /// `EEGetFootprintTiles(unit, tiles)`; the units on them (`EEGetUnitsOn`)
    fn footprint_tiles(&mut self, unit: u64) -> Vec<(i32, i32)>;
    fn footprint_units(&mut self, unit: u64) -> Vec<u64>;
    /// `EEGetBlockingUnits` / `EEGetMovableTileUnits(player, type, map tile (x, y), 0, units)`: the units
    fn blocking_units(&mut self, player: u64, unit_type: i32, x: i32, y: i32) -> Vec<u64>;
    fn movable_units(&mut self, player: u64, unit_type: i32, x: i32, y: i32) -> Vec<u64>;
    /// `EEGetTileUnitStanding(unit, player, type, false)`
    fn tile_standing(&mut self, unit: u64, player: u64, unit_type: i32) -> i32;
    /// the unit's class's `ActionChanged(unit)`
    fn action_changed(&mut self, unit: u64);
    /// `EEUGRust::NewBuildRepair(player, target, false)`; a gather goal's `SetGatherStorage(storage)`
    fn new_build_goal(&mut self, player: u64, target: u64) -> u64;
    fn set_gather_storage(&mut self, goal: u64, storage: u64);
}

fn tile_center(r: &EeUnitsBuildUnit) -> [f32; 2] {
    [r.tile_x as f32, r.tile_y as f32]
}

/// 0x61eb31 `EEHelpBuildNearby`: the unit helps build the first unbuilt, paid-for land building of its player
/// within 4 tiles of the target (a build goal, the move to the site)
pub fn help_build_nearby(h: &mut dyn WorldHost, unit: u64, target: u64) {
    let t = h.build_unit(target);
    let units = h.units_in_radius(tile_center(&t), BUILD_REPAIR_RADIUS);
    let player = h.build_unit(unit).player;
    for u in units {
        let r = h.build_unit(u);
        if !r.built && u != target && r.hit_points > 0 && r.land_building && r.paid_for && r.player == player {
            let goal = h.new_build_goal(r.player, u);
            h.set_goal(unit, goal, 0, 0);
            h.start_action(unit, ACTION_MOVE_TO_REPAIR_SITE);
            return;
        }
    }
}

/// 0x62333b `BuildFarms`' search: a build goal (queued, the gather goal's storage) for each unfinished living
/// settlement or town centre of the gatherer's player within its line of sight, the list walked from its end
pub fn build_farms(h: &mut dyn WorldHost, unit: u64, goal: u64) {
    let u = h.build_unit(unit);
    let units = h.units_in_radius(tile_center(&u), u.line_of_sight);
    for &x in units.iter().rev() {
        let r = h.build_unit(x);
        if (r.unit_type != UNIT_TYPE_SETTLEMENT && r.unit_type != UNIT_TYPE_TOWN_CENTER)
            || r.player != u.player
            || r.hit_points <= 0
            || r.built
        {
            continue;
        }
        let build = h.new_build_goal(u.player, x);
        h.set_gather_storage(goal, x);
        h.set_goal(unit, build, 1, 0);
        h.action_changed(unit);
    }
}

/// 0x623675 `FindUnfinishedFarm`: the last unfinished living settlement within twice the gatherer's line of sight
/// (0 for none)
pub fn unfinished_farm(h: &mut dyn WorldHost, unit: u64) -> u64 {
    let u = h.build_unit(unit);
    let units = h.units_in_radius(tile_center(&u), u.line_of_sight * 2.0);
    for &x in units.iter().rev() {
        let r = h.build_unit(x);
        if r.unit_type == UNIT_TYPE_SETTLEMENT && r.hit_points > 0 && !r.built {
            return x;
        }
    }
    0
}

/// a gate's own parts (its first part and every next one); none for another building
fn gate_parts(h: &mut dyn WorldHost, target: u64) -> Vec<u64> {
    let mut parts = Vec::new();
    let t = h.build_unit(target);
    if t.category != CATEGORY_GATE {
        return parts;
    }
    let mut part = t.first;
    while part != 0 {
        parts.push(part);
        part = h.build_unit(part).next;
    }
    parts
}

/// 0x61f4f5 `IsBuildSiteTaken`: units other than the building block a tile of its footprint
pub fn site_taken(h: &mut dyn WorldHost, target: u64, player: u64, unit_type: i32) -> bool {
    for (x, y) in h.footprint_tiles(target) {
        let units = h.blocking_units(player, unit_type, x, y);
        if units.len() > 1 || (units.len() == 1 && units[0] != target) {
            return true;
        }
    }
    false
}

/// 0x61f5fc `IsFootprintClear`: no blocking or movable unit stands on the footprint (the building, a runway under
/// type 0x42 and a gate's own parts excepted); farms (0x2a, 0x33a) always pass. A tile off the map holds no unit.
pub fn footprint_clear(h: &mut dyn WorldHost, target: u64, player: u64, unit_type: i32) -> bool {
    if unit_type == 0x2a || unit_type == 0x33a {
        return true;
    }
    let linked = gate_parts(h, target);
    let mut result = true;
    for (x, y) in h.footprint_tiles(target) {
        let mut units = h.blocking_units(player, unit_type, x, y);
        let alone = units.len() == 1;
        if units.is_empty()
            || (alone && units[0] == target)
            || (alone && unit_type == 0x42 && h.build_unit(units[0]).unit_type == UNIT_TYPE_AIRPORT_RUNWAY)
        {
            units = h.movable_units(player, unit_type, x, y);
            result = units.is_empty();
        } else {
            result = false;
        }
        if !result && !linked.is_empty() {
            result = units.iter().any(|u| linked.contains(u));
        }
        if !result {
            break;
        }
    }
    result
}

/// 0x61f85e `IsSiteFreeOfUnits`: no unit on the footprint stands in the way (standing 2) and no foundation above 1
/// hit point is there (the building, the player's runway under type 0x42 and a gate's own parts excepted)
pub fn site_free(h: &mut dyn WorldHost, target: u64, player: u64, unit_type: i32) -> bool {
    let units = h.footprint_units(target);
    let linked = gate_parts(h, target);
    let mut result = true;
    for u in units {
        if u == target {
            continue;
        }
        let r = h.build_unit(u);
        if (r.unit_type == UNIT_TYPE_AIRPORT_RUNWAY && unit_type == 0x42 && r.player == player) || linked.contains(&u) {
            continue;
        }
        result = h.tile_standing(u, player, unit_type) != 2;
        if r.foundation && r.hit_points > 1 {
            result = false;
        }
        if !result {
            break;
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::build::tests::{World, P};

    impl WorldHost for World {
        fn units_in_radius(&mut self, center: [f32; 2], radius: f32) -> Vec<u64> {
            self.log.push(format!("radius {center:?} {radius}"));
            self.radius_units.clone()
        }
        fn footprint_tiles(&mut self, _unit: u64) -> Vec<(i32, i32)> {
            self.footprint.clone()
        }
        fn footprint_units(&mut self, _unit: u64) -> Vec<u64> {
            self.footprint_units.clone()
        }
        fn blocking_units(&mut self, _p: u64, _t: i32, x: i32, y: i32) -> Vec<u64> {
            self.log.push(format!("blocking {x},{y}"));
            self.blocking.get(&(x, y)).cloned().unwrap_or_default()
        }
        fn movable_units(&mut self, _p: u64, _t: i32, x: i32, y: i32) -> Vec<u64> {
            self.log.push(format!("movable {x},{y}"));
            self.movable.get(&(x, y)).cloned().unwrap_or_default()
        }
        fn tile_standing(&mut self, unit: u64, _p: u64, _t: i32) -> i32 {
            self.standing.get(&unit).copied().unwrap_or(0)
        }
        fn action_changed(&mut self, unit: u64) {
            self.log.push(format!("changed {unit:x}"));
        }
        fn new_build_goal(&mut self, player: u64, target: u64) -> u64 {
            self.log.push(format!("build {player:x} {target:x}"));
            0x8000 + target
        }
        fn set_gather_storage(&mut self, goal: u64, storage: u64) {
            self.log.push(format!("storage {goal:x} {storage:x}"));
        }
    }

    const U: u64 = 0x1000;
    const Q: u64 = 0x51;

    fn unit(w: &mut World, u: u64, player: u64, unit_type: i32, hp: i32) -> &mut EeUnitsBuildUnit {
        let r = w.unit(u);
        r.player = player;
        r.unit_type = unit_type;
        r.hit_points = hp;
        r
    }

    #[test]
    fn help_build_takes_the_first_fit_in_list_order() {
        let mut w = World::default();
        unit(&mut w, U, P, 1, 10);
        let t = unit(&mut w, 0x2000, P, 2, 10);
        (t.tile_x, t.tile_y) = (7, 9);
        // built; another player's; not paid for; no land building; dead; the fit; a second fit
        for (i, (built, player, paid, land, hp)) in [
            (true, P, true, true, 5),
            (false, Q, true, true, 5),
            (false, P, false, true, 5),
            (false, P, true, false, 5),
            (false, P, true, true, 0),
            (false, P, true, true, 5),
            (false, P, true, true, 5),
        ]
        .into_iter()
        .enumerate()
        {
            let r = unit(&mut w, 0x3000 + i as u64, player, 3, hp);
            (r.built, r.paid_for, r.land_building) = (built, paid, land);
            w.radius_units.push(0x3000 + i as u64);
        }
        w.radius_units.insert(0, 0x2000); // the target itself is passed over
        help_build_nearby(&mut w, U, 0x2000);
        assert_eq!(w.log, vec!["radius [7.0, 9.0] 4", "build 50 3005", "set goal 1000 b005 0 0", "start 1000 8017"]);
        // nothing fits: no goal
        let mut w2 = World::default();
        unit(&mut w2, U, P, 1, 10);
        unit(&mut w2, 0x2000, P, 2, 10);
        help_build_nearby(&mut w2, U, 0x2000);
        assert_eq!(w2.log.len(), 1);
    }

    #[test]
    fn build_farms_and_unfinished_farm_walk_from_the_end() {
        let mut w = World::default();
        let u = unit(&mut w, U, P, 1, 10);
        (u.tile_x, u.tile_y, u.line_of_sight) = (3, 4, 6.5);
        unit(&mut w, 0x10, P, UNIT_TYPE_SETTLEMENT, 5);
        unit(&mut w, 0x11, P, UNIT_TYPE_TOWN_CENTER, 5);
        unit(&mut w, 0x12, P, UNIT_TYPE_SETTLEMENT, 5).built = true;
        unit(&mut w, 0x13, Q, UNIT_TYPE_SETTLEMENT, 5);
        unit(&mut w, 0x14, P, UNIT_TYPE_SETTLEMENT, 0);
        unit(&mut w, 0x15, P, 7, 5);
        w.radius_units = vec![0x10, 0x11, 0x12, 0x13, 0x14, 0x15];
        build_farms(&mut w, U, 0x9000);
        assert_eq!(
            w.log,
            vec![
                "radius [3.0, 4.0] 6.5",
                "build 50 11",
                "storage 9000 11",
                "set goal 1000 8011 1 0",
                "changed 1000",
                "build 50 10",
                "storage 9000 10",
                "set goal 1000 8010 1 0",
                "changed 1000",
            ]
        );
        w.log.clear();
        // the last unfinished living settlement of any player within twice the sight
        assert_eq!(unfinished_farm(&mut w, U), 0x13);
        assert_eq!(w.log, vec!["radius [3.0, 4.0] 13"]);
        w.radius_units = vec![0x11, 0x12, 0x14];
        assert_eq!(unfinished_farm(&mut w, U), 0);
    }

    #[test]
    fn footprint_tests() {
        let b = 0x2000;
        let mut w = World::default();
        unit(&mut w, b, P, 5, 10);
        w.footprint = vec![(1, 1), (1, 2), (2, 1)];
        // free tiles: not taken, clear
        assert!(!site_taken(&mut w, b, P, 5));
        assert!(footprint_clear(&mut w, b, P, 5));
        // the building alone on a tile; a movable unit on another
        w.blocking.insert((1, 1), vec![b]);
        w.movable.insert((1, 2), vec![0x30]);
        assert!(!site_taken(&mut w, b, P, 5));
        w.log.clear();
        assert!(!footprint_clear(&mut w, b, P, 5));
        assert_eq!(w.log, vec!["blocking 1,1", "movable 1,1", "blocking 1,2", "movable 1,2"]);
        // farms always pass
        assert!(footprint_clear(&mut w, b, P, 0x2a));
        assert!(footprint_clear(&mut w, b, P, 0x33a));
        // another unit blocking: taken, not clear; a runway under type 0x42 counts as free
        w.movable.clear();
        w.blocking.insert((2, 1), vec![0x40]);
        assert!(site_taken(&mut w, b, P, 5));
        assert!(!footprint_clear(&mut w, b, P, 5));
        unit(&mut w, 0x40, P, UNIT_TYPE_AIRPORT_RUNWAY, 10);
        assert!(!footprint_clear(&mut w, b, P, 5));
        assert!(footprint_clear(&mut w, b, P, 0x42));
        // a gate's own parts do not block it
        let g = w.unit(b);
        (g.category, g.first) = (CATEGORY_GATE, 0x50);
        w.unit(0x50).next = 0x40;
        assert!(footprint_clear(&mut w, b, P, 5));

        // site free: the building, the player's runway under 0x42 and the gate's parts excepted
        let mut w = World::default();
        unit(&mut w, b, P, 5, 10);
        unit(&mut w, 0x40, P, UNIT_TYPE_AIRPORT_RUNWAY, 10);
        unit(&mut w, 0x41, P, 3, 10);
        w.footprint_units = vec![b, 0x40, 0x41];
        w.standing.insert(0x40, 2);
        assert!(!site_free(&mut w, b, P, 5));
        assert!(site_free(&mut w, b, P, 0x42));
        assert!(!site_free(&mut w, b, Q, 0x42));
        // a foundation above 1 hit point is in the way even when it stands aside
        w.footprint_units = vec![0x41];
        assert!(site_free(&mut w, b, P, 5));
        w.unit(0x41).foundation = true;
        assert!(!site_free(&mut w, b, P, 5));
        w.unit(0x41).hit_points = 1;
        assert!(site_free(&mut w, b, P, 5));
    }
}
