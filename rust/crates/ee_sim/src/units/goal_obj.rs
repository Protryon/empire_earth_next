//! The goal objects whole in Rust (units behaviour lane, round 3): a goal is a Rust allocation whose head is the
//! base goal's fields ([`EeUnitsGoalBase`], `EEUnitGoal`'s members in their order: ee/EEUnitGoal.h asserts the
//! offsets), its kind's state the kinds' tables by the goal's address (rust_goal.rs and the combat kinds). Rust makes
//! them ([`goal_new`]: `EEUnitGoal::EEUnitGoal` 0x23c375, the C++ `Make`), copies them (the kinds' Clone through
//! [`goal_new`]), runs their bodies with the C++ world host registered once ([`ee_units_goal_call`]) and deletes them
//! ([`goal_delete`]: the kind's destroy, then the allocation; the C++ `delete` is the same two steps). The goals'
//! counter (`sGoalCounter`, 0x564480: the next goal's id, saved by the player) is Rust's.
//!
//! The base fields' host operations (`BASE_FIELDS`, `GOAL_ARG`, `SET_ACTIVE`, `RESET_PATH_ATTEMPTS`, `NEW_GOAL`)
//! are answered here: [`ee_units_rust_goal`](crate::units::rust_goal::ee_units_rust_goal) wraps every host table in
//! [`base_host_op`], which forwards the rest.

// `c_long` and `c_ulong` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

use crate::units::gather::GOAL_GATHER;
use crate::units::goals;
use crate::units::location::GOAL_LOCATION;
use crate::units::rust_goal::{
    ee_units_rust_goal, EeUnitsGoalHost, EeUnitsGoalIo, EE_UNITS_GOAL_DESTROY, EE_UNITS_GOAL_HOST_BASE_FIELDS,
    EE_UNITS_GOAL_HOST_GOAL_ARG, EE_UNITS_GOAL_HOST_LOCATION_GOAL, EE_UNITS_GOAL_HOST_NEW_GATHER,
    EE_UNITS_GOAL_HOST_NEW_GOAL, EE_UNITS_GOAL_HOST_RESET_ACTIVE_PATH, EE_UNITS_GOAL_HOST_RESET_PATH_ATTEMPTS,
    EE_UNITS_GOAL_HOST_SET_ACTIVE, EE_UNITS_GOAL_HOST_SET_GOAL, EE_UNITS_GOAL_NEW_GATHER, EE_UNITS_GOAL_NEW_LOCATION,
    EE_UNITS_GOAL_SET_PATH_ATTEMPTS,
};

/// `EEUnitGoal`'s members (the original's +0x04..+0x1d after its vptr), the head of every goal object.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsGoalBase {
    pub is_active: bool,
    pub kind: u8,
    /// `mAction` (the action the goal starts: `kActionIdle` when made)
    pub action: c_ulong,
    pub start_time: c_ulong,
    pub creation_time: c_ulong,
    /// `mID`: the goal's number (`sGoalCounter`'s value when made)
    pub id: c_ulong,
    pub path_attempts: c_long,
    pub b1c: bool,
    pub b1d: bool,
}

/// `kActionIdle`
const ACTION_IDLE: c_ulong = 5000;

/// `sGoalCounter`: the last goal id drawn.
static GOAL_COUNTER: Mutex<c_ulong> = Mutex::new(0);

fn counter<R>(f: impl FnOnce(&mut c_ulong) -> R) -> R {
    f(&mut GOAL_COUNTER.lock().unwrap_or_else(|e| e.into_inner()))
}

/// The game time a new goal is stamped with (`gPhysics->mGameTime`; 0 without a world).
fn game_time() -> c_ulong {
    #[cfg(not(test))]
    {
        // SAFETY: goals are made by the simulation (or the editor) holding the world
        let t = unsafe { crate::world::WorldRead::begin() };
        crate::world::WorldView::global(&t).map_or(0, |w| w.game_time() as c_long as c_ulong)
    }
    #[cfg(test)]
    {
        TEST_TIME.with(|t| t.get())
    }
}

#[cfg(test)]
thread_local! {
    static TEST_TIME: std::cell::Cell<c_ulong> = const { std::cell::Cell::new(0) };
}

/// `EEUnitGoal(inKind, inArg)` (0x23c375): an active, idle goal of `kind` with the next id, stamped with the game
/// time, its kind's state not made yet (the kinds' `NEW_*` operations make it). Its handle.
pub fn goal_new(kind: u8, arg: bool) -> u64 {
    let id = counter(|c| {
        *c = c.wrapping_add(1);
        *c
    });
    let goal = Box::new(EeUnitsGoalBase {
        is_active: true,
        kind,
        action: ACTION_IDLE,
        start_time: 0,
        creation_time: game_time(),
        id,
        path_attempts: 0,
        b1c: arg,
        b1d: false,
    });
    Box::into_raw(goal) as usize as u64
}

/// The base fields of the goal `goal` (a live goal object).
///
/// # Safety
/// `goal` is a live goal's handle and no other reference to it is in use.
pub unsafe fn base<'a>(goal: u64) -> &'a mut EeUnitsGoalBase {
    // SAFETY: the caller's contract
    unsafe { &mut *(goal as usize as *mut EeUnitsGoalBase) }
}

/// The world host the context-free calls use (EEUGRust.cpp's operation callback, registered at start-up).
type EeUnitsGoalHostOp = unsafe extern "C" fn(*mut c_void, i32, u64, u64, *mut EeUnitsGoalIo) -> i64;
static REGISTERED: Mutex<Option<EeUnitsGoalHostOp>> = Mutex::new(None);

fn registered() -> Option<EeUnitsGoalHostOp> {
    *REGISTERED.lock().unwrap_or_else(|e| e.into_inner())
}

/// The base fields' host operations, then `inner`'s (`context` is the wrapped table).
///
/// # Safety
/// `context` points to a valid table for the call, `io` to a valid block; a goal handle names a live goal.
pub unsafe extern "C" fn base_host_op(context: *mut c_void, op: i32, a: u64, b: u64, io: *mut EeUnitsGoalIo) -> i64 {
    // SAFETY: the caller's contract
    let (inner, io) = unsafe { (&*(context as *const EeUnitsGoalHost), &mut *io) };
    match op {
        EE_UNITS_GOAL_HOST_NEW_GOAL => goal_new(a as u8, b != 0) as i64,
        EE_UNITS_GOAL_HOST_GOAL_ARG => i64::from(unsafe { base(a) }.b1c),
        EE_UNITS_GOAL_HOST_SET_ACTIVE => {
            unsafe { base(a) }.is_active = io.i[0] != 0;
            0
        }
        EE_UNITS_GOAL_HOST_BASE_FIELDS => {
            // SAFETY: a live goal
            let g = unsafe { base(a) };
            if b == 0 {
                io.i[..6].copy_from_slice(&[
                    g.action as i64,
                    i64::from(g.b1c),
                    g.id as i64,
                    i64::from(g.b1d),
                    g.start_time as i64,
                    g.creation_time as i64,
                ]);
            } else {
                g.action = io.i[0] as c_ulong;
                g.b1c = io.i[1] != 0;
                g.id = io.i[2] as c_ulong;
                g.b1d = io.i[3] != 0;
                g.start_time = io.i[4] as c_ulong;
                g.creation_time = io.i[5] as c_ulong;
            }
            0
        }
        EE_UNITS_GOAL_HOST_RESET_PATH_ATTEMPTS => {
            // SetPathAttempts(0): the base goal's count, then the kind's
            let kind = {
                // SAFETY: a live goal
                let g = unsafe { base(a) };
                g.path_attempts = 0;
                g.kind
            };
            let mut args = EeUnitsGoalIo::default();
            // SAFETY: the same table, wrapped again by the dispatch
            unsafe { ee_units_rust_goal(EE_UNITS_GOAL_SET_PATH_ATTEMPTS, kind, a, 0, 0, &mut args, inner) };
            0
        }
        // the goal calls of a unit (goals.rs): `SetGoal(b, i[0], i[1])`, a new gather goal on resource `a` and
        // storage `b`, a forced move to the point (`SetGoal(NewLocation(&f, false, true, false, 0, false, 0), 0, 0)`),
        // the active goal's `SetPathAttempts(0)`
        EE_UNITS_GOAL_HOST_SET_GOAL => {
            let index = io.i[1] as c_long;
            if index >= 0 {
                goals::set_goal(a, b, io.i[0] != 0, index as usize, &mut goals::Goals);
            }
            0
        }
        EE_UNITS_GOAL_HOST_NEW_GATHER => {
            let goal = goal_new(GOAL_GATHER, io.i[0] != 0);
            // SAFETY: the goal just made
            unsafe { call(EE_UNITS_GOAL_NEW_GATHER, goal, a, b, std::ptr::null_mut()) };
            goal as i64
        }
        EE_UNITS_GOAL_HOST_LOCATION_GOAL => {
            let goal = goal_new(GOAL_LOCATION, false);
            let mut args = EeUnitsGoalIo { i: [1, 0, 0, 0, 0, 0, 0, 0], ..Default::default() };
            args.f[..3].copy_from_slice(&io.f[..3]);
            // SAFETY: the goal just made
            unsafe { call(EE_UNITS_GOAL_NEW_LOCATION, goal, 0, 0, &mut args) };
            goals::set_goal(a, goal, false, 0, &mut goals::Goals);
            0
        }
        EE_UNITS_GOAL_HOST_RESET_ACTIVE_PATH => {
            // SAFETY: the caller holds the world; `a` is a live complex unit
            let t = unsafe { crate::world::WorldRead::begin() };
            let active = unsafe { crate::world::UnitView::of(&t, a) }.map_or(0, |u| u.active_goal_handle());
            if active != 0 {
                // SAFETY: the unit's live active goal
                unsafe {
                    base(active).path_attempts = 0;
                    call(EE_UNITS_GOAL_SET_PATH_ATTEMPTS, active, 0, 0, std::ptr::null_mut());
                }
            }
            0
        }
        // the compound operations in their statements (goal_split.rs), the rest one statement of the wrapped table
        _ => {
            let mut h = crate::units::goal_split::FfiSplit(inner);
            crate::units::goal_split::split(&mut h, op, a, b, io)
                .unwrap_or_else(|| crate::units::goal_split::GoalSplitHost::raw(&mut h, op, a, b, io))
        }
    }
}

/// One body of the goal `goal` with the registered host and no file or text context (0 when no host is
/// registered).
///
/// # Safety
/// `goal` is a live goal's handle, `io` null or a valid block.
pub unsafe fn call(op: i32, goal: u64, a: u64, b: u64, io: *mut EeUnitsGoalIo) -> i64 {
    let Some(f) = registered() else { return 0 };
    let host = EeUnitsGoalHost { context: std::ptr::null_mut(), op: f };
    let mut local = EeUnitsGoalIo::default();
    let io = if io.is_null() { &mut local as *mut EeUnitsGoalIo } else { io };
    // SAFETY: the caller's contract; the kind read from the live goal
    unsafe { ee_units_rust_goal(op, base(goal).kind, goal, a, b, io, &host) }
}

/// Deletes the goal `goal`: its kind's destroy (the references it holds dropped), then the object.
///
/// # Safety
/// `goal` is a live goal's handle, used by no one afterwards.
pub unsafe fn goal_delete(goal: u64) {
    if goal == 0 {
        return;
    }
    // SAFETY: the caller's contract
    unsafe {
        call(EE_UNITS_GOAL_DESTROY, goal, 0, 0, std::ptr::null_mut());
        dealloc(goal);
    }
}

/// Frees the goal object `goal` without its destroy.
///
/// # Safety
/// `goal` is a goal object made by [`goal_new`], used by no one afterwards.
pub unsafe fn dealloc(goal: u64) {
    if goal != 0 {
        // SAFETY: the caller's contract: a Box made by goal_new
        drop(unsafe { Box::from_raw(goal as usize as *mut EeUnitsGoalBase) });
    }
}

/// `new EEUnitGoal(inKind, inArg)` (`EEUnitGoal::Make`): see [`goal_new`].
#[no_mangle]
pub extern "C" fn ee_units_goal_new(kind: u8, arg: bool) -> *mut EeUnitsGoalBase {
    goal_new(kind, arg) as usize as *mut EeUnitsGoalBase
}

/// `EEUnitGoal::CreateGoal`'s id hand-back: the counter one lower (a loaded goal's id is its record's).
#[no_mangle]
pub extern "C" fn ee_units_goal_counter_back() -> c_ulong {
    counter(|c| {
        *c = c.wrapping_sub(1);
        *c
    })
}

/// `sGoalCounter` (the player's save).
#[no_mangle]
pub extern "C" fn ee_units_goal_counter() -> c_ulong {
    counter(|c| *c)
}

/// `sGoalCounter = value` (the player's load).
#[no_mangle]
pub extern "C" fn ee_units_goal_counter_set(value: c_ulong) {
    counter(|c| *c = value);
}

/// The C++ `operator delete` of a goal (after its destructor's destroy): the object freed.
///
/// # Safety
/// `goal` is a goal object made by [`ee_units_goal_new`] or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_dealloc(goal: *mut EeUnitsGoalBase) {
    // SAFETY: the caller's contract
    unsafe { dealloc(goal as usize as u64) }
}

/// Registers the world host the context-free calls use (EEUGRust.cpp's operation callback).
#[no_mangle]
pub extern "C" fn ee_units_goal_register_host(op: EeUnitsGoalHostOp) {
    *REGISTERED.lock().unwrap_or_else(|e| e.into_inner()) = Some(op);
}

/// One body (`EE_UNITS_GOAL_*`) of `goal` with the registered host (the goal's accessors and virtuals that write no
/// file or text): its result.
///
/// # Safety
/// `goal` is a live goal object, `io` null or a valid block.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_call(
    op: i32,
    goal: *const EeUnitsGoalBase,
    a: u64,
    b: u64,
    io: *mut EeUnitsGoalIo,
) -> i64 {
    // SAFETY: the caller's contract
    unsafe { call(op, goal as usize as u64, a, b, io) }
}

/// `delete inGoal` from Rust's side (`EEUnitGoal::Delete`): see [`goal_delete`].
///
/// # Safety
/// `goal` is a live goal object or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_delete(goal: *mut EeUnitsGoalBase) {
    // SAFETY: the caller's contract
    unsafe { goal_delete(goal as usize as u64) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_new_goal_is_active_idle_and_numbered() {
        TEST_TIME.with(|t| t.set(1234));
        let first = goal_new(9, true);
        let second = goal_new(1, false);
        // SAFETY: the goals just made
        let (f, s) = unsafe { (*base(first), *base(second)) };
        assert!(f.is_active && f.b1c && !f.b1d && f.kind == 9 && f.action == ACTION_IDLE && f.path_attempts == 0);
        assert_eq!(f.creation_time, 1234);
        assert_eq!(s.id, f.id.wrapping_add(1));
        assert!(!s.b1c && s.kind == 1);
        // (the one test that makes goals: the counter is the process's)
        assert_eq!(ee_units_goal_counter(), s.id);
        assert_eq!(ee_units_goal_counter_back(), f.id);
        ee_units_goal_counter_set(s.id);
        // SAFETY: as above
        unsafe {
            dealloc(first);
            dealloc(second);
        }
        the_base_operations_are_answered_and_the_rest_forwarded();
    }

    unsafe extern "C" fn inner_op(context: *mut c_void, op: i32, a: u64, _b: u64, _io: *mut EeUnitsGoalIo) -> i64 {
        // SAFETY: the test's counter
        unsafe { *(context as *mut i64) += 1 };
        i64::from(op) * 1000 + a as i64
    }

    fn the_base_operations_are_answered_and_the_rest_forwarded() {
        let goal = goal_new(1, true);
        let mut calls = 0i64;
        let inner = EeUnitsGoalHost { context: &mut calls as *mut i64 as *mut c_void, op: inner_op };
        let ctx = &inner as *const EeUnitsGoalHost as *mut c_void;
        let mut io = EeUnitsGoalIo::default();
        // SAFETY: the goal and table just made
        unsafe {
            assert_eq!(base_host_op(ctx, EE_UNITS_GOAL_HOST_GOAL_ARG, goal, 0, &mut io), 1);
            io.i[0] = 0;
            base_host_op(ctx, EE_UNITS_GOAL_HOST_SET_ACTIVE, goal, 0, &mut io);
            assert!(!base(goal).is_active);
            io.i[..6].copy_from_slice(&[7, 0, 99, 1, 5, 6]);
            base_host_op(ctx, EE_UNITS_GOAL_HOST_BASE_FIELDS, goal, 1, &mut io);
            let g = *base(goal);
            assert_eq!((g.action, g.b1c, g.id, g.b1d, g.start_time, g.creation_time), (7, false, 99, true, 5, 6));
            io = EeUnitsGoalIo::default();
            base_host_op(ctx, EE_UNITS_GOAL_HOST_BASE_FIELDS, goal, 0, &mut io);
            assert_eq!(&io.i[..6], &[7, 0, 99, 1, 5, 6]);
            assert_eq!(base_host_op(ctx, 3, 42, 0, &mut io), 3042);
            dealloc(goal);
        }
        assert_eq!(calls, 1);
    }
}
