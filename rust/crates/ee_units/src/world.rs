//! The goals' world searches (RUST_PLAN R5b.4, round 22): the helpers the build/repair and gather goals' C++ host
//! kept after round 19 (EEUGRust.cpp), now Rust over finer host calls: the radius searches of the next job
//! (`EEHelpBuildNearby` 0x61eb31, `BuildFarms`' search 0x62333b, `FindUnfinishedFarm` 0x623675) and the footprint tests
//! (`IsBuildSiteTaken` 0x61f4f5, `IsFootprintClear` 0x61f5fc, `IsSiteFreeOfUnits` 0x61f85e), and the gathering
//! (`BeginGathering` 0x622ae2, `Work` 0x62410b, `SearchNavalResource`'s world part 0x624250). A unit's fields come
//! from the goals' unit record ([`EeUnitsBuildUnit`]); the tile and radius queries, the tile standing test, the
//! gathering's effects and the goal calls are host operations.

use crate::build::BuildHost;
use crate::gather::GatherHost;
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

// ---- the gathering (round 22): `BeginGathering` (0x622ae2), `Work` (0x62410b), `SearchNavalResource`'s world part
// (0x624250), their effects host operations

/// `kUnitCategoryLand`, `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const CATEGORY_LAND: i32 = 5;
const CATEGORY_LAND_CITIZEN: i32 = 6;
const CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
/// `kUnitTypeFarm`, `kUnitTypeResFish`
const UNIT_TYPE_FARM: i32 = 0x2a;
const UNIT_TYPE_RES_FISH: i32 = 0x13b;
/// `kResourceFood`, `kResourceWood`, `kResourceStone`, `kResourceGold`, `kResourceIron`
const RESOURCE_FOOD: i32 = 2;
const RESOURCE_WOOD: i32 = 3;
const RESOURCE_STONE: i32 = 4;
const RESOURCE_GOLD: i32 = 5;
const RESOURCE_IRON: i32 = 6;

/// The gathering's world calls (the FFI adapter; the tests' mock).
pub trait GatheringHost: WorldHost + GatherHost {
    /// the unit's `mAnimation`
    fn animation(&mut self, unit: u64) -> i64;
    /// the resource's `TakeHit(unit, UMFloatToLong(unit's attack), 0, 1)`
    fn take_hit(&mut self, resource: u64, unit: u64);
    /// one unit taken: the resource's `DepleteResource(1)`, the unit's `AddResourceAmount(1)`, `SetLastResource`
    fn take_one(&mut self, resource: u64, unit: u64);
    /// the tree's `RegrowForest(unit)`
    fn regrow(&mut self, resource: u64, unit: u64);
    /// the work timer of the resource's gathering (the stats' gather time, variant-scaled but for stone)
    fn work_timer(&mut self, unit: u64, resource: u64);
    /// the unit's player's epoch has high-tech tools; its `ReleaseFireEffect()`; the mining beam to the resource
    /// (the unit's fire effect)
    fn high_tech_tools(&mut self, unit: u64) -> bool;
    fn release_fire(&mut self, unit: u64);
    fn mining_beam(&mut self, unit: u64, resource: u64);
    /// the unit's `SetResourceType(type)`
    fn set_resource_type(&mut self, unit: u64, resource_type: i32);
    /// the naval citizen class's `FindResourceOfKind(unit, fish, fish, its sight times 5, the fish's type, ...)`
    fn find_fish(&mut self, unit: u64, fish: u64) -> u64;
    /// `gPhysics->mRandom.Generate()`, the unit's stats' `mCanFireOverWalls`
    fn random_draw(&mut self) -> f32;
    fn can_fire_over_walls(&mut self, unit: u64) -> bool;
}

fn on_land(category: i32) -> bool {
    category == CATEGORY_LAND_CITIZEN || category == CATEGORY_LAND
}

/// 0x622ae2 `BeginGathering`: the animation for the resource (hunt, farm, chop, mine; with high-tech tools the fire
/// effect released, a mining beam for ore), the resource type, facing it, the work timer, the gatherer stopped
pub fn begin_gathering(h: &mut dyn GatheringHost, unit: u64, resource: u64) {
    let u = h.build_unit(unit);
    let r = h.build_unit(resource);
    if u.category == CATEGORY_NAVAL_CITIZEN {
        h.set_animation(unit, 0xb);
    } else if on_land(r.category) {
        h.set_animation(unit, if r.hit_points > 0 { 0xb } else { 0xa });
    } else {
        match r.resource_type {
            RESOURCE_FOOD => h.set_animation(unit, if r.unit_type == UNIT_TYPE_FARM { 6 } else { 0xa }),
            RESOURCE_WOOD => {
                h.set_animation(unit, 3);
                if h.high_tech_tools(unit) {
                    h.release_fire(unit);
                }
            }
            RESOURCE_STONE | RESOURCE_GOLD | RESOURCE_IRON => {
                h.set_animation(unit, 9);
                if h.high_tech_tools(unit) {
                    h.release_fire(unit);
                    h.mining_beam(unit, resource);
                }
            }
            _ => {}
        }
    }
    h.set_resource_type(unit, r.resource_type);
    if u.category != CATEGORY_NAVAL_CITIZEN {
        h.face_target(unit, resource);
    }
    h.work_timer(unit, resource);
    h.drop_path(unit);
    h.reset_active_path(unit);
    h.halt(unit, 6);
}

/// 0x62410b `Work`: one gathering tick: a living resource but a farm is hit with the gatherer's attack, else one unit
/// of it is taken; the animation follows; a tree regrows; the work timer restarts
pub fn gather_work(h: &mut dyn GatheringHost, unit: u64, resource: u64) {
    let state = h.animation(unit);
    let r = h.build_unit(resource);
    if r.hit_points != 0 && r.unit_type != UNIT_TYPE_FARM {
        h.take_hit(resource, unit);
        let r = h.build_unit(resource);
        if on_land(r.category) {
            let state_new = i64::from(r.hit_points > 0) + 10;
            if state_new != state {
                h.set_animation(unit, state_new);
            }
        }
    } else {
        if on_land(r.category) && state != 10 {
            h.set_animation(unit, 10);
        }
        h.take_one(resource, unit);
    }
    if h.build_unit(resource).is_tree {
        h.regrow(resource, unit);
    }
    h.work_timer(unit, resource);
}

/// 0x624250 `SearchNavalResource`'s world part: 0 idle (no resource, or a computer player's farm), 1 another fish
/// (the result), 2 nothing, 3 waits facing the resource (animation 7 or 8 by the physics draw)
pub fn naval_search(h: &mut dyn GatheringHost, unit: u64, resource: u64) -> (i64, u64) {
    if resource == 0 {
        return (0, 0);
    }
    let r = h.build_unit(resource);
    if h.build_unit(unit).computer && r.unit_type == UNIT_TYPE_FARM {
        return (0, 0);
    }
    if r.unit_type == UNIT_TYPE_RES_FISH {
        let found = h.find_fish(unit, resource);
        if found == 0 || found == resource {
            return (2, 0);
        }
        return (1, found);
    }
    h.set_physics_action(unit, 4);
    h.face_target(unit, resource);
    let animation = if h.random_draw() < 0.5 || h.can_fire_over_walls(unit) { 7 } else { 8 };
    h.set_animation(unit, animation);
    (3, 0)
}

#[cfg(test)]
mod gathering_tests {
    use super::*;
    use crate::build::tests::{World, P};

    impl GatheringHost for World {
        fn animation(&mut self, unit: u64) -> i64 {
            i64::from(self.unit(unit).animation)
        }
        fn take_hit(&mut self, resource: u64, unit: u64) {
            self.log.push(format!("hit {resource:x} {unit:x}"));
            let r = self.unit(resource);
            r.hit_points = (r.hit_points - 4).max(0);
        }
        fn take_one(&mut self, resource: u64, unit: u64) {
            self.log.push(format!("take {resource:x} {unit:x}"));
        }
        fn regrow(&mut self, resource: u64, _unit: u64) {
            self.log.push(format!("regrow {resource:x}"));
        }
        fn work_timer(&mut self, unit: u64, resource: u64) {
            self.log.push(format!("timer {unit:x} {resource:x}"));
        }
        fn high_tech_tools(&mut self, _unit: u64) -> bool {
            self.knobs.get("tools").copied().unwrap_or(0) != 0
        }
        fn release_fire(&mut self, unit: u64) {
            self.log.push(format!("release {unit:x}"));
        }
        fn mining_beam(&mut self, unit: u64, resource: u64) {
            self.log.push(format!("beam {unit:x} {resource:x}"));
        }
        fn set_resource_type(&mut self, unit: u64, resource_type: i32) {
            self.log.push(format!("type {unit:x} {resource_type}"));
        }
        fn find_fish(&mut self, _unit: u64, _fish: u64) -> u64 {
            self.knobs.get("fish").copied().unwrap_or(0) as u64
        }
        fn random_draw(&mut self) -> f32 {
            self.knobs.get("draw").copied().unwrap_or(0) as f32 / 100.0
        }
        fn can_fire_over_walls(&mut self, _unit: u64) -> bool {
            self.knobs.get("walls").copied().unwrap_or(0) != 0
        }
    }

    const U: u64 = 0x1000;
    const R: u64 = 0x2000;

    fn world(category: i32, r_category: i32, r_type: i32, resource: i32, hp: i32) -> World {
        let mut w = World::default();
        w.unit(U).category = category;
        let r = w.unit(R);
        (r.category, r.unit_type, r.resource_type, r.hit_points, r.player) = (r_category, r_type, resource, hp, P);
        w
    }

    #[test]
    fn begin_gathering_by_resource() {
        let end = ["timer 1000 2000", "drop path 1000", "reset path 1000", "halt 1000 6"];
        let expect = |head: &[&str], face: bool| {
            let mut v: Vec<String> = head.iter().map(|s| s.to_string()).collect();
            if face {
                v.push("face 1000 2000".into());
            }
            v.extend(end.iter().map(|s| s.to_string()));
            v
        };
        // a naval citizen: animation 0xb, no facing
        let mut w = world(CATEGORY_NAVAL_CITIZEN, 0, 0x13b, RESOURCE_FOOD, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 11", "type 1000 2"], false));
        // a living animal on land, a dead one
        let mut w = world(CATEGORY_LAND_CITIZEN, CATEGORY_LAND, 7, RESOURCE_FOOD, 3);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 11", "type 1000 2"], true));
        let mut w = world(CATEGORY_LAND_CITIZEN, CATEGORY_LAND_CITIZEN, 7, RESOURCE_FOOD, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 10", "type 1000 2"], true));
        // a farm, berries
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, UNIT_TYPE_FARM, RESOURCE_FOOD, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 6", "type 1000 2"], true));
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, 7, RESOURCE_FOOD, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 10", "type 1000 2"], true));
        // wood with and without high-tech tools
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, 7, RESOURCE_WOOD, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 3", "type 1000 3"], true));
        w.log.clear();
        w.knobs.insert("tools", 1);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 3", "release 1000", "type 1000 3"], true));
        // ore with the tools: the fire released, the beam; without: the animation alone
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, 7, RESOURCE_GOLD, 0);
        w.knobs.insert("tools", 1);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 9", "release 1000", "beam 1000 2000", "type 1000 5"], true));
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, 7, RESOURCE_IRON, 0);
        begin_gathering(&mut w, U, R);
        assert_eq!(w.log, expect(&["animation 1000 9", "type 1000 6"], true));
    }

    #[test]
    fn gather_work_hits_or_takes() {
        // a living animal on land: hit, the animation by its life, no regrowth
        let mut w = world(CATEGORY_LAND_CITIZEN, CATEGORY_LAND, 7, RESOURCE_FOOD, 6);
        w.unit(U).animation = 11;
        gather_work(&mut w, U, R);
        assert_eq!(w.log, vec!["hit 2000 1000", "timer 1000 2000"]);
        gather_work(&mut w, U, R);
        assert_eq!(w.log[2..], ["hit 2000 1000", "animation 1000 10", "timer 1000 2000"]);
        // a farm: one unit taken; a dead tree: taken and regrown
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, UNIT_TYPE_FARM, RESOURCE_FOOD, 5);
        gather_work(&mut w, U, R);
        assert_eq!(w.log, vec!["take 2000 1000", "timer 1000 2000"]);
        let mut w = world(CATEGORY_LAND_CITIZEN, 0, 9, RESOURCE_WOOD, 0);
        w.unit(R).is_tree = true;
        gather_work(&mut w, U, R);
        assert_eq!(w.log, vec!["take 2000 1000", "regrow 2000", "timer 1000 2000"]);
    }

    #[test]
    fn naval_search_outcomes() {
        let mut w = world(CATEGORY_NAVAL_CITIZEN, 0, UNIT_TYPE_RES_FISH, RESOURCE_FOOD, 0);
        assert_eq!(naval_search(&mut w, U, 0), (0, 0));
        assert_eq!(naval_search(&mut w, U, R), (2, 0));
        w.knobs.insert("fish", R as i64);
        assert_eq!(naval_search(&mut w, U, R), (2, 0));
        w.knobs.insert("fish", 0x3000);
        assert_eq!(naval_search(&mut w, U, R), (1, 0x3000));
        // a computer player's farm: idle; another resource: wait facing it, 7 or 8 by the draw
        w.unit(R).unit_type = UNIT_TYPE_FARM;
        w.unit(U).computer = true;
        assert_eq!(naval_search(&mut w, U, R), (0, 0));
        w.unit(U).computer = false;
        w.knobs.insert("draw", 70);
        assert_eq!(naval_search(&mut w, U, R), (3, 0));
        assert_eq!(w.log[w.log.len() - 2..], ["face 1000 2000", "animation 1000 8"]);
        w.knobs.insert("walls", 1);
        assert_eq!(naval_search(&mut w, U, R), (3, 0));
        assert_eq!(w.log.last().unwrap(), "animation 1000 7");
    }
}
