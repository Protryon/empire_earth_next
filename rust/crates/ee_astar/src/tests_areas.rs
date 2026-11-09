//! The path area and path sector fills' tests: a small world behind `AreaView`, logging every query.

use crate::area_goals::*;
use crate::goals::{View, TERRAIN_CLIFF, TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::{ModView, Skip, Unit, TERRAIN_SHALLOW_WATER};
use crate::storage::AStarRect;
use std::ffi::c_void;

const STORAGE: *const c_void = 0x30 as *const c_void;

/// `rows` (x across, y down): '.' land, '~' deep water, 's' shallow water, '#' cliff, '*' space, 'f' a forest
/// (resource 1), 'g' gold (resource 3), 'U' a unit obstacle (unit 7, player 2), 'G' a gate (unit 8, player 2), 'W'
/// a wall (unit 9, player 2), 'd' shallow water with a live dock. Continent 1 (land) everywhere but water (2, water),
/// space (3, space) and the edge, which is the map edge continent 4.
struct World {
    rows: Vec<&'static [u8]>,
    values: Vec<Vec<i32>>,
    areas: Vec<Vec<Option<EeAStarPathAreaInfo>>>,
    stored: Vec<Vec<u16>>,
    q: Vec<String>,
}

impl World {
    fn new(rows: Vec<&'static [u8]>) -> World {
        let n = rows.len();
        World {
            rows,
            values: vec![vec![0; n]; n],
            areas: vec![vec![None; n]; n],
            stored: vec![vec![0; n]; n],
            q: vec![],
        }
    }
    fn n(&self) -> i32 {
        self.rows.len() as i32
    }
    fn at(&self, x: i32, y: i32) -> u8 {
        self.rows[y as usize][x as usize]
    }
    fn edge(&self, x: i32, y: i32) -> bool {
        x == 0 || y == 0 || x == self.n() - 1 || y == self.n() - 1
    }
}

impl View for World {
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("has {x} {y}"));
        x >= 0 && y >= 0 && x < self.n() && y < self.n()
    }
    fn terrain(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("terrain {x} {y}"));
        match self.at(x, y) {
            b'~' => TERRAIN_DEEP_WATER,
            b's' | b'd' => TERRAIN_SHALLOW_WATER,
            b'#' => TERRAIN_CLIFF,
            b'*' => TERRAIN_SPACE,
            _ => 0,
        }
    }
    fn is_edge(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("edge {x} {y}"));
        self.edge(x, y)
    }
    fn height_level(&mut self, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    fn continent_id(&mut self, _: i32, _: i32) -> u16 {
        unreachable!()
    }
    fn choke_tile(&mut self, _: i32) -> Option<u32> {
        unreachable!()
    }
}

impl ModView for World {
    fn ai_value(&mut self, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    fn tile_flag(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("flag {x} {y}"));
        self.values[y as usize][x as usize]
    }
    fn can_move_on(&mut self, _: Unit, _: i32) -> bool {
        unreachable!()
    }
    fn are_passable(&mut self, _: Unit, _: i32, _: i32, _: Skip) -> bool {
        unreachable!()
    }
    fn formation_rank(&mut self, _: Unit) -> i32 {
        unreachable!()
    }
    fn step_blocker(&mut self, _: Unit, _: bool, _: i32, _: i32) -> bool {
        unreachable!()
    }
    fn unit_region_state(&mut self, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    fn epoch(&mut self, _: Unit) -> i32 {
        unreachable!()
    }
    fn is_alone(&mut self, _: Unit) -> bool {
        unreachable!()
    }
    fn water_state(&mut self, _: Unit) -> i32 {
        unreachable!()
    }
    fn is_sea_going(&mut self, _: Unit) -> bool {
        unreachable!()
    }
    fn has_hill(&mut self, _: i32, _: i32) -> bool {
        unreachable!()
    }
    fn random_at_most(&mut self, _: f32) -> bool {
        unreachable!()
    }
}

impl AreaView for World {
    fn has_units(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("units {x} {y}"));
        matches!(self.at(x, y), b'f' | b'g' | b'U' | b'G' | b'W' | b'd')
    }
    fn continent_at(&mut self, x: i32, y: i32) -> EeAStarContinentInfo {
        self.q.push(format!("continent {x} {y}"));
        if self.edge(x, y) {
            return EeAStarContinentInfo { id: 4, category: 5 };
        }
        match self.at(x, y) {
            b'~' => EeAStarContinentInfo { id: 2, category: 2 },
            b'*' => EeAStarContinentInfo { id: 3, category: 4 },
            b'#' => EeAStarContinentInfo { id: 5, category: 3 },
            _ => EeAStarContinentInfo { id: 1, category: 1 },
        }
    }
    fn tile_value(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("value {x} {y}"));
        self.values[y as usize][x as usize]
    }
    fn path_area_at(&mut self, x: i32, y: i32) -> EeAStarPathAreaInfo {
        self.q.push(format!("area {x} {y}"));
        self.areas[y as usize][x as usize].unwrap_or_default()
    }
    fn path_area(&mut self, id: u16) -> EeAStarPathAreaInfo {
        self.q.push(format!("area# {id}"));
        EeAStarPathAreaInfo { found: true, id, continent_id: 1, kind: if id == 3 { KIND_CLIFF } else { KIND_LAND } }
    }
    fn stored_path_area_id(&mut self, storage: *const c_void, x: i32, y: i32) -> u16 {
        assert_eq!(storage, STORAGE);
        self.q.push(format!("stored {x} {y}"));
        self.stored[y as usize][x as usize]
    }
    fn resource_kind(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("resource {x} {y}"));
        match self.at(x, y) {
            b'f' => 1,
            b'g' => 3,
            _ => 0,
        }
    }
    fn has_blocker(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("blocker {x} {y}"));
        matches!(self.at(x, y), b'U' | b'G' | b'W')
    }
    fn first_blocker_id(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("blocker# {x} {y}"));
        self.first_blocker(x, y).unit_id
    }
    fn first_blocker(&mut self, x: i32, y: i32) -> EeAStarBlocker {
        let b =
            |id, gate, wall| EeAStarBlocker { found: true, unit_id: id, player_id: 2, is_gate: gate, is_wall: wall };
        match self.at(x, y) {
            b'U' => b(7, false, false),
            b'G' => b(8, true, false),
            b'W' => b(9, false, true),
            _ => EeAStarBlocker { unit_id: -1, ..Default::default() },
        }
    }
    fn first_obstacle_id(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("obstacle {x} {y}"));
        self.first_blocker(x, y).unit_id
    }
    fn own_wall_first(&mut self, x: i32, y: i32, player_id: i32) -> bool {
        self.q.push(format!("wall {x} {y} {player_id}"));
        self.at(x, y) == b'W' && player_id == 2
    }
    fn area_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        self.q.push(format!("agate {x} {y} {player_id}"));
        self.first_blocker(x, y).unit_id
    }
    fn sector_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        self.q.push(format!("sgate {x} {y} {player_id}"));
        self.first_blocker(x, y).unit_id
    }
    fn live_dock(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("dock {x} {y}"));
        self.at(x, y) == b'd'
    }
    fn live_space_dock(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("sdock {x} {y}"));
        false
    }
}

const MAP: [&[u8]; 7] = [b".......", b".ff.~~.", b".gU.~~.", b"..GW.s.", b".#..sd.", b".**.##.", b"......."];

fn area_fill(mode: i32) -> EeAStarPathAreaFill {
    EeAStarPathAreaFill { mode, owner_id: -1, next_forest_id: 5, ..Default::default() }
}

fn rect(left: i32, top: i32, right: i32, bottom: i32) -> AStarRect {
    AStarRect { bottom, left, right, top }
}

#[test]
fn area_first_land_tile_picks_land_then_matches_it() {
    let mut v = World::new(MAP.to_vec());
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 3, 1));
    assert_eq!((f.continent_id, f.continent_kind, f.kind, f.check_corners, f.tile_count), (1, 1, KIND_LAND, 1, 1));
    assert_eq!(f.tile_bounds, rect(3, 1, 3, 1));
    assert_eq!(v.q, ["has 3 1", "continent 3 1", "terrain 3 1", "resource 3 1", "edge 3 1"]);
    v.q.clear();
    assert!(path_area_accept(&mut f, &mut v, 3, 2));
    assert_eq!(v.q, ["has 3 2", "continent 3 2", "terrain 3 2", "resource 3 2", "blocker 3 2", "edge 3 2"]);
    assert_eq!(f.check_corners, -1);
    assert_eq!(f.tile_bounds, rect(3, 1, 3, 2));
    assert!(!path_area_accept(&mut f, &mut v, 1, 1)); // a forest
    assert!(!path_area_accept(&mut f, &mut v, 2, 2)); // a unit
    assert!(!path_area_accept(&mut f, &mut v, 4, 1)); // water: another continent
    assert!(!path_area_accept(&mut f, &mut v, 5, 3)); // shallows on land
    assert!(!path_area_accept(&mut f, &mut v, 9, 9));
    assert_eq!(f.tile_count, 2);
}

#[test]
fn area_first_tiles_of_the_other_kinds() {
    let mut v = World::new(MAP.to_vec());
    // a forest: its kind, the next forest id, its resources, no corners
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 1, 1));
    assert_eq!((f.kind, f.forest_id, f.resources, f.check_corners), (KIND_FOREST, 5, [1, 0, 0], 0));
    assert!(path_area_accept(&mut f, &mut v, 2, 1));
    assert!(path_area_accept(&mut f, &mut v, 1, 2));
    assert_eq!(f.resources, [2, 0, 1]);
    assert!(!path_area_accept(&mut f, &mut v, 3, 1));
    // unit obstacles, gates and walls take their unit and player
    for (x, y, kind, owner) in [(2, 2, KIND_UNIT_OBSTACLE, 7), (2, 3, KIND_GATE, 8), (3, 3, KIND_WALL, 9)] {
        let mut f = area_fill(0);
        assert!(path_area_accept(&mut f, &mut v, x, y));
        assert_eq!((f.kind, f.owner_id, f.player_id, f.check_corners), (kind, owner, 2, 0));
        assert!(path_area_accept(&mut f, &mut v, x, y)); // the same unit again
    }
    // water by its tile value; shallows on land by theirs
    v.values[1][4] = -1;
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 4, 1));
    assert_eq!((f.kind, f.check_corners), (KIND_SHORE, 1));
    assert!(!path_area_accept(&mut f, &mut v, 5, 1)); // value 0
    v.values[3][5] = 2;
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 5, 3));
    assert_eq!(f.kind, KIND_SHALLOWS_NEAR_SHORE);
    // cliffs and the map edge: no corners; the edge continent takes only edge tiles
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 4, 5));
    assert_eq!((f.kind, f.check_corners), (KIND_CLIFF, 0));
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 0, 0));
    assert_eq!((f.kind, f.check_corners), (KIND_MAP_EDGE, 0));
    assert!(path_area_accept(&mut f, &mut v, 6, 0));
    let mut f = area_fill(0);
    assert!(path_area_accept(&mut f, &mut v, 1, 5));
    assert_eq!(f.kind, KIND_SPACE);
}

#[test]
fn area_kind_pass_classifies() {
    let mut v = World::new(MAP.to_vec());
    // land mass: the first tile's class, then land tiles without resources
    let mut f = area_fill(2);
    assert!(path_area_accept(&mut f, &mut v, 3, 1));
    assert_eq!((f.continent_kind, f.tile_count), (1, 1));
    assert_eq!(f.check_corners, -1);
    assert!(path_area_accept(&mut f, &mut v, 3, 2));
    assert!(!path_area_accept(&mut f, &mut v, 1, 1)); // a forest
    assert!(!path_area_accept(&mut f, &mut v, 4, 1)); // water
                                                      // water body: shallows count as water; under units only a working dock
    let mut f = area_fill(1);
    assert!(path_area_accept(&mut f, &mut v, 5, 3));
    assert_eq!(f.continent_kind, 2);
    v.areas[4][5] = Some(EeAStarPathAreaInfo { found: true, id: 9, continent_id: 1, kind: KIND_UNIT_OBSTACLE });
    v.q.clear();
    assert!(path_area_accept(&mut f, &mut v, 5, 4));
    assert_eq!(v.q, ["has 5 4", "terrain 5 4", "edge 5 4", "units 5 4", "area 5 4", "dock 5 4"]);
    v.areas[4][5] = Some(EeAStarPathAreaInfo { found: true, id: 9, continent_id: 1, kind: KIND_LAND });
    assert!(!path_area_accept(&mut f, &mut v, 5, 4));
    // the non-space pass: shallows and water are land
    let mut f = area_fill(4);
    assert!(path_area_accept(&mut f, &mut v, 4, 1));
    assert_eq!(f.continent_kind, 1);
    assert!(path_area_accept(&mut f, &mut v, 1, 1));
    assert!(!path_area_accept(&mut f, &mut v, 1, 5)); // space
                                                      // an edge first tile
    let mut f = area_fill(3);
    assert!(path_area_accept(&mut f, &mut v, 0, 3));
    assert_eq!(f.continent_kind, 5);
    assert!(!path_area_accept(&mut f, &mut v, 3, 3));
}

fn sector_fill(classify: bool) -> EeAStarPathSectorFill {
    EeAStarPathSectorFill {
        path_area_storage: STORAGE,
        classify_tiles: classify,
        tile_count: 0,
        tile_bounds: AStarRect::default(),
        sector_bounds: rect(1, 1, 4, 4),
        path_area_id: 0,
        path_area_kind: 0,
        continent_id: 0,
        category: 0,
        kind: 0,
        unit_id: -1,
        player_id: 0,
        wa0: 0,
        wa2: 6,
        check_corners: -1,
        stale: false,
    }
}

#[test]
fn sector_follows_its_first_path_area_inside_the_bounds() {
    let mut v = World::new(MAP.to_vec());
    v.stored[1][3] = 2;
    v.stored[2][3] = 2;
    v.stored[5][3] = 2;
    v.stored[1][1] = 3;
    let mut f = sector_fill(false);
    assert!(path_sector_accept(&mut f, &mut v, 3, 1));
    assert_eq!((f.path_area_id, f.continent_id, f.path_area_kind, f.check_corners), (2, 1, KIND_LAND, 1));
    assert_eq!(v.q, ["has 3 1", "stored 3 1", "area# 2"]);
    assert!(path_sector_accept(&mut f, &mut v, 3, 2));
    assert!(!path_sector_accept(&mut f, &mut v, 4, 2)); // another area
    assert!(!path_sector_accept(&mut f, &mut v, 3, 5)); // outside the bounds
    assert_eq!((f.tile_count, f.tile_bounds), (2, rect(3, 1, 3, 2)));
    // a cliff area: no corners
    let mut f = sector_fill(false);
    assert!(path_sector_accept(&mut f, &mut v, 1, 1));
    assert_eq!(f.check_corners, 0);
}

#[test]
fn sector_classify_sets_then_matches_and_marks_stale_tiles() {
    let mut v = World::new(MAP.to_vec());
    v.areas[1][3] = Some(EeAStarPathAreaInfo { found: true, id: 4, continent_id: 1, kind: KIND_LAND });
    let mut f = sector_fill(true);
    assert!(path_sector_accept(&mut f, &mut v, 3, 1));
    assert_eq!(
        (f.continent_id, f.category, f.kind, f.path_area_id, f.check_corners, f.stale),
        (1, 1, KIND_LAND, 4, 1, false)
    );
    assert_eq!(v.q, ["has 3 1", "terrain 3 1", "continent 3 1", "area 3 1", "resource 3 1", "edge 3 1", "area 3 1"]);
    v.q.clear();
    assert!(path_sector_accept(&mut f, &mut v, 3, 2));
    assert!(f.stale); // no path area of its kind there
    assert_eq!(f.check_corners, -1);
    assert_eq!(v.q, ["has 3 2", "terrain 3 2", "continent 3 2", "resource 3 2", "blocker 3 2", "edge 3 2", "area 3 2"]);
    assert!(!path_sector_accept(&mut f, &mut v, 2, 2)); // a unit
    assert!(!path_sector_accept(&mut f, &mut v, 5, 3)); // shallows
    assert_eq!(f.tile_count, 2);
    // a forest keeps mWA2 in mWA0; a gate its unit and player, without corners
    let mut f = sector_fill(true);
    assert!(path_sector_accept(&mut f, &mut v, 1, 1));
    assert_eq!((f.kind, f.wa0), (KIND_FOREST, 6));
    let mut f = sector_fill(true);
    assert!(path_sector_accept(&mut f, &mut v, 2, 3));
    assert_eq!((f.kind, f.unit_id, f.player_id, f.check_corners), (KIND_GATE, 8, 2, 0));
    assert!(path_sector_accept(&mut f, &mut v, 2, 3));
    // shallows by the tile flag; a wall of the fill's player
    v.values[3][5] = -3;
    v.values[4][4] = -1;
    let mut f = sector_fill(true);
    f.sector_bounds = rect(0, 0, 6, 6);
    assert!(path_sector_accept(&mut f, &mut v, 5, 3));
    assert_eq!(f.kind, KIND_SHALLOWS_SHORE);
    assert!(path_sector_accept(&mut f, &mut v, 4, 4));
    let mut f = sector_fill(true);
    assert!(path_sector_accept(&mut f, &mut v, 3, 3));
    assert_eq!(f.kind, KIND_WALL);
    v.q.clear();
    assert!(path_sector_accept(&mut f, &mut v, 3, 3));
    assert!(v.q.contains(&"wall 3 3 2".to_string()));
    // the map edge
    let mut f = sector_fill(true);
    assert!(path_sector_accept(&mut f, &mut v, 0, 2));
    assert_eq!((f.kind, f.check_corners), (KIND_MAP_EDGE, 0));
    assert!(!path_sector_accept(&mut f, &mut v, 6, 2)); // outside the sector bounds
}
