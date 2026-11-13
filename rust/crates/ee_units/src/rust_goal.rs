//! The goals whose class moved whole into Rust (RUST_PLAN R5b.4, rounds 16 and 17): one C++ class, `EEUGRust`
//! (ee/EEUGRust.h), keeps the base goal's fields and vtable; every virtual is one call of
//! [`ee_units_rust_goal`], which dispatches by the goal's kind to the class's bodies (`patrol`, `calamity`,
//! `explore`, `unload`, `load`), their state a Rust table by the goal's object. The C++ answers through one operation callback
//! ([`EeUnitsGoalHost`]): every read and world call the bodies make is one numbered operation
//! (`EE_UNITS_GOAL_HOST_*`) with two handles and an argument block ([`EeUnitsGoalIo`]).
//!
//! Units, groups and goals are C++ objects held by handle (their addresses; an `EEComplexUnit` is its `EEUnit` at
//! the same address). Text (the debug texts, the out-of-synch dump) is appended through the host as UTF-8.

use std::ffi::c_void;

use crate::build::{BuildHost, GOAL_BUILD_REPAIR};
use crate::calamity::CalamityHost;
use crate::explore::ExploreHost;
use crate::gather::{GatherHost, GOAL_GATHER};
use crate::load::{LoadClass, LoadHost, LoadUnit, GOAL_LOAD_GARRISON, GOAL_LOAD_TRANSPORT};
use crate::location::GOAL_LOCATION;
use crate::patrol::PatrolHost;
use crate::unload::UnloadHost;

/// What a unit or group field of a goal holds.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Held {
    None,
    /// a live object (referenced)
    Object(u64),
    /// a saved id read by `Load`, resolved by `RepairPointers`
    Saved(u32),
}

impl Held {
    /// the live object, 0 otherwise
    pub fn object(self) -> u64 {
        match self {
            Held::Object(o) => o,
            _ => 0,
        }
    }

    /// a live object or none
    pub fn of(object: u64) -> Held {
        if object == 0 {
            Held::None
        } else {
            Held::Object(object)
        }
    }

    /// the id `RepairPointers` reads back (`USavedID`): a saved id, -1 for none (a live object: not -1)
    pub fn saved_id(self) -> u32 {
        match self {
            Held::Saved(id) => id,
            Held::Object(_) => u32::MAX - 1,
            Held::None => 0,
        }
    }
}

/// The host calls every goal class makes (the FFI adapter; the tests' mocks).
pub trait GoalHost {
    /// `Reference()` / `Dereference()` of a patrol group (`group` true) or a unit
    fn reference(&mut self, object: u64, group: bool);
    fn dereference(&mut self, object: u64, group: bool);
    /// a unit's `mUnitID`, `mTileX`/`mTileY`, `mIsValid`, its command's type (0: none)
    fn unit_id(&mut self, unit: u64) -> u32;
    fn unit_tile(&mut self, unit: u64) -> (i64, i64);
    fn unit_valid(&mut self, unit: u64) -> bool;
    fn unit_command(&mut self, unit: u64) -> i64;
    /// the goal's `SetPathAttempts(0)`; the unit's `StartAction(a)` and its class's `ActionChanged`
    fn reset_path_attempts(&mut self, goal: u64);
    fn start_action(&mut self, unit: u64, action: i64);
    /// the world's unit of an id: its complex unit (`EEUnitCast<EEComplexUnit>`) or any unit (0: none)
    fn find_unit(&mut self, id: u32, complex: bool) -> u64;
    /// a unit's `GetPosition`; the map's `GetTileLocation(x, y)`
    fn unit_position(&mut self, unit: u64) -> [f32; 3];
    fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3];
    /// the unit's active group's `TellGroup(unit, point)` when it has one
    fn tell_group(&mut self, unit: u64, point: [f32; 3]);
    /// the unit's `SetCurrentPhysicsAction(a)`, `SetAnimation(a, -1, 0)`, `DropPath()`
    fn set_physics_action(&mut self, unit: u64, action: i64);
    fn set_animation(&mut self, unit: u64, animation: i64);
    fn drop_path(&mut self, unit: u64);
    /// a new `EEUGRust` of the kind (`new EEUGRust(kind, arg)`, its state not made yet)
    fn new_goal(&mut self, kind: u8, arg: bool) -> u64;
    /// the file's `GetIsOpen()` and its typed calls (`unsigned long`, `long`, `bool`); a read takes the value the
    /// field held and gives back what the file left in it
    fn file_open(&mut self) -> bool;
    fn write_u32(&mut self, value: u32) -> i32;
    fn write_i32(&mut self, value: i32) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn read_u32(&mut self, value: &mut u32) -> i32;
    fn read_i32(&mut self, value: &mut i32) -> i32;
    fn read_bool(&mut self, value: &mut bool) -> i32;
    /// text onto the debug text or the out-of-synch dump; a unit's class name (`mUnitClass->mName`)
    fn append(&mut self, text: &str);
    fn append_class_name(&mut self, unit: u64);
}

/// `kTriggerUnitFacingNotRequired`: the base goal's one trigger
pub const TRIGGER_UNIT_FACING_NOT_REQUIRED: i64 = 9014;

/// `EEUnitGoal::EvaluateCondition`
pub fn base_condition(event: i64) -> bool {
    event == TRIGGER_UNIT_FACING_NOT_REQUIRED
}

// ---- the C++ host: one operation callback
/// The argument block of one operation (ints, floats, text, a list sink, a list of handles).
#[repr(C)]
pub struct EeUnitsGoalIo {
    pub i: [i64; 8],
    pub f: [f32; 8],
    pub text: *const u8,
    pub len: usize,
    /// a `Vec<(i32, i32)>` the host pushes tiles onto (`ee_units_goal_push_tile`)
    pub sink: *mut c_void,
    /// handles (units) handed with the operation: the load goals' units
    pub list: *const u64,
    pub list_len: usize,
}

impl Default for EeUnitsGoalIo {
    fn default() -> Self {
        EeUnitsGoalIo {
            i: [0; 8],
            f: [0.0; 8],
            text: std::ptr::null(),
            len: 0,
            sink: std::ptr::null_mut(),
            list: std::ptr::null(),
            list_len: 0,
        }
    }
}

/// The host: `op(context, operation, a, b, io)` (`EE_UNITS_GOAL_HOST_*`).
#[repr(C)]
pub struct EeUnitsGoalHost {
    pub context: *mut c_void,
    pub op: unsafe extern "C" fn(*mut c_void, i32, u64, u64, *mut EeUnitsGoalIo) -> i64,
}

// the operations: the common ones
pub const EE_UNITS_GOAL_HOST_REFERENCE: i32 = 1;
pub const EE_UNITS_GOAL_HOST_DEREFERENCE: i32 = 2;
pub const EE_UNITS_GOAL_HOST_UNIT_ID: i32 = 3;
pub const EE_UNITS_GOAL_HOST_UNIT_TILE: i32 = 4;
pub const EE_UNITS_GOAL_HOST_UNIT_VALID: i32 = 5;
pub const EE_UNITS_GOAL_HOST_UNIT_COMMAND: i32 = 6;
pub const EE_UNITS_GOAL_HOST_RESET_PATH_ATTEMPTS: i32 = 7;
/// `i[0]`: the action, `i[1]` 1: without the class's `ActionChanged`
pub const EE_UNITS_GOAL_HOST_START_ACTION: i32 = 8;
pub const EE_UNITS_GOAL_HOST_FIND_UNIT: i32 = 9;
pub const EE_UNITS_GOAL_HOST_UNIT_POSITION: i32 = 10;
pub const EE_UNITS_GOAL_HOST_TILE_LOCATION: i32 = 11;
pub const EE_UNITS_GOAL_HOST_TELL_GROUP: i32 = 12;
pub const EE_UNITS_GOAL_HOST_SET_PHYSICS_ACTION: i32 = 13;
pub const EE_UNITS_GOAL_HOST_SET_ANIMATION: i32 = 14;
pub const EE_UNITS_GOAL_HOST_DROP_PATH: i32 = 15;
pub const EE_UNITS_GOAL_HOST_NEW_GOAL: i32 = 16;
pub const EE_UNITS_GOAL_HOST_FILE_OPEN: i32 = 17;
/// `i[0]`: the type (0 `unsigned long`, 1 `long`, 2 `bool`, 3 `float`: its bits), `i[1]` the value
pub const EE_UNITS_GOAL_HOST_WRITE: i32 = 18;
/// `i[0]`: the type, `i[1]` the value in and out
pub const EE_UNITS_GOAL_HOST_READ: i32 = 19;
pub const EE_UNITS_GOAL_HOST_APPEND: i32 = 20;
pub const EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME: i32 = 21;
// the patrol's
pub const EE_UNITS_GOAL_HOST_NEW_PATROL_GROUP: i32 = 30;
pub const EE_UNITS_GOAL_HOST_GROUP_NEXT: i32 = 31;
pub const EE_UNITS_GOAL_HOST_GROUP_PREVIOUS: i32 = 32;
pub const EE_UNITS_GOAL_HOST_GROUP_COUNT: i32 = 33;
pub const EE_UNITS_GOAL_HOST_GROUP_ID: i32 = 34;
pub const EE_UNITS_GOAL_HOST_GROUP_NEAREST: i32 = 35;
pub const EE_UNITS_GOAL_HOST_PLAYER_GROUP: i32 = 36;
pub const EE_UNITS_GOAL_HOST_GROUP_POINTS: i32 = 37;
pub const EE_UNITS_GOAL_HOST_GROUP_POINT: i32 = 38;
// the cast calamity's
pub const EE_UNITS_GOAL_HOST_CALAMITY_CLASS: i32 = 50;
pub const EE_UNITS_GOAL_HOST_CALAMITY_PLACE: i32 = 51;
pub const EE_UNITS_GOAL_HOST_CALAMITY_INFO: i32 = 52;
pub const EE_UNITS_GOAL_HOST_CREATE_CALAMITY: i32 = 53;
pub const EE_UNITS_GOAL_HOST_RESET_SAVED_GROUP: i32 = 54;
pub const EE_UNITS_GOAL_HOST_SPEND_POWER: i32 = 55;
pub const EE_UNITS_GOAL_HOST_CAN_CAST_AT: i32 = 56;
pub const EE_UNITS_GOAL_HOST_FACES_TARGET: i32 = 57;
pub const EE_UNITS_GOAL_HOST_FACES_POINT: i32 = 58;
pub const EE_UNITS_GOAL_HOST_UNIT_IN_SET: i32 = 59;
pub const EE_UNITS_GOAL_HOST_STATS_RANGE: i32 = 60;
pub const EE_UNITS_GOAL_HOST_CONTACT_RANGE: i32 = 61;
pub const EE_UNITS_GOAL_HOST_DISTANCE_TO_UNIT: i32 = 62;
pub const EE_UNITS_GOAL_HOST_UNIT_HEIGHT: i32 = 63;
pub const EE_UNITS_GOAL_HOST_HIT_POINTS: i32 = 64;
pub const EE_UNITS_GOAL_HOST_UNDETECTABLE: i32 = 65;
pub const EE_UNITS_GOAL_HOST_COMPLEX_UNIT: i32 = 66;
// the explore goal's
pub const EE_UNITS_GOAL_HOST_PLAYER_HUMAN: i32 = 70;
pub const EE_UNITS_GOAL_HOST_PLAYER_SEES_ALL: i32 = 71;
pub const EE_UNITS_GOAL_HOST_CONTAINER: i32 = 72;
pub const EE_UNITS_GOAL_HOST_GROUP_UNITS: i32 = 73;
pub const EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS: i32 = 74;
pub const EE_UNITS_GOAL_HOST_LINE_OF_SIGHT: i32 = 75;
pub const EE_UNITS_GOAL_HOST_RING_TILES: i32 = 76;
pub const EE_UNITS_GOAL_HOST_TILE_SCREEN: i32 = 77;
pub const EE_UNITS_GOAL_HOST_CELL_TAKEN: i32 = 78;
pub const EE_UNITS_GOAL_HOST_CAN_STAND: i32 = 79;
pub const EE_UNITS_GOAL_HOST_PATH_REACHABLE: i32 = 80;
pub const EE_UNITS_GOAL_HOST_PATH_AREA_ID: i32 = 81;
pub const EE_UNITS_GOAL_HOST_MAP_DIMENSIONS: i32 = 82;
pub const EE_UNITS_GOAL_HOST_CONTINENT_POINT: i32 = 83;
pub const EE_UNITS_GOAL_HOST_RANDOM: i32 = 84;
pub const EE_UNITS_GOAL_HOST_NOTICE_FOREIGN: i32 = 85;
pub const EE_UNITS_GOAL_HOST_UNIT_FAMILY: i32 = 86;
pub const EE_UNITS_GOAL_HOST_OWNS_LIGHTHOUSE: i32 = 87;
// the unload transport's
pub const EE_UNITS_GOAL_HOST_UNLOAD_RANGE: i32 = 90;
pub const EE_UNITS_GOAL_HOST_CLASS_TYPE: i32 = 91;
pub const EE_UNITS_GOAL_HOST_CARGO_COUNT: i32 = 92;
pub const EE_UNITS_GOAL_HOST_CARGO_CATEGORY: i32 = 93;
pub const EE_UNITS_GOAL_HOST_TRANSPORT_KIND: i32 = 94;
pub const EE_UNITS_GOAL_HOST_CAN_UNLOAD_AT: i32 = 95;
/// `i[0..8]`: the destination, unload, first cargo and found tiles in and out
pub const EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES: i32 = 96;
/// `i[0..6]`: the unload, first cargo and destination tiles in and out
pub const EE_UNITS_GOAL_HOST_CAN_UNLOAD_NEAR: i32 = 97;
pub const EE_UNITS_GOAL_HOST_SAME_PLAYER: i32 = 98;
pub const EE_UNITS_GOAL_HOST_FIRE_CARGO: i32 = 99;
pub const EE_UNITS_GOAL_HOST_UNLOAD_AT: i32 = 100;
/// `i[0]`: the space sound
pub const EE_UNITS_GOAL_HOST_UNLOAD_SOUND: i32 = 101;
// the load goals'
/// `i`: the garrison and transport capacities, weight, garrison technology, type, can garrison, tile size; `f`: the
/// load range, the inner collision sphere (zeros for no unit)
pub const EE_UNITS_GOAL_HOST_LOAD_CLASS: i32 = 110;
/// `i`: the garrison count, cargo room, hit points, disabled, no waypoints, animation (zeros for no unit)
pub const EE_UNITS_GOAL_HOST_LOAD_UNIT: i32 = 111;
pub const EE_UNITS_GOAL_HOST_UNIT_LOCATION: i32 = 112;
/// `i[0..4]`: the two tiles
pub const EE_UNITS_GOAL_HOST_LINE_BLOCKED: i32 = 113;
pub const EE_UNITS_GOAL_HOST_CAN_LOAD: i32 = 114;
/// `f[0..3]`: the location, `f[3..6]` the result in and out
pub const EE_UNITS_GOAL_HOST_BOARDING: i32 = 115;
/// `f[0..3]`: the result in and out
pub const EE_UNITS_GOAL_HOST_PICK_TILE_NEAR: i32 = 116;
/// `list`: the units, `i[4]` the base class's rule; `i[0..4]` the tiles in and out
pub const EE_UNITS_GOAL_HOST_RENDEZVOUS: i32 = 117;
pub const EE_UNITS_GOAL_HOST_CENTER_OFFSET: i32 = 118;
/// `i[0]`: the technology; out: the node, the class, available
pub const EE_UNITS_GOAL_HOST_GARRISON_UPGRADE: i32 = 119;
/// `b` 1: `GetGoal()`, 0: `GetLoadGoal()`; `i[0]` its kind
pub const EE_UNITS_GOAL_HOST_UNIT_GOAL: i32 = 120;
pub const EE_UNITS_GOAL_HOST_QUEUED_GOAL_KIND: i32 = 121;
pub const EE_UNITS_GOAL_HOST_CLEAR_GOALS: i32 = 122;
pub const EE_UNITS_GOAL_HOST_DEPOSIT: i32 = 123;
pub const EE_UNITS_GOAL_HOST_GARRISON_UNIT: i32 = 124;
pub const EE_UNITS_GOAL_HOST_LOAD_CARGO: i32 = 125;
pub const EE_UNITS_GOAL_HOST_HAS_CARGO: i32 = 126;
/// `i[0]`: the sound
pub const EE_UNITS_GOAL_HOST_PLAY_SOUND: i32 = 127;
pub const EE_UNITS_GOAL_HOST_REPLACE_CLASS: i32 = 128;
pub const EE_UNITS_GOAL_HOST_GENERATE: i32 = 129;
pub const EE_UNITS_GOAL_HOST_FIRES_OVER_WALLS: i32 = 130;
// the build/repair goal's (from 400: see the body operations below)
/// `a`: the unit, `sink`: an `EeUnitsBuildUnit` it fills (zeros for no unit; `b` 1: only the class, player, tile and
/// complex unit)
pub const EE_UNITS_GOAL_HOST_BUILD_UNIT: i32 = 400;
pub const EE_UNITS_GOAL_HOST_TECH_NODE: i32 = 401;
pub const EE_UNITS_GOAL_HOST_NODE_INFO: i32 = 402;
pub const EE_UNITS_GOAL_HOST_PLAYER_ID: i32 = 403;
pub const EE_UNITS_GOAL_HOST_CLASS_UNIT_TYPE: i32 = 404;
pub const EE_UNITS_GOAL_HOST_BUILDERS_ADD: i32 = 405;
pub const EE_UNITS_GOAL_HOST_ATTACK_PART: i32 = 406;
pub const EE_UNITS_GOAL_HOST_IN_WORK_REACH: i32 = 407;
pub const EE_UNITS_GOAL_HOST_SURROUNDING_TILES: i32 = 408;
pub const EE_UNITS_GOAL_HOST_COMPLETE_BUILDING: i32 = 409;
pub const EE_UNITS_GOAL_HOST_PLAY_EFFECT: i32 = 410;
pub const EE_UNITS_GOAL_HOST_SET_PRODUCTION_TIME: i32 = 411;
pub const EE_UNITS_GOAL_HOST_FILL_PARTS: i32 = 412;
pub const EE_UNITS_GOAL_HOST_NEXT_FREE_TILE: i32 = 413;
pub const EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION: i32 = 414;
pub const EE_UNITS_GOAL_HOST_LOCATION_GOAL: i32 = 415;
pub const EE_UNITS_GOAL_HOST_HELP_BUILD: i32 = 416;
pub const EE_UNITS_GOAL_HOST_TEND_FARM: i32 = 417;
pub const EE_UNITS_GOAL_HOST_GATHER_NEAR: i32 = 418;
pub const EE_UNITS_GOAL_HOST_GATE_SIDE_PART: i32 = 419;
pub const EE_UNITS_GOAL_HOST_BUILD_EVENT: i32 = 420;
pub const EE_UNITS_GOAL_HOST_ABANDON_BUILDING: i32 = 421;
pub const EE_UNITS_GOAL_HOST_FACE_TARGET: i32 = 422;
pub const EE_UNITS_GOAL_HOST_SET_WORK_TIMER: i32 = 423;
pub const EE_UNITS_GOAL_HOST_HALT: i32 = 424;
pub const EE_UNITS_GOAL_HOST_SITE_TAKEN: i32 = 425;
pub const EE_UNITS_GOAL_HOST_FOOTPRINT_CLEAR: i32 = 426;
pub const EE_UNITS_GOAL_HOST_SITE_FREE: i32 = 427;
pub const EE_UNITS_GOAL_HOST_OTHER_GOAL_QUEUED: i32 = 428;
pub const EE_UNITS_GOAL_HOST_RESOURCE_USERS: i32 = 429;
pub const EE_UNITS_GOAL_HOST_NEW_GATHER: i32 = 430;
pub const EE_UNITS_GOAL_HOST_SET_GOAL: i32 = 431;
pub const EE_UNITS_GOAL_HOST_NEAREST_SITE: i32 = 432;
pub const EE_UNITS_GOAL_HOST_CLEAR_SITE: i32 = 433;
pub const EE_UNITS_GOAL_HOST_CAN_PRODUCE: i32 = 434;
pub const EE_UNITS_GOAL_HOST_CAN_AFFORD_REPAIR: i32 = 435;
pub const EE_UNITS_GOAL_HOST_FRIENDLY: i32 = 436;
pub const EE_UNITS_GOAL_HOST_IN_CINEMATIC: i32 = 437;
pub const EE_UNITS_GOAL_HOST_SET_PAID: i32 = 438;
pub const EE_UNITS_GOAL_HOST_PAY: i32 = 439;
pub const EE_UNITS_GOAL_HOST_SET_HIT_POINTS: i32 = 440;
pub const EE_UNITS_GOAL_HOST_FOUNDATION_STARTED: i32 = 441;
pub const EE_UNITS_GOAL_HOST_GOAL_ARG: i32 = 442;
// the gather goal's
pub const EE_UNITS_GOAL_HOST_FARM_TILE: i32 = 450;
pub const EE_UNITS_GOAL_HOST_RESOURCE_SPOT: i32 = 451;
pub const EE_UNITS_GOAL_HOST_NEXT_RESOURCE: i32 = 452;
pub const EE_UNITS_GOAL_HOST_FIND_STORAGE: i32 = 453;
pub const EE_UNITS_GOAL_HOST_FREE_TILE: i32 = 454;
pub const EE_UNITS_GOAL_HOST_TILE_HEIGHT: i32 = 455;
pub const EE_UNITS_GOAL_HOST_AT_RESOURCE_SPOT: i32 = 456;
pub const EE_UNITS_GOAL_HOST_NEXT_FARM: i32 = 457;
pub const EE_UNITS_GOAL_HOST_STORAGE_SEARCH: i32 = 458;
pub const EE_UNITS_GOAL_HOST_GATHER_SPOT: i32 = 459;
pub const EE_UNITS_GOAL_HOST_BEGIN_GATHERING: i32 = 460;
pub const EE_UNITS_GOAL_HOST_DEPOSIT_LOAD: i32 = 461;
pub const EE_UNITS_GOAL_HOST_NEXT_TREE: i32 = 462;
pub const EE_UNITS_GOAL_HOST_BUILD_FARMS: i32 = 463;
pub const EE_UNITS_GOAL_HOST_HUNT: i32 = 464;
pub const EE_UNITS_GOAL_HOST_SET_GATHERED: i32 = 465;
pub const EE_UNITS_GOAL_HOST_GATHER_ROOM: i32 = 466;
pub const EE_UNITS_GOAL_HOST_LAST_WAYPOINT: i32 = 467;
pub const EE_UNITS_GOAL_HOST_RESOURCE_REACH: i32 = 468;
pub const EE_UNITS_GOAL_HOST_STORAGE_REACH: i32 = 469;
pub const EE_UNITS_GOAL_HOST_SET_ACTIVE: i32 = 470;
pub const EE_UNITS_GOAL_HOST_FIND_GHOST: i32 = 471;
pub const EE_UNITS_GOAL_HOST_AT_RESOURCE: i32 = 472;
pub const EE_UNITS_GOAL_HOST_AT_STORAGE: i32 = 473;
pub const EE_UNITS_GOAL_HOST_LINE_TO_UNIT_BLOCKED: i32 = 474;
pub const EE_UNITS_GOAL_HOST_ACCESSIBLE: i32 = 475;
pub const EE_UNITS_GOAL_HOST_PATH_LENGTH: i32 = 476;
pub const EE_UNITS_GOAL_HOST_GATHER_WORK: i32 = 477;
pub const EE_UNITS_GOAL_HOST_NAVAL_SEARCH: i32 = 478;
pub const EE_UNITS_GOAL_HOST_RESET_ACTIVE_PATH: i32 = 479;
// the location goal's (from 490)
/// `a`: the unit; its class's `mCategory`
pub const EE_UNITS_GOAL_HOST_UNIT_CATEGORY: i32 = 490;
/// `a`: the unit, `sink`: a `GETransformation` it fills (`mPhysicalModel.GetLocation()`)
pub const EE_UNITS_GOAL_HOST_UNIT_TRANSFORMATION: i32 = 491;
/// `a`: the unit, `sink`: the `GETransformation` it is given (`SetTransformation(t, true)`)
pub const EE_UNITS_GOAL_HOST_SET_TRANSFORMATION: i32 = 492;
/// `a`: the unit, `i[0]`: `SetTargetSearchTimer`'s value
pub const EE_UNITS_GOAL_HOST_SET_TARGET_SEARCH_TIMER: i32 = 493;
/// `a`: the unit; `mGoalQueues[0].mGoals.size()`
pub const EE_UNITS_GOAL_HOST_GOAL_QUEUE_LEN: i32 = 494;

/// A unit's fields the build/repair goal reads (`EE_UNITS_GOAL_HOST_BUILD_UNIT`; the complex unit's own only for
/// a complex unit).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsBuildUnit {
    /// `mComplexUnit`, `mUnitClass`, `mPlayer`; the first of the complex unit's linked parts (`mPreviousPart`
    /// walked), its `mNextPart`
    pub complex: u64,
    pub unit_class: u64,
    pub player: u64,
    pub first: u64,
    pub next: u64,
    /// the class's type, tile size, category; `gUnitClassTable[type]`'s technology (`kTechNone` without one)
    pub unit_type: i32,
    pub tile_size: i32,
    pub category: i32,
    pub table_technology: i32,
    /// `mHitPoints`, `mStats->mMaxHitPoints`, `mNumBuilders`, the tile, the command's type (0: none)
    pub hit_points: i32,
    pub max_hit_points: i32,
    pub builders: i32,
    pub tile_x: i32,
    pub tile_y: i32,
    pub command: i32,
    /// `mStats->mRepairRate`, `mStats->mWorkRate`, `mStats->mBuildTimeScale`, `GetLineOfSight()`, the physical
    /// model's location
    pub repair_rate: i32,
    pub work_rate: i32,
    pub build_time_scale: f32,
    pub line_of_sight: f32,
    pub location: [f32; 3],
    /// `mIsBuilt`, `mTileNode`, `mTile`, `mIsValid`, `mIsPaidFor`, `IsFoundation()`
    pub built: bool,
    pub tile_node: bool,
    pub tile: bool,
    pub valid: bool,
    pub paid_for: bool,
    pub foundation: bool,
    /// the class's `mIsWall`, `mIsTower`, `IsDropSite()`
    pub wall: bool,
    pub tower: bool,
    pub drop_site: bool,
    /// the player's `mInstantBuild`, `IsHuman()`, `mComputerPlayer != NULL`; the unit's `mHasShiftQueuedBuild`
    pub instant_build: bool,
    pub human: bool,
    pub computer: bool,
    pub shift_queued_build: bool,
    /// the gather goal's: `mResourceType`, `mResourceAmount`, `mStats->mCarryAmount`, the class's `mFamily` and
    /// `mIsTree`, the complex unit's `mStats->mMaxVelocity > 0`, `mIsDisabled`
    pub resource_type: i32,
    pub resource_amount: i32,
    pub carry_amount: i32,
    pub family: i32,
    pub is_tree: bool,
    pub moving: bool,
    pub disabled: bool,
}

/// The FFI adapter over the callback.
pub struct OpHost<'a>(pub &'a EeUnitsGoalHost);

impl OpHost<'_> {
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
        // SAFETY: the table's contract: `context` and `op` are live for the FFI call that handed out the table
        unsafe { (self.0.op)(self.0.context, op, a, b, io) }
    }
    fn op(&mut self, op: i32, a: u64, b: u64) -> i64 {
        let mut io = EeUnitsGoalIo::default();
        self.call(op, a, b, &mut io)
    }
    fn op_ints(&mut self, op: i32, a: u64, b: u64, ints: &[i64]) -> i64 {
        let mut io = EeUnitsGoalIo::default();
        io.i[..ints.len()].copy_from_slice(ints);
        self.call(op, a, b, &mut io)
    }
    fn op_point(&mut self, op: i32, a: u64, b: u64, ints: &[i64]) -> [f32; 3] {
        let mut io = EeUnitsGoalIo::default();
        io.i[..ints.len()].copy_from_slice(ints);
        self.call(op, a, b, &mut io);
        [io.f[0], io.f[1], io.f[2]]
    }
    fn op_with_point(&mut self, op: i32, a: u64, b: u64, p: [f32; 3]) -> i64 {
        let mut io = EeUnitsGoalIo::default();
        io.f[..3].copy_from_slice(&p);
        self.call(op, a, b, &mut io)
    }
    fn write(&mut self, kind: i64, value: i64) -> i32 {
        self.op_ints(EE_UNITS_GOAL_HOST_WRITE, 0, 0, &[kind, value]) as i32
    }
    fn read(&mut self, kind: i64, value: i64) -> (i32, i64) {
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = kind;
        io.i[1] = value;
        let result = self.call(EE_UNITS_GOAL_HOST_READ, 0, 0, &mut io) as i32;
        (result, io.i[1])
    }
}

impl GoalHost for OpHost<'_> {
    fn reference(&mut self, object: u64, group: bool) {
        self.op(EE_UNITS_GOAL_HOST_REFERENCE, object, u64::from(group));
    }
    fn dereference(&mut self, object: u64, group: bool) {
        self.op(EE_UNITS_GOAL_HOST_DEREFERENCE, object, u64::from(group));
    }
    fn unit_id(&mut self, unit: u64) -> u32 {
        self.op(EE_UNITS_GOAL_HOST_UNIT_ID, unit, 0) as u32
    }
    fn unit_tile(&mut self, unit: u64) -> (i64, i64) {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_UNIT_TILE, unit, 0, &mut io);
        (io.i[0], io.i[1])
    }
    fn unit_valid(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_UNIT_VALID, unit, 0) != 0
    }
    fn unit_command(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_UNIT_COMMAND, unit, 0)
    }
    fn reset_path_attempts(&mut self, goal: u64) {
        self.op(EE_UNITS_GOAL_HOST_RESET_PATH_ATTEMPTS, goal, 0);
    }
    fn start_action(&mut self, unit: u64, action: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_START_ACTION, unit, 0, &[action]);
    }
    fn find_unit(&mut self, id: u32, complex: bool) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_FIND_UNIT, u64::from(id), u64::from(complex)) as u64
    }
    fn unit_position(&mut self, unit: u64) -> [f32; 3] {
        self.op_point(EE_UNITS_GOAL_HOST_UNIT_POSITION, unit, 0, &[])
    }
    fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
        self.op_point(EE_UNITS_GOAL_HOST_TILE_LOCATION, 0, 0, &[x, y])
    }
    fn tell_group(&mut self, unit: u64, point: [f32; 3]) {
        self.op_with_point(EE_UNITS_GOAL_HOST_TELL_GROUP, unit, 0, point);
    }
    fn set_physics_action(&mut self, unit: u64, action: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_PHYSICS_ACTION, unit, 0, &[action]);
    }
    fn set_animation(&mut self, unit: u64, animation: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_ANIMATION, unit, 0, &[animation]);
    }
    fn drop_path(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_DROP_PATH, unit, 0);
    }
    fn new_goal(&mut self, kind: u8, arg: bool) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_NEW_GOAL, u64::from(kind), u64::from(arg)) as u64
    }
    fn file_open(&mut self) -> bool {
        self.op(EE_UNITS_GOAL_HOST_FILE_OPEN, 0, 0) != 0
    }
    fn write_u32(&mut self, value: u32) -> i32 {
        self.write(0, i64::from(value))
    }
    fn write_i32(&mut self, value: i32) -> i32 {
        self.write(1, i64::from(value))
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        self.write(2, i64::from(value))
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        let (result, v) = self.read(0, i64::from(*value));
        *value = v as u32;
        result
    }
    fn read_i32(&mut self, value: &mut i32) -> i32 {
        let (result, v) = self.read(1, i64::from(*value));
        *value = v as i32;
        result
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        let (result, v) = self.read(2, i64::from(*value));
        *value = v != 0;
        result
    }
    fn append(&mut self, text: &str) {
        let mut io = EeUnitsGoalIo { text: text.as_ptr(), len: text.len(), ..Default::default() };
        self.call(EE_UNITS_GOAL_HOST_APPEND, 0, 0, &mut io);
    }
    fn append_class_name(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME, unit, 0);
    }
}

impl PatrolHost for OpHost<'_> {
    fn new_group(&mut self) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_NEW_PATROL_GROUP, 0, 0) as u64
    }
    fn group_next(&mut self, group: u64, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_NEXT, group, unit) as u64
    }
    fn group_previous(&mut self, group: u64, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_PREVIOUS, group, unit) as u64
    }
    fn group_count(&mut self, group: u64) -> u32 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_COUNT, group, 0) as u32
    }
    fn group_id(&mut self, group: u64) -> u32 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_ID, group, 0) as u32
    }
    fn group_nearest(&mut self, group: u64, x: i64, y: i64) -> u64 {
        self.op_ints(EE_UNITS_GOAL_HOST_GROUP_NEAREST, group, 0, &[x, y]) as u64
    }
    fn player_group(&mut self, unit: u64, id: u32) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_PLAYER_GROUP, unit, u64::from(id)) as u64
    }
    fn group_points(&mut self, group: u64) -> u32 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_POINTS, group, 0) as u32
    }
    fn group_point(&mut self, group: u64, index: u32) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_POINT, group, u64::from(index)) as u64
    }
}

impl CalamityHost for OpHost<'_> {
    fn calamity_class(&mut self, unit: u64, technology: i32) -> (u64, i64, i64) {
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = i64::from(technology);
        let class = self.call(EE_UNITS_GOAL_HOST_CALAMITY_CLASS, unit, 0, &mut io) as u64;
        (class, io.i[1], io.i[2])
    }
    fn calamity_place(&mut self, class: u64, unit: u64, x: i32, y: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_CALAMITY_PLACE, class, unit, &[i64::from(x), i64::from(y)]);
    }
    fn calamity_info(&mut self, class: u64) -> (i64, i32, i32) {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_CALAMITY_INFO, class, 0, &mut io);
        (io.i[0], io.i[1] as i32, io.i[2] as i32)
    }
    fn create_calamity(
        &mut self,
        kind: i64,
        calamity: i64,
        source: u64,
        target: u64,
        tile: (i32, i32),
        unit: u64,
    ) -> bool {
        self.op_ints(
            EE_UNITS_GOAL_HOST_CREATE_CALAMITY,
            source,
            target,
            &[kind, calamity, i64::from(tile.0), i64::from(tile.1), unit as i64],
        ) != 0
    }
    fn reset_saved_group(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_RESET_SAVED_GROUP, unit, 0);
    }
    fn spend_power(&mut self, unit: u64, cost: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_SPEND_POWER, unit, 0, &[i64::from(cost)]);
    }
    fn can_cast_at(&mut self, class: u64, unit: u64, x: i32, y: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_CAN_CAST_AT, class, unit, &[i64::from(x), i64::from(y)]) != 0
    }
    fn faces_target(&mut self, unit: u64, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_FACES_TARGET, unit, target) != 0
    }
    fn faces_point(&mut self, unit: u64, point: [f32; 3]) -> bool {
        self.op_with_point(EE_UNITS_GOAL_HOST_FACES_POINT, unit, 0, point) != 0
    }
    fn unit_in_set(&mut self, unit: u64, set: i64) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_UNIT_IN_SET, unit, 0, &[set]) != 0
    }
    fn stats_range(&mut self, unit: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_STATS_RANGE, unit, 0, &[])[0]
    }
    fn contact_range(&mut self, unit: u64, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_CONTACT_RANGE, unit, target) != 0
    }
    fn distance_to_unit(&mut self, unit: u64, target: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_DISTANCE_TO_UNIT, unit, target, &[])[0]
    }
    fn unit_height(&mut self, unit: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_UNIT_HEIGHT, unit, 0, &[])[0]
    }
    fn hit_points(&mut self, unit: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_HIT_POINTS, unit, 0) as i32
    }
    fn undetectable(&mut self, unit: u64, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_UNDETECTABLE, unit, target) != 0
    }
    fn complex_unit(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_COMPLEX_UNIT, unit, 0) as u64
    }
}

impl ExploreHost for OpHost<'_> {
    fn player_human(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_PLAYER_HUMAN, unit, 0) != 0
    }
    fn player_sees_all(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_PLAYER_SEES_ALL, unit, 0) != 0
    }
    fn container(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_CONTAINER, unit, 0) as u64
    }
    fn group_units(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_GROUP_UNITS, unit, 0)
    }
    fn path_area_bounds(&mut self, unit: u64) -> Option<[i32; 4]> {
        let mut io = EeUnitsGoalIo::default();
        if self.call(EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS, unit, 0, &mut io) == 0 {
            return None;
        }
        Some([io.i[0] as i32, io.i[1] as i32, io.i[2] as i32, io.i[3] as i32])
    }
    fn line_of_sight(&mut self, unit: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_LINE_OF_SIGHT, unit, 0, &[])[0]
    }
    fn ring_tiles(&mut self, x: i32, y: i32, ring: i32) -> Vec<(i32, i32)> {
        let mut tiles: Vec<(i32, i32)> = Vec::new();
        let mut io = EeUnitsGoalIo { sink: (&mut tiles as *mut Vec<(i32, i32)>).cast(), ..Default::default() };
        io.i[0] = i64::from(x);
        io.i[1] = i64::from(y);
        io.i[2] = i64::from(ring);
        self.call(EE_UNITS_GOAL_HOST_RING_TILES, 0, 0, &mut io);
        tiles
    }
    fn tile_screen(&mut self, unit: u64, x: i32, y: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_TILE_SCREEN, unit, 0, &[i64::from(x), i64::from(y)]) != 0
    }
    fn cell_taken(&mut self, unit: u64, x: i32, y: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_CELL_TAKEN, unit, 0, &[i64::from(x), i64::from(y)]) != 0
    }
    fn can_stand(&mut self, unit: u64, x: i32, y: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_CAN_STAND, unit, 0, &[i64::from(x), i64::from(y)]) != 0
    }
    fn path_reachable(&mut self, unit: u64, from: (i32, i32), to: (i32, i32)) -> bool {
        let ints = [i64::from(from.0), i64::from(from.1), i64::from(to.0), i64::from(to.1)];
        self.op_ints(EE_UNITS_GOAL_HOST_PATH_REACHABLE, unit, 0, &ints) != 0
    }
    fn path_area_id(&mut self, x: i32, y: i32) -> u16 {
        self.op_ints(EE_UNITS_GOAL_HOST_PATH_AREA_ID, 0, 0, &[i64::from(x), i64::from(y)]) as u16
    }
    fn map_dimensions(&mut self) -> [i32; 4] {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_MAP_DIMENSIONS, 0, 0, &mut io);
        [io.i[0] as i32, io.i[1] as i32, io.i[2] as i32, io.i[3] as i32]
    }
    fn continent_point(&mut self, unit: u64) -> Option<(i32, i32)> {
        let mut io = EeUnitsGoalIo::default();
        if self.call(EE_UNITS_GOAL_HOST_CONTINENT_POINT, unit, 0, &mut io) == 0 {
            return None;
        }
        Some((io.i[0] as i32, io.i[1] as i32))
    }
    fn random(&mut self, low: i32, high: i32) -> i32 {
        self.op_ints(EE_UNITS_GOAL_HOST_RANDOM, 0, 0, &[i64::from(low), i64::from(high)]) as i32
    }
    fn notice_foreign(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_NOTICE_FOREIGN, unit, 0);
    }
    fn unit_family(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_UNIT_FAMILY, unit, 0)
    }
    fn owns_lighthouse(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_OWNS_LIGHTHOUSE, unit, 0) != 0
    }
}

/// The host pushes one tile onto a list sink (`io.sink` of the ring tiles' operation).
///
/// # Safety
/// `sink` is the `io.sink` of the operation in progress.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_push_tile(sink: *mut c_void, x: i32, y: i32) {
    // SAFETY: the caller's contract: the sink is the Vec the operation made
    let tiles = unsafe { &mut *sink.cast::<Vec<(i32, i32)>>() };
    tiles.push((x, y));
}

fn tiles_in(io: &mut EeUnitsGoalIo, tiles: &[(i32, i32)]) {
    for (k, t) in tiles.iter().enumerate() {
        io.i[2 * k] = i64::from(t.0);
        io.i[2 * k + 1] = i64::from(t.1);
    }
}

fn tiles_out(io: &EeUnitsGoalIo, tiles: &mut [(i32, i32)]) {
    for (k, t) in tiles.iter_mut().enumerate() {
        *t = (io.i[2 * k] as i32, io.i[2 * k + 1] as i32);
    }
}

impl UnloadHost for OpHost<'_> {
    fn unload_range(&mut self, unit: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_UNLOAD_RANGE, unit, 0, &[])[0]
    }
    fn class_type(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_CLASS_TYPE, unit, 0)
    }
    fn cargo_count(&mut self, unit: u64) -> u32 {
        self.op(EE_UNITS_GOAL_HOST_CARGO_COUNT, unit, 0) as u32
    }
    fn cargo_category(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_CARGO_CATEGORY, unit, 0)
    }
    fn transport_kind(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_TRANSPORT_KIND, unit, 0)
    }
    fn can_unload_at(&mut self, unit: u64, tile: (i32, i32)) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_CAN_UNLOAD_AT, unit, 0, &[i64::from(tile.0), i64::from(tile.1)]) != 0
    }
    fn find_unload_tiles(&mut self, unit: u64, tiles: &mut [(i32, i32); 4]) {
        let mut io = EeUnitsGoalIo::default();
        tiles_in(&mut io, tiles);
        self.call(EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES, unit, 0, &mut io);
        tiles_out(&io, tiles);
    }
    fn can_unload_near(&mut self, unit: u64, tiles: &mut [(i32, i32); 3]) -> bool {
        let mut io = EeUnitsGoalIo::default();
        tiles_in(&mut io, tiles);
        let near = self.call(EE_UNITS_GOAL_HOST_CAN_UNLOAD_NEAR, unit, 0, &mut io) != 0;
        tiles_out(&io, tiles);
        near
    }
    fn human(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_PLAYER_HUMAN, unit, 0) != 0
    }
    fn same_player(&mut self, unit: u64, other: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_SAME_PLAYER, unit, other) != 0
    }
    fn hit_points(&mut self, unit: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_HIT_POINTS, unit, 0) as i32
    }
    fn start_action_only(&mut self, unit: u64, action: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_START_ACTION, unit, 0, &[action, 1]);
    }
    fn fire_cargo(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_FIRE_CARGO, unit, 0);
    }
    fn unload_at(&mut self, unit: u64, tile: (i32, i32)) {
        self.op_ints(EE_UNITS_GOAL_HOST_UNLOAD_AT, unit, 0, &[i64::from(tile.0), i64::from(tile.1)]);
    }
    fn unload_sound(&mut self, unit: u64, space: bool) {
        self.op_ints(EE_UNITS_GOAL_HOST_UNLOAD_SOUND, unit, 0, &[i64::from(space)]);
    }
    fn complex_unit(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_COMPLEX_UNIT, unit, 0) as u64
    }
}

impl LoadHost for OpHost<'_> {
    fn load_class(&mut self, unit: u64) -> LoadClass {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_LOAD_CLASS, unit, 0, &mut io);
        LoadClass {
            garrison_capacity: io.i[0] as i32,
            transport_capacity: io.i[1] as i32,
            weight: io.i[2] as i32,
            garrison_technology: io.i[3] as i32,
            unit_type: io.i[4],
            can_garrison: io.i[5] != 0,
            tile_size: io.i[6] as i32,
            load_range: io.f[0],
            inner_sphere: io.f[1],
        }
    }
    fn load_unit(&mut self, unit: u64) -> LoadUnit {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_LOAD_UNIT, unit, 0, &mut io);
        LoadUnit {
            garrison_count: io.i[0] as i32,
            cargo_room: io.i[1] as i32,
            hit_points: io.i[2] as i32,
            disabled: io.i[3] != 0,
            no_waypoints: io.i[4] != 0,
            animation: io.i[5],
        }
    }
    fn unit_location(&mut self, unit: u64) -> [f32; 3] {
        self.op_point(EE_UNITS_GOAL_HOST_UNIT_LOCATION, unit, 0, &[])
    }
    fn line_blocked(&mut self, from: (i32, i32), to: (i32, i32)) -> bool {
        let ints = [i64::from(from.0), i64::from(from.1), i64::from(to.0), i64::from(to.1)];
        self.op_ints(EE_UNITS_GOAL_HOST_LINE_BLOCKED, 0, 0, &ints) != 0
    }
    fn transport_kind(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_TRANSPORT_KIND, unit, 0)
    }
    fn can_load(&mut self, container: u64, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_CAN_LOAD, container, unit) != 0
    }
    fn boarding_location(&mut self, unit: u64, container: u64, location: [f32; 3], out: &mut [f32; 3]) {
        let mut io = EeUnitsGoalIo::default();
        io.f[..3].copy_from_slice(&location);
        io.f[3..6].copy_from_slice(out);
        self.call(EE_UNITS_GOAL_HOST_BOARDING, unit, container, &mut io);
        out.copy_from_slice(&io.f[3..6]);
    }
    fn pick_tile_near(&mut self, unit: u64, container: u64, out: &mut [f32; 3]) -> bool {
        let mut io = EeUnitsGoalIo::default();
        io.f[..3].copy_from_slice(out);
        let found = self.call(EE_UNITS_GOAL_HOST_PICK_TILE_NEAR, unit, container, &mut io) != 0;
        out.copy_from_slice(&io.f[..3]);
        found
    }
    fn rendezvous_tiles(&mut self, container: u64, units: &[u64], base: bool, tiles: &mut [(i32, i32); 2]) {
        let mut io = EeUnitsGoalIo { list: units.as_ptr(), list_len: units.len(), ..Default::default() };
        tiles_in(&mut io, tiles);
        io.i[4] = i64::from(base);
        self.call(EE_UNITS_GOAL_HOST_RENDEZVOUS, container, 0, &mut io);
        tiles_out(&io, tiles);
    }
    fn center_offset(&mut self, container: u64, point: &mut [f32; 2]) {
        let mut io = EeUnitsGoalIo::default();
        io.f[..2].copy_from_slice(point);
        self.call(EE_UNITS_GOAL_HOST_CENTER_OFFSET, container, 0, &mut io);
        point.copy_from_slice(&io.f[..2]);
    }
    fn garrison_upgrade(&mut self, container: u64, technology: i32) -> (bool, i32, bool) {
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = i64::from(technology);
        self.call(EE_UNITS_GOAL_HOST_GARRISON_UPGRADE, container, 0, &mut io);
        (io.i[0] != 0, io.i[1] as i32, io.i[2] != 0)
    }
    fn unit_goal(&mut self, unit: u64, goal_state: bool) -> (u64, i64) {
        let mut io = EeUnitsGoalIo::default();
        let goal = self.call(EE_UNITS_GOAL_HOST_UNIT_GOAL, unit, u64::from(goal_state), &mut io) as u64;
        (goal, io.i[0])
    }
    fn queued_goal_kind(&mut self, unit: u64, index: i64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_QUEUED_GOAL_KIND, unit, index as u64)
    }
    fn clear_goals(&mut self, unit: u64, index: i64) {
        self.op(EE_UNITS_GOAL_HOST_CLEAR_GOALS, unit, index as u64);
    }
    fn holder(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_CONTAINER, unit, 0) as u64
    }
    fn deposit(&mut self, unit: u64, container: u64) {
        self.op(EE_UNITS_GOAL_HOST_DEPOSIT, unit, container);
    }
    fn garrison_unit(&mut self, container: u64, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_GARRISON_UNIT, container, unit) != 0
    }
    fn load_cargo(&mut self, container: u64, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_LOAD_CARGO, container, unit)
    }
    fn has_cargo(&mut self, container: u64, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_HAS_CARGO, container, unit) != 0
    }
    fn play_sound(&mut self, unit: u64, sound: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_PLAY_SOUND, unit, 0, &[sound]);
    }
    fn replace_class(&mut self, container: u64, class: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_REPLACE_CLASS, container, 0, &[i64::from(class)]);
    }
    fn generate(&mut self) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_GENERATE, 0, 0, &[])[0]
    }
    fn fires_over_walls(&mut self, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_FIRES_OVER_WALLS, unit, 0) != 0
    }
    fn same_player(&mut self, unit: u64, other: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_SAME_PLAYER, unit, other) != 0
    }
    fn hit_points(&mut self, unit: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_HIT_POINTS, unit, 0) as i32
    }
    fn complex_unit(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_COMPLEX_UNIT, unit, 0) as u64
    }
    fn write_f32(&mut self, value: f32) -> i32 {
        self.write(3, i64::from(value.to_bits()))
    }
    fn read_f32(&mut self, value: &mut f32) -> i32 {
        let (result, v) = self.read(3, i64::from(value.to_bits()));
        *value = f32::from_bits(v as u32);
        result
    }
}

impl BuildHost for OpHost<'_> {
    fn build_unit(&mut self, unit: u64) -> EeUnitsBuildUnit {
        let mut fields = EeUnitsBuildUnit::default();
        if unit != 0 {
            let mut io = EeUnitsGoalIo { sink: (&mut fields as *mut EeUnitsBuildUnit).cast(), ..Default::default() };
            self.call(EE_UNITS_GOAL_HOST_BUILD_UNIT, unit, 0, &mut io);
        }
        fields
    }
    fn build_unit_basic(&mut self, unit: u64) -> EeUnitsBuildUnit {
        let mut fields = EeUnitsBuildUnit::default();
        if unit != 0 {
            let mut io = EeUnitsGoalIo { sink: (&mut fields as *mut EeUnitsBuildUnit).cast(), ..Default::default() };
            self.call(EE_UNITS_GOAL_HOST_BUILD_UNIT, unit, 1, &mut io);
        }
        fields
    }
    fn tech_node(&mut self, player: u64, technology: i32) -> u64 {
        self.op_ints(EE_UNITS_GOAL_HOST_TECH_NODE, player, 0, &[i64::from(technology)]) as u64
    }
    fn node_info(&mut self, node: u64) -> (i32, i32, i32) {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_NODE_INFO, node, 0, &mut io);
        (io.i[0] as i32, io.i[1] as i32, io.i[2] as i32)
    }
    fn player_id(&mut self, player: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_PLAYER_ID, player, 0) as i32
    }
    fn class_type(&mut self, class: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_CLASS_UNIT_TYPE, class, 0) as i32
    }
    fn builders_add(&mut self, unit: u64, delta: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_BUILDERS_ADD, unit, 0, &[i64::from(delta)]);
    }
    fn attack_part(&mut self, unit: u64, target: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_ATTACK_PART, unit, target) as u64
    }
    fn in_work_reach(&mut self, unit: u64, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_IN_WORK_REACH, unit, target) != 0
    }
    fn surrounding_tiles(&mut self, target: u64) -> Vec<(i32, i32)> {
        let mut tiles: Vec<(i32, i32)> = Vec::new();
        let mut io = EeUnitsGoalIo { sink: (&mut tiles as *mut Vec<(i32, i32)>).cast(), ..Default::default() };
        self.call(EE_UNITS_GOAL_HOST_SURROUNDING_TILES, target, 0, &mut io);
        tiles
    }
    fn complete_building(&mut self, worker: u64, outer: u64, target_type: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_COMPLETE_BUILDING, worker, outer, &[i64::from(target_type)]);
    }
    fn play_effect(&mut self, unit: u64, effect: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_PLAY_EFFECT, unit, 0, &[i64::from(effect)]);
    }
    fn set_production_time(&mut self, unit: u64, time: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_PRODUCTION_TIME, unit, 0, &[i64::from(time)]);
    }
    fn set_work_timer(&mut self, unit: u64, time: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_WORK_TIMER, unit, 0, &[i64::from(time)]);
    }
    fn face_target(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_FACE_TARGET, unit, target);
    }
    fn fill_parts(&mut self, outer: u64) {
        self.op(EE_UNITS_GOAL_HOST_FILL_PARTS, outer, 0);
    }
    fn next_free_tile(&mut self, unit: u64, target: u64) -> Option<[f32; 3]> {
        let mut io = EeUnitsGoalIo::default();
        if self.call(EE_UNITS_GOAL_HOST_NEXT_FREE_TILE, unit, target, &mut io) == 0 {
            return None;
        }
        Some([io.f[0], io.f[1], io.f[2]])
    }
    fn cancel_foundation(&mut self, outer: u64, worker: u64, node: u64) {
        self.op_ints(EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION, outer, worker, &[node as i64]);
    }
    fn location_goal(&mut self, unit: u64, point: [f32; 3]) {
        self.op_with_point(EE_UNITS_GOAL_HOST_LOCATION_GOAL, unit, 0, point);
    }
    fn help_build_nearby(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_HELP_BUILD, unit, target);
    }
    fn tend_next_farm(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_TEND_FARM, unit, target);
    }
    fn gather_near(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_GATHER_NEAR, unit, target);
    }
    fn gate_side_part(&mut self, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_GATE_SIDE_PART, target, 0) != 0
    }
    fn build_event(&mut self, player: u64, target: u64, repair: bool) {
        self.op_ints(EE_UNITS_GOAL_HOST_BUILD_EVENT, player, target, &[i64::from(repair)]);
    }
    fn abandon_building(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_ABANDON_BUILDING, unit, target);
    }
    fn halt(&mut self, unit: u64, action: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_HALT, unit, 0, &[action]);
    }
    fn site_taken(&mut self, target: u64, player: u64, target_type: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_SITE_TAKEN, target, player, &[i64::from(target_type)]) != 0
    }
    fn footprint_clear(&mut self, target: u64, player: u64, target_type: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_FOOTPRINT_CLEAR, target, player, &[i64::from(target_type)]) != 0
    }
    fn site_free(&mut self, target: u64, player: u64, target_type: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_SITE_FREE, target, player, &[i64::from(target_type)]) != 0
    }
    fn other_goal_queued(&mut self, unit: u64, goal: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_OTHER_GOAL_QUEUED, unit, goal) != 0
    }
    fn resource_users(&mut self, unit: u64) -> i32 {
        self.op(EE_UNITS_GOAL_HOST_RESOURCE_USERS, unit, 0) as i32
    }
    fn new_gather(&mut self, resource: u64, storage: u64, arg: bool) -> u64 {
        self.op_ints(EE_UNITS_GOAL_HOST_NEW_GATHER, resource, storage, &[i64::from(arg)]) as u64
    }
    fn set_goal(&mut self, unit: u64, goal: u64, queue: i32, flag: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_GOAL, unit, goal, &[i64::from(queue), i64::from(flag)]);
    }
    fn nearest_site(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_NEAREST_SITE, unit, 0) as u64
    }
    fn clear_site(&mut self, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_CLEAR_SITE, target, 0);
    }
    fn can_produce(&mut self, target: u64, outer: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_CAN_PRODUCE, target, outer) != 0
    }
    fn can_afford_repair(&mut self, unit: u64, outer: u64, amount: i32) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_CAN_AFFORD_REPAIR, unit, outer, &[i64::from(amount)]) != 0
    }
    fn friendly(&mut self, player: u64, unit: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_FRIENDLY, player, unit) != 0
    }
    fn in_cinematic(&mut self) -> bool {
        self.op(EE_UNITS_GOAL_HOST_IN_CINEMATIC, 0, 0) != 0
    }
    fn set_paid(&mut self, outer: u64) {
        self.op(EE_UNITS_GOAL_HOST_SET_PAID, outer, 0);
    }
    fn pay(&mut self, player: u64, node: u64, fraction: f32) {
        let mut io = EeUnitsGoalIo::default();
        io.f[0] = fraction;
        self.call(EE_UNITS_GOAL_HOST_PAY, player, node, &mut io);
    }
    fn set_hit_points(&mut self, unit: u64, hit_points: i32) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_HIT_POINTS, unit, 0, &[i64::from(hit_points)]);
    }
    fn foundation_started(&mut self, outer: u64, target: u64, wonder: bool) {
        self.op_ints(EE_UNITS_GOAL_HOST_FOUNDATION_STARTED, outer, target, &[i64::from(wonder)]);
    }
    fn goal_arg(&mut self, goal: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_GOAL_ARG, goal, 0) != 0
    }
    fn deposit(&mut self, unit: u64, target: u64) {
        self.op(EE_UNITS_GOAL_HOST_DEPOSIT, unit, target);
    }
    fn goal_state(&mut self, unit: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_UNIT_GOAL, unit, 1) as u64
    }
    fn write_f32(&mut self, value: f32) -> i32 {
        self.write(3, i64::from(value.to_bits()))
    }
    fn read_f32(&mut self, value: &mut f32) -> i32 {
        let (result, v) = self.read(3, i64::from(value.to_bits()));
        *value = f32::from_bits(v as u32);
        result
    }
}

impl crate::location::LocationHost for OpHost<'_> {
    fn unit_category(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_UNIT_CATEGORY, unit, 0)
    }
    fn unit_transformation(&mut self, unit: u64) -> ee_ge::types::GeTransformation {
        let mut t = ee_ge::types::GeTransformation::default();
        let mut io =
            EeUnitsGoalIo { sink: (&mut t as *mut ee_ge::types::GeTransformation).cast(), ..Default::default() };
        self.call(EE_UNITS_GOAL_HOST_UNIT_TRANSFORMATION, unit, 0, &mut io);
        t
    }
    fn set_transformation(&mut self, unit: u64, t: &ee_ge::types::GeTransformation) {
        let mut copy = *t;
        let mut io =
            EeUnitsGoalIo { sink: (&mut copy as *mut ee_ge::types::GeTransformation).cast(), ..Default::default() };
        self.call(EE_UNITS_GOAL_HOST_SET_TRANSFORMATION, unit, 0, &mut io);
    }
    fn set_target_search_timer(&mut self, unit: u64, value: i64) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_TARGET_SEARCH_TIMER, unit, 0, &[value]);
    }
    fn goal_queue_len(&mut self, unit: u64) -> i64 {
        self.op(EE_UNITS_GOAL_HOST_GOAL_QUEUE_LEN, unit, 0)
    }
    fn write_f32(&mut self, value: f32) -> i32 {
        self.write(3, i64::from(value.to_bits()))
    }
    fn read_f32(&mut self, value: &mut f32) -> i32 {
        let (result, v) = self.read(3, i64::from(value.to_bits()));
        *value = f32::from_bits(v as u32);
        result
    }
}

impl OpHost<'_> {
    fn op_point_io(&mut self, op: i32, a: u64, b: u64, out: &mut [f32; 3]) -> bool {
        let mut io = EeUnitsGoalIo::default();
        io.f[..3].copy_from_slice(out);
        let found = self.call(op, a, b, &mut io) != 0;
        out.copy_from_slice(&io.f[..3]);
        found
    }
}

impl GatherHost for OpHost<'_> {
    fn farm_tile(&mut self, farm: u64, near: u64) -> (i32, i32) {
        let mut io = EeUnitsGoalIo::default();
        self.call(EE_UNITS_GOAL_HOST_FARM_TILE, farm, near, &mut io);
        (io.i[0] as i32, io.i[1] as i32)
    }
    fn resource_spot(&mut self, unit: u64, resource: u64, out: &mut [f32; 3]) -> bool {
        self.op_point_io(EE_UNITS_GOAL_HOST_RESOURCE_SPOT, unit, resource, out)
    }
    fn next_resource(&mut self, unit: u64, resource: u64, kind: i32, passed: &[u64]) -> u64 {
        let mut io = EeUnitsGoalIo { list: passed.as_ptr(), list_len: passed.len(), ..Default::default() };
        io.i[0] = i64::from(kind);
        self.call(EE_UNITS_GOAL_HOST_NEXT_RESOURCE, unit, resource, &mut io) as u64
    }
    fn find_storage(&mut self, unit: u64, new: bool) -> u64 {
        self.op_ints(EE_UNITS_GOAL_HOST_FIND_STORAGE, unit, 0, &[i64::from(new)]) as u64
    }
    fn free_tile(&mut self, unit: u64, storage: u64, out: &mut [f32; 3]) -> bool {
        self.op_point_io(EE_UNITS_GOAL_HOST_FREE_TILE, unit, storage, out)
    }
    fn tile_height(&mut self, unit: u64) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_TILE_HEIGHT, unit, 0, &[])[0]
    }
    fn at_resource_spot(&mut self, unit: u64, resource: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_AT_RESOURCE_SPOT, unit, resource) != 0
    }
    fn next_farm(&mut self, unit: u64, resource: u64) {
        self.op(EE_UNITS_GOAL_HOST_NEXT_FARM, unit, resource);
    }
    fn storage_search(&mut self, unit: u64) -> Option<u64> {
        let found = self.op(EE_UNITS_GOAL_HOST_STORAGE_SEARCH, unit, 0);
        if found == -1 {
            None
        } else {
            Some(found as u64)
        }
    }
    fn gather_spot(&mut self, unit: u64, resource: u64) -> Option<[f32; 3]> {
        let mut out = [0.0f32; 3];
        self.op_point_io(EE_UNITS_GOAL_HOST_GATHER_SPOT, unit, resource, &mut out).then_some(out)
    }
    fn begin_gathering(&mut self, unit: u64, resource: u64) {
        self.op(EE_UNITS_GOAL_HOST_BEGIN_GATHERING, unit, resource);
    }
    fn deposit_load(&mut self, unit: u64, storage: u64) {
        self.op(EE_UNITS_GOAL_HOST_DEPOSIT_LOAD, unit, storage);
    }
    fn next_tree(&mut self, unit: u64, resource: u64) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_NEXT_TREE, unit, resource) as u64
    }
    fn build_farms(&mut self, unit: u64, goal: u64) {
        self.op(EE_UNITS_GOAL_HOST_BUILD_FARMS, unit, goal);
    }
    fn hunt(&mut self, unit: u64, resource: u64, arg: bool) -> bool {
        self.op_ints(EE_UNITS_GOAL_HOST_HUNT, unit, resource, &[i64::from(arg)]) != 0
    }
    fn set_gathered(&mut self, unit: u64, resource: u64) {
        self.op(EE_UNITS_GOAL_HOST_SET_GATHERED, unit, resource);
    }
    fn gather_room(&mut self, unit: u64, resource: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_GATHER_ROOM, unit, resource) != 0
    }
    fn last_waypoint(&mut self, unit: u64) -> [f32; 3] {
        self.op_point(EE_UNITS_GOAL_HOST_LAST_WAYPOINT, unit, 0, &[])
    }
    fn resource_reach(&mut self, unit: u64, resource: u64, point: [f32; 3]) -> bool {
        self.op_with_point(EE_UNITS_GOAL_HOST_RESOURCE_REACH, unit, resource, point) != 0
    }
    fn storage_reach(&mut self, storage: u64, point: [f32; 3]) -> bool {
        self.op_with_point(EE_UNITS_GOAL_HOST_STORAGE_REACH, storage, 0, point) != 0
    }
    fn set_active(&mut self, goal: u64, active: bool) {
        self.op_ints(EE_UNITS_GOAL_HOST_SET_ACTIVE, goal, 0, &[i64::from(active)]);
    }
    fn find_ghost(&mut self, id: u32, complex: bool) -> u64 {
        self.op(EE_UNITS_GOAL_HOST_FIND_GHOST, u64::from(id), u64::from(complex)) as u64
    }
    fn at_resource(&mut self, unit: u64, resource: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_AT_RESOURCE, unit, resource) != 0
    }
    fn at_storage(&mut self, unit: u64, storage: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_AT_STORAGE, unit, storage) != 0
    }
    fn line_to_unit_blocked(&mut self, unit: u64, target: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_LINE_TO_UNIT_BLOCKED, unit, target) != 0
    }
    fn accessible(&mut self, unit: u64, resource: u64) -> bool {
        self.op(EE_UNITS_GOAL_HOST_ACCESSIBLE, unit, resource) != 0
    }
    fn path_length(&mut self, unit: u64, tile: (i32, i32)) -> f32 {
        self.op_point(EE_UNITS_GOAL_HOST_PATH_LENGTH, unit, 0, &[i64::from(tile.0), i64::from(tile.1)])[0]
    }
    fn gather_work(&mut self, unit: u64, resource: u64) {
        self.op(EE_UNITS_GOAL_HOST_GATHER_WORK, unit, resource);
    }
    fn naval_search(&mut self, unit: u64, resource: u64) -> (i64, u64) {
        let mut io = EeUnitsGoalIo::default();
        let found = self.call(EE_UNITS_GOAL_HOST_NAVAL_SEARCH, unit, resource, &mut io) as u64;
        (io.i[0], found)
    }
    fn reset_active_path(&mut self, unit: u64) {
        self.op(EE_UNITS_GOAL_HOST_RESET_ACTIVE_PATH, unit, 0);
    }
}

// ---- the goal's bodies, by operation (`EEUGRust`'s virtuals and accessors)
pub const EE_UNITS_GOAL_DESTROY: i32 = 1;
/// `b`: the base goal's `GetB1C()`; the new goal's handle
pub const EE_UNITS_GOAL_CLONE: i32 = 2;
pub const EE_UNITS_GOAL_DUMP: i32 = 3;
pub const EE_UNITS_GOAL_CHECKSUM: i32 = 4;
pub const EE_UNITS_GOAL_DEBUG_LINE: i32 = 5;
pub const EE_UNITS_GOAL_DEBUG_TEXT: i32 = 6;
/// `io.f`: the location
pub const EE_UNITS_GOAL_LOCATION: i32 = 7;
/// `a`: the unit; `io.f`: the destination (the unit's active group told)
pub const EE_UNITS_GOAL_DESTINATION: i32 = 8;
pub const EE_UNITS_GOAL_START_ACTION: i32 = 9;
pub const EE_UNITS_GOAL_SET_OWNER: i32 = 10;
pub const EE_UNITS_GOAL_STEP: i32 = 11;
pub const EE_UNITS_GOAL_LOAD: i32 = 12;
/// `a`, `b`: the save's major and minor version
pub const EE_UNITS_GOAL_SAVE: i32 = 13;
pub const EE_UNITS_GOAL_REPAIR: i32 = 14;
pub const EE_UNITS_GOAL_START: i32 = 15;
/// `a`: the unit, `b`: the event; 0 or 1
pub const EE_UNITS_GOAL_EVALUATE: i32 = 16;
/// `a`: the old complex unit, `b`: the new one, `io.i[0]` the unit whose goal it is
pub const EE_UNITS_GOAL_REPLACE_UNIT: i32 = 17;
/// `GetMaxPathAttempts` (the base goal's 3 but for the unload's 9)
pub const EE_UNITS_GOAL_MAX_PATH_ATTEMPTS: i32 = 18;
/// `a`: the unit; `Suits` (true but for the unload)
pub const EE_UNITS_GOAL_SUITS: i32 = 19;
/// `a`: the new path attempt count (`SetPathAttempts`, after the base goal's; nothing more for these kinds)
pub const EE_UNITS_GOAL_SET_PATH_ATTEMPTS: i32 = 22;
/// `a`: the unit; `AddCreatedUnit` and `Abandon` (the base goal's do nothing)
pub const EE_UNITS_GOAL_ADD_CREATED_UNIT: i32 = 20;
pub const EE_UNITS_GOAL_ABANDON: i32 = 21;
// the classes' own: creation (the goal made by `new EEUGRust(kind, arg)`) and accessors
/// a patrol: `a` the group (0: a new empty one), `b` the point
pub const EE_UNITS_GOAL_NEW_PATROL: i32 = 30;
pub const EE_UNITS_GOAL_PATROL_TARGET: i32 = 31;
pub const EE_UNITS_GOAL_PATROL_GROUP: i32 = 32;
pub const EE_UNITS_GOAL_PATROL_SET_GROUP: i32 = 33;
pub const EE_UNITS_GOAL_PATROL_NEW_EMPTY: i32 = 34;
/// a cast: `a` the source, `b` the target, `io.i` the tile x, y and the technology
pub const EE_UNITS_GOAL_NEW_CALAMITY: i32 = 40;
/// `io.i`: the technology, the tile x, y; the target unit (the result)
pub const EE_UNITS_GOAL_CALAMITY_FIELDS: i32 = 41;
/// `io.f`: where the calamity goes
pub const EE_UNITS_GOAL_CALAMITY_TARGET_LOCATION: i32 = 42;
/// the load's empty cast
pub const EE_UNITS_GOAL_CALAMITY_NEW_EMPTY: i32 = 43;
pub const EE_UNITS_GOAL_NEW_EXPLORE: i32 = 50;
/// `io.i`: the tile x, y (out; in for the set)
pub const EE_UNITS_GOAL_EXPLORE_TILE: i32 = 51;
pub const EE_UNITS_GOAL_EXPLORE_SET_TILE: i32 = 52;
/// an unload: `a` the transport, `io.i` the tile
pub const EE_UNITS_GOAL_NEW_UNLOAD: i32 = 60;
pub const EE_UNITS_GOAL_UNLOAD_NEW_EMPTY: i32 = 61;
pub const EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT: i32 = 62;
pub const EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD: i32 = 63;
/// the transport (the result), `io.i` the unload tile
pub const EE_UNITS_GOAL_UNLOAD_FIELDS: i32 = 64;
/// `io.i`: a new unload tile, the unload tiles found for it
pub const EE_UNITS_GOAL_UNLOAD_SET_DESTINATION: i32 = 65;
/// the transport dropped without its release
pub const EE_UNITS_GOAL_UNLOAD_FORGET_TRANSPORT: i32 = 66;
/// a load goal of the kind: `a` the container, `io.list` the units (already sorted by id, no duplicates)
pub const EE_UNITS_GOAL_NEW_LOAD: i32 = 70;
pub const EE_UNITS_GOAL_LOAD_NEW_EMPTY: i32 = 71;
/// `a`: a unit to load (referenced, unless listed)
pub const EE_UNITS_GOAL_LOAD_ADD_UNIT: i32 = 72;
/// `a`: whether the unit is listed
pub const EE_UNITS_GOAL_LOAD_HAS_UNIT: i32 = 73;
pub const EE_UNITS_GOAL_LOAD_UNIT_COUNT: i32 = 74;
/// `a`: the index; the unit
pub const EE_UNITS_GOAL_LOAD_UNIT_AT: i32 = 75;
/// the container (the result), `io.i`: the container and unit tiles, the garrison's valid flag, the unit count
pub const EE_UNITS_GOAL_LOAD_FIELDS: i32 = 76;

// the round-19 operations (from 300: the combat goals' own take 200-299, their host's from 1000)
/// `a`: the unit; `Work` (the base goal's does nothing)
pub const EE_UNITS_GOAL_WORK: i32 = 300;
/// `a`: the unit; `PathTaken` (the base goal's does nothing)
pub const EE_UNITS_GOAL_PATH_TAKEN: i32 = 301;
/// a build/repair goal: `a` the player, `b` the building
pub const EE_UNITS_GOAL_NEW_BUILD: i32 = 310;
pub const EE_UNITS_GOAL_BUILD_NEW_EMPTY: i32 = 311;
/// the building (the result), `io.i`: its type, technology, tech tree node and class
pub const EE_UNITS_GOAL_BUILD_FIELDS: i32 = 312;
/// `IsBuildSiteTaken`
pub const EE_UNITS_GOAL_BUILD_SITE_TAKEN: i32 = 313;
/// a gather goal: `a` the resource, `b` the storage
pub const EE_UNITS_GOAL_NEW_GATHER: i32 = 320;
pub const EE_UNITS_GOAL_GATHER_NEW_EMPTY: i32 = 321;
/// the resource (the result), `io.i`: the storage, the gathering and waiting flags; `io.f` the destination
pub const EE_UNITS_GOAL_GATHER_FIELDS: i32 = 322;
/// `a`: the new resource, storage (`SetResource`, `SetStorage`)
pub const EE_UNITS_GOAL_GATHER_SET_RESOURCE: i32 = 323;
pub const EE_UNITS_GOAL_GATHER_SET_STORAGE: i32 = 324;
/// `a`: the unit; `FindGatherSpot`
pub const EE_UNITS_GOAL_GATHER_FIND_SPOT: i32 = 325;
// the round-21 operations (from 480)
/// a location goal: `io.f` the location and yaw, `io.i` the forced-move, facing, auto-waypoint flags and the group
/// path state (no group movement)
pub const EE_UNITS_GOAL_NEW_LOCATION: i32 = 480;
/// `a`: the unit; `FaceYaw` (0 or 1)
pub const EE_UNITS_GOAL_FACE_YAW: i32 = 481;

/// `kGoalUnloadTransport`, `kGoalPatrol`, `kGoalExplore`, `kGoalCastCalamity`
pub const GOAL_UNLOAD_TRANSPORT: u8 = 8;
pub const GOAL_PATROL: u8 = 9;
pub const GOAL_EXPLORE: u8 = 10;
pub const GOAL_CAST_CALAMITY: u8 = 11;

/// One body of a Rust goal: `op` (`EE_UNITS_GOAL_*`) on the goal object `goal` of kind `kind` (`a`, `b`, `io` as
/// the operation takes them); its result (a handle, a status, a checksum or a flag; 0 otherwise).
///
/// # Safety
/// `host` points to a valid table for the call, `io` to a valid block.
#[no_mangle]
pub unsafe extern "C" fn ee_units_rust_goal(
    op: i32,
    kind: u8,
    goal: u64,
    a: u64,
    b: u64,
    io: *mut EeUnitsGoalIo,
    host: *const EeUnitsGoalHost,
) -> i64 {
    if !matches!(
        kind,
        GOAL_LOCATION
            | GOAL_PATROL
            | GOAL_CAST_CALAMITY
            | GOAL_EXPLORE
            | GOAL_UNLOAD_TRANSPORT
            | GOAL_LOAD_TRANSPORT
            | GOAL_LOAD_GARRISON
            | GOAL_BUILD_REPAIR
            | GOAL_GATHER
    ) {
        // another crate's kinds (the combat lane's attack goals: ee_combat), through the same host
        let other = *OTHER_KINDS.lock().unwrap_or_else(|e| e.into_inner());
        // SAFETY: the caller's contract, passed on
        return other.map_or(0, |f| unsafe { f(op, kind, goal, a, b, io.cast(), host.cast()) });
    }
    // SAFETY: the caller's contract
    let (io, table) = unsafe { (&mut *io, &*host) };
    let mut h = OpHost(table);
    // the base goal's answers for the classes that keep them
    if kind != GOAL_UNLOAD_TRANSPORT
        && kind != GOAL_LOAD_TRANSPORT
        && kind != GOAL_LOAD_GARRISON
        && kind != GOAL_BUILD_REPAIR
        && kind != GOAL_GATHER
    {
        match op {
            EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 3,
            EE_UNITS_GOAL_SUITS => return 1,
            _ => {}
        }
    }
    match kind {
        GOAL_LOCATION => crate::location::body(op, goal, a, b, io, &mut h),
        GOAL_PATROL => crate::patrol::body(op, goal, a, b, io, &mut h),
        GOAL_CAST_CALAMITY => crate::calamity::body(op, goal, a, b, io, &mut h),
        GOAL_EXPLORE => crate::explore::body(op, goal, a, b, io, &mut h),
        GOAL_UNLOAD_TRANSPORT => crate::unload::body(op, goal, a, b, io, &mut h),
        GOAL_LOAD_TRANSPORT | GOAL_LOAD_GARRISON => crate::load::body(op, kind, goal, a, b, io, &mut h),
        GOAL_BUILD_REPAIR => crate::build::body(op, goal, a, b, io, &mut h),
        GOAL_GATHER => crate::gather::body(op, goal, a, b, io, &mut h),
        _ => 0,
    }
}

/// The bodies of the goal kinds other crates own: `f(op, kind, goal, a, b, io, host)` with the `EeUnitsGoalIo` and
/// `EeUnitsGoalHost` pointers (ee_combat's goal_objects.rs mirrors their layout). ee_rust registers them at start-up
/// (ee_units cannot depend on ee_combat, whose bridges are ee_rust's).
#[allow(clippy::type_complexity)]
static OTHER_KINDS: std::sync::Mutex<Option<unsafe fn(i32, u8, u64, u64, u64, *mut c_void, *const c_void) -> i64>> =
    std::sync::Mutex::new(None);

/// Registers the other crates' goal bodies (see `OTHER_KINDS`).
pub fn register_other_goal_kinds(f: unsafe fn(i32, u8, u64, u64, u64, *mut c_void, *const c_void) -> i64) {
    *OTHER_KINDS.lock().unwrap_or_else(|e| e.into_inner()) = Some(f);
}

/// `%08lx` of an `unsigned long`
pub fn hex8(value: u32) -> String {
    format!("{value:08x}")
}
