//! The C ABI of the decisions (`decide.rs`) and the walks (`walks.rs`): records in, results back by value; the walks'
//! world through host tables of C callbacks (`EeCombat*Host`, filled per call by the C++: `ctx` is its state,
//! every handle a C++ pointer). Nothing is kept after a call returns.

use std::ffi::c_void;

use crate::decide::*;
use crate::walks::*;

/// # Safety
/// `input` must point to a valid record.
unsafe fn read<'a, T>(input: *const T) -> &'a T {
    // SAFETY: the caller's contract
    unsafe { &*input }
}

/// `EECanConvert`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_can_convert(input: *const EeCombatCanConvertIn) -> EeCombatCanConvertOut {
    can_convert(unsafe { read(input) })
}

/// `EECanHeal`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_can_heal(input: *const EeCombatCanHealIn) -> i32 {
    can_heal(unsafe { read(input) })
}

/// `EEFindDropLocation`'s drop point.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_drop_point(input: *const EeCombatDropIn) -> EeCombatDropOut {
    drop_point(unsafe { read(input) })
}

// ---- the line of fire

/// `EEIsLineToUnitBlocked`'s world (see `LineHost`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatLineHost {
    pub ctx: *mut c_void,
    pub start_height: unsafe extern "C" fn(ctx: *mut c_void, bare: i32) -> f32,
    /// The tile line as `count` (x, y) pairs, valid until the next call.
    pub tile_line:
        unsafe extern "C" fn(ctx: *mut c_void, from_x: i32, from_y: i32, to_x: i32, to_y: i32, count: *mut u32) -> *const i32,
    pub target_top: unsafe extern "C" fn(ctx: *mut c_void, out: *mut EeCombatTargetTop),
    pub tile_top_above: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32, height: f32, bare: i32) -> i32,
}

impl LineHost for EeCombatLineHost {
    fn start_height(&mut self, bare: bool) -> f32 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.start_height)(self.ctx, bare as i32) }
    }
    fn tile_line(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> Vec<(i32, i32)> {
        let mut count = 0u32;
        // SAFETY: as above; the pairs stay valid until the next host call, and are copied before it
        unsafe {
            let pairs = (self.tile_line)(self.ctx, from_x, from_y, to_x, to_y, &mut count);
            if pairs.is_null() || count == 0 {
                return Vec::new();
            }
            std::slice::from_raw_parts(pairs, count as usize * 2).chunks_exact(2).map(|p| (p[0], p[1])).collect()
        }
    }
    fn target_top(&mut self) -> EeCombatTargetTop {
        let mut out = EeCombatTargetTop::default();
        // SAFETY: as above
        unsafe { (self.target_top)(self.ctx, &mut out) };
        out
    }
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> bool {
        // SAFETY: as above
        unsafe { (self.tile_top_above)(self.ctx, x, y, height, bare as i32) != 0 }
    }
}

/// `EEIsLineToUnitBlocked`.
///
/// # Safety
/// `input` and `host` must point to valid records; the host's callbacks must be callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_line_blocked(
    input: *const EeCombatLineIn,
    host: *const EeCombatLineHost,
) -> EeCombatLineOut {
    let mut host = unsafe { *host };
    line_blocked(unsafe { read(input) }, &mut host)
}

// ---- splash damage

/// `ApplyAreaDamage`'s world (see `AreaHost`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatAreaHost {
    pub ctx: *mut c_void,
    pub tile_first: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> u64,
    pub node_next: unsafe extern "C" fn(ctx: *mut c_void, node: u64) -> u64,
    pub node_unit: unsafe extern "C" fn(ctx: *mut c_void, node: u64) -> u64,
    pub distance_squared: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> f32,
    pub targetable: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub same_player: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub hit: unsafe extern "C" fn(ctx: *mut c_void, unit: u64),
}

impl AreaHost for EeCombatAreaHost {
    fn tile_first(&mut self, x: i32, y: i32) -> Handle {
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.tile_first)(self.ctx, x, y) }
    }
    fn node_next(&mut self, node: Handle) -> Handle {
        // SAFETY: as above
        unsafe { (self.node_next)(self.ctx, node) }
    }
    fn node_unit(&mut self, node: Handle) -> Handle {
        // SAFETY: as above
        unsafe { (self.node_unit)(self.ctx, node) }
    }
    fn distance_squared(&mut self, unit: Handle) -> f32 {
        // SAFETY: as above
        unsafe { (self.distance_squared)(self.ctx, unit) }
    }
    fn targetable(&mut self, unit: Handle) -> bool {
        // SAFETY: as above
        unsafe { (self.targetable)(self.ctx, unit) != 0 }
    }
    fn same_player(&mut self, unit: Handle) -> bool {
        // SAFETY: as above
        unsafe { (self.same_player)(self.ctx, unit) != 0 }
    }
    fn hit(&mut self, unit: Handle) {
        // SAFETY: as above
        unsafe { (self.hit)(self.ctx, unit) }
    }
}

/// `ApplyAreaDamage`'s walk: calls `host.hit` for every victim in order.
///
/// # Safety
/// `input` and `host` must point to valid records; the host's callbacks must be callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_area_victims(input: *const EeCombatAreaIn, host: *const EeCombatAreaHost) {
    let mut host = unsafe { *host };
    area_victims(unsafe { read(input) }, &mut host)
}

// ---- the new target search

/// `EEFindNewTarget`'s world (see `TargetHost`), plus `push` for the candidates' list the C++ keeps.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatTargetHost {
    pub ctx: *mut c_void,
    /// The regions' rectangle into `out` (left, top, right, bottom).
    pub regions:
        unsafe extern "C" fn(ctx: *mut c_void, left: i32, top: i32, right: i32, bottom: i32, out: *mut [i32; 4]),
    pub region_len: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub region_unit: unsafe extern "C" fn(ctx: *mut c_void, x: i32, y: i32, index: i32) -> u64,
    pub unit_tile: unsafe extern "C" fn(ctx: *mut c_void, unit: u64, out: *mut [i32; 2]),
    pub screen: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub multiple: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> i32,
    pub unit_id: unsafe extern "C" fn(ctx: *mut c_void, unit: u64) -> u32,
    pub test: unsafe extern "C" fn(ctx: *mut c_void, test: u32, unit: u64) -> i32,
    /// Appends a candidate to the C++'s list (`ee_combat_units_in_area`'s result).
    pub push: unsafe extern "C" fn(ctx: *mut c_void, unit: u64),
}

impl TargetHost for EeCombatTargetHost {
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        let mut out = [0i32; 4];
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.regions)(self.ctx, left, top, right, bottom, &mut out) };
        (out[0], out[1], out[2], out[3])
    }
    fn region_len(&mut self, x: i32, y: i32) -> i32 {
        // SAFETY: as above
        unsafe { (self.region_len)(self.ctx, x, y) }
    }
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> Handle {
        // SAFETY: as above
        unsafe { (self.region_unit)(self.ctx, x, y, index) }
    }
    fn unit_tile(&mut self, unit: Handle) -> (i32, i32) {
        let mut out = [0i32; 2];
        // SAFETY: as above
        unsafe { (self.unit_tile)(self.ctx, unit, &mut out) };
        (out[0], out[1])
    }
    fn screen(&mut self, unit: Handle) -> bool {
        // SAFETY: as above
        unsafe { (self.screen)(self.ctx, unit) != 0 }
    }
    fn multiple(&mut self, unit: Handle) -> bool {
        // SAFETY: as above
        unsafe { (self.multiple)(self.ctx, unit) != 0 }
    }
    fn unit_id(&mut self, unit: Handle) -> u32 {
        // SAFETY: as above
        unsafe { (self.unit_id)(self.ctx, unit) }
    }
    fn test(&mut self, test: u32, unit: Handle) -> bool {
        // SAFETY: as above
        unsafe { (self.test)(self.ctx, test, unit) != 0 }
    }
}

/// `EEUUGetUnitsInArea`: pushes the candidates in order (`host.push`); returns their number.
///
/// # Safety
/// `input` and `host` must point to valid records; the host's callbacks must be callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_units_in_area(
    input: *const EeCombatFindTargetIn,
    host: *const EeCombatTargetHost,
) -> u32 {
    let mut host = unsafe { *host };
    let units = units_in_area(unsafe { read(input) }, &mut host);
    for &unit in &units {
        // SAFETY: as in the trait's calls
        unsafe { (host.push)(host.ctx, unit) };
    }
    units.len() as u32
}

/// `EEFindNewTarget`'s choice among `count` candidates: the index of the first that passes, -1 for none.
///
/// # Safety
/// `units` must point to `count` handles, `host` to a valid record whose callbacks are callable with its `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_choose_target(
    units: *const u64,
    count: u32,
    skip_kind: i32,
    host: *const EeCombatTargetHost,
) -> i32 {
    let mut host = unsafe { *host };
    // copied: the C++ list must not be borrowed across the host's calls
    let candidates: Vec<Handle> =
        if count == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(units, count as usize) }.to_vec() };
    choose_target(&candidates, skip_kind != 0, &mut host).map_or(-1, |n| n as i32)
}
