//! The game lobby's player rows (form 0x1a, `UI Events/EEUIELobbyEvents.cpp`; the random map setup of a single-player
//! game and the multiplayer staging screen): which of a slot's controls show as an editable control, as a label or
//! not at all, which player-type menu its type box holds and which line is selected, what its name label says and
//! in which colour.
//!
//! The C++ keeps the controls, the strings and the game options (read under `gGameOptions.mLock`); it hands a slot
//! and the lobby's context here as plain values and makes the calls the plans name, in its own order.

use core::ffi::{c_long, c_ulong};

/// A display mode as `UIControl::SetDisplayMode` takes it: 0 hidden, 1 shown.
pub const EE_SCREENS_HIDDEN: c_ulong = 0;

/// A slot of the lobby as the rows read it (`EEGameOptionsPlayer`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbySlot {
    /// `mType`: 0 computer, 1 human, 2 observer, 3 open, 4 closed.
    pub kind: c_ulong,
    /// `mLoadedType`: a loaded game's slot's type (1 a human's).
    pub loaded_type: c_long,
    /// `mSlot`: the slot's index (a row shows only under 8).
    pub slot: c_ulong,
    pub is_local: bool,
    pub is_ready: bool,
    /// `mB36`: the slot was a player's in the scenario.
    pub b36: bool,
}

/// The lobby's state the rows depend on.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyContext {
    /// `gCommunications->IsServer()`: this machine hosts.
    pub server: bool,
    /// `gGameOptions.mIsMultiplayer`.
    pub multiplayer: bool,
    /// `gGameOptions.mLoadType`: 0 a new random map, 1 a scenario, 2 a saved game.
    pub load_type: c_ulong,
    /// The host's slot (0) is ready.
    pub host_ready: bool,
}

/// What a row's name label says (`LobbyShowHostRow`, `LobbyShowClientRow`).
pub const EE_SCREENS_LOBBY_TEXT_COMPUTER: u32 = 0;
pub const EE_SCREENS_LOBBY_TEXT_NAME: u32 = 1;
pub const EE_SCREENS_LOBBY_TEXT_OPEN: u32 = 2;
pub const EE_SCREENS_LOBBY_TEXT_CLOSED: u32 = 3;

/// A player-type menu's lines (`LobbyFillPlayerTypes`).
pub const EE_SCREENS_LOBBY_LINE_NAME: u8 = 0;
pub const EE_SCREENS_LOBBY_LINE_OPEN: u8 = 1;
pub const EE_SCREENS_LOBBY_LINE_COMPUTER: u8 = 2;
pub const EE_SCREENS_LOBBY_LINE_CLOSED: u8 = 3;
pub const EE_SCREENS_LOBBY_LINE_OBSERVE: u8 = 4;

/// The name colours (`LobbyShowName`).
pub const EE_SCREENS_LOBBY_RED: u32 = 0;
pub const EE_SCREENS_LOBBY_YELLOW: u32 = 1;
pub const EE_SCREENS_LOBBY_WHITE: u32 = 2;
pub const EE_SCREENS_LOBBY_GREEN: u32 = 3;

/// `PlayerTypeFromMenu`: the type of line `selection` of menu `menu`; 5 (none) for a line or menu past them.
pub fn player_type_from_menu(selection: c_ulong, menu: c_long) -> c_long {
    match (menu, selection) {
        (5, 2) => 0,
        (5, 1) => 3,
        (5, 0) => 1,
        (4, 1) => 0,
        (4, 0) => 3,
        (3, 1) => 2,
        (3, 0) => 1,
        (2, 3) => 4,
        (2, 2) => 0,
        (2, 1) => 3,
        (2, 0) => 1,
        (1, 2) => 4,
        (1, 1) => 0,
        (1, 0) => 3,
        _ => 5,
    }
}

/// `LobbyFillPlayerTypes`: a menu's lines (`EE_SCREENS_LOBBY_LINE_*`); none for a menu past 1..5.
pub fn player_type_lines(menu: c_long) -> &'static [u8] {
    const NAME: u8 = EE_SCREENS_LOBBY_LINE_NAME;
    const OPEN: u8 = EE_SCREENS_LOBBY_LINE_OPEN;
    const COMPUTER: u8 = EE_SCREENS_LOBBY_LINE_COMPUTER;
    const CLOSED: u8 = EE_SCREENS_LOBBY_LINE_CLOSED;
    const OBSERVE: u8 = EE_SCREENS_LOBBY_LINE_OBSERVE;
    match menu {
        5 => &[NAME, OPEN, COMPUTER],
        4 => &[OPEN, COMPUTER],
        3 => &[NAME, OBSERVE],
        2 => &[NAME, OPEN, COMPUTER, CLOSED],
        1 => &[OPEN, COMPUTER, CLOSED],
        _ => &[],
    }
}

/// `LobbySelectPlayerType`: the line of menu `menu` a slot of type `kind` selects; none for a menu past 1..5 (the
/// selection stays).
pub fn player_type_line(menu: c_long, kind: c_ulong) -> Option<c_ulong> {
    Some(match menu {
        5 => match kind {
            3 => 1,
            0 => 2,
            _ => 0,
        },
        4 => (kind != 3) as c_ulong,
        3 => (kind != 1) as c_ulong,
        2 => match kind {
            1 | 2 => 0,
            3 => 1,
            0 => 2,
            _ => 3,
        },
        1 => match kind {
            3 => 0,
            0 => 1,
            _ => 2,
        },
        _ => return None,
    })
}

/// A row's choice: its type box's menu, its name label's text (`EE_SCREENS_LOBBY_TEXT_*`) and the arguments of
/// `LobbyShowRowControls` (ready, team, colour, citizens: 0 the label, 1 the editable control, 2 neither).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyRow {
    pub menu: c_long,
    pub text: u32,
    pub controls: [c_ulong; 4],
}

fn row_text(p: &EeScreensLobbySlot, c: &EeScreensLobbyContext) -> u32 {
    if p.kind == 0 || p.kind == 3 && c.load_type == 1 && c.host_ready {
        EE_SCREENS_LOBBY_TEXT_COMPUTER
    } else if p.kind == 1 || p.kind == 2 || p.kind == 3 && c.load_type == 2 {
        EE_SCREENS_LOBBY_TEXT_NAME
    } else if p.kind == 3 {
        EE_SCREENS_LOBBY_TEXT_OPEN
    } else {
        EE_SCREENS_LOBBY_TEXT_CLOSED
    }
}

/// `LobbyShowHostRow`: a slot's row as the host sees it.
pub fn host_row(p: &EeScreensLobbySlot, c: &EeScreensLobbyContext) -> EeScreensLobbyRow {
    let player = p.kind == 1 || p.kind == 2;
    let menu = if c.load_type == 0 {
        if p.is_local {
            3
        } else if player {
            2
        } else {
            1
        }
    } else if c.load_type == 1 && !p.b36 {
        if player {
            5
        } else {
            4
        }
    } else {
        1
    };
    let scenario = c.load_type == 1;
    let controls = match p.kind {
        0 if !scenario => [2, 1, 2, 1],
        1 if !scenario && p.is_local => [1, 1, 1, 1],
        1 if !scenario => [0, 0, 0, 0],
        2 if !scenario && p.is_local => [1, 2, 2, 2],
        2 if !scenario => [0, 2, 2, 2],
        0..=2 => [2, 0, 2, 0],
        _ => [2, 2, 2, 2],
    };
    EeScreensLobbyRow { menu, text: row_text(p, c), controls }
}

/// `LobbyShowClientRow`: a slot's row as a joined player sees it.
pub fn client_row(p: &EeScreensLobbySlot, c: &EeScreensLobbyContext) -> EeScreensLobbyRow {
    let menu = if c.load_type == 0 && p.is_local { 3 } else { 1 };
    let controls = match p.kind {
        0 => [2, 0, 2, 0],
        1 if p.is_local && c.multiplayer => [1, 1, 1, 0],
        1 if p.is_local => [1, 1, 1, 1],
        1 => [0, 0, 0, 0],
        2 if p.is_local => [1, 2, 2, 2],
        2 => [0, 2, 2, 2],
        _ => [2, 2, 2, 2],
    };
    EeScreensLobbyRow { menu, text: row_text(p, c), controls }
}

/// `LobbyShowRowControls`: one pair's display modes, (label, editable control), from its argument (0 the label,
/// 1 the control, 2 or more neither).
pub fn row_pair(argument: c_ulong) -> [c_ulong; 2] {
    if argument < 2 {
        [(argument == 0) as c_ulong, (argument == 1) as c_ulong]
    } else {
        [EE_SCREENS_HIDDEN; 2]
    }
}

/// `LobbyShowName`'s plan: the type box's and the name label's display modes, the name's colour
/// (`EE_SCREENS_LOBBY_*` colours) and whether the type box is marked dirty (the label always is).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyName {
    pub box_mode: c_ulong,
    pub label_mode: c_ulong,
    pub colour: u32,
    pub box_dirty: bool,
}

pub fn name_plan(p: &EeScreensLobbySlot, c: &EeScreensLobbyContext) -> EeScreensLobbyName {
    let shown = (p.slot < 8) as c_ulong;
    let editable = c.load_type != 2
        && (c.server || p.is_local)
        && (!p.is_ready && p.is_local && c.multiplayer || c.server && !c.host_ready && !p.is_local)
        && (c.load_type != 1 || !p.b36 && !p.is_local && p.kind != 4);
    let (box_mode, label_mode) = if editable { (shown, EE_SCREENS_HIDDEN) } else { (EE_SCREENS_HIDDEN, shown) };
    let loaded_open = p.kind == 3 && c.load_type == 2;
    let colour = if loaded_open {
        if p.loaded_type == 1 {
            EE_SCREENS_LOBBY_RED
        } else {
            EE_SCREENS_LOBBY_YELLOW
        }
    } else if p.kind != 2 {
        EE_SCREENS_LOBBY_WHITE
    } else {
        EE_SCREENS_LOBBY_GREEN
    };
    EeScreensLobbyName { box_mode, label_mode, colour, box_dirty: !loaded_open }
}

/// `LobbyShowPlayer`'s display modes for each of a row's parts, as (editable control, label) pairs: the ready check
/// box and its bitmap, the colour button and label, the team box and label, the citizens box and label, the engine
/// box and label (`LobbyShowEngine`), then the ping meter.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyPlayer {
    pub ready: [c_ulong; 2],
    pub colour: [c_ulong; 2],
    pub team: [c_ulong; 2],
    pub citizens: [c_ulong; 2],
    pub engine: [c_ulong; 2],
    pub ping: c_ulong,
}

pub fn player_plan(p: &EeScreensLobbySlot, c: &EeScreensLobbyContext) -> EeScreensLobbyPlayer {
    let shown = (p.slot < 8) as c_ulong;
    let h = EE_SCREENS_HIDDEN;
    let player = p.kind == 1 || p.kind == 2;
    let ready = if p.is_local {
        if c.multiplayer {
            [shown, h]
        } else {
            [h, h]
        }
    } else if player {
        [h, shown]
    } else {
        [h, h]
    };
    let colour = if p.kind == 1 {
        if p.is_local && !p.is_ready && c.load_type == 0 {
            [shown, h]
        } else {
            [h, shown]
        }
    } else {
        [h, h]
    };
    // a team and citizens: only on a human's or a computer's row, or an open one a scenario's ready host fills
    let rowless = p.kind != 1 && p.kind != 0 && !(p.kind == 3 && c.load_type == 1 && c.host_ready);
    let team = if rowless {
        [h, h]
    } else if c.load_type == 0 && !p.is_ready && (p.is_local || c.server && p.kind == 0) {
        [shown, h]
    } else {
        [h, shown]
    };
    let host_editing = c.server && !c.host_ready && c.load_type == 0;
    let citizens = if rowless {
        [h, h]
    } else if host_editing {
        [shown, h]
    } else {
        [h, shown]
    };
    let engine = if p.kind != 0 {
        [h, h]
    } else if host_editing {
        [shown, h]
    } else {
        [h, shown]
    };
    let ping = if player && p.slot != 0 { shown } else { h };
    EeScreensLobbyPlayer { ready, colour, team, citizens, engine, ping }
}

/// `LobbyShowEngine`'s choice: the slot's engine, or Classic (0) past the choices.
pub fn engine_choice(engine: c_ulong, count: c_ulong) -> c_ulong {
    if engine < count {
        engine
    } else {
        0
    }
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests;
