//! `RTSUnit`'s and `RTSPlayer`'s list bodies (LLE round 8, from `src/RTS Engine/Units/RTSUnit.cpp` and
//! `src/RTS Engine/Player/RTSPlayer.cpp`): a unit's two lists (its owner's, through `mNext`/`mPrevious`, and the
//! x-sorted tile chain, through its `RTSUnitLink` whose head may be an `RTSDummyUnit`), its life cycle's resets and
//! frees, and the player's unit list, its 128 id buckets, its pending and killed units, its checksum and its units'
//! save order. Simulation code: the oracle's seeds and the saves run every one of them.
//!
//! The classes stay C++ (`ee/RTSUnit.h`, `ee/EEPlayer.h`); their plain members are the ee_sim records the classes
//! derive from (`EeUnitsRtsUnitFields`, `EeUnitsRtsPlayerFields`), read and written in place; the pointers, the
//! links and the buckets through one-statement shims ([`UnitsCpp`]), the virtuals and the unit store by call, in the
//! original's order.

use core::ffi::{c_char, c_long, c_ulong, CStr};

use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;
use ee_sim::world::unit_fields::EeUnitsRtsUnitFields;

ee_base::cpp_shims! {
    trait UnitsCpp;
    // a unit
    fn unit_fields(unit: usize) -> *mut EeUnitsRtsUnitFields = ee_rtsucpp_unit_fields;
    fn node_slot(unit: usize) -> *mut usize = ee_rtsucpp_node_slot;
    fn next_slot(unit: usize) -> *mut usize = ee_rtsucpp_next_slot;
    fn previous_slot(unit: usize) -> *mut usize = ee_rtsucpp_previous_slot;
    fn tile_slot(unit: usize) -> *mut usize = ee_rtsucpp_tile_slot;
    fn player_slot(unit: usize) -> *mut usize = ee_rtsucpp_player_slot;
    fn dummy_head(link: usize) -> usize = ee_rtsucpp_dummy_head;
    fn head_first_slot(head: usize) -> *mut usize = ee_rtsucpp_head_first_slot;
    fn link_new() -> usize = ee_rtsucpp_link_new;
    fn link_delete(link: usize) = ee_rtsucpp_link_delete;
    fn store_linked(unit: usize) = ee_rtsucpp_store_linked;
    fn store_remove(unit: usize) = ee_rtsucpp_store_remove;
    fn unregister(unit: usize) = ee_rtsucpp_unregister;
    fn map_tile(map: usize, x: c_long, y: c_long) -> usize = ee_rtsucpp_map_tile;
    fn sight_on(unit: usize) = ee_rtsucpp_sight_on;
    fn sight_off(unit: usize) = ee_rtsucpp_sight_off;
    fn unit_reference(unit: usize) = ee_rtswcpp_unit_reference;
    fn unit_dereference(unit: usize) = ee_rtswcpp_unit_dereference;
    fn unit_invalidate(unit: usize) = ee_rtsucpp_unit_invalidate;
    fn unit_repair_pointers(unit: usize, first: c_ulong, second: c_ulong) = ee_rtsucpp_unit_repair_pointers;
    fn register(unit: usize) = ee_rtsucpp_register;
    fn store_insert(unit: usize, id: c_ulong, player: usize) = ee_rtsucpp_store_insert;
    // a player
    fn player_fields(player: usize) -> *mut EeUnitsRtsPlayerFields = ee_rtspcpp_player_fields;
    fn buckets(player: usize) -> *mut EeRtsUnitBucket = ee_rtspcpp_buckets;
    fn first_slot(player: usize) -> *mut usize = ee_rtspcpp_first_slot;
    fn last_slot(player: usize) -> *mut usize = ee_rtspcpp_last_slot;
    fn pending_size(player: usize) -> usize = ee_rtspcpp_pending_size;
    fn pending_at(player: usize, index: usize) -> usize = ee_rtspcpp_pending_at;
    fn pending_clear(player: usize) = ee_rtspcpp_pending_clear;
    fn killed_size(player: usize) -> usize = ee_rtspcpp_killed_size;
    fn killed_at(player: usize, index: usize) -> usize = ee_rtspcpp_killed_at;
    fn killed_clear(player: usize) = ee_rtspcpp_killed_clear;
    fn killed_push(player: usize, unit: usize) = ee_rtspcpp_killed_push;
    fn add_unit_virtual(player: usize, unit: usize, keep_id: bool) = ee_rtspcpp_add_unit;
    fn release_pending_virtual(player: usize) = ee_rtspcpp_release_pending;
    fn removing_unit(player: usize, unit: usize) = ee_rtspcpp_removing_unit;
    fn unit_removed(player: usize, unit: usize) = ee_rtspcpp_unit_removed;
    fn removals_done(player: usize) = ee_rtspcpp_removals_done;
    fn save_unit(player: usize, unit: usize, major: c_ulong, minor: c_ulong, file: usize) -> c_long = ee_rtspcpp_save_unit;
    fn name_checksum(player: usize, checksum: usize) = ee_rtspcpp_name_checksum;
    fn checksum_add(checksum: usize, name: *const c_char, value: c_ulong) = ee_rtspcpp_checksum_add;
    fn write_ulong(file: usize, value: c_ulong) -> c_long = ee_rtscpp_write_ulong;
}

/// `RTSUnitBucket`: the first unit of the bucket's chain and its count.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeRtsUnitBucket {
    pub first: usize,
    pub count: c_ulong,
}

/// The unit's link's two members (`RTSUnitLink`: next, then previous).
fn link<C: UnitsCpp>(c: &mut C, unit: usize) -> *mut [usize; 2] {
    // SAFETY: the unit's member
    unsafe { *c.node_slot(unit) as *mut [usize; 2] }
}
const NEXT: usize = 0;
const PREV: usize = 1;
/// No tile (`-10000`).
const OFF_MAP: c_long = -10000;

macro_rules! failed {
    ($c:expr, $where:expr, $what:expr, $unit:expr, $other:expr) => {
        list_failed($c, $where.as_ptr(), $what.as_ptr(), $unit, $other)
    };
}

/// Whether the unit chains are checked (`EE_CHECK_UNIT_LIST` set, not "0"; read once, said once on stderr).
fn check_on() -> bool {
    static ON: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    cfg!(test)
        || *ON.get_or_init(|| {
            let on = std::env::var_os("EE_CHECK_UNIT_LIST").is_some_and(|v| !v.is_empty() && v != "0");
            if on {
                eprintln!("EE-UNITLIST: checking the unit chains (EE_CHECK_UNIT_LIST)");
            }
            on
        })
}

/// `RTSUnitListFailed`: the broken chain reported on stderr (where, what, the unit and its links, the other unit's or
/// the chain head's), then the process stops (a test panics with `what`).
fn list_failed<C: UnitsCpp>(c: &mut C, where_: *const c_char, what: *const c_char, unit: usize, other: usize) {
    // SAFETY: the callers' literals
    let text = |p: *const c_char| unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned();
    let mut line = format!("EE-UNITLIST FAILED in {}: {}; ", text(where_), text(what));
    let links = |c: &mut C, u: usize| {
        let l = link(c, u);
        // SAFETY: a live link or none
        if l.is_null() {
            (0, 0)
        } else {
            unsafe { ((*l)[PREV], (*l)[NEXT]) }
        }
    };
    if unit != 0 {
        // SAFETY: a live unit's members
        let f = unsafe { &*c.unit_fields(unit) };
        let (id, x, y, valid) = (f.mUnitID, f.mTileX, f.mTileY, f.mIsValid as i32);
        let node = link(c, unit) as usize;
        let (prev, next) = links(c, unit);
        line += &format!(
            "unit {unit:#x} id {id:08x} x {x} y {y} valid {valid} node {node:#x} prev {prev:#x} next {next:#x}"
        );
    } else {
        line += "unit 0x0 id 00000000 x 0 y 0 valid -1 node 0x0 prev 0x0 next 0x0";
    }
    if other != 0 && c.dummy_head(other) == 0 {
        // SAFETY: as above
        let f = unsafe { &*c.unit_fields(other) };
        let (id, x, valid) = (f.mUnitID, f.mTileX, f.mIsValid as i32);
        let (prev, next) = links(c, other);
        line += &format!("; other {other:#x} id {id:08x} x {x} valid {valid} prev {prev:#x} next {next:#x}");
    } else if other != 0 {
        line += &format!("; other: the chain's head {other:#x}");
    }
    eprintln!("{line}");
    #[cfg(test)]
    panic!("{}", text(what));
    #[cfg(not(test))]
    std::process::abort();
}

/// A unit (or a chain head) is in a chain: a head always is; a unit has a previous link.
fn in_chain<C: UnitsCpp>(c: &mut C, unit: usize) -> bool {
    if c.dummy_head(unit) != 0 {
        return true;
    }
    let l = link(c, unit);
    // SAFETY: a live link
    !l.is_null() && unsafe { (*l)[PREV] } != 0
}

/// `RTSCheckNeighbours` (debugging): the unit's neighbours point back at it.
fn check_neighbours<C: UnitsCpp>(c: &mut C, unit: usize, where_: &core::ffi::CStr) {
    let l = link(c, unit);
    // SAFETY: a live link
    let (prev, next) = unsafe { ((*l)[PREV], (*l)[NEXT]) };
    if prev == 0 {
        if next != 0 {
            failed!(c, where_, c"a next link without a previous one", unit, next);
        }
        return;
    }
    let head = c.dummy_head(prev);
    if head != 0 {
        // SAFETY: the head's member
        if unsafe { *c.head_first_slot(head) } != unit {
            failed!(c, where_, c"the head's first unit is another", unit, prev);
        }
    } else {
        let pl = link(c, prev);
        // SAFETY: a live link
        if pl.is_null() || unsafe { (*pl)[NEXT] } != unit {
            failed!(c, where_, c"the previous unit's next is another", unit, prev);
        } else if !in_chain(c, prev) {
            failed!(c, where_, c"the previous unit is in no chain", unit, prev);
        }
    }
    if next != 0 {
        let nl = link(c, next);
        // SAFETY: a live link
        if nl.is_null() || unsafe { (*nl)[PREV] } != unit {
            failed!(c, where_, c"the next unit's previous is another", unit, next);
        }
    }
}

/// `RTSCheckUnitList` (debugging, `EE_CHECK_UNIT_LIST`): a whole chain walked.
///
/// # Safety
/// `head` is null or a live chain head.
pub unsafe fn check_unit_list<C: UnitsCpp>(c: &mut C, head: usize, where_: *const c_char) {
    if !check_on() || head == 0 {
        return;
    }
    let mut prev = head;
    let mut count: c_ulong = 0;
    let mut unit = unsafe { *c.head_first_slot(head) };
    while unit != 0 {
        let l = link(c, unit);
        if l.is_null() {
            list_failed(c, where_, c"a unit in the chain has no node".as_ptr(), unit, prev);
        }
        if unsafe { (*l)[PREV] } != prev {
            list_failed(c, where_, c"a unit's previous is not the unit before it".as_ptr(), unit, prev);
        }
        if !unsafe { (*c.unit_fields(unit)).mIsValid } {
            list_failed(c, where_, c"an invalid (removed) unit is in the chain".as_ptr(), unit, prev);
        }
        count += 1;
        if count > (1 << 22) {
            list_failed(c, where_, c"the chain loops (over 4M units)".as_ptr(), unit, prev);
        }
        prev = unit;
        unit = unsafe { (*l)[NEXT] };
    }
}

/// `RTSUnit::Reset`: no id, no node, tile or neighbours, off the map, no owner, flags cleared.
pub fn reset<C: UnitsCpp>(c: &mut C, unit: usize) {
    let f = c.unit_fields(unit);
    // SAFETY: the unit's members
    unsafe {
        (*f).mUnitID = 0xffff_ffff;
        *c.node_slot(unit) = 0;
        *c.tile_slot(unit) = 0;
        (*f).mTileX = OFF_MAP;
        (*f).mTileY = OFF_MAP;
        *c.next_slot(unit) = 0;
        *c.previous_slot(unit) = 0;
        *c.player_slot(unit) = 0;
        (*f).mIsQueuedForDelete = false;
        (*f).mIsEditorSelected = false;
        (*f).mIsValid = false;
        (*f).mIsNodeFreed = false;
    }
}

/// `RTSUnit::RTSUnit`'s body: reset, not valid, the node freed.
pub fn construct<C: UnitsCpp>(c: &mut C, unit: usize) {
    reset(c, unit);
    let f = c.unit_fields(unit);
    // SAFETY: the unit's members
    unsafe {
        (*f).mIsValid = false;
        (*f).mIsNodeFreed = true;
    }
}

/// `RTSUnit::Initialize`: reset, valid, a new link unless `linked` is 0.
pub fn initialize<C: UnitsCpp>(c: &mut C, unit: usize, linked: c_long) {
    reset(c, unit);
    let f = c.unit_fields(unit);
    // SAFETY: the unit's members
    unsafe {
        (*f).mIsValid = true;
        (*f).mIsNodeFreed = false;
        *c.node_slot(unit) = if linked == 0 { 0 } else { c.link_new() };
    }
}

/// `RTSUnit::Clear`: reset, not valid.
pub fn clear<C: UnitsCpp>(c: &mut C, unit: usize) {
    reset(c, unit);
    // SAFETY: the unit's member
    unsafe { (*c.unit_fields(unit)).mIsValid = false };
}

/// `RTSUnit::RemoveFromList`: out of its owner's list.
pub fn remove_from_list<C: UnitsCpp>(c: &mut C, unit: usize) {
    // SAFETY: the units' members
    unsafe {
        let next = *c.next_slot(unit);
        let prev = *c.previous_slot(unit);
        if next != 0 {
            *c.previous_slot(next) = prev;
        }
        if prev != 0 {
            *c.next_slot(prev) = next;
        }
        *c.next_slot(unit) = 0;
        *c.previous_slot(unit) = 0;
    }
}

/// `RTSUnit::InsertAfter`: into its owner's list after `after` (null: alone).
pub fn insert_after<C: UnitsCpp>(c: &mut C, unit: usize, after: usize) {
    remove_from_list(c, unit);
    // SAFETY: the units' members
    unsafe {
        if after != 0 {
            *c.next_slot(unit) = *c.next_slot(after);
            *c.next_slot(after) = unit;
            let next = *c.next_slot(unit);
            if next != 0 {
                *c.previous_slot(next) = unit;
            }
        }
        *c.previous_slot(unit) = after;
    }
}

/// `RTSUnit::Link`: into the tile chain after `after` (a unit, or the chain's head: then first).
pub fn link_after<C: UnitsCpp>(c: &mut C, unit: usize, after: usize) {
    unlink(c, unit);
    let check = check_on();
    if check && !unsafe { (*c.unit_fields(unit)).mIsValid } {
        failed!(c, c"RTSUnit::Link", c"linking a removed (invalid) unit", unit, after);
    }
    if check && !in_chain(c, after) {
        failed!(c, c"RTSUnit::Link", c"linking after a unit that is in no chain", unit, after);
    }
    let l = link(c, unit);
    let head = c.dummy_head(after);
    // SAFETY: the live links and the head
    unsafe {
        if head != 0 {
            let first = c.head_first_slot(head);
            (*l)[NEXT] = *first;
            (*l)[PREV] = after;
            *first = unit;
        } else {
            let al = link(c, after);
            (*l)[NEXT] = (*al)[NEXT];
            (*l)[PREV] = after;
            (*al)[NEXT] = unit;
        }
        let next = (*l)[NEXT];
        if next != 0 {
            (*link(c, next))[PREV] = unit;
        }
    }
    if check {
        check_neighbours(c, unit, c"RTSUnit::Link (after)");
    }
    c.store_linked(unit);
}

/// `RTSUnit::Unlink`: out of the tile chain.
pub fn unlink<C: UnitsCpp>(c: &mut C, unit: usize) {
    if check_on() {
        check_neighbours(c, unit, c"RTSUnit::Unlink (before)");
    }
    let l = link(c, unit);
    // SAFETY: the live links and the head
    unsafe {
        let (next, prev) = ((*l)[NEXT], (*l)[PREV]);
        if next != 0 {
            (*link(c, next))[PREV] = prev;
        }
        if prev != 0 {
            let head = c.dummy_head(prev);
            if head != 0 {
                *c.head_first_slot(head) = next;
            } else {
                (*link(c, prev))[NEXT] = next;
            }
        }
        (*l)[NEXT] = 0;
        (*l)[PREV] = 0;
    }
    c.store_linked(unit);
}

/// `ESVERIFY_AT` of RTSUnit.cpp.
fn verify(ok: bool, line: c_ulong, expression: &core::ffi::CStr) {
    if ok {
        return;
    }
    #[cfg(not(test))]
    // SAFETY: C strings
    unsafe {
        ee_base::error::ee_error_verify(c"C:\\EEX\\RTS Engine\\Units\\RTSUnit.cpp".as_ptr(), line, expression.as_ptr())
    };
    #[cfg(test)]
    panic!("verify {line}: {expression:?}");
}

/// `RTSUnit::Invalidate`: out of the owner's list and the tile chain, not valid.
pub fn invalidate<C: UnitsCpp>(c: &mut C, unit: usize) {
    let f = c.unit_fields(unit);
    verify(unsafe { (*f).mIsValid }, 80, c"this->mIsValid == true");
    remove_from_list(c, unit);
    // SAFETY: the unit's member
    if unsafe { *c.node_slot(unit) } != 0 {
        unlink(c, unit);
    }
    unsafe { (*f).mIsValid = false };
}

/// `RTSUnit::FreeNode`: out of the registry and the store, the link deleted.
pub fn free_node<C: UnitsCpp>(c: &mut C, unit: usize) {
    let f = c.unit_fields(unit);
    verify(!unsafe { (*f).mIsValid }, 119, c"this->mIsValid == false");
    c.unregister(unit);
    c.store_remove(unit);
    // SAFETY: the unit's member
    let node = unsafe { *c.node_slot(unit) };
    c.link_delete(node);
    unsafe {
        *c.node_slot(unit) = 0;
        (*f).mIsNodeFreed = true;
    }
}

/// `RTSUnit::UpdateTile`: the map's tile at the unit's tile coordinates (off the map: none; no such tile: off the
/// map).
pub fn update_tile<C: UnitsCpp>(c: &mut C, unit: usize, map: usize) {
    let f = c.unit_fields(unit);
    // SAFETY: the unit's members
    unsafe {
        if (*f).mTileX == OFF_MAP || (*f).mTileY == OFF_MAP {
            *c.tile_slot(unit) = 0;
        } else {
            let tile = c.map_tile(map, (*f).mTileX, (*f).mTileY);
            if tile == 0 {
                (*f).mTileX = OFF_MAP;
                (*f).mTileY = OFF_MAP;
            }
            *c.tile_slot(unit) = tile;
        }
    }
}

// ---- the player

fn bucket<C: UnitsCpp>(c: &mut C, player: usize, index: usize) -> *mut EeRtsUnitBucket {
    // SAFETY: the player's 128 buckets
    unsafe { c.buckets(player).add(index & 0x7f) }
}

/// `RTSPlayer::Reset`: no slot, no units, no ids, flags cleared.
pub fn player_reset<C: UnitsCpp>(c: &mut C, player: usize) {
    let f = c.player_fields(player);
    // SAFETY: the player's members
    unsafe {
        (*f).mSlot = 0xff;
        *c.first_slot(player) = 0;
        *c.last_slot(player) = 0;
        (*f).mNextUniqueID = 0;
        (*f).mPlayerID = 0;
        (*f).mUnitCount = 0;
        (*f).mSeesAll = false;
        (*f).mSaveVisMap = false;
        (*f).mObserverMode = false;
    }
}

/// `RTSPlayer::AddPendingUnits`: every pending unit added (a virtual), the list optionally released.
pub fn add_pending_units<C: UnitsCpp>(c: &mut C, player: usize, keep_id: bool, release: bool) {
    let mut i = 0;
    while i < c.pending_size(player) {
        let u = c.pending_at(player, i);
        c.add_unit_virtual(player, u, keep_id);
        i += 1;
    }
    if release {
        c.release_pending_virtual(player);
    }
}

/// `RTSPlayer::ReleasePendingUnits`: every pending unit dereferenced, the list emptied.
pub fn release_pending_units<C: UnitsCpp>(c: &mut C, player: usize) {
    let mut i = 0;
    while i < c.pending_size(player) {
        let u = c.pending_at(player, i);
        c.unit_dereference(u);
        i += 1;
    }
    c.pending_clear(player);
}

/// `RTSPlayer::AddUnit`: a new id (the player's in the top byte, the next unique one below; its sight placed when
/// it is linked) unless `keep_id`, then `InsertUnit`.
pub fn add_unit<C: UnitsCpp>(c: &mut C, player: usize, unit: usize, keep_id: bool) {
    if !keep_id {
        let f = c.player_fields(player);
        let uf = c.unit_fields(unit);
        // SAFETY: the records
        unsafe {
            (*uf).mUnitID = (*f).mPlayerID.wrapping_mul(0x100_0000).wrapping_add((*f).mNextUniqueID);
            (*f).mNextUniqueID = ((*f).mNextUniqueID + 1) & 0xff_ffff;
        }
        if unsafe { *c.node_slot(unit) } != 0 {
            c.sight_on(unit);
        }
    }
    insert_unit(c, player, unit);
}

/// `RTSPlayer::InsertUnit`: the unit referenced, registered, owned and stored, then into the player's list: in
/// its bucket's chain (after the bucket's first unit), or at the end for a new bucket.
pub fn insert_unit<C: UnitsCpp>(c: &mut C, player: usize, unit: usize) {
    let f = c.player_fields(player);
    let id = unsafe { (*c.unit_fields(unit)).mUnitID };
    c.unit_reference(unit);
    c.register(unit);
    unsafe { *c.player_slot(unit) = player };
    c.store_insert(unit, unsafe { (*f).mPlayerID }, player);
    unsafe { (*f).mUnitCount = (*f).mUnitCount.wrapping_add(1) };
    let b = bucket(c, player, id as usize);
    // SAFETY: the player's members and bucket
    unsafe {
        if *c.first_slot(player) == 0 {
            insert_after(c, unit, 0);
            *c.first_slot(player) = unit;
            *c.last_slot(player) = unit;
            (*b).first = unit;
            (*b).count = 1;
        } else if (*b).count == 0 {
            let last = *c.last_slot(player);
            insert_after(c, unit, last);
            *c.last_slot(player) = unit;
            (*b).first = unit;
            (*b).count = 1;
        } else {
            insert_after(c, unit, (*b).first);
            if *c.next_slot(unit) == 0 {
                *c.last_slot(player) = unit;
            }
            (*b).count += 1;
        }
    }
}

/// `RTSPlayer::RemoveKilledUnits`: every killed unit out of the player (its sight removed when linked and the
/// player keeps a visibility map; the list's ends and its bucket fixed; out of the store; invalidated and
/// dereferenced), the virtuals around each removal and after all of them.
pub fn remove_killed_units<C: UnitsCpp>(c: &mut C, player: usize) {
    let f = c.player_fields(player);
    let mut i = 0;
    while i < c.killed_size(player) {
        let unit = c.killed_at(player, i);
        let id = unsafe { (*c.unit_fields(unit)).mUnitID };
        let b = bucket(c, player, id as usize);
        if unsafe { *c.node_slot(unit) } != 0 && unsafe { (*f).mSaveVisMap } {
            c.sight_off(unit);
        }
        c.removing_unit(player, unit);
        // SAFETY: the player's members and bucket
        unsafe {
            if *c.first_slot(player) == unit {
                *c.first_slot(player) = *c.next_slot(unit);
            }
            if *c.last_slot(player) == unit {
                *c.last_slot(player) = *c.previous_slot(unit);
            }
            (*b).count = (*b).count.wrapping_sub(1);
            if (*b).count != 0 && (*b).first == unit {
                (*b).first = *c.next_slot(unit);
            } else if (*b).count == 0 {
                (*b).first = 0;
            }
        }
        remove_from_list(c, unit);
        c.store_remove(unit);
        c.unit_removed(player, unit);
        unsafe { (*f).mUnitCount = (*f).mUnitCount.wrapping_sub(1) };
        c.unit_invalidate(unit);
        c.unit_dereference(unit);
        i += 1;
    }
    c.removals_done(player);
    c.killed_clear(player);
}

/// `RTSPlayer::GetChecksum`: the name's, then ("RTSPlayer-HashChecksum", 0) and ("RTSPlayer", id * 10 + units +
/// next id).
pub fn player_checksum<C: UnitsCpp>(c: &mut C, player: usize, checksum: usize) {
    let f = c.player_fields(player);
    let value =
        unsafe { (*f).mPlayerID.wrapping_mul(10).wrapping_add((*f).mUnitCount).wrapping_add((*f).mNextUniqueID) };
    c.name_checksum(player, checksum);
    c.checksum_add(checksum, c"RTSPlayer-HashChecksum".as_ptr(), 0);
    c.checksum_add(checksum, c"RTSPlayer".as_ptr(), value);
}

/// `RTSPlayer::GetPendingUnit`: NULL past the end.
pub fn pending_unit<C: UnitsCpp>(c: &mut C, player: usize, index: c_ulong) -> usize {
    if (index as usize) < c.pending_size(player) {
        return c.pending_at(player, index as usize);
    }
    0
}

/// `RTSPlayer::GetUnit`: by id through its bucket's chain.
pub fn player_unit<C: UnitsCpp>(c: &mut C, player: usize, id: c_ulong) -> usize {
    let b = bucket(c, player, id as usize);
    // SAFETY: the bucket and the units' members
    unsafe {
        let count = (*b).count;
        let mut unit = (*b).first;
        for _ in 0..count {
            if (*c.unit_fields(unit)).mUnitID == id {
                return unit;
            }
            unit = *c.next_slot(unit);
        }
    }
    0
}

/// `RTSPlayer::KillUnit`: queued for deletion once (or again when forced).
pub fn kill_unit<C: UnitsCpp>(c: &mut C, player: usize, unit: usize, force: bool) {
    let uf = c.unit_fields(unit);
    if !unsafe { (*uf).mIsQueuedForDelete } || force {
        unsafe { (*uf).mIsQueuedForDelete = true };
        c.killed_push(player, unit);
    }
}

/// `RTSPlayer::RepairPointers`: every unit's.
pub fn repair_pointers<C: UnitsCpp>(c: &mut C, player: usize, first: c_ulong, second: c_ulong) -> c_long {
    let mut unit = unsafe { *c.first_slot(player) };
    while unit != 0 {
        c.unit_repair_pointers(unit, first, second);
        unit = unsafe { *c.next_slot(unit) };
    }
    0
}

/// `RTSPlayer::SaveUnits`: each bucket's count, then its units: the first, then from the chain's last back to
/// the second (the original's order); the first error.
pub fn save_units<C: UnitsCpp>(c: &mut C, player: usize, major: c_ulong, minor: c_ulong, file: usize) -> c_long {
    let mut result: c_long = 0;
    for i in 0..0x80 {
        let b = bucket(c, player, i);
        let count = unsafe { (*b).count };
        result = c.write_ulong(file, count);
        if result != 0 {
            return result;
        }
        let mut unit = unsafe { (*b).first };
        if unit != 0 {
            result = c.save_unit(player, unit, major, minor, file);
            for _ in 0..count.wrapping_sub(1) {
                unit = unsafe { *c.next_slot(unit) };
            }
            let mut j: c_ulong = 0;
            while j < count.wrapping_sub(1) && result == 0 {
                result = c.save_unit(player, unit, major, minor, file);
                unit = unsafe { *c.previous_slot(unit) };
                j += 1;
            }
            if result != 0 {
                return result;
            }
        }
    }
    result
}

#[cfg(not(test))]
pub(crate) mod ffi {
    use super::*;
    use core::ffi::c_void;
    use ee_base::shim::ShimFfi as Ffi;

    /// `RTSUnit::RTSUnit`'s body.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_construct(unit: *mut c_void) {
        construct(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::Initialize`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_initialize(unit: *mut c_void, linked: c_long) {
        initialize(&mut Ffi, unit as usize, linked)
    }

    /// `RTSUnit::Reset`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_reset(unit: *mut c_void) {
        reset(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::Clear`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_clear(unit: *mut c_void) {
        clear(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::RemoveFromList`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_remove_from_list(unit: *mut c_void) {
        remove_from_list(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::InsertAfter`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_insert_after(unit: *mut c_void, after: *mut c_void) {
        insert_after(&mut Ffi, unit as usize, after as usize)
    }

    /// `RTSUnit::Link`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_link(unit: *mut c_void, after: *mut c_void) {
        link_after(&mut Ffi, unit as usize, after as usize)
    }

    /// `RTSUnit::Unlink`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_unlink(unit: *mut c_void) {
        unlink(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::Invalidate`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_invalidate(unit: *mut c_void) {
        invalidate(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::FreeNode`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_free_node(unit: *mut c_void) {
        free_node(&mut Ffi, unit as usize)
    }

    /// `RTSUnit::UpdateTile`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_update_tile(unit: *mut c_void, map: *mut c_void) {
        update_tile(&mut Ffi, unit as usize, map as usize)
    }

    /// `RTSCheckUnitList`.
    #[no_mangle]
    pub extern "C" fn ee_rts_unit_check_list(head: *const c_void, where_: *const c_char) {
        unsafe { check_unit_list(&mut Ffi, head as usize, where_) }
    }

    /// `RTSPlayer::Reset`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_reset(player: *mut c_void) {
        player_reset(&mut Ffi, player as usize)
    }

    /// `RTSPlayer::AddPendingUnits`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_add_pending_units(player: *mut c_void, keep_id: bool, release: bool) {
        add_pending_units(&mut Ffi, player as usize, keep_id, release)
    }

    /// `RTSPlayer::ReleasePendingUnits`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_release_pending_units(player: *mut c_void) {
        release_pending_units(&mut Ffi, player as usize)
    }

    /// `EEPlayer::AddPendingUnits`: the pending groups appended to the running ones (`groups`, the player's
    /// `mGroupLists`: ee_sim group_obj.rs), then `RTSPlayer`'s.
    ///
    /// # Safety
    /// `groups` is the player's live group lists.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_eeplayer_add_pending_units(
        player: *mut c_void,
        groups: *mut c_void,
        keep_id: bool,
        release: bool,
    ) {
        // SAFETY: the caller's contract
        unsafe { ee_sim::units::group_obj::ee_units_player_groups_do(groups.cast(), 0) };
        add_pending_units(&mut Ffi, player as usize, keep_id, release)
    }

    /// `EEPlayer::ReleasePendingUnits`: the pending groups cleared, then `RTSPlayer`'s.
    ///
    /// # Safety
    /// As [`ee_rts_eeplayer_add_pending_units`].
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_eeplayer_release_pending_units(player: *mut c_void, groups: *mut c_void) {
        // SAFETY: the caller's contract
        unsafe { ee_sim::units::group_obj::ee_units_player_groups_do(groups.cast(), 1) };
        release_pending_units(&mut Ffi, player as usize)
    }

    /// `RTSPlayer::AddUnit`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_add_unit(player: *mut c_void, unit: *mut c_void, keep_id: bool) {
        add_unit(&mut Ffi, player as usize, unit as usize, keep_id)
    }

    /// `RTSPlayer::InsertUnit`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_insert_unit(player: *mut c_void, unit: *mut c_void) {
        insert_unit(&mut Ffi, player as usize, unit as usize)
    }

    /// `RTSPlayer::RemoveKilledUnits`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_remove_killed_units(player: *mut c_void) {
        remove_killed_units(&mut Ffi, player as usize)
    }

    /// `RTSPlayer::GetChecksum`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_checksum(player: *mut c_void, checksum: *mut c_void) {
        player_checksum(&mut Ffi, player as usize, checksum as usize)
    }

    /// `RTSPlayer::GetPendingUnit`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_pending_unit(player: *mut c_void, index: c_ulong) -> *mut c_void {
        pending_unit(&mut Ffi, player as usize, index) as *mut c_void
    }

    /// `RTSPlayer::GetUnit`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_unit(player: *mut c_void, id: c_ulong) -> *mut c_void {
        player_unit(&mut Ffi, player as usize, id) as *mut c_void
    }

    /// `RTSPlayer::KillUnit`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_kill_unit(player: *mut c_void, unit: *mut c_void, force: bool) {
        kill_unit(&mut Ffi, player as usize, unit as usize, force)
    }

    /// `RTSPlayer::RepairPointers`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_repair_pointers(player: *mut c_void, first: c_ulong, second: c_ulong) -> c_long {
        repair_pointers(&mut Ffi, player as usize, first, second)
    }

    /// `RTSPlayer::SaveUnits`.
    #[no_mangle]
    pub extern "C" fn ee_rts_player_save_units(
        player: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
        file: *mut c_void,
    ) -> c_long {
        save_units(&mut Ffi, player as usize, major, minor, file as usize)
    }
}

// ---- the unit registry's C++ entry points (RTSUnit.cpp; LLE round 11) ----

/// The id of a unit without one.
const NO_ID: u32 = 0xffff_ffff;

/// `RTSResolveUnitID`: the registered unit of an id (NULL for the no-id value or an unregistered id).
#[no_mangle]
pub extern "C" fn ee_rts_unit_resolve(id: u32) -> *mut core::ffi::c_void {
    if id == NO_ID {
        return core::ptr::null_mut();
    }
    ee_sim::units::ffi::ee_units_resolve(id)
}

/// `RTSRegisterUnit` (`register`) and `RTSUnregisterUnit`: the unit `unit` (whose record is `fields`) under its id,
/// unless it has none; an unregister leaves an entry that is another unit's.
///
/// # Safety
/// `fields` is the live unit's record.
#[no_mangle]
pub unsafe extern "C" fn ee_rts_unit_registry(
    fields: *const EeUnitsRtsUnitFields,
    unit: *mut core::ffi::c_void,
    register: bool,
) {
    // SAFETY: the function's contract
    let id = unsafe { (*fields).mUnitID } as u32;
    if id == NO_ID {
        return;
    }
    if register {
        ee_sim::units::ffi::ee_units_register(id, unit);
    } else {
        ee_sim::units::ffi::ee_units_unregister(id, unit);
    }
}

/// `EEUnitHandleID`: the unit's id; a unit without one is reported once (its handle resolves to nothing).
///
/// # Safety
/// `fields` is the live unit's record.
#[no_mangle]
pub unsafe extern "C" fn ee_rts_unit_handle_id(fields: *const EeUnitsRtsUnitFields) -> u32 {
    static REPORTED: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);
    // SAFETY: the function's contract
    let id = unsafe { (*fields).mUnitID } as u32;
    if id == NO_ID && !REPORTED.swap(true, core::sync::atomic::Ordering::Relaxed) {
        eprintln!("EE-HANDLE: a handle to a unit without an id ({fields:p}): it resolves to nothing");
    }
    id
}

/// `RTSUnit::Describe` (`long` false: "uid N, at X,Y:") and `DescribeLong` ("UnitID:N\nTile:(X,Y)\n"): the text
/// appended, the id's low 24 bits. The text lives until the thread's next call.
///
/// # Safety
/// `fields` is the live unit's record.
#[no_mangle]
pub unsafe extern "C" fn ee_rts_unit_describe(fields: *const EeUnitsRtsUnitFields, long: bool) -> *const c_char {
    thread_local!(static TEXT: core::cell::RefCell<std::ffi::CString> = core::cell::RefCell::default());
    // SAFETY: the function's contract
    let f = unsafe { &*fields };
    let text = describe(f.mUnitID, f.mTileX, f.mTileY, long);
    TEXT.with(|t| {
        *t.borrow_mut() = std::ffi::CString::new(text).unwrap_or_default();
        t.borrow().as_ptr()
    })
}

fn describe(id: c_ulong, x: c_long, y: c_long, long: bool) -> String {
    let id = id & 0xff_ffff;
    if long {
        format!("UnitID:{id}\nTile:({x},{y})\n")
    } else {
        format!("uid {id}, at {x},{y}:")
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn a_description_is_the_low_id_and_the_tile() {
        assert_eq!(describe(0x0300_0012, 4, -1, false), "uid 18, at 4,-1:");
        assert_eq!(describe(5, 7, 8, true), "UnitID:5\nTile:(7,8)\n");
    }

    #[test]
    fn the_no_id_value_resolves_to_nothing() {
        assert!(ee_rts_unit_resolve(NO_ID).is_null());
    }

    use super::*;
    use std::cell::RefCell;
    use std::collections::BTreeMap;

    /// A small world: units with records and links, a player with buckets, and a log of the virtual calls.
    #[derive(Default)]
    struct World {
        fields: BTreeMap<usize, Box<EeUnitsRtsUnitFields>>,
        node: BTreeMap<usize, Box<usize>>,
        next: BTreeMap<usize, Box<usize>>,
        prev: BTreeMap<usize, Box<usize>>,
        tile: BTreeMap<usize, Box<usize>>,
        owner: BTreeMap<usize, Box<usize>>,
        // boxed: the links' addresses stay put as the vector grows
        #[allow(clippy::vec_box)]
        links: Vec<Box<[usize; 2]>>,
        player: Box<EeUnitsRtsPlayerFields>,
        buckets: Vec<EeRtsUnitBucket>,
        first: Box<usize>,
        last: Box<usize>,
        pending: Vec<usize>,
        killed: Vec<usize>,
        heads: BTreeMap<usize, Box<usize>>,
        log: RefCell<Vec<String>>,
    }

    impl World {
        fn new() -> World {
            World {
                buckets: vec![EeRtsUnitBucket { first: 0, count: 0 }; 128],
                player: Box::new(EeUnitsRtsPlayerFields::default()),
                ..Default::default()
            }
        }
        fn unit(&mut self, u: usize) {
            let f = EeUnitsRtsUnitFields { mIsValid: true, ..Default::default() };
            self.fields.insert(u, Box::new(f));
            for m in [&mut self.node, &mut self.next, &mut self.prev, &mut self.tile, &mut self.owner] {
                m.insert(u, Box::new(0));
            }
        }
        fn note(&self, s: String) {
            self.log.borrow_mut().push(s);
        }
        fn order(&self) -> Vec<usize> {
            let mut v = Vec::new();
            let mut u = *self.first;
            while u != 0 {
                v.push(u);
                u = *self.next[&u];
            }
            v
        }
    }

    impl UnitsCpp for World {
        fn unit_fields(&mut self, unit: usize) -> *mut EeUnitsRtsUnitFields {
            &mut **self.fields.get_mut(&unit).unwrap()
        }
        fn node_slot(&mut self, unit: usize) -> *mut usize {
            &mut **self.node.get_mut(&unit).unwrap()
        }
        fn next_slot(&mut self, unit: usize) -> *mut usize {
            &mut **self.next.get_mut(&unit).unwrap()
        }
        fn previous_slot(&mut self, unit: usize) -> *mut usize {
            &mut **self.prev.get_mut(&unit).unwrap()
        }
        fn tile_slot(&mut self, unit: usize) -> *mut usize {
            &mut **self.tile.get_mut(&unit).unwrap()
        }
        fn player_slot(&mut self, unit: usize) -> *mut usize {
            &mut **self.owner.get_mut(&unit).unwrap()
        }
        fn dummy_head(&mut self, link: usize) -> usize {
            if self.heads.contains_key(&link) {
                link
            } else {
                0
            }
        }
        fn head_first_slot(&mut self, head: usize) -> *mut usize {
            &mut **self.heads.get_mut(&head).unwrap()
        }
        fn link_new(&mut self) -> usize {
            self.links.push(Box::new([0, 0]));
            self.links.last_mut().unwrap().as_mut_ptr() as usize
        }
        fn link_delete(&mut self, link: usize) {
            self.note(format!("link_delete({})", link != 0));
        }
        fn store_linked(&mut self, unit: usize) {
            self.note(format!("store_linked({unit})"));
        }
        fn store_remove(&mut self, unit: usize) {
            self.note(format!("store_remove({unit})"));
        }
        fn unregister(&mut self, unit: usize) {
            self.note(format!("unregister({unit})"));
        }
        fn map_tile(&mut self, _map: usize, x: c_long, _y: c_long) -> usize {
            if x < 0 {
                0
            } else {
                0x77
            }
        }
        fn sight_on(&mut self, unit: usize) {
            self.note(format!("sight_on({unit})"));
        }
        fn sight_off(&mut self, unit: usize) {
            self.note(format!("sight_off({unit})"));
        }
        fn unit_reference(&mut self, unit: usize) {
            self.note(format!("reference({unit})"));
        }
        fn unit_dereference(&mut self, unit: usize) {
            self.note(format!("dereference({unit})"));
        }
        fn unit_invalidate(&mut self, unit: usize) {
            self.note(format!("invalidate({unit})"));
            self.fields.get_mut(&unit).unwrap().mIsValid = false;
        }
        fn unit_repair_pointers(&mut self, unit: usize, _f: c_ulong, _s: c_ulong) {
            self.note(format!("repair({unit})"));
        }
        fn register(&mut self, unit: usize) {
            self.note(format!("register({unit})"));
        }
        fn store_insert(&mut self, unit: usize, id: c_ulong, _p: usize) {
            self.note(format!("store_insert({unit}, {id})"));
        }
        fn player_fields(&mut self, _p: usize) -> *mut EeUnitsRtsPlayerFields {
            &mut *self.player
        }
        fn buckets(&mut self, _p: usize) -> *mut EeRtsUnitBucket {
            self.buckets.as_mut_ptr()
        }
        fn first_slot(&mut self, _p: usize) -> *mut usize {
            &mut *self.first
        }
        fn last_slot(&mut self, _p: usize) -> *mut usize {
            &mut *self.last
        }
        fn pending_size(&mut self, _p: usize) -> usize {
            self.pending.len()
        }
        fn pending_at(&mut self, _p: usize, i: usize) -> usize {
            self.pending[i]
        }
        fn pending_clear(&mut self, _p: usize) {
            self.pending.clear();
        }
        fn killed_size(&mut self, _p: usize) -> usize {
            self.killed.len()
        }
        fn killed_at(&mut self, _p: usize, i: usize) -> usize {
            self.killed[i]
        }
        fn killed_clear(&mut self, _p: usize) {
            self.killed.clear();
        }
        fn killed_push(&mut self, _p: usize, unit: usize) {
            self.killed.push(unit);
        }
        fn add_unit_virtual(&mut self, p: usize, unit: usize, keep: bool) {
            add_unit(self, p, unit, keep);
        }
        fn release_pending_virtual(&mut self, p: usize) {
            release_pending_units(self, p);
        }
        fn removing_unit(&mut self, _p: usize, unit: usize) {
            self.note(format!("removing({unit})"));
        }
        fn unit_removed(&mut self, _p: usize, unit: usize) {
            self.note(format!("removed({unit})"));
        }
        fn removals_done(&mut self, _p: usize) {
            self.note("removals_done".into());
        }
        fn save_unit(&mut self, _p: usize, unit: usize, _a: c_ulong, _b: c_ulong, _f: usize) -> c_long {
            self.note(format!("save({unit})"));
            0
        }
        fn name_checksum(&mut self, _p: usize, _c: usize) {
            self.note("name_checksum".into());
        }
        fn checksum_add(&mut self, _c: usize, name: *const c_char, value: c_ulong) {
            self.note(format!("{}={value}", unsafe { core::ffi::CStr::from_ptr(name) }.to_string_lossy()));
        }
        fn write_ulong(&mut self, _f: usize, value: c_ulong) -> c_long {
            if value != 0 {
                self.note(format!("count({value})"));
            }
            0
        }
    }

    #[test]
    fn ids_buckets_and_the_owner_list() {
        let mut w = World::new();
        w.player.mPlayerID = 2;
        // units 0x10.. get ids 2<<24 | 0, 1, ...: buckets 0, 1, then 0x80 wraps to bucket 0
        for u in [0x10, 0x11, 0x12] {
            w.unit(u);
        }
        w.player.mNextUniqueID = 0x7f;
        add_unit(&mut w, 1, 0x10, false); // id ..7f -> bucket 0x7f
        add_unit(&mut w, 1, 0x11, false); // id ..80 -> bucket 0
        w.fields.get_mut(&0x12).unwrap().mUnitID = 0x0200_00ff; // bucket 0x7f, kept
        add_unit(&mut w, 1, 0x12, true);
        assert_eq!(w.fields[&0x10].mUnitID, 0x0200_007f);
        assert_eq!(w.fields[&0x11].mUnitID, 0x0200_0080);
        assert_eq!(w.player.mNextUniqueID, 0x81);
        assert_eq!(w.player.mUnitCount, 3);
        // 0x12 joins bucket 0x7f's chain after its first unit
        assert_eq!(w.order(), [0x10, 0x12, 0x11]);
        assert_eq!(*w.last, 0x11);
        assert_eq!((w.buckets[0x7f].first, w.buckets[0x7f].count), (0x10, 2));
        assert_eq!(player_unit(&mut w, 1, 0x0200_00ff), 0x12);
        assert_eq!(player_unit(&mut w, 1, 0x0200_0080), 0x11);
        assert_eq!(player_unit(&mut w, 1, 0x0200_0001), 0);
        // save: bucket 0 (one unit), bucket 0x7f: the first, then the chain's last back to the second
        w.log.borrow_mut().clear();
        assert_eq!(save_units(&mut w, 1, 0, 0, 9), 0);
        assert_eq!(w.log.borrow().join(" "), "count(1) save(17) count(2) save(16) save(18)");
        // kill the middle one
        kill_unit(&mut w, 1, 0x12, false);
        kill_unit(&mut w, 1, 0x12, false);
        assert_eq!(w.killed, [0x12]);
        w.player.mSaveVisMap = true;
        w.log.borrow_mut().clear();
        remove_killed_units(&mut w, 1);
        assert_eq!(w.order(), [0x10, 0x11]);
        assert_eq!(w.buckets[0x7f].count, 1);
        assert_eq!(w.player.mUnitCount, 2);
        assert_eq!(
            w.log.borrow().join(" "),
            "removing(18) store_remove(18) removed(18) invalidate(18) dereference(18) removals_done"
        );
        // checksum: 2 * 10 + 2 + 0x81
        w.log.borrow_mut().clear();
        player_checksum(&mut w, 1, 5);
        assert_eq!(w.log.borrow().join(" "), "name_checksum RTSPlayer-HashChecksum=0 RTSPlayer=151");
    }

    #[test]
    fn the_tile_chain_links_after_a_head_or_a_unit() {
        let mut w = World::new();
        w.heads.insert(0x900, Box::new(0));
        for u in [0x10, 0x11, 0x12] {
            w.unit(u);
            initialize(&mut w, u, 1);
            w.fields.get_mut(&u).unwrap().mIsValid = true;
        }
        link_after(&mut w, 0x10, 0x900);
        link_after(&mut w, 0x11, 0x900); // first again: 0x11, 0x10
        link_after(&mut w, 0x12, 0x11); // 0x11, 0x12, 0x10
        let chain = |w: &mut World| {
            let mut v = Vec::new();
            let mut u = *w.heads[&0x900];
            while u != 0 {
                v.push(u);
                u = unsafe { (*link(w, u))[NEXT] };
            }
            v
        };
        assert_eq!(chain(&mut w), [0x11, 0x12, 0x10]);
        unsafe { check_unit_list(&mut w, 0x900, c"test".as_ptr()) };
        unlink(&mut w, 0x11);
        assert_eq!(chain(&mut w), [0x12, 0x10]);
        assert_eq!(unsafe { (*link(&mut w, 0x12))[PREV] }, 0x900);
        invalidate(&mut w, 0x10);
        assert_eq!(chain(&mut w), [0x12]);
        assert!(!w.fields[&0x10].mIsValid);
        free_node(&mut w, 0x10);
        assert!(w.fields[&0x10].mIsNodeFreed);
        assert_eq!(*w.node[&0x10], 0);
        // the tile
        w.fields.get_mut(&0x12).unwrap().mTileX = 4;
        w.fields.get_mut(&0x12).unwrap().mTileY = 5;
        update_tile(&mut w, 0x12, 1);
        assert_eq!(*w.tile[&0x12], 0x77);
        w.fields.get_mut(&0x12).unwrap().mTileX = -3;
        update_tile(&mut w, 0x12, 1);
        assert_eq!((*w.tile[&0x12], w.fields[&0x12].mTileX), (0, OFF_MAP));
    }
}
