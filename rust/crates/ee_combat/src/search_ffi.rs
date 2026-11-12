//! The C ABI of the target searches (`search.rs`). The searches' world comes
//! through `EeCombatSearchHost`, a table of C callbacks filled per call by the C++ (`ctx` is its state, every handle
//! a C++ pointer). Nothing is kept after a call returns. A unit's tile is the unit store's (`store.rs`).

use std::ffi::c_void;

use crate::search::*;
use crate::store;
use crate::walks::EeCombatHandle;

/// The searches' world (see `SearchHost`). A search calls only the callbacks its walk needs; the others may be any
/// function of the right type.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatSearchHost {
    pub ctx: *mut c_void,
    /// The regions' rectangle into `out` (left, top, right, bottom).
    pub regions:
        unsafe extern "C" fn(ctx: *mut c_void, left: i32, top: i32, right: i32, bottom: i32, out: *mut [i32; 4]),
    pub region_len: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub region_unit: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32, index: i32) -> u64,
    pub unit_position: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, out: *mut [f32; 2]),
    pub list_next: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, prev: i32) -> u64,
    pub dummy_head: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub test: unsafe extern "C" fn(ctx: *mut c_void, test: u32, unit: u64) -> i32,
    pub target: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, out: *mut EeCombatSearchTarget),
    pub hit_distance_squared: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, x: f32, y: f32) -> f32,
    pub attack_factor: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> f32,
    pub line_blocked: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub reachable: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub ignore: unsafe extern "C" fn(ctx: *mut c_void, unit: u64),
    pub footprint_tile: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, out: *mut [i32; 2]),
    pub trace: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, code: i32, score: i32),
}

// SAFETY (every call below): the table's contract: `ctx` is live for the call that handed it over, and the
// callbacks are callable with it.
impl SearchHost for EeCombatSearchHost {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        let mut out = [0i32; 4];
        unsafe { (self.regions)(self.ctx, left, top, right, bottom, &mut out) };
        (out[0], out[1], out[2], out[3])
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        unsafe { (self.region_len)(self.ctx, x, y) }
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> EeCombatHandle {
        unsafe { (self.region_unit)(self.ctx, x, y, index) }
    }
    fn unit_tile(&mut self, unit: EeCombatHandle) -> (i32, i32) {
        // the unit store's record (ee_combat store.rs: every handle here a live complex unit)
        store::unit_of(unit).map_or((0, 0), |r| (r.tile_x as i32, r.tile_y as i32))
    }
    fn unit_position(&mut self, unit: EeCombatHandle) -> (f32, f32) {
        let mut out = [0f32; 2];
        unsafe { (self.unit_position)(self.ctx, unit, &mut out) };
        (out[0], out[1])
    }
    fn list_next(&mut self, unit: EeCombatHandle, prev: bool) -> EeCombatHandle {
        unsafe { (self.list_next)(self.ctx, unit, prev as i32) }
    }
    fn dummy_head(&mut self, unit: EeCombatHandle) -> bool {
        unsafe { (self.dummy_head)(self.ctx, unit) != 0 }
    }
    fn test(&mut self, test: u32, unit: EeCombatHandle) -> bool {
        unsafe { (self.test)(self.ctx, test, unit) != 0 }
    }
    fn target(&mut self, unit: EeCombatHandle) -> EeCombatSearchTarget {
        let mut out = EeCombatSearchTarget::default();
        unsafe { (self.target)(self.ctx, unit, &mut out) };
        out
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
