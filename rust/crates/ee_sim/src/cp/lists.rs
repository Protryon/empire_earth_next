//! The computer player's lists as Rust's (the AI Thread cluster, round 2): the resource centres (`EECPResourceCenter`
//! whole: its members are [`CpCenter`], its members' bodies the forwarders below with their PLCE records), the
//! favourite types' history, the units to kill and the danger zones. `EEComputerPlayer`'s record holds them
//! ([`super::fields::EeCpFields::mLists`], made by `ee_cp_lists_new` after `Init`).
//!
//! A centre is a box at a fixed address: its address is the handle the bodies (`resource_center`) and the C++ hand
//! round, as the `EECPResourceCenter*` was. The forwarders keep the C++ ones' PLCE scopes (TEST ONLY), through the
//! host's capture operations (`PlaceHost::scope_*`).

// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::os::raw::{c_long, c_ulong};

use super::fields::EeCpFields;
use super::file::{SaveFile, OK};
use super::place_host::{PlaceHost, Tile};
use super::resource_center as rc;

/// `EEUnitHandle`'s none.
pub const EE_CP_LISTS_NONE: u32 = 0xffff_ffff;

// the PLCE methods of the centres (ee/EECPPlaceCapture.h)
pub const EE_CP_PLCE_CENTER_ADD_UNIT: u32 = 60;
pub const EE_CP_PLCE_CENTER_CONTAINS: u32 = 61;
pub const EE_CP_PLCE_CENTER_ADOPT_DROP_SITE: u32 = 62;
pub const EE_CP_PLCE_CENTER_LOAD: u32 = 68;
pub const EE_CP_PLCE_CENTER_RESTORE: u32 = 69;
pub const EE_CP_PLCE_CENTER_IS_WITHIN_RADIUS: u32 = 70;
pub const EE_CP_PLCE_CENTER_SAVE: u32 = 71;
pub const EE_CP_PLCE_CENTER_SET_UNIT: u32 = 72;
pub const EE_CP_PLCE_CENTER_SET_ACTIVE: u32 = 73;
pub const EE_CP_PLCE_CENTER_RELEASE_UNIT: u32 = 74;
pub const EE_CP_PLCE_CENTER_GET_CENTER: u32 = 75;

/// A resource centre (`EECPResourceCenter`): its resource units (handles, in order; referenced), its main unit (a
/// handle; referenced), whether it is active.
#[derive(Clone, Debug, PartialEq)]
pub struct CpCenter {
    pub units: Vec<u32>,
    pub main: u32,
    pub active: bool,
}

impl Default for CpCenter {
    fn default() -> Self {
        CpCenter { units: Vec::new(), main: EE_CP_LISTS_NONE, active: false }
    }
}

/// A danger zone: a tile and the game time it was noted.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCpDangerZone {
    pub x: c_long,
    pub y: c_long,
    pub time: c_ulong,
}

/// The computer player's lists.
#[derive(Default)]
pub struct EeCpLists {
    /// `mResourceCenters`
    pub centers: Vec<Box<CpCenter>>,
    /// `mFavoriteTypeHistory`: (unit type, epoch)
    pub history: Vec<(c_ulong, c_long)>,
    /// `mUnitsToKill` (handles; referenced)
    pub kill: Vec<u32>,
    /// `mDangerZones`
    pub zones: Vec<EeCpDangerZone>,
}

/// The record's lists.
///
/// # Safety
/// `p` is a live record whose lists were made; no other borrow of them is live.
#[allow(clippy::mut_from_ref)]
pub unsafe fn lists<'a>(p: *const EeCpFields) -> &'a mut EeCpLists {
    // SAFETY: the caller's contract
    unsafe { &mut *((*p).mLists as usize as *mut EeCpLists) }
}

/// A centre by its handle.
///
/// # Safety
/// `c` is a live centre's handle; no other borrow of it is live.
#[allow(clippy::mut_from_ref)]
pub unsafe fn center<'a>(c: u64) -> &'a mut CpCenter {
    // SAFETY: the caller's contract
    unsafe { &mut *(c as usize as *mut CpCenter) }
}

pub fn handle_of(c: &CpCenter) -> u64 {
    c as *const CpCenter as usize as u64
}

/// A new centre, not listed (`new EECPResourceCenter`): its handle; [`list_center`] takes it.
pub fn new_center() -> u64 {
    Box::into_raw(Box::<CpCenter>::default()) as usize as u64
}

/// Lists a centre made by [`new_center`] at the end or the front.
///
/// # Safety
/// `c` came from [`new_center`] and is not listed yet; `p` as [`lists`].
pub unsafe fn list_center(p: *const EeCpFields, c: u64, front: bool) {
    // SAFETY: the caller's contract
    let (l, b) = unsafe { (lists(p), Box::from_raw(c as usize as *mut CpCenter)) };
    if front {
        l.centers.insert(0, b);
    } else {
        l.centers.push(b);
    }
}

/// The centres' handles, in order.
///
/// # Safety
/// As [`lists`].
pub unsafe fn handles(p: *const EeCpFields) -> Vec<u64> {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.centers.iter().map(|c| handle_of(c)).collect()
}

// ---- the forwarders (the C++ members' PLCE scopes: the scope's words only when it records)

struct Scope(u64);

fn open<H: PlaceHost + ?Sized>(h: &mut H, method: u32) -> Scope {
    Scope(h.scope_open(method))
}

fn word<H: PlaceHost + ?Sized>(h: &mut H, s: &Scope, w: i64) {
    h.scope_word(s.0, w as u32);
}

/// `State(center)`: its units' ids, its main unit's id (not the active flag).
fn state<H: PlaceHost + ?Sized>(h: &mut H, s: &Scope, c: u64) {
    if !h.scope_on(s.0) {
        return;
    }
    // SAFETY: a live centre; copied out before the host calls
    let (units, main) = unsafe { (center(c).units.clone(), center(c).main) };
    word(h, s, units.len() as i64);
    for id in units {
        word(h, s, i64::from(id));
    }
    word(h, s, i64::from(main));
}

fn close<H: PlaceHost + ?Sized>(h: &mut H, s: Scope) {
    h.scope_close(s.0);
}

/// `AddUnit`: the unit referenced and listed.
pub fn add_unit<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) {
    let s = open(h, EE_CP_PLCE_CENTER_ADD_UNIT);
    h.scope_unit(s.0, unit);
    h.unit_reference(unit);
    let handle = h.unit_handle(unit);
    // SAFETY: a live centre
    unsafe { center(c) }.units.push(handle);
    state(h, &s, c);
    close(h, s);
}

/// `EECPResourceCenter(player, unit)`: a new centre (not listed) holding the unit if any.
pub fn create<H: PlaceHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    let c = new_center();
    if unit != 0 {
        add_unit(h, c, unit);
    }
    c
}

/// `Contains`.
pub fn contains<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) -> bool {
    let s = open(h, EE_CP_PLCE_CENTER_CONTAINS);
    h.scope_unit(s.0, unit);
    let r = rc::contains(h, c, unit);
    word(h, &s, i64::from(r));
    close(h, s);
    r
}

/// `AdoptDropSite`: the drop site becomes the main building of a centre with none within the drop-off distance of
/// one of its units.
pub fn adopt_drop_site<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) {
    let s = open(h, EE_CP_PLCE_CENTER_ADOPT_DROP_SITE);
    h.scope_unit(s.0, unit);
    state(h, &s, c);
    if rc::adopt_drop_site(h, c, unit) {
        let handle = h.unit_handle(unit);
        // SAFETY: a live centre
        unsafe { center(c) }.main = handle;
        h.unit_reference(unit);
    }
    state(h, &s, c);
    close(h, s);
}

/// `GetCenter` (`tile` in and out).
pub fn get_center<H: PlaceHost + ?Sized>(h: &mut H, c: u64, tile: &mut Tile) {
    let s = open(h, EE_CP_PLCE_CENTER_GET_CENTER);
    state(h, &s, c);
    // SAFETY: a live centre
    let (has_units, main) = unsafe { (!center(c).units.is_empty(), center(c).main) };
    let set = has_units || h.resolve(main) != 0;
    rc::get_center(h, c, tile);
    word(h, &s, if set { i64::from(tile[0]) } else { 0 });
    word(h, &s, if set { i64::from(tile[1]) } else { 0 });
    close(h, s);
}

/// `IsWithinRadius`.
pub fn is_within_radius<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) -> bool {
    let s = open(h, EE_CP_PLCE_CENTER_IS_WITHIN_RADIUS);
    h.scope_unit(s.0, unit);
    state(h, &s, c);
    let r = rc::is_within_radius(h, c, unit);
    word(h, &s, i64::from(r));
    close(h, s);
    r
}

/// `SetUnit`: the main building (the reference moved from the old one).
pub fn set_unit<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) {
    let s = open(h, EE_CP_PLCE_CENTER_SET_UNIT);
    h.scope_unit(s.0, unit);
    // SAFETY: a live centre
    let main = unsafe { center(c) }.main;
    let old = h.resolve(main);
    if old != 0 {
        h.unit_dereference(old);
    }
    let handle = if unit != 0 { h.unit_handle(unit) } else { EE_CP_LISTS_NONE };
    // SAFETY: as above
    unsafe { center(c) }.main = handle;
    if unit != 0 {
        h.unit_reference(unit);
    }
    state(h, &s, c);
    close(h, s);
}

/// `ReleaseUnit`: the main building released when it is the unit.
pub fn release_unit<H: PlaceHost + ?Sized>(h: &mut H, c: u64, unit: u64) {
    let s = open(h, EE_CP_PLCE_CENTER_RELEASE_UNIT);
    h.scope_unit(s.0, unit);
    // SAFETY: a live centre
    let main = unsafe { center(c) }.main;
    let p = h.resolve(main);
    if p == unit {
        h.unit_dereference(p);
        // SAFETY: as above
        unsafe { center(c) }.main = EE_CP_LISTS_NONE;
    }
    state(h, &s, c);
    close(h, s);
}

/// `SetActive`: activating a centre also works its centre out (the result unused).
pub fn set_active<H: PlaceHost + ?Sized>(h: &mut H, c: u64, active: bool, arg: c_long) {
    let s = open(h, EE_CP_PLCE_CENTER_SET_ACTIVE);
    word(h, &s, i64::from(active));
    word(h, &s, i64::from(arg));
    // SAFETY: a live centre
    unsafe { center(c) }.active = active;
    if active {
        let mut t: Tile = [0, 0];
        rc::get_center(h, c, &mut t);
    }
    state(h, &s, c);
    close(h, s);
}

/// `Restore`.
pub fn restore<H: PlaceHost + ?Sized>(h: &mut H, c: u64, major: u32, minor: u32) -> i32 {
    let s = open(h, EE_CP_PLCE_CENTER_RESTORE);
    word(h, &s, i64::from(major));
    word(h, &s, i64::from(minor));
    state(h, &s, c);
    let r = rc::restore(h, c);
    word(h, &s, i64::from(r));
    state(h, &s, c);
    close(h, s);
    r
}

/// `Save`.
pub fn save<H: PlaceHost + ?Sized>(h: &mut H, c: u64, major: u32, minor: u32, f: &mut SaveFile) -> i32 {
    let s = open(h, EE_CP_PLCE_CENTER_SAVE);
    word(h, &s, i64::from(major));
    word(h, &s, i64::from(minor));
    state(h, &s, c);
    let r = rc::save(h, f, c);
    word(h, &s, i64::from(r));
    close(h, s);
    r
}

/// `Load` into a new centre (listed loaded or not, as the C++ pushed it): the read's result.
///
/// # Safety
/// `p` as [`lists`].
pub unsafe fn load<H: PlaceHost + ?Sized>(
    h: &mut H,
    p: *const EeCpFields,
    major: u32,
    minor: u32,
    f: &mut SaveFile,
) -> i32 {
    let c = new_center();
    let s = open(h, EE_CP_PLCE_CENTER_LOAD);
    word(h, &s, i64::from(major));
    word(h, &s, i64::from(minor));
    // SAFETY: the new centre
    let r = rc::load_into(f, minor, unsafe { center(c) });
    word(h, &s, i64::from(r));
    state(h, &s, c);
    // SAFETY: as above
    let active = unsafe { center(c) }.active;
    word(h, &s, if minor >= rc::CP_ACTIVE_FLAG_VERSION && r == OK { i64::from(active) } else { 2 });
    close(h, s);
    // SAFETY: the caller's contract
    unsafe { list_center(p, c, false) };
    r
}

// ---- the player's loops over its centres

/// `AdoptDropSite` by every centre (`UnitCompleted`, `UnitsSwapped`).
///
/// # Safety
/// As [`lists`].
pub unsafe fn adopt_by_all<H: PlaceHost + ?Sized>(h: &mut H, p: *const EeCpFields, unit: u64) {
    // SAFETY: the caller's contract (the count read once, the centre at i read each time)
    let n = unsafe { lists(p) }.centers.len();
    for i in 0..n {
        // SAFETY: as above
        let c = handle_of(&unsafe { lists(p) }.centers[i]);
        adopt_drop_site(h, c, unit);
    }
}

/// `ReleaseUnit` by every centre (`UnitTaken`).
///
/// # Safety
/// As [`lists`].
pub unsafe fn release_by_all<H: PlaceHost + ?Sized>(h: &mut H, p: *const EeCpFields, unit: u64) {
    // SAFETY: the caller's contract
    let n = unsafe { lists(p) }.centers.len();
    for i in 0..n {
        // SAFETY: as above
        let c = handle_of(&unsafe { lists(p) }.centers[i]);
        release_unit(h, c, unit);
    }
}

/// `Restore` of every centre until one fails (`RestoreManagers`).
///
/// # Safety
/// As [`lists`].
pub unsafe fn restore_all<H: PlaceHost + ?Sized>(h: &mut H, p: *const EeCpFields, major: u32, minor: u32) -> i32 {
    let mut r = OK;
    // SAFETY: the caller's contract
    let n = unsafe { lists(p) }.centers.len();
    for i in 0..n {
        if r != OK {
            break;
        }
        // SAFETY: as above
        let c = handle_of(&unsafe { lists(p) }.centers[i]);
        r = restore(h, c, major, minor);
    }
    r
}

/// `DeleteVectorContents(mResourceCenters)`: every centre's units dereferenced (not its main unit, as the
/// destructor did), in order.
///
/// # Safety
/// As [`lists`].
pub unsafe fn delete_centers<H: PlaceHost + ?Sized>(h: &mut H, p: *const EeCpFields) {
    // SAFETY: the caller's contract
    let centers = std::mem::take(&mut unsafe { lists(p) }.centers);
    for c in centers {
        for &id in &c.units {
            let u = h.resolve(id);
            h.unit_dereference(u);
        }
    }
}

// ---- TEST ONLY (PLCE, CPEV): the digests of the lists

fn fnv(h: &mut u64, v: i64) {
    for b in (v as u32).to_le_bytes() {
        *h = (*h ^ u64::from(b)).wrapping_mul(0x100000001b3);
    }
}

const FNV_BASIS: u64 = 0xcbf29ce484222325;

/// `Centers(player)`: the centres' count and an FNV-1a 64 of their units' ids and main units' ids (low, high).
pub fn centers_digest(l: &EeCpLists) -> [u32; 3] {
    let mut h = FNV_BASIS;
    for c in &l.centers {
        fnv(&mut h, c.units.len() as i64);
        for &id in &c.units {
            fnv(&mut h, i64::from(id));
        }
        fnv(&mut h, i64::from(c.main));
    }
    [l.centers.len() as u32, h as u32, (h >> 32) as u32]
}

/// `Player(player)`'s lists part: the history's count and FNV-1a (low), the units to kill's and the danger zones'
/// counts, the centres' count and FNV-1a (low) of their units, main units and active flags.
pub fn lists_digest(l: &EeCpLists) -> [u32; 6] {
    let mut h = FNV_BASIS;
    for &(first, second) in &l.history {
        fnv(&mut h, first as i64);
        fnv(&mut h, second as i64);
    }
    let history = h as u32;
    let mut h = FNV_BASIS;
    for c in &l.centers {
        fnv(&mut h, c.units.len() as i64);
        for &id in &c.units {
            fnv(&mut h, i64::from(id));
        }
        fnv(&mut h, i64::from(c.main));
        fnv(&mut h, i64::from(c.active));
    }
    [l.history.len() as u32, history, l.kill.len() as u32, l.zones.len() as u32, l.centers.len() as u32, h as u32]
}

// ---- the C ABI of the plain lists (`p`: a live record with lists, no other borrow of them live)

/// A favourite types' history entry: (unit type, epoch).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCpHistoryEntry {
    pub unit_type: c_ulong,
    pub epoch: c_long,
}

/// The history's length.
///
/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_history_count(p: *const EeCpFields) -> usize {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.history.len()
}

/// History entry `i`.
///
/// # Safety
/// As [`lists`]; `i` below the length.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_history_at(p: *const EeCpFields, i: usize) -> EeCpHistoryEntry {
    // SAFETY: the caller's contract
    let (unit_type, epoch) = unsafe { lists(p) }.history[i];
    EeCpHistoryEntry { unit_type, epoch }
}

/// The history set to `count` entries.
///
/// # Safety
/// As [`lists`]; `entries` holds `count` entries (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_history_set(p: *const EeCpFields, entries: *const EeCpHistoryEntry, count: usize) {
    let list = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(entries, count) } };
    // SAFETY: the caller's contract
    unsafe { lists(p) }.history = list.iter().map(|e| (e.unit_type, e.epoch)).collect();
}

/// A unit to kill (its handle; the C++ took the reference).
///
/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_kill_push(p: *const EeCpFields, handle: u32) {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.kill.push(handle);
}

/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_kill_count(p: *const EeCpFields) -> usize {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.kill.len()
}

/// # Safety
/// As [`lists`]; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_kill_at(p: *const EeCpFields, i: usize) -> u32 {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.kill[i]
}

/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_kill_clear(p: *const EeCpFields) {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.kill.clear();
}

/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_zone_count(p: *const EeCpFields) -> usize {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.zones.len()
}

/// # Safety
/// As [`lists`]; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_zone_at(p: *const EeCpFields, i: usize) -> EeCpDangerZone {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.zones[i]
}

/// A danger zone added.
///
/// # Safety
/// As [`lists`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_zone_push(p: *const EeCpFields, zone: EeCpDangerZone) {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.zones.push(zone);
}

/// Danger zone `i` erased.
///
/// # Safety
/// As [`lists`]; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_zone_erase(p: *const EeCpFields, i: usize) {
    // SAFETY: the caller's contract
    unsafe { lists(p) }.zones.remove(i);
}

/// The danger zones set to `count` zones (`Clear`: none; a loaded game's).
///
/// # Safety
/// As [`lists`]; `zones` holds `count` zones (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_zones_set(p: *const EeCpFields, zones: *const EeCpDangerZone, count: usize) {
    let list = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(zones, count) } };
    // SAFETY: the caller's contract
    unsafe { lists(p) }.zones = list.to_vec();
}
