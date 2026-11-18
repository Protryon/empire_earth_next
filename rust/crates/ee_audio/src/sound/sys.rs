//! What the Sound Engine's objects reach outside the crate, by their C names: the Thread System's C ABI (ee_ts),
//! the music engine (`crate::dmusic`, under dmusic's names), the platform layer and the C runtime (the conductor's file reads), and the few C++ trampolines left in
//! the Sound Engine's units (`ee_soundcpp_*`: the plug-in module, the sound path and its files, `ESVerify`, the
//! floating-point control). In the game they resolve to ee_ts, the music engine, ee_plat_sys and the C++ units; the cargo
//! replay (`golden_objects.rs`) defines them as the golden driver's fakes.

use std::ffi::{c_char, c_int, c_long, c_ulong, c_void};

/// `unsigned long (*)(void*)`: a thread's procedure.
pub type EeSoundThreadFunction = Option<unsafe extern "C" fn(*mut c_void) -> c_ulong>;
/// dmusic's `DmLoaderResolverCallback` (the conductor's resolver).
pub type DmResolver = unsafe extern "C" fn(*mut c_void, *const c_char, *mut usize) -> *mut c_void;

/// `DmResult_SUCCESS`, `DmLoader_DOWNLOAD`, `DmTiming_MEASURE`, `DmRender_FLOAT | DmRender_STEREO`.
pub(crate) const DM_SUCCESS: c_int = 0;
pub(crate) const DM_LOADER_DOWNLOAD: c_int = 1;
pub(crate) const DM_TIMING_MEASURE: c_int = 4;
pub(crate) const DM_RENDER_FLOAT_STEREO: c_int = 2 | 4;

/// `kTSWaitInfinite`, `kTSWaitTimeout`, `kTSWaitFailed` (`PLAT_WAIT_*`).
pub(crate) const WAIT_INFINITE: u32 = 0xffff_ffff;
pub(crate) const WAIT_TIMEOUT: i32 = -1;
pub(crate) const WAIT_FAILED: i32 = -2;

extern "C" {
    /// The engine's shutdown list (ee_util, a sibling crate of the static library; faked by the tests).
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_util_register_shutdown")]
    pub fn ee_util_register_shutdown(function: unsafe extern "C" fn(bool)) -> std::ffi::c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_section_new")]
    pub fn ee_ts_section_new() -> *mut c_void;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_section_free")]
    pub fn ee_ts_section_free(section: *mut c_void);
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_section_enter")]
    pub fn ee_ts_section_enter(section: *const c_void);
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_section_leave")]
    pub fn ee_ts_section_leave(section: *const c_void);
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_new")]
    pub fn ee_ts_event_new(out: *mut *mut c_void, manual: bool) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_free")]
    pub fn ee_ts_event_free(event: *mut c_void);
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_set")]
    pub fn ee_ts_event_set(event: *const c_void) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_clear")]
    pub fn ee_ts_event_clear(event: *const c_void) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_wait")]
    pub fn ee_ts_event_wait(event: *const c_void, timeout: u32) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_event_wait_any")]
    pub fn ee_ts_event_wait_any(events: *const *const c_void, count: u32, timeout: u32) -> i32;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_thread_new")]
    pub fn ee_ts_thread_new(
        out: *mut *mut c_void,
        function: EeSoundThreadFunction,
        parameter: *mut c_void,
        stack: u32,
        suspended: bool,
    ) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_thread_free")]
    pub fn ee_ts_thread_free(thread: *mut c_void) -> c_long;
    #[cfg_attr(test, link_name = "ee_sound_fake_ee_ts_thread_set_name")]
    pub fn ee_ts_thread_set_name(thread: *const c_void, name: *const c_char);

    #[cfg_attr(test, link_name = "ee_sound_fake_PlatSleep")]
    pub fn PlatSleep(milliseconds: u32);
    #[cfg_attr(test, link_name = "ee_sound_fake_PlatGetLastError")]
    pub fn PlatGetLastError() -> u32;
    #[cfg_attr(test, link_name = "ee_sound_fake_PlatFOpen")]
    pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
    pub fn fseek(stream: *mut c_void, offset: c_long, origin: c_int) -> c_int;
    pub fn ftell(stream: *mut c_void) -> c_long;
    pub fn fread(buffer: *mut c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
    pub fn fclose(stream: *mut c_void) -> c_int;
    pub fn malloc(size: usize) -> *mut c_void;
    pub fn free(block: *mut c_void);

    /// `ESVerify(__FILE__, line, expression)` (SESoundManager.cpp).
    pub fn ee_soundcpp_verify(expression: *const c_char, line: u32);
    /// `UMSetFloatingPointPrecision(kFPPrecision53)`.
    pub fn ee_soundcpp_fp_precision53();
    /// The scavenger loop's checks: 53-bit precision and round-to-nearest, set again when they differ.
    pub fn ee_soundcpp_fp_check();
    /// `FSPath::Reference` / `Dereference` (the last one deletes the path).
    pub fn ee_soundcpp_path_reference(path: *mut c_void);
    pub fn ee_soundcpp_path_dereference(path: *mut c_void);
    /// `path->FindFile(name, spec)`: the spec, or NULL when it failed.
    pub fn ee_soundcpp_find_file(path: *mut c_void, name: *const c_char) -> *mut c_void;
    /// `delete spec`.
    pub fn ee_soundcpp_file_delete(file: *mut c_void);
    /// `module->GetEntry(name)` (SEMixer.cpp), `module->IsValid()`, `module->Unload()`.
    pub fn ee_soundcpp_module_entry(module: *mut c_void, name: *const c_char) -> *mut c_void;
    pub fn ee_soundcpp_module_is_valid(module: *mut c_void) -> bool;
    pub fn ee_soundcpp_module_unload(module: *mut c_void);
    /// `PIMModule::GetFirstModule(directory, ' snd', start->mModule)` / `GetNextModule(start->mModule)`.
    pub fn ee_soundcpp_first_module(start: *mut c_void) -> i32;
    pub fn ee_soundcpp_next_module(start: *mut c_void) -> i32;
}

/// Calls `ee_soundcpp_verify` when `ok` is false (`ESVERIFY_AT(expression, line)`).
pub(crate) fn verify(ok: bool, expression: &'static [u8], line: u32) {
    if !ok {
        debug_assert_eq!(expression.last(), Some(&0));
        unsafe { ee_soundcpp_verify(expression.as_ptr() as *const c_char, line) }
    }
}

/// The section's `Enter` ... `Leave` around `f` (a TSCriticalSection held as the C++ held it).
pub(crate) fn locked<R>(section: *mut c_void, f: impl FnOnce() -> R) -> R {
    unsafe { ee_ts_section_enter(section) };
    let r = f();
    unsafe { ee_ts_section_leave(section) };
    r
}

// The music engine (crate::dmusic, the DirectMusic port): its seam under dmusic's C names. The Sound Engine objects'
// golden replay (golden_objects.rs) defines these names as the driver's fakes.
#[cfg(not(test))]
pub use crate::dmusic::seam::*;
#[cfg(test)]
extern "C" {
    pub fn DmLoader_create(out: *mut *mut c_void, options: c_int) -> c_int;
    pub fn DmLoader_release(loader: *mut c_void);
    pub fn DmLoader_addResolver(loader: *mut c_void, resolve: DmResolver, context: *mut c_void) -> c_int;
    pub fn DmLoader_getSegment(loader: *mut c_void, name: *const c_char, out: *mut *mut c_void) -> c_int;
    pub fn DmSegment_release(segment: *mut c_void);
    pub fn DmSegment_getRepeats(segment: *const c_void) -> u32;
    pub fn DmSegment_setRepeats(segment: *mut c_void, repeats: u32);
    pub fn DmPerformance_create(out: *mut *mut c_void, rate: u32) -> c_int;
    pub fn DmPerformance_release(performance: *mut c_void);
    pub fn DmPerformance_playSegment(performance: *mut c_void, segment: *mut c_void, timing: c_int) -> c_int;
    pub fn DmPerformance_playTransition(
        performance: *mut c_void,
        segment: *mut c_void,
        embellishment: c_int,
        timing: c_int,
    ) -> c_int;
    pub fn DmPerformance_setVolume(performance: *mut c_void, volume: f32);
    pub fn DmPerformance_setGrooveLevel(performance: *mut c_void, level: u8);
    pub fn DmPerformance_stop(performance: *mut c_void, lookahead_ms: u32);
    pub fn DmPerformance_setReverb(performance: *mut c_void, enabled: c_int, gain: f32, mix: f32, time: f32, hf: f32);
    /// The general MIDI set built into the program (crate::dmusic::GENERAL_MIDI, EE.sf2) and its length.
    pub fn SEGeneralMIDISet(length: *mut usize) -> *const u8;
    pub fn DmPerformance_renderPcm(
        performance: *mut c_void,
        buffer: *mut c_void,
        count: usize,
        options: c_int,
    ) -> c_int;
}
