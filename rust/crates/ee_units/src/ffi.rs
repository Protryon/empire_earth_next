//! The C ABI behind `RTSRegisterUnit` / `RTSUnregisterUnit` / `RTSResolveUnitID` (`src/RTS Engine/Units/RTSUnit.cpp`).
//! One process-wide registry; the C++ keeps the "no id" value (0xffffffff) out of it.
//! And the unit store's (`include/ee/EEUnitStore.h`, `src/Empire Earth/Units/EEUnitStore.cpp`; see `store`).

use crate::order::with_order;
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

/// The location's write (`EEUnitStoreModel`'s setters): x, y and z as float bits, in one call; ignored unless the
/// record under `id` is `object`'s.
#[no_mangle]
pub extern "C" fn ee_units_store_set_location(id: u32, object: u64, x: u32, y: u32, z: u32) -> bool {
    with_store(|s| {
        s.set(id, object, crate::store::EE_UNIT_FIELD_LOCATION_X, i64::from(x))
            && s.set(id, object, crate::store::EE_UNIT_FIELD_LOCATION_Y, i64::from(y))
            && s.set(id, object, crate::store::EE_UNIT_FIELD_LOCATION_Z, i64::from(z))
    })
}

/// The out-of-sync checksum's flag 2 over the store's units on the map (`Store::checksum_flag2`).
#[no_mangle]
pub extern "C" fn ee_units_store_checksum_flag2() -> u64 {
    with_store(|s| s.checksum_flag2())
}

/// `EEWorld::mUpdateUnits.push_back` (`order`).
#[no_mangle]
pub extern "C" fn ee_units_order_push(id: u32) {
    with_order(|o| o.push(id));
}

/// `EEWorld::mUpdateUnits.erase` of the unit `id` (its first entry); false when the list has none.
#[no_mangle]
pub extern "C" fn ee_units_order_erase(id: u32) -> bool {
    with_order(|o| o.erase(id))
}

/// `EEWorld::mUpdateUnits.clear()`.
#[no_mangle]
pub extern "C" fn ee_units_order_clear() {
    with_order(|o| o.clear());
}

/// The number of stepped units listed.
#[no_mangle]
pub extern "C" fn ee_units_order_count() -> u32 {
    with_order(|o| o.len()) as u32
}

/// The stepped list replaced by `count` ids in order (a load's reorder).
///
/// # Safety
/// `ids` points to `count` ids (or is null with `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_order_reset(ids: *const u32, count: usize) {
    // SAFETY: the caller's contract
    let ids = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(ids, count) } };
    with_order(|o| o.reset(ids));
}

/// TEST ONLY: the deque's `count` ids (in its order) against the list (`UpdateOrder::check`: 0 the same, else the
/// first differing index plus 1); `out_unrecorded` gets the number of listed ids with no store record.
///
/// # Safety
/// `ids` points to `count` ids (or is null with `count` 0); `out_unrecorded` to a writable u32.
#[no_mangle]
pub unsafe extern "C" fn ee_units_order_check(ids: *const u32, count: usize, out_unrecorded: *mut u32) -> u32 {
    // SAFETY: the caller's contract
    let ids = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(ids, count) } };
    let (result, listed) = with_order(|o| (o.check(ids), o.ids().to_vec()));
    let unrecorded = with_store(|s| listed.iter().filter(|&&id| s.get(id).is_none()).count()) as u32;
    // SAFETY: the caller's contract
    unsafe { *out_unrecorded = unrecorded };
    result
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

/// A unit made out of every list (a shot): its record by object (`Store::insert_unlisted`).
///
/// # Safety
/// `record` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_insert_unlisted(record: *const EeUnitRecord) {
    // SAFETY: the caller's contract
    let record = unsafe { *record };
    with_store(|s| s.insert_unlisted(record));
}

/// TEST ONLY: the unlisted records' objects into `out` (at most `capacity`); their count.
///
/// # Safety
/// `out` points to `capacity` writable words (or is null with `capacity` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_unlisted(out: *mut u64, capacity: usize) -> usize {
    let objects = with_store(|s| s.unlisted_objects());
    for (i, o) in objects.iter().take(capacity).enumerate() {
        // SAFETY: the caller's contract (i < capacity)
        unsafe { *out.add(i) = *o };
    }
    objects.len()
}

/// TEST ONLY: the unlisted records against `objects` (`count` records filled from the objects
/// `ee_units_store_unlisted` named): the mismatches (`out`'s first id and field when it had none).
///
/// # Safety
/// `objects` points to `count` records (or is null with `count` 0); `out` to a valid result.
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_check_unlisted(
    objects: *const EeUnitRecord,
    count: usize,
    out: *mut EeUnitStoreCheck,
) -> u32 {
    let objects = if count == 0 || objects.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(objects, count) }
    };
    // SAFETY: the caller's contract
    let out = unsafe { &mut *out };
    with_store(|s| s.check_unlisted(objects, out))
}

/// The out-of-sync checksum's flag 1 over the store's units on the map (`Store::checksum_flag1`).
#[no_mangle]
pub extern "C" fn ee_units_store_checksum_flag1() -> u64 {
    with_store(|s| s.checksum_flag1())
}

/// TEST ONLY: the world's unit chain (`count` units: their ids and objects, in order) against the records' on-map
/// flags (`Store::check_on_map`); the mismatches.
///
/// # Safety
/// `ids` and `objects` point to `count` values each (either may be null when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_store_check_on_map(ids: *const u32, objects: *const u64, count: usize) -> u32 {
    let chain: Vec<(u32, u64)> = if count == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        let (ids, objects) =
            unsafe { (std::slice::from_raw_parts(ids, count), std::slice::from_raw_parts(objects, count)) };
        ids.iter().copied().zip(objects.iter().copied()).collect()
    };
    with_store(|s| s.check_on_map(&chain))
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

/// `EEActiveGroup::Fill`'s test over the store (`group::fill_members`): the indices into `ids` of the units an active
/// group takes for `slot`, in order, into `out` (room for `count`), their number.
///
/// # Safety
/// `ids` points to `count` ids and `out` to room for `count` (either may be null when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_fill_members(ids: *const u32, count: usize, slot: i64, out: *mut u32) -> usize {
    if count == 0 {
        return 0;
    }
    // SAFETY: the caller's contract
    let ids = unsafe { std::slice::from_raw_parts(ids, count) };
    let mut taken = Vec::with_capacity(count);
    with_store(|s| crate::group::fill_members(s, ids, slot, &mut taken));
    // SAFETY: the caller's contract; at most one index per id
    unsafe { std::ptr::copy_nonoverlapping(taken.as_ptr(), out, taken.len()) };
    taken.len()
}

/// `EEMExplore::Execute`'s body (`group::explore`) for the sender `player` (an `EEPlayer*`) and its selection
/// (`EEGroup*`), through the game's host calls.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_explore(player: *mut c_void, selection: *mut c_void) {
    let mut host = crate::host::game::GameHost;
    crate::group::explore(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection));
}

/// `EEActiveGroup::KeepGroundAttackers`' test over the store (`group::keeps_ground_attacker`): whether the group keeps
/// the unit.
#[no_mangle]
pub extern "C" fn ee_units_keeps_ground_attacker(id: u32) -> bool {
    with_store(|s| crate::group::keeps_ground_attacker(s.get(id)))
}

/// `EEActiveGroup::KeepNonCitizens`' test over the store (`group::keeps_non_citizen`): whether the group keeps the
/// unit.
#[no_mangle]
pub extern "C" fn ee_units_keeps_non_citizen(id: u32) -> bool {
    with_store(|s| crate::group::keeps_non_citizen(s.get(id)))
}

/// `EEMAddPatrolPoint::Execute`'s body (`group::add_patrol_point`) for the sender, its selection and the point.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_add_patrol_point(player: *mut c_void, selection: *mut c_void, x: f32, y: f32) {
    let mut host = crate::host::game::GameHost;
    crate::group::add_patrol_point(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), x, y);
}

/// `EEMAttackUnit::Execute`'s body (`group::attack_unit`) for the sender, its selection, the target id and the
/// queued and convert flags.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_attack_unit(
    player: *mut c_void,
    selection: *mut c_void,
    target: u32,
    queued: bool,
    convert: bool,
) {
    let mut host = crate::host::game::GameHost;
    let (player, selection) = (crate::host::HostRef(player), crate::host::HostRef(selection));
    crate::group::attack_unit(&mut host, player, selection, target, queued, convert);
}

/// `EEMHealUnit::Execute`'s body (`group::heal_unit`) for the sender, its selection and the target id.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_heal_unit(player: *mut c_void, selection: *mut c_void, target: u32) {
    let mut host = crate::host::game::GameHost;
    crate::group::heal_unit(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), target);
}

/// `EEMPathTo::Execute`'s body (`group::path_to`) for the sender, its selection, the message's point (read, and
/// written back as the body adjusted it) and its options.
///
/// # Safety
/// `x` and `y` point to the message's floats for the call.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_path_to(
    player: *mut c_void,
    selection: *mut c_void,
    x: *mut f32,
    y: *mut f32,
    facing: f32,
    forced: bool,
    rotating: bool,
    queued: bool,
    group_movement: bool,
) {
    let mut host = crate::host::game::GameHost;
    let (player, selection) = (crate::host::HostRef(player), crate::host::HostRef(selection));
    let o = crate::group::PathToOptions { facing, forced, rotating, queued, group_movement };
    // SAFETY: the caller's two floats
    let (px, py) = unsafe { (*x, *y) };
    if let Some((nx, ny)) = crate::group::path_to(&mut host, player, selection, px, py, o) {
        unsafe {
            *x = nx;
            *y = ny;
        }
    }
}

/// `EEMAttackGround::Execute`'s body (`group::attack_ground`) for the sender `player` (an `EEPlayer*`), its selection
/// (`EEGroup*`) and the message's point, through the game's host calls.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_attack_ground(player: *mut c_void, selection: *mut c_void, x: f32, y: f32) {
    let mut host = crate::host::game::GameHost;
    crate::group::attack_ground(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), x, y);
}

/// `EEMPatrol::Execute`'s body (`group::patrol`) for the sender, its selection and the message's target id.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_patrol(player: *mut c_void, selection: *mut c_void, target: i32) {
    let mut host = crate::host::game::GameHost;
    crate::group::patrol(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), target);
}

/// `EEMGatherResource::Execute`'s body (`group::gather`) for the sender, its selection, the target id and the
/// queued flag.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_gather(player: *mut c_void, selection: *mut c_void, target: u32, queued: bool) {
    let mut host = crate::host::game::GameHost;
    crate::group::gather(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), target, queued);
}

/// `EEMRepairUnit::Execute`'s body (`group::repair`) for the sender, its selection, the target id and the queued flag.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_repair(player: *mut c_void, selection: *mut c_void, target: u32, queued: bool) {
    let mut host = crate::host::game::GameHost;
    crate::group::repair(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), target, queued);
}

/// `EEActiveGroup::KeepCitizens`' test over the store (`group::keeps_citizen`): whether the group keeps the unit.
#[no_mangle]
pub extern "C" fn ee_units_keeps_citizen(id: u32) -> bool {
    with_store(|s| crate::group::keeps_citizen(s.get(id)))
}

/// `EEMBuildWall::Execute`'s body (`group::build_wall`) for the message's player, its selection, the technology and
/// the wall's two points.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_build_wall(
    player: *mut c_void,
    selection: *mut c_void,
    technology: i32,
    start_x: f32,
    start_y: f32,
    end_x: f32,
    end_y: f32,
) {
    let mut host = crate::host::game::GameHost;
    crate::group::build_wall(
        &mut host,
        crate::host::HostRef(player),
        crate::host::HostRef(selection),
        technology,
        (start_x, start_y),
        (end_x, end_y),
    );
}

/// `EEMUnloadTransport::Execute`'s body (`group::unload_transport`) for the sender, its selection and the tile.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_unload_transport(player: *mut c_void, selection: *mut c_void, x: i64, y: i64) {
    let mut host = crate::host::game::GameHost;
    crate::group::unload_transport(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), x, y);
}

/// `EEMDropoffResource::Execute`'s body (`group::dropoff_resource`) for the sender, its selection and the drop-off id.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_dropoff_resource(player: *mut c_void, selection: *mut c_void, dropoff: u32) {
    let mut host = crate::host::game::GameHost;
    crate::group::dropoff_resource(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), dropoff);
}

/// `EEMBuildUnit::Execute`'s body (`commands::build_unit`) for the sender, its selection, the technology, the tile
/// (the message's point through the original's `(long)` casts) and the queued flag.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_build_unit(
    player: *mut c_void,
    selection: *mut c_void,
    technology: i32,
    x: i64,
    y: i64,
    queued: bool,
) {
    let mut host = crate::host::game::GameHost;
    crate::commands::build_unit(
        &mut host,
        crate::host::HostRef(player),
        crate::host::HostRef(selection),
        technology,
        x,
        y,
        queued,
    );
}

/// `EEMSmartFarmers::Execute`'s body (`commands::smart_farmers`) for the sender, its selection and the target id.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_smart_farmers(player: *mut c_void, selection: *mut c_void, target: u32) {
    let mut host = crate::host::game::GameHost;
    crate::commands::smart_farmers(&mut host, crate::host::HostRef(player), crate::host::HostRef(selection), target);
}

/// `EEMCastCalamity::Execute`'s body (`commands::cast_calamity`) for the sender, its selection, the caster id, the
/// target id (-1: none), the tile and the technology.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_cast_calamity(
    player: *mut c_void,
    selection: *mut c_void,
    caster: u32,
    target: i32,
    x: i32,
    y: i32,
    technology: i32,
) {
    let mut host = crate::host::game::GameHost;
    crate::commands::cast_calamity(
        &mut host,
        crate::host::HostRef(player),
        crate::host::HostRef(selection),
        caster,
        target,
        x,
        y,
        technology,
    );
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

    #[test]
    fn fill_reads_the_process_store() {
        // ids and a player no other test of this crate uses
        let rec = |id: u32, hit_points: i64| EeUnitRecord {
            id,
            player: 0xfa00,
            valid: 1,
            hit_points,
            domains: crate::group::EE_MOVEMENT_LAND,
            ..Default::default()
        };
        for (id, hp) in [(0xfa00_0001, 5), (0xfa00_0002, 0)] {
            unsafe { ee_units_store_insert(&rec(id, hp)) };
        }
        let ids = [0xfa00_0002u32, 0xfa00_0009, 0xfa00_0001];
        let mut out = [0u32; 3];
        let n = unsafe { ee_units_fill_members(ids.as_ptr(), ids.len(), 2, out.as_mut_ptr()) };
        assert_eq!(&out[..n], &[2]);
        assert_eq!(unsafe { ee_units_fill_members(ids.as_ptr(), ids.len(), 1, out.as_mut_ptr()) }, 0);
        assert_eq!(unsafe { ee_units_fill_members(std::ptr::null(), 0, 0, std::ptr::null_mut()) }, 0);
        assert_eq!(ee_units_store_clear_player(0xfa00), 2);
    }

    #[test]
    fn the_ground_attackers_test_reads_the_process_store() {
        // an id and a player no other test of this crate uses
        let rec = EeUnitRecord { id: 0xf900_0001, player: 0xf900, can_attack_ground: 1, ..Default::default() };
        unsafe { ee_units_store_insert(&rec) };
        assert!(ee_units_keeps_ground_attacker(0xf900_0001));
        assert!(!ee_units_keeps_ground_attacker(0xf900_0002));
        assert_eq!(ee_units_store_clear_player(0xf900), 1);
        assert!(!ee_units_keeps_ground_attacker(0xf900_0001));
    }
}
