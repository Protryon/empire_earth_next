//! The scenario manager's members and stream operations that were C++ statement sequences
//! (Scenario Manager/EEScenarioManager.cpp; the scenario editor lane's round 4): the destructor, `AddV134Effect`,
//! `SetTimedEffectHasRun`, `MoveCameraToStart`, `ObjectChanged`, the round's capture, and the save's element loads
//! (`EESMLoadElement` and the dropped "object 0" conditions) and dialog message vector (`EESMLoadVector` /
//! `EESMSaveVector`). Composed here of single C++ statements, in the C++'s order: the round host's
//! (`EE_SCENARIO_ROUND_*` from 76, `RoundOp`) and the stream's (`EE_SCENARIO_IO_*` from 22, `EESMManagerIO`).

// c_long/c_ulong are 32 bits on Windows
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::scenario::round::{camera_step, EeScenarioRoundHost, Round, EE_SCENARIO_ROUND_CAPTURE};
use crate::scenario::stream::*;

// ---- the round host's statements (on the manager `m`)
/// TEST ONLY: `EESMCaptureUpdate(m, a != 0)`.
pub const EE_SCENARIO_ROUND_CAPTURE_RECORD: u32 = 76;
/// `DeleteVectorContents` of `m->mTriggers`, `mConditions`, `mEffects`, `mObjects`, `mAreas`, `mPendingEffects`.
pub const EE_SCENARIO_ROUND_DELETE_TRIGGERS: u32 = 77;
pub const EE_SCENARIO_ROUND_DELETE_CONDITIONS: u32 = 78;
pub const EE_SCENARIO_ROUND_DELETE_EFFECTS: u32 = 79;
pub const EE_SCENARIO_ROUND_DELETE_OBJECTS: u32 = 80;
pub const EE_SCENARIO_ROUND_DELETE_AREAS: u32 = 81;
pub const EE_SCENARIO_ROUND_DELETE_PENDING: u32 = 82;
/// `ee_scenario_timed_free(m->mTimedEffects)`, `m->mTimedEffects = NULL`.
pub const EE_SCENARIO_ROUND_TIMED_FREE: u32 = 83;
pub const EE_SCENARIO_ROUND_TIMED_FORGET: u32 = 84;
/// `new EESMEffect` (its handle), effect a's `Copy(*effect b)`, `m->mPendingEffects.push_back(effect a)`.
pub const EE_SCENARIO_ROUND_EFFECT_NEW: u32 = 85;
pub const EE_SCENARIO_ROUND_EFFECT_COPY: u32 = 86;
pub const EE_SCENARIO_ROUND_PENDING_PUSH: u32 = 87;
/// `m->mEffects.size()`, `m->mEffects[a]` (a handle).
pub const EE_SCENARIO_ROUND_EFFECTS: u32 = 88;
pub const EE_SCENARIO_ROUND_EFFECT_AT: u32 = 89;
/// The held point (the original's local) set to `gWorld.mLine.GetStart()`: its address (a `GE3DPoint*`, x, y, z
/// `f32`s); `gWorld.mCameraControl.mZoom`, `gSideScrollSpeed` (an `f32`'s bits each);
/// `gWorld.mCameraControl.SetLocation(the held point, false, true)`.
pub const EE_SCENARIO_ROUND_LINE_START: u32 = 90;
pub const EE_SCENARIO_ROUND_CAMERA_ZOOM_FIELD: u32 = 92;
pub const EE_SCENARIO_ROUND_SIDE_SCROLL_SPEED: u32 = 93;
pub const EE_SCENARIO_ROUND_CAMERA_SET_START: u32 = 94;

// ---- the stream's statements (`slot` an element list, `value` as each says)
/// A new element of list `slot`: its handle into `*value` (a `void*`).
pub const EE_SCENARIO_IO_ELEMENT_NEW: u32 = 22;
/// The element `value` of list `slot`: its `Load` (the result), `push_back` onto the list, `delete`.
pub const EE_SCENARIO_IO_ELEMENT_READ: u32 = 23;
pub const EE_SCENARIO_IO_ELEMENT_KEEP: u32 = 24;
pub const EE_SCENARIO_IO_ELEMENT_FREE: u32 = 25;
/// `GetIsLoadingScenario()` (1/0).
pub const EE_SCENARIO_IO_LOADING_SCENARIO: u32 = 26;
/// The condition `value` names object 0 (`mType == kSMConditionObject && mObjectIndex == 0`; 1/0).
pub const EE_SCENARIO_IO_CONDITION_OBJECT_ZERO: u32 = 27;
/// The manager's `mMessages.size()` into `*value` (an `unsigned long`), `resize(*value)`, message `index`'s `Load`
/// and `Save` (the result).
pub const EE_SCENARIO_IO_MESSAGES_COUNT: u32 = 28;
pub const EE_SCENARIO_IO_MESSAGES_RESIZE: u32 = 29;
pub const EE_SCENARIO_IO_MESSAGE_LOAD: u32 = 30;
pub const EE_SCENARIO_IO_MESSAGE_SAVE: u32 = 31;

/// One stream statement: `io(op, slot, index, value)`.
pub type Io<'a> = dyn FnMut(u32, u32, c_ulong, *mut c_void) -> i32 + 'a;

/// A new element of list `kind` read by its `Load` and kept as the original kept it: a condition naming object 0
/// is dropped, a pending effect kept only while a saved game (not a scenario) loads; freed when not kept. The
/// result is the element's `Load`'s.
pub fn element_load(io: &mut Io, kind: u32) -> i32 {
    let mut element: *mut c_void = std::ptr::null_mut();
    io(EE_SCENARIO_IO_ELEMENT_NEW, kind, 0, (&mut element as *mut *mut c_void).cast());
    let r = io(EE_SCENARIO_IO_ELEMENT_READ, kind, 0, element);
    let mut keep = r == 0;
    if keep && kind == EE_SCENARIO_ELEMENT_CONDITIONS {
        keep = io(EE_SCENARIO_IO_CONDITION_OBJECT_ZERO, kind, 0, element) == 0;
    }
    if keep && kind == EE_SCENARIO_ELEMENT_PENDING {
        keep = io(EE_SCENARIO_IO_LOADING_SCENARIO, kind, 0, std::ptr::null_mut()) == 0;
    }
    let op = if keep { EE_SCENARIO_IO_ELEMENT_KEEP } else { EE_SCENARIO_IO_ELEMENT_FREE };
    io(op, kind, 0, element);
    r
}

/// The dialog messages' vector (`EESMLoadVector` / `EESMSaveVector`): the count, then each message's `Load` (the
/// vector resized to the count read) or `Save` while the result is 0; the first error.
pub fn messages(io: &mut Io, save: bool) -> i32 {
    let mut count: c_ulong = 0;
    let count_at = (&mut count as *mut c_ulong).cast::<c_void>();
    let mut r;
    if save {
        io(EE_SCENARIO_IO_MESSAGES_COUNT, 0, 0, count_at);
        r = io(EE_SCENARIO_IO_WRITE_ULONG, 0, 0, count_at);
    } else {
        r = io(EE_SCENARIO_IO_READ_ULONG, 0, 0, count_at);
        io(EE_SCENARIO_IO_MESSAGES_RESIZE, 0, 0, count_at);
    }
    let op = if save { EE_SCENARIO_IO_MESSAGE_SAVE } else { EE_SCENARIO_IO_MESSAGE_LOAD };
    let mut i: c_ulong = 0;
    while i < count && r == 0 {
        r = io(op, 0, i, std::ptr::null_mut());
        i += 1;
    }
    r
}

/// `EEScenarioManager::~EEScenarioManager`: the media unloaded, the element lists deleted, the timed queue freed.
pub fn destroy<H: Round>(h: &mut H) {
    crate::scenario::cinematic::unload_media(h, false);
    for op in EE_SCENARIO_ROUND_DELETE_TRIGGERS..=EE_SCENARIO_ROUND_DELETE_AREAS {
        h.op(op, 0, 0, 0.0);
    }
    h.op(EE_SCENARIO_ROUND_TIMED_FREE, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_TIMED_FORGET, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_DELETE_PENDING, 0, 0, 0.0);
}

/// `AddV134Effect`: a copy of `effect` kept pending (none while `mB234`).
pub fn add_pending<H: Round>(h: &mut H, effect: i64) {
    if h.fields(|f| f.mB234) {
        return;
    }
    let copy = h.op(EE_SCENARIO_ROUND_EFFECT_NEW, 0, 0, 0.0);
    h.op(EE_SCENARIO_ROUND_EFFECT_COPY, copy, effect, 0.0);
    h.op(EE_SCENARIO_ROUND_PENDING_PUSH, copy, 0, 0.0);
}

/// `SetTimedEffectHasRun`: the effect's `mEffects` index (the first holding it), its timed entries' flag.
pub fn set_timed_has_run<H: Round>(h: &mut H, effect: i64, flag: bool) {
    let count = h.op(EE_SCENARIO_ROUND_EFFECTS, 0, 0, 0.0) as c_ulong;
    let mut index: c_ulong = 0;
    while index < count && h.op(EE_SCENARIO_ROUND_EFFECT_AT, index as i64, 0, 0.0) != effect {
        index += 1;
    }
    let found = effect != 0 && index < count;
    h.queue(|q| q.set_has_run(found.then_some(index), flag));
}

fn float(v: i64) -> f32 {
    f32::from_bits(v as u32)
}

/// `MoveCameraToStart`: the camera one step from the line's start towards the scenario's start (`mStartX`,
/// `mStartY`), the step `(mZoom + 0.5) * gSideScrollSpeed`.
pub fn move_camera_to_start<H: Round>(h: &mut H) {
    let point = h.op(EE_SCENARIO_ROUND_LINE_START, 0, 0, 0.0) as usize as *mut [f32; 3];
    // SAFETY: the held point, read at once
    let mut at = unsafe { [(*point)[0], (*point)[1]] };
    let zoom = float(h.op(EE_SCENARIO_ROUND_CAMERA_ZOOM_FIELD, 0, 0, 0.0));
    let speed = float(h.op(EE_SCENARIO_ROUND_SIDE_SCROLL_SPEED, 0, 0, 0.0));
    if !h.fields(|f| camera_step(f, &mut at, (zoom + 0.5) * speed)) {
        return;
    }
    // SAFETY: as above, written before the next call
    unsafe {
        (*point)[0] = at[0];
        (*point)[1] = at[1];
    }
    h.op(EE_SCENARIO_ROUND_CAMERA_SET_START, 0, 0, 0.0);
}

/// The round host's operations composed here, or `None` for the others (the capture: only while recording).
pub fn compose<H: Round>(h: &mut H, op: u32, a: i64) -> Option<i64> {
    if op != EE_SCENARIO_ROUND_CAPTURE {
        return None;
    }
    if crate::app::capture_record::active() {
        h.op(EE_SCENARIO_ROUND_CAPTURE_RECORD, a, 0, 0.0);
    }
    Some(0)
}

// ---- the C ABI (the C++ members forward here with their round host, `EESMRoundHost(this)`)

/// `EEScenarioManager::~EEScenarioManager`.
#[no_mangle]
pub extern "C" fn ee_scenario_manager_destroy(mut host: EeScenarioRoundHost) {
    destroy(&mut host);
}

/// `EEScenarioManager::AddV134Effect` (`effect` the `const EESMEffect*`).
#[no_mangle]
pub extern "C" fn ee_scenario_add_pending(mut host: EeScenarioRoundHost, effect: *const c_void) {
    add_pending(&mut host, effect as usize as i64);
}

/// `EEScenarioManager::SetTimedEffectHasRun` (`effect` the `EESMEffect*`).
#[no_mangle]
pub extern "C" fn ee_scenario_set_timed_has_run(mut host: EeScenarioRoundHost, effect: *const c_void, flag: bool) {
    set_timed_has_run(&mut host, effect as usize as i64, flag);
}

/// `EEScenarioManager::MoveCameraToStart`.
#[no_mangle]
pub extern "C" fn ee_scenario_move_camera_to_start(mut host: EeScenarioRoundHost) {
    move_camera_to_start(&mut host);
}

/// `EEScenarioManager::ObjectChanged`: the object's parent set (`parent`) or its conditions renamed.
///
/// # Safety
/// As `ee_scenario_list_set_parent` and `ee_scenario_list_rename_object_conditions`.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_changed(
    host: *const crate::scenario::lists::EeScenarioListHost,
    object: *mut c_void,
    parent: bool,
    value: c_long,
) {
    // SAFETY: the caller's contract
    unsafe {
        if parent {
            crate::scenario::lists::ee_scenario_list_set_parent(host, object, value);
        } else {
            crate::scenario::lists::ee_scenario_list_rename_object_conditions(host, object);
        }
    }
}

#[cfg(test)]
#[path = "manager_compose_tests.rs"]
mod tests;
