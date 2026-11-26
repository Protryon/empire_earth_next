//! The music manager's thread and members (game core lane, round 11, lane B; were EEMusicManager.cpp's
//! constructor, destructor, `MusicManagerMain`, and the `ee_musiccpp_get`, `_set`, `_current_epoch` and
//! `_song_reverb` trampolines): the events, the thread and the wait are ee_base's Thread System (the C++ inlines'
//! callees), the members a record from `mRandom` on ([`EeGamecppMusicMembers`], asserted by EEMusicManager.cpp),
//! the decisions ee_audio sound music.rs's (`ee_music_step`). The world's and the songs' reads are one-statement
//! cases of EEGameLog.cpp's `EECore11OpB` ([`super::EeGameCpp`]'s `Em` ops).
//!
//! The music thread runs in every game with music on (the tour; EE-SIM makes no music manager).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};
use std::ptr::{addr_of, addr_of_mut, read_volatile, write_volatile};

use ee_base::random::EeURandom;
use ee_base::ts::{self, EeTsEvent, EeTsThread, EE_TS_OK, EE_TS_WAIT_INFINITE};

use super::{h, hd, hp, O, P};

/// `EEMusicManager`'s members after its vtable (ee/EEMusicManager.h), in order.
#[repr(C)]
#[allow(non_snake_case)]
pub struct EeGamecppMusicMembers {
    pub mRandom: EeURandom,
    pub mConductor: *mut c_void,
    pub mThread: *mut EeTsThread,
    /// mThreadCreated, mThreadDone, mExit, mPause, mSetIntensity, mStart, mStop
    pub mEvents: [*mut EeTsEvent; 7],
    pub mMusic: *mut c_void,
    pub mScoreID: c_long,
    pub mState: c_long,
    pub mIntensityDelay: c_ulong,
    pub mIntensity: c_ulong,
    /// volatile (Update re-reads it)
    pub mTimeToNextSong: c_ulong,
    pub mRequestedIntensity: c_ulong,
}

const CREATED: usize = 0;
const DONE: usize = 1;
/// the thread's five wake-ups, in WaitAny's order: exit, pause, intensity, start, stop
const EXIT: usize = 2;
const WAKE_UPS: usize = 5;

/// `kFPPrecision53`.
const PRECISION_53: c_ulong = 0x10000;

fn members(m: *mut c_void) -> *mut EeGamecppMusicMembers {
    hp(O::EmMembers, m as P, &[]) as *mut EeGamecppMusicMembers
}

fn verify(ok: bool, line: c_ulong, expression: &'static [u8]) {
    if !ok {
        let mut e = expression.to_vec();
        e.push(0);
        // SAFETY: C strings
        unsafe {
            ee_base::error::ee_error_verify(
                c"C:\\EEX\\Empire Earth\\Empire Earth\\EEMusicManager.cpp".as_ptr(),
                line,
                e.as_ptr().cast(),
            )
        };
    }
}

/// The asserted expressions of the events' creation, by event (the constructor's lines 124..130).
const EVENT_EXPRESSIONS: [&[u8]; 7] = [
    b"TSScheduler::CreateEvent(this->mThreadCreated, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mThreadDone, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mExit, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mPause, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mSetIntensity, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mStart, false) == keseOK",
    b"TSScheduler::CreateEvent(this->mStop, false) == keseOK",
];

/// The constructor: no song, intensity 0, ten minutes to the next song, the generator seeded with the time, the
/// seven events, the music thread started at `priority` and waited for, then the conductor's intensity.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_music_init(manager: *mut c_void, priority: c_long) {
    let m = members(manager);
    // SAFETY: the manager's members; the thread starts after they are set and reads them as the C++'s did
    unsafe {
        (*m).mMusic = std::ptr::null_mut();
        (*m).mScoreID = 0;
        (*m).mIntensityDelay = 0;
        (*m).mIntensity = 0;
        (*m).mRequestedIntensity = 0;
        (*m).mState = 0;
        write_volatile(addr_of_mut!((*m).mTimeToNextSong), 600000);
        (*m).mRandom.seed = ee_base::time::ee_time_get();
        for (k, expression) in EVENT_EXPRESSIONS.iter().enumerate() {
            let r = ts::ee_ts_event_new(addr_of_mut!((*m).mEvents[k]), false);
            verify(r == EE_TS_OK, 124 + k as c_ulong, expression);
        }
        let r = ts::ee_ts_thread_new(addr_of_mut!((*m).mThread), Some(ee_gamecpp_music_main), manager, 0, false);
        verify(
            r == EE_TS_OK,
            133,
            b"TSScheduler::CreateThread(this->mThread, EEMusicManager::MusicManagerMain, (void*)this) == keseOK",
        );
        let r = ts::ee_ts_thread_set_priority((*m).mThread, priority as i32);
        verify(r == EE_TS_OK, 134, b"this->mThread->SetPriority(inThreadPriority) == keseOK");
        ts::ee_ts_event_wait((*m).mEvents[CREATED], EE_TS_WAIT_INFINITE);
        ts::ee_ts_thread_set_name((*m).mThread, c"MusicManager".as_ptr());
        h(O::EmConductorIntensity, manager as P, &[(*m).mIntensity as i64]);
    }
}

/// The destructor: the thread told to exit and waited for, the events and the thread freed.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_music_destroy(manager: *mut c_void) {
    let m = members(manager);
    // SAFETY: the manager's members
    unsafe {
        ts::ee_ts_event_set((*m).mEvents[EXIT]);
        ts::ee_ts_event_wait((*m).mEvents[DONE], EE_TS_WAIT_INFINITE);
        for e in (*m).mEvents {
            ts::ee_ts_event_free(e);
        }
        ts::ee_ts_thread_free((*m).mThread);
    }
}

/// `MusicManagerMain`: the music thread. It waits for exit, pause, intensity, start or stop (or the time to the
/// next song) and hands each wake-up to the Rust step (ee_audio music.rs) until it says to exit.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_music_main(manager: *mut c_void) -> c_ulong {
    let m = members(manager);
    let mut last = ee_base::time::ee_time_get();
    // SAFETY: the manager's members (its events live until the destructor has waited for this thread)
    unsafe {
        ts::ee_ts_event_set((*m).mEvents[CREATED]);
        let events: [*const EeTsEvent; WAKE_UPS] = std::array::from_fn(|k| (*m).mEvents[EXIT + k] as *const EeTsEvent);
        ee_base::util::fp::ee_util_fp_set_precision(PRECISION_53);
        loop {
            if ee_base::util::fp::ee_util_fp_precision() != PRECISION_53 {
                ee_base::util::fp::ee_util_fp_set_precision(PRECISION_53);
            }
            if ee_base::util::fp::ee_util_fp_rounding() != 0 {
                ee_base::util::fp::ee_util_fp_set_rounding(0);
            }
            let timeout = read_volatile(addr_of!((*m).mTimeToNextSong));
            let result = ts::ee_ts_event_wait_any(events.as_ptr(), WAKE_UPS as u32, timeout as u32);
            let now = ee_base::time::ee_time_get();
            let elapsed = now.wrapping_sub(last);
            last = now;
            if ee_audio::sound::music_ffi::ee_music_step(manager, result, elapsed) {
                break;
            }
        }
        ts::ee_ts_event_set((*m).mEvents[DONE]);
    }
    0
}

/// The members `ee_musiccpp_get`/`set` reach by number (ee_audio music_ffi.rs: 0 score id, 1 state, 2 intensity
/// delay, 3 intensity, 4 time to the next song, 5 requested intensity), as 32-bit values.
#[no_mangle]
pub unsafe extern "C" fn ee_musiccpp_get(manager: *mut c_void, field: u32) -> u32 {
    let m = members(manager);
    // SAFETY: the manager's members
    unsafe {
        match field {
            0 => (*m).mScoreID as u32,
            1 => (*m).mState as u32,
            2 => (*m).mIntensityDelay as u32,
            3 => (*m).mIntensity as u32,
            4 => read_volatile(addr_of!((*m).mTimeToNextSong)) as u32,
            _ => (*m).mRequestedIntensity as u32,
        }
    }
}

/// as [`ee_musiccpp_get`]
#[no_mangle]
pub unsafe extern "C" fn ee_musiccpp_set(manager: *mut c_void, field: u32, value: u32) {
    let m = members(manager);
    // SAFETY: the manager's members
    unsafe {
        match field {
            0 => (*m).mScoreID = value as i32 as c_long,
            1 => (*m).mState = value as i32 as c_long,
            2 => (*m).mIntensityDelay = value as c_ulong,
            3 => (*m).mIntensity = value as c_ulong,
            4 => write_volatile(addr_of_mut!((*m).mTimeToNextSong), value as c_ulong),
            _ => (*m).mRequestedIntensity = value as c_ulong,
        }
    }
}

/// The local player's current epoch (false with no player or tech tree).
#[no_mangle]
pub unsafe extern "C" fn ee_musiccpp_current_epoch(epoch: *mut i32) -> bool {
    let player = hp(O::EcCurrentPlayer, 0, &[]);
    let tree = if player != 0 { crate::tech_tree::objects::player_tree(player) } else { 0 };
    if tree == 0 {
        return false;
    }
    // SAFETY: the caller's out value
    unsafe { *epoch = h(O::EmCurrentEpoch, tree, &[]) as i32 };
    true
}

/// A song's reverb: gain, high-frequency ratio, mix, time.
#[no_mangle]
pub unsafe extern "C" fn ee_musiccpp_song_reverb(song: *mut c_void, out: *mut f32) {
    let s = song as P;
    let r = [O::EmReverbGain, O::EmReverbHighFreqRatio, O::EmReverbMix, O::EmReverbTime].map(|op| hd(op, s, &[], &[]));
    // SAFETY: the caller's four floats
    unsafe { std::ptr::copy_nonoverlapping(r.as_ptr(), out, 4) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_members_are_the_classes_order() {
        use std::mem::offset_of;
        let pointer = std::mem::size_of::<*mut c_void>();
        assert_eq!(offset_of!(EeGamecppMusicMembers, mConductor), 16);
        assert_eq!(offset_of!(EeGamecppMusicMembers, mThread), 16 + pointer);
        assert_eq!(offset_of!(EeGamecppMusicMembers, mMusic), 16 + 9 * pointer);
        assert_eq!(offset_of!(EeGamecppMusicMembers, mScoreID), 16 + 10 * pointer);
        assert_eq!(EVENT_EXPRESSIONS[EXIT], b"TSScheduler::CreateEvent(this->mExit, false) == keseOK");
        assert_eq!(EXIT + WAKE_UPS, 7);
    }
}
