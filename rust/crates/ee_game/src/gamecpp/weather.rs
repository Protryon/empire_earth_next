//! The weather (game core lane, round 10, lane D; was EEEMStorm.cpp's and EEEnvironmentManager.cpp's bodies): a
//! storm's start (its goal in a random player's town, its start over water or cliffs and hills, the shaped path and
//! the weather effect around it), its route request, its save and load, its step's host (ee_sim app storm.rs's
//! [`StormHost`] over one-statement cases), the environment manager's storms, wind and save, and the TEST ONLY storm
//! record (STRM) and sweep. Each C++ call or member is a one-statement case of EEEMStorm.cpp's `EECore10OpD2` or
//! EEEnvironmentManager.cpp's `EECore10OpD3` ([`super::EeGameCpp`]'s `Ds`, `De` ops).
//!
//! Simulation code (RUST_PLAN 2.1): the random draws are the C++'s, in its order (the player, the town, the snow's
//! coin, the steps; the storm roll and its extra draw); the wind's step is the same f32 sum and its rate the C++'s
//! double division rounded to f32; `UMFloatToLong` is `lrintf`.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::um;
use ee_sim::app::capture_record::{self, Record};
use ee_sim::app::storm::{self as st, EeAppStormFields, StormHost};
use ee_sim::astar::modifier_obj::{
    ee_astar_modifier_new_storm_planner, ee_astar_modifier_new_storm_shaper, ee_astar_modifier_release,
};
use ee_sim::astar::request::{ee_path_request_dereference, ee_path_request_done, ee_path_request_tiles};
use ee_sim::astar::storage::AStarRect;

use super::persistent::Pf;
use super::{h, hd, hf, hp, pa, ptr, rec, Owned, O, P};

/// `kStormUnknown`, `kStormRain`, `kStormSnow`, `kStormHurricane`, `kStormBlizzard`.
const STORM_UNKNOWN: c_long = 0;
const STORM_RAIN: c_long = 1;
const STORM_SNOW: c_long = 2;
const STORM_HURRICANE: c_long = 3;
const STORM_BLIZZARD: c_long = 4;
/// `kGraphicEffectRain`, `kGraphicEffectSnow`.
const EFFECT_RAIN: i64 = 0x54;
const EFFECT_SNOW: i64 = 0xac;
/// the wind's limits (EEEnvironmentManager.cpp's .rdata)
const WIND_STEP_MAXIMUM: f32 = 1.3333333e-06;
const WIND_MINIMUM: f32 = 1.25e-06;
const WIND_MAXIMUM: f32 = 1.4285714e-06;
const WIND_STEP_MINIMUM: f32 = 1e-08;

/// `U2DRectangle<long>` in its member order (bottom, left, right, top).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGamecppStormRect {
    pub bottom: c_long,
    pub left: c_long,
    pub right: c_long,
    pub top: c_long,
}

impl EeGamecppStormRect {
    /// `Set(point)`
    pub fn at(p: [c_long; 2]) -> EeGamecppStormRect {
        EeGamecppStormRect { bottom: p[1], left: p[0], right: p[0], top: p[1] }
    }

    /// the edges out by `by`, clamped to [0, size] (the C++'s `max(x - by, 0)` and `min(x + by, size)`)
    pub fn grown(self, by: c_long, size: c_long) -> EeGamecppStormRect {
        let lo = |v: c_long| if v - by > 0 { v - by } else { 0 };
        let hi = |v: c_long| if v + by < size { v + by } else { size };
        EeGamecppStormRect { top: lo(self.top), left: lo(self.left), right: hi(self.right), bottom: hi(self.bottom) }
    }

    /// `Contains(point)`
    pub fn contains(&self, p: [c_long; 2]) -> bool {
        p[0] >= self.left && p[0] <= self.right && p[1] >= self.top && p[1] <= self.bottom
    }

    fn astar(self) -> AStarRect {
        AStarRect { bottom: self.bottom as i32, left: self.left as i32, right: self.right as i32, top: self.top as i32 }
    }
}

/// `EECPASGFlags`: the route request's flags (every one set: the C++'s memset to 1).
#[repr(C)]
struct EeGamecppStormFlags {
    player: c_long,
    flags: [bool; 17],
}

// ---- the storm's members

fn fields<'a>(s: P) -> &'a mut EeAppStormFields {
    // SAFETY: the storm's record (EEEMStorm's base), used before the next C++ call that changes it
    unsafe { &mut *rec(O::DsFields, s) }
}

fn member<'a, T>(op: O, s: P) -> &'a mut T {
    // SAFETY: the op answers the address of the storm's member of type T
    unsafe { &mut *rec(op, s) }
}

fn effect(s: P) -> &'static mut P {
    member::<P>(O::DsStorm, s)
}

fn request(s: P) -> &'static mut P {
    member::<P>(O::DsRequest, s)
}

fn area(s: P) -> &'static mut EeGamecppStormRect {
    member::<EeGamecppStormRect>(O::DsArea, s)
}

fn point(op: O, s: P) -> &'static mut [c_long; 2] {
    member::<[c_long; 2]>(op, s)
}

fn is_rain(kind: c_long) -> bool {
    kind == STORM_RAIN || kind == STORM_HURRICANE
}

fn is_snow(kind: c_long) -> bool {
    kind == STORM_SNOW || kind == STORM_BLIZZARD
}

/// EEEMStorm's constructor: the step's members for the kind, no effect or request, the kind's name; a known kind
/// begins.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_init(storm: *mut c_void, kind: c_long) {
    let s = storm as P;
    *fields(s) = st::new_fields(kind);
    *effect(s) = 0;
    *request(s) = 0;
    h(O::DsSetName, s, &[kind as i64]);
    if kind != STORM_UNKNOWN {
        ee_gamecpp_storm_begin(storm);
    }
}

/// `Begin`: the goal in a random player's town (else the largest land, else any continent), the start over the
/// largest water (rain), a random cliff or hill (snow) or any continent, the path shaped around the start, the weather
/// effect over the tiles it visited, the route request; state 2.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_begin(storm: *mut c_void) {
    let s = storm as P;
    let pl = hp(O::DsRandomPlayer, 0, &[]);
    let mut a: P = 0;
    let town = pl != 0 && h(O::DsTownCount, pl, &[]) != 0 && {
        a = hp(O::DsRandomTown, pl, &[]);
        a != 0
    };
    if !town {
        a = hp(O::DsLargestLand, 0, &[]);
        if a == 0 {
            a = hp(O::DsLargestAny, 0, &[]);
        }
    }
    h(O::DsRandomPoint, a, &[pa(point(O::DsGoal, s) as *mut _ as P)]);
    let kind = fields(s).mType;
    a = 0;
    if is_rain(kind) {
        a = hp(O::DsLargestWater, 0, &[]);
    } else if is_snow(kind) {
        if hd(O::PhysGetRandom, 0, &[], &[]) <= 0.5 {
            a = hp(O::DsRandomCliff, 0, &[]);
        } else {
            a = hp(O::DsRandomHill, 0, &[]);
        }
    }
    if a == 0 {
        a = hp(O::DsLargestAny, 0, &[]);
    }
    h(O::DsRandomPoint, a, &[pa(point(O::DsStart, s) as *mut _ as P)]);
    let start = *point(O::DsStart, s);
    let size = h(O::DsMapSize, 0, &[]) as c_long;
    let inner = EeGamecppStormRect::at(start).grown(4, size);
    let outer = inner.grown(4, size);
    let steps = h(O::DsRandomLong, 0, &[0x2d, 0x87]) as c_long;
    let shaper =
        ee_astar_modifier_new_storm_shaper(inner.astar(), outer.astar(), steps as i32, is_rain(kind), is_snow(kind));
    let mut bounds = EeGamecppStormRect::at(start).grown(30, size);
    let path = Owned::new(O::DsPointsNew, O::DsPointsDelete);
    let visited = Owned::new(O::DsPointsNew, O::DsPointsDelete);
    h(O::DsFindPath, s, &[ptr(&mut bounds), pa(path.0), shaper as usize as i64, pa(visited.0)]);
    let graphic = if is_rain(kind) {
        hp(O::GfxEffectGet, 0, &[EFFECT_RAIN])
    } else if is_snow(kind) {
        hp(O::GfxEffectGet, 0, &[EFFECT_SNOW])
    } else {
        0
    };
    let mut handle: u32 = 0;
    h(O::DsCreateWeather, visited.0, &[pa(graphic), ptr(&mut handle)]);
    *effect(s) = hp(O::EffGet, 0, &[handle as i64]);
    h(O::DsEffectReference, *effect(s), &[]);
    h(O::DsEffectBounds, s, &[]);
    h(O::DsSustain, s, &[1]);
    // SAFETY: the shaper this made
    unsafe { ee_astar_modifier_release(shaper) };
    ee_gamecpp_storm_request_route(storm);
    fields(s).mState = 2;
}

/// `IsOver`: no effect, or finished.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_is_over(storm: *mut c_void) -> bool {
    let s = storm as P;
    *effect(s) == 0 || fields(s).mState == 6
}

fn cancel_request(s: P) {
    let r = *request(s);
    if r != 0 {
        h(O::DsCancelRequest, r, &[]);
        // SAFETY: the storm's own reference to its request (the Rust request, ee_sim astar request.rs)
        unsafe { ee_path_request_dereference(r as *mut c_void) };
        *request(s) = 0;
    }
}

/// `RequestRoute`: the old request cancelled, a route requested with the kind's planner.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_request_route(storm: *mut c_void) {
    let s = storm as P;
    cancel_request(s);
    let planner = ee_astar_modifier_new_storm_planner(is_rain(fields(s).mType));
    let mut flags = EeGamecppStormFlags { player: 0, flags: [true; 17] };
    *request(s) = hp(O::DsRequestPath, s, &[planner as usize as i64, ptr(&mut flags)]);
    // SAFETY: the planner this made
    unsafe { ee_astar_modifier_release(planner) };
}

/// `TakePath`: the finished route reversed (to pop from the back); the request released.
fn take_path(s: P) {
    let r = *request(s) as *const c_void;
    // SAFETY: the storm's finished request: its tiles as (x, y) pairs
    let mut tiles = vec![0 as c_long; 2 * unsafe { ee_path_request_tiles(r, std::ptr::null_mut(), 0) } as usize];
    unsafe { ee_path_request_tiles(r, tiles.as_mut_ptr(), (tiles.len() / 2) as u32) };
    h(O::DsPathClear, s, &[]);
    for k in (0..tiles.len() / 2).rev() {
        h(O::DsPathPush, s, &[tiles[2 * k] as i64, tiles[2 * k + 1] as i64]);
    }
    // SAFETY: as cancel_request
    unsafe { ee_path_request_dereference(r as *mut c_void) };
    *request(s) = 0;
}

/// `Release`: the route request cancelled, the effect ended and released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_release(storm: *mut c_void) {
    let s = storm as P;
    cancel_request(s);
    if *effect(s) == 0 {
        return;
    }
    h(O::DsSustain, s, &[0]);
    h(O::DsEndEffect, s, &[]);
    h(O::DsEffectRelease, *effect(s), &[]);
    *effect(s) = 0;
}

/// `Load`: the route, the areas, the effect's handle (as the pointer, until `Restore`), the step's members; always 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_load(storm: *mut c_void, file: *mut c_void) -> c_long {
    let (s, f) = (storm as P, Pf(file as P));
    let mut count: c_ulong = 0;
    let mut r = f.read_ulong(&mut count);
    if r != 0 {
        return 0;
    }
    h(O::DsPathResize, s, &[count as i64]);
    for i in 0..count {
        if r == 0 {
            r = h(O::DsPathPointLoad, s, &[i as i64, pa(f.0)]);
        }
    }
    if r != 0 {
        return 0;
    }
    let a = area(s) as *mut EeGamecppStormRect as P;
    h(O::DsRectLoad, a, &[pa(f.0)]);
    h(O::DsPointLoad, point(O::DsStart, s) as *mut _ as P, &[pa(f.0)]);
    h(O::DsPointLoad, point(O::DsGoal, s) as *mut _ as P, &[pa(f.0)]);
    h(O::DsRectLoad, a, &[pa(f.0)]);
    let mut handle: u32 = 0;
    r = h(O::DsReadHandle, f.0, &[ptr(&mut handle)]);
    *effect(s) = handle as P;
    let k = fields(s);
    if r == 0
        && f.read_f32(&mut k.mTargetIntensity) == 0
        && f.read_f32(&mut k.mIntensity) == 0
        && f.read_f32(&mut k.mStep) == 0
        && f.read_long(&mut k.mType) == 0
        && f.read_long(&mut k.mState) == 0
        && f.read_ulong(&mut k.mNextMoveTime) == 0
        && f.read_bool(&mut k.mIsSevere) == 0
    {
        f.read_ulong(&mut k.mEndTime);
    }
    0
}

/// `Restore` (RepairPointers): the effect from its saved handle (9 when it is gone), an empty route's effect not
/// sustained, the name again; a routing storm routes again (state 8).
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_restore(storm: *mut c_void) -> c_long {
    let s = storm as P;
    let mut result = 0;
    let handle = *effect(s) as u32;
    *effect(s) = hp(O::EffGet, 0, &[handle as i64]);
    if *effect(s) != 0 {
        h(O::DsEffectReference, *effect(s), &[]);
    } else {
        *effect(s) = 0;
        result = 9;
    }
    if h(O::DsPathLen, s, &[]) == 0 && *effect(s) != 0 {
        h(O::DsSustain, s, &[0]);
    }
    h(O::DsSetName, s, &[fields(s).mType as i64]);
    if fields(s).mState == 2 {
        fields(s).mState = 8;
    }
    result
}

/// `Save`: as `Load`; always 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_save(storm: *mut c_void, file: *mut c_void) -> c_long {
    let (s, f) = (storm as P, Pf(file as P));
    let count = h(O::DsPathLen, s, &[]) as c_ulong;
    let mut r = f.write_ulong(count);
    if r != 0 {
        return 0;
    }
    for i in 0..count {
        if r == 0 {
            r = h(O::DsPathPointSave, s, &[i as i64, pa(f.0)]);
        }
    }
    if r != 0 {
        return 0;
    }
    let a = area(s) as *mut EeGamecppStormRect as P;
    h(O::DsRectSave, a, &[pa(f.0)]);
    h(O::DsPointSave, point(O::DsStart, s) as *mut _ as P, &[pa(f.0)]);
    h(O::DsPointSave, point(O::DsGoal, s) as *mut _ as P, &[pa(f.0)]);
    h(O::DsRectSave, a, &[pa(f.0)]);
    let handle = h(O::DsEffectHandle, *effect(s), &[]) as c_ulong;
    let k = *fields(s);
    if f.write_ulong(handle) == 0
        && f.write_f32(k.mTargetIntensity) == 0
        && f.write_f32(k.mIntensity) == 0
        && f.write_f32(k.mStep) == 0
        && f.write_long(k.mType) == 0
        && f.write_long(k.mState) == 0
        && f.write_ulong(k.mNextMoveTime) == 0
        && f.write_bool(k.mIsSevere) == 0
    {
        f.write_ulong(k.mEndTime);
    }
    0
}

/// The step's host (ee_sim app storm.rs): each fact one case.
struct Host(P);

impl StormHost for Host {
    fn request(&mut self) -> Option<bool> {
        let r = *request(self.0);
        if r == 0 {
            None
        } else {
            Some(unsafe { ee_path_request_done(r as *const c_void) })
        }
    }
    fn take_path(&mut self) {
        take_path(self.0);
    }
    fn request_route(&mut self) {
        ee_gamecpp_storm_request_route(self.0 as *mut c_void);
    }
    fn path_len(&mut self) -> usize {
        h(O::DsPathLen, self.0, &[]) as usize
    }
    fn distance_squared(&mut self, goal: bool) -> c_long {
        h(O::DsDistance, self.0, &[goal as i64]) as c_long
    }
    fn pop_path(&mut self) {
        h(O::DsPopPath, self.0, &[]);
    }
    fn move_to(&mut self, goal: bool) {
        h(O::DsMove, self.0, &[goal as i64]);
    }
    fn tile_count(&mut self) -> c_ulong {
        h(O::DsTileCount, *effect(self.0), &[]) as c_ulong
    }
    fn end_effect(&mut self) {
        h(O::DsEndEffect, self.0, &[]);
    }
    fn set_effect_intensity(&mut self, intensity: f32) {
        if *effect(self.0) != 0 {
            hf(O::DsSetIntensity, self.0, &[], &[intensity]);
        }
    }
    fn game_speed(&mut self) -> f32 {
        hd(O::DsGameSpeed, 0, &[], &[])
    }
    fn random_range(&mut self, min: f32, max: f32) -> f32 {
        hd(O::PhysRandomRange, 0, &[], &[min, max])
    }
    fn random(&mut self) -> f32 {
        hd(O::PhysGenerate, 0, &[], &[])
    }
    fn reset_color(&mut self) {
        h(O::DsResetColor, self.0, &[]);
    }
}

/// `Step`: ee_sim app storm.rs's step over the host, then its STRM record (TEST ONLY).
#[no_mangle]
pub extern "C" fn ee_gamecpp_storm_step(storm: *mut c_void, time: c_ulong) {
    let s = storm as P;
    // the record copied in and back, as ee_app_storm_step did (no host operation touches it)
    let mut k = *fields(s);
    st::step(&mut k, &mut Host(s), time);
    *fields(s) = k;
    capture_storm(s, 1, time);
}

/// STRM (TEST ONLY): a storm's state after its creation (phase 0) and each step (1, with its time).
fn capture_storm(s: P, phase: u32, time: c_ulong) {
    if !capture_record::active() {
        return;
    }
    let k = *fields(s);
    let mut r = Record::new(u32::from_le_bytes(*b"STRM"));
    r.u32(phase);
    r.u32(time as u32);
    r.i32(k.mType as i32);
    r.i32(k.mState as i32);
    r.f32(k.mTargetIntensity);
    r.f32(k.mIntensity);
    r.f32(k.mStep);
    r.u32(k.mNextMoveTime as u32);
    r.u32(k.mEndTime as u32);
    r.u32(k.mIsSevere as u32);
    let (start, goal, a) = (*point(O::DsStart, s), *point(O::DsGoal, s), *area(s));
    for v in [start[0], start[1], goal[0], goal[1], a.top, a.left, a.bottom, a.right] {
        r.i32(v as i32);
    }
    let n = h(O::DsPathLen, s, &[]);
    r.u32(n as u32);
    for i in 0..n {
        let mut p: [c_long; 2] = [0; 2];
        h(O::DsPathAt, s, &[i, ptr(&mut p)]);
        r.i32(p[0] as i32);
        r.i32(p[1] as i32);
    }
    r.u32((*request(s) != 0) as u32);
    if *effect(s) != 0 {
        let mut c: [c_long; 2] = [0; 2];
        h(O::DsCenter, *effect(s), &[ptr(&mut c)]);
        r.i32(c[0] as i32);
        r.i32(c[1] as i32);
        r.u32(h(O::DsTileCount, *effect(s), &[]) as u32);
    } else {
        r.u32(0xffff_ffff);
    }
    r.commit();
}

/// EE-SIM --app-sweep's storms (TEST ONLY): the weather on, one storm of each kind, each recorded (STRM phase 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_storm_sweep() {
    let m = hp(O::DeManager, 0, &[]);
    if !capture_record::active() || m == 0 {
        return;
    }
    ee_gamecpp_env_set_enabled(m as *mut c_void, true);
    for kind in [STORM_RAIN, STORM_SNOW, STORM_HURRICANE, STORM_BLIZZARD] {
        let s = hp(O::DeNewStorm, m, &[kind as i64]);
        capture_storm(s, 0, 0);
        h(O::DeStormPush, m, &[pa(s)]);
    }
}

// ---- the environment manager (p: an EEEnvironmentManager*)

fn em<'a, T>(op: O, m: P) -> &'a mut T {
    // SAFETY: the op answers the address of the manager's member of type T
    unsafe { &mut *rec(op, m) }
}

/// `EEEnvironmentManager()`'s members (game core lane round 11, lane B): disabled, no storms, a calm wind, a
/// weather step every 5 s, at most three storms at a time.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_init(manager: *mut c_void) {
    let m = manager as P;
    *em::<f32>(O::DeWind, m) = 1.25e-06f32;
    *em::<f32>(O::DeWindTarget, m) = 1.25e-06f32;
    *em::<f32>(O::DeWindRate, m) = 1.3333333e-06f32;
    *em::<c_ulong>(O::DeNextStep, m) = 0;
    *em::<bool>(O::DeEnabled, m) = false;
    *em::<c_long>(O::DeInterval, m) = 5000;
    *em::<c_long>(O::DeMaxStorms, m) = 3;
    *em::<f32>(O::EbEnvF40, m) = 0.25;
    *em::<f32>(O::EbEnvF44, m) = 0.75;
}

fn storms(m: P) -> i64 {
    h(O::DeStormCount, m, &[])
}

fn storm_at(m: P, i: i64) -> P {
    hp(O::DeStormAt, m, &[i])
}

/// `IsInStorm`: whether a tile is inside a rain or hurricane storm (`rain`) and/or a snow or blizzard storm (`snow`).
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_is_in_storm(
    manager: *mut c_void,
    tile: *const c_void,
    rain: bool,
    snow: bool,
) -> bool {
    let m = manager as P;
    // SAFETY: the caller's U2DPoint<long>
    let p = unsafe { *(tile as *const [c_long; 2]) };
    let mut result = false;
    let mut i = storms(m) - 1;
    while i >= 0 && !result {
        let s = storm_at(m, i);
        let kind = fields(s).mType;
        if rain && is_rain(kind) {
            result = area(s).contains(p);
        }
        if snow && is_snow(kind) {
            result = area(s).contains(p);
        }
        i -= 1;
    }
    result
}

/// `Step`: the storms and the wind, a finished state cleared, the next step by the game speed (the time of day the
/// C++ read for them is unused).
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_step(manager: *mut c_void, time: c_ulong) {
    let m = manager as P;
    update_storms(m, time);
    update_wind(m);
    let state = em::<c_long>(O::DeState, m);
    if *state == 2 {
        *state = 0;
    }
    let interval = *em::<c_long>(O::DeInterval, m);
    let next = um::lrintf(hd(O::DsGameSpeed, 0, &[], &[]) * interval as f32) as c_long;
    *em::<c_ulong>(O::DeNextStep, m) = (next as c_ulong).wrapping_add(time);
}

/// `UpdateStorms`: each storm stepped, the ones over released and deleted; then a roll for a new storm below the
/// limit that never makes one (the roll, its extra draw and the rain effect's lookup are all it does).
fn update_storms(m: P, time: c_ulong) {
    let mut i = storms(m) - 1;
    while i >= 0 {
        ee_gamecpp_storm_step(storm_at(m, i) as *mut c_void, time);
        let s = storm_at(m, i);
        if ee_gamecpp_storm_is_over(s as *mut c_void) {
            ee_gamecpp_storm_release(s as *mut c_void);
            h(O::DeStormErase, m, &[i]);
            h(O::DeStormDelete, s, &[]);
        }
        i -= 1;
    }
    let max = *em::<c_long>(O::DeMaxStorms, m) as c_ulong;
    if (storms(m) as u64) < max as u64 && hd(O::PhysRandomRange, 0, &[], &[0.0001, 1.0]) <= 0.2 {
        hd(O::PhysGenerate, 0, &[], &[]);
        h(O::GfxEffectGet, 0, &[EFFECT_RAIN]);
    }
}

/// The wind's step toward `target` by `rate`: the new wind.
pub fn wind_toward(wind: f32, target: f32, rate: f32) -> f32 {
    if wind < target {
        let w = wind + rate;
        if w > target {
            target
        } else {
            w
        }
    } else {
        let w = wind - rate;
        if w < target {
            target
        } else {
            w
        }
    }
}

/// The rate that reaches `target` from `wind` in about fifteen steps (the C++'s `/ 15.0` in double), clamped.
pub fn wind_rate(wind: f32, target: f32) -> f32 {
    let delta = target - wind;
    let step = ((if delta < 0.0 { -delta } else { delta }) as f64 / 15.0) as f32;
    let at_least = if step > WIND_STEP_MINIMUM { step } else { WIND_STEP_MINIMUM };
    if at_least < WIND_STEP_MAXIMUM {
        at_least
    } else {
        WIND_STEP_MAXIMUM
    }
}

/// `UpdateWind`: one step toward the target; there, a new random target and its rate.
fn update_wind(m: P) {
    let wind = *em::<f32>(O::DeWind, m);
    let target = *em::<f32>(O::DeWindTarget, m);
    if wind != target {
        let rate = *em::<f32>(O::DeWindRate, m);
        *em::<f32>(O::DeWind, m) = wind_toward(wind, target, rate);
    } else {
        let target = hd(O::PhysRandomRange, 0, &[], &[WIND_MINIMUM, WIND_MAXIMUM]);
        *em::<f32>(O::DeWindTarget, m) = target;
        if wind != target {
            *em::<f32>(O::DeWindRate, m) = wind_rate(wind, target);
        }
    }
}

/// `Load`: the storms (each a new unknown storm loaded), the state, the wind, the timing and the colours; the first
/// read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_load(manager: *mut c_void, file: *mut c_void) -> c_long {
    let (m, f) = (manager as P, Pf(file as P));
    let mut count: c_ulong = 0;
    let mut r = f.read_ulong(&mut count);
    if r != 0 {
        return r as c_long;
    }
    h(O::DeStormResize, m, &[count as i64]);
    for i in 0..count {
        let s = hp(O::DeNewStorm, m, &[STORM_UNKNOWN as i64]);
        h(O::DeStormSet, m, &[i as i64, pa(s)]);
        if r == 0 {
            r = ee_gamecpp_storm_load(s as *mut c_void, file) as i64;
        }
    }
    if r != 0 {
        return r as c_long;
    }
    if {
        r = f.read_long(em(O::DeState, m));
        r == 0
    } && {
        r = f.read_f32(em(O::DeWind, m));
        r == 0
    } && {
        r = f.read_f32(em(O::DeWindTarget, m));
        r == 0
    } && {
        r = f.read_f32(em(O::DeWindRate, m));
        r == 0
    } && {
        r = f.read_ulong(em(O::DeNextStep, m));
        r == 0
    } && {
        r = f.read_long(em(O::DeInterval, m));
        r == 0
    } && {
        r = h(O::DeColorLoad, hp(O::DeC24, m, &[]), &[pa(f.0)]);
        r == 0
    } && {
        r = h(O::DeColorLoad, hp(O::DeC18, m, &[]), &[pa(f.0)]);
        r == 0
    } {
        r = f.read_ulong(em(O::DeL50, m));
    }
    r as c_long
}

/// `RepairPointers`: the loaded storms reconnected, the ones that fail deleted; 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_repair(manager: *mut c_void) -> c_long {
    let m = manager as P;
    let mut i = storms(m) - 1;
    while i >= 0 {
        let s = storm_at(m, i);
        if ee_gamecpp_storm_restore(s as *mut c_void) != 0 {
            h(O::DeStormDelete, s, &[]);
            h(O::DeStormErase, m, &[i]);
        }
        i -= 1;
    }
    0
}

/// `Save`: as `Load`; the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_save(manager: *mut c_void, file: *mut c_void) -> c_long {
    let (m, f) = (manager as P, Pf(file as P));
    let count = storms(m) as c_ulong;
    let mut r = f.write_ulong(count);
    if r != 0 {
        return r as c_long;
    }
    for i in 0..count {
        if r == 0 {
            r = ee_gamecpp_storm_save(storm_at(m, i as i64) as *mut c_void, file) as i64;
        }
    }
    if r != 0 {
        return r as c_long;
    }
    if {
        r = f.write_long(*em(O::DeState, m));
        r == 0
    } && {
        r = f.write_f32(*em(O::DeWind, m));
        r == 0
    } && {
        r = f.write_f32(*em(O::DeWindTarget, m));
        r == 0
    } && {
        r = f.write_f32(*em(O::DeWindRate, m));
        r == 0
    } && {
        r = f.write_ulong(*em(O::DeNextStep, m));
        r == 0
    } && {
        r = f.write_long(*em(O::DeInterval, m));
        r == 0
    } && {
        r = h(O::DeColorSave, hp(O::DeC24, m, &[]), &[pa(f.0)]);
        r == 0
    } && {
        r = h(O::DeColorSave, hp(O::DeC18, m, &[]), &[pa(f.0)]);
        r == 0
    } {
        r = f.write_ulong(*em(O::DeL50, m));
    }
    r as c_long
}

/// `SetEnabled`: off deletes every storm and unloads the rain and snow effects, on loads them.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_set_enabled(manager: *mut c_void, enabled: bool) {
    let m = manager as P;
    let on = em::<bool>(O::DeEnabled, m);
    if *on == enabled {
        return;
    }
    *on = enabled;
    if !enabled {
        ee_gamecpp_env_delete_storms(manager);
        h(O::GfxEffectUnload, 0, &[EFFECT_RAIN]);
        h(O::GfxEffectUnload, 0, &[EFFECT_SNOW]);
    } else {
        h(O::GfxEffectLoad, 0, &[EFFECT_RAIN, 0]);
        h(O::GfxEffectLoad, 0, &[EFFECT_SNOW, 0]);
    }
}

/// `DeleteStorms`: every storm released and deleted, the list emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_env_delete_storms(manager: *mut c_void) {
    let m = manager as P;
    let mut i = storms(m) - 1;
    while i >= 0 {
        let s = storm_at(m, i);
        ee_gamecpp_storm_release(s as *mut c_void);
        h(O::DeStormDelete, s, &[]);
        i -= 1;
    }
    h(O::DeStormClear, m, &[]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_rectangles_grow_clamped_in_the_cpp_order() {
        let r = EeGamecppStormRect::at([2, 50]).grown(4, 52);
        assert_eq!(r, EeGamecppStormRect { bottom: 52, left: 0, right: 6, top: 46 });
        assert!(r.contains([0, 46]) && r.contains([6, 52]) && !r.contains([7, 50]));
        assert_eq!(std::mem::size_of::<EeGamecppStormRect>(), 4 * std::mem::size_of::<c_long>());
        let a = r.astar();
        assert_eq!((a.bottom, a.left, a.right, a.top), (52, 0, 6, 46));
    }

    #[test]
    fn the_wind_steps_and_its_rate_clamps() {
        assert_eq!(wind_toward(1.0, 2.0, 0.5), 1.5);
        assert_eq!(wind_toward(1.0, 1.2, 0.5), 1.2);
        assert_eq!(wind_toward(2.0, 1.0, 0.5), 1.5);
        assert_eq!(wind_toward(2.0, 1.9, 0.5), 1.9);
        assert_eq!(wind_rate(WIND_MINIMUM, WIND_MINIMUM + 1e-12), WIND_STEP_MINIMUM);
        assert_eq!(wind_rate(0.0, 1.0), WIND_STEP_MAXIMUM);
        let d = WIND_MAXIMUM - WIND_MINIMUM;
        assert_eq!(wind_rate(WIND_MINIMUM, WIND_MAXIMUM), (d as f64 / 15.0) as f32);
    }
}
