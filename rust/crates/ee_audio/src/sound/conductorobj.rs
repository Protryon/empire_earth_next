//! `SEConductor` whole in Rust: music score playback on the music engine (`crate::dmusic`: a DirectMusic
//! performance on rustysynth, ported from dmusic, reached under dmusic's C names through `sys`). The conductor lives inside its mixer ([`crate::mixerobj::EeSoundMixer`]); an
//! `SEConductor*` is an [`EeSoundConductor`] and the C++ class is data-less. The decisions are `conductor.rs`'s; this
//! module holds the engine's loader and performance, the scores ([`crate::objects::EeSoundScore`]), the scalar members,
//! the music folder and the lock (a Thread System section, taken where the C++ took it). The performance renders
//! into the mixer's output: the mixer's device thread calls [`render`] (`SEMixer::SetMusicSource`), reading the
//! performance without the lock, as the original did; it is an atomic here.

use std::cell::UnsafeCell;
use std::ffi::{c_char, c_ulong, c_void, CStr};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::sound::conductor::{Conductor, ConductorFields, Embellishment, Host, Score, Timing};
use crate::sound::mixerobj::{self, EeSoundMixer};
use crate::sound::objects::{self, EeSoundScore};
use crate::sound::sys;

/// `MAX_PATH`: the resolver's path buffer.
const MAX_PATH: usize = 260;

struct Inner {
    table: Conductor,
    fields: ConductorFields,
    /// The folder the score files are read from, NUL-terminated.
    folder: Vec<u8>,
    /// `SetNotificationCallback`'s callbacks (dmusic reports no event: never called).
    callbacks: [*mut c_void; 6],
    mixer: *mut EeSoundMixer,
    loader: *mut c_void,
}

/// The conductor.
pub struct EeSoundConductor {
    lock: *mut c_void,
    performance: AtomicPtr<c_void>,
    inner: UnsafeCell<Inner>,
}

/// The conductor's side of the decisions: dmusic, the scores, the mixer.
struct Dm<'a> {
    c: &'a EeSoundConductor,
    i: &'a mut Inner,
}

impl Host for Dm<'_> {
    fn output_rate(&mut self) -> u32 {
        if self.i.mixer.is_null() {
            0
        } else {
            unsafe { mixerobj::ee_sound_mixer_output_rate(self.i.mixer) as u32 }
        }
    }
    fn start(&mut self, rate: u32) -> bool {
        let mut loader = null_mut();
        let mut performance = null_mut();
        unsafe {
            if sys::DmLoader_create(&mut loader, sys::DM_LOADER_DOWNLOAD) != sys::DM_SUCCESS {
                return false;
            }
            let me = self.c as *const EeSoundConductor as *mut c_void;
            if sys::DmLoader_addResolver(loader, resolve, me) == sys::DM_SUCCESS
                && sys::DmPerformance_create(&mut performance, rate) == sys::DM_SUCCESS
            {
                self.i.loader = loader;
                self.c.performance.store(performance, Ordering::SeqCst);
                return true;
            }
            sys::DmLoader_release(loader);
        }
        false
    }
    fn release(&mut self) {
        unsafe {
            sys::DmPerformance_release(self.c.performance.load(Ordering::SeqCst));
            sys::DmLoader_release(self.i.loader);
        }
        self.c.performance.store(null_mut(), Ordering::SeqCst);
        self.i.loader = null_mut();
    }
    fn set_music_source(&mut self, on: bool) {
        let me = self.c as *const EeSoundConductor as *mut c_void;
        if on {
            unsafe { mixerobj::ee_sound_mixer_set_music_source(self.i.mixer, Some(render), me) }
        } else if !self.i.mixer.is_null() {
            // the device thread no longer renders after this returns
            unsafe { mixerobj::ee_sound_mixer_set_music_source(self.i.mixer, None, null_mut()) }
        }
    }
    fn load(&mut self, name: &[u8]) -> Option<Score> {
        let mut z = name.to_vec();
        z.push(0);
        let mut segment = null_mut();
        let r = unsafe { sys::DmLoader_getSegment(self.i.loader, z.as_ptr() as *const c_char, &mut segment) };
        if r != sys::DM_SUCCESS || segment.is_null() {
            return None;
        }
        Some(Score(objects::score_new(segment, false).cast()))
    }
    fn delete(&mut self, s: Score) {
        unsafe { objects::score_delete(s.0.cast()) }
    }
    fn is_secondary(&self, s: Score) -> bool {
        unsafe { (*(s.0 as *const EeSoundScore)).secondary }
    }
    fn play(&mut self, s: Option<Score>, timing: Timing) -> bool {
        let segment = s.map_or(null_mut(), |s| unsafe { (*(s.0 as *const EeSoundScore)).segment });
        let p = self.c.performance.load(Ordering::SeqCst);
        unsafe { sys::DmPerformance_playSegment(p, segment, timing as i32) == sys::DM_SUCCESS }
    }
    fn play_transition(&mut self, s: Score, pattern: Embellishment) -> bool {
        let segment = unsafe { (*(s.0 as *const EeSoundScore)).segment };
        let p = self.c.performance.load(Ordering::SeqCst);
        unsafe {
            sys::DmPerformance_playTransition(p, segment, pattern as i32, sys::DM_TIMING_MEASURE) == sys::DM_SUCCESS
        }
    }
    fn set_performance_volume(&mut self, amplitude: f32) {
        unsafe { sys::DmPerformance_setVolume(self.c.performance.load(Ordering::SeqCst), amplitude) }
    }
    fn set_groove_level(&mut self, level: u8) {
        unsafe { sys::DmPerformance_setGrooveLevel(self.c.performance.load(Ordering::SeqCst), level) }
    }
    fn stop_all(&mut self, lookahead_ms: u32) {
        unsafe { sys::DmPerformance_stop(self.c.performance.load(Ordering::SeqCst), lookahead_ms) }
    }
    fn set_reverb(&mut self, enabled: bool, gain: f32, mix: f32, time: f32, hf_ratio: f32) {
        let p = self.c.performance.load(Ordering::SeqCst);
        unsafe { sys::DmPerformance_setReverb(p, enabled as i32, gain, mix, time, hf_ratio) }
    }
}

impl EeSoundConductor {
    /// The constructor: its lock, volume 1, reverb on, stopped, no scores.
    pub(crate) fn new() -> Self {
        let lock = unsafe { sys::ee_ts_section_new() };
        EeSoundConductor {
            lock,
            performance: AtomicPtr::new(null_mut()),
            inner: UnsafeCell::new(Inner {
                table: Conductor::new(),
                fields: ConductorFields::new(),
                folder: vec![0],
                callbacks: [null_mut(); 6],
                mixer: null_mut(),
                loader: null_mut(),
            }),
        }
    }

    pub(crate) fn set_mixer(&self, m: *mut EeSoundMixer) {
        unsafe { (*self.inner.get()).mixer = m };
    }

    /// Runs `f` under the lock with the decisions' view.
    fn with<R>(&self, f: impl FnOnce(&mut Conductor, &mut Dm, &mut ConductorFields) -> R) -> R {
        sys::locked(self.lock, || {
            let i = unsafe { &mut *self.inner.get() };
            let mut table = std::mem::take(&mut i.table);
            let mut fields = i.fields;
            let r = {
                let mut dm = Dm { c: self, i };
                f(&mut table, &mut dm, &mut fields)
            };
            let i = unsafe { &mut *self.inner.get() };
            i.table = table;
            i.fields = fields;
            r
        })
    }

    fn fields(&self) -> ConductorFields {
        unsafe { (*self.inner.get()).fields }
    }

    /// The destructor: stopped first if running (a stopped conductor holds no score), then the lock.
    ///
    /// # Safety
    /// Not used afterwards.
    pub(crate) unsafe fn destroy(&self) {
        if self.fields().state == 1 {
            self.with(|t, h, f| t.uninitialize(h, f));
        }
        unsafe { sys::ee_ts_section_free(self.lock) };
    }
}

/// The mixer's device thread: the next `count` stereo frames of music (interleaved float).
unsafe extern "C" fn render(conductor: *mut c_void, frames: *mut f32, count: c_ulong) {
    let c = unsafe { &*(conductor as *const EeSoundConductor) };
    let p = c.performance.load(Ordering::SeqCst);
    if !p.is_null() {
        unsafe { sys::DmPerformance_renderPcm(p, frames.cast(), 2 * count as usize, sys::DM_RENDER_FLOAT_STEREO) };
    }
}

/// The music engine's loader: a score file (segment, style or instrument collection: a SoundFont, `X.sf2` for the
/// original's `X.dls`) from the music folder in a `malloc`'d buffer (the engine frees it); NULL when it cannot be
/// opened (`length` untouched then). A `gm.sf2` the folder lacks is the general MIDI set built into the program
/// (`crate::dmusic::GENERAL_MIDI`, EE.sf2; main 32b4344f4: the original took the system's `gm.dls`).
unsafe extern "C" fn resolve(conductor: *mut c_void, file: *const c_char, length: *mut usize) -> *mut c_void {
    let c = unsafe { &*(conductor as *const EeSoundConductor) };
    let folder = unsafe { &(*c.inner.get()).folder };
    let folder = &folder[..folder.len() - 1];
    let file = unsafe { CStr::from_ptr(file) }.to_bytes();
    let mut path = Vec::with_capacity(MAX_PATH);
    if !folder.is_empty() {
        path.extend_from_slice(folder);
        path.push(b'\\');
    }
    path.extend_from_slice(file);
    path.truncate(MAX_PATH - 1);
    path.push(0);
    unsafe {
        let stream = sys::PlatFOpen(path.as_ptr() as *const c_char, c"rb".as_ptr());
        if stream.is_null() {
            if !file.eq_ignore_ascii_case(b"gm.sf2") {
                return null_mut();
            }
            let mut n = 0usize;
            let set = sys::SEGeneralMIDISet(&mut n);
            let copy = sys::malloc(n);
            if !copy.is_null() {
                std::ptr::copy_nonoverlapping(set, copy.cast::<u8>(), n);
            }
            *length = if copy.is_null() { 0 } else { n };
            return copy;
        }
        sys::fseek(stream, 0, 2);
        let n = sys::ftell(stream);
        sys::fseek(stream, 0, 0);
        let mut data = if n > 0 { sys::malloc(n as usize) } else { null_mut() };
        if !data.is_null() && sys::fread(data, 1, n as usize, stream) != n as usize {
            sys::free(data);
            data = null_mut();
        }
        sys::fclose(stream);
        *length = if data.is_null() { 0 } else { n as usize };
        data
    }
}

/// `SEConductor::GetCurrentlyPlayingScore` (-1: none or not running).
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_playing(c: *const EeSoundConductor) -> i32 {
    unsafe { &*c }.with(|_, _, f| Conductor::currently_playing(f))
}

/// `GetScore` (NULL: none or not loaded).
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_score(c: *const EeSoundConductor, id: i32) -> *mut EeSoundScore {
    unsafe { &*c }.with(|t, _, _| t.score(id).map_or(null_mut(), |s| s.0.cast()))
}

/// `GetScoreID` (-1: not found or NULL).
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_score_id(c: *const EeSoundConductor, score: *mut EeSoundScore) -> i32 {
    unsafe { &*c }.with(|t, _, _| t.score_id((!score.is_null()).then_some(Score(score.cast()))))
}

/// `LoadScore(name, outID)`: `out_id` is written when the C++ wrote it.
///
/// # Safety
/// `c` is a live conductor, `name` NUL-terminated, `out_id` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_load_score(
    c: *const EeSoundConductor,
    name: *const c_char,
    out_id: *mut i32,
) -> i32 {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    let (r, id) = unsafe { &*c }.with(|t, h, f| t.load_score(h, f, name, false));
    if let Some(id) = id {
        unsafe { *out_id = id };
    }
    r
}

/// `Play`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_play(c: *const EeSoundConductor, id: i32) -> i32 {
    unsafe { &*c }.with(|t, h, f| t.play(h, f, id))
}

/// `Queue`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_queue(c: *const EeSoundConductor, id: i32) -> i32 {
    unsafe { &*c }.with(|t, h, f| t.queue(h, f, id))
}

/// `SetChannelGroupCount`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_channel_group_count(c: *const EeSoundConductor, count: u32) -> i32 {
    unsafe { &*c }.with(|_, _, f| Conductor::set_channel_group_count(f, count))
}

/// `SetConductorState`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_state(c: *const EeSoundConductor, state: u32) -> i32 {
    unsafe { &*c }.with(|t, h, f| t.set_state(h, f, state))
}

/// `SetIntensity`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_intensity(
    c: *const EeSoundConductor,
    intensity: u32,
    immediate: bool,
) -> i32 {
    unsafe { &*c }.with(|_, h, f| Conductor::set_intensity(h, f, intensity, immediate))
}

/// `SetMusicFolder`: the name without its last character (the trailing separator).
///
/// # Safety
/// `c` is a live conductor, `folder` NUL-terminated (UTF-8).
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_music_folder(c: *const EeSoundConductor, folder: *const c_char) -> i32 {
    let mut v = unsafe { CStr::from_ptr(folder) }.to_bytes().to_vec();
    // the last character: back over UTF-8 continuation bytes to its lead byte
    if let Some(mut k) = v.len().checked_sub(1) {
        while k > 0 && v[k] & 0xc0 == 0x80 {
            k -= 1;
        }
        v.truncate(k);
    }
    v.push(0);
    let c = unsafe { &*c };
    sys::locked(c.lock, || unsafe { (*c.inner.get()).folder = v });
    0
}

/// `GetMusicFolder`: NUL-terminated, valid until the next `SetMusicFolder`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_music_folder(c: *const EeSoundConductor) -> *const c_char {
    unsafe { (*(*c).inner.get()).folder.as_ptr() as *const c_char }
}

/// `SetReverb`: on (1) or off, in the style last set; while stopped only the state is kept.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_reverb(c: *const EeSoundConductor, reverb: u32) -> i32 {
    unsafe { &*c }.with(|_, h, f| Conductor::set_reverb(h, f, reverb))
}

/// `SetReverbStyle(gain, hf_ratio, mix, time)`: kept and applied in the reverb's current state.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_reverb_style(
    c: *const EeSoundConductor,
    gain: f32,
    hf_ratio: f32,
    mix: f32,
    time: f32,
) -> i32 {
    unsafe { &*c }.with(|_, h, f| Conductor::set_reverb_style(h, f, gain, hf_ratio, mix, time))
}

/// `SetVolume`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_volume(c: *const EeSoundConductor, volume: f32) -> i32 {
    unsafe { &*c }.with(|_, h, f| Conductor::set_volume(h, f, volume))
}

/// `Stop`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_stop(c: *const EeSoundConductor) -> i32 {
    unsafe { &*c }.with(|_, h, f| Conductor::stop(h, f))
}

/// `TransitionTo(id, command, flags)` (the flags are not used by dmusic).
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_transition_to(c: *const EeSoundConductor, id: i32, command: u32) -> i32 {
    unsafe { &*c }.with(|t, h, f| t.transition_to(h, f, id, command))
}

/// `UnloadScore`.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_unload_score(c: *const EeSoundConductor, id: i32) -> i32 {
    unsafe { &*c }.with(|t, h, f| t.unload_score(h, f, id))
}

/// The scalar members the game reads (no lock, as the original's inline getters): volume.
///
/// # Safety
/// `c` is a live conductor.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_fields(c: *const EeSoundConductor) -> ConductorFields {
    unsafe { &*c }.fields()
}

/// `SetNotificationCallback` (kept for the interface: dmusic reports no event).
///
/// # Safety
/// `c` is a live conductor, `event` < 6.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_conductor_set_callback(
    c: *const EeSoundConductor,
    event: u32,
    callback: *mut c_void,
) {
    unsafe { (*(*c).inner.get()).callbacks[event as usize] = callback };
}

#[cfg(test)]
mod tests {
    #[test]
    fn the_folder_loses_its_last_character() {
        // the rule ee_sound_conductor_set_music_folder applies (exercised with the lock by the golden replay)
        fn cut(s: &[u8]) -> Vec<u8> {
            let mut v = s.to_vec();
            if let Some(mut k) = v.len().checked_sub(1) {
                while k > 0 && v[k] & 0xc0 == 0x80 {
                    k -= 1;
                }
                v.truncate(k);
            }
            v
        }
        assert_eq!(cut(b"music\\"), b"music");
        assert_eq!(cut(b""), b"");
        assert_eq!(cut("m\u{e9}".as_bytes()), b"m");
        assert_eq!(cut("\u{1f600}".as_bytes()), b"");
    }
}
