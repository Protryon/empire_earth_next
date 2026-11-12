//! `extern "C"` for the lobby's game options (`lobby_options.rs`; `UI Events/EEUIELobbyEvents.cpp`): plain values
//! in and out, no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::lobby_options::{
    self, EeScreensLobbyOptionChange, EeScreensLobbyOptionState, EeScreensLobbyOptionsPlan, EeScreensLobbyOptionsView,
    EeScreensLobbyStart, EeScreensLobbyStartSlot,
};

unsafe fn flags<'a>(selectable: *const bool, count: usize) -> &'a [bool] {
    if selectable.is_null() || count == 0 {
        &[]
    } else {
        core::slice::from_raw_parts(selectable, count)
    }
}

/// `LobbyShowGameOptions`' plan for the view.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_show_options(
    view: *const EeScreensLobbyOptionsView,
    out: *mut EeScreensLobbyOptionsPlan,
) {
    out.write(lobby_options::show_options(&*view));
}

/// The variant box's line of `variant` over the variants' selectable flags (`count` of them).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_variant_line(
    selectable: *const bool,
    count: usize,
    variant: c_ulong,
) -> c_ulong {
    lobby_options::variant_line(flags(selectable, count), variant)
}

/// An option event's change (`option` the `EEMGameOptions` option, `selection` the box's line or a check box's 0/1;
/// `selectable`/`count` the variants' flags, the variant event's).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_option_change(
    option: c_long,
    selection: c_ulong,
    state: *const EeScreensLobbyOptionState,
    selectable: *const bool,
    count: usize,
    out: *mut EeScreensLobbyOptionChange,
) {
    out.write(lobby_options::option_change(option, selection, &*state, flags(selectable, count)));
}

/// `LobbyCheckStart` after the load (`load_ok`: a loaded game's file loaded and matched); `slots` 16 of them.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_start_check(
    load_type: c_ulong,
    load_ok: bool,
    multiplayer: bool,
    slots: *const EeScreensLobbyStartSlot,
    out: *mut EeScreensLobbyStart,
) {
    out.write(lobby_options::start_check(
        load_type,
        load_ok,
        multiplayer,
        &*slots.cast::<[EeScreensLobbyStartSlot; 16]>(),
    ));
}
