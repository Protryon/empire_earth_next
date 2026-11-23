//! TEST ONLY: the unit store's consistency check (the units' lane, round 8; was EEUnitStore.cpp `EEUnitStoreCheck`):
//! the players' listed units, the unlisted units, the world's unit chain with the checksum's flags 1 and 2, the players'
//! diplomacy tables and the stepped units' order against the store, the USCK record, and the stop on a difference.
//! The world's objects are read by [`UsHost`] (the game's: `ee_units_us_host`, EEUnitStore.cpp, one statement each);
//! the store's checks are the Rust store's.

use crate::app::capture_record::{flush, Record};
use crate::units::store::{EeUnitRecord, EeUnitStoreCheck};

// ---- the statements (`ee_units_us_host`'s `op`; `a` an object, `b`, `c` values)
/// `gWorld.mPlayers[b]`
pub const EE_UNITS_US_PLAYER: i32 = 1;
/// player `a`'s `mPlayerID`
pub const EE_UNITS_US_PLAYER_ID: i32 = 2;
/// `static_cast<RTSPlayer*>(` player `a)`
pub const EE_UNITS_US_PLAYER_RTS: i32 = 3;
/// player `a`'s `mFirstUnit`
pub const EE_UNITS_US_FIRST_UNIT: i32 = 4;
/// unit `a`'s `mNext`
pub const EE_UNITS_US_NEXT_UNIT: i32 = 5;
/// `Fill(` unit `a, b, ` player object `c, *out)`
pub const EE_UNITS_US_FILL: i32 = 6;
/// unit `a`'s `mPlayer` when it is an `EEUnit`, else NULL
pub const EE_UNITS_US_UNIT_PLAYER: i32 = 7;
/// `gWorld.mUnits.mFirst`
pub const EE_UNITS_US_CHAIN_FIRST: i32 = 8;
/// unit `a`'s `mTileNode->mNext`
pub const EE_UNITS_US_CHAIN_NEXT: i32 = 9;
/// unit `a`'s `mUnitID`
pub const EE_UNITS_US_UNIT_ID: i32 = 10;
/// unit `a`'s `GetChecksum(b)`
pub const EE_UNITS_US_CHECKSUM: i32 = 11;
/// `PlayerWords(` player `a, out)` (its 17 stances)
pub const EE_UNITS_US_PLAYER_WORDS: i32 = 12;
/// `gWorld.GetNumUpdateUnits()`
pub const EE_UNITS_US_ORDER_COUNT: i32 = 13;
/// `gWorld.GetUpdateUnitID(b)`
pub const EE_UNITS_US_ORDER_ID: i32 = 14;
/// `gPhysics != NULL ? gPhysics->mNumTicksDone : 0`
pub const EE_UNITS_US_TICK: i32 = 15;
/// `exit(3)`
pub const EE_UNITS_US_EXIT: i32 = 16;

/// "USCK", as `EE_COMBAT_TAG` builds it (the first letter in the low byte)
pub const USCK: u32 = u32::from_le_bytes(*b"USCK");

/// The world's objects and the store's checks.
pub trait UsHost {
    /// one statement (`EE_UNITS_US_*`); `out` the record or the stances it writes
    fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut std::ffi::c_void) -> i64;
    /// `ee_units_store_check`
    fn store_check(&mut self, objects: &[EeUnitRecord]) -> EeUnitStoreCheck;
    /// `ee_units_store_unlisted`
    fn store_unlisted(&mut self) -> Vec<u64>;
    /// `ee_units_store_check_unlisted`
    fn store_check_unlisted(&mut self, objects: &[EeUnitRecord], check: &mut EeUnitStoreCheck) -> u32;
    /// `ee_units_store_check_on_map`
    fn store_check_on_map(&mut self, ids: &[u32], objects: &[u64]) -> u32;
    /// `ee_units_store_checksum_flag1`, `_flag2`
    fn store_checksum_flags(&mut self) -> (u64, u64);
    /// `ee_units_player_check`
    fn player_check(&mut self, objects: &[u64], ids: &[u32], enemies: &[u32]) -> u32;
    /// `ee_units_order_check` (the index plus one of the first difference, 0 for none; the unrecorded count)
    fn order_check(&mut self, ids: &[u32]) -> (u32, u32);
    /// `ee_units_order_count`
    fn order_count(&mut self) -> u32;
    /// the USCK record
    fn record(&mut self, r: &Record);
}

/// the most stepped units without a store record seen yet (the original's function static)
static MOST_UNRECORDED: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(0);

fn fill<H: UsHost + ?Sized>(h: &mut H, unit: u64, owner: u32, player: u64) -> EeUnitRecord {
    let mut r = EeUnitRecord::default();
    h.us(EE_UNITS_US_FILL, unit, i64::from(owner), player, (&mut r as *mut EeUnitRecord).cast());
    r
}

/// A unit's record with its own player as the list's (`thePlayer != NULL ? thePlayer->mPlayerID : 0`).
fn fill_own<H: UsHost + ?Sized>(h: &mut H, unit: u64) -> EeUnitRecord {
    let player = h.us(EE_UNITS_US_UNIT_PLAYER, unit, 0, 0, std::ptr::null_mut()) as u64;
    let (owner, object) = if player != 0 {
        (
            h.us(EE_UNITS_US_PLAYER_ID, player, 0, 0, std::ptr::null_mut()) as u32,
            h.us(EE_UNITS_US_PLAYER_RTS, player, 0, 0, std::ptr::null_mut()) as u64,
        )
    } else {
        (0, 0)
    };
    fill(h, unit, owner, object)
}

/// `EEUnitStoreCheck(where)`: the listed units' count; a difference reports and stops the process (exit 3).
pub fn check<H: UsHost + ?Sized>(h: &mut H, at: u64) -> u64 {
    let null = std::ptr::null_mut();
    let mut objects = Vec::new();
    let mut listed: u64 = 0;
    for i in 0..17 {
        let player = h.us(EE_UNITS_US_PLAYER, 0, i, 0, null) as u64;
        if player == 0 {
            continue;
        }
        let mut count = 0;
        let mut unit = h.us(EE_UNITS_US_FIRST_UNIT, player, 0, 0, null) as u64;
        while unit != 0 {
            let owner = h.us(EE_UNITS_US_PLAYER_ID, player, 0, 0, null) as u32;
            let object = h.us(EE_UNITS_US_PLAYER_RTS, player, 0, 0, null) as u64;
            objects.push(fill(h, unit, owner, object));
            count += 1;
            unit = h.us(EE_UNITS_US_NEXT_UNIT, unit, 0, 0, null) as u64;
        }
        listed += count;
    }
    let mut check = h.store_check(&objects);
    // the unlisted units' records (the shots, a scenario's path points) against their objects; a difference counts
    // like a field's (the compared count stays the listed units', so the USCK record is unchanged while there is none)
    let unlisted = h.store_unlisted();
    let records: Vec<EeUnitRecord> = unlisted.iter().map(|&u| fill_own(h, u)).collect();
    let unlisted_bad = h.store_check_unlisted(&records, &mut check);
    if unlisted_bad != 0 {
        eprintln!("EE-UNIT-STORE: {} of the {} unlisted units differ from the store", unlisted_bad, unlisted.len());
        check.mismatches = check.mismatches.wrapping_add(unlisted_bad);
    }
    // the world's unit chain (the out-of-sync checksum's walk) against the records' on-the-map flags, and the
    // checksum's flags 1 and 2 over it (EEComplexUnit::GetChecksum) against the store's sums; a mismatch counts like a
    // field's
    let mut chain_ids = Vec::new();
    let mut chain_objects = Vec::new();
    let mut flag1: u32 = 0;
    let mut flag2: u32 = 0;
    let mut unit = h.us(EE_UNITS_US_CHAIN_FIRST, 0, 0, 0, null) as u64;
    while unit != 0 {
        chain_ids.push(h.us(EE_UNITS_US_UNIT_ID, unit, 0, 0, null) as u32);
        chain_objects.push(unit);
        flag1 = flag1.wrapping_add(h.us(EE_UNITS_US_CHECKSUM, unit, 1, 0, null) as u32);
        flag2 = flag2.wrapping_add(h.us(EE_UNITS_US_CHECKSUM, unit, 2, 0, null) as u32);
        unit = h.us(EE_UNITS_US_CHAIN_NEXT, unit, 0, 0, null) as u64;
    }
    let off_map = h.store_check_on_map(&chain_ids, &chain_objects);
    let (store1, store2) = h.store_checksum_flags();
    let (store1, store2) = (store1 as u32, store2 as u32);
    if off_map != 0 || store1 != flag1 || store2 != flag2 {
        eprintln!(
            "EE-UNIT-STORE: the unit chain ({} units) and the store's on-the-map records differ: {}; flag 1 {:08x} over \
             the chain, {:08x} over the store; flag 2 {:08x}, {:08x}",
            chain_ids.len(),
            off_map,
            flag1,
            store1,
            flag2,
            store2
        );
        check.mismatches = check.mismatches.wrapping_add(if off_map != 0 { off_map } else { 1 });
    }
    // the world's players' diplomacy tables (players.rs); a difference counts like a field's
    let mut player_objects = Vec::new();
    let mut player_ids = Vec::new();
    let mut enemies = Vec::new();
    for i in 0..17 {
        let player = h.us(EE_UNITS_US_PLAYER, 0, i, 0, null) as u64;
        if player == 0 {
            continue;
        }
        player_objects.push(h.us(EE_UNITS_US_PLAYER_RTS, player, 0, 0, null) as u64);
        player_ids.push(h.us(EE_UNITS_US_PLAYER_ID, player, 0, 0, null) as u32);
        let mut words = [0u32; 17];
        h.us(EE_UNITS_US_PLAYER_WORDS, player, 0, 0, words.as_mut_ptr().cast());
        enemies.extend_from_slice(&words);
    }
    let differ = h.player_check(&player_objects, &player_ids, &enemies);
    if differ != 0 {
        eprintln!(
            "EE-UNIT-STORE: {} of the {} players' diplomacy tables differ from the store",
            differ,
            player_objects.len()
        );
        check.mismatches = check.mismatches.wrapping_add(differ);
    }
    // the stepped units' deque against the Rust list (order.rs), in order; a difference counts like a field's
    let order_count = h.us(EE_UNITS_US_ORDER_COUNT, 0, 0, 0, null) as usize;
    let order_ids: Vec<u32> =
        (0..order_count).map(|i| h.us(EE_UNITS_US_ORDER_ID, 0, i as i64, 0, null) as u32).collect();
    let (order_at, unrecorded) = h.order_check(&order_ids);
    if order_at != 0 {
        let rust_count = h.order_count();
        eprintln!(
            "EE-UNIT-STORE: the stepped units ({}) and the Rust list ({}) differ at index {}",
            order_ids.len(),
            rust_count,
            order_at - 1
        );
        check.mismatches = check.mismatches.wrapping_add(1);
    }
    if unrecorded > MOST_UNRECORDED.load(std::sync::atomic::Ordering::Relaxed) {
        // TEST ONLY: stepped units with no store record (the step's leaves read it)
        MOST_UNRECORDED.store(unrecorded, std::sync::atomic::Ordering::Relaxed);
        let tick = h.us(EE_UNITS_US_TICK, 0, 0, 0, null) as u32;
        eprintln!(
            "EE-UNIT-ORDER: {} stepped units without a store record at {} (tick {})",
            unrecorded, at as u32, tick
        );
    }
    let mut record = Record::new(USCK);
    record.u32(at as u32);
    record.u32(check.compared);
    record.u32(check.mismatches);
    h.record(&record);
    if check.mismatches != 0 {
        eprintln!(
            "EE-UNIT-STORE: {} of {} units differ from the store ({} records) at {}: first id 0x{:08x} field {}",
            check.mismatches, check.compared, check.records, at as u32, check.first_id, check.first_field
        );
        flush();
        h.us(EE_UNITS_US_EXIT, 0, 0, 0, null);
    }
    listed
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::ffi::*;
    use crate::units::players::ee_units_player_check;
    use std::ffi::c_void;

    extern "C" {
        fn ee_units_us_host(op: i32, a: u64, b: i64, c: u64, out: *mut c_void) -> i64;
    }

    struct Game;

    fn ptr<T>(v: &[T]) -> *const T {
        if v.is_empty() {
            std::ptr::null()
        } else {
            v.as_ptr()
        }
    }

    impl UsHost for Game {
        fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut c_void) -> i64 {
            // SAFETY: the C++ statement over the world's live objects (the check runs on the simulation thread)
            unsafe { ee_units_us_host(op, a, b, c, out) }
        }
        fn store_check(&mut self, objects: &[EeUnitRecord]) -> EeUnitStoreCheck {
            let mut check = EeUnitStoreCheck::default();
            // SAFETY: the slice and the out
            unsafe { ee_units_store_check(ptr(objects), objects.len(), &mut check) };
            check
        }
        fn store_unlisted(&mut self) -> Vec<u64> {
            // SAFETY: a count query, then the copy into a buffer of that size
            let count = unsafe { ee_units_store_unlisted(std::ptr::null_mut(), 0) };
            let mut v = vec![0u64; count];
            if count != 0 {
                // SAFETY: as above
                unsafe { ee_units_store_unlisted(v.as_mut_ptr(), count) };
            }
            v
        }
        fn store_check_unlisted(&mut self, objects: &[EeUnitRecord], check: &mut EeUnitStoreCheck) -> u32 {
            // SAFETY: the slice and the out
            unsafe { ee_units_store_check_unlisted(ptr(objects), objects.len(), check) }
        }
        fn store_check_on_map(&mut self, ids: &[u32], objects: &[u64]) -> u32 {
            // SAFETY: the slices (the same length)
            unsafe { ee_units_store_check_on_map(ptr(ids), ptr(objects), ids.len()) }
        }
        fn store_checksum_flags(&mut self) -> (u64, u64) {
            (ee_units_store_checksum_flag1(), ee_units_store_checksum_flag2())
        }
        fn player_check(&mut self, objects: &[u64], ids: &[u32], enemies: &[u32]) -> u32 {
            // SAFETY: the slices (17 stances a player)
            unsafe { ee_units_player_check(objects.as_ptr(), ids.as_ptr(), enemies.as_ptr(), objects.len()) }
        }
        fn order_check(&mut self, ids: &[u32]) -> (u32, u32) {
            let mut unrecorded = 0;
            // SAFETY: the slice and the out
            let at = unsafe { ee_units_order_check(ptr(ids), ids.len(), &mut unrecorded) };
            (at, unrecorded)
        }
        fn order_count(&mut self) -> u32 {
            ee_units_order_count()
        }
        fn record(&mut self, r: &Record) {
            r.commit();
        }
    }

    /// TEST ONLY: `EEUnitStoreCheck(where)`.
    #[no_mangle]
    pub extern "C" fn ee_units_us_check(at: u64) -> u64 {
        check(&mut Game, at)
    }
}

#[cfg(test)]
#[path = "us_check_tests.rs"]
mod tests;
