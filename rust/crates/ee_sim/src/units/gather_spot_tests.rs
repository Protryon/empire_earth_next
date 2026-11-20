//! gather_spot.rs: the spot search over a made-up world, the terrain flags over a made-up class host.

use super::*;

/// A world of sub sectors by tile: `subs[(x, y)]`, the ones with room for the gatherer, the heights.
#[derive(Default)]
struct World {
    tiles: Vec<[i32; 2]>,
    unit: ([f32; 2], [i32; 2]),
    start: u64,
    subs: Vec<([i32; 2], u64)>,
    neighbors: Vec<(u64, u64)>,
    kinds: Vec<(u64, i64)>,
    flags: [bool; 17],
    room: Vec<[i32; 2]>,
    no_tile: Vec<[i32; 2]>,
    flag_reads: u32,
    placed: Vec<[i32; 2]>,
}

impl SpotWorld for World {
    fn resource_tiles(&mut self, _resource: u64) -> Vec<[i32; 2]> {
        self.tiles.clone()
    }
    fn unit_place(&mut self, _unit: u64) -> ([f32; 2], [i32; 2]) {
        self.unit
    }
    fn resource_sub_sector(&mut self, _resource: u64) -> u64 {
        self.start
    }
    fn sub_sector_at(&mut self, x: i32, y: i32) -> u64 {
        self.subs.iter().find(|(t, _)| *t == [x, y]).map_or(0, |s| s.1)
    }
    fn is_neighbor(&mut self, start: u64, other: u64) -> bool {
        self.neighbors.contains(&(start, other))
    }
    fn kind(&mut self, sub: u64) -> i64 {
        self.kinds.iter().find(|k| k.0 == sub).map_or(0, |k| k.1)
    }
    fn terrain_flags(&mut self, _unit: u64) -> [bool; 17] {
        self.flag_reads += 1;
        self.flags
    }
    fn tile_height(&mut self, x: i32, y: i32) -> Option<f32> {
        (!self.no_tile.contains(&[x, y])).then_some(x as f32 * 0.25)
    }
    fn place_on_tile(&mut self, _unit: u64, x: i32, y: i32, io: &mut [f32; 3]) -> bool {
        self.placed.push([x, y]);
        if self.room.contains(&[x, y]) {
            io[0] = x as f32 + 0.5;
            io[1] = y as f32 + 0.5;
            true
        } else {
            false
        }
    }
}

#[test]
fn the_nearest_tile_in_the_resources_sub_sector_wins() {
    let mut w = World {
        tiles: vec![[10, 10], [2, 2], [5, 5]],
        unit: ([0.0, 0.0], [0, 0]),
        start: 7,
        subs: vec![([10, 10], 7), ([2, 2], 7), ([5, 5], 7)],
        room: vec![[2, 2], [5, 5], [10, 10]],
        ..Default::default()
    };
    assert_eq!(find_spot(&mut w, 1, 2), Some([2.5, 2.5, 0.5]));
    assert_eq!(w.placed, vec![[2, 2]]);
}

#[test]
fn a_full_tile_falls_through_to_the_next_and_others_last() {
    let mut w = World {
        tiles: vec![[1, 1], [3, 3], [4, 4]],
        unit: ([0.0, 0.0], [9, 9]),
        start: 7,
        // (1,1) another, unreachable sub sector; (3,3) the start's but full; (4,4) a neighbour of a passable kind
        subs: vec![([1, 1], 8), ([3, 3], 7), ([4, 4], 9)],
        neighbors: vec![(7, 9)],
        kinds: vec![(9, 2)],
        flags: {
            let mut f = [false; 17];
            f[2] = true;
            f
        },
        room: vec![[1, 1], [4, 4]],
        ..Default::default()
    };
    assert_eq!(find_spot(&mut w, 1, 2), Some([4.5, 4.5, 1.0]));
    assert_eq!(w.flag_reads, 1, "the flags are read for the neighbour only");
    // only the nearest tile of another sub sector: the second pass
    w.room = vec![[1, 1]];
    w.placed.clear();
    assert_eq!(find_spot(&mut w, 1, 2), Some([1.5, 1.5, 0.25]));
    assert_eq!(w.placed, vec![[3, 3], [4, 4], [1, 1]]);
    assert_eq!(w.flag_reads, 2, "once a search");
}

#[test]
fn the_gatherers_own_tile_is_taken_and_a_missing_map_tile_is_not() {
    let mut w = World {
        tiles: vec![[6, 6]],
        unit: ([6.0, 6.0], [6, 6]),
        start: 7,
        room: vec![[6, 6]],
        no_tile: vec![[6, 6]],
        ..Default::default()
    };
    assert_eq!(find_spot(&mut w, 1, 2), None);
    assert!(w.placed.is_empty());
    w.no_tile.clear();
    assert_eq!(find_spot(&mut w, 1, 2), Some([6.5, 6.5, 1.5]));
}

#[test]
fn accessibility_by_the_gatherers_category() {
    let mut yes = || true;
    let mut no = || false;
    assert!(accessible(CATEGORY_LAND_CITIZEN, Some(0), &mut yes, &mut || true));
    assert!(!accessible(CATEGORY_LAND_CITIZEN, Some(TERRAIN_DEEP_WATER), &mut yes, &mut || true));
    assert!(accessible(CATEGORY_NAVAL_CITIZEN, Some(TERRAIN_DEEP_WATER), &mut || true, &mut || true));
    assert!(!accessible(CATEGORY_NAVAL_CITIZEN, Some(0), &mut || true, &mut || true));
    assert!(!accessible(CATEGORY_LAND_CITIZEN, None, &mut || true, &mut || true));
    assert!(!accessible(9, Some(0), &mut || true, &mut || true));
    let mut asked = false;
    assert!(!accessible(CATEGORY_LAND_CITIZEN, Some(0), &mut no, &mut || {
        asked = true;
        true
    }));
    assert!(!asked, "the gather point is asked only in the same continent");
}

#[test]
fn the_flags_of_a_land_unit_a_ship_and_an_aircraft() {
    let mut asked = Vec::new();
    let land = TerrainClass { domains: 2, water_kind: 0, can_cross_forest: false, unit_type: 5 };
    let f = terrain_flags(
        land,
        1,
        1 << 14,
        &mut |t| {
            asked.push(t);
            [0, 9].contains(&t)
        },
        &mut |_| false,
    );
    assert!(f[TILE_LAND] && f[TILE_CLIFF] && f[TILE_FOREST] && !f[TILE_WATER] && !f[TILE_SPACE] && !f[TILE_GATE]);
    assert_eq!(asked, vec![0xa, 0, 9, 1, 1, 0xc], "the original's order, the water test twice");
    let ship = TerrainClass { domains: 4, water_kind: 1, can_cross_forest: true, unit_type: 5 };
    let f = terrain_flags(ship, 0, 0, &mut |t| [1, 0xa].contains(&t), &mut |ty| ty == 5);
    assert!(f[TILE_WATER] && f[TILE_SHALLOWS] && !f[TILE_SHORE] && !f[TILE_SHALLOWS_SHORE] && f[TILE_NEAR_SHORE]);
    assert!(f[TILE_SHALLOWS_NEAR_SHORE] && f[TILE_FOREST] && f[TILE_GATE]);
    let air = TerrainClass { domains: 1, ..Default::default() };
    let f = terrain_flags(air, 1, 0, &mut |_| false, &mut |_| unreachable!());
    assert!(f[..TILE_SPACE].iter().all(|&b| b) && !f[TILE_SPACE]);
}
