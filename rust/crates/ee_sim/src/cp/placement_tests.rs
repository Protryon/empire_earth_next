//! The placement's unit tests over a fake world ([`Fake`]: land tiles, units, one continent, the threats and area
//! effects a test sets), shared with `resource_center_tests`.

use std::collections::BTreeSet;

use super::place_host::{
    corner_tiles_all, distance_squared, distance_to, shuffle, square_block_ring_all, within, EeCpUnitInfo, PlaceHost,
    Rect, RectFilter, Tile, CP_NO_TILE,
};
use super::placement::*;

pub(crate) const SIZE: i32 = 40;
pub(crate) const WATER: i32 = 1;

/// A fake world: a `SIZE` square map (terrain 0, land, unless set), units (handle = index + 1, id = handle + 100),
/// one land continent (id 1) and water continent 2, the centres (handle = 0x1000 + index).
pub(crate) struct Fake {
    pub terrain: Vec<i32>,
    pub units: Vec<EeCpUnitInfo>,
    pub own: Vec<u64>,
    pub town: Tile,
    pub epoch: i32,
    pub threats: BTreeSet<Tile>,
    pub effects: BTreeSet<(i32, i32, i32)>,
    pub centers: Vec<(Vec<u32>, u32, bool)>,
    pub draws: Vec<i32>,
    pub random_value: i32,
    pub ranges: Vec<(i32, f32)>,
    pub morale: Option<(i32, i32, f32)>,
    pub class_size: i32,
    /// the events tests' world: the record, the players (human, population), the teams with enough wonders,
    /// the favourite candidates by class, the history, the cheats on
    pub rec: Box<super::fields::EeCpFields>,
    pub players: Vec<(bool, i32)>,
    pub wonder_teams: Vec<bool>,
    pub candidates: Vec<Option<(i32, i32)>>,
    pub history: Vec<(u32, i32)>,
    pub cheats: i32,
}

impl Default for Fake {
    fn default() -> Fake {
        Fake {
            terrain: vec![0; (SIZE * SIZE) as usize],
            units: Vec::new(),
            own: Vec::new(),
            town: [CP_NO_TILE, CP_NO_TILE],
            epoch: 1,
            threats: BTreeSet::new(),
            effects: BTreeSet::new(),
            centers: Vec::new(),
            draws: Vec::new(),
            random_value: 0,
            ranges: Vec::new(),
            morale: None,
            class_size: 2,
            rec: Box::default(),
            players: Vec::new(),
            wonder_teams: Vec::new(),
            candidates: Vec::new(),
            history: Vec::new(),
            cheats: 0,
        }
    }
}

impl Fake {
    pub fn set(&mut self, t: Tile, terrain: i32) {
        self.terrain[(t[1] * SIZE + t[0]) as usize] = terrain;
    }
    fn on_map(t: Tile) -> bool {
        t[0] >= 0 && t[1] >= 0 && t[0] < SIZE && t[1] < SIZE
    }
    /// A unit of the player (or not): its handle.
    pub fn add(&mut self, unit_type: i32, t: Tile, own: bool) -> u64 {
        let handle = self.units.len() as u64 + 1;
        self.units.push(EeCpUnitInfo {
            tile: t,
            id: handle as u32 + 100,
            unit_type,
            tile_size: 1,
            hit_points: 10,
            resource_amount: 100,
            complex: handle,
            complex_tile: t,
            complex_hit_points: 10,
            valid: 1,
            has_tile: 1,
            has_tile_node: 1,
            has_class: 1,
            ..Default::default()
        });
        if own {
            self.own.push(handle);
        }
        handle
    }
    fn occupied(&self, t: Tile) -> bool {
        self.units.iter().any(|u| u.tile == t && u.valid != 0)
    }
}

impl PlaceHost for Fake {
    fn player_id(&mut self) -> u32 {
        1
    }
    fn player_handle(&mut self) -> u64 {
        0x77
    }
    fn epoch(&mut self) -> i32 {
        self.epoch
    }
    fn drop_off_distance(&mut self) -> i32 {
        6
    }
    fn tower_spacing(&mut self) -> f32 {
        1.0
    }
    fn town(&mut self) -> Tile {
        self.town
    }
    fn set_town(&mut self, t: Tile) {
        self.town = t;
    }
    fn passability(&mut self) -> i32 {
        1
    }
    fn range(&mut self, class: i32) -> f32 {
        self.ranges.iter().find(|r| r.0 == class).map_or(4.0, |r| r.1)
    }
    fn tech_kind(&mut self, _id: i32) -> Option<i32> {
        None
    }
    fn has_pathfinding(&mut self) -> bool {
        false
    }
    fn claim_range_on(&mut self) -> bool {
        false
    }
    fn claim_range(&mut self) -> i32 {
        0
    }
    fn own_units(&mut self) -> Vec<u64> {
        self.own.clone()
    }
    fn units_of_type(&mut self, unit_type: i32, _player_id: u32, units: &mut Vec<u64>) {
        units.clear();
        for &u in &self.own {
            if self.units[u as usize - 1].unit_type == unit_type {
                units.push(u);
            }
        }
    }
    fn nature_units(&mut self) -> Option<Vec<u64>> {
        Some((1..=self.units.len() as u64).filter(|u| !self.own.contains(u)).collect())
    }
    fn towns(&mut self) -> Vec<Option<(u16, Tile)>> {
        Vec::new()
    }
    fn centers(&mut self) -> Vec<u64> {
        (0..self.centers.len() as u64).map(|i| 0x1000 + i).collect()
    }
    fn center_units(&mut self, center: u64) -> Vec<u32> {
        self.centers[(center - 0x1000) as usize].0.clone()
    }
    fn center_unit(&mut self, center: u64) -> u32 {
        self.centers[(center - 0x1000) as usize].1
    }
    fn center_active(&mut self, center: u64) -> bool {
        self.centers[(center - 0x1000) as usize].2
    }
    fn center_set_order(&mut self, center: u64, ids: &[u32]) {
        self.centers[(center - 0x1000) as usize].0 = ids.to_vec();
    }
    fn center_clear_units(&mut self, center: u64) {
        self.centers[(center - 0x1000) as usize].0.clear();
    }
    fn center_add_unit(&mut self, center: u64, unit: u64) {
        let id = self.units[unit as usize - 1].id;
        self.centers[(center - 0x1000) as usize].0.push(id);
    }
    fn center_restore_main(&mut self, center: u64, id: u32) -> i32 {
        if id == u32::MAX {
            return 0;
        }
        let found = self.resolve(id) != 0;
        if !found {
            self.centers[(center - 0x1000) as usize].1 = u32::MAX;
        }
        if found {
            0
        } else {
            3
        }
    }
    fn has_player0(&mut self) -> bool {
        true
    }
    fn resolve(&mut self, id: u32) -> u64 {
        self.units.iter().position(|u| u.id == id).map_or(0, |i| i as u64 + 1)
    }
    fn world_unit(&mut self, id: u32) -> u64 {
        self.resolve(id)
    }
    fn unit(&mut self, unit: u64) -> EeCpUnitInfo {
        if unit == 0 {
            return EeCpUnitInfo::default();
        }
        self.units[unit as usize - 1]
    }
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32 {
        let (a, b) = (self.unit(a).tile, self.unit(b).tile);
        distance_squared(a, b) as f32
    }
    fn class_count(&mut self) -> u32 {
        0x400
    }
    fn class_exists(&mut self, _class: i32) -> bool {
        true
    }
    fn class_tile_size(&mut self, _class: i32) -> i32 {
        self.class_size
    }
    fn class_terrain(&mut self, _class: i32, terrain: i32) -> bool {
        terrain == 0
    }
    fn class_technology(&mut self, _class: i32) -> i32 {
        0
    }
    fn has_map(&mut self) -> bool {
        true
    }
    fn map_flags(&mut self) -> u32 {
        0
    }
    fn tile_terrain(&mut self, t: Tile) -> Option<i32> {
        Fake::on_map(t).then(|| self.terrain[(t[1] * SIZE + t[0]) as usize])
    }
    fn map_tile_terrain(&mut self, t: Tile) -> Option<i32> {
        self.tile_terrain(t)
    }
    fn tile_free(&mut self, t: Tile) -> Option<bool> {
        Fake::on_map(t).then(|| !self.occupied(t))
    }
    fn is_edge(&mut self, _t: Tile) -> bool {
        false
    }
    fn largest_continent(&mut self, _kind: i32) -> i32 {
        2
    }
    fn continent_from_id(&mut self, id: u16) -> u64 {
        id as u64
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        id as u64
    }
    fn continent_at(&mut self, t: Tile) -> u64 {
        self.tile_continent(t) as u64
    }
    fn continent_id(&mut self, continent: u64) -> u16 {
        continent as u16
    }
    fn continent_flag(&mut self, _a: u16, _b: u16) -> bool {
        true
    }
    fn shore_next_to(&mut self, _continent: u64, _id: u16, tiles: &mut Vec<Tile>) {
        tiles.extend(self.shore_tiles(2));
    }
    fn shore_tiles(&mut self, _continent: u64) -> Vec<Tile> {
        // the water tiles with a land neighbour, top row first
        let mut out = Vec::new();
        for y in 0..SIZE {
            for x in 0..SIZE {
                if self.terrain[(y * SIZE + x) as usize] != WATER {
                    continue;
                }
                let land = [[-1, 0], [1, 0], [0, -1], [0, 1]]
                    .iter()
                    .any(|d| matches!(self.tile_terrain([x + d[0], y + d[1]]), Some(0)));
                if land {
                    out.push([x, y]);
                }
            }
        }
        out
    }
    fn hull_center(&mut self, _area: u64, _choke: bool) -> Tile {
        [0, 0]
    }
    fn are_connected(&mut self, a: u64, b: u64, _flags: i32) -> bool {
        a == b
    }
    fn tile_continent(&mut self, t: Tile) -> u16 {
        match self.tile_terrain(t) {
            Some(0) => 1,
            Some(_) => 2,
            None => 0,
        }
    }
    fn unit_continent(&mut self, unit: u64) -> u16 {
        let t = self.unit(unit).tile;
        self.tile_continent(t)
    }
    fn continent_id_at(&mut self, t: Tile) -> u16 {
        self.tile_continent(t)
    }
    fn choke_at(&mut self, _t: Tile) -> bool {
        false
    }
    fn hill_at(&mut self, _t: Tile) -> bool {
        false
    }
    fn path_area_at(&mut self, t: Tile) -> u64 {
        self.tile_continent(t) as u64
    }
    fn unit_path_area(&mut self, unit: u64) -> u64 {
        self.unit_continent(unit) as u64
    }
    fn choke_points(&mut self, _id: u16) -> Vec<u64> {
        Vec::new()
    }
    fn hull_bounds(&mut self, _area: u64) -> Rect {
        Rect::default()
    }
    fn choke_point_tiles(&mut self, _choke: u64, tiles: &mut Vec<Tile>) {
        tiles.clear();
    }
    fn block_takes(&mut self, _unit_type: i32, x: i32, y: i32, w: i32, h: i32) -> bool {
        for tx in x..x + w {
            for ty in y..y + h {
                if self.tile_terrain([tx, ty]) != Some(0) || self.occupied([tx, ty]) {
                    return false;
                }
            }
        }
        true
    }
    fn facing_quarter(&mut self, _t: Tile) -> i32 {
        0
    }
    fn site_threat(&mut self, t: Tile) -> bool {
        self.threats.contains(&t)
    }
    fn tile_unit_of_type(&mut self, t: Tile, unit_type: i32) -> bool {
        self.units.iter().any(|u| u.tile == t && u.unit_type == unit_type)
    }
    fn units_in_rect(&mut self, rect: Rect, _filter: RectFilter, units: &mut Vec<u64>) {
        for (i, u) in self.units.iter().enumerate() {
            if rect.contains(u.tile) {
                units.push(i as u64 + 1);
            }
        }
    }
    fn random(&mut self, max: i32) -> i32 {
        self.draws.push(max);
        self.random_value.min(max)
    }
    fn space_dock_open(&mut self, _t: Tile) -> bool {
        false
    }
    fn sort_order(&mut self, keys: &[i64]) -> Vec<usize> {
        let mut order: Vec<usize> = (0..keys.len()).collect();
        order.sort_by_key(|&i| keys[i]);
        order
    }
    fn has_area_effect(&mut self, kind: i32, x: i32, y: i32, _player: u32) -> bool {
        self.effects.contains(&(kind, x, y))
    }
    fn morale_radius(&mut self, _unit: u64) -> Option<f32> {
        self.morale.map(|m| m.2)
    }
    fn morale_recount(&mut self, _unit: u64) -> Option<(i32, i32, f32)> {
        self.morale
    }
    fn effect_templates(&mut self, _class: i32) -> [u32; 4] {
        [0, 7, 0, 9]
    }
    fn template_radius(&mut self, template: u32) -> f32 {
        if template == 7 {
            6.4
        } else {
            3.6
        }
    }
}

#[test]
fn epoch_classes() {
    assert_eq!([anti_air_class(8), anti_air_class(9), anti_air_class(12), anti_air_class(14)], [0, 0x91, 0x93, 0x300]);
    assert_eq!(anti_air_class(15), 0);
    assert_eq!(
        [fishing_boat_class(0), fishing_boat_class(3), fishing_boat_class(9), fishing_boat_class(15)],
        [0x6c, 0x6d, 0xd3, 0]
    );
    assert_eq!(
        [gate_class(1), gate_class(2), gate_class(11), gate_class(14), gate_class(15)],
        [0x2e, 0x1d9, 0x1ba, 0x2ff, 0]
    );
    assert_eq!([tower_class(0), tower_class(6), tower_class(13), tower_class(14)], [0x2f, 0x1cf, 0x1d2, 0x2fd]);
    assert_eq!([wall_class(1), wall_class(2), wall_class(8), wall_class(14)], [0, 0x1d3, 0x1d6, 0x2fe]);
    assert!(is_site_class(0x24) && is_site_class(0x31f) && !is_site_class(0x2f));
}

#[test]
fn tile_math() {
    assert_eq!(distance_squared([1, 2], [4, 6]), 25);
    assert_eq!(distance_to([0, 0], [3, 4]), 5);
    assert_eq!(distance_to([0, 0], [1, 1]), 1);
    assert!(within([0, 0], [3, 4], 25.0) && !within([0, 0], [3, 4], 24.9));
    // the block ring: the sides per row and column, then the corners
    assert_eq!(square_block_ring_all(5, 5, 1), vec![[4, 5], [6, 5], [5, 4], [5, 6], [4, 4], [4, 6], [6, 4], [6, 6]]);
    assert_eq!(corner_tiles_all(5, 5, 2), vec![[4, 4], [4, 7], [7, 4], [7, 7]]);
    let mut r = Rect::point([3, 4]);
    r.extend([9, 1]);
    assert_eq!(r.center(), [6, 2]);
    r.grow(5, true);
    assert_eq!((r.left, r.top, r.right, r.bottom), (0, 0, 14, 9));
}

#[test]
fn shuffle_draws_as_dinkum() {
    let mut h = Fake::default();
    let mut v = vec![10, 20, 30, 40];
    shuffle(&mut h, &mut v);
    // every draw 0: each element swapped with the first in turn
    assert_eq!(v, vec![40, 10, 20, 30]);
    assert_eq!(h.draws, vec![1, 2, 3]);
    let mut empty: Vec<i32> = Vec::new();
    shuffle(&mut h, &mut empty);
    assert_eq!(h.draws.len(), 3);
}

#[test]
fn can_build_at_open_land_and_border() {
    let mut h = Fake::default();
    assert!(can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [10, 10], 2, false, true, [-1, -1]));
    // the wrong continent, or an enemy near
    assert!(!can_build_at(&mut h, 2, CP_TYPE_BARRACKS, [10, 10], 2, false, true, [-1, -1]));
    h.threats.insert([10, 10]);
    assert!(!can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [10, 10], 2, false, true, [-1, -1]));
    h.threats.clear();
    // one blocked run on the border is let through, two are not
    h.set([9, 10], WATER);
    h.set([9, 11], WATER);
    assert!(can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [10, 10], 2, false, true, [-1, -1]));
    h.set([12, 12], WATER);
    assert!(!can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [10, 10], 2, false, true, [-1, -1]));
    // not strict: the border is not asked
    assert!(can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [10, 10], 2, false, false, [-1, -1]));
    // the margin needs the grown footprint free
    let blocker = h.add(CP_TYPE_HOUSE, [15, 15], false);
    assert!(blocker != 0);
    assert!(!can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [16, 16], 2, true, false, [-1, -1]));
    assert!(can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [17, 17], 2, true, false, [-1, -1]));
}

#[test]
fn town_center_and_units_near() {
    let mut h = Fake::default();
    let mut s = EeCpPlacement::default();
    let house = h.add(CP_TYPE_HOUSE, [3, 3], true);
    h.units[house as usize - 1].land_building = 1;
    unit_added(&mut h, &mut s, house);
    assert_eq!(h.town, [CP_NO_TILE, CP_NO_TILE]);
    let citizen = h.add(CP_TYPE_CITIZEN, [8, 9], true);
    unit_added(&mut h, &mut s, citizen);
    assert_eq!(h.town, [8, 9]);
    // a second class does not move it; a seen class is not asked again
    let other = h.add(0x50, [20, 20], true);
    unit_added(&mut h, &mut s, other);
    assert_eq!(h.town, [8, 9]);
    assert_eq!(s.types_seen.len(), 3);
    reset(&mut h, &mut s);
    assert_eq!(h.town, [CP_NO_TILE, CP_NO_TILE]);
    assert!(s.types_seen.is_empty());
    let tower = h.add(tower_class(1), [5, 5], true);
    assert!(tower != 0);
    assert!(has_tower_near(&mut h, [5, 8], 9.0) && !has_tower_near(&mut h, [5, 8], 8.9));
    let tc = h.add(CP_TYPE_TOWN_CENTER, [30, 30], true);
    assert!(tc != 0);
    assert!(has_town_center_near(&mut h, [33, 30], 3.0) && !has_town_center_near(&mut h, [34, 30], 3.0));
    assert_eq!(count_own(&mut h, CP_TYPE_CITIZEN), 1);
}

#[test]
fn next_place_search_rings() {
    let mut h = Fake::default();
    let s = EeCpPlacement::default();
    // ring 0 is the tile itself (with the margin)
    let r = next_place_search(&mut h, Some(&s), CP_TYPE_BARRACKS, CP_PLACE_RING, [10, 10], 0, 3, &[], 0);
    assert_eq!((r.found, r.x, r.y, r.gap), (1, 10, 10, -1));
    // blocked there: the first tile of ring 1 that fits (the top row, left to right)
    h.add(CP_TYPE_HOUSE, [11, 11], false);
    let r = next_place_search(&mut h, Some(&s), CP_TYPE_BARRACKS, CP_PLACE_RING, [10, 10], 0, 3, &[], 0);
    assert_eq!(r.found, 1);
    assert!(can_build_at(&mut h, 1, CP_TYPE_BARRACKS, [r.x, r.y], 2, true, true, [10, 10]));
    // a find at (0, 0) is none, as is an unknown mode or no placement
    let r = next_place_search(&mut h, Some(&s), CP_TYPE_BARRACKS, 9, [10, 10], 0, 3, &[], 0);
    assert_eq!((r.found, r.x, r.y), (0, -1, -1));
    let r = next_place_search(&mut h, None, CP_TYPE_BARRACKS, CP_PLACE_RING, [10, 10], 0, 3, &[], 0);
    assert_eq!(r.found, 0);
}

#[test]
fn next_place_adjacent_prefers_the_touching_tile() {
    let mut h = Fake { town: [5, 5], ..Default::default() };
    let s = EeCpPlacement::default();
    let mine = h.add(0x60, [20, 20], false);
    let id = h.units[mine as usize - 1].id as i64;
    let r = next_place_search(&mut h, Some(&s), CP_TYPE_SETTLEMENT, CP_PLACE_ADJACENT, [20, 20], 0, 6, &[id], 0);
    assert_eq!(r.found, 1);
    // the nearest gap the margin allows, and of those the one nearest the town
    assert_eq!(r.gap, 1);
    assert!(distance_squared([r.x, r.y], [5, 5]) <= distance_squared([22, 22], [5, 5]));
}

#[test]
fn airport_end_and_wonder_rings() {
    let mut h = Fake::default();
    assert!(!is_near_airport_end(&mut h, [3, 3], 2, 2.0));
    h.town = [20, 20];
    let mut out = [0, 0];
    // the wonder's site: ring 1 round the town; every ring listed after the find
    assert!(find_wonder_site(&mut h, 0x200, 2, &mut out));
    assert_eq!(out, [20, 22]);
    let mut out = [0, 0];
    h.town = [CP_NO_TILE, CP_NO_TILE];
    assert!(!find_wonder_site(&mut h, 0x200, 2, &mut out));
}
