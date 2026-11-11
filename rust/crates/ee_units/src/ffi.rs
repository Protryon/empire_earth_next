//! The C ABI behind `RTSRegisterUnit` / `RTSUnregisterUnit` / `RTSResolveUnitID` (`src/RTS Engine/Units/RTSUnit.cpp`).
//! One process-wide registry; the C++ keeps the "no id" value (0xffffffff) out of it.
//! And the unit store's (`include/ee/EEUnitStore.h`, `src/Empire Earth/Units/EEUnitStore.cpp`; see `store`).

use crate::store::{with_store, EeUnitRecord, EeUnitStoreCheck};
use crate::Registry;
use std::ffi::c_void;

static REGISTRY: Registry<c_void> = Registry::new();

/// Makes `id` resolve to `unit` (`RTSPlayer::InsertUnit`).
#[no_mangle]
pub extern "C" fn ee_units_register(id: u32, unit: *mut c_void) {
    REGISTRY.register(id, unit);
}

/// Clears `id`'s entry if it still names `unit` (`RTSUnit::FreeNode`); returns whether it did.
#[no_mangle]
pub extern "C" fn ee_units_unregister(id: u32, unit: *mut c_void) -> bool {
    REGISTRY.unregister(id, unit)
}

/// The object registered under `id`, or null (`EEUnitHandle<T>::Get`).
#[no_mangle]
pub extern "C" fn ee_units_resolve(id: u32) -> *mut c_void {
    REGISTRY.resolve(id)
}

/// A unit enters a player's list: its record (replacing any under its id).
///
/// # Safety
/// `record` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_insert(record: *const EeUnitRecord) {
    // SAFETY: the caller's contract
    let record = unsafe { *record };
    with_store(|s| s.insert(record));
}

/// A unit leaves its player's list (or is freed): the record of that object, if it is the one under the id.
#[no_mangle]
pub extern "C" fn ee_units_store_remove(id: u32, object: u64) -> bool {
    with_store(|s| s.remove(id, object))
}

/// A player object goes: the records of its list. The count removed.
#[no_mangle]
pub extern "C" fn ee_units_store_clear_player(player: u64) -> u32 {
    with_store(|s| s.clear_player(player)) as u32
}

/// A holder's write (`EE_UNIT_FIELD_*`); ignored unless the record under `id` is `object`'s.
#[no_mangle]
pub extern "C" fn ee_units_store_set(id: u32, object: u64, field: u32, value: i64) -> bool {
    with_store(|s| s.set(id, object, field, value))
}

/// The record under `id` into `out`; false (and `out` untouched) when there is none.
///
/// # Safety
/// `out` points to writable memory for one record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_get(id: u32, out: *mut EeUnitRecord) -> bool {
    match with_store(|s| s.get(id).copied()) {
        Some(r) => {
            // SAFETY: the caller's contract
            unsafe { *out = r };
            true
        }
        None => false,
    }
}

/// The number of records.
#[no_mangle]
pub extern "C" fn ee_units_store_count() -> u32 {
    with_store(|s| s.len()) as u32
}

/// TEST ONLY: the objects' fields (every unit of every player's list) against the records.
///
/// # Safety
/// `objects` points to `count` records (or is null with `count` 0); `out` to writable memory for the result.
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_check(objects: *const EeUnitRecord, count: usize, out: *mut EeUnitStoreCheck) {
    let objects = if count == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(objects, count) }
    };
    let result = with_store(|s| s.check(objects));
    // SAFETY: the caller's contract
    unsafe { *out = result };
}

/// The box selection's pick over the store (`EEPlayer::SelectUnits`; `select::select_pick`): the picked ids of
/// `ids` into `out` (room for `count`: a pick never has more), their number.
///
/// # Safety
/// `ids` points to `count` ids and `out` to room for `count` (either may be null when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_select_pick(player: u64, ids: *const u32, count: usize, out: *mut u32) -> usize {
    if count == 0 {
        return 0;
    }
    // SAFETY: the caller's contract
    let ids = unsafe { std::slice::from_raw_parts(ids, count) };
    let mut picked = Vec::with_capacity(count);
    with_store(|s| crate::select::select_pick(s, player, ids, &mut picked));
    // SAFETY: the caller's contract; a pick holds at most one id per input id
    unsafe { std::ptr::copy_nonoverlapping(picked.as_ptr(), out, picked.len()) };
    picked.len()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_process_registry_round_trips() {
        // ids no other test of this crate uses (the static is shared by the test binary's threads)
        let id = 0xfe00_0042;
        let unit = 0x1000usize as *mut c_void;
        assert!(ee_units_resolve(id).is_null());
        ee_units_register(id, unit);
        assert_eq!(ee_units_resolve(id), unit);
        assert!(!ee_units_unregister(id, 0x2000usize as *mut c_void));
        assert!(ee_units_unregister(id, unit));
        assert!(ee_units_resolve(id).is_null());
    }

    #[test]
    fn the_store_round_trips_through_the_abi() {
        // ids and a player no other test of this crate uses
        let rec = EeUnitRecord {
            id: 0xfc00_0007,
            owner: 0xfc,
            object: 0x4000,
            player: 0xfc00,
            goal_kind: -1,
            ..Default::default()
        };
        unsafe { ee_units_store_insert(&rec) };
        assert!(ee_units_store_set(rec.id, 0x4000, crate::store::EE_UNIT_FIELD_HIT_POINTS, 55));
        let mut out = EeUnitRecord::default();
        assert!(unsafe { ee_units_store_get(rec.id, &mut out) });
        assert_eq!(out.hit_points, 55);
        assert!(ee_units_store_count() >= 1);
        assert!(!ee_units_store_remove(rec.id, 0x4001));
        assert!(ee_units_store_remove(rec.id, 0x4000));
        assert!(!unsafe { ee_units_store_get(rec.id, &mut out) });
        unsafe { ee_units_store_insert(&rec) };
        assert_eq!(ee_units_store_clear_player(0xfc00), 1);
    }

    #[test]
    fn the_pick_reads_the_process_store() {
        // ids and a player no other test of this crate uses
        let rec = |id: u32| EeUnitRecord {
            id,
            owner: 0xfb,
            object: 0x7000 + id as u64,
            player: 0xfb00,
            object_kind: crate::kinds::EE_OBJECT_COMPLEX_UNIT,
            complex: crate::store::EE_UNIT_COMPLEX_SELF,
            valid: 1,
            built: 1,
            goal_kind: -1,
            ..Default::default()
        };
        for id in [0xfb00_0001, 0xfb00_0002] {
            unsafe { ee_units_store_insert(&rec(id)) };
        }
        let ids = [0xfb00_0002u32, 0xfb00_0009, 0xfb00_0001, 0xfb00_0002];
        let mut out = [0u32; 4];
        let n = unsafe { ee_units_select_pick(0xfb00, ids.as_ptr(), ids.len(), out.as_mut_ptr()) };
        assert_eq!(&out[..n], &[0xfb00_0002, 0xfb00_0001]);
        assert_eq!(unsafe { ee_units_select_pick(0xfb00, std::ptr::null(), 0, std::ptr::null_mut()) }, 0);
        assert_eq!(ee_units_store_clear_player(0xfb00), 2);
    }
}
