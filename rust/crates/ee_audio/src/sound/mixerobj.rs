//! `SEMixer` whole in Rust: a loaded mixer plug-in (its `SPIM*` entry points, looked up by name when the mixer is
//! made) with the conductor that plays scores through it; the registry of mixers and the active mixer
//! (`sActiveMixer`); `SEInitializeMixers` and `SEShutdown`'s mixers. The decisions are `mixers.rs`'s. An `SEMixer*`
//! is an [`EeSoundMixer`]; the C++ class is data-less, every member a forwarder. The plug-in module stays a C++
//! `PIMModule`, reached through `ee_soundcpp_module_*`.

// the C ABI's `long`/`unsigned long` are 32-bit on Windows and 64-bit on LP64: the casts are not no-ops there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_long, c_ulong, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Mutex;

use crate::sound::conductorobj::{self, EeSoundConductor};
use crate::sound::mixers::{self, EngineHost, Host, Mixer, Registry};
use crate::sound::sys;

/// A mixer's music or movie source: interleaved stereo float frames added at the output rate.
pub type EeSoundSource = Option<unsafe extern "C" fn(*mut c_void, *mut f32, c_ulong)>;

type F0 = Option<unsafe extern "C" fn() -> c_long>;
type FU = Option<unsafe extern "C" fn() -> c_ulong>;
type FChan = Option<unsafe extern "C" fn(c_ulong) -> c_long>;
type FSampleF = Option<unsafe extern "C" fn(*mut c_void, f32) -> c_long>;
type FSource = Option<unsafe extern "C" fn(EeSoundSource, *mut c_void)>;

/// The plug-in's entry points (`SPIM*`), in the order the original looked them up.
#[repr(C)]
pub struct EeSoundSpim {
    pub activate_mixer: F0,
    pub create_channels: Option<unsafe extern "C" fn(c_ulong, *mut c_void) -> c_long>,
    pub deactivate_mixer: F0,
    pub delete_wave_data: Option<unsafe extern "C" fn(*mut c_void)>,
    pub destroy_channels: F0,
    pub duplicate_sample: Option<unsafe extern "C" fn(*const c_void, *mut *mut c_void) -> c_long>,
    pub get_channel_count: FU,
    pub get_cpu_usage: Option<unsafe extern "C" fn() -> f32>,
    pub get_direct_sound_interface: Option<unsafe extern "C" fn() -> *mut c_void>,
    pub get_highest_timer_delay: FU,
    pub get_maximum_channel_count: FU,
    pub get_sample_playback_time: Option<unsafe extern "C" fn(*mut c_void, *mut c_long, *mut c_long) -> c_long>,
    pub get_sample_offset: Option<unsafe extern "C" fn(*mut c_void) -> c_ulong>,
    pub get_volume: Option<unsafe extern "C" fn() -> f32>,
    pub initialize_mixer: Option<unsafe extern "C" fn(*mut c_void)>,
    pub is_channel_busy_playing: FChan,
    pub load_wave_data: Option<unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_long>,
    pub play_channel: FChan,
    pub process_message: Option<unsafe extern "C" fn(c_ulong, *mut c_void, *mut c_void) -> bool>,
    pub set_channel_sample: Option<unsafe extern "C" fn(c_ulong, *mut c_void) -> c_long>,
    pub mix_ahead: F0,
    pub set_latency: FChan,
    pub set_sample_offset: Option<unsafe extern "C" fn(*mut c_void, c_ulong) -> c_long>,
    pub set_sample_looping: Option<unsafe extern "C" fn(*mut c_void, bool)>,
    pub set_sample_pan: FSampleF,
    pub set_sample_pitch: FSampleF,
    pub set_sample_volume: FSampleF,
    pub set_volume: Option<unsafe extern "C" fn(f32) -> c_long>,
    pub stop_all_channels: F0,
    pub stop_channel: FChan,
    /// This port's additions (optional).
    pub get_output_rate: FU,
    pub set_music_source: FSource,
    pub set_movie_source: FSource,
}

/// The entry names, in `EeSoundSpim`'s order.
const NAMES: [&[u8]; 33] = [
    b"SPIMActivateMixer\0",
    b"SPIMCreateChannels\0",
    b"SPIMDeactivateMixer\0",
    b"SPIMDeleteWaveData\0",
    b"SPIMDestroyChannels\0",
    b"SPIMDuplicateSample\0",
    b"SPIMGetChannelCount\0",
    b"SPIMGetCPUUsage\0",
    b"SPIMGetDirectSoundInterface\0",
    b"SPIMGetHighestTimerDelay\0",
    b"SPIMGetMaximumChannelCount\0",
    b"SPIMGetSamplePlayBackTime\0",
    b"SPIMGetSampleOffset\0",
    b"SPIMGetVolume\0",
    b"SPIMInitializeMixer\0",
    b"SPIMIsChannelBusyPlaying\0",
    b"SPIMLoadWaveData\0",
    b"SPIMPlayChannel\0",
    b"SPIMProcessMessage\0",
    b"SPIMSetChannelSample\0",
    b"SPIMMixAhead\0",
    b"SPIMSetLatency\0",
    b"SPIMSetSampleOffset\0",
    b"SPIMSetSampleLooping\0",
    b"SPIMSetSamplePan\0",
    b"SPIMSetSamplePitch\0",
    b"SPIMSetSampleVolume\0",
    b"SPIMSetVolume\0",
    b"SPIMStopAllChannels\0",
    b"SPIMStopChannel\0",
    b"SPIMGetOutputRate\0",
    b"SPIMSetMusicSource\0",
    b"SPIMSetMovieSource\0",
];

/// A mixer: its conductor, its plug-in module and the module's entry points.
pub struct EeSoundMixer {
    pub(crate) conductor: EeSoundConductor,
    module: *mut c_void,
    pub(crate) spim: EeSoundSpim,
}

/// `SEMixerStart` (Sound Engine's `SEInitializeMixers`): the directory, the window and the module being walked.
#[repr(C)]
pub struct EeSoundMixerStart {
    pub directory: *const c_void,
    pub window: *mut c_void,
    pub module: *mut c_void,
}

/// `sActiveMixer`.
static ACTIVE: AtomicPtr<EeSoundMixer> = AtomicPtr::new(null_mut());
static REGISTRY: Mutex<Registry> = Mutex::new(Registry::new());

fn registry() -> std::sync::MutexGuard<'static, Registry> {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner())
}

pub(crate) fn active() -> *mut EeSoundMixer {
    ACTIVE.load(Ordering::SeqCst)
}

/// # Safety
/// `m` is a live mixer.
pub(crate) unsafe fn spim<'a>(m: *mut EeSoundMixer) -> &'a EeSoundSpim {
    unsafe { &(*m).spim }
}

/// `mixer->DeleteWaveData(sample)` (the sample's destructor).
///
/// # Safety
/// `m` is a live mixer, `s` a live sample.
pub(crate) unsafe fn delete_wave_data(m: *mut EeSoundMixer, s: *mut crate::sound::objects::EeSoundSample) {
    unsafe { spim(m).delete_wave_data.expect("SPIMDeleteWaveData")(s.cast()) }
}

/// `new SEMixer(module, window)`: the conductor first (its lock), then the registry, the entry points by name, the
/// plug-in's `SPIMInitializeMixer(window)` and the conductor's mixer.
///
/// # Safety
/// `module` is a live `PIMModule*`.
pub(crate) unsafe fn create(module: *mut c_void, window: *mut c_void) -> *mut EeSoundMixer {
    let conductor = EeSoundConductor::new();
    let mut entries = [null_mut::<c_void>(); 33];
    let m = Box::into_raw(Box::new(EeSoundMixer { conductor, module, spim: unsafe { std::mem::zeroed() } }));
    registry().register(Mixer(m.cast()));
    for (e, name) in entries.iter_mut().zip(NAMES) {
        *e = unsafe { sys::ee_soundcpp_module_entry(module, name.as_ptr() as *const c_char) };
    }
    // the table's fields are the entries in order, each an optional function pointer (pointer-sized, null: None)
    unsafe { (*m).spim = std::mem::transmute::<[*mut c_void; 33], EeSoundSpim>(entries) };
    unsafe { (*m).spim.initialize_mixer.expect("SPIMInitializeMixer")(window) };
    unsafe { (*m).conductor.set_mixer(m) };
    m
}

/// `delete mixer`: the active one stops its music and deactivates, it leaves the registry, its module unloads;
/// then the conductor goes (stopping first if it runs).
///
/// # Safety
/// `m` is a live mixer, not used afterwards.
pub(crate) unsafe fn delete(m: *mut EeSoundMixer) {
    mixers::on_destroy(&mut Objects, Mixer(m.cast()));
    registry().unregister(Mixer(m.cast()));
    unsafe { sys::ee_soundcpp_module_unload((*m).module) };
    let b = unsafe { Box::from_raw(m) };
    unsafe { b.conductor.destroy() };
    drop(b);
}

struct Objects;

impl Host for Objects {
    fn active(&self) -> Option<Mixer> {
        let a = active();
        (!a.is_null()).then_some(Mixer(a.cast()))
    }
    fn set_active(&mut self, m: Option<Mixer>) {
        ACTIVE.store(m.map_or(null_mut(), |m| m.0.cast()), Ordering::SeqCst)
    }
    fn stop_music(&mut self, m: Mixer) -> i32 {
        unsafe { conductorobj::ee_sound_conductor_set_state(&(*(m.0 as *mut EeSoundMixer)).conductor, 0) }
    }
    fn activate(&mut self, m: Mixer) -> i32 {
        unsafe { spim(m.0.cast()).activate_mixer.expect("SPIMActivateMixer")() as i32 }
    }
    fn deactivate(&mut self, m: Mixer) -> i32 {
        unsafe { spim(m.0.cast()).deactivate_mixer.expect("SPIMDeactivateMixer")() as i32 }
    }
}

/// The start's walk over the plug-in table.
struct Start(*mut EeSoundMixerStart);

impl EngineHost for Start {
    fn first_module(&mut self) -> i32 {
        unsafe { sys::ee_soundcpp_first_module(self.0.cast()) }
    }
    fn next_module(&mut self) -> i32 {
        unsafe { sys::ee_soundcpp_next_module(self.0.cast()) }
    }
    fn create_mixer(&mut self) -> Mixer {
        Mixer(unsafe { create((*self.0).module, (*self.0).window) }.cast())
    }
    fn is_valid(&mut self, m: Mixer) -> bool {
        unsafe { sys::ee_soundcpp_module_is_valid((*(m.0 as *mut EeSoundMixer)).module) }
    }
    fn delete_mixer(&mut self, m: Mixer) {
        unsafe { delete(m.0.cast()) }
    }
    fn count(&mut self) -> u32 {
        registry().count()
    }
    fn first(&mut self) -> Option<Mixer> {
        registry().get(0).ok()
    }
}

/// `SEInitializeMixers`: a mixer for every valid sound plug-in; `kErrorNotFound` when none is left.
///
/// # Safety
/// `start` is the C++ start state.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixers_initialize(start: *mut EeSoundMixerStart) -> i32 {
    mixers::initialize_mixers(&mut Start(start))
}

/// `SEInitialize`: [`ee_sound_shutdown`] registered with the engine's shutdown list (ee_util), then the mixers.
///
/// # Safety
/// As `ee_sound_mixers_initialize`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_initialize(start: *mut EeSoundMixerStart) -> i32 {
    crate::sound::sys::ee_util_register_shutdown(ee_sound_shutdown);
    ee_sound_mixers_initialize(start)
}

/// `SEShutdown` (a shutdown-list function): every mixer deleted.
///
/// # Safety
/// None beyond the engine's: the shutdown list calls it once the sound threads are done.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_shutdown(_forced: bool) {
    ee_sound_mixers_delete_all();
}

/// `SEShutdown`'s mixers: the first one deleted until none is left.
#[no_mangle]
pub extern "C" fn ee_sound_mixers_delete_all() {
    mixers::delete_mixers(&mut Start(null_mut()))
}

/// `SEMixer::GetActiveMixer`.
#[no_mangle]
pub extern "C" fn ee_sound_mixer_active() -> *mut EeSoundMixer {
    active()
}

/// `SEMixer::GetCount`.
#[no_mangle]
pub extern "C" fn ee_sound_mixer_count() -> u32 {
    registry().count()
}

/// `SEMixer::GetMixer`: `kErrorBeyondEnd` past the last (`out` untouched).
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_get(index: u32, out: *mut *mut EeSoundMixer) -> i32 {
    match registry().get(index) {
        Ok(m) => {
            unsafe { *out = m.0.cast() };
            mixers::OK
        }
        Err(e) => e,
    }
}

/// `SEMixer::UseMixer` (null: none).
///
/// # Safety
/// `m` is null or a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_use(m: *mut EeSoundMixer) -> i32 {
    mixers::use_mixer(&mut Objects, (!m.is_null()).then_some(Mixer(m.cast())))
}

/// `SEMixer::GetConductor`.
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_conductor(m: *mut EeSoundMixer) -> *mut EeSoundConductor {
    unsafe { &mut (*m).conductor }
}

/// The mixer's `PIMModule*` (`GetName`, `IsValid`).
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_module(m: *mut EeSoundMixer) -> *mut c_void {
    unsafe { (*m).module }
}

/// `GetCPUUsage`.
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_cpu_usage(m: *mut EeSoundMixer) -> f32 {
    unsafe { spim(m).get_cpu_usage.expect("SPIMGetCPUUsage")() }
}

/// `GetDirectSoundInterface`.
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_direct_sound(m: *mut EeSoundMixer) -> *mut c_void {
    unsafe { spim(m).get_direct_sound_interface.expect("SPIMGetDirectSoundInterface")() }
}

/// `ProcessMessage`.
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_process_message(
    m: *mut EeSoundMixer,
    message: c_ulong,
    p1: *mut c_void,
    p2: *mut c_void,
) -> bool {
    unsafe { spim(m).process_message.expect("SPIMProcessMessage")(message, p1, p2) }
}

/// `GetOutputRate` (0 for a plug-in without it).
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_output_rate(m: *mut EeSoundMixer) -> c_ulong {
    unsafe { spim(m).get_output_rate.map_or(0, |f| f()) }
}

/// `SetMusicSource` (nothing for a plug-in without it).
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_set_music_source(
    m: *mut EeSoundMixer,
    source: EeSoundSource,
    context: *mut c_void,
) {
    if let Some(f) = unsafe { spim(m).set_music_source } {
        unsafe { f(source, context) }
    }
}

/// `SetMovieSource` (nothing for a plug-in without it).
///
/// # Safety
/// `m` is a live mixer.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_mixer_set_movie_source(
    m: *mut EeSoundMixer,
    source: EeSoundSource,
    context: *mut c_void,
) {
    if let Some(f) = unsafe { spim(m).set_movie_source } {
        unsafe { f(source, context) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_entry_table_is_one_pointer_per_name() {
        assert_eq!(std::mem::size_of::<EeSoundSpim>(), NAMES.len() * std::mem::size_of::<*mut c_void>());
        assert!(NAMES.iter().all(|n| n.ends_with(b"\0") && n.starts_with(b"SPIM")));
    }
}
