//! `extern "C"` for the game's actions whole in Rust (`game_actions.rs`): the calls of `EEARust` (`ee/EEARust.h`),
//! its constructor, destructor and run, and its users' (the click's execute and selection, the lasso's selection, the
//! build placements, the calamity's cast and check, the camera's targets, the edge scroll's update, the improvement's
//! begin). The host is `ee_screenscpp_ga` and `ee_screenscpp_ga_record` (`Actions/EEARust.cpp`).

use core::ffi::c_void;

use crate::game_actions::{EeScreensGaFields, EeScreensGaValue, Ga, GaHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    use crate::game_actions::{EeScreensGaFields, EeScreensGaValue};

    extern "C" {
        pub fn ee_screenscpp_ga(op: u32, r: *mut EeScreensGaFields, p: *mut c_void, i: *mut i64, f: *mut f32) -> i64;
        pub fn ee_screenscpp_ga_record(kind: u32, values: *const EeScreensGaValue, count: u32) -> *mut c_void;
    }
}

struct Host;

impl GaHost for Host {
    fn op(&mut self, op: u32, r: *mut EeScreensGaFields, p: usize, i: &mut [i64; 8], f: &mut [f32; 8]) -> i64 {
        unsafe { sys::ee_screenscpp_ga(op, r, p as *mut c_void, i.as_mut_ptr(), f.as_mut_ptr()) }
    }
    fn record(&mut self, kind: u32, values: &[EeScreensGaValue]) -> usize {
        unsafe { sys::ee_screenscpp_ga_record(kind, values.as_ptr(), values.len() as u32) as usize }
    }
}

/// `code` over the action's record and the C++ host.
///
/// # Safety
/// `r` is a live `EEARust`'s record.
unsafe fn with<R>(r: *mut EeScreensGaFields, code: impl FnOnce(&mut Ga<'_, Host>) -> R) -> R {
    let mut host = Host;
    let mut ga = unsafe { Ga::new(r, &mut host) };
    code(&mut ga)
}

/// The constructor's members (the record value-initialised, its kind set).
///
/// # Safety
/// `r` is the action's record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_init(r: *mut EeScreensGaFields) {
    unsafe { with(r, |g| g.init()) }
}

/// The destructor's releases.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_free(r: *mut EeScreensGaFields) {
    unsafe { with(r, |g| g.free()) }
}

/// `Virtual4`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_run(r: *mut EeScreensGaFields) {
    unsafe { with(r, |g| g.run()) }
}

/// The mouse click's `Execute`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_screens_ga_click_execute(
    r: *mut EeScreensGaFields,
    x: f32,
    y: f32,
    mv: bool,
    modifiers: u32,
    rotating: bool,
    facing: f32,
    force: bool,
) {
    unsafe { with(r, |g| g.click_execute(x, y, mv, modifiers, rotating, facing, force)) }
}

/// The mouse click's `SelectAtMouse`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_select_at_mouse(r: *mut EeScreensGaFields, modifiers: u32) {
    unsafe { with(r, |g| g.select_at_mouse(modifiers)) }
}

/// The lasso's `SelectUnits` over the rectangle `rect` (a `GERectangle*`, handed to the message).
///
/// # Safety
/// As `ee_screens_ga_init`; `rect` lives for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_select_units(
    r: *mut EeScreensGaFields,
    rect: *const c_void,
    flags: i32,
    unit_type: u32,
    patrol_group: i32,
) {
    unsafe { with(r, |g| g.lasso_select(rect as usize, flags, unit_type, patrol_group)) }
}

/// The build's `Build`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build(r: *mut EeScreensGaFields, x: f32, y: f32) {
    unsafe { with(r, |g| g.build(x, y)) }
}

/// The build's `BuildGate`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build_gate(r: *mut EeScreensGaFields, x: f32, y: f32, side: i32) {
    unsafe { with(r, |g| g.build_gate(x, y, side)) }
}

/// The build's `BuildWall`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_build_wall(r: *mut EeScreensGaFields, x0: f32, y0: f32, x1: f32, y1: f32) {
    unsafe { with(r, |g| g.build_wall(x0, y0, x1, y1)) }
}

/// The calamity's `Cast`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_cast(r: *mut EeScreensGaFields, x: f32, y: f32) {
    unsafe { with(r, |g| g.cast((x, y))) }
}

/// The calamity's `Check`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_check(r: *mut EeScreensGaFields, x: f32, y: f32) -> i32 {
    unsafe { with(r, |g| g.check((x, y))) }
}

/// The camera's `SetLocation`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_set_location(r: *mut EeScreensGaFields, x: i32, y: i32) {
    unsafe { with(r, |g| g.set_location(x, y)) }
}

/// The camera's `SetUnit` (an `EEUnit*`).
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_set_unit(r: *mut EeScreensGaFields, unit: *mut c_void) {
    unsafe { with(r, |g| g.set_unit(unit as usize)) }
}

/// The edge scroll's `Update`.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_scroll_update(r: *mut EeScreensGaFields, x: f32, y: f32) -> bool {
    unsafe { with(r, |g| g.scroll_update((x, y))) }
}

/// The improvement's constructor: the unit (an `EEUnit*`) referenced and held, the action posted.
///
/// # Safety
/// As `ee_screens_ga_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ga_upgrade_begin(r: *mut EeScreensGaFields, unit: *mut c_void) {
    unsafe { with(r, |g| g.upgrade_begin(unit as usize)) }
}
