//! The C interface of `effect_state.rs`: the effects' records, their base classes' Load and Save, their region, the
//! effects' random generator and the smoke generators. Each call works on a copy of the record and writes it back
//! when it returns, so no Rust reference to the record lives across the C++'s callbacks (the file, the parts, the
//! world).

use std::ffi::c_void;

use ee_ge::types::Ge3DPoint;

use crate::effect_state::{
    self, DamagedHost, EeGfxAmbientNew, EeGfxDamagedTick, EeGfxEffectLists, EeGfxEffectLoaded, EeGfxEffectPart,
    EeGfxEffectRecord, EeGfxParticleSetUp, EeGfxParticleTick, EeGfxPlumeTick, EeGfxSmokeSetUp, EeGfxSmokeTick,
    EffectFile, EffectParts, ParticleGround, SmokeHost,
};

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

#[cfg(test)]
mod tests {
    use super::*;

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
