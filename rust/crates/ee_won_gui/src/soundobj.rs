//! The lobby's sounds as Rust objects (R6 WON GUI round 41): WONAPI's Sound, SoundDescriptor and SoundDecoder
//! (Sound.cpp, SoundDecoder.cpp) and the game's EELobbyMusic, EELobbySound and EELobbySoundDecoder (EELobbySound.cpp)
//! went; a sound is one of these, which C++ holds as a RefCount (`SmartPtr<Sound>`, `Sound` an empty class): no
//! vtable, its count at +8, the kind `EE_WON_RC_SOUND` at +12 (`RefCount::Release` gives the last reference to
//! `ee_won_gstr_free`, which frees it by its kind).
//!
//! A sound keeps the descriptor's flags (4: muted) and path and its looping flag; what playing it does is its hook,
//! the game's music or effect (ee_won_lobby eesound.rs); a sound without one (the window manager's without a
//! decoder) plays nothing. The decoder (`WindowManager::DecodeSound`'s `mSoundDecoder`, the game's
//! EELobbySoundDecoder) is the function `set_decoder` installs.

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::Mutex;

/// The kind of a Rust-made RefCount (the 32 bits after its count) that is a sound.
pub const EE_WON_RC_SOUND: u32 = 2;
/// The descriptor's muted flag (`Sound::SetMuted`, `GetMuted`).
pub const EE_WON_SOUND_MUTED: i32 = 4;

/// What playing a sound does (the original's PlayHook, StopHook and ResumeHook virtuals; RewindHook was empty in
/// every class). Dropped with the sound (EELobbySound's destructor unloads its sample).
pub trait SoundHook {
    fn play(&self);
    fn stop(&self);
    fn resume(&self) {}
}

/// One sound.
#[repr(C)]
pub struct EeWonSound {
    vtable: *const c_void,
    refs: i32,
    kind: u32,
    flags: Cell<i32>,
    looping: Cell<bool>,
    path: Vec<u8>,
    hook: Option<Box<dyn SoundHook>>,
}

/// The calls (`ee_won_sound_call`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSoundCall {
    /// `Play()`: the hook's play unless muted.
    Play = 1,
    /// `Stop()`.
    Stop,
    /// `Resume()`.
    Resume,
    /// `Rewind()` (no hook rewinds).
    Rewind,
    /// `SetLooping(a != 0)`.
    SetLooping,
    /// `SetMuted(a != 0)`: muting stops it.
    SetMuted,
    /// `GetMuted()` (0 or 1).
    GetMuted,
    /// the looping flag (0 or 1).
    Looping,
    /// the descriptor's flags.
    Flags,
}

/// A sound of the flags `flags` and the path `path`, playing through `hook`, with no reference yet (`new Sound`).
pub fn new_sound(flags: i32, path: &[u8], hook: Option<Box<dyn SoundHook>>) -> *mut EeWonSound {
    let s = EeWonSound {
        vtable: std::ptr::null(),
        refs: 0,
        kind: EE_WON_RC_SOUND,
        flags: Cell::new(flags),
        looping: Cell::new(false),
        path: path.to_vec(),
        hook,
    };
    Box::into_raw(Box::new(s))
}

impl EeWonSound {
    fn muted(&self) -> bool {
        self.flags.get() & EE_WON_SOUND_MUTED != 0
    }

    fn stop(&self) {
        if let Some(h) = &self.hook {
            h.stop();
        }
    }

    /// The path of the descriptor it was made from.
    pub fn path(&self) -> &[u8] {
        &self.path
    }

    pub fn call(&self, call: EeWonSoundCall, a: isize) -> isize {
        match call {
            EeWonSoundCall::Play => {
                if !self.muted() {
                    if let Some(h) = &self.hook {
                        h.play();
                    }
                }
            }
            EeWonSoundCall::Stop => self.stop(),
            EeWonSoundCall::Resume => {
                if let Some(h) = &self.hook {
                    h.resume();
                }
            }
            EeWonSoundCall::Rewind => {}
            EeWonSoundCall::SetLooping => self.looping.set(a != 0),
            EeWonSoundCall::SetMuted => {
                let muted = a != 0;
                if muted != self.muted() {
                    let f = self.flags.get();
                    self.flags.set(if muted { f | EE_WON_SOUND_MUTED } else { f & !EE_WON_SOUND_MUTED });
                    if muted {
                        self.stop();
                    }
                }
            }
            EeWonSoundCall::GetMuted => return isize::from(self.muted()),
            EeWonSoundCall::Looping => return isize::from(self.looping.get()),
            EeWonSoundCall::Flags => return self.flags.get() as isize,
        }
        0
    }
}

/// The decoder: a hook for a path (`None`: no sound).
pub type SoundDecoder = fn(&[u8]) -> Option<Box<dyn SoundHook>>;

static DECODER: Mutex<Option<SoundDecoder>> = Mutex::new(None);

/// `WindowManager::SetSoundDecoder` (the game's EELobbySoundDecoder, or none).
pub fn set_decoder(d: Option<SoundDecoder>) {
    *DECODER.lock().unwrap_or_else(|e| e.into_inner()) = d;
}

/// `WindowManager::DecodeSound(desc)`: a plain sound without a decoder, else the decoder's (null when it has none).
pub fn decode(flags: i32, path: &[u8]) -> *mut EeWonSound {
    let d = *DECODER.lock().unwrap_or_else(|e| e.into_inner());
    match d {
        None => new_sound(flags, path, None),
        Some(d) => match d(path) {
            Some(h) => new_sound(flags, path, Some(h)),
            None => std::ptr::null_mut(),
        },
    }
}

/// One of the sound's methods (nothing on a null sound).
///
/// # Safety
/// `s` is null or a live sound.
#[no_mangle]
pub unsafe extern "C" fn ee_won_sound_call(s: *const EeWonSound, call: EeWonSoundCall, a: isize) -> isize {
    // SAFETY: the contract; a hook does not come back into its sound
    match unsafe { s.as_ref() } {
        Some(s) => s.call(call, a),
        None => 0,
    }
}

/// Whether the RefCount `p` (live) is a Rust-made sound.
///
/// # Safety
/// `p` is a live RefCount object.
pub unsafe fn is_sound(p: *const c_void) -> bool {
    // SAFETY: the contract: its vtable slot, then (for one without a vtable) its kind
    !p.is_null() && unsafe { *(p as *const usize) == 0 && (*(p as *const EeWonSound)).kind == EE_WON_RC_SOUND }
}

/// A sound freed at its last reference (its hook dropped).
///
/// # Safety
/// `p` came from `new_sound` and is not used again.
pub(crate) unsafe fn free_sound(p: *mut c_void) {
    // SAFETY: the contract: made by Box
    drop(unsafe { Box::from_raw(p as *mut EeWonSound) });
}

#[cfg(test)]
#[path = "soundobj_tests.rs"]
mod tests;
