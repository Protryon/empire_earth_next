//! The complex unit's save and load parts that were C++ loops (the units' lane, round 8; were EEComplexDoppelganger.cpp
//! `EEUnitSaveOperation`'s compound cases and `EEComplexUnitLoadSection`): the citizen queue's, waypoints', turret
//! angles', goal queues' and ignored targets' save sections, the path goal and the herd's part, the location and path
//! loads, the scale, action, last attacker and owner sets, and the waypoints' and goal queues' load sections. Each
//! C++ statement is one `EE_UNITS_DOPS_*` operation of the same callback (`EEUnitSaveOperation`), asked in the
//! original's order; the file's primitives are the callback's (`EESavePrimitive`, the original's overloads).
//! [`DopSaveHost`] answers the composite operations in front of the callback, under the view hosts
//! (unit_save_views.rs).

// `c_long` and `c_ulong` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::player_save::{
    EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT, EE_UNITS_PLAYER_SAVE_READ_LONG,
    EE_UNITS_PLAYER_SAVE_READ_ULONG,
};
use crate::units::unit_load::*;
use crate::units::unit_save::*;

// ---- the statements (`op` of the callback; `index` an element; the value in io)
/// `io.i = mCitizenBuildQueue.size()`
pub const EE_UNITS_DOPS_CITIZEN_QUEUE_SIZE: i32 = 400;
/// `io.i = mCitizenBuildQueue[index]->mIsValid`
pub const EE_UNITS_DOPS_CITIZEN_VALID: i32 = 401;
/// `io.i = mCitizenBuildQueue[index]->mUnitID`
pub const EE_UNITS_DOPS_CITIZEN_ID: i32 = 402;
/// `io.i = mWaypoints.size()`
pub const EE_UNITS_DOPS_WAYPOINT_COUNT: i32 = 403;
/// `mWaypoints[index].mLocation.Save(file)` (the result)
pub const EE_UNITS_DOPS_WAYPOINT_SAVE_LOCATION: i32 = 404;
/// `io.f = mWaypoints[index].mDistance`
pub const EE_UNITS_DOPS_WAYPOINT_DISTANCE: i32 = 405;
/// `io.f = mWaypoints[index].mStartDistance`
pub const EE_UNITS_DOPS_WAYPOINT_START_DISTANCE: i32 = 406;
/// `io.i = GetModel()->GetNumParts()`
pub const EE_UNITS_DOPS_MODEL_PARTS: i32 = 407;
/// `io.f = mTurretAngles[index]`
pub const EE_UNITS_DOPS_TURRET_ANGLE: i32 = 408;
/// `io.i = mGoalQueues.size()`
pub const EE_UNITS_DOPS_GOAL_QUEUE_COUNT: i32 = 409;
/// `mGoalQueues[index].Save(major, minor, file)` (the result)
pub const EE_UNITS_DOPS_GOAL_QUEUE_SAVE: i32 = 410;
/// `io.i = mIgnoredTargets.size()`
pub const EE_UNITS_DOPS_IGNORED_COUNT: i32 = 411;
/// `io.i = mIgnoredTargets[index]`'s `mTileX`, `mTileY`, `mFromX`, `mFromY` (`long`s), `mUnitID`, `mTime`
/// (`unsigned long`s): the six operations from here
pub const EE_UNITS_DOPS_IGNORED_TILE_X: i32 = 412;
pub const EE_UNITS_DOPS_IGNORED_TILE_Y: i32 = 413;
pub const EE_UNITS_DOPS_IGNORED_FROM_X: i32 = 414;
pub const EE_UNITS_DOPS_IGNORED_FROM_Y: i32 = 415;
pub const EE_UNITS_DOPS_IGNORED_UNIT_ID: i32 = 416;
pub const EE_UNITS_DOPS_IGNORED_TIME: i32 = 417;
/// `io.b = gPathManager.GetPathGoal(unit, the tile)`
pub const EE_UNITS_DOPS_PATH_GOAL: i32 = 418;
/// the tile's `Save(file)` (the result)
pub const EE_UNITS_DOPS_TILE_SAVE: i32 = 419;
/// the tile `= U2DPoint<long>()`
pub const EE_UNITS_DOPS_TILE_RESET: i32 = 420;
/// `io.i = mHerd != NULL`
pub const EE_UNITS_DOPS_HAS_HERD: i32 = 421;
/// `mHerd->HasUnit(unit)`
pub const EE_UNITS_DOPS_HERD_HAS_UNIT: i32 = 422;
/// the location's `Load(file)` (a `GETransformation`; the result)
pub const EE_UNITS_DOPS_LOCATION_LOAD: i32 = 423;
/// `mPhysicalModel.SetLocation(` the location `)`
pub const EE_UNITS_DOPS_SET_LOCATION: i32 = 425;
/// the tile's `Load(file)` (the result)
pub const EE_UNITS_DOPS_TILE_LOAD: i32 = 426;
/// `gPathManager.AddPending(mUnitID, the tile)`
pub const EE_UNITS_DOPS_ADD_PENDING: i32 = 427;
/// `io.i = GetIsLoadingScenario()`
pub const EE_UNITS_DOPS_LOADING_SCENARIO: i32 = 428;
/// `io.i = GetUnitClass() != NULL`
pub const EE_UNITS_DOPS_HAS_CLASS: i32 = 429;
/// `io.f = GetUnitClass()->GetScale()`
pub const EE_UNITS_DOPS_CLASS_SCALE: i32 = 430;
/// `mPhysicalModel.SetScale(io.f)`
pub const EE_UNITS_DOPS_SET_SCALE: i32 = 431;
/// `GetUnitClass()->FindAction(io.i) != NULL` (the result)
pub const EE_UNITS_DOPS_FINDS_ACTION: i32 = 432;
/// `StartAction(io.i)`
pub const EE_UNITS_DOPS_START_ACTION: i32 = 433;
/// `io.i = mLastAttacker != NULL`
pub const EE_UNITS_DOPS_HAS_LAST_ATTACKER: i32 = 434;
/// `mLastAttacker->Dereference()`
pub const EE_UNITS_DOPS_DEREFERENCE_LAST_ATTACKER: i32 = 435;
/// `mLastAttacker = NULL`
pub const EE_UNITS_DOPS_CLEAR_LAST_ATTACKER: i32 = 436;
/// `io.i = mOwner != NULL`
pub const EE_UNITS_DOPS_HAS_OWNER: i32 = 437;
/// `mOwner->Dereference()`
pub const EE_UNITS_DOPS_DEREFERENCE_OWNER: i32 = 438;
/// `mOwner = NULL`
pub const EE_UNITS_DOPS_CLEAR_OWNER: i32 = 439;
/// `mWaypoints.clear()`
pub const EE_UNITS_DOPS_WAYPOINTS_CLEAR: i32 = 440;
/// the waypoint's `mLocation.Load(file)` (the result)
pub const EE_UNITS_DOPS_WAYPOINT_LOAD_LOCATION: i32 = 441;
/// the waypoint's `mDistance = io.f`
pub const EE_UNITS_DOPS_SET_WAYPOINT_DISTANCE: i32 = 442;
/// the waypoint's `mStartDistance = io.f`
pub const EE_UNITS_DOPS_SET_WAYPOINT_START_DISTANCE: i32 = 443;
/// `mWaypoints.push_back(` the waypoint `)`
pub const EE_UNITS_DOPS_WAYPOINT_PUSH: i32 = 444;
/// `mGoalQueues[index].Load(major, minor, file)` (the result)
pub const EE_UNITS_DOPS_GOAL_QUEUE_LOAD: i32 = 446;
/// the spare queue's (`*context.mGoals`) `Load(major, minor, file)` (the result)
pub const EE_UNITS_DOPS_SPARE_QUEUE_LOAD: i32 = 447;
/// `file.Read(mConversionEffect.mValue)` (the result)
pub const EE_UNITS_DOPS_READ_CONVERSION_EFFECT: i32 = 448;
/// `file.Read(mHealEffect.mValue)` (the result)
pub const EE_UNITS_DOPS_READ_HEAL_EFFECT: i32 = 449;

/// `kActionIdle`'s stand-in for a saved action 0
const ACTION_IDLE: i64 = 5000;

/// The callback with the composite operations answered in front of it.
pub(crate) struct DopSaveHost<'a, H: SaveHost> {
    pub(crate) inner: &'a mut H,
}

impl<H: SaveHost> DopSaveHost<'_, H> {
    fn ask(&mut self, op: i32, index: u32) -> EeUnitsUnitSaveIo {
        let mut io = EeUnitsUnitSaveIo::default();
        self.inner.op(op, index, &mut io);
        io
    }
    fn i(&mut self, op: i32, index: u32) -> i64 {
        self.ask(op, index).i
    }
    fn f(&mut self, op: i32, index: u32) -> f32 {
        self.ask(op, index).f
    }
    fn yes(&mut self, op: i32) -> bool {
        self.i(op, 0) != 0
    }
    fn result(&mut self, op: i32, index: u32) -> i32 {
        self.inner.op(op, index, &mut EeUnitsUnitSaveIo::default())
    }
    fn put(&mut self, op: i32, io: EeUnitsUnitSaveIo) -> i32 {
        let mut io = io;
        self.inner.op(op, 0, &mut io)
    }
    fn long(&mut self, v: i64) -> i32 {
        self.put(EE_UNITS_UNIT_SAVE_WRITE_LONG, EeUnitsUnitSaveIo { i: v, ..Default::default() })
    }
    fn ulong(&mut self, v: i64) -> i32 {
        self.put(EE_UNITS_UNIT_SAVE_WRITE_ULONG, EeUnitsUnitSaveIo { i: v, ..Default::default() })
    }
    fn float(&mut self, v: f32) -> i32 {
        self.put(EE_UNITS_UNIT_SAVE_WRITE_FLOAT, EeUnitsUnitSaveIo { f: v, ..Default::default() })
    }
    fn bool(&mut self, v: u8) -> i32 {
        self.put(EE_UNITS_UNIT_SAVE_WRITE_BOOL, EeUnitsUnitSaveIo { b: v, ..Default::default() })
    }
    /// a read primitive from the io's initial value (the original's local's); (result, the value)
    fn read(&mut self, op: i32, io: EeUnitsUnitSaveIo) -> (i32, EeUnitsUnitSaveIo) {
        let mut io = io;
        let r = self.inner.op(op, 0, &mut io);
        (r, io)
    }

    /// The save sections (`EE_UNITS_UNIT_SAVE_SECTION_*`) this host writes; None for the others.
    fn save_section(&mut self, index: u32) -> Option<i32> {
        Some(match index {
            EE_UNITS_UNIT_SAVE_SECTION_CITIZEN_QUEUE => {
                let queued = self.i(EE_UNITS_DOPS_CITIZEN_QUEUE_SIZE, 0) as c_long;
                let mut length: c_long = 0;
                for j in 0..queued {
                    if self.i(EE_UNITS_DOPS_CITIZEN_VALID, j as u32) != 0 {
                        length += 1;
                    }
                }
                let mut r = self.long(i64::from(length));
                let mut j = 0;
                while j < queued && r == 0 {
                    if self.i(EE_UNITS_DOPS_CITIZEN_VALID, j as u32) != 0 {
                        let id = self.i(EE_UNITS_DOPS_CITIZEN_ID, j as u32) as c_ulong;
                        r = self.ulong(id as i64);
                    }
                    j += 1;
                }
                r
            }
            EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS => {
                let length = self.i(EE_UNITS_DOPS_WAYPOINT_COUNT, 0) as c_long;
                let mut r = self.long(i64::from(length));
                let mut p = 0;
                while p < length && r == 0 {
                    let distance = self.f(EE_UNITS_DOPS_WAYPOINT_DISTANCE, p as u32);
                    let start = self.f(EE_UNITS_DOPS_WAYPOINT_START_DISTANCE, p as u32);
                    r = self.result(EE_UNITS_DOPS_WAYPOINT_SAVE_LOCATION, p as u32);
                    if r == 0 {
                        r = self.float(distance);
                        if r == 0 {
                            r = self.float(start);
                        }
                    }
                    p += 1;
                }
                r
            }
            EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES => {
                let length = self.i(EE_UNITS_DOPS_MODEL_PARTS, 0) as c_long;
                let mut r = self.long(i64::from(length));
                let mut q = 0;
                while q < length && r == 0 {
                    let angle = self.f(EE_UNITS_DOPS_TURRET_ANGLE, q as u32);
                    r = self.float(angle);
                    q += 1;
                }
                r
            }
            EE_UNITS_UNIT_SAVE_SECTION_GOAL_QUEUES => {
                let count = self.i(EE_UNITS_DOPS_GOAL_QUEUE_COUNT, 0) as c_ulong;
                let mut r = self.ulong(count as i64);
                for item in 0..count {
                    if r == 0 {
                        r = self.result(EE_UNITS_DOPS_GOAL_QUEUE_SAVE, item as u32);
                    }
                }
                r
            }
            EE_UNITS_UNIT_SAVE_SECTION_IGNORED_TARGETS => {
                let count = self.i(EE_UNITS_DOPS_IGNORED_COUNT, 0) as c_ulong;
                let mut r = self.ulong(count as i64);
                let mut k = 0;
                while (k as u64) < count as u64 && r == 0 {
                    let at = k as u32;
                    let v = self.i(EE_UNITS_DOPS_IGNORED_TILE_X, at);
                    r = self.long(v);
                    if r == 0 {
                        let v = self.i(EE_UNITS_DOPS_IGNORED_TILE_Y, at);
                        r = self.long(v);
                    }
                    if r == 0 {
                        let v = self.i(EE_UNITS_DOPS_IGNORED_FROM_X, at);
                        r = self.long(v);
                    }
                    if r == 0 {
                        let v = self.i(EE_UNITS_DOPS_IGNORED_FROM_Y, at);
                        r = self.long(v);
                    }
                    if r == 0 {
                        let v = self.i(EE_UNITS_DOPS_IGNORED_UNIT_ID, at);
                        r = self.ulong(v);
                    }
                    if r == 0 {
                        let v = self.i(EE_UNITS_DOPS_IGNORED_TIME, at);
                        r = self.ulong(v);
                    }
                    k += 1;
                }
                r
            }
            _ => return None,
        })
    }

    /// The save parts (`EE_UNITS_UNIT_SAVE_PART_*`) this host writes; None for the others.
    fn save_part(&mut self, index: u32) -> Option<i32> {
        Some(match index {
            EE_UNITS_UNIT_SAVE_PART_PATH => {
                self.result(EE_UNITS_DOPS_TILE_RESET, 0);
                let has_path = self.ask(EE_UNITS_DOPS_PATH_GOAL, 0).b;
                let mut r = self.bool(u8::from(has_path != 0));
                if r == 0 {
                    r = self.result(EE_UNITS_DOPS_TILE_SAVE, 0);
                }
                r
            }
            EE_UNITS_UNIT_SAVE_PART_HERD_HAS_UNIT => {
                if self.yes(EE_UNITS_DOPS_HAS_HERD) {
                    self.result(EE_UNITS_DOPS_HERD_HAS_UNIT, 0);
                }
                0
            }
            _ => return None,
        })
    }

    /// The load parts (`EE_UNITS_UNIT_LOAD_PART_*`) this host reads; None for the others.
    fn load_part(&mut self, index: u32) -> Option<i32> {
        Some(match index {
            EE_UNITS_UNIT_LOAD_PART_LOCATION => {
                let r = self.result(EE_UNITS_DOPS_LOCATION_LOAD, 0);
                if r == 0 {
                    self.result(EE_UNITS_DOPS_SET_LOCATION, 0);
                }
                r
            }
            EE_UNITS_UNIT_LOAD_PART_PATH => {
                self.result(EE_UNITS_DOPS_TILE_RESET, 0);
                let (mut r, io) = self.read(EE_UNITS_PLAYER_SAVE_READ_BOOL, EeUnitsUnitSaveIo::default());
                if r == 0 {
                    r = self.result(EE_UNITS_DOPS_TILE_LOAD, 0);
                }
                if io.b == 0 {
                    self.result(EE_UNITS_DOPS_ADD_PENDING, 0);
                }
                r
            }
            _ => return None,
        })
    }

    /// The load sets (`EE_UNITS_UNIT_LOAD_SET_*`) this host does; false for the others.
    fn load_set(&mut self, index: u32, io: &EeUnitsUnitSaveIo) -> bool {
        match index {
            EE_UNITS_UNIT_LOAD_SET_SCALE => {
                let mut scale = io.f;
                if self.yes(EE_UNITS_DOPS_LOADING_SCENARIO) || scale == 0.0 {
                    if !self.yes(EE_UNITS_DOPS_HAS_CLASS) {
                        if scale == 0.0 {
                            scale = 1.0;
                        }
                    } else {
                        scale = self.f(EE_UNITS_DOPS_CLASS_SCALE, 0);
                    }
                }
                self.put(EE_UNITS_DOPS_SET_SCALE, EeUnitsUnitSaveIo { f: scale, ..Default::default() });
            }
            EE_UNITS_UNIT_LOAD_SET_ACTION => {
                let mut value = io.i as c_ulong;
                if self.yes(EE_UNITS_DOPS_HAS_CLASS)
                    && self.put(EE_UNITS_DOPS_FINDS_ACTION, EeUnitsUnitSaveIo { i: value as i64, ..Default::default() })
                        != 0
                {
                    if value == 0 {
                        value = ACTION_IDLE as c_ulong;
                    }
                    self.put(EE_UNITS_DOPS_START_ACTION, EeUnitsUnitSaveIo { i: value as i64, ..Default::default() });
                }
            }
            EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER_CLEAR => {
                if self.yes(EE_UNITS_DOPS_HAS_LAST_ATTACKER) {
                    self.result(EE_UNITS_DOPS_DEREFERENCE_LAST_ATTACKER, 0);
                }
                self.result(EE_UNITS_DOPS_CLEAR_LAST_ATTACKER, 0);
            }
            EE_UNITS_UNIT_LOAD_SET_OWNER_CLEAR => {
                if self.yes(EE_UNITS_DOPS_HAS_OWNER) {
                    self.result(EE_UNITS_DOPS_DEREFERENCE_OWNER, 0);
                }
                self.result(EE_UNITS_DOPS_CLEAR_OWNER, 0);
            }
            _ => return false,
        }
        true
    }

    /// The load sections (`EE_UNITS_UNIT_LOAD_SECTION_*`) this host reads; None for the others.
    fn load_section(&mut self, index: u32) -> Option<i32> {
        Some(match index {
            EE_UNITS_UNIT_LOAD_SECTION_WAYPOINTS => {
                self.result(EE_UNITS_DOPS_WAYPOINTS_CLEAR, 0);
                let (mut r, io) = self.read(EE_UNITS_PLAYER_SAVE_READ_LONG, EeUnitsUnitSaveIo::default());
                let count = io.i as c_long;
                let mut p = 0;
                while p < count && r == 0 {
                    r = self.result(EE_UNITS_DOPS_WAYPOINT_LOAD_LOCATION, 0);
                    if r == 0 {
                        let (rd, io) = self.read(EE_UNITS_PLAYER_SAVE_READ_FLOAT, EeUnitsUnitSaveIo::default());
                        r = rd;
                        self.put(EE_UNITS_DOPS_SET_WAYPOINT_DISTANCE, io);
                        if r == 0 {
                            let (rs, io) = self.read(EE_UNITS_PLAYER_SAVE_READ_FLOAT, EeUnitsUnitSaveIo::default());
                            r = rs;
                            self.put(EE_UNITS_DOPS_SET_WAYPOINT_START_DISTANCE, io);
                            if r == 0 {
                                self.result(EE_UNITS_DOPS_WAYPOINT_PUSH, 0);
                            }
                        }
                    }
                    p += 1;
                }
                r
            }
            EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES => {
                let (mut r, io) = self.read(EE_UNITS_PLAYER_SAVE_READ_ULONG, EeUnitsUnitSaveIo::default());
                let count = io.i as c_ulong;
                for goal in 0..count {
                    if r == 0 {
                        let queues = self.i(EE_UNITS_DOPS_GOAL_QUEUE_COUNT, 0) as u64;
                        r = if (goal as u64) < queues {
                            self.result(EE_UNITS_DOPS_GOAL_QUEUE_LOAD, goal as u32)
                        } else {
                            self.result(EE_UNITS_DOPS_SPARE_QUEUE_LOAD, 0)
                        };
                    }
                }
                if r == 0 {
                    r = self.result(EE_UNITS_DOPS_READ_CONVERSION_EFFECT, 0);
                    if r == 0 {
                        r = self.result(EE_UNITS_DOPS_READ_HEAL_EFFECT, 0);
                    }
                }
                r
            }
            _ => return None,
        })
    }
}

impl<H: SaveHost> SaveHost for DopSaveHost<'_, H> {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        let answer = match op {
            EE_UNITS_UNIT_SAVE_SECTION => self.save_section(index),
            EE_UNITS_UNIT_SAVE_PART => self.save_part(index),
            EE_UNITS_UNIT_LOAD_PART => self.load_part(index),
            EE_UNITS_UNIT_LOAD_SECTION => self.load_section(index),
            EE_UNITS_UNIT_LOAD_SET => self.load_set(index, io).then_some(0),
            _ => None,
        };
        match answer {
            Some(r) => r,
            None => self.inner.op(op, index, io),
        }
    }
}

#[cfg(test)]
#[path = "dop_save_tests.rs"]
mod tests;
