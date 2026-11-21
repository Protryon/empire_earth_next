use super::*;
use std::collections::BTreeMap;

/// A small world: tiles (x, y) -> their doppelgangers; doppelgangers -> (player, unit, unit type, tile size);
/// everything else answered from `answers` keyed by (op, a, b) (0 else); every statement recorded.
#[derive(Default)]
struct World {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64, i64), i64>,
    /// tile handle -> its doppelgangers
    tiles: BTreeMap<u64, Vec<u64>>,
    /// doppelganger -> (player, unit, unit type, tile size)
    dops: BTreeMap<u64, (u64, u64, i64, i64)>,
    freed: Vec<u64>,
    next: u64,
}

/// tile (x, y)'s handle
fn tile(x: i64, y: i64) -> u64 {
    1000 + (x as u64) * 100 + y as u64
}

impl DmHost for World {
    fn dm(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.log.push((op, a, b, c));
        match op {
            EE_UNITS_DM_MAP_TILE => {
                let t = tile(b, c);
                if self.tiles.contains_key(&t) {
                    t as i64
                } else {
                    0
                }
            }
            EE_UNITS_DM_NUM_DOPPELGANGERS => self.tiles[&a].len() as i64,
            EE_UNITS_DM_DOPPELGANGER_AT => self.tiles[&a][b as usize] as i64,
            EE_UNITS_DM_DOPPELGANGER_PLAYER => self.dops[&a].0 as i64,
            EE_UNITS_DM_DOPPELGANGER_UNIT => self.dops[&a].1 as i64,
            EE_UNITS_DM_DOPPELGANGER_UNIT_TYPE => self.dops[&a].2,
            EE_UNITS_DM_DOPPELGANGER_TILE_SIZE => self.dops[&a].3,
            EE_UNITS_DM_TILE_ADD => {
                self.tiles.get_mut(&a).unwrap().push(b as u64);
                0
            }
            EE_UNITS_DM_TILE_REMOVE => {
                self.tiles.get_mut(&a).unwrap().retain(|d| *d != b as u64);
                0
            }
            EE_UNITS_DM_NEW => {
                self.next += 1;
                let d = 500 + self.next;
                self.dops.insert(d, (0, 0, 0, 1));
                d as i64
            }
            EE_UNITS_DM_INITIALIZE => {
                let size = *self.answers.get(&(EE_UNITS_DM_UNIT_TILE_SIZE, b as u64, 0)).unwrap_or(&1);
                self.dops.insert(a, (c as u64, b as u64, 0, size));
                0
            }
            EE_UNITS_DM_POOL_DELETE | EE_UNITS_DM_COMPLEX_POOL_DELETE => {
                self.freed.push(a);
                0
            }
            _ => *self.answers.get(&(op, a, b)).unwrap_or(&0),
        }
    }
}

impl World {
    fn with_tiles(n: i64) -> Self {
        let mut w = World::default();
        for x in 0..n {
            for y in 0..n {
                w.tiles.insert(tile(x, y), Vec::new());
            }
        }
        w
    }
    fn answer(mut self, op: i32, a: u64, b: i64, value: i64) -> Self {
        self.answers.insert((op, a, b), value);
        self
    }
}

const PLAYER: u64 = 7;

#[test]
fn a_doppelganger_covers_its_square_and_an_airports_runways() {
    let mut w = World::with_tiles(12);
    w.dops.insert(600, (PLAYER, 40, UNIT_TYPE_AIRPORT, 2));
    w.answers.insert((EE_UNITS_DM_DOPPELGANGER_FACING, 600, 0), 0);
    tiles(&mut w, 600, 1, 1, true, true);
    // 2 x 2 at (1, 1), then 3 runway squares up y
    let covered = w.tiles.values().filter(|v| v.contains(&600)).count();
    assert_eq!(covered, 16);
    assert!(w.tiles[&tile(2, 8)].contains(&600));
    tiles(&mut w, 600, 1, 1, true, false);
    assert_eq!(w.tiles.values().filter(|v| v.contains(&600)).count(), 0);
}

#[test]
fn the_finds_match_player_and_unit() {
    let mut w = World::with_tiles(2);
    w.dops.insert(600, (PLAYER, 40, 0, 1));
    w.tiles.get_mut(&tile(0, 0)).unwrap().push(600);
    w.answers.insert((EE_UNITS_DM_PLAYER_AT, 0, 3), PLAYER as i64);
    assert!(has_doppelganger_of(&mut w, tile(0, 0), 40, 3));
    assert!(!has_doppelganger_of(&mut w, tile(0, 0), 41, 3));
    assert!(!has_doppelganger_of(&mut w, 0, 40, 3));
    assert!(has_doppelganger(&mut w, tile(0, 0), 40));
    assert_eq!(get_doppelganger(&mut w, tile(0, 0), 40, 3), 600);
    assert_eq!(get_doppelganger(&mut w, tile(0, 0), 40, 4), 0);
}

#[test]
fn a_shown_tile_frees_the_players_doppelgangers_only() {
    let mut w =
        World::with_tiles(3).answer(EE_UNITS_DM_IS_CURRENT, PLAYER, 0, 1).answer(EE_UNITS_DM_IS_COMPLEX, 601, 0, 1);
    w.dops.insert(600, (PLAYER, 40, 0, 1));
    w.dops.insert(601, (PLAYER, 41, 0, 1));
    w.dops.insert(602, (8, 42, 0, 1));
    w.tiles.get_mut(&tile(1, 1)).unwrap().extend([600, 602, 601]);
    tile_shown(&mut w, tile(1, 1), 1, 1, PLAYER);
    assert_eq!(w.tiles[&tile(1, 1)], [602]);
    assert_eq!(w.freed, [600, 601]);
    assert_eq!(w.log.iter().filter(|e| e.0 == EE_UNITS_DM_MINIMAP_REMOVE).count(), 2);
    assert!(w.log.iter().any(|e| e.0 == EE_UNITS_DM_COMPLEX_CLEAR && e.1 == 601));
}

#[test]
fn a_hidden_tile_remembers_a_resource_out_of_sight() {
    let unit = 40;
    let t = tile(1, 1);
    let mut w = World::with_tiles(3)
        .answer(EE_UNITS_DM_PLAYER_ID, PLAYER, 0, 2)
        .answer(EE_UNITS_DM_TILE_FIRST_NODE, t, 0, 90)
        .answer(EE_UNITS_DM_NODE_UNIT, 90, 0, unit as i64)
        .answer(EE_UNITS_DM_CATEGORY, unit, 0, CATEGORY_RESOURCE)
        .answer(EE_UNITS_DM_UNIT_TILE, unit, 0, t as i64)
        .answer(EE_UNITS_DM_HAS_SCALE, unit, 0, 1)
        .answer(EE_UNITS_DM_UNIT_TILE_XY, unit, 0, 1)
        .answer(EE_UNITS_DM_UNIT_TILE_XY, unit, 1, 1)
        .answer(EE_UNITS_DM_IS_CURRENT, PLAYER, 0, 1);
    tile_hidden(&mut w, t, PLAYER);
    assert_eq!(w.tiles[&t].len(), 1);
    let d = w.tiles[&t][0];
    assert_eq!(w.dops[&d].0, PLAYER);
    assert!(w.log.iter().any(|e| e.0 == EE_UNITS_DM_MINIMAP_ADD && e.1 == d));
    // seen (visibility 2): nothing
    let mut w2 = World::with_tiles(3)
        .answer(EE_UNITS_DM_PLAYER_ID, PLAYER, 0, 2)
        .answer(EE_UNITS_DM_TILE_FIRST_NODE, t, 0, 90)
        .answer(EE_UNITS_DM_TILE_VISIBILITY, t, 2, 2);
    tile_hidden(&mut w2, t, PLAYER);
    assert!(!w2.log.iter().any(|e| e.0 == EE_UNITS_DM_NODE_UNIT));
}

#[test]
fn a_cloaked_or_wonder_unit_is_never_out_of_sight() {
    let unit = 40;
    let mut w = World::with_tiles(3).answer(EE_UNITS_DM_UNIT_COMPLEX, unit, 0, unit as i64).answer(
        EE_UNITS_DM_IS_CLOAKED,
        unit,
        0,
        1,
    );
    assert!(!is_out_of_sight(&mut w, unit, PLAYER));
    let mut w = World::with_tiles(3);
    assert!(is_out_of_sight(&mut w, unit, PLAYER));
    // a tile off the map: not out of sight
    let mut w = World::with_tiles(3).answer(EE_UNITS_DM_UNIT_TILE_XY, unit, 0, 5);
    assert!(!is_out_of_sight(&mut w, unit, PLAYER));
}

#[test]
fn an_old_record_is_skipped_by_its_eras_reads() {
    let mut w = World::default();
    assert_eq!(load(&mut w, 9, 0, 0x20), 0);
    let reads: Vec<i32> = w.log.iter().skip(1).map(|e| e.0).collect();
    assert_eq!(reads.len(), 6);
    let mut w = World::default();
    assert_eq!(load(&mut w, 9, 0, 0x80), 0);
    assert_eq!(w.log.len() - 1, 7);
    // a current record: a new doppelganger, loaded and queued
    let mut w = World::default().answer(EE_UNITS_DM_FLAG, 0, 0, 1);
    let d = load(&mut w, 9, 0, 0x118);
    assert_eq!(d, 501);
    let ops: Vec<i32> = w.log.iter().map(|e| e.0).collect();
    assert_eq!(&ops[ops.len() - 2..], [EE_UNITS_DM_LOAD, EE_UNITS_DM_RESTORABLE]);
    assert!(w.log.iter().any(|e| e.0 == EE_UNITS_DM_NEW && e.2 == 1));
}

#[test]
fn unregistering_frees_the_players_doppelgangers_everywhere() {
    let mut w = World::with_tiles(2).answer(EE_UNITS_DM_HAS_MAP, 0, 0, 1).answer(EE_UNITS_DM_MAP_SIZE, 0, 0, 2);
    w.dops.insert(600, (PLAYER, 40, 0, 1));
    w.dops.insert(601, (8, 41, 0, 1));
    w.tiles.get_mut(&tile(1, 0)).unwrap().extend([600, 601]);
    unregister_player(&mut w, PLAYER);
    assert_eq!(w.tiles[&tile(1, 0)], [601]);
    assert_eq!(w.freed, [600]);
}
