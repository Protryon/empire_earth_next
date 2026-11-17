//! The ambient sounds' C ABI (include/rust/ee_app.h): one callback host by operation.

use crate::app::ambient::{
    self, AmbientHost, EeAppAmbientCounts, EeAppAmbientFields, EeAppAmbientRecord, EeAppAmbientSoundFields,
};
use std::ffi::c_void;

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppAmbientIo {
    /// the sound's slot
    pub slot: u32,
    /// a count, a time
    pub n: u32,
    pub flag: bool,
    /// a second answer (`LoadSound`'s loop flag: `has` set when it was loaded now)
    pub has: bool,
    pub volume: f32,
    pub pan: f32,
    pub pitch: f32,
    /// the slot's record
    pub sound: *mut EeAppAmbientSoundFields,
}

impl Default for EeAppAmbientIo {
    fn default() -> Self {
        EeAppAmbientIo {
            slot: 0,
            n: 0,
            flag: false,
            has: false,
            volume: 0.0,
            pan: 0.0,
            pitch: 0.0,
            sound: std::ptr::null_mut(),
        }
    }
}

/// The host: `(ctx, op, io)`.
pub type EeAppAmbientFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppAmbientIo);

// ---- the operations (io's fields: in -> out)
/// -> n: the sounds' count; slot -> sound: the slot's record
pub const EE_APP_AMBIENT_OP_SOUNDS: i32 = 1;
pub const EE_APP_AMBIENT_OP_SOUND: i32 = 2;
/// -> volume: `mRandom.Generate()`
pub const EE_APP_AMBIENT_OP_RANDOM: i32 = 3;
/// slot, volume, pan, pitch: `gSoundManager->Play` of the slot's sample
pub const EE_APP_AMBIENT_OP_PLAY: i32 = 4;
/// slot -> flag: an instance plays
pub const EE_APP_AMBIENT_OP_HAS_INSTANCE: i32 = 5;
/// slot, volume, pan, pitch, flag (looping): the instance started
pub const EE_APP_AMBIENT_OP_START: i32 = 6;
/// slot, volume, pan, pitch: `SetSoundParameters`
pub const EE_APP_AMBIENT_OP_SET_PARAMETERS: i32 = 7;
/// slot: `Stop(mInstance, true)`, the instance released
pub const EE_APP_AMBIENT_OP_STOP_INSTANCE: i32 = 8;
/// slot -> flag: a sound object; has: loaded now, then n its loop flag (`LoadSound`)
pub const EE_APP_AMBIENT_OP_LOAD_SOUND: i32 = 9;
/// slot: the media unloaded, the object forgotten
pub const EE_APP_AMBIENT_OP_UNLOAD_SOUND: i32 = 10;
/// -> volume: the time of day; -> n: `TMGetTime()`; -> n: the visible tiles; -> volume: the water's, the space's share
pub const EE_APP_AMBIENT_OP_TIME_OF_DAY: i32 = 11;
pub const EE_APP_AMBIENT_OP_TIME: i32 = 12;
pub const EE_APP_AMBIENT_OP_VISIBLE_TILES: i32 = 13;
pub const EE_APP_AMBIENT_OP_VISIBLE_WATER: i32 = 14;
pub const EE_APP_AMBIENT_OP_VISIBLE_SPACE: i32 = 15;

struct Host {
    ctx: *mut c_void,
    call: EeAppAmbientFn,
}

impl Host {
    fn op(&mut self, op: i32, io: EeAppAmbientIo) -> EeAppAmbientIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.call)(self.ctx, op, &mut io) };
        io
    }
    fn at(&mut self, op: i32, slot: usize) -> EeAppAmbientIo {
        self.op(op, EeAppAmbientIo { slot: slot as u32, ..Default::default() })
    }
    fn sound_op(&mut self, op: i32, slot: usize, volume: f32, pan: f32, pitch: f32, flag: bool) {
        self.op(op, EeAppAmbientIo { slot: slot as u32, volume, pan, pitch, flag, ..Default::default() });
    }
}

impl AmbientHost for Host {
    fn sounds(&mut self) -> usize {
        self.op(EE_APP_AMBIENT_OP_SOUNDS, EeAppAmbientIo::default()).n as usize
    }
    fn sound(&mut self, slot: usize) -> *mut EeAppAmbientSoundFields {
        self.at(EE_APP_AMBIENT_OP_SOUND, slot).sound
    }
    fn random(&mut self) -> f32 {
        self.op(EE_APP_AMBIENT_OP_RANDOM, EeAppAmbientIo::default()).volume
    }
    fn play(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32) {
        self.sound_op(EE_APP_AMBIENT_OP_PLAY, slot, volume, pan, pitch, false);
    }
    fn has_instance(&mut self, slot: usize) -> bool {
        self.at(EE_APP_AMBIENT_OP_HAS_INSTANCE, slot).flag
    }
    fn start(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32, looping: bool) {
        self.sound_op(EE_APP_AMBIENT_OP_START, slot, volume, pan, pitch, looping);
    }
    fn set_parameters(&mut self, slot: usize, volume: f32, pan: f32, pitch: f32) {
        self.sound_op(EE_APP_AMBIENT_OP_SET_PARAMETERS, slot, volume, pan, pitch, false);
    }
    fn stop_instance(&mut self, slot: usize) {
        self.at(EE_APP_AMBIENT_OP_STOP_INSTANCE, slot);
    }
    fn load_sound(&mut self, slot: usize) -> (bool, Option<bool>) {
        let io = self.at(EE_APP_AMBIENT_OP_LOAD_SOUND, slot);
        (io.flag, io.has.then_some(io.n != 0))
    }
    fn unload_sound(&mut self, slot: usize) {
        self.at(EE_APP_AMBIENT_OP_UNLOAD_SOUND, slot);
    }
    fn time_of_day(&mut self) -> f32 {
        self.op(EE_APP_AMBIENT_OP_TIME_OF_DAY, EeAppAmbientIo::default()).volume
    }
    fn time(&mut self) -> u32 {
        self.op(EE_APP_AMBIENT_OP_TIME, EeAppAmbientIo::default()).n
    }
    fn visible_tiles(&mut self) -> u32 {
        self.op(EE_APP_AMBIENT_OP_VISIBLE_TILES, EeAppAmbientIo::default()).n
    }
    fn visible_water(&mut self) -> f32 {
        self.op(EE_APP_AMBIENT_OP_VISIBLE_WATER, EeAppAmbientIo::default()).volume
    }
    fn visible_space(&mut self) -> f32 {
        self.op(EE_APP_AMBIENT_OP_VISIBLE_SPACE, EeAppAmbientIo::default()).volume
    }
}

/// The manager's and the counts' records copied in, the body run, copied back (the host touches neither).
fn with<R>(
    m: *mut EeAppAmbientFields,
    c: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
    body: impl FnOnce(&mut EeAppAmbientFields, &mut EeAppAmbientCounts, &mut Host) -> R,
) -> R {
    // SAFETY: the entries' contract: live records
    let (mut mm, mut cc) = unsafe { (*m, *c) };
    let r = body(&mut mm, &mut cc, &mut Host { ctx, call });
    // SAFETY: as above
    unsafe {
        *m = mm;
        *c = cc;
    }
    r
}

/// `EEAmbientSoundManager()`'s plain members.
///
/// # Safety
/// `out` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_default(out: *mut EeAppAmbientFields) {
    unsafe { *out = EeAppAmbientFields::default() };
}

/// `EEAmbientSound()`'s plain members.
///
/// # Safety
/// `out` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_sound_default(out: *mut EeAppAmbientSoundFields) {
    unsafe { *out = EeAppAmbientSoundFields::default() };
}

/// `EEAmbientSound::Init(record)`.
///
/// # Safety
/// live records.
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_sound_init(
    out: *mut EeAppAmbientSoundFields,
    record: *const EeAppAmbientRecord,
) {
    unsafe { ambient::init(&mut *out, &*record) };
}

/// `EEAmbientWorldAnalyzer::Clear`.
///
/// # Safety
/// `counts` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_clear(counts: *mut EeAppAmbientCounts) {
    unsafe { ambient::clear(&mut *counts) };
}

/// `Update`.
///
/// # Safety
/// live records; `call` answers for `ctx` throughout.
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_update(
    manager: *mut EeAppAmbientFields,
    counts: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
) {
    with(manager, counts, ctx, call, ambient::update);
}

/// `UpdateState(time of day, time)`.
///
/// # Safety
/// as [`ee_app_ambient_update`]
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_update_state(
    manager: *mut EeAppAmbientFields,
    counts: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
    time_of_day: f32,
    time: i32,
) {
    with(manager, counts, ctx, call, |m, c, h| ambient::update_state(m, c, h, time_of_day, time as _));
}

/// `SetEnabled(enabled)`.
///
/// # Safety
/// as [`ee_app_ambient_update`]
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_set_enabled(
    manager: *mut EeAppAmbientFields,
    counts: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
    enabled: bool,
) {
    with(manager, counts, ctx, call, |m, _c, h| ambient::set_enabled(m, h, enabled));
}

/// `StopSounds` (`pause`: then `Pause`'s flag).
///
/// # Safety
/// as [`ee_app_ambient_update`]
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_stop_sounds(
    manager: *mut EeAppAmbientFields,
    counts: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
    pause: bool,
) {
    with(manager, counts, ctx, call, |m, _c, h| if pause { ambient::pause(m, h) } else { ambient::stop_sounds(h) });
}

/// `EEAmbientSound::PlayAtVolume(volume)` (0 or less: the sound stopped and silent).
///
/// # Safety
/// as [`ee_app_ambient_update`]
#[no_mangle]
pub unsafe extern "C" fn ee_app_ambient_play_at_volume(
    manager: *mut EeAppAmbientFields,
    counts: *mut EeAppAmbientCounts,
    ctx: *mut c_void,
    call: EeAppAmbientFn,
    slot: u32,
    volume: f32,
) {
    with(manager, counts, ctx, call, |_m, _c, h| ambient::play_at_volume(h, slot as usize, volume));
}
