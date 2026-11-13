//! The C interface of `scene.rs`: the capture's fade, letterbox, clocks and time-of-day blend
//! (`EERenderObject.cpp`), and `capture.rs`'s per-object arithmetic. The state is the C++'s storage, passed for each
//! call.

use ee_ge::types::{Ge2DPoint, Ge3DPoint};

use crate::capture::{self, GroundHeights, EE_GFX_RING_POINTS};
use crate::ffi_effect::EeGfxParticleGround;
use crate::scene::{
    self, EeGfxDaylight, EeGfxDaylightTables, EeGfxLetterbox, EeGfxSceneClocks, EeGfxSceneFade, EeGfxSceneStep,
};

fn fade<'a>(s: *mut EeGfxSceneFade) -> &'a mut EeGfxSceneFade {
    // SAFETY: the C++ passes its one state, not otherwise borrowed during the call
    unsafe { &mut *s }
}

/// `EEInitialiseScene`'s reset of the fade and letterbox.
///
/// # Safety
/// `s` is the C++'s state.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_reset(s: *mut EeGfxSceneFade) {
    scene::reset(fade(s))
}

/// `EEStartLightFadeOut` (`down`) / `EEStartLightFadeIn` over `frames` (0: at once).
///
/// # Safety
/// `s` is the C++'s state.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_start_fade(s: *mut EeGfxSceneFade, down: bool, frames: u32) {
    scene::start_fade(fade(s), down, frames)
}

/// `EEStartLetterboxOut` (`out`) / `EEStartLetterboxIn` over `frames`, from the zoom `zoom`.
///
/// # Safety
/// `s` is the C++'s state.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_start_letterbox(s: *mut EeGfxSceneFade, out: bool, frames: u32, zoom: f32) {
    scene::start_letterbox(fade(s), out, frames, zoom)
}

/// The grid, water and cloud clocks after `elapsed` milliseconds in a wind of `wind`.
///
/// # Safety
/// `c` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_step_clocks(c: *mut EeGfxSceneClocks, elapsed: f32, wind: f32) {
    // SAFETY: the function's contract
    scene::step_clocks(unsafe { &mut *c }, elapsed, wind)
}

/// A frame's fade and letterbox for `elapsed` milliseconds from the zoom `zoom`.
///
/// # Safety
/// `s` is the C++'s state.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_step(s: *mut EeGfxSceneFade, elapsed: f32, zoom: f32) -> EeGfxSceneStep {
    scene::step(fade(s), elapsed, zoom)
}

/// The view band at `zoom` and the bars' rows at the window height `height`.
#[no_mangle]
pub extern "C" fn ee_gfx_scene_letterbox(zoom: f32, height: f32) -> EeGfxLetterbox {
    scene::letterbox(zoom, height)
}

/// The lights at `time_of_day`.
///
/// # Safety
/// `t` is readable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_scene_daylight(t: *const EeGfxDaylightTables, time_of_day: f32) -> EeGfxDaylight {
    // SAFETY: the function's contract
    scene::daylight(unsafe { &*t }, time_of_day)
}

// ---- the capture's per-object arithmetic (capture.rs)

/// A map object's place inside its tiles (`FillObject`, `FillDoppelgangerObject`).
#[no_mangle]
pub extern "C" fn ee_gfx_capture_object_place(id: i32, size: i32, tile_x: i32, tile_y: i32) -> Ge2DPoint {
    let (x, y) = capture::object_place(id, size, tile_x, tile_y);
    Ge2DPoint { x, y }
}

/// A map object's size jitter by id.
#[no_mangle]
pub extern "C" fn ee_gfx_capture_object_scale(id: i32) -> f32 {
    capture::object_scale(id)
}

/// The sway clock after a frame at `now`.
///
/// # Safety
/// `clock` and `last` are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_capture_sway_tick(clock: *mut u32, last: *mut u32, now: u32) {
    // SAFETY: the function's contract
    unsafe { capture::sway_tick(&mut *clock, &mut *last, now) }
}

/// A swaying map object's blend fraction.
#[no_mangle]
pub extern "C" fn ee_gfx_capture_sway_fraction(clock: u32, id: i32) -> f32 {
    capture::sway_fraction(clock, id)
}

struct Ground<'a>(&'a EeGfxParticleGround);

impl GroundHeights for Ground<'_> {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool {
        (self.0.is_valid_tile)(self.0.context, x, y)
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        (self.0.mesh_z)(self.0.context, x, y)
    }
}

/// `DrawGroundCircle`'s 51 points into `points` (the last the first again); returns the strip's width.
///
/// # Safety
/// `ground` is readable; `points` has room for `EE_GFX_RING_POINTS`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_capture_ground_circle(
    ground: *const EeGfxParticleGround,
    x: f32,
    y: f32,
    radius: f32,
    points: *mut Ge3DPoint,
) -> f32 {
    // SAFETY: the function's contract
    let (p, width) = capture::ground_circle(&mut Ground(unsafe { &*ground }), x, y, radius);
    // SAFETY: the function's contract
    unsafe { std::ptr::copy_nonoverlapping(p.as_ptr(), points, EE_GFX_RING_POINTS) };
    width
}
