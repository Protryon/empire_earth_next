//! `SESoundManager` whole in Rust: the manager's objects (the sound path, the mixer, the scavenger thread, its two
//! events and the lock) around its bookkeeping and decisions (`manager.rs`), over the Rust nodes and samples
//! (`objects.rs`). An `SESoundManager*` is an [`EeSoundManager`]; the C++ class is data-less, every member a
//! forwarder (`SESoundManager::Create` / `Destroy` in place of `new` / `delete`).
//!
//! Every lock is taken where the C++ took it, and the checks the C++ made before taking it (a NULL node, a none
//! instance id) come first, before the manager is touched: the headless simulation calls `Stop(node)` on a NULL
//! manager with a NULL node.

// the C ABI's `long`/`unsigned long` are 32-bit on Windows and 64-bit on LP64: the casts are not no-ops there
#![allow(clippy::unnecessary_cast)]

use std::cell::{Cell, UnsafeCell};
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicBool, Ordering};

use crate::manager::{FileSpec, Host, Node, NodeFields, Sample, SoundManager, NONE};
use crate::mixerobj::{self, spim, EeSoundMixer};
use crate::objects::{self, EeSoundNode, EeSoundSample};
use crate::sys;

/// The manager.
pub struct EeSoundManager {
    busy: Cell<bool>,
    state: UnsafeCell<SoundManager>,
    path: Cell<*mut c_void>,
    mixer: *mut EeSoundMixer,
    thread: *mut c_void,
    finished: *mut c_void,
    exit: *mut c_void,
    lock: *mut c_void,
    muted: AtomicBool,
}

/// The objects as the decisions see them: the nodes and samples (Rust's), the manager's mixer and sound path.
struct Objects {
    mixer: *mut EeSoundMixer,
    path: *mut c_void,
}

fn sample_ptr(s: Sample) -> *mut EeSoundSample {
    s.0.cast()
}

impl Objects {
    fn spim(&self) -> &mixerobj::EeSoundSpim {
        unsafe { spim(self.mixer) }
    }
}

impl Host for Objects {
    fn node_new(&mut self) -> Node {
        Node(objects::node_new().cast())
    }
    fn node_reference(&mut self, n: Node) {
        unsafe { objects::ee_sound_node_reference(n.0.cast()) }
    }
    fn node_dereference(&mut self, n: Node) {
        unsafe { objects::ee_sound_node_dereference(n.0.cast()) }
    }
    fn node(&self, n: Node) -> NodeFields {
        unsafe { *objects::fields(n.0) }
    }
    fn set_node(&mut self, n: Node, f: &NodeFields) {
        unsafe { *objects::fields(n.0) = *f }
    }
    fn sample_is_looping(&self, s: Sample) -> bool {
        unsafe { objects::ee_sound_sample_is_looping(sample_ptr(s)) }
    }
    fn sample_set_looping(&mut self, s: Sample, looping: bool) {
        unsafe { objects::ee_sound_sample_set_is_looping(sample_ptr(s), looping) }
    }
    fn sample_volume(&self, s: Sample) -> f32 {
        unsafe { objects::ee_sound_sample_volume(sample_ptr(s)) }
    }
    fn sample_pan(&self, s: Sample) -> f32 {
        unsafe { objects::ee_sound_sample_pan(sample_ptr(s)) }
    }
    fn sample_pitch(&self, s: Sample) -> f32 {
        unsafe { objects::ee_sound_sample_pitch(sample_ptr(s)) }
    }
    fn sample_set_volume(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { objects::ee_sound_sample_set_volume(sample_ptr(s), v) }
    }
    fn sample_set_pan(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { objects::ee_sound_sample_set_pan(sample_ptr(s), v) }
    }
    fn sample_set_pitch(&mut self, s: Sample, v: f32) -> i32 {
        unsafe { objects::ee_sound_sample_set_pitch(sample_ptr(s), v) }
    }
    fn sample_mixer_channel(&self, s: Sample) -> u32 {
        unsafe { objects::ee_sound_sample_mixer_channel(sample_ptr(s)) }
    }
    fn sample_set_mixer_channel(&mut self, s: Sample, channel: u32) {
        unsafe { objects::ee_sound_sample_set_mixer_channel(sample_ptr(s), channel) }
    }
    fn sample_length(&self, s: Sample) -> u32 {
        unsafe { objects::ee_sound_sample_length_in_samples(sample_ptr(s)) }
    }
    fn sample_rate(&self, s: Sample) -> u32 {
        unsafe { objects::ee_sound_sample_rate(sample_ptr(s)) }
    }
    fn sample_load(&mut self, file: FileSpec) -> Sample {
        let s = objects::sample_new();
        unsafe { objects::ee_sound_sample_load_wave_data(s, file.0) };
        Sample(s.cast())
    }
    fn sample_duplicate(&mut self, s: Sample) -> Option<Sample> {
        let mut copy = null_mut();
        unsafe { self.spim().duplicate_sample.expect("SPIMDuplicateSample")(s.0, &mut copy) };
        (!copy.is_null()).then_some(Sample(copy))
    }
    fn sample_delete(&mut self, s: Option<Sample>) {
        if let Some(s) = s {
            unsafe { objects::sample_delete(sample_ptr(s)) }
        }
    }
    fn find_file(&mut self, name: &[u8]) -> Option<FileSpec> {
        if self.path.is_null() {
            return None;
        }
        let mut z = name.to_vec();
        z.push(0);
        let f = unsafe { sys::ee_soundcpp_find_file(self.path, z.as_ptr() as *const c_char) };
        (!f.is_null()).then_some(FileSpec(f))
    }
    fn file_delete(&mut self, f: FileSpec) {
        unsafe { sys::ee_soundcpp_file_delete(f.0) }
    }
    fn stop_channel(&mut self, channel: u32) {
        unsafe { self.spim().stop_channel.expect("SPIMStopChannel")(wide(channel)) };
    }
    fn set_sample_offset(&mut self, s: Sample, offset: u32) {
        unsafe { self.spim().set_sample_offset.expect("SPIMSetSampleOffset")(s.0, offset as c_ulong) };
    }
    fn channel_state(&mut self, channel: u32) -> i32 {
        unsafe { self.spim().is_channel_busy_playing.expect("SPIMIsChannelBusyPlaying")(wide(channel)) as i32 }
    }
    fn playback_time(&mut self, s: Sample) -> (i32, i32) {
        let (mut total, mut remaining): (c_long, c_long) = (0, 0);
        unsafe {
            self.spim().get_sample_playback_time.expect("SPIMGetSamplePlayBackTime")(s.0, &mut total, &mut remaining)
        };
        (total as i32, remaining as i32)
    }
    fn set_channel_sample(&mut self, channel: u32, s: Sample) -> i32 {
        unsafe { self.spim().set_channel_sample.expect("SPIMSetChannelSample")(wide(channel), s.0) as i32 }
    }
    fn play_channel(&mut self, channel: u32) {
        unsafe { self.spim().play_channel.expect("SPIMPlayChannel")(wide(channel)) };
    }
    fn stop_all_channels(&mut self) {
        unsafe { self.spim().stop_all_channels.expect("SPIMStopAllChannels")() };
    }
    fn destroy_channels(&mut self) {
        unsafe { self.spim().destroy_channels.expect("SPIMDestroyChannels")() };
    }
    fn create_channels(&mut self, count: u32) -> i32 {
        let mut description = [0u32; 3];
        unsafe {
            self.spim().create_channels.expect("SPIMCreateChannels")(count as c_ulong, description.as_mut_ptr().cast())
                as i32
        }
    }
}

/// A channel as an `unsigned long`: a none (-1) or paused (-2) channel keeps all bits set on LP64, as the C++ passed
/// its `unsigned long` member.
fn wide(v: u32) -> c_ulong {
    if v >= 0xffff_fffe {
        v as i32 as c_long as c_ulong
    } else {
        v as c_ulong
    }
}

impl EeSoundManager {
    /// The decisions under the manager's lock (taken by the caller where the C++ took it).
    fn run<R>(&self, f: impl FnOnce(&mut SoundManager, &mut Objects) -> R) -> R {
        assert!(!self.busy.replace(true), "ee_sound: the sound manager was re-entered");
        let mut h = Objects { mixer: self.mixer, path: self.path.get() };
        let r = f(unsafe { &mut *self.state.get() }, &mut h);
        self.busy.set(false);
        r
    }

    fn locked<R>(&self, f: impl FnOnce(&mut SoundManager, &mut Objects) -> R) -> R {
        sys::locked(self.lock, || self.run(f))
    }
}

/// The node forms' objects (no manager state: only the node and its sample, through the sample's own mixer).
fn node_objects() -> Objects {
    Objects { mixer: null_mut(), path: null_mut() }
}

/// `SESoundManager::Create` (the constructor): the pool's lock, the path (referenced), the state (every category
/// at 100%; the mixer's channel count when `channel_count` is 0), the lock, the events, the scavenger thread.
///
/// # Safety
/// `mixer` is a live mixer, `path` NULL or a live `FSPath*`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_new(
    path: *mut c_void,
    mixer: *mut EeSoundMixer,
    category_count: u32,
    channel_count: u32,
) -> *mut EeSoundManager {
    sys::verify(!mixer.is_null(), b"inMixer != NULL\0", 204);
    sys::verify(category_count > 0, b"inCategoryCount > 0\0", 205);
    objects::create_pool_lock();
    if !path.is_null() {
        unsafe { sys::ee_soundcpp_path_reference(path) };
    }
    let channels = if channel_count != 0 {
        channel_count
    } else {
        unsafe { spim(mixer).get_channel_count.expect("SPIMGetChannelCount")() as u32 }
    };
    let state = SoundManager::new(category_count, channels);
    let lock = unsafe { sys::ee_ts_section_new() };
    let m = Box::into_raw(Box::new(EeSoundManager {
        busy: Cell::new(false),
        state: UnsafeCell::new(state),
        path: Cell::new(path),
        mixer,
        thread: null_mut(),
        finished: null_mut(),
        exit: null_mut(),
        lock,
        muted: AtomicBool::new(false),
    }));
    unsafe {
        let r = sys::ee_ts_event_new(&mut (*m).finished, true);
        sys::verify(r == 0, b"TSScheduler::CreateEvent(this->mScavengerThreadFinishedEvent) == keseOK\0", 248);
        let r = sys::ee_ts_event_new(&mut (*m).exit, true);
        sys::verify(r == 0, b"TSScheduler::CreateEvent(this->mScavengerThreadExitEvent) == keseOK\0", 249);
        let r = sys::ee_ts_thread_new(&mut (*m).thread, Some(scavenger), m.cast(), 0, false);
        sys::verify(
            r == 0,
            b"TSScheduler::CreateThread(this->mScavengerThread, SESoundManager::Scavenger, this) == keseOK\0",
            255,
        );
        sys::ee_ts_thread_set_name((*m).thread, c"SoundScavngr".as_ptr());
    }
    m
}

/// `SESoundManager::Destroy` (the destructor): the scavenger stopped, the thread, events and lock gone, every paused
/// instance dropped, every sample and category deleted, the mixer's channels stopped and destroyed; the path
/// dereferenced.
///
/// # Safety
/// `m` from `ee_sound_manager_new`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_free(m: *mut EeSoundManager) {
    let me = unsafe { &*m };
    unsafe {
        sys::ee_ts_event_set(me.exit);
        sys::ee_ts_event_wait(me.finished, sys::WAIT_INFINITE);
        sys::ee_ts_thread_free(me.thread);
        sys::ee_ts_event_free(me.finished);
        sys::ee_ts_event_free(me.exit);
        sys::ee_ts_section_free(me.lock);
    }
    me.run(|s, h| s.destroy(h));
    let path = me.path.get();
    drop(unsafe { Box::from_raw(m) });
    if !path.is_null() {
        unsafe { sys::ee_soundcpp_path_dereference(path) };
    }
}

/// The scavenger thread: every second retires finished or silent channels, detaches the samples of silent paused
/// instances and restarts the loudest paused instances on free channels; the one-shot sounds are updated every turn.
unsafe extern "C" fn scavenger(manager: *mut c_void) -> c_ulong {
    sys::verify(!manager.is_null(), b"inThisObject != NULL\0", 2626);
    let m = unsafe { &*(manager as *const EeSoundManager) };
    let events = [m.exit as *const c_void];
    unsafe { sys::ee_soundcpp_fp_precision53() };
    loop {
        unsafe { sys::ee_soundcpp_fp_check() };
        m.locked(|s, h| s.update_one_shots(h));
        match unsafe { sys::ee_ts_event_wait_any(events.as_ptr(), 1, 1000) } {
            0 => {
                unsafe { sys::ee_ts_event_clear(m.exit) };
                break;
            }
            sys::WAIT_TIMEOUT => {
                let muted = m.muted.load(Ordering::SeqCst);
                m.locked(|s, h| s.scavenge(h, muted));
                unsafe { sys::PlatSleep(1) };
            }
            sys::WAIT_FAILED => {
                unsafe { sys::PlatGetLastError() };
            }
            _ => {}
        }
    }
    unsafe { sys::ee_ts_event_set(m.finished) };
    0
}

/// # Safety
/// `m` is a live manager.
unsafe fn me<'a>(m: *const EeSoundManager) -> &'a EeSoundManager {
    unsafe { &*m }
}

fn node_ptr(n: Option<Node>) -> *mut EeSoundNode {
    n.map_or(null_mut(), |n| n.0.cast())
}

/// `GetAvailableChannels`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_available_channels(m: *const EeSoundManager) -> u32 {
    unsafe { me(m) }.locked(|s, _| s.available_channels)
}

/// `GetInstanceID(node)` (`NONE`: no node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_instance_id(m: *const EeSoundManager, node: *mut EeSoundNode) -> u32 {
    if node.is_null() {
        return NONE;
    }
    sys::locked(unsafe { me(m) }.lock, || unsafe { objects::fields(node.cast()).instance_id })
}

/// `GetSampleID(node)` (`NONE`: no node).
///
/// # Safety
/// As `ee_sound_manager_node_instance_id`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_sample_id(m: *const EeSoundManager, node: *mut EeSoundNode) -> u32 {
    if node.is_null() {
        return NONE;
    }
    sys::locked(unsafe { me(m) }.lock, || unsafe { objects::fields(node.cast()).sample_id })
}

/// `GetIsLooping(id)`.
///
/// # Safety
/// `m` is a live manager when `id` is not none.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_is_looping(m: *const EeSoundManager, id: u32) -> bool {
    if id == NONE {
        return false;
    }
    unsafe { me(m) }.locked(|s, h| s.is_looping(h, id))
}

/// The node form (`false` without a node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_is_looping(m: *const EeSoundManager, node: *mut EeSoundNode) -> bool {
    if node.is_null() {
        return false;
    }
    sys::locked(unsafe { me(m) }.lock, || SoundManager::node_is_looping(&node_objects(), Node(node.cast())))
}
/// The node form (`false` without a node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_is_playing(m: *const EeSoundManager, node: *mut EeSoundNode) -> bool {
    if node.is_null() {
        return false;
    }
    sys::locked(unsafe { me(m) }.lock, || SoundManager::node_is_playing(&node_objects(), Node(node.cast())))
}
/// The node form (`0.0` without a node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_pan(m: *const EeSoundManager, node: *mut EeSoundNode) -> f32 {
    if node.is_null() {
        return 0.0;
    }
    sys::locked(unsafe { me(m) }.lock, || SoundManager::node_pan(&node_objects(), Node(node.cast())))
}
/// The node form (`0.0` without a node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_pitch(m: *const EeSoundManager, node: *mut EeSoundNode) -> f32 {
    if node.is_null() {
        return 0.0;
    }
    sys::locked(unsafe { me(m) }.lock, || SoundManager::node_pitch(&node_objects(), Node(node.cast())))
}

/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_is_playing(m: *const EeSoundManager, id: u32) -> bool {
    unsafe { me(m) }.locked(|s, h| s.is_playing(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_pan(m: *const EeSoundManager, id: u32) -> f32 {
    unsafe { me(m) }.locked(|s, h| s.pan(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_pitch(m: *const EeSoundManager, id: u32) -> f32 {
    unsafe { me(m) }.locked(|s, h| s.pitch(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_sample_id(m: *const EeSoundManager, id: u32) -> u32 {
    unsafe { me(m) }.locked(|s, h| s.sample_id(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_sample_rate(m: *const EeSoundManager, id: u32) -> u32 {
    unsafe { me(m) }.locked(|s, h| s.sample_rate(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_time_left(m: *const EeSoundManager, id: u32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.time_left_of(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_total_play_time(m: *const EeSoundManager, id: u32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.total_play_time_of(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_volume(m: *const EeSoundManager, id: u32) -> f32 {
    unsafe { me(m) }.locked(|s, h| s.volume(h, id))
}
/// The instance-id form, under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_unload(m: *const EeSoundManager, id: u32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.unload(h, id))
}

/// `GetAndReferenceSoundInstance` (NULL: none).
///
/// # Safety
/// `m` is a live manager when `id` is not none.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_get_and_reference(m: *const EeSoundManager, id: u32) -> *mut EeSoundNode {
    if id == NONE {
        return null_mut();
    }
    unsafe { me(m) }.locked(|s, h| {
        let n = s.find_instance(h, id);
        if let Some(n) = n {
            h.node_reference(n);
        }
        node_ptr(n)
    })
}

/// `Load` -> the sample id (`NONE`: not loaded).
///
/// # Safety
/// `m` is a live manager, `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_load(
    m: *const EeSoundManager,
    name: *const c_char,
    category: u32,
    max: u32,
) -> u32 {
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    unsafe { me(m) }.locked(|s, h| s.load(h, name, category, max))
}

/// `PlayAndReturnReferencedNode` (NULL: not played).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_play_node(
    m: *const EeSoundManager,
    sample_id: u32,
    volume: f32,
    pan: f32,
    pitch: f32,
    looping: bool,
) -> *mut EeSoundNode {
    unsafe { me(m) }.locked(|s, h| node_ptr(s.play(h, sample_id, volume, pan, pitch, looping)))
}

/// `Play` -> the new instance id (`NONE`: not played).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_play(
    m: *const EeSoundManager,
    sample_id: u32,
    volume: f32,
    pan: f32,
    pitch: f32,
    looping: bool,
) -> u32 {
    let n = unsafe { ee_sound_manager_play_node(m, sample_id, volume, pan, pitch, looping) };
    if n.is_null() {
        return NONE;
    }
    sys::locked(unsafe { me(m) }.lock, || {
        let id = unsafe { objects::fields(n.cast()).instance_id };
        unsafe { objects::ee_sound_node_dereference(n) };
        id
    })
}

/// `RegisterOneShotSound`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_register_one_shot(m: *const EeSoundManager, sample_id: u32, id: u32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.register_one_shot(h, sample_id, id))
}

/// `SetCategoryPercentage` -> the category's instance limit.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_category_percentage(
    m: *const EeSoundManager,
    category: u32,
    percent: f32,
) -> u32 {
    let p = percent as f64;
    sys::verify((0.0..=1.0).contains(&p), b"inPercent >= 0.0 && inPercent <= 1.0\0", 2936);
    unsafe { me(m) }.locked(|s, _| s.set_category_percentage(category, percent))
}

/// `SetCategoryPriority`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_category_priority(
    m: *const EeSoundManager,
    category: u32,
    priority: u32,
) -> i32 {
    unsafe { me(m) }.locked(|s, _| s.set_category_priority(category, priority))
}

/// `SetMaxChannels`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_max_channels(m: *const EeSoundManager, count: u32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.set_max_channels(h, count))
}

/// `SetPath`: the old path dereferenced, the new one referenced.
///
/// # Safety
/// `m` is a live manager, `path` NULL or a live `FSPath*`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_path(m: *const EeSoundManager, path: *mut c_void) {
    let m = unsafe { me(m) };
    let old = m.path.get();
    if !old.is_null() {
        unsafe { sys::ee_soundcpp_path_dereference(old) };
    }
    m.path.set(path);
    if !path.is_null() {
        unsafe { sys::ee_soundcpp_path_reference(path) };
    }
}

/// `GetPath`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_path(m: *const EeSoundManager) -> *mut c_void {
    unsafe { me(m) }.path.get()
}

/// `SetVolume(id)`, `SetPan(id)`, `SetPitch(id)`, `SetSoundParameters(id)`: under the lock.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_volume(m: *const EeSoundManager, id: u32, v: f32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.set_volume(h, id, v))
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_pan(m: *const EeSoundManager, id: u32, v: f32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.set_pan(h, id, v))
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_pitch(m: *const EeSoundManager, id: u32, v: f32) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.set_pitch(h, id, v))
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_parameters(
    m: *const EeSoundManager,
    id: u32,
    volume: f32,
    pan: f32,
    pitch: f32,
) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.set_parameters(h, id, volume, pan, pitch))
}

/// The node forms of `SetVolume`, `SetPan`, `SetPitch`: the lock is taken even without a node (as the C++ took it).
///
/// # Safety
/// `m` is a live manager, `node` NULL or a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_set(
    m: *const EeSoundManager,
    node: *mut EeSoundNode,
    which: u32,
    v: f32,
) -> i32 {
    sys::locked(unsafe { me(m) }.lock, || {
        if node.is_null() {
            return 0;
        }
        let h = &mut node_objects();
        let n = Node(node.cast());
        match which {
            0 => SoundManager::node_set_volume(h, n, v),
            1 => SoundManager::node_set_pan(h, n, v),
            _ => SoundManager::node_set_pitch(h, n, v),
        }
    })
}

/// `SetSoundParameters(node)` (no lock without a node).
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_node_set_parameters(
    m: *const EeSoundManager,
    node: *mut EeSoundNode,
    volume: f32,
    pan: f32,
    pitch: f32,
) -> i32 {
    if node.is_null() {
        return 0;
    }
    sys::locked(unsafe { me(m) }.lock, || {
        SoundManager::node_set_parameters(&mut node_objects(), Node(node.cast()), volume, pan, pitch)
    })
}

/// `Stop(id, letFinish)`.
///
/// # Safety
/// `m` is a live manager when `id` is not none.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_stop(m: *const EeSoundManager, id: u32, let_finish: bool) -> i32 {
    if id == NONE {
        return 0;
    }
    unsafe { me(m) }.locked(|s, h| s.stop(h, id, let_finish))
}

/// `Stop(node, flag)`: by the node's instance id, read under the lock.
///
/// # Safety
/// `node` is NULL or a live node; `m` is live when it is not.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_stop_node(
    m: *const EeSoundManager,
    node: *mut EeSoundNode,
    flag: bool,
) -> i32 {
    if node.is_null() {
        return 0;
    }
    let id = sys::locked(unsafe { me(m) }.lock, || unsafe { objects::fields(node.cast()).instance_id });
    if id != NONE {
        unsafe { ee_sound_manager_stop(m, id, flag) };
    }
    0
}

/// `StopAllOneShotSounds`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_stop_all_one_shots(m: *const EeSoundManager) -> i32 {
    unsafe { me(m) }.locked(|s, h| s.stop_all_one_shots(h))
}

/// `GetCriticalSection` (a `TSCriticalSection*`).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_lock(m: *const EeSoundManager) -> *mut c_void {
    unsafe { me(m) }.lock
}

/// `GetIsMuted` / `SetIsMuted` (read by the scavenger's pass).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_muted(m: *const EeSoundManager) -> bool {
    unsafe { me(m) }.muted.load(Ordering::SeqCst)
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_muted(m: *const EeSoundManager, muted: bool) {
    unsafe { me(m) }.muted.store(muted, Ordering::SeqCst)
}

/// The manager's mixer (`GetMasterVolume`, `SetMasterVolume`, `GetMaxChannels`, ... are the mixer's).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_mixer(m: *const EeSoundManager) -> *mut EeSoundMixer {
    unsafe { me(m) }.mixer
}

/// `GetHighestTimerDelay`, `GetMaxChannels` (the mixer's).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_mixer_value(m: *const EeSoundManager, which: u32) -> u32 {
    let s = unsafe { spim(me(m).mixer) };
    unsafe {
        match which {
            0 => s.get_highest_timer_delay.expect("SPIMGetHighestTimerDelay")() as u32,
            _ => s.get_maximum_channel_count.expect("SPIMGetMaximumChannelCount")() as u32,
        }
    }
}

/// `GetMasterVolume`.
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_master_volume(m: *const EeSoundManager) -> f32 {
    unsafe { spim(me(m).mixer).get_volume.expect("SPIMGetVolume")() }
}

/// `SetMasterVolume`, `SetLatency(ms)`, `MixAhead` (the mixer's results).
///
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_master_volume(m: *const EeSoundManager, v: f32) -> i32 {
    unsafe { spim(me(m).mixer).set_volume.expect("SPIMSetVolume")(v) as i32 }
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_set_latency(m: *const EeSoundManager, milliseconds: u32) -> i32 {
    unsafe { spim(me(m).mixer).set_latency.expect("SPIMSetLatency")(milliseconds as c_ulong) as i32 }
}
/// # Safety
/// `m` is a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_manager_mix_ahead(m: *const EeSoundManager) -> i32 {
    unsafe { spim(me(m).mixer).mix_ahead.expect("SPIMMixAhead")() as i32 }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn none_and_paused_channels_keep_every_bit() {
        assert_eq!(wide(3), 3);
        assert_eq!(wide(NONE) as u64 & 0xffff_ffff, 0xffff_ffff);
        assert_eq!(wide(NONE - 1) as u64 & 0xffff_ffff, 0xffff_fffe);
        if std::mem::size_of::<c_ulong>() == 8 {
            assert_eq!(wide(NONE) as u64, u64::MAX);
        }
    }
}
