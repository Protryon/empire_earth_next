//! The herd's game host (`ee_cp_herd_host`, EEHerd.cpp) and the entry points `EEHerd`'s members forward to. Absent
//! from a test binary.

#![cfg(not(test))]
// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use super::file::SaveFile;
use super::herd::{self, EeHerdFields, EeHerdIo, EeHerdPoint, Herd, HerdHost};
use crate::units::player_save::EeUnitsPlayerSaveFn;

extern "C" {
    /// The herd's world (EEHerd.cpp): `herd` the `EEHerd*`.
    pub(crate) fn ee_cp_herd_host(herd: *mut c_void, op: i32, a: u64, b: u64, io: *mut EeHerdIo) -> i64;
}

struct Game {
    herd: *mut c_void,
}

impl HerdHost for Game {
    fn this(&self) -> u64 {
        self.herd as u64
    }
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> i64 {
        // SAFETY: the herd is live for the call; io is a live value
        if let Some(r) = unsafe { world_op(self.herd as u64, op, a, b, io) } {
            return r;
        }
        // SAFETY: as above
        unsafe { ee_cp_herd_host(self.herd, op, a, b, io) }
    }
}

/// The herd's operations on its area, the continent manager and the unit registry, over their Rust records (AI
/// Thread cluster round 6; once `ee_cp_herd_host`'s). `None` for the C++ host's.
///
/// # Safety
/// `herd` the live `EEHerd*` (its `EEAIArea*`: the first base, no vtable); `a`, `b` live units or herds as the
/// operation takes them.
unsafe fn world_op(herd: u64, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> Option<i64> {
    use crate::astar::cm_io;
    use crate::astar::continent_grid::EePoint;
    use crate::astar::hull::EeHullPoint;
    use herd::*;
    let t = crate::world::WorldRead::begin();
    let cm = crate::astar::world_view::cm();
    let (x, y) = (io.v[0], io.v[1]);
    let tiles = cm_io::area_tiles(herd) as *mut ee_base::sparse::EeSparseArray;
    Some(match op {
        EE_CP_HERD_RESOLVE => {
            let id = io.v[0] as u32;
            if id == u32::MAX {
                0
            } else {
                crate::units::ffi::ee_units_resolve(id) as i64
            }
        }
        EE_CP_HERD_GAME_TIME => crate::world::WorldView::global(&t)
            .and_then(|w| w.physics().map(|_| w.game_time()))
            .map_or(-1, |g| g as c_long as i64),
        EE_CP_HERD_CONTINENT_OF => {
            let Some(r) = cm else { return Some(0) };
            let (ux, uy) = crate::world::UnitView::of(&t, a).map_or((0, 0), |u| u.tile_xy());
            if (ux as c_ulong) < r.width && (uy as c_ulong) < r.height {
                i64::from(cm_io::continent_id_at(r, ux as c_long, uy as c_long))
            } else {
                0
            }
        }
        EE_CP_HERD_CONTINENT_AT => cm.map_or(0, |r| i64::from(cm_io::continent_id_at(r, x as c_long, y as c_long))),
        EE_CP_HERD_CONTINENT => {
            let c = cm
                .and_then(|r| {
                    r.tables().tables[crate::astar::cm::EeAStarCmTable::Continents as usize]
                        .get(x as u16 as usize)
                        .copied()
                })
                .unwrap_or(0);
            if c == 0 {
                return Some(0);
            }
            let bounds = cm_io::area(c).hull_bounds;
            io.v[0] = i64::from(cm_io::continent(c).category);
            io.v[1] = bounds.bottom as i64;
            io.v[2] = bounds.left as i64;
            io.v[3] = bounds.right as i64;
            io.v[4] = bounds.top as i64;
            1
        }
        EE_CP_HERD_AREA => cm.map_or(0, |r| r.tables().area(io.v[0] as c_ulong) as i64),
        EE_CP_HERD_AREA_CREATE => {
            let input = io.input as *const i64;
            let points: Vec<EePoint> = (0..io.input_len)
                .map(|i| EePoint { x: *input.add(2 * i) as i32, y: *input.add(2 * i + 1) as i32 })
                .collect();
            crate::astar::cm_host::area_create(herd, &points);
            0
        }
        EE_CP_HERD_NUM_TILES => i64::from(ee_base::sparse::ee_sparse_size(tiles)),
        EE_CP_HERD_CLOSEST_HULL => {
            let q = crate::astar::area_ops::ee_astar_area_closest_hull_point(
                cm_io::area(herd),
                EeHullPoint::new(x as c_long, y as c_long),
            );
            io.v[0] = q.x as i64;
            io.v[1] = q.y as i64;
            0
        }
        EE_CP_HERD_HULL_DISTANCE => {
            io.f[0] = crate::astar::area_ops::ee_astar_area_hull_distance_squared(cm_io::area(a), cm_io::area(b));
            0
        }
        EE_CP_HERD_TILES => {
            let mut n = 0;
            let points = ee_base::sparse::ee_sparse_points(tiles, false, 1, &mut n);
            let points: Vec<EePoint> = (0..n).map(|k| *points.add(k)).collect();
            for q in points {
                herd::ee_cp_herd_push(io.list, i64::from(q.x), i64::from(q.y), 0, 0);
            }
            0
        }
        EE_CP_HERD_PLAYER_HERDS => {
            if let Some(p) = crate::world::PlayerView::of(&t, a) {
                for i in (0..p.herds_len()).rev() {
                    herd::ee_cp_herd_push(io.list, p.herd_handle(i) as i64, 0, 0, 0);
                }
            }
            0
        }
        EE_CP_HERD_PHYSICAL => {
            let p = crate::world::UnitView::of(&t, a).map_or([0.0; 3], |u| u.location());
            io.f[0] = p[0];
            io.f[1] = p[1];
            0
        }
        EE_CP_HERD_MAP_TILE => {
            i64::from(crate::world::WorldView::global(&t).and_then(|w| w.map()).and_then(|m| m.tile(x, y)).is_some())
        }
        EE_CP_HERD_PLAYER_ID => crate::world::PlayerView::of(&t, a).map_or(0, |p| i64::from(p.id())),
        _ => {
            drop(t);
            return super::herd_world::herd_op(herd, op, a, b, io);
        }
    })
}

/// Runs `body` over the herd.
///
/// # Safety
/// `herd` is the live `EEHerd*` whose record is `f`.
unsafe fn with<R>(herd: *mut c_void, f: *mut EeHerdFields, body: impl FnOnce(&mut Herd<'_, Game>) -> R) -> R {
    let mut game = Game { herd };
    // SAFETY: the caller's contract
    let mut h = unsafe { Herd::new(&mut game, f) };
    body(&mut h)
}

/// The record's lists made (the constructors, first).
///
/// # Safety
/// `f` is a live record without lists.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_init(f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe { (*f).mLists = herd::new_lists() };
}

/// The record's lists dropped (the destructor, last).
///
/// # Safety
/// `f` is a live record whose lists are not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_free(f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe {
        herd::free_lists((*f).mLists);
        (*f).mLists = std::ptr::null_mut();
    }
}

/// The constructors' start: the record's lists made, the area's type the herd kind (`kAIAreaHerd`).
///
/// # Safety
/// `herd` is the `EEHerd*` being constructed (its `EEAIArea` base, the area record at offset 0, made); `f` its record.
unsafe fn herd_begin(herd: *mut c_void, f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe {
        ee_cp_herd_init(f);
        (*(herd as *mut crate::astar::area::EeAStarAreaRecord)).area_type = AREA_KIND_HERD;
    }
}

/// `kAIAreaHerd` (ee/EEAIKindIDs.h)
const AREA_KIND_HERD: c_long = 9;

/// `EEHerd()`: the lists, the area kind; no units, leader, target or path request, then `Clear`.
///
/// # Safety
/// `herd` is the `EEHerd*` being constructed whose record is `f`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_construct_default(herd: *mut c_void, f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe {
        herd_begin(herd, f);
        (*f).mLeader = herd::EE_CP_HERD_NONE;
        (*f).mTarget = herd::EE_CP_HERD_NONE;
        (*f).mPathRequest = 0;
        with(herd, f, |h| h.clear());
    }
}

/// `EEHerd(player, animal, id)`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_construct(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    player: u64,
    animal: c_long,
    id: u16,
) {
    // SAFETY: the caller's contract
    unsafe {
        herd_begin(herd, f);
        with(herd, f, |h| h.construct(player, animal, id))
    }
}

/// `Clear` (the destructor's).
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_clear(herd: *mut c_void, f: *mut EeHerdFields) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.clear()) }
}

/// `AddUnit`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `unit` a live `EEComplexUnit*`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_add_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.add_unit(unit)) }
}

/// `RemoveUnit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_remove_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.remove_unit(unit)) }
}

/// `HasUnit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_has_unit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.has_unit(unit)) }
}

/// `CanTake`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_can_take(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    unit: u64,
    check_range: bool,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.can_take(unit, check_range)) }
}

/// `HandleUnitHit`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_handle_unit_hit(herd: *mut c_void, f: *mut EeHerdFields, unit: u64) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.handle_unit_hit(unit)) }
}

/// `AttackBack`.
///
/// # Safety
/// As [`ee_cp_herd_add_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_attack_back(herd: *mut c_void, f: *mut EeHerdFields, unit: u64, all: bool) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.attack_back(unit, all)) }
}

/// `Update`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_update(herd: *mut c_void, f: *mut EeHerdFields, time: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.update(time)) }
}

/// `Create` (the area's override) over `count` tiles.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `tiles` holds `count` points.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_create(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    tiles: *const EeHerdPoint,
    count: usize,
) {
    // SAFETY: the caller's contract (copied before any host call)
    let tiles: Vec<EeHerdPoint> =
        if count == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(tiles, count) }.to_vec() };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.create(&tiles)) }
}

/// `GetRandomPoint` (the area's override).
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_random_point(herd: *mut c_void, f: *mut EeHerdFields) -> EeHerdPoint {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.random_point()) }
}

/// `Load` over the saved game's file (`host` its primitives).
///
/// # Safety
/// As [`ee_cp_herd_construct_default`]; `host` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_load(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
) -> i32 {
    // SAFETY: the caller's contract
    let mut file = unsafe { SaveFile::new(ctx, host) };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.load(major, minor, &mut file)) }
}

/// `Save`.
///
/// # Safety
/// As [`ee_cp_herd_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_save(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
) -> i32 {
    // SAFETY: the caller's contract
    let mut file = unsafe { SaveFile::new(ctx, host) };
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.save(major, minor, &mut file)) }
}

/// `RepairPointers`.
///
/// # Safety
/// As [`ee_cp_herd_construct_default`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_repair_pointers(
    herd: *mut c_void,
    f: *mut EeHerdFields,
    major: u32,
    minor: u32,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe { with(herd, f, |h| h.repair_pointers(major, minor)) }
}

/// The units' count (`mUnits.size()`).
///
/// # Safety
/// `f` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_unit_count(f: *const EeHerdFields) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*(*f).mLists).units.as_slice().len() }
}

/// Unit `i`'s handle (`mUnits[i]`: a unit id).
///
/// # Safety
/// `f` is a live record; `i` below the count.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_herd_unit_handle(f: *const EeHerdFields, i: usize) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*(*f).mLists).units.as_slice()[i] }
}
