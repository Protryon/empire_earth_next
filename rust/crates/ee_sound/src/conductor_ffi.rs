//! The C ABI of `SEConductor.cpp` and the C++ trampolines (`ee_conductorcpp_*`, defined there) through which the
//! conductor reaches dmusic, the scores and the mixer. Compiled out of `cargo test`.
//!
//! Every `ee_conductor_*` function taking an `EeConductor` is called with the C++ conductor's lock held and the C++
//! object's scalar members in `f` (written back after the call). `ctx` is the C++ `SEConductor*`, handed back to the
//! trampolines. No trampoline calls back into this module; a reentrant call aborts (the `busy` flag).

use std::cell::{Cell, UnsafeCell};
use std::ffi::{c_char, c_void, CStr};

use crate::conductor::{Conductor, ConductorFields, Embellishment, Host, Score, Timing};

/// cbindgen:ignore
mod sys {
    use super::*;
    extern "C" {
        pub(super) fn ee_conductorcpp_output_rate(ctx: *mut c_void) -> u32;
        pub(super) fn ee_conductorcpp_start(ctx: *mut c_void, rate: u32) -> bool;
        pub(super) fn ee_conductorcpp_release(ctx: *mut c_void);
        pub(super) fn ee_conductorcpp_set_music_source(ctx: *mut c_void, on: bool);
        pub(super) fn ee_conductorcpp_load(ctx: *mut c_void, name: *const c_char) -> *mut c_void;
        pub(super) fn ee_conductorcpp_delete(score: *mut c_void);
        pub(super) fn ee_conductorcpp_is_secondary(score: *mut c_void) -> bool;
        pub(super) fn ee_conductorcpp_play(ctx: *mut c_void, score: *mut c_void, timing: u32) -> bool;
        pub(super) fn ee_conductorcpp_play_transition(ctx: *mut c_void, score: *mut c_void, pattern: u32) -> bool;
        pub(super) fn ee_conductorcpp_set_volume(ctx: *mut c_void, amplitude: f32);
        pub(super) fn ee_conductorcpp_set_groove_level(ctx: *mut c_void, level: i8);
    }
}
use sys::*;

struct CppHost {
    ctx: *mut c_void,
}

impl Host for CppHost {
    fn output_rate(&mut self) -> u32 {
        unsafe { ee_conductorcpp_output_rate(self.ctx) }
    }
    fn start(&mut self, rate: u32) -> bool {
        unsafe { ee_conductorcpp_start(self.ctx, rate) }
    }
    fn release(&mut self) {
        unsafe { ee_conductorcpp_release(self.ctx) }
    }
    fn set_music_source(&mut self, on: bool) {
        unsafe { ee_conductorcpp_set_music_source(self.ctx, on) }
    }
    fn load(&mut self, name: &[u8]) -> Option<Score> {
        let mut z = name.to_vec();
        z.push(0);
        let s = unsafe { ee_conductorcpp_load(self.ctx, z.as_ptr() as *const c_char) };
        (!s.is_null()).then_some(Score(s))
    }
    fn delete(&mut self, s: Score) {
        unsafe { ee_conductorcpp_delete(s.0) }
    }
    fn is_secondary(&self, s: Score) -> bool {
        unsafe { ee_conductorcpp_is_secondary(s.0) }
    }
    fn play(&mut self, s: Option<Score>, timing: Timing) -> bool {
        unsafe { ee_conductorcpp_play(self.ctx, s.map_or(std::ptr::null_mut(), |s| s.0), timing as u32) }
    }
    fn play_transition(&mut self, s: Score, pattern: Embellishment) -> bool {
        unsafe { ee_conductorcpp_play_transition(self.ctx, s.0, pattern as u32) }
    }
    fn set_performance_volume(&mut self, amplitude: f32) {
        unsafe { ee_conductorcpp_set_volume(self.ctx, amplitude) }
    }
    fn set_groove_level(&mut self, level: i8) {
        unsafe { ee_conductorcpp_set_groove_level(self.ctx, level) }
    }
}

/// The score table, owned by the C++ `SEConductor` (created and destroyed with it).
pub struct EeConductor {
    busy: Cell<bool>,
    inner: UnsafeCell<Conductor>,
}

/// # Safety
/// `c` comes from `ee_conductor_create`, the C++ lock is held, `f` is valid.
unsafe fn with<R>(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    g: impl FnOnce(&mut Conductor, &mut CppHost, &mut ConductorFields) -> R,
) -> R {
    let c = unsafe { &*c };
    assert!(!c.busy.replace(true), "ee_sound: the conductor was re-entered from a C++ callback");
    let r = g(unsafe { &mut *c.inner.get() }, &mut CppHost { ctx }, unsafe { &mut *f });
    c.busy.set(false);
    r
}

/// An empty score table (the constructor).
#[no_mangle]
pub extern "C" fn ee_conductor_create() -> *mut EeConductor {
    Box::into_raw(Box::new(EeConductor { busy: Cell::new(false), inner: UnsafeCell::new(Conductor::new()) }))
}

/// Frees the table; its scores were deleted by the stop before (a stopped conductor holds none).
///
/// # Safety
/// `c` from `ee_conductor_create`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_destroy(c: *mut EeConductor) {
    drop(unsafe { Box::from_raw(c) });
}

/// The constructor's scalar members.
#[no_mangle]
pub extern "C" fn ee_conductor_fields_new() -> ConductorFields {
    ConductorFields::new()
}

/// `LoadScore(name, outID, secondary)`; `written` says whether the C++ wrote `outID` (then `out_id` holds it).
///
/// # Safety
/// As `with`; `name` NUL-terminated, the outs valid.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_load_score(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    name: *const c_char,
    secondary: bool,
    out_id: *mut i32,
    written: *mut bool,
) -> i32 {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    let (r, id) = unsafe { with(c, ctx, f, |s, h, f| s.load_score(h, f, name, secondary)) };
    unsafe { *written = id.is_some() };
    if let Some(id) = id {
        unsafe { *out_id = id };
    }
    r
}

/// `SetConductorState`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_set_state(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    state: u32,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.set_state(h, f, state)) }
}

/// `InitializeConductor`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_initialize(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.initialize(h, f)) }
}

/// `UninitializeConductor`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_uninitialize(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.uninitialize(h, f)) }
}

/// `Play`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_play(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    id: i32,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.play(h, f, id)) }
}

/// `Queue`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_queue(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    id: i32,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.queue(h, f, id)) }
}

/// `Stop`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_stop(c: *mut EeConductor, ctx: *mut c_void, f: *mut ConductorFields) -> i32 {
    unsafe { with(c, ctx, f, |_, h, f| Conductor::stop(h, f)) }
}

/// `TransitionTo(id, command, flags)` (the flags are unused).
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_transition_to(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    id: i32,
    command: u32,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.transition_to(h, f, id, command)) }
}

/// `UnloadScore`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_unload_score(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    id: i32,
) -> i32 {
    unsafe { with(c, ctx, f, |s, h, f| s.unload_score(h, f, id)) }
}

/// `SetVolume`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_set_volume(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    volume: f32,
) -> i32 {
    unsafe { with(c, ctx, f, |_, h, f| Conductor::set_volume(h, f, volume)) }
}

/// `SetIntensity`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_set_intensity(
    c: *mut EeConductor,
    ctx: *mut c_void,
    f: *mut ConductorFields,
    intensity: u32,
    immediate: bool,
) -> i32 {
    unsafe { with(c, ctx, f, |_, h, f| Conductor::set_intensity(h, f, intensity, immediate)) }
}

/// `SetChannelGroupCount`.
///
/// # Safety
/// `f` valid, the C++ lock held.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_set_channel_group_count(f: *mut ConductorFields, count: u32) -> i32 {
    Conductor::set_channel_group_count(unsafe { &mut *f }, count)
}

/// `GetCurrentlyPlayingScore`.
///
/// # Safety
/// `f` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_currently_playing(f: *const ConductorFields) -> i32 {
    Conductor::currently_playing(unsafe { &*f })
}

/// `GetScore`: the `SEScore*` (null: none).
///
/// # Safety
/// As `with` (no host is reached).
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_score(c: *mut EeConductor, id: i32) -> *mut c_void {
    let mut f = ConductorFields::new();
    unsafe { with(c, std::ptr::null_mut(), &mut f, |s, _, _| s.score(id)) }.map_or(std::ptr::null_mut(), |s| s.0)
}

/// `GetScoreID`.
///
/// # Safety
/// As `with` (no host is reached).
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_score_id(c: *mut EeConductor, score: *mut c_void) -> i32 {
    let mut f = ConductorFields::new();
    let s = (!score.is_null()).then_some(Score(score));
    unsafe { with(c, std::ptr::null_mut(), &mut f, |t, _, _| t.score_id(s)) }
}

/// One entry of the score table, for inspection (tests, the golden driver): false past the last. `name` gets at
/// most `name_capacity - 1` bytes and a NUL.
///
/// # Safety
/// As `with`; `name` has `name_capacity` bytes (> 0), the outs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_conductor_entry(
    c: *mut EeConductor,
    index: u32,
    name: *mut c_char,
    name_capacity: u32,
    score: *mut *mut c_void,
    id: *mut i32,
    load_count: *mut u32,
) -> bool {
    let mut f = ConductorFields::new();
    unsafe {
        with(c, std::ptr::null_mut(), &mut f, |s, _, _| match s.scores.get(index as usize) {
            Some(e) => {
                let n = e.name.len().min(name_capacity as usize - 1);
                std::ptr::copy_nonoverlapping(e.name.as_ptr(), name as *mut u8, n);
                *name.add(n) = 0;
                *score = e.score.map_or(std::ptr::null_mut(), |s| s.0);
                *id = e.id;
                *load_count = e.load_count;
                true
            }
            None => false,
        })
    }
}
