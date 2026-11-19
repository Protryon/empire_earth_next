//! The town over a fake world: buildings by address, the tile set and hull as a list.

use std::collections::BTreeMap;

use super::herd::{ee_cp_herd_push, EeHerdIo};
use super::town::*;

const PLAYER: u64 = 0x7000;

#[derive(Default)]
struct World {
    units: BTreeMap<u64, EeTownUnit>,
    tiles: Vec<(i64, i64)>,
    patrol: bool,
    patrol_points: usize,
    created: usize,
    /// the connected continents: (id, category, the closest points' squared distance)
    continents: Vec<(i64, i64, i64)>,
    closest_asked: usize,
}

impl World {
    fn building(&mut self, p: u64, x: i32, y: i32, influence: i32, unit_type: i32, wall: bool) {
        let u = EeTownUnit {
            influence: influence as _,
            wall,
            unit_type: unit_type as _,
            area_x: x as _,
            area_y: y as _,
            player: PLAYER,
            ..Default::default()
        };
        self.units.insert(p, u);
    }
    fn push(io: &TownIo, points: &[(i64, i64)]) {
        for &(x, y) in points {
            // SAFETY: the operation's list
            unsafe { ee_cp_herd_push(io.list, x, y, 0, 0) };
        }
    }
}

impl TownHost for World {
    fn call(&mut self, op: i32, a: u64, _b: u64, io: &mut TownIo) -> i64 {
        let t = (io.v[0], io.v[1]);
        match op {
            EE_CP_TOWN_UNIT => {
                let u = self.units[&a];
                io.v[..7].copy_from_slice(&[
                    i64::from(u.influence),
                    i64::from(u.wonder),
                    i64::from(u.wall),
                    i64::from(u.unit_type),
                    i64::from(u.area_x),
                    i64::from(u.area_y),
                    u.player as i64,
                ]);
                0
            }
            EE_CP_TOWN_IS_TOWN_UNIT => 1,
            EE_CP_TOWN_PLAYER => PLAYER as i64,
            EE_CP_TOWN_NUM_TILES => self.tiles.len() as i64,
            // the hull: every tile (the fake's)
            EE_CP_TOWN_HULL_CONTAINS | EE_CP_TOWN_TILE_CONTAINS => i64::from(self.tiles.contains(&t)),
            EE_CP_TOWN_HULL_POINTS | EE_CP_TOWN_TILES => {
                World::push(io, &self.tiles);
                0
            }
            EE_CP_TOWN_TILE_INSERT => {
                if !self.tiles.contains(&t) {
                    self.tiles.push(t);
                }
                0
            }
            EE_CP_TOWN_TILE_REMOVE => {
                self.tiles.retain(|&p| p != t);
                0
            }
            EE_CP_TOWN_CREATE | EE_CP_TOWN_SET_HULL => {
                self.created += 1;
                0
            }
            EE_CP_TOWN_CLOSEST_HULL => {
                let c = *self.tiles.first().unwrap_or(&(0, 0));
                io.v[0] = c.0;
                io.v[1] = c.1;
                0
            }
            EE_CP_TOWN_CONTINENT_AT => 1,
            EE_CP_TOWN_CONNECTED => {
                let ids: Vec<(i64, i64)> = self.continents.iter().map(|c| (c.0, 0)).collect();
                World::push(io, &ids);
                0
            }
            EE_CP_TOWN_CONTINENT => {
                let Some(&(_, category, d)) = self.continents.iter().find(|c| c.0 == io.v[0]) else { return -1 };
                if io.v[1] != 0 {
                    self.closest_asked += 1;
                    io.v[2..7].copy_from_slice(&[1, 0, 0, d, 0]);
                }
                category
            }
            EE_CP_TOWN_HAS_PATROL => i64::from(self.patrol),
            EE_CP_TOWN_NEW_PATROL => {
                self.patrol = true;
                0
            }
            EE_CP_TOWN_SET_PATROL => {
                self.patrol_points = io.input_len;
                0
            }
            EE_CP_TOWN_DROP_PATROL => {
                self.patrol = false;
                0
            }
            _ => 0,
        }
    }
    fn area(&mut self, _op: i32, _v: &[i64]) -> (i64, EeHerdIo) {
        (0, EeHerdIo::default())
    }
}

#[test]
fn buildings_count_and_walls_count_a_tenth() {
    let mut w = World::default();
    w.building(0x100, 10, 10, 50, 0x24, false);
    w.building(0x200, 11, 10, 25, 0x30, true);
    w.building(0x300, 40, 40, 10, 0x30, false);
    let mut f = EeTownFields::default();
    // SAFETY: f outlives the town value
    let mut t = unsafe { Town::new(&mut w, &mut f) };
    t.reset();
    assert!(t.add_unit(0x100));
    assert!(t.add_unit(0x200));
    // too far from the hull for CanTake (the reach is 256 squared tiles; AddUnit itself takes any once there are tiles)
    assert!(!t.can_take(0x300));
    assert_eq!((f.mNumUnits, f.mNumDocks, f.mNumWalls, f.mL80, f.mValue), (2, 1, 1, 1, 52));
    assert_eq!((f.mContinent, f.mReach), (1, 0x100));
    // the second tile made a hull of two points: a patrol group with them
    assert!(w.patrol);
    assert_eq!(w.patrol_points, 2);
}

#[test]
fn a_removal_counts_down_to_zero_at_least() {
    let mut w = World::default();
    w.building(0x100, 10, 10, 50, 0x42, false);
    let mut f = EeTownFields::default();
    // SAFETY: f outlives the town value
    let mut t = unsafe { Town::new(&mut w, &mut f) };
    t.reset();
    t.add_unit(0x100);
    assert_eq!(f.mNumAirports, 1);
    // SAFETY: as above
    let mut t = unsafe { Town::new(&mut w, &mut f) };
    assert!(t.remove_unit(0x100));
    // an empty town takes any building: its tile is not there, the counters stay at 0
    assert!(!t.remove_unit(0x100));
    assert_eq!((f.mNumAirports, f.mNumUnits, f.mL80, f.mValue), (0, 0, 0, 0));
    assert!(w.tiles.is_empty());
}

#[test]
fn a_merge_adds_the_counters_and_tiles() {
    let mut w = World::default();
    let mut f = EeTownFields { mNumUnits: 2, mValue: 10, ..Default::default() };
    let o = EeTownFields { mNumUnits: 3, mValue: 5, mNumDocks: 1, ..Default::default() };
    w.tiles = vec![(1, 1), (2, 2)];
    // SAFETY: f outlives the town value
    unsafe { Town::new(&mut w, &mut f) }.merge(0x900, &o);
    assert_eq!((f.mNumUnits, f.mValue, f.mNumDocks), (5, 15, 1));
}

#[test]
fn near_water_asks_the_water_and_space_continents_only() {
    // from the last: a cliff (3: not asked), a space continent 9 tiles off, a water continent 11 tiles off
    let mut w = World { tiles: vec![(1, 1)], continents: vec![(2, 2, 11), (3, 4, 9), (4, 3, 1)], ..Default::default() };
    let mut f = EeTownFields { mContinent: 1, ..Default::default() };
    // SAFETY: f outlives the town value
    assert!(!unsafe { Town::new(&mut w, &mut f) }.update_near_water());
    assert_eq!((f.mNearWater, f.mNearMapEdge, w.closest_asked), (false, true, 2));
}
