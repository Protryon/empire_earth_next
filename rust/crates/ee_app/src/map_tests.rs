//! map.rs over a small square map host: the bodies' calls and the values they leave.

use crate::map::{self, EeAppMapDims, MapHost, Terrain, TERRAIN_CLIFF};
use ee_base::random::URandom;
use ee_ge::types::GeVector;
use std::collections::BTreeMap;
use std::os::raw::{c_long, c_ulong};

const N: c_long = 6;

struct Mock {
    z: Vec<f32>,
    normals: Vec<GeVector>,
    texture: Vec<c_ulong>,
    terrain_type: Vec<c_long>,
    water: Vec<bool>,
    colors: BTreeMap<(c_ulong, c_ulong), c_ulong>,
    references: BTreeMap<(c_ulong, c_ulong), i32>,
    log: Vec<String>,
    near: Vec<(c_long, c_long)>,
    sees: bool,
}

impl Mock {
    fn new() -> Self {
        let points = ((N + 1) * (N + 1)) as usize;
        let tiles = (N * N) as usize;
        Mock {
            z: vec![1.0; points],
            normals: vec![GeVector { x: 0.0, y: 0.0, z: 1.0 }; points],
            texture: vec![0; tiles],
            terrain_type: vec![0; tiles],
            water: vec![true; tiles],
            colors: BTreeMap::new(),
            references: BTreeMap::new(),
            log: Vec::new(),
            near: Vec::new(),
            sees: true,
        }
    }
    fn p(x: c_ulong, y: c_ulong) -> usize {
        (y as usize) * (N as usize + 1) + x as usize
    }
    fn t(x: c_long, y: c_long) -> usize {
        (y * N + x) as usize
    }
}

fn dims() -> EeAppMapDims {
    EeAppMapDims { size: N, l195610: 3, l195614: 3, f1955f4: 0.5, base_terrain: 1, cliff_terrain: 0, players: 2 }
}

impl MapHost for Mock {
    fn tile(&mut self, x: c_long, y: c_long) -> bool {
        (x as c_ulong) < N as c_ulong && (y as c_ulong) < N as c_ulong
    }
    fn terrain_type(&mut self, x: c_long, y: c_long) -> c_long {
        self.terrain_type[Self::t(x, y)]
    }
    fn set_terrain_type(&mut self, x: c_long, y: c_long, terrain_type: c_long) {
        self.terrain_type[Self::t(x, y)] = terrain_type;
    }
    fn set_tile_terrain(&mut self, x: c_long, y: c_long, texture_set: c_long, unload: bool) {
        self.texture[Self::t(x, y)] = texture_set as c_ulong;
        self.log.push(format!("tex {x} {y} {texture_set} {unload}"));
    }
    fn terrain(&mut self, id: c_long) -> Terrain {
        // terrain i: texture set 10 + i, type 20 + i, colour type 30 + i
        Terrain { texture_set: 10 + id, terrain_type: 20 + id, color_type: 30 + id }
    }
    fn color_set(&mut self, color_type: c_long) -> c_ulong {
        (color_type + 100) as c_ulong
    }
    fn cliff(&mut self, _id: c_long) -> (c_long, c_long) {
        (2, 5)
    }
    fn tile_checksum(&mut self, x: c_long, y: c_long) -> c_long {
        x * 100 + y
    }
    fn z(&mut self, x: c_ulong, y: c_ulong) -> f32 {
        self.z[Self::p(x, y)]
    }
    fn normal(&mut self, x: c_ulong, y: c_ulong) -> GeVector {
        self.normals[Self::p(x, y)]
    }
    fn texture_set(&mut self, x: c_ulong, y: c_ulong) -> c_ulong {
        self.texture[Self::t(x as c_long, y as c_long)]
    }
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        x < N as c_ulong && y < N as c_ulong
    }
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool {
        x <= N as c_ulong && y <= N as c_ulong
    }
    fn water_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        self.water[Self::t(x as c_long, y as c_long)]
    }
    fn space_tile(&mut self, _x: c_ulong, _y: c_ulong) -> bool {
        false
    }
    fn set_color_set(&mut self, x: c_ulong, y: c_ulong, set: c_ulong) {
        self.colors.insert((x, y), set);
    }
    fn set_water_color_set(&mut self, x: c_ulong, y: c_ulong) {
        self.colors.insert((x, y), 999);
    }
    fn reference_tile(&mut self, x: c_ulong, y: c_ulong, on: bool) {
        *self.references.entry((x, y)).or_default() += if on { 1 } else { -1 };
    }
    fn sight_enabled(&mut self) -> bool {
        true
    }
    fn local_sees(&mut self, _shared: bool) -> bool {
        self.sees
    }
    fn tiles_near(&mut self, _x: f32, _y: f32, _radius: f32, out: &mut Vec<(c_long, c_long)>) {
        out.extend(self.near.iter().copied());
    }
    fn sight_at(&mut self, x: c_long, y: c_long, add: bool, shared: bool) {
        self.log.push(format!("sight {x} {y} {add} {shared}"));
    }
    fn count_cell(&mut self, x: c_long, y: c_long, on: bool) {
        self.log.push(format!("cell {x} {y} {on}"));
    }
}

#[test]
fn ftol_truncates_and_gives_the_indefinite_value_out_of_range() {
    assert_eq!(map::ftol(2.9), 2);
    assert_eq!(map::ftol(-2.9), -2);
    assert_eq!(map::ftol(f32::NAN), c_long::MIN);
    assert_eq!(map::ftol(1e30), c_long::MIN);
    assert_eq!(map::ftol(-1e30), c_long::MIN);
}

#[test]
fn tile_count_is_the_square_minus_the_corner_triangles() {
    let d = EeAppMapDims { size: 10, l195610: 4, l195614: 6, ..Default::default() };
    // 100 - 2 * (4 * 3 / 2 + 6 * 5 / 2) = 100 - 2 * 21
    assert_eq!(map::tile_count(&d), 58);
}

#[test]
fn update_water_types_the_four_tiles_and_colours_the_vertex() {
    let mut m = Mock::new();
    m.texture[Mock::t(2, 2)] = 3;
    m.texture[Mock::t(1, 1)] = 4;
    map::update_water(&mut m, &dims(), 2, 2);
    // each tile's type from the terrain its texture set indexes
    assert_eq!(m.terrain_type[Mock::t(2, 2)], 23);
    assert_eq!(m.terrain_type[Mock::t(1, 1)], 24);
    assert_eq!(m.terrain_type[Mock::t(1, 2)], 20);
    // above water: the colour set of the lower-left tile's terrain (texture 4 -> colour type 34 -> set 134)
    assert_eq!(m.colors[&(2, 2)], 134);
    m.z[Mock::p(2, 2)] = -0.5;
    map::update_water(&mut m, &dims(), 2, 2);
    assert_eq!(m.colors[&(2, 2)], 999);
}

#[test]
fn set_cliff_color_sets_skips_the_outer_corners_and_the_water() {
    let mut m = Mock::new();
    m.z[Mock::p(2, 3)] = -1.0; // below -f1955f4: kept
    map::set_cliff_color_sets(&mut m, &dims(), 1, 2, 3, 2, 7);
    let mut got: Vec<_> = m.colors.keys().copied().collect();
    got.sort();
    // the bottom row: its left tile only corner 1, the others corners 0 and 1; the top row's left tile corners 1, 3
    assert_eq!(got, vec![(2, 3), (2, 4), (3, 3), (3, 4)].into_iter().filter(|&k| k != (2, 3)).collect::<Vec<_>>());
    assert!(m.colors.values().all(|&s| s == 107));
}

#[test]
fn paint_terrain_types_the_tile_and_paints_its_lower_left_neighbours() {
    let mut m = Mock::new();
    map::paint_terrain(&mut m, &dims(), 3, 3, 2);
    assert_eq!(m.terrain_type[Mock::t(3, 3)], 22);
    assert_eq!(m.log, vec!["tex 2 2 12 true", "tex 2 3 12 true", "tex 3 2 12 true", "tex 3 3 12 true"]);
    // off the map: nothing
    m.log.clear();
    map::paint_terrain(&mut m, &dims(), -1, 3, 2);
    assert!(m.log.is_empty());
}

#[test]
fn paint_terrain_rect_paints_top_down_then_colours() {
    let mut m = Mock::new();
    map::paint_terrain_rect(&mut m, &dims(), 0, 1, 1, 0, 3, 4);
    assert_eq!(m.log, vec!["tex 0 1 13 true", "tex 1 1 13 true", "tex 0 0 13 true", "tex 1 0 13 true"]);
    assert!(m.terrain_type[..2].iter().all(|&t| t == 23));
    assert!(m.colors.values().all(|&s| s == 104));
}

#[test]
fn update_cliffs_makes_steep_tiles_cliffs_with_the_random_table() {
    let mut m = Mock::new();
    // tile (2, 2) steep: its corner (3, 3) high
    m.z[Mock::p(3, 3)] = 3.0;
    map::update_cliffs(&mut m, &dims(), 0, N, 0, N);
    assert_eq!(m.terrain_type[Mock::t(2, 2)], TERRAIN_CLIFF);
    // flat normals face neither slope: the table's entries, drawn from seed 0
    let mut r = URandom { seed: 0 };
    let table: Vec<c_long> = (0..32 * 32).map(|_| r.generate_int(0, 3) as c_long + 2).collect();
    let expect = |tx: usize, ty: usize| 10 + table[tx * 32 + ty];
    assert_eq!(m.texture[Mock::t(1, 1)] as c_long, expect(1, 1));
    assert_eq!(m.texture[Mock::t(2, 1)] as c_long, expect(2, 1));
    assert_eq!(m.texture[Mock::t(2, 2)] as c_long, expect(2, 2));
    // a normal facing +x takes the last cliff terrain
    let mut m = Mock::new();
    m.z[Mock::p(3, 3)] = 3.0;
    let facing = {
        let mut v = GeVector { x: 1.0, y: 0.0, z: 1.0 };
        v.normalize();
        v
    };
    for p in m.normals.iter_mut() {
        *p = facing;
    }
    map::update_cliffs(&mut m, &dims(), 0, N, 0, N);
    assert_eq!(m.texture[Mock::t(1, 1)], 15);
}

#[test]
fn update_cliffs_counts_cliff_neighbours() {
    let mut m = Mock::new();
    // three of tile (1, 1)'s lower-left neighbours on a cliff texture (12..=15)
    for (x, y) in [(0, 0), (1, 0), (0, 1)] {
        m.texture[Mock::t(x, y)] = 13;
    }
    map::update_cliffs(&mut m, &dims(), 1, 2, 1, 2);
    assert_eq!(m.terrain_type[Mock::t(1, 1)], TERRAIN_CLIFF);
    // an edge tile: its missing neighbours count (no space map)
    let mut m = Mock::new();
    map::update_cliffs(&mut m, &dims(), 0, 1, 0, 1);
    assert_eq!(m.terrain_type[Mock::t(0, 0)], TERRAIN_CLIFF);
}

#[test]
fn remove_flat_cliffs_repaints_the_base_terrain() {
    let mut m = Mock::new();
    m.terrain_type[Mock::t(2, 2)] = TERRAIN_CLIFF;
    m.terrain_type[Mock::t(3, 3)] = TERRAIN_CLIFF;
    m.z[Mock::p(4, 4)] = 5.0; // (3, 3) stays steep
    map::remove_flat_cliffs(&mut m, &dims(), 0, N as c_ulong, 0, N as c_ulong);
    assert_eq!(m.terrain_type[Mock::t(2, 2)], 21);
    assert_eq!(m.terrain_type[Mock::t(3, 3)], TERRAIN_CLIFF);
}

#[test]
fn checksum_sums_a_window_and_advances_the_cursor() {
    let mut m = Mock::new();
    let d = dims();
    let mut cursor = 0;
    // one window covers the whole 6 x 6 map: each tile x * 100 + y, its heights 1000 + 3 * 1000 (the scale's minus)
    let sum = map::compute_checksum(&mut m, &d, &mut cursor);
    let tiles: c_long = (0..N).flat_map(|x| (0..N).map(move |y| x * 100 + y)).sum();
    assert_eq!(sum, tiles + 36 * 4000);
    assert_eq!(cursor, 0); // the next band starts past the map: back to 0
    let d = EeAppMapDims { size: 100, ..dims() };
    let mut cursor = 10;
    let mut e = Mock::new();
    // every tile of these windows off the mock's map: only the cursor moves (to 10 + 64, inside the row)
    map::compute_checksum(&mut e, &d, &mut cursor);
    assert_eq!(cursor, 74);
    map::compute_checksum(&mut e, &d, &mut cursor);
    assert_eq!(cursor, 64 * 100);
}

#[test]
fn sight_reaches_only_flagged_tiles_and_counts_the_local_players() {
    let mut m = Mock::new();
    m.near = vec![(1, 1), (2, 2), (9, 9)];
    m.water[Mock::t(2, 2)] = false;
    map::sight(&mut m, 1.0, 1.0, 2.0, true, true);
    assert_eq!(m.log, vec!["sight 1 1 true true", "cell 1 1 true"]);
    assert_eq!(m.references[&(1, 1)], 1);
    m.log.clear();
    m.sees = false;
    map::sight(&mut m, 1.0, 1.0, 2.0, false, false);
    assert_eq!(m.log, vec!["sight 1 1 false false"]);
    assert_eq!(m.references[&(1, 1)], 1);
}
