//! The player's own lists as a Rust object (`EEPlayer`'s private containers, EEPlayer.cpp; the game core lane, round
//! 5): the stranded units, the units waiting to repatrol, the stand-in units' loaded ids, the saved construction sites'
//! and wonders' ids (until `RepairPointers`), the unit classes whose media `LoadAllClassMedia` loaded, the player's
//! loaded media, and the units taken off the map with their records (`EEWUnitsOffMapInfo`, now
//! [`EeUnitsOffMapInfo`], with its Load, Save and RepairPointers). The player/tech lane, round 2: the player's other
//! containers too (the towns, the herds, the allies, the sight sharers, the patrol groups, the construction sites, the
//! wonders and the media warnings; were `std::vector`s and `UReferenceContainer<EEComplexUnit*>`s on EEPlayer).
//!
//! The units are `RTSUnit*` addresses; their reference counts are the C++'s (`ee_units_lists_host`: reference,
//! dereference, a unit's id, the world's unit by id). Kept: the stranded and repatrol lists add a unit once (a
//! reference each); the id lists push without a test; the media list skips 0; a removal takes the first match; the
//! off-map lookups and removals walk from the last record; a record's load stores the id it read (the original's
//! `reinterpret_cast`) whatever the read returned; a record's save writes -1 for no unit.

#![allow(non_snake_case)]

use crate::units::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// `EEWUnitsOffMapInfo`: a unit taken off the map (its C++ names and types; the callers set `mReason`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsOffMapInfo {
    /// the tile it left (`EETileLocation`: x, y; saved)
    pub mLocation: [c_long; 2],
    /// the unit (`RTSUnit*`, referenced; its id while loading)
    pub mUnit: *mut c_void,
    /// the game time it left (saved)
    pub mTime: c_ulong,
    /// why (1..7: the transports, the calamities, the scenario's effect; saved)
    pub mReason: c_long,
}

impl Default for EeUnitsOffMapInfo {
    fn default() -> Self {
        EeUnitsOffMapInfo { mLocation: [0; 2], mUnit: std::ptr::null_mut(), mTime: 0, mReason: 0 }
    }
}

/// The lists (`ee_units_lists_*`'s `list`).
pub const EE_UNITS_LISTS_STRANDED: u32 = 0;
pub const EE_UNITS_LISTS_REPATROL: u32 = 1;
pub const EE_UNITS_LISTS_LOADED_IDS: u32 = 2;
pub const EE_UNITS_LISTS_SITE_IDS: u32 = 3;
pub const EE_UNITS_LISTS_WONDER_IDS: u32 = 4;
pub const EE_UNITS_LISTS_SOUND_CLASSES: u32 = 5;
pub const EE_UNITS_LISTS_MEDIA: u32 = 6;
/// `mTowns` (`EETown*`, null entries kept), `mHerds` (`EEHerd*`)
pub const EE_UNITS_LISTS_TOWNS: u32 = 7;
pub const EE_UNITS_LISTS_HERDS: u32 = 8;
/// `mAllies`, `mSightSharers` (`EEPlayer*`)
pub const EE_UNITS_LISTS_ALLIES: u32 = 9;
pub const EE_UNITS_LISTS_SIGHT_SHARERS: u32 = 10;
/// `mPatrolGroups` (`EEPPatrolGroup*`, referenced)
pub const EE_UNITS_LISTS_PATROLS: u32 = 11;
/// `mConstructionSites`, `mWonders` (the original's `UReferenceContainer<EEComplexUnit*>`: referenced units)
pub const EE_UNITS_LISTS_SITES: u32 = 12;
pub const EE_UNITS_LISTS_WONDERS: u32 = 13;
const LISTS: usize = 14;

/// The host's operations (`ee_units_lists_host(op, value)`).
pub const EE_UNITS_LISTS_HOST_REFERENCE: i32 = 0;
pub const EE_UNITS_LISTS_HOST_DEREFERENCE: i32 = 1;
/// the unit's `GetUnitID()`
pub const EE_UNITS_LISTS_HOST_UNIT_ID: i32 = 2;
/// `gWorld.GetUnit(id)` (0 none)
pub const EE_UNITS_LISTS_HOST_UNIT_BY_ID: i32 = 3;
/// a patrol group referenced, dereferenced (patrol_group.rs: answered in Rust)
pub const EE_UNITS_LISTS_HOST_PG_REFERENCE: i32 = 4;
pub const EE_UNITS_LISTS_HOST_PG_DEREFERENCE: i32 = 5;

/// The C++ side: the units' reference counts and ids.
pub trait ListsHost {
    fn call(&mut self, op: i32, value: u64) -> u64;
}

/// The player's lists.
#[derive(Debug, Default)]
pub struct EeUnitsPlayerLists {
    pub lists: [Vec<u64>; LISTS],
    pub off_map: Vec<Box<EeUnitsOffMapInfo>>,
    /// `mMediaWarnings` (`DumpMedia`'s lines)
    pub warnings: Vec<std::ffi::CString>,
}

/// Lists whose entries hold a unit's reference, a patrol group's (the host's reference and dereference).
fn references(list: u32) -> Option<(i32, i32)> {
    match list {
        EE_UNITS_LISTS_STRANDED | EE_UNITS_LISTS_REPATROL | EE_UNITS_LISTS_SITES | EE_UNITS_LISTS_WONDERS => {
            Some((EE_UNITS_LISTS_HOST_REFERENCE, EE_UNITS_LISTS_HOST_DEREFERENCE))
        }
        EE_UNITS_LISTS_PATROLS => Some((EE_UNITS_LISTS_HOST_PG_REFERENCE, EE_UNITS_LISTS_HOST_PG_DEREFERENCE)),
        _ => None,
    }
}

/// The containers' operations the player's bodies name ([`EeUnitsPlayerLists::apply`]; the answer a u64).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ListOp {
    /// `size()`
    Len,
    /// entry `i` (0 past the end)
    At(usize),
    /// [`EeUnitsPlayerLists::add`] (the referenced lists' `Add`: referenced, pushed)
    Add(u64),
    /// whether it holds the value (`std::find`, `Contains`)
    Has(u64),
    /// the first match out, its reference released (`Remove`; the plain lists' `erase(find)`): whether it was there
    Remove(u64),
    /// `erase(begin() + i)` (no reference released)
    EraseAt(usize),
    /// `UReferenceContainer::Replace(old, new)`: the first `old`'s slot gets `new`, its reference moved
    Replace(u64, u64),
    /// emptied; `release`: each entry's reference released ([`EeUnitsPlayerLists::clear`])
    Clear(bool),
    /// `[i] = v`
    Set(usize, u64),
    /// `resize(n, NULL)`
    Resize(usize),
}

fn call_refs(h: &mut dyn ListsHost, list: u32, value: u64, release: bool) {
    if let Some((reference, dereference)) = references(list) {
        h.call(if release { dereference } else { reference }, value);
    }
}

impl EeUnitsPlayerLists {
    /// `AddStrandedUnit`, `AddUnitToRepatrol` (once, referenced), `AddLoadedMedia` (not 0), the ids' push_back.
    /// The construction sites', wonders' and patrol groups' `Add` (referenced, pushed), the other lists' push_back.
    pub fn add(&mut self, h: &mut dyn ListsHost, list: u32, value: u64) -> bool {
        let Some(l) = self.lists.get_mut(list as usize) else { return false };
        if (list == EE_UNITS_LISTS_STRANDED || list == EE_UNITS_LISTS_REPATROL) && l.contains(&value) {
            return false;
        }
        if list == EE_UNITS_LISTS_MEDIA && value == 0 {
            return false;
        }
        call_refs(h, list, value, false);
        l.push(value);
        true
    }
    /// One [`ListOp`] on list `list` (0 for a list past the last).
    pub fn apply(&mut self, h: &mut dyn ListsHost, list: u32, op: ListOp) -> u64 {
        if self.lists.get(list as usize).is_none() {
            return 0;
        }
        if let ListOp::Add(v) = op {
            return u64::from(self.add(h, list, v));
        }
        if let ListOp::Clear(release) = op {
            self.clear(h, list, release);
            return 0;
        }
        let l = &mut self.lists[list as usize];
        match op {
            ListOp::Len => l.len() as u64,
            ListOp::At(i) => l.get(i).copied().unwrap_or(0),
            ListOp::Has(v) => u64::from(l.contains(&v)),
            ListOp::Remove(v) => match l.iter().position(|&x| x == v) {
                Some(i) => {
                    l.remove(i);
                    call_refs(h, list, v, true);
                    1
                }
                None => 0,
            },
            ListOp::EraseAt(i) => {
                if i < l.len() {
                    l.remove(i);
                }
                0
            }
            ListOp::Replace(old, new) => match l.iter().position(|&x| x == old) {
                Some(i) => {
                    l[i] = new;
                    call_refs(h, list, old, true);
                    call_refs(h, list, new, false);
                    1
                }
                None => 0,
            },
            ListOp::Set(i, v) => {
                if let Some(x) = l.get_mut(i) {
                    *x = v;
                }
                0
            }
            ListOp::Resize(n) => {
                l.resize(n, 0);
                0
            }
            ListOp::Add(_) | ListOp::Clear(_) => 0,
        }
    }
    /// The first `value` out (`RemoveLoadedMedia`'s erase): whether it was there.
    pub fn remove(&mut self, list: u32, value: u64) -> bool {
        let Some(l) = self.lists.get_mut(list as usize) else { return false };
        match l.iter().position(|&v| v == value) {
            Some(i) => {
                l.remove(i);
                true
            }
            None => false,
        }
    }
    /// `HasLoadedUnitID`: std::find.
    pub fn has(&self, list: u32, value: u64) -> bool {
        self.lists.get(list as usize).is_some_and(|l| l.contains(&value))
    }
    /// Emptied; `release`: `ClearUnitsToRepatrol` dereferences its units from the last, the construction sites',
    /// wonders' (`UReferenceContainer::Clear`) and patrol groups' (`EEPDereferencer`) from the first.
    pub fn clear(&mut self, h: &mut dyn ListsHost, list: u32, release: bool) {
        let Some(l) = self.lists.get_mut(list as usize) else { return };
        let mut taken = std::mem::take(l);
        if release {
            if list == EE_UNITS_LISTS_REPATROL {
                taken.reverse();
            }
            for &u in &taken {
                if u != 0 {
                    call_refs(h, list, u, true);
                }
            }
        }
    }

    /// `AddOffMapUnit`: a record of `unit` (referenced) at its tile and the game time.
    pub fn off_map_add(&mut self, h: &mut dyn ListsHost, unit: u64, x: c_long, y: c_long, time: c_ulong) {
        if unit == 0 {
            return;
        }
        h.call(EE_UNITS_LISTS_HOST_REFERENCE, unit);
        self.off_map.push(Box::new(EeUnitsOffMapInfo {
            mLocation: [x, y],
            mUnit: unit as *mut c_void,
            mTime: time,
            mReason: 0,
        }));
    }
    /// `GetOffMapInfo`: the last record of `unit` (none for 0).
    pub fn off_map_find(&mut self, unit: u64) -> Option<&mut EeUnitsOffMapInfo> {
        if unit == 0 {
            return None;
        }
        self.off_map.iter_mut().rev().find(|r| r.mUnit as u64 == unit).map(|b| &mut **b)
    }
    /// `RemoveOffMapUnit`: the last record of `unit` deleted (its reference released).
    pub fn off_map_remove(&mut self, h: &mut dyn ListsHost, unit: u64) {
        if unit == 0 {
            return;
        }
        if let Some(i) = self.off_map.iter().rposition(|r| r.mUnit as u64 == unit) {
            let r = self.off_map.remove(i);
            release(h, &r);
        }
    }
    /// The records deleted (the destructor's and `Clear`'s `EEDeletePointers`: in order, each releasing its unit).
    pub fn off_map_clear(&mut self, h: &mut dyn ListsHost) {
        for r in std::mem::take(&mut self.off_map) {
            release(h, &r);
        }
    }
    /// `RepairPointers`' walk from the last record: each saved id back to its unit (referenced), the records whose
    /// unit is gone deleted.
    pub fn off_map_repair(&mut self, h: &mut dyn ListsHost) {
        for i in (0..self.off_map.len()).rev() {
            let id = self.off_map[i].mUnit as u64 as u32;
            let unit = h.call(EE_UNITS_LISTS_HOST_UNIT_BY_ID, u64::from(id));
            if unit != 0 {
                h.call(EE_UNITS_LISTS_HOST_REFERENCE, unit);
            }
            self.off_map[i].mUnit = unit as *mut c_void;
            if unit == 0 {
                self.off_map.remove(i);
            }
        }
    }
}

/// `EEWUnitsOffMapInfo::ReleaseUnit`
fn release(h: &mut dyn ListsHost, r: &EeUnitsOffMapInfo) {
    if !r.mUnit.is_null() {
        h.call(EE_UNITS_LISTS_HOST_DEREFERENCE, r.mUnit as u64);
    }
}

/// `EEWUnitsOffMapInfo::Load`: the unit's id (stored whatever the read returned), the tile, the time, the reason;
/// the first error.
pub fn off_map_load(r: &mut EeUnitsOffMapInfo, f: &mut dyn PlayerSaveHost) -> i32 {
    let mut id: c_ulong = c_ulong::MAX;
    let mut result = f.read_ulong(&mut id);
    r.mUnit = (id as u32 as usize) as *mut c_void;
    if result != 0 {
        return result;
    }
    result = f.read_long(&mut r.mLocation[0]);
    if result == 0 {
        result = f.read_long(&mut r.mLocation[1]);
    }
    if result != 0 {
        return result;
    }
    result = f.read_ulong(&mut r.mTime);
    if result == 0 {
        result = f.read_long(&mut r.mReason);
    }
    result
}

/// `EEWUnitsOffMapInfo::Save`: the unit's id (-1 none), the tile, the time, the reason; the first error.
pub fn off_map_save(r: &EeUnitsOffMapInfo, h: &mut dyn ListsHost, f: &mut dyn PlayerSaveHost) -> i32 {
    let id =
        if r.mUnit.is_null() { c_ulong::MAX } else { h.call(EE_UNITS_LISTS_HOST_UNIT_ID, r.mUnit as u64) as c_ulong };
    let mut result = f.write_ulong(id);
    if result != 0 {
        return result;
    }
    result = f.write_long(r.mLocation[0]);
    if result == 0 {
        result = f.write_long(r.mLocation[1]);
    }
    if result != 0 {
        return result;
    }
    result = f.write_ulong(r.mTime);
    if result == 0 {
        result = f.write_long(r.mReason);
    }
    result
}

// ---- FFI

#[cfg(not(test))]
extern "C" {
    fn ee_units_lists_host(op: i32, value: u64) -> u64;
}

struct Ffi;

impl ListsHost for Ffi {
    fn call(&mut self, op: i32, value: u64) -> u64 {
        #[cfg(not(test))]
        {
            use crate::units::patrol_group::{ee_units_pg_dereference, ee_units_pg_reference, EeUnitsPatrolGroup};
            let g = value as usize as *mut EeUnitsPatrolGroup;
            // SAFETY: a live patrol group of the player's list; live units through the C++ host (the callers'
            // contract)
            unsafe {
                match op {
                    EE_UNITS_LISTS_HOST_PG_REFERENCE => ee_units_pg_reference(g),
                    EE_UNITS_LISTS_HOST_PG_DEREFERENCE => ee_units_pg_dereference(g),
                    _ => return ee_units_lists_host(op, value),
                }
            }
            return 0;
        }
        #[cfg(test)]
        {
            let _ = (op, value);
            0
        }
    }
}

struct FileHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl PlayerSaveHost for FileHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ file primitives with their own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// Player `p`'s lists (its `mLists`, at the layout's offset).
#[cfg(not(test))]
pub fn player_lists(p: u64) -> *mut EeUnitsPlayerLists {
    // SAFETY: a live player's member, made by its constructor (the callers' contract)
    unsafe { *((p as usize + crate::world::layout::layout().player_lists as usize) as *const *mut EeUnitsPlayerLists) }
}

/// One [`ListOp`] on player `p`'s list `list` (the game's hosts').
#[cfg(not(test))]
pub fn player_op(p: u64, list: u32, op: ListOp) -> u64 {
    // SAFETY: a live player's lists; the units' and patrol groups' counts never reach them
    unsafe { &mut *player_lists(p) }.apply(&mut Ffi, list, op)
}

/// The player host's container operations (player.rs `EE_UNITS_PLAYER_*`: were EEPlayer.cpp's `EEPlayerHost` cases)
/// on player `p`'s lists, `i` the operation's arguments (a player 0 is `p`); None for the others.
#[cfg(not(test))]
pub fn answer_player(p: u64, op: i32, i: &[i64; 8]) -> Option<i64> {
    use crate::units::player_split::{
        EE_UNITS_PLAYER_X_ALLY_ADD, EE_UNITS_PLAYER_X_ALLY_REMOVE, EE_UNITS_PLAYER_X_SHARERS_ERASE,
        EE_UNITS_PLAYER_X_SHARERS_HAS, EE_UNITS_PLAYER_X_SHARERS_PUSH, EE_UNITS_PLAYER_X_WONDERS_ADD,
        EE_UNITS_PLAYER_X_WONDERS_REMOVE,
    };
    use crate::units::player_split_more::{
        EE_UNITS_PLAYER_X_HERDS_ERASE, EE_UNITS_PLAYER_X_HERDS_PUSH, EE_UNITS_PLAYER_X_TOWNS_ERASE,
        EE_UNITS_PLAYER_X_TOWNS_PUSH,
    };
    use crate::units::player_units::EE_UNITS_PLAYER_REMOVE_SITE;
    let or_me = |x: i64| if x != 0 { x as u64 } else { p };
    let (player, list, op) = match op {
        EE_UNITS_PLAYER_REMOVE_SITE => (p, EE_UNITS_LISTS_SITES, ListOp::Remove(i[0] as u64)),
        EE_UNITS_PLAYER_X_ALLY_ADD => (or_me(i[0]), EE_UNITS_LISTS_ALLIES, ListOp::Add(or_me(i[1]))),
        EE_UNITS_PLAYER_X_ALLY_REMOVE => (or_me(i[0]), EE_UNITS_LISTS_ALLIES, ListOp::Remove(or_me(i[1]))),
        EE_UNITS_PLAYER_X_SHARERS_PUSH => (p, EE_UNITS_LISTS_SIGHT_SHARERS, ListOp::Add(or_me(i[0]))),
        EE_UNITS_PLAYER_X_SHARERS_HAS => (p, EE_UNITS_LISTS_SIGHT_SHARERS, ListOp::Has(or_me(i[0]))),
        EE_UNITS_PLAYER_X_SHARERS_ERASE => (p, EE_UNITS_LISTS_SIGHT_SHARERS, ListOp::Remove(or_me(i[0]))),
        EE_UNITS_PLAYER_X_WONDERS_ADD => (p, EE_UNITS_LISTS_WONDERS, ListOp::Add(i[0] as u64)),
        EE_UNITS_PLAYER_X_WONDERS_REMOVE => (p, EE_UNITS_LISTS_WONDERS, ListOp::Remove(i[0] as u64)),
        EE_UNITS_PLAYER_X_TOWNS_PUSH => (p, EE_UNITS_LISTS_TOWNS, ListOp::Add(i[0] as u64)),
        EE_UNITS_PLAYER_X_HERDS_PUSH => (p, EE_UNITS_LISTS_HERDS, ListOp::Add(i[0] as u64)),
        EE_UNITS_PLAYER_X_TOWNS_ERASE => (p, EE_UNITS_LISTS_TOWNS, ListOp::EraseAt(i[0] as usize)),
        EE_UNITS_PLAYER_X_HERDS_ERASE => (p, EE_UNITS_LISTS_HERDS, ListOp::EraseAt(i[0] as usize)),
        _ => return None,
    };
    Some(player_op(player, list, op) as i64)
}

#[cfg(test)]
pub fn answer_player(_: u64, _: i32, _: &[i64; 8]) -> Option<i64> {
    None
}

/// Unit `unit`'s `GetUnitID()` (the C++ host's).
#[cfg(not(test))]
pub fn unit_id(unit: u64) -> u64 {
    Ffi.call(EE_UNITS_LISTS_HOST_UNIT_ID, unit)
}

/// One [`ListOp`] on a lone list `v` (kind `list`; the references' calls dropped): the test hosts'.
#[cfg(test)]
pub fn apply_vec(v: &mut Vec<u64>, list: u32, op: ListOp) -> u64 {
    let mut l = EeUnitsPlayerLists::default();
    l.lists[list as usize] = std::mem::take(v);
    let r = l.apply(&mut Recorder(&mut Vec::new()), list, op);
    *v = std::mem::take(&mut l.lists[list as usize]);
    r
}

/// Player `p`'s list `list`, copied (the game's readers outside the player).
#[cfg(not(test))]
pub fn player_list(p: u64, list: u32) -> Vec<u64> {
    // SAFETY: a live player's lists
    unsafe { &*player_lists(p) }.lists.get(list as usize).cloned().unwrap_or_default()
}

/// The test hosts' lists: per player, with the references' calls (op, value) in order.
#[cfg(test)]
#[derive(Default)]
pub struct FakeLists {
    pub players: std::collections::BTreeMap<u64, EeUnitsPlayerLists>,
    pub refs: Vec<(i32, u64)>,
}

#[cfg(test)]
struct Recorder<'a>(&'a mut Vec<(i32, u64)>);

#[cfg(test)]
impl ListsHost for Recorder<'_> {
    fn call(&mut self, op: i32, value: u64) -> u64 {
        self.0.push((op, value));
        0
    }
}

#[cfg(test)]
impl FakeLists {
    pub fn op(&mut self, p: u64, list: u32, op: ListOp) -> u64 {
        self.players.entry(p).or_default().apply(&mut Recorder(&mut self.refs), list, op)
    }
    /// player `p`'s list `list`
    pub fn list(&mut self, p: u64, list: u32) -> &mut Vec<u64> {
        &mut self.players.entry(p).or_default().lists[list as usize]
    }
}

/// New, empty lists (the constructors').
#[no_mangle]
pub extern "C" fn ee_units_lists_new() -> *mut EeUnitsPlayerLists {
    Box::into_raw(Box::default())
}

/// # Safety
/// `l` came from [`ee_units_lists_new`] (or is null) and is not used after. The lists' references are not released
/// (the destructor releases what the original did first).
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_free(l: *mut EeUnitsPlayerLists) {
    if !l.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(l) });
    }
}

/// [`EeUnitsPlayerLists::add`]
///
/// # Safety
/// `l` is live; a referenced list's `value` is a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_add(l: *mut EeUnitsPlayerLists, list: u32, value: u64) -> bool {
    // SAFETY: the caller's contract; the host only touches the unit's count
    unsafe { &mut *l }.add(&mut Ffi, list, value)
}

/// [`EeUnitsPlayerLists::remove`]
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_remove(l: *mut EeUnitsPlayerLists, list: u32, value: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { &mut *l }.remove(list, value)
}

/// [`EeUnitsPlayerLists::has`]
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_has(l: *const EeUnitsPlayerLists, list: u32, value: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { &*l }.has(list, value)
}

/// The list's length.
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_len(l: *const EeUnitsPlayerLists, list: u32) -> usize {
    // SAFETY: the caller's contract
    unsafe { &*l }.lists.get(list as usize).map_or(0, Vec::len)
}

/// Entry `i` (0 past the end).
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_at(l: *const EeUnitsPlayerLists, list: u32, i: usize) -> u64 {
    // SAFETY: the caller's contract
    unsafe { &*l }.lists.get(list as usize).and_then(|v| v.get(i)).copied().unwrap_or(0)
}

/// [`EeUnitsPlayerLists::clear`]
///
/// # Safety
/// `l` is live; with `release`, its entries are live units.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lists_clear(l: *mut EeUnitsPlayerLists, list: u32, release: bool) {
    // SAFETY: the caller's contract; a dereference never reaches the lists (the list was taken first)
    unsafe { &mut *l }.clear(&mut Ffi, list, release)
}

/// The off-map records: `what` 0 `AddOffMapUnit(unit)` at (`x`, `y`) and `time`, 1 `RemoveOffMapUnit(unit)`, 2 the
/// records deleted, 3 `RepairPointers`' walk.
///
/// # Safety
/// `l` is live; `unit` a live unit (for 0, 1); the world is loaded (for 3).
#[no_mangle]
pub unsafe extern "C" fn ee_units_off_map(
    l: *mut EeUnitsPlayerLists,
    what: u32,
    unit: u64,
    x: c_long,
    y: c_long,
    time: c_ulong,
) {
    // SAFETY: the caller's contract; the host's calls never reach the lists
    let l = unsafe { &mut *l };
    match what {
        0 => l.off_map_add(&mut Ffi, unit, x, y, time),
        1 => l.off_map_remove(&mut Ffi, unit),
        2 => l.off_map_clear(&mut Ffi),
        _ => l.off_map_repair(&mut Ffi),
    }
}

/// `GetOffMapInfo(unit)`: the record (null none), live until the next off-map call.
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_off_map_info(l: *mut EeUnitsPlayerLists, unit: u64) -> *mut EeUnitsOffMapInfo {
    // SAFETY: the caller's contract (a boxed record: its address is stable)
    unsafe { &mut *l }.off_map_find(unit).map_or(std::ptr::null_mut(), |r| r as *mut EeUnitsOffMapInfo)
}

/// The player's save of the records: `what` 0 the count (into `io.i`), 1 record `index`'s Save; its load: 2 the
/// records made (`io.i` of them, empty), 3 record `index`'s Load. The file's primitives through `f` with `ctx`.
///
/// # Safety
/// `l` is live; `f` is valid with `ctx` for the call; `index` is below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_units_off_map_file(
    l: *mut EeUnitsPlayerLists,
    what: u32,
    index: u32,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    io: *mut EeUnitsPlayerSaveIo,
) -> i32 {
    // SAFETY: the caller's contract; the primitives never reach the lists
    let (l, io) = unsafe { (&mut *l, &mut *io) };
    let mut file = FileHost { ctx, f };
    match what {
        0 => {
            io.i = l.off_map.len() as i64;
            0
        }
        1 => l.off_map.get(index as usize).map_or(0, |r| off_map_save(r, &mut Ffi, &mut file)),
        2 => {
            l.off_map.clear();
            l.off_map.resize_with(io.i.max(0) as usize, Box::default);
            0
        }
        _ => l.off_map.get_mut(index as usize).map_or(0, |r| off_map_load(r, &mut file)),
    }
}

#[cfg(test)]
#[path = "player_lists_tests.rs"]
mod tests;
