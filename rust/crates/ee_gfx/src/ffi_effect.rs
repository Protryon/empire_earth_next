//! The C interface of `effect_state.rs`: the effects' records, their base classes' Load and Save, their region, the
//! effects' random generator and the smoke generators. Each call works on a copy of the record and writes it back
//! when it returns, so no Rust reference to the record lives across the C++'s callbacks (the file, the parts, the
//! world).

use std::ffi::c_void;

use ee_ge::types::Ge3DPoint;

use crate::effect_state::{
    self, DamagedHost, EeGfxAmbientNew, EeGfxDamagedTick, EeGfxEffectLists, EeGfxEffectLoaded, EeGfxEffectPart,
    EeGfxEffectRecord, EeGfxEmitterBurst, EeGfxEmitterMove, EeGfxParticleSetUp, EeGfxParticleTick, EeGfxPlumeTick,
    EeGfxSmokeSetUp, EeGfxSmokeTick, EffectFile, EffectParts, EmitterHost, ParticleGround, SmokeHost,
};
use crate::effect_storm::{self, EeGfxStormDraw, EeGfxStormLists, StormHost};

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

struct File<'a>(&'a EeGfxEffectFile);

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

/// `EEEffect2D::mParts` for Load and Save (`context`: the effect).
#[repr(C)]
pub struct EeGfxEffectParts {
    pub context: *mut c_void,
    /// a loaded part: the C++ gives it the graphic object, loads its media, keeps it on success; the result
    pub push: extern "C" fn(context: *mut c_void, part: *const EeGfxEffectPart) -> i32,
    pub count: extern "C" fn(context: *mut c_void) -> u32,
    pub get: extern "C" fn(context: *mut c_void, index: u32, part: *mut EeGfxEffectPart),
}

struct Parts<'a>(&'a EeGfxEffectParts);

impl EffectParts for Parts<'_> {
    fn push(&mut self, part: &EeGfxEffectPart) -> i32 {
        (self.0.push)(self.0.context, part)
    }
    fn count(&mut self) -> u32 {
        (self.0.count)(self.0.context)
    }
    fn get(&mut self, i: u32) -> EeGfxEffectPart {
        let mut p = EeGfxEffectPart::default();
        (self.0.get)(self.0.context, i, &mut p);
        p
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

struct Host<'a>(&'a EeGfxSmokeHost);

impl SmokeHost for Host<'_> {
    fn tag_point(&mut self, i: u32) -> Ge3DPoint {
        let mut p = Ge3DPoint::default();
        (self.0.tag_point)(self.0.context, i, &mut p);
        p
    }
    fn place(&mut self, p: Ge3DPoint) -> i32 {
        (self.0.place)(self.0.context, &p)
    }
    fn line(&mut self, s: Ge3DPoint, d: Ge3DPoint) {
        (self.0.line)(self.0.context, &s, &d)
    }
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

/// `EEEffect::Init`'s part of the record.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_init(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| r.init()) }
}

/// `EEEffect3D::InitTransformation`.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_init_transformation(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| r.init_transformation()) }
}

/// `EEEffect2D::Clear`'s part of the record.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_clear_2d(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| r.clear_2d()) }
}

/// The effect region under (x, y), regions of `size` tiles (`MoveToPoint`, `MoveToTransformation`): whether it is
/// not the record's; `out_x`/`out_y` get it.
///
/// # Safety
/// `r` is a live record; the outputs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_region_changed(
    r: *const EeGfxEffectRecord,
    x: f32,
    y: f32,
    size: i32,
    out_x: *mut i32,
    out_y: *mut i32,
) -> bool {
    let (rx, ry) = EeGfxEffectRecord::region_of(x, y, size);
    // SAFETY: the function's contract
    unsafe {
        *out_x = rx;
        *out_y = ry;
        rx != (*r).tile_x || ry != (*r).tile_y
    }
}

/// `EEEffect::Load`'s reads (the C++ resolves what `out` names).
///
/// # Safety
/// `r` is a live record; `file` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load(
    r: *mut EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    out: *mut EeGfxEffectLoaded,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut loaded = EeGfxEffectLoaded::default();
        let res = on_copy(r, |r| effect_state::load_base(r, &mut File(&*file), &mut loaded));
        *out = loaded;
        res
    }
}

/// `EEEffect::Save`'s writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save(
    r: *const EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    graphic_effect_id: i32,
    unit_id: u32,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_base(&local, &mut File(&*file), graphic_effect_id, unit_id)
    }
}

/// `EEEffect3D::Load`'s own reads.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_3d(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_3d(r, &mut File(&*file))) }
}

/// `EEEffect3D::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_3d(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_3d(&local, &mut File(&*file))
    }
}

/// `EEEffect2D::Load`'s own reads.
///
/// # Safety
/// `r` is a live record; `file` and `parts` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_2d(
    r: *mut EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    parts: *const EeGfxEffectParts,
) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_2d(r, &mut File(&*file), &mut Parts(&*parts))) }
}

/// `EEEffect2D::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` and `parts` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_2d(
    r: *const EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    parts: *const EeGfxEffectParts,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_2d(&local, &mut File(&*file), &mut Parts(&*parts))
    }
}

/// `EEE3DSmokeGenerator::Load`'s own reads (and the destruction smoke's radius).
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_smoke(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_smoke(r, &mut File(&*file))) }
}

/// `EEE3DSmokeGenerator::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_smoke(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_smoke(&local, &mut File(&*file))
    }
}

/// `EEE3DSmokeGenerator::Step` (every smoke kind): 7 when it ends.
///
/// # Safety
/// `r` is a live record; `tick` and `host` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_smoke_step(
    r: *mut EeGfxEffectRecord,
    tick: *const EeGfxSmokeTick,
    host: *const EeGfxSmokeHost,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let t = *tick;
        on_copy(r, |r| effect_state::smoke_step(r, &t, &mut Host(&*host)))
    }
}

/// `EEE3DSmokeGenerator::SetUp` (the destruction smoke's lines): 0.
///
/// # Safety
/// `r` is a live record; `setup` and `host` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_smoke_set_up(
    r: *const EeGfxEffectRecord,
    setup: *const EeGfxSmokeSetUp,
    host: *const EeGfxSmokeHost,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::smoke_set_up(&local, &*setup, &mut Host(&*host))
    }
}

/// `EEE2DSmokePlume`'s constructor at a point (its draws; `velocity`: the velocity particle's, else zero).
///
/// # Safety
/// `r` is a live record; the points are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_plume_new(
    r: *mut EeGfxEffectRecord,
    start: *const Ge3DPoint,
    param1: f32,
    param2: f32,
    velocity: *const Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe {
        let (s, v) = (*start, *velocity);
        on_copy(r, |r| effect_state::plume_new(r, s, param1, param2, v))
    }
}

/// `EEE2DSmokePlume::Step` (both kinds): 7 when it ends; `moved`: the C++ sets the effect's position to the point.
///
/// # Safety
/// `r` is a live record; `tick` and `moved` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_plume_step(
    r: *mut EeGfxEffectRecord,
    tick: *const EeGfxPlumeTick,
    moved: *mut bool,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let t = *tick;
        let mut m = false;
        let res = on_copy(r, |r| effect_state::plume_step(r, &t, &mut m));
        *moved = m;
        res
    }
}

/// `EEE2DSmokePlume::Load`'s own reads (and the velocity particle's velocity).
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_plume(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_plume(r, &mut File(&*file))) }
}

/// `EEE2DSmokePlume::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_plume(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_plume(&local, &mut File(&*file))
    }
}

/// `EEE2DMarchingIcons::ComputeOffsets`' candidates (the C++ keeps those `IsIconShown` takes): up to `capacity`
/// points into `out`; their count.
///
/// # Safety
/// `out` has room for `capacity` points.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_marching_offsets(
    kind: i32,
    count: i32,
    angle: f32,
    radius: f32,
    half_side: f32,
    out: *mut Ge3DPoint,
    capacity: usize,
) -> usize {
    if out.is_null() || capacity == 0 {
        return 0;
    }
    // SAFETY: the function's contract
    let out = unsafe { std::slice::from_raw_parts_mut(out, capacity) };
    effect_state::marching_offsets_into(kind, count, angle, radius, half_side, out)
}

/// The square icons' bounding radius (`sqrt(2 r r)`).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_square_icons_radius(half_side: f32) -> f32 {
    effect_state::square_radius(half_side)
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

struct Ground<'a>(&'a EeGfxParticleGround);

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

/// The particles' `SetUp` state (their draws).
///
/// # Safety
/// `r` is a live record; `setup` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_particle_set_up(r: *mut EeGfxEffectRecord, setup: *const EeGfxParticleSetUp) {
    // SAFETY: the function's contract
    unsafe {
        let s = *setup;
        on_copy(r, |r| effect_state::particle_set_up(r, &s))
    }
}

/// The particles' `Clear` state.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_particle_clear(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe { on_copy(r, effect_state::particle_clear) }
}

/// The particles' `Update` (the renderer's move): 7 when it ends.
///
/// # Safety
/// `r` is a live record; `tick` and `ground` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_particle_update(
    r: *mut EeGfxEffectRecord,
    tick: *const EeGfxParticleTick,
    ground: *const EeGfxParticleGround,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let t = *tick;
        on_copy(r, |r| effect_state::particle_update(r, &t, &mut Ground(&*ground)))
    }
}

/// The particles' `Step`: 7 when it ends; `move_to`: the C++ applies the moved position.
///
/// # Safety
/// `r` is a live record; `move_to` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_particle_step(r: *mut EeGfxEffectRecord, now: f32, move_to: *mut bool) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut m = false;
        let res = on_copy(r, |r| effect_state::particle_step(r, now, &mut m));
        *move_to = m;
        res
    }
}

/// The particles' own Load reads; `loaded`: the C++ sets the draw position and duration.
///
/// # Safety
/// `r` is a live record; `file` and `loaded` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_particle(
    r: *mut EeGfxEffectRecord,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
    loaded: *mut bool,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut l = false;
        let res = on_copy(r, |r| effect_state::load_particle(r, &mut File(&*file), major, minor, &mut l));
        *loaded = l;
        res
    }
}

/// The particles' own Save writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_particle(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_particle(&local, &mut File(&*file))
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

/// `EEE2DBattlefieldAmbient::Step`: 7 when it ends.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_ambient_step(
    r: *mut EeGfxEffectRecord,
    tick_seconds: f32,
    duration: u32,
) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::ambient_step(r, tick_seconds, duration)) }
}

/// `EEE2DBattlefieldAmbient::Load`'s own reads.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_ambient(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_ambient(r, &mut File(&*file))) }
}

/// `EEE2DBattlefieldAmbient::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_ambient(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_ambient(&local, &mut File(&*file))
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

struct Emitter<'a>(&'a EeGfxEmitterHost);

impl EmitterHost for Emitter<'_> {
    fn line(&mut self, s: Ge3DPoint, d: Ge3DPoint) -> i32 {
        (self.0.line)(self.0.context, &s, &d)
    }
}

/// `EEE3DParticleEmitter::Step`'s start: the tick in ms, added to the running time.
///
/// # Safety
/// `r` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_emitter_begin(r: *mut EeGfxEffectRecord, tick_seconds: f32) -> f32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::emitter_begin(r, tick_seconds)) }
}

/// `EEE3DParticleEmitter::Move`: 7 off the map; `out` the next position.
///
/// # Safety
/// `r` is a live record; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_emitter_move(
    r: *mut EeGfxEffectRecord,
    seconds: f32,
    m: *const EeGfxEmitterMove,
    ground: *const EeGfxParticleGround,
    out: *mut Ge3DPoint,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mv = *m;
        let mut p = Ge3DPoint::default();
        let res = on_copy(r, |r| effect_state::emitter_move(r, seconds, &mv, &mut Ground(&*ground), &mut p));
        *out = p;
        res
    }
}

/// `EEE3DParticleEmitter::Step`'s burst; the last line's result (`result` without one).
///
/// # Safety
/// `r` is a live record; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_emitter_burst(
    r: *mut EeGfxEffectRecord,
    delta: f32,
    location: *const Ge3DPoint,
    transformation: *const ee_ge::types::GeTransformation,
    burst: *const EeGfxEmitterBurst,
    result: i32,
    host: *const EeGfxEmitterHost,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let (l, t, b) = (*location, *transformation, *burst);
        on_copy(r, |r| effect_state::emitter_burst(r, delta, l, &t, &b, result, &mut Emitter(&*host)))
    }
}

/// `EEE3DParticleEmitter::Load`'s own reads.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_emitter(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_emitter(r, &mut File(&*file))) }
}

/// `EEE3DParticleEmitter::Save`'s own writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_emitter(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_emitter(&local, &mut File(&*file))
    }
}

/// The aged effects' `Step`: 7 when it ends; the ripple's `EE_GFX_EFFECT_ASK_BLOCKED` with its tile in `tile`.
///
/// # Safety
/// `r` is a live record; `tile` has room for two.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_aged_step(
    r: *mut EeGfxEffectRecord,
    tick_seconds: f32,
    duration: u32,
    tile: *mut i32,
) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let mut t = [0i32; 2];
        let res = on_copy(r, |r| effect_state::aged_step(r, tick_seconds, duration, &mut t));
        *tile = t[0];
        *tile.add(1) = t[1];
        res
    }
}

/// The aged effects' own Load reads.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_load_aged(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_copy(r, |r| effect_state::load_aged(r, &mut File(&*file))) }
}

/// The aged effects' own Save writes.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_save_aged(r: *const EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe {
        let local = r.read();
        effect_state::save_aged(&local, &mut File(&*file))
    }
}

/// `EEEffect::GetRandom`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_random() -> f32 {
    effect_state::random()
}

/// `GetEffectRandomFloat`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_random_float(min: f32, max: f32) -> f32 {
    effect_state::random_float(min, max)
}

/// `GetEffectRandomLong`.
#[no_mangle]
pub extern "C" fn ee_gfx_effect_random_long(min: i32, max: i32) -> i32 {
    effect_state::random_long(min, max)
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

struct Storm<'a>(&'a EeGfxStormHost);

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
unsafe fn take_storm(r: *mut EeGfxEffectRecord) -> EeGfxStormLists {
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
unsafe fn put_storm(r: *mut EeGfxEffectRecord, lists: EeGfxStormLists) {
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
unsafe fn on_storm<T>(
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

/// The rain's and the snow's `SetUp` (after the C++ read the graphic effect): `out` gets the point on the terrain
/// at the tiles' mean; the radius is set.
///
/// # Safety
/// `r` is a live record; `host` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_set_up(
    r: *mut EeGfxEffectRecord,
    host: *const EeGfxStormHost,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe { *out = on_storm(r, |r, l| effect_storm::storm_set_up(r, l, &mut Storm(&*host))) }
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

/// The storm's `Step` after the rain's sounds: 7 when it ends.
///
/// # Safety
/// `r` is a live record; `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_step(
    r: *mut EeGfxEffectRecord,
    tick_time: f32,
    game_speed: f32,
    host: *const EeGfxStormHost,
) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_storm(r, |r, l| effect_storm::storm_step(r, l, tick_time, game_speed, &mut Storm(&*host))) }
}

/// `RepairPointers`' respawn of the saved drops (after the C++ read the graphic effect again).
///
/// # Safety
/// `r` is a live record; `host` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_repair(r: *mut EeGfxEffectRecord, host: *const EeGfxStormHost) {
    // SAFETY: the function's contract
    unsafe { on_storm(r, |r, l| effect_storm::storm_repair(r, l, &mut Storm(&*host))) }
}

/// The storm's and the leaf's own reads after `EEEffect3D::Load`'s.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_load(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_storm(r, |r, l| effect_storm::load_storm(r, l, &mut File(&*file))) }
}

/// The storm's and the leaf's own writes after `EEEffect3D::Save`'s.
///
/// # Safety
/// `r` is a live record; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_storm_save(r: *mut EeGfxEffectRecord, file: *const EeGfxEffectFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { on_storm(r, |r, l| effect_storm::save_storm(r, l, &mut File(&*file))) }
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
            let (mut x, mut y) = (0, 0);
            assert!(ee_gfx_effect_region_changed(r, 40.5, 3.0, 8, &mut x, &mut y));
            assert_eq!((x, y), (5, 0));
            (*r).tile_x = 5;
            (*r).tile_y = 0;
            assert!(!ee_gfx_effect_region_changed(r, 47.9, 7.9, 8, &mut x, &mut y));
            (*r).transformation.m[0][0] = 3.0;
            ee_gfx_effect_init(r);
            assert_eq!(((*r).handle, (*r).tile_x, (*r).transformation.m[0][0]), (0, -1, 3.0));
            ee_gfx_effect_init_transformation(r);
            assert_eq!((*r).transformation.m[0][0], 1.0);
            ee_gfx_effect_record_free(r);
            ee_gfx_effect_record_free(std::ptr::null_mut());
        }
    }
}
