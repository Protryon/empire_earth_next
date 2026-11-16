//! citizen.rs' tests over a mock map: units as records on tiles, the unit utilities as simple rings, every effect
//! logged.

use super::*;
use std::collections::BTreeMap;

#[derive(Default)]
struct Mock {
    units: BTreeMap<u64, EeUnitsCzUnit>,
    terrain: BTreeMap<(i32, i32), i32>,
    goals: BTreeMap<u64, (u64, u64, bool)>,
    walls: BTreeMap<u64, u32>,
    drop_sites: Vec<u64>,
    takes: bool,
    player_list: Vec<u64>,
    log: Vec<String>,
}

fn tile_id(x: i32, y: i32) -> u64 {
    ((x + 1000) as u64) << 16 | (y + 1000) as u64
}

fn tile_xy(t: u64) -> (i32, i32) {
    ((t >> 16) as i32 - 1000, (t & 0xffff) as i32 - 1000)
}

impl Mock {
    fn put(&mut self, obj: u64, x: i32, y: i32, f: impl FnOnce(&mut EeUnitsCzUnit)) {
        let mut u = EeUnitsCzUnit {
            obj,
            complex: obj,
            tile_x: x,
            tile_y: y,
            hit_points: 10,
            on_map: 1,
            built: 1,
            tile_size: 1,
            goal_kind: -1,
            category: 1,
            ..Default::default()
        };
        f(&mut u);
        self.units.insert(obj, u);
    }
}

impl CzHost for Mock {
    fn unit(&mut self, u: u64) -> EeUnitsCzUnit {
        self.units.get(&u).copied().unwrap_or_default()
    }
    fn class_tile_size(&mut self, _t: i32) -> i32 {
        1
    }
    fn map_size(&mut self) -> (u32, u32) {
        (64, 64)
    }
    fn square_ring_tiles(&mut self, x: i32, y: i32, r: i32) -> Vec<CzTile> {
        vec![[x + r, y], [x - r, y]]
    }
    fn side_tiles(&mut self, x: i32, y: i32, size: i32, tiles: &mut Vec<CzTile>) {
        *tiles = vec![[x + size, y], [x - 1, y]];
    }
    fn ring_corners(&mut self, _c: i32, x: i32, y: i32, w: i32, h: i32, tiles: &mut Vec<CzTile>) {
        tiles.extend([[x - 1, y - 1], [x + w, y + h]]);
    }
    fn square_ring(&mut self, x: i32, y: i32, w: i32, _h: i32, tiles: &mut Vec<CzTile>) {
        tiles.extend([[x + w, y], [x - 1, y], [x, y + 1]]);
    }
    fn shuffle(&mut self, tiles: &mut Vec<CzTile>) {
        tiles.reverse();
    }
    fn tile_continent(&mut self, x: i32, _y: i32) -> u16 {
        (x >= 0) as u16
    }
    fn unit_continent(&mut self, _u: u64) -> u16 {
        1
    }
    fn block_takes(&mut self, _u: u64, _t: i32, x: i32, _y: i32, _s: i32) -> bool {
        x > 12
    }
    fn tile(&mut self, x: i32, y: i32) -> u64 {
        if x < 0 || y < 0 {
            0
        } else {
            tile_id(x, y)
        }
    }
    fn tile_units(&mut self, tile: u64) -> Vec<u64> {
        let (x, y) = tile_xy(tile);
        self.units.values().filter(|u| u.tile_x == x && u.tile_y == y && u.on_map != 0).map(|u| u.obj).collect()
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        *self.terrain.get(&tile_xy(tile)).unwrap_or(&0)
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        tile_xy(tile).0 as f32 / 10.0
    }
    fn can_gather(&mut self, _u: u64, _t: u64, _x: i32, _y: i32) -> bool {
        true
    }
    fn can_get_to_unit(&mut self, _a: u64, _b: u64) -> bool {
        true
    }
    fn distance_2d(&mut self, u: u64, x: f32, y: f32) -> f32 {
        let s = self.units[&u];
        (s.tile_x as f32 - x).powi(2) + (s.tile_y as f32 - y).powi(2)
    }
    fn tile_distance(&mut self, u: u64, c: u64) -> f32 {
        let (a, b) = (self.units[&u], self.units[&c]);
        ((a.tile_x - b.tile_x).pow(2) + (a.tile_y - b.tile_y).pow(2)) as f32
    }
    fn path_area(&mut self, _x: i32, _y: i32) -> u16 {
        1
    }
    fn gather_storage(&mut self, g: u64) -> u64 {
        self.goals[&g].0
    }
    fn gather_resource(&mut self, g: u64) -> u64 {
        self.goals[&g].1
    }
    fn is_gathering(&mut self, g: u64) -> bool {
        self.goals[&g].2
    }
    fn build_target_is_wall(&mut self, g: u64) -> bool {
        self.walls.contains_key(&g)
    }
    fn build_target_type(&mut self, g: u64) -> u32 {
        self.walls[&g]
    }
    fn farm_drop_sites(&mut self, _farm: u64) -> Vec<u64> {
        self.drop_sites.clone()
    }
    fn can_take(&mut self, _s: u64, _u: u64, _t: u32) -> bool {
        self.takes
    }
    fn empty_load(&mut self, u: u64, t: u32) {
        self.log.push(format!("empty {u} {t}"));
    }
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32 {
        let (a, b) = (self.units[&a], self.units[&b]);
        ((a.tile_x - b.tile_x).pow(2) + (a.tile_y - b.tile_y).pow(2)) as f32
    }
    fn distance_to(&mut self, a: u64, b: u64) -> f32 {
        self.distance_to_unit(a, b)
    }
    fn player_units(&mut self, _p: u64) -> Vec<u64> {
        self.player_list.clone()
    }
}

#[test]
fn wood_site_is_the_first_ring_tile_on_the_continent_that_takes_the_building() {
    let mut m = Mock::default();
    m.put(1, 10, 10, |_| {});
    // rings 2..: (12, 10) is refused, (8, 10) refused, radius 3: (13, 10) taken
    assert_eq!(find_wood_site(&mut m, 1, 0x2b), Some([13.0, 10.0]));
    m.put(2, -20, 10, |_| {});
    assert_eq!(find_wood_site(&mut m, 2, 0x2b), None);
}

#[test]
fn resource_kinds() {
    let tree = EeUnitsCzUnit { resource_type: 3, family: 1, ..Default::default() };
    let animal = EeUnitsCzUnit { family: FAMILY_ANIMAL, ..Default::default() };
    let farm = EeUnitsCzUnit { unit_type: UNIT_TYPE_FARM, resource_type: 0, ..Default::default() };
    assert!(is_resource_of_kind(&tree, None, 3, false));
    assert!(!is_resource_of_kind(&tree, None, 2, false));
    assert!(is_resource_of_kind(&tree, None, 1, false));
    assert!(!is_resource_of_kind(&animal, None, 1, false));
    assert!(is_resource_of_kind(&animal, Some(&animal), 5, true));
    assert!(is_resource_of_kind(&farm, Some(&farm), 2, false));
}

#[test]
fn gather_point_is_the_nearest_free_ring_tile_else_the_corner() {
    let mut m = Mock::default();
    m.put(1, 12, 10, |_| {});
    // the ring of (10, 10) 1x1: (11, 10), (9, 10), (10, 11); (11, 10) is held
    m.put(5, 11, 10, |u| u.category = 2);
    let p = find_gather_point(&mut m, 1, 10, 10, 1, 1);
    assert_eq!(p, [10.5, 11.5, 1.0]);
    // a farm and a foundation do not hold a tile; a cliff is dropped
    m.units.get_mut(&5).unwrap().unit_type = UNIT_TYPE_FARM;
    assert_eq!(find_gather_point(&mut m, 1, 10, 10, 1, 1), [11.5, 10.5, 1.1]);
    m.terrain.insert((11, 10), TERRAIN_CLIFF);
    m.terrain.insert((10, 11), TERRAIN_CLIFF);
    m.terrain.insert((9, 10), TERRAIN_CLIFF);
    assert_eq!(find_gather_point(&mut m, 1, 10, 10, 1, 1), [10.0, 10.0, 1.0]);
    // unit 5's ring from (12, 10): (12, 10) is the citizen's own tile (held), (11, 11) the nearest free one
    assert!(has_gather_point(&mut m, 1, 5));
}

#[test]
fn gather_point_adds_the_corners_for_a_non_gather_goal_on_water() {
    let mut m = Mock::default();
    m.put(1, 0, 0, |u| (u.goal_state, u.goal_kind) = (77, GOAL_BUILD_REPAIR));
    m.terrain.insert((10, 10), TERRAIN_DEEP_WATER);
    m.terrain.insert((11, 10), TERRAIN_DEEP_WATER);
    m.terrain.insert((9, 10), TERRAIN_DEEP_WATER);
    m.terrain.insert((10, 11), TERRAIN_SHALLOW_WATER);
    // the corners (9, 9) and (11, 11) are land: the nearest to (0, 0) wins
    assert_eq!(find_gather_point(&mut m, 1, 10, 10, 1, 1), [9.5, 9.5, 0.9]);
    // a wall build drops the tiles of its own pieces
    m.walls.insert(77, 0x99);
    m.put(9, 9, 9, |u| (u.unit_type, u.foundation) = (0x99, 0));
    m.units.get_mut(&9).unwrap().category = CATEGORY_AMBIENT;
    assert_eq!(find_gather_point(&mut m, 1, 10, 10, 1, 1), [11.5, 11.5, 1.1]);
}

#[test]
fn resource_of_kind_takes_the_nearest_in_the_shuffled_rings() {
    let mut m = Mock::default();
    m.put(1, 5, 5, |u| u.category = CATEGORY_LAND_CITIZEN);
    // the side tiles of (5, 5): (6, 5), (4, 5); a tree at each, the corners empty
    m.put(10, 6, 5, |u| (u.resource_type, u.category) = (3, 2));
    m.put(11, 4, 5, |u| (u.resource_type, u.category) = (3, 2));
    let found = find_resource_of_kind(&mut m, CATEGORY_LAND_CITIZEN, 1, 1, 0, 1, 3, &[], false);
    assert!(found == 10 || found == 11);
    // the taken one is skipped
    // (range 2: the farther tree's floor(2.5) is not under range 1's best, 2)
    let other = find_resource_of_kind(&mut m, CATEGORY_LAND_CITIZEN, 1, 1, 0, 2, 3, &[found], false);
    assert!(other != found && other != 0);
    // no range: none
    assert_eq!(find_resource_of_kind(&mut m, CATEGORY_LAND_CITIZEN, 1, 1, 0, 0, 3, &[], false), 0);
}

#[test]
fn storage_prefers_the_goal_then_the_nearest_drop_site() {
    let mut m = Mock::default();
    m.put(1, 0, 0, |u| (u.category, u.resource_type) = (CATEGORY_LAND_CITIZEN, 3));
    m.put(20, 10, 0, |u| u.drop_site = 1);
    m.put(21, 3, 0, |u| u.drop_site = 1);
    m.put(22, 1, 0, |u| (u.drop_site, u.built) = (1, 0));
    m.player_list = vec![20, 21, 22];
    m.takes = true;
    assert_eq!(find_storage(&mut m, 1, false), 21);
    // a gather goal's storage is kept unless asked for any
    m.goals.insert(50, (20, 0, true));
    m.units.get_mut(&1).unwrap().goal_state = 50;
    m.units.get_mut(&1).unwrap().goal_kind = GOAL_GATHER;
    assert_eq!(find_storage(&mut m, 1, false), 21);
    m.takes = false;
    assert_eq!(find_storage(&mut m, 1, false), 20);
    assert_eq!(find_storage(&mut m, 1, true), 0);
}

#[test]
fn storage_of_a_farm_is_its_drop_site_and_empties_a_load_it_does_not_take() {
    let mut m = Mock::default();
    m.put(1, 0, 0, |u| (u.category, u.resource_type) = (CATEGORY_LAND_CITIZEN, 1));
    m.put(30, 2, 0, |u| u.unit_type = UNIT_TYPE_FARM);
    m.put(31, 4, 0, |u| (u.drop_site, u.resource_type) = (1, 6));
    m.goals.insert(50, (0, 30, true));
    m.units.get_mut(&1).unwrap().goal_state = 50;
    m.units.get_mut(&1).unwrap().goal_kind = GOAL_GATHER;
    m.drop_sites = vec![31];
    m.player_list = vec![];
    assert_eq!(find_storage(&mut m, 1, false), 31);
    assert_eq!(m.log, vec!["empty 1 6"]);
}
