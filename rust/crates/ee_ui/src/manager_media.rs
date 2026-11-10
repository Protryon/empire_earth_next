//! `UIManager`'s music bookkeeping and its IME members (`User Interface/UIManager.cpp`: `SetMusicVolume`,
//! `StartMusic`, `StopMusic`, `UpdateUI`'s pending stop and start, `SetSoundInstance`, `RemoveSoundInstance`,
//! `RemoveAndStopSoundInstance`; `AddIMEResultString`, `ChangeIMECompositionString`, `CloseIMECompositionString`,
//! `OpenIMECompositionString`, `ChangeIMECandidateWindow`, `OpenIMECandidateWindow`, `CloseIMECandidateWindow` and
//! `CopyData`'s candidate window).
//!
//! The logic only: the sound manager, the playing instance (a reference-counted C++ node), the forms, the candidate
//! window and the locks stay C++ and are host calls ([`MusicHost`], [`ImeHost`]). The track's file name is game text
//! (UTF-8 bytes, decision D6), compared byte for byte as `UWideString`'s `operator!=` did.

use core::ffi::{c_long, c_ulong, c_void};

use crate::rect::UiRect;

/// A C++ `SESMInstanceNode*`, opaque (NULL: none).
pub type Instance = *mut c_void;

/// `StartMusic`'s default track when the volume comes up with no track chosen yet.
pub const INTRO: &[u8] = b"sounds\\eeintro.mp3";

/// The manager's music members and the sound manager as the bookkeeping asks about them.
pub trait MusicHost {
    /// `mSoundManager != NULL`.
    fn has_sound_manager(&self) -> bool;
    /// `mMusicVolume`.
    fn volume(&self) -> f32;
    fn set_volume(&mut self, v: f32);
    /// `mMusicFileName` (bytes).
    fn file_name(&self) -> Vec<u8>;
    fn set_file_name(&mut self, name: &[u8]);
    /// `PlatMbsLwr` of a copy of the name.
    fn lower(&self, name: &[u8]) -> Vec<u8>;
    /// `mMusicID` (-1: none).
    fn music_id(&self) -> c_long;
    fn set_music_id(&mut self, id: c_long);
    /// `mStartMusic`, `mStopMusic`.
    fn start_flag(&self) -> bool;
    fn set_start_flag(&mut self, v: bool);
    fn stop_flag(&self) -> bool;
    fn set_stop_flag(&mut self, v: bool);
    /// `mMusicInstance`.
    fn instance(&self) -> Instance;
    fn set_instance_field(&mut self, i: Instance);
    /// The instance's `Reference()` / `Dereference()`.
    fn reference(&mut self, i: Instance);
    fn dereference(&mut self, i: Instance);
    /// The sound manager's `Stop(i, false)`, `Unload(id)`, `Load(name, 0, 1)` (-1: failed),
    /// `PlayAndReturnReferencedNode(id, volume, 0, 1, true, NULL)`, `SetSoundParameters(i, volume, 0, 1)`.
    fn stop(&mut self, i: Instance);
    fn unload(&mut self, id: c_long);
    fn load(&mut self, name: &[u8]) -> c_long;
    fn play(&mut self, id: c_long, volume: f32) -> Instance;
    fn set_parameters(&mut self, i: Instance, volume: f32);
    /// `mCriticalSection->Enter()` / `Leave()`.
    fn lock(&mut self);
    fn unlock(&mut self);
}

/// `RemoveSoundInstance`: drops the manager's reference to the playing instance.
pub fn remove_sound_instance(h: &mut impl MusicHost) {
    let i = h.instance();
    if !i.is_null() {
        h.dereference(i);
    }
    h.set_instance_field(core::ptr::null_mut());
}

/// `RemoveAndStopSoundInstance`: stops the playing instance and drops it.
pub fn remove_and_stop_sound_instance(h: &mut impl MusicHost) {
    let i = h.instance();
    if !i.is_null() {
        h.stop(i);
        remove_sound_instance(h);
    }
}

/// `SetSoundInstance`: the reference moves to the new instance (nothing for the current one).
pub fn set_sound_instance(h: &mut impl MusicHost, i: Instance) {
    if i == h.instance() {
        return;
    }
    remove_sound_instance(h);
    h.set_instance_field(i);
    if !i.is_null() {
        h.reference(i);
    }
}

/// Plays the loaded track when nothing plays (the node comes referenced: the manager keeps one reference).
fn play_if_silent(h: &mut impl MusicHost) {
    if h.instance().is_null() {
        let (id, v) = (h.music_id(), h.volume());
        let node = h.play(id, v);
        set_sound_instance(h, node);
        let i = h.instance();
        if !i.is_null() {
            h.dereference(i);
        }
    }
}

/// `StartMusic`: loads and starts a track unless it is the current one (the names compared lower-cased; the lower-
/// cased name is kept, the file is loaded by the name as given).
pub fn start_music(h: &mut impl MusicHost, name: &[u8]) {
    if !h.has_sound_manager() {
        return;
    }
    let lower = h.lower(name);
    if h.file_name() == lower {
        return;
    }
    h.set_stop_flag(true);
    if h.music_id() != -1 {
        remove_and_stop_sound_instance(h);
        let id = h.music_id();
        h.unload(id);
    }
    let id = h.load(name);
    h.set_music_id(id);
    if id != -1 {
        h.set_file_name(&lower);
        h.set_start_flag(true);
        h.lock();
        play_if_silent(h);
        h.unlock();
    }
}

/// `StopMusic`: stops the music and unloads the track.
pub fn stop_music(h: &mut impl MusicHost) {
    remove_and_stop_sound_instance(h);
    if h.music_id() == -1 {
        return;
    }
    let id = h.music_id();
    h.unload(id);
    h.set_music_id(-1);
    h.set_file_name(b"");
    h.set_stop_flag(true);
}

/// `SetMusicVolume`: down to 0 stops the music; up from 0 restarts the last track (or the intro); the playing
/// instance takes the new volume.
pub fn set_music_volume(h: &mut impl MusicHost, v: f32) {
    if h.volume() > 0.0 && v <= 0.0 {
        stop_music(h);
    } else if h.volume() <= 0.0 && v > 0.0 {
        let name = h.file_name();
        if name.is_empty() {
            start_music(h, INTRO);
        } else {
            h.set_file_name(b"");
            start_music(h, &name);
        }
    }
    h.set_volume(v);
    let i = h.instance();
    if !i.is_null() {
        h.set_parameters(i, v);
    }
}

/// `UpdateUI`'s music step (inside the lock): a pending stop, then a pending start.
pub fn update_music(h: &mut impl MusicHost) {
    if h.stop_flag() {
        remove_and_stop_sound_instance(h);
        h.set_stop_flag(false);
    }
    if h.start_flag() {
        play_if_silent(h);
        h.set_start_flag(false);
    }
}

/// The IME calls on the top form.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ImeCall {
    /// `AddIMEResultString` (no result).
    AddResult = 0,
    ChangeComposition = 1,
    CloseComposition = 2,
    OpenComposition = 3,
}

/// The manager's IME members as their logic asks about them.
pub trait ImeHost {
    fn lock(&mut self);
    fn unlock(&mut self);
    /// `mFormStack.size() > 0`.
    fn has_form(&self) -> bool;
    /// The call on `mFormStack.back()` -> its result.
    fn form_call(&mut self, call: ImeCall) -> c_long;
    /// The top form's keyboard-holding control: whether there is one and it is IME-enabled (`GetIMEEnabled()`, a
    /// virtual), and its rectangle. `current`: through `GetCurrentForm()`, else the stack's back.
    fn captured_ime(&mut self, current: bool) -> bool;
    fn captured_rect(&self, current: bool) -> UiRect;
    /// The window of the manager's screen: whether there is one, its client origin and size.
    fn window(&mut self) -> Option<(f32, f32, c_ulong, c_ulong)>;
    /// `mCandidateWindowOpen`, `mDrawCandidateWindow`.
    fn candidate_open(&self) -> bool;
    fn set_candidate_open(&mut self, v: bool);
    fn set_draw_candidate(&mut self, v: bool);
    /// The candidate window's `SetCandidateList(list, left)`, `OpenCandidateWindow(screen, control)`,
    /// `CloseCandidateWindow()`, `CopyData()`.
    fn candidate_set_list(&mut self, left: f32);
    fn candidate_open_window(&mut self, screen: UiRect, control: UiRect);
    fn candidate_close(&mut self);
    fn candidate_copy(&mut self) -> c_long;
}

/// `AddIMEResultString`, `ChangeIMECompositionString`, `CloseIMECompositionString`, `OpenIMECompositionString`: the
/// call on the top form under the UI lock -> its result (`kErrorOK` without a form).
pub fn ime_form_call(h: &mut impl ImeHost, call: ImeCall) -> c_long {
    let mut result = 0;
    h.lock();
    if h.has_form() {
        result = h.form_call(call);
    }
    h.unlock();
    result
}

/// `ChangeIMECandidateWindow`: for the IME-enabled keyboard control of the current form, a style-2 list of at most
/// one candidate closes the window, any other list opens it at the control's left.
pub fn change_candidate_window(h: &mut impl ImeHost, style: c_ulong, count: c_ulong) -> c_long {
    h.lock();
    if h.captured_ime(true) {
        if style & 2 != 0 && count <= 1 {
            h.set_candidate_open(false);
        } else {
            h.set_candidate_open(true);
            let left = h.captured_rect(true).left;
            h.candidate_set_list(left);
        }
    }
    h.unlock();
    0
}

/// `OpenIMECandidateWindow`: over the window's client area, for the top form's IME-enabled keyboard control.
pub fn open_candidate_window(h: &mut impl ImeHost) -> c_long {
    h.lock();
    if h.has_form() && h.captured_ime(false) {
        if let Some((x, y, w, ht)) = h.window() {
            h.set_candidate_open(true);
            let left = x;
            let right = w as f32 + left;
            let top = y;
            let bottom = ht as f32 + top;
            let control = h.captured_rect(false);
            h.candidate_open_window(UiRect::new(left, top, right, bottom), control);
        }
    }
    h.unlock();
    0
}

/// `CloseIMECandidateWindow`.
pub fn close_candidate_window(h: &mut impl ImeHost) -> c_long {
    h.candidate_close();
    h.set_candidate_open(false);
    0
}

/// `CopyData`'s candidate window: drawn this frame when open -> its copy's result, or `result` unchanged.
pub fn copy_candidate_window(h: &mut impl ImeHost, result: c_long) -> c_long {
    let open = h.candidate_open();
    h.set_draw_candidate(open);
    if open {
        h.candidate_copy()
    } else {
        result
    }
}

#[cfg(test)]
mod tests;
