//! The unit store's C++ fills (the units' lane, round 9; were EEUnitStore.cpp): `Fill` (a unit's record as its
//! object is now), the update order's reset from the world's list and a player's table. Every C++ statement of the
//! originals is one operation of the store's host ([`UsHost::us`], `ee_units_us_host`: its `EE_UNITS_US_*` and the
//! `EE_UNITS_US_F_*` below), in the original's order.

use crate::units::kinds::{EE_OBJECT_COMPLEX_UNIT, EE_OBJECT_UNIT};
use crate::units::players::EE_UNITS_PLAYER_COUNT;
use crate::units::store::EeUnitRecord;
use crate::units::us_check::{
    UsHost, EE_UNITS_US_ORDER_COUNT, EE_UNITS_US_ORDER_ID, EE_UNITS_US_PLAYER_ID, EE_UNITS_US_PLAYER_RTS,
    EE_UNITS_US_PLAYER_WORDS, EE_UNITS_US_UNIT_ID,
};

// ---- the statements (`op`; the answer). `a` is the unit.
/// `mObjectKind.mValue`
pub const EE_UNITS_US_F_OBJECT_KIND: i32 = 4000;
/// `OnMap(unit)` (in the world's unit chain)
pub const EE_UNITS_US_F_ON_MAP: i32 = 4001;
/// `static_cast<EEUnit*>(unit)->mStats`
pub const EE_UNITS_US_F_STATS: i32 = 4002;
/// the complex unit's `mPhysicalModel.GetLocation()`'s x, y, z (`b` 0, 1, 2) as float bits
pub const EE_UNITS_US_F_LOCATION: i32 = 4003;
/// the unit's `EeUnitsRtsUnitFields`, `EeUnitsUnitFields`, `EeUnitsComplexUnitFields` base subobjects (`b` 0, 1, 2)
pub const EE_UNITS_US_F_FIELDS: i32 = 4004;

/// The one-statement part of the store's host (`UsHost::us`).
pub trait UsOps {
    fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut std::ffi::c_void) -> i64;
}

impl<T: UsHost> UsOps for T {
    fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut std::ffi::c_void) -> i64 {
        UsHost::us(self, op, a, b, c, out)
    }
}

fn q<H: UsOps + ?Sized>(h: &mut H, op: i32, a: u64, b: i64) -> i64 {
    h.us(op, a, b, 0, std::ptr::null_mut())
}

/// `Fill(unit, owner, player, record)`: `record` (zeroed by the caller) filled as the unit's object is now; the base
/// subobjects whose records the store reads next (`ee_units_store_set_fields`: 0 for a lesser object's).
pub fn fill<H: UsOps + ?Sized>(h: &mut H, record: &mut EeUnitRecord, unit: u64, owner: u32, player: u64) -> [u64; 3] {
    record.id = q(h, EE_UNITS_US_UNIT_ID, unit, 0) as u32;
    record.owner = owner;
    record.object = unit;
    record.player = player;
    record.object_kind = q(h, EE_UNITS_US_F_OBJECT_KIND, unit, 0) as u8;
    record.on_map = u8::from(q(h, EE_UNITS_US_F_ON_MAP, unit, 0) != 0);
    if record.object_kind >= EE_OBJECT_UNIT {
        record.stats = q(h, EE_UNITS_US_F_STATS, unit, 0) as u64;
    }
    if record.object_kind == EE_OBJECT_COMPLEX_UNIT {
        for i in 0..3 {
            record.location[i] = q(h, EE_UNITS_US_F_LOCATION, unit, i as i64) as u32;
        }
    }
    let unit_fields = if record.object_kind >= EE_OBJECT_UNIT { q(h, EE_UNITS_US_F_FIELDS, unit, 1) as u64 } else { 0 };
    let complex_fields =
        if record.object_kind == EE_OBJECT_COMPLEX_UNIT { q(h, EE_UNITS_US_F_FIELDS, unit, 2) as u64 } else { 0 };
    [q(h, EE_UNITS_US_F_FIELDS, unit, 0) as u64, unit_fields, complex_fields]
}

/// `EEUnitOrderReset()`'s ids: the world's update list in its order.
pub fn order_ids<H: UsOps + ?Sized>(h: &mut H) -> Vec<u32> {
    let count = q(h, EE_UNITS_US_ORDER_COUNT, 0, 0);
    (0..count).map(|i| q(h, EE_UNITS_US_ORDER_ID, 0, i) as u32).collect()
}

/// `EEUnitStorePlayerTable(player)`'s table: the player's list object, its id and its enemy words.
pub fn player_table<H: UsOps + ?Sized>(h: &mut H, player: u64) -> (u64, u32, [u32; EE_UNITS_PLAYER_COUNT]) {
    let mut enemies = [0u32; EE_UNITS_PLAYER_COUNT];
    h.us(EE_UNITS_US_PLAYER_WORDS, player, 0, 0, enemies.as_mut_ptr().cast());
    let rts = q(h, EE_UNITS_US_PLAYER_RTS, player, 0) as u64;
    let id = q(h, EE_UNITS_US_PLAYER_ID, player, 0) as u32;
    (rts, id, enemies)
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::us_check::game::Game;

    /// `Fill(unit, owner, player, *record)`: the record of a unit as its object is now.
    ///
    /// # Safety
    /// `record` is writable, `unit` a live `RTSUnit`, `player` the list's player object (or 0).
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_store_fill(record: *mut EeUnitRecord, unit: u64, owner: u32, player: u64) {
        // SAFETY: the caller's contract (the C++ memset: padding too)
        unsafe { std::ptr::write_bytes(record, 0, 1) };
        // SAFETY: as above
        let r = unsafe { &mut *record };
        let [rts, u, c] = fill(&mut Game, r, unit, owner, player);
        // SAFETY: the unit's live base subobjects (or null)
        unsafe { crate::units::ffi::ee_units_store_set_fields(record, rts as usize as _, u as usize as _, c as usize as _) };
    }

    /// `EEUnitOrderReset()`: the update order from the world's list.
    #[no_mangle]
    pub extern "C" fn ee_units_order_reset_world() {
        let ids = order_ids(&mut Game);
        // SAFETY: a live slice
        unsafe { crate::units::ffi::ee_units_order_reset(ids.as_ptr(), ids.len()) };
    }

    /// `EEUnitStorePlayerTable(player)`: the player's table in the store.
    ///
    /// # Safety
    /// `player` is a live `EEPlayer`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_store_player_table(player: u64) {
        let (rts, id, enemies) = player_table(&mut Game, player);
        // SAFETY: a live table of the player count
        unsafe { crate::units::players::ee_units_player_set(rts, id, enemies.as_ptr()) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Rec {
        log: Vec<(i32, u64, i64)>,
        kind: i64,
    }

    impl UsOps for Rec {
        fn us(&mut self, op: i32, a: u64, b: i64, _c: u64, out: *mut std::ffi::c_void) -> i64 {
            self.log.push((op, a, b));
            match op {
                EE_UNITS_US_F_OBJECT_KIND => self.kind,
                EE_UNITS_US_UNIT_ID => 42,
                EE_UNITS_US_F_ON_MAP => 1,
                EE_UNITS_US_F_STATS => 0x700,
                EE_UNITS_US_F_LOCATION => 0x4000_0000 + b,
                EE_UNITS_US_F_FIELDS => 0x900 + b,
                EE_UNITS_US_ORDER_COUNT => 3,
                EE_UNITS_US_ORDER_ID => 10 + b,
                EE_UNITS_US_PLAYER_RTS => 0x80,
                EE_UNITS_US_PLAYER_ID => 2,
                EE_UNITS_US_PLAYER_WORDS => {
                    // SAFETY: the test's buffer of the player count
                    unsafe { *out.cast::<u32>().add(3) = 1 };
                    0
                }
                _ => 0,
            }
        }
    }

    #[test]
    fn a_complex_units_record_has_its_stats_location_and_three_subobjects() {
        let mut r = Rec { log: vec![], kind: i64::from(EE_OBJECT_COMPLEX_UNIT) };
        let mut rec = EeUnitRecord::default();
        let f = fill(&mut r, &mut rec, 0x5000, 1, 0x80);
        assert_eq!((rec.id, rec.owner, rec.object, rec.player, rec.on_map, rec.stats), (42, 1, 0x5000, 0x80, 1, 0x700));
        assert_eq!(rec.location, [0x4000_0000, 0x4000_0001, 0x4000_0002]);
        assert_eq!(f, [0x900, 0x901, 0x902]);
    }

    #[test]
    fn a_plain_objects_record_has_no_stats_location_or_unit_subobjects() {
        let mut r = Rec { log: vec![], kind: 0 };
        let mut rec = EeUnitRecord::default();
        let f = fill(&mut r, &mut rec, 0x5000, 0, 0);
        assert_eq!((rec.stats, rec.location), (0, [0; 3]));
        assert_eq!(f, [0x900, 0, 0]);
        assert!(!r.log.iter().any(|e| e.0 == EE_UNITS_US_F_STATS || e.0 == EE_UNITS_US_F_LOCATION));
    }

    #[test]
    fn the_order_and_the_player_table() {
        let mut r = Rec { log: vec![], kind: 0 };
        assert_eq!(order_ids(&mut r), [10, 11, 12]);
        let (rts, id, enemies) = player_table(&mut r, 0x60);
        assert_eq!((rts, id, enemies[3], enemies[0]), (0x80, 2, 1, 0));
    }
}
