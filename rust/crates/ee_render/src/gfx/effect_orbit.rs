//! The 3D effects with orbiting parts whole in Rust: the linear projectile (kind 0x25, `EEE3DLinearProjectile`,
//! deleted): a model flying straight from one point to another in about the effect's duration (+-12.5% at random),
//! drawn in pass 5 alpha blended with its orbiting parts turned and fading over the last fifth of its path; the
//! disease (0x23, `EEE3DDisease`) and the virus (0x42, `EEE3DVirus`), both deleted: the graphic's model on a unit
//! playing its animation 13 over the effect's duration with the animation's sound (the host's), the disease at the
//! unit's location, the virus at its first tag point with its texture scrolled in u, drawn in pass 5 alpha blended
//! with z writes, unit lit, their orbitals turned, fading over the last tenth of their life.
//!
//! The hurricane (`effect_hurricane.rs`) shares the orbitals and the animation.
//!
//! The orbitals (`EEOrbital`) are copies of the graphic object's (the host's `orbital`): each a part model of the
//! graphic's model, turning at its speed (radians a second, `EEOrbital::Update`), set as that model's pre-model
//! transformation (`SetOrientationYPR(angle, 0, 0)`) before the draw. They are owned by the record
//! (`EeGfxEffectOrbit::orbitals`, made on first use, freed with it). `EEE3DRust` holds the graphic object and answers
//! the world (`EeGfxEffect3DHost`); the draws go through the rasterizer table (`raster.rs`).
//!
//! The record is reached through its raw pointer field by field: the C++ reads it during the host calls (the region
//! move), so no Rust reference to it lives across one.

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeVector};

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_shot::{load_line, save_line};
use crate::gfx::effect_state::{identity, random, EeGfxEffectRecord, EffectFile};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{
    EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE, ZBUFFER_TEST_ONLY,
};
use ee_base::math::um::lrintf;

/// `kEffect3DLinearProjectile`.
pub const EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE: i32 = 0x25;
/// `kEffect3DDisease`, `kEffect3DVirus`.
pub const EE_GFX_EFFECT_KIND_DISEASE: i32 = 0x23;
pub const EE_GFX_EFFECT_KIND_VIRUS: i32 = 0x42;

/// The graphic object's animation 13 (`mStartFrames[13]`, `mFrameTimes[13]` in ms, `mFrameCounts[13]`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxAnimation {
    pub start: u32,
    pub frame_time: u32,
    pub frame_count: u32,
}

/// `kErrorOK`.
const OK: i32 = 0;
/// pi and two pi as `EEOrbital::Update` wraps the angle.
const PI: f32 = 3.1415927;
const TWO_PI: f32 = 6.2831855;

/// One orbiting part (`EEOrbital`): its model, its speed (radians a second) and its angle.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxOrbital {
    pub model: EeGfxModel,
    pub speed: f32,
    pub angle: f32,
}

impl EeGfxOrbital {
    /// `EEOrbital::Update`: the angle advanced by the speed over the tick, wrapped to -pi..pi.
    pub fn update(&mut self, tick_seconds: f32) {
        let mut angle = tick_seconds * self.speed + self.angle;
        while angle < -PI {
            angle += TWO_PI;
        }
        while angle > PI {
            angle -= TWO_PI;
        }
        self.angle = angle;
    }
}

/// The effect's orbitals (`mOrbitals`, copies of the graphic's) and the angles the draw uses (`mAngles`).
#[derive(Debug, Default)]
pub struct EeGfxOrbitals {
    pub list: Vec<EeGfxOrbital>,
    pub angles: Vec<f32>,
}

/// The orbiting effects' state beyond the shot's and the aged record's.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectOrbit {
    /// owned: made on first use, freed with the record
    pub orbitals: *mut EeGfxOrbitals,
    /// `mOrbitalCount`: the graphic's orbital count when they were copied
    pub count: i32,
    /// the disease's and virus's `mTime` (ms into the frame) and `mFrame` (both saved), the draw's frame
    pub time: f32,
    pub frame: u32,
    pub draw_frame: u32,
    /// the virus's `mScroll` (the texture's u offset, saved) and the draw's copy
    pub scroll: f32,
    pub draw_scroll: f32,
    /// the hurricane's (`effect_hurricane.rs`) `mSpawnTime` (ms until its next linked effect) and `mScale` (0.1..1),
    /// both saved, and the draw's scale
    pub spawn_time: f32,
    pub scale: f32,
    pub draw_scale: f32,
}

impl Default for EeGfxEffectOrbit {
    fn default() -> EeGfxEffectOrbit {
        EeGfxEffectOrbit {
            orbitals: std::ptr::null_mut(),
            count: 0,
            time: 0.0,
            frame: 0,
            draw_frame: 0,
            scroll: 0.0,
            draw_scroll: 0.0,
            spawn_time: 0.0,
            scale: 0.0,
            draw_scale: 0.0,
        }
    }
}

/// The record's orbitals (made on first use).
///
/// # Safety
/// `r` is a live record; the result is used only until the next call that may free or replace them.
unsafe fn orbitals<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxOrbitals {
    // SAFETY: the function's contract
    unsafe {
        if (*r).orbit.orbitals.is_null() {
            (*r).orbit.orbitals = Box::into_raw(Box::default());
        }
        &mut *(*r).orbit.orbitals
    }
}

/// Copies the graphic's orbitals (`GetOrbitalCount`, `GetOrbital`), at random starting angles when `turn`
/// (`GetRandom() * 2 pi`, one draw each in order).
///
/// # Safety
/// As `orbit`.
pub(crate) unsafe fn copy_orbitals<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H, turn: bool) {
    let count = h.orbital_count();
    // SAFETY (every access below): the function's contract; the host's orbital calls do not reach the record
    unsafe {
        (*r).orbit.count = count as i32;
        let o = orbitals(r);
        o.angles.resize(count as usize, 0.0);
        for i in 0..count {
            let orbital = h.orbital(i);
            o.list.push(orbital);
            if turn {
                o.list[i as usize].angle = random() * TWO_PI;
            }
        }
    }
}

/// `Update` of each orbital.
///
/// # Safety
/// As `orbit`.
pub(crate) unsafe fn update_orbitals(r: *mut EeGfxEffectRecord, tick_seconds: f32) {
    // SAFETY: the function's contract
    unsafe {
        let count = (*r).orbit.count;
        let o = orbitals(r);
        for i in 0..count.max(0) as usize {
            o.list[i].update(tick_seconds);
        }
    }
}

/// `Queue`'s copy of the orbitals' angles.
///
/// # Safety
/// As `orbit`.
pub(crate) unsafe fn keep_angles(r: *mut EeGfxEffectRecord) {
    // SAFETY: the function's contract
    unsafe {
        let count = (*r).orbit.count;
        let o = orbitals(r);
        for i in 0..count.max(0) as usize {
            o.angles[i] = o.list[i].angle;
        }
    }
}

/// Each orbital's model turned to its kept angle (its pre-model transformation).
///
/// # Safety
/// As `orbit`.
pub(crate) unsafe fn turn_orbitals<R: Raster>(r: *mut EeGfxEffectRecord, x: &mut R) {
    // SAFETY: the function's contract; the rasterizer does not reach the record
    unsafe {
        let count = (*r).orbit.count;
        let o = orbitals(r);
        let mut t = identity();
        for i in 0..count.max(0) as usize {
            t.set_orientation(RotationOrder::Ypr, o.angles[i], 0.0, 0.0);
            x.model_pre_transform(o.list[i].model, &t);
        }
    }
}

/// The draw of the model at `opacity` (fading below 1); the draw's result.
fn draw_faded<R: Raster>(x: &mut R, model: EeGfxModel, opacity: Option<f32>) -> i32 {
    match opacity {
        Some(o) => {
            x.model_opacity(model, o);
            let result = x.model_draw(model, 1.0);
            x.model_opacity(model, 1.0);
            result
        }
        None => x.model_draw(model, 1.0),
    }
}

/// The orbiting effects' virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn orbit<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    if unsafe { (*r).kind } != EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE {
        return unsafe { sick(op, r, g, h, x) };
    }
    match op {
        // from `shot.path.start` to `shot.location` (the C++ constructor's points)
        EeGfxEffect3DOp::Create => {
            let (from, to) = unsafe { ((*r).shot.path.start, (*r).shot.location) };
            let mut t = identity();
            unsafe {
                (*r).shot.path.direction = GeVector { x: to.x - from.x, y: to.y - from.y, z: to.z - from.z };
                (*r).shot.location = Ge3DPoint::default();
                t.m[0][3] = from.x;
                t.m[1][3] = from.y;
                t.m[2][3] = from.z;
                (*r).transformation = t;
            }
            h.move_to();
            let duration = g.duration as f32;
            unsafe {
                (*r).bounding_radius = g.size;
                (*r).shot.speed = 1000.0 / (((random() - 0.5) * duration) * 0.25 + duration) / 1000.0;
            }
            0
        }
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            unsafe { copy_orbitals(r, h, true) };
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    (*r).shot.draw_position = (*r).shot.position;
                    let t = (*r).transformation;
                    (*r).shot.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                    keep_angles(r);
                }
                h.queue(3);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass5 => unsafe { draw_projectile(r, g, h, x) },
        EeGfxEffect3DOp::Step => {
            unsafe { update_orbitals(r, g.tick_seconds) };
            let position = unsafe {
                (*r).shot.position += g.tick_time * (*r).shot.speed;
                (*r).shot.position
            };
            if position <= 1.0 {
                let point = unsafe { (*r).shot.path.point_at(position) };
                let mut t = identity();
                t.m[0][3] = point.x;
                t.m[1][3] = point.y;
                t.m[2][3] = point.z;
                unsafe { (*r).transformation = t };
                h.move_to();
                0
            } else {
                unsafe { (*r).shot.position = 1.0 };
                7
            }
        }
        _ => 0,
    }
}

/// The projectile's pass 5.
///
/// # Safety
/// As `orbit`.
unsafe fn draw_projectile<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let model = h.model();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.lighting(EeGfxLighting::Prelit);
    // SAFETY (every access below): the function's contract
    let (location, draw_position) = unsafe { ((*r).shot.location, (*r).shot.draw_position) };
    let mut t = identity();
    t.m[0][3] = location.x;
    t.m[1][3] = location.y;
    t.m[2][3] = location.z;
    x.model_transform(model, &t);
    x.set_shading_source(0);
    x.model_scale(model, g.size);
    unsafe { turn_orbitals(r, x) };
    let texture = h.texture();
    x.model_own_texture(model, texture);
    x.model_prepare(model);
    draw_faded(x, model, if draw_position >= 0.8 { Some((1.0 - draw_position) * 5.0) } else { None })
}

/// `Load` after the base's (the projectile's): the media and the orbitals (at random angles; both results ignored),
/// then the position, speed and path; the first read error. The disease's and virus's: `load_sick`.
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn load_orbit<H: Effect3DHost, F: EffectFile>(
    r: *mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    // SAFETY (every access below): the function's contract
    if unsafe { (*r).kind } != EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE {
        return unsafe { load_sick(r, f, h) };
    }
    h.load_media();
    unsafe {
        copy_orbitals(r, h, true);
        let s = &mut (*r).shot;
        let mut res = f.read_f32(&mut s.position);
        if res == OK {
            res = f.read_f32(&mut s.speed);
        }
        if res == OK {
            res = load_line(f, &mut s.path);
        }
        res
    }
}

/// `Save` after the base's (ignored): the position, speed and path; the first write error.
pub(crate) fn save_orbit<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    if r.kind != EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE {
        return save_sick(r, f);
    }
    let s = &r.shot;
    let mut res = f.write_f32(s.position);
    if res == OK {
        res = f.write_f32(s.speed);
    }
    if res == OK {
        res = save_line(f, &s.path);
    }
    res
}

/// The disease's and the virus's virtuals: a model on a unit playing the graphic's animation 13 over the effect's
/// duration, its orbitals turning, with the animation's sound at the unit (the host's); the disease stands on its
/// unit's location, the virus on its first tag point (`FollowUnit`) with its texture scrolled in u.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn sick<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    let virus = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_VIRUS;
    match op {
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            let animation = h.animation();
            unsafe {
                (*r).orbit.frame = animation.start;
                (*r).orbit.time = 0.0;
                (*r).aged.age = 0.0;
                copy_orbitals(r, h, false);
            }
            if virus {
                h.follow_unit();
            } else {
                let location = h.unit_location();
                unsafe { (*r).transformation = location };
                h.move_to();
            }
            unsafe { (*r).bounding_radius = g.size };
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    (*r).orbit.draw_frame = (*r).orbit.frame;
                    (*r).aged.draw_age = (*r).aged.age;
                    if virus {
                        (*r).orbit.draw_scroll = (*r).orbit.scroll;
                    }
                    let t = (*r).transformation;
                    (*r).aged.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                    keep_angles(r);
                }
                h.queue(3);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass5 => unsafe { draw_sick(r, g, h, x, virus) },
        EeGfxEffect3DOp::Step => unsafe { step_sick(r, g, h, virus) },
        _ => 0,
    }
}

/// The disease's and the virus's step: 7 once the unit is dead or the duration is over.
///
/// # Safety
/// As `sick`.
unsafe fn step_sick<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H, virus: bool) -> i32 {
    let animation = h.animation();
    // SAFETY (every access below): the function's contract
    let (old, mut time) = unsafe { ((*r).orbit.frame, (*r).orbit.time) };
    let mut frame = old;
    let start = frame == animation.start && time == 0.0;
    if g.unit_hit_points <= 0 {
        return 7;
    }
    let age = unsafe {
        (*r).aged.age += g.tick_time / g.duration as f32;
        (*r).aged.age
    };
    if age > 1.0 {
        return 7;
    }
    if virus {
        unsafe {
            let o = &mut (*r).orbit;
            o.scroll += g.param1 * g.tick_time;
            if o.scroll > 1.0 {
                o.scroll -= 1.0;
            }
            if o.scroll < 0.0 {
                o.scroll += 1.0;
            }
        }
        h.follow_unit();
    } else {
        let location = h.unit_location();
        unsafe { (*r).transformation = location };
        h.move_to();
    }
    time = g.tick_time + time;
    let frame_time = animation.frame_time as f32;
    if time >= frame_time {
        frame = lrintf(time / frame_time) as u32;
        time -= frame.wrapping_mul(animation.frame_time) as f32;
        // a graphic without frames in animation 13: frame 0 (the C++ divided by zero)
        frame = frame.wrapping_add(old).checked_rem(animation.frame_count).unwrap_or(0);
    }
    unsafe {
        (*r).orbit.time = time;
        (*r).orbit.frame = frame;
        update_orbitals(r, g.tick_seconds);
    }
    h.animation_sound(frame, start || frame != old);
    0
}

/// The disease's and the virus's pass 5.
///
/// # Safety
/// As `sick`.
unsafe fn draw_sick<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
    virus: bool,
) -> i32 {
    let model = h.model();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    if !virus {
        x.set_shading_source(0);
    }
    x.lighting(EeGfxLighting::Unit);
    // SAFETY (every access below): the function's contract
    let (location, draw_frame, draw_age, draw_scroll) =
        unsafe { ((*r).aged.location, (*r).orbit.draw_frame, (*r).aged.draw_age, (*r).orbit.draw_scroll) };
    let mut t = identity();
    t.m[0][3] = location.x;
    t.m[1][3] = location.y;
    t.m[2][3] = location.z;
    x.model_transform(model, &t);
    x.model_frame(model, draw_frame);
    x.model_scale(model, g.size);
    if virus {
        x.model_scroll_u(model, draw_scroll);
    }
    unsafe { turn_orbitals(r, x) };
    x.model_prepare(model);
    let texture = h.texture();
    x.model_own_texture(model, texture);
    let result = draw_faded(x, model, if draw_age >= 0.9 { Some((1.0 - draw_age) * 10.0) } else { None });
    if virus {
        x.model_restore_u(model);
    }
    result
}

/// The disease's and the virus's `Load` after the base's: the media and the orbitals (both results ignored), then
/// the time, frame, age and (the virus's) scroll; the first read error.
///
/// # Safety
/// `r` is a live record.
unsafe fn load_sick<H: Effect3DHost, F: EffectFile>(r: *mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    h.load_media();
    // SAFETY (every access below): the function's contract
    unsafe {
        copy_orbitals(r, h, false);
        let mut res = f.read_f32(&mut (*r).orbit.time);
        if res == OK {
            res = f.read_u32(&mut (*r).orbit.frame);
        }
        if res == OK {
            res = f.read_f32(&mut (*r).aged.age);
        }
        if res == OK && (*r).kind == EE_GFX_EFFECT_KIND_VIRUS {
            res = f.read_f32(&mut (*r).orbit.scroll);
        }
        res
    }
}

/// The disease's and the virus's `Save` after the base's (ignored).
fn save_sick<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let mut res = f.write_f32(r.orbit.time);
    if res == OK {
        res = f.write_u32(r.orbit.frame);
    }
    if res == OK {
        res = f.write_f32(r.aged.age);
    }
    if res == OK && r.kind == EE_GFX_EFFECT_KIND_VIRUS {
        res = f.write_f32(r.orbit.scroll);
    }
    res
}

#[cfg(test)]
#[path = "effect_orbit_tests.rs"]
mod tests;
