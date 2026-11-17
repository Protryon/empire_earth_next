//! The towns' saved ids, order, counters and areas (the port's save parts 0.260 and 0.268; R5b.4, the thirty-first
//! round): `EESaveTowns`, `EELoadTowns` and the pending towns a load keeps until each player's `RepairPointers`
//! makes them (`EEMakeSavedTowns`: 0.268 on) or matches the rebuilt towns to them by hull (`EEApplySavedTowns`).
//!
//! The pending towns are a Rust state (one per process: a load is one at a time); the towns themselves stay C++
//! objects (`EETown`), read and made over one host ([`EeUnitsTownFn`]): the file's primitives (player_save.rs's), the
//! towns of a player by slot, a hull vertex, a town's area as bytes, the making and the matching's writes.

use std::collections::BTreeMap;
use std::os::raw::{c_long, c_void};
use std::sync::Mutex;

use crate::units::player_save::{
    EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_LONG,
    EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
    EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};

/// The file's `kErrorOK`.
const OK: i32 = 0;
/// `kErrorBeyondEnd`: an area read short.
pub const EE_UNITS_TOWN_BEYOND_END: i32 = 0xa;

/// `gWorld.mTownIDCounter` (`i`); [`EE_UNITS_TOWN_SET_COUNTER`] sets it.
pub const EE_UNITS_TOWN_COUNTER: i32 = 300;
pub const EE_UNITS_TOWN_SET_COUNTER: i32 = 301;
/// `gWorld.mNumPlayers` (`i`).
pub const EE_UNITS_TOWN_PLAYERS: i32 = 302;
/// The player's town slots (`mTowns.size()`, 0 for no player) as the result.
pub const EE_UNITS_TOWN_SLOTS: i32 = 303;
/// The town in slot `index`: 0 for none, else 1 with its id, hull vertex count, counters, flags and continent.
pub const EE_UNITS_TOWN_GET: i32 = 304;
/// Vertex `io.i` of the hull of the town in slot `index` (`x`, `y`).
pub const EE_UNITS_TOWN_HULL_POINT: i32 = 305;
/// The area of the town in slot `index` as `EEWriteAreaBytes` writes it (`bytes`, `len`: the host's, valid until
/// its next call).
pub const EE_UNITS_TOWN_AREA: i32 = 306;
/// Reads `len` bytes into `bytes` (the result the read's; `io.i` the bytes read); [`EE_UNITS_TOWN_WRITE_BYTES`]
/// writes them.
pub const EE_UNITS_TOWN_READ_BYTES: i32 = 307;
pub const EE_UNITS_TOWN_WRITE_BYTES: i32 = 308;
/// Makes a town of the player with the id, area (`bytes`, `len`), continent, counters and flags: 1, or 0 when the
/// area cannot be read (nothing kept).
pub const EE_UNITS_TOWN_MAKE: i32 = 309;
/// Deletes the towns made so far (one could not be read).
pub const EE_UNITS_TOWN_MAKE_DISCARD: i32 = 310;
/// Gives the towns made their serials, areas, ids and slots, in the order made.
pub const EE_UNITS_TOWN_MAKE_COMMIT: i32 = 311;
/// Sets the town in slot `index`'s id, counters and flags.
pub const EE_UNITS_TOWN_SET_SAVED: i32 = 312;
/// Rebuilds the player's towns by id from its slots.
pub const EE_UNITS_TOWN_REINDEX: i32 = 313;
/// Puts the player's towns in the order of the slots `bytes` lists (`len` i32s).
pub const EE_UNITS_TOWN_REORDER: i32 = 314;
/// A debug line (`bytes`, `len`).
pub const EE_UNITS_TOWN_DEBUG: i32 = 315;

/// One town as the host reads, writes or makes it.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsTownIo {
    /// the file's primitives (player_save.rs's operations)
    pub prim: EeUnitsPlayerSaveIo,
    pub id: c_long,
    pub points: u32,
    /// mValue, mNumWalls, mL80, mNumWonders, mNumDocks, mNumAirports, mNumUnits, mReach
    pub counters: [c_long; 8],
    pub near_water: u8,
    pub near_map_edge: u8,
    pub continent: c_long,
    pub x: c_long,
    pub y: c_long,
    pub bytes: *mut u8,
    pub len: usize,
}

impl Default for EeUnitsTownIo {
    fn default() -> Self {
        EeUnitsTownIo {
            prim: EeUnitsPlayerSaveIo::default(),
            id: 0,
            points: 0,
            counters: [0; 8],
            near_water: 0,
            near_map_edge: 0,
            continent: 0,
            x: 0,
            y: 0,
            bytes: std::ptr::null_mut(),
            len: 0,
        }
    }
}

/// The host: `(ctx, op, player, index, io) -> result`.
pub type EeUnitsTownFn = unsafe extern "C" fn(*mut c_void, i32, u32, u32, *mut EeUnitsTownIo) -> i32;

/// The host as the walks call it.
pub trait TownHost {
    fn call(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32;
}

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsTownFn,
}

impl TownHost for FfiHost {
    fn call(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32 {
        // SAFETY: the C++ host's contract (ee_units_towns_*)
        unsafe { (self.f)(self.ctx, op, player, index, io) }
    }
}

/// A saved town.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedTown {
    pub id: c_long,
    pub hull: Vec<(c_long, c_long)>,
    pub counters: [c_long; 8],
    pub near_water: bool,
    pub near_map_edge: bool,
    /// 0.268 on: the area as it was (`EEWriteAreaBytes`) and its continent
    pub area: Vec<u8>,
    pub continent: c_long,
}

/// The towns a load read, until the loaded game's own (`sSavedTowns`, `sSavedTownIDCounter`, `sSavedTownAreas`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct PendingTowns {
    /// by player id
    pub players: BTreeMap<u32, Vec<SavedTown>>,
    pub counter: c_long,
    /// the towns have their areas (0.268 on)
    pub areas: bool,
}

static PENDING: Mutex<Option<PendingTowns>> = Mutex::new(None);

fn pending() -> std::sync::MutexGuard<'static, Option<PendingTowns>> {
    PENDING.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn prim(host: &mut dyn TownHost, op: i32, value: i64) -> (i32, i64) {
    let mut io =
        EeUnitsTownIo { prim: EeUnitsPlayerSaveIo { i: value, f: 0.0, b: u8::from(value != 0) }, ..Default::default() };
    let r = host.call(op, 0, 0, &mut io);
    let v = if op == EE_UNITS_PLAYER_SAVE_READ_BOOL { i64::from(io.prim.b) } else { io.prim.i };
    (r, v)
}

#[allow(clippy::cast_possible_truncation)]
fn write_long(host: &mut dyn TownHost, value: c_long) -> i32 {
    prim(host, EE_UNITS_PLAYER_SAVE_WRITE_LONG, i64::from(value)).0
}

/// `EESaveTowns`: the id counter, the players, and per player its towns (in slot order, empty slots skipped): id,
/// hull, counters, flags, continent and area; stops at the first failed write and returns its result.
#[allow(clippy::cast_possible_truncation, clippy::cast_possible_wrap)]
pub fn save(host: &mut dyn TownHost) -> i32 {
    let mut io = EeUnitsTownIo::default();
    host.call(EE_UNITS_TOWN_COUNTER, 0, 0, &mut io);
    let mut r = write_long(host, io.prim.i as c_long);
    host.call(EE_UNITS_TOWN_PLAYERS, 0, 0, &mut io);
    let players = io.prim.i as u32;
    if r == OK {
        r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_ULONG, i64::from(players)).0;
    }
    let mut p = 0;
    while p < players && r == OK {
        let slots = host.call(EE_UNITS_TOWN_SLOTS, p, 0, &mut io) as u32;
        let count = (0..slots).filter(|&t| host.call(EE_UNITS_TOWN_GET, p, t, &mut io) != 0).count();
        r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_ULONG, count as i64).0;
        let mut t = 0;
        while t < slots && r == OK {
            let mut town = EeUnitsTownIo::default();
            if host.call(EE_UNITS_TOWN_GET, p, t, &mut town) == 0 {
                t += 1;
                continue;
            }
            r = write_long(host, town.id);
            if r == OK {
                r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_ULONG, i64::from(town.points)).0;
            }
            let mut v = 0;
            while v < town.points && r == OK {
                let mut point =
                    EeUnitsTownIo { prim: EeUnitsPlayerSaveIo { i: i64::from(v), f: 0.0, b: 0 }, ..Default::default() };
                host.call(EE_UNITS_TOWN_HULL_POINT, p, t, &mut point);
                r = write_long(host, point.x);
                if r == OK {
                    r = write_long(host, point.y);
                }
                v += 1;
            }
            for c in 0..8 {
                if r != OK {
                    break;
                }
                r = write_long(host, town.counters[c]);
            }
            if r == OK {
                r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_BOOL, i64::from(town.near_water)).0;
            }
            if r == OK {
                r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_BOOL, i64::from(town.near_map_edge)).0;
            }
            if r == OK {
                r = write_long(host, town.continent);
            }
            let mut area = EeUnitsTownIo::default();
            host.call(EE_UNITS_TOWN_AREA, p, t, &mut area);
            if r == OK {
                r = prim(host, EE_UNITS_PLAYER_SAVE_WRITE_ULONG, area.len as i64).0;
            }
            if r == OK && area.len > 0 {
                r = host.call(EE_UNITS_TOWN_WRITE_BYTES, p, t, &mut area);
            }
            t += 1;
        }
        p += 1;
    }
    r
}

/// `EELoadTowns`: reads what [`save`] wrote (the areas and continents only when `areas`); the towns become pending
/// only when every read succeeded. Returns the last read's result.
#[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
pub fn load(host: &mut dyn TownHost, areas: bool) -> (i32, Option<PendingTowns>) {
    let mut players_towns: BTreeMap<u32, Vec<SavedTown>> = BTreeMap::new();
    let read_long = |host: &mut dyn TownHost, old: c_long| -> (i32, c_long) {
        let (r, v) = prim(host, EE_UNITS_PLAYER_SAVE_READ_LONG, i64::from(old));
        (r, v as c_long)
    };
    let read_ulong = |host: &mut dyn TownHost| -> (i32, u64) {
        let (r, v) = prim(host, EE_UNITS_PLAYER_SAVE_READ_ULONG, 0);
        (r, v as u64)
    };
    let (mut r, counter) = read_long(host, 0);
    let mut players = 0u64;
    if r == OK {
        (r, players) = read_ulong(host);
    }
    let mut p = 0u64;
    while p < players && r == OK {
        let count;
        (r, count) = read_ulong(host);
        let mut t = 0u64;
        while t < count && r == OK {
            let mut town = SavedTown::default();
            let mut points = 0u64;
            (r, town.id) = read_long(host, 0);
            if r == OK {
                (r, points) = read_ulong(host);
            }
            let mut v = 0u64;
            while v < points && r == OK {
                let (mut x, mut y) = (0, 0);
                (r, x) = read_long(host, x);
                if r == OK {
                    (r, y) = read_long(host, y);
                }
                town.hull.push((x, y));
                v += 1;
            }
            for c in 0..8 {
                if r != OK {
                    break;
                }
                (r, town.counters[c]) = read_long(host, 0);
            }
            if r == OK {
                let (rr, b) = prim(host, EE_UNITS_PLAYER_SAVE_READ_BOOL, 0);
                (r, town.near_water) = (rr, b != 0);
            }
            if r == OK {
                let (rr, b) = prim(host, EE_UNITS_PLAYER_SAVE_READ_BOOL, 0);
                (r, town.near_map_edge) = (rr, b != 0);
            }
            if areas {
                let mut size = 0u64;
                if r == OK {
                    (r, town.continent) = read_long(host, 0);
                }
                if r == OK {
                    (r, size) = read_ulong(host);
                }
                if r == OK && size > 0 {
                    town.area = vec![0u8; size as usize];
                    let mut io =
                        EeUnitsTownIo { bytes: town.area.as_mut_ptr(), len: town.area.len(), ..Default::default() };
                    r = host.call(EE_UNITS_TOWN_READ_BYTES, 0, 0, &mut io);
                    if r == OK && io.prim.i as u64 != size {
                        r = EE_UNITS_TOWN_BEYOND_END;
                    }
                }
            }
            players_towns.entry(p as u32).or_default().push(town);
            t += 1;
        }
        p += 1;
    }
    if r != OK {
        return (r, None);
    }
    (r, Some(PendingTowns { players: players_towns, counter, areas }))
}

fn town_io(saved: &SavedTown) -> EeUnitsTownIo {
    EeUnitsTownIo {
        id: saved.id,
        counters: saved.counters,
        near_water: u8::from(saved.near_water),
        near_map_edge: u8::from(saved.near_map_edge),
        continent: saved.continent,
        ..Default::default()
    }
}

fn debug(host: &mut dyn TownHost, player: u32, text: &str) {
    let mut bytes = text.as_bytes().to_vec();
    let mut io = EeUnitsTownIo { bytes: bytes.as_mut_ptr(), len: bytes.len(), ..Default::default() };
    host.call(EE_UNITS_TOWN_DEBUG, player, 0, &mut io);
}

/// `EEMakeSavedTowns`: the player's towns made from the pending ones (each one's area, continent, counters and
/// flags, in the saved order); false (nothing made) when the save has no areas or an area cannot be read.
pub fn make(host: &mut dyn TownHost, pending: Option<&PendingTowns>, player: u32) -> bool {
    let Some(pending) = pending.filter(|p| p.counter >= 0 && p.areas) else {
        return false;
    };
    for saved in pending.players.get(&player).map_or(&[][..], Vec::as_slice) {
        let mut area = saved.area.clone();
        let mut io = EeUnitsTownIo { bytes: area.as_mut_ptr(), len: area.len(), ..town_io(saved) };
        if host.call(EE_UNITS_TOWN_MAKE, player, 0, &mut io) == 0 {
            debug(
                host,
                player,
                &format!(
                    "EEPlayer: player {player}'s saved town {} could not be read: its towns are rebuilt\n",
                    saved.id
                ),
            );
            host.call(EE_UNITS_TOWN_MAKE_DISCARD, player, 0, &mut EeUnitsTownIo::default());
            return false;
        }
    }
    host.call(EE_UNITS_TOWN_MAKE_COMMIT, player, 0, &mut EeUnitsTownIo::default());
    true
}

/// `EEApplySavedTowns`: the id counter as saved; each rebuilt town whose hull equals a saved one's (the first unmatched,
/// in slot order) takes its id, counters and flags; the towns are indexed by id again, and when every saved town
/// is matched and no other was rebuilt they take the saved order.
#[allow(clippy::cast_possible_truncation, clippy::cast_possible_wrap)]
pub fn apply(host: &mut dyn TownHost, pending: Option<&PendingTowns>, player: u32) {
    let Some(pending) = pending.filter(|p| p.counter >= 0) else {
        return;
    };
    let mut io = EeUnitsTownIo {
        prim: EeUnitsPlayerSaveIo { i: i64::from(pending.counter), f: 0.0, b: 0 },
        ..Default::default()
    };
    host.call(EE_UNITS_TOWN_SET_COUNTER, player, 0, &mut io);
    let Some(saved) = pending.players.get(&player) else {
        return;
    };
    let mut order: Vec<Option<u32>> = vec![None; saved.len()];
    let mut matched = 0usize;
    let slots = host.call(EE_UNITS_TOWN_SLOTS, player, 0, &mut io) as u32;
    let mut loaded = 0usize;
    for t in 0..slots {
        let mut town = EeUnitsTownIo::default();
        if host.call(EE_UNITS_TOWN_GET, player, t, &mut town) == 0 {
            continue;
        }
        loaded += 1;
        let hull: Vec<(c_long, c_long)> = (0..town.points)
            .map(|v| {
                let mut point =
                    EeUnitsTownIo { prim: EeUnitsPlayerSaveIo { i: i64::from(v), f: 0.0, b: 0 }, ..Default::default() };
                host.call(EE_UNITS_TOWN_HULL_POINT, player, t, &mut point);
                (point.x, point.y)
            })
            .collect();
        if let Some(s) = (0..saved.len()).find(|&s| order[s].is_none() && saved[s].hull == hull) {
            order[s] = Some(t);
            matched += 1;
        }
    }
    for (s, slot) in order.iter().enumerate() {
        if let Some(t) = *slot {
            host.call(EE_UNITS_TOWN_SET_SAVED, player, t, &mut town_io(&saved[s]));
        }
    }
    host.call(EE_UNITS_TOWN_REINDEX, player, 0, &mut io);
    if matched == saved.len() && loaded == saved.len() {
        let mut slots: Vec<i32> = order.iter().map(|t| t.map_or(-1, |t| t as i32)).collect();
        let mut io = EeUnitsTownIo { bytes: slots.as_mut_ptr().cast(), len: slots.len(), ..Default::default() };
        host.call(EE_UNITS_TOWN_REORDER, player, 0, &mut io);
    } else {
        debug(
            host,
            player,
            &format!(
                "EEPlayer: player {player} rebuilt {loaded} towns, {matched} of the {} saved ones: their ids and order are partly the load's\n",
                saved.len()
            ),
        );
    }
}

/// `EESaveTowns` over the C++ host.
///
/// # Safety
/// `f` is callable with `ctx` as [`EeUnitsTownFn`] documents.
#[no_mangle]
pub unsafe extern "C" fn ee_units_towns_save(ctx: *mut c_void, f: EeUnitsTownFn) -> i32 {
    save(&mut FfiHost { ctx, f })
}

/// `EELoadTowns` over the C++ host: on success the towns are pending (returns the read result; `*pending` 1 when
/// they are).
///
/// # Safety
/// As [`ee_units_towns_save`]; `pending` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_towns_load(ctx: *mut c_void, f: EeUnitsTownFn, areas: bool, pending: *mut u8) -> i32 {
    let (r, towns) = load(&mut FfiHost { ctx, f }, areas);
    // SAFETY: the caller's contract
    unsafe { *pending = u8::from(towns.is_some()) };
    if towns.is_some() {
        *self::pending() = towns;
    }
    r
}

/// The loaded game's own towns from here on (the pending towns dropped).
#[no_mangle]
pub extern "C" fn ee_units_towns_clear() {
    *pending() = None;
}

/// `EEMakeSavedTowns` (`apply` false) or `EEApplySavedTowns` (`apply` true) for one player over the C++ host.
///
/// # Safety
/// As [`ee_units_towns_save`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_towns_repair(ctx: *mut c_void, f: EeUnitsTownFn, player: u32, apply: bool) -> bool {
    // the pending towns are a copy while the host runs (it never comes back here)
    let towns = pending().clone();
    let mut host = FfiHost { ctx, f };
    if apply {
        self::apply(&mut host, towns.as_ref(), player);
        true
    } else {
        make(&mut host, towns.as_ref(), player)
    }
}

#[cfg(test)]
#[path = "town_save_tests.rs"]
mod tests;
