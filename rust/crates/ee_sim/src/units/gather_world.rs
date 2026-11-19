//! The gather and build/repair goals' farm and resource searches (the units behaviour lane, round 1): the helpers
//! EEUGRust.cpp's goal host kept in C++ after round 22 (`EETendNextFarm` 0x61ec58, `EEGatherNearBuilding` 0x61ed5e,
//! the unit utilities' `EEWorkFarm`, `IsAtResourceSpot` 0x622271, `SendToNextFarm` 0x622593, `HasGatherRoom`
//! 0x623757), now Rust over the goals' host: the noted unit utilities (farm drop sites, free farms, tile rings,
//! resource spots, the place on a tile) are the same entry points the C++ forwarders called, so every UUTL note is
//! as before; the class's resource search (its UCBD note) and the goal calls stay host operations.

use crate::units::world::WorldHost;

/// `kActionMoveToWaitingArea`, `kActionMoveToResource`, `kActionMoveToRepairSite`
const ACTION_MOVE_TO_WAITING_AREA: i64 = 4023;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;
/// `kUnitTypeFarm`; `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const UNIT_TYPE_FARM: i32 = 0x2a;
const CATEGORY_LAND_CITIZEN: i32 = 6;
const CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
/// the animal id of a class that is no animal (a mine's)
const NO_ANIMAL: i32 = 1000;
/// the squared distance within which a gatherer checks the resource's spots (0x44759c)
const GATHER_RANGE_SQUARED: f32 = 16.0;
/// `FindResourceOfKind`'s rings and kind for the search near a drop site (a mine: kind 1)
const NEAR_BUILDING_RANGE: i32 = 6;
const NEAR_BUILDING_KIND: i32 = 1;

/// The world calls of the searches beyond the build goals' (the FFI adapter; the tests' mock).
pub trait GatherWorldHost: WorldHost {
    /// the unit's `StartAction(action)` alone (the class told after)
    fn start_only(&mut self, unit: u64, action: i64);
    /// `EEGetFarmDropSites(farm, sites)` into an empty list: whether any, the sites
    fn farm_drop_sites(&mut self, farm: u64) -> (bool, Vec<u64>);
    /// `EEFindFreeFarm(unit, true, &ring)`: the farm's complex unit (0: none)
    fn find_free_farm(&mut self, unit: u64, ring: &mut i32) -> u64;
    /// the unit's class's `FindResourceOfKind(unit, center, resource, range, kind, &taken, farms)`
    #[allow(clippy::too_many_arguments)]
    fn resource_of_kind(
        &mut self,
        unit: u64,
        center: u64,
        resource: u64,
        range: i32,
        kind: i32,
        taken: &[u64],
        farms: bool,
    ) -> u64;
    /// a class's `mIsTree` and `mAnimalID`; a player's `GetMiningLimit()`
    fn resource_class(&mut self, class: u64) -> (bool, i32);
    fn mining_limit(&mut self, player: u64) -> i32;
    /// `GetResourceUsers(unit)` (unsigned)
    fn users_of(&mut self, resource: u64) -> u32;
    /// `EEAIRandomShuffle` of the units
    fn shuffle_units(&mut self, units: &mut [u64]);
    /// `complex->DistanceSquaredToUnit(other)`
    fn squared_distance_to_unit(&mut self, complex: u64, other: u64) -> f32;
    /// `EEGetResourceSpots(resource, tiles)`
    fn resource_spots(&mut self, resource: u64) -> Vec<(i32, i32)>;
    /// `EEFindPlaceOnTile(unit, class, x, y, 1, x, y, 1, &zero)`
    fn place_on_tile(&mut self, unit: u64, class: u64, x: i32, y: i32) -> bool;
    /// `EEGetSideTiles(x, y, size)` and `EEGetCornerTiles(0, x, y, size)`
    fn side_tiles(&mut self, x: i32, y: i32, size: i32) -> Vec<(i32, i32)>;
    fn corner_tiles(&mut self, x: i32, y: i32, size: i32) -> Vec<(i32, i32)>;
    /// the map tile's units in its list's order (0 for a node without one); None off the map
    fn map_tile_units(&mut self, x: i32, y: i32) -> Option<Vec<u64>>;
    /// `unit->IsPassableBy(by)` (a virtual)
    fn passable_by(&mut self, unit: u64, by: u64) -> bool;
}

/// `EETileDistanceLess(center)(a, b)` on units: the farther tile first (the sort puts the nearest last)
fn tile_distance_squared(center: (i32, i32), tile: (i32, i32)) -> i32 {
    let dy = center.1.wrapping_sub(tile.1);
    let dx = center.0.wrapping_sub(tile.0);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// The unit utilities' `EEWorkFarm(farm, unit)`: a gather goal on a built, unhurt farm, else a build/repair goal
/// for its player; the move started, then the class told.
pub fn work_farm(h: &mut dyn GatherWorldHost, farm: u64, unit: u64) {
    let f = h.build_unit(farm);
    if f.built && f.hit_points == f.max_hit_points {
        let goal = h.new_gather(farm, 0, false);
        h.set_goal(unit, goal, 0, 0);
        h.start_only(unit, ACTION_MOVE_TO_RESOURCE);
    } else {
        let goal = h.new_build_goal(f.player, farm);
        h.set_goal(unit, goal, 0, 0);
        h.start_only(unit, ACTION_MOVE_TO_REPAIR_SITE);
    }
    h.action_changed(unit);
}

/// 0x61ec58 `EETendNextFarm`: the unit works the nearest free farm around the farm's drop sites (all of each
/// site's free farms, ring by ring; the list sorted the MSVC way, the nearest last).
pub fn tend_next_farm(h: &mut dyn GatherWorldHost, unit: u64, target: u64) {
    let (found, sites) = h.farm_drop_sites(target);
    if !found {
        return;
    }
    let mut ring = 1;
    let mut free = Vec::new();
    for site in sites {
        let complex = h.build_unit(site).complex;
        loop {
            let farm = h.find_free_farm(complex, &mut ring);
            if farm == 0 {
                break;
            }
            free.push(farm);
        }
    }
    if free.is_empty() {
        return;
    }
    let u = h.build_unit(unit);
    let center = (u.tile_x, u.tile_y);
    let tiles: Vec<(i32, i32)> = free
        .iter()
        .map(|&f| {
            let r = h.build_unit(f);
            (r.tile_x, r.tile_y)
        })
        .collect();
    let mut order: Vec<usize> = (0..free.len()).collect();
    crate::units::msvc_sort::sort_by(&mut order, |&a, &b| {
        tile_distance_squared(center, tiles[a]) > tile_distance_squared(center, tiles[b])
    });
    let last = free[*order.last().expect("a free farm")];
    let complex = h.build_unit(last).complex;
    work_farm(h, complex, unit);
}

/// 0x61ed5e `EEGatherNearBuilding`: the unit gathers a mine near a drop site, passing over mines at the player's
/// worker limit (then a random one of those). The original repeats the identical first search when it finds none:
/// both searches are kept.
pub fn gather_near_building(h: &mut dyn GatherWorldHost, unit: u64, target: u64) {
    let mut rejected: Vec<u64> = Vec::new();
    let search = |h: &mut dyn GatherWorldHost, rejected: &[u64]| {
        h.resource_of_kind(unit, target, 0, NEAR_BUILDING_RANGE, NEAR_BUILDING_KIND, rejected, false)
    };
    let mut resource = search(h, &rejected);
    if resource == 0 {
        resource = search(h, &rejected);
    }
    let player = h.build_unit(unit).player;
    let mut done = false;
    while !done && resource != 0 {
        let class = h.build_unit(resource).unit_class;
        let (tree, animal) = h.resource_class(class);
        if !tree && animal == NO_ANIMAL && h.users_of(resource) >= h.mining_limit(player) as u32 {
            rejected.push(resource);
            resource = search(h, &rejected);
        } else {
            let goal = h.new_gather(resource, target, false);
            h.set_goal(unit, goal, 0, 0);
            h.start_action(unit, ACTION_MOVE_TO_WAITING_AREA);
            done = true;
        }
    }
    if !done && !rejected.is_empty() {
        h.shuffle_units(&mut rejected);
        let goal = h.new_gather(rejected[0], target, false);
        h.set_goal(unit, goal, 0, 0);
        h.start_action(unit, ACTION_MOVE_TO_WAITING_AREA);
    }
}

/// 0x622271 `IsAtResourceSpot`: within 4 tiles of the resource, on one of its gather spots with room to stand.
pub fn at_resource_spot(h: &mut dyn GatherWorldHost, unit: u64, resource: u64) -> bool {
    if resource == 0 || h.squared_distance_to_unit(unit, resource) > GATHER_RANGE_SQUARED {
        return false;
    }
    let spots = h.resource_spots(resource);
    let u = h.build_unit(unit);
    let tile = (u.tile_x, u.tile_y);
    if !spots.contains(&tile) {
        return false;
    }
    h.place_on_tile(unit, u.unit_class, tile.0, tile.1)
}

/// 0x622593 `SendToNextFarm`: the first free farm around the drop sites of the gatherer's farm is worked.
pub fn next_farm(h: &mut dyn GatherWorldHost, unit: u64, resource: u64) {
    let farm = h.build_unit(resource).complex;
    let (found, sites) = h.farm_drop_sites(farm);
    if !found {
        return;
    }
    let mut ring = 1;
    let mut target = 0;
    for site in sites {
        if target != 0 {
            break;
        }
        let complex = h.build_unit(site).complex;
        target = h.find_free_farm(complex, &mut ring);
    }
    if target != 0 {
        work_farm(h, target, unit);
    }
}

/// 0x623757 `HasGatherRoom`: a farm takes one gatherer; another resource its side tiles (counted twice) and its
/// corner tiles, less those off the map or under a unit that is no citizen and does not let the gatherer pass,
/// capped by the player's mining limit (the counts unsigned, as the original's).
pub fn has_gather_room(h: &mut dyn GatherWorldHost, unit: u64, resource: u64) -> bool {
    let r = h.build_unit(resource);
    let limit = if r.unit_type == UNIT_TYPE_FARM {
        1u32
    } else {
        let sides = h.side_tiles(r.tile_x, r.tile_y, r.tile_size);
        let corners = h.corner_tiles(r.tile_x, r.tile_y, r.tile_size);
        let mut count = (sides.len() as u32).wrapping_mul(2).wrapping_add(corners.len() as u32);
        for (x, y) in sides.into_iter().chain(corners) {
            match h.map_tile_units(x, y) {
                None => count = count.wrapping_sub(1),
                Some(units) => {
                    for other in units {
                        if other == 0 {
                            continue;
                        }
                        let category = h.build_unit(other).category;
                        if category != CATEGORY_LAND_CITIZEN
                            && category != CATEGORY_NAVAL_CITIZEN
                            && !h.passable_by(other, unit)
                        {
                            count = count.wrapping_sub(1);
                        }
                    }
                }
            }
        }
        let player = h.build_unit(unit).player;
        let mining = h.mining_limit(player) as u32;
        if count < mining {
            count
        } else {
            mining
        }
    };
    h.users_of(resource) < limit
}

#[cfg(test)]
#[allow(clippy::field_reassign_with_default)]
mod tests {
    use super::*;
    use crate::units::build::tests::World;
    use crate::units::rust_goal::EeUnitsBuildUnit;

    impl GatherWorldHost for World {
        fn start_only(&mut self, u: u64, a: i64) {
            self.log.push(format!("start only {u:x} {a}"));
        }
        fn farm_drop_sites(&mut self, farm: u64) -> (bool, Vec<u64>) {
            self.log.push(format!("drop sites {farm:x}"));
            (!self.radius_units.is_empty(), self.radius_units.clone())
        }
        fn find_free_farm(&mut self, unit: u64, ring: &mut i32) -> u64 {
            // each site's farms by the mock's blocking table, one per call
            let farm =
                self.blocking
                    .get_mut(&(unit as i32, 0))
                    .and_then(|v| if v.is_empty() { None } else { Some(v.remove(0)) });
            *ring += 1;
            farm.unwrap_or(0)
        }
        fn resource_of_kind(
            &mut self,
            _u: u64,
            _c: u64,
            _r: u64,
            range: i32,
            kind: i32,
            taken: &[u64],
            _f: bool,
        ) -> u64 {
            self.log.push(format!("search {range} {kind} {}", taken.len()));
            self.footprint_units.iter().copied().find(|r| !taken.contains(r)).unwrap_or(0)
        }
        fn resource_class(&mut self, class: u64) -> (bool, i32) {
            (false, if class == 0x900 { NO_ANIMAL } else { 0 })
        }
        fn mining_limit(&mut self, _p: u64) -> i32 {
            3
        }
        fn users_of(&mut self, _r: u64) -> u32 {
            self.users as u32
        }
        fn shuffle_units(&mut self, units: &mut [u64]) {
            units.reverse();
        }
        fn squared_distance_to_unit(&mut self, _c: u64, _o: u64) -> f32 {
            9.0
        }
        fn resource_spots(&mut self, _r: u64) -> Vec<(i32, i32)> {
            self.surrounding.clone()
        }
        fn place_on_tile(&mut self, _u: u64, _c: u64, x: i32, y: i32) -> bool {
            self.log.push(format!("place {x},{y}"));
            true
        }
        fn side_tiles(&mut self, x: i32, y: i32, _s: i32) -> Vec<(i32, i32)> {
            vec![(x - 1, y), (x + 1, y)]
        }
        fn corner_tiles(&mut self, x: i32, y: i32, _s: i32) -> Vec<(i32, i32)> {
            vec![(x - 1, y - 1)]
        }
        fn map_tile_units(&mut self, x: i32, y: i32) -> Option<Vec<u64>> {
            (x >= 0).then(|| self.blocking.get(&(x, y)).cloned().unwrap_or_default())
        }
        fn passable_by(&mut self, _u: u64, _b: u64) -> bool {
            false
        }
    }

    #[test]
    fn a_ready_farm_is_gathered_and_another_repaired() {
        let mut w = World::default();
        *w.unit(0xf1) = EeUnitsBuildUnit { built: true, hit_points: 5, max_hit_points: 5, ..Default::default() };
        *w.unit(0xf2) =
            EeUnitsBuildUnit { built: true, hit_points: 4, max_hit_points: 5, player: 0x50, ..Default::default() };
        work_farm(&mut w, 0xf1, 0xa);
        work_farm(&mut w, 0xf2, 0xa);
        assert_eq!(
            w.log,
            [
                "new gather f1 0 false",
                "set goal a 7777 0 0",
                "start only a 8008",
                "changed a",
                "build 50 f2",
                "set goal a 80f2 0 0",
                "start only a 8017",
                "changed a"
            ]
        );
    }

    #[test]
    fn the_nearest_free_farm_is_tended() {
        let mut w = World::default();
        w.radius_units = vec![0x10];
        *w.unit(0x10) = EeUnitsBuildUnit { complex: 0x10, ..Default::default() };
        w.blocking.insert((0x10, 0), vec![0x21, 0x22, 0x23]);
        *w.unit(0xa) = EeUnitsBuildUnit { tile_x: 10, tile_y: 10, ..Default::default() };
        for (f, x) in [(0x21, 20), (0x22, 11), (0x23, 15)] {
            *w.unit(f) = EeUnitsBuildUnit { complex: f, tile_x: x, tile_y: 10, built: true, ..Default::default() };
        }
        tend_next_farm(&mut w, 0xa, 0xf0);
        assert!(w.log.contains(&"new gather 22 0 false".to_string()), "{:?}", w.log);
    }

    #[test]
    fn a_mine_at_its_limit_is_passed_over_then_one_drawn() {
        let mut w = World::default();
        w.footprint_units = vec![0x31, 0x32];
        w.users = 3;
        for m in [0x31, 0x32] {
            *w.unit(m) = EeUnitsBuildUnit { unit_class: 0x900, ..Default::default() };
        }
        gather_near_building(&mut w, 0xa, 0xb);
        // two first searches (the original's repeat only when the first finds none: here one), each rejection a search
        assert_eq!(w.log.iter().filter(|l| l.starts_with("search")).count(), 3, "{:?}", w.log);
        assert!(w.log.contains(&"new gather 32 b false".to_string()), "{:?}", w.log);
        assert!(w.log.contains(&"start a 4023".to_string()));
    }

    #[test]
    fn the_room_counts_the_free_sides_twice_and_the_corners() {
        let mut w = World::default();
        *w.unit(0xa) = EeUnitsBuildUnit { player: 0x50, ..Default::default() };
        *w.unit(0x40) = EeUnitsBuildUnit { tile_x: 5, tile_y: 5, tile_size: 1, ..Default::default() };
        // sides (4,5), (6,5); corner (4,4): one blocker on (6,5)
        w.blocking.insert((6, 5), vec![0x41]);
        *w.unit(0x41) = EeUnitsBuildUnit { category: 3, ..Default::default() };
        // count 2 * 2 + 1 - 1 = 4, capped at the mining limit 3
        w.users = 2;
        assert!(has_gather_room(&mut w, 0xa, 0x40));
        w.users = 3;
        assert!(!has_gather_room(&mut w, 0xa, 0x40));
        *w.unit(0x40) = EeUnitsBuildUnit { unit_type: UNIT_TYPE_FARM, ..Default::default() };
        w.users = 0;
        assert!(has_gather_room(&mut w, 0xa, 0x40));
    }

    #[test]
    fn a_spot_needs_the_unit_on_one_of_the_resources_tiles() {
        let mut w = World::default();
        w.surrounding = vec![(3, 4)];
        *w.unit(0xa) = EeUnitsBuildUnit { tile_x: 3, tile_y: 4, ..Default::default() };
        assert!(at_resource_spot(&mut w, 0xa, 0x50));
        assert!(!at_resource_spot(&mut w, 0xa, 0));
        *w.unit(0xa) = EeUnitsBuildUnit { tile_x: 2, tile_y: 4, ..Default::default() };
        assert!(!at_resource_spot(&mut w, 0xa, 0x50));
    }
}
