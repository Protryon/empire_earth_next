//! The C ABI the C++ facade (`SESoundManager.cpp`) calls, and the C++ trampolines (`ee_soundcpp_*`, defined there)
//! through which the manager reaches nodes, samples, the mixer and the sound path. Compiled out of `cargo test`.
//!
//! Every `ee_sound_*` function taking an `EeSoundManager` is called with the C++ manager's lock held, as the C++
//! held it. `ctx` is the C++ `SESoundManager*`, handed back to the trampolines (they use its mixer and path). No
//! trampoline calls back into this module; a reentrant call would abort (the `busy` flag) rather than alias.

use std::cell::{Cell, UnsafeCell};
use std::ffi::{c_char, c_void, CStr};

use crate::manager::{FileSpec, Host, Node, NodeFields, Sample, SoundManager};

/// cbindgen:ignore
mod sys {
    use super::*;
    extern "C" {
        pub(super) fn ee_soundcpp_node_new() -> *mut c_void;
        pub(super) fn ee_soundcpp_node_reference(node: *mut c_void);
        pub(super) fn ee_soundcpp_node_dereference(node: *mut c_void);
        pub(super) fn ee_soundcpp_node_get(node: *mut c_void, out: *mut NodeFields);
        pub(super) fn ee_soundcpp_node_set(node: *mut c_void, fields: *const NodeFields);
        pub(super) fn ee_soundcpp_sample_is_looping(sample: *mut c_void) -> bool;
        pub(super) fn ee_soundcpp_sample_set_looping(sample: *mut c_void, looping: bool);
        pub(super) fn ee_soundcpp_sample_volume(sample: *mut c_void) -> f32;
        pub(super) fn ee_soundcpp_sample_pan(sample: *mut c_void) -> f32;
        pub(super) fn ee_soundcpp_sample_pitch(sample: *mut c_void) -> f32;
        pub(super) fn ee_soundcpp_sample_set_volume(sample: *mut c_void, v: f32) -> i32;
        pub(super) fn ee_soundcpp_sample_set_pan(sample: *mut c_void, v: f32) -> i32;
        pub(super) fn ee_soundcpp_sample_set_pitch(sample: *mut c_void, v: f32) -> i32;
        pub(super) fn ee_soundcpp_sample_mixer_channel(sample: *mut c_void) -> u32;
        pub(super) fn ee_soundcpp_sample_set_mixer_channel(sample: *mut c_void, channel: u32);
        pub(super) fn ee_soundcpp_sample_length(sample: *mut c_void) -> u32;
        pub(super) fn ee_soundcpp_sample_rate(sample: *mut c_void) -> u32;
        pub(super) fn ee_soundcpp_sample_load(ctx: *mut c_void, file: *mut c_void) -> *mut c_void;
        pub(super) fn ee_soundcpp_sample_duplicate(ctx: *mut c_void, sample: *mut c_void) -> *mut c_void;
        pub(super) fn ee_soundcpp_sample_delete(sample: *mut c_void);
        pub(super) fn ee_soundcpp_find_file(ctx: *mut c_void, name: *const c_char) -> *mut c_void;
        pub(super) fn ee_soundcpp_file_delete(file: *mut c_void);
        pub(super) fn ee_soundcpp_mixer_stop_channel(ctx: *mut c_void, channel: u32);
        pub(super) fn ee_soundcpp_mixer_set_sample_offset(ctx: *mut c_void, sample: *mut c_void, offset: u32);
        pub(super) fn ee_soundcpp_mixer_channel_state(ctx: *mut c_void, channel: u32) -> i32;
        pub(super) fn ee_soundcpp_mixer_playback_time(
            ctx: *mut c_void,
            sample: *mut c_void,
            total: *mut i32,
            remaining: *mut i32,
        );
        pub(super) fn ee_soundcpp_mixer_set_channel_sample(ctx: *mut c_void, channel: u32, sample: *mut c_void) -> i32;
        pub(super) fn ee_soundcpp_mixer_play_channel(ctx: *mut c_void, channel: u32);
        pub(super) fn ee_soundcpp_mixer_stop_all_channels(ctx: *mut c_void);
        pub(super) fn ee_soundcpp_mixer_destroy_channels(ctx: *mut c_void);
        pub(super) fn ee_soundcpp_mixer_create_channels(ctx: *mut c_void, count: u32) -> i32;
    }
}
use sys::*;

/// The C++ side seen from the manager.
struct CppHost {
    ctx: *mut c_void,
}

impl Host for CppHost {
    fn node_new(&mut self) -> Node {
        Node(unsafe { ee_soundcpp_node_new() })
    }
    fn node_reference(&mut self, n: Node) {
        unsafe { ee_soundcpp_node_reference(n.0) }
    }
    fn node_dereference(&mut self, n: Node) {
        unsafe { ee_soundcpp_node_dereference(n.0) }
    }
    fn node(&self, n: Node) -> NodeFields {
        let mut f = std::mem::MaybeUninit::<NodeFields>::uninit();
        unsafe {
            ee_soundcpp_node_get(n.0, f.as_mut_ptr());
            f.assume_init()
        }
    }
    fn set_node(&mut self, n: Node, f: &NodeFields) {
        unsafe { ee_soundcpp_node_set(n.0, f) }
    }
    fn sample_is_looping(&self, s: Sample) -> bool {
        unsafe { ee_soundcpp_sample_is_looping(s.0) }
    }
    fn sample_set_looping(&mut self, s: Sample, looping: bool) {
        unsafe { ee_soundcpp_sample_set_looping(s.0, looping) }
    }
    fn sample_volume(&self, s: Sample) -> f32 {
        unsafe { ee_soundcpp_sample_volume(s.0) }
    }
    fn sample_pan(&self, s: Sample) -> f32 {
        unsafe { ee_soundcpp_sample_pan(s.0) }
    }
    fn sample_pitch(&self, s: Sample) -> f32 {
        unsafe { ee_soundcpp_sample_pitch(s.0) }
    }
    fn sample_set_volume(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { ee_soundcpp_sample_set_volume(s.0, v) }
    }
    fn sample_set_pan(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { ee_soundcpp_sample_set_pan(s.0, v) }
    }
    fn sample_set_pitch(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { ee_soundcpp_sample_set_pitch(s.0, v) }
    }
    fn sample_mixer_channel(&self, s: Sample) -> u32 {
        unsafe { ee_soundcpp_sample_mixer_channel(s.0) }
    }
    fn sample_set_mixer_channel(&mut self, s: Sample, channel: u32) {
        unsafe { ee_soundcpp_sample_set_mixer_channel(s.0, channel) }
    }
    fn sample_length(&self, s: Sample) -> u32 {
        unsafe { ee_soundcpp_sample_length(s.0) }
    }
    fn sample_rate(&self, s: Sample) -> u32 {
        unsafe { ee_soundcpp_sample_rate(s.0) }
    }
    fn sample_load(&mut self, file: FileSpec) -> Sample {
        Sample(unsafe { ee_soundcpp_sample_load(self.ctx, file.0) })
    }
    fn sample_duplicate(&mut self, s: Sample) -> Option<Sample> {
        let d = unsafe { ee_soundcpp_sample_duplicate(self.ctx, s.0) };
        (!d.is_null()).then_some(Sample(d))
    }
    fn sample_delete(&mut self, s: Option<Sample>) {
        if let Some(s) = s {
            unsafe { ee_soundcpp_sample_delete(s.0) }
        }
    }
    fn find_file(&mut self, name: &[u8]) -> Option<FileSpec> {
        let mut z = name.to_vec();
        z.push(0);
        let f = unsafe { ee_soundcpp_find_file(self.ctx, z.as_ptr() as *const c_char) };
        (!f.is_null()).then_some(FileSpec(f))
    }
    fn file_delete(&mut self, f: FileSpec) {
        unsafe { ee_soundcpp_file_delete(f.0) }
    }
    fn stop_channel(&mut self, channel: u32) {
        unsafe { ee_soundcpp_mixer_stop_channel(self.ctx, channel) }
    }
    fn set_sample_offset(&mut self, s: Sample, offset: u32) {
        unsafe { ee_soundcpp_mixer_set_sample_offset(self.ctx, s.0, offset) }
    }
    fn channel_state(&mut self, channel: u32) -> i32 {
        unsafe { ee_soundcpp_mixer_channel_state(self.ctx, channel) }
    }
    fn playback_time(&mut self, s: Sample) -> (i32, i32) {
        let (mut total, mut remaining) = (0, 0);
        unsafe { ee_soundcpp_mixer_playback_time(self.ctx, s.0, &mut total, &mut remaining) };
        (total, remaining)
    }
    fn set_channel_sample(&mut self, channel: u32, s: Sample) -> i32 {
        unsafe { ee_soundcpp_mixer_set_channel_sample(self.ctx, channel, s.0) }
    }
    fn play_channel(&mut self, channel: u32) {
        unsafe { ee_soundcpp_mixer_play_channel(self.ctx, channel) }
    }
    fn stop_all_channels(&mut self) {
        unsafe { ee_soundcpp_mixer_stop_all_channels(self.ctx) }
    }
    fn destroy_channels(&mut self) {
        unsafe { ee_soundcpp_mixer_destroy_channels(self.ctx) }
    }
    fn create_channels(&mut self, count: u32) -> i32 {
        unsafe { ee_soundcpp_mixer_create_channels(self.ctx, count) }
    }
}

/// The manager's state, owned by the C++ `SESoundManager` (created and destroyed with it).
pub struct EeSoundManager {
    busy: Cell<bool>,
    inner: UnsafeCell<SoundManager>,
}

/// # Safety
/// `m` comes from `ee_sound_create` and the C++ lock is held.
unsafe fn with<R>(m: *mut EeSoundManager, ctx: *mut c_void, f: impl FnOnce(&mut SoundManager, &mut CppHost) -> R) -> R {
    let m = unsafe { &*m };
    assert!(!m.busy.replace(true), "ee_sound: the sound manager was re-entered from a C++ callback");
    let r = f(unsafe { &mut *m.inner.get() }, &mut CppHost { ctx });
    m.busy.set(false);
    r
}

fn host() -> CppHost {
    CppHost { ctx: std::ptr::null_mut() }
}

fn ptr(n: Option<Node>) -> *mut c_void {
    n.map_or(std::ptr::null_mut(), |n| n.0)
}

/// The constructor's state: categories each allowed every channel.
#[no_mangle]
pub extern "C" fn ee_sound_create(category_count: u32, channel_count: u32) -> *mut EeSoundManager {
    Box::into_raw(Box::new(EeSoundManager {
        busy: Cell::new(false),
        inner: UnsafeCell::new(SoundManager::new(category_count, channel_count)),
    }))
}

/// The destructor's part after the scavenger thread and the lock are gone; frees `m`.
///
/// # Safety
/// `m` from `ee_sound_create`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_destroy(m: *mut EeSoundManager, ctx: *mut c_void) {
    unsafe { with(m, ctx, |s, h| s.destroy(h)) };
    drop(unsafe { Box::from_raw(m) });
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_category_percentage(m: *mut EeSoundManager, category: u32, percent: f32) -> u32 {
    unsafe { with(m, std::ptr::null_mut(), |s, _| s.set_category_percentage(category, percent)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_category_priority(m: *mut EeSoundManager, category: u32, priority: u32) -> i32 {
    unsafe { with(m, std::ptr::null_mut(), |s, _| s.set_category_priority(category, priority)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_available_channels(m: *mut EeSoundManager) -> u32 {
    unsafe { with(m, std::ptr::null_mut(), |s, _| s.available_channels) }
}

/// # Safety
/// As `with`; `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_load(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    name: *const c_char,
    category: u32,
    max_instances: u32,
) -> u32 {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    unsafe { with(m, ctx, |s, h| s.load(h, name, category, max_instances)) }
}

/// `PlayAndReturnReferencedNode`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_play(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    sample_id: u32,
    volume: f32,
    pan: f32,
    pitch: f32,
    looping: bool,
) -> *mut c_void {
    unsafe { with(m, ctx, |s, h| ptr(s.play(h, sample_id, volume, pan, pitch, looping))) }
}

/// The playing, else paused, instance with the id (no reference added).
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_find_instance(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    instance_id: u32,
) -> *mut c_void {
    unsafe { with(m, ctx, |s, h| ptr(s.find_instance(h, instance_id))) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_is_looping(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> bool {
    unsafe { with(m, ctx, |s, h| s.is_looping(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_is_playing(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> bool {
    unsafe { with(m, ctx, |s, h| s.is_playing(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_pan(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> f32 {
    unsafe { with(m, ctx, |s, h| s.pan(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_pitch(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> f32 {
    unsafe { with(m, ctx, |s, h| s.pitch(h, instance_id)) }
}

/// `GetVolume2(id)`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_volume(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> f32 {
    unsafe { with(m, ctx, |s, h| s.volume(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_id(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> u32 {
    unsafe { with(m, ctx, |s, h| s.sample_id(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_rate(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> u32 {
    unsafe { with(m, ctx, |s, h| s.sample_rate(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_time_left(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.time_left_of(h, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_total_play_time(m: *mut EeSoundManager, ctx: *mut c_void, sample_id: u32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.total_play_time_of(h, sample_id)) }
}

/// `SetVolume2(id)`.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_volume(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    instance_id: u32,
    v: f32,
) -> i32 {
    unsafe { with(m, ctx, |s, h| s.set_volume(h, instance_id, v)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_pan(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32, v: f32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.set_pan(h, instance_id, v)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_pitch(m: *mut EeSoundManager, ctx: *mut c_void, instance_id: u32, v: f32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.set_pitch(h, instance_id, v)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_parameters(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    instance_id: u32,
    volume: f32,
    pan: f32,
    pitch: f32,
) -> i32 {
    unsafe { with(m, ctx, |s, h| s.set_parameters(h, instance_id, volume, pan, pitch)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_register_one_shot(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    sample_id: u32,
    instance_id: u32,
) -> i32 {
    unsafe { with(m, ctx, |s, h| s.register_one_shot(h, sample_id, instance_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_stop(
    m: *mut EeSoundManager,
    ctx: *mut c_void,
    instance_id: u32,
    let_finish: bool,
) -> i32 {
    unsafe { with(m, ctx, |s, h| s.stop(h, instance_id, let_finish)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_stop_all_one_shots(m: *mut EeSoundManager, ctx: *mut c_void) -> i32 {
    unsafe { with(m, ctx, |s, h| s.stop_all_one_shots(h)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_unload(m: *mut EeSoundManager, ctx: *mut c_void, sample_id: u32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.unload(h, sample_id)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_update_one_shots(m: *mut EeSoundManager, ctx: *mut c_void) {
    unsafe { with(m, ctx, |s, h| s.update_one_shots(h)) }
}

/// One pass of the scavenger thread.
///
/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_scavenge(m: *mut EeSoundManager, ctx: *mut c_void, muted: bool) {
    unsafe { with(m, ctx, |s, h| s.scavenge(h, muted)) }
}

/// # Safety
/// As `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_set_max_channels(m: *mut EeSoundManager, ctx: *mut c_void, count: u32) -> i32 {
    unsafe { with(m, ctx, |s, h| s.set_max_channels(h, count)) }
}

// The node forms: no manager state, only the node and its sample (`node` is a live `SESMInstanceNode*`; the C++
// facade holds the lock around them as before).

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_is_looping(node: *mut c_void) -> bool {
    SoundManager::node_is_looping(&host(), Node(node))
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_is_playing(node: *mut c_void) -> bool {
    SoundManager::node_is_playing(&host(), Node(node))
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_pan(node: *mut c_void) -> f32 {
    SoundManager::node_pan(&host(), Node(node))
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_pitch(node: *mut c_void) -> f32 {
    SoundManager::node_pitch(&host(), Node(node))
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_set_volume(node: *mut c_void, v: f32) -> i32 {
    SoundManager::node_set_volume(&mut host(), Node(node), v)
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_set_pan(node: *mut c_void, v: f32) -> i32 {
    SoundManager::node_set_pan(&mut host(), Node(node), v)
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_set_pitch(node: *mut c_void, v: f32) -> i32 {
    SoundManager::node_set_pitch(&mut host(), Node(node), v)
}

/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_set_parameters(node: *mut c_void, volume: f32, pan: f32, pitch: f32) -> i32 {
    SoundManager::node_set_parameters(&mut host(), Node(node), volume, pan, pitch)
}
