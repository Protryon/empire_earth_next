//! The game's random map whole (world lane, round 1): the generator state (`EERandomMapData`, the Default generator
//! PIMs' struct), the try (`EERandomMap`: its set-up, the generator's steps, the game's passes, the start location)
//! and the retries of `EERandomMap::CreateRandomMap` are one Rust object. The C++ keeps the generator list (its
//! names are `UWideString`s the lobby reads) and answers the world's calls: one operation per C++ call
//! ([`EeRmapCpp`], one-statement cases of `ee_rmap_cpp` / `ee_rmap_cpp_f` in `EERandomMap.cpp`).
//!
//! The state moved from the C++ struct field for field: the grids, the players, the teams, the lists a script builds,
//! the scalar block (`EeRmapScriptScalars`, the twin of the struct's block from `mMapSize`), the rings and the flat
//! area. A generator step sees what `Default.cpp`'s adapter handed it (a copy of the state as `Map`) and keeps what
//! the adapter copied back (the seeds, the team player count, the four grids, the players, the team positions, the
//! objects, the features, the clump circles and the player order); a game pass reads the state through the same
//! `EeRmapGameState` the C++ view built (the grids by their rows) and takes the advanced seeds back.
//!
//! Simulation input: the order of the C++ calls and of every random draw is the original's (RUST_PLAN 2.1).

#![allow(clippy::unnecessary_cast)] // `long` and `unsigned long` are 32-bit here, 64-bit on LP64

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicI32, Ordering};

use ee_base::math::types::Ge2DPoint;
use ee_base::random::URandom;

use crate::rmap::game::{
    grid_size, rings, EeRmapGameClass, EeRmapGameForest, EeRmapGamePlayer, EeRmapGameRadii, EeRmapGameRegion,
    EeRmapGameState, EeRmapGameTerrain, EeRmapGameTerrainInfo, EeRmapGameTile, EeRmapGameUnitName, EeRmapGameWall,
    Game, GameHost,
};
use crate::rmap::generator::{run_step, Step};
use crate::rmap::map::{
    Circle, Feature, Grid, Map, Object, Player, Rect, ResourceKind, ResourceSlot, Ring, Settings, SpacingRule, Team,
    URandomState,
};
use crate::rmap::math::Point;
use crate::rmap::rmv::{
    read_script, script_generator, script_name, EeRmapScriptBytes, EeRmapScriptRegion, EeRmapScriptScalars,
    EeRmapScriptTerrain, EeRmapScriptWall, ScriptHost, ScriptTables,
};
use crate::units::uu_note::Note;
use crate::units::uu_tiles::{self, Tile};

/// The world's calls (p: the C++ context, `EERMWorld`: the `EEMap`, the game's players and options; or the object a
/// call names). Each is one statement of `ee_rmap_cpp` (`_f`: a float answer), see `EERandomMap.cpp`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeRmapCpp {
    // ---- the map (p: the context)
    /// `mMap->mL195610` (the map's height in tiles)
    MapHeight = 1,
    /// `mMap->mL195614`
    MapWidth,
    MapTileCount,
    /// `mMap->mSize`
    MapSize,
    /// `mMap->GetF1955F4()` (float)
    MapTileUnit,
    /// `mMap->GetTile(a0, a1)` (a pointer, 0 off the map)
    MapTile,
    MapIsEdgeTile,
    MapPaintTerrain,
    MapSetCliffTexture,
    /// `UpdateCliffs(0, mSize, 0, mSize)`
    MapUpdateCliffs,
    MapSetCharacteristics,
    MapRaisedCorners,
    /// `mTerrainMesh.GetZ` (float)
    MeshZ,
    MeshSetZ,
    MeshValidPoint,
    MeshValidTile,
    MeshWaterColorSet,
    MeshVertexNormal,
    MeshSetIsWater,
    MeshShorelines,
    // ---- a tile (p: an EEMapTile*)
    TileTerrain,
    /// `GetHeight()` (float)
    TileHeight,
    TileSetHeight,
    /// `mUnits` (a pointer)
    TileUnitNode,
    /// `mUnits->mUnit`
    TileFirstUnit,
    // ---- the game's players and options (p: the context)
    PlayerCount,
    /// `(*mGamePlayers)[a0]`
    Player,
    // ---- a player (p: an EEPlayer*)
    PlayerId,
    PlayerEnemy,
    PlayerMaxCitizens,
    StartEpoch,
    // ---- the rules, the tables, the classes (p: unused)
    RuleMapSizeCount,
    RuleMapSizeTiles,
    RuleMapSizeScript,
    ClassCount,
    /// `gUnitClassTable[a0]`
    Class,
    // ---- a unit class (p: an EEUnitClass*)
    ClassEnabled,
    /// the name's bytes into the sink a0
    ClassName,
    ClassTileSizeMember,
    ClassGetTileSize,
    ClassIsLandCitizen,
    ClassIsWall,
    ClassAllowsTerrain,
    ClassAIKind,
    /// `CreateUnitAtPoint(a0 player, f0, f1, a1 map, f2)`
    ClassCreateAtPoint,
    /// `CreateUnitAtTile(a0 player, a1, a2, a3 map, f0)`
    ClassCreateAtTile,
    // ---- the terrain tables (p: unused, or the table)
    TerrainCount,
    Terrain,
    TerrainName,
    TerrainUnitCount,
    TerrainAmbientChance,
    TerrainUnitClass,
    CliffCount,
    Cliff,
    CliffName,
    // ---- a unit (p: an EEUnit*, an EEComplexUnit* for the Cx ones)
    UnitComplex,
    UnitGetComplex,
    UnitPlayer,
    UnitClassOf,
    UnitResourceType,
    UnitResourceAmount,
    CxSetBuilt,
    CxIsPaidFor,
    CxSetPaidFor,
    CxAnimation,
    // ---- the game (p: unused)
    /// `gStartupVolume + gStartupDirectory + "Data\\Random Map Scripts\\"` into the sink a0
    ScriptDirectory,
    /// `PlatFOpen(a0, "r")`'s whole text into the sink a1 -> false when missing
    ReadText,
    SetUsingCarbon,
    LoadProgress,
    /// `ESDebugOut("%s", a0)`
    DebugOut,
    Time,
    // ---- the generator list (EERandomMap::sRandomMapGenerators; p: the script search, EERMFind)
    GeneratorsClear,
    GeneratorsSort,
    GeneratorCount,
    /// `PlatMbsICmp(sRandomMapGenerators[a0].mName, *(UWideString*)a1) == 0`
    GeneratorMatches,
    /// `PlatStrICmp(sRandomMapGenerators[a0].mModuleName, "Default") == 0`
    GeneratorIsDefault,
    /// the generator's name into the sink a1
    GeneratorName,
    /// a generator pushed: its name, module and display name (C strings a0, a1, a2)
    GeneratorPush,
    FindNew,
    FindDelete,
    /// `mDirectory.GetFirstFile("*.rmv", mFile)`
    FindFirst,
    FindNext,
    /// the found file's name into the sink a0
    FindName,
    /// the found file's text into the sink a0 -> false when it does not open
    FindText,
    /// `gResource->GetString(a1, display)` into the sink a0 -> its result
    ResourceString,
    /// `ESVERIFY_AT(a0 != 0, a1)`
    Verify,
}

pub use EeRmapCpp as O;

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EeRmapCpp;
    extern "C" {
        // EERandomMap.cpp
        pub fn ee_rmap_cpp(op: EeRmapCpp, p: usize, a: *const i64, f: *const f32) -> i64;
        pub fn ee_rmap_cpp_f(op: EeRmapCpp, p: usize, a: *const i64, f: *const f32) -> f64;
    }
}

/// The tests run no C++: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use super::EeRmapCpp;
    pub unsafe fn ee_rmap_cpp(op: EeRmapCpp, _: usize, _: *const i64, _: *const f32) -> i64 {
        panic!("no C++ in the tests ({op:?})")
    }
    pub unsafe fn ee_rmap_cpp_f(op: EeRmapCpp, _: usize, _: *const i64, _: *const f32) -> f64 {
        panic!("no C++ in the tests ({op:?})")
    }
}

fn args(a: &[i64], f: &[f32]) -> ([i64; 6], [f32; 4]) {
    let mut ai = [0i64; 6];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    (ai, fi)
}

fn hf(op: O, p: usize, a: &[i64], f: &[f32]) -> i64 {
    let (ai, fi) = args(a, f);
    // SAFETY: the host's operation over the C++ objects the caller holds (the thread that makes the map)
    unsafe { sys::ee_rmap_cpp(op, p, ai.as_ptr(), fi.as_ptr()) }
}

fn h(op: O, p: usize, a: &[i64]) -> i64 {
    hf(op, p, a, &[])
}

fn hd(op: O, p: usize, a: &[i64]) -> f32 {
    let (ai, fi) = args(a, &[]);
    // SAFETY: as hf
    unsafe { sys::ee_rmap_cpp_f(op, p, ai.as_ptr(), fi.as_ptr()) as f32 }
}

/// A text the C++ writes into a sink (`ee_rmap_script_bytes_push`).
fn text(op: O, p: usize, a: &[i64]) -> Vec<u8> {
    let mut sink = EeRmapScriptBytes(Vec::new());
    let mut ai = vec![&mut sink as *mut EeRmapScriptBytes as usize as i64];
    ai.extend_from_slice(a);
    h(op, p, &ai);
    sink.0
}

/// `gSpaceTerrain`: the terrain named "Space" (kept from one map to the next, as the C++ global).
static SPACE_TERRAIN: AtomicI32 = AtomicI32::new(0);

/// `kTerrainCliff`, `kTerrainDeepWater`, `kTerrainShallowWater`, `kAIKindFortification`, the wall's mid-build
/// animation.
const TERRAIN_CLIFF: i32 = 9;
const TERRAIN_DEEP_WATER: i32 = 2;
const TERRAIN_SHALLOW_WATER: i32 = 1;
const AI_KIND_FORTIFICATION: i32 = 0x19;
const ANIMATION_WALL_BUILDING: i32 = 0xb;
/// `kRMMapPlanetsMap`, `kRMMapMarsMap`.
const MAP_PLANETS: u32 = 4;
const MAP_MARS: u32 = 0x10;
/// `kResourceFood`; the last resource type is 6.
const RESOURCE_FOOD: i32 = 2;

/// `EERMForest`.
#[derive(Clone, Debug, Default)]
pub struct Forest {
    pub tree_types: Vec<c_ulong>,
    pub id: c_long,
    pub terrain_texture: c_long,
}

/// `EERandomMapData`, the generator state, and the try's own members.
#[derive(Clone, Debug, Default)]
pub struct RandomMap {
    pub random: URandomState,
    pub random_integers: URandomState,
    /// `mScriptDirectory`: its volume and directory
    pub script_directory: Vec<u8>,
    pub kind_spacing: [[c_ulong; 2]; 7],
    pub flat_tiles: Grid<u8>,
    pub heights: Grid<f32>,
    pub tiles: Grid<u32>,
    pub claimed_tiles: Grid<u8>,
    pub players: Vec<Player>,
    pub valid_tiles: Grid<u8>,
    /// `mUnitNames`: name, unit class, size
    pub unit_names: Vec<(Vec<u8>, i32, u32)>,
    pub teams: Vec<Team>,
    pub objects: Vec<Object>,
    pub water_points: Vec<Point>,
    pub cliff_names: Vec<(Vec<u8>, i32)>,
    pub terrain_names: Vec<(Vec<u8>, i32)>,
    pub features: Vec<Feature>,
    pub walls: Vec<EeRmapScriptWall>,
    pub terrains: Vec<EeRmapScriptTerrain>,
    pub terrain_regions: Vec<EeRmapScriptRegion>,
    pub clump_circles: Vec<Vec<Circle>>,
    pub available_animals: Vec<c_ulong>,
    pub forests: Vec<Forest>,
    pub tree_types: Vec<i32>,
    pub resource_kinds: Vec<ResourceKind>,
    pub resource_kind_ids: Vec<i32>,
    pub resource_slots: Vec<ResourceSlot>,
    pub spacing_rules: Vec<SpacingRule>,
    pub flat_area: Rect,
    pub ring: Ring,
    pub neutral_ring: Ring,
    pub moon_ring: Ring,
    /// the block from `mMapSize` to `mIsSpaceMap`
    pub sc: EeRmapScriptScalars,
    /// BuildPlayerOrder's order (the adapter's globals, reset by every script read)
    pub player_order: Vec<i32>,
    pub player_order_index: u32,
}

impl RandomMap {
    /// The generator's settings (the adapter's `FillSettings`).
    pub fn settings(&self) -> Settings {
        let m = &self.sc;
        let mut s = Settings {
            random_seed: self.random.0.seed,
            random_integers_seed: self.random_integers.0.seed,
            player_order_index: self.player_order_index,
            seed: m.seed as u32,
            maximum_x: m.maximum_x as u32,
            maximum_y: m.maximum_y as u32,
            grid_size: m.grid_size as u32,
            tile_count: m.tile_count as u32,
            map_characteristics: m.map_characteristics as u32,
            river_width: m.river_width as u32,
            num_team_players: m.num_team_players as u32,
            num_moons: m.num_moons as u32,
            num_inner_neutrals: m.num_inner_neutrals as u32,
            num_outer_neutrals: m.num_outer_neutrals as u32,
            moon_size: m.moon_size as u32,
            moon_resources: m.moon_resources as u32,
            num_terrain_bands: m.num_terrain_bands as u32,
            terrain_band_offset: m.terrain_band_offset as u32,
            max_loop_count: m.max_loop_count as u32,
            max_tree_loop_count: m.max_tree_loop_count as u32,
            max_clumps_per_forest: m.max_clumps_per_forest as u32,
            forests_per_player: m.forests_per_player as u32,
            inner_neutral_flat_clumps: m.inner_neutral_flat_clumps as u32,
            inner_neutral_land_clumps: m.inner_neutral_land_clumps as u32,
            outer_neutral_flat_clumps: m.outer_neutral_flat_clumps as u32,
            outer_neutral_land_clumps: m.outer_neutral_land_clumps as u32,
            player_flat_clumps: m.player_flat_clumps as u32,
            player_land_clumps: m.player_land_clumps as u32,
            resource_to_edge_distance: m.resource_to_edge_distance as u32,
            resource_to_water_distance: m.resource_to_water_distance as u32,
            max_iterations_clump_growth: m.max_iterations_clump_growth as i32,
            water_border: m.water_border as i32,
            max_int_elevation: m.max_int_elevation as i32,
            min_int_elevation: m.min_int_elevation as i32,
            shallows_width: m.shallows_width as i32,
            max_shallow_connections_per_player: m.max_shallow_connections_per_player as i32,
            outer_push_from_map_center: m.outer_push_from_map_center as i32,
            inner_push_from_map_center: m.inner_push_from_map_center as i32,
            minimum_start_position_to_map_edge: m.minimum_start_position_to_map_edge as i32,
            coastal_to_deeper_water_percentage: m.coastal_to_deeper_water_percentage,
            convergent_sample_size: m.convergent_sample_size,
            convergent_threshold: m.convergent_threshold,
            elevation_scale: m.elevation_scale,
            elevation_unit: m.elevation_unit,
            flat_elevation: m.flat_elevation,
            forest_chaos_level: m.forest_chaos_level,
            forest_free_radius: m.forest_free_radius,
            initial_elevation: m.initial_elevation,
            inner_neutral_flat_chaos: m.inner_neutral_flat_chaos,
            inner_neutral_land_chaos: m.inner_neutral_land_chaos,
            inner_neutral_percent_flat: m.inner_neutral_percent_flat,
            inner_neutral_percent_land: m.inner_neutral_percent_land,
            inner_neutral_tree_percentage: m.inner_neutral_tree_percentage,
            land_elevation_threshold: m.land_elevation_threshold,
            max_angle_between_team_members: m.max_angle_between_team_members,
            max_resource_elevation: m.max_resource_elevation,
            max_tree_elevation: m.max_tree_elevation,
            outer_neutral_flat_chaos: m.outer_neutral_flat_chaos,
            outer_neutral_land_chaos: m.outer_neutral_land_chaos,
            outer_neutral_percent_flat: m.outer_neutral_percent_flat,
            outer_neutral_percent_land: m.outer_neutral_percent_land,
            outer_neutral_tree_percentage: m.outer_neutral_tree_percentage,
            percent_land: m.percent_land,
            player_flat_chaos: m.player_flat_chaos,
            player_land_chaos: m.player_land_chaos,
            player_percent_flat: m.player_percent_flat,
            player_tree_percentage: m.player_tree_percentage,
            shallows_elevation: m.shallows_elevation,
            height_map_chaos: m.height_map_chaos,
            landfill_percentage: m.landfill_percentage,
            resource_seperation: m.resource_seperation,
            half_map_size: m.half_map_size,
            flat_area: self.flat_area,
            ring: self.ring,
            neutral_ring: self.neutral_ring,
            moon_ring: self.moon_ring,
            team_growth: m.team_growth as u8,
            generate_shallows: m.generate_shallows as u8,
            is_space_map: m.is_space_map as u8,
            ..Settings::default()
        };
        for i in 0..7 {
            s.resource_per_player[i] = m.resource_per_player[i] as i32;
            s.resource_per_neutral_land[i] = m.resource_per_neutral_land[i] as u32;
            s.kind_spacing[i] = [self.kind_spacing[i][0] as u32, self.kind_spacing[i][1] as u32];
        }
        s
    }

    /// One generator step: the state as the adapter's `Flatten` handed it, and back what its `Unflatten` kept.
    pub fn run_step(&mut self, step: Step) {
        let mut map = Map {
            s: self.settings(),
            random: self.random,
            random_integers: self.random_integers,
            flat_tiles: std::mem::take(&mut self.flat_tiles),
            heights: std::mem::take(&mut self.heights),
            tiles: std::mem::take(&mut self.tiles),
            claimed_tiles: std::mem::take(&mut self.claimed_tiles),
            valid_tiles: self.valid_tiles.clone(),
            players: std::mem::take(&mut self.players),
            teams: self.teams.clone(),
            objects: std::mem::take(&mut self.objects),
            water_points: self.water_points.clone(),
            features: std::mem::take(&mut self.features),
            clump_circles: std::mem::take(&mut self.clump_circles),
            tree_types: self.tree_types.clone(),
            resource_kinds: self.resource_kinds.clone(),
            resource_kind_ids: self.resource_kind_ids.clone(),
            resource_slots: self.resource_slots.clone(),
            spacing_rules: self.spacing_rules.clone(),
            player_order: std::mem::take(&mut self.player_order),
        };
        run_step(&mut map, step);
        self.random = map.random;
        self.random_integers = map.random_integers;
        self.sc.num_team_players = map.s.num_team_players as c_ulong;
        self.flat_tiles = map.flat_tiles;
        self.claimed_tiles = map.claimed_tiles;
        self.heights = map.heights;
        self.tiles = map.tiles;
        self.players = map.players;
        for (team, out) in self.teams.iter_mut().zip(map.teams.iter()) {
            for (position, new) in team.positions.iter_mut().zip(out.positions.iter()) {
                *position = *new;
            }
        }
        self.objects = map.objects;
        self.features = map.features;
        self.clump_circles = map.clump_circles;
        self.player_order = map.player_order;
        self.player_order_index = map.s.player_order_index;
    }
}

/// The C++ world a try is made in: the context of `ee_rmap_cpp`.
#[derive(Clone, Copy)]
pub struct World {
    pub context: usize,
    /// the `EEMap*` (for the unit utilities' handles)
    pub map: usize,
}

impl World {
    fn player(&self, index: u32) -> usize {
        h(O::Player, self.context, &[index as i64]) as usize
    }
}

/// The game passes' host over the world, the water points kept in Rust.
struct PassHost<'a> {
    world: World,
    water_points: &'a mut Vec<Point>,
}

impl GameHost for PassHost<'_> {
    fn tile(&mut self, x: u32, y: u32) -> Option<EeRmapGameTile> {
        let tile = h(O::MapTile, self.world.context, &[x as i64, y as i64]) as usize;
        (tile != 0).then(|| EeRmapGameTile {
            terrain_type: h(O::TileTerrain, tile, &[]) as i32,
            height: hd(O::TileHeight, tile, &[]),
        })
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        h(O::MapIsEdgeTile, self.world.context, &[x as i64, y as i64]) != 0
    }
    fn paint_terrain(&mut self, x: i32, y: i32, texture: i32) {
        h(O::MapPaintTerrain, self.world.context, &[x as i64, y as i64, texture as i64]);
    }
    fn terrain(&mut self, texture: i32) -> Option<EeRmapGameTerrainInfo> {
        let table = h(O::Terrain, 0, &[texture as i64]) as usize;
        (table != 0).then(|| EeRmapGameTerrainInfo {
            unit_count: h(O::TerrainUnitCount, table, &[]) as u32,
            ambient_chance: h(O::TerrainAmbientChance, table, &[]) as i32,
        })
    }
    fn terrain_unit_class(&mut self, texture: i32, index: u32) -> i32 {
        let table = h(O::Terrain, 0, &[texture as i64]) as usize;
        h(O::TerrainUnitClass, table, &[index as i64]) as i32
    }
    fn mesh_z(&mut self, x: u32, y: u32) -> f32 {
        hd(O::MeshZ, self.world.context, &[x as i64, y as i64])
    }
    fn mesh_set_z(&mut self, x: u32, y: u32, z: f32) {
        hf(O::MeshSetZ, self.world.context, &[x as i64, y as i64], &[z]);
    }
    fn mesh_valid_point(&mut self, x: u32, y: u32) -> bool {
        h(O::MeshValidPoint, self.world.context, &[x as i64, y as i64]) != 0
    }
    fn mesh_valid_tile(&mut self, x: u32, y: u32) -> bool {
        h(O::MeshValidTile, self.world.context, &[x as i64, y as i64]) != 0
    }
    fn mesh_set_water_color_set(&mut self, x: u32, y: u32) {
        h(O::MeshWaterColorSet, self.world.context, &[x as i64, y as i64]);
    }
    fn mesh_vertex_normal(&mut self, x: u32, y: u32) {
        h(O::MeshVertexNormal, self.world.context, &[x as i64, y as i64]);
    }
    fn mesh_set_is_water(&mut self, x: u32, y: u32, is_water: bool) {
        h(O::MeshSetIsWater, self.world.context, &[x as i64, y as i64, is_water as i64]);
    }
    fn mesh_shorelines(&mut self, x: u32, y: u32) {
        h(O::MeshShorelines, self.world.context, &[x as i64, y as i64]);
    }
    fn set_tile_height(&mut self, x: u32, y: u32, height: f32) {
        let tile = h(O::MapTile, self.world.context, &[x as i64, y as i64]) as usize;
        hf(O::TileSetHeight, tile, &[], &[height]);
    }
    fn unit_class_count(&mut self) -> u32 {
        h(O::ClassCount, 0, &[]) as u32
    }
    fn unit_class(&mut self, class: u32) -> Option<EeRmapGameClass> {
        let count = h(O::ClassCount, 0, &[]) as u32;
        let c = if class < count { h(O::Class, 0, &[class as i64]) as usize } else { 0 };
        (c != 0).then(|| EeRmapGameClass {
            tile_size: h(O::ClassGetTileSize, c, &[]) as i32,
            is_land_citizen: h(O::ClassIsLandCitizen, c, &[]) as u8,
            is_wall: h(O::ClassIsWall, c, &[]) as u8,
        })
    }
    fn class_allows_terrain(&mut self, class: u32, terrain_type: i32) -> bool {
        let c = h(O::Class, 0, &[class as i64]) as usize;
        h(O::ClassAllowsTerrain, c, &[terrain_type as i64]) != 0
    }
    fn max_starting_citizens(&mut self, player: u32) -> i32 {
        h(O::PlayerMaxCitizens, self.world.player(player), &[]) as i32
    }
    fn create_unit_at_point(&mut self, owner: u32, class: u32, x: f32, y: f32, heading: f32) -> i32 {
        let c = h(O::Class, 0, &[class as i64]) as usize;
        let player = self.world.player(owner) as i64;
        let unit = hf(O::ClassCreateAtPoint, c, &[player, self.world.map as i64], &[x, y, heading]) as usize;
        if unit == 0 {
            return -1;
        }
        let complex = h(O::UnitComplex, unit, &[]) as usize;
        if complex != 0 {
            h(O::CxSetBuilt, complex, &[1, 0, 0]);
            if h(O::CxIsPaidFor, complex, &[]) == 0 {
                h(O::CxSetPaidFor, complex, &[]);
            }
            return -1;
        }
        let resource = h(O::UnitResourceType, unit, &[]) as i32;
        if (RESOURCE_FOOD..7).contains(&resource) && h(O::UnitResourceAmount, unit, &[]) > 0 {
            return resource;
        }
        -1
    }
    fn create_unit_at_tile(&mut self, owner: u32, class: u32, x: i32, y: i32, heading: f32) {
        let c = h(O::Class, 0, &[class as i64]) as usize;
        let player = self.world.player(owner) as i64;
        hf(O::ClassCreateAtTile, c, &[player, x as i64, y as i64, self.world.map as i64], &[heading]);
    }
    fn place_wall(&mut self, class: u32, start: Point, end: Point, player: u32) {
        let c = h(O::Class, 0, &[class as i64]) as usize;
        let player = self.world.player(player);
        place_wall(self.world, c, start, end, player);
    }
    fn wall_tiles(&mut self, start: Point, end: Point, out: &mut Vec<(i32, i32)>) {
        *out = wall_line_tiles(start, end).iter().map(|t| (t[0], t[1])).collect();
    }
    fn push_water_point(&mut self, point: Point) {
        self.water_points.push(point);
    }
}

/// A wall line's tiles (`EERMWallLineTiles`): a point every tile along the line, truncated to tiles, the diagonal
/// corners dropped (a line only), then the repeats (the unit utilities' notes 15 and 46, as `ee_units_uu_drop_tiles`
/// wrote them).
pub fn wall_line_tiles(start: Point, end: Point) -> Vec<Tile> {
    let mut tiles: Vec<Tile> = Vec::new();
    if start != end {
        // GE2DPoint::InterpolateTo forwards to ee_base's (not the generator's copy in rmap::math)
        let (from, to) = (Ge2DPoint::new(start.x, start.y), Ge2DPoint::new(end.x, end.y));
        from.interpolate_to(to, 1.0, |p| tiles.push([p.x as i32, p.y as i32]));
        drop_tiles(0, &mut tiles);
    } else {
        tiles.push([start.x as i32, start.y as i32]);
    }
    drop_tiles(1, &mut tiles);
    tiles
}

/// `ee_units_uu_drop_tiles` over a Rust list (its notes in its order).
fn drop_tiles(which: i32, tiles: &mut Vec<Tile>) {
    let n = Note::begin(if which == 0 { 15 } else { 46 });
    n.t(tiles);
    if which == 0 {
        uu_tiles::drop_diagonal_corners(tiles);
    } else {
        uu_tiles::drop_repeated_tiles(tiles);
    }
    n.t(tiles);
}

/// `EERandomMap::PlaceWall`: drops the cliff and edge tiles of the line, places the shaped pieces on the free dry tiles
/// and joins the player's fortifications already on it.
pub fn place_wall(world: World, class: usize, start: Point, end: Point, player: usize) {
    let c = world.context;
    let mut tiles = wall_line_tiles(start, end);
    let mut j = 0usize;
    while j < tiles.len() {
        let [x, y] = tiles[j];
        let tile = h(O::MapTile, c, &[x as i64, y as i64]) as usize;
        if tile == 0
            || h(O::TileTerrain, tile, &[]) as i32 == TERRAIN_CLIFF
            || h(O::MapIsEdgeTile, c, &[x as i64, y as i64]) != 0
        {
            tiles.remove(j);
        } else {
            j += 1;
        }
    }
    let corners = pick_wall_pieces(world.map as u64, &tiles);
    let mut units: Vec<u64> = Vec::new();
    let mut placed: Vec<Tile> = Vec::new();
    for (k, &[x, y]) in tiles.iter().enumerate() {
        let tile = h(O::MapTile, c, &[x as i64, y as i64]) as usize;
        if tile == 0 {
            continue;
        }
        if h(O::TileUnitNode, tile, &[]) == 0 {
            let terrain = h(O::TileTerrain, tile, &[]) as i32;
            if terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER {
                continue;
            }
            let unit =
                hf(O::ClassCreateAtTile, class, &[player as i64, x as i64, y as i64, world.map as i64], &[3.1415927]);
            let complex = h(O::UnitGetComplex, unit as usize, &[]) as usize;
            h(O::CxSetBuilt, complex, &[1, 0, 0]);
            placed.push([x, y]);
            let raised = h(O::MapRaisedCorners, c, &[x as i64, y as i64]) as i32;
            shape_wall(complex as u64, corners.get(k).copied().unwrap_or(0), raised);
        } else {
            let other = h(O::TileFirstUnit, tile, &[]) as usize;
            let complex = h(O::UnitGetComplex, other, &[]) as usize;
            let class_of = h(O::UnitClassOf, other, &[]) as usize;
            if h(O::UnitPlayer, other, &[]) as usize == player
                && h(O::ClassAIKind, class_of, &[]) as i32 == AI_KIND_FORTIFICATION
                && complex != 0
                && h(O::CxAnimation, complex, &[]) as i32 != ANIMATION_WALL_BUILDING
            {
                units.push(complex as u64);
            }
        }
    }
    make_wall_junctions(&units, &placed, player as u64, world.map as u64);
}

/// The unit utilities PlaceWall calls (their world is the game's): `EEPickWallPieces`, `EEMakeWallJunctions` and
/// `ee_units_uu_shape_wall` (its note 76 with it).
#[cfg(not(test))]
mod walls {
    use crate::units::uu;
    use crate::units::uu_host::Game;
    use crate::units::uu_tiles::Tile;

    pub fn pick_wall_pieces(map: u64, tiles: &[Tile]) -> Vec<i32> {
        uu::pick_wall_pieces(&mut Game, map, tiles)
    }
    pub fn make_wall_junctions(units: &[u64], placed: &[Tile], player: u64, map: u64) {
        uu::make_wall_junctions(&mut Game, units, placed, player, map)
    }
    pub fn shape_wall(complex: u64, corner: i32, raised: i32) {
        crate::units::uu_ffi::ee_units_uu_shape_wall(complex, corner, raised);
    }
}

/// The tests run no C++.
#[cfg(test)]
mod walls {
    use crate::units::uu_tiles::Tile;

    pub fn pick_wall_pieces(_: u64, _: &[Tile]) -> Vec<i32> {
        panic!("no C++ in the tests (pick_wall_pieces)")
    }
    pub fn make_wall_junctions(_: &[u64], _: &[Tile], _: u64, _: u64) {
        panic!("no C++ in the tests (make_wall_junctions)")
    }
    pub fn shape_wall(_: u64, _: i32, _: i32) {
        panic!("no C++ in the tests (shape_wall)")
    }
}
use walls::{make_wall_junctions, pick_wall_pieces, shape_wall};

/// The script reader's host: the files through the C++, the draws from the map's streams.
struct ReadHost<'a> {
    directory: &'a [u8],
    random: &'a mut URandom,
    random_integers: &'a mut URandom,
}

impl ScriptHost for ReadHost<'_> {
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
        let mut path = self.directory.to_vec();
        path.extend_from_slice(name);
        path.extend_from_slice(b".rmv\0");
        let mut sink = EeRmapScriptBytes(Vec::new());
        let found =
            h(O::ReadText, 0, &[path.as_ptr() as usize as i64, &mut sink as *mut EeRmapScriptBytes as usize as i64]);
        (found != 0).then_some(sink.0)
    }
    fn draw_long(&mut self, minimum: i32, maximum: i32) -> i32 {
        self.random_integers.generate_int(minimum, maximum)
    }
    fn draw_float(&mut self, minimum: f32, maximum: f32) -> f32 {
        self.random.generate_range(minimum, maximum)
    }
}

/// The rows of a flat grid, as the game passes read the C++ grids.
fn rows<T>(grid: &mut Grid<T>) -> Vec<*mut T> {
    let n = grid.size as usize;
    if grid.data.len() < n * n {
        return Vec::new();
    }
    let base = grid.data.as_mut_ptr();
    // SAFETY: row x starts x * n cells into the grid's n * n
    (0..n).map(|x| unsafe { base.add(x * n) }).collect()
}

impl RandomMap {
    /// `InitializeValues`: the seeds, the map dimensions and size class, the players, the start epoch, and the unit,
    /// terrain and cliff names (spaces removed) the scripts refer to.
    pub fn initialize_values(&mut self, world: World, seed: u32) {
        let c = world.context;
        self.sc.seed = seed as c_ulong;
        self.random.0.seed = seed;
        self.random_integers.0.seed = seed;
        let height = h(O::MapHeight, c, &[]);
        self.sc.maximum_y = (height << 1) as c_ulong;
        self.sc.maximum_x = (h(O::MapWidth, c, &[]) << 1) as c_ulong;
        self.script_directory = text(O::ScriptDirectory, 0, &[]);
        self.sc.tile_count = h(O::MapTileCount, c, &[]) as c_ulong;
        self.clump_circles.clear();
        self.sc.player_count = (h(O::PlayerCount, c, &[]) as c_long).wrapping_sub(1);
        self.sc.start_epoch = h(O::StartEpoch, c, &[]) as c_long;
        // the scripts' size class: that of the smallest map size (gameoptions.yaml) at least this high
        let height = height as u32 as c_ulong;
        let mut best: Option<(i64, i64)> = None;
        for i in 0..h(O::RuleMapSizeCount, 0, &[]) {
            let tiles = h(O::RuleMapSizeTiles, 0, &[i]);
            if tiles as u32 as c_ulong >= height && best.is_none_or(|(t, _)| tiles < t) {
                best = Some((tiles, h(O::RuleMapSizeScript, 0, &[i])));
            }
        }
        match best {
            Some((_, script)) => self.sc.map_size = script as c_long,
            None => panic!("Verification failure (no map size of the rules holds the map: EERandomMap.cpp 1735)"),
        }
        for i in 0..h(O::ClassCount, 0, &[]) {
            let class = h(O::Class, 0, &[i]) as usize;
            if class == 0 || h(O::ClassEnabled, class, &[]) == 0 {
                continue;
            }
            let Some(name) = script_name(&text(O::ClassName, class, &[])) else { continue };
            let size = h(O::ClassTileSizeMember, class, &[]) as i32;
            self.unit_names.push((name, i as i32, if size <= 1 { 1 } else { size as u32 }));
        }
        self.features.clear();
        self.terrain_names.clear();
        for i in 0..h(O::TerrainCount, 0, &[]) {
            let table = h(O::Terrain, 0, &[i]) as usize;
            if table == 0 {
                continue;
            }
            let Some(name) = script_name(&text(O::TerrainName, table, &[])) else { continue };
            if name.eq_ignore_ascii_case(b"Space") {
                SPACE_TERRAIN.store(i as i32, Ordering::Relaxed);
            }
            self.terrain_names.push((name, i as i32));
        }
        self.cliff_names.clear();
        for i in 0..h(O::CliffCount, 0, &[]) {
            let table = h(O::Cliff, 0, &[i]) as usize;
            if table == 0 {
                continue;
            }
            let Some(name) = script_name(&text(O::CliffName, table, &[])) else { continue };
            self.cliff_names.push((name, i as i32));
        }
    }

    /// The Default generator's `SetRandomMapValues`: resets the player order, then reads the script into the map (the
    /// defaults, the script, the rescaled elevations, the flat area, the forests' tree types).
    pub fn set_random_map_values(&mut self, script: &[u8]) {
        self.player_order.clear();
        self.player_order_index = 0;
        let tables = ScriptTables {
            units: self.unit_names.iter().map(|(n, _, size)| (n.clone(), *size)).collect(),
            terrains: self.terrain_names.clone(),
            cliffs: self.cliff_names.clone(),
        };
        let mut host = ReadHost {
            directory: &self.script_directory,
            random: &mut self.random.0,
            random_integers: &mut self.random_integers.0,
        };
        let lists = read_script(&mut host, &tables, &mut self.sc, &mut self.kind_spacing, script);
        self.flat_area.top = lists.flat_area[0];
        self.flat_area.left = lists.flat_area[1];
        self.flat_area.bottom = lists.flat_area[2];
        self.flat_area.right = lists.flat_area[3];
        self.features = lists
            .features
            .iter()
            .map(|f| Feature {
                angle: f.angle,
                position: Point::new(f.x, f.y),
                unit_type: f.unit_type as i32,
                index: f.index as i32,
                is_world_unit: f.is_world_unit as u8,
                in_ring: f.in_ring as u8,
                is_herd: f.is_herd as u8,
            })
            .collect();
        self.walls = lists.walls;
        self.terrains = lists.terrains;
        self.terrain_regions = lists.regions;
        self.available_animals = lists.animals;
        self.tree_types = lists.tree_types.iter().map(|&t| t as i32).collect();
        self.resource_kinds = lists
            .kinds
            .iter()
            .map(|k| ResourceKind {
                minimum: k.minimum as i32,
                maximum: k.maximum as i32,
                id: k.id as i32,
                unit_type: k.unit_type as i32,
                area: k.area as u32,
            })
            .collect();
        self.resource_kind_ids = lists.kind_ids.iter().map(|&k| k as i32).collect();
        self.resource_slots = lists
            .slots
            .iter()
            .map(|s| ResourceSlot { id: s.id as i32, per_player: s.per_player as u8, random_kind: s.random_kind as u8 })
            .collect();
        self.spacing_rules = lists
            .rules
            .iter()
            .map(|r| SpacingRule { kind: r.kind as i32, minimum: r.minimum as i32, maximum: r.maximum as i32 })
            .collect();
        self.forests = lists
            .forests
            .iter()
            .map(|f| Forest {
                tree_types: lists.forest_trees[f.first as usize..(f.first + f.count) as usize].to_vec(),
                id: f.id,
                terrain_texture: f.terrain_texture,
            })
            .collect();
    }

    /// `CreateGrids`: the grids at the power of two above the map (64 to 1024), at the initial elevation, and the
    /// player, neutral and moon rings.
    pub fn create_grids(&mut self, world: World) {
        let c = world.context;
        self.sc.grid_size =
            grid_size(h(O::MapHeight, c, &[]) as u32, h(O::MapWidth, c, &[]) as u32, self.sc.grid_size as u32)
                as c_ulong;
        let n = (self.sc.grid_size as u32).wrapping_add(1);
        self.claimed_tiles = Grid::new(n, 0);
        self.tiles = Grid::new(n, 0);
        self.heights = Grid::new(n, self.sc.initial_elevation);
        self.flat_tiles = Grid::new(n, 0);
        self.valid_tiles = Grid::new(n, 0);
        let radii = EeRmapGameRadii {
            player_inner: self.sc.player_inner_radius,
            player_outer: self.sc.player_outer_radius,
            neutral_inner: self.sc.neutral_inner_radius,
            neutral_outer: self.sc.neutral_outer_radius,
        };
        let r = rings(self.flat_area, h(O::MapSize, c, &[]) as i32, radii);
        self.sc.half_map_size = r.half_map_size;
        self.ring = r.ring;
        self.neutral_ring = r.neutral_ring;
        self.moon_ring = r.moon_ring;
    }

    /// `CreateValidMap`: which grid points are terrain-mesh points (the count logged).
    pub fn create_valid_map(&mut self, world: World) {
        let n = (self.sc.grid_size as u32).wrapping_add(1);
        let mut count = 0u32;
        for x in 0..n {
            for y in 0..n {
                let valid = h(O::MeshValidPoint, world.context, &[x as i64, y as i64]) as u8;
                self.valid_tiles.set(x, y, valid);
                if valid != 0 {
                    count += 1;
                }
            }
        }
        debug_out(&format!("EERandomMap::CreateValidMap {count} valid tiles found\n"));
    }

    /// `CreateTeams`: each player not yet placed starts a team with the later players it is not at war with.
    pub fn create_teams(&mut self, world: World) {
        self.teams.clear();
        let count = h(O::PlayerCount, world.context, &[]) as u32;
        let mut assigned = vec![false; count as usize];
        for i in 1..count {
            if assigned[i as usize] {
                continue;
            }
            let mut team = Team { players: vec![i as i32], positions: vec![Point::default()] };
            assigned[i as usize] = true;
            for j in i + 1..count {
                if assigned[j as usize] {
                    continue;
                }
                let id = h(O::PlayerId, world.player(j), &[]);
                if h(O::PlayerEnemy, world.player(i), &[id]) == 0 {
                    team.players.push(j as i32);
                    team.positions.push(Point::default());
                    assigned[j as usize] = true;
                }
            }
            self.teams.push(team);
        }
    }

    /// `SetUp`: the state from the game, the script's values, then the grids, the valid map and the teams.
    pub fn set_up(&mut self, world: World, script: &[u8], seed: u32) {
        self.initialize_values(world, seed);
        self.set_random_map_values(script);
        self.create_grids(world);
        self.create_valid_map(world);
        self.create_teams(world);
    }

    /// The state as the game passes read it (`EERMGameView`), with the buffers it points into.
    fn pass<R>(&mut self, world: World, pass: impl FnOnce(&mut Game, &mut PassHost) -> R) -> R {
        let mut heights = rows(&mut self.heights);
        let mut tiles = rows(&mut self.tiles);
        let mut claimed = rows(&mut self.claimed_tiles);
        let mut valid = rows(&mut self.valid_tiles);
        let players: Vec<EeRmapGamePlayer> = self
            .players
            .iter()
            .map(|p| EeRmapGamePlayer { position: p.position, index: p.index, is_neutral: p.is_neutral as u8 })
            .collect();
        let walls: Vec<EeRmapGameWall> = self
            .walls
            .iter()
            .map(|w| EeRmapGameWall {
                start_x: w.start_x as i32,
                start_y: w.start_y as i32,
                end_x: w.end_x as i32,
                end_y: w.end_y as i32,
                unit_type: w.unit_type as i32,
            })
            .collect();
        let unit_names: Vec<EeRmapGameUnitName> = self
            .unit_names
            .iter()
            .map(|(_, class, size)| EeRmapGameUnitName { unit_class: *class, size: *size })
            .collect();
        let terrains: Vec<EeRmapGameTerrain> = self
            .terrains
            .iter()
            .map(|t| EeRmapGameTerrain { textures: std::array::from_fn(|e| t.textures[e] as i32) })
            .collect();
        let regions: Vec<EeRmapGameRegion> = self
            .terrain_regions
            .iter()
            .map(|r| EeRmapGameRegion { from: r.from as i32, to: r.to as i32, terrain: r.terrain as u32 })
            .collect();
        let mut forest_tree_types: Vec<u32> = Vec::new();
        let forests: Vec<EeRmapGameForest> = self
            .forests
            .iter()
            .map(|f| {
                let r = EeRmapGameForest {
                    tree_types_start: forest_tree_types.len() as u32,
                    tree_types_len: f.tree_types.len() as u32,
                    terrain_texture: f.terrain_texture as i32,
                };
                forest_tree_types.extend(f.tree_types.iter().map(|&t| t as u32));
                r
            })
            .collect();
        let animals: Vec<u32> = self.available_animals.iter().map(|&a| a as u32).collect();
        let m = &self.sc;
        let state = EeRmapGameState {
            grid_n: self.tiles.size,
            heights: heights.as_mut_ptr(),
            tiles: tiles.as_mut_ptr(),
            claimed_tiles: claimed.as_mut_ptr(),
            valid_tiles: valid.as_mut_ptr(),
            grid_size: m.grid_size as u32,
            maximum_y: m.maximum_y as u32,
            map_characteristics: m.map_characteristics as u32,
            is_space_map: m.is_space_map as u8,
            half_map_size: m.half_map_size,
            water_border: m.water_border as i32,
            ring_center: self.ring.center,
            flat_area: self.flat_area,
            min_int_elevation: m.min_int_elevation as i32,
            max_int_elevation: m.max_int_elevation as i32,
            height_map_smoothness: m.height_map_smoothness,
            terrain_chaos: m.terrain_chaos,
            seed: m.seed as u32,
            num_terrain_bands: m.num_terrain_bands as u32,
            terrain_band_offset: m.terrain_band_offset as u32,
            max_loop_count: m.max_loop_count as u32,
            tile_unit: hd(O::MapTileUnit, world.context, &[]),
            space_terrain: SPACE_TERRAIN.load(Ordering::Relaxed),
            random_seed: self.random.0.seed,
            random_integers_seed: self.random_integers.0.seed,
            resource_terrain_texture: std::array::from_fn(|i| m.resource_terrain_texture[i] as i32),
            players: players.as_ptr(),
            players_len: players.len() as u32,
            features: self.features.as_ptr(),
            features_len: self.features.len() as u32,
            walls: walls.as_ptr(),
            walls_len: walls.len() as u32,
            unit_names: unit_names.as_ptr(),
            unit_names_len: unit_names.len() as u32,
            terrains: terrains.as_ptr(),
            terrains_len: terrains.len() as u32,
            terrain_regions: regions.as_ptr(),
            terrain_regions_len: regions.len() as u32,
            objects: self.objects.as_ptr(),
            objects_len: self.objects.len() as u32,
            forests: forests.as_ptr(),
            forests_len: forests.len() as u32,
            forest_tree_types: forest_tree_types.as_ptr(),
            forest_tree_types_len: forest_tree_types.len() as u32,
            available_animals: animals.as_ptr(),
            available_animals_len: animals.len() as u32,
        };
        let mut host = PassHost { world, water_points: &mut self.water_points };
        // SAFETY: the state's pointers are the buffers above and the object's grids and lists, alive and otherwise
        // untouched while the game lives (the host reaches the C++ only, and the water points are not in the state)
        let (result, seeds) = {
            let mut game = unsafe { Game::new(&state) };
            let result = pass(&mut game, &mut host);
            (result, (game.random.seed, game.random_integers.seed))
        };
        self.random.0.seed = seeds.0;
        self.random_integers.0.seed = seeds.1;
        result
    }

    /// `EERMArePlayersConnected`: every start dry, non-space land and no two players too close.
    pub fn players_connected(&mut self, world: World) -> bool {
        let seeds = (self.random.0.seed, self.random_integers.0.seed);
        let connected = self.pass(world, |g, host| g.players_connected(host));
        // (the C++ read a view of its own and kept its seeds)
        self.random.0.seed = seeds.0;
        self.random_integers.0.seed = seeds.1;
        connected
    }

    /// `Generate`: the generator's steps in order with the load progress and the game's passes between them; false
    /// when the players end up cut off (the C++'s 9).
    pub fn generate(&mut self, world: World) -> bool {
        let c = world.context;
        if self.sc.map_characteristics as u32 & MAP_PLANETS == MAP_PLANETS {
            let last = self.terrain_regions[self.terrain_regions.len() - 1].to as i32;
            self.sc.num_terrain_bands = (last.wrapping_add(1) as u32 as c_ulong) / 100;
            self.sc.terrain_band_offset =
                self.random_integers.generate_int(0, self.sc.num_terrain_bands as u32 as i32) as u32 as c_ulong;
        }
        h(O::SetUsingCarbon, 0, &[(self.sc.map_characteristics as u32 & MAP_MARS == MAP_MARS) as i64]);
        let progress = |value: f32| {
            hf(O::LoadProgress, 0, &[], &[value]);
        };
        self.run_step(Step::PlayerPositions);
        progress(0.1);
        self.run_step(Step::NeutralPositions);
        progress(0.14);
        self.run_step(Step::PlayerLandWater);
        progress(0.18);
        // (the neutral land and water step does nothing)
        progress(0.22);
        self.water_points.clear();
        self.pass(world, |g, host| g.water_points(host));
        self.run_step(Step::FlatMap);
        progress(0.26);
        self.run_step(Step::HeightMap);
        progress(0.3);
        self.run_step(Step::Shallows);
        progress(0.34);
        if self.sc.filter_height_map {
            self.run_step(Step::FilterHeightMap);
        }
        progress(0.38);
        self.pass(world, |g, host| g.smooth_heights(host));
        h(O::MapSetCliffTexture, c, &[self.sc.cliff_texture as i64]);
        self.pass(world, |g, host| g.apply_heights(host));
        if !self.players_connected(world) {
            return false;
        }
        let start = h(O::Time, 0, &[]) as u32;
        self.pass(world, |g, host| g.paint_tiles(host));
        let elapsed = (h(O::Time, 0, &[]) as u32).wrapping_sub(start) as f32 * 0.001f32;
        debug_out(&format!("EERandomMap::PaintTiles Processing completed in {:6.2} s\n", elapsed as f64));
        h(O::MapUpdateCliffs, c, &[]);
        self.pass(world, |g, host| g.mark_features(host));
        self.run_step(Step::Resources);
        progress(0.4 + 0.02);
        self.run_step(Step::Trees);
        progress(0.46);
        self.pass(world, |g, host| g.place_features(host));
        self.pass(world, |g, host| g.place_objects(host));
        h(O::MapSetCharacteristics, c, &[self.sc.map_characteristics as u32 as i64]);
        true
    }

    /// The instance `CreateRandomMap`: sets up and generates the map, and answers the player's start point (the
    /// centre for 0).
    pub fn create(&mut self, world: World, player: u32, script: &[u8], seed: u32) -> [f32; 3] {
        let c = world.context;
        let start = h(O::Time, 0, &[]) as u32;
        self.set_up(world, script, seed);
        self.generate(world);
        let location = if player > 0 {
            let p = self.players[player as usize - 1].position;
            let (x, y) = (p.x + 0.5, p.y + 0.5);
            let tile = h(
                O::MapTile,
                c,
                &[crate::rmap::math::floor_unsigned(x) as i64, crate::rmap::math::floor_unsigned(y) as i64],
            );
            [x, y, hd(O::TileHeight, tile as usize, &[])]
        } else {
            let size = h(O::MapSize, c, &[]) as i32;
            let middle = (size as f32 + 1.0) * 0.5;
            let tile = h(O::MapTile, c, &[(size / 2) as i64, (size / 2) as i64]);
            [middle, middle, hd(O::TileHeight, tile as usize, &[])]
        };
        let elapsed = (h(O::Time, 0, &[]) as u32).wrapping_sub(start) as f32 * 0.001f32;
        debug_out(&format!("EERandomMap::CreateRandomMap processing completed in {:6.2} s\n", elapsed as f64));
        location
    }
}

fn debug_out(line: &str) {
    let mut bytes = line.as_bytes().to_vec();
    bytes.push(0);
    h(O::DebugOut, 0, &[bytes.as_ptr() as usize as i64]);
}

/// `EERandomMap::Initialize`: the generator list, every `.rmv` script of `Data\Random Map Scripts` that names the
/// linked generator (Default), with its string table display name (else its name), sorted by display name (the C++'s
/// `std::sort`); at least one.
pub fn initialize_generators() {
    h(O::GeneratorsClear, 0, &[]);
    debug_out(
        "EERandomMap::Initialize: Default random map generator detected.
",
    );
    let find = h(O::FindNew, 0, &[]) as usize;
    if h(O::FindFirst, find, &[]) == 0 {
        loop {
            let mut sink = EeRmapScriptBytes(Vec::new());
            if h(O::FindText, find, &[&mut sink as *mut EeRmapScriptBytes as usize as i64]) != 0 {
                let file = text(O::FindName, find, &[]);
                if let Some(g) = script_generator(&file, &sink.0) {
                    // the valid plug-ins' find: a C string compare with the linked generator's name
                    if g.module.split(|&c| c == 0).next() == Some(&b"Default"[..]) {
                        let display = match g.string_id {
                            Some(id) => {
                                let mut out = EeRmapScriptBytes(Vec::new());
                                let ptr = &mut out as *mut EeRmapScriptBytes as usize as i64;
                                if h(O::ResourceString, 0, &[ptr, id as i64]) == 0 {
                                    out.0
                                } else {
                                    g.name.clone()
                                }
                            }
                            None => g.name.clone(),
                        };
                        let c = |b: &[u8]| {
                            let mut v = b.to_vec();
                            v.push(0);
                            v
                        };
                        let (name, module, display) = (c(&g.name), c(&g.module), c(&display));
                        let at = |v: &Vec<u8>| v.as_ptr() as usize as i64;
                        h(O::GeneratorPush, 0, &[at(&name), at(&module), at(&display)]);
                    }
                }
            }
            if h(O::FindNext, find, &[]) != 0 {
                break;
            }
        }
    }
    h(O::FindDelete, find, &[]);
    h(O::Verify, 0, &[(h(O::GeneratorCount, 0, &[]) > 0) as i64, 1652]);
    h(O::GeneratorsSort, 0, &[]);
}

/// `EERandomMap::Initialize` (its directory argument is unused, as the original's).
#[no_mangle]
pub extern "C" fn ee_rmap_initialize_generators() {
    initialize_generators()
}

/// The static `EERandomMap::CreateRandomMap`: the generator found by name (`generator` the C++'s `UWideString*`,
/// compared as the C++ did); 7 (`kErrorNoMoreFiles`) when its script names another generator, else the map made and
/// its start location in `out` -> 0.
///
/// # Safety
/// `out` has room for three floats; `generator` is a live `UWideString`; `context` is the world `ee_rmap_cpp` expects.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_create_random_map(
    context: *mut c_void,
    map: *mut c_void,
    player: u32,
    generator: *const c_void,
    seed: u32,
    out: *mut f32,
) -> i32 {
    let count = h(O::GeneratorCount, 0, &[]);
    let mut index = 0;
    while index < count {
        if h(O::GeneratorMatches, 0, &[index, generator as usize as i64]) != 0 {
            break;
        }
        index += 1;
    }
    h(O::Verify, 0, &[(index < count) as i64, 1340]);
    if h(O::GeneratorIsDefault, 0, &[index]) == 0 {
        return 7;
    }
    let script = text(O::GeneratorName, 0, &[index]);
    let location = create_random_map(World { context: context as usize, map: map as usize }, player, &script, seed);
    // SAFETY: the caller's guarantee
    unsafe { std::ptr::copy_nonoverlapping(location.as_ptr(), out, 3) };
    0
}

/// The static `EERandomMap::CreateRandomMap` after the generator's lookup: a new map per try, retried with a fresh
/// seed (11 tries at most) until the players are connected; the start location of the last try.
pub fn create_random_map(world: World, player: u32, script: &[u8], mut seed: u32) -> [f32; 3] {
    let mut attempts = 0u32;
    let mut location;
    loop {
        let mut map = Box::<RandomMap>::default();
        location = map.create(world, player, script, seed);
        let connected = map.players_connected(world);
        seed = map.random_integers.generate_int(0, 2000000) as u32;
        attempts += 1;
        if connected || attempts > 10 {
            break;
        }
    }
    debug_out(&format!("!Playable random map created after {attempts} attempts.\n"));
    location
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_step_keeps_what_the_adapter_copied_back() {
        let mut m = RandomMap::default();
        m.sc.grid_size = 64;
        m.sc.num_team_players = 3;
        m.random.0.seed = 7;
        m.random_integers.0.seed = 9;
        m.water_points = vec![Point::new(1.0, 2.0)];
        m.tree_types = vec![4];
        let s = m.settings();
        assert_eq!((s.grid_size, s.num_team_players, s.random_seed, s.random_integers_seed), (64, 3, 7, 9));
        // a step of no players changes nothing it does not own; the lists it does not return stay
        m.run_step(Step::Shallows);
        assert_eq!(m.water_points, vec![Point::new(1.0, 2.0)]);
        assert_eq!(m.tree_types, vec![4]);
    }

    #[test]
    fn wall_line_tiles_truncate_and_drop_repeats() {
        let tiles = wall_line_tiles(Point::new(1.5, 1.5), Point::new(1.5, 1.5));
        assert_eq!(tiles, vec![[1, 1]]);
        let tiles = wall_line_tiles(Point::new(0.0, 0.0), Point::new(3.0, 0.0));
        assert_eq!(tiles, vec![[0, 0], [1, 0], [2, 0], [3, 0]]);
    }

    #[test]
    fn rows_point_at_each_x() {
        let mut g = Grid::new(3, 0u8);
        g.set(2, 1, 5);
        let r = rows(&mut g);
        assert_eq!(r.len(), 3);
        // SAFETY: row 2 of a 3 x 3 grid
        assert_eq!(unsafe { *r[2].add(1) }, 5);
    }

    #[test]
    fn settings_carry_the_scalars_and_the_rings() {
        let mut m = RandomMap::default();
        m.sc.percent_land = 0.5;
        m.sc.is_space_map = true;
        m.kind_spacing[3] = [4, 5];
        m.ring.min_radius = 2.0;
        let s = m.settings();
        assert_eq!(s.percent_land, 0.5);
        assert_eq!(s.is_space_map, 1);
        assert_eq!(s.kind_spacing[3], [4, 5]);
        assert_eq!(s.ring.min_radius, 2.0);
    }
}
