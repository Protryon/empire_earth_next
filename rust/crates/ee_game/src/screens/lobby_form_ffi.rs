//! The lobby's host over the C++ calls (`lobby_form.rs`; `UI Events/EEScreens.cpp`), its state and the functions the
//! user interface thread calls (`EEUserInterface.cpp`'s posted lobby updates).
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_long;
use std::sync::Mutex;

use crate::screens::lobby_form::*;
use crate::screens::menus::EE_SCREENS_TEXT_STRING;
use crate::screens::menus_ffi::Form;
use crate::screens::scenario_screens_ffi::ee_screenscpp_form;

/// The lobby's state (the UI thread's; locked only around its own reads and writes).
static LOBBY: Mutex<LobbyForm> = Mutex::new(LobbyForm::new());

/// One of the lobby's events; false for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> bool {
    is_lobby_event(event) && crate::screens::lobby_form::dispatch(host, &LOBBY, event)
}

impl Form {
    /// The host on form `form` (the event's: [`LE_FORM_EVENT`]; the same entity).
    fn lobby_on(&self, form: u32) -> Form {
        if form == LE_FORM_EVENT {
            Form(self.0, self.1)
        } else {
            Form(ee_screenscpp_form(form), self.1)
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
        let (result, out) = crate::screens::screens_more_calls::selection(
            &mut crate::screens::game_cpp::Ffi,
            self.lobby_on(form).0 as usize,
            control,
            false,
        );
        (result == 0, out)
    }
    fn player(&mut self, source: u32, index: u32) -> LobbyPlayer {
        crate::screens::lobby_calls::player(&mut crate::screens::game_cpp::Ffi, source, index)
    }
    fn put_player(&mut self, source: u32, index: u32, player: &LobbyPlayer) {
        crate::screens::lobby_calls::put_player(&mut crate::screens::game_cpp::Ffi, source, index, player)
    }
    fn options(&mut self, source: u32) -> EeScreensLobbyOptionsRecord {
        crate::screens::lobby_calls::options(&mut crate::screens::game_cpp::Ffi, source)
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

/// cbindgen:ignore
mod options_sys {
    extern "C" {
        /// the lobby's options (`gGameOptions`: `EEGameOptions`' Rust base)
        pub fn ee_screenscpp_lobby_options() -> *mut crate::app::game_options::EeAppOptionsFields;
    }
}

/// An option of the lobby's options (`EEMGameOptions`' numbers, `EE_SCREENS_OPTION_*`; 0 the load type, 17 the
/// victory flag) set to `value` (the C++ `long`'s: truncated to `c_long`; a flag is set when non-zero).
#[no_mangle]
pub extern "C" fn ee_screens_lobby_set_option(option: u32, value: i64) {
    use crate::screens::lobby_options::*;
    use core::ffi::c_ulong;
    // SAFETY: the game's live lobby options (the UI thread's)
    let o = unsafe { &mut *options_sys::ee_screenscpp_lobby_options() };
    let v = value as c_long;
    let on = v != 0;
    match option as c_long {
        0 => o.mLoadType = v as c_ulong,
        EE_SCREENS_OPTION_MAP_SIZE => o.mMapSize = v as c_ulong,
        EE_SCREENS_OPTION_RESOURCES => o.mResources = v as c_ulong,
        EE_SCREENS_OPTION_EPOCH_START => o.mEpochStart = v,
        EE_SCREENS_OPTION_EPOCH_END => o.mEpochEnd = v,
        EE_SCREENS_OPTION_UNIT_LIMIT => o.mUnitLimit = v as c_ulong,
        EE_SCREENS_OPTION_WONDERS => o.mWondersForVictory = v as c_ulong,
        EE_SCREENS_OPTION_VARIANT => o.mVariant = v as c_ulong,
        EE_SCREENS_OPTION_DIFFICULTY => o.mDifficulty = v as c_ulong,
        EE_SCREENS_OPTION_SPEED => o.mGameSpeed = v as c_ulong,
        EE_SCREENS_OPTION_RECORD_GAMES => o.mRecordGames = on,
        EE_SCREENS_OPTION_CUSTOM_CIVS => o.mAllowCustomCivs = on,
        EE_SCREENS_OPTION_REVEAL_MAP => o.mRevealMap = on,
        EE_SCREENS_OPTION_LOCK_TEAMS => o.mLockTeams = on,
        EE_SCREENS_OPTION_LOCK_SPEED => o.mLockSpeed = on,
        EE_SCREENS_OPTION_CHEAT_CODES => o.mCheatCodes = on,
        17 => o.mVictory = on,
        _ => {}
    }
}
