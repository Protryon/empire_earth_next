//! The effects' plain state as Rust records (`EEEffect`, `EEEffect3D`, `EEEffect2D` and the smoke generators,
//! `src/Empire Earth/Graphics Thread/EEEffect*.cpp`, `EEE3DSmokeGenerator.cpp`): one record per effect, made by its
//! constructor and freed by its destructor; the C++ members are references into it, so every reader and writer
//! keeps its spelling. The C++ keeps the effect object, its unit and graphic effect (referenced pointers), the map
//! regions' lists and every call into the world.
//!
//! Here too: the base classes' Load and Save (the same typed reads and writes in the same order, the same first
//! error, the same results ignored; the save format is unchanged), the region an effect is in, the effects' random
//! generator (the original's `URandom` file static of `EEEffect.cpp`) and the smoke generators' tick and set-up
//! (`EEE3DSmokeGenerator`, whose kinds 0x2c and 0x1e were the classes `EEE3DDamageSmoke` and `EEE3DDestructionSmoke`).
//!
//! This is simulation state: the records are saved, and the tick steps them (RUST_PLAN 2.1: the C++'s f32
//! operations in order, the draws in the original's order and count, `lrintf` for `UMFloatToLong`).

use std::sync::Mutex;

use ee_ge::types::{Ge3DPoint, GeTransformation};
use ee_ge::um;
use ee_random::URandom;

use crate::effect_icon::EeGfxEffectIcon;
use crate::effect_storm::EeGfxEffectStorm;

/// `kErrorOK`.
const OK: i32 = 0;

/// `EEE3DSmokeGenerator`'s state (and the destruction smoke's radius, kind 0x1e).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectSmoke {
    /// `mTime`: milliseconds alive (saved)
    pub time: f32,
    /// `mSinceEmit`: milliseconds since the last puff (saved)
    pub since_emit: f32,
    /// `mRadius` (kind 0x1e only): half the unit class's scale (saved)
    pub radius: f32,
}

/// `EEE2DSmokePlume`'s state (kind 0xb) and the velocity particle's (kind 0xc, the original's
/// `EEE2DVelocityParticle`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectPlume {
    /// `mAge`: 0..1 (saved)
    pub age: f32,
    /// `mStart`: the puff's position (saved)
    pub start: Ge3DPoint,
    /// `mRotation`: random 0..2 pi (saved)
    pub rotation: f32,
    /// `mBrightness`: random round param 1 by param 2, clamped to 0..1 (saved)
    pub brightness: f32,
    /// `mVelocity` (kind 0xc only): the line's direction (saved)
    pub velocity: Ge3DPoint,
}

/// `EEE2DParticle`'s and `EEE3DParticle`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectParticle {
    /// `mVelocity` (saved)
    pub velocity: Ge3DPoint,
    /// the renderer's moved position (2D `mDrawPosition`, 3D `mPosition`)
    pub draw_position: Ge3DPoint,
    /// 0..1 (2D `mDrawAge`, 3D `mLifeFraction`)
    pub draw_age: f32,
    /// `mAge`: ms (saved)
    pub age: f32,
    /// 2D `mRotation`, 3D `mYaw` (saved), 3D `mPitch`, `mRoll` (saved)
    pub yaw: f32,
    pub pitch: f32,
    pub roll: f32,
    /// `mBrightness` (saved)
    pub brightness: f32,
    /// `mLastTime`: the game clock at the last move (saved)
    pub last_time: f32,
    /// `mDuration`
    pub duration: f32,
    /// `mMoved`: the renderer moved it (the tick applies the position)
    pub moved: bool,
}

/// `EEE2DBattlefieldAmbient`'s state (a ground decal: shell holes, blood stains).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectAmbient {
    /// `mType`: the graphic effect's int param 1 (saved)
    pub kind: u32,
    /// `mOnSlope`: 1 when the corners are not level (saved)
    pub on_slope: u32,
    /// `mCenter` (saved)
    pub center: Ge3DPoint,
    /// `mCorners`: on the terrain (saved)
    pub corners: [Ge3DPoint; 4],
    /// `mTransformation`: its position is the centre (saved)
    pub transformation: GeTransformation,
    /// `mAge`: 0..1 over the effect's duration (saved)
    pub age: f32,
    /// `mRotation`, `mSize` (saved)
    pub rotation: f32,
    pub size: f32,
    /// `mHidden`: 1 off the map or on a slope
    pub hidden: f32,
}

impl Default for EeGfxEffectAmbient {
    fn default() -> EeGfxEffectAmbient {
        EeGfxEffectAmbient {
            kind: 0,
            on_slope: 0,
            center: Ge3DPoint::default(),
            corners: [Ge3DPoint::default(); 4],
            transformation: identity(),
            age: 0.0,
            rotation: 0.0,
            size: 0.0,
            hidden: 0.0,
        }
    }
}

/// `EEEDamagedEffect`'s state: its child effects' handles (owned: made on first use, freed with the record) and the
/// finished flag (the fire functor sets it).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectDamaged {
    pub lists: *mut EeGfxEffectLists,
    pub finished: bool,
}

impl Default for EeGfxEffectDamaged {
    fn default() -> EeGfxEffectDamaged {
        EeGfxEffectDamaged { lists: std::ptr::null_mut(), finished: false }
    }
}

/// `EEE3DParticleEmitter`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectEmitter {
    /// `mVelocity` (saved)
    pub velocity: Ge3DPoint,
    /// `mTagPoint`: the unit's tag point it follows (saved)
    pub tag_point: u32,
    /// `mTime`: ms running (saved)
    pub time: f32,
    /// `mEmitTime`: ms to the next burst (saved)
    pub emit_time: f32,
}

/// The aged effects' state (water splash, shock wave, screen flash, water ripple).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectAged {
    /// `mAge`: 0..1 over the duration (saved)
    pub age: f32,
    /// the water ripple's own `mTransformation`, at water level (saved)
    pub transformation: GeTransformation,
}

impl Default for EeGfxEffectAged {
    fn default() -> EeGfxEffectAged {
        EeGfxEffectAged { age: 0.0, transformation: identity() }
    }
}

/// An effect's plain state: `EEEffect`'s, `EEEffect3D`'s, `EEEffect2D`'s and the leaves' that moved.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectRecord {
    /// `mKind`
    pub kind: i32,
    /// `mHandle` (saved)
    pub handle: u32,
    /// `mPlayer` (the local-only test; not saved)
    pub player: i32,
    /// `mTileX` / `mTileY`: the effect region it is in (-1: none; saved)
    pub tile_x: i32,
    pub tile_y: i32,
    /// `mKeepsUnit` (saved)
    pub keeps_unit: bool,
    /// `mIsLocalOnly`
    pub local_only: bool,
    /// `mIsSaved`
    pub saved: bool,
    /// `EEEffect3D::mTransformation` (saved)
    pub transformation: GeTransformation,
    /// `EEEffect3D::mBoundingRadius` and `EEEffect2D::mBoundingRadius` (saved)
    pub bounding_radius: f32,
    /// `EEEffect2D::mPosition` (saved)
    pub position: Ge3DPoint,
    /// `EEE3DSmokeGenerator`'s
    pub smoke: EeGfxEffectSmoke,
    /// `EEE2DSmokePlume`'s
    pub plume: EeGfxEffectPlume,
    /// the pooled particles'
    pub particle: EeGfxEffectParticle,
    /// the battlefield decals'
    pub ambient: EeGfxEffectAmbient,
    /// the damage controller's
    pub damaged: EeGfxEffectDamaged,
    /// the particle emitter's
    pub emitter: EeGfxEffectEmitter,
    /// the aged effects'
    pub aged: EeGfxEffectAged,
    /// the weather storms' (`effect_storm.rs`)
    pub storm: EeGfxEffectStorm,
    /// the 3D icons' (`effect_icon.rs`)
    pub icon: EeGfxEffectIcon,
}

/// The identity (`GETransformation()`).
pub fn identity() -> GeTransformation {
    let mut t = GeTransformation::default();
    for i in 0..4 {
        t.m[i][i] = 1.0;
    }
    t
}

impl EeGfxEffectRecord {
    /// A new effect's record: `Init`'s values (no handle, on no tile, everyone's, saved), the identity, no position.
    pub fn new(kind: i32) -> EeGfxEffectRecord {
        EeGfxEffectRecord {
            kind,
            handle: 0,
            player: 0,
            tile_x: -1,
            tile_y: -1,
            keeps_unit: false,
            local_only: false,
            saved: true,
            transformation: identity(),
            bounding_radius: 0.0,
            position: Ge3DPoint::default(),
            smoke: EeGfxEffectSmoke::default(),
            plume: EeGfxEffectPlume::default(),
            particle: EeGfxEffectParticle::default(),
            ambient: EeGfxEffectAmbient::default(),
            damaged: EeGfxEffectDamaged::default(),
            emitter: EeGfxEffectEmitter::default(),
            aged: EeGfxEffectAged::default(),
            storm: EeGfxEffectStorm::default(),
            icon: EeGfxEffectIcon::default(),
        }
    }

    /// `EEEffect::Init`'s part (the C++ clears the unit and graphic effect pointers).
    pub fn init(&mut self) {
        self.handle = 0;
        self.tile_x = -1;
        self.tile_y = -1;
        self.player = 0;
        self.keeps_unit = false;
        self.saved = true;
        self.local_only = false;
    }

    /// `EEEffect3D::InitTransformation`.
    pub fn init_transformation(&mut self) {
        self.transformation = identity();
        self.bounding_radius = 0.0;
    }

    /// `EEEffect2D::Clear`'s part (the C++ drops the parts and the model pointer).
    pub fn clear_2d(&mut self) {
        self.position = Ge3DPoint::default();
        self.bounding_radius = 0.0;
    }

    /// `MoveToPoint` / `MoveToTransformation`'s region: the region under (x, y) for regions of `size` tiles; whether
    /// it differs from the one the effect is in (the C++ then leaves the old region's list, `set_region`s and joins
    /// the new one).
    pub fn region_of(x: f32, y: f32, size: i32) -> (i32, i32) {
        let rx = um::floor(x);
        let ry = um::floor(y);
        (rx.wrapping_div(size), ry.wrapping_div(size))
    }
}

// ---- the save file

/// An `FSFile` as the typed reads and writes the effects make (each returns the C++'s `long`; 0: done).
pub trait EffectFile {
    fn is_open(&mut self) -> bool;
    fn read_u32(&mut self, v: &mut u32) -> i32;
    fn read_i32(&mut self, v: &mut i32) -> i32;
    fn read_f32(&mut self, v: &mut f32) -> i32;
    fn read_bool(&mut self, v: &mut bool) -> i32;
    fn write_u32(&mut self, v: u32) -> i32;
    fn write_i32(&mut self, v: i32) -> i32;
    fn write_f32(&mut self, v: f32) -> i32;
    fn write_bool(&mut self, v: bool) -> i32;
}

/// `GE3DPoint::Load(FSFile&)`: 4 on a closed file.
pub(crate) fn load_point<F: EffectFile>(f: &mut F, p: &mut Ge3DPoint) -> i32 {
    if !f.is_open() {
        return 4;
    }
    let mut r = f.read_f32(&mut p.x);
    if r == OK {
        r = f.read_f32(&mut p.y);
        if r == OK {
            r = f.read_f32(&mut p.z);
        }
    }
    r
}

/// `GE3DPoint::Save(FSFile&)`.
pub(crate) fn save_point<F: EffectFile>(f: &mut F, p: &Ge3DPoint) -> i32 {
    if !f.is_open() {
        return 4;
    }
    let mut r = f.write_f32(p.x);
    if r == OK {
        r = f.write_f32(p.y);
        if r == OK {
            r = f.write_f32(p.z);
        }
    }
    r
}

/// `GETransformation::Load(FSFile&)`: row by row, stopping at the first error.
fn load_transformation<F: EffectFile>(f: &mut F, t: &mut GeTransformation) -> i32 {
    let mut r = OK;
    let mut i = 0;
    while i < 4 && r == OK {
        let mut j = 0;
        while j < 4 && r == OK {
            r = f.read_f32(&mut t.m[i][j]);
            j += 1;
        }
        i += 1;
    }
    r
}

/// `GETransformation::Save(FSFile&)`.
fn save_transformation<F: EffectFile>(f: &mut F, t: &GeTransformation) -> i32 {
    let mut r = OK;
    let mut i = 0;
    while i < 4 && r == OK {
        let mut j = 0;
        while j < 4 && r == OK {
            r = f.write_f32(t.m[i][j]);
            j += 1;
        }
        i += 1;
    }
    r
}

/// What `EEEffect::Load` read for the C++: the graphic effect's id (when read: the C++ resolves, references and
/// loads it) and the unit's saved id (when its read was made: the C++ keeps it in the unit pointer for
/// RepairPointers, even after a failed read, as the original).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxEffectLoaded {
    pub graphic_effect_id: u32,
    pub has_graphic_effect: bool,
    pub unit_id: u32,
    pub has_unit: bool,
}

/// `EEEffect::Load`: the graphic effect id, the region, the unit's id, the handle, the keeps-unit flag; the first
/// error.
pub fn load_base<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, out: &mut EeGfxEffectLoaded) -> i32 {
    let mut value = 0u32;
    let res = f.read_u32(&mut value);
    if res != OK {
        return res;
    }
    out.graphic_effect_id = value;
    out.has_graphic_effect = true;
    let res = f.read_i32(&mut r.tile_x);
    if res != OK {
        return res;
    }
    let res = f.read_i32(&mut r.tile_y);
    if res != OK {
        return res;
    }
    let res = f.read_u32(&mut value);
    out.unit_id = value;
    out.has_unit = true;
    if res != OK {
        return res;
    }
    let mut res = f.read_u32(&mut r.handle);
    if res == OK {
        res = f.read_bool(&mut r.keeps_unit);
    }
    res
}

/// `EEEffect::Save`: the graphic effect's id (the C++'s `long` copy), the region, the unit's id (-1: none), the
/// handle, the keeps-unit flag; the first error.
pub fn save_base<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F, graphic_effect_id: i32, unit_id: u32) -> i32 {
    let res = f.write_i32(graphic_effect_id);
    if res != OK {
        return res;
    }
    let res = f.write_i32(r.tile_x);
    if res != OK {
        return res;
    }
    let res = f.write_i32(r.tile_y);
    if res != OK {
        return res;
    }
    let res = f.write_u32(unit_id);
    if res != OK {
        return res;
    }
    let mut res = f.write_u32(r.handle);
    if res == OK {
        res = f.write_bool(r.keeps_unit);
    }
    res
}

/// `EEEffect3D::Load` after the base's (whose result it ignores): the transformation and the radius.
pub fn load_3d<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = load_transformation(f, &mut r.transformation);
    if res == OK {
        res = f.read_f32(&mut r.bounding_radius);
    }
    res
}

/// `EEEffect3D::Save` after the base's (result ignored).
pub fn save_3d<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = save_transformation(f, &r.transformation);
    if res == OK {
        res = f.write_f32(r.bounding_radius);
    }
    res
}

/// A 2D effect's part as saved (`EEEffect2DPart` without its graphic object and draw frame).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectPart {
    pub position: Ge3DPoint,
    pub tag: u32,
    pub frame: u32,
    pub time: f32,
    pub done: bool,
}

/// The C++'s parts (`EEEffect2D::mParts`) as `load_2d` and `save_2d` use them.
pub trait EffectParts {
    /// a loaded part: the C++ gives it the effect's graphic object, loads its media and keeps it when that
    /// succeeded; the media's result
    fn push(&mut self, part: &EeGfxEffectPart) -> i32;
    fn count(&mut self) -> u32;
    fn get(&mut self, i: u32) -> EeGfxEffectPart;
}

/// `EEEffect2D::Load` after the base's (result ignored): the position, the radius, the parts; the first error,
/// stopping at a part whose media fails.
pub fn load_2d<F: EffectFile, P: EffectParts>(r: &mut EeGfxEffectRecord, f: &mut F, parts: &mut P) -> i32 {
    let mut res = load_point(f, &mut r.position);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut r.bounding_radius);
    if res != OK {
        return res;
    }
    let mut count = 0i32;
    res = f.read_i32(&mut count);
    if res != OK {
        return res;
    }
    // the original's part is one local, reused: a failed read leaves the earlier part's values
    let mut part = EeGfxEffectPart::default();
    let mut i = 0;
    while i < count && res == OK {
        i += 1;
        res = load_point(f, &mut part.position);
        if res != OK {
            continue;
        }
        res = f.read_u32(&mut part.tag);
        if res != OK {
            continue;
        }
        res = f.read_u32(&mut part.frame);
        if res != OK {
            continue;
        }
        res = f.read_f32(&mut part.time);
        if res != OK {
            continue;
        }
        res = f.read_bool(&mut part.done);
        if res != OK {
            continue;
        }
        res = parts.push(&part);
    }
    res
}

/// `EEEffect2D::Save` after the base's (result ignored).
pub fn save_2d<F: EffectFile, P: EffectParts>(r: &EeGfxEffectRecord, f: &mut F, parts: &mut P) -> i32 {
    let mut res = save_point(f, &r.position);
    if res != OK {
        return res;
    }
    res = f.write_f32(r.bounding_radius);
    if res != OK {
        return res;
    }
    let count = parts.count();
    // the original writes the size through its `long` result variable
    res = f.write_i32(count as i32);
    if res != OK {
        return res;
    }
    let mut i = 0;
    while i < parts.count() && res == OK {
        let p = parts.get(i);
        i += 1;
        res = save_point(f, &p.position);
        if res != OK {
            continue;
        }
        res = f.write_u32(p.tag);
        if res != OK {
            continue;
        }
        res = f.write_u32(p.frame);
        if res != OK {
            continue;
        }
        res = f.write_f32(p.time);
        if res == OK {
            res = f.write_bool(p.done);
        }
    }
    res
}

/// `EEE3DSmokeGenerator::Load` after `EEEffect3D::Load` (result ignored): the times; kind 0x1e (the destruction
/// smoke) then the radius, whose result it returns (the generator's ignored).
pub fn load_smoke<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = f.read_f32(&mut r.smoke.time);
    if res == OK {
        res = f.read_f32(&mut r.smoke.since_emit);
    }
    if r.kind == EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE {
        res = f.read_f32(&mut r.smoke.radius);
    }
    res
}

/// `EEE3DSmokeGenerator::Save` after `EEEffect3D::Save` (result ignored); kind 0x1e then the radius.
pub fn save_smoke<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = f.write_f32(r.smoke.time);
    if res == OK {
        res = f.write_f32(r.smoke.since_emit);
    }
    if r.kind == EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE {
        res = f.write_f32(r.smoke.radius);
    }
    res
}

/// `GE3DPoint::Load` / `GEVector::Load` (the same reads).
fn load_vector<F: EffectFile>(f: &mut F, p: &mut Ge3DPoint) -> i32 {
    load_point(f, p)
}

/// `EEE2DSmokePlume::Load`'s own reads after `EEEffect2D::Load` (result ignored) and the C++'s media load: the
/// point, age, rotation, brightness, the first error; kind 0xc then the velocity, whose result it returns (the
/// puff's ignored).
pub fn load_plume<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = load_point(f, &mut r.plume.start);
    if res == OK {
        res = f.read_f32(&mut r.plume.age);
        if res == OK {
            res = f.read_f32(&mut r.plume.rotation);
            if res == OK {
                res = f.read_f32(&mut r.plume.brightness);
            }
        }
    }
    if r.kind == EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE {
        res = load_vector(f, &mut r.plume.velocity);
    }
    res
}

/// `EEE2DSmokePlume::Save`'s own writes after `EEEffect2D::Save` (result ignored); kind 0xc then the velocity.
pub fn save_plume<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = save_point(f, &r.plume.start);
    if res == OK {
        res = f.write_f32(r.plume.age);
        if res == OK {
            res = f.write_f32(r.plume.rotation);
            if res == OK {
                res = f.write_f32(r.plume.brightness);
            }
        }
    }
    if r.kind == EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE {
        res = save_point(f, &r.plume.velocity);
    }
    res
}

// ---- the effects' random generator

/// The effects' generator is one for the process: the tests that draw from it take turns.
#[cfg(test)]
pub(crate) static RANDOM_TESTS: Mutex<()> = Mutex::new(());

/// `EEEffect.cpp`'s `URandom` (default seed: 1; never reseeded, not saved).
static RANDOM: Mutex<URandom> = Mutex::new(URandom { seed: 1 });

fn with_random<T>(f: impl FnOnce(&mut URandom) -> T) -> T {
    let mut g = RANDOM.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut g)
}

/// `EEEffect::GetRandom`: the next value in [0, 1).
pub fn random() -> f32 {
    with_random(|r| r.generate())
}

/// `GetEffectRandomFloat`.
pub fn random_float(min: f32, max: f32) -> f32 {
    with_random(|r| r.generate_range(min, max))
}

/// `GetEffectRandomLong`: `UMFloatToLong(r * (max - min) + min)`, at most `max`.
pub fn random_long(min: i32, max: i32) -> i32 {
    let v = with_random(|r| r.generate());
    let res = um::lrintf(v * max.wrapping_sub(min) as f32 + min as f32);
    if res > max {
        max
    } else {
        res
    }
}

/// The generator's seed (the EFST capture).
pub fn random_seed() -> u32 {
    with_random(|r| r.seed)
}

/// Sets the seed (tests).
pub fn set_random_seed(seed: u32) {
    with_random(|r| r.seed = seed)
}

/// `GetRandomSpread`: `(0.5 - r) * spread + center`.
fn spread(spread: f32, center: f32) -> f32 {
    (0.5 - random()) * spread + center
}

// ---- the smoke generators (EEE3DSmokeGenerator; kinds 0x2b, 0x2c damage smoke, 0x1e destruction smoke)

/// The plain smoke generator (on a unit or at a point).
pub const EE_GFX_EFFECT_KIND_SMOKE_GENERATOR: i32 = 0x2b;
/// The damage smoke: puffs from a damaged unit's tag points, more often the lower its hit points.
pub const EE_GFX_EFFECT_KIND_DAMAGE_SMOKE: i32 = 0x2c;
/// The destruction smoke: lines thrown out around a destroyed unit at its set-up.
pub const EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE: i32 = 0x1e;

/// `kActionDead`.
pub const EE_GFX_EFFECT_ACTION_DEAD: i32 = 0x1773;

/// What a smoke generator's tick reads (the C++ fills it from the world; the unit's fields only with a unit).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSmokeTick {
    /// `gPhysics->GetTickSeconds()`
    pub tick_seconds: f32,
    /// `gPhysics->GetTickTime()`
    pub tick_time: f32,
    pub has_unit: bool,
    /// the unit's position (its physical model's location)
    pub unit_position: Ge3DPoint,
    /// the unit's command type (`GetCommandType()`; the damage smoke's: its command's `mType`, 0 without one)
    pub unit_command: i32,
    /// the unit's hit points and its stats' maximum (the damage smoke)
    pub hit_points: i32,
    pub max_hit_points: i32,
    /// the graphic effect's `mDuration`, `mTime1`, `mTime2`, `GetParam1()`, `GetParam2()`
    pub duration: u32,
    pub time1: u32,
    pub time2: i32,
    pub param1: f32,
    pub param2: f32,
    /// the damage smoke's tag points of kind 0 (in the unit's frame and scaled location): whether the list exists
    /// and its size
    pub has_tag_points: bool,
    pub tag_point_count: u32,
}

/// The world a smoke generator's tick and set-up call into (in order: each puff is created before the next one's
/// draws, as the original).
pub trait SmokeHost {
    /// the damage smoke's tag point `i` (`GetPoint(frame, the scaled location)`)
    fn tag_point(&mut self, i: u32) -> Ge3DPoint;
    /// `CreatePlacedEffect(point, mLinkedEffect1, NULL)`; its result
    fn place(&mut self, point: Ge3DPoint) -> i32;
    /// `CreateLineEffect(line, mLinkedEffect1, NULL)`
    fn line(&mut self, start: Ge3DPoint, direction: Ge3DPoint);
}

/// `EEE3DSmokeGenerator::Step` (and the damage smoke's): 7 when it ends.
pub fn smoke_step<H: SmokeHost>(r: &mut EeGfxEffectRecord, t: &EeGfxSmokeTick, host: &mut H) -> i32 {
    if r.kind == EE_GFX_EFFECT_KIND_DAMAGE_SMOKE {
        return damage_smoke_step(r, t, host);
    }
    let delta = t.tick_seconds * 1000.0;
    let time = delta + r.smoke.time;
    r.smoke.time = time;
    r.smoke.since_emit += delta;
    let mut result = 0;
    let mut location;
    if t.has_unit {
        location = t.unit_position;
        if t.unit_command == EE_GFX_EFFECT_ACTION_DEAD {
            result = 7;
        }
    } else {
        location = Ge3DPoint { x: r.transformation.m[0][3], y: r.transformation.m[1][3], z: r.transformation.m[2][3] };
        if time > t.duration as f32 {
            result = 7;
        }
    }
    if !(r.smoke.since_emit >= t.time1 as f32) {
        return result;
    }
    location.x = t.param1 * (0.5 - random()) + location.x;
    location.y = spread(t.param1, location.y);
    location.z = spread(t.param1, location.z);
    // the destruction smoke's puff is empty (its draws and the count down stay)
    if r.kind != EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE {
        host.place(location);
    }
    r.smoke.since_emit -= t.time1 as f32;
    result
}

/// `EEE3DDamageSmoke::Step`: while the unit lives below the effect's health share (param 2), puffs at each tag point
/// (else at the unit) every interval, which shortens with the health; 7 once it is dead or healed.
fn damage_smoke_step<H: SmokeHost>(r: &mut EeGfxEffectRecord, t: &EeGfxSmokeTick, host: &mut H) -> i32 {
    let mut result = 0;
    let mut health = t.hit_points as f32;
    health /= t.max_hit_points as f32;
    r.smoke.since_emit += t.tick_time;
    if t.unit_command != EE_GFX_EFFECT_ACTION_DEAD && health < t.param2 {
        // `(mTime2 - mTime1)`: long minus unsigned long, an unsigned 32-bit difference
        let span = (t.time2 as u32).wrapping_sub(t.time1) as f32;
        let interval = um::lrintf(span * (health / t.param2) + t.time1 as f32) as u32;
        if r.smoke.since_emit >= interval as f32 {
            if t.has_tag_points {
                for i in 0..t.tag_point_count {
                    let mut p = host.tag_point(i);
                    p.x = (0.5 - random()) * t.param1 + p.x;
                    p.y = (0.5 - random()) * t.param1 + p.y;
                    p.z = (0.5 - random()) * t.param1 + p.z;
                    result = host.place(p);
                }
            } else {
                let mut p = t.unit_position;
                p.x = (0.5 - random()) * t.param1 + p.x;
                p.y = (0.5 - random()) * t.param1 + p.y;
                p.z = (0.5 - random()) * t.param1 + p.z;
                result = host.place(p);
            }
            r.smoke.since_emit -= interval as f32;
        }
    } else {
        result = 7;
    }
    result
}

/// The destruction smoke's set-up reads: the graphic effect's `mParam2` (the speed), `GetParam1()` and
/// `mIntParam1`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSmokeSetUp {
    pub param1: f32,
    pub param2: f32,
    pub int_param1: i32,
}

/// `EEE3DDestructionSmoke::SetUp`: lines from the centre out in random directions, more for a bigger radius; the
/// other smoke generators set up nothing. 0.
pub fn smoke_set_up<H: SmokeHost>(r: &EeGfxEffectRecord, s: &EeGfxSmokeSetUp, host: &mut H) -> i32 {
    if r.kind != EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE {
        return 0;
    }
    let radius = r.smoke.radius;
    let speed = s.param2;
    let mut count = um::lrintf(radius / s.param1 * s.int_param1 as f32) as u32;
    let center = Ge3DPoint { x: r.transformation.m[0][3], y: r.transformation.m[1][3], z: r.transformation.m[2][3] };
    // one transformation for every line: only its rotation is set, its position stays the identity's
    let mut turn = identity();
    let mut offset = Ge3DPoint::default();
    while count != 0 {
        let a = random();
        let b = random();
        let distance = (1.0 - a * b) * radius;
        let yaw = random() * 6.2831855;
        turn.set_orientation(ee_ge::transform::RotationOrder::Ypr, yaw, 0.0, 0.0);
        offset.x = distance;
        let rotated = turn.apply_point(offset);
        let start = Ge3DPoint { x: rotated.x + center.x, y: rotated.y + center.y, z: center.z };
        let direction = Ge3DPoint { x: rotated.x * speed, y: rotated.y * speed, z: 0.0 };
        host.line(start, direction);
        count -= 1;
    }
    0
}

// ---- the smoke puffs (EEE2DSmokePlume; kinds 0xb, 0xc velocity particle)

/// The smoke puff.
pub const EE_GFX_EFFECT_KIND_SMOKE_PLUME: i32 = 0xb;
/// The velocity particle: a puff thrown along a line's direction, slowing down over the effect's int param 1.
pub const EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE: i32 = 0xc;

/// `EEE2DSmokePlume(point, ...)`: at the point, a random rotation, then a random brightness in param 1 +- param 2
/// clamped to 0..1 (the original's `max`/`min` macros); the velocity particle's velocity.
pub fn plume_new(r: &mut EeGfxEffectRecord, start: Ge3DPoint, param1: f32, param2: f32, velocity: Ge3DPoint) {
    let minimum = param1 - param2;
    let maximum = param1 + param2;
    r.plume.start = start;
    r.plume.age = 0.0;
    r.plume.rotation = random() * 6.2831855;
    let mut b = random() * (maximum - minimum) + minimum;
    b = if b > 0.0 { b } else { 0.0 };
    b = if b < 1.0 { b } else { 1.0 };
    r.plume.brightness = b;
    r.plume.velocity = velocity;
}

/// What a puff's tick reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxPlumeTick {
    /// `gPhysics->GetTickSeconds()` (the puff) and `GetTickTime()` (the velocity particle)
    pub tick_seconds: f32,
    pub tick_time: f32,
    /// `gWorld.mWindDirection`
    pub wind: Ge3DPoint,
    /// the graphic effect's `mDuration`, `mIntParam1`, `GetParam3()`, `mFlag1`
    pub duration: u32,
    pub int_param1: i32,
    pub param3: f32,
    pub flag1: bool,
}

/// `EEE2DSmokePlume::Step` (and `EEE2DVelocityParticle::Step`): 7 once its life is over; `moved`: the velocity
/// particle moved (the C++ then sets the effect's position to the puff's point: `SetPosition(mStart)`).
pub fn plume_step(r: &mut EeGfxEffectRecord, t: &EeGfxPlumeTick, moved: &mut bool) -> i32 {
    *moved = false;
    let p = &mut r.plume;
    if r.kind == EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE {
        let elapsed = t.tick_time;
        let step = elapsed * 0.125;
        let duration = t.duration as f32;
        p.age += elapsed / duration;
        if p.age <= 1.0 {
            let mut damping = 1.0 - p.age * duration / t.int_param1 as f32;
            if damping < 0.0 {
                damping = 0.0;
            }
            p.start.x = p.start.x + (p.velocity.x * damping + t.wind.x * step);
            p.start.y = p.start.y + (p.velocity.y * damping + t.wind.y * step);
            p.start.z = p.start.z + ((1.0 - p.age) * t.param3 * elapsed + p.velocity.z * damping + t.wind.z * step);
            *moved = true;
            return 0;
        }
        return 7;
    }
    let elapsed = t.tick_seconds * 1000.0;
    p.age += elapsed / t.duration as f32;
    let age = p.age;
    if p.age <= 1.0 {
        if !t.flag1 {
            let step = elapsed * 0.25;
            p.start.x = p.start.x + age * t.wind.x * step;
            p.start.y = p.start.y + t.wind.y * p.age * step;
            p.start.z = p.start.z + ((1.0 - p.age) * t.param3 * elapsed + t.wind.z * p.age * step);
        }
        0
    } else {
        7
    }
}

// ---- the marching icons' layouts (EEE2DMarchingIcons; kinds 0x10 circle, 0xf square)

/// The icons on a circle (the original's `EEE2DCircularMarchingIcons`).
pub const EE_GFX_EFFECT_KIND_CIRCULAR_ICONS: i32 = 0x10;
/// The icons on a square (the original's `EEE2DSquareMarchingIcons`).
pub const EE_GFX_EFFECT_KIND_SQUARE_ICONS: i32 = 0xf;

/// `ComputeOffsets`' candidates in order (the C++ keeps those `IsIconShown` takes): `count` icons spaced evenly from
/// `angle`, on the circle of `radius` (the effect's bounding radius) or the square of half side `half_side` (the
/// graphic effect's param 1).
pub fn marching_offsets(kind: i32, count: i32, angle: f32, radius: f32, half_side: f32) -> Vec<Ge3DPoint> {
    let mut out = vec![Ge3DPoint::default(); count.max(0) as usize];
    let n = marching_offsets_into(kind, count, angle, radius, half_side, &mut out);
    out.truncate(n);
    out
}

/// `marching_offsets` into `out` (no allocation: the tick makes none the original did not); the count written.
pub fn marching_offsets_into(
    kind: i32,
    count: i32,
    angle: f32,
    radius: f32,
    half_side: f32,
    out: &mut [Ge3DPoint],
) -> usize {
    if count <= 0 {
        return 0;
    }
    let step = 6.2831855 / count as f32;
    let mut n = 0;
    for i in 0..count {
        if n == out.len() {
            break;
        }
        let a = angle + i as f32 * step;
        out[n] =
            if kind == EE_GFX_EFFECT_KIND_SQUARE_ICONS { square_icon(a, half_side) } else { circle_icon(a, radius) };
        n += 1;
    }
    n
}

/// `EEE2DCircularMarchingIcons::AddIcon`'s point (the exe's pi/2, not `UMSin`'s).
fn circle_icon(angle: f32, radius: f32) -> Ge3DPoint {
    Ge3DPoint { x: um::um_cos(angle) * radius, y: um::um_cos(1.5707964 - angle) * radius, z: 0.0 }
}

/// `EEE2DSquareMarchingIcons::AddIcon`'s point: the angle mapped onto the square's perimeter, one side per quarter.
fn square_icon(angle: f32, radius: f32) -> Ge3DPoint {
    let mut angle = angle;
    let sign: i32 = if angle >= 0.0 { 1 } else { -1 };
    while sign as f32 * angle > 6.2831855 {
        angle -= sign as f32 * 6.2831855;
    }
    let mut fraction = angle.abs() * 0.15915494;
    let mut p = Ge3DPoint::default();
    if fraction > 0.875 || fraction <= 0.125 {
        p.x = radius;
        fraction += 0.125;
        if fraction > 1.0 {
            fraction -= 1.0;
        }
        p.y = (fraction * 4.0 - 0.5) * radius * 2.0;
    } else if fraction <= 0.375 {
        p.y = radius;
        p.x = -(((fraction - 0.125) * 4.0 - 0.5) * radius * 2.0);
    } else if fraction <= 0.625 {
        p.x = -radius;
        p.y = -(((fraction - 0.375) * 4.0 - 0.5) * radius * 2.0);
    } else {
        p.y = -radius;
        p.x = ((fraction - 0.625) * 4.0 - 0.5) * radius * 2.0;
    }
    p
}

/// The square's bounding radius: its half diagonal (`sqrt` through `double`).
pub fn square_radius(half_side: f32) -> f32 {
    um::sqrt(half_side * half_side + half_side * half_side)
}

// ---- the pooled particles (EEE2DParticle, kind 0x2e; EEE3DParticle, kind 0x39)

/// The 2D particle (a billboard).
pub const EE_GFX_EFFECT_KIND_PARTICLE_2D: i32 = 0x2e;
/// The 3D particle (a model).
pub const EE_GFX_EFFECT_KIND_PARTICLE_3D: i32 = 0x39;

/// What a particle's set-up reads from its graphic effect: `mIntParam1` (0: random turn), `GetParam2()`,
/// `GetParam3()` and (the 3D particle's other spelling of the same values) `mParam2`, `mParam3`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxParticleSetUp {
    pub int_param1: i32,
    pub param2: f32,
    pub param3: f32,
}

/// `EEE2DParticle::SetUp`'s and `EEE3DParticle::SetUp`'s state (the C++ sets the radius and loads the graphic):
/// age 0, the turn (2D: one draw for the rotation when int param 1 is 0; 3D: yaw, pitch and roll), then the
/// brightness in param 2 +- param 3 clamped to 0..1 (the 3D particle spells the range differently: kept).
pub fn particle_set_up(r: &mut EeGfxEffectRecord, s: &EeGfxParticleSetUp) {
    let p = &mut r.particle;
    let random_turn = s.int_param1 == 0;
    p.age = 0.0;
    if r.kind == EE_GFX_EFFECT_KIND_PARTICLE_3D {
        p.yaw = if random_turn { random() * 6.2831855 } else { 0.0 };
        p.pitch = if random_turn { random() * 6.2831855 } else { 0.0 };
        p.roll = if random_turn { random() * 6.2831855 } else { 0.0 };
        let difference = s.param2 - s.param3;
        let mut b = random() * (s.param3 + s.param2 - difference) + difference;
        b = if b > 0.0 { b } else { 0.0 };
        b = if b < 1.0 { b } else { 1.0 };
        p.brightness = b;
    } else {
        p.yaw = if random_turn { random() * 6.2831855 } else { 0.0 };
        let minimum = s.param2 - s.param3;
        let maximum = s.param2 + s.param3;
        let mut b = random() * (maximum - minimum) + minimum;
        b = if b > 0.0 { b } else { 0.0 };
        b = if b < 1.0 { b } else { 1.0 };
        p.brightness = b;
    }
}

/// What a particle's move reads (the C++ fills it per call).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxParticleTick {
    /// `gPhysics->mGameClock`, `mGameOver`
    pub now: f32,
    pub game_over: bool,
    /// `gWorld.mWindDirection`
    pub wind: Ge3DPoint,
    /// the effect's position (2D: `mPosition`; 3D: the transformation's)
    pub position: Ge3DPoint,
    /// the graphic effect's `mDuration`, `mParam4`, `mParam8`, `GetParam9()`, `GetParam10()`, `mParam11`, `mFlag2`
    pub duration: u32,
    pub param4: f32,
    pub param8: f32,
    pub param9: f32,
    pub param10: f32,
    pub param11: f32,
    pub flag2: bool,
}

/// The terrain a bouncing particle asks (`GETerrainMesh`).
pub trait ParticleGround {
    fn is_valid_tile(&mut self, x: u32, y: u32) -> bool;
    fn is_space_tile(&mut self, x: u32, y: u32) -> bool;
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
    fn mesh_normal(&mut self, x: f32, y: f32) -> Ge3DPoint;
}

/// `GEVector::DotProduct` (`v.z * n.z + v.y * n.y + v.x * n.x`).
fn dot(n: Ge3DPoint, v: Ge3DPoint) -> f32 {
    v.z * n.z + v.y * n.y + v.x * n.x
}

/// `EEE2DParticle::Update` / `EEE3DParticle::Update` (run by the renderer's `Queue`): the move by the game time since
/// the last one (drag, gravity, wind and lift, a bounce off the ground with flag 2); 7 once past the duration (the
/// 2D particle also off the map).
pub fn particle_update<G: ParticleGround>(r: &mut EeGfxEffectRecord, t: &EeGfxParticleTick, ground: &mut G) -> i32 {
    let three_d = r.kind == EE_GFX_EFFECT_KIND_PARTICLE_3D;
    let p = &mut r.particle;
    let mut result = 0;
    let elapsed = t.now - p.last_time;
    if !(elapsed < -0.0001 || elapsed > 0.0001) || (!three_d && t.game_over) {
        return 0;
    }
    p.last_time = t.now;
    p.age += elapsed;
    if !(p.age <= t.duration as f32) {
        return 7;
    }
    let mut fall = -t.param8;
    let mut pos = t.position;
    let step = elapsed / 1000.0;
    let drag = -t.param11;
    let force = Ge3DPoint { x: p.velocity.x * drag, y: p.velocity.y * drag, z: p.velocity.z * drag };
    fall += force.z;
    p.velocity.x = force.x * step + p.velocity.x;
    p.velocity.y = force.y * step + p.velocity.y;
    p.velocity.z = fall * step + p.velocity.z;
    pos.x = t.param9 * t.wind.x * step + p.velocity.x * step + pos.x;
    pos.y = t.param9 * t.wind.y * step + p.velocity.y * step + pos.y;
    pos.z = t.param9 * t.wind.z * step + t.param4 * step + p.velocity.z * step + pos.z;
    if t.flag2 {
        // the original's floor order: 2D y after x, 3D y first (both pure)
        let x = um::floor_unsigned(pos.x);
        let y = um::floor_unsigned(pos.y);
        if !ground.is_valid_tile(x, y) {
            if three_d {
                // the 3D particle freezes over an invalid tile: nothing more this time
                return 0;
            }
            result = 7;
        } else {
            let height = ground.mesh_z(pos.x, pos.y);
            if pos.z <= height && (three_d || !ground.is_space_tile(x, y)) {
                let n = ground.mesh_normal(pos.x, pos.y);
                if dot(n, p.velocity) < 0.0 {
                    pos.z = height;
                    let bounce = dot(n, p.velocity) * (-1.0 - t.param10) / dot(n, n);
                    p.velocity.x = n.x * bounce + p.velocity.x;
                    p.velocity.y = n.y * bounce + p.velocity.y;
                    p.velocity.z = n.z * bounce + p.velocity.z;
                }
            }
        }
    }
    if result == 0 {
        p.draw_age = p.age / t.duration as f32;
        p.draw_position = pos;
        p.moved = true;
    }
    result
}

/// `EEE2DParticle::Step` / `EEE3DParticle::Step`: 7 once past the duration (`now - last + age` against it: the 2D
/// particle by `<=`, the 3D by `!(>)`); else `move_to` when the renderer's move left a position to apply (the C++
/// then sets the effect there; the flag is cleared here).
pub fn particle_step(r: &mut EeGfxEffectRecord, now: f32, move_to: &mut bool) -> i32 {
    let p = &mut r.particle;
    *move_to = false;
    let t = now - p.last_time + p.age;
    let alive = if r.kind == EE_GFX_EFFECT_KIND_PARTICLE_3D { !(t > p.duration) } else { t <= p.duration };
    if !alive {
        return 7;
    }
    if p.moved {
        p.moved = false;
        *move_to = true;
    }
    0
}

/// `Clear`'s state (the C++ drops the graphic object).
pub fn particle_clear(r: &mut EeGfxEffectRecord) {
    r.particle = EeGfxEffectParticle::default();
}

/// The particles' Load after the base's (result ignored) and the C++'s media load: the velocity, age, the turn (2D
/// rotation; 3D yaw, pitch, roll), brightness, and from 0.218 the last move's clock; the first error. On success
/// the C++ sets the draw position from the effect's and the duration from the graphic effect (`loaded`).
pub fn load_particle<F: EffectFile>(
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    loaded: &mut bool,
) -> i32 {
    let three_d = r.kind == EE_GFX_EFFECT_KIND_PARTICLE_3D;
    let p = &mut r.particle;
    *loaded = false;
    let mut res = load_point(f, &mut p.velocity);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut p.age);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut p.yaw);
    if res != OK {
        return res;
    }
    if three_d {
        res = f.read_f32(&mut p.pitch);
        if res != OK {
            return res;
        }
        res = f.read_f32(&mut p.roll);
        if res != OK {
            return res;
        }
    }
    res = f.read_f32(&mut p.brightness);
    if res != OK {
        return res;
    }
    if major == 0 && minor <= 0xd9 {
        p.last_time = 0.0;
    } else {
        res = f.read_f32(&mut p.last_time);
    }
    p.moved = false;
    *loaded = true;
    res
}

/// The particles' Save after the base's (result ignored).
pub fn save_particle<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let p = &r.particle;
    let mut res = save_point(f, &p.velocity);
    if res != OK {
        return res;
    }
    res = f.write_f32(p.age);
    if res != OK {
        return res;
    }
    res = f.write_f32(p.yaw);
    if res != OK {
        return res;
    }
    if r.kind == EE_GFX_EFFECT_KIND_PARTICLE_3D {
        res = f.write_f32(p.pitch);
        if res != OK {
            return res;
        }
        res = f.write_f32(p.roll);
        if res != OK {
            return res;
        }
    }
    res = f.write_f32(p.brightness);
    if res == OK {
        res = f.write_f32(p.last_time);
    }
    res
}

// ---- the battlefield decals (EEE2DBattlefieldAmbient, kind 0x32)

/// What a decal's constructor reads from its graphic effect: `GetStartSize()`, `GetSize()`, `mIntParam1`, `mFlag1`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxAmbientNew {
    pub start_size: f32,
    pub size: f32,
    pub int_param1: i32,
    pub flag1: bool,
}

/// `EEE2DBattlefieldAmbient(point, ...)`: a random size, the four corners on the terrain; off the map or not level
/// (a rise over 0.03) it is hidden and expires at once (age 1), else a random turn when flag 1 asks and the
/// transformation at the point. The original's on-slope branch cannot run (an uneven corner hides the decal first):
/// kept.
pub fn ambient_new<G: ParticleGround>(r: &mut EeGfxEffectRecord, at: Ge3DPoint, n: &EeGfxAmbientNew, ground: &mut G) {
    let a = &mut r.ambient;
    a.age = 0.0;
    a.hidden = 0.0;
    a.kind = n.int_param1 as u32;
    a.size = n.start_size + random() * (n.size - n.start_size);
    a.center = at;
    let h = a.size * 0.5;
    a.corners[0].x = at.x - h;
    a.corners[0].y = at.y - h;
    a.corners[1].x = at.x + h;
    a.corners[1].y = at.y - h;
    a.corners[2].x = at.x + h;
    a.corners[2].y = at.y + h;
    a.corners[3].x = at.x - h;
    a.corners[3].y = at.y + h;
    let mut level = true;
    let mut first = 0.0f32;
    for i in 0..4 {
        let x = um::floor_unsigned(a.corners[i].x);
        let y = um::floor_unsigned(a.corners[i].y);
        if !ground.is_valid_tile(x, y) {
            a.hidden = 1.0;
            break;
        }
        a.corners[i].z = ground.mesh_z(a.corners[i].x, a.corners[i].y);
        let z = a.corners[i].z;
        if i == 0 {
            first = a.corners[0].z;
        } else if level {
            let rise = z - first;
            if rise > 0.03 || rise < -0.03 {
                level = false;
                a.hidden = 1.0;
            }
        }
    }
    if a.hidden != 0.0 {
        a.age = 1.0;
    } else {
        if level {
            a.on_slope = 0;
            a.rotation = if n.flag1 { random() * 6.2831855 } else { 0.0 };
        } else {
            a.on_slope = 1;
            a.rotation = 0.0;
        }
        a.transformation.m[0][3] = at.x;
        a.transformation.m[1][3] = at.y;
        a.transformation.m[2][3] = at.z;
    }
}

/// `EEE2DBattlefieldAmbient::Step`: the age by the tick over the duration; 7 past 1.
pub fn ambient_step(r: &mut EeGfxEffectRecord, tick_seconds: f32, duration: u32) -> i32 {
    r.ambient.age += (tick_seconds * 1000.0) / duration as f32;
    if r.ambient.age > 1.0 {
        7
    } else {
        0
    }
}

/// `EEE2DBattlefieldAmbient::Load`'s own reads after the base's (ignored) and the media load: the transformation,
/// age, centre, type, rotation, size, slope flag and corners, each after the last succeeded; hidden by the slope
/// flag whatever was read.
pub fn load_ambient<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let a = &mut r.ambient;
    let mut res = load_transformation(f, &mut a.transformation);
    if res == OK {
        res = f.read_f32(&mut a.age);
    }
    if res == OK {
        res = load_point(f, &mut a.center);
    }
    if res == OK {
        let mut v = a.kind as i32;
        res = f.read_i32(&mut v);
        a.kind = v as u32;
    }
    if res == OK {
        res = f.read_f32(&mut a.rotation);
    }
    if res == OK {
        res = f.read_f32(&mut a.size);
    }
    if res == OK {
        let mut v = a.on_slope as i32;
        res = f.read_i32(&mut v);
        a.on_slope = v as u32;
    }
    if res == OK {
        for c in a.corners.iter_mut() {
            res = load_point(f, c);
            if res != OK {
                break;
            }
        }
    }
    a.hidden = if a.on_slope != 0 { 1.0 } else { 0.0 };
    res
}

/// `EEE2DBattlefieldAmbient::Save`'s own writes after the base's (ignored).
pub fn save_ambient<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let a = &r.ambient;
    let mut res = save_transformation(f, &a.transformation);
    if res == OK {
        res = f.write_f32(a.age);
    }
    if res == OK {
        res = save_point(f, &a.center);
    }
    if res == OK {
        res = f.write_i32(a.kind as i32);
    }
    if res == OK {
        res = f.write_f32(a.rotation);
    }
    if res == OK {
        res = f.write_f32(a.size);
    }
    if res == OK {
        res = f.write_i32(a.on_slope as i32);
    }
    if res != OK {
        return res;
    }
    for c in a.corners.iter() {
        res = save_point(f, c);
        if res != OK {
            break;
        }
    }
    res
}

// ---- the damage controller (EEEDamagedEffect, kind 0x3e)

/// The damage controller's child effects: the handles of the graphic's linked effect 1 and 2 (`mPrimaryEffects`,
/// `mSecondaryEffects`), owned by the record (`EeGfxEffectDamaged::lists`, made on first use, freed with it).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeGfxEffectLists {
    pub primary: Vec<u32>,
    pub secondary: Vec<u32>,
}

/// What the damage controller's tick reads (the C++ fills it from the unit, the head part of a multi-part one).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxDamagedTick {
    /// the effect has a unit
    pub has_unit: bool,
    /// the damage tag points of linked effect 1's kind (1 without a list)
    pub tag_count: u32,
    /// the unit's hit points and its stats' maximum
    pub hit_points: i32,
    pub max_hit_points: i32,
    /// the graphic effect's `GetParam2()` (the health share below which the damage shows) and linked effect 2
    pub param2: f32,
    pub has_linked2: bool,
    /// the unit is on the map, cloaked, and the current player is not its ally: no child effects
    pub hidden: bool,
}

/// The world the damage controller's tick calls into, in the original's order.
pub trait DamagedHost {
    /// `CreateUnitEffect(unit, linked effect 1 or 2, &handle, 0, index, 0)`: the handle
    fn create(&mut self, secondary: bool, index: u32) -> u32;
    /// `DestroyEffect(handle)`
    fn destroy(&mut self, handle: u32);
}

/// `EEEDamagedEffect::Step`: one child effect (and its secondary) per damage tag point, more as the health drops;
/// none on a hidden unit; once finished, destroys them all and returns 7.
pub fn damaged_step<H: DamagedHost>(
    finished: &mut bool,
    lists: &mut EeGfxEffectLists,
    t: &EeGfxDamagedTick,
    host: &mut H,
) -> i32 {
    if *finished {
        for _ in 0..lists.primary.len() {
            host.destroy(lists.primary.pop().unwrap_or(0));
            if t.has_linked2 {
                host.destroy(lists.secondary.pop().unwrap_or(0));
            }
        }
        return 7;
    }
    if !t.has_unit {
        return 0;
    }
    let ratio = t.hit_points as f32 / t.max_hit_points as f32;
    let wanted: u32 = if ratio <= t.param2 && t.hit_points > 0 {
        let amount = (1.0 - ratio * (1.0 / t.param2)) * t.tag_count as f32;
        let mut w = um::lrintf(amount) as u32;
        if (w as f32) < amount {
            w = w.wrapping_add(1);
        }
        w
    } else {
        0
    };
    if wanted == 0 && t.param2 < 1.0 {
        *finished = true;
    }
    let mut delta = (wanted as i32).wrapping_sub(lists.primary.len() as i32);
    if t.hidden {
        delta = -(lists.primary.len() as i32);
    }
    if delta == 0 {
        return 0;
    }
    if delta > 0 {
        while delta > 0 {
            let h = host.create(false, lists.primary.len() as u32);
            lists.primary.push(h);
            if t.has_linked2 {
                let h2 = host.create(true, lists.secondary.len() as u32);
                lists.secondary.push(h2);
            }
            delta -= 1;
        }
    } else {
        while delta < 0 {
            host.destroy(lists.primary.pop().unwrap_or(0));
            if t.has_linked2 {
                host.destroy(lists.secondary.pop().unwrap_or(0));
            }
            delta += 1;
        }
    }
    0
}

/// `EEEDamagedEffect::Load`'s own reads after the base's (ignored): whether secondaries were saved, the handles,
/// and from 0.249 the finished flag; the first error.
pub fn load_damaged<F: EffectFile>(
    finished: &mut bool,
    lists: &mut EeGfxEffectLists,
    f: &mut F,
    major: u32,
    minor: u32,
) -> i32 {
    let mut has_second = false;
    let mut res = f.read_bool(&mut has_second);
    if res != OK {
        return res;
    }
    let mut count = 0u32;
    res = f.read_u32(&mut count);
    if res != OK {
        return res;
    }
    let mut handle = 0u32;
    let mut i = 0;
    while i < count && res == OK {
        i += 1;
        res = f.read_u32(&mut handle);
        if res != OK {
            continue;
        }
        lists.primary.push(handle);
        if !has_second {
            continue;
        }
        res = f.read_u32(&mut handle);
        if res == OK {
            lists.secondary.push(handle);
        }
    }
    if res == OK && (major > 0 || minor >= 0xf9) {
        res = f.read_bool(finished);
    }
    res
}

/// `EEEDamagedEffect::Save`'s own writes after the base's (ignored): whether linked effect 2 is set, the handles
/// (each primary with its secondary), the finished flag; the first error.
pub fn save_damaged<F: EffectFile>(finished: bool, lists: &EeGfxEffectLists, f: &mut F, has_linked2: bool) -> i32 {
    let mut res = f.write_bool(has_linked2);
    if res != OK {
        return res;
    }
    res = f.write_u32(lists.primary.len() as u32);
    if res != OK {
        return res;
    }
    for i in 0..lists.primary.len() {
        res = f.write_u32(lists.primary[i]);
        if res != OK {
            break;
        }
        if !has_linked2 {
            continue;
        }
        res = f.write_u32(lists.secondary.get(i).copied().unwrap_or(0));
        if res != OK {
            break;
        }
    }
    if res == OK {
        res = f.write_bool(finished);
    }
    res
}

// ---- the particle emitter (EEE3DParticleEmitter, kind 0x3a)

/// What the emitter's move reads: the wind and the graphic effect's `GetParam4()`, `GetParam8()`, `GetParam9()`,
/// `GetParam10()`, `mParam11`, `mFlag2`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEmitterMove {
    pub wind: Ge3DPoint,
    pub param4: f32,
    pub param8: f32,
    pub param9: f32,
    pub param10: f32,
    pub param11: f32,
    pub flag2: bool,
}

/// `EEE3DParticleEmitter::Step`'s start: the tick in ms (`GetTickSeconds() * 1000`) added to the running time;
/// returns it (the move takes it in seconds, `/ 1000`).
pub fn emitter_begin(r: &mut EeGfxEffectRecord, tick_seconds: f32) -> f32 {
    let delta = tick_seconds * 1000.0;
    r.emitter.time += delta;
    delta
}

/// `EEE3DParticleEmitter::Move`: drag, gravity, wind and rise on the velocity, the next position from the
/// transformation's; with flag 2 a bounce off the terrain, 7 off the map.
pub fn emitter_move<G: ParticleGround>(
    r: &mut EeGfxEffectRecord,
    seconds: f32,
    m: &EeGfxEmitterMove,
    ground: &mut G,
    out: &mut Ge3DPoint,
) -> i32 {
    let t = r.transformation;
    let v = &mut r.emitter.velocity;
    let mut gravity = -m.param8;
    let drag = -m.param11;
    let force = Ge3DPoint { x: v.x * drag, y: v.y * drag, z: v.z * drag };
    gravity += force.z;
    v.x = force.x * seconds + v.x;
    v.y = force.y * seconds + v.y;
    v.z = gravity * seconds + v.z;
    out.x = m.param9 * m.wind.x * seconds + v.x * seconds + t.m[0][3];
    out.y = m.param9 * m.wind.y * seconds + v.y * seconds + t.m[1][3];
    out.z = m.param9 * m.wind.z * seconds + m.param4 * seconds + v.z * seconds + t.m[2][3];
    let mut result = 0;
    if m.flag2 {
        let y = um::floor_unsigned(out.y);
        let x = um::floor_unsigned(out.x);
        if ground.is_valid_tile(x, y) {
            let height = ground.mesh_z(out.x, out.y);
            if out.z <= height {
                let n = ground.mesh_normal(out.x, out.y);
                if n.z * v.z + n.y * v.y + n.x * v.x < 0.0 {
                    out.z = height;
                    let bounce =
                        (n.z * v.z + n.y * v.y + n.x * v.x) * (-1.0 - m.param10) / (n.z * n.z + n.y * n.y + n.x * n.x);
                    v.x = n.x * bounce + v.x;
                    v.y = n.y * bounce + v.y;
                    v.z = n.z * bounce + v.z;
                }
            }
        } else {
            result = 7;
        }
    }
    result
}

/// What the emitter's burst reads: the graphic effect's `mIntParam3` (particles a burst), `mEmitPositionSpread`,
/// `mIsEmitFlat`, `GetParam1()`..`GetParam6()` (pitch, yaw and speed: centre and spread), `mIntParam1`/`mIntParam2`
/// (the countdown's centre and spread).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEmitterBurst {
    pub count: u32,
    pub position_spread: f32,
    pub flat: bool,
    pub param1: f32,
    pub param2: f32,
    pub param3: f32,
    pub param4: f32,
    pub param5: f32,
    pub param6: f32,
    pub int_param1: i32,
    pub int_param2: i32,
}

/// The emitter's line effects (`CreateLineEffect(line, mLinkedEffect1, NULL)`; its result).
pub trait EmitterHost {
    fn line(&mut self, start: Ge3DPoint, direction: Ge3DPoint) -> i32;
}

/// `EEE3DParticleEmitter::Step`'s burst (when the effect goes on): the countdown by the tick; when it runs out,
/// `count` line particles from the location, each spread, turned by a random pitch and yaw from the transformation
/// and thrown at a random speed, then a new random countdown. Returns the last line's result (`result` without a
/// burst).
pub fn emitter_burst<H: EmitterHost>(
    r: &mut EeGfxEffectRecord,
    delta: f32,
    location: Ge3DPoint,
    transformation: &GeTransformation,
    b: &EeGfxEmitterBurst,
    result: i32,
    host: &mut H,
) -> i32 {
    let mut result = result;
    r.emitter.emit_time -= delta;
    if r.emitter.emit_time <= 0.0 {
        let mut i = b.count;
        while i > 0 {
            let mut p = location;
            p.x = (0.5 - random()) * b.position_spread + p.x;
            p.y = (0.5 - random()) * b.position_spread + p.y;
            if !b.flat {
                p.z = (0.5 - random()) * b.position_spread + p.z;
            }
            let pitch = spread(b.param2, b.param1);
            let yaw = spread(b.param4, b.param3);
            let speed = spread(b.param6, b.param5);
            let mut turn = *transformation;
            turn.revolve(yaw, pitch, 0.0);
            let d = turn.apply_vector(ee_ge::types::GeVector { x: 0.0, y: 0.0, z: 1.0 });
            let direction = Ge3DPoint { x: speed * d.x, y: speed * d.y, z: speed * d.z };
            result = host.line(p, direction);
            i -= 1;
        }
        // `GetRandomSpread(long, long)`: the spread and centre as floats
        r.emitter.emit_time = (0.5 - random()) * b.int_param2 as f32 + b.int_param1 as f32;
    }
    result
}

/// `EEE3DParticleEmitter::Load`'s own reads after `EEEffect3D::Load` (ignored): the velocity, running time,
/// countdown and tag point; the first error.
pub fn load_emitter<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let e = &mut r.emitter;
    let mut res = load_point(f, &mut e.velocity);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut e.time);
    if res != OK {
        return res;
    }
    res = f.read_f32(&mut e.emit_time);
    if res == OK {
        res = f.read_u32(&mut e.tag_point);
    }
    res
}

/// `EEE3DParticleEmitter::Save`'s own writes.
pub fn save_emitter<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let e = &r.emitter;
    let mut res = save_point(f, &e.velocity);
    if res != OK {
        return res;
    }
    res = f.write_f32(e.time);
    if res != OK {
        return res;
    }
    res = f.write_f32(e.emit_time);
    if res == OK {
        res = f.write_u32(e.tag_point);
    }
    res
}

// ---- the effects that only age (EEE3DWaterSplash 0x1b, EEE2DShockwave 0x12, EEE3DScreenFlash 0x46, EEE2DWaterRipple 0x22)

/// The water splash.
pub const EE_GFX_EFFECT_KIND_WATER_SPLASH: i32 = 0x1b;
/// The shock wave.
pub const EE_GFX_EFFECT_KIND_SHOCKWAVE: i32 = 0x12;
/// The screen flash.
pub const EE_GFX_EFFECT_KIND_SCREEN_FLASH: i32 = 0x46;
/// The water ripple.
pub const EE_GFX_EFFECT_KIND_WATER_RIPPLE: i32 = 0x22;

/// An aged effect's tick asks the C++ whether the tile under the ripple is blocked (the ripple lives on while it is).
pub const EE_GFX_EFFECT_ASK_BLOCKED: i32 = 1;

/// The aged effects' `Step`: the age by the tick in ms over the duration; 7 once past 1 (the screen flash at 1).
/// The ripple, still young, sits at water level and returns `EE_GFX_EFFECT_ASK_BLOCKED` with its tile in `tile`
/// (the C++ ends it, 7, unless the tile is blocked).
pub fn aged_step(r: &mut EeGfxEffectRecord, tick_seconds: f32, duration: u32, tile: &mut [i32; 2]) -> i32 {
    let a = &mut r.aged;
    a.age += (tick_seconds * 1000.0) / duration as f32;
    match r.kind {
        EE_GFX_EFFECT_KIND_SCREEN_FLASH => {
            if a.age >= 1.0 {
                7
            } else {
                0
            }
        }
        EE_GFX_EFFECT_KIND_WATER_RIPPLE => {
            if a.age <= 1.0 {
                a.transformation.m[2][3] = 0.0;
                tile[1] = um::floor(a.transformation.m[1][3]);
                tile[0] = um::floor(a.transformation.m[0][3]);
                EE_GFX_EFFECT_ASK_BLOCKED
            } else {
                7
            }
        }
        _ => {
            if a.age > 1.0 {
                7
            } else {
                0
            }
        }
    }
}

/// The aged effects' own Load after the base's (ignored) and the media load: the ripple's transformation, then the
/// age; the first error.
pub fn load_aged<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let ripple = r.kind == EE_GFX_EFFECT_KIND_WATER_RIPPLE;
    let a = &mut r.aged;
    if ripple {
        let res = load_transformation(f, &mut a.transformation);
        if res != OK {
            return res;
        }
    }
    f.read_f32(&mut a.age)
}

/// The aged effects' own Save.
pub fn save_aged<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let a = &r.aged;
    if r.kind == EE_GFX_EFFECT_KIND_WATER_RIPPLE {
        let res = save_transformation(f, &a.transformation);
        if res != OK {
            return res;
        }
    }
    f.write_f32(a.age)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A file of typed values (the debug file's view: each write and its type).
    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
        fail_at: Option<usize>,
    }

    impl Mem {
        fn read(&mut self, ty: char) -> Result<u32, i32> {
            if self.fail_at == Some(self.at) || self.at >= self.items.len() {
                return Err(3);
            }
            let (t, v) = self.items[self.at];
            assert_eq!(t, ty, "item {}", self.at);
            self.at += 1;
            Ok(v)
        }
        fn write(&mut self, ty: char, v: u32) -> i32 {
            if self.fail_at == Some(self.items.len()) {
                return 5;
            }
            self.items.push((ty, v));
            0
        }
    }

    impl EffectFile for Mem {
        fn is_open(&mut self) -> bool {
            true
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            self.read('u').map(|x| *v = x).err().unwrap_or(0)
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            self.read('i').map(|x| *v = x as i32).err().unwrap_or(0)
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            self.read('f').map(|x| *v = f32::from_bits(x)).err().unwrap_or(0)
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            self.read('b').map(|x| *v = x != 0).err().unwrap_or(0)
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.write('u', v)
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.write('i', v as u32)
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.write('f', v.to_bits())
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.write('b', v as u32)
        }
    }

    #[derive(Default)]
    struct Parts(Vec<EeGfxEffectPart>, i32);

    impl EffectParts for Parts {
        fn push(&mut self, part: &EeGfxEffectPart) -> i32 {
            if self.1 == 0 {
                self.0.push(*part);
            }
            self.1
        }
        fn count(&mut self) -> u32 {
            self.0.len() as u32
        }
        fn get(&mut self, i: u32) -> EeGfxEffectPart {
            self.0[i as usize]
        }
    }

    fn sample() -> EeGfxEffectRecord {
        let mut r = EeGfxEffectRecord::new(0x2b);
        r.handle = 77;
        r.tile_x = 3;
        r.tile_y = -1;
        r.keeps_unit = true;
        r.transformation.m[0][3] = 12.5;
        r.transformation.m[2][1] = -0.25;
        r.bounding_radius = 1.75;
        r.position = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        r.smoke = EeGfxEffectSmoke { time: 10.0, since_emit: 2.5, radius: 0.5 };
        r
    }

    #[test]
    fn new_records_hold_init_s_values() {
        let r = EeGfxEffectRecord::new(9);
        assert_eq!((r.kind, r.handle, r.player, r.tile_x, r.tile_y), (9, 0, 0, -1, -1));
        assert!(!r.keeps_unit && !r.local_only && r.saved);
        assert_eq!(r.transformation, identity());
        let mut s = sample();
        s.player = 4;
        s.local_only = true;
        s.saved = false;
        s.init();
        assert_eq!((s.handle, s.player, s.tile_x, s.keeps_unit, s.local_only, s.saved), (0, 0, -1, false, false, true));
        assert_eq!(s.kind, 0x2b);
        s.init_transformation();
        s.clear_2d();
        assert_eq!((s.transformation, s.bounding_radius, s.position), (identity(), 0.0, Ge3DPoint::default()));
    }

    #[test]
    fn the_base_and_3d_save_in_the_original_s_order_and_types() {
        let r = sample();
        let mut f = Mem::default();
        assert_eq!(save_base(&r, &mut f, 41, u32::MAX), 0);
        assert_eq!(save_3d(&r, &mut f), 0);
        assert_eq!(save_smoke(&r, &mut f), 0);
        let types: String = f.items.iter().map(|i| i.0).collect();
        assert_eq!(types, "iiiuub".to_owned() + &"f".repeat(17) + "ff");
        assert_eq!(f.items[0].1, 41);
        assert_eq!(f.items[2].1, u32::MAX);
        assert_eq!(f.items[3].1, u32::MAX);
        assert_eq!(f.items[4].1, 77);
        // the reads take back what was written (the file's types checked)
        let mut back = EeGfxEffectRecord::new(0x2b);
        let mut g = Mem {
            items: f.items.iter().map(|&(t, v)| (if t == 'i' && v == 41 { 'u' } else { t }, v)).collect(),
            ..Default::default()
        };
        // the graphic effect id is written as a long and read as an unsigned long
        g.items[3].0 = 'u';
        let mut out = EeGfxEffectLoaded::default();
        assert_eq!(load_base(&mut back, &mut g, &mut out), 0);
        assert_eq!(load_3d(&mut back, &mut g), 0);
        assert_eq!(load_smoke(&mut back, &mut g), 0);
        assert_eq!(
            out,
            EeGfxEffectLoaded { graphic_effect_id: 41, has_graphic_effect: true, unit_id: u32::MAX, has_unit: true }
        );
        assert_eq!((back.tile_x, back.tile_y, back.handle, back.keeps_unit), (3, -1, 77, true));
        assert_eq!((back.transformation, back.bounding_radius, back.smoke.time), (r.transformation, 1.75, 10.0));
        assert_eq!(back.smoke.radius, 0.0);
    }

    #[test]
    fn a_failed_read_stops_where_the_original_does() {
        let r = sample();
        let mut f = Mem::default();
        save_base(&r, &mut f, 5, 9);
        f.items[0].0 = 'u';
        // the unit's read fails: its id is still taken, the rest is not read
        let mut g = Mem { items: f.items.clone(), fail_at: Some(3), ..Default::default() };
        let mut back = EeGfxEffectRecord::new(1);
        let mut out = EeGfxEffectLoaded::default();
        assert_eq!(load_base(&mut back, &mut g, &mut out), 3);
        assert!(out.has_graphic_effect && out.has_unit);
        assert_eq!((back.tile_x, back.handle), (3, 0));
        let mut g = Mem { items: f.items.clone(), fail_at: Some(0), ..Default::default() };
        let mut out = EeGfxEffectLoaded::default();
        assert_eq!(load_base(&mut EeGfxEffectRecord::new(1), &mut g, &mut out), 3);
        assert!(!out.has_graphic_effect && !out.has_unit);
        // a failed write stops the save
        let mut h = Mem { fail_at: Some(2), ..Default::default() };
        assert_eq!(save_base(&r, &mut h, 5, 9), 5);
        assert_eq!(h.items.len(), 2);
        // the destruction smoke's radius result is the one returned
        let mut d = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE);
        let mut m = Mem {
            items: vec![('f', 1.0f32.to_bits()), ('f', 2.0f32.to_bits()), ('f', 4.0f32.to_bits())],
            ..Default::default()
        };
        assert_eq!(load_smoke(&mut d, &mut m), 0);
        assert_eq!(d.smoke, EeGfxEffectSmoke { time: 1.0, since_emit: 2.0, radius: 4.0 });
        let mut m = Mem { items: vec![('f', 1.0f32.to_bits())], ..Default::default() };
        assert_eq!(load_smoke(&mut d, &mut m), 3);
    }

    #[test]
    fn the_2d_state_saves_its_parts_and_loads_the_kept_ones() {
        let r = sample();
        let mut parts = Parts::default();
        parts.0.push(EeGfxEffectPart {
            position: Ge3DPoint { x: 4.0, y: 5.0, z: 6.0 },
            tag: 10000,
            frame: 3,
            time: 0.5,
            done: false,
        });
        parts.0.push(EeGfxEffectPart { position: Ge3DPoint::default(), tag: 2, frame: 0, time: 0.0, done: true });
        let mut f = Mem::default();
        assert_eq!(save_2d(&r, &mut f, &mut parts), 0);
        let types: String = f.items.iter().map(|i| i.0).collect();
        assert_eq!(types, "ffffi".to_owned() + "fffuufb" + "fffuufb");
        let mut back = EeGfxEffectRecord::new(1);
        let mut got = Parts::default();
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_2d(&mut back, &mut g, &mut got), 0);
        assert_eq!(got.0, parts.0);
        assert_eq!((back.position, back.bounding_radius), (r.position, 1.75));
        // a part whose media fails stops the load (not kept)
        let mut bad = Parts(Vec::new(), 6);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_2d(&mut back, &mut g, &mut bad), 6);
        assert_eq!((bad.0.len(), g.at), (0, 12));
    }

    #[test]
    fn the_puffs_save_load_and_step() {
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE);
        r.plume = EeGfxEffectPlume {
            age: 0.25,
            start: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            rotation: 0.5,
            brightness: 0.75,
            velocity: Ge3DPoint { x: 0.5, y: -0.5, z: 0.125 },
        };
        let mut f = Mem::default();
        assert_eq!(save_plume(&r, &mut f), 0);
        let types: String = f.items.iter().map(|i| i.0).collect();
        assert_eq!(types, "ffffffff".to_owned() + "f");
        let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_plume(&mut back, &mut g), 0);
        assert_eq!(back.plume, r.plume);
        // a plain puff stops before the velocity
        let mut puff = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_PLUME);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_plume(&mut puff, &mut g), 0);
        assert_eq!((g.at, puff.plume.velocity), (6, Ge3DPoint::default()));

        // the velocity particle: damped by int param 1, carried by the wind, rising as it ages
        let t = EeGfxPlumeTick {
            tick_seconds: 0.1,
            tick_time: 100.0,
            wind: Ge3DPoint { x: 0.01, y: 0.0, z: 0.0 },
            duration: 1000,
            int_param1: 500,
            param3: 0.001,
            flag1: false,
        };
        let mut moved = false;
        let mut v = r;
        assert_eq!(plume_step(&mut v, &t, &mut moved), 0);
        assert!(moved);
        let age = 0.25f32 + 100.0 / 1000.0;
        let d = 1.0 - age * 1000.0 / 500.0;
        assert_eq!(v.plume.age, age);
        assert_eq!(v.plume.start.x, 1.0 + (0.5 * d + 0.01 * 12.5));
        assert_eq!(v.plume.start.z, 3.0 + ((1.0 - age) * 0.001 * 100.0 + 0.125 * d + 0.0 * 12.5));
        v.plume.age = 0.95;
        assert_eq!(plume_step(&mut v, &t, &mut moved), 7);
        assert!(!moved);
        // the puff: its age by the tick's seconds; flag 1 holds it in place
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_PLUME);
        p.plume.start = Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 };
        assert_eq!(plume_step(&mut p, &t, &mut moved), 0);
        assert!(!moved);
        assert_eq!(p.plume.start.x, 1.0 + 0.1 * 0.01 * 25.0);
        assert_eq!(p.plume.start.z, 1.0 + (0.9 * 0.001 * 100.0 + 0.0 * 0.1 * 25.0));
        let mut q = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_PLUME);
        assert_eq!(plume_step(&mut q, &EeGfxPlumeTick { flag1: true, ..t }, &mut moved), 0);
        assert_eq!((q.plume.age, q.plume.start), (0.1, Ge3DPoint::default()));
    }

    #[test]
    fn the_marching_icons_lie_on_their_circle_and_square() {
        assert!(marching_offsets(EE_GFX_EFFECT_KIND_SQUARE_ICONS, 0, 0.0, 1.0, 1.0).is_empty());
        let c = marching_offsets(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS, 4, 0.0, 2.0, 9.0);
        assert_eq!(c.len(), 4);
        assert_eq!(c[0], Ge3DPoint { x: um::um_cos(0.0) * 2.0, y: um::um_cos(1.5707964) * 2.0, z: 0.0 });
        for p in &c {
            assert!(((p.x * p.x + p.y * p.y).sqrt() - 2.0).abs() < 0.01);
        }
        // the square: the right side first (from -1/8 to 1/8 turn), then the top, left and bottom
        let s = marching_offsets(EE_GFX_EFFECT_KIND_SQUARE_ICONS, 4, 0.0, 9.0, 1.0);
        assert_eq!(s[0], Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 });
        assert_eq!((s[1].y, s[2].x, s[3].y), (1.0, -1.0, -1.0));
        for p in &s {
            assert!(p.x.abs() <= 1.0 && p.y.abs() <= 1.0);
        }
        // angles past a turn wrap; negative ones by their size
        // (the C++'s subtractions, one turn at a time)
        assert_eq!(square_icon(6.2831855 * 2.0 + 0.1, 1.0), square_icon(6.2831855 * 2.0 + 0.1 - 6.2831855, 1.0));
        assert!((square_icon(6.2831855 + 0.1, 1.0).y - square_icon(0.1, 1.0).y).abs() < 1e-5);
        assert_eq!(square_icon(-0.3, 1.0), square_icon(0.3, 1.0));
        assert_eq!(square_radius(1.0), (2.0f64).sqrt() as f32);
    }

    struct Flat(f32, bool, bool);

    impl ParticleGround for Flat {
        fn is_valid_tile(&mut self, _x: u32, _y: u32) -> bool {
            self.1
        }
        fn is_space_tile(&mut self, _x: u32, _y: u32) -> bool {
            self.2
        }
        fn mesh_z(&mut self, _x: f32, _y: f32) -> f32 {
            self.0
        }
        fn mesh_normal(&mut self, _x: f32, _y: f32) -> Ge3DPoint {
            Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 }
        }
    }

    #[test]
    fn particles_move_bounce_age_and_save() {
        let tick = EeGfxParticleTick {
            now: 100.0,
            game_over: false,
            wind: Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 },
            position: Ge3DPoint { x: 5.0, y: 5.0, z: 0.05 },
            duration: 1000,
            param4: 0.0,
            param8: 10.0,
            param9: 0.5,
            param10: 0.5,
            param11: 0.0,
            flag2: true,
        };
        // falls onto the ground at 0 and bounces up at half speed
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_2D);
        r.particle.velocity = Ge3DPoint { x: 0.0, y: 0.0, z: -1.0 };
        assert_eq!(particle_update(&mut r, &tick, &mut Flat(0.0, true, false)), 0);
        let vz = -10.0f32 * 0.1 + -1.0;
        let bounce = vz * (-1.0 - 0.5) / 1.0;
        assert_eq!(r.particle.velocity.z, 1.0 * bounce + vz);
        assert_eq!(r.particle.draw_position.z, 0.0);
        assert_eq!(r.particle.draw_position.x, 0.5 * 1.0 * 0.1 + 0.0 * 0.1 + 5.0);
        assert_eq!(
            (r.particle.age, r.particle.last_time, r.particle.draw_age, r.particle.moved),
            (100.0, 100.0, 0.1, true)
        );
        // no time passed: nothing
        assert_eq!(particle_update(&mut r, &tick, &mut Flat(0.0, true, false)), 0);
        assert_eq!(r.particle.age, 100.0);
        // off the map: the 2D particle ends, the 3D one freezes
        let t2 = EeGfxParticleTick { now: 200.0, ..tick };
        let mut a = r;
        assert_eq!(particle_update(&mut a, &t2, &mut Flat(0.0, false, false)), 7);
        let mut b = r;
        b.kind = EE_GFX_EFFECT_KIND_PARTICLE_3D;
        b.particle.moved = false;
        assert_eq!(particle_update(&mut b, &t2, &mut Flat(0.0, false, false)), 0);
        assert!(!b.particle.moved);
        // the game over stops the 2D particle only
        let mut g2 = r;
        assert_eq!(
            particle_update(&mut g2, &EeGfxParticleTick { game_over: true, ..t2 }, &mut Flat(0.0, true, false)),
            0
        );
        assert_eq!(g2.particle.age, 100.0);
        // past the duration
        let mut c = r;
        assert_eq!(particle_update(&mut c, &EeGfxParticleTick { now: 2000.0, ..tick }, &mut Flat(0.0, true, false)), 7);
        // the tick applies a moved position once and ends past the duration
        let mut moved = false;
        r.particle.duration = 1000.0;
        assert_eq!(particle_step(&mut r, 100.0, &mut moved), 0);
        assert!(moved && !r.particle.moved);
        assert_eq!(particle_step(&mut r, 100.0, &mut moved), 0);
        assert!(!moved);
        assert_eq!(particle_step(&mut r, 1001.0, &mut moved), 7);
        // save and load: the 3D particle's three angles, the clock from 0.218
        let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
        s.particle = EeGfxEffectParticle {
            velocity: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            age: 4.0,
            yaw: 5.0,
            pitch: 6.0,
            roll: 7.0,
            brightness: 0.5,
            last_time: 9.0,
            ..Default::default()
        };
        let mut f = Mem::default();
        assert_eq!(save_particle(&s, &mut f), 0);
        assert_eq!(f.items.len(), 3 + 6);
        let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        let mut loaded = false;
        assert_eq!(load_particle(&mut back, &mut g, 0, 0x10c, &mut loaded), 0);
        assert!(loaded);
        assert_eq!(back.particle, s.particle);
        let mut old = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_particle(&mut old, &mut g, 0, 0xd9, &mut loaded), 0);
        assert_eq!((old.particle.last_time, g.at), (0.0, 8));
        let mut two = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_2D);
        two.particle = s.particle;
        let mut f2 = Mem::default();
        save_particle(&two, &mut f2);
        assert_eq!(f2.items.len(), 3 + 4);
        particle_clear(&mut two);
        assert_eq!(two.particle, EeGfxEffectParticle::default());
    }

    #[test]
    fn decals_lie_level_or_hide_and_save() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(777);
        let mut check = URandom { seed: 777 };
        let n = EeGfxAmbientNew { start_size: 1.0, size: 3.0, int_param1: 2, flag1: true };
        let at = Ge3DPoint { x: 10.0, y: 10.0, z: 0.5 };
        let mut r = EeGfxEffectRecord::new(0x32);
        ambient_new(&mut r, at, &n, &mut Flat(0.5, true, false));
        let size = 1.0 + check.generate() * (3.0 - 1.0);
        let rot = check.generate() * 6.2831855;
        let a = r.ambient;
        assert_eq!((a.size, a.rotation, a.hidden, a.age, a.kind, a.on_slope), (size, rot, 0.0, 0.0, 2, 0));
        assert_eq!(a.corners[2], Ge3DPoint { x: 10.0 + size * 0.5, y: 10.0 + size * 0.5, z: 0.5 });
        assert_eq!(a.transformation.m[0][3], 10.0);
        // off the map: hidden, expired, no turn drawn
        let mut h = EeGfxEffectRecord::new(0x32);
        ambient_new(&mut h, at, &n, &mut Flat(0.5, false, false));
        check.generate();
        assert_eq!((h.ambient.hidden, h.ambient.age, h.ambient.transformation), (1.0, 1.0, identity()));
        assert_eq!(random_seed(), check.seed);
        // ages by the tick
        assert_eq!(ambient_step(&mut r, 0.5, 1000), 0);
        assert_eq!(ambient_step(&mut r, 0.6, 1000), 7);
        // save and load
        let mut f = Mem::default();
        assert_eq!(save_ambient(&r, &mut f), 0);
        assert_eq!(f.items.len(), 16 + 1 + 3 + 1 + 1 + 1 + 1 + 12);
        let mut back = EeGfxEffectRecord::new(0x32);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_ambient(&mut back, &mut g), 0);
        let mut want = r.ambient;
        want.hidden = 0.0;
        assert_eq!(back.ambient, want);
    }

    #[derive(Default)]
    struct Children {
        next: u32,
        log: Vec<(char, u32)>,
    }

    impl DamagedHost for Children {
        fn create(&mut self, secondary: bool, index: u32) -> u32 {
            self.next += 1;
            self.log.push((if secondary { 's' } else { 'p' }, index));
            self.next
        }
        fn destroy(&mut self, handle: u32) {
            self.log.push(('d', handle));
        }
    }

    #[test]
    fn the_damage_controller_keeps_a_child_per_lost_share_and_saves_them() {
        let mut lists = EeGfxEffectLists::default();
        let mut finished = false;
        let mut h = Children::default();
        let t = EeGfxDamagedTick {
            has_unit: true,
            tag_count: 4,
            hit_points: 25,
            max_hit_points: 100,
            param2: 0.5,
            has_linked2: true,
            hidden: false,
        };
        // a quarter of the health with damage below half: ceil((1 - 0.25 / 0.5) * 4) = 2 children, each with its secondary
        assert_eq!(damaged_step(&mut finished, &mut lists, &t, &mut h), 0);
        assert_eq!(lists, EeGfxEffectLists { primary: vec![1, 3], secondary: vec![2, 4] });
        assert_eq!(h.log, vec![('p', 0), ('s', 0), ('p', 1), ('s', 1)]);
        // hidden: all go, last first
        h.log.clear();
        assert_eq!(damaged_step(&mut finished, &mut lists, &EeGfxDamagedTick { hidden: true, ..t }, &mut h), 0);
        assert_eq!(h.log, vec![('d', 3), ('d', 4), ('d', 1), ('d', 2)]);
        // healed past the share: finished (param 2 below 1), then everything goes and it ends
        lists = EeGfxEffectLists { primary: vec![7], secondary: vec![8] };
        h.log.clear();
        assert_eq!(damaged_step(&mut finished, &mut lists, &EeGfxDamagedTick { hit_points: 80, ..t }, &mut h), 0);
        assert!(finished);
        assert_eq!(damaged_step(&mut finished, &mut lists, &t, &mut h), 7);
        assert_eq!(h.log, vec![('d', 7), ('d', 8)]);
        // without a unit nothing happens
        let mut f2 = false;
        assert_eq!(damaged_step(&mut f2, &mut lists, &EeGfxDamagedTick { has_unit: false, ..t }, &mut h), 0);
        // save and load
        let full = EeGfxEffectLists { primary: vec![5, 6], secondary: vec![9, 10] };
        let mut f = Mem::default();
        assert_eq!(save_damaged(true, &full, &mut f, true), 0);
        let types: String = f.items.iter().map(|i| i.0).collect();
        assert_eq!(types, "buuuuub");
        let mut back = EeGfxEffectLists::default();
        let mut fin = false;
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_damaged(&mut fin, &mut back, &mut g, 0, 0x10c), 0);
        assert_eq!((back, fin), (full.clone(), true));
        // before 0.249 the flag was not saved
        let mut old = EeGfxEffectLists::default();
        let mut fin2 = false;
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_damaged(&mut fin2, &mut old, &mut g, 0, 0xf8), 0);
        assert!(!fin2);
    }

    struct Lines(Vec<(Ge3DPoint, Ge3DPoint)>);

    impl EmitterHost for Lines {
        fn line(&mut self, s: Ge3DPoint, d: Ge3DPoint) -> i32 {
            self.0.push((s, d));
            self.0.len() as i32
        }
    }

    #[test]
    fn the_emitter_moves_bursts_and_saves() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        let mut r = EeGfxEffectRecord::new(0x3a);
        r.transformation.m[0][3] = 3.0;
        r.emitter.velocity = Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 };
        assert_eq!(emitter_begin(&mut r, 0.125), 125.0);
        assert_eq!(r.emitter.time, 125.0);
        let m = EeGfxEmitterMove {
            wind: Ge3DPoint::default(),
            param4: 0.0,
            param8: 0.0,
            param9: 0.0,
            param10: 0.0,
            param11: 1.0,
            flag2: true,
        };
        let mut out = Ge3DPoint::default();
        assert_eq!(emitter_move(&mut r, 0.125, &m, &mut Flat(-5.0, true, false), &mut out), 0);
        let vx = -0.125f32 + 1.0;
        assert_eq!((r.emitter.velocity.x, out.x), (vx, vx * 0.125 + 3.0));
        assert_eq!(emitter_move(&mut r, 0.125, &m, &mut Flat(-5.0, false, false), &mut out), 7);
        // the burst: three draws of place (two when flat), pitch, yaw and speed per particle, then the countdown
        set_random_seed(4242);
        let mut check = URandom { seed: 4242 };
        let b = EeGfxEmitterBurst {
            count: 2,
            position_spread: 1.0,
            flat: true,
            param1: 0.5,
            param2: 0.1,
            param3: 0.0,
            param4: 1.0,
            param5: 2.0,
            param6: 0.5,
            int_param1: 300,
            int_param2: 100,
        };
        let mut lines = Lines(Vec::new());
        let t = identity();
        assert_eq!(emitter_burst(&mut r, 10.0, Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 }, &t, &b, 0, &mut lines), 2);
        for (s, d) in &lines.0 {
            let x = (0.5 - check.generate()) * 1.0 + 1.0;
            let y = (0.5 - check.generate()) * 1.0 + 1.0;
            let pitch = (0.5 - check.generate()) * 0.1 + 0.5;
            let yaw = (0.5 - check.generate()) * 1.0 + 0.0;
            let speed = (0.5 - check.generate()) * 0.5 + 2.0;
            assert_eq!(*s, Ge3DPoint { x, y, z: 1.0 });
            let mut turn = identity();
            turn.revolve(yaw, pitch, 0.0);
            let v = turn.apply_vector(ee_ge::types::GeVector { x: 0.0, y: 0.0, z: 1.0 });
            assert_eq!(*d, Ge3DPoint { x: speed * v.x, y: speed * v.y, z: speed * v.z });
        }
        assert_eq!(r.emitter.emit_time, (0.5 - check.generate()) * 100.0 + 300.0);
        // no burst while the countdown runs
        assert_eq!(emitter_burst(&mut r, 10.0, Ge3DPoint::default(), &t, &b, 5, &mut lines), 5);
        assert_eq!(random_seed(), check.seed);
        // save and load
        let mut f = Mem::default();
        assert_eq!(save_emitter(&r, &mut f), 0);
        let mut back = EeGfxEffectRecord::new(0x3a);
        let mut g = Mem { items: f.items.clone(), ..Default::default() };
        assert_eq!(load_emitter(&mut back, &mut g), 0);
        assert_eq!(back.emitter, r.emitter);
    }

    #[test]
    fn the_aged_effects_end_by_their_age() {
        let mut tile = [0, 0];
        let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_SPLASH);
        assert_eq!(aged_step(&mut s, 0.5, 1000, &mut tile), 0);
        assert_eq!(aged_step(&mut s, 0.5, 1000, &mut tile), 0);
        assert_eq!(aged_step(&mut s, 0.001, 1000, &mut tile), 7);
        let mut f = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SCREEN_FLASH);
        aged_step(&mut f, 0.5, 1000, &mut tile);
        assert_eq!(aged_step(&mut f, 0.5, 1000, &mut tile), 7);
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_RIPPLE);
        w.aged.transformation.m[0][3] = 3.5;
        w.aged.transformation.m[1][3] = -0.5;
        w.aged.transformation.m[2][3] = 2.0;
        assert_eq!(aged_step(&mut w, 0.5, 1000, &mut tile), EE_GFX_EFFECT_ASK_BLOCKED);
        assert_eq!((tile, w.aged.transformation.m[2][3]), ([3, -1], 0.0));
        assert_eq!(aged_step(&mut w, 0.6, 1000, &mut tile), 7);
        let mut m = Mem::default();
        assert_eq!(save_aged(&w, &mut m), 0);
        assert_eq!(m.items.len(), 17);
        let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_RIPPLE);
        let mut g = Mem { items: m.items.clone(), ..Default::default() };
        assert_eq!(load_aged(&mut back, &mut g), 0);
        assert_eq!(back.aged, w.aged);
        let mut m2 = Mem::default();
        save_aged(&s, &mut m2);
        assert_eq!(m2.items.len(), 1);
    }

    #[test]
    fn regions_floor_then_divide() {
        assert_eq!(EeGfxEffectRecord::region_of(17.9, 0.0, 8), (2, 0));
        assert_eq!(EeGfxEffectRecord::region_of(-0.5, 15.99, 8), (0, 1));
        assert_eq!(EeGfxEffectRecord::region_of(-9.5, 64.0, 8), (-1, 8));
    }

    /// The generator's draws, checked against an independent URandom.
    struct Host {
        placed: Vec<Ge3DPoint>,
        lines: Vec<(Ge3DPoint, Ge3DPoint)>,
        tags: Vec<Ge3DPoint>,
    }

    impl SmokeHost for Host {
        fn tag_point(&mut self, i: u32) -> Ge3DPoint {
            self.tags[i as usize]
        }
        fn place(&mut self, p: Ge3DPoint) -> i32 {
            self.placed.push(p);
            // a created effect draws too (its constructor's spread): the next puff's draws follow it
            random();
            0
        }
        fn line(&mut self, s: Ge3DPoint, d: Ge3DPoint) {
            self.lines.push((s, d));
        }
    }

    fn host() -> Host {
        Host {
            placed: Vec::new(),
            lines: Vec::new(),
            tags: vec![Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, Ge3DPoint { x: -1.0, y: 0.0, z: 8.0 }],
        }
    }

    // the generator is global: one test walks every smoke kind in turn
    #[test]
    fn the_smoke_generators_step_draw_and_place_in_the_original_s_order() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(12345);
        let mut check = URandom { seed: 12345 };
        // a point generator: puffs once its interval passes, ends after its duration
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_GENERATOR);
        r.transformation.m[0][3] = 10.0;
        r.transformation.m[1][3] = 20.0;
        r.transformation.m[2][3] = 1.0;
        let t = EeGfxSmokeTick { tick_seconds: 0.1, duration: 250, time1: 150, param1: 2.0, ..Default::default() };
        let mut h = host();
        assert_eq!(smoke_step(&mut r, &t, &mut h), 0);
        assert_eq!((r.smoke.time, r.smoke.since_emit, h.placed.len()), (100.0, 100.0, 0));
        assert_eq!(smoke_step(&mut r, &t, &mut h), 0);
        let x = 2.0 * (0.5 - check.generate()) + 10.0;
        let y = (0.5 - check.generate()) * 2.0 + 20.0;
        let z = (0.5 - check.generate()) * 2.0 + 1.0;
        check.generate();
        assert_eq!(h.placed, vec![Ge3DPoint { x, y, z }]);
        assert_eq!(r.smoke.since_emit, 50.0);
        // past its duration it still puffs in its last tick
        assert_eq!(smoke_step(&mut r, &t, &mut h), 7);
        for _ in 0..4 {
            check.generate();
        }
        assert_eq!((h.placed.len(), r.smoke.since_emit), (2, 0.0));
        // on a unit: the unit's position, its death ends it
        let mut u = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_GENERATOR);
        let tu = EeGfxSmokeTick {
            has_unit: true,
            unit_position: Ge3DPoint { x: 5.0, y: 5.0, z: 0.0 },
            unit_command: EE_GFX_EFFECT_ACTION_DEAD,
            ..t
        };
        assert_eq!(smoke_step(&mut u, &tu, &mut h), 7);
        // the destruction smoke draws and counts down but places nothing
        let mut d = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE);
        d.smoke.since_emit = 200.0;
        let before = h.placed.len();
        assert_eq!(smoke_step(&mut d, &t, &mut h), 0);
        for _ in 0..3 {
            check.generate();
        }
        assert_eq!((h.placed.len(), d.smoke.since_emit), (before, 150.0));
        // the damage smoke: below param 2's share, a puff per tag point at its interval
        let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_SMOKE);
        let ts = EeGfxSmokeTick {
            tick_time: 100.0,
            has_unit: true,
            hit_points: 25,
            max_hit_points: 100,
            time1: 100,
            time2: 500,
            param1: 1.0,
            param2: 0.5,
            has_tag_points: true,
            tag_point_count: 2,
            ..Default::default()
        };
        // interval: lrintf(400 * 0.5 + 100) = 300
        h.placed.clear();
        assert_eq!(smoke_step(&mut s, &ts, &mut h), 0);
        assert_eq!(smoke_step(&mut s, &ts, &mut h), 0);
        assert!(h.placed.is_empty());
        assert_eq!(smoke_step(&mut s, &ts, &mut h), 0);
        let mut want = Vec::new();
        for p in &h.tags {
            let x = (0.5 - check.generate()) * 1.0 + p.x;
            let y = (0.5 - check.generate()) * 1.0 + p.y;
            let z = (0.5 - check.generate()) * 1.0 + p.z;
            check.generate();
            want.push(Ge3DPoint { x, y, z });
        }
        assert_eq!(h.placed, want);
        assert_eq!(s.smoke.since_emit, 0.0);
        // healed past param 2's share, or dead: it ends
        assert_eq!(smoke_step(&mut s, &EeGfxSmokeTick { hit_points: 60, ..ts }, &mut h), 7);
        assert_eq!(smoke_step(&mut s, &EeGfxSmokeTick { unit_command: EE_GFX_EFFECT_ACTION_DEAD, ..ts }, &mut h), 7);
        // time2 below time1: the unsigned difference
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_SMOKE);
        let tw = EeGfxSmokeTick { time1: 500, time2: 100, has_tag_points: false, ..ts };
        assert_eq!(smoke_step(&mut w, &tw, &mut h), 0);
        assert_eq!(w.smoke.since_emit, 100.0);
        assert_eq!(random_seed(), check.seed);

        // the destruction smoke's set-up: lines by the radius, three draws each
        let mut e = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE);
        e.smoke.radius = 2.0;
        e.transformation.m[0][3] = 4.0;
        e.transformation.m[2][3] = 1.5;
        let su = EeGfxSmokeSetUp { param1: 1.0, param2: 3.0, int_param1: 2 };
        assert_eq!(smoke_set_up(&e, &su, &mut h), 0);
        assert_eq!(h.lines.len(), 4);
        let mut turn = identity();
        for (s, d) in &h.lines {
            let a = check.generate();
            let b = check.generate();
            let dist = (1.0 - a * b) * 2.0;
            turn.set_orientation(ee_ge::transform::RotationOrder::Ypr, check.generate() * 6.2831855, 0.0, 0.0);
            let rot = turn.apply_point(Ge3DPoint { x: dist, y: 0.0, z: 0.0 });
            assert_eq!(*s, Ge3DPoint { x: rot.x + 4.0, y: rot.y, z: 1.5 });
            assert_eq!(*d, Ge3DPoint { x: rot.x * 3.0, y: rot.y * 3.0, z: 0.0 });
        }
        assert_eq!(smoke_set_up(&EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_SMOKE), &su, &mut h), 0);
        assert_eq!(h.lines.len(), 4);
        // GetEffectRandomLong clamps to the maximum
        let v = random_long(2, 5);
        let want = um::lrintf(check.generate() * 3.0 + 2.0).min(5);
        assert_eq!(v, want);
        assert_eq!(random_float(1.0, 3.0), check.generate_range(1.0, 3.0));

        // a new puff: the rotation, then the brightness, clamped
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_PLUME);
        plume_new(&mut p, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, 0.5, 0.25, Ge3DPoint::default());
        let rot = check.generate() * 6.2831855;
        let b = check.generate() * (0.75 - 0.25) + 0.25;
        assert_eq!((p.plume.rotation, p.plume.brightness, p.plume.age), (rot, b, 0.0));
        plume_new(&mut p, Ge3DPoint::default(), 3.0, 0.5, Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 });
        check.generate();
        check.generate();
        assert_eq!((p.plume.brightness, p.plume.velocity.x), (1.0, 1.0));
        plume_new(&mut p, Ge3DPoint::default(), -3.0, 0.5, Ge3DPoint::default());
        check.generate();
        check.generate();
        assert_eq!(p.plume.brightness, 0.0);
        assert_eq!(random_seed(), check.seed);

        // the particles' set-ups: the 2D rotation then brightness; the 3D yaw, pitch, roll then brightness
        let mut q = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_2D);
        particle_set_up(&mut q, &EeGfxParticleSetUp { int_param1: 0, param2: 0.5, param3: 0.25 });
        let rot = check.generate() * 6.2831855;
        let b = check.generate() * (0.75 - 0.25) + 0.25;
        assert_eq!((q.particle.yaw, q.particle.brightness), (rot, b));
        particle_set_up(&mut q, &EeGfxParticleSetUp { int_param1: 1, param2: 0.5, param3: 0.25 });
        check.generate();
        assert_eq!(q.particle.yaw, 0.0);
        let mut d = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
        particle_set_up(&mut d, &EeGfxParticleSetUp { int_param1: 0, param2: 0.5, param3: 0.25 });
        let y = check.generate() * 6.2831855;
        let pi = check.generate() * 6.2831855;
        let ro = check.generate() * 6.2831855;
        let diff = 0.5f32 - 0.25;
        let b3 = check.generate() * (0.25 + 0.5 - diff) + diff;
        assert_eq!(
            (d.particle.yaw, d.particle.pitch, d.particle.roll, d.particle.brightness),
            (y, pi, ro, b3.min(1.0))
        );
        assert_eq!(random_seed(), check.seed);
    }
}
