//! The goals whose class moved whole into Rust (RUST_PLAN R5b.4, rounds 16 and 17): one C++ class, `EEUGRust`
//! (ee/EEUGRust.h), keeps the base goal's fields and vtable; every virtual is one call of
//! [`ee_units_rust_goal`], which dispatches by the goal's kind to the class's bodies (`patrol`, `calamity`,
//! `explore`, `unload`), their state a Rust table by the goal's object. The C++ answers through one operation callback
//! ([`EeUnitsGoalHost`]): every read and world call the bodies make is one numbered operation
//! (`EE_UNITS_GOAL_HOST_*`) with two handles and an argument block ([`EeUnitsGoalIo`]).
//!
//! Units, groups and goals are C++ objects held by handle (their addresses; an `EEComplexUnit` is its `EEUnit` at
//! the same address). Text (the debug texts, the out-of-synch dump) is appended through the host as UTF-8.

use std::ffi::c_void;

use crate::calamity::CalamityHost;
use crate::explore::ExploreHost;
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
/// The argument block of one operation (ints, floats, text, a list sink).
#[repr(C)]
pub struct EeUnitsGoalIo {
    pub i: [i64; 8],
    pub f: [f32; 4],
    pub text: *const u8,
    pub len: usize,
    /// a `Vec<(i32, i32)>` the host pushes tiles onto (`ee_units_goal_push_tile`)
    pub sink: *mut c_void,
}

impl Default for EeUnitsGoalIo {
    fn default() -> Self {
        EeUnitsGoalIo { i: [0; 8], f: [0.0; 4], text: std::ptr::null(), len: 0, sink: std::ptr::null_mut() }
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
/// `i[0]`: the type (0 `unsigned long`, 1 `long`, 2 `bool`), `i[1]` the value
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
/// `a`: the old complex unit, `b`: the new one
pub const EE_UNITS_GOAL_REPLACE_UNIT: i32 = 17;
/// `GetMaxPathAttempts` (the base goal's 3 but for the unload's 9)
pub const EE_UNITS_GOAL_MAX_PATH_ATTEMPTS: i32 = 18;
/// `a`: the unit; `Suits` (true but for the unload)
pub const EE_UNITS_GOAL_SUITS: i32 = 19;
/// `a`: the new path attempt count (`SetPathAttempts`, after the base goal's; nothing more for these kinds)
pub const EE_UNITS_GOAL_SET_PATH_ATTEMPTS: i32 = 20;
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
    if !matches!(kind, GOAL_PATROL | GOAL_CAST_CALAMITY | GOAL_EXPLORE | GOAL_UNLOAD_TRANSPORT) {
        // another crate's kinds (the combat lane's attack goals: ee_combat), through the same host
        let other = *OTHER_KINDS.lock().unwrap_or_else(|e| e.into_inner());
        // SAFETY: the caller's contract, passed on
        return other.map_or(0, |f| unsafe { f(op, kind, goal, a, b, io.cast(), host.cast()) });
    }
    // SAFETY: the caller's contract
    let (io, table) = unsafe { (&mut *io, &*host) };
    let mut h = OpHost(table);
    // the base goal's answers for the classes that keep them
    if kind != GOAL_UNLOAD_TRANSPORT {
        match op {
            EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 3,
            EE_UNITS_GOAL_SUITS => return 1,
            _ => {}
        }
    }
    match kind {
        GOAL_PATROL => crate::patrol::body(op, goal, a, b, io, &mut h),
        GOAL_CAST_CALAMITY => crate::calamity::body(op, goal, a, b, io, &mut h),
        GOAL_EXPLORE => crate::explore::body(op, goal, a, b, io, &mut h),
        GOAL_UNLOAD_TRANSPORT => crate::unload::body(op, goal, a, b, io, &mut h),
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
