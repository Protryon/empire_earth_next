//! The goal objects' C++ accessors' bodies (ee/EEUnitGoal.h, EEUGRust.cpp, EEUGRustCombat.cpp): a new goal of a
//! kind made by its body's operation with integer arguments, one operation with integer arguments, one field of a
//! body's answer block, the location, attack and heal records by value and their one-field setters, the cast
//! calamity's and the unload goal's out-parameter reads, the path attempts' store. Each C++ accessor is one call.

use crate::units::goal_obj::{ee_units_goal_call, ee_units_goal_new, EeUnitsGoalBase};
use crate::units::location::EeUnitsLocationGoal;
use crate::units::rust_goal::{EeUnitsGoalIo, EE_UNITS_GOAL_NEW_LOCATION};
use core::ffi::c_long;
use ee_base::math::types::Ge3DPoint;

/// The pointers of a `std::vector<T*>` (its first two words are its begin and end: the world layout's check).
///
/// # Safety
/// `vector` points to a live `std::vector` of pointers.
pub unsafe fn vector_pointers(vector: *const core::ffi::c_void) -> Vec<u64> {
    let w = vector as *const usize;
    // SAFETY: the caller's contract
    unsafe {
        let (begin, end) = (*w, *w.add(1));
        let n = (end - begin) / std::mem::size_of::<usize>();
        (0..n).map(|i| *(begin as *const usize).add(i) as u64).collect()
    }
}

/// An answer block with `i[0..3]` set.
fn io3(i0: i64, i1: i64, i2: i64) -> EeUnitsGoalIo {
    let mut io = EeUnitsGoalIo::default();
    io.i[0] = i0;
    io.i[1] = i1;
    io.i[2] = i2;
    io
}

/// `EEUnitGoal::New(kind, arg, operation, a, b, ioArgs)`: `Make(kind, arg)`, then the operation.
///
/// # Safety
/// `io` null or a writable block; `a`, `b` the operation's live handles (or 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_new_io(
    kind: u8,
    arg: bool,
    op: i32,
    a: u64,
    b: u64,
    io: *mut EeUnitsGoalIo,
) -> *mut EeUnitsGoalBase {
    let goal = ee_units_goal_new(kind, arg);
    // SAFETY: the new goal; the caller's contract
    unsafe { ee_units_goal_call(op, goal, a, b, io) };
    goal
}

/// [`ee_units_goal_new_io`] with an answer block of `i[0..3]` (the New functions' `{{x, y, z}}` blocks).
///
/// # Safety
/// As [`ee_units_goal_new_io`].
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_goal_new_call(
    kind: u8,
    arg: bool,
    op: i32,
    a: u64,
    b: u64,
    i0: i64,
    i1: i64,
    i2: i64,
) -> *mut EeUnitsGoalBase {
    let mut io = io3(i0, i1, i2);
    // SAFETY: the caller's contract; a local block
    unsafe { ee_units_goal_new_io(kind, arg, op, a, b, &mut io) }
}

/// A new goal of a kind made by its operation over a point (`f[0..3]`; the attack-ground goal's
/// `EE_COMBAT_GOAL_NEW_GROUND`).
#[no_mangle]
pub extern "C" fn ee_units_goal_new_point(
    kind: u8,
    arg: bool,
    op: i32,
    x: f32,
    y: f32,
    z: f32,
) -> *mut EeUnitsGoalBase {
    let mut io = EeUnitsGoalIo::default();
    io.f[..3].copy_from_slice(&[x, y, z]);
    // SAFETY: a local block; no handles
    unsafe { ee_units_goal_new_io(kind, arg, op, 0, 0, &mut io) }
}

/// `EEUGRust::NewLoad(kind, container, units, arg)` after the C++ sorted the units by id and dropped the
/// duplicates: the goal made, its `EE_UNITS_GOAL_NEW_LOAD` with the container and the units' handles.
///
/// # Safety
/// `units` a live `std::vector<EEComplexUnit*>`; the handles live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_new_load(
    kind: u8,
    arg: bool,
    container: u64,
    units: *const core::ffi::c_void,
) -> *mut EeUnitsGoalBase {
    let goal = ee_units_goal_new(kind, arg);
    // SAFETY: the caller's contract
    let handles = unsafe { vector_pointers(units) };
    let mut io = EeUnitsGoalIo { list: handles.as_ptr(), list_len: handles.len(), ..Default::default() };
    // SAFETY: the new goal; the list lives through the call
    unsafe { ee_units_goal_call(crate::units::rust_goal::EE_UNITS_GOAL_NEW_LOAD, goal, container, 0, &mut io) };
    goal
}

/// `goal->Call(operation, a, b, {{i0, i1, i2}})`.
///
/// # Safety
/// `goal` a live goal; the handles live (or 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_call3(
    op: i32,
    goal: *const EeUnitsGoalBase,
    a: u64,
    b: u64,
    i0: i64,
    i1: i64,
    i2: i64,
) -> i64 {
    let mut io = io3(i0, i1, i2);
    // SAFETY: the caller's contract
    unsafe { ee_units_goal_call(op, goal, a, b, &mut io) }
}

/// `EEUnitGoal::Field(operation, index)`: the operation over an empty block, then its `i[index]`.
///
/// # Safety
/// `goal` a live goal.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_field(op: i32, goal: *const EeUnitsGoalBase, index: i32) -> i64 {
    let mut io = EeUnitsGoalIo::default();
    // SAFETY: the caller's contract
    unsafe { ee_units_goal_call(op, goal, 0, 0, &mut io) };
    io.i[index as usize]
}

/// `SetPathAttempts(value)`: the base's `mPathAttempts`, then the kind's operation.
///
/// # Safety
/// `goal` a live, writable goal.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_set_path_attempts(op: i32, goal: *mut EeUnitsGoalBase, value: c_long) {
    // SAFETY: the caller's contract
    unsafe {
        (*goal).path_attempts = value;
        ee_units_goal_call(op, goal, value as i64 as u64, 0, core::ptr::null_mut());
    }
}

/// `GetCalamityType(outTileX, outTileY, outTarget)`: the operation's answer (the target its result, the type and
/// tile in the block), each out pointer written when set.
///
/// # Safety
/// `goal` a live cast-calamity goal; the out pointers writable or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_calamity_type(
    op: i32,
    goal: *const EeUnitsGoalBase,
    out_x: *mut c_long,
    out_y: *mut c_long,
    out_target: *mut u64,
) -> c_long {
    let mut io = EeUnitsGoalIo::default();
    // SAFETY: the caller's contract
    let target = unsafe { ee_units_goal_call(op, goal, 0, 0, &mut io) } as u64;
    // SAFETY: as above
    unsafe {
        if let Some(x) = out_x.as_mut() {
            *x = io.i[1] as c_long;
        }
        if let Some(y) = out_y.as_mut() {
            *y = io.i[2] as c_long;
        }
        if let Some(t) = out_target.as_mut() {
            *t = target;
        }
    }
    io.i[0] as c_long
}

/// `GetUnloadTransport(outX, outY)`: the operation's transport (its result), the tile from the block into the out
/// pointers that are set.
///
/// # Safety
/// `goal` a live unload goal; the out pointers writable or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_unload_transport(
    op: i32,
    goal: *const EeUnitsGoalBase,
    out_x: *mut c_long,
    out_y: *mut c_long,
) -> u64 {
    let mut io = EeUnitsGoalIo::default();
    // SAFETY: the caller's contract
    let transport = unsafe { ee_units_goal_call(op, goal, 0, 0, &mut io) } as u64;
    // SAFETY: as above
    unsafe {
        if let Some(x) = out_x.as_mut() {
            *x = io.i[0] as c_long;
        }
        if let Some(y) = out_y.as_mut() {
            *y = io.i[1] as c_long;
        }
    }
    transport
}

/// The location goal's record by value (`EEUnitGoal::Location()`).
#[no_mangle]
pub extern "C" fn ee_units_location_goal_of(goal: u64) -> EeUnitsLocationGoal {
    crate::units::location::get(goal)
}

/// `SetUseGroupMovement(value)` (`which` 0) or `SetGroupPathState(value)` (1).
#[no_mangle]
pub extern "C" fn ee_units_location_goal_set_group(goal: u64, which: i32, value: u8) {
    let mut r = crate::units::location::get(goal);
    if which == 0 {
        r.group_movement = value != 0;
    } else {
        r.group_path_state = value;
    }
    crate::units::location::put(goal, r);
}

/// `SetLocation(point)`: the record's location.
#[no_mangle]
pub extern "C" fn ee_units_location_goal_set_point(goal: u64, x: f32, y: f32, z: f32) {
    let r = crate::units::location::get(goal);
    crate::units::location::put(goal, EeUnitsLocationGoal { location: [x, y, z], ..r });
}

/// The location goal's point by value (`EEUGRust::GetLocation`).
#[no_mangle]
pub extern "C" fn ee_units_location_goal_point(goal: u64) -> Ge3DPoint {
    let l = crate::units::location::get(goal).location;
    Ge3DPoint { x: l[0], y: l[1], z: l[2] }
}

/// `NewLocationGoal(point, arg, forced, face, yaw, auto, state)` (EEUGRust.cpp): a location goal made by its
/// `EE_UNITS_GOAL_NEW_LOCATION` (the flags in `i`, the point and yaw in `f`).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_units_goal_new_location(
    arg: bool,
    x: f32,
    y: f32,
    z: f32,
    forced_move: bool,
    should_face: bool,
    yaw: f32,
    auto_waypoint: bool,
    group_path_state: u8,
) -> *mut EeUnitsGoalBase {
    let mut io = EeUnitsGoalIo::default();
    io.i[..4].copy_from_slice(&[
        i64::from(forced_move),
        i64::from(should_face),
        i64::from(auto_waypoint),
        i64::from(group_path_state),
    ]);
    io.f[..4].copy_from_slice(&[x, y, z, yaw]);
    // SAFETY: a local block; no handles
    unsafe {
        ee_units_goal_new_io(crate::units::location::GOAL_LOCATION, arg, EE_UNITS_GOAL_NEW_LOCATION, 0, 0, &mut io)
    }
}

/// A point operation (`GetDestination`, `GetGoalLocation` seed the block with `point`, `GetCalamityTargetLocation`
/// with zeros): the operation, then the point from `f[0..3]`.
///
/// # Safety
/// `goal` a live goal, `point` three writable floats, `a` a live handle or 0.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_point(
    op: i32,
    goal: *const EeUnitsGoalBase,
    a: u64,
    point: *mut [f32; 3],
    seed: bool,
) {
    let mut io = EeUnitsGoalIo::default();
    // SAFETY: the caller's contract
    unsafe {
        if seed {
            io.f[..3].copy_from_slice(&*point);
        }
        ee_units_goal_call(op, goal, a, 0, &mut io);
        (*point).copy_from_slice(&io.f[..3]);
    }
}

/// `GetExploreTileCenter(outLocation)`: the explored tile's centre into x and y (z kept).
///
/// # Safety
/// `goal` a live explore goal, `point` three writable floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_explore_center(op: i32, goal: *const EeUnitsGoalBase, point: *mut [f32; 3]) {
    let mut io = EeUnitsGoalIo::default();
    // SAFETY: the caller's contract
    unsafe {
        ee_units_goal_call(op, goal, 0, 0, &mut io);
        (*point)[0] = io.i[0] as c_long as f32 + 0.5;
        (*point)[1] = io.i[1] as c_long as f32 + 0.5;
    }
}

/// The kind of a goal (0 for none).
///
/// # Safety
/// `goal` a live goal or null.
unsafe fn kind_of(goal: *const EeUnitsGoalBase) -> u8 {
    // SAFETY: the caller's contract
    unsafe { goal.as_ref() }.map_or(0, |g| g.kind)
}

/// `EELocationGoalPoint(goal)`: a location goal's point, a heal goal's location, else 0, 0, 0.
///
/// # Safety
/// `goal` a live goal or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_location_point_any(goal: *const EeUnitsGoalBase) -> Ge3DPoint {
    let h = goal as usize as u64;
    // SAFETY: the caller's contract
    match unsafe { kind_of(goal) } {
        crate::units::location::GOAL_LOCATION => ee_units_location_goal_point(h),
        crate::combat::goal_heal::GOAL_HEAL_UNIT => crate::combat::goal_heal::ee_combat_heal_location(h),
        _ => Ge3DPoint::default(),
    }
}

/// `EESetLocationGoalPoint(goal, point)`: a location goal's point, a heal goal's location (another goal untouched).
///
/// # Safety
/// `goal` a live goal or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_set_location_point_any(goal: *const EeUnitsGoalBase, x: f32, y: f32, z: f32) {
    let h = goal as usize as u64;
    // SAFETY: the caller's contract
    match unsafe { kind_of(goal) } {
        crate::units::location::GOAL_LOCATION => ee_units_location_goal_set_point(h, x, y, z),
        crate::combat::goal_heal::GOAL_HEAL_UNIT => {
            crate::combat::goal_heal::ee_combat_heal_set_location(h, x.to_bits(), y.to_bits(), z.to_bits())
        }
        _ => {}
    }
}

/// `EELocationGoalYaw(goal)`: a location goal's yaw, else 0 (the heal goal's was made 0 and never changed).
///
/// # Safety
/// `goal` a live goal or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_location_yaw_any(goal: *const EeUnitsGoalBase) -> f32 {
    // SAFETY: the caller's contract
    if unsafe { kind_of(goal) } == crate::units::location::GOAL_LOCATION {
        return crate::units::location::get(goal as usize as u64).yaw;
    }
    0.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_location_setters_change_one_field() {
        let g = 0x9a10;
        crate::units::location::put(
            g,
            EeUnitsLocationGoal { location: [1.0, 2.0, 3.0], yaw: 0.5, forced_move: true, ..Default::default() },
        );
        ee_units_location_goal_set_group(g, 0, 1);
        ee_units_location_goal_set_group(g, 1, 7);
        ee_units_location_goal_set_point(g, 4.0, 5.0, 6.0);
        let r = ee_units_location_goal_of(g);
        assert_eq!(r.location, [4.0, 5.0, 6.0]);
        assert_eq!((r.yaw, r.forced_move, r.group_movement, r.group_path_state), (0.5, true, true, 7));
        ee_units_location_goal_set_group(g, 0, 0);
        assert!(!ee_units_location_goal_of(g).group_movement);
    }

    #[test]
    fn the_location_point_of_any_goal_reads_its_kind() {
        let mut loc = EeUnitsGoalBase { kind: crate::units::location::GOAL_LOCATION, ..Default::default() };
        let g = &mut loc as *mut EeUnitsGoalBase;
        let h = g as usize as u64;
        crate::units::location::put(h, EeUnitsLocationGoal { yaw: 0.25, ..Default::default() });
        // SAFETY: a local goal head (no body asked)
        unsafe {
            ee_units_goal_set_location_point_any(g, 1.0, 2.0, 3.0);
            assert_eq!(ee_units_goal_location_point_any(g), Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
            assert_eq!(ee_units_goal_location_yaw_any(g), 0.25);
            assert_eq!(ee_units_goal_location_point_any(core::ptr::null()), Ge3DPoint::default());
            assert_eq!(ee_units_goal_location_yaw_any(core::ptr::null()), 0.0);
        }
        let other = EeUnitsGoalBase { kind: 9, ..Default::default() };
        // SAFETY: as above
        unsafe {
            ee_units_goal_set_location_point_any(&other, 7.0, 7.0, 7.0);
            assert_eq!(ee_units_goal_location_point_any(&other), Ge3DPoint::default());
        }
    }

    #[test]
    fn the_blocks_carry_three_integers() {
        let io = io3(4, -5, 6);
        assert_eq!(&io.i[..4], &[4, -5, 6, 0]);
        assert_eq!(io.f, [0.0; 8]);
    }
}
