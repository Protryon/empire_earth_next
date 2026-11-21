//! The C ABI of the mixer plug-in (called by the C++ shim, `Mixer.cpp`: the entry points, the device, the decoder) and the `SESample` access through the shim's
//! `ee_mixer_host_*`. Compiled out of `cargo test`.

// `c_ulong`/`c_long` are the plug-in's `unsigned long`/`long`: 32 bits on Windows, 64 on LP64, so their casts are
// needed on one of them
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::slice;
use std::sync::{Arc, OnceLock};

use crate::mixer::mixer::{Mixer, SampleData, SampleHandle, SampleHost, CHANNEL_NONE};
use crate::mixer::plugin;

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

// ---- the entry points over the C++ file objects, the device choice and the entry table (LLE round 8: Mixer.cpp's
// bodies; the C++ keeps one-statement shims, `ee_mixercpp_*`)

/// cbindgen:ignore
mod cpp {
    use super::*;
    extern "C" {
        /// `dynamic_cast<FSMemoryFile*>(spec)`.
        pub(super) fn ee_mixercpp_memory_file(spec: *mut c_void) -> *mut c_void;
        /// `memory->GetLength(*out)`.
        pub(super) fn ee_mixercpp_memory_length(memory: *mut c_void, out: *mut c_ulong) -> c_long;
        /// `memory->GetData()`.
        pub(super) fn ee_mixercpp_memory_data(memory: *mut c_void) -> *const u8;
        /// `spec->GetVolume() + GetDirectory() + GetFile()` into `out` (cut to `capacity`, NUL-terminated).
        pub(super) fn ee_mixercpp_spec_path(spec: *mut c_void, out: *mut c_char, capacity: usize);
        /// `spec->GetFile()`.
        pub(super) fn ee_mixercpp_spec_file(spec: *mut c_void) -> *const c_char;
        pub(super) fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub(super) fn fseek(stream: *mut c_void, offset: c_long, origin: i32) -> i32;
        pub(super) fn ftell(stream: *mut c_void) -> c_long;
        pub(super) fn fread(data: *mut c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
        pub(super) fn fclose(stream: *mut c_void) -> i32;
    }
}

/// The output device `SPIMSetOutputDevice` chose (empty: the system's default).
static OUTPUT_DEVICE: std::sync::Mutex<Vec<u8>> = std::sync::Mutex::new(Vec::new());

/// `SPIMSetOutputDevice`: the device the next activation opens (NULL: the default).
///
/// # Safety
/// `name` is NULL or NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn SPIMSetOutputDevice(name: *const c_char) {
    let name = if name.is_null() { Vec::new() } else { unsafe { CStr::from_ptr(name) }.to_bytes().to_vec() };
    *OUTPUT_DEVICE.lock().unwrap_or_else(|e| e.into_inner()) = name;
}

/// `SPIMGetOutputDevices`: the playback devices' names, 128 bytes apart.
///
/// # Safety
/// `out` is writable for `capacity` names of 128 bytes.
#[no_mangle]
pub unsafe extern "C" fn SPIMGetOutputDevices(out: *mut [c_char; 128], capacity: c_ulong) -> c_ulong {
    unsafe { ee_mixer_output_devices(out.cast(), 128, capacity as u32) as c_ulong }
}

/// `SPIMActivateMixer`: opens the playback device (the chosen output, else the default, else the null device; the
/// null device alone in the headless simulation) and the mixer at its rate.
#[no_mangle]
pub extern "C" fn SPIMActivateMixer() -> c_long {
    if ee_mixer_is_active() {
        return plugin::SPIM_OK;
    }
    let mut name = OUTPUT_DEVICE.lock().unwrap_or_else(|e| e.into_inner()).clone();
    name.push(0);
    let sim = ee_base::plat_core::sim::gPlatSimMode.get() != 0;
    let rate = unsafe { ee_mixer_device_open(name.as_ptr() as *const c_char, sim) };
    ee_mixer_activated(rate);
    plugin::SPIM_OK
}

/// `SPIMDeactivateMixer`: the channels, then the device.
#[no_mangle]
pub extern "C" fn SPIMDeactivateMixer() -> c_long {
    if !ee_mixer_is_active() {
        return plugin::SPIM_OK;
    }
    SPIMDestroyChannels();
    ee_mixer_device_close();
    ee_mixer_deactivated();
    plugin::SPIM_OK
}

/// `SPIMInitializeMixer`: the window is not needed (the device is cpal's).
#[no_mangle]
pub extern "C" fn SPIMInitializeMixer(_window: *mut c_void) {}

/// The plug-in's check (`SEMixer::IsValid`, `ee_soundcpp_module_is_valid`): it activates, and is deactivated again.
#[no_mangle]
pub extern "C" fn ee_mixer_plugin_is_valid() -> bool {
    if SPIMActivateMixer() != plugin::SPIM_OK {
        return false;
    }
    SPIMDeactivateMixer();
    true
}

/// `SPIMDuplicateSample`: a copy of the sample sharing its wave data (one more reference; none when the original's
/// wave did not load).
///
/// # Safety
/// `sample` is a live sample, `out` writable.
#[no_mangle]
pub unsafe extern "C" fn SPIMDuplicateSample(sample: *mut c_void, out: *mut *mut c_void) -> c_long {
    use crate::sound::objects::{ee_sound_sample_data, ee_sound_sample_duplicate, ee_sound_sample_set_data};
    unsafe {
        let data = ee_sound_sample_data(sample.cast());
        let copy = ee_sound_sample_duplicate(sample.cast());
        if !data.is_null() {
            ee_mixer_sample_reference(data);
        }
        ee_sound_sample_set_data(copy, data);
        *out = copy.cast();
    }
    plugin::SPIM_OK
}

/// The whole file at `path` (NUL-terminated) through the platform's `fopen` (`PlatFOpen`: its path forms); None
/// when it does not open or reads nothing.
unsafe fn read_file(path: &[u8]) -> Option<Vec<u8>> {
    let stream = unsafe { cpp::PlatFOpen(path.as_ptr() as *const c_char, c"rb".as_ptr()) };
    if stream.is_null() {
        return None;
    }
    let mut file = Vec::new();
    let mut size = 0usize;
    unsafe {
        cpp::fseek(stream, 0, 2);
        let length = cpp::ftell(stream);
        cpp::fseek(stream, 0, 0);
        if length > 0 {
            file.resize(length as usize, 0);
            size = cpp::fread(file.as_mut_ptr().cast(), 1, length as usize, stream);
        }
        cpp::fclose(stream);
    }
    file.truncate(size);
    (size != 0).then_some(file)
}

/// `SPIMLoadWaveData`: the sound file (from disk, or a memory file's bytes) decoded into the sample, its name, its
/// format and length set; `kSPIMNativeError` when the file cannot be read, `kSPIMFailed` when it cannot be decoded.
///
/// # Safety
/// `spec` is a live `FSFileSpec`, `sample` a live sample.
#[no_mangle]
pub unsafe extern "C" fn SPIMLoadWaveData(spec: *mut c_void, sample: *mut c_void) -> c_long {
    use crate::sound::objects::*;
    let memory = unsafe { cpp::ee_mixercpp_memory_file(spec) };
    let disk;
    let (data, size): (*const u8, c_ulong) = if memory.is_null() {
        let mut path = [0 as c_char; 260];
        unsafe { cpp::ee_mixercpp_spec_path(spec, path.as_mut_ptr(), path.len()) };
        let path = unsafe { CStr::from_ptr(path.as_ptr()) }.to_bytes_with_nul().to_vec();
        let Some(file) = (unsafe { read_file(&path) }) else { return plugin::SPIM_NATIVE_ERROR };
        disk = file;
        (disk.as_ptr(), disk.len() as c_ulong)
    } else {
        let mut size: c_ulong = 0;
        if unsafe { cpp::ee_mixercpp_memory_length(memory, &mut size) } != plugin::SPIM_OK {
            return plugin::SPIM_NATIVE_ERROR;
        }
        (unsafe { cpp::ee_mixercpp_memory_data(memory) }, size)
    };
    let wave = unsafe { ee_mixer_decode(data, size as u32) };
    if wave.is_null() {
        return plugin::SPIM_FAILED;
    }
    let (mut frames, mut rate, mut frame_bytes) = (0u32, 0u32, 0u32);
    unsafe {
        ee_mixer_sample_info(wave, &mut frames, &mut rate, &mut frame_bytes);
        let s = sample.cast();
        ee_sound_sample_set_filename(s, cpp::ee_mixercpp_spec_file(spec));
        ee_sound_sample_set_data(s, wave as *mut c_void);
        ee_sound_sample_set_length_in_bytes(s, size as u32);
        ee_sound_sample_set_bits(s, frame_bytes);
        ee_sound_sample_set_length_in_samples(s, frames);
        ee_sound_sample_set_rate(s, rate);
    }
    plugin::SPIM_OK
}

/// The plug-in's entry points by name (`plugin::NAMES`' order).
fn entries() -> [*mut c_void; 32] {
    [
        SPIMActivateMixer as *mut c_void,
        SPIMCreateChannels as *mut c_void,
        SPIMDeactivateMixer as *mut c_void,
        SPIMDeleteWaveData as *mut c_void,
        SPIMDestroyChannels as *mut c_void,
        SPIMDuplicateSample as *mut c_void,
        SPIMGetCPUUsage as *mut c_void,
        SPIMGetChannelCount as *mut c_void,
        SPIMGetDirectSoundInterface as *mut c_void,
        SPIMGetHighestTimerDelay as *mut c_void,
        SPIMGetMaximumChannelCount as *mut c_void,
        SPIMGetOutputRate as *mut c_void,
        SPIMGetSampleOffset as *mut c_void,
        SPIMGetSamplePlayBackTime as *mut c_void,
        SPIMGetVolume as *mut c_void,
        SPIMInitializeMixer as *mut c_void,
        SPIMIsChannelBusyPlaying as *mut c_void,
        SPIMLoadWaveData as *mut c_void,
        SPIMMixAhead as *mut c_void,
        SPIMPlayChannel as *mut c_void,
        SPIMProcessMessage as *mut c_void,
        SPIMSetChannelSample as *mut c_void,
        SPIMSetLatency as *mut c_void,
        SPIMSetMusicSource as *mut c_void,
        SPIMSetSampleLooping as *mut c_void,
        SPIMSetSampleOffset as *mut c_void,
        SPIMSetSamplePan as *mut c_void,
        SPIMSetSamplePitch as *mut c_void,
        SPIMSetSampleVolume as *mut c_void,
        SPIMSetVolume as *mut c_void,
        SPIMStopAllChannels as *mut c_void,
        SPIMStopChannel as *mut c_void,
    ]
}

/// The plug-in's entry point of that name (the mixer looks its SPIM entries up by name; every plug-in is linked
/// in, and the mixer is the one sound plug-in); NULL for another name.
///
/// # Safety
/// `name` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_soundcpp_module_entry(_module: *mut c_void, name: *const c_char) -> *mut c_void {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    plugin::index(name).map_or(std::ptr::null_mut(), |i| entries()[i])
}

/// `ee_soundcpp_module_is_valid`: the mixer's check.
#[no_mangle]
pub extern "C" fn ee_soundcpp_module_is_valid(_module: *mut c_void) -> bool {
    ee_mixer_plugin_is_valid()
}

/// The plug-in is linked in: nothing to unload.
#[no_mangle]
pub extern "C" fn ee_soundcpp_module_unload(_module: *mut c_void) {}

/// The first (and only) sound plug-in: this one (`SEMixerStart::mModule` a non-null marker).
///
/// # Safety
/// `start` is a live `SEMixerStart` (`EeSoundMixerStart`).
#[no_mangle]
pub unsafe extern "C" fn ee_soundcpp_first_module(start: *mut c_void) -> i32 {
    unsafe { (*(start as *mut crate::sound::mixerobj::EeSoundMixerStart)).module = plugin::MODULE as *mut c_void };
    0
}

/// No more plug-ins (`kErrorNoMoreFiles`).
///
/// # Safety
/// As [`ee_soundcpp_first_module`].
#[no_mangle]
pub unsafe extern "C" fn ee_soundcpp_next_module(start: *mut c_void) -> i32 {
    unsafe { (*(start as *mut crate::sound::mixerobj::EeSoundMixerStart)).module = std::ptr::null_mut() };
    plugin::NO_MORE_FILES
}
