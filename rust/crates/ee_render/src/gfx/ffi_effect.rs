//! The C interface of `effect_state.rs`: the effects' records, their base classes' Load and Save, their region, the
//! effects' random generator and the smoke generators. Each call works on a copy of the record and writes it back
//! when it returns, so no Rust reference to the record lives across the C++'s callbacks (the file, the parts, the
//! world).

use std::ffi::c_void;

use ee_base::math::types::Ge3DPoint;

use crate::gfx::effect_state::{
    self, DamagedHost, EeGfxAmbientNew, EeGfxDamagedTick, EeGfxEffectLists, EeGfxEffectRecord, EffectFile,
    ParticleGround,
};
use crate::gfx::effect_storm::{self, EeGfxStormDraw, EeGfxStormLists, StormHost};

/// The C++ glue's view of an `FSFile&` for the effects (the typed reads and writes, each the `FSFile` call of that
/// type; `context` is the file).
#[repr(C)]
pub struct EeGfxEffectFile {
    pub context: *mut c_void,
    pub is_open: extern "C" fn(context: *mut c_void) -> bool,
    pub read_u32: extern "C" fn(context: *mut c_void, value: *mut u32) -> i32,
    pub read_i32: extern "C" fn(context: *mut c_void, value: *mut i32) -> i32,
    pub read_f32: extern "C" fn(context: *mut c_void, value: *mut f32) -> i32,
    pub read_bool: extern "C" fn(context: *mut c_void, value: *mut bool) -> i32,
    pub write_u32: extern "C" fn(context: *mut c_void, value: u32) -> i32,
    pub write_i32: extern "C" fn(context: *mut c_void, value: i32) -> i32,
    pub write_f32: extern "C" fn(context: *mut c_void, value: f32) -> i32,
    pub write_bool: extern "C" fn(context: *mut c_void, value: bool) -> i32,
}

pub(crate) struct File<'a>(pub(crate) &'a EeGfxEffectFile);

impl EffectFile for File<'_> {
    fn is_open(&mut self) -> bool {
        (self.0.is_open)(self.0.context)
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        (self.0.read_u32)(self.0.context, v)
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        (self.0.read_i32)(self.0.context, v)
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        (self.0.read_f32)(self.0.context, v)
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        (self.0.read_bool)(self.0.context, v)
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        (self.0.write_u32)(self.0.context, v)
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        (self.0.write_i32)(self.0.context, v)
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        (self.0.write_f32)(self.0.context, v)
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        (self.0.write_bool)(self.0.context, v)
    }
}

/// The world a smoke generator calls into (`context`: the effect).
#[repr(C)]
pub struct EeGfxSmokeHost {
    pub context: *mut c_void,
    pub tag_point: extern "C" fn(context: *mut c_void, index: u32, point: *mut Ge3DPoint),
    pub place: extern "C" fn(context: *mut c_void, point: *const Ge3DPoint) -> i32,
    pub line: extern "C" fn(context: *mut c_void, start: *const Ge3DPoint, direction: *const Ge3DPoint),
}

/// Runs `f` on a copy of the record and writes the copy back.
///
/// # Safety
/// `r` is a record from `ee_gfx_effect_record_new`, not freed.
unsafe fn on_copy<T>(r: *mut EeGfxEffectRecord, f: impl FnOnce(&mut EeGfxEffectRecord) -> T) -> T {
    // SAFETY: the function's contract; the copy is the only reference while `f` runs
    let mut local = unsafe { r.read() };
    let res = f(&mut local);
    // SAFETY: as above
    unsafe { r.write(local) };
    res
}

/// A new effect's record (`Init`'s values, the identity, `kind`), freed by `ee_gfx_effect_record_free`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_record_new(kind: i32) -> *mut EeGfxEffectRecord {
    Box::into_raw(Box::new(EeGfxEffectRecord::new(kind)))
}

/// Frees a record (null: nothing).
///
/// # Safety
/// `r` is null or from `ee_gfx_effect_record_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_record_free(r: *mut EeGfxEffectRecord) {
    if !r.is_null() {
        // SAFETY: the function's contract
        let record = unsafe { Box::from_raw(r) };
        if !record.damaged.lists.is_null() {
            // SAFETY: the lists came from `take_lists`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.damaged.lists) });
        }
        if !record.storm.lists.is_null() {
            // SAFETY: the lists came from `put_storm`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.storm.lists) });
        }
        if !record.storm.draws.is_null() {
            // SAFETY: the draws came from `effect_weather`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.storm.draws) });
        }
        if !record.orbit.orbitals.is_null() {
            // SAFETY: the orbitals came from `effect_orbit`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.orbit.orbitals) });
        }
        if !record.vent.parts.is_null() {
            // SAFETY: the jets came from `effect_vent`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.vent.parts) });
        }
        if !record.volcano.mesh.is_null() {
            // SAFETY: the mesh came from `effect_volcano`'s Box and is owned by the record
            drop(unsafe { Box::from_raw(record.volcano.mesh) });
        }
        if !record.marching.lists.is_null() {
            // SAFETY: the lists came from `effect_icons`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.marching.lists) });
        }
        if !record.glow.spots.is_null() {
            // SAFETY: the spots came from `effect_glow`'s Box and are owned by the record
            drop(unsafe { Box::from_raw(record.glow.spots) });
        }
    }
}

/// The damage controller's lists taken out of the record for a call (empty when it has none yet).
///
/// # Safety
/// `r` is a live record.
unsafe fn take_lists(r: *mut EeGfxEffectRecord) -> EeGfxEffectLists {
    // SAFETY: the function's contract
    unsafe {
        let p = (*r).damaged.lists;
        if p.is_null() {
            EeGfxEffectLists::default()
        } else {
            std::mem::take(&mut *p)
        }
    }
}

/// Puts the lists back into the record (made on first use).
///
/// # Safety
/// `r` is a live record.
unsafe fn put_lists(r: *mut EeGfxEffectRecord, lists: EeGfxEffectLists) {
    // SAFETY: the function's contract
    unsafe {
        let p = (*r).damaged.lists;
        if p.is_null() {
            (*r).damaged.lists = Box::into_raw(Box::new(lists));
        } else {
            *p = lists;
        }
    }
}

/// The terrain a bouncing particle asks (`gWorld.mMap->mTerrainMesh`; `context`: unused).
#[repr(C)]
pub struct EeGfxParticleGround {
    pub context: *mut c_void,
    pub is_valid_tile: extern "C" fn(context: *mut c_void, x: u32, y: u32) -> bool,
    pub is_space_tile: extern "C" fn(context: *mut c_void, x: u32, y: u32) -> bool,
    pub mesh_z: extern "C" fn(context: *mut c_void, x: f32, y: f32) -> f32,
    pub mesh_normal: extern "C" fn(context: *mut c_void, x: f32, y: f32, normal: *mut Ge3DPoint),
}

pub(crate) struct Ground<'a>(pub(crate) &'a EeGfxParticleGround);

extern "C" fn flat_valid(_c: *mut c_void, _x: u32, _y: u32) -> bool {
    true
}
extern "C" fn flat_space(_c: *mut c_void, _x: u32, _y: u32) -> bool {
    false
}
extern "C" fn flat_z(_c: *mut c_void, _x: f32, _y: f32) -> f32 {
    0.0
}
extern "C" fn flat_normal(_c: *mut c_void, _x: f32, _y: f32, n: *mut Ge3DPoint) {
    // SAFETY: the caller's out pointer
    unsafe { *n = Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 } };
}

/// Flat ground at height 0 everywhere (the tests' hosts' particle ground).
pub(crate) fn flat_ground() -> EeGfxParticleGround {
    EeGfxParticleGround {
        context: std::ptr::null_mut(),
        is_valid_tile: flat_valid,
        is_space_tile: flat_space,
        mesh_z: flat_z,
        mesh_normal: flat_normal,
    }
}

impl ParticleGround for Ground<'_> {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool {
        (self.0.is_valid_tile)(self.0.context, x, y)
    }
    fn is_space_tile(&mut self, x: u32, y: u32) -> bool {
        (self.0.is_space_tile)(self.0.context, x, y)
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        (self.0.mesh_z)(self.0.context, x, y)
    }
    fn mesh_normal(&mut self, x: f32, y: f32) -> Ge3DPoint {
        let mut n = Ge3DPoint::default();
        (self.0.mesh_normal)(self.0.context, x, y, &mut n);
        n
    }
}

/// `EEE2DBattlefieldAmbient`'s constructor at a point (its draws and the terrain's corners).
///
/// # Safety
/// `r` is a live record; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_ambient_new(
    r: *mut EeGfxEffectRecord,
    at: *const Ge3DPoint,
    n: *const EeGfxAmbientNew,
    ground: *const EeGfxParticleGround,
) {
    // SAFETY: the function's contract
    unsafe {
        let (p, a) = (*at, *n);
        on_copy(r, |r| effect_state::ambient_new(r, p, &a, &mut Ground(&*ground)))
    }
}

/// The world the damage controller calls into (`context`: the effect).
#[repr(C)]
pub struct EeGfxDamagedHost {
    pub context: *mut c_void,
    /// `CreateUnitEffect(unit, linked effect 1 (or 2: `secondary`), &handle, 0, index, 0)`: the handle
    pub create: extern "C" fn(context: *mut c_void, secondary: bool, index: u32) -> u32,
    pub destroy: extern "C" fn(context: *mut c_void, handle: u32),
}

struct Damaged<'a>(&'a EeGfxDamagedHost);

impl DamagedHost for Damaged<'_> {
    fn create(&mut self, secondary: bool, index: u32) -> u32 {
        (self.0.create)(self.0.context, secondary, index)
    }
    fn destroy(&mut self, handle: u32) {
        (self.0.destroy)(self.0.context, handle)
    }
}

/// `EEEDamagedEffect::Step`: 7 when it ends. The lists are out of the record while the C++ creates and destroys.
///
/// # Safety
/// `r` is a live record; `tick` and `host` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_damaged_step(
    r: *mut EeGfxEffectRecord,
    tick: *const EeGfxDamagedTick,
    host: *const EeGfxDamagedHost,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let t = *tick;
        let mut lists = take_lists(r);
        let mut finished = (*r).damaged.finished;
        let res = effect_state::damaged_step(&mut finished, &mut lists, &t, &mut Damaged(&*host));
        (*r).damaged.finished = finished;
        put_lists(r, lists);
        res
    }
}

/// `EEEDamagedEffect::Load`'s own reads.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_damaged(
    r: *mut EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut lists = take_lists(r);
        let mut finished = (*r).damaged.finished;
        let res = effect_state::load_damaged(&mut finished, &mut lists, &mut File(&*file), major, minor);
        (*r).damaged.finished = finished;
        put_lists(r, lists);
        res
    }
}

/// `EEEDamagedEffect::Save`'s own writes (`has_linked2`: the graphic effect's linked effect 2 is set).
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_damaged(
    r: *mut EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    has_linked2: bool,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let lists = take_lists(r);
        let finished = (*r).damaged.finished;
        let res = effect_state::save_damaged(finished, &lists, &mut File(&*file), has_linked2);
        put_lists(r, lists);
        res
    }
}

/// The emitter's line effects (`context`: the effect).
#[repr(C)]
pub struct EeGfxEmitterHost {
    pub context: *mut c_void,
    pub line: extern "C" fn(context: *mut c_void, start: *const Ge3DPoint, direction: *const Ge3DPoint) -> i32,
}

/// `EEEffect::GetRandom`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_random() -> f32 {
    effect_state::random()
}

/// The effects' random seed (TEST ONLY: the EFST capture).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_random_seed() -> u32 {
    effect_state::random_seed()
}

// ---- the weather storms (effect_storm.rs)

/// The world a storm reads and calls into (`context`: the effect).
#[repr(C)]
pub struct EeGfxStormHost {
    pub context: *mut c_void,
    /// the map has the tile
    pub on_map: extern "C" fn(context: *mut c_void, x: i32, y: i32) -> bool,
    /// the tile is deep or shallow water (false off the map)
    pub is_water: extern "C" fn(context: *mut c_void, x: i32, y: i32) -> bool,
    /// the terrain mesh's `GetZ(x, y)`
    pub ground_z: extern "C" fn(context: *mut c_void, x: i32, y: i32) -> f32,
    /// `EEGetNextTile(from, EEGetTileDirection(from, to), 1)` into `out` (x, y)
    pub step_toward: extern "C" fn(context: *mut c_void, from_x: i32, from_y: i32, to_x: i32, to_y: i32, out: *mut i32),
    /// the land ripple where a drop hit the water
    pub ripple: extern "C" fn(context: *mut c_void, x: f32, y: f32),
}

pub(crate) struct Storm<'a>(pub(crate) &'a EeGfxStormHost);

impl StormHost for Storm<'_> {
    fn on_map(&mut self, x: i32, y: i32) -> bool {
        (self.0.on_map)(self.0.context, x, y)
    }
    fn is_water(&mut self, x: i32, y: i32) -> bool {
        (self.0.is_water)(self.0.context, x, y)
    }
    fn ground_z(&mut self, x: i32, y: i32) -> f32 {
        (self.0.ground_z)(self.0.context, x, y)
    }
    fn step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2] {
        let mut out = from;
        (self.0.step_toward)(self.0.context, from[0], from[1], to[0], to[1], out.as_mut_ptr());
        out
    }
    fn ripple(&mut self, x: f32, y: f32) {
        (self.0.ripple)(self.0.context, x, y)
    }
}

/// The storm's lists taken out of the record for a call (empty when it has none yet).
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn take_storm(r: *mut EeGfxEffectRecord) -> EeGfxStormLists {
    // SAFETY: the function's contract
    unsafe {
        let p = (*r).storm.lists;
        if p.is_null() {
            EeGfxStormLists::default()
        } else {
            std::mem::take(&mut *p)
        }
    }
}

/// Puts the lists back into the record (made on first use).
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn put_storm(r: *mut EeGfxEffectRecord, lists: EeGfxStormLists) {
    // SAFETY: the function's contract
    unsafe {
        let p = (*r).storm.lists;
        if p.is_null() {
            (*r).storm.lists = Box::into_raw(Box::new(lists));
        } else {
            *p = lists;
        }
    }
}

/// Runs `f` on a copy of the record and the storm's lists taken out of it, then puts both back.
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn on_storm<T>(
    r: *mut EeGfxEffectRecord,
    f: impl FnOnce(&mut EeGfxEffectRecord, &mut EeGfxStormLists) -> T,
) -> T {
    // SAFETY: the function's contract
    unsafe {
        let mut lists = take_storm(r);
        let res = on_copy(r, |r| f(r, &mut lists));
        put_storm(r, lists);
        res
    }
}

/// The lists of a record, read only (none when it has none).
///
/// # Safety
/// `r` is a live record whose lists no call has out.
unsafe fn storm_lists<'a>(r: *const EeGfxEffectRecord) -> Option<&'a EeGfxStormLists> {
    // SAFETY: the function's contract
    unsafe { (*r).storm.lists.as_ref() }
}

/// `EEE3DStorm`'s constructor: the storm over `count` tiles (`tiles`: x, y pairs).
///
/// # Safety
/// `r` is a live record; `tiles` holds `2 * count` values (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_new(r: *mut EeGfxEffectRecord, tiles: *const i32, count: u32) {
    let mut v = Vec::with_capacity(count as usize);
    for i in 0..count as usize {
        // SAFETY: the function's contract
        unsafe { v.push([*tiles.add(2 * i), *tiles.add(2 * i + 1)]) };
    }
    // SAFETY: the function's contract
    unsafe { on_storm(r, |_, l| effect_storm::storm_new(l, &v)) }
}

/// `EEE3DRain`'s constructor: the first thunder's delay.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_rain_new(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe { on_copy(r, effect_storm::rain_new) }
}

/// `EEE3DStorm::GetCenter`: whether it has tiles; `out` gets the point on the terrain at the new centre.
///
/// # Safety
/// `r` is a live record; `host` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_center(
    r: *mut EeGfxEffectRecord,
    host: *const EeGfxStormHost,
    out: *mut Ge3DPoint,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        match on_storm(r, |r, l| effect_storm::storm_center(r, l, &mut Storm(&*host))) {
            Some(p) => {
                *out = p;
                true
            }
            None => false,
        }
    }
}

/// `EEE3DStorm::Move` towards (`to_x`, `to_y`): whether it has tiles; `bounds` (left, top, right, bottom) and `out`
/// (the point on the terrain at the new centre) are set then.
///
/// # Safety
/// `r` is a live record; `bounds` holds 4 values; `host` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_move(
    r: *mut EeGfxEffectRecord,
    to_x: i32,
    to_y: i32,
    bounds: *mut i32,
    host: *const EeGfxStormHost,
    out: *mut Ge3DPoint,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let mut b = [*bounds, *bounds.add(1), *bounds.add(2), *bounds.add(3)];
        match on_storm(r, |r, l| effect_storm::storm_move(r, l, [to_x, to_y], &mut b, &mut Storm(&*host))) {
            Some(p) => {
                for (i, v) in b.iter().enumerate() {
                    *bounds.add(i) = *v;
                }
                *out = p;
                true
            }
            None => false,
        }
    }
}

/// `EEE3DStorm::GetBounds`: whether it has tiles; `bounds` (left, top, right, bottom) is set then.
///
/// # Safety
/// `r` is a live record; `bounds` holds 4 values.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_bounds(r: *const EeGfxEffectRecord, bounds: *mut i32) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let Some(l) = storm_lists(r) else {
            return false;
        };
        let mut b = [0i32; 4];
        if !effect_storm::storm_bounds(l, &mut b) {
            return false;
        }
        for (i, v) in b.iter().enumerate() {
            *bounds.add(i) = *v;
        }
        true
    }
}

/// The storm's number of tiles (`mTiles.size()`).
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_tile_count(r: *const EeGfxEffectRecord) -> u32 {
    // SAFETY: the function's contract
    unsafe { storm_lists(r).map_or(0, |l| l.tiles.len() as u32) }
}

/// The storm's number of drops or flakes (`mDrops.size()`, `mFlakes.size()`).
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_drop_count(r: *const EeGfxEffectRecord) -> u32 {
    // SAFETY: the function's contract
    unsafe { storm_lists(r).map_or(0, |l| l.drops.len() as u32) }
}

/// `Queue`'s copy of the drops into `out` (at most `count`).
///
/// # Safety
/// `r` is a live record; `out` holds `count` draws.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_draws(r: *const EeGfxEffectRecord, out: *mut EeGfxStormDraw, count: u32) {
    // SAFETY: the function's contract
    unsafe {
        if let Some(l) = storm_lists(r) {
            if count > 0 {
                effect_storm::storm_draws(l, std::slice::from_raw_parts_mut(out, count as usize));
            }
        }
    }
}

/// The rain's thunder timer: 0, or the rumble sound (1 or 2) to play.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_rain_rumble(r: *mut EeGfxEffectRecord, tick_time: f32) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_storm::rain_rumble(r, tick_time)) }
}

#[cfg(test)]
mod tests {
    use super::*;

    extern "C" fn on_map(_: *mut c_void, x: i32, _: i32) -> bool {
        x >= 0
    }
    extern "C" fn is_water(_: *mut c_void, _: i32, _: i32) -> bool {
        false
    }
    extern "C" fn ground_z(_: *mut c_void, x: i32, y: i32) -> f32 {
        (x * 10 + y) as f32
    }
    extern "C" fn step_toward(_: *mut c_void, fx: i32, fy: i32, tx: i32, ty: i32, out: *mut i32) {
        // SAFETY: two values
        unsafe {
            *out = fx + (tx - fx).signum();
            *out.add(1) = fy + (ty - fy).signum();
        }
    }
    extern "C" fn ripple(_: *mut c_void, _: f32, _: f32) {}

    #[test]
    fn storms_own_their_tiles_through_the_interface() {
        let host = EeGfxStormHost { context: std::ptr::null_mut(), on_map, is_water, ground_z, step_toward, ripple };
        let r = ee_gfx_effect_record_new(effect_storm::EE_GFX_EFFECT_KIND_SNOW);
        // SAFETY: a live record
        unsafe {
            assert_eq!(ee_gfx_storm_tile_count(r), 0);
            let mut b = [7i32; 4];
            assert!(!ee_gfx_storm_bounds(r, b.as_mut_ptr()));
            let tiles = [1, 2, 3, 4];
            ee_gfx_storm_new(r, tiles.as_ptr(), 2);
            assert_eq!(ee_gfx_storm_tile_count(r), 2);
            let mut p = Ge3DPoint::default();
            assert!(ee_gfx_storm_center(r, &host, &mut p));
            assert_eq!(((*r).storm.center_x, (*r).storm.center_y), (2, 3));
            assert_eq!(p, Ge3DPoint { x: 2.0, y: 3.0, z: 23.0 });
            assert!(ee_gfx_storm_bounds(r, b.as_mut_ptr()));
            assert_eq!(b, [1, 2, 3, 4]);
            assert!(ee_gfx_storm_move(r, 5, 3, b.as_mut_ptr(), &host, &mut p));
            assert_eq!(((*r).storm.center_x, (*r).storm.center_y), (3, 3));
            assert_eq!(ee_gfx_storm_drop_count(r), 0);
            ee_gfx_storm_draws(r, std::ptr::null_mut(), 0);
            ee_gfx_effect_record_free(r);
        }
    }

    #[test]
    fn records_are_made_changed_and_freed_through_the_interface() {
        let r = ee_gfx_effect_record_new(0x2b);
        // SAFETY: a live record
        unsafe {
            (*r).tile_x = 4;
            (*r).handle = 9;
            assert_eq!(EeGfxEffectRecord::region_of(40.5, 3.0, 8), (5, 0));
            assert_eq!(EeGfxEffectRecord::region_of(47.9, 7.9, 8), (5, 0));
            (*r).transformation.m[0][0] = 3.0;
            (*r).init();
            assert_eq!(((*r).handle, (*r).tile_x, (*r).transformation.m[0][0]), (0, -1, 3.0));
            (*r).init_transformation();
            assert_eq!((*r).transformation.m[0][0], 1.0);
            ee_gfx_effect_record_free(r);
            ee_gfx_effect_record_free(std::ptr::null_mut());
        }
    }
}
