//! NESynchronousCommunications' state (the base class of the lock-step server and client): the last tick received,
//! the player number, the communications ticks the physics processed, the physics' two averages, the human players,
//! the paused and multiplayer flags. The C++ base class holds one (`EeNetComms`, created by its constructor) and
//! forwards its accessors; the Rust server and client share it and read and write the fields directly (round 4 and
//! before, they asked the C++ through their hosts).
//!
//! Every field is an atomic: the original's were plain fields read and written by the game's threads, the server's
//! and client's threads and the physics (mNumberOfCTsProcessedInPhysics was `volatile`, its increment load, add,
//! store). The values are the original's 32-bit ones (`long` and `unsigned long` on Win32); the last tick received
//! is a `long` kept as its 32 bits.

use std::sync::atomic::AtomicU32;
use std::sync::atomic::Ordering::SeqCst;

use crate::net::ffi_comms::EeNetCommsField as Field;

const FIELDS: usize = 8;

pub struct Comms {
    fields: [AtomicU32; FIELDS],
}

impl Comms {
    /// The constructor: the CTs processed given (the server's is its first communications tick, the client's 0), the
    /// last tick received -2, the rest 0 or false
    pub fn new(cts_processed: u32) -> Self {
        let c = Comms { fields: std::array::from_fn(|_| AtomicU32::new(0)) };
        c.set(Field::LastTickReceived, -2i32 as u32);
        c.set(Field::CtsProcessed, cts_processed);
        c
    }

    /// A field's value (the last tick received as its 32 bits; a flag 0 or 1; the human players a byte)
    pub fn get(&self, f: Field) -> u32 {
        self.fields[f as usize].load(SeqCst)
    }

    /// A field set (to its type: the human players' byte, a flag's truth)
    pub fn set(&self, f: Field, value: u32) {
        let v = match f {
            Field::NumHumanPlayers => value & 0xff,
            Field::IsGamePaused | Field::IsMultiplayerSession => (value != 0) as u32,
            _ => value,
        };
        self.fields[f as usize].store(v, SeqCst);
    }

    /// The last tick received, a `long` (-2 before the connection, -1 "none")
    pub fn last_tick_received(&self) -> i32 {
        self.get(Field::LastTickReceived) as i32
    }

    pub fn is_game_paused(&self) -> bool {
        self.get(Field::IsGamePaused) != 0
    }

    pub fn is_multiplayer_session(&self) -> bool {
        self.get(Field::IsMultiplayerSession) != 0
    }

    pub fn num_human_players(&self) -> u8 {
        self.get(Field::NumHumanPlayers) as u8
    }

    /// IncrementNumberOfCTsProcessedInPhysics (wrapping, as the 32-bit `unsigned long` did)
    pub fn increment_cts_processed(&self) {
        self.fields[Field::CtsProcessed as usize].fetch_add(1, SeqCst);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_constructor_and_the_fields_are_the_originals() {
        let c = Comms::new(50);
        assert_eq!(c.last_tick_received(), -2);
        assert_eq!(c.get(Field::LastTickReceived), 0xffff_fffe);
        assert_eq!(c.get(Field::CtsProcessed), 50);
        for f in [
            Field::PlayerNumber,
            Field::AveragePhysicsUpdateTime,
            Field::AverageTimeBetweenPhysicsTicks,
            Field::NumHumanPlayers,
            Field::IsGamePaused,
            Field::IsMultiplayerSession,
        ] {
            assert_eq!(c.get(f), 0, "{f:?}");
        }
        // a byte and two flags
        c.set(Field::NumHumanPlayers, 0x1_02);
        assert_eq!(c.num_human_players(), 2);
        c.set(Field::IsGamePaused, 7);
        assert!(c.is_game_paused());
        assert_eq!(c.get(Field::IsGamePaused), 1);
        c.set(Field::IsMultiplayerSession, 0);
        assert!(!c.is_multiplayer_session());
        // the 32-bit tick, wrapping counts
        c.set(Field::LastTickReceived, (-1i32) as u32);
        assert_eq!(c.last_tick_received(), -1);
        c.set(Field::CtsProcessed, u32::MAX);
        c.increment_cts_processed();
        assert_eq!(c.get(Field::CtsProcessed), 0);
        c.set(Field::AverageTimeBetweenPhysicsTicks, 123);
        assert_eq!(c.get(Field::AverageTimeBetweenPhysicsTicks), 123);
        assert_eq!(c.get(Field::AveragePhysicsUpdateTime), 0);
    }
}
