//! `NESynchronousServer`'s state and members in Rust, as they move (PORT_LEARNINGS "LLE Network Engine, round 4").
//! This part: the world checksums the clients and the server answer each tick with, the out-of-sync slots they name,
//! and the removal of players from a running game (`LogChecksumResponse`, `RemovePlayerFromGameInProgress` and the
//! out-of-sync block of `SendCommunicationsTick`). The server's other fields are still the C++ class's: the host
//! answers them, with the game's virtual members.

use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

/// The server's own slot
pub const SLOT_SERVER: u8 = 0xfe;

/// The server's fields the moved members read and write (still the C++ class's)
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SvField {
    /// NESynchronousCommunications::mNumHumanPlayers (read, written)
    NumHumanPlayers = 0,
    /// mMaxPlayers
    MaxPlayers = 1,
}

/// The server's calls out
pub trait ServerHost {
    fn get(&self, f: SvField) -> u32;
    fn set(&self, f: SvField, value: u32);
    /// the game's IsPlayerPresent(player)
    fn is_player_present(&self, player: u8) -> bool;
    /// the game's GetPlayerSendToSlot(player)
    fn player_send_to_slot(&self, player: u8) -> u8;
    /// UnregisterSlot(slot) (the message manager's slot, never the server's or none)
    fn unregister_slot(&self, slot: u8);
    /// the game's BeforeRemovingPlayers()
    fn before_removing_players(&self);
    /// ProcessLocalUserInput(new NEMRemovePlayer(player, booted, out of sync, exiting), kSlotNone)
    fn remove_player_input(&self, player: u8, was_booted: bool, is_oos: bool, game_is_exiting: bool);
}

/// The checksums of each tick: (checksum, slot) in a `std::multimap`'s order (by checksum, equal ones as they came)
#[derive(Default)]
struct Checks {
    responses: BTreeMap<u32, Vec<(u32, u8)>>,
    oos_slots: Vec<u8>,
}

impl Checks {
    fn insert(&mut self, tick: u32, checksum: u32, slot: u8) {
        let list = self.responses.entry(tick).or_default();
        let at = list.partition_point(|e| e.0 <= checksum);
        list.insert(at, (checksum, slot));
    }
}

pub struct Server<H: ServerHost> {
    pub host: H,
    /// mChecksumResponses and mOOSSlots under mChecksumResponsesLock
    checks: Mutex<Checks>,
    /// mIsOOS
    is_oos: AtomicBool,
}

impl<H: ServerHost> Server<H> {
    pub fn new(host: H) -> Self {
        Server { host, checks: Mutex::new(Checks::default()), is_oos: AtomicBool::new(false) }
    }

    fn checks(&self) -> std::sync::MutexGuard<'_, Checks> {
        self.checks.lock().unwrap_or_else(|e| e.into_inner())
    }

    pub fn is_oos(&self) -> bool {
        self.is_oos.load(Ordering::SeqCst)
    }

    /// LogChecksumResponse: collects a tick's checksums; once as many have come as there are humans, a tick whose
    /// checksums differ names its out-of-sync slots (two humans: every client; more: the slots off the most common
    /// checksum, the server's winning a tie, the server's slot first) and the server is out of sync. The ticks before
    /// it are forgotten (the tick itself is kept, as the original's erase left it).
    pub fn log_checksum_response(&self, tick: u32, checksum: u32, slot: u8) {
        let num_players = self.host.get(SvField::NumHumanPlayers) as u8;
        let mut c = self.checks();
        if tick == 0 || num_players <= 1 {
            return;
        }
        let known = c.responses.contains_key(&tick);
        c.insert(tick, checksum, slot);
        if !known {
            return;
        }
        let list = c.responses[&tick].clone();
        if list.len() != num_players as usize {
            return;
        }
        let mut counts: BTreeMap<u32, u32> = BTreeMap::new();
        for e in &list {
            *counts.entry(e.0).or_insert(0) += 1;
        }
        if counts.len() != 1 {
            // (the field read again under the lock, as the original did; a getter, no call back)
            if self.host.get(SvField::NumHumanPlayers) as u8 == 2 {
                for e in &list {
                    if e.1 != SLOT_SERVER {
                        c.oos_slots.push(e.1);
                    }
                }
            } else {
                let servers = list.iter().find(|e| e.1 == SLOT_SERVER).map_or(0, |e| e.0);
                let mut highest = 0u32;
                let mut most_common = 0u32;
                for (&k, &n) in &counts {
                    if n > highest || (n == highest && k == servers) {
                        most_common = k;
                        highest = n;
                    }
                }
                for e in &list {
                    if e.0 != most_common {
                        c.oos_slots.push(e.1);
                    }
                }
                if c.oos_slots.len() > 1 {
                    // std::sort with "the server's slot before any other" over a few slots: an insertion sort, so
                    // the others keep their order
                    c.oos_slots.sort_by_key(|&s| s != SLOT_SERVER);
                }
            }
            self.is_oos.store(true, Ordering::SeqCst);
        }
        c.responses.retain(|&t, _| t >= tick);
    }

    /// RemovePlayerFromGameInProgress: the player whose send slot is `slot` (the server's slot: its own player 0xfe)
    /// is announced removed; a booted client's slot is unregistered and counted out of the humans
    pub fn remove_player_from_game_in_progress(&self, slot: u8, was_booted: bool, is_oos: bool, game_is_exiting: bool) {
        let mut player: u8 = SLOT_SERVER;
        let mut found = true;
        if slot != SLOT_SERVER {
            found = false;
            player = 1;
            while (player as u32) < self.host.get(SvField::MaxPlayers) & 0xff {
                self.host.player_send_to_slot(player);
                if !self.host.is_player_present(player) || self.host.player_send_to_slot(player) != slot {
                    player = player.wrapping_add(1);
                    continue;
                }
                if player != SLOT_SERVER && was_booted {
                    self.host.unregister_slot(slot);
                    let n = self.host.get(SvField::NumHumanPlayers) as u8;
                    self.host.set(SvField::NumHumanPlayers, n.wrapping_sub(1) as u32);
                }
                found = true;
                break;
            }
        }
        if !found {
            return;
        }
        self.host.remove_player_input(player, was_booted, is_oos, game_is_exiting);
    }

    /// SendCommunicationsTick's out-of-sync block (after the tick went): the game is told players go, each
    /// out-of-sync slot's player is removed as booted, and the server is in sync again
    pub fn remove_oos_players(&self) {
        if !self.is_oos() {
            return;
        }
        self.host.before_removing_players();
        let mut i = 0;
        loop {
            let slot = {
                let c = self.checks();
                if i >= c.oos_slots.len() {
                    break;
                }
                c.oos_slots[i]
            };
            self.remove_player_from_game_in_progress(slot, true, true, false);
            i += 1;
        }
        self.is_oos.store(false, Ordering::SeqCst);
        self.checks().oos_slots.clear();
    }

    /// The checksums' state (the tests): "tick:checksum/slot,...;..." then the out-of-sync slots
    pub fn checks_state(&self) -> String {
        let c = self.checks();
        let mut s = String::new();
        for (t, list) in &c.responses {
            s += &format!("{t}:");
            for (i, e) in list.iter().enumerate() {
                s += &format!("{}{}/{}", if i > 0 { "," } else { "" }, e.0, e.1);
            }
            s += ";";
        }
        s += " oos";
        for o in &c.oos_slots {
            s += &format!(" {o}");
        }
        s
    }
}
