//! The view's host in the game (`game_view.rs`): the two C++ fills (`UI Events/EEScreens.cpp`), the sound manager's
//! and the conductor's Rust objects (`ee_sound`), the chat log's store, the toolkit's Rust controls (`ee_ui
//! direct.rs`) and the lobby's slot record.
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_ulong, c_void};

use ee_sound::conductorobj::{self, EeSoundConductor};
use ee_sound::managerobj::{self, EeSoundManager};
use ee_ui::direct;
use ee_ui::obj::EeUiOp;
use ee_ui::statusobj::EeUiStatusBarCall;

use crate::game_view::{EeScreensGameView, Sound, ViewHost};
use crate::lobby_form::{LobbyHost, EE_SCREENS_LE_SOURCE_LOBBY};
use crate::menus_ffi::Form;

extern "C" {
    /// The values of `group` (`EE_SCREENS_VIEW_*`) for `arg` into `out`.
    fn ee_screenscpp_game_view(group: u32, arg: u32, out: *mut EeScreensGameView);
    /// The texts of `group` (`EE_SCREENS_VIEW_TEXTS_*`) for `arg`, each ended by a 0 byte, into `out` (up to
    /// `capacity` bytes); their length.
    fn ee_screenscpp_game_texts(group: u32, arg: u32, out: *mut u8, capacity: usize) -> usize;
}

impl ViewHost for Form {
    fn view(&mut self, group: u32, arg: u32) -> EeScreensGameView {
        let mut v = EeScreensGameView::default();
        // SAFETY: the host's fill writes the record
        unsafe { ee_screenscpp_game_view(group, arg, &mut v) };
        v
    }
    fn texts(&mut self, group: u32, arg: u32) -> Vec<u8> {
        let mut out = vec![0u8; 1024];
        // SAFETY: the host writes at most `capacity` bytes and answers the whole length
        let n = unsafe { ee_screenscpp_game_texts(group, arg, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, 0);
            unsafe { ee_screenscpp_game_texts(group, arg, out.as_mut_ptr(), out.len()) };
        }
        out.truncate(n);
        out
    }
    fn conductor(&mut self, c: usize, op: Sound, value: u32) -> u32 {
        let c = c as *const EeSoundConductor;
        // SAFETY: `c` is the mixer's conductor (`SEConductor` is the Rust object), as its C++ forwarders cast it
        unsafe {
            match op {
                Sound::SetVolume => conductorobj::ee_sound_conductor_set_volume(c, f32::from_bits(value)) as u32,
                Sound::Volume => conductorobj::ee_sound_conductor_fields(c).volume.to_bits(),
                Sound::SetReverb => conductorobj::ee_sound_conductor_set_reverb(c, value) as u32,
                Sound::SetState => conductorobj::ee_sound_conductor_set_state(c, value) as u32,
                Sound::State => conductorobj::ee_sound_conductor_fields(c).state,
                _ => 0,
            }
        }
    }
    fn sound(&mut self, m: usize, op: Sound, value: u32) -> u32 {
        let m = m as *const EeSoundManager;
        // SAFETY: `m` is `gSoundManager` (`SESoundManager` is the Rust object), as its C++ forwarders cast it
        unsafe {
            match op {
                Sound::SetMasterVolume => {
                    managerobj::ee_sound_manager_set_master_volume(m, f32::from_bits(value)) as u32
                }
                Sound::MasterVolume => managerobj::ee_sound_manager_master_volume(m).to_bits(),
                Sound::MaxChannels => managerobj::ee_sound_manager_mixer_value(m, 1),
                Sound::SetMaxChannels => managerobj::ee_sound_manager_set_max_channels(m, value) as u32,
                Sound::Stop => managerobj::ee_sound_manager_stop(m, value, false) as u32,
                Sound::SetLatency => managerobj::ee_sound_manager_set_latency(m, value) as u32,
                _ => 0,
            }
        }
    }
    fn chat_log_count(&mut self) -> u32 {
        crate::chat_log_ffi::ee_screens_chat_log_count()
    }
    fn chat_log_text(&mut self, index: u32, which: u32) -> Vec<u8> {
        crate::chat_log_ffi::text(index, which)
    }
    fn form_exists(&mut self, id: u32) -> bool {
        !direct::form_by_id(direct::manager_of(core::ptr::null_mut()), id as c_ulong).is_null()
    }
    fn lobby_slot(&mut self, index: u32) -> (u32, bool, u32, Vec<u8>) {
        let p = self.player(EE_SCREENS_LE_SOURCE_LOBBY, index);
        (p.r.kind as u32, p.r.is_local, p.r.color as u32, p.name)
    }
    fn set_bg_colour(&mut self, control: u32, colour: [f32; 3]) {
        let c = direct::control(self.0, control as c_ulong);
        direct::control_virtual(c, EeUiOp::PartSetBgColor, 0, 0, &mut [colour[0], colour[1], colour[2], 0.0]);
    }
    fn set_bar_colour(&mut self, control: u32, colour: [f32; 3]) {
        let c: *mut c_void = direct::control(self.0, control as c_ulong);
        direct::status_call(c, EeUiStatusBarCall::SetStatusBarColor, 0, 0, &mut [colour[0], colour[1], colour[2], 0.0]);
    }
}
