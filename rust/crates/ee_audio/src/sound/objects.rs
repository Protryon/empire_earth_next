//! The Sound Engine's small objects whole in Rust: the instance node (`SESMInstanceNode`: pooled, reference counted,
//! held by units, unit classes and effects), the sample (`SESample`: a mixer's wave data with its playing state) and
//! the score (`SEScore`: a dmusic segment). The C++ classes are data-less: an `SESMInstanceNode*` is an
//! [`EeSoundNode`], an `SESample*` an [`EeSoundSample`], an `SEScore*` an [`EeSoundScore`], and every member is a
//! one-line forwarder to the functions here.
//!
//! The node pool keeps the original's lock (`sSoundNodePoolLock`, a Thread System section created by every sound
//! manager and destroyed by `CleanupMemoryPoolAndDestroyCriticalSections`): a node is taken and returned under it,
//! as the C++ did. Freed nodes stay allocated and are reused last-in first-out (the `UMemoryPool` reused its free
//! slots the same way); the cleanup frees the free ones only (the original freed its chunks, live nodes with them).

// the C ABI's `long`/`unsigned long` are 32-bit on Windows and 64-bit on LP64: the casts are not no-ops there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_ulong, c_void, CStr};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicPtr, AtomicU32, Ordering};
use std::sync::Mutex;

use crate::sound::manager::{NodeFields, NONE};
use crate::sound::mixerobj::{self, EeSoundMixer};
use crate::sound::sys;

/// An instance node: its reference count, free flag and fields.
pub struct EeSoundNode {
    refs: AtomicU32,
    free: std::cell::Cell<bool>,
    pub(crate) fields: std::cell::UnsafeCell<NodeFields>,
}

const CLEARED: NodeFields = NodeFields {
    category: NONE,
    sample_id: NONE,
    max_instances: 0,
    instance_id: NONE,
    instance_index: NONE,
    channel: NONE,
    sample: null_mut(),
    volume: 0.0,
    pan: 0.0,
    pitch: 0.0,
};

/// The free nodes (the pool's free slots).
struct Pool(Vec<*mut EeSoundNode>);
unsafe impl Send for Pool {}
static POOL: Mutex<Pool> = Mutex::new(Pool(Vec::new()));
/// `sSoundNodePoolLock`.
static POOL_LOCK: AtomicPtr<c_void> = AtomicPtr::new(null_mut());

fn pool() -> std::sync::MutexGuard<'static, Pool> {
    POOL.lock().unwrap_or_else(|e| e.into_inner())
}

/// `CreateSoundPoolLock`: a new section (a previous one is left as it was, as the original left it).
pub(crate) fn create_pool_lock() {
    POOL_LOCK.store(unsafe { sys::ee_ts_section_new() }, Ordering::SeqCst);
}

/// `GetNewSoundInstance`: a cleared node with one reference, taken under the pool's lock.
pub(crate) fn node_new() -> *mut EeSoundNode {
    let lock = POOL_LOCK.load(Ordering::SeqCst);
    sys::locked(lock, || {
        let n = pool().0.pop().unwrap_or_else(|| {
            Box::into_raw(Box::new(EeSoundNode {
                refs: AtomicU32::new(0),
                free: std::cell::Cell::new(true),
                fields: std::cell::UnsafeCell::new(CLEARED),
            }))
        });
        // InitializeFromPool: Clear, then the first reference
        let node = unsafe { &*n };
        unsafe { *node.fields.get() = CLEARED };
        node.free.set(false);
        node.refs.fetch_add(1, Ordering::SeqCst);
        n
    })
}

/// # Safety
/// `n` is a live node.
pub(crate) unsafe fn fields<'a>(n: *mut c_void) -> &'a mut NodeFields {
    unsafe { &mut *(*(n as *mut EeSoundNode)).fields.get() }
}

/// `SESMInstanceNode::Reference`.
///
/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_reference(node: *mut EeSoundNode) {
    unsafe { (*node).refs.fetch_add(1, Ordering::SeqCst) };
}

/// `SESMInstanceNode::Dereference`: the last reference returns the node to the pool (under the pool's lock).
///
/// # Safety
/// `node` is a live node.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_node_dereference(node: *mut EeSoundNode) {
    let n = unsafe { &*node };
    let mut r = n.refs.load(Ordering::SeqCst);
    while r > 1 {
        match n.refs.compare_exchange(r, r - 1, Ordering::SeqCst, Ordering::SeqCst) {
            Ok(_) => return,
            Err(now) => r = now,
        }
    }
    // LastDereference
    n.refs.store(0, Ordering::SeqCst);
    n.free.set(true);
    sys::locked(POOL_LOCK.load(Ordering::SeqCst), || pool().0.push(node));
}

/// `SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections`: once; frees the free nodes and the pool's lock.
#[no_mangle]
pub extern "C" fn ee_sound_pool_cleanup() {
    let lock = POOL_LOCK.load(Ordering::SeqCst);
    if lock.is_null() {
        return;
    }
    sys::locked(lock, || {
        for n in std::mem::take(&mut pool().0) {
            drop(unsafe { Box::from_raw(n) });
        }
    });
    unsafe { sys::ee_ts_section_free(lock) };
    POOL_LOCK.store(null_mut(), Ordering::SeqCst);
}

/// A sample: the mixer it was made for, its wave data (the mixer's) and its playing state.
pub struct EeSoundSample {
    /// NUL-terminated.
    filename: Vec<u8>,
    data: *mut c_void,
    pub(crate) mixer: *mut EeSoundMixer,
    pan: f32,
    pitch: f32,
    volume: f32,
    /// `NONE`: not on a channel.
    mixer_channel: u32,
    length_bytes: u32,
    length_samples: u32,
    bits: u32,
    rate: u32,
    looping: bool,
}

/// `new SESample`: full volume, centred, normal pitch, no channel, for the active mixer.
pub(crate) fn sample_new() -> *mut EeSoundSample {
    Box::into_raw(Box::new(EeSoundSample {
        filename: vec![0],
        data: null_mut(),
        mixer: mixerobj::active(),
        pan: 0.0,
        pitch: 1.0,
        volume: 1.0,
        mixer_channel: NONE,
        length_bytes: 0,
        length_samples: 0,
        bits: 0,
        rate: 0,
        looping: false,
    }))
}

/// `delete sample`: the mixer deletes its wave data first (NULL: nothing).
///
/// # Safety
/// `s` is NULL or a live sample, not used afterwards.
pub(crate) unsafe fn sample_delete(s: *mut EeSoundSample) {
    if s.is_null() {
        return;
    }
    unsafe { mixerobj::delete_wave_data((*s).mixer, s) };
    drop(unsafe { Box::from_raw(s) });
}

/// # Safety
/// `s` is a live sample.
unsafe fn sample<'a>(s: *mut EeSoundSample) -> &'a mut EeSoundSample {
    unsafe { &mut *s }
}

/// The copy constructor (`SPIMDuplicateSample`'s `new SESample(*inSample)`): the state without the wave data and
/// the channel.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_duplicate(s: *const EeSoundSample) -> *mut EeSoundSample {
    let s = unsafe { &*s };
    Box::into_raw(Box::new(EeSoundSample {
        filename: s.filename.clone(),
        data: null_mut(),
        mixer: s.mixer,
        pan: s.pan,
        pitch: s.pitch,
        volume: s.volume,
        mixer_channel: NONE,
        length_bytes: s.length_bytes,
        length_samples: s.length_samples,
        bits: s.bits,
        rate: s.rate,
        looping: s.looping,
    }))
}

/// `data`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_data(s: *mut EeSoundSample) -> *mut c_void {
    unsafe { sample(s).data }
}
/// `looping`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_is_looping(s: *mut EeSoundSample) -> bool {
    unsafe { sample(s).looping }
}
/// `length_bytes`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_length_in_bytes(s: *mut EeSoundSample) -> u32 {
    unsafe { sample(s).length_bytes }
}
/// `length_samples`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_length_in_samples(s: *mut EeSoundSample) -> u32 {
    unsafe { sample(s).length_samples }
}
/// `mixer_channel`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_mixer_channel(s: *mut EeSoundSample) -> u32 {
    unsafe { sample(s).mixer_channel }
}
/// `pan`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_pan(s: *mut EeSoundSample) -> f32 {
    unsafe { sample(s).pan }
}
/// `pitch`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_pitch(s: *mut EeSoundSample) -> f32 {
    unsafe { sample(s).pitch }
}
/// `bits`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_bits(s: *mut EeSoundSample) -> u32 {
    unsafe { sample(s).bits }
}
/// `rate`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_rate(s: *mut EeSoundSample) -> u32 {
    unsafe { sample(s).rate }
}
/// `volume`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_volume(s: *mut EeSoundSample) -> f32 {
    unsafe { sample(s).volume }
}
/// The plain store of `data` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_data(s: *mut EeSoundSample, v: *mut c_void) {
    unsafe { sample(s).data = v };
}
/// The plain store of `length_bytes` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_length_in_bytes(s: *mut EeSoundSample, v: u32) {
    unsafe { sample(s).length_bytes = v };
}
/// The plain store of `length_samples` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_length_in_samples(s: *mut EeSoundSample, v: u32) {
    unsafe { sample(s).length_samples = v };
}
/// The plain store of `mixer_channel` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_mixer_channel(s: *mut EeSoundSample, v: u32) {
    unsafe { sample(s).mixer_channel = v };
}
/// The plain store of `bits` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_bits(s: *mut EeSoundSample, v: u32) {
    unsafe { sample(s).bits = v };
}
/// The plain store of `rate` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_rate(s: *mut EeSoundSample, v: u32) {
    unsafe { sample(s).rate = v };
}
/// The plain store of `looping` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_the_is_looping(s: *mut EeSoundSample, v: bool) {
    unsafe { sample(s).looping = v };
}
/// The plain store of `pan` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_the_pan(s: *mut EeSoundSample, v: f32) {
    unsafe { sample(s).pan = v };
}
/// The plain store of `pitch` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_the_pitch(s: *mut EeSoundSample, v: f32) {
    unsafe { sample(s).pitch = v };
}
/// The plain store of `volume` (the mixer is not told).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_the_volume(s: *mut EeSoundSample, v: f32) {
    unsafe { sample(s).volume = v };
}

/// `GetFilename`: NUL-terminated, valid until the next `SetFilename`.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_filename(s: *mut EeSoundSample) -> *const c_char {
    unsafe { sample(s).filename.as_ptr() as *const c_char }
}

/// `SetFilename`.
///
/// # Safety
/// `s` is a live sample, `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_filename(s: *mut EeSoundSample, name: *const c_char) {
    let mut v = unsafe { CStr::from_ptr(name) }.to_bytes().to_vec();
    v.push(0);
    unsafe { sample(s).filename = v };
}

/// Through the sample's mixer: `SetIsLooping` (`SPIMSetSampleLooping`).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_is_looping(s: *mut EeSoundSample, looping: bool) {
    unsafe { mixerobj::spim(sample(s).mixer).set_sample_looping.expect("SPIMSetSampleLooping")(s.cast(), looping) }
}

/// `SetVolume`, `SetPan`, `SetPitch` through the sample's mixer: the mixer's result.
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_volume(s: *mut EeSoundSample, v: f32) -> i32 {
    unsafe { mixerobj::spim(sample(s).mixer).set_sample_volume.expect("SPIMSetSampleVolume")(s.cast(), v) as i32 }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_pan(s: *mut EeSoundSample, v: f32) -> i32 {
    unsafe { mixerobj::spim(sample(s).mixer).set_sample_pan.expect("SPIMSetSamplePan")(s.cast(), v) as i32 }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_pitch(s: *mut EeSoundSample, v: f32) -> i32 {
    unsafe { mixerobj::spim(sample(s).mixer).set_sample_pitch.expect("SPIMSetSamplePitch")(s.cast(), v) as i32 }
}
/// `SetOffset` (`SPIMSetSampleOffset`).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_set_offset(s: *mut EeSoundSample, offset: u32) -> i32 {
    unsafe {
        mixerobj::spim(sample(s).mixer).set_sample_offset.expect("SPIMSetSampleOffset")(s.cast(), offset as c_ulong)
            as i32
    }
}
/// `GetSampleOffset` (`SPIMGetSampleOffset`).
///
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_offset(s: *mut EeSoundSample) -> u32 {
    unsafe { mixerobj::spim(sample(s).mixer).get_sample_offset.expect("SPIMGetSampleOffset")(s.cast()) as u32 }
}
/// `LoadWaveData(spec)` (`SPIMLoadWaveData`).
///
/// # Safety
/// `s` is a live sample, `spec` a live `FSFileSpec*`.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_sample_load_wave_data(s: *mut EeSoundSample, spec: *mut c_void) -> i32 {
    unsafe { mixerobj::spim(sample(s).mixer).load_wave_data.expect("SPIMLoadWaveData")(spec, s.cast()) as i32 }
}

// The plug-in's view of a sample (rust/crates/ee_audio/src/mixer's `ee_mixer_host_*`; the mixer plug-in's sample access).

/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_mixer_channel(s: *mut c_void) -> u32 {
    unsafe { sample(s.cast()).mixer_channel }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_mixer_channel(s: *mut c_void, channel: u32) {
    unsafe { sample(s.cast()).mixer_channel = channel };
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_data(s: *mut c_void) -> *const c_void {
    unsafe { sample(s.cast()).data }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_data(s: *mut c_void, data: *const c_void) {
    unsafe { sample(s.cast()).data = data as *mut c_void };
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_is_looping(s: *mut c_void) -> bool {
    unsafe { sample(s.cast()).looping }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_pan(s: *mut c_void) -> f32 {
    unsafe { sample(s.cast()).pan }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_pitch(s: *mut c_void) -> f32 {
    unsafe { sample(s.cast()).pitch }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_volume(s: *mut c_void) -> f32 {
    unsafe { sample(s.cast()).volume }
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_is_looping(s: *mut c_void, looping: bool) {
    unsafe { sample(s.cast()).looping = looping };
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_pan(s: *mut c_void, v: f32) {
    unsafe { sample(s.cast()).pan = v };
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_pitch(s: *mut c_void, v: f32) {
    unsafe { sample(s.cast()).pitch = v };
}
/// # Safety
/// `s` is a live sample.
#[no_mangle]
pub unsafe extern "C" fn ee_mixer_host_set_volume(s: *mut c_void, v: f32) {
    unsafe { sample(s.cast()).volume = v };
}

/// A score: one dmusic segment (one reference, released with the score). The original's secondary scores (one per
/// motif of the segment's style) do not exist on dmusic: none is ever made.
pub struct EeSoundScore {
    pub(crate) segment: *mut c_void,
    pub(crate) secondary: bool,
}

pub(crate) fn score_new(segment: *mut c_void, secondary: bool) -> *mut EeSoundScore {
    Box::into_raw(Box::new(EeSoundScore { segment, secondary }))
}

/// `delete score`: releases its segment.
///
/// # Safety
/// `s` is a live score, not used afterwards.
pub(crate) unsafe fn score_delete(s: *mut EeSoundScore) {
    let s = unsafe { Box::from_raw(s) };
    unsafe { sys::DmSegment_release(s.segment) };
}

/// `SEScore::GetIsSecondaryScore`.
///
/// # Safety
/// `s` is a live score.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_score_is_secondary(s: *const EeSoundScore) -> bool {
    unsafe { (*s).secondary }
}

/// `SEScore::GetRepeatCount`.
///
/// # Safety
/// `s` is a live score.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_score_repeat_count(s: *const EeSoundScore) -> u32 {
    unsafe { sys::DmSegment_getRepeats((*s).segment) }
}

/// `SEScore::SetRepeatCount` (the game loops a score with a large count).
///
/// # Safety
/// `s` is a live score.
#[no_mangle]
pub unsafe extern "C" fn ee_sound_score_set_repeat_count(s: *const EeSoundScore, count: u32) {
    unsafe { sys::DmSegment_setRepeats((*s).segment, count) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_duplicate_keeps_the_state_but_not_the_wave_or_channel() {
        let s = Box::into_raw(Box::new(EeSoundSample {
            filename: b"x.wav\0".to_vec(),
            data: 8 as *mut c_void,
            mixer: null_mut(),
            pan: 0.5,
            pitch: 2.0,
            volume: 0.25,
            mixer_channel: 3,
            length_bytes: 10,
            length_samples: 5,
            bits: 8,
            rate: 22050,
            looping: true,
        }));
        unsafe {
            let d = ee_sound_sample_duplicate(s);
            assert!((*d).data.is_null());
            assert_eq!((*d).mixer_channel, NONE);
            assert_eq!((*d).filename, b"x.wav\0");
            assert_eq!(((*d).pan, (*d).pitch, (*d).volume), (0.5, 2.0, 0.25));
            assert_eq!(((*d).length_bytes, (*d).length_samples, (*d).bits, (*d).rate), (10, 5, 8, 22050));
            assert!((*d).looping);
            drop(Box::from_raw(d));
            drop(Box::from_raw(s));
        }
    }
}
