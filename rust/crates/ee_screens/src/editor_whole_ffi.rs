//! `extern "C"` for the scenario editor's actions whole in Rust (`editor_whole.rs`): the record an `EESEARust`
//! (`ee/EESEARust.h`) holds, made, begun, run and freed by it, and the calls of the actions' users (the rotation,
//! the trigger area's corner, the wall and gate placements). The host is `ee_screenscpp_sew` and
//! `ee_screenscpp_sew_elevation` (`Scenario Editor Actions/EESEAPaintTerrain.cpp`).

use core::ffi::c_void;

use crate::editor_whole::{SewAction, SewElevation, SewGate, SewHost, SewWall};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sew(op: u32, p: *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sew_elevation(data: *mut i16, capacity: usize, out: *mut i32) -> usize;
    }
}

struct Host;

impl SewHost for Host {
    fn op(&mut self, op: u32, p: usize, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
        unsafe { sys::ee_screenscpp_sew(op, p as *mut c_void, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
    fn elevation(&mut self) -> Option<SewElevation> {
        // the host reads the picked file pair; a second call with room for all its heights (none written when
        // there was too little room)
        let mut out = [0i32; 6];
        let mut data: Vec<i16> = Vec::new();
        loop {
            let n = unsafe { sys::ee_screenscpp_sew_elevation(data.as_mut_ptr(), data.len(), out.as_mut_ptr()) };
            if n == 0 {
                return None;
            }
            if n <= data.len() {
                data.truncate(n);
                break;
            }
            data = vec![0; n];
        }
        Some(SewElevation {
            width: out[0] as u32,
            height: out[1] as u32,
            data,
            water_cutoff: out[2],
            water_depth: out[3],
            land_height: out[4],
            land_cutoff: out[5],
        })
    }
}

/// An `EESEARust`'s action (opaque to C++).
pub struct EeScreensSewAction(SewAction);

/// `code` over the action taken out of its box: no Rust reference into the box lives across the host's calls.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new`.
unsafe fn with_action<R>(action: *mut EeScreensSewAction, code: impl FnOnce(&mut SewAction) -> R) -> R {
    let empty = SewAction { owner: 0, kind: crate::editor_whole::SewKind::Exit };
    let mut record = unsafe { core::mem::replace(&mut (*action).0, empty) };
    let result = code(&mut record);
    unsafe { (*action).0 = record };
    result
}

/// The action of `kind` (`EE_SCREENS_SEW_*`) for the `EESEARust` `owner`, with its object and arguments; NULL for
/// an unknown kind. Freed by `ee_screens_sew_free`.
#[no_mangle]
pub extern "C" fn ee_screens_sew_new(
    owner: *mut c_void,
    kind: u32,
    object: *mut c_void,
    a: i32,
    b: i32,
    c: i32,
    d: i32,
) -> *mut EeScreensSewAction {
    match SewAction::new(&mut Host, owner as usize, kind, object as usize, a, b, c, d) {
        Some(action) => Box::into_raw(Box::new(EeScreensSewAction(action))),
        None => core::ptr::null_mut(),
    }
}

/// After the `EESEARust` holds it: the kinds made posted post.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_begin(action: *mut EeScreensSewAction) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.begin(&mut Host)) }
    }
}

/// `Virtual4`.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_run(action: *mut EeScreensSewAction) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.run(&mut Host)) }
    }
}

/// The `EESEARust`'s destructor.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_free(action: *mut EeScreensSewAction) {
    if !action.is_null() {
        let action = unsafe { Box::from_raw(action) };
        action.0.free(&mut Host);
    }
}

/// The rotation's yaw kept and the action posted (`EESEARotateUnits::Rotate`).
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_rotate(action: *mut EeScreensSewAction, yaw: f32) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.rotate(&mut Host, yaw)) }
    }
}

/// The trigger area's dragged corner (`EESEAPaintArea::SetCorner`).
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_area_corner(action: *mut EeScreensSewAction, x: i32, y: i32) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.area_corner(&mut Host, x, y)) }
    }
}

/// The trigger area's rectangle -> out[0..4] = left, top, right, bottom.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL); `out` points at four integers.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_area(action: *const EeScreensSewAction, out: *mut i32) {
    let r = if action.is_null() { [0; 4] } else { unsafe { (*action).0.area() } };
    unsafe { core::slice::from_raw_parts_mut(out, 4).copy_from_slice(&r) };
}

/// `EESEAPlaceWall::Place`: a wall line queued and the action posted.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_wall(
    action: *mut EeScreensSewAction,
    x0: f32,
    y0: f32,
    x1: f32,
    y1: f32,
    player: i32,
    technology: i32,
) {
    if !action.is_null() {
        let wall = SewWall { start: (x0, y0), end: (x1, y1), player, technology };
        unsafe { with_action(action, |a| a.place_wall(&mut Host, wall)) }
    }
}

/// `EESEAPlaceGate::Place`: a gate queued and the action posted when every ghost can be placed; 1 when it was.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_gate(
    action: *mut EeScreensSewAction,
    x: i32,
    y: i32,
    player: i32,
    technology: i32,
    direction: i32,
) -> u8 {
    if action.is_null() {
        return 0;
    }
    let gate = SewGate { x, y, player, technology, direction };
    unsafe { with_action(action, |a| u8::from(a.place_gate(&mut Host, gate))) }
}
