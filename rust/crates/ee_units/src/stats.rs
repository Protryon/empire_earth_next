//! The units' stats as a Rust table (RUST_PLAN R5b.4, the fourteenth round): one record per stats object (an
//! `EEUnitClassUpgrade`, a player's upgrade of a unit class: `mStats` of every unit of that class and player), keyed
//! by the object's address and versioned. The C++ reports an object whenever its fields may have changed (its `Init`,
//! the improvements' `ChangeLevel`, the civilization builder's and the scenario events' writes, `SetMaxVelocity`,
//! `SetRange`, the media's death sound: `EEUnitStatsScope` at each writer), and drops it in its destructor; the
//! version counts the reports that changed the record, so a reader can keep a copy and know when it is stale. A unit's
//! record names its stats object (`EeUnitRecord::stats`).
//!
//! The combat lane's reads (the trigger fill's range, reload time, fire over walls, flight time, attack and max hit
//! points; the reads' scale, max velocity) and this crate's (regeneration, power, gather times, the death sound) are
//! here. The capture's USCK check compares every unit's stats object with its record.

use std::collections::BTreeMap;
use std::sync::Mutex;

/// One stats object's fields (floats as their bits), as the C++ fills them (`EEUnitStatsFill`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitStatsRecord {
    /// the `EEUnitClassUpgrade*`'s address (the key)
    pub object: u64,
    /// the reports that changed this record (1: the first); set by the table, not compared
    pub version: u32,
    /// `mUnitClass->mUnitType` (-1: no class)
    pub unit_type: i64,
    /// `mScale`, `mRange`, `mAreaEffect`, `mLineOfSight`, `mAcceleration`, `mMaxVelocity`, `mMaxAngularVelocity`,
    /// `mAttack`, `mLeadership` (float bits)
    pub scale: u32,
    pub range: u32,
    pub area_effect: u32,
    pub line_of_sight: u32,
    pub acceleration: u32,
    pub max_velocity: u32,
    pub max_angular_velocity: u32,
    pub attack: u32,
    pub leadership: u32,
    /// `mArmor[6]` (float bits)
    pub armor: [u32; 6],
    /// `mGatherTimes[7]`
    pub gather_times: [i64; 7],
    /// `mPowerRegeneration`, `mFlightTime`, `mWorkRate`, `mReloadTime`, `mMaxHitPoints`, `mPower`, `mHealingPoints`,
    /// `mRegeneration` (`long`s)
    pub power_regeneration: i64,
    pub flight_time: i64,
    pub work_rate: i64,
    pub reload_time: i64,
    pub max_hit_points: i64,
    pub power: i64,
    pub healing_points: i64,
    pub regeneration: i64,
    /// `mCanFireOverWalls`, `mConvertsBuildings`, `mConvertsPriests`, `mDeathSound != NULL`
    pub can_fire_over_walls: u8,
    pub converts_buildings: u8,
    pub converts_priests: u8,
    pub death_sound: u8,
}

impl EeUnitStatsRecord {
    /// The fields equal (the key and the version not compared).
    pub fn same_fields(&self, other: &EeUnitStatsRecord) -> bool {
        let mut a = *self;
        let mut b = *other;
        a.version = 0;
        b.version = 0;
        a.object = 0;
        b.object = 0;
        a == b
    }
}

/// The stats records by object.
#[derive(Default)]
pub struct StatsTable {
    records: BTreeMap<u64, EeUnitStatsRecord>,
}

impl StatsTable {
    pub const fn new() -> Self {
        StatsTable { records: BTreeMap::new() }
    }

    /// A report: the record replaces the object's; the version counts the reports that changed it. Returns the
    /// version.
    pub fn report(&mut self, mut record: EeUnitStatsRecord) -> u32 {
        record.version = match self.records.get(&record.object) {
            Some(old) if old.same_fields(&record) => old.version,
            Some(old) => old.version.wrapping_add(1),
            None => 1,
        };
        self.records.insert(record.object, record);
        record.version
    }

    /// The object goes (its destructor). True when it had a record.
    pub fn remove(&mut self, object: u64) -> bool {
        self.records.remove(&object).is_some()
    }

    pub fn get(&self, object: u64) -> Option<&EeUnitStatsRecord> {
        self.records.get(&object)
    }

    pub fn len(&self) -> usize {
        self.records.len()
    }

    pub fn is_empty(&self) -> bool {
        self.records.is_empty()
    }

    pub fn clear(&mut self) {
        self.records.clear();
    }

    /// TEST ONLY: the objects (filled by the C++ from every unit's `mStats`) whose record is missing or differs.
    pub fn check(&self, objects: &[EeUnitStatsRecord]) -> u32 {
        objects.iter().filter(|o| self.records.get(&o.object).is_none_or(|r| !r.same_fields(o))).count() as u32
    }
}

/// The process's table (written under the world's lock, like the store).
pub static STATS: Mutex<StatsTable> = Mutex::new(StatsTable::new());

/// Runs `f` over the process's table (a poisoned lock is taken over: plain values only).
pub fn with_stats<R>(f: impl FnOnce(&mut StatsTable) -> R) -> R {
    let mut guard = STATS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

/// The stats record of a stats object (a unit record's `stats`), when the table has it.
pub fn stats_of(object: u64) -> Option<EeUnitStatsRecord> {
    if object == 0 {
        return None;
    }
    with_stats(|t| t.get(object).copied())
}

/// A stats object's report (its fields may have changed). Returns its version.
///
/// # Safety
/// `record` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_report(record: *const EeUnitStatsRecord) -> u32 {
    // SAFETY: the caller's contract
    let record = unsafe { *record };
    with_stats(|t| t.report(record))
}

/// A stats object's destructor.
#[no_mangle]
pub extern "C" fn ee_units_stats_remove(object: u64) {
    with_stats(|t| t.remove(object));
}

/// Copies a stats object's record into `out`; false (and `out` untouched) when the table has none.
///
/// # Safety
/// `out` points to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_get(object: u64, out: *mut EeUnitStatsRecord) -> bool {
    match stats_of(object) {
        Some(r) => {
            // SAFETY: the caller's contract
            unsafe { *out = r };
            true
        }
        None => false,
    }
}

/// TEST ONLY: the objects of `objects` (`count` records filled from the units' stats) whose record is missing or
/// differs.
///
/// # Safety
/// `objects` points to `count` records (or is null with `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_check(objects: *const EeUnitStatsRecord, count: usize) -> u32 {
    let objects = if count == 0 || objects.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(objects, count) }
    };
    with_stats(|t| t.check(objects))
}

/// The table's records (TEST ONLY: the teardown's leak check).
#[no_mangle]
pub extern "C" fn ee_units_stats_count() -> u32 {
    with_stats(|t| t.len() as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rec(object: u64, range: f32) -> EeUnitStatsRecord {
        EeUnitStatsRecord {
            object,
            unit_type: 7,
            range: range.to_bits(),
            reload_time: 2000,
            max_hit_points: 120,
            ..Default::default()
        }
    }

    #[test]
    fn the_version_counts_the_changes() {
        let mut t = StatsTable::new();
        assert_eq!(t.report(rec(0x1000, 4.0)), 1);
        // the same fields again (another writer that changed nothing): the same version
        assert_eq!(t.report(rec(0x1000, 4.0)), 1);
        // an upgrade
        assert_eq!(t.report(rec(0x1000, 5.0)), 2);
        assert_eq!(f32::from_bits(t.get(0x1000).unwrap().range), 5.0);
        // another object starts at 1
        assert_eq!(t.report(rec(0x2000, 5.0)), 1);
        assert_eq!(t.len(), 2);
        assert!(t.remove(0x1000));
        assert!(!t.remove(0x1000));
        assert!(t.get(0x1000).is_none());
        // the key and the version are not fields
        let mut a = rec(1, 1.0);
        let mut b = rec(2, 1.0);
        a.version = 3;
        b.version = 9;
        assert!(a.same_fields(&b));
        b.armor[5] = 1;
        assert!(!a.same_fields(&b));
    }

    #[test]
    fn check_counts_missing_and_stale() {
        let mut t = StatsTable::new();
        t.report(rec(0x10, 1.0));
        t.report(rec(0x20, 2.0));
        assert_eq!(t.check(&[rec(0x10, 1.0), rec(0x20, 2.0)]), 0);
        // a write no report followed (stale) and an object never reported (missing)
        assert_eq!(t.check(&[rec(0x10, 1.5), rec(0x20, 2.0), rec(0x30, 1.0)]), 2);
        // a copy taken before an upgrade knows it is stale by the version
        let copy = *t.get(0x20).unwrap();
        let mut up = rec(0x20, 2.0);
        up.attack = 3.0f32.to_bits();
        t.report(up);
        assert_ne!(t.get(0x20).unwrap().version, copy.version);
    }

    #[test]
    fn the_process_table_is_reachable() {
        let r =
            EeUnitStatsRecord { object: 0xfeed_0000_0001, flight_time: 30_000, death_sound: 1, ..Default::default() };
        // SAFETY: valid pointers to locals
        unsafe {
            assert_eq!(ee_units_stats_report(&r), 1);
            let mut out = EeUnitStatsRecord::default();
            assert!(ee_units_stats_get(r.object, &mut out));
            assert_eq!((out.flight_time, out.death_sound, out.version), (30_000, 1, 1));
            assert_eq!(ee_units_stats_check(&r, 1), 0);
        }
        assert_eq!(stats_of(r.object).map(|s| s.flight_time), Some(30_000));
        assert_eq!(stats_of(0), None);
        ee_units_stats_remove(r.object);
        assert_eq!(stats_of(r.object), None);
    }
}
