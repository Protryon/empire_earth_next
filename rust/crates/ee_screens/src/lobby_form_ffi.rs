//! The lobby's host over the C++ calls (`lobby_form.rs`; `UI Events/EEScreens.cpp`), its state and the functions the
//! user interface thread calls (`EEUserInterface.cpp`'s posted lobby updates).
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_long;
use std::sync::Mutex;

use crate::lobby_form::*;
use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::{sys, Form};
use crate::scenario_screens_ffi::ee_screenscpp_form;

extern "C" {
    /// A slot of `source` (`EE_SCREENS_LE_SOURCE_*`) into `out`, its name into `name` (up to `capacity` bytes); the
    /// name's length.
    fn ee_screenscpp_lobby_player(
        source: u32,
        index: u32,
        out: *mut EeScreensLobbyPlayerRecord,
        name: *mut u8,
        capacity: usize,
    ) -> usize;
    /// A slot of `source` from a record and a name.
    fn ee_screenscpp_put_lobby_player(
        source: u32,
        index: u32,
        player: *const EeScreensLobbyPlayerRecord,
        name: *const u8,
        length: usize,
    );
    /// The options of `source` into `out`.
    fn ee_screenscpp_lobby_options(source: u32, out: *mut EeScreensLobbyOptionsRecord);
}

/// The lobby's state (the UI thread's; locked only around its own reads and writes).
static LOBBY: Mutex<LobbyForm> = Mutex::new(LobbyForm::new());

/// One of the lobby's events; false for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> bool {
    is_lobby_event(event) && crate::lobby_form::dispatch(host, &LOBBY, event)
}

impl Form {
    /// The host on form `form` (the event's: [`LE_FORM_EVENT`]; the same entity).
    fn lobby_on(&self, form: u32) -> Form {
        if form == LE_FORM_EVENT {
            Form(self.0, self.1)
        } else {
            Form(unsafe { ee_screenscpp_form(form) }, self.1)
        }
    }
}

impl LobbyHost for Form {
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.lobby_on(form).op(op, a, b, c, d)
    }
    fn text(&mut self, form: u32, which: u32, arg: u32) -> Vec<u8> {
        self.lobby_on(form).get(which, arg)
    }
    fn text_checked(&mut self, form: u32, which: u32, arg: u32) -> Option<Vec<u8>> {
        self.lobby_on(form).get_checked(which, arg)
    }
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.lobby_on(form).put(which, arg, text)
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn selection(&mut self, form: u32, control: u32) -> (bool, u32) {
        let mut out = 0u32;
        let result = unsafe { sys::ee_screenscpp_selection(self.lobby_on(form).0, control, false, &mut out) };
        (result == 0, out)
    }
    fn player(&mut self, source: u32, index: u32) -> LobbyPlayer {
        let mut r = EeScreensLobbyPlayerRecord::default();
        let mut name = vec![0u8; 256];
        let n = unsafe { ee_screenscpp_lobby_player(source, index, &mut r, name.as_mut_ptr(), name.len()) };
        if n > name.len() {
            name.resize(n, 0);
            unsafe { ee_screenscpp_lobby_player(source, index, &mut r, name.as_mut_ptr(), name.len()) };
        }
        name.truncate(n);
        LobbyPlayer { r, name }
    }
    fn put_player(&mut self, source: u32, index: u32, player: &LobbyPlayer) {
        unsafe { ee_screenscpp_put_lobby_player(source, index, &player.r, player.name.as_ptr(), player.name.len()) };
    }
    fn options(&mut self, source: u32) -> EeScreensLobbyOptionsRecord {
        let mut o = EeScreensLobbyOptionsRecord::default();
        unsafe { ee_screenscpp_lobby_options(source, &mut o) };
        o
    }
}

/// `LobbyUpdatePlayer`: the given parts (`LobbyShowPlayer`'s flags) of one slot's row.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_update_player(slot: c_long, flags: c_long) {
    update_player(&mut Form(core::ptr::null_mut(), core::ptr::null_mut()), &LOBBY, slot, flags);
}

/// `LobbyUpdateGameOptions`: the lobby's game options redrawn.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_update_game_options() {
    update_game_options(&mut Form(core::ptr::null_mut(), core::ptr::null_mut()));
}

/// `LobbyUpdateForm`: the lobby's end epoch, its events and its controls enabled.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_update_form() {
    update_form(&mut Form(core::ptr::null_mut(), core::ptr::null_mut()), &LOBBY);
}

/// `LobbyShowTransferProgress`: a client's file transfer on its row.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_show_transfer_progress(slot: u8, progress: u8) {
    show_transfer_progress(&mut Form(core::ptr::null_mut(), core::ptr::null_mut()), slot, progress);
}
