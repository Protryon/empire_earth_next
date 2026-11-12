//! The players' diplomacy table (RUST_PLAN R5b.4 leaf 5; the combat lane's store fields (5)): each player object's id
//! and stances toward the 17 player ids (`EEPlayer::mEnemies`: non-zero, at war), readable by the player object (the
//! combat reads' player handle) without a call into C++.
//!
//! The C++ reports a player's whole table where it writes `mEnemies` (`EEPlayer::Reset`, the loads,
//! `SetDiplomaticStance`) and drops it with the player object (`~RTSPlayer`); the capture's USCK check compares the
//! world's players with it.
//!
//! Determinism: a `BTreeMap` by the object's address, read by key only; plain stores of the C++ values.

use std::collections::BTreeMap;
use std::sync::Mutex;

/// The player ids (`gWorld.mPlayers`, `mEnemies`: 17 entries).
pub const EE_UNITS_PLAYER_COUNT: usize = 17;

/// One player object's table.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PlayerRecord {
    /// `mPlayerID`
    pub id: u32,
    /// `mEnemies` (the `unsigned long`s' low 32 bits: the stances are 0 and 1, the scenario's diplomacy values)
    pub enemies: [u32; EE_UNITS_PLAYER_COUNT],
}

/// The tables by player object.
#[derive(Default)]
pub struct Players {
    players: BTreeMap<u64, PlayerRecord>,
}

impl Players {
    pub const fn new() -> Self {
        Players { players: BTreeMap::new() }
    }

    /// A player's table as the C++ has it now (replacing any).
    pub fn set(&mut self, object: u64, record: PlayerRecord) {
        self.players.insert(object, record);
    }

    /// The player object goes. True when it had a table.
    pub fn drop_player(&mut self, object: u64) -> bool {
        self.players.remove(&object).is_some()
    }

    pub fn get(&self, object: u64) -> Option<&PlayerRecord> {
        self.players.get(&object)
    }

    /// `mEnemies[other]` of the player object (None: no table, or an id out of range).
    pub fn enemy(&self, object: u64, other: u32) -> Option<u32> {
        self.players.get(&object).and_then(|p| p.enemies.get(other as usize).copied())
    }

    /// TEST ONLY: the world's players (object, as the C++ has it) against the tables: the count that differ or
    /// have none.
    pub fn check(&self, objects: &[(u64, PlayerRecord)]) -> u32 {
        objects.iter().filter(|(o, r)| self.players.get(o) != Some(r)).count() as u32
    }

    pub fn len(&self) -> usize {
        self.players.len()
    }

    pub fn is_empty(&self) -> bool {
        self.players.is_empty()
    }
}

/// The process's tables (written under the world's write lock, as the unit store).
pub static PLAYERS: Mutex<Players> = Mutex::new(Players::new());

/// Runs `f` over the process's tables (a poisoned lock is taken over: plain values only).
pub fn with_players<R>(f: impl FnOnce(&mut Players) -> R) -> R {
    let mut guard = PLAYERS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

/// A player's table: its id and its 17 stances (`enemies` has 17 words).
///
/// # Safety
/// `enemies` points to 17 readable words.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_set(object: u64, id: u32, enemies: *const u32) {
    let mut record = PlayerRecord { id, ..Default::default() };
    // SAFETY: the caller's contract
    record.enemies.copy_from_slice(unsafe { std::slice::from_raw_parts(enemies, EE_UNITS_PLAYER_COUNT) });
    with_players(|p| p.set(object, record));
}

/// The player object goes (`~RTSPlayer`).
#[no_mangle]
pub extern "C" fn ee_units_player_drop(object: u64) -> bool {
    with_players(|p| p.drop_player(object))
}

/// `mEnemies[other]` of the player object into `out`; false when it has no table (or `other` is out of range).
///
/// # Safety
/// `out` points to a writable word.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_enemy(object: u64, other: u32, out: *mut u32) -> bool {
    match with_players(|p| p.enemy(object, other)) {
        Some(v) => {
            // SAFETY: the caller's contract
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// TEST ONLY: the world's players' tables (`objects`, `ids`: `count` each; `enemies`: 17 words a player) against the
/// Rust ones; the count that differ.
///
/// # Safety
/// The pointers hold `count` (`enemies`: `17 * count`) readable entries, or `count` is 0.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_check(
    objects: *const u64,
    ids: *const u32,
    enemies: *const u32,
    count: usize,
) -> u32 {
    if count == 0 {
        return 0;
    }
    // SAFETY: the caller's contract
    let (objects, ids, enemies) = unsafe {
        (
            std::slice::from_raw_parts(objects, count),
            std::slice::from_raw_parts(ids, count),
            std::slice::from_raw_parts(enemies, count * EE_UNITS_PLAYER_COUNT),
        )
    };
    let list: Vec<(u64, PlayerRecord)> = (0..count)
        .map(|i| {
            let mut r = PlayerRecord { id: ids[i], ..Default::default() };
            r.enemies.copy_from_slice(&enemies[i * EE_UNITS_PLAYER_COUNT..(i + 1) * EE_UNITS_PLAYER_COUNT]);
            (objects[i], r)
        })
        .collect();
    with_players(|p| p.check(&list))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rec(id: u32) -> PlayerRecord {
        let mut r = PlayerRecord { id, ..Default::default() };
        for (i, e) in r.enemies.iter_mut().enumerate() {
            *e = u32::from(i as u32 != id);
        }
        r
    }

    #[test]
    fn a_table_is_read_by_its_object() {
        let mut p = Players::new();
        p.set(0x100, rec(1));
        p.set(0x200, rec(2));
        assert_eq!(p.enemy(0x100, 1), Some(0));
        assert_eq!(p.enemy(0x100, 2), Some(1));
        assert_eq!(p.enemy(0x100, 17), None);
        assert_eq!(p.enemy(0x300, 1), None);
        // an alliance written over
        let mut allied = rec(1);
        allied.enemies[2] = 0;
        p.set(0x100, allied);
        assert_eq!(p.enemy(0x100, 2), Some(0));
        assert_eq!(p.check(&[(0x100, allied), (0x200, rec(2))]), 0);
        assert_eq!(p.check(&[(0x100, rec(1)), (0x400, rec(4))]), 2);
        assert!(p.drop_player(0x100));
        assert!(!p.drop_player(0x100));
        assert_eq!(p.len(), 1);
    }

    #[test]
    fn the_ffi_copies_the_seventeen_words() {
        let words: Vec<u32> = (0..17).collect();
        // SAFETY: 17 words
        unsafe { ee_units_player_set(0xfe00_0000, 5, words.as_ptr()) };
        let mut out = 0;
        // SAFETY: a word
        assert!(unsafe { ee_units_player_enemy(0xfe00_0000, 16, &mut out) });
        assert_eq!(out, 16);
        let ids = [5u32];
        let objects = [0xfe00_0000u64];
        // SAFETY: one player's entries
        assert_eq!(unsafe { ee_units_player_check(objects.as_ptr(), ids.as_ptr(), words.as_ptr(), 1) }, 0);
        assert!(ee_units_player_drop(0xfe00_0000));
    }
}
