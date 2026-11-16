// c_ulong is u32 on Windows, u64 on LP64: the casts are needed on one of them
#![allow(clippy::unnecessary_cast)]

use super::*;
use crate::map::Ring;
use std::ffi::c_ulong;

/// A grid the way the C++ holds one: rows of cells, and the row pointers the state carries.
struct Owned<T> {
    rows: Vec<Vec<T>>,
    ptrs: Vec<*mut T>,
}

impl<T: Copy> Owned<T> {
    fn new(n: u32, fill: T) -> Self {
        let mut rows = vec![vec![fill; n as usize]; n as usize];
        let ptrs = rows.iter_mut().map(|r| r.as_mut_ptr()).collect();
        Owned { rows, ptrs }
    }

    fn get(&self, x: u32, y: u32) -> T {
        self.rows[x as usize][y as usize]
    }

    fn set(&mut self, x: u32, y: u32, v: T) {
        self.rows[x as usize][y as usize] = v;
    }
}

/// The state's grids and lists for a `size` x `size` generator grid (rows of `size + 1`).
struct Fixture {
    heights: Owned<f32>,
    tiles: Owned<c_ulong>,
    claimed: Owned<u8>,
    valid: Owned<u8>,
    players: Vec<EeRmapGamePlayer>,
    features: Vec<Feature>,
    walls: Vec<EeRmapGameWall>,
    unit_names: Vec<EeRmapGameUnitName>,
    terrains: Vec<EeRmapGameTerrain>,
    regions: Vec<EeRmapGameRegion>,
    objects: Vec<Object>,
    forests: Vec<EeRmapGameForest>,
    forest_tree_types: Vec<u32>,
    animals: Vec<u32>,
    state: EeRmapGameState,
}

impl Fixture {
    fn new(size: u32) -> Self {
        let n = size + 1;
        let mut valid = Owned::new(n, 1u8);
        // the last row and column hold mesh points, not tiles: keep them valid as the game's points are
        valid.set(0, 0, 1);
        Fixture {
            heights: Owned::new(n, 0.5),
            tiles: Owned::new(n, 0),
            claimed: Owned::new(n, 0),
            valid,
            players: Vec::new(),
            features: Vec::new(),
            walls: Vec::new(),
            unit_names: Vec::new(),
            terrains: Vec::new(),
            regions: Vec::new(),
            objects: Vec::new(),
            forests: Vec::new(),
            forest_tree_types: Vec::new(),
            animals: Vec::new(),
            state: EeRmapGameState {
                grid_n: n,
                heights: std::ptr::null(),
                tiles: std::ptr::null(),
                claimed_tiles: std::ptr::null(),
                valid_tiles: std::ptr::null(),
                grid_size: size,
                maximum_y: size - 1,
                map_characteristics: 0,
                is_space_map: 0,
                half_map_size: size as f32 * 0.5,
                water_border: 0,
                ring_center: Point::new(size as f32 * 0.5, size as f32 * 0.5),
                flat_area: Rect { left: -1.0, bottom: -1.0, right: 1.0, top: 1.0 },
                min_int_elevation: -10,
                max_int_elevation: 10,
                height_map_smoothness: 0.5,
                terrain_chaos: 0.5,
                seed: 7,
                num_terrain_bands: 1,
                terrain_band_offset: 0,
                max_loop_count: 50,
                tile_unit: 1.0,
                space_terrain: 99,
                random_seed: 11,
                random_integers_seed: 13,
                resource_terrain_texture: [10, 11, 12, 13, -1, -1, 16],
                players: std::ptr::null(),
                players_len: 0,
                features: std::ptr::null(),
                features_len: 0,
                walls: std::ptr::null(),
                walls_len: 0,
                unit_names: std::ptr::null(),
                unit_names_len: 0,
                terrains: std::ptr::null(),
                terrains_len: 0,
                terrain_regions: std::ptr::null(),
                terrain_regions_len: 0,
                objects: std::ptr::null(),
                objects_len: 0,
                forests: std::ptr::null(),
                forests_len: 0,
                forest_tree_types: std::ptr::null(),
                forest_tree_types_len: 0,
                available_animals: std::ptr::null(),
                available_animals_len: 0,
            },
        }
    }

    /// Points the state at the fixture's buffers (after the lists are filled).
    fn bind(&mut self) -> &mut EeRmapGameState {
        let s = &mut self.state;
        s.heights = self.heights.ptrs.as_ptr();
        s.tiles = self.tiles.ptrs.as_ptr();
        s.claimed_tiles = self.claimed.ptrs.as_ptr();
        s.valid_tiles = self.valid.ptrs.as_ptr();
        s.players = self.players.as_ptr();
        s.players_len = self.players.len() as u32;
        s.features = self.features.as_ptr();
        s.features_len = self.features.len() as u32;
        s.walls = self.walls.as_ptr();
        s.walls_len = self.walls.len() as u32;
        s.unit_names = self.unit_names.as_ptr();
        s.unit_names_len = self.unit_names.len() as u32;
        s.terrains = self.terrains.as_ptr();
        s.terrains_len = self.terrains.len() as u32;
        s.terrain_regions = self.regions.as_ptr();
        s.terrain_regions_len = self.regions.len() as u32;
        s.objects = self.objects.as_ptr();
        s.objects_len = self.objects.len() as u32;
        s.forests = self.forests.as_ptr();
        s.forests_len = self.forests.len() as u32;
        s.forest_tree_types = self.forest_tree_types.as_ptr();
        s.forest_tree_types_len = self.forest_tree_types.len() as u32;
        s.available_animals = self.animals.as_ptr();
        s.available_animals_len = self.animals.len() as u32;
        s
    }

    fn run<R>(&mut self, host: &mut Fake, pass: impl FnOnce(&mut Game, &mut Fake) -> R) -> R {
        let s: *const EeRmapGameState = self.bind();
        // SAFETY: the fixture's buffers outlive the call and nothing else touches them meanwhile
        let mut game = unsafe { Game::new(&*s) };
        let r = pass(&mut game, host);
        self.state.random_seed = game.random.seed;
        self.state.random_integers_seed = game.random_integers.seed;
        r
    }
}

/// A map of `size` x `size` tiles (all of terrain 0, height 1) and its mesh, recording what the passes do.
#[derive(Default)]
struct Fake {
    size: u32,
    terrain: Vec<i32>,
    height: Vec<f32>,
    missing: Vec<(u32, u32)>,
    edges: Vec<(i32, i32)>,
    mesh: Vec<f32>,
    painted: Vec<(i32, i32, i32)>,
    at_point: Vec<(u32, u32, f32, f32, f32)>,
    at_tile: Vec<(u32, u32, i32, i32, f32)>,
    walls: Vec<(u32, Point, Point, u32)>,
    water: Vec<Point>,
    normals: u32,
    shorelines: u32,
    water_tiles: Vec<(u32, u32, bool)>,
    classes: Vec<Option<EeRmapGameClass>>,
    citizens: i32,
    ambient: Option<EeRmapGameTerrainInfo>,
    resource_of: Vec<(u32, i32)>,
}

impl Fake {
    fn new(size: u32) -> Self {
        let n = size as usize;
        Fake {
            size,
            terrain: vec![0; n * n],
            height: vec![1.0; n * n],
            mesh: vec![0.0; (n + 1) * (n + 1)],
            citizens: 100,
            ..Default::default()
        }
    }

    fn at(&self, x: u32, y: u32) -> usize {
        x as usize * self.size as usize + y as usize
    }

    fn m(&self, x: u32, y: u32) -> usize {
        x as usize * (self.size as usize + 1) + y as usize
    }
}

impl GameHost for Fake {
    fn tile(&mut self, x: u32, y: u32) -> Option<EeRmapGameTile> {
        (x < self.size && y < self.size && !self.missing.contains(&(x, y)))
            .then(|| EeRmapGameTile { terrain_type: self.terrain[self.at(x, y)], height: self.height[self.at(x, y)] })
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        self.edges.contains(&(x, y))
    }
    fn paint_terrain(&mut self, x: i32, y: i32, texture: i32) {
        self.painted.push((x, y, texture));
    }
    fn terrain(&mut self, texture: i32) -> Option<EeRmapGameTerrainInfo> {
        (texture >= 0).then(|| self.ambient.unwrap_or_default())
    }
    fn terrain_unit_class(&mut self, _texture: i32, index: u32) -> i32 {
        index as i32 + 1
    }
    fn mesh_z(&mut self, x: u32, y: u32) -> f32 {
        self.mesh[self.m(x, y)]
    }
    fn mesh_set_z(&mut self, x: u32, y: u32, z: f32) {
        let i = self.m(x, y);
        self.mesh[i] = z;
    }
    fn mesh_valid_point(&mut self, x: u32, y: u32) -> bool {
        x <= self.size && y <= self.size
    }
    fn mesh_valid_tile(&mut self, x: u32, y: u32) -> bool {
        x < self.size && y < self.size
    }
    fn mesh_set_water_color_set(&mut self, _x: u32, _y: u32) {}
    fn mesh_vertex_normal(&mut self, _x: u32, _y: u32) {
        self.normals += 1;
    }
    fn mesh_set_is_water(&mut self, x: u32, y: u32, is_water: bool) {
        self.water_tiles.push((x, y, is_water));
    }
    fn mesh_shorelines(&mut self, _x: u32, _y: u32) {
        self.shorelines += 1;
    }
    fn set_tile_height(&mut self, x: u32, y: u32, height: f32) {
        let i = self.at(x, y);
        self.height[i] = height;
    }
    fn unit_class_count(&mut self) -> u32 {
        self.classes.len() as u32
    }
    fn unit_class(&mut self, class: u32) -> Option<EeRmapGameClass> {
        self.classes.get(class as usize).copied().flatten()
    }
    fn class_allows_terrain(&mut self, _class: u32, terrain_type: i32) -> bool {
        terrain_type == 0
    }
    fn max_starting_citizens(&mut self, _player: u32) -> i32 {
        self.citizens
    }
    fn create_unit_at_point(&mut self, owner: u32, class: u32, x: f32, y: f32, heading: f32) -> i32 {
        self.at_point.push((owner, class, x, y, heading));
        self.resource_of.iter().find(|r| r.0 == class).map_or(-1, |r| r.1)
    }
    fn create_unit_at_tile(&mut self, owner: u32, class: u32, x: i32, y: i32, heading: f32) {
        self.at_tile.push((owner, class, x, y, heading));
    }
    fn place_wall(&mut self, class: u32, start: Point, end: Point, player: u32) {
        self.walls.push((class, start, end, player));
    }
    fn wall_tiles(&mut self, start: Point, end: Point, out: &mut Vec<(i32, i32)>) {
        out.clear();
        let (mut x, mut y) = (start.x as i32, start.y as i32);
        out.push((x, y));
        while (x, y) != (end.x as i32, end.y as i32) {
            x += (end.x as i32 - x).signum();
            y += (end.y as i32 - y).signum();
            out.push((x, y));
        }
    }
    fn push_water_point(&mut self, point: Point) {
        self.water.push(point);
    }
}

fn class(tile_size: i32) -> Option<EeRmapGameClass> {
    Some(EeRmapGameClass { tile_size, is_land_citizen: 0, is_wall: 0 })
}

#[test]
fn rows_read_and_write_the_cpp_rows() {
    let mut g = Owned::new(3, 0u32);
    {
        // SAFETY: the pointers are the rows of `g`, three cells each
        let rows = unsafe { Rows::new(g.ptrs.as_ptr(), 3) };
        rows.set(2, 1, 9);
        assert_eq!(rows.get(2, 1), 9);
        assert_eq!(rows.len(), 3);
    }
    assert_eq!(g.get(2, 1), 9);
    g.set(0, 2, 4);
    // SAFETY: as above
    let rows = unsafe { Rows::new(g.ptrs.as_ptr(), 3) };
    assert_eq!(rows.get(0, 2), 4);
}

#[test]
#[should_panic(expected = "past 3")]
fn rows_refuse_a_cell_off_the_grid() {
    let g = Owned::new(3, 0u8);
    // SAFETY: the rows of `g`
    let rows = unsafe { Rows::new(g.ptrs.as_ptr(), 3) };
    rows.get(3, 0);
}

#[test]
fn water_points_are_the_empty_valid_cells_with_a_tile() {
    let mut f = Fixture::new(4);
    let mut h = Fake::new(4);
    h.missing.push((0, 1));
    f.tiles.set(2, 2, (0x401) as c_ulong);
    f.tiles.set(3, 3, (0x8000) as c_ulong); // a cliff without land bits: still water
    f.valid.set(1, 1, 0);
    f.run(&mut h, |g, h| g.water_points(h));
    let mut want = Vec::new();
    for x in 0..4 {
        for y in 0..4 {
            if (x, y) != (0, 1) && (x, y) != (2, 2) && (x, y) != (1, 1) {
                want.push(Point::new(x as f32, y as f32));
            }
        }
    }
    assert_eq!(h.water, want);
}

#[test]
fn a_space_map_fills_cells_with_five_land_neighbours() {
    let mut f = Fixture::new(4);
    let mut h = Fake::new(4);
    f.state.is_space_map = 1;
    for (x, y) in [(0, 0), (0, 1), (0, 2), (1, 0), (2, 0)] {
        f.tiles.set(x, y, (0x10 + x * 4 + y) as c_ulong);
    }
    f.run(&mut h, |g, h| g.water_points(h));
    // (1, 1) sees five land cells around it and takes the last one counted, (2, 0)
    assert_eq!((f.tiles.get(1, 1) as u32), 0x10 + 8);
    // (0, 0)'s neighbours are not looked at: the loop from x - 1 wraps and does not run
    assert_eq!((f.tiles.get(2, 2) as u32), 0);
}

#[test]
fn an_earth_map_fills_a_disc_around_the_centre() {
    let mut f = Fixture::new(8);
    let mut h = Fake::new(8);
    f.state.is_space_map = 1;
    f.state.map_characteristics = 8;
    f.state.water_border = 0;
    f.run(&mut h, |g, h| g.water_points(h));
    // radius (4 - 0) * 0.75 = 3 around (4, 4)
    assert_eq!((f.tiles.get(4, 4) as u32), TILE_LAND);
    assert_eq!((f.tiles.get(4, 1) as u32), TILE_LAND);
    assert_eq!((f.tiles.get(4, 0) as u32), 0);
    assert_eq!((f.tiles.get(6, 6) as u32), TILE_LAND); // 2.83
    assert_eq!((f.tiles.get(7, 7) as u32), 0);
}

#[test]
fn smoothing_takes_the_neighbours_mean_when_the_step_is_large() {
    let mut f = Fixture::new(8);
    let mut h = Fake::new(9);
    // the neighbours swept before it see a step of 1 (a mean of 0.55): under the threshold
    f.state.height_map_smoothness = 1.5;
    f.heights.set(4, 4, 0.9);
    f.run(&mut h, |g, h| g.smooth_heights(h));
    // (0.9 * 20 - 10) - (0.5 * 20 - 10) = 8 >= 1.5: the mean of the eight 0.5s
    assert_eq!(f.heights.get(4, 4), 0.5);
    assert_eq!(f.heights.get(3, 3), 0.5);
    // a small step stays
    let mut f = Fixture::new(8);
    f.state.height_map_smoothness = 1.5;
    f.heights.set(4, 4, 0.51);
    f.run(&mut h, |g, h| g.smooth_heights(h));
    assert_eq!(f.heights.get(4, 4), 0.51);
    // a cell on the grid's last row sees five neighbours: (4 x 0.5 + 0.9) / 5 = 0.58, a step of 1.6
    let mut f = Fixture::new(4);
    f.state.height_map_smoothness = 1.5;
    f.heights.set(2, 2, 0.9);
    f.run(&mut h, |g, h| g.smooth_heights(h));
    assert_eq!(f.heights.get(1, 3), 0.58000004);
}

#[test]
fn smoothing_keeps_edge_cells_and_straddling_corners_at_sea_level() {
    let mut f = Fixture::new(4);
    let mut h = Fake::new(5);
    f.state.height_map_smoothness = 100.0;
    for x in 0..5 {
        for y in 0..5 {
            f.heights.set(x, y, 0.2);
        }
    }
    f.heights.set(2, 2, 0.6);
    f.run(&mut h, |g, h| g.smooth_heights(h));
    // sea level -(-10 / 20) = 0.5: the edge row (no tile at x - 1) rises to it
    assert_eq!(f.heights.get(0, 2), 0.5);
    // inner cells under the sea stay, except the corners of tiles touching the high cell
    assert_eq!(f.heights.get(3, 3), 0.5);
    assert_eq!(f.heights.get(1, 1), 0.5);
    assert_eq!(f.heights.get(2, 2), 0.6);
    // (4, 4) is past the grid size: no pass touches it
    assert_eq!(f.heights.get(4, 4), 0.2);
}

#[test]
fn applying_heights_clamps_flattens_and_sets_the_tiles() {
    let mut f = Fixture::new(2);
    let mut h = Fake::new(2);
    f.state.maximum_y = 2;
    f.state.tile_unit = 2.0;
    f.heights.set(0, 0, 2.0); // 30 -> clamped to 10, x 2
    f.heights.set(0, 1, 0.49); // -0.2: flattened to 0
    f.heights.set(1, 0, 0.4); // -2 x 2
    f.run(&mut h, |g, h| g.apply_heights(h));
    assert_eq!(h.mesh[h.m(0, 0)], 20.0);
    assert_eq!(h.mesh[h.m(0, 1)], 0.0);
    assert_eq!(h.mesh[h.m(1, 0)], -4.0);
    assert_eq!(h.mesh[h.m(1, 1)], 0.0);
    assert_eq!(h.height[h.at(0, 0)], -4.0);
    assert!(h.water_tiles.contains(&(0, 0, true)));
    assert!(h.water_tiles.contains(&(1, 1, false)));
    assert_eq!(h.normals, 9);
    assert_eq!(h.shorelines, 4);
}

#[test]
fn players_must_stand_on_dry_land_and_apart() {
    let mut f = Fixture::new(16);
    let mut h = Fake::new(16);
    let p = |x: f32, y: f32| EeRmapGamePlayer { position: Point::new(x, y), index: 1, is_neutral: 0 };
    f.players = vec![p(3.0, 3.0), p(12.0, 12.0)];
    assert!(f.run(&mut h, |g, h| g.players_connected(h)));
    f.players = vec![p(3.0, 3.0), p(7.0, 6.0)];
    assert!(!f.run(&mut h, |g, h| g.players_connected(h)), "5 apart");
    f.players = vec![p(3.0, 3.0), EeRmapGamePlayer { is_neutral: 1, ..p(4.0, 4.0) }];
    assert!(f.run(&mut h, |g, h| g.players_connected(h)), "a neutral is not a player");
    let i = h.at(4, 4);
    h.height[i] = -0.5;
    f.players = vec![p(3.0, 3.0)];
    assert!(!f.run(&mut h, |g, h| g.players_connected(h)), "a wet flat area");
    f.players.clear();
    assert!(f.run(&mut h, |g, h| g.players_connected(h)));
}

#[test]
fn features_claim_their_squares_walls_their_lines_and_cliffs_their_tiles() {
    let mut f = Fixture::new(16);
    let mut h = Fake::new(16);
    f.players = vec![EeRmapGamePlayer { position: Point::new(5.0, 5.0), index: 1, is_neutral: 0 }];
    f.unit_names = vec![EeRmapGameUnitName { unit_class: 3, size: 2 }];
    f.features = vec![Feature { position: Point::new(1.5, -2.0), unit_type: 0, ..Default::default() }];
    f.walls = vec![EeRmapGameWall { start_x: -3, start_y: 2, end_x: -1, end_y: 2, unit_type: 0 }];
    let i = h.at(10, 11);
    h.terrain[i] = TERRAIN_CLIFF;
    f.run(&mut h, |g, h| g.mark_features(h));
    for (x, y) in [(6, 3), (7, 3), (6, 4), (7, 4)] {
        assert_eq!(f.claimed.get(x, y), 1, "({x}, {y})");
    }
    assert_eq!(f.claimed.get(8, 3), 0);
    // the wall from (2.5, 7.5) to (4.5, 7.5)
    for x in 2..=4 {
        assert_eq!((f.claimed.get(x, 7), (f.tiles.get(x, 7) as u32)), (1, TILE_LAND));
    }
    assert_eq!(f.claimed.get(10, 11), 1);
}

#[test]
fn elevation_index_runs_from_minus_ten_and_clamps() {
    assert_eq!(Game::elevation_index(0.0), 10);
    assert_eq!(Game::elevation_index(-0.5), 9);
    assert_eq!(Game::elevation_index(-10.0), 0);
    assert_eq!(Game::elevation_index(-11.0), 30, "below -10 wraps unsigned, then clamps");
    assert_eq!(Game::elevation_index(25.0), 30);
}

fn one_terrain(f: &mut Fixture) {
    let mut textures = [0; 31];
    for (e, t) in textures.iter_mut().enumerate() {
        *t = 100 + e as i32;
    }
    f.terrains = vec![EeRmapGameTerrain { textures }];
    f.regions = vec![EeRmapGameRegion { from: 0, to: 99, terrain: 0 }];
}

#[test]
fn painting_uses_the_texture_of_each_tiles_elevation() {
    let mut f = Fixture::new(4);
    let mut h = Fake::new(4);
    one_terrain(&mut f);
    let i = h.at(1, 2);
    h.height[i] = -1.5;
    let j = h.at(3, 3);
    h.terrain[j] = TERRAIN_SPACE;
    let (seed, integers) = (f.state.random_seed, f.state.random_integers_seed);
    f.run(&mut h, |g, h| g.paint_tiles(h));
    // land first (height 1: elevation 11), then the water tile: its highest corner 0 counts as one unit deep (9)
    assert_eq!(h.painted.len(), 15);
    assert!(h.painted[..14].iter().all(|&(_, _, t)| t == 111));
    assert_eq!(h.painted[14], (1, 2, 109));
    assert!(!h.painted.iter().any(|&(x, y, _)| (x, y) == (3, 3)));
    // no ambient units: nothing drawn
    assert_eq!((f.state.random_seed, f.state.random_integers_seed), (seed, integers));
}

#[test]
fn a_region_past_the_terrains_paints_nothing_and_no_region_holding_the_value_takes_the_first() {
    let mut f = Fixture::new(2);
    let mut h = Fake::new(2);
    one_terrain(&mut f);
    f.regions = vec![EeRmapGameRegion { from: 200, to: 300, terrain: 0 }];
    f.run(&mut h, |g, h| g.paint_tiles(h));
    assert_eq!(h.painted.len(), 4);
    f.regions = vec![EeRmapGameRegion { from: 0, to: 99, terrain: 5 }];
    h.painted.clear();
    f.run(&mut h, |g, h| g.paint_tiles(h));
    assert!(h.painted.is_empty());
}

#[test]
fn ambient_units_go_on_unclaimed_cells_away_from_the_edge() {
    let mut f = Fixture::new(8);
    let mut h = Fake::new(8);
    one_terrain(&mut f);
    h.ambient = Some(EeRmapGameTerrainInfo { unit_count: 2, ambient_chance: 100 });
    h.classes = vec![None, class(1), class(1)];
    f.claimed.set(3, 3, 1);
    f.run(&mut h, |g, h| g.paint_tiles(h));
    // chance 100 x 0.01: every draw passes; the edge test needs (x - 1 .. x + 2) on the grid of 9
    let cells: Vec<(i32, i32)> = h.at_tile.iter().map(|u| (u.2, u.3)).collect();
    assert!(!cells.contains(&(3, 3)));
    assert!(!cells.contains(&(0, 4)));
    assert!(!cells.contains(&(7, 4)));
    assert!(cells.contains(&(1, 1)) && cells.contains(&(6, 6)));
    assert_eq!(cells.len(), 35);
    assert!(h.at_tile.iter().all(|u| u.0 == 0 && (u.1 == 1 || u.1 == 2) && u.4 == 0.0));
    assert_eq!(f.claimed.get(1, 1), 1);
    // the stream drawn once per unclaimed cell, the integers once per unit: replay them
    let mut r = URandom { seed: 11 };
    let mut n = URandom { seed: 13 };
    for _ in 0..63 {
        r.generate();
    }
    for _ in 0..35 {
        n.generate_int(0, 1);
    }
    assert_eq!((f.state.random_seed, f.state.random_integers_seed), (r.seed, n.seed));
}

#[test]
fn objects_become_natures_units_on_their_tiles() {
    let mut f = Fixture::new(8);
    let mut h = Fake::new(8);
    h.classes = vec![None; 0x140];
    for c in [0x26, 0x13b, 5] {
        h.classes[c] = class(1);
    }
    f.unit_names = vec![EeRmapGameUnitName { unit_class: 5, size: 1 }];
    f.animals = vec![0];
    f.objects = vec![
        Object { location: Point::new(2.7, 3.2), kind: 0, object_type: 0 },
        Object { location: Point::new(4.0, 4.0), kind: 5, object_type: 0 },
        Object { location: Point::new(5.0, 5.0), kind: 4, object_type: 0 },
        Object { location: Point::new(6.0, 6.0), kind: 1, object_type: 0 }, // no gold class: skipped
        Object { location: Point::new(1.0, 1.0), kind: 9, object_type: 0 },
    ];
    let i = h.at(4, 4);
    h.terrain[i] = TERRAIN_CLIFF;
    f.run(&mut h, |g, h| g.place_objects(h));
    assert_eq!(h.at_tile.len(), 2);
    assert_eq!(h.at_tile[0], (0, 0x26, 2, 3, 3.1415927));
    assert_eq!(h.painted, vec![(2, 3, 10)]);
    let mut r = URandom { seed: 11 };
    let mut n = URandom { seed: 13 };
    n.generate_int(0, 0);
    let heading = r.generate() * 6.2831855 - 3.1415927;
    assert_eq!(h.at_tile[1], (0, 5, 5, 5, heading));
    assert_eq!((f.state.random_seed, f.state.random_integers_seed), (r.seed, n.seed));
}

#[test]
fn trees_take_a_random_type_of_their_forest_and_its_texture() {
    let mut f = Fixture::new(8);
    let mut h = Fake::new(8);
    h.classes = vec![class(1); 10];
    f.unit_names = vec![EeRmapGameUnitName { unit_class: 7, size: 1 }, EeRmapGameUnitName { unit_class: 8, size: 1 }];
    f.forest_tree_types = vec![1, 0, 1];
    f.forests = vec![EeRmapGameForest { tree_types_start: 1, tree_types_len: 2, terrain_texture: 42 }];
    f.objects = vec![Object { location: Point::new(3.0, 3.0), kind: 2, object_type: 0 }];
    f.run(&mut h, |g, h| g.place_objects(h));
    let mut n = URandom { seed: 13 };
    let pick = n.generate_int(0, 1) as usize;
    assert_eq!(h.at_tile[0].1, [7, 8][pick]);
    assert_eq!(h.painted, vec![(3, 3, 42)]);
}

#[test]
fn start_units_respect_the_citizen_limit_and_paint_resources() {
    let mut f = Fixture::new(16);
    let mut h = Fake::new(16);
    h.classes = vec![None, Some(EeRmapGameClass { tile_size: 1, is_land_citizen: 1, is_wall: 0 }), class(1), None];
    h.classes.push(Some(EeRmapGameClass { tile_size: 1, is_land_citizen: 0, is_wall: 1 }));
    h.citizens = 1;
    h.resource_of = vec![(2, RESOURCE_GOLD)];
    f.unit_names = vec![
        EeRmapGameUnitName { unit_class: 1, size: 1 },
        EeRmapGameUnitName { unit_class: 2, size: 1 },
        EeRmapGameUnitName { unit_class: 4, size: 1 },
    ];
    f.players = vec![EeRmapGamePlayer { position: Point::new(8.0, 8.0), index: 2, is_neutral: 0 }];
    let feature =
        |unit_type, x, y| Feature { position: Point::new(x, y), unit_type, angle: 0.25, ..Default::default() };
    f.features = vec![
        feature(0, 1.0, 0.0),
        feature(0, 2.0, 0.0),
        Feature { is_world_unit: 1, ..feature(1, -1.0, -1.0) },
        Feature { is_herd: 1, ..feature(1, 3.5, 4.5) },
    ];
    f.walls = vec![EeRmapGameWall { start_x: 1, start_y: 1, end_x: 3, end_y: 1, unit_type: 2 }];
    f.run(&mut h, |g, h| g.place_features(h));
    assert_eq!(
        h.at_point,
        vec![(2, 1, 9.0, 8.0, 0.25), (0, 2, 7.0, 7.0, 0.25), (0, 2, 3.5, 4.5, 0.25)],
        "the second citizen is past the limit; the world unit and the herd are nature's"
    );
    assert_eq!(h.painted, vec![(7, 7, 11), (3, 4, 11)]);
    assert_eq!(h.walls, vec![(4, Point::new(9.5, 9.5), Point::new(11.5, 9.5), 2)]);
}

#[test]
fn ring_units_land_on_a_free_tile_and_claim_it() {
    let mut f = Fixture::new(32);
    let mut h = Fake::new(32);
    h.classes = vec![None, class(1)];
    f.unit_names = vec![EeRmapGameUnitName { unit_class: 1, size: 1 }];
    f.players = vec![EeRmapGamePlayer { position: Point::new(16.0, 16.0), index: 1, is_neutral: 0 }];
    f.features = vec![Feature { position: Point::new(3.0, 6.0), unit_type: 0, in_ring: 1, ..Default::default() }];
    f.run(&mut h, |g, h| g.place_features(h));
    let mut r = URandom { seed: 11 };
    let angle = r.generate() * 6.2831855 - 3.1415927;
    let radius = r.generate() * 3.0 + 3.0;
    let x = floor(um_cos(angle) * radius + 16.0);
    let y = floor(um_sin(angle) * radius + 16.0);
    assert_eq!(h.at_point, vec![(1, 1, x as f32 + 0.5, y as f32 + 0.5, 0.0)]);
    assert_eq!(f.claimed.get(x as u32, y as u32), 1);
    assert_eq!(f.state.random_seed, r.seed);
}

#[test]
fn a_ring_unit_with_no_free_tile_is_dropped() {
    let mut f = Fixture::new(32);
    let mut h = Fake::new(32);
    h.classes = vec![None, class(1)];
    f.state.max_loop_count = 3;
    for x in 0..33 {
        for y in 0..33 {
            f.claimed.set(x, y, 1);
        }
    }
    f.unit_names = vec![EeRmapGameUnitName { unit_class: 1, size: 1 }];
    f.players = vec![EeRmapGamePlayer { position: Point::new(16.0, 16.0), index: 1, is_neutral: 0 }];
    f.features = vec![Feature { position: Point::new(3.0, 6.0), unit_type: 0, in_ring: 1, ..Default::default() }];
    f.run(&mut h, |g, h| g.place_features(h));
    assert!(h.at_point.is_empty());
    let mut r = URandom { seed: 11 };
    for _ in 0..6 {
        r.generate();
    }
    assert_eq!(f.state.random_seed, r.seed);
}

#[test]
fn grid_sizes_are_the_power_of_two_above_twice_the_map() {
    assert_eq!(grid_size(20, 10, 0), 0x40);
    assert_eq!(grid_size(32, 31, 0), 0x80);
    assert_eq!(grid_size(100, 200, 0), 0x200);
    assert_eq!(grid_size(511, 0, 0), 0x400);
    assert_eq!(grid_size(512, 0, 77), 77, "past the largest size the C++ assigns nothing");
}

#[test]
fn rings_scale_with_the_map_past_the_flat_area() {
    let radii = EeRmapGameRadii { player_inner: 0.5, player_outer: 1.0, neutral_inner: 0.25, neutral_outer: 0.75 };
    let r = rings(Rect { left: -3.0, bottom: -5.0, right: 4.0, top: 2.0 }, 100, radii);
    let radius = (100.0f32 - 5.0) * RING_SCALE;
    assert_eq!(r.half_map_size, 50.0);
    assert_eq!(r.ring, Ring { center: Point::new(50.0, 50.0), min_radius: radius * 0.5, max_radius: radius });
    assert_eq!(r.neutral_ring.min_radius, radius * 0.25);
    assert_eq!(r.neutral_ring.max_radius, radius * 0.75);
    assert_eq!(
        r.moon_ring,
        Ring { center: Point::new(50.0, 50.0), min_radius: (50.0 - radius) * 0.5 + radius, max_radius: 50.0 }
    );
}
