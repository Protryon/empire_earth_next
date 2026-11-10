//! `extern "C"` for `UIManager`'s music bookkeeping and IME members (`User Interface/UIManager.cpp`): `manager` is
//! the C++ `UIManager*`, asked through `ee_uicpp_manager_music_*` / `ee_uicpp_manager_ime_*` on each step. `args`
//! is the C++ call's own arguments (the IME strings, the candidate list), passed back to the host untouched.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

use crate::manager_media::{self, ImeCall, ImeHost, Instance, MusicHost};
use crate::rect::UiRect;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

    use crate::rect::UiRect;

    extern "C" {
        // value: 0 mSoundManager != NULL, 1 mMusicID, 2 mStartMusic, 3 mStopMusic
        pub fn ee_uicpp_manager_music_value(manager: *mut c_void, which: c_int) -> c_long;
        pub fn ee_uicpp_manager_music_set_value(manager: *mut c_void, which: c_int, v: c_long);
        pub fn ee_uicpp_manager_music_volume(manager: *mut c_void) -> f32;
        pub fn ee_uicpp_manager_music_set_volume(manager: *mut c_void, v: f32);
        pub fn ee_uicpp_manager_music_name(manager: *mut c_void) -> *const c_char;
        pub fn ee_uicpp_manager_music_set_name(manager: *mut c_void, name: *const c_char);
        pub fn ee_uicpp_manager_music_lower(name: *const c_char, out: *mut u8, capacity: usize);
        pub fn ee_uicpp_manager_music_instance(manager: *mut c_void) -> *mut c_void;
        pub fn ee_uicpp_manager_music_set_instance(manager: *mut c_void, i: *mut c_void);
        // call: 0 Reference, 1 Dereference, 2 the sound manager's Stop(i, false)
        pub fn ee_uicpp_manager_music_instance_call(manager: *mut c_void, call: c_int, i: *mut c_void);
        pub fn ee_uicpp_manager_music_unload(manager: *mut c_void, id: c_long);
        pub fn ee_uicpp_manager_music_load(manager: *mut c_void, name: *const c_char) -> c_long;
        pub fn ee_uicpp_manager_music_play(manager: *mut c_void, id: c_long, volume: f32) -> *mut c_void;
        pub fn ee_uicpp_manager_music_set_parameters(manager: *mut c_void, i: *mut c_void, volume: f32);
        // 0 Enter, 1 Leave (mCriticalSection)
        pub fn ee_uicpp_manager_lock(manager: *mut c_void, leave: c_int);
        pub fn ee_uicpp_manager_ime_has_form(manager: *mut c_void) -> bool;
        pub fn ee_uicpp_manager_ime_form_call(manager: *mut c_void, call: c_int, args: *const c_void) -> c_long;
        pub fn ee_uicpp_manager_ime_captured(manager: *mut c_void, current: bool) -> bool;
        pub fn ee_uicpp_manager_ime_captured_rect(manager: *mut c_void, current: bool) -> UiRect;
        pub fn ee_uicpp_manager_ime_window(manager: *mut c_void, out: *mut f32, size: *mut c_ulong) -> bool;
        // flag: 0 mCandidateWindowOpen, 1 mDrawCandidateWindow
        pub fn ee_uicpp_manager_ime_flag(manager: *mut c_void, flag: c_int) -> bool;
        pub fn ee_uicpp_manager_ime_set_flag(manager: *mut c_void, flag: c_int, v: bool);
        pub fn ee_uicpp_manager_ime_set_list(manager: *mut c_void, list: *const c_void, left: f32);
        pub fn ee_uicpp_manager_ime_open_window(manager: *mut c_void, screen: *const UiRect, control: *const UiRect);
        pub fn ee_uicpp_manager_ime_close_window(manager: *mut c_void);
        pub fn ee_uicpp_manager_ime_copy(manager: *mut c_void) -> c_long;
    }
}

fn c_name(name: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(name.len() + 1);
    v.extend_from_slice(name);
    v.push(0);
    v
}

struct Raw {
    manager: *mut c_void,
    args: *const c_void,
}

impl MusicHost for Raw {
    fn has_sound_manager(&self) -> bool {
        unsafe { sys::ee_uicpp_manager_music_value(self.manager, 0) != 0 }
    }
    fn volume(&self) -> f32 {
        unsafe { sys::ee_uicpp_manager_music_volume(self.manager) }
    }
    fn set_volume(&mut self, v: f32) {
        unsafe { sys::ee_uicpp_manager_music_set_volume(self.manager, v) }
    }
    fn file_name(&self) -> Vec<u8> {
        unsafe { core::ffi::CStr::from_ptr(sys::ee_uicpp_manager_music_name(self.manager)).to_bytes().to_vec() }
    }
    fn set_file_name(&mut self, name: &[u8]) {
        let n = c_name(name);
        unsafe { sys::ee_uicpp_manager_music_set_name(self.manager, n.as_ptr() as *const c_char) }
    }
    fn lower(&self, name: &[u8]) -> Vec<u8> {
        let n = c_name(name);
        let mut out = vec![0u8; n.len()];
        unsafe { sys::ee_uicpp_manager_music_lower(n.as_ptr() as *const c_char, out.as_mut_ptr(), out.len()) };
        let end = out.iter().position(|&b| b == 0).unwrap_or(out.len());
        out.truncate(end);
        out
    }
    fn music_id(&self) -> c_long {
        unsafe { sys::ee_uicpp_manager_music_value(self.manager, 1) }
    }
    fn set_music_id(&mut self, id: c_long) {
        unsafe { sys::ee_uicpp_manager_music_set_value(self.manager, 1, id) }
    }
    fn start_flag(&self) -> bool {
        unsafe { sys::ee_uicpp_manager_music_value(self.manager, 2) != 0 }
    }
    fn set_start_flag(&mut self, v: bool) {
        unsafe { sys::ee_uicpp_manager_music_set_value(self.manager, 2, v as c_long) }
    }
    fn stop_flag(&self) -> bool {
        unsafe { sys::ee_uicpp_manager_music_value(self.manager, 3) != 0 }
    }
    fn set_stop_flag(&mut self, v: bool) {
        unsafe { sys::ee_uicpp_manager_music_set_value(self.manager, 3, v as c_long) }
    }
    fn instance(&self) -> Instance {
        unsafe { sys::ee_uicpp_manager_music_instance(self.manager) }
    }
    fn set_instance_field(&mut self, i: Instance) {
        unsafe { sys::ee_uicpp_manager_music_set_instance(self.manager, i) }
    }
    fn reference(&mut self, i: Instance) {
        unsafe { sys::ee_uicpp_manager_music_instance_call(self.manager, 0, i) }
    }
    fn dereference(&mut self, i: Instance) {
        unsafe { sys::ee_uicpp_manager_music_instance_call(self.manager, 1, i) }
    }
    fn stop(&mut self, i: Instance) {
        unsafe { sys::ee_uicpp_manager_music_instance_call(self.manager, 2, i) }
    }
    fn unload(&mut self, id: c_long) {
        unsafe { sys::ee_uicpp_manager_music_unload(self.manager, id) }
    }
    fn load(&mut self, name: &[u8]) -> c_long {
        let n = c_name(name);
        unsafe { sys::ee_uicpp_manager_music_load(self.manager, n.as_ptr() as *const c_char) }
    }
    fn play(&mut self, id: c_long, volume: f32) -> Instance {
        unsafe { sys::ee_uicpp_manager_music_play(self.manager, id, volume) }
    }
    fn set_parameters(&mut self, i: Instance, volume: f32) {
        unsafe { sys::ee_uicpp_manager_music_set_parameters(self.manager, i, volume) }
    }
    fn lock(&mut self) {
        unsafe { sys::ee_uicpp_manager_lock(self.manager, 0) }
    }
    fn unlock(&mut self) {
        unsafe { sys::ee_uicpp_manager_lock(self.manager, 1) }
    }
}

impl ImeHost for Raw {
    fn lock(&mut self) {
        unsafe { sys::ee_uicpp_manager_lock(self.manager, 0) }
    }
    fn unlock(&mut self) {
        unsafe { sys::ee_uicpp_manager_lock(self.manager, 1) }
    }
    fn has_form(&self) -> bool {
        unsafe { sys::ee_uicpp_manager_ime_has_form(self.manager) }
    }
    fn form_call(&mut self, call: ImeCall) -> c_long {
        unsafe { sys::ee_uicpp_manager_ime_form_call(self.manager, call as c_int, self.args) }
    }
    fn captured_ime(&mut self, current: bool) -> bool {
        unsafe { sys::ee_uicpp_manager_ime_captured(self.manager, current) }
    }
    fn captured_rect(&self, current: bool) -> UiRect {
        unsafe { sys::ee_uicpp_manager_ime_captured_rect(self.manager, current) }
    }
    fn window(&mut self) -> Option<(f32, f32, c_ulong, c_ulong)> {
        let mut origin = [0.0f32; 2];
        let mut size: [c_ulong; 2] = [0; 2];
        unsafe { sys::ee_uicpp_manager_ime_window(self.manager, origin.as_mut_ptr(), size.as_mut_ptr()) }
            .then_some((origin[0], origin[1], size[0], size[1]))
    }
    fn candidate_open(&self) -> bool {
        unsafe { sys::ee_uicpp_manager_ime_flag(self.manager, 0) }
    }
    fn set_candidate_open(&mut self, v: bool) {
        unsafe { sys::ee_uicpp_manager_ime_set_flag(self.manager, 0, v) }
    }
    fn set_draw_candidate(&mut self, v: bool) {
        unsafe { sys::ee_uicpp_manager_ime_set_flag(self.manager, 1, v) }
    }
    fn candidate_set_list(&mut self, left: f32) {
        unsafe { sys::ee_uicpp_manager_ime_set_list(self.manager, self.args, left) }
    }
    fn candidate_open_window(&mut self, screen: UiRect, control: UiRect) {
        unsafe { sys::ee_uicpp_manager_ime_open_window(self.manager, &screen, &control) }
    }
    fn candidate_close(&mut self) {
        unsafe { sys::ee_uicpp_manager_ime_close_window(self.manager) }
    }
    fn candidate_copy(&mut self) -> c_long {
        unsafe { sys::ee_uicpp_manager_ime_copy(self.manager) }
    }
}

fn raw(manager: *mut c_void) -> Raw {
    Raw { manager, args: core::ptr::null() }
}

unsafe fn bytes<'a>(s: *const c_char) -> &'a [u8] {
    core::ffi::CStr::from_ptr(s).to_bytes()
}

/// `UIManager::SetMusicVolume(volume)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_set_music_volume(manager: *mut c_void, volume: f32) {
    manager_media::set_music_volume(&mut raw(manager), volume);
}

/// `UIManager::StartMusic(name)` (the name as a C string).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_start_music(manager: *mut c_void, name: *const c_char) {
    manager_media::start_music(&mut raw(manager), bytes(name));
}

/// `UIManager::StopMusic`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_stop_music(manager: *mut c_void) {
    manager_media::stop_music(&mut raw(manager));
}

/// `UIManager::SetSoundInstance(instance)`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_set_sound_instance(manager: *mut c_void, instance: *mut c_void) {
    manager_media::set_sound_instance(&mut raw(manager), instance);
}

/// `UIManager::RemoveSoundInstance`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_remove_sound_instance(manager: *mut c_void) {
    manager_media::remove_sound_instance(&mut raw(manager));
}

/// `UIManager::RemoveAndStopSoundInstance`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_remove_and_stop_sound_instance(manager: *mut c_void) {
    manager_media::remove_and_stop_sound_instance(&mut raw(manager));
}

/// `UIManager::UpdateUI`'s music step (inside the lock).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_update_music(manager: *mut c_void) {
    manager_media::update_music(&mut raw(manager));
}

/// `UIManager::AddIMEResultString` (call 0), `ChangeIMECompositionString` (1), `CloseIMECompositionString` (2),
/// `OpenIMECompositionString` (3): `args` are the call's arguments for the host -> the form's result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_ime_form_call(manager: *mut c_void, call: c_int, args: *const c_void) -> c_long {
    let call = match call {
        0 => ImeCall::AddResult,
        1 => ImeCall::ChangeComposition,
        2 => ImeCall::CloseComposition,
        _ => ImeCall::OpenComposition,
    };
    manager_media::ime_form_call(&mut Raw { manager, args }, call)
}

/// `UIManager::ChangeIMECandidateWindow(list)`: `list` is the C++ list (for `SetCandidateList`), with its style and
/// count.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_change_candidate_window(
    manager: *mut c_void,
    list: *const c_void,
    style: c_ulong,
    count: c_ulong,
) -> c_long {
    manager_media::change_candidate_window(&mut Raw { manager, args: list }, style, count)
}

/// `UIManager::OpenIMECandidateWindow`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_open_candidate_window(manager: *mut c_void) -> c_long {
    manager_media::open_candidate_window(&mut raw(manager))
}

/// `UIManager::CloseIMECandidateWindow`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_close_candidate_window(manager: *mut c_void) -> c_long {
    manager_media::close_candidate_window(&mut raw(manager))
}

/// `UIManager::CopyData`'s candidate window, after the forms (`result`: theirs) -> the result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_copy_candidate_window(manager: *mut c_void, result: c_long) -> c_long {
    manager_media::copy_candidate_window(&mut raw(manager), result)
}
