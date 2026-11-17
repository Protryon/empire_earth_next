//! The random map script (`.rmv`) reader whole in Rust (data lane round 1; it was `Unit10.cpp`): the preprocessor
//! (`#define`, `#if`/`#elif`/`#else`/`#endif`, `#include`), the keyword tables, the value parser (`OneOf`,
//! `Between`, the `#if` conditions) and `SetRandomMapValues`' defaults and rescaling.
//!
//! The reader writes the map's scalar settings in place (`EeRmapScriptScalars`, the twin of `EERandomMap`'s block from
//! `mMapSize` to `mIsSpaceMap`) and its kind spacing, and returns the lists a script builds (features, walls,
//! terrains, forests, herds...) for the C++ host (`Default.cpp`) to store in the map. The host reads the script files
//! (`FSFileSpec` + `PlatFOpen` in text mode, `fread`) and draws the map's random numbers, so the draws are the
//! C++'s `URandomIntegers`/`URandom` in the original's order.
//!
//! Semantics kept from the C++ (simulation input: RUST_PLAN 2.1):
//! - lines are what `fgets` with a 0x200 buffer returned: at most 511 bytes, up to and including a newline, cut at a
//!   NUL; a line is split with `UString::NestedTokenize`/`Tokenize` (ee_ustring's own functions);
//! - numbers are `sscanf("%ld")` (a 32-bit `long`, as the Windows build: decimal only) and `sscanf("%f")` (the
//!   UCRT's: decimal, hexadecimal, `inf`, `nan`); a failed parse leaves the value as it was;
//! - the tables are `std::map`s that `insert` (a second `#define` of a name is ignored), looked up by exact bytes;
//! - `#if` conditions are tokenized flat (brackets are separators), as the original's were.
//!
//! The C++ read past a vector's end in a few malformed lines (an attribute or `#define` without a value, an empty
//! expression, a `TerrainRegion` with fewer than three values, a negative or too large resource kind): here those
//! read nothing (an empty value, no record). The `gReportErrors` diagnostics and the attribute/keyword handlers were
//! never set in the game and are gone.

// `c_long`/`c_ulong` are the build's: 32 bits on Windows, 64 on LP64, so their casts are needed on one of them
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void};

// value types of an attribute (InitializeDefines)
const LONG: i32 = 100;
const FLOAT: i32 = 101;
/// A list entry that failed to parse (`0xbaadf00d`, negative as the 32-bit original's).
const UNSET: i32 = 0xbaad_f00d_u32 as i32;
const OBJECT_KINDS: usize = 7;

// expression keywords (InitializeKeywords)
const KW_UNKNOWN: u32 = 0;
const KW_BETWEEN: u32 = 100;
const KW_ONE_OF: u32 = 101;
const KW_AND: u32 = 200;
const KW_OR: u32 = 203;
const KW_XOR: u32 = 204;
const KW_NOT: u32 = 205;
const KW_EQ: u32 = 206;
const KW_NEQ: u32 = 207;
const KW_LT: u32 = 208;
const KW_LTEQ: u32 = 209;
const KW_GT: u32 = 210;
const KW_GTEQ: u32 = 211;
const KW_PERCENT: u32 = 212;
const KW_DEFINED: u32 = 1000;
const KW_NOT_DEFINED: u32 = 1001;

const KEYWORDS: [(&str, u32); 15] = [
    ("Between", KW_BETWEEN),
    ("OneOf", KW_ONE_OF),
    ("AND", KW_AND),
    ("OR", KW_OR),
    ("XOR", KW_XOR),
    ("NOT", KW_NOT),
    ("EQ", KW_EQ),
    ("NEQ", KW_NEQ),
    ("LT", KW_LT),
    ("LTEQ", KW_LTEQ),
    ("GT", KW_GT),
    ("GTEQ", KW_GTEQ),
    ("PERCENT", KW_PERCENT),
    ("DEFINED", KW_DEFINED),
    ("NOTDEFINED", KW_NOT_DEFINED),
];

/// The attribute and directive tokens (`ee/EERandomMapIDs.h`, `EERMAttributeToken`).
mod tok {
    pub const DEFINE: u32 = 10;
    pub const IF: u32 = 11;
    pub const ELIF: u32 = 12;
    pub const ELSE: u32 = 13;
    pub const ENDIF: u32 = 14;
    pub const INCLUDE: u32 = 15;
    pub const MAX_INT_ELEVATION: u32 = 100;
    pub const MIN_INT_ELEVATION: u32 = 101;
    pub const INITIAL_ELEVATION: u32 = 102;
    pub const FLAT_ELEVATION: u32 = 103;
    pub const ELEVATION_SCALE: u32 = 104;
    pub const HEIGHT_MAP_CHAOS: u32 = 105;
    pub const HEIGHT_MAP_SMOOTHNESS: u32 = 106;
    pub const PERCENT_LAND: u32 = 200;
    pub const LAND_ELEVATION_THRESHOLD: u32 = 201;
    pub const WATER_BORDER: u32 = 202;
    pub const RIVER_WIDTH: u32 = 203;
    pub const COASTAL_TO_DEEPER_WATER_PERCENTAGE: u32 = 204;
    pub const TEAM_GROWTH: u32 = 205;
    pub const SHALLOWS_WIDTH: u32 = 206;
    pub const MAX_SHALLOW_CONNECTIONS_PER_PLAYER: u32 = 207;
    pub const GENERATE_SHALLOWS: u32 = 208;
    pub const SHALLOWS_ELEVATION: u32 = 209;
    pub const LANDFILL_PERCENTAGE: u32 = 210;
    pub const IS_SPACE_MAP: u32 = 211;
    pub const NUM_MOONS: u32 = 212;
    pub const MOON_SIZE: u32 = 213;
    pub const MOON_RESOURCES: u32 = 214;
    pub const PLAYER_INNER_RADIUS: u32 = 300;
    pub const PLAYER_OUTER_RADIUS: u32 = 301;
    pub const PLAYER_PERCENT_FLAT: u32 = 303;
    pub const PLAYER_TREE_PERCENTAGE: u32 = 304;
    pub const PLAYER_FLAT_CHAOS: u32 = 305;
    pub const PLAYER_FLAT_CLUMPS: u32 = 306;
    pub const PLAYER_LAND_CHAOS: u32 = 307;
    pub const PLAYER_LAND_CLUMPS: u32 = 308;
    pub const MAX_ANGLE_BETWEEN_TEAM_MEMBERS: u32 = 309;
    pub const OUTER_PUSH_FROM_MAP_CENTER: u32 = 310;
    pub const INNER_PUSH_FROM_MAP_CENTER: u32 = 311;
    pub const MINIMUM_START_POSITION_TO_MAP_EDGE: u32 = 312;
    pub const NEUTRAL_INNER_RADIUS: u32 = 400;
    pub const NEUTRAL_OUTER_RADIUS: u32 = 401;
    pub const NUM_OUTER_NEUTRALS: u32 = 500;
    pub const OUTER_NEUTRAL_PERCENT_LAND: u32 = 501;
    pub const OUTER_NEUTRAL_PERCENT_FLAT: u32 = 502;
    pub const OUTER_NEUTRAL_TREE_PERCENTAGE: u32 = 503;
    pub const OUTER_NEUTRAL_LAND_CHAOS: u32 = 504;
    pub const OUTER_NEUTRAL_LAND_CLUMPS: u32 = 505;
    pub const OUTER_NEUTRAL_FLAT_CHAOS: u32 = 506;
    pub const OUTER_NEUTRAL_FLAT_CLUMPS: u32 = 507;
    pub const NUM_INNER_NEUTRALS: u32 = 600;
    pub const INNER_NEUTRAL_PERCENT_LAND: u32 = 601;
    pub const INNER_NEUTRAL_PERCENT_FLAT: u32 = 602;
    pub const INNER_NEUTRAL_TREE_PERCENTAGE: u32 = 603;
    pub const INNER_NEUTRAL_LAND_CHAOS: u32 = 604;
    pub const INNER_NEUTRAL_LAND_CLUMPS: u32 = 605;
    pub const INNER_NEUTRAL_FLAT_CHAOS: u32 = 606;
    pub const INNER_NEUTRAL_FLAT_CLUMPS: u32 = 607;
    pub const FOREST_FREE_RADIUS: u32 = 700;
    pub const FORESTS_PER_PLAYER: u32 = 701;
    pub const FOREST_CHAOS_LEVEL: u32 = 702;
    pub const MAX_CLUMPS_PER_FOREST: u32 = 703;
    pub const MAX_TREE_LOOP_COUNT: u32 = 704;
    pub const MAXIMUM_TREES_PER_CLUMP: u32 = 705;
    pub const DEFINE_FOREST: u32 = 706;
    pub const AVAILABLE_FORESTS: u32 = 707;
    pub const RESOURCE_PER_PLAYER: u32 = 800;
    pub const RESOURCE_PER_NEUTRAL_LAND: u32 = 801;
    pub const MAX_RESOURCE_ELEVATION: u32 = 802;
    pub const RESOURCE_TO_EDGE_DISTANCE: u32 = 803;
    pub const RESOURCE_TO_WATER_DISTANCE: u32 = 804;
    pub const AVAILABLE_ANIMALS: u32 = 805;
    pub const RESOURCE_PLACEMENT_LIMIT: u32 = 806;
    pub const DEFINE_HERD: u32 = 807;
    pub const DEFINE_RANDOM_HERD_SET: u32 = 808;
    pub const HERD_PER_PLAYER: u32 = 809;
    pub const HERD_PER_NEUTRAL: u32 = 810;
    pub const RANDOM_HERD_PER_PLAYER: u32 = 811;
    pub const RANDOM_HERD_PER_NEUTRAL: u32 = 812;
    pub const MAX_TREE_ELEVATION: u32 = 813;
    pub const RESOURCE_SEPERATION: u32 = 814;
    pub const RESOURCE_TERRAIN_TEXTURE: u32 = 815;
    pub const PLACE_RESOURCE_IN_RING: u32 = 816;
    pub const PLACE_UNIT: u32 = 900;
    pub const PLACE_WALL: u32 = 901;
    pub const PLACE_WORLD_UNIT: u32 = 902;
    pub const PLACE_UNIT_IN_RING: u32 = 903;
    pub const PLACE_WORLD_UNIT_IN_RING: u32 = 904;
    pub const TERRAIN_REGION: u32 = 1000;
    pub const TERRAIN_CHAOS: u32 = 1001;
    pub const CLIFF_TEXTURE: u32 = 1002;
    pub const DECLARE_RANDOM_MAP_TERRAIN: u32 = 1003;
    pub const RANDOM_MAP_TERRAIN_RANGE: u32 = 1004;
    pub const CONVERGENT_SAMPLE_SIZE: u32 = 1100;
    pub const CONVERGENT_THRESHOLD: u32 = 1101;
    pub const MAX_LOOP_COUNT: u32 = 1102;
    pub const MAX_ITERATIONS_CLUMP_GROWTH: u32 = 1103;
    pub const USE_MAP_GENERATOR: u32 = 1104;
    pub const MAP_CHARACTERISTICS: u32 = 1105;
    pub const STRING_TABLE_ID: u32 = 1106;
    pub const FILTER_HEIGHT_MAP: u32 = 10000;
    pub const TESTING_LAND_DISTRIBUTION: u32 = 10001;
}

/// The script's attribute and directive names with their tokens and value types (InitializeDefines).
const DEFINES: [(&str, u32, i32); 102] = [
    ("#define", tok::DEFINE, 0),
    ("#if", tok::IF, 0),
    ("#elif", tok::ELIF, 0),
    ("#else", tok::ELSE, 0),
    ("#endif", tok::ENDIF, 0),
    ("#include", tok::INCLUDE, 0),
    ("MaxIntElevation", tok::MAX_INT_ELEVATION, LONG),
    ("MinIntElevation", tok::MIN_INT_ELEVATION, LONG),
    ("InitialElevation", tok::INITIAL_ELEVATION, LONG),
    ("FlatElevation", tok::FLAT_ELEVATION, LONG),
    ("ElevationScale", tok::ELEVATION_SCALE, FLOAT),
    ("HeightMapChaos", tok::HEIGHT_MAP_CHAOS, FLOAT),
    ("HeightMapSmoothness", tok::HEIGHT_MAP_SMOOTHNESS, FLOAT),
    ("PercentLand", tok::PERCENT_LAND, FLOAT),
    ("LandElevationThreshold", tok::LAND_ELEVATION_THRESHOLD, LONG),
    ("WaterBorder", tok::WATER_BORDER, LONG),
    ("RiverWidth", tok::RIVER_WIDTH, LONG),
    ("CoastalToDeeperWaterPercentage", tok::COASTAL_TO_DEEPER_WATER_PERCENTAGE, FLOAT),
    ("TeamGrowth", tok::TEAM_GROWTH, LONG),
    ("LandfillPercentage", tok::LANDFILL_PERCENTAGE, FLOAT),
    ("PlayerInnerRadius", tok::PLAYER_INNER_RADIUS, FLOAT),
    ("PlayerOuterRadius", tok::PLAYER_OUTER_RADIUS, FLOAT),
    ("PlayerPercentFlat", tok::PLAYER_PERCENT_FLAT, FLOAT),
    ("PlayerTreePercentage", tok::PLAYER_TREE_PERCENTAGE, FLOAT),
    ("PlayerFlatChaos", tok::PLAYER_FLAT_CHAOS, FLOAT),
    ("PlayerFlatClumps", tok::PLAYER_FLAT_CLUMPS, LONG),
    ("PlayerLandChaos", tok::PLAYER_LAND_CHAOS, FLOAT),
    ("PlayerLandClumps", tok::PLAYER_LAND_CLUMPS, LONG),
    ("MaxAngleBetweenTeamMembers", tok::MAX_ANGLE_BETWEEN_TEAM_MEMBERS, FLOAT),
    ("NeutralInnerRadius", tok::NEUTRAL_INNER_RADIUS, FLOAT),
    ("NeutralOuterRadius", tok::NEUTRAL_OUTER_RADIUS, FLOAT),
    ("NumOuterNeutrals", tok::NUM_OUTER_NEUTRALS, LONG),
    ("OuterNeutralPercentLand", tok::OUTER_NEUTRAL_PERCENT_LAND, FLOAT),
    ("OuterNeutralPercentFlat", tok::OUTER_NEUTRAL_PERCENT_FLAT, FLOAT),
    ("OuterNeutralTreePercentage", tok::OUTER_NEUTRAL_TREE_PERCENTAGE, FLOAT),
    ("OuterNeutralLandChaos", tok::OUTER_NEUTRAL_LAND_CHAOS, FLOAT),
    ("OuterNeutralLandClumps", tok::OUTER_NEUTRAL_LAND_CLUMPS, LONG),
    ("OuterNeutralFlatChaos", tok::OUTER_NEUTRAL_FLAT_CHAOS, FLOAT),
    ("OuterNeutralFlatClumps", tok::OUTER_NEUTRAL_FLAT_CLUMPS, LONG),
    ("NumInnerNeutrals", tok::NUM_INNER_NEUTRALS, LONG),
    ("InnerNeutralPercentLand", tok::INNER_NEUTRAL_PERCENT_LAND, FLOAT),
    ("InnerNeutralPercentFlat", tok::INNER_NEUTRAL_PERCENT_FLAT, FLOAT),
    ("InnerNeutralTreePercentage", tok::INNER_NEUTRAL_TREE_PERCENTAGE, FLOAT),
    ("InnerNeutralLandChaos", tok::INNER_NEUTRAL_LAND_CHAOS, FLOAT),
    ("InnerNeutralLandClumps", tok::INNER_NEUTRAL_LAND_CLUMPS, LONG),
    ("InnerNeutralFlatChaos", tok::INNER_NEUTRAL_FLAT_CHAOS, FLOAT),
    ("InnerNeutralFlatClumps", tok::INNER_NEUTRAL_FLAT_CLUMPS, LONG),
    ("NumMoons", tok::NUM_MOONS, LONG),
    ("MoonSize", tok::MOON_SIZE, LONG),
    ("MoonResources", tok::MOON_RESOURCES, LONG),
    ("ForestFreeRadius", tok::FOREST_FREE_RADIUS, FLOAT),
    ("ForestsPerPlayer", tok::FORESTS_PER_PLAYER, LONG),
    ("ForestChaosLevel", tok::FOREST_CHAOS_LEVEL, FLOAT),
    ("MaxClumpsPerForest", tok::MAX_CLUMPS_PER_FOREST, LONG),
    ("ResourceToEdgeDistance", tok::RESOURCE_TO_EDGE_DISTANCE, LONG),
    ("ResourceToWaterDistance", tok::RESOURCE_TO_WATER_DISTANCE, LONG),
    ("MaxTreeLoopCount", tok::MAX_TREE_LOOP_COUNT, LONG),
    ("MaximumTreesPerClump", tok::MAXIMUM_TREES_PER_CLUMP, LONG),
    ("DefineForest", tok::DEFINE_FOREST, LONG),
    ("AvailableForests", tok::AVAILABLE_FORESTS, LONG),
    ("ResourcePerPlayer", tok::RESOURCE_PER_PLAYER, LONG),
    ("ResourcePerNeutralLand", tok::RESOURCE_PER_NEUTRAL_LAND, LONG),
    ("MaxResourceElevation", tok::MAX_RESOURCE_ELEVATION, LONG),
    ("MaxTreeElevation", tok::MAX_TREE_ELEVATION, LONG),
    ("ResourcePlacementLimit", tok::RESOURCE_PLACEMENT_LIMIT, LONG),
    ("ResourceSeperation", tok::RESOURCE_SEPERATION, FLOAT),
    ("ResourceTerrainTexture", tok::RESOURCE_TERRAIN_TEXTURE, LONG),
    ("PlaceResourceInRing", tok::PLACE_RESOURCE_IN_RING, LONG),
    ("DefineHerd", tok::DEFINE_HERD, LONG),
    ("DefineRandomHerdSet", tok::DEFINE_RANDOM_HERD_SET, LONG),
    ("HerdPerPlayer", tok::HERD_PER_PLAYER, LONG),
    ("HerdPerNeutral", tok::HERD_PER_NEUTRAL, LONG),
    ("RandomHerdPerPlayer", tok::RANDOM_HERD_PER_PLAYER, LONG),
    ("RandomHerdPerNeutral", tok::RANDOM_HERD_PER_NEUTRAL, LONG),
    ("ConvergentSampleSize", tok::CONVERGENT_SAMPLE_SIZE, FLOAT),
    ("ConvergentThreshold", tok::CONVERGENT_THRESHOLD, FLOAT),
    ("MaxLoopCount", tok::MAX_LOOP_COUNT, LONG),
    ("MaxIterationsClumpGrowth", tok::MAX_ITERATIONS_CLUMP_GROWTH, LONG),
    ("FilterHeightMap", tok::FILTER_HEIGHT_MAP, LONG),
    ("TestingLandDistribution", tok::TESTING_LAND_DISTRIBUTION, LONG),
    ("TerrainRegion", tok::TERRAIN_REGION, LONG),
    ("TerrainChaos", tok::TERRAIN_CHAOS, FLOAT),
    ("CliffTexture", tok::CLIFF_TEXTURE, LONG),
    ("DeclareRandomMapTerrain", tok::DECLARE_RANDOM_MAP_TERRAIN, LONG),
    ("RandomMapTerrainRange", tok::RANDOM_MAP_TERRAIN_RANGE, LONG),
    ("PlaceUnit", tok::PLACE_UNIT, FLOAT),
    ("PlaceWall", tok::PLACE_WALL, LONG),
    ("PlaceWorldUnit", tok::PLACE_WORLD_UNIT, FLOAT),
    ("PlaceUnitInRing", tok::PLACE_UNIT_IN_RING, FLOAT),
    ("PlaceWorldUnitInRing", tok::PLACE_WORLD_UNIT_IN_RING, FLOAT),
    ("UseMapGenerator", tok::USE_MAP_GENERATOR, 0),
    ("StringTableID", tok::STRING_TABLE_ID, 0),
    ("ShallowsElevation", tok::SHALLOWS_ELEVATION, LONG),
    ("ShallowsWidth", tok::SHALLOWS_WIDTH, LONG),
    ("MaxShallowConnectionsPerPlayer", tok::MAX_SHALLOW_CONNECTIONS_PER_PLAYER, LONG),
    ("GenerateShallows", tok::GENERATE_SHALLOWS, LONG),
    ("IsSpaceMap", tok::IS_SPACE_MAP, LONG),
    ("AvailableAnimals", tok::AVAILABLE_ANIMALS, LONG),
    ("OuterPushFromMapCenter", tok::OUTER_PUSH_FROM_MAP_CENTER, LONG),
    ("InnerPushFromMapCenter", tok::INNER_PUSH_FROM_MAP_CENTER, LONG),
    ("MinimumStartPositionToMapEdge", tok::MINIMUM_START_POSITION_TO_MAP_EDGE, LONG),
    ("MapCharacteristics", tok::MAP_CHARACTERISTICS, LONG),
];

/// The script constants that are not names from the tables (InitializeMapTypes' first part, in its order).
const MAP_TYPES: [(&str, &str); 32] = [
    ("IslandMap", "1"),
    ("WaterlessMap", "2"),
    ("PlanetsMap", "4"),
    ("EarthMap", "8"),
    ("MarsMap", "16"),
    ("true", "1"),
    ("false", "0"),
    ("Infinity", "0xFFFFFFFF"),
    ("TinyMap", "0"),
    ("SmallMap", "1"),
    ("MediumMap", "2"),
    ("LargeMap", "3"),
    ("HugeMap", "4"),
    ("GiganticMap", "5"),
    ("Stone", "0"),
    ("Gold", "1"),
    ("Tree", "2"),
    ("Berry", "3"),
    ("Animal", "4"),
    ("Fish", "5"),
    ("Steel", "6"),
    ("Iron", "6"),
    ("PaleoEpoch", "0"),
    ("StoneEpoch", "1"),
    ("CopperEpoch", "2"),
    ("BronzeEpoch", "3"),
    ("DarkEpoch", "4"),
    ("MiddleEpoch", "5"),
    ("RenaissanceEpoch", "6"),
    ("ImperialEpoch", "7"),
    ("IndustrialEpoch", "8"),
    ("WW1Epoch", "9"),
];
const MAP_TYPES_2: [(&str, &str); 5] =
    [("WW2Epoch", "10"), ("ModernEpoch", "11"), ("InformationEpoch", "12"), ("NanoEpoch", "13"), ("SpaceEpoch", "14")];

const DEGREES: f32 = 0.017453292;
const PI: f32 = 3.1415927;

// ---------------------------------------------------------------------------------------------- the records

/// `EERandomMap`'s scalar block from `mMapSize` (+0x260) to `mIsSpaceMap` (+0x3f8), field for field (`long` is the
/// build's: `c_long`). `Default.cpp` checks the layout against the C++ struct.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptScalars {
    pub map_size: c_long,
    pub start_epoch: c_long,
    pub player_count: c_long,
    pub coastal_to_deeper_water_percentage: f32,
    pub convergent_sample_size: f32,
    pub convergent_threshold: f32,
    pub elevation_scale: f32,
    pub elevation_unit: f32,
    pub flat_elevation: f32,
    pub forest_chaos_level: f32,
    pub forest_free_radius: f32,
    pub initial_elevation: f32,
    pub inner_neutral_flat_chaos: f32,
    pub inner_neutral_land_chaos: f32,
    pub inner_neutral_percent_flat: f32,
    pub inner_neutral_percent_land: f32,
    pub inner_neutral_tree_percentage: f32,
    pub land_elevation_threshold: f32,
    pub max_angle_between_team_members: f32,
    pub max_resource_elevation: f32,
    pub max_tree_elevation: f32,
    pub neutral_inner_radius: f32,
    pub neutral_outer_radius: f32,
    pub outer_neutral_flat_chaos: f32,
    pub outer_neutral_land_chaos: f32,
    pub outer_neutral_percent_flat: f32,
    pub outer_neutral_percent_land: f32,
    pub outer_neutral_tree_percentage: f32,
    pub percent_land: f32,
    pub player_flat_chaos: f32,
    pub player_inner_radius: f32,
    pub player_land_chaos: f32,
    pub player_outer_radius: f32,
    pub player_percent_flat: f32,
    pub player_tree_percentage: f32,
    pub terrain_chaos: f32,
    pub shallows_elevation: f32,
    pub height_map_chaos: f32,
    pub height_map_smoothness: f32,
    pub landfill_percentage: f32,
    pub resource_seperation: f32,
    pub half_map_size: f32,
    pub max_int_elevation: c_long,
    pub max_iterations_clump_growth: c_long,
    pub maximum_trees_per_clump: c_long,
    pub min_int_elevation: c_long,
    pub water_border: c_long,
    pub shallows_width: c_long,
    pub max_shallow_connections_per_player: c_long,
    pub outer_push_from_map_center: c_long,
    pub inner_push_from_map_center: c_long,
    pub minimum_start_position_to_map_edge: c_long,
    pub resource_terrain_texture: [c_long; 7],
    pub resource_per_player: [c_long; 7],
    pub resource_per_neutral_land: [c_ulong; 7],
    pub forests_per_player: c_ulong,
    pub inner_neutral_flat_clumps: c_ulong,
    pub inner_neutral_land_clumps: c_ulong,
    pub maximum_y: c_ulong,
    pub grid_size: c_ulong,
    pub maximum_x: c_ulong,
    pub max_clumps_per_forest: c_ulong,
    pub max_loop_count: c_ulong,
    pub max_tree_loop_count: c_ulong,
    pub num_inner_neutrals: c_ulong,
    pub num_outer_neutrals: c_ulong,
    pub num_team_players: c_ulong,
    pub outer_neutral_flat_clumps: c_ulong,
    pub outer_neutral_land_clumps: c_ulong,
    pub player_flat_clumps: c_ulong,
    pub player_land_clumps: c_ulong,
    pub seed: c_ulong,
    pub river_width: c_ulong,
    pub tile_count: c_ulong,
    pub resource_to_edge_distance: c_ulong,
    pub resource_to_water_distance: c_ulong,
    pub map_characteristics: c_ulong,
    pub cliff_texture: c_long,
    pub num_moons: c_ulong,
    pub moon_size: c_ulong,
    pub moon_resources: c_ulong,
    pub num_terrain_bands: c_ulong,
    pub terrain_band_offset: c_ulong,
    pub team_growth: bool,
    pub filter_height_map: bool,
    pub testing_land_distribution: bool,
    pub generate_shallows: bool,
    pub is_space_map: bool,
}

/// `EERMFeature`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptFeature {
    pub angle: f32,
    pub x: f32,
    pub y: f32,
    pub unit_type: c_long,
    pub index: c_long,
    pub is_world_unit: bool,
    pub in_ring: bool,
    pub is_herd: bool,
}

/// `EERMWall`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptWall {
    pub start_x: c_long,
    pub start_y: c_long,
    pub end_x: c_long,
    pub end_y: c_long,
    pub unit_type: c_long,
}

/// `EERMTerrain`'s twin: a terrain id and its texture per elevation -10..20.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeRmapScriptTerrain {
    pub id: c_long,
    pub textures: [c_long; 31],
}

/// `EERMTerrainRegion`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptRegion {
    pub from: c_long,
    pub to: c_long,
    pub terrain: c_ulong,
}

/// `EERMResourceKind`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptKind {
    pub minimum: c_long,
    pub maximum: c_long,
    pub id: c_long,
    pub unit_type: c_long,
    pub area: c_ulong,
}

/// `EERMResourceSlot`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptSlot {
    pub id: c_long,
    pub per_player: bool,
    pub random_kind: bool,
}

/// `EERMSpacingRule`'s twin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptRule {
    pub kind: c_long,
    pub minimum: c_long,
    pub maximum: c_long,
}

/// One `EERMForest`: its tree types are `count` values of the output's `forest_trees` from `first`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeRmapScriptForest {
    pub id: c_long,
    pub terrain_texture: c_long,
    pub first: u32,
    pub count: u32,
}

/// What a script read builds besides the scalars and the kind spacing (the map's lists, replaced whole).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct ScriptLists {
    pub features: Vec<EeRmapScriptFeature>,
    pub walls: Vec<EeRmapScriptWall>,
    pub terrains: Vec<EeRmapScriptTerrain>,
    pub regions: Vec<EeRmapScriptRegion>,
    pub animals: Vec<c_ulong>,
    pub forests: Vec<EeRmapScriptForest>,
    pub forest_trees: Vec<c_ulong>,
    pub tree_types: Vec<c_long>,
    pub kinds: Vec<EeRmapScriptKind>,
    pub kind_ids: Vec<c_long>,
    pub slots: Vec<EeRmapScriptSlot>,
    pub rules: Vec<EeRmapScriptRule>,
    /// `mFlatArea`: top, left, bottom, right
    pub flat_area: [f32; 4],
}

/// The map's tables the script names (InitializeMapTypes): units by index (with their `mSize`), terrains and
/// cliffs by value.
#[derive(Clone, Debug, Default)]
pub struct ScriptTables {
    pub units: Vec<(Vec<u8>, u32)>,
    pub terrains: Vec<(Vec<u8>, i32)>,
    pub cliffs: Vec<(Vec<u8>, i32)>,
}

/// What the reader needs from the game.
pub trait ScriptHost {
    /// The text of `<script directory>/<name>.rmv` as the C runtime's text mode reads it, `None` when it is missing.
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>>;
    /// `mRandomIntegers.Generate(minimum, maximum)`.
    fn draw_long(&mut self, minimum: i32, maximum: i32) -> i32;
    /// `mRandom.Generate(minimum, maximum)`.
    fn draw_float(&mut self, minimum: f32, maximum: f32) -> f32;
}

// ---------------------------------------------------------------------------------------------- the parsers

/// `sscanf(text, "%ld")` with a 32-bit `long`: `None` when no number starts the text.
pub fn scan_long(text: &[u8]) -> Option<i32> {
    let mut i = skip_space(text, 0);
    let negative = match text.get(i) {
        Some(b'-') => {
            i += 1;
            true
        }
        Some(b'+') => {
            i += 1;
            false
        }
        _ => false,
    };
    let start = i;
    let mut value: u64 = 0;
    while let Some(&c) = text.get(i).filter(|c| c.is_ascii_digit()) {
        value = value.wrapping_mul(10).wrapping_add(u64::from(c - b'0'));
        i += 1;
    }
    if i == start {
        return None;
    }
    Some((if negative { value.wrapping_neg() } else { value }) as u32 as i32)
}

fn skip_space(text: &[u8], mut i: usize) -> usize {
    while matches!(text.get(i), Some(b' ' | b'\t' | b'\n' | b'\x0b' | b'\x0c' | b'\r')) {
        i += 1;
    }
    i
}

fn starts_with_ignore_case(text: &[u8], word: &[u8]) -> bool {
    text.len() >= word.len() && text[..word.len()].eq_ignore_ascii_case(word)
}

/// `sscanf(text, "%f")` (the UCRT's: decimal, hexadecimal, `inf`/`infinity`, `nan`), `None` when no number starts
/// the text. A decimal number is rounded once to `f32`, as `strtof` does.
pub fn scan_float(text: &[u8]) -> Option<f32> {
    let mut i = skip_space(text, 0);
    let negative = match text.get(i) {
        Some(b'-') => {
            i += 1;
            true
        }
        Some(b'+') => {
            i += 1;
            false
        }
        _ => false,
    };
    let rest = &text[i..];
    let magnitude = if starts_with_ignore_case(rest, b"inf") {
        f32::INFINITY
    } else if starts_with_ignore_case(rest, b"nan") {
        f32::NAN
    } else if rest.len() > 2 && rest[0] == b'0' && (rest[1] | 0x20) == b'x' && scan_hex(&rest[2..]).is_some() {
        scan_hex(&rest[2..])?
    } else {
        let mut j = 0;
        let mut digits = 0;
        while rest.get(j).is_some_and(u8::is_ascii_digit) {
            j += 1;
            digits += 1;
        }
        if rest.get(j) == Some(&b'.') {
            j += 1;
            while rest.get(j).is_some_and(u8::is_ascii_digit) {
                j += 1;
                digits += 1;
            }
        }
        if digits == 0 {
            return None;
        }
        if matches!(rest.get(j), Some(b'e' | b'E')) {
            let mut k = j + 1;
            if matches!(rest.get(k), Some(b'+' | b'-')) {
                k += 1;
            }
            if rest.get(k).is_some_and(u8::is_ascii_digit) {
                while rest.get(k).is_some_and(u8::is_ascii_digit) {
                    k += 1;
                }
                j = k;
            }
        }
        let mut number = String::from_utf8_lossy(&rest[..j]).into_owned();
        if number.starts_with('.') {
            number.insert(0, '0');
        }
        number.parse::<f32>().ok()?
    };
    Some(if negative { -magnitude } else { magnitude })
}

/// A hexadecimal float's digits after `0x` (`h.hhh` with an optional `p` exponent) -> its value.
fn scan_hex(text: &[u8]) -> Option<f32> {
    let (mut mantissa, mut exponent, mut digits, mut i) = (0u64, 0i32, 0, 0);
    let mut fraction = false;
    loop {
        match text.get(i) {
            Some(&c) if c.is_ascii_hexdigit() => {
                let d = (c as char).to_digit(16).unwrap_or(0) as u64;
                if mantissa >> 56 == 0 {
                    mantissa = mantissa << 4 | d;
                    if fraction {
                        exponent -= 4;
                    }
                } else if !fraction {
                    exponent += 4;
                }
                digits += 1;
            }
            Some(b'.') if !fraction => fraction = true,
            _ => break,
        }
        i += 1;
    }
    if digits == 0 {
        return None;
    }
    if matches!(text.get(i), Some(b'p' | b'P')) {
        if let Some(e) = scan_long(&text[i + 1..]) {
            exponent = exponent.saturating_add(e);
        }
    }
    Some((mantissa as f64 * 2f64.powi(exponent)) as f32)
}

/// `(long)value`: cvttss2si's truncation, `0x80000000` for NaN and values out of range.
fn float_to_long(value: f32) -> i32 {
    if value.is_nan() || value >= 2_147_483_648.0 || value < -2_147_483_648.0 {
        i32::MIN
    } else {
        value as i32
    }
}

fn tokens(text: &[u8], separators: &[u8]) -> Vec<Vec<u8>> {
    ee_base::ustring::tokenize(text, separators)
        .into_iter()
        .map(|(a, b)| text[a as usize..=b as usize].to_vec())
        .collect()
}

fn nested_tokens(text: &[u8], separators: &[u8]) -> Vec<Vec<u8>> {
    ee_base::ustring::nested_tokenize(text, separators, b'(', b')')
        .into_iter()
        .map(|(a, b)| text[a as usize..=b as usize].to_vec())
        .collect()
}

// ---------------------------------------------------------------------------------------------- the reader

struct Reader<'a, H: ScriptHost> {
    host: &'a mut H,
    s: &'a mut EeRmapScriptScalars,
    kind_spacing: &'a mut [[c_ulong; 2]; OBJECT_KINDS],
    lists: ScriptLists,
    if_stack: Vec<i32>,
    defines: BTreeMap<&'static [u8], (u32, i32)>,
    map_types: BTreeMap<Vec<u8>, Vec<u8>>,
    keywords: BTreeMap<&'static [u8], u32>,
}

impl<H: ScriptHost> Reader<'_, H> {
    fn add_map_type(&mut self, name: &[u8], value: &[u8]) {
        self.map_types.entry(name.to_vec()).or_insert_with(|| value.to_vec());
    }

    /// ExpandMapTypes: every token naming a constant becomes its value (a leading `#` token stays).
    fn expand(&self, tokens: &[Vec<u8>]) -> Vec<Vec<u8>> {
        tokens
            .iter()
            .enumerate()
            .map(|(i, t)| {
                if i == 0 && t.first() == Some(&b'#') {
                    t.clone()
                } else {
                    self.map_types.get(t).cloned().unwrap_or_else(|| t.clone())
                }
            })
            .collect()
    }

    fn keyword(&self, name: &[u8]) -> u32 {
        self.keywords.get(name).copied().unwrap_or(KW_UNKNOWN)
    }

    /// EvaluateExpression for an integer: `OneOf` draws one of its values, `Between` a value in its range.
    fn expression_long(&mut self, text: &[u8], value: &mut i32) {
        let expanded = self.expand(&tokens(text, b" \t(),"));
        let Some(first) = expanded.first() else { return };
        match self.keyword(first) {
            KW_ONE_OF if expanded.len() >= 2 => {
                let mut number = 0;
                let mut values = Vec::with_capacity(expanded.len() - 1);
                for t in &expanded[1..] {
                    if let Some(v) = scan_long(t) {
                        number = v;
                    }
                    values.push(number);
                }
                let k = self.host.draw_long(0, (values.len() - 1) as i32);
                *value = values.get(k as usize).copied().unwrap_or(0);
            }
            KW_BETWEEN if expanded.len() == 3 => {
                let (low, high) = (scan_long(&expanded[1]).unwrap_or(0), scan_long(&expanded[2]).unwrap_or(0));
                *value = self.host.draw_long(low, high);
            }
            _ => {}
        }
    }

    /// EvaluateExpression for a float.
    fn expression_float(&mut self, text: &[u8], value: &mut f32) {
        let expanded = self.expand(&tokens(text, b" \t(),"));
        let Some(first) = expanded.first() else { return };
        match self.keyword(first) {
            KW_ONE_OF if expanded.len() >= 2 => {
                let mut number = 0.0;
                let mut values = Vec::with_capacity(expanded.len() - 1);
                for t in &expanded[1..] {
                    if let Some(v) = scan_float(t) {
                        number = v;
                    }
                    values.push(number);
                }
                let k = self.host.draw_long(0, (values.len() - 1) as i32);
                *value = values.get(k as usize).copied().unwrap_or(0.0);
            }
            KW_BETWEEN if expanded.len() == 3 => {
                let (low, high) = (scan_float(&expanded[1]).unwrap_or(0.0), scan_float(&expanded[2]).unwrap_or(0.0));
                *value = self.host.draw_float(low, high);
            }
            _ => {}
        }
    }

    /// ParseLongList: a value list, plain numbers read and expressions evaluated; a failed entry is `0xbaadf00d`.
    fn long_list(&mut self, text: &[u8]) -> Vec<i32> {
        let expanded = self.expand(&nested_tokens(text, b" \t,"));
        let mut out = Vec::with_capacity(expanded.len());
        for t in &expanded {
            let mut v = UNSET;
            if !t.contains(&b'(') {
                if let Some(n) = scan_long(t) {
                    v = n;
                }
            } else {
                self.expression_long(t, &mut v);
            }
            out.push(v);
        }
        out
    }

    /// ParseFloatList: as `long_list` (a failed entry is -1163005952.0).
    fn float_list(&mut self, text: &[u8]) -> Vec<f32> {
        let expanded = self.expand(&nested_tokens(text, b" \t,"));
        let mut out = Vec::with_capacity(expanded.len());
        for t in &expanded {
            let mut v = UNSET as f32;
            if !t.contains(&b'(') {
                if let Some(n) = scan_float(t) {
                    v = n;
                }
            } else {
                self.expression_float(t, &mut v);
            }
            out.push(v);
        }
        out
    }

    /// EvaluateCondition: a number, a logical or comparison keyword over nested conditions, `PERCENT` (true with
    /// that chance) or `DEFINED`/`NOTDEFINED` of a constant -> 1 or 0 (a number: its value).
    fn condition(&mut self, text: &[u8]) -> i32 {
        let tokens = tokens(text, b" \t(),");
        let expanded = self.expand(&tokens);
        let Some(first) = expanded.first() else { return 0 };
        let token = self.keyword(first);
        if expanded.len() == 1 {
            return scan_long(first).unwrap_or(0);
        }
        if (KW_AND..=KW_PERCENT).contains(&token) {
            let a = self.condition(&expanded[1]);
            let b = if expanded.len() == 3 { self.condition(&expanded[2]) } else { 0 };
            return match token {
                KW_AND => (a != 0 && b != 0) as i32,
                KW_OR => (a != 0 || b != 0) as i32,
                KW_XOR => ((a != 0 || b != 0) && a != b) as i32,
                KW_NOT => (a == 0) as i32,
                KW_EQ => (a == b) as i32,
                KW_NEQ => (a != b) as i32,
                KW_LT => (a < b) as i32,
                KW_LTEQ => (a <= b) as i32,
                KW_GT => (a > b) as i32,
                KW_GTEQ => (a >= b) as i32,
                KW_PERCENT => match a {
                    0 => 0,
                    100 => 1,
                    _ => (self.host.draw_long(0, 99) <= a.wrapping_sub(1)) as i32,
                },
                _ => 0,
            };
        }
        if !(KW_DEFINED..=KW_NOT_DEFINED).contains(&token) {
            return 0;
        }
        let defined = tokens.get(1).is_some_and(|t| self.map_types.contains_key(t));
        if token == KW_DEFINED {
            defined as i32
        } else {
            (!defined) as i32
        }
    }

    /// ReadScript: `<name>.rmv` line by line as `fgets` with a 0x200 buffer gave them; a missing file is skipped.
    fn read_script(&mut self, name: &[u8]) {
        let Some(text) = self.host.read(name) else { return };
        let mut at = 0;
        while at < text.len() {
            let mut end = at;
            while end < text.len() && end - at < 0x1ff {
                end += 1;
                if text[end - 1] == b'\n' {
                    break;
                }
            }
            let chunk = &text[at..end];
            let line = chunk.iter().position(|&c| c == 0).map_or(chunk, |n| &chunk[..n]).to_vec();
            at = end;
            self.parse_line(&line);
        }
    }

    fn active(&self) -> bool {
        self.if_stack.last().is_none_or(|&v| v == 1)
    }

    /// ParseLine: an attribute (inside an active `#if` branch) or a directive; comment lines are skipped.
    fn parse_line(&mut self, line: &[u8]) {
        let line_tokens = nested_tokens(line, b" \t\n,");
        let Some(head) = line_tokens.first() else { return };
        if head.windows(2).any(|w| w == b"//") {
            return;
        }
        let expanded = self.expand(&line_tokens);
        let (token, kind) = self.defines.get(expanded[0].as_slice()).copied().unwrap_or((0, 0));
        let value: &[u8] = expanded.get(1).map_or(&[], Vec::as_slice);
        if token >= tok::MAX_INT_ELEVATION
            && token != tok::USE_MAP_GENERATOR
            && token != tok::STRING_TABLE_ID
            && self.active()
        {
            let (mut longs, mut floats) = (Vec::new(), Vec::new());
            if kind == LONG {
                longs = self.long_list(value);
            } else {
                floats = self.float_list(value);
            }
            self.attribute(token, &longs, &floats);
            return;
        }
        match token {
            tok::DEFINE => {
                if self.active() {
                    let tokens = nested_tokens(value, b" \t\n,");
                    let Some(name) = tokens.first() else { return };
                    let expanded = self.expand(&tokens);
                    let value = expanded.get(1).cloned().unwrap_or_default();
                    self.add_map_type(&name.clone(), &value);
                }
            }
            tok::IF => {
                let top = self.if_stack.last().copied();
                let state = if top == Some(2) || top == Some(0) {
                    2
                } else if !value.is_empty() && self.condition(value) != 0 {
                    1
                } else {
                    0
                };
                self.if_stack.push(state);
            }
            tok::ELIF => {
                if let Some(&top) = self.if_stack.last() {
                    if top == 0 {
                        if !value.is_empty() && self.condition(value) != 0 {
                            *self.if_stack.last_mut().unwrap() = 1;
                        }
                    } else if top != 2 {
                        *self.if_stack.last_mut().unwrap() = 2;
                    }
                }
            }
            tok::ELSE => {
                if let Some(top) = self.if_stack.last_mut() {
                    if *top != 2 {
                        *top = if *top == 0 { 1 } else { 2 };
                    }
                }
            }
            tok::ENDIF => {
                self.if_stack.pop();
            }
            tok::INCLUDE if self.active() => {
                if let Some(name) = tokens(line, b"\"").get(1) {
                    self.read_script(name);
                }
            }
            _ => {}
        }
    }

    /// One attribute's values applied to the settings.
    fn attribute(&mut self, token: u32, longs: &[i32], floats: &[f32]) {
        let long = longs.first().copied().unwrap_or(0);
        let second = longs.get(1).copied().unwrap_or(0);
        let float = floats.first().copied().unwrap_or(0.0);
        let unsigned = long as u32 as c_ulong;
        let s = &mut *self.s;
        let l = &mut self.lists;
        match token {
            tok::MAX_INT_ELEVATION => s.max_int_elevation = long as c_long,
            tok::MIN_INT_ELEVATION => s.min_int_elevation = long as c_long,
            tok::INITIAL_ELEVATION => s.initial_elevation = long as f32,
            tok::FLAT_ELEVATION => s.flat_elevation = long as f32,
            tok::ELEVATION_SCALE => s.elevation_scale = float,
            tok::HEIGHT_MAP_CHAOS => s.height_map_chaos = float,
            tok::HEIGHT_MAP_SMOOTHNESS => s.height_map_smoothness = float,
            tok::PERCENT_LAND => s.percent_land = float,
            tok::LAND_ELEVATION_THRESHOLD => s.land_elevation_threshold = long as f32,
            tok::WATER_BORDER => s.water_border = long as c_long,
            tok::RIVER_WIDTH => s.river_width = unsigned,
            tok::COASTAL_TO_DEEPER_WATER_PERCENTAGE => s.coastal_to_deeper_water_percentage = float,
            tok::TEAM_GROWTH => s.team_growth = long > 0,
            tok::SHALLOWS_WIDTH => s.shallows_width = long as c_long,
            tok::MAX_SHALLOW_CONNECTIONS_PER_PLAYER => s.max_shallow_connections_per_player = long as c_long,
            tok::GENERATE_SHALLOWS => s.generate_shallows = long == 1,
            tok::LANDFILL_PERCENTAGE => s.landfill_percentage = float,
            tok::IS_SPACE_MAP => s.is_space_map = long == 1,
            tok::NUM_MOONS => s.num_moons = unsigned,
            tok::MOON_SIZE => s.moon_size = unsigned,
            tok::MOON_RESOURCES => s.moon_resources = unsigned,
            tok::PLAYER_INNER_RADIUS => s.player_inner_radius = float,
            tok::PLAYER_OUTER_RADIUS => s.player_outer_radius = float,
            tok::PLAYER_PERCENT_FLAT => s.player_percent_flat = float,
            tok::PLAYER_TREE_PERCENTAGE => s.player_tree_percentage = float,
            tok::PLAYER_FLAT_CHAOS => s.player_flat_chaos = float,
            tok::PLAYER_FLAT_CLUMPS => s.player_flat_clumps = unsigned,
            tok::PLAYER_LAND_CHAOS => s.player_land_chaos = float,
            tok::PLAYER_LAND_CLUMPS => s.player_land_clumps = unsigned,
            tok::MAX_ANGLE_BETWEEN_TEAM_MEMBERS => s.max_angle_between_team_members = float * DEGREES,
            tok::OUTER_PUSH_FROM_MAP_CENTER => s.outer_push_from_map_center = long as c_long,
            tok::INNER_PUSH_FROM_MAP_CENTER => s.inner_push_from_map_center = long as c_long,
            tok::MINIMUM_START_POSITION_TO_MAP_EDGE => s.minimum_start_position_to_map_edge = long as c_long,
            tok::NEUTRAL_INNER_RADIUS => s.neutral_inner_radius = float,
            tok::NEUTRAL_OUTER_RADIUS => s.neutral_outer_radius = float,
            tok::NUM_OUTER_NEUTRALS => s.num_outer_neutrals = unsigned,
            tok::OUTER_NEUTRAL_PERCENT_LAND => s.outer_neutral_percent_land = float,
            tok::OUTER_NEUTRAL_PERCENT_FLAT => s.outer_neutral_percent_flat = float,
            tok::OUTER_NEUTRAL_TREE_PERCENTAGE => s.outer_neutral_tree_percentage = float,
            tok::OUTER_NEUTRAL_LAND_CHAOS => s.outer_neutral_land_chaos = float,
            tok::OUTER_NEUTRAL_LAND_CLUMPS => s.outer_neutral_land_clumps = unsigned,
            tok::OUTER_NEUTRAL_FLAT_CHAOS => s.outer_neutral_flat_chaos = float,
            tok::OUTER_NEUTRAL_FLAT_CLUMPS => s.outer_neutral_flat_clumps = unsigned,
            tok::NUM_INNER_NEUTRALS => s.num_inner_neutrals = unsigned,
            tok::INNER_NEUTRAL_PERCENT_LAND => s.inner_neutral_percent_land = float,
            tok::INNER_NEUTRAL_PERCENT_FLAT => s.inner_neutral_percent_flat = float,
            tok::INNER_NEUTRAL_TREE_PERCENTAGE => s.inner_neutral_tree_percentage = float,
            tok::INNER_NEUTRAL_LAND_CHAOS => s.inner_neutral_land_chaos = float,
            tok::INNER_NEUTRAL_LAND_CLUMPS => s.inner_neutral_land_clumps = unsigned,
            tok::INNER_NEUTRAL_FLAT_CHAOS => s.inner_neutral_flat_chaos = float,
            tok::INNER_NEUTRAL_FLAT_CLUMPS => s.inner_neutral_flat_clumps = unsigned,
            tok::FOREST_FREE_RADIUS => s.forest_free_radius = float,
            tok::FORESTS_PER_PLAYER => s.forests_per_player = unsigned,
            tok::FOREST_CHAOS_LEVEL => s.forest_chaos_level = float,
            tok::MAX_CLUMPS_PER_FOREST => s.max_clumps_per_forest = unsigned,
            tok::MAX_TREE_LOOP_COUNT => s.max_tree_loop_count = unsigned,
            tok::MAXIMUM_TREES_PER_CLUMP => s.maximum_trees_per_clump = long as c_long,
            tok::DEFINE_FOREST => {
                if longs.len() > 2 {
                    l.forests.push(EeRmapScriptForest {
                        id: longs[0] as c_long,
                        terrain_texture: longs[1] as c_long,
                        first: l.forest_trees.len() as u32,
                        count: (longs.len() - 2) as u32,
                    });
                    l.forest_trees.extend(longs[2..].iter().map(|&v| v as u32 as c_ulong));
                }
            }
            tok::AVAILABLE_FORESTS => l.tree_types = longs.iter().map(|&v| v as c_long).collect(),
            tok::RESOURCE_PER_PLAYER => {
                if (0..OBJECT_KINDS as i32).contains(&long) && second >= 0 {
                    s.resource_per_player[long as usize] = second as c_long;
                }
            }
            tok::RESOURCE_PER_NEUTRAL_LAND => {
                if (0..OBJECT_KINDS as i32).contains(&long) && second >= 0 {
                    s.resource_per_neutral_land[long as usize] = second as u32 as c_ulong;
                }
            }
            tok::MAX_RESOURCE_ELEVATION => s.max_resource_elevation = long as f32,
            tok::RESOURCE_TO_EDGE_DISTANCE => s.resource_to_edge_distance = unsigned,
            tok::RESOURCE_TO_WATER_DISTANCE => s.resource_to_water_distance = unsigned,
            tok::AVAILABLE_ANIMALS => l.animals = longs.iter().map(|&v| v as u32 as c_ulong).collect(),
            tok::RESOURCE_PLACEMENT_LIMIT => {
                if longs.len() == 3 && (0..OBJECT_KINDS as i32).contains(&longs[0]) {
                    let k = longs[0] as usize;
                    self.kind_spacing[k][0] = longs[1].wrapping_mul(longs[1]) as u32 as c_ulong;
                    self.kind_spacing[k][1] = longs[2].wrapping_mul(longs[2]) as u32 as c_ulong;
                }
            }
            tok::DEFINE_HERD => {
                if longs.len() == 5 {
                    l.kinds.push(EeRmapScriptKind {
                        id: longs[0] as c_long,
                        unit_type: longs[1] as c_long,
                        area: longs[2] as u32 as c_ulong,
                        minimum: longs[3] as c_long,
                        maximum: longs[4] as c_long,
                    });
                }
            }
            tok::DEFINE_RANDOM_HERD_SET => l.kind_ids = longs.iter().map(|&v| v as c_long).collect(),
            tok::HERD_PER_PLAYER | tok::HERD_PER_NEUTRAL => {
                if longs.len() == 2 && longs[1] > 0 {
                    let slot = EeRmapScriptSlot {
                        id: longs[0] as c_long,
                        per_player: token == tok::HERD_PER_PLAYER,
                        random_kind: false,
                    };
                    l.slots.extend(std::iter::repeat_n(slot, longs[1] as usize));
                }
            }
            tok::RANDOM_HERD_PER_PLAYER | tok::RANDOM_HERD_PER_NEUTRAL => {
                if longs.len() == 1 && longs[0] > 0 {
                    let slot =
                        EeRmapScriptSlot { id: 0, per_player: token == tok::RANDOM_HERD_PER_PLAYER, random_kind: true };
                    l.slots.extend(std::iter::repeat_n(slot, longs[0] as usize));
                }
            }
            tok::MAX_TREE_ELEVATION => s.max_tree_elevation = long as f32,
            tok::RESOURCE_SEPERATION => s.resource_seperation = float,
            tok::RESOURCE_TERRAIN_TEXTURE => {
                if longs.len() == 2 && (0..OBJECT_KINDS as i32).contains(&longs[0]) {
                    s.resource_terrain_texture[longs[0] as usize] = longs[1] as c_long;
                }
            }
            tok::PLACE_RESOURCE_IN_RING => {
                if longs.len() == 3 && (longs[0] as u32 as usize) < OBJECT_KINDS {
                    l.rules.push(EeRmapScriptRule {
                        kind: longs[0] as c_long,
                        minimum: longs[1] as c_long,
                        maximum: longs[2] as c_long,
                    });
                }
            }
            tok::PLACE_UNIT | tok::PLACE_WORLD_UNIT | tok::PLACE_UNIT_IN_RING | tok::PLACE_WORLD_UNIT_IN_RING => {
                if floats.len() >= 3 {
                    l.features.push(EeRmapScriptFeature {
                        unit_type: float_to_long(floats[0]) as c_long,
                        x: floats[1],
                        y: floats[2],
                        is_world_unit: token == tok::PLACE_WORLD_UNIT || token == tok::PLACE_WORLD_UNIT_IN_RING,
                        in_ring: token == tok::PLACE_UNIT_IN_RING || token == tok::PLACE_WORLD_UNIT_IN_RING,
                        is_herd: false,
                        angle: if floats.len() == 4 { floats[3] * DEGREES } else { PI },
                        index: 0,
                    });
                }
            }
            tok::PLACE_WALL => {
                if longs.len() == 5 {
                    l.walls.push(EeRmapScriptWall {
                        unit_type: longs[0] as c_long,
                        start_x: longs[1] as c_long,
                        start_y: longs[2] as c_long,
                        end_x: longs[3] as c_long,
                        end_y: longs[4] as c_long,
                    });
                }
            }
            tok::SHALLOWS_ELEVATION => s.shallows_elevation = long as f32,
            tok::TERRAIN_REGION => {
                if longs.len() >= 3 {
                    let (from, to) = if longs.len() == 3 { (longs[0], longs[1]) } else { (0, 0) };
                    if let Some(i) = l.terrains.iter().position(|t| t.id as i32 == longs[2]) {
                        l.regions.push(EeRmapScriptRegion {
                            from: from as c_long,
                            to: to as c_long,
                            terrain: i as c_ulong,
                        });
                    }
                }
            }
            tok::TERRAIN_CHAOS => s.terrain_chaos = float,
            tok::CLIFF_TEXTURE => s.cliff_texture = long as c_long,
            tok::DECLARE_RANDOM_MAP_TERRAIN => {
                if longs.len() == 2 && !l.terrains.iter().any(|t| t.id as i32 == long) {
                    l.terrains.push(EeRmapScriptTerrain { id: long as c_long, textures: [second as c_long; 31] });
                }
            }
            tok::RANDOM_MAP_TERRAIN_RANGE => {
                if longs.len() == 4 {
                    if let Some(t) = l.terrains.iter_mut().find(|t| t.id as i32 == longs[0]) {
                        let (low, high) = (longs[1].wrapping_add(10) as u32, longs[2].wrapping_add(10) as u32);
                        if low <= 30 && high <= 30 && low <= high {
                            for j in low..=high {
                                t.textures[j as usize] = longs[3] as c_long;
                            }
                        }
                    }
                }
            }
            tok::CONVERGENT_SAMPLE_SIZE => s.convergent_sample_size = float,
            tok::CONVERGENT_THRESHOLD => s.convergent_threshold = float,
            tok::MAX_LOOP_COUNT => s.max_loop_count = unsigned,
            tok::MAX_ITERATIONS_CLUMP_GROWTH => s.max_iterations_clump_growth = long as c_long,
            tok::MAP_CHARACTERISTICS => {
                s.map_characteristics = longs.iter().fold(0u32, |acc, &v| acc | v as u32) as c_ulong;
            }
            tok::FILTER_HEIGHT_MAP => s.filter_height_map = long > 0,
            tok::TESTING_LAND_DISTRIBUTION => s.testing_land_distribution = long > 0,
            _ => {}
        }
    }
}

/// SetDefaults: every setting a script can set, before the script is read.
fn set_defaults(s: &mut EeRmapScriptScalars, kind_spacing: &mut [[c_ulong; 2]; OBJECT_KINDS]) {
    *kind_spacing = [[0, u32::MAX as c_ulong]; OBJECT_KINDS];
    s.resource_per_player = [0; OBJECT_KINDS];
    s.resource_per_neutral_land = [0; OBJECT_KINDS];
    s.resource_terrain_texture = [-1; OBJECT_KINDS];
    s.cliff_texture = -1;
    s.resource_seperation = 3.0;
    s.map_characteristics = 0;
    s.land_elevation_threshold = 0.0;
    s.initial_elevation = 0.0;
    s.height_map_chaos = 1.0;
    s.height_map_smoothness = 0.2;
    s.convergent_sample_size = 32.0;
    s.convergent_threshold = 0.1;
    s.percent_land = 0.6;
    s.player_percent_flat = 0.75;
    s.player_inner_radius = 0.5;
    s.player_outer_radius = 0.8;
    s.landfill_percentage = 0.0;
    s.num_moons = 0;
    s.moon_size = 5;
    s.moon_resources = 5;
    s.neutral_inner_radius = 0.1;
    s.neutral_outer_radius = 0.4;
    s.max_angle_between_team_members = 360.0;
    s.max_loop_count = 2000;
    s.max_int_elevation = 20;
    s.min_int_elevation = -10;
    s.flat_elevation = 0.0;
    s.max_resource_elevation = 20.0;
    s.max_tree_elevation = 20.0;
    s.max_iterations_clump_growth = 1000;
    s.forest_chaos_level = 0.1;
    s.max_clumps_per_forest = 4;
    s.forests_per_player = 3;
    s.max_tree_loop_count = 1000;
    s.filter_height_map = true;
    s.testing_land_distribution = false;
    s.resource_to_water_distance = 3;
    s.resource_to_edge_distance = 3;
    s.water_border = 7;
    s.forest_free_radius = 10.0;
    s.maximum_trees_per_clump = 10000;
    s.inner_neutral_flat_chaos = 0.7;
    s.inner_neutral_land_chaos = 0.7;
    s.inner_neutral_tree_percentage = 0.0;
    s.inner_neutral_percent_flat = 0.9;
    s.inner_neutral_percent_land = 0.25;
    s.outer_neutral_flat_chaos = 0.7;
    s.outer_neutral_land_chaos = 0.7;
    s.outer_neutral_tree_percentage = 0.7;
    s.outer_neutral_percent_flat = 0.9;
    s.outer_neutral_percent_land = 0.25;
    s.player_flat_chaos = 0.7;
    s.player_land_chaos = 0.7;
    s.player_tree_percentage = 0.4;
    s.num_inner_neutrals = 1;
    s.num_outer_neutrals = 1;
    s.inner_neutral_flat_clumps = 3;
    s.inner_neutral_land_clumps = 3;
    s.outer_neutral_flat_clumps = 3;
    s.outer_neutral_land_clumps = 3;
    s.player_flat_clumps = 3;
    s.player_land_clumps = 3;
    s.coastal_to_deeper_water_percentage = 0.0;
    s.river_width = 0;
    s.team_growth = false;
    s.terrain_chaos = 10.0;
    s.shallows_elevation = -1.0;
    s.shallows_width = 0;
    s.max_shallow_connections_per_player = 0;
    s.generate_shallows = false;
    s.is_space_map = false;
    s.outer_push_from_map_center = 0;
    s.inner_push_from_map_center = 0;
    s.minimum_start_position_to_map_edge = 0;
}

/// SetRandomMapValues (after the host's `ResetPlayerOrder`): the defaults, the script `<name>.rmv`, then the
/// elevations converted to 0..1 units, the flat area sized from the placed units and walls, and the available tree
/// types mapped to forest indices (unknown ones dropped). The lists are new (the C++ appended to the map's, which
/// the game hands over empty: a fresh `EERandomMap`, its features cleared by `InitializeValues`).
pub fn read_script<H: ScriptHost>(
    host: &mut H,
    tables: &ScriptTables,
    scalars: &mut EeRmapScriptScalars,
    kind_spacing: &mut [[c_ulong; 2]; OBJECT_KINDS],
    name: &[u8],
) -> ScriptLists {
    let mut r = Reader {
        host,
        s: scalars,
        kind_spacing,
        lists: ScriptLists::default(),
        if_stack: Vec::new(),
        defines: DEFINES.iter().map(|&(n, t, k)| (n.as_bytes(), (t, k))).collect(),
        map_types: BTreeMap::new(),
        keywords: KEYWORDS.iter().map(|&(n, t)| (n.as_bytes(), t)).collect(),
    };
    // InitializeMapTypes, in its order (insert: a later duplicate name keeps the first value)
    let (epoch, players, size) = (r.s.start_epoch as i32, r.s.player_count as i32, r.s.map_size as i32);
    for (n, v) in &MAP_TYPES[..8] {
        r.add_map_type(n.as_bytes(), v.as_bytes());
    }
    r.add_map_type(b"$StartEpoch", epoch.to_string().as_bytes());
    r.add_map_type(b"$PlayerCount", players.to_string().as_bytes());
    r.add_map_type(b"$MapSize", size.to_string().as_bytes());
    for (n, v) in MAP_TYPES[8..].iter().chain(MAP_TYPES_2.iter()) {
        r.add_map_type(n.as_bytes(), v.as_bytes());
    }
    for (i, (n, _)) in tables.units.iter().enumerate() {
        r.add_map_type(n, (i as u32).to_string().as_bytes());
    }
    for (n, v) in tables.terrains.iter().chain(tables.cliffs.iter()) {
        r.add_map_type(n, v.to_string().as_bytes());
    }
    set_defaults(r.s, r.kind_spacing);
    r.read_script(name);

    let mut lists = std::mem::take(&mut r.lists);
    let s = &mut *r.s;
    let scale = 1.0f32 / ((s.max_int_elevation as i32).wrapping_sub(s.min_int_elevation as i32)) as f32;
    s.elevation_unit = scale;
    let minimum = s.min_int_elevation as i32 as f32;
    s.flat_elevation = (s.flat_elevation - minimum) * scale;
    s.land_elevation_threshold = (s.land_elevation_threshold - minimum) * scale;
    let threshold = s.land_elevation_threshold;
    s.initial_elevation = (s.initial_elevation - minimum) * scale;
    s.shallows_elevation = (s.shallows_elevation - minimum) * scale - 0.00001;
    s.max_resource_elevation = (s.max_resource_elevation - minimum) * scale;
    if s.max_resource_elevation < threshold {
        s.max_resource_elevation = threshold;
    }
    s.max_tree_elevation = (s.max_tree_elevation - minimum) * scale;
    if s.max_tree_elevation < threshold {
        s.max_tree_elevation = threshold;
    }
    if s.num_inner_neutrals == 0 {
        s.inner_neutral_percent_land = 0.0;
    }
    if s.num_outer_neutrals == 0 {
        s.outer_neutral_percent_land = 0.0;
    }
    if s.resource_to_edge_distance == 0 {
        s.resource_to_edge_distance = 1;
    }
    let (mut min_x, mut max_x, mut min_y, mut max_y) = (0.0f32, 0.0f32, 0.0f32, 0.0f32);
    let mut widen = |x: f32, right: f32, y: f32, top: f32| {
        if min_x > x {
            min_x = x;
        }
        if max_x < right {
            max_x = right;
        }
        if y < min_y {
            min_y = y;
        }
        if top > max_y {
            max_y = top;
        }
    };
    for f in lists.features.iter().filter(|f| !f.in_ring && !f.is_herd) {
        let size = tables.units.get(f.unit_type as i32 as u32 as usize).map_or(0, |u| u.1);
        if size < 2 {
            widen(f.x, f.x, f.y, f.y);
        } else {
            widen(f.x, size as f32 + f.x, f.y, size as f32 + f.y);
        }
    }
    for w in &lists.walls {
        let (sx, ex, sy, ey) = (w.start_x as i32, w.end_x as i32, w.start_y as i32, w.end_y as i32);
        let (x, right) = if sx < ex { (sx, ex) } else { (ex, sx) };
        let (y, top) = if sy < ey { (sy, ey) } else { (ey, sy) };
        widen(x as f32, right as f32, y as f32, top as f32);
    }
    lists.flat_area = [max_y, min_x, min_y, max_x];
    let forests = &lists.forests;
    lists.tree_types = lists
        .tree_types
        .iter()
        .filter_map(|&t| forests.iter().position(|f| f.id as i32 == t as i32).map(|i| i as c_long))
        .collect();
    lists
}

// ---------------------------------------------------------------------------------------------- the C interface

/// One named table entry: a unit (`value` its `mSize`), a terrain or a cliff (`value` its `mValue`).
#[repr(C)]
pub struct EeRmapScriptName {
    pub text: *const c_char,
    pub length: u32,
    pub value: i32,
}

/// The bytes of one script file, filled by the host's `read` through `ee_rmap_script_bytes_push`.
pub struct EeRmapScriptBytes(Vec<u8>);

/// The game's side of a read.
#[repr(C)]
pub struct EeRmapScriptHost {
    pub context: *mut c_void,
    /// Reads `<script directory>/<name>.rmv` (`name` a C string) into `sink` -> false when the file is missing.
    pub read: unsafe extern "C" fn(context: *mut c_void, name: *const c_char, sink: *mut EeRmapScriptBytes) -> bool,
    pub draw_long: unsafe extern "C" fn(context: *mut c_void, minimum: c_long, maximum: c_long) -> c_long,
    pub draw_float: unsafe extern "C" fn(context: *mut c_void, minimum: f32, maximum: f32) -> f32,
}

/// A read's lists, as pointers into the returned handle (valid until `ee_rmap_script_free`).
#[repr(C)]
pub struct EeRmapScriptOutput {
    pub flat_area_top: f32,
    pub flat_area_left: f32,
    pub flat_area_bottom: f32,
    pub flat_area_right: f32,
    pub features: *const EeRmapScriptFeature,
    pub feature_count: u32,
    pub walls: *const EeRmapScriptWall,
    pub wall_count: u32,
    pub terrains: *const EeRmapScriptTerrain,
    pub terrain_count: u32,
    pub regions: *const EeRmapScriptRegion,
    pub region_count: u32,
    pub animals: *const c_ulong,
    pub animal_count: u32,
    pub forests: *const EeRmapScriptForest,
    pub forest_count: u32,
    pub forest_trees: *const c_ulong,
    pub tree_types: *const c_long,
    pub tree_type_count: u32,
    pub kinds: *const EeRmapScriptKind,
    pub kind_count: u32,
    pub kind_ids: *const c_long,
    pub kind_id_count: u32,
    pub slots: *const EeRmapScriptSlot,
    pub slot_count: u32,
    pub rules: *const EeRmapScriptRule,
    pub rule_count: u32,
}

/// A finished read's lists (opaque to the C++).
pub struct EeRmapScript(ScriptLists);

struct FfiHost<'a>(&'a EeRmapScriptHost);

impl ScriptHost for FfiHost<'_> {
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
        let mut text = name.to_vec();
        text.push(0);
        let mut sink = EeRmapScriptBytes(Vec::new());
        // SAFETY: the host's callback with its own context, a C string and a live sink
        let found = unsafe { (self.0.read)(self.0.context, text.as_ptr().cast(), &mut sink) };
        found.then_some(sink.0)
    }

    fn draw_long(&mut self, minimum: i32, maximum: i32) -> i32 {
        // SAFETY: the host's callback with its own context
        unsafe { (self.0.draw_long)(self.0.context, minimum as c_long, maximum as c_long) as i32 }
    }

    fn draw_float(&mut self, minimum: f32, maximum: f32) -> f32 {
        // SAFETY: the host's callback with its own context
        unsafe { (self.0.draw_float)(self.0.context, minimum, maximum) }
    }
}

/// Appends `length` bytes to a read's file text.
///
/// # Safety
/// `sink` is the sink the host's `read` received; `bytes` holds `length` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_script_bytes_push(sink: *mut EeRmapScriptBytes, bytes: *const u8, length: usize) {
    if length != 0 {
        (*sink).0.extend_from_slice(std::slice::from_raw_parts(bytes, length));
    }
}

unsafe fn names(p: *const EeRmapScriptName, n: u32) -> Vec<(Vec<u8>, i32)> {
    (0..n as usize)
        .map(|i| {
            let e = &*p.add(i);
            let text =
                if e.length == 0 { &[][..] } else { std::slice::from_raw_parts(e.text.cast(), e.length as usize) };
            (text.to_vec(), e.value)
        })
        .collect()
}

/// SetRandomMapValues: reads script `name` (a C string) into `scalars` (the map's block from `mMapSize`) and
/// `kind_spacing` (`mKindSpacing`, 7 x 2), and its lists into `out` -> the handle that owns them.
///
/// # Safety
/// Every pointer is live for its count; `host`'s callbacks are callable with its context.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_rmap_script_read(
    host: *const EeRmapScriptHost,
    scalars: *mut EeRmapScriptScalars,
    kind_spacing: *mut c_ulong,
    units: *const EeRmapScriptName,
    unit_count: u32,
    terrains: *const EeRmapScriptName,
    terrain_count: u32,
    cliffs: *const EeRmapScriptName,
    cliff_count: u32,
    name: *const c_char,
    out: *mut EeRmapScriptOutput,
) -> *mut EeRmapScript {
    let tables = ScriptTables {
        units: names(units, unit_count).into_iter().map(|(n, v)| (n, v as u32)).collect(),
        terrains: names(terrains, terrain_count),
        cliffs: names(cliffs, cliff_count),
    };
    let spacing = &mut *kind_spacing.cast::<[[c_ulong; 2]; OBJECT_KINDS]>();
    let name = std::ffi::CStr::from_ptr(name).to_bytes();
    let script = Box::new(EeRmapScript(read_script(&mut FfiHost(&*host), &tables, &mut *scalars, spacing, name)));
    let l = &script.0;
    *out = EeRmapScriptOutput {
        flat_area_top: l.flat_area[0],
        flat_area_left: l.flat_area[1],
        flat_area_bottom: l.flat_area[2],
        flat_area_right: l.flat_area[3],
        features: l.features.as_ptr(),
        feature_count: l.features.len() as u32,
        walls: l.walls.as_ptr(),
        wall_count: l.walls.len() as u32,
        terrains: l.terrains.as_ptr(),
        terrain_count: l.terrains.len() as u32,
        regions: l.regions.as_ptr(),
        region_count: l.regions.len() as u32,
        animals: l.animals.as_ptr(),
        animal_count: l.animals.len() as u32,
        forests: l.forests.as_ptr(),
        forest_count: l.forests.len() as u32,
        forest_trees: l.forest_trees.as_ptr(),
        tree_types: l.tree_types.as_ptr(),
        tree_type_count: l.tree_types.len() as u32,
        kinds: l.kinds.as_ptr(),
        kind_count: l.kinds.len() as u32,
        kind_ids: l.kind_ids.as_ptr(),
        kind_id_count: l.kind_ids.len() as u32,
        slots: l.slots.as_ptr(),
        slot_count: l.slots.len() as u32,
        rules: l.rules.as_ptr(),
        rule_count: l.rules.len() as u32,
    };
    Box::into_raw(script)
}

/// Frees a read's lists.
///
/// # Safety
/// `script` came from `ee_rmap_script_read` (or is null) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_script_free(script: *mut EeRmapScript) {
    if !script.is_null() {
        drop(Box::from_raw(script));
    }
}

// ---------------------------------------------------------------------------------------------- the generator list

/// What `EERandomMap::Initialize` reads from one `.rmv` file for the generator list.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct ScriptGenerator {
    /// The generator's name: the file name before its (last, any case) `.rmv`.
    pub name: Vec<u8>,
    /// The `rmap` PIM the script names (`UseMapGenerator "<module>"`; the last such line wins).
    pub module: Vec<u8>,
    /// `StringTableID <id>`: the display name's string id (the last such line; a value that does not parse keeps the
    /// one before, 0 at first).
    pub string_id: Option<i32>,
}

/// `EERandomMap::Initialize`'s read of a script: its lines as `fgets` with a 0x400 buffer gave them (at most 1,023
/// bytes, up to and including a newline, cut at a NUL), each split on `"` for `UseMapGenerator` and nested-tokenized
/// (`" \t\n,"`, brackets `()`) for `StringTableID`. `None` without a `UseMapGenerator` line.
pub fn script_generator(file: &[u8], text: &[u8]) -> Option<ScriptGenerator> {
    const USE_MAP_GENERATOR: &[u8] = b"UseMapGenerator";
    const STRING_TABLE_ID: &[u8] = b"StringTableID";
    let mut found: Option<ScriptGenerator> = None;
    let mut string_id: Option<i32> = None;
    let mut at = 0;
    while at < text.len() {
        let mut end = at;
        while end < text.len() && end - at < 0x3ff {
            end += 1;
            if text[end - 1] == b'\n' {
                break;
            }
        }
        let chunk = &text[at..end];
        let line = chunk.iter().position(|&c| c == 0).map_or(chunk, |n| &chunk[..n]);
        at = end;
        let quoted = tokens(line, b"\"");
        if quoted.len() >= 2 && quoted[0].starts_with(USE_MAP_GENERATOR) {
            let lower = file.to_ascii_lowercase();
            let cut = lower.windows(4).rposition(|w| w == b".rmv").unwrap_or(file.len());
            found = Some(ScriptGenerator { name: file[..cut].to_vec(), module: quoted[1].clone(), string_id: None });
        }
        let words = nested_tokens(line, b" \t\n,");
        if words.len() >= 2 && words[0].starts_with(STRING_TABLE_ID) {
            string_id = Some(scan_long(&words[1]).unwrap_or(string_id.unwrap_or(0)));
        }
    }
    found.map(|g| ScriptGenerator { string_id, ..g })
}

/// A unit's, terrain's or cliff's name as the scripts name it (`EERandomMap::InitializeValues`): its space-separated
/// words joined; `None` for a name of spaces only.
pub fn script_name(name: &[u8]) -> Option<Vec<u8>> {
    let words = tokens(name, b" ");
    (!words.is_empty()).then(|| words.concat())
}

/// [`script_generator`]'s answer for the C++: the name and module point into the handle's buffers.
#[repr(C)]
pub struct EeRmapScriptGenerator {
    pub name: *const c_char,
    pub name_length: usize,
    pub module: *const c_char,
    pub module_length: usize,
    pub has_string_id: bool,
    pub string_id: c_long,
}

/// [`script_generator`] over a file's name and text: false (and `out` untouched) without a generator; else `out`'s
/// strings are valid until [`ee_rmap_script_generator_free`] of `*handle`.
///
/// # Safety
/// `file` (`file_length` bytes) and `text` (`text_length`) are readable; `out` and `handle` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_script_generator(
    file: *const c_char,
    file_length: usize,
    text: *const c_char,
    text_length: usize,
    out: *mut EeRmapScriptGenerator,
    handle: *mut *mut c_void,
) -> bool {
    let slice = |p: *const c_char, n: usize| {
        // SAFETY: the caller's guarantee
        if n == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(p as *const u8, n) } }
    };
    let Some(g) = script_generator(slice(file, file_length), slice(text, text_length)) else { return false };
    let g = Box::new(g);
    // SAFETY: the caller's guarantee
    unsafe {
        *out = EeRmapScriptGenerator {
            name: g.name.as_ptr() as *const c_char,
            name_length: g.name.len(),
            module: g.module.as_ptr() as *const c_char,
            module_length: g.module.len(),
            has_string_id: g.string_id.is_some(),
            string_id: g.string_id.unwrap_or(0) as c_long,
        };
        *handle = Box::into_raw(g) as *mut c_void;
    }
    true
}

/// Frees a [`ee_rmap_script_generator`] answer.
///
/// # Safety
/// `handle` came from [`ee_rmap_script_generator`] and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_script_generator_free(handle: *mut c_void) {
    // SAFETY: the caller's guarantee
    drop(unsafe { Box::from_raw(handle as *mut ScriptGenerator) });
}

/// [`script_name`] of `name` (`length` bytes) into `out` (room for `length` bytes); returns the joined length, or
/// -1 for a name of spaces only.
///
/// # Safety
/// `name` holds `length` bytes; `out` has room for `length`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_script_name(name: *const c_char, length: usize, out: *mut c_char) -> isize {
    // SAFETY: the caller's guarantee
    let text = if length == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(name as *const u8, length) } };
    match script_name(text) {
        Some(n) => {
            // SAFETY: the joined name is no longer than the name
            unsafe { std::ptr::copy_nonoverlapping(n.as_ptr(), out as *mut u8, n.len()) };
            n.len() as isize
        }
        None => -1,
    }
}

#[cfg(test)]
mod generator_list_tests {
    use super::*;

    #[test]
    fn a_script_names_its_generator_module_and_string() {
        let text = b"// a map\r\nUseMapGenerator \"rmap_default\"\nStringTableID 1234 , x\nMapSize(3)\n";
        let g = script_generator(b"Continental.RMV", text).unwrap();
        assert_eq!(g.name, b"Continental");
        assert_eq!(g.module, b"rmap_default");
        assert_eq!(g.string_id, Some(1234));
        assert_eq!(script_generator(b"x.rmv", b"StringTableID 5\n"), None);
        // a string id that does not parse keeps the one before; the last generator line wins
        let text = b"StringTableID 7\nStringTableID x\nUseMapGenerator \"b\"\nUseMapGenerator\"c\"";
        let g = script_generator(b"a.rmv", text).unwrap();
        assert_eq!((g.module.as_slice(), g.string_id), (&b"c"[..], Some(7)));
        let g = script_generator(b"a.rmv", b"UseMapGenerator \"m\"\nStringTableID\n").unwrap();
        assert_eq!(g.string_id, None);
    }

    #[test]
    fn lines_are_fgets_chunks() {
        // a key past the first 1,023 bytes of a line starts a chunk of its own
        let mut text = vec![b'x'; 0x3ff];
        text.extend_from_slice(b"UseMapGenerator \"late\"\n");
        assert_eq!(script_generator(b"a.rmv", &text).unwrap().module, b"late");
        let mut text = vec![b'x'; 0x3fe];
        text.extend_from_slice(b"UseMapGenerator \"late\"\n");
        assert_eq!(script_generator(b"a.rmv", &text), None);
        // a NUL ends the line
        assert_eq!(script_generator(b"a.rmv", b"\0UseMapGenerator \"m\"\n"), None);
    }

    #[test]
    fn script_names_drop_their_spaces() {
        assert_eq!(script_name(b"Grass  Rough (x)"), Some(b"GrassRough(x)".to_vec()));
        assert_eq!(script_name(b" Space "), Some(b"Space".to_vec()));
        assert_eq!(script_name(b"   "), None);
        assert_eq!(script_name(b""), None);
        let mut out = [0 as c_char; 8];
        // SAFETY: 6 readable bytes, room for 6
        assert_eq!(unsafe { ee_rmap_script_name(c"a b c ".as_ptr(), 6, out.as_mut_ptr()) }, 3);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// Files by name, and the draws as a log (`draw_float` returns the midpoint).
    #[derive(Default)]
    struct TestHost {
        files: HashMap<Vec<u8>, Vec<u8>>,
        draws: Vec<String>,
        /// `draw_long`'s answer, clamped to the range (`minimum` when unset)
        long_answer: Option<i32>,
    }

    impl ScriptHost for TestHost {
        fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
            self.files.get(name).cloned()
        }
        fn draw_long(&mut self, minimum: i32, maximum: i32) -> i32 {
            self.draws.push(format!("L{minimum},{maximum}"));
            self.long_answer.map_or(minimum, |a| a.clamp(minimum, maximum))
        }
        fn draw_float(&mut self, minimum: f32, maximum: f32) -> f32 {
            self.draws.push(format!("F{minimum},{maximum}"));
            (minimum + maximum) * 0.5
        }
    }

    fn run(host: &mut TestHost, script: &str) -> (EeRmapScriptScalars, [[c_ulong; 2]; 7], ScriptLists) {
        host.files.insert(b"main".to_vec(), script.as_bytes().to_vec());
        let tables = ScriptTables {
            units: vec![(b"Villager".to_vec(), 1), (b"TownCenter".to_vec(), 4), (b"Wall".to_vec(), 1)],
            terrains: vec![(b"Grass".to_vec(), 7)],
            cliffs: vec![(b"RockCliff".to_vec(), 3)],
        };
        let mut s = EeRmapScriptScalars { map_size: 2, start_epoch: 1, player_count: 4, ..Default::default() };
        let mut spacing = [[9; 2]; 7];
        let lists = read_script(host, &tables, &mut s, &mut spacing, b"main");
        (s, spacing, lists)
    }

    #[test]
    fn scanf_numbers() {
        assert_eq!(scan_long(b"42"), Some(42));
        assert_eq!(scan_long(b"-7abc"), Some(-7));
        assert_eq!(scan_long(b"+3"), Some(3));
        assert_eq!(scan_long(b"0xFFFFFFFF"), Some(0));
        assert_eq!(scan_long(b"abc"), None);
        assert_eq!(scan_long(b"-"), None);
        assert_eq!(scan_long(b"4294967297"), Some(1));
        assert_eq!(scan_float(b".76"), Some(0.76));
        assert_eq!(scan_float(b"75.0"), Some(75.0));
        assert_eq!(scan_float(b"5."), Some(5.0));
        assert_eq!(scan_float(b"-2.5e1x"), Some(-25.0));
        assert_eq!(scan_float(b"1e"), Some(1.0));
        assert_eq!(scan_float(b"0xFFFFFFFF"), Some(4_294_967_296.0));
        assert_eq!(scan_float(b"0x1.8p1"), Some(3.0));
        assert_eq!(scan_float(b"inf"), Some(f32::INFINITY));
        assert!(scan_float(b"nan").unwrap().is_nan());
        assert_eq!(scan_float(b"."), None);
        assert_eq!(scan_float(b"x"), None);
        assert_eq!(float_to_long(3.9), 3);
        assert_eq!(float_to_long(-3.9), -3);
        assert_eq!(float_to_long(f32::NAN), i32::MIN);
        assert_eq!(float_to_long(3e9), i32::MIN);
    }

    #[test]
    fn defaults_and_rescaling() {
        let mut host = TestHost::default();
        let (s, spacing, lists) = run(&mut host, "");
        assert_eq!(spacing, [[0, u32::MAX as c_ulong]; 7]);
        assert_eq!(s.elevation_unit, 1.0 / 30.0);
        assert_eq!(s.flat_elevation, (0.0 + 10.0) * (1.0f32 / 30.0));
        assert_eq!(s.shallows_elevation, (-1.0f32 + 10.0) * (1.0f32 / 30.0) - 0.00001);
        assert_eq!(s.max_tree_elevation, 30.0 * (1.0f32 / 30.0));
        assert_eq!(s.resource_terrain_texture, [-1; 7]);
        assert_eq!(s.max_angle_between_team_members, 360.0);
        assert!(s.filter_height_map);
        assert_eq!(lists, ScriptLists::default());
        assert!(host.draws.is_empty());
    }

    #[test]
    fn attributes_defines_and_includes() {
        let mut host = TestHost::default();
        host.files.insert(
            b"inc".to_vec(),
            b"#define (kLand, Between(.5, .7))\n#define (kOnce, 1)\n#define (kOnce, 2)\nPercentLand kLand\n".to_vec(),
        );
        let script = "// a comment line\n\
            #include \"inc\"\n\
            MaxIntElevation 40\n\
            NumMoons kOnce\n\
            MaxAngleBetweenTeamMembers 90\n\
            PlaceUnit (TownCenter, 3, -2, 45)\n\
            PlaceWorldUnitInRing (Villager, 1, 1)\n\
            PlaceWall (Wall, 5, 1, -4, 6)\n\
            DeclareRandomMapTerrain (Grass, 11)\n\
            DeclareRandomMapTerrain (Grass, 12)\n\
            RandomMapTerrainRange (Grass, -10, -9, 99)\n\
            TerrainRegion (0, 50, Grass)\n\
            TerrainRegion (0, 50, 8)\n\
            DefineForest (5, 2, 0, 1)\n\
            DefineForest (6, 2)\n\
            AvailableForests (6, 5, 5)\n\
            ResourcePlacementLimit (Gold, 3, 4)\n\
            ResourcePerPlayer (Iron, 2)\n\
            ResourcePerPlayer (-1, 2)\n\
            DefineHerd (1, Villager, 9, 2, 3)\n\
            HerdPerPlayer (1, 2)\n\
            RandomHerdPerNeutral (1)\n\
            PlaceResourceInRing (Fish, 1, 2)\n\
            PlaceResourceInRing (7, 1, 2)\n\
            MapCharacteristics (WaterlessMap, EarthMap)\n\
            CliffTexture RockCliff\n\
            AvailableAnimals (1, 2)\n\
            UseMapGenerator \"Default\"\n";
        let (s, spacing, l) = run(&mut host, script);
        assert_eq!(host.draws, ["F0.5,0.7"]);
        assert_eq!(s.percent_land, 0.6);
        assert_eq!(s.max_int_elevation, 40);
        assert_eq!(s.num_moons, 1);
        assert_eq!(s.max_angle_between_team_members, 90.0 * DEGREES);
        assert_eq!(s.map_characteristics, 10);
        assert_eq!(s.cliff_texture, 3);
        assert_eq!(s.resource_per_player[6], 2);
        assert_eq!(spacing[1], [9, 16]);
        assert_eq!(l.features.len(), 2);
        assert_eq!((l.features[0].unit_type, l.features[0].x, l.features[0].y), (1, 3.0, -2.0));
        assert_eq!(l.features[0].angle, 45.0 * DEGREES);
        assert!(l.features[1].is_world_unit && l.features[1].in_ring && l.features[1].angle == PI);
        assert_eq!(l.walls, [EeRmapScriptWall { unit_type: 2, start_x: 5, start_y: 1, end_x: -4, end_y: 6 }]);
        assert_eq!(l.terrains.len(), 1);
        assert_eq!(&l.terrains[0].textures[..3], &[99, 99, 11]);
        assert_eq!(l.regions, [EeRmapScriptRegion { from: 0, to: 50, terrain: 0 }]);
        assert_eq!(l.forests.len(), 1);
        assert_eq!(l.forest_trees, [0, 1]);
        assert_eq!(l.tree_types, [0, 0]);
        assert_eq!(l.kinds, [EeRmapScriptKind { id: 1, unit_type: 0, area: 9, minimum: 2, maximum: 3 }]);
        assert_eq!(l.slots.len(), 3);
        assert!(l.slots[0].per_player && !l.slots[2].per_player && l.slots[2].random_kind);
        assert_eq!(l.rules, [EeRmapScriptRule { kind: 5, minimum: 1, maximum: 2 }]);
        assert_eq!(l.animals, [1, 2]);
        // the town centre (size 4) at (3, -2) and the wall from (-4, 1) to (5, 6)
        assert_eq!(l.flat_area, [6.0, -4.0, -2.0, 7.0]);
        let scale = 1.0f32 / 50.0;
        assert_eq!(s.elevation_unit, scale);
    }

    #[test]
    fn conditions_and_expressions() {
        let mut host = TestHost { long_answer: Some(30), ..Default::default() };
        let script = "#if EQ($PlayerCount, 4)\nNumMoons 1\n#elif 1\nNumMoons 2\n#else\nNumMoons 3\n#endif\n\
            #if 0\nMoonSize 1\n#if 1\nMoonSize 2\n#endif\n#elif PERCENT(50)\nMoonSize 3\n#endif\n\
            #if DEFINED(Gold)\nMoonResources OneOf(4, 5, 6)\n#endif\n\
            #if NOTDEFINED(Gold)\nRiverWidth 9\n#endif\n\
            #if AND(1, 0)\nWaterBorder 1\n#else\nWaterBorder Between(2, 8)\n#endif\n\
            ShallowsWidth OneOf(x, 7)\n\
            #endif\n#endif\n#else\nMaxLoopCount\n";
        let (s, _, _) = run(&mut host, script);
        assert_eq!(s.num_moons, 1);
        assert_eq!(s.moon_size, 3);
        assert_eq!(host.draws, ["L0,99", "L0,2", "L2,8", "L0,1"]);
        assert_eq!(s.moon_resources, 6);
        assert_eq!(s.river_width, 0);
        assert_eq!(s.water_border, 8);
        assert_eq!(s.shallows_width, 7);
        // a bare attribute reads an empty list: 0
        assert_eq!(s.max_loop_count, 0);
    }

    #[test]
    fn fgets_lines() {
        let mut host = TestHost::default();
        let mut script = vec![b' '; 600];
        script[..10].copy_from_slice(b"NumMoons 5");
        script[511..519].copy_from_slice(b"MoonSize");
        script.extend_from_slice(b" 7\nMoon\0Resources 9\n");
        host.files.insert(b"main".to_vec(), script);
        let tables = ScriptTables::default();
        let mut s = EeRmapScriptScalars::default();
        let mut spacing = [[0; 2]; 7];
        read_script(&mut host, &tables, &mut s, &mut spacing, b"main");
        assert_eq!(s.num_moons, 5);
        assert_eq!(s.moon_size, 7);
        assert_eq!(s.moon_resources, 5);
    }
}
