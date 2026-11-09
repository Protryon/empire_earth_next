//! The C ABI of the mixer plug-in (called by the C++ shim, `Mixer.cpp`) and the `SESample` access through the shim's
//! `ee_mixer_host_*`. Compiled out of `cargo test`.

use std::ffi::{c_ulong, c_void};
use std::slice;
use std::sync::{Arc, OnceLock};

use crate::mixer::{Mixer, SampleData, SampleHandle, SampleHost, CHANNEL_NONE};

/// cbindgen:ignore
mod sys {
    use super::*;
    extern "C" {
        pub(super) fn ee_mixer_host_mixer_channel(sample: *mut c_void) -> u32;
        pub(super) fn ee_mixer_host_set_mixer_channel(sample: *mut c_void, channel: u32);
        pub(super) fn ee_mixer_host_data(sample: *mut c_void) -> *const c_void;
        pub(super) fn ee_mixer_host_set_data(sample: *mut c_void, data: *const c_void);
        pub(super) fn ee_mixer_host_is_looping(sample: *mut c_void) -> bool;
        pub(super) fn ee_mixer_host_pan(sample: *mut c_void) -> f32;
        pub(super) fn ee_mixer_host_pitch(sample: *mut c_void) -> f32;
        pub(super) fn ee_mixer_host_volume(sample: *mut c_void) -> f32;
        pub(super) fn ee_mixer_host_set_is_looping(sample: *mut c_void, looping: bool);
        pub(super) fn ee_mixer_host_set_pan(sample: *mut c_void, pan: f32);
        pub(super) fn ee_mixer_host_set_pitch(sample: *mut c_void, pitch: f32);
        pub(super) fn ee_mixer_host_set_volume(sample: *mut c_void, volume: f32);
    }
}
use sys::*;

struct PlatHost;

impl SampleHost for PlatHost {
    fn mixer_channel(&self, s: SampleHandle) -> u32 {
        unsafe { ee_mixer_host_mixer_channel(s) }
    }
    fn set_mixer_channel(&self, s: SampleHandle, channel: u32) {
        unsafe { ee_mixer_host_set_mixer_channel(s, channel) }
    }
    fn data(&self, s: SampleHandle) -> *const SampleData {
        unsafe { ee_mixer_host_data(s) as *const SampleData }
    }
    fn is_looping(&self, s: SampleHandle) -> bool {
        unsafe { ee_mixer_host_is_looping(s) }
    }
    fn pan(&self, s: SampleHandle) -> f32 {
        unsafe { ee_mixer_host_pan(s) }
    }
    fn pitch(&self, s: SampleHandle) -> f32 {
        unsafe { ee_mixer_host_pitch(s) }
    }
    fn volume(&self, s: SampleHandle) -> f32 {
        unsafe { ee_mixer_host_volume(s) }
    }
    fn set_is_looping(&self, s: SampleHandle, looping: bool) {
        unsafe { ee_mixer_host_set_is_looping(s, looping) }
    }
    fn set_pan(&self, s: SampleHandle, pan: f32) {
        unsafe { ee_mixer_host_set_pan(s, pan) }
    }
    fn set_pitch(&self, s: SampleHandle, pitch: f32) {
        unsafe { ee_mixer_host_set_pitch(s, pitch) }
    }
    fn set_volume(&self, s: SampleHandle, volume: f32) {
        unsafe { ee_mixer_host_set_volume(s, volume) }
    }
}

static MIXER: OnceLock<Mixer<PlatHost>> = OnceLock::new();

fn mixer() -> &'static Mixer<PlatHost> {
    MIXER.get_or_init(|| Mixer::new(PlatHost))
}

/// The device opened at `rate`.
#[no_mangle]
pub extern "C" fn ee_mixer_activated(rate: u32) {
    mixer().activated(rate);
}

/// The device closed.
#[no_mangle]
pub extern "C" fn ee_mixer_deactivated() {
    mixer().deactivated();
}

#[no_mangle]
pub extern "C" fn ee_mixer_is_active() -> bool {
    mixer().is_active()
}

#[no_mangle]
pub extern "C" fn ee_mixer_output_rate() -> u32 {
    mixer().output_rate()
}

#[no_mangle]
pub extern "C" fn ee_mixer_create_channels(count: u32) -> i32 {
    mixer().create_channels(count)
}

#[no_mangle]
pub extern "C" fn ee_mixer_destroy_channels() -> i32 {
    mixer().destroy_channels()
}

#[no_mangle]
pub extern "C" fn ee_mixer_channel_count() -> u32 {
    mixer().channel_count()
}

#[no_mangle]
pub extern "C" fn ee_mixer_maximum_channel_count() -> u32 {
    mixer().maximum_channel_count()
}

#[no_mangle]
pub extern "C" fn ee_mixer_is_channel_busy_playing(channel: u32) -> i32 {
    mixer().is_channel_busy_playing(channel)
}

#[no_mangle]
pub extern "C" fn ee_mixer_play_channel(channel: u32) -> i32 {
    mixer().play_channel(channel)
}

#[no_mangle]
pub extern "C" fn ee_mixer_stop_channel(channel: u32) -> i32 {
    mixer().stop_channel(channel)
}

#[no_mangle]
pub extern "C" fn ee_mixer_stop_all_channels() -> i32 {
    mixer().stop_all_channels()
}

/// `SPIMSetChannelSample`.
///
/// # Safety
/// `sample` is a live `SESample*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_channel_sample(channel: u32, sample: *mut c_void) -> i32 {
    mixer().set_channel_sample(channel, sample)
}

/// `SPIMSetSampleLooping`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_sample_looping(sample: *mut c_void, looping: bool) {
    mixer().set_sample_looping(sample, looping);
}

/// `SPIMSetSamplePan`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_sample_pan(sample: *mut c_void, pan: f32) -> i32 {
    mixer().set_sample_pan(sample, pan)
}

/// `SPIMSetSamplePitch`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_sample_pitch(sample: *mut c_void, pitch: f32) -> i32 {
    mixer().set_sample_pitch(sample, pitch)
}

/// `SPIMSetSampleVolume`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_sample_volume(sample: *mut c_void, volume: f32) -> i32 {
    mixer().set_sample_volume(sample, volume)
}

/// `SPIMGetSampleOffset`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_offset(sample: *mut c_void) -> u32 {
    mixer().sample_offset(sample)
}

/// `SPIMSetSampleOffset`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_sample_offset(sample: *mut c_void, offset: u32) -> i32 {
    mixer().set_sample_offset(sample, offset)
}

/// `SPIMGetSamplePlayBackTime`.
///
/// # Safety
/// `sample` is a live `SESample*`; `total` and `remaining` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_play_back_time(
    sample: *mut c_void,
    total: *mut i32,
    remaining: *mut i32,
) -> i32 {
    let (code, t, r) = mixer().sample_play_back_time(sample);
    unsafe {
        *total = t;
        *remaining = r;
    }
    code
}

/// `SPIMDeleteWaveData`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_delete_wave_data(sample: *mut c_void) {
    let channel = unsafe { ee_mixer_host_mixer_channel(sample) };
    if channel != CHANNEL_NONE {
        mixer().stop_channel(channel);
    }
    let data = unsafe { ee_mixer_host_data(sample) };
    if !data.is_null() {
        // SAFETY: the sample's data pointer is a live `Arc<SampleData>` raw pointer (see `ee_mixer_sample_create`)
        unsafe { drop(Arc::from_raw(data as *const SampleData)) };
        unsafe { ee_mixer_host_set_data(sample, std::ptr::null()) };
    }
}

#[no_mangle]
pub extern "C" fn ee_mixer_volume() -> f32 {
    mixer().volume()
}

#[no_mangle]
pub extern "C" fn ee_mixer_set_volume(volume: f32) -> i32 {
    mixer().set_volume(volume)
}

#[no_mangle]
pub extern "C" fn ee_mixer_set_master_volume(volume: f32) {
    mixer().set_master_volume(volume);
}

#[no_mangle]
pub extern "C" fn ee_mixer_set_muted(muted: bool) {
    mixer().set_muted(muted);
}

/// `SPIMSetMusicSource`: `source` NULL clears; returns once the device thread is out of the old source.
///
/// # Safety
/// `source` adds `count` stereo float frames at `frames`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_music_source(
    source: Option<unsafe extern "C" fn(context: *mut c_void, frames: *mut f32, count: c_ulong)>,
    context: *mut c_void,
) {
    mixer().set_music_source(source, context);
}

/// `SPIMSetMovieSource`: as the music source.
///
/// # Safety
/// As `ee_mixer_set_music_source`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_set_movie_source(
    source: Option<unsafe extern "C" fn(context: *mut c_void, frames: *mut f32, count: c_ulong)>,
    context: *mut c_void,
) {
    mixer().set_movie_source(source, context);
}

/// The device thread's callback: mixes `count` stereo frames.
///
/// # Safety
/// `frames` is writable for `count * 2` floats.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_mix(frames: *mut f32, count: u32) {
    mixer().mix(unsafe { slice::from_raw_parts_mut(frames, count as usize * 2) });
}

/// `SPIMLoadWaveData`: a decoded sample from a file image and its interleaved PCM; NULL when empty. The caller stores
/// the pointer in the sample (`SESample::SetData`).
///
/// # Safety
/// `file` is valid for `file_size`, `pcm` for `pcm_length` floats.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_create(
    file: *const u8,
    file_size: u32,
    pcm: *const f32,
    pcm_length: usize,
    file_channels: u32,
    rate: u32,
) -> *const c_void {
    let file = unsafe { slice::from_raw_parts(file, file_size as usize) };
    let pcm = unsafe { slice::from_raw_parts(pcm, pcm_length) };
    match SampleData::new(file, pcm, file_channels, rate) {
        Some(data) => Arc::into_raw(data) as *const c_void,
        None => std::ptr::null(),
    }
}

/// The sample's frame count, rate and file-frame size (`SPIMLoadWaveData`).
///
/// # Safety
/// `data` is a pointer from `ee_mixer_sample_create`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_info(
    data: *const c_void,
    frames: *mut u32,
    rate: *mut u32,
    frame_bytes: *mut u32,
) {
    let sample = unsafe { &*(data as *const SampleData) };
    unsafe {
        *frames = sample.frames;
        *rate = sample.rate;
        *frame_bytes = sample.frame_bytes;
    }
}

/// `SPIMDuplicateSample`'s reference on the shared wave data.
///
/// # Safety
/// `data` is a pointer from `ee_mixer_sample_create`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_reference(data: *const c_void) {
    unsafe { Arc::increment_strong_count(data as *const SampleData) };
}
