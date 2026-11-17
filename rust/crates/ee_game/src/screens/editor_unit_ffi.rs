//! `extern "C"` for the scenario editor's unit tab whole in Rust (`editor_unit.rs`): the object `EESEUnit` holds,
//! made, called and freed by it. The shared control ops (below 100) are `ee_screenscpp_sepl`
//! (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_seun` (`Scenario Editor/EESEUnit.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::SeplHost;
use crate::screens::editor_unit::UnitTab;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seun(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 300 {
                crate::screens::sgvw::editor(op, i, f, |i, f| {
                    sys::ee_screenscpp_seun(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
                })
            } else {
                crate::screens::sgvw::editor(op, i, f, |i, f| {
                    sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
                })
            }
        }
    }
}

/// The unit tab (opaque to C++).
pub struct EeScreensSeUnit(UnitTab);

/// The tab's members without arguments: `Update`, `Leave`, `Enter`, `UpdateKeys`, `PlaceWall`, `EndWall`,
/// `PlaceGate`, `EndGate`, `ReselectUnitType`, `SelectUnitType`, `UpdateSelection`, `RefreshUnitList`.
pub const EE_SCREENS_SEUN_UPDATE: u32 = 1;
pub const EE_SCREENS_SEUN_LEAVE: u32 = 2;
pub const EE_SCREENS_SEUN_ENTER: u32 = 3;
pub const EE_SCREENS_SEUN_UPDATE_KEYS: u32 = 4;
pub const EE_SCREENS_SEUN_PLACE_WALL: u32 = 5;
pub const EE_SCREENS_SEUN_END_WALL: u32 = 6;
pub const EE_SCREENS_SEUN_PLACE_GATE: u32 = 7;
pub const EE_SCREENS_SEUN_END_GATE: u32 = 8;
pub const EE_SCREENS_SEUN_RESELECT: u32 = 9;
pub const EE_SCREENS_SEUN_SELECT: u32 = 10;
pub const EE_SCREENS_SEUN_UPDATE_SELECTION: u32 = 11;
pub const EE_SCREENS_SEUN_REFRESH: u32 = 12;
/// With an argument `a`: `ShowCargo(a != 0)`, `MouseMove(x, y)`; `ShowUnitStatus(p)`, `Draw(p: the rectangle)`.
pub const EE_SCREENS_SEUN_SHOW_CARGO: u32 = 13;
pub const EE_SCREENS_SEUN_MOUSE_MOVE: u32 = 14;
pub const EE_SCREENS_SEUN_SHOW_UNIT: u32 = 15;
pub const EE_SCREENS_SEUN_DRAW: u32 = 16;
/// Values: `GetLayer`, `GetSortMode`, the unit under the mouse, the placement flags (1 placing, 2 a wall, 4 a gate,
/// 8 a held gate), the palette, sort and owner last listed; `GetRotationAngle` through `ee_screens_seun_angle`.
pub const EE_SCREENS_SEUN_GET_LAYER: u32 = 20;
pub const EE_SCREENS_SEUN_GET_SORT: u32 = 21;
pub const EE_SCREENS_SEUN_GET_UNDER_MOUSE: u32 = 22;
pub const EE_SCREENS_SEUN_GET_FLAGS: u32 = 23;
pub const EE_SCREENS_SEUN_GET_LISTED_MODE: u32 = 24;
pub const EE_SCREENS_SEUN_GET_LISTED_SORT: u32 = 25;
pub const EE_SCREENS_SEUN_GET_LISTED_LAYER: u32 = 26;
/// Handles: the selection tool.
pub const EE_SCREENS_SEUN_GET_SELECT_UNITS: u32 = 27;

/// `EESEUnit`'s constructor: every control made on `form` from the control index `*index` (advanced); `shell` is
/// the C++ `EESEUnit` (its build tools), `players` the editor's player combo box. Freed by `ee_screens_seun_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_new(
    shell: *mut c_void,
    form: *mut c_void,
    players: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeUnit {
    let mut next = unsafe { *index };
    let u = UnitTab::new(&mut Host, shell as usize, form as usize, players as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeUnit(u)))
}

/// The destructor: every control deleted, the actions, bitmaps and event released (the original's order), the
/// object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_free(tab: *mut EeScreensSeUnit) {
    if tab.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(tab) };
    b.0.destroy(&mut Host);
}

/// One of the tab's members (`EE_SCREENS_SEUN_UPDATE` ...) with its arguments (`a`, `x`, `y`, `p`).
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL); `p` is what the member takes.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_call(
    tab: *const EeScreensSeUnit,
    what: u32,
    a: i32,
    x: f32,
    y: f32,
    p: *mut c_void,
) {
    if tab.is_null() {
        return;
    }
    // shared: a host call can come back into the tab (a build tool's Build or Cancel, a sort radio's event)
    let u = unsafe { &(*tab).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SEUN_UPDATE => u.update(h),
        EE_SCREENS_SEUN_LEAVE => u.leave(h),
        EE_SCREENS_SEUN_ENTER => u.enter(h),
        EE_SCREENS_SEUN_UPDATE_KEYS => u.update_keys(h),
        EE_SCREENS_SEUN_PLACE_WALL => u.place_wall(h),
        EE_SCREENS_SEUN_END_WALL => u.end_wall(h),
        EE_SCREENS_SEUN_PLACE_GATE => u.place_gate(h),
        EE_SCREENS_SEUN_END_GATE => u.end_gate(h),
        EE_SCREENS_SEUN_RESELECT => u.reselect_unit_type(h),
        EE_SCREENS_SEUN_SELECT => u.select_unit_type(h),
        EE_SCREENS_SEUN_UPDATE_SELECTION => u.update_selection(h),
        EE_SCREENS_SEUN_REFRESH => u.refresh_unit_list(h),
        EE_SCREENS_SEUN_SHOW_CARGO => u.show_cargo(h, a != 0),
        EE_SCREENS_SEUN_MOUSE_MOVE => u.mouse_move(h, x, y),
        EE_SCREENS_SEUN_SHOW_UNIT => u.show_unit_status(h, p as usize),
        EE_SCREENS_SEUN_DRAW => u.draw(h, p as usize),
        _ => {}
    }
}

/// One of the tab's values (`EE_SCREENS_SEUN_GET_LAYER` ...); `SetUnitUnderMouse` with `EE_SCREENS_SEUN_GET_UNDER_MOUSE`
/// through `ee_screens_seun_set_under_mouse`.
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_get(tab: *const EeScreensSeUnit, what: u32) -> i32 {
    if tab.is_null() {
        return 0;
    }
    let u = unsafe { &(*tab).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SEUN_GET_LAYER => u.get_layer(h),
        EE_SCREENS_SEUN_GET_SORT => u.get_sort_mode(h),
        EE_SCREENS_SEUN_GET_UNDER_MOUSE => u.under_mouse.get(),
        EE_SCREENS_SEUN_GET_FLAGS => {
            u.placing.get() as i32
                | (u.placing_wall.get() as i32) << 1
                | (u.placing_gate.get() as i32) << 2
                | (u.gate_held.get() as i32) << 3
        }
        EE_SCREENS_SEUN_GET_LISTED_MODE => u.type_mode.get(),
        EE_SCREENS_SEUN_GET_LISTED_SORT => u.sort_mode.get(),
        EE_SCREENS_SEUN_GET_LISTED_LAYER => u.layer.get(),
        _ => 0,
    }
}

/// A handle of the tab (`EE_SCREENS_SEUN_GET_SELECT_UNITS`).
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_handle(tab: *const EeScreensSeUnit, what: u32) -> *mut c_void {
    if tab.is_null() {
        return core::ptr::null_mut();
    }
    let u = unsafe { &(*tab).0 };
    match what {
        EE_SCREENS_SEUN_GET_SELECT_UNITS => u.select_units as *mut c_void,
        _ => core::ptr::null_mut(),
    }
}

/// `SetUnitUnderMouse`.
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_set_under_mouse(tab: *const EeScreensSeUnit, unit: i32) {
    if !tab.is_null() {
        unsafe { &(*tab).0 }.under_mouse.set(unit);
    }
}

/// `GetRotationAngle`: the rotation button's yaw.
///
/// # Safety
/// `tab` comes from `ee_screens_seun_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seun_angle(tab: *const EeScreensSeUnit) -> f32 {
    if tab.is_null() {
        return 0.0;
    }
    unsafe { &(*tab).0 }.rotation_angle(&mut Host)
}
