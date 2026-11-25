//! The unit store's C++ fills (the units' lane, round 9; were EEUnitStore.cpp): `Fill` (a unit's record as its
//! object is now), the update order's reset from the world's list and a player's table. Every C++ statement of the
//! originals is one operation of the store's host ([`UsHost::us`], `ee_units_us_host`: its `EE_UNITS_US_*` and the
//! `EE_UNITS_US_F_*` below), in the original's order.

use crate::units::kinds::{EE_OBJECT_COMPLEX_UNIT, EE_OBJECT_UNIT};
use crate::units::players::EE_UNITS_PLAYER_COUNT;
use crate::units::store::EeUnitRecord;
use crate::units::us_check::{
    UsHost, EE_UNITS_US_ORDER_COUNT, EE_UNITS_US_ORDER_ID, EE_UNITS_US_PLAYER_ID, EE_UNITS_US_PLAYER_RTS,
    EE_UNITS_US_UNIT_ID,
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

// ---- the store's entry points' statements (units round 11: EEUnitStore.cpp's bodies here). `a` is the unit or player.
/// `unit->mInUnitStore`
pub const EE_UNITS_US_B_IN_STORE: i32 = 10300;
/// `unit->mInUnitStore = b != 0`
pub const EE_UNITS_US_B_SET_IN_STORE: i32 = 10301;
/// `unit->mPlayer` (the RTSUnit member, whatever the object's kind)
pub const EE_UNITS_US_B_RAW_PLAYER: i32 = 10302;
/// `player->mEnemies[b]` as a word
pub const EE_UNITS_US_B_ENEMY: i32 = 10303;

/// The one-statement part of the store's host (`UsHost::us`).
pub trait UsOps {
    fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut std::ffi::c_void) -> i64;
}

impl<T: UsHost + ?Sized> UsOps for T {
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
    let enemies = enemy_words(h, player);
    let rts = q(h, EE_UNITS_US_PLAYER_RTS, player, 0) as u64;
    let id = q(h, EE_UNITS_US_PLAYER_ID, player, 0) as u32;
    (rts, id, enemies)
}

/// The store's writes the entry points make (ee_units ffi.rs, players.rs; a recording fake in the tests).
pub trait StoreSink {
    /// `ee_units_store_set_fields(record, rts, unit, complex)`
    fn set_fields(&mut self, record: &mut EeUnitRecord, fields: [u64; 3]);
    fn insert(&mut self, record: &EeUnitRecord);
    fn insert_unlisted(&mut self, record: &EeUnitRecord);
    fn remove(&mut self, id: u32, object: u64);
    fn set(&mut self, id: u32, object: u64, field: u32, value: i64);
    fn clear_player(&mut self, player: u64);
    fn player_drop(&mut self, player: u64);
}

/// `Fill`, then the record's object records: the record of the unit's object as it is now.
fn filled<H: UsOps + ?Sized, S: StoreSink + ?Sized>(
    h: &mut H,
    s: &mut S,
    unit: u64,
    owner: u32,
    player: u64,
) -> EeUnitRecord {
    let mut record = EeUnitRecord::default();
    let fields = fill(h, &mut record, unit, owner, player);
    s.set_fields(&mut record, fields);
    record
}

/// A player's list id and object (`thePlayer != NULL ? thePlayer->mPlayerID : 0` and its RTSPlayer; 0, 0 for none).
fn owner_of<H: UsOps + ?Sized>(h: &mut H, player: u64) -> (u32, u64) {
    if player == 0 {
        (0, 0)
    } else {
        (q(h, EE_UNITS_US_PLAYER_ID, player, 0) as u32, q(h, EE_UNITS_US_PLAYER_RTS, player, 0) as u64)
    }
}

/// `EEUnitStoreInsert(unit, owner, player)`: the unit's record into the store; the unit marks itself stored.
pub fn insert<H: UsOps + ?Sized, S: StoreSink + ?Sized>(h: &mut H, s: &mut S, unit: u64, owner: u32, player: u64) {
    let record = filled(h, s, unit, owner, player);
    s.insert(&record);
    q(h, EE_UNITS_US_B_SET_IN_STORE, unit, 1);
}

/// `EEUnitStoreInsertUnlisted(unit)`: a unit of id 0xffffffff in no store yet into the unlisted table (the owner and
/// player its own when it is a unit).
pub fn insert_unlisted<H: UsOps + ?Sized, S: StoreSink + ?Sized>(h: &mut H, s: &mut S, unit: u64) {
    if unit == 0
        || q(h, EE_UNITS_US_B_IN_STORE, unit, 0) != 0
        || q(h, EE_UNITS_US_UNIT_ID, unit, 0) as u32 != 0xffff_ffff
    {
        return;
    }
    let player = q(h, crate::units::us_check::EE_UNITS_US_UNIT_PLAYER, unit, 0) as u64;
    let (owner, rts) = owner_of(h, player);
    let record = filled(h, s, unit, owner, rts);
    s.insert_unlisted(&record);
    q(h, EE_UNITS_US_B_SET_IN_STORE, unit, 1);
}

/// `EEUnitStoreRemove(unit)`: a stored unit's record out of the store.
pub fn remove<H: UsOps + ?Sized, S: StoreSink + ?Sized>(h: &mut H, s: &mut S, unit: u64) {
    if q(h, EE_UNITS_US_B_IN_STORE, unit, 0) == 0 {
        return;
    }
    s.remove(q(h, EE_UNITS_US_UNIT_ID, unit, 0) as u32, unit);
    q(h, EE_UNITS_US_B_SET_IN_STORE, unit, 0);
}

/// `EEUnitStoreLinked(unit)`: a stored unit's on-the-map flag.
pub fn linked<H: UsOps + ?Sized, S: StoreSink + ?Sized>(h: &mut H, s: &mut S, unit: u64) {
    if q(h, EE_UNITS_US_B_IN_STORE, unit, 0) != 0 {
        let id = q(h, EE_UNITS_US_UNIT_ID, unit, 0) as u32;
        let on_map = i64::from(q(h, EE_UNITS_US_F_ON_MAP, unit, 0) != 0);
        s.set(id, unit, crate::units::store::EE_UNIT_FIELD_ON_MAP, on_map);
    }
}

/// `ee_combat_host_unit_record(unit)`: a unit's record from its live object (its `mPlayer` as the list's player).
pub fn object_record<H: UsOps + ?Sized, S: StoreSink + ?Sized>(h: &mut H, s: &mut S, unit: u64) -> EeUnitRecord {
    let player = q(h, EE_UNITS_US_B_RAW_PLAYER, unit, 0) as u64;
    let (owner, rts) = owner_of(h, player);
    filled(h, s, unit, owner, rts)
}

/// A player's enemy words (`PlayerWords`: `mEnemies[i]` for each player slot).
pub fn enemy_words<H: UsOps + ?Sized>(h: &mut H, player: u64) -> [u32; EE_UNITS_PLAYER_COUNT] {
    let mut words = [0u32; EE_UNITS_PLAYER_COUNT];
    for (i, w) in words.iter_mut().enumerate() {
        *w = q(h, EE_UNITS_US_B_ENEMY, player, i as i64) as u32;
    }
    words
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
        unsafe {
            crate::units::ffi::ee_units_store_set_fields(record, rts as usize as _, u as usize as _, c as usize as _)
        };
    }

    /// The game's store (ee_units ffi.rs, players.rs).
    struct Store;

    impl StoreSink for Store {
        fn set_fields(&mut self, record: &mut EeUnitRecord, f: [u64; 3]) {
            // SAFETY: the unit's live base subobjects (or null)
            unsafe {
                crate::units::ffi::ee_units_store_set_fields(
                    record,
                    f[0] as usize as _,
                    f[1] as usize as _,
                    f[2] as usize as _,
                )
            };
        }
        fn insert(&mut self, record: &EeUnitRecord) {
            // SAFETY: a valid record
            unsafe { crate::units::ffi::ee_units_store_insert(record) };
        }
        fn insert_unlisted(&mut self, record: &EeUnitRecord) {
            // SAFETY: a valid record
            unsafe { crate::units::ffi::ee_units_store_insert_unlisted(record) };
        }
        fn remove(&mut self, id: u32, object: u64) {
            crate::units::ffi::ee_units_store_remove(id, object);
        }
        fn set(&mut self, id: u32, object: u64, field: u32, value: i64) {
            crate::units::ffi::ee_units_store_set(id, object, field, value);
        }
        fn clear_player(&mut self, player: u64) {
            crate::units::ffi::ee_units_store_clear_player(player);
        }
        fn player_drop(&mut self, player: u64) {
            crate::units::players::ee_units_player_drop(player);
        }
    }

    /// `EEUnitStoreInsert(unit, owner, player)` ([`insert`]).
    #[no_mangle]
    pub extern "C" fn ee_units_store_insert_object(unit: u64, owner: u32, player: u64) {
        insert(&mut Game, &mut Store, unit, owner, player);
    }

    /// `EEUnitStoreInsertUnlisted(unit)` ([`insert_unlisted`]).
    #[no_mangle]
    pub extern "C" fn ee_units_store_insert_unlisted_object(unit: u64) {
        insert_unlisted(&mut Game, &mut Store, unit);
    }

    /// `EEUnitStoreRemove(unit)` ([`remove`]).
    #[no_mangle]
    pub extern "C" fn ee_units_store_remove_object(unit: u64) {
        remove(&mut Game, &mut Store, unit);
    }

    /// `EEUnitStoreLinked(unit)` ([`linked`]).
    #[no_mangle]
    pub extern "C" fn ee_units_store_linked(unit: u64) {
        linked(&mut Game, &mut Store, unit);
    }

    /// `EEUnitStoreClearPlayer(player)`: its list's records and its diplomacy table out of the store.
    #[no_mangle]
    pub extern "C" fn ee_units_store_clear_player_object(player: u64) {
        Store.clear_player(player);
        Store.player_drop(player);
    }

    /// The combat reads' fill of a unit out of every list ([`object_record`]).
    ///
    /// # Safety
    /// `out` is writable, `object` a live unit.
    #[no_mangle]
    pub unsafe extern "C" fn ee_combat_host_unit_record(object: u64, out: *mut EeUnitRecord) {
        let record = object_record(&mut Game, &mut Store, object);
        // SAFETY: the caller's contract
        unsafe { *out = record };
    }

    /// The combat reads' table of a player without one: its id and enemy words.
    ///
    /// # Safety
    /// `id` and `enemies` (the player count) are writable, `object` a live `EEPlayer`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_combat_host_player_table(object: u64, id: *mut u32, enemies: *mut u32) {
        let words = enemy_words(&mut Game, object);
        // SAFETY: the caller's contract
        unsafe {
            *id = q(&mut Game, EE_UNITS_US_PLAYER_ID, object, 0) as u32;
            std::ptr::copy_nonoverlapping(words.as_ptr(), enemies, words.len());
        }
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
        stored: i64,
    }

    impl UsOps for Rec {
        fn us(&mut self, op: i32, a: u64, b: i64, _c: u64, _out: *mut std::ffi::c_void) -> i64 {
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
                EE_UNITS_US_B_ENEMY => i64::from(b == 3),
                EE_UNITS_US_B_IN_STORE => self.stored,
                EE_UNITS_US_B_RAW_PLAYER | crate::units::us_check::EE_UNITS_US_UNIT_PLAYER => 0x60,
                _ => 0,
            }
        }
    }

    #[test]
    fn a_complex_units_record_has_its_stats_location_and_three_subobjects() {
        let mut r = Rec { log: vec![], kind: i64::from(EE_OBJECT_COMPLEX_UNIT), stored: 0 };
        let mut rec = EeUnitRecord::default();
        let f = fill(&mut r, &mut rec, 0x5000, 1, 0x80);
        assert_eq!((rec.id, rec.owner, rec.object, rec.player, rec.on_map, rec.stats), (42, 1, 0x5000, 0x80, 1, 0x700));
        assert_eq!(rec.location, [0x4000_0000, 0x4000_0001, 0x4000_0002]);
        assert_eq!(f, [0x900, 0x901, 0x902]);
    }

    #[test]
    fn a_plain_objects_record_has_no_stats_location_or_unit_subobjects() {
        let mut r = Rec { log: vec![], kind: 0, stored: 0 };
        let mut rec = EeUnitRecord::default();
        let f = fill(&mut r, &mut rec, 0x5000, 0, 0);
        assert_eq!((rec.stats, rec.location), (0, [0; 3]));
        assert_eq!(f, [0x900, 0, 0]);
        assert!(!r.log.iter().any(|e| e.0 == EE_UNITS_US_F_STATS || e.0 == EE_UNITS_US_F_LOCATION));
    }

    #[test]
    fn the_order_and_the_player_table() {
        let mut r = Rec { log: vec![], kind: 0, stored: 0 };
        assert_eq!(order_ids(&mut r), [10, 11, 12]);
        let (rts, id, enemies) = player_table(&mut r, 0x60);
        assert_eq!((rts, id, enemies[3], enemies[0]), (0x80, 2, 1, 0));
    }

    /// The store's writes, logged.
    #[derive(Default)]
    struct Sink {
        log: Vec<String>,
    }

    impl StoreSink for Sink {
        fn set_fields(&mut self, record: &mut EeUnitRecord, f: [u64; 3]) {
            self.log.push(format!("fields {:x} {:x} {:x}", f[0], f[1], f[2]));
            record.category = 7;
        }
        fn insert(&mut self, r: &EeUnitRecord) {
            self.log.push(format!("insert {} {} {:x} {}", r.id, r.owner, r.player, r.category));
        }
        fn insert_unlisted(&mut self, r: &EeUnitRecord) {
            self.log.push(format!("unlisted {} {} {:x}", r.id, r.owner, r.player));
        }
        fn remove(&mut self, id: u32, object: u64) {
            self.log.push(format!("remove {id} {object:x}"));
        }
        fn set(&mut self, id: u32, object: u64, field: u32, value: i64) {
            self.log.push(format!("set {id} {object:x} {field} {value}"));
        }
        fn clear_player(&mut self, player: u64) {
            self.log.push(format!("clear {player:x}"));
        }
        fn player_drop(&mut self, player: u64) {
            self.log.push(format!("drop {player:x}"));
        }
    }

    #[test]
    fn insert_fills_the_record_then_marks_the_unit_stored() {
        let (mut r, mut s) = (Rec { log: vec![], kind: 0, stored: 0 }, Sink::default());
        insert(&mut r, &mut s, 0x5000, 3, 0x80);
        assert_eq!(s.log, ["fields 900 0 0", "insert 42 3 80 7"]);
        assert_eq!(r.log.last(), Some(&(EE_UNITS_US_B_SET_IN_STORE, 0x5000, 1)));
    }

    #[test]
    fn remove_and_linked_act_only_on_a_stored_unit() {
        let (mut r, mut s) = (Rec { log: vec![], kind: 0, stored: 0 }, Sink::default());
        remove(&mut r, &mut s, 0x5000);
        linked(&mut r, &mut s, 0x5000);
        assert!(s.log.is_empty());
        r.stored = 1;
        linked(&mut r, &mut s, 0x5000);
        remove(&mut r, &mut s, 0x5000);
        let on_map = crate::units::store::EE_UNIT_FIELD_ON_MAP;
        assert_eq!(s.log, [format!("set 42 5000 {on_map} 1"), "remove 42 5000".to_string()]);
        assert_eq!(r.log.last(), Some(&(EE_UNITS_US_B_SET_IN_STORE, 0x5000, 0)));
    }

    #[test]
    fn an_unlisted_unit_needs_the_none_id_and_no_store_and_takes_its_owner() {
        let (mut r, mut s) = (Rec { log: vec![], kind: 0, stored: 0 }, Sink::default());
        insert_unlisted(&mut r, &mut s, 0x5000);
        assert!(s.log.is_empty(), "id 42 is not 0xffffffff");
        insert_unlisted(&mut r, &mut s, 0);
        assert!(s.log.is_empty());
        let rec = object_record(&mut r, &mut s, 0x5000);
        assert_eq!((rec.owner, rec.player, rec.category), (2, 0x80, 7));
        assert_eq!(enemy_words(&mut r, 0x60)[3], 1);
    }
}
