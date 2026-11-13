//! The location goal whole in Rust (RUST_PLAN R5b.4, the twenty-first round; was `EEUGLocation`
//! 0x227621..0x227de8): an `EEUGRust` of kind `kGoalLocation` (1), its state one record by goal object. The location
//! goal is every move order, the group paths' slots, forced moves, facing on arrival and the automatic waypoints.
//!
//! The original's members are the record's fields (`mLocation`, `mYaw`, `mIsForcedMove`, `mShouldFace`,
//! `mIsAutoWaypoint`, `mUseGroupMovement`, `mGroupPathState`). Its callers read and write them through C++ accessors
//! over [`ee_units_location_goal_get`] / [`ee_units_location_goal_set`] (no host call: the record is copied out and
//! back). The bodies: the copy, the out-of-synch dump and checksum, the debug texts, the goal location, the start, the
//! save and load by version, the facing triggers and `FaceYaw` (the transformation's arithmetic over ee_ge, the
//! C++'s `Apply`, `UMArcTan2` and `SetOrientationYPR` forward there). The rest is the base goal's (`GetDestination`
//! told to the group, `Step`, `SetOwner`, `Abandon`, `ReplaceUnit`, `Suits` true, three path attempts).
//!
//! Reentrancy: no lock is held across a host call; `Start` and `FaceYaw` read the record before their host calls
//! and write nothing.

use std::collections::BTreeMap;
use std::f32::consts::{PI, TAU};
use std::sync::Mutex;

use ee_ge::transform::RotationOrder;
use ee_ge::types::{GeTransformation, GeVector};
use ee_ge::um::um_arctan2;

use crate::rust_goal::{base_condition, EeUnitsGoalIo, GoalHost, TRIGGER_UNIT_FACING_NOT_REQUIRED};

/// `kGoalLocation`
pub const GOAL_LOCATION: u8 = 1;
/// `kActionPrepareToMove`, `kActionIdle`, `kActionTurnToUnitFacing`
pub const ACTION_PREPARE_TO_MOVE: i64 = 8000;
pub const ACTION_IDLE: i64 = 5000;
pub const ACTION_TURN_TO_UNIT_FACING: i64 = 9006;
/// `kTriggerCompletedUnitFacing`
pub const TRIGGER_COMPLETED_UNIT_FACING: i64 = 9015;
/// `kUnitCategoryAirplane`, `kUnitCategoryPlanetSpaceship`: the start's aircraft
pub const CATEGORY_AIRPLANE: i64 = 3;
pub const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;

/// `kOneDegree` (0x447898)
const ONE_DEGREE: f32 = 0.017_453_292;

/// A location goal's state (`EEUGLocation`'s members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsLocationGoal {
    /// `mLocation`
    pub location: [f32; 3],
    /// `mYaw`: the heading to face on arrival
    pub yaw: f32,
    /// `mIsForcedMove`, `mShouldFace`, `mIsAutoWaypoint`, `mUseGroupMovement`
    pub forced_move: bool,
    pub should_face: bool,
    pub auto_waypoint: bool,
    pub group_movement: bool,
    /// `mGroupPathState`
    pub group_path_state: u8,
}

static LOCATIONS: Mutex<BTreeMap<u64, EeUnitsLocationGoal>> = Mutex::new(BTreeMap::new());

/// The goal's record (zeros for none).
pub fn get(goal: u64) -> EeUnitsLocationGoal {
    LOCATIONS.lock().unwrap_or_else(|e| e.into_inner()).get(&goal).copied().unwrap_or_default()
}

fn put(goal: u64, x: EeUnitsLocationGoal) {
    LOCATIONS.lock().unwrap_or_else(|e| e.into_inner()).insert(goal, x);
}

fn remove(goal: u64) {
    LOCATIONS.lock().unwrap_or_else(|e| e.into_inner()).remove(&goal);
}

/// The location goal's host calls beyond the common ones.
pub trait LocationHost: GoalHost {
    /// the unit's class's `mCategory`
    fn unit_category(&mut self, unit: u64) -> i64;
    /// the unit's `mPhysicalModel.GetLocation()`
    fn unit_transformation(&mut self, unit: u64) -> GeTransformation;
    /// the unit's `SetTransformation(t, true)`
    fn set_transformation(&mut self, unit: u64, t: &GeTransformation);
    /// the unit's `SetTargetSearchTimer(value)`
    fn set_target_search_timer(&mut self, unit: u64, value: i64);
    /// `mGoalQueues[0].mGoals.size()`
    fn goal_queue_len(&mut self, unit: u64) -> i64;
    /// the file's `float` calls
    fn write_f32(&mut self, value: f32) -> i32;
    fn read_f32(&mut self, value: &mut f32) -> i32;
}

/// The four constructors: the location, the forced-move, facing (with its yaw), auto-waypoint flags and the group path
/// state; no group movement (the `(bool, ...)` load constructor: 0, 0, 0, yaw 0, no facing).
pub fn create(goal: u64, x: EeUnitsLocationGoal) {
    put(goal, EeUnitsLocationGoal { group_movement: false, ..x });
}

/// `Clone`: a new goal with the location, the settings and the group movement flag (`b1c` the base goal's flag).
pub fn clone_goal(goal: u64, b1c: bool, host: &mut dyn LocationHost) -> u64 {
    let x = get(goal);
    let copy = host.new_goal(GOAL_LOCATION, b1c);
    put(copy, x);
    copy
}

/// `DumpOutOfSynch`: the location, yaw, flags and group path state (the base goal's dump is empty).
pub fn dump(goal: u64, host: &mut dyn LocationHost) {
    let x = get(goal);
    let p = "OUT OF SYNCH DUMP:  EEUGLocation - ";
    let [lx, ly, lz] = x.location.map(f64::from);
    host.append(&format!("{p}Location is ({lx:6.6}, {ly:6.6}, {lz:6.6})\n"));
    host.append(&format!("{p}Yaw is {:6.6}\n", f64::from(x.yaw)));
    host.append(&format!("{p}Forced Move Flag is {}\n", i32::from(x.forced_move)));
    host.append(&format!("{p}Should Face Flag is {}\n", i32::from(x.should_face)));
    host.append(&format!("{p}Is Auto Waypoint Flag is {}\n", i32::from(x.auto_waypoint)));
    host.append(&format!("{p}Use Group Movement Flag is {}\n", i32::from(x.group_movement)));
    host.append(&format!("{p}Group Path State is {}\n", x.group_path_state));
}

/// `GetOutOfSynchChecksum`'s own part: the raw bits of the location and yaw, the flags and the group path state.
pub fn checksum(goal: u64) -> i64 {
    let x = get(goal);
    let bits = |v: f32| v.to_bits() as i32;
    let mut sum = bits(x.location[0]);
    sum = sum.wrapping_add(bits(x.location[1]));
    sum = sum.wrapping_add(bits(x.location[2]));
    let rest = bits(x.yaw)
        .wrapping_add(i32::from(x.forced_move))
        .wrapping_add(i32::from(x.should_face))
        .wrapping_add(i32::from(x.auto_waypoint))
        .wrapping_add(i32::from(x.group_movement))
        .wrapping_add(i32::from(x.group_path_state));
    i64::from(sum.wrapping_add(rest))
}

/// `GetDebugLine`: "loc", the location and the auto-waypoint flag.
pub fn debug_line(goal: u64, host: &mut dyn LocationHost) {
    let x = get(goal);
    let [lx, ly, lz] = x.location.map(f64::from);
    host.append(&format!("loc {lx:.2},{ly:.2},{lz:.2}: auto({})", i32::from(x.auto_waypoint)));
}

/// `GetDebugText`: the location, auto-waypoint flag, yaw, attack move (not forced) and facing flag.
pub fn debug_text(goal: u64, host: &mut dyn LocationHost) {
    let x = get(goal);
    let [lx, ly, lz] = x.location.map(f64::from);
    let tf = |b: bool| if b { "true" } else { "false" };
    host.append(&format!(
        "Location: {lx:.2},{ly:.2},{lz:.2}\nAutoWaypoint:{}\nYaw:{:.4}\nAttackMove:{}\nShouldFace:{}\n",
        tf(x.auto_waypoint),
        f64::from(x.yaw),
        tf(!x.forced_move),
        tf(x.should_face)
    ));
}

/// `FaceYaw`: whether the unit already faces the goal's yaw (within one degree); if so, it is turned to that yaw
/// exactly (its transformation's orientation set, the position kept, its tiles updated).
pub fn face_yaw(goal: u64, unit: u64, host: &mut dyn LocationHost) -> bool {
    let yaw = get(goal).yaw;
    face_yaw_at(yaw, unit, host)
}

fn face_yaw_at(yaw: f32, unit: u64, host: &mut dyn LocationHost) -> bool {
    let t = host.unit_transformation(unit);
    let d = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let heading = um_arctan2(-d.x, d.y);
    let mut delta = heading - yaw;
    // the original's -3.1415927f, 6.2831855f: f32's pi and tau
    if delta < -PI {
        delta += TAU;
    } else if delta > PI {
        delta -= TAU;
    }
    let faces = (-ONE_DEGREE..=ONE_DEGREE).contains(&delta);
    if faces {
        let mut turned = t;
        turned.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        host.set_transformation(unit, &turned);
    }
    faces
}

/// `Start`: a unit that is no aircraft and stands on the location turns to face (or idles); otherwise it prepares to
/// move, an unforced move resetting its target search timer first. Each start also tells the class (`ActionChanged`).
pub fn start(goal: u64, unit: u64, host: &mut dyn LocationHost) {
    let x = get(goal);
    let category = host.unit_category(unit);
    if category != CATEGORY_AIRPLANE && category != CATEGORY_PLANET_SPACESHIP {
        let t = host.unit_transformation(unit);
        let position = [t.x(), t.y(), t.z()];
        if position[0] == x.location[0] && position[1] == x.location[1] && position[2] == x.location[2] {
            host.start_action(unit, if x.should_face { ACTION_TURN_TO_UNIT_FACING } else { ACTION_IDLE });
            return;
        }
        if !x.forced_move {
            host.set_target_search_timer(unit, 0);
        }
    }
    host.start_action(unit, ACTION_PREPARE_TO_MOVE);
}

/// `EvaluateCondition`: facing done (`FaceYaw`), facing not required (no facing, or more goals queued after this
/// one); any other event the base goal's.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn LocationHost) -> bool {
    match event {
        TRIGGER_COMPLETED_UNIT_FACING => face_yaw(goal, unit, host),
        TRIGGER_UNIT_FACING_NOT_REQUIRED => !get(goal).should_face || host.goal_queue_len(unit) > 1,
        _ => base_condition(event),
    }
}

fn save_point(p: [f32; 3], host: &mut dyn LocationHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    for v in p {
        let result = host.write_f32(v);
        if result != 0 {
            return result;
        }
    }
    0
}

fn load_point(p: &mut [f32; 3], host: &mut dyn LocationHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    for v in p.iter_mut() {
        let result = host.read_f32(v);
        if result != 0 {
            return result;
        }
    }
    0
}

/// `Save`'s part after the base goal's: the location, forced-move flag, yaw, facing, auto-waypoint and group
/// movement flags; the first failing status.
pub fn save(goal: u64, host: &mut dyn LocationHost) -> i32 {
    let x = get(goal);
    let mut r = save_point(x.location, host);
    if r == 0 {
        r = host.write_bool(x.forced_move);
    }
    if r == 0 {
        r = host.write_f32(x.yaw);
    }
    if r == 0 {
        r = host.write_bool(x.should_face);
    }
    if r == 0 {
        r = host.write_bool(x.auto_waypoint);
    }
    if r == 0 {
        r = host.write_bool(x.group_movement);
    }
    r
}

/// `Load`'s part after the base goal's: the location and the settings the save's version has (forced move from
/// 0.15, yaw and facing from 0.26, auto waypoint from 0.135, group movement from 0.173); a field not read keeps its
/// value; the first failing status.
pub fn load(goal: u64, major: u32, minor: u32, host: &mut dyn LocationHost) -> i32 {
    let mut x = get(goal);
    let old = major == 0;
    // the fields each version has, in the file's order
    let forced = !(old && minor <= 14);
    let facing = !(old && minor <= 25);
    let auto = !(old && minor < 135);
    let group = !(old && minor < 173);
    let mut r = load_point(&mut x.location, host);
    if forced && r == 0 {
        r = host.read_bool(&mut x.forced_move);
    }
    if facing && r == 0 {
        r = host.read_f32(&mut x.yaw);
    }
    if facing && r == 0 {
        r = host.read_bool(&mut x.should_face);
    }
    if auto && r == 0 {
        r = host.read_bool(&mut x.auto_waypoint);
    }
    if group && r == 0 {
        r = host.read_bool(&mut x.group_movement);
    }
    put(goal, x);
    r
}

/// The location goal's bodies (`ee_units_rust_goal`'s dispatch for `kGoalLocation`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn LocationHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_LOCATION => create(
            goal,
            EeUnitsLocationGoal {
                location: [io.f[0], io.f[1], io.f[2]],
                yaw: io.f[3],
                forced_move: io.i[0] != 0,
                should_face: io.i[1] != 0,
                auto_waypoint: io.i[2] != 0,
                group_movement: false,
                group_path_state: io.i[3] as u8,
            },
        ),
        EE_UNITS_GOAL_DESTROY => remove(goal),
        EE_UNITS_GOAL_CLONE => return clone_goal(goal, b != 0, host) as i64,
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION => io.f[..3].copy_from_slice(&get(goal).location),
        EE_UNITS_GOAL_DESTINATION => {
            // the base goal's: the goal location, told to the unit's active group
            let location = get(goal).location;
            io.f[..3].copy_from_slice(&location);
            host.tell_group(a, location);
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_PREPARE_TO_MOVE,
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_FACE_YAW => return i64::from(face_yaw(goal, a, host)),
        // the base goal's: RepairPointers 0 (no pointers), Step, SetOwner, Abandon, AddCreatedUnit, Work,
        // PathTaken, ReplaceUnit do nothing (Suits and the path attempts are the dispatch's defaults)
        _ => {}
    }
    0
}

// ---- the C ABI: the accessors' record (no host call)

/// The goal's record into `out` (zeros for a goal that has none).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_location_goal_get(goal: u64, out: *mut EeUnitsLocationGoal) {
    // SAFETY: the caller's contract
    unsafe { *out = get(goal) };
}

/// The goal's record replaced by `*record` (the accessors' setters: the location, the group movement flag, the
/// group path state).
///
/// # Safety
/// `record` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_location_goal_set(goal: u64, record: *const EeUnitsLocationGoal) {
    // SAFETY: the caller's contract
    put(goal, unsafe { *record });
}

#[cfg(test)]
#[path = "location_tests.rs"]
mod tests;
