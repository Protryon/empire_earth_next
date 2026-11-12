//! `extern "C"` for the lobby's player rows (`lobby.rs`; `UI Events/EEUIELobbyEvents.cpp`): plain values in and out,
//! no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong};

use crate::lobby::{
    self, EeScreensLobbyContext, EeScreensLobbyName, EeScreensLobbyPlayer, EeScreensLobbyRow, EeScreensLobbySlot,
};

/// `PlayerTypeFromMenu`.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_type_from_menu(selection: c_ulong, menu: c_long) -> c_long {
    lobby::player_type_from_menu(selection, menu)
}

/// `LobbyFillPlayerTypes`: menu `menu`'s lines (`EE_SCREENS_LOBBY_LINE_*`) into `out` (room for 4) -> their count.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_type_lines(menu: c_long, out: *mut u8) -> c_ulong {
    let lines = lobby::player_type_lines(menu);
    core::ptr::copy_nonoverlapping(lines.as_ptr(), out, lines.len());
    lines.len() as c_ulong
}

/// `LobbySelectPlayerType`: whether menu `menu` selects a line for type `kind` (`out` then the line).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_type_line(menu: c_long, kind: c_ulong, out: *mut c_ulong) -> bool {
    match lobby::player_type_line(menu, kind) {
        Some(l) => {
            out.write(l);
            true
        }
        None => false,
    }
}

/// `LobbyShowHostRow` (`host` true) or `LobbyShowClientRow`'s choice.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_row(
    host: bool,
    slot: *const EeScreensLobbySlot,
    context: *const EeScreensLobbyContext,
    out: *mut EeScreensLobbyRow,
) {
    let (p, c) = (&*slot, &*context);
    out.write(if host { lobby::host_row(p, c) } else { lobby::client_row(p, c) });
}

/// `LobbyShowRowControls`: one pair's (label, control) display modes into `out` (2).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_row_pair(argument: c_ulong, out: *mut c_ulong) {
    let pair = lobby::row_pair(argument);
    out.write(pair[0]);
    out.add(1).write(pair[1]);
}

/// `LobbyShowName`'s plan.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_name(
    slot: *const EeScreensLobbySlot,
    context: *const EeScreensLobbyContext,
    out: *mut EeScreensLobbyName,
) {
    out.write(lobby::name_plan(&*slot, &*context));
}

/// `LobbyShowPlayer`'s display modes.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_lobby_player(
    slot: *const EeScreensLobbySlot,
    context: *const EeScreensLobbyContext,
    out: *mut EeScreensLobbyPlayer,
) {
    out.write(lobby::player_plan(&*slot, &*context));
}

/// `LobbyShowEngine`: the engine a row shows.
#[no_mangle]
pub extern "C" fn ee_screens_lobby_engine(engine: c_ulong, count: c_ulong) -> c_ulong {
    lobby::engine_choice(engine, count)
}
