//! The EE lobby's sounds as Rust objects (R6 WONLobby round 7; the original EELobbySound.cpp): EELobbyMusic (the
//! lobby music: "playing" it raises the game's music volume to the player's setting, stopping it sets 0, resuming
//! plays again), EELobbySound (a lobby sound effect: a sample of the engine's sound manager, played once at full
//! volume, centred, its playing instance remembered; stopping stops that instance; freeing unloads the sample) and
//! EELobbySoundDecoder's load (a sample of the path, or none without a sound manager).
//!
//! The C++ keeps the classes (WONGUI's `Sound` / `SoundDecoder` bases and their virtuals, which forward here). Every
//! call on the game's UI manager or sound manager is one op of the host given to `ee_won_eesound_new`
//! (`WONLobby/EELobbyGlueHost.cpp`).

use std::cell::Cell;
use std::ffi::c_void;

/// The two kinds.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeSoundKind {
    Music = 0,
    Effect = 1,
}

/// The host's ops.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeSoundOp {
    /// `gUIManager != NULL` (0 or 1).
    HasUiManager = 1,
    /// `gUIManager->SetMusicVolume(gGamePreferences.mSettings.GetMusicVolume())`.
    MusicVolumeOn,
    /// `gUIManager->SetMusicVolume(0.0f)`.
    MusicVolumeOff,
    /// `gSoundManager != NULL` (0 or 1).
    HasSoundManager,
    /// `gSoundManager->Unload(a)`.
    Unload,
    /// `gSoundManager->Play(a, 1.0f, 0.0f, 1.0f, false, NULL)`: the instance.
    Play,
    /// `gSoundManager->Stop(a, false)`.
    Stop,
    /// `gSoundManager->Load(path, 0, 1)` (`a` the descriptor's path, a NUL-terminated `char*`): the sample.
    Load,
}

/// The inward calls (the classes' virtuals).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonEeSoundCall {
    /// `PlayHook()`.
    Play = 1,
    /// `StopHook()`.
    Stop,
    /// `ResumeHook()` (the music's; an effect's is the base's empty one).
    Resume,
    /// a member: 0 `mSampleID`, 1 `mInstanceID`.
    Field,
}

/// The host: `(object, op, a) -> result` (`object` the C++ sound, or null for the decoder's load).
pub type EeWonEeSoundHostFn = Option<unsafe extern "C" fn(obj: *mut c_void, op: EeWonEeSoundOp, a: isize) -> isize>;

/// The original's "no instance" (`-1` in an `unsigned long`; `UIsNone32`).
const NONE: u32 = u32::MAX;

/// One lobby sound.
pub struct EeWonEeSound {
    obj: *mut c_void,
    kind: EeWonEeSoundKind,
    host: EeWonEeSoundHostFn,
    sample: u32,
    instance: Cell<u32>,
}

fn call_host(host: EeWonEeSoundHostFn, obj: *mut c_void, op: EeWonEeSoundOp, a: isize) -> isize {
    match host {
        // SAFETY: the host's contract (an op on its own object)
        Some(h) => unsafe { h(obj, op, a) },
        None => 0,
    }
}

impl EeWonEeSound {
    pub fn new(obj: *mut c_void, kind: EeWonEeSoundKind, host: EeWonEeSoundHostFn, sample: u32) -> Self {
        EeWonEeSound { obj, kind, host, sample, instance: Cell::new(NONE) }
    }

    fn op(&self, op: EeWonEeSoundOp, a: isize) -> isize {
        call_host(self.host, self.obj, op, a)
    }

    fn play(&self) {
        match self.kind {
            EeWonEeSoundKind::Music => {
                if self.op(EeWonEeSoundOp::HasUiManager, 0) != 0 {
                    self.op(EeWonEeSoundOp::MusicVolumeOn, 0);
                }
            }
            EeWonEeSoundKind::Effect => {
                if self.op(EeWonEeSoundOp::HasSoundManager, 0) != 0 {
                    let i = self.op(EeWonEeSoundOp::Play, self.sample as isize);
                    self.instance.set(i as u32);
                }
            }
        }
    }

    fn stop(&self) {
        match self.kind {
            EeWonEeSoundKind::Music => {
                if self.op(EeWonEeSoundOp::HasUiManager, 0) != 0 {
                    self.op(EeWonEeSoundOp::MusicVolumeOff, 0);
                }
            }
            EeWonEeSoundKind::Effect => {
                if self.op(EeWonEeSoundOp::HasSoundManager, 0) != 0 && self.instance.get() != NONE {
                    self.op(EeWonEeSoundOp::Stop, self.instance.get() as isize);
                }
                self.instance.set(NONE);
            }
        }
    }

    pub fn call(&self, call: EeWonEeSoundCall, a: isize) -> isize {
        match call {
            EeWonEeSoundCall::Play => self.play(),
            EeWonEeSoundCall::Stop => self.stop(),
            EeWonEeSoundCall::Resume => {
                if self.kind == EeWonEeSoundKind::Music {
                    self.play();
                }
            }
            EeWonEeSoundCall::Field => {
                return match a {
                    0 => self.sample as isize,
                    1 => self.instance.get() as isize,
                    _ => 0,
                }
            }
        }
        0
    }

    /// The effect's destructor: its sample unloaded.
    fn release(&self) {
        if self.kind == EeWonEeSoundKind::Effect && self.op(EeWonEeSoundOp::HasSoundManager, 0) != 0 {
            self.op(EeWonEeSoundOp::Unload, self.sample as isize);
        }
    }
}

/// A new sound for the C++ object `obj` (an effect of sample `sample`; the music's is unused).
#[no_mangle]
pub extern "C" fn ee_won_eesound_new(
    obj: *mut c_void,
    kind: EeWonEeSoundKind,
    host: EeWonEeSoundHostFn,
    sample: u32,
) -> *mut EeWonEeSound {
    Box::into_raw(Box::new(EeWonEeSound::new(obj, kind, host, sample)))
}

/// The C++ object's destructor: an effect's sample unloaded, the object freed.
///
/// # Safety
/// `p` came from [`ee_won_eesound_new`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_eesound_free(p: *mut EeWonEeSound) {
    if !p.is_null() {
        // SAFETY: the contract
        let s = unsafe { Box::from_raw(p) };
        s.release();
    }
}

/// A virtual of the C++ object.
///
/// # Safety
/// `p` is a live object of [`ee_won_eesound_new`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_eesound_call(p: *const EeWonEeSound, call: EeWonEeSoundCall, a: isize) -> isize {
    // SAFETY: the contract; the host's calls do not come back into this object
    match unsafe { p.as_ref() } {
        Some(s) => s.call(call, a),
        None => 0,
    }
}

/// `EELobbySoundDecoder::Decode`'s load: the sample of `path` (a NUL-terminated `char*`), or -1 without a sound
/// manager (no sound is made then).
#[no_mangle]
pub extern "C" fn ee_won_eesound_load(host: EeWonEeSoundHostFn, path: *const u8) -> i64 {
    if call_host(host, std::ptr::null_mut(), EeWonEeSoundOp::HasSoundManager, 0) == 0 {
        return -1;
    }
    call_host(host, std::ptr::null_mut(), EeWonEeSoundOp::Load, path as isize) as u32 as i64
}

#[cfg(test)]
#[path = "eesound_tests.rs"]
mod tests;
