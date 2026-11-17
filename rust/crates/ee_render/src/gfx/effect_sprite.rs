//! The 2D sprites whole in Rust on `EEE2DRust` (R4.4, twentieth round): the impact (kind 6, `EEE2DImpact`), the blood
//! impact (7, `EEE2DImpactBlood`), the muzzle flash (9, `EEE2DMuzzleFlash`), the lightning (8, `EEE2DLightning`), the
//! plasma pulse (0xa, `EEE2DPlasmaPulse`), the pulse wave (0x2f, `EEE2DPulseWave`), the water ripple (0x22,
//! `EEE2DWaterRipple`) and the fire trail (0xe, `EEE2DFireTrail`), their classes deleted. Their own state is the
//! record's `sprite` (the water ripple's the aged record's, as before); the C++ host keeps the graphic object, the
//! animation sound and the unit's tag points, weapon point and model point; the draws go through the rasterizer table.

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation};
use crate::ge::um;

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::{
    aged_step, load_aged, load_point, random, random_float, save_aged, save_point, EeGfxEffectRecord, EffectFile,
    EE_GFX_EFFECT_ASK_BLOCKED, OK,
};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, EE_GFX_SPRITE_GLOW, EE_GFX_SPRITE_GROUND,
    ZBUFFER_TEST_ONLY,
};

/// `kEffect2DImpact` .. the kinds of this file.
pub const EE_GFX_EFFECT_KIND_IMPACT: i32 = 6;
pub const EE_GFX_EFFECT_KIND_IMPACT_BLOOD: i32 = 7;
pub const EE_GFX_EFFECT_KIND_LIGHTNING: i32 = 8;
pub const EE_GFX_EFFECT_KIND_MUZZLE_FLASH: i32 = 9;
pub const EE_GFX_EFFECT_KIND_PLASMA_PULSE: i32 = 0xa;
pub const EE_GFX_EFFECT_KIND_FIRE_TRAIL: i32 = 0xe;
pub const EE_GFX_EFFECT_KIND_PULSE_WAVE: i32 = 0x2f;

/// The fire trail's rise per ms (0x43fc38).
const FIRE_TRAIL_RISE: f32 = 0.000_200_000_01;

/// The 2D sprites' own state (the members of the deleted classes; which kinds use which field in brackets).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectSprite {
    /// `mStart`: the point made at or the tag point (impacts; saved)
    pub start: Ge3DPoint,
    /// `mDrawPosition`: the queue's copy of the start (impacts) or of the position (plasma pulse, fire trail)
    pub draw_position: Ge3DPoint,
    /// `mLifeFraction` (impacts; saved)
    pub life: f32,
    /// `mTime`: ms into the frame (impacts, lightning, plasma pulse; saved)
    pub time: f32,
    /// `mAge` 0..1 (pulse wave, fire trail; saved) and the queue's copy
    pub age: f32,
    pub draw_age: f32,
    /// `mRadius` (pulse wave; saved) and the queue's copy
    pub radius: f32,
    pub draw_radius: f32,
    /// `mRotation` (fire trail; random; saved)
    pub rotation: f32,
    /// `mFrame` (saved) and the queue's copy
    pub frame: u32,
    pub draw_frame: u32,
    /// `mTagIndex`: the impact's tag point (saved from 0.245)
    pub tag_index: u32,
    /// `mHitPoints`: the pulse wave's unit's hit points at the last tick (saved)
    pub hit_points: i32,
    /// `mShrinking` (pulse wave; saved)
    pub shrinking: bool,
    /// `mVelocity`: the muzzle blast's direction (saved); the velocity particle's line direction while it is made
    pub velocity: Ge3DPoint,
    /// `mAngle` (exhaust trail; saved) and the queue's copy
    pub angle: f32,
    pub draw_angle: f32,
    /// `mClockwise` (exhaust trail; saved)
    pub flag: bool,
    /// `mLastPart`: the fire's last burning part (saved)
    pub last_part: i32,
    /// the winter breath's `mDuration` (ms), `mSpeed`, `mSlowDownTime` and `mFade` (saved); the projectile's speed
    pub duration: u32,
    pub speed: f32,
    pub slow_down: f32,
    pub fade: f32,
}

/// A point the host may not have (`has` false: no complex unit).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfx2DPoint {
    pub has: bool,
    pub point: Ge3DPoint,
}

/// The record's sprite state.
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn sp<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxEffectSprite {
    // SAFETY: the function's contract
    unsafe { &mut (*r).sprite }
}

/// `SetPosition(p)`: the record's position, then the host's `MoveToPoint`.
///
/// # Safety
/// As `sp`.
unsafe fn set_position<H: Effect2DHost>(r: *mut EeGfxEffectRecord, p: Ge3DPoint, h: &mut H) {
    // SAFETY: the function's contract
    unsafe { (*r).position = p };
    h.move_to_point();
}

/// A transformation's translation (`GE3DPoint(const GETransformation&)`).
fn translation(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] }
}

/// `SetPosition` on a transformation (the camera's copy).
fn at(mut t: GeTransformation, p: &Ge3DPoint) -> GeTransformation {
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
    t
}

/// The alpha reference of the graphic effect's object (0 without one).
fn alpha_ref(g: &EeGfxGraphicFacts) -> f32 {
    if g.has_object {
        g.alpha_ref
    } else {
        0.0
    }
}

/// One step of an animation over `[start, last]`: the new frame and time, or `None` past the last frame.
fn advance(frame: u32, time: f32, frame_time: u32) -> (u32, f32, bool) {
    if time >= frame_time as f32 {
        let n = um::lrintf(time / frame_time as f32) as u32;
        (n.wrapping_add(frame), time - n.wrapping_mul(frame_time) as f32, true)
    } else {
        (frame, time, false)
    }
}

/// The sprites' virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn sprite<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    let kind = unsafe { (*r).kind };
    match kind {
        EE_GFX_EFFECT_KIND_IMPACT | EE_GFX_EFFECT_KIND_IMPACT_BLOOD | EE_GFX_EFFECT_KIND_MUZZLE_FLASH => unsafe {
            impact(op, r, g, h, x)
        },
        EE_GFX_EFFECT_KIND_LIGHTNING => unsafe { lightning(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_PLASMA_PULSE => unsafe { plasma(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_PULSE_WAVE => unsafe { pulse_wave(op, r, g, h, x) },
        crate::gfx::effect_state::EE_GFX_EFFECT_KIND_WATER_RIPPLE => unsafe { ripple(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_FIRE_TRAIL => unsafe { fire_trail(op, r, g, h, x) },
        _ => 0,
    }
}

/// `Load` (`load`) or `Save` of a sprite after `EEEffect2D`'s (whose result each ignores); `major`/`minor` the file's
/// version.
pub fn sprite_file<H: Effect2DHost, F: EffectFile>(
    load: bool,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    let kind = r.kind;
    let s = &mut r.sprite;
    match kind {
        // 0xe1497 / 0xe1524 (the muzzle flash's forward to them): the start, time, frame, life and, from 0.245, the
        // tag point (else 0)
        EE_GFX_EFFECT_KIND_IMPACT | EE_GFX_EFFECT_KIND_IMPACT_BLOOD | EE_GFX_EFFECT_KIND_MUZZLE_FLASH => {
            if load {
                h.object_media();
                let mut res = load_point(f, &mut s.start);
                if res != OK {
                    return res;
                }
                res = f.read_f32(&mut s.time);
                if res != OK {
                    return res;
                }
                res = f.read_u32(&mut s.frame);
                if res != OK {
                    return res;
                }
                res = f.read_f32(&mut s.life);
                if res == OK && (major > 0 || minor >= 0xf5) {
                    res = f.read_u32(&mut s.tag_index);
                } else {
                    s.tag_index = 0;
                }
                res
            } else {
                let mut res = save_point(f, &s.start);
                if res != OK {
                    return res;
                }
                res = f.write_f32(s.time);
                if res != OK {
                    return res;
                }
                res = f.write_u32(s.frame);
                if res != OK {
                    return res;
                }
                res = f.write_f32(s.life);
                if res == OK {
                    res = f.write_u32(s.tag_index);
                }
                res
            }
        }
        // 0xe2c0f (the media's error stops it) / 0xe2c5b; 0xe5b04 / 0xe5b45: the time and frame
        EE_GFX_EFFECT_KIND_LIGHTNING | EE_GFX_EFFECT_KIND_PLASMA_PULSE => {
            if load {
                let media = h.object_media();
                if media != OK && kind == EE_GFX_EFFECT_KIND_LIGHTNING {
                    return media;
                }
                let res = f.read_f32(&mut s.time);
                if res == OK {
                    f.read_u32(&mut s.frame)
                } else {
                    res
                }
            } else {
                let res = f.write_f32(s.time);
                if res == OK {
                    f.write_u32(s.frame)
                } else {
                    res
                }
            }
        }
        // 0xe5fa0 / 0xe6004: the age, radius, hit points and shrinking flag
        EE_GFX_EFFECT_KIND_PULSE_WAVE => {
            if load {
                h.object_media();
                let mut res = f.read_f32(&mut s.age);
                if res != OK {
                    return res;
                }
                res = f.read_f32(&mut s.radius);
                if res != OK {
                    return res;
                }
                res = f.read_i32(&mut s.hit_points);
                if res == OK {
                    res = f.read_bool(&mut s.shrinking);
                }
                res
            } else {
                let mut res = f.write_f32(s.age);
                if res != OK {
                    return res;
                }
                res = f.write_f32(s.radius);
                if res != OK {
                    return res;
                }
                res = f.write_i32(s.hit_points);
                if res == OK {
                    res = f.write_bool(s.shrinking);
                }
                res
            }
        }
        // 0xe803e / 0xe8089: the aged record's transformation and age
        crate::gfx::effect_state::EE_GFX_EFFECT_KIND_WATER_RIPPLE => {
            if load {
                h.object_media();
                load_aged(r, f)
            } else {
                save_aged(r, f)
            }
        }
        // 0xe0ec9 / 0xe0f0f: the age and rotation
        EE_GFX_EFFECT_KIND_FIRE_TRAIL => {
            if load {
                h.object_media();
                let res = f.read_f32(&mut s.age);
                if res == OK {
                    f.read_f32(&mut s.rotation)
                } else {
                    res
                }
            } else {
                let res = f.write_f32(s.age);
                if res == OK {
                    f.write_f32(s.rotation)
                } else {
                    res
                }
            }
        }
        _ => 0,
    }
}

/// The impact's tag point `tag_index` on the unit's current frame (`UpdatePosition`, 0xe15e3: nothing without a
/// unit or complex unit).
///
/// # Safety
/// As `sp`.
unsafe fn impact_follow<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    if !g.has_unit {
        return;
    }
    // SAFETY: the function's contract
    let index = unsafe { sp(r).tag_index };
    let p = h.tag_point(6, index);
    if p.has {
        unsafe {
            sp(r).start = p.point;
            set_position(r, p.point, h);
        }
    }
}

/// The impact, blood impact and muzzle flash.
///
/// # Safety
/// As `sprite`.
unsafe fn impact<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    let kind = unsafe { (*r).kind };
    match op {
        // 0xe103d on a unit, 0xe10af at a point (the C++ put the point in `start`): life 0
        EeGfxEffect3DOp::Create | EeGfxEffect3DOp::CreateAt => {
            unsafe { sp(r).life = 0.0 };
            0
        }
        // 0xe458e: the muzzle flash at the weapon tag point; 0
        EeGfxEffect3DOp::SetUp if kind == EE_GFX_EFFECT_KIND_MUZZLE_FLASH => {
            let start = h.weapon_point(3);
            unsafe { sp(r).start = start };
            h.object_media();
            let first = h.frames(0).start;
            unsafe {
                let s = sp(r);
                s.time = 0.0;
                s.draw_frame = 0;
                s.frame = first;
                set_position(r, start, h);
                (*r).bounding_radius = g.size;
            }
            0
        }
        // 0xe123e: the media at its first frame; a unit's at a tag point of kind 6 picked by the game time
        // (`PickTagPoint`, 0xe158e), else at the start; the radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            let first = h.frames(0).start;
            unsafe {
                let s = sp(r);
                s.time = 0.0;
                s.draw_frame = 0;
                s.frame = first;
            }
            if g.has_unit {
                let count = h.tag_count(6);
                if count > 0 {
                    unsafe { sp(r).tag_index = g.game_time_bits % count as u32 };
                    unsafe { impact_follow(r, g, h) };
                }
            } else {
                let start = unsafe { sp(r).start };
                unsafe { set_position(r, start, h) };
            }
            unsafe { (*r).bounding_radius = g.size };
            res
        }
        // 0xe11ca: the frame and start for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_frame = s.frame;
                    s.draw_position = s.start;
                }
                h.queue(4);
            }
            res
        }
        EeGfxEffect3DOp::DrawPass6 if kind == EE_GFX_EFFECT_KIND_IMPACT_BLOOD => unsafe { blood_draw(r, g, h, x) },
        EeGfxEffect3DOp::DrawPass6 => unsafe { impact_draw(r, g, h, x) },
        EeGfxEffect3DOp::Step => unsafe { impact_step(r, g, h) },
        _ => 0,
    }
}

/// 0xe1290: the additive fog-lit sprite (spun at random under int param 1).
///
/// # Safety
/// As `sprite`.
unsafe fn impact_draw<H: Effect2DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let camera = x.camera();
    let model = h.model();
    x.set_alpha_ref(alpha_ref(g));
    let fog = x.get_fog();
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_ONE, BLEND_ONE);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::White);
    let texture = h.object_texture(0);
    // SAFETY: the function's contract
    let (frame, p) = unsafe { (sp(r).draw_frame, sp(r).draw_position) };
    x.texture_frame(texture, frame);
    x.model_texture(model, texture);
    let value = x.fog_factor(&p, fog);
    x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
    let t = at(camera, &p);
    if g.int_param1 != 0 {
        let mut spin = GeTransformation::default();
        spin.set_orientation(RotationOrder::Ypr, 0.0, 0.0, random_float(0.0, 3.141_592_7));
        let mut spin = at(spin, &translation(&t));
        spin.m[3] = [0.0, 0.0, 0.0, 1.0];
        x.model_transform(model, &spin);
    } else {
        x.model_transform(model, &t);
    }
    x.model_scale(model, g.size);
    x.model_prepare(model);
    let res = x.model_draw(model, 1.0);
    x.set_fog(fog);
    res
}

/// 0xe18c9: the blood impact's red alpha-blended sprite at its start, only with blood on; the material back to white.
///
/// # Safety
/// As `sprite`.
unsafe fn blood_draw<H: Effect2DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    if !h.draw_blood() {
        return 0;
    }
    let camera = x.camera();
    let model = h.model();
    x.set_alpha_ref(alpha_ref(g));
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::Unit);
    let texture = h.object_texture(0);
    // SAFETY: the function's contract
    let (frame, start) = unsafe { (sp(r).draw_frame, sp(r).start) };
    x.texture_frame(texture, frame);
    x.model_texture(model, texture);
    x.model_transform(model, &at(camera, &start));
    x.model_scale(model, g.size);
    x.model_prepare(model);
    x.model_color(model, EeGfxColor { red: 0.8, green: 0.0, blue: 0.0 });
    let res = x.model_draw(model, 1.0);
    x.model_color(model, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    res
}

/// 0xe166b: the animation and its sound; 7 after the last frame unless looping (a loop of a timed impact follows its
/// tag point again), or when a timed impact's unit dies or its life ends.
///
/// # Safety
/// As `sprite`.
unsafe fn impact_step<H: Effect2DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) -> i32 {
    // SAFETY (every access below): the function's contract
    let location = unsafe { (*r).position };
    let (old, time) = unsafe { (sp(r).frame, sp(r).time) };
    let frames = h.frames(0);
    let start = old == frames.start && time == 0.0;
    let time = g.tick_time + time;
    let mut res = 0;
    if g.flag2 {
        if g.has_unit && g.unit_hit_points <= 0 {
            res = 7;
        } else {
            let s = unsafe { sp(r) };
            s.life += g.tick_seconds * 1000.0 / g.duration as f32;
            if s.life > 1.0 {
                res = 7;
            }
        }
    }
    let (mut frame, mut time) = (old, time);
    if res == 0 {
        let (f, t, moved) = advance(old, time, frames.time);
        if moved {
            (frame, time) = (f, t);
            if frame > frames.last {
                if !g.flag1 {
                    res = 7;
                } else {
                    frame = frames.start;
                    if g.flag2 {
                        unsafe { impact_follow(r, g, h) };
                    }
                }
            }
        }
    }
    if res == 0 {
        unsafe {
            let s = sp(r);
            s.frame = frame;
            s.time = time;
        }
        h.animation_sound(location, frame, start || frame != old, false);
    }
    res
}

/// The lightning.
///
/// # Safety
/// As `sprite`.
unsafe fn lightning<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe294b: at the point (the C++ put it in `start`)
        EeGfxEffect3DOp::CreateAt => {
            let p = unsafe { sp(r).start };
            unsafe { set_position(r, p, h) };
            0
        }
        // 0xe29a1: the media at its first frame, half its size above the unit's model point (or its own point)
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            let first = h.frames(0).start;
            unsafe {
                let s = sp(r);
                s.frame = first;
                s.time = 0.0;
                s.draw_frame = 0;
            }
            let mut p = if g.has_unit && h.has_complex() {
                let mut t = h.unit_location();
                let offset = h.default_point();
                let scale = h.stats_scale();
                for row in t.m.iter_mut().take(3) {
                    for v in row.iter_mut().take(3) {
                        *v *= scale;
                    }
                }
                t.apply_point(offset)
            } else {
                unsafe { (*r).position }
            };
            p.z += g.size * 0.5;
            unsafe {
                set_position(r, p, h);
                (*r).bounding_radius = g.size;
            }
            res
        }
        // 0xe2a0b: the frame for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe { sp(r).draw_frame = sp(r).frame };
                h.queue(4);
            }
            res
        }
        // 0xe2ad7: the additive fog-lit sprite on frames 0 and 2 only (a flicker)
        EeGfxEffect3DOp::DrawPass6 => {
            let camera = x.camera();
            let model = h.model();
            let mut res = 0;
            x.set_alpha_ref(alpha_ref(g));
            let fog = x.get_fog();
            let frame = unsafe { sp(r).draw_frame };
            if frame == 0 || frame == 2 {
                x.set_zbuffer(ZBUFFER_TEST_ONLY);
                x.set_blending(BLEND_ONE, BLEND_ONE);
                x.set_shading_source(0);
                x.lighting(EeGfxLighting::White);
                let texture = h.object_texture(0);
                x.model_texture(model, texture);
                let p = unsafe { (*r).position };
                let value = x.fog_factor(&p, fog);
                x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
                x.model_transform(model, &at(camera, &p));
                x.model_scale(model, g.size);
                x.model_prepare(model);
                res = x.model_draw(model, 1.0);
            }
            x.set_fog(fog);
            res
        }
        // 0xe2c95: the animation and its sound; 7 after the last frame
        EeGfxEffect3DOp::Step => {
            let location = unsafe { (*r).position };
            let (old, time) = unsafe { (sp(r).frame, sp(r).time) };
            let frames = h.frames(0);
            let start = old == frames.start && time == 0.0;
            let (frame, time, moved) = advance(old, g.tick_time + time, frames.time);
            if moved && frame > frames.last {
                return 7;
            }
            unsafe {
                let s = sp(r);
                s.time = time;
                s.frame = frame;
            }
            h.animation_sound(location, frame, start || frame != old, false);
            0
        }
        _ => 0,
    }
}

/// The plasma pulse.
///
/// # Safety
/// As `sprite`.
unsafe fn plasma<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe5806: the media at its first frame, at the unit
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            let first = h.frames(0).start;
            unsafe {
                let s = sp(r);
                s.time = 0.0;
                s.draw_frame = 0;
                s.frame = first;
            }
            let p = translation(&h.unit_location());
            unsafe {
                set_position(r, p, h);
                (*r).bounding_radius = g.size;
            }
            res
        }
        // 0xe5804 (the fire trail's too): the frame and position for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_frame = s.frame;
                    s.draw_position = (*r).position;
                }
                h.queue(4);
            }
            res
        }
        // 0xe5880: the additive fog-lit sprite at the drawn position
        EeGfxEffect3DOp::DrawPass6 => {
            let camera = x.camera();
            let model = h.model();
            x.set_alpha_ref(alpha_ref(g));
            let fog = x.get_fog();
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            let texture = h.object_texture(0);
            let (frame, p) = unsafe { (sp(r).draw_frame, sp(r).draw_position) };
            x.texture_frame(texture, frame);
            x.model_texture(model, texture);
            let value = x.fog_factor(&p, fog);
            x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
            x.model_transform(model, &at(camera, &p));
            x.model_scale(model, g.size);
            x.model_prepare(model);
            let res = x.model_draw(model, 1.0);
            x.set_fog(fog);
            res
        }
        // 0xe5b6b: follows the unit and loops the animation (frame modulo the count) with its sound; 7 once the unit
        // has no hit points (exactly 0)
        EeGfxEffect3DOp::Step => {
            let (old, time) = unsafe { (sp(r).frame, sp(r).time) };
            let frames = h.frames(0);
            let start = old == frames.start && time == 0.0;
            if g.unit_hit_points == 0 {
                return 7;
            }
            let location = translation(&h.unit_location());
            unsafe { set_position(r, location, h) };
            let time = g.tick_time + time;
            let (frame, time) = if time >= frames.time as f32 {
                let n = um::lrintf(time / frames.time as f32) as u32;
                (n.wrapping_add(old) % frames.count, time - n.wrapping_mul(frames.time) as f32)
            } else {
                (old, time)
            };
            unsafe {
                let s = sp(r);
                s.time = time;
                s.frame = frame;
            }
            h.animation_sound(location, frame, start || frame != old, false);
            0
        }
        _ => 0,
    }
}

/// The pulse wave.
///
/// # Safety
/// As `sprite`.
unsafe fn pulse_wave<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe5ca6: age 0, radius 0, growing
        EeGfxEffect3DOp::Create => {
            unsafe {
                let s = sp(r);
                s.age = 0.0;
                s.draw_age = 0.0;
                s.shrinking = false;
                s.radius = 0.0;
            }
            0
        }
        // 0xe5da0: the radius, the media, the full size (parameter 3)
        EeGfxEffect3DOp::SetUp => {
            unsafe { (*r).bounding_radius = g.size };
            let res = h.object_media();
            unsafe { sp(r).radius = g.param3 };
            res
        }
        // 0xe5d39: the age and radius for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_age = s.age;
                    s.draw_radius = s.radius;
                }
                h.queue(4);
            }
            res
        }
        // 0xe5dc6: the additive camera-facing sprite sized by the radius, 0.2 above the position, its light fading over
        // the last tenth of its life
        EeGfxEffect3DOp::DrawPass6 => {
            let model = h.model();
            x.set_alpha_ref(alpha_ref(g));
            let fog = x.get_fog();
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            let texture = h.object_texture(0);
            x.model_texture(model, texture);
            let (age, radius, p) = unsafe { (sp(r).draw_age, sp(r).draw_radius, (*r).position) };
            let scale = (g.size - g.start_size) * radius + g.start_size;
            let mut t = GeTransformation::default();
            t.set_orientation(RotationOrder::Ypr, x.camera_yaw(), 0.0, 0.0);
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z + 0.2;
            t.m[3] = [0.0, 0.0, 0.0, 1.0];
            x.model_transform(model, &t);
            x.model_scale(model, scale);
            let value = x.fog_factor(&p, fog);
            let light = if age < 0.9 { value } else { (1.0 - age) * value * 10.0 };
            x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
            x.model_prepare(model);
            let res = x.model_draw(model, 1.0);
            x.set_fog(fog);
            res
        }
        // 0xe605a: follows the unit; a hit jumps to the full radius and shrinks to parameter 2, else it grows back by
        // parameter 1; 7 when the unit dies or the life ends
        EeGfxEffect3DOp::Step => {
            if g.unit_hit_points <= 0 {
                return 7;
            }
            let age = unsafe {
                sp(r).age += g.tick_time / g.duration as f32;
                sp(r).age
            };
            if !(age <= 1.0) {
                return 7;
            }
            let p = translation(&h.unit_location());
            unsafe { set_position(r, p, h) };
            let s = unsafe { sp(r) };
            if s.shrinking {
                let shrunk = s.radius - g.param1;
                s.radius = if g.param2 > shrunk { g.param2 } else { shrunk };
                s.shrinking = s.radius != g.param2;
            } else if s.hit_points > g.unit_hit_points {
                s.radius = g.param3;
                s.shrinking = true;
            } else {
                let grown = g.param1 + s.radius;
                s.radius = if g.param3 < grown { g.param3 } else { grown };
            }
            s.hit_points = g.unit_hit_points;
            0
        }
        _ => 0,
    }
}

/// The water ripple (its state the aged record's).
///
/// # Safety
/// As `sprite`.
unsafe fn ripple<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe7daa at a point (in `start`) / 0xe7e50 at a transformation (the C++ put it in the aged record): flattened
        // to height 0, age 0
        EeGfxEffect3DOp::CreateAt | EeGfxEffect3DOp::CreateTransformation => {
            let a = unsafe { &mut (*r).aged };
            if op == EeGfxEffect3DOp::CreateAt {
                let p = unsafe { (*r).sprite.start };
                a.transformation.m[0][3] = p.x;
                a.transformation.m[1][3] = p.y;
                a.transformation.m[2][3] = p.z;
            }
            a.transformation.m[2][3] = 0.0;
            a.age = 0.0;
            a.draw_age = 0.0;
            0
        }
        // 0xe7f34: the media; at the transformation's position; the radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            let p = translation(unsafe { &(*r).aged.transformation });
            unsafe {
                set_position(r, p, h);
                (*r).bounding_radius = g.size;
            }
            res
        }
        // 0xe7ecd: the age for the draw, render queue 3
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe { (*r).aged.draw_age = (*r).aged.age };
                h.queue(3);
            }
            res
        }
        // 0xe7f90: a flat sprite from the start size to the size, fading over its second half
        EeGfxEffect3DOp::DrawPass5 => {
            let (age, p) = unsafe { ((*r).aged.draw_age, translation(&(*r).aged.transformation)) };
            let alpha = if age < 0.5 { 1.0 } else { (1.0 - age) * 2.0 };
            let size = (g.size - g.start_size) * age + g.start_size;
            let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
            x.sprite(EE_GFX_SPRITE_GROUND, &p, white, 0.0, size, alpha, 1.0, alpha_ref(g));
            0
        }
        // 0xe80c0: the age; 7 once over or when its tile is no longer blocked
        EeGfxEffect3DOp::Step => {
            let mut tile = [0; 2];
            let mut local = unsafe { r.read() };
            let res = aged_step(&mut local, g.tick_seconds, g.duration, &mut tile);
            unsafe { r.write(local) };
            if res == EE_GFX_EFFECT_ASK_BLOCKED {
                if h.tile_blocked(tile[0], tile[1]) {
                    0
                } else {
                    7
                }
            } else {
                res
            }
        }
        _ => 0,
    }
}

/// The fire trail.
///
/// # Safety
/// As `sprite`.
unsafe fn fire_trail<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    match op {
        // 0xe0d38: age 0, a random rotation
        EeGfxEffect3DOp::Create => {
            let rotation = random() * 6.283_185_5;
            let s = unsafe { sp(r) };
            s.age = 0.0;
            s.draw_age = 0.0;
            s.rotation = rotation;
            0
        }
        // 0xe0dee: at the unit (with a complex unit) sized by the start size; the media
        EeGfxEffect3DOp::SetUp => {
            if h.has_complex() {
                let p = translation(&h.unit_location());
                unsafe {
                    set_position(r, p, h);
                    (*r).bounding_radius = g.start_size;
                }
            }
            h.object_media()
        }
        // 0xe5804: the age and position for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_age = s.age;
                    s.draw_position = (*r).position;
                }
                h.queue(4);
            }
            res
        }
        // 0xe0e50: a glow sprite from the start size to the size, fading with age
        EeGfxEffect3DOp::DrawPass6 => unsafe { fire_trail_draw(r, g, x) },
        // 0xe0f44: drifts with the wind, rising and growing; 7 once over
        EeGfxEffect3DOp::Step => {
            let mut p = unsafe { (*r).position };
            let wind = h.wind();
            let elapsed = g.tick_time;
            let age = unsafe {
                sp(r).age += elapsed / g.duration as f32;
                sp(r).age
            };
            if !(age <= 1.0) {
                return 7;
            }
            p.x += wind.x;
            p.y += wind.y;
            p.z += wind.z + elapsed * FIRE_TRAIL_RISE;
            unsafe {
                set_position(r, p, h);
                (*r).bounding_radius = ((g.size - g.start_size) * age + g.start_size) * 0.5;
            }
            0
        }
        _ => 0,
    }
}

/// The fire trail's pass 6 (0xe0e50).
///
/// # Safety
/// As `sprite`.
unsafe fn fire_trail_draw<R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, x: &mut R) -> i32 {
    // SAFETY: the function's contract
    let s = unsafe { *sp(r) };
    let size = (g.size - g.start_size) * s.draw_age + g.start_size;
    let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
    x.sprite(EE_GFX_SPRITE_GLOW, &s.draw_position, white, s.rotation, size, 1.0 - s.draw_age, 1.0, alpha_ref(g));
    0
}

#[cfg(test)]
#[path = "effect_sprite_tests.rs"]
mod tests;
