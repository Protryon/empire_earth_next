//! The C ABI of the mixer plug-in (called by the C++ shim, `Mixer.cpp`: the entry points, the device, the decoder) and the `SESample` access through the shim's
//! `ee_mixer_host_*`. Compiled out of `cargo test`.

// `c_ulong`/`c_long` are the plug-in's `unsigned long`/`long`: 32 bits on Windows, 64 on LP64, so their casts are
// needed on one of them
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::slice;
use std::sync::{Arc, OnceLock};

use crate::mixer::mixer::{Mixer, SampleData, SampleHandle, SampleHost, CHANNEL_NONE};

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
pub extern "C" fn SPIMGetOutputRate() -> c_ulong {
    mixer().output_rate() as c_ulong
}

#[no_mangle]
pub extern "C" fn SPIMCreateChannels(count: c_ulong, _info: *mut c_void) -> c_long {
    mixer().create_channels(count as u32) as c_long
}

#[no_mangle]
pub extern "C" fn SPIMDestroyChannels() -> c_long {
    mixer().destroy_channels() as c_long
}

#[no_mangle]
pub extern "C" fn SPIMGetChannelCount() -> c_ulong {
    mixer().channel_count() as c_ulong
}

#[no_mangle]
pub extern "C" fn SPIMGetMaximumChannelCount() -> c_ulong {
    mixer().maximum_channel_count() as c_ulong
}

#[no_mangle]
pub extern "C" fn SPIMIsChannelBusyPlaying(channel: c_ulong) -> c_long {
    mixer().is_channel_busy_playing(channel as u32) as c_long
}

#[no_mangle]
pub extern "C" fn SPIMPlayChannel(channel: c_ulong) -> c_long {
    mixer().play_channel(channel as u32) as c_long
}

#[no_mangle]
pub extern "C" fn SPIMStopChannel(channel: c_ulong) -> c_long {
    mixer().stop_channel(channel as u32) as c_long
}

#[no_mangle]
pub extern "C" fn SPIMStopAllChannels() -> c_long {
    mixer().stop_all_channels() as c_long
}

/// `SPIMSetChannelSample`.
///
/// # Safety
/// `sample` is a live `SESample*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetChannelSample(channel: c_ulong, sample: *mut c_void) -> c_long {
    mixer().set_channel_sample(channel as u32, sample) as c_long
}

/// `SPIMSetSampleLooping`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetSampleLooping(sample: *mut c_void, looping: bool) {
    mixer().set_sample_looping(sample, looping);
}

/// `SPIMSetSamplePan`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetSamplePan(sample: *mut c_void, pan: f32) -> c_long {
    mixer().set_sample_pan(sample, pan) as c_long
}

/// `SPIMSetSamplePitch`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetSamplePitch(sample: *mut c_void, pitch: f32) -> c_long {
    mixer().set_sample_pitch(sample, pitch) as c_long
}

/// `SPIMSetSampleVolume`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetSampleVolume(sample: *mut c_void, volume: f32) -> c_long {
    mixer().set_sample_volume(sample, volume) as c_long
}

/// `SPIMGetSampleOffset`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMGetSampleOffset(sample: *mut c_void) -> c_ulong {
    mixer().sample_offset(sample) as c_ulong
}

/// `SPIMSetSampleOffset`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetSampleOffset(sample: *mut c_void, offset: c_ulong) -> c_long {
    mixer().set_sample_offset(sample, offset as u32) as c_long
}

/// `SPIMGetSamplePlayBackTime`.
///
/// # Safety
/// `sample` is a live `SESample*`; `total` and `remaining` for a write.
#[no_mangle]
pub unsafe extern "C" fn SPIMGetSamplePlayBackTime(
    sample: *mut c_void,
    total: *mut c_long,
    remaining: *mut c_long,
) -> c_long {
    let (code, t, r) = mixer().sample_play_back_time(sample);
    unsafe {
        *total = t as c_long;
        *remaining = r as c_long;
    }
    code as c_long
}

/// `SPIMDeleteWaveData`.
///
/// # Safety
/// `sample` is a live `SESample*`.
#[no_mangle]
pub unsafe extern "C" fn SPIMDeleteWaveData(sample: *mut c_void) {
    let channel = unsafe { ee_mixer_host_mixer_channel(sample) };
    if channel != CHANNEL_NONE {
        mixer().stop_channel(channel);
    }
    let data = unsafe { ee_mixer_host_data(sample) };
    if !data.is_null() {
        // SAFETY: the sample's data pointer is a live `Arc<SampleData>` raw pointer (see `ee_mixer_decode`)
        unsafe { drop(Arc::from_raw(data as *const SampleData)) };
        unsafe { ee_mixer_host_set_data(sample, std::ptr::null()) };
    }
}

#[no_mangle]
pub extern "C" fn SPIMGetVolume() -> f32 {
    mixer().volume()
}

#[no_mangle]
pub extern "C" fn SPIMSetVolume(volume: f32) -> c_long {
    mixer().set_volume(volume) as c_long
}

#[no_mangle]
pub extern "C" fn SPIMSetMasterVolume(volume: f32) {
    mixer().set_master_volume(volume);
}

#[no_mangle]
pub extern "C" fn SPIMSetMuted(muted: bool) {
    mixer().set_muted(muted);
}

/// `SPIMSetMusicSource`: `source` NULL clears; returns once the device thread is out of the old source.
///
/// # Safety
/// `source` adds `count` stereo float frames at `frames`.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetMusicSource(
    source: Option<unsafe extern "C" fn(context: *mut c_void, frames: *mut f32, count: c_ulong)>,
    context: *mut c_void,
) {
    mixer().set_music_source(source, context);
}

/// `SPIMActivateMixer`'s device: the named output (the system's default when there is none of that name; NULL or ""
/// for the default) through cpal, else, or with `null_only` (the headless simulation), the null device; the mixer
/// pulls the mix on the device's thread from now on. The device's rate (the caller hands it to
/// `ee_mixer_activated`).
///
/// # Safety
/// `name` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_device_open(name: *const c_char, null_only: bool) -> u32 {
    let name =
        if name.is_null() { String::new() } else { unsafe { CStr::from_ptr(name) }.to_string_lossy().into_owned() };
    crate::mixer::device::open(&name, null_only, |buf| mixer().mix(buf))
}

/// The device closed: when this returns, its thread no longer mixes.
#[no_mangle]
pub extern "C" fn ee_mixer_device_close() {
    crate::mixer::device::close();
}

/// `SPIMGetOutputDevices`: the playback devices' names, NUL-terminated (cut to `stride - 1` bytes), `stride` bytes
/// apart, at most `capacity` of them; how many.
///
/// # Safety
/// `out` is writable for `stride * capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_output_devices(out: *mut c_char, stride: usize, capacity: u32) -> u32 {
    if out.is_null() || stride == 0 {
        return 0;
    }
    let mut n = 0u32;
    for name in crate::mixer::device::output_devices().into_iter().take(capacity as usize) {
        let bytes = name.as_bytes();
        let len = bytes.len().min(stride - 1);
        unsafe {
            let at = out.add(n as usize * stride);
            std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, at, len);
            *at.add(len) = 0;
        }
        n += 1;
    }
    n
}

/// `SPIMLoadWaveData`: a file image decoded (WAV, MP3, FLAC: symphonia, miniaudio's sample conversions) into a
/// sample; NULL when it cannot be decoded. The caller stores the pointer in the sample (`SESample::SetData`).
///
/// # Safety
/// `file` is valid for `file_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_decode(file: *const u8, file_size: u32) -> *const c_void {
    if file.is_null() {
        return std::ptr::null();
    }
    let file = unsafe { slice::from_raw_parts(file, file_size as usize) };
    let Some(d) = crate::mixer::device::decode(file) else { return std::ptr::null() };
    match SampleData::new(file, &d.pcm, d.channels, d.rate) {
        Some(data) => Arc::into_raw(data) as *const c_void,
        None => std::ptr::null(),
    }
}

/// The sample's frame count, rate and file-frame size (`SPIMLoadWaveData`).
///
/// # Safety
/// `data` is a pointer from `ee_mixer_decode`.
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
/// `data` is a pointer from `ee_mixer_decode`.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_sample_reference(data: *const c_void) {
    unsafe { Arc::increment_strong_count(data as *const SampleData) };
}

// ---- the entry points with nothing to do on this device (the original's DirectSound tuning)

/// `SPIMGetCPUUsage`: not measured.
#[no_mangle]
pub extern "C" fn SPIMGetCPUUsage() -> f32 {
    0.0
}

/// `SPIMGetDirectSoundInterface`: no DirectSound.
#[no_mangle]
pub extern "C" fn SPIMGetDirectSoundInterface() -> *mut c_void {
    std::ptr::null_mut()
}

/// `SPIMGetHighestTimerDelay`: no mixing timer.
#[no_mangle]
pub extern "C" fn SPIMGetHighestTimerDelay() -> c_long {
    0
}

/// `SPIMMixAhead`: the device pulls the mix.
#[no_mangle]
pub extern "C" fn SPIMMixAhead() -> c_long {
    0
}

/// `SPIMProcessMessage`: no window messages to handle.
#[no_mangle]
pub extern "C" fn SPIMProcessMessage() -> bool {
    true
}

/// `SPIMSetLatency`: the device's own.
#[no_mangle]
pub extern "C" fn SPIMSetLatency(_milliseconds: c_ulong) -> c_long {
    0
}
