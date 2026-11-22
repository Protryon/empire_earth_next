//! The scenario editor's actions' host sequences composed of single C++ statements (port addition). The paint
//! actions' world edits (`ee_screenscpp_sea`, [`crate::screens::editor_actions`]), the whole actions' world and files
//! (`ee_screenscpp_sew`, [`crate::screens::editor_whole`]), the placements' (`ee_screenscpp_sep`,
//! [`crate::screens::editor_place`]) and the selection's (`ee_screenscpp_seu`) host cases that were C++ statement
//! sequences are answered here, in the C++'s order, from the one-statement cases of `ee_screenscpp_seax`
//! (`Scenario Editor Actions/EESEARust.cpp`, `EE_SCREENS_SEAX_*` 3000..). The adapters (`editor_actions_ffi.rs`,
//! `editor_whole_ffi.rs`) call [`sea`], [`sew`], [`sep`] and [`seu`] first and the C++ host for the ops they leave
//! (`None`), so the older modules and their recorded-stream tests see the same ops.

use ee_base::math::types::GeTransformation;

use crate::screens::editor_actions::{
    EE_SCREENS_SEA_ELEVATION_MODE, EE_SCREENS_SEA_PLANT, EE_SCREENS_SEA_POINT, EE_SCREENS_SEA_RAISE,
    EE_SCREENS_SEA_STOP_FOLLOW, EE_SCREENS_SEA_TERRAIN, EE_SCREENS_SEU_KILL_PART, EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC,
    EE_SCREENS_SEU_REMOVE_SIMPLE, EE_SCREENS_SEU_ROTATE, EE_SCREENS_SEU_UNLOAD,
};
use crate::screens::editor_ids::{
    EE_SCREENS_SE_ELEVATION_LOWER_BUTTON, EE_SCREENS_SE_ELEVATION_RAISE_BUTTON, EE_SCREENS_SE_MAP_LAND_CUTOFF,
    EE_SCREENS_SE_MAP_LAND_HEIGHT, EE_SCREENS_SE_MAP_WATER_CUTOFF, EE_SCREENS_SE_MAP_WATER_DEPTH,
    EE_SCREENS_SE_RB_UNIT_TYPES_OTHER_TECH_LIST, EE_SCREENS_SE_RB_UNIT_TYPES_TECH_LIST,
};
use crate::screens::editor_place::{
    EE_SCREENS_SEP_AIRPORT, EE_SCREENS_SEP_BOARD, EE_SCREENS_SEP_CREATE, EE_SCREENS_SEP_F_AIRPORT,
    EE_SCREENS_SEP_F_CLASS, EE_SCREENS_SEP_F_FLIES, EE_SCREENS_SEP_F_GRANARY, EE_SCREENS_SEP_F_SMALL,
    EE_SCREENS_SEP_GHOST, EE_SCREENS_SEP_GHOST_LOCK, EE_SCREENS_SEP_LOAD_INTO, EE_SCREENS_SEP_MOVE_TO,
    EE_SCREENS_SEP_RELEASE, EE_SCREENS_SEP_TILE_HEIGHT, EE_SCREENS_SEP_UNIT, EE_SCREENS_SEP_U_AIRPORT,
    EE_SCREENS_SEP_U_BUILDING, EE_SCREENS_SEP_U_COMPLEX, EE_SCREENS_SEP_U_EXISTS, EE_SCREENS_SEP_U_FLIES,
    EE_SCREENS_SEP_U_PITCHES,
};
use crate::screens::editor_whole::{
    SewElevation, EE_SCREENS_SEW_OP_ADD_PENDING, EE_SCREENS_SEW_OP_BUILD_GATE, EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE,
    EE_SCREENS_SEW_OP_GHOST_LOCK, EE_SCREENS_SEW_OP_MAP_DIMENSIONS, EE_SCREENS_SEW_OP_MINIMAP_REDRAW,
    EE_SCREENS_SEW_OP_MINIMAP_SET, EE_SCREENS_SEW_OP_NEW_MAP, EE_SCREENS_SEW_OP_PAINT_AREA, EE_SCREENS_SEW_OP_PAUSE,
    EE_SCREENS_SEW_OP_PLAN_WALL, EE_SCREENS_SEW_OP_PLAYER, EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT,
    EE_SCREENS_SEW_OP_POST_SCREEN_EVENT, EE_SCREENS_SEW_OP_QUICK_SAVE, EE_SCREENS_SEW_OP_RESET,
    EE_SCREENS_SEW_OP_RESUME, EE_SCREENS_SEW_OP_TECH_FILL, EE_SCREENS_SEW_OP_TILE_WATER, EE_SCREENS_SEW_OP_UNIT_PLAYER,
};

// ---- the statements (`ee_screenscpp_seax(op, p, q, t, i, f)`; a pointer comes back as the result)

/// `gUserInterface->PostAction(p)` (an `EEAction*`).
pub const EE_SCREENS_SEAX_POST_ACTION: u32 = 3000;
/// The action p released.
pub const EE_SCREENS_SEAX_DEREFERENCE_ACTION: u32 = 3001;
/// `gB529066 = false`, `gCameraFollowing = false`, `gB529068 = false` (each its own statement).
pub const EE_SCREENS_SEAX_FOLLOW_529066: u32 = 3002;
pub const EE_SCREENS_SEAX_CAMERA_FOLLOWING: u32 = 3003;
pub const EE_SCREENS_SEAX_FOLLOW_529068: u32 = 3004;
/// -> whether the mesh point (i0, i1) is valid; its height -> f0.
pub const EE_SCREENS_SEAX_MESH_VALID_POINT: u32 = 3005;
pub const EE_SCREENS_SEAX_MESH_Z: u32 = 3006;
/// The terrain i0's table entry: -> ambients only, its unit class count, its ambient unit chance.
pub const EE_SCREENS_SEAX_TERRAIN_AMBIENTS_ONLY: u32 = 3007;
pub const EE_SCREENS_SEAX_TERRAIN_NUM_CLASSES: u32 = 3008;
pub const EE_SCREENS_SEAX_TERRAIN_CHANCE: u32 = 3009;
/// -> whether the world player may place the class i0 on the tile (i1, i2); the unit made there.
pub const EE_SCREENS_SEAX_PLANT_CAN_PLACE: u32 = 3010;
pub const EE_SCREENS_SEAX_PLANT_CREATE: u32 = 3011;
/// -> whether the editor's radio button of id i0 is checked.
pub const EE_SCREENS_SEAX_RADIO_CHECKED: u32 = 3012;
/// The statics' longs (`sSeaxLong[i0] = i1`, -> `sSeaxLong[i0]`) and `EEMap::Raise` over them (f0..f3, i0 != 0).
pub const EE_SCREENS_SEAX_LONG_SET: u32 = 3013;
pub const EE_SCREENS_SEAX_LONG_GET: u32 = 3014;
pub const EE_SCREENS_SEAX_RAISE: u32 = 3015;
/// -> the map's tile (i0, i1) (`EEMapTile*`), its first unit node, a node's next and unit.
pub const EE_SCREENS_SEAX_TILE: u32 = 3016;
pub const EE_SCREENS_SEAX_TILE_FIRST_NODE: u32 = 3017;
pub const EE_SCREENS_SEAX_NODE_NEXT: u32 = 3018;
pub const EE_SCREENS_SEAX_NODE_UNIT: u32 = 3019;
/// The unit p (`EEUnit*`): -> on a tile's list; -> its complex unit's previous part (an `EEUnit*`); referenced;
/// -> a path point; -> a resource; emptied; killed; taken from its player; released.
pub const EE_SCREENS_SEAX_UNIT_ON_TILE: u32 = 3020;
pub const EE_SCREENS_SEAX_UNIT_PREVIOUS_PART: u32 = 3021;
pub const EE_SCREENS_SEAX_UNIT_REFERENCE: u32 = 3022;
pub const EE_SCREENS_SEAX_UNIT_PATH_POINT: u32 = 3023;
pub const EE_SCREENS_SEAX_UNIT_RESOURCE: u32 = 3024;
pub const EE_SCREENS_SEAX_UNIT_EMPTY: u32 = 3025;
pub const EE_SCREENS_SEAX_UNIT_KILL: u32 = 3026;
pub const EE_SCREENS_SEAX_UNIT_PLAYER_KILL: u32 = 3027;
pub const EE_SCREENS_SEAX_UNIT_DEREFERENCE: u32 = 3028;
/// -> the screen event i0 (`EECreateScreenEvent`); the event p posted; released.
pub const EE_SCREENS_SEAX_SCREEN_EVENT_NEW: u32 = 3030;
pub const EE_SCREENS_SEAX_EVENT_POST: u32 = 3031;
pub const EE_SCREENS_SEAX_EVENT_DEREFERENCE: u32 = 3032;
/// The quick save p (`EEAQuickSaveLoad`): not a load; written to its file.
pub const EE_SCREENS_SEAX_QUICK_SAVE_NOT_LOAD: u32 = 3033;
pub const EE_SCREENS_SEAX_QUICK_SAVE_WRITE: u32 = 3034;
/// The tech list of id i0 filled with the lines i1..i2.
pub const EE_SCREENS_SEAX_TECH_FILL: u32 = 3035;
/// The editor's paint area `GERectangle(left i0, top i1, right i2, bottom i3)`.
pub const EE_SCREENS_SEAX_PAINT_AREA: u32 = 3036;
/// The pause's statements: posted; the world's write lock and the UI copy lock let go; -> paused; `PlatSleep(1)`;
/// the locks taken again.
pub const EE_SCREENS_SEAX_PAUSE_POST: u32 = 3037;
pub const EE_SCREENS_SEAX_WORLD_WRITE_UNLOCK: u32 = 3038;
pub const EE_SCREENS_SEAX_UI_UNLOCK: u32 = 3039;
pub const EE_SCREENS_SEAX_GRAPHICS_PAUSED: u32 = 3040;
pub const EE_SCREENS_SEAX_SLEEP: u32 = 3041;
pub const EE_SCREENS_SEAX_UI_LOCK: u32 = 3042;
pub const EE_SCREENS_SEAX_WORLD_WRITE_LOCK: u32 = 3043;
/// The world reset; a new scenario manager; a new environment manager.
pub const EE_SCREENS_SEAX_WORLD_RESET: u32 = 3044;
pub const EE_SCREENS_SEAX_NEW_SCENARIO_MANAGER: u32 = 3045;
pub const EE_SCREENS_SEAX_NEW_ENVIRONMENT_MANAGER: u32 = 3046;
/// -> a new map (i0..i3); verified; set as the world's.
pub const EE_SCREENS_SEAX_MAP_NEW: u32 = 3047;
pub const EE_SCREENS_SEAX_MAP_VERIFY: u32 = 3048;
pub const EE_SCREENS_SEAX_MAP_SET: u32 = 3049;
/// The mesh tile (i0, i1) water when i2; the tile's height f0.
pub const EE_SCREENS_SEAX_MESH_SET_WATER: u32 = 3050;
pub const EE_SCREENS_SEAX_TILE_SET_HEIGHT: u32 = 3051;
/// -> a terrain paint of the terrain i0 (an `EEAction*`); its area (i0..i3).
pub const EE_SCREENS_SEAX_PAINT_NEW: u32 = 3052;
pub const EE_SCREENS_SEAX_PAINT_SET_AREA: u32 = 3053;
/// The minimap: given the map; `EEUIMiniMap::Update`; `RedrawAll`; `Redraw`; `Refresh`.
pub const EE_SCREENS_SEAX_MINIMAP_SET_MAP: u32 = 3054;
pub const EE_SCREENS_SEAX_MINIMAP_UPDATE: u32 = 3055;
pub const EE_SCREENS_SEAX_MINIMAP_REDRAW_ALL: u32 = 3056;
pub const EE_SCREENS_SEAX_MINIMAP_REDRAW: u32 = 3057;
pub const EE_SCREENS_SEAX_MINIMAP_REFRESH: u32 = 3058;
/// The renderer: the scene released; made again; resumed.
pub const EE_SCREENS_SEAX_GRAPHICS_RELEASE_SCENE: u32 = 3059;
pub const EE_SCREENS_SEAX_GRAPHICS_INITIALISE_SCENE: u32 = 3060;
pub const EE_SCREENS_SEAX_GRAPHICS_RESUME: u32 = 3061;
/// -> new game options; p's map size i0; its start epoch i0; its map dimensions into the longs 0, 1; deleted.
pub const EE_SCREENS_SEAX_OPTIONS_NEW: u32 = 3062;
pub const EE_SCREENS_SEAX_OPTIONS_MAP_SIZE: u32 = 3063;
pub const EE_SCREENS_SEAX_OPTIONS_EPOCH: u32 = 3064;
pub const EE_SCREENS_SEAX_OPTIONS_DIMENSIONS: u32 = 3065;
pub const EE_SCREENS_SEAX_OPTIONS_DELETE: u32 = 3066;
/// -> the player i0 (`EEPlayer*`); p's observer flag, whether it is human, its tech tree's start epoch.
pub const EE_SCREENS_SEAX_PLAYER: u32 = 3067;
pub const EE_SCREENS_SEAX_PLAYER_OBSERVER: u32 = 3068;
pub const EE_SCREENS_SEAX_PLAYER_HUMAN: u32 = 3069;
pub const EE_SCREENS_SEAX_PLAYER_START_EPOCH: u32 = 3070;
/// The random map: its players cleared, the player i0 added; the location zeroed; `CreateRandomMap(options p,
/// the players, local i0, the generator of style i1, the seed, the map, the location)`; the location's x, y, z.
pub const EE_SCREENS_SEAX_PLAYERS_CLEAR: u32 = 3071;
pub const EE_SCREENS_SEAX_PLAYERS_PUSH: u32 = 3072;
pub const EE_SCREENS_SEAX_POINT_ZERO: u32 = 3073;
pub const EE_SCREENS_SEAX_RANDOM_MAP: u32 = 3075;
pub const EE_SCREENS_SEAX_POINT_X: u32 = 3076;
pub const EE_SCREENS_SEAX_POINT_Y: u32 = 3077;
pub const EE_SCREENS_SEAX_POINT_Z: u32 = 3078;
/// The player p's pending units: their count; -> the unit i0; the unit p's media loaded; the pending unit i0 added
/// to the world; -> the unit p is a built land building; the player p loads the unit q's class buttons.
pub const EE_SCREENS_SEAX_PLAYER_PENDING_COUNT: u32 = 3079;
pub const EE_SCREENS_SEAX_PENDING: u32 = 3081;
pub const EE_SCREENS_SEAX_UNIT_LOAD_MEDIA: u32 = 3082;
pub const EE_SCREENS_SEAX_ADD_UNIT_ENTRY: u32 = 3083;
pub const EE_SCREENS_SEAX_UNIT_BUILT_LAND_BUILDING: u32 = 3084;
pub const EE_SCREENS_SEAX_PLAYER_LOAD_BUTTONS: u32 = 3085;
/// The wall plan's list cleared; `EEPlanWall` from (f0, f1) to (f2, f3) of the player i0's class of the technology i1.
pub const EE_SCREENS_SEAX_WALLS_CLEAR: u32 = 3086;
pub const EE_SCREENS_SEAX_PLAN_WALL: u32 = 3087;
/// The ghosts: their lock taken; let go; -> their count; -> the ghost i0 (`EEGhostUnit*`).
pub const EE_SCREENS_SEAX_GHOST_ENTER: u32 = 3088;
pub const EE_SCREENS_SEAX_GHOST_LEAVE: u32 = 3089;
pub const EE_SCREENS_SEAX_GHOST_COUNT: u32 = 3090;
pub const EE_SCREENS_SEAX_GHOST: u32 = 3091;
/// `EEBuildGate` on the tile (i0, i1) for the player i2, the technology i3, the direction i4.
pub const EE_SCREENS_SEAX_BUILD_GATE: u32 = 3092;
/// -> the world's unit of id i0 (an `RTSUnit*`); -> p's player's id.
pub const EE_SCREENS_SEAX_UNIT_BY_ID: u32 = 3093;
pub const EE_SCREENS_SEAX_RTS_UNIT_PLAYER_ID: u32 = 3094;
/// The elevation file pair: the map tab's list selection read; -> it; -> a new file spec; its directory; its file
/// (the selection's name, i0 ".txt" else ".dat"); -> its `Open` result; i0 bytes read into q; -> the count read;
/// closed; deleted; -> the edit box i0's number.
pub const EE_SCREENS_SEAX_ELEVATION_SELECTION: u32 = 3095;
pub const EE_SCREENS_SEAX_SELECTION_GET: u32 = 3096;
pub const EE_SCREENS_SEAX_SPEC_NEW: u32 = 3097;
pub const EE_SCREENS_SEAX_SPEC_DIRECTORY: u32 = 3098;
pub const EE_SCREENS_SEAX_SPEC_FILE: u32 = 3099;
pub const EE_SCREENS_SEAX_SPEC_OPEN: u32 = 3100;
pub const EE_SCREENS_SEAX_SPEC_READ: u32 = 3101;
pub const EE_SCREENS_SEAX_READ_COUNT: u32 = 3102;
pub const EE_SCREENS_SEAX_SPEC_CLOSE: u32 = 3103;
pub const EE_SCREENS_SEAX_SPEC_DELETE: u32 = 3104;
pub const EE_SCREENS_SEAX_EDIT_INT: u32 = 3105;
/// The ghost p: -> its class's type; -> the class of type i0 (`EEUnitClass*`); the class p: -> small; -> flies over;
/// its cruise altitude -> f0; the ghost p's tile, class tile size, animation, transformation (-> t), can place.
pub const EE_SCREENS_SEAX_GHOST_TYPE: u32 = 3111;
pub const EE_SCREENS_SEAX_CLASS: u32 = 3112;
pub const EE_SCREENS_SEAX_CLASS_SMALL: u32 = 3113;
pub const EE_SCREENS_SEAX_CLASS_FLIES: u32 = 3114;
pub const EE_SCREENS_SEAX_CLASS_CRUISE: u32 = 3115;
pub const EE_SCREENS_SEAX_GHOST_TILE_X: u32 = 3116;
pub const EE_SCREENS_SEAX_GHOST_TILE_Y: u32 = 3117;
pub const EE_SCREENS_SEAX_GHOST_CLASS_TILE_SIZE: u32 = 3118;
pub const EE_SCREENS_SEAX_GHOST_ANIMATION: u32 = 3119;
pub const EE_SCREENS_SEAX_GHOST_TRANSFORMATION: u32 = 3120;
pub const EE_SCREENS_SEAX_GHOST_CAN_PLACE: u32 = 3121;
/// The tile p's height -> f0.
pub const EE_SCREENS_SEAX_TILE_HEIGHT: u32 = 3122;
/// -> the unit of id i0 (the placements' handles: `EEUnit*`, `EEComplexUnit*`).
pub const EE_SCREENS_SEAX_SEP_UNIT: u32 = 3123;
pub const EE_SCREENS_SEAX_SEP_COMPLEX: u32 = 3124;
/// The unit p: -> its complex unit; -> its player.
pub const EE_SCREENS_SEAX_UNIT_COMPLEX: u32 = 3125;
pub const EE_SCREENS_SEAX_UNIT_PLAYER: u32 = 3126;
/// -> the complex unit p's player is q; -> the unit p's class transports; -> the complex unit p has room for the
/// class q; -> the class p can enter the complex unit q's class.
pub const EE_SCREENS_SEAX_COMPLEX_PLAYER_IS: u32 = 3127;
pub const EE_SCREENS_SEAX_UNIT_TRANSPORTS: u32 = 3128;
pub const EE_SCREENS_SEAX_COMPLEX_ROOM_FOR: u32 = 3129;
pub const EE_SCREENS_SEAX_CLASS_CAN_ENTER: u32 = 3130;
/// The complex unit p referenced; -> its id.
pub const EE_SCREENS_SEAX_COMPLEX_REFERENCE: u32 = 3131;
pub const EE_SCREENS_SEAX_COMPLEX_ID: u32 = 3132;
/// -> the class p's type; -> a unit of it made at t (or at the world's transformation) for the player i0.
pub const EE_SCREENS_SEAX_CLASS_TYPE: u32 = 3133;
pub const EE_SCREENS_SEAX_CREATE_AT: u32 = 3134;
pub const EE_SCREENS_SEAX_CREATE_AT_WORLD: u32 = 3135;
/// The unit p's chain cloakable; cloaked; t's closest tile into the longs 0, 1; the unit's tile x, y from them; its
/// tile updated.
pub const EE_SCREENS_SEAX_UNIT_CLOAKABLE: u32 = 3136;
pub const EE_SCREENS_SEAX_UNIT_CLOAKED: u32 = 3137;
pub const EE_SCREENS_SEAX_CLOSEST_TILE: u32 = 3138;
pub const EE_SCREENS_SEAX_UNIT_SET_TILE_X: u32 = 3139;
pub const EE_SCREENS_SEAX_UNIT_SET_TILE_Y: u32 = 3140;
pub const EE_SCREENS_SEAX_UNIT_UPDATE_TILE: u32 = 3141;
/// The player p releases the media of the unit type i0.
pub const EE_SCREENS_SEAX_PLAYER_UNLOAD_MEDIA: u32 = 3142;
/// -> the class p is a land building.
pub const EE_SCREENS_SEAX_CLASS_LAND_BUILDING: u32 = 3143;
/// The unit p's complex unit: built; the animation i0; -> its stats' power is above 0; given its power.
pub const EE_SCREENS_SEAX_UNIT_SET_BUILT: u32 = 3144;
pub const EE_SCREENS_SEAX_UNIT_SET_ANIMATION: u32 = 3145;
pub const EE_SCREENS_SEAX_UNIT_POWERED: u32 = 3146;
pub const EE_SCREENS_SEAX_UNIT_SET_POWER: u32 = 3147;
/// The player p's pending units grown by one (`AddPendingUnits(0, 1)`).
pub const EE_SCREENS_SEAX_PLAYER_ADD_PENDING: u32 = 3148;
/// The complex unit p loads the unit q's complex unit (`GetComplexUnit`).
pub const EE_SCREENS_SEAX_COMPLEX_LOAD_UNIT: u32 = 3149;
/// -> the complex unit p is editor-selected; the editor selects it; the unit tab's selection updated; p released.
pub const EE_SCREENS_SEAX_COMPLEX_EDITOR_SELECTED: u32 = 3150;
pub const EE_SCREENS_SEAX_EDITOR_SELECT_COMPLEX: u32 = 3151;
pub const EE_SCREENS_SEAX_EDITOR_UPDATE_SELECTION: u32 = 3152;
pub const EE_SCREENS_SEAX_COMPLEX_DEREFERENCE: u32 = 3153;
/// -> the unit p's id; -> the complex unit p's next part; -> the unit p's class; -> the class p pitches.
pub const EE_SCREENS_SEAX_UNIT_ID: u32 = 3154;
pub const EE_SCREENS_SEAX_COMPLEX_NEXT: u32 = 3155;
pub const EE_SCREENS_SEAX_UNIT_CLASS: u32 = 3156;
pub const EE_SCREENS_SEAX_CLASS_PITCHES: u32 = 3157;
/// The complex unit p's location -> t; -> its class.
pub const EE_SCREENS_SEAX_COMPLEX_LOCATION: u32 = 3158;
pub const EE_SCREENS_SEAX_COMPLEX_CLASS: u32 = 3159;
/// The complex unit p loads the unit q's `mComplexUnit`.
pub const EE_SCREENS_SEAX_COMPLEX_LOAD_COMPLEX: u32 = 3160;
/// `MoveTo(i0, i1, true)` of the complex unit p; of the unit p.
pub const EE_SCREENS_SEAX_COMPLEX_MOVE_TO: u32 = 3161;
pub const EE_SCREENS_SEAX_UNIT_MOVE_TO: u32 = 3162;
/// The complex unit p: -> its facing quarter; -> its facing; its facing set to i0; its class levels the ground at
/// its tile (f0 in and out).
pub const EE_SCREENS_SEAX_FACING_QUARTER: u32 = 3163;
pub const EE_SCREENS_SEAX_FACING: u32 = 3164;
pub const EE_SCREENS_SEAX_SET_FACING: u32 = 3165;
pub const EE_SCREENS_SEAX_LEVEL_GROUND: u32 = 3166;
/// The rotation: the unit p's complex unit's location into the statics' transformation; its orientation the yaw f0;
/// set as the complex unit's.
pub const EE_SCREENS_SEAX_T_FROM_LOCATION: u32 = 3170;
pub const EE_SCREENS_SEAX_T_YPR: u32 = 3171;
pub const EE_SCREENS_SEAX_T_APPLY: u32 = 3172;
/// -> the complex unit p is an air traffic building; it leaves its player's air traffic list.
pub const EE_SCREENS_SEAX_COMPLEX_ATC_AIRPORT: u32 = 3173;
pub const EE_SCREENS_SEAX_ATC_REMOVE: u32 = 3174;
/// The complex unit p killed; the player p takes the complex unit q off.
pub const EE_SCREENS_SEAX_COMPLEX_KILL: u32 = 3175;
pub const EE_SCREENS_SEAX_PLAYER_KILL_COMPLEX: u32 = 3176;
/// The complex unit p's cargo q (an index) unloaded at a copy of its location.
pub const EE_SCREENS_SEAX_COMPLEX_UNLOAD: u32 = 3177;
/// The lasso's ids: cleared; the units of type i0 in the rectangle p picked; -> their count; -> the id i0.
pub const EE_SCREENS_SEAX_IDS_CLEAR: u32 = 3178;
pub const EE_SCREENS_SEAX_PICK: u32 = 3179;
pub const EE_SCREENS_SEAX_IDS_COUNT: u32 = 3180;
pub const EE_SCREENS_SEAX_ID: u32 = 3181;

/// The unit types the placements test (`kUnitTypeAirport`, `kUnitTypeGranary`, `kUnitTypeGranaryImproved`,
/// `kUnitTypeCameraObject`; the C++ static_asserts them).
pub const EE_SCREENS_SEAX_TYPE_AIRPORT: i32 = 0x42;
pub const EE_SCREENS_SEAX_TYPE_GRANARY: i32 = 0x34;
pub const EE_SCREENS_SEAX_TYPE_GRANARY_IMPROVED: i32 = 0x2ba;
pub const EE_SCREENS_SEAX_TYPE_CAMERA_OBJECT: i32 = 0x167;

/// One statement's arguments: two objects, a transformation (sixteen floats, row by row), integers and floats (in
/// and out).
#[derive(Clone, Debug, PartialEq)]
pub struct SeaxArgs {
    pub p: usize,
    pub q: usize,
    pub t: [f32; 16],
    pub i: [i32; 8],
    pub f: [f32; 4],
}

impl SeaxArgs {
    fn new(p: usize, q: usize, ints: &[i32], floats: &[f32]) -> Self {
        let mut a = SeaxArgs { p, q, t: [0.0; 16], i: [0; 8], f: [0.0; 4] };
        a.i[..ints.len()].copy_from_slice(ints);
        a.f[..floats.len()].copy_from_slice(floats);
        a
    }
}

/// The statements' host (`ee_screenscpp_seax`).
pub trait SeaxHost {
    fn x(&mut self, op: u32, a: &mut SeaxArgs) -> isize;
}

/// A statement on p, q and integers -> its result.
fn c<H: SeaxHost + ?Sized>(h: &mut H, op: u32, p: usize, q: usize, ints: &[i32]) -> isize {
    h.x(op, &mut SeaxArgs::new(p, q, ints, &[]))
}

/// A statement with floats in -> its arguments after (the floats and transformation out).
fn cf<H: SeaxHost + ?Sized>(h: &mut H, op: u32, p: usize, ints: &[i32], floats: &[f32]) -> (isize, SeaxArgs) {
    let mut a = SeaxArgs::new(p, 0, ints, floats);
    let r = h.x(op, &mut a);
    (r, a)
}

/// A statement over the transformation `t` (in and out) -> its result.
fn ct<H: SeaxHost + ?Sized>(h: &mut H, op: u32, p: usize, t: &mut GeTransformation, ints: &[i32]) -> isize {
    let mut a = SeaxArgs::new(p, 0, ints, &[]);
    a.t = flat(t);
    let r = h.x(op, &mut a);
    *t = unflat(&a.t);
    r
}

fn flat(t: &GeTransformation) -> [f32; 16] {
    let mut out = [0.0; 16];
    for (k, v) in out.iter_mut().enumerate() {
        *v = t.m[k / 4][k % 4];
    }
    out
}

fn unflat(v: &[f32; 16]) -> GeTransformation {
    let mut t = GeTransformation { m: [[0.0; 4]; 4] };
    for (k, x) in v.iter().enumerate() {
        t.m[k / 4][k % 4] = *x;
    }
    t
}

/// A unit removed as the bulldozer and the deletion remove it (a resource emptied first, killed, taken from its
/// player); not released.
fn remove<H: SeaxHost + ?Sized>(h: &mut H, unit: usize) {
    if c(h, EE_SCREENS_SEAX_UNIT_RESOURCE, unit, 0, &[]) != 0 {
        c(h, EE_SCREENS_SEAX_UNIT_EMPTY, unit, 0, &[]);
    }
    c(h, EE_SCREENS_SEAX_UNIT_KILL, unit, 0, &[]);
    c(h, EE_SCREENS_SEAX_UNIT_PLAYER_KILL, unit, 0, &[]);
}

// ---- the paint actions' host (`ee_screenscpp_sea`)

/// The `ee_screenscpp_sea` op answered by its statements, `None` for the C++'s own.
pub fn sea<H: SeaxHost + ?Sized>(h: &mut H, op: u32, i: &mut [i32; 6], f: &mut [f32; 5]) -> Option<i32> {
    match op {
        EE_SCREENS_SEA_STOP_FOLLOW => {
            c(h, EE_SCREENS_SEAX_FOLLOW_529066, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_CAMERA_FOLLOWING, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_FOLLOW_529068, 0, 0, &[]);
            Some(0)
        }
        EE_SCREENS_SEA_POINT => {
            if c(h, EE_SCREENS_SEAX_MESH_VALID_POINT, 0, 0, &[i[0], i[1]]) == 0 {
                return Some(0);
            }
            f[0] = cf(h, EE_SCREENS_SEAX_MESH_Z, 0, &[i[0], i[1]], &[]).1.f[0];
            Some(1)
        }
        EE_SCREENS_SEA_TERRAIN => {
            i[1] = c(h, EE_SCREENS_SEAX_TERRAIN_AMBIENTS_ONLY, 0, 0, &[i[0]]) as i32;
            i[2] = c(h, EE_SCREENS_SEAX_TERRAIN_NUM_CLASSES, 0, 0, &[i[0]]) as i32;
            Some(c(h, EE_SCREENS_SEAX_TERRAIN_CHANCE, 0, 0, &[i[0]]) as i32)
        }
        EE_SCREENS_SEA_PLANT => {
            let args = [i[0], i[1], i[2]];
            if c(h, EE_SCREENS_SEAX_PLANT_CAN_PLACE, 0, 0, &args) != 0 {
                c(h, EE_SCREENS_SEAX_PLANT_CREATE, 0, 0, &args);
            }
            Some(0)
        }
        EE_SCREENS_SEA_ELEVATION_MODE => {
            if c(h, EE_SCREENS_SEAX_RADIO_CHECKED, 0, 0, &[EE_SCREENS_SE_ELEVATION_RAISE_BUTTON]) != 0 {
                return Some(0);
            }
            if c(h, EE_SCREENS_SEAX_RADIO_CHECKED, 0, 0, &[EE_SCREENS_SE_ELEVATION_LOWER_BUTTON]) != 0 {
                return Some(1);
            }
            Some(2)
        }
        EE_SCREENS_SEA_RAISE => {
            for (k, v) in i.iter().take(4).enumerate() {
                c(h, EE_SCREENS_SEAX_LONG_SET, 0, 0, &[k as i32, *v]);
            }
            cf(h, EE_SCREENS_SEAX_RAISE, 0, &[i[4]], &[f[0], f[1], f[2], f[3]]);
            for (k, v) in i.iter_mut().take(4).enumerate() {
                *v = c(h, EE_SCREENS_SEAX_LONG_GET, 0, 0, &[k as i32]) as i32;
            }
            Some(0)
        }
        _ => None,
    }
}

/// The units on the tile (x, y), each the first part of its unit and referenced, with whether it is a path point,
/// in the tile's list order (the original `ee_screenscpp_sea_units`).
pub fn tile_units<H: SeaxHost + ?Sized>(h: &mut H, x: i32, y: i32) -> Vec<(usize, bool)> {
    let mut out = Vec::new();
    let tile = c(h, EE_SCREENS_SEAX_TILE, 0, 0, &[x, y]) as usize;
    if tile == 0 {
        return out;
    }
    let mut node = c(h, EE_SCREENS_SEAX_TILE_FIRST_NODE, tile, 0, &[]) as usize;
    while node != 0 {
        let mut unit = c(h, EE_SCREENS_SEAX_NODE_UNIT, node, 0, &[]) as usize;
        if c(h, EE_SCREENS_SEAX_UNIT_ON_TILE, unit, 0, &[]) != 0 {
            loop {
                let previous = c(h, EE_SCREENS_SEAX_UNIT_PREVIOUS_PART, unit, 0, &[]) as usize;
                if previous == 0 {
                    break;
                }
                unit = previous;
            }
        }
        c(h, EE_SCREENS_SEAX_UNIT_REFERENCE, unit, 0, &[]);
        let path = c(h, EE_SCREENS_SEAX_UNIT_PATH_POINT, unit, 0, &[]) != 0;
        out.push((unit, path));
        node = c(h, EE_SCREENS_SEAX_NODE_NEXT, node, 0, &[]) as usize;
    }
    out
}

/// A unit removed as the bulldozer removes it, its reference released (the original `ee_screenscpp_sea_remove`).
pub fn remove_unit<H: SeaxHost + ?Sized>(h: &mut H, unit: usize) {
    remove(h, unit);
    c(h, EE_SCREENS_SEAX_UNIT_DEREFERENCE, unit, 0, &[]);
}

// ---- the whole actions' host (`ee_screenscpp_sew`)

/// The `ee_screenscpp_sew` op answered by its statements, `None` for the C++'s own (and for the random map:
/// [`random_map`]).
pub fn sew<H: SeaxHost + ?Sized>(h: &mut H, op: u32, p: usize, i: &mut [i32; 8], f: &mut [f32; 4]) -> Option<i32> {
    match op {
        EE_SCREENS_SEW_OP_POST_SCREEN_EVENT => {
            let event = c(h, EE_SCREENS_SEAX_SCREEN_EVENT_NEW, 0, 0, &[i[0]]) as usize;
            c(h, EE_SCREENS_SEAX_EVENT_POST, event, 0, &[]);
            c(h, EE_SCREENS_SEAX_EVENT_DEREFERENCE, event, 0, &[]);
        }
        EE_SCREENS_SEW_OP_QUICK_SAVE => {
            c(h, EE_SCREENS_SEAX_QUICK_SAVE_NOT_LOAD, p, 0, &[]);
            c(h, EE_SCREENS_SEAX_QUICK_SAVE_WRITE, p, 0, &[]);
            c(h, EE_SCREENS_SEAX_POST_ACTION, p, 0, &[]);
        }
        EE_SCREENS_SEW_OP_TECH_FILL => {
            let list = if i[0] != 0 {
                EE_SCREENS_SE_RB_UNIT_TYPES_TECH_LIST
            } else {
                EE_SCREENS_SE_RB_UNIT_TYPES_OTHER_TECH_LIST
            };
            c(h, EE_SCREENS_SEAX_TECH_FILL, 0, 0, &[list, i[1], i[2]]);
        }
        EE_SCREENS_SEW_OP_PAINT_AREA => {
            c(h, EE_SCREENS_SEAX_PAINT_AREA, 0, 0, &[i[0], i[1], i[2], i[3]]);
        }
        EE_SCREENS_SEW_OP_PAUSE => {
            c(h, EE_SCREENS_SEAX_PAUSE_POST, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_WORLD_WRITE_UNLOCK, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_UI_UNLOCK, 0, 0, &[]);
            while c(h, EE_SCREENS_SEAX_GRAPHICS_PAUSED, 0, 0, &[]) == 0 {
                c(h, EE_SCREENS_SEAX_SLEEP, 0, 0, &[]);
            }
            c(h, EE_SCREENS_SEAX_UI_LOCK, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_WORLD_WRITE_LOCK, 0, 0, &[]);
        }
        EE_SCREENS_SEW_OP_RESET => {
            c(h, EE_SCREENS_SEAX_WORLD_RESET, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_NEW_SCENARIO_MANAGER, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_NEW_ENVIRONMENT_MANAGER, 0, 0, &[]);
        }
        EE_SCREENS_SEW_OP_NEW_MAP => {
            let map = c(h, EE_SCREENS_SEAX_MAP_NEW, 0, 0, &[i[0], i[1], i[2], i[3]]) as usize;
            c(h, EE_SCREENS_SEAX_MAP_VERIFY, map, 0, &[]);
            c(h, EE_SCREENS_SEAX_MAP_SET, map, 0, &[]);
        }
        EE_SCREENS_SEW_OP_TILE_WATER => {
            c(h, EE_SCREENS_SEAX_MESH_SET_WATER, 0, 0, &[i[0], i[1], i[2]]);
            cf(h, EE_SCREENS_SEAX_TILE_SET_HEIGHT, 0, &[i[0], i[1]], &[f[0]]);
        }
        EE_SCREENS_SEW_OP_POST_AMBIENT_PAINT => {
            let paint = c(h, EE_SCREENS_SEAX_PAINT_NEW, 0, 0, &[i[0]]) as usize;
            let last = i[1].wrapping_sub(1);
            c(h, EE_SCREENS_SEAX_PAINT_SET_AREA, paint, 0, &[0, last, last, 0]);
            c(h, EE_SCREENS_SEAX_POST_ACTION, paint, 0, &[]);
            c(h, EE_SCREENS_SEAX_DEREFERENCE_ACTION, paint, 0, &[]);
        }
        EE_SCREENS_SEW_OP_MINIMAP_SET => {
            c(h, EE_SCREENS_SEAX_MINIMAP_SET_MAP, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_MINIMAP_UPDATE, 0, 0, &[]);
        }
        EE_SCREENS_SEW_OP_MINIMAP_REDRAW => {
            c(h, EE_SCREENS_SEAX_MINIMAP_REDRAW_ALL, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_MINIMAP_REDRAW, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_MINIMAP_REFRESH, 0, 0, &[]);
        }
        EE_SCREENS_SEW_OP_RESUME => {
            c(h, EE_SCREENS_SEAX_GRAPHICS_RELEASE_SCENE, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_GRAPHICS_INITIALISE_SCENE, 0, 0, &[]);
            c(h, EE_SCREENS_SEAX_GRAPHICS_RESUME, 0, 0, &[]);
        }
        EE_SCREENS_SEW_OP_MAP_DIMENSIONS => {
            let options = c(h, EE_SCREENS_SEAX_OPTIONS_NEW, 0, 0, &[]) as usize;
            c(h, EE_SCREENS_SEAX_OPTIONS_MAP_SIZE, options, 0, &[i[0]]);
            c(h, EE_SCREENS_SEAX_OPTIONS_DIMENSIONS, options, 0, &[]);
            i[0] = c(h, EE_SCREENS_SEAX_LONG_GET, 0, 0, &[0]) as i32;
            i[1] = c(h, EE_SCREENS_SEAX_LONG_GET, 0, 0, &[1]) as i32;
            c(h, EE_SCREENS_SEAX_OPTIONS_DELETE, options, 0, &[]);
        }
        EE_SCREENS_SEW_OP_PLAYER => {
            let player = c(h, EE_SCREENS_SEAX_PLAYER, 0, 0, &[i[0]]) as usize;
            if player == 0 {
                return Some(0);
            }
            i[1] = c(h, EE_SCREENS_SEAX_PLAYER_OBSERVER, player, 0, &[]) as i32;
            i[2] = c(h, EE_SCREENS_SEAX_PLAYER_HUMAN, player, 0, &[]) as i32;
            i[3] = c(h, EE_SCREENS_SEAX_PLAYER_START_EPOCH, player, 0, &[]) as i32;
            return Some(1);
        }
        EE_SCREENS_SEW_OP_ADD_PENDING => {
            let player = c(h, EE_SCREENS_SEAX_PLAYER, 0, 0, &[i[0]]) as usize;
            if player == 0 {
                return Some(0);
            }
            let mut j: usize = 0;
            while j < c(h, EE_SCREENS_SEAX_PLAYER_PENDING_COUNT, player, 0, &[]) as usize {
                let unit = c(h, EE_SCREENS_SEAX_PENDING, player, 0, &[j as i32]) as usize;
                c(h, EE_SCREENS_SEAX_UNIT_LOAD_MEDIA, unit, 0, &[]);
                c(h, EE_SCREENS_SEAX_ADD_UNIT_ENTRY, player, 0, &[j as i32]);
                if c(h, EE_SCREENS_SEAX_UNIT_BUILT_LAND_BUILDING, unit, 0, &[]) != 0 {
                    c(h, EE_SCREENS_SEAX_PLAYER_LOAD_BUTTONS, player, unit, &[]);
                }
                j += 1;
            }
        }
        EE_SCREENS_SEW_OP_PLAN_WALL => {
            c(h, EE_SCREENS_SEAX_WALLS_CLEAR, 0, 0, &[]);
            cf(h, EE_SCREENS_SEAX_PLAN_WALL, 0, &[i[0], i[1]], &[f[0], f[1], f[2], f[3]]);
        }
        EE_SCREENS_SEW_OP_GHOST_LOCK => {
            ghost_lock(h, i[0]);
        }
        EE_SCREENS_SEW_OP_GHOSTS_PLACEABLE => {
            let mut k: usize = 0;
            while k < c(h, EE_SCREENS_SEAX_GHOST_COUNT, 0, 0, &[]) as usize {
                let ghost = c(h, EE_SCREENS_SEAX_GHOST, 0, 0, &[k as i32]) as usize;
                if c(h, EE_SCREENS_SEAX_GHOST_CAN_PLACE, ghost, 0, &[]) == 0 {
                    return Some(0);
                }
                k += 1;
            }
            return Some(1);
        }
        EE_SCREENS_SEW_OP_BUILD_GATE => {
            c(h, EE_SCREENS_SEAX_BUILD_GATE, 0, 0, &[i[0], i[1], i[2], i[3], i[4]]);
        }
        EE_SCREENS_SEW_OP_UNIT_PLAYER => {
            let unit = c(h, EE_SCREENS_SEAX_UNIT_BY_ID, 0, 0, &[i[0]]) as usize;
            if unit == 0 {
                return Some(0);
            }
            i[1] = c(h, EE_SCREENS_SEAX_RTS_UNIT_PLAYER_ID, unit, 0, &[]) as i32;
            return Some(1);
        }
        _ => return None,
    }
    Some(0)
}

fn ghost_lock<H: SeaxHost + ?Sized>(h: &mut H, take: i32) {
    if take != 0 {
        c(h, EE_SCREENS_SEAX_GHOST_ENTER, 0, 0, &[]);
    } else {
        c(h, EE_SCREENS_SEAX_GHOST_LEAVE, 0, 0, &[]);
    }
}

/// `EE_SCREENS_SEW_OP_RANDOM_MAP`: `EERandomMap::CreateRandomMap` for the players (indices), the local index
/// `local`, the generator of `style`, the start `epoch`, the map `size` -> the camera's location.
pub fn random_map<H: SeaxHost + ?Sized>(
    h: &mut H,
    players: &[i32],
    local: i32,
    style: i32,
    epoch: i32,
    size: i32,
) -> [f32; 3] {
    let options = c(h, EE_SCREENS_SEAX_OPTIONS_NEW, 0, 0, &[]) as usize;
    c(h, EE_SCREENS_SEAX_OPTIONS_MAP_SIZE, options, 0, &[size]);
    c(h, EE_SCREENS_SEAX_OPTIONS_EPOCH, options, 0, &[epoch]);
    c(h, EE_SCREENS_SEAX_PLAYERS_CLEAR, 0, 0, &[]);
    for &player in players {
        c(h, EE_SCREENS_SEAX_PLAYERS_PUSH, 0, 0, &[player]);
    }
    c(h, EE_SCREENS_SEAX_POINT_ZERO, 0, 0, &[]);
    c(h, EE_SCREENS_SEAX_RANDOM_MAP, options, 0, &[local, style]);
    let x = cf(h, EE_SCREENS_SEAX_POINT_X, 0, &[], &[]).1.f[0];
    let y = cf(h, EE_SCREENS_SEAX_POINT_Y, 0, &[], &[]).1.f[0];
    let z = cf(h, EE_SCREENS_SEAX_POINT_Z, 0, &[], &[]).1.f[0];
    c(h, EE_SCREENS_SEAX_OPTIONS_DELETE, options, 0, &[]);
    [x, y, z]
}

/// The two numbers `sscanf("%ld%ld")` reads from the line (0 for one it cannot read), as the original's unsigned
/// longs (32 bits).
fn scan_two(line: &[u8]) -> (u32, u32) {
    let mut at = 0;
    let mut one = || -> Option<u32> {
        while at < line.len() && matches!(line[at], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
            at += 1;
        }
        let negative = at < line.len() && line[at] == b'-';
        if at < line.len() && (line[at] == b'-' || line[at] == b'+') {
            at += 1;
        }
        let start = at;
        let mut value: i64 = 0;
        while at < line.len() && line[at].is_ascii_digit() {
            value = value.wrapping_mul(10).wrapping_add(i64::from(line[at] - b'0'));
            at += 1;
        }
        if at == start {
            return None;
        }
        Some((if negative { value.wrapping_neg() } else { value }) as u32)
    };
    match one() {
        None => (0, 0),
        Some(width) => (width, one().unwrap_or(0)),
    }
}

/// The map tab's picked elevation file pair (`data\Scenarios\Elevation Maps\NAME.dat` and `.txt`: "WIDTH HEIGHT",
/// then the shorts) read whole, with the tab's water cutoff and depth, land height and cutoff (the original
/// `ee_screenscpp_sew_elevation`); `None` when none is picked or the files do not read whole.
pub fn elevation<H: SeaxHost + ?Sized>(h: &mut H) -> Option<SewElevation> {
    c(h, EE_SCREENS_SEAX_ELEVATION_SELECTION, 0, 0, &[]);
    if c(h, EE_SCREENS_SEAX_SELECTION_GET, 0, 0, &[]) == 0 {
        return None;
    }
    let map = c(h, EE_SCREENS_SEAX_SPEC_NEW, 0, 0, &[]) as usize;
    let info = c(h, EE_SCREENS_SEAX_SPEC_NEW, 0, 0, &[]) as usize;
    c(h, EE_SCREENS_SEAX_SPEC_DIRECTORY, map, 0, &[]);
    c(h, EE_SCREENS_SEAX_SPEC_FILE, map, 0, &[0]);
    c(h, EE_SCREENS_SEAX_SPEC_DIRECTORY, info, 0, &[]);
    c(h, EE_SCREENS_SEAX_SPEC_FILE, info, 0, &[1]);
    let mut result = None;
    if c(h, EE_SCREENS_SEAX_SPEC_OPEN, map, 0, &[]) == 0 {
        if c(h, EE_SCREENS_SEAX_SPEC_OPEN, info, 0, &[]) == 0 {
            let mut line = [0u8; 32];
            c(h, EE_SCREENS_SEAX_SPEC_READ, info, line.as_mut_ptr() as usize, &[0x20]);
            let read = c(h, EE_SCREENS_SEAX_READ_COUNT, 0, 0, &[]) as usize;
            let (width, height) = scan_two(&line[..read.min(line.len())]);
            let count = height.wrapping_mul(width) as usize;
            if count > 0 {
                let mut data = vec![0i16; count];
                let bytes = (count * 2) as u32;
                c(h, EE_SCREENS_SEAX_SPEC_READ, map, data.as_mut_ptr() as usize, &[bytes as i32]);
                let read = c(h, EE_SCREENS_SEAX_READ_COUNT, 0, 0, &[]) as u32;
                if read == bytes {
                    let mut boxes = [0i32; 4];
                    let ids = [
                        EE_SCREENS_SE_MAP_WATER_CUTOFF,
                        EE_SCREENS_SE_MAP_WATER_DEPTH,
                        EE_SCREENS_SE_MAP_LAND_HEIGHT,
                        EE_SCREENS_SE_MAP_LAND_CUTOFF,
                    ];
                    for (value, id) in boxes.iter_mut().zip(ids) {
                        *value = c(h, EE_SCREENS_SEAX_EDIT_INT, 0, 0, &[id]) as i32;
                    }
                    result = Some(SewElevation {
                        width,
                        height,
                        data,
                        water_cutoff: boxes[0],
                        water_depth: boxes[1],
                        land_height: boxes[2],
                        land_cutoff: boxes[3],
                    });
                }
            }
            c(h, EE_SCREENS_SEAX_SPEC_CLOSE, info, 0, &[]);
        }
        c(h, EE_SCREENS_SEAX_SPEC_CLOSE, map, 0, &[]);
    }
    c(h, EE_SCREENS_SEAX_SPEC_DELETE, info, 0, &[]);
    c(h, EE_SCREENS_SEAX_SPEC_DELETE, map, 0, &[]);
    result
}

// ---- the placements' host (`ee_screenscpp_sep`)

/// The `ee_screenscpp_sep` op answered by its statements, `None` for the C++'s own (and for the parts: [`parts`]).
pub fn sep<H: SeaxHost + ?Sized>(
    h: &mut H,
    op: u32,
    t: &mut GeTransformation,
    i: &mut [i32; 8],
    f: &mut [f32; 4],
) -> Option<i32> {
    match op {
        EE_SCREENS_SEP_GHOST_LOCK => ghost_lock(h, i[0]),
        EE_SCREENS_SEP_GHOST => return Some(ghost(h, t, i, f)),
        EE_SCREENS_SEP_TILE_HEIGHT => {
            let tile = c(h, EE_SCREENS_SEAX_TILE, 0, 0, &[i[0], i[1]]) as usize;
            if tile == 0 {
                return Some(0);
            }
            f[0] = cf(h, EE_SCREENS_SEAX_TILE_HEIGHT, tile, &[], &[]).1.f[0];
            return Some(1);
        }
        EE_SCREENS_SEP_BOARD => return Some(board(h, i)),
        EE_SCREENS_SEP_CREATE => create(h, t, i),
        EE_SCREENS_SEP_RELEASE => {
            let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[0]]) as usize;
            if unit != 0 {
                c(h, EE_SCREENS_SEAX_UNIT_DEREFERENCE, unit, 0, &[]);
            }
        }
        EE_SCREENS_SEP_UNIT => return Some(unit_flags(h, t, i[0])),
        EE_SCREENS_SEP_LOAD_INTO => {
            let container = c(h, EE_SCREENS_SEAX_SEP_COMPLEX, 0, 0, &[i[1]]) as usize;
            let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[0]]) as usize;
            c(h, EE_SCREENS_SEAX_COMPLEX_LOAD_COMPLEX, container, unit, &[]);
            select_container(h, container);
        }
        EE_SCREENS_SEP_MOVE_TO => {
            let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[0]]) as usize;
            let complex = c(h, EE_SCREENS_SEAX_UNIT_COMPLEX, unit, 0, &[]) as usize;
            if complex != 0 {
                c(h, EE_SCREENS_SEAX_COMPLEX_MOVE_TO, complex, 0, &[i[1], i[2]]);
            } else {
                c(h, EE_SCREENS_SEAX_UNIT_MOVE_TO, unit, 0, &[i[1], i[2]]);
            }
        }
        EE_SCREENS_SEP_AIRPORT => {
            let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[0]]) as usize;
            let airport = c(h, EE_SCREENS_SEAX_UNIT_COMPLEX, unit, 0, &[]) as usize;
            let level = c(h, EE_SCREENS_SEAX_FACING_QUARTER, airport, 0, &[]);
            if c(h, EE_SCREENS_SEAX_FACING, airport, 0, &[]) != level {
                c(h, EE_SCREENS_SEAX_SET_FACING, airport, 0, &[level as i32]);
            }
            let mut other = c(h, EE_SCREENS_SEAX_COMPLEX_NEXT, airport, 0, &[]) as usize;
            while other != 0 {
                let facing = c(h, EE_SCREENS_SEAX_FACING, airport, 0, &[]);
                if c(h, EE_SCREENS_SEAX_FACING, other, 0, &[]) != facing {
                    c(h, EE_SCREENS_SEAX_SET_FACING, other, 0, &[facing as i32]);
                }
                f[0] = cf(h, EE_SCREENS_SEAX_LEVEL_GROUND, other, &[], &[f[0]]).1.f[0];
                other = c(h, EE_SCREENS_SEAX_COMPLEX_NEXT, other, 0, &[]) as usize;
            }
        }
        _ => return None,
    }
    Some(0)
}

/// `EE_SCREENS_SEP_GHOST`.
fn ghost<H: SeaxHost + ?Sized>(h: &mut H, t: &mut GeTransformation, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
    let count = c(h, EE_SCREENS_SEAX_GHOST_COUNT, 0, 0, &[]);
    if i[0] < 0 || i[0] as isize >= count {
        return 0;
    }
    let ghost = c(h, EE_SCREENS_SEAX_GHOST, 0, 0, &[i[0]]) as usize;
    let unit_type = c(h, EE_SCREENS_SEAX_GHOST_TYPE, ghost, 0, &[]) as i32;
    let class = c(h, EE_SCREENS_SEAX_CLASS, 0, 0, &[unit_type]) as usize;
    let mut flags = 0;
    if class != 0 {
        flags |= EE_SCREENS_SEP_F_CLASS;
        if c(h, EE_SCREENS_SEAX_CLASS_SMALL, class, 0, &[]) != 0 {
            flags |= EE_SCREENS_SEP_F_SMALL;
        }
        if c(h, EE_SCREENS_SEAX_CLASS_FLIES, class, 0, &[]) != 0 {
            flags |= EE_SCREENS_SEP_F_FLIES;
            f[0] = cf(h, EE_SCREENS_SEAX_CLASS_CRUISE, class, &[], &[]).1.f[0];
        }
    }
    if unit_type == EE_SCREENS_SEAX_TYPE_AIRPORT {
        flags |= EE_SCREENS_SEP_F_AIRPORT;
    }
    if unit_type == EE_SCREENS_SEAX_TYPE_GRANARY_IMPROVED || unit_type == EE_SCREENS_SEAX_TYPE_GRANARY {
        flags |= EE_SCREENS_SEP_F_GRANARY;
    }
    i[1] = unit_type;
    i[2] = c(h, EE_SCREENS_SEAX_GHOST_TILE_X, ghost, 0, &[]) as i32;
    i[3] = c(h, EE_SCREENS_SEAX_GHOST_TILE_Y, ghost, 0, &[]) as i32;
    i[4] = c(h, EE_SCREENS_SEAX_GHOST_CLASS_TILE_SIZE, ghost, 0, &[]) as i32;
    i[5] = c(h, EE_SCREENS_SEAX_GHOST_ANIMATION, ghost, 0, &[]) as i32;
    i[6] = flags;
    ct(h, EE_SCREENS_SEAX_GHOST_TRANSFORMATION, ghost, t, &[]);
    c(h, EE_SCREENS_SEAX_GHOST_CAN_PLACE, ghost, 0, &[]) as i32
}

/// `EE_SCREENS_SEP_BOARD`.
fn board<H: SeaxHost + ?Sized>(h: &mut H, i: &mut [i32; 8]) -> i32 {
    let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[0]]) as usize;
    if unit == 0 {
        return 0;
    }
    let container = c(h, EE_SCREENS_SEAX_UNIT_COMPLEX, unit, 0, &[]) as usize;
    let class = c(h, EE_SCREENS_SEAX_CLASS, 0, 0, &[i[2]]) as usize;
    let player = if i[1] >= 0 {
        c(h, EE_SCREENS_SEAX_PLAYER, 0, 0, &[i[1]]) as usize
    } else {
        let other = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[i[3]]) as usize;
        c(h, EE_SCREENS_SEAX_UNIT_PLAYER, other, 0, &[]) as usize
    };
    if container == 0
        || c(h, EE_SCREENS_SEAX_COMPLEX_PLAYER_IS, container, player, &[]) == 0
        || c(h, EE_SCREENS_SEAX_UNIT_TRANSPORTS, unit, 0, &[]) == 0
        || c(h, EE_SCREENS_SEAX_COMPLEX_ROOM_FOR, container, class, &[]) == 0
        || c(h, EE_SCREENS_SEAX_CLASS_CAN_ENTER, class, container, &[]) == 0
    {
        return 0;
    }
    c(h, EE_SCREENS_SEAX_COMPLEX_REFERENCE, container, 0, &[]);
    i[4] = c(h, EE_SCREENS_SEAX_COMPLEX_ID, container, 0, &[]) as i32;
    1
}

/// The transport selected (unless it is) after a load, the unit tab's selection updated, the transport released.
fn select_container<H: SeaxHost + ?Sized>(h: &mut H, container: usize) {
    if c(h, EE_SCREENS_SEAX_COMPLEX_EDITOR_SELECTED, container, 0, &[]) == 0 {
        c(h, EE_SCREENS_SEAX_EDITOR_SELECT_COMPLEX, container, 0, &[]);
    }
    c(h, EE_SCREENS_SEAX_EDITOR_UPDATE_SELECTION, 0, 0, &[]);
    c(h, EE_SCREENS_SEAX_COMPLEX_DEREFERENCE, container, 0, &[]);
}

/// `EE_SCREENS_SEP_CREATE`.
fn create<H: SeaxHost + ?Sized>(h: &mut H, t: &mut GeTransformation, i: &mut [i32; 8]) {
    let class = c(h, EE_SCREENS_SEAX_CLASS, 0, 0, &[i[1]]) as usize;
    let player = c(h, EE_SCREENS_SEAX_PLAYER, 0, 0, &[i[0]]) as usize;
    // (a handle: none once gone)
    let container = if i[3] != -1 { c(h, EE_SCREENS_SEAX_SEP_COMPLEX, 0, 0, &[i[3]]) as usize } else { 0 };
    let unit_type = c(h, EE_SCREENS_SEAX_CLASS_TYPE, class, 0, &[]) as i32;
    let unit;
    if unit_type != EE_SCREENS_SEAX_TYPE_CAMERA_OBJECT {
        if container != 0 {
            t.m[0][3] = -10000.0;
            t.m[1][3] = -10000.0;
        }
        unit = ct(h, EE_SCREENS_SEAX_CREATE_AT, class, t, &[i[0]]) as usize;
    } else {
        unit = c(h, EE_SCREENS_SEAX_CREATE_AT_WORLD, class, 0, &[i[0]]) as usize;
        c(h, EE_SCREENS_SEAX_UNIT_CLOAKABLE, unit, 0, &[]);
        c(h, EE_SCREENS_SEAX_UNIT_CLOAKED, unit, 0, &[]);
        ct(h, EE_SCREENS_SEAX_CLOSEST_TILE, 0, t, &[]);
        c(h, EE_SCREENS_SEAX_UNIT_SET_TILE_X, unit, 0, &[]);
        c(h, EE_SCREENS_SEAX_UNIT_SET_TILE_Y, unit, 0, &[]);
        c(h, EE_SCREENS_SEAX_UNIT_UPDATE_TILE, unit, 0, &[]);
    }
    c(h, EE_SCREENS_SEAX_PLAYER_UNLOAD_MEDIA, player, 0, &[unit_type]);
    if c(h, EE_SCREENS_SEAX_UNIT_ON_TILE, unit, 0, &[]) != 0 {
        if c(h, EE_SCREENS_SEAX_CLASS_LAND_BUILDING, class, 0, &[]) != 0 {
            c(h, EE_SCREENS_SEAX_UNIT_SET_BUILT, unit, 0, &[]);
            c(h, EE_SCREENS_SEAX_UNIT_SET_ANIMATION, unit, 0, &[i[2]]);
        }
        if c(h, EE_SCREENS_SEAX_UNIT_ON_TILE, unit, 0, &[]) != 0
            && c(h, EE_SCREENS_SEAX_UNIT_POWERED, unit, 0, &[]) != 0
        {
            c(h, EE_SCREENS_SEAX_UNIT_SET_POWER, unit, 0, &[]);
        }
    }
    if container != 0 {
        // Port: the new unit is inserted first (its id given, as the physics' flush gives it): the transport's
        // cargo holds units by id since R5a.4, and a unit without one resolved to nothing (the next save crashed)
        c(h, EE_SCREENS_SEAX_PLAYER_ADD_PENDING, player, 0, &[]);
        c(h, EE_SCREENS_SEAX_COMPLEX_LOAD_UNIT, container, unit, &[]);
        select_container(h, container);
    }
}

/// `EE_SCREENS_SEP_UNIT`.
fn unit_flags<H: SeaxHost + ?Sized>(h: &mut H, t: &mut GeTransformation, id: i32) -> i32 {
    let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[id]) as usize;
    if unit == 0 {
        return 0;
    }
    let mut flags = EE_SCREENS_SEP_U_EXISTS;
    let class = c(h, EE_SCREENS_SEAX_UNIT_CLASS, unit, 0, &[]) as usize;
    if c(h, EE_SCREENS_SEAX_CLASS_FLIES, class, 0, &[]) != 0 {
        flags |= EE_SCREENS_SEP_U_FLIES;
    }
    if c(h, EE_SCREENS_SEAX_CLASS_PITCHES, class, 0, &[]) != 0 {
        flags |= EE_SCREENS_SEP_U_PITCHES;
    }
    let complex = c(h, EE_SCREENS_SEAX_UNIT_COMPLEX, unit, 0, &[]) as usize;
    if complex != 0 {
        flags |= EE_SCREENS_SEP_U_COMPLEX;
        ct(h, EE_SCREENS_SEAX_COMPLEX_LOCATION, complex, t, &[]);
        let complex_class = c(h, EE_SCREENS_SEAX_COMPLEX_CLASS, complex, 0, &[]) as usize;
        if c(h, EE_SCREENS_SEAX_CLASS_TYPE, complex_class, 0, &[]) as i32 == EE_SCREENS_SEAX_TYPE_AIRPORT {
            flags |= EE_SCREENS_SEP_U_AIRPORT;
        }
        if c(h, EE_SCREENS_SEAX_CLASS_LAND_BUILDING, complex_class, 0, &[]) != 0
            && c(h, EE_SCREENS_SEAX_CLASS_TYPE, complex_class, 0, &[]) as i32 != EE_SCREENS_SEAX_TYPE_CAMERA_OBJECT
        {
            flags |= EE_SCREENS_SEP_U_BUILDING;
        }
    }
    flags
}

/// `EE_SCREENS_SEP_PARTS`: the unit's parts' ids (each referenced) into `out` (at most `room` after a simple unit's
/// own) -> their count (0: no such unit).
pub fn parts<H: SeaxHost + ?Sized>(h: &mut H, id: i32, room: i32, out: &mut [i32]) -> i32 {
    if id as u32 == u32::MAX {
        return 0;
    }
    let unit = c(h, EE_SCREENS_SEAX_SEP_UNIT, 0, 0, &[id]) as usize;
    if unit == 0 {
        return 0;
    }
    let mut count = 0;
    let mut part = c(h, EE_SCREENS_SEAX_UNIT_COMPLEX, unit, 0, &[]) as usize;
    if part == 0 {
        c(h, EE_SCREENS_SEAX_UNIT_REFERENCE, unit, 0, &[]);
        out[count as usize] = c(h, EE_SCREENS_SEAX_UNIT_ID, unit, 0, &[]) as i32;
        count += 1;
    }
    while part != 0 && count < room {
        c(h, EE_SCREENS_SEAX_COMPLEX_REFERENCE, part, 0, &[]);
        out[count as usize] = c(h, EE_SCREENS_SEAX_COMPLEX_ID, part, 0, &[]) as i32;
        count += 1;
        part = c(h, EE_SCREENS_SEAX_COMPLEX_NEXT, part, 0, &[]) as usize;
    }
    count
}

// ---- the selection's host (`ee_screenscpp_seu`)

/// The `ee_screenscpp_seu` op answered by its statements, `None` for the C++'s own.
pub fn seu<H: SeaxHost + ?Sized>(h: &mut H, op: u32, unit: usize, a: isize, yaw: f32) -> Option<usize> {
    match op {
        EE_SCREENS_SEU_ROTATE => {
            c(h, EE_SCREENS_SEAX_T_FROM_LOCATION, unit, 0, &[]);
            cf(h, EE_SCREENS_SEAX_T_YPR, 0, &[], &[yaw]);
            c(h, EE_SCREENS_SEAX_T_APPLY, unit, 0, &[]);
        }
        EE_SCREENS_SEU_REMOVE_SIMPLE => remove(h, unit),
        EE_SCREENS_SEU_LEAVE_AIR_TRAFFIC => {
            if c(h, EE_SCREENS_SEAX_COMPLEX_ATC_AIRPORT, unit, 0, &[]) != 0 {
                c(h, EE_SCREENS_SEAX_ATC_REMOVE, unit, 0, &[]);
            }
        }
        EE_SCREENS_SEU_KILL_PART => {
            c(h, EE_SCREENS_SEAX_COMPLEX_KILL, unit, 0, &[]);
            c(h, EE_SCREENS_SEAX_PLAYER_KILL_COMPLEX, a as usize, unit, &[]);
        }
        EE_SCREENS_SEU_UNLOAD => {
            c(h, EE_SCREENS_SEAX_COMPLEX_UNLOAD, unit, a as usize, &[]);
        }
        _ => return None,
    }
    Some(0)
}

/// The ids of the units of `unit_type` picked in the screen rectangle (a `GERectangle*`; the original
/// `ee_screenscpp_seu_pick`).
pub fn pick<H: SeaxHost + ?Sized>(h: &mut H, rectangle: usize, unit_type: i32) -> Vec<u32> {
    c(h, EE_SCREENS_SEAX_IDS_CLEAR, 0, 0, &[]);
    c(h, EE_SCREENS_SEAX_PICK, rectangle, 0, &[unit_type]);
    let count = c(h, EE_SCREENS_SEAX_IDS_COUNT, 0, 0, &[]);
    (0..count).map(|k| c(h, EE_SCREENS_SEAX_ID, 0, 0, &[k as i32]) as u32).collect()
}

#[cfg(test)]
#[path = "editor_seax_tests.rs"]
mod tests;
