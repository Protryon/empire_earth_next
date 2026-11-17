//! The game's side of a random map (`EERandomMap.cpp`): the passes between and after the generator's PIM steps that
//! turn the generator state into the `EEMap`'s heights, terrain and units.
//!
//! The state stays the C++'s (`EERandomMapData`, the generator PIMs' struct): the grids are reached through their row
//! pointers (`std::vector<std::vector<T>>` rows, [`EeRmapGameState`]), the lists through flat copies made for the call.
//! The map, the terrain mesh and the unit classes are the host's ([`GameHost`], over [`EeRmapGameHost`]'s callbacks).
//! The two random streams (`mRandom`, `mRandomIntegers`) are drawn here from their seeds, which go back into the state.
//!
//! Simulation input: every float operation is the C++'s, in its order (RUST_PLAN 2.1); unsigned arithmetic wraps as
//! the C++'s `unsigned long` did.

use crate::rmap::map::{Feature, Object, Rect};
use crate::rmap::math::{floor, floor_unsigned, lrintf, um_cos, um_sin, Point};
use ee_base::random::URandom;
use ee_base::util::EeUtilFractal;
use std::ffi::{c_ulong, c_void};

/// `kRMMinimumPlayerDistance`: the least distance between two players' starts.
const MINIMUM_PLAYER_DISTANCE: f32 = 6.0;
/// `kRMRingScale`: the player ring's radius per tile of the map past the flat area.
const RING_SCALE: f32 = 0.35355338;
/// `kRMElevationOffset`: a terrain's texture index is its elevation (in tile units) minus this.
const ELEVATION_OFFSET: f32 = -10.0;
/// `kHeightEpsilon` (EECPAttackSubGroup.cpp): a terrain's ambient unit chance is a percentage.
const HEIGHT_EPSILON: f32 = 0.01;

const TERRAIN_CLIFF: i32 = 9;
const TERRAIN_SPACE: i32 = 12;

const RESOURCE_FOOD: i32 = 2;
const RESOURCE_WOOD: i32 = 3;
const RESOURCE_STONE: i32 = 4;
const RESOURCE_GOLD: i32 = 5;
const RESOURCE_IRON: i32 = 6;

const MAP_PLANETS: u32 = 4;
const MAP_EARTH_OR_MARS: u32 = 8 | 0x10;
const MAP_MARS: u32 = 0x10;

/// A tile cell with land on it: its low 15 bits.
const TILE_LAND_MASK: u32 = 0x7fff;
/// The land value the space fill and the wall lines write.
const TILE_LAND: u32 = 0x400;
/// A cell at or above this is a cliff.
const TILE_CLIFF: u32 = 0x8000;

/// One of the state's grids, `[x][y]`, through the C++ rows: `n` rows of `n` cells.
pub struct Rows<'a, T> {
    rows: &'a [*mut T],
}

impl<'a, T: Copy> Rows<'a, T> {
    /// # Safety
    /// `rows` holds `n` pointers to `n` cells each, valid and not otherwise accessed while the `Rows` lives.
    pub unsafe fn new(rows: *const *mut T, n: u32) -> Self {
        if rows.is_null() || n == 0 {
            Rows { rows: &[] }
        } else {
            // SAFETY: the caller's guarantee
            Rows { rows: unsafe { std::slice::from_raw_parts(rows, n as usize) } }
        }
    }

    /// The rows (and cells per row): the C++'s `grid.size()`.
    pub fn len(&self) -> u32 {
        self.rows.len() as u32
    }

    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    fn ptr(&self, x: u32, y: u32) -> *mut T {
        let n = self.rows.len();
        assert!((x as usize) < n && (y as usize) < n, "random map grid cell ({x}, {y}) past {n}");
        // SAFETY: in bounds (checked); the rows are `n` cells long (`new`)
        unsafe { self.rows[x as usize].add(y as usize) }
    }

    pub fn get(&self, x: u32, y: u32) -> T {
        // SAFETY: `ptr` checked the cell
        unsafe { *self.ptr(x, y) }
    }

    pub fn set(&self, x: u32, y: u32, value: T) {
        // SAFETY: `ptr` checked the cell; the grid is ours for the call (`new`)
        unsafe { *self.ptr(x, y) = value }
    }
}

/// One of the generator's players as the game passes read it (`EERMPlayer`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGamePlayer {
    pub position: Point,
    /// The game's player number (`mIndex`: `(*mGamePlayers)[mIndex]`).
    pub index: u32,
    pub is_neutral: u8,
}

/// `EERMWall`: a start wall, relative to the player's tile.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameWall {
    pub start_x: i32,
    pub start_y: i32,
    pub end_x: i32,
    pub end_y: i32,
    pub unit_type: i32,
}

/// `EERMNamedEntry`'s numbers: the unit class a script unit name stands for, and its tile size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameUnitName {
    pub unit_class: i32,
    pub size: u32,
}

/// `EERMTerrain`'s textures: one per elevation, -10 to 20 tile units.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeRmapGameTerrain {
    pub textures: [i32; 31],
}

/// `EERMTerrainRegion`: fractal values `from..=to` (plus the cliff, Mars or band hundreds) paint terrain `terrain`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameRegion {
    pub from: i32,
    pub to: i32,
    pub terrain: u32,
}

/// `EERMForest`; its tree types are `forest_tree_types[tree_types_start..][..tree_types_len]`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameForest {
    pub tree_types_start: u32,
    pub tree_types_len: u32,
    pub terrain_texture: i32,
}

/// What the game passes read and write of `EERandomMapData`. The grids are the C++ rows (`*_rows[x]` points at row
/// `x`, `grid_n` rows of `grid_n` cells); the lists are copies (a pointer may be null when its length is 0). The two
/// seeds come back advanced.
#[repr(C)]
pub struct EeRmapGameState {
    pub grid_n: u32,
    pub heights: *const *mut f32,
    /// `mTiles`: `unsigned long` cells (the values fit 32 bits).
    pub tiles: *const *mut c_ulong,
    pub claimed_tiles: *const *mut u8,
    pub valid_tiles: *const *mut u8,
    /// `mGridSize` (the grids hold one more row and column).
    pub grid_size: u32,
    pub maximum_y: u32,
    pub map_characteristics: u32,
    pub is_space_map: u8,
    pub half_map_size: f32,
    pub water_border: i32,
    pub ring_center: Point,
    pub flat_area: Rect,
    pub min_int_elevation: i32,
    pub max_int_elevation: i32,
    pub height_map_smoothness: f32,
    pub terrain_chaos: f32,
    pub seed: u32,
    pub num_terrain_bands: u32,
    pub terrain_band_offset: u32,
    pub max_loop_count: u32,
    /// `EEMap::GetF1955F4`: the height of one elevation unit.
    pub tile_unit: f32,
    /// `gSpaceTerrain`: the terrain named "Space".
    pub space_terrain: i32,
    pub random_seed: u32,
    pub random_integers_seed: u32,
    pub resource_terrain_texture: [i32; 7],
    pub players: *const EeRmapGamePlayer,
    pub players_len: u32,
    pub features: *const Feature,
    pub features_len: u32,
    pub walls: *const EeRmapGameWall,
    pub walls_len: u32,
    pub unit_names: *const EeRmapGameUnitName,
    pub unit_names_len: u32,
    pub terrains: *const EeRmapGameTerrain,
    pub terrains_len: u32,
    pub terrain_regions: *const EeRmapGameRegion,
    pub terrain_regions_len: u32,
    pub objects: *const Object,
    pub objects_len: u32,
    pub forests: *const EeRmapGameForest,
    pub forests_len: u32,
    pub forest_tree_types: *const u32,
    pub forest_tree_types_len: u32,
    pub available_animals: *const u32,
    pub available_animals_len: u32,
}

/// A map tile as the passes read it (`EETile`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapGameTile {
    pub terrain_type: i32,
    pub height: f32,
}

/// A terrain table entry's ambient units (`EETerrainTable`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameTerrainInfo {
    pub unit_count: u32,
    /// `mAmbientUnitChance`, a percentage.
    pub ambient_chance: i32,
}

/// A unit class as the passes read it (`EEUnitClass`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameClass {
    pub tile_size: i32,
    pub is_land_citizen: u8,
    pub is_wall: u8,
}

/// The map, the mesh, the tables and the units, for the passes.
pub trait GameHost {
    /// `EEMap::GetTile` (unsigned coordinates: a negative one wraps off the map): `None` off the map.
    fn tile(&mut self, x: u32, y: u32) -> Option<EeRmapGameTile>;
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool;
    fn paint_terrain(&mut self, x: i32, y: i32, texture: i32);
    /// `EETerrainTable::Get`: `None` for no entry.
    fn terrain(&mut self, texture: i32) -> Option<EeRmapGameTerrainInfo>;
    fn terrain_unit_class(&mut self, texture: i32, index: u32) -> i32;
    fn mesh_z(&mut self, x: u32, y: u32) -> f32;
    fn mesh_set_z(&mut self, x: u32, y: u32, z: f32);
    fn mesh_valid_point(&mut self, x: u32, y: u32) -> bool;
    fn mesh_valid_tile(&mut self, x: u32, y: u32) -> bool;
    fn mesh_set_water_color_set(&mut self, x: u32, y: u32);
    fn mesh_vertex_normal(&mut self, x: u32, y: u32);
    fn mesh_set_is_water(&mut self, x: u32, y: u32, is_water: bool);
    fn mesh_shorelines(&mut self, x: u32, y: u32);
    /// The tile's `mHeight` (the tile exists).
    fn set_tile_height(&mut self, x: u32, y: u32, height: f32);
    /// `EEUnitClassCount()`.
    fn unit_class_count(&mut self) -> u32;
    /// `gUnitClassTable[class]`: `None` when NULL (or past the table).
    fn unit_class(&mut self, class: u32) -> Option<EeRmapGameClass>;
    /// `mTerrainTypes[terrain_type]` of the class.
    fn class_allows_terrain(&mut self, class: u32, terrain_type: i32) -> bool;
    /// `(*mGamePlayers)[player]->mMaxStartingCitizens`.
    fn max_starting_citizens(&mut self, player: u32) -> i32;
    /// `CreateUnitAtPoint` for `(*mGamePlayers)[owner]`; a complex unit is built and paid for. Returns the resource
    /// type (food 2 .. iron 6) of a new resource unit with an amount, else -1.
    fn create_unit_at_point(&mut self, owner: u32, class: u32, x: f32, y: f32, heading: f32) -> i32;
    /// `CreateUnitAtTile` for `(*mGamePlayers)[owner]`.
    fn create_unit_at_tile(&mut self, owner: u32, class: u32, x: i32, y: i32, heading: f32);
    /// `EERandomMap::PlaceWall` for `(*mGamePlayers)[player]`.
    fn place_wall(&mut self, class: u32, start: Point, end: Point, player: u32);
    /// A wall line's tiles: `InterpolateTo` at 1, truncated to tiles, `EEDropDiagonalCorners` (a line, not one point),
    /// `EEDropRepeatedTiles`.
    fn wall_tiles(&mut self, start: Point, end: Point, out: &mut Vec<(i32, i32)>);
    /// `mWaterPoints.push_back`.
    fn push_water_point(&mut self, point: Point);
}

/// The sink `EeRmapGameHost::wall_tiles` fills.
pub struct EeRmapGameTiles(Vec<(i32, i32)>);

/// The host's callbacks (C++: `EERandomMap.cpp`'s host over the `EEMap`, the tables and the game's players).
#[repr(C)]
pub struct EeRmapGameHost {
    pub context: *mut c_void,
    pub tile: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32, out: *mut EeRmapGameTile) -> u8,
    pub is_edge_tile: unsafe extern "C" fn(context: *mut c_void, x: i32, y: i32) -> u8,
    pub paint_terrain: unsafe extern "C" fn(context: *mut c_void, x: i32, y: i32, texture: i32),
    pub terrain: unsafe extern "C" fn(context: *mut c_void, texture: i32, out: *mut EeRmapGameTerrainInfo) -> u8,
    pub terrain_unit_class: unsafe extern "C" fn(context: *mut c_void, texture: i32, index: u32) -> i32,
    pub mesh_z: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> f32,
    pub mesh_set_z: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32, z: f32),
    pub mesh_valid_point: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> u8,
    pub mesh_valid_tile: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> u8,
    pub mesh_set_water_color_set: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32),
    pub mesh_vertex_normal: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32),
    pub mesh_set_is_water: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32, is_water: u8),
    pub mesh_shorelines: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32),
    pub set_tile_height: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32, height: f32),
    pub unit_class_count: unsafe extern "C" fn(context: *mut c_void) -> u32,
    pub unit_class: unsafe extern "C" fn(context: *mut c_void, unit_class: u32, out: *mut EeRmapGameClass) -> u8,
    pub class_allows_terrain: unsafe extern "C" fn(context: *mut c_void, unit_class: u32, terrain_type: i32) -> u8,
    pub max_starting_citizens: unsafe extern "C" fn(context: *mut c_void, player: u32) -> i32,
    pub create_unit_at_point:
        unsafe extern "C" fn(context: *mut c_void, owner: u32, unit_class: u32, x: f32, y: f32, heading: f32) -> i32,
    pub create_unit_at_tile:
        unsafe extern "C" fn(context: *mut c_void, owner: u32, unit_class: u32, x: i32, y: i32, heading: f32),
    pub place_wall: unsafe extern "C" fn(context: *mut c_void, unit_class: u32, start: Point, end: Point, player: u32),
    pub wall_tiles: unsafe extern "C" fn(context: *mut c_void, start: Point, end: Point, sink: *mut EeRmapGameTiles),
    pub push_water_point: unsafe extern "C" fn(context: *mut c_void, x: f32, y: f32),
}

/// Adds a tile to the line `EeRmapGameHost::wall_tiles` is filling.
///
/// # Safety
/// `sink` is the sink the callback was handed.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_tiles_push(sink: *mut EeRmapGameTiles, x: i32, y: i32) {
    // SAFETY: the caller's guarantee
    if let Some(sink) = unsafe { sink.as_mut() } {
        sink.0.push((x, y));
    }
}

impl GameHost for EeRmapGameHost {
    fn tile(&mut self, x: u32, y: u32) -> Option<EeRmapGameTile> {
        let mut out = EeRmapGameTile::default();
        // SAFETY: the C++ host's callbacks take its context and an out record
        (unsafe { (self.tile)(self.context, x, y, &mut out) } != 0).then_some(out)
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        // SAFETY: as `tile`
        unsafe { (self.is_edge_tile)(self.context, x, y) != 0 }
    }
    fn paint_terrain(&mut self, x: i32, y: i32, texture: i32) {
        // SAFETY: as `tile`
        unsafe { (self.paint_terrain)(self.context, x, y, texture) }
    }
    fn terrain(&mut self, texture: i32) -> Option<EeRmapGameTerrainInfo> {
        let mut out = EeRmapGameTerrainInfo::default();
        // SAFETY: as `tile`
        (unsafe { (self.terrain)(self.context, texture, &mut out) } != 0).then_some(out)
    }
    fn terrain_unit_class(&mut self, texture: i32, index: u32) -> i32 {
        // SAFETY: as `tile`
        unsafe { (self.terrain_unit_class)(self.context, texture, index) }
    }
    fn mesh_z(&mut self, x: u32, y: u32) -> f32 {
        // SAFETY: as `tile`
        unsafe { (self.mesh_z)(self.context, x, y) }
    }
    fn mesh_set_z(&mut self, x: u32, y: u32, z: f32) {
        // SAFETY: as `tile`
        unsafe { (self.mesh_set_z)(self.context, x, y, z) }
    }
    fn mesh_valid_point(&mut self, x: u32, y: u32) -> bool {
        // SAFETY: as `tile`
        unsafe { (self.mesh_valid_point)(self.context, x, y) != 0 }
    }
    fn mesh_valid_tile(&mut self, x: u32, y: u32) -> bool {
        // SAFETY: as `tile`
        unsafe { (self.mesh_valid_tile)(self.context, x, y) != 0 }
    }
    fn mesh_set_water_color_set(&mut self, x: u32, y: u32) {
        // SAFETY: as `tile`
        unsafe { (self.mesh_set_water_color_set)(self.context, x, y) }
    }
    fn mesh_vertex_normal(&mut self, x: u32, y: u32) {
        // SAFETY: as `tile`
        unsafe { (self.mesh_vertex_normal)(self.context, x, y) }
    }
    fn mesh_set_is_water(&mut self, x: u32, y: u32, is_water: bool) {
        // SAFETY: as `tile`
        unsafe { (self.mesh_set_is_water)(self.context, x, y, is_water as u8) }
    }
    fn mesh_shorelines(&mut self, x: u32, y: u32) {
        // SAFETY: as `tile`
        unsafe { (self.mesh_shorelines)(self.context, x, y) }
    }
    fn set_tile_height(&mut self, x: u32, y: u32, height: f32) {
        // SAFETY: as `tile`
        unsafe { (self.set_tile_height)(self.context, x, y, height) }
    }
    fn unit_class_count(&mut self) -> u32 {
        // SAFETY: as `tile`
        unsafe { (self.unit_class_count)(self.context) }
    }
    fn unit_class(&mut self, class: u32) -> Option<EeRmapGameClass> {
        let mut out = EeRmapGameClass::default();
        // SAFETY: as `tile`
        (unsafe { (self.unit_class)(self.context, class, &mut out) } != 0).then_some(out)
    }
    fn class_allows_terrain(&mut self, class: u32, terrain_type: i32) -> bool {
        // SAFETY: as `tile`
        unsafe { (self.class_allows_terrain)(self.context, class, terrain_type) != 0 }
    }
    fn max_starting_citizens(&mut self, player: u32) -> i32 {
        // SAFETY: as `tile`
        unsafe { (self.max_starting_citizens)(self.context, player) }
    }
    fn create_unit_at_point(&mut self, owner: u32, class: u32, x: f32, y: f32, heading: f32) -> i32 {
        // SAFETY: as `tile`
        unsafe { (self.create_unit_at_point)(self.context, owner, class, x, y, heading) }
    }
    fn create_unit_at_tile(&mut self, owner: u32, class: u32, x: i32, y: i32, heading: f32) {
        // SAFETY: as `tile`
        unsafe { (self.create_unit_at_tile)(self.context, owner, class, x, y, heading) }
    }
    fn place_wall(&mut self, class: u32, start: Point, end: Point, player: u32) {
        // SAFETY: as `tile`
        unsafe { (self.place_wall)(self.context, class, start, end, player) }
    }
    fn wall_tiles(&mut self, start: Point, end: Point, out: &mut Vec<(i32, i32)>) {
        let mut sink = EeRmapGameTiles(std::mem::take(out));
        sink.0.clear();
        // SAFETY: as `tile`; the callback fills the sink through ee_rmap_game_tiles_push only
        unsafe { (self.wall_tiles)(self.context, start, end, &mut sink) };
        *out = sink.0;
    }
    fn push_water_point(&mut self, point: Point) {
        // SAFETY: as `tile`
        unsafe { (self.push_water_point)(self.context, point.x, point.y) }
    }
}

/// The state for one pass: the grids, the lists and the two random streams.
pub struct Game<'a> {
    pub heights: Rows<'a, f32>,
    pub tiles: Rows<'a, c_ulong>,
    pub claimed_tiles: Rows<'a, u8>,
    pub valid_tiles: Rows<'a, u8>,
    pub s: &'a EeRmapGameState,
    pub players: &'a [EeRmapGamePlayer],
    pub features: &'a [Feature],
    pub walls: &'a [EeRmapGameWall],
    pub unit_names: &'a [EeRmapGameUnitName],
    pub terrains: &'a [EeRmapGameTerrain],
    pub terrain_regions: &'a [EeRmapGameRegion],
    pub objects: &'a [Object],
    pub forests: &'a [EeRmapGameForest],
    pub forest_tree_types: &'a [u32],
    pub available_animals: &'a [u32],
    /// `mRandom`.
    pub random: URandom,
    /// `mRandomIntegers`.
    pub random_integers: URandom,
}

unsafe fn slice<'a, T>(data: *const T, len: u32) -> &'a [T] {
    if len == 0 || data.is_null() {
        &[]
    } else {
        // SAFETY: the caller passes `len` valid elements
        unsafe { std::slice::from_raw_parts(data, len as usize) }
    }
}

impl<'a> Game<'a> {
    /// # Safety
    /// The state's pointers are valid for their lengths (the grids `grid_n` rows of `grid_n` cells) while the `Game`
    /// lives, and the four grids are distinct.
    pub unsafe fn new(s: &'a EeRmapGameState) -> Self {
        // SAFETY: the caller's guarantee
        unsafe {
            Game {
                heights: Rows::new(s.heights, s.grid_n),
                tiles: Rows::new(s.tiles, s.grid_n),
                claimed_tiles: Rows::new(s.claimed_tiles, s.grid_n),
                valid_tiles: Rows::new(s.valid_tiles, s.grid_n),
                s,
                players: slice(s.players, s.players_len),
                features: slice(s.features, s.features_len),
                walls: slice(s.walls, s.walls_len),
                unit_names: slice(s.unit_names, s.unit_names_len),
                terrains: slice(s.terrains, s.terrains_len),
                terrain_regions: slice(s.terrain_regions, s.terrain_regions_len),
                objects: slice(s.objects, s.objects_len),
                forests: slice(s.forests, s.forests_len),
                forest_tree_types: slice(s.forest_tree_types, s.forest_tree_types_len),
                available_animals: slice(s.available_animals, s.available_animals_len),
                random: URandom { seed: s.random_seed },
                random_integers: URandom { seed: s.random_integers_seed },
            }
        }
    }

    fn is_space_map(&self) -> bool {
        self.s.is_space_map != 0
    }

    /// A `mTiles` cell (32 bits on every target: the generator writes `uint32_t`s).
    #[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64
    fn tile_value(&self, x: u32, y: u32) -> u32 {
        self.tiles.get(x, y) as u32
    }

    #[allow(clippy::unnecessary_cast)]
    fn set_tile_value(&self, x: u32, y: u32, value: u32) {
        self.tiles.set(x, y, value as c_ulong);
    }

    /// `EERMIsTileFree`: no land value.
    fn is_tile_free(&self, x: u32, y: u32) -> bool {
        self.tile_value(x, y) & TILE_LAND_MASK == 0
    }

    /// `EERMIsCellSet` over the valid tiles: inside the grid and set.
    fn is_valid_cell(&self, x: u32, y: u32) -> bool {
        let n = self.valid_tiles.len();
        x < n && y < n && self.valid_tiles.get(x, y) != 0
    }

    /// `EERMIsAreaFree`: every cell of the square (clipped to the grid) valid and unclaimed.
    fn is_area_free(&self, x: u32, y: u32, radius: u32) -> bool {
        let size = self.valid_tiles.len();
        let mut left = x.wrapping_sub(radius);
        let mut right = x.wrapping_add(radius);
        let mut bottom = y.wrapping_sub(radius);
        let mut top = y.wrapping_add(radius);
        if left >= size {
            left = 0;
        }
        if bottom >= size {
            bottom = 0;
        }
        if right >= size {
            right = size.wrapping_sub(1);
        }
        if top >= size {
            top = size.wrapping_sub(1);
        }
        let mut result = true;
        let mut cx = left;
        while cx <= right && result {
            let mut cy = bottom;
            while cy <= top && result {
                result = !(self.valid_tiles.get(cx, cy) == 0 || self.claimed_tiles.get(cx, cy) != 0);
                cy = cy.wrapping_add(1);
            }
            cx = cx.wrapping_add(1);
        }
        result
    }

    /// `EERMIsNearEdge`: a corner of the square around the cell off the grid or not valid.
    fn is_near_edge(&self, x: u32, y: u32, radius: u32) -> bool {
        let size = self.valid_tiles.len();
        let left = x.wrapping_sub(radius);
        let right = x.wrapping_add(1).wrapping_add(radius);
        let bottom = y.wrapping_sub(radius);
        let top = y.wrapping_add(1).wrapping_add(radius);
        !(left < size
            && right < size
            && bottom < size
            && top < size
            && self.valid_tiles.get(left, bottom) != 0
            && self.valid_tiles.get(left, top) != 0
            && self.valid_tiles.get(right, bottom) != 0
            && self.valid_tiles.get(right, top) != 0)
    }

    /// `EERMClaimArea`: the `size` x `size` square from the cell, clipped to the grid.
    fn claim_area(&self, x: u32, y: u32, size: u32) {
        let n = self.claimed_tiles.len();
        for i in 0..size {
            for j in 0..size {
                if x.wrapping_add(i) < n && j.wrapping_add(y) < n {
                    self.claimed_tiles.set(x.wrapping_add(i), j.wrapping_add(y), 1);
                }
            }
        }
    }

    fn unit_name(&self, unit_type: i32) -> Option<EeRmapGameUnitName> {
        // the C++ indexes the list unchecked; a script never names a unit type past it
        usize::try_from(unit_type).ok().and_then(|i| self.unit_names.get(i)).copied()
    }

    /// The resource terrain under a new resource unit (`PlaceFeatures`' switch): -1 for none.
    fn resource_texture(&self, resource_type: i32) -> Option<i32> {
        let t = &self.s.resource_terrain_texture;
        match resource_type {
            RESOURCE_FOOD => Some(t[3]),
            RESOURCE_WOOD => Some(t[2]),
            RESOURCE_STONE => Some(t[0]),
            RESOURCE_GOLD => Some(t[1]),
            RESOURCE_IRON => Some(t[6]),
            _ => None,
        }
    }

    /// `EERandomMap::Fn284149`: on space maps fills the empty cells (a disc on Earth and Mars maps, else cells with 5
    /// or more land neighbours), then lists the empty valid map cells as the water points.
    pub fn water_points(&mut self, host: &mut impl GameHost) {
        let size = self.s.grid_size;
        if self.is_space_map() {
            if self.s.map_characteristics & MAP_EARTH_OR_MARS != 0 {
                let radius = (self.s.half_map_size - self.s.water_border as f32) * 0.75;
                for x in 0..size {
                    for y in 0..size {
                        if self.valid_tiles.get(x, y) != 0
                            && self.s.ring_center.distance_to(Point::new(x as f32, y as f32)) <= radius
                            && self.is_tile_free(x, y)
                        {
                            self.set_tile_value(x, y, TILE_LAND);
                        }
                    }
                }
            } else {
                for x in 0..size {
                    for y in 0..size {
                        if self.valid_tiles.get(x, y) == 0 || !self.is_tile_free(x, y) {
                            continue;
                        }
                        let mut value = 0u32;
                        let mut count = 0u32;
                        // the C++'s unsigned loops: from x - 1, which wraps at 0 (and the loop does not run)
                        let mut i = x.wrapping_sub(1);
                        while i <= x.wrapping_add(1) {
                            let mut j = y.wrapping_sub(1);
                            while j <= y.wrapping_add(1) {
                                if !self.is_tile_free(i, j) {
                                    value = self.tile_value(i, j);
                                    count += 1;
                                }
                                j = j.wrapping_add(1);
                            }
                            i = i.wrapping_add(1);
                        }
                        if count >= 5 {
                            self.set_tile_value(x, y, value);
                        }
                    }
                }
            }
        }
        for x in 0..size {
            for y in 0..size {
                if host.tile(x, y).is_none() || !self.is_valid_cell(x, y) || self.tile_value(x, y) & TILE_LAND_MASK != 0
                {
                    continue;
                }
                host.push_water_point(Point::new(x as f32, y as f32));
            }
        }
    }

    /// `EERandomMap::AddNeighbourHeight` (its minimum and maximum are never read).
    fn add_neighbour_height(&self, x: u32, y: u32, sum: &mut f32, count: &mut i32) {
        if !(x < self.s.grid_size) || !(y < self.s.grid_size) || self.valid_tiles.get(x, y) == 0 {
            return;
        }
        *sum += self.heights.get(x, y);
        *count += 1;
    }

    /// `EERandomMap::SmoothHeights`: each height toward its neighbours' mean, edge (space maps: empty) cells kept at
    /// sea level, then the low corners of the tiles straddling the sea level raised to it.
    pub fn smooth_heights(&mut self, host: &mut impl GameHost) {
        let range = (self.s.max_int_elevation.wrapping_sub(self.s.min_int_elevation)) as f32;
        let scale = self.s.tile_unit;
        let minimum = self.s.min_int_elevation as f32;
        let sea_level = -(minimum / range);
        let size = self.s.grid_size as i32;
        for x in 0..size {
            for y in 0..size {
                let (ux, uy) = (x as u32, y as u32);
                if self.valid_tiles.get(ux, uy) == 0 {
                    continue;
                }
                let mut height = self.heights.get(ux, uy);
                let mut sum = 0.0f32;
                let mut count = 0i32;
                for (dx, dy) in [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)] {
                    self.add_neighbour_height((x + dx) as u32, (y + dy) as u32, &mut sum, &mut count);
                }
                sum /= count as f32;
                height = (height * range + minimum) * scale;
                let delta = height - (sum * range + minimum) * scale;
                let delta = if delta < 0.0 { -delta } else { delta };
                if delta >= scale * self.s.height_map_smoothness {
                    self.heights.set(ux, uy, sum);
                }
                if self.is_space_map() {
                    if self.heights.get(ux, uy) < sea_level && self.is_tile_free(ux, uy) {
                        self.heights.set(ux, uy, sea_level);
                    }
                } else {
                    if host.tile(ux, uy).is_some()
                        && host.tile((x - 1) as u32, uy).is_some()
                        && host.tile((x - 1) as u32, (y - 1) as u32).is_some()
                        && host.tile(ux, (y - 1) as u32).is_some()
                    {
                        continue;
                    }
                    if self.heights.get(ux, uy) < sea_level {
                        self.heights.set(ux, uy, sea_level);
                    }
                }
            }
        }
        let below = sea_level + 0.0001;
        for x in 0..size as u32 {
            for y in 0..size as u32 {
                if self.valid_tiles.get(x, y) == 0 || host.tile(x, y).is_none() {
                    continue;
                }
                let mut low = false;
                let mut high = false;
                for (cx, cy) in [(x, y), (x, y + 1), (x + 1, y)] {
                    let h = self.heights.get(cx, cy);
                    if h > sea_level {
                        high = true;
                    } else if h < below {
                        low = true;
                    }
                }
                // the fourth corner: high goes on to the low test; else a tile with no high corner is done
                let h = self.heights.get(x + 1, y + 1);
                if !(h > sea_level) {
                    if h < below {
                        low = true;
                    }
                    if !high {
                        continue;
                    }
                }
                if !low {
                    continue;
                }
                for (cx, cy) in [(x, y), (x, y + 1), (x + 1, y), (x + 1, y + 1)] {
                    if self.heights.get(cx, cy) < sea_level {
                        self.heights.set(cx, cy, sea_level);
                    }
                }
            }
        }
    }

    /// `EERandomMap::Fn288839`: the height grid onto the terrain mesh (clamped, shallow dips flattened, empty
    /// space-map cells painted Space), then the mesh's water, normals, tile heights and shorelines.
    pub fn apply_heights(&mut self, host: &mut impl GameHost) {
        let range = (self.s.max_int_elevation.wrapping_sub(self.s.min_int_elevation)) as f32;
        let scale = self.s.tile_unit;
        let size = self.s.maximum_y.wrapping_add(1);
        let minimum = self.s.min_int_elevation as f32;
        let max_z = self.s.max_int_elevation as f32;
        let min_z = self.s.min_int_elevation as f32;
        for x in 0..size {
            for y in 0..size {
                let mut z = range * self.heights.get(x, y) + minimum;
                if z > max_z {
                    z = max_z;
                } else if z < min_z {
                    z = min_z;
                }
                if z < 0.0 && (z > -0.25 || self.is_space_map()) {
                    z = 0.0;
                }
                z *= scale;
                if self.is_space_map() && self.is_tile_free(x, y) {
                    if host.mesh_valid_point(x, y) {
                        host.mesh_set_z(x, y, 0.0);
                    }
                    host.paint_terrain(x as i32, y as i32, self.s.space_terrain);
                } else if host.mesh_valid_point(x, y) {
                    host.mesh_set_z(x, y, z);
                    if z < 0.0 {
                        host.mesh_set_water_color_set(x, y);
                    }
                }
            }
        }
        // the C++'s min(): (a) < (b) ? (a) : (b)
        let min = |a: f32, b: f32| if a < b { a } else { b };
        for x in 0..size {
            for y in 0..size {
                if !host.mesh_valid_point(x, y) {
                    continue;
                }
                host.mesh_vertex_normal(x, y);
                if !host.mesh_valid_tile(x, y) {
                    continue;
                }
                let lowest = min(host.mesh_z(x, y), host.mesh_z(x + 1, y));
                let lowest = min(lowest, host.mesh_z(x, y + 1));
                let lowest = min(lowest, host.mesh_z(x + 1, y + 1));
                host.mesh_set_is_water(x, y, lowest < 0.0);
                host.set_tile_height(x, y, lowest);
            }
        }
        for x in 0..size {
            for y in 0..size {
                if host.mesh_valid_tile(x, y) {
                    host.mesh_shorelines(x, y);
                }
            }
        }
    }

    /// `EERMArePlayersConnected`: every player's flat area dry, non-space land, and no two players closer than
    /// `kRMMinimumPlayerDistance`.
    pub fn players_connected(&self, host: &mut impl GameHost) -> bool {
        let flat = self.s.flat_area;
        let mut result = true;
        let mut i = 0;
        while i < self.players.len() && result {
            let p = self.players[i];
            i += 1;
            if p.is_neutral != 0 {
                continue;
            }
            let mut x = floor(p.position.x + flat.left) as u32;
            let bottom = floor(p.position.y + flat.bottom) as u32;
            let right = (floor(p.position.x + flat.right) as u32).wrapping_add(1);
            let top = (floor(p.position.y + flat.top) as u32).wrapping_add(1);
            while x < right && result {
                let mut y = bottom;
                while y < top && result {
                    result = match host.tile(x, y) {
                        Some(t) => t.height >= 0.0 && t.terrain_type != TERRAIN_SPACE,
                        None => false,
                    };
                    y = y.wrapping_add(1);
                }
                x = x.wrapping_add(1);
            }
        }
        // the C++ runs i to size - 1 (unsigned): with no players it would read past them
        let last = self.players.len().wrapping_sub(1);
        let mut i = 0;
        while i < last && i < self.players.len() && result {
            if self.players[i].is_neutral == 0 {
                let mut j = i + 1;
                while j < self.players.len() && result {
                    if self.players[j].is_neutral == 0 {
                        let d = self.players[i].position.distance_to_squared(self.players[j].position);
                        result = d >= MINIMUM_PLAYER_DISTANCE * MINIMUM_PLAYER_DISTANCE;
                    }
                    j += 1;
                }
            }
            i += 1;
        }
        result
    }

    /// `EERandomMap::MarkLine`: a wall line's tiles as land and claimed.
    fn mark_line(&self, host: &mut impl GameHost, start: Point, end: Point, tiles: &mut Vec<(i32, i32)>) {
        host.wall_tiles(start, end, tiles);
        for &(x, y) in tiles.iter() {
            if host.tile(x as u32, y as u32).is_some() {
                self.set_tile_value(x as u32, y as u32, TILE_LAND);
                self.claimed_tiles.set(x as u32, y as u32, 1);
            }
        }
    }

    /// `EERandomMap::MarkFeatures`: claims the cells of every player's start units and walls, and of every cliff tile.
    pub fn mark_features(&mut self, host: &mut impl GameHost) {
        let mut tiles = Vec::new();
        for p in self.players {
            if p.is_neutral != 0 {
                continue;
            }
            let (px, py) = (p.position.x, p.position.y);
            for f in self.features {
                let Some(entry) = self.unit_name(f.unit_type) else { continue };
                let y = floor(f.position.y + py) as u32;
                let x = floor(f.position.x + px) as u32;
                self.claim_area(x, y, entry.size);
            }
            for w in self.walls {
                let start = Point::new(w.start_x as f32 + px + 0.5, w.start_y as f32 + py + 0.5);
                let end = Point::new(w.end_x as f32 + px + 0.5, w.end_y as f32 + py + 0.5);
                self.mark_line(host, start, end, &mut tiles);
            }
        }
        // the C++ walks the grid with float counters (exact: every value is a small integer)
        let mut x = 0.0f32;
        while x < self.s.grid_size as f32 {
            let mut y = 0.0f32;
            while y < self.s.grid_size as f32 {
                let (tx, ty) = (floor(x) as u32, floor(y) as u32);
                if host.tile(tx, ty).is_some_and(|t| t.terrain_type == TERRAIN_CLIFF) {
                    self.claimed_tiles.set(tx, ty, 1);
                }
                y += 1.0;
            }
            x += 1.0;
        }
    }

    /// `EERMGetElevation`: a terrain's texture index for a height in elevation units (0 to 30).
    fn elevation_index(height: f32) -> usize {
        let index = floor_unsigned(floor(height) as f32 - ELEVATION_OFFSET);
        index.min(30) as usize
    }

    /// `EERandomMap::PlaceTerrainUnit`: by the terrain's chance, one of its ambient units on an unclaimed cell away
    /// from the edge, owned by nature.
    fn place_terrain_unit(
        &mut self,
        host: &mut impl GameHost,
        x: u32,
        y: u32,
        texture: i32,
        info: EeRmapGameTerrainInfo,
    ) {
        if !(info.unit_count > 0)
            || self.claimed_tiles.get(x, y) != 0
            || !(self.random.generate() <= info.ambient_chance as f32 * HEIGHT_EPSILON)
            || self.is_near_edge(x, y, if self.is_space_map() { 3 } else { 1 })
        {
            return;
        }
        let index = self.random_integers.generate_int(0, info.unit_count.wrapping_sub(1) as i32);
        let class = host.terrain_unit_class(texture, index as u32) as u32;
        if host.unit_class(class).is_some() {
            host.create_unit_at_tile(0, class, x as i32, y as i32, 0.0);
            self.claimed_tiles.set(x, y, 1);
        }
    }

    /// The terrain a tile's fractal value selects (`PaintTiles`): the first region holding the value, else the first
    /// region; `None` past the terrain list.
    fn region_terrain(&self, value: u32) -> Option<usize> {
        let count = self.terrains.len() as u32;
        let mut terrain = count;
        for r in self.terrain_regions {
            if value >= r.from as u32 && value <= r.to as u32 {
                terrain = r.terrain;
                break;
            }
        }
        if terrain == count && !self.terrain_regions.is_empty() {
            terrain = self.terrain_regions[0].terrain;
        }
        (terrain < count).then_some(terrain as usize)
    }

    fn paint_with(&mut self, host: &mut impl GameHost, x: u32, y: u32, texture: i32) {
        if let Some(info) = host.terrain(texture) {
            host.paint_terrain(x as i32, y as i32, texture);
            self.place_terrain_unit(host, x, y, texture, info);
        }
    }

    /// `EERandomMap::PaintTiles`: every land tile, then every water tile, painted with the terrain its fractal value
    /// (plus the cliff, Mars or band offset) selects by elevation, perhaps with that terrain's ambient unit.
    pub fn paint_tiles(&mut self, host: &mut impl GameHost) {
        let size = self.s.grid_size;
        let mut fractal = EeUtilFractal::new(size);
        fractal.compute(0.0, self.s.terrain_chaos, self.s.seed);
        let value_at = |x: u32, y: u32| lrintf(fractal.value(x, y) * 100.0).clamp(0, 99) as u32;
        let unit = self.s.tile_unit;
        for x in 0..size {
            for y in 0..size {
                let Some(tile) = host.tile(x, y) else { continue };
                if tile.height < 0.0 || tile.terrain_type == TERRAIN_SPACE {
                    continue;
                }
                let mut value = value_at(x, y);
                let cell = self.tile_value(x, y);
                if cell >= TILE_CLIFF {
                    value = value.wrapping_add(((cell.wrapping_add(1) & 3) + 10).wrapping_mul(100));
                } else if self.s.map_characteristics & MAP_MARS == MAP_MARS {
                    value = value.wrapping_add(100);
                } else if self.s.map_characteristics & MAP_PLANETS == MAP_PLANETS {
                    let band = self.s.terrain_band_offset.wrapping_add(cell) % self.s.num_terrain_bands;
                    value = value.wrapping_add(band.wrapping_mul(100));
                }
                let Some(terrain) = self.region_terrain(value) else { continue };
                let texture = self.terrains[terrain].textures[Self::elevation_index(tile.height / unit)];
                self.paint_with(host, x, y, texture);
            }
        }
        for x in 0..size {
            for y in 0..size {
                let Some(tile) = host.tile(x, y) else { continue };
                if tile.height >= 0.0 {
                    continue;
                }
                let Some(terrain) = self.region_terrain(value_at(x, y)) else { continue };
                // the water tile's highest corner; one at or above the water line counts as one unit deep
                let mut highest = host.mesh_z(x, y);
                for (cx, cy) in [(x + 1, y), (x, y + 1), (x + 1, y + 1)] {
                    let z = host.mesh_z(cx, cy);
                    if z > highest {
                        highest = z;
                    }
                }
                if highest >= 0.0 {
                    highest = -unit;
                }
                let texture = self.terrains[terrain].textures[Self::elevation_index(highest / unit)];
                self.paint_with(host, x, y, texture);
            }
        }
    }

    /// Creates a feature's unit (`PlaceFeatures`) and paints its resource terrain under it.
    fn create_feature_unit(&self, host: &mut impl GameHost, owner: u32, class: u32, x: f32, y: f32, angle: f32) {
        let resource = host.create_unit_at_point(owner, class, x, y, angle);
        if let Some(texture) = self.resource_texture(resource) {
            if texture != -1 {
                host.paint_terrain(floor(x), floor(y), texture);
            }
        }
    }

    /// `EERandomMap::PlaceFeatures`: each player's start units (citizens up to the player's limit, ring units on
    /// random free tiles) and walls, then the herds, owned by nature.
    pub fn place_features(&mut self, host: &mut impl GameHost) {
        let class_count = host.unit_class_count();
        for p in self.players {
            if p.is_neutral != 0 {
                continue;
            }
            let (px, py) = (p.position.x, p.position.y);
            let player = p.index;
            let citizen_limit = host.max_starting_citizens(player) as u32;
            let mut citizens = 0u32;
            for f in self.features {
                let Some(entry) = self.unit_name(f.unit_type) else { continue };
                if f.is_herd != 0 || entry.unit_class as u32 >= class_count {
                    continue;
                }
                let class_index = entry.unit_class as u32;
                let mut class = host.unit_class(class_index);
                if class.is_some_and(|c| c.is_land_citizen != 0) {
                    citizens += 1;
                    if citizens > citizen_limit {
                        class = None;
                    }
                }
                let Some(class) = class else { continue };
                let (mut x, mut y);
                if f.in_ring == 0 {
                    x = f.position.x + px;
                    y = f.position.y + py;
                } else {
                    x = 0.0;
                    let mut tries = 0u32;
                    y = 0.0;
                    if tries < self.s.max_loop_count {
                        let range = f.position.y - f.position.x;
                        loop {
                            let angle = self.random.generate() * 6.2831855 - 3.1415927;
                            let radius = self.random.generate() * range + f.position.x;
                            x = um_cos(angle) * radius + px;
                            y = um_sin(angle) * radius + py;
                            let tx = floor(x);
                            let ty = floor(y);
                            let size = class.tile_size as u32;
                            if let Some(tile) = host.tile(tx as u32, ty as u32) {
                                if !host.is_edge_tile(tx, ty)
                                    && host.class_allows_terrain(class_index, tile.terrain_type)
                                    && self.is_area_free(tx as u32, ty as u32, size)
                                    && !self.is_near_edge(tx as u32, ty as u32, 1)
                                {
                                    self.claim_area(tx as u32, ty as u32, size);
                                    x = tx as f32 + 0.5;
                                    y = ty as f32 + 0.5;
                                    break;
                                }
                            }
                            tries += 1;
                            if tries >= self.s.max_loop_count {
                                break;
                            }
                        }
                    }
                    if tries == self.s.max_loop_count {
                        x = -1.0;
                        y = -1.0;
                    }
                }
                if host.tile(floor(x) as u32, floor(y) as u32).is_none() || host.is_edge_tile(floor(x), floor(y)) {
                    continue;
                }
                let owner = if f.is_world_unit != 0 { 0 } else { player };
                self.create_feature_unit(host, owner, class_index, x, y, f.angle);
            }
            for w in self.walls {
                let Some(entry) = self.unit_name(w.unit_type) else { continue };
                if entry.unit_class as u32 >= class_count {
                    continue;
                }
                let class_index = entry.unit_class as u32;
                if !host.unit_class(class_index).is_some_and(|c| c.is_wall != 0) {
                    continue;
                }
                let (fx, fy) = (floor(px), floor(py));
                let start =
                    Point::new(fx.wrapping_add(w.start_x) as f32 + 0.5, fy.wrapping_add(w.start_y) as f32 + 0.5);
                let end = Point::new(fx.wrapping_add(w.end_x) as f32 + 0.5, fy.wrapping_add(w.end_y) as f32 + 0.5);
                host.place_wall(class_index, start, end, player);
            }
        }
        for f in self.features {
            let Some(entry) = self.unit_name(f.unit_type) else { continue };
            if f.is_herd != 1 || entry.unit_class as u32 >= class_count {
                continue;
            }
            let class_index = entry.unit_class as u32;
            if host.unit_class(class_index).is_none() {
                continue;
            }
            let (tx, ty) = (floor(f.position.x), floor(f.position.y));
            if host.tile(tx as u32, ty as u32).is_none() || host.is_edge_tile(tx, ty) {
                continue;
            }
            self.create_feature_unit(host, 0, class_index, f.position.x, f.position.y, f.angle);
        }
    }

    /// `EERandomMap::Fn288426`: the generator's resource objects (stone, gold, trees, food, animals, fish, iron) as
    /// nature's units, with their resource terrain painted under them.
    pub fn place_objects(&mut self, host: &mut impl GameHost) {
        let class_count = host.unit_class_count();
        for o in self.objects {
            let mut random_heading = false;
            let class: u32;
            let kind_texture = || self.s.resource_terrain_texture[o.kind as usize];
            let texture = match o.kind {
                0 => {
                    class = 0x26;
                    kind_texture()
                }
                1 => {
                    class = 0x32;
                    kind_texture()
                }
                2 => {
                    let forest = self.forests[o.object_type as usize];
                    let types =
                        &self.forest_tree_types[forest.tree_types_start as usize..][..forest.tree_types_len as usize];
                    let index = self.random_integers.generate_int(0, (types.len() as u32).wrapping_sub(1) as i32);
                    class = self.unit_names[types[index as usize] as usize].unit_class as u32;
                    forest.terrain_texture
                }
                3 => {
                    class = 0x37;
                    kind_texture()
                }
                4 => {
                    if !self.available_animals.is_empty() {
                        let last = (self.available_animals.len() as u32).wrapping_sub(1) as i32;
                        let index = self.random_integers.generate_int(0, last);
                        class = self.unit_names[self.available_animals[index as usize] as usize].unit_class as u32;
                        random_heading = true;
                    } else {
                        class = 0;
                    }
                    -1
                }
                5 => {
                    class = 0x13b;
                    -1
                }
                6 => {
                    class = 0xe0;
                    kind_texture()
                }
                _ => continue,
            };
            if class == 0 || class >= class_count {
                continue;
            }
            let x = floor(o.location.x) as u32;
            let y = floor(o.location.y) as u32;
            if host.unit_class(class).is_none() {
                continue;
            }
            match host.tile(x, y) {
                Some(t) if t.terrain_type != TERRAIN_CLIFF => {}
                _ => continue,
            }
            let heading = if random_heading { self.random.generate() * 6.2831855 - 3.1415927 } else { 3.1415927 };
            host.create_unit_at_tile(0, class, x as i32, y as i32, heading);
            // the tile as it is after the unit went on it
            let terrain_type = host.tile(x, y).map_or(TERRAIN_SPACE, |t| t.terrain_type);
            if texture != -1 && terrain_type != TERRAIN_SPACE && self.tile_value(x, y) < TILE_CLIFF {
                host.paint_terrain(x as i32, y as i32, texture);
            }
        }
    }
}

/// `EERandomMap::CreateGrids`' grid size: the power of two (64 to 1024) above twice the map's larger side; a map past
/// it keeps `current` (the C++ assigns nothing then).
pub fn grid_size(height: u32, width: u32, current: u32) -> u32 {
    let mut size = height.wrapping_mul(2);
    let other = width.wrapping_mul(2);
    if other > size {
        size = other;
    }
    match size {
        s if s < 0x40 => 0x40,
        s if s < 0x80 => 0x80,
        s if s < 0x100 => 0x100,
        s if s < 0x200 => 0x200,
        s if s < 0x400 => 0x400,
        _ => current,
    }
}

/// The rings `CreateGrids` sets up, and the half map size.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapGameRings {
    pub half_map_size: f32,
    pub ring: crate::rmap::map::Ring,
    pub neutral_ring: crate::rmap::map::Ring,
    pub moon_ring: crate::rmap::map::Ring,
}

/// The radii factors of `CreateGrids`' rings (`mPlayerInnerRadius` ...).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapGameRadii {
    pub player_inner: f32,
    pub player_outer: f32,
    pub neutral_inner: f32,
    pub neutral_outer: f32,
}

/// `EERandomMap::CreateGrids`' rings: the player and neutral rings around the centre, scaled by the map past the flat
/// area's largest extent, and the moon ring outside the player ring.
pub fn rings(flat_area: Rect, map_size: i32, radii: EeRmapGameRadii) -> EeRmapGameRings {
    let abs = |v: f32| if v < 0.0 { -v } else { v };
    let mut extent = abs(flat_area.left);
    for v in [flat_area.right, flat_area.bottom, flat_area.top] {
        let v = abs(v);
        if v > extent {
            extent = v;
        }
    }
    let value = map_size as f32;
    let radius = (value - extent) * RING_SCALE;
    let half = value * 0.5;
    let center = Point::new(0.5 * map_size as f32, 0.5 * map_size as f32);
    let ring = crate::rmap::map::Ring {
        center,
        min_radius: radius * radii.player_inner,
        max_radius: radius * radii.player_outer,
    };
    let neutral_ring = crate::rmap::map::Ring {
        center,
        min_radius: radius * radii.neutral_inner,
        max_radius: radius * radii.neutral_outer,
    };
    let moon_ring = crate::rmap::map::Ring {
        center,
        max_radius: half,
        min_radius: (half - ring.max_radius) * 0.5 + ring.max_radius,
    };
    EeRmapGameRings { half_map_size: half, ring, neutral_ring, moon_ring }
}

/// One pass over the state and the host; the advanced seeds go back into the state.
///
/// # Safety
/// `state` and `host` are valid; the state's pointers as `Game::new` asks.
unsafe fn run<R>(
    state: *mut EeRmapGameState,
    host: *mut EeRmapGameHost,
    pass: impl FnOnce(&mut Game, &mut EeRmapGameHost) -> R,
) -> R {
    // SAFETY: the caller's guarantee
    let host = unsafe { &mut *host };
    let (result, seeds) = {
        // SAFETY: the caller's guarantee; the record is only read while the game lives
        let mut game = unsafe { Game::new(&*state) };
        let result = pass(&mut game, host);
        (result, (game.random.seed, game.random_integers.seed))
    };
    // SAFETY: the caller's guarantee (the game, which read the record, is gone)
    unsafe {
        (*state).random_seed = seeds.0;
        (*state).random_integers_seed = seeds.1;
    }
    result
}

/// `EERandomMap::Fn284149` (the space fill and the water points).
///
/// # Safety
/// `state` and `host` are valid (`EeRmapGameState`'s pointers for their lengths).
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_water_points(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.water_points(h)) }
}

/// `EERandomMap::SmoothHeights`.
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_smooth_heights(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.smooth_heights(h)) }
}

/// `EERandomMap::Fn288839` (the heights onto the mesh).
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_apply_heights(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.apply_heights(h)) }
}

/// `EERMArePlayersConnected`.
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_players_connected(
    state: *mut EeRmapGameState,
    host: *mut EeRmapGameHost,
) -> bool {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.players_connected(h)) }
}

/// `EERandomMap::MarkFeatures`.
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_mark_features(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.mark_features(h)) }
}

/// `EERandomMap::PaintTiles` (with `PlaceTerrainUnit`).
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_paint_tiles(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.paint_tiles(h)) }
}

/// `EERandomMap::PlaceFeatures`.
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_place_features(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.place_features(h)) }
}

/// `EERandomMap::Fn288426` (the resource objects).
///
/// # Safety
/// As `ee_rmap_game_water_points`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_game_place_objects(state: *mut EeRmapGameState, host: *mut EeRmapGameHost) {
    // SAFETY: the caller's guarantee
    unsafe { run(state, host, |g, h| g.place_objects(h)) }
}

/// `CreateGrids`' grid size (see [`grid_size`]).
#[no_mangle]
pub extern "C" fn ee_rmap_game_grid_size(height: u32, width: u32, current: u32) -> u32 {
    grid_size(height, width, current)
}

/// `CreateGrids`' rings (see [`rings`]).
#[no_mangle]
pub extern "C" fn ee_rmap_game_rings(flat_area: Rect, map_size: i32, radii: EeRmapGameRadii) -> EeRmapGameRings {
    rings(flat_area, map_size, radii)
}

#[cfg(test)]
mod tests;
