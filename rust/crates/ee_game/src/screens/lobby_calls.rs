//! The lobby's C++ reads and writes (`UI Events/EEScreens.cpp`: the original's `EEScreensLobbyOptions`,
//! `EEScreensLobbySlot`, `EEScreensLobbyLoad`, `ee_screenscpp_lobby_player`, `_put_lobby_player`, `_lobby_options`,
//! `_lobby_fill` and the `EE_SCREENS_GAME_LE_APPLY_SAVED` / `_BOOT_FREE` cases) in Rust over one-statement C++ shims
//! (UI lane round 9; the shims are `ee_screenscpp_lp_*`, `_lo_*`, `_ll_*` in `EEScreens.cpp`).

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the shims' `u64` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};

use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimText as Text};
use crate::screens::lobby_form::{
    EeScreensLobbyOptionsRecord, EeScreensLobbyPlayerRecord, LobbyPlayer, EE_SCREENS_LE_LOAD_MISMATCH,
    EE_SCREENS_LE_LOAD_NOT_FOUND, EE_SCREENS_LE_LOAD_OK, EE_SCREENS_LE_LOAD_READ_ERROR, EE_SCREENS_LE_SOURCE_BOOT,
    EE_SCREENS_LE_SOURCE_COPY, EE_SCREENS_LE_SOURCE_HEADER,
};
use crate::screens::lobby_options::{
    EE_SCREENS_OPTION_CHEAT_CODES, EE_SCREENS_OPTION_CUSTOM_CIVS, EE_SCREENS_OPTION_DIFFICULTY,
    EE_SCREENS_OPTION_EPOCH_END, EE_SCREENS_OPTION_EPOCH_START, EE_SCREENS_OPTION_LOCK_SPEED,
    EE_SCREENS_OPTION_LOCK_TEAMS, EE_SCREENS_OPTION_MAP_SIZE, EE_SCREENS_OPTION_RECORD_GAMES,
    EE_SCREENS_OPTION_RESOURCES, EE_SCREENS_OPTION_REVEAL_MAP, EE_SCREENS_OPTION_SPEED, EE_SCREENS_OPTION_UNIT_LIMIT,
    EE_SCREENS_OPTION_VARIANT, EE_SCREENS_OPTION_WONDERS,
};
use crate::screens::single_player::{EeScreensSinglePlayerSlot, EE_SCREENS_SP_CLOSED, EE_SCREENS_SP_HUMAN};

/// `EEScreensLobbySetOption`'s number of the victory option (its last case).
const LC_OPTION_VICTORY: u32 = 17;

/// The options of a source (`EE_SCREENS_LE_SOURCE_*`): the loaded header's, the game type's copy (when there is
/// one), else the lobby options (`gGameOptions`); an `EEGameOptions*`.
pub(crate) fn options_of<C: Cpp + ?Sized>(c: &mut C, source: u32) -> usize {
    if source == EE_SCREENS_LE_SOURCE_HEADER {
        let header = c.lobby_header();
        if header != 0 {
            return c.lobby_header_options(header);
        }
    }
    if source == EE_SCREENS_LE_SOURCE_COPY {
        let copy = c.lobby_copy();
        if copy != 0 {
            return copy;
        }
    }
    c.game_options_ptr()
}

/// The slot `index` of a source (the boot question's copy for `EE_SCREENS_LE_SOURCE_BOOT`); an
/// `EEGameOptionsPlayer*`, 0 for none.
pub(crate) fn slot_of<C: Cpp + ?Sized>(c: &mut C, source: u32, index: u32) -> usize {
    if source == EE_SCREENS_LE_SOURCE_BOOT {
        return c.lobby_boot();
    }
    if index >= 16 {
        return 0;
    }
    let options = options_of(c, source);
    c.options_player(options, index)
}

/// A slot of `source` and its name (a zero record and no name for none).
pub(crate) fn player<C: Cpp + ?Sized>(c: &mut C, source: u32, index: u32) -> LobbyPlayer {
    let p = slot_of(c, source, index);
    if p == 0 {
        return LobbyPlayer::default();
    }
    let r = EeScreensLobbyPlayerRecord {
        kind: c.lp_kind(p) as c_ulong,
        loaded_type: c.lp_loaded_type(p) as c_long,
        ping: c.lp_ping(p),
        slot: c.lp_slot(p) as c_ulong,
        color: c.lp_color(p) as c_ulong,
        team: c.lp_team(p) as c_ulong,
        citizens: c.lp_citizens(p) as c_ulong,
        transfer: c.lp_transfer(p) as c_ulong,
        is_local: c.lp_is_local(p),
        is_ready: c.lp_is_ready(p),
        b36: c.lp_b36(p),
        engine: c.lp_engine(p) as c_ulong,
    };
    let mut name = Vec::new();
    c.lp_name(p, EeScreensShimOut::of(&mut name));
    LobbyPlayer { r, name }
}

/// A slot of `source` from a record and a name (the boot question's copy made when there is none).
pub(crate) fn put_player<C: Cpp + ?Sized>(c: &mut C, source: u32, index: u32, player: &LobbyPlayer) {
    if source == EE_SCREENS_LE_SOURCE_BOOT && c.lobby_boot() == 0 {
        c.lobby_boot_new();
    }
    let p = slot_of(c, source, index);
    if p == 0 {
        return;
    }
    let r = &player.r;
    c.lp_set_kind(p, r.kind as u64);
    c.lp_set_loaded_type(p, r.loaded_type as i64);
    c.lp_set_name(p, Text::of(&player.name));
    c.lp_set_ping(p, r.ping);
    c.lp_set_slot(p, r.slot as u64);
    c.lp_set_color(p, r.color as u64);
    c.lp_set_team(p, r.team as u64);
    c.lp_set_citizens(p, r.citizens as u64);
    c.lp_set_transfer(p, r.transfer as u64);
    c.lp_set_is_local(p, r.is_local);
    c.lp_set_is_ready(p, r.is_ready);
    c.lp_set_b36(p, r.b36);
    c.lp_set_engine(p, r.engine as u64);
}

/// The options of `source`.
pub(crate) fn options<C: Cpp + ?Sized>(c: &mut C, source: u32) -> EeScreensLobbyOptionsRecord {
    let o = options_of(c, source);
    EeScreensLobbyOptionsRecord {
        load_type: c.lo_load_type(o) as c_ulong,
        map_size: c.lo_map_size(o) as c_ulong,
        resources: c.lo_resources(o) as c_ulong,
        epoch_start: c.lo_epoch_start(o) as c_long,
        epoch_end: c.lo_epoch_end(o) as c_long,
        unit_limit: c.lo_unit_limit(o) as c_ulong,
        wonders: c.lo_wonders(o) as c_ulong,
        variant: c.lo_variant(o) as c_ulong,
        difficulty: c.lo_difficulty(o) as c_ulong,
        speed: c.lo_speed(o) as c_ulong,
        multiplayer: c.lo_multiplayer(o),
        record_games: c.lo_record_games(o),
        custom_civs: c.lo_custom_civs(o),
        reveal_map: c.lo_reveal_map(o),
        lock_teams: c.lo_lock_teams(o),
        lock_speed: c.lo_lock_speed(o),
        cheat_codes: c.lo_cheat_codes(o),
        victory: c.lo_victory(o),
    }
}

/// `EE_SCREENS_GAME_LE_APPLY_SAVED` (`LobbyApplySavedGameOptions`): the loaded header's options into the lobby
/// options (all but the load type, the map type and the multiplayer flag), sent whole; nothing without a header.
pub(crate) fn apply_saved<C: Cpp + ?Sized>(c: &mut C) {
    let header = c.lobby_header();
    if header == 0 {
        return;
    }
    let o = c.lobby_header_options(header);
    let values: [(c_long, i64); 15] = [
        (EE_SCREENS_OPTION_MAP_SIZE, c.lo_map_size(o)),
        (EE_SCREENS_OPTION_RESOURCES, c.lo_resources(o)),
        (EE_SCREENS_OPTION_EPOCH_START, c.lo_epoch_start(o)),
        (EE_SCREENS_OPTION_EPOCH_END, c.lo_epoch_end(o)),
        (EE_SCREENS_OPTION_UNIT_LIMIT, c.lo_unit_limit(o)),
        (EE_SCREENS_OPTION_WONDERS, c.lo_wonders(o)),
        (EE_SCREENS_OPTION_VARIANT, c.lo_variant(o)),
        (EE_SCREENS_OPTION_DIFFICULTY, c.lo_difficulty(o)),
        (EE_SCREENS_OPTION_SPEED, c.lo_speed(o)),
        (EE_SCREENS_OPTION_RECORD_GAMES, c.lo_record_games(o) as i64),
        (EE_SCREENS_OPTION_CUSTOM_CIVS, c.lo_custom_civs(o) as i64),
        (EE_SCREENS_OPTION_REVEAL_MAP, c.lo_reveal_map(o) as i64),
        (EE_SCREENS_OPTION_LOCK_TEAMS, c.lo_lock_teams(o) as i64),
        (EE_SCREENS_OPTION_LOCK_SPEED, c.lo_lock_speed(o) as i64),
        (EE_SCREENS_OPTION_CHEAT_CODES, c.lo_cheat_codes(o) as i64),
    ];
    let victory = c.lo_victory(o) as i64;
    for (option, value) in values {
        c.lobby_set_option(option as u32, value);
    }
    c.lobby_set_option(LC_OPTION_VICTORY, victory);
    c.send_full_options();
}

/// `EE_SCREENS_GAME_LE_BOOT_FREE`: the boot question's copy freed.
pub(crate) fn boot_free<C: Cpp + ?Sized>(c: &mut C) {
    c.lobby_boot_delete();
    c.lobby_boot_clear();
}

/// The random map's and the single player's slots into the world's lobby options (a single player game; a closed
/// slot a new slot with its number), the options' write lock released (`ee_screenscpp_lobby_fill`).
pub(crate) fn fill<C: Cpp + ?Sized>(c: &mut C, slots: &[EeScreensSinglePlayerSlot; 16], human: &[u8], computer: &[u8]) {
    c.world_lobby_single();
    for (i, s) in slots.iter().enumerate() {
        let i = i as u32;
        c.lobby_player_reset(i);
        if s.source != EE_SCREENS_SP_CLOSED {
            c.lobby_player_set_type(i, s.kind as u32);
            c.lobby_player_set_color(i, s.color as u32);
            c.lobby_player_set_local(i, s.local);
            c.lobby_player_set_ready(i, s.ready);
            c.lobby_player_set_name(i, Text::of(if s.source == EE_SCREENS_SP_HUMAN { human } else { computer }));
        }
        c.lobby_player_set_slot(i, s.slot as u32);
    }
    c.lobby_options_write_unlock();
}

/// Where a saved game's check reads (`EEScreensLobbyLoad`'s header and file): the lobby's (`sLobbyHeader`,
/// `sLobbyFile`) or a check's own (a new header, freed after).
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum LobbyLoad {
    /// The load screen's: against the lobby options with this load type.
    Lobby(u32),
    /// The start's check: against the lobby options.
    Start,
}

/// `EEScreensLobbyLoad`: a save file's header and the options check, `EE_SCREENS_LE_LOAD_*`; the missing lines kept
/// for `EE_SCREENS_TEXT_LE_MISSING`.
pub(crate) fn lobby_load<C: Cpp + ?Sized>(c: &mut C, file: &[u8], directory: &[u8], how: LobbyLoad) -> u32 {
    match how {
        LobbyLoad::Lobby(load_type) => {
            if c.ll_open_lobby(Text::of(file), Text::of(directory)) != 0 {
                return EE_SCREENS_LE_LOAD_NOT_FOUND;
            }
            let header = c.lobby_header();
            let spec = c.lobby_file();
            ll_check(c, header, spec, Some(load_type))
        }
        LobbyLoad::Start => {
            let header = c.header_new();
            c.ll_check_clear();
            let r = if c.ll_open_check(Text::of(file), Text::of(directory)) != 0 {
                EE_SCREENS_LE_LOAD_NOT_FOUND
            } else {
                let spec = c.ll_check_file();
                ll_check(c, header, spec, None)
            };
            c.header_delete(header);
            r
        }
    }
}

/// The check after the file opened: the header's versions and load, then its options against the lobby's.
fn ll_check<C: Cpp + ?Sized>(c: &mut C, header: usize, spec: usize, load_type: Option<u32>) -> u32 {
    c.header_set_major(header);
    c.header_set_minor(header);
    if c.header_load(header, spec) != 0 {
        return EE_SCREENS_LE_LOAD_READ_ERROR;
    }
    c.ll_options_from_header(header);
    c.ll_missing_clear();
    let matched = match load_type {
        None => c.ll_check_start(header),
        Some(t) => {
            c.ll_current_from_game();
            c.ll_current_load_type(t);
            c.ll_check_load(header)
        }
    };
    if matched {
        return EE_SCREENS_LE_LOAD_OK;
    }
    c.lobby_missing_clear();
    for i in 0..c.ll_missing_count() {
        c.lobby_missing_add(i);
    }
    EE_SCREENS_LE_LOAD_MISMATCH
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn a_source_falls_back_to_the_lobby_options() {
        let mut f = Fake::default();
        f.answer("game_options_ptr", &[0x10, 0x10]).answer("lobby_copy", &[0x30]);
        assert_eq!(options_of(&mut f, EE_SCREENS_LE_SOURCE_HEADER), 0x10);
        assert_eq!(options_of(&mut f, EE_SCREENS_LE_SOURCE_COPY), 0x30);
        assert_eq!(options_of(&mut f, 0), 0x10);
        assert_eq!(f.calls(), "lobby_header()\ngame_options_ptr()\nlobby_copy()\ngame_options_ptr()");
    }

    #[test]
    fn a_slot_past_sixteen_is_none_and_the_boot_copy_is_made_on_a_put() {
        let mut f = Fake::default();
        assert_eq!(player(&mut f, 0, 16), LobbyPlayer::default());
        let p = LobbyPlayer { r: EeScreensLobbyPlayerRecord { kind: 2, ..Default::default() }, name: b"Bob".to_vec() };
        f.answer("lobby_boot", &[0, 0x50]);
        put_player(&mut f, EE_SCREENS_LE_SOURCE_BOOT, 0, &p);
        let calls = f.calls();
        assert!(calls.starts_with("lobby_boot()\nlobby_boot_new()\nlobby_boot()\nlp_set_kind(80, 2)"), "{calls}");
        assert!(calls.contains("lp_set_name(80, \"Bob\")"));
        assert!(calls.ends_with("lp_set_engine(80, 0)"));
    }

    #[test]
    fn a_slot_reads_every_field_and_its_name() {
        let mut f = Fake::default();
        f.answer("game_options_ptr", &[0x10]).answer("options_player", &[0x60]);
        f.answer("lp_kind", &[3]).answer("lp_is_ready", &[1]).text("lp_name", b"Ann");
        let p = player(&mut f, 0, 2);
        assert_eq!((p.r.kind, p.r.is_ready, p.name.as_slice()), (3, true, &b"Ann"[..]));
        assert!(f.calls().contains("options_player(16, 2)"));
        let mut f = Fake::default();
        f.answer("game_options_ptr", &[0x10]).answer("lo_unit_limit", &[200]).answer("lo_victory", &[1]);
        let o = options(&mut f, 0);
        assert_eq!((o.unit_limit, o.victory, o.reveal_map), (200, true, false));
    }

    #[test]
    fn the_saved_options_apply_only_with_a_header() {
        let mut f = Fake::default();
        apply_saved(&mut f);
        assert_eq!(f.calls(), "lobby_header()");
        let mut f = Fake::default();
        f.answer("lobby_header", &[0x70]).answer("lobby_header_options", &[0x71]).answer("lo_map_size", &[4]);
        apply_saved(&mut f);
        let calls = f.calls();
        assert!(calls.contains("lobby_set_option(1, 4)"));
        assert!(calls.contains("lobby_set_option(17, 0)\nsend_full_options()"));
        assert_eq!(calls.matches("lobby_set_option").count(), 16);
        let mut f = Fake::default();
        boot_free(&mut f);
        assert_eq!(f.calls(), "lobby_boot_delete()\nlobby_boot_clear()");
    }

    #[test]
    fn a_fill_resets_every_slot_and_names_the_open_ones() {
        let mut slots = [EeScreensSinglePlayerSlot { source: EE_SCREENS_SP_CLOSED, slot: 4, ..Default::default() }; 16];
        slots[0] =
            EeScreensSinglePlayerSlot { source: EE_SCREENS_SP_HUMAN, kind: 1, local: true, ..Default::default() };
        let mut f = Fake::default();
        fill(&mut f, &slots, b"Me", b"CPU");
        let calls = f.calls();
        assert!(calls.starts_with("world_lobby_single()\nlobby_player_reset(0)\nlobby_player_set_type(0, 1)"));
        assert!(calls.contains("lobby_player_set_name(0, \"Me\")\nlobby_player_set_slot(0, 0)\nlobby_player_reset(1)"));
        assert!(calls.contains("lobby_player_reset(15)\nlobby_player_set_slot(15, 4)\nlobby_options_write_unlock()"));
    }

    #[test]
    fn a_load_checks_the_header_and_keeps_the_missing_lines() {
        let mut f = Fake::default();
        f.answer("ll_open_lobby", &[1]);
        assert_eq!(lobby_load(&mut f, b"a", b"b", LobbyLoad::Lobby(2)), EE_SCREENS_LE_LOAD_NOT_FOUND);
        let mut f = Fake::default();
        f.answer("lobby_header", &[0x70]).answer("lobby_file", &[0x80]).answer("ll_missing_count", &[2]);
        assert_eq!(lobby_load(&mut f, b"a", b"b", LobbyLoad::Lobby(2)), EE_SCREENS_LE_LOAD_MISMATCH);
        let calls = f.calls();
        assert!(calls.contains("header_load(112, 128)\nll_options_from_header(112)\nll_missing_clear()"));
        assert!(calls.contains("ll_current_load_type(2)\nll_check_load(112)\nlobby_missing_clear()"));
        assert!(calls.ends_with("lobby_missing_add(0)\nlobby_missing_add(1)"));
        let mut f = Fake::default();
        f.answer("header_new", &[0x90]).answer("ll_check_file", &[0x91]).answer("ll_check_start", &[1]);
        assert_eq!(lobby_load(&mut f, b"a", b"b", LobbyLoad::Start), EE_SCREENS_LE_LOAD_OK);
        assert!(f.calls().ends_with("ll_check_start(144)\nheader_delete(144)"));
        let mut f = Fake::default();
        f.answer("header_new", &[0x90]).answer("header_load", &[1]);
        assert_eq!(lobby_load(&mut f, b"a", b"b", LobbyLoad::Start), EE_SCREENS_LE_LOAD_READ_ERROR);
        assert!(f.calls().ends_with("header_delete(144)"));
    }
}
