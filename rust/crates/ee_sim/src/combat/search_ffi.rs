//! The C ABI of the target searches (`search.rs`). The searches' world comes
//! through `EeCombatSearchHost`, a table of C callbacks filled per call by the C++ (`ctx` is its state, every handle
//! a C++ pointer). Nothing is kept after a call returns. A unit's tile is the unit store's (`store.rs`).

use std::ffi::c_void;

use crate::combat::search::*;
use crate::combat::store;
use crate::combat::walks::EeCombatHandle;

/// The searches' world (see `SearchHost`). A search calls only the callbacks its walk needs; the others may be any
/// function of the right type.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatSearchHost {
    pub ctx: *mut c_void,
    /// The map's unit regions (`gWorld.mMap->mUnitRegions`, regions.rs), live for the call.
    pub unit_regions: *const crate::combat::regions::EeCombatUnitRegions,
    pub list_next: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, prev: i32) -> u64,
    pub dummy_head: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub test: unsafe extern "C" fn(ctx: *mut c_void, test: u32, unit: u64) -> i32,
    pub hit_distance_squared: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, x: f32, y: f32) -> f32,
    pub attack_factor: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> f32,
    pub line_blocked: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub reachable: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub ignore: unsafe extern "C" fn(ctx: *mut c_void, unit: u64),
    pub footprint_tile: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, out: *mut [i32; 2]),
    pub trace: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, code: i32, score: i32),
}

/// `f` of the view of a candidate (a handle the C++ search gave: a live unit while the search runs).
fn with_unit<R>(unit: EeCombatHandle, f: impl FnOnce(crate::world::UnitView<'_>) -> R) -> R {
    // SAFETY: a search runs inside the C++ call that holds the world
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above; the handle is a live unit
    f(unsafe { crate::world::UnitView::of(&t, unit) }.expect("a candidate"))
}

impl EeCombatSearchHost {
    fn table(&self) -> &crate::combat::regions::EeCombatUnitRegions {
        // SAFETY: the table's contract: the regions are live for the call that handed them over
        unsafe { &*self.unit_regions }
    }
}

// SAFETY (every call below): the table's contract: `ctx` is live for the call that handed it over, and the
// callbacks are callable with it.
impl SearchHost for EeCombatSearchHost {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        self.table().rect(left, top, right, bottom)
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        self.table().region(x as u32, y as u32).map_or(-1, |r| r.len() as i32)
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> EeCombatHandle {
        self.table().region(x as u32, y as u32).map_or(0, |r| r[index as usize])
    }
    fn unit_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        // the unit store's record (ee_combat store.rs: every handle here a live complex unit)
        store::unit_of(unit).map_or((0, 0), |r| (r.tile_x as i32, r.tile_y as i32))
    }
    fn unit_position(&mut self, unit: EeCombatHandle) -> (f32, f32) {
        // the candidate's complex unit's location (a region's unit is its own complex unit)
        with_unit(unit, |u| {
            let p = u.complex_unit().expect("a candidate with a complex unit").location();
            (p[0], p[1])
        })
    }
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle {
        unsafe { (self.list_next)(self.ctx, unit, prev as i32) }
    }
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool {
        unsafe { (self.dummy_head)(self.ctx, unit) != 0 }
    }
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool {
        // the plain reads, through the views; the others the C++'s
        match test {
            EE_COMBAT_SEARCH_TEST_NATURE => with_unit(unit, |u| u.player().expect("an owner").id() == 0),
            EE_COMBAT_SEARCH_TEST_WALL => with_unit(unit, |u| {
                let c = u.class().expect("a candidate's class");
                let tower: bool = crate::world_field!(c.fields(), mIsTower);
                c.wall() && !tower
            }),
            EE_COMBAT_SEARCH_TEST_ALIVE => with_unit(unit, |u| u.hit_points() > 0),
            _ => unsafe { (self.test)(self.ctx, test, unit) != 0 },
        }
    }
    fn target(&mut self, unit: EeCombatHandle) -> EeCombatSearchTarget {
        // its class's size and type, its stats' attack and maximum velocity
        with_unit(unit, |u| {
            let class = u.class().expect("a candidate's class");
            let size: std::os::raw::c_long = crate::world_field!(class.fields(), mTileSize);
            let stats = u.stats().expect("a candidate's stats");
            EeCombatSearchTarget {
                multiple: i32::from(size > 1),
                unit_type: class.unit_type() as i32,
                attack: crate::world_field!(stats.fields(), mAttack),
                max_velocity: crate::world_field!(stats.fields(), mMaxVelocity),
            }
        })
    }
    fn hit_distance_squared(&mut self, unit: EeCombatHandle, x: f32, y: f32) -> f32 {
        unsafe { (self.hit_distance_squared)(self.ctx, unit, x, y) }
    }
    fn attack_factor(&mut self, unit: EeCombatHandle) -> f32 {
        unsafe { (self.attack_factor)(self.ctx, unit) }
    }
    fn line_blocked(&mut self, unit: EeCombatHandle) -> bool {
        unsafe { (self.line_blocked)(self.ctx, unit) != 0 }
    }
    fn reachable(&mut self, unit: EeCombatHandle) -> bool {
        unsafe { (self.reachable)(self.ctx, unit) != 0 }
    }
    fn ignore(&mut self, unit: EeCombatHandle) {
        unsafe { (self.ignore)(self.ctx, unit) }
    }
    fn footprint_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        let mut out = [0i32; 2];
        unsafe { (self.footprint_tile)(self.ctx, unit, &mut out) };
        (out[0], out[1])
    }
    fn trace(&mut self, unit: EeCombatHandle, code: i32, score: i32) {
        unsafe { (self.trace)(self.ctx, unit, code, score) }
    }
}

/// `EESearchTarget`.
///
/// # Safety
/// `input` and `host` must point to valid records; the host's callbacks must be callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_search_target(
    input: *const EeCombatSearchIn,
    host: *const EeCombatSearchHost,
) -> EeCombatSearchOut {
    let mut host = unsafe { *host };
    search_target(unsafe { &*input }, &mut host)
}

/// `EESearchTargetAlongList` from `unit` (the searching unit's handle in the list).
///
/// # Safety
/// As `ee_combat_search_target`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_search_along_list(
    input: *const EeCombatSearchIn,
    unit: u64,
    host: *const EeCombatSearchHost,
) -> EeCombatSearchOut {
    let mut host = unsafe { *host };
    search_along_list(unsafe { &*input }, unit, &mut host)
}
