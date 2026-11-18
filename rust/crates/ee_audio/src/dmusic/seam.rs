//! The conductor's calls into the music engine, under the C names and conventions of dmusic's API (`DmLoader_*`,
//! `DmSegment_*`, `DmPerformance_*`: opaque pointers, `DmResult` codes, a `malloc`'d resolver buffer), so that
//! `conductorobj.rs` and the Sound Engine objects' golden replay (`golden_objects.rs`, which defines these names as
//! the driver's fakes) are unchanged. `sound::sys` re-exports them outside the tests. They are Rust functions, not
//! exported to C.
#![allow(non_snake_case)]
#![allow(clippy::missing_safety_doc)] // each is `sound::sys`'s seam: the conductor's pointers, as dmusic's API took them

use std::ffi::{c_char, c_int, c_void, CStr};
use std::sync::{Arc, Mutex};

use super::files::Segment;
use super::loader::{CResolve, Loader};
use super::perf::Performance;
use super::synth::Synth;

/// `DmResult`'s.
const SUCCESS: c_int = 0;
const INVALID_ARGUMENT: c_int = 1;
const NOT_FOUND: c_int = 2;
const FILE_CORRUPT: c_int = 3;

/// `DmRender_FLOAT | DmRender_STEREO`, the only format the conductor asks for.
const RENDER_FLOAT_STEREO: c_int = 2 | 4;

type Perf = Mutex<Performance<Synth>>;

pub unsafe fn DmLoader_create(out: *mut *mut c_void, _options: c_int) -> c_int {
    if out.is_null() {
        return INVALID_ARGUMENT;
    }
    unsafe { *out = Box::into_raw(Box::new(Loader::default())).cast() };
    SUCCESS
}

pub unsafe fn DmLoader_release(loader: *mut c_void) {
    if !loader.is_null() {
        drop(unsafe { Box::from_raw(loader as *mut Loader) });
    }
}

pub unsafe fn DmLoader_addResolver(loader: *mut c_void, resolve: CResolve, context: *mut c_void) -> c_int {
    if loader.is_null() {
        return INVALID_ARGUMENT;
    }
    unsafe { (*(loader as *const Loader)).add_c_resolver(resolve, context) };
    SUCCESS
}

pub unsafe fn DmLoader_getSegment(loader: *mut c_void, name: *const c_char, out: *mut *mut c_void) -> c_int {
    if loader.is_null() || name.is_null() || out.is_null() {
        return INVALID_ARGUMENT;
    }
    let loader = unsafe { &*(loader as *const Loader) };
    let name = unsafe { CStr::from_ptr(name) }.to_string_lossy();
    match loader.segment(&name) {
        Ok(s) => {
            unsafe { *out = Arc::into_raw(s) as *mut c_void };
            SUCCESS
        }
        Err(super::loader::Error::NotFound) => NOT_FOUND,
        Err(super::loader::Error::FileCorrupt) => FILE_CORRUPT,
        Err(super::loader::Error::InvalidArgument) => INVALID_ARGUMENT,
    }
}

/// The segment behind a handle, one more reference.
unsafe fn segment(s: *const c_void) -> Option<Arc<Segment>> {
    if s.is_null() {
        return None;
    }
    let s = s as *const Segment;
    unsafe { Arc::increment_strong_count(s) };
    Some(unsafe { Arc::from_raw(s) })
}

pub unsafe fn DmSegment_release(s: *mut c_void) {
    if !s.is_null() {
        drop(unsafe { Arc::from_raw(s as *const Segment) });
    }
}

pub unsafe fn DmSegment_getRepeats(s: *const c_void) -> u32 {
    if s.is_null() {
        return 0;
    }
    unsafe { (*(s as *const Segment)).repeats() }
}

pub unsafe fn DmSegment_setRepeats(s: *mut c_void, repeats: u32) {
    if !s.is_null() {
        unsafe { (*(s as *const Segment)).repeats.store(repeats, std::sync::atomic::Ordering::SeqCst) };
    }
}

pub unsafe fn DmPerformance_create(out: *mut *mut c_void, rate: u32) -> c_int {
    if out.is_null() {
        return INVALID_ARGUMENT;
    }
    let rate = if rate == 0 { 44100 } else { rate };
    let p: Perf = Mutex::new(Performance::new(rate, Synth::new(rate)));
    unsafe { *out = Box::into_raw(Box::new(p)).cast() };
    SUCCESS
}

pub unsafe fn DmPerformance_release(p: *mut c_void) {
    if !p.is_null() {
        drop(unsafe { Box::from_raw(p as *mut Perf) });
    }
}

/// The performance under its lock (the render thread and the game's both call it).
unsafe fn with<R>(p: *mut c_void, f: impl FnOnce(&mut Performance<Synth>) -> R) -> Option<R> {
    if p.is_null() {
        return None;
    }
    let p = unsafe { &*(p as *const Perf) };
    let mut guard = p.lock().unwrap_or_else(|e| e.into_inner());
    Some(f(&mut guard))
}

pub unsafe fn DmPerformance_playSegment(p: *mut c_void, s: *mut c_void, timing: c_int) -> c_int {
    let s = unsafe { segment(s) };
    match unsafe { with(p, |p| p.play_segment(s, timing)) } {
        Some(true) => SUCCESS,
        _ => INVALID_ARGUMENT,
    }
}

pub unsafe fn DmPerformance_playTransition(
    p: *mut c_void,
    s: *mut c_void,
    embellishment: c_int,
    timing: c_int,
) -> c_int {
    let Some(s) = (unsafe { segment(s) }) else { return INVALID_ARGUMENT };
    match unsafe { with(p, |p| p.play_transition(s, embellishment, timing)) } {
        Some(true) => SUCCESS,
        _ => INVALID_ARGUMENT,
    }
}

pub unsafe fn DmPerformance_setVolume(p: *mut c_void, volume: f32) {
    unsafe { with(p, |p| p.set_volume(volume)) };
}

pub unsafe fn DmPerformance_setGrooveLevel(p: *mut c_void, level: u8) {
    unsafe { with(p, |p| p.set_groove_level(level)) };
}

pub unsafe fn DmPerformance_stop(p: *mut c_void, lookahead_ms: u32) {
    unsafe { with(p, |p| p.stop(lookahead_ms)) };
}

pub unsafe fn DmPerformance_setReverb(p: *mut c_void, enabled: c_int, gain: f32, mix: f32, time: f32, hf: f32) {
    unsafe { with(p, |p| p.set_reverb(enabled != 0, gain, mix, time, hf)) };
}

/// `count` samples (2 a frame) of interleaved float stereo; anything else is refused, as dmusic refused an odd count.
pub unsafe fn DmPerformance_renderPcm(p: *mut c_void, buffer: *mut c_void, count: usize, options: c_int) -> c_int {
    if buffer.is_null() || options != RENDER_FLOAT_STEREO || !count.is_multiple_of(2) {
        return INVALID_ARGUMENT;
    }
    let out = unsafe { std::slice::from_raw_parts_mut(buffer as *mut f32, count) };
    match unsafe { with(p, |p| p.render(out)) } {
        Some(()) => SUCCESS,
        None => INVALID_ARGUMENT,
    }
}

/// The general MIDI set built into the program and its length (the conductor's resolver hands it out for a
/// `gm.sf2` the music folder lacks).
pub unsafe fn SEGeneralMIDISet(length: *mut usize) -> *const u8 {
    unsafe { *length = super::GENERAL_MIDI.len() };
    super::GENERAL_MIDI.as_ptr()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ptr::null_mut;

    unsafe extern "C" fn none(_: *mut c_void, _: *const c_char, _: *mut usize) -> *mut c_void {
        null_mut()
    }

    #[test]
    fn handles_and_results() {
        unsafe {
            let mut l = null_mut();
            assert_eq!(DmLoader_create(&mut l, 1), SUCCESS);
            assert_eq!(DmLoader_addResolver(l, none, null_mut()), SUCCESS);
            let mut s = null_mut();
            assert_eq!(DmLoader_getSegment(l, c"x.sgt".as_ptr(), &mut s), NOT_FOUND);
            let mut p = null_mut();
            assert_eq!(DmPerformance_create(&mut p, 48000), SUCCESS);
            assert_eq!(DmPerformance_playSegment(p, null_mut(), 4), SUCCESS, "a stop at the next measure");
            assert_eq!(DmPerformance_playTransition(p, null_mut(), 1, 4), INVALID_ARGUMENT);
            let mut buf = [1f32; 64];
            assert_eq!(DmPerformance_renderPcm(p, buf.as_mut_ptr().cast(), 64, RENDER_FLOAT_STEREO), SUCCESS);
            assert!(buf.iter().all(|&v| v == 0.0), "no instruments: silence");
            assert_eq!(DmPerformance_renderPcm(p, buf.as_mut_ptr().cast(), 63, RENDER_FLOAT_STEREO), INVALID_ARGUMENT);
            DmPerformance_setReverb(p, 1, 0.0, -10.0, 1000.0, 0.001);
            DmPerformance_stop(p, 200);
            DmPerformance_release(p);
            DmLoader_release(l);
            let mut n = 0;
            let set = SEGeneralMIDISet(&mut n);
            assert_eq!(std::slice::from_raw_parts(set, 4), b"RIFF");
            assert_eq!(n, crate::dmusic::GENERAL_MIDI.len());
        }
    }
}
