//! The lobby's game options (form 0x1a, `UI Events/EEUIELobbyEvents.cpp`): what `LobbyShowGameOptions` shows (an
//! editable box or a label per option, the lines the boxes select, which check boxes to set), what an option event
//! sends (`EEUIELEGame*`/`EEUIELEEpochEnd`: the `EEMGameOptions` messages and the values they store), and the start
//! check (`LobbyCheckStart`: the messages it shows and whether the game may start).
//!
//! The C++ keeps `gGameOptions` and its lock, the controls, the strings, the rules' lists and the messages: it hands
//! the values over, makes the calls these plans name in its own order, and stores the values the messages carry.

use core::ffi::{c_long, c_ulong};

/// A row of the game options with an editable box (the map size, the map type): hidden on a loaded game.
pub const EE_SCREENS_LOBBY_ROW_HIDDEN: u32 = 0;
/// The box shown, its label hidden (the host before ready, a random map).
pub const EE_SCREENS_LOBBY_ROW_EDIT: u32 = 1;
/// The label shown, the box hidden.
pub const EE_SCREENS_LOBBY_ROW_LABEL: u32 = 2;
/// The cheat codes row of a multiplayer game: left as it is.
pub const EE_SCREENS_LOBBY_ROW_KEEP: u32 = 3;

/// The game type box's string index (`sGameTypeStrings`: 0 random map, 1 saved game, 2 scenario); none past them.
pub const EE_SCREENS_LOBBY_NO_STRING: c_long = -1;

/// The option check boxes in `LobbyShowGameOptions`' order: record games (214), custom civilizations (216), reveal
/// map (215), lock teams (217), lock speed (218), cheat codes (219).
pub const EE_SCREENS_LOBBY_FLAGS: usize = 6;

/// What `LobbyShowGameOptions` reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyOptionsView {
    /// The shown options' load type, and the lobby's own (`gGameOptions`: the map type box follows it).
    pub load_type: c_long,
    pub lobby_load_type: c_long,
    pub server: bool,
    pub host_ready: bool,
    /// The lobby's multiplayer flag (the game type box) and the shown options' (the cheat codes row).
    pub lobby_multiplayer: bool,
    pub options_multiplayer: bool,
    /// The game type box's selection, when it has one.
    pub type_selected: bool,
    pub type_selection: c_ulong,
    /// The map generator's line (`EERMFindGenerator` found it), else the first.
    pub map_found: bool,
    pub map_line: c_ulong,
    /// Whether the start epoch box has a selection.
    pub epoch_selected: bool,
    /// The unit limit and the rules' minimum and step (gameoptions.yaml unit_limit).
    pub unit_limit: c_long,
    pub unit_minimum: c_ulong,
    pub unit_step: c_long,
    pub wonders: c_ulong,
    /// The option flags and their check boxes' states, in [`EE_SCREENS_LOBBY_FLAGS`]' order.
    pub flags: [bool; EE_SCREENS_LOBBY_FLAGS],
    pub checked: [bool; EE_SCREENS_LOBBY_FLAGS],
}

/// `LobbyShowGameOptions`' plan.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyOptionsPlan {
    /// Whether the server sets the game type box, and to which line (0 random map, 1 saved game, 2 scenario).
    pub set_type_line: bool,
    pub type_line: c_ulong,
    /// The game type label's string ([`EE_SCREENS_LOBBY_NO_STRING`]: the label shows nothing).
    pub type_string: c_long,
    /// The game type box shown (else its label).
    pub type_editable: bool,
    /// The map size's and the map type's rows (`EE_SCREENS_LOBBY_ROW_*`).
    pub map_rows: u32,
    /// Whether the map type box is set, and to which line.
    pub set_map_line: bool,
    pub map_line: c_ulong,
    /// The scenario's information shown (a loaded game).
    pub scenario_info: bool,
    /// The other options' boxes shown (else their labels): the host before ready, a random map.
    pub editable: bool,
    /// Whether the start epoch box is set (it has no selection yet).
    pub set_epoch_start: bool,
    /// The unit limit box's line.
    pub unit_line: c_ulong,
    /// The wonders label shows the number (else "Off").
    pub wonders_number: bool,
    /// The check boxes to set to their flag (they differ), in [`EE_SCREENS_LOBBY_FLAGS`]' order.
    pub set_checked: [bool; EE_SCREENS_LOBBY_FLAGS],
    /// The cheat codes row (`EE_SCREENS_LOBBY_ROW_EDIT`, `_LABEL` or `_KEEP`).
    pub cheats_row: u32,
}

/// The game type box's line for a load type: a scenario (1) the third, a saved game (2) the second, else the first.
pub fn game_type_line(load_type: c_long) -> c_ulong {
    match load_type {
        1 => 2,
        2 => 1,
        _ => 0,
    }
}

/// The game type label's string index: a saved game (2) 1, a scenario (1) 2, a random map (0) 0; none past them.
pub fn game_type_string(load_type: c_long) -> c_long {
    match load_type {
        0 => 0,
        1 => 2,
        2 => 1,
        _ => EE_SCREENS_LOBBY_NO_STRING,
    }
}

/// The unit limit box's line: (limit - minimum) / step, the C++'s `unsigned long` arithmetic (a limit below the
/// minimum wraps); a zero step (the rules refuse one) gives the first line.
pub fn unit_limit_line(limit: c_long, minimum: c_ulong, step: c_long) -> c_ulong {
    (limit as c_ulong).wrapping_sub(minimum).checked_div(step as c_ulong).unwrap_or(0)
}

pub fn show_options(v: &EeScreensLobbyOptionsView) -> EeScreensLobbyOptionsPlan {
    let line = game_type_line(v.load_type);
    let random = v.load_type == 0;
    let editable = random && v.server && !v.host_ready;
    let set_checked: [bool; EE_SCREENS_LOBBY_FLAGS] = core::array::from_fn(|k| v.flags[k] != v.checked[k]);
    EeScreensLobbyOptionsPlan {
        set_type_line: v.server && (!v.type_selected || v.type_selection != line),
        type_line: line,
        type_string: game_type_string(v.load_type),
        type_editable: v.server && !v.host_ready && v.lobby_multiplayer,
        map_rows: if !random {
            EE_SCREENS_LOBBY_ROW_HIDDEN
        } else if v.server && !v.host_ready {
            EE_SCREENS_LOBBY_ROW_EDIT
        } else {
            EE_SCREENS_LOBBY_ROW_LABEL
        },
        set_map_line: v.lobby_load_type == 0,
        map_line: if v.map_found { v.map_line } else { 0 },
        scenario_info: !random,
        editable,
        set_epoch_start: !v.epoch_selected,
        unit_line: unit_limit_line(v.unit_limit, v.unit_minimum, v.unit_step),
        wonders_number: v.wonders > 0,
        set_checked,
        cheats_row: if v.options_multiplayer {
            EE_SCREENS_LOBBY_ROW_KEEP
        } else if editable {
            EE_SCREENS_LOBBY_ROW_EDIT
        } else {
            EE_SCREENS_LOBBY_ROW_LABEL
        },
    }
}

/// The variant box's line of a variant: the selectable variants before it, when it is selectable itself (else, and
/// past the list, the first line).
pub fn variant_line(selectable: &[bool], variant: c_ulong) -> c_ulong {
    match usize::try_from(variant).ok().filter(|&v| v < selectable.len()) {
        Some(v) if selectable[v] => selectable[..v].iter().filter(|&&s| s).count() as c_ulong,
        _ => 0,
    }
}

/// The variant a line of the variant box holds: the line-th selectable variant (none past them: the original
/// walked on past the list).
pub fn variant_of_line(selectable: &[bool], line: c_ulong) -> Option<c_ulong> {
    let mut count: c_ulong = 0;
    for (v, &s) in selectable.iter().enumerate() {
        if !s {
            continue;
        }
        if count == line {
            return Some(v as c_ulong);
        }
        count += 1;
    }
    None
}

/// The `EEMGameOptions` options (the message's first argument; the option events by it).
pub const EE_SCREENS_OPTION_MAP_SIZE: c_long = 1;
pub const EE_SCREENS_OPTION_RESOURCES: c_long = 3;
pub const EE_SCREENS_OPTION_EPOCH_START: c_long = 4;
pub const EE_SCREENS_OPTION_EPOCH_END: c_long = 5;
pub const EE_SCREENS_OPTION_UNIT_LIMIT: c_long = 6;
pub const EE_SCREENS_OPTION_WONDERS: c_long = 7;
pub const EE_SCREENS_OPTION_VARIANT: c_long = 8;
pub const EE_SCREENS_OPTION_DIFFICULTY: c_long = 9;
pub const EE_SCREENS_OPTION_SPEED: c_long = 10;
pub const EE_SCREENS_OPTION_RECORD_GAMES: c_long = 11;
pub const EE_SCREENS_OPTION_CUSTOM_CIVS: c_long = 12;
pub const EE_SCREENS_OPTION_REVEAL_MAP: c_long = 13;
pub const EE_SCREENS_OPTION_LOCK_TEAMS: c_long = 14;
pub const EE_SCREENS_OPTION_LOCK_SPEED: c_long = 15;
pub const EE_SCREENS_OPTION_CHEAT_CODES: c_long = 16;
/// One past the options the table holds.
pub const EE_SCREENS_OPTION_COUNT: usize = 17;

/// The lobby's options as the option events compare them: each option's current value by its number (a flag 0 or
/// 1, an `unsigned long` option its bits), and what the unit limit and epoch events need.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyOptionState {
    pub current: [c_long; EE_SCREENS_OPTION_COUNT],
    pub epoch_count: c_long,
    pub unit_minimum: c_ulong,
    pub unit_step: c_long,
}

/// A game options message: the option and its value (the value the lobby then stores).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyMessage {
    pub option: c_long,
    pub value: c_long,
}

/// What an option event does: the messages to send in order (each one then stored), whether the end epoch box is
/// trimmed first (`LobbyUpdateEpochEnd`: the start epoch's) and whether the global difficulty levels take the
/// first message's value before it is sent (the difficulty's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyOptionChange {
    pub count: u32,
    pub messages: [EeScreensLobbyMessage; 2],
    pub update_epoch_end: bool,
    pub set_difficulty: bool,
}

impl EeScreensLobbyOptionChange {
    fn send(&mut self, option: c_long, value: c_long) {
        self.messages[self.count as usize] = EeScreensLobbyMessage { option, value };
        self.count += 1;
    }
}

/// An option event's change for the box's selection (a check box: 0 or 1). `selectable` is the variants' flags
/// (the variant event's).
pub fn option_change(
    option: c_long,
    selection: c_ulong,
    s: &EeScreensLobbyOptionState,
    selectable: &[bool],
) -> EeScreensLobbyOptionChange {
    let mut c = EeScreensLobbyOptionChange::default();
    let Some(current) = usize::try_from(option).ok().and_then(|o| s.current.get(o)).copied() else {
        return c;
    };
    match option {
        EE_SCREENS_OPTION_MAP_SIZE
        | EE_SCREENS_OPTION_RESOURCES
        | EE_SCREENS_OPTION_WONDERS
        | EE_SCREENS_OPTION_DIFFICULTY
        | EE_SCREENS_OPTION_SPEED => {
            if selection != current as c_ulong {
                c.set_difficulty = option == EE_SCREENS_OPTION_DIFFICULTY;
                c.send(option, selection as c_long);
            }
        }
        EE_SCREENS_OPTION_UNIT_LIMIT => {
            let step = s.unit_step as c_ulong;
            if selection != (current as c_ulong).wrapping_sub(s.unit_minimum).checked_div(step).unwrap_or(0) {
                c.send(option, selection.wrapping_mul(step).wrapping_add(s.unit_minimum) as c_long);
            }
        }
        EE_SCREENS_OPTION_EPOCH_END => {
            let start = s.current[EE_SCREENS_OPTION_EPOCH_START as usize];
            let mut epoch = selection;
            if start != s.epoch_count {
                epoch = epoch.wrapping_add(start as c_ulong);
            }
            if epoch != current as c_ulong {
                c.send(option, epoch as c_long);
            }
        }
        EE_SCREENS_OPTION_EPOCH_START => {
            if selection != current as c_ulong {
                c.update_epoch_end = true;
                c.send(option, selection as c_long);
                // a random start (the epoch count) counts from the first epoch
                let first = if selection >= s.epoch_count as c_ulong { 0 } else { selection };
                if (s.current[EE_SCREENS_OPTION_EPOCH_END as usize] as c_ulong) < first {
                    c.send(EE_SCREENS_OPTION_EPOCH_END, first as c_long);
                }
            }
        }
        EE_SCREENS_OPTION_VARIANT => {
            if let Some(v) = variant_of_line(selectable, selection) {
                if v != current as c_ulong {
                    c.send(option, v as c_long);
                }
            }
        }
        EE_SCREENS_OPTION_RECORD_GAMES..=EE_SCREENS_OPTION_CHEAT_CODES => {
            let checked = selection != 0;
            if checked != (current != 0) {
                c.send(option, checked as c_long);
            }
        }
        _ => {}
    }
    c
}

/// The start check's messages (the C++ shows each one's string: IDS_YOU_NEED_AT_LEAST_1_HUMAN ...).
pub const EE_SCREENS_START_NEED_HUMAN: u32 = 0;
pub const EE_SCREENS_START_ONE_PLAYER_MULTIPLAYER: u32 = 1;
pub const EE_SCREENS_START_ONE_PLAYER: u32 = 2;
pub const EE_SCREENS_START_SAME_COLOUR: u32 = 3;
pub const EE_SCREENS_START_SAME_TEAM: u32 = 4;
pub const EE_SCREENS_START_NOT_READY: u32 = 5;
pub const EE_SCREENS_START_TRANSFER: u32 = 6;
/// The most messages one check shows (one per player count, colour clash, team, ready and transfer).
pub const EE_SCREENS_START_MESSAGES: usize = 24;

/// A slot as the start check reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensLobbyStartSlot {
    pub kind: c_ulong,
    pub color: c_ulong,
    pub team: c_ulong,
    pub ready: bool,
    pub transfer: c_ulong,
}

/// The start check's outcome: whether the game may start, its messages in order, and whether a loaded game's
/// failed check hides the rows' controls 257..271.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeScreensLobbyStart {
    pub result: bool,
    pub hide_rows: bool,
    pub count: u32,
    pub messages: [u32; EE_SCREENS_START_MESSAGES],
}

impl Default for EeScreensLobbyStart {
    fn default() -> Self {
        EeScreensLobbyStart { result: true, hide_rows: false, count: 0, messages: [0; EE_SCREENS_START_MESSAGES] }
    }
}

impl EeScreensLobbyStart {
    fn fail(&mut self, message: u32) {
        self.result = false;
        if (self.count as usize) < EE_SCREENS_START_MESSAGES {
            self.messages[self.count as usize] = message;
            self.count += 1;
        }
    }
}

const COMPUTER: c_ulong = 0;
const HUMAN: c_ulong = 1;
const OBSERVER: c_ulong = 2;

/// `LobbyCheckStart` after the load: a loaded game (`load_type` not 0) passes when its file loaded and matched
/// (`load_ok`, the C++'s, with its own messages); a random map needs two players, one of them human, no two humans
/// of one colour, and not everyone on the host's team. Then every human and observer ready (multiplayer) and no
/// file transfer left.
pub fn start_check(
    load_type: c_ulong,
    load_ok: bool,
    multiplayer: bool,
    slots: &[EeScreensLobbyStartSlot; 16],
) -> EeScreensLobbyStart {
    let mut out = EeScreensLobbyStart::default();
    if load_type != 0 {
        out.result = load_ok;
    } else {
        let (mut count, mut humans) = (0u32, 0u32);
        for p in slots.iter().filter(|p| p.kind == COMPUTER || p.kind == HUMAN) {
            count += 1;
            if count > 1 && humans > 0 {
                break;
            }
            if p.kind == HUMAN {
                humans += 1;
            }
        }
        if count > 1 {
            if humans < 1 {
                out.fail(EE_SCREENS_START_NEED_HUMAN);
            }
        } else {
            out.fail(if multiplayer { EE_SCREENS_START_ONE_PLAYER_MULTIPLAYER } else { EE_SCREENS_START_ONE_PLAYER });
        }
        // a colour past the 16 (the original indexed past its table) clashes with nothing
        let mut used = [false; 16];
        for p in slots.iter().filter(|p| p.kind == HUMAN) {
            match used.get_mut(p.color as usize) {
                Some(true) => out.fail(EE_SCREENS_START_SAME_COLOUR),
                Some(u) => *u = true,
                None => {}
            }
        }
        if out.result {
            let team = slots[0].team;
            if team != 0
                && !slots[1..]
                    .iter()
                    .any(|p| (p.kind == COMPUTER || p.kind == HUMAN) && (p.team == 0 || p.team != team))
            {
                out.fail(EE_SCREENS_START_SAME_TEAM);
            }
        }
    }
    let plays = |p: &EeScreensLobbyStartSlot| p.kind == HUMAN || p.kind == OBSERVER;
    if out.result && multiplayer && slots.iter().any(|p| plays(p) && !p.ready) {
        out.fail(EE_SCREENS_START_NOT_READY);
    }
    if out.result {
        if slots[1..].iter().any(|p| plays(p) && p.transfer > 0) {
            out.fail(EE_SCREENS_START_TRANSFER);
        }
    } else if load_type != 0 {
        out.hide_rows = true;
    }
    out
}

/// [`game_type_action`]: nothing to do.
pub const EE_SCREENS_GAME_TYPE_NONE: u32 = 0;
/// [`game_type_action`]: push the load screen for a saved game.
pub const EE_SCREENS_GAME_TYPE_SAVED_GAME: u32 = 1;
/// [`game_type_action`]: push the load screen for a scenario.
pub const EE_SCREENS_GAME_TYPE_SCENARIO: u32 = 2;
/// [`game_type_action`]: back to a random map: the loaded game's options cleared, sent and shown again.
pub const EE_SCREENS_GAME_TYPE_RANDOM_MAP: u32 = 3;

/// The game type box's event (`EEUIELEGameType`): its line (random map, saved game, scenario) and the options'
/// load type. A random map does something only when a game or scenario was loaded.
pub fn game_type_action(selection: c_ulong, load_type: c_ulong) -> u32 {
    match selection {
        2 => EE_SCREENS_GAME_TYPE_SCENARIO,
        1 => EE_SCREENS_GAME_TYPE_SAVED_GAME,
        0 if load_type != 0 => EE_SCREENS_GAME_TYPE_RANDOM_MAP,
        _ => EE_SCREENS_GAME_TYPE_NONE,
    }
}

/// The map type box's event (`EEUIELEGameMapType`): the line's generator name differs from the options' map type
/// (`operator!=`'s multibyte compare of two C strings: their bytes), so the change is sent (option 2, the line and
/// the name) and stored.
pub fn map_type_changed(name: &[u8], current: &[u8]) -> bool {
    name != current
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden3;
#[cfg(test)]
mod tests;
