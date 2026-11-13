//! `extern "C"` for the scenario editor's actions whole in Rust (`editor_whole.rs`): the record an `EESEARust`
//! (`ee/EESEARust.h`) holds, made, begun, run and freed by it, and the calls of the actions' users (the rotation,
//! the trigger area's corner, the wall and gate placements). The host is `ee_screenscpp_sew` and
//! `ee_screenscpp_sew_elevation` (`Scenario Editor Actions/EESEARust.cpp`).

use core::ffi::c_void;

use ee_ge::types::GeTransformation;

use crate::editor_events::{SeeHost, SeeKind};
use crate::editor_place::SepHost;
use crate::editor_whole::{SewAction, SewElevation, SewGate, SewHost, SewWall};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sew(op: u32, p: *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sew_elevation(data: *mut i16, capacity: usize, out: *mut i32) -> usize;
        pub fn ee_screenscpp_see(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sep(op: u32, p: *mut c_void, t: *mut f32, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SepHost for Host {
    fn sep(&mut self, op: u32, p: usize, t: &mut GeTransformation, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
        // a transformation is sixteen floats, row by row (GETransformation's mMatrix)
        let t = t.m.as_mut_ptr().cast::<f32>();
        unsafe { sys::ee_screenscpp_sep(op, p as *mut c_void, t, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
}

impl SewHost for Host {
    fn op(&mut self, op: u32, p: usize, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
        unsafe { sys::ee_screenscpp_sew(op, p as *mut c_void, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
    fn delete(&mut self, what: crate::editor_actions::SeuDelete) {
        crate::editor_actions::delete_units(&mut crate::editor_actions_ffi::Selection, what);
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

/// `EESEAPlaceUnit::Place(player)`: 1 when any unit was queued (the action posted).
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_place_unit(action: *mut EeScreensSewAction, player: i32) -> u8 {
    if action.is_null() {
        return 0;
    }
    unsafe { with_action(action, |a| u8::from(a.place_unit(&mut Host, player))) }
}

/// `EESEAMoveUnit::PickUp(id)`.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_pick_up(action: *mut EeScreensSewAction, id: u32) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.pick_up(&mut Host, id)) }
    }
}

/// `EESEAMoveUnit::PutDown`.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_put_down(action: *mut EeScreensSewAction) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.put_down(&mut Host)) }
    }
}

/// `EESEAMoveUnit::Cancel`.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_cancel(action: *mut EeScreensSewAction) {
    if !action.is_null() {
        unsafe { with_action(action, |a| a.cancel(&mut Host)) }
    }
}

/// The move's `mIsMoving`: 1 while a unit is picked up.
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_moving(action: *const EeScreensSewAction) -> u8 {
    if action.is_null() {
        return 0;
    }
    u8::from(unsafe { (*action).0.moving() })
}

/// A unit queued for the placement without a ghost (the original's `mUnits.push_back`: no transport, not posted);
/// `t` the unit's transformation (sixteen floats).
///
/// # Safety
/// `action` comes from `ee_screens_sew_new` (or is NULL); `t` points at sixteen floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sew_place_push(
    action: *mut EeScreensSewAction,
    player: i32,
    unit_type: i32,
    t: *const f32,
    animation: i32,
) {
    if action.is_null() {
        return;
    }
    let mut transformation = GeTransformation::IDENTITY;
    let floats = unsafe { core::slice::from_raw_parts(t, 16) };
    for (k, v) in floats.iter().enumerate() {
        transformation.m[k / 4][k % 4] = *v;
    }
    let entry = crate::editor_place::SepPlaced { container: -1, t: transformation, player, unit_type, animation };
    unsafe { (*action).0.place_push(entry) };
}

// ---- the editor form's events whole in Rust (`editor_events.rs`; `EESEERust`; the host `ee_screenscpp_see` in
// `EEUIFSETRBPaintType.cpp`)

impl SeeHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        // three pointers (the host's p[0], p[1], p[2]), four integers, four floats
        let p = p.map(|x| x as *mut c_void);
        unsafe { sys::ee_screenscpp_see(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
    }
}

/// An `EESEERust`'s event (opaque to C++).
pub struct EeScreensSeeEvent(SeeKind);

/// The event of `kind` (`EE_SCREENS_SEE_*`; `object` the quick save/load action); NULL for an unknown kind. Freed
/// by `ee_screens_see_free`.
#[no_mangle]
pub extern "C" fn ee_screens_see_new(kind: u32, object: *mut c_void) -> *mut EeScreensSeeEvent {
    match SeeKind::new(&mut Host, kind, object as usize) {
        Some(e) => Box::into_raw(Box::new(EeScreensSeeEvent(e))),
        None => core::ptr::null_mut(),
    }
}

/// `ProcessEvent`: the event's `form` and control `entity`.
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_process(
    event: *const EeScreensSeeEvent,
    form: *mut c_void,
    entity: *mut c_void,
) -> i32 {
    if event.is_null() {
        return 0;
    }
    // a copy: no Rust reference into the box across the host's calls (the post may run other events)
    let e = unsafe { (*event).0.clone() };
    e.process_in(&mut Host, form as usize, entity as usize)
}

/// The viewport change's rectangle (four floats: left, top, right, bottom) and whether the UI is hidden.
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL); `rectangle` points at four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_viewport(event: *mut EeScreensSeeEvent, rectangle: *const f32, hidden: u8) {
    if !event.is_null() {
        let r = unsafe { core::slice::from_raw_parts(rectangle, 4) };
        unsafe { (*event).0.set_viewport([r[0], r[1], r[2], r[3]], hidden != 0) };
    }
}

/// The hide-UI event's views (four floats each: left, top, right, bottom), posted while the box is unchecked and
/// checked (`EESETriggerScreen::Enter`; the original's inline `SetRectangles` on the event's two rectangles).
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL); `unchecked` and `checked` point at four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_hide_ui_views(
    event: *mut EeScreensSeeEvent,
    unchecked: *const f32,
    checked: *const f32,
) {
    if !event.is_null() {
        let (u, c) = unsafe { (core::slice::from_raw_parts(unchecked, 4), core::slice::from_raw_parts(checked, 4)) };
        unsafe { (*event).0.set_hide_ui_views([u[0], u[1], u[2], u[3]], [c[0], c[1], c[2], c[3]]) };
    }
}

/// The slider event's label, NUL-terminated format, scale and offset (`EESEESliderChange`'s constructor).
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL); `format` is NUL-terminated (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_slider(
    event: *mut EeScreensSeeEvent,
    label: *mut c_void,
    format: *const core::ffi::c_char,
    scale: f32,
    offset: f32,
) {
    if !event.is_null() && !format.is_null() {
        let format = unsafe { core::ffi::CStr::from_ptr(format) }.to_bytes();
        unsafe { (*event).0.set_slider(label as usize, format, scale, offset) };
    }
}

/// A control the check box event shows and hides (`EESEECheckDisplay::mControls.push_back`).
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_push_control(event: *mut EeScreensSeeEvent, control: *mut c_void) {
    if !event.is_null() {
        unsafe { (*event).0.push_control(control as usize) };
    }
}

/// The object/area event's label, scenario combo box and area control (`EESEEUseObjectArea`'s constructor).
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_object_area(
    event: *mut EeScreensSeeEvent,
    label: *mut c_void,
    combo: *mut c_void,
    area: *mut c_void,
) {
    if !event.is_null() {
        unsafe { (*event).0.set_object_area(label as usize, combo as usize, area as usize) };
    }
}

/// The effect editors' events' objects (`EESETSETCAMGModifyObject`: the type and its two lists;
/// `EESETSETMSDCameraScrollChange`, `EESETSETSAAAttributeChange`: the type and the effect's controls, `c` NULL).
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_objects(
    event: *mut EeScreensSeeEvent,
    a: *mut c_void,
    b: *mut c_void,
    c: *mut c_void,
) {
    if !event.is_null() {
        unsafe { (*event).0.set_objects(a as usize, b as usize, c as usize) };
    }
}

/// The `EESEERust`'s destructor.
///
/// # Safety
/// `event` comes from `ee_screens_see_new` (or is NULL) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_see_free(event: *mut EeScreensSeeEvent) {
    if !event.is_null() {
        let e = unsafe { Box::from_raw(event) };
        e.0.free(&mut Host);
    }
}
