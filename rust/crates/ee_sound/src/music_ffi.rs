//! The C ABI of `EEMusicManager.cpp` and the C++ trampolines (`ee_musiccpp_*`, defined there) through which the
//! music manager's decisions reach its members, the conductor, the songs, the world and the preferences.
//! Compiled out of `cargo test`. `ctx` is the C++ `EEMusicManager*`. The manager has no Rust state: the callers'
//! threads call in as they called the C++ (the original took no lock either).

use std::ffi::c_void;

use crate::conductor::Score;
use crate::music::{self, Host, Song};

/// cbindgen:ignore
mod sys {
    use super::*;
    extern "C" {
        pub(super) fn ee_musiccpp_get(ctx: *mut c_void, field: u32) -> u32;
        pub(super) fn ee_musiccpp_set(ctx: *mut c_void, field: u32, value: u32);
        pub(super) fn ee_musiccpp_music(ctx: *mut c_void) -> *mut c_void;
        pub(super) fn ee_musiccpp_set_music(ctx: *mut c_void, song: *mut c_void);
        pub(super) fn ee_musiccpp_random(ctx: *mut c_void) -> f32;
        pub(super) fn ee_musiccpp_request_stop(ctx: *mut c_void);
        pub(super) fn ee_musiccpp_last_error();
        pub(super) fn ee_musiccpp_current_epoch(epoch: *mut i32) -> bool;
        pub(super) fn ee_musiccpp_music_volume() -> f32;
        pub(super) fn ee_musiccpp_sleep(ms: u32);
        pub(super) fn ee_musiccpp_random_song(epoch: i32) -> *mut c_void;
        pub(super) fn ee_musiccpp_song_load(song: *mut c_void);
        pub(super) fn ee_musiccpp_song_score_id(song: *mut c_void) -> i32;
        pub(super) fn ee_musiccpp_song_unload(song: *mut c_void) -> i32;
        pub(super) fn ee_musiccpp_song_reverb(song: *mut c_void, out: *mut f32);
        pub(super) fn ee_musiccpp_volume(ctx: *mut c_void) -> f32;
        pub(super) fn ee_musiccpp_set_volume(ctx: *mut c_void, v: f32);
        pub(super) fn ee_musiccpp_stop(ctx: *mut c_void);
        pub(super) fn ee_musiccpp_score(ctx: *mut c_void, id: i32) -> *mut c_void;
        pub(super) fn ee_musiccpp_set_repeat_count(score: *mut c_void, count: u32);
        pub(super) fn ee_musiccpp_set_intensity(ctx: *mut c_void, intensity: u32, immediate: bool);
        pub(super) fn ee_musiccpp_set_reverb_style(ctx: *mut c_void, reverb: *const f32);
        pub(super) fn ee_musiccpp_play(ctx: *mut c_void, id: i32);
        pub(super) fn ee_musiccpp_transition_to(ctx: *mut c_void, id: i32, command: u32, flags: u32);
        pub(super) fn ee_musiccpp_global_set_intensity(intensity: u32, immediate: bool);
    }
}
use sys::*;

/// The members `ee_musiccpp_get`/`set` reach (as u32 bits; the signed ones are `long`).
const SCORE_ID: u32 = 0;
const STATE: u32 = 1;
const INTENSITY_DELAY: u32 = 2;
const INTENSITY: u32 = 3;
const TIME_TO_NEXT_SONG: u32 = 4;
const REQUESTED_INTENSITY: u32 = 5;

struct CppHost {
    ctx: *mut c_void,
}

fn song(p: *mut c_void) -> Option<Song> {
    (!p.is_null()).then_some(Song(p))
}

impl CppHost {
    fn get(&self, f: u32) -> u32 {
        unsafe { ee_musiccpp_get(self.ctx, f) }
    }
    fn put(&mut self, f: u32, v: u32) {
        unsafe { ee_musiccpp_set(self.ctx, f, v) }
    }
}

impl Host for CppHost {
    fn music(&self) -> Option<Song> {
        song(unsafe { ee_musiccpp_music(self.ctx) })
    }
    fn set_music(&mut self, s: Option<Song>) {
        unsafe { ee_musiccpp_set_music(self.ctx, s.map_or(std::ptr::null_mut(), |s| s.0)) }
    }
    fn score_id(&self) -> i32 {
        self.get(SCORE_ID) as i32
    }
    fn set_score_id(&mut self, id: i32) {
        self.put(SCORE_ID, id as u32)
    }
    fn state(&self) -> i32 {
        self.get(STATE) as i32
    }
    fn set_state(&mut self, s: i32) {
        self.put(STATE, s as u32)
    }
    fn intensity_delay(&self) -> u32 {
        self.get(INTENSITY_DELAY)
    }
    fn set_intensity_delay(&mut self, d: u32) {
        self.put(INTENSITY_DELAY, d)
    }
    fn intensity(&self) -> u32 {
        self.get(INTENSITY)
    }
    fn set_intensity(&mut self, i: u32) {
        self.put(INTENSITY, i)
    }
    fn time_to_next_song(&self) -> u32 {
        self.get(TIME_TO_NEXT_SONG)
    }
    fn set_time_to_next_song(&mut self, t: u32) {
        self.put(TIME_TO_NEXT_SONG, t)
    }
    fn requested_intensity(&self) -> u32 {
        self.get(REQUESTED_INTENSITY)
    }
    fn set_requested_intensity(&mut self, i: u32) {
        self.put(REQUESTED_INTENSITY, i)
    }
    fn random(&mut self) -> f32 {
        unsafe { ee_musiccpp_random(self.ctx) }
    }
    fn request_stop(&mut self) {
        unsafe { ee_musiccpp_request_stop(self.ctx) }
    }
    fn last_error(&mut self) {
        unsafe { ee_musiccpp_last_error() }
    }
    fn current_epoch(&mut self) -> Option<i32> {
        let mut e = 0;
        unsafe { ee_musiccpp_current_epoch(&mut e) }.then_some(e)
    }
    fn music_volume(&self) -> f32 {
        unsafe { ee_musiccpp_music_volume() }
    }
    fn sleep(&mut self, ms: u32) {
        unsafe { ee_musiccpp_sleep(ms) }
    }
    fn random_song(&mut self, epoch: i32) -> Option<Song> {
        song(unsafe { ee_musiccpp_random_song(epoch) })
    }
    fn song_load(&mut self, s: Song) {
        unsafe { ee_musiccpp_song_load(s.0) }
    }
    fn song_score_id(&self, s: Song) -> i32 {
        unsafe { ee_musiccpp_song_score_id(s.0) }
    }
    fn song_unload(&mut self, s: Song) -> i32 {
        unsafe { ee_musiccpp_song_unload(s.0) }
    }
    fn song_reverb(&self, s: Song) -> [f32; 4] {
        let mut r = [0.0f32; 4];
        unsafe { ee_musiccpp_song_reverb(s.0, r.as_mut_ptr()) };
        r
    }
    fn volume(&self) -> f32 {
        unsafe { ee_musiccpp_volume(self.ctx) }
    }
    fn set_volume(&mut self, v: f32) {
        unsafe { ee_musiccpp_set_volume(self.ctx, v) }
    }
    fn stop(&mut self) {
        unsafe { ee_musiccpp_stop(self.ctx) }
    }
    fn score(&mut self, id: i32) -> Option<Score> {
        let s = unsafe { ee_musiccpp_score(self.ctx, id) };
        (!s.is_null()).then_some(Score(s))
    }
    fn set_repeat_count(&mut self, s: Score, count: u32) {
        unsafe { ee_musiccpp_set_repeat_count(s.0, count) }
    }
    fn conductor_set_intensity(&mut self, intensity: u32, immediate: bool) {
        unsafe { ee_musiccpp_set_intensity(self.ctx, intensity, immediate) }
    }
    fn set_reverb_style(&mut self, reverb: [f32; 4]) {
        unsafe { ee_musiccpp_set_reverb_style(self.ctx, reverb.as_ptr()) }
    }
    fn play(&mut self, id: i32) {
        unsafe { ee_musiccpp_play(self.ctx, id) }
    }
    fn transition_to(&mut self, id: i32, command: u32, flags: u32) {
        unsafe { ee_musiccpp_transition_to(self.ctx, id, command, flags) }
    }
    fn global_set_intensity(&mut self, intensity: u32, immediate: bool) {
        unsafe { ee_musiccpp_global_set_intensity(intensity, immediate) }
    }
}

/// One wake-up of the music thread: `wait` is `TSEvent::WaitAny`'s result, `elapsed` the time since the last
/// one. True when the thread must end.
///
/// # Safety
/// `ctx` is the live `EEMusicManager*`, called from its thread.
#[no_mangle]
pub unsafe extern "C" fn ee_music_step(ctx: *mut c_void, wait: i32, elapsed: u32) -> bool {
    music::step(&mut CppHost { ctx }, wait, elapsed)
}

/// `SetIntensity`'s rule (the C++ sets the event after it).
///
/// # Safety
/// `ctx` is the live `EEMusicManager*`.
#[no_mangle]
pub unsafe extern "C" fn ee_music_request_intensity(ctx: *mut c_void, intensity: u32) {
    music::request_intensity(&mut CppHost { ctx }, intensity)
}

/// `DoPause`, `DoSetIntensity`, `DoStart`, `DoStop`, `NextSong`: 0..4.
///
/// # Safety
/// `ctx` is the live `EEMusicManager*`, called from its thread.
#[no_mangle]
pub unsafe extern "C" fn ee_music_do(ctx: *mut c_void, what: u32) {
    let h = &mut CppHost { ctx };
    match what {
        0 => music::do_pause(h),
        1 => music::do_set_intensity(h),
        2 => music::do_start(h),
        3 => music::do_stop(h),
        _ => music::next_song(h),
    }
}

/// `Update`.
///
/// # Safety
/// `ctx` is the live `EEMusicManager*`, called from its thread.
#[no_mangle]
pub unsafe extern "C" fn ee_music_update(ctx: *mut c_void, elapsed: u32) {
    music::update(&mut CppHost { ctx }, elapsed)
}
